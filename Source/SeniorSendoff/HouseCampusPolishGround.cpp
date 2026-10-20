// Calder State exterior polish, step 3b (ground): the campus lawns mown in stripes, their colour wandering from lush to
// dry; desire paths worn across them where people cut corners; mulched rings round the trees; leaves blown against the
// buildings; and the plan's walks, sidewalks and plazas jointed every slab, a few slabs cracked. All drawn over the
// exterior's terrain and the plan's paving (no collision: the ground under it keeps its own); nothing on the house's side
// of Fraternity Park Dr.
//
// On the Calder terrain (SourceArt/CalderLandscape, step 1c) every lawn patch, desire path, mulch ring, joint and crack is
// built as before wherever the relief under it is flat (Dormant: at zero relief, everywhere), and elsewhere is draped over
// the relief lattice (HouseCalderDrape) at the same height over the ground or the walk. Joints and cracks on a slab built flat
// stay level on its own top: the plazas (level by design; the tower plaza rides the knoll) and the few sidewalks kept at
// today's height across a frozen zone's edge (HouseCampusLayout::PavedRectFlatTop).
#include "HouseCampusPolish.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"
#include "HouseCalderDrape.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderTerrainTiles.h"
#include "HouseCampusFallKit.h"
#include "HouseCampusLayout.h"
#include "HouseClockTowerMesh.h"
#include "HouseExteriorCampus.h"
#include "HouseExteriorSiteLayout.h"
#include "Engine/Texture2D.h"

