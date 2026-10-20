// Hollis Hall of Science (master job, step 2c, Tier A): a 1930s six-storey brick classroom building, its ground and first
// floors walk-in. Canonical frame (main doors on +Y): X -2300..2300, Y -1600..1600; floors at -55 and 360, plaster
// ceilings at 330 and 745. Every partition that meets an outer wall stands on a pier of the window grid.
//
// Ground floor: the lobby (twin oak stairs rising to the corridor above, the directory, the specimen case, the Physics Club
// bake sale by the doors, the alumni banner); the chemistry lab 101 (fume hoods on the west piers, epoxy-topped bench
// islands with gas cocks and glassware, reagent cabinets, the safety shower, the periodic table, the day's equations); the
// physics lab 102 (an air track, a pendulum, an inclined plane, the oscilloscope, a Van de Graaff generator); a corridor of
// lockers across the middle; and behind it the tiered lecture hall 110, two storeys high: the lecturer's well (three boards
// of equations, the giant slide rule, the demo bench, the lectern, an overhead projector and its screen) and ten rows of
// fixed seats rising to a cross-aisle under the clerestory windows. The prep room 112 and the department office 114 are
// locked. First floor: the upper lobby round the stair wells (a case of old instruments, the open-house easel), the optics
// lab 204 (laser benches, the spectrum chart), seminar room 210, the faculty lounge 212 with the alumni coffee; the
// darkroom 206 and the offices 214 are locked. Rooms numbered on brass plates; the directory agrees.
#include "HouseCampusInteriorsRooms.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusFallKit.h"
#include "Engine/Texture2D.h"

