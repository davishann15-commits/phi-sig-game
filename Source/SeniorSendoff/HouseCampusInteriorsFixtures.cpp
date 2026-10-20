// Calder State's 1986 fixtures that the house has no mesh for (master job, step 2c): CRT televisions, chalkboards,
// tablet-arm desks, lockers, vending machines, payphones, post office boxes, lab benches and fume hoods, a pinball
// machine, washers and dryers, bulletin boards. All runtime FHallMesh geometry in a building's canonical frame; lit
// screens and panels glow by emission (the Glow mesh), never by a light. Declared in HouseCampusInteriorsRooms.h; their
// materials are added by FixturesLook.
#include "HouseCampusInteriorsRooms.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusFallKit.h"
#include "Engine/Texture2D.h"

namespace HouseCampusInteriorsImpl
{
/** A fixture's own frame: Origin on the floor at its footprint's centre, Front the way it faces, Right across it. */
struct FFixFrame
{
    FVector Origin, Front, Right;
    FRotator Rot;
    FFixFrame(const FVector& InOrigin, const FVector& Facing) : Origin(InOrigin)
    {
        Front = Facing.GetSafeNormal2D();
        Right = FVector(-Front.Y, Front.X, 0.f);
        Rot = Front.Rotation();
    }
    FVector P(float F, float R, float Z) const { return Origin + Front * F + Right * R + FVector(0, 0, Z); }
    /** A box centred at (F, R, Z) with half extents along front, right, up. */
    void B(FHallMesh& M, FName K, float F, float R, float Z, float HF, float HR, float HZ) const { M.Box(K, FTransform(Rot, P(F, R, Z)), FVector(HF, HR, HZ), UVPlain()); }
    /** A quad on the plane facing Front at depth F, spanning R0..R1 and Z0..Z1, with 0..1 UVs (u across, v down). Right is
     *  the fixture's own right, so the viewer's is -Right: u runs from R1 to R0, and painted lettering reads. */
    void Face(FHallMesh& M, FName K, float F, float R0, float R1, float Z0, float Z1) const
    {
        M.QuadUV(K, P(F, R0, Z0), P(F, R1, Z0), P(F, R1, Z1), P(F, R0, Z1), Front, FVector2D(1, 1), FVector2D(0, 1), FVector2D(0, 0), FVector2D(1, 0), -Right);
    }
};

void CrtTelevision(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing, bool bConsole)
{
    const FFixFrame F(C, Facing);
    if (bConsole)
    {
        // A 25-inch wood-grain console: the cabinet on short legs, a speaker grille beside the tube, two knobs.
        for (const float R : { -42.f, 42.f }) for (const float D : { -18.f, 18.f }) F.B(Solid, TEXT("CI_OakDark"), D, R, 4.f, 2.f, 2.f, 4.f);
        F.B(Solid, TEXT("CI_Walnut"), 0.f, 0.f, 46.f, 24.f, 50.f, 38.f);
        F.B(Solid, TEXT("CI_CrtBezel"), 24.5f, -10.f, 50.f, .6f, 33.f, 26.f);
        F.Face(Glow, TEXT("CI_CrtScreen"), 25.3f, -36.f, 16.f, 31.f, 69.f);
        F.B(Solid, TEXT("CI_Grille"), 24.6f, 34.f, 46.f, .5f, 11.f, 30.f);
        for (const float Z : { 66.f, 56.f }) Solid.Cylinder(TEXT("CI_Brass"), F.P(25.f, 34.f, Z), F.P(28.f, 34.f, Z), 2.4f, 10, UVPlain());
        return;
    }
    // A 13-inch portable: a beige plastic box, the tube's bulge behind, rabbit ears.
    F.B(Solid, TEXT("CI_Beige"), 0.f, 0.f, 18.f, 18.f, 20.f, 18.f);
    F.B(Solid, TEXT("CI_Beige"), -22.f, 0.f, 17.f, 6.f, 14.f, 13.f);
    F.Face(Glow, TEXT("CI_CrtScreen"), 18.4f, -15.f, 9.f, 6.f, 30.f);
    for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Bar(Solid, TEXT("CI_Steel"), F.P(-6.f, 0.f, 36.f), F.P(-10.f, S * 18.f, 64.f), .6f, .6f, F.Front);
}

void Chalkboard(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Out, float W, float H, FName Writing)
{
    // A slate-green board in an oak frame, its chalk tray with stubs and an eraser; Writing is the painted board (CI_Chalk*).
    const FFixFrame F(C, Out);
    F.B(Solid, TEXT("CI_Oak"), 1.f, 0.f, 0.f, 1.f, W * .5f + 5.f, H * .5f + 5.f);
    F.Face(Clutter, Writing.IsNone() ? FName(TEXT("CI_ChalkBlank")) : Writing, 2.1f, -W * .5f, W * .5f, -H * .5f, H * .5f);
    F.B(Solid, TEXT("CI_OakDark"), 5.f, 0.f, -H * .5f - 4.f, 5.f, W * .5f, 1.5f);
    for (const float R : { -60.f, -20.f, 45.f }) F.B(Clutter, TEXT("CI_Paper"), 6.f, R, -H * .5f - 2.f, .6f, 4.f, .6f);
    F.B(Clutter, TEXT("CI_Felt"), 6.f, W * .3f, -H * .5f - 1.5f, 2.5f, 7.f, 1.5f);
}

void TabletDesk(FHallMesh& Solid, const FVector& C, const FVector& Facing)
{
    // A 1980s tablet-arm chair: tubular steel legs, a moulded seat and back, the writing tablet on the right arm.
    const FFixFrame F(C, Facing);
    for (const float D : { -17.f, 17.f }) for (const float R : { -19.f, 19.f }) F.B(Solid, TEXT("CI_Steel"), D, R, 21.f, 1.2f, 1.2f, 21.f);
    F.B(Solid, TEXT("CI_Moulded"), 0.f, 0.f, 44.f, 21.f, 22.f, 2.f);
    F.B(Solid, TEXT("CI_Moulded"), -21.f, 0.f, 68.f, 2.f, 21.f, 16.f);
    F.B(Solid, TEXT("CI_Steel"), -21.f, 0.f, 50.f, 1.f, 1.f, 6.f);
    F.B(Solid, TEXT("CI_Laminate"), 14.f, 18.f, 72.f, 22.f, 13.f, 1.f);
    F.B(Solid, TEXT("CI_Steel"), 4.f, 26.f, 58.f, 1.f, 1.f, 14.f);
}

void Lockers(FHallMesh& Solid, FHallMesh& Clutter, const FVector& A, const FVector& Along, int32 Count, const FVector& Out)
{
    // A bank of steel hall lockers, 38 cm each, 180 tall, louvres and a handle on every door, on a base.
    const FVector U = Along.GetSafeNormal2D();
    for (int32 K = 0; K < Count; ++K)
    {
        const FFixFrame F(A + U * (19.f + 38.f * K) + Out.GetSafeNormal2D() * 25.f, Out);
        F.B(Solid, TEXT("CI_LockerSteel"), 0.f, 0.f, 96.f, 25.f, 18.8f, 84.f);
        F.B(Solid, TEXT("CI_OakDark"), 0.f, 0.f, 6.f, 25.f, 19.f, 6.f);
        for (int32 L = 0; L < 4; ++L) F.B(Clutter, TEXT("CI_Rubber"), 25.3f, 0.f, 160.f + L * 4.f, .3f, 9.f, .6f);
        F.B(Clutter, TEXT("CI_Steel"), 25.6f, 12.f, 110.f, .6f, 1.2f, 6.f);
        F.B(Clutter, TEXT("CI_Paper"), 25.4f, 0.f, 172.f, .2f, 5.f, 2.f);
    }
}

void VendingMachine(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing, bool bSnacks)
{
    // A cold-drink or snack machine: the cabinet, a lit front panel (invented, unbranded), the coin plate, the delivery bin.
    const FFixFrame F(C, Facing);
    F.B(Solid, TEXT("CI_VendingRed"), 0.f, 0.f, 92.f, 40.f, 46.f, 92.f);
    F.Face(Glow, bSnacks ? FName(TEXT("CI_VendingSnacks")) : FName(TEXT("CI_VendingDrinks")), 40.6f, -40.f, 20.f, 60.f, 176.f);
    F.B(Solid, TEXT("CI_Steel"), 41.f, 32.f, 120.f, 1.f, 8.f, 22.f);
    F.B(Solid, TEXT("CI_Rubber"), 41.f, -10.f, 22.f, 1.f, 28.f, 9.f);
}

void PayPhone(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Out)
{
    // A wall payphone in a steel housing, the handset on its hook and the armoured cord.
    const FFixFrame F(C, Out);
    F.B(Solid, TEXT("CI_Steel"), 6.f, 0.f, 0.f, 6.f, 13.f, 26.f);
    F.B(Clutter, TEXT("CI_Rubber"), 12.5f, 0.f, 4.f, .4f, 8.f, 12.f);
    F.B(Solid, TEXT("CI_Rubber"), 13.f, -16.f, 2.f, 3.f, 3.f, 11.f);
    HouseCampusFallKit::Bar(Clutter, TEXT("CI_Steel"), F.P(12.f, -16.f, -9.f), F.P(12.f, -8.f, -18.f), .6f, .6f, F.Front);
}

void MailboxWall(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Out, float W, float H)
{
    // A wall of brass post office boxes (the painted face, CI_PoBoxes), framed in oak.
    const FFixFrame F(C, Out);
    F.B(Solid, TEXT("CI_OakDark"), 2.f, 0.f, 0.f, 2.f, W * .5f + 4.f, H * .5f + 4.f);
    F.Face(Clutter, TEXT("CI_PoBoxes"), 4.1f, -W * .5f, W * .5f, -H * .5f, H * .5f);
}

void LabBench(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Along, float L)
{
    // A teaching-lab island: oak cabinets under a black epoxy top, a sink, gooseneck taps and gas cocks down the middle.
    const FFixFrame F(C, FVector(-Along.Y, Along.X, 0.f));
    F.B(Solid, TEXT("CI_Oak"), 0.f, 0.f, 43.f, 60.f, L * .5f, 43.f);
    F.B(Solid, TEXT("CI_Epoxy"), 0.f, 0.f, 89.f, 65.f, L * .5f + 4.f, 3.f);
    F.B(Clutter, TEXT("CI_Steel"), 0.f, 0.f, 92.3f, 15.f, 20.f, .3f);
    for (float R = -L * .5f + 40.f; R < L * .5f - 20.f; R += 80.f)
    {
        Solid.Cylinder(TEXT("CI_Brass"), F.P(0.f, R, 92.f), F.P(0.f, R, 120.f), 1.f, 8, UVPlain());
        HouseCampusFallKit::Bar(Clutter, TEXT("CI_Brass"), F.P(0.f, R, 120.f), F.P(14.f, R, 112.f), .8f, .8f, F.Front);
        for (const float S : { -1.f, 1.f }) F.B(Clutter, TEXT("CI_Brass"), S * 30.f, R + 12.f, 95.f, 2.f, 1.f, 3.f);
    }
}

void FumeHood(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Out)
{
    // A fume hood against a wall: the steel body, the sash glass, the lit work surface inside, the exhaust duct up.
    const FFixFrame F(C, Out);
    F.B(Solid, TEXT("CI_Beige"), 0.f, 0.f, 45.f, 38.f, 75.f, 45.f);
    F.B(Solid, TEXT("CI_Epoxy"), 0.f, 0.f, 92.f, 40.f, 77.f, 2.f);
    F.B(Solid, TEXT("CI_Beige"), -30.f, 0.f, 160.f, 8.f, 75.f, 66.f);
    for (const float S : { -1.f, 1.f }) F.B(Solid, TEXT("CI_Beige"), 0.f, S * 72.f, 160.f, 38.f, 3.f, 66.f);
    F.B(Solid, TEXT("CI_Beige"), 0.f, 0.f, 218.f, 38.f, 75.f, 8.f);
    F.Face(Glow, TEXT("CI_HoodLight"), -21.f, -66.f, 66.f, 96.f, 206.f);
    F.Face(Solid, TEXT("CI_GlassSash"), 37.f, -70.f, 70.f, 135.f, 205.f);
    Solid.Cylinder(TEXT("CI_Steel"), F.P(-20.f, 0.f, 226.f), F.P(-20.f, 0.f, 300.f), 16.f, 12, UVPlain());
}

void Pinball(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing, FName Backglass)
{
    // A pinball machine: the cabinet on four legs, the lit playfield under glass, the backbox with its painted backglass.
    const FFixFrame F(C, Facing);
    for (const float D : { -50.f, 50.f }) for (const float R : { -27.f, 27.f }) F.B(Solid, TEXT("CI_Steel"), D, R, 40.f, 2.f, 2.f, 40.f);
    F.B(Solid, TEXT("CI_PinballBody"), 0.f, 0.f, 90.f, 60.f, 30.f, 12.f);
    Glow.QuadUV(TEXT("CI_Playfield"), F.P(58.f, -27.f, 99.f), F.P(58.f, 27.f, 99.f), F.P(-56.f, 27.f, 105.f), F.P(-56.f, -27.f, 105.f), FVector::UpVector,
        FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0), F.Right);
    F.B(Solid, TEXT("CI_PinballBody"), -62.f, 0.f, 145.f, 7.f, 30.f, 40.f);
    F.Face(Glow, Backglass.IsNone() ? FName(TEXT("CI_Backglass")) : Backglass, -54.8f, -27.f, 27.f, 110.f, 180.f);
}

