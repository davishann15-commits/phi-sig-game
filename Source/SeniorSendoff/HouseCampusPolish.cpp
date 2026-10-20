// Calder State's exterior polish: the actor that builds the areas (see HouseCampusPolish.h).
#include "HouseCampusPolish.h"
#include "HouseCalderWildlife.h"
#include "HouseCalderMapTrees.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseCampusLayout.h"
#include "HouseCampusFall.h"
#include "HouseExteriorSpatial.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "HouseStreetScene.h"
#include "HouseCampusInteriors.h"
#include "HouseBlackHallPaint.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "AudioMixerBlueprintLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/StaticMesh.h"
#include "Async/Async.h"
#include "Misc/ScopeExit.h"
#include "Components/StaticMeshComponent.h"
#include "HouseCampusStaticMesh.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseCalderFeatures.h"

bool HouseCampusPolish::FParts::Clear(const FVector2D& P, float Radius) const
{
    // Off the plan's buildings, roads, lots, walks and plazas, and off every keep-clear spot.
    if (HouseCampusLayout::Occupied(P, Radius) || HouseCampusLayout::OnTrafficLane(P)) return false;
    if (HouseCalderFeatures::InStair(P, Radius) || HouseCalderFeatures::InRamp(P, Radius)) return false;   // the Calder stairs and ramps
    for (const FVector& K : KeepClear) if (FVector2D::Distance(P, FVector2D(K.X, K.Y)) < K.Z + Radius) return false;
    return true;
}

bool HouseCampusPolish::Enabled()
{
    static const bool bOn = HouseCampusLayout::Enabled() && !FParse::Param(FCommandLine::Get(), TEXT("NoCampusPolish"));
    return bOn;
}

void HouseCampusPolish::CreateLook(UHouseBlackHallSubsystem* Look)
{
    if (!Look || Look->Materials.Contains(TEXT("CP_Ready"))) return;
    HallMakePlain(Look, TEXT("CP_Ready"), FLinearColor::Black, 1.f);
    StadiumLook(Look);
    BuildingsLook(Look);
    GroundLook(Look);
    LandmarksLook(Look);
    StreetFurnitureLook(Look);
    TownLook(Look);
}

AHouseCampusPolish::AHouseCampusPolish()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.f;
    // After the camera has moved this frame, so the haze faces the view it is drawn for (a camera cut included).
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    // Static, like the exterior's root: the tiles are static and attach to it; the puffs, the flags and the sounds are
    // movable children.
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Static);
    SetRootComponent(Root);
}

AHouseCampusPolish* AHouseCampusPolish::SpawnFor(AActor* Exterior, UHouseBlackHallSubsystem* Look)
{
    UWorld* World = Exterior ? Exterior->GetWorld() : nullptr;
    if (!World || !Look || !HouseCampusPolish::Enabled()) return nullptr;
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    // At the origin, like the exterior, so every part is in world space; no Owner (the woods audit counts the exterior's).
    AHouseCampusPolish* Actor = World->SpawnActor<AHouseCampusPolish>(AHouseCampusPolish::StaticClass(), FTransform::Identity, Params);
    if (!Actor) return nullptr;
    TArray<FVector> Trees;
    TArray<HouseCampusPolish::FCar> Cars;
    TArray<UInstancedStaticMeshComponent*> Batches;
    Exterior->GetComponents(Batches);
    for (const UInstancedStaticMeshComponent* Batch : Batches)
    {
        if (!Batch) continue;
        const FString Name = Batch->GetName();
        const bool bTree = Name.StartsWith(TEXT("ExteriorTree")) || Name.StartsWith(TEXT("CampusFallTree")), bCar = Name.StartsWith(TEXT("ExteriorCar"));
        if (!bTree && !bCar) continue;
        const FBox Bounds = bCar && Batch->GetStaticMesh() ? Batch->GetStaticMesh()->GetBoundingBox() : FBox(ForceInit);
        for (int32 I = 0; I < Batch->GetInstanceCount(); ++I)
        {
            FTransform T;
            if (!Batch->GetInstanceTransform(I, T, true)) continue;
            if (bTree) Trees.Add(FVector(T.GetLocation().X, T.GetLocation().Y, T.GetScale3D().Z));
            else Cars.Add({ T, Bounds });
        }
    }
    Actor->Build(Look, Trees, Cars);
    return Actor;
}

