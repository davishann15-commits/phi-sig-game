#include "HouseReactiveFixture.h"

#include "HouseDamageSubsystem.h"
#include "HouseSfx.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float SwingStiffness = 8.6f;
constexpr float SwingDamping = 1.05f;

// Closed-form integration gives the same motion at 30, 60 or 120 fps and
// consumes a slow frame's full elapsed time without unstable spring steps.
void AdvanceDampedSpring(FVector2D& Position, FVector2D& Velocity, const FVector2D& Target,
    float Stiffness, float Damping, float DeltaSeconds)
{
    if (DeltaSeconds <= 0.f) return;
    const float Decay = Damping * .5f;
    const float Frequency = FMath::Sqrt(FMath::Max(.001f, Stiffness - Decay * Decay));
    const float Envelope = FMath::Exp(-Decay * DeltaSeconds);
    const float Sine = FMath::Sin(Frequency * DeltaSeconds);
    const float Cosine = FMath::Cos(Frequency * DeltaSeconds);
    const FVector2D Displacement = Position - Target;
    const FVector2D PreviousVelocity = Velocity;
    Position = Target + (Displacement * Cosine + (PreviousVelocity + Displacement * Decay)
        * (Sine / Frequency)) * Envelope;
    Velocity = (PreviousVelocity * Cosine - (PreviousVelocity * Decay + Displacement * Stiffness)
        * (Sine / Frequency)) * Envelope;
}

void AdvanceCurtainGather(float& Position, float& Velocity, float Target, float Frequency, float DeltaSeconds)
{
    const float Displacement = Position - Target;
    const float Momentum = Velocity + Frequency * Displacement;
    const float Envelope = FMath::Exp(-Frequency * DeltaSeconds);
    Position = Target + (Displacement + Momentum * DeltaSeconds) * Envelope;
    Velocity = (Velocity - Frequency * Momentum * DeltaSeconds) * Envelope;
}
}

AHouseReactiveFixture::AHouseReactiveFixture()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    bReplicates = true;
    SetNetUpdateFrequency(15.f);
    SetCanBeDamaged(true);
    Tags.Add(TEXT("HouseReactiveFixture"));
    // The weapon uses this tag to pass point damage to scenery.
    Tags.Add(TEXT("HouseBreakable"));
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("FixedFixtureAnchor"));
    RootComponent = Root;
    MotionPivot = CreateDefaultSubobject<USceneComponent>(TEXT("FixtureMotionPivot"));
    MotionPivot->SetupAttachment(Root);
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OriginalFixture"));
    Body->SetupAttachment(MotionPivot);
    Body->SetMobility(EComponentMobility::Movable);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BreakawayFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BreakawayWindowGrille"));
    BreakawayFrame->SetupAttachment(Body);
    BreakawayFrame->SetMobility(EComponentMobility::Movable);
    // The pane's existing hit volume covers the grille until the final hit.
    BreakawayFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("FixtureInteractionVolume"));
    HitVolume->SetupAttachment(Body);
    HitVolume->SetCollisionObjectType(ECC_WorldDynamic);
    HitVolume->SetGenerateOverlapEvents(false);
    HitVolume->SetNotifyRigidBodyCollision(true);
    GlassShards = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BrokenGlassPieces"));
    GlassShards->SetupAttachment(Root);
    GlassShards->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GlassShards->SetCastShadow(false);
    GlassShards->bVisibleInRayTracing = false;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    GlassShards->SetStaticMesh(Cube.Object);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShardMaterial(
        TEXT("/Game/SeniorSendOff/House/Interactivity/M_WindowShard.M_WindowShard"));
    GlassShards->SetMaterial(0, ShardMaterial.Object);
    GlassEdgeGlints = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SettledGlassEdgeGlints"));
    GlassEdgeGlints->SetupAttachment(Root);
    GlassEdgeGlints->SetStaticMesh(Cube.Object);
    GlassEdgeGlints->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GlassEdgeGlints->SetCastShadow(false);
    GlassEdgeGlints->bVisibleInRayTracing = false;
    GlassCracks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GlassImpactCracks"));
    GlassCracks->SetupAttachment(Body);
    GlassCracks->SetStaticMesh(Cube.Object);
    GlassCracks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GlassCracks->SetCastShadow(false);
    GlassCracks->bVisibleInRayTracing = false;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CrackMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    GlassCracks->SetMaterial(0, CrackMaterial.Object);
    GlassEdgeGlints->SetMaterial(0, CrackMaterial.Object);
}

