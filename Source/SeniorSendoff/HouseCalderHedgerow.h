#pragma once
#include "CoreMinimal.h"
class AActor;
class UWorld;
class UHouseBlackHallSubsystem;

namespace HouseCalderHedgerow
{
bool Enabled();
/** First farm-fence planting pass. No new collision or boundary-box edits. */
void Build(AActor* Owner, UHouseBlackHallSubsystem* Look);
bool DetailView(UWorld* World, FName Name, FVector& Eye, FVector& Target);
}
