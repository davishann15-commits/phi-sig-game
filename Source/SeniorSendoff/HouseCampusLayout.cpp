// The Calder State campus plan: data and builders (see HouseCampusLayout.h and SourceArt/CampusMaster/PLAN.md).
//
// Profile (the same as Fraternity Park Dr): roads and lots at street level (-72) with the terrain cut away under them,
// 15 cm curbs, and sidewalks, walks and plazas at -57 just above the lawn (-60). Curb ramps cut down to the road at every
// crosswalk, and walks ramp down across the drives they cross. Every road edge that meets lawn gets a curb (found
// automatically), so no slit opens between a road and the terrain. Where a road curves, the terrain over its bounding
// square is cut and refilled here with lawn in the terrain's own material and mapping.
//
// On the Calder terrain those levels are offsets over the one ground (HouseExteriorSite::GroundHeight): paving tops come from
// HouseCalderTerrain::SurfaceZ (road -12, walk +3), the rest are today's datums carried onto the ground, and the tower plaza
// rides HouseClockTowerSite::Lift(). At zero relief the ground is exactly -60, so every height is exactly today's.
//
// Where the relief under a piece is not 0 it is draped (HouseCalderDrape, step 1c): roads, walks, sidewalks, curb ramps, curbs,
// road paint, the loop island and the lawn fills follow the ground at every point. The plazas, the tower plaza and the lots are
// level by design and stay flat at their own height (the landform brings the ground to them), and so do the seat wall and what
// lies on a lot (stall lines, wheel stops, islands: FlatPaveTop). Wherever the relief round a piece is 0 (all of today), or it
// straddles a frozen zone's edge or lies wholly inside one, it is built exactly as before (FFooting::Drape); paint and the
// median on a road kept flat that way (Calder Dr, the west road) stay flat on it too (FlatPaveTop).
#include "HouseCampusLayout.h"
#include "HouseCampusInteriors.h"
#include "HouseCalderDrape.h"
#include "HouseCalderRoads.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderLandform.h"
#include "HouseCalderTerrain.h"
#include "HouseClockTowerMesh.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusFall.h"
#include "HouseCampusFallKit.h"
#include "HouseSurroundingsPolish.h"
#include "Engine/Texture2D.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace HouseCampusLayoutImpl
{
// Today's flat datums. On the Calder terrain each height is its offset over the ground there (road -12, walk and curb +3,
// slab bottoms -40, plaza bases -5): see FFooting below.
constexpr float Ground = -60.f, RoadTop = -72.f, WalkTop = -57.f, CurbTop = -57.f, Bottom = -100.f, CurbW = 30.f, CheekW = 15.f;

/** A straight paved run (centre line A-B, full width W). bFall: the homecoming dressing paves it while it is on. */
struct FRun { FVector2D A, B; float W; bool bFall; };
/** An annulus sector; angles in degrees from +X toward +Y. */
struct FArc { FVector2D C; float R0, R1, A0, A1; };
/** A sloped slab (curb ramp or drive crossing): LowSide is the edge at road level. 0 = -X, 1 = +X, 2 = -Y, 3 = +Y. */
struct FWedge { FBox2D R; int32 LowSide; bool bOnStreet; };
struct FLot { FBox2D Area; TArray<FVector2D> Islands; bool bGravel = false; };
struct FStallRow { float Y0, Y1, X0, X1, Yaw; };

FBox2D Rect(float X0, float X1, float Y0, float Y1) { return FBox2D(FVector2D(X0, Y0), FVector2D(X1, Y1)); }

// ------------------------------------------------------------------ the plan (SourceArt/CampusMaster/plan/net.py checks it)
const TArray<HouseCampusLayout::FBlock>& BlockTable()
{
    using EK = HouseCampusLayout::EKind;
    static const TArray<HouseCampusLayout::FBlock> Table = {
        { TEXT("LIB"),  TEXT("MERCER LIBRARY"),        EK::Library,     'A', -19000.f, -15600.f, -13300.f,  -8300.f, 2120.f, 1, TEXT("EW") },
        { TEXT("MAIN"), TEXT("OLD MAIN"),              EK::Admin,       'B', -15200.f, -11400.f,  -9700.f,  -7100.f, 2540.f, 0, TEXT("NS") },
        { TEXT("HAR"),  TEXT("HARLOW HALL"),           EK::Dorm,        'A', -10600.f,  -6800.f,  -9450.f,  -7100.f, 2540.f, 1, TEXT("N") },
        { TEXT("HLTH"), TEXT("STUDENT HEALTH CENTER"), EK::Health,      'C',  -4200.f,  -1800.f,  -8700.f,  -7100.f, 1260.f, 2, TEXT("NW") },
        { TEXT("COM"),  TEXT("THE COMMONS"),           EK::Dining,      'B',  11000.f,  14200.f, -10000.f,  -7100.f, 1300.f, 2, TEXT("WN") },
        { TEXT("ASH"),  TEXT("ASHBY HALL"),            EK::Dorm,        'B',  14700.f,  18900.f, -10400.f,  -7100.f, 2540.f, 0, TEXT("NW") },
        { TEXT("SCI"),  TEXT("HOLLIS SCIENCE HALL"),   EK::Science,     'A', -15200.f, -10600.f, -17400.f, -14200.f, 2540.f, 2, TEXT("N") },
        { TEXT("HUM"),  TEXT("CALDER HALL"),           EK::Humanities,  'B', -19000.f, -15800.f, -18600.f, -14400.f, 2120.f, 0, TEXT("NW") },
        { TEXT("UNI"),  TEXT("GOOSE UNION"),           EK::Union,       'A',  -8200.f,  -4300.f, -14600.f, -10700.f, 1700.f, 1, TEXT("WNES") },
        { TEXT("KER"),  TEXT("KERR HALL"),             EK::Classroom,   'B',  11000.f,  14400.f, -15400.f, -11400.f, 2540.f, 2, TEXT("W") },
        { TEXT("FLD"),  TEXT("VARSITY FIELDHOUSE"),    EK::Athletics,   'C',  15000.f,  18900.f, -17000.f, -12200.f, 1700.f, 0, TEXT("NW") },
        { TEXT("ART"),  TEXT("WHITCOMB FINE ARTS"),    EK::Arts,        'B', -13250.f,  -9050.f, -21400.f, -18400.f, 1700.f, 1, TEXT("NE") },
        { TEXT("MUS"),  TEXT("BAND AND MUSIC HALL"),   EK::Music,       'C',  -7750.f,  -4150.f, -20250.f, -17450.f, 1500.f, 2, TEXT("NEW") },
        { TEXT("ENG"),  TEXT("PRATT ENGINEERING"),     EK::Engineering, 'C', -10000.f,  -6400.f, -24750.f, -21950.f, 2120.f, 0, TEXT("NS") },
        { TEXT("PLT"),  TEXT("PHYSICAL PLANT"),        EK::Plant,       'C',  11750.f,  15950.f, -21250.f, -18250.f, 1300.f, 2, TEXT("W") },
        { TEXT("ALU"),  TEXT("ALUMNI HALL"),           EK::Alumni,      'C', -19000.f, -15600.f, -24500.f, -21800.f, 1500.f, 1, TEXT("NS") },
    };
    return Table;
}

// Calder Dr runs on the old side street's line, curves west into Stadium Dr and ends in the bus loop. The west road (a
// service road behind the library and Calder Hall) bends east into the south lane, which reaches the South Lot's back
// drive and the Physical Plant's yard. Everything meets Fraternity Park Dr flush, at street level.
constexpr float DriveX0 = 8700.f, DriveX1 = 10300.f, DriveSouthY = -17200.f;
constexpr float WestX0 = -20650.f, WestX1 = -20100.f;
const FBox2D Median = Rect(9420.f, 9580.f, -9300.f, -6200.f);
const FArc Curve{ FVector2D(7700.f, -17200.f), 1000.f, 2600.f, -90.f, 0.f };
const FArc Bend{ FVector2D(-19500.f, -25150.f), 600.f, 1150.f, -180.f, -90.f };
const FVector2D LoopCentre(-1000.f, -19300.f);
constexpr float LoopR = 1100.f, LoopIslandR = 420.f;
const FBox2D BusBay = Rect(300.f, 5200.f, -18200.f, -17850.f);
/** Where pedestrians cross Calder Dr (continental bars across the carriageway, curb ramps both sides). */
const float Crosswalks[] = { -6500.f, -8550.f, -13400.f, -15900.f };
constexpr float CrossHalf = 160.f, RampLen = 180.f;
/** The far sidewalk of Fraternity Park Dr is left out across the two drives that now meet it (with ramps either side). */
constexpr float VisitorX0 = -5150.f, VisitorX1 = -4550.f;
const FVector2D FarGaps[] = { FVector2D(WestX0 - RampLen, WestX1 + RampLen), FVector2D(VisitorX0 - RampLen, VisitorX1 + RampLen) };
/** The mid-block crosswalk on Fraternity Park Dr, on the stadium axis, and the existing one at the junction. */
const FVector2D StreetCrossings[] = { FVector2D(2250.f, 2550.f), FVector2D(8320.f, 8510.f) };
constexpr float StreetFarKerbY = -5530.f, StreetNearKerbY = -4170.f, StreetZ = -72.f;

const TArray<FBox2D>& RoadRects()
{
    static const TArray<FBox2D> R = {
        Rect(DriveX0, DriveX1, DriveSouthY, -5550.f),          // Calder Dr
        Rect(-1000.f, 7700.f, -19800.f, -18200.f),             // Stadium Dr
        BusBay,
        Rect(10300.f, 15000.f, -11000.f, -10450.f),            // Ashby Lane
        Rect(3600.f, 4400.f, -20550.f, -19800.f),              // South Lot drive
        Rect(VisitorX0, VisitorX1, -6700.f, -5550.f),          // visitor lot drive (clear of the street's pole at X -4500)
        Rect(WestX0, WestX1, -25150.f, -5550.f),               // the west road
        Rect(-19500.f, 13050.f, -26300.f, -25750.f),           // the south lane
        Rect(1000.f, 1700.f, -25750.f, -25050.f),              // South Lot back drive
        Rect(12500.f, 13050.f, -25750.f, -22200.f),            // Physical Plant lane
        Rect(11750.f, 14400.f, -22200.f, -21250.f),            // Physical Plant yard
        Rect(12000.f, 12600.f, -10450.f, -10000.f),            // the Commons' dock
        Rect(4500.f, 5100.f, -26700.f, -26300.f),              // the overflow lot's apron off the south lane
    };
    return R;
}
const TArray<FLot>& Lots()
{
    static const TArray<FLot> L = {
        { Rect(-1400.f, 8300.f, -25050.f, -20550.f), { FVector2D(3034.f, -21572.f), FVector2D(7347.f, -21896.f), FVector2D(6958.f, -24532.f), FVector2D(5559.f, -24805.f), FVector2D(-884.f, -24705.f) } },
        { Rect(-6400.f, -4400.f, -8500.f, -6700.f), {} },
        { Rect(15000.f, 18900.f, -11550.f, -10450.f), {} },
        { Rect(2000.f, 8000.f, -28800.f, -26700.f), {}, true },          // the gravel overflow lot off the south lane
    };
    return L;
}
const TArray<FStallRow>& StallRows()
{
    // Yaw is the direction the car's nose points once parked (nose-in from the aisle).
    static const TArray<FStallRow> R = {
        { -21800.f, -21250.f, -1300.f, 8200.f, -90.f }, { -22350.f, -21800.f, -1300.f, 8200.f, 90.f },
        { -23600.f, -23050.f, -1300.f, 8200.f, -90.f }, { -25050.f, -24500.f, -1300.f, 8200.f, -90.f },     // South Lot
        { -7250.f, -6700.f, -6400.f, -5100.f, 90.f }, { -8500.f, -7950.f, -6400.f, -4400.f, -90.f },         // Health visitor lot
        { -11550.f, -11000.f, 15100.f, 18800.f, -90.f },                                                     // Ashby permit lot
    };
    return R;
}
constexpr float StallW = 270.f;

/** Raised sidewalks along the campus roads (already split where curb ramps and drive crossings replace them). */
const TArray<FBox2D>& Sidewalks()
{
    static const TArray<FBox2D> S = [] {
        TArray<FBox2D> Out;
        // Calder Dr, both sides, between the crosswalk ramps; the east side also stops either side of Ashby Lane.
        for (const float X0 : { 8450.f, 10300.f })
        {
            TArray<FVector2D> Cuts;
            for (const float Y : Crosswalks) Cuts.Add(FVector2D(Y - CrossHalf, Y + CrossHalf));
            if (X0 > 9000.f) Cuts.Add(FVector2D(-11000.f - RampLen, -10450.f + RampLen));
            Cuts.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
            float Y = DriveSouthY;
            for (const FVector2D& C : Cuts) { Out.Add(Rect(X0, X0 + 250.f, Y, C.X)); Y = C.Y; }
            Out.Add(Rect(X0, X0 + 250.f, Y, -5800.f));
        }
        Out.Add(Rect(8450.f, 8700.f, -5800.f, -5550.f));                     // the corner at the junction
        Out.Add(Rect(-1000.f, 7700.f, -17850.f, -17600.f));                   // along the bus bay
        Out.Add(Rect(5200.f, 7700.f, -18200.f, -17850.f));                    // ... widened to meet the curve's inner walk
        Out.Add(Rect(-1200.f, -800.f, -18000.f, -17850.f));                   // ... and tied to the loop's ring walk
        Out.Add(Rect(0.f, 3600.f - RampLen, -20050.f, -19800.f));             // Stadium Dr's south side, either side of the lot drive
        Out.Add(Rect(4400.f + RampLen, 7700.f, -20050.f, -19800.f));
        Out.Add(Rect(10550.f, 15000.f, -11250.f, -11000.f));                  // Ashby Lane's south side
        return Out;
    }();
    return S;
}
const TArray<FArc>& SidewalkArcs()
{
    static const TArray<FArc> A = {
        { Curve.C, 750.f, 1000.f, -90.f, 0.f }, { Curve.C, 2600.f, 2850.f, -90.f, 0.f },     // round the curve
        { LoopCentre, 1100.f, 1450.f, 90.f, 330.f },                                     // round the bus loop
    };
    return A;
}
const TArray<FWedge>& Wedges()
{
    static const TArray<FWedge> W = [] {
        TArray<FWedge> Out;
        for (const float Y : Crosswalks)
        {
            Out.Add({ Rect(8450.f, 8700.f, Y - CrossHalf, Y + CrossHalf), 1, false });
            Out.Add({ Rect(10300.f, 10550.f, Y - CrossHalf, Y + CrossHalf), 0, false });
        }
        Out.Add({ Rect(10300.f, 10550.f, -11000.f - RampLen, -11000.f), 3, false });      // Calder Dr's east walk down across Ashby Lane
        Out.Add({ Rect(10300.f, 10550.f, -10450.f, -10450.f + RampLen), 2, false });
        Out.Add({ Rect(3600.f - RampLen, 3600.f, -20050.f, -19800.f), 1, false });       // Stadium Dr's south walk across the lot drive
        Out.Add({ Rect(4400.f, 4400.f + RampLen, -20050.f, -19800.f), 0, false });
        Out.Add({ Rect(VisitorX0 - RampLen, VisitorX0, -6650.f, -6350.f), 1, false });   // the promenade across the visitor drive
        Out.Add({ Rect(VisitorX1, VisitorX1 + RampLen, -6650.f, -6350.f), 0, false });
        for (const FVector2D& G : FarGaps)                                                 // the far sidewalk across the two drives
        {
            Out.Add({ Rect(G.X, G.X + RampLen, -5800.f, -5550.f), 1, false });
            Out.Add({ Rect(G.Y - RampLen, G.Y, -5800.f, -5550.f), 0, false });
        }
        for (const FVector2D& C : StreetCrossings)                                         // curb ramps built out into the street's gutter
        {
            Out.Add({ Rect(C.X, C.Y, StreetFarKerbY, StreetFarKerbY + 150.f), 3, true });
            Out.Add({ Rect(C.X, C.Y, StreetNearKerbY - 150.f, StreetNearKerbY), 2, true });
        }
        return Out;
    }();
    return W;
}

const TArray<FRun>& Walks()
{
    static const TArray<FRun> W = {
        { { -19975.f, -6500.f }, { VisitorX0 - RampLen, -6500.f }, 300.f, false }, { { VisitorX1 + RampLen, -6500.f }, { 8450.f, -6500.f }, 300.f, false },   // front promenade
        { { 10550.f, -6500.f }, { 19000.f, -6500.f }, 300.f, false },
        { { -14400.f, -10800.f }, { -9395.f, -10800.f }, 400.f, false },                                                   // the quad axis
        { { -13300.f, -7100.f }, { -13300.f, -6650.f }, 300.f, false }, { { -13300.f, -9700.f }, { -12900.f, -14200.f }, 300.f, false },
        { { -14400.f, -13600.f }, { -10200.f, -9900.f }, 250.f, false }, { { -14400.f, -9900.f }, { -10200.f, -13600.f }, 250.f, false },   // diagonals
        { { -8700.f, -7100.f }, { -8700.f, -6650.f }, 300.f, false },
        { { -11000.f, -6500.f }, { -11000.f, -10800.f }, 250.f, false },
        { { -17400.f, -14400.f }, { -17400.f, -13850.f }, 300.f, false }, { { -17400.f, -13850.f }, { -14400.f, -13850.f }, 250.f, false }, { { -14400.f, -13850.f }, { -14400.f, -13600.f }, 250.f, false },
        { { -6250.f, -10700.f }, { -6250.f, -9900.f }, 250.f, false }, { { -6250.f, -9900.f }, { -8700.f, -9900.f }, 250.f, false }, { { -8700.f, -9900.f }, { -8700.f, -10050.f }, 250.f, false },
        { { -4300.f, -12650.f }, { -3400.f, -12650.f }, 300.f, false },
        { { -1200.f, -6500.f }, { -1200.f, -9800.f }, 250.f, false }, { { -1200.f, -9800.f }, { -3400.f, -12100.f }, 250.f, false },
        { { -3400.f, -12100.f }, { -3400.f, -13300.f }, 250.f, false }, { { -3400.f, -13300.f }, { -3200.f, -13300.f }, 250.f, false },
        { { -6250.f, -14600.f }, { -6250.f, -16750.f }, 250.f, false },                                                   // the Union's south doors
        { { -11150.f, -17650.f }, { -8200.f, -17650.f }, 250.f, false }, { { -8200.f, -16750.f }, { -1100.f, -16750.f }, 250.f, false }, { { -1100.f, -16750.f }, { -1100.f, -17600.f }, 250.f, false },
        { { -11150.f, -18400.f }, { -11150.f, -17650.f }, 250.f, false },
        { { -10400.f, -17650.f }, { -10400.f, -14200.f }, 250.f, false }, { { -10400.f, -14200.f }, { -10200.f, -13600.f }, 250.f, false },
        { { -9050.f, -19900.f }, { -8200.f, -19900.f }, 250.f, false }, { { -7750.f, -18850.f }, { -8200.f, -18850.f }, 250.f, false },   // the arts and music side doors
        { { -8200.f, -21950.f }, { -8200.f, -16750.f }, 250.f, false },
        { { -5950.f, -17450.f }, { -5950.f, -16750.f }, 300.f, false },
        { { -17300.f, -21800.f }, { -17300.f, -20000.f }, 250.f, false }, { { -17300.f, -20000.f }, { -19975.f, -20000.f }, 250.f, false },
        { { -3000.f, -7100.f }, { -3000.f, -6650.f }, 300.f, false },
        { { -4400.f, -7900.f }, { -4200.f, -7900.f }, 250.f, false },
        { { 10550.f, -8550.f }, { 11000.f, -8550.f }, 300.f, false }, { { 12600.f, -7100.f }, { 12600.f, -6650.f }, 300.f, false }, { { 16800.f, -7100.f }, { 16800.f, -6650.f }, 300.f, false },
        { { 14450.f, -6500.f }, { 14450.f, -8750.f }, 250.f, false }, { { 14450.f, -8750.f }, { 14700.f, -8750.f }, 250.f, false },
        { { 10550.f, -13400.f }, { 11000.f, -13400.f }, 300.f, false },
        { { 10550.f, -15900.f }, { 14650.f, -15900.f }, 250.f, false }, { { 14650.f, -15900.f }, { 14650.f, -14600.f }, 250.f, false }, { { 14650.f, -14600.f }, { 15000.f, -14600.f }, 250.f, false },
        { { 16950.f, -12200.f }, { 16950.f, -11550.f }, 300.f, false },
        { { 2839.f, -17100.f }, { 2839.f, -17600.f }, 400.f, false },
        { { 9000.f, -19650.f }, { 11750.f, -19750.f }, 250.f, false },
        { { 2400.f, -5800.f }, { 2400.f, -6030.f }, 300.f, false },                                                         // the mid-block crosswalk to the kiosk
        { { 2400.f, -6630.f }, { 2400.f, -8600.f }, 300.f, true },                                                          // gate path (homecoming paves its own)
        { { -15600.f, -10800.f }, { -14400.f, -10800.f }, 400.f, false },
        { { -19975.f, -10800.f }, { -19000.f, -10800.f }, 250.f, false },
        { { -4150.f, -18850.f }, { -2350.f, -18900.f }, 250.f, false },
        { { -17300.f, -24500.f }, { -17300.f, -25625.f }, 250.f, false },
        { { -8200.f, -24750.f }, { -8200.f, -25625.f }, 250.f, false },
        { { -19975.f, -16500.f }, { -19000.f, -16500.f }, 250.f, false },
    };
    return W;
}
struct FPlaza { FVector2D C; float R; };
const TArray<FPlaza>& PlazaDiscs()
{
    static const TArray<FPlaza> P = { { FVector2D(-9000.f, -10800.f), 800.f } };   // the tower plaza
    return P;
}
struct FPlazaRect { FBox2D R; bool bFall; };
const TArray<FPlazaRect>& PlazaRects()
{
    static const TArray<FPlazaRect> P = {
        { Rect(-15600.f, -14400.f, -11700.f, -9900.f), false },     // the library forecourt
        { Rect(1300.f, 3500.f, -9480.f, -8600.f), false },          // the ticket plaza at the north gates
        { Rect(-8550.f, -8200.f, -12950.f, -10600.f), false },      // the Union's forecourt off the tower plaza
        { Rect(2100.f, 2700.f, -6630.f, -6030.f), true },           // the kiosk plaza (homecoming paves its own)
    };
    return P;
}
/** The squares around the curved roads, where this file replaces the terrain (lawn in the terrain's own material). */
const FBox2D CurveSquare = Rect(7700.f, 10300.f, -19800.f, -17200.f);
const FBox2D BendSquare = Rect(-20650.f, -19500.f, -26300.f, -25150.f);
const FBox2D LoopSquare = Rect(-2100.f, 100.f, -20400.f, -18200.f);

// ------------------------------------------------------------------ shape tests
bool InArc(const FArc& A, const FVector2D& P, float Pad = 0.f)
{
    const FVector2D D = P - A.C;
    const float R = D.Size();
    if (R < A.R0 - Pad || R > A.R1 + Pad) return false;
    float Ang = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
    const float Lo = FMath::Min(A.A0, A.A1), Hi = FMath::Max(A.A0, A.A1);
    if (Ang < Lo - 1e-3f) Ang += 360.f;
    const float PadDeg = R > 1.f ? FMath::RadiansToDegrees(Pad / R) : 0.f;
    return Ang >= Lo - PadDeg && Ang <= Hi + PadDeg;
}
bool OnFace(const FVector2D& P);
float RunOn(const FVector2D& End, const FVector2D& Out, float Width);
/** Inside a run as Run() builds it: square ends that run on by half the width (see RunOn). */
bool InRun(const FRun& W, const FVector2D& P, float Pad)
{
    const FVector2D D = W.B - W.A;
    const float L = D.Size();
    if (L < 1.f) return false;
    const FVector2D U = D / L;
    const float T = FVector2D::DotProduct(P - W.A, U), N = FMath::Abs(FVector2D::CrossProduct(U, P - W.A));
    // (A run-on is never more than half the width: points beyond that need no RunOn probes. The same answer, faster.)
    if (N > W.W * .5f + Pad || T < -W.W * .5f - Pad || T > L + W.W * .5f + Pad) return false;
    const float ExA = RunOn(W.A, -U, W.W), ExB = RunOn(W.B, U, W.W);
    return N <= W.W * .5f + Pad && T >= -ExA - Pad && T <= L + ExB + Pad;
}
/** Every planned run is paved: by this file (visible), or under homecoming by the dressing's own drawn path with this
 *  file's hidden collision floor under it (BuildFallFloors). RunVisible: this file draws it. */
bool RunBuilt(const FRun& W) { return true; }
bool RunVisible(const FRun& W) { return !W.bFall || !HouseCampusFall::Enabled(); }
bool PlazaVisible(bool bFall) { return !bFall || !HouseCampusFall::Enabled(); }
bool InIsland(const FVector2D& P, float Pad)
{
    for (const FLot& L : Lots()) for (const FVector2D& I : L.Islands) if (FVector2D::Distance(P, I) < 320.f + Pad) return true;
    return false;
}
/** Carriageway, drive, lane, yard or lot (islands included). */
bool RoadAt(const FVector2D& P, float Pad = 0.f)
{
    for (const FBox2D& R : RoadRects()) if (R.ExpandBy(Pad).IsInside(P)) return true;
    for (const FLot& L : Lots()) if (L.Area.ExpandBy(Pad).IsInside(P)) return true;
    if (InArc(Curve, P, Pad) || InArc(Bend, P, Pad)) return true;
    return FVector2D::Distance(P, LoopCentre) < LoopR + Pad;
}
bool SidewalkAt(const FVector2D& P, float Pad = 0.f)
{
    for (const FBox2D& R : Sidewalks()) if (R.ExpandBy(Pad).IsInside(P)) return true;
    for (const FArc& A : SidewalkArcs()) if (InArc(A, P, Pad)) return true;
    for (const FWedge& W : Wedges()) if (W.R.ExpandBy(Pad).IsInside(P)) return true;
    return false;
}
/** Fraternity Park Dr, its far sidewalk band, and the house-side end of the old side street (all built by the exterior). */
bool StreetAt(const FVector2D& P)
{
    if (P.X <= DriveX0 + 1.f && P.Y >= -5800.f && P.Y <= -4150.f) return true;
    if (P.X >= 8450.f && P.X <= 10550.f && P.Y >= -5800.f) return true;
    return false;
}
/** A curb is needed on a road edge only where the ground beyond is lawn (a walk ending there still gets one under it). */
bool CurbStop(const FVector2D& P) { return RoadAt(P) || SidewalkAt(P) || StreetAt(P); }
bool WedgeAt(const FVector2D& P)
{
    for (const FWedge& W : Wedges()) if (W.R.IsInside(P)) return true;
    return false;
}
/** How far a walk end runs on past its end point: half the width, so walks meeting at a corner close it, but not at a
 *  building face, and not where the run-on would land on a road, a lot, a curb ramp or the street (a flat slab there
 *  would stand as a ledge on the ramp or float above the asphalt). */
float RunOn(const FVector2D& End, const FVector2D& Out, float Width)
{
    if (OnFace(End)) return 0.f;
    for (const FPlazaRect& Q : PlazaRects()) if (Q.R.ExpandBy(2.f).IsInside(End)) return 0.f;   // the plaza closes the end (homecoming's too)
    for (const FPlaza& Q : PlazaDiscs()) if (FVector2D::Distance(End, Q.C) < Q.R + 2.f) return 0.f;
    const float Ex = Width * .5f;
    const FVector2D Side(-Out.Y, Out.X);
    for (const float S : { -1.f, 0.f, 1.f })
        for (const float T : { 2.f, Ex * .5f, Ex - 1.f })
        {
            const FVector2D P = End + Out * T + Side * (S * (Ex - 1.f));
            if (RoadAt(P) || WedgeAt(P) || StreetAt(P)) return 0.f;
        }
    return Ex;
}
bool WalkAt(const FVector2D& P, float Pad)
{
    for (const FRun& W : Walks()) if (RunBuilt(W) && InRun(W, P, Pad)) return true;
    for (const FPlaza& Q : PlazaDiscs()) if (FVector2D::Distance(P, Q.C) < Q.R + Pad) return true;
    for (const FPlazaRect& Q : PlazaRects()) if (Q.R.ExpandBy(Pad).IsInside(P)) return true;
    return false;
}
const FBox2D TowerPlinth = Rect(-9395.f, -8605.f, -11195.f, -10405.f);
bool OnFace(const FVector2D& P)
{
    for (const HouseCampusLayout::FBlock& B : BlockTable()) if (B.Rect().ExpandBy(5.f).IsInside(P)) return true;
    if (TowerPlinth.ExpandBy(5.f).IsInside(P)) return true;
    return FMath::Square((P.X - 2400.f) / 5600.f) + FMath::Square((P.Y + 13300.f) / 3800.f) < 1.02f;
}

// ------------------------------------------------------------------ small builders
FName AsphaltKey() { return HouseSurroundings::Enabled() ? FName(TEXT("EX_StreetAsphalt")) : FName(TEXT("EX_Asphalt")); }
FName ConcreteKey() { return HouseSurroundings::Enabled() ? FName(TEXT("EX_StreetConcrete")) : FName(TEXT("EX_Concrete")); }
const FHallUV& AsphaltUV() { static const FHallUV U = FHallUV::World(500.f); return U; }
const FHallUV& ConcreteUV() { static const FHallUV U = FHallUV::World(240.f); return U; }   // the street's own period
const FHallUV& GrassUV() { static const FHallUV U = FHallUV::World(180.f); return U; }

// ------------------------------------------------------------------ heights on the Calder terrain
float GroundAt(const FVector2D& P) { return HouseExteriorSite::GroundHeight(P.X, P.Y); }
float RoadZ(const FVector2D& P) { return HouseCalderTerrain::SurfaceZ(P.X, P.Y, HouseCalderTerrain::EPave::Road); }
float WalkZ(const FVector2D& P) { return HouseCalderTerrain::SurfaceZ(P.X, P.Y, HouseCalderTerrain::EPave::Walk); }
/** Draped levels (step 1c), each one of today's levels at every point of the ground: a paving top (HouseCalderTerrain::SurfaceZ,
 *  road or walk class) plus a lift, or one of today's datums carried onto the ground (slab bottoms 40 cm under it, walk and
 *  plaza bases 5 cm, the lawn on it). */
struct FDrapeRoad { float Lift; float operator()(float X, float Y) const { return HouseCalderTerrain::SurfaceZ(X, Y, HouseCalderTerrain::EPave::Road) + Lift; } };
struct FDrapeWalk { float Lift; float operator()(float X, float Y) const { return HouseCalderTerrain::SurfaceZ(X, Y, HouseCalderTerrain::EPave::Walk) + Lift; } };
struct FDrapeDatum { float Datum; float operator()(float X, float Y) const { return HouseExteriorSite::GroundHeight(X, Y) + (Datum - Ground); } };
/** Some but not all of a footprint (its corners, edge middles and centre) lies in a frozen zone: it straddles the edge. */
bool Straddles(const FBox2D& Box)
{
    const FVector2D C = Box.GetCenter();
    const FVector2D Points[] = { Box.Min, FVector2D(C.X, Box.Min.Y), FVector2D(Box.Max.X, Box.Min.Y), FVector2D(Box.Min.X, C.Y), C,
                                 FVector2D(Box.Max.X, C.Y), FVector2D(Box.Min.X, Box.Max.Y), FVector2D(C.X, Box.Max.Y), Box.Max };
    int32 In = 0, Out = 0;
    for (const FVector2D& P : Points) (HouseCalderTerrain::InFrozen(P.X, P.Y) ? In : Out)++;
    return In > 0 && Out > 0;
}
/** Where a flat primitive (slab, disc, run, arc, paint, curb) stands. Built flat, it stands on the ground at its footprint's
 *  centre, its paving top from HouseCalderTerrain::SurfaceZ and every other level as today's datum carried onto that ground; a
 *  footprint that straddles a frozen zone's edge keeps today's datums, so its frozen part never moves. At zero relief every
 *  height is exactly today's datum. Drape() says when the builder drapes it on the relief lattice instead (step 1c). */
struct FFooting
{
    FVector2D C;
    FBox2D Span;   // the footprint the drape test reads
    bool bFrozenEdge;
    explicit FFooting(const FBox2D& Box) : C(Box.GetCenter()), Span(Box), bFrozenEdge(Straddles(Box)) {}
    FFooting(const FVector2D& Centre, const FBox2D& Box) : C(Centre), Span(Box), bFrozenEdge(Straddles(Box)) {}
    FFooting(const FVector2D& Centre, float Radius) : C(Centre), Span(Centre - FVector2D(Radius, Radius), Centre + FVector2D(Radius, Radius)), bFrozenEdge(Straddles(Span)) {}
    /** One of today's datums (Bottom, Ground - 5, Ground) carried onto the ground here. */
    float Z(float Datum) const { return bFrozenEdge ? Datum : GroundAt(C) + (Datum - Ground); }   // frozen edge: stays literal
    float Road() const { return bFrozenEdge ? RoadTop : RoadZ(C); }                                // frozen edge: stays literal
    float Walk() const { return bFrozenEdge ? WalkTop : WalkZ(C); }                                // frozen edge: stays literal (curbs too)
    /** Draped (top and bottom at their levels over the ground at every point, cut along the lattice) only where the relief
     *  under it, or within one lattice cell of it, is not 0, and never across a frozen zone's edge or wholly inside a frozen
     *  zone (whose geometry stays today's). Otherwise (all of today, and -NoCalderTerrain) the builder takes exactly today's
     *  flat path: same vertices, same counts. */
    bool Drape() const
    {
        return !bFrozenEdge && !HouseCalderTerrain::ReliefFlat(Span.ExpandBy(HouseCalderTerrain::Cell)) && !HouseCalderTerrain::BoxFrozen(Span);
    }
};
/** A plaza (level by design) is built level at its footing's walk height, except on the Calder relief where the ground under
 *  it is not level or a Calder stair or ramp stands in it (the library forecourt's east edge, the Union's forecourt off the
 *  tower plaza): there it drapes on the ground, cut round the stair (HouseCalderDrape). Today (zero relief) never. */
bool PlazaDrapes(const FBox2D& R)
{
    if (!FFooting(R).Drape()) return false;
    if (HouseCalderTerrain::ReliefMin(R) != HouseCalderTerrain::ReliefMax(R)) return true;
    for (const TArray<FVector2D>& H : HouseCalderFeatures::Footprints())
    {
        FBox2D B(ForceInit);
        for (const FVector2D& V : H) B += V;
        if (B.Intersect(R)) return true;
    }
    return false;
}
/** On the Calder terrain a road box that straddles a frozen zone's edge comes back split along that zone's edges: the part
 *  inside stays today's (flat, literal), every other part follows the relief like any other road. Calder Dr, the visitor drive
 *  and the west road all run 2.5 m into the frozen street; kept whole and flat at the street's level, Founders Way (which climbs
 *  19 m) lay in a 9 m trench under its own bank (2026-10-15). Off the relief (and -NoCalderTerrain) the box itself. */
void SplitAtFrozen(const FBox2D& R, TArray<FBox2D>& Out)
{
    Out.Reset();
    Out.Add(R);
    if (!HouseCalderTerrain::Enabled() || !Straddles(R)) return;
    for (const FBox2D& Z : HouseCalderTerrain::FrozenRects())
    {
        TArray<FBox2D> Next;
        for (const FBox2D& B : Out)
        {
            if (B.Min.X >= Z.Max.X || B.Max.X <= Z.Min.X || B.Min.Y >= Z.Max.Y || B.Max.Y <= Z.Min.Y) { Next.Add(B); continue; }
            TArray<double> Xs = { B.Min.X, B.Max.X }, Ys = { B.Min.Y, B.Max.Y };
            for (const double V : { double(Z.Min.X), double(Z.Max.X) }) if (V > B.Min.X && V < B.Max.X) Xs.Add(V);
            for (const double V : { double(Z.Min.Y), double(Z.Max.Y) }) if (V > B.Min.Y && V < B.Max.Y) Ys.Add(V);
            Xs.Sort();
            Ys.Sort();
            for (int32 I = 0; I + 1 < Xs.Num(); ++I)
                for (int32 J = 0; J + 1 < Ys.Num(); ++J)
                    if (Xs[I + 1] - Xs[I] > .01 && Ys[J + 1] - Ys[J] > .01) Next.Add(FBox2D(FVector2D(Xs[I], Ys[J]), FVector2D(Xs[I + 1], Ys[J + 1])));
        }
        Out = MoveTemp(Next);
    }
}
/** The footing of one piece SplitAtFrozen returned: a piece of a split box no longer straddles anything (it only shares the
 *  frozen zone's edge, which InFrozen counts as inside), so it is literal only when it lies wholly in the zone. */
FFooting PieceFooting(const FBox2D& Piece, bool bSplit)
{
    FFooting F(Piece);
    if (bSplit) F.bFrozenEdge = false;
    return F;
}
/** An annulus sector's true bounds: its corners and the farthest points of every axis direction it sweeps through. */
FBox2D ArcBounds(const FArc& A)
{
    auto At = [&A](float Deg, float R) { const float T = FMath::DegreesToRadians(Deg); return A.C + FVector2D(FMath::Cos(T), FMath::Sin(T)) * R; };
    const float Lo = FMath::Min(A.A0, A.A1), Hi = FMath::Max(A.A0, A.A1);
    FBox2D Box(ForceInit);
    for (const float R : { A.R0, A.R1 }) { Box += At(Lo, R); Box += At(Hi, R); }
    for (float Deg = 90.f * FMath::CeilToFloat(Lo / 90.f); Deg < Hi; Deg += 90.f) Box += At(Deg, A.R1);
    return Box;
}
/** An annulus sector's footing: at its middle (mid angle, mid radius), judged over its corners and middle; the drape test
 *  reads its true bounds (three angles can miss most of a wide sector). */
FFooting ArcFooting(const FArc& A)
{
    auto At = [&A](float Deg, float R) { const float T = FMath::DegreesToRadians(Deg); return A.C + FVector2D(FMath::Cos(T), FMath::Sin(T)) * R; };
    FBox2D Box(ForceInit);
    for (const float Deg : { A.A0, (A.A0 + A.A1) * .5f, A.A1 }) { Box += At(Deg, A.R0); Box += At(Deg, A.R1); }
    FFooting F(At((A.A0 + A.A1) * .5f, (A.R0 + A.R1) * .5f), Box);
    F.Span = ArcBounds(A);
    return F;
}
/** Where the road or lot under P is built flat on the relief, its top (BuildGround's own choice; keep the two in step): a lot
 *  (a lot tier) is level by design and never draped, and a road rect whose footprint straddles a frozen zone's edge keeps
 *  today's literal height (FFooting::bFrozenEdge). What lies on them (stall lines, wheel stops, islands, the median, road paint,
 *  StreetDetail's inlets, manholes and stop bar) sits on that top in the drape branch instead of following the ground under the
 *  cut-away terrain. False where the road under P drapes, and off every road rect and lot (OutTop is then left alone). */
bool FlatPaveTop(const FVector2D& P, float& OutTop)
{
    for (const FLot& L : Lots())
        if (L.Area.IsInsideOrOn(P))   // (or on: a row's first stall line)
        {
            // On the relief a lot follows the landform's own tiers and falls (the South Lot's three tiers, tier A's 3 % fall):
            // it is draped, so what lies on it drapes too.
            const FFooting F(L.Area);
            if (F.Drape()) return false;
            OutTop = F.Road();
            return true;
        }
    TArray<FBox2D> Pieces;
    for (const FBox2D& R0 : RoadRects())
    {
        if (!R0.IsInside(P)) continue;
        SplitAtFrozen(R0, Pieces);
        for (const FBox2D& R : Pieces)
        {
            if (!R.IsInsideOrOn(P)) continue;
            const FFooting F = PieceFooting(R, Pieces.Num() > 1);
            if (!F.bFrozenEdge) return false;   // it drapes (or the relief round it is 0, where draped and flat agree)
            OutTop = F.Road();
            return true;
        }
        }
    return false;
}
/** The lowest ground under a footprint (its corners and centre): where a post or wall footing stands so no foot floats. */
float LowestGround(const FBox2D& Box)
{
    float Z = GroundAt(Box.GetCenter());
    for (const FVector2D& P : { Box.Min, Box.Max, FVector2D(Box.Min.X, Box.Max.Y), FVector2D(Box.Max.X, Box.Min.Y) }) Z = FMath::Min(Z, GroundAt(P));
    return Z;
}

void Slab(FHallMesh& M, FName K, const FBox2D& R, float Lo, float Hi, const FHallUV& UV)
{
    if (R.Max.X - R.Min.X < 1.f || R.Max.Y - R.Min.Y < 1.f) return;
    M.BoxMinMax(K, FVector(R.Min.X, R.Min.Y, Lo), FVector(R.Max.X, R.Max.Y, Hi), UV);
}

// ------------------------------------------------------------------ draped builders (step 1c; only where FFooting::Drape())
/** Recessed Calder joins replace the old road or lot pavement inside these
 *  owned boxes. Leaving both meshes lets the old, coarser surface cover the
 *  replacement's smooth transition. Empty when the new roads are disabled. */
void CutRoadReplacements(TArray<FBox2D>& Pieces)
{
    TArray<FBox2D> Cuts;
    HouseCalderRoads::TerrainCuts(Cuts);
    for(const FBox2D& Cut:Cuts)
    {
        TArray<FBox2D> Remaining;
        for(const FBox2D& Piece:Pieces)
        {
            const FVector2D Lo(FMath::Max(Piece.Min.X,Cut.Min.X),FMath::Max(Piece.Min.Y,Cut.Min.Y));
            const FVector2D Hi(FMath::Min(Piece.Max.X,Cut.Max.X),FMath::Min(Piece.Max.Y,Cut.Max.Y));
            if(Lo.X>=Hi.X || Lo.Y>=Hi.Y) { Remaining.Add(Piece); continue; }
            for(const FBox2D& Part:{FBox2D(Piece.Min,FVector2D(Lo.X,Piece.Max.Y)),FBox2D(FVector2D(Hi.X,Piece.Min.Y),Piece.Max),
                FBox2D(FVector2D(Lo.X,Piece.Min.Y),FVector2D(Hi.X,Lo.Y)),FBox2D(FVector2D(Lo.X,Hi.Y),FVector2D(Hi.X,Piece.Max.Y))})
                if(Part.Max.X-Part.Min.X>.01 && Part.Max.Y-Part.Min.Y>.01) Remaining.Add(Part);
        }
        Pieces=MoveTemp(Remaining);
    }
}
/** Slab draped: the box's top at Hi(X, Y) and its sides down to Lo(X, Y), cut along the lattice. */
void DrapedSlab(FHallMesh& M, FName K, const FBox2D& R, TFunctionRef<float(float, float)> Lo, TFunctionRef<float(float, float)> Hi, const FHallUV& UV)
{
    if (R.Max.X - R.Min.X < 1.f || R.Max.Y - R.Min.Y < 1.f) return;
    HouseCalderDrape::Slab(M, K, HouseCalderDrape::RectOutline(R), Hi, Lo, UV);
}
/** One side face from P to Q (facing Out), draped: split wherever it crosses a lattice line or diagonal, from Hi(X, Y) down to
 *  Lo(X, Y) at every split (HouseCalderDrape::Slab's walls, for a single edge). */
void DrapedEdge(FHallMesh& M, FName K, const FVector2D& P, const FVector2D& Q, const FVector& Out, TFunctionRef<float(float, float)> Lo, TFunctionRef<float(float, float)> Hi, const FHallUV& UV)
{
    const TArray<FVector2D> Points = HouseCalderDrape::Densify({ P, Q });   // P, its crossings on the way to Q, Q, then the way back
    const int32 End = Points.IndexOfByKey(Q);
    for (int32 I = 0; I < End; ++I)
    {
        const FVector2D& A = Points[I];
        const FVector2D& B = Points[I + 1];
        const FVector AT(A, Hi(A.X, A.Y)), BT(B, Hi(B.X, B.Y)), AB(A, Lo(A.X, A.Y)), BB(B, Lo(B.X, B.Y));
        if (AT.Z - AB.Z < .05f && BT.Z - BB.Z < .05f) continue;
        M.Quad(K, AT, BT, BB, AB, Out, UV);
    }
}
/** Disc draped: the same rim polygon, its top at Top(X, Y) and its skirt down to Lo(X, Y). */
void DrapedDisc(FHallMesh& M, FName K, const FVector2D& C, float R, TFunctionRef<float(float, float)> Lo, TFunctionRef<float(float, float)> Top, const FHallUV& UV, int32 Seg = 48)
{
    TArray<FVector2D> Rim;
    for (int32 I = 0; I < Seg; ++I)
    {
        const float A = 2.f * PI * I / Seg;
        Rim.Add(FVector2D(C.X + FMath::Cos(A) * R, C.Y + FMath::Sin(A) * R));
    }
    HouseCalderDrape::Slab(M, K, Rim, Top, Lo, UV);
}
/** ArcSlab draped: the same segments, each cut along the lattice with its top at Hi(X, Y), and the curved faces (and the end
 *  caps) down to Lo(X, Y), split at the same lines. */
void DrapedArcSlab(FHallMesh& M, FName K, const FArc& A, TFunctionRef<float(float, float)> Lo, TFunctionRef<float(float, float)> Hi, const FHallUV& UV, bool bCaps = true)
{
    const int32 Seg = FMath::Max(2, FMath::CeilToInt(FMath::Abs(A.A1 - A.A0) / 5.f));
    auto P = [&A](float Deg, float R) { const float T = FMath::DegreesToRadians(Deg); return A.C + FVector2D(FMath::Cos(T), FMath::Sin(T)) * R; };
    for (int32 I = 0; I < Seg; ++I)
    {
        const float D0 = FMath::Lerp(A.A0, A.A1, float(I) / Seg), D1 = FMath::Lerp(A.A0, A.A1, float(I + 1) / Seg);
        if (A.R0 > 1.f) HouseCalderDrape::Sheet(M, K, HouseCalderDrape::SectorPiece(A.C, A.R0, A.R1, D0, D1), Hi, UV);
        else HouseCalderDrape::Sheet(M, K, { A.C, P(D0, A.R1), P(D1, A.R1) }, Hi, UV);
        const float Dm = FMath::DegreesToRadians((D0 + D1) * .5f);
        const FVector Out(FMath::Cos(Dm), FMath::Sin(Dm), 0.f);
        DrapedEdge(M, K, P(D0, A.R1), P(D1, A.R1), Out, Lo, Hi, UV);
        if (A.R0 > 1.f) DrapedEdge(M, K, P(D0, A.R0), P(D1, A.R0), -Out, Lo, Hi, UV);
    }
    if (!bCaps) return;
    const float Turn = FMath::Sign(A.A1 - A.A0);
    for (const float D : { A.A0, A.A1 })
    {
        const float T = FMath::DegreesToRadians(D);
        const FVector Tan(-FMath::Sin(T), FMath::Cos(T), 0.f);
        DrapedEdge(M, K, P(D, A.R0), P(D, A.R1), D == A.A0 ? -Tan * Turn : Tan * Turn, Lo, Hi, UV);
    }
}

/** A straight run of walk as an oriented slab, its top at walk height over the ground at its middle and its base 5 cm into
 *  the lawn; each end runs on by half its width unless it meets a building face. Draped, the top is at walk height
 *  (HouseCalderTerrain::SurfaceZ, walk class) and the base 5 cm into the lawn at every point. */
void Run(FHallMesh& M, FName K, const FRun& W, const FHallUV& UV)
{
    const FVector2D D = W.B - W.A;
    const float L = D.Size();
    if (L < 1.f) return;
    const FVector2D Dir = D / L;
    const float ExA = RunOn(W.A, -Dir, W.W), ExB = RunOn(W.B, Dir, W.W);
    const FVector2D A = W.A - Dir * ExA, B = W.B + Dir * ExB;
    const FVector2D Side = FVector2D(-Dir.Y, Dir.X) * (W.W * .5f);
    FBox2D Foot(ForceInit);
    for (const FVector2D& P : { A - Side, A + Side, B - Side, B + Side }) Foot += P;
    const FFooting F(FVector2D((A.X + B.X) * .5f, (A.Y + B.Y) * .5f), Foot);
    if (F.Drape())
    {
        HouseCalderDrape::Slab(M, K, HouseCalderDrape::RunOutline(A, B, W.W), FDrapeWalk{ 0.f }, FDrapeDatum{ Ground - 5.f }, UV);
        return;
    }
    const float Top = F.Walk(), G = F.Z(Ground);
    const FVector Mid((A.X + B.X) * .5f, (A.Y + B.Y) * .5f, (Top + G - 5.f) * .5f);
    const FRotator Rot(0.f, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0.f);
    M.Box(K, FTransform(Rot, Mid), FVector((L + ExA + ExB) * .5f, W.W * .5f, (Top - (G - 5.f)) * .5f), UV);
}

/** A flat disc with a skirt down to Lo. */
void Disc(FHallMesh& M, FName K, const FVector2D& C, float R, float Lo, float Top, const FHallUV& UV, int32 Seg = 48)
{
    TArray<FVector> Rim;
    for (int32 I = 0; I < Seg; ++I)
    {
        const float A = 2.f * PI * I / Seg;
        Rim.Add(FVector(C.X + FMath::Cos(A) * R, C.Y + FMath::Sin(A) * R, Top));
    }
    const FVector Hub(C.X, C.Y, Top);
    for (int32 I = 0; I < Seg; ++I)
    {
        const FVector A = Rim[I], B = Rim[(I + 1) % Seg];
        HouseCampusFallKit::Tri(M, K, Hub, A, B, FVector::UpVector, UV);
        const FVector Out = FVector(((A + B) * .5f - Hub).GetSafeNormal2D());
        M.Quad(K, A, B, FVector(B.X, B.Y, Lo), FVector(A.X, A.Y, Lo), Out, UV);
    }
}

/** An annulus sector slab from Lo to Hi: top, both curved faces and (optionally) the two end caps. */
void ArcSlab(FHallMesh& M, FName K, const FArc& A, float Lo, float Hi, const FHallUV& UV, bool bCaps = true)
{
    const int32 Seg = FMath::Max(2, FMath::CeilToInt(FMath::Abs(A.A1 - A.A0) / 5.f));
    auto P = [&A](float Deg, float R, float Z) { const float T = FMath::DegreesToRadians(Deg); return FVector(A.C.X + FMath::Cos(T) * R, A.C.Y + FMath::Sin(T) * R, Z); };
    for (int32 I = 0; I < Seg; ++I)
    {
        const float D0 = FMath::Lerp(A.A0, A.A1, float(I) / Seg), D1 = FMath::Lerp(A.A0, A.A1, float(I + 1) / Seg);
        if (A.R0 > 1.f) M.Quad(K, P(D0, A.R0, Hi), P(D1, A.R0, Hi), P(D1, A.R1, Hi), P(D0, A.R1, Hi), FVector::UpVector, UV);
        else HouseCampusFallKit::Tri(M, K, FVector(A.C, Hi), P(D0, A.R1, Hi), P(D1, A.R1, Hi), FVector::UpVector, UV);
        const float Dm = FMath::DegreesToRadians((D0 + D1) * .5f);
        const FVector Out(FMath::Cos(Dm), FMath::Sin(Dm), 0.f);
        M.Quad(K, P(D0, A.R1, Hi), P(D1, A.R1, Hi), P(D1, A.R1, Lo), P(D0, A.R1, Lo), Out, UV);
        if (A.R0 > 1.f) M.Quad(K, P(D0, A.R0, Hi), P(D1, A.R0, Hi), P(D1, A.R0, Lo), P(D0, A.R0, Lo), -Out, UV);
    }
    if (!bCaps) return;
    const float Turn = FMath::Sign(A.A1 - A.A0);
    for (const float D : { A.A0, A.A1 })
    {
        const float T = FMath::DegreesToRadians(D);
        const FVector Tan(-FMath::Sin(T), FMath::Cos(T), 0.f);
        M.Quad(K, P(D, A.R0, Hi), P(D, A.R1, Hi), P(D, A.R1, Lo), P(D, A.R0, Lo), D == A.A0 ? -Tan * Turn : Tan * Turn, UV);
    }
}

/** The arc with its outer radius pushed out so ArcSlab's straight chords reach the true radius everywhere (the curbs and
 *  walks beyond are finer polygons; without this a hairline to the void opens between them). */
FArc Circumscribed(const FArc& A)
{
    const int32 Seg = FMath::Max(2, FMath::CeilToInt(FMath::Abs(A.A1 - A.A0) / 5.f));
    const float Half = FMath::DegreesToRadians(FMath::Abs(A.A1 - A.A0) / Seg * .5f);
    return FArc{ A.C, A.R0, A.R1 / FMath::Cos(Half) + 1.f, A.A0, A.A1 };
}

/** A curb ramp's or drive crossing's top at (X,Y), T of the way from its high side (0) to its low side (1): from walk height
 *  down to 4 mm over the road there (over the street's own paving for the frozen on-street ramps). The slab and the review's
 *  samples on it both read this. */
float WedgeZ(const FWedge& W, float X, float Y, float T)
{
    const float Hi = HouseCalderTerrain::SurfaceZ(X, Y, HouseCalderTerrain::EPave::Walk);
    const float Lo = W.bOnStreet ? StreetZ + .4f : HouseCalderTerrain::SurfaceZ(X, Y, HouseCalderTerrain::EPave::Road) + .4f;
    return FMath::Lerp(Hi, Lo, T);
}

/** A sloped slab: the top falls from walk height on the side opposite LowSide to road height on LowSide (WedgeZ, at each
 *  corner); all four sides go down to the slab bottom (40 cm under the ground at each corner). Draped, the top is WedgeZ at
 *  every point (the ground plus a plane, so exact on each lattice piece) and the sides go down to 40 cm under the ground. */
void Wedge(FHallMesh& M, FName K, const FWedge& W, const FHallUV& UV)
{
    const FBox2D& R = W.R;
    auto Z = [&](float X, float Y)
    {
        float T = 0.f;   // 0 at the high side, 1 at the low side
        switch (W.LowSide)
        {
        case 0: T = (R.Max.X - X) / (R.Max.X - R.Min.X); break;
        case 1: T = (X - R.Min.X) / (R.Max.X - R.Min.X); break;
        case 2: T = (R.Max.Y - Y) / (R.Max.Y - R.Min.Y); break;
        default: T = (Y - R.Min.Y) / (R.Max.Y - R.Min.Y); break;
        }
        return WedgeZ(W, X, Y, T);
    };
    auto Foot = [](const FVector& P) { return FVector(P.X, P.Y, GroundAt(FVector2D(P.X, P.Y)) + (Bottom - Ground)); };
    if (FFooting(R).Drape())
    {
        HouseCalderDrape::Slab(M, K, HouseCalderDrape::RectOutline(R), Z, FDrapeDatum{ Bottom }, UV);
        return;
    }
    const FVector2D C[4] = { R.Min, FVector2D(R.Max.X, R.Min.Y), R.Max, FVector2D(R.Min.X, R.Max.Y) };
    FVector T[4];
    for (int32 I = 0; I < 4; ++I) T[I] = FVector(C[I].X, C[I].Y, Z(C[I].X, C[I].Y));
    FVector Up = FVector::CrossProduct(T[1] - T[0], T[3] - T[0]).GetSafeNormal();
    if (Up.Z < 0.f) Up = -Up;
    M.Quad(K, T[0], T[1], T[2], T[3], Up, UV);
    const FVector Outs[4] = { FVector(0, -1, 0), FVector(1, 0, 0), FVector(0, 1, 0), FVector(-1, 0, 0) };
    for (int32 I = 0; I < 4; ++I)
    {
        const FVector& A = T[I];
        const FVector& B = T[(I + 1) % 4];
        M.Quad(K, A, B, Foot(B), Foot(A), Outs[I], UV);
    }
}

/** A raised concrete curb ring (planting islands, the loop island): each bar from road height to 4 cm over curb height at
 *  its own middle (2 cm under the one, 2 cm over the other; draped, at every point, except on a lot, which is level by design:
 *  there the ring stands level on the lot's top, FlatPaveTop). */
void CurbRing(FHallMesh& M, const FVector2D& C, float R, int32 Seg = 32)
{
    for (int32 I = 0; I < Seg; ++I)
    {
        const float A0 = 2.f * PI * I / Seg, A1 = 2.f * PI * (I + 1) / Seg;
        const FVector2D P0(C.X + FMath::Cos(A0) * R, C.Y + FMath::Sin(A0) * R), P1(C.X + FMath::Cos(A1) * R, C.Y + FMath::Sin(A1) * R);
        const FFooting F((P0 + P1) * .5f, FBox2D(C - FVector2D(R, R), C + FVector2D(R, R)));
        if (F.Drape())
        {
            float LotTop = 0.f;
            if (FlatPaveTop(C, LotTop))   // a lot island: level with the lot round it, curb height over its top
            {
                const float Hi = LotTop + (WalkTop - RoadTop);
                HouseCampusFallKit::Bar(M, ConcreteKey(), FVector(P0, (LotTop + Hi) * .5f), FVector(P1, (LotTop + Hi) * .5f), 16.f, Hi - LotTop + 4.f, FVector::UpVector);
            }
            else HouseCalderDrape::Slab(M, ConcreteKey(), HouseCalderDrape::RunOutline(P0, P1, 16.f), FDrapeWalk{ 2.f }, FDrapeRoad{ -2.f }, HouseCampusFallKit::PlainUV());
            continue;
        }
        const float Lo = F.Road(), Hi = F.Walk();
        HouseCampusFallKit::Bar(M, ConcreteKey(), FVector(P0, (Lo + Hi) * .5f), FVector(P1, (Lo + Hi) * .5f), 16.f, Hi - Lo + 4.f, FVector::UpVector);
    }
}

void Paint(FHallMesh& M, FName K, float X0, float X1, float Y0, float Y1, float Z)
{
    M.Quad(K, FVector(X0, Y0, Z), FVector(X1, Y0, Z), FVector(X1, Y1, Z), FVector(X0, Y1, Z), FVector::UpVector, HouseCampusFallKit::PlainUV());
}
/** Paint on a campus road or lot, 5 mm over the asphalt at the strip's middle (Layer: a further lift over other paint);
 *  draped, 5 mm over the road surface at every point, except where the paving under it is built flat (a lot, level by
 *  design; a road kept at today's height across a frozen zone's edge: FlatPaveTop), 5 mm over that flat top. */
void RoadPaint(FHallMesh& M, FName K, float X0, float X1, float Y0, float Y1, float Layer = 0.f)
{
    const FFooting F(Rect(X0, X1, Y0, Y1));
    if (F.Drape())
    {
        float PaveTop = 0.f;
        if (FlatPaveTop(F.C, PaveTop)) Paint(M, K, X0, X1, Y0, Y1, PaveTop + .5f + Layer);
        else HouseCalderDrape::Sheet(M, K, HouseCalderDrape::RectOutline(Rect(X0, X1, Y0, Y1)), FDrapeRoad{ .5f + Layer }, HouseCampusFallKit::PlainUV());
        return;
    }
    Paint(M, K, X0, X1, Y0, Y1, F.Road() + .5f + Layer);
}

/** B less the part of it inside Cut, where Cut spans B fully across one axis (a curb meeting another's band end-on). */
FBox2D TrimBox(FBox2D B, const FBox2D& Cut)
{
    if (!B.Intersect(Cut)) return B;
    if (Cut.Min.X <= B.Min.X && Cut.Max.X >= B.Max.X)
    {
        if (Cut.Max.Y < B.Max.Y) B.Min.Y = FMath::Max(B.Min.Y, Cut.Max.Y);
        else if (Cut.Min.Y > B.Min.Y) B.Max.Y = FMath::Min(B.Max.Y, Cut.Min.Y);
        else B.Max.Y = B.Min.Y;
    }
    else if (Cut.Min.Y <= B.Min.Y && Cut.Max.Y >= B.Max.Y)
    {
        if (Cut.Max.X < B.Max.X) B.Min.X = FMath::Max(B.Min.X, Cut.Max.X);
        else if (Cut.Min.X > B.Min.X) B.Max.X = FMath::Min(B.Max.X, Cut.Min.X);
        else B.Max.X = B.Min.X;
    }
    return B;
}

/** Curbs on every road edge that meets lawn: each edge is sampled every 25 cm just outside, and each lawn stretch gets a
 *  curb. A stretch runs one cell on under a flat sidewalk at either end, so a walk that meets the edge at an angle leaves
 *  no slit; a road's curbs stop at a lot's own curb line (Yield), so two curbs never share a face. */
void RectCurbs(FHallMesh& M, FName K, const FBox2D& R, const FHallUV& UV, bool bYieldToLots = false)
{
    struct FEdge { FVector2D A, B, Out; };
    const FEdge Edges[4] = {
        { R.Min, FVector2D(R.Max.X, R.Min.Y), FVector2D(0, -1) }, { FVector2D(R.Max.X, R.Min.Y), R.Max, FVector2D(1, 0) },
        { R.Max, FVector2D(R.Min.X, R.Max.Y), FVector2D(0, 1) }, { FVector2D(R.Min.X, R.Max.Y), R.Min, FVector2D(-1, 0) } };
    auto Put = [&](const FVector2D& P0, const FVector2D& P1)
    {
        FBox2D B(FVector2D(FMath::Min(P0.X, P1.X), FMath::Min(P0.Y, P1.Y)), FVector2D(FMath::Max(P0.X, P1.X), FMath::Max(P0.Y, P1.Y)));
        if (bYieldToLots) for (const FLot& L : Lots()) B = TrimBox(B, L.Area.ExpandBy(CurbW));
        TArray<FBox2D> Pieces;
        SplitAtFrozen(B, Pieces);
        for (const FBox2D& Piece : Pieces)
        {
            const FFooting F = PieceFooting(Piece, Pieces.Num() > 1);
            if (F.Drape()) DrapedSlab(M, K, Piece, FDrapeDatum{ Bottom }, FDrapeWalk{ 0.f }, UV);
            else Slab(M, K, Piece, F.Z(Bottom), F.Walk(), UV);
        }
    };
    for (int32 EI = 0; EI < 4; ++EI)
    {
        const FEdge& E = Edges[EI];
        const float L = FVector2D::Distance(E.A, E.B);
        const FVector2D Dir = (E.B - E.A) / L;
        const int32 N = FMath::Max(1, FMath::CeilToInt(L / 25.f));
        auto Sample = [&](int32 I) { return E.A + Dir * ((I + .5f) * L / N) + E.Out * 6.f; };
        auto FlatWalk = [&](int32 I) { if (I < 0 || I >= N) return false; const FVector2D S = Sample(I); return SidewalkAt(S) && !WedgeAt(S) && !RoadAt(S) && !StreetAt(S); };
        int32 Start = -1;
        for (int32 I = 0; I <= N; ++I)
        {
            const bool bLawn = I < N && !CurbStop(Sample(I));
            if (bLawn && Start < 0) Start = I;
            if (!bLawn && Start >= 0)
            {
                const int32 I0 = FlatWalk(Start - 1) ? Start - 1 : Start, I1 = FlatWalk(I) ? I + 1 : I;
                Put(E.A + Dir * (I0 * L / N), E.A + Dir * (I1 * L / N) + E.Out * CurbW);
                Start = -1;
            }
        }
        // The outside corner square at this edge's end, when the corner itself is lawn and both edges have a curb there.
        const FEdge& Next = Edges[(EI + 1) % 4];
        const FVector2D Corner = E.B + E.Out * (CurbW * .5f) + Next.Out * (CurbW * .5f);
        if (!CurbStop(Corner) && !CurbStop(E.B - Dir * 12.f + E.Out * 6.f) && !CurbStop(E.B + Next.Out * 6.f + (Next.B - Next.A).GetSafeNormal() * 12.f))
            Put(E.B, E.B + E.Out * CurbW + Next.Out * CurbW);
    }
}
void ArcCurbs(FHallMesh& M, FName K, const FArc& A, const FHallUV& UV)
{
    const int32 Seg = FMath::Max(2, FMath::CeilToInt(FMath::Abs(A.A1 - A.A0) / 2.5f));
    for (const bool bOuter : { false, true })
    {
        const float R = bOuter ? A.R1 : A.R0;
        if (R < 1.f) continue;
        for (int32 I = 0; I < Seg; ++I)
        {
            const float D0 = FMath::Lerp(A.A0, A.A1, float(I) / Seg), D1 = FMath::Lerp(A.A0, A.A1, float(I + 1) / Seg);
            const float Dm = FMath::DegreesToRadians((D0 + D1) * .5f);
            const FVector2D S = A.C + FVector2D(FMath::Cos(Dm), FMath::Sin(Dm)) * (R + (bOuter ? 6.f : -6.f));
            if (CurbStop(S)) continue;
            const FArc Piece{ A.C, bOuter ? R : R - CurbW, bOuter ? R + CurbW : R, D0, D1 };
            const FFooting F = ArcFooting(Piece);
            if (F.Drape()) DrapedArcSlab(M, K, Piece, FDrapeDatum{ Bottom }, FDrapeWalk{ 0.f }, UV, false);
            else ArcSlab(M, K, Piece, F.Z(Bottom), F.Walk(), UV, false);
        }
    }
}

/** Lawn over a square whose terrain was cut: every 20 cm cell that lies wholly off the road, merged into strips per row,
 *  each corner on the ground (draped: on the ground at every point). The gaps this leaves next to a road edge are always
 *  under a sidewalk or curb (both at least 30 wide). */
void LawnFill(FHallMesh& M, FName K, const FHallUV& UV, const FBox2D& Sq)
{
    constexpr float Cell = 20.f;
    auto At = [](float X, float Y) { return FVector(X, Y, HouseExteriorSite::GroundHeight(X, Y)); };
    const int32 NX = FMath::CeilToInt((Sq.Max.X - Sq.Min.X) / Cell), NY = FMath::CeilToInt((Sq.Max.Y - Sq.Min.Y) / Cell);
    for (int32 J = 0; J < NY; ++J)
    {
        const float Y0 = Sq.Min.Y + J * Cell, Y1 = FMath::Min(Sq.Max.Y, Y0 + Cell);
        int32 Start = -1;
        for (int32 I = 0; I <= NX; ++I)
        {
            bool bLawn = false;
            if (I < NX)
            {
                const float X0 = Sq.Min.X + I * Cell, X1 = FMath::Min(Sq.Max.X, X0 + Cell);
                bLawn = true;
                for (const FVector2D& P : { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1), FVector2D((X0 + X1) * .5f, (Y0 + Y1) * .5f) })
                    if (RoadAt(P)) { bLawn = false; break; }
            }
            if (bLawn && Start < 0) Start = I;
            if (!bLawn && Start >= 0)
            {
                const float X0 = Sq.Min.X + Start * Cell, X1 = FMath::Min(Sq.Max.X, Sq.Min.X + I * Cell);
                const FBox2D Strip = Rect(X0, X1, Y0, Y1);
                if (FFooting(Strip).Drape()) HouseCalderDrape::Sheet(M, K, HouseCalderDrape::RectOutline(Strip), FDrapeDatum{ Ground }, UV);
                else M.Quad(K, At(X0, Y0), At(X1, Y0), At(X1, Y1), At(X0, Y1), FVector::UpVector, UV);
                Start = -1;
            }
        }
    }
}

