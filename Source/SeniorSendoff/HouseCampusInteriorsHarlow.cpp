// Harlow Hall (master job, step 2c, Tier A): a 1960s six-storey dorm, its ground and first floors walk-in. Canonical frame
// (main doors on +Y): X -1900..1900, Y -1175..1175; floors at -55 and 360, a dropped acoustic ceiling 310 over each.
//
// Each floor is a racetrack: student rooms along both long walls (a window each), a corridor inside each row, and a core
// between the corridors: the two stairs in their shafts, the communal bathroom (men below, women above), the laundry or the
// quiet study, the rec room or the floor kitchen; the corridors meet at the ends in window commons. Ground floor front: the
// lobby (the hall desk's pass-through, the mailboxes, the alumni sign-in), the TV lounge with the vending machines. Rooms
// with their doors propped open are furnished, each its own way (the house's loft bunks, desks, dressers, rugs and
// clothes; milk crates, boomboxes, posters for invented bands, mini fridges and hot plates, Geese pennants); locked rooms
// keep interior-mapped windows. Painted cinderblock, vinyl tile, steel frames; troffers and the TV glow by emission.
#include "HouseCampusInteriorsRooms.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusFallKit.h"
#include "Engine/Texture2D.h"

namespace HouseCampusInteriorsImpl
{
// Bands across the plan (partition centre lines along X): back rooms | back corridor | core | front corridor | front rooms.
constexpr float HarYS1 = -680.f, HarYS2 = -430.f, HarYN2 = 400.f, HarYN1 = 650.f;
// The bays: partition centre lines along Y on the window grid's piers (one window a room), between the end walls' faces.
constexpr float HarBay[] = { -1870.f, -1472.5f, -1112.5f, -752.5f, -392.5f, -32.5f, 327.5f, 687.5f, 1047.5f, 1407.5f, 1870.f };
// The core's cross walls: the west stair shaft, the bathroom, the middle room, the east room, the east stair shaft.
constexpr float HarCX[] = { -1460.f, -1300.f, -380.f, 380.f, 1300.f, 1460.f };
constexpr float HarCeil = 310.f;       // the dropped ceiling over each floor
constexpr float HarInX = 1870.f, HarInY = 1145.f;
// Corridor centre lines.
constexpr float HarCorN = (HarYN2 + HarYN1) * .5f, HarCorS = (HarYS1 + HarYS2) * .5f;

FBox2D HarShaft(bool bWest) { return bWest ? Rect(-1452.5f, -1307.5f, -422.5f, 392.5f) : Rect(1307.5f, 1452.5f, -422.5f, 392.5f); }
/** The floor-1 slab's stair wells: the west flight climbs north (to the front corridor), the east one south. */
FBox2D HarWell(bool bWest) { return bWest ? Rect(-1452.5f, -1307.5f, -422.5f, 292.f) : Rect(1307.5f, 1452.5f, -322.f, 392.5f); }

/** A bay of the front (north) or back (south) row on floor K: its room number (0: not a student room) and whether its door
 *  stands open (the room is furnished; a locked one keeps an interior-mapped window). */
struct FHarRoom { int32 Number = 0; bool bOpen = false; };
FHarRoom HarRoom(int32 K, bool bNorth, int32 Bay)
{
    FHarRoom R;
    if (Bay < 0 || Bay > 9 || K < 0 || K > 1) return R;
    if (K == 0 && bNorth)
    {
        // 101, 103, 105, the hall desk office, the lobby (two bays), the TV lounge (three), 107.
        static const int32 FrontRow[] = { 101, 103, 105, 0, 0, 0, 0, 0, 0, 107 };
        R.Number = FrontRow[Bay];
        R.bOpen = Bay == 0 || Bay == 2;
        return R;
    }
    R.Number = (K + 1) * 100 + (bNorth ? 1 : 2) + 2 * Bay;
    // Open: 205 209 213 at the front of floor 1; 104 110 116 and 202 210 216 at the back.
    const int32 Mask = bNorth ? 0x54 : K == 0 ? 0x92 : 0x91;
    R.bOpen = ((Mask >> Bay) & 1) != 0;
    return R;
}
/** Each furnished room's character: 0 the football player, 1 the record collector, 2 the studious one, 3 the mess, 4 the float
 *  builders, 5 the musician. */
int32 HarStyle(int32 Number)
{
    switch (Number)
    {
    case 101: case 209: return 0;
    case 104: case 216: return 1;
    case 105: case 202: return 2;
    case 110: case 213: return 3;
    case 116: case 210: return 4;
    default: return 5;
    }
}
/** The number plates' atlas cell for a room (8 x 5 cells). */
int32 HarPlateIndex(int32 Number)
{
    if (Number < 200) return Number % 2 ? (Number - 101) / 2 : 4 + (Number - 102) / 2;
    return Number % 2 ? 14 + (Number - 201) / 2 : 24 + (Number - 202) / 2;
}
int32 HarPlateNumber(int32 Index)
{
    if (Index < 4) return 101 + 2 * Index;
    if (Index < 14) return 102 + 2 * (Index - 4);
    if (Index < 24) return 201 + 2 * (Index - 14);
    if (Index < 34) return 202 + 2 * (Index - 24);
    return 0;
}

FHarFrame HarFrameOf(bool bNorth, int32 Bay)
{
    FHarFrame F;
    F.XL = HarBay[Bay] + (Bay == 0 ? 0.f : PartT * .5f);
    F.XR = HarBay[Bay + 1] - (Bay == 9 ? 0.f : PartT * .5f);
    F.YDoor = bNorth ? HarYN1 + PartT * .5f : HarYS1 - PartT * .5f;
    F.Dir = bNorth ? 1.f : -1.f;
    F.Depth = bNorth ? HarInY - F.YDoor : F.YDoor + HarInY;
    return F;
}
/** A room's doorway in its corridor wall (U along X), 92 wide, 30 in from the bay's west line. */
FVector2D HarDoorU(int32 Bay) { return FVector2D(HarBay[Bay] + 30.f, HarBay[Bay] + 122.f); }

// ------------------------------------------------------------------ small things the house has no mesh for
/** The reader's right on a wall facing Out. */
FVector HarRight(const FVector& Out) { return FVector::CrossProduct(FVector::UpVector, -Out.GetSafeNormal2D()).GetSafeNormal(); }
/** A poster taped to a wall (its centre C on the wall face). */
void HarPoster(FHallMesh& M, FName Key, const FVector& C, const FVector& Out, float W, float H)
{
    const FVector N = Out.GetSafeNormal2D();
    HouseCampusFallKit::Card(M, Key, C + N * .3f, HarRight(N), FVector::UpVector, W, H, N);
}
/** A felt pennant pinned by its hoist at C, the tip to the reader's right (so its lettering reads), drooping a little. */
void HarPennant(FHallMesh& M, FName Key, const FVector& C, const FVector& Out, float Len, float H)
{
    const FVector N = Out.GetSafeNormal2D(), R = HarRight(N), P = C + N * .3f;
    const FVector Tip = P + R * Len - FVector(0, 0, H * .2f);
    M.QuadUV(Key, P - FVector(0, 0, H * .5f), Tip - FVector(0, 0, 1.f), Tip + FVector(0, 0, 1.f), P + FVector(0, 0, H * .5f), N,
        FVector2D(0, 1), FVector2D(1, .54f), FVector2D(1, .46f), FVector2D(0, 0), R);
}
/** A room's number plate beside its door (a cell of the CI_HarPlates atlas). */
void HarPlate(FHallMesh& M, int32 Number, const FVector& C, const FVector& Out)
{
    const int32 I = HarPlateIndex(Number);
    const float U0 = (I % 8) / 8.f, U1 = (I % 8 + 1) / 8.f, V0 = (I / 8) / 5.f, V1 = (I / 8 + 1) / 5.f;
    const FVector N = Out.GetSafeNormal2D(), R = HarRight(N) * 10.f, U = FVector(0, 0, 5.f), P = C + N * .4f;
    M.QuadUV(TEXT("CI_HarPlates"), P - R - U, P + R - U, P + R + U, P - R + U, N, FVector2D(U0, V1), FVector2D(U1, V1), FVector2D(U1, V0), FVector2D(U0, V0), R.GetSafeNormal());
}
/** A football lying on its side along Along, its middle at C (on the surface at C.Z). */
void HarFootball(FHallMesh& M, const FVector& C, const FVector& Along)
{
    const FVector A = Along.GetSafeNormal(), Mid = C + FVector(0, 0, 8.5f);
    M.Dome(TEXT("CI_Football"), Mid, A, 8.5f, 14.f, 10);
    M.Dome(TEXT("CI_Football"), Mid, -A, 8.5f, 14.f, 10);
    M.Box(TEXT("CI_Paper"), FTransform(A.Rotation(), Mid + FVector(0, 0, 8.3f)), FVector(4.f, .5f, .3f), UVPlain());
}
/** An acoustic guitar standing on end against a wall (its foot at C, the wall behind along -Out). */
void HarGuitar(FHallMesh& M, const FVector& C, const FVector& Out)
{
    const FVector N = Out.GetSafeNormal2D(), Up = (FVector::UpVector - N * .2f).GetSafeNormal();
    const FVector Body = C + N * 6.f;
    M.Cylinder(TEXT("CI_Walnut"), Body + Up * 22.f - N * 5.f, Body + Up * 22.f + N * 5.f, 19.f, 18, UVPlain());
    M.Cylinder(TEXT("CI_Walnut"), Body + Up * 50.f - N * 5.f, Body + Up * 50.f + N * 5.f, 14.f, 16, UVPlain());
    M.Cylinder(TEXT("CI_Epoxy"), Body + Up * 40.f + N * 5.f, Body + Up * 40.f + N * 5.3f, 4.5f, 12, UVPlain());
    HouseCampusFallKit::Bar(M, TEXT("CI_OakDark"), Body + Up * 60.f + N * 3.f, Body + Up * 104.f + N * 3.f, 5.f, 2.5f, N);
    HouseCampusFallKit::Bar(M, TEXT("CI_OakDark"), Body + Up * 104.f + N * 2.f, Body + Up * 120.f + N * 1.f, 8.f, 2.f, N);
}
/** A pizza box (closed, greasy lid painted) on the surface at C. */
void HarPizzaBox(FHallMesh& M, const FVector& C, float Yaw)
{
    const FRotator R(0.f, Yaw, 0.f);
    M.Box(TEXT("CI_Cardboard"), FTransform(R, C + FVector(0, 0, 2.f)), FVector(20.f, 20.f, 2.f), UVPlain());
    HouseCampusFallKit::Card(M, TEXT("CI_PizzaLid"), C + FVector(0, 0, 4.05f), R.RotateVector(FVector(0, 1, 0)), R.RotateVector(FVector(1, 0, 0)), 39.f, 39.f, FVector::UpVector);
}
/** A soda can (unbranded), standing or knocked over. */
void HarCan(FHallMesh& M, const FVector& C, bool bRed, bool bDown, float Yaw)
{
    const FName K = bRed ? FName(TEXT("CI_CanRed")) : FName(TEXT("CI_CanSilver"));
    if (!bDown) { M.Cylinder(K, C, C + FVector(0, 0, 12.2f), 3.3f, 10, UVPlain()); return; }
    const FVector A = FRotator(0.f, Yaw, 0.f).Vector();
    M.Cylinder(K, C + FVector(0, 0, 3.3f) - A * 6.1f, C + FVector(0, 0, 3.3f) + A * 6.1f, 3.3f, 10, UVPlain());
}
/** A guitar amp, a speaker grille on its front. */
void HarAmp(FHallMesh& M, const FVector& C, const FVector& Facing)
{
    const FVector F = Facing.GetSafeNormal2D();
    const FRotator R = F.Rotation();
    M.Box(TEXT("CI_CrtBezel"), FTransform(R, C + FVector(0, 0, 21.f)), FVector(12.f, 24.f, 21.f), UVPlain());
    M.Box(TEXT("CI_Grille"), FTransform(R, C + F * 12.1f + FVector(0, 0, 18.f)), FVector(.2f, 21.f, 15.f), UVPlain());
    for (float T = -12.f; T <= 12.f; T += 6.f) M.Cylinder(TEXT("CI_Brass"), C + F * 12.f + R.RotateVector(FVector(0, T, 0)) + FVector(0, 0, 38.f), C + F * 13.5f + R.RotateVector(FVector(0, T, 0)) + FVector(0, 0, 38.f), 1.f, 8, UVPlain());
}
/** Homecoming float work: tissue-paper flowers piled in an open carton, and loose ones round it. */
void HarTissueBox(FHallMesh& M, const FVector& C, int32 Seed)
{
    for (const float S : { -1.f, 1.f })
    {
        M.Box(TEXT("CI_Cardboard"), FTransform(C + FVector(S * 24.f, 0, 15.f)), FVector(.6f, 18.f, 15.f), UVPlain());
        M.Box(TEXT("CI_Cardboard"), FTransform(C + FVector(0, S * 18.f, 15.f)), FVector(24.f, .6f, 15.f), UVPlain());
    }
    M.Box(TEXT("CI_Cardboard"), FTransform(C + FVector(0, 0, .5f)), FVector(24.f, 18.f, .5f), UVPlain());
    const FName Tissue[] = { TEXT("CI_TissuePurple"), TEXT("CI_TissueGold"), TEXT("CI_TissueWhite") };
    FRandomStream R(Seed);
    for (int32 K = 0; K < 26; ++K)
        HouseCampusFallKit::Ball(M, Tissue[K % 3], C + FVector(R.FRandRange(-19.f, 19.f), R.FRandRange(-13.f, 13.f), 26.f + R.FRandRange(-3.f, 6.f)), 4.5f, 6);
    for (int32 K = 0; K < 9; ++K)
        HouseCampusFallKit::Ball(M, Tissue[K % 3], C + FVector(R.FRandRange(-80.f, 80.f), R.FRandRange(-70.f, 70.f), 3.5f), 4.f, 6);
}
/** A dorm room's milk-glass ceiling fixture (lit by emission). */
void HarDomeLight(FHallMesh& Glow, FHallMesh& Clutter, const FVector& C)
{
    Glow.Dome(TEXT("CI_DomeLight"), C - FVector(0, 0, 1.f), -FVector::UpVector, 19.f, 9.f, 14);
    Clutter.Cylinder(TEXT("CI_Brass"), C - FVector(0, 0, 1.f), C, 21.f, 14, UVPlain());
}

/** The loft bunk, stacked as the house's own bedroom stacks it (surveyed by Tools/claude/survey_campus_bedroom.py): the daybed
 *  on the loft's lower rails 28 cm up, the mattress on its deck 133.6 up, a pillow at the head (the loft's local +X end) of
 *  each. C is the footprint's centre, Yaw the way its open long side faces. */
void HarBunk(const FKitter& Kit, const FVector2D& C, float Z, float Yaw, int32 Seed)
{
    using EKit = HouseCampusInteriors::EKit;
    Kit.Floor(EKit::LoftFrame, C, Z, Yaw, false);
    Kit.Floor(EKit::LowDaybed, C, Z + 28.2f, Yaw, false);
    Kit.Floor(EKit::QuiltedMattress, C, Z + 133.6f, Yaw, false);
    const FVector Head = FRotator(0.f, Yaw + KitDef(EKit::LoftFrame).FrontYaw, 0.f).RotateVector(FVector(64.7f, 0.f, 0.f));
    Kit.Floor(EKit::GreyPillow, C + FVector2D(Head), Z + 133.6f + 27.3f, Yaw + 4.f, true);
    Kit.Floor(Seed % 2 ? EKit::PatternedPillow : EKit::GreyPillow, C + FVector2D(Head), Z + 28.2f + 23.7f, Yaw - 6.f, true);
}

/** A furnished student room (see the header): the bunk on the far side wall, two desks and the dresser on the near one,
 *  milk crates by the door, the mini fridge in the window corner; then its occupants' own things (Style). */
void HarFurnish(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, const FHarFrame& F, float Z, float WinA, int32 Style, int32 Number)
{
    using EKit = HouseCampusInteriors::EKit;
    const float W = F.W(), D = F.Depth;
    const float ToA = F.Yaw(1.f, 0.f), FromA = F.Yaw(-1.f, 0.f);
    FRandomStream Rnd(Number * 31 + Style);
    HarBunk(Kit, F.P(W - 74.f, 215.f), Z, FromA, Number);
    Kit.Floor(EKit::WritingDesk, F.P(29.f, 155.f), Z, ToA, false);
    Kit.Floor(EKit::WritingDesk, F.P(29.f, 262.f), Z, ToA, false);
    const FVector2D Chair1 = F.P(84.f, 150.f + Rnd.FRandRange(-12.f, 12.f)), Chair2 = F.P(88.f, 268.f);
    const float Chair1Yaw = FromA + Rnd.FRandRange(-18.f, 18.f), Chair2Yaw = FromA + Rnd.FRandRange(-25.f, 25.f);
    Kit.Floor(EKit::SpindleChair, Chair1, Z, Chair1Yaw, true);
    Kit.Floor(EKit::SpindleChair, Chair2, Z, Chair2Yaw, true);
    Kit.Floor(EKit::Dresser, F.P(22.f, D - 75.f), Z, ToA, false);
    Kit.Floor(EKit::StripedRug, F.P(140.f, 230.f), Z, ToA, false);
    Radiator(Solid, F.V(WinA, D, Z), F.Dv(0.f, -1.f), 120.f);
    MilkCrateShelf(Solid, Clutter, F.V(175.f, 17.f, Z), F.Dv(0.f, 1.f), Number);
    MiniFridge(Solid, F.V(W - 30.f, D - 30.f, Z), F.Dv(-1.f, 0.f));
    Kit.Floor(EKit::TrashBin, F.P(W - 83.f, D - 30.f), Z, F.Yaw(0.f, -1.f), true);
    Kit.Floor(EKit::Sneaker, F.P(W - 60.f, 345.f), Z, Rnd.FRandRange(0.f, 360.f), true);
    Kit.Floor(EKit::Sneaker, F.P(W - 95.f, 362.f), Z, Rnd.FRandRange(0.f, 360.f), true);
    HarDomeLight(Glow, Clutter, F.V(W * .5f, D * .5f, Z + HarCeil));
    // Posters: over the desks, and over the crates by the door.
    const FName Bands[] = { TEXT("CI_PosterCanoes"), TEXT("CI_PosterGravel"), TEXT("CI_PosterMercury"), TEXT("CI_PosterTractor") };
    // (An end room has a window in its near wall: its poster goes up by the door instead.)
    if (F.XL > -HarInX + 1.f) HarPoster(Clutter, Bands[Number % 4], F.V(.1f, 208.f, Z + 168.f), F.Dv(1.f, 0.f), 55.f, 76.f);
    else HarPoster(Clutter, Bands[Number % 4], F.V(W - 60.f, .1f, Z + 165.f), F.Dv(0.f, 1.f), 55.f, 76.f);
    HarPoster(Clutter, Bands[(Number / 2 + 1) % 4], F.V(175.f, .1f, Z + 165.f), F.Dv(0.f, 1.f), 50.f, 70.f);
    // A Geese pennant beside the window, its tip toward the reader's right.
    const FVector Out = F.Dv(0.f, -1.f);
    const float Side = FVector::DotProduct(HarRight(Out), FVector(1, 0, 0)) > 0.f ? 1.f : -1.f;
    HarPennant(Clutter, TEXT("CI_Pennant"), F.V(WinA - Side * 100.f, D - .1f, Z + 205.f), Out, 70.f, 26.f);

    const float DeskTop = Z + 76.f, DresserTop = Z + Kit.Height(EKit::Dresser);
    switch (Style)
    {
    case 0:   // the football player: the helmet on the dresser, a ball on the floor, a towel on the chair, laundry piling up
        Helmet(Solid, F.V(22.f, D - 95.f, DresserTop), F.Dv(1.f, 0.f));
        HarFootball(Solid, F.V(W - 190.f, 345.f, Z), F.Dv(1.f, .4f));
        Kit.Floor(EKit::LaundryBasket, F.P(130.f, D - 40.f), Z, ToA + 10.f, true);
        Kit.Floor(EKit::ChairJacket, Chair1 - FVector2D(FRotator(0.f, Chair1Yaw, 0.f).Vector()) * 22.f, Z + 32.f, Chair1Yaw, false);
        Boombox(Solid, F.V(175.f, 17.f, Z + 84.f), F.Dv(0.f, 1.f));
        HarPoster(Clutter, TEXT("CI_PosterBeat"), F.V(W - .1f, 215.f, Z + 262.f), F.Dv(-1.f, 0.f), 90.f, 45.f);
        HarPennant(Clutter, TEXT("CI_Pennant"), F.V(.1f, D - 120.f, Z + 175.f), F.Dv(1.f, 0.f), 60.f, 22.f);
        break;
    case 1:   // the record collector: the boombox on the dresser, a stack of records, a poster over the bunk
        Boombox(Solid, F.V(22.f, D - 70.f, DresserTop), F.Dv(1.f, 0.f));
        HarPoster(Clutter, Bands[(Number + 2) % 4], F.V(W - .1f, 215.f, Z + 262.f), F.Dv(-1.f, 0.f), 55.f, 70.f);
        for (int32 K = 0; K < 6; ++K) Box(Solid, K % 2 ? TEXT("CI_CrtBezel") : TEXT("CI_CrateBlue"), F.V(30.f, 230.f, DeskTop + K * .8f) - FVector(16.f, 16.f, 0.f), F.V(30.f, 230.f, DeskTop + K * .8f + .7f) + FVector(16.f, 16.f, 0.f));
        Kit.Floor(EKit::StackedBooks, F.P(30.f, 170.f), DeskTop, ToA + 15.f, true);
        Kit.Floor(EKit::PaperCup, F.P(40.f, 290.f), DeskTop, 0.f, true);
        break;
    case 2:   // the studious one: a typewriter, a lamp, books, the bed made
        Typewriter(Solid, Clutter, F.V(32.f, 150.f, DeskTop), F.Dv(1.f, 0.f));
        Kit.Floor(EKit::DeskLamp, F.P(18.f, 300.f), DeskTop, ToA, true);
        Kit.Floor(EKit::StackedBooks, F.P(30.f, 245.f), DeskTop, ToA + 5.f, true);
        Kit.Floor(EKit::StackedBooks, F.P(26.f, 190.f), DeskTop, ToA - 10.f, true);
        Kit.Floor(EKit::PaperStack, F.P(36.f, 125.f), DeskTop, ToA + 20.f, true);
        HarPoster(Clutter, TEXT("CI_HarCalendar"), F.V(.1f, 120.f, Z + 160.f), F.Dv(1.f, 0.f), 40.f, 52.f);
        break;
    case 3:   // the mess: clothes and a pizza box on the floor, cans, a jacket on the chair, the laundry bag full
        Kit.Floor(EKit::LooseShirt, F.P(150.f, 140.f), Z + .5f, Rnd.FRandRange(0.f, 360.f), true);
        Kit.Floor(EKit::LooseShirt, F.P(115.f, 330.f), Z + .5f, Rnd.FRandRange(0.f, 360.f), true);
        Kit.Floor(EKit::LooseShirt, F.P(W - 120.f, 360.f), Z + .5f, Rnd.FRandRange(0.f, 360.f), true);
        Kit.Floor(EKit::BunchedTowel, F.P(W - 74.f, 250.f), Z + 52.f, Rnd.FRandRange(0.f, 360.f), false);
        Kit.Floor(EKit::ChairJacket, Chair2 - FVector2D(FRotator(0.f, Chair2Yaw, 0.f).Vector()) * 22.f, Z + 32.f, Chair2Yaw, false);
        Kit.Wall(EKit::HangingLaundryBag, F.V(250.f, .1f, Z + 120.f), F.Dv(0.f, 1.f));
        HarPizzaBox(Clutter, F.V(150.f, 280.f, Z + 1.5f), Rnd.FRandRange(0.f, 90.f));
        HarCan(Clutter, F.V(40.f, 140.f, DeskTop), true, false, 0.f);
        HarCan(Clutter, F.V(165.f, 300.f, Z + 1.5f), false, true, Rnd.FRandRange(0.f, 360.f));
        HarCan(Clutter, F.V(W - 150.f, 120.f, Z), true, true, Rnd.FRandRange(0.f, 360.f));
        Kit.Floor(EKit::Sneaker, F.P(60.f, 360.f), Z, Rnd.FRandRange(0.f, 360.f), true);
        break;
    case 4:   // the float builders: tissue flowers by the carton, chicken wire, paint, a banner half done
        HarTissueBox(Solid, F.V(120.f, D - 45.f, Z), Number);
        Solid.Cylinder(TEXT("CI_ChickenWire"), F.V(W - 175.f, 400.f, Z + 18.f) - F.Dv(1.f, 0.f) * 45.f, F.V(W - 175.f, 400.f, Z + 18.f) + F.Dv(1.f, 0.f) * 45.f, 18.f, 14, UVPlain());
        for (int32 K = 0; K < 3; ++K)
        {
            const FVector Can = F.V(170.f + K * 22.f, 340.f, Z + 1.5f);
            Clutter.Cylinder(TEXT("CI_Enamel"), Can, Can + FVector(0, 0, 11.f), 8.f, 12, UVPlain());
            Clutter.Cylinder(K == 1 ? TEXT("CI_StreamerGold") : TEXT("CI_StreamerPurple"), Can + FVector(0, 0, 11.f), Can + FVector(0, 0, 11.3f), 7.f, 12, UVPlain());
        }
        // Lying face up, its lettering's top toward the window (so it reads from the door).
        HouseCampusFallKit::Card(Clutter, TEXT("CI_HarFloatBanner"), F.V(140.f, 245.f, Z + 1.7f), FVector::CrossProduct(F.Dv(0.f, 1.f), -FVector::UpVector), F.Dv(0.f, 1.f), 150.f, 70.f, FVector::UpVector);
        break;
    default:  // the musician: a guitar against the wall, the amp, the boombox, a poster over the bunk
        HarGuitar(Solid, F.V(240.f, 2.f, Z), F.Dv(0.f, 1.f));
        HarAmp(Solid, F.V(W - 60.f, 20.f, Z), F.Dv(0.f, 1.f));
        Boombox(Solid, F.V(22.f, D - 70.f, DresserTop), F.Dv(1.f, 0.f));
        HarPoster(Clutter, Bands[(Number + 3) % 4], F.V(W - .1f, 215.f, Z + 262.f), F.Dv(-1.f, 0.f), 55.f, 70.f);
        Kit.Floor(EKit::PaperCup, F.P(40.f, 140.f), DeskTop, 0.f, true);
        break;
    }
}

// ------------------------------------------------------------------ the building
/** Steel window stools at every real window of floor K (the closed ones are solid wall with an interior-mapped window). */
void HarWindowStools(FHallMesh& Solid, int32 Seed, float Height, int32 K)
{
    const FBox2D In = Rect(-HarInX, HarInX, -HarInY, HarInY);
    const float Sill = HouseCampusInteriors::WindowSill(Height, K);
    for (int32 Side = 0; Side < 4; ++Side)
    {
        TArray<FVector2D> Cols;
        HouseCampusInteriors::WindowColumns(Seed, Side, Cols);
        for (const FVector2D& C : Cols)
        {
            if (HouseCampusInteriors::WindowReplaced(Seed, Side, K, C.X, C.Y) || !HouseCampusInteriors::WindowOpen(Seed, Side, K, C.X, C.Y)) continue;
            const float U0 = C.X - 4.f, U1 = C.Y + 4.f;
            switch (Side)
            {
            case 0: Box(Solid, TEXT("CI_Beige"), FVector(U0, In.Max.Y - 14.f, Sill - 3.f), FVector(U1, In.Max.Y + 6.f, Sill)); break;
            case 1: Box(Solid, TEXT("CI_Beige"), FVector(U0, In.Min.Y - 6.f, Sill - 3.f), FVector(U1, In.Min.Y + 14.f, Sill)); break;
            case 2: Box(Solid, TEXT("CI_Beige"), FVector(In.Min.X - 6.f, U0, Sill - 3.f), FVector(In.Min.X + 14.f, U1, Sill)); break;
            default: Box(Solid, TEXT("CI_Beige"), FVector(In.Max.X - 14.f, U0, Sill - 3.f), FVector(In.Max.X + 6.f, U1, Sill)); break;
            }
        }
    }
}

/** The bathroom of floor K (men's below, women's above): stalls and showers along the back, sinks and mirrors along the
 *  front, a privacy wall inside the door, urinals or a vanity on the east wall. */
void HarBathroom(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, int32 K, float Z)
{
    using EKit = HouseCampusInteriors::EKit;
    const float X0 = HarCX[1] + PartT * .5f, X1 = HarCX[2] - PartT * .5f, Y0 = HarYS2 + PartT * .5f, Y1 = HarYN2 - PartT * .5f;
    // Glazed tile to 180 on every wall (round the door), the privacy wall tiled both sides.
    const FName Tile = TEXT("CI_WallTile");
    const FHallUV TileUV = FHallUV::World(60.f);
    TArray<FBox2D> DoorHole = { FBox2D(FVector2D(-1245.f, Z - 1.f), FVector2D(-1143.f, Z + 220.f)) };
    Subtract(X0, X1, Z, Z + 180.f, DoorHole, [&](float A, float B, float ZA, float ZB) { Box(Solid, Tile, FVector(A, Y1 - .6f, ZA), FVector(B, Y1, ZB), TileUV); });
    Box(Solid, Tile, FVector(X0, Y0, Z), FVector(X1, Y0 + .6f, Z + 180.f), TileUV);
    Box(Solid, Tile, FVector(X0, Y0, Z), FVector(X0 + .6f, Y1, Z + 180.f), TileUV);
    Box(Solid, Tile, FVector(X1 - .6f, Y0, Z), FVector(X1, Y1, Z + 180.f), TileUV);
    Box(Solid, Tile, FVector(X0, 242.f, Z), FVector(-1080.f, 258.f, Z + 213.f), TileUV);   // the privacy wall inside the door
    Box(Solid, TEXT("CI_Beige"), FVector(X0, 241.f, Z + 213.f), FVector(-1078.f, 259.f, Z + 217.f));
    // Toilet stalls: five along the back wall, steel partitions on legs, doors (two ajar), the bowls and tanks.
    const FName Steel = TEXT("CI_StallSteel");
    const float SY = Y0 + 150.f;
    for (int32 I = 0; I <= 5; ++I)
    {
        const float X = X0 + 90.f * I;
        if (I > 0) Box(Solid, Steel, FVector(X - 1.25f, Y0, Z + 15.f), FVector(X + 1.25f, SY, Z + 185.f));
        Box(Solid, TEXT("CI_Steel"), FVector(X - 3.f, SY - 3.f, Z), FVector(X + 3.f, SY + 3.f, Z + 190.f));   // pilaster
        if (I == 5) break;
        const float XC = X + 45.f;
        Box(Solid, Steel, FVector(X + 3.f, SY - 1.25f, Z + 15.f), FVector(X + 15.f, SY + 1.25f, Z + 185.f));
        if (I == 1 || I == 3)
        {
            // Ajar, swung in.
            const FVector Hinge(X + 15.f, SY, Z + 15.f);
            Solid.Box(Steel, FTransform(FRotator(0.f, -60.f, 0.f), Hinge + FRotator(0.f, -60.f, 0.f).RotateVector(FVector(36.f, 0, 85.f))), FVector(36.f, 1.25f, 85.f), UVPlain());
        }
        else Box(Solid, Steel, FVector(X + 15.f, SY - 1.25f, Z + 15.f), FVector(X + 87.f, SY + 1.25f, Z + 185.f));
        Solid.Cylinder(TEXT("CI_Porcelain"), FVector(XC, Y0 + 50.f, Z), FVector(XC, Y0 + 50.f, Z + 38.f), 19.f, 14, UVPlain());
        Box(Solid, TEXT("CI_Porcelain"), FVector(XC - 22.f, Y0, Z + 38.f), FVector(XC + 22.f, Y0 + 20.f, Z + 78.f));
        Box(Clutter, TEXT("CI_Rubber"), FVector(XC - 18.f, Y0 + 32.f, Z + 38.f), FVector(XC + 18.f, Y0 + 68.f, Z + 40.f));   // the seat
        Clutter.Cylinder(TEXT("CI_Paper"), FVector(X + 80.f, Y0 + 55.f, Z + 70.f), FVector(X + 88.f, Y0 + 55.f, Z + 70.f), 6.f, 10, UVPlain());
    }
    // Showers: four tiled stalls further along the back wall, curtains (two drawn back), heads and drains.
    for (int32 I = 0; I < 4; ++I)
    {
        const float XA = -800.f + 100.f * I, XB = XA + 100.f;
        Box(Solid, Tile, FVector(XA - 4.f, Y0, Z), FVector(XA + 4.f, Y0 + 120.f, Z + 200.f), TileUV);
        if (I == 3) Box(Solid, Tile, FVector(XB - 4.f, Y0, Z), FVector(XB + 4.f, Y0 + 120.f, Z + 200.f), TileUV);
        Clutter.Cylinder(TEXT("CI_Steel"), FVector(XA, Y0 + 122.f, Z + 198.f), FVector(XB, Y0 + 122.f, Z + 198.f), .8f, 6, UVPlain());
        const float Drawn = (I % 2) ? 30.f : 92.f;
        Box(Solid, TEXT("CI_ShowerCurtain"), FVector(XA + 4.f, Y0 + 121.f, Z + 22.f), FVector(XA + 4.f + Drawn, Y0 + 123.f, Z + 196.f));
        Clutter.Cylinder(TEXT("CI_Steel"), FVector((XA + XB) * .5f, Y0 + 1.f, Z + 190.f), FVector((XA + XB) * .5f, Y0 + 14.f, Z + 186.f), 4.f, 10, UVPlain());
        Clutter.Cylinder(TEXT("CI_Epoxy"), FVector((XA + XB) * .5f, Y0 + 60.f, Z), FVector((XA + XB) * .5f, Y0 + 60.f, Z + .3f), 5.f, 10, UVPlain());
    }
    Kit.Wall(EKit::DrapedTowel, FVector(X1, Y0 + 160.f, Z + 150.f), FVector(-1, 0, 0));
    // Sinks along the front wall, a mirror and a shelf over each; one tap still dripping.
    for (int32 I = 0; I < 6; ++I)
    {
        const float X = -1000.f + 100.f * I;
        Box(Solid, TEXT("CI_Porcelain"), FVector(X - 25.f, Y1 - 46.f, Z + 68.f), FVector(X + 25.f, Y1, Z + 86.f));
        Box(Clutter, TEXT("CI_Steel"), FVector(X - 18.f, Y1 - 40.f, Z + 85.f), FVector(X + 18.f, Y1 - 12.f, Z + 86.2f));
        for (const float S : { -1.f, 1.f }) Clutter.Cylinder(TEXT("CI_Steel"), FVector(X + S * 12.f, Y1 - 6.f, Z + 86.f), FVector(X + S * 12.f, Y1 - 6.f, Z + 93.f), 1.6f, 8, UVPlain());
        Clutter.Cylinder(TEXT("CI_Steel"), FVector(X, Y1 - 4.f, Z + 92.f), FVector(X, Y1 - 14.f, Z + 92.f), 1.f, 8, UVPlain());
        Solid.Cylinder(TEXT("CI_Steel"), FVector(X, Y1 - 10.f, Z), FVector(X, Y1 - 10.f, Z + 68.f), 2.f, 8, UVPlain());
        Box(Clutter, TEXT("CI_Mirror"), FVector(X - 30.f, Y1 - .8f, Z + 112.f), FVector(X + 30.f, Y1 - .2f, Z + 188.f));
        Box(Solid, TEXT("CI_Glass"), FVector(X - 30.f, Y1 - 13.f, Z + 102.f), FVector(X + 30.f, Y1, Z + 103.f));
    }
    Box(Solid, TEXT("CI_Steel"), FVector(-420.f, Y1 - 12.f, Z + 110.f), FVector(-390.f, Y1, Z + 150.f));   // paper towels
    Kit.Floor(EKit::TrashCan, FVector2D(-430.f, Y1 - 40.f), Z, 0.f, true);
    if (K == 0)
    {
        // Urinals on the east wall with steel screens between.
        for (int32 I = 0; I < 3; ++I)
        {
            const float Y = -150.f + 90.f * I;
            Box(Solid, TEXT("CI_Porcelain"), FVector(X1 - 34.f, Y - 18.f, Z + 40.f), FVector(X1, Y + 18.f, Z + 100.f));
            Clutter.Cylinder(TEXT("CI_Steel"), FVector(X1 - 6.f, Y, Z + 100.f), FVector(X1 - 6.f, Y, Z + 135.f), 1.5f, 8, UVPlain());
            Box(Solid, Steel, FVector(X1 - 50.f, Y + 44.f, Z + 30.f), FVector(X1, Y + 46.5f, Z + 160.f));
        }
    }
    else
    {
        // A vanity counter with a long mirror and a stool.
        Box(Solid, TEXT("CI_Laminate"), FVector(X1 - 55.f, -200.f, Z), FVector(X1, 120.f, Z + 76.f));
        Box(Clutter, TEXT("CI_Mirror"), FVector(X1 - .8f, -190.f, Z + 100.f), FVector(X1 - .2f, 110.f, Z + 190.f));
        Kit.Floor(EKit::BarStool, FVector2D(X1 - 90.f, -40.f), Z, 0.f, true);
        Kit.Floor(EKit::BunchedTowel, FVector2D(X1 - 30.f, 60.f), Z + 76.f, 30.f, false);
    }
}

/** One floor's partitions: the corridor walls (with every room's doorway), the core's cross walls, the room walls. */
void HarWalls(FHallMesh& Solid, int32 K, float Z, float ShaftTop)
{
    const FName Block = TEXT("CI_Cinderblock"), Steel = TEXT("CI_FrameBrown");
    const FHallUV BlockUV = FHallUV::World(80.f);
    const float Top = Z + HarCeil + 1.f;
    // Corridor | rooms walls, a doorway into every bay's room (each wall run is given only its own doorways).
    for (const bool bNorth : { true, false })
    {
        if (bNorth && K == 0)
        {
            // Rooms 101-105 and the desk office; the lobby's wide opening; the TV lounge's doorway; room 107.
            BlockWall(Solid, Block, false, HarYN1, -HarInX, HarBay[4] - 5.f, Z, Top, { HarDoorU(0), HarDoorU(1), HarDoorU(2), HarDoorU(3) }, 213.f, {}, Steel, BlockUV);
            BlockWall(Solid, Block, false, HarYN1, HarBay[4] - 5.f, HarBay[6] + 5.f, Z, Top, { FVector2D(HarBay[4] + 8.f, HarBay[6] - 8.f) }, 262.f, {}, Steel, BlockUV);
            BlockWall(Solid, Block, false, HarYN1, HarBay[6] + 5.f, 1200.f, Z, Top, { FVector2D(760.f, 1000.f) }, 228.f, {}, Steel, BlockUV);
            BlockWall(Solid, Block, false, HarYN1, 1200.f, HarInX, Z, Top, { HarDoorU(9) }, 213.f, {}, Steel, BlockUV);
        }
        else
        {
            TArray<FVector2D> Doors;
            for (int32 Bay = 0; Bay < 10; ++Bay) Doors.Add(HarDoorU(Bay));
            BlockWall(Solid, Block, false, bNorth ? HarYN1 : HarYS1, -HarInX, HarInX, Z, Top, Doors, 213.f, {}, Steel, BlockUV);
        }
        // Room | room walls on the piers.
        for (int32 Bay = 1; Bay < 10; ++Bay)
        {
            const float X = HarBay[Bay];
            if (bNorth && K == 0)
            {
                if (Bay == 5 || Bay == 7 || Bay == 8) continue;   // inside the lobby and the lounge
                if (Bay == 4)
                {
                    // The hall desk's pass-through counter into the lobby.
                    BlockWall(Solid, Block, true, X, HarYN1, HarInY, Z, Top, {}, 213.f, { FBox2D(FVector2D(820.f, Z + 100.f), FVector2D(1020.f, Z + 215.f)) }, Steel, BlockUV);
                    continue;
                }
                if (Bay == 6) { BlockWall(Solid, Block, true, X, HarYN1, HarInY, Z, Top, { FVector2D(690.f, 890.f) }, 228.f, {}, Steel, BlockUV); continue; }
            }
            if (bNorth) BlockWall(Solid, Block, true, X, HarYN1, HarInY, Z, Top, {}, 213.f, {}, Steel, BlockUV);
            else BlockWall(Solid, Block, true, X, -HarInY, HarYS1, Z, Top, {}, 213.f, {}, Steel, BlockUV);
        }
    }
    // The core's corridor walls: the stair shafts' doorways (the west flight climbs north, the east one south), the
    // bathroom's, the middle room's and the east room's.
    const FVector2D WestStair(-1445.f, -1315.f), EastStair(1315.f, 1445.f), Bath(-1240.f, -1148.f), Middle(120.f, 212.f);
    TArray<FVector2D> SouthDoors, NorthDoors = { Bath, Middle };
    if (K == 0) { SouthDoors = { WestStair, FVector2D(700.f, 860.f) }; NorthDoors.Add(EastStair); }
    else { SouthDoors = { EastStair, FVector2D(700.f, 792.f) }; NorthDoors.Add(WestStair); }
    BlockWall(Solid, Block, false, HarYS2, HarCX[0] - PartT * .5f, HarCX[5] + PartT * .5f, Z, Top, SouthDoors, K == 0 ? 228.f : 213.f, {}, Steel, BlockUV);
    BlockWall(Solid, Block, false, HarYN2, HarCX[0] - PartT * .5f, HarCX[5] + PartT * .5f, Z, Top, NorthDoors, 213.f, {}, Steel, BlockUV);
    // Cross walls; the stair shafts' run the full height of both floors (built with floor 0).
    for (int32 I = 0; I < 6; ++I)
    {
        const bool bShaft = I == 0 || I == 1 || I == 4 || I == 5;
        if (bShaft && K > 0) continue;
        BlockWall(Solid, Block, true, HarCX[I], HarYS2 + PartT * .5f, HarYN2 - PartT * .5f, Z, bShaft ? ShaftTop : Top, {}, 213.f, {}, Steel, BlockUV);
    }
}

void HarLightAnchors(int32 Seed, float Height, TArray<FVector>& Out)
{
    for (int32 K = 0; K < 2; ++K)
    {
        const float Z = HouseCampusInteriors::FloorTop(Height, K) + 280.f;
        for (const float X : { -1450.f, -600.f, 250.f, 1100.f }) { Out.Add(FVector(X, HarCorN, Z)); Out.Add(FVector(X, HarCorS, Z)); }
        Out.Add(FVector(-1665.f, 0.f, Z)); Out.Add(FVector(1665.f, 0.f, Z));
        Out.Add(FVector(-840.f, 0.f, Z)); Out.Add(FVector(0.f, 0.f, Z)); Out.Add(FVector(840.f, 0.f, Z));
        Out.Add(FVector(-1380.f, -50.f, Z)); Out.Add(FVector(1380.f, -50.f, Z));
        if (K == 0) { Out.Add(FVector(-30.f, 900.f, Z)); Out.Add(FVector(870.f, 900.f, Z)); Out.Add(FVector(-572.f, 900.f, Z)); }
        for (const bool bNorth : { true, false })
            for (int32 Bay = 0; Bay < 10; ++Bay)
                if (HarRoom(K, bNorth, Bay).bOpen)
                {
                    const FHarFrame F = HarFrameOf(bNorth, Bay);
                    Out.Add(F.V(F.W() * .5f, F.Depth * .55f, Z));
                }
    }
}

/** The ground floor's front: the lobby, the TV lounge and the hall desk's office. */
void HarFront(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, float Z)
{
    using EKit = HouseCampusInteriors::EKit;
    const float Ceil = Z + HarCeil;
    // ---- the lobby (X -385..320, Y 657.5..1145)
    Kit.Floor(EKit::EntryRug, FVector2D(0.f, 1075.f), Z, 90.f, false);
    HangingSign(Clutter, TEXT("CI_SignHarlow"), FVector(0.f, 860.f, Ceil - 55.f), FVector(0, 1, 0), 260.f, 40.f, Ceil);
    HouseCampusFallKit::Card(Clutter, TEXT("CI_HarAlumni"), FVector(-30.f, HarYN1 + PartT * .5f + .4f, Z + 287.f), FVector(1, 0, 0), FVector::UpVector, 420.f, 42.f, FVector(0, 1, 0));
    ExitSign(Solid, Glow, FVector(0.f, HarInY - 6.f, Z + 236.f), FVector(0, -1, 0));
    // The desk's pass-through: an oak ledge both sides, the sign over it.
    Box(Solid, TEXT("CI_Oak"), FVector(HarBay[4] - 28.f, 815.f, Z + 100.f), FVector(HarBay[4] + 26.f, 1025.f, Z + 104.f));
    WallPlaque(Clutter, TEXT("CI_HarDesk"), FVector(HarBay[4] + PartT * .5f, 920.f, Z + 245.f), FVector(1, 0, 0), 70.f, 24.f);
    Kit.Floor(EKit::PaperStack, FVector2D(HarBay[4] + 14.f, 870.f), Z + 104.f, 10.f, true);
    DeskPhone(Solid, FVector(HarBay[4] - 14.f, 980.f, Z + 104.f), FVector(1, 0, 0), true);
    // Mailboxes on the lounge wall, the notice board and the college's photo either side of the doors.
    MailboxWall(Solid, Clutter, FVector(HarBay[6] - PartT * .5f, 1035.f, Z + 128.f), FVector(-1, 0, 0), 160.f, 110.f);
    BulletinBoard(Solid, Clutter, FVector(-260.f, HarInY - 1.f, Z + 150.f), FVector(0, -1, 0), 150.f, 90.f, TEXT("CI_NoticeBoard"));
    WallClock(Clutter, FVector(-260.f, HarInY, Z + 258.f), FVector(0, -1, 0), 16.f);
    Kit.Wall(EKit::GroupPhoto, FVector(230.f, HarInY, Z + 165.f), FVector(0, -1, 0));
    Kit.Floor(EKit::PlantStand, FVector2D(-335.f, 1095.f), Z, -90.f, true);
    // The alumni sign-in table by the corridor: the sheet, a cup of coffee.
    Table(Solid, FVector(-290.f, 722.f, Z), 120.f, 56.f, 76.f, true);
    Kit.Floor(EKit::PaperStack, FVector2D(-300.f, 722.f), Z + 76.f, 0.f, true);
    Kit.Floor(EKit::PaperCup, FVector2D(-250.f, 735.f), Z + 76.f, 0.f, true);
    // Streamers corner to corner, purple and gold twisted together.
    for (const float D : { 0.f, 14.f })
    {
        Streamer(Clutter, FVector(-380.f, 700.f + D, Ceil - 8.f), FVector(315.f, 1100.f + D, Ceil - 8.f), D > 0.f);
        Streamer(Clutter, FVector(-380.f, 1100.f + D, Ceil - 8.f), FVector(315.f, 700.f + D, Ceil - 8.f), D == 0.f);
    }
    // ---- the TV lounge (X 335..1400, Y 657.5..1145): the console TV still showing the game, the sofa and chairs round it,
    // the card table, the vending machines; coats left on chairs, cups, a bowl of popcorn.
    CrtTelevision(Solid, Glow, FVector(1400.f - 26.f, 900.f, Z), FVector(-1, 0, 0), true);
    Kit.Floor(EKit::StudyRug, FVector2D(1130.f, 900.f), Z, 0.f, false);
    Kit.Floor(EKit::LeatherSofa, FVector2D(990.f, 900.f), Z, 0.f, true);
    Kit.Floor(EKit::TanRecliner, FVector2D(1170.f, 720.f), Z, 40.f, true);
    Kit.Floor(EKit::ClubChair, FVector2D(1170.f, 1080.f), Z, -40.f, true);
    Kit.Floor(EKit::CoffeeChest, FVector2D(1175.f, 900.f), Z, 0.f, true);
    const float ChestTop = Z + Kit.Height(EKit::CoffeeChest);
    Solid.Cylinder(TEXT("CI_Enamel"), FVector(1170.f, 880.f, ChestTop), FVector(1170.f, 880.f, ChestTop + 8.f), 15.f, 16, UVPlain());
    for (int32 I = 0; I < 14; ++I) HouseCampusFallKit::Ball(Clutter, TEXT("CI_Paper"), FVector(1170.f + 9.f * FMath::Cos(I * 2.4f) * (I % 3) * .5f, 880.f + 9.f * FMath::Sin(I * 2.4f) * (I % 3) * .5f, ChestTop + 8.f + (I % 4)), 1.8f, 5);
    Kit.Floor(EKit::PaperCup, FVector2D(1190.f, 930.f), ChestTop, 0.f, true);
    Kit.Floor(EKit::FloorLamp, FVector2D(940.f, 1090.f), Z, 0.f, true);
    Kit.Floor(EKit::OctagonalCardTable, FVector2D(600.f, 880.f), Z, 0.f, false);
    for (int32 I = 0; I < 4; ++I)
    {
        const float A = 45.f + 90.f * I;
        const FVector2D C = FVector2D(600.f, 880.f) + FVector2D(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A))) * 108.f;
        const float Yaw = A + 180.f;
        Kit.Floor(EKit::SpindleChair, C, Z, Yaw, true);
        if (I == 0 || I == 2) Kit.Floor(EKit::ChairJacket, C - FVector2D(FRotator(0.f, Yaw, 0.f).Vector()) * 22.f, Z + 32.f, Yaw, false);
    }
    Kit.Floor(EKit::PaperCup, FVector2D(570.f, 900.f), Z + 76.f, 0.f, true);
    Box(Clutter, TEXT("CI_Paper"), FVector(605.f, 860.f, Z + 76.f), FVector(611.f, 869.f, Z + 77.5f));   // the deck
    VendingMachine(Solid, Glow, FVector(335.f + 42.f, 1000.f, Z), FVector(1, 0, 0), false);
    VendingMachine(Solid, Glow, FVector(335.f + 42.f, 1093.f, Z), FVector(1, 0, 0), true);
    for (const float X : { 507.5f, 867.5f, 1227.5f }) Radiator(Solid, FVector(X, HarInY, Z), FVector(0, -1, 0), 120.f);
    WallPlaque(Clutter, TEXT("CI_HarLounge"), FVector(880.f, HarYN1 - PartT * .5f, Z + 250.f), FVector(0, -1, 0), 90.f, 26.f);
    WallClock(Clutter, FVector(1060.f, HarYN1 + PartT * .5f, Z + 250.f), FVector(0, 1, 0), 15.f);
    // ---- the hall desk's office (locked; seen through the pass-through and its window)
    Table(Solid, FVector(-620.f, 930.f, Z), 160.f, 75.f, 76.f, false);
    Typewriter(Solid, Clutter, FVector(-610.f, 900.f, Z + 76.f), FVector(1, 0, 0));
    DeskPhone(Solid, FVector(-610.f, 980.f, Z + 76.f), FVector(1, 0, 0), false);
    Kit.Floor(EKit::GrayOfficeChair, FVector2D(-680.f, 930.f), Z, 0.f, false);
    Box(Solid, TEXT("CI_Steel"), FVector(-745.f, 765.f, Z), FVector(-700.f, 810.f, Z + 132.f));   // filing cabinet, clear of the door's swing
    Box(Solid, TEXT("CI_OakDark"), FVector(-745.f, 1030.f, Z + 120.f), FVector(-738.f, 1110.f, Z + 190.f));
    HouseCampusFallKit::Card(Clutter, TEXT("CI_HarKeys"), FVector(-737.8f, 1070.f, Z + 155.f), FVector(0, -1, 0), FVector::UpVector, 76.f, 66.f, FVector(1, 0, 0));
    Box(Solid, TEXT("CI_OakDark"), FVector(-600.f, HarYN1 + 8.f, Z + 90.f), FVector(-410.f, HarYN1 + 40.f, Z + 180.f));   // mail sorting cubbies
    HouseCampusFallKit::Card(Clutter, TEXT("CI_PoBoxes"), FVector(-505.f, HarYN1 + 40.1f, Z + 135.f), FVector(1, 0, 0), FVector::UpVector, 186.f, 86.f, FVector(0, 1, 0));
    Radiator(Solid, FVector(-572.5f, HarInY, Z), FVector(0, -1, 0), 120.f);
}

