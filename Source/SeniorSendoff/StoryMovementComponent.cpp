#include "StoryMovementComponent.h"

#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "HouseHotTubWater.h"
#include "HouseFurnitureInteractionComponent.h"
#include "HouseInteractiveProp.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarSSOFurniturePush(TEXT("sso.FurniturePushFix"),1,
    TEXT("1 = weight-aware gentle pushing of physics furniture by the player (default). 0 = engine default push."));

class FSavedMove_StoryMovement final : public FSavedMove_Character
{
public:
    bool bSavedWantsToSprint = false;
    bool bSavedWantsToSlide = false;
    bool bSavedWantsToCrawl = false;
    bool bSavedIsSliding = false;
    bool bSavedSlideConsumedForHold = false;

    virtual void Clear() override
    {
        FSavedMove_Character::Clear();
        bSavedWantsToSprint = false;
        bSavedWantsToSlide = false;
        bSavedWantsToCrawl = false;
        bSavedIsSliding = false;
        bSavedSlideConsumedForHold = false;
    }

    virtual uint8 GetCompressedFlags() const override
    {
        uint8 Flags = FSavedMove_Character::GetCompressedFlags();
        if (bSavedWantsToSprint) Flags |= FLAG_Custom_0;
        if (bSavedWantsToSlide) Flags |= FLAG_Custom_1;
        if (bSavedSlideConsumedForHold) Flags |= FLAG_Custom_2;
        if (bSavedWantsToCrawl) Flags |= FLAG_Custom_3;
        return Flags;
    }

    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override
    {
        const auto* Other = static_cast<const FSavedMove_StoryMovement*>(NewMove.Get());
        return Other && bSavedWantsToSprint == Other->bSavedWantsToSprint
            && bSavedWantsToSlide == Other->bSavedWantsToSlide
            && bSavedWantsToCrawl == Other->bSavedWantsToCrawl
            && bSavedIsSliding == Other->bSavedIsSliding
            && bSavedSlideConsumedForHold == Other->bSavedSlideConsumedForHold
            && FSavedMove_Character::CanCombineWith(NewMove, Character, MaxDelta);
    }

    virtual void SetMoveFor(ACharacter* Character, float DeltaTime, const FVector& NewAccel,
        FNetworkPredictionData_Client_Character& ClientData) override
    {
        FSavedMove_Character::SetMoveFor(Character, DeltaTime, NewAccel, ClientData);
        if (const auto* Movement = Cast<UStoryMovementComponent>(Character->GetCharacterMovement()))
        {
            bSavedWantsToSprint = Movement->bWantsToSprint;
            bSavedWantsToSlide = Movement->bWantsToSlide;
            bSavedWantsToCrawl = Movement->bWantsToCrawl;
            bSavedIsSliding = Movement->bIsSliding;
            bSavedSlideConsumedForHold = Movement->bSlideConsumedForHold;
        }
    }

    virtual void PrepMoveFor(ACharacter* Character) override
    {
        FSavedMove_Character::PrepMoveFor(Character);
        if (auto* Movement = Cast<UStoryMovementComponent>(Character->GetCharacterMovement()))
        {
            Movement->bWantsToSprint = bSavedWantsToSprint;
            Movement->bWantsToSlide = bSavedWantsToSlide;
            Movement->bWantsToCrawl = bSavedWantsToCrawl;
            Movement->bIsSliding = bSavedIsSliding;
            Movement->bSlideConsumedForHold = bSavedSlideConsumedForHold;
        }
    }
};

class FNetworkPredictionData_Client_StoryMovement final : public FNetworkPredictionData_Client_Character
{
public:
    explicit FNetworkPredictionData_Client_StoryMovement(const UCharacterMovementComponent& Movement)
        : FNetworkPredictionData_Client_Character(Movement) {}

    virtual FSavedMovePtr AllocateNewMove() override
    {
        return FSavedMovePtr(new FSavedMove_StoryMovement());
    }
};

