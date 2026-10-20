// The Calder landform's walls, rock faces, stairs and ramps (see HouseCalderFeatures.h).
#include "HouseCalderFeatures.h"
#include "HouseCalderLandscape.h"
#include "HouseCalderLandform.h"
#include "HouseCampusLayout.h"
#include "HouseCampusInteriors.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderTerrainTiles.h"
#include "HouseCalderWater.h"
#include "HouseExteriorSiteLayout.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"

namespace HouseCalderFeaturesImpl
{
using HouseCalderLandform::EWallKind;

constexpr int32 TilesPerSide = int32(2 * HouseCalderTerrain::Half / HouseCalderTerrainTiles::TileSize);   // 5, as the terrain
constexpr float WallThick = 40.f;          // a wall stands this far into its high side, its low face on the wall line
constexpr float FootDepth = 40.f;          // and reaches this far under the low ground
constexpr float MinStep = 3.f;             // smaller steps are left to the ground
constexpr double SteepCollisionNz = .766;  // cos 40 deg: an up-facing collision triangle steeper than this is Unwalkable
constexpr float ParapetDrop = 75.f;        // a drop this deep gets a parapet (Wall/PadCourt/Terrace/Lot/Apron/Parapet/Dam/Reserve)
constexpr float ParapetRise = 100.f;
constexpr float OverlookParapetRise = 107.f;   // Founders' Overlook: the 1.07 m stone parapet of the PLAN
constexpr float CopingRise = 8.f;          // a low wall's coping stands this far over the high ground
constexpr float CopingOverhang = 10.f;
constexpr float RockAmp = 12.f;            // a rock face's value-noise relief, +-
constexpr float RockBatterTan = .14054083f;   // tan(8 deg): rock faces lean back
constexpr float RockLip = 2.f;             // a rock face's top stands this far over the high ground
constexpr float RockRecess = 12.f;         // ... and may be let this far back into the high side where paving leaves no room
constexpr float WallSampleStep = 100.f;
constexpr float FineSampleStep = 20.f;     // class changes along a wall are looked for this finely (then pinned to 4 mm)
constexpr float OpeningTolerance = 2.f;    // a wall opens only where the stair's tread (or cheek) reaches the high ground to this
constexpr float MaxHeadLip = 25.f;         // a top tread is raised to the highest ground it covers by at most this
constexpr float CheekWidth = 30.f;
constexpr float RampWidth = 180.f, RampLanding = 150.f;
constexpr float LandingReach = 25.f;       // a ramp landing's open sides and end reach this far on (15 cm past a wall 10 cm off)
constexpr float RailRadius = 2.f, PostRadius = 2.4f;
constexpr float StairHandrail = 90.f, RampHandrail = 90.f, LedgeRail = 100.f;
constexpr float PostSpacing = 150.f;
constexpr double RockHousePathHalf = 45.0;   // the rock house path's walking width, either side of its line (HouseCalderLandform::RockHouse)
constexpr float CutRailReach = 100.f;      // paving this close to a cut's high side gets a rail at its drop (as the review's WalkAt)
constexpr float CutRailInset = 0.f;        // that rail's posts stand on the cut's edge (the paving on its high side runs to the line)
constexpr float TreadLift = .6f;           // treads and ramp tops stand 6 mm over the walk they meet (no shared plane)

enum EMat : uint8 { MatWall, MatField, MatRock, MatCut, MatCoping, MatStep, MatIron, MatBronze, MatCount };
const TCHAR* const MatSlots[MatCount] = { TEXT("Wall"), TEXT("Fieldstone"), TEXT("Rock"), TEXT("RockCut"), TEXT("Coping"), TEXT("Step"), TEXT("Iron"), TEXT("Bronze") };
/** Which collision a face joins: none, the walkable mesh (copings, rock lips, treads, cheeks, ramps) or the Unwalkable one
 *  (wall and rock faces, rails), so nothing climbs a wall. */
enum class EColl : uint8 { None, Walk, Block };
enum class ECat : uint8 { Wall, Stair, Ramp };
constexpr int32 NumCats = 3;
const TCHAR* const CatNames[NumCats] = { TEXT("CalderWall"), TEXT("CalderStair"), TEXT("CalderRamp") };

// ------------------------------------------------------------------ ground, plan helpers and noise
float GroundAt(const FVector2D& P) { return HouseExteriorSite::GroundHeight(float(P.X), float(P.Y)); }
float BaseAt(const FVector2D& P) { return HouseExteriorSite::BaseGrade(float(P.X), float(P.Y)); }
bool FrozenAt(const FVector2D& P) { return HouseCalderTerrain::InFrozen(float(P.X), float(P.Y)); }
/** P lies more than 1 cm inside a frozen rectangle (not on its edge). */
bool InsideFrozen(const FVector2D& P)
{
    for (const FBox2D& R : HouseCalderTerrain::FrozenRects())
        if (P.X > R.Min.X + 1.0 && P.X < R.Max.X - 1.0 && P.Y > R.Min.Y + 1.0 && P.Y < R.Max.Y - 1.0) return true;
    return false;
}
FVector Lift(const FVector2D& P, double Z) { return FVector(P.X, P.Y, Z); }
double Cross2(const FVector2D& U, const FVector2D& V) { return U.X * V.Y - U.Y * V.X; }

/** Integer hash (no RNG stream: the same value for the same lattice point on every machine). */
uint32 Mix(int32 X, int32 Y, int32 Z, uint32 Seed)
{
    uint32 H = Seed ^ 0x9E3779B9u;
    H = (H ^ uint32(X)) * 0x85EBCA6Bu; H ^= H >> 13;
    H = (H ^ uint32(Y)) * 0xC2B2AE35u; H ^= H >> 16;
    H = (H ^ uint32(Z)) * 0x27D4EB2Fu; H ^= H >> 15;
    H *= 0x165667B1u; H ^= H >> 16;
    return H;
}

/** Smoothstep value noise in world space, -1..1. */
float Value3(const FVector& P, float Period, uint32 Seed)
{
    const FVector Q = P / Period;
    const int32 IX = FMath::FloorToInt(Q.X), IY = FMath::FloorToInt(Q.Y), IZ = FMath::FloorToInt(Q.Z);
    const float FX = float(Q.X - IX), FY = float(Q.Y - IY), FZ = float(Q.Z - IZ);
    const float SX = FX * FX * (3.f - 2.f * FX), SY = FY * FY * (3.f - 2.f * FY), SZ = FZ * FZ * (3.f - 2.f * FZ);
    auto Corner = [IX, IY, IZ, Seed](int32 DX, int32 DY, int32 DZ) { return float(Mix(IX + DX, IY + DY, IZ + DZ, Seed) & 0xFFFFFFu) / float(0xFFFFFF); };
    const float X00 = FMath::Lerp(Corner(0, 0, 0), Corner(1, 0, 0), SX), X10 = FMath::Lerp(Corner(0, 1, 0), Corner(1, 1, 0), SX);
    const float X01 = FMath::Lerp(Corner(0, 0, 1), Corner(1, 0, 1), SX), X11 = FMath::Lerp(Corner(0, 1, 1), Corner(1, 1, 1), SX);
    const float V = FMath::Lerp(FMath::Lerp(X00, X10, SY), FMath::Lerp(X01, X11, SY), SZ);
    return V * 2.f - 1.f;
}

/** A rock face's relief, -1..1: bedding-scale lumps and smaller knobs. */
float RockNoise(const FVector& P)
{
    const float N = .62f * Value3(P, 150.f, 7301u) + .38f * Value3(P, 52.f, 7302u);
    return FMath::Clamp(N * 1.45f, -1.f, 1.f);
}

// ------------------------------------------------------------------ meshes (per 128 m tile and kind)
struct FDrawPart { TArray<FVector> V, N; TArray<float> Ao; TArray<int32> I; };
struct FCollPart { TArray<FVector> V; TArray<int32> I; };
struct FBucket { FDrawPart Draw[MatCount]; FCollPart Walk, Block; };

struct FMeshes
{
    TArray<FBucket> Buckets;
    FMeshes() { Buckets.SetNum(NumCats * TilesPerSide * TilesPerSide); }
    static int32 TileOf(const FVector2D& P)
    {
        const double HalfSpan = HouseCalderTerrain::Half, Tile = HouseCalderTerrainTiles::TileSize;
        const int32 TX = FMath::Clamp(FMath::FloorToInt((P.X + HalfSpan) / Tile), 0, TilesPerSide - 1);
        const int32 TY = FMath::Clamp(FMath::FloorToInt((P.Y + HalfSpan) / Tile), 0, TilesPerSide - 1);
        return TY * TilesPerSide + TX;
    }
    FBucket& At(ECat Cat, const FVector2D& P) { return Buckets[int32(Cat) * TilesPerSide * TilesPerSide + TileOf(P)]; }
};

/** A convex polygon facing Out, fanned from its first point. FHallMesh's rule, as the terrain tiles use it: a triangle
 *  (A, B, C) faces the side its cross product points away from. Normals: per point (smooth), or null for the polygon's own.
 *  Mat == MatCount: collision only. */
void Poly(FBucket& B, EMat Mat, EColl Coll, const FVector* P, int32 Num, const FVector& Out, const FVector* Normals = nullptr, const float* Ao = nullptr, float AoAll = 1.f)
{
    if (Num < 3) return;
    FVector Newell(0.0);
    for (int32 K = 0; K < Num; ++K)
    {
        const FVector& U = P[K];
        const FVector& W = P[(K + 1) % Num];
        Newell.X += (U.Y - W.Y) * (U.Z + W.Z);
        Newell.Y += (U.Z - W.Z) * (U.X + W.X);
        Newell.Z += (U.X - W.X) * (U.Y + W.Y);
    }
    FVector Face = Newell.GetSafeNormal();
    if (Face.IsNearlyZero()) return;
    if (FVector::DotProduct(Face, Out) < 0.0) Face = -Face;
    auto Fan = [P, Num, &Face](TArray<int32>& I, int32 Base0)
    {
        for (int32 K = 1; K + 1 < Num; ++K)
        {
            const FVector X = FVector::CrossProduct(P[K] - P[0], P[K + 1] - P[0]);
            if (X.SizeSquared() < 1e-6) continue;
            if (FVector::DotProduct(X, Face) < 0.0) I.Append({ Base0, Base0 + K, Base0 + K + 1 });
            else I.Append({ Base0, Base0 + K + 1, Base0 + K });
        }
    };
    if (Mat != MatCount)
    {
        FDrawPart& D = B.Draw[Mat];
        const int32 Base0 = D.V.Num();
        for (int32 K = 0; K < Num; ++K) { D.V.Add(P[K]); D.N.Add(Normals ? Normals[K] : Face); D.Ao.Add(Ao ? Ao[K] : AoAll); }
        Fan(D.I, Base0);
    }
    if (Coll != EColl::None)
    {
        // Collision triangle by triangle: one of a walkable part's that faces up but is steeper than 40 deg (a coping or rock top
        // stepping between two columns, one half of a non-planar quad) joins the Unwalkable part, so nothing over 40 deg is
        // walkable collision (MASTER_PROMPT, walkability).
        for (int32 K = 1; K + 1 < Num; ++K)
        {
            const FVector X = FVector::CrossProduct(P[K] - P[0], P[K + 1] - P[0]);
            if (X.SizeSquared() < 1e-6) continue;
            const bool bFlip = FVector::DotProduct(X, Face) < 0.0;
            const double Up = (bFlip ? -X : X).GetSafeNormal().Z;
            FCollPart& C = Coll == EColl::Walk && !(Up > .05 && Up < SteepCollisionNz) ? B.Walk : B.Block;
            const int32 Base0 = C.V.Num();
            C.V.Append({ P[0], P[K], P[K + 1] });
            if (bFlip) C.I.Append({ Base0, Base0 + 1, Base0 + 2 });
            else C.I.Append({ Base0, Base0 + 2, Base0 + 1 });
        }
    }
}

/** A hexahedron: Low[4] round its base, High[4] above them. Faces by Mask: 1 top, 2 bottom, 4 << K side K (Low[K] to
 *  Low[K + 1]). Each face looks away from the solid's centre. */
void Hexa(FBucket& B, EMat Mat, EColl Coll, const FVector (&Low)[4], const FVector (&High)[4], int32 Mask, float AoLow = 1.f, float AoHigh = 1.f)
{
    FVector Centre(0.0);
    for (int32 K = 0; K < 4; ++K) Centre += Low[K] + High[K];
    Centre /= 8.0;
    auto Emit = [&](const FVector (&Q)[4], const float (&A)[4]) { Poly(B, Mat, Coll, Q, 4, (Q[0] + Q[1] + Q[2] + Q[3]) * .25 - Centre, nullptr, A); };
    if (Mask & 1) { const FVector Q[4] = { High[0], High[1], High[2], High[3] }; const float A[4] = { AoHigh, AoHigh, AoHigh, AoHigh }; Emit(Q, A); }
    if (Mask & 2) { const FVector Q[4] = { Low[0], Low[1], Low[2], Low[3] }; const float A[4] = { AoLow, AoLow, AoLow, AoLow }; Emit(Q, A); }
    for (int32 K = 0; K < 4; ++K)
    {
        if (!(Mask & (4 << K))) continue;
        const int32 L = (K + 1) % 4;
        const FVector Q[4] = { Low[K], Low[L], High[L], High[K] };
        const float A[4] = { AoLow, AoLow, AoHigh, AoHigh };
        Emit(Q, A);
    }
}
constexpr int32 AllFaces = 63, SidesOnly = 60, SidesAndTop = 61;

/** A box in a plan frame: Along [A0, A1] x Across [S0, S1] from Origin, bottom ZLow, its top ZTop0 at A0 rising evenly to
 *  ZTop1 at A1. Side 0 is the A0..A1 edge at S0, side 1 the A1 end, side 2 the S1 edge, side 3 the A0 end. */
void PlanBox(FBucket& B, EMat Mat, EColl Coll, const FVector2D& Origin, const FVector2D& Along, const FVector2D& Across,
    double A0, double A1, double S0, double S1, double ZLow, double ZTop0, double ZTop1, int32 Mask, float AoLow = 1.f, float AoHigh = 1.f)
{
    const FVector2D C[4] = { Origin + Along * A0 + Across * S0, Origin + Along * A1 + Across * S0, Origin + Along * A1 + Across * S1, Origin + Along * A0 + Across * S1 };
    const double Z[4] = { ZTop0, ZTop1, ZTop1, ZTop0 };
    const FVector Low[4] = { Lift(C[0], ZLow), Lift(C[1], ZLow), Lift(C[2], ZLow), Lift(C[3], ZLow) };
    const FVector High[4] = { Lift(C[0], Z[0]), Lift(C[1], Z[1]), Lift(C[2], Z[2]), Lift(C[3], Z[3]) };
    Hexa(B, Mat, Coll, Low, High, Mask, AoLow, AoHigh);
}

/** A round pipe (six sides) from A to C. */
void Pipe(FBucket& B, EMat Mat, EColl Coll, const FVector& A, const FVector& C, float Radius, bool bCapA, bool bCapC)
{
    const FVector Axis = (C - A).GetSafeNormal();
    if (Axis.IsNearlyZero()) return;
    const FVector Ref = FMath::Abs(Axis.Z) < .9 ? FVector::UpVector : FVector::ForwardVector;
    const FVector U = FVector::CrossProduct(Axis, Ref).GetSafeNormal(), W = FVector::CrossProduct(Axis, U);
    constexpr int32 Sides = 6;
    FVector Ring[Sides];
    for (int32 K = 0; K < Sides; ++K)
    {
        const double Ang = 2.0 * UE_DOUBLE_PI * K / Sides;
        Ring[K] = U * FMath::Cos(Ang) + W * FMath::Sin(Ang);
    }
    for (int32 K = 0; K < Sides; ++K)
    {
        const int32 L = (K + 1) % Sides;
        const FVector Q[4] = { A + Ring[K] * Radius, A + Ring[L] * Radius, C + Ring[L] * Radius, C + Ring[K] * Radius };
        const FVector Nrm[4] = { Ring[K], Ring[L], Ring[L], Ring[K] };
        Poly(B, Mat, Coll, Q, 4, Ring[K] + Ring[L], Nrm);
    }
    for (int32 End = 0; End < 2; ++End)
    {
        if (!(End == 0 ? bCapA : bCapC)) continue;
        const FVector& At = End == 0 ? A : C;
        FVector Q[Sides];
        for (int32 K = 0; K < Sides; ++K) Q[K] = At + Ring[K] * Radius;
        Poly(B, Mat, Coll, Q, Sides, End == 0 ? -Axis : Axis);
    }
}

/** A pipe rail: a post standing at every base point (but the first or last when another rail stands there), the top rail
 *  Height over the bases, and a mid rail half way when bMid. Returns the rail's length. */
double Rail(FBucket& B, const TArray<FVector>& Bases, float Height, bool bMid, bool bFirstPost = true, bool bLastPost = true)
{
    if (Bases.Num() < 2) return 0.0;
    double Length = 0.0;
    for (int32 K = 0; K < Bases.Num(); ++K)
    {
        if ((K == 0 && !bFirstPost) || (K + 1 == Bases.Num() && !bLastPost)) continue;   // a post another rail already has
        Pipe(B, MatIron, EColl::Block, Bases[K] - FVector(0, 0, 3), Bases[K] + FVector(0, 0, Height), PostRadius, false, false);
    }
    for (int32 K = 0; K + 1 < Bases.Num(); ++K)
    {
        const bool bFirst = K == 0, bLast = K + 2 == Bases.Num();
        Pipe(B, MatIron, EColl::Block, Bases[K] + FVector(0, 0, Height), Bases[K + 1] + FVector(0, 0, Height), RailRadius, bFirst, bLast);
        if (bMid) Pipe(B, MatIron, EColl::Block, Bases[K] + FVector(0, 0, Height * .5f), Bases[K + 1] + FVector(0, 0, Height * .5f), RailRadius * .8f, bFirst, bLast);
        Length += (Bases[K + 1] - Bases[K]).Size2D();
    }
    return Length;
}

/** Evenly spaced points from S0 to S1, no further apart than Spacing (both ends included). */
TArray<double> Spaced(double S0, double S1, double Spacing)
{
    TArray<double> Out;
    const int32 Gaps = FMath::Max(1, FMath::CeilToInt((S1 - S0) / Spacing));
    for (int32 K = 0; K <= Gaps; ++K) Out.Add(S0 + (S1 - S0) * K / Gaps);
    return Out;
}

// ------------------------------------------------------------------ the stair and ramp plans
struct FStep
{
    double S0 = 0.0, S1 = 0.0;   // along the stair from its foot: the riser and the back of the tread
    float TopZ = 0.f, LowZ = 0.f;
    bool bCheek[2] = { false, false };   // side -1 (index 0) and +1 (index 1) along the plan's Side vector
    float CheekLow[2] = { 0.f, 0.f }, CheekTop[2] = { 0.f, 0.f }, Beside[2] = { 0.f, 0.f };
    double CheekOuter[2] = { 0.0, 0.0 };   // how far out from the axis the cheek reaches (it stops at a wall running alongside)
};

struct FStairPlan
{
    FString Name;
    FVector2D Foot, Head, Dir, Side;
    double Len = 0.0;
    float Width = 0.f, Riser = 0.f, Tread = 0.f, TopTread = 0.f, FootZ = 0.f, HeadZ = 0.f;   // Tread: every step but the top one
    TArray<FStep> Steps;
    TArray<double> Crossings;   // where a declared wall with a real step crosses its axis
    bool bSenior = false;
    FVector2D At(double S, double L) const { return Foot + Dir * S + Side * L; }
    /** The step whose tread lies over S along the stair. */
    int32 StepAt(double S) const { return Tread > 0.f ? FMath::Clamp(FMath::FloorToInt(S / Tread), 0, Steps.Num() - 1) : Steps.Num() - 1; }
};

// Concrete approaches flatten the walking width before the first tread.
// At asphalt their entry curb is 15.6 cm; on paving it is only a small lip.
// Outer corners follow the cross-fall; the inner edge meets the flat tread.
constexpr double LotApproachLength = 150.0;
bool HasLotApproaches(const FStairPlan& S)
{
    return S.Name.StartsWith(TEXT("walk south_lot_tier_links ")) || S.Name == TEXT("walk walk_45 stair")
        || S.Name == TEXT("walk walk_00 stair") || S.Name == TEXT("walk walk_29 stair");
}
double StairApproachLength(const FStairPlan& S, bool bHead)
{
    if (!HasLotApproaches(S)) return 0.0;
    // Music's first nosing is only 19 cm from its facade. Keep this threshold
    // transition outside the building, and leave its upper walk30 junction free.
    if (S.Name == TEXT("walk walk_29 stair")) return bHead ? 0.0 : 12.0;
    return LotApproachLength;
}
void LotApproachCorners(const FStairPlan& S, bool bHead, FVector2D (&XY)[4], float (&Z)[4])
{
    const double Length = StairApproachLength(S, bHead);
    const double A0 = bHead ? S.Len : -Length;
    const double A1 = bHead ? S.Len + Length : 0.0;
    const double Half = S.Width * .5;
    XY[0] = S.At(A0, -Half); XY[1] = S.At(A1, -Half);
    XY[2] = S.At(A1, Half); XY[3] = S.At(A0, Half);
    for (int32 Corner = 0; Corner < 4; ++Corner)
    {
        const bool bInner = bHead ? (Corner == 0 || Corner == 3) : (Corner == 1 || Corner == 2);
        Z[Corner] = bInner ? (bHead ? S.HeadZ : S.FootZ) : GroundAt(XY[Corner]) + 3.f + TreadLift;
        if (S.Name == TEXT("walk walk_29 stair")) Z[Corner] = S.FootZ;
    }
}
bool LotApproachAt(const FStairPlan& S, const FVector2D& P, float& Z)
{
    if (!HasLotApproaches(S)) return false;
    const FVector2D Rel = P - S.Foot;
    const double A = FVector2D::DotProduct(Rel, S.Dir), L = FVector2D::DotProduct(Rel, S.Side);
    if (FMath::Abs(L) > S.Width * .5 || A < -StairApproachLength(S, false) || A > S.Len + StairApproachLength(S, true)
        || (A > 0.0 && A < S.Len)) return false;
    const bool bHead = A >= S.Len;
    const double Length = StairApproachLength(S, bHead);
    if (Length <= 0.0) return false;
    FVector2D XY[4]; float H[4]; LotApproachCorners(S, bHead, XY, H);
    const double U = (A - (bHead ? S.Len : -Length)) / Length;
    const double V = (L + S.Width * .5) / S.Width;
    // Match Poly's actual 0-1-2 / 0-2-3 triangle fan, including the twist.
    Z = U >= V ? H[0] + (H[1] - H[0]) * U + (H[2] - H[1]) * V
        : H[0] + (H[2] - H[3]) * U + (H[3] - H[0]) * V;
    return true;
}

struct FRampPlan
{
    FString Name;
    FVector2D A, B, Dir, Side;
    double Len = 0.0;
    float ZA = 0.f, ZB = 0.f, LowZ = 0.f;
    bool bLandingRail[2] = { true, true }, bEndRail = true;
    /** The landing's footprint in the ramp's frame: along [Len, LandingEndS], across [LandingL0, LandingL1] (its open sides and
     *  end reach LandingReach further, onto the high ground through the wall's opening). */
    double LandingEndS = 0.0, LandingL0 = 0.0, LandingL1 = 0.0;
    FVector2D LandingEnd() const { return B + Dir * RampLanding; }
};

struct FPlans
{
    TArray<FStairPlan> Stairs;
    TArray<FRampPlan> Ramps;
    TArray<FString> Notes;     // logged once by Build
    int32 StairsSkipped = 0, StairWarnings = 0, RampWarnings = 0;
};

/** Risers for a rise over a run of Len: 14-18 cm each and treads (Len / count) of 30 cm or more where an integer count allows
 *  both, nearest 16.7 cm. */
int32 Risers(float Rise, double Len)
{
    const int32 First = FMath::Max(1, FMath::FloorToInt(Rise / 18.f) - 1), Last = FMath::Max(First, FMath::CeilToInt(Rise / 14.f) + 1);
    int32 Best = First;
    double BestKey = DBL_MAX;
    for (int32 Count = First; Count <= Last; ++Count)
    {
        const double R = double(Rise) / Count, T = Len / Count;
        const double Key = FMath::Max3(0.0, 14.0 - R, R - 18.0) * 100.0 + FMath::Max(0.0, 30.0 - T) * 100.0 + FMath::Abs(R - 16.7);
        if (Key < BestKey) { BestKey = Key; Best = Count; }
    }
    return Best;
}

/** A declared wall (with a real step) crossing a stair's axis: how far along the axis, and which wall. */
struct FCrossing { double T = 0.0; int32 Wall = INDEX_NONE; };

/** Where declared walls with a real step (>= MinStep) cross the line Foot + Dir * T, for T in [TMin, TMax], at an angle of more
 *  than 37 degrees (a wall running along a stair is not crossed by it), nearest first. */
TArray<FCrossing> WallCrossings(const FVector2D& Foot, const FVector2D& Dir, double TMin, double TMax)
{
    TArray<FCrossing> Out;
    const TArray<HouseCalderLandform::FWall>& All = HouseCalderLandform::Walls();
    for (int32 WI = 0; WI < All.Num(); ++WI)
    {
        const HouseCalderLandform::FWall& Wl = All[WI];
        const FVector2D WD = Wl.B - Wl.A;
        const double WLen = WD.Size();
        if (WLen < 1.0) continue;
        const FVector2D WU = WD / WLen;
        if (FMath::Abs(FVector2D::DotProduct(WU, Dir)) >= .8) continue;
        const double Den = Cross2(Dir, WD);
        if (FMath::Abs(Den) < 1e-9) continue;
        const FVector2D AF = Wl.A - Foot;
        const double T = Cross2(AF, WD) / Den, U = Cross2(AF, Dir) / Den;
        if (T < TMin || T > TMax || U < 0.0 || U > 1.0) continue;
        const FVector2D X = Foot + Dir * T, WN(-WU.Y, WU.X);
        if (FMath::Abs(GroundAt(X + WN * 3.0) - GroundAt(X - WN * 3.0)) < MinStep) continue;
        Out.Add({ T, WI });
    }
    Out.Sort([](const FCrossing& P, const FCrossing& Q) { return P.T < Q.T; });
    return Out;
}

/** Where the line Foot + Side * L + Dir * t meets wall WI's line (within 60 cm of its ends): OutT. False when they run
 *  parallel or miss. */
bool CrossingAt(const FVector2D& Foot, const FVector2D& Dir, const FVector2D& Side, double L, int32 WI, double& OutT)
{
    const HouseCalderLandform::FWall& Wl = HouseCalderLandform::Walls()[WI];
    const FVector2D WD = Wl.B - Wl.A;
    const double WLen = WD.Size(), Den = Cross2(Dir, WD);
    if (WLen < 1.0 || FMath::Abs(Den) < 1e-9) return false;
    const FVector2D AO = Wl.A - (Foot + Side * L);
    const double U = Cross2(AO, Dir) / Den;
    if (U * WLen < -60.0 || U * WLen > WLen + 60.0) return false;
    OutT = Cross2(AO, WD) / Den;
    return true;
}

/** How far out from a stair's axis (on side Sg) its cheek may reach over [S0, S1] along it: HalfW + CheekWidth, or less where
 *  a declared wall with a real step runs alongside (within 15 degrees of the stair's direction) inside that band, at the
 *  wall's line. */
double SideWallLateral(const FStairPlan& S, double S0, double S1, double Sg, double HalfW)
{
    double Outer = HalfW + CheekWidth;
    const double Mid = (S0 + S1) * .5;
    for (const HouseCalderLandform::FWall& Wl : HouseCalderLandform::Walls())
    {
        const FVector2D WD = Wl.B - Wl.A;
        const double WLen = WD.Size();
        if (WLen < 1.0 || FMath::Abs(FVector2D::DotProduct(WD / WLen, S.Dir)) < .966) continue;
        const double AA = FVector2D::DotProduct(Wl.A - S.Foot, S.Dir), AB = FVector2D::DotProduct(Wl.B - S.Foot, S.Dir);
        if (FMath::Max(AA, AB) < S0 || FMath::Min(AA, AB) > S1 || FMath::Abs(AB - AA) < 1e-6) continue;
        const double T = FMath::Clamp((FMath::Clamp(Mid, FMath::Min(AA, AB), FMath::Max(AA, AB)) - AA) / (AB - AA), 0.0, 1.0);
        const FVector2D On = Wl.A + WD * T;
        const double Lat = Sg * FVector2D::DotProduct(On - S.Foot, S.Side);
        if (Lat <= HalfW - 2.0 || Lat >= Outer + 2.0) continue;
        const FVector2D WN(-WD.Y / WLen, WD.X / WLen);
        if (FMath::Abs(GroundAt(On + WN * 3.0) - GroundAt(On - WN * 3.0)) < MinStep) continue;
        Outer = FMath::Min(Outer, FMath::Max(Lat, HalfW));
    }
    return Outer;
}

void PlanStair(FPlans& Pl, const HouseCalderLandform::FStair& D)
{
    auto Skip = [&Pl, &D](const FString& Why)
    {
        ++Pl.StairsSkipped;
        Pl.Notes.Add(FString::Printf(TEXT("CALDER_FEATURES_STAIR skipped name=\"%s\" reason=%s"), *D.Name, *Why));
    };
    FStairPlan S;
    S.Name = D.Name;
    S.Foot = D.Foot;
    S.Head = D.Head;
    S.Width = D.Width;
    S.Len = (S.Head - S.Foot).Size();
    if (S.Len < 1.0) { Skip(TEXT("zero-length (head and foot at one point; no direction)")); return; }
    S.Dir = (S.Head - S.Foot) / S.Len;
    S.Side = FVector2D(-S.Dir.Y, S.Dir.X);
    const double HalfW = S.Width * .5;
    // An at-wall stair's top tread meets the high ground just past its wall: the head goes 15 cm past the wall it crosses
    // last, carried on where the data leaves it short and drawn back where the data runs it on over level ground (a lower
    // tread straddling the wall would stand under the high ground and leave the step open over it). Near / Far: where that
    // wall crosses the cheeks' and the treads' outer edges (they differ where it runs across the stair at a slant).
    const TArray<FCrossing> Cross = WallCrossings(S.Foot, S.Dir, -30.0, S.Len + 60.0);
    for (const FCrossing& C : Cross) S.Crossings.Add(C.T);
    double Moved = 0.0, Near = 0.0, Far = 0.0;
    if (Cross.Num())
    {
        const FCrossing& LastCross = Cross.Last();
        Near = Far = LastCross.T;
        double Edge = 0.0;
        for (const double L : { -HalfW, HalfW }) if (CrossingAt(S.Foot, S.Dir, S.Side, L, LastCross.Wall, Edge)) Far = FMath::Max(Far, Edge);
        for (const double L : { -HalfW - CheekWidth, HalfW + CheekWidth }) if (CrossingAt(S.Foot, S.Dir, S.Side, L, LastCross.Wall, Edge)) Near = FMath::Min(Near, Edge);
        const double Target = LastCross.T + 15.0;
        const bool bLevel = FMath::Abs(GroundAt(S.At(S.Len, 0.0)) - GroundAt(S.At(Target, 0.0))) <= 8.f;
        if (S.Len < Target || (S.Len > Target + 1.0 && bLevel))
        {
            Moved = Target - S.Len;
            S.Len = Target;
            S.Head = S.At(S.Len, 0.0);
        }
    }
    for (const double A : { 0.0, S.Len })
        for (const double L : { -HalfW, HalfW })
            if (FrozenAt(S.At(A, L))) { Skip(TEXT("in a frozen zone")); return; }
    // The stair joins the walks at its foot and head: the ground there (the lattice is the authority) plus the walk's 3 cm,
    // and 6 mm more, so the top tread (it reaches 15 cm onto the high side) never shares a plane with the walk it meets.
    const bool bLotApproaches = HasLotApproaches(S);
    const float FootDatum = S.Name == TEXT("walk walk_29 stair")
        ? HouseCampusInteriors::InteriorFloorZ(HouseCampusLayout::Find(TEXT("MUS")), 0) + 5.f - TreadLift - GroundAt(S.Foot)
        : bLotApproaches ? 3.f + TreadLift
        : HouseCalderTerrain::PaveOffset(HouseCampusLayout::PaveKindAt(S.Foot));
    const float HeadPave = bLotApproaches ? 3.f
        : HouseCalderTerrain::PaveOffset(HouseCampusLayout::PaveKindAt(S.Head));
    const float HeadDatum = S.Name == TEXT("walk walk_47 flight")
        ? HouseCampusInteriors::InteriorFloorZ(HouseCampusLayout::Find(TEXT("PLT")), 0) - GroundAt(S.Head) : HeadPave;
    // FootZ is a datum, not a visible tread: begin on the actual approach
    // surface. Only the head/tread needs the small anti-overlap lift.
    S.FootZ = GroundAt(S.Foot) + FootDatum;
    if (S.Name == TEXT("walk walk_47 flight"))
    {
        // The curve has a slight cross-fall. Join the lowest actual approach
        // surface across the walking width so its outer first rise stays safe.
        S.FootZ = FLT_MAX;
        for (const double Across : { -(HalfW - 25.0), 0.0, HalfW - 25.0 })
        {
            const FVector2D P = S.At(-3.1, Across);
            S.FootZ = FMath::Min(S.FootZ, HouseCalderTerrain::SurfaceZ(P.X, P.Y));
        }
    }
    S.HeadZ = GroundAt(S.Head) + HeadDatum + TreadLift;
    int32 Count = 1;
    double Top0 = 0.0;
    float HeadLip = 0.f;
    bool bLanding = false;
    for (int32 Pass = 0; Pass < 2; ++Pass)
    {
        const float PassRise = S.HeadZ - S.FootZ;
        if (PassRise < 10.f) { Skip(FString::Printf(TEXT("rise=%.1f (the ground at its head is not above its foot)"), PassRise)); return; }
        Count = Risers(PassRise, S.Len);
        Top0 = S.Len * (Count - 1) / Count;
        if (Cross.Num() && Count > 1 && (Far + 15.0 > S.Len + .5 || Top0 > Near - 2.0))
        {
            // A wall across the stair at a slant: the head goes 15 cm past its furthest crossing of the treads and the top tread
            // starts at or before its nearest crossing of the cheeks (a landing), where the other treads keep 30 cm or more.
            const double LenF = FMath::Max(S.Len, Far + 15.0);
            const double Top0F = FMath::Min(LenF * (Count - 1) / Count, Near - 2.0);
            if (Top0F / (Count - 1) >= 30.0 && FMath::Abs(GroundAt(S.At(LenF, 0.0)) - GroundAt(S.At(S.Len, 0.0))) <= 8.f)
            {
                Moved += LenF - S.Len;
                S.Len = LenF;
                S.Head = S.At(S.Len, 0.0);
                Top0 = Top0F;
                bLanding = Top0F < LenF * (Count - 1) / Count - .5;
            }
        }
        // The top tread stands on (never under) the ground it covers: where the high side falls away from its wall or slopes
        // across the stair (by up to MaxHeadLip), the top tread meets the highest point and the walk steps down from it.
        const float AtHead = GroundAt(S.Head) + HeadDatum + TreadLift;
        float Highest = AtHead;
        for (int32 IA = 0; IA <= 4; ++IA)
            for (int32 IL = 0; IL <= 4; ++IL)
                Highest = FMath::Max(Highest, GroundAt(S.At(FMath::Lerp(Top0, S.Len, IA / 4.0), FMath::Lerp(-(HalfW - 10.0), HalfW - 10.0, IL / 4.0))) + HeadPave + TreadLift);
        HeadLip = Highest - AtHead;
        S.HeadZ = HeadLip <= MaxHeadLip ? Highest : AtHead;
    }
    const float Rise = S.HeadZ - S.FootZ;
    S.Riser = Rise / Count;
    S.Tread = Count > 1 ? float(Top0 / (Count - 1)) : float(S.Len);
    S.TopTread = float(S.Len - Top0);
    if (FMath::Min(S.Tread, S.TopTread) < 25.f) { Skip(FString::Printf(TEXT("tread=%.1f (rise %.0f over %.0f cm: too steep for a stair)"), FMath::Min(S.Tread, S.TopTread), Rise, S.Len)); return; }
    S.bSenior = S.Name.Contains(TEXT("Senior Steps"));
    int32 Pokes = 0;
    float WorstPoke = 0.f;
    for (int32 K = 0; K < Count; ++K)
    {
        FStep& St = S.Steps.AddDefaulted_GetRef();
        St.S0 = K * double(S.Tread);
        St.S1 = K + 1 < Count ? (K + 1) * double(S.Tread) : S.Len;   // the top tread runs on to the head
        St.TopZ = S.FootZ + (K + 1) * S.Riser;
        float Lowest = FLT_MAX;
        for (int32 IA = 0; IA <= 4; ++IA)
            for (int32 IL = 0; IL <= 4; ++IL)
                Lowest = FMath::Min(Lowest, GroundAt(S.At(FMath::Lerp(St.S0 - 3.0, St.S1, IA / 4.0), FMath::Lerp(-HalfW, HalfW, IL / 4.0))));
        St.LowZ = FMath::Min(Lowest, St.TopZ - 4.f) - 30.f;
        for (const double FA : { .2, .5, .8 })
            for (const double L : { -(HalfW - 25.0), 0.0, HalfW - 25.0 })
            {
                const float Over = GroundAt(S.At(FMath::Lerp(St.S0, St.S1, FA), L)) + HeadPave + TreadLift - St.TopZ;
                if (Over > 2.f) { ++Pokes; WorstPoke = FMath::Max(WorstPoke, Over); }
            }
        bool bAtWall = false;
        for (const double C : S.Crossings) bAtWall |= C >= St.S0 - 15.0 && C <= St.S1 + 15.0;
        for (int32 SideIx = 0; SideIx < 2; ++SideIx)
        {
            const double Sg = SideIx == 0 ? -1.0 : 1.0;
            // A wall running alongside within the cheek band (a flight in a walled cut, a notch in a stoop) is the cheek
            // there: the cheek stops at its line, and the ground beside is read only up to it.
            const double Outer = SideWallLateral(S, St.S0, St.S1, Sg, HalfW);
            St.CheekOuter[SideIx] = Outer;
            float MinB = FLT_MAX, MaxB = -FLT_MAX;
            for (const double FA : { .1, .5, .9 })
                for (const double L : { FMath::Min(HalfW + 5.0, Outer - 2.0), Outer - 2.0 })
                {
                    const float G = GroundAt(S.At(FMath::Lerp(St.S0, St.S1, FA), Sg * L));
                    MinB = FMath::Min(MinB, G);
                    MaxB = FMath::Max(MaxB, G);
                }
            St.Beside[SideIx] = MinB;
            St.bCheek[SideIx] = Outer - HalfW >= 5.0 && (St.TopZ - MinB > 30.f || MaxB > St.TopZ + 2.f || bAtWall);
            St.CheekLow[SideIx] = FMath::Min(MinB, St.LowZ + 30.f) - 30.f;
            St.CheekTop[SideIx] = FMath::Max(St.TopZ + 10.f, MaxB + 5.f);
        }
    }
    // Problems the landform should fix (the stair is built as laid out here; the review shows what they cost).
    const float Scale = HouseCalderTerrain::ReliefScale();
    const float DataFoot = BaseAt(D.Foot) + D.FootRelief * Scale + FootDatum, DataHead = BaseAt(D.Head) + D.HeadRelief * Scale + HeadDatum + TreadLift;
    TArray<FString> Warn;
    if (FMath::Abs(Moved) > .5) Warn.Add(FString::Printf(TEXT("headMoved=%.0f"), Moved));   // + carried on, - drawn back
    if (bLanding) Warn.Add(FString::Printf(TEXT("landing=%.0f"), S.TopTread));            // a wall at a slant: a long top tread
    if (FMath::Abs(S.FootZ - DataFoot) > 10.f) Warn.Add(FString::Printf(TEXT("footGround=%.0f data=%.0f"), S.FootZ, DataFoot));
    if (FMath::Abs(Moved) <= .5 && FMath::Abs(S.HeadZ - DataHead) > 10.f) Warn.Add(FString::Printf(TEXT("headGround=%.0f data=%.0f"), S.HeadZ, DataHead));
    if (HeadLip > MaxHeadLip) Warn.Add(FString::Printf(TEXT("headHighSide=%.0f"), HeadLip));   // not raised: the landform must fix it
    else if (HeadLip > 2.f) Warn.Add(FString::Printf(TEXT("headLip=%.0f"), HeadLip));
    if (S.Riser < 14.f || S.Riser > 18.f) Warn.Add(FString::Printf(TEXT("riser=%.1f"), S.Riser));
    if (S.Tread < 30.f || S.TopTread < 30.f) Warn.Add(FString::Printf(TEXT("tread=%.1f"), FMath::Min(S.Tread, S.TopTread)));
    if (Pokes) Warn.Add(FString::Printf(TEXT("groundAboveTreads=%d worst=%.0f"), Pokes, WorstPoke));
    if (Warn.Num()) ++Pl.StairWarnings;
    Pl.Notes.Add(FString::Printf(TEXT("CALDER_FEATURES_STAIR built name=\"%s\" risers=%d riser=%.1f tread=%.1f topTread=%.1f len=%.0f width=%.0f foot=(%.0f,%.0f,%.0f) head=(%.0f,%.0f,%.0f)%s%s"),
        *S.Name, Count, S.Riser, S.Tread, S.TopTread, S.Len, S.Width, S.Foot.X, S.Foot.Y, S.FootZ, S.Head.X, S.Head.Y, S.HeadZ,
        Warn.Num() ? TEXT(" WARN ") : TEXT(""), *FString::Join(Warn, TEXT(" "))));
    Pl.Notes.Add(FString::Printf(TEXT("CALDER_STAIR_SURFACES name=\"%s\" foot=(%.3f,%.3f) footKind=%d head=(%.3f,%.3f) headKind=%d"),
        *S.Name, S.Foot.X, S.Foot.Y, int32(HouseCampusLayout::PaveKindAt(S.Foot)),
        S.Head.X, S.Head.Y, int32(HouseCampusLayout::PaveKindAt(S.Head))));
    Pl.Stairs.Add(MoveTemp(S));
}

void PlanRamp(FPlans& Pl, const HouseCalderLandform::FRamp& D)
{
    FRampPlan R;
    R.Name = D.Name;
    R.A = D.A;
    R.B = D.B;
    R.Len = (R.B - R.A).Size();
    if (R.Len < 1.0) { Pl.Notes.Add(FString::Printf(TEXT("CALDER_FEATURES_RAMP skipped name=\"%s\" reason=zero-length"), *R.Name)); return; }
    R.Dir = (R.B - R.A) / R.Len;
    R.Side = FVector2D(-R.Dir.Y, R.Dir.X);
    const float Scale = HouseCalderTerrain::ReliefScale();
    R.ZA = BaseAt(R.A) + D.Z0 * Scale + 3.f + TreadLift;
    R.ZB = BaseAt(R.B) + D.Z1 * Scale + 3.f + TreadLift;
    const double HalfW = RampWidth * .5;
    float Lowest = FLT_MAX;
    bool bFrozen = false;
    for (const double S : Spaced(0.0, R.Len + RampLanding, 100.0))
        for (const double L : { -HalfW, 0.0, HalfW })
        {
            const FVector2D P = R.A + R.Dir * S + R.Side * L;
            Lowest = FMath::Min(Lowest, GroundAt(P));
            bFrozen |= FrozenAt(P);
        }
    if (bFrozen) { Pl.Notes.Add(FString::Printf(TEXT("CALDER_FEATURES_RAMP skipped name=\"%s\" reason=frozen"), *R.Name)); return; }
    R.LowZ = FMath::Min(Lowest, FMath::Min(R.ZA, R.ZB) - 4.f) - 30.f;
    // The landing's sides and end get a rail only where they stand over a drop (>= 30 cm all along), so its way off onto the
    // high ground (through the wall's opening) stays open.
    for (int32 SideIx = 0; SideIx < 2; ++SideIx)
    {
        float MaxB = -FLT_MAX;
        for (const double S : { 25.0, 75.0, 125.0 }) MaxB = FMath::Max(MaxB, GroundAt(R.B + R.Dir * S + R.Side * ((SideIx == 0 ? -1.0 : 1.0) * (HalfW + 40.0))));
        R.bLandingRail[SideIx] = R.ZB - 3.f - TreadLift - MaxB >= 30.f;
    }
    {
        float MaxB = -FLT_MAX;
        for (const double L : { -60.0, 0.0, 60.0 }) MaxB = FMath::Max(MaxB, GroundAt(R.LandingEnd() + R.Dir * 40.0 + R.Side * L));
        R.bEndRail = R.ZB - 3.f - TreadLift - MaxB >= 30.f;
    }
    // Its open sides and end reach LandingReach on, so where they meet a wall (a ramp runs 10 cm off the wall it climbs) the
    // landing runs 15 cm past the wall line, like a stair's top tread, and closes the wall's opening.
    R.LandingEndS = R.Len + RampLanding + (R.bEndRail ? 0.0 : LandingReach);
    R.LandingL0 = -HalfW - (R.bLandingRail[0] ? 0.0 : LandingReach);
    R.LandingL1 = HalfW + (R.bLandingRail[1] ? 0.0 : LandingReach);
    TArray<FString> Warn;
    const float GA = GroundAt(R.A) + 3.f + TreadLift;
    if (FMath::Abs(GA - R.ZA) > 10.f) Warn.Add(FString::Printf(TEXT("footGround=%.0f slab=%.0f"), GA, R.ZA));
    float Over = -FLT_MAX;
    for (const double S : Spaced(0.0, R.Len, 50.0))
        for (const double L : { -HalfW + 10.0, 0.0, HalfW - 10.0 })
            Over = FMath::Max(Over, GroundAt(R.A + R.Dir * S + R.Side * L) + 3.f + TreadLift - FMath::Lerp(R.ZA, R.ZB, float(S / R.Len)));
    if (Over > 2.f) Warn.Add(FString::Printf(TEXT("groundAboveSlab=%.0f"), Over));
    if (R.bLandingRail[0] && R.bLandingRail[1] && R.bEndRail) Warn.Add(TEXT("landingMeetsNoHighGround"));
    if (Warn.Num()) ++Pl.RampWarnings;
    Pl.Notes.Add(FString::Printf(TEXT("CALDER_FEATURES_RAMP built name=\"%s\" len=%.0f grade=%.1f%% from=%.0f to=%.0f%s%s"), *R.Name, R.Len,
        (R.ZB - R.ZA) / R.Len * 100.0, R.ZA, R.ZB, Warn.Num() ? TEXT(" WARN ") : TEXT(""), *FString::Join(Warn, TEXT(" "))));
    Pl.Ramps.Add(MoveTemp(R));
}

const FPlans& Plans()
{
    static const FPlans Built = []
    {
        FPlans Pl;
        if (!HouseCalderFeatures::Active()) return Pl;
        for (const HouseCalderLandform::FStair& D : HouseCalderLandform::Stairs()) PlanStair(Pl, D);
        for (const HouseCalderLandform::FRamp& D : HouseCalderLandform::Ramps()) PlanRamp(Pl, D);
        return Pl;
    }();
    return Built;
}

/** A wall sample at P along a wall running Dir, its high side at Hi, lies in an opening: only where a stair or a ramp landing
 *  stands on the wall line and reaches the high ground (to OpeningTolerance), so the step under the opening is always closed
 *  by the stair's solid steps, its cheeks or the landing slab:
 *  - a stair: over its treads (the wall must cross it, not run along it), where the step there stands at the high ground; in
 *    the 15 cm margin beside the treads only where that step's cheek stands to the high ground too. A stair that meets a wall
 *    below the high ground (a wall across it at a slant, a high side sloping across it) runs into the wall there instead of
 *    leaving a hole under the high ground;
 *  - a ramp: under its landing's footprint (its open sides and end reach LandingReach past the slab), grown by 2 cm. */
bool InOpening(const FPlans& Pl, const FVector2D& P, const FVector2D& Dir, float Hi)
{
    for (const FStairPlan& S : Pl.Stairs)
    {
        if (FMath::Abs(FVector2D::DotProduct(Dir, S.Dir)) >= .8 || !S.Steps.Num()) continue;
        const FVector2D Rel = P - S.Foot;
        const double Along = FVector2D::DotProduct(Rel, S.Dir), Lat = FVector2D::DotProduct(Rel, S.Side), HalfW = S.Width * .5;
        if (Along < 0.0 || Along > S.Len || FMath::Abs(Lat) > HalfW + 15.0) continue;
        const FStep& St = S.Steps[S.StepAt(Along)];
        if (St.TopZ < Hi - OpeningTolerance) continue;
        if (FMath::Abs(Lat) <= HalfW) return true;
        const int32 SideIx = Lat < 0.0 ? 0 : 1;
        if (St.bCheek[SideIx] && St.CheekTop[SideIx] >= Hi - OpeningTolerance) return true;
    }
    for (const FRampPlan& R : Pl.Ramps)
    {
        if (R.ZB < Hi - OpeningTolerance) continue;
        const FVector2D Rel = P - R.A;
        const double Along = FVector2D::DotProduct(Rel, R.Dir), Lat = FVector2D::DotProduct(Rel, R.Side);
        if (Along >= R.Len - 2.0 && Along <= R.LandingEndS + 2.0 && Lat >= R.LandingL0 - 2.0 && Lat <= R.LandingL1 + 2.0) return true;
    }
    return false;
}

/** P lies on a built stair's solid (its treads with the 3 cm nosing, its cheeks where built) or a ramp's slab or landing. */
bool OnFeature(const FPlans& Pl, const FVector2D& P)
{
    for (const FStairPlan& S : Pl.Stairs)
    {
        const FVector2D Rel = P - S.Foot;
        const double Along = FVector2D::DotProduct(Rel, S.Dir), Lat = FVector2D::DotProduct(Rel, S.Side), HalfW = S.Width * .5;
        if (Along < -3.0 || Along > S.Len || !S.Steps.Num()) continue;
        if (FMath::Abs(Lat) <= HalfW) return true;
        const FStep& St = S.Steps[S.StepAt(Along)];
        const int32 SideIx = Lat < 0.0 ? 0 : 1;
        if (St.bCheek[SideIx] && FMath::Abs(Lat) <= St.CheekOuter[SideIx]) return true;
    }
    for (const FRampPlan& R : Pl.Ramps)
    {
        const FVector2D Rel = P - R.A;
        const double Along = FVector2D::DotProduct(Rel, R.Dir), Lat = FVector2D::DotProduct(Rel, R.Side), HalfW = RampWidth * .5;
        if (Along < 0.0 || Along > R.LandingEndS) continue;
        if (Along <= R.Len ? FMath::Abs(Lat) <= HalfW : (Lat >= R.LandingL0 && Lat <= R.LandingL1)) return true;
    }
    return false;
}

/** P lies within the rock house path's walking band (RockHousePathHalf either side of its line), grown by Pad. */
bool NearRockHousePath(const FVector2D& P, double Pad)
{
    const HouseCalderLandform::FRockHouse& R = HouseCalderLandform::RockHouse();
    const FVector2D AB = R.PathB - R.PathA;
    const double L2 = AB.SizeSquared();
    if (L2 <= 1.0) return false;
    const double T = FMath::Clamp(FVector2D::DotProduct(P - R.PathA, AB) / L2, 0.0, 1.0);
    return FVector2D::Distance(P, R.PathA + AB * T) <= RockHousePathHalf + Pad;
}

/** Paving (a walk, sidewalk, plaza, apron, road or lot of the campus plan) or a stair or ramp at P; bWalksOnly: only what
 *  people walk along (walks, sidewalks, plazas, stairs, ramps), not an entrance apron's landing or a road. */
bool PavedAt(const FPlans& Pl, const FVector2D& P, bool bWalksOnly = false)
{
    const HouseCalderTerrain::EPave K = HouseCampusLayout::PaveKindAt(P);
    if (bWalksOnly ? K == HouseCalderTerrain::EPave::Walk : K != HouseCalderTerrain::EPave::Lawn) return true;
    // The railed path behind Calder Falls is a walk too: the shale face behind it keeps off it as off any paving (its batter
    // and noise had reached 55 cm onto the 140 cm shelf, leaving the walker one 23 cm band to stand in).
    if (NearRockHousePath(P, 0.0)) return true;
    return OnFeature(Pl, P);
}

/** A stair or ramp footprint lies within Pad of P. */
bool FeatureNear(const FPlans& Pl, const FVector2D& P, double Pad)
{
    for (const FStairPlan& S : Pl.Stairs)
    {
        const FVector2D Rel = P - S.Foot;
        const double Along = FVector2D::DotProduct(Rel, S.Dir), Lat = FMath::Abs(FVector2D::DotProduct(Rel, S.Side));
        if (Along >= -3.0 - Pad && Along <= S.Len + Pad && Lat <= S.Width * .5 + CheekWidth + Pad) return true;
    }
    for (const FRampPlan& R : Pl.Ramps)
    {
        const FVector2D Rel = P - R.A;
        const double Along = FVector2D::DotProduct(Rel, R.Dir), Lat = FVector2D::DotProduct(Rel, R.Side);
        if (Along >= -Pad && Along <= R.LandingEndS + Pad && Lat >= FMath::Min(R.LandingL0, -RampWidth * .5) - Pad && Lat <= FMath::Max(R.LandingL1, RampWidth * .5) + Pad) return true;
    }
    return false;
}

/** How far from P along Out the first paving lies (to 2.5 cm, from 1 cm out), or MaxD + 1 when there is none within MaxD.
 *  Probed every 10 cm, then refined. */
float PavedClear(const FPlans& Pl, const FVector2D& P, const FVector2D& Out, float MaxD, bool bWalksOnly = false)
{
    if (!HouseCampusLayout::PavingNear(P, MaxD + 1.f) && !FeatureNear(Pl, P, MaxD + 1.0) && !NearRockHousePath(P, MaxD + 1.0)) return MaxD + 1.f;
    float Prev = 1.f - 2.5f;
    for (float D = 1.f; D <= MaxD + 9.99f; D += 10.f)
    {
        const float At = FMath::Min(D, MaxD);
        if (PavedAt(Pl, P + Out * At, bWalksOnly))
        {
            for (float F = Prev + 2.5f; F < At; F += 2.5f) if (PavedAt(Pl, P + Out * F, bWalksOnly)) return F;
            return At;
        }
        Prev = At;
        if (At >= MaxD) break;
    }
    return MaxD + 1.f;
}

// ------------------------------------------------------------------ walls
struct FStats
{
    int32 Walls = 0, Runs = 0, RockRuns = 0, Stairs = 0, Steps = 0, Cheeks = 0, Ramps = 0, Rails = 0, Plaques = 0;
    int32 LowSideRuns = 0, TightRuns = 0, RockHeldBack = 0;   // masonry with its body on the low side; with a thin body; rock faces held off paving
    int32 CutRails = 0;                                        // pipe rails along road and walk cuts with paving on top
    double WallCm = 0.0, RailCm = 0.0;
};

/** One sample across a wall line: the ground each side (3 cm off it), and its class: +1 / -1 for a real step whose high side
 *  lies along +Nl / -Nl, 0 where nothing is built (the step under MinStep, the wall's body in a frozen zone, an opening). */
struct FWallSample { double S = 0.0; FVector2D P; float Lo = 0.f, Hi = 0.f; int32 Cls = 0; };

bool IsRock(EWallKind K) { return K == EWallKind::RockBand || K == EWallKind::RockCut || K == EWallKind::RockLedge; }
EMat WallMat(EWallKind K)
{
    switch (K)
    {
    case EWallKind::Reserve: return MatField;     // the family cemetery's low fieldstone wall
    case EWallKind::RockCut: return MatCut;       // fresh road and portal cuts: little moss
    case EWallKind::RockBand: case EWallKind::RockLedge: return MatRock;
    default: return MatWall;                      // 1930s mortared stone
    }
}

/** Where a wall's run ends at a corner it shares with another built wall, both are mitred: for an offset U into the high
 *  side, how far past the corner (along the run, outward) the face at that offset runs, so the two walls meet on one plane.
 *  Out: the run's direction toward the corner; NhOwn its high side; Other the other wall's direction away from the corner
 *  and NhOther its high side. */
double Miter(const FVector2D& Out, const FVector2D& NhOwn, const FVector2D& Other, const FVector2D& NhOther, double U)
{
    const double Den = Cross2(Out, Other);
    if (FMath::Abs(Den) < .17) return 0.0;   // nearly straight on: the ends simply meet
    const double T = Cross2((NhOther - NhOwn) * U, Other) / Den;
    return FMath::Clamp(T, -120.0, 120.0);
}

/** The other wall that shares the corner End (within 2 cm) and has a real step there: its direction away from the corner and
 *  its high side. False when there is none (or only one running straight on). */
bool CornerPartner(int32 Self, const FVector2D& End, const FVector2D& OutDir, FVector2D& OtherDir, FVector2D& OtherNh)
{
    const TArray<HouseCalderLandform::FWall>& All = HouseCalderLandform::Walls();
    double BestCross = .17;
    bool bFound = false;
    for (int32 J = 0; J < All.Num(); ++J)
    {
        if (J == Self) continue;
        const FVector2D Span = All[J].B - All[J].A;
        const double Len = Span.Size();
        if (Len < 1.0) continue;
        FVector2D Away;
        if ((All[J].A - End).Size() < 2.0) Away = Span / Len;
        else if ((All[J].B - End).Size() < 2.0) Away = -Span / Len;
        else continue;
        const double Cr = FMath::Abs(Cross2(OutDir, Away));
        if (Cr <= BestCross) continue;
        const FVector2D Probe = End + Away * FMath::Min(30.0, Len * .5), Nl(-Span.Y / Len, Span.X / Len);
        const float ZL = GroundAt(Probe + Nl * 3.0), ZR = GroundAt(Probe - Nl * 3.0);
        if (FMath::Abs(ZL - ZR) < MinStep) continue;
        BestCross = Cr;
        OtherDir = Away;
        OtherNh = ZL >= ZR ? Nl : -Nl;
        bFound = true;
    }
    return bFound;
}

/** A masonry run: S range [Sa, Sb] of one wall, a level top, a stone body and a concrete coping. The body spans U0..U1 across
 *  the line (U along the high side's normal: 0..40 on the high side as a rule, -40..0 on the low side where paving lies
 *  along the high side), and the coping overhangs it by OvLow on the low side and OvHigh on the high side. */
struct FMasonryRun
{
    double Sa, Sb; float TopZ, LowZ; bool bParapet; bool bMiterA, bMiterB; FVector2D OtherA, OtherNhA, OtherB, OtherNhB;
    double U0 = 0.0, U1 = WallThick, OvLow = CopingOverhang, OvHigh = 0.0;
};

void EmitMasonry(FMeshes& M, const FVector2D& Origin, const FVector2D& Dir, const FVector2D& Nh, EMat Mat, const FMasonryRun& Run)
{
    const double OvL = Run.OvLow, OvH = Run.OvHigh;
    const float CopThick = Run.bParapet ? 10.f : 12.f;
    const float CopLow = Run.TopZ - CopThick;
    FMasonryRun R = Run;
    auto SAt = [&R, &Dir, &Nh](bool bEndB, double U) -> double
    {
        if (!bEndB) return R.Sa - (R.bMiterA ? Miter(-Dir, Nh, R.OtherA, R.OtherNhA, U) : 0.0);
        return R.Sb + (R.bMiterB ? Miter(Dir, Nh, R.OtherB, R.OtherNhB, U) : 0.0);
    };
    // A run shorter than its mitres would cross itself: square ends there instead.
    for (const double U : { R.U0 - OvL, R.U0, R.U1, R.U1 + OvH })
        if (SAt(true, U) - SAt(false, U) < 1.0) { R.bMiterA = R.bMiterB = false; break; }
    auto Pt = [&](bool bEndB, double U) { return Origin + Dir * SAt(bEndB, U) + Nh * U; };
    FBucket& B = M.At(ECat::Wall, Origin + Dir * ((R.Sa + R.Sb) * .5));
    {
        // The body: U0..U1 across the line; the top lies under the coping.
        const FVector2D C[4] = { Pt(false, R.U0), Pt(true, R.U0), Pt(true, R.U1), Pt(false, R.U1) };
        const FVector Low[4] = { Lift(C[0], R.LowZ), Lift(C[1], R.LowZ), Lift(C[2], R.LowZ), Lift(C[3], R.LowZ) };
        const FVector High[4] = { Lift(C[0], CopLow), Lift(C[1], CopLow), Lift(C[2], CopLow), Lift(C[3], CopLow) };
        Hexa(B, Mat, EColl::Block, Low, High, SidesOnly, .5f, 1.f);
    }
    {
        const FVector2D C[4] = { Pt(false, R.U0 - OvL), Pt(true, R.U0 - OvL), Pt(true, R.U1 + OvH), Pt(false, R.U1 + OvH) };
        const FVector Low[4] = { Lift(C[0], CopLow), Lift(C[1], CopLow), Lift(C[2], CopLow), Lift(C[3], CopLow) };
        const FVector High[4] = { Lift(C[0], R.TopZ), Lift(C[1], R.TopZ), Lift(C[2], R.TopZ), Lift(C[3], R.TopZ) };
        Hexa(B, MatCoping, EColl::Walk, Low, High, AllFaces, .8f, 1.f);
    }
}

/** Where a masonry run's body and coping go, so nothing stands inside the paving beside it (a walk sample's capsule reaches
 *  5 cm past the paved edge): the body on the high side (U 0..40) unless paving lies within its reach there, then on the low
 *  side (U -40..0) if that side has room (never into a frozen zone); with no room either side, on the roomier side, thinned
 *  to fit (20 cm at least). No coping overhang reaches within 5 cm of paving, nor over a frozen low side. */
void PlaceBody(const FPlans& Pl, const FVector2D& Origin, const FVector2D& Dir, const FVector2D& Nh, FMasonryRun& R, bool bLowFrozen, FStats& St)
{
    // (Clear distances are probed to 2.5 cm. A walk sample's capsule reaches 5 cm past a walk's side and 20 cm past its end
    // (the run-on's sample stands 10 cm in), so the body keeps 22 cm off paving; copings 9.)
    constexpr float Margin = 9.f, BodyMargin = 22.f;
    constexpr float Reach = WallThick + CopingOverhang + BodyMargin + 3.f;
    float HiClear = FLT_MAX, LoClear = FLT_MAX, HiWalk = FLT_MAX, LoWalk = FLT_MAX;
    // (Sampled along the run, but 45 cm in from the ends of a longer one: an end beside an opening has the stair or ramp
    // through it right there, which the body does not reach.)
    const double EndIn = R.Sb - R.Sa > 120.0 ? 45.0 : 5.0;
    const TArray<double> Ss = R.Sb - R.Sa > 20.0 ? Spaced(R.Sa + EndIn, R.Sb - EndIn, 75.0) : TArray<double>{ (R.Sa + R.Sb) * .5 };
    for (const double S : Ss)
    {
        const FVector2D P = Origin + Dir * S;
        const float Hc = PavedClear(Pl, P, Nh, Reach), Lc = PavedClear(Pl, P, -Nh, Reach);
        HiClear = FMath::Min(HiClear, Hc);
        LoClear = FMath::Min(LoClear, Lc);
        HiWalk = FMath::Min(HiWalk, Hc < Reach ? PavedClear(Pl, P, Nh, Reach, true) : Hc);
        LoWalk = FMath::Min(LoWalk, Lc < Reach ? PavedClear(Pl, P, -Nh, Reach, true) : Lc);
    }
    const double Need = WallThick + BodyMargin;
    bool bLow = false;
    double Thick = WallThick;
    if (HiClear < Need)
    {
        // Paving along the high side: the body goes to the low side if it has the room (over an entrance landing's or a
        // road's edge if need be, never into a walk's width), else back on the high side over a landing's edge.
        if (!bLowFrozen && LoClear >= Need) bLow = true;
        else if (!bLowFrozen && LoWalk >= Need) bLow = true;
        else if (HiWalk >= Need) bLow = false;
        else
        {
            bLow = !bLowFrozen && LoWalk > HiWalk;
            Thick = FMath::Max(20.0, double(bLow ? LoWalk : HiWalk) - BodyMargin);
            Thick = FMath::Min(Thick, double(WallThick));
            ++St.TightRuns;
        }
    }
    if (bLow)
    {
        R.U0 = -Thick; R.U1 = 0.0;
        R.OvLow = LoClear >= Thick + CopingOverhang + Margin ? CopingOverhang : 0.0;
        R.OvHigh = 0.0;
        ++St.LowSideRuns;
    }
    else
    {
        R.U0 = 0.0; R.U1 = Thick;
        R.OvLow = !bLowFrozen && LoClear >= CopingOverhang + Margin ? CopingOverhang : 0.0;
        R.OvHigh = R.bParapet && HiClear >= Thick + CopingOverhang + Margin ? CopingOverhang : 0.0;
    }
    // A mitred end reaches past the corner (to meet the other wall's face); where that would put it on or by paving (the
    // other wall's body went to its other side), or inside a frozen zone (the other wall's coping keeps off its frozen low side,
    // so the mitre would reach 10 cm past it: the front lawn's corners at (-18700,-5845) and (-5330,-5900); Calder 1d/1e
    // verification), the end stays square.
    for (int32 End = 0; End < 2; ++End)
    {
        bool& bMiter = End == 0 ? R.bMiterA : R.bMiterB;
        if (!bMiter) continue;
        for (const double U : { R.U0 - R.OvLow, R.U1 + R.OvHigh })
        {
            const double Ext = End == 0 ? Miter(-Dir, Nh, R.OtherA, R.OtherNhA, U) : Miter(Dir, Nh, R.OtherB, R.OtherNhB, U);
            if (Ext <= 0.0) continue;
            const FVector2D Corner = Origin + Dir * (End == 0 ? R.Sa - Ext : R.Sb + Ext) + Nh * U;
            bool bNear = false;
            for (const FVector2D& O : { FVector2D(0, 0), FVector2D(Margin, 0), FVector2D(-Margin, 0), FVector2D(0, Margin), FVector2D(0, -Margin) }) bNear |= PavedAt(Pl, Corner + O);
            if (bNear || InsideFrozen(Corner)) { bMiter = false; break; }
        }
    }
}

/** A rock run: the samples I0..I1 of one wall. A grid face from 40 cm under the low ground to a lip 2 cm over the high
 *  ground, leaning back 8 degrees (its foot out on the low side, capped) and displaced by value noise, never behind the wall
 *  line; a rock top back to 40 cm into the high side. Returns the number of quads. */
int32 EmitRock(FMeshes& M, const FPlans& Pl, const TArray<FWallSample>& Smp, int32 I0, int32 I1, const FVector2D& Dir, const FVector2D& Nh, EWallKind Kind, int32 WallIndex, FStats& St)
{
    const int32 Cols = I1 - I0 + 1;
    if (Cols < 2) return 0;
    const EMat Mat = WallMat(Kind);
    const double BatterCap = Kind == EWallKind::RockBand ? 600.0 : 45.0;   // a road or portal cut keeps its foot on its verge
    const float TopLift = RockLip + .3f * (WallIndex % 4);                   // rock tops meeting at a corner never share a plane
    float MaxH = 0.f;
    for (int32 C = I0; C <= I1; ++C) MaxH = FMath::Max(MaxH, Smp[C].Hi - Smp[C].Lo);
    const int32 Rows = FMath::Clamp(FMath::CeilToInt((MaxH + FootDepth + RockLip) / 150.f), 2, 30);
    const FVector Nh3(Nh.X, Nh.Y, 0.0);
    TArray<FVector> V;
    TArray<float> Ao;
    V.SetNum(Cols * (Rows + 1));
    Ao.SetNum(Cols * (Rows + 1));
    for (int32 C = 0; C < Cols; ++C)
    {
        const FWallSample& W = Smp[I0 + C];
        const float H = FMath::Max(W.Hi - W.Lo, 1.f);
        const bool bLowFrozen = FrozenAt(W.P - Nh * 30.0);
        double Proj = bLowFrozen ? 0.0 : FMath::Min(double(H) * RockBatterTan, BatterCap);
        // The face never reaches within 5 cm of paving, a stair or a ramp on the low side (a walk in a walled cut, a flight's
        // treads): its noise and batter shrink to what the room there allows (a plain face on the line where there is none).
        const float Probe = float(2.0 * RockAmp + 1.3 * Proj + 24.0);
        // (A walk sample's capsule reaches 20 cm past a walk's end: the face keeps 22 cm off paving. With less room than its
        // relief needs, the face is let back into the high side by up to 12 cm, under the turf's edge.)
        const double Avail = FMath::Max(0.0, double(PavedClear(Pl, W.P, -Nh, Probe)) - 22.0);
        const double Amp = FMath::Min(double(RockAmp), (Avail + RockRecess) * .5);
        const double Shift = FMath::Clamp(2.0 * Amp - Avail, 0.0, double(RockRecess));
        if (Amp < RockAmp || Shift > 0.0 || Proj * 1.3 > Avail - 2.0 * Amp + Shift) ++St.RockHeldBack;
        Proj = FMath::Min(Proj, FMath::Max(0.0, (Avail - 2.0 * Amp + Shift) / 1.3));
        for (int32 R = 0; R <= Rows; ++R)
        {
            const float Z = FMath::Lerp(W.Lo - FootDepth, W.Hi + TopLift, float(R) / Rows);
            const FVector Base = Lift(W.P, Z);
            const float N = RockNoise(Base);
            // u along Nh: <= 0, the face on or in front of the wall line, so the high ground's edge is always covered.
            const double Batter = Proj * FMath::Clamp((W.Hi - Z) / H, 0.f, 1.3f);
            const double U = (R == Rows ? -(Amp * .5 + Amp * .5 * N) : -(Amp + Amp * N)) + Shift - Batter;
            V[C * (Rows + 1) + R] = Base + Nh3 * U;
            const float Height01 = FMath::Clamp((Z - W.Lo) / 180.f, 0.f, 1.f);
            Ao[C * (Rows + 1) + R] = (.5f + .5f * Height01) * (.78f + .22f * (N * .5f + .5f));
        }
    }
    auto Vx = [&V, Rows, Cols](int32 C, int32 R) -> const FVector& { return V[FMath::Clamp(C, 0, Cols - 1) * (Rows + 1) + FMath::Clamp(R, 0, Rows)]; };
    TArray<FVector> Nrm;
    Nrm.SetNum(V.Num());
    for (int32 C = 0; C < Cols; ++C)
        for (int32 R = 0; R <= Rows; ++R)
        {
            FVector N = FVector::CrossProduct(Vx(C + 1, R) - Vx(C - 1, R), Vx(C, R + 1) - Vx(C, R - 1)).GetSafeNormal();
            if (FVector::DotProduct(N, -Nh3) < 0.0) N = -N;
            Nrm[C * (Rows + 1) + R] = N.IsNearlyZero() ? -Nh3 : N;
        }
    int32 Quads = 0;
    for (int32 C = 0; C + 1 < Cols; ++C)
    {
        const FWallSample& W0 = Smp[I0 + C];
        const FWallSample& W1 = Smp[I0 + C + 1];
        if (W1.S - W0.S < .5) continue;
        FBucket& B = M.At(ECat::Wall, (W0.P + W1.P) * .5);
        for (int32 R = 0; R < Rows; ++R)
        {
            const int32 K00 = C * (Rows + 1) + R, K10 = (C + 1) * (Rows + 1) + R;
            const FVector Q[4] = { V[K00], V[K10], V[K10 + 1], V[K00 + 1] };
            const FVector N4[4] = { Nrm[K00], Nrm[K10], Nrm[K10 + 1], Nrm[K00 + 1] };
            const float A4[4] = { Ao[K00], Ao[K10], Ao[K10 + 1], Ao[K00 + 1] };
            Poly(B, Mat, EColl::Block, Q, 4, -Nh3 + FVector(0, 0, .05), N4, A4);
            ++Quads;
        }
        // The rock top: from the lip back 40 cm into the high side (walkable, like a coping).
        const int32 T0 = C * (Rows + 1) + Rows, T1 = (C + 1) * (Rows + 1) + Rows;
        const FVector Q[4] = { V[T0], V[T1], Lift(W1.P + Nh * WallThick, W1.Hi + TopLift), Lift(W0.P + Nh * WallThick, W0.Hi + TopLift) };
        Poly(B, Mat, EColl::Walk, Q, 4, FVector::UpVector, nullptr, nullptr, 1.f);
    }
    // End caps: the face's edge column back to the rock's back 40 cm in.
    for (int32 End = 0; End < 2; ++End)
    {
        const int32 C = End == 0 ? 0 : Cols - 1;
        const FWallSample& W = Smp[I0 + C];
        FBucket& B = M.At(ECat::Wall, W.P);
        const FVector2D Back = W.P + Nh * WallThick;
        const float CapLow = W.Lo - FootDepth;
        const float CapHigh = W.Hi + TopLift;
        for (int32 R = 0; R < Rows; ++R)
        {
            const float Z0 = FMath::Lerp(CapLow, CapHigh, float(R) / Rows), Z1 = FMath::Lerp(CapLow, CapHigh, float(R + 1) / Rows);
            const FVector Q[4] = { Vx(C, R), Vx(C, R + 1), Lift(Back, Z1), Lift(Back, Z0) };
            Poly(B, Mat, EColl::Block, Q, 4, FVector(End == 0 ? -Dir : Dir, 0.0), nullptr, nullptr, .7f);
        }
    }
    return Quads;
}

void BuildWalls(FMeshes& M, const FPlans& Pl, FStats& St)
{
    const TArray<HouseCalderLandform::FWall>& All = HouseCalderLandform::Walls();
    const double HalfSpan = HouseCalderTerrain::Half, Cell = HouseCalderTerrain::Cell;
    for (int32 WI = 0; WI < All.Num(); ++WI)
    {
        const HouseCalderLandform::FWall& Wl = All[WI];
        if (Wl.Kind == EWallKind::Dam && HouseCalderWater::BuildsDamWalls()) continue;   // the stone dam is built whole by HouseCalderWater
        if (Wl.Kind == EWallKind::RoadAbutment && HouseCalderWater::BuildsRoadBridgeWalls()) continue;
        const FVector2D Span = Wl.B - Wl.A;
        const double Len = Span.Size();
        if (Len < 1.0) continue;
        const FVector2D Dir = Span / Len, Nl(-Dir.Y, Dir.X);
        auto Probe = [&](double S)
        {
            FWallSample W;
            W.S = S;
            W.P = Wl.A + Dir * S;
            const float ZL = GroundAt(W.P + Nl * 3.0), ZR = GroundAt(W.P - Nl * 3.0);
            W.Lo = FMath::Min(ZL, ZR);
            W.Hi = FMath::Max(ZL, ZR);
            const int32 Sign = ZL >= ZR ? 1 : -1;
            W.Cls = W.Hi - W.Lo >= MinStep && !FrozenAt(W.P + Nl * (Sign * WallThick * .5)) && !InOpening(Pl, W.P, Dir, W.Hi) ? Sign : 0;
            return W;
        };
        // Samples at <= 1 m and at every lattice line the wall crosses (the ground is linear between them on each side).
        TArray<double> Ss = { 0.0, Len };
        const int32 Uniform = FMath::CeilToInt(Len / WallSampleStep);
        for (int32 K = 1; K < Uniform; ++K) Ss.Add(Len * K / Uniform);
        auto Lines = [&Ss, Len, Cell](double V0, double V1, double Origin)
        {
            if (FMath::Abs(V1 - V0) < 1e-6) return;
            const double Lo = FMath::Min(V0, V1), Hi = FMath::Max(V0, V1);
            for (double V = Origin + FMath::CeilToDouble((Lo - Origin) / Cell) * Cell; V < Hi; V += Cell)
            {
                const double T = (V - V0) / (V1 - V0);
                if (T > 1e-6 && T < 1.0 - 1e-6) Ss.Add(T * Len);
            }
        };
        Lines(Wl.A.X, Wl.B.X, -HalfSpan);
        Lines(Wl.A.Y, Wl.B.Y, -HalfSpan);
        Ss.Sort();
        TArray<FWallSample> Raw;
        for (int32 K = 0; K < Ss.Num(); ++K)
            if (K == 0 || Ss[K] - Ss[K - 1] > .05) Raw.Add(Probe(Ss[K]));
        // Where the class changes between two samples, find the change to 4 mm from each side, so a run stops exactly at an
        // opening, a frozen edge or the end of its step.
        auto Edge = [&Probe](const FWallSample& From, const FWallSample& To)
        {
            double Keep = From.S, Lose = To.S;
            for (int32 It = 0; It < 8; ++It)
            {
                const double Mid = (Keep + Lose) * .5;
                if (Probe(Mid).Cls == From.Cls) Keep = Mid; else Lose = Mid;
            }
            return Probe(Keep);
        };
        // Changes are looked for on a 20 cm pitch too: a stretch of another class shorter than the 1 m samples (a corner pocket,
        // an opening's edge) would otherwise be lost, and its step left open. Only the pinned changes join the samples (the rock
        // faces keep their 1 m columns).
        {
            const int32 Fine = FMath::Max(1, FMath::CeilToInt(Len / FineSampleStep));
            FWallSample Prev = Probe(0.0);
            for (int32 K = 1; K <= Fine; ++K)
            {
                const FWallSample Next = Probe(Len * K / Fine);
                if (Next.Cls != Prev.Cls)
                {
                    if (Prev.Cls != 0) Raw.Add(Edge(Prev, Next));
                    if (Next.Cls != 0) Raw.Add(Edge(Next, Prev));
                }
                Prev = Next;
            }
            Raw.StableSort([](const FWallSample& P0, const FWallSample& P1) { return P0.S < P1.S; });
        }
        TArray<FWallSample> Smp;
        for (int32 K = 0; K < Raw.Num(); ++K)
        {
            Smp.Add(Raw[K]);
            if (K + 1 == Raw.Num() || Raw[K].Cls == Raw[K + 1].Cls) continue;
            if (Raw[K].Cls != 0) Smp.Add(Edge(Raw[K], Raw[K + 1]));
            if (Raw[K + 1].Cls != 0) Smp.Add(Edge(Raw[K + 1], Raw[K]));
        }
        const bool bRock = IsRock(Wl.Kind);
        const bool bParapetKind = !bRock;
        const float ParapetH = Wl.Kind == EWallKind::Parapet ? OverlookParapetRise : ParapetRise;
        const bool bLedgeRail = Wl.Kind == EWallKind::RockLedge && Wl.Name.Contains(TEXT("Crow"));
        // A road or walk cut whose high side is paving (a plaza's or walk's edge over the cut, past a flight's head) gets a pipe
        // rail on the rock's top wherever the drop is 75 cm or more (MASTER_PROMPT: rails at drops beside a walk).
        const bool bCutRail = (Wl.Kind == EWallKind::RockCut || (Wl.Kind == EWallKind::RockLedge && !bLedgeRail)) && !Wl.Name.Contains(TEXT("Crow"));
        bool bAny = false;
        int32 K = 0;
        while (K + 1 < Smp.Num())
        {
            if (Smp[K].Cls == 0 || Smp[K + 1].Cls != Smp[K].Cls) { ++K; continue; }
            int32 E = K + 1;
            while (E + 1 < Smp.Num() && Smp[E + 1].Cls == Smp[K].Cls) ++E;
            const int32 Cls = Smp[K].Cls;
            const FVector2D Nh = Nl * double(Cls);
            if (Smp[E].S - Smp[K].S >= .5)
            {
                bAny = true;
                St.WallCm += Smp[E].S - Smp[K].S;
                if (bRock)
                {
                    EmitRock(M, Pl, Smp, K, E, Dir, Nh, Wl.Kind, WI, St);
                    ++St.RockRuns;
                    if (bCutRail)
                    {
                        auto Owed = [&](int32 I)
                        {
                            if (FMath::Max(Smp[I].Hi - Smp[I].Lo, Smp[I + 1].Hi - Smp[I + 1].Lo) < ParapetDrop) return false;
                            return PavedClear(Pl, Smp[I].P, Nh, CutRailReach) <= CutRailReach || PavedClear(Pl, Smp[I + 1].P, Nh, CutRailReach) <= CutRailReach;
                        };
                        int32 R0 = K;
                        while (R0 < E)
                        {
                            if (!Owed(R0)) { ++R0; continue; }
                            int32 R1 = R0 + 1;
                            while (R1 < E && Owed(R1)) ++R1;
                            TArray<FVector> Bases;
                            for (const double S : Spaced(Smp[R0].S, Smp[R1].S, PostSpacing))
                            {
                                const FVector2D P = Wl.A + Dir * S;
                                Bases.Add(Lift(P + Nh * CutRailInset, GroundAt(P + Nh * 3.0) + RockLip));
                            }
                            FBucket& B = M.At(ECat::Wall, Wl.A + Dir * ((Smp[R0].S + Smp[R1].S) * .5));
                            St.RailCm += Rail(B, Bases, LedgeRail, true);
                            ++St.Rails;
                            ++St.CutRails;
                            R0 = R1;
                        }
                    }
                    if (bLedgeRail)
                    {
                        // Crow Ledge: a pipe rail on the high side, 30 cm in, wherever the drop is 75 cm or more.
                        int32 R0 = K;
                        while (R0 < E)
                        {
                            if (FMath::Max(Smp[R0].Hi - Smp[R0].Lo, Smp[R0 + 1].Hi - Smp[R0 + 1].Lo) < ParapetDrop) { ++R0; continue; }
                            int32 R1 = R0 + 1;
                            while (R1 < E && FMath::Max(Smp[R1].Hi - Smp[R1].Lo, Smp[R1 + 1].Hi - Smp[R1 + 1].Lo) >= ParapetDrop) ++R1;
                            TArray<FVector> Bases;
                            for (const double S : Spaced(Smp[R0].S, Smp[R1].S, PostSpacing))
                            {
                                const FVector2D P = Wl.A + Dir * S;
                                Bases.Add(Lift(P + Nh * 30.0, GroundAt(P + Nh * 30.0) + RockLip));
                            }
                            FBucket& B = M.At(ECat::Wall, Wl.A + Dir * ((Smp[R0].S + Smp[R1].S) * .5));
                            St.RailCm += Rail(B, Bases, LedgeRail, true);
                            ++St.Rails;
                            R0 = R1;
                        }
                    }
                }
                else
                {
                    // Pieces with a drop of 75 cm or more get the parapet; the rest a coping 8 cm over the high side. Each type
                    // splits into level runs (the coping steps where the high ground changes by more than 20 cm, or every 8 m).
                    int32 P0 = K;
                    while (P0 < E)
                    {
                        auto PieceParapet = [&](int32 I) { return bParapetKind && FMath::Max(Smp[I].Hi - Smp[I].Lo, Smp[I + 1].Hi - Smp[I + 1].Lo) >= ParapetDrop; };
                        const bool bPar = PieceParapet(P0);
                        int32 P1 = P0 + 1;
                        while (P1 < E && PieceParapet(P1) == bPar) ++P1;
                        int32 J0 = P0;
                        while (J0 < P1)
                        {
                            float HiMin = FMath::Min(Smp[J0].Hi, Smp[J0 + 1].Hi), HiMax = FMath::Max(Smp[J0].Hi, Smp[J0 + 1].Hi);
                            int32 J1 = J0 + 1;
                            while (J1 < P1)
                            {
                                const float NMin = FMath::Min(HiMin, Smp[J1 + 1].Hi), NMax = FMath::Max(HiMax, Smp[J1 + 1].Hi);
                                if (NMax - NMin > 20.f || Smp[J1 + 1].S - Smp[J0].S > 800.0) break;
                                HiMin = NMin; HiMax = NMax;
                                ++J1;
                            }
                            FMasonryRun R;
                            R.Sa = Smp[J0].S;
                            R.Sb = Smp[J1].S;
                            R.bParapet = bPar;
                            float LoMin = FLT_MAX;
                            for (int32 I = J0; I <= J1; ++I) LoMin = FMath::Min(LoMin, Smp[I].Lo);
                            // Coping tops never share a plane where two walls meet (a few mm per wall index).
                            R.TopZ = HiMax + (bPar ? ParapetH : CopingRise) + .4f * (WI % 4);
                            R.LowZ = LoMin - FootDepth;
                            if (Wl.Kind == EWallKind::Parapet)
                            {
                                // Founders' Overlook: where the wall's high side is only a ledge before a drop (the strip on
                                // the bluff's edge), the walking side is the low one, and the parapet stands 107 cm over it.
                                float LoMax = -FLT_MAX;
                                bool bEdge = false;
                                for (int32 I = J0; I <= J1; ++I)
                                {
                                    LoMax = FMath::Max(LoMax, Smp[I].Lo);
                                    bEdge |= GroundAt(Smp[I].P + Nh * 100.0) < Smp[I].Hi - ParapetDrop;
                                }
                                const float EdgeTop = LoMax + OverlookParapetRise + .4f * (WI % 4);
                                if (bEdge && EdgeTop > R.TopZ) { R.TopZ = EdgeTop; R.bParapet = true; }
                            }
                            R.bMiterA = R.Sa < .5 && CornerPartner(WI, Wl.A, -Dir, R.OtherA, R.OtherNhA);
                            R.bMiterB = R.Sb > Len - .5 && CornerPartner(WI, Wl.B, Dir, R.OtherB, R.OtherNhB);
                            if (R.Sb - R.Sa >= .5)
                            {
                                const FVector2D Mid = Wl.A + Dir * ((R.Sa + R.Sb) * .5);
                                PlaceBody(Pl, Wl.A, Dir, Nh, R, FrozenAt(Mid - Nh * 5.0), St);
                                EmitMasonry(M, Wl.A, Dir, Nh, WallMat(Wl.Kind), R);
                                ++St.Runs;
                            }
                            J0 = J1;
                        }
                        P0 = P1;
                    }
                }
            }
            K = E;
        }
        St.Walls += bAny ? 1 : 0;
    }
}

// ------------------------------------------------------------------ stairs and ramps
void EmitStair(FMeshes& M, const FStairPlan& S, FStats& St)
{
    FBucket& B = M.At(ECat::Stair, (S.Foot + S.Head) * .5);
    const double HalfW = S.Width * .5;
    if (HasLotApproaches(S))
        for (const bool bHead : { false, true })
        {
            if (StairApproachLength(S, bHead) <= 0.0) continue;
            FVector2D XY[4]; float Z[4]; LotApproachCorners(S, bHead, XY, Z);
            FVector Low[4], High[4];
            for (int32 Corner = 0; Corner < 4; ++Corner)
            {
                Low[Corner] = Lift(XY[Corner], GroundAt(XY[Corner]) - 30.f);
                High[Corner] = Lift(XY[Corner], Z[Corner]);
            }
            Hexa(B, MatStep, EColl::Walk, Low, High, AllFaces, .7f, 1.f);
        }
    for (const FStep& Sp : S.Steps)
    {
        // The step: a solid block to 30 cm under the ground beneath it, and its tread slab with a 3 cm nosing.
        PlanBox(B, MatStep, EColl::Walk, S.Foot, S.Dir, S.Side, Sp.S0, Sp.S1, -HalfW, HalfW, Sp.LowZ, Sp.TopZ - 4.f, Sp.TopZ - 4.f, SidesOnly, .7f, 1.f);
        PlanBox(B, MatStep, EColl::Walk, S.Foot, S.Dir, S.Side, Sp.S0 - 3.0, Sp.S1, -HalfW, HalfW, Sp.TopZ - 4.f, Sp.TopZ, Sp.TopZ, AllFaces, .95f, 1.f);
        for (int32 SideIx = 0; SideIx < 2; ++SideIx)
        {
            if (!Sp.bCheek[SideIx]) continue;
            const double L0 = SideIx == 0 ? -Sp.CheekOuter[0] : HalfW, L1 = SideIx == 0 ? -HalfW : Sp.CheekOuter[1];
            PlanBox(B, MatWall, EColl::Walk, S.Foot, S.Dir, S.Side, Sp.S0, Sp.S1, L0, L1, Sp.CheekLow[SideIx], Sp.CheekTop[SideIx], Sp.CheekTop[SideIx], SidesAndTop, .55f, 1.f);
            ++St.Cheeks;
        }
        ++St.Steps;
    }
    // Pipe handrails both sides from 4 risers: posts on the treads every 150 cm or less, the rail 90 cm over the nosings.
    const int32 Count = S.Steps.Num();
    if (Count >= 4)
    {
        const int32 Every = FMath::Max(1, FMath::FloorToInt(PostSpacing / S.Tread));
        TArray<int32> PostSteps;
        for (int32 K = 0; K < Count - 1; K += Every) PostSteps.Add(K);
        PostSteps.Add(Count - 1);
        for (int32 SideIx = 0; SideIx < 2; ++SideIx)
        {
            const double L = (SideIx == 0 ? -1.0 : 1.0) * (HalfW - 6.0);
            TArray<FVector> Bases;
            for (const int32 K : PostSteps) Bases.Add(Lift(S.At((S.Steps[K].S0 + S.Steps[K].S1) * .5, L), S.Steps[K].TopZ));
            const float Height = StairHandrail + S.Riser * .5f;   // over the tread at a post = 90 cm over the nosing line there
            St.RailCm += Rail(B, Bases, Height, false);
            // ... and on level, 25 cm past the top post.
            const FVector RailEnd = Bases.Last() + FVector(0, 0, Height);
            const double Past = FMath::Min(25.0, S.Len - (S.Steps.Last().S0 + S.Steps.Last().S1) * .5 - 5.0);
            if (Past > 2.0) Pipe(B, MatIron, EColl::Block, RailEnd, RailEnd + FVector(S.Dir * Past, 0.0), RailRadius, false, true);
            ++St.Rails;
        }
    }
    // The Senior Steps: a dark bronze CLASS OF 1936 plate on the north cheek (its outer face), where the cheek stands tallest
    // near the middle of the flight.
    if (S.bSenior)
    {
        const int32 North = S.Side.Y >= 0.0 ? 1 : 0;
        const double Sg = North == 1 ? 1.0 : -1.0;
        int32 Best = INDEX_NONE;
        double BestScore = -1.0;
        float BestTop = 0.f, BestLow = 0.f;
        for (int32 K = 0; K < Count; ++K)
        {
            // The plate spans the step's middle +- 28 cm: every cheek under it must stand, and it keeps under the lowest of
            // their stepped tops and over the highest ground beside them.
            const double PlateMid = (S.Steps[K].S0 + S.Steps[K].S1) * .5;
            float Top = FLT_MAX, Low = -FLT_MAX;
            bool bAll = true;
            for (const FStep& Sj : S.Steps)
            {
                if (Sj.S1 <= PlateMid - 28.0 || Sj.S0 >= PlateMid + 28.0) continue;
                bAll &= Sj.bCheek[North];
                Top = FMath::Min(Top, Sj.CheekTop[North]);
                Low = FMath::Max(Low, Sj.Beside[North]);
            }
            const float Exposed = bAll ? Top - Low : 0.f;
            if (Exposed < 70.f) continue;
            // ... clear of the wall the stair passes through (the wall's body continues beside the cheek there), as near the
            // middle of the flight as it can be.
            bool bNearWall = false;
            for (const double C : S.Crossings) bNearWall |= FMath::Abs(C - PlateMid) < 28.0 + WallThick + 30.0;
            if (bNearWall) continue;
            const double Score = 1000.0 - FMath::Abs(K - (Count - 1) * .5) * 100.0 + Exposed * .01;
            if (Score > BestScore) { BestScore = Score; Best = K; BestTop = Top; BestLow = Low; }
        }
        if (Best != INDEX_NONE)
        {
            const FStep& Sp = S.Steps[Best];
            const double Mid = (Sp.S0 + Sp.S1) * .5, Face = Sg * Sp.CheekOuter[North];
            const float ZC = FMath::Max(BestTop - 32.f, BestLow + 30.f);
            // The plate (56 x 38 cm, 2.5 cm proud) and its raised field (48 x 30 cm, 1 cm more).
            const double In = FMath::Min(Face, Face + Sg * 2.5), Outer = FMath::Max(Face, Face + Sg * 2.5);
            PlanBox(B, MatBronze, EColl::None, S.Foot, S.Dir, S.Side, Mid - 28.0, Mid + 28.0, In, Outer, ZC - 19.f, ZC + 19.f, ZC + 19.f, AllFaces);
            const double In2 = FMath::Min(Face + Sg * 2.5, Face + Sg * 3.5), Outer2 = FMath::Max(Face + Sg * 2.5, Face + Sg * 3.5);
            PlanBox(B, MatBronze, EColl::None, S.Foot, S.Dir, S.Side, Mid - 24.0, Mid + 24.0, In2, Outer2, ZC - 15.f, ZC + 15.f, ZC + 15.f, AllFaces);
            ++St.Plaques;
        }
    }
    ++St.Stairs;
}

void EmitRamp(FMeshes& M, const FRampPlan& R, FStats& St)
{
    FBucket& B = M.At(ECat::Ramp, (R.A + R.B) * .5);
    const double HalfW = RampWidth * .5;
    // The slab, its top rising evenly from A to B, and its level landing; the sides down to 30 cm under the ground.
    PlanBox(B, MatStep, EColl::Walk, R.A, R.Dir, R.Side, 0.0, R.Len, -HalfW, HalfW, R.LowZ, R.ZA, R.ZB, SidesAndTop, .7f, 1.f);
    PlanBox(B, MatStep, EColl::Walk, R.A, R.Dir, R.Side, R.Len, R.LandingEndS, R.LandingL0, R.LandingL1, R.LowZ, R.ZB, R.ZB, SidesAndTop, .7f, 1.f);
    auto SurfaceAt = [&R](double S) { return S >= R.Len ? R.ZB : FMath::Lerp(R.ZA, R.ZB, float(S / R.Len)); };
    for (int32 SideIx = 0; SideIx < 2; ++SideIx)
    {
        const double L = (SideIx == 0 ? -1.0 : 1.0) * (HalfW - 6.0);
        const double SEnd = R.bLandingRail[SideIx] ? R.Len + RampLanding - 6.0 : R.Len;
        TArray<double> Ss = Spaced(6.0, R.Len, PostSpacing);
        if (SEnd > R.Len) for (const double S : Spaced(R.Len, SEnd, PostSpacing)) if (S > R.Len + 1.0) Ss.Add(S);
        TArray<FVector> Bases;
        for (const double S : Ss) Bases.Add(Lift(R.A + R.Dir * S + R.Side * L, SurfaceAt(S)));
        St.RailCm += Rail(B, Bases, RampHandrail, false);
        ++St.Rails;
    }
    if (R.bEndRail)
    {
        TArray<FVector> Bases;
        for (const double L : Spaced(-(HalfW - 6.0), HalfW - 6.0, PostSpacing)) Bases.Add(Lift(R.LandingEnd() - R.Dir * 6.0 + R.Side * L, R.ZB));
        St.RailCm += Rail(B, Bases, RampHandrail, false, !R.bLandingRail[0], !R.bLandingRail[1]);
        ++St.Rails;
    }
    ++St.Ramps;
}

// ------------------------------------------------------------------ meshes, materials and components
// (The masters are looked up on every build, not cached in statics: a raw pointer kept across worlds would dangle once the
// garbage collector unloads an asset nothing references between two loads of the map.)
UMaterialInterface* RockMaterial(UObject* Outer, const TCHAR* Id, float Tile, float Moss, const FLinearColor& Tint)
{
    UMaterialInterface* const Base = HouseCalderLandscape::RockMaster(Id);
    if (!Base) return nullptr;
    UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Outer);
    for (const TCHAR* Kind : { TEXT("A"), TEXT("N"), TEXT("ARM") })
    {
        const FString Path = FString::Printf(TEXT("/Game/SeniorSendOff/Calder/Textures/%s/T_%s_%s.T_%s_%s"), Id, Id, Kind, Id, Kind);
        if (UTexture* Tex = LoadObject<UTexture>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
            Mid->SetTextureParameterValue(FCString::Strcmp(Kind, TEXT("A")) == 0 ? FName(TEXT("Albedo")) : (FCString::Strcmp(Kind, TEXT("N")) == 0 ? FName(TEXT("Normal")) : FName(TEXT("ARM"))), Tex);
        else UE_LOG(LogTemp, Warning, TEXT("CALDER_FEATURES texture %s is missing"), *Path);
    }
    Mid->SetScalarParameterValue(TEXT("TileSize"), Tile);
    Mid->SetScalarParameterValue(TEXT("MossAmount"), Moss);
    Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
    HouseCalderLandscape::RockLook(Mid, Id);
    return Mid;
}

UMaterialInterface* PlainMaterial(UObject* Outer, const FLinearColor& Tint, float Rough, float Metal)
{
    UMaterialInterface* const Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/Materials/M_House_Plain.M_House_Plain"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Base) return nullptr;
    UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Outer);
    Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
    Mid->SetScalarParameterValue(TEXT("Roughness"), Rough);
    Mid->SetScalarParameterValue(TEXT("Metallic"), Metal);
    Mid->SetVectorParameterValue(TEXT("Emission"), FLinearColor::Black);
    return Mid;
}