void Laundry(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing, bool bDryer)
{
    // A coin-op washer (top loader) or dryer (front door), enamel white, with its coin slide.
    const FFixFrame F(C, Facing);
    F.B(Solid, TEXT("CI_Enamel"), 0.f, 0.f, 46.f, 33.f, 34.f, 46.f);
    F.B(Solid, TEXT("CI_Enamel"), -26.f, 0.f, 100.f, 7.f, 34.f, 10.f);
    F.B(Solid, TEXT("CI_Steel"), -19.f, 12.f, 100.f, .6f, 8.f, 3.f);
    if (bDryer) Solid.Cylinder(TEXT("CI_GlassSash"), F.P(33.f, 0.f, 50.f), F.P(34.f, 0.f, 50.f), 19.f, 20, UVPlain());
    else F.B(Solid, TEXT("CI_Steel"), 4.f, 0.f, 92.3f, 22.f, 26.f, .3f);
    F.B(Glow, TEXT("CI_PilotLamp"), -18.6f, -20.f, 104.f, .4f, 1.2f, 1.2f);
}

void BulletinBoard(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Out, float W, float H, FName Pinned)
{
    // Cork in an aluminium frame, covered in notes and flyers (Pinned: CI_RideBoard, CI_NoticeBoard...).
    const FFixFrame F(C, Out);
    F.B(Solid, TEXT("CI_Steel"), 1.f, 0.f, 0.f, 1.f, W * .5f + 2.f, H * .5f + 2.f);
    F.Face(Clutter, Pinned.IsNone() ? FName(TEXT("CI_NoticeBoard")) : Pinned, 2.1f, -W * .5f, W * .5f, -H * .5f, H * .5f);
}