namespace HouseCampusInteriorsImpl
{
// Partition centre lines (all on piers): the corridor's walls; the lobby's side walls; the lecture hall's side walls; the
// first floor's seminar | lounge wall.
constexpr float SciYN = 267.5f, SciYS = -92.5f, SciXW = -792.5f, SciXE = 647.5f, SciHX0 = -1512.5f, SciHX1 = 1367.5f, SciLX = 1367.5f;
constexpr float SciInX = 2270.f, SciInY = 1570.f;
constexpr float SciWellLow = 987.5f;                         // the lobby stairs' feet (they climb toward -Y to the corridor)
constexpr float SciCorY = (SciYN + SciYS) * .5f;
// The lecture hall's tiers: ten rows of 90 rising 17.3 from the lecturer's well, the cross-aisle as the eleventh.
constexpr int32 SciRows = 10;
constexpr float SciRowD = 90.f, SciRise = 17.3f, SciWellY = -540.f;
constexpr float SciAisle0 = -143.f, SciAisle1 = -3.f;        // the centre aisle, on the back wall's centre pier

FBox2D SciHall() { return Rect(SciHX0 + PartT * .5f, SciHX1 - PartT * .5f, -SciInY, SciYS - PartT * .5f); }
void SciStairWells(TArray<FBox2D>& Out)
{
    Out.Add(Rect(SciXW + PartT * .5f, SciXW + PartT * .5f + 250.f, SciYN, SciWellLow));
    Out.Add(Rect(SciXE - PartT * .5f - 250.f, SciXE - PartT * .5f, SciYN, SciWellLow));
}

// ------------------------------------------------------------------ fittings
/** A schoolhouse globe on a rod from the ceiling (the milk glass lit by emission). */
void SciGlobe(FHallMesh& Glow, FHallMesh& Clutter, const FVector2D& P, float Ceil)
{
    Clutter.Cylinder(TEXT("CI_Brass"), FVector(P, Ceil - 60.f), FVector(P, Ceil), .8f, 6, UVPlain());
    Clutter.Cylinder(TEXT("CI_Brass"), FVector(P, Ceil - 64.f), FVector(P, Ceil - 58.f), 6.f, 10, UVPlain());
    HouseCampusFallKit::Ball(Glow, TEXT("CI_DomeLight"), FVector(P, Ceil - 80.f), 17.f, 12);
}
/** A lab stool fixed to the floor: a round oak seat on four splayed steel legs. */
void SciStool(FHallMesh& Solid, const FVector& C)
{
    Solid.Cylinder(TEXT("CI_OakDark"), C + FVector(0, 0, 60.f), C + FVector(0, 0, 64.f), 17.f, 10, UVPlain());
    for (const FVector2D& D : { FVector2D(1, 1), FVector2D(-1, 1), FVector2D(1, -1), FVector2D(-1, -1) })
        HouseCampusFallKit::Bar(Solid, TEXT("CI_Steel"), C + FVector(D.X * 15.f, D.Y * 15.f, 0.f), C + FVector(D.X * 9.f, D.Y * 9.f, 60.f), 2.f, 2.f, FVector(D.X, D.Y, 0.f));
}
/** A glass-fronted oak cabinet against a wall (W wide, D deep, H tall, facing Facing), bottles on its shelves. */
void SciCabinet(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Facing, float W, float D, float H, int32 Seed)
{
    const FVector F = Facing.GetSafeNormal2D(), R(-F.Y, F.X, 0.f);
    const FRotator Rot = F.Rotation();
    Solid.Box(TEXT("CI_Oak"), FTransform(Rot, C + FVector(0, 0, 45.f)), FVector(D * .5f, W * .5f, 45.f), UVPlain());
    Solid.Box(TEXT("CI_OakDark"), FTransform(Rot, C - F * (D * .5f - 1.f) + FVector(0, 0, H * .5f)), FVector(1.f, W * .5f, H * .5f), UVPlain());
    Solid.Box(TEXT("CI_OakDark"), FTransform(Rot, C + FVector(0, 0, H - 2.f)), FVector(D * .5f, W * .5f, 2.f), UVPlain());
    for (const float S : { -1.f, 1.f }) Solid.Box(TEXT("CI_OakDark"), FTransform(Rot, C + R * (S * (W * .5f - 1.5f)) + FVector(0, 0, H * .5f)), FVector(D * .5f, 1.5f, H * .5f), UVPlain());
    Solid.Box(TEXT("CI_GlassSash"), FTransform(Rot, C + F * (D * .5f - .5f) + FVector(0, 0, 90.f + (H - 92.f) * .5f)), FVector(.4f, W * .5f - 3.f, (H - 92.f) * .5f), UVPlain());
    FRandomStream Rng(Seed);
    const FName Glass[] = { TEXT("CI_SciReagent"), TEXT("CI_SciAmber"), TEXT("CI_Beige"), TEXT("CI_SciBlueGlass") };
    for (int32 Shelf = 0; Shelf < 3; ++Shelf)
    {
        const float Z = 92.f + Shelf * (H - 100.f) / 3.f;
        Clutter.Box(TEXT("CI_Oak"), FTransform(Rot, C + FVector(0, 0, Z)), FVector(D * .5f - 3.f, W * .5f - 3.f, .8f), UVPlain());
        for (float Along = -W * .5f + 12.f; Along < W * .5f - 10.f; Along += Rng.FRandRange(14.f, 24.f))
        {
            const float BH = Rng.FRandRange(12.f, 24.f), BR = Rng.FRandRange(3.f, 5.f);
            const FVector Base = C + R * Along + F * Rng.FRandRange(-4.f, 4.f) + FVector(0, 0, Z + .8f);
            Clutter.Cylinder(Glass[Rng.RandRange(0, 3)], Base, Base + FVector(0, 0, BH), BR, 6, UVPlain());
        }
    }
}
/** Glassware on a bench top at C: a beaker, an Erlenmeyer flask, a rack of test tubes, a Bunsen burner (some of them). */
void SciGlassware(FHallMesh& Clutter, const FVector& C, int32 Seed)
{
    FRandomStream R(Seed);
    if (R.FRand() < .8f)
    {
        const FVector B = C + FVector(R.FRandRange(-10.f, 10.f), R.FRandRange(-8.f, 8.f), 0.f);
        Clutter.Cylinder(TEXT("CI_Glass"), B, B + FVector(0, 0, 14.f), 5.f, 10, UVPlain());
        Clutter.Cylinder(TEXT("CI_SciReagent"), B + FVector(0, 0, .3f), B + FVector(0, 0, 6.f), 4.6f, 10, UVPlain());
    }
    if (R.FRand() < .7f)
    {
        const FVector B = C + FVector(25.f, R.FRandRange(-10.f, 10.f), 0.f);
        HouseCampusFallKit::Lathe(Clutter, TEXT("CI_Glass"), B, { FVector2D(.1f, 0.f), FVector2D(7.f, 0.f), FVector2D(7.f, 1.f), FVector2D(2.f, 12.f), FVector2D(2.f, 18.f), FVector2D(.1f, 18.f) }, 10);
        HouseCampusFallKit::Lathe(Clutter, R.FRand() < .5f ? TEXT("CI_SciAmber") : TEXT("CI_SciBlueGlass"), B + FVector(0, 0, .3f), { FVector2D(.1f, 0.f), FVector2D(6.5f, 0.f), FVector2D(5.f, 4.f), FVector2D(.1f, 4.f) }, 10);
    }
    if (R.FRand() < .6f)
    {
        const FVector B = C + FVector(-28.f, 0.f, 0.f);
        Clutter.Box(TEXT("CI_OakDark"), FTransform(B + FVector(0, 0, 6.f)), FVector(4.f, 14.f, .8f), UVPlain());
        for (const float S : { -1.f, 1.f }) Clutter.Box(TEXT("CI_OakDark"), FTransform(B + FVector(0, S * 13.f, 4.f)), FVector(4.f, .8f, 4.f), UVPlain());
        for (int32 K = 0; K < 5; ++K) Clutter.Cylinder(TEXT("CI_Glass"), B + FVector(0, -10.f + 5.f * K, 1.f), B + FVector(0, -10.f + 5.f * K, 13.f), 1.f, 6, UVPlain());
    }
    if (R.FRand() < .5f)
    {
        const FVector B = C + FVector(R.FRandRange(-20.f, 20.f), 22.f, 0.f);
        Clutter.Cylinder(TEXT("CI_Steel"), B, B + FVector(0, 0, 1.f), 5.f, 10, UVPlain());
        Clutter.Cylinder(TEXT("CI_Steel"), B, B + FVector(0, 0, 14.f), 1.2f, 8, UVPlain());
        Clutter.Cylinder(TEXT("CI_Rubber"), B + FVector(4.f, 0, .6f), B + FVector(18.f, 6.f, .6f), .5f, 6, UVPlain());
    }
}
/** The specimen case: an oak base, a glass case above, jars of specimens, a skull, minerals, a fossil. */
void SciSpecimenCase(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FVector& C, const FVector& Out, float W)
{
    const FVector N = Out.GetSafeNormal2D(), U(-N.Y, N.X, 0.f);
    const FRotator Rot = N.Rotation();
    Solid.Box(TEXT("CI_OakDark"), FTransform(Rot, C + N * 25.f + FVector(0, 0, 45.f)), FVector(25.f, W * .5f, 45.f), UVPlain());
    Solid.Box(TEXT("CI_Glass"), FTransform(Rot, C + N * 49.f + FVector(0, 0, 150.f)), FVector(.5f, W * .5f, 60.f), UVPlain());
    for (const float S : { -1.f, 1.f }) Solid.Box(TEXT("CI_OakDark"), FTransform(Rot, C + N * 25.f + U * (S * (W * .5f - 2.f)) + FVector(0, 0, 150.f)), FVector(25.f, 2.f, 60.f), UVPlain());
    Solid.Box(TEXT("CI_OakDark"), FTransform(Rot, C + N * 25.f + FVector(0, 0, 213.f)), FVector(25.f, W * .5f, 3.f), UVPlain());
    Solid.Box(TEXT("CI_OakDark"), FTransform(Rot, C + N * 1.f + FVector(0, 0, 150.f)), FVector(1.f, W * .5f, 60.f), UVPlain());
    Glow.Box(TEXT("CI_HoodLight"), FTransform(Rot, C + N * 25.f + FVector(0, 0, 209.f)), FVector(18.f, W * .5f - 6.f, .5f), UVPlain());
    Solid.Box(TEXT("CI_Glass"), FTransform(Rot, C + N * 25.f + FVector(0, 0, 150.f)), FVector(22.f, W * .5f - 4.f, .6f), UVPlain());
    // Lower shelf (on the base): jars; upper shelf: the skull, the minerals, the fossil.
    FRandomStream R(int32(C.X + C.Y));
    for (float T = -W * .5f + 18.f; T < W * .5f - 14.f; T += 26.f)
    {
        const FVector P = C + N * 25.f + U * T + FVector(0, 0, 90.f);
        const float H = R.FRandRange(16.f, 28.f);
        Clutter.Cylinder(TEXT("CI_Glass"), P, P + FVector(0, 0, H), 7.f, 12, UVPlain());
        Clutter.Cylinder(TEXT("CI_SciFormalin"), P + FVector(0, 0, .5f), P + FVector(0, 0, H * .8f), 6.4f, 12, UVPlain());
        HouseCampusFallKit::Ball(Clutter, TEXT("CI_SciSpecimen"), P + FVector(0, 0, H * .4f), R.FRandRange(2.5f, 4.f), 6);
        Clutter.Cylinder(TEXT("CI_Rubber"), P + FVector(0, 0, H), P + FVector(0, 0, H + 2.f), 7.2f, 12, UVPlain());
    }
    const FVector Top = C + N * 25.f + FVector(0, 0, 150.6f);
    HouseCampusFallKit::Ball(Clutter, TEXT("CI_SciBone"), Top + U * (-W * .25f) + FVector(0, 0, 9.f), 9.f, 10);   // the skull
    Clutter.Box(TEXT("CI_SciBone"), FTransform(Rot, Top + U * (-W * .25f) + N * 6.f + FVector(0, 0, 3.f)), FVector(4.f, 5.f, 3.f), UVPlain());
    Clutter.Box(TEXT("CI_Epoxy"), FTransform(Rot, Top + U * (-W * .25f) + N * 8.f + FVector(0, 0, 11.f)), FVector(1.f, 4.f, 2.f), UVPlain());
    const FName Rock[] = { TEXT("CI_CrateRed"), TEXT("CI_SciBlueGlass"), TEXT("CI_Brass"), TEXT("CI_Sand"), TEXT("CI_SciAmber") };
    for (int32 K = 0; K < 7; ++K)
        Clutter.Box(Rock[K % 5], FTransform(FRotator(R.FRandRange(0, 40), R.FRandRange(0, 360), 0), Top + U * (W * .05f + 14.f * K - 40.f) + N * R.FRandRange(-6.f, 6.f) + FVector(0, 0, 3.f)), FVector(R.FRandRange(2.f, 4.f), R.FRandRange(2.f, 4.f), R.FRandRange(2.f, 3.f)), UVPlain());
    Clutter.Cylinder(TEXT("CI_Sand"), Top + U * (W * .38f), Top + U * (W * .38f) + FVector(0, 0, 3.f), 9.f, 14, UVPlain());   // an ammonite
    Clutter.Cylinder(TEXT("CI_SciBone"), Top + U * (W * .38f) + FVector(0, 0, 3.f), Top + U * (W * .38f) + FVector(0, 0, 3.3f), 6.f, 14, UVPlain());
}
/** A wall chart (any painted key) at C on a wall facing Out, its wooden rails top and bottom. */
void SciChart(FHallMesh& Clutter, FName Key, const FVector& C, const FVector& Out, float W, float H)
{
    const FVector N = Out.GetSafeNormal2D(), R = FVector::CrossProduct(FVector::UpVector, -N).GetSafeNormal();
    HouseCampusFallKit::Card(Clutter, Key, C + N * .5f, R, FVector::UpVector, W, H, N);
    for (const float S : { -1.f, 1.f }) Clutter.Box(TEXT("CI_OakDark"), FTransform(N.Rotation(), C + N * 1.f + FVector(0, 0, S * (H * .5f + 1.f))), FVector(1.f, W * .5f + 3.f, 1.2f), UVPlain());
}

// ------------------------------------------------------------------ rooms
/** The lecture hall's tiers, seats, well and boards (floor 0, two storeys high). */
void SciLectureHall(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, float F0, float Ceil)
{
    using EKit = HouseCampusInteriors::EKit;
    const FBox2D H = SciHall();
    // The tiers: each row a block from the floor to its own top, an oak riser face and carpet; the cross-aisle above them.
    for (int32 K = 1; K <= SciRows + 1; ++K)
    {
        const float YFront = SciWellY - SciRowD * (K - 1), YBack = K > SciRows ? H.Min.Y : SciWellY - SciRowD * K, Z = F0 + SciRise * K;
        Box(Solid, TEXT("CI_OakDark"), FVector(H.Min.X, YBack, F0 - 5.f), FVector(H.Max.X, YFront, Z - .6f));
        Box(Solid, TEXT("CI_Carpet"), FVector(H.Min.X, YBack, Z - .6f), FVector(H.Max.X, YFront, Z), FHallUV::World(180.f));
        if (K > SciRows) break;
        // Fixed seats facing the well: a writing counter on a modesty panel, then oak seats on steel standards, in two banks.
        for (const FVector2D& Bank : { FVector2D(H.Min.X + 100.f, SciAisle0), FVector2D(SciAisle1, H.Max.X - 100.f) })
        {
            Box(Solid, TEXT("CI_Oak"), FVector(Bank.X, YFront - 30.f, Z), FVector(Bank.Y, YFront - 27.f, Z + 72.f));
            Box(Solid, TEXT("CI_Laminate"), FVector(Bank.X, YFront - 30.f, Z + 72.f), FVector(Bank.Y, YFront - 3.f, Z + 75.f));
            const int32 Seats = FMath::FloorToInt((Bank.Y - Bank.X) / 55.f);
            const float Pitch = (Bank.Y - Bank.X) / Seats;
            for (int32 I = 0; I <= Seats; ++I)
            {
                const float X = Bank.X + Pitch * I;
                Box(Solid, TEXT("CI_Steel"), FVector(X - 1.5f, YFront - 82.f, Z), FVector(X + 1.5f, YFront - 45.f, Z + 64.f));
                if (I == Seats) break;
                const float XC = X + Pitch * .5f;
                Box(Solid, TEXT("CI_Oak"), FVector(XC - 23.f, YFront - 76.f, Z + 42.f), FVector(XC + 23.f, YFront - 40.f, Z + 46.f));
                Box(Solid, TEXT("CI_Oak"), FVector(XC - 23.f, YFront - 84.f, Z + 48.f), FVector(XC + 23.f, YFront - 80.f, Z + 96.f));   // the back
            }
        }
    }
    // The left-behind things: notebooks, a cup, a homecoming programme, a purple and gold scarf on a seat.
    Box(Clutter, TEXT("CI_SciNotebook"), FVector(-600.f, SciWellY - 90.f * 3 - 20.f, F0 + SciRise * 4 + 75.f), FVector(-575.f, SciWellY - 90.f * 3 - 4.f, F0 + SciRise * 4 + 76.5f));
    Box(Clutter, TEXT("CI_SciProgramme"), FVector(420.f, SciWellY - 90.f * 6 - 22.f, F0 + SciRise * 7 + 75.f), FVector(440.f, SciWellY - 90.f * 6 - 6.f, F0 + SciRise * 7 + 75.4f));
    Kit.Floor(EKit::PaperCup, FVector2D(-330.f, SciWellY - 90.f * 5 - 14.f), F0 + SciRise * 6 + 75.f, 0.f, true);
    for (int32 K = 0; K < 6; ++K)
        HouseCampusFallKit::Bar(Clutter, K % 2 ? TEXT("CI_StreamerGold") : TEXT("CI_StreamerPurple"), FVector(160.f + K * 4.f, SciWellY - 90.f * 2 - 58.f, F0 + SciRise * 3 + 47.f), FVector(160.f + K * 4.f, SciWellY - 90.f * 2 - 40.f, F0 + SciRise * 3 + 30.f), 4.f, .6f, FVector(0, 1, 0));
    // The well: three boards of the morning's equations, the slide rule over them, a screen pulled down over the middle one.
    const float Wall = SciYS - PartT * .5f;
    Chalkboard(Solid, Clutter, FVector(-520.f, Wall, F0 + 150.f), FVector(0, -1, 0), 360.f, 120.f, TEXT("CI_ChalkPhysics"));
    Chalkboard(Solid, Clutter, FVector(-73.f, Wall, F0 + 150.f), FVector(0, -1, 0), 360.f, 120.f, TEXT("CI_SciEquations"));
    Chalkboard(Solid, Clutter, FVector(374.f, Wall, F0 + 150.f), FVector(0, -1, 0), 360.f, 120.f, TEXT("CI_ChalkChem"));
    SciChart(Clutter, TEXT("CI_SciSlideRule"), FVector(-73.f, Wall, F0 + 262.f), FVector(0, -1, 0), 520.f, 34.f);
    Box(Clutter, TEXT("CI_Steel"), FVector(-60.f, Wall - 14.f, F0 + 300.f), FVector(260.f, Wall - 2.f, F0 + 309.f));
    Box(Clutter, TEXT("CI_SciScreen"), FVector(-50.f, Wall - 9.f, F0 + 160.f), FVector(250.f, Wall - 8.6f, F0 + 300.f));
    LabBench(Solid, Clutter, FVector(-73.f, -330.f, F0), FVector(1, 0, 0), 360.f);
    SciGlassware(Clutter, FVector(-150.f, -330.f, F0 + 92.f), 110);
    SciGlassware(Clutter, FVector(40.f, -330.f, F0 + 92.f), 111);
    // The lectern, the projector on its cart, the periodic table on the side wall.
    Box(Solid, TEXT("CI_Oak"), FVector(280.f, -480.f, F0), FVector(340.f, -440.f, F0 + 104.f));
    Solid.Box(TEXT("CI_OakDark"), FTransform(FRotator(18.f, 90.f, 0.f), FVector(310.f, -462.f, F0 + 110.f)), FVector(30.f, 34.f, 1.5f), UVPlain());
    OverheadProjector(Solid, Glow, FVector(520.f, -470.f, F0), FVector(0, -1, 0));
    SciChart(Clutter, TEXT("CI_PeriodicTable"), FVector(H.Min.X, -320.f, F0 + 185.f), FVector(1, 0, 0), 310.f, 175.f);
    WallPlaque(Clutter, TEXT("CI_SciHallRules"), FVector(H.Max.X, -320.f, F0 + 175.f), FVector(-1, 0, 0), 90.f, 60.f);
    WallClock(Clutter, FVector(H.Max.X, -320.f, F0 + 262.f), FVector(-1, 0, 0), 20.f);
    // Blackout drapes over the ground floor's back windows (behind the top rows; closed windows outside).
    TArray<FVector2D> Cols;
    for (float X = -1420.f; X < 1300.f; X += 360.f) Cols.Add(FVector2D(X, X + 175.f));
    for (const FVector2D& C : Cols) Box(Clutter, TEXT("CI_SciDrape"), FVector(C.X - 20.f, H.Min.Y + .5f, F0 + SciRise * 11.f), FVector(C.Y + 20.f, H.Min.Y + 4.f, F0 + 310.f));
    // Lights high in the hall.
    for (float X = -1200.f; X <= 1100.f; X += 460.f)
        for (float Y = -300.f; Y >= -1450.f; Y -= 380.f) Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), true);
}

