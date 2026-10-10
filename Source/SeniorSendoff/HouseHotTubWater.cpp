#include "HouseHotTubWater.h"

#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "SeniorDouli.h"
#include "SeniorSauce.h"
#include "StoryCampaign.h"
#include "UObject/ConstructorHelpers.h"

#if !UE_BUILD_SHIPPING
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#endif

AHouseHotTubWater::AHouseHotTubWater()
{
    bReplicates = true;
    SetReplicateMovement(false);
    SetNetUpdateFrequency(2.f);
    NetCullDistanceSquared = FMath::Square(15000.f);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("WaterOrigin"));
    WaterSurface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ResponsiveWater"));
    WaterSurface->SetupAttachment(RootComponent);
    WaterSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WaterSurface->SetGenerateOverlapEvents(false);
    WaterSurface->SetCastShadow(false);
    WaterSurface->bUseAsyncCooking = true;
    SteamCards = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WarmSteam"));
    SteamCards->SetupAttachment(RootComponent);
    SteamCards->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SteamCards->SetGenerateOverlapEvents(false);
    SteamCards->SetCastShadow(false);
    SteamCards->SetNumCustomDataFloats(1);
    SplashDroplets = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ImpactSpray"));
    SplashDroplets->SetupAttachment(RootComponent);
    SplashFoam = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FloatingSplashFoam"));
    SplashFoam->SetupAttachment(RootComponent);
    for (UInstancedStaticMeshComponent* Pool : {SplashDroplets.Get(), SplashFoam.Get()})
    {
        Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Pool->SetGenerateOverlapEvents(false);
        Pool->SetCastShadow(false);
        Pool->SetCanEverAffectNavigation(false);
    }
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Card(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Drop(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    SteamCardMesh = Card.Object;
    SplashDroplets->SetStaticMesh(Drop.Object);
    SplashFoam->SetStaticMesh(Drop.Object);
}

void AHouseHotTubWater::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHouseHotTubWater, bJetsRunning);
}

void AHouseHotTubWater::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildWater();
    RebuildSteam();
    RebuildSplash();
}

void AHouseHotTubWater::RefreshWater()
{
    RebuildWater();
    RebuildSteam();
    RebuildSplash();
}

void AHouseHotTubWater::BeginPlay()
{
    Super::BeginPlay();
    RebuildWater();
    RebuildSteam();
    RebuildSplash();
    // All simulation values are actor-local centimetres. Keep the placed actor
    // at unit scale, matching the already scaled tub assets.
    UE_LOG(LogTemp, Display, TEXT("SPA_WATER_READY size=%.1fx%.1f depth=%.1f grid=%d triangles=%d steam=%d"),
        WaterSize.X, WaterSize.Y, WaterDepth, GridResolution, MeshTriangles.Num() / 3, SteamStates.Num());
    UE_LOG(LogTemp, Display, TEXT("SPA_WATER_RENDER actor=%s surface=%s steam=%s scale=%s hidden=%d waterVisible=%d waterHidden=%d steamVisible=%d steamHidden=%d waterMat=%s steamMat=%s steamMesh=%s waterBounds=%s steamBounds=%s"),
        *GetActorLocation().ToCompactString(), *WaterSurface->GetComponentLocation().ToCompactString(),
        *SteamCards->GetComponentLocation().ToCompactString(), *GetActorScale3D().ToCompactString(), int32(IsHidden()),
        int32(WaterSurface->IsVisible()), int32(WaterSurface->bHiddenInGame), int32(SteamCards->IsVisible()), int32(SteamCards->bHiddenInGame),
        *GetNameSafe(WaterSurface->GetMaterial(0)), *GetNameSafe(SteamCards->GetMaterial(0)), *GetNameSafe(SteamCards->GetStaticMesh()),
        *WaterSurface->Bounds.Origin.ToCompactString(), *SteamCards->Bounds.Origin.ToCompactString());
    UE_LOG(LogTemp, Display, TEXT("SPA_WATER_GEOMETRY registered=%d,%d renderState=%d,%d waterExtent=%s steamExtent=%s sectionVisible=%d steamInstances=%d"),
        int32(WaterSurface->IsRegistered()), int32(SteamCards->IsRegistered()),
        int32(WaterSurface->IsRenderStateCreated()), int32(SteamCards->IsRenderStateCreated()),
        *WaterSurface->Bounds.BoxExtent.ToCompactString(), *SteamCards->Bounds.BoxExtent.ToCompactString(),
        int32(WaterSurface->IsMeshSectionVisible(0)), SteamCards->GetInstanceCount());
#if !UE_BUILD_SHIPPING
    // Diagnostic-only shader override to separate missing geometry/transform
    // from translucency material problems; it never edits the saved map.
    if (FParse::Param(FCommandLine::Get(), TEXT("SpaWaterDiagnostic")))
    {
        const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
        UE_LOG(LogTemp, Display, TEXT("SPA_WATER_RENDER_FLAGS viewport=%d translucency=%d visualizeLightCulling=%d materials=%d lighting=%d postProcessing=%d"),
            int32(Viewport != nullptr), Viewport ? int32(Viewport->EngineShowFlags.Translucency) : -1,
            Viewport ? int32(Viewport->EngineShowFlags.VisualizeLightCulling) : -1,
            Viewport ? int32(Viewport->EngineShowFlags.Materials) : -1,
            Viewport ? int32(Viewport->EngineShowFlags.Lighting) : -1,
            Viewport ? int32(Viewport->EngineShowFlags.PostProcessing) : -1);
        const TCHAR* RenderVariables[] = {
            TEXT("r.Substrate"), TEXT("r.Substrate.BlendableGBuffer"), TEXT("r.MaterialQualityLevel"),
            TEXT("r.RefractionQuality"), TEXT("r.SeparateTranslucency"), TEXT("r.SeparateTranslucencyScreenPercentage"),
            TEXT("r.Translucency.StandardSeparated"), TEXT("r.Translucency.AutoBeforeDOF"),
            TEXT("r.Translucency.DynamicRes.MinScreenPercentage"), TEXT("r.Translucency.DynamicRes.MaxScreenPercentage"),
            TEXT("r.TranslucencyLightingVolume.Dim"), TEXT("r.SceneColorFormat"), TEXT("r.PostProcessing.PropagateAlpha"),
            TEXT("r.Tonemapper.Quality"), TEXT("r.ForwardShading"), TEXT("r.Mobile.ShadingPath")
        };
        for (const TCHAR* Name : RenderVariables)
        {
            const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name);
            UE_LOG(LogTemp, Display, TEXT("SPA_WATER_RENDER_CVAR %s=%s"), Name,
                Variable ? *Variable->GetString() : TEXT("unregistered"));
        }
        // The probe is an opaque, two-sided teal material authored by the room
        // importer. Keep the original translucent steam visible beside it.
        if (UMaterialInterface* Opaque = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/HotTubRoomV1/M_Spa_RenderProbeOpaque.M_Spa_RenderProbeOpaque")))
        {
            WaterSurface->SetMaterial(0, Opaque);
            WaterSurface->SetVisibility(true); WaterSurface->SetHiddenInGame(false);
            SetActorHiddenInGame(false);
            UE_LOG(LogTemp, Display, TEXT("SPA_WATER_DIAGNOSTIC_OPAQUE material=%s steamMaterial=%s"),
                *GetNameSafe(WaterSurface->GetMaterial(0)), *GetNameSafe(SteamCards->GetMaterial(0)));
        }
        else UE_LOG(LogTemp, Warning, TEXT("SPA_WATER_DIAGNOSTIC_MISSING_PROBE"));
    }
#endif
}

