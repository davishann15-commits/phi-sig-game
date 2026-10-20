// The Goose Union (master job, step 2c, Tier A): the 1960s student union, a four-storey square with an entrance on every
// face; its ground and first floors walk-in. Canonical frame (main doors on +Y, the plan's west face): X -1950..1950,
// Y -1950..1950; floors at -55 and 357.5, ceilings at 327.5 and 740.
//
// A concourse in a cross joins the four entrances; where the arms meet, an atrium rises through both floors under the
// homecoming banner, and a stair climbs each of the north and south arms. Ground floor quadrants: the TV lounge (the game
// replaying on its CRTs) with the campus bookstore behind its glass front (closed Saturday); the Goose Pit snack bar and its
// kitchen; the game room (pool tables, pinball and arcade machines of invented games, foosball, the jukebox); the mailroom
// with its wall of brass post office boxes (the sorting room locked). In the arms: the trophy case with the Dunmore rivalry
// trophy, the information desk, the ride board, payphones and a cigarette machine. First floor: the homecoming committee
// room (tissue scraps, chicken wire, paint, the float's goose head half done) and the Calder Ballroom set for the Class of
// 1961's reunion dinner; the student government, the student paper and the meeting rooms are locked. Homecoming state:
// half-eaten food in the Goose Pit, the TVs still on, coats on chairs, streamers in the school's purple and gold.
#include "HouseCampusInteriorsRooms.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusFallKit.h"
#include "Engine/Texture2D.h"

