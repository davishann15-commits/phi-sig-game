// The Calder landform (SourceArt/CalderLandscape/PLAN.md): designed and checked in Python (Saved/CalderWork/plan/
// landform.py, the authority) and baked by Saved/CalderWork/tools/bake_landform.py into HouseCalderLandformData.inl, the
// same data on every machine: the 2 m relief lattice, the pieces a declared wall splits a lattice cell into (so retaining
// walls are true steps), the building pads, the tower lift, the walls, stairs and ramps, the frozen rectangles, the site
// trees the landform hides and the reserved mystery spots. HouseCalderTerrain reads it and gates it (-NoCalderTerrain);
// the features (walls, stairs, ramps) are built from it by HouseCalderFeatures.
#pragma once

#include "CoreMinimal.h"

namespace HouseCalderLandform
{
/** Relief at lattice corner (I, J) of the 2 m lattice over +-32000 (cm above today's grade). */
float LatticeRelief(int32 I, int32 J);
/** The convex pieces (anticlockwise; X, Y, relief) a wall splits lattice cell (I, J) into; null for a cell no wall crosses.
 *  A piece's relief inside it is its vertex-0 fan, triangle by triangle. */
const TArray<TArray<FVector>>* WallPieces(int32 I, int32 J);
/** Relief beyond the walkable square: the outer land (HouseCalderFarLand::Ground) over today's grade there; 0 inside. */
double Relief(double X, double Y);
/** How deep the landform levels the entrance apron on face Face of block Id (out from the face, cm): the layout's own depth,
 *  or less where the landform clipped it for a stair (APRON_CLIP). False when it has none there. */
bool ApronDepth(const TCHAR* Id, TCHAR Face, float& OutDepth);
/** A plan building's pad above -60 (by seed), the tower's knoll lift. */
double PadDZ(int32 Seed);
double TowerLift();
/** Beyond +-32000: the outer land's ground where the town stands (HouseCalderFarLand::Ground; the walkable ground
 *  inside), and the north ridges' base (HouseCalderFarLand::NorthBase(X)). Absolute heights. */
double Valley(double X, double Y);
double RidgeBase(double X, double Y);
/** The frozen rectangles (the landform's zero rectangles: the frozen zones and the street wall's verge). */
const TArray<FBox2D>& FrozenRects();

/** Kinds of declared wall. */
enum class EWallKind : uint8 { Wall, RockBand, RockCut, RockLedge, Dam, Parapet, PadCourt, Terrace, Lot, Apron, Reserve, RoadAbutment };
struct FWall { FString Name; EWallKind Kind; FVector2D A, B; };
const TArray<FWall>& Walls();
/** A stair or flight: head (top) and foot (bottom) in XY with their relief, and its width. */
struct FStair { FString Name; FVector2D Head, Foot; float HeadRelief, FootRelief, Width; };
const TArray<FStair>& Stairs();
/** A railed ramp: its run from A (relief Z0) to B (relief Z1). */
struct FRamp { FString Name; FVector2D A, B; float Z0, Z1; };
const TArray<FRamp>& Ramps();
/** A reserved level spot (scenery only): centre, size, relief. */
struct FReserve { FString Name; FVector2D Centre, Size; float Relief; };
const TArray<FReserve>& Reserves();
/** A site tree (TreeA / TreeB) at this spot is hidden by the landform (it would stand on a wall, a stair, a ramp, in water,
 *  on a reserve or in a lot tier). */
bool TreeHidden(const FVector2D& At);

// ---- The builders' data: designed and computed by landform.py exports() on the baked ground, so data and ground agree
// (Saved/CalderWork/plan/data_exports.md describes every table). Heights are ABSOLUTE (UE Z, cm: today's grade + relief).
// They hold with the Calder terrain on at relief scale 1: every builder gates on HouseCalderTerrain::Enabled() /
// HouseCalderFeatures::Active(), so -NoCalderTerrain stays today's.

/** Gander Pond: its water's edge (closed polygon) and its surface. */
const TArray<FVector2D>& PondOutline();
float PondWaterZ();
/** A point on a water centreline: the water surface, the water's half-width and depth at the centre there, and the arc
 *  length from the line's start. Water levels go by latitude: interpolate by Y between points. */
struct FWaterPoint { FVector2D At; float WaterZ, HalfWidth, Depth, S; };
/** Calder Branch above the pond: its centreline vertices, north edge (29400, 32000) to the pond inlet; the channel fans into
 *  the pond's shore over its last 8 m. */
const TArray<FWaterPoint>& CreekPoints();
/** The glen below the dam: its centreline every 1 m to the square's south edge, plus points 5 mm either side of the falls
 *  and the cascade (the water steps there) and at the headwall. */
const TArray<FWaterPoint>& GlenPoints();
/** A ledge the water drops over: the lip (the water's width just upstream), the water above and below, the declared rock
 *  wall's extent, the line the curtain leaves (the falls: the caprock overhang's edge, 3 m out; the cascade: its own line),
 *  and the flow direction. */
struct FWaterLedge { FString Name; FVector2D LipA, LipB; float TopZ, BottomZ; FVector2D WallA, WallB; float CurtainY; FVector2D Flow; };
const FWaterLedge& CalderFalls();
const FWaterLedge& Cascade();
/** The plunge pool's deep water (1.1 m), clear of the rock house shelf: a disc, and the latitude where it is deep from. */
struct FPlungePool { FVector2D Centre; float Radius, WaterZ, Depth, DeepFromY; };
const FPlungePool& PlungePool();
/** The stone headwall with its grate where the glen leaves the walkable square (on the campus hedge line, Y -29800): its
 *  wing walls run across the glen floor from WingWestX to WingEastX (the hedge stops there). */
struct FGlenHeadwall { FVector2D Centre, Flow; float WaterZ, InvertZ, WaterHalfWidth, FloorHalfWidth, WingWestX, WingEastX, GrateWidth; };
const FGlenHeadwall& GlenHeadwall();
/** Every built path or road crossing of the water: the structure runs
 *  Back and On along Axis from Centre (the pipe, the notch, the stream's line), its deck at DeckZ, its ends' ground EndZ0/1. */
enum class EWaterCrossing : uint8 { CausewayCulvert, ArchFootbridge, TimberFootbridge, RoadBridge };
struct FWaterCrossing { FString Name; EWaterCrossing Kind; FVector2D Centre, Axis; float Back, On, Width, DeckZ, WaterZ, InvertZ, EndZ0, EndZ1, PipeDiameter; };
const TArray<FWaterCrossing>& WaterCrossings();
/** The stone dam (its crest walk, the weir notch the arch spans, the outlet pool below) and the spillway race (the pond's
 *  water from its south edge to the notch). */
struct FPondDam { TArray<FVector2D> Crest; float CrestZ, CrestWidth, NotchWidth, OutletZ; TArray<FVector2D> Race; float RaceWaterZ, RaceHalfWidth, RaceFloorHalfWidth; };
const FPondDam& PondDam();
/** Stepping stones (crossing index into SteppingStoneCrossings()): top 8-15 cm over the water, set on the bed. */
struct FSteppingStone { int32 Crossing; FVector2D At; float TopZ, BedZ, Radius, Yaw; };
const TArray<FSteppingStone>& SteppingStones();
const TArray<FString>& SteppingStoneCrossings();
/** The timber dock off the loop: from its landward end (the shore at deck level) over the water's edge to its tip. */
struct FPondDock { FVector2D Land, Edge, Tip, Axis; float Width, DeckZ, WaterZ, TipDepth, RailFromEdge; };
const FPondDock& PondDock();
/** The rock house behind Calder Falls: the shelf (FaceY..ShelfY, X0..X1, at FloorZ) under the caprock overhang (to
 *  OverhangY), the water from WetY; the path PathA-PathB, railed RailA-RailB on the pool side. */
struct FRockHouse { float X0, X1, FaceY, OverhangY, ShelfY, WetY, FloorZ, PoolZ; FVector2D PathA, PathB, RailA, RailB; };
const FRockHouse& RockHouse();
/** Landform paths the layout does not have (the drape paves their Ground stretches; the rest are structures). */
enum class EPathStretch : uint8 { Ground, DamCrest, ArchFootbridge, Causeway };
struct FLandformPath { FString Name; TArray<FVector2D> Points; TArray<EPathStretch> Stretches; float Width; bool bClosed; };
const TArray<FLandformPath>& LandformPaths();
/** Authored asphalt approaches and a turnaround. Mode 0 joins Ashby Lane and the bridge, 1 joins the bridge at its start,
 *  2 is the level turnaround. Polygons are convex paving pieces; heights follow the baked ground. */
struct FRoad { FString Name; TArray<FVector2D> Points; TArray<TArray<FVector2D>> Polygons; float Width; int32 Mode; };
const TArray<FRoad>& Roads();
/** Crow Ledge: its outline and level (its trail is a LandformPaths() entry). */
struct FCrowLedge { TArray<FVector2D> Outline; float LevelZ; };
const FCrowLedge& CrowLedge();

/** Roadside runs: Side +1 left of the run's direction, -1 right, 0 across its end; Value the drop (or the cut's rise), cm;
 *  bAtWall: the drop (or the cut) is a declared wall (its parapet guards it, its weep holes drain it); bWalk: a walk's ditch. */
enum class ERoadsideRun : uint8 { Guardrail, Ditch, NeedsRail };
struct FRoadsideRun { FString Name; ERoadsideRun Kind; int32 Side; float Value; bool bAtWall, bWalk; TArray<FVector2D> Points; };
const TArray<FRoadsideRun>& Guardrails();
const TArray<FRoadsideRun>& Ditches();
const TArray<FRoadsideRun>& NeedsRail();
struct FCatchBasin { FString Name; FVector At; bool bLot; };
const TArray<FCatchBasin>& CatchBasins();
/** A pipe under a road at a ditch's low end (stone headwalls and rip-rap at both ends). */
struct FDitchCulvert { FString Ditch, Under; FVector2D Centre, Axis; float Length, RoadZ; int32 Side; };
const TArray<FDitchCulvert>& DitchCulverts();

/** Closures 3 m in from the square's edge wherever its ground stands more than 3 m off today's grade (the campus side is
 *  closed by its hedges). */
enum class EEdgeClosure : uint8 { RockBand, Rockslide, BoulderChoke };
struct FEdgeClosure { FString Edge; EEdgeClosure Kind; TArray<FVector2D> Points; };
const TArray<FEdgeClosure>& EdgeClosures();
/** Today's boundary hedges on the relief, <= 2 m segments: bottom 20 cm under the lowest ground, top 190 cm over the highest
 *  (bFrozen: wholly in a frozen zone, build today's). The gaps (From..To along the hedge's own axis) have no hedge. */
struct FHedgeSegment { int32 Hedge; FVector2D A, B; float BottomZ, TopZ; bool bFrozen; };
const TArray<FHedgeSegment>& HedgeSegments();
const TArray<FString>& HedgeNames();
struct FHedgeGap { int32 Hedge; float From, To; FString Why; };
const TArray<FHedgeGap>& HedgeGaps();

/** Spots: overlooks (At.Z the item's own height: the plaque's centre, the viewer's base, the standing ground) and the eye
 *  spots P1-P8 (At.Z the eye, 170 cm over the ground or the deck). */
struct FSpot { FString Name; FVector At; float Yaw, GroundZ; };
const TArray<FSpot>& Overlooks();
const TArray<FSpot>& EyeSpots();
/** The lowest walkable ground in the square (outside footprints) and where; the fall limit and the rule above it. */
float LowestWalkableZ();
FVector2D LowestWalkableAt();
float FallLimitZ();
float MinWalkableZ();
}
