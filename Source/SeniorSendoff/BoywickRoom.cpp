#include "BoywickRoom.h"
#include "HouseWorldScale.h"
#include "ExistingHouseDoor.h"
#include "HouseInteractiveProp.h"
#include "BoywickCharacter.h"
#include "BoywickMetaHumanVisual.h"
#include "Camera/PlayerCameraManager.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "StoryCampaign.h"
#include "HouseRoamingPig.h"
#include "HouseBlackHallPaint.h"
#include "HouseBlackHallLook.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "StoryMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"

using namespace BoywickRoomSpec;

// ------------------------------------------------------------------------------------------------
// Painted dressing
// ------------------------------------------------------------------------------------------------
ABoywickRoomDecor::ABoywickRoomDecor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("BoywickDecorRoot")));
}

namespace
{
UMaterialInstanceDynamic* TexturedMaterial(UObject* Outer, UTexture2D* Base, float Rough, const FLinearColor& Emission = FLinearColor::Black)
{
    UMaterialInterface* Parent = HallHouseMat(TEXT("M_House_Textured"));
    if (!Parent) return nullptr;
    UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Parent, Outer);
    if (Base) M->SetTextureParameterValue(TEXT("BaseMap"), Base);
    M->SetScalarParameterValue(TEXT("UVScale"), 1.f);
    M->SetScalarParameterValue(TEXT("Roughness"), Rough);
    M->SetScalarParameterValue(TEXT("Metallic"), 0.f);
    M->SetScalarParameterValue(TEXT("NormalStrength"), 0.f);
    M->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);
    M->SetVectorParameterValue(TEXT("Emission"), Emission);
    return M;
}

UMaterialInstanceDynamic* PlainMat(UObject* Outer, const FLinearColor& Tint, float Rough, float Metal = 0.f, const FLinearColor& Emission = FLinearColor::Black)
{
    UMaterialInterface* Parent = HallHouseMat(TEXT("M_House_Plain"));
    if (!Parent) return nullptr;
    UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Parent, Outer);
    M->SetVectorParameterValue(TEXT("Tint"), Tint);
    M->SetScalarParameterValue(TEXT("Roughness"), Rough);
    M->SetScalarParameterValue(TEXT("Metallic"), Metal);
    M->SetVectorParameterValue(TEXT("Emission"), Emission);
    return M;
}

/** A double-sided quad whose texture reads upright for a viewer standing on the Front side. */
void AddQuad(UProceduralMeshComponent* Mesh, int32 Section, const FVector& Center, const FVector& Right, const FVector& Up, float Width, float Height, UMaterialInterface* Material)
{
    const FVector R = Right.GetSafeNormal() * Width * .5f, U = Up.GetSafeNormal() * Height * .5f;
    // Shading normal points at the viewer (their right x up points away from them in this left-handed frame).
    const FVector N = -FVector::CrossProduct(Right, Up).GetSafeNormal();
    TArray<FVector> V = { Center - R - U, Center + R - U, Center + R + U, Center - R + U };
    TArray<FVector2D> UV = { FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0) };
    TArray<int32> Tri = { 0, 2, 1, 0, 3, 2, 0, 1, 2, 0, 2, 3 };
    TArray<FVector> Normals = { N, N, N, N };
    TArray<FProcMeshTangent> Tangents; Tangents.Init(FProcMeshTangent(Right.GetSafeNormal(), false), 4);
    TArray<FLinearColor> Colors; Colors.Init(FLinearColor::White, 4);
    Mesh->CreateMeshSection_LinearColor(Section, V, Tri, Normals, UV, Colors, Tangents, false);
    Mesh->SetMaterial(Section, Material);
}

