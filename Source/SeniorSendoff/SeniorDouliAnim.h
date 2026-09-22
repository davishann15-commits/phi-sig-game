#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SeniorDouliAnim.generated.h"
class UAnimSequence;
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
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