namespace HouseCampusInteriorsImpl
{
constexpr float UniIn = 1920.f;
// The concourse arms' walls (the same lines on both axes, on piers of the window grid), and the quadrants' splits: the TV
// lounge | bookstore wall, the snack bar | kitchen and mailroom | sorting room wall.
constexpr float UniB0 = -442.5f, UniB1 = 277.5f, UniSplitY = 997.5f, UniSplitX = 1357.5f;
constexpr float UniMid = (UniB0 + UniB1) * .5f;
constexpr float UniHole0 = -300.f, UniHole1 = 135.f;      // the atrium's opening upstairs (a walkway left round it)

FBox2D UniAtrium() { return Rect(UniHole0, UniHole1, UniHole0, UniHole1); }
/** The two stairs' wells: the south one climbs north along the south arm's west wall, the north one south along the north
 *  arm's east wall. */
FBox2D UniWellS() { return Rect(UniB0 + PartT * .5f, UniB0 + PartT * .5f + 200.f, -1300.f, -580.f); }
FBox2D UniWellN() { return Rect(UniB1 - PartT * .5f - 200.f, UniB1 - PartT * .5f, 580.f, 1300.f); }

// ------------------------------------------------------------------ fittings
/** A straight flight in its well (climbing toward +Y or -Y) with a stepped oak guard on its open side (at XG) and a brass
 *  rail, and balustrades upstairs along the well's open side and across its foot. */
void UniStair(FHallMesh& Solid, FHallMesh& Clutter, const FBox2D& W, bool bClimbNorth, float XG, float F0, float F1)
{
    const float YBottom = bClimbNorth ? W.Min.Y : W.Max.Y, YTop = bClimbNorth ? W.Max.Y : W.Min.Y;
    Stair(Solid, W.Min.X + 4.f, W.Max.X - 4.f, YBottom, YTop, F0, F1);
    const int32 N = FMath::CeilToInt((F1 - F0) / 17.5f);
    const float Run = (YTop - YBottom) / N, Rise = (F1 - F0) / N;
    for (int32 K = 0; K < N; ++K)
    {
        const float YA = YBottom + Run * K, YB = YA + Run;
        Box(Solid, TEXT("CI_Oak"), FVector(XG - 4.f, FMath::Min(YA, YB), F0), FVector(XG + 4.f, FMath::Max(YA, YB), F0 + Rise * (K + 1) + 92.f));
    }
    Clutter.Cylinder(TEXT("CI_Brass"), FVector(XG, YBottom, F0 + Rise + 96.f), FVector(XG, YTop, F1 + 96.f), 3.f, 10, UVPlain());
    Balustrade(Solid, FVector2D(XG, W.Min.Y), FVector2D(XG, W.Max.Y), F1);
    Balustrade(Solid, FVector2D(W.Min.X, YBottom), FVector2D(W.Max.X, YBottom), F1);
}
/** A cigarette machine (unbranded): a wood-grain cabinet, a window of packs, a row of chrome pull knobs, the coin slot. */
void UniCigaretteMachine(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing)
{
    const FVector F = Facing.GetSafeNormal2D(), R(-F.Y, F.X, 0.f);
    const FRotator Rot = F.Rotation();
    Solid.Box(TEXT("CI_Walnut"), FTransform(Rot, C + FVector(0, 0, 80.f)), FVector(25.f, 40.f, 80.f), UVPlain());
    HouseCampusFallKit::Card(Glow, TEXT("CI_UniCigWindow"), C + F * 25.3f + FVector(0, 0, 118.f), -R, FVector::UpVector, 70.f, 50.f, F);
    for (int32 K = 0; K < 8; ++K) Solid.Cylinder(TEXT("CI_Steel"), C + F * 25.f + R * (-31.f + K * 9.f) + FVector(0, 0, 78.f), C + F * 30.f + R * (-31.f + K * 9.f) + FVector(0, 0, 78.f), 1.8f, 8, UVPlain());
    Solid.Box(TEXT("CI_Steel"), FTransform(Rot, C + F * 25.5f + R * 28.f + FVector(0, 0, 60.f)), FVector(.5f, 6.f, 9.f), UVPlain());
    Solid.Box(TEXT("CI_Rubber"), FTransform(Rot, C + F * 25.5f + FVector(0, 0, 30.f)), FVector(.5f, 30.f, 7.f), UVPlain());
}
/** A jukebox: the cabinet, its lit arch and the record selector window. */
void UniJukebox(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing)
{
    const FVector F = Facing.GetSafeNormal2D(), R(-F.Y, F.X, 0.f);
    const FRotator Rot = F.Rotation();
    Solid.Box(TEXT("CI_Walnut"), FTransform(Rot, C + FVector(0, 0, 65.f)), FVector(30.f, 38.f, 65.f), UVPlain());
    Solid.Cylinder(TEXT("CI_Walnut"), C + FVector(0, 0, 130.f) - F * 30.f, C + FVector(0, 0, 130.f) + F * 30.f, 38.f, 16, UVPlain());
    Glow.Cylinder(TEXT("CI_UniJukeGlow"), C + FVector(0, 0, 130.f) + F * 30.f, C + FVector(0, 0, 130.f) + F * 31.f, 33.f, 16, UVPlain());
    HouseCampusFallKit::Card(Glow, TEXT("CI_PoBoxes"), C + F * 30.3f + FVector(0, 0, 95.f), -R, FVector::UpVector, 56.f, 30.f, F);
    Solid.Box(TEXT("CI_Grille"), FTransform(Rot, C + F * 30.2f + FVector(0, 0, 40.f)), FVector(.3f, 30.f, 28.f), UVPlain());
}
/** A foosball table on legs, the rods across it and the players. */
void UniFoosball(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C)
{
    for (const float SX : { -1.f, 1.f }) for (const float SY : { -1.f, 1.f }) Box(Solid, TEXT("CI_Walnut"), C + FVector(SX * 55.f - 3.f, SY * 30.f - 3.f, 0.f), C + FVector(SX * 55.f + 3.f, SY * 30.f + 3.f, 70.f));
    Box(Solid, TEXT("CI_Walnut"), C + FVector(-60.f, -36.f, 70.f), C + FVector(60.f, 36.f, 92.f));
    Box(Clutter, TEXT("CI_PingPong"), C + FVector(-56.f, -32.f, 92.f), C + FVector(56.f, 32.f, 92.3f));
    for (int32 K = 0; K < 8; ++K)
    {
        const float X = -49.f + K * 14.f;
        Clutter.Cylinder(TEXT("CI_Steel"), C + FVector(X, -52.f, 100.f), C + FVector(X, 52.f, 100.f), .7f, 6, UVPlain());
        Clutter.Cylinder(TEXT("CI_Rubber"), C + FVector(X, K % 2 ? 46.f : -46.f, 100.f), C + FVector(X, K % 2 ? 52.f : -52.f, 100.f), 1.6f, 8, UVPlain());
        for (int32 P = 0; P < 3; ++P) Box(Clutter, K % 2 ? TEXT("CI_StreamerGold") : TEXT("CI_StreamerPurple"), C + FVector(X - 1.5f, -18.f + P * 18.f - 2.f, 93.f), C + FVector(X + 1.5f, -18.f + P * 18.f + 2.f, 104.f));
    }
}
/** A banquet table set for dinner: a floor-length cloth, eight chairs round it, balloons in the school's colours. */
void UniBanquetTable(FHallMesh& Solid, FHallMesh& Clutter, const FKitter& Kit, const FVector& C, int32 Seed)
{
    using EKit = HouseCampusInteriors::EKit;
    Solid.Cylinder(TEXT("CI_Tablecloth"), C, C + FVector(0, 0, 75.f), 90.f, 24, UVPlain());
    for (int32 K = 0; K < 8; ++K)
    {
        const float A = K * 45.f + (Seed % 2) * 22.5f;
        const FVector D = FRotator(0.f, A, 0.f).Vector();
        Kit.Floor(EKit::SpindleChair, FVector2D(C + D * 124.f), C.Z, A + 180.f, true);
        Clutter.Cylinder(TEXT("CI_Enamel"), C + D * 66.f + FVector(0, 0, 75.f), C + D * 66.f + FVector(0, 0, 75.6f), 13.f, 14, UVPlain());
    }
    for (int32 K = 0; K < 3; ++K)
    {
        const FVector Top = C + FVector(FMath::Cos(K * 2.1f) * 8.f, FMath::Sin(K * 2.1f) * 8.f, 150.f + K * 12.f);
        Clutter.Cylinder(TEXT("CI_Paper"), C + FVector(0, 0, 75.f), Top, .2f, 4, UVPlain());
        HouseCampusFallKit::Ball(Clutter, K % 2 ? TEXT("CI_StreamerGold") : TEXT("CI_StreamerPurple"), Top + FVector(0, 0, 13.f), 13.f, 10);
    }
    Clutter.Cylinder(TEXT("CI_Brass"), C + FVector(0, 0, 75.f), C + FVector(0, 0, 77.f), 8.f, 10, UVPlain());
}
/** A burger, half eaten, on its paper, with fries in a boat beside it. */
void UniLeftovers(FHallMesh& Clutter, const FVector& C, float Yaw, int32 Seed)
{
    const FRotator R(0.f, Yaw, 0.f);
    Clutter.Box(TEXT("CI_Paper"), FTransform(R, C + FVector(0, 0, .1f)), FVector(14.f, 14.f, .1f), UVPlain());
    Clutter.Cylinder(TEXT("CI_Bun"), C + FVector(0, 0, .2f), C + FVector(0, 0, 2.f), 5.5f, 12, UVPlain());
    Clutter.Cylinder(TEXT("CI_Patty"), C + FVector(0, 0, 2.f), C + FVector(0, 0, 3.f), 5.8f, 12, UVPlain());
    if (Seed % 2) Clutter.Dome(TEXT("CI_Bun"), C + FVector(0, 0, 3.f), FVector::UpVector, 5.5f, 3.f, 12);
    const FVector Boat = C + R.RotateVector(FVector(0.f, 17.f, 0.f));
    Clutter.Box(TEXT("CI_Paper"), FTransform(R, Boat + FVector(0, 0, 2.f)), FVector(7.f, 4.f, 2.f), UVPlain());
    for (int32 K = 0; K < 7; ++K) Clutter.Box(TEXT("CI_Fries"), FTransform(FRotator(0.f, Yaw + K * 9.f, 15.f), Boat + FVector(-4.f + K * 1.3f, 0.f, 4.f)), FVector(3.5f, .4f, .4f), UVPlain());
    Clutter.Cylinder(TEXT("CI_Ketchup"), C + R.RotateVector(FVector(-15.f, 8.f, 0.f)), C + R.RotateVector(FVector(-15.f, 8.f, 0.f)) + FVector(0, 0, 18.f), 2.8f, 10, UVPlain());
}
/** The homecoming float's goose head, half done on its pallet: the chicken-wire neck and head, white tissue pomps over
 *  part of it, the gold beak. */
void UniGooseHead(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, float Yaw)
{
    const FRotator R(0.f, Yaw, 0.f);
    Box(Solid, TEXT("CI_Oak"), C + FVector(-60.f, -60.f, 0.f), C + FVector(60.f, 60.f, 12.f));
    FVector Prev = C + FVector(0, 0, 12.f);
    for (int32 K = 1; K <= 5; ++K)
    {
        const FVector P = C + R.RotateVector(FVector(FMath::Sin(K * .35f) * 30.f, 0.f, 12.f + K * 34.f));
        Solid.Cylinder(TEXT("CI_ChickenWire"), Prev, P, 22.f - K * 1.5f, 12, UVPlain());
        Prev = P;
    }
    const FVector Head = Prev + R.RotateVector(FVector(22.f, 0.f, 18.f));
    HouseCampusFallKit::Ball(Solid, TEXT("CI_ChickenWire"), Head, 34.f, 14);
    HouseCampusFallKit::Lathe(Clutter, TEXT("CI_TissueGold"), Head + R.RotateVector(FVector(30.f, 0.f, -4.f)), { FVector2D(.1f, 0.f), FVector2D(10.f, 0.f), FVector2D(.1f, 1.f) }, 10);
    Clutter.Cylinder(TEXT("CI_TissueGold"), Head + R.RotateVector(FVector(28.f, 0.f, -6.f)), Head + R.RotateVector(FVector(58.f, 0.f, -10.f)), 7.f, 10, UVPlain());
    HouseCampusFallKit::Ball(Clutter, TEXT("CI_Epoxy"), Head + R.RotateVector(FVector(20.f, 22.f, 10.f)), 4.f, 8);
    HouseCampusFallKit::Ball(Clutter, TEXT("CI_Epoxy"), Head + R.RotateVector(FVector(20.f, -22.f, 10.f)), 4.f, 8);
    // Pomps on the back of the head and down one side of the neck: the rest is still bare wire.
    FRandomStream Rnd(int32(C.X));
    for (int32 K = 0; K < 70; ++K)
    {
        const FVector D = Rnd.VRand();
        if (FVector::DotProduct(D, R.Vector()) > .2f) continue;
        HouseCampusFallKit::Ball(Clutter, TEXT("CI_TissueWhite"), Head + D * 35.f, 5.f, 6);
    }
    for (int32 K = 0; K < 30; ++K)
        HouseCampusFallKit::Ball(Clutter, TEXT("CI_TissueWhite"), C + R.RotateVector(FVector(-16.f + Rnd.FRandRange(-4.f, 4.f), -10.f + Rnd.FRandRange(-12.f, 12.f), 20.f + K * 5.5f)), 5.f, 6);
}
/** A small combo's drum kit on the bandstand. */
void UniDrums(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Facing)
{
    const FVector F = Facing.GetSafeNormal2D(), R(-F.Y, F.X, 0.f);
    Solid.Cylinder(TEXT("CI_StreamerPurple"), C + R * -18.f + FVector(0, 0, 28.f), C + R * 18.f + FVector(0, 0, 28.f), 28.f, 16, UVPlain());
    Solid.Cylinder(TEXT("CI_Enamel"), C + R * -18.5f + FVector(0, 0, 28.f), C + R * -18.f + FVector(0, 0, 28.f), 26.f, 16, UVPlain());
    for (const float S : { -1.f, 1.f })
    {
        const FVector T = C - F * 20.f + R * (S * 30.f) + FVector(0, 0, 62.f);
        Solid.Cylinder(TEXT("CI_StreamerPurple"), T, T + FVector(0, 0, 16.f), 14.f, 14, UVPlain());
        Clutter.Cylinder(TEXT("CI_Steel"), C - F * 40.f + R * (S * 55.f), C - F * 40.f + R * (S * 55.f) + FVector(0, 0, 105.f), .8f, 6, UVPlain());
        Clutter.Cylinder(TEXT("CI_Brass"), C - F * 40.f + R * (S * 55.f) + FVector(0, 0, 105.f), C - F * 40.f + R * (S * 55.f) + FVector(0, 0, 106.f), 22.f, 16, UVPlain());
    }
    Clutter.Cylinder(TEXT("CI_Steel"), C + F * 60.f, C + F * 60.f + FVector(0, 0, 150.f), .7f, 6, UVPlain());   // a microphone stand
    Clutter.Cylinder(TEXT("CI_CrtBezel"), C + F * 60.f + FVector(0, 0, 150.f), C + F * 56.f + FVector(0, 0, 158.f), 1.8f, 8, UVPlain());
}

// ------------------------------------------------------------------ the ground floor
void UniGround(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, float Z, float Ceil)
{
    using EKit = HouseCampusInteriors::EKit;
    const float H = PartT * .5f;
    // ---- the TV lounge (X -1920..-450, Y 285..990): the console and two portables still replaying the game
    CrtTelevision(Solid, Glow, FVector(-1185.f, UniSplitY - H - 26.f, Z), FVector(0, -1, 0), true);
    for (const float X : { -1720.f, -660.f })
    {
        Box(Solid, TEXT("CI_Steel"), FVector(X - 30.f, UniSplitY - H - 70.f, Z), FVector(X + 30.f, UniSplitY - H - 20.f, Z + 80.f));
        CrtTelevision(Solid, Glow, FVector(X, UniSplitY - H - 45.f, Z + 80.f), FVector(X < -1000.f ? .35f : -.35f, -1.f, 0.f), false);
    }
    for (float X = -1900.f; X < -470.f; X += 61.f) Box(Solid, TEXT("CI_Walnut"), FVector(X, UniSplitY - H - 1.5f, Z), FVector(X + 60.f, UniSplitY - H, Z + 240.f));   // paneling
    WallClock(Clutter, FVector(-880.f, UniSplitY - H - 1.5f, Z + 266.f), FVector(0, -1, 0), 15.f);
    Kit.Floor(EKit::StudyRug, FVector2D(-1185.f, 720.f), Z, 0.f, false);
    for (const float X : { -1400.f, -950.f })
    {
        Kit.Floor(EKit::LeatherSofa, FVector2D(X, 600.f), Z, 90.f, true);
        Kit.Floor(EKit::ChairJacket, FVector2D(X + 40.f, 560.f), Z + 22.f, 90.f, false);
    }
    Kit.Floor(EKit::TanRecliner, FVector2D(-1700.f, 680.f), Z, 60.f, true);
    Kit.Floor(EKit::ClubChair, FVector2D(-700.f, 860.f), Z, 150.f, true);
    Kit.Floor(EKit::CoffeeChest, FVector2D(-1185.f, 800.f), Z, 90.f, true);
    const float Chest = Z + Kit.Height(EKit::CoffeeChest);
    Solid.Cylinder(TEXT("CI_Enamel"), FVector(-1210.f, 800.f, Chest), FVector(-1210.f, 800.f, Chest + 8.f), 15.f, 16, UVPlain());
    for (int32 I = 0; I < 14; ++I) HouseCampusFallKit::Ball(Clutter, TEXT("CI_Paper"), FVector(-1210.f + 9.f * FMath::Cos(I * 2.4f) * (I % 3) * .5f, 800.f + 9.f * FMath::Sin(I * 2.4f) * (I % 3) * .5f, Chest + 8.f + (I % 4)), 1.8f, 5);
    for (const FVector2D& C : { FVector2D(-1180.f, 815.f), FVector2D(-1172.f, 780.f) }) Kit.Floor(EKit::PaperCup, C, Chest, 0.f, true);   // on the chest's top
    HarCan(Clutter, FVector(-980.f, 760.f, Z), true, true, 40.f);
    for (const float X : { -1580.f, -800.f }) AshUrn(Solid, FVector(X, 420.f, Z));
    for (const float Y : { 457.5f, 817.5f }) Radiator(Solid, FVector(-UniIn, Y, Z), FVector(1, 0, 0), 120.f);
    for (const float X : { -1500.f, -870.f }) for (const float Y : { 450.f, 820.f }) Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), true);
    WallPlaque(Clutter, TEXT("CI_UniTVLounge"), FVector(UniB0 - H, 600.f, Z + 280.f), FVector(1, 0, 0), 120.f, 28.f);
    // ---- the bookstore (closed; seen through its glass front): shelving, the counter and register, a rack of sweatshirts
    for (const float Y : { 1350.f, 1560.f, 1770.f }) Shelving(Solid, Clutter, FVector(-1820.f, Y, 0.f), FVector(1, 0, 0), 900.f, Z, 180.f);
    Box(Solid, TEXT("CI_Oak"), FVector(-720.f, 1060.f, Z), FVector(-560.f, 1230.f, Z + 100.f));
    Box(Solid, TEXT("CI_Beige"), FVector(-690.f, 1120.f, Z + 100.f), FVector(-640.f, 1170.f, Z + 125.f));
    Clutter.Cylinder(TEXT("CI_Steel"), FVector(-1500.f, 1120.f, Z + 150.f), FVector(-900.f, 1120.f, Z + 150.f), 1.2f, 8, UVPlain());
    for (float X = -1480.f; X < -920.f; X += 24.f)
        Box(Clutter, int32(X / 24.f) % 2 ? TEXT("CI_StreamerGold") : TEXT("CI_StreamerPurple"), FVector(X - 10.f, 1110.f, Z + 80.f), FVector(X + 10.f, 1130.f, Z + 148.f));
    for (const float Y : { 1340.f, 1700.f }) HarPennant(Clutter, TEXT("CI_Pennant"), FVector(-UniIn + .1f, Y, Z + 200.f), FVector(1, 0, 0), 70.f, 26.f);   // on the piers
    for (const float X : { -1600.f, -1000.f }) Troffer(Glow, Clutter, FVector(X, 1450.f, Ceil - .6f), true);
    HangingSign(Clutter, TEXT("CI_UniBookstore"), FVector(UniB0 + 60.f, 1620.f, Ceil - 55.f), FVector(1, 0, 0), 240.f, 34.f, Ceil);
    // ---- the Goose Pit (X 285..1350, Y 285..1920): the serving line at the kitchen's pass-through, tables, the leftovers
    const FBox2D Pit = Rect(UniB1 + H, UniSplitX - H, UniB1 + H, UniIn);
    RoomDining(Solid, Glow, Kit, Pit, Z, 3, 77);
    HouseCampusFallKit::Card(Clutter, TEXT("CI_UniMenu"), FVector(UniSplitX - H - .5f, 950.f, Z + 280.f), FVector(0, 1, 0), FVector::UpVector, 760.f, 72.f, FVector(-1, 0, 0));
    Box(Solid, TEXT("CI_Steel"), FVector(UniSplitX - 30.f, 500.f, Z + 152.f), FVector(UniSplitX + 30.f, 1400.f, Z + 155.f));   // the pick-up shelf
    Glow.Box(TEXT("CI_HoodLight"), FTransform(FVector(UniSplitX, 950.f, Z + 232.f)), FVector(8.f, 440.f, .5f), UVPlain());
    FRandomStream Rnd(1986);
    // (At five of RoomDining's tables: X 1050/820/590, Y 445 + 260k; a coat on the chair at each one's west side.)
    for (const FVector2D& P : { FVector2D(1050.f, 445.f), FVector2D(820.f, 965.f), FVector2D(590.f, 1225.f), FVector2D(1050.f, 1485.f), FVector2D(820.f, 1745.f) })
    {
        UniLeftovers(Clutter, FVector(P + FVector2D(Rnd.FRandRange(-12.f, 12.f), -30.f), Z + 77.f), Rnd.FRandRange(0.f, 360.f), Rnd.RandRange(0, 9));
        Kit.Floor(EKit::ChairJacket, P + FVector2D(-82.f, 45.f), Z + 32.f, 0.f, false);
    }
    for (const float X : { 457.5f, 817.5f, 1177.5f }) Radiator(Solid, FVector(X, UniIn, Z), FVector(0, -1, 0), 120.f);
    for (const float X : { 500.f, 820.f, 1140.f }) for (const float Y : { 500.f, 900.f, 1300.f, 1700.f }) Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), false);
    HangingSign(Clutter, TEXT("CI_UniGoosePit"), FVector(UniB1 + 60.f, 1600.f, Ceil - 55.f), FVector(-1, 0, 0), 260.f, 40.f, Ceil);
    // ---- the kitchen behind (X 1365..1920): the griddle and fryers on the line, prep tables, the refrigerator, shelves
    Box(Solid, TEXT("CI_Steel"), FVector(UniSplitX + H, 500.f, Z), FVector(UniSplitX + H + 80.f, 900.f, Z + 88.f));
    Box(Solid, TEXT("CI_Epoxy"), FVector(UniSplitX + H, 520.f, Z + 88.f), FVector(UniSplitX + H + 70.f, 880.f, Z + 90.f));
    for (int32 K = 0; K < 2; ++K)
    {
        const float Y = 950.f + K * 70.f;
        Box(Solid, TEXT("CI_Steel"), FVector(UniSplitX + H, Y, Z), FVector(UniSplitX + H + 70.f, Y + 60.f, Z + 92.f));
        Box(Clutter, TEXT("CI_SciAmber"), FVector(UniSplitX + H + 8.f, Y + 8.f, Z + 85.f), FVector(UniSplitX + H + 62.f, Y + 52.f, Z + 91.f));
    }
    Box(Solid, TEXT("CI_Enamel"), FVector(UniSplitX + H, 1120.f, Z), FVector(UniSplitX + H + 76.f, 1400.f, Z + 90.f));
    Table(Solid, FVector(1660.f, 1000.f, Z), 300.f, 80.f, 90.f, false);
    Box(Solid, TEXT("CI_Enamel"), FVector(1820.f, 1760.f, Z), FVector(1918.f, 1900.f, Z + 190.f));
    for (const float Y : { 457.5f, 817.5f, 1177.5f, 1537.5f }) Box(Solid, TEXT("CI_Steel"), FVector(UniIn - 45.f, Y - 80.f, Z), FVector(UniIn, Y + 80.f, Z + 80.f));
    for (const float Y : { 600.f, 1200.f, 1700.f }) Troffer(Glow, Clutter, FVector(1640.f, Y, Ceil - .6f), true);
    // ---- the game room (X -1920..-450, Y -1920..-450)
    for (const float X : { -1560.f, -840.f })
        for (const float Y : { -900.f, -1450.f })
        {
            Kit.Floor(EKit::BilliardTable, FVector2D(X, Y), Z, 90.f, false);
            Box(Clutter, TEXT("CI_SciGreen"), FVector(X - 70.f, Y - 14.f, Ceil - 75.f), FVector(X + 70.f, Y + 14.f, Ceil - 60.f));
            Glow.Box(TEXT("CI_HoodLight"), FTransform(FVector(X, Y, Ceil - 75.5f)), FVector(66.f, 11.f, .3f), UVPlain());
            for (const float S : { -1.f, 1.f }) Clutter.Cylinder(TEXT("CI_Steel"), FVector(X + S * 60.f, Y, Ceil - 60.f), FVector(X + S * 60.f, Y, Ceil), .5f, 4, UVPlain());
        }
    for (const float Y : { -1522.5f, -1162.5f, -802.5f }) Kit.Wall(EKit::CueRack, FVector(-UniIn, Y, Z + 110.f), FVector(1, 0, 0));
    const FName Glasses[] = { TEXT("CI_Backglass"), TEXT("CI_BackglassHalftime"), TEXT("CI_BackglassMoon") };
    for (int32 K = 0; K < 3; ++K) Pinball(Solid, Glow, FVector(-1820.f + K * 130.f, UniB0 - H - 70.f, Z), FVector(0, -1, 0), Glasses[K]);
    for (int32 K = 0; K < 3; ++K) Kit.Floor(EKit::Arcade, FVector2D(UniB0 - H - 42.f, -1250.f + K * 95.f), Z, 180.f, false);
    UniJukebox(Solid, Glow, FVector(-880.f, UniB0 - H - 32.f, Z), FVector(0, -1, 0));
    UniFoosball(Solid, Clutter, FVector(-1700.f, -1780.f, Z));
    Chalkboard(Solid, Clutter, FVector(-660.f, UniB0 - H, Z + 150.f), FVector(0, -1, 0), 180.f, 100.f, TEXT("CI_UniPoolLadder"));
    Box(Solid, TEXT("CI_Steel"), FVector(UniB0 - H - 14.f, -1460.f, Z + 110.f), FVector(UniB0 - H, -1420.f, Z + 160.f));   // the change machine
    for (const float Y : { -622.5f, -982.5f, -1342.5f, -1702.5f }) Radiator(Solid, FVector(-UniIn, Y, Z), FVector(1, 0, 0), 120.f);
    for (const float X : { -1200.f, -500.f }) Troffer(Glow, Clutter, FVector(X, -1180.f, Ceil - .6f), true);
    for (const float X : { -1800.f, -1200.f, -600.f }) Troffer(Glow, Clutter, FVector(X, -650.f, Ceil - .6f), true, X < -1500.f ? FName(TEXT("CI_TrofferFlicker")) : FName());
    WallPlaque(Clutter, TEXT("CI_UniGameRoom"), FVector(-1150.f, UniB0 + H, Z + 262.f), FVector(0, 1, 0), 120.f, 28.f);
    // ---- the mailroom (X 285..1350, Y -1920..-450): the post office boxes, the service window (closed), a writing desk
    MailboxWall(Solid, Clutter, FVector(UniSplitX - H, -1200.f, Z + 130.f), FVector(-1, 0, 0), 700.f, 180.f);
    Box(Solid, TEXT("CI_Oak"), FVector(UniSplitX - 28.f, -705.f, Z + 98.f), FVector(UniSplitX + 28.f, -545.f, Z + 102.f));
    HouseCampusFallKit::Card(Clutter, TEXT("CI_UniWindowClosed"), FVector(UniSplitX - 2.f, -625.f, Z + 150.f), FVector(0, 1, 0), FVector::UpVector, 140.f, 98.f, FVector(-1, 0, 0));
    Table(Solid, FVector(800.f, -1200.f, Z), 220.f, 70.f, 100.f, true);
    Kit.Floor(EKit::PaperStack, FVector2D(760.f, -1200.f), Z + 100.f, 15.f, true);
    for (const float X : { 457.5f, 817.5f, 1177.5f }) Radiator(Solid, FVector(X, -UniIn, Z), FVector(0, 1, 0), 120.f);
    for (const float X : { 550.f, 1050.f }) for (const float Y : { -1600.f, -900.f }) Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), true);
    WallPlaque(Clutter, TEXT("CI_UniMail"), FVector(UniB1 + H, -950.f, Z + 270.f), FVector(1, 0, 0), 120.f, 28.f);
    // ---- the concourse: entry mats at the four doors, the directory, the trophy case with the Dunmore trophy, the
    // information desk, the ride board, payphones, the cigarette machine, lounge seating, the floor seal, streamers.
    Kit.Floor(EKit::EntryRug, FVector2D(0.f, UniIn - 70.f), Z, 90.f, false);
    Kit.Floor(EKit::EntryRug, FVector2D(0.f, -UniIn + 70.f), Z, 90.f, false);
    Kit.Floor(EKit::EntryRug, FVector2D(-UniIn + 70.f, 0.f), Z, 0.f, false);
    Kit.Floor(EKit::EntryRug, FVector2D(UniIn - 70.f, 0.f), Z, 0.f, false);
    for (const FVector& E : { FVector(0.f, UniIn - 6.f, 0.f), FVector(0.f, -UniIn + 6.f, 1.f), FVector(-UniIn + 6.f, 0.f, 2.f), FVector(UniIn - 6.f, 0.f, 3.f) })
        ExitSign(Solid, Glow, FVector(E.X, E.Y, Z + 236.f), E.Z == 0.f ? FVector(0, -1, 0) : E.Z == 1.f ? FVector(0, 1, 0) : E.Z == 2.f ? FVector(1, 0, 0) : FVector(-1, 0, 0));
    DirectoryBoard(Solid, Clutter, FVector(UniB1 - H, 450.f, Z + 160.f), FVector(-1, 0, 0), TEXT("CI_UniDirectory"));
    HouseCampusFallKit::Card(Clutter, TEXT("CI_UniSeal"), FVector(-82.5f, -82.5f, Z + .4f), FVector(-1, 0, 0), FVector(0, 1, 0), 300.f, 300.f, FVector::UpVector);   // reads from the main doors' side
    TrophyCase(Solid, Glow, FVector(1250.f, UniB1 - H, Z), FVector(0, -1, 0), 300.f);
    {
        // The Old Lantern: Calder and Dunmore's rivalry trophy since 1923, a brass lantern on a walnut base, in the middle
        // of the case; its plaque over the case.
        const FVector L(1250.f, UniB1 - H - 22.f, Z + 128.6f);
        Box(Solid, TEXT("CI_Walnut"), L + FVector(-14.f, -10.f, 0.f), L + FVector(14.f, 10.f, 8.f));
        HouseCampusFallKit::Lathe(Solid, TEXT("CI_Brass"), L + FVector(0, 0, 8.f), { FVector2D(.1f, 0.f), FVector2D(8.f, 0.f), FVector2D(8.f, 3.f), FVector2D(6.f, 4.f), FVector2D(6.f, 20.f), FVector2D(9.f, 22.f), FVector2D(4.f, 28.f), FVector2D(1.f, 32.f), FVector2D(.1f, 32.f) }, 12);
        Glow.Cylinder(TEXT("CI_PilotLamp"), L + FVector(0, 0, 13.f), L + FVector(0, 0, 25.f), 4.f, 10, UVPlain());
        WallPlaque(Clutter, TEXT("CI_UniLantern"), FVector(1250.f, UniB1 - H, Z + 250.f), FVector(0, -1, 0), 200.f, 40.f);
    }
    Box(Solid, TEXT("CI_Oak"), FVector(800.f, UniB0 + H, Z), FVector(1150.f, UniB0 + H + 70.f, Z + 105.f));   // the information desk
    Box(Solid, TEXT("CI_OakDark"), FVector(795.f, UniB0 + H + 60.f, Z + 105.f), FVector(1155.f, UniB0 + H + 80.f, Z + 108.f));
    DeskPhone(Solid, FVector(1100.f, UniB0 + H + 35.f, Z + 105.f), FVector(0, 1, 0), false);
    Kit.Floor(EKit::PaperStack, FVector2D(900.f, UniB0 + H + 40.f), Z + 105.f, 0.f, true);
    Box(Solid, TEXT("CI_Cardboard"), FVector(1160.f, UniB0 + H + 5.f, Z), FVector(1230.f, UniB0 + H + 55.f, Z + 45.f));   // lost and found
    HangingSign(Clutter, TEXT("CI_UniInfo"), FVector(975.f, UniB0 + 60.f, Ceil - 55.f), FVector(0, 1, 0), 220.f, 34.f, Ceil);
    WallClock(Clutter, FVector(975.f, UniB0 + H, Z + 245.f), FVector(0, 1, 0), 18.f);
    BulletinBoard(Solid, Clutter, FVector(UniB1 - H, -1500.f, Z + 150.f), FVector(-1, 0, 0), 220.f, 120.f, TEXT("CI_RideBoard"));
    WallPlaque(Clutter, TEXT("CI_UniRideBoard"), FVector(UniB1 - H, -1500.f, Z + 228.f), FVector(-1, 0, 0), 110.f, 24.f);
    BulletinBoard(Solid, Clutter, FVector(UniB1 - H, -1780.f, Z + 150.f), FVector(-1, 0, 0), 140.f, 100.f, TEXT("CI_NoticeBoard"));
    for (const float X : { -1340.f, -1280.f, -1220.f }) PayPhone(Solid, Clutter, FVector(X, UniB1 - H, Z + 140.f), FVector(0, -1, 0));
    UniCigaretteMachine(Solid, Glow, FVector(-960.f, UniB1 - H - 26.f, Z), FVector(0, -1, 0));
    WaterFountain(Solid, FVector(-760.f, UniB1 - H, Z), FVector(0, -1, 0));
    for (const float X : { -1700.f, -1450.f }) Kit.Floor(EKit::Loveseat, FVector2D(X, UniB0 + H + 45.f), Z, 90.f, true);
    Kit.Floor(EKit::FireExtinguisher, FVector2D(UniB0 + H + 12.f, -400.f), Z + 80.f, 0.f, false);
    Kit.Floor(EKit::PlantStand, FVector2D(-380.f, UniIn - 60.f), Z, 0.f, true);
    Kit.Floor(EKit::PlantStand, FVector2D(220.f, UniIn - 60.f), Z, 0.f, true);
    for (float D = -1700.f; D < 1500.f; D += 640.f)
    {
        Streamer(Clutter, FVector(UniB0 + 10.f, D, Ceil - 10.f), FVector(UniB1 - 10.f, D + 640.f, Ceil - 10.f), int32((D + 1700.f) / 640.f) % 2 == 0);
        Streamer(Clutter, FVector(D, UniB0 + 10.f, Ceil - 10.f), FVector(D + 640.f, UniB1 - 10.f, Ceil - 10.f), int32((D + 1700.f) / 640.f) % 2 == 1);
    }
    for (float D = -1600.f; D <= 1600.f; D += 400.f)
    {
        if (D > UniB0 - 100.f && D < UniB1 + 100.f) continue;   // under the atrium
        Troffer(Glow, Clutter, FVector(UniMid, D, Ceil - .6f), false);
        Troffer(Glow, Clutter, FVector(D, UniMid, Ceil - .6f), true);
    }
}