void PaintBanner(FHallImage& Img)
{
    const int32 W = 720, H = 440;
    Img.Init(W, H, FLinearColor(.40f, .26f, .10f));
    // Cloth: warm gold at the top fading to rust at the bottom, with soft fold shading.
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float T = float(Y) / H;
            FLinearColor C = FMath::Lerp(FLinearColor(.47f, .33f, .13f), FLinearColor(.36f, .12f, .05f), FMath::SmoothStep(.2f, 1.f, T));
            const float Fold = .06f * FMath::Sin(X * .021f + Y * .006f) + HallFbm(X * .008f, Y * .008f, 3, 7) * .08f;
            Img.At(X, Y) = C * (1.f + Fold);
        }
    // Flames licking up from the hem behind the word.
    for (int32 Pass = 0; Pass < 3; ++Pass)
    {
        const FLinearColor C = Pass == 0 ? FLinearColor(.55f, .10f, .03f) : Pass == 1 ? FLinearColor(.85f, .32f, .05f) : FLinearColor(.98f, .62f, .16f);
        const float Scale = Pass == 0 ? 1.f : Pass == 1 ? .7f : .42f;
        TArray<FVector2D> Poly; Poly.Add(FVector2D(W * .25f, H));
        for (int32 X = int32(W * .25f); X <= W; X += 6)
        {
            const float Tongue = FMath::Abs(FMath::Sin(X * .045f + Pass * 1.7f)) * 120.f + FMath::Sin(X * .11f + Pass) * 30.f + 50.f;
            Poly.Add(FVector2D(X, H - Tongue * Scale - 10.f));
        }
        Poly.Add(FVector2D(W, H));
        Img.FillPolygon(Poly, C, .85f);
    }
    // FLAVORTOWN in cream with a dark outline, lower right like the real flag.
    FHallTextStyle Style; Style.Height = 70.f; Style.Stroke = 12.f; Style.Spacing = .22f; Style.WidthScale = .82f; Style.EndBlob = 4.f; Style.Seed = 3;
    const FString Word = TEXT("FLAVORTOWN");
    const float TextW = HallTextWidth(Word, Style);
    const FVector2D Origin(W - TextW - 40.f, H * .80f);
    FHallTextStyle Outline = Style; Outline.Stroke = 19.f;
    HallDrawText(Img, Word, Origin, Outline, FLinearColor(.16f, .07f, .02f));
    HallDrawText(Img, Word, Origin, Style, FLinearColor(.96f, .90f, .66f));
    // Grommets in the corners.
    for (const FVector2D& P : { FVector2D(14, 14), FVector2D(W - 14, 14), FVector2D(14, H - 14), FVector2D(W - 14, H - 14) })
    { Img.Ellipse(P, FVector2D(7, 7), 0, FLinearColor(.75f, .62f, .30f)); Img.Ellipse(P, FVector2D(3.5f, 3.5f), 0, FLinearColor(.05f, .04f, .03f)); }
}

void PaintPoster(FHallImage& Img)
{
    const int32 W = 600, H = 400;
    Img.Init(W, H, FLinearColor(.06f, .06f, .065f));
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float G = HallNoise(X * .9f, Y * .9f, 11) * .05f + (1.f - FMath::Abs(X - W * .5f) / (W * .5f)) * .18f;
            Img.At(X, Y) += FLinearColor(G, G, G);
        }
    // Pale doorway glow and a city skyline in silhouette.
    Img.Rect(FVector2D(W * .36f, 60), FVector2D(W * .64f, 330), FLinearColor(.62f, .62f, .60f), .9f);
    FRandomStream R(5);
    for (float X = 0; X < W; X += R.FRandRange(26.f, 48.f))
        Img.Rect(FVector2D(X, 330 - R.FRandRange(30.f, 140.f)), FVector2D(X + R.FRandRange(18.f, 40.f), 332), FLinearColor(.03f, .03f, .035f));
    FHallTextStyle Title; Title.Height = 40.f; Title.Stroke = 7.f; Title.Spacing = .3f; Title.Seed = 2;
    const FString Name = TEXT("NIGHT SHIFT");
    HallDrawText(Img, Name, FVector2D(26, H - 22.f), Title, FLinearColor(.94f, .93f, .9f));
}

void PaintComic(FHallImage& Img)
{
    const int32 W = 300, H = 420;
    Img.Init(W, H, FLinearColor(.18f, .12f, .45f));
    Img.Rect(FVector2D(0, 0), FVector2D(W, 92), FLinearColor(.95f, .78f, .10f));
    FHallTextStyle T; T.Height = 30.f; T.Stroke = 7.f; T.Spacing = .2f; T.Seed = 6;
    HallDrawText(Img, TEXT("MEGA"), FVector2D(24, 42), T, FLinearColor(.75f, .10f, .08f));
    HallDrawText(Img, TEXT("COMICS"), FVector2D(24, 82), T, FLinearColor(.75f, .10f, .08f));
    // A burly cartoon strongman in red and green.
    Img.Ellipse(FVector2D(150, 160), FVector2D(28, 30), 0, FLinearColor(.95f, .70f, .55f));
    Img.FillPolygon({ FVector2D(95, 195), FVector2D(205, 195), FVector2D(225, 300), FVector2D(75, 300) }, FLinearColor(.80f, .16f, .10f));
    Img.Rect(FVector2D(95, 300), FVector2D(205, 330), FLinearColor(.10f, .55f, .20f));
    Img.Rect(FVector2D(100, 330), FVector2D(140, 395), FLinearColor(.80f, .16f, .10f)); Img.Rect(FVector2D(160, 330), FVector2D(200, 395), FLinearColor(.80f, .16f, .10f));
    Img.Ellipse(FVector2D(60, 230), FVector2D(18, 40), 25, FLinearColor(.95f, .70f, .55f)); Img.Ellipse(FVector2D(240, 230), FVector2D(18, 40), -25, FLinearColor(.95f, .70f, .55f));
    Img.Ellipse(FVector2D(60, 360), FVector2D(30, 22), 0, FLinearColor(.95f, .95f, .95f));
    Img.Rect(FVector2D(0, 0), FVector2D(W, 5), FLinearColor::White); Img.Rect(FVector2D(0, H - 5), FVector2D(W, H), FLinearColor::White);
    Img.Rect(FVector2D(0, 0), FVector2D(5, H), FLinearColor::White); Img.Rect(FVector2D(W - 5, 0), FVector2D(W, H), FLinearColor::White);
}