/** The chemistry lab (floor 0, front west). */
void SciChemLab(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, float Z, float Ceil)
{
    using EKit = HouseCampusInteriors::EKit;
    for (const float Y : { 627.5f, 987.5f, 1412.5f }) FumeHood(Solid, Glow, FVector(-SciInX + 40.f, Y, Z), FVector(1, 0, 0));
    int32 Seed = 1;
    // Four islands, clear of the hoods to the west and of the demo bench to the east.
    for (const float X : { -1900.f, -1400.f })
        for (const float Y : { 700.f, 1100.f })
        {
            LabBench(Solid, Clutter, FVector(X, Y, Z), FVector(1, 0, 0), 380.f);
            for (float DX = -150.f; DX <= 150.f; DX += 100.f)
            {
                SciGlassware(Clutter, FVector(X + DX, Y + (Seed % 2 ? 30.f : -30.f), Z + 92.f), Seed * 7 + int32(DX));
                for (const float S : { -1.f, 1.f }) Kit.Floor(EKit::BarStool, FVector2D(X + DX + 10.f, Y + S * 92.f), Z, S > 0.f ? -90.f : 90.f, true);
            }
            ++Seed;
        }
    // The teacher's demo bench and board on the lobby wall; the periodic table beside it.
    const float East = SciXW - PartT * .5f;
    Chalkboard(Solid, Clutter, FVector(East, 900.f, Z + 150.f), FVector(-1, 0, 0), 400.f, 120.f, TEXT("CI_ChalkChem"));
    WallClock(Clutter, FVector(East, 900.f, Z + 258.f), FVector(-1, 0, 0), 15.f);
    LabBench(Solid, Clutter, FVector(East - 170.f, 900.f, Z), FVector(0, 1, 0), 300.f);
    SciGlassware(Clutter, FVector(East - 170.f, 860.f, Z + 92.f), 77);
    SciChart(Clutter, TEXT("CI_PeriodicTable"), FVector(East, 1350.f, Z + 180.f), FVector(-1, 0, 0), 300.f, 170.f);
    // Reagent cabinets along the corridor wall, the safety shower in the corner.
    for (const float X : { -2150.f, -1850.f, -1550.f, -1250.f }) SciCabinet(Solid, Clutter, FVector(X, SciYN + PartT * .5f + 22.f, Z), FVector(0, 1, 0), 140.f, 44.f, 210.f, int32(-X));
    const FVector Shower(-2190.f, 1490.f, Z);
    Clutter.Cylinder(TEXT("CI_Steel"), Shower + FVector(0, 0, 220.f), FVector(Shower.X, Shower.Y, Ceil), 2.f, 8, UVPlain());
    Clutter.Cylinder(TEXT("CI_Steel"), Shower + FVector(0, 0, 214.f), Shower + FVector(0, 0, 222.f), 14.f, 12, UVPlain());
    Clutter.Cylinder(TEXT("CI_Steel"), Shower + FVector(12.f, 0, 214.f), Shower + FVector(12.f, 0, 160.f), .4f, 6, UVPlain());
    Clutter.Cylinder(TEXT("CI_SciGreen"), Shower + FVector(12.f, -5.f, 158.f), Shower + FVector(12.f, 5.f, 158.f), 4.f, 10, UVPlain());
    WallPlaque(Clutter, TEXT("CI_SciShowerSign"), FVector(-2100.f, SciInY, Z + 240.f), FVector(0, -1, 0), 70.f, 30.f);
    for (const float X : { -2052.5f, -1692.5f, -1332.5f, -972.5f }) Radiator(Solid, FVector(X, SciInY, Z), FVector(0, -1, 0), 120.f);
    for (float X = -2000.f; X <= -1000.f; X += 500.f) for (const float Y : { 500.f, 900.f, 1300.f }) Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), true);
    Kit.Floor(EKit::TrashCan, FVector2D(-900.f, 360.f), Z, 0.f, true);
}

/** The physics lab (floor 0, front east). */
void SciPhysicsLab(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, float Z, float Ceil)
{
    using EKit = HouseCampusInteriors::EKit;
    const float West = SciXE + PartT * .5f;
    Chalkboard(Solid, Clutter, FVector(West, 900.f, Z + 150.f), FVector(1, 0, 0), 400.f, 120.f, TEXT("CI_ChalkPhysics"));
    WallClock(Clutter, FVector(West, 900.f, Z + 258.f), FVector(1, 0, 0), 15.f);
    Table(Solid, FVector(West + 170.f, 900.f, Z), 180.f, 80.f, 86.f, false);
    Kit.Floor(EKit::GrayOfficeChair, FVector2D(West + 90.f, 980.f), Z, 0.f, true);
    const FVector T1(1150.f, 700.f, Z), T2(1750.f, 700.f, Z), T3(1150.f, 1150.f, Z), T4(1750.f, 1150.f, Z);
    for (const FVector& T : { T1, T2, T3, T4 })
    {
        Table(Solid, T, 260.f, 90.f, 86.f, true);
        for (float DX = -90.f; DX <= 90.f; DX += 90.f) Kit.Floor(EKit::BarStool, FVector2D(T.X + DX, T.Y - 80.f), Z, 90.f, true);
    }
    const float Top = Z + 86.f;
    // The air track and its gliders.
    Box(Solid, TEXT("CI_Aluminium"), FVector(T1.X - 120.f, T1.Y - 5.f, Top), FVector(T1.X + 120.f, T1.Y + 5.f, Top + 9.f));
    for (const float X : { -60.f, 40.f }) Box(Solid, TEXT("CI_CrateRed"), FVector(T1.X + X - 9.f, T1.Y - 7.f, Top + 7.f), FVector(T1.X + X + 9.f, T1.Y + 7.f, Top + 13.f));
    Clutter.Cylinder(TEXT("CI_CrtBezel"), FVector(T1.X + 125.f, T1.Y, Top), FVector(T1.X + 125.f, T1.Y, Top + 16.f), 7.f, 10, UVPlain());   // the blower
    // The pendulum on its stand.
    const FVector Stand(T2.X - 60.f, T2.Y + 10.f, Top);
    Clutter.Cylinder(TEXT("CI_Steel"), Stand, Stand + FVector(0, 0, 90.f), 1.2f, 8, UVPlain());
    Clutter.Cylinder(TEXT("CI_Steel"), Stand + FVector(0, 0, 88.f), Stand + FVector(40.f, 0, 88.f), 1.f, 8, UVPlain());
    Box(Clutter, TEXT("CI_Steel"), Stand + FVector(-10.f, -8.f, 0.f), Stand + FVector(10.f, 8.f, 2.f));
    Clutter.Cylinder(TEXT("CI_Paper"), Stand + FVector(40.f, 0, 88.f), Stand + FVector(46.f, 0, 30.f), .2f, 4, UVPlain());
    HouseCampusFallKit::Ball(Solid, TEXT("CI_Brass"), Stand + FVector(46.6f, 0, 26.f), 4.f, 8);
    // The inclined plane and its cart; meter sticks; weights.
    Solid.Box(TEXT("CI_Oak"), FTransform(FRotator(14.f, 0.f, 0.f), FVector(T3.X - 30.f, T3.Y, Top + 16.f)), FVector(60.f, 12.f, 1.2f), UVPlain());
    Box(Solid, TEXT("CI_Oak"), FVector(T3.X + 25.f, T3.Y - 12.f, Top), FVector(T3.X + 28.f, T3.Y + 12.f, Top + 30.f));
    Solid.Box(TEXT("CI_CrateBlue"), FTransform(FRotator(14.f, 0.f, 0.f), FVector(T3.X - 30.f, T3.Y, Top + 20.f)), FVector(8.f, 6.f, 3.f), UVPlain());
    for (int32 K = 0; K < 3; ++K) Box(Clutter, TEXT("CI_TissueGold"), FVector(T3.X + 40.f, T3.Y - 30.f + K * 4.f, Top), FVector(T3.X + 140.f, T3.Y - 27.f + K * 4.f, Top + .8f));
    for (int32 K = 0; K < 4; ++K) Clutter.Cylinder(TEXT("CI_Brass"), FVector(T3.X + 60.f + K * 9.f, T3.Y + 25.f, Top), FVector(T3.X + 60.f + K * 9.f, T3.Y + 25.f, Top + 3.f + K * 2.f), 3.f + K, 10, UVPlain());
    // The oscilloscope and signal generator.
    Terminal(Solid, Glow, FVector(T4.X - 40.f, T4.Y + 10.f, Top), FVector(0, -1, 0));
    Box(Solid, TEXT("CI_Beige"), FVector(T4.X + 20.f, T4.Y - 10.f, Top), FVector(T4.X + 70.f, T4.Y + 25.f, Top + 18.f));
    for (int32 K = 0; K < 3; ++K) Clutter.Cylinder(TEXT("CI_Epoxy"), FVector(T4.X + 30.f + K * 14.f, T4.Y - 10.f, Top + 9.f), FVector(T4.X + 30.f + K * 14.f, T4.Y - 12.f, Top + 9.f), 2.5f, 8, UVPlain());
    // The Van de Graaff generator in the corner.
    const FVector V(2150.f, 1440.f, Z);
    Solid.Cylinder(TEXT("CI_CrtBezel"), V, V + FVector(0, 0, 12.f), 22.f, 14, UVPlain());
    Solid.Cylinder(TEXT("CI_SciAcrylic"), V + FVector(0, 0, 12.f), V + FVector(0, 0, 120.f), 6.f, 12, UVPlain());
    HouseCampusFallKit::Ball(Solid, TEXT("CI_Aluminium"), V + FVector(0, 0, 140.f), 26.f, 16);
    for (const float X : { 827.5f, 1187.5f, 1547.5f, 1907.5f }) Radiator(Solid, FVector(X, SciInY, Z), FVector(0, -1, 0), 120.f);
    for (const float X : { 1300.f, 1700.f }) SciCabinet(Solid, Clutter, FVector(X, SciYN + PartT * .5f + 22.f, Z), FVector(0, 1, 0), 140.f, 44.f, 210.f, int32(X));
    for (float X = 850.f; X <= 2150.f; X += 430.f) for (const float Y : { 500.f, 900.f, 1300.f }) Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), true);
}