bool AHouseHotTubWater::InsideXY(const FVector& Local, float Inset) const
{
    const float HX = FMath::Max(1.f, float(WaterSize.X) * .5f - Inset);
    const float HY = FMath::Max(1.f, float(WaterSize.Y) * .5f - Inset);
    const float Radius = FMath::Clamp(CornerRadius - Inset, 0.f, FMath::Min(HX, HY));
    const float AX = FMath::Abs(Local.X), AY = FMath::Abs(Local.Y);
    if (AX > HX || AY > HY) return false;
    if (Radius <= KINDA_SMALL_NUMBER) return true;
    const float DX = FMath::Max(0.f, AX - (HX - Radius));
    const float DY = FMath::Max(0.f, AY - (HY - Radius));
    return DX * DX + DY * DY <= Radius * Radius + .01f;
}

bool AHouseHotTubWater::IsPointInsideWater(FVector WorldPosition) const
{
    const FVector Local = GetActorTransform().InverseTransformPosition(WorldPosition);
    return InsideXY(Local) && Local.Z <= MaximumWaveHeight && Local.Z >= -WaterDepth;
}

void AHouseHotTubWater::RebuildWater()
{
    if (!WaterSurface) return;
    GridResolution = FMath::Clamp(GridResolution, 17, 65);
    WaterSize.X = FMath::Clamp(WaterSize.X, 80., 800.);
    WaterSize.Y = FMath::Clamp(WaterSize.Y, 80., 800.);
    WaterDepth = FMath::Clamp(WaterDepth, 15.f, 150.f);
    CornerRadius = FMath::Clamp(CornerRadius, 0.f, float(FMath::Min(WaterSize.X, WaterSize.Y)) * .49f);
    const int32 N = GridResolution, Count = N * N;
    Heights.Init(0.f, Count); Speeds.Init(0.f, Count); Accelerations.Init(0.f, Count); FoamLevels.Init(0.f, Count);
    ActiveCells.Init(0, Count);
    MeshVertices.SetNum(Count); MeshNormals.Init(FVector::UpVector, Count);
    MeshColors.Init(FColor::Black, Count); MeshUVs.SetNum(Count);
    MeshTriangles.Reset();
    CellWidth = float(WaterSize.X) / (N - 1);
    CellDepth = float(WaterSize.Y) / (N - 1);
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const int32 I = Y * N + X;
            MeshVertices[I] = FVector(X * CellWidth - WaterSize.X * .5, Y * CellDepth - WaterSize.Y * .5, 0);
            MeshUVs[I] = FVector2D(float(X) / (N - 1), float(Y) / (N - 1));
            ActiveCells[I] = InsideXY(MeshVertices[I]) ? 1 : 0;
            if (X == N - 1 || Y == N - 1) continue;
            // Only include triangles fully inside the rounded tub opening.
            // Hidden unused border vertices stay flat and never accumulate energy.
            const int32 A = I, B = I + 1, C = I + N, D = I + N + 1;
            const FVector VB((X + 1) * CellWidth - WaterSize.X * .5, Y * CellDepth - WaterSize.Y * .5, 0);
            const FVector VC(X * CellWidth - WaterSize.X * .5, (Y + 1) * CellDepth - WaterSize.Y * .5, 0);
            const FVector VD((X + 1) * CellWidth - WaterSize.X * .5, (Y + 1) * CellDepth - WaterSize.Y * .5, 0);
            const bool bA = InsideXY(MeshVertices[I]), bB = InsideXY(VB), bC = InsideXY(VC), bD = InsideXY(VD);
            if (bA && bB && bC) { MeshTriangles.Add(A); MeshTriangles.Add(C); MeshTriangles.Add(B); }
            if (bB && bD && bC) { MeshTriangles.Add(B); MeshTriangles.Add(C); MeshTriangles.Add(D); }
        }
    TArray<FProcMeshTangent> Tangents;
    WaterSurface->ClearAllMeshSections();
    WaterSurface->CreateMeshSection(0, MeshVertices, MeshTriangles, MeshNormals, MeshUVs, MeshColors, Tangents, false);
    if (WaterMaterial) WaterSurface->SetMaterial(0, WaterMaterial);
    WaterSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SimulationAccumulator = RenderAccumulator = JetAccumulator = 0.f;
}

void AHouseHotTubWater::RebuildSteam()
{
    if (!SteamCards) return;
    SteamCards->ClearInstances();
    SteamStates.Reset();
    SteamCards->SetNumCustomDataFloats(1);
    SteamCards->SetStaticMesh(SteamCardMesh);
    if (SteamMaterial) SteamCards->SetMaterial(0, SteamMaterial);
    SteamCards->SetVisibility(SteamMaterial != nullptr && SteamCardMesh != nullptr && SteamCount > 0);
    FRandomStream Random(71377);
    for (int32 I = 0; I < FMath::Clamp(SteamCount, 0, 32); ++I)
    {
        FSteamState S;
        S.Start = FVector2D(Random.FRandRange(-.35f, .35f) * WaterSize.X, Random.FRandRange(-.35f, .35f) * WaterSize.Y);
        S.Phase = Random.FRand(); S.Duration = Random.FRandRange(5.8f, 8.8f);
        S.Sway = Random.FRandRange(0.f, 2.f * PI); S.Size = Random.FRandRange(.43f, .75f);
        SteamStates.Add(S);
        const int32 Index = SteamCards->AddInstance(FTransform(FRotator(90, 0, 0), FVector(S.Start, 8.f), FVector(S.Size)));
        SteamCards->SetCustomDataValue(Index, 0, 0.f, false);
    }
    UpdateSteam(0.f);
}