void PaintPrint(FHallImage& Img)
{
    // Framed card-game painting: green felt table, lamp light, a few figures around it.
    const int32 W = 480, H = 300;
    Img.Init(W, H, FLinearColor(.18f, .07f, .05f));
    Img.Ellipse(FVector2D(240, 210), FVector2D(200, 70), 0, FLinearColor(.10f, .35f, .16f));
    Img.Ellipse(FVector2D(240, 40), FVector2D(70, 30), 0, FLinearColor(.95f, .75f, .35f), .8f);
    const FLinearColor Fur[4] = { FLinearColor(.45f, .30f, .15f), FLinearColor(.75f, .65f, .5f), FLinearColor(.2f, .15f, .1f), FLinearColor(.6f, .4f, .2f) };
    for (int32 I = 0; I < 4; ++I)
    {
        const float X = 90 + I * 100;
        Img.Ellipse(FVector2D(X, 150), FVector2D(28, 40), 0, Fur[I]);
        Img.Ellipse(FVector2D(X, 100), FVector2D(20, 18), 0, Fur[I]);
        Img.Ellipse(FVector2D(X + 14, 104), FVector2D(10, 6), 0, Fur[I] * .8f);
    }
    for (int32 I = 0; I < 5; ++I) Img.Rect(FVector2D(170 + I * 28, 200), FVector2D(190 + I * 28, 214), FLinearColor(.95f, .95f, .92f));
    for (int32 X = 0; X < W; ++X) for (int32 Y = 0; Y < 14; ++Y) { Img.At(X, Y) = FLinearColor(.06f, .04f, .03f); Img.At(X, H - 1 - Y) = FLinearColor(.06f, .04f, .03f); }
    for (int32 Y = 0; Y < H; ++Y) for (int32 X = 0; X < 14; ++X) { Img.At(X, Y) = FLinearColor(.06f, .04f, .03f); Img.At(W - 1 - X, Y) = FLinearColor(.06f, .04f, .03f); }
}

void PaintTile(FHallImage& Img)
{
    const int32 W = 200, H = 200;
    Img.Init(W, H, FLinearColor(.45f, .65f, .85f));
    for (int32 Y = 100; Y < H; ++Y) for (int32 X = 0; X < W; ++X) Img.At(X, Y) = FLinearColor(.75f, .58f, .35f);
    Img.Ellipse(FVector2D(100, 95), FVector2D(42, 55), 0, FLinearColor(.85f, .72f, .45f));
    Img.Ellipse(FVector2D(100, 80), FVector2D(18, 12), 0, FLinearColor(.25f, .15f, .1f));
}
}

UStaticMeshComponent* ABoywickRoomDecor::Panel(const FName& Name, const FVector& Center, const FRotator& Rotation, float Width, float Height, UMaterialInterface* Material, float Thickness)
{
    static UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, Name, RF_Transient);
    C->SetStaticMesh(Cube);
    C->SetupAttachment(GetRootComponent());
    C->SetWorldLocation(Center); C->SetWorldRotation(Rotation);
    C->SetWorldScale3D(FVector(Width / 100.f, Thickness / 100.f, Height / 100.f));
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetMaterial(0, Material);
    C->RegisterComponent();
    Panels.Add(C);
    return C;
}

void ABoywickRoomDecor::StringLights(const TArray<FVector>& Hooks)
{
    static UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Bulbs = NewObject<UInstancedStaticMeshComponent>(this, TEXT("BoywickBulbs"), RF_Transient);
    Wire = NewObject<UInstancedStaticMeshComponent>(this, TEXT("BoywickWire"), RF_Transient);
    for (UInstancedStaticMeshComponent* C : { Bulbs.Get(), Wire.Get() })
    {
        C->SetupAttachment(GetRootComponent());
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCastShadow(false);
        C->RegisterComponent();
    }
    Bulbs->SetStaticMesh(Sphere); Wire->SetStaticMesh(Cylinder);
    UMaterialInstanceDynamic* BulbMat = PlainMat(this, FLinearColor(.85f, .82f, .72f), .25f, 0.f, FLinearColor(1.f, .8f, .5f) * .6f);
    UMaterialInstanceDynamic* WireMat = PlainMat(this, FLinearColor(.03f, .07f, .04f), .6f);
    Materials.Add(BulbMat); Materials.Add(WireMat);
    Bulbs->SetMaterial(0, BulbMat); Wire->SetMaterial(0, WireMat);
    int32 GlowIndex = 0;
    for (int32 I = 0; I + 1 < Hooks.Num(); ++I)
    {
        const FVector A = Hooks[I], B = Hooks[I + 1];
        const float Length = FVector::Dist(A, B);
        const int32 Steps = FMath::Max(3, FMath::RoundToInt(Length / 11.f));
        FVector Previous = A;
        for (int32 S = 1; S <= Steps; ++S)
        {
            const float T = float(S) / Steps;
            FVector P = FMath::Lerp(A, B, T);
            P.Z -= FMath::Sin(T * PI) * FMath::Min(22.f, Length * .22f);
            const FVector Dir = P - Previous;
            FTransform WireT(FRotationMatrix::MakeFromZ(Dir.GetSafeNormal()).Rotator(), (P + Previous) * .5f, FVector(.004f, .004f, Dir.Size() / 100.f));
            Wire->AddInstance(WireT, true);
            if (S < Steps)
            {
                Bulbs->AddInstance(FTransform(FRotator::ZeroRotator, P - FVector(0, 0, 2.5f), FVector(.020f)), true);
                ++BulbCount;
            }
            Previous = P;
        }
        if ((I % 2) == 0)
        {
            UPointLightComponent* L = NewObject<UPointLightComponent>(this, *FString::Printf(TEXT("BoywickGlow%d"), GlowIndex++), RF_Transient);
            L->SetupAttachment(GetRootComponent());
            L->SetWorldLocation((A + B) * .5f - FVector(0, 0, 24.f));
            L->SetIntensity(12.f);
            L->SetLightColor(FLinearColor(1.f, .78f, .5f));
            L->SetAttenuationRadius(330.f);
            L->SetCastShadows(false);
            L->RegisterComponent();
            Glow.Add(L);
        }
    }
}