/** The lobby (floor 0) and the upper lobby (floor 1) round the twin stairs. */
void SciLobby(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, float F0, float F1, float C0, float C1)
{
    using EKit = HouseCampusInteriors::EKit;
    TArray<FBox2D> Wells; SciStairWells(Wells);
    for (const FBox2D& W : Wells)
    {
        Stair(Solid, W.Min.X + 4.f, W.Max.X - 4.f, W.Max.Y, W.Min.Y, F0, F1);
        // The guard on the stair's open side follows the treads (a stepped oak wall), a brass rail along its pitch.
        const bool bWest = W.Min.X < 0.f;
        const float XG = bWest ? W.Max.X : W.Min.X;
        const int32 N = FMath::CeilToInt((F1 - F0) / 17.5f);
        const float Run = (W.Min.Y - W.Max.Y) / N, Rise = (F1 - F0) / N;
        for (int32 K = 0; K < N; ++K)
        {
            const float YA = W.Max.Y + Run * K, YB = YA + Run;
            Box(Solid, TEXT("CI_Oak"), FVector(XG - 4.f, FMath::Min(YA, YB), F0), FVector(XG + 4.f, FMath::Max(YA, YB), F0 + Rise * (K + 1) + 92.f));
        }
        Clutter.Cylinder(TEXT("CI_Brass"), FVector(XG, W.Max.Y, F0 + 17.3f + 96.f), FVector(XG, W.Min.Y, F1 + 96.f), 3.f, 10, UVPlain());
        // Upstairs: balustrades along the well's open side and across its foot.
        Balustrade(Solid, FVector2D(XG, W.Min.Y), FVector2D(XG, W.Max.Y), F1);
        Balustrade(Solid, FVector2D(W.Min.X, W.Max.Y), FVector2D(W.Max.X, W.Max.Y), F1);
    }
    // ---- the ground floor lobby
    Kit.Floor(EKit::EntryRug, FVector2D(-73.f, SciInY - 70.f), F0, 90.f, false);
    Kit.Floor(EKit::LongRug, FVector2D(-73.f, 900.f), F0, 90.f, false);
    HangingSign(Clutter, TEXT("CI_SignHollis"), FVector(-73.f, 1150.f, C0 - 65.f), FVector(0, 1, 0), 320.f, 46.f, C0);
    ExitSign(Solid, Glow, FVector(-73.f, SciInY - 6.f, F0 + 236.f), FVector(0, -1, 0));
    DirectoryBoard(Solid, Clutter, FVector(-420.f, SciInY, F0 + 160.f), FVector(0, -1, 0), TEXT("CI_SciDirectory"));
    SciSpecimenCase(Solid, Clutter, Glow, FVector(SciXE - PartT * .5f, 1250.f, F0), FVector(-1, 0, 0), 220.f);
    for (const float X : { SciXW + PartT * .5f + 25.f })
    {
        Box(Solid, TEXT("CI_Oak"), FVector(X - 22.f, 1080.f, F0), FVector(X + 22.f, 1400.f, F0 + 44.f));   // oak benches
        Box(Solid, TEXT("CI_OakDark"), FVector(X - 24.f, 1078.f, F0 + 44.f), FVector(X + 24.f, 1402.f, F0 + 47.f));
    }
    // The Physics Club bake sale by the doors: the table, the sign, cookies, brownies, the cash box.
    Table(Solid, FVector(-420.f, 1250.f, F0), 180.f, 70.f, 76.f, true);
    for (int32 K = 0; K < 3; ++K)
    {
        const FVector P(-480.f + K * 60.f, 1250.f, F0 + 76.f);
        Clutter.Cylinder(TEXT("CI_Enamel"), P, P + FVector(0, 0, .8f), 13.f, 14, UVPlain());
        for (int32 C = 0; C < 5; ++C) Clutter.Cylinder(K == 1 ? TEXT("CI_SciBrownie") : TEXT("CI_SciCookie"), P + FVector(FMath::Cos(C * 1.3f) * 6.f, FMath::Sin(C * 1.3f) * 6.f, .8f + C * .6f), P + FVector(FMath::Cos(C * 1.3f) * 6.f, FMath::Sin(C * 1.3f) * 6.f, 1.4f + C * .6f), K == 1 ? 3.5f : 4.f, 8, UVPlain());
    }
    Box(Solid, TEXT("CI_Steel"), FVector(-345.f, 1235.f, F0 + 76.f), FVector(-320.f, 1265.f, F0 + 84.f));
    SciChart(Clutter, TEXT("CI_SciBakeSale"), FVector(-420.f, 1290.f, F0 + 120.f), FVector(0, 1, 0), 100.f, 70.f);   // facing the doors
    Box(Clutter, TEXT("CI_OakDark"), FVector(-470.f, 1291.f, F0 + 76.f), FVector(-468.f, 1293.f, F0 + 155.f));
    Box(Clutter, TEXT("CI_OakDark"), FVector(-372.f, 1291.f, F0 + 76.f), FVector(-370.f, 1293.f, F0 + 155.f));
    for (const float X : { -612.5f, 467.5f }) Radiator(Solid, FVector(X, SciInY, F0), FVector(0, -1, 0), 120.f);
    for (const float X : { -400.f, 260.f }) for (const float Y : { 600.f, 1050.f, 1400.f }) SciGlobe(Glow, Clutter, FVector2D(X, Y), C0);
    // ---- the upper lobby: the alumni banner over the stair heads, the case of old instruments, the open-house easel
    HangingSign(Clutter, TEXT("CI_SciBanner"), FVector(-73.f, 330.f, C1 - 70.f), FVector(0, 1, 0), 500.f, 60.f, C1);   // over the stair heads
    for (const float X : { -612.5f, -252.5f, 107.5f, 467.5f }) Radiator(Solid, FVector(X, SciInY, F1), FVector(0, -1, 0), 120.f);
    const FVector Case(-73.f, 1300.f, F1);
    Box(Solid, TEXT("CI_OakDark"), Case + FVector(-90.f, -35.f, 0.f), Case + FVector(90.f, 35.f, 85.f));
    Box(Solid, TEXT("CI_Glass"), Case + FVector(-88.f, -33.f, 85.f), Case + FVector(88.f, 33.f, 135.f));
    Clutter.Cylinder(TEXT("CI_Brass"), Case + FVector(-50.f, 0, 85.f), Case + FVector(-50.f, 0, 87.f), 8.f, 12, UVPlain());     // a brass microscope
    HouseCampusFallKit::Bar(Clutter, TEXT("CI_Brass"), Case + FVector(-50.f, 0, 87.f), Case + FVector(-46.f, 0, 118.f), 3.f, 3.f, FVector(1, 0, 0));
    Clutter.Cylinder(TEXT("CI_Brass"), Case + FVector(-46.f, 0, 108.f), Case + FVector(-40.f, 0, 126.f), 2.f, 10, UVPlain());
    Clutter.Cylinder(TEXT("CI_Brass"), Case + FVector(10.f, -10.f, 92.f), Case + FVector(70.f, 10.f, 104.f), 4.f, 12, UVPlain());   // a small telescope
    Clutter.Cylinder(TEXT("CI_OakDark"), Case + FVector(30.f, 0, 85.f), Case + FVector(30.f, 0, 92.f), 6.f, 10, UVPlain());
    for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Bar(Solid, TEXT("CI_OakDark"), FVector(240.f + S * 22.f, 1450.f, F1), FVector(240.f, 1440.f, F1 + 170.f), 3.f, 3.f, FVector(0, 1, 0));
    SciChart(Clutter, TEXT("CI_SciOpenHouse"), FVector(240.f, 1436.f, F1 + 130.f), FVector(0, -1, 0), 70.f, 90.f);
    Kit.Floor(EKit::PlantStand, FVector2D(-480.f, 1500.f), F1, 0.f, true);
    for (const float X : { -400.f, 260.f }) for (const float Y : { 600.f, 1050.f, 1400.f }) SciGlobe(Glow, Clutter, FVector2D(X, Y), C1);
}

/** Lockers along a corridor wall run U0..U1 (along X, the wall face at Y, facing Out), round the given gaps. */
void SciLockerRun(FHallMesh& Solid, FHallMesh& Clutter, float U0, float U1, float Y, float Out, float Z, std::initializer_list<FVector2D> Gaps)
{
    TArray<FVector2D> Sorted(Gaps);
    Sorted.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
    float U = U0;
    auto Run = [&](float A, float B) { const int32 N = FMath::FloorToInt((B - A) / 38.f); if (N > 0) Lockers(Solid, Clutter, FVector(A + (B - A - 38.f * N) * .5f, Y, Z), FVector(1, 0, 0), N, FVector(0, Out, 0)); };
    for (const FVector2D& G : Sorted) { if (G.X > U) Run(U, G.X); U = FMath::Max(U, G.Y); }
    if (U1 > U) Run(U, U1);
}

