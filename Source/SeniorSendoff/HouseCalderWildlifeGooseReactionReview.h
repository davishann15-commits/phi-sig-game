#pragma once
#include "CoreMinimal.h"
#if !UE_BUILD_SHIPPING
class UWorld;class APlayerController;class AStoryFirstPersonCharacter;class AHouseCalderWildlifeDirector;
namespace HouseCalderWildlifeGooseReactionReview
{
bool BeginNetwork(UWorld* World,APlayerController* PC,AStoryFirstPersonCharacter* Pawn,AHouseCalderWildlifeDirector* Director);
bool NetworkDone(bool& Passed,int32& Id);
int32 NetworkFocal();
}
#endif
