#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseOxbloodMask.generated.h"

class AStoryFirstPersonCharacter;
class USceneComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UStaticMesh;

// A shared, one-time pickup in the Chapter 1 house. The pawn owns the bonus.
UCLASS()
class SENIORSENDOFF_API AHouseOxbloodMaskPickup : public AActor
{
    GENERATED_BODY()
public:
    AHouseOxbloodMaskPickup();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    bool CanCollect(const AStoryFirstPersonCharacter* Pawn) const;
    bool Collect(AStoryFirstPersonCharacter* Pawn);
    FVector InteractionLocation() const;

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> DisplayMask;
    bool bConsumed = false;

    bool FindSupportingSurface(const FVector& Start, const FVector& End, FHitResult& Hit) const;
    bool SupportIsUsable(UPrimitiveComponent* Component) const;
    bool CurrentSupportIsUsable() const;
    void SetSupport(const FHitResult& Hit, bool bPreserveAuthoredPosition);
    void ReleaseSupport();
    void FallToSupport(float DeltaSeconds);
    FVector VisibleSurfacePoint(const FHitResult& Hit) const;

    TWeakObjectPtr<UPrimitiveComponent> SupportComponent;
    TWeakObjectPtr<UStaticMesh> SupportMesh;
    TWeakObjectPtr<UPrimitiveComponent> RecentlyReleasedSupport;
    FTransform SupportRelativeTransform;
    FVector SupportLocalNormal = FVector::UpVector;
    FVector FallVelocity = FVector::ZeroVector;
    FVector LastSupportVelocity = FVector::ZeroVector;
    bool bSupportWasBroken = false;
    bool bSupportWasVisible = false;
    bool bSupportWasComponentHidden = false;
    bool bSupportWasActorHidden = false;
    double StartupResolveUntil = 0;
    double IgnoreReleasedUntil = 0;
};
