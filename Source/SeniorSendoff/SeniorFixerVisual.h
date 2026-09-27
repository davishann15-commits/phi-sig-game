#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SeniorFixerVisual.generated.h"

class USkeletalMeshComponent;
class UAnimSequence;

// C03's assembled native character. Identity is replicated by PlayerState;
// each client owns this local cosmetic actor and its native render components.
UCLASS()
class SENIORSENDOFF_API ASeniorFixerVisual : public AActor
{
    GENERATED_BODY()
public:
    ASeniorFixerVisual();
    static constexpr float HeightCm = 168.f;
    bool InitializeVisual(bool bPreview, bool bHideFromOwner);
    void KeepPreviewActive();
    void SetVisualActive(bool bActive);
    void PrestreamPreviewTextures();
    void SetMoveSpeed(float Speed);
    AActor* GetNativeCharacter() const { return NativeCharacter; }
    USkeletalMeshComponent* GetBodyMesh() const { return Body; }
    UAnimSequence* GetIdleAnimation() const { return Idle; }
    UAnimSequence* GetWalkAnimation() const { return Walk; }
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<AActor> NativeCharacter;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
    bool bPreview = false;
    bool bActive = true;
    bool bWalking = false;
    double PreviewLeaseUntil = 0;
};
