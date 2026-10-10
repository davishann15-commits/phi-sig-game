#include "HouseOxbloodMask.h"
#include "HouseInteractiveProp.h"
#include "StoryCampaign.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Math/RotationMatrix.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float MinimumSupportUp = .70710678f; // Release beyond 45 degrees.
constexpr float StartupGraceSeconds = 2.f;
}

AHouseOxbloodMaskPickup::AHouseOxbloodMaskPickup()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(30.f);
    SetMinNetUpdateFrequency(10.f);
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SceneRoot->SetMobility(EComponentMobility::Movable);
    SetRootComponent(SceneRoot);
    DisplayMask = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OxbloodMask"));
    DisplayMask->SetupAttachment(SceneRoot);
    DisplayMask->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DisplayMask->SetGenerateOverlapEvents(false);
    DisplayMask->SetRelativeRotation(FRotator(0, 0, -90));
    DisplayMask->SetRelativeScale3D(FVector(.62f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(
        TEXT("/Game/Items/HouseMask/SM_HouseOxbloodMaskResting.SM_HouseOxbloodMaskResting"));
    DisplayMask->SetStaticMesh(Mesh.Object);
}

void AHouseOxbloodMaskPickup::BeginPlay()
{
    Super::BeginPlay();
    if (DisplayMask && DisplayMask->GetStaticMesh())
    {
        // The source mesh includes a relaxed strap. Centre its full footprint
        // and leave the lowest visible point 1.5 mm above the authored surface.
        const FBox Resting = DisplayMask->GetStaticMesh()->GetBoundingBox().TransformBy(
            FTransform(DisplayMask->GetRelativeRotation(), FVector::ZeroVector, DisplayMask->GetRelativeScale3D()));
        DisplayMask->SetRelativeLocation(FVector(-Resting.GetCenter().X,
            -Resting.GetCenter().Y, -Resting.Min.Z + .15f));
    }
    if (!HasAuthority())
    {
        // Clients render the server's replicated transform, never their own
        // furniture simulation or local floor trace.
        SetActorTickEnabled(false);
        return;
    }
    StartupResolveUntil = GetWorld()->GetTimeSeconds() + StartupGraceSeconds;
    FHitResult Hit;
    if (FindSupportingSurface(GetActorLocation() + FVector(0,0,8), GetActorLocation() - FVector(0,0,12), Hit))
        SetSupport(Hit, true);
}

bool AHouseOxbloodMaskPickup::SupportIsUsable(UPrimitiveComponent* Component) const
{
    if (!IsValid(Component) || !Component->IsRegistered())
        return false;
    const AActor* Owner = Component->GetOwner();
    if (!IsValid(Owner) || Owner->IsActorBeingDestroyed() || !Owner->GetActorEnableCollision())
        return false;
    if (Owner == this || Owner->IsA<APawn>() || Owner->IsA<AHouseOxbloodMaskPickup>()) return false;
    // Floors/walls and several static tabletops deliberately use hidden COLF_,
    // COLL_, COLW_ and *_COL_* bodies. Their enabled collision remains support.
    // Interactive furniture is the visible body itself, so hiding it removes it.
    if (Owner->IsA<AHouseInteractiveProp>() &&
        (!Component->IsVisible() || Component->bHiddenInGame || Owner->IsHidden())) return false;
    const ECollisionEnabled::Type Collision = Component->GetCollisionEnabled();
    return (Collision == ECollisionEnabled::QueryOnly || Collision == ECollisionEnabled::QueryAndPhysics)
        && Component->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block;
}

bool AHouseOxbloodMaskPickup::FindSupportingSurface(const FVector& Start, const FVector& End, FHitResult& Hit) const
{
    if (!GetWorld()) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseMaskSupport), false, this);
    for (TActorIterator<APawn> It(GetWorld()); It; ++It) Query.AddIgnoredActor(*It);
    if (GetWorld()->GetTimeSeconds() < IgnoreReleasedUntil && RecentlyReleasedSupport.IsValid())
        Query.AddIgnoredComponent(RecentlyReleasedSupport.Get());
    // Skip disabled or steep collision surfaces while searching for a usable
    // tabletop or floor. Hidden static collision proxies remain valid supports.
    for (int32 Attempt = 0; Attempt < 12; ++Attempt)
    {
        FHitResult Candidate;
        if (!GetWorld()->LineTraceSingleByChannel(Candidate, Start, End, ECC_Visibility, Query)) return false;
        UPrimitiveComponent* Component = Candidate.GetComponent();
        if (SupportIsUsable(Component) && Candidate.ImpactNormal.Z >= MinimumSupportUp)
        { Hit = Candidate; return true; }
        if (!Component) return false;
        Query.AddIgnoredComponent(Component);
    }
    return false;
}