UStoryMovementComponent::UStoryMovementComponent()
{
    MaxWalkSpeed = 500.f;
    MaxWalkSpeedCrouched = 260.f;
    MaxAcceleration = 4200.f;
    GroundFriction = 3.5f;
    BrakingDecelerationWalking = 900.f;
    JumpZVelocity = 520.f;
    AirControl = 1.f;
    AirControlBoostMultiplier = 0.f;
    FallingLateralFriction = 0.f;
    BrakingDecelerationFalling = 0.f;
    SetCrouchedHalfHeight(54.f);
    // Pushing is handled in ApplyImpactPhysicsForces (weight-aware, no lift, no spin).
    bPushForceScaledToMass = true;
    bPushForceUsingZOffset = false;
    RepulsionForce = .6f;
    GetNavAgentPropertiesRef().bCanCrouch = true;
    bCanWalkOffLedgesWhenCrouching = true;
}

void UStoryMovementComponent::SetSlideHeld(bool bHeld)
{
    bWantsToSlide = bHeld;
    if (!bHeld) bSlideConsumedForHold = false;
    // The base component predicts and replicates crouch using its own saved flag.
    bWantsToCrouch = bHeld || bWantsToCrawl || bCrawlForced;
}

void UStoryMovementComponent::SetCrawlRequested(bool bRequested)
{
    bWantsToCrawl = bRequested;
    bWantsToCrouch = bWantsToSlide || bWantsToCrawl || bCrawlForced;
    if (HasValidData() && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
        UpdateCharacterStateBeforeMovement(0.f);
}

bool UStoryMovementComponent::SetCrawlForced(bool bForced)
{
    bCrawlForced = bForced;
    bWantsToCrouch = bWantsToSlide || bWantsToCrawl || bCrawlForced;
    if (HasValidData() && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
        UpdateCharacterStateBeforeMovement(0.f);
    return !bForced || IsCrawling();
}

void UStoryMovementComponent::SetStandingCapsuleHalfHeight(float HalfHeight)
{
    StandingHalfHeightOverride = FMath::Max(HalfHeight, 1.f);
}

bool UStoryMovementComponent::IsCrawling() const
{
    const UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
    return Capsule && CharacterOwner->bIsCrouched
        && Capsule->GetUnscaledCapsuleRadius() <= CrawlCapsuleRadius + .5f
        && Capsule->GetUnscaledCapsuleHalfHeight() <= CrawlCapsuleHalfHeight + .5f;
}

void UStoryMovementComponent::SetSimulatedCrawl(bool bReplicatedCrawl)
{
    bSimulatedCrawl = bReplicatedCrawl;
    if (!CharacterOwner || CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy) return;
    SetCrouchedHalfHeight(bSimulatedCrawl ? CrawlCapsuleHalfHeight : 54.f);
    if (CharacterOwner->bIsCrouched) Crouch(true);
}

void UStoryMovementComponent::Crouch(bool bClientSimulation)
{
    Super::Crouch(bClientSimulation);
    // Engine proxy crouch restores the class-default radius first. Finish the
    // replicated prone capsule after its crouch callback has run.
    if (bClientSimulation && bSimulatedCrawl && CharacterOwner
        && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && CharacterOwner->bIsCrouched)
    {
        UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
        Capsule->SetCapsuleSize(CrawlCapsuleRadius, CrawlCapsuleHalfHeight, true);
        const float HeightAdjust = StandingCapsuleHalfHeight() - CrawlCapsuleHalfHeight;
        CharacterOwner->OnStartCrouch(HeightAdjust, HeightAdjust * Capsule->GetShapeScale());
    }
}

void UStoryMovementComponent::UnCrouch(bool bClientSimulation)
{
    if (!HasValidData() || !CharacterOwner->GetCapsuleComponent()) return;
    UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
    const ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
    const float TargetRadius = DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
    const float TargetHalfHeight = FMath::Max(StandingCapsuleHalfHeight(), TargetRadius);
    const float OldHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
    const float ScaledChange = (TargetHalfHeight - OldHalfHeight) * Capsule->GetShapeScale();

    if (bClientSimulation)
    {
        // The server replicates the capsule centre. A simulated proxy only
        // restores the roster member's actual size and mesh offset.
        Capsule->SetCapsuleSize(TargetRadius, TargetHalfHeight, true);
        bShrinkProxyCapsule = true;
        AdjustProxyCapsuleSize();
        CharacterOwner->OnEndCrouch(TargetHalfHeight - OldHalfHeight, ScaledChange);
        return;
    }

    if (!CanFitCapsule(TargetRadius, TargetHalfHeight)) return;
    if (bCrouchMaintainsBaseLocation && !FMath::IsNearlyZero(ScaledChange))
        UpdatedComponent->MoveComponent(-GetGravityDirection() * ScaledChange,
            UpdatedComponent->GetComponentQuat(), false, nullptr,
            EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
    Capsule->SetCapsuleSize(TargetRadius, TargetHalfHeight, true);
    CharacterOwner->SetIsCrouched(false);
    bForceNextFloorCheck = true;
    CharacterOwner->OnEndCrouch(TargetHalfHeight - OldHalfHeight, ScaledChange);

    if (IsNetMode(NM_ListenServer) && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy)
        if (FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character())
        {
            ClientData->MeshTranslationOffset += ScaledChange * -GetGravityDirection();
            ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
        }
}

bool UStoryMovementComponent::IsSprinting() const
{
    const auto* Carry=CharacterOwner?CharacterOwner->FindComponentByClass<UHouseFurnitureInteractionComponent>():nullptr;
    return bWantsToSprint && IsMovingOnGround() && !IsCrouching() && !bIsSliding
        && !(Carry && Carry->BlocksSprint());
}

float UStoryMovementComponent::GetMaxSpeed() const
{
    const auto* Carry=CharacterOwner?CharacterOwner->FindComponentByClass<UHouseFurnitureInteractionComponent>():nullptr;
    const float CarryScale=Carry?Carry->GetMovementScale():1.f;
    if (IsCrawling()) return CrawlSpeed;
    if(IsMovingOnGround())
    {
        // A two-handed carry or drag cannot turn into a sprint or slide launch.
        // A small one-handed piece keeps full speed, including sprint.
        if(Carry && Carry->BlocksSprint())return Super::GetMaxSpeed()*CarryScale;
        if(bIsSliding)return SlideMaxSpeed;
        if(IsSprinting())return SprintSpeed;
    }
    return Super::GetMaxSpeed()*CarryScale;
}

// The still-water volume is static. Sampling it from the replayed character
// position applies the same forces to normal, server and saved-move physics.
// No movement setting or saved state is overwritten by the visual water actor.
float UStoryMovementComponent::GetSpaImmersion() const
{
    if (!HasValidData() || !CharacterOwner->GetCapsuleComponent() || !GetWorld()) return 0.f;
    const FVector Feet = UpdatedComponent->GetComponentLocation() -
        FVector(0, 0, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    for (TActorIterator<AHouseHotTubWater> It(GetWorld()); It; ++It)
    {
        const AHouseHotTubWater* Water = *It;
        if (!Water->bAffectCharacters || !Water->IsPointInsideWater(Feet)) continue;
        const FVector Local = Water->GetActorTransform().InverseTransformPosition(Feet);
        return FMath::Clamp(float((-Local.Z + 9.f) / FMath::Max(25.f, Water->WaterDepth)), 0.f, 1.f);
    }
    return 0.f;
}

float UStoryMovementComponent::GetGravityZ() const
{
    const float Gravity = Super::GetGravityZ();
    return IsFalling() ? Gravity * (1.f - .72f * GetSpaImmersion()) : Gravity;
}

FVector UStoryMovementComponent::NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const
{
    FVector Result = Super::NewFallVelocity(InitialVelocity, Gravity, DeltaTime);
    if (DeltaTime > 0.f)
    {
        const float Immersion = GetSpaImmersion();
        if (Immersion > 0.f)
        {
            const FVector Vertical = GetGravityDirection() * FVector::DotProduct(Result, GetGravityDirection());
            Result += Vertical * (FMath::Exp(-1.5f * Immersion * DeltaTime) - 1.f);
        }
    }
    return Result;
}

void UStoryMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
    CalcDryVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
    if (DeltaTime <= 0.f || !HasValidData() || HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity() ||
        CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy) return;
    const float Immersion = GetSpaImmersion();
    if (Immersion > 0.f)
    {
        const FVector Horizontal = FVector::VectorPlaneProject(Velocity, GetGravityDirection());
        Velocity += Horizontal * (FMath::Exp(-2.2f * Immersion * DeltaTime) - 1.f);
    }
}

void UStoryMovementComponent::CalcDryVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
    if (IsFalling() && HasValidData() && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
    {
        // PhysFalling temporarily removes vertical velocity before calling us.
        // Cap input's component of horizontal speed, rather than clamping the
        // entire velocity vector to walk speed on every airborne frame.
        if (DeltaTime <= 0.f || Acceleration.IsNearlyZero()) return;
        const FVector WishDirection = FVector(Acceleration.X, Acceleration.Y, 0.f).GetSafeNormal();
        if (WishDirection.IsNearlyZero()) return;
        FVector Horizontal(Velocity.X, Velocity.Y, 0.f);
        const float RoomInWishDirection = AirWishSpeed - FVector::DotProduct(Horizontal, WishDirection);
        if (RoomInWishDirection <= 0.f) return;
        const float InputStrength = FMath::Clamp(Acceleration.Size2D() / FMath::Max(GetMaxAcceleration(), 1.f), 0.f, 1.f);
        const float AddedSpeed = FMath::Min(RoomInWishDirection, AirAcceleration * InputStrength * DeltaTime);
        const float AllowedTotalSpeed = FMath::Max(AirSpeedCap, Horizontal.Size());
        Horizontal = (Horizontal + WishDirection * AddedSpeed).GetClampedToMaxSize(AllowedTotalSpeed);
        Velocity.X = Horizontal.X;
        Velocity.Y = Horizontal.Y;
        return;
    }

    if (bIsSliding && IsMovingOnGround() && HasValidData() && !HasAnimRootMotion()
        && !CurrentRootMotion.HasOverrideVelocity())
    {
        TGuardValue<FVector> ReducedAcceleration(Acceleration, Acceleration * SlideSteeringMultiplier);
        // Partial steering must not make the base movement component brake an
        // 8 m/s slide down to its analog walking speed. Keep the slide cap while
        // the actual acceleration remains reduced for directional control.
        TGuardValue<float> FullSlideAnalogSpeed(AnalogInputModifier, 1.f);
        Super::CalcVelocity(DeltaTime, SlideGroundFriction, bFluid, SlideBrakingDeceleration);
        return;
    }
    Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

float UStoryMovementComponent::StandingCapsuleHalfHeight() const
{
    if (StandingHalfHeightOverride > 0.f) return StandingHalfHeightOverride;
    const ACharacter* DefaultCharacter = CharacterOwner
        ? CharacterOwner->GetClass()->GetDefaultObject<ACharacter>() : nullptr;
    return DefaultCharacter && DefaultCharacter->GetCapsuleComponent()
        ? DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 90.f;
}

bool UStoryMovementComponent::CanFitCapsule(float Radius, float HalfHeight) const
{
    if (!HasValidData() || !CharacterOwner->GetCapsuleComponent() || !GetWorld()) return false;
    const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
    const float Scale = Capsule->GetShapeScale();
    FVector Center = UpdatedComponent->GetComponentLocation();
    if (bCrouchMaintainsBaseLocation)
        Center -= GetGravityDirection() * (HalfHeight - Capsule->GetUnscaledCapsuleHalfHeight()) * Scale;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(StoryCrawlClearance), false, CharacterOwner);
    FCollisionResponseParams Response;
    InitCollisionParams(Query, Response);
    return !GetWorld()->OverlapBlockingTestByChannel(Center, UpdatedComponent->GetComponentQuat(),
        UpdatedComponent->GetCollisionObjectType(),
        FCollisionShape::MakeCapsule(Radius * Scale, HalfHeight * Scale), Query, Response);
}

void UStoryMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    if (HasValidData() && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
    {
        UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
        const ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
        const float StandingRadius = DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
        const bool bLowRequested = bWantsToCrawl || bCrawlForced;
        const bool bWasNarrow = Capsule->GetUnscaledCapsuleRadius() < StandingRadius - .5f;
        // Check both width and height before rising. Engine UnCrouch checks the
        // new height with the *current* radius, which is too narrow here.
        const float ExitHalfHeight = bWantsToSlide ? 54.f : StandingCapsuleHalfHeight();
        const bool bExitBlocked = !bLowRequested && bWasNarrow
            && !CanFitCapsule(StandingRadius, ExitHalfHeight);
        const bool bStayLow = bLowRequested || bExitBlocked;
        SetCrouchedHalfHeight(bStayLow ? CrawlCapsuleHalfHeight : 54.f);
        bWantsToCrouch = bStayLow || bWantsToSlide;

        // A shrinking radius cannot cause an encroachment. The standard
        // crouch operation then reduces height while keeping the feet fixed.
        if (bStayLow && Capsule->GetUnscaledCapsuleRadius() > CrawlCapsuleRadius)
            Capsule->SetCapsuleRadius(CrawlCapsuleRadius, true);

        Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
        // Base UpdateCharacterState does not call Crouch again when already
        // crouched, so explicitly apply a lower/higher crouch target.
        if (IsCrouching() && bWantsToCrouch
            && !FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleHalfHeight(), GetCrouchedHalfHeight(), .5f))
            Crouch(false);

        // For crawl -> Ctrl crouch, the height becomes 54 cm before restoring
        // the full 32 cm radius. For crawl -> stand, UnCrouch restores both.
        if (!bStayLow && Capsule->GetUnscaledCapsuleRadius() < StandingRadius - .5f
            && Capsule->GetUnscaledCapsuleHalfHeight() >= ExitHalfHeight - .5f)
            Capsule->SetCapsuleRadius(StandingRadius, true);

        if (IsCrawling())
        {
            bIsSliding = false;
            const FVector Horizontal = Velocity.GetSafeNormal2D() *
                FMath::Min(float(Velocity.Size2D()), CrawlSpeed);
            Velocity = Horizontal + FVector(0, 0, Velocity.Z);
        }
    }
    else Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
    const float Speed = Velocity.Size2D();
    const auto* Carry=CharacterOwner?CharacterOwner->FindComponentByClass<UHouseFurnitureInteractionComponent>():nullptr;
    const bool bCanSlide = bWantsToSlide && IsMovingOnGround() && IsCrouching()
        && !IsCrawling() && !(Carry && Carry->bHoldingFurniture);
    if (!bIsSliding && bCanSlide && !bSlideConsumedForHold && Speed >= SlideStartSpeed)
    {
        bIsSliding = true;
        bSlideConsumedForHold = true;
        const float EntrySpeed = FMath::Clamp(FMath::Max(Speed, SlideEntrySpeed), 0.f, SlideMaxSpeed);
        Velocity = Velocity.GetSafeNormal2D() * EntrySpeed + FVector(0, 0, Velocity.Z);
    }
    else if (!bCanSlide || Speed < SlideEndSpeed)
    {
        bIsSliding = false;
    }
}

void UStoryMovementComponent::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
    Super::UpdateCharacterStateAfterMovement(DeltaSeconds);
    if (!IsMovingOnGround() || !IsCrouching() || IsCrawling() || Velocity.Size2D() < SlideEndSpeed) bIsSliding = false;
}

void UStoryMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
    bWantsToSlide = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
    bSlideConsumedForHold = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
    bWantsToCrawl = (Flags & FSavedMove_Character::FLAG_Custom_3) != 0;
}