void Materials(UObject* Outer, UMaterialInterface* (&Out)[MatCount])
{
    if (!LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderRock.M_CalderRock"), nullptr, LOAD_NoWarn | LOAD_Quiet))
        UE_LOG(LogTemp, Warning, TEXT("CALDER_FEATURES M_CalderRock is missing; walls and stairs fall back to the default material"));
    Out[MatWall] = RockMaterial(Outer, TEXT("rock_wall_15"), 200.f, .3f, FLinearColor(.95f, .93f, .9f));
    Out[MatField] = RockMaterial(Outer, TEXT("stacked_stone_wall"), 180.f, .5f, FLinearColor(.9f, .9f, .88f));
    Out[MatRock] = RockMaterial(Outer, TEXT("rock_face_04"), 420.f, .5f, FLinearColor(.88f, .86f, .82f));
    Out[MatCut] = RockMaterial(Outer, TEXT("rock_face_04"), 380.f, .12f, FLinearColor(.95f, .9f, .84f));
    Out[MatCoping] = RockMaterial(Outer, TEXT("cracked_concrete"), 160.f, .3f, FLinearColor(.86f, .85f, .82f));
    Out[MatStep] = RockMaterial(Outer, TEXT("cracked_concrete"), 150.f, .08f, FLinearColor(.84f, .83f, .8f));
    Out[MatIron] = PlainMaterial(Outer, FLinearColor(.045f, .045f, .05f), .55f, .6f);      // painted pipe, near black
    Out[MatBronze] = PlainMaterial(Outer, FLinearColor(.11f, .075f, .04f), .42f, .9f);    // dark weathered bronze
}

UStaticMesh* BuildMesh(const FBucket& B, UMaterialInterface* const (&Mats)[MatCount], const FString& Name, int32& OutTris)
{
    OutTris = 0;
    FMeshDescription Description;
    FStaticMeshAttributes Attributes(Description);
    Attributes.Register();
    auto Positions = Attributes.GetVertexPositions();
    auto Normals = Attributes.GetVertexInstanceNormals();
    auto Tangents = Attributes.GetVertexInstanceTangents();
    auto Signs = Attributes.GetVertexInstanceBinormalSigns();
    auto Colors = Attributes.GetVertexInstanceColors();
    auto UVs = Attributes.GetVertexInstanceUVs();
    UVs.SetNumChannels(1);
    auto Slots = Attributes.GetPolygonGroupMaterialSlotNames();
    TArray<FStaticMaterial> Used;
    for (int32 MI = 0; MI < MatCount; ++MI)
    {
        const FDrawPart& D = B.Draw[MI];
        if (D.I.Num() < 3) continue;
        const FPolygonGroupID Group = Description.CreatePolygonGroup();
        Slots[Group] = MatSlots[MI];
        Used.Add(FStaticMaterial(Mats[MI], MatSlots[MI]));   // (the imported slot name defaults to it; a third argument is the overlay material outside the editor)
        TArray<FVertexInstanceID> Instances;
        Instances.Reserve(D.V.Num());
        for (int32 K = 0; K < D.V.Num(); ++K)
        {
            const FVertexID Vertex = Description.CreateVertex();
            Positions[Vertex] = FVector3f(D.V[K]);
            const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
            FVector Tangent = FVector(1, 0, 0) - D.N[K] * D.N[K].X;
            if (Tangent.SizeSquared() < 1e-4) Tangent = FVector(0, 1, 0) - D.N[K] * D.N[K].Y;
            Normals[Instance] = FVector3f(D.N[K]);
            Tangents[Instance] = FVector3f(Tangent.GetSafeNormal());
            Signs[Instance] = 1.f;
            Colors[Instance] = FVector4f(1.f, 1.f, 1.f, D.Ao[K]);
            UVs.Set(Instance, 0, FVector2f(float((D.V[K].X + D.V[K].Y) / 100.0), float(D.V[K].Z / 100.0)));
            Instances.Add(Instance);
        }
        for (int32 K = 0; K + 2 < D.I.Num(); K += 3)
            Description.CreatePolygon(Group, TArray<FVertexInstanceID, TInlineAllocator<3>>{ Instances[D.I[K]], Instances[D.I[K + 1]], Instances[D.I[K + 2]] });
        OutTris += D.I.Num() / 3;
    }
    if (!Used.Num()) return nullptr;
    UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UStaticMesh::StaticClass(), FName(*Name)), RF_Transient);
    Mesh->NeverStream = true;
    Mesh->bAllowCPUAccess = false;
    for (const FStaticMaterial& Mat : Used) Mesh->GetStaticMaterials().Add(Mat);
    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = false;
    Params.bFastBuild = true;
    Params.bAllowCpuAccess = false;
    Params.bMarkPackageDirty = false;
    Params.bCommitMeshDescription = false;
    return Mesh->BuildFromMeshDescriptions({ &Description }, Params) ? Mesh : nullptr;
}

