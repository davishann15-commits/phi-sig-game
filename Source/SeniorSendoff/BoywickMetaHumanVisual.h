#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoywickMetaHumanVisual.generated.h"

class ABoywickCharacter;
class USkeletalMeshComponent;
class UAnimSequence;

UCLASS()
class SENIORSENDOFF_API ABoywickMetaHumanVisual : public AActor
{
    GENERATED_BODY()
public:
    ABoywickMetaHumanVisual();
    bool Initialize(ABoywickCharacter* Character);
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    USkeletalMeshComponent* GetBody() const { return Body; }
    USkeletalMeshComponent* GetFace() const { return Face; }
    bool IsReady() const { return Native && Body && Face && Idle && Walk && Jog; }
private:
    UPROPERTY(Transient) TObjectPtr<ABoywickCharacter> Character;
    UPROPERTY(Transient) TObjectPtr<AActor> Native;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Face;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Jog;
    float Clock = .72f, Talk = 0.f, Angry = 0.f, Sleep = 0.f;
};
