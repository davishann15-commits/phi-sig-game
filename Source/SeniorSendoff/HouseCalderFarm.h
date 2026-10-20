#pragma once
#include "CoreMinimal.h"

class AActor;
class UWorld;
namespace HouseCalderFarm
{
bool Enabled();
/** Pure, deterministic scenery footprints for ground cover and later habitats. */
bool Occupied(const FVector2D& Point, float Radius);
/** Farm appearance; optional tree trunk and solid-surface collision trials. No terrain or route edits. */
void Build(AActor* Owner);
/** New screenshot eyes only, from the actual ground plus 130 cm. */
bool DetailView(UWorld* World, FName Name, FVector& Eye, FVector& Target);
}