/** A hidden procedural copy for collision (BlockAll, the camera ignores it), complex as simple, cooked now. */
void AddCollision(AActor* Owner, const FCollPart& C, const FString& Name, bool bUnwalkable)
{
    if (C.I.Num() < 3) return;
    UProceduralMeshComponent* Col = NewObject<UProceduralMeshComponent>(Owner, *Name);
    Col->SetMobility(EComponentMobility::Static);
    Col->bUseComplexAsSimpleCollision = true;
    Col->bUseAsyncCooking = false;
    Col->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    Col->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    if (bUnwalkable) Col->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.f));
    Col->SetCastShadow(false);
    Col->bAffectDistanceFieldLighting = false;
    Col->SetVisibility(false);
    Col->SetHiddenInGame(true);
    Col->RegisterComponent();
    Col->CreateMeshSection_LinearColor(0, C.V, C.I, TArray<FVector>(), TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
}
}

bool HouseCalderFeatures::Active()
{
    static const bool bOn = HouseCalderTerrain::Enabled() && HouseCalderTerrain::ReliefScale() > 0.f
        && !HouseCalderTerrain::ReliefFlat(FBox2D(FVector2D(-HouseCalderTerrain::Half, -HouseCalderTerrain::Half), FVector2D(HouseCalderTerrain::Half, HouseCalderTerrain::Half)));
    return bOn;
}

