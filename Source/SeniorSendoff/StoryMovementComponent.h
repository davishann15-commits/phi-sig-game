#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StoryMovementComponent.generated.h"

/** Movement shared by the story pawn on the host and predicted LAN clients. */
UCLASS(ClassGroup=(Movement), meta=(BlueprintSpawnableComponent))
class SENIORSENDOFF_API UStoryMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UStoryMovementComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Sprint", meta=(ClampMin="0", ForceUnits="cm/s"))
    float SprintSpeed = 750.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Slide", meta=(ClampMin="0", ForceUnits="cm/s"))
    float SlideStartSpeed = 570.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Slide", meta=(ClampMin="0", ForceUnits="cm/s"))
    float SlideEndSpeed = 320.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Slide", meta=(ClampMin="0", ForceUnits="cm/s"))
    float SlideMaxSpeed = 900.f;

    // A sprint-to-slide should carry enough speed to be a useful traversal
    // move, even though crouched walking itself is deliberately slow.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Slide", meta=(ClampMin="0", ForceUnits="cm/s"))
    float SlideEntrySpeed = 820.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Slide", meta=(ClampMin="0"))
    float SlideGroundFriction = 0.18f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Slide", meta=(ClampMin="0", ForceUnits="cm/s/s"))
    float SlideBrakingDeceleration = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Slide", meta=(ClampMin="0", ClampMax="1"))
    float SlideSteeringMultiplier = 0.2f;

    // Air acceleration adds speed along the input direction without erasing the
    // velocity already earned on the ground or by a previous air strafe.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Air", meta=(ClampMin="0", ForceUnits="cm/s"))
    float AirWishSpeed = 350.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Air", meta=(ClampMin="0", ForceUnits="cm/s/s"))
    float AirAcceleration = 1600.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Air", meta=(ClampMin="0", ForceUnits="cm/s"))
    float AirSpeedCap = 1000.f;

    void SetSprintHeld(bool bHeld) { bWantsToSprint = bHeld; }
    void SetSlideHeld(bool bHeld);

    UFUNCTION(BlueprintPure, Category="Movement") bool IsSliding() const { return bIsSliding; }
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSprinting() const;
    UFUNCTION(BlueprintPure, Category="Movement") float GetHorizontalSpeed() const { return Velocity.Size2D(); }

    virtual float GetMaxSpeed() const override;
    virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
    virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
    virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

private:
    friend class FSavedMove_StoryMovement;

    bool bWantsToSprint = false;
    bool bWantsToSlide = false;
    bool bIsSliding = false;
    bool bSlideConsumedForHold = false;
};