void ExitSign(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing)
{
    // A 1986 incandescent EXIT box over a door, lit by emission (both faces), hung from the ceiling or the wall.
    const FFixFrame F(C, Facing);
    F.B(Solid, TEXT("CI_ExitBox"), 0.f, 0.f, 0.f, 4.f, 18.f, 11.f);
    for (const float S : { -1.f, 1.f })
    {
        const FFixFrame G(C, Facing * S);
        G.Face(Glow, TEXT("CI_ExitFace"), 4.1f, -16.f, 16.f, -9.f, 9.f);
    }
}

void Radiator(FHallMesh& Solid, const FVector& C, const FVector& Out, float W)
{
    // A cast-iron column radiator under a window: sections every 6 cm on short feet, painted the wall colour, a valve.
    const FFixFrame F(C, Out);
    for (float R = -W * .5f + 3.f; R <= W * .5f - 3.f; R += 6.f) F.B(Solid, TEXT("CI_RadiatorPaint"), 9.f, R, 46.f, 7.f, 2.2f, 34.f);
    F.B(Solid, TEXT("CI_RadiatorPaint"), 9.f, 0.f, 14.f, 6.f, W * .5f, 2.f);
    F.B(Solid, TEXT("CI_RadiatorPaint"), 9.f, 0.f, 79.f, 6.f, W * .5f, 2.f);
    for (const float S : { -1.f, 1.f }) F.B(Solid, TEXT("CI_Steel"), 9.f, S * (W * .5f - 6.f), 6.f, 5.f, 2.f, 6.f);
    Solid.Cylinder(TEXT("CI_Brass"), F.P(9.f, W * .5f + 4.f, 20.f), F.P(9.f, W * .5f + 4.f, 30.f), 2.f, 8, UVPlain());
}