void AHouseHotTubWater::RebuildSplash()
{
    if (!SplashDroplets || !SplashFoam) return;
    SplashDroplets->ClearInstances(); SplashFoam->ClearInstances();
    DropletStates.SetNum(DropletPoolSize); FoamStates.SetNum(FoamPoolSize);
    for (FSplashState& State : DropletStates) State = FSplashState();
    for (FSplashState& State : FoamStates) State = FSplashState();
    if (SplashMaterial) SplashDroplets->SetMaterial(0, SplashMaterial);
    else if (WaterMaterial) SplashDroplets->SetMaterial(0, WaterMaterial);
    if (FoamMaterial) SplashFoam->SetMaterial(0, FoamMaterial);
    else if (UMaterialInterface* Plain = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        SplashFoam->SetMaterial(0, Plain);
    const FTransform Hidden(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
    for (int32 I = 0; I < DropletPoolSize; ++I) SplashDroplets->AddInstance(Hidden);
    for (int32 I = 0; I < FoamPoolSize; ++I) SplashFoam->AddInstance(Hidden);
    NextDroplet = NextFoam = ActiveSplashParticleCount = ActiveFoamParticleCount = 0;
}

void AHouseHotTubWater::AddWaterImpulse(FVector WorldPosition, float Strength, float Radius)
{
    if (!HasAuthority() || WorldPosition.ContainsNaN() || !FMath::IsFinite(Strength) || !FMath::IsFinite(Radius)) return;
    MulticastWaterImpulse(WorldPosition, FMath::Clamp(Strength, -120.f, 120.f), FMath::Clamp(Radius, 4.f, 90.f));
}

void AHouseHotTubWater::MulticastWaterImpulse_Implementation(FVector_NetQuantize Position, float Strength, float Radius)
{
    ApplyLocalImpulse(Position, Strength, Radius);
}

void AHouseHotTubWater::AddWaterSplash(FVector WorldPosition, float Strength, float Radius, FVector WorldVelocity)
{
    if (!HasAuthority() || WorldPosition.ContainsNaN() || WorldVelocity.ContainsNaN() ||
        !FMath::IsFinite(Strength) || !FMath::IsFinite(Radius)) return;
    MulticastWaterSplash(WorldPosition, FMath::Clamp(FMath::Abs(Strength), 8.f, 100.f),
        FMath::Clamp(Radius, 8.f, 55.f), WorldVelocity.GetClampedToMaxSize(1100.f));
}

void AHouseHotTubWater::MulticastWaterSplash_Implementation(FVector_NetQuantize Position, float Strength, float Radius, FVector_NetQuantize Velocity)
{
    const FVector Local = GetActorTransform().InverseTransformPosition(Position);
    if (!InsideXY(Local)) return;
    // The body depresses water locally and displaces it outwards. A wider
    // compensating positive impulse avoids draining the entire basin over time.
    ApplyLocalImpulse(Position, -Strength, Radius);
    ApplyLocalImpulse(Position, Strength * .275f, Radius * 1.9f);
    EmitSplash(Position, Strength, Radius, Velocity);
}

void AHouseHotTubWater::EmitSplash(FVector WorldPosition, float Strength, float Radius, FVector WorldVelocity)
{
    if (DropletStates.Num() != DropletPoolSize || FoamStates.Num() != FoamPoolSize) return;
    const FVector Centre = GetActorTransform().InverseTransformPosition(WorldPosition);
    if (!InsideXY(Centre)) return;
    const FVector Travel = GetActorTransform().InverseTransformVectorNoScale(WorldVelocity);
    const float Impact = FMath::Clamp(Strength / 70.f, .15f, 1.4f);
    FRandomStream Random(19631 + SplashBurstCount * 109);
    ++SplashBurstCount;
    const int32 SprayCount = FMath::Clamp(FMath::RoundToInt(8.f + 17.f * Impact), 10, 32);
    for (int32 I = 0; I < SprayCount; ++I)
    {
        const float Angle = (I + Random.FRandRange(-.2f, .2f)) * (2.f * PI / SprayCount);
        const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0);
        FVector Position = Centre + Out * Random.FRandRange(Radius * .35f, Radius * .85f);
        if (!InsideXY(Position, 2.f)) continue;
        Position.Z = SampleHeight(Position) + 1.5f;
        FSplashState& S = DropletStates[NextDroplet];
        S = FSplashState(); S.bActive = true; S.Position = Position;
        S.Velocity = Out * Random.FRandRange(35.f, 90.f) * Impact + FVector(Travel.X, Travel.Y, 0) * .1f;
        S.Velocity.Z = Random.FRandRange(125.f, 240.f) * FMath::Sqrt(Impact);
        S.Lifetime = 1.05f; S.Size = Random.FRandRange(.011f, .024f) * FMath::Sqrt(Impact);
        NextDroplet = (NextDroplet + 1) % DropletPoolSize;
    }
    const int32 FoamCount = FMath::Clamp(FMath::RoundToInt(7.f + 8.f * Impact), 8, 18);
    for (int32 I = 0; I < FoamCount; ++I)
    {
        const float Angle = Random.FRandRange(0.f, 2.f * PI);
        const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0);
        FVector Position = Centre + Out * Random.FRandRange(Radius * .25f, Radius * 1.05f);
        if (!InsideXY(Position, 2.f)) continue;
        Position.Z = SampleHeight(Position) + .3f;
        FSplashState& S = FoamStates[NextFoam];
        S = FSplashState(); S.bActive = true; S.Position = Position;
        S.Velocity = Out * Random.FRandRange(12.f, 29.f) * Impact + FVector(Travel.X, Travel.Y, 0) * .055f;
        S.Lifetime = Random.FRandRange(.7f, 1.7f); S.Size = Random.FRandRange(.025f, .075f) * FMath::Sqrt(Impact);
        S.Yaw = Random.FRandRange(0.f, 2.f * PI);
        NextFoam = (NextFoam + 1) % FoamPoolSize;
    }
}

void AHouseHotTubWater::ApplyLocalImpulse(FVector WorldPosition, float Strength, float Radius)
{
    if (Heights.IsEmpty()) return;
    const FVector Local = GetActorTransform().InverseTransformPosition(WorldPosition);
    if (!InsideXY(Local)) return;
    const float Radius2 = FMath::Square(FMath::Max(4.f, Radius));
    for (int32 I = 0; I < MeshVertices.Num(); ++I)
    {
        if (!ActiveCells[I]) continue;
        const float D2 = FMath::Square(float(MeshVertices[I].X - Local.X)) + FMath::Square(float(MeshVertices[I].Y - Local.Y));
        if (D2 > Radius2 * 4.f) continue;
        const float Weight = FMath::Exp(-2.f * D2 / Radius2);
        Speeds[I] = FMath::Clamp(Speeds[I] + Strength * Weight, -160.f, 160.f);
    }
    ++ReceivedImpulseCount;
}

