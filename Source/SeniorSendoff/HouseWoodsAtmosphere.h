#pragma once
#include "CoreMinimal.h"

class AActor;
namespace HouseWoodsAtmosphere
{
// Retunes only this exterior's existing woodland mist; three extra analytic
// pools follow the existing hollow floors. No lights or volumetric simulation.
void Apply(AActor* Owner);
float DepthAt(const FVector& Position);
// World-lifetime lease on the local-fog near limit, restored when the last
// runtime dusk component ends. Individual old fog volumes retain 10m starts.
void AcquireNearFog();
void ReleaseNearFog();
}
