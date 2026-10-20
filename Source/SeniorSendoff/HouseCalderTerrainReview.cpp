#include "HouseFraternityRow.h"
// -CalderTerrainReview (the Calder terrain job: SourceArt/CalderLandscape/MASTER_PROMPT.md, "Verification and deliverables";
// design notes in Saved/CalderWork/plan/terrainreview_design.md): the terrain in the world, once its collision is ready. It
// runs in stages, one per frame, so no frame stalls for long, and each stage logs its own CALDER_* line:
//  1. the base checks (as they were):
//     - accuracy: at 20,000 seeded points outside the frozen zones, the building footprints, the road and lot cuts and the
//       woods, the tile collision under the point must exist (no holes) and match HouseExteriorSite::GroundHeight to 3 cm;
//     - the frozen zones: a 10 m grid traced against today's ground mesh (ExteriorGrounds) is written to frozen_grid.csv, so
//       a run can be compared with a -NoCalderTerrain run (SourceArt/CalderLandscape: within 0.5 cm);
//     - the counts: tiles drawn, collision parts, the lattice hash, and the point and spot lights outside the house.
//  2. CALDER_SKYLINE, the eye-level proof (landform.py check_eye / check_eyes / ray_layers / walkable_frame_peak, ported):
//     from each eye spot P1-P8 (HouseCalderLandform::EyeSpots: 170 cm over GroundHeight, or over the deck at P6 and P7) the
//     whole ground with no buildings (HouseCalderFarLand::SkylineGround: GroundHeight inside the square, the outer land and
//     the north ridges beyond it) is marched every 5 deg of azimuth out to 30 km. P1-P4, P6 and P7 must show a median of
//     8 deg or more; 3 deg or more on every azimuth but the declared notches (the Calder Branch valley, the water gap), where
//     a lower azimuth must show 3 or more ridge layers; 14 deg or more over the south sector (yaw 240-300); a ground range of
//     8 m or more within 150 m outside the frozen zones; and the spots' own rules (P1: 6 deg up its facing yaw; P2: 3 ridge
//     layers on a bearing of its frame and walkable ground 5 deg up in it; P6: both glen walls 15 m over the eye).
//  3. CALDER_CRACKS: across every tile edge and every edge of the pieces a declared wall splits a lattice cell into (bar the
//     edges on a declared wall), read 2.5 mm either side, neither GroundHeight nor the tile collision steps over 3 cm.
//  4. CALDER_SLOPES: what the player would stand on, traced from above on a 2 m grid and in bands across every declared wall:
//     a Calder part's face steeper than 40 deg is Unwalkable (its walkable-slope override).
//  5. CALDER_RAILS / CALDER_GAPS / CALDER_STAIRS: where a declared wall drops 75 cm or more with a walk, stair, ramp, road,
//     lot, landform path or overlook within 1 m of its top, something stands 90 cm or more over the top (horizontal rays at
//     89.5-130 cm across the line); the same beside the built stairs and ramps and along the exported NeedsRail runs; every
//     built wall closes its step (a ray across its line from the low side hits it; a face standing further out, a battered
//     rock foot, only where its foot meets the ground); no boundary hedge segment leaves a gap
//     of 36 cm under it; the built stairs, measured along their axes, have risers of 14-18 cm and goings and top treads of
//     30 cm or more; the ramps are 1:12 or flatter.
//  6. CALDER_LEVELS (the lowest walkable ground against the fall limit: the first PlayerStart's Z - 1200, + 400),
//     CALDER_SIGHTLINE (the frat lawn (2400,-3750,140) to the raised dial: over the ground, under every plan building's top,
//     through no tree canopy's bounds but a frozen tree's that today's line to the unlifted dial crosses too, listed),
//     CALDER_WATER_REVIEW (landform.py check_water ported to the game's ground, plus every
//     depth and bank of the creek, the race and the glen), CALDER_FOOTPOINTS (HouseCalderFootPoints: every registered foot
//     within -10..+3 cm of its surface, the worst 20 listed) and CALDER_WORLD (point and spot lights outside the house <= 53,
//     the tree crc read back from this run's log, the terrain hash, the time to collision-ready).
// With -NoCalderTerrain the terrain stages report SKIP; the levels, foot points and world checks still run.
// Logs CALDER_TERRAIN_REVIEW PASS/FAIL (its first fields as before, every stage's verdict after them) and writes
// Saved/CalderTerrainReview/result.txt (deleted first), with every stage line in review.txt and the details in skyline.csv
// and details.csv. It exits the game on a failure, and on a pass when no other self-exiting review or capture shares the run.
#if !UE_BUILD_SHIPPING
#include "HouseCalderFarLand.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderWater.h"
#include "HouseCalderFootPoints.h"
#include "HouseCalderLandform.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderTerrainTiles.h"
#include "HouseCampusLayout.h"
#include "HouseClockTower.h"
#include "HouseClockTowerMesh.h"
#include "HouseExterior.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseWoodsDetail.h"
#include "Async/ParallelFor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LocalLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformOutputDevices.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"

namespace HouseCalderTerrainReviewImpl
{
using FWaterPts = TArray<HouseCalderLandform::FWaterPoint>;

// ------------------------------------------------------------------ rules and constants
constexpr int32 PerSide = int32(2 * HouseCalderTerrain::Half / HouseCalderTerrainTiles::TileSize);
constexpr double SquareHalf = 32000.0;              // landform.py WORLD: the walkable square
constexpr double SkyStep = 5.0;                     // check_eye: every 5 deg of azimuth ...
constexpr int32 SkyCount = 72;
constexpr double SkyMax = 3000000.0;                // ... out to 30 km ...
constexpr double SkyStop = 400000.0;                // ... or the end of the outer land
constexpr double EyeHeight = 170.0;
constexpr double DesignFloor = -690.0;              // landform.py DESIGN_FLOOR: the glen floor's design minimum
constexpr float CrackLimit = 3.f, CrackEps = .25f;  // a step over 3 cm across a non-wall edge is a crack; read 2.5 mm either side
constexpr float SlopeLimitDeg = 40.f;               // anything steeper must be Unwalkable
constexpr float TerrainSlopeDeg = 33.690068f;       // 1:1.5, the tiles' own limit (reported)
constexpr float PlayerWalkableDeg = 44.765f;        // the engine default the player keeps
constexpr float RailDrop = 75.f, RailHeight = 90.f, RailTop = 130.f, RailPitch = 2.5f;
constexpr float MinWallStep = 3.f;                  // HouseCalderFeatures builds a wall wherever the step is 3 cm or more
constexpr float RiserMin = 14.f, RiserMax = 18.f, TreadMin = 30.f, DimTol = .05f;
constexpr float RampMaxGrade = 1.f / 12.f, GradeTol = .0005f;
constexpr float FootLift = .6f;                     // HouseCalderFeatures' TreadLift: treads stand 6 mm over the walk they meet
constexpr float HedgeGap = 36.f;                    // the crawl capsule
int32 MaxLights() { return HouseFraternityRow::ExteriorLightBudget(); }
constexpr int32 FinalStage = 10;                   // the stages: base, skyline, cracks, 4 slope-grid quarters, wall bands, rails, stairs, final
const TCHAR* const ExpectedTreeCrc = TEXT("0f3b2bb7");
const FVector SightEye(2400.0, -3750.0, 140.0);     // the frat-lawn eye of the tower sight line
constexpr double CanopyBase = .3;                   // a tree's canopy bounds: its mesh bounds above the lowest 30 % (the clear trunk)

// ------------------------------------------------------------------ state
/** A stage's verdict: 1 pass, 0 fail, -1 skipped (the terrain is off). */
const TCHAR* Verdict(int32 V) { return V < 0 ? TEXT("SKIP") : V ? TEXT("PASS") : TEXT("FAIL"); }

struct FState
{
    TWeakObjectPtr<UWorld> World;
    double Start = 0.0, ReadyAt = -1.0, Began = 0.0;
    int32 Stage = 0;
    bool bDone = false;
    int32 Base = 1, Skyline = -1, Cracks = -1, Slopes = -1, Rails = -1, Gaps = -1, StairsV = -1, Levels = -1, Sight = -1, WaterV = -1, Feet = -1, WorldV = -1;
    FString BaseLine, Crc;
    float FallLimit = 0.f, Lowest = 0.f;
    int32 Lights = 0, FootPoints = 0;
    // The slope sweep's running totals (the grid and the wall bands run in two frames).
    int32 SlopeSamples = 0, SlopeHits = 0, SlopeOther = 0, SlopeSteep = 0, SlopeBad = 0, SlopeWalkable = 0, SlopeDown = 0, TerrainOver = 0;
    float TerrainMax = 0.f;
    TArray<FString> SlopeWorst;
    TArray<FString> Lines;
    FString Skycsv, Details;
};
FState& State() { static FState S; return S; }

void Say(FState& S, const FString& Line)
{
    UE_LOG(LogTemp, Display, TEXT("%s"), *Line);
    S.Lines.Add(Line);
}

void Detail(FState& S, const TCHAR* Section, const FString& Row) { S.Details += FString::Printf(TEXT("%s,%s\n"), Section, *Row); }

bool SharedRun()
{
    static const TCHAR* const SelfExiting[] = {
        TEXT("FreeRoamWalkReview"), TEXT("ParkingLotWalkReview"), TEXT("WoodsWalkReview"), TEXT("ClockTowerReview"), TEXT("HouseExteriorViews"), TEXT("CalderAccessReview"), TEXT("CalderFarmSolidWalkReview"), TEXT("CalderWildlifeReview"), TEXT("CalderWildlifePathsReview"), TEXT("CalderWildlifeNetworkReview"), TEXT("CalderWildlifeBirdArtReview"), TEXT("CalderWildlifeSkyPathsReview"), TEXT("CalderWildlifeSkyReview"), TEXT("CalderGoosePondReview"), TEXT("CalderGoosePondNetworkReview"),
        TEXT("CampusInteriorsReview"), TEXT("CampusInteriorsSmoke"), TEXT("HouseExteriorWalkReview"), TEXT("HouseWalkPerf"), TEXT("CampusDrumsRecord"),
        TEXT("WestFieldWalkReview"), TEXT("CampusLayoutReview"), TEXT("CalderReachReview"), TEXT("CalderLandscapeReview"), TEXT("CalderWildlifeReview"),
        TEXT("CalderFarLandSelfTest") };
    TArray<FString> Tokens, Switches;
    FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);
    for (FString Sw : Switches)
    {
        int32 Eq = INDEX_NONE;
        if (Sw.FindChar(TEXT('='), Eq)) Sw.LeftInline(Eq);
        for (const TCHAR* Name : SelfExiting) if (Sw.Equals(Name, ESearchCase::CaseSensitive)) return true;
    }
    return false;
}

bool OnPlanGround(const FVector2D& P, const TArray<FBox2D>& Cuts)
{
    for (const HouseCampusLayout::FBlock& B : HouseCampusLayout::Blocks()) if (B.Rect().ExpandBy(60.f).IsInside(P)) return false;
    for (const FBox2D& Cut : Cuts) if (Cut.ExpandBy(60.f).IsInside(P)) return false;
    return true;
}

// ------------------------------------------------------------------ the world's parts
/** A Calder collision part that blocks the player, with its bounds. */
struct FPrim { UPrimitiveComponent* C = nullptr; FBox Box; bool bTerrain = false; };

struct FContext
{
    UWorld* W = nullptr;
    AHouseExteriorDetail* Exterior = nullptr;
    TMap<int32, UPrimitiveComponent*> Collision;   // the terrain tiles' collision by tile index
    int32 Drawn = 0;
    UPrimitiveComponent* Grounds = nullptr;        // today's ground mesh (the frozen zones)
    TArray<FPrim> Prims;
};

void Gather(FContext& C)
{
    TArray<UPrimitiveComponent*> Owned;
    C.Exterior->GetComponents(Owned);
    for (UPrimitiveComponent* P : Owned)
    {
        const FString Name = P->GetName();
        if (Name.StartsWith(TEXT("CalderTerrainCollision_"))) C.Collision.Add(FCString::Atoi(*Name.RightChop(23)), P);
        else if (Name.StartsWith(TEXT("CalderTerrainTile_"))) ++C.Drawn;
        else if (Name == TEXT("ExteriorGrounds")) C.Grounds = P;
    }
    for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
    {
        UPrimitiveComponent* P = *It;
        if (!IsValid(P) || P->GetWorld() != C.W || !P->IsRegistered() || !P->GetName().StartsWith(TEXT("Calder"))) continue;
        if (!P->IsQueryCollisionEnabled() || P->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block) continue;
        FPrim& F = C.Prims.AddDefaulted_GetRef();
        F.C = P;
        F.Box = P->Bounds.GetBox();
        F.bTerrain = P->GetName().StartsWith(TEXT("CalderTerrainCollision_"));
    }
}

FCollisionQueryParams Query(bool bComplex) { return FCollisionQueryParams(SCENE_QUERY_STAT(CalderTerrainReview), bComplex); }

float Ground(const FVector2D& P) { return HouseExteriorSite::GroundHeight(float(P.X), float(P.Y)); }
bool Frozen(const FVector2D& P) { return HouseCalderTerrain::InFrozen(float(P.X), float(P.Y)); }
bool InSquare(const FVector2D& P, double Inset = 0.0) { return FMath::Abs(P.X) <= SquareHalf - Inset && FMath::Abs(P.Y) <= SquareHalf - Inset; }
FVector2D Perp(const FVector2D& D) { return FVector2D(-D.Y, D.X); }

/** Inside a plan building's footprint, its edges included (landform.py footprint_at), grown by Pad. */
bool InFootprint(const FVector2D& P, double Pad = 0.0)
{
    for (const HouseCampusLayout::FBlock& B : HouseCampusLayout::Blocks())
        if (P.X >= B.X0 - Pad && P.X <= B.X1 + Pad && P.Y >= B.Y0 - Pad && P.Y <= B.Y1 + Pad) return true;
    return false;
}

bool InBoxes(const FVector2D& P, const TArray<FBox2D>& Boxes, double Pad)
{
    for (const FBox2D& B : Boxes) if (P.X >= B.Min.X - Pad && P.X <= B.Max.X + Pad && P.Y >= B.Min.Y - Pad && P.Y <= B.Max.Y + Pad) return true;
    return false;
}

/** landform.py seg_proj: the distance from P to segment AB (and where along it, 0..1). */
double SegDist(const FVector2D& P, const FVector2D& A, const FVector2D& B, double* OutT = nullptr)
{
    const FVector2D D = B - A;
    const double L2 = D.SizeSquared();
    const double T = L2 > 0.0 ? FMath::Clamp(FVector2D::DotProduct(P - A, D) / L2, 0.0, 1.0) : 0.0;
    if (OutT) *OutT = T;
    return FVector2D::Distance(A + D * T, P);
}

double PolyDist(const TArray<FVector2D>& Pts, const FVector2D& P, bool bClosed)
{
    double Best = DBL_MAX;
    const int32 Num = Pts.Num();
    for (int32 K = 0; K + 1 < Num || (bClosed && K < Num && Num > 2); ++K)
        Best = FMath::Min(Best, SegDist(P, Pts[K], Pts[(K + 1) % Num]));
    return Num == 1 ? FVector2D::Distance(Pts[0], P) : Best;
}

/** landform.py in_poly (even-odd). */
bool InPoly(const TArray<FVector2D>& Poly, const FVector2D& P)
{
    bool bIn = false;
    for (int32 I = 0, J = Poly.Num() - 1; I < Poly.Num(); J = I++)
    {
        const FVector2D& A = Poly[I];
        const FVector2D& B = Poly[J];
        if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X) bIn = !bIn;
    }
    return bIn;
}

/** Evenly spaced stations along a run of Len, about Step apart, Inset from either end (the middle when it is too short). */
TArray<double> Stations(double Len, double Step, double Inset)
{
    TArray<double> Out;
    const double Span = Len - 2.0 * Inset;
    if (Span <= 0.0) { Out.Add(Len * .5); return Out; }
    const int32 Count = FMath::Max(1, int32(FMath::CeilToInt(Span / Step)));
    for (int32 K = 0; K <= Count; ++K) Out.Add(Inset + Span * K / Count);
    return Out;
}

/** The segment AB passes through the box (slabs). */
bool SegmentHitsBox(const FVector& A, const FVector& B, const FBox& Box)
{
    double T0 = 0.0, T1 = 1.0;
    const FVector D = B - A;
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const double From = A[Axis], Along = D[Axis], Lo = Box.Min[Axis], Hi = Box.Max[Axis];
        if (FMath::Abs(Along) < 1e-9)
        {
            if (From < Lo || From > Hi) return false;
            continue;
        }
        double TA = (Lo - From) / Along, TB = (Hi - From) / Along;
        if (TA > TB) Swap(TA, TB);
        T0 = FMath::Max(T0, TA);
        T1 = FMath::Min(T1, TB);
        if (T0 > T1) return false;
    }
    return true;
}

/** The terrain tile collision under P (the tile P falls in), traced from 3 m over its GroundHeight to 3 m under it. */
bool TileZ(const FContext& C, const FVector2D& P, float& OutZ)
{
    const int32 TX = FMath::Clamp(FMath::FloorToInt((P.X + HouseCalderTerrain::Half) / HouseCalderTerrainTiles::TileSize), 0, PerSide - 1);
    const int32 TY = FMath::Clamp(FMath::FloorToInt((P.Y + HouseCalderTerrain::Half) / HouseCalderTerrainTiles::TileSize), 0, PerSide - 1);
    UPrimitiveComponent* const* Tile = C.Collision.Find(TY * PerSide + TX);
    if (!Tile) return false;
    const float G = Ground(P);
    FHitResult Hit;
    if (!(*Tile)->LineTraceComponent(Hit, FVector(P, G + 300.f), FVector(P, G - 300.f), Query(true))) return false;
    OutZ = float(Hit.ImpactPoint.Z);
    return true;
}

