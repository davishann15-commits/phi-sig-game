#pragma once

#include "CoreMinimal.h"

struct FHallMesh;
class UHouseBlackHallSubsystem;

/** Cosmetic, deterministic foliage for the existing house site. No new gameplay actors or lights. */
namespace HouseNature
{
    void CreateLook(UHouseBlackHallSubsystem* Look);
    void Tree(FHallMesh& Mesh, bool bBig, int32 Variant, TArray<FBox>& Collision);
    void Shrub(FHallMesh& Mesh, int32 Variant, TArray<FBox>& Collision);
    void GrassClump(FHallMesh& Mesh, bool bDry, int32 Variant);
    void LeafLitter(FHallMesh& Mesh, int32 Variant);
    void SingleLeaf(FHallMesh& Mesh, int32 Variant);
    /** Exterior lawn, mulch and pedestrian surfaces; excludes the house and road. */
    bool IsLeafGround(float X, float Y);
    float ApproximateGroundHeight(float X, float Y);
    void Scatter(TArray<FTransform>& Green, TArray<FTransform>& Dry, TArray<FTransform>& Leaves,
        const TArray<FTransform>& BigTrees, const TArray<FTransform>& SmallTrees, const TArray<FTransform>& Shrubs);
}