void AHouseReactiveFixture::ConfigureGeometry()
{
    Body->SetStaticMesh(OriginalMesh);
    Body->EmptyOverrideMaterials();
    for (int32 Index = 0; Index < MaterialOverrides.Num(); ++Index)
        if (MaterialOverrides[Index]) Body->SetMaterial(Index, MaterialOverrides[Index]);
    BreakawayFrame->SetStaticMesh(BreakawayFrameMesh);
    BreakawayFrame->SetRelativeTransform(FTransform::Identity);
    BreakawayFrame->EmptyOverrideMaterials();
    for (int32 Index=0; Index<BreakawayFrameMaterials.Num(); ++Index)
        if (BreakawayFrameMaterials[Index]) BreakawayFrame->SetMaterial(Index,BreakawayFrameMaterials[Index]);
    BreakawayFrame->SetVisibility(BreakawayFrameMesh!=nullptr && !BreakState.bBroken);
    BreakawayFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MotionPivot->SetRelativeLocation(MotionPivotLocal);
    FTransform Local = MeshTransform;
    Local.SetTranslation(MeshTransform.GetTranslation() - MotionPivotLocal);
    Body->SetRelativeTransform(Local);
    if (OriginalMesh)
    {
        const FBoxSphereBounds Bounds = OriginalMesh->GetBounds();
        HitVolume->SetRelativeLocation(Bounds.Origin);
        HitVolume->SetBoxExtent(Bounds.BoxExtent.ComponentMax(FVector(1.2f)));
    }
    HitVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    HitVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    HitVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    HitVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
    if (FixtureKind == EHouseReactiveFixtureKind::Window)
        HitVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    HitVolume->SetCollisionEnabled((BreakState.bBroken || !OriginalMesh) ? ECollisionEnabled::NoCollision
        : (FixtureKind == EHouseReactiveFixtureKind::Curtain
            ? ECollisionEnabled::QueryOnly : ECollisionEnabled::QueryAndPhysics));
    Body->SetVisibility(!BreakState.bBroken);
    if (FixtureKind == EHouseReactiveFixtureKind::Curtain) UpdateCurtainPose();
    else MotionPivot->SetRelativeScale3D(FVector::OneVector);
}

void AHouseReactiveFixture::RefreshFixture()
{
    if (!HasActorBegunPlay())
    {
        bCurtainOpen = bInitiallyOpen;
        CurtainProgress = bInitiallyOpen ? 1.f : 0.f;
        CurtainVelocity = 0.f;
        CurtainSway = CurtainSwayVelocity = FVector2D::ZeroVector;
    }
    ConfigureGeometry();
}

void AHouseReactiveFixture::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (!HasActorBegunPlay())
    {
        bCurtainOpen = bInitiallyOpen;
        CurtainProgress = bInitiallyOpen ? 1.f : 0.f;
        CurtainVelocity = 0.f;
        CurtainSway = CurtainSwayVelocity = FVector2D::ZeroVector;
    }
    ConfigureGeometry();
}

void AHouseReactiveFixture::BeginPlay()
{
    Super::BeginPlay();
    ConfigureGeometry();
    HitVolume->OnComponentHit.AddDynamic(this, &AHouseReactiveFixture::OnFixtureHit);
    // A late-joining client must not hear an old break as though it just happened.
    LastAudibleGlassDamage = BreakState.AccumulatedDamage;
    bGlassShatterSoundPlayed = BreakState.bBroken;
    if (BreakState.bBroken) OnRep_Broken();
    else SetActorTickEnabled(false);
}

void AHouseReactiveFixture::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHouseReactiveFixture, bCurtainOpen);
    DOREPLIFETIME(AHouseReactiveFixture, ReplicatedSwing);
    DOREPLIFETIME(AHouseReactiveFixture, BreakState);
    DOREPLIFETIME(AHouseReactiveFixture, FixtureKind);
    DOREPLIFETIME(AHouseReactiveFixture, FixtureId);
    DOREPLIFETIME(AHouseReactiveFixture, OriginalMesh);
    DOREPLIFETIME(AHouseReactiveFixture, MaterialOverrides);
    DOREPLIFETIME(AHouseReactiveFixture, MeshTransform);
    DOREPLIFETIME(AHouseReactiveFixture, MotionPivotLocal);
    DOREPLIFETIME(AHouseReactiveFixture, GlassBreakingDamage);
    DOREPLIFETIME(AHouseReactiveFixture, BreakawayFrameMesh);
    DOREPLIFETIME(AHouseReactiveFixture, BreakawayFrameMaterials);
}

