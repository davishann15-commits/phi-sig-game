#pragma once
#include "CoreMinimal.h"

class UWorld;
class AStoryFirstPersonCharacter;
class APlayerController;
class FJsonObject;

/** Opt-in helper for HouseInteractivitySmoke. True means complete; a nonempty failure means fail. */
bool TickHouseFixtureSmoke(UWorld* World, AStoryFirstPersonCharacter* Pawn, APlayerController* PC,
    FString& OutFailure, TSharedPtr<FJsonObject>& OutReport);

/** Isolated, opt-in fixture verification; bypasses unrelated house-prop audits. */
void TickHouseGlassSmoke(UWorld* World);