/** A landscaped berm: a rounded mound over Rect, its long axis along the rect's longer side, Height at the crest. */
struct FBerm { FBox2D R; float Height; };
const TArray<FBerm>& Berms()
{
    // Screening berms in open lawn, all clear of trees, walks, roads and lamps (checked by -CampusLayoutReview):
    // between the South Lot and the south lane, west of the Physical Plant's yard, and along the west road behind the
    // library and Calder Hall.
    static const TArray<FBerm> B = {
        { Rect(-1300.f, 900.f, -25700.f, -25100.f), 70.f }, { Rect(1800.f, 8200.f, -25700.f, -25100.f), 70.f },
        { Rect(10700.f, 11650.f, -22150.f, -20450.f), 95.f },
        { Rect(-20000.f, -19150.f, -15900.f, -11700.f), 85.f }, { Rect(-20000.f, -19150.f, -19300.f, -17100.f), 85.f },
    };
    return B;
}
float BermHeight(const FBerm& B, const FVector2D& P)
{
    if (!B.R.IsInside(P)) return 0.f;
    const bool bAlongX = B.R.GetSize().X >= B.R.GetSize().Y;
    const float U = bAlongX ? (P.Y - B.R.Min.Y) / B.R.GetSize().Y : (P.X - B.R.Min.X) / B.R.GetSize().X;   // across, 0..1
    const float V = bAlongX ? (P.X - B.R.Min.X) / B.R.GetSize().X : (P.Y - B.R.Min.Y) / B.R.GetSize().Y;   // along, 0..1
    const float Len = bAlongX ? B.R.GetSize().X : B.R.GetSize().Y;
    const float Across = FMath::Square(FMath::Sin(PI * U));
    const float Taper = FMath::Clamp(FMath::Min(V, 1.f - V) * Len / 450.f, 0.f, 1.f);   // 4.5 m rounded ends
    return B.Height * Across * FMath::SmoothStep(0.f, 1.f, Taper);
}
bool OnBerm(const FVector2D& P, float Pad)
{
    for (const FBerm& B : Berms()) if (B.R.ExpandBy(Pad).IsInside(P)) return true;
    return false;
}
/** A berm as a smooth height field (per-vertex normals) in the terrain's material and mapping, so its toe meets the lawn.
 *  It drapes as it stands (step 1c): every vertex reads the ground, and its 50 cm grid (the berm rects lie on it) nests in the
 *  2 m relief lattice with the same diagonal, so each triangle lies in one lattice triangle and the toe follows the ground. */