void AHouseReactiveFixture::OnRep_Configuration()
{
    ConfigureGeometry();
    if (BreakState.AccumulatedDamage > 0.f || BreakState.bBroken) OnRep_Broken();
}

bool AHouseReactiveFixture::CanReach(const APawn* Player) const
{
    if (!Player || !Body || !OriginalMesh) return false;
    const FVector Nearest = Body->Bounds.GetBox().GetClosestPointTo(Player->GetPawnViewLocation());
    return FVector::DistSquared(Nearest, Player->GetPawnViewLocation()) <= FMath::Square(290.f);
}

FString AHouseReactiveFixture::FocusLabel() const
{
    if (BreakState.bBroken) return FString();
    if (FixtureKind == EHouseReactiveFixtureKind::Curtain)
        return bCurtainOpen ? TEXT("E CLOSE CURTAIN") : TEXT("E OPEN CURTAIN");
    if (FixtureKind == EHouseReactiveFixtureKind::Chandelier) return TEXT("E NUDGE LIGHT");
    return TEXT("THROW TO BREAK GLASS");
}

bool AHouseReactiveFixture::Interact(APawn* Player)
{
    if (!HasAuthority() || BreakState.bBroken || !CanReach(Player)) return false;
    if (FixtureKind == EHouseReactiveFixtureKind::Curtain)
    {
        bCurtainOpen = !bCurtainOpen;
        OnRep_CurtainOpen();
        ForceNetUpdate();
        return true;
    }
    if (FixtureKind == EHouseReactiveFixtureKind::Chandelier)
    {
        ApplyImpact(Body->Bounds.Origin, Player->GetBaseAimRotation().Vector(), 12.f,
            Player->GetController());
        return true;
    }
    return false;
}

void AHouseReactiveFixture::OnRep_CurtainOpen()
{
    if (FixtureKind == EHouseReactiveFixtureKind::Curtain) SetActorTickEnabled(true);
}

void AHouseReactiveFixture::UpdateCurtainPose()
{
    FVector Scale = FVector::OneVector;
    Scale[CurtainAxis == 1 ? 1 : 0] = FMath::Lerp(1.f, FMath::Clamp(CurtainGatheredScale, .08f, .95f), CurtainProgress);
    MotionPivot->SetRelativeScale3D(Scale);
    // Only the separated cloth moves; its authored upper anchor stays fixed.
    MotionPivot->SetRelativeRotation(CurtainAxis == 1
        ? FRotator(CurtainSway.X, 0.f, 0.f) : FRotator(0.f, 0.f, CurtainSway.X));
}

void AHouseReactiveFixture::OnRep_Swing()
{
    if (FixtureKind != EHouseReactiveFixtureKind::Chandelier) return;
    if (!HasAuthority())
    {
        SwingAngles = ReplicatedSwing.Angles;
        SwingVelocity = ReplicatedSwing.Velocity;
        if (const AGameStateBase* GameState = GetWorld()->GetGameState())
        {
            const float Age = FMath::Clamp(float(GameState->GetServerWorldTimeSeconds())
                - ReplicatedSwing.ServerTime, 0.f, .5f);
            AdvanceDampedSpring(SwingAngles, SwingVelocity, FVector2D::ZeroVector,
                SwingStiffness, SwingDamping, Age);
        }
        // Never snap the displayed light to an arriving network snapshot.
        // The next frames reconcile its existing motion with this prediction.
    }
    SetActorTickEnabled(true);
}

void AHouseReactiveFixture::PublishSwingState()
{
    if (!HasAuthority()) return;
    ReplicatedSwing.Angles = SwingAngles;
    ReplicatedSwing.Velocity = SwingVelocity;
    ReplicatedSwing.ServerTime = GetWorld()->GetTimeSeconds();
}

