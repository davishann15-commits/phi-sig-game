#pragma once

#include "CoreMinimal.h"

class ASeniorLobbyController;
class SWidget;

enum class ESeniorLobbyPage : uint8
{
    Home,
    Character,
    Loadout,
    Settings,
    Achievements,
    MainLobby,
    News,
    Credits
};

// Content uses the lobby's 1600 x 900 design space and the caller's backdrop.
TSharedRef<SWidget> MakeSeniorLobbyPage(ASeniorLobbyController* Controller,
    ESeniorLobbyPage Page, FSimpleDelegate OnBack);