FNetworkPredictionData_Client* UStoryMovementComponent::GetPredictionData_Client() const
{
    if (!ClientPredictionData)
    {
        auto* MutableThis = const_cast<UStoryMovementComponent*>(this);
        MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_StoryMovement(*this);
    }
    return ClientPredictionData;
}

void UStoryMovementComponent::ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity)
{
    if (CVarSSOFurniturePush.GetValueOnGameThread() == 0)
    {
        Super::ApplyImpactPhysicsForces(Impact, ImpactAcceleration, ImpactVelocity);
        return;
    }
    if (!bEnablePhysicsInteraction || !Impact.bBlockingHit) return;
    UPrimitiveComponent* Body = Impact.GetComponent();
    if (!Body) return;
    AHouseInteractiveProp* Prop = Cast<AHouseInteractiveProp>(Body->GetOwner());
    // A parked piece of furniture wakes up when it is bumped (server decides).
    if (!Body->IsSimulatingPhysics(Impact.BoneName) && Prop && Prop->Body == Body && Prop->HasAuthority()
        && Prop->CanBeMoved() && !Prop->IsClothFlag() && !Prop->bWallMounted)
        Prop->WakeForInteraction(CharacterOwner);
    if (!Body->IsSimulatingPhysics(Impact.BoneName)) return;
    // Never shove a piece someone (including us) is holding.
    if (Body->ComponentHasTag(TEXT("SSOFurnitureHeld"))) return;

    // Push only sideways along the floor: a body never gets lifted or flipped by
    // a shoulder, so nothing launches into the air.
    FVector PushDir(-Impact.ImpactNormal.X, -Impact.ImpactNormal.Y, 0.f);
    if (!PushDir.Normalize()) return;
    const float Into = FVector::DotProduct(FVector(ImpactVelocity.X, ImpactVelocity.Y, 0.f), PushDir);
    if (Into <= 5.f) return;

    const float Mass = FMath::Max(.5f, Body->GetMass());
    if (Prop && Prop->IsRollingChair())
    {
        // Casters: the chair rolls off at most of the bump speed, and an
        // off-centre bump sets it spinning on the spot.
        const float Target = FMath::Min(Into * .7f, 560.f);
        const FVector Velocity = Body->GetPhysicsLinearVelocity(Impact.BoneName);
        const float Current = FVector::DotProduct(Velocity, PushDir);
        if (Current < Target)
        {
            Body->AddImpulse(PushDir * (Target - Current), Impact.BoneName, true);
            const FVector Lever = Impact.ImpactPoint - Body->GetCenterOfMass(Impact.BoneName);
            const float Swivel = FVector::CrossProduct(FVector(Lever.X, Lever.Y, 0.f), PushDir).Z;
            Body->AddAngularImpulseInRadians(FVector(0, 0, FMath::Clamp(Swivel * .045f * (Target - Current) / 100.f, -4.f, 4.f)), Impact.BoneName, true);
        }
        return;
    }
    const ESSOCarrySize Size = UHouseFurnitureInteractionComponent::ClassifyFurniture(Body, Body->GetOwner());
    // How fast the piece may be nudged along, as a share of the player's speed.
    // Large furniture creeps a few cm/s even at a sprint, like leaning on a couch.
    float Share = 0.f, Cap = 0.f;
    switch (Size)
    {
    case ESSOCarrySize::Large:  Share = FMath::Clamp(3.f / Mass, .01f, .05f); Cap = 18.f; break;
    case ESSOCarrySize::Medium: Share = FMath::Clamp(9.f / Mass, .12f, .4f); Cap = 140.f; break;
    default:                    Share = FMath::Clamp(5.f / Mass, .5f, .95f); Cap = 520.f; break;
    }
    const float TargetSpeed = FMath::Min(Into * Share, Cap);
    const FVector BodyVelocity = Body->GetPhysicsLinearVelocity(Impact.BoneName);
    const float Current = FVector::DotProduct(BodyVelocity, PushDir);
    if (Current >= TargetSpeed) return;
    // Velocity change at the centre of mass: moves the piece, adds no spin.
    Body->AddImpulse(PushDir * (TargetSpeed - Current), Impact.BoneName, true);
    // Never let a shove leave a big piece hopping.
    if (Size != ESSOCarrySize::Small && BodyVelocity.Z > 20.f)
        Body->SetPhysicsLinearVelocity(FVector(BodyVelocity.X, BodyVelocity.Y, 20.f), false, Impact.BoneName);
}
