// Calder State's 1986 street details (master job, step 1d): regulatory and guide signs on the campus roads, a signalised
// mid-block crossing on Fraternity Park Dr (heads glow by emission only, no lights), coin meters at the visitor lot, storm
// inlets and manholes, painted curbs, and a gravel overflow lot off the south lane. Everything is invented and period: no
// real brands, no modern symbols. Built at runtime into the campus plan's own ground mesh; -NoCampusLayout removes it.
#pragma once

#include "CoreMinimal.h"

struct FHallMesh;
class UHouseBlackHallSubsystem;

namespace HouseCampusStreetDetail
{
/** Sign faces, signal lenses, meter and gravel materials (CS_*). Idempotent; does nothing under -NoCampusLayout. */
void CreateLook(UHouseBlackHallSubsystem* Look);
/** Solid: posts, signs, meters, signal poles (collide). Flat: paint, grates, gravel (no shadows). */
void Build(FHallMesh& Solid, FHallMesh& Flat);
}
