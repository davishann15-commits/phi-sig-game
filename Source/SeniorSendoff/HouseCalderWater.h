// Calder Branch, Gander Pond and Calder Falls (SourceArt/CalderLandscape/MASTER_PROMPT.md, Phase 1 "Calder Branch, Gander Pond
// and Calder Falls"): the water and the structures that cross and hold it, built from the landform's own data
// (HouseCalderLandform: PondOutline, CreekPoints, GlenPoints, CalderFalls, Cascade, GlenHeadwall, WaterCrossings, PondDam,
// SteppingStones, PondDock, RockHouse, LandformPaths). The ground (the 2 m lattice, HouseExteriorSite::GroundHeight) is the
// authority: the water lies where the ground under it is lower than the water's level.
//  - WATER (M_CalderWater, no collision, no shadows; the beds are the walkable terrain): Gander Pond with its spillway race and
//    the dam's weir notch (one flat sheet at the pond's level), Calder Branch above the pond (its level by latitude, a riffle
//    over the gravel of its mouth), the glen below the dam in three reaches (the outlet pool to the falls' lip, the plunge
//    pool to the cascade, the cascade's foot to the square's edge), each a grid sheet whose edge sinks 3 cm under its bank;
//    the brink over the caprock, the 6.8 m curtain of Calder Falls (bowed, fast, foaming, both faces), the 2 m cascade and the
//    weir's 1.4 m sheet. Vertex colour R is foam (the falls' and the cascade's foot, the weir's foot, riffles, the stones),
//    A the depth.
//  - THE DAM: the whole stone dam (it replaces the generic retaining wall HouseCalderFeatures would build on its declared
//    Dam walls: HouseCalderFeatures skips them while BuildsDamWalls()): a mossy masonry body under a 240 cm crest walk
//    (concrete, 100 cm parapets both sides), the weir notch with its stone sill, and the stone-arch footbridge over it; low
//    stone kerbs close the run-on steps past its ends.
//  - CROSSINGS: the loop's stone causeway over Calder Branch (kerbs, two stone headwalls, a rusted 45 cm pipe with a dark
//    mouth, rip-rap at the outfall); the timber pony-truss footbridge below the falls (P6; plank deck, trusses 112 cm high,
//    timber bents on the dry floor); the stepping stones; the pond's weathered timber dock (a low curb where it is rail-less,
//    a 100 cm timber rail where the drop reaches 75 cm); Ashby Lane's concrete road arch with 107 cm stone parapets.
//  - THE GLEN: the sandstone caprock overhanging the rock house by 3 m (the falls leave its edge), the rock house's rusted
//    pipe rail (100 cm) on the pool side, the stone headwall where the glen leaves the walkable square: a tall dark arch with a
//    rusted iron grate (one bar pushed in, sticks caught on it), its wings to the hedge's ends, every top 2 m over the ground
//    beside it.
//  - Gander Pond's mortared stone edge at the waterline.
// Everything is a runtime static mesh per area and material (no shadows on water), plus hidden procedural-mesh collision
// (BlockAll, camera ignored): decks, treads, sills, stone tops walkable; faces, parapets, rails, rock and anything over 40
// degrees Unwalkable. Components: CalderWater_*, CalderWaterWorks_*. Nothing is built unless the Calder terrain is on and its
// relief is real (HouseCalderFeatures::Active), so -NoCalderTerrain stays today's.
#pragma once

#include "CoreMinimal.h"

class AActor;

namespace HouseCalderWater
{
/** The water and its crossings are built: HouseCalderFeatures::Active() (the Calder terrain on, its relief real). */
bool Active();
/** The stone dam (every EWallKind::Dam wall: its crest, its weir notch and its run-ons) is built here, not as a generic
 *  retaining wall: HouseCalderFeatures must skip Dam walls while this is true (its 100 cm parapet would stand down the middle
 *  of the crest walk and across the weir). */
bool BuildsDamWalls();
/** The road bridge owns its abutment faces while its full deck/body is built. */
bool BuildsRoadBridgeWalls();
/** Builds every water surface and crossing on Owner (call after HouseCalderFeatures::Build). On a dedicated server only the
 *  collision. Returns the drawn triangle count; logs CALDER_WATER surfaces= tris= bridges= stones= rails= ms= ... */
int32 Build(AActor* Owner);
/** The walking surface of a built deck, sill, kerb top or stepping stone at P (the dam's crest and arch, the causeway, the
 *  dock, the timber footbridge, the stones): for reviews and placers that read the floor. False elsewhere and when off. */
bool DeckAt(const FVector2D& P, float& OutZ);
/** P lies under open water this builds (the pond, the race and notch, Calder Branch, the glen; not under a deck), with the
 *  water's surface there: for placers that keep off water. False elsewhere and when off. */
bool WaterAt(const FVector2D& P, float& OutZ);
}