void AHouseCampusPolish::MakePart(const FHallMesh& M, const TCHAR* Name, bool bCollision, float CullDistance, UHouseBlackHallSubsystem* Look, float Tile, float SpreadTile)
{
    if (SpreadTile > 0.f)
    {
        // A material that would lie in many small tiles (the blinds on every building, litter everywhere) is a draw in each
        // of them: those go to large tiles (<Name>Spread); the one-off ones (signs, plates, a statue) stay in small ones.
        TMap<FName, int32> InTiles;
        for (const FHallMesh& T : HouseSpatialMeshes(M, Tile))
            for (const TPair<FName, FHallSection>& Sc : T.Sections) if (Sc.Value.I.Num() > 0) ++InTiles.FindOrAdd(Sc.Key);
        FHallMesh Local, Spread;
        for (const FName Key : M.Order) if (const FHallSection* Sc = M.Sections.Find(Key)) (InTiles.FindRef(Key) >= 6 ? Spread : Local).Sec(Key) = *Sc;
        MakePart(Local, Name, bCollision, CullDistance, Look, Tile);
        MakePart(Spread, *(FString(Name) + TEXT("Spread")), bCollision, CullDistance, Look, SpreadTile);
        return;
    }
    // In tiles, so each piece culls on its own (Tile cm square). Every material in a tile is a draw of its own, so: small
    // tiles only for the detail that culls close and has many one-off materials; large ones for the parts that are never
    // culled or whose few materials lie everywhere (the skyline, the glow, the lawns, the ground).
    //
    // Each tile is drawn as a static mesh built at runtime (the woods' way): its draws are cached, where a procedural mesh's
    // sections are gathered again every frame on the render thread. What collides keeps a procedural copy that is not drawn
    // (collision only). A dedicated server builds only that.
    const TArray<FHallMesh> Tiles = HouseSpatialMeshes(M, Tile);
    const int32 Sections0 = Sections, Components0 = Components, Triangles0 = Triangles;
    ON_SCOPE_EXIT { UE_LOG(LogTemp, Display, TEXT("CAMPUS_POLISH part=%s components=%d sections=%d tris=%d materials=%d"), Name, Components - Components0, Sections - Sections0, Triangles - Triangles0, M.Sections.Num()); };
    const bool bDrawn = GetNetMode() != NM_DedicatedServer;
    // The light pools fall on the ground and what lies on it (lawns, joints, leaves, litter), not up the furniture.
    const bool bDecals = FCString::Strstr(Name, TEXT("Lawn")) || FCString::Strstr(Name, TEXT("Detail")) || FCString::Strstr(Name, TEXT("Ground"));
    for (int32 I = 0; I < Tiles.Num(); ++I)
    {
        if (Tiles[I].NumTriangles() == 0) continue;
        if (bDrawn)
            if (UStaticMesh* Mesh = HouseCampusStaticMesh::Build(Tiles[I], FName(*FString::Printf(TEXT("SM_%s_%02d"), Name, I)), Look))
            {
                UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, FName(*FString::Printf(TEXT("%s_%02d"), Name, I)));
                C->SetMobility(EComponentMobility::Static);
                C->SetupAttachment(GetRootComponent());
                C->SetStaticMesh(Mesh);
                C->SetCastShadow(false);
                C->bAffectDistanceFieldLighting = false;
                C->SetReceivesDecals(bDecals);
                C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                if (CullDistance > 0.f) C->SetCullDistance(CullDistance);
                C->RegisterComponent();
                AddInstanceComponent(C);
                Drawn.Add(C);
                ++Components;
                Sections += Mesh->GetStaticMaterials().Num();
            }
        Triangles += Tiles[I].NumTriangles();
        if (!bCollision) continue;
        UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, FName(*FString::Printf(TEXT("%sCollision_%02d"), Name, I)));
        PM->SetMobility(EComponentMobility::Static);
        PM->SetupAttachment(GetRootComponent());
        PM->SetVisibility(false);   // drawn by the static mesh above; this one only collides
        PM->SetCastShadow(false);
        PM->bUseComplexAsSimpleCollision = true;
        PM->bUseAsyncCooking = true;
        PM->bAffectDistanceFieldLighting = false;
        PM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        PM->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        PM->RegisterComponent();
        AddInstanceComponent(PM);
        Tiles[I].Build(PM, [Look](FName Key) { return Look->Mat(Key); }, true);
        Meshes.Add(PM);
    }
}

