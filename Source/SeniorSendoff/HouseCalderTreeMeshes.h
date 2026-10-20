#pragma once
#include "CoreMinimal.h"
class UStaticMesh;
namespace HouseCalderTreeMeshes
{
/** Preview geometry: reduced leaf cards, intact lower trunk, clustered upper
 * wood, or independent pre-reduced wood LODs. Read-only source assets,
 * full-precision UV/pivot data, no collision. */
UStaticMesh* RetainStructure(UObject* Outer,UStaticMesh* Leaves,UStaticMesh* Structure,
    float NearGridCm=2.f,float MiddleGridCm=12.f,float FarGridCm=24.f,int32 ForestBudget=0);
/** Fit render geometry to an existing woods envelope; duplicate its exact
 * simple body without moving instances or modifying the original mesh/body. */
UStaticMesh* FitToOriginal(UObject* Outer,UStaticMesh* Prepared,UStaticMesh* Original,float CrownLift=0.f);
}