void AHouseHotTubWater::StepWaves(float Step)
{
    const int32 N = GridResolution;
    // CFL-safe wave speed even if the editor chooses a tiny high-resolution
    // surface. This bounds the 2D explicit solver through long render frames.
    const float MaxStableSpeed = .62f / (Step * FMath::Sqrt(1.f / FMath::Square(CellWidth) + 1.f / FMath::Square(CellDepth)));
    const float C2 = FMath::Square(FMath::Min(FMath::Clamp(WaveSpeed, 15.f, 90.f), MaxStableSpeed));
    const float Damp = FMath::Clamp(WaveDamping, .2f, 4.f);
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const int32 I = Y * N + X;
            if (!ActiveCells[I]) { Accelerations[I] = 0.f; continue; }
            const float H = Heights[I];
            auto Adjacent = [&](int32 NX, int32 NY) { const int32 NI = NY * N + NX; return ActiveCells[NI] ? Heights[NI] : H; };
            const float L = X > 0 ? Adjacent(X - 1, Y) : H;
            const float R = X + 1 < N ? Adjacent(X + 1, Y) : H;
            const float B = Y > 0 ? Adjacent(X, Y - 1) : H;
            const float T = Y + 1 < N ? Adjacent(X, Y + 1) : H;
            Accelerations[I] = C2 * ((L + R - 2.f * H) / FMath::Square(CellWidth) +
                (B + T - 2.f * H) / FMath::Square(CellDepth)) - 2.f * Damp * Speeds[I] - .35f * H;
        }
    const float Limit = FMath::Clamp(MaximumWaveHeight, .5f, 6.f);
    for (int32 I = 0; I < Heights.Num(); ++I)
    {
        if (!ActiveCells[I]) continue;
        Speeds[I] = FMath::Clamp(Speeds[I] + Accelerations[I] * Step, -160.f, 160.f);
        const float Proposed = Heights[I] + Speeds[I] * Step;
        Heights[I] = FMath::Clamp(Proposed, -Limit, Limit);
        if (FMath::Abs(Proposed) > Limit) Speeds[I] *= .25f;
        if (!FMath::IsFinite(Heights[I]) || !FMath::IsFinite(Speeds[I])) { Heights[I] = 0.f; Speeds[I] = 0.f; }
        // Foam follows disturbance energy and lingers briefly as the wake
        // spreads. Stationary characters add no new energy or foam.
        const float FreshFoam = FMath::Clamp(FMath::Abs(Speeds[I]) / 58.f, 0.f, 1.f);
        FoamLevels[I] = FMath::Max(FreshFoam, FoamLevels[I] * FMath::Exp(-1.7f * Step));
    }
}

void AHouseHotTubWater::UpdateWaterMesh()
{
    const int32 N = GridResolution;
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const int32 I = Y * N + X;
            MeshVertices[I].Z = Heights[I];
            auto Adjacent = [&](int32 NX, int32 NY) { const int32 NI = NY * N + NX; return ActiveCells[NI] ? Heights[NI] : Heights[I]; };
            const float L = X > 0 ? Adjacent(X - 1, Y) : Heights[I];
            const float R = X + 1 < N ? Adjacent(X + 1, Y) : Heights[I];
            const float B = Y > 0 ? Adjacent(X, Y - 1) : Heights[I];
            const float T = Y + 1 < N ? Adjacent(X, Y + 1) : Heights[I];
            MeshNormals[I] = FVector(-(R - L) / (2.f * CellWidth), -(T - B) / (2.f * CellDepth), 1.f).GetSafeNormal();
            const float Foam = FoamLevels[I];
            MeshColors[I] = FColor(FMath::RoundToInt(Foam * 255.f), 0, 0, 255);
        }
    TArray<FProcMeshTangent> Tangents;
    WaterSurface->UpdateMeshSection(0, MeshVertices, MeshNormals, MeshUVs, MeshColors, Tangents);
}

float AHouseHotTubWater::GetMaximumDisplacement() const
{
    float Max = 0.f;
    for (float H : Heights) Max = FMath::Max(Max, FMath::Abs(H));
    return Max;
}

float AHouseHotTubWater::SampleHeight(const FVector& Local) const
{
    if (Heights.IsEmpty() || !InsideXY(Local)) return 0.f;
    const float GX = FMath::Clamp(float((Local.X + WaterSize.X * .5) / CellWidth), 0.f, float(GridResolution - 1));
    const float GY = FMath::Clamp(float((Local.Y + WaterSize.Y * .5) / CellDepth), 0.f, float(GridResolution - 1));
    const int32 X = FMath::Min(FMath::FloorToInt(GX), GridResolution - 2);
    const int32 Y = FMath::Min(FMath::FloorToInt(GY), GridResolution - 2);
    const int32 I = Y * GridResolution + X;
    return FMath::Lerp(FMath::Lerp(Heights[I], Heights[I + 1], GX - X),
        FMath::Lerp(Heights[I + GridResolution], Heights[I + GridResolution + 1], GX - X), GY - Y);
}

float AHouseHotTubWater::GetDisplacementAtWorld(FVector WorldPosition) const
{
    return SampleHeight(GetActorTransform().InverseTransformPosition(WorldPosition));
}

float AHouseHotTubWater::GetWaveEnergy() const
{
    if (Heights.IsEmpty()) return 0.f;
    double Energy = 0;
    int32 Count = 0;
    const int32 N = GridResolution;
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const int32 I = Y * N + X;
            if (!ActiveCells[I]) continue;
            const float DX = X + 1 < N && ActiveCells[I + 1] ? (Heights[I + 1] - Heights[I]) / CellWidth : 0.f;
            const float DY = Y + 1 < N && ActiveCells[I + N] ? (Heights[I + N] - Heights[I]) / CellDepth : 0.f;
            Energy += .5 * (FMath::Square(Speeds[I]) + FMath::Square(WaveSpeed) * (DX * DX + DY * DY) + .35f * FMath::Square(Heights[I]));
            ++Count;
        }
    return Count > 0 ? float(Energy / Count) : 0.f;
}

