#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SeniorBraxtonVisual.generated.h"
class USkeletalMeshComponent;
class UGroomComponent;
class UAnimSequence;

// Native assembled character, shared by the selection turntable and network pawns.
// This actor is cosmetic and never replicated; character identity stays on PlayerState.
UCLASS()
class SENIORSENDOFF_API ASeniorBraxtonVisual : public AActor
{
    GENERATED_BODY()
public:
    ASeniorBraxtonVisual();
    UFUNCTION(BlueprintCallable) bool InitializeVisual(bool bPreview = false, bool bHideFromOwner = false);
    UFUNCTION(BlueprintCallable) void SetVisualActive(bool bActive);
    UFUNCTION(BlueprintCallable) void SetMoveSpeed(float Speed);
    UFUNCTION(BlueprintCallable) AActor* GetNativeCharacter() const { return NativeCharacter; }
    void KeepPreviewActive();
    void PrestreamPreviewTextures();
    void SetDouliEquipped(bool bEquipped, float Motion = 0, float FlightBlend = 0);
    USkeletalMeshComponent* GetBodyMesh() const { return Body; }
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<AActor> NativeCharacter;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<UGroomComponent> Hair;
    UPROPERTY(Transient) TArray<TObjectPtr<USkeletalMeshComponent>> Garments;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
    UPROPERTY(Transient) TObjectPtr<class UStaticMeshComponent> LobbyHat;
    bool bDouliEquipped = false;
    float Time = 0;
    float LastYaw = 0;
    float TurnVelocity = 0;
    FVector LastLocation = FVector::ZeroVector;
    bool bVisualActive = true;
    bool bWalking = false;
    bool bIsPreview = false;
    double PreviewLeaseUntil = 0;
};
