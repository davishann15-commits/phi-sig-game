#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SeniorDouliAnim.generated.h"
class UAnimSequence;
class AHouseRoamingPig;
UCLASS(Transient)
class SENIORSENDOFF_API USeniorDouliAnim : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> BaseSequence;
    UPROPERTY(Transient) bool bEquipped = false;
    UPROPERTY(Transient) bool bFirstPerson = false;
    UPROPERTY(Transient) bool bLobbyIdle = false;
    UPROPERTY(Transient) float LobbyTime = 0;
    UPROPERTY(Transient) bool bSelectionRoom = false;
    UPROPERTY(Transient) float SelectionRoomTime = 0;
    UPROPERTY(Transient) float Motion = 0;
    UPROPERTY(Transient) float FlightBlend = 0;
    UPROPERTY(Transient) float Speed = 0;
    // All targets are captured on the game thread in the mesh's component
    // space. The animation worker never reads a live pig or player actor.
    UPROPERTY(Transient) float PigCarryBlend = 0;
    UPROPERTY(Transient) float PigGripBlend = 0;
    UPROPERTY(Transient) bool bHoldingPig = false;
    UPROPERTY(Transient) bool bPigPlacing = false;
    UPROPERTY(Transient) FVector PigGripTarget = FVector::ZeroVector;
    UPROPERTY(Transient) FVector PigGripDirection = FVector::UpVector;
    UPROPERTY(Transient) FVector PigHandForward = FVector(0, -1, 0);
    UPROPERTY(Transient) TObjectPtr<AHouseRoamingPig> PigTargetActor;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