void WaterFountain(FHallMesh& Solid, const FVector& C, const FVector& Out)
{
    // A wall-hung stainless drinking fountain, its push button and bubbler.
    const FFixFrame F(C, Out);
    F.B(Solid, TEXT("CI_Steel"), 18.f, 0.f, 88.f, 18.f, 22.f, 8.f);
    F.B(Solid, TEXT("CI_Steel"), 6.f, 0.f, 60.f, 6.f, 12.f, 22.f);
    F.B(Solid, TEXT("CI_Enamel"), 22.f, 0.f, 96.6f, 12.f, 16.f, .5f);
    Solid.Cylinder(TEXT("CI_Brass"), F.P(16.f, 0.f, 96.f), F.P(16.f, 0.f, 102.f), 1.2f, 8, UVPlain());
    F.B(Solid, TEXT("CI_Brass"), 36.5f, 0.f, 90.f, .6f, 3.f, 2.f);
}

void AshUrn(FHallMesh& Solid, const FVector& C)
{
    // A standing chrome ash urn with its sand-filled top.
    Solid.Cylinder(TEXT("CI_Steel"), C, C + FVector(0, 0, 62.f), 15.f, 14, UVPlain());
    Solid.Cylinder(TEXT("CI_Steel"), C + FVector(0, 0, 62.f), C + FVector(0, 0, 66.f), 17.f, 14, UVPlain());
    Solid.Cylinder(TEXT("CI_Sand"), C + FVector(0, 0, 66.f), C + FVector(0, 0, 66.4f), 15.f, 14, UVPlain());
}

void OverheadProjector(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing)
{
    // An overhead projector on its cart: the lit stage, the arm and head, a transparency on the glass.
    const FFixFrame F(C, Facing);
    F.B(Solid, TEXT("CI_Steel"), 0.f, 0.f, 40.f, 30.f, 22.f, 40.f);
    F.B(Solid, TEXT("CI_Beige"), 0.f, 0.f, 92.f, 20.f, 20.f, 12.f);
    Glow.QuadUV(TEXT("CI_ProjectorStage"), F.P(-16.f, -16.f, 104.2f), F.P(-16.f, 16.f, 104.2f), F.P(16.f, 16.f, 104.2f), F.P(16.f, -16.f, 104.2f), FVector::UpVector,
        FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0), F.Front);
    F.B(Solid, TEXT("CI_Steel"), -18.f, 0.f, 135.f, 1.5f, 1.5f, 30.f);
    F.B(Solid, TEXT("CI_CrtBezel"), -10.f, 0.f, 166.f, 10.f, 8.f, 6.f);
}

void Terminal(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing)
{
    // A beige green-screen terminal and its keyboard (the screen glows by emission).
    const FFixFrame F(C, Facing);
    F.B(Solid, TEXT("CI_Beige"), -6.f, 0.f, 18.f, 20.f, 19.f, 18.f);
    F.Face(Glow, TEXT("CI_GreenScreen"), 14.1f, -14.f, 14.f, 7.f, 31.f);
    F.B(Solid, TEXT("CI_Beige"), 30.f, 0.f, 2.f, 9.f, 22.f, 2.f);
}