bool AHouseOxbloodMaskPickup::CurrentSupportIsUsable() const
{
    UPrimitiveComponent* Component = SupportComponent.Get();
    if (!SupportIsUsable(Component)) return false;
    // Initially hidden collision proxies are valid; a visible support becoming
    // hidden during play is a removal and must release its carried mask.
    if ((bSupportWasVisible && !Component->IsVisible())
        || (!bSupportWasComponentHidden && Component->bHiddenInGame)
        || (!bSupportWasActorHidden && Component->GetOwner()->IsHidden())) return false;
    if (Component->GetComponentTransform().TransformVectorNoScale(SupportLocalNormal).GetSafeNormal().Z < MinimumSupportUp)
        return false;
    if (const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Component))
        if (Static->GetStaticMesh() != SupportMesh.Get()) return false;
    // BreakProp keeps Body enabled and swaps the visible mesh (or crushes its
    // scale). Observe that state transition even when collision stays enabled.
    if (const AHouseInteractiveProp* Prop = Cast<AHouseInteractiveProp>(Component->GetOwner()))
        if (Prop->bBroken != bSupportWasBroken) return false;
    return true;
}

FVector AHouseOxbloodMaskPickup::VisibleSurfacePoint(const FHitResult& Hit) const
{
    UPrimitiveComponent* Component = Hit.GetComponent();
    if (Component)
    {
        FCollisionQueryParams VisualQuery(SCENE_QUERY_STAT(HouseMaskVisualSurface), true, this);
        FHitResult Visual;
        // Prefer triangle contact where the asset has complex collision. The
        // broad furniture proxy may sit 1-2 cm above its visible tabletop.
        if (Component->LineTraceComponent(Visual, Hit.ImpactPoint + FVector(0,0,6),
            Hit.ImpactPoint - FVector(0,0,6), VisualQuery) && Visual.ImpactNormal.Z >= MinimumSupportUp)
            return Visual.ImpactPoint;
    }
    return Hit.ImpactPoint;
}

void AHouseOxbloodMaskPickup::SetSupport(const FHitResult& Hit, bool bPreserveAuthoredPosition)
{
    UPrimitiveComponent* Component = Hit.GetComponent();
    if (!SupportIsUsable(Component)) return;
    if (!bPreserveAuthoredPosition)
    {
        // Rest flat on a reasonably level surface after a fall. The original
        // table placement keeps its exact authored Z rather than proxy height.
        SetActorLocationAndRotation(VisibleSurfacePoint(Hit),
            FRotationMatrix::MakeFromZX(Hit.ImpactNormal, GetActorForwardVector()).Rotator(),
            false, nullptr, ETeleportType::TeleportPhysics);
    }
    SupportComponent = Component;
    const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Component);
    SupportMesh = Static ? Static->GetStaticMesh() : nullptr;
    const AHouseInteractiveProp* Prop = Cast<AHouseInteractiveProp>(Component->GetOwner());
    bSupportWasBroken = Prop && Prop->bBroken;
    bSupportWasVisible = Component->IsVisible();
    bSupportWasComponentHidden = Component->bHiddenInGame;
    bSupportWasActorHidden = Component->GetOwner()->IsHidden();
    SupportRelativeTransform = GetActorTransform().GetRelativeTransform(Component->GetComponentTransform());
    SupportLocalNormal = Component->GetComponentTransform().InverseTransformVectorNoScale(Hit.ImpactNormal).GetSafeNormal();
    FallVelocity = FVector::ZeroVector;
    LastSupportVelocity = Component->GetComponentVelocity().GetClampedToMaxSize(700.f);
    FlushNetDormancy(); ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("HOUSE_MASK_SUPPORT mask=%s support=%s component=%s authored=%d location=%s"),
        *GetName(), *GetNameSafe(Component->GetOwner()), *Component->GetName(), bPreserveAuthoredPosition ? 1 : 0,
        *GetActorLocation().ToCompactString());
}

