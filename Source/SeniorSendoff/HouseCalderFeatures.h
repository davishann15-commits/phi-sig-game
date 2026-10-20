// The Calder landform's built features (SourceArt/CalderLandscape/MASTER_PROMPT.md, steps 1c/1d): the retaining walls,
// rock faces, stairs and ramps the relief needs. The landform (HouseCalderLandform: Walls(), Stairs(), Ramps()) declares
// where the ground steps; the terrain tiles draw each side of a step and leave it open, and this closes it:
//  - WALLS: every declared wall segment, sampled at <= 1 m and at each 2 m lattice line it crosses, the two sides read with
//    HouseExteriorSite::GroundHeight 3 cm either side of the line. A wall 40 cm thick stands on the HIGH side (its low face on
//    the line) from 40 cm under the low ground: mortared stone with a concrete coping stepped in level runs (8 cm over the
//    high ground, or a 100 cm parapet where the drop is 75 cm or more). Where paving (a walk, plaza, apron, road or a stair
//    or ramp) comes within reach of the high side (the body, its overhang and 5 cm), the body stands on the LOW side instead
//    (its high face on the line), and no coping overhangs paving within 15 cm, so nothing stands inside a walk's width.
//    For the rock kinds a battered rock face with
//    value-noise relief and a rock lip, its protrusion in front of the line held 5 cm short of any paving or stair on the
//    low side (Crow Ledge also gets a pipe rail; Founders' Overlook's parapet stands 107 cm over the
//    terrace where its high side is only the strip on the bluff's edge). Class changes along a wall are found on a 20 cm
//    pitch. Gaps: where the step is under 3 cm, in a frozen zone, and only where a stair's tread (or, in the 15 cm beside
//    it, its cheek) or a ramp's landing stands on the wall line at the high ground (to 2 cm), so every opening is closed
//    below by the stair or the landing; a stair or landing that meets a wall lower than that runs into the wall.
//  - STAIRS: 14-18 cm risers on treads of 30 cm or more where an integer count allows both, laid evenly from foot to head;
//    the head goes 15 cm past the last wall the stair crosses (carried on, or drawn back over level ground), and where that
//    wall runs across the stair at a slant the top tread becomes a landing from its nearest to 15 cm past its furthest
//    crossing; the top tread stands on the highest ground it covers (by up to 25 cm). Each step a solid box (3 cm nosing)
//    down to 30 cm under the ground; stone cheek walls wherever the stair stands > 30 cm over the ground beside it (or
//    crosses a wall there); pipe handrails both sides from 4 risers; the Senior Steps' bronze CLASS OF 1936 plaque on its
//    north cheek.
//  - RAMPS: a 180 cm slab from A to B with a level 150 cm landing at B and pipe rails; the landing's unrailed sides and end
//    reach 25 cm further (15 cm past a wall 10 cm off).
// Everything is a runtime static mesh per 128 m tile (no shadows, no distance fields) plus hidden procedural-mesh collision
// (BlockAll, camera ignored); wall and rock faces and rails are Unwalkable. Components: CalderWall_*, CalderStair_*,
// CalderRamp_*. Nothing is built unless the Calder terrain is on and its relief is not flat (zero relief adds nothing).
#pragma once

#include "CoreMinimal.h"

class AActor;

namespace HouseCalderFeatures
{
/** The features exist: the Calder terrain is on (HouseCalderTerrain::Enabled), the relief scale is above 0 and the lattice
 *  is not flat. */
bool Active();
/** Builds every wall, stair and ramp on Owner (call after HouseCalderTerrainTiles::Build). On a dedicated server only the
 *  collision. Returns the drawn triangle count; logs CALDER_FEATURES walls= wallMeters= stairs= steps= ramps= rails= tris= ms=. */
int32 Build(AActor* Owner);
/** P lies inside a built stair's footprint (its treads foot to head, plus the 30 cm cheek band each side), grown by Pad. */
bool InStair(const FVector2D& P, float Pad = 0.f);
/** Every built tread's top: its centre and +-(Width/2 - 25 cm) across, for the layout review's floor traces. */
void StairSamples(TArray<FVector>& Out);
/** P lies inside a built ramp's footprint (its slab, and its landing with the parts that reach onto the high ground), grown
 *  by Pad. */
bool InRamp(const FVector2D& P, float Pad = 0.f);
/** Points on every built ramp's walking surface (each metre up the slab and over the landing; centre and +-65 cm across). */
void RampSamples(TArray<FVector>& Out);
/** The footprints (convex, anticlockwise) the paving under them gives way to: every built stair's (its treads foot to head
 *  with the 3 cm nosing, plus the 30 cm cheek band each side, as InStair) and every ramp's slab and landing. Empty unless
 *  Active(). HouseCalderDrape cuts them out of every draped sheet and slab. */
const TArray<TArray<FVector2D>>& Footprints();
/** The walking surface of a built stair tread or ramp at P (the tread's top, the slab's or landing's), if P lies on one. */
bool FloorAt(const FVector2D& P, float& OutZ);
}