/** The first thing the player would stand on under P, traced down from Top to Bottom. */
bool WorldZ(UWorld* W, const FVector2D& P, double Top, double Bottom, float& OutZ, FHitResult* OutHit = nullptr)
{
    FHitResult Hit;
    FCollisionQueryParams FloorQuery = Query(false);
    // A concurrent walking review must not turn its own pawn into a floor.
    FloorQuery.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(W, 0));
    if (!W->LineTraceSingleByChannel(Hit, FVector(P, Top), FVector(P, Bottom), ECC_Pawn, FloorQuery)) return false;
    OutZ = float(Hit.ImpactPoint.Z);
    if (OutHit) *OutHit = Hit;
    return true;
}

// ------------------------------------------------------------------ declared walls
struct FWallSeg { FString Name; HouseCalderLandform::EWallKind Kind; FVector2D A, B, Dir; double Len = 0.0; FBox2D Box; };

TArray<FWallSeg> WallSegs()
{
    TArray<FWallSeg> Out;
    for (const HouseCalderLandform::FWall& Wl : HouseCalderLandform::Walls())
    {
        const double Len = FVector2D::Distance(Wl.A, Wl.B);
        if (Len < 1.0) continue;
        FWallSeg& Sg = Out.AddDefaulted_GetRef();
        Sg.Name = Wl.Name;
        Sg.Kind = Wl.Kind;
        Sg.A = Wl.A;
        Sg.B = Wl.B;
        Sg.Len = Len;
        Sg.Dir = (Wl.B - Wl.A) / Len;
        Sg.Box = FBox2D(FVector2D(FMath::Min(Wl.A.X, Wl.B.X), FMath::Min(Wl.A.Y, Wl.B.Y)), FVector2D(FMath::Max(Wl.A.X, Wl.B.X), FMath::Max(Wl.A.Y, Wl.B.Y)));
    }
    return Out;
}

/** The edge UV lies on one of the walls (both ends within 1 cm of its line, within its extent). */
bool OnWallLine(const FVector2D& U, const FVector2D& V, const TArray<const FWallSeg*>& Cands)
{
    for (const FWallSeg* Wl : Cands)
    {
        const FVector2D N = Perp(Wl->Dir);
        const double DU = FMath::Abs(FVector2D::DotProduct(U - Wl->A, N)), DV = FMath::Abs(FVector2D::DotProduct(V - Wl->A, N));
        const double SU = FVector2D::DotProduct(U - Wl->A, Wl->Dir), SV = FVector2D::DotProduct(V - Wl->A, Wl->Dir);
        if (DU <= 1.0 && DV <= 1.0 && FMath::Min(SU, SV) >= -1.0 && FMath::Max(SU, SV) <= Wl->Len + 1.0) return true;
    }
    return false;
}

bool NearWall(const FVector2D& P, double R, const TArray<const FWallSeg*>& Cands)
{
    for (const FWallSeg* Wl : Cands) if (SegDist(P, Wl->A, Wl->B) < R) return true;
    return false;
}

TArray<const FWallSeg*> WallsNear(const TArray<FWallSeg>& All, const FBox2D& Box)
{
    TArray<const FWallSeg*> Out;
    for (const FWallSeg& Wl : All) if (Wl.Box.Intersect(Box)) Out.Add(&Wl);
    return Out;
}

// ------------------------------------------------------------------ water (the exported data, landform.py in_water ported)
struct FWater
{
    TArray<FVector2D> Pond, GlenLine, CreekLine, Crest, Race;
    FWaterPts Glen, Creek;
    float PondZ = 0.f, RaceHalf = 0.f, CreekHalf = 180.f, FallsY = 0.f, CascadeY = 0.f, WetY = 0.f, ShelfX0 = 0.f, ShelfX1 = 0.f;
};

const FWater& WaterData()
{
    static const FWater Data = []
    {
        FWater Out;
        Out.Pond = HouseCalderLandform::PondOutline();
        Out.PondZ = HouseCalderLandform::PondWaterZ();
        Out.Glen = HouseCalderLandform::GlenPoints();
        Out.Creek = HouseCalderLandform::CreekPoints();
        for (const HouseCalderLandform::FWaterPoint& P : Out.Glen) Out.GlenLine.Add(P.At);
        for (const HouseCalderLandform::FWaterPoint& P : Out.Creek) Out.CreekLine.Add(P.At);
        if (Out.Creek.Num()) Out.CreekHalf = Out.Creek[0].HalfWidth;
        const HouseCalderLandform::FPondDam& Dam = HouseCalderLandform::PondDam();
        Out.Crest = Dam.Crest;
        Out.Race = Dam.Race;
        Out.RaceHalf = Dam.RaceHalfWidth;
        Out.FallsY = float(HouseCalderLandform::CalderFalls().LipA.Y);
        Out.CascadeY = float(HouseCalderLandform::Cascade().LipA.Y);
        const HouseCalderLandform::FRockHouse& Rock = HouseCalderLandform::RockHouse();
        Out.WetY = Rock.WetY;
        Out.ShelfX0 = Rock.X0;
        Out.ShelfX1 = Rock.X1;
        return Out;
    }();
    return Data;
}

/** landform.py dam_line_y: the dam crest's Y at X (held flat past its ends). */
double DamLineY(const FWater& Wd, double X)
{
    const TArray<FVector2D>& C = Wd.Crest;
    if (!C.Num()) return -1e9;
    if (X <= C[0].X) return C[0].Y;
    for (int32 K = 0; K + 1 < C.Num(); ++K)
        if (X <= C[K + 1].X) return FMath::Lerp(C[K].Y, C[K + 1].Y, (X - C[K].X) / (C[K + 1].X - C[K].X));
    return C.Last().Y;
}

/** Where latitude Y falls between a water line's points (they run north to south): the point before and the fraction on. */
void LatitudeSpan(const FWaterPts& Pts, double Y, int32& OutI, double& OutT)
{
    OutI = 0;
    OutT = 0.0;
    if (Pts.Num() < 2 || Y >= Pts[0].At.Y) return;
    if (Y <= Pts.Last().At.Y) { OutI = Pts.Num() - 2; OutT = 1.0; return; }
    int32 Lo = 0, Hi = Pts.Num() - 1;
    while (Hi - Lo > 1)
    {
        const int32 Mid = (Lo + Hi) / 2;
        if (Pts[Mid].At.Y > Y) Lo = Mid; else Hi = Mid;
    }
    OutI = Lo;
    const double DY = Pts[Lo].At.Y - Pts[Hi].At.Y;
    OutT = DY > 0.0 ? (Pts[Lo].At.Y - Y) / DY : 0.0;
}

double HalfWidthAtY(const FWaterPts& Pts, double Y)
{
    int32 I; double T;
    LatitudeSpan(Pts, Y, I, T);
    return Pts.Num() < 2 ? 0.0 : FMath::Lerp(double(Pts[I].HalfWidth), double(Pts[I + 1].HalfWidth), T);
}

double WaterZAtY(const FWaterPts& Pts, double Y)
{
    int32 I; double T;
    LatitudeSpan(Pts, Y, I, T);
    return Pts.Num() < 2 ? 0.0 : FMath::Lerp(double(Pts[I].WaterZ), double(Pts[I + 1].WaterZ), T);
}

/** The point and unit tangent at arc length Sv along a water line (its points carry their arc length). */
void LineAt(const FWaterPts& Pts, double Sv, FVector2D& OutP, FVector2D& OutT)
{
    OutP = Pts.Num() ? Pts[0].At : FVector2D::ZeroVector;
    OutT = FVector2D(0.0, 1.0);
    for (int32 K = 0; K + 1 < Pts.Num(); ++K)
        if ((Sv >= Pts[K].S && Sv <= Pts[K + 1].S) || K + 2 == Pts.Num())
        {
            const double Span = FMath::Max(1e-6, double(Pts[K + 1].S - Pts[K].S));
            const double T = FMath::Clamp((Sv - Pts[K].S) / Span, 0.0, 1.0);
            OutP = FMath::Lerp(Pts[K].At, Pts[K + 1].At, T);
            OutT = (Pts[K + 1].At - Pts[K].At).GetSafeNormal();
            return;
        }
}

double WaterZAtS(const FWaterPts& Pts, double Sv)
{
    for (int32 K = 0; K + 1 < Pts.Num(); ++K)
        if (Sv <= Pts[K + 1].S || K + 2 == Pts.Num())
        {
            const double Span = FMath::Max(1e-6, double(Pts[K + 1].S - Pts[K].S));
            return FMath::Lerp(double(Pts[K].WaterZ), double(Pts[K + 1].WaterZ), FMath::Clamp((Sv - Pts[K].S) / Span, 0.0, 1.0));
        }
    return Pts.Num() ? Pts.Last().WaterZ : 0.0;
}

double PondEdgeDist(const FWater& Wd, const FVector2D& P) { return PolyDist(Wd.Pond, P, true); }

/** landform.py in_water on the exported data. */
bool InWater(const FWater& Wd, const FVector2D& P)
{
    const double X = P.X, Y = P.Y;
    if (X > 22800.0 && X < 27300.0 && Y > -16100.0 && Y < -10700.0 && InPoly(Wd.Pond, P)) return true;
    const double Dl = DamLineY(Wd, X);
    if (Y < Dl && X > 20000.0 && X < 32000.0 && PolyDist(Wd.GlenLine, P, false) < HalfWidthAtY(Wd.Glen, Y))
        return !(Y > Wd.CascadeY && Y < Wd.FallsY && Y > Wd.WetY);   // the rock house shelf is dry
    if (Y >= Dl && X > 24600.0 && X < 25900.0 && Y < -15300.0 && Wd.Race.Num() >= 2 && SegDist(P, Wd.Race[0], Wd.Race[1]) < Wd.RaceHalf && !InPoly(Wd.Pond, P)) return true;
    return Y > -11400.0 && X > 20000.0 && X < 32000.0 && PolyDist(Wd.CreekLine, P, false) < Wd.CreekHalf;
}

// ------------------------------------------------------------------ 1. the base checks
void StageBase(FState& S, const FContext& C, const FString& Dir)
{
    TArray<FBox2D> Cuts;
    HouseCampusLayout::TerrainCuts(Cuts);
    const FBox2D Woods(FVector2D(HouseWoods::XMin, HouseWoods::YMin), FVector2D(HouseWoods::XMax, HouseWoods::YMax));

    // Accuracy and holes.
    FRandomStream Rng(19861018);
    int32 Samples = 0, Holes = 0, Over = 0;
    float MaxErr = 0.f;
    FString Worst;
    const bool bTerrain = HouseCalderTerrain::Enabled();
    for (int32 Try = 0; bTerrain && Samples < 20000 && Try < 400000; ++Try)
    {
        const FVector2D P(Rng.FRandRange(-31900.f, 31900.f), Rng.FRandRange(-31900.f, 31900.f));
        if (HouseCalderTerrain::InFrozen(P.X, P.Y) || Woods.ExpandBy(60.f).IsInside(P) || !OnPlanGround(P, Cuts)) continue;
        if (P.X > 8640.f && P.X < 10360.f) continue;   // Calder Dr's carriageway: the exterior cuts the ground for it the whole way north
        bool bNearFrozen = false;   // the frozen rectangles' edges are where the tiles stop: keep 60 cm off them
        for (const FBox2D& R : HouseCalderTerrain::FrozenRects()) bNearFrozen |= R.ExpandBy(60.f).IsInside(P);
        if (bNearFrozen) continue;
        ++Samples;
        const float G = HouseExteriorSite::GroundHeight(P.X, P.Y);
        const int32 TX = FMath::Clamp(FMath::FloorToInt((P.X + HouseCalderTerrain::Half) / HouseCalderTerrainTiles::TileSize), 0, PerSide - 1);
        const int32 TY = FMath::Clamp(FMath::FloorToInt((P.Y + HouseCalderTerrain::Half) / HouseCalderTerrainTiles::TileSize), 0, PerSide - 1);
        UPrimitiveComponent* const* Tile = C.Collision.Find(TY * PerSide + TX);
        FHitResult Hit;
        if (!Tile || !(*Tile)->LineTraceComponent(Hit, FVector(P, G + 500.f), FVector(P, G - 500.f), FCollisionQueryParams(SCENE_QUERY_STAT(CalderTerrainReview), true)))
        {
            if (++Holes <= 5) UE_LOG(LogTemp, Display, TEXT("CALDER_TERRAIN_REVIEW hole at (%.0f,%.0f) ground %.1f"), P.X, P.Y, G);
            continue;
        }
        const float Err = FMath::Abs(Hit.ImpactPoint.Z - G);
        Over += Err > 3.f ? 1 : 0;
        if (Err > MaxErr) { MaxErr = Err; Worst = FString::Printf(TEXT("(%.0f,%.0f) ground %.2f hit %.2f"), P.X, P.Y, G, Hit.ImpactPoint.Z); }
    }

    // The frozen zones' ground on a 10 m grid (wherever today's ground mesh is there), for the A/B comparison.
    FString Grid = TEXT("x,y,z\n");
    int32 GridPoints = 0;
    for (const FBox2D& R : HouseCalderTerrain::FrozenRects())
        for (float Y = FMath::CeilToFloat(R.Min.Y / 1000.f) * 1000.f; Y <= R.Max.Y; Y += 1000.f)
            for (float X = FMath::CeilToFloat(R.Min.X / 1000.f) * 1000.f; X <= R.Max.X; X += 1000.f)
            {
                FHitResult Hit;
                if (!C.Grounds || !C.Grounds->LineTraceComponent(Hit, FVector(X, Y, 3000.f), FVector(X, Y, -3000.f), FCollisionQueryParams(SCENE_QUERY_STAT(CalderTerrainReviewFrozen), true))) continue;
                Grid += FString::Printf(TEXT("%.0f,%.0f,%.3f\n"), X, Y, Hit.ImpactPoint.Z);
                ++GridPoints;
            }
    FFileHelper::SaveStringToFile(Grid, *(Dir / TEXT("frozen_grid.csv")));

    // Point and spot lights outside the house (the house's own rooms are lit inside its footprint).
    const FBox2D House(FVector2D(-3500.f, -2300.f), FVector2D(3400.f, 1800.f));
    int32 Lights = 0;
    for (TObjectIterator<ULocalLightComponent> It; It; ++It)
        if (It->GetWorld() == C.W && It->IsRegistered() && It->IsVisible() && !House.IsInside(FVector2D(It->GetComponentLocation()))) ++Lights;

    const bool bOK = !bTerrain || (Samples >= 20000 && Holes == 0 && Over == 0 && C.Collision.Num() > 0 && C.Drawn == C.Collision.Num());
    S.Base = bOK ? 1 : 0;
    S.Lights = Lights;
    S.BaseLine = FString::Printf(TEXT("terrain=%d hash=%08x tiles=%d collisionParts=%d samples=%d holes=%d over3cm=%d maxErr=%.2f worst=%s frozenGrid=%d lightsOutsideHouse=%d"),
        int32(bTerrain), HouseCalderTerrain::Hash(), C.Drawn, C.Collision.Num(), Samples, Holes, Over, MaxErr, Worst.IsEmpty() ? TEXT("-") : *Worst, GridPoints, Lights);
}

// ------------------------------------------------------------------ 2. the eye-level skyline proof
double PyMod360(double A) { return A - 360.0 * FMath::FloorToDouble(A / 360.0); }
double AngDiff(double A, double B) { const double D = PyMod360(FMath::Abs(A - B)); return FMath::Min(D, 360.0 - D); }
double DegToRad(double D) { return D * (UE_DOUBLE_PI / 180.0); }

/** landform.py check_eye's march along one azimuth: the highest elevation angle of the skyline ground, and how far out. */
void March(double X0, double Y0, double Eye, double AzDeg, double& OutBest, double& OutDist)
{
    const double A = DegToRad(AzDeg), CA = FMath::Cos(A), SA = FMath::Sin(A);
    double Best = -90.0, BestD = 0.0;
    for (double D = 150.0; D < SkyMax; D += FMath::Max(100.0, D * 0.006))
    {
        const double X = X0 + CA * D, Y = Y0 + SA * D;
        if (FMath::Abs(X) > SkyStop || FMath::Abs(Y) > SkyStop) break;
        const double Ang = FMath::RadiansToDegrees(FMath::Atan2(HouseCalderFarLand::SkylineGround(X, Y) - Eye, D));
        if (Ang > Best) { Best = Ang; BestD = D; }
    }
    OutBest = Best;
    OutDist = BestD;
}

/** landform.py ray_layers: silhouette layers on one bearing (skyline records each 1.4 times further out than the last, after a
 *  dip of 0.3 deg or more). */
int32 RayLayers(double X0, double Y0, double Eye, double AzDeg)
{
    const double A = DegToRad(AzDeg), CA = FMath::Cos(A), SA = FMath::Sin(A);
    double Rec = -90.0, Dip = 90.0, LastD = 0.0;
    int32 Layers = 0;
    for (double D = 150.0; D < SkyMax; D += FMath::Max(100.0, D * 0.006))
    {
        const double X = X0 + CA * D, Y = Y0 + SA * D;
        if (FMath::Abs(X) > SkyStop || FMath::Abs(Y) > SkyStop) break;
        const double Ang = FMath::RadiansToDegrees(FMath::Atan2(HouseCalderFarLand::SkylineGround(X, Y) - Eye, D));
        if (Ang > Rec)
        {
            if (Layers == 0 || (D > LastD * 1.4 && Rec - Dip >= 0.3)) ++Layers;
            LastD = D;
            Rec = Ang;
            Dip = Ang;
        }
        else Dip = FMath::Min(Dip, Ang);
    }
    return Layers;
}

