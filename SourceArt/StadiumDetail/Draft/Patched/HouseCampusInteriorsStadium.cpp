// The walk-in stadium (master job, step 2c, Tier A): Calder State's football stadium after the homecoming game, its north
// side open. A free-standing interior in world space (the bowl is the exterior's, HouseExteriorCampus: an ellipse 5600 x
// 3800 round (2400, -13300), 40 segments of 9 degrees, twelve rows of seats rising from the podium wall to the rim).
//
// - The concourse runs behind the north stands (segments 7..12, under rows 8..11), entered through GATE 4 (turnstiles) and
//   GATE 5, its window band lit. Along its field-side wall: the GEESE GRILL, the popcorn stand, the restrooms (locked for
//   the night), the cold-drinks stand (shuttered), the Geese locker room's door, and the stair up to the vomitory in
//   segment 11, where rows 4..7 give way to a landing that looks over the field.
// - The Geese locker room is under the east stands (50..66 degrees): lockers and benches, helmets, tape on the floor, the
//   board where BEAT DUNMORE is crossed out and 24-17 written under it, the game ball on its stand; its tunnel opens through
//   the podium wall onto the track.
// - The press box on the rim (the exterior's box, hollow) has the press row of typewriters and phones along its windows,
//   the radio booth (the postgame show on air), the PA and scoreboard desk, spotters' charts; a brick stair tower beside it
//   climbs from the ground.
// Lit by emission and the shared pool. Built at runtime; never saved.
#include "HouseStadiumDetail.h"
#include "HouseCampusInteriorsRooms.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusFallKit.h"
#include "HouseExteriorCampus.h"
#include "Engine/Texture2D.h"

namespace HouseCampusInteriorsImpl
{
constexpr float StaRX = 5600.f, StaRY = 3800.f, StaGround = -60.f;
constexpr int32 StaSeg = 40;
// The concourse: segments K0..K1, between the field-side wall (on the RIn chord, its concourse face 25 out) and the outer
// skin (on the ROut chord, 25 in), floor and ceiling heights.
constexpr int32 StaK0 = 7, StaK1 = 12;
constexpr float StaRIn = .885f, StaROut = .995f, StaFloor = -55.f, StaCeil = 640.f, StaWallT = 25.f;
// The vomitory: segment 11, rows 4..7 cut; its landing at the top of row 3, the mouth in the concourse wall.
constexpr int32 StaVomK = 11, StaVomRow0 = 4, StaVomRow1 = 7;
constexpr float StaVomZ = StaGround + 240.f + 63.f * 3.f, StaVomR0 = .69f + .024f * 4.f, StaVomR1 = .69f + .024f * 8.f;
constexpr float StaMouth0 = 283.f, StaMouth1 = 483.f, StaStairBottom = 1133.f, StaStairD0 = StaWallT, StaStairD1 = StaWallT + 130.f;
// The locker room: 50..66 degrees, from the podium wall to the concourse's line, under row 0; the tunnel in segment 6.
constexpr float StaLockA0 = 50.f, StaLockA1 = 66.f, StaLockR0 = .695f, StaLockR1 = StaRIn, StaLockCeil = 175.f;
constexpr int32 StaTunnelK = 6;
// The press box (the exterior's box, hollow) and the stair tower beside its east end.
constexpr float StaPBX0 = 100.f, StaPBX1 = 4700.f, StaPBY0 = -10000.f, StaPBY1 = -9250.f, StaPBFloor = 1065.f, StaPBCeil = 1480.f, StaPBTop = 1830.f;
constexpr float StaTwX0 = 4700.f, StaTwX1 = 4960.f, StaTwY0 = -9640.f, StaTwY1 = -9100.f, StaTwTop = 1400.f;

float StaAngle(int32 K) { return K * 2.f * PI / StaSeg; }
FVector StaAt(float A, float R, float Z) { return FVector(HouseCampus::StadiumX + FMath::Cos(A) * StaRX * R, HouseCampus::StadiumY + FMath::Sin(A) * StaRY * R, Z); }
/** The point at bowl angle A (radians) and radius R in the stadium's polar terms of P (the inverse of StaAt). */
void StaPolar(const FVector& P, float& A, float& R)
{
    const float DX = (P.X - HouseCampus::StadiumX) / StaRX, DY = (P.Y - HouseCampus::StadiumY) / StaRY;
    R = FMath::Sqrt(DX * DX + DY * DY);
    A = FMath::Atan2(DY, DX);
}
/** A bowl segment's chord at radius R: its ends, Along it, the outward normal N, its length; At(U, D, Z) a point U along it,
 *  D out from it. Slab builds a box U0..U1 along, D0..D1 out, Z0..Z1. */
struct FStaChord
{
    FVector A, B, Along, N;
    float Len = 0.f;
    FStaChord(int32 K, float R)
    {
        A = StaAt(StaAngle(K), R, 0.f); B = StaAt(StaAngle(K + 1), R, 0.f);
        Along = (B - A).GetSafeNormal(); Len = FVector::Dist(A, B);
        N = FVector(Along.Y, -Along.X, 0.f);   // the bowl runs anticlockwise, so Along's right points out
    }
    FVector At(float U, float D, float Z) const { return A + Along * U + N * D + FVector(0, 0, Z); }
    void Slab(FHallMesh& M, FName Key, float U0, float U1, float D0, float D1, float Z0, float Z1, const FHallUV& UV = UVPlain()) const
    {
        if (U1 - U0 < .1f || D1 - D0 < .05f || Z1 - Z0 < .05f) return;
        M.Box(Key, FTransform(Along.Rotation(), At((U0 + U1) * .5f, (D0 + D1) * .5f, (Z0 + Z1) * .5f)), FVector((U1 - U0) * .5f, (D1 - D0) * .5f, (Z1 - Z0) * .5f), UV);
    }
    /** A wall U0..U1 x Z0..Z1 between depths D0..D1, less the Holes (U, Z boxes). */
    void Wall(FHallMesh& M, FName Key, float U0, float U1, float D0, float D1, float Z0, float Z1, const TArray<FBox2D>& Holes, const FHallUV& UV = UVPlain()) const
    {
        Subtract(U0, U1, Z0, Z1, Holes, [&](float UA, float UB, float ZA, float ZB) { Slab(M, Key, UA, UB, D0, D1, ZA, ZB, UV); });
    }
    /** The yaw of a piece whose front faces out (Out true) or in. */
    float Yaw(bool bOut) const { return FMath::RadiansToDegrees(FMath::Atan2(bOut ? N.Y : -N.Y, bOut ? N.X : -N.X)); }
};
/** A flat band between a segment's chords at R0 and R1 over the fraction T0..T1 of each, at Z, facing up or down. */
void StaBand(FHallMesh& M, FName Key, int32 K, float R0, float R1, float T0, float T1, float Z, bool bUp, const FHallUV& UV)
{
    const FStaChord I(K, R0), O(K, R1);
    const FVector P0 = FMath::Lerp(I.A, I.B, T0), P1 = FMath::Lerp(I.A, I.B, T1), Q1 = FMath::Lerp(O.A, O.B, T1), Q0 = FMath::Lerp(O.A, O.B, T0);
    const FVector H(0, 0, Z);
    M.Quad(Key, P0 + H, P1 + H, Q1 + H, Q0 + H, bUp ? FVector::UpVector : -FVector::UpVector, UV);
}
/** A wall along the radius at angle A (radians) from R0 to R1, T thick, Z0..Z1, offset Shift along the bowl's direction. */
void StaRadialWall(FHallMesh& M, FName Key, float A, float R0, float R1, float T, float Z0, float Z1, float Shift, const TArray<FBox2D>& Holes = {})
{
    const FVector P0 = StaAt(A, R0, 0.f), P1 = StaAt(A, R1, 0.f), Out = (P1 - P0).GetSafeNormal(), Side(-Out.Y, Out.X, 0.f);
    const float L = FVector::Dist(P0, P1);
    const FRotator Rot = Out.Rotation();
    Subtract(0.f, L, Z0, Z1, Holes, [&](float UA, float UB, float ZA, float ZB)
    {
        M.Box(Key, FTransform(Rot, P0 + Out * ((UA + UB) * .5f) + Side * Shift + FVector(0, 0, (ZA + ZB) * .5f)), FVector((UB - UA) * .5f, T * .5f, (ZB - ZA) * .5f), UVPlain());
    });
}
/** The segment that holds bowl angle A (degrees) and A's fraction along it. */
void StaSegOf(float ADeg, int32& K, float& T) { K = FMath::Clamp(FMath::FloorToInt(ADeg / 9.f), 0, StaSeg - 1); T = (ADeg - K * 9.f) / 9.f; }

/** A straight flight along a chord's frame (U from UBottom to UTop, D0..D1 out), solid to the floor, with a stepped guard
 *  on its open side (at D1) and a rail. */
void StaFlight(FHallMesh& Solid, FHallMesh& Clutter, const FStaChord& C, float UBottom, float UTop, float D0, float D1, float ZBottom, float ZTop)
{
    const int32 N = FMath::CeilToInt((ZTop - ZBottom) / (HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6?18.f:17.5f));
    const float Rise = (ZTop - ZBottom) / N, Run = (UTop - UBottom) / N;
    for (int32 K = 0; K < N; ++K)
    {
        const float UA = UBottom + Run * K, UB = UA + Run, Z = ZBottom + Rise * (K + 1);
        C.Slab(Solid, TEXT("CI_StaConcrete"), FMath::Min(UA, UB), FMath::Max(UA, UB), D0, D1, ZBottom - 5.f, Z);
        C.Slab(Solid, TEXT("CI_StaNosing"), FMath::Min(UA, UA + Run * .1f), FMath::Max(UA, UA + Run * .1f), D0 + 2.f, D1 - 2.f, Z, Z + .3f);
        C.Slab(Solid, TEXT("CI_StaConcrete"), FMath::Min(UA, UB), FMath::Max(UA, UB), D1, D1 + 12.f, ZBottom - 5.f, Z + 95.f);   // the guard
    }
    Clutter.Cylinder(TEXT("CI_StaRail"), C.At(UBottom, D1 + 6.f, ZBottom + Rise + 100.f), C.At(UTop, D1 + 6.f, ZTop + 100.f), 2.5f, 8, UVPlain());
}

// ------------------------------------------------------------------ the concourse
/** A concession stand behind the field-side wall: its counter window (shutter up or down), the booth's walls and things,
 *  the menu over the window. Kind 0 the grill, 1 popcorn and candy, 2 cold drinks (shuttered). */
void StaConcession(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, int32 K, float U0, float U1, int32 Kind)
{
    using EKit = HouseCampusInteriors::EKit;
    const FStaChord C(K, StaRIn);
    const float Sill = StaFloor + 100.f, Head = StaFloor + 260.f, Back = -170.f, Roof = StaFloor + 310.f;
    // The booth behind the window (field side of the wall): floor, walls, ceiling.
    C.Slab(Solid, TEXT("CI_StaQuarry"), U0 - 20.f, U1 + 20.f, Back, 0.f, StaGround, StaFloor, FHallUV::World(40.f));
    C.Slab(Solid, TEXT("CI_StaBlock"), U0 - 35.f, U1 + 35.f, Back - 15.f, Back, StaGround, Roof, FHallUV::World(80.f));
    for (const float U : { U0 - 35.f, U1 + 20.f }) C.Slab(Solid, TEXT("CI_StaBlock"), U, U + 15.f, Back, 0.f, StaGround, Roof, FHallUV::World(80.f));
    C.Slab(Solid, TEXT("CI_StaConcrete"), U0 - 35.f, U1 + 35.f, Back - 15.f, 0.f, Roof, Roof + 15.f);
    Glow.Box(TEXT("CI_Troffer"), FTransform(C.Along.Rotation(), C.At((U0 + U1) * .5f, Back * .5f, Roof - .3f)), FVector((U1 - U0) * .3f, 20.f, .2f), UVPlain());
    // The counter: stainless top through the window, a ledge on the concourse side.
    C.Slab(Solid, TEXT("CI_Steel"), U0, U1, -70.f, StaWallT + 18.f, Sill - 4.f, Sill);
    C.Slab(Solid, TEXT("CI_StaBooth"), U0, U1, -70.f, -40.f, StaFloor, Sill - 4.f);
    // The roll-up shutter: half down over an open stand, right down over a shut one.
    const float ShutterBottom = Kind == 2 ? Sill : Head - 70.f;
    for (float Z = ShutterBottom; Z < Head; Z += 8.f) C.Slab(Clutter, TEXT("CI_StaShutter"), U0, U1, StaWallT + 1.f, StaWallT + 3.f, Z, FMath::Min(Head, Z + 7.5f));
    // The menu over the window.
    const FName Menu = Kind == 0 ? (HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=5?FName(TEXT("SD_Menu")):FName(TEXT("CI_StaMenuGrill"))) : Kind == 1 ? FName(TEXT("CI_StaMenuPopcorn")) : FName(TEXT("CI_StaMenuDrinks"));
    HouseCampusFallKit::Card(Glow, Menu, C.At((U0 + U1) * .5f, StaWallT + 1.f, Head + 45.f), -C.Along, FVector::UpVector, U1 - U0, 70.f, C.N);
    // What's inside.
    auto In = [&](float T, float D, float Z) { return C.At(FMath::Lerp(U0, U1, T), D, Z); };
    if (Kind == 0)
    {
        C.Slab(Solid, TEXT("CI_Steel"), U0 + 20.f, U0 + 150.f, Back + 5.f, Back + 70.f, StaFloor, StaFloor + 90.f);   // the grill
        C.Slab(Solid, TEXT("CI_Epoxy"), U0 + 25.f, U0 + 145.f, Back + 10.f, Back + 65.f, StaFloor + 90.f, StaFloor + 92.f);
        for (int32 I = 0; I < 6; ++I) Clutter.Cylinder(TEXT("CI_StaHotDog"), In(.1f + I * .04f, Back + 20.f, StaFloor + 93.f), In(.1f + I * .04f, Back + 55.f, StaFloor + 93.f), 1.4f, 8, UVPlain());
        Solid.Cylinder(TEXT("CI_Steel"), In(.75f, Back + 30.f, StaFloor), In(.75f, Back + 30.f, StaFloor + 150.f), 22.f, 14, UVPlain());   // the coffee urn
        for (const float T : { .3f, .45f }) Clutter.Cylinder(T < .4f ? TEXT("CI_StaMustard") : TEXT("CI_Ketchup"), In(T, -55.f, Sill), In(T, -55.f, Sill + 20.f), 3.f, 10, UVPlain());
        Box(Clutter, TEXT("CI_Beige"), In(.62f, -62.f, Sill), In(.62f, -42.f, Sill + 22.f) + C.Along * 30.f);   // the register
    }
    else if (Kind == 1)
    {
        // The popcorn machine: a lit glass case full of popcorn on a cart.
        const FVector P = In(.35f, -95.f, StaFloor);
        Solid.Box(TEXT("CI_StaBooth"), FTransform(C.Along.Rotation(), P + FVector(0, 0, 40.f)), FVector(35.f, 28.f, 40.f), UVPlain());
        Solid.Box(TEXT("CI_Glass"), FTransform(C.Along.Rotation(), P + FVector(0, 0, 120.f)), FVector(35.f, 28.f, 40.f), UVPlain());
        Glow.Box(TEXT("CI_StaPopcornGlow"), FTransform(C.Along.Rotation(), P + FVector(0, 0, 100.f)), FVector(32.f, 25.f, 18.f), UVPlain());
        for (int32 I = 0; I < 5; ++I) Box(Clutter, TEXT("CI_StaPopBox"), In(.6f + I * .06f, -60.f, Sill), In(.6f + I * .06f, -48.f, Sill + 18.f) + C.Along * 10.f);
        for (int32 I = 0; I < 3; ++I) for (int32 J = 0; J < 4; ++J)
            Box(Clutter, (I + J) % 2 ? TEXT("CI_CrateRed") : TEXT("CI_StreamerGold"), In(.15f + J * .05f, Back + 10.f, StaFloor + 120.f + I * 30.f), In(.15f + J * .05f, Back + 25.f, StaFloor + 135.f + I * 30.f) + C.Along * 15.f);
    }
    else
    {
        C.Slab(Solid, TEXT("CI_Steel"), U0 + 40.f, U0 + 140.f, Back + 5.f, Back + 60.f, StaFloor, StaFloor + 170.f);   // the drink dispenser
        C.Slab(Solid, TEXT("CI_Enamel"), U1 - 150.f, U1 - 30.f, Back + 5.f, Back + 70.f, StaFloor, StaFloor + 85.f);  // the ice chest
        HouseCampusFallKit::Card(Clutter, TEXT("CI_StaClosed"), C.At((U0 + U1) * .5f, StaWallT + 3.5f, Sill + 60.f), -C.Along, FVector::UpVector, 70.f, 40.f, C.N);
    }
    Kit.Floor(EKit::TrashCan, FVector2D(C.At(U1 + 40.f, StaWallT + 30.f, 0.f)), StaFloor, 0.f, true);
}

/** Litter on the concourse floor after the game: cups, programmes, popcorn boxes, a dropped pennant. */
void StaLitter(FHallMesh& Clutter, const FStaChord& C, int32 Seed)
{
    FRandomStream R(Seed);
    for (int32 I = 0; I < 9; ++I)
    {
        const FVector P = C.At(R.FRandRange(40.f, C.Len - 40.f), R.FRandRange(-140.f, 140.f), StaFloor + .1f);   // (C is the concourse's middle line)
        switch (I % 3)
        {
        case 0: Clutter.Cylinder(TEXT("CI_Paper"), P + FVector(0, 0, 3.f), P + FVector(R.FRandRange(-9.f, 9.f), R.FRandRange(-9.f, 9.f), 3.f), 3.5f, 8, UVPlain()); break;
        case 1: Clutter.Box(TEXT("CI_SciProgramme"), FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), P + FVector(0, 0, .3f)), FVector(10.f, 7.f, .25f), UVPlain()); break;
        default: Clutter.Box(TEXT("CI_StaPopBox"), FTransform(FRotator(80.f, R.FRandRange(0.f, 360.f), 0.f), P + FVector(0, 0, 4.f)), FVector(4.f, 6.f, 9.f), UVPlain()); break;
        }
    }
}