void AHouseCampusPolish::Build(UHouseBlackHallSubsystem* Look, const TArray<FVector>& Trees, const TArray<HouseCampusPolish::FCar>& Cars)
{
    const double BuildStart = FPlatformTime::Seconds();
    HouseCampusPolish::CreateLook(Look);
    HouseCampusPolish::FParts P;
    P.Trees = Trees;
    P.Cars = Cars;
    // Keep clear of the homecoming float and the band's trailer, and of the bonfire's whole roped ring (its sign at 7.4 m).
    for (const FVector& S : { HouseCampusFall::FloatSpot(), HouseCampusFall::TrailerSpot() }) if (S.Z > 0.f) P.KeepClear.Add(S);
    if (HouseCampusFall::BonfireSpot().Z > 0.f) P.KeepClear.Add(FVector(FVector2D(HouseCampusFall::BonfireSpot()), FMath::Max(HouseCampusFall::BonfireSpot().Z, 790.f)));
    HouseCampusPolish::Stadium(P);
    HouseCampusPolish::Buildings(P);
    HouseCampusPolish::Ground(P);
    HouseCampusPolish::Landmarks(P);
    HouseCampusPolish::StreetFurniture(P);
    HouseCampusPolish::LightPools(P);
    HouseCalderMapTrees::TownLook(Look);
    HouseCampusPolish::Town(P);
    // -CampusPolishSkip=solid,detail,ground,glow,far,lawn,town,pools,puffs,flutters (development): leaves layers out, to time each one.
    FString Skip;
    FParse::Value(FCommandLine::Get(), TEXT("CampusPolishSkip="), Skip, false);
    // A dedicated server draws nothing: it keeps what collides and leaves out the rest (and does not tick).
    const bool bDrawn = GetNetMode() != NM_DedicatedServer;
    // The large tiles' size (-CampusPolishTile=<cm>, development): large enough that the few materials lying everywhere are
    // not repeated in every small tile, small enough that a tile behind the camera is still culled.
    float Big = 16000.f;
    FParse::Value(FCommandLine::Get(), TEXT("CampusPolishTile="), Big);
    if (!Skip.Contains(TEXT("solid"))) MakePart(P.Solid, TEXT("CampusPolishSolid"), true, 0.f, Look, 4000.f, Big);
    if (bDrawn && !Skip.Contains(TEXT("detail"))) MakePart(P.Detail, TEXT("CampusPolishDetail"), false, 9000.f, Look, 4000.f, Big);
    if (bDrawn && !Skip.Contains(TEXT("ground"))) MakePart(P.Ground, TEXT("CampusPolishGround"), false, 9000.f, Look, Big);
    if (bDrawn && !Skip.Contains(TEXT("glow"))) MakePart(P.Glow, TEXT("CampusPolishGlow"), false, 0.f, Look, Big);
    if (bDrawn && !Skip.Contains(TEXT("far"))) MakePart(P.Far, TEXT("CampusPolishFar"), false, 0.f, Look, Big);
    if (bDrawn && !Skip.Contains(TEXT("lawn"))) MakePart(P.Lawn, TEXT("CampusPolishLawn"), false, 16000.f, Look, Big);
    if (bDrawn && !Skip.Contains(TEXT("town"))) MakePart(P.Skyline, TEXT("CampusPolishSkyline"), false, 0.f, Look, 100000.f);
    if (!bDrawn || Skip.Contains(TEXT("pools"))) P.Pools.Reset();
    if (!bDrawn || Skip.Contains(TEXT("puffs"))) P.Puffs.Reset();
    if (!bDrawn || Skip.Contains(TEXT("flutters"))) P.Flutters.Reset();
    if (!bDrawn) SetActorTickEnabled(false);
    // The light pools: emissive-only decals (no light), each a soft disc of its colour on whatever is under it.
    if (UMaterialInterface* PoolMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/CampusInterior/M_LightPoolDecal.M_LightPoolDecal"), nullptr, LOAD_NoWarn | LOAD_Quiet))
        for (const HouseCampusPolish::FPool& Pool : P.Pools)
        {
            UDecalComponent* D = NewObject<UDecalComponent>(this);
            D->SetupAttachment(GetRootComponent());
            UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(PoolMat, this);
            M->SetVectorParameterValue(TEXT("Tint"), Pool.Tint);
            M->SetScalarParameterValue(TEXT("Intensity"), Pool.Strength);
            D->SetDecalMaterial(M);
            // 60 cm up and down (the ground's grade, not the lamp post), and as much again as the ground can rise or fall
            // across the pool: its radius times the steepest slope under it, sampled at the centre and round two rings (the
            // Calder terrain; exactly 0 on today's flat campus).
            float Slope = 0.f;
            for (int32 K = 0; K < 17; ++K)
            {
                const float A = 2.f * PI * (K % 8) / 8.f, Rr = K == 0 ? 0.f : K <= 8 ? Pool.Radius * .5f : Pool.Radius;
                const FVector Nrm = HouseExteriorSite::GroundNormal(Pool.At.X + FMath::Cos(A) * Rr, Pool.At.Y + FMath::Sin(A) * Rr);
                Slope = FMath::Max(Slope, float(FVector2D(Nrm.X, Nrm.Y).Size() / FMath::Max(Nrm.Z, .1)));
            }
            D->DecalSize = FVector(60.f + Pool.Radius * Slope, Pool.Radius, Pool.Radius);
            D->SetWorldLocationAndRotation(Pool.At, FRotator(-90.f, 0.f, 0.f));
            D->SetFadeScreenSize(.012f);
            D->RegisterComponent();
            AddInstanceComponent(D);
            PoolDecals.Add(D);
        }
    PoolCount = PoolDecals.Num();
    BuildPuffs(Look, P.Puffs);
    BuildFlutters(Look, P.Flutters);
    UE_LOG(LogTemp, Display, TEXT("CAMPUS_POLISH built tris=%d components=%d sections=%d pools=%d puffs=%d flutters=%d ms=%.1f"), Triangles, Components, Sections, PoolCount, PuffCount, FlutterCount, (FPlatformTime::Seconds() - BuildStart) * 1000.0);
}

// ------------------------------------------------------------------ step 3d: haze and smoke, cloth in the wind, sound
void AHouseCampusPolish::BuildPuffs(UHouseBlackHallSubsystem* Look, const TArray<HouseCampusPolish::FPuff>& In)
{
    if (In.Num() == 0) return;
    FHallImage Img;
    constexpr int32 N = 64;
    Img.Init(N, N, FLinearColor(1, 1, 1, 0));
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const float DX = (X + .5f) / N * 2.f - 1.f, DY = (Y + .5f) / N * 2.f - 1.f, R2 = DX * DX + DY * DY;
            const float Wisp = .75f + .25f * HallNoise(X * .2f, Y * .2f, 9);
            Img.At(X, Y) = FLinearColor(1, 1, 1, R2 >= 1.f ? 0.f : FMath::Square(1.f - R2) * Wisp);
        }
    UTexture2D* Tex = HouseStreetScene::MakeTexture(Img, true, false, TEXT("T_CP_Haze"), false);
    UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(HouseStreetScene::MakeSmokeMaterial(Look, TEXT("CP_Haze"), Tex));
    if (!Mat) return;
    Mat->SetScalarParameterValue(TEXT("Glow"), 2.6f);
    Mat->SetScalarParameterValue(TEXT("Opacity"), 1.f);
    Mat->SetVectorParameterValue(TEXT("SmokeColor"), FLinearColor(.5f, .53f, .58f));
    PuffSpots = In;
    const int32 V = PuffSpots.Num() * 4;
    PuffPos.Init(FVector::ZeroVector, V); PuffNrm.Init(FVector::UpVector, V); PuffCol.Init(FLinearColor::Transparent, V);
    TArray<FVector2D> UV; UV.SetNum(V);
    TArray<int32> Tri;
    for (int32 Q = 0; Q < PuffSpots.Num(); ++Q)
    {
        UV[Q * 4] = FVector2D(0, 1); UV[Q * 4 + 1] = FVector2D(1, 1); UV[Q * 4 + 2] = FVector2D(1, 0); UV[Q * 4 + 3] = FVector2D(0, 0);
        Tri.Append({ Q * 4, Q * 4 + 1, Q * 4 + 2, Q * 4, Q * 4 + 2, Q * 4 + 3 });
        for (int32 K = 0; K < 4; ++K) PuffPos[Q * 4 + K] = PuffSpots[Q].At;
    }
    PuffMesh = NewObject<UProceduralMeshComponent>(this, TEXT("CampusPolishPuffs"));
    PuffMesh->SetMobility(EComponentMobility::Movable);
    PuffMesh->SetupAttachment(GetRootComponent());
    PuffMesh->SetCastShadow(false);
    PuffMesh->SetReceivesDecals(false);
    PuffMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PuffMesh->bAffectDistanceFieldLighting = false;
    PuffMesh->RegisterComponent();
    AddInstanceComponent(PuffMesh);
    PuffMesh->CreateMeshSection_LinearColor(0, PuffPos, Tri, PuffNrm, UV, PuffCol, TArray<FProcMeshTangent>(), false);
    PuffMesh->SetMaterial(0, Mat);
    // Bounds that never cull the haze while it is in view (the quads move every frame).
    PuffMesh->SetBoundsScale(4.f);
    PuffCount = PuffSpots.Num();
}