namespace HouseCampusPolishGroundImpl
{
// Today's grade and the sheets' heights over it: each is laid that far (Z - G) over the ground (the Calder terrain).
constexpr float G = -60.f, LawnZ = -59.65f, PathZ = -59.45f, MulchZ = -59.3f;
constexpr float CampusX0 = -20300.f, CampusX1 = 19300.f, CampusY0 = -26400.f, CampusY1 = -6000.f;
const FHallUV& Plain() { return HouseCampusFallKit::PlainUV(); }

/** The ground's height at P plus Z's height over today's grade (Z - G is exact, so on today's flat campus this is Z). */
float OverGround(const FVector2D& P, float Z) { return HouseExteriorSite::GroundHeight(P.X, P.Y) + (Z - G); }
FVector OnGround(const FVector2D& P, float Z) { return FVector(P, OverGround(P, Z)); }
/** The joints' height on the plan's paving at P: the walk's top (SurfaceZ) + .07. */
float JointZ(const FVector2D& P) { return HouseCalderTerrain::SurfaceZ(P.X, P.Y, HouseCalderTerrain::EPave::Walk) + .07f; }

/** Step 1c's gate: a piece keeps today's build where the relief under it and one lattice cell round it is 0, where its
 *  footprint straddles a frozen zone's edge (its frozen part never moves; the same test as HouseCampusLayout's) or lies
 *  wholly in one (today's ground there); only elsewhere is it draped. At zero relief nothing is ever draped. */
bool Dormant(const FBox2D& Box)
{
    if (HouseCalderTerrain::ReliefFlat(Box.ExpandBy(HouseCalderTerrain::Cell)) || HouseCalderTerrain::BoxFrozen(Box)) return true;
    const FVector2D C = Box.GetCenter();
    const FVector2D Points[] = { Box.Min, FVector2D(C.X, Box.Min.Y), FVector2D(Box.Max.X, Box.Min.Y), FVector2D(Box.Min.X, C.Y), C,
                                 FVector2D(Box.Max.X, C.Y), FVector2D(Box.Min.X, Box.Max.Y), FVector2D(C.X, Box.Max.Y), Box.Max };
    int32 In = 0, Out = 0;
    for (const FVector2D& P : Points) (HouseCalderTerrain::InFrozen(P.X, P.Y) ? In : Out)++;
    return In > 0 && Out > 0;
}

/** Open lawn here: inside the campus, off the plan's paving and buildings, off the stadium and its apron. */
bool Lawn(const FVector2D& P, float Pad)
{
    if (P.X < CampusX0 || P.X > CampusX1 || P.Y < CampusY0 || P.Y > CampusY1) return false;
    if (HouseCampusLayout::Occupied(P, Pad) || HouseCampusLayout::OnTrafficLane(P)) return false;
    const FVector2D D(P.X - HouseCampus::StadiumX, P.Y - HouseCampus::StadiumY);
    if (FMath::Square(D.X / (5600.f + 500.f + Pad)) + FMath::Square(D.Y / (3800.f + 500.f + Pad)) < 1.f) return false;
    if (FMath::Abs(P.X + 9000.f) < 400.f + Pad && FMath::Abs(P.Y + 10800.f) < 400.f + Pad) return false;   // the tower's plinth
    return true;
}

void Lawns(HouseCampusPolish::FParts& P)
{
    // 3 m cells over the campus: stripes of light and dark (the mower's passes, 4.5 m wide, running north-south), and the
    // grass itself lusher in the low places, drier and thinner where the sun bakes it.
    static const FName Keys[3][2] = { { TEXT("CP_LawnLushA"), TEXT("CP_LawnLushB") }, { TEXT("CP_LawnA"), TEXT("CP_LawnB") }, { TEXT("CP_LawnDryA"), TEXT("CP_LawnDryB") } };
    constexpr float Cell = 300.f;
    const FHallUV UV = FHallUV::World(260.f);
    for (float Y = CampusY0; Y < CampusY1; Y += Cell)
        for (float X = CampusX0; X < CampusX1; X += Cell)
        {
            bool bOK = true;
            for (const FVector2D& C : { FVector2D(X, Y), FVector2D(X + Cell, Y), FVector2D(X, Y + Cell), FVector2D(X + Cell, Y + Cell), FVector2D(X + Cell * .5f, Y + Cell * .5f) })
                bOK &= Lawn(C, 8.f);
            if (!bOK) continue;
            const float Wet = HallFbm(X * .00035f, Y * .00035f, 4, 1986);
            const int32 Tone = Wet > .58f ? 0 : Wet < .38f ? 2 : 1;
            const int32 Stripe = FMath::FloorToInt((X + 20000.f) / 450.f) & 1;
            const FBox2D Patch(FVector2D(X, Y), FVector2D(X + Cell, Y + Cell));
            // The continuous owned ground carries contour mowing and macro
            // colour. Keep every legacy patch touching a frozen rectangle.
            if (HouseCalderTerrainTiles::UsesLandscapeGround())
            {
                bool bProtected = false;
                for (const FBox2D& Frozen : HouseCalderTerrain::FrozenRects()) bProtected |= Frozen.Intersect(Patch);
                if (!bProtected) continue;
            }
            if (Dormant(Patch))
                P.Lawn.Quad(Keys[Tone][Stripe], OnGround(FVector2D(X, Y), LawnZ), OnGround(FVector2D(X + Cell, Y), LawnZ), OnGround(FVector2D(X + Cell, Y + Cell), LawnZ), OnGround(FVector2D(X, Y + Cell), LawnZ), FVector::UpVector, UV);
            else   // over the relief one quad on its corners would cut through the lattice's 2 m triangles: drape it on them
                HouseCalderDrape::Sheet(P.Lawn, Keys[Tone][Stripe], HouseCalderDrape::RectOutline(Patch), [](float Sx, float Sy) { return OverGround(FVector2D(Sx, Sy), LawnZ); }, UV);
        }
}

/** A worn path along a polyline: a strip of quads with ragged edges, only where it crosses lawn. */
void DesirePath(HouseCampusPolish::FParts& P, const TArray<FVector2D>& Line, float Width, int32 Seed)
{
    FRandomStream R(Seed);
    for (int32 I = 0; I + 1 < Line.Num(); ++I)
    {
        const FVector2D A = Line[I], B = Line[I + 1], Dir = (B - A).GetSafeNormal(), Side(-Dir.Y, Dir.X);
        const float Len = FVector2D::Distance(A, B);
        float W0 = Width * R.FRandRange(.7f, 1.1f);
        for (float T = 0.f; T < Len; T += 80.f)
        {
            const float T1 = FMath::Min(Len, T + 80.f), W1 = FMath::Clamp(W0 + R.FRandRange(-12.f, 12.f), Width * .55f, Width * 1.25f);
            const FVector2D P0 = A + Dir * T, P1 = A + Dir * T1, Mid = (P0 + P1) * .5f;
            const float Wander = 14.f * FMath::Sin((T + I * 300.f) * .004f);
            if (Lawn(Mid, 20.f))
            {
                const FVector2D Q0 = P0 - Side * (W0 * .5f) + Side * Wander, Q1 = P1 - Side * (W1 * .5f) + Side * Wander;
                const FVector2D Q2 = P1 + Side * (W1 * .5f) + Side * Wander, Q3 = P0 + Side * (W0 * .5f) + Side * Wander;
                FBox2D Foot(ForceInit);
                for (const FVector2D& Q : { Q0, Q1, Q2, Q3 }) Foot += Q;
                if (Dormant(Foot))
                    P.Far.Quad(TEXT("CP_DesirePath"), OnGround(Q0, PathZ), OnGround(Q1, PathZ), OnGround(Q2, PathZ), OnGround(Q3, PathZ), FVector::UpVector, FHallUV::World(200.f));
                else
                    HouseCalderDrape::Sheet(P.Far, TEXT("CP_DesirePath"), { Q0, Q1, Q2, Q3 }, [](float Sx, float Sy) { return OverGround(FVector2D(Sx, Sy), PathZ); }, FHallUV::World(200.f));
            }
            W0 = W1;
        }
    }
}

void TreeRings(HouseCampusPolish::FParts& P)
{
    for (const FVector& T : P.Trees)
    {
        const FVector2D C(T.X, T.Y);
        const float R = FMath::Clamp(85.f + 30.f * T.Z, 90.f, 160.f);
        if (!Lawn(C, R)) continue;
        if (!Dormant(FBox2D(C - FVector2D(R + 2.f, R + 2.f), C + FVector2D(R + 2.f, R + 2.f))))
        {
            // Over the relief: the mulch draped on the ground, its edging along the rim split where the lattice crosses it.
            TArray<FVector2D> Outline;
            for (int32 K = 0; K < 20; ++K)
            {
                const float A = 2.f * PI * K / 20.f;
                Outline.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
            }
            HouseCalderDrape::Sheet(P.Ground, TEXT("CP_Mulch"), Outline, [](float Sx, float Sy) { return OverGround(FVector2D(Sx, Sy), MulchZ); }, Plain());
            const TArray<FVector2D> Edge = HouseCalderDrape::Densify(Outline);
            for (int32 K = 0; K < Edge.Num(); ++K)
                HouseCampusFallKit::Bar(P.Ground, TEXT("CP_Edging"), OnGround(Edge[K], MulchZ) + FVector(0, 0, .8f), OnGround(Edge[(K + 1) % Edge.Num()], MulchZ) + FVector(0, 0, .8f), 2.f, 3.f, FVector::UpVector);
            continue;
        }
        const float Mulch = OverGround(C, MulchZ);   // a flat disc at the ground under the trunk
        TArray<FVector> Rim;
        for (int32 K = 0; K < 20; ++K)
        {
            const float A = 2.f * PI * K / 20.f;
            Rim.Add(FVector(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R, Mulch));
        }
        HouseCampusFallKit::Fan(P.Ground, TEXT("CP_Mulch"), FVector(C, Mulch), Rim, FVector::UpVector);
        for (int32 K = 0; K < 20; ++K) HouseCampusFallKit::Bar(P.Ground, TEXT("CP_Edging"), Rim[K] + FVector(0, 0, .8f), Rim[(K + 1) % 20] + FVector(0, 0, .8f), 2.f, 3.f, FVector::UpVector);
    }
}

/** Leaves blown against a wall and into the corners of the buildings. */
void WallLeaves(HouseCampusPolish::FParts& P, FRandomStream& R)
{
    static const FName Keys[] = { TEXT("CP_LeafRed"), TEXT("CP_LeafOrange"), TEXT("CP_LeafBrown"), TEXT("CP_LeafYellow"), TEXT("CP_LeafBrown") };
    for (const HouseCampusLayout::FBlock& B : HouseCampusLayout::Blocks())
        for (int32 K = 0; K < 6; ++K)
        {
            // Along a random side, deepest at the wall.
            const int32 Side = R.RandRange(0, 3);
            const float U = R.FRand();
            const FVector2D At = Side == 0 ? FVector2D(FMath::Lerp(B.X0, B.X1, U), B.Y1 + 14.f) : Side == 1 ? FVector2D(FMath::Lerp(B.X0, B.X1, U), B.Y0 - 14.f) : Side == 2 ? FVector2D(B.X0 - 14.f, FMath::Lerp(B.Y0, B.Y1, U)) : FVector2D(B.X1 + 14.f, FMath::Lerp(B.Y0, B.Y1, U));
            const FVector2D Out = Side == 0 ? FVector2D(0, 1) : Side == 1 ? FVector2D(0, -1) : Side == 2 ? FVector2D(-1, 0) : FVector2D(1, 0), Along(-Out.Y, Out.X);
            if (!Lawn(At + Out * 60.f, 10.f)) continue;
            const float Len = R.FRandRange(120.f, 260.f);
            for (int32 I = 0; I < int32(Len * .8f); ++I)
            {
                const float Uu = R.FRandRange(-.5f, .5f) * Len, Dd = FMath::Square(R.FRand()) * 80.f;
                const FVector2D Spot = At + Along * Uu + Out * (Dd + 4.f);
                const FVector C(Spot, OverGround(Spot, LawnZ) + .5f + (1.f - Dd / 80.f) * 6.f * R.FRand());
                const FRotator Ro(R.FRandRange(-30.f, 30.f), R.FRandRange(0.f, 360.f), R.FRandRange(-30.f, 30.f));
                const FVector X = Ro.RotateVector(FVector(1, 0, 0)) * R.FRandRange(3.f, 5.f), Y = Ro.RotateVector(FVector(0, 1, 0)) * R.FRandRange(2.2f, 3.6f);
                if (Dormant(FBox2D(Spot - FVector2D(8.f, 8.f), Spot + FVector2D(8.f, 8.f))))
                    P.Ground.Quad(Keys[R.RandRange(0, 4)], C - X - Y, C + X - Y, C + X + Y, C - X + Y, Ro.RotateVector(FVector::UpVector), Plain());
                else
                {
                    // Over the relief the leaf lies on the slope: its tumble turned from level onto the ground's plane there.
                    const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, HouseExteriorSite::GroundNormal(Spot.X, Spot.Y));
                    const FVector TX = Tilt.RotateVector(X), TY = Tilt.RotateVector(Y);
                    P.Ground.Quad(Keys[R.RandRange(0, 4)], C - TX - TY, C + TX - TY, C + TX + TY, C - TX + TY, Tilt.RotateVector(Ro.RotateVector(FVector::UpVector)), Plain());
                }
            }
        }
}