int32 HouseCalderFeatures::Build(AActor* Owner)
{
    namespace FI = HouseCalderFeaturesImpl;
    if (!Owner || !Owner->GetWorld()) return 0;
    if (!Active())
    {
        UE_LOG(LogTemp, Display, TEXT("CALDER_FEATURES off (the Calder terrain is off or its relief is flat)"));
        return 0;
    }
    const double Start = FPlatformTime::Seconds();
    const FI::FPlans& Pl = FI::Plans();
    for (const FString& Note : Pl.Notes) UE_LOG(LogTemp, Display, TEXT("%s"), *Note);
    FI::FMeshes Meshes;
    FI::FStats St;
    FI::BuildWalls(Meshes, Pl, St);
    for (const FI::FStairPlan& S : Pl.Stairs) FI::EmitStair(Meshes, S, St);
    for (const FI::FRampPlan& R : Pl.Ramps) FI::EmitRamp(Meshes, R, St);

    const bool bDraw = Owner->GetWorld()->GetNetMode() != NM_DedicatedServer;
    UMaterialInterface* Mats[FI::MatCount] = {};
    if (bDraw) FI::Materials(Owner, Mats);
    int32 Drawn = 0, Parts = 0, CollTris = 0;
    for (int32 Cat = 0; Cat < FI::NumCats; ++Cat)
        for (int32 Tile = 0; Tile < FI::TilesPerSide * FI::TilesPerSide; ++Tile)
        {
            const FI::FBucket& B = Meshes.Buckets[Cat * FI::TilesPerSide * FI::TilesPerSide + Tile];
            const FString Prefix = FString::Printf(TEXT("%s_%02d"), FI::CatNames[Cat], Tile);
            FI::AddCollision(Owner, B.Walk, Prefix + TEXT("_Collision"), false);
            FI::AddCollision(Owner, B.Block, Prefix + TEXT("_CollisionUnwalkable"), true);
            CollTris += (B.Walk.I.Num() + B.Block.I.Num()) / 3;
            if (!bDraw) continue;
            int32 Tris = 0;
            UStaticMesh* Mesh = FI::BuildMesh(B, Mats, Prefix, Tris);
            if (!Mesh) continue;
            UStaticMeshComponent* Draw = NewObject<UStaticMeshComponent>(Owner, *Prefix);
            Draw->SetMobility(EComponentMobility::Static);
            Draw->SetStaticMesh(Mesh);
            Draw->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Draw->SetCastShadow(false);
            Draw->bAffectDistanceFieldLighting = false;
            Draw->RegisterComponent();
            Drawn += Tris;
            ++Parts;
        }
    UE_LOG(LogTemp, Display, TEXT("CALDER_FEATURES walls=%d wallMeters=%.0f stairs=%d steps=%d ramps=%d rails=%d tris=%d ms=%.0f wallRuns=%d rockRuns=%d cheeks=%d railMeters=%.0f plaques=%d meshes=%d collisionTris=%d stairsSkipped=%d stairWarnings=%d rampWarnings=%d lowSideRuns=%d tightRuns=%d rockHeldBack=%d cutRails=%d"),
        St.Walls, St.WallCm / 100.0, St.Stairs, St.Steps, St.Ramps, St.Rails, Drawn, (FPlatformTime::Seconds() - Start) * 1000.0,
        St.Runs, St.RockRuns, St.Cheeks, St.RailCm / 100.0, St.Plaques, Parts, CollTris, Pl.StairsSkipped, Pl.StairWarnings, Pl.RampWarnings,
        St.LowSideRuns, St.TightRuns, St.RockHeldBack, St.CutRails);
    return Drawn;
}