void AHouseHotTubWater::UpdateSteam(float DeltaSeconds)
{
    FVector CameraLocal(0, -300, 180);
    if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        FVector Camera; FRotator Rotation; PC->GetPlayerViewPoint(Camera, Rotation);
        CameraLocal = GetActorTransform().InverseTransformPosition(Camera);
    }
    for (int32 I = 0; I < SteamStates.Num(); ++I)
    {
        FSteamState& S = SteamStates[I];
        S.Phase = FMath::Fmod(S.Phase + DeltaSeconds / S.Duration, 1.f);
        const float U = S.Phase;
        const FVector Position(S.Start.X + FMath::Sin(U * 4.f + S.Sway) * (7.f + 19.f * U),
            S.Start.Y + FMath::Cos(U * 3.f + S.Sway) * (6.f + 15.f * U), 6.f + SteamRiseHeight * U);
        const FQuat Facing = FRotationMatrix::MakeFromZ((CameraLocal - Position).GetSafeNormal()).ToQuat();
        const float Size = S.Size * (.65f + 1.1f * U);
        SteamCards->UpdateInstanceTransform(I, FTransform(Facing, Position, FVector(Size, Size * 1.25f, Size)), false, false, true);
        const float Opacity = FMath::Pow(FMath::Max(0.f, FMath::Sin(U * PI)), 1.4f) * FMath::Clamp(SteamOpacity, 0.f, .25f);
        SteamCards->SetCustomDataValue(I, 0, Opacity, false);
    }
    if (!SteamStates.IsEmpty()) SteamCards->MarkRenderStateDirty();
}

