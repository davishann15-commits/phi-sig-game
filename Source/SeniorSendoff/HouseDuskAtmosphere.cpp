// Blue-hour dusk, the outside-only film look, grill smoke and road decals. See HouseDuskAtmosphere.h.
#include "HouseDuskAtmosphere.h"
#include "HouseCalderLandscape.h"
#include "HouseCampusInteriors.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"
#include "HouseWoodsPass.h"
#include "HouseWoodsAtmosphere.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "TextureResource.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace HouseDuskImpl
{
const TCHAR* DecalParentPath = TEXT("/Game/SeniorSendOff/House/ExteriorStreet/M_StreetDecal.M_StreetDecal");
const TCHAR* SmokeParentPath = TEXT("/Game/SeniorSendOff/House/ExteriorStreet/M_StreetSmoke.M_StreetSmoke");
constexpr int32 PuffCount = 44;
}

// ------------------------------------------------------------------ engine-side helpers for HouseStreetScene
UTexture2D* HouseStreetScene::MakeTexture(const FHallImage& Img, bool bSRGB, bool bNormalMap, const TCHAR* Name, bool bWrap)
{
    if (Img.W <= 0 || Img.H <= 0) return nullptr;
    UTexture2D* T = UTexture2D::CreateTransient(Img.W, Img.H, PF_B8G8R8A8, MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), FName(Name)));
    if (!T) return nullptr;
    T->SRGB = bSRGB;
    T->CompressionSettings = bNormalMap ? TC_Normalmap : TC_Default;
    T->AddressX = T->AddressY = bWrap ? TA_Wrap : TA_Clamp;
    T->Filter = TF_Trilinear;
    T->LODGroup = TEXTUREGROUP_World;
    FTexturePlatformData* PD = T->GetPlatformData();
    if (!PD || PD->Mips.Num() == 0) return nullptr;
    TArray<FLinearColor> Level = Img.Px;
    int32 LW = Img.W, LH = Img.H;
    for (int32 Mip = 0; ; ++Mip)
    {
        TArray<FColor> Encoded; Encoded.SetNumUninitialized(LW * LH);
        for (int32 I = 0; I < LW * LH; ++I)
        {
            const FLinearColor C = Level[I];
            const FLinearColor Opaque(C.R, C.G, C.B, 1.f);
            FColor E = bSRGB ? Opaque.ToFColor(true) : Opaque.QuantizeRound();
            E.A = (uint8)FMath::Clamp(FMath::RoundToInt(C.A * 255.f), 0, 255);
            Encoded[I] = E;
        }
        FTexture2DMipMap* Target = nullptr;
        if (Mip == 0) Target = &PD->Mips[0];
        else { Target = new FTexture2DMipMap(LW, LH, 1); PD->Mips.Add(Target); }
        Target->SizeX = LW; Target->SizeY = LH;
        Target->BulkData.Lock(LOCK_READ_WRITE);
        void* Data = Target->BulkData.Realloc(int64(LW) * LH * 4);
        FMemory::Memcpy(Data, Encoded.GetData(), int64(LW) * LH * 4);
        Target->BulkData.Unlock();
        if ((LW == 1 && LH == 1) || Mip >= 11) break;
        // Coverage-weighted colour so painted edges do not pick up the black of empty texels further down the chain.
        const int32 NW = FMath::Max(1, LW / 2), NH = FMath::Max(1, LH / 2);
        TArray<FLinearColor> Next; Next.SetNumUninitialized(NW * NH);
        for (int32 Y = 0; Y < NH; ++Y)
            for (int32 X = 0; X < NW; ++X)
            {
                const FLinearColor* Q[4] = { &Level[FMath::Min(Y * 2, LH - 1) * LW + FMath::Min(X * 2, LW - 1)], &Level[FMath::Min(Y * 2, LH - 1) * LW + FMath::Min(X * 2 + 1, LW - 1)],
                    &Level[FMath::Min(Y * 2 + 1, LH - 1) * LW + FMath::Min(X * 2, LW - 1)], &Level[FMath::Min(Y * 2 + 1, LH - 1) * LW + FMath::Min(X * 2 + 1, LW - 1)] };
                float SumA = 0, WSum = 0; FLinearColor SumC(0, 0, 0, 0);
                for (const FLinearColor* P : Q) { const float Wt = FMath::Max(P->A, 1e-3f); SumA += P->A; WSum += Wt; SumC += FLinearColor(P->R, P->G, P->B, 0.f) * Wt; }
                FLinearColor C = SumC / WSum;
                C.A = SumA * .25f;
                Next[Y * NW + X] = C;
            }
        Level = MoveTemp(Next); LW = NW; LH = NH;
    }
    T->UpdateResource();
    return T;
}