bool HouseCalderFeatures::InStair(const FVector2D& P, float Pad)
{
    if (!Active()) return false;
    for (const HouseCalderFeaturesImpl::FStairPlan& S : HouseCalderFeaturesImpl::Plans().Stairs)
    {
        const FVector2D Rel = P - S.Foot;
        const double Along = FVector2D::DotProduct(Rel, S.Dir), Lat = FMath::Abs(FVector2D::DotProduct(Rel, S.Side));
        if (Along >= -FMath::Max(3.0, HouseCalderFeaturesImpl::StairApproachLength(S, false)) - Pad
            && Along <= S.Len + HouseCalderFeaturesImpl::StairApproachLength(S, true) + Pad
            && Lat <= S.Width * .5 + HouseCalderFeaturesImpl::CheekWidth + Pad) return true;
    }
    return false;
}

void HouseCalderFeatures::StairSamples(TArray<FVector>& Out)
{
    if (!Active()) return;
    namespace FI = HouseCalderFeaturesImpl;
    for (const FI::FStairPlan& S : FI::Plans().Stairs)
    {
        for (const FI::FStep& Sp : S.Steps)
        {
            const double Mid = (Sp.S0 + Sp.S1) * .5;
            for (const double L : { 0.0, -(S.Width * .5 - 25.0), S.Width * .5 - 25.0 }) Out.Add(FI::Lift(S.At(Mid, L), Sp.TopZ));
        }
        if (FI::HasLotApproaches(S))
            for (const double Along : { -FI::StairApproachLength(S, false) * .6, S.Len + FI::StairApproachLength(S, true) * .6 })
                for (const double Across : { -50.0, 0.0, 50.0 })
                {
                    const FVector2D P = S.At(Along, Across);
                    float Z = 0.f;
                    if (FI::LotApproachAt(S, P, Z)) Out.Add(FI::Lift(P, Z));
                }
    }
}

