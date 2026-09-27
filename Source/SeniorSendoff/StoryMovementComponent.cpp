#include "StoryMovementComponent.h"

#include "GameFramework/Character.h"

class FSavedMove_StoryMovement final : public FSavedMove_Character
{
public:
    bool bSavedWantsToSprint = false;
    bool bSavedWantsToSlide = false;
    bool bSavedIsSliding = false;
    bool bSavedSlideConsumedForHold = false;

    virtual void Clear() override
    {
        FSavedMove_Character::Clear();
        bSavedWantsToSprint = false;
        bSavedWantsToSlide = false;
        bSavedIsSliding = false;
        bSavedSlideConsumedForHold = false;
    }

    virtual uint8 GetCompressedFlags() const override
    {
        uint8 Flags = FSavedMove_Character::GetCompressedFlags();
        if (bSavedWantsToSprint) Flags |= FLAG_Custom_0;
        if (bSavedWantsToSlide) Flags |= FLAG_Custom_1;
        if (bSavedSlideConsumedForHold) Flags |= FLAG_Custom_2;
        return Flags;
    }

    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override
    {
        const auto* Other = static_cast<const FSavedMove_StoryMovement*>(NewMove.Get());
        return Other && bSavedWantsToSprint == Other->bSavedWantsToSprint
            && bSavedWantsToSlide == Other->bSavedWantsToSlide
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
    GetNavAgentPropertiesRef().bCanCrouch = true;
    bCanWalkOffLedgesWhenCrouching = true;
}

void UStoryMovementComponent::SetSlideHeld(bool bHeld)
{
    bWantsToSlide = bHeld;
    if (!bHeld) bSlideConsumedForHold = false;
    // The base component predicts and replicates crouch using its own saved flag.
    bWantsToCrouch = bHeld;
}

bool UStoryMovementComponent::IsSprinting() const
{
    return bWantsToSprint && IsMovingOnGround() && !IsCrouching() && !bIsSliding;
}

float UStoryMovementComponent::GetMaxSpeed() const
{
    if (IsMovingOnGround())
    {
        if (bIsSliding) return SlideMaxSpeed;
        if (IsSprinting()) return SprintSpeed;
    }
    return Super::GetMaxSpeed();
}

void UStoryMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
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

void UStoryMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
    const float Speed = Velocity.Size2D();
    const bool bCanSlide = bWantsToSlide && IsMovingOnGround() && IsCrouching();
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
    if (!IsMovingOnGround() || !IsCrouching() || Velocity.Size2D() < SlideEndSpeed) bIsSliding = false;
}

void UStoryMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
    bWantsToSlide = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
    bSlideConsumedForHold = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
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
