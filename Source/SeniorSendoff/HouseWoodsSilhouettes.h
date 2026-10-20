#pragma once
#include "CoreMinimal.h"

class AActor;
class UHouseBlackHallSubsystem;

namespace HouseWoodsSilhouettes
{
/** Pass two: bare deadwood only. Human traces are deliberately kept in pass three. */
void Build(AActor* Owner, UHouseBlackHallSubsystem* Look);
}