void AHouseOxbloodMaskPickup::ReleaseSupport()
{
    RecentlyReleasedSupport = SupportComponent;
    IgnoreReleasedUntil = GetWorld()->GetTimeSeconds() + .2f;
    FallVelocity = LastSupportVelocity.GetClampedToMaxSize(700.f);
    SupportComponent.Reset(); SupportMesh.Reset();
    SetActorRotation(FRotator(0, GetActorRotation().Yaw, 0), ETeleportType::TeleportPhysics);
    // Keep the short initial grace when source furniture is being replaced by
    // its interactive version. After startup this deadline is already past.
    FlushNetDormancy(); ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("HOUSE_MASK_RELEASE mask=%s location=%s velocity=%s"),
        *GetName(), *GetActorLocation().ToCompactString(), *FallVelocity.ToCompactString());
}

void AHouseOxbloodMaskPickup::FallToSupport(float DeltaSeconds)
{
    // Small server-only substeps keep a fast fall from skipping thin tables.
    float Remaining = FMath::Min(FMath::Max(DeltaSeconds, 0.f), .25f);
    while (Remaining > SMALL_NUMBER && !SupportComponent.IsValid())
    {
        const float Step = FMath::Min(Remaining, 1.f/60.f); Remaining -= Step;
        FallVelocity.Z = FMath::Max(FallVelocity.Z + GetWorld()->GetGravityZ() * Step, -1200.f);
        FallVelocity.X *= FMath::Max(0.f, 1.f - .35f * Step);
        FallVelocity.Y *= FMath::Max(0.f, 1.f - .35f * Step);
        const FVector Before = GetActorLocation();
        const FVector After = Before + FallVelocity * Step;
        FHitResult Hit;
        // Only land while descending. The short headroom handles a mask that
        // starts just below an approximate furniture collision proxy.
        if (FallVelocity.Z <= 0 && FindSupportingSurface(Before + FVector(0,0,3), After - FVector(0,0,.25f), Hit))
        { SetSupport(Hit, false); return; }
        SetActorLocation(After, false, nullptr, ETeleportType::TeleportPhysics);
    }
}

void AHouseOxbloodMaskPickup::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || bConsumed || !GetWorld()) return;
    if (SupportComponent.IsValid())
    {
        if (CurrentSupportIsUsable())
        {
            UPrimitiveComponent* Component = SupportComponent.Get();
            FTransform Desired = SupportRelativeTransform * Component->GetComponentTransform();
            Desired.SetScale3D(GetActorScale3D());
            LastSupportVelocity = Component->GetComponentVelocity().GetClampedToMaxSize(700.f);
            if (!GetActorTransform().Equals(Desired, .001f))
                SetActorTransform(Desired, false, nullptr, ETeleportType::TeleportPhysics);
            return;
        }
        ReleaseSupport();
    }
    else if (!SupportComponent.IsExplicitlyNull())
    {
        // A destroyed component becomes a stale weak pointer; preserve its
        // last measured velocity without dereferencing its former owner.
        ReleaseSupport();
    }
    FHitResult Hit;
    if (GetWorld()->GetTimeSeconds() < StartupResolveUntil)
    {
        if (FindSupportingSurface(GetActorLocation() + FVector(0,0,8), GetActorLocation() - FVector(0,0,12), Hit))
            SetSupport(Hit, true);
        return;
    }
    FallToSupport(DeltaSeconds);
}

FVector AHouseOxbloodMaskPickup::InteractionLocation() const
{
    return DisplayMask ? DisplayMask->Bounds.Origin : GetActorLocation() + FVector(0, 0, 15);
}

bool AHouseOxbloodMaskPickup::CanCollect(const AStoryFirstPersonCharacter* Pawn) const
{
    return HasAuthority() && !bConsumed && IsValid(Pawn) && !Pawn->bHasHouseMask &&
        FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) <= 260.f;
}

bool AHouseOxbloodMaskPickup::Collect(AStoryFirstPersonCharacter* Pawn)
{
    if (!CanCollect(Pawn)) return false;
    bConsumed = true;
    Pawn->ReceiveHouseMask();
    Destroy();
    return true;
}