/** The corridor of floor K (lockers, the drink machine, the payphone, fountains, notices, globes, plaques). */
void SciCorridor(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, int32 K, float Z, float Ceil)
{
    using EKit = HouseCampusInteriors::EKit;
    const float North = SciYN - PartT * .5f, South = SciYS + PartT * .5f;
    if (K == 0)
    {
        // (Each gap is a doorway and its casings.)
        SciLockerRun(Solid, Clutter, -SciInX + 20.f, SciXW - 20.f, North, -1.f, Z, { FVector2D(-2025.f, -1913.f), FVector2D(-1110.f, -998.f) });
        SciLockerRun(Solid, Clutter, SciXE + 20.f, SciInX - 20.f, North, -1.f, Z, { FVector2D(890.f, 1002.f), FVector2D(1890.f, 2002.f) });
        SciLockerRun(Solid, Clutter, -SciInX + 20.f, -1520.f, South, 1.f, Z, { FVector2D(-1960.f, -1848.f) });
        SciLockerRun(Solid, Clutter, SciHX1 + 20.f, SciInX - 20.f, South, 1.f, Z, { FVector2D(1690.f, 1802.f) });
        VendingMachine(Solid, Glow, FVector(-1000.f, South + 42.f, Z), FVector(0, 1, 0), false);
        PayPhone(Solid, Clutter, FVector(-860.f, South, Z + 140.f), FVector(0, 1, 0));
        WaterFountain(Solid, FVector(-760.f, South, Z), FVector(0, 1, 0));
        BulletinBoard(Solid, Clutter, FVector(-400.f, South, Z + 150.f), FVector(0, 1, 0), 180.f, 100.f, TEXT("CI_NoticeBoard"));
        BulletinBoard(Solid, Clutter, FVector(400.f, South, Z + 150.f), FVector(0, 1, 0), 180.f, 100.f, TEXT("CI_RideBoard"));
        TrophyCase(Solid, Glow, FVector(900.f, South, Z), FVector(0, 1, 0), 240.f);
        WallPlaque(Clutter, TEXT("CI_SciFairPlaque"), FVector(900.f, South, Z + 245.f), FVector(0, 1, 0), 160.f, 26.f);
        for (const float X : { -1354.f, 1206.f }) WallPlaque(Clutter, TEXT("CI_Sci110"), FVector(X, South, Z + 245.f), FVector(0, 1, 0), 70.f, 30.f);
        WallPlaque(Clutter, TEXT("CI_Sci101"), FVector(-1150.f, North, Z + 170.f), FVector(0, -1, 0), 50.f, 22.f);
        WallPlaque(Clutter, TEXT("CI_Sci102"), FVector(1050.f, North, Z + 170.f), FVector(0, -1, 0), 50.f, 22.f);
        WallPlaque(Clutter, TEXT("CI_Sci112"), FVector(-1820.f, South, Z + 170.f), FVector(0, 1, 0), 50.f, 22.f);
        WallPlaque(Clutter, TEXT("CI_Sci114"), FVector(1840.f, South, Z + 170.f), FVector(0, 1, 0), 50.f, 22.f);
        for (float X = -1700.f; X < 1700.f; X += 900.f) Streamer(Clutter, FVector(X, North - 6.f, Ceil - 10.f), FVector(X + 900.f, South + 6.f, Ceil - 10.f), int32((X + 1700.f) / 900.f) % 2 == 0);
    }
    else
    {
        SciLockerRun(Solid, Clutter, -SciInX + 20.f, SciXW - 20.f, North, -1.f, Z, { FVector2D(-1110.f, -998.f) });
        SciLockerRun(Solid, Clutter, SciXE + 20.f, SciLX - 20.f, North, -1.f, Z, { FVector2D(890.f, 1002.f) });
        SciLockerRun(Solid, Clutter, -SciInX + 20.f, -1520.f, South, 1.f, Z, { FVector2D(-1960.f, -1848.f) });
        WaterFountain(Solid, FVector(-760.f, South, Z), FVector(0, 1, 0));
        BulletinBoard(Solid, Clutter, FVector(0.f, South, Z + 150.f), FVector(0, 1, 0), 220.f, 100.f, TEXT("CI_NoticeBoard"));
        SciChart(Clutter, TEXT("CI_PeriodicTable"), FVector(-600.f, South, Z + 180.f), FVector(0, 1, 0), 260.f, 150.f);
        SciCabinet(Solid, Clutter, FVector(600.f, South + 22.f, Z), FVector(0, 1, 0), 160.f, 44.f, 210.f, 204);
        WallPlaque(Clutter, TEXT("CI_Sci204"), FVector(-1150.f, North, Z + 170.f), FVector(0, -1, 0), 50.f, 22.f);
        WallPlaque(Clutter, TEXT("CI_Sci210"), FVector(1050.f, North, Z + 170.f), FVector(0, -1, 0), 50.f, 22.f);
        WallPlaque(Clutter, TEXT("CI_Sci212"), FVector(1850.f, North, Z + 170.f), FVector(0, -1, 0), 50.f, 22.f);
        WallPlaque(Clutter, TEXT("CI_Sci206"), FVector(-1820.f, South, Z + 170.f), FVector(0, 1, 0), 50.f, 22.f);
        WallPlaque(Clutter, TEXT("CI_Sci214"), FVector(1690.f, South, Z + 170.f), FVector(0, 1, 0), 50.f, 22.f);
    }
    Kit.Floor(EKit::FireExtinguisher, FVector2D(-1530.f + (K ? 0.f : 40.f), South + 12.f), Z + 80.f, 90.f, false);
    WallClock(Clutter, FVector(K ? -300.f : 0.f, South, Z + 258.f), FVector(0, 1, 0), 16.f);
    for (float X = -2000.f; X <= 2000.f; X += 500.f) SciGlobe(Glow, Clutter, FVector2D(X, SciCorY), Ceil);
    for (const float S : { -1.f, 1.f }) Radiator(Solid, FVector(S * SciInX, 87.5f, Z), FVector(-S, 0, 0), 120.f);
}

/** The first floor's front rooms: the optics lab 204, seminar room 210, the faculty lounge 212. */
void SciUpstairs(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, float Z, float Ceil)
{
    using EKit = HouseCampusInteriors::EKit;
    // ---- the optics lab: two laser benches (a rail of lens holders, the beam drawn in red), the spectrum chart, the board
    for (const float Y : { 700.f, 1150.f })
    {
        Table(Solid, FVector(-1550.f, Y, Z), 520.f, 90.f, 86.f, true);
        const float Top = Z + 86.f;
        Box(Solid, TEXT("CI_CrtBezel"), FVector(-1790.f, Y - 4.f, Top), FVector(-1310.f, Y + 4.f, Top + 4.f));                 // the optical rail
        Box(Solid, TEXT("CI_SciLaser"), FVector(-1800.f, Y - 6.f, Top + 4.f), FVector(-1760.f, Y + 6.f, Top + 16.f));          // the He-Ne tube
        for (const float X : { -1660.f, -1540.f, -1420.f })
        {
            Clutter.Cylinder(TEXT("CI_Steel"), FVector(X, Y, Top + 4.f), FVector(X, Y, Top + 18.f), .8f, 6, UVPlain());
            Clutter.Cylinder(TEXT("CI_Glass"), FVector(X - .6f, Y, Top + 20.f), FVector(X + .6f, Y, Top + 20.f), 5.f, 12, UVPlain());
        }
        Box(Solid, TEXT("CI_Enamel"), FVector(-1322.f, Y - 12.f, Top + 4.f), FVector(-1318.f, Y + 12.f, Top + 34.f));            // the screen
        Glow.Box(TEXT("CI_SciBeam"), FTransform(FVector(-1540.f, Y, Top + 20.f)), FVector(220.f, .15f, .15f), UVPlain());
        Glow.Cylinder(TEXT("CI_SciBeam"), FVector(-1317.5f, Y, Top + 20.f), FVector(-1317.f, Y, Top + 20.f), 1.2f, 8, UVPlain());
        for (float DX = -200.f; DX <= 200.f; DX += 100.f) Kit.Floor(EKit::BarStool, FVector2D(-1550.f + DX, Y - 80.f), Z, 90.f, true);
    }
    const float East = SciXW - PartT * .5f;
    Chalkboard(Solid, Clutter, FVector(East, 900.f, Z + 150.f), FVector(-1, 0, 0), 400.f, 120.f, TEXT("CI_ChalkOptics"));
    WallClock(Clutter, FVector(East, 900.f, Z + 258.f), FVector(-1, 0, 0), 15.f);
    SciChart(Clutter, TEXT("CI_SciSpectrum"), FVector(East, 1350.f, Z + 190.f), FVector(-1, 0, 0), 280.f, 90.f);
    Terminal(Solid, Glow, FVector(-2150.f, 450.f, Z + 86.f), FVector(1, 0, 0));
    Table(Solid, FVector(-2170.f, 450.f, Z), 160.f, 70.f, 86.f, false);
    for (const float X : { -2052.5f, -1692.5f, -1332.5f, -972.5f }) Radiator(Solid, FVector(X, SciInY, Z), FVector(0, -1, 0), 120.f);
    for (float X = -2000.f; X <= -1000.f; X += 500.f) for (const float Y : { 500.f, 900.f, 1300.f }) Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), true);
    // ---- seminar room 210
    const FBox2D Sem = Rect(SciXE + PartT * .5f, SciLX - PartT * .5f, SciYN + PartT * .5f, SciInY);
    RoomSeminar(Solid, Clutter, Kit, Sem, Z, 2, TEXT("CI_ChalkSeminar"), 210);
    for (const float X : { 827.5f, 1187.5f }) Radiator(Solid, FVector(X, SciInY, Z), FVector(0, -1, 0), 120.f);
    for (const float Y : { 600.f, 1000.f, 1400.f }) SciGlobe(Glow, Clutter, FVector2D(1010.f, Y), Ceil);
    // ---- the faculty lounge 212, set out for the alumni: the coffee urn, cups, a plate of doughnuts
    const FBox2D Lounge = Rect(SciLX + PartT * .5f, SciInX, SciYN + PartT * .5f, SciInY);
    RoomLounge(Solid, Glow, Kit, Lounge, Z, 3, 212);
    Table(Solid, FVector(1700.f, 1450.f, Z), 200.f, 70.f, 76.f, true);
    Solid.Cylinder(TEXT("CI_Steel"), FVector(1630.f, 1455.f, Z + 76.f), FVector(1630.f, 1455.f, Z + 118.f), 13.f, 14, UVPlain());
    Clutter.Cylinder(TEXT("CI_Epoxy"), FVector(1630.f, 1441.f, Z + 84.f), FVector(1630.f, 1437.f, Z + 84.f), 1.5f, 8, UVPlain());
    for (int32 I = 0; I < 6; ++I) Kit.Floor(EKit::PaperCup, FVector2D(1680.f + (I % 3) * 14.f, 1440.f + (I / 3) * 14.f), Z + 76.f, 0.f, true);
    Clutter.Cylinder(TEXT("CI_Enamel"), FVector(1760.f, 1450.f, Z + 76.f), FVector(1760.f, 1450.f, Z + 76.8f), 14.f, 14, UVPlain());
    for (int32 C = 0; C < 4; ++C) Clutter.Cylinder(TEXT("CI_SciCookie"), FVector(1760.f + (C % 2) * 9.f - 4.f, 1450.f + (C / 2) * 9.f - 4.f, Z + 76.8f), FVector(1760.f + (C % 2) * 9.f - 4.f, 1450.f + (C / 2) * 9.f - 4.f, Z + 80.f), 4.5f, 10, UVPlain());
    SciChart(Clutter, TEXT("CI_SciAlumniCoffee"), FVector(1700.f, SciInY, Z + 160.f), FVector(0, -1, 0), 90.f, 50.f);
    for (const float X : { 1547.5f, 1907.5f }) Radiator(Solid, FVector(X, SciInY, Z), FVector(0, -1, 0), 120.f);
    for (const float Y : { 600.f, 1000.f, 1400.f }) SciGlobe(Glow, Clutter, FVector2D(1820.f, Y), Ceil);
}