void Typewriter(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Facing)
{
    // An office electric typewriter with a sheet in the platen.
    const FFixFrame F(C, Facing);
    F.B(Solid, TEXT("CI_TypewriterBlue"), 0.f, 0.f, 6.f, 18.f, 23.f, 6.f);
    F.B(Solid, TEXT("CI_TypewriterBlue"), -12.f, 0.f, 14.f, 6.f, 25.f, 4.f);
    F.B(Clutter, TEXT("CI_Paper"), -14.f, 0.f, 24.f, .2f, 10.5f, 12.f);
    F.B(Clutter, TEXT("CI_Rubber"), 6.f, 0.f, 12.1f, 10.f, 18.f, .2f);
}

void DeskPhone(FHallMesh& Solid, const FVector& C, const FVector& Facing, bool bRotary)
{
    // A desk telephone: the body, a rotary dial or push buttons, the handset in its cradle.
    const FFixFrame F(C, Facing);
    F.B(Solid, TEXT("CI_PhoneBeige"), 0.f, 0.f, 4.f, 10.f, 10.f, 4.f);
    if (bRotary) Solid.Cylinder(TEXT("CI_Steel"), F.P(4.f, 0.f, 8.f), F.P(4.5f, 0.f, 8.4f), 4.f, 12, UVPlain());
    else F.B(Solid, TEXT("CI_Enamel"), 4.f, 0.f, 8.1f, 3.f, 3.f, .2f);
    F.B(Solid, TEXT("CI_PhoneBeige"), -3.f, 0.f, 10.f, 3.f, 11.f, 2.f);
}

void DirectoryBoard(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Out, FName Board)
{
    // A building directory: black felt letter board in an oak frame behind glass (the painted listing, Board).
    const FFixFrame F(C, Out);
    F.B(Solid, TEXT("CI_OakDark"), 2.f, 0.f, 0.f, 2.f, 52.f, 72.f);
    F.Face(Clutter, Board.IsNone() ? FName(TEXT("CI_ChalkBlank")) : Board, 4.1f, -48.f, 48.f, -68.f, 68.f);
}

void Boombox(FHallMesh& Solid, const FVector& C, const FVector& Facing)
{
    // A twin-speaker cassette boombox with its handle.
    const FFixFrame F(C, Facing);
    F.B(Solid, TEXT("CI_CrtBezel"), 0.f, 0.f, 13.f, 7.f, 28.f, 13.f);
    for (const float S : { -1.f, 1.f }) Solid.Cylinder(TEXT("CI_Grille"), F.P(7.f, S * 16.f, 12.f), F.P(7.5f, S * 16.f, 12.f), 8.f, 14, UVPlain());
    F.B(Solid, TEXT("CI_Steel"), 7.2f, 0.f, 16.f, .3f, 5.f, 3.f);
    HouseCampusFallKit::Bar(Solid, TEXT("CI_Steel"), F.P(0.f, -20.f, 26.f), F.P(0.f, 20.f, 26.f), 1.2f, 1.2f, F.Front);
}

void MiniFridge(FHallMesh& Solid, const FVector& C, const FVector& Facing)
{
    // A dorm mini fridge, wood-grain door, a hot plate and a pan on top.
    const FFixFrame F(C, Facing);
    F.B(Solid, TEXT("CI_Enamel"), 0.f, 0.f, 40.f, 24.f, 24.f, 40.f);
    F.B(Solid, TEXT("CI_Walnut"), 24.4f, 0.f, 40.f, .4f, 22.f, 37.f);
    F.B(Solid, TEXT("CI_Steel"), 0.f, 0.f, 83.f, 14.f, 18.f, 3.f);
    Solid.Cylinder(TEXT("CI_Rubber"), F.P(0.f, -6.f, 86.f), F.P(0.f, -6.f, 87.f), 7.f, 12, UVPlain());
    Solid.Cylinder(TEXT("CI_Steel"), F.P(0.f, -6.f, 87.f), F.P(0.f, -6.f, 93.f), 8.f, 12, UVPlain());
}

void MilkCrateShelf(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Facing, int32 Seed)
{
    // Milk crates stacked as shelves, records and paperbacks in them.
    const FFixFrame F(C, Facing);
    const FName Crates[] = { TEXT("CI_CrateRed"), TEXT("CI_CrateBlue"), TEXT("CI_CrateRed") };
    for (int32 Row = 0; Row < 3; ++Row)
        for (int32 Col = 0; Col < 2; ++Col)
        {
            const float R = (Col - .5f) * 34.f, Z = 14.f + Row * 28.f;
            const FName K = Crates[(Row + Col + Seed) % 3];
            F.B(Solid, K, 0.f, R, Z - 13.f, 16.f, 16.f, 1.f);
            F.B(Solid, K, -15.f, R, Z, 1.f, 16.f, 13.f);
            for (const float S : { -1.f, 1.f }) F.B(Solid, K, 0.f, R + S * 15.f, Z, 16.f, 1.f, 13.f);
            F.Face(Clutter, TEXT("CI_Books"), 4.f, R - 14.f, R + 14.f, Z - 12.f, Z + 9.f);
        }
}