void ABoywickRoomDecor::Build()
{
    // The reference furniture and actual photographed artwork are saved in the level.
    // Only the lightweight string lights need local runtime dressing.
    const TArray<FVector> Hooks = {
        FVector(XW + 4.f, -658.8f, ZC - 19.f),
        FVector(XW + 4.f, -838.8f, ZC - 17.f),
        FVector(XW + 4.f, -1018.8f, ZC - 35.f),
        FVector(XW + 4.f, -1183.8f, ZC - 26.f),
        FVector(XW + 5.f, -1202.8f, WindowTop + 8.f),
        FVector(XW + 5.f, -1330.8f, WindowTop + 8.f),
        FVector(XW + 5.f, -1330.8f, ZF + 150.f),
        FVector(XW + 5.f, -1330.8f, ZF + 62.f)
    };
    StringLights(Hooks);
    // The dressing is constructed at the original authoring coordinates; one
    // root transform keeps its wires, bulbs and lights aligned with the house.
    SetActorScale3D(FVector(HouseWorldScale::Factor(GetWorld())));
    for (UPointLightComponent* Light : Glow)
        if (Light) Light->SetAttenuationRadius(330.f * HouseWorldScale::Factor(GetWorld()));
    UE_LOG(LogTemp, Display, TEXT("BOYWICK_DECOR_BUILT referenceRoom=1 bulbs=%d"), BulbCount);
}