UMaterialInterface* HouseStreetScene::MakeDecalMaterial(UHouseBlackHallSubsystem* Look, FName Key, UTexture2D* Atlas, float Roughness, float Opacity, const FLinearColor& Tint)
{
    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, HouseDuskImpl::DecalParentPath);
    if (!Look || !Parent || !Atlas) return nullptr;
    UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Parent, Look);
    M->SetTextureParameterValue(TEXT("Atlas"), Atlas);
    M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
    M->SetScalarParameterValue(TEXT("Opacity"), Opacity);
    M->SetVectorParameterValue(TEXT("Tint"), Tint);
    Look->Materials.Add(Key, M);
    return M;
}

UMaterialInterface* HouseStreetScene::MakeSmokeMaterial(UHouseBlackHallSubsystem* Look, FName Key, UTexture2D* Puff)
{
    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, HouseDuskImpl::SmokeParentPath);
    if (!Look || !Parent || !Puff) return nullptr;
    UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Parent, Look);
    M->SetTextureParameterValue(TEXT("Puff"), Puff);
    M->SetScalarParameterValue(TEXT("Opacity"), .95f);
    M->SetScalarParameterValue(TEXT("Glow"), 5.f);
    M->SetVectorParameterValue(TEXT("SmokeColor"), FLinearColor(.70f, .71f, .74f));
    Look->Materials.Add(Key, M);
    return M;
}

