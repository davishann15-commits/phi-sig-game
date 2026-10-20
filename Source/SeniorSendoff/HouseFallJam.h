// The morning-after of the FALL HARVEST JAM (feat. Project Jaystorm) over the whole east lawn (x 4050..8400, y -3660..-100):
// a plywood stage on scaffold with the backline still on it (drums, amps, guitars, a banjo, a keyboard, mics, the PA), wooden
// snow fence on T-posts round the lawn with a gate toward the drive and a wide hayride gate toward the street, sections down,
// steel crowd barricades, hay-bale seating round a dead fire and across the field, the hayride (tractor and wagon), chemical
// toilets, the keg tank, a pop-up canopy, the generator, corn shocks, pumpkins, a scarecrow, the cider barrel, string lights on
// an inner and an outer ring of poles (one run down), and the litter of a long night. The police cruisers round the fence are
// HouseClassicCars' Cruiser type, placed by HouseExterior. Everything is original geometry painted in code; no brands. Adds no
// lights (the bulbs are emissive only).
#pragma once

#include "CoreMinimal.h"

struct FHallMesh;
class UHouseBlackHallSubsystem;

namespace HouseFallJam
{
/** Materials (FJ_*). Runs after HouseStreetScene::CreateLook (reuses its cups, cans and chair webbing). */
void CreateLook(UHouseBlackHallSubsystem* Look);
/** Solid: collision and shadows. Litter: neither. Glow: the emissive bulbs and embers (no collision, no shadows). */
void Build(FHallMesh& Solid, FHallMesh& Litter, FHallMesh& Glow);
/** The field itself: trees are kept out of it (the tree grid still draws the same random numbers there). */
bool Inside(float X, float Y);
}