void AHouseCampusPolish::UpdatePuffs(const FVector& Camera, const FRotator& CameraRotation, float Time, float Cycle)
{
    if (!PuffMesh) return;
    const FRotationMatrix Frame(CameraRotation);
    const FVector Across = Frame.GetUnitAxis(EAxis::Y), Upward = Frame.GetUnitAxis(EAxis::Z), Facing = -CameraRotation.Vector();
    for (int32 Q = 0; Q < PuffSpots.Num(); ++Q)
    {
        const HouseCampusPolish::FPuff& Spot = PuffSpots[Q];
        FVector P = Spot.At;
        float Size = Spot.Size;
        FLinearColor C = Spot.Color;
        if (Spot.Kind == 1)
        {
            // Smoke: each puff rises and grows over a ten-second cycle, drifting with the wind, and fades as it goes.
            const float T = FMath::Fmod(Cycle + Q * .37f, 1.f);
            P += FVector(T * 160.f, T * 60.f, T * 420.f);
            Size *= .6f + T * 1.2f;
            C.A *= FMath::Sin(T * PI);
        }
        else
        {
            // Haze breathes a little and keeps clear of the camera.
            P += FVector(FMath::Sin(Time * .05f + Q) * 80.f, FMath::Cos(Time * .04f + Q * 1.3f) * 80.f, 0.f);
        }
        const float Into = FMath::Clamp(float((FVector::Dist(Camera, P) - Size * .35f) / Size), 0.f, 1.f);
        C.A *= Into;
        const FVector A1 = (Across - Upward) * (Size * .5f), A2 = (Across + Upward) * (Size * .5f);
        PuffPos[Q * 4] = P - A2; PuffPos[Q * 4 + 1] = P + A1; PuffPos[Q * 4 + 2] = P + A2; PuffPos[Q * 4 + 3] = P - A1;
        for (int32 K = 0; K < 4; ++K) { PuffNrm[Q * 4 + K] = Facing; PuffCol[Q * 4 + K] = C; }
    }
    PuffMesh->UpdateMeshSection_LinearColor(0, PuffPos, PuffNrm, TArray<FVector2D>(), PuffCol, TArray<FProcMeshTangent>());
}

