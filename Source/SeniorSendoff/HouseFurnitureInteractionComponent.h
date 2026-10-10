#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SSOCarrySize.h"
#include "HouseFurnitureInteractionComponent.generated.h"

class AActor;
class UPhysicsHandleComponent;
class UPrimitiveComponent;
class FLifetimeProperty;

/**
 * Lets an owning player pick up, carry, set down, drag and throw a
 * physics-simulated, tagged furniture actor. Three handling sizes:
 *  - Small:  one hand, full movement speed, wind-up and long throw.
 *  - Medium: both hands on the sides, slower movement, chest-height carry, short toss.
 *  - Large:  brace and heave one side off the floor, then drag it; shove instead of throw.
 * The component must be a replicated default subobject of a replicated pawn.
 */
UCLASS(ClassGroup=(SeniorSendoff), meta=(BlueprintSpawnableComponent))
class SENIORSENDOFF_API UHouseFurnitureInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHouseFurnitureInteractionComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Bind this to the local E press. Press again to set the piece down. */
    UFUNCTION(BlueprintCallable, Category="Furniture")
    void ToggleGrab();

    /** Safe to call on death, possession loss, or any other forced release. Always immediate. */
    UFUNCTION(BlueprintCallable, Category="Furniture")
    void ReleaseGrab();

    /** Preserve the held body and constraint through a periodic hallway step. */
    void TranslateHeldFurniture(const FVector& Delta);

    /** LMB / right trigger: wind up and throw (small/medium) or shove (large). */
    UFUNCTION(BlueprintCallable, Category="Furniture")
    void ThrowHeldFurniture();

    /** One local view trace for a focused, available movable furniture prompt. */
    UFUNCTION(BlueprintCallable, Category="Furniture")
    AActor* FocusedFurnitureForInteraction() const;

    /** Handling size for a body (prop override first, then mass and footprint). */
    static ESSOCarrySize ClassifyFurniture(const UPrimitiveComponent* Body, const AActor* Furniture);

    UPROPERTY(ReplicatedUsing=OnRep_HoldingFurniture, BlueprintReadOnly, Category="Furniture")
    bool bHoldingFurniture = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture|Tuning", meta=(ClampMin="50.0"))
    float GrabRange = 260.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture|Tuning", meta=(ClampMin="30.0"))
    float MinimumHoldDistance = 52.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture|Tuning", meta=(ClampMin="50.0"))
    float MaximumHoldDistance = 85.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture|Tuning", meta=(ClampMin="100.0"))
    float BreakDistance = 450.0f;

    /** When false, set-down and throw happen instantly (automated smoke tests). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture|Tuning")
    bool bAnimatedTransitions = true;

    /** Launch speed (cm/s) for a small one-handed throw before weight scaling. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture|Tuning")
    float SmallThrowSpeed = 1450.f;

    /** Launch speed (cm/s) for a medium two-handed toss before weight scaling. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture|Tuning")
    float MediumThrowSpeed = 620.f;

    /** Actual visual-surface contacts, sampled again by the animation proxy. */
    bool GetHandContact(bool bRight, FVector& Position, FVector& Normal) const;
    /** World finger direction for the hand frame; false = use the default up-projected fingers. */
    bool GetHandFingerDirection(bool bRight, FVector& OutFingers) const;
    UPrimitiveComponent* GetHeldBody() const { return HeldBody; }
    float GetCarryPoseBlend() const { return CarryPoseBlend; }
    float GetFingerGripBlend() const { return FingerGripBlend; }
    float GetPickupBend() const;
    float GetPickupLean() const;
    float GetMovementScale() const;
    bool UsesTwoHands() const { return bTwoHandedGrip; }
    bool IsDraggingHeavyFurniture() const { return bDraggingHeavy && bHoldingFurniture; }
    ESSOCarrySize GetCarrySize() const { return CarrySize; }
    ESSOCarryPhase GetCarryPhase() const { return CarryPhase; }
    /** Sprinting is only allowed with a small one-handed piece. */
    bool BlocksSprint() const { return bHoldingFurniture && CarrySize != ESSOCarrySize::Small; }
    /** 0..1 effort used for bracing, trembling and body strain. */
    float GetStrain() const;
    /** 0..1 eased throw/shove wind-up. */
    float GetThrowWindup() const;
    /** 0..1..0 follow-through after the release. */
    float GetThrowFollowThrough() const;
    /** Small first-person camera offset (X forward, Y right, Z up, cm) for effort, bob and throws. */
    FVector GetCameraEffortOffset() const;
    /** Diagnostics: server distance between the held grip and its carry target last frame. */
    float GetDebugTargetError() const { return DebugTargetError; }