void StaConcourse(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit)
{
    using EKit = HouseCampusInteriors::EKit;
    for (int32 K = StaK0; K <= StaK1; ++K)
    {
        const FStaChord In(K, StaRIn), Out(K, StaROut);
        // Floor, ceiling.
        StaBand(Solid, TEXT("CI_StaConcreteFloor"), K, StaRIn, StaROut, 0.f, 1.f, StaFloor, true, FHallUV::World(200.f));
        StaBand(Solid, TEXT("CI_StaConcrete"), K, StaRIn, StaROut, 0.f, 1.f, StaCeil, false, FHallUV::World(200.f));
        StaBand(Solid, TEXT("CI_StaConcrete"), K, StaRIn, StaROut, 0.f, 1.f, StaCeil + 15.f, true, FHallUV::World(200.f));   // its top, under the seats
        // The outer skin: the window band (glazed) and, on the gate segments, the gate.
        const float WinU0 = Out.Len * HouseCampusInteriors::StadiumWinT0, WinU1 = Out.Len * HouseCampusInteriors::StadiumWinT1;
        const float WinZ0 = StaGround + HouseCampusInteriors::StadiumWinZ0, WinZ1 = StaGround + HouseCampusInteriors::StadiumWinZ1;
        TArray<FBox2D> OutHoles = { FBox2D(FVector2D(WinU0, WinZ0), FVector2D(WinU1, WinZ1)) };
        const bool bGate = HouseCampusInteriors::StadiumGate(K);
        const float Mid = Out.Len * .5f;
        if (bGate) OutHoles.Add(FBox2D(FVector2D(Mid - 112.f, StaGround - 1.f), FVector2D(Mid + 112.f, StaGround + 295.f)));
        Out.Wall(Solid, TEXT("CI_StaBlock"), 0.f, Out.Len, -StaWallT, 0.f, StaGround, StaCeil, OutHoles, FHallUV::World(80.f));
        {
            // The window: a steel frame, mullions, glass.
            const float U0 = WinU0, U1 = WinU1, Z0 = WinZ0, Z1 = WinZ1;
            Out.Slab(Solid, TEXT("CI_Glass"), U0, U1, -13.f, -12.f, Z0, Z1);
            for (float U = U0; U <= U1 + 1.f; U += (U1 - U0) / 4.f) Out.Slab(Solid, TEXT("CI_StaRail"), U - 3.f, U + 3.f, -16.f, -9.f, Z0, Z1);
            Out.Slab(Solid, TEXT("CI_StaRail"), U0, U1, -26.f, -9.f, Z0 - 3.f, Z0);
        }
        // The reveal between this skin and the exterior wall, round the gate; the turnstiles in GATE 4; the gate's name.
        const FStaChord Ext(K, 1.f);
        const float Gap = FVector::DotProduct(Ext.A - Out.A, Out.N);
        if (bGate)
        {
            for (const float S : { -1.f, 1.f }) Out.Slab(Solid, TEXT("CI_StaConcrete"), Mid + S * 112.f - (S > 0.f ? 0.f : 6.f), Mid + S * 112.f + (S > 0.f ? 6.f : 0.f), -StaWallT, Gap + 2.f, StaGround, StaGround + 295.f);
            Out.Slab(Solid, TEXT("CI_StaConcrete"), Mid - 118.f, Mid + 118.f, -StaWallT, Gap + 2.f, StaGround + 295.f, StaGround + 301.f);
            Out.Slab(Solid, TEXT("CI_StaConcreteFloor"), Mid - 112.f, Mid + 112.f, -StaWallT, Gap + 2.f, StaGround, StaFloor);
            if (K == 8)
                for (const float S : { -1.f, 1.f })
                {
                    // A turnstile each side, the middle left clear: a cabinet, its hub and three arms.
                    const FVector Base = Out.At(Mid + S * 70.f, -StaWallT - 40.f, StaFloor);
                    Solid.Box(TEXT("CI_Steel"), FTransform(Out.Along.Rotation(), Base + FVector(0, 0, 50.f) + Out.Along * (S * 20.f)), FVector(12.f, 30.f, 50.f), UVPlain());
                    for (int32 Arm = 0; Arm < 3; ++Arm)
                    {
                        const FVector D = FRotator(0.f, Out.Along.Rotation().Yaw + 90.f + Arm * 120.f, 0.f).Vector();
                        Clutter.Cylinder(TEXT("CI_StaRail"), Base + FVector(0, 0, 90.f), Base + FVector(0, 0, 90.f - (Arm == 0 ? 0.f : 18.f)) + D * 32.f, 1.6f, 8, UVPlain());
                    }
                }
            WallPlaque(Clutter, K == 8 ? TEXT("CI_StaGate4") : TEXT("CI_StaGate5"), Out.At(Mid, -StaWallT, StaGround + 350.f), -Out.N, 180.f, 46.f);
        }
        // The field-side wall: whatever stands in it on this segment.
        TArray<FBox2D> InHoles;
        switch (K)
        {
        case 7: InHoles = { FBox2D(FVector2D(60.f, StaGround - 1.f), FVector2D(152.f, StaFloor + 213.f)), FBox2D(FVector2D(350.f, StaFloor + 100.f), FVector2D(650.f, StaFloor + 260.f)) }; break;
        case 8: InHoles = { FBox2D(FVector2D(230.f, StaFloor + 100.f), FVector2D(530.f, StaFloor + 260.f)) }; break;
        case 9: InHoles = { FBox2D(FVector2D(150.f, StaGround - 1.f), FVector2D(242.f, StaFloor + 213.f)), FBox2D(FVector2D(534.f, StaGround - 1.f), FVector2D(626.f, StaFloor + 213.f)) }; break;
        case 10: InHoles = { FBox2D(FVector2D(240.f, StaFloor + 100.f), FVector2D(540.f, StaFloor + 260.f)) }; break;
        case 11: InHoles = { FBox2D(FVector2D(StaMouth0, StaVomZ), FVector2D(StaMouth1, StaGround + 240.f + 63.f * 7.f)) }; break;
        default: break;
        }
        In.Wall(Solid, TEXT("CI_StaBlock"), 0.f, In.Len, 0.f, StaWallT, StaGround, StaCeil, InHoles, FHallUV::World(80.f));
        if (K == 7) StaConcession(Solid, Clutter, Glow, Kit, 7, 350.f, 650.f, 0);
        if (K == 8) StaConcession(Solid, Clutter, Glow, Kit, 8, 230.f, 530.f, 1);
        if (K == 10) StaConcession(Solid, Clutter, Glow, Kit, 10, 240.f, 540.f, 2);
        if (K == 9)
        {
            WallPlaque(Clutter, TEXT("CI_StaMen"), In.At(196.f, StaWallT, StaFloor + 245.f), In.N, 70.f, 26.f);
            WallPlaque(Clutter, TEXT("CI_StaWomen"), In.At(580.f, StaWallT, StaFloor + 245.f), In.N, 80.f, 26.f);
            WallClock(Clutter, In.At(390.f, StaWallT, StaFloor + 262.f), In.N, 24.f);
            Kit.Floor(EKit::TrashCan, FVector2D(In.At(390.f, StaWallT + 30.f, 0.f)), StaFloor, 0.f, true);
        }
        if (K == 7) WallPlaque(Clutter, TEXT("CI_StaLockerSign"), In.At(106.f, StaWallT, StaFloor + 245.f), In.N, 110.f, 30.f);
        // Lights down the middle; streamers; litter.
        for (float T = .2f; T < .9f; T += .3f)
        {
            const FVector P = FMath::Lerp(In.At(T * In.Len, 0.f, 0.f), Out.At(T * Out.Len, 0.f, 0.f), .5f);
            Glow.Box(K == 10 && T > .7f ? TEXT("CI_TrofferFlicker") : TEXT("CI_Troffer"), FTransform(In.Along.Rotation(), FVector(P.X, P.Y, StaCeil - 2.f)), FVector(60.f, 8.f, .3f), UVPlain());
            Clutter.Box(TEXT("CI_TrofferFrame"), FTransform(In.Along.Rotation(), FVector(P.X, P.Y, StaCeil - 1.f)), FVector(64.f, 12.f, 1.f), UVPlain());
        }
        Streamer(Clutter, In.At(30.f, StaWallT + 5.f, StaCeil - 12.f), Out.At(Out.Len - 30.f, -StaWallT - 5.f, StaCeil - 12.f), K % 2 == 0);
        StaLitter(Clutter, FStaChord(K, (StaRIn + StaROut) * .5f), 860 + K);
    }
    // The ends of the concourse.
    for (const float A : { StaAngle(StaK0), StaAngle(StaK1 + 1) })
        StaRadialWall(Solid, TEXT("CI_StaBlock"), A, StaRIn, StaROut, 25.f, StaGround, StaCeil + 15.f, A < 1.5f ? 12.5f : -12.5f);
    // The stair up to the vomitory: along segment 11's wall from the west, its landing before the mouth.
    const FStaChord V(StaVomK, StaRIn);
    const float DetailLandingEnd=HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6?StaMouth1-70.f:StaMouth1;
    StaFlight(Solid, Clutter, V, StaStairBottom, DetailLandingEnd, StaStairD0, StaStairD1, StaFloor, StaVomZ);
    V.Slab(Solid, TEXT("CI_StaConcrete"), StaMouth0 - 20.f, DetailLandingEnd, StaStairD0, StaStairD1, StaFloor - 5.f, StaVomZ);
    V.Slab(Solid, TEXT("CI_StaConcrete"), StaMouth0 - 20.f, DetailLandingEnd, StaStairD1, StaStairD1 + 12.f, StaFloor - 5.f, StaVomZ + 95.f);
    V.Slab(Solid, TEXT("CI_StaConcrete"), StaMouth0 - 32.f, StaMouth0 - 20.f, StaStairD0, StaStairD1 + 12.f, StaFloor - 5.f, StaVomZ + 95.f);
    WallPlaque(Clutter, TEXT("CI_StaSections"), V.At(StaMouth0 - 80.f, StaWallT, StaVomZ + 150.f), V.N, 150.f, 34.f);   // beside the mouth, over the landing's end guard
}

