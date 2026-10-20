#pragma once
#include "CoreMinimal.h"
class AActor;

namespace HouseWoodsPass
{
/** 0 retains the baseline; 1 structure/floor; 2 atmosphere; 3 motion/traces. */
int32 Stage();
void RecordOriginalTrees(const TArray<FTransform>& Big, const TArray<FTransform>& Small,
    const TArray<FTransform>& Distant);
void Audit(AActor* Owner);
}