void Helmet(FHallMesh& Solid, const FVector& C, const FVector& Facing)
{
    // A football helmet (purple shell, gold stripe, a cage) sitting on a bench or the floor.
    const FFixFrame F(C, Facing);
    Solid.Dome(TEXT("CI_HelmetPurple"), F.P(0.f, 0.f, 4.f), FVector::UpVector, 13.f, 17.f, 12);
    F.B(Solid, TEXT("CI_Brass"), 0.f, 0.f, 16.f, 12.f, 1.2f, 4.f);
    for (const float Z : { 6.f, 10.f }) HouseCampusFallKit::Bar(Solid, TEXT("CI_Steel"), F.P(13.f, -9.f, Z), F.P(13.f, 9.f, Z), .6f, .6f, F.Front);
}

void Streamer(FHallMesh& Clutter, const FVector& A, const FVector& B, bool bGold)
{
    // A twisted crepe-paper streamer sagging between two points.
    const int32 N = 10;
    FVector Prev = A;
    for (int32 K = 1; K <= N; ++K)
    {
        const float T = float(K) / N;
        const FVector P = FMath::Lerp(A, B, T) - FVector(0, 0, FMath::Sin(T * PI) * FVector::Dist(A, B) * .08f);
        HouseCampusFallKit::Bar(Clutter, bGold ? FName(TEXT("CI_StreamerGold")) : FName(TEXT("CI_StreamerPurple")), Prev, P, 4.f, .2f, FVector(FMath::Cos(K * 1.3f), FMath::Sin(K * 1.3f), .3f).GetSafeNormal());
        Prev = P;
    }
}

UTexture2D* PaintFixtureTexture(UHouseBlackHallSubsystem* Look, FName Key, const FHallImage& Img)
{
    UTexture2D* Tex = HallMakeTexture(Img, true, false, *Key.ToString(), false);
    if (Tex) Look->Textures.Add(Key, Tex);
    return Tex;
}