// ------------------------------------------------------------------ the building
void BuildScience(int32 Seed, float Height, FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit)
{
    const float F0 = HouseCampusInteriors::FloorTop(Height, 0), F1 = HouseCampusInteriors::FloorTop(Height, 1), Top = HouseCampusInteriors::OpenTop(Seed);
    const float C0 = F1 - 30.f, C1 = Top;
    const FBox2D In = Rect(-SciInX, SciInX, -SciInY, SciInY);
    const FBox2D Corridor = Rect(-SciInX, SciInX, SciYS, SciYN), Lobby = Rect(SciXW, SciXE, SciYN, SciInY), Hall = Rect(SciHX0, SciHX1, -SciInY, SciYS);
    TArray<FBox2D> Wells; SciStairWells(Wells);
    // Shell: plaster on the outer walls; terrazzo in the corridors and lobby, vinyl tile in the rooms, carpet in the hall;
    // the first floor's slab open over the lecture hall and the stairs; plaster ceilings.
    HouseCampusInteriors::OpenWalls(Solid, TEXT("CI_Plaster"), Seed, OuterSkin, WallT, UVPlain());
    FloorSlab(Solid, In, F0, 70.f, { Corridor, Lobby, Hall }, TEXT("CI_VCT"), TEXT("CI_Plaster"), FHallUV::World(61.f));
    FloorSlab(Solid, Corridor, F0, 70.f, {}, TEXT("CI_Terrazzo"), TEXT("CI_Plaster"), FHallUV::World(220.f));
    FloorSlab(Solid, Lobby, F0, 70.f, {}, TEXT("CI_Terrazzo"), TEXT("CI_Plaster"), FHallUV::World(220.f));
    FloorSlab(Solid, Hall, F0, 70.f, {}, TEXT("CI_Carpet"), TEXT("CI_Plaster"), FHallUV::World(180.f));
    TArray<FBox2D> Holes(Wells); Holes.Add(Hall); Holes.Add(Corridor); Holes.Add(Lobby);
    FloorSlab(Solid, In, F1, 30.f, Holes, TEXT("CI_VCT"), TEXT("CI_Plaster"), FHallUV::World(61.f));
    FloorSlab(Solid, Corridor, F1, 30.f, {}, TEXT("CI_Terrazzo"), TEXT("CI_Plaster"), FHallUV::World(220.f));
    FloorSlab(Solid, Lobby, F1, 30.f, Wells, TEXT("CI_Terrazzo"), TEXT("CI_Plaster"), FHallUV::World(220.f));
    Box(Solid, TEXT("CI_Plaster"), FVector(In.Min, C1 - .5f), FVector(In.Max, C1), UVPlain());
    // Oak wainscot on the outer walls of both floors (not on the lecture hall's back wall upstairs: the hall has no first
    // floor), and oak stools and casings at every real window.
    for (int32 K = 0; K < 2; ++K)
    {
        const float Z = K == 0 ? F0 : F1;
        Wainscot(Solid, true, In.Max.Y, -1.f, In.Min.X, In.Max.X, Z, K == 0 ? TArray<FVector2D>{ FVector2D(-129.f, 129.f) } : TArray<FVector2D>{}, 60.f);
        if (K == 0) Wainscot(Solid, true, In.Min.Y, 1.f, In.Min.X, In.Max.X, Z, {}, 60.f);
        else { Wainscot(Solid, true, In.Min.Y, 1.f, In.Min.X, SciHX0, Z, {}, 60.f); Wainscot(Solid, true, In.Min.Y, 1.f, SciHX1, In.Max.X, Z, {}, 60.f); }
        Wainscot(Solid, false, In.Min.X, 1.f, In.Min.Y, In.Max.Y, Z, {}, 60.f);
        Wainscot(Solid, false, In.Max.X, -1.f, In.Min.Y, In.Max.Y, Z, {}, 60.f);
        for (int32 Side = 0; Side < 4; ++Side)
        {
            TArray<FVector2D> Cols;
            HouseCampusInteriors::WindowColumns(Seed, Side, Cols);
            const float Sill = HouseCampusInteriors::WindowSill(Height, K), Head = Sill + HouseCampusInteriors::WindowHeight;
            for (const FVector2D& C : Cols)
            {
                if (HouseCampusInteriors::WindowReplaced(Seed, Side, K, C.X, C.Y) || !HouseCampusInteriors::WindowOpen(Seed, Side, K, C.X, C.Y)) continue;
                auto Trim = [&](float U0, float U1, float Z0, float Z1, float D0, float D1, FName Key)
                {
                    switch (Side)
                    {
                    case 0: Box(Solid, Key, FVector(U0, In.Max.Y - D1, Z0), FVector(U1, In.Max.Y - D0, Z1)); break;
                    case 1: Box(Solid, Key, FVector(U0, In.Min.Y + D0, Z0), FVector(U1, In.Min.Y + D1, Z1)); break;
                    case 2: Box(Solid, Key, FVector(In.Min.X + D0, U0, Z0), FVector(In.Min.X + D1, U1, Z1)); break;
                    default: Box(Solid, Key, FVector(In.Max.X - D1, U0, Z0), FVector(In.Max.X - D0, U1, Z1)); break;
                    }
                };
                Trim(C.X - 6.f, C.Y + 6.f, Sill - 4.f, Sill, -6.f, 8.f, TEXT("CI_Oak"));
                Trim(C.X - 9.f, C.X, Sill, Head + 9.f, 0.f, 2.f, TEXT("CI_OakDark"));
                Trim(C.Y, C.Y + 9.f, Sill, Head + 9.f, 0.f, 2.f, TEXT("CI_OakDark"));
                Trim(C.X - 9.f, C.Y + 9.f, Head, Head + 9.f, 0.f, 2.f, TEXT("CI_OakDark"));
            }
        }
    }
    // ---- partitions. Ground floor walls run up through the slab; the lecture hall's walls run the height of both floors.
    const FName Wall = TEXT("CI_PlasterWall");
    Partition(Solid, Wall, false, SciYN, -SciInX, SciXW + PartT * .5f, F0, F1, { FVector2D(-2015.f, -1923.f), FVector2D(-1100.f, -1008.f) }, 213.f);
    Partition(Solid, Wall, false, SciYN, SciXE - PartT * .5f, SciInX, F0, F1, { FVector2D(900.f, 992.f), FVector2D(1900.f, 1992.f) }, 213.f);
    Partition(Solid, Wall, false, SciYS, -SciInX, SciHX0 + PartT * .5f, F0, F1, { FVector2D(-1950.f, -1858.f) }, 213.f);
    Partition(Solid, Wall, false, SciYS, SciHX0 - PartT * .5f, SciHX1 + PartT * .5f, F0, C1, { FVector2D(-1400.f, -1308.f), FVector2D(1160.f, 1252.f) }, 213.f);
    Partition(Solid, Wall, false, SciYS, SciHX1 - PartT * .5f, SciInX, F0, F1, { FVector2D(1700.f, 1792.f) }, 213.f);
    Partition(Solid, Wall, true, SciXW, SciYN, SciInY, F0, F1, {});
    Partition(Solid, Wall, true, SciXE, SciYN, SciInY, F0, F1, {});
    Partition(Solid, Wall, true, SciHX0, -SciInY, SciYS, F0, C1, {});
    Partition(Solid, Wall, true, SciHX1, -SciInY, SciYS, F0, C1, {});
    // First floor.
    Partition(Solid, Wall, false, SciYN, -SciInX, SciXW + PartT * .5f, F1, C1, { FVector2D(-1100.f, -1008.f) }, 213.f);
    Partition(Solid, Wall, false, SciYN, SciXE - PartT * .5f, SciInX, F1, C1, { FVector2D(900.f, 992.f), FVector2D(1700.f, 1792.f) }, 213.f);
    Partition(Solid, Wall, false, SciYS, -SciInX, SciHX0 + PartT * .5f, F1, C1, { FVector2D(-1950.f, -1858.f) }, 213.f);
    Partition(Solid, Wall, false, SciYS, SciHX1 - PartT * .5f, SciInX, F1, C1, { FVector2D(1600.f, 1692.f) }, 213.f);
    Partition(Solid, Wall, true, SciXW, SciYN, SciInY, F1, C1, {});
    Partition(Solid, Wall, true, SciXE, SciYN, SciInY, F1, C1, {});
    Partition(Solid, Wall, true, SciLX, SciYN, SciInY, F1, C1, {});
    Wainscot(Solid, true, SciYS + PartT * .5f, 1.f, SciHX0, SciHX1, F1, {});   // the corridor side of the lecture hall's wall, upstairs
    // ---- rooms
    SciLobby(Solid, Clutter, Glow, Kit, F0, F1, C0, C1);
    SciChemLab(Solid, Clutter, Glow, Kit, F0, C0);
    SciPhysicsLab(Solid, Clutter, Glow, Kit, F0, C0);
    SciLectureHall(Solid, Clutter, Glow, Kit, F0, C1);
    SciCorridor(Solid, Clutter, Glow, Kit, 0, F0, C0);
    SciCorridor(Solid, Clutter, Glow, Kit, 1, F1, C1);
    SciUpstairs(Solid, Clutter, Glow, Kit, F1, C1);
}