void BuildBerm(FHallMesh& M, FName K, const FHallUV& UV, const FBerm& B)
{
    constexpr float Step = 50.f;
    const int32 NX = FMath::Max(2, FMath::CeilToInt(B.R.GetSize().X / Step)), NY = FMath::Max(2, FMath::CeilToInt(B.R.GetSize().Y / Step));
    FHallSection& S = M.Sec(K);
    const int32 Base = S.V.Num();
    auto H = [&B](float X, float Y) { return HouseExteriorSite::GroundHeight(X, Y) + BermHeight(B, FVector2D(X, Y)); };
    for (int32 J = 0; J <= NY; ++J)
        for (int32 I = 0; I <= NX; ++I)
        {
            const float X = FMath::Lerp(B.R.Min.X, B.R.Max.X, float(I) / NX), Y = FMath::Lerp(B.R.Min.Y, B.R.Max.Y, float(J) / NY);
            const FVector P(X, Y, H(X, Y) + .3f);
            const FVector N = FVector(H(X - 10.f, Y) - H(X + 10.f, Y), H(X, Y - 10.f) - H(X, Y + 10.f), 20.f).GetSafeNormal();
            S.V.Add(P); S.N.Add(N); S.UV.Add(M.ComputeUV(P, FVector::UpVector, UV)); S.T.Add(FProcMeshTangent(FVector(1, 0, 0), false));
        }
    for (int32 J = 0; J < NY; ++J)
        for (int32 I = 0; I < NX; ++I)
        {
            const int32 A = Base + J * (NX + 1) + I, Bq = A + 1, C = A + NX + 1, D = C + 1;
            // Front faces point up: UE fronts wind opposite to the right-hand cross product (see FHallMesh::Quad).
            S.I.Append({ A, D, Bq, A, C, D });
        }
}
/** A low stone seat wall round the tower plaza, open wherever a walk or forecourt meets the plaza. It rides the tower's
 *  knoll with the plaza (HouseClockTowerSite::Lift). */
