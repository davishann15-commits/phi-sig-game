#pragma once
#include "CoreMinimal.h"
struct FHallMesh;
namespace HouseCalderHedgerowImpl
{
extern const FName CaneKey, CedarKey, RoseKey, BriarKey, HipKey, SprayKey;
void Geometry(FHallMesh& Mesh, int32 LOD, TArray<FVector>* Hips = nullptr, TArray<FVector>* Cedars = nullptr);
}
