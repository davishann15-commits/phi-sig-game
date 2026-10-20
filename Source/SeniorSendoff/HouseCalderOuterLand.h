// The Calder outer land's drawn ground (SourceArt/CalderLandscape, step 1f): scenery beyond the walkable square, no
// collision, no shadows. Concentric rings round the square, each a tensor grid of HouseCalderFarLand::Ground:
//   ring 0  +-32000 .. X +-48000, Y -64000 .. +48000      8 m (the town, the river, the lower bluff, the near ridge's
//           foot where the town's houses and trees stand; extra rows across the river so its trough and far bank show;
//           a 4 m patch on the water tower's knob)
//   ring 1  .. X -128000 .. +102400, Y +-102400            16 m (the cove walls, the near ridge's crest, the north
//           foothills; an 8 m patch on Gosling Knob)
//   ring 2  .. X +-320000, Y -320000 .. +153600            64 m (the mid and far ridges to the south, the cove walls'
//           backs; north of the square today's ridge sheets, re-based in HouseExteriorCampus.cpp, make the skyline)
// Each ring's inner edge takes the finer side's vertices exactly (the seam: the walkable tiles' 2 m lattice vertices and
// the wall-split cells' piece vertices, at HouseExteriorSite::GroundHeight), fanned into its own cells, so nothing
// cracks; a 2 m skirt hangs under every ring edge as well. One runtime static mesh per ring side (CalderOuterLand_*),
// M_CalderGround with vertex colour: leaf litter (G) on slopes, the lawn/meadow on the valley floor, rock (R) on the
// steepest. Logs CALDER_OUTERLAND rings= tris= seamMaxGap= ms=.
#pragma once

#include "CoreMinimal.h"

class AActor;

namespace HouseCalderOuterLand
{
/** Builds the outer land on Owner (only when HouseCalderTerrain::Enabled(); nothing on a dedicated server). */
void Build(AActor* Owner);
/** The lowest the drawn ground can stand in the drawing's cell that holds (X, Y): the least ground over every point that
 *  cell's triangles can take (its corners, the river rows through it, its sides at the finer step a ring's inner edge takes
 *  points at); inside the walkable square, HouseExteriorSite::GroundHeight there. (A wall's step on the seam aside.) */
double LowestDrawn(double X, double Y);
/** Seats a sheet of scenery on the Calder ground (today's north ridge sheets, HouseExteriorCampus.cpp Mountains()), Z only.
 *  V is its (NX + 1) x (NY + 1) vertex grid, V[I * (NY + 1) + J], built as quads (I, J) (I + 1, J) (I + 1, J + 1) (I, J + 1)
 *  split on their first diagonal, rows rising from J = 0 (front) to NY (back). Every rim vertex (row 0 or NY, column 0 or
 *  NX) standing above the drawn ground sinks 2 m under it, so no edge of the sheet hangs in the air; and where row 0 dips
 *  inside the walkable square (Y < 32000), it sinks far enough that the sheet stays 2 m under the walkable ground up to the
 *  seam and rises out of the outer land beyond it. */
void SeatSheet(TArray<FVector>& V, int32 NX, int32 NY);
}