void AHouseReactiveFixture::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    bool bNeedsTick = false;
    if (FixtureKind == EHouseReactiveFixtureKind::Curtain)
    {
        const float Target = bCurtainOpen ? 1.f : 0.f;
        AdvanceCurtainGather(CurtainProgress, CurtainVelocity, Target,
            10.f / FMath::Max(.3f, CurtainTravelSeconds), DeltaSeconds);
        CurtainProgress = FMath::Clamp(CurtainProgress, 0.f, 1.f);
        if (FMath::Abs(CurtainProgress - Target) < .0005f && FMath::Abs(CurtainVelocity) < .004f)
        {
            CurtainProgress = Target;
            CurtainVelocity = 0.f;
        }
        const FVector2D SwayTarget(FMath::Clamp(CurtainVelocity * .42f, -1.4f, 1.4f), 0.f);
        AdvanceDampedSpring(CurtainSway, CurtainSwayVelocity, SwayTarget, 36.f, 5.f, DeltaSeconds);
        bNeedsTick = CurtainProgress != Target || FMath::Abs(CurtainVelocity) > .001f
            || CurtainSway.SizeSquared() > .0001f || CurtainSwayVelocity.SizeSquared() > .0004f;
        if (!bNeedsTick) CurtainSway = CurtainSwayVelocity = FVector2D::ZeroVector;
        UpdateCurtainPose();
    }
    if (FixtureKind == EHouseReactiveFixtureKind::Chandelier)
    {
        AdvanceDampedSpring(SwingAngles, SwingVelocity, FVector2D::ZeroVector,
            SwingStiffness, SwingDamping, DeltaSeconds);
        if (HasAuthority())
        {
            RenderedSwingAngles = SwingAngles;
            RenderedSwingVelocity = SwingVelocity;
        }
        else
        {
            AdvanceDampedSpring(RenderedSwingAngles, RenderedSwingVelocity, FVector2D::ZeroVector,
                SwingStiffness, SwingDamping, DeltaSeconds);
            const float Correction = 1.f - FMath::Exp(-12.f * DeltaSeconds);
            RenderedSwingAngles = FMath::Lerp(RenderedSwingAngles, SwingAngles, Correction);
            RenderedSwingVelocity = FMath::Lerp(RenderedSwingVelocity, SwingVelocity, Correction);
        }
        bNeedsTick |= SwingAngles.SizeSquared() > .0004f || SwingVelocity.SizeSquared() > .0004f
            || RenderedSwingAngles.SizeSquared() > .0004f || RenderedSwingVelocity.SizeSquared() > .0004f;
        if (!bNeedsTick)
        {
            SwingAngles = SwingVelocity = RenderedSwingAngles = RenderedSwingVelocity = FVector2D::ZeroVector;
            if (HasAuthority()) ForceNetUpdate();
        }
        PublishSwingState();
        MotionPivot->SetRelativeRotation(FRotator(RenderedSwingAngles.X, 0.f, RenderedSwingAngles.Y));
    }
    if (Shards.Num() && ShardAge < 7.f)
    {
        UpdateShards(DeltaSeconds);
        bNeedsTick |= ShardAge < 7.f;
    }
    if (!bNeedsTick) SetActorTickEnabled(false);
}

float AHouseReactiveFixture::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f || BreakState.bBroken) return 0.f;
    FHitResult Hit;
    FVector Direction;
    DamageEvent.GetBestHitInfo(this, DamageCauser, Hit, Direction);
    if (Direction.IsNearlyZero()) Direction = GetActorForwardVector();
    // Repeated weapon strikes first craze the pane, then shatter it. A heavy
    // thrown physics body can still deliver a stronger direct ApplyImpact.
    const float GlassDamage = FixtureKind == EHouseReactiveFixtureKind::Window
        && DamageEvent.IsOfType(FPointDamageEvent::ClassID) ? FMath::Min(DamageAmount, 8.f) : DamageAmount;
    ApplyImpact(Hit.bBlockingHit ? Hit.ImpactPoint : Body->Bounds.Origin, Direction,
        GlassDamage, EventInstigator);
    return GlassDamage;
}

void AHouseReactiveFixture::ApplyImpact(const FVector& WorldPoint, const FVector& WorldDirection,
    float Strength, AController* Instigator)
{
    if (!HasAuthority() || BreakState.bBroken || !FMath::IsFinite(Strength) || Strength <= 0.f) return;
    ++ImpactCount;
    if (FixtureKind == EHouseReactiveFixtureKind::Window)
    {
        if (BreakState.AccumulatedDamage <= 0.f)
            BreakState.FirstImpactLocal = GetActorTransform().InverseTransformPosition(WorldPoint);
        BreakState.ImpactLocal = GetActorTransform().InverseTransformPosition(WorldPoint);
        BreakState.AccumulatedDamage += Strength;
        if (BreakState.AccumulatedDamage >= FMath::Max(1.f, GlassBreakingDamage))
            BreakGlass(WorldPoint, WorldDirection, Instigator);
        else OnRep_Broken();
    }
    else if (FixtureKind == EHouseReactiveFixtureKind::Chandelier)
    {
        FVector Local = GetActorTransform().InverseTransformVectorNoScale(WorldDirection).GetSafeNormal();
        if (Local.SizeSquared2D() < .04f) Local = FVector(.7f, .3f, 0.f);
        const float Push = FMath::Clamp(Strength * .65f, 8.f, 45.f);
        SwingVelocity += FVector2D(Local.X, -Local.Y) * Push;
        // Limit injected energy, rather than hitting an angular stop and
        // reversing abruptly at maximum swing. Damping then settles naturally.
        const float Limit = FMath::Clamp(MaximumSwingDegrees, 3.f, 22.f);
        const float AvailableEnergy = SwingStiffness * FMath::Max(0.f,
            Limit * Limit - float(SwingAngles.SizeSquared()));
        SwingVelocity = SwingVelocity.GetClampedToMaxSize(FMath::Sqrt(AvailableEnergy));
        PublishSwingState();
        OnRep_Swing();
    }
    else
    {
        // Cloth yields away from a thrown object and can then be drawn closed with E.
        bCurtainOpen = true;
        CurtainSwayVelocity.X += FMath::Clamp(Strength * .025f, .2f, 1.4f);
        OnRep_CurtainOpen();
    }
    ForceNetUpdate();
}

