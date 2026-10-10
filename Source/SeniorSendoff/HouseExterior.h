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

UCLASS(NotPlaceable, Transient)
class SENIORSENDOFF_API AHouseExteriorDetail : public AActor
{
    GENERATED_BODY()
public:
    AHouseExteriorDetail();
    void Build(UHouseBlackHallSubsystem* Look);
    int32 Triangles = 0, Instances = 0, Lights = 0;
    FString Notes;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
    UPROPERTY(Transient) TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Instanced;
    UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> PointLights;
    UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> Blockers;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> KeepAlive;
};

/** Opt-in (-HouseExteriorViews) fixed-view screenshot sweep with frame-time probes inside and outside. */
void TickHouseExteriorViews(AStoryFirstPersonCharacter* Pawn);
