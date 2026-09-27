#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SeniorRunnerVisual.generated.h"

class USkeletalMeshComponent;
class UAnimSequence;

// Cosmetic MetaHuman for C02. The replicated roster index remains authoritative.
UCLASS()
class SENIORSENDOFF_API ASeniorRunnerVisual : public AActor
{
    GENERATED_BODY()
public:
    ASeniorRunnerVisual();
    bool InitializeVisual(bool bPreview, bool bHideFromOwner);
    void KeepPreviewActive();
    void SetVisualActive(bool bActive);
    void PrestreamPreviewTextures();
    void SetMoveSpeed(float Speed);
    AActor* GetNativeCharacter() const { return NativeCharacter; }
    USkeletalMeshComponent* GetBodyMesh() const { return Body; }
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<AActor> NativeCharacter;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Garment;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
    bool bPreview = false;
    bool bActive = true;
    bool bWalking = false;
    double PreviewLeaseUntil = 0;
};
