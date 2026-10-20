// Runtime-built exterior of the fraternity house (brick shell, roofs, portico, porch arcade, porte-cochere) and the
// night-time grounds around it (yard, street loop, parking, neighbours, stadium, trees, lights). Built locally on every
// machine from the measured interior, so nothing in the saved map changes.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseExterior.generated.h"

class UHouseBlackHallSubsystem;
class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UPointLightComponent;
class UBoxComponent;
class AStoryFirstPersonCharacter;
class AHouseTreeReaction;
class UPrimitiveComponent;
class AHouseReactiveFixture;

struct FHouseTreeBatch
{
    TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Component;
    uint8 Kind=0, Variant=0;
};

UCLASS(NotPlaceable, Transient)
class SENIORSENDOFF_API AHouseExteriorDetail : public AActor
{
    GENERATED_BODY()
public:
    AHouseExteriorDetail();
    virtual void Tick(float DeltaSeconds) override;
    void Build(UHouseBlackHallSubsystem* Look);
    virtual float TakeDamage(float Damage,const FDamageEvent& Event,AController* Instigator,AActor* Causer) override;
    int32 Triangles = 0, Instances = 0, Lights = 0;
    FString Notes;
private:
    void OnConnectedWindowBroken(AHouseReactiveFixture* Window);
    TArray<TWeakObjectPtr<AHouseReactiveFixture>> ConnectedWindows;
    TWeakObjectPtr<UPrimitiveComponent> LandingGuard;
    FTransform GuardStart;
    float GuardFallAge=-1.f,GuardDrop=0.f;
    bool ReactToTreeHit(UPrimitiveComponent* Component,const FHitResult& Hit,const FVector& Direction);
    UFUNCTION() void OnTreeHit(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit);
    TArray<FHouseTreeBatch> TreeBatches;
    UPROPERTY(Transient) TObjectPtr<AHouseTreeReaction> TreeReaction;
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
    UPROPERTY(Transient) TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Instanced;
    UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> PointLights;
    UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> Blockers;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> KeepAlive;
};

/** Opt-in (-HouseExteriorViews) fixed-view screenshot sweep with frame-time probes inside and outside. */
void TickHouseExteriorViews(AStoryFirstPersonCharacter* Pawn);