/** The vomitory's landing in segment 11: the floor where rows 4..7 were, the cheek walls where the rows stop, the parapet
 *  over row 3. */
void StaVomitory(FHallMesh& Solid, FHallMesh& Clutter)
{
    StaBand(Solid, TEXT("CI_StaConcreteFloor"), StaVomK, StaVomR0, StaVomR1, 0.f, 1.f, StaVomZ, true, FHallUV::World(200.f));
    // Under it, down to the ground, so nothing shows through from the stands' sides.
    for (const float A : { StaAngle(StaVomK), StaAngle(StaVomK + 1) })
        for (int32 Row = StaVomRow0; Row <= StaVomRow1; ++Row)
        {
            const float R0 = .69f + .024f * Row, R1 = R0 + .024f, Z = StaGround + 240.f + 63.f * Row;
            StaRadialWall(Solid, TEXT("CI_StaConcrete"), A, R0, R1 + .002f, 15.f, StaGround, Z, A < StaAngle(StaVomK) + .01f ? 7.5f : -7.5f);
        }
    const FStaChord P(StaVomK, StaVomR0);
    P.Slab(Solid, TEXT("CI_StaConcrete"), 0.f, P.Len, 0.f, 15.f, StaVomZ, StaVomZ + 85.f);
    Clutter.Cylinder(TEXT("CI_StaRail"), P.At(10.f, 7.5f, StaVomZ + 100.f), P.At(P.Len - 10.f, 7.5f, StaVomZ + 100.f), 3.f, 10, UVPlain());
    for (float U = 10.f; U < P.Len; U += 150.f) Clutter.Cylinder(TEXT("CI_StaRail"), P.At(U, 7.5f, StaVomZ + 85.f), P.At(U, 7.5f, StaVomZ + 100.f), 2.f, 8, UVPlain());
    // A programme and a cup left on the landing.
    const FStaChord M(StaVomK, (StaVomR0 + StaVomR1) * .5f);
    Clutter.Box(TEXT("CI_SciProgramme"), FTransform(FRotator(0.f, 30.f, 0.f), M.At(M.Len * .3f, 0.f, StaVomZ + .3f)), FVector(10.f, 7.f, .25f), UVPlain());
    Clutter.Cylinder(TEXT("CI_Paper"), M.At(M.Len * .62f, 10.f, StaVomZ + 3.5f), M.At(M.Len * .62f + 9.f, 14.f, StaVomZ + 3.5f), 3.5f, 8, UVPlain());
}