namespace HouseCampusPolishFlutterImpl
{
constexpr int32 Segs = 10;
/** A flutter's two edges at time T: Top[i], Bottom[i] for i = 0..Segs. */
void Shape(const HouseCampusPolish::FFlutter& F, float T, int32 Index, TArray<FVector>& Top, TArray<FVector>& Bottom)
{
    Top.Reset(); Bottom.Reset();
    const FVector Along = F.Along.GetSafeNormal2D().IsNearlyZero() ? FVector(1, 0, 0) : F.Along.GetSafeNormal2D();
    const FVector Lat = FVector::CrossProduct(FVector::UpVector, Along).GetSafeNormal();
    const bool bFlag = F.Width > 40.f;
    const float Droop = F.bHanging ? 80.f : bFlag ? 6.f : 25.f, Amp = bFlag ? .12f * F.Length : .2f * F.Length;
    const FVector Dir = F.bHanging ? -FVector::UpVector : (Along * FMath::Cos(FMath::DegreesToRadians(Droop)) - FVector::UpVector * FMath::Sin(FMath::DegreesToRadians(Droop)));
    for (int32 I = 0; I <= Segs; ++I)
    {
        const float X = float(I) / Segs, Wave = FMath::Sin(X * 7.f - T * (bFlag ? 5.f : 8.f) + Index * 1.7f) * X;
        const FVector P = F.Root + Dir * (X * F.Length) + (F.bHanging ? Along : Lat) * (Amp * Wave) + FVector::UpVector * (Amp * .15f * Wave * X);
        Top.Add(P);
        Bottom.Add(P - FVector::UpVector * F.Width + Lat * (Amp * .1f * Wave));
    }
}
}

void AHouseCampusPolish::BuildFlutters(UHouseBlackHallSubsystem* Look, const TArray<HouseCampusPolish::FFlutter>& In)
{
    namespace FI = HouseCampusPolishFlutterImpl;
    if (In.Num() == 0) return;
    FlutterSpots = In;
    FlutterMesh = NewObject<UProceduralMeshComponent>(this, TEXT("CampusPolishFlutters"));
    FlutterMesh->SetMobility(EComponentMobility::Movable);
    FlutterMesh->SetupAttachment(GetRootComponent());
    FlutterMesh->SetCastShadow(false);
    FlutterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FlutterMesh->bAffectDistanceFieldLighting = false;
    FlutterMesh->RegisterComponent();
    AddInstanceComponent(FlutterMesh);
    // One section a material; each flutter is two faces of (Segs + 1) x 2 vertices.
    for (int32 I = 0; I < FlutterSpots.Num(); ++I) FlutterByKey.FindOrAdd(FlutterSpots[I].Key).Add(I);
    int32 Section = 0;
    for (const TPair<FName, TArray<int32>>& K : FlutterByKey)
    {
        TArray<FVector> V, N; TArray<FVector2D> UV; TArray<int32> Tri;
        TArray<FVector> Top, Bottom;
        for (const int32 I : K.Value)
            for (int32 Face = 0; Face < 2; ++Face)
            {
                FI::Shape(FlutterSpots[I], 0.f, I, Top, Bottom);
                const int32 Base = V.Num();
                for (int32 J = 0; J <= FI::Segs; ++J)
                {
                    V.Add(Top[J]); V.Add(Bottom[J]);
                    N.Add(FVector::UpVector); N.Add(FVector::UpVector);
                    UV.Add(FVector2D(float(J) / FI::Segs, 0.f)); UV.Add(FVector2D(float(J) / FI::Segs, 1.f));
                }
                for (int32 J = 0; J < FI::Segs; ++J)
                {
                    const int32 A = Base + J * 2, B = A + 2;
                    if (Face == 0) Tri.Append({ A, B, A + 1, B, B + 1, A + 1 });
                    else Tri.Append({ A, A + 1, B, B, A + 1, B + 1 });
                }
            }
        FlutterMesh->CreateMeshSection(Section, V, Tri, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
        FlutterMesh->SetMaterial(Section, Look->Mat(K.Key));
        FlutterSection.Add(K.Key, Section);
        FlutterPos.Add(V); FlutterNrm.Add(N);
        ++Section;
    }
    FlutterMesh->SetBoundsScale(2.f);
    FlutterCount = FlutterSpots.Num();
}

void AHouseCampusPolish::UpdateFlutters(float Time)
{
    namespace FI = HouseCampusPolishFlutterImpl;
    if (!FlutterMesh) return;
    TArray<FVector> Top, Bottom;
    for (const TPair<FName, TArray<int32>>& K : FlutterByKey)
    {
        const int32* Section = FlutterSection.Find(K.Key);
        if (!Section || !FlutterPos.IsValidIndex(*Section)) continue;
        TArray<FVector>& V = FlutterPos[*Section];
        TArray<FVector>& N = FlutterNrm[*Section];
        int32 W = 0;
        for (const int32 I : K.Value)
        {
            FI::Shape(FlutterSpots[I], Time, I, Top, Bottom);
            for (int32 Face = 0; Face < 2; ++Face)
                for (int32 J = 0; J <= FI::Segs; ++J)
                {
                    const FVector Tan = (Top[FMath::Min(J + 1, FI::Segs)] - Top[FMath::Max(J - 1, 0)]).GetSafeNormal();
                    const FVector Nrm = FVector::CrossProduct(Tan, Bottom[J] - Top[J]).GetSafeNormal() * (Face ? -1.f : 1.f);
                    if (V.IsValidIndex(W + 1)) { V[W] = Top[J]; V[W + 1] = Bottom[J]; N[W] = Nrm; N[W + 1] = Nrm; }
                    W += 2;
                }
        }
        FlutterMesh->UpdateMeshSection(*Section, V, N, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>());
    }
}

bool AHouseCampusPolish::SoundOn() const
{
    static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("NoCampusSound"));
    return !bOff && GEngine && GEngine->UseSound() && GetNetMode() != NM_DedicatedServer;
}