// ------------------------------------------------------------------ the first floor
void UniUpstairs(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, float Z, float Ceil)
{
    using EKit = HouseCampusInteriors::EKit;
    const float H = PartT * .5f;
    // ---- the homecoming committee room (X -1920..-450, Y 285..1920)
    UniGooseHead(Solid, Clutter, FVector(-1550.f, 1600.f, Z), 0.f);
    int32 Seed = 3;
    for (const FVector2D& T : { FVector2D(-1550.f, 1050.f), FVector2D(-1000.f, 1050.f), FVector2D(-1000.f, 1450.f) })
    {
        Table(Solid, FVector(T, Z), 240.f, 100.f, 76.f, true);
        HarTissueBox(Solid, FVector(T.X - 60.f, T.Y, Z + 76.f), Seed++);
        for (int32 K = 0; K < 3; ++K) Clutter.Cylinder(K % 2 ? TEXT("CI_StreamerGold") : TEXT("CI_StreamerPurple"), FVector(T.X + 40.f + K * 14.f, T.Y - 20.f, Z + 80.f), FVector(T.X + 40.f + K * 14.f, T.Y + 20.f, Z + 80.f), 4.f, 10, UVPlain());
        Box(Clutter, TEXT("CI_Steel"), FVector(T.X + 90.f, T.Y + 20.f, Z + 76.f), FVector(T.X + 104.f, T.Y + 22.f, Z + 76.6f));   // scissors
        for (const float S : { -1.f, 1.f }) Kit.Floor(EKit::SpindleChair, T + FVector2D(S * 70.f, -80.f), Z, 90.f + S * 12.f, true);
    }
    FRandomStream Rnd(1961);
    const FName Tissue[] = { TEXT("CI_TissuePurple"), TEXT("CI_TissueGold"), TEXT("CI_TissueWhite") };
    for (int32 K = 0; K < 90; ++K)   // the scraps on the floor
    {
        const FVector P(Rnd.FRandRange(-1880.f, -500.f), Rnd.FRandRange(320.f, 1880.f), Z + .15f);
        Clutter.Box(Tissue[K % 3], FTransform(FRotator(0.f, Rnd.FRandRange(0.f, 360.f), 0.f), P), FVector(Rnd.FRandRange(3.f, 7.f), Rnd.FRandRange(3.f, 7.f), .1f), UVPlain());
    }
    for (const float X : { -1750.f, -1650.f }) Solid.Cylinder(TEXT("CI_ChickenWire"), FVector(X, 380.f, Z + 18.f), FVector(X, 470.f, Z + 18.f), 18.f, 14, UVPlain());
    for (int32 K = 0; K < 4; ++K)
    {
        const FVector Can(-1250.f + K * 22.f, 800.f, Z);
        Clutter.Cylinder(TEXT("CI_Enamel"), Can, Can + FVector(0, 0, 11.f), 8.f, 12, UVPlain());
        Clutter.Cylinder(K % 2 ? TEXT("CI_StreamerGold") : TEXT("CI_StreamerPurple"), Can + FVector(0, 0, 11.f), Can + FVector(0, 0, 11.3f), 7.f, 12, UVPlain());
    }
    HouseCampusFallKit::Card(Clutter, TEXT("CI_UniBeatBanner"), FVector(-1450.f, 600.f, Z + 1.f), FVector(-1, 0, 0), FVector(0, 1, 0), 300.f, 110.f, FVector::UpVector);   // (face up, read from the south)
    Chalkboard(Solid, Clutter, FVector(UniB0 - H, 1300.f, Z + 150.f), FVector(-1, 0, 0), 300.f, 110.f, TEXT("CI_UniCommittee"));
    HarPizzaBox(Clutter, FVector(-1530.f, 1070.f, Z + 76.f), 20.f);
    Kit.Floor(EKit::PaperCup, FVector2D(-1480.f, 1040.f), Z + 76.f, 0.f, true);
    Kit.Floor(EKit::PaperCup, FVector2D(-980.f, 1430.f), Z + 76.f, 0.f, true);
    for (const float Y : { 457.5f, 817.5f, 1177.5f, 1537.5f }) Radiator(Solid, FVector(-UniIn, Y, Z), FVector(1, 0, 0), 120.f);
    for (const float X : { -1600.f, -1000.f }) for (const float Y : { 600.f, 1100.f, 1600.f }) Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), true);
    WallPlaque(Clutter, TEXT("CI_UniCommitteeSign"), FVector(UniB0 + H, 760.f, Z + 170.f), FVector(1, 0, 0), 80.f, 26.f);
    // ---- the Calder Ballroom (X 285..1920, Y 285..1920), set for the reunion dinner
    Box(Solid, TEXT("CI_OakDark"), FVector(900.f, UniB1 + H, Z), FVector(1500.f, UniB1 + H + 300.f, Z + 60.f));   // the bandstand
    for (int32 K = 0; K < 4; ++K) Box(Solid, TEXT("CI_OakDark"), FVector(1150.f, UniB1 + H + 300.f, Z), FVector(1250.f, UniB1 + H + 300.f + 30.f * (4 - K), Z + 15.f * (K + 1)));
    UniDrums(Solid, Clutter, FVector(1300.f, UniB1 + H + 120.f, Z + 60.f), FVector(0, 1, 0));
    Box(Solid, TEXT("CI_Oak"), FVector(1000.f, UniB1 + H + 180.f, Z + 60.f), FVector(1050.f, UniB1 + H + 220.f, Z + 170.f));   // the podium
    HouseCampusFallKit::Card(Clutter, TEXT("CI_UniReunion"), FVector(1200.f, UniB1 + H + .5f, Z + 250.f), FVector(1, 0, 0), FVector::UpVector, 560.f, 70.f, FVector(0, 1, 0));
    Box(Clutter, TEXT("CI_Oak"), FVector(950.f, 1050.f, Z), FVector(1250.f, 1350.f, Z + .4f));   // the dance floor
    int32 Table = 0;
    for (const float X : { 650.f, 1050.f, 1450.f })
        for (const float Y : { 850.f, 1250.f, 1650.f })
        {
            if (X == 1050.f && Y == 1250.f) continue;
            UniBanquetTable(Solid, Clutter, Kit, FVector(X, Y, Z), Table++);
        }
    Clutter.Cylinder(TEXT("CI_Steel"), FVector(400.f, 1800.f, Z), FVector(400.f, 1800.f, Z + 175.f), 1.5f, 8, UVPlain());   // the coat rack
    for (int32 K = 0; K < 4; ++K) Kit.Floor(EKit::ChairJacket, FVector2D(400.f + FMath::Cos(K * 1.57f) * 14.f, 1800.f + FMath::Sin(K * 1.57f) * 14.f), Z + 110.f, K * 90.f, false);
    for (float X = 350.f; X < 1900.f; X += 500.f) Streamer(Clutter, FVector(X, 300.f, Ceil - 8.f), FVector(X + 300.f, 1900.f, Ceil - 8.f), int32(X / 500.f) % 2 == 0);
    for (const float X : { 457.5f, 817.5f, 1177.5f, 1537.5f }) Radiator(Solid, FVector(X, UniIn, Z), FVector(0, -1, 0), 120.f);
    for (const float Y : { 457.5f, 817.5f, 1177.5f, 1537.5f }) Radiator(Solid, FVector(UniIn, Y, Z), FVector(-1, 0, 0), 120.f);
    for (const float X : { 650.f, 1050.f, 1450.f }) for (const float Y : { 650.f, 1100.f, 1550.f }) Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), false);
    WallPlaque(Clutter, TEXT("CI_UniBallroomSign"), FVector(UniB1 - H, 1560.f, Z + 170.f), FVector(-1, 0, 0), 90.f, 26.f);
    // ---- the arms upstairs: notices and plaques at the locked doors, seating by the atrium, streamers
    WallPlaque(Clutter, TEXT("CI_UniGovSign"), FVector(-1150.f, UniB0 + H, Z + 170.f), FVector(0, 1, 0), 90.f, 26.f);
    WallPlaque(Clutter, TEXT("CI_UniHonkerSign"), FVector(UniB0 + H, -1560.f, Z + 170.f), FVector(1, 0, 0), 90.f, 26.f);
    WallPlaque(Clutter, TEXT("CI_UniMeetingSign"), FVector(950.f, UniB0 + H, Z + 170.f), FVector(0, 1, 0), 90.f, 26.f);
    BulletinBoard(Solid, Clutter, FVector(-700.f, UniB1 - H, Z + 150.f), FVector(0, -1, 0), 180.f, 100.f, TEXT("CI_NoticeBoard"));
    WallClock(Clutter, FVector(-700.f, UniB1 - H, Z + 258.f), FVector(0, -1, 0), 15.f);
    for (const float X : { 1200.f, 1650.f }) Kit.Floor(EKit::Loveseat, FVector2D(X, UniB0 + H + 45.f), Z, 90.f, true);
    for (float D = -1700.f; D < 1500.f; D += 640.f)
        Streamer(Clutter, FVector(D, UniB0 + 10.f, Ceil - 10.f), FVector(D + 640.f, UniB1 - 10.f, Ceil - 10.f), int32((D + 1700.f) / 640.f) % 2 == 0);
    for (float D = -1600.f; D <= 1600.f; D += 400.f)
    {
        if (D > UniB0 - 100.f && D < UniB1 + 100.f) continue;
        Troffer(Glow, Clutter, FVector(UniMid, D, Ceil - .6f), false);
        Troffer(Glow, Clutter, FVector(D, UniMid, Ceil - .6f), true);
    }
    // The atrium: balustrades round its opening, the homecoming banner hung across it, a light ring overhead.
    const FBox2D A = UniAtrium();
    Balustrade(Solid, FVector2D(A.Min.X, A.Min.Y), FVector2D(A.Max.X, A.Min.Y), Z);
    Balustrade(Solid, FVector2D(A.Min.X, A.Max.Y), FVector2D(A.Max.X, A.Max.Y), Z);
    Balustrade(Solid, FVector2D(A.Min.X, A.Min.Y), FVector2D(A.Min.X, A.Max.Y), Z);
    Balustrade(Solid, FVector2D(A.Max.X, A.Min.Y), FVector2D(A.Max.X, A.Max.Y), Z);
    HangingSign(Clutter, TEXT("CI_UniBanner"), FVector(-82.5f, -82.5f, Z + 60.f), FVector(0, 1, 0), 420.f, 60.f, Ceil);
    for (int32 K = 0; K < 12; ++K)
    {
        const FVector2D P = FVector2D(-82.5f, -82.5f) + FVector2D(FMath::Cos(K * PI / 6.f), FMath::Sin(K * PI / 6.f)) * 150.f;
        HouseCampusFallKit::Ball(Glow, TEXT("CI_DomeLight"), FVector(P, Ceil - 40.f), 12.f, 10);
        Clutter.Cylinder(TEXT("CI_Brass"), FVector(P, Ceil - 30.f), FVector(P, Ceil), .6f, 6, UVPlain());
    }
}