void SeatWall(FHallMesh& M, const FVector2D& C, float R)
{
    constexpr int32 Seg = 96;
    const float Knoll = HouseClockTowerSite::Lift();
    for (int32 I = 0; I < Seg; ++I)
    {
        const float D0 = 360.f * I / Seg, D1 = 360.f * (I + 1) / Seg, Dm = FMath::DegreesToRadians((D0 + D1) * .5f);
        const FVector2D Out = C + FVector2D(FMath::Cos(Dm), FMath::Sin(Dm)) * (R + 70.f);
        bool bOpen = SidewalkAt(Out);
        for (const FRun& W : Walks()) bOpen |= RunBuilt(W) && InRun(W, Out, 40.f);
        for (const FPlazaRect& Q : PlazaRects()) bOpen |= Q.R.ExpandBy(40.f).IsInside(Out);
        bOpen |= HouseCalderFeatures::InRamp(Out, 40.f) || HouseCalderFeatures::InStair(Out, 40.f);   // the knoll's stair and ramps arrive here
        if (bOpen) continue;
        ArcSlab(M, TEXT("EX_Stone"), FArc{ C, R - 40.f, R, D0, D1 }, Ground - 5.f + Knoll, WalkTop + 45.f + Knoll, FHallUV::World(180.f));
    }
}

/** The stalls of every lot, in a fixed order (row by row, west to east), skipping those that touch a tree island. */
void Stalls(TArray<FTransform>& Out)
{
    for (const FStallRow& R : StallRows())
        for (float X = R.X0 + StallW * .5f; X < R.X1; X += StallW)
        {
            const FVector2D C(X, (R.Y0 + R.Y1) * .5f);
            if (InIsland(C, 140.f)) continue;
            Out.Add(FTransform(FRotator(0.f, R.Yaw, 0.f), FVector(C, RoadZ(C))));
        }
}