/** The core rooms of floor K, the window commons at the ends, and the corridors' fittings. */
void HarCore(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, int32 K, float Z)
{
    using EKit = HouseCampusInteriors::EKit;
    const float Y0 = HarYS2 + PartT * .5f, Y1 = HarYN2 - PartT * .5f, Ceil = Z + HarCeil;
    HarBathroom(Solid, Clutter, Glow, Kit, K, Z);
    WallPlaque(Clutter, K == 0 ? TEXT("CI_HarMen") : TEXT("CI_HarWomen"), FVector(-1100.f, HarYN2 + PartT * .5f, Z + 160.f), FVector(0, 1, 0), 50.f, 20.f);
    // ---- the middle room: the laundry below, the quiet study above
    if (K == 0)
    {
        for (int32 I = 0; I < 4; ++I)
        {
            Laundry(Solid, Glow, FVector(-310.f + 72.f * I, Y0 + 36.f, Z), FVector(0, 1, 0), false);
            Laundry(Solid, Glow, FVector(94.f + 72.f * I, Y0 + 36.f, Z), FVector(0, 1, 0), true);
        }
        Table(Solid, FVector(0.f, 40.f, Z), 240.f, 90.f, 86.f, true);
        Kit.Floor(EKit::LaundryBasket, FVector2D(-60.f, 40.f), Z + 86.f, 15.f, true);
        Kit.Floor(EKit::LooseShirt, FVector2D(40.f, 50.f), Z + 86.5f, 70.f, true);
        Box(Solid, TEXT("CI_Cardboard"), FVector(80.f, 20.f, Z + 86.f), FVector(100.f, 32.f, Z + 112.f));   // the soap powder
        BulletinBoard(Solid, Clutter, FVector(-372.5f + 1.f, 60.f, Z + 150.f), FVector(1, 0, 0), 140.f, 90.f, TEXT("CI_NoticeBoard"));
        WallClock(Clutter, FVector(-372.5f, 60.f, Z + 248.f), FVector(1, 0, 0), 14.f);
        Kit.Floor(EKit::SpindleChair, FVector2D(300.f, 200.f), Z, -120.f, true);
        Box(Solid, TEXT("CI_Steel"), FVector(372.5f - 14.f, -60.f, Z + 120.f), FVector(372.5f, -20.f, Z + 170.f));   // the change machine
        WallPlaque(Clutter, TEXT("CI_HarLaundry"), FVector(166.f, HarYN2 + PartT * .5f, Z + 250.f), FVector(0, 1, 0), 90.f, 26.f);
    }
    else
    {
        Chalkboard(Solid, Clutter, FVector(0.f, Y0 + 2.f, Z + 150.f), FVector(0, 1, 0), 360.f, 110.f, TEXT("CI_ChalkDorm"));
        for (const float X : { -200.f, 200.f })
        {
            Kit.Floor(EKit::StudyTable, FVector2D(X, 80.f), Z, 0.f, false);
            for (const float S : { -1.f, 1.f }) Kit.Floor(EKit::DeskChair, FVector2D(X + S * 60.f, 80.f - 85.f), Z, 90.f, true);
            Kit.Floor(EKit::TableLamp, FVector2D(X, 80.f), Z + Kit.Height(EKit::StudyTable), -90.f, true);
        }
        Kit.Floor(EKit::StackedBooks, FVector2D(-170.f, 60.f), Z + Kit.Height(EKit::StudyTable), 25.f, true);
        Kit.Floor(EKit::Backpack, FVector2D(260.f, -40.f), Z, 140.f, true);
        Kit.Floor(EKit::Bookcase, FVector2D(-372.5f + 18.f, 200.f), Z, 0.f, false);
        Kit.FillBookcase(FVector2D(-372.5f + 18.f, 200.f), Z, 0.f, 3);
        WallClock(Clutter, FVector(-372.5f, 40.f, Z + 248.f), FVector(1, 0, 0), 14.f);
        WallPlaque(Clutter, TEXT("CI_HarStudy"), FVector(166.f, HarYN2 + PartT * .5f, Z + 250.f), FVector(0, 1, 0), 110.f, 26.f);
    }
    // ---- the east room: the rec room below, the floor kitchen above
    if (K == 0)
    {
        const FVector T(840.f, 0.f, Z);
        Box(Solid, TEXT("CI_PingPong"), T + FVector(-137.f, -76.f, 73.f), T + FVector(137.f, 76.f, 76.f));
        Box(Clutter, TEXT("CI_Paper"), T + FVector(-137.f, -.5f, 76.f), T + FVector(137.f, .5f, 76.1f));
        for (const float S : { -1.f, 1.f })
        {
            Box(Clutter, TEXT("CI_Paper"), T + FVector(-137.f, S * 75.f - 1.f, 76.f), T + FVector(137.f, S * 75.f + 1.f, 76.1f));
            Box(Clutter, TEXT("CI_Paper"), T + FVector(S * 136.f - 1.f, -76.f, 76.f), T + FVector(S * 136.f + 1.f, 76.f, 76.1f));
            for (const float S2 : { -1.f, 1.f }) Box(Solid, TEXT("CI_Steel"), T + FVector(S * 110.f - 2.f, S2 * 60.f - 2.f, 0.f), T + FVector(S * 110.f + 2.f, S2 * 60.f + 2.f, 73.f));
        }
        Box(Clutter, TEXT("CI_Net"), T + FVector(-.3f, -88.f, 76.f), T + FVector(.3f, 88.f, 91.f));
        Clutter.Cylinder(TEXT("CI_CrateRed"), T + FVector(70.f, 30.f, 76.f), T + FVector(70.f, 30.f, 77.f), 8.f, 12, UVPlain());
        HouseCampusFallKit::Ball(Clutter, TEXT("CI_Enamel"), T + FVector(-40.f, -20.f, 78.f), 2.f, 6);
        Pinball(Solid, Glow, FVector(560.f, Y1 - 70.f, Z), FVector(0, -1, 0));
        Kit.Floor(EKit::Arcade, FVector2D(720.f, Y1 - 50.f), Z, -90.f, false);
        Kit.Floor(EKit::LeatherSofa, FVector2D(1292.5f - 50.f, 0.f), Z, 180.f, true);
        WallPlaque(Clutter, TEXT("CI_HarRec"), FVector(780.f, HarYS2 - PartT * .5f, Z + 262.f), FVector(0, -1, 0), 120.f, 26.f);
    }
    else
    {
        // The counter with its sink along the front wall, the stove and the refrigerator, a table.
        Box(Solid, TEXT("CI_Oak"), FVector(450.f, Y1 - 60.f, Z), FVector(1100.f, Y1, Z + 86.f));
        Box(Solid, TEXT("CI_Laminate"), FVector(447.f, Y1 - 63.f, Z + 86.f), FVector(1100.f, Y1, Z + 90.f));
        Box(Clutter, TEXT("CI_Steel"), FVector(660.f, Y1 - 50.f, Z + 90.f), FVector(740.f, Y1 - 12.f, Z + 90.3f));
        Clutter.Cylinder(TEXT("CI_Steel"), FVector(700.f, Y1 - 6.f, Z + 90.f), FVector(700.f, Y1 - 6.f, Z + 115.f), 1.4f, 8, UVPlain());
        Box(Solid, TEXT("CI_Enamel"), FVector(1110.f, Y1 - 64.f, Z), FVector(1186.f, Y1, Z + 90.f));
        for (const float DX : { 18.f, 56.f }) for (const float DY : { -46.f, -18.f })
            Clutter.Cylinder(TEXT("CI_Epoxy"), FVector(1110.f + DX, Y1 + DY, Z + 90.f), FVector(1110.f + DX, Y1 + DY, Z + 91.f), 9.f, 12, UVPlain());
        Box(Solid, TEXT("CI_Enamel"), FVector(1200.f, Y1 - 70.f, Z), FVector(1280.f, Y1, Z + 168.f));
        Box(Clutter, TEXT("CI_Steel"), FVector(1270.f, Y1 - 71.f, Z + 100.f), FVector(1274.f, Y1 - 70.f, Z + 150.f));
        Solid.Cylinder(TEXT("CI_Steel"), FVector(1148.f, Y1 - 46.f, Z + 91.f), FVector(1148.f, Y1 - 46.f, Z + 108.f), 9.f, 12, UVPlain());   // the kettle
        Kit.Floor(EKit::DiningTable, FVector2D(960.f, -60.f), Z, 0.f, false);
        for (const float DX : { -50.f, 50.f }) for (const float S : { -1.f, 1.f })
            Kit.Floor(EKit::GreenSpindleChair, FVector2D(960.f + DX, -60.f + S * 70.f), Z, S > 0.f ? -90.f : 90.f, true);
        Kit.Floor(EKit::PaperCup, FVector2D(940.f, -50.f), Z + Kit.Height(EKit::DiningTable), 0.f, true);
        WallPlaque(Clutter, TEXT("CI_HarKitchen"), FVector(746.f, HarYS2 - PartT * .5f, Z + 250.f), FVector(0, -1, 0), 100.f, 26.f);
    }
    WallClock(Clutter, FVector(1300.f - PartT * .5f, -40.f, Z + 240.f), FVector(-1, 0, 0), 15.f);
    // ---- the window commons at the corridors' ends
    for (const float S : { -1.f, 1.f })
    {
        const float XWall = S * HarInX;
        for (const float Y : { -207.5f, 152.5f }) Radiator(Solid, FVector(XWall, Y, Z), FVector(-S, 0, 0), 120.f);
        Radiator(Solid, FVector(XWall, 512.5f, Z), FVector(-S, 0, 0), 120.f);
        Radiator(Solid, FVector(XWall, -567.5f, Z), FVector(-S, 0, 0), 120.f);
    }
    if (K == 0)
    {
        VendingMachine(Solid, Glow, FVector(HarCX[0] - PartT * .5f - 42.f, -100.f, Z), FVector(-1, 0, 0), false);
        VendingMachine(Solid, Glow, FVector(HarCX[0] - PartT * .5f - 42.f, -8.f, Z), FVector(-1, 0, 0), true);
        Kit.Floor(EKit::TrashCan, FVector2D(HarCX[0] - PartT * .5f - 30.f, 90.f), Z, 0.f, true);
        Kit.Floor(EKit::SpindleChair, FVector2D(-1780.f, -30.f), Z, 20.f, true);
        Kit.Floor(EKit::SpindleChair, FVector2D(-1760.f, 60.f), Z, -40.f, true);
        Kit.Floor(EKit::LeatherSofa, FVector2D(1600.f, -30.f), Z, 0.f, true);
        Kit.Floor(EKit::TanRecliner, FVector2D(1780.f, 280.f), Z, -120.f, true);
        AshUrn(Solid, FVector(1540.f, 250.f, Z));
        HarPoster(Clutter, TEXT("CI_PosterBeat"), FVector(HarCX[5] + PartT * .5f, 0.f, Z + 170.f), FVector(1, 0, 0), 120.f, 60.f);
    }
    else
    {
        // Two study tables by the windows (long side along the wall), their chairs on the room side, clear of the way
        // through between the corridors.
        for (const float Y : { -190.f, 190.f })
        {
            Kit.Floor(EKit::StudyTable, FVector2D(-1775.f, Y), Z, 0.f, false);
            for (const float DY : { -45.f, 45.f }) Kit.Floor(EKit::DeskChair, FVector2D(-1690.f, Y + DY), Z, 180.f, true);
        }
        Kit.Floor(EKit::FloorLamp, FVector2D(-1520.f, 300.f), Z, 0.f, true);
        Kit.Floor(EKit::PlantStand, FVector2D(-1520.f, -330.f), Z, 0.f, true);
        HouseCampusFallKit::TenSpeed(Solid, TEXT("CI_CrateBlue"), TEXT("CI_Rubber"), TEXT("CI_Steel"), FVector(1700.f, -330.f, Z), 0.f);
        Kit.Floor(EKit::LaundryBasket, FVector2D(1560.f, 200.f), Z, 30.f, true);
        Box(Solid, TEXT("CI_Steel"), FVector(1600.f, 20.f, Z + 80.f), FVector(1760.f, 55.f, Z + 82.f));   // an ironing board, left up
        for (const float DX : { 20.f, 140.f }) HouseCampusFallKit::Bar(Solid, TEXT("CI_Steel"), FVector(1600.f + DX, 37.f, Z), FVector(1600.f + 160.f - DX, 37.f, Z + 80.f), 1.5f, 1.5f, FVector(0, 1, 0));
    }
    // ---- the corridors: troffers (one on its way out), EXIT signs at the stairs, a fountain, notices, extinguishers, the
    // payphones by the laundry; homecoming streamers down the front corridor.
    int32 Tube = 0;
    for (float X = -1700.f; X <= 1700.f; X += 300.f)
        for (const float Y : { HarCorN, HarCorS })
            Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), true, (++Tube == (K == 0 ? 9 : 17)) ? FName(TEXT("CI_TrofferFlicker")) : FName());
    for (const float X : { -1600.f, 1600.f }) for (const float Y : { -150.f, 250.f }) Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), false);
    // Troffers in the core rooms; the laundry has a tube going.
    for (const float X : { -1100.f, -840.f, -580.f }) for (const float Y : { -200.f, 200.f }) Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), true);
    for (const float X : { -180.f, 180.f }) for (const float Y : { -200.f, 200.f }) Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), true, (K == 0 && X > 0.f && Y > 0.f) ? FName(TEXT("CI_TrofferFlicker")) : FName());
    for (const float X : { 600.f, 840.f, 1080.f }) for (const float Y : { -200.f, 200.f }) Troffer(Glow, Clutter, FVector(X, Y, Ceil - .6f), true);
    if (K == 0)
    {
        for (const float X : { -150.f, 150.f }) Troffer(Glow, Clutter, FVector(X, 900.f, Ceil - .6f), false);
        for (const float X : { 600.f, 900.f, 1200.f }) Troffer(Glow, Clutter, FVector(X, 900.f, Ceil - .6f), false);
        Troffer(Glow, Clutter, FVector(-572.f, 900.f, Ceil - .6f), false);
    }
    const FVector WestDoor(-1380.f, K == 0 ? HarYS2 - PartT * .5f : HarYN2 + PartT * .5f, Z + 252.f), EastDoor(1380.f, K == 0 ? HarYN2 + PartT * .5f : HarYS2 - PartT * .5f, Z + 252.f);
    ExitSign(Solid, Glow, WestDoor + FVector(0.f, (K == 0 ? -6.f : 6.f), 0.f), FVector(0.f, K == 0 ? -1.f : 1.f, 0.f));
    ExitSign(Solid, Glow, EastDoor + FVector(0.f, (K == 0 ? 6.f : -6.f), 0.f), FVector(0.f, K == 0 ? 1.f : -1.f, 0.f));
    WaterFountain(Solid, FVector(-560.f, HarYN2 + PartT * .5f, Z), FVector(0, 1, 0));
    BulletinBoard(Solid, Clutter, FVector(-250.f, HarYS2 - PartT * .5f, Z + 150.f), FVector(0, -1, 0), 160.f, 90.f, TEXT("CI_NoticeBoard"));
    WallClock(Clutter, FVector(-250.f, HarYS2 - PartT * .5f, Z + 258.f), FVector(0, -1, 0), 15.f);
    WallClock(Clutter, FVector(-800.f, HarYN2 + PartT * .5f, Z + 258.f), FVector(0, 1, 0), 15.f);
    Kit.Floor(EKit::FireExtinguisher, FVector2D(1000.f, HarYN2 + PartT * .5f + 10.f), Z + 80.f, 90.f, false);
    Kit.Floor(EKit::FireExtinguisher, FVector2D(-1000.f, HarYS2 - PartT * .5f - 10.f), Z + 80.f, -90.f, false);
    for (const float S : { -1.f, 1.f }) Box(Solid, TEXT("CI_Steel"), FVector(S * 1000.f - 8.f, S > 0.f ? HarYN2 + PartT * .5f : HarYS2 - PartT * .5f - 2.f, Z + 74.f), FVector(S * 1000.f + 8.f, S > 0.f ? HarYN2 + PartT * .5f + 2.f : HarYS2 - PartT * .5f, Z + 80.f));
    if (K == 0)
    {
        for (const float X : { 270.f, 330.f }) PayPhone(Solid, Clutter, FVector(X, HarYN2 + PartT * .5f, Z + 140.f), FVector(0, 1, 0));
        for (float X = -1300.f; X < 1300.f; X += 520.f) Streamer(Clutter, FVector(X, HarYN1 - 9.f, Ceil - 10.f), FVector(X + 520.f, HarYN2 + 9.f, Ceil - 10.f), int32((X + 1300.f) / 520.f) % 2 == 0);
    }
    else
        HouseCampusFallKit::Card(Clutter, TEXT("CI_HarBeat"), FVector(0.f, HarYN1 - PartT * .5f - .5f, Z + 250.f), FVector(-1, 0, 0), FVector::UpVector, 300.f, 60.f, FVector(0, -1, 0));
}

