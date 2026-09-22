#pragma once

#include "CoreMinimal.h"

class SWidget;

// Cosmetic, local-only animation. It never participates in party replication.
TSharedRef<SWidget> MakeSeniorLobbyAtmosphere(TAttribute<bool> Active);
bool IsSeniorLobbyMotionEnabled();
void SetSeniorLobbyMotionEnabled(bool Enabled);

#if WITH_EDITOR
struct FSeniorLobbyAtmosphereDiagnostics
{
    double Seconds = 0;
    uint64 Paints = 0;
    int32 Leaves = 0;
    int32 WindowPassers = 0;
    bool bSkyMaterial = false;
    bool bAnimating = false;
};
FSeniorLobbyAtmosphereDiagnostics GetSeniorLobbyAtmosphereDiagnostics();
// In-memory override for an isolated smoke test; never writes the user's settings.
void SetSeniorLobbyMotionTestOverride(TOptional<bool> Enabled);
void SetSeniorLobbyPhotoPreviewTime(TOptional<double> Seconds);
#endif