// ------------------------------------------------------------------ the sign painter (carved limestone panels)
void SignTexture(UHouseBlackHallSubsystem* Look, FName K, const FString& Line1, const FString& Line2, bool bDarkStone)
{
    constexpr int32 W = 1024, H = 128;
    FHallImage Img;
    const FLinearColor Stone = bDarkStone ? FLinearColor(.16f, .15f, .13f) : FLinearColor(.46f, .44f, .38f);
    Img.Init(W, H, Stone);
    for (int32 Y = 0; Y < H; ++Y) for (int32 X = 0; X < W; ++X) Img.At(X, Y) = Stone * (.9f + .14f * HallNoise(X * .21f, Y * .21f, 4411));
    Img.Rect(FVector2D(5, 5), FVector2D(W - 5, H - 5), Stone * .8f);
    Img.Rect(FVector2D(10, 10), FVector2D(W - 10, H - 10), Stone);
    const FLinearColor Ink = bDarkStone ? FLinearColor(.78f, .62f, .2f) : FLinearColor(.05f, .045f, .04f);
    FHallTextStyle S;
    S.Height = Line2.IsEmpty() ? H * .5f : H * .4f; S.Stroke = S.Height * .13f; S.Spacing = .22f;
    S.WidthScale = FMath::Min(1.f, (W - 60.f) / HallTextWidth(Line1, S));
    HallDrawText(Img, Line1, FVector2D((W - HallTextWidth(Line1, S)) * .5f, Line2.IsEmpty() ? H * .75f : H * .55f), S, Ink);
    if (!Line2.IsEmpty())
    {
        FHallTextStyle T = S; T.Height = H * .2f; T.Stroke = T.Height * .14f; T.WidthScale = FMath::Min(1.f, (W - 60.f) / HallTextWidth(Line2, T));
        HallDrawText(Img, Line2, FVector2D((W - HallTextWidth(Line2, T)) * .5f, H * .86f), T, Ink);
    }
    UTexture2D* Tex = HallMakeTexture(Img, true, false, *K.ToString(), false);
    if (Tex) Look->Textures.Add(K, Tex);
    HallMakeTextured(Look, K, Tex, nullptr, .85f, FLinearColor::White, 0.f, 0.f, bDarkStone ? FLinearColor(.02f, .016f, .006f) : FLinearColor(.012f, .011f, .009f));
}
}

// ------------------------------------------------------------------ public
bool HouseCampusLayout::Enabled() { static const bool bOn = !FParse::Param(FCommandLine::Get(), TEXT("NoCampusLayout")); return bOn; }
const TArray<HouseCampusLayout::FBlock>& HouseCampusLayout::Blocks() { return HouseCampusLayoutImpl::BlockTable(); }
int32 HouseCampusLayout::Find(const TCHAR* Id)
{
    for (int32 I = 0; I < Blocks().Num(); ++I) if (FCString::Strcmp(Blocks()[I].Id, Id) == 0) return I;
    return INDEX_NONE;
}
bool HouseCampusLayout::IsDorm(int32 Seed) { return Blocks().IsValidIndex(Seed) && Blocks()[Seed].Kind == EKind::Dorm; }
FName HouseCampusLayout::SignKey(int32 Seed) { return Blocks().IsValidIndex(Seed) ? FName(*FString::Printf(TEXT("CL_Sign_%s"), Blocks()[Seed].Id)) : NAME_None; }
float HouseCampusLayout::FaceYaw(TCHAR Face) { return Face == 'S' ? 180.f : Face == 'E' ? -90.f : Face == 'W' ? 90.f : 0.f; }
FVector HouseCampusLayout::FaceNormal(TCHAR Face) { return Face == 'S' ? FVector(0, -1, 0) : Face == 'E' ? FVector(1, 0, 0) : Face == 'W' ? FVector(-1, 0, 0) : FVector(0, 1, 0); }

void HouseCampusLayout::CreateLook(UHouseBlackHallSubsystem* Look)
{
    if (!Look || !Enabled() || Look->Materials.Contains(TEXT("CL_EntranceSign"))) return;
    for (int32 I = 0; I < Blocks().Num(); ++I) HouseCampusLayoutImpl::SignTexture(Look, SignKey(I), Blocks()[I].Name, FString(), false);
    HouseCampusLayoutImpl::SignTexture(Look, TEXT("CL_EntranceSign"), TEXT("CALDER STATE UNIVERSITY"), TEXT("FOUNDED 1891"), true);
    HallMakePlain(Look, TEXT("CL_Mulch"), FLinearColor(.07f, .045f, .025f), .95f);
    HallMakePlain(Look, TEXT("CL_Hedge"), FLinearColor(.035f, .07f, .03f), .9f);
    HallMakePlain(Look, TEXT("CL_Tie"), FLinearColor(.06f, .045f, .035f), .9f);   // creosoted railroad ties
    {
        // Crushed-stone gravel: grey-beige speckle with a few darker stones and wheel-worn patches.
        constexpr int32 N = 256;
        FHallImage Img;
        Img.Init(N, N, FLinearColor(.24f, .22f, .19f));
        FRandomStream R(19860);
        for (int32 Y = 0; Y < N; ++Y)
            for (int32 X = 0; X < N; ++X)
            {
                const float Grain = R.FRandRange(.72f, 1.25f), Patch = .9f + .2f * HallNoise(X * .05f, Y * .05f, 77);
                Img.At(X, Y) = FLinearColor(.24f, .22f, .19f) * Grain * Patch;
            }
        for (int32 K = 0; K < 900; ++K) { const int32 X = R.RandRange(0, N - 2), Y = R.RandRange(0, N - 2); const FLinearColor C = FLinearColor(.1f, .095f, .085f) * R.FRandRange(.8f, 1.4f); Img.At(X, Y) = C; Img.At(X + 1, Y) = C; Img.At(X, Y + 1) = C; }
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CL_Gravel"), true);
        if (Tex) Look->Textures.Add(TEXT("CL_Gravel"), Tex);
        HallMakeTextured(Look, TEXT("CL_Gravel"), Tex, nullptr, .95f);
    }
}

void HouseCampusLayout::TerrainCuts(TArray<FBox2D>& Out)
{
    if (!Enabled()) return;
    HouseCalderRoads::TerrainCuts(Out);
    Out.Append(HouseCampusLayoutImpl::RoadRects());
    for (const HouseCampusLayoutImpl::FLot& L : HouseCampusLayoutImpl::Lots()) Out.Add(L.Area);
    for (const HouseCampusLayoutImpl::FWedge& W : HouseCampusLayoutImpl::Wedges()) if (!W.bOnStreet) Out.Add(W.R);
    Out.Add(HouseCampusLayoutImpl::CurveSquare);
    Out.Add(HouseCampusLayoutImpl::BendSquare);
    Out.Add(HouseCampusLayoutImpl::LoopSquare);
}

void HouseCampusLayout::FarSidewalkGaps(TArray<FVector2D>& Out)
{
    if (!Enabled()) return;
    for (const FVector2D& G : HouseCampusLayoutImpl::FarGaps) Out.Add(G);
}

void HouseCampusLayout::BuildFallFloors(FHallMesh& M)
{
    // Homecoming draws its own gate path and kiosk plaza (HouseCampusFallQuad), with no collision. The floor under them is
    // this: the same slabs as the plan's, hidden, so feet and the review land on walk height there too.
    namespace CL = HouseCampusLayoutImpl;
    if (!Enabled() || !HouseCampusFall::Enabled()) return;
    const FHallUV& CUV = CL::ConcreteUV();
    for (const CL::FRun& W : CL::Walks()) if (W.bFall) CL::Run(M, CL::ConcreteKey(), W, CUV);   // draped on the relief (CL::Run)
    for (const CL::FPlazaRect& P : CL::PlazaRects())   // a plaza is level by design (draped only where the relief is not: CL::PlazaDrapes)
        if (P.bFall)
        {
            const CL::FFooting F(P.R);
            if (CL::PlazaDrapes(P.R)) CL::DrapedSlab(M, CL::ConcreteKey(), P.R, CL::FDrapeDatum{ CL::Ground - 5.f }, CL::FDrapeWalk{ 0.f }, CUV);
            else CL::Slab(M, CL::ConcreteKey(), P.R, F.Z(CL::Ground - 5.f), F.Walk(), CUV);
        }
}

