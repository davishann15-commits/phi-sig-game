#pragma once

#include "CoreMinimal.h"

class SWidget;
class ASeniorLobbyController;

TSharedRef<SWidget> MakeSeniorLobbyWidget(ASeniorLobbyController* Controller);
TSharedRef<SWidget> MakeSeniorLoadingWidget(int32 Chapter = 1);