// ------------------------------------------------------------------------------------------------
// Subsystem
// ------------------------------------------------------------------------------------------------
bool UBoywickRoomSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBoywickRoomSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (UGameplayStatics::GetCurrentLevelName(&InWorld, true) != TEXT("Chapter01_House")) return;
    if (FParse::Param(FCommandLine::Get(), TEXT("NoBoywick"))) return;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;
    if (ABoywickRoomDecor* Built = InWorld.SpawnActor<ABoywickRoomDecor>(ABoywickRoomDecor::StaticClass(), FTransform::Identity, Params))
    {
        Built->Build();
        Decor = Built;
    }
    if (InWorld.GetNetMode() == NM_Client) return;
    if (TActorIterator<ABoywickCharacter> Existing(&InWorld); Existing) Boywick = *Existing;
    if (!Boywick.IsValid())
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        const FVector Feet = HouseWorldScale::Point(&InWorld, SpawnFeet());
        ABoywickCharacter* B = InWorld.SpawnActor<ABoywickCharacter>(ABoywickCharacter::StaticClass(),
            FTransform(FRotator(0.f, 160.f, 0.f), Feet + FVector(0, 0, 85.f)), Spawn);
        if (B)
        {
            B->RoomMinimum = HouseWorldScale::Point(&InWorld, WanderMin());
            B->RoomMaximum = HouseWorldScale::Point(&InWorld, WanderMax());
            Boywick = B;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("BOYWICK_ROOM boywick=%s decor=%d"), *GetNameSafe(Boywick.Get()), Decor.IsValid() ? 1 : 0);
    if (const ABoywickCharacter* B=Boywick.Get())
        UE_LOG(LogTemp,Display,TEXT("BOYWICK_ROOM_SCALE factor=%.3f feet=%s bounds=[%s..%s] actorScale=%s capsule=[%.1f,%.1f] decorScale=%s"),
            HouseWorldScale::Factor(&InWorld),*B->FeetLocation().ToString(),*B->RoomMinimum.ToString(),*B->RoomMaximum.ToString(),
            *B->GetActorScale3D().ToString(),B->GetCapsuleComponent()->GetScaledCapsuleRadius(),B->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
            Decor.IsValid()?*Decor->GetActorScale3D().ToString():TEXT("none"));
    if (FParse::Param(FCommandLine::Get(), TEXT("BoywickRoomCheck")))
    {
        FTimerHandle LayoutTimer;
        InWorld.GetTimerManager().SetTimer(LayoutTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            UWorld* World = GetWorld();
            APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
            ACharacter* Pawn = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
            if (!World || !Pawn) return;
            const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
            const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            FCollisionQueryParams Query(SCENE_QUERY_STAT(BoywickRoomLayout), false, Pawn);
            if (Boywick.IsValid()) Query.AddIgnoredActor(Boywick.Get());
            const FVector Points[] = {
                FVector(-2415,-1235,ZF), FVector(-2500,-1235,ZF),
                FVector(-2495,-1170,ZF), FVector(-2495,-900,ZF),
                FVector(-2770,-900,ZF), FVector(-2495,-900,ZF),
                FVector(-2495,-750,ZF)
            };
            bool bPass = true;
            for (int32 I=1; I<UE_ARRAY_COUNT(Points); ++I)
            {
                const FVector Lift(0,0,Half+5.f);
                FHitResult Hit;
                const bool bBlocked = World->SweepSingleByChannel(Hit,
                    HouseWorldScale::Point(World,Points[I-1])+Lift,HouseWorldScale::Point(World,Points[I])+Lift,
                    FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Query);
                UE_LOG(LogTemp,Display,TEXT("BWR_ROUTE segment=%d blocked=%d actor=%s component=%s radius=%.1f half=%.1f"),
                    I,bBlocked,*(Cast<AHouseInteractiveProp>(Hit.GetActor()) ? Cast<AHouseInteractiveProp>(Hit.GetActor())->PropName : GetNameSafe(Hit.GetActor())),*GetNameSafe(Hit.GetComponent()),Radius,Half);
                bPass &= !bBlocked;
            }
            int32 Props=0,Loose=0;
            bool bSmallClear=true;
            for (TActorIterator<AHouseInteractiveProp> It(World); It; ++It)
            {
                if (!It->ActorHasTag(TEXT("BoywickPhotoRoom"))) continue;
                ++Props;
                if (It->ActorHasTag(TEXT("SSOBoywickSmallItem")))
                {
                    ++Loose;
                    bSmallClear &= It->Body && It->Body->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Ignore;
                }
            }
            const bool bNPCReady=Boywick.IsValid() && Boywick->IsMetaHumanReady();
            bPass &= Props>=90 && Loose>=60 && bSmallClear && bNPCReady;
            UE_LOG(LogTemp,Display,TEXT("BOYWICK_ROOM_CHECK_%s props=%d loose=%d smallClear=%d nativeNPC=%d"),
                bPass ? TEXT("PASSED") : TEXT("FAILED"), Props,Loose,bSmallClear,bNPCReady);
            if (!bPass) FPlatformMisc::RequestExitWithStatus(false,1);
        }), 1.f, false, 6.f);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BoywickSmoke")))
    {
        // Opt-in end-to-end check: listen to a whole story, receive the blessing, then anger and knock him out.
        SmokeStarted = InWorld.GetTimeSeconds();
        InWorld.GetTimerManager().SetTimer(SmokeTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { TickSmoke(); }), .25f, true, 3.f);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BoywickCheck")))
    {
        // Opt-in diagnostics for headless runs: where the animals and Boywick actually are, every few seconds.
        InWorld.GetTimerManager().SetTimer(CheckTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            UWorld* World = GetWorld();
            if (!World) return;
            for (TActorIterator<AHouseRoamingPig> It(World); It; ++It)
                UE_LOG(LogTemp, Display, TEXT("CLAUDE_CHECK pig=%s feet=%s inside=%d mood=%d health=%.0f bed=%d velocity=%.0f"), *It->PigName, *It->FeetLocation().ToString(),
                    It->IsInsidePigsty() ? 1 : 0, int32(It->Mood), It->Health, It->IsBedAvailable() ? 1 : 0, It->GetVelocity().Size());
            if (ABoywickCharacter* B = Boywick.Get())
                UE_LOG(LogTemp, Display, TEXT("CLAUDE_CHECK boywick feet=%s mood=%d health=%.0f travel=%.0f stories=%d"), *B->FeetLocation().ToString(),
                    int32(B->Mood), B->Health, B->TravelDistance, B->StoriesTold);
        }), 4.f, true, 2.f);
    }
}

