#pragma once

#include "CoreMinimal.h"

// Local, device-specific controls. Stored alongside Unreal's GameUserSettings
// so a new campaign or a lobby-to-house transition never resets them.
namespace SeniorPlayerPreferences
{
struct FSettings
{
    float MouseSensitivity = 1.0f;
    bool bInvertY = false;
    float FieldOfView = 90.0f;
};

SENIORSENDOFF_API const FSettings& Get();
SENIORSENDOFF_API void SetMouseSensitivity(float Value);
SENIORSENDOFF_API void SetInvertY(bool bValue);
SENIORSENDOFF_API void SetFieldOfView(float Value);
}
