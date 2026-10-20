#pragma once

#include "CoreMinimal.h"

struct FHallMesh;
class UHouseBlackHallSubsystem;

/** Shared geometry and canopy origins for the locally built, instanced trees. */
namespace HouseNature
{
    enum class ETreeKind : uint8 { Maple, Oak, Birch, Count };
    enum class EYardDebris : uint8 { Stick, Pinecone, Paper, Can, Cup, Count };
    struct FTreeCanopy
    {
        ETreeKind Kind = ETreeKind::Maple;
        uint8 Variant = 0;
        TArray<FVector> Leaves;
    };
    void CreateLook(UHouseBlackHallSubsystem* Look);
    void Tree(FHallMesh& Mesh, ETreeKind Kind, int32 Variant, TArray<FBox>& Collision, TArray<FVector>* LeafOrigins = nullptr);
    const TCHAR* TreeName(ETreeKind Kind);
    void Shrub(FHallMesh& Mesh, int32 Variant, TArray<FBox>& Collision);
    /** Fine twig / cupped-leaf mesh, used only in the existing front planting beds. */
    void FrontShrub(FHallMesh& Mesh, int32 Variant, TArray<FBox>& Collision);
    void GrassClump(FHallMesh& Mesh, bool bDry, int32 Variant);
    void LeafLitter(FHallMesh& Mesh, int32 Variant);
    void SingleLeaf(FHallMesh& Mesh, int32 Variant, ETreeKind Kind = ETreeKind::Maple);
    void YardDebris(FHallMesh& Mesh, EYardDebris Kind, int32 Variant);
    /** Sparse visual detail on the front lawn; no walking or weapon collision. */
    void ScatterYardDebris(TArray<FTransform>& Sticks, TArray<FTransform>& Pinecones,
        TArray<FTransform>& Paper, TArray<FTransform>& Cans, TArray<FTransform>& Cups);
    /** Exterior lawn, mulch and pedestrian surfaces; excludes the house and road. */
    bool IsLeafGround(float X, float Y);
    /** The ground surface the nature scatter stands on: the court bank, the woods, else the grade (west field included)
     *  plus the Calder relief (HouseCalderTerrain::ReliefLattice; 0 in the frozen zones). */
    float ApproximateGroundHeight(float X, float Y);
    void Scatter(TArray<FTransform>& Green, TArray<FTransform>& Dry, TArray<FTransform>& Leaves,
        const TArray<FTransform>& BigTrees, const TArray<FTransform>& SmallTrees, const TArray<FTransform>& Shrubs);
}