void AHouseHotTubWater::UpdateSplash(float DeltaSeconds)
{
    const FTransform Hidden(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
    bool bDropletsChanged = false, bFoamChanged = false;
    ActiveSplashParticleCount = ActiveFoamParticleCount = 0;
    for (int32 I = 0; I < DropletStates.Num(); ++I)
    {
        FSplashState& S = DropletStates[I];
        if (!S.bActive) continue;
        bDropletsChanged = true;
        S.Age += DeltaSeconds;
        constexpr float Gravity = -650.f;
        S.Position += S.Velocity * DeltaSeconds + FVector(0, 0, .5f * Gravity * DeltaSeconds * DeltaSeconds);
        S.Velocity.Z += Gravity * DeltaSeconds;
        const bool bInBasin = InsideXY(S.Position, 1.f);
        const bool bLanded = S.Velocity.Z < 0.f && S.Position.Z <= SampleHeight(S.Position) + .4f;
        if (S.Age >= S.Lifetime || !bInBasin || bLanded)
        {
            if (bLanded && bInBasin)
                ApplyLocalImpulse(GetActorTransform().TransformPosition(S.Position),
                    FMath::Clamp(FMath::Abs(float(S.Velocity.Z)) * .007f, .4f, 2.2f), 5.f);
            S.bActive = false;
            SplashDroplets->UpdateInstanceTransform(I, Hidden, false, false, true);
            continue;
        }
        const FQuat Facing = FRotationMatrix::MakeFromZ(S.Velocity.GetSafeNormal()).ToQuat();
        const float Stretch = FMath::Clamp(float(S.Velocity.Size()) / 125.f, 1.15f, 2.1f);
        SplashDroplets->UpdateInstanceTransform(I, FTransform(Facing, S.Position,
            FVector(S.Size, S.Size, S.Size * Stretch)), false, false, true);
        ++ActiveSplashParticleCount;
    }
    for (int32 I = 0; I < FoamStates.Num(); ++I)
    {
        FSplashState& S = FoamStates[I];
        if (!S.bActive) continue;
        bFoamChanged = true;
        S.Age += DeltaSeconds;
        S.Position += S.Velocity * DeltaSeconds;
        S.Velocity *= FMath::Exp(-2.6f * DeltaSeconds);
        if (S.Age >= S.Lifetime || !InsideXY(S.Position, 2.f))
        {
            S.bActive = false;
            SplashFoam->UpdateInstanceTransform(I, Hidden, false, false, true);
            continue;
        }
        S.Position.Z = SampleHeight(S.Position) + .35f;
        const float DX = SampleHeight(S.Position + FVector(2, 0, 0)) - SampleHeight(S.Position - FVector(2, 0, 0));
        const float DY = SampleHeight(S.Position + FVector(0, 2, 0)) - SampleHeight(S.Position - FVector(0, 2, 0));
        const FVector Normal = FVector(-DX * .25f, -DY * .25f, 1.f).GetSafeNormal();
        const FQuat Facing = FRotationMatrix::MakeFromZ(Normal).ToQuat() * FQuat(FVector::UpVector, S.Yaw);
        const float Shrink = FMath::Pow(FMath::Clamp(1.f - S.Age / S.Lifetime, 0.f, 1.f), .7f);
        SplashFoam->UpdateInstanceTransform(I, FTransform(Facing, S.Position,
            FVector(S.Size * Shrink, S.Size * .5f * Shrink, .0025f * Shrink)), false, false, true);
        ++ActiveFoamParticleCount;
    }
    if (bDropletsChanged) SplashDroplets->MarkRenderStateDirty();
    if (bFoamChanged) SplashFoam->MarkRenderStateDirty();
}

void AHouseHotTubWater::TickCharacters(float DeltaSeconds)
{
    const float Now = GetWorld()->GetTimeSeconds();
    const FTransform Transform = GetActorTransform();
    for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
    {
        ACharacter* Character = *It;
        if (!Character || !Character->GetCapsuleComponent() || !Character->GetCharacterMovement()) continue;
        UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
        const FVector Feet = Character->GetActorLocation() - FVector(0, 0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        const FVector Local = Transform.InverseTransformPosition(Feet);
        FPawnWaterState& State = PawnStates.FindOrAdd(Character);
        const float SurfaceHeight = SampleHeight(Local);
        // A small hysteresis band keeps one splash from being retriggered by
        // the waves at the edge. The tub collision remains wholly independent.
        const bool bInside = InsideXY(Local, State.bInside ? 2.f : 5.f) &&
            Local.Z < SurfaceHeight + (State.bInside ? 12.f : 4.f) && Local.Z > -WaterDepth - 12.f;
        FVector Crossing;
        const bool bCrossedDown = State.bSeen &&
            Transform.InverseTransformPosition(State.PreviousFeet).Z > 0.f && Local.Z <= 0.f &&
            SweepWaterCrossing(State.PreviousFeet, Feet, Crossing);
        const FVector ObservedVelocity = State.bSeen ? (Feet - State.PreviousFeet) / FMath::Max(DeltaSeconds, .001f) : Movement->Velocity;
        const float ActualSpeed = FMath::Min(float(ObservedVelocity.Size2D()), 650.f);
        FVector Surface = Transform.TransformPosition(FVector(Local.X, Local.Y, 0));
        if (HasAuthority())
        {
            if (!State.bInside && (bInside || bCrossedDown))
            {
                if (bCrossedDown) Surface = Crossing;
                const float FallSpeed = FMath::Max(FMath::Max(0.f, float(-ObservedVelocity.Z)), FMath::Max(0.f, float(-Movement->Velocity.Z)));
                AddWaterSplash(Surface, FMath::Clamp(36.f + ActualSpeed * .065f + FallSpeed * .12f, 36.f, 95.f), 26.f, ObservedVelocity);
                ++PawnEntryImpulseCount;
                State.LastRippleAt = State.LastSplashAt = Now; State.LastRippleFeet = Feet;
            }
            else if (bInside && State.bInside && Now - State.LastRippleAt >= .12f && ActualSpeed > 8.f &&
                FVector::DistSquared2D(State.LastRippleFeet, Feet) >= 9.f)
            {
                // Actual displacement rather than input velocity avoids making
                // waves when the player is holding a direction against a wall.
                const FVector Travel = (Feet - State.LastRippleFeet).GetSafeNormal2D();
                const FVector Across(-Travel.Y, Travel.X, 0);
                const FVector Foot = Surface + Across * ((State.Step++ & 1) ? 8.f : -8.f);
                const FVector Wake = InsideXY(Transform.InverseTransformPosition(Foot), 3.f) ? Foot : Surface;
                const float Strength = FMath::Clamp(12.f + ActualSpeed * .1f, 12.f, 56.f);
                AddWaterImpulse(Wake + Travel * 10.f, -Strength, 16.f);
                AddWaterImpulse(Wake - Travel * 12.f, Strength * .72f, 19.f);
                ++PawnMotionImpulseCount;
                if (Now - State.LastSplashAt >= .3f)
                {
                    AddWaterSplash(Wake, Strength * .55f, 14.f, ObservedVelocity);
                    State.LastSplashAt = Now;
                }
                State.LastRippleAt = Now; State.LastRippleFeet = Feet;
            }
            else if (!bInside && State.bInside)
            {
                // Lifted feet leave a trailing splash at their last wet position.
                FVector ExitLocal = Transform.InverseTransformPosition(State.PreviousFeet);
                ExitLocal.Z = 0;
                if (InsideXY(ExitLocal))
                    AddWaterSplash(Transform.TransformPosition(ExitLocal), FMath::Clamp(18.f + ActualSpeed * .025f, 18.f, 34.f), 17.f, ObservedVelocity);
                ++PawnExitImpulseCount;
                State.LastSplashAt = Now;
            }
        }
        // Existing immersion forces run inside StoryMovementComponent physics;
        // this visual simulation never changes movement mode, gravity or input.
        State.bInside = bInside;
        State.PreviousFeet = Feet; State.bSeen = true;
        if (!bInside) State.LastRippleFeet = Feet;
    }
    for (auto It = PawnStates.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
}

void AHouseHotTubWater::TickPhysicsBodies(float DeltaSeconds)
{
    if (!HasAuthority()) return;
    TArray<FOverlapResult> Overlaps;
    FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_PhysicsBody); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SpaWaterBodies), false, this);
    const FVector Centre = GetActorTransform().TransformPosition(FVector(0, 0, -WaterDepth * .5f + 20.f));
    const FVector Extent(WaterSize.X * .5f + 20.f, WaterSize.Y * .5f + 20.f, WaterDepth * .5f + 50.f);
    GetWorld()->OverlapMultiByObjectType(Overlaps, Centre, GetActorQuat(), Objects, FCollisionShape::MakeBox(Extent), Query);
    const float Now = GetWorld()->GetTimeSeconds();
    TSet<UPrimitiveComponent*> Seen;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        UPrimitiveComponent* Body = Overlap.GetComponent();
        if (!Body || Seen.Contains(Body) || !Body->IsSimulatingPhysics()) continue;
        Seen.Add(Body);
        const FVector Local = GetActorTransform().InverseTransformPosition(Body->GetComponentLocation());
        const FVector Velocity = Body->GetPhysicsLinearVelocity();
        FBodyWaterState& State = BodyStates.FindOrAdd(Body);
        const float Radius = FMath::Clamp(float(Body->Bounds.BoxExtent.Z), 3.f, 60.f);
        FVector Surface;
        const FVector CurrentSurface = Body->GetComponentLocation() - FVector(0, 0, Radius * .65f);
        const FVector PreviousSurface = GetActorTransform().TransformPosition(State.PreviousLocal) - FVector(0, 0, Radius * .65f);
        if ((State.bSeen && SweepWaterCrossing(PreviousSurface, CurrentSurface, Surface)) ||
            (!State.bSeen && Local.Z < Radius * .65f && Local.Z > -WaterDepth && Velocity.Z < -35.f && InsideXY(Local)))
        {
            if (!State.bSeen) Surface = GetActorTransform().TransformPosition(FVector(Local.X, Local.Y, 0));
            AddWaterSplash(Surface, FMath::Clamp(FMath::Abs(Velocity.Z) * .14f, 18.f, 95.f), FMath::Clamp(Radius * 1.8f, 14.f, 55.f), Velocity);
            ++PhysicsImpactImpulseCount;
        }
        if (bAffectPhysicsBodies && InsideXY(Local) && Local.Z > -WaterDepth - Radius && Local.Z < Radius)
        {
            const float Submerged = FMath::Clamp((Radius - Local.Z) / (2.f * Radius), 0.f, 1.f);
            const float Mass = Body->GetMass();
            // Buoyant support and drag are actual Chaos forces. No permanent
            // damping or gravity settings are overwritten on the dropped body.
            Body->AddForce(FVector(0, 0, FMath::Abs(GetWorld()->GetGravityZ()) * Mass * 1.15f * Submerged) - Velocity * Mass * 2.3f * Submerged);
        }
        State.PreviousLocal = Local; State.bSeen = true; State.LastSeenAt = Now;
    }
    for (auto It = BodyStates.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || Now - It.Value().LastSeenAt > 2.f) It.RemoveCurrent();
}

bool AHouseHotTubWater::SweepWaterCrossing(const FVector& PreviousWorld, const FVector& CurrentWorld, FVector& SurfaceWorld) const
{
    const FTransform Transform = GetActorTransform();
    const FVector A = Transform.InverseTransformPosition(PreviousWorld), B = Transform.InverseTransformPosition(CurrentWorld);
    if ((A.Z > 0.f && B.Z > 0.f) || (A.Z < 0.f && B.Z < 0.f) || FMath::IsNearlyEqual(float(A.Z), float(B.Z))) return false;
    const float T = float(-A.Z / (B.Z - A.Z));
    if (T < 0.f || T > 1.f) return false;
    const FVector Point = FMath::Lerp(A, B, T);
    if (!InsideXY(Point, 2.f)) return false;
    SurfaceWorld = Transform.TransformPosition(FVector(Point.X, Point.Y, 0));
    return true;
}