void SciLightAnchors(int32 Seed, float Height, TArray<FVector>& Out)
{
    const float Z0 = HouseCampusInteriors::FloorTop(Height, 0) + 300.f, Z1 = HouseCampusInteriors::FloorTop(Height, 1) + 300.f;
    for (const float X : { -1800.f, -700.f, 500.f, 1700.f }) { Out.Add(FVector(X, SciCorY, Z0)); Out.Add(FVector(X, SciCorY, Z1)); }
    for (const FVector2D& P : { FVector2D(-73.f, 1250.f), FVector2D(-73.f, 650.f), FVector2D(-1550.f, 700.f), FVector2D(-1550.f, 1250.f), FVector2D(1450.f, 750.f), FVector2D(1450.f, 1250.f) })
        Out.Add(FVector(P, Z0));
    // The lecture hall (ground floor, two storeys): over the well and over the tiers.
    for (const FVector& P : { FVector(-650.f, -350.f, 0.f), FVector(500.f, -350.f, 0.f), FVector(-700.f, -1000.f, 150.f), FVector(600.f, -1000.f, 150.f), FVector(-73.f, -1350.f, 250.f) })
        Out.Add(FVector(P.X, P.Y, Z0 + P.Z));
    for (const FVector2D& P : { FVector2D(-73.f, 1200.f), FVector2D(-1550.f, 900.f), FVector2D(1010.f, 900.f), FVector2D(1820.f, 900.f) })
        Out.Add(FVector(P, Z1));
}
void SciOpenings(int32 Seed, float Height, TArray<HouseCampusInteriors::FOpening>& Out)
{
    const float F0 = HouseCampusInteriors::FloorTop(Height, 0);
    Out.Add({ 0, -120.f, 120.f, F0, F0 + 213.f });   // the main doors into the lobby
}
void SciDoors(int32 Seed, float Height, TArray<FDoorSpec>& Out)
{
    const FBox2D B = HouseCampusInteriors::CanonRect(Seed);
    TArray<HouseCampusInteriors::FOpening> Openings;
    SciOpenings(Seed, Height, Openings);
    MainDoorPair(TEXT("SCI"), B, Openings[0], Out);
    const float F0 = HouseCampusInteriors::FloorTop(Height, 0), F1 = HouseCampusInteriors::FloorTop(Height, 1);
    const uint8 Glazed = AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::KickPlate | AHouseBlackHallDoor::Closer;
    const uint8 Steel = AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::KickPlate;
    // Ground floor: the labs and the hall stand open for the open house; the prep room and the office are locked.
    InnerDoor(Out, TEXT("Lab101West"), false, SciYN, -2015.f, -1923.f, F0, 1.f, EHallDoorStyle::French, Glazed, FString(), true);
    InnerDoor(Out, TEXT("Lab101"), false, SciYN, -1100.f, -1008.f, F0, 1.f, EHallDoorStyle::French, Glazed, FString(), true);
    InnerDoor(Out, TEXT("Lab102"), false, SciYN, 900.f, 992.f, F0, 1.f, EHallDoorStyle::French, Glazed, FString(), true);
    InnerDoor(Out, TEXT("Lab102East"), false, SciYN, 1900.f, 1992.f, F0, 1.f, EHallDoorStyle::French, Glazed, FString(), true);
    InnerDoor(Out, TEXT("HallWest"), false, SciYS, -1400.f, -1308.f, F0, -1.f, EHallDoorStyle::French, Glazed, FString(), true);
    InnerDoor(Out, TEXT("HallEast"), false, SciYS, 1160.f, 1252.f, F0, -1.f, EHallDoorStyle::French, Glazed, FString(), true);
    InnerDoor(Out, TEXT("PrepRoom"), false, SciYS, -1950.f, -1858.f, F0, -1.f, EHallDoorStyle::Equipment, Steel, TEXT("PREP ROOM 112 - AUTHORIZED PERSONNEL ONLY"));
    InnerDoor(Out, TEXT("Office"), false, SciYS, 1700.f, 1792.f, F0, -1.f, EHallDoorStyle::French, Glazed, TEXT("DEPARTMENT OFFICE 114 - CLOSED SATURDAY"));
    // First floor.
    InnerDoor(Out, TEXT("Lab204"), false, SciYN, -1100.f, -1008.f, F1, 1.f, EHallDoorStyle::French, Glazed, FString(), true);
    InnerDoor(Out, TEXT("Seminar210"), false, SciYN, 900.f, 992.f, F1, 1.f, EHallDoorStyle::French, Glazed, FString(), true);
    InnerDoor(Out, TEXT("Lounge212"), false, SciYN, 1700.f, 1792.f, F1, 1.f, EHallDoorStyle::French, Glazed, FString(), true);
    InnerDoor(Out, TEXT("Darkroom206"), false, SciYS, -1950.f, -1858.f, F1, -1.f, EHallDoorStyle::Equipment, Steel, TEXT("DARKROOM 206 - IN USE, DO NOT OPEN"));
    InnerDoor(Out, TEXT("Offices214"), false, SciYS, 1600.f, 1692.f, F1, -1.f, EHallDoorStyle::French, Glazed, TEXT("FACULTY OFFICES 214 - OFFICE HOURS MON-FRI"));
}
bool SciWindowOpen(int32 Seed, int32 Side, int32 K, float U0, float U1)
{
    // The back rooms (the prep room, the office, the darkroom, the faculty offices) are locked: their windows stay
    // interior-mapped. The lecture hall's ground floor windows are behind the top rows (drapes inside); its upper ones light it.
    const float U = (U0 + U1) * .5f;
    if (Side == 1) return K == 1 && U > SciHX0 && U < SciHX1;
    if (Side >= 2) return U > SciYS;
    return true;
}

/** Lines of text painted down a board (a directory or a notice): Lines at LineH pixels each. */
void SciPaintLines(UHouseBlackHallSubsystem* Look, FName Key, int32 W, int32 H, const FLinearColor& Bg, const FLinearColor& Ink, std::initializer_list<const TCHAR*> Lines, float LineH, const FLinearColor& Emission)
{
    FHallImage Img; Img.Init(W, H, Bg);
    FHallTextStyle S; S.Height = LineH * .62f; S.Stroke = FMath::Max(1.4f, S.Height * .12f); S.Spacing = .18f;
    float Y = LineH;
    for (const TCHAR* L : Lines)
    {
        FHallTextStyle Fit = S;
        Fit.WidthScale = FMath::Min(1.f, (W - 24.f) / FMath::Max(1.f, HallTextWidth(L, S)));
        HallDrawText(Img, L, FVector2D(12.f, Y), Fit, Ink);
        Y += LineH;
    }
    UTexture2D* Tex = HallMakeTexture(Img, true, false, *Key.ToString(), false);
    if (Tex) Look->Textures.Add(Key, Tex);
    HallMakeTextured(Look, Key, Tex, nullptr, .5f, FLinearColor::White, 0.f, 0.f, Emission);
}