void UBoywickRoomSubsystem::TickSmoke()
{
    UWorld* World = GetWorld();
    ABoywickCharacter* B = Boywick.Get();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    AStoryFirstPersonCharacter* Pawn = PC ? Cast<AStoryFirstPersonCharacter>(PC->GetPawn()) : nullptr;
    if (!World || !B || !PC || !Pawn) return;
    const float Now = World->GetTimeSeconds();
    auto Fail = [&](const TCHAR* Why)
    {
        UE_LOG(LogTemp, Error, TEXT("BOYWICK_SMOKE_FAILED step=%d %s"), SmokeStep, Why);
        World->GetTimerManager().ClearTimer(SmokeTimer);
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };
    if (Now - SmokeStarted > 240.f) { Fail(TEXT("timed out")); return; }
    // Keep the player close to Boywick and facing him, like an attentive listener.
    if (SmokeStep <= 2)
    {
        const FVector Spot = B->FeetLocation() + FVector(1, 0, 0) * 150.f + FVector(0, 0, Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f);
        if (FVector::Dist2D(Pawn->GetActorLocation(), Spot) > 60.f) Pawn->SetActorLocation(Spot, false, nullptr, ETeleportType::TeleportPhysics);
        PC->SetControlRotation((B->GetInteractionLocation() - Pawn->GetPawnViewLocation()).Rotation());
    }
    switch (SmokeStep)
    {
    case 0:
        if (!B->IsMetaHumanReady()) { Fail(TEXT("native MetaHuman or locomotion assets missing")); return; }
        B->SetActorLocation(HouseWorldScale::Point(World,BoywickRoomSpec::SpawnFeet()) + FVector(0,0,B->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f), false, nullptr, ETeleportType::TeleportPhysics);
        B->GetCharacterMovement()->StopMovementImmediately();
        Pawn->SetActorLocation(B->FeetLocation()+FVector(150,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f), false, nullptr, ETeleportType::TeleportPhysics);
        PC->bShowMouseCursor=false;
        PC->SetControlRotation((B->GetInteractionLocation()-Pawn->GetPawnViewLocation()).Rotation());
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
        if (Pawn->FocusedBoywick()!=B) { Fail(TEXT("player focus does not reach the NPC")); return; }
        SmokeBaseWalk=Pawn->GetStoryMovement()->MaxWalkSpeed;
        SmokeBaseSprint=Pawn->GetStoryMovement()->SprintSpeed;
        SmokeBaseJump=Pawn->GetStoryMovement()->JumpZVelocity;
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1.f));
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0.f));
        SmokeStep=12;SmokeStepAt=Now;break;
    case 12:
        if (!B->IsTellingTo(Pawn)) { Fail(TEXT("the E key did not start a story")); return; }
        UE_LOG(LogTemp,Display,TEXT("BOYWICK_SMOKE listening title=%s"),*B->CurrentStoryTitle());
        SmokeStep=1;SmokeLine=-1;break;
    case 1:
        if (ABoywickMetaHumanVisual* Visual=B->GetMetaHumanVisual())
        {
            if (Visual->GetFace() && Visual->GetFace()->GetAnimInstance())
                SmokeJawMaximum=FMath::Max(SmokeJawMaximum,Visual->GetFace()->GetAnimInstance()->GetCurveValue(TEXT("CTRL_expressions_jawOpen")));
            if (!bSmokeStoryImage && B->StoryLineElapsed()>2.f)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("BoywickReview/Story.png"),false,false);
                bSmokeStoryImage=true;
            }
        }
        if (B->IsTelling() && B->LineIndex != SmokeLine) { SmokeLine = B->LineIndex; UE_LOG(LogTemp, Display, TEXT("BOYWICK_SMOKE line %d: %s"), SmokeLine, *B->CurrentLine()); }
        if (B->StoriesTold >= 1)
        {
            if (Pawn->BoywickBlessing == 0) { Fail(TEXT("story finished without a blessing")); return; }
            if (SmokeJawMaximum<.005f) { Fail(TEXT("face did not animate while talking")); return; }
            if (!FMath::IsNearlyEqual(Pawn->GetStoryMovement()->SprintSpeed,SmokeBaseSprint*1.4f,1.f)) { Fail(TEXT("speed blessing did not boost sprint")); return; }
            if (Pawn->BoywickBlessingUntil-Now<85.f) { Fail(TEXT("blessing does not last 90 seconds")); return; }
            UE_LOG(LogTemp, Display, TEXT("BOYWICK_SMOKE blessing kind=%d status=%s walk=%.0f sprint=%.0f jump=%.0f"), Pawn->BoywickBlessing, *Pawn->BoywickBlessingStatus(),
                Pawn->GetStoryMovement() ? Pawn->GetStoryMovement()->MaxWalkSpeed : -1.f, Pawn->GetStoryMovement() ? Pawn->GetStoryMovement()->SprintSpeed : -1.f,
                Pawn->GetStoryMovement() ? Pawn->GetStoryMovement()->JumpZVelocity : -1.f);
            SmokeStep = 2; SmokeStepAt = Now;
        }
        break;
    case 2:
        if (Now - SmokeStepAt < 1.f) break;
        UGameplayStatics::ApplyDamage(B, 30.f, PC, Pawn, UDamageType::StaticClass());
        if (B->Mood != EBoywickMood::Angry) { Fail(TEXT("a hit did not make him angry")); return; }
        UE_LOG(LogTemp, Display, TEXT("BOYWICK_SMOKE angry health=%.0f remark=%s"), B->Health, *B->CurrentRemark());
        SmokeStep = 3; SmokeStepAt = Now; break;
    case 3:
        if (B->Shoves >= 1)
        {
            UE_LOG(LogTemp, Display, TEXT("BOYWICK_SMOKE shoved player velocity=%.0f"), Pawn->GetVelocity().Size());
            SmokeStep = 4; SmokeStepAt = Now;
        }
        else if (Now - SmokeStepAt > 20.f) { Fail(TEXT("no shove within 20 s")); return; }
        break;
    case 4:
        if (Now - SmokeStepAt < 2.f) break;
        UGameplayStatics::ApplyDamage(B, 500.f, PC, Pawn, UDamageType::StaticClass());
        if (B->Mood != EBoywickMood::KnockedOut) { Fail(TEXT("lethal damage did not knock him out")); return; }
        UE_LOG(LogTemp, Display, TEXT("BOYWICK_SMOKE knocked out"));
        SmokeStep = 5; SmokeStepAt = Now; break;
    case 5:
        if (B->Mood != EBoywickMood::KnockedOut)
        {
            UE_LOG(LogTemp, Display, TEXT("BOYWICK_SMOKE recovered health=%.0f after=%.0fs stories=%d blessings=%d shoves=%d knockouts=%d"),
                B->Health, Now - SmokeStepAt, B->StoriesTold, B->BlessingsGranted, B->Shoves, B->Knockouts);
            B->BeginStory(Pawn,1);
            PC->SetControlRotation((Pawn->GetPawnViewLocation()-B->GetInteractionLocation()).Rotation());
            SmokeStep=6;SmokeStepAt=Now;
        }
        else if (Now - SmokeStepAt > 60.f) { Fail(TEXT("did not recover from the knockout")); return; }
        break;
    case 6:
        if (Now-SmokeStepAt<4.f) break;
        if (B->IsTelling() || B->BlessingsGranted!=1) { Fail(TEXT("walking away from a story awarded a blessing")); return; }
        Pawn->BoywickBlessingUntil=Now+1.f;
        SmokeStep=7;SmokeStepAt=Now;
        break;
    case 7:
        if (Now-SmokeStepAt<2.f) break;
        if (!FMath::IsNearlyEqual(Pawn->GetStoryMovement()->SprintSpeed,SmokeBaseSprint,1.f)) { Fail(TEXT("speed did not restore after expiry")); return; }
        Pawn->GrantBoywickBlessing(uint8(EBoywickBlessing::Jump),2.f);
        SmokeStep=8;SmokeStepAt=Now;
        break;
    case 8:
        if (Now-SmokeStepAt<.5f) break;
        if (!FMath::IsNearlyEqual(Pawn->GetStoryMovement()->JumpZVelocity,SmokeBaseJump*1.5f,1.f)) { Fail(TEXT("jump blessing did not boost jump")); return; }
        SmokeStep=9;
        break;
    case 9:
        if (Now-SmokeStepAt<3.f) break;
        if (!FMath::IsNearlyEqual(Pawn->GetStoryMovement()->JumpZVelocity,SmokeBaseJump,1.f)) { Fail(TEXT("jump did not restore after expiry")); return; }
        Pawn->GrantBoywickBlessing(uint8(EBoywickBlessing::Might),2.f);
        SmokeStep=10;SmokeStepAt=Now;
        break;
    case 10:
        if (Now-SmokeStepAt<.5f) break;
        if (!FMath::IsNearlyEqual(Pawn->BoywickDamageMultiplier(),1.6f,.01f)) { Fail(TEXT("might blessing did not boost damage")); return; }
        SmokeStep=11;
        break;
    case 11:
        if (Now-SmokeStepAt<3.f) break;
        if (!FMath::IsNearlyEqual(Pawn->BoywickDamageMultiplier(),1.f,.01f)) { Fail(TEXT("damage did not restore after expiry")); return; }
        UE_LOG(LogTemp,Display,TEXT("BOYWICK_SMOKE_PASSED native=1 faceJaw=%.3f stories=%d rewards=speed,jump,might expiry=1 attention=1 shoves=%d recovery=1 travel=%.0f"),SmokeJawMaximum,B->StoriesTold,B->Shoves,B->TravelDistance);
        World->GetTimerManager().ClearTimer(SmokeTimer);
        FPlatformMisc::RequestExitWithStatus(false,0);
        break;
    default: break;
    }
}

