#pragma once
#include "CoreMinimal.h"
class AActor;
struct FHallMesh;
namespace HouseCalderFarmSolid
{
bool Enabled();
void Build(AActor* Owner,const FHallMesh& Boards,const FHallMesh& Roof,
    const FHallMesh& Fence,const FHallMesh& Hay,const TArray<FTransform>& Bales);
}