// ------------------------------------------------------------------ the building
void BuildUnion(int32 Seed, float Height, FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit)
{
    const float F0 = HouseCampusInteriors::FloorTop(Height, 0), F1 = HouseCampusInteriors::FloorTop(Height, 1), Top = HouseCampusInteriors::OpenTop(Seed);
    const float C0 = F1 - 30.f, C1 = Top;
    const FBox2D In = Rect(-UniIn, UniIn, -UniIn, UniIn);
    const FBox2D NS = Rect(UniB0, UniB1, -UniIn, UniIn), WArm = Rect(-UniIn, UniB0, UniB0, UniB1), EArm = Rect(UniB1, UniIn, UniB0, UniB1);
    const FBox2D NW = Rect(-UniIn, UniB0, UniB1, UniIn);
    // Shell: plaster on the outer walls; terrazzo in the concourse, carpet in the lounge and bookstore and upstairs, vinyl tile
    // elsewhere; the first floor's slab open over the atrium and the stairs; acoustic tile ceilings.
    HouseCampusInteriors::OpenWalls(Solid, TEXT("CI_Plaster"), Seed, OuterSkin, WallT, UVPlain());
    FloorSlab(Solid, In, F0, 70.f, { NS, WArm, EArm, NW }, TEXT("CI_VCT"), TEXT("CI_Plaster"), FHallUV::World(61.f));
    for (const FBox2D& B : { NS, WArm, EArm }) FloorSlab(Solid, B, F0, 70.f, {}, TEXT("CI_Terrazzo"), TEXT("CI_Plaster"), FHallUV::World(220.f));
    FloorSlab(Solid, NW, F0, 70.f, {}, TEXT("CI_Carpet"), TEXT("CI_Plaster"), FHallUV::World(180.f));
    FloorSlab(Solid, In, F1, 30.f, { NS, WArm, EArm }, TEXT("CI_Carpet"), TEXT("CI_CeilingTile"), FHallUV::World(180.f));
    FloorSlab(Solid, NS, F1, 30.f, { UniAtrium(), UniWellS(), UniWellN() }, TEXT("CI_Terrazzo"), TEXT("CI_CeilingTile"), FHallUV::World(220.f));
    for (const FBox2D& B : { WArm, EArm }) FloorSlab(Solid, B, F1, 30.f, {}, TEXT("CI_Terrazzo"), TEXT("CI_CeilingTile"), FHallUV::World(220.f));
    Box(Solid, TEXT("CI_CeilingTile"), FVector(In.Min, C1 - .5f), FVector(In.Max, C1), FHallUV::World(122.f));
    // Oak wainscot on the outer walls (round the four doors downstairs) and oak stools and casings at the real windows.
    for (int32 K = 0; K < 2; ++K)
    {
        const float Z = K == 0 ? F0 : F1;
        const TArray<FVector2D> Gap = K == 0 ? TArray<FVector2D>{ FVector2D(-129.f, 129.f) } : TArray<FVector2D>{};
        Wainscot(Solid, true, In.Max.Y, -1.f, In.Min.X, In.Max.X, Z, Gap, 60.f);
        Wainscot(Solid, true, In.Min.Y, 1.f, In.Min.X, In.Max.X, Z, Gap, 60.f);
        Wainscot(Solid, false, In.Min.X, 1.f, In.Min.Y, In.Max.Y, Z, Gap, 60.f);
        Wainscot(Solid, false, In.Max.X, -1.f, In.Min.Y, In.Max.Y, Z, Gap, 60.f);
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
    // ---- partitions. The ground floor's stop at its ceiling (the slab's underside): where no wall stands on one upstairs, a
    // top flush with the floor above would show through it.
    const FName Wall = TEXT("CI_PlasterWall");
    const float H = PartT * .5f;
    // Along X: the arms' walls.
    Partition(Solid, Wall, false, UniB1, -UniIn, UniB0 + H, F0, C0, { FVector2D(-1800.f, -1600.f) });                       // TV lounge | west arm
    Partition(Solid, Wall, false, UniB1, UniB1 - H, UniIn, F0, C0, { FVector2D(300.f, 500.f) }, 260.f);                      // Goose Pit | east arm
    Partition(Solid, Wall, false, UniB0, -UniIn, UniB0 + H, F0, C0, { FVector2D(-1250.f, -1050.f) });                       // game room | west arm
    Partition(Solid, Wall, false, UniB0, UniB1 - H, UniIn, F0, C0, { FVector2D(500.f, 700.f), FVector2D(1500.f, 1592.f) }, 228.f);   // mailroom, sorting | east arm
    Partition(Solid, Wall, false, UniSplitY, -UniIn, UniB0 + H, F0, C0, {});                                               // TV lounge | bookstore
    // Along Y.
    Partition(Solid, Wall, true, UniB0, UniB1 - H, 1250.f, F0, C0, { FVector2D(450.f, 750.f) }, 260.f);                     // TV lounge | north arm
    Partition(Solid, Wall, true, UniB0, 1250.f, 1420.f, F0, C0, { FVector2D(1258.f, 1350.f) }, 213.f);                      // the bookstore's door
    {
        // The bookstore's glass front (Y 1420..1840): an oak base, plate glass, the transom wall over it.
        const float GY0 = 1420.f, GY1 = 1840.f;
        Box(Solid, TEXT("CI_Oak"), FVector(UniB0 - H, GY0, F0), FVector(UniB0 + H, GY1, F0 + 60.f));
        Box(Solid, TEXT("CI_Glass"), FVector(UniB0 - 1.f, GY0, F0 + 60.f), FVector(UniB0 + 1.f, GY1, F0 + 240.f));
        Box(Solid, Wall, FVector(UniB0 - H, GY0, F0 + 240.f), FVector(UniB0 + H, GY1, C0));
        for (float Y = GY0; Y <= GY1 + 1.f; Y += (GY1 - GY0) / 3.f) Box(Solid, TEXT("CI_OakDark"), FVector(UniB0 - H - 1.f, Y - 3.f, F0 + 60.f), FVector(UniB0 + H + 1.f, Y + 3.f, F0 + 240.f));
        Box(Solid, TEXT("CI_OakDark"), FVector(UniB0 - H - 1.f, GY0, F0 + 236.f), FVector(UniB0 + H + 1.f, GY1, F0 + 244.f));
        Partition(Solid, Wall, true, UniB0, GY1, UniIn, F0, C0, {});
    }
    Partition(Solid, Wall, true, UniB0, -UniIn, UniB0 + H, F0, C0, { FVector2D(-1880.f, -1700.f) }, 260.f);                 // game room | south arm
    Partition(Solid, Wall, true, UniB1, UniB1 - H, UniIn, F0, C0, { FVector2D(1400.f, 1800.f) }, 260.f);                     // Goose Pit | north arm
    Partition(Solid, Wall, true, UniB1, -UniIn, UniB0 + H, F0, C0, { FVector2D(-1100.f, -800.f) }, 260.f);                   // mailroom | south arm
    // The kitchen's pass-through and the mailroom's service window are openings in their walls.
    BlockWall(Solid, Wall, true, UniSplitX, UniB1 + H, UniIn, F0, C0, { FVector2D(1750.f, 1842.f) }, 213.f, { FBox2D(FVector2D(500.f, F0 + 155.f), FVector2D(1400.f, F0 + 235.f)) }, TEXT("CI_Steel"), UVPlain());
    BlockWall(Solid, Wall, true, UniSplitX, -UniIn, UniB0 - H, F0, C0, {}, 213.f, { FBox2D(FVector2D(-700.f, F0 + 98.f), FVector2D(-550.f, F0 + 200.f)) }, TEXT("CI_Steel"), UVPlain());
    // First floor.
    Partition(Solid, Wall, false, UniB1, -UniIn, UniB0 + H, F1, C1, { FVector2D(-1100.f, -1008.f) }, 213.f);
    Partition(Solid, Wall, false, UniB1, UniB1 - H, UniIn, F1, C1, { FVector2D(400.f, 492.f) }, 213.f);
    Partition(Solid, Wall, false, UniB0, -UniIn, UniB0 + H, F1, C1, { FVector2D(-1100.f, -1008.f) }, 213.f);
    Partition(Solid, Wall, false, UniB0, UniB1 - H, UniIn, F1, C1, { FVector2D(800.f, 892.f) }, 213.f);
    Partition(Solid, Wall, true, UniB0, UniB1 - H, UniIn, F1, C1, { FVector2D(600.f, 692.f) }, 213.f);
    Partition(Solid, Wall, true, UniB0, -UniIn, UniB0 + H, F1, C1, { FVector2D(-1700.f, -1608.f) }, 213.f);
    Partition(Solid, Wall, true, UniB1, UniB1 - H, UniIn, F1, C1, { FVector2D(1404.f, 1496.f) }, 213.f);
    Partition(Solid, Wall, true, UniB1, -UniIn, UniB0 + H, F1, C1, { FVector2D(-1100.f, -1008.f) }, 213.f);
    // ---- the stairs and the rooms
    UniStair(Solid, Clutter, UniWellS(), true, UniWellS().Max.X, F0, F1);
    UniStair(Solid, Clutter, UniWellN(), false, UniWellN().Min.X, F0, F1);
    UniGround(Solid, Clutter, Glow, Kit, F0, C0);
    UniUpstairs(Solid, Clutter, Glow, Kit, F1, C1);
}

void UniLightAnchors(int32 Seed, float Height, TArray<FVector>& Out)
{
    const float Z0 = HouseCampusInteriors::FloorTop(Height, 0) + 290.f, Z1 = HouseCampusInteriors::FloorTop(Height, 1) + 300.f;
    for (const FVector2D& P : { FVector2D(UniMid, 1300.f), FVector2D(UniMid, 650.f), FVector2D(UniMid, UniMid), FVector2D(UniMid, -800.f), FVector2D(UniMid, -1500.f),
        FVector2D(-1200.f, UniMid), FVector2D(1100.f, UniMid), FVector2D(-1185.f, 640.f), FVector2D(-1200.f, 1450.f), FVector2D(-1200.f, -1180.f), FVector2D(-700.f, -1600.f),
        FVector2D(800.f, 1100.f), FVector2D(800.f, 550.f), FVector2D(1640.f, 1000.f), FVector2D(800.f, -1200.f) })
        Out.Add(FVector(P, Z0));
    for (const FVector2D& P : { FVector2D(UniMid, 1300.f), FVector2D(UniMid, UniMid), FVector2D(UniMid, -1300.f), FVector2D(-1200.f, UniMid), FVector2D(1100.f, UniMid),
        FVector2D(-1200.f, 1100.f), FVector2D(-900.f, 600.f), FVector2D(1100.f, 1100.f), FVector2D(1100.f, 600.f), FVector2D(1600.f, 1600.f) })
        Out.Add(FVector(P, Z1));
}
void UniOpenings(int32 Seed, float Height, TArray<HouseCampusInteriors::FOpening>& Out)
{
    // A pair of glazed doors in the middle of every face: the main ones (the plan's west face) first.
    const float F0 = HouseCampusInteriors::FloorTop(Height, 0);
    for (int32 Side = 0; Side < 4; ++Side) Out.Add({ Side, -120.f, 120.f, F0, F0 + 213.f });
}
void UniDoors(int32 Seed, float Height, TArray<FDoorSpec>& Out)
{
    const FBox2D B = HouseCampusInteriors::CanonRect(Seed);
    TArray<HouseCampusInteriors::FOpening> Openings;
    UniOpenings(Seed, Height, Openings);
    MainDoorPair(TEXT("UNI"), B, Openings[0], Out);
    // Canonical sides 1, 2 and 3 face the plan's east, south and north.
    SidePair(TEXT("UNI"), TEXT("East"), B, Openings[1], FString(), Out);
    SidePair(TEXT("UNI"), TEXT("South"), B, Openings[2], FString(), Out);
    SidePair(TEXT("UNI"), TEXT("North"), B, Openings[3], FString(), Out);
    const float F0 = HouseCampusInteriors::FloorTop(Height, 0), F1 = HouseCampusInteriors::FloorTop(Height, 1);
    const uint8 Glazed = AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::KickPlate | AHouseBlackHallDoor::Closer;
    const uint8 Steel = AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::KickPlate;
    InnerDoor(Out, TEXT("Bookstore"), true, UniB0, 1258.f, 1350.f, F0, -1.f, EHallDoorStyle::French, Glazed, TEXT("CAMPUS BOOKSTORE - CLOSED SATURDAY, OPEN MONDAY 8 AM"));
    InnerDoor(Out, TEXT("Sorting"), false, UniB0, 1500.f, 1592.f, F0, -1.f, EHallDoorStyle::Equipment, Steel, TEXT("MAIL SORTING - STAFF ONLY"));
    InnerDoor(Out, TEXT("Committee"), true, UniB0, 600.f, 692.f, F1, -1.f, EHallDoorStyle::French, Glazed, FString(), true);
    InnerDoor(Out, TEXT("CommitteeWest"), false, UniB1, -1100.f, -1008.f, F1, 1.f, EHallDoorStyle::French, Glazed, FString(), true);
    InnerDoor(Out, TEXT("Ballroom"), true, UniB1, 1404.f, 1496.f, F1, 1.f, EHallDoorStyle::French, Glazed, FString(), true);
    InnerDoor(Out, TEXT("BallroomEast"), false, UniB1, 400.f, 492.f, F1, 1.f, EHallDoorStyle::French, Glazed, FString(), true);
    InnerDoor(Out, TEXT("StudentGov"), false, UniB0, -1100.f, -1008.f, F1, -1.f, EHallDoorStyle::Equipment, Steel, TEXT("STUDENT GOVERNMENT - BACK MONDAY"));
    InnerDoor(Out, TEXT("Honker"), true, UniB0, -1700.f, -1608.f, F1, -1.f, EHallDoorStyle::Equipment, Steel, TEXT("THE HONKER, THE STUDENT PAPER - DEADLINE SUNDAY 6 PM"));
    InnerDoor(Out, TEXT("MeetingEast"), false, UniB0, 800.f, 892.f, F1, -1.f, EHallDoorStyle::French, Glazed, TEXT("MEETING ROOMS - RESERVED FOR THE ALUMNI BOARD"));
    InnerDoor(Out, TEXT("MeetingSouth"), true, UniB1, -1100.f, -1008.f, F1, 1.f, EHallDoorStyle::French, Glazed, TEXT("MEETING ROOMS - RESERVED FOR THE ALUMNI BOARD"));
}
bool UniWindowOpen(int32 Seed, int32 Side, int32 K, float U0, float U1)
{
    // Locked rooms keep interior-mapped windows: the mail sorting room below; the offices and meeting rooms above.
    const float U = (U0 + U1) * .5f;
    if (K == 0) return !((Side == 1 && U > UniSplitX) || (Side == 3 && U < UniB0));
    if (Side == 1) return U > UniB0 && U < UniB1;
    if (Side >= 2) return U > UniB0;
    return true;
}

void UnionLook(UHouseBlackHallSubsystem* Look)
{
    if (!Look || Look->Materials.Contains(TEXT("CI_UniMenu"))) return;
    {
        // The college seal set in the concourse floor: a ring of lettering, a goose in the middle.
        FHallImage Img; Img.Init(512, 512, FLinearColor(.5f, .47f, .4f));
        Img.Ellipse(FVector2D(256, 256), FVector2D(250, 250), 0.f, FLinearColor(.78f, .58f, .18f));
        Img.Ellipse(FVector2D(256, 256), FVector2D(236, 236), 0.f, FLinearColor(.22f, .06f, .32f));
        Img.Ellipse(FVector2D(256, 256), FVector2D(160, 160), 0.f, FLinearColor(.78f, .58f, .18f));
        Img.Ellipse(FVector2D(256, 256), FVector2D(150, 150), 0.f, FLinearColor(.85f, .82f, .74f));
        FHallTextStyle S; S.Height = 34.f; S.Stroke = 5.f; S.Spacing = .2f;
        HallDrawText(Img, TEXT("CALDER STATE"), FVector2D((512.f - HallTextWidth(TEXT("CALDER STATE"), S)) * .5f, 80.f), S, FLinearColor(.85f, .7f, .25f));
        HallDrawText(Img, TEXT("1891"), FVector2D((512.f - HallTextWidth(TEXT("1891"), S)) * .5f, 470.f), S, FLinearColor(.85f, .7f, .25f));
        // A goose in profile: body, neck, head, beak.
        Img.Ellipse(FVector2D(250, 300), FVector2D(80, 40), -10.f, FLinearColor(.25f, .22f, .2f));
        Img.Stroke({ FVector2D(300, 285), FVector2D(320, 240), FVector2D(325, 200) }, 18.f, FLinearColor(.12f, .12f, .12f));
        Img.Ellipse(FVector2D(330, 190), FVector2D(18, 14), 0.f, FLinearColor(.12f, .12f, .12f));
        Img.FillPolygon({ FVector2D(345, 186), FVector2D(370, 192), FVector2D(345, 198) }, FLinearColor(.8f, .6f, .15f));
        Img.Ellipse(FVector2D(318, 222), FVector2D(10, 6), 0.f, FLinearColor(.85f, .82f, .74f));
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_UniSeal"), false);
        if (Tex) Look->Textures.Add(TEXT("CI_UniSeal"), Tex);
        HallMakeTextured(Look, TEXT("CI_UniSeal"), Tex, nullptr, .25f);
    }
    {
        // The cigarette machine's window: rows of packs in invented colours.
        FHallImage Img; Img.Init(128, 96, FLinearColor(.08f, .07f, .06f));
        FRandomStream R(57);
        for (int32 Row = 0; Row < 3; ++Row) for (int32 Col = 0; Col < 8; ++Col)
            Img.Rect(FVector2D(5 + Col * 15, 8 + Row * 28), FVector2D(17 + Col * 15, 30 + Row * 28), FLinearColor(R.FRandRange(.3f, .9f), R.FRandRange(.2f, .8f), R.FRandRange(.1f, .6f)));
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_UniCigWindow"), false);
        if (Tex) Look->Textures.Add(TEXT("CI_UniCigWindow"), Tex);
        HallMakeTextured(Look, TEXT("CI_UniCigWindow"), Tex, nullptr, .2f, FLinearColor::White, 0.f, 0.f, FLinearColor(1.2f, 1.1f, 1.f));
    }
    const FLinearColor Board(.06f, .13f, .09f), Chalk(.8f, .82f, .78f), SignBg(.08f, .06f, .04f), Gold(.78f, .58f, .18f), Cream(.85f, .82f, .74f), Purple(.22f, .05f, .32f);
    PaintText(Look, TEXT("CI_UniMenu"), 1024, 96, FLinearColor(.06f, .05f, .05f), FLinearColor(.95f, .9f, .75f),
        { { TEXT("THE GOOSE PIT   BURGER 1.25  FRIES .60  SHAKE .85  PIE .75  COFFEE .35"), .3f } }, true, FLinearColor(.6f, .55f, .45f));
    PaintText(Look, TEXT("CI_UniGoosePit"), 640, 100, Purple, Gold, { { TEXT("THE GOOSE PIT"), .45f } }, true);
    PaintText(Look, TEXT("CI_UniBanner"), 1024, 146, Cream, Purple, { { TEXT("WELCOME HOME ALUMNI"), .36f }, { TEXT("HOMECOMING 1986"), .26f } }, false);
    PaintText(Look, TEXT("CI_UniReunion"), 1024, 128, Cream, Purple, { { TEXT("WELCOME BACK CLASS OF 1961"), .32f }, { TEXT("25TH REUNION DINNER - 7 PM"), .22f } }, false);
    PaintText(Look, TEXT("CI_UniLantern"), 512, 104, SignBg, Gold, { { TEXT("THE OLD LANTERN"), .32f }, { TEXT("CALDER - DUNMORE  SINCE 1923"), .2f } }, true);
    PaintText(Look, TEXT("CI_UniCommittee"), 512, 190, Board, Chalk, { { TEXT("HOMECOMING COMMITTEE"), .14f }, { TEXT("FLOAT - FINISH HEAD BY FRI 6 PM"), .1f }, { TEXT("BONFIRE CREW THURS 9 PM"), .1f } }, false);
    PaintText(Look, TEXT("CI_UniPoolLadder"), 384, 220, Board, Chalk, { { TEXT("POOL LADDER"), .14f }, { TEXT("1 R.K.   2 DAWSON"), .1f }, { TEXT("3 T.T.   4 MO"), .1f } }, false);
    PaintText(Look, TEXT("CI_BackglassHalftime"), 256, 256, FLinearColor(.05f, .2f, .08f), FLinearColor(.95f, .85f, .3f), { { TEXT("HALF"), .22f }, { TEXT("TIME"), .22f } }, true, FLinearColor(2.f, 1.6f, 1.2f));
    PaintText(Look, TEXT("CI_BackglassMoon"), 256, 256, FLinearColor(.04f, .04f, .18f), FLinearColor(.9f, .9f, .95f), { { TEXT("MOON"), .22f }, { TEXT("RANCH"), .22f } }, true, FLinearColor(1.6f, 1.6f, 2.f));
    PaintText(Look, TEXT("CI_UniWindowClosed"), 256, 180, Cream, FLinearColor(.12f, .1f, .1f), { { TEXT("WINDOW"), .16f }, { TEXT("CLOSED"), .16f }, { TEXT("SATURDAY"), .12f } }, true);
    PaintText(Look, TEXT("CI_UniBeatBanner"), 640, 240, FLinearColor(.85f, .83f, .78f), Purple, { { TEXT("BEAT DUN"), .3f } }, false);
    SciPaintLines(Look, TEXT("CI_UniDirectory"), 256, 360, FLinearColor(.05f, .05f, .06f), Cream, {
        TEXT("THE GOOSE UNION"), TEXT(""), TEXT("GOOSE PIT"), TEXT("TV LOUNGE"), TEXT("GAME ROOM"), TEXT("CAMPUS MAIL"), TEXT("BOOKSTORE"), TEXT("INFORMATION"), TEXT(""),
        TEXT("UPSTAIRS"), TEXT("CALDER BALLROOM"), TEXT("STUDENT GOVERNMENT"), TEXT("THE HONKER"), TEXT("MEETING ROOMS") }, 25.f);
    PaintText(Look, TEXT("CI_UniBookstore"), 640, 100, SignBg, Gold, { { TEXT("CAMPUS BOOKSTORE"), .42f } }, true);
    PaintText(Look, TEXT("CI_UniInfo"), 512, 88, SignBg, Gold, { { TEXT("INFORMATION"), .45f } }, true);
    PaintText(Look, TEXT("CI_UniMail"), 384, 92, SignBg, Cream, { { TEXT("CAMPUS MAIL"), .42f } }, true);
    PaintText(Look, TEXT("CI_UniGameRoom"), 384, 92, SignBg, Cream, { { TEXT("GAME ROOM"), .42f } }, true);
    PaintText(Look, TEXT("CI_UniTVLounge"), 384, 92, SignBg, Cream, { { TEXT("TV LOUNGE"), .42f } }, true);
    PaintText(Look, TEXT("CI_UniRideBoard"), 384, 84, Purple, Gold, { { TEXT("RIDE BOARD"), .42f } }, true);
    PaintText(Look, TEXT("CI_UniCommitteeSign"), 320, 104, SignBg, Cream, { { TEXT("HOMECOMING"), .26f }, { TEXT("COMMITTEE"), .26f } }, true);
    PaintText(Look, TEXT("CI_UniBallroomSign"), 320, 104, SignBg, Gold, { { TEXT("CALDER"), .28f }, { TEXT("BALLROOM"), .28f } }, true);
    PaintText(Look, TEXT("CI_UniGovSign"), 320, 104, SignBg, Cream, { { TEXT("STUDENT"), .26f }, { TEXT("GOVERNMENT"), .26f } }, true);
    PaintText(Look, TEXT("CI_UniHonkerSign"), 320, 104, SignBg, Cream, { { TEXT("THE HONKER"), .28f }, { TEXT("STUDENT PAPER"), .2f } }, true);
    PaintText(Look, TEXT("CI_UniMeetingSign"), 320, 104, SignBg, Cream, { { TEXT("MEETING"), .28f }, { TEXT("ROOMS"), .28f } }, true);
    HallMakePlain(Look, TEXT("CI_Tablecloth"), FLinearColor(.82f, .8f, .76f), .85f);
    HallMakePlain(Look, TEXT("CI_Bun"), FLinearColor(.6f, .38f, .15f), .7f);
    HallMakePlain(Look, TEXT("CI_Patty"), FLinearColor(.18f, .09f, .05f), .8f);
    HallMakePlain(Look, TEXT("CI_Fries"), FLinearColor(.75f, .55f, .2f), .8f);
    HallMakePlain(Look, TEXT("CI_Ketchup"), FLinearColor(.5f, .03f, .02f), .3f);
    HallMakePlain(Look, TEXT("CI_UniJukeGlow"), FLinearColor(.9f, .5f, .2f), .3f, 0.f, FLinearColor(4.f, 2.f, .8f));
}
/** The TV lounge's console set, still on the replay. */
void UnionSounds(int32 Seed, float Height, TArray<FSoundSpot>& Out)
{
    Out.Add({ 1, FVector(-1185.f, UniSplitY - PartT * .5f - 26.f, HouseCampusInteriors::FloorTop(Height, 0) + 50.f) });
}
FInteriorDef UnionDef()
{
    FInteriorDef D{ TEXT("UNI"), 2, &BuildUnion, &UniLightAnchors, &UniOpenings, &UniDoors, &UnionLook };
    D.WindowOpen = &UniWindowOpen;
    D.Sounds = &UnionSounds;
    return D;
}
}
