// The Calder terrain's own ground (SourceArt/CalderLandscape, step 1b): outside the frozen zones the exterior's flat grass
// patches (ExPatch) give way to tiles that follow HouseExteriorSite::GroundHeight exactly. Each region (one of today's
// terrain pieces there: roads, lots, the house and the building footprints already cut out) is cut along the 2 m lattice's
// lines and each cell's (I,J)-(I+1,J+1) diagonal, so every triangle lies on one lattice triangle and the drawn ground, its
// collision and every placer read the same surface. Tiles are 128 m squares: a runtime static mesh each (M_CalderGround,
// vertex colour = the material's layer masks), plus a hidden procedural mesh for collision, cooked asynchronously;
// HouseCalderTerrain::CollisionReady turns true once every tile's body exists.
#pragma once

#include "CoreMinimal.h"

class AActor;

namespace HouseCalderTerrainTiles
{
constexpr float TileSize = 12800.f;
/** Builds the tiles on Owner (components named CalderTerrainTile_* / CalderTerrainCollision_*). On a dedicated server only
 *  the collision. Returns the drawn triangle count. */
int32 Build(AActor* Owner, const TArray<FBox2D>& Regions);
/** The owned appearance material was loaded for this exterior. Lawn overlays
 *  may yield only outside frozen rectangles; false on the comparison path. */
bool UsesLandscapeGround();
}