// ------------------------------------------------------------------ the Geese locker room
void StaLockerRoom(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit)
{
    using EKit = HouseCampusInteriors::EKit;
    // Its pieces along the bowl's segments: (segment, fraction from, fraction to).
    struct FPart { int32 K; float T0, T1; };
    TArray<FPart> Parts;
    for (int32 K = 0; K < StaSeg; ++K)
    {
        const float A = K * 9.f, B = A + 9.f;
        if (B <= StaLockA0 || A >= StaLockA1) continue;
        Parts.Add({ K, FMath::Max(0.f, (StaLockA0 - A) / 9.f), FMath::Min(1.f, (StaLockA1 - A) / 9.f) });
    }
    int32 Bench = 0;
    for (const FPart& Pt : Parts)
    {
        const FStaChord I(Pt.K, StaLockR0), O(Pt.K, StaLockR1);
        StaBand(Solid, TEXT("CI_StaRubberFloor"), Pt.K, StaLockR0, StaLockR1, Pt.T0, Pt.T1, StaFloor, true, FHallUV::World(120.f));
        StaBand(Solid, TEXT("CI_CeilingTile"), Pt.K, StaLockR0, StaLockR1, Pt.T0, Pt.T1, StaLockCeil, false, FHallUV::World(122.f));
        // The field-side wall (inside the podium wall) with the tunnel in segment 6; the outer wall (the concourse's, from
        // 63 degrees on, is the concourse's own).
        TArray<FBox2D> Holes;
        if (Pt.K == StaTunnelK) Holes.Add(FBox2D(FVector2D(I.Len * .5f - 150.f, StaGround - 1.f), FVector2D(I.Len * .5f + 150.f, StaGround + 220.f)));
        I.Wall(Solid, TEXT("CI_StaBlockPurple"), I.Len * Pt.T0, I.Len * Pt.T1, -22.f, 0.f, StaGround, StaLockCeil + 2.f, Holes, FHallUV::World(80.f));
        if (Pt.K < StaK0) O.Wall(Solid, TEXT("CI_StaBlockPurple"), O.Len * Pt.T0, O.Len * Pt.T1, 0.f, StaWallT, StaGround, StaLockCeil + 2.f, {}, FHallUV::World(80.f));
        if (Pt.K == StaTunnelK)
        {
            // The tunnel: its sides through to the podium wall, a sign over it.
            const FStaChord Pod(Pt.K, .69f);
            const float Gap = FVector::DotProduct(I.A - Pod.A, I.N);
            for (const float S : { -1.f, 1.f }) I.Slab(Solid, TEXT("CI_StaConcrete"), I.Len * .5f + S * 150.f - (S > 0.f ? 0.f : 8.f), I.Len * .5f + S * 150.f + (S > 0.f ? 8.f : 0.f), -Gap - 2.f, 0.f, StaGround, StaGround + 220.f);
            I.Slab(Solid, TEXT("CI_StaConcrete"), I.Len * .5f - 158.f, I.Len * .5f + 158.f, -Gap - 2.f, 0.f, StaGround + 220.f, StaGround + 228.f);
            I.Slab(Solid, TEXT("CI_StaRubberFloor"), I.Len * .5f - 150.f, I.Len * .5f + 150.f, -Gap - 2.f, 0.f, StaGround, StaFloor);
            WallPlaque(Clutter, TEXT("CI_StaTunnelSign"), I.At(I.Len * .5f, 0.f, StaGround + 205.f), I.N, 220.f, 30.f);
        }
        // Lockers along the outer wall (the field-side wall keeps the tunnel and the logo), a bench in front of them, a
        // helmet on it now and then, jerseys in the lockers.
        const float U0 = O.Len * Pt.T0 + 20.f, U1 = O.Len * Pt.T1 - 20.f;
        const int32 Count = FMath::FloorToInt((U1 - U0) / 38.f);
        if (Count > 0 && Pt.K < StaK0)
        {
            if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6)HouseStadiumDetail::WoodLockers(Solid,Clutter,O.At(U0,0.f,StaFloor),O.Along,Count,-O.N);else Lockers(Solid, Clutter, O.At(U0, 0.f, StaFloor), O.Along, Count, -O.N);
            for (int32 L = 0; L < Count; L += 2)
                O.Slab(Clutter, TEXT("CI_StreamerPurple"), U0 + 38.f * L + 8.f, U0 + 38.f * L + 30.f, -(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6?15.f:54.f), -(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6?12.f:51.f), StaFloor + 70.f, StaFloor + 150.f);   // detail: inside the open wood bay; legacy: on the door hook
            O.Slab(Solid, TEXT("CI_Oak"), U0, U0 + 38.f * Count, -105.f, -70.f, StaFloor + 42.f, StaFloor + 46.f);
            for (float U = U0 + 20.f; U < U0 + 38.f * Count; U += 120.f) O.Slab(Solid, TEXT("CI_Steel"), U - 2.f, U + 2.f, -100.f, -75.f, StaFloor, StaFloor + 42.f);
            for (float U = U0 + 40.f; U < U0 + 38.f * Count - 20.f; U += 90.f)
            {
                if ((Bench++ % 3) == 2) continue;
                Helmet(Solid, O.At(U, -88.f, StaFloor + 46.f), FVector(FMath::Cos(Bench * 1.7f), FMath::Sin(Bench * 1.7f), 0.f));
            }
        }
        // Lights.
        for (float T = Pt.T0 + .15f; T < Pt.T1; T += .4f)
        {
            const FVector P = FMath::Lerp(FMath::Lerp(I.A, I.B, T), FMath::Lerp(O.A, O.B, T), .5f);
            Troffer(Glow, Clutter, FVector(P.X, P.Y, StaLockCeil - .6f), false);
        }
    }
    // The end walls.
    StaRadialWall(Solid, TEXT("CI_StaBlockPurple"), FMath::DegreesToRadians(StaLockA0), StaLockR0, StaLockR1 + .007f, 20.f, StaGround, StaLockCeil + 2.f, 10.f);
    StaRadialWall(Solid, TEXT("CI_StaBlockPurple"), FMath::DegreesToRadians(StaLockA1), StaLockR0, StaLockR1, 20.f, StaGround, StaLockCeil + 2.f, -10.f);
    // The board on the east end wall: BEAT DUNMORE crossed out, 24-17 under it.
    {
        const float A = FMath::DegreesToRadians(StaLockA0);
        const FVector P0 = StaAt(A, .78f, 0.f), P1 = StaAt(A, .80f, 0.f), Along = (P1 - P0).GetSafeNormal(), Into(-Along.Y, Along.X, 0.f);
        Chalkboard(Solid, Clutter, FVector(StaAt(A, .79f, StaGround + 135.f)) + Into * 20.f, Into, 300.f, 110.f, TEXT("CI_StaChalk"));   // on the end wall's face
        WallClock(Clutter, FVector(StaAt(A, .79f, StaGround + 211.f)) + Into * 20.f, Into, 13.f);   // under the ceiling, over the board
    }
    // The Geese on the field-side wall by the tunnel; the game ball on its stand near the door; tape on the floor; towels;
    // the water cooler; a coat on the coach's chair.
    {
        const FStaChord I(StaTunnelK, StaLockR0);
        HouseCampusFallKit::Card(Clutter, TEXT("CI_StaGeese"), I.At(I.Len * .15f, .5f, StaGround + 120.f), -I.Along, FVector::UpVector, 150.f, 110.f, I.N);
    }
    {
        int32 K; float T;
        StaSegOf(61.5f, K, T);
        const FStaChord C(K, .76f);
        const FVector Stand = C.At(C.Len * T, 0.f, StaFloor);
        Solid.Cylinder(TEXT("CI_Walnut"), Stand, Stand + FVector(0, 0, 100.f), 14.f, 12, UVPlain());
        Solid.Cylinder(TEXT("CI_Glass"), Stand + FVector(0, 0, 100.f), Stand + FVector(0, 0, 130.f), 15.f, 14, UVPlain());
        HarFootball(Solid, Stand + FVector(0, 0, 101.f), C.Along);
        HouseCampusFallKit::Card(Clutter, TEXT("CI_StaGameBall"), Stand + FVector(0, 0, 80.f) + C.N * 14.5f, -C.Along, FVector::UpVector, 26.f, 26.f, C.N);
    }
    FRandomStream R(1018);
    for (int32 I = 0; I < 26; ++I)
    {
        const float A = FMath::DegreesToRadians(R.FRandRange(StaLockA0 + 1.f, StaLockA1 - 1.f)), Rr = R.FRandRange(StaLockR0 + .02f, StaLockR1 - .04f);
        const FVector P = StaAt(A, Rr, StaFloor + 1.f);
        if (I % 3) HouseCampusFallKit::Ball(Clutter, TEXT("CI_Paper"), P + FVector(0, 0, 1.5f), R.FRandRange(2.f, 3.5f), 6);
        else Clutter.Box(TEXT("CI_Paper"), FTransform(FRotator(0.f, R.FRandRange(0.f, 180.f), 0.f), P), FVector(R.FRandRange(12.f, 30.f), 2.5f, .15f), UVPlain());
    }
    {
        // A taped X on the floor in front of the board (where the coach stood).
        const FVector P = StaAt(FMath::DegreesToRadians(StaLockA0 + 2.5f), .79f, StaFloor + .2f);
        for (const float Yaw : { 45.f, -45.f }) Clutter.Box(TEXT("CI_Paper"), FTransform(FRotator(0.f, Yaw, 0.f), P), FVector(30.f, 2.5f, .15f), UVPlain());
    }
    for (int32 I = 0; I < 4; ++I)
        Kit.Floor(EKit::BunchedTowel, FVector2D(StaAt(FMath::DegreesToRadians(54.f + I * 2.5f), .82f, 0.f)), StaFloor, R.FRandRange(0.f, 360.f), false);
    const FVector Cooler = StaAt(FMath::DegreesToRadians(65.f), .74f, StaFloor);
    Box(Solid, TEXT("CI_Steel"), Cooler - FVector(25.f, 25.f, 0.f), Cooler + FVector(25.f, 25.f, 70.f));
    Solid.Cylinder(TEXT("CI_StaCooler"), Cooler + FVector(0, 0, 70.f), Cooler + FVector(0, 0, 125.f), 22.f, 16, UVPlain());
    Solid.Cylinder(TEXT("CI_Enamel"), Cooler + FVector(0, 0, 125.f), Cooler + FVector(0, 0, 130.f), 23.f, 16, UVPlain());
    Clutter.Cylinder(TEXT("CI_Paper"), Cooler + FVector(30.f, 0, 60.f), Cooler + FVector(30.f, 0, 85.f), 4.f, 8, UVPlain());
    const FVector Desk = StaAt(FMath::DegreesToRadians(52.5f), .73f, StaFloor);
    Table(Solid, Desk, 120.f, 60.f, 76.f, true);
    Kit.Floor(EKit::GrayOfficeChair, FVector2D(Desk) + FVector2D(0.f, 55.f), StaFloor, -90.f, true);
    Kit.Floor(EKit::ChairJacket, FVector2D(Desk) + FVector2D(0.f, 77.f), StaFloor + 32.f, -90.f, false);
    Kit.Floor(EKit::PaperStack, FVector2D(Desk) + FVector2D(-20.f, 0.f), StaFloor + 76.f, 10.f, true);
}