private:
    UFUNCTION() void OnRep_HoldingFurniture();
    UFUNCTION() void OnRep_CarryPhase();
    bool CaptureHandContacts(UPrimitiveComponent* Body, const FVector& Point, const FVector& ViewLocation, const FVector& ViewRight, const FVector& ViewForward, FVector& OutSurface);
    float LiftDuration() const;
    float BraceDuration() const;
    float WindupDuration() const;
    float SetDownDuration() const;
    float FollowDuration() const;
    float LargeHeave() const;
    void BeginSetDownAuthoritative();
    void ExecuteThrowAuthoritative();
    void StartFollowThrough();

    UPROPERTY(Replicated) TObjectPtr<UPrimitiveComponent> HeldBody;
    UPROPERTY(Replicated) bool bDraggingHeavy = false;
    UPROPERTY(Replicated) bool bTwoHandedGrip = true;
    UPROPERTY(Replicated) ESSOCarrySize CarrySize = ESSOCarrySize::Medium;
    UPROPERTY(ReplicatedUsing=OnRep_CarryPhase) ESSOCarryPhase CarryPhase = ESSOCarryPhase::None;
    UPROPERTY(Replicated) float InitialBend = 0.f;
    UPROPERTY(Replicated) float SetDownBend = 0.f;
    UPROPERTY(Replicated) float HeldMass = 0.f;
    UPROPERTY(Replicated) FVector LocalRightContact = FVector::ZeroVector;
    UPROPERTY(Replicated) FVector LocalLeftContact = FVector::ZeroVector;
    UPROPERTY(Replicated) FVector LocalRightNormal = FVector::ForwardVector;
    UPROPERTY(Replicated) FVector LocalLeftNormal = FVector::ForwardVector;
    /** Body-local finger reference (zero = default). */
    UPROPERTY(Replicated) FVector LocalFingerDirection = FVector::ZeroVector;
    /** Body-local outward normal of the face a large piece is lifted from. */
    UPROPERTY(Replicated) FVector LocalFaceNormal = FVector::ZeroVector;
    FVector LastRightContact = FVector::ZeroVector, LastLeftContact = FVector::ZeroVector;
    FVector LastRightNormal = FVector::ForwardVector, LastLeftNormal = FVector::ForwardVector;
    FVector PickupGripStart = FVector::ZeroVector, SmoothedTarget = FVector::ZeroVector, SpringVelocity = FVector::ZeroVector;
    FVector SmoothedOffset = FVector::ZeroVector;
    FVector SetDownFrom = FVector::ZeroVector, SetDownTo = FVector::ZeroVector;
    FVector ThrowDirection = FVector::ForwardVector;
    FVector LastRightFingers = FVector::ZeroVector, LastLeftFingers = FVector::ZeroVector;
    float PickupAge = 0.f, PhaseAge = 0.f, FollowAge = 100.f, CarryPoseBlend = 0.f, FingerGripBlend = 0.f;
    // Sound bookkeeping (every machine watches the replicated state and plays its own clip).
    bool bSfxWasHolding = false;
    ESSOCarryPhase SfxLastPhase = ESSOCarryPhase::None;
    uint8 SfxHeldSurface = 0;
    FVector SfxHeldLocation = FVector::ZeroVector;
    float WalkPhase = 0.f, MoveAlpha = 0.f, EffortClock = 0.f;
    float GripHeightAboveBottom = 0.f, FaceSpan = 100.f;
    ESSOCarrySize LastReleasedSize = ESSOCarrySize::Medium;
    float SavedLinearDamping = .45f, SavedAngularDamping = 1.25f;
    TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
    float DebugTargetError = 0.f;
    FVector LeadVelocity = FVector::ZeroVector;
    UFUNCTION(Server, Reliable)
    void ServerToggleGrab();

    UFUNCTION(Server, Reliable)
    void ServerReleaseGrab();

    UFUNCTION(Server, Reliable)
    void ServerThrowHeldFurniture();

    bool GetServerView(FVector& OutLocation, FRotator& OutRotation) const;
    void ToggleGrabAuthoritative();
    void ReleaseGrabAuthoritative();
    void ThrowHeldFurnitureAuthoritative();

    UPROPERTY(Transient)
    TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;

    TWeakObjectPtr<AActor> HeldActor;
    TWeakObjectPtr<UPrimitiveComponent> HeldComponent;
    FRotator HeldRotation = FRotator::ZeroRotator, StartingBodyRotation = FRotator::ZeroRotator;
    float StartingViewYaw = 0.f;
    FVector HeldLocalGrip = FVector::ZeroVector;
    float HoldDistance = 0.0f;
};