/** landform.py notch_bearings: the Calder Branch valley's and the water gap's bearings from a spot. */
void NotchBearings(const FVector2D& P, double& OutNe, double& OutGap)
{
    OutNe = PyMod360(FMath::RadiansToDegrees(FMath::Atan2(42000.0 - P.Y, 36000.0 - P.X)));
    OutGap = PyMod360(FMath::RadiansToDegrees(FMath::Atan2(-89000.0 - P.Y, 60000.0 + 0.12 * 43400.0 - P.X)));
}

/** landform.py walkable_frame_peak: the walkable ground's highest elevation angle within +-45 deg of a facing. */
double FramePeak(const FVector2D& At, double Eye, double Facing, double& OutYaw, double& OutDist)
{
    double Best = -90.0;
    OutYaw = Facing;
    OutDist = 0.0;
    for (int32 K = -9; K <= 9; ++K)
    {
        const double Yaw = Facing + 5.0 * K, A = DegToRad(Yaw), CA = FMath::Cos(A), SA = FMath::Sin(A);
        for (double D = 150.0;; D += FMath::Max(100.0, D * 0.006))
        {
            const FVector2D P(At.X + CA * D, At.Y + SA * D);
            if (FMath::Abs(P.X) > SquareHalf || FMath::Abs(P.Y) > SquareHalf) break;
            const double Ang = FMath::RadiansToDegrees(FMath::Atan2(double(Ground(P)) - Eye, D));
            if (Ang > Best) { Best = Ang; OutYaw = Yaw; OutDist = D; }
        }
    }
    return Best;
}

void StageSkyline(FState& S)
{
    struct FEye { FString Id, Desc; FVector2D At = FVector2D::ZeroVector; double Facing = 0.0, Base = 0.0, Eye = 0.0; bool bDeck = false, bPass = false; };
    const FWater& Wd = WaterData();
    TArray<FEye> Eyes;
    for (const HouseCalderLandform::FSpot& Sp : HouseCalderLandform::EyeSpots())
    {
        FEye& E = Eyes.AddDefaulted_GetRef();
        int32 Colon = INDEX_NONE;
        E.Id = Sp.Name.FindChar(TEXT(':'), Colon) ? Sp.Name.Left(Colon) : Sp.Name;
        E.Desc = Colon != INDEX_NONE ? Sp.Name.RightChop(Colon + 1).TrimStartAndEnd() : FString();
        E.At = FVector2D(Sp.At);
        E.Facing = Sp.Yaw;
        const double G = Ground(E.At);
        E.bDeck = Sp.GroundZ - G > 20.0;   // a deck over lower ground (P6's timber footbridge, P7's arch on the dam)
        E.Base = E.bDeck ? double(Sp.GroundZ) : G;
        E.Eye = E.Base + EyeHeight;
        E.bPass = E.Id == TEXT("P1") || E.Id == TEXT("P2") || E.Id == TEXT("P3") || E.Id == TEXT("P4") || E.Id == TEXT("P6") || E.Id == TEXT("P7");
    }
    const int32 NumEyes = Eyes.Num();
    TArray<double> Ang, Dist;
    Ang.SetNumZeroed(NumEyes * SkyCount);
    Dist.SetNumZeroed(NumEyes * SkyCount);
    ParallelFor(NumEyes * SkyCount, [&Eyes, &Ang, &Dist](int32 K)
    {
        const FEye& E = Eyes[K / SkyCount];
        March(E.At.X, E.At.Y, E.Eye, (K % SkyCount) * SkyStep, Ang[K], Dist[K]);
    });

    bool bAll = NumEyes > 0;
    S.Skycsv = TEXT("spot,azimuth,deg,distM,notch\n");
    for (int32 EI = 0; EI < NumEyes; ++EI)
    {
        const FEye& E = Eyes[EI];
        TArray<double> A;
        for (int32 K = 0; K < SkyCount; ++K) A.Add(Ang[EI * SkyCount + K]);
        TArray<double> Sorted = A;
        Sorted.Sort();
        const double Med = (Sorted[SkyCount / 2 - 1] + Sorted[SkyCount / 2]) * .5;
        double ANe, AGap;
        NotchBearings(E.At, ANe, AGap);
        auto IsNotch = [ANe, AGap](double Az) { return AngDiff(Az, ANe) <= 20.0 || AngDiff(Az, AGap) <= 8.0; };
        TArray<double> SouthAll, SouthEff;
        FString Low, Undeclared, NotchLayers;
        bool bNotchesOK = true;
        for (int32 K = 0; K < SkyCount; ++K)
        {
            const double Az = K * SkyStep;
            S.Skycsv += FString::Printf(TEXT("%s,%.0f,%.3f,%.1f,%d\n"), *E.Id, Az, A[K], Dist[EI * SkyCount + K] / 100.0, IsNotch(Az) ? 1 : 0);
            if (Az >= 240.0 && Az <= 300.0) { SouthAll.Add(A[K]); if (!IsNotch(Az)) SouthEff.Add(A[K]); }
            if (A[K] >= 3.0) continue;
            Low += FString::Printf(TEXT("%s%.0f:%.1f"), Low.IsEmpty() ? TEXT("") : TEXT(";"), Az, A[K]);
            if (!IsNotch(Az)) { Undeclared += FString::Printf(TEXT("%s%.0f"), Undeclared.IsEmpty() ? TEXT("") : TEXT(";"), Az); continue; }
            // A declared notch may sit lower than 3 deg only where it shows three ridge layers or more.
            const int32 Layers = RayLayers(E.At.X, E.At.Y, E.Eye, Az);
            NotchLayers += FString::Printf(TEXT("%s%.0f:%d"), NotchLayers.IsEmpty() ? TEXT("") : TEXT(";"), Az, Layers);
            bNotchesOK &= Layers >= 3;
        }
        if (!SouthEff.Num()) SouthEff = SouthAll;
        double SouthMin = 90.0;
        for (const double V : SouthEff) SouthMin = FMath::Min(SouthMin, V);

        // The ground within 150 m, on a 5 m grid, outside the frozen zones, the footprints and the water.
        double ZMin = DBL_MAX, ZMax = -DBL_MAX, RMin = DBL_MAX, RMax = -DBL_MAX;
        int32 N150 = 0;
        constexpr double R150 = 15000.0;
        for (double YY = -R150; YY <= R150 + 1e-9; YY += 500.0)
            for (double XX = -R150; XX <= R150 + 1e-9; XX += 500.0)
            {
                if (XX * XX + YY * YY > R150 * R150) continue;
                const FVector2D P(E.At.X + XX, E.At.Y + YY);
                if (!InSquare(P) || Frozen(P) || InFootprint(P) || InWater(Wd, P)) continue;
                const double Z = Ground(P), R = Z - HouseExteriorSite::BaseGrade(float(P.X), float(P.Y));
                ZMin = FMath::Min(ZMin, Z); ZMax = FMath::Max(ZMax, Z);
                RMin = FMath::Min(RMin, R); RMax = FMath::Max(RMax, R);
                ++N150;
            }
        const double Range = N150 ? ZMax - ZMin : 0.0, ReliefRange = N150 ? RMax - RMin : 0.0;
        const int32 FaceK = ((int32(FMath::RoundToInt(E.Facing / SkyStep)) % SkyCount) + SkyCount) % SkyCount;
        const double FacingDeg = A[FaceK];
        const int32 LayFacing = RayLayers(E.At.X, E.At.Y, E.Eye, E.Facing);
        const int32 LayNe = RayLayers(E.At.X, E.At.Y, E.Eye, FMath::RoundHalfToEven(ANe / 5.0) * 5.0);
        const int32 LayGap = RayLayers(E.At.X, E.At.Y, E.Eye, FMath::RoundHalfToEven(AGap / 5.0) * 5.0);
        TArray<FString> Failing;
        if (Med < 8.0) Failing.Add(TEXT("median<8"));
        if (!Undeclared.IsEmpty()) Failing.Add(TEXT("azimuth<3"));
        if (!bNotchesOK) Failing.Add(TEXT("notchLayers<3"));
        if (SouthMin < 14.0) Failing.Add(TEXT("south<14"));
        if (Range < 800.0) Failing.Add(TEXT("range150<8m"));
        const bool bOK = !Failing.Num();
        if (E.bPass) bAll &= bOK;
        Say(S, FString::Printf(TEXT("CALDER_SKYLINE spot=%s minDeg=%.2f medDeg=%.2f maxDeg=%.2f relief150m=%.1f reliefOnly150m=%.1f n150=%d southMinDeg=%.2f facingDeg=%.2f eye=%.1f deck=%d notches=%.0f/%.0f low=%s undeclared=%s notchLayers=%s layers=facing:%d,branch:%d,gap:%d %s%s"),
            *E.Id, Sorted[0], Med, Sorted.Last(), Range / 100.0, ReliefRange / 100.0, N150, SouthMin, FacingDeg, E.Eye, int32(E.bDeck), ANe, AGap,
            Low.IsEmpty() ? TEXT("none") : *Low, Undeclared.IsEmpty() ? TEXT("none") : *Undeclared, NotchLayers.IsEmpty() ? TEXT("-") : *NotchLayers,
            LayFacing, LayNe, LayGap, E.bPass ? (bOK ? TEXT("PASS") : TEXT("FAIL")) : TEXT("INFO"),
            Failing.Num() ? *(TEXT(" failing=") + FString::Join(Failing, TEXT(","))) : TEXT("")));

        // The spots' own rules (landform.py check_eyes).
        if (E.Id == TEXT("P1"))
        {
            const bool bFace = FacingDeg >= 6.0;
            bAll &= bFace;
            double Yaw, Far;
            const double Peak = FramePeak(E.At, E.Eye, E.Facing, Yaw, Far);
            Say(S, FString::Printf(TEXT("CALDER_SKYLINE_RULE spot=P1 rule=eastCoveWall>=6deg facingDeg=%.2f %s walkablePeakDeg=%.2f yaw=%.0f at=%.0fm (INFO)"),
                FacingDeg, bFace ? TEXT("PASS") : TEXT("FAIL"), Peak, PyMod360(Yaw), Far / 100.0));
        }
        else if (E.Id == TEXT("P2"))
        {
            int32 BestLayers = -1;
            double BestAz = E.Facing;
            for (double Az = E.Facing - 45.0; Az <= E.Facing + 45.0 + 1e-9; Az += 5.0)
            {
                const int32 L = RayLayers(E.At.X, E.At.Y, E.Eye, PyMod360(Az));
                if (L > BestLayers) { BestLayers = L; BestAz = Az; }
            }
            double Yaw, Far;
            const double Peak = FramePeak(E.At, E.Eye, E.Facing, Yaw, Far);
            const bool bLayers = BestLayers >= 3, bPeak = Peak >= 5.0;
            bAll &= bLayers && bPeak;
            Say(S, FString::Printf(TEXT("CALDER_SKYLINE_RULE spot=P2 rule=ridgeLayers>=3 best=%d yaw=%.0f %s rule=walkablePeak>=5deg deg=%.2f yaw=%.0f at=%.0fm %s"),
                BestLayers, PyMod360(BestAz), bLayers ? TEXT("PASS") : TEXT("FAIL"), Peak, PyMod360(Yaw), Far / 100.0, bPeak ? TEXT("PASS") : TEXT("FAIL")));
        }
        else if (E.Id == TEXT("P6") && Wd.Glen.Num() >= 2)
        {
            // The glen's direction at the footbridge, then the highest walkable ground 1-80 m out either side of it.
            int32 I; double T;
            LatitudeSpan(Wd.Glen, E.At.Y, I, T);
            const FVector2D Tan = (Wd.Glen[I + 1].At - Wd.Glen[I].At).GetSafeNormal();
            double Walls[2] = { -DBL_MAX, -DBL_MAX }, WallAt[2] = { 0.0, 0.0 };
            for (int32 Side = 0; Side < 2; ++Side)
            {
                const double Sg = Side == 0 ? -1.0 : 1.0;
                for (double D = 100.0; D <= 8000.0 + 1e-9; D += 50.0)
                {
                    const double Z = Ground(E.At + Perp(Tan) * (D * Sg));
                    if (Z > Walls[Side]) { Walls[Side] = Z; WallAt[Side] = D; }
                }
            }
            const double West = (Walls[0] - E.Eye) / 100.0, East = (Walls[1] - E.Eye) / 100.0;
            const bool bWalls = FMath::Min(West, East) >= 15.0;
            bAll &= bWalls;
            Say(S, FString::Printf(TEXT("CALDER_SKYLINE_RULE spot=P6 rule=glenWalls>=15m overEye=%+.1fm(at %.0fm),%+.1fm(at %.0fm) %s"),
                West, WallAt[0] / 100.0, East, WallAt[1] / 100.0, bWalls ? TEXT("PASS") : TEXT("FAIL")));
        }
    }
    S.Skyline = bAll ? 1 : 0;
}

// ------------------------------------------------------------------ 3. cracks
void StageCracks(FState& S, const FContext& C)
{
    const TArray<FWallSeg> AllWalls = WallSegs();
    TArray<FBox2D> Cuts;
    HouseCampusLayout::TerrainCuts(Cuts);
    int32 Samples = 0, TileSamples = 0, PieceEdges = 0, OnWall = 0, NearWallSkip = 0, Open = 0, FrozenSide = 0, AnalyticOver = 0, CollisionOver = 0;
    float MaxAnalytic = 0.f, MaxCollision = 0.f;
    struct FCrack { float Step = 0.f; FVector2D At = FVector2D::ZeroVector; bool bTile = false, bCollision = false; };
    TArray<FCrack> Found;

    // M on an edge whose unit normal is N: the ground 2.5 mm either side, by GroundHeight and by the tile collision.
    auto Probe = [&](const FVector2D& M, const FVector2D& N, bool bTile)
    {
        const FVector2D PA = M + N * CrackEps, PB = M - N * CrackEps;
        if (!InSquare(PA, 1.0) || !InSquare(PB, 1.0)) return;
        if (Frozen(PA) || Frozen(PB)) { ++FrozenSide; return; }
        if (InFootprint(PA, 1.0) || InFootprint(PB, 1.0) || InBoxes(PA, Cuts, 1.0) || InBoxes(PB, Cuts, 1.0)) { ++Open; return; }
        ++Samples;
        TileSamples += bTile ? 1 : 0;
        const float Step = FMath::Abs(Ground(PA) - Ground(PB));
        MaxAnalytic = FMath::Max(MaxAnalytic, Step);
        if (Step > CrackLimit) { ++AnalyticOver; Found.Add({ Step, M, bTile, false }); }
        float ZA = 0.f, ZB = 0.f;
        if (!TileZ(C, PA, ZA) || !TileZ(C, PB, ZB)) { ++Open; return; }   // a hole beside (the base check owns holes)
        const float Coll = FMath::Abs(ZA - ZB);
        MaxCollision = FMath::Max(MaxCollision, Coll);
        if (Coll > CrackLimit) { ++CollisionOver; Found.Add({ Coll, M, bTile, true }); }
    };

    // Every tile edge (lattice lines), every 50 cm, off the declared walls that cross it.
    const double TileSize = HouseCalderTerrainTiles::TileSize, Half = HouseCalderTerrain::Half;
    for (int32 K = 1; K < PerSide; ++K)
    {
        const double Line = -Half + K * TileSize;
        for (int32 Axis = 0; Axis < 2; ++Axis)
        {
            const FBox2D Strip = Axis == 0 ? FBox2D(FVector2D(Line - 3.0, -Half), FVector2D(Line + 3.0, Half)) : FBox2D(FVector2D(-Half, Line - 3.0), FVector2D(Half, Line + 3.0));
            const TArray<const FWallSeg*> Cands = WallsNear(AllWalls, Strip);
            for (double Along = -Half + 25.0; Along < Half; Along += 50.0)
            {
                const FVector2D M = Axis == 0 ? FVector2D(Line, Along) : FVector2D(Along, Line);
                if (NearWall(M, 2.0, Cands)) { ++NearWallSkip; continue; }
                Probe(M, Axis == 0 ? FVector2D(1.0, 0.0) : FVector2D(0.0, 1.0), true);
            }
        }
    }

    // Every edge of every wall-split cell's pieces that is not on a declared wall: a quarter, half and three quarters along.
    const int32 Cells = HouseCalderTerrain::Cells;
    const double CellSize = HouseCalderTerrain::Cell;
    for (int32 J = 0; J < Cells; ++J)
        for (int32 I = 0; I < Cells; ++I)
        {
            const TArray<TArray<FVector>>* Pieces = HouseCalderLandform::WallPieces(I, J);
            if (!Pieces) continue;
            const FVector2D Lo(-Half + I * CellSize, -Half + J * CellSize);
            const TArray<const FWallSeg*> Cands = WallsNear(AllWalls, FBox2D(Lo, Lo + FVector2D(CellSize, CellSize)).ExpandBy(3.0));
            for (const TArray<FVector>& Poly : *Pieces)
                for (int32 V = 0; V < Poly.Num(); ++V)
                {
                    const FVector2D U(Poly[V]), W2(Poly[(V + 1) % Poly.Num()]);
                    const double Len = FVector2D::Distance(U, W2);
                    if (Len < .5) continue;
                    ++PieceEdges;
                    if (OnWallLine(U, W2, Cands)) { ++OnWall; continue; }
                    const bool bOnSquareEdge = (FMath::Abs(U.X) >= Half - .01 && FMath::Abs(W2.X) >= Half - .01 && U.X * W2.X > 0.0)
                        || (FMath::Abs(U.Y) >= Half - .01 && FMath::Abs(W2.Y) >= Half - .01 && U.Y * W2.Y > 0.0);
                    if (bOnSquareEdge) continue;
                    const FVector2D N = Perp((W2 - U) / Len);
                    for (const double T : Len >= 3.0 ? TArray<double>({ .25, .5, .75 }) : TArray<double>({ .5 }))
                    {
                        const FVector2D M = FMath::Lerp(U, W2, T);
                        if (NearWall(M, 1.0, Cands)) { ++NearWallSkip; continue; }
                        Probe(M, N, false);
                    }
                }
        }
    Found.Sort([](const FCrack& A, const FCrack& B) { return A.Step > B.Step; });
    FString Worst;
    for (int32 K = 0; K < FMath::Min(10, Found.Num()); ++K)
    {
        const FCrack& Ck = Found[K];
        Worst += FString::Printf(TEXT("%s(%.0f,%.0f):%.1f%s%s"), Worst.IsEmpty() ? TEXT("") : TEXT(";"), Ck.At.X, Ck.At.Y, Ck.Step, Ck.bCollision ? TEXT("c") : TEXT("g"), Ck.bTile ? TEXT("t") : TEXT("p"));
        Detail(S, TEXT("crack"), FString::Printf(TEXT("%.1f,%.1f,%.2f,%s,%s"), Ck.At.X, Ck.At.Y, Ck.Step, Ck.bCollision ? TEXT("collision") : TEXT("ground"), Ck.bTile ? TEXT("tileEdge") : TEXT("pieceEdge")));
    }
    const bool bOK = Samples > 0 && AnalyticOver == 0 && CollisionOver == 0;
    S.Cracks = bOK ? 1 : 0;
    Say(S, FString::Printf(TEXT("CALDER_CRACKS %s samples=%d tileEdgeSamples=%d pieceEdges=%d onDeclaredWall=%d nearWallSkipped=%d open=%d frozenSide=%d groundOver3cm=%d collisionOver3cm=%d maxGroundStep=%.2f maxCollisionStep=%.2f worst=%s"),
        bOK ? TEXT("PASS") : TEXT("FAIL"), Samples, TileSamples, PieceEdges, OnWall, NearWallSkip, Open, FrozenSide, AnalyticOver, CollisionOver, MaxAnalytic, MaxCollision, Worst.IsEmpty() ? TEXT("none") : *Worst));
}