// ------------------------------------------------------------------ the press box (inside the exterior's shell)
void StaPressBox(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit)
{
    using EKit = HouseCampusInteriors::EKit;
    const float Z = StaPBFloor, In0 = StaPBY0 + 20.f, In1 = StaPBY1 - 20.f, X0 = StaPBX0 + 20.f, X1 = StaPBX1 - 20.f;
    Box(Solid, TEXT("CI_Carpet"), FVector(X0, In0, Z - .5f), FVector(X1, In1, Z), FHallUV::World(180.f));
    Box(Solid, TEXT("CI_CeilingTile"), FVector(X0, In0, StaPBCeil), FVector(X1, In1, StaPBCeil + 1.f), FHallUV::World(122.f));
    // Paneling on the walls (under the window band on the field side, full height elsewhere).
    Box(Solid, TEXT("CI_Walnut"), FVector(X0, In0, Z), FVector(X1, In0 + 1.5f, Z + 115.f));
    Box(Solid, TEXT("CI_Walnut"), FVector(X0, In1 - 1.5f, Z), FVector(X1, In1, StaPBCeil));
    Box(Solid, TEXT("CI_Walnut"), FVector(X0, In0, Z), FVector(X0 + 1.5f, In1, StaPBCeil));
    TArray<FBox2D> DoorHole = { FBox2D(FVector2D(-9618.f, Z - 1.f), FVector2D(-9510.f, Z + 222.f)) };
    Subtract(In0, In1, Z, StaPBCeil, DoorHole, [&](float A, float B, float ZA, float ZB) { Box(Solid, TEXT("CI_Walnut"), FVector(X1 - 1.5f, A, ZA), FVector(X1, B, ZB)); });
    // The press row: a counter under the windows, chairs, typewriters, phones, binoculars, the papers' cards.
    Box(Solid, TEXT("CI_Laminate"), FVector(1060.f, In0, Z + 88.f), FVector(3700.f, In0 + 75.f, Z + 92.f));
    Box(Solid, TEXT("CI_OakDark"), FVector(1060.f, In0 + 60.f, Z), FVector(3700.f, In0 + 64.f, Z + 88.f));
    static const TCHAR* const Papers[] = { TEXT("CI_StaPaperCourier"), TEXT("CI_StaPaperDispatch"), TEXT("CI_StaPaperHonker"), TEXT("CI_StaPaperLedger") };
    int32 Seat = 0;
    for (float X = 1150.f; X < 3650.f; X += 140.f, ++Seat)
    {
        Kit.Floor(EKit::GrayOfficeChair, FVector2D(X, In0 + 120.f), Z, -90.f, true);
        if (Seat % 3 == 0) Typewriter(Solid, Clutter, FVector(X, In0 + 40.f, Z + 92.f), FVector(0, 1, 0));
        else Kit.Floor(EKit::PaperStack, FVector2D(X, In0 + 40.f), Z + 92.f, Seat * 13.f, true);
        if (Seat % 2) DeskPhone(Solid, FVector(X + 45.f, In0 + 30.f, Z + 92.f), FVector(0, 1, 0), Seat % 4 == 1);
        if (Seat % 4 == 2) { Clutter.Cylinder(TEXT("CI_CrtBezel"), FVector(X - 40.f, In0 + 25.f, Z + 96.f), FVector(X - 40.f, In0 + 10.f, Z + 96.f), 3.f, 8, UVPlain()); Clutter.Cylinder(TEXT("CI_CrtBezel"), FVector(X - 30.f, In0 + 25.f, Z + 96.f), FVector(X - 30.f, In0 + 10.f, Z + 96.f), 3.f, 8, UVPlain()); }
        if (Seat % 3 == 1) Kit.Floor(EKit::PaperCup, FVector2D(X + 20.f, In0 + 50.f), Z + 92.f, 0.f, true);
        HouseCampusFallKit::Card(Clutter, Papers[Seat % 4], FVector(X, In0 + 74.5f, Z + 80.f), FVector(1, 0, 0), FVector::UpVector, 40.f, 12.f, FVector(0, 1, 0));
    }
    // The radio booth at the west end, behind glass: the desk, microphones, headsets, the board; ON AIR over its door.
    const float BoothX = 1000.f;
    TArray<FBox2D> BoothHoles = { FBox2D(FVector2D(In1 - 300.f, Z - 1.f), FVector2D(In1 - 208.f, Z + 213.f)), FBox2D(FVector2D(In0 + 40.f, Z + 100.f), FVector2D(In1 - 340.f, Z + 230.f)) };
    Subtract(In0, In1, Z, StaPBCeil, BoothHoles, [&](float A, float B, float ZA, float ZB) { Box(Solid, TEXT("CI_PlasterWall"), FVector(BoothX - 7.5f, A, ZA), FVector(BoothX + 7.5f, B, ZB)); });
    Box(Solid, TEXT("CI_Glass"), FVector(BoothX - 1.f, In0 + 40.f, Z + 100.f), FVector(BoothX + 1.f, In1 - 340.f, Z + 230.f));
    Box(Solid, TEXT("CI_Laminate"), FVector(X0, In0, Z + 88.f), FVector(BoothX - 8.f, In0 + 80.f, Z + 92.f));
    Box(Solid, TEXT("CI_OakDark"), FVector(X0, In0 + 66.f, Z), FVector(BoothX - 8.f, In0 + 70.f, Z + 88.f));
    for (const float X : { 300.f, 600.f })
    {
        Clutter.Cylinder(TEXT("CI_Steel"), FVector(X, In0 + 40.f, Z + 92.f), FVector(X, In0 + 40.f, Z + 125.f), .8f, 6, UVPlain());
        Clutter.Cylinder(TEXT("CI_CrtBezel"), FVector(X, In0 + 40.f, Z + 125.f), FVector(X, In0 + 32.f, Z + 133.f), 3.5f, 10, UVPlain());
        Kit.Floor(EKit::GrayOfficeChair, FVector2D(X, In0 + 125.f), Z, -90.f, false);
        Clutter.Box(TEXT("CI_CrtBezel"), FTransform(FVector(X + 40.f, In0 + 50.f, Z + 93.f)), FVector(10.f, 8.f, 1.f), UVPlain());   // headset
    }
    Box(Solid, TEXT("CI_Beige"), FVector(400.f, In0 + 10.f, Z + 92.f), FVector(520.f, In0 + 55.f, Z + 104.f));   // the board
    Glow.Box(TEXT("CI_GreenScreen"), FTransform(FVector(460.f, In0 + 20.f, Z + 104.2f)), FVector(40.f, 6.f, .2f), UVPlain());
    for (int32 I = 0; I < 8; ++I) Clutter.Cylinder(TEXT("CI_Epoxy"), FVector(410.f + I * 14.f, In0 + 40.f, Z + 104.f), FVector(410.f + I * 14.f, In0 + 40.f, Z + 108.f), 2.f, 8, UVPlain());
    Glow.Box(TEXT("CI_StaOnAir"), FTransform(FVector(BoothX + 9.f, In1 - 254.f, Z + 245.f)), FVector(1.5f, 30.f, 9.f), UVPlain());
    HouseCampusFallKit::Card(Glow, TEXT("CI_StaOnAirFace"), FVector(BoothX + 10.6f, In1 - 254.f, Z + 245.f), FVector(0, -1, 0), FVector::UpVector, 58.f, 17.f, FVector(1, 0, 0));
    WallPlaque(Clutter, TEXT("CI_StaRadio"), FVector(BoothX + 7.5f, In1 - 120.f, Z + 190.f), FVector(1, 0, 0), 120.f, 40.f);
    // The PA and scoreboard desk at the east end: the panel of switches and lamps, the announcer's microphone.
    Box(Solid, TEXT("CI_Steel"), FVector(3800.f, In0, Z), FVector(4400.f, In0 + 70.f, Z + 90.f));
    Box(Solid, TEXT("CI_Beige"), FVector(3850.f, In0 + 10.f, Z + 90.f), FVector(4250.f, In0 + 50.f, Z + 110.f));
    for (int32 I = 0; I < 24; ++I) Glow.Cylinder(I % 5 == 0 ? TEXT("CI_PilotLamp") : TEXT("CI_GreenScreen"), FVector(3870.f + (I % 12) * 30.f, In0 + 22.f + (I / 12) * 16.f, Z + 110.f), FVector(3870.f + (I % 12) * 30.f, In0 + 22.f + (I / 12) * 16.f, Z + 110.6f), 3.f, 8, UVPlain());
    Clutter.Cylinder(TEXT("CI_Steel"), FVector(4320.f, In0 + 40.f, Z + 90.f), FVector(4320.f, In0 + 40.f, Z + 125.f), .8f, 6, UVPlain());
    Clutter.Cylinder(TEXT("CI_CrtBezel"), FVector(4320.f, In0 + 40.f, Z + 125.f), FVector(4320.f, In0 + 32.f, Z + 133.f), 3.5f, 10, UVPlain());
    Kit.Floor(EKit::GrayOfficeChair, FVector2D(4000.f, In0 + 120.f), Z, -90.f, true);
    WallPlaque(Clutter, TEXT("CI_StaScoreSign"), FVector(4050.f, In1, Z + 200.f), FVector(0, -1, 0), 160.f, 30.f);
    WallClock(Clutter, FVector(2280.f, In1, Z + 228.f), FVector(0, -1, 0), 18.f);
    // The middle flood tower's pole comes up through the box near its back wall: cased in walnut to the ceiling, a column.
    const FVector Pole = StaAt(HALF_PI, 1.04f, Z);
    Box(Solid, TEXT("CI_Walnut"), FVector(Pole.X - 32.f, Pole.Y - 32.f, Z), FVector(Pole.X + 32.f, In1, StaPBCeil));
    // Spotters' charts along the back wall; the coffee table; lights.
    for (int32 I = 0; I < 4; ++I)
        HouseCampusFallKit::Card(Clutter, I % 2 ? TEXT("CI_StaChartDunmore") : TEXT("CI_StaChartGeese"), FVector(1500.f + I * 520.f, In1 - 1.f, Z + 180.f), FVector(-1, 0, 0), FVector::UpVector, 150.f, 110.f, FVector(0, -1, 0));
    Table(Solid, FVector(3350.f, In1 - 60.f, Z), 160.f, 60.f, 76.f, true);
    Solid.Cylinder(TEXT("CI_Steel"), FVector(3300.f, In1 - 60.f, Z + 76.f), FVector(3300.f, In1 - 60.f, Z + 110.f), 11.f, 12, UVPlain());
    for (int32 I = 0; I < 5; ++I) Kit.Floor(EKit::PaperCup, FVector2D(3360.f + (I % 3) * 15.f, In1 - 70.f + (I / 3) * 15.f), Z + 76.f, 0.f, true);
    for (float X = 400.f; X < 4600.f; X += 600.f) Troffer(Glow, Clutter, FVector(X, (In0 + In1) * .5f, StaPBCeil - .6f), true);
}

// ------------------------------------------------------------------ the building
void BuildStadium(int32 Seed, float Height, FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit)
{
    StaConcourse(Solid, Clutter, Glow, Kit);
    StaVomitory(Solid, Clutter);
    StaLockerRoom(Solid, Clutter, Glow, Kit);
    StaPressBox(Solid, Clutter, Glow, Kit);
    HouseStadiumDetail::Interiors(Solid,Clutter,Glow);
    // The stair tower's lamps (the tower is the exterior's).
    for (int32 L = 0; L <= 8; ++L)
    {
        const float Z = StaFloor + 140.f * L + 250.f;
        const float Y = L % 2 ? StaTwY1 - 90.f : StaTwY0 + 90.f;
        HouseCampusFallKit::Ball(Glow, TEXT("CI_DomeLight"), FVector(StaTwX1 - 26.f, Y, FMath::Min(Z, StaTwTop - 40.f)), 7.f, 8);
    }
}