bool HouseCalderFeatures::InRamp(const FVector2D& P, float Pad)
{
    if (!Active()) return false;
    for (const HouseCalderFeaturesImpl::FRampPlan& R : HouseCalderFeaturesImpl::Plans().Ramps)
    {
        const FVector2D Rel = P - R.A;
        const double Along = FVector2D::DotProduct(Rel, R.Dir), Lat = FVector2D::DotProduct(Rel, R.Side), HalfW = HouseCalderFeaturesImpl::RampWidth * .5;
        if (Along < -Pad || Along > R.LandingEndS + Pad) continue;
        const bool bLanding = Along >= R.Len;
        if (Lat >= (bLanding ? R.LandingL0 : -HalfW) - Pad && Lat <= (bLanding ? R.LandingL1 : HalfW) + Pad) return true;
    }
    return false;
}

void HouseCalderFeatures::RampSamples(TArray<FVector>& Out)
{
    if (!Active()) return;
    namespace FI = HouseCalderFeaturesImpl;
    for (const FI::FRampPlan& R : FI::Plans().Ramps)
        for (const double S : FI::Spaced(10.0, R.Len + FI::RampLanding - 10.0, 100.0))
        {
            const float Z = S >= R.Len ? R.ZB : FMath::Lerp(R.ZA, R.ZB, float(S / R.Len));
            for (const double L : { 0.0, -(FI::RampWidth * .5 - 25.0), FI::RampWidth * .5 - 25.0 }) Out.Add(FI::Lift(R.A + R.Dir * S + R.Side * L, Z));
        }
}