// ------------------------------------------------------------------ 4. slopes
/** What the player would stand on at P (the first part a ray down from 30 m over the ground meets, as the player collides):
 *  when it is a Calder part and its face there is steeper than 40 deg, the part must be Unwalkable (its walkable-slope
 *  override); one the player could even walk on (40 deg up to the engine's 44.8, or an override that raises it) is listed
 *  apart. Anything else on top (a building, a tree trunk, a bench) is not this review's: counted. */
void SlopeAt(FState& S, const FContext& C, const FVector2D& P, float FloorZ)
{
    ++S.SlopeSamples;
    const float G = Ground(P);
    FHitResult Hit;
    if (!C.W->LineTraceSingleByChannel(Hit, FVector(P, G + 3000.f), FVector(P, G - 300.f), ECC_Pawn, Query(false)) || !Hit.GetComponent()) return;
    UPrimitiveComponent* Comp = Hit.GetComponent();
    const FString Name = Comp->GetName();
    if (!Name.StartsWith(TEXT("Calder"))) { ++S.SlopeOther; return; }
    ++S.SlopeHits;
    const FVector N = Hit.ImpactNormal;
    if (N.Z <= 0.0) { ++S.SlopeDown; return; }
    const float Deg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(N.Z), -1.f, 1.f)));
    if (Name.StartsWith(TEXT("CalderTerrainCollision_")))
    {
        S.TerrainMax = FMath::Max(S.TerrainMax, Deg);
        S.TerrainOver += Deg > TerrainSlopeDeg ? 1 : 0;
    }
    if (Deg <= SlopeLimitDeg) return;
    ++S.SlopeSteep;
    const FWalkableSlopeOverride& Override = Comp->GetWalkableSlopeOverride();
    if (Override.GetWalkableSlopeBehavior() == WalkableSlope_Unwalkable) return;
    const bool bWalkable = N.Z >= Override.ModifyWalkableFloorZ(FloorZ);
    ++S.SlopeBad;
    S.SlopeWalkable += bWalkable ? 1 : 0;
    if (S.SlopeWorst.Num() < 12) S.SlopeWorst.Add(FString::Printf(TEXT("%s(%.0f,%.0f,%.0f):%.1fdeg%s"), *Name, P.X, P.Y, Hit.ImpactPoint.Z, Deg, bWalkable ? TEXT(",walkable") : TEXT("")));
    Detail(S, TEXT("slope"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.2f,%s,%d"), P.X, P.Y, Hit.ImpactPoint.Z, Deg, *Name, int32(bWalkable)));
}

float WalkFloorZ() { return FMath::Cos(FMath::DegreesToRadians(PlayerWalkableDeg)); }

/** A quarter of the 2 m grid (rows Quarter * 80 on), so no frame traces it all. */
void StageSlopesGrid(FState& S, const FContext& C, int32 Quarter)
{
    const float FloorZ = WalkFloorZ();
    const int32 Rows = (HouseCalderTerrain::Cells + 3) / 4;
    for (int32 J = Quarter * Rows; J < FMath::Min(HouseCalderTerrain::Cells, (Quarter + 1) * Rows); ++J)
        for (int32 I = 0; I < HouseCalderTerrain::Cells; ++I)
        {
            // Off the lattice lines, so no ray runs down a cell edge.
            const FVector2D P(-HouseCalderTerrain::Half + I * HouseCalderTerrain::Cell + 73.0, -HouseCalderTerrain::Half + J * HouseCalderTerrain::Cell + 131.0);
            if (!Frozen(P)) SlopeAt(S, C, P, FloorZ);
        }
}

void StageSlopesWalls(FState& S, const FContext& C)
{
    const float FloorZ = WalkFloorZ();
    for (const FWallSeg& Wl : WallSegs())
    {
        const FVector2D N = Perp(Wl.Dir);
        for (const double St : Stations(Wl.Len, 50.0, 10.0))
            for (int32 K = -5; K <= 5; ++K)
            {
                const FVector2D P = Wl.A + Wl.Dir * St + N * (15.0 * K);
                if (InSquare(P) && !Frozen(P)) SlopeAt(S, C, P, FloorZ);
            }
    }
    S.Slopes = S.SlopeBad == 0 && S.SlopeHits > 0 ? 1 : 0;
    Say(S, FString::Printf(TEXT("CALDER_SLOPES %s parts=%d samples=%d calderHits=%d otherOnTop=%d steeperThan40=%d notUnwalkable=%d ofWhichWalkable=%d downFacing=%d terrainMaxDeg=%.2f terrainOver1to1.5=%d worst=%s"),
        Verdict(S.Slopes), C.Prims.Num(), S.SlopeSamples, S.SlopeHits, S.SlopeOther, S.SlopeSteep, S.SlopeBad, S.SlopeWalkable, S.SlopeDown, S.TerrainMax, S.TerrainOver,
        S.SlopeWorst.Num() ? *FString::Join(S.SlopeWorst, TEXT(";")) : TEXT("none")));
}

// ------------------------------------------------------------------ 5. rails, gaps, stairs and ramps
/** The segment A-B crosses a declared wall's line (within the wall's extent). */
bool CrossesWall(const FVector2D& A, const FVector2D& B)
{
    const FVector2D D = B - A;
    for (const HouseCalderLandform::FWall& Wl : HouseCalderLandform::Walls())
    {
        const FVector2D E = Wl.B - Wl.A;
        const double Den = D.X * E.Y - D.Y * E.X;
        if (FMath::Abs(Den) < 1e-9) continue;
        const FVector2D AW = Wl.A - A;
        const double T = (AW.X * E.Y - AW.Y * E.X) / Den, U = (AW.X * D.Y - AW.Y * D.X) / Den;
        if (T > 0.0 && T < 1.0 && U >= 0.0 && U <= 1.0) return true;
    }
    return false;
}

/** The walking surface under P between Top and Bottom: the first upward hit the player could stand on (walkable by angle and
 *  not an Unwalkable part: a rail's top or a post's cap is not the deck). */
bool StandZ(UWorld* W, const FVector2D& P, double Top, double Bottom, float& OutZ, FHitResult* OutHit = nullptr)
{
    FCollisionQueryParams Q = Query(false);
    for (int32 Pass = 0; Pass < 8; ++Pass)
    {
        FHitResult Hit;
        if (!W->LineTraceSingleByChannel(Hit, FVector(P, Top), FVector(P, Bottom), ECC_Pawn, Q) || !Hit.GetComponent()) return false;
        const bool bUnwalkable = Hit.GetComponent()->GetWalkableSlopeOverride().GetWalkableSlopeBehavior() == WalkableSlope_Unwalkable;
        if (Hit.ImpactNormal.Z >= .7 && !bUnwalkable)
        {
            OutZ = float(Hit.ImpactPoint.Z);
            if (OutHit) *OutHit = Hit;
            return true;
        }
        Q.AddIgnoredComponent(Hit.GetComponent());
    }
    return false;
}

/** A Calder rock or wall face (an Unwalkable part) covers P from above, over Z: P lies behind that face (inside a battered rock
 *  face's body, whose foot stands out on the low side), not in the open. */
bool CoveredByFace(UWorld* W, const FVector2D& P, float Z)
{
    FHitResult Hit;
    if (!W->LineTraceSingleByChannel(Hit, FVector(P, Z + 6000.0), FVector(P, Z), ECC_Pawn, Query(false)) || !Hit.GetComponent()) return false;
    const UPrimitiveComponent* C = Hit.GetComponent();
    return C->GetName().StartsWith(TEXT("Calder")) && C->GetWalkableSlopeOverride().GetWalkableSlopeBehavior() == WalkableSlope_Unwalkable;
}

/** (1d/1e verification) A face that stands out in front of its wall line (a battered rock face) closes the step only where its
 *  foot meets the ground. Out from the wall station P (Hi toward the high side), every 5 cm from From to 9 m: the first spot
 *  where no Calder face stands more than H cm over the ground is the foot. A ray H cm over the ground there, back toward the
 *  line and 60 cm past it, must meet a Calder part that is not the terrain (the face itself), or ground or an object that no
 *  face covers (the ground rising in the open). Under a foot hanging over falling ground the ray passes into the rock's hollow:
 *  it meets nothing, or the terrain or an object under the face: an opening under the wall. Returns 1 closed, 0 open, -1 when
 *  the ray first meets something in the open that is not Calder's (a bench, a hedge: it cannot tell). No foot within 9 m is
 *  open too. */
int32 FaceFoot(UWorld* W, const FVector2D& P, const FVector2D& Hi, float H, double From, FVector2D& OutFoot)
{
    for (double D = From; D <= 900.0; D += 5.0)
    {
        const FVector2D Q = P - Hi * D;
        const float G = Ground(Q);
        if (CoveredByFace(W, Q, G + H)) continue;
        OutFoot = Q;
        FHitResult Hit;
        if (!W->LineTraceSingleByChannel(Hit, FVector(Q, G + H), FVector(P + Hi * 60.0, G + H), ECC_Pawn, Query(false)) || !Hit.GetComponent()) return 0;
        const FString Name = Hit.GetComponent()->GetName();
        if (Name.StartsWith(TEXT("Calder")) && !Name.StartsWith(TEXT("CalderTerrain"))) return 1;
        // The terrain or another part: in the open (the ground rising before the face, an object in front of it), or under the
        // face (the ray went in under its foot)?
        const FVector2D At = FVector2D(Hit.ImpactPoint) + Hi * 2.0;
        if (CoveredByFace(W, At, float(Hit.ImpactPoint.Z) + 1.f)) return 0;
        return Name.StartsWith(TEXT("Calder")) ? 1 : -1;
    }
    OutFoot = P - Hi * 900.0;
    return 0;
}

/** Some walk is laid at Q: a road, lot, walk, sidewalk, plaza or apron of the plan, a built stair or ramp, a landform path
 *  (the pond loop, the Crow Ledge trail), Crow Ledge itself or within 3 m of an overlook. */
bool WalkAt(const FVector2D& Q)
{
    if (HouseCampusLayout::PaveKindAt(Q) != HouseCalderTerrain::EPave::Lawn) return true;
    if (HouseCalderFeatures::InStair(Q) || HouseCalderFeatures::InRamp(Q)) return true;
    for (const HouseCalderLandform::FLandformPath& Path : HouseCalderLandform::LandformPaths())
        if (Path.Points.Num() && PolyDist(Path.Points, Q, Path.bClosed) <= Path.Width * .5) return true;
    // (Within 3 m of an overlook spot on its own side of every declared wall: the strip beyond Founders' Overlook's parapet,
    // 1 m below the terrace and outside its 1.07 m guard, is not the overlook's walk.)
    for (const HouseCalderLandform::FSpot& O : HouseCalderLandform::Overlooks())
        if (FVector2D::Distance(FVector2D(O.At), Q) <= 300.0 && !CrossesWall(FVector2D(O.At), Q)) return true;
    const HouseCalderLandform::FCrowLedge& Crow = HouseCalderLandform::CrowLedge();
    return Crow.Outline.Num() >= 3 && InPoly(Crow.Outline, Q);
}

/** Something blocks the player 90 cm or more over BaseZ across the edge at P: horizontal rays at 89.5-130 cm over it from
 *  Inside cm in (against Out) to Outside cm out (along Out). */
bool RailAt(UWorld* W, const FVector2D& P, const FVector2D& Out, double Inside, double Outside, float BaseZ, float& OutHeight)
{
    for (float H = RailHeight - .5f; H <= RailTop; H += RailPitch)
    {
        FHitResult Hit;
        if (W->LineTraceSingleByChannel(Hit, FVector(P - Out * Inside, BaseZ + H), FVector(P + Out * Outside, BaseZ + H), ECC_Pawn, Query(false)))
        {
            OutHeight = H;
            return true;
        }
    }
    return false;
}

void StageRails(FState& S, const FContext& C)
{
    UWorld* W = C.W;
    const TArray<FWallSeg> AllWalls = WallSegs();
    int32 WallStations = 0, RailStations = 0, RailBad = 0, Closed = 0, Unclear = 0, WallGaps = 0;
    TArray<FString> RailWorst, GapWorst;

    // Declared walls: every built wall (step >= 3 cm) closes its step; a drop of 75 cm or more under a walk gets a rail.
    int32 ClosedBehindFace = 0, DamStations = 0, FaceFootChecks = 0, FaceFootOpen = 0;
    const bool bDamBuilt = HouseCalderWater::BuildsDamWalls();
    for (const FWallSeg& Wl : AllWalls)
    {
        const FVector2D N = Perp(Wl.Dir);
        if (bDamBuilt && Wl.Kind == HouseCalderLandform::EWallKind::Dam)
        {
            // The stone dam (HouseCalderWater builds it whole): its declared line runs along the middle of its crest walk, so its
            // drops are at the crest's two edges. Every metre, on each side: from the line out to where the walk ends (its deck,
            // HouseCalderWater::DeckAt), the drop from the walk to the ground 50-100 cm past that edge; 75 cm or more needs a rail
            // 90 cm over the walk there, and the dam's face must close the step (a ray 18 cm over the low ground from 1.2 m out
            // meets it within 90 cm of the edge).
            for (const double St : Stations(Wl.Len, 100.0, 10.0))
            {
                const FVector2D P = Wl.A + Wl.Dir * St;
                float Walk = 0.f;
                if (!InSquare(P, 5.0) || Frozen(P) || !HouseCalderWater::DeckAt(P, Walk)) continue;
                for (const double Sgn : { 1.0, -1.0 })
                {
                    const FVector2D Out = N * Sgn;
                    double Edge = 0.0;
                    float Z = 0.f;
                    while (Edge < 400.0 && HouseCalderWater::DeckAt(P + Out * (Edge + 5.0), Z)) Edge += 5.0;
                    const FVector2D E = P + Out * Edge;
                    const float LowZ = FMath::Min(Ground(E + Out * 50.0), Ground(E + Out * 100.0));
                    const float Drop = Walk - LowZ;
                    if (Drop < MinWallStep) continue;
                    ++WallStations;
                    ++DamStations;
                    FHitResult Hit;
                    const float RayZ = LowZ + FMath::Min(Drop * .5f, 18.f);
                    if (W->LineTraceSingleByChannel(Hit, FVector(E + Out * 120.0, RayZ), FVector(E - Out * 60.0, RayZ), ECC_Pawn, Query(false)))
                    {
                        if (FVector2D::DotProduct(FVector2D(Hit.ImpactPoint) - E, Out) <= 90.0) ++Closed; else ++Unclear;
                    }
                    else
                    {
                        if (++WallGaps <= 10) GapWorst.Add(FString::Printf(TEXT("dam \"%s\" (%.0f,%.0f) step %.0f"), *Wl.Name, E.X, E.Y, Drop));
                        Detail(S, TEXT("wallOpen"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f,\"%s\""), E.X, E.Y, LowZ, Drop, *Wl.Name));
                    }
                    if (Drop < RailDrop) continue;
                    ++RailStations;
                    float Height = 0.f;
                    if (RailAt(W, E, Out, 40.0, 40.0, Walk, Height)) continue;
                    if (++RailBad <= 10) RailWorst.Add(FString::Printf(TEXT("dam \"%s\" (%.0f,%.0f) drop %.0f"), *Wl.Name, E.X, E.Y, Drop));
                    Detail(S, TEXT("rail"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f,\"dam %s\""), E.X, E.Y, Walk, Drop, *Wl.Name));
                }
            }
            continue;
        }
        for (const double St : Stations(Wl.Len, 100.0, 10.0))
        {
            const FVector2D P = Wl.A + Wl.Dir * St;
            if (!InSquare(P, 5.0) || Frozen(P) || HouseCalderFeatures::InStair(P, 30.f) || HouseCalderFeatures::InRamp(P, 30.f)) continue;
            const float Z1 = Ground(P - N * 3.0), Z2 = Ground(P + N * 3.0);
            const FVector2D Hi = Z2 >= Z1 ? N : -N;
            const float High = FMath::Max(Z1, Z2), LowZ = FMath::Min(Z1, Z2), Step = High - LowZ;
            if (Step < MinWallStep) continue;
            ++WallStations;
            // A ray at the low side across the line hits the wall (no hole under the high ground); a first hit further than
            // 90 cm out on the low side (its own ground rising) cannot tell.
            const float RayZ = LowZ + FMath::Min(Step * .5f, 18.f);
            FHitResult Hit;
            FVector2D Foot = P;
            bool bFootOpen = false;
            // (Only a rock face leans out over its low side: a battered band, a cut or a ledge. A face further out than 90 cm in
            // front of a masonry wall is another wall's - an acute corner, a kerbed landing - and cannot tell.)
            const bool bRock = Wl.Kind == HouseCalderLandform::EWallKind::RockBand || Wl.Kind == HouseCalderLandform::EWallKind::RockCut
                || Wl.Kind == HouseCalderLandform::EWallKind::RockLedge;
            if (W->LineTraceSingleByChannel(Hit, FVector(P - Hi * 120.0, RayZ), FVector(P + Hi * 60.0, RayZ), ECC_Pawn, Query(false)))
            {
                const UPrimitiveComponent* HitPart = Hit.GetComponent();
                if (FVector2D::DotProduct(FVector2D(Hit.ImpactPoint) - P, Hi) >= -90.0) ++Closed;
                else if (bRock && HitPart && HitPart->GetName().StartsWith(TEXT("Calder")) && HitPart->GetWalkableSlopeOverride().GetWalkableSlopeBehavior() == WalkableSlope_Unwalkable)
                {
                    // (1d/1e verification) The first hit is a face standing more than 90 cm out (a battered foot): it closes the
                    // step only if its foot meets the ground there.
                    ++FaceFootChecks;
                    const double Out = -FVector2D::DotProduct(FVector2D(Hit.ImpactPoint) - P, Hi);
                    const int32 Foot1 = FaceFoot(W, P, Hi, FMath::Min(Step * .5f, 18.f), Out + 5.0, Foot);
                    if (Foot1 > 0) ++Closed; else if (Foot1 < 0) ++Unclear; else bFootOpen = true;
                }
                else ++Unclear;
            }
            else if (CoveredByFace(W, P - Hi * 120.0, RayZ))
            {
                // The ray started behind a rock face whose foot stands further out (a battered band: the bluff's): that face
                // closes the step if its foot, further out, meets the ground (1d/1e verification: checked, not assumed).
                if (!bRock) ++Unclear;   // (under another structure, not a battered face: it cannot tell)
                else
                {
                    ++FaceFootChecks;
                    const int32 Foot1 = FaceFoot(W, P, Hi, FMath::Min(Step * .5f, 18.f), 125.0, Foot);
                    if (Foot1 > 0) { ++Closed; ++ClosedBehindFace; } else if (Foot1 < 0) ++Unclear; else bFootOpen = true;
                }
            }
            else bFootOpen = true;
            if (bFootOpen && Foot != P)
            {
                ++FaceFootOpen;
                if (++WallGaps <= 10) GapWorst.Add(FString::Printf(TEXT("wall \"%s\" (%.0f,%.0f) step %.0f: face foot open at (%.0f,%.0f)"), *Wl.Name, P.X, P.Y, Step, Foot.X, Foot.Y));
                Detail(S, TEXT("wallOpen"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f,\"%s foot (%.0f,%.0f)\""), P.X, P.Y, LowZ, Step, *Wl.Name, Foot.X, Foot.Y));
            }
            else if (bFootOpen)
            {
                if (++WallGaps <= 10) GapWorst.Add(FString::Printf(TEXT("wall \"%s\" (%.0f,%.0f) step %.0f"), *Wl.Name, P.X, P.Y, Step));
                Detail(S, TEXT("wallOpen"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f,\"%s\""), P.X, P.Y, LowZ, Step, *Wl.Name));
            }
            if (Step < RailDrop) continue;
            bool bBeside = false;
            for (const double D : { 15.0, 40.0, 70.0, 100.0 }) bBeside = bBeside || WalkAt(P + Hi * D);
            if (!bBeside) continue;
            ++RailStations;
            float Height = 0.f;
            if (RailAt(W, P, -Hi, 70.0, 50.0, High, Height)) continue;
            if (++RailBad <= 10) RailWorst.Add(FString::Printf(TEXT("wall \"%s\" (%.0f,%.0f) drop %.0f"), *Wl.Name, P.X, P.Y, Step));
            Detail(S, TEXT("rail"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f,\"wall %s\""), P.X, P.Y, High, Step, *Wl.Name));
        }
    }

    // The built stairs' and ramps' sides: where the ground beside drops 75 cm or more under the tread, a rail 90 cm over it.
    // The samples come in threes (HouseCalderFeatures::StairSamples / RampSamples): the centre, then a point Width/2 - 25 cm
    // out to either side.
    int32 SideStations = 0, SideBad = 0;
    auto Sides = [&](const TArray<FVector>& Samples, const TCHAR* What)
    {
        for (int32 K = 0; K + 2 < Samples.Num(); K += 3)
        {
            const FVector2D Centre(Samples[K]);
            const float TreadZ = float(Samples[K].Z);
            for (int32 Side = 1; Side <= 2; ++Side)
            {
                const FVector2D Off = FVector2D(Samples[K + Side]) - Centre;
                const double HalfWidth = Off.Size() + 25.0;
                const FVector2D Out = Off.GetSafeNormal();
                if (Out.IsNearlyZero()) continue;
                const float Drop = TreadZ - Ground(Centre + Out * (HalfWidth + 50.0));
                if (Drop < RailDrop) continue;
                ++SideStations;
                float Height = 0.f;
                if (RailAt(W, Centre + Out * HalfWidth, Out, 30.0, 60.0, TreadZ, Height)) continue;
                if (++SideBad <= 10) RailWorst.Add(FString::Printf(TEXT("%s side (%.0f,%.0f) drop %.0f"), What, Centre.X, Centre.Y, Drop));
                Detail(S, TEXT("rail"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f,%s side"), Centre.X, Centre.Y, TreadZ, Drop, What));
            }
        }
    };
    TArray<FVector> StairPts, RampPts;
    HouseCalderFeatures::StairSamples(StairPts);
    HouseCalderFeatures::RampSamples(RampPts);
    Sides(StairPts, TEXT("stair"));
    Sides(RampPts, TEXT("ramp"));

    // The exported NeedsRail runs (the water structures' own drops): a rail 90 cm over the walk along every built one.
    int32 NeedStations = 0, NeedBad = 0, NeedUnbuilt = 0, NeedLowDrop = 0;
    for (const HouseCalderLandform::FRoadsideRun& Run : HouseCalderLandform::NeedsRail())
        for (int32 K = 0; K + 1 < Run.Points.Num(); ++K)
        {
            const FVector2D A = Run.Points[K], B = Run.Points[K + 1];
            const double Len = FVector2D::Distance(A, B);
            if (Len < 1.0) continue;
            const FVector2D Dir = (B - A) / Len, Out = Perp(Dir) * double(Run.Side == 0 ? 1 : Run.Side);
            for (const double St : Stations(Len, 100.0, 10.0))
            {
                const FVector2D P = A + Dir * St, In = P - Out * 30.0;
                if (!InSquare(P, 5.0)) continue;
                float Deck = 0.f;
                FHitResult Hit;
                if (!StandZ(W, In, Ground(In) + 250.0, Ground(In) - 50.0, Deck, &Hit)) { ++NeedUnbuilt; continue; }
                // Only the terrain there and level ground beyond the edge: the structure is not built (yet).
                const bool bTerrainOnly = Hit.GetComponent() && Hit.GetComponent()->GetName().StartsWith(TEXT("CalderTerrainCollision_"));
                if (bTerrainOnly && Deck - Ground(P + Out * 100.0) < 30.f) { ++NeedUnbuilt; continue; }
                // The rule is per drop: where the ground 50-100 cm past this edge lies less than 75 cm under the deck (a deck's
                // end landing on its bank), no rail is owed there.
                if (Deck - FMath::Min(Ground(P + Out * 50.0), Ground(P + Out * 100.0)) < RailDrop) { ++NeedLowDrop; continue; }
                ++NeedStations;
                float Height = 0.f;
                if (RailAt(W, P, Out, 40.0, 40.0, Deck, Height)) continue;
                if (++NeedBad <= 10) RailWorst.Add(FString::Printf(TEXT("\"%s\" (%.0f,%.0f) deck %.0f"), *Run.Name, P.X, P.Y, Deck));
                Detail(S, TEXT("rail"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f,\"%s\""), P.X, P.Y, Deck, Run.Value, *Run.Name));
            }
        }
    S.Rails = RailBad == 0 && SideBad == 0 && NeedBad == 0 ? 1 : 0;
    Say(S, FString::Printf(TEXT("CALDER_RAILS %s wallStations=%d wallDropsBesideWalks=%d wallUnrailed=%d stairRampSides=%d sideUnrailed=%d needsRailStations=%d needsRailUnrailed=%d needsRailUnbuilt=%d needsRailUnder75=%d damStations=%d worst=%s"),
        Verdict(S.Rails), WallStations, RailStations, RailBad, SideStations, SideBad, NeedStations, NeedBad, NeedUnbuilt, NeedLowDrop, DamStations, RailWorst.Num() ? *FString::Join(RailWorst, TEXT(";")) : TEXT("none")));

    // Boundary hedges (the segments on the relief): no gap of 36 cm under them (rays across the segment's middle at 5, 18 and
    // 33 cm over the ground there: all three free is a gap).
    int32 HedgeChecked = 0, HedgeGaps = 0;
    const TArray<HouseCalderLandform::FHedgeGap>& Gaps = HouseCalderLandform::HedgeGaps();
    for (const HouseCalderLandform::FHedgeSegment& Hg : HouseCalderLandform::HedgeSegments())
    {
        if (Hg.bFrozen) continue;
        const double Len = FVector2D::Distance(Hg.A, Hg.B);
        if (Len < 1.0) continue;
        const FVector2D Mid = (Hg.A + Hg.B) * .5, N = Perp((Hg.B - Hg.A) / Len);
        const bool bAlongX = FMath::Abs(Hg.B.X - Hg.A.X) >= FMath::Abs(Hg.B.Y - Hg.A.Y);
        bool bInGap = false;
        for (const HouseCalderLandform::FHedgeGap& G : Gaps)
            bInGap = bInGap || (G.Hedge == Hg.Hedge && (bAlongX ? Mid.X : Mid.Y) >= G.From && (bAlongX ? Mid.X : Mid.Y) <= G.To);
        if (bInGap || !InSquare(Mid, 5.0)) continue;
        ++HedgeChecked;
        const float LowZ = Ground(Mid);
        bool bBlocked = false;
        // (The hedges are 120 cm thick: a ray that starts inside one sees nothing. Each ray starts 40 cm outside a face and counts
        // only a hit on the hedge's own body, within 65 cm of its middle line.)
        for (const float H : { 5.f, HedgeGap * .5f, HedgeGap - 3.f })
            for (const double Sgn : { 1.0, -1.0 })
            {
                FHitResult Hit;
                if (W->LineTraceSingleByChannel(Hit, FVector(Mid - N * (100.0 * Sgn), LowZ + H), FVector(Mid + N * (100.0 * Sgn), LowZ + H), ECC_Pawn, Query(false))
                    && FMath::Abs(FVector2D::DotProduct(FVector2D(Hit.ImpactPoint) - Mid, N)) <= 65.0) bBlocked = true;
            }
        if (bBlocked) continue;
        if (++HedgeGaps <= 10) GapWorst.Add(FString::Printf(TEXT("hedge %d (%.0f,%.0f)"), Hg.Hedge, Mid.X, Mid.Y));
        Detail(S, TEXT("hedgeGap"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%d"), Mid.X, Mid.Y, LowZ, Hg.Hedge));
    }
    S.Gaps = WallGaps == 0 && HedgeGaps == 0 ? 1 : 0;
    Say(S, FString::Printf(TEXT("CALDER_GAPS %s wallStations=%d closed=%d closedBehindFace=%d faceFootChecks=%d faceFootOpen=%d unclear=%d open=%d hedgeSegments=%d hedgeGaps=%d worst=%s"),
        Verdict(S.Gaps), WallStations, Closed, ClosedBehindFace, FaceFootChecks, FaceFootOpen, Unclear, WallGaps, HedgeChecked, HedgeGaps, GapWorst.Num() ? *FString::Join(GapWorst, TEXT(";")) : TEXT("none")));
}

void StageStairs(FState& S, const FContext& C)
{
    UWorld* W = C.W;
    int32 Stairs = 0, Built = 0, Risers = 0, Bad = 0, Holes = 0, Descents = 0, FootWarn = 0, FirstSamples = 0, FirstBad = 0;
    float RiserLo = FLT_MAX, RiserHi = 0.f, GoingLo = FLT_MAX, TopLo = FLT_MAX, FootLo = FLT_MAX, FootHi = 0.f;
    float FirstLo = FLT_MAX, FirstHi = -FLT_MAX;
    TArray<FString> Worst, Warn;
    for (const HouseCalderLandform::FStair& D : HouseCalderLandform::Stairs())
    {
        const double Len = FVector2D::Distance(D.Foot, D.Head);
        if (Len < 1.0) continue;
        ++Stairs;
        const FVector2D Dir = (D.Head - D.Foot) / Len;
        // The built treads along the axis, every centimetre where HouseCalderFeatures has a tread, as the player's collision
        // has them.
        TArray<double> Ss;
        TArray<float> Zs;
        double LastOn = -1e9;
        // Include the added concrete approach footprints and their outer
        // joins. All other flights retain their original scan bounds.
        const bool bLotApproaches = D.Name.StartsWith(TEXT("walk south_lot_tier_links ")) || D.Name == TEXT("walk walk_45 stair")
            || D.Name == TEXT("walk walk_00 stair") || D.Name == TEXT("walk walk_29 stair");
        const double ProfileStart = bLotApproaches ? -220.0 : -60.0;
        const double ProfileEnd = Len + (bLotApproaches ? 220.0 : 150.0);
        for (double Sv = ProfileStart; Sv <= ProfileEnd; Sv += 1.0)
        {
            const FVector2D P = D.Foot + Dir * Sv;
            float FZ = 0.f, Z = 0.f;
            if (!HouseCalderFeatures::FloorAt(P, FZ)) { if (Ss.Num() && Sv > LastOn + 3.0) break; continue; }
            LastOn = Sv;
            // (A sample on the very edge where the tread starts - the walk's paving is cut out to the nosing's line, so a trace there
            // can slip down the seam - is not a tread: the first nosing is found by bisection below.)
            if (!Ss.Num() && !HouseCalderFeatures::FloorAt(D.Foot + Dir * (Sv - .5), FZ)) continue;
            if (!WorldZ(W, P, FZ + 40.0, FZ - 40.0, Z)) { ++Holes; continue; }
            Ss.Add(Sv);
            Zs.Add(Z);
        }
        if (Ss.Num() < 2) continue;
        ++Built;
        // The first tread's nosing and the top tread's end: where HouseCalderFeatures starts and stops having a tread (to 0.1 mm).
        double OnS = LastOn, OffS = LastOn + 1.0, StartOff = Ss[0] - 1.0, StartOn = Ss[0];
        for (int32 It = 0; It < 14; ++It)
        {
            const double MidS = (OnS + OffS) * .5, MidStart = (StartOff + StartOn) * .5;
            float FZ = 0.f;
            if (HouseCalderFeatures::FloorAt(D.Foot + Dir * MidS, FZ)) OnS = MidS; else OffS = MidS;
            if (HouseCalderFeatures::FloorAt(D.Foot + Dir * MidStart, FZ)) StartOn = MidStart; else StartOff = MidStart;
        }
        // Risers between the samples; each nosing found to 0.1 mm.
        TArray<double> Nosings;
        bool bStairBad = false;
        for (int32 K = 0; K + 1 < Ss.Num(); ++K)
        {
            const float Jump = Zs[K + 1] - Zs[K];
            if (Jump < -5.f) { ++Descents; bStairBad = true; continue; }
            if (Jump <= 5.f) continue;
            ++Risers;
            RiserLo = FMath::Min(RiserLo, Jump);
            RiserHi = FMath::Max(RiserHi, Jump);
            if (Jump < RiserMin - DimTol || Jump > RiserMax + DimTol) { bStairBad = true; Worst.Add(FString::Printf(TEXT("\"%s\" riser %.2f at %.0f"), *D.Name, Jump, Ss[K])); }
            double Lo = Ss[K], Hi = Ss[K + 1];
            for (int32 It = 0; It < 14; ++It)
            {
                const double MidS = (Lo + Hi) * .5;
                float Z = 0.f;
                if (WorldZ(W, D.Foot + Dir * MidS, Zs[K + 1] + 40.0, Zs[K] - 40.0, Z) && Z > (Zs[K] + Zs[K + 1]) * .5f) Hi = MidS; else Lo = MidS;
            }
            Nosings.Add((Lo + Hi) * .5);
        }
        const double OuterEdge = (StartOn + StartOff) * .5;
        const double FirstStairNosing = Nosings.Num() ? Nosings[0] : OuterEdge;
        bool bOuterIsNosing = true;
        if (bLotApproaches)
        {
            float Before = 0.f, After = 0.f;
            if (WorldZ(W, D.Foot + Dir * (OuterEdge - .1), Zs[0] + 10.f, Zs[0] - 120.f, Before)
                && WorldZ(W, D.Foot + Dir * (OuterEdge + .1), Zs[0] + 10.f, Zs[0] - 10.f, After)
                && FMath::Abs(After - Before) < 5.f) bOuterIsNosing = false;
        }
        // Apply the same significant-riser rule as above. A paving lip at an
        // approach's outer edge is not a stair nosing or a short stair tread.
        // Actual entry curbs and every real tread keep the original limits.
        if (Nosings.Num() && bOuterIsNosing) Nosings.Insert(OuterEdge, 0);
        for (int32 K = 0; K + 1 < Nosings.Num(); ++K)
        {
            const float Going = float(Nosings[K + 1] - Nosings[K]);
            GoingLo = FMath::Min(GoingLo, Going);
            if (Going < TreadMin - DimTol) { bStairBad = true; Worst.Add(FString::Printf(TEXT("\"%s\" going %.2f at %.0f"), *D.Name, Going, Nosings[K])); }
        }
        if (Nosings.Num())
        {
            const float Top = float(OnS - Nosings.Last());
            TopLo = FMath::Min(TopLo, Top);
            if (Top < TreadMin - DimTol) { bStairBad = true; Worst.Add(FString::Printf(TEXT("\"%s\" topTread %.2f"), *D.Name, Top)); }
        }
        // Also measure the real first vertical edge at three walking-width
        // positions. Keep the older 10 cm approach diagnostic below unchanged.
        const FVector2D FirstSide(-Dir.Y, Dir.X);
        const double FirstEdge = OuterEdge;
        for (const double Across : { -(D.Width * .5 - 25.0), 0.0, D.Width * .5 - 25.0 })
        {
            float BeforeZ = 0.f, AfterZ = 0.f;
            double ProbeEdge = FirstEdge;
            FVector2D BeforeP = D.Foot + Dir * (ProbeEdge - .1) + FirstSide * Across;
            FVector2D AfterP = D.Foot + Dir * (ProbeEdge + .1) + FirstSide * Across;
            ++FirstSamples;
            if (!WorldZ(W, BeforeP, Zs[0] + 10.f, Zs[0] - 120.f, BeforeZ)
                || !WorldZ(W, AfterP, Zs[0] + 10.f, Zs[0] - 10.f, AfterZ))
            {
                ++FirstBad;
                Worst.Add(FString::Printf(TEXT("\"%s\" first-edge floor missing across=%.1f"), *D.Name, Across));
                continue;
            }
            // A concrete approach on paving begins with a sub-5 cm lip,
            // not a stair riser. Measure the first actual nosing as well;
            // significant entry curbs still use the outer edge above.
            if (bLotApproaches && FMath::Abs(AfterZ - BeforeZ) < 5.f && Nosings.Num())
            {
                ProbeEdge = FirstStairNosing;
                BeforeP = D.Foot + Dir * (ProbeEdge - .1) + FirstSide * Across;
                AfterP = D.Foot + Dir * (ProbeEdge + .1) + FirstSide * Across;
                float Datum = 0.f;
                if (!HouseCalderFeatures::FloorAt(AfterP, Datum)
                    || !WorldZ(W, BeforeP, Datum + 40.f, Datum - 120.f, BeforeZ)
                    || !WorldZ(W, AfterP, Datum + 40.f, Datum - 120.f, AfterZ))
                {
                    ++FirstBad;
                    Worst.Add(FString::Printf(TEXT("\"%s\" first-nosing floor missing across=%.1f"), *D.Name, Across));
                    continue;
                }
            }
            const float FirstRise = AfterZ - BeforeZ;
            FirstLo = FMath::Min(FirstLo, FirstRise); FirstHi = FMath::Max(FirstHi, FirstRise);
            if (FirstRise < RiserMin - DimTol || FirstRise > RiserMax + DimTol)
            {
                ++FirstBad;
                Worst.Add(FString::Printf(TEXT("\"%s\" true first-riser %.3f across=%.1f"), *D.Name, FirstRise, Across));
            }
        }
        // The foot riser: from the walk 10 cm before the first tread (HouseCalderFeatures lifts treads 6 mm over the walk).
        float WalkZ = 0.f;
        if (WorldZ(W, D.Foot + Dir * (Ss[0] - 10.0), Zs[0] + 10.0, Zs[0] - 120.0, WalkZ))
        {
            const float Foot = Zs[0] - WalkZ;
            FootLo = FMath::Min(FootLo, Foot);
            FootHi = FMath::Max(FootHi, Foot);
            if (Foot < RiserMin - DimTol || Foot > RiserMax + FootLift + DimTol)
            {
                ++FootWarn;
                Warn.Add(FString::Printf(TEXT("\"%s\" footRiser %.2f"), *D.Name, Foot));
                const FVector2D FootProbe = D.Foot + Dir * (Ss[0] - 10.0);
                Say(S, FString::Printf(TEXT("CALDER_STAIR_FOOT INFO name=\"%s\" at=(%.2f,%.2f) firstTop=%.3f actualBefore=%.3f groundBefore=%.3f declaredSurfaceBefore=%.3f paveKind=%d"),
                    *D.Name, FootProbe.X, FootProbe.Y, Zs[0], WalkZ,
                    HouseExteriorSite::GroundHeight(FootProbe.X, FootProbe.Y),
                    HouseCalderTerrain::SurfaceZ(FootProbe.X, FootProbe.Y), int32(HouseCampusLayout::PaveKindAt(FootProbe))));
            }
        }
        Bad += bStairBad ? 1 : 0;
        Detail(S, TEXT("stair"), FString::Printf(TEXT("%.1f,%.1f,%d,%d,\"%s\""), D.Foot.X, D.Foot.Y, FMath::Max(1, Nosings.Num()), bStairBad ? 0 : 1, *D.Name));
    }

    // Ramps: the built slab along its axis, every 50 cm: 1:12 or flatter.
    int32 RampsBuilt = 0, RampBad = 0;
    float RampMax = 0.f;
    for (const HouseCalderLandform::FRamp& R : HouseCalderLandform::Ramps())
    {
        const double Len = FVector2D::Distance(R.A, R.B);
        if (Len < 1.0) continue;
        const FVector2D Dir = (R.B - R.A) / Len;
        double PrevS = -1.0;
        float PrevZ = 0.f, Steepest = 0.f;
        for (double Sv = 10.0; Sv <= Len - 10.0; Sv += 50.0)
        {
            const FVector2D P = R.A + Dir * Sv;
            float FZ = 0.f, Z = 0.f;
            if (!HouseCalderFeatures::FloorAt(P, FZ) || !WorldZ(W, P, FZ + 40.0, FZ - 40.0, Z)) { PrevS = -1.0; continue; }
            if (PrevS >= 0.0) Steepest = FMath::Max(Steepest, float(FMath::Abs(Z - PrevZ) / (Sv - PrevS)));
            PrevS = Sv;
            PrevZ = Z;
        }
        if (Steepest <= 0.f) continue;
        ++RampsBuilt;
        RampMax = FMath::Max(RampMax, Steepest);
        if (Steepest > RampMaxGrade + GradeTol) { ++RampBad; Worst.Add(FString::Printf(TEXT("ramp \"%s\" grade %.2f%%"), *R.Name, Steepest * 100.f)); }
    }
    S.StairsV = Bad == 0 && Holes == 0 && RampBad == 0 && FirstBad == 0 ? 1 : 0;
    Say(S, FString::Printf(TEXT("CALDER_FIRST_RISERS %s samples=%d min=%.3f max=%.3f bad=%d tolerance=%.3f"),
        FirstBad == 0 ? TEXT("PASS") : TEXT("FAIL"), FirstSamples, FirstLo == FLT_MAX ? 0.f : FirstLo, FirstHi, FirstBad, DimTol));
    Say(S, FString::Printf(TEXT("CALDER_STAIRS %s stairs=%d built=%d risers=%d riser=%.2f..%.2f goingMin=%.2f topTreadMin=%.2f bad=%d holes=%d descents=%d footRiser=%.2f..%.2f footRiserWarn=%d ramps=%d rampMaxGrade=%.2f%% rampBad=%d worst=%s warn=%s"),
        Verdict(S.StairsV), Stairs, Built, Risers, Risers ? RiserLo : 0.f, RiserHi, GoingLo == FLT_MAX ? 0.f : GoingLo, TopLo == FLT_MAX ? 0.f : TopLo, Bad, Holes, Descents,
        FootLo == FLT_MAX ? 0.f : FootLo, FootHi, FootWarn, RampsBuilt, RampMax * 100.f, RampBad,
        Worst.Num() ? *FString::Join(Worst, TEXT(";")) : TEXT("none"), Warn.Num() ? *FString::Join(Warn, TEXT(";")) : TEXT("none")));
}

// ------------------------------------------------------------------ 6. levels, the sight line, water, foot points, the world
void StageLevels(FState& S, const FContext& C)
{
    // The fall limit as AStoryGameMode sets it (StoryCampaign.cpp: the first PlayerStart's Z - 1200).
    TArray<AActor*> Starts;
    UGameplayStatics::GetAllActorsOfClass(C.W, APlayerStart::StaticClass(), Starts);
    const bool bStart = Starts.Num() > 0;
    S.FallLimit = bStart ? float(Starts[0]->GetActorLocation().Z - 1200.0) : HouseCalderLandform::FallLimitZ();
    // The lowest walkable ground: every lattice corner and every wall piece's vertex (the surface's lowest points), outside
    // the plan buildings' footprints.
    float Lowest = FLT_MAX;
    FVector2D LowAt = FVector2D::ZeroVector;
    const double Half = HouseCalderTerrain::Half, Cell = HouseCalderTerrain::Cell;
    for (int32 J = 0; J <= HouseCalderTerrain::Cells; ++J)
        for (int32 I = 0; I <= HouseCalderTerrain::Cells; ++I)
        {
            const FVector2D P(-Half + I * Cell, -Half + J * Cell);
            if (InFootprint(P)) continue;
            const float Z = Ground(P);
            if (Z < Lowest) { Lowest = Z; LowAt = P; }
        }
    if (HouseCalderTerrain::Enabled())
        for (int32 J = 0; J < HouseCalderTerrain::Cells; ++J)
            for (int32 I = 0; I < HouseCalderTerrain::Cells; ++I)
                if (const TArray<TArray<FVector>>* Pieces = HouseCalderLandform::WallPieces(I, J))
                    for (const TArray<FVector>& Poly : *Pieces)
                        for (const FVector& V : Poly)
                        {
                            const FVector2D P(V);
                            if (InFootprint(P) || Frozen(P)) continue;
                            const float Z = HouseExteriorSite::BaseGrade(float(P.X), float(P.Y)) + float(V.Z) * HouseCalderTerrain::ReliefScale();
                            if (Z < Lowest) { Lowest = Z; LowAt = P; }
                        }
    S.Lowest = Lowest;
    float Traced = 0.f;
    const bool bTraced = HouseCalderTerrain::Enabled() && TileZ(C, LowAt, Traced);
    const float Floor = S.FallLimit + 400.f;
    S.Levels = Lowest >= Floor ? 1 : 0;
    Say(S, FString::Printf(TEXT("CALDER_LEVELS %s fallLimit=%.1f (%s) rule=lowest>=%.1f lowestWalkable=%.2f at=(%.0f,%.0f) traced=%s exported=%.2f margin=%.1f"),
        Verdict(S.Levels), S.FallLimit, bStart ? TEXT("first PlayerStart Z - 1200") : TEXT("no PlayerStart: the exported limit"), Floor, Lowest, LowAt.X, LowAt.Y,
        bTraced ? *FString::Printf(TEXT("%.2f"), Traced) : TEXT("-"), HouseCalderLandform::LowestWalkableZ(), Lowest - Floor));
}

void StageSight(FState& S, const FContext& C)
{
    UWorld* W = C.W;
    const FVector Dial = HouseClockTowerMesh::DialCenter(1);   // the +X face: the one the frat lawn sees
    const FVector2D E2(SightEye), D2(Dial);
    const double Dist = FVector2D::Distance(E2, D2);
    // The ground (landform.py check_sightline): every 50 cm the line keeps over GroundHeight, outside the plan buildings.
    double Clear = DBL_MAX;
    FVector2D ClearAt = E2;
    const int32 Steps = int32(Dist / 50.0);
    for (int32 K = 1; K < Steps; ++K)
    {
        const double T = double(K) / Steps;
        const FVector2D P = FMath::Lerp(E2, D2, T);
        if (InFootprint(P)) continue;
        const double Gap = FMath::Lerp(SightEye.Z, Dial.Z, T) - Ground(P);
        if (Gap < Clear) { Clear = Gap; ClearAt = P; }
    }
    // The plan buildings: where the 2-D line enters one, the line stands over its top (-60 + its height + its pad).
    TArray<FString> Blocked;
    const TArray<HouseCampusLayout::FBlock>& Blocks = HouseCampusLayout::Blocks();
    for (int32 Seed = 0; Seed < Blocks.Num(); ++Seed)
    {
        const HouseCampusLayout::FBlock& B = Blocks[Seed];
        for (int32 K = 1; K < Steps; ++K)
        {
            const double T = double(K) / Steps;
            const FVector2D P = FMath::Lerp(E2, D2, T);
            if (P.X < B.X0 || P.X > B.X1 || P.Y < B.Y0 || P.Y > B.Y1) continue;
            const double Top = -60.0 + B.Height + HouseCampusLayout::BlockDZ(Seed), LineZ = FMath::Lerp(SightEye.Z, Dial.Z, T);
            if (Top >= LineZ) Blocked.Add(FString::Printf(TEXT("%s top %.0f line %.0f"), B.Id, Top, LineZ));
            break;
        }
    }
    // Every tree canopy's bounds (the instanced trees of the site, the campus, the woods and any new Calder trees): the mesh
    // bounds above the clear trunk (the lowest 30 % of the tree's height). A line through the trunk zone alone is listed.
    // (1d/1e verification) Today's line, to the dial before the tower rode up onto the knoll: a frozen tree (bit-identical to
    // today, the crc) whose canopy box today's line crosses too is today's state, which the relief cannot change and the job may
    // not touch: listed as todayCrossings, not failed. (The frozen garden oak by the frat-lawn eye: the eye stands under its
    // 18 m box, which both lines leave through its lower corner; ClockTowerReview's 7_from_the_lawn_far.png, taken from this eye,
    // shows both dials clear of its crown.) Every other crossing fails, frozen or not.
    const FVector TodayDial = Dial - FVector(0.0, 0.0, HouseCalderTerrain::TowerLift());
    const FVector TodayEnd = TodayDial - (TodayDial - SightEye).GetSafeNormal() * 10.0;
    TArray<FString> TodayHits;
    int32 Trees = 0, TrunkZoneOnly = 0;
    TArray<FString> TreeHits;
    const FVector Toward = (Dial - SightEye).GetSafeNormal();
    const FVector End = Dial - Toward * 10.0;
    for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
    {
        UInstancedStaticMeshComponent* Ism = *It;
        if (!IsValid(Ism) || Ism->GetWorld() != W || !Ism->IsRegistered() || !Ism->IsVisible() || !Ism->GetStaticMesh()) continue;
        const FString Name = Ism->GetName();
        const bool bTree = Name.Contains(TEXT("Tree"), ESearchCase::CaseSensitive) || Name.StartsWith(TEXT("ExteriorForestDistant")) || Name.StartsWith(TEXT("ExteriorWoodsPass")) || Name.StartsWith(TEXT("ExteriorAppalachian"));
        if (!bTree) continue;
        const FBox Local = Ism->GetStaticMesh()->GetBounds().GetBox();
        for (int32 I = 0; I < Ism->GetInstanceCount(); ++I)
        {
            FTransform T;
            if (!Ism->GetInstanceTransform(I, T, true)) continue;
            ++Trees;
            const FBox Box = Local.TransformBy(T);
            if (!SegmentHitsBox(SightEye, End, Box)) continue;
            FBox Canopy = Box;
            Canopy.Min.Z = Box.Min.Z + CanopyBase * (Box.Max.Z - Box.Min.Z);
            if (!SegmentHitsBox(SightEye, End, Canopy)) { ++TrunkZoneOnly; continue; }
            if (Frozen(FVector2D(T.GetLocation())) && SegmentHitsBox(SightEye, TodayEnd, Canopy))
            {
                TodayHits.Add(FString::Printf(TEXT("%s[%d](%.0f,%.0f,%.0f)"), *Name, I, T.GetLocation().X, T.GetLocation().Y, T.GetLocation().Z));
                Detail(S, TEXT("sightTreeToday"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%d,%s"), T.GetLocation().X, T.GetLocation().Y, T.GetLocation().Z, I, *Name));
                continue;
            }
            TreeHits.Add(FString::Printf(TEXT("%s[%d](%.0f,%.0f,%.0f)"), *Name, I, T.GetLocation().X, T.GetLocation().Y, T.GetLocation().Z));
            Detail(S, TEXT("sightTree"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%d,%s,box=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f),canopyFrom=%.0f"), T.GetLocation().X, T.GetLocation().Y, T.GetLocation().Z, I, *Name,
                Box.Min.X, Box.Min.Y, Box.Min.Z, Box.Max.X, Box.Max.Y, Box.Max.Z, Canopy.Min.Z));
        }
    }
    // The world itself (visibility, the tower ignored), blocker by blocker: the ground, a Calder part or a building fails;
    // anything else is listed.
    TArray<FString> WorldBad, Other;
    FCollisionQueryParams Params = Query(true);
    if (AHouseClockTower* Tower = AHouseClockTower::Find(W)) Params.AddIgnoredActor(Tower);
    for (int32 Pass = 0; Pass < 16; ++Pass)
    {
        FHitResult Hit;
        if (!W->LineTraceSingleByChannel(Hit, SightEye, End, ECC_Visibility, Params) || !Hit.GetComponent()) break;
        UPrimitiveComponent* Comp = Hit.GetComponent();
        Params.AddIgnoredComponent(Comp);
        const FString Name = Comp->GetName();
        const FString Where = FString::Printf(TEXT("%s(%.0f,%.0f,%.0f)"), *Name, Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
        if (Name.StartsWith(TEXT("Calder")) || Name == TEXT("ExteriorGrounds") || InFootprint(FVector2D(Hit.ImpactPoint), 100.0)) WorldBad.Add(Where); else Other.Add(Where);
    }
    S.Sight = Clear > 0.0 && !Blocked.Num() && !TreeHits.Num() && !WorldBad.Num() ? 1 : 0;
    Say(S, FString::Printf(TEXT("CALDER_SIGHTLINE %s eye=(%.0f,%.0f,%.0f) dial=(%.0f,%.0f,%.0f) groundClearance=%.0f at=(%.0f,%.0f) buildingsBlocking=%s trees=%d canopyBoundsHit=%d %s todayCrossings=%d %s trunkZoneOnly=%d worldBlocking=%s other=%s"),
        Verdict(S.Sight), SightEye.X, SightEye.Y, SightEye.Z, Dial.X, Dial.Y, Dial.Z, Clear, ClearAt.X, ClearAt.Y, Blocked.Num() ? *FString::Join(Blocked, TEXT(";")) : TEXT("none"),
        Trees, TreeHits.Num(), TreeHits.Num() ? *FString::Join(TArray<FString>(TreeHits.GetData(), FMath::Min(8, TreeHits.Num())), TEXT(";")) : TEXT("-"),
        TodayHits.Num(), TodayHits.Num() ? *FString::Join(TArray<FString>(TodayHits.GetData(), FMath::Min(8, TodayHits.Num())), TEXT(";")) : TEXT("-"), TrunkZoneOnly,
        WorldBad.Num() ? *FString::Join(WorldBad, TEXT(";")) : TEXT("none"), Other.Num() ? *FString::Join(Other, TEXT(";")) : TEXT("none")));
}

void StageWater(FState& S)
{
    const FWater& Wd = WaterData();
    TArray<FString> Failing;
    // Gander Pond's depths (check_water): <= 120 cm, <= 60 cm within 3 m of the bank.
    float Deepest = 0.f;
    int32 PondSamples = 0, NearDeep = 0;
    for (double X = 22800.0; X <= 27300.0 + 1e-9; X += 100.0)
        for (double Y = -16100.0; Y <= -10700.0 + 1e-9; Y += 100.0)
        {
            const FVector2D P(X, Y);
            if (!InPoly(Wd.Pond, P)) continue;
            ++PondSamples;
            const float Dpt = Wd.PondZ - Ground(P);
            Deepest = FMath::Max(Deepest, Dpt);
            if (PondEdgeDist(Wd, P) <= 300.0 && Dpt > 60.f) { ++NearDeep; Detail(S, TEXT("waterDeep"), FString::Printf(TEXT("%.1f,%.1f,%.1f,pond"), X, Y, Dpt)); }
        }
    if (Deepest > 120.f || NearDeep) Failing.Add(TEXT("pondDepth"));
    // Its banks: the ground 1.5, 6 and 12 m out from the shore (off the water, off the dam's downstream face) is over its water.
    double LowShore = DBL_MAX;
    FVector2D LowShoreAt = FVector2D::ZeroVector;
    for (int32 I = 0; I < Wd.Pond.Num(); ++I)
    {
        const FVector2D A = Wd.Pond[I], B = Wd.Pond[(I + 1) % Wd.Pond.Num()];
        const double L = FVector2D::Distance(A, B);
        if (L < 1.0) continue;
        const FVector2D Nrm((B.Y - A.Y) / L, -(B.X - A.X) / L);
        const int32 Count = int32(L / 100.0);
        for (int32 K = 0; K < Count; ++K)
        {
            const FVector2D P = A + (B - A) * (double(K) / Count);
            for (const double Dd : { 150.0, 600.0, 1200.0 })
            {
                FVector2D Q = P + Nrm * Dd;
                if (InPoly(Wd.Pond, Q)) Q = P - Nrm * Dd;
                if (InWater(Wd, Q) || (Q.Y < -15900.0 && Q.X > 23800.0 && Q.X < 26600.0)) continue;
                const double Z = Ground(Q);
                if (Z < LowShore) { LowShore = Z; LowShoreAt = Q; }
            }
        }
    }
    if (!(LowShore > Wd.PondZ)) Failing.Add(TEXT("pondBanks"));
    // Calder Branch above the pond: the ground 5 m and 15 m out both sides is over the water (the last 8 m are the pond's shore).
    double CreekWorst = DBL_MAX;
    FVector2D CreekWorstAt = FVector2D::ZeroVector;
    int32 Perched = 0;
    const double CreekLen = Wd.Creek.Num() ? double(Wd.Creek.Last().S) : 0.0;
    for (int32 K = 0; K < int32(CreekLen); K += 200)
    {
        FVector2D P, T;
        LineAt(Wd.Creek, K, P, T);
        if (!InSquare(P)) continue;
        const double Wl = WaterZAtS(Wd.Creek, K);
        for (const double Sg : { -1.0, 1.0 })
            for (const double Dd : { 500.0, 1500.0 })
            {
                const FVector2D Q = P + Perp(T) * (Dd * Sg);
                if (!InSquare(Q) || InPoly(Wd.Pond, Q) || PondEdgeDist(Wd, Q) < 800.0) continue;
                const double M = Ground(Q) - Wl;
                Perched += Dd == 1500.0 && M < 50.0 ? 1 : 0;
                if (M < CreekWorst) { CreekWorst = M; CreekWorstAt = Q; }
            }
    }
    if (CreekWorst != DBL_MAX && !(CreekWorst > 0.0)) Failing.Add(TEXT("creekBanks"));
    // The glen floor (its bed) stays at or over the design floor.
    double FloorMin = DBL_MAX;
    FVector2D FloorAtP = FVector2D::ZeroVector;
    const double GlenLen = Wd.Glen.Num() ? double(Wd.Glen.Last().S) : 0.0;
    for (int32 K = 0; K < int32(GlenLen); K += 100)
    {
        FVector2D P, T;
        LineAt(Wd.Glen, K, P, T);
        if (FMath::Abs(P.Y) > SquareHalf) continue;
        const double Z = Ground(P);
        if (Z < FloorMin) { FloorMin = Z; FloorAtP = P; }
    }
    if (FloorMin != DBL_MAX && FloorMin < DesignFloor) Failing.Add(TEXT("glenFloor"));
    // Every point in the water reaches a bank at 1:3 or flatter: the beds across the glen and the creek, 40 cm at a time.
    double SteepBed = 0.0;
    FVector2D SteepAt = FVector2D::ZeroVector;
    auto Across = [&](const FWaterPts& Line, double LineLen, int32 Every, double U0, double U1, bool bGlen)
    {
        for (int32 K = 0; K < int32(LineLen); K += Every)
        {
            FVector2D P, T;
            LineAt(Line, K, P, T);
            if (FMath::Abs(P.Y) > SquareHalf - 100.0 || (bGlen && P.Y > DamLineY(Wd, P.X) - 200.0)) continue;
            for (double U = U0; U <= U1 + 1e-9; U += 40.0)
            {
                const FVector2D QA = P + Perp(T) * U, QB = P + Perp(T) * (U + 40.0);
                if (bGlen && ((QA.Y - Wd.FallsY) * (QB.Y - Wd.FallsY) <= 0.0 || (QA.Y - Wd.CascadeY) * (QB.Y - Wd.CascadeY) <= 0.0)) continue;
                if (!InWater(Wd, QA) && !InWater(Wd, QB)) continue;
                const double Sl = FMath::Abs(Ground(QA) - Ground(QB)) / 40.0;
                if (Sl > SteepBed) { SteepBed = Sl; SteepAt = QA; }
            }
        }
    };
    Across(Wd.Glen, GlenLen, 200, -900.0, 860.0, true);
    Across(Wd.Creek, CreekLen, 400, -300.0, 260.0, false);
    if (SteepBed > 1.0 / 3.0 + 0.01) Failing.Add(TEXT("bedSlope"));
    // Depths and banks of the glen, the creek and the race, as landform.py defines them (the water by latitude; the distance to
    // a bank = the half-width there less the distance to the centreline, and in the plunge pool also the rock house shelf's
    // wet edge): <= 120 cm, and <= 60 cm within 3 m of a bank. The banks hold the water: 25-150 cm past the water's edge the
    // ground stands no more than 3 cm (the lattice's tolerance) under it. A dry floor 2-5 m out under the water is listed.
    enum class EChan : uint8 { Glen, Creek, Race };
    int32 ChannelSamples = 0, ChannelDeep = 0, ChannelNearDeep = 0, BankSides = 0, BankLow = 0, Swales = 0;
    float ChannelDeepest = 0.f, LowestBank = FLT_MAX, LowestSwale = FLT_MAX;
    FVector2D LowestBankAt = FVector2D::ZeroVector;
    auto WaterHere = [&Wd](EChan Kind, const FVector2D& Q, double& OutZ, double& OutToBank) -> bool
    {
        if (!InWater(Wd, Q)) return false;
        if (Kind == EChan::Race)
        {
            OutZ = Wd.PondZ;
            OutToBank = Wd.RaceHalf - SegDist(Q, Wd.Race[0], Wd.Race[1]);
            return true;
        }
        const bool bGlen = Kind == EChan::Glen;
        OutZ = WaterZAtY(bGlen ? Wd.Glen : Wd.Creek, Q.Y);
        OutToBank = HalfWidthAtY(bGlen ? Wd.Glen : Wd.Creek, Q.Y) - PolyDist(bGlen ? Wd.GlenLine : Wd.CreekLine, Q, false);
        if (bGlen && Q.Y < Wd.WetY && Q.Y > Wd.CascadeY && Q.X >= Wd.ShelfX0 && Q.X <= Wd.ShelfX1) OutToBank = FMath::Min(OutToBank, double(Wd.WetY) - Q.Y);
        return true;
    };
    auto Section = [&](EChan Kind, const FVector2D& P, const FVector2D& T, double Hw, const TCHAR* What)
    {
        const FVector2D N = Perp(T);
        for (double U = -Hw; U <= Hw + 1e-9; U += 25.0)
        {
            const FVector2D Q = P + N * U;
            double Wq = 0.0, ToBank = 0.0;
            if (!InSquare(Q) || !WaterHere(Kind, Q, Wq, ToBank)) continue;
            const float Dpt = float(Wq) - Ground(Q);
            if (Dpt <= 0.f) continue;
            ++ChannelSamples;
            ChannelDeepest = FMath::Max(ChannelDeepest, Dpt);
            if (Dpt > 120.f) { ++ChannelDeep; Detail(S, TEXT("waterDeep"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%s"), Q.X, Q.Y, Dpt, What)); }
            else if (ToBank <= 300.0 && Dpt > 60.f) { ++ChannelNearDeep; Detail(S, TEXT("waterNearBank"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%s,%.0f"), Q.X, Q.Y, Dpt, What, ToBank)); }
        }
        const double Wz = Kind == EChan::Race ? double(Wd.PondZ) : WaterZAtY(Kind == EChan::Glen ? Wd.Glen : Wd.Creek, P.Y);
        // Off this water's own reach: across a ledge or the dam line (the glen), past the dam line (the race), or water again.
        auto Off = [&Wd, Kind, &P](const FVector2D& Q)
        {
            if (!InSquare(Q) || InWater(Wd, Q)) return true;
            if (Kind == EChan::Race) return Q.Y < DamLineY(Wd, Q.X) + 50.0;
            if (Kind != EChan::Glen) return false;
            return (Q.Y - Wd.FallsY) * (P.Y - Wd.FallsY) < 0.0 || (Q.Y - Wd.CascadeY) * (P.Y - Wd.CascadeY) < 0.0 || Q.Y > DamLineY(Wd, Q.X) - 100.0;
        };
        for (const double Sg : { -1.0, 1.0 })
        {
            // A bank: within 1.5 m past the water's edge the ground rises to the water (less the lattice's 3 cm).
            float High = -FLT_MAX;
            FVector2D HighAt = P;
            for (const double O : { 25.0, 50.0, 100.0, 150.0 })
            {
                const FVector2D Q = P + N * (Sg * (Hw + O));
                if (Off(Q)) continue;
                const float Z = Ground(Q);
                if (Z > High) { High = Z; HighAt = Q; }
            }
            if (High != -FLT_MAX)
            {
                ++BankSides;
                if (High - float(Wz) < LowestBank) { LowestBank = High - float(Wz); LowestBankAt = HighAt; }
                if (float(Wz) - High > 3.f) { ++BankLow; Detail(S, TEXT("waterOverBank"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%s"), HighAt.X, HighAt.Y, High - float(Wz), What)); }
            }
            // The dry floor beyond it (INFO): 2-5 m out, more than 3 cm under the water.
            for (double O = 200.0; O <= 500.0 + 1e-9; O += 50.0)
            {
                const FVector2D Q = P + N * (Sg * (Hw + O));
                if (Off(Q)) continue;
                const float Over = Ground(Q) - float(Wz);
                LowestSwale = FMath::Min(LowestSwale, Over);
                if (Over < -3.f) { ++Swales; break; }
            }
        }
    };
    for (int32 K = 0; K + 1 < Wd.Glen.Num(); ++K)
    {
        const HouseCalderLandform::FWaterPoint& G = Wd.Glen[K];
        const double Y = G.At.Y;
        if (!InSquare(G.At) || Y > DamLineY(Wd, G.At.X) - 200.0 || FMath::Abs(Y - Wd.FallsY) < 150.0 || FMath::Abs(Y - Wd.CascadeY) < 150.0) continue;
        if (Y > Wd.CascadeY && Y < Wd.FallsY && Y > Wd.WetY) continue;   // the rock house shelf: dry
        Section(EChan::Glen, G.At, (Wd.Glen[K + 1].At - G.At).GetSafeNormal(), G.HalfWidth, TEXT("glen"));
    }
    for (int32 K = 0; K < int32(CreekLen); K += 100)
    {
        FVector2D P, T;
        LineAt(Wd.Creek, K, P, T);
        if (!InSquare(P, 100.0) || PondEdgeDist(Wd, P) < 800.0 || InPoly(Wd.Pond, P)) continue;
        Section(EChan::Creek, P, T, Wd.CreekHalf, TEXT("creek"));
    }
    if (Wd.Race.Num() >= 2)
    {
        const double RaceLen = FVector2D::Distance(Wd.Race[0], Wd.Race[1]);
        const FVector2D T = (Wd.Race[1] - Wd.Race[0]).GetSafeNormal();
        for (const double St : Stations(RaceLen, 50.0, 25.0))
        {
            const FVector2D P = Wd.Race[0] + T * St;
            if (!InPoly(Wd.Pond, P) && P.Y > DamLineY(Wd, P.X) + 50.0) Section(EChan::Race, P, T, Wd.RaceHalf, TEXT("race"));
        }
    }
    if (ChannelDeep || ChannelNearDeep) Failing.Add(TEXT("channelDepth"));
    if (BankLow) Failing.Add(TEXT("channelBanks"));
    // Compare the public wildlife/placement query with the independently baked
    // water levels, especially the lower outlet just beyond the dam. A query
    // returning the pond's height there must not pass the terrain review.
    int32 QuerySamples = 0, QueryFailures = 0;
    float QueryWorst = 0.f;
    auto CheckQuery = [&](const FVector2D& P, float Expected)
    {
        float DeckZ = 0.f;
        if (!InSquare(P, 100.0) || Ground(P) >= Expected - .5f) return;
        bool bUnderRoad = false;
        for (const HouseCalderLandform::FWaterCrossing& Crossing : HouseCalderLandform::WaterCrossings())
        {
            if (Crossing.Kind != HouseCalderLandform::EWaterCrossing::RoadBridge) continue;
            const FVector2D Axis = Crossing.Axis.GetSafeNormal(), Side(-Axis.Y, Axis.X), Local = P - Crossing.Centre;
            const double Along = FVector2D::DotProduct(Local, Axis), Across = FVector2D::DotProduct(Local, Side);
            bUnderRoad |= Along >= -Crossing.Back && Along <= Crossing.On && FMath::Abs(Across) <= Crossing.Width * .5 + 40.0;
        }
        // Keep these pre-existing water stations when the new bridge covers
        // them. Placement must reject its deck while the water and bed remain
        // physically present below it; the road review checks that lower layer.
        if (!bUnderRoad && HouseCalderWater::DeckAt(P, DeckZ)) return;
        ++QuerySamples;
        float Actual = 0.f;
        const bool bWet = HouseCalderWater::WaterAt(P, Actual);
        const float Error = bUnderRoad ? (bWet ? FLT_MAX : 0.f) : (bWet ? FMath::Abs(Actual - Expected) : FLT_MAX);
        if (bWet) QueryWorst = FMath::Max(QueryWorst, Error);
        if (Error > 3.f)
        {
            ++QueryFailures;
            Detail(S, TEXT("waterQuery"), FString::Printf(TEXT("%.1f,%.1f,expected=%.1f,actual=%.1f,wet=%d"), P.X, P.Y, Expected, Actual, int32(bWet)));
        }
    };
    for (const HouseCalderLandform::FWaterPoint& G : Wd.Glen)
        if (G.At.Y < DamLineY(Wd, G.At.X) - 200.0 && FMath::Abs(G.At.Y - Wd.FallsY) > 3.0 && FMath::Abs(G.At.Y - Wd.CascadeY) > 3.0)
            CheckQuery(G.At, G.WaterZ);
    for (const HouseCalderLandform::FWaterPoint& G : Wd.Creek)
        if (!InPoly(Wd.Pond, G.At) && PondEdgeDist(Wd, G.At) > 800.0) CheckQuery(G.At, G.WaterZ);
    if (!QuerySamples || QueryFailures) Failing.Add(TEXT("waterQueries"));
    Say(S, FString::Printf(TEXT("CALDER_WATER_QUERY %s samples=%d failures=%d worstCm=%.3f"), QuerySamples > 0 && QueryFailures == 0 ? TEXT("PASS") : TEXT("FAIL"), QuerySamples, QueryFailures, QueryWorst));
    S.WaterV = Failing.Num() ? 0 : 1;
    Say(S, FString::Printf(TEXT("CALDER_WATER_REVIEW %s pondZ=%.1f pondSamples=%d pondDeepest=%.1f pondDeeperThan60NearBank=%d pondLowestShore=%.1f at=(%.0f,%.0f) creekLowestBankOverWater=%.1f at=(%.0f,%.0f) creekPerched=%d glenFloorMin=%.1f at=(%.0f,%.0f) rule>=%.0f steepestBed=1:%.1f at=(%.0f,%.0f) channelSamples=%d channelDeepest=%.1f over120=%d over60NearBank=%d bankSides=%d waterOverBank=%d lowestBankOverWater=%.1f at=(%.0f,%.0f) dryFloorUnderWater=%d lowestDryFloorOverWater=%.1f%s"),
        Verdict(S.WaterV), Wd.PondZ, PondSamples, Deepest, NearDeep, LowShore, LowShoreAt.X, LowShoreAt.Y, CreekWorst, CreekWorstAt.X, CreekWorstAt.Y, Perched,
        FloorMin, FloorAtP.X, FloorAtP.Y, DesignFloor, 1.0 / FMath::Max(SteepBed, 1e-6), SteepAt.X, SteepAt.Y, ChannelSamples, ChannelDeepest, ChannelDeep, ChannelNearDeep, BankSides, BankLow,
        LowestBank == FLT_MAX ? 0.f : LowestBank, LowestBankAt.X, LowestBankAt.Y, Swales, LowestSwale == FLT_MAX ? 0.f : LowestSwale, Failing.Num() ? *(TEXT(" failing=") + FString::Join(Failing, TEXT(","))) : TEXT("")));
}

void StageFeet(FState& S, const FContext& C)
{
    // The surface under a foot: the terrain collision (or today's ground mesh in a frozen zone), a built stair or ramp tread,
    // or the paved surface the plan lays there: whichever lies nearest the placer's own reading, within 15 cm.
    auto Measure = [&C](const FVector& Foot, float PlacerZ, float& OutZ) -> bool
    {
        const FVector2D P(Foot);
        float Best = 0.f, BestErr = FLT_MAX, Z = 0.f;
        auto Consider = [&Best, &BestErr, PlacerZ](float Cand) { const float E = FMath::Abs(Cand - PlacerZ); if (E < BestErr) { BestErr = E; Best = Cand; } };
        if (TileZ(C, P, Z)) Consider(Z);
        else if (C.Grounds)
        {
            FHitResult Hit;
            if (C.Grounds->LineTraceComponent(Hit, FVector(P, PlacerZ + 300.0), FVector(P, PlacerZ - 300.0), Query(true))) Consider(float(Hit.ImpactPoint.Z));
        }
        if (HouseCalderFeatures::FloorAt(P, Z)) Consider(Z);
        if (HouseCalderWater::DeckAt(P, Z)) Consider(Z);   // a deck the water builds (the dam's walk, the arch, the causeway, the dock, the footbridge)
        Consider(HouseCalderTerrain::SurfaceZ(float(P.X), float(P.Y)));
        if (BestErr > 15.f) return false;
        OutZ = Best;
        return true;
    };
    const HouseCalderFootPoints::FFootReport R = HouseCalderFootPoints::Check(Measure, 20);
    S.FootPoints = R.Points;
    S.Feet = R.Pass() ? 1 : 0;
    FString ByWho;
    for (const TPair<FName, FIntPoint>& It : R.ByWho) ByWho += FString::Printf(TEXT("%s%s:%d/%d"), ByWho.IsEmpty() ? TEXT("") : TEXT(","), *It.Key.ToString(), It.Value.Y, It.Value.X);
    Say(S, FString::Printf(TEXT("CALDER_FOOTPOINTS %s points=%d placers=%d bad=%d unmeasured=%d gap=%.2f..%.2f rule=%.0f..+%.0f badByPlacer=%s"),
        Verdict(S.Feet), R.Points, R.Placers, R.Bad, R.Unmeasured, R.LowestGap, R.HighestGap, HouseCalderFootPoints::MinGap, HouseCalderFootPoints::MaxGap, ByWho.IsEmpty() ? TEXT("none") : *ByWho));
    for (int32 K = 0; K < R.Worst.Num(); ++K)
    {
        const HouseCalderFootPoints::FFootResult& F = R.Worst[K];
        Say(S, FString::Printf(TEXT("CALDER_FOOTPOINTS_WORST %d who=%s at=(%.0f,%.0f,%.1f) surface=%.1f placerSurface=%.1f gap=%.2f measured=%d %s"),
            K + 1, *F.Point.Who.ToString(), F.Point.Foot.X, F.Point.Foot.Y, F.Point.Foot.Z, F.MeasuredZ, F.Point.SurfaceZ, F.Gap, int32(F.bMeasured), F.Outside > 0.f ? TEXT("BAD") : TEXT("ok")));
        Detail(S, TEXT("footpoint"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.2f,%s"), F.Point.Foot.X, F.Point.Foot.Y, F.Point.Foot.Z, F.Gap, *F.Point.Who.ToString()));
    }
}

/** The tree crc this run logged (HouseWoodsPass::RecordOriginalTrees: WOODS_PASS_ORIGINAL_TREES ... crc=), read back from
 *  the log file; empty when the log has no such line. */
FString LoggedTreeCrc()
{
    if (GLog) GLog->Flush();
    const FString Path = FPlatformOutputDevices::GetAbsoluteLogFilename();
    FString Text;
    if (Path.IsEmpty() || !FFileHelper::LoadFileToString(Text, *Path)) return FString();
    const int32 Line = Text.Find(TEXT("WOODS_PASS_ORIGINAL_TREES"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    if (Line == INDEX_NONE) return FString();
    const int32 At = Text.Find(TEXT("crc="), ESearchCase::CaseSensitive, ESearchDir::FromStart, Line);
    return At == INDEX_NONE ? FString() : Text.Mid(At + 4, 8);
}

void StageWorld(FState& S)
{
    S.Crc = LoggedTreeCrc();
    const bool bLights = S.Lights <= MaxLights(), bCrc = S.Crc.IsEmpty() || S.Crc == ExpectedTreeCrc;
    S.WorldV = bLights && bCrc ? 1 : 0;
    Say(S, FString::Printf(TEXT("CALDER_WORLD %s lightsOutsideHouse=%d rule<=%d treeCrc=%s expected=%s terrainHash=%08x collisionReadyS=%.1f"),
        Verdict(S.WorldV), S.Lights, MaxLights(), S.Crc.IsEmpty() ? TEXT("not-in-log") : *S.Crc, ExpectedTreeCrc, HouseCalderTerrain::Hash(), S.ReadyAt >= 0.0 ? S.ReadyAt - S.Start : -1.0));
}

/** Conservative design information only, never a substitute for the P8 image.
 *  Roof ridges come from the protected house's three HipRoof calls. Determine
 *  the eye height that would clear actual instantiated canopy bounds on each
 *  bearing, so subsequent ledge changes are based on the world being drawn. */
void P8RoofInformation(FState& S, UWorld* W)
{
    FVector Eye(1500.0, 26550.0, 0.0);
    Eye.Z = Ground(FVector2D(Eye)) + EyeHeight;
    const FVector Roofs[] = { FVector(-2410.0, 0.0, 1070.0), FVector(-30.0, -30.0, 1066.4), FVector(2350.0, 0.0, 1070.0) };
    double Required[3] = { 0.0, 0.0, 0.0 };
    int32 Crossings[3] = { 0, 0, 0 };
    FString Worst[3];
    for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
    {
        UInstancedStaticMeshComponent* Ism = *It;
        if (!IsValid(Ism) || Ism->GetWorld() != W || !Ism->IsRegistered() || !Ism->IsVisible() || !Ism->GetStaticMesh()) continue;
        const FString Name = Ism->GetName();
        if (!Name.Contains(TEXT("Tree")) && !Name.StartsWith(TEXT("ExteriorForestDistant")) && !Name.StartsWith(TEXT("ExteriorWoodsPass"))) continue;
        const FBox Local = Ism->GetStaticMesh()->GetBounds().GetBox();
        for (int32 Instance = 0; Instance < Ism->GetInstanceCount(); ++Instance)
        {
            FTransform T;
            if (!Ism->GetInstanceTransform(Instance, T, true)) continue;
            FBox Canopy = Local.TransformBy(T);
            Canopy.Min.Z += CanopyBase * (Canopy.Max.Z - Canopy.Min.Z);
            for (int32 Roof = 0; Roof < 3; ++Roof)
            {
                double Near = 0.0, Far = 1.0;
                bool Crosses = true;
                const FVector Delta = Roofs[Roof] - Eye;
                for (int32 Axis = 0; Axis < 2; ++Axis)
                {
                    if (FMath::Abs(Delta[Axis]) < 1e-6)
                    {
                        if (Eye[Axis] < Canopy.Min[Axis] || Eye[Axis] > Canopy.Max[Axis]) { Crosses = false; break; }
                        continue;
                    }
                    const double A = (Canopy.Min[Axis] - Eye[Axis]) / Delta[Axis];
                    const double B = (Canopy.Max[Axis] - Eye[Axis]) / Delta[Axis];
                    Near = FMath::Max(Near, FMath::Min(A, B));
                    Far = FMath::Min(Far, FMath::Max(A, B));
                    if (Near > Far) { Crosses = false; break; }
                }
                if (!Crosses || Far <= 0.0 || Near >= 1.0) continue;
                Crossings[Roof] += SegmentHitsBox(Eye, Roofs[Roof], Canopy) ? 1 : 0;
                const double At = FMath::Clamp(Far, 0.0, .999);
                const double Height = (Canopy.Max.Z + 5.0 - Roofs[Roof].Z * At) / (1.0 - At);
                if (Height > Required[Roof])
                {
                    Required[Roof] = Height;
                    Worst[Roof] = FString::Printf(TEXT("%s[%d]top=%.0f"), *Name, Instance, Canopy.Max.Z);
                }
            }
        }
    }
    for (int32 Roof = 0; Roof < 3; ++Roof)
        Say(S, FString::Printf(TEXT("CALDER_P8_ROOF INFO ridge=%d eyeZ=%.1f target=%s canopyCrossings=%d conservativeClearEyeZ=%.1f worst=%s visualAcceptanceStillRequired=1"),
            Roof, Eye.Z, *Roofs[Roof].ToString(), Crossings[Roof], Required[Roof], *Worst[Roof]));
}

// ------------------------------------------------------------------ the tick
void Tick(UWorld* W, ELevelTick, float)
{
    if (!W || !W->IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("CalderTerrainReview"))) return;
    if (UGameplayStatics::GetCurrentLevelName(W, true) != TEXT("Chapter01_House")) return;
    FState& S = State();
    const FString Dir = FPaths::ProjectSavedDir() / TEXT("CalderTerrainReview");
    if (S.World.Get() != W)
    {
        S = FState();
        S.World = W;
        S.Start = W->GetTimeSeconds();
        for (const TCHAR* File : { TEXT("result.txt"), TEXT("review.txt"), TEXT("skyline.csv"), TEXT("details.csv"), TEXT("frozen_grid.csv") })
            IFileManager::Get().Delete(*(Dir / File), false, true, true);
    }
    if (S.bDone) return;
    AHouseExteriorDetail* Exterior = nullptr;
    for (TActorIterator<AHouseExteriorDetail> It(W); It; ++It) Exterior = *It;
    if (!Exterior) return;
    if (S.ReadyAt < 0.0 && HouseCalderTerrain::CollisionReady(W)) S.ReadyAt = W->GetTimeSeconds();
    if (W->GetTimeSeconds() - S.Start < 8.0 || S.ReadyAt < 0.0) return;

    FContext C;
    C.W = W;
    C.Exterior = Exterior;
    Gather(C);
    const bool bTerrain = HouseCalderTerrain::Enabled();
    const double T0 = FPlatformTime::Seconds();
    const int32 Stage = S.Stage++;
    switch (Stage)
    {
    case 0:
        IFileManager::Get().MakeDirectory(*Dir, true);
        S.Began = T0;
        StageBase(S, C, Dir);
        break;
    case 1: if (bTerrain) StageSkyline(S); break;
    case 2: if (bTerrain) StageCracks(S, C); break;
    case 3: case 4: case 5: case 6: if (bTerrain) StageSlopesGrid(S, C, Stage - 3); break;
    case 7: if (bTerrain) StageSlopesWalls(S, C); break;
    case 8: if (bTerrain) StageRails(S, C); break;
    case 9: if (bTerrain) StageStairs(S, C); break;
    case FinalStage:
        StageLevels(S, C);
        if (bTerrain) { StageSight(S, C); StageWater(S); P8RoofInformation(S, W); }
        StageFeet(S, C);
        StageWorld(S);
        break;
    default: break;
    }
    UE_LOG(LogTemp, Display, TEXT("CALDER_TERRAIN_REVIEW stage=%d ms=%.0f"), Stage, (FPlatformTime::Seconds() - T0) * 1000.0);
    if (Stage < FinalStage) return;

    S.bDone = true;
    const int32 Verdicts[] = { S.Base, S.Skyline, S.Cracks, S.Slopes, S.Rails, S.Gaps, S.StairsV, S.Levels, S.Sight, S.WaterV, S.Feet, S.WorldV };
    bool bOK = true;
    for (const int32 V : Verdicts) bOK &= V != 0;
    const FString Line = FString::Printf(TEXT("%s %s skyline=%s cracks=%s slopes=%s rails=%s gaps=%s stairs=%s levels=%s sightline=%s water=%s footpoints=%s world=%s treeCrc=%s fallLimit=%.1f lowestWalkable=%.2f footPoints=%d collisionReadyS=%.1f reviewMs=%.0f"),
        bOK ? TEXT("PASS") : TEXT("FAIL"), *S.BaseLine, Verdict(S.Skyline), Verdict(S.Cracks), Verdict(S.Slopes), Verdict(S.Rails), Verdict(S.Gaps), Verdict(S.StairsV),
        Verdict(S.Levels), Verdict(S.Sight), Verdict(S.WaterV), Verdict(S.Feet), Verdict(S.WorldV), S.Crc.IsEmpty() ? TEXT("-") : *S.Crc, S.FallLimit, S.Lowest, S.FootPoints,
        S.ReadyAt - S.Start, (FPlatformTime::Seconds() - S.Began) * 1000.0);
    UE_LOG(LogTemp, Display, TEXT("CALDER_TERRAIN_REVIEW %s"), *Line);
    FFileHelper::SaveStringToFile(Line + TEXT("\n"), *(Dir / TEXT("result.txt")));
    FFileHelper::SaveStringToFile(FString::Join(S.Lines, TEXT("\n")) + TEXT("\nCALDER_TERRAIN_REVIEW ") + Line + TEXT("\n"), *(Dir / TEXT("review.txt")));
    if (!S.Skycsv.IsEmpty()) FFileHelper::SaveStringToFile(S.Skycsv, *(Dir / TEXT("skyline.csv")));
    FFileHelper::SaveStringToFile(TEXT("section,x,y,z/value,...\n") + S.Details, *(Dir / TEXT("details.csv")));
    if (!bOK || !SharedRun()) FPlatformMisc::RequestExitWithStatus(false, bOK ? 0 : 1);
}

struct FRegistration { FRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick); } };
FRegistration Registration;
}
#endif