UAudioComponent* AHouseCampusPolish::StartSound(int32 Kind, const FVector& At, bool bSpatial, bool bLoop, float Volume)
{
    if (!SoundOn() || !Pcm.IsValidIndex(Kind) || Pcm[Kind].Num() == 0) return nullptr;
    auto Falloff = [this](bool bFar) -> USoundAttenuation*
    {
        TObjectPtr<USoundAttenuation>& Slot = bFar ? FarFalloff : NearFalloff;
        if (!Slot)
        {
            Slot = NewObject<USoundAttenuation>(this, NAME_None, RF_Transient);
            FSoundAttenuationSettings& A = Slot->Attenuation;
            A.bAttenuate = true; A.bSpatialize = true;
            A.AttenuationShape = EAttenuationShape::Sphere;
            A.AttenuationShapeExtents = FVector(bFar ? 600.f : 300.f, 0.f, 0.f);
            A.FalloffDistance = bFar ? 9000.f : 3500.f;
            A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
            A.bAttenuateWithLPF = true; A.LPFRadiusMin = 1000.f; A.LPFRadiusMax = bFar ? 9000.f : 3500.f; A.LPFFrequencyAtMin = 20000.f; A.LPFFrequencyAtMax = 2500.f;
        }
        return Slot;
    };
    UAudioComponent* C = NewObject<UAudioComponent>(this, NAME_None, RF_Transient);
    C->SetupAttachment(GetRootComponent());
    C->bAutoActivate = false;
    C->bAutoDestroy = false;
    C->bAllowSpatialization = bSpatial;
    if (bSpatial) C->AttenuationSettings = Falloff(Kind == 0 || Kind == 4);
    C->RegisterComponent();
    C->SetWorldLocation(At);
    C->SetVolumeMultiplier(Volume * IndoorLevel);
    USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(C, NAME_None, RF_Transient);
    W->SetSampleRate(HouseCampusPolish::SoundRate);
    W->NumChannels = 1;
    W->Duration = INDEFINITELY_LOOPING_DURATION;
    W->SoundGroup = SOUNDGROUP_Default;
    W->bLooping = false;
    W->bRequiresStopFade = true;
    W->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
    FFeed F;
    F.Comp = C; F.Wave = W; F.Kind = Kind; F.bLoop = bLoop; F.Base = Volume;
    // (Through int32: a negative float converted straight to uint32 is undefined, and gave every loop the same start.)
    F.Cursor = bLoop ? int32((uint32(int32(At.X * 3.f + At.Y * 7.f) + Kind) * 2654435761u) % uint32(Pcm[Kind].Num())) : 0;
    F.Until = bLoop ? 0.0 : GetWorld()->GetTimeSeconds() + double(Pcm[Kind].Num()) / HouseCampusPolish::SoundRate + 3.0;
    Feeds.Add(F);
    C->SetSound(W);
    C->Play();
    ++SoundsStarted;
    return C;
}