void HouseCampusLayout::BuildGround(FHallMesh& M, FName GrassKey, const FHallUV& TerrainUV)
{
    namespace CL = HouseCampusLayoutImpl;
    const FName Asphalt = CL::AsphaltKey(), Concrete = CL::ConcreteKey();
    const FHallUV& AUV = CL::AsphaltUV();
    const FHallUV& CUV = CL::ConcreteUV();
    const FName Line(TEXT("EX_Line")), Yellow(TEXT("EX_Yellow"));
    HouseCalderRoads::BuildGround(M, Asphalt, AUV);
    // Every flat piece stands on the ground at its own middle (CL::FFooting); road paint lies 5 mm over the asphalt there. On
    // the relief (CL::FFooting::Drape) roads, walks, ramps, curbs, paint and overlays are draped on the ground instead; the lots
    // (with their islands, stall lines and wheel stops), plazas, the tower plaza and its seat wall stay level at their own height.

    // Carriageways, drives, lanes and the yard; the curve, the bend and the bus loop; the lots.
    for (const FBox2D& R0 : CL::RoadRects())
    {
        TArray<FBox2D> Pieces;
        CL::SplitAtFrozen(R0, Pieces);
        CL::CutRoadReplacements(Pieces);
        for (const FBox2D& R : Pieces)
        {
            const CL::FFooting F = CL::PieceFooting(R, Pieces.Num() > 1);
            if (F.Drape()) CL::DrapedSlab(M, Asphalt, R, CL::FDrapeDatum{ CL::Bottom }, CL::FDrapeRoad{ 0.f }, AUV);
            else CL::Slab(M, Asphalt, R, F.Z(CL::Bottom), F.Road(), AUV);
        }
    }
    {
        const CL::FFooting F = CL::ArcFooting(CL::Curve);
        if (F.Drape()) CL::DrapedArcSlab(M, Asphalt, CL::Curve, CL::FDrapeDatum{ CL::Bottom }, CL::FDrapeRoad{ 0.f }, AUV, false);
        else CL::ArcSlab(M, Asphalt, CL::Curve, F.Z(CL::Bottom), F.Road(), AUV, false);
    }
    {
        const CL::FArc BendSlab = CL::Circumscribed(CL::Bend);
        const CL::FFooting F = CL::ArcFooting(BendSlab);
        if (F.Drape()) CL::DrapedArcSlab(M, Asphalt, BendSlab, CL::FDrapeDatum{ CL::Bottom }, CL::FDrapeRoad{ 0.f }, AUV, false);
        else CL::ArcSlab(M, Asphalt, BendSlab, F.Z(CL::Bottom), F.Road(), AUV, false);
    }
    {
        const CL::FFooting F(CL::LoopCentre, CL::LoopR);   // on the stadium's apron ring (the terrain holds it at today's grade)
        const float LoopDiscR = CL::LoopR / FMath::Cos(PI / 48.f) + 1.f;   // chords reach under the curbs and the ring walk
        if (F.Drape()) CL::DrapedDisc(M, Asphalt, CL::LoopCentre, LoopDiscR, CL::FDrapeDatum{ CL::Bottom }, CL::FDrapeRoad{ 0.f }, AUV);
        else CL::Disc(M, Asphalt, CL::LoopCentre, LoopDiscR, F.Z(CL::Bottom), F.Road(), AUV);
    }
    for (const CL::FLot& L : CL::Lots())
    {
        // A lot (a lot tier) is level by design, never draped: the landform brings its ground to it (CL::FlatPaveTop reads the
        // same top). Its islands, stall lines and wheel stops lie on it, so on the relief they stay level with it too.
        const CL::FFooting F(L.Area);
        const FName LotKey = L.bGravel ? FName(TEXT("CL_Gravel")) : Asphalt;
        const FHallUV& LotUV = L.bGravel ? FHallUV::World(300.f) : AUV;
        // On the relief a lot is draped: the landform levels its tiers and gives them their falls (the South Lot's three tiers
        // step 3 and 4 m behind stone walls; built as one flat slab it floated 2.75 m over tier A and lay 4.25 m down in tier C).
        const bool bLotDrapes = F.Drape();
        if (bLotDrapes)
        {
            TArray<FBox2D> LotPieces={L.Area};
            CL::CutRoadReplacements(LotPieces);
            for(const FBox2D& Piece:LotPieces) CL::DrapedSlab(M,LotKey,Piece,CL::FDrapeDatum{CL::Bottom},CL::FDrapeRoad{0.f},LotUV);
        }
        else CL::Slab(M, LotKey, L.Area, F.Z(CL::Bottom), F.Road(), LotUV);
        for (const FVector2D& C : L.Islands)
        {
            CL::CurbRing(M, C, 320.f);
            const CL::FFooting Island(C, 320.f);
            if (bLotDrapes)   // on the draped lot: today's datums carried onto the lot's surface at every point
                CL::DrapedDisc(M, TEXT("CL_Mulch"), C, 320.f, CL::FDrapeDatum{ CL::Bottom }, CL::FDrapeWalk{ -.5f }, HouseCampusFallKit::PlainUV(), 32);
            else if (Island.Drape())   // level on the lot: today's datums carried onto the lot's top
                CL::Disc(M, TEXT("CL_Mulch"), C, 320.f, F.Road() + (CL::Bottom - CL::RoadTop), F.Road() + (CL::WalkTop - CL::RoadTop) - .5f, HouseCampusFallKit::PlainUV(), 32);
            else CL::Disc(M, TEXT("CL_Mulch"), C, 320.f, Island.Z(CL::Bottom), Island.Walk() - .5f, HouseCampusFallKit::PlainUV(), 32);   // rim on the ring's own polygon, inside the bars
        }
    }
    // Calder Dr: the planted median, the double yellow south of it, edge lines, and the crosswalks.
    {
        // The median stops either side of each crosswalk it meets, so the walk crosses at road level (a refuge).
        float Y1 = CL::Median.Max.Y;
        auto Piece = [&](float Y0)
        {
            if (Y1 - Y0 < 30.f) return;
            const FBox2D P(FVector2D(CL::Median.Min.X, Y0), FVector2D(CL::Median.Max.X, Y1));
            const CL::FFooting F(P);
            if (F.Drape())
            {
                // Calder Dr kept flat (its rect straddles the frozen street's edge, CL::FlatPaveTop): so is its median on it.
                float LaneTop = 0.f;
                if (CL::FlatPaveTop(P.GetCenter(), LaneTop))
                {
                    const float Kerb = LaneTop + (CL::WalkTop - CL::RoadTop);
                    CL::Slab(M, Concrete, P, LaneTop + (CL::Bottom - CL::RoadTop), Kerb, CUV);
                    CL::Slab(M, TEXT("EX_Grass"), FBox2D(P.Min + FVector2D(14.f, 14.f), P.Max - FVector2D(14.f, 14.f)), Kerb, Kerb + .6f, CL::GrassUV());
                    return;
                }
                CL::DrapedSlab(M, Concrete, P, CL::FDrapeDatum{ CL::Bottom }, CL::FDrapeWalk{ 0.f }, CUV);
                CL::DrapedSlab(M, TEXT("EX_Grass"), FBox2D(P.Min + FVector2D(14.f, 14.f), P.Max - FVector2D(14.f, 14.f)), CL::FDrapeWalk{ 0.f }, CL::FDrapeWalk{ .6f }, CL::GrassUV());
                return;
            }
            const float Kerb = F.Walk();
            CL::Slab(M, Concrete, P, F.Z(CL::Bottom), Kerb, CUV);
            CL::Slab(M, TEXT("EX_Grass"), FBox2D(P.Min + FVector2D(14.f, 14.f), P.Max - FVector2D(14.f, 14.f)), Kerb, Kerb + .6f, CL::GrassUV());
        };
        for (const float Y : CL::Crosswalks)   // listed north to south
            if (Y > CL::Median.Min.Y && Y < CL::Median.Max.Y) { Piece(Y + CL::CrossHalf); Y1 = Y - CL::CrossHalf; }
        Piece(CL::Median.Min.Y);
    }
    CL::RoadPaint(M, Yellow, 9478.f, 9490.f, CL::DriveSouthY, CL::Median.Min.Y);
    CL::RoadPaint(M, Yellow, 9510.f, 9522.f, CL::DriveSouthY, CL::Median.Min.Y);
    for (const float X : { CL::DriveX0 + 70.f, CL::DriveX1 - 80.f }) CL::RoadPaint(M, Line, X, X + 10.f, CL::DriveSouthY, -5900.f);
    for (const float Y : CL::Crosswalks)
    {
        for (float X = CL::DriveX0 + 60.f; X < CL::DriveX1 - 60.f; X += 130.f) CL::RoadPaint(M, Line, X, X + 60.f, Y - CL::CrossHalf + 15.f, Y + CL::CrossHalf - 15.f, .05f);
    }
    // The curve's double yellow, Stadium Dr's, and the bus bay's stall lines.
    {
        const FVector2D C = CL::Curve.C;
        constexpr int32 Seg = 36;
        for (int32 K = 0; K < Seg; ++K)
        {
            const float A0 = FMath::DegreesToRadians(-90.f * K / Seg), A1 = FMath::DegreesToRadians(-90.f * (K + 1) / Seg);
            auto P = [&](float A, float R) { const FVector2D Q(C.X + FMath::Cos(A) * R, C.Y + FMath::Sin(A) * R); return FVector(Q, CL::RoadZ(Q) + .5f); };
            auto At = [&](float A, float R) { return FVector2D(C.X + FMath::Cos(A) * R, C.Y + FMath::Sin(A) * R); };
            for (const float R : { 1788.f, 1812.f })
            {
                const TArray<FVector2D> Strip = { At(A0, R - 6.f), At(A1, R - 6.f), At(A1, R + 6.f), At(A0, R + 6.f) };
                FBox2D Foot(ForceInit);
                for (const FVector2D& S : Strip) Foot += S;
                if (CL::FFooting(Foot).Drape()) HouseCalderDrape::Sheet(M, Yellow, Strip, CL::FDrapeRoad{ .5f }, HouseCampusFallKit::PlainUV());
                else M.Quad(Yellow, P(A0, R - 6.f), P(A1, R - 6.f), P(A1, R + 6.f), P(A0, R + 6.f), FVector::UpVector, HouseCampusFallKit::PlainUV());
            }
        }
        CL::RoadPaint(M, Yellow, 200.f, 7700.f, -19012.f, -19000.f);   // Stadium Dr and the bus bay: on the apron ring
        CL::RoadPaint(M, Yellow, 200.f, 7700.f, -18990.f, -18978.f);
        for (float X = CL::BusBay.Min.X; X <= CL::BusBay.Max.X; X += 1220.f) CL::RoadPaint(M, Line, X, X + 10.f, CL::BusBay.Min.Y, CL::BusBay.Max.Y);
    }
    // The loop's planted island.
    CL::CurbRing(M, CL::LoopCentre, CL::LoopIslandR);
    {
        const CL::FFooting F(CL::LoopCentre, CL::LoopIslandR);
        if (F.Drape()) CL::DrapedDisc(M, TEXT("EX_Grass"), CL::LoopCentre, CL::LoopIslandR, CL::FDrapeDatum{ CL::Bottom }, CL::FDrapeWalk{ -.5f }, CL::GrassUV(), 32);
        else CL::Disc(M, TEXT("EX_Grass"), CL::LoopCentre, CL::LoopIslandR, F.Z(CL::Bottom), F.Walk() - .5f, CL::GrassUV(), 32);
    }
    // Stall lines and wheel stops.
    for (const CL::FStallRow& R : CL::StallRows())
    {
        const bool bNoseSouth = R.Yaw < 0.f;
        for (float X = R.X0; X <= R.X1 + 1.f; X += CL::StallW)
            if (!CL::InIsland(FVector2D(X, (R.Y0 + R.Y1) * .5f), 10.f)) CL::RoadPaint(M, Line, X - 5.f, X + 5.f, R.Y0 + 20.f, R.Y1 - 20.f);
        for (float X = R.X0 + CL::StallW * .5f; X < R.X1; X += CL::StallW)
        {
            if (CL::InIsland(FVector2D(X, (R.Y0 + R.Y1) * .5f), 140.f)) continue;
            const float WY = bNoseSouth ? R.Y0 + 45.f : R.Y1 - 45.f;
            const CL::FFooting StopFoot(CL::Rect(X - 90.f, X + 90.f, WY - 8.f, WY + 8.f));
            if (StopFoot.Drape())   // every stall row is on a lot: the stop stands level on the lot's top
            {
                float LotTop = 0.f;
                if (CL::FlatPaveTop(StopFoot.C, LotTop)) M.BoxMinMax(Concrete, FVector(X - 90.f, WY - 8.f, LotTop), FVector(X + 90.f, WY + 8.f, LotTop + 10.f), CUV);
                else CL::DrapedSlab(M, Concrete, StopFoot.Span, CL::FDrapeRoad{ 0.f }, CL::FDrapeRoad{ 10.f }, CUV);
                continue;
            }
            const float Stop = StopFoot.Road();
            M.BoxMinMax(Concrete, FVector(X - 90.f, WY - 8.f, Stop), FVector(X + 90.f, WY + 8.f, Stop + 10.f), CUV);
        }
    }
    // Sidewalks, curb ramps and drive crossings, and the curbs wherever a road meets lawn.
    for (const FBox2D& S : CL::Sidewalks())
    {
        const CL::FFooting F(S);
        if (F.Drape()) CL::DrapedSlab(M, Concrete, S, CL::FDrapeDatum{ CL::Bottom }, CL::FDrapeWalk{ 0.f }, CUV);
        else CL::Slab(M, Concrete, S, F.Z(CL::Bottom), F.Walk(), CUV);
    }
    for (const CL::FArc& A : CL::SidewalkArcs())
    {
        const CL::FFooting F = CL::ArcFooting(A);
        if (F.Drape()) CL::DrapedArcSlab(M, Concrete, A, CL::FDrapeDatum{ CL::Bottom }, CL::FDrapeWalk{ 0.f }, CUV);
        else CL::ArcSlab(M, Concrete, A, F.Z(CL::Bottom), F.Walk(), CUV);
    }
    for (const CL::FWedge& W : CL::Wedges())
    {
        CL::Wedge(M, Concrete, W, CUV);
        if (W.bOnStreet) continue;
        // Cheek walls along a ramp's sides where they meet lawn, so no slit opens beside the dipping slab.
        const bool bAlongX = W.LowSide < 2;
        for (const float S : { -1.f, 1.f })
        {
            const FVector2D Mid = W.R.GetCenter() + (bAlongX ? FVector2D(0.f, S * (W.R.GetExtent().Y + 6.f)) : FVector2D(S * (W.R.GetExtent().X + 6.f), 0.f));
            if (CL::CurbStop(Mid) || CL::WalkAt(Mid, 0.f)) continue;
            const FBox2D Cheek = bAlongX ? FBox2D(FVector2D(W.R.Min.X, S < 0 ? W.R.Min.Y : W.R.Max.Y - CL::CheekW), FVector2D(W.R.Max.X, S < 0 ? W.R.Min.Y + CL::CheekW : W.R.Max.Y))
                                         : FBox2D(FVector2D(S < 0 ? W.R.Min.X : W.R.Max.X - CL::CheekW, W.R.Min.Y), FVector2D(S < 0 ? W.R.Min.X + CL::CheekW : W.R.Max.X, W.R.Max.Y));
            const CL::FFooting F(Cheek);
            if (F.Drape()) CL::DrapedSlab(M, Concrete, Cheek, CL::FDrapeDatum{ CL::Bottom }, CL::FDrapeWalk{ 0.f }, CUV);
            else CL::Slab(M, Concrete, Cheek, F.Z(CL::Bottom), F.Walk(), CUV);
        }
    }
    for (const FBox2D& R : CL::RoadRects()) CL::RectCurbs(M, Concrete, R, CUV, true);
    for (const CL::FLot& L : CL::Lots()) CL::RectCurbs(M, L.bGravel ? FName(TEXT("CL_Tie")) : Concrete, L.Area, L.bGravel ? FHallUV::World(120.f) : CUV);
    CL::ArcCurbs(M, Concrete, CL::Curve, CUV);
    CL::ArcCurbs(M, Concrete, CL::Bend, CUV);
    CL::ArcCurbs(M, Concrete, CL::FArc{ CL::LoopCentre, 0.f, CL::LoopR, -180.f, 180.f }, CUV);
    // The mid-block crosswalk on Fraternity Park Dr: ladder bars between edge lines (its curb ramps are wedges above; the
    // junction's bars are the street scene's own). The street is frozen, so its paint keeps today's literal height.
    for (const FVector2D& C : CL::StreetCrossings)
    {
        if (C.X > 8000.f) continue;
        for (float Y = CL::StreetFarKerbY + 170.f; Y < CL::StreetNearKerbY - 170.f; Y += 118.f) CL::Paint(M, Line, C.X + 15.f, C.Y - 15.f, Y, Y + 60.f, CL::StreetZ + .9f);
        for (const float X : { C.X, C.Y - 12.f }) CL::Paint(M, Line, X, X + 12.f, CL::StreetFarKerbY + 150.f, CL::StreetNearKerbY - 150.f, CL::StreetZ + .85f);
    }
    // Walks (draped on the relief, CL::Run) and plazas (level by design, never draped: the landform brings the ground to them).
    for (const CL::FRun& W : CL::Walks()) if (CL::RunVisible(W)) CL::Run(M, Concrete, W, CUV);
    const float Knoll = HouseClockTowerSite::Lift();   // the tower plaza (the one disc) rides the tower's knoll
    for (const CL::FPlaza& P : CL::PlazaDiscs()) CL::Disc(M, Concrete, P.C, P.R, CL::Ground - 5.f + Knoll, CL::WalkTop + Knoll, CUV);
    for (const CL::FPlazaRect& P : CL::PlazaRects())
        if (CL::PlazaVisible(P.bFall))
        {
            const CL::FFooting F(P.R);
            if (CL::PlazaDrapes(P.R)) CL::DrapedSlab(M, Concrete, P.R, CL::FDrapeDatum{ CL::Ground - 5.f }, CL::FDrapeWalk{ 0.f }, CUV);
            else CL::Slab(M, Concrete, P.R, F.Z(CL::Ground - 5.f), F.Walk(), CUV);
        }
    // Topography: landscaped berms (they drape as they stand, CL::BuildBerm) and the tower plaza's stone seat wall (level, on
    // the knoll with the plaza).
    for (const CL::FBerm& Bm : CL::Berms()) CL::BuildBerm(M, GrassKey, TerrainUV, Bm);
    CL::SeatWall(M, CL::PlazaDiscs()[0].C, CL::PlazaDiscs()[0].R);
    // Lawn where the terrain was cut round the curved roads.
    for (const FBox2D& Sq : { CL::CurveSquare, CL::BendSquare, CL::LoopSquare }) CL::LawnFill(M, GrassKey, TerrainUV, Sq);

    // The entrance: brick gateposts with stone caps either side of Calder Dr, and the sign wall on the east lawn, each on a
    // level footing at the lowest ground under it; the hedge balls each on the ground under it.
    for (const float X : { 8330.f, 10670.f })
    {
        const float G = CL::LowestGround(CL::Rect(X - 45.f, X + 45.f, -6195.f, -6105.f));
        M.BoxMinMax(TEXT("EX_Brick"), FVector(X - 45.f, -6195.f, G - 20.f), FVector(X + 45.f, -6105.f, G + 230.f), FHallUV::World(192.f));
        M.BoxMinMax(TEXT("EX_Stone"), FVector(X - 55.f, -6205.f, G + 230.f), FVector(X + 55.f, -6095.f, G + 248.f), FHallUV::World(180.f));
        M.Dome(TEXT("EX_Stone"), FVector(X, -6150.f, G + 248.f), FVector::UpVector, 26.f, 22.f, 12);
    }
    {
        const float X0 = 10900.f, X1 = 12900.f, Y0 = -6260.f, Y1 = -6160.f;
        const float G = CL::LowestGround(CL::Rect(X0, X1, Y0, Y1));
        M.BoxMinMax(TEXT("EX_Brick"), FVector(X0, Y0, G - 20.f), FVector(X1, Y1, G + 120.f), FHallUV::World(192.f));
        M.BoxMinMax(TEXT("EX_Stone"), FVector(X0 - 10.f, Y0 - 10.f, G + 120.f), FVector(X1 + 10.f, Y1 + 10.f, G + 134.f), FHallUV::World(180.f));
        HouseCampusFallKit::Card(M, TEXT("CL_EntranceSign"), FVector((X0 + X1) * .5f, Y1 + 1.f, G + 62.f), FVector(1, 0, 0), FVector::UpVector, 1800.f, 100.f, FVector(0, 1, 0));
        for (float X = X0 + 150.f; X < X1; X += 260.f)
            HouseCampusFallKit::Ball(M, TEXT("CL_Hedge"), FVector(X, Y1 + 90.f, CL::GroundAt(FVector2D(X, Y1 + 90.f)) + 30.f), 52.f, 10);
    }
    // Park benches: wooden slats on cast-iron ends.
    TArray<FTransform> Benches;
    BenchPoints(Benches);
    for (const FTransform& T : Benches)
    {
        const FQuat Q = T.GetRotation();
        auto At = [&](float X, float Y, float Z) { return T.GetLocation() + Q.RotateVector(FVector(X, Y, Z)); };
        for (int32 K = 0; K < 5; ++K) M.Box(TEXT("EX_Wood"), FTransform(Q, At(0.f, -22.f + K * 11.f, 44.5f)), FVector(110.f, 4.5f, 2.5f), HouseCampusFallKit::PlainUV());
        for (int32 K = 0; K < 4; ++K) M.Box(TEXT("EX_Wood"), FTransform(Q, At(0.f, 30.f, 57.f + K * 11.f)), FVector(110.f, 3.f, 4.f), HouseCampusFallKit::PlainUV());
        for (const float X : { -85.f, 85.f })
        {
            M.Box(TEXT("EX_Metal"), FTransform(Q, At(X, 0.f, 21.f)), FVector(3.f, 28.f, 21.f), HouseCampusFallKit::PlainUV());
            M.Box(TEXT("EX_Metal"), FTransform(Q, At(X, 31.f, 70.f)), FVector(3.f, 3.f, 28.f), HouseCampusFallKit::PlainUV());
        }
    }
}

bool HouseCampusLayout::Occupied(const FVector2D& P, float Pad)
{
    if (!Enabled()) return false;
    if (HouseCalderRoads::Contains(P, Pad)) return true;
    for (const FBlock& B : Blocks()) if (B.Rect().ExpandBy(Pad).IsInside(P)) return true;
    if (HouseCampusLayoutImpl::RoadAt(P, Pad) && !HouseCampusLayoutImpl::InIsland(P, -Pad)) return true;
    return HouseCampusLayoutImpl::SidewalkAt(P, Pad) || HouseCampusLayoutImpl::WalkAt(P, Pad);
}

void HouseCampusLayout::WalkStrips(TArray<FWalkStrip>& Out)
{
    if (!Enabled()) return;
    for (const HouseCampusLayoutImpl::FRun& W : HouseCampusLayoutImpl::Walks())
        if (HouseCampusLayoutImpl::RunBuilt(W) && HouseCampusLayoutImpl::RunVisible(W)) Out.Add({ W.A, W.B, W.W });
}
void HouseCampusLayout::PavedRects(TArray<FBox2D>& Out)
{
    if (!Enabled()) return;
    Out.Append(HouseCampusLayoutImpl::Sidewalks());
    for (const HouseCampusLayoutImpl::FPlazaRect& P : HouseCampusLayoutImpl::PlazaRects()) if (HouseCampusLayoutImpl::PlazaVisible(P.bFall)) Out.Add(P.R);
}
void HouseCampusLayout::PavedDiscs(TArray<FVector>& Out)
{
    if (!Enabled()) return;
    for (const HouseCampusLayoutImpl::FPlaza& P : HouseCampusLayoutImpl::PlazaDiscs()) Out.Add(FVector(P.C, P.R));
}
bool HouseCampusLayout::FlatPaveTop(const FVector2D& P, float& OutTop) { return Enabled() && HouseCampusLayoutImpl::FlatPaveTop(P, OutTop); }
bool HouseCampusLayout::PavedRectFlatTop(const FBox2D& R, float& OutTop)
{
    // As BuildGround and BuildFallFloors build them: a plaza level at its footing's walk height, a sidewalk draped unless its
    // footprint straddles a frozen zone's edge (FFooting::Drape; where the relief round it is 0, draped and flat agree).
    namespace CL = HouseCampusLayoutImpl;
    if (!Enabled()) return false;
    for (const CL::FPlazaRect& P : CL::PlazaRects())
        if (P.R == R)
        {
            if (CL::PlazaDrapes(P.R)) return false;   // draped on the relief: its top is HouseCalderTerrain::SurfaceZ (walk class)
            OutTop = CL::FFooting(P.R).Walk();
            return true;
        }
    for (const FBox2D& S : CL::Sidewalks())
        if (S == R)
        {
            const CL::FFooting F(S);
            if (!F.bFrozenEdge) return false;
            OutTop = F.Walk();
            return true;
        }
    return false;
}

namespace HouseCampusLayoutImpl
{
/** On the Calder terrain: the nearest declared wall line within Reach of P where it really steps (3 cm or more between the
 *  ground 5 cm either side of the line at P's foot on it). OutHigh is the unit normal toward its high side, OutSide P's
 *  signed distance from the line along it. (The roadside audit, data_exports.md: props on the old flat datums met them.) */
bool NearStepWall(const FVector2D& P, float Reach, FVector2D& OutHigh, float& OutSide)
{
    bool bFound = false;
    float Best = Reach;
    for (const HouseCalderLandform::FWall& Wl : HouseCalderLandform::Walls())
    {
        const FVector2D D = Wl.B - Wl.A;
        const float Len = float(D.Size());
        if (Len < 1.f) continue;
        const FVector2D U = D / Len, N(-U.Y, U.X);
        const FVector2D Q = Wl.A + U * FMath::Clamp(float(FVector2D::DotProduct(P - Wl.A, U)), 0.f, Len);
        const float Dist = float(FVector2D::Distance(P, Q));
        if (Dist >= Best) continue;
        const float Step = HouseExteriorSite::GroundHeight(Q.X + N.X * 5.f, Q.Y + N.Y * 5.f) - HouseExteriorSite::GroundHeight(Q.X - N.X * 5.f, Q.Y - N.Y * 5.f);
        if (FMath::Abs(Step) < 3.f) continue;
        Best = Dist;
        bFound = true;
        OutHigh = Step > 0.f ? N : -N;
        OutSide = float(FVector2D::DotProduct(P - Q, OutHigh));
    }
    return bFound;
}
}

void HouseCampusLayout::LampPoints(TArray<FVector2D>& Out)
{
    // The street verge row stays, minus the lamps that would now stand in a drive or through the shuttle shelter.
    for (float X = -16000.f; X < 17000.f; X += 1900.f)
    {
        if ((X > HouseCampusLayoutImpl::FarGaps[1].X && X < HouseCampusLayoutImpl::FarGaps[1].Y) || (X > -3300.f && X < -2400.f) || (X > 8300.f && X < 10700.f)) continue;
        Out.Add(FVector2D(X, -5900.f));
    }
    auto Clear = [&Out](const FVector2D& P)
    {
        for (const FBlock& B : Blocks()) if (B.Rect().ExpandBy(150.f).IsInside(P)) return false;
        if (HouseCampusLayoutImpl::RoadAt(P, 120.f) || HouseCampusLayoutImpl::SidewalkAt(P, 40.f) || HouseCampusLayoutImpl::WalkAt(P, 30.f) || HouseCampusLayoutImpl::OnBerm(P, 80.f)) return false;
        if (FMath::Square((P.X - 2400.f) / 5900.f) + FMath::Square((P.Y + 13300.f) / 4100.f) < 1.f) return false;
        if (TreeHidden(FVector(P, -60.f))) return false;
        for (const FVector2D& Q : Out) if (FVector2D::Distance(P, Q) < 1100.f) return false;
        return true;
    };
    // Along the walks: about every 1900, just off the walk's left edge.
    for (const HouseCampusLayoutImpl::FRun& W : HouseCampusLayoutImpl::Walks())
    {
        const FVector2D D = W.B - W.A;
        const float L = D.Size();
        if (L < 900.f) continue;
        const FVector2D Dir = D / L, Left(-Dir.Y, Dir.X);
        for (float T = 500.f; T < L - 300.f; T += 1900.f)
        {
            const FVector2D P = W.A + Dir * T + Left * (W.W * .5f + 80.f);
            if (Clear(P)) Out.Add(P);
        }
    }
    // Calder Dr's east verge and the bus bay's sidewalk edge.
    for (float Y = -6900.f; Y > HouseCampusLayoutImpl::DriveSouthY; Y -= 2400.f) { const FVector2D P(10640.f, Y); if (Clear(P)) Out.Add(P); }
    for (float X = 0.f; X < 7600.f; X += 2400.f) { const FVector2D P(X, -17530.f); if (Clear(P)) Out.Add(P); }
    // On the Calder terrain a lamp within 60 cm of a declared wall's line where it steps (the street row on the front lawn's
    // wall, the Physical Plant bench's wall, the ALU south court's) moves off it, 75 cm from the line onto the wall's high side
    // (behind its 40 cm body and coping), or else onto its low side; one that finds no clear ground either way, or stands on a
    // Calder stair or ramp, goes. Nothing changes off the relief.
    if (HouseCalderFeatures::Active())
    {
        auto Open = [](const FVector2D& P)
        {
            FVector2D High; float Side = 0.f;
            if (HouseCalderTerrain::InFrozen(P.X, P.Y) || HouseCalderFeatures::InStair(P, 40.f) || HouseCalderFeatures::InRamp(P, 40.f)) return false;
            if (HouseCampusLayoutImpl::NearStepWall(P, 60.f, High, Side)) return false;
            for (const FBlock& B : Blocks()) if (B.Rect().ExpandBy(60.f).IsInside(P)) return false;
            return !(HouseCampusLayoutImpl::RoadAt(P, 120.f) || HouseCampusLayoutImpl::SidewalkAt(P, 40.f) || HouseCampusLayoutImpl::WalkAt(P, 30.f));
        };
        TArray<FVector2D> Kept;
        for (const FVector2D& P : Out)
        {
            FVector2D High; float Side = 0.f;
            if (!HouseCampusLayoutImpl::NearStepWall(P, 60.f, High, Side))
            {
                if (!HouseCalderFeatures::InStair(P, 40.f) && !HouseCalderFeatures::InRamp(P, 40.f)) Kept.Add(P);
                continue;
            }
            const FVector2D OnLine = P - High * Side;
            if (Open(OnLine + High * 75.f)) Kept.Add(OnLine + High * 75.f);
            else if (Open(OnLine - High * 75.f)) Kept.Add(OnLine - High * 75.f);
        }
        Out = MoveTemp(Kept);
    }
}

