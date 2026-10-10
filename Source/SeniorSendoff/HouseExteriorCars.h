#pragma once
#include "CoreMinimal.h"
struct FHallMesh;
class UHouseBlackHallSubsystem;
namespace HouseCars
{
    void CreateLook(UHouseBlackHallSubsystem* Look);
    void Build(FHallMesh& Mesh,int32 Style,TArray<FBox>& Collision);
}