void AHouseReactiveFixture::OnFixtureHit(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!HasAuthority() || !OtherActor || !OtherComp || Cast<APawn>(OtherActor)
        || !OtherComp->IsSimulatingPhysics() || BreakState.bBroken) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastPhysicsImpactTime < .22f) return;
    const FVector Velocity = OtherComp->GetPhysicsLinearVelocity();
    const float Speed = Velocity.Size();
    if (Speed < 180.f && NormalImpulse.Size() < 4000.f) return;
    LastPhysicsImpactTime = Now;
    ApplyImpact(Hit.ImpactPoint, Velocity.IsNearlyZero() ? -Hit.ImpactNormal : Velocity.GetSafeNormal(),
        FMath::Clamp(Speed * .045f + NormalImpulse.Size() * .001f, 8.f, 70.f),
        OtherActor->GetInstigatorController());
}

void AHouseReactiveFixture::BreakGlass(const FVector& Point, const FVector& Direction, AController* Instigator)
{
    if (BreakState.bBroken) return;
    BreakState.bBroken = true;
    BreakState.ImpactLocal = GetActorTransform().InverseTransformPosition(Point);
    BreakState.DirectionLocal = GetActorTransform().InverseTransformVectorNoScale(Direction).GetSafeNormal();
    if (UHouseDamageSubsystem* Damage = GetWorld()->GetSubsystem<UHouseDamageSubsystem>())
        Damage->RegisterBrokenActor(this, TEXT("Window"), Instigator);
    OnRep_Broken();
    ForceNetUpdate();
}

void AHouseReactiveFixture::OnRep_Broken()
{
    UpdateGlassCracks();
    if (FixtureKind == EHouseReactiveFixtureKind::Window && HasActorBegunPlay())
    {
        const FVector At = GetActorTransform().TransformPosition(BreakState.ImpactLocal);
        if (BreakState.bBroken && !bGlassShatterSoundPlayed)
        {
            HouseSfx::Break(GetWorld(), At, EHouseSurface::Glass, 4.f, false);
            bGlassShatterSoundPlayed = true;
            ++GlassShatterSoundCount;
        }
        else if (!BreakState.bBroken && BreakState.AccumulatedDamage > LastAudibleGlassDamage + .01f)
        {
            HouseSfx::Break(GetWorld(), At, EHouseSurface::Glass, 2.f, true);
            ++GlassCrackSoundCount;
        }
        LastAudibleGlassDamage = FMath::Max(LastAudibleGlassDamage, BreakState.AccumulatedDamage);
    }
    if (!BreakState.bBroken) return;
    Body->SetVisibility(false);
    BreakawayFrame->SetVisibility(false);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HitVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (GetNetMode() != NM_DedicatedServer && !bShardsStarted) StartShards();
    if (!bBreakNotified)
    {
        bBreakNotified = true;
        OnFixtureBroken.Broadcast(this);
    }
}

