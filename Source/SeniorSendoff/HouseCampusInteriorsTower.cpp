// The walk-in clock tower (master job, Tier A, a free-standing interior like the stadium): the Calder Tower's shaft
// (HouseClockTower; its shell is HouseClockTowerMesh's, opened up while this is on) entered by its oak door and climbed by
// an open-well oak stair to the belfry. World space (Frame is identity, Height 0); heights below are above the tower's
// foot (TwrBase: campus grade TwrG on Tower Knoll, HouseClockTowerSite::Lift), plans about the tower's axis (TwrAt).
//
// - The ground floor (rel 75.5, flagstones over the plinth's lit top): Buildings & Grounds' store (the bench with the
//   clock's logbook and a wire rack under the second flight, a shelf of ledgers in the north-east corner, cartons of
//   homecoming bunting and storage boxes in the well's west half, rock salt and a shovel for the steps), the cupboard under
//   the first flight, the clock's weight chute in a corner of the well, the notice board by the door and the wall clock
//   over the first flight.
// - The stair: 11 flights of 13 risers (18.95 cm) and 12 goings of 27.5 round a 330 cm open well, along the walls -Y, -X,
//   +Y, +X in turn, 95 x 95 landings in the corners, closed strings, balusters and a handrail on the well side, a wall
//   rail. The top flight runs along the +Y wall up through a slot in the belfry's floor; over the last landing the course
//   under the belfry is thinned for headroom (HouseClockTowerMesh, E8).
// - The belfry is the tower's own (Upper, no collision). Its collision is here, never drawn (Blockers): the floor, the
//   four walls up to the clock stage (no way out through the arches or louvres), the frame's posts, the tie beams to
//   crouch under, the bells, a guard round the slot (the tower draws only the slot's trap lid, open against the +Y wall:
//   a drawn rail would show between the louvre slats in the Belfry review view). The well's guards have jump-proof planes
//   behind them, and every lancet has a plug behind its glass.
// - TwrInside is the shaft only: in the belfry the bells ring at full strength (the point of the climb), the street's
//   lights reach the player and the pool stays off.
// Lit by the shared pool (anchors over the landings) and caged bulbs (emission). Built at runtime; never saved.
#include "HouseCampusInteriorsRooms.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusFallKit.h"
#include "HouseClockTowerMesh.h"
#include "Engine/Texture2D.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace HouseCampusInteriorsImpl
{
// The tower's axis and grade (HouseClockTowerSite).
constexpr float TwrX = HouseClockTowerSite::CenterX, TwrY = HouseClockTowerSite::CenterY, TwrG = HouseClockTowerSite::GroundZ;
/** The tower's foot in the world: its grade raised onto Tower Knoll by the lift the shell has (HouseClockTowerSite::Center).
 *  Every world height below starts here, once (the interior's Frame is identity, so nothing else lifts it). */
float TwrBase() { return TwrG + HouseClockTowerSite::Lift(); }
// The flagstone floor (over the plinth's top at 75), the belfry floor (Upper's top; the blockers' slab), the underside of
// the course under the belfry (the shaft's ceiling) and of its thinned part over the last landing.
constexpr float TwrFloor = 75.5f, TwrBelfry = 2786.f, TwrCeil = 2740.f, TwrRecessTop = 2768.f;
// The lining's inner face and the open well's half width: flights and landings fill Dist 165..258 along the walls; the
// flights' walking line is half way.
constexpr float TwrIn = 258.f, TwrWell = 165.f, TwrMid = (TwrWell + TwrIn) * .5f;
// Eleven flights of thirteen risers and twelve goings, each 330 cm from one well corner to the next.
constexpr int32 TwrFlights = 11, TwrSteps = 13;
constexpr float TwrGoing = 27.5f, TwrRun = TwrGoing * (TwrSteps - 1), TwrRise = (TwrBelfry - TwrFloor) / (TwrFlights * TwrSteps);
// The slot the top flight climbs through (X -165..165 from Y 165 to the +Y wall), the thinned course west of it (over the
// last landing and the head of the flight below, from Y 110), and where the (undrawn) guard along the slot stops (the stair
// head: the top two steps are within two risers of the floor). These match HouseClockTowerMesh's cuts (E8, E9).
constexpr float TwrSlotX0 = -165.f, TwrSlotX1 = 165.f, TwrSlotY0 = 165.f, TwrRecessY0 = 110.f, TwrRailEnd = 110.f;

FVector TwrAt(float X, float Y, float H) { return FVector(TwrX + X, TwrY + Y, TwrBase() + H); }
FVector2D TwrPlan(float X, float Y) { return FVector2D(TwrX + X, TwrY + Y); }
/** The middle of landing K (0: the ground floor at the foot of the first flight; 11: the belfry at the head of the last),
 *  about the axis: the corners (+,-), (-,-), (-,+), (+,+) in turn. */
FVector2D TwrCorner(int32 K)
{
    const int32 C = K % 4;
    return FVector2D(C == 0 || C == 3 ? TwrMid : -TwrMid, C < 2 ? -TwrMid : TwrMid);
}
float TwrLandingH(int32 K) { return TwrFloor + TwrRise * TwrSteps * K; }

/** Flight K (1..11), from landing K-1 to landing K along one wall: Along its climb, Out toward its wall, Foot the height it
 *  starts from. U runs along it from the well's edge at its foot (0) to the one at its head (TwrRun), D out from the axis. */
struct FTwrFlight
{
    FVector Along, Out;
    float Foot = 0.f;
    explicit FTwrFlight(int32 K)
    {
        const FVector2D From = TwrCorner(K - 1), To = TwrCorner(K);
        Along = FVector((To - From).GetSafeNormal(), 0.f);
        Out = FMath::Abs(Along.X) > .5f ? FVector(0.f, FMath::Sign(From.Y), 0.f) : FVector(FMath::Sign(From.X), 0.f, 0.f);
        Foot = TwrLandingH(K - 1);
    }
    FVector P(float U, float D, float H) const { return TwrAt(0.f, 0.f, H) + Along * (U - TwrWell) + Out * D; }
    /** The line through the nosings (tread S's is at U = Going (S - 1) - 2.5, Foot + Rise S). */
    float Nosing(float U) const { return Foot + TwrRise * ((U + 2.5f) / TwrGoing + 1.f); }
    /** A box U0..U1 along, D0..D1 out, Z0..Z1 above grade. */
    void Slab(FHallMesh& M, FName Key, float U0, float U1, float D0, float D1, float Z0, float Z1) const { Box(M, Key, P(U0, D0, Z0), P(U1, D1, Z1)); }
    /** A board in the flight's plane, D0..D0 + Depth out; Outline is (U, height) and convex. */
    void Board(FHallMesh& M, FName Key, const TArray<FVector2D>& Outline, float D0, float Depth) const
    {
        M.Prism(Key, Outline, P(0.f, D0, 0.f), Along, FVector::UpVector, Out, Depth, UVPlain());
    }
    /** A band along the flight between Below and Above the nosings (D0..D0 + Depth out), cut off at the level Limit (the top
     *  flight's under the ceiling: there its foot rises past Limit too, so the whole parallelogram is clipped, not only its
     *  top; a convex outline stays convex). */
    void Band(FHallMesh& M, FName Key, float D0, float Depth, float Below, float Above, float Limit) const
    {
        const FVector2D Corners[] = { FVector2D(0.f, Nosing(0.f) - Below), FVector2D(TwrRun, Nosing(TwrRun) - Below), FVector2D(TwrRun, Nosing(TwrRun) + Above),
            FVector2D(0.f, Nosing(0.f) + Above) };
        TArray<FVector2D> Outline;
        for (int32 I = 0; I < 4; ++I)
        {
            const FVector2D& A = Corners[I];
            const FVector2D& B = Corners[(I + 1) % 4];
            if (A.Y <= Limit) Outline.Add(A);
            if ((A.Y <= Limit) != (B.Y <= Limit)) Outline.Add(FVector2D(A.X + (B.X - A.X) * (Limit - A.Y) / (B.Y - A.Y), Limit));
        }
        if (Outline.Num() >= 3) Board(M, Key, Outline, D0, Depth);
    }
};

/** K's half size at campus scale, front to back (bDepth) or side to side: to stand it against a wall, or put things on it. */
float TwrKitHalf(const FKitter& Kit, HouseCampusInteriors::EKit K, bool bDepth)
{
    const FBox B = Kit.Sub ? Kit.Sub->KitBounds(K) : FBox(ForceInit);
    if (!B.IsValid) return 0.f;
    const FVector Front = FRotator(0.f, -KitDef(K).FrontYaw, 0.f).RotateVector(FVector::ForwardVector);
    const FVector Axis = bDepth ? Front : FVector(-Front.Y, Front.X, 0.f);
    const FVector E = B.GetExtent();
    return float(FMath::Abs(Axis.X) * E.X + FMath::Abs(Axis.Y) * E.Y);
}

// ------------------------------------------------------------------ the shaft
/** The lining (the tower's brick, sooty, Dist 258..260; the doorway and the lancets through it where the shell has them),
 *  the lancets' glass and reveals and the door's oak tympanum seen from inside (the shell's fills face out only):
 *  HouseClockTowerMesh::ShaftLining. The flagstones over the plinth's lit top; a boarded ceiling under the course below the
 *  belfry (its stone is the outside's, unlit by the pool), round the stair's slot and the recess over the last landing;
 *  fascias on the course's cut faces. */
void TwrShell(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow)
{
    HouseClockTowerMesh::ShaftLining(Solid, Glow, TEXT("CI_TwrBrick"), TEXT("CI_TwrStone"), TEXT("CI_TwrPane"), TEXT("CI_TwrPaneLit"), TEXT("CT_Oak"));
    Box(Solid, TEXT("CI_TwrFlags"), TwrAt(-TwrIn, -TwrIn, 75.f), TwrAt(TwrIn, TwrIn, TwrFloor), FHallUV::World(180.f));
    const FHallUV Boards = FHallUV::World(120.f);
    auto Ceiling = [&Clutter, &Boards](const FBox2D& R, float H)
    {
        const float X0 = float(R.Min.X), X1 = float(R.Max.X), Y0 = float(R.Min.Y), Y1 = float(R.Max.Y);
        Clutter.Quad(TEXT("CI_TwrBoards"), TwrAt(X0, Y0, H), TwrAt(X1, Y0, H), TwrAt(X1, Y1, H), TwrAt(X0, Y1, H), -FVector::UpVector, Boards);
    };
    const FBox2D Slot = Rect(TwrSlotX0, TwrSlotX1, TwrSlotY0, TwrIn), Recess = Rect(-TwrIn, TwrSlotX0, TwrRecessY0, TwrIn);
    SubtractPlan(Rect(-TwrIn, TwrIn, -TwrIn, TwrIn), { Slot, Recess }, [&Ceiling](const FBox2D& R) { Ceiling(R, TwrCeil - .5f); });
    Ceiling(Recess, TwrRecessTop - .5f);
    Box(Clutter, TEXT("CI_OakDark"), TwrAt(TwrSlotX0, TwrSlotY0, TwrCeil - 2.f), TwrAt(TwrSlotX1, TwrSlotY0 + 1.5f, TwrBelfry));               // the slot's south side
    Box(Clutter, TEXT("CI_OakDark"), TwrAt(TwrSlotX1 - 1.5f, TwrSlotY0, TwrCeil - 2.f), TwrAt(TwrSlotX1, TwrIn, TwrBelfry - TwrRise));        // under the top step
    Box(Clutter, TEXT("CI_OakDark"), TwrAt(TwrSlotX0, TwrSlotY0, TwrRecessTop - 2.f), TwrAt(TwrSlotX0 + 1.5f, TwrIn, TwrBelfry));             // the lip at the slot's west end
    Box(Clutter, TEXT("CI_OakDark"), TwrAt(-TwrIn, TwrRecessY0, TwrCeil - 2.f), TwrAt(TwrSlotX0, TwrRecessY0 + 1.5f, TwrRecessTop));          // the recess's south side
    Box(Clutter, TEXT("CI_OakDark"), TwrAt(TwrSlotX0 - 1.5f, TwrRecessY0, TwrCeil - 2.f), TwrAt(TwrSlotX0, TwrSlotY0, TwrRecessTop));        // and its east side
}

/** Flight K: twelve oak treads on their risers between the wall string and the well string, the well's balusters and its
 *  handrail into the newels, a wall rail on brass brackets. The top flight climbs into the belfry's slot, where its well
 *  side is the course's stone above the ceiling: its well string, balusters and handrail stop at the ceiling, its wall
 *  string at the belfry floor; its last riser is its own (the others' is their landing's face). */
void TwrFlight(FHallMesh& Solid, FHallMesh& Clutter, int32 K)
{
    const FTwrFlight F(K);
    const bool bTop = K == TwrFlights;
    const float Slope = TwrRise / TwrGoing, Cap = bTop ? TwrCeil : 1e5f;
    for (int32 S = 1; S < TwrSteps; ++S)
    {
        const float U0 = TwrGoing * (S - 1), Top = F.Foot + TwrRise * S;
        F.Slab(Solid, TEXT("CI_OakDark"), U0, U0 + 2.f, TwrWell, TwrIn, Top - TwrRise, Top - 4.f);                 // riser S, on the step below
        F.Slab(Solid, TEXT("CI_Oak"), U0 - 2.5f, U0 + TwrGoing + 2.f, TwrWell, TwrIn, Top - 4.f, Top);             // tread S (its nosing 2.5 over)
    }
    if (bTop) F.Slab(Solid, TEXT("CI_OakDark"), TwrRun - 2.f, TwrRun, TwrWell, TwrIn, TwrBelfry - TwrRise, TwrBelfry);
    F.Band(Solid, TEXT("CI_OakDark"), TwrIn - 3.f, 3.f, 30.f, 8.f, bTop ? TwrBelfry : 1e5f);                       // the wall string
    F.Band(Solid, TEXT("CI_OakDark"), TwrWell - 5.f, 6.f, 26.f, 12.f, Cap);                                         // the well string
    for (int32 S = 1; S < TwrSteps; ++S)
        for (const float Off : { 7.f, 20.75f })
        {
            const float U = TwrGoing * (S - 1) + Off, Z0 = F.Nosing(U) + 12.f, Z1 = FMath::Min(F.Nosing(U) + 93.f, Cap);
            if (Z1 > Z0 + 4.f) Clutter.Cylinder(TEXT("CI_OakDark"), F.P(U, TwrWell - 2.f, Z0), F.P(U, TwrWell - 2.f, Z1), 1.7f, 8, UVPlain());
        }
    const float R0 = F.Nosing(0.f) + 96.f, UEnd = FMath::Min(TwrRun, (Cap - 3.f - R0) / Slope);
    HouseCampusFallKit::Bar(Clutter, TEXT("CI_Oak"), F.P(0.f, TwrWell - 2.f, R0), F.P(UEnd, TwrWell - 2.f, R0 + Slope * UEnd), 6.f, 5.f);
    if (!bTop)
    {
        const float W0 = 12.f, W1 = TwrRun - 12.f;
        Clutter.Cylinder(TEXT("CI_Oak"), F.P(W0, TwrIn - 7.f, F.Nosing(W0) + 90.f), F.P(W1, TwrIn - 7.f, F.Nosing(W1) + 90.f), 2.4f, 10, UVPlain());
        for (const float U : { 40.f, TwrRun * .5f, TwrRun - 40.f })
            HouseCampusFallKit::Bar(Clutter, TEXT("CI_Brass"), F.P(U, TwrIn - 7.f, F.Nosing(U) + 87.6f), F.P(U, TwrIn - .2f, F.Nosing(U) + 80.f), 1.4f, 1.4f, F.Along);
    }
}

/** Landing K (1..10) at the head of flight K, in its corner: an oak deck on a bearer, its nosing over the flight below. */
void TwrLanding(FHallMesh& Solid, int32 K)
{
    const FVector2D C = TwrCorner(K);
    const FVector Nose = -FTwrFlight(K).Along * 2.5f;
    const float Z = TwrLandingH(K);
    const float X0 = C.X > 0.f ? TwrWell : -TwrIn, X1 = C.X > 0.f ? TwrIn : -TwrWell, Y0 = C.Y > 0.f ? TwrWell : -TwrIn, Y1 = C.Y > 0.f ? TwrIn : -TwrWell;
    const float NX = float(Nose.X), NY = float(Nose.Y);
    Box(Solid, TEXT("CI_Oak"), TwrAt(X0 + FMath::Min(0.f, NX), Y0 + FMath::Min(0.f, NY), Z - 4.f), TwrAt(X1 + FMath::Max(0.f, NX), Y1 + FMath::Max(0.f, NY), Z));
    Box(Solid, TEXT("CI_OakDark"), TwrAt(X0, Y0, Z - 24.f), TwrAt(X1, Y1, Z - 4.f));
}

/** The newels at the well's corners, where each flight's string and handrail meet the next's: the foot of the first flight
 *  and every landing's inner corner, a drop under the landing (the first landing's runs down to the floor: it closes the
 *  cupboard under the first flight). */
void TwrNewels(FHallMesh& Clutter)
{
    for (int32 K = 0; K < TwrFlights; ++K)
    {
        const FVector2D C = TwrCorner(K);
        const float NX = (C.X > 0.f ? 1.f : -1.f) * (TwrWell - 1.5f), NY = (C.Y > 0.f ? 1.f : -1.f) * (TwrWell - 1.5f);
        const float Z = TwrLandingH(K), Bottom = K <= 1 ? TwrFloor : Z - 34.f, Top = Z + 122.f;
        Box(Clutter, TEXT("CI_OakDark"), TwrAt(NX - 4.5f, NY - 4.5f, Bottom), TwrAt(NX + 4.5f, NY + 4.5f, Top - 4.f));
        Box(Clutter, TEXT("CI_Oak"), TwrAt(NX - 6.f, NY - 6.f, Top - 4.f), TwrAt(NX + 6.f, NY + 6.f, Top));
    }
}

/** Caged bulbs on short iron arms: one in each landing's outer corner, one over the door, one over the store. */
void TwrLamps(FHallMesh& Clutter, FHallMesh& Glow)
{
    auto Lamp = [&Clutter, &Glow](const FVector& Bulb, const FVector& Mount)
    {
        HouseCampusFallKit::Ball(Glow, TEXT("CI_DomeLight"), Bulb, 6.f, 8);
        Clutter.Cylinder(TEXT("CT_Iron"), Bulb + FVector(0.f, 0.f, 7.f), Bulb + FVector(0.f, 0.f, 10.f), 8.f, 10, UVPlain());
        for (int32 I = 0; I < 4; ++I)
        {
            const float A = HALF_PI * I + PI * .25f;
            const FVector Rim(FMath::Cos(A) * 7.5f, FMath::Sin(A) * 7.5f, 0.f);
            HouseCampusFallKit::Bar(Clutter, TEXT("CT_Iron"), Bulb + Rim + FVector(0.f, 0.f, 8.f), Bulb + Rim * .4f - FVector(0.f, 0.f, 8.f), .8f, .8f, Rim);
        }
        HouseCampusFallKit::Bar(Clutter, TEXT("CT_Iron"), Bulb + FVector(0.f, 0.f, 10.f), FVector(Mount.X, Mount.Y, Bulb.Z + 10.f), 1.6f, 1.6f);
    };
    for (int32 K = 1; K < TwrFlights; ++K)
    {
        const FVector2D C = TwrCorner(K);
        const float SX = C.X > 0.f ? 1.f : -1.f, SY = C.Y > 0.f ? 1.f : -1.f;
        const float Ceiling = K == TwrFlights - 1 ? TwrRecessTop : TwrCeil;
        const float Z = FMath::Min(TwrLandingH(K) + 228.f, Ceiling - 16.f);
        Lamp(TwrAt(SX * 236.f, SY * 236.f, Z), TwrAt(SX * (TwrIn - .5f), SY * (TwrIn - .5f), Z));
    }
    Lamp(TwrAt(0.f, 240.f, 540.f), TwrAt(0.f, TwrIn - .5f, 540.f));
    Lamp(TwrAt(-238.f, 60.f, 330.f), TwrAt(-TwrIn + .5f, 60.f, 330.f));
}

// ------------------------------------------------------------------ the ground floor
/** The cupboard under the first flight, the weight chute, the wall clock, the notice board, and Buildings & Grounds' corner:
 *  the house's kit (a shelf of ledgers, a wire rack, the bench with the clock's logbook, the coffee chest, a wastebasket, an
 *  extinguisher) and its props (cartons of bunting and lights, storage boxes, papers and cups on the bench and the chest, a
 *  backpack, an umbrella, a trash can), all clear of the walk from the door to the first flight and of the leaves' swing. */
void TwrGroundFloor(FHallMesh& Solid, FHallMesh& Clutter, const FKitter& Kit)
{
    using EKit = HouseCampusInteriors::EKit;
    const float Z = TwrBase() + TwrFloor;
    // The cupboard: boarded on the well side under the first flight's string and across its west end, its door there.
    const FTwrFlight First(1);
    const float UFoot = (26.f / TwrRise - 1.f) * TwrGoing - 2.5f;   // where the well string's underside leaves the floor
    First.Board(Solid, TEXT("CI_TwrBoards"), { FVector2D(UFoot, TwrFloor), FVector2D(TwrRun, TwrFloor), FVector2D(TwrRun, First.Nosing(TwrRun) - 26.f) }, TwrWell - 5.f, 3.f);
    First.Slab(Solid, TEXT("CI_TwrBoards"), TwrRun, TwrRun + 3.f, TwrWell + 3.f, TwrIn, TwrFloor, TwrLandingH(1) - 24.f);
    First.Slab(Clutter, TEXT("CI_OakDark"), TwrRun + 3.f, TwrRun + 4.f, 190.f, 248.f, TwrFloor + 2.f, TwrFloor + 150.f);
    First.Slab(Clutter, TEXT("CT_Iron"), TwrRun + 4.f, TwrRun + 5.5f, 194.f, 200.f, TwrFloor + 80.f, TwrFloor + 86.f);
    // The clock's weight chute: a boarded trunk in the well's south-west corner up to the ceiling (the weights come down it
    // from the movement over the bells), battened, its hatch at the foot.
    Box(Solid, TEXT("CI_TwrBoards"), TwrAt(-158.f, -158.f, TwrFloor), TwrAt(-98.f, -98.f, TwrCeil), FHallUV::World(120.f));
    for (float H = TwrFloor + 260.f; H < TwrCeil - 100.f; H += 330.f) Box(Clutter, TEXT("CI_OakDark"), TwrAt(-159.5f, -159.5f, H), TwrAt(-96.5f, -96.5f, H + 8.f));
    Box(Clutter, TEXT("CI_OakDark"), TwrAt(-97.f, -150.f, TwrFloor + 20.f), TwrAt(-96.f, -106.f, TwrFloor + 90.f));
    Box(Clutter, TEXT("CT_Iron"), TwrAt(-96.f, -113.f, TwrFloor + 50.f), TwrAt(-94.8f, -109.f, TwrFloor + 60.f));
    // The wall clock over the first flight, the first thing seen from the door; the notice board beside the door, with
    // Buildings & Grounds' notice over it and the tower's duties pinned on it.
    WallClock(Clutter, TwrAt(0.f, -TwrIn, 430.f), FVector(0, 1, 0), 20.f);
    BulletinBoard(Solid, Clutter, TwrAt(-185.f, TwrIn, 215.f), FVector(0, -1, 0), 110.f, 75.f, TEXT("CI_NoticeBoard"));
    WallPlaque(Clutter, TEXT("CI_TwrSchedule"), TwrAt(-205.f, TwrIn - 2.f, 215.f), FVector(0, -1, 0), 40.f, 50.f);
    WallPlaque(Clutter, TEXT("CI_TwrNotice"), TwrAt(-185.f, TwrIn, 300.f), FVector(0, -1, 0), 100.f, 56.f);
    // Rock salt for the steps outside, two sacks east of the door (clear of leaf B's swing, X <= 98), and the snow shovel
    // against the west wall.
    Box(Solid, TEXT("CI_TwrSack"), TwrAt(110.f, 210.f, TwrFloor), TwrAt(160.f, 246.f, TwrFloor + 14.f));
    Solid.Box(TEXT("CI_TwrSack"), FTransform(FRotator(0.f, 9.f, 0.f), TwrAt(135.f, 228.f, TwrFloor + 21.f)), FVector(25.f, 18.f, 7.f), UVPlain());
    HouseCampusFallKit::Card(Clutter, TEXT("CI_TwrSalt"), TwrAt(135.f, 209.6f, TwrFloor + 7.f), FVector(-1, 0, 0), FVector::UpVector, 40.f, 10.f, FVector(0, -1, 0));
    const FVector Grip = TwrAt(-255.f, 168.f, TwrFloor + 140.f), Heel = TwrAt(-238.f, 172.f, TwrFloor + 32.f), Shaft = (Grip - Heel).GetSafeNormal();
    Clutter.Cylinder(TEXT("CI_Oak"), Heel, Grip, 1.8f, 8, UVPlain());
    Clutter.Box(TEXT("CT_Iron"), FTransform(FRotationMatrix::MakeFromZY(Shaft, FVector(0, 1, 0)).ToQuat(), Heel - Shaft * 14.f), FVector(.6f, 13.f, 15.f), UVPlain());
    // The house's kit, stood from its real bounds (the manifests' sizes at campus scale: worktable 216 x 82, wire rack 76 x 51,
    // bookcase 74 x 30, coffee chest 113 x 59, wastebasket 27 x 25): along the west wall under the second flight the bench
    // (from just north of the cupboard's end, so its door stays clear) and the wire rack; the coffee chest and the wastebasket
    // west of the door, clear of leaf A's swing (X >= -98); the ledger shelf in the north-east corner, the extinguisher on the
    // east wall, both off the walk from the door to the first flight.
    const float BenchD = TwrKitHalf(Kit, EKit::Worktable, true), BenchW = TwrKitHalf(Kit, EKit::Worktable, false);
    const FVector2D Bench = TwrPlan(-TwrIn + BenchD + 1.f, -TwrWell - 1.f + BenchW);
    Kit.Floor(EKit::Worktable, Bench, Z, 0.f, false);
    const float BenchTop = Z + Kit.Height(EKit::Worktable);
    Kit.Floor(EKit::DeskLamp, Bench + FVector2D(-.5f * BenchD, .7f * BenchW), BenchTop, 20.f, false);
    const float RackD = TwrKitHalf(Kit, EKit::WireRack, true), RackW = TwrKitHalf(Kit, EKit::WireRack, false);
    Kit.Floor(EKit::WireRack, TwrPlan(-TwrIn + RackD + 1.f, -TwrWell - 1.f + 2.f * BenchW + 6.f + RackW), Z, 0.f, false);
    const float ChestD = TwrKitHalf(Kit, EKit::CoffeeChest, true), ChestW = TwrKitHalf(Kit, EKit::CoffeeChest, false);
    const FVector2D Chest = TwrPlan(-TwrIn + ChestW + 1.f, TwrIn - ChestD - 1.f);
    Kit.Floor(EKit::CoffeeChest, Chest, Z, -90.f, false);
    Kit.Floor(EKit::TrashBin, TwrPlan(-TwrIn + 2.f * ChestW + 9.f + TwrKitHalf(Kit, EKit::TrashBin, false), TwrIn - TwrKitHalf(Kit, EKit::TrashBin, true) - 1.f), Z, -90.f, false);
    const FVector2D Shelf = TwrPlan(TwrIn - TwrKitHalf(Kit, EKit::Bookcase, false) - 1.f, TwrIn - TwrKitHalf(Kit, EKit::Bookcase, true) - 1.f);
    Kit.Floor(EKit::Bookcase, Shelf, Z, -90.f, false);
    Kit.FillBookcase(Shelf, Z, -90.f, 4);
    Kit.Wall(EKit::FireExtinguisher, TwrAt(TwrIn, 120.f, TwrFloor + 55.f), FVector(-1, 0, 0));
    // Props, each on the floor or a flat top (none stacked: an open carton under another, or under papers, reads as floating
    // to the smoke's trace and lets them fall in): the logbook's bench (papers, ledgers, cups), a cup and papers on the chest,
    // cartons (44 x 41) and storage boxes (48 x 52) in two columns in the well's west half (X -159..-54, 8 cm apart, north of
    // the chute), a backpack, a trash can, an umbrella (107 long, lying between the shelf's end and the chest).
    const EKit OnBench[] = { EKit::PaperStack, EKit::StackedBooks, EKit::PaperStack, EKit::StackedBooks, EKit::PaperCup, EKit::PaperStack, EKit::PaperCup };
    const FVector2D BenchSpots[] = { FVector2D(-.45f, -.7f), FVector2D(.45f, -.7f), FVector2D(-.45f, -.25f), FVector2D(.45f, -.25f), FVector2D(-.45f, .2f), FVector2D(.45f, .2f), FVector2D(.45f, .65f) };
    for (int32 I = 0; I < int32(UE_ARRAY_COUNT(OnBench)); ++I) Kit.Floor(OnBench[I], Bench + FVector2D(BenchSpots[I].X * BenchD, BenchSpots[I].Y * BenchW), BenchTop, 23.f * I, true);
    const float ChestTop = Z + Kit.Height(EKit::CoffeeChest);
    Kit.Floor(EKit::PaperCup, Chest, ChestTop, 0.f, true);
    Kit.Floor(EKit::PaperStack, Chest + FVector2D(-.55f * ChestW, 2.f), ChestTop, 30.f, true);
    Kit.Floor(EKit::PaperStack, Chest + FVector2D(.55f * ChestW, -6.f), ChestTop, -12.f, true);
    struct FTwrFloorProp { EKit Piece; float X, Y, Yaw; };
    const FTwrFloorProp OnFloor[] = {
        { EKit::Cardboard, -135.f, 134.f, 4.f }, { EKit::Cardboard, -135.f, 85.f, -6.f }, { EKit::Cardboard, -135.f, 36.f, 2.f }, { EKit::StorageBox, -135.f, -20.f, 88.f },
        { EKit::Cardboard, -78.f, 134.f, -3.f }, { EKit::StorageBox, -78.f, 79.f, 92.f }, { EKit::StorageBox, -78.f, 19.f, 90.f }, { EKit::StorageBox, -78.f, -41.f, 87.f },
        { EKit::Backpack, -25.f, 125.f, 180.f }, { EKit::TrashCan, -20.f, -30.f, 0.f }, { EKit::Umbrella, -160.f, 175.f, 30.f },
    };
    for (const FTwrFloorProp& P : OnFloor) Kit.Floor(P.Piece, TwrPlan(P.X, P.Y), Z, P.Yaw, true);
}

/** Scratched into the lining where people stop for breath: the class that restored the tower, and the team. */
void TwrGraffiti(FHallMesh& Clutter)
{
    const FTwrFlight Sixth(6), Third(3);
    HouseCampusFallKit::GlyphStrips(Clutter, TEXT("CI_TwrScratch"), TEXT("CLASS OF 61"), TwrAt(-TwrIn + .3f, 41.f, Sixth.Nosing(TwrWell) + 150.f),
        FVector(0, -1, 0), FVector::UpVector, FVector(1, 0, 0), 9.f, .7f, 61);
    HouseCampusFallKit::GlyphStrips(Clutter, TEXT("CI_TwrScratch"), TEXT("GO GEESE"), TwrAt(-70.f, TwrIn - .3f, Third.Nosing(65.f) + 140.f),
        FVector(-1, 0, 0), FVector::UpVector, FVector(0, -1, 0), 11.f, .8f, 86);
}

void BuildTower(int32 Seed, float Height, FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit)
{
    TwrShell(Solid, Clutter, Glow);
    for (int32 K = 1; K <= TwrFlights; ++K) TwrFlight(Solid, Clutter, K);
    for (int32 K = 1; K < TwrFlights; ++K) TwrLanding(Solid, K);
    TwrNewels(Clutter);
    TwrLamps(Clutter, Glow);
    TwrGroundFloor(Solid, Clutter, Kit);
    TwrGraffiti(Clutter);
}

// ------------------------------------------------------------------ never drawn: the belfry's guards and the well's
/** Collision only (a hidden part of its own, FInteriorDef::Blockers). The belfry (the tower's Upper has none): its floor
 *  (Upper's top is 6 cm over the course's), its four walls up to the clock stage (every arch and louvred lancet: no fall
 *  from the tower), the frame's posts, the tie beams (crouch under them: 114 cm), the hour bell and the quarter bells, the
 *  guard round the slot's long side and west end, 86 cm (the tower draws no rail there, only the trap lid against the +Y
 *  wall). The shaft: behind each flight's well guard a plane from under its string to 2 m over the nosings
 *  (a 100 cm guard can be jumped), the top one's held under the ceiling; a plug behind each lancet's glass. */
void TwrBlockers(int32 Seed, float Height, FHallMesh& Out)
{
    const FName Key = TEXT("CI_Blocker");
    auto Guard = [&Out, Key](float X0, float X1, float Y0, float Y1, float Z0, float Z1) { Box(Out, Key, TwrAt(X0, Y0, Z0), TwrAt(X1, Y1, Z1)); };
    SubtractPlan(Rect(-255.f, 255.f, -255.f, 255.f), { Rect(TwrSlotX0, TwrSlotX1, TwrSlotY0, 255.f) }, [&Guard](const FBox2D& R)
    {
        Guard(float(R.Min.X), float(R.Max.X), float(R.Min.Y), float(R.Max.Y), TwrBelfry - 6.f, TwrBelfry);
    });
    const float Roof = 3600.f;   // the clock stage's floor (HouseClockTowerMesh HClock)
    Guard(-260.f, 260.f, 255.f, 260.f, TwrBelfry - 6.f, Roof);
    Guard(-260.f, 260.f, -260.f, -255.f, TwrBelfry - 6.f, Roof);
    Guard(255.f, 260.f, -260.f, 260.f, TwrBelfry - 6.f, Roof);
    Guard(-260.f, -255.f, -260.f, 260.f, TwrBelfry - 6.f, Roof);
    for (const float SX : { -1.f, 1.f })
    {
        for (const float SY : { -1.f, 1.f })
        {
            Guard(SX * 100.f - 9.f, SX * 100.f + 9.f, SY * 150.f - 9.f, SY * 150.f + 9.f, TwrBelfry, 3250.f);
            Guard(SX * 228.f - 8.f, SX * 228.f + 8.f, SY * 228.f - 8.f, SY * 228.f + 8.f, TwrBelfry, 3330.f);
            Guard(SX * 190.f - 47.f, SX * 190.f + 47.f, SY * 190.f - 47.f, SY * 190.f + 47.f, 3215.f, 3300.f);
        }
        Guard(-110.f, 110.f, SX * 150.f - 8.f, SX * 150.f + 8.f, 2900.f, 2916.f);
    }
    Guard(-85.f, 85.f, -85.f, 85.f, 3100.f, 3250.f);
    Guard(-171.f, TwrRailEnd, TwrSlotY0 - 6.f, TwrSlotY0, TwrBelfry, TwrBelfry + 86.f);
    Guard(-171.f, TwrSlotX0, TwrSlotY0, 255.f, TwrBelfry, TwrBelfry + 86.f);
    for (int32 K = 1; K <= TwrFlights; ++K) FTwrFlight(K).Band(Out, Key, TwrWell - 4.f, 4.f, 26.f, 200.f, K == TwrFlights ? TwrCeil : 1e5f);
    // The lancets (HouseClockTowerMesh Shaft): their fills, one-sided, are at Dist 282 (the low one) and 284 (the pair).
    for (int32 F = 0; F < HouseClockTowerMesh::NumFaces; ++F)
    {
        const FVector N = HouseClockTowerMesh::FaceNormal(F), U = HouseClockTowerMesh::FaceRight(F);
        const FQuat Q = FRotationMatrix::MakeFromXZ(N, FVector::UpVector).ToQuat();
        auto Plug = [&Out, &Key, &N, &U, &Q](float UC, float HalfW, float Sill, float Apex, float D1)
        {
            Out.Box(Key, FTransform(Q, TwrAt(0.f, 0.f, (Sill + Apex) * .5f) + N * ((259.f + D1) * .5f) + U * UC), FVector((D1 - 259.f) * .5f, HalfW, (Apex - Sill) * .5f), UVPlain());
        };
        Plug(0.f, 32.f, 880.f, 1312.f, 281.f);
        for (const float UC : { -62.f, 62.f }) Plug(UC, 26.f, 1680.f, 2119.f, 283.f);
    }
}

// ------------------------------------------------------------------ the registry's functions
FBox TwrBounds() { return FBox(TwrAt(-346.f, -346.f, -10.f), TwrAt(346.f, 346.f, 3600.f)); }   // the base course's ledge, to the clock stage
/** The shaft only, up to the course under the belfry: the open belfry is outdoors (the bells at full strength there, the
 *  street's lights on the player, the pool off). */
bool TwrInside(const FVector& P)
{
    const FVector R = P - TwrAt(0.f, 0.f, 0.f);
    return FMath::Abs(R.X) < 259.f && FMath::Abs(R.Y) < 259.f && R.Z > 60.f && R.Z < 2780.f;
}
/** Floor 0 is the door's, 1 the belfry's (the review's floor checks). The light pool bands views and anchors by these
 *  (FloorOf: the highest K, from 8 down, whose floor less 20 is under them), so floors 2..8 are put under the ground floor:
 *  every view and anchor in the tower is band 8, the whole shaft, with no slab between its flights, is one band (a low
 *  view on the ground floor still gets its lights), and no anchor is a ground-floor one, so nothing is lit from outside
 *  (bLightsInsideOnly besides). */
float TwrFloorZ(int32 Floor) { return TwrBase() + (Floor == 1 ? TwrBelfry : Floor == 0 ? TwrFloor : TwrFloor - 200.f); }

void TwrLightAnchors(int32 Seed, float Height, TArray<FVector>& Out)
{
    // Over the foot of the well, and 2.4 m over each landing (held under the ceiling at the top): every one over a floor of
    // the stair's own (the smoke traces down from each).
    Out.Add(TwrAt(0.f, 0.f, 365.f));
    for (int32 K = 1; K < TwrFlights; ++K)
    {
        const FVector2D C = TwrCorner(K);
        Out.Add(TwrAt(float(C.X), float(C.Y), FMath::Min(TwrLandingH(K) + 240.f, 2700.f)));
    }
}
void TwrOpenings(int32 Seed, float Height, TArray<HouseCampusInteriors::FOpening>& Out) {}
void TwrDoors(int32 Seed, float Height, TArray<FDoorSpec>& Out)
{
    // The pair in the tower's pointed doorway (U -95..95 on its +Y face, sill 75; the leaves up to the springing at 330, the
    // tympanum over them stays the tower's): hinged 4.4 inside the fixed door's plane (Dist 294), so a closed leaf's outer
    // face stands where that did, swinging in to Dist 195 inside the lining's doorway. Ledged plank leaves in the tower's own
    // materials, no lever or closer; the meeting bar on A.
    const float HingeY = TwrY + 294.f - 4.4f, Z = TwrBase() + 75.f, W = (190.f - 1.2f) * .5f;
    const FVector Center(TwrX, TwrY + 300.f, TwrBase() + 180.f);
    const FName Group(TEXT("Campus_TWR_Main"));
    FDoorSpec A{ TEXT("MainA"), FVector(TwrX - 95.f + .4f, HingeY, Z), 0.f, EHallDoorStyle::Plank, W, -1.f, uint8(AHouseBlackHallDoor::Astragal), Center, Group, FString() };
    A.Height = 255.f;
    Out.Add(A);
    FDoorSpec B{ TEXT("MainB"), FVector(TwrX + 95.f - .4f, HingeY, Z), 180.f, EHallDoorStyle::Plank, W, 1.f, uint8(0), Center, Group, FString() };
    B.Height = 255.f;
    Out.Add(B);
}

void TowerLook(UHouseBlackHallSubsystem* Look)
{
    // (Not with -NoClockTower: no tower to open, and no need to paint its look here.)
    if (!Look || !HouseCampusInteriors::ClockTowerOpen() || Look->Materials.Contains(TEXT("CI_TwrBrick"))) return;
    // The tower's own brick (its texture and normal), sooty and without the floodlit glow it has outside; the CT_ keys the
    // door leaves, cages and hatch use come with it.
    HouseClockTowerMesh::CreateLook(Look);
    UTexture2D* const Brick = Look->Textures.FindRef(TEXT("T_CT_Brick"));
    UTexture2D* const BrickN = Look->Textures.FindRef(TEXT("T_CT_BrickN"));
    if (Brick) HallMakeTextured(Look, TEXT("CI_TwrBrick"), Brick, BrickN, .92f, FLinearColor(.62f, .56f, .52f), .8f);
    else HallMakePlain(Look, TEXT("CI_TwrBrick"), FLinearColor(.17f, .07f, .045f), .92f);
    HallMakePlain(Look, TEXT("CI_TwrStone"), FLinearColor(.3f, .28f, .25f), .85f);
    {
        // Flagstones in staggered courses, the joints dark with grit.
        FHallImage Img; Img.Init(256, 256, FLinearColor(.06f, .055f, .05f));
        FRandomStream R(1897);
        for (int32 Row = 0; Row < 4; ++Row)
            for (int32 Col = -1; Col < 4; ++Col)
            {
                const float X0 = Col * 64.f + (Row % 2 ? 32.f : 0.f), Y0 = Row * 64.f;
                Img.Rect(FVector2D(X0 + 2.f, Y0 + 2.f), FVector2D(X0 + 62.f, Y0 + 62.f), FLinearColor(.3f, .28f, .25f) * R.FRandRange(.78f, 1.1f));
            }
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_TwrFlags"), true);
        if (Tex) Look->Textures.Add(TEXT("CI_TwrFlags"), Tex);
        HallMakeTextured(Look, TEXT("CI_TwrFlags"), Tex, nullptr, .85f);
    }
    {
        // Boards: the ceiling, the cupboard, the weight chute.
        FHallImage Img; Img.Init(128, 128, FLinearColor(.045f, .025f, .012f));
        FRandomStream R(1898);
        for (int32 Board = 0; Board < 4; ++Board) Img.Rect(FVector2D(Board * 32.f + 1.f, 0.f), FVector2D(Board * 32.f + 31.f, 128.f), FLinearColor(.2f, .11f, .05f) * R.FRandRange(.8f, 1.15f));
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_TwrBoards"), true);
        if (Tex) Look->Textures.Add(TEXT("CI_TwrBoards"), Tex);
        HallMakeTextured(Look, TEXT("CI_TwrBoards"), Tex, nullptr, .8f);
    }
    // The lancets from inside: dusk through plain glass; the two lit from outside amber, as if the lamp's light were on them.
    HallMakePlain(Look, TEXT("CI_TwrPane"), FLinearColor(.03f, .045f, .08f), .1f, .3f, FLinearColor(.035f, .06f, .13f));
    HallMakePlain(Look, TEXT("CI_TwrPaneLit"), FLinearColor(.5f, .35f, .2f), .3f, 0.f, FLinearColor(.95f, .56f, .24f));
    HallMakePlain(Look, TEXT("CI_TwrScratch"), FLinearColor(.5f, .42f, .34f), .95f);
    HallMakePlain(Look, TEXT("CI_TwrSack"), FLinearColor(.36f, .3f, .2f), .95f);
    const FLinearColor Cream(.88f, .85f, .78f), Red(.55f, .07f, .05f), Ink(.08f, .07f, .06f);
    PaintText(Look, TEXT("CI_TwrNotice"), 512, 288, Cream, Red, { { TEXT("CALDER STATE COLLEGE"), .1f }, { TEXT("BUILDINGS & GROUNDS"), .12f }, { TEXT("NO ADMITTANCE"), .17f },
        { TEXT("KEEP CLEAR OF THE BELLS WHILE THE CLOCK STRIKES"), .07f } }, true);
    SciPaintLines(Look, TEXT("CI_TwrSchedule"), 256, 320, Cream, Ink, { TEXT("TOWER DUTIES"), TEXT("WIND THE CLOCK - MONDAYS"), TEXT("OIL THE BELL FRAME - OCT 1"),
        TEXT("CHECK WEIGHT LINES FOR ICE"), TEXT("SALT THE STEPS - DEC TO MARCH"), TEXT("CHIMES 7 AM TO 10 PM"), TEXT("KEYS AT B & G - EXT 214") }, 34.f);
    PaintText(Look, TEXT("CI_TwrSalt"), 256, 64, FLinearColor(.36f, .3f, .2f), FLinearColor(.12f, .2f, .45f), { { TEXT("ROCK SALT 50 LB"), .5f } }, false);
}

FInteriorDef TowerDef()
{
    // Off with the tower itself (-NoClockTower): no inside, no doors, no indoor test, and the shell stays closed.
    FInteriorDef D{ TEXT("TWR"), FParse::Param(FCommandLine::Get(), TEXT("NoClockTower")) ? 0 : 2, &BuildTower, &TwrLightAnchors, &TwrOpenings, &TwrDoors, &TowerLook };
    D.Bounds = &TwrBounds;
    D.Inside = &TwrInside;
    D.FloorZ = &TwrFloorZ;
    D.Blockers = &TwrBlockers;
    // Its box takes in the belfry and the plinth round the shaft: a view there must keep the outside lights of the building
    // it would have had them from without the tower (the -ClockTowerReview Belfry camera: the Union's).
    D.bLightsInsideOnly = true;
    return D;
}
}

// ------------------------------------------------------------------ the tower's side (HouseClockTowerMesh asks this)
bool HouseCampusInteriors::ClockTowerOpen() { return OpenFloors(SeedFor(TEXT("TWR"))) > 0; }