// ------------------------------------------------------------------ lighting
FString HouseDusk::ApplyLighting(UWorld* World)
{
    if (!World) return FString();
    // The existing directional light becomes the sun a few degrees under the horizon, set off to the west down the street
    // (light yaw 25 = sun toward -x), so the sky over the house is deep blue with the afterglow low down the street. Below the
    // horizon the atmosphere's transmittance toward it is zero, so it lights nothing directly (rooms keep their own lighting)
    // and only drives the sky. Tunable for look tests.
    // The sky light is kept low so the sodium pools carry the street.
    float SunLux = 14000.f, Below = 4.5f, Yaw = 25.f, SkyScale = 2.f, SkyMin = .6f, Glow = HouseCalderLandscape::Enabled() ? .8f : 1.3f;
    FParse::Value(FCommandLine::Get(), TEXT("DuskSun="), SunLux);
    FParse::Value(FCommandLine::Get(), TEXT("DuskBelow="), Below);
    FParse::Value(FCommandLine::Get(), TEXT("DuskYaw="), Yaw);
    FParse::Value(FCommandLine::Get(), TEXT("DuskSky="), SkyScale);
    FParse::Value(FCommandLine::Get(), TEXT("DuskSkyMin="), SkyMin);
    FParse::Value(FCommandLine::Get(), TEXT("DuskGlow="), Glow);
    // Blue-hour tint on the sky itself (and so on the sky light's realtime capture: cool fill in the shadows). The fog's
    // directional term puts the warm afterglow back along the horizon toward the sun. -DuskSkyTint=r,g,b
    FLinearColor Tint(.80f, .93f, 1.16f);
    FString TintArg;
    if (FParse::Value(FCommandLine::Get(), TEXT("DuskSkyTint="), TintArg, false))
    {
        TArray<FString> Parts;
        TintArg.ParseIntoArray(Parts, TEXT(","));
        if (Parts.Num() == 3) Tint = FLinearColor(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
    }
    FString Notes;
    // The map has a single directional light; take the first one found.
    TActorIterator<ADirectionalLight> First(World);
    if (ADirectionalLight* Light = First ? *First : nullptr)
    {
        if (UDirectionalLightComponent* D = Cast<UDirectionalLightComponent>(Light->GetLightComponent()))
        {
            Notes += FString::Printf(TEXT(" sun(%.2f->%.0f below=%.1f yaw=%.0f)"), D->Intensity, SunLux, Below, Yaw);
            D->SetIntensity(SunLux);
            D->SetLightColor(FLinearColor(1.f, .9f, .8f));
            D->SetAtmosphereSunLight(true);
            D->SetCastShadows(false);
            D->MarkRenderStateDirty();
            Light->SetActorRotation(FRotator(Below, Yaw, 0.f));
        }
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        USkyAtmosphereComponent* Atmosphere = It->FindComponentByClass<USkyAtmosphereComponent>();
        if (!Atmosphere) continue;
        // Through reflection, so an engine without the art-direction factor just keeps its sky.
        FStructProperty* Factor = FindFProperty<FStructProperty>(USkyAtmosphereComponent::StaticClass(), TEXT("SkyLuminanceFactor"));
        if (Factor && Factor->Struct == TBaseStructure<FLinearColor>::Get())
        {
            *Factor->ContainerPtrToValuePtr<FLinearColor>(Atmosphere) = Tint;
            Atmosphere->MarkRenderStateDirty();
            Notes += FString::Printf(TEXT(" skyTint(%.2f,%.2f,%.2f)"), Tint.R, Tint.G, Tint.B);
        }
        else Notes += TEXT(" skyTint(unavailable)");
        break;
    }
    for (TActorIterator<ASkyLight> It(World); It; ++It)
    {
        if (USkyLightComponent* Sky = It->GetLightComponent())
        {
            const float Old = Sky->Intensity;
            Sky->SetIntensity(FMath::Max(SkyMin, Old * SkyScale));
            Sky->RecaptureSky();
            Notes += FString::Printf(TEXT(" sky(%.2f->%.2f realtime=%d)"), Old, Sky->Intensity, Sky->bRealTimeCapture ? 1 : 0);
        }
    }
    for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
    {
        if (UExponentialHeightFogComponent* F = It->GetComponent())
        {
            // Cool blue haze; warm only far off toward the afterglow, so the street itself is not veiled in orange.
            F->FogInscatteringLuminance = FLinearColor(.020f, .033f, .072f);
            F->DirectionalInscatteringLuminance = FLinearColor(.13f, .06f, .02f) * Glow;
            F->DirectionalInscatteringExponent = 6.f;
            F->DirectionalInscatteringStartDistance = 5000.f;
            F->MarkRenderStateDirty();
            Notes += FString::Printf(TEXT(" duskFog(glow=%.2f)"), Glow);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("HOUSE_DUSK lighting%s extraLights=0"), *Notes);
    return Notes;
}

int32 HouseDusk::SpawnDecals(AActor* Owner, UHouseBlackHallSubsystem* Look, const TArray<HouseStreetScene::FDecal>& Decals, TArray<TObjectPtr<UDecalComponent>>& Out)
{
    if (!Owner || !Look) return 0;
    int32 N = 0;
    for (const HouseStreetScene::FDecal& D : Decals)
    {
        UMaterialInterface* M = Look->Materials.FindRef(D.Material);
        if (!M) continue;
        UDecalComponent* C = NewObject<UDecalComponent>(Owner);
        C->SetupAttachment(Owner->GetRootComponent());
        C->SetDecalMaterial(M);
        C->DecalSize = D.HalfSize;
        C->SortOrder = D.Sort;
        C->SetFadeScreenSize(0.f);
        C->RegisterComponent();
        C->SetWorldLocationAndRotation(D.Center, FRotator(-90.f, D.Yaw, 0.f));
        Owner->AddInstanceComponent(C);
        Out.Add(C);
        ++N;
    }
    return N;
}

// ------------------------------------------------------------------ component
UHouseDuskAtmosphereComponent::UHouseDuskAtmosphereComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.f;
    SetIsReplicatedByDefault(false);
}

float UHouseDuskAtmosphereComponent::DistanceOutsideHouse(const FVector& P)
{
    // The three blocks of the house (wings and centre), basement to roof.
    static const FBox Blocks[] = {
        FBox(FVector(-3400, -2200, -420), FVector(-1420, 1540, 1150)),
        FBox(FVector(-1420, -1010, -420), FVector(1360, 1640, 1150)),
        FBox(FVector(1360, -2130, -420), FVector(3340, 1540, 1150)) };
    float Best = FLT_MAX;
    for (const FBox& B : Blocks) Best = FMath::Min(Best, FMath::Sqrt(B.ComputeSquaredDistanceToPoint(P)));
    // Calder State's walk-in buildings are indoors too (indoor exposure, no film grade, the clock tower's bells muffled).
    return FMath::Min(Best, HouseCampusInteriors::DistanceOutside(P));
}

float UHouseDuskAtmosphereComponent::Rand()
{
    Seed ^= Seed << 13; Seed ^= Seed >> 17; Seed ^= Seed << 5;
    return (Seed & 0xffffff) / 16777216.f;
}

void UHouseDuskAtmosphereComponent::Build(UHouseBlackHallSubsystem* Look, const FVector& SmokeOrigin)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Look) return;
    Origin = SmokeOrigin;
    Seed = 0x9e3779b9u;
    if (HouseWoodsPass::Stage() >= 2 && !bWoodlandFogLease && !FParse::Param(FCommandLine::Get(), TEXT("NoWoodsFog")))
    {
        HouseWoodsAtmosphere::AcquireNearFog();
        bWoodlandFogLease = true;
    }
    // Film look, outside only: the weight follows the camera out of the house (TickComponent).
    FilmLook = NewObject<UPostProcessComponent>(Owner, TEXT("ExteriorDuskFilmLook"));
    FilmLook->SetupAttachment(Owner->GetRootComponent());
    FilmLook->bUnbound = true;
    FilmLook->Priority = 3.f;   // above the map's two unbound volumes (priorities 0 and 2)
    FilmLook->BlendWeight = 1.f;
    FPostProcessSettings& S = FilmLook->Settings;
    S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = .3f;
    S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(.9, .92, .95, 1);
    S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(.94, .94, .94, 1);
    // Teal, slightly lifted shadows; neutral midtones; warm highlights (the sodium pools and the afterglow).
    S.bOverride_ColorGainShadows = true; S.ColorGainShadows = FVector4(.84, 1.0, 1.08, 1);
    S.bOverride_ColorOffsetShadows = true; S.ColorOffsetShadows = FVector4(0, .007, .010, 0);
    S.bOverride_ColorGainMidtones = true; S.ColorGainMidtones = FVector4(1.0, 1.0, .98, 1);
    S.bOverride_ColorGainHighlights = true; S.ColorGainHighlights = FVector4(1.08, 1.0, .88, 1);
    S.bOverride_ColorSaturationHighlights = true; S.ColorSaturationHighlights = FVector4(1.06, 1.06, 1.06, 1);
    S.bOverride_VignetteIntensity = true; S.VignetteIntensity = .55f;
    S.bOverride_SceneFringeIntensity = true; S.SceneFringeIntensity = .6f;
    // The map's manual exposure (unbound volume, bias -4.5) is made a third of a stop brighter outside only, so the house front
    // reads in the blue hour; indoors the weight is 0 and the rooms keep exactly -4.5. -DuskEV=<stops> changes the offset.
    float ExposureOffset = .35f;
    FParse::Value(FCommandLine::Get(), TEXT("DuskEV="), ExposureOffset);
    BaseExposure = -4.5f + ExposureOffset;
    if (ExposureOffset != 0.f || HouseWoodsPass::Stage() >= 2) { S.bOverride_AutoExposureBias = true; S.AutoExposureBias = BaseExposure; }
    FilmLook->RegisterComponent();
    Owner->AddInstanceComponent(FilmLook);

    // Grill smoke: camera-facing puffs in one translucent mesh, sorted back to front every frame.
    UMaterialInterface* SmokeMat = Look->Materials.FindRef(TEXT("ST_Smoke"));
    if (SmokeMat && Owner->GetNetMode() != NM_DedicatedServer)
    {
        Smoke = NewObject<UProceduralMeshComponent>(Owner, TEXT("ExteriorGrillSmoke"));
        Smoke->SetupAttachment(Owner->GetRootComponent());
        Smoke->SetMobility(EComponentMobility::Movable);
        Smoke->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Smoke->SetCastShadow(false);
        Smoke->SetReceivesDecals(false);
        Smoke->bAffectDynamicIndirectLighting = false;
        Smoke->bAffectDistanceFieldLighting = false;
        Smoke->RegisterComponent();
        Smoke->SetWorldLocation(FVector::ZeroVector);
        Owner->AddInstanceComponent(Smoke);
        Puffs.SetNum(HouseDuskImpl::PuffCount);
        const int32 V = HouseDuskImpl::PuffCount * 4;
        Pos.Init(Origin, V); Nrm.Init(FVector::UpVector, V); UV.SetNum(V); Col.Init(FLinearColor(0, 0, 0, 0), V); Tan.Init(FProcMeshTangent(1, 0, 0), V);
        TArray<int32> Tri;
        for (int32 Q = 0; Q < HouseDuskImpl::PuffCount; ++Q)
        {
            UV[Q * 4] = FVector2D(0, 1); UV[Q * 4 + 1] = FVector2D(1, 1); UV[Q * 4 + 2] = FVector2D(1, 0); UV[Q * 4 + 3] = FVector2D(0, 0);
            Tri.Append({ Q * 4, Q * 4 + 1, Q * 4 + 2, Q * 4, Q * 4 + 2, Q * 4 + 3 });
        }
        Smoke->CreateMeshSection_LinearColor(0, Pos, Tri, Nrm, UV, Col, Tan, false);
        Smoke->SetMaterial(0, SmokeMat);
        // Warm the plume up so it is already rising on the first frame.
        for (int32 K = 0; K < 240; ++K) SimSmoke(1.f / 30.f);
    }
    UE_LOG(LogTemp, Display, TEXT("HOUSE_DUSK film look priority=3 outsideOnly=1 smoke=%d puffs=%d"), Smoke ? 1 : 0, Puffs.Num());
}

void UHouseDuskAtmosphereComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bWoodlandFogLease) { HouseWoodsAtmosphere::ReleaseNearFog(); bWoodlandFogLease = false; }
    Super::EndPlay(Reason);
}

void UHouseDuskAtmosphereComponent::TickComponent(float Dt, ELevelTick Type, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Dt, Type, Function);
    UWorld* World = GetWorld();
    if (!World) return;
    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC || !PC->PlayerCameraManager) return;
    const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
    const FRotator CamRot = PC->PlayerCameraManager->GetCameraRotation();
    if (FilmLook)
    {
        // Full strength 40 cm outside the walls, none indoors.
        const float W = FMath::Clamp(DistanceOutsideHouse(Cam) / 40.f, 0.f, 1.f);
        if (W != Weight) { FilmLook->BlendWeight = W; Weight = W; }
        if (HouseWoodsPass::Stage() >= 2)
        {
            // Only the deep rear woodland loses a quarter stop. Keep all dusk
            // color grading, sky/sun and the house/party exposure unchanged.
            WoodlandDepth = FMath::FInterpTo(WoodlandDepth, HouseWoodsAtmosphere::DepthAt(Cam), Dt, 2.f);
            FilmLook->Settings.AutoExposureBias = BaseExposure - .24f * WoodlandDepth;
            FilmLook->Settings.VignetteIntensity = .55f + .045f * WoodlandDepth;
        }
    }
    if (Smoke)
    {
        SimSmoke(FMath::Min(Dt, .1f));
        WriteSmoke(Cam, CamRot);
    }
}