void AHouseReactiveFixture::StartShards()
{
    if (!OriginalMesh) return;
    bShardsStarted = true;
    ShardAge = 0.f;
    // A broken porch leaf can still swing. Settled shards belong to the floor,
    // rather than moving with the door's attachment hierarchy.
    GlassShards->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    GlassEdgeGlints->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    GlassShards->ClearInstances();
    GlassEdgeGlints->ClearInstances();
    Shards.Reset();
    if (UStaticMesh* TriangularShard = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/SeniorSendOff/House/Interactivity/HI_GlassShard.HI_GlassShard")))
        GlassShards->SetStaticMesh(TriangularShard);
    FRandomStream Random(GetTypeHash(FixtureId.IsNone() ? GetFName() : FixtureId));
    const FBox Bounds = OriginalMesh->GetBoundingBox();
    const FVector Extent = Bounds.GetExtent();
    const FTransform Transform = Body->GetComponentTransform();
    int32 NormalAxis = 0;
    if (Extent.Y < Extent[NormalAxis]) NormalAxis = 1;
    if (Extent.Z < Extent[NormalAxis]) NormalAxis = 2;
    const int32 AxisA = (NormalAxis + 1) % 3, AxisB = (NormalAxis + 2) % 3;
    FVector LocalNormal = FVector::ZeroVector;
    LocalNormal[NormalAxis] = 1.f;
    const FVector PaneNormal = Transform.TransformVectorNoScale(LocalNormal).GetSafeNormal();
    const FVector HitDirection = GetActorTransform().TransformVectorNoScale(BreakState.DirectionLocal).GetSafeNormal();
    const FVector Outward = PaneNormal * (FVector::DotProduct(PaneNormal, HitDirection) >= 0.f ? 1.f : -1.f);
    const FVector Impact = GetActorTransform().TransformPosition(BreakState.ImpactLocal);
    const FVector ShardMeshSize = GlassShards->GetStaticMesh()->GetBoundingBox().GetSize().ComponentMax(FVector(.01f));
    // Stratified positions make the entire pane disintegrate; a random point
    // in its 3-D bounds made fragments appear inside walls and in empty air.
    constexpr int32 Columns = 7, Rows = 8;
    for (int32 Row = 0; Row < Rows; ++Row)
    for (int32 Column = 0; Column < Columns; ++Column)
    {
        FShard& Shard = Shards.AddDefaulted_GetRef();
        FVector Point = Bounds.GetCenter();
        Point[AxisA] += ((Column + Random.FRandRange(.16f, .84f)) / Columns * 2.f - 1.f) * Extent[AxisA];
        Point[AxisB] += ((Row + Random.FRandRange(.16f, .84f)) / Rows * 2.f - 1.f) * Extent[AxisB];
        Shard.Position = Transform.TransformPosition(Point);
        const FVector Radial = (Shard.Position - Impact).GetSafeNormal();
        const float DistanceFraction = FMath::Clamp(FVector::Dist(Shard.Position, Impact)
            / FMath::Max(1.f, Extent.GetMax() * Transform.GetScale3D().GetAbsMax() * 2.f), 0.f, 1.f);
        Shard.ReleaseTime = Random.FRandRange(0.f, .025f) + DistanceFraction * .12f;
        Shard.Velocity = Outward * Random.FRandRange(100.f, 300.f)
            + Radial * Random.FRandRange(55.f, 190.f)
            + FVector(Random.FRandRange(-38.f, 38.f), Random.FRandRange(-38.f, 38.f), Random.FRandRange(10.f, 125.f));
        const float CellA = 2.f * Extent[AxisA] * FMath::Abs(Transform.GetScale3D()[AxisA]) / Columns;
        const float CellB = 2.f * Extent[AxisB] * FMath::Abs(Transform.GetScale3D()[AxisB]) / Rows;
        Shard.Scale = FVector(FMath::Clamp(CellA * Random.FRandRange(.43f, .88f), 2.5f, 17.f),
            FMath::Clamp(CellB * Random.FRandRange(.42f, .9f), 2.5f, 17.f), .22f) / ShardMeshSize;
        Shard.Rotation = FRotator(Random.FRandRange(-80.f,80.f),Random.FRandRange(0.f,360.f),Random.FRandRange(-80.f,80.f));
        Shard.Spin = FRotator(Random.FRandRange(-360.f,360.f), Random.FRandRange(-500.f,500.f), Random.FRandRange(-360.f,360.f));
        GlassShards->AddInstance(FTransform(Shard.Rotation, Shard.Position, Shard.Scale), true);
    }
    SetActorTickEnabled(true);
}

void AHouseReactiveFixture::UpdateShards(float DeltaSeconds)
{
    const float PreviousAge = ShardAge;
    ShardAge += FMath::Max(0.f, DeltaSeconds);
    const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / .025f), 1, 12);
    const float Step = DeltaSeconds / Steps;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseWindowShards), false, this);
    for (int32 Substep = 0; Substep < Steps; ++Substep)
    for (int32 Index = 0; Index < Shards.Num(); ++Index)
    {
        FShard& Shard = Shards[Index];
        const float StartAge = PreviousAge + Substep * Step;
        const float LiveStep = FMath::Max(0.f, StartAge + Step - FMath::Max(StartAge, Shard.ReleaseTime));
        if (Shard.bResting || LiveStep <= 0.f) continue;
        Shard.Velocity.Z -= 980.f * LiveStep;
        FVector Next = Shard.Position + Shard.Velocity * LiveStep;
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Shard.Position, Next, ECC_Visibility, Query))
        {
            // The authored tile/wood surfaces sit slightly above some simple
            // collision hulls. Keep resting glass clear of the visible floor.
            Next = Hit.ImpactPoint + Hit.ImpactNormal * (Hit.ImpactNormal.Z > .6f ? 2.2f : .35f);
            if (Hit.ImpactNormal.Z > .6f)
            {
                if (Shard.FloorBounces < 2 && FMath::Abs(Shard.Velocity.Z) > 85.f)
                {
                    ++Shard.FloorBounces;
                    Shard.Velocity = FVector(Shard.Velocity.X * .48f, Shard.Velocity.Y * .48f,
                        FMath::Abs(Shard.Velocity.Z) * (Shard.FloorBounces == 1 ? .22f : .11f));
                }
                else
                {
                    Shard.bResting = true;
                    Shard.Velocity = FVector::ZeroVector;
                    Shard.Rotation.Pitch = FMath::Clamp(Shard.Rotation.Pitch, -7.f, 7.f);
                    Shard.Rotation.Roll = FMath::Clamp(Shard.Rotation.Roll, -7.f, 7.f);
                }
            }
            else Shard.Velocity = Shard.Velocity.MirrorByVector(Hit.ImpactNormal) * .24f;
        }
        Shard.Position = Next;
        if (!Shard.bResting) Shard.Rotation += Shard.Spin * LiveStep;
    }
    if (ShardAge >= 6.5f)
    {
        for (FShard& Shard : Shards)
        {
            if (Shard.bResting) continue;
            FHitResult Floor;
            if (GetWorld()->LineTraceSingleByChannel(Floor, Shard.Position + FVector(0, 0, 20.f),
                Shard.Position - FVector(0, 0, 1500.f), ECC_Visibility, Query) && Floor.ImpactNormal.Z > .6f)
            {
                Shard.Position = Floor.ImpactPoint + FVector(0, 0, 2.2f);
                Shard.Rotation.Pitch = Shard.Rotation.Roll = 0.f;
            }
            else Shard.Scale = FVector::ZeroVector; // never freeze visible debris in midair
            Shard.bResting = true;
        }
    }
    for (int32 Index = 0; Index < Shards.Num(); ++Index)
    {
        FShard& Shard = Shards[Index];
        GlassShards->UpdateInstanceTransform(Index, FTransform(Shard.Rotation, Shard.Position, Shard.Scale), true, false, true);
        if (Shard.bResting && !Shard.bGlintPlaced)
        {
            Shard.bGlintPlaced = true;
            // Only some pieces catch a sharp highlight. This gives a readable
            // scatter on both light tile and dark wood without opaque decals.
            if (Index % 2 == 0 && !Shard.Scale.IsNearlyZero())
            {
                const float Width = Shard.Scale.X * GlassShards->GetStaticMesh()->GetBoundingBox().GetSize().X;
                const FVector Edge = Shard.Position + Shard.Rotation.RotateVector(FVector(0.f, -6.f * Shard.Scale.Y, .28f));
                GlassEdgeGlints->AddInstance(FTransform(FRotator(0.f, Shard.Rotation.Yaw, 0.f), Edge,
                    FVector(FMath::Clamp(Width * .55f, 2.2f, 8.f) / 100.f, .008f, .0025f)), true);
            }
        }
    }
    GlassShards->MarkRenderStateDirty();
}

