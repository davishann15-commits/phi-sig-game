#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BoywickAnimInstance.generated.h"

class UAnimSequence;

/** All state is sampled on the game thread before the animation worker runs. */
UCLASS(Transient)
class SENIORSENDOFF_API UBoywickAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Jog;
    UPROPERTY(Transient) float Speed = 0.f;
    UPROPERTY(Transient) float Talking = 0.f;
    UPROPERTY(Transient) float Anger = 0.f;
    UPROPERTY(Transient) float Time = 0.f;
    UPROPERTY(Transient) float HeadYaw = 0.f;
    UPROPERTY(Transient) float HeadPitch = 0.f;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};

/** Copy the animated body, then supply expression curves to the native RigLogic post process. */
UCLASS(Transient)
class SENIORSENDOFF_API UBoywickFaceAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) float Talking = 0.f;
    UPROPERTY(Transient) float Anger = 0.f;
    UPROPERTY(Transient) float Time = 0.f;
    UPROPERTY(Transient) float SpeechPhase = 0.f;
    UPROPERTY(Transient) float Sleep = 0.f;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