const TArray<TArray<FVector2D>>& HouseCalderFeatures::Footprints()
{
    static const TArray<TArray<FVector2D>> Out = []
    {
        namespace FI = HouseCalderFeaturesImpl;
        TArray<TArray<FVector2D>> Polys;
        if (!Active()) return Polys;
        auto Box = [&Polys](const FVector2D& O, const FVector2D& Dir, const FVector2D& Side, double A0, double A1, double L0, double L1)
        {
            if (A1 - A0 < .5 || L1 - L0 < .5) return;
            // (Dir, Side) is a right-handed frame (Side = Dir turned +90 degrees), so this order runs anticlockwise.
            Polys.Add({ O + Dir * A0 + Side * L0, O + Dir * A1 + Side * L0, O + Dir * A1 + Side * L1, O + Dir * A0 + Side * L1 });
        };
        for (const FI::FStairPlan& S : FI::Plans().Stairs)
        {
            const double HalfW = S.Width * .5;
            Box(S.Foot, S.Dir, S.Side, -FMath::Max(3.0, FI::StairApproachLength(S, false)), S.Len + FI::StairApproachLength(S, true), -HalfW, HalfW);
            // The cheek band only where a cheek wall stands: at a stair's foot on level paving there is none, and cutting the
            // paving there left holes beside walk_45's foot in the draped Ashby lot (2026-10-15).
            for (int32 SideIx = 0; SideIx < 2; ++SideIx)
            {
                int32 K = 0;
                while (K < S.Steps.Num())
                {
                    if (!S.Steps[K].bCheek[SideIx]) { ++K; continue; }
                    int32 E = K;
                    double Outer = S.Steps[K].CheekOuter[SideIx];
                    while (E + 1 < S.Steps.Num() && S.Steps[E + 1].bCheek[SideIx]) { ++E; Outer = FMath::Max(Outer, double(S.Steps[E].CheekOuter[SideIx])); }
                    const double A0 = S.Steps[K].S0 - (K == 0 ? 3.0 : 0.0), A1 = S.Steps[E].S1;
                    if (SideIx == 0) Box(S.Foot, S.Dir, S.Side, A0, A1, -Outer, -HalfW);
                    else Box(S.Foot, S.Dir, S.Side, A0, A1, HalfW, Outer);
                    K = E + 1;
                }
            }
        }
        for (const FI::FRampPlan& R : FI::Plans().Ramps)
        {
            const double Half = FI::RampWidth * .5;
            Box(R.A, R.Dir, R.Side, 0.0, R.Len, -Half, Half);
            Box(R.A, R.Dir, R.Side, R.Len, R.LandingEndS, R.LandingL0, R.LandingL1);
        }
        return Polys;
    }();
    return Out;
}