/** A tooled joint, or one step of a crack, from A to B, Width wide, Above the joints' height. Where the relief under it is flat
 *  it is a thin bar between its ends on the paving; elsewhere a sheet draped at JointZ + Above, so that across a draped walk
 *  it lies on the walk all the way instead of running straight from end to end, but on a slab built flat (Level: its top, from
 *  HouseCampusLayout::PavedRectFlatTop) a level bar on that top. */
void JointStrip(FHallMesh& M, const FVector2D& A, const FVector2D& B, float Width, float Above, const float* Level = nullptr)
{
    FBox2D Foot(ForceInit);
    Foot += A;
    Foot += B;
    if (Dormant(Foot.ExpandBy(Width * .5f)))
        HouseCampusFallKit::Bar(M, TEXT("CP_Joint"), FVector(A, JointZ(A) + Above), FVector(B, JointZ(B) + Above), Width, .08f, FVector::UpVector);
    else if (Level)
        HouseCampusFallKit::Bar(M, TEXT("CP_Joint"), FVector(A, *Level + .07f + Above), FVector(B, *Level + .07f + Above), Width, .08f, FVector::UpVector);
    else
        HouseCalderDrape::Sheet(M, TEXT("CP_Joint"), HouseCalderDrape::RunOutline(A, B, Width), [Above](float Sx, float Sy) { return JointZ(FVector2D(Sx, Sy)) + Above; }, Plain());
}

