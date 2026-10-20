#pragma once
#include "CoreMinimal.h"
#if !UE_BUILD_SHIPPING
class UWorld;class APlayerController;class AStoryFirstPersonCharacter;class AHouseCalderWildlifeDirector;
namespace HouseCalderWildlifeApproachReview
{
bool BeginNetwork(UWorld* World,APlayerController* PC,AStoryFirstPersonCharacter* Pawn,AHouseCalderWildlifeDirector* Director);
bool NetworkFled(int32& Id);
}
#endif