void HouseCampusLayout::BenchPoints(TArray<FTransform>& Out)
{
    // A bench seats along its local X with its back on local +Y, so the sitter faces local -Y. It stands on the lower of the
    // ground under its two cast-iron ends (85 cm either side of its middle along the seat).
    auto Bench = [&Out](float Yaw, float X, float Y)
    {
        // (On the Calder terrain no bench stands on a stair or ramp the landform adds: the two by the knoll's ramps go.)
        if (HouseCalderFeatures::InRamp(FVector2D(X, Y), 60.f) || HouseCalderFeatures::InStair(FVector2D(X, Y), 60.f)) return;
        // (And one whose seat or back would reach within 50 cm of a declared wall's line where it steps - the quad's
        // north-west bench, backed onto the MAIN S court wall's 2.7 m drop - slides forward, the way its sitter faces, until
        // it is clear: at most 1.5 m.)
        if (HouseCalderFeatures::Active())
        {
            const FRotator R(0.f, Yaw, 0.f);
            const FVector2D Fwd(R.RotateVector(FVector(0.f, -1.f, 0.f)));
            auto OnWall = [&R](float BX, float BY)
            {
                FVector2D High; float Side = 0.f;
                for (const float LX : { -110.f, -55.f, 0.f, 55.f, 110.f })
                    for (const float LY : { -28.f, 3.f, 34.f })
                    {
                        const FVector L = R.RotateVector(FVector(LX, LY, 0.f));
                        if (HouseCampusLayoutImpl::NearStepWall(FVector2D(BX + L.X, BY + L.Y), 50.f, High, Side)) return true;
                    }
                return false;
            };
            for (int32 K = 0; K < 15 && OnWall(X, Y); ++K) { X += float(Fwd.X) * 10.f; Y += float(Fwd.Y) * 10.f; }
        }
        const FVector End = FRotator(0.f, Yaw, 0.f).RotateVector(FVector(85.f, 0.f, 0.f));
        const float Z = FMath::Min(HouseExteriorSite::GroundHeight(X - End.X, Y - End.Y), HouseExteriorSite::GroundHeight(X + End.X, Y + End.Y));
        Out.Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(X, Y, Z)));
    };
    for (const float X : { -12100.f, -10100.f })
    {
        Bench(0.f, X, -10390.f);
        Bench(180.f, X, -11210.f);
    }
    Bench(90.f, -15050.f, -10100.f);
    // (On the Calder terrain the forecourt's fountain moves west off the Senior Steps' railed ramp, and this bench with it.)
    // (72 cm off the fountain's rim on both, as today: at 22 cm a walker stepped from the seat onto the 46 cm rim and into the
    // basin, and could not step out again.)
    Bench(90.f, HouseCalderFeatures::Active() ? -15450.f : -15050.f, -11500.f);
    for (const float X : { -12000.f, -9600.f, 600.f, 5600.f, 13500.f, 15600.f }) Bench(180.f, X, -6820.f);
}

bool HouseCampusLayout::TreeHidden(const FVector& At, float TrunkReach)
{
    // The Calder landform hides the site trees that would stand on its walls, stairs, ramps, water, reserves or lot tiers.
    if (HouseCalderTerrain::Enabled() && HouseCalderLandform::TreeHidden(FVector2D(At.X, At.Y))) return true;
    if (!Enabled() || At.Y > JunctionSouthY) return false;
    const FVector2D P(At.X, At.Y);
    const float Building = FMath::Max(60.f, TrunkReach), Lane = FMath::Max(40.f, TrunkReach), Walk = FMath::Max(20.f, TrunkReach);
    for (const FBlock& B : Blocks()) if (B.Rect().ExpandBy(Building).IsInside(P)) return true;
    if (HouseCampusLayoutImpl::RoadAt(P, Lane) && !HouseCampusLayoutImpl::InIsland(P, -Lane)) return true;
    return HouseCampusLayoutImpl::SidewalkAt(P, Walk) || HouseCampusLayoutImpl::WalkAt(P, Walk);
}
float HouseCampusLayout::ApronDepth(int32 Seed, TCHAR Face, float Width, float MaxDepth)
{
    namespace CL = HouseCampusLayoutImpl;
    if (!Enabled() || !Blocks().IsValidIndex(Seed)) return MaxDepth;
    const FBlock& B = Blocks()[Seed];
    const FVector N3 = FaceNormal(Face);
    const FVector2D N(N3.X, N3.Y), Along(-N.Y, N.X);
    const FVector2D Door = B.Centre() + FVector2D(N.X * (B.X1 - B.X0) * .5f, N.Y * (B.Y1 - B.Y0) * .5f);
    auto Blocked = [&](const FVector2D& P)
    {
        if (CL::RoadAt(P) || CL::WedgeAt(P) || CL::StreetAt(P) || CL::SidewalkAt(P)) return true;
        for (const CL::FPlaza& Q : CL::PlazaDiscs()) if (FVector2D::Distance(P, Q.C) < Q.R) return true;
        for (const CL::FPlazaRect& Q : CL::PlazaRects()) if (Q.R.IsInside(P)) return true;   // homecoming's own paving too
        for (const CL::FRun& W : CL::Walks())   // a walk to this door may cross its apron; any other may not
            if (CL::RunBuilt(W) && CL::InRun(W, P, 0.f) && FVector2D::Distance(W.A, Door) > 300.f && FVector2D::Distance(W.B, Door) > 300.f) return true;
        for (const FBlock& O : Blocks()) if (&O != &B && O.Rect().IsInside(P)) return true;
        return false;
    };
    auto Layer = [&](float S)
    {
        for (float T = -Width * .5f + 5.f; T <= Width * .5f - 5.f + 1.f; T += 25.f) if (Blocked(Door + N * S + Along * T)) return true;
        return Blocked(Door + N * S + Along * (Width * .5f - 5.f));
    };
    // The deepest clear depth, found every 25 cm and then refined to 2 cm.
    float Clear = 0.f;
    for (float S = 25.f; S < MaxDepth + 25.f; S += 25.f)
    {
        const float Probe = FMath::Min(S, MaxDepth);
        if (!Layer(Probe - 1.f)) { Clear = Probe; continue; }
        for (float F = Clear + 2.f; F <= Probe; F += 2.f) { if (Layer(F - 1.f)) break; Clear = F; }
        break;
    }
    // On the Calder terrain the landform levels each apron, clipped where a stair needs the room in front of the door
    // (APRON_CLIP): the apron is built no deeper than that, so it never stands over the stair.
    float Baked = 0.f;
    if (Clear >= 100.f && HouseCalderTerrain::Enabled() && HouseCalderLandform::ApronDepth(B.Id, Face, Baked)) Clear = FMath::Min(Clear, Baked);
    return Clear < 100.f ? 0.f : Clear;
}
void HouseCampusLayout::EntranceAprons(TArray<FApron>& Out)
{
    if (!Enabled()) return;
    for (int32 I = 0; I < Blocks().Num(); ++I)
    {
        const FBlock& B = Blocks()[I];
        for (int32 K = 0; B.Faces[K]; ++K)
        {
            if (K > 0 && HouseCampusInteriors::RealDoorOn(I, B.Faces[K])) continue;   // a real side door has its own small stoop
            const float W = K == 0 ? MainApronW : SideApronW, D = ApronDepth(I, B.Faces[K], W, K == 0 ? MainApronD : SideApronD);
            if (D <= 0.f) continue;
            const FVector N3 = FaceNormal(B.Faces[K]);
            const FVector2D N(N3.X, N3.Y), Along(-N.Y, N.X);
            const FVector2D Door = B.Centre() + FVector2D(N.X * (B.X1 - B.X0) * .5f, N.Y * (B.Y1 - B.Y0) * .5f);
            const FVector2D P0 = Door - Along * (W * .5f), P1 = Door + Along * (W * .5f) + N * D;
            Out.Add({ FBox2D(FVector2D(FMath::Min(P0.X, P1.X), FMath::Min(P0.Y, P1.Y)), FVector2D(FMath::Max(P0.X, P1.X), FMath::Max(P0.Y, P1.Y))), ApronTop + BlockDZ(I) });
        }
    }
}
bool HouseCampusLayout::FootprintClash(const FBox2D& Box, FString& What)
{
    namespace CL = HouseCampusLayoutImpl;
    if (!Enabled()) return false;
    for (const FBlock& B : Blocks()) if (B.Rect().Intersect(Box)) { What = FString::Printf(TEXT("building %s"), B.Id); return true; }
    const FVector2D C = Box.GetCenter(), H = Box.GetExtent();
    for (float SX : { -1.f, 0.f, 1.f })
        for (float SY : { -1.f, 0.f, 1.f })
        {
            const FVector2D P = C + FVector2D(SX * H.X, SY * H.Y);
            if (HouseCalderRoads::Contains(P)) { What = TEXT("a Calder traffic lane"); return true; }
            if (CL::RoadAt(P) && !CL::InIsland(P, 0.f)) { What = TEXT("a traffic lane"); return true; }
            if (CL::SidewalkAt(P) || CL::WalkAt(P, 0.f)) { What = TEXT("a walk"); return true; }
        }
    return false;
}

bool HouseCampusLayout::PaveZ(const FVector& At, float& OutZ)
{
    if (!Enabled()) return false;
    const FVector2D P(At.X, At.Y);
    if (HouseCalderRoads::FloorAt(P, OutZ)) return true;
    if (P.Y > JunctionSouthY) return false;
    if (HouseCampusLayoutImpl::RoadAt(P) && !HouseCampusLayoutImpl::InIsland(P, 0.f)) { OutZ = HouseCampusLayoutImpl::RoadZ(P); return true; }
    return false;
}

void HouseCampusLayout::RemapLotCars(TArray<FTransform>& A, TArray<FTransform>& B, TArray<FTransform>& C)
{
    if (!Enabled()) return;
    // The two old health-lot rows (X -8550..-250 at Y -8250 and -6150) keep their random draws; their cars move to new stalls.
    TArray<FTransform*> Moving;
    for (TArray<FTransform>* Arr : { &A, &B, &C })
        for (FTransform& T : *Arr)
        {
            const FVector P = T.GetLocation();
            if (P.X > -9000.f && P.X < 0.f && P.Y > -8600.f && P.Y < -5800.f) Moving.Add(&T);
        }
    Moving.Sort([](const FTransform& L, const FTransform& R) { const FVector PL = L.GetLocation(), PR = R.GetLocation(); return PL.Y != PR.Y ? PL.Y > PR.Y : PL.X < PR.X; });
    TArray<FTransform> Stalls;
    HouseCampusLayoutImpl::Stalls(Stalls);
    // Visitor lot first, then the South Lot, spread out rather than packed.
    TArray<FTransform> Visitor, South;
    for (const FTransform& S : Stalls) (S.GetLocation().X < -4000.f && S.GetLocation().Y > -9000.f ? Visitor : South).Add(S);
    TArray<FTransform> Order;
    for (int32 I = 0; I < Visitor.Num(); I += 2) Order.Add(Visitor[I]);
    for (int32 I = 3; I < South.Num(); I += 5) Order.Add(South[I]);
    for (int32 K = 0; K < Moving.Num() && K < Order.Num(); ++K)
    {
        const float Jitter = Moving[K]->Rotator().Yaw - 90.f * FMath::RoundToFloat(Moving[K]->Rotator().Yaw / 90.f);
        FTransform T = Order[K];
        T.SetRotation(FRotator(0.f, T.Rotator().Yaw + Jitter, 0.f).Quaternion());
        T.SetLocation(FVector(T.GetLocation().X, T.GetLocation().Y, Moving[K]->GetLocation().Z));
        T.SetScale3D(Moving[K]->GetScale3D());
        *Moving[K] = T;
    }
}

void HouseCampusLayout::LotPoles(TArray<FVector>& Out)
{
    Out.Add(FVector(-6460.f, -7600.f, 0.f));      // visitor lot, on the lawn at its west edge, lens toward the stalls
    Out.Add(FVector(3034.f, -21372.f, 0.f));      // South Lot, in two of the tree islands
    Out.Add(FVector(5559.f, -24605.f, 180.f));
}

// ------------------------------------------------------------------ review checks
namespace HouseCampusLayoutImpl
{
/** The service roads people may walk along (they have no sidewalks): the west road, the south lane, the lot's back drive,
 *  the plant lane, the overflow lot's apron, and the bend. */
bool ServiceRoadAt(const FVector2D& P)
{
    const TArray<FBox2D>& R = RoadRects();
    for (const int32 I : { 6, 7, 8, 9, 12 }) if (R[I].IsInside(P)) return true;
    return InArc(Bend, P);
}
/** Where a walk may legitimately lie on a road: crosswalks, drive crossings, and the homecoming's own paving. */
bool CrossingAt(const FVector2D& P)
{
    for (const float Y : Crosswalks) if (P.X >= DriveX0 && P.X <= DriveX1 && FMath::Abs(P.Y - Y) <= CrossHalf) return true;
    for (const FVector2D& C : StreetCrossings) if (P.X >= C.X && P.X <= C.Y && P.Y >= -5550.f && P.Y <= -4150.f) return true;
    for (const FVector2D& G : FarGaps) if (P.X >= G.X && P.X <= G.Y && P.Y >= -5800.f && P.Y <= -5550.f) return true;
    if (P.X >= VisitorX0 && P.X <= VisitorX1 && P.Y >= -6700.f && P.Y <= -6350.f) return true;   // the promenade over the visitor drive, from the lot
    if (P.X >= 10300.f && P.X <= 10550.f && P.Y >= -11000.f && P.Y <= -10450.f) return true;     // Calder Dr's east walk over Ashby Lane
    if (P.X >= 3600.f && P.X <= 4400.f && P.Y >= -20050.f && P.Y <= -19800.f) return true;       // Stadium Dr's south walk over the lot drive
    return false;
}
bool InLot(const FVector2D& P)
{
    for (const FLot& L : Lots()) if (L.Area.IsInside(P)) return true;
    return false;
}
/** Pedestrian ground for the connectivity check. */
bool PedestrianAt(const FVector2D& P)
{
    if (SidewalkAt(P) || WalkAt(P, 0.f) || CrossingAt(P) || ServiceRoadAt(P) || InLot(P)) return true;
    // Fraternity Park Dr's sidewalks (both sides) and the old side street's east walk, built by the exterior.
    if (P.X <= 8450.f && P.Y >= -5800.f && P.Y <= -5550.f)
    {
        bool bGap = false;
        for (const FVector2D& G : FarGaps) bGap |= P.X > G.X && P.X < G.Y;
        if (!bGap) return true;
    }
    if (P.X <= 8700.f && P.Y >= -4150.f && P.Y <= -3900.f) return true;
    if (P.X >= 10300.f && P.X <= 10550.f && P.Y >= -5800.f) return true;
    // While the homecoming dressing is on, its kiosk plaza and gate path carry the walk to the stadium.
    if (HouseCampusFall::Enabled() && (FVector2D::Distance(P, FVector2D(2400.f, -6230.f)) < 300.f || (P.X >= 2250.f && P.X <= 2550.f && P.Y >= -8930.f && P.Y <= -6500.f))) return true;
    return false;
}
}

bool HouseCampusLayout::OnTrafficLane(const FVector2D& P) { return Enabled() && HouseCampusLayoutImpl::RoadAt(P) && !HouseCampusLayoutImpl::InIsland(P, 0.f); }