// ------------------------------------------------------------------------------------------------
// HUD overlay
// ------------------------------------------------------------------------------------------------
namespace
{
TArray<FString> WrapLine(const FString& Text, int32 MaxChars)
{
    TArray<FString> Out; FString Current;
    TArray<FString> Words; Text.ParseIntoArray(Words, TEXT(" "), true);
    for (const FString& Word : Words)
    {
        if (!Current.IsEmpty() && Current.Len() + 1 + Word.Len() > MaxChars) { Out.Add(Current); Current.Empty(); }
        Current += (Current.IsEmpty() ? TEXT("") : TEXT(" ")) + Word;
    }
    if (!Current.IsEmpty()) Out.Add(Current);
    return Out;
}
}

void BoywickDrawOverlay(AHUD* Hud, AStoryFirstPersonCharacter* Pawn, APlayerController* PC, float S, float W, float H,
    const FLinearColor& Ink, const FLinearColor& Gold)
{
    if (!Hud || !PC || !GEngine) return;
    UFont* Font = GEngine->GetMediumFont();
    auto Text = [&](const FString& T, float X, float Y, const FLinearColor& C, float Scale) { Hud->DrawText(T, C, X, Y, Font, S * Scale); };
    const bool bCursor = PC->bShowMouseCursor;
    for (TActorIterator<ABoywickCharacter> It(Hud->GetWorld()); It; ++It)
    {
        ABoywickCharacter* B = *It;
        FVector LabelWorld; FVector2D Screen;
        if (B->ShouldShowHealthTo(PC, LabelWorld) && PC->ProjectWorldLocationToScreen(LabelWorld, Screen, true)
            && Screen.X > 10 * S && Screen.X < W - 10 * S && Screen.Y > 15 * S && Screen.Y < H - 15 * S)
        {
            const float Width = 132 * S, X = Screen.X - Width * .5f, Y = Screen.Y;
            const FString MoodLabel = B->Mood == EBoywickMood::Angry ? TEXT("FURIOUS") : B->Mood == EBoywickMood::KnockedOut ? TEXT("OUT COLD")
                : B->Mood == EBoywickMood::Talking ? TEXT("TELLING A STORY") : FString();
            Hud->DrawRect(Ink, X - 5 * S, Y - 4 * S, Width + 10 * S, (MoodLabel.IsEmpty() ? 40.f : 53.f) * S);
            Text(FString::Printf(TEXT("%s  %d / %d"), *B->DisplayName, FMath::CeilToInt(B->Health), FMath::RoundToInt(B->MaxHealth)), X + 3 * S, Y, FLinearColor::White, .72f);
            Hud->DrawRect(FLinearColor(.12f, .14f, .13f, 1), X, Y + 23 * S, Width, 7 * S);
            const float Fraction = FMath::Clamp(B->Health / FMath::Max(1.f, B->MaxHealth), 0.f, 1.f);
            Hud->DrawRect(Fraction > .35f ? FLinearColor(.42f, .72f, .39f, 1) : FLinearColor(.9f, .32f, .2f, 1), X, Y + 23 * S, Width * Fraction, 7 * S);
            if (!MoodLabel.IsEmpty()) Text(MoodLabel, X + 3 * S, Y + 33 * S, B->Mood == EBoywickMood::Angry ? FLinearColor(1.f, .45f, .3f) : Gold, .62f);
        }
        if (!Pawn) continue;
        const float Distance = FVector::Dist(B->GetActorLocation(), Pawn->GetActorLocation());
        const FString Remark = B->CurrentRemark();
        const bool bHearStory = B->IsTelling() && (B->Listener == Pawn || Distance < 700.f);
        if (bHearStory || (!Remark.IsEmpty() && Distance < 900.f))
        {
            const float BoxW = 680 * S, BoxX = W * .5f - BoxW * .5f;
            TArray<FString> Lines = WrapLine(bHearStory ? B->CurrentLine() : Remark, 74);
            const float BoxH = (46.f + 22.f * Lines.Num() + (bHearStory ? 16.f : 0.f)) * S;
            const float BoxY = H - 130 * S - BoxH;
            Hud->DrawRect(Ink, BoxX, BoxY, BoxW, BoxH);
            Text(bHearStory ? FString::Printf(TEXT("BOYWICK  |  %s"), *B->CurrentStoryTitle().ToUpper()) : TEXT("BOYWICK"), BoxX + 14 * S, BoxY + 8 * S, Gold, .78f);
            for (int32 I = 0; I < Lines.Num(); ++I) Text(Lines[I], BoxX + 14 * S, BoxY + (34.f + 22.f * I) * S, FLinearColor::White, .86f);
            if (bHearStory)
            {
                const float BarY = BoxY + BoxH - 11 * S;
                Hud->DrawRect(FLinearColor(.12f, .14f, .13f, 1), BoxX + 14 * S, BarY, BoxW - 28 * S, 5 * S);
                Hud->DrawRect(Gold, BoxX + 14 * S, BarY, (BoxW - 28 * S) * B->StoryProgress(), 5 * S);
                if (B->Listener == Pawn)
                    Text(FString::Printf(TEXT("stay close and keep listening  |  reward: %s"), *ABoywickCharacter::BlessingName(B->Stories.IsValidIndex(B->StoryIndex) ? B->Stories[B->StoryIndex].Reward : EBoywickBlessing::None)),
                        BoxX + BoxW - 330 * S, BoxY + 8 * S, FLinearColor(.7f, .8f, .9f), .62f);
            }
        }
    }
    if (Pawn && !bCursor)
        if (ABoywickCharacter* B = Pawn->FocusedBoywick())
        {
            const FString Prompt = B->InteractionPrompt(Pawn);
            float PW = 0.f, PH = 0.f;
            Hud->GetTextSize(Prompt, PW, PH, Font, S * .85f);
            Hud->DrawRect(Ink, W * .5f - PW * .5f - 14 * S, H * .5f + 25 * S, PW + 28 * S, 32 * S);
            Text(Prompt, W * .5f - PW * .5f, H * .5f + 29 * S, B->Mood == EBoywickMood::Angry ? FLinearColor(1.f, .45f, .3f) : Gold, .85f);
        }
    if (Pawn)
    {
        const FString Status = Pawn->BoywickBlessingStatus();
        if (!Status.IsEmpty())
        {
            const float BX = W - 245 * S, BY = 68 * S;
            Hud->DrawRect(Ink, BX, BY, 136 * S, 30 * S);
            Text(Status, BX + 8 * S, BY + 7 * S, Gold, .7f);
        }
    }
}
