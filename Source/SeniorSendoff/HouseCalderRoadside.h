// The Calder landform's roadside (SourceArt/CalderLandscape/MASTER_PROMPT.md, step 1d: "Guardrail" and "Drainage" under
// Grades): what a 1986 county road crew and the campus grounds shop left along the roads and walks the relief made, built
// from the landform's roadside tables (HouseCalderLandform: Guardrails(), Ditches(), NeedsRail(), CatchBasins(),
// DitchCulverts()) on the game's own ground (HouseExteriorSite::GroundHeight). Old, worn and quiet, never new:
//  - GUARDRAIL: galvanised W-beam gone dull (a few older, darker panels; rust streaks under the bolts) on 15 x 20 cm timber
//    posts every 1.905 m with timber blockouts, the beam's top 64 cm over the verge, and turned-down ends: over the last
//    7.62 m the beam drops and twists into the ground to a stained concrete anchor. Posts and the beam collide (Unwalkable);
//    where a turned-down end lies nearly flat it is walkable. An exported run whose drop is a declared wall (bAtWall: the wall's
//    parapet guards it, and its verge is a sidewalk) is not built. Calder Dr north's fill side (Y 20600..24300, west) gets
//    one run for the look, as the master prompt describes the road ("climbs with a rock cut and guardrail");
//    -NoCalderLookGuardrail leaves it out.
//  - RAILS: a 100 cm galvanised pipe rail (rails at 100, 66 and 32 cm: no gap of 36 cm) on an exported "needs rail" drop
//    beside a walk or road. The drops on the new water structures (the arch, the footbridge, the dock, the rock house path)
//    are their builders' to rail and are skipped here.
//  - DITCHES: a cobbled rip-rap lining draped 1.2 cm over the verge between the curb (or the walk) and the cut, never on
//    paving, a stair, a ramp, a landform path, a frozen zone or under a rock face (where a rock cut's foot fills the verge
//    there is no lining), with small mossy rock clusters along it; weep holes (a rusted pipe stub, a dark wet stain under it)
//    in a masonry retaining wall that stands along a ditch.
//  - CATCH BASINS: a cast-iron grate in a stained concrete frame, flush with the road or lot (at most 1.1 cm proud, no
//    collision, no trip edge): level on a flat lot or road (HouseCampusLayout::FlatPaveTop), draped on a draped road; moved
//    inward (up to 1.2 m) until the whole frame lies on the paving.
//  - CULVERTS: at a road ditch's low end, a headwall across the ditch (1930s mortared stone on the old county roads, stained
//    concrete on the campus service roads) with a dark pipe mouth facing up the ditch; where the lining is too narrow for a
//    headwall, a drop inlet (a grate set on the stones); where a rock face leaves no ditch at all, a gutter grate in the road
//    on that side. Across the road, where the open lawn does not rise in front of it and no wall lies between, an outlet
//    headwall with wing walls, its dark mouth and a rip-rap apron with boulders.
// Everything is a runtime static mesh per 128 m tile and draw distance (70 m for the grates, bolts, streaks and weep holes;
// 300 m for the rest), no shadows, no distance fields, plus hidden procedural collision per tile (BlockAll, the camera
// ignores it). Components: CalderRoadside_*. No lights. Nothing in a frozen zone, and nothing at all unless the Calder
// terrain is on with real relief (HouseCalderFeatures::Active), so -NoCalderTerrain gives today's site exactly.
#pragma once

#include "CoreMinimal.h"

class AActor;

namespace HouseCalderRoadside
{
/** Builds the roadside on Owner (call after HouseCalderFeatures::Build). On a dedicated server only the collision. Returns the
 *  drawn triangle count; logs CALDER_ROADSIDE guardrailM= posts= rails= ditches= basins= headwalls= tris= ms= (and details). */
int32 Build(AActor* Owner);
/** A built roadside solid (a guardrail post, its beam, a pipe rail, a headwall or wing, a rock; not the flush grates and the
 *  ditch linings) stands within Pad of P: for scatter builders that must keep their trees, bushes and stones off them. */
bool Occupied(const FVector2D& P, float Pad = 0.f);
/** Where each built standing thing meets the ground (a guardrail or rail post's foot, an anchor, a headwall's toe, a rock's
 *  lowest ground), at that height; the same points are registered with HouseCalderFootPoints for -CalderTerrainReview (the
 *  flush grates and the draped linings are not standing things). Empty before Build. */
void FootPoints(TArray<FVector>& Out);
}
