// The Calder landform's edges, overlooks and Crow Ledge (SourceArt/CalderLandscape/MASTER_PROMPT.md, step 1e: "Edges and
// containment", "Overlooks"; "How to build it" 3, the Crow Ledge trees). Everything is read from the landform's exported
// tables (HouseCalderLandform: EdgeClosures, Overlooks, LandformPaths, CrowLedge, Walls, Stairs) and seated on the game's
// own ground (HouseExteriorSite::GroundHeight):
//  - EDGE CLOSURES: where the walkable ground at the square's edge stands more than 3 m off today's grade, a rock band 3 m in
//    from the edge (X +-31700, Y 31700) the player can't climb and never an invisible wall: a weathered sandstone and shale
//    outcrop (M_CalderRock), bedded in level courses (the sandstone standing out, the shale weathered back), jointed,
//    leaning back a little; from 40 cm under the lowest ground under it to 2.1-4.1 m over the highest, so there is no gap
//    under it and no way over it; faces and top Unwalkable. Across Calder Dr it is a rockslide (fresh broken rock, blocks
//    spilled onto the road), over Calder Branch a boulder choke (the water runs out between mossy boulders). Bands that meet
//    at a corner run on past it; a free end turns out to the world edge (a short return), so the 3 m strip behind a band
//    can't be walked into from its end.
//  - FOUNDERS' OVERLOOK: a 1930s panorama table (a dark, verdigris-streaked bronze plate on a dressed sandstone pedestal in
//    front of the parapet): FOUNDERS' OVERLOOK, ERECTED BY THE CLASS OF 1931, the skyline as it is seen from the overlook
//    (raised, computed from the outer land) and the invented peaks named at their true bearings (GOSLING KNOB, WIDOWS NOTCH,
//    LANTERN KNOB, PALE RIDGE, COLDHEARTH GAP; CALDER below), in raised stroke letters; and a coin-operated binocular viewer
//    with no brand (heavy painted steel on a pipe post, the paint chipped to rust).
//  - THE POND OVERLOOK: an old park bench (cast-iron ends, silvered slats, one back slat split) beside the loop, facing the
//    water, with a small bronze CLASS OF 1958 plate.
//  - CROW LEDGE: sandstone flags on the ledge's floor (scenery, 2 cm, no collision); a weathered timber post-and-rail fence
//    (rails 100 / 62 / 28 cm) along the trail's downhill cut wall wherever it drops 75 cm or more; woods trees (the exterior's
//    oaks and maples, trunk collision) round the trail and behind the ledge, clear of the P8 view, where the distant forest
//    is hidden (HidesForestDistant: the exterior skips those instances after RecordOriginalTrees, so the crc is unchanged).
// Everything is a runtime static mesh per area (the rock bands per 128 m tile) with no shadows, no distance fields and a cull
// distance, plus hidden procedural-mesh collision (BlockAll, camera ignored). No lights. Components: CalderEdgeRock_*,
// CalderOverlook, CalderPondBench, CalderCrowLedge, CalderCrowTree*. Nothing is built unless HouseCalderFeatures::Active()
// (the Calder terrain on and its relief real), so -NoCalderTerrain is today's exactly.
#pragma once

#include "CoreMinimal.h"

class AActor;
class UHouseBlackHallSubsystem;

namespace HouseCalderEdges
{
/** The edges are built: HouseCalderFeatures::Active(). */
bool Active();
/** Builds every edge closure, the overlook furniture, the pond bench and Crow Ledge on Owner (call after
 *  HouseCalderFeatures::Build). Look: the exterior's material subsystem (the woods trees' materials). ForestDistant: the
 *  site's distant-forest transforms (only counted here: how many HidesForestDistant hides). On a dedicated server only the
 *  collision (and the trees, whose instances carry their trunks). Returns the drawn triangle count; logs
 *  CALDER_EDGES segments= rockM= hedgeSegs= hiddenForestDistant= plantedTrees= ms= (and the details). */
int32 Build(AActor* Owner, UHouseBlackHallSubsystem* Look, const TArray<FTransform>& ForestDistant);
/** A distant-forest tree (ExteriorForestDistant*) standing at At is hidden: it is within 40 m (in plan) of the Crow Ledge
 *  trail or the ledge, or in the P8 view south from the ledge between it and the old property's north edge (Y 19800).
 *  False unless Active(). */
bool HidesForestDistant(const FVector& At);
}