void UHouseDuskAtmosphereComponent::SimSmoke(float Dt)
{
    Clock += Dt;
    Spawn += Dt;
    constexpr float Every = .2f;
    while (Spawn >= Every)
    {
        Spawn -= Every;
        for (FPuff& P : Puffs)
        {
            if (P.bLive) continue;
            P.bLive = true; P.Age = 0; P.Life = 6.5f + 2.5f * Rand();
            P.P = Origin + FVector((Rand() - .5f) * 22.f, (Rand() - .5f) * 22.f, 0);
            P.V = FVector((Rand() - .5f) * 6.f, (Rand() - .5f) * 6.f, 36.f + 14.f * Rand());
            P.Size = 16.f + 8.f * Rand(); P.Spin = Rand() * 6.283f; P.SpinRate = (Rand() - .5f) * .5f;
            break;
        }
    }
    const FVector Wind(16.f, -9.f, 0.f);
    for (FPuff& P : Puffs)
    {
        if (!P.bLive) continue;
        P.Age += Dt;
        if (P.Age >= P.Life) { P.bLive = false; continue; }
        P.V.Z = FMath::Max(8.f, P.V.Z - 7.f * Dt);
        const FVector Curl(FMath::Sin(Clock * .7f + P.Spin * 3.f) * 7.f, FMath::Cos(Clock * .55f + P.Spin * 2.f) * 7.f, 0.f);
        P.P += (P.V + (Wind + Curl) * FMath::Min(1.f, P.Age * .5f)) * Dt;
        P.Spin += P.SpinRate * Dt;
    }
}