void BuildHarlow(int32 Seed, float Height, FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit)
{
    const float F0 = HouseCampusInteriors::FloorTop(Height, 0), F1 = HouseCampusInteriors::FloorTop(Height, 1);
    const FBox2D In = Rect(-HarInX, HarInX, -HarInY, HarInY);
    const FBox2D Lobby = Rect(HarBay[4] + PartT * .5f, HarBay[6] - PartT * .5f, HarYN1 + PartT * .5f, HarInY);
    const FBox2D Bath = Rect(HarCX[1] + PartT * .5f, HarCX[2] - PartT * .5f, HarYS2 + PartT * .5f, HarYN2 - PartT * .5f);
    // Shell inside: painted block on the outer walls, the slabs (vinyl tile; the lobby terrazzo, the bathrooms hex tile), the
    // dropped ceilings (open over the stair shafts on the ground floor), and steel stools at the windows.
    HouseCampusInteriors::OpenWalls(Solid, TEXT("CI_Cinderblock"), Seed, OuterSkin, WallT, FHallUV::World(80.f));
    FloorSlab(Solid, In, F0, 70.f, { Lobby, Bath }, TEXT("CI_VCT"), TEXT("CI_Plaster"), FHallUV::World(61.f));
    FloorSlab(Solid, Lobby, F0, 70.f, {}, TEXT("CI_Terrazzo"), TEXT("CI_Plaster"), FHallUV::World(220.f));
    FloorSlab(Solid, Bath, F0, 70.f, {}, TEXT("CI_HexTile"), TEXT("CI_Plaster"), FHallUV::World(30.f));
    FloorSlab(Solid, In, F1, 30.f, { HarWell(true), HarWell(false), Bath }, TEXT("CI_VCT"), TEXT("CI_CeilingTile"), FHallUV::World(61.f));
    FloorSlab(Solid, Bath, F1, 30.f, {}, TEXT("CI_HexTile"), TEXT("CI_CeilingTile"), FHallUV::World(30.f));
    SubtractPlan(In, { HarShaft(true), HarShaft(false) }, [&](const FBox2D& P) { Box(Solid, TEXT("CI_CeilingTile"), FVector(P.Min, F0 + HarCeil), FVector(P.Max, F0 + HarCeil + 1.f), FHallUV::World(122.f)); });
    Box(Solid, TEXT("CI_CeilingTile"), FVector(In.Min, F1 + HarCeil), FVector(In.Max, F1 + HarCeil + 1.f), FHallUV::World(122.f));
    for (int32 K = 0; K < 2; ++K) HarWindowStools(Solid, Seed, Height, K);
    // The walls of both floors; the stair shafts closed above the ground floor's ceiling, up to the slab.
    HarWalls(Solid, 0, F0, F1 + HarCeil + 1.f);
    HarWalls(Solid, 1, F1, F1 + HarCeil + 1.f);
    for (const bool bWest : { true, false })
    {
        const FBox2D S = HarShaft(bWest);
        for (const float Y : { HarYS2, HarYN2 })
            Box(Solid, TEXT("CI_Cinderblock"), FVector(S.Min.X, Y - PartT * .5f, F0 + HarCeil), FVector(S.Max.X, Y + PartT * .5f, F1 - 30.f), FHallUV::World(80.f));
    }
    // The stairs: the west flight climbs north from the back corridor, the east one south from the front corridor; a steel
    // handrail on each shaft wall along the pitch.
    Stair(Solid, -1448.5f, -1311.5f, -380.f, 292.f, F0, F1);
    Stair(Solid, 1311.5f, 1448.5f, 350.f, -322.f, F0, F1);
    for (const float X : { -1450.f, -1310.f })
        Clutter.Cylinder(TEXT("CI_Steel"), FVector(X, -380.f, F0 + 90.f), FVector(X, 292.f, F1 + 90.f), 2.5f, 8, UVPlain());
    for (const float X : { 1310.f, 1450.f })
        Clutter.Cylinder(TEXT("CI_Steel"), FVector(X, 350.f, F0 + 90.f), FVector(X, -322.f, F1 + 90.f), 2.5f, 8, UVPlain());
    // Rooms: a number by every door; the open ones furnished.
    for (int32 K = 0; K < 2; ++K)
    {
        const float Z = K == 0 ? F0 : F1;
        for (const bool bNorth : { true, false })
            for (int32 Bay = 0; Bay < 10; ++Bay)
            {
                const FHarRoom R = HarRoom(K, bNorth, Bay);
                if (R.Number == 0) continue;
                const FVector2D DoorU = HarDoorU(Bay);
                const float Wall = bNorth ? HarYN1 - PartT * .5f : HarYS1 + PartT * .5f;
                HarPlate(Clutter, R.Number, FVector(DoorU.Y + 16.f, Wall, Z + 150.f), FVector(0.f, bNorth ? -1.f : 1.f, 0.f));
                if (!R.bOpen)
                {
                    // A few doors dressed for the game.
                    if (R.Number % 5 == 1) HarPoster(Clutter, TEXT("CI_HarGoGeese"), FVector(DoorU.Y + 40.f, Wall, Z + 120.f), FVector(0.f, bNorth ? -1.f : 1.f, 0.f), 40.f, 50.f);
                    continue;
                }
                const FHarFrame F = HarFrameOf(bNorth, Bay);
                HarFurnish(Solid, Clutter, Glow, Kit, F, Z, -1652.5f + 360.f * Bay - F.XL, HarStyle(R.Number), R.Number);
            }
        HarCore(Solid, Clutter, Glow, Kit, K, Z);
    }
    HarFront(Solid, Clutter, Glow, Kit, F0);
}

