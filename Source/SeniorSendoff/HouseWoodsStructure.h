#pragma once

#include "CoreMinimal.h"

class AActor;
class UHouseBlackHallSubsystem;

/** Additive, deterministic Appalachian canopy and understory. Never mutates the old tree grid. */
namespace HouseWoodsStructure
{
void Build(AActor* Owner, UHouseBlackHallSubsystem* Look, const TArray<FTransform>& ExistingTrees);
/** Seven foliage MIDs, plus five fixed-root twig MIDs in pass three, for the shared gust controller. */
TArray<FName> SwayMaterialKeys();
}