void UHouseDuskAtmosphereComponent::WriteSmoke(const FVector& Cam, const FRotator& CamRot)
{
    TArray<int32> Order;
    for (int32 I = 0; I < Puffs.Num(); ++I) if (Puffs[I].bLive) Order.Add(I);
    Order.Sort([&](int32 A, int32 B) { return FVector::DistSquared(Puffs[A].P, Cam) > FVector::DistSquared(Puffs[B].P, Cam); });
    const FRotationMatrix Frame(CamRot);
    const FVector Right = Frame.GetUnitAxis(EAxis::Y), Up = Frame.GetUnitAxis(EAxis::Z), Facing = -CamRot.Vector();
    for (int32 Slot = 0; Slot < Puffs.Num(); ++Slot)
    {
        const int32 B = Slot * 4;
        if (Slot >= Order.Num())
        {
            for (int32 K = 0; K < 4; ++K) { Pos[B + K] = Origin; Col[B + K] = FLinearColor(0, 0, 0, 0); }
            continue;
        }
        const FPuff& P = Puffs[Order[Slot]];
        const float T = P.Age / P.Life;
        const float Size = P.Size + 230.f * FMath::Pow(T, .7f);
        const float Fade = FMath::Clamp(P.Age / .6f, 0.f, 1.f) * FMath::Square(1.f - T);
        const float Glow = FMath::Square(FMath::Clamp(1.f - P.Age / 1.1f, 0.f, 1.f));
        const float C = FMath::Cos(P.Spin), S = FMath::Sin(P.Spin);
        const FVector A1 = (Right * C + Up * S) * (Size * .5f), A2 = (Up * C - Right * S) * (Size * .5f);
        Pos[B] = P.P - A1 - A2; Pos[B + 1] = P.P + A1 - A2; Pos[B + 2] = P.P + A1 + A2; Pos[B + 3] = P.P - A1 + A2;
        const FLinearColor Tint(Glow, .38f * Glow, .1f * Glow, .95f * Fade);
        for (int32 K = 0; K < 4; ++K) { Nrm[B + K] = Facing; Col[B + K] = Tint; Tan[B + K] = FProcMeshTangent(Right, false); }
    }
    if (Smoke) Smoke->UpdateMeshSection_LinearColor(0, Pos, Nrm, UV, Col, Tan);
}