bool HouseCalderFeatures::FloorAt(const FVector2D& P, float& OutZ)
{
    if (!Active()) return false;
    namespace FI = HouseCalderFeaturesImpl;
    for (const FI::FStairPlan& S : FI::Plans().Stairs)
    {
        const FVector2D Rel = P - S.Foot;
        const double Along = FVector2D::DotProduct(Rel, S.Dir), Lat = FVector2D::DotProduct(Rel, S.Side);
        if (Along < -3.0 || Along > S.Len || FMath::Abs(Lat) > S.Width * .5 || !S.Steps.Num())
        {
            if (FI::LotApproachAt(S, P, OutZ)) return true;
            continue;
        }
        // The step under P, or the next one up where P lies on that step's 3 cm nosing.
        OutZ = FMath::Max(S.Steps[S.StepAt(Along)].TopZ, S.Steps[S.StepAt(FMath::Min(Along + 3.0, S.Len))].TopZ);
        return true;
    }
    for (const FI::FRampPlan& R : FI::Plans().Ramps)
    {
        const FVector2D Rel = P - R.A;
        const double Along = FVector2D::DotProduct(Rel, R.Dir), Lat = FVector2D::DotProduct(Rel, R.Side);
        if (Along < 0.0 || Along > R.LandingEndS) continue;
        if (Along <= R.Len ? FMath::Abs(Lat) > FI::RampWidth * .5 : (Lat < R.LandingL0 || Lat > R.LandingL1)) continue;
        OutZ = Along >= R.Len ? R.ZB : FMath::Lerp(R.ZA, R.ZB, float(Along / R.Len));
        return true;
    }
    return false;
}