FBox StaBounds() { return FBox(FVector(-250.f, -11400.f, StaGround - 10.f), FVector(5650.f, StaTwY1 + 10.f, 1900.f)); }   // the concourse, locker room, press box, tower
bool StaInside(const FVector& P)
{
    if (P.X > StaPBX0 && P.X < StaPBX1 && P.Y > StaPBY0 && P.Y < StaPBY1 && P.Z > 1040.f && P.Z < 1900.f) return true;
    if (P.X > StaTwX0 && P.X < StaTwX1 && P.Y > StaTwY0 && P.Y < StaTwY1 && P.Z > StaGround - 10.f && P.Z < StaTwTop) return true;
    float A, R;
    StaPolar(P, A, R);
    const float Deg = FMath::RadiansToDegrees(A);
    if (Deg >= StaK0 * 9.f && Deg <= (StaK1 + 1) * 9.f && R > StaRIn && R < StaROut && P.Z < StaCeil) return true;
    if (Deg >= StaLockA0 && Deg <= StaLockA1 && R > StaLockR0 && R < StaLockR1 && P.Z < StaLockCeil) return true;
    return false;
}
float StaFloorZ(int32 Floor) { return Floor == 1 ? StaPBFloor : StaFloor; }

void StaLightAnchors(int32 Seed, float Height, TArray<FVector>& Out)
{
    for (const float Deg : { 66.f, 75.f, 84.f, 93.f, 102.f, 111.f }) { const FVector2D P = HouseCampusInteriors::StadiumPoint(Deg, .94f); Out.Add(FVector(P, 420.f)); }
    for (const float Deg : { 54.f, 61.f }) { const FVector2D P = HouseCampusInteriors::StadiumPoint(Deg, .79f); Out.Add(FVector(P, 130.f)); }
    for (const float X : { 600.f, 1700.f, 2800.f, 3900.f }) Out.Add(FVector(X, -9620.f, StaPBFloor + 300.f));
    for (const float Z : { 200.f, 650.f, 1100.f }) Out.Add(FVector(StaTwX0 + 188.f, (StaTwY0 + StaTwY1) * .5f, Z));   // over the east flights, not the gap between them
}
void StaOpenings(int32 Seed, float Height, TArray<HouseCampusInteriors::FOpening>& Out) {}
void StaDoors(int32 Seed, float Height, TArray<FDoorSpec>& Out)
{
    // The stair tower's pair at its foot (west face, under the press box, swinging in onto the ground landing, where they
    // fold back clear of the first flight): the stadium's main doors.
    {
        const float X = StaTwX0 + 20.f + 1.f, U0 = -9618.f, U1 = -9488.f, W = (U1 - U0 - 1.2f) * .5f;
        const uint8 Flags = uint8(AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::KickPlate | AHouseBlackHallDoor::Closer);
        const FVector Center(StaTwX0 + 10.f, (U0 + U1) * .5f, StaFloor + 105.f);
        Out.Add({ TEXT("MainA"), FVector(X, U0 + .4f, StaFloor), 90.f, EHallDoorStyle::Equipment, W, -1.f, Flags, Center, TEXT("Campus_STA_Main"), FString() });
        Out.Add({ TEXT("MainB"), FVector(X, U1 - .4f, StaFloor), 270.f, EHallDoorStyle::Equipment, W, 1.f, Flags, Center, TEXT("Campus_STA_Main"), FString() });
    }
    // At its head, into the press box (standing open).
    InnerDoor(Out, TEXT("PressBox"), true, StaPBX1, -9610.f, -9518.f, StaPBFloor, -1.f, EHallDoorStyle::French, uint8(AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::Closer), FString(), true);
    // The radio booth (on the air).
    InnerDoor(Out, TEXT("RadioBooth"), true, 1000.f, StaPBY1 - 20.f - 300.f, StaPBY1 - 20.f - 208.f, StaPBFloor, -1.f, EHallDoorStyle::French, uint8(AHouseBlackHallDoor::Lever), TEXT("ON AIR - THE POSTGAME SHOW, DO NOT ENTER"));
    // The concourse's doors in its field-side wall (on the chords: hinged on the U0 jamb, the room on the field side).
    auto ChordDoor = [&Out](FName Name, int32 K, float U0, float U1, const FString& Reason, bool bOpen)
    {
        const FStaChord C(K, StaRIn);
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(C.Along.Y, C.Along.X));
        FDoorSpec D{ Name, C.At(U0 + .4f, -1.f, StaFloor), Yaw, EHallDoorStyle::Equipment, U1 - U0 - .8f, 1.f, uint8(AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::KickPlate),
            C.At((U0 + U1) * .5f, StaWallT * .5f, StaFloor + 105.f), NAME_None, Reason };
        D.bOpen = bOpen;
        Out.Add(D);
    };
    ChordDoor(TEXT("LockerRoom"), 7, 60.f, 152.f, FString(), true);
    ChordDoor(TEXT("MensRoom"), 9, 150.f, 242.f, TEXT("MEN - CLOSED FOR CLEANING AFTER THE GAME"), false);
    ChordDoor(TEXT("WomensRoom"), 9, 534.f, 626.f, TEXT("WOMEN - CLOSED FOR CLEANING AFTER THE GAME"), false);
}

