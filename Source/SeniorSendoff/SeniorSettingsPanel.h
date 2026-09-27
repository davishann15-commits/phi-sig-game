#pragma once

#include "CoreMinimal.h"

class SWidget;

// Shared by the story lobby and the in-game pause screen. Fits a 1480 x 570
// viewport but scrolls when more controls are added later.
SENIORSENDOFF_API TSharedRef<SWidget> MakeSeniorSettingsPanel();
