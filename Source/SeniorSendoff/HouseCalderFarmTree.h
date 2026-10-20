#pragma once
#include "CoreMinimal.h"

class AActor;
class UWorld;
class UMaterialInterface;
struct FHallMesh;

namespace HouseCalderFarmTree
{
bool Enabled();
bool Occupied(const FVector2D& Point, float Radius);
bool Materials(UObject* Outer, TMap<FName, UMaterialInterface*>& Out);
void Geometry(FHallMesh& Mesh, int32 LOD);
void BuildCollision(AActor* Owner);
bool DetailView(UWorld* World, FName Name, FVector& Eye, FVector& Target);
}
