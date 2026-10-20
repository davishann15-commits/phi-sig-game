#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "HouseFrontPortico.generated.h"

class UHouseBlackHallSubsystem;
class UProceduralMeshComponent;
struct FHallMesh;

/** Four pinned cloth flags on the front columns; cosmetic, with no player collision. */
UCLASS()
class SENIORSENDOFF_API UHouseFrontFlagsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHouseFrontFlagsComponent();
    void Build(UHouseBlackHallSubsystem* Look);
    virtual void TickComponent(float Dt, ELevelTick Type, FActorComponentTickFunction* Function) override;
private:
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Cloth;
    struct FFlag
    {
        FVector Start, Along, Out, Down;
        float Phase = 0;
        int32 MaterialIndex = 0;
        TArray<FVector> Nodes, Previous;
        struct FLink { int32 A, B; float Length, Stiffness; };
        TArray<FLink> Links;
        TArray<FVector> Positions, Normals;
        TArray<FProcMeshTangent> Tangents;
        TArray<FVector2D> UV;
    };
    TArray<FFlag> Flags;
    float Accumulator = 0;
    float SimulationTime = 0;
    void Simulate(float Dt, float Time);
    void UpdateMesh();
};

/** Replaces the short lower columns with six full-height, turned white columns. */
void BuildHouseFrontPortico(FHallMesh& Architecture, AActor* Owner, UHouseBlackHallSubsystem* Look);