void AHouseHotTubWater::TickThrowables(float DeltaSeconds)
{
    if (!HasAuthority()) return;
    auto Track = [&](AActor* Actor, bool bFlying)
    {
        const FVector Current = Actor->GetActorLocation();
        if (const FVector* Previous = ThrowablePrevious.Find(Actor))
        {
            FVector Surface;
            if (bFlying && SweepWaterCrossing(*Previous, Current, Surface))
            {
                const float Speed = FVector::Dist(*Previous, Current) / FMath::Max(DeltaSeconds, .001f);
                AddWaterSplash(Surface, FMath::Clamp(Speed * .025f, 20.f, 65.f), 18.f, (Current - *Previous) / FMath::Max(DeltaSeconds, .001f));
                ++ThrowableImpulseCount;
            }
        }
        ThrowablePrevious.Add(Actor, Current);
    };
    for (TActorIterator<ASeniorDouli> It(GetWorld()); It; ++It) Track(*It, !It->IsHeld());
    for (TActorIterator<ASeniorSaucePacket> It(GetWorld()); It; ++It) Track(*It, It->Phase == ESeniorSaucePhase::Flying);
    for (auto It = ThrowablePrevious.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
}

void AHouseHotTubWater::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Heights.IsEmpty() || DeltaSeconds <= 0.f) return;
    const float Delta = FMath::Min(DeltaSeconds, .08f);
    TickCharacters(Delta);
    TickPhysicsBodies(Delta);
    TickThrowables(Delta);
    if (bJetsRunning)
    {
        JetAccumulator += Delta;
        if (JetAccumulator >= .18f)
        {
            JetAccumulator = FMath::Fmod(JetAccumulator, .18f);
            const float X = (JetStep & 1) ? 1.f : -1.f;
            const float Y = (JetStep & 2) ? 1.f : -1.f;
            ApplyLocalImpulse(GetActorTransform().TransformPosition(FVector(X * WaterSize.X * .29, Y * WaterSize.Y * .29, 0)), 3.5f, 16.f);
            ++JetStep;
        }
    }
    constexpr float Step = 1.f / 120.f;
    // At most ten tiny grid steps per rendered frame: no backlog spiral.
    SimulationAccumulator = FMath::Min(SimulationAccumulator + Delta, Step * 10.f);
    while (SimulationAccumulator >= Step) { StepWaves(Step); SimulationAccumulator -= Step; }
    RenderAccumulator += Delta;
    if (RenderAccumulator >= 1.f / 30.f)
    {
        const float SteamDelta = RenderAccumulator;
        RenderAccumulator = FMath::Fmod(RenderAccumulator, 1.f / 30.f);
        UpdateWaterMesh(); UpdateSteam(SteamDelta); UpdateSplash(SteamDelta);
    }
    TickSmoke(DeltaSeconds);
}

void AHouseHotTubWater::EndPlay(const EEndPlayReason::Type Reason)
{
    RestoreSmokePawn();
#if !UE_BUILD_SHIPPING
    if (SmokeBody.IsValid()) SmokeBody->Destroy();
#endif
    Super::EndPlay(Reason);
}

