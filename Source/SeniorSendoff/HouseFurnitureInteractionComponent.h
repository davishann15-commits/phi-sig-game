#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HouseFurnitureInteractionComponent.generated.h"

class AActor;
class UPhysicsHandleComponent;
class UPrimitiveComponent;
class FLifetimeProperty;

/**
 * Lets an owning player drag a physics-simulated, tagged furniture actor.
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

    /** Bind this to the local E press. Press again to release. */
    UFUNCTION(BlueprintCallable, Category="Furniture")
    void ToggleGrab();

    /** Safe to call on death, possession loss, or any other forced release. */
    UFUNCTION(BlueprintCallable, Category="Furniture")
    void ReleaseGrab();

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Furniture")
    bool bHoldingFurniture = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture|Tuning", meta=(ClampMin="50.0"))
    float GrabRange = 260.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture|Tuning", meta=(ClampMin="50.0"))
    float MinimumHoldDistance = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture|Tuning", meta=(ClampMin="50.0"))
    float MaximumHoldDistance = 280.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture|Tuning", meta=(ClampMin="100.0"))
    float BreakDistance = 450.0f;

private:
    UFUNCTION(Server, Reliable)
    void ServerToggleGrab();

    UFUNCTION(Server, Reliable)
    void ServerReleaseGrab();

    bool GetServerView(FVector& OutLocation, FRotator& OutRotation) const;
    void ToggleGrabAuthoritative();
    void ReleaseGrabAuthoritative();

    UPROPERTY(Transient)
    TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;

    TWeakObjectPtr<AActor> HeldActor;
    TWeakObjectPtr<UPrimitiveComponent> HeldComponent;
    FRotator HeldRotation = FRotator::ZeroRotator;
    float HoldDistance = 0.0f;
};