int32 AHouseReactiveFixture::GetShardCount() const
{
    return GlassShards ? GlassShards->GetInstanceCount() : 0;
}

int32 AHouseReactiveFixture::GetRestingShardCount() const
{
    int32 Count = 0;
    for (const FShard& Shard : Shards)
        if (Shard.bResting && !Shard.Scale.IsNearlyZero()) ++Count;
    return Count;
}

int32 AHouseReactiveFixture::GetSupportedShardCount() const
{
    if (!GetWorld()) return 0;
    int32 Count = 0;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseWindowShardSupport), false, this);
    for (const FShard& Shard : Shards)
    {
        if (!Shard.bResting || Shard.Scale.IsNearlyZero()) continue;
        FHitResult Support;
        if (GetWorld()->LineTraceSingleByChannel(Support, Shard.Position + FVector(0, 0, 2.f),
            Shard.Position - FVector(0, 0, 5.f), ECC_Visibility, Query) && Support.ImpactNormal.Z > .6f
            && Shard.Position.Z > Support.ImpactPoint.Z)
            ++Count;
    }
    return Count;
}

int32 AHouseReactiveFixture::GetCrackCount() const
{
    return GlassCracks ? GlassCracks->GetInstanceCount() : 0;
}

void AHouseReactiveFixture::UpdateGlassCracks()
{
    GlassCracks->ClearInstances();
    if (FixtureKind != EHouseReactiveFixtureKind::Window || !OriginalMesh
        || BreakState.bBroken || BreakState.AccumulatedDamage <= 0.f) return;
    const FBox Bounds = OriginalMesh->GetBoundingBox();
    const FVector Extent = Bounds.GetExtent();
    int32 NormalAxis = 0;
    if (Extent.Y < Extent[NormalAxis]) NormalAxis = 1;
    if (Extent.Z < Extent[NormalAxis]) NormalAxis = 2;
    const int32 AxisA = (NormalAxis + 1) % 3, AxisB = (NormalAxis + 2) % 3;
    const FTransform Transform = Body->GetComponentTransform();
    FVector Center = Transform.InverseTransformPosition(GetActorTransform().TransformPosition(BreakState.FirstImpactLocal));
    Center = Bounds.GetClosestPointTo(Center);
    Center[NormalAxis] = Bounds.GetCenter()[NormalAxis];
    const float Fraction = FMath::Clamp(BreakState.AccumulatedDamage/FMath::Max(1.f,GlassBreakingDamage),0.f,1.f);
    const int32 Rays = Fraction > .5f ? 8 : 5;
    FRandomStream Random(GetTypeHash(FixtureId.IsNone() ? GetFName() : FixtureId) + 173);
    FVector NormalLocal = FVector::ZeroVector;
    NormalLocal[NormalAxis] = 1.f;
    const FVector NormalWorld = Transform.TransformVectorNoScale(NormalLocal).GetSafeNormal();
    const auto AddLine = [&](const FVector& A, const FVector& B)
    {
        const FVector Delta = B-A;
        if (Delta.SizeSquared() < .1f) return;
        // Thin pale hairlines catch the light without obscuring the glass.
        GlassCracks->AddInstance(FTransform(Delta.Rotation(),(A+B)*.5f,
            FVector(Delta.Size()/100.f,.0018f,.0012f)),true);
    };
    for (int32 Ray=0; Ray<Rays; ++Ray)
    {
        const float Angle = (2.f*PI*Ray/Rays) + Random.FRandRange(-.11f,.11f);
        FVector Previous = Center;
        for (int32 Segment=1; Segment<=3; ++Segment)
        {
            const float Radius = Segment/3.f * (.52f + Fraction*.28f);
            FVector Point = Center;
            Point[AxisA] += FMath::Cos(Angle + Random.FRandRange(-.09f,.09f)) * Extent[AxisA] * Radius;
            Point[AxisB] += FMath::Sin(Angle + Random.FRandRange(-.09f,.09f)) * Extent[AxisB] * Radius;
            Point = Bounds.GetClosestPointTo(Point);
            for (float Side : {-1.f,1.f})
            {
                FVector A = Previous, B = Point;
                A[NormalAxis] = B[NormalAxis] = Bounds.GetCenter()[NormalAxis] + Extent[NormalAxis]*Side;
                AddLine(Transform.TransformPosition(A)+NormalWorld*Side*.14f,
                    Transform.TransformPosition(B)+NormalWorld*Side*.14f);
            }
            if (Fraction > .5f && Segment == 2)
            {
                FVector Branch = Point;
                Branch[AxisA] += FMath::Cos(Angle+.6f)*Extent[AxisA]*.17f;
                Branch[AxisB] += FMath::Sin(Angle+.6f)*Extent[AxisB]*.17f;
                Branch = Bounds.GetClosestPointTo(Branch);
                for (float Side : {-1.f,1.f})
                {
                    FVector A = Point, B = Branch;
                    A[NormalAxis] = B[NormalAxis] = Bounds.GetCenter()[NormalAxis] + Extent[NormalAxis]*Side;
                    AddLine(Transform.TransformPosition(A)+NormalWorld*Side*.14f,
                        Transform.TransformPosition(B)+NormalWorld*Side*.14f);
                }
            }
            Previous = Point;
        }
    }
}