void StadiumLook(UHouseBlackHallSubsystem* Look)
{
    if (!Look || Look->Materials.Contains(TEXT("CI_StaBlock"))) return;
    PaintNoise(Look, TEXT("CI_StaConcrete"), FLinearColor(.42f, .41f, .38f), .18f, .85f, 31);
    PaintNoise(Look, TEXT("CI_StaConcreteFloor"), FLinearColor(.33f, .32f, .3f), .22f, .7f, 32, 400, FLinearColor(.2f, .19f, .18f));
    PaintNoise(Look, TEXT("CI_StaRubberFloor"), FLinearColor(.06f, .06f, .07f), .3f, .8f, 33, 300, FLinearColor(.2f, .06f, .3f));
    PaintNoise(Look, TEXT("CI_StaQuarry"), FLinearColor(.36f, .12f, .07f), .2f, .6f, 34);
    {
        // Painted block, the school's colours on the locker room's: grey in the concourse, purple below a gold band inside.
        for (const bool bPurple : { false, true })
        {
            FHallImage Img; Img.Init(256, 256, bPurple ? FLinearColor(.12f, .04f, .18f) : FLinearColor(.36f, .35f, .32f));
            FRandomStream R(bPurple ? 71 : 70);
            for (int32 Row = 0; Row < 4; ++Row) for (int32 Col = -1; Col < 2; ++Col)
            {
                const float X0 = Col * 128.f + (Row % 2 ? 64.f : 0.f), Y0 = Row * 64.f;
                const FLinearColor Face = bPurple ? FLinearColor(.2f, .06f, .3f) : FLinearColor(.52f, .51f, .47f);
                Img.Rect(FVector2D(X0 + 2.f, Y0 + 2.f), FVector2D(X0 + 126.f, Y0 + 62.f), Face * R.FRandRange(.95f, 1.04f));
            }
            const FName Key = bPurple ? FName(TEXT("CI_StaBlockPurple")) : FName(TEXT("CI_StaBlock"));
            UTexture2D* Tex = HallMakeTexture(Img, true, false, *Key.ToString(), true);
            if (Tex) Look->Textures.Add(Key, Tex);
            HallMakeTextured(Look, Key, Tex, nullptr, .8f);
        }
    }
    {
        // The locker room board: BEAT DUNMORE in the morning's hand, struck through; 24-17 under it, in a bigger hand.
        FHallImage Img; Img.Init(512, 192, FLinearColor(.06f, .13f, .09f));
        const FLinearColor Chalk(.82f, .84f, .8f);
        FHallTextStyle S; S.Height = 38.f; S.Stroke = 4.f; S.Spacing = .2f; S.Wobble = 1.2f;
        HallDrawText(Img, TEXT("BEAT DUNMORE"), FVector2D((512.f - HallTextWidth(TEXT("BEAT DUNMORE"), S)) * .5f, 64.f), S, Chalk);
        Img.Stroke({ FVector2D(70.f, 46.f), FVector2D(250.f, 50.f), FVector2D(445.f, 42.f) }, 5.f, Chalk);
        FHallTextStyle Big; Big.Height = 70.f; Big.Stroke = 7.f; Big.Spacing = .25f; Big.Wobble = 2.f;
        HallDrawText(Img, TEXT("24-17"), FVector2D((512.f - HallTextWidth(TEXT("24-17"), Big)) * .5f, 160.f), Big, FLinearColor(.95f, .85f, .4f));
        Img.Stroke({ FVector2D(140.f, 172.f), FVector2D(372.f, 176.f) }, 3.f, FLinearColor(.95f, .85f, .4f));
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_StaChalk"), false);
        if (Tex) Look->Textures.Add(TEXT("CI_StaChalk"), Tex);
        HallMakeTextured(Look, TEXT("CI_StaChalk"), Tex, nullptr, .9f);
    }
    {
        // The Geese: a goose's head and neck in purple and gold on the locker room wall.
        FHallImage Img; Img.Init(256, 192, FLinearColor(.2f, .06f, .3f));
        Img.Stroke({ FVector2D(70, 180), FVector2D(95, 120), FVector2D(110, 80) }, 30.f, FLinearColor(.85f, .82f, .76f));
        Img.Ellipse(FVector2D(130, 66), FVector2D(42, 30), -15.f, FLinearColor(.85f, .82f, .76f));
        Img.FillPolygon({ FVector2D(165, 58), FVector2D(225, 70), FVector2D(165, 82) }, FLinearColor(.85f, .62f, .12f));
        Img.Ellipse(FVector2D(140, 56), FVector2D(6, 6), 0.f, FLinearColor(.05f, .05f, .05f));
        FHallTextStyle S; S.Height = 30.f; S.Stroke = 5.f; S.Spacing = .2f;
        HallDrawText(Img, TEXT("GEESE"), FVector2D(150.f, 176.f), S, FLinearColor(.85f, .62f, .12f));
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_StaGeese"), false);
        if (Tex) Look->Textures.Add(TEXT("CI_StaGeese"), Tex);
        HallMakeTextured(Look, TEXT("CI_StaGeese"), Tex, nullptr, .5f);
    }
    const FLinearColor Purple(.22f, .05f, .32f), Gold(.85f, .65f, .15f), Cream(.88f, .85f, .78f), Dark(.06f, .05f, .05f);
    PaintText(Look, TEXT("CI_StaMenuGrill"), 640, 150, Dark, FLinearColor(.95f, .88f, .6f), { { TEXT("GEESE GRILL"), .28f }, { TEXT("HOT DOGS .75  BURGERS 1.25  COFFEE .35"), .16f } }, true, FLinearColor(.5f, .45f, .3f));
    PaintText(Look, TEXT("CI_StaMenuPopcorn"), 640, 150, Purple, Gold, { { TEXT("POPCORN AND CANDY"), .26f }, { TEXT("POPCORN .50  CANDY .40  PEANUTS .50"), .16f } }, true, FLinearColor(.3f, .2f, .3f));
    PaintText(Look, TEXT("CI_StaMenuDrinks"), 640, 150, FLinearColor(.05f, .15f, .3f), Cream, { { TEXT("COLD DRINKS"), .28f }, { TEXT("COLA .60  ORANGE .60  HOT COCOA .50"), .16f } }, true, FLinearColor(.3f, .35f, .45f));
    PaintText(Look, TEXT("CI_StaClosed"), 256, 150, Cream, FLinearColor(.6f, .08f, .06f), { { TEXT("SORRY"), .22f }, { TEXT("CLOSED"), .26f } }, true);
    PaintText(Look, TEXT("CI_StaGate4"), 512, 130, Purple, Gold, { { TEXT("GATE 4"), .5f } }, true);
    PaintText(Look, TEXT("CI_StaGate5"), 512, 130, Purple, Gold, { { TEXT("GATE 5"), .5f } }, true);
    PaintText(Look, TEXT("CI_StaSections"), 512, 116, Purple, Gold, { { TEXT("SECTIONS 21 - 24"), .42f } }, true);
    PaintText(Look, TEXT("CI_StaMen"), 256, 96, Dark, Cream, { { TEXT("MEN"), .5f } }, true);
    PaintText(Look, TEXT("CI_StaWomen"), 256, 84, Dark, Cream, { { TEXT("WOMEN"), .5f } }, true);
    PaintText(Look, TEXT("CI_StaLockerSign"), 384, 104, Purple, Gold, { { TEXT("GEESE"), .3f }, { TEXT("PLAYERS ONLY"), .22f } }, true);
    PaintText(Look, TEXT("CI_StaTunnelSign"), 768, 104, Purple, Gold, { { TEXT("PLAY LIKE A GOOSE TODAY"), .42f } }, true);
    PaintText(Look, TEXT("CI_StaGameBall"), 128, 128, Cream, Purple, { { TEXT("GAME BALL"), .14f }, { TEXT("24 - 17"), .2f }, { TEXT("OCT 18 1986"), .1f } }, true);
    PaintText(Look, TEXT("CI_StaRadio"), 384, 128, Dark, Gold, { { TEXT("GEESE RADIO"), .26f }, { TEXT("NETWORK"), .22f } }, true);
    PaintText(Look, TEXT("CI_StaOnAirFace"), 256, 76, FLinearColor(.5f, .02f, .02f), FLinearColor(1.f, .9f, .85f), { { TEXT("ON AIR"), .6f } }, false, FLinearColor(6.f, .6f, .4f));
    PaintText(Look, TEXT("CI_StaScoreSign"), 384, 72, Dark, Cream, { { TEXT("PA - SCOREBOARD"), .45f } }, true);
    PaintText(Look, TEXT("CI_StaPaperCourier"), 256, 76, Cream, Dark, { { TEXT("CALDER COURIER"), .4f } }, false);
    PaintText(Look, TEXT("CI_StaPaperDispatch"), 256, 76, Cream, Dark, { { TEXT("DUNMORE DISPATCH"), .38f } }, false);
    PaintText(Look, TEXT("CI_StaPaperHonker"), 256, 76, Cream, Dark, { { TEXT("THE HONKER"), .45f } }, false);
    PaintText(Look, TEXT("CI_StaPaperLedger"), 256, 76, Cream, Dark, { { TEXT("COUNTY LEDGER"), .4f } }, false);
    SciPaintLines(Look, TEXT("CI_StaChartGeese"), 256, 188, Cream, Purple, { TEXT("GEESE"), TEXT("7 HOLLOWAY QB"), TEXT("22 RENNICK TB"), TEXT("31 OKAFOR FB"), TEXT("84 STRAND SE"), TEXT("88 PALUMBO TE"), TEXT("2 VOSS K") }, 26.f);
    SciPaintLines(Look, TEXT("CI_StaChartDunmore"), 256, 188, Cream, FLinearColor(.05f, .2f, .1f), { TEXT("DUNMORE"), TEXT("12 BRANDT QB"), TEXT("26 COLLIER TB"), TEXT("40 MAREK FB"), TEXT("81 TOOMEY WR"), TEXT("55 DEVLIN LB"), TEXT("9 ACKER P") }, 26.f);
    HallMakePlain(Look, TEXT("CI_StaNosing"), FLinearColor(.75f, .6f, .1f), .6f);
    HallMakePlain(Look, TEXT("CI_StaRail"), FLinearColor(.2f, .2f, .22f), .45f, .6f);
    HallMakePlain(Look, TEXT("CI_StaBooth"), FLinearColor(.45f, .3f, .1f), .5f);
    HallMakePlain(Look, TEXT("CI_StaShutter"), FLinearColor(.45f, .45f, .43f), .4f, .7f);
    HallMakePlain(Look, TEXT("CI_StaHotDog"), FLinearColor(.45f, .12f, .06f), .5f);
    HallMakePlain(Look, TEXT("CI_StaMustard"), FLinearColor(.8f, .6f, .05f), .35f);
    HallMakePlain(Look, TEXT("CI_StaPopBox"), FLinearColor(.75f, .1f, .08f), .6f);
    HallMakePlain(Look, TEXT("CI_StaCooler"), FLinearColor(.8f, .3f, .03f), .4f);
    HallMakePlain(Look, TEXT("CI_StaPopcornGlow"), FLinearColor(.95f, .85f, .5f), .6f, 0.f, FLinearColor(3.f, 2.6f, 1.4f));
    HallMakePlain(Look, TEXT("CI_StaOnAir"), FLinearColor(.5f, .02f, .02f), .4f, 0.f, FLinearColor(4.f, .3f, .2f));
}
/** The press box's broadcast desk: the station's postgame show still going out (ON AIR). */
void StadiumSounds(int32 Seed, float Height, TArray<FSoundSpot>& Out)
{
    Out.Add({ 2, FVector(500.f, StaPBY0 + 60.f, StaPBFloor + 100.f) });
}
FInteriorDef StadiumDef()
{
    FInteriorDef D{ TEXT("STA"), 2, &BuildStadium, &StaLightAnchors, &StaOpenings, &StaDoors, &StadiumLook };
    D.Sounds = &StadiumSounds;
    D.Bounds = &StaBounds;
    D.Inside = &StaInside;
    D.FloorZ = &StaFloorZ;
    return D;
}
}

// ------------------------------------------------------------------ the exterior's side (HouseExteriorCampus asks these)
bool HouseCampusInteriors::StadiumOpen() { return Enabled(); }
bool HouseCampusInteriors::StadiumGate(int32 K) { return StadiumOpen() && (K == 8 || K == 10); }
bool HouseCampusInteriors::StadiumConcourse(int32 K) { return StadiumOpen() && K >= HouseCampusInteriorsImpl::StaK0 && K <= HouseCampusInteriorsImpl::StaK1; }
bool HouseCampusInteriors::StadiumVomitory(int32 K, int32 Row)
{
    return StadiumOpen() && K == HouseCampusInteriorsImpl::StaVomK && Row >= HouseCampusInteriorsImpl::StaVomRow0 && Row <= HouseCampusInteriorsImpl::StaVomRow1;
}
bool HouseCampusInteriors::StadiumTunnel(int32 K) { return StadiumOpen() && K == HouseCampusInteriorsImpl::StaTunnelK; }
FVector2D HouseCampusInteriors::StadiumPoint(float Degrees, float R)
{
    const FVector P = HouseCampusInteriorsImpl::StaAt(FMath::DegreesToRadians(Degrees), R, 0.f);
    return FVector2D(P.X, P.Y);
}

void HouseCampusInteriors::StadiumGateWall(FHallMesh& M, int32 K, const FVector& A0, const FVector& B0, const FVector& N, bool bWindow)
{
    // The brick wall with a walk-through opening 224 wide to the springing (295); above it the lunette with the grille half
    // up; the stone arch round it, jambs and voussoirs (the solid arch plates are not built on these segments).
    const FVector Along = (B0 - A0).GetSafeNormal();
    const float Len = FVector::Dist(A0, B0), Mid = Len * .5f;
    const FHallUV Brick = FHallUV::World(100.f);
    TArray<FBox2D> Holes = { FBox2D(FVector2D(Mid - 112.f, -1.f), FVector2D(Mid + 112.f, 295.f)) };
    TArray<FVector> Lunette;
    TArray<FBox2D> WallHoles = Holes;
    if (bWindow) WallHoles.Add(FBox2D(FVector2D(Len * StadiumWinT0, StadiumWinZ0), FVector2D(Len * StadiumWinT1, StadiumWinZ1)));
    HouseCampusInteriorsImpl::Subtract(0.f, Len, 0.f, 1050.f, WallHoles, [&](float UA, float UB, float ZA, float ZB)
    {
        M.Quad(TEXT("EX_Brick"), A0 + Along * UA + FVector(0, 0, ZA), A0 + Along * UB + FVector(0, 0, ZA), A0 + Along * UB + FVector(0, 0, ZB), A0 + Along * UA + FVector(0, 0, ZB), N, Brick);
    });
    // The stone base course (it runs round the whole bowl at knee height) stops either side of the opening.
    HouseCampusInteriorsImpl::Subtract(0.f, Len, 30.f, 85.f, Holes, [&](float UA, float UB, float ZA, float ZB)
    {
        M.Quad(TEXT("EX_Stone"), A0 + N * 3.f + Along * UA + FVector(0, 0, ZA), A0 + N * 3.f + Along * UB + FVector(0, 0, ZA), A0 + N * 3.f + Along * UB + FVector(0, 0, ZB), A0 + N * 3.f + Along * UA + FVector(0, 0, ZB), N, FHallUV::World(100.f));
    });
    const FVector Centre = A0 + Along * Mid + N * 9.f, Spring = Centre + FVector(0, 0, 295.f);
    // The lunette: dark behind, a grille of bars in front (half up: it stops at the springing).
    for (int32 I = 0; I <= 12; ++I) { const float T = PI * I / 12.f; Lunette.Add(Spring + N * 1.f + Along * (112.f * FMath::Cos(T)) + FVector(0, 0, 112.f * FMath::Sin(T))); }
    HouseCampusFallKit::Fan(M, TEXT("EX_Dark"), Spring + N * 1.f, Lunette, N, false);
    for (float U = -100.f; U <= 101.f; U += 20.f)
    {
        const float H = FMath::Sqrt(FMath::Max(0.f, 112.f * 112.f - U * U));
        HouseCampusFallKit::Bar(M, TEXT("EX_Metal"), Spring + Along * U + N * 5.f - FVector(0, 0, 18.f), Spring + Along * U + N * 5.f + FVector(0, 0, H), 2.5f, 2.5f, N);
    }
    HouseCampusFallKit::Bar(M, TEXT("EX_Metal"), Spring - Along * 112.f + N * 6.f - FVector(0, 0, 18.f), Spring + Along * 112.f + N * 6.f - FVector(0, 0, 18.f), 6.f, 5.f, N);
    // Jambs and the arch ring in stone.
    for (const float S : { -1.f, 1.f })
        M.Box(TEXT("EX_Stone"), FTransform(Along.Rotation(), Centre + Along * (S * 123.5f) + N * 6.f + FVector(0, 0, 147.5f)), FVector(11.5f, 6.f, 147.5f), FHallUV::World(180.f));
    for (int32 I = 0; I < 12; ++I)
    {
        const float T0 = PI * I / 12.f, T1 = PI * (I + 1) / 12.f, Tm = (T0 + T1) * .5f;
        const FVector Dir = Along * FMath::Cos(Tm) + FVector(0, 0, FMath::Sin(Tm));
        const FRotator Rot = FRotationMatrix::MakeFromXZ(Dir, N).Rotator();
        M.Box(TEXT("EX_Stone"), FTransform(Rot, Spring + Dir * 123.5f + N * 6.f), FVector(11.5f, 123.5f * (T1 - T0) * .55f, 6.f), FHallUV::World(180.f));   // radial, along the ring, deep
    }
}

