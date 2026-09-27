#include "SeniorPlayerPreferences.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace SeniorPlayerPreferences
{
namespace
{
constexpr const TCHAR* Section = TEXT("SeniorSendoff.PlayerPreferences");
FSettings Current;
bool bLoaded = false;

void Load()
{
    if (bLoaded) return;
    bLoaded = true;
    if (!GConfig) return;
    GConfig->GetFloat(Section, TEXT("MouseSensitivity"), Current.MouseSensitivity, GGameUserSettingsIni);
    GConfig->GetBool(Section, TEXT("InvertY"), Current.bInvertY, GGameUserSettingsIni);
    GConfig->GetFloat(Section, TEXT("FieldOfView"), Current.FieldOfView, GGameUserSettingsIni);
    Current.MouseSensitivity = FMath::Clamp(Current.MouseSensitivity, 0.15f, 4.0f);
    Current.FieldOfView = FMath::Clamp(Current.FieldOfView, 80.0f, 115.0f);
}

void Save()
{
    // The opt-in native input test changes the in-memory preference twice.
    // It must never rewrite the player's actual settings file.
    if (FParse::Param(FCommandLine::Get(), TEXT("CombinedHouseSmoke"))) return;
    if (!GConfig) return;
    GConfig->SetFloat(Section, TEXT("MouseSensitivity"), Current.MouseSensitivity, GGameUserSettingsIni);
    GConfig->SetBool(Section, TEXT("InvertY"), Current.bInvertY, GGameUserSettingsIni);
    GConfig->SetFloat(Section, TEXT("FieldOfView"), Current.FieldOfView, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}
}

const FSettings& Get()
{
    Load();
    return Current;
}

void SetMouseSensitivity(float Value)
{
    Load();
    Current.MouseSensitivity = FMath::Clamp(Value, 0.15f, 4.0f);
    Save();
}

void SetInvertY(bool bValue)
{
    Load();
    Current.bInvertY = bValue;
    Save();
}

void SetFieldOfView(float Value)
{
    Load();
    Current.FieldOfView = FMath::Clamp(Value, 80.0f, 115.0f);
    Save();
}
}