void HarlowOpenings(int32 Seed, float Height, TArray<HouseCampusInteriors::FOpening>& Out)
{
    const float F0 = HouseCampusInteriors::FloorTop(Height, 0);
    Out.Add({ 0, -120.f, 120.f, F0, F0 + 213.f });   // the main doors into the lobby
}
void HarlowDoors(int32 Seed, float Height, TArray<FDoorSpec>& Out)
{
    const FBox2D B = HouseCampusInteriors::CanonRect(Seed);
    TArray<HouseCampusInteriors::FOpening> Openings;
    HarlowOpenings(Seed, Height, Openings);
    MainDoorPair(TEXT("HAR"), B, Openings[0], Out);
    const uint8 Wood = AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::KickPlate;
    static const TCHAR* const Reasons[] = {
        TEXT("Room %d - a note taped to it: AT THE GAME, BACK LATE"), TEXT("Room %d - nobody's home"), TEXT("Room %d - DO NOT DISTURB on the knob"),
        TEXT("Room %d - locked; a radio playing inside"), TEXT("Room %d - locked tight"), TEXT("Room %d - GONE HOME FOR THE WEEKEND") };
    for (int32 K = 0; K < 2; ++K)
    {
        const float Z = HouseCampusInteriors::FloorTop(Height, K);
        for (const bool bNorth : { true, false })
            for (int32 Bay = 0; Bay < 10; ++Bay)
            {
                const FHarRoom R = HarRoom(K, bNorth, Bay);
                const FVector2D U = HarDoorU(Bay);
                if (R.Number == 0)
                {
                    if (K == 0 && bNorth && Bay == 3)
                        InnerDoor(Out, TEXT("DeskOffice"), false, HarYN1, U.X, U.Y, Z, 1.f, EHallDoorStyle::Equipment, Wood, TEXT("HALL STAFF ONLY - ring at the desk"));
                    continue;
                }
                const FString Reason = R.bOpen ? FString() : FString(Reasons[(R.Number * 7) % UE_ARRAY_COUNT(Reasons)]).Replace(TEXT("%d"), *FString::FromInt(R.Number));
                InnerDoor(Out, FName(*FString::Printf(TEXT("Room%d"), R.Number)), false, bNorth ? HarYN1 : HarYS1, U.X, U.Y, Z, bNorth ? 1.f : -1.f, EHallDoorStyle::Equipment, Wood, Reason, R.bOpen);
            }
    }
}
bool HarlowWindowOpen(int32 Seed, int32 Side, int32 K, float U0, float U1)
{
    // A locked room's window stays an interior-mapped one; every other window of the open floors is real.
    const float U = (U0 + U1) * .5f;
    if (Side <= 1)
    {
        int32 Bay = 0;
        while (Bay < 9 && U > HarBay[Bay + 1]) ++Bay;
        const FHarRoom R = HarRoom(K, Side == 0, Bay);
        return R.Number == 0 || R.bOpen;
    }
    if (U > HarYN1 || U < HarYS1)
    {
        const FHarRoom R = HarRoom(K, U > HarYN1, Side == 2 ? 0 : 9);
        return R.Number == 0 || R.bOpen;
    }
    return true;
}

