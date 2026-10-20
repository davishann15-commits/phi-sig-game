#pragma once
#include "CoreMinimal.h"
class AActor;
namespace HouseCalderRidgeForest
{
/** Opt-in merged scenery only. Existing terrain/ridge sheets remain untouched. */
bool Enabled();
void Build(AActor* Owner);
}