/** A crack wandering across part of a slab, Above the joints' height (Level: the slab's top, where it is built flat). */
void Crack(FHallMesh& M, FRandomStream& R, const FVector& From, const FVector& Dir, float Len, float Above, const float* Level = nullptr)
{
    FVector P = From, D = Dir;
    for (int32 K = 0; K < 6; ++K)
    {
        D = (D + FVector(R.FRandRange(-.5f, .5f), R.FRandRange(-.5f, .5f), 0.f)).GetSafeNormal2D();
        const FVector Q = P + D * (Len / 6.f);
        JointStrip(M, FVector2D(P), FVector2D(Q), .7f, Above, Level);
        P = Q;
    }
}
void Joints(HouseCampusPolish::FParts& P)
{
    // Walks: a tooled joint across every 1.5 m slab; one slab in nine cracked. Every joint on the paving's top (JointZ): its
    // two ends where the relief is flat, all along it where the walk drapes (JointStrip).
    FRandomStream R(1961);
    TArray<HouseCampusLayout::FWalkStrip> Strips;
    HouseCampusLayout::WalkStrips(Strips);
    for (const HouseCampusLayout::FWalkStrip& S : Strips)
    {
        const FVector2D Dir = (S.B - S.A).GetSafeNormal(), Side(-Dir.Y, Dir.X);
        const float Len = FVector2D::Distance(S.A, S.B);
        for (float T = 150.f; T < Len - 20.f; T += 150.f)
        {
            const FVector2D C = S.A + Dir * T;
            if (C.Y > -5600.f) continue;   // the house's side of the street is not ours
            JointStrip(P.Ground, C - Side * (S.W * .5f - 3.f), C + Side * (S.W * .5f - 3.f), 1.2f, 0.f);
            if (R.FRand() < .11f) Crack(P.Ground, R, FVector(C - Dir * R.FRandRange(20.f, 120.f) + Side * R.FRandRange(-S.W * .4f, S.W * .4f), 0.f), FVector(Side, 0.f) * (R.FRand() < .5f ? 1.f : -1.f), S.W * R.FRandRange(.5f, .9f), .01f);
        }
    }
    // Sidewalks and plazas: slabs on a grid. A sidewalk drapes on the relief and its joints with it; a plaza is level by design
    // (and a sidewalk across a frozen zone's edge keeps today's height), so on the relief their joints and cracks stay level on
    // that top (Level: HouseCampusLayout::PavedRectFlatTop).
    TArray<FBox2D> Rects;
    HouseCampusLayout::PavedRects(Rects);
    for (const FBox2D& Rc : Rects)
    {
        if (Rc.Max.Y > -5600.f) continue;
        float FlatTop = 0.f;
        const float* Level = HouseCampusLayout::PavedRectFlatTop(Rc, FlatTop) ? &FlatTop : nullptr;
        const FVector2D Sz = Rc.GetSize();
        const bool bLong = FMath::Max(Sz.X, Sz.Y) > 3.f * FMath::Min(Sz.X, Sz.Y);
        const float Step = bLong ? 150.f : 200.f;
        for (float X = Rc.Min.X + Step; X < Rc.Max.X - 20.f; X += Step)
            if (!bLong || Sz.X > Sz.Y) JointStrip(P.Ground, FVector2D(X, Rc.Min.Y + 3.f), FVector2D(X, Rc.Max.Y - 3.f), 1.2f, 0.f, Level);
        for (float Y = Rc.Min.Y + Step; Y < Rc.Max.Y - 20.f; Y += Step)
            if (!bLong || Sz.Y >= Sz.X) JointStrip(P.Ground, FVector2D(Rc.Min.X + 3.f, Y), FVector2D(Rc.Max.X - 3.f, Y), 1.2f, 0.f, Level);
        for (int32 K = 0; K < int32(Sz.X * Sz.Y / 400000.f); ++K)
            Crack(P.Ground, R, FVector(R.FRandRange(Rc.Min.X + 30.f, Rc.Max.X - 30.f), R.FRandRange(Rc.Min.Y + 30.f, Rc.Max.Y - 30.f), 0.f), FVector(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal2D(), R.FRandRange(60.f, 140.f), .01f, Level);
    }
    // The tower plaza: rings and radial joints round the plinth, on the plaza's own top. The plaza (the one paved disc) is
    // level and rides the tower's knoll (HouseCampusLayout builds it at WalkTopZ + HouseClockTowerSite::Lift()), so its
    // joints do too, not the ground under them.
    TArray<FVector> Discs;
    HouseCampusLayout::PavedDiscs(Discs);
    const float DiscZ = HouseCampusLayout::WalkTopZ + HouseClockTowerSite::Lift() + .07f;
    for (const FVector& Dc : Discs)
    {
        for (const float Rr : { Dc.Z * .62f, Dc.Z * .81f })
            for (int32 K = 0; K < 48; ++K)
            {
                const float A0 = 2.f * PI * K / 48.f, A1 = 2.f * PI * (K + 1) / 48.f;
                const FVector P0(Dc.X + FMath::Cos(A0) * Rr, Dc.Y + FMath::Sin(A0) * Rr, DiscZ), P1(Dc.X + FMath::Cos(A1) * Rr, Dc.Y + FMath::Sin(A1) * Rr, DiscZ);
                if (FMath::Abs(P0.X - Dc.X) < 400.f && FMath::Abs(P0.Y - Dc.Y) < 400.f) continue;
                HouseCampusFallKit::Bar(P.Ground, TEXT("CP_Joint"), P0, P1, 1.2f, .08f, FVector::UpVector);
            }
        for (int32 K = 0; K < 16; ++K)
        {
            const FVector Dd(FMath::Cos(K * PI / 8.f), FMath::Sin(K * PI / 8.f), 0.f);
            const float R0 = 400.f / FMath::Max(FMath::Abs(Dd.X), FMath::Abs(Dd.Y));
            HouseCampusFallKit::Bar(P.Ground, TEXT("CP_Joint"), FVector(Dc.X, Dc.Y, DiscZ) + Dd * (R0 + 4.f), FVector(Dc.X, Dc.Y, DiscZ) + Dd * (Dc.Z - 6.f), 1.2f, .08f, FVector::UpVector);
        }
    }
}

void PaintGrass(FHallImage& Img, int32 Seed)
{
    // Close-mown turf: fine blade noise, a few clover patches; tinted per material.
    constexpr int32 N = 256;
    Img.Init(N, N, FLinearColor(.5f, .5f, .5f));
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const float Blade = HallNoise(X * .9f, Y * .9f, Seed) * .5f + HallNoise(X * 2.3f, Y * 2.3f, Seed + 1) * .3f;
            const float Patch = HallFbm(X * .03f, Y * .03f, 3, Seed + 2);
            Img.At(X, Y) = FLinearColor(.42f, .5f, .3f) * (.72f + .4f * Blade) * (.88f + .24f * Patch);
        }
}
void PaintDirt(FHallImage& Img, int32 Seed)
{
    constexpr int32 N = 128;
    Img.Init(N, N, FLinearColor(.2f, .16f, .11f));
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const float V = .8f + .3f * HallFbm(X * .08f, Y * .08f, 3, Seed);
            const float Grass = HallNoise(X * .5f, Y * .5f, Seed + 1);
            const float Edge = FMath::Abs(Y / float(N) - .5f) * 2.f;
            Img.At(X, Y) = Grass > .62f - Edge * .35f ? FLinearColor(.1f, .14f, .06f) * V : FLinearColor(.22f, .17f, .11f) * V;
        }
}
void PaintMulch(FHallImage& Img, int32 Seed)
{
    constexpr int32 N = 128;
    Img.Init(N, N, FLinearColor(.12f, .07f, .04f));
    FRandomStream R(Seed);
    for (int32 K = 0; K < 900; ++K)
        Img.Ellipse(FVector2D(R.FRandRange(0.f, N), R.FRandRange(0.f, N)), FVector2D(R.FRandRange(2.f, 6.f), R.FRandRange(.8f, 1.6f)), R.FRandRange(0.f, 180.f), FLinearColor(.2f, .11f, .05f) * R.FRandRange(.6f, 1.3f));
}
}

