// The Calder outer land (SourceArt/CalderLandscape, step 1f): the ground beyond the walkable square (+-32000), scenery
// with no collision. An exact port of Saved/CalderWork/plan/landform.py's "THE OUTER LAND (Far)" section (far(), outer()
// and everything they call), in double precision with the same uint32 hash noise, so the game and the prototype agree
// to rounding (Saved/CalderWork/plan/farland_port_check.py writes the expected values; -CalderFarLandSelfTest checks them).
//   - the town's valley: the lower bluff below the campus, Main St about -1900, the river about -2400 at Y -45000;
//   - the near ridge (crest 240-320 m above the river at Y -85000..-100000) with its water gap at X ~60000, the mid and far
//     ridges behind it; the cove walls east and west; Gosling Knob, the broad knob (380 m, 33-deg flanks, a notched
//     shoulder) closing the valley's west end at yaw 205 from the frat lawn; the water tower's knob;
//   - north of the square, the foothills rising from the seam, the Calder Branch notch and Calder Dr's far road.
// Along the seam the outer land equals the walkable ground (HouseExteriorSite::GroundHeight: the 2 m lattice, the game's
// authority) and blends to the outer land over 60 m (25 m below the bluff). Everywhere off the seam it is continuous (no
// step over 5 cm: landform.py check_far_c0): the walls that reach the seam (the bluff's rock band, Calder Dr's rock cut,
// the glen) fan out beyond it, and every form fades in where it starts.
// Units are cm: X east, Y north, Z up. Pure functions, safe from any thread once HouseExteriorSite::GroundHeight is.
#pragma once

#include "CoreMinimal.h"

namespace HouseCalderFarLand
{
/** The walkable square's half size: the seam. */
constexpr double World = 32000.0;
/** The river's nominal level at the bridge (landform.py RIVER_Z); the river falls 0.1 % toward the east. */
constexpr double RiverDatum = -2400.0;

/** Absolute ground height anywhere (landform.py ground()): beyond the walkable square the outer land (far()), inside it
 *  the walkable ground (HouseExteriorSite::GroundHeight). */
double Ground(double X, double Y);
/** The outer land with no seam blend (landform.py outer()). Meaningful beyond the square only. */
double Outer(double X, double Y);
/** The town's valley floor (landform.py town_floor()): the lower bluff from its toe, Main St, the river, the far bank. */
double TownFloor(double X, double Y);
/** The walkable ground at the seam point nearest (X, Y) (landform.py seam_value(): GroundHeight there). */
double SeamValue(double X, double Y);
/** The foothill top along the north seam, continued on the cove walls beyond |X| 32000 (landform.py north_base()): the
 *  base today's north ridge sheets (EX_RidgeNear/Mid/Far) are re-based onto. */
double NorthBase(double X);
/** The Calder Branch valley notch cut through height Z north-east of the square (landform.py ne_notch()); the near
 *  ridge sheet takes it with FloorAdd 900. */
double NeNotch(double X, double Y, double Z, double FloorAdd = 600.0);
/** Today's north ridge sheets re-based on the foothills, the near one notched (landform.py ex_ridges()): the highest sheet
 *  over (X, Y), -1e9 where none is. Uses today's trig. */
double ExRidges(double X, double Y);
/** max(Ground, ExRidges) north of the square (landform.py skyline_ground()). */
double SkylineGround(double X, double Y);
/** The river's water surface at X along the town's river (Y -45000): 40 cm over the river's datum, falling 0.1 % east
 *  as the valley floor does (about 27 cm deep mid-channel, 7-8 m between its edges). */
double RiverSurface(double X);
}