void AHouseCampusPolish::UpdateSound(const FVector& View, float DeltaSeconds)
{
    if (!SoundOn()) return;
    if (Pcm.Num() < 6)
    {
        // The six loops take a while to synthesise (the crowd alone is twenty voices over eleven seconds): once, off the
        // game thread; the soundscape starts when they are ready.
        if (!PcmTask.IsValid())
            PcmTask = Async(EAsyncExecution::ThreadPool, []
            {
                TArray<TArray<int16>> Out;
                Out.SetNum(6);
                for (int32 K = 0; K < 6; ++K) HouseCampusPolish::SoundPcm(K, Out[K]);
                return Out;
            });
        if (!PcmTask.IsReady()) return;
        Pcm = PcmTask.Consume();
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("CampusSoundDump")))
        {
            static const TCHAR* Names[] = { TEXT("Crowd"), TEXT("Wind"), TEXT("Crickets"), TEXT("BusIdle"), TEXT("Horn"), TEXT("CarDoor") };
            for (int32 K = 0; K < 6; ++K)
            {
                TArray<uint8> Wav;
                auto U32 = [&Wav](uint32 V) { for (int32 B = 0; B < 4; ++B) Wav.Add(uint8(V >> (8 * B))); };
                auto U16 = [&Wav](uint16 V) { Wav.Add(uint8(V)); Wav.Add(uint8(V >> 8)); };
                const uint32 Bytes = uint32(Pcm[K].Num() * sizeof(int16)), Rate = uint32(HouseCampusPolish::SoundRate);
                Wav.Append(reinterpret_cast<const uint8*>("RIFF"), 4); U32(36 + Bytes); Wav.Append(reinterpret_cast<const uint8*>("WAVEfmt "), 8);
                U32(16); U16(1); U16(1); U32(Rate); U32(Rate * 2); U16(2); U16(16);
                Wav.Append(reinterpret_cast<const uint8*>("data"), 4); U32(Bytes);
                Wav.Append(reinterpret_cast<const uint8*>(Pcm[K].GetData()), int32(Bytes));
                FFileHelper::SaveArrayToFile(Wav, *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CampusPolish") / FString::Printf(TEXT("CampusOutdoor_%s.wav"), Names[K])));
            }
            UE_LOG(LogTemp, Display, TEXT("CAMPUS_POLISH_SOUND dumped=6"));
        }
#endif
    }
    UWorld* W = GetWorld();
    const double Now = W->GetTimeSeconds();
    if (Started < 0.0) Started = Now;
    // On campus (off the house's side of the street) the wind and the crickets come up; indoors everything outside drops.
    const bool bCampus = View.Y < -5600.f;
    const bool bInside = HouseCampusInteriors::DistanceOutside(View) < 1.f;
    CampusLevel = FMath::FInterpConstantTo(CampusLevel, bCampus ? 1.f : 0.f, DeltaSeconds, .4f);
    IndoorLevel = FMath::FInterpConstantTo(IndoorLevel, bInside ? .25f : 1.f, DeltaSeconds, 1.f);
    // A spot Above the ground there (the Calder terrain; on today's flat campus the ground is -60).
    auto OverGround = [](float X, float Y, float Above) { return FVector(X, Y, HouseExteriorSite::GroundHeight(X, Y) + Above); };
    if (!bLoopsStarted)
    {
        bLoopsStarted = true;
        // The crowd going home from the north gates and through the South Lot; the bus idling in the bay (which keeps
        // today's height: the stadium's apron ring, as the buses there do).
        StartSound(0, OverGround(2400.f, -9000.f, 120.f), true, true, .5f);
        StartSound(0, OverGround(4200.f, -21800.f, 120.f), true, true, .4f);
        StartSound(3, FVector(4520.f, -18025.f, 40.f), true, true, .55f);
        Wind = StartSound(1, View, false, true, .001f);
        if(!HouseCalderWildlife::SoundEnabled())Crickets = StartSound(2, View, false, true, .001f);
    }
    // The crowd thins out over the first ten minutes.
    const float Disperse = FMath::Clamp(1.f - float(Now - Started) / 600.f, .3f, 1.f);
    // Now and then a horn honking for the Geese from the road or the lot; car doors in the lot.
    if (NextHonk == 0.0) { NextHonk = Now + SoundRng.FRandRange(20.f, 50.f); NextDoor = Now + SoundRng.FRandRange(6.f, 20.f); }
    if (Now >= NextHonk)
    {
        static const FVector Spots[] = { OverGround(9500.f, -15000.f, 140.f), OverGround(4000.f, -22500.f, 140.f), OverGround(500.f, -19000.f, 140.f), OverGround(9400.f, -9000.f, 140.f) };
        StartSound(4, Spots[SoundRng.RandRange(0, 3)], true, false, .6f);
        NextHonk = Now + SoundRng.FRandRange(35.f, 90.f);
    }
    if (Now >= NextDoor)
    {
        FVector Door(SoundRng.FRandRange(-1000.f, 8000.f), SoundRng.FRandRange(-24800.f, -20800.f), 0.f);
        Door.Z = HouseExteriorSite::GroundHeight(Door.X, Door.Y) + 130.f;
        StartSound(5, Door, true, false, .5f);
        NextDoor = Now + SoundRng.FRandRange(12.f, 40.f);
    }