void HouseCampusPolish::GroundLook(UHouseBlackHallSubsystem* Look)
{
    namespace GI = HouseCampusPolishGroundImpl;
    FHallImage Img;
    GI::PaintGrass(Img, 81);
    UTexture2D* Grass = HallMakeTexture(Img, true, false, TEXT("T_CP_Lawn"), true);
    if (Grass) Look->Textures.Add(TEXT("T_CP_Lawn"), Grass);
    struct FTone { const TCHAR* Key; FLinearColor Tint; };
    const FTone Tones[] = {
        { TEXT("CP_LawnLushA"), FLinearColor(.62f, .8f, .52f) }, { TEXT("CP_LawnLushB"), FLinearColor(.5f, .68f, .42f) },
        { TEXT("CP_LawnA"), FLinearColor(.66f, .74f, .5f) }, { TEXT("CP_LawnB"), FLinearColor(.54f, .62f, .41f) },
        { TEXT("CP_LawnDryA"), FLinearColor(.78f, .74f, .48f) }, { TEXT("CP_LawnDryB"), FLinearColor(.66f, .62f, .4f) },
    };
    for (const FTone& T : Tones) HallMakeTextured(Look, T.Key, Grass, nullptr, .93f, T.Tint);
    GI::PaintDirt(Img, 82);
    UTexture2D* Dirt = HallMakeTexture(Img, true, false, TEXT("T_CP_DesirePath"), true);
    if (Dirt) Look->Textures.Add(TEXT("T_CP_DesirePath"), Dirt);
    HallMakeTextured(Look, TEXT("CP_DesirePath"), Dirt, nullptr, .95f);
    GI::PaintMulch(Img, 83);
    UTexture2D* Mulch = HallMakeTexture(Img, true, false, TEXT("T_CP_Mulch"), true);
    if (Mulch) Look->Textures.Add(TEXT("T_CP_Mulch"), Mulch);
    HallMakeTextured(Look, TEXT("CP_Mulch"), Mulch, nullptr, .95f);
    HallMakePlain(Look, TEXT("CP_Edging"), FLinearColor(.08f, .07f, .06f), .6f, .4f);
}

