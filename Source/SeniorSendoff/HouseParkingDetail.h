// Lived-in detail for the west parking lot built by HouseParking: oil stains, sealed cracks, patches, tyre polish,
// puddles, worn paint, bay numbers, road text, fallen leaves and litter on the paving (flat, no collision or shadows),
// plus solid props around it (lamp pier bases, bollards, signs, a dumpster on its pad and two roll carts).
#pragma once

#include "CoreMinimal.h"

struct FHallMesh;
class UHouseBlackHallSubsystem;

namespace HouseLotDetail
{
void CreateLook(UHouseBlackHallSubsystem* Look);
/** Wear: flat decals just above the paving (build without collision or shadows). Props: solid pieces (collision, shadows). */
void Build(FHallMesh& Wear, FHallMesh& Props);
}