UTexture2D* HarTexture(UHouseBlackHallSubsystem* Look, FName Key, const FHallImage& Img, bool bWrap, float Rough, const FLinearColor& Emission = FLinearColor::Black)
{
    UTexture2D* Tex = HallMakeTexture(Img, true, false, *Key.ToString(), bWrap);
    if (Tex) Look->Textures.Add(Key, Tex);
    HallMakeTextured(Look, Key, Tex, nullptr, Rough, FLinearColor::White, 0.f, 0.f, Emission);
    return Tex;
}

void HarlowLook(UHouseBlackHallSubsystem* Look)
{
    if (!Look || Look->Materials.Contains(TEXT("CI_Cinderblock"))) return;
    FRandomStream R(1964);
    {
        // Painted cinderblock: 40 x 20 cm blocks in running bond (the texture is 80 cm square), cream, the joints shadowed.
        FHallImage Img; Img.Init(256, 256, FLinearColor(.47f, .44f, .36f));
        for (int32 Row = 0; Row < 4; ++Row)
            for (int32 Col = -1; Col < 2; ++Col)
            {
                const float X0 = Col * 128.f + (Row % 2 ? 64.f : 0.f), Y0 = Row * 64.f;
                const float Shade = R.FRandRange(.96f, 1.03f);
                Img.Rect(FVector2D(X0 + 2.f, Y0 + 2.f), FVector2D(X0 + 126.f, Y0 + 62.f), FLinearColor(.63f, .6f, .5f) * Shade);
            }
        for (int32 K = 0; K < 2600; ++K) { const int32 X = R.RandRange(0, 255), Y = R.RandRange(0, 255); Img.At(X, Y) = Img.At(X, Y) * R.FRandRange(.82f, .95f); }
        HarTexture(Look, TEXT("CI_Cinderblock"), Img, true, .82f);
    }
    {
        // Vinyl composition tile, 30 cm squares (the texture is 61 cm), two tones with a speckle; scuffed.
        FHallImage Img; Img.Init(256, 256, FLinearColor(.5f, .47f, .4f));
        for (int32 TY = 0; TY < 2; ++TY) for (int32 TX = 0; TX < 2; ++TX)
        {
            const FLinearColor C = (TX + TY) % 2 ? FLinearColor(.52f, .49f, .42f) : FLinearColor(.44f, .42f, .37f);
            Img.Rect(FVector2D(TX * 128 + 1, TY * 128 + 1), FVector2D(TX * 128 + 127, TY * 128 + 127), C);
        }
        for (int32 K = 0; K < 3000; ++K) { const int32 X = R.RandRange(0, 255), Y = R.RandRange(0, 255); Img.At(X, Y) = R.FRand() < .5f ? FLinearColor(.3f, .28f, .25f) : FLinearColor(.66f, .63f, .56f); }
        HarTexture(Look, TEXT("CI_VCT"), Img, true, .45f);
    }
    {
        // White hexagonal mosaic (2.5 cm), grey grout; the texture is 30 cm.
        FHallImage Img; Img.Init(256, 256, FLinearColor(.42f, .42f, .4f));
        for (int32 Row = 0; Row < 12; ++Row) for (int32 Col = 0; Col < 13; ++Col)
            Img.Ellipse(FVector2D(Col * 21.33f + (Row % 2 ? 10.66f : 0.f), Row * 21.33f + 10.f), FVector2D(9.5f, 9.5f), 0.f, FLinearColor(.78f, .78f, .75f) * R.FRandRange(.95f, 1.02f));
        HarTexture(Look, TEXT("CI_HexTile"), Img, true, .3f);
    }
    {
        // Glazed wall tile, 15 cm, mint green (the texture is 60 cm).
        FHallImage Img; Img.Init(256, 256, FLinearColor(.4f, .44f, .4f));
        for (int32 Row = 0; Row < 4; ++Row) for (int32 Col = 0; Col < 4; ++Col)
            Img.Rect(FVector2D(Col * 64 + 2, Row * 64 + 2), FVector2D(Col * 64 + 62, Row * 64 + 62), FLinearColor(.5f, .62f, .52f) * R.FRandRange(.97f, 1.02f));
        HarTexture(Look, TEXT("CI_WallTile"), Img, true, .15f);
    }
    {
        // Chicken wire: a hexagonal net of wire (for the float).
        FHallImage Img; Img.Init(64, 64, FLinearColor(.12f, .1f, .09f));
        for (int32 Row = 0; Row < 4; ++Row) for (int32 Col = 0; Col < 4; ++Col)
            Img.Ellipse(FVector2D(Col * 16.f + (Row % 2 ? 8.f : 0.f), Row * 16.f + 8.f), FVector2D(7.f, 7.f), 0.f, FLinearColor(.55f, .55f, .52f));
        for (int32 Row = 0; Row < 4; ++Row) for (int32 Col = 0; Col < 4; ++Col)
            Img.Ellipse(FVector2D(Col * 16.f + (Row % 2 ? 8.f : 0.f), Row * 16.f + 8.f), FVector2D(5.5f, 5.5f), 0.f, FLinearColor(.12f, .1f, .09f));
        HarTexture(Look, TEXT("CI_ChickenWire"), Img, true, .5f);
    }
    {
        // Room number plates: 8 x 5 cells, white letters cut into brown plastic.
        FHallImage Img; Img.Init(768, 240, FLinearColor(.12f, .08f, .05f));
        for (int32 I = 0; I < 40; ++I)
        {
            const int32 N = HarPlateNumber(I);
            if (!N) continue;
            const float X0 = (I % 8) * 96.f, Y0 = (I / 8) * 48.f;
            Img.Rect(FVector2D(X0 + 3, Y0 + 3), FVector2D(X0 + 93, Y0 + 45), FLinearColor(.75f, .72f, .65f));
            Img.Rect(FVector2D(X0 + 5, Y0 + 5), FVector2D(X0 + 91, Y0 + 43), FLinearColor(.12f, .08f, .05f));
            FHallTextStyle S; S.Height = 24.f; S.Stroke = 3.4f; S.Spacing = .22f;
            const FString T = FString::FromInt(N);
            HallDrawText(Img, T, FVector2D(X0 + (96.f - HallTextWidth(T, S)) * .5f, Y0 + 24.f + 12.f), S, FLinearColor(.85f, .82f, .75f));
        }
        HarTexture(Look, TEXT("CI_HarPlates"), Img, false, .4f);
    }
    {
        // The Geese pennant: purple felt, GEESE in gold along it, a gold stripe at the hoist.
        FHallImage Img; Img.Init(256, 96, FLinearColor(.2f, .05f, .32f));
        Img.Rect(FVector2D(0, 0), FVector2D(18, 96), FLinearColor(.78f, .55f, .08f));
        FHallTextStyle S; S.Height = 30.f; S.Stroke = 5.f; S.Spacing = .2f;
        HallDrawText(Img, TEXT("GEESE"), FVector2D(32.f, 48.f + 15.f), S, FLinearColor(.85f, .62f, .12f));
        HarTexture(Look, TEXT("CI_Pennant"), Img, false, .9f);
    }
    {
        // A key cabinet: rows of brass hooks, a tag on each.
        FHallImage Img; Img.Init(128, 112, FLinearColor(.16f, .1f, .05f));
        for (int32 Row = 0; Row < 6; ++Row) for (int32 Col = 0; Col < 8; ++Col)
        {
            const float X = 8.f + Col * 15.f, Y = 8.f + Row * 17.f;
            Img.Rect(FVector2D(X + 4, Y), FVector2D(X + 6, Y + 4), FLinearColor(.6f, .45f, .15f));
            if (R.FRand() < .8f) Img.Rect(FVector2D(X + 2, Y + 5), FVector2D(X + 9, Y + 13), FLinearColor(.75f, .7f, .55f));
        }
        HarTexture(Look, TEXT("CI_HarKeys"), Img, false, .5f);
    }
    {
        // October 1986 on a wall calendar (it began on a Wednesday).
        FHallImage Img; Img.Init(160, 208, FLinearColor(.85f, .83f, .76f));
        Img.Rect(FVector2D(8, 8), FVector2D(152, 80), FLinearColor(.3f, .45f, .55f));
        FHallTextStyle S; S.Height = 14.f; S.Stroke = 2.2f; S.Spacing = .2f;
        HallDrawText(Img, TEXT("OCTOBER 1986"), FVector2D((160.f - HallTextWidth(TEXT("OCTOBER 1986"), S)) * .5f, 100.f), S, FLinearColor(.1f, .1f, .12f));
        for (int32 D = 0; D < 31; ++D)
        {
            const int32 Cell = D + 3, X = 10 + (Cell % 7) * 20, Y = 112 + (Cell / 7) * 18;
            Img.Rect(FVector2D(X, Y), FVector2D(X + 17, Y + 15), D == 17 ? FLinearColor(.75f, .2f, .15f) : FLinearColor(.72f, .7f, .64f));
        }
        HarTexture(Look, TEXT("CI_HarCalendar"), Img, false, .9f);
    }
    PaintText(Look, TEXT("CI_PizzaLid"), 128, 128, FLinearColor(.82f, .78f, .68f), FLinearColor(.6f, .1f, .08f), { { TEXT("HOT"), .2f }, { TEXT("PIZZA"), .22f } }, true);
    // Posters for bands that never were, the Dunmore game, the float.
    PaintText(Look, TEXT("CI_PosterCanoes"), 220, 300, FLinearColor(.08f, .1f, .2f), FLinearColor(.95f, .55f, .2f), { { TEXT("THE TIN"), .1f }, { TEXT("CANOES"), .12f }, { TEXT("FALL TOUR 86"), .06f } }, true);
    PaintText(Look, TEXT("CI_PosterGravel"), 220, 300, FLinearColor(.75f, .7f, .55f), FLinearColor(.15f, .1f, .08f), { { TEXT("GRAVEL"), .12f }, { TEXT("SAINTS"), .12f }, { TEXT("LIVE AT THE ARMORY"), .05f } }, true);
    PaintText(Look, TEXT("CI_PosterMercury"), 220, 300, FLinearColor(.5f, .08f, .1f), FLinearColor(.95f, .9f, .75f), { { TEXT("MERCURY"), .11f }, { TEXT("BICYCLE"), .11f }, { TEXT("SIDE ONE"), .06f } }, false);
    PaintText(Look, TEXT("CI_PosterTractor"), 220, 300, FLinearColor(.2f, .35f, .2f), FLinearColor(.95f, .85f, .3f), { { TEXT("VELVET"), .12f }, { TEXT("TRACTOR"), .12f }, { TEXT("OCT 31 - 8PM"), .06f } }, true);
    PaintText(Look, TEXT("CI_PosterBeat"), 400, 200, FLinearColor(.85f, .82f, .75f), FLinearColor(.25f, .06f, .38f), { { TEXT("BEAT"), .22f }, { TEXT("DUNMORE"), .26f } }, false);
    PaintText(Look, TEXT("CI_HarGoGeese"), 160, 200, FLinearColor(.78f, .55f, .1f), FLinearColor(.2f, .05f, .32f), { { TEXT("GO"), .2f }, { TEXT("GEESE"), .17f } }, false);
    PaintText(Look, TEXT("CI_HarFloatBanner"), 512, 240, FLinearColor(.88f, .86f, .8f), FLinearColor(.25f, .06f, .38f), { { TEXT("GO GEESE"), .22f }, { TEXT("HONK IF YOU"), .1f } }, false);
    PaintText(Look, TEXT("CI_HarAlumni"), 1024, 104, FLinearColor(.85f, .83f, .78f), FLinearColor(.25f, .06f, .38f), { { TEXT("WELCOME HOME ALUMNI"), .5f } }, false);
    PaintText(Look, TEXT("CI_HarBeat"), 512, 104, FLinearColor(.85f, .83f, .78f), FLinearColor(.25f, .06f, .38f), { { TEXT("HARLOW SAYS BEAT DUNMORE"), .4f } }, false);
    const FLinearColor SignBg(.12f, .09f, .06f), Cream(.85f, .82f, .72f);
    PaintText(Look, TEXT("CI_SignHarlow"), 768, 118, SignBg, FLinearColor(.78f, .58f, .18f), { { TEXT("HARLOW HALL"), .42f } }, true);
    PaintText(Look, TEXT("CI_HarDesk"), 256, 88, SignBg, Cream, { { TEXT("DESK"), .45f } }, true);
    PaintText(Look, TEXT("CI_HarMen"), 256, 100, SignBg, Cream, { { TEXT("MEN"), .45f } }, true);
    PaintText(Look, TEXT("CI_HarWomen"), 256, 100, SignBg, Cream, { { TEXT("WOMEN"), .45f } }, true);
    PaintText(Look, TEXT("CI_HarLaundry"), 320, 92, SignBg, Cream, { { TEXT("LAUNDRY"), .42f } }, true);
    PaintText(Look, TEXT("CI_HarStudy"), 384, 92, SignBg, Cream, { { TEXT("QUIET STUDY"), .4f } }, true);
    PaintText(Look, TEXT("CI_HarRec"), 420, 92, SignBg, Cream, { { TEXT("RECREATION"), .4f } }, true);
    PaintText(Look, TEXT("CI_HarKitchen"), 352, 92, SignBg, Cream, { { TEXT("KITCHEN"), .42f } }, true);
    PaintText(Look, TEXT("CI_HarLounge"), 320, 92, SignBg, Cream, { { TEXT("LOUNGE"), .42f } }, true);
    HallMakePlain(Look, TEXT("CI_StallSteel"), FLinearColor(.42f, .34f, .2f), .35f, .3f);    // baked enamel, harvest gold
    HallMakePlain(Look, TEXT("CI_Mirror"), FLinearColor(.9f, .9f, .9f), .03f, 1.f);
    HallMakePlain(Look, TEXT("CI_Porcelain"), FLinearColor(.78f, .78f, .75f), .12f);
    HallMakePlain(Look, TEXT("CI_ShowerCurtain"), FLinearColor(.25f, .4f, .45f), .6f);
    HallMakePlain(Look, TEXT("CI_Cardboard"), FLinearColor(.42f, .3f, .17f), .9f);
    HallMakePlain(Look, TEXT("CI_TissuePurple"), FLinearColor(.3f, .08f, .45f), .9f);
    HallMakePlain(Look, TEXT("CI_TissueGold"), FLinearColor(.85f, .62f, .1f), .9f);
    HallMakePlain(Look, TEXT("CI_TissueWhite"), FLinearColor(.85f, .85f, .82f), .9f);
    HallMakePlain(Look, TEXT("CI_PingPong"), FLinearColor(.03f, .18f, .08f), .5f);
    HallMakePlain(Look, TEXT("CI_Net"), FLinearColor(.75f, .75f, .72f), .9f);
    HallMakePlain(Look, TEXT("CI_Football"), FLinearColor(.25f, .1f, .04f), .6f);
    HallMakePlain(Look, TEXT("CI_CanRed"), FLinearColor(.5f, .05f, .04f), .3f, .6f);
    HallMakePlain(Look, TEXT("CI_CanSilver"), FLinearColor(.6f, .6f, .62f), .3f, .9f);
    HallMakePlain(Look, TEXT("CI_DomeLight"), FLinearColor(.9f, .88f, .82f), .4f, 0.f, FLinearColor(4.f, 3.7f, 3.1f));
}
/** The men's room's dripping tap, the TV lounge's set still on the replay, room 101's boombox on the postgame show. */
void HarlowSounds(int32 Seed, float Height, TArray<FSoundSpot>& Out)
{
    const float F0 = HouseCampusInteriors::FloorTop(Height, 0);
    Out.Add({ 0, FVector(-700.f, HarYN2 - PartT * .5f - 25.f, F0 + 88.f) });
    Out.Add({ 1, FVector(1374.f, 900.f, F0 + 50.f) });
    Out.Add({ 2, FVector(-1690.f, 674.5f, F0 + 90.f) });
}
FInteriorDef HarlowDef()
{
    FInteriorDef D{ TEXT("HAR"), 2, &BuildHarlow, &HarLightAnchors, &HarlowOpenings, &HarlowDoors, &HarlowLook };
    D.WindowOpen = &HarlowWindowOpen;
    D.Sounds = &HarlowSounds;
    return D;
}
}