#if !UE_BUILD_SHIPPING
    {
        // -CampusPolishRecord=<seconds>: the mix, recorded once the loops have been going 15 s (development builds).
        static int32 RecordSeconds = -1;
        static double RecordUntil = -1.0;
        if (RecordSeconds < 0) { RecordSeconds = 0; FParse::Value(FCommandLine::Get(), TEXT("CampusPolishRecord="), RecordSeconds); }
        if (RecordSeconds > 0 && RecordUntil < 0.0 && Now - Started > 15.0)
        {
            UAudioMixerBlueprintLibrary::StartRecordingOutput(this, float(RecordSeconds) + 2.f);
            RecordUntil = Now + RecordSeconds;
            UE_LOG(LogTemp, Display, TEXT("CAMPUS_POLISH_SOUND recording at=%s inside=%d feeds=%d"), *View.ToCompactString(), int32(bInside), Feeds.Num());
        }
        else if (RecordSeconds > 0 && RecordUntil > 0.0 && Now >= RecordUntil)
        {
            const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CampusPolish"));
            IFileManager::Get().MakeDirectory(*Dir, true);
            UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, TEXT("CampusOutdoorSound"), Dir);
            UE_LOG(LogTemp, Display, TEXT("CAMPUS_POLISH_SOUND recorded=%s started=%d"), *(Dir / TEXT("CampusOutdoorSound.wav")), SoundsStarted);
            RecordSeconds = 0;
        }
    }
#endif
    // Keep every wave fed; one-shots end when their sound has been queued and played out.
    Feeds.RemoveAll([](const FFeed& F) { return !F.Comp.IsValid() || !F.Wave.IsValid(); });
    for (FFeed& F : Feeds)
    {
        const TArray<int16>& Data = Pcm.IsValidIndex(F.Kind) ? Pcm[F.Kind] : Pcm[0];
        if (Data.Num() == 0) continue;
        // (Only when the level moves: each call is a command to the audio thread.)
        const float Level = F.Comp.Get() == Wind.Get() ? FMath::Max(.001f, .16f * CampusLevel * IndoorLevel)
            : F.Comp.Get() == Crickets.Get() ? FMath::Max(.001f, .09f * CampusLevel * IndoorLevel)
            : FMath::Max(.001f, F.Base * IndoorLevel * (F.Kind == 0 ? Disperse : 1.f));
        if (FMath::Abs(Level - F.Level) > .002f) { F.Comp->SetVolumeMultiplier(Level); F.Level = Level; }
        if (!F.bLoop)
        {
            if (F.Cursor < Data.Num())
            {
                F.Wave->QueueAudio(reinterpret_cast<const uint8*>(Data.GetData()), Data.Num() * sizeof(int16));
                F.Cursor = Data.Num();
            }
            // Played out, or never played (culled, or no device for this world): either way it goes by its deadline.
            else if (F.Wave->GetAvailableAudioByteCount() <= 0 || Now > F.Until) { F.Comp->Stop(); F.Comp->DestroyComponent(); }
            continue;
        }
        while (F.Wave->GetAvailableAudioByteCount() < HouseCampusPolish::SoundRate * 2)
        {
            const int32 Count = FMath::Min(HouseCampusPolish::SoundRate / 2, Data.Num() - F.Cursor);
            F.Wave->QueueAudio(reinterpret_cast<const uint8*>(Data.GetData() + F.Cursor), Count * sizeof(int16));
            F.Cursor = (F.Cursor + Count) % Data.Num();
        }
    }
}

void AHouseCampusPolish::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UWorld* W = GetWorld();
    APlayerCameraManager* PCM = W ? UGameplayStatics::GetPlayerCameraManager(W, 0) : nullptr;
    if (!PCM) return;
    const FVector Camera = PCM->GetCameraLocation();
    // Every wave and drift rate here turns a whole number of times in 200 pi seconds, and the smoke's cycle is ten seconds,
    // so both clocks wrap without a jump (and stay small enough for float precision).
    const double Seconds = W->GetTimeSeconds();
    const float Time = float(FMath::Fmod(Seconds, 200.0 * UE_DOUBLE_PI)), Cycle = float(FMath::Fmod(Seconds * .1, 1.0));
    UpdatePuffs(Camera, PCM->GetCameraRotation(), Time, Cycle);
    UpdateFlutters(Time);
    UpdateSound(Camera, DeltaSeconds);
}