void AHouseHotTubWater::RestoreSmokePawn()
{
#if !UE_BUILD_SHIPPING
    if (SmokePawn.IsValid())
    {
        ACharacter* Pawn = SmokePawn.Get();
        Pawn->SetActorTransform(SmokePawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
        if (UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement())
        {
            Movement->SetMovementMode(EMovementMode(SmokePawnMovementMode), SmokePawnCustomMode);
            Movement->Velocity = SmokePawnVelocity;
        }
        if (AController* Controller = Pawn->GetController()) Controller->SetControlRotation(SmokeControlRotation);
        SmokePawn.Reset();
    }
#endif
}

void AHouseHotTubWater::TickSmoke(float DeltaSeconds)
{
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(), TEXT("SpaRoomSmoke")) || SmokePhase < 0 || !HasAuthority()) return;
    const double Now = FPlatformTime::Seconds();
    auto Fail = [&](const TCHAR* Why)
    {
        UE_LOG(LogTemp, Error, TEXT("SPA_ROOM_SMOKE_FAILED %s"), Why);
        RestoreSmokePawn(); if (SmokeBody.IsValid()) SmokeBody->Destroy(); bJetsRunning = bSmokeSavedJets;
        SmokePhase = -1; FPlatformMisc::RequestExitWithStatus(false, 1);
    };
    if (SmokePhase == 0)
    {
        FString IsolatedDir;
        if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), IsolatedDir) || IsolatedDir.IsEmpty())
        { Fail(TEXT("Requires isolated -UserDir to protect the player save")); return; }
        SmokeStarted = Now; bSmokeSavedJets = bJetsRunning; SmokePhase = 1;
    }
    else if (SmokePhase == 1 && Now - SmokeStarted >= 3.)
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        AStoryFirstPersonCharacter* Pawn = PC ? Cast<AStoryFirstPersonCharacter>(PC->GetPawn()) : nullptr;
        if (!Pawn) { Fail(TEXT("Actual first-person character did not load")); return; }
        if (!WaterMaterial || !SteamMaterial || SteamStates.Num() < 8 || MeshTriangles.Num() < 900 ||
            WaterSurface->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
        { Fail(TEXT("Water material, steam pool or nonblocking surface not configured")); return; }
        SmokePawn = Pawn; SmokePawnTransform = Pawn->GetActorTransform();
        SmokeControlRotation = PC->GetControlRotation(); SmokePawnVelocity = Pawn->GetCharacterMovement()->Velocity;
        SmokePawnGravity = Pawn->GetCharacterMovement()->GravityScale; SmokePawnMaxSpeed = Pawn->GetCharacterMovement()->MaxWalkSpeed;
        SmokePawnMovementMode = uint8(Pawn->GetCharacterMovement()->MovementMode); SmokePawnCustomMode = Pawn->GetCharacterMovement()->CustomMovementMode;
        bJetsRunning = false; Heights.Init(0.f, Heights.Num()); Speeds.Init(0.f, Speeds.Num());
        AddWaterImpulse(GetActorLocation(), 45.f, 23.f);
        SmokeStarted = Now; SmokePhase = 2;
    }
    else if (SmokePhase == 2 && Now - SmokeStarted >= .25)
    {
        SmokeInitialEnergy = GetWaveEnergy();
        if (SmokeInitialEnergy <= .001f || GetMaximumDisplacement() <= .05f)
        { Fail(TEXT("Injected disturbance did not displace the actual water mesh")); return; }
        UE_LOG(LogTemp, Display, TEXT("SPA_WAVE_PROPAGATION peakCm=%.4f energy=%.6f"), GetMaximumDisplacement(), SmokeInitialEnergy);
        SmokePhase = 3;
    }
    else if (SmokePhase == 3 && Now - SmokeStarted >= 5.)
    {
        if (!FMath::IsFinite(GetWaveEnergy()) || GetWaveEnergy() >= SmokeInitialEnergy * .25f || GetMaximumDisplacement() > MaximumWaveHeight + .01f)
        { Fail(TEXT("Wave solver failed damping or bounds")); return; }
        UE_LOG(LogTemp, Display, TEXT("SPA_WAVE_DECAY initial=%.6f final=%.6f peakCm=%.4f"), SmokeInitialEnergy, GetWaveEnergy(), GetMaximumDisplacement());
        ACharacter* Pawn = SmokePawn.Get();
        const float HH = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Pawn->SetActorLocation(GetActorTransform().TransformPosition(FVector(-40, 0, HH - 20)), false, nullptr, ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        Pawn->GetCharacterMovement()->Velocity = GetActorQuat().RotateVector(FVector(180, 0, 0));
        SmokeInitialPawnEntries = PawnEntryImpulseCount; SmokeInitialMotion = PawnMotionImpulseCount;
        SmokeStarted = Now; SmokePhase = 4;
    }
    else if (SmokePhase == 4 && Now - SmokeStarted >= .7)
    {
        ACharacter* Pawn = SmokePawn.Get();
        UCharacterMovementComponent* Movement = Pawn ? Pawn->GetCharacterMovement() : nullptr;
        if (!Movement || PawnEntryImpulseCount <= SmokeInitialPawnEntries || PawnMotionImpulseCount <= SmokeInitialMotion ||
            Movement->GravityScale != SmokePawnGravity || Movement->MaxWalkSpeed != SmokePawnMaxSpeed)
        { Fail(TEXT("Actual pawn entry/movement did not disturb water or changed persistent movement settings")); return; }
        UE_LOG(LogTemp, Display, TEXT("SPA_PAWN_RESPONSE entry=%d motion=%d gravityUnchanged=1 speedSettingUnchanged=1"), PawnEntryImpulseCount - SmokeInitialPawnEntries, PawnMotionImpulseCount - SmokeInitialMotion);
        // Move the actual player out of the water while the drop is measured.
        Pawn->SetActorTransform(SmokePawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
        Movement->SetMovementMode(EMovementMode(SmokePawnMovementMode), SmokePawnCustomMode); Movement->Velocity = FVector::ZeroVector;
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AActor* BodyActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), GetActorLocation() + FVector(45, 0, 100), FRotator::ZeroRotator, Params);
        if (!BodyActor) { Fail(TEXT("Physics drop fixture did not spawn")); return; }
        UStaticMeshComponent* Ball = NewObject<UStaticMeshComponent>(BodyActor, TEXT("TransientWaterDrop"));
        BodyActor->SetRootComponent(Ball); BodyActor->AddInstanceComponent(Ball);
        Ball->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
        Ball->SetWorldScale3D(FVector(.2f)); Ball->SetCollisionProfileName(TEXT("PhysicsActor"));
        Ball->RegisterComponent(); Ball->SetWorldLocation(GetActorLocation() + FVector(45, 0, 100));
        Ball->SetSimulatePhysics(true); Ball->SetEnableGravity(true); Ball->SetPhysicsLinearVelocity(FVector(0, 0, -220));
        SmokeBody = BodyActor; SmokeInitialPhysics = PhysicsImpactImpulseCount;
        SmokeStarted = Now; SmokePhase = 5;
    }
    else if (SmokePhase == 5 && Now - SmokeStarted >= 1.3)
    {
        if (PhysicsImpactImpulseCount <= SmokeInitialPhysics)
        { Fail(TEXT("Real falling Chaos body did not generate a water crossing impulse")); return; }
        UE_LOG(LogTemp, Display, TEXT("SPA_PHYSICS_DROP impacts=%d"), PhysicsImpactImpulseCount - SmokeInitialPhysics);
        if (SmokeBody.IsValid()) SmokeBody->Destroy(); SmokeBody.Reset();
        AStoryFirstPersonCharacter* Pawn = Cast<AStoryFirstPersonCharacter>(SmokePawn.Get());
        if (!Pawn || (!Pawn->Douli && !Pawn->SaucePacket)) { Fail(TEXT("Player throwable unavailable for crossing test")); return; }
        const float HH = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Pawn->SetActorLocation(GetActorTransform().TransformPosition(FVector(0, 0, HH - 12)), false, nullptr, ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking); Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
        if (AController* Controller = Pawn->GetController()) Controller->SetControlRotation(FRotator(-88, 0, 0));
        SmokeInitialThrowables = ThrowableImpulseCount;
        SmokeStarted = Now; SmokePhase = 6;
    }
    else if (SmokePhase == 6 && Now - SmokeStarted >= .15)
    {
        AStoryFirstPersonCharacter* Pawn = Cast<AStoryFirstPersonCharacter>(SmokePawn.Get());
        const bool Started = Pawn && (Pawn->Douli ? Pawn->Douli->BeginThrow() : Pawn->SaucePacket && Pawn->SaucePacket->BeginThrow());
        if (!Started) { Fail(TEXT("Actual equipped throwable could not begin flight")); return; }
        SmokeStarted = Now; SmokePhase = 7;
    }
    else if (SmokePhase == 7 && Now - SmokeStarted >= 1.2)
    {
        if (ThrowableImpulseCount <= SmokeInitialThrowables)
        { Fail(TEXT("Actual equipped throwable crossing did not disturb the water")); return; }
        UE_LOG(LogTemp, Display, TEXT("SPA_THROWABLE_CROSSING impulses=%d"), ThrowableImpulseCount - SmokeInitialThrowables);
        RestoreSmokePawn(); bJetsRunning = bSmokeSavedJets;
        UE_LOG(LogTemp, Display, TEXT("SPA_ROOM_SMOKE_PASSED propagated=1 decayed=1 pawn=1 physicsDrop=1 throwable=1 steamCards=%d"), SteamStates.Num());
        SmokePhase = -1; FPlatformMisc::RequestExitWithStatus(false, 0);
    }
    if (SmokePhase > 1 && Now - SmokeStarted > 15.) Fail(TEXT("Water smoke phase timed out"));
#endif
}
