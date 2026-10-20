// The Calder State campus plan (master job, Phase 1): which buildings stand where and which way they face, the campus
// roads (Calder Dr, its curve and Stadium Dr with the bus loop), the lots, the walk network, plazas and the entrance
// gateposts. Everything is data plus builders; BuildCampus and the exterior read it. -NoCampusLayout gives back the old
// grid of 16 boxes, the side street through the campus and the health-centre lot.
//
// The network is checked offline by SourceArt/CampusMaster/plan/net.py and in game by -CampusLayoutReview.
//
// Tree planting never sees this plan: the old footprints, walk bands and lot NoGo stay frozen as reservations (see
// SourceArt/CampusMaster/PLAN.md §7), so WOODS_PASS_ORIGINAL_TREES keeps crc 0f3b2bb7. A tree that ends up inside new
// geometry is hidden when the tree batches are built (TreeHidden), never moved.
#pragma once

#include "CoreMinimal.h"

struct FHallMesh;
struct FHallUV;
class UHouseBlackHallSubsystem;
namespace HouseCalderTerrain { enum class EPave : uint8; }

namespace HouseCampusLayout
{
bool Enabled();

enum class EKind : uint8 { Library, Admin, Dorm, Health, Dining, Science, Humanities, Union, Classroom, Athletics, Arts, Music, Engineering, Plant, Alumni };

/** One campus building. Faces lists its entrances, main one first: N = +Y (toward the street), S = -Y, E = +X, W = -X. */
struct FBlock
{
    const TCHAR* Id;
    const TCHAR* Name;
    EKind Kind;
    TCHAR Tier;
    float X0, X1, Y0, Y1, Height;
    int32 Style;          // 0 brick, 1 limestone, 2 warm brick
    const TCHAR* Faces;
    FBox2D Rect() const { return FBox2D(FVector2D(X0, Y0), FVector2D(X1, Y1)); }
    FVector2D Centre() const { return FVector2D((X0 + X1) * .5f, (Y0 + Y1) * .5f); }
};
const TArray<FBlock>& Blocks();
int32 Find(const TCHAR* Id);
/** How far building Seed's level pad stands above today's (-60) grade (the Calder terrain; 0 with it off), and the
 *  building's frame: its footprint centre on the pad, turned so its main entrance faces +Y. Every builder that places a
 *  building's canonical-frame geometry uses this one frame. */
float BlockDZ(int32 Seed);
FTransform Frame(int32 Seed);
/** What is laid at this spot of the campus plan: a road or lot (islands excepted), an entrance apron, a walk, sidewalk or
 *  plaza, or lawn. HouseCalderTerrain::SurfaceZ adds its height over the ground. */
HouseCalderTerrain::EPave PaveKindAt(const FVector2D& P);
/** Some paving of the plan (a road or lot, an entrance apron, a walk, sidewalk or plaza) lies within Pad of P: a cheap test
 *  before probing PaveKindAt closely. */
bool PavingNear(const FVector2D& P, float Pad);
/** By build seed (the index into Blocks()). */
bool IsDorm(int32 Seed);
/** The material key of a building's carved name sign. */
FName SignKey(int32 Seed);
/** Yaw that turns a building's canonical frame (main entrance on +Y) to face Face (N/S/E/W). */
float FaceYaw(TCHAR Face);
FVector FaceNormal(TCHAR Face);

/** Sign textures and the few materials the plan adds. Idempotent. */
void CreateLook(UHouseBlackHallSubsystem* Look);
/** Campus roads, lots, walks, plazas, curbs, crosswalks and the entrance gateposts (collidable ground, its own mesh).
 *  Roads and lots sit at street level with the terrain cut away under them (TerrainCuts); round the curved roads the
 *  lawn is rebuilt here in the terrain's own grass material and mapping. */
void BuildGround(FHallMesh& M, FName GrassKey, const FHallUV& GrassUV);
/** Rectangles the exterior's terrain must leave out under the plan (roads, lots, ramps, and the squares round the curves). */
void TerrainCuts(TArray<FBox2D>& Out);
/** X ranges where Fraternity Park Dr's far sidewalk and kerb are left out, because a campus drive meets the street there. */
void FarSidewalkGaps(TArray<FVector2D>& Out);
/** The collision floor (hidden) under homecoming's own drawn gate path and kiosk plaza. */
void BuildFallFloors(FHallMesh& M);
/** Something of the plan (a building, road, lot, sidewalk, walk or plaza) is within Pad of this point. */
bool Occupied(const FVector2D& P, float Pad);
/** Walkway lamps (mercury heads, no lights) and benches along the walk network. */
void LampPoints(TArray<FVector2D>& Out);
void BenchPoints(TArray<FTransform>& Out);

/** A tree at this spot would stand in a building, road or lot of the new plan (lot tree islands excepted), or its trunk,
 *  reaching TrunkReach round its centre, would touch one or a walk. */
bool TreeHidden(const FVector& At, float TrunkReach = 0.f);
/** How deep the concrete apron in front of an entrance on Face of building Seed may run (up to MaxDepth, Width wide) before
 *  it would lap over paving that does not lead to that door (a road, ramp, sidewalk, plaza or passing walk); 0 = none. */
float ApronDepth(int32 Seed, TCHAR Face, float Width, float MaxDepth);
/** The entrance aprons as built (main 10.4 m wide, side 8 m), in the world, and their top height. */
struct FApron { FBox2D R; float Top; };
void EntranceAprons(TArray<FApron>& Out);
constexpr float MainApronW = 1040.f, MainApronD = 550.f, SideApronW = 800.f, SideApronD = 420.f, ApronTop = -55.f;
/** A collision footprint (a trunk's) overlaps a building, a traffic lane or a walk; What names which. */
bool FootprintClash(const FBox2D& Box, FString& What);
/** Paving height of a campus lot or campus road at this spot, if any (HouseCalderTerrain::SurfaceZ, road: the ground there
 *  less 12 cm). */
bool PaveZ(const FVector& At, float& OutZ);
/** Where the campus road or lot under P is built flat on the Calder relief rather than draped, its top: a lot is level by
 *  design, and a road rect across a frozen zone's edge keeps today's literal height. What is fitted into that paving (inlets,
 *  manholes, a stop bar) sits level on it. False where the road there drapes (its top is PaveZ), and off the plan's paving. */
bool FlatPaveTop(const FVector2D& P, float& OutTop);
/** Moves the cars that the old health-centre lot rows produced into stalls of the new lots (their RNG draws are kept). */
void RemapLotCars(TArray<FTransform>& A, TArray<FTransform>& B, TArray<FTransform>& C);
/** Where the three campus lot poles (and so their lights) stand now: X, Y, yaw. */
void LotPoles(TArray<FVector>& Out);
/** -CampusLayoutReview's data checks (HouseCampusLayoutReview.cpp adds the in-world ones). Problems lists every violation. */
struct FPlanReport
{
    int32 Buildings = 0, Entrances = 0, Lots = 0, Roads = 0, PedestrianCells = 0, Components = 0;
    int32 Overlaps = 0, EntrancesOff = 0, LotsUnreached = 0, WalkOnLane = 0, DeadEnds = 0, PropsOnLane = 0, BlankWalls = 0;
    TArray<FString> Problems;
};
void CheckPlan(FPlanReport& Out);
/** A carriageway, drive, lane, yard, loop or lot surface (lot tree islands are not). */
bool OnTrafficLane(const FVector2D& P);
/** Points along every built walk, plaza and flat sidewalk at their top height, for floor traces (about every 2 m): walk
 *  height over the ground there (HouseCalderTerrain::SurfaceZ), down the curb ramps as they are built, the tower plaza on
 *  its knoll, and an entrance apron's own top (ApronTop + the building's pad). */
void WalkSamples(TArray<FVector>& Out);
/** The plan's own visible walks as centre lines (A to B, W wide), its flat sidewalks and its plazas (rectangles and discs:
 *  X, Y, radius), all with their top at walk height (WalkTopZ over today's flat grade; HouseCalderTerrain::SurfaceZ on the
 *  Calder terrain, but WalkTopZ + HouseClockTowerSite::Lift() for the tower plaza disc): read by the exterior polish to joint
 *  and crack them. */
struct FWalkStrip { FVector2D A, B; float W; };
void WalkStrips(TArray<FWalkStrip>& Out);
void PavedRects(TArray<FBox2D>& Out);
void PavedDiscs(TArray<FVector>& Out);
/** A PavedRects rectangle built flat rather than draped on the Calder relief, and its top: a plaza (level by design), or a
 *  sidewalk across a frozen zone's edge (kept at today's literal height). Its joints lie level on it. False for a sidewalk
 *  that drapes (its top HouseCalderTerrain::SurfaceZ, walk class, at every point). */
bool PavedRectFlatTop(const FBox2D& R, float& OutTop);
/** Today's flat walk datum (3 cm over the -60 grade); the walk's real top is HouseCalderTerrain::SurfaceZ (walk class). */
constexpr float WalkTopZ = -57.f;
/** The old side-street ground south of the junction is replaced by campus roads. */
constexpr float JunctionSouthY = -5550.f;
}
