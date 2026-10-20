#pragma once

#include "CoreMinimal.h"

class AActor;
class UHouseBlackHallSubsystem;

/** Code-painted late-October forest floor; preserves the existing terrain and tree transforms. */
namespace HouseWoodsFloor
{
void CreateLook(UHouseBlackHallSubsystem* Look);
void Build(AActor* Owner, UHouseBlackHallSubsystem* Look, const TArray<FTransform>& ExistingTrees);
}