void ScienceLook(UHouseBlackHallSubsystem* Look)
{
    if (!Look || Look->Materials.Contains(TEXT("CI_PeriodicTable"))) return;
    {
        // The periodic table (to 103, as 1986 charts printed it): the main block, the two f-rows below; cells tinted by family.
        static const TCHAR* const Sym[] = { TEXT("H"), TEXT("HE"), TEXT("LI"), TEXT("BE"), TEXT("B"), TEXT("C"), TEXT("N"), TEXT("O"), TEXT("F"), TEXT("NE"),
            TEXT("NA"), TEXT("MG"), TEXT("AL"), TEXT("SI"), TEXT("P"), TEXT("S"), TEXT("CL"), TEXT("AR"), TEXT("K"), TEXT("CA"), TEXT("SC"), TEXT("TI"), TEXT("V"),
            TEXT("CR"), TEXT("MN"), TEXT("FE"), TEXT("CO"), TEXT("NI"), TEXT("CU"), TEXT("ZN"), TEXT("GA"), TEXT("GE"), TEXT("AS"), TEXT("SE"), TEXT("BR"), TEXT("KR"),
            TEXT("RB"), TEXT("SR"), TEXT("Y"), TEXT("ZR"), TEXT("NB"), TEXT("MO"), TEXT("TC"), TEXT("RU"), TEXT("RH"), TEXT("PD"), TEXT("AG"), TEXT("CD"), TEXT("IN"),
            TEXT("SN"), TEXT("SB"), TEXT("TE"), TEXT("I"), TEXT("XE"), TEXT("CS"), TEXT("BA"), TEXT("LA"), TEXT("CE"), TEXT("PR"), TEXT("ND"), TEXT("PM"), TEXT("SM"),
            TEXT("EU"), TEXT("GD"), TEXT("TB"), TEXT("DY"), TEXT("HO"), TEXT("ER"), TEXT("TM"), TEXT("YB"), TEXT("LU"), TEXT("HF"), TEXT("TA"), TEXT("W"), TEXT("RE"),
            TEXT("OS"), TEXT("IR"), TEXT("PT"), TEXT("AU"), TEXT("HG"), TEXT("TL"), TEXT("PB"), TEXT("BI"), TEXT("PO"), TEXT("AT"), TEXT("RN"), TEXT("FR"), TEXT("RA"),
            TEXT("AC"), TEXT("TH"), TEXT("PA"), TEXT("U"), TEXT("NP"), TEXT("PU"), TEXT("AM"), TEXT("CM"), TEXT("BK"), TEXT("CF"), TEXT("ES"), TEXT("FM"), TEXT("MD"),
            TEXT("NO"), TEXT("LR") };
        constexpr int32 Cell = 54, PadX = 26, PadY = 60;
        FHallImage Img; Img.Init(PadX * 2 + Cell * 18, PadY + Cell * 10 + 20, FLinearColor(.86f, .84f, .78f));
        FHallTextStyle Title; Title.Height = 30.f; Title.Stroke = 4.f; Title.Spacing = .2f;
        HallDrawText(Img, TEXT("PERIODIC TABLE OF THE ELEMENTS"), FVector2D((Img.W - HallTextWidth(TEXT("PERIODIC TABLE OF THE ELEMENTS"), Title)) * .5f, 44.f), Title, FLinearColor(.1f, .1f, .14f));
        FHallTextStyle S; S.Height = 17.f; S.Stroke = 2.6f; S.Spacing = .15f;
        for (int32 Z = 1; Z <= 103; ++Z)
        {
            int32 Row = 0, Col = 0;   // 0-based; rows 7 and 8 are the f-rows (drawn a little apart)
            if (Z == 1) { Row = 0; Col = 0; } else if (Z == 2) { Row = 0; Col = 17; }
            else if (Z <= 10) { Row = 1; Col = Z <= 4 ? Z - 3 : Z - 5 + 12; }
            else if (Z <= 18) { Row = 2; Col = Z <= 12 ? Z - 11 : Z - 13 + 12; }
            else if (Z <= 36) { Row = 3; Col = Z - 19; }
            else if (Z <= 54) { Row = 4; Col = Z - 37; }
            else if (Z <= 57) { Row = 5; Col = Z - 55; }
            else if (Z <= 71) { Row = 7; Col = Z - 58 + 3; }
            else if (Z <= 86) { Row = 5; Col = Z - 72 + 3; }
            else if (Z <= 89) { Row = 6; Col = Z - 87; }
            else { Row = 8; Col = Z - 90 + 3; }
            const float X0 = PadX + Col * Cell, Y0 = PadY + Row * Cell + (Row >= 7 ? 14 : 0);
            FLinearColor C(.75f, .72f, .55f);
            if (Row >= 7) C = FLinearColor(.78f, .58f, .6f);
            else if (Col == 0 && Z != 1) C = FLinearColor(.82f, .5f, .4f);
            else if (Col == 1) C = FLinearColor(.85f, .68f, .4f);
            else if (Col == 17) C = FLinearColor(.62f, .55f, .78f);
            else if (Col >= 12 || Z == 1) C = FLinearColor(.55f, .72f, .62f);
            Img.Rect(FVector2D(X0 + 1, Y0 + 1), FVector2D(X0 + Cell - 1, Y0 + Cell - 1), FLinearColor(.2f, .2f, .22f));
            Img.Rect(FVector2D(X0 + 2.5f, Y0 + 2.5f), FVector2D(X0 + Cell - 2.5f, Y0 + Cell - 2.5f), C);
            const FString T = Sym[Z - 1];
            HallDrawText(Img, T, FVector2D(X0 + (Cell - HallTextWidth(T, S)) * .5f, Y0 + Cell * .5f + 12.f), S, FLinearColor(.08f, .08f, .1f));
            FHallTextStyle NS; NS.Height = 8.f; NS.Stroke = 1.3f; NS.Spacing = .15f;
            HallDrawText(Img, FString::FromInt(Z), FVector2D(X0 + 5.f, Y0 + 13.f), NS, FLinearColor(.1f, .1f, .12f));
        }
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_PeriodicTable"), false);
        if (Tex) Look->Textures.Add(TEXT("CI_PeriodicTable"), Tex);
        HallMakeTextured(Look, TEXT("CI_PeriodicTable"), Tex, nullptr, .55f);
    }
    {
        // The demonstration slide rule: its scales and the cursor.
        FHallImage Img; Img.Init(1024, 72, FLinearColor(.86f, .84f, .74f));
        for (int32 Scale = 0; Scale < 3; ++Scale)
        {
            const float Y0 = 6.f + Scale * 22.f;
            for (int32 N = 10; N <= 100; ++N)
            {
                const float X = 24.f + 976.f * FMath::LogX(10.f, N / 10.f);
                const float Len = N % 10 == 0 ? 10.f : N % 5 == 0 ? 7.f : 4.f;
                Img.Rect(FVector2D(X, Y0), FVector2D(X + 1.6f, Y0 + Len), FLinearColor(.08f, .08f, .08f));
            }
        }
        Img.Rect(FVector2D(600, 0), FVector2D(604, 72), FLinearColor(.6f, .1f, .1f));
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_SciSlideRule"), false);
        if (Tex) Look->Textures.Add(TEXT("CI_SciSlideRule"), Tex);
        HallMakeTextured(Look, TEXT("CI_SciSlideRule"), Tex, nullptr, .5f);
    }
    {
        // The visible spectrum, 400 to 700 nm.
        FHallImage Img; Img.Init(512, 160, FLinearColor(.08f, .08f, .1f));
        for (int32 X = 0; X < 480; ++X)
        {
            const float T = X / 480.f;
            const FLinearColor C = FLinearColor::MakeFromHSV8(uint8(FMath::Lerp(200.f, 0.f, T)), 255, 230);
            Img.Rect(FVector2D(16 + X, 40), FVector2D(17 + X, 110), C);
        }
        FHallTextStyle S; S.Height = 16.f; S.Stroke = 2.4f; S.Spacing = .2f;
        HallDrawText(Img, TEXT("THE VISIBLE SPECTRUM"), FVector2D(130.f, 30.f), S, FLinearColor(.85f, .85f, .8f));
        int32 K = 0;
        for (const TCHAR* L : { TEXT("400"), TEXT("500"), TEXT("600"), TEXT("700") }) HallDrawText(Img, L, FVector2D(16.f + 152.f * K++, 140.f), S, FLinearColor(.85f, .85f, .8f));
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_SciSpectrum"), false);
        if (Tex) Look->Textures.Add(TEXT("CI_SciSpectrum"), Tex);
        HallMakeTextured(Look, TEXT("CI_SciSpectrum"), Tex, nullptr, .5f);
    }
    const FLinearColor Board(.06f, .13f, .09f), Chalk(.8f, .82f, .78f);
    PaintText(Look, TEXT("CI_SciEquations"), 512, 160, Board, Chalk, { { TEXT("E = MC2     F = GMM / R2"), .17f }, { TEXT("MIDTERM THURS - BRING SLIDE RULE"), .11f } }, false);
    PaintText(Look, TEXT("CI_ChalkOptics"), 512, 160, Board, Chalk, { { TEXT("N1 SIN A = N2 SIN B"), .18f }, { TEXT("LAB 4 - THIN LENSES"), .14f } }, false);
    SciPaintLines(Look, TEXT("CI_SciDirectory"), 256, 360, FLinearColor(.05f, .05f, .06f), FLinearColor(.86f, .84f, .78f), {
        TEXT("HOLLIS HALL"), TEXT(""), TEXT("101  CHEMISTRY LAB"), TEXT("102  PHYSICS LAB"), TEXT("110  LECTURE HALL"), TEXT("112  PREP ROOM"),
        TEXT("114  DEPT OFFICE"), TEXT(""), TEXT("204  OPTICS LAB"), TEXT("206  DARKROOM"), TEXT("210  SEMINAR"), TEXT("212  FACULTY LOUNGE"), TEXT("214  OFFICES") }, 26.f);
    PaintText(Look, TEXT("CI_SciBakeSale"), 256, 180, FLinearColor(.85f, .72f, .2f), FLinearColor(.22f, .05f, .32f), { { TEXT("PHYSICS CLUB"), .14f }, { TEXT("BAKE SALE"), .2f }, { TEXT("COOKIES 25 CENTS"), .1f } }, true);
    PaintText(Look, TEXT("CI_SciBanner"), 1024, 120, FLinearColor(.85f, .83f, .78f), FLinearColor(.22f, .05f, .32f), { { TEXT("WELCOME ALUMNI - SCIENCE OPEN HOUSE 10 TO 2"), .38f } }, false);
    PaintText(Look, TEXT("CI_SciOpenHouse"), 160, 200, FLinearColor(.85f, .83f, .78f), FLinearColor(.2f, .2f, .25f), { { TEXT("OPEN HOUSE"), .14f }, { TEXT("LABS 101 102"), .1f }, { TEXT("204 UPSTAIRS"), .1f } }, true);
    PaintText(Look, TEXT("CI_SciAlumniCoffee"), 256, 140, FLinearColor(.85f, .83f, .78f), FLinearColor(.22f, .05f, .32f), { { TEXT("ALUMNI"), .22f }, { TEXT("COFFEE - HELP YOURSELF"), .12f } }, true);
    PaintText(Look, TEXT("CI_SciShowerSign"), 256, 110, FLinearColor(.1f, .45f, .2f), FLinearColor(.95f, .95f, .9f), { { TEXT("SAFETY"), .3f }, { TEXT("SHOWER"), .3f } }, true);
    PaintText(Look, TEXT("CI_SciHallRules"), 256, 170, FLinearColor(.85f, .83f, .78f), FLinearColor(.12f, .12f, .14f), { { TEXT("LECTURE HALL 110"), .13f }, { TEXT("NO FOOD OR DRINK"), .11f }, { TEXT("NO SMOKING"), .11f } }, true);
    PaintText(Look, TEXT("CI_SciFairPlaque"), 512, 84, FLinearColor(.08f, .06f, .04f), FLinearColor(.78f, .58f, .18f), { { TEXT("SCIENCE FAIR HONORS"), .42f } }, true);
    PaintText(Look, TEXT("CI_SciProgramme"), 128, 160, FLinearColor(.22f, .05f, .32f), FLinearColor(.85f, .65f, .15f), { { TEXT("HOMECOMING"), .12f }, { TEXT("86"), .3f } }, false);
    PaintText(Look, TEXT("CI_SignHollis"), 896, 128, FLinearColor(.03f, .03f, .035f), FLinearColor(.75f, .58f, .2f), { { TEXT("HOLLIS HALL OF SCIENCE"), .38f } }, true);
    const FLinearColor PlateBg(.08f, .06f, .04f), PlateInk(.78f, .6f, .25f);
    PaintText(Look, TEXT("CI_Sci101"), 256, 110, PlateBg, PlateInk, { { TEXT("101"), .3f }, { TEXT("CHEMISTRY"), .18f } }, true);
    PaintText(Look, TEXT("CI_Sci102"), 256, 110, PlateBg, PlateInk, { { TEXT("102"), .3f }, { TEXT("PHYSICS"), .18f } }, true);
    PaintText(Look, TEXT("CI_Sci110"), 256, 110, PlateBg, PlateInk, { { TEXT("110"), .3f }, { TEXT("LECTURE HALL"), .17f } }, true);
    PaintText(Look, TEXT("CI_Sci112"), 256, 110, PlateBg, PlateInk, { { TEXT("112"), .3f }, { TEXT("PREP ROOM"), .18f } }, true);
    PaintText(Look, TEXT("CI_Sci114"), 256, 110, PlateBg, PlateInk, { { TEXT("114"), .3f }, { TEXT("OFFICE"), .18f } }, true);
    PaintText(Look, TEXT("CI_Sci204"), 256, 110, PlateBg, PlateInk, { { TEXT("204"), .3f }, { TEXT("OPTICS LAB"), .18f } }, true);
    PaintText(Look, TEXT("CI_Sci206"), 256, 110, PlateBg, PlateInk, { { TEXT("206"), .3f }, { TEXT("DARKROOM"), .18f } }, true);
    PaintText(Look, TEXT("CI_Sci210"), 256, 110, PlateBg, PlateInk, { { TEXT("210"), .3f }, { TEXT("SEMINAR"), .18f } }, true);
    PaintText(Look, TEXT("CI_Sci212"), 256, 110, PlateBg, PlateInk, { { TEXT("212"), .3f }, { TEXT("FACULTY LOUNGE"), .16f } }, true);
    PaintText(Look, TEXT("CI_Sci214"), 256, 110, PlateBg, PlateInk, { { TEXT("214"), .3f }, { TEXT("OFFICES"), .18f } }, true);
    HallMakePlain(Look, TEXT("CI_SciReagent"), FLinearColor(.5f, .6f, .55f), .1f);
    HallMakePlain(Look, TEXT("CI_SciAmber"), FLinearColor(.35f, .16f, .03f), .15f);
    HallMakePlain(Look, TEXT("CI_SciBlueGlass"), FLinearColor(.08f, .18f, .45f), .15f);
    HallMakePlain(Look, TEXT("CI_SciFormalin"), FLinearColor(.55f, .5f, .25f), .15f);
    HallMakePlain(Look, TEXT("CI_SciSpecimen"), FLinearColor(.45f, .4f, .32f), .5f);
    HallMakePlain(Look, TEXT("CI_SciBone"), FLinearColor(.72f, .66f, .52f), .6f);
    HallMakePlain(Look, TEXT("CI_SciGreen"), FLinearColor(.1f, .45f, .15f), .4f);
    HallMakePlain(Look, TEXT("CI_SciCookie"), FLinearColor(.45f, .28f, .12f), .8f);
    HallMakePlain(Look, TEXT("CI_SciBrownie"), FLinearColor(.12f, .06f, .03f), .7f);
    HallMakePlain(Look, TEXT("CI_SciNotebook"), FLinearColor(.08f, .2f, .45f), .7f);
    HallMakePlain(Look, TEXT("CI_SciScreen"), FLinearColor(.85f, .85f, .82f), .7f);
    HallMakePlain(Look, TEXT("CI_SciDrape"), FLinearColor(.05f, .04f, .06f), .95f);
    HallMakePlain(Look, TEXT("CI_SciAcrylic"), FLinearColor(.5f, .5f, .48f), .1f);
    HallMakePlain(Look, TEXT("CI_SciLaser"), FLinearColor(.1f, .1f, .1f), .4f, .3f);
    HallMakePlain(Look, TEXT("CI_Aluminium"), FLinearColor(.65f, .65f, .66f), .3f, .9f);
    HallMakePlain(Look, TEXT("CI_SciBeam"), FLinearColor(1.f, .1f, .05f), .5f, 0.f, FLinearColor(12.f, .6f, .3f));
}
FInteriorDef ScienceDef()
{
    FInteriorDef D{ TEXT("SCI"), 2, &BuildScience, &SciLightAnchors, &SciOpenings, &SciDoors, &ScienceLook };
    D.WindowOpen = &SciWindowOpen;
    return D;
}
}