void HouseCampusLayout::WalkSamples(TArray<FVector>& Out)
{
    namespace CL = HouseCampusLayoutImpl;
    if (!Enabled()) return;
    HouseCalderRoads::FloorSamples(Out);
    for (const CL::FRun& W : CL::Walks())
    {
        if (!CL::RunBuilt(W)) continue;
        const float L = FVector2D::Distance(W.A, W.B);
        const FVector2D U = (W.B - W.A) / FMath::Max(L, 1.f), Side(-U.Y, U.X);
        const int32 N = FMath::Max(1, FMath::CeilToInt(L / 200.f));
        for (int32 K = 0; K <= N; ++K)
        {
            const FVector2D C = FMath::Lerp(W.A, W.B, float(K) / N);
            if (CL::OnFace(C)) continue;   // a door or the tower plinth: the building's own apron is there
            for (const float S : { 0.f, -1.f, 1.f })
            {
                const FVector2D Spot = C + Side * (S * (W.W * .5f - 25.f));
                Out.Add(FVector(Spot, CL::WalkZ(Spot)));
            }
        }
        // The run-on past each free end (where there is one), so a slab end left hanging shows up.
        for (const bool bA : { true, false })
        {
            const FVector2D E = bA ? W.A : W.B, Dir = bA ? -U : U;
            const float Ex = CL::RunOn(E, Dir, W.W);
            if (Ex > 20.f) { const FVector2D Spot = E + Dir * (Ex - 10.f); Out.Add(FVector(Spot, CL::WalkZ(Spot))); }
        }
    }
    // Curb ramps and drive crossings: 3 x 3 points on each slope at its own height (CL::WedgeZ, as the slab is built; clear of
    // the cheek walls).
    for (const CL::FWedge& W : CL::Wedges())
    {
        const bool bAlongX = W.LowSide < 2;
        for (const float T : { .15f, .5f, .85f })        // 0 at the high side, 1 at the low side
            for (const float S : { -1.f, 0.f, 1.f })
            {
                const float Across = S * ((bAlongX ? W.R.GetExtent().Y : W.R.GetExtent().X) - 50.f);
                float X = 0.f, Y = 0.f;
                switch (W.LowSide)
                {
                case 0: X = FMath::Lerp(W.R.Max.X, W.R.Min.X, T); Y = W.R.GetCenter().Y + Across; break;
                case 1: X = FMath::Lerp(W.R.Min.X, W.R.Max.X, T); Y = W.R.GetCenter().Y + Across; break;
                case 2: Y = FMath::Lerp(W.R.Max.Y, W.R.Min.Y, T); X = W.R.GetCenter().X + Across; break;
                default: Y = FMath::Lerp(W.R.Min.Y, W.R.Max.Y, T); X = W.R.GetCenter().X + Across; break;
                }
                Out.Add(FVector(X, Y, CL::WedgeZ(W, X, Y, T)));
            }
    }
    // The curved sidewalks, along their middle.
    for (const CL::FArc& A : CL::SidewalkArcs())
    {
        const float R = (A.R0 + A.R1) * .5f, Len = FMath::DegreesToRadians(FMath::Abs(A.A1 - A.A0)) * R;
        const int32 N = FMath::Max(2, FMath::CeilToInt(Len / 200.f));
        for (int32 K = 0; K <= N; ++K)
        {
            const float D = FMath::DegreesToRadians(FMath::Lerp(A.A0, A.A1, FMath::Clamp(float(K) / N, .01f, .99f)));
            const FVector2D Spot = A.C + FVector2D(FMath::Cos(D), FMath::Sin(D)) * R;
            Out.Add(FVector(Spot, CL::WalkZ(Spot)));
        }
    }
    for (const FBox2D& S : CL::Sidewalks())
    {
        const bool bAlongX = S.GetSize().X > S.GetSize().Y;
        const float L = bAlongX ? S.GetSize().X : S.GetSize().Y;
        const int32 N = FMath::Max(1, FMath::CeilToInt(L / 200.f));
        for (int32 K = 0; K <= N; ++K)
        {
            const float T = FMath::Clamp(float(K) / N, .02f, .98f);
            const FVector2D Spot(bAlongX ? FMath::Lerp(S.Min.X, S.Max.X, T) : S.GetCenter().X, bAlongX ? S.GetCenter().Y : FMath::Lerp(S.Min.Y, S.Max.Y, T));
            Out.Add(FVector(Spot, CL::WalkZ(Spot)));
        }
    }
    for (const CL::FPlaza& P : CL::PlazaDiscs()) Out.Add(FVector(P.C + FVector2D(P.R * .6f, 0.f), CL::WalkTop + HouseClockTowerSite::Lift()));   // the tower plaza, on its knoll
    for (const CL::FPlazaRect& P : CL::PlazaRects()) Out.Add(FVector(P.R.GetCenter(), CL::WalkZ(P.R.GetCenter())));
    // A walk to a door crosses the entrance apron, 2 cm higher; a sample within 5 cm of an apron's edge could read either.
    TArray<FApron> Aprons;
    EntranceAprons(Aprons);
    for (int32 I = Out.Num() - 1; I >= 0; --I)
        for (const FApron& A : Aprons)
        {
            const FVector2D P(Out[I].X, Out[I].Y);
            if (A.R.ExpandBy(-5.f).IsInside(P)) { Out[I].Z = A.Top; break; }
            if (A.R.ExpandBy(5.f).IsInside(P)) { Out.RemoveAt(I); break; }
        }
}

void HouseCampusLayout::CheckPlan(FPlanReport& R)
{
    namespace CL = HouseCampusLayoutImpl;
    if (!Enabled()) return;
    auto Problem = [&R](int32& Counter, const FString& Text) { ++Counter; if (R.Problems.Num() < 60) R.Problems.Add(Text); };
    const TArray<FBlock>& Bs = Blocks();
    R.Buildings = Bs.Num();
    R.Lots = CL::Lots().Num();
    R.Roads = CL::RoadRects().Num() + 3;

    // 1. Nothing paved inside a building, the stadium bowl or the tower plinth; no two buildings overlap.
    for (int32 I = 0; I < Bs.Num(); ++I)
    {
        const FBox2D In = Bs[I].Rect().ExpandBy(-30.f);
        for (int32 J = I + 1; J < Bs.Num(); ++J) if (Bs[I].Rect().Intersect(Bs[J].Rect())) Problem(R.Overlaps, FString::Printf(TEXT("building %s overlaps %s"), Bs[I].Id, Bs[J].Id));
        int32 Hits = 0;
        FVector2D First = FVector2D::ZeroVector;
        for (float X = In.Min.X; X <= In.Max.X; X += 50.f)
            for (float Y = In.Min.Y; Y <= In.Max.Y; Y += 50.f)
            {
                const FVector2D P(X, Y);
                if (CL::RoadAt(P) || CL::SidewalkAt(P) || CL::WalkAt(P, 0.f)) { if (Hits++ == 0) First = P; }
            }
        if (Hits) Problem(R.Overlaps, FString::Printf(TEXT("building %s has %d paved samples inside, first at (%.0f,%.0f)"), Bs[I].Id, Hits, First.X, First.Y));
    }
    {
        bool bFound = false;
        for (float X = 2400.f - 5570.f; X <= 2400.f + 5570.f && !bFound; X += 50.f)
            for (float Y = -13300.f - 3770.f; Y <= -13300.f + 3770.f && !bFound; Y += 50.f)
            {
                const FVector2D P(X, Y);
                if (FMath::Square((X - 2400.f) / 5570.f) + FMath::Square((Y + 13300.f) / 3770.f) >= 1.f) continue;
                if (CL::RoadAt(P) || CL::SidewalkAt(P) || CL::WalkAt(P, 0.f)) { bFound = true; Problem(R.Overlaps, FString::Printf(TEXT("paving inside the stadium bowl at (%.0f,%.0f)"), X, Y)); }
            }
    }
    for (float X = CL::TowerPlinth.Min.X + 20.f; X < CL::TowerPlinth.Max.X; X += 50.f)
        for (float Y = CL::TowerPlinth.Min.Y + 20.f; Y < CL::TowerPlinth.Max.Y; Y += 50.f)
            if (CL::RoadAt(FVector2D(X, Y)) || CL::SidewalkAt(FVector2D(X, Y))) Problem(R.Overlaps, FString::Printf(TEXT("road or sidewalk under the clock tower at (%.0f,%.0f)"), X, Y));

    for (const CL::FBerm& Bm : CL::Berms())
        for (float X = Bm.R.Min.X; X <= Bm.R.Max.X; X += 50.f)
            for (float Y = Bm.R.Min.Y; Y <= Bm.R.Max.Y; Y += 50.f)
                if (CL::RoadAt(FVector2D(X, Y), 30.f) || CL::SidewalkAt(FVector2D(X, Y), 30.f) || CL::WalkAt(FVector2D(X, Y), 30.f)) { Problem(R.Overlaps, FString::Printf(TEXT("berm at (%.0f,%.0f) runs onto paving at (%.0f,%.0f)"), Bm.R.GetCenter().X, Bm.R.GetCenter().Y, X, Y)); X = Bm.R.Max.X + 1.f; break; }

    // 2. Every entrance within 2 m of the walk network.
    for (const FBlock& B : Bs)
        for (const TCHAR* F = B.Faces; *F; ++F)
        {
            const FVector Nrm = FaceNormal(*F);
            const FVector2D C = B.Centre(), H(B.X1 - B.X0, B.Y1 - B.Y0);
            const FVector2D Door = C + FVector2D(Nrm.X * (H.X * .5f + 60.f), Nrm.Y * (H.Y * .5f + 60.f));
            ++R.Entrances;
            bool bNear = false;
            for (float DX = -200.f; DX <= 200.f && !bNear; DX += 25.f)
                for (float DY = -200.f; DY <= 200.f && !bNear; DY += 25.f)
                    if (DX * DX + DY * DY <= 200.f * 200.f && (CL::SidewalkAt(Door + FVector2D(DX, DY)) || CL::WalkAt(Door + FVector2D(DX, DY), 0.f))) bNear = true;
            if (!bNear) Problem(R.EntrancesOff, FString::Printf(TEXT("entrance %s/%c at (%.0f,%.0f) is not within 2 m of a walk"), B.Id, *F, Door.X, Door.Y));
        }

    // 2b. The other way round: every walk that ends on a building ends at one of its entrances, never at a blank wall.
    for (const CL::FRun& W : CL::Walks())
    {
        if (!CL::RunBuilt(W)) continue;
        for (const FVector2D& E : { W.A, W.B })
            for (const FBlock& B : Bs)
            {
                if (!B.Rect().ExpandBy(5.f).IsInside(E)) continue;
                bool bDoor = false;
                for (const TCHAR* F = B.Faces; *F && !bDoor; ++F)
                {
                    const FVector Nrm = FaceNormal(*F);
                    const FVector2D Door = B.Centre() + FVector2D(Nrm.X * (B.X1 - B.X0) * .5f, Nrm.Y * (B.Y1 - B.Y0) * .5f);
                    bDoor = FVector2D::Distance(Door, E) < 200.f;
                }
                if (!bDoor) Problem(R.BlankWalls, FString::Printf(TEXT("a walk ends at a blank wall of %s at (%.0f,%.0f)"), B.Id, E.X, E.Y));
            }
    }

    // 3. Every lot reached by a road.
    for (const CL::FLot& L : CL::Lots())
    {
        bool bReached = false;
        const FBox2D Ring = L.Area.ExpandBy(10.f);
        for (float T = 0.f; T <= 1.f && !bReached; T += .005f)
            for (const FVector2D& P : { FVector2D(FMath::Lerp(Ring.Min.X, Ring.Max.X, T), Ring.Min.Y), FVector2D(FMath::Lerp(Ring.Min.X, Ring.Max.X, T), Ring.Max.Y),
                                        FVector2D(Ring.Min.X, FMath::Lerp(Ring.Min.Y, Ring.Max.Y, T)), FVector2D(Ring.Max.X, FMath::Lerp(Ring.Min.Y, Ring.Max.Y, T)) })
                for (const FBox2D& Road : CL::RoadRects()) if (Road.IsInside(P)) bReached = true;
        if (!bReached) Problem(R.LotsUnreached, FString::Printf(TEXT("lot at (%.0f,%.0f) has no road to it"), L.Area.GetCenter().X, L.Area.GetCenter().Y));
    }

    // 4. No road ends in the void: each short end must meet a road, lot or the street, or a building (a dock), unless another
    //    road joins this one within 7 m of that end (a corner).
    const TArray<FBox2D>& Roads = CL::RoadRects();
    for (int32 I = 0; I < Roads.Num(); ++I)
    {
        const FBox2D& Rd = Roads[I];
        const bool bLongX = Rd.GetSize().X >= Rd.GetSize().Y;
        for (const float S : { -1.f, 1.f })
        {
            const float EndC = bLongX ? (S < 0 ? Rd.Min.X : Rd.Max.X) : (S < 0 ? Rd.Min.Y : Rd.Max.Y);
            bool bMeets = false;
            for (float T = .05f; T < 1.f && !bMeets; T += .1f)
            {
                const FVector2D P = bLongX ? FVector2D(EndC + S * 10.f, FMath::Lerp(Rd.Min.Y, Rd.Max.Y, T)) : FVector2D(FMath::Lerp(Rd.Min.X, Rd.Max.X, T), EndC + S * 10.f);
                bMeets = CL::RoadAt(P) || CL::StreetAt(P);
            }
            const FBox2D EndZone = bLongX ? FBox2D(FVector2D(S < 0 ? Rd.Min.X : Rd.Max.X - 700.f, Rd.Min.Y - 20.f), FVector2D(S < 0 ? Rd.Min.X + 700.f : Rd.Max.X, Rd.Max.Y + 20.f))
                                          : FBox2D(FVector2D(Rd.Min.X - 20.f, S < 0 ? Rd.Min.Y : Rd.Max.Y - 700.f), FVector2D(Rd.Max.X + 20.f, S < 0 ? Rd.Min.Y + 700.f : Rd.Max.Y));
            for (int32 J = 0; J < Roads.Num() && !bMeets; ++J) if (J != I && Roads[J].Intersect(EndZone)) bMeets = true;
            for (const CL::FLot& L : CL::Lots()) if (L.Area.Intersect(EndZone)) bMeets = true;
            for (const FBlock& B : Bs) if (B.Rect().ExpandBy(300.f).Intersect(EndZone)) bMeets = true;
            if (!bMeets) Problem(R.DeadEnds, FString::Printf(TEXT("road %d ends in the open at %s=%.0f"), I, bLongX ? TEXT("X") : TEXT("Y"), EndC));
        }
    }
    for (const CL::FArc& A : { CL::Curve, CL::Bend })
        for (const float D : { A.A0, A.A1 })
        {
            const float T = FMath::DegreesToRadians(D), Away = (D == A.A0 ? -1.f : 1.f) * FMath::Sign(A.A1 - A.A0);
            const FVector2D Mid = A.C + FVector2D(FMath::Cos(T), FMath::Sin(T)) * ((A.R0 + A.R1) * .5f) + FVector2D(-FMath::Sin(T), FMath::Cos(T)) * (Away * 15.f);
            bool bMeets = false;
            for (const FBox2D& Road : Roads) bMeets |= Road.IsInside(Mid);
            if (!bMeets) Problem(R.DeadEnds, FString::Printf(TEXT("curve at (%.0f,%.0f) ends in the open at %.0f degrees"), A.C.X, A.C.Y, D));
        }

    // 5. Walks continuous (one pedestrian network, joined to Fraternity Park Dr's sidewalks), and on a road only at a crossing.
    constexpr float Cell = 50.f;
    const FBox2D Area = CL::Rect(-21500.f, 19500.f, -27000.f, -3800.f);
    const int32 NX = FMath::CeilToInt(Area.GetSize().X / Cell), NY = FMath::CeilToInt(Area.GetSize().Y / Cell);
    TArray<uint8> Ped;
    Ped.SetNumZeroed(NX * NY);
    for (int32 J = 0; J < NY; ++J)
        for (int32 I = 0; I < NX; ++I)
        {
            const FVector2D P(Area.Min.X + (I + .5f) * Cell, Area.Min.Y + (J + .5f) * Cell);
            if (!CL::PedestrianAt(P)) continue;
            Ped[J * NX + I] = 1;
            ++R.PedestrianCells;
            const bool bWalk = CL::WalkAt(P, 0.f) || CL::SidewalkAt(P);
            const bool bLane = (CL::RoadAt(P) && !CL::ServiceRoadAt(P) && !CL::InIsland(P, 0.f) && !CL::InLot(P)) || (P.X <= CL::DriveX0 && P.Y > -5550.f && P.Y < -4150.f);
            if (bWalk && bLane && !CL::CrossingAt(P)) Problem(R.WalkOnLane, FString::Printf(TEXT("walk on a traffic lane without a crossing at (%.0f,%.0f)"), P.X, P.Y));
        }
    TArray<int32> Stack;
    for (int32 Seed = 0; Seed < Ped.Num(); ++Seed)
    {
        if (Ped[Seed] != 1) continue;
        ++R.Components;
        int32 Size = 0;
        Stack.Reset();
        Stack.Add(Seed);
        Ped[Seed] = 2;
        while (Stack.Num())
        {
            const int32 U = Stack.Pop(EAllowShrinking::No);
            ++Size;
            const int32 UI = U % NX, UJ = U / NX;
            for (int32 DJ = -1; DJ <= 1; ++DJ)
                for (int32 DI = -1; DI <= 1; ++DI)
                {
                    const int32 VI = UI + DI, VJ = UJ + DJ;
                    if (VI < 0 || VJ < 0 || VI >= NX || VJ >= NY) continue;
                    const int32 V = VJ * NX + VI;
                    if (Ped[V] == 1) { Ped[V] = 2; Stack.Add(V); }
                }
        }
        if (R.Components > 1) R.Problems.Add(FString::Printf(TEXT("pedestrian island of %d cells starting at (%.0f,%.0f)"), Size, Area.Min.X + (Seed % NX + .5f) * Cell, Area.Min.Y + (Seed / NX + .5f) * Cell));
    }

    // 6. No lamp on a traffic lane or in a building (poles and trees are checked in the world by the review).
    TArray<FVector2D> Lamps;
    LampPoints(Lamps);
    for (const FVector2D& L : Lamps)
    {
        bool bBad = OnTrafficLane(L);
        for (const FBlock& B : Bs) bBad |= B.Rect().IsInside(L);
        if (bBad) Problem(R.PropsOnLane, FString::Printf(TEXT("lamp at (%.0f,%.0f) stands in a building or on a lane"), L.X, L.Y));
    }
}

// ------------------------------------------------------------------ the Calder terrain (pads, frames, paving)
float HouseCampusLayout::BlockDZ(int32 Seed) { return Blocks().IsValidIndex(Seed) ? HouseCalderTerrain::PadDZ(Seed) : 0.f; }
FTransform HouseCampusLayout::Frame(int32 Seed)
{
    if (!Blocks().IsValidIndex(Seed)) return FTransform::Identity;
    const FBlock& B = Blocks()[Seed];
    return FTransform(FRotator(0.f, FaceYaw(B.Faces[0]), 0.f), FVector(B.Centre(), BlockDZ(Seed)));
}
bool HouseCampusLayout::PavingNear(const FVector2D& P, float Pad)
{
    if (!Enabled()) return false;
    if (HouseCalderRoads::Contains(P,Pad)) return true;
    if (HouseCampusLayoutImpl::RoadAt(P, Pad) || HouseCampusLayoutImpl::SidewalkAt(P, Pad) || HouseCampusLayoutImpl::WalkAt(P, Pad)) return true;
    static const TArray<FApron> Aprons = [] { TArray<FApron> A; EntranceAprons(A); return A; }();
    for (const FApron& A : Aprons) if (A.R.ExpandBy(Pad).IsInside(P)) return true;
    return false;
}
HouseCalderTerrain::EPave HouseCampusLayout::PaveKindAt(const FVector2D& P)
{
    using HouseCalderTerrain::EPave;
    if (!Enabled()) return EPave::Lawn;
    if (HouseCalderRoads::Contains(P)) return EPave::Road;
    if (HouseCampusLayoutImpl::RoadAt(P) && !HouseCampusLayoutImpl::InIsland(P, 0.f)) return EPave::Road;
    static const TArray<FApron> Aprons = [] { TArray<FApron> A; EntranceAprons(A); return A; }();
    for (const FApron& A : Aprons) if (A.R.IsInside(P)) return EPave::Apron;
    if (HouseCampusLayoutImpl::SidewalkAt(P) || HouseCampusLayoutImpl::WalkAt(P, 0.f)) return EPave::Walk;
    return EPave::Lawn;
}