void HouseCampusPolish::Ground(FParts& P)
{
    namespace GI = HouseCampusPolishGroundImpl;
    GI::Lawns(P);
    // Desire paths: across the lawn to the stadium's north-east side, the corner of the green toward the ticket plaza, the
    // quad's diagonal to the Union, behind Calder Hall to Alumni Hall, and the shortcut from the dorm row to the Commons.
    GI::DesirePath(P, { { 8450.f, -13150.f }, { 7851.f, -11731.f }, { 6886.f, -10637.f }, { 5900.f, -9950.f } }, 95.f, 1);
    GI::DesirePath(P, { { 6200.f, -6700.f }, { 4900.f, -7700.f }, { 3700.f, -8550.f } }, 85.f, 2);
    GI::DesirePath(P, { { -14300.f, -11800.f }, { -11600.f, -12400.f }, { -8600.f, -12900.f } }, 80.f, 3);
    GI::DesirePath(P, { { -17400.f, -18700.f }, { -17200.f, -20300.f }, { -17300.f, -21750.f } }, 75.f, 4);
    GI::DesirePath(P, { { 14600.f, -9000.f }, { 14400.f, -8200.f }, { 14300.f, -6700.f } }, 70.f, 5);
    GI::TreeRings(P);
    FRandomStream R(1018);
    GI::WallLeaves(P, R);
    GI::Joints(P);
}