void HouseCampusInteriors::StadiumTunnelWall(FHallMesh& M, int32 K, const FVector& A0, const FVector& B0, const FVector& NIn, float Top)
{
    // The podium wall, facing the field, round the tunnel's mouth (300 wide, 220 high), with a stone lintel.
    const FVector Along = (B0 - A0).GetSafeNormal();
    const float Len = FVector::Dist(A0, B0), Mid = Len * .5f;
    TArray<FBox2D> Holes = { FBox2D(FVector2D(Mid - 150.f, -1.f), FVector2D(Mid + 150.f, 220.f)) };
    HouseCampusInteriorsImpl::Subtract(0.f, Len, 0.f, Top, Holes, [&](float UA, float UB, float ZA, float ZB)
    {
        M.Quad(TEXT("EX_Stone"), A0 + Along * UA + FVector(0, 0, ZA), A0 + Along * UB + FVector(0, 0, ZA), A0 + Along * UB + FVector(0, 0, ZB), A0 + Along * UA + FVector(0, 0, ZB), NIn, FHallUV::World(180.f));
    });
    M.Box(TEXT("EX_Trim"), FTransform(Along.Rotation(), A0 + Along * Mid + NIn * 3.f + FVector(0, 0, 226.f)), FVector(165.f, 3.f, 6.f), FHallUV::World(100.f));
}

void HouseCampusInteriors::StadiumPressShell(FHallMesh& M)
{
    namespace CI = HouseCampusInteriorsImpl;
    // The exterior's press box, hollow: floor, walls 20 thick, a ceiling under the roof; the window band toward the field
    // (the lit glass is the homecoming dressing's), the door through to the stair tower at the east end.
    const FHallUV UV = FHallUV::World(100.f);
    auto B = [&](float X0, float X1, float Y0, float Y1, float Z0, float Z1) { M.BoxMinMax(TEXT("EX_Dark"), FVector(X0, Y0, Z0), FVector(X1, Y1, Z1), UV); };
    const float Bottom = 1050.f, Top = 1890.f;
    B(CI::StaPBX0, CI::StaPBX1, CI::StaPBY0, CI::StaPBY1, Bottom, CI::StaPBFloor);
    B(CI::StaPBX0, CI::StaPBX1, CI::StaPBY0, CI::StaPBY1, CI::StaPBTop, Top);
    HouseCampusInteriorsImpl::Subtract(CI::StaPBX0, CI::StaPBX1, CI::StaPBFloor, CI::StaPBTop, { FBox2D(FVector2D(300.f, 1180.f), FVector2D(4500.f, 1430.f)) },
        [&](float A, float Bb, float ZA, float ZB) { B(A, Bb, CI::StaPBY0, CI::StaPBY0 + 20.f, ZA, ZB); });
    B(CI::StaPBX0, CI::StaPBX1, CI::StaPBY1 - 20.f, CI::StaPBY1, CI::StaPBFloor, CI::StaPBTop);
    B(CI::StaPBX0, CI::StaPBX0 + 20.f, CI::StaPBY0, CI::StaPBY1, CI::StaPBFloor, CI::StaPBTop);
    HouseCampusInteriorsImpl::Subtract(CI::StaPBY0, CI::StaPBY1, CI::StaPBFloor, CI::StaPBTop, { FBox2D(FVector2D(-9618.f, CI::StaPBFloor - 1.f), FVector2D(-9510.f, CI::StaPBFloor + 222.f)) },
        [&](float A, float Bb, float ZA, float ZB) { B(CI::StaPBX1 - 20.f, CI::StaPBX1, A, Bb, ZA, ZB); });
}

void HouseCampusInteriors::StadiumTower(FHallMesh& M)
{
    namespace CI = HouseCampusInteriorsImpl;
    // A brick stair tower against the press box's east end: walls 20 thick, the pair at its foot on the west face, the door
    // at its head into the press box; inside, a concrete switchback (eight flights of eight risers, landings at each end, a
    // spine wall between the lanes); lamp slits up the east face; a stone cap.
    const FHallUV BrickUV = FHallUV::World(100.f), Plain = FHallUV::World(100.f);
    const float X0 = CI::StaTwX0, X1 = CI::StaTwX1, Y0 = CI::StaTwY0, Y1 = CI::StaTwY1, Floor = CI::StaFloor;
    auto B = [&](FName K, float AX, float BX, float AY, float BY, float AZ, float BZ) { M.BoxMinMax(K, FVector(AX, AY, AZ), FVector(BX, BY, BZ), K == TEXT("EX_Brick") ? BrickUV : Plain); };
    B(TEXT("EX_Brick"), X0, X1, Y0, Y0 + 20.f, CI::StaGround, CI::StaTwTop);
    B(TEXT("EX_Brick"), X0, X1, Y1 - 20.f, Y1, CI::StaGround, CI::StaTwTop);
    HouseCampusInteriorsImpl::Subtract(Y0 + 20.f, Y1 - 20.f, CI::StaGround, CI::StaTwTop, { FBox2D(FVector2D(-9610.f, CI::StaPBFloor - 1.f), FVector2D(-9518.f, CI::StaPBFloor + 213.f)), FBox2D(FVector2D(-9618.f, CI::StaGround - 1.f), FVector2D(-9488.f, Floor + 213.f)) },
        [&](float A, float Bb, float ZA, float ZB) { B(TEXT("EX_Brick"), X0, X0 + 20.f, A, Bb, ZA, ZB); });
    TArray<FBox2D> Slits;
    for (int32 L = 1; L <= 8; ++L) Slits.Add(FBox2D(FVector2D(L % 2 ? -9200.f : -9560.f, Floor + 140.f * L + 120.f), FVector2D(L % 2 ? -9170.f : -9530.f, Floor + 140.f * L + 220.f)));
    HouseCampusInteriorsImpl::Subtract(Y0 + 20.f, Y1 - 20.f, CI::StaGround, CI::StaTwTop, Slits, [&](float A, float Bb, float ZA, float ZB) { B(TEXT("EX_Brick"), X1 - 20.f, X1, A, Bb, ZA, ZB); });
    for (int32 L = 1; L <= 8; ++L)
    {
        const float Y = L % 2 ? -9185.f : -9545.f, Z = Floor + 140.f * L + 170.f;
        B(TEXT("EX_Glass"), X1 - 11.f, X1 - 9.f, Y - 15.f, Y + 15.f, Z - 50.f, Z + 50.f);
    }
    B(TEXT("EX_Stone"), X0 - 10.f, X1 + 10.f, Y0 - 10.f, Y1 + 10.f, CI::StaTwTop, CI::StaTwTop + 30.f);
    B(TEXT("EX_Concrete"), X0 + 20.f, X1 - 20.f, Y0 + 20.f, Y1 - 20.f, CI::StaTwTop - 20.f, CI::StaTwTop);
    // The stairs.
    const float DetailTowerLanding=HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6?130.f:140.f;
    const float IX0 = X0 + 20.f, IX1 = X1 - 20.f, SY0 = Y0 + 20.f + DetailTowerLanding, SY1 = Y1 - 20.f - DetailTowerLanding, SpineX = (IX0 + IX1) * .5f;
    B(TEXT("EX_Concrete"), IX0, IX1, Y0 + 20.f, Y1 - 20.f, CI::StaGround, Floor);                                  // the ground floor
    B(TEXT("EX_Concrete"), SpineX - 5.f, SpineX + 5.f, SY0, SY1, Floor, CI::StaPBFloor + 120.f);                   // the spine
    for (int32 L = 1; L <= 8; ++L)
    {
        const float Z = Floor + 140.f * L;
        const bool bNorth = L % 2 == 1;
        // The landing this flight arrives at.
        if (bNorth) B(TEXT("EX_Concrete"), IX0, IX1, SY1, Y1 - 20.f, Z - 15.f, Z);
        else B(TEXT("EX_Concrete"), IX0, IX1, Y0 + 20.f, SY0, Z - 15.f, Z);
        // The flight: odd ones up the east lane northward, even ones up the west lane southward; thin treads, so the flight
        // two levels up clears the head.
        const float LX0 = bNorth ? SpineX + 5.f : IX0, LX1 = bNorth ? IX1 : SpineX - 5.f;
        for (int32 S = 0; S < 8; ++S)
        {
            const float T0 = SY0 + (SY1 - SY0) * S / 8.f, T1 = SY0 + (SY1 - SY0) * (S + 1) / 8.f;
            const float YA = bNorth ? T0 : SY1 - (T1 - SY0), YB = bNorth ? T1 : SY1 - (T0 - SY0);
            const float ZT = Z - 140.f + 17.5f * (S + 1);
            B(TEXT("EX_Concrete"), LX0, LX1, YA, YB, ZT - 22.f, ZT);
        }
        // A rail on the spine side.
        const FVector R0(bNorth ? SpineX + 9.f : SpineX - 9.f, bNorth ? SY0 : SY1, Z - 140.f + 95.f), R1(R0.X, bNorth ? SY1 : SY0, Z + 95.f);
        M.Cylinder(TEXT("EX_Metal"), R0, R1, 2.5f, 8, Plain);
    }
}