void FixturesLook(UHouseBlackHallSubsystem* Look)
{
    if (!Look || Look->Materials.Contains(TEXT("CI_CrtScreen"))) return;
    HallMakePlain(Look, TEXT("CI_Walnut"), FLinearColor(.16f, .08f, .035f), .45f);
    HallMakePlain(Look, TEXT("CI_CrtBezel"), FLinearColor(.03f, .03f, .03f), .4f);
    HallMakePlain(Look, TEXT("CI_Grille"), FLinearColor(.08f, .06f, .04f), .95f);
    HallMakePlain(Look, TEXT("CI_Moulded"), FLinearColor(.32f, .17f, .06f), .45f);     // harvest-orange plastic
    HallMakePlain(Look, TEXT("CI_Laminate"), FLinearColor(.45f, .38f, .26f), .5f);
    HallMakePlain(Look, TEXT("CI_LockerSteel"), FLinearColor(.12f, .2f, .3f), .45f, .4f);
    HallMakePlain(Look, TEXT("CI_VendingRed"), FLinearColor(.45f, .04f, .03f), .35f, .2f);
    HallMakePlain(Look, TEXT("CI_Epoxy"), FLinearColor(.02f, .02f, .02f), .3f);
    HallMakePlain(Look, TEXT("CI_Felt"), FLinearColor(.12f, .12f, .12f), .95f);
    HallMakePlain(Look, TEXT("CI_PinballBody"), FLinearColor(.08f, .02f, .12f), .4f);
    HallMakePlain(Look, TEXT("CI_Enamel"), FLinearColor(.72f, .72f, .68f), .3f);
    HallMakePlain(Look, TEXT("CI_ExitBox"), FLinearColor(.5f, .48f, .42f), .5f);
    HallMakePlain(Look, TEXT("CI_GlassSash"), FLinearColor(.05f, .07f, .08f), .08f, .2f);
    HallMakePlain(Look, TEXT("CI_ChalkBlank"), FLinearColor(.06f, .13f, .09f), .85f);
    HallMakePlain(Look, TEXT("CI_RadiatorPaint"), FLinearColor(.5f, .48f, .42f), .55f, .2f);
    HallMakePlain(Look, TEXT("CI_Sand"), FLinearColor(.55f, .5f, .4f), .95f);
    HallMakePlain(Look, TEXT("CI_TypewriterBlue"), FLinearColor(.08f, .12f, .2f), .4f);
    HallMakePlain(Look, TEXT("CI_PhoneBeige"), FLinearColor(.55f, .5f, .4f), .45f);
    HallMakePlain(Look, TEXT("CI_CrateRed"), FLinearColor(.4f, .04f, .03f), .6f);
    HallMakePlain(Look, TEXT("CI_CrateBlue"), FLinearColor(.04f, .08f, .3f), .6f);
    HallMakePlain(Look, TEXT("CI_HelmetPurple"), FLinearColor(.12f, .03f, .2f), .25f, .1f);
    HallMakePlain(Look, TEXT("CI_StreamerGold"), FLinearColor(.78f, .55f, .08f), .8f);     // crepe paper, the school's colours
    HallMakePlain(Look, TEXT("CI_StreamerPurple"), FLinearColor(.2f, .05f, .32f), .8f);
    HallMakePlain(Look, TEXT("CI_FrameBrown"), FLinearColor(.14f, .09f, .06f), .45f, .2f);   // hollow-metal door frames
    // Lit by emission (CI_TrofferFlicker is driven by the interiors actor: a few tubes on their way out).
    HallMakePlain(Look, TEXT("CI_TrofferFlicker"), FLinearColor(.9f, .93f, .95f), .4f, 0.f, FLinearColor(5.5f, 6.f, 6.2f));
    HallMakePlain(Look, TEXT("CI_GreenScreen"), FLinearColor(.02f, .15f, .04f), .2f, 0.f, FLinearColor(.2f, 1.6f, .35f));
    HallMakePlain(Look, TEXT("CI_ProjectorStage"), FLinearColor(.9f, .9f, .85f), .2f, 0.f, FLinearColor(4.f, 4.f, 3.6f));
    HallMakePlain(Look, TEXT("CI_HoodLight"), FLinearColor(.8f, .82f, .8f), .5f, 0.f, FLinearColor(2.5f, 2.6f, 2.6f));
    HallMakePlain(Look, TEXT("CI_PilotLamp"), FLinearColor(1.f, .4f, .1f), .4f, 0.f, FLinearColor(6.f, 2.f, .4f));
    {
        // The CRT picture: a bluish broadcast glow with scan lines (the homecoming replay is just light on glass).
        FHallImage Img; Img.Init(64, 48, FLinearColor(.2f, .3f, .4f));
        for (int32 Y = 0; Y < 48; Y += 2) for (int32 X = 0; X < 64; ++X) Img.At(X, Y) = FLinearColor(.12f, .2f, .28f);
        Img.Rect(FVector2D(8, 26), FVector2D(56, 44), FLinearColor(.18f, .4f, .16f));   // a green field
        Img.Rect(FVector2D(26, 30), FVector2D(30, 38), FLinearColor(.5f, .2f, .6f)); Img.Rect(FVector2D(38, 32), FVector2D(41, 39), FLinearColor(.8f, .8f, .8f));
        UTexture2D* Tex = PaintFixtureTexture(Look, TEXT("CI_CrtScreen"), Img);
        HallMakeTextured(Look, TEXT("CI_CrtScreen"), Tex, nullptr, .15f, FLinearColor::White, 0.f, 0.f, FLinearColor(2.2f, 2.4f, 2.8f));
    }
    {
        FHallImage Img; Img.Init(128, 192, FLinearColor(.06f, .06f, .07f));
        for (int32 Row = 0; Row < 6; ++Row) for (int32 Col = 0; Col < 4; ++Col)
        {
            const FLinearColor Can[] = { FLinearColor(.7f, .1f, .08f), FLinearColor(.1f, .3f, .7f), FLinearColor(.85f, .7f, .1f), FLinearColor(.2f, .6f, .2f) };
            Img.Rect(FVector2D(10 + Col * 28, 40 + Row * 24), FVector2D(32 + Col * 28, 58 + Row * 24), Can[(Row + Col) % 4]);
        }
        Img.Rect(FVector2D(8, 8), FVector2D(120, 30), FLinearColor(.9f, .85f, .7f));
        UTexture2D* Tex = PaintFixtureTexture(Look, TEXT("CI_VendingDrinks"), Img);
        HallMakeTextured(Look, TEXT("CI_VendingDrinks"), Tex, nullptr, .2f, FLinearColor::White, 0.f, 0.f, FLinearColor(1.6f, 1.6f, 1.5f));
        FHallImage Snack; Snack.Init(128, 192, FLinearColor(.08f, .08f, .09f));
        FRandomStream R(7);
        for (int32 Row = 0; Row < 5; ++Row) for (int32 Col = 0; Col < 5; ++Col)
            Snack.Rect(FVector2D(8 + Col * 23, 30 + Row * 32), FVector2D(26 + Col * 23, 52 + Row * 32), FLinearColor(R.FRandRange(.2f, .9f), R.FRandRange(.1f, .7f), R.FRandRange(.05f, .5f)));
        UTexture2D* Tex2 = PaintFixtureTexture(Look, TEXT("CI_VendingSnacks"), Snack);
        HallMakeTextured(Look, TEXT("CI_VendingSnacks"), Tex2, nullptr, .2f, FLinearColor::White, 0.f, 0.f, FLinearColor(1.4f, 1.4f, 1.3f));
    }
    {
        // Brass post office boxes: 8 x 10 doors, each with its window and number plate.
        FHallImage Img; Img.Init(256, 256, FLinearColor(.25f, .18f, .06f));
        for (int32 Row = 0; Row < 10; ++Row) for (int32 Col = 0; Col < 8; ++Col)
        {
            const float X0 = Col * 32.f, Y0 = Row * 25.6f;
            Img.Rect(FVector2D(X0 + 2, Y0 + 2), FVector2D(X0 + 30, Y0 + 23), FLinearColor(.55f, .42f, .16f));
            Img.Rect(FVector2D(X0 + 8, Y0 + 5), FVector2D(X0 + 24, Y0 + 13), FLinearColor(.08f, .08f, .07f));
            Img.Rect(FVector2D(X0 + 11, Y0 + 16), FVector2D(X0 + 21, Y0 + 20), FLinearColor(.85f, .8f, .6f));
        }
        UTexture2D* Tex = PaintFixtureTexture(Look, TEXT("CI_PoBoxes"), Img);
        HallMakeTextured(Look, TEXT("CI_PoBoxes"), Tex, nullptr, .35f, FLinearColor::White, 0.f, .7f);
    }
    {
        // Cork boards: index cards, flyers and a hand-drawn map (the ride board) or notices.
        for (const bool bRide : { true, false })
        {
            FHallImage Img; Img.Init(256, 160, FLinearColor(.42f, .28f, .14f));
            FRandomStream R(bRide ? 1986 : 1891);
            for (int32 K = 0; K < 30; ++K)
            {
                const float X = R.FRandRange(4, 220), Y = R.FRandRange(4, 130), W = R.FRandRange(18, 36), H = R.FRandRange(12, 26);
                const FLinearColor Card = R.FRand() < .6f ? FLinearColor(.85f, .83f, .75f) : FLinearColor(R.FRandRange(.5f, .9f), R.FRandRange(.5f, .9f), R.FRandRange(.3f, .7f));
                Img.Rect(FVector2D(X, Y), FVector2D(X + W, Y + H), Card);
                for (float L = Y + 4; L < Y + H - 3; L += 4) Img.Rect(FVector2D(X + 3, L), FVector2D(X + W - 4, L + 1), FLinearColor(.25f, .25f, .3f));
                Img.Rect(FVector2D(X + W * .5f - 1, Y + 1), FVector2D(X + W * .5f + 2, Y + 4), FLinearColor(.7f, .1f, .1f));
            }
            if (bRide) { Img.Rect(FVector2D(90, 50), FVector2D(170, 110), FLinearColor(.8f, .78f, .65f)); Img.Stroke({ FVector2D(95, 100), FVector2D(120, 80), FVector2D(150, 85), FVector2D(165, 58) }, 2.f, FLinearColor(.6f, .1f, .1f)); }
            const FName Key = bRide ? FName(TEXT("CI_RideBoard")) : FName(TEXT("CI_NoticeBoard"));
            UTexture2D* Tex = PaintFixtureTexture(Look, Key, Img);
            HallMakeTextured(Look, Key, Tex, nullptr, .9f);
        }
    }
    {
        // Chalkboards with the day's work still on them: equations (science), a seminar outline, a dorm floor meeting note.
        const TCHAR* Boards[][3] = {
            { TEXT("CI_ChalkPhysics"), TEXT("F = MA     V = V0 + AT"), TEXT("EXAM FRIDAY - CH 6, 7") },
            { TEXT("CI_ChalkChem"), TEXT("PV = NRT    2H2 + O2 > 2H2O"), TEXT("LAB REPORTS DUE MON") },
            { TEXT("CI_ChalkSeminar"), TEXT("THE NOVEL AS MIRROR"), TEXT("READ CH 1-4 FOR TUES") },
            { TEXT("CI_ChalkDorm"), TEXT("FLOOR MEETING SUN 9PM"), TEXT("QUIET HOURS 11-7") },
        };
        for (const auto& B : Boards)
            PaintText(Look, B[0], 512, 160, FLinearColor(.06f, .13f, .09f), FLinearColor(.8f, .82f, .78f), { { B[1], .2f }, { B[2], .16f } }, false);
    }
    PaintText(Look, TEXT("CI_ExitFace"), 128, 64, FLinearColor(.95f, .9f, .85f), FLinearColor(.75f, .05f, .03f), { { TEXT("EXIT"), .55f } }, false, FLinearColor(3.f, 1.2f, 1.f));
    PaintText(Look, TEXT("CI_Backglass"), 256, 256, FLinearColor(.1f, .02f, .2f), FLinearColor(.95f, .75f, .2f), { { TEXT("GOOSE"), .22f }, { TEXT("CHASE"), .22f } }, true, FLinearColor(2.f, 1.6f, 1.2f));
    {
        FHallImage Img; Img.Init(64, 128, FLinearColor(.05f, .1f, .25f));
        FRandomStream R(42);
        for (int32 K = 0; K < 14; ++K) Img.Ellipse(FVector2D(R.FRandRange(8, 56), R.FRandRange(10, 110)), FVector2D(4, 4), 0.f, FLinearColor(R.FRandRange(.4f, 1.f), R.FRandRange(.2f, .9f), .2f));
        UTexture2D* Tex = PaintFixtureTexture(Look, TEXT("CI_Playfield"), Img);
        HallMakeTextured(Look, TEXT("CI_Playfield"), Tex, nullptr, .2f, FLinearColor::White, 0.f, 0.f, FLinearColor(1.5f, 1.5f, 1.6f));
    }
}
}
