#pragma once

#include "CoreMinimal.h"

class SWidget;
class UWorld;

// A local, transparent 3D preview. No replicated actor or editor-only viewport is required.
TSharedRef<SWidget> MakeSeniorCharacterPreview(
    UWorld* World, TAttribute<int32> CharacterIndex, bool bInteractive = true, TAttribute<int32> WeaponIndex = 0);
TSharedRef<SWidget> MakeSeniorCharacterRoomPreview(UWorld* World,TAttribute<int32> CharacterIndex,
    TAttribute<int32> WeaponIndex,TFunction<void(int32)> OnWeapon,FSimpleDelegate OnBack);
