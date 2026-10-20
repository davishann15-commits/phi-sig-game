// The Calder landform's roadside: guardrails, rails, ditches, catch basins and culvert headwalls (see HouseCalderRoadside.h).
#include "HouseCalderRoadside.h"
#include "HouseBlackHallMesh.h"
#include "HouseCalderDrape.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderFootPoints.h"
#include "HouseCalderLandform.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderTerrainTiles.h"
#include "HouseCampusLayout.h"
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
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"

namespace HouseCalderRoadsideImpl
{
using HouseCalderLandform::EWallKind;
using HouseCalderTerrain::EPave;

constexpr int32 TilesPerSide = int32(2 * HouseCalderTerrain::Half / HouseCalderTerrainTiles::TileSize);   // 5, as the terrain
constexpr int32 NumTiles = TilesPerSide * TilesPerSide;
constexpr float MinStep = 3.f;               // a declared wall's step under this is left to the ground (as HouseCalderFeatures)

// The 1986 W-beam guardrail: a 12-gauge beam on 6 x 8 in timber posts and blockouts, with turned-down ends.
constexpr float BeamTop = 64.f;              // the beam's top over the verge (an older 25 in mounting)
constexpr float BeamHeight = 31.1f, BeamDepth = 8.3f, BeamSheet = .35f;
constexpr float PostSpacing = 190.5f, PanelLength = 381.f, TerminalLength = 762.f, TerminalTwistDeg = 72.f;
constexpr float PostAlong = 15.f, PostAcross = 20.f, PostBury = 45.f, BlockAcross = 20.f, BlockHalfHeight = 18.f;
constexpr int32 SamplesPerPanel = 8, SamplesPerPost = 4;   // the beam is sampled every 47.6 cm; a post every 4th sample
constexpr float FlatBeamDeg = 50.f;          // where a turned-down end lies flatter than this (its face within 40 deg of up), it is walkable
constexpr float WalkableNz = .766f;          // cos 40 deg: a face steeper than this joins the Unwalkable collision
constexpr double SteepCollisionNz = .766;  // cos 40 deg: an up-facing collision triangle steeper than this is Unwalkable
// Calder Dr north's fill side (landform.py CALDER_N: centreline X 9500, its paving with the sidewalk to X 8450, an 80 cm
// verge to X 8370; west of it the ground falls from about Y 20400 to Y 24000, by up to 53 cm 3 m out): the beam's face
// 35 cm off the sidewalk, its turned-down ends reaching Y 20600 and Y 24300.
constexpr float LookFaceX = 8415.f, LookY0 = 20600.f, LookY1 = 24300.f;
// Pipe rails on a "needs rail" drop: posts every 1.5 m 25 cm out over the drop, rails at 100, 66 and 32 cm.
constexpr float PipeRailSpacing = 150.f, PipeRailRadius = 2.4f, PipePostRadius = 3.f, PipeRailOffset = 25.f;
constexpr float PipeRailHeights[] = { 100.f, 66.f, 32.f };
// Ditches.
constexpr float LiningLift = 1.2f, RoadLiningHalf = 45.f, WalkLiningHalf = 30.f, MinLining = 24.f, MaxSegment = 210.f;
constexpr float CurbW = 30.f;                // the campus plan's curb lies on the lawn side of a road or lot edge (HouseCampusLayout)
constexpr float RockBatterTan = .14054083f, RockAmp = 12.f;   // how a rock face's foot stands out (HouseCalderFeatures)
constexpr float WeepSpacing = 250.f, WeepRadius = 4.f, WeepMinStep = 70.f, WeepLift = 30.f, WeepRow = 150.f;
// Catch basins: a 115 x 80 cm concrete frame round an 80 x 48 cm grate; a gutter grate 100 x 50 in a 112 x 62 frame, its
// centre 34 cm in from the carriageway's edge; a ditch's drop inlet a 44 cm grate (along the ditch) in a 60 cm frame.
constexpr float BasinFrameA = 57.5f, BasinFrameB = 40.f, BasinGrateA = 40.f, BasinGrateB = 24.f;
constexpr float GutterFrameA = 56.f, GutterFrameB = 31.f, GutterGrateA = 50.f, GutterGrateB = 25.f, GutterOffset = 34.f;
constexpr float FrameLift = .35f, VoidLift = .7f, IronLift = 1.05f, BasinSearch = 120.f, BasinMargin = 10.f;
// Culverts.
constexpr float HeadwallThick = 35.f, InletPipe = 45.f, OutletPipe = 45.f, OutletHalfWidth = 105.f, ApronLength = 240.f;
constexpr float OutletRiseMax = 35.f;        // the ground ahead of an outlet may rise no more than this over the apron
// Draw distances: the small things (grates, bolts, rust streaks, weep holes) and the rest.
constexpr float FarCull = 30000.f, NearCull = 7000.f;

enum EMat : uint8 { MatGalv, MatGalvOld, MatRust, MatTimber, MatConcrete, MatStone, MatRipRap, MatBoulder, MatIron, MatVoid, MatStain, MatCount };
const TCHAR* const MatSlots[MatCount] = { TEXT("Galvanised"), TEXT("GalvanisedOld"), TEXT("Rust"), TEXT("Timber"), TEXT("Concrete"), TEXT("Stone"),
    TEXT("RipRap"), TEXT("Boulder"), TEXT("CastIron"), TEXT("Void"), TEXT("Stain") };
/** Which collision a face joins: none, the walkable mesh (caps, anchors, rock tops, the flat end of a turned-down beam) or the
 *  Unwalkable one (posts, the beam, rails, headwall faces and wings, rock sides over 40 deg), so nothing perches on a post or
 *  a beam. */
enum class EColl : uint8 { None, Walk, Block };
enum EDetail : uint8 { DetailFar, DetailNear, NumDetail };
const TCHAR* const DetailNames[NumDetail] = { TEXT("Far"), TEXT("Near") };

// ------------------------------------------------------------------ meshes (per 128 m tile and draw distance)
struct FDrawPart { TArray<FVector> V, N; TArray<float> Ao; TArray<int32> I; };
struct FCollPart { TArray<FVector> V; TArray<int32> I; };
struct FBucket { FDrawPart Draw[MatCount]; FCollPart Walk, Block; };

struct FMeshes
{
    TArray<FBucket> Buckets;
    FMeshes() { Buckets.SetNum(NumDetail * NumTiles); }
    static int32 TileOf(const FVector2D& P)
    {
        const double HalfSpan = HouseCalderTerrain::Half, Tile = HouseCalderTerrainTiles::TileSize;
        const int32 TX = FMath::Clamp(FMath::FloorToInt((P.X + HalfSpan) / Tile), 0, TilesPerSide - 1);
        const int32 TY = FMath::Clamp(FMath::FloorToInt((P.Y + HalfSpan) / Tile), 0, TilesPerSide - 1);
        return TY * TilesPerSide + TX;
    }
    FBucket& At(EDetail Detail, const FVector2D& P) { return Buckets[int32(Detail) * NumTiles + TileOf(P)]; }
};

struct FStats
{
    double GuardCm = 0.0, RailCm = 0.0, LiningCm = 0.0, HeldCm = 0.0;
    int32 GuardRuns = 0, LookRuns = 0, GuardSkipped = 0, Terminals = 0, Posts = 0, Rails = 0, RailsOnStructures = 0, RailsSkipped = 0;
    int32 Ditches = 0, DitchesHeld = 0, Rocks = 0, WeepHoles = 0, Basins = 0, BasinsSkipped = 0;
    int32 Headwalls = 0, Inlets = 0, DropInlets = 0, GutterInlets = 0, Outlets = 0, OutletsSkipped = 0;
    TArray<FString> Notes;   // logged once by Build
};

/** What Build left standing, for Occupied and FootPoints. */
struct FBuilt { TArray<FBox2D> Boxes; TArray<FVector> Feet; };
FBuilt& Built()
{
    static FBuilt State;
    return State;
}

/** Where a built thing meets the ground (Surface: the ground there as read now): kept for FootPoints and registered with the
 *  review's foot-point registry (HouseCalderFootPoints; nothing while the Calder terrain is off). */
void RegisterFoot(const FVector& At, float Surface)
{
    Built().Feet.Add(At);
    HouseCalderFootPoints::Register(TEXT("CalderRoadside"), At, Surface);
}

// ------------------------------------------------------------------ plan helpers, the ground and noise
float GroundAt(const FVector2D& P) { return HouseExteriorSite::GroundHeight(float(P.X), float(P.Y)); }
FVector At3(const FVector2D& P, double Z) { return FVector(P.X, P.Y, Z); }
double Cross2(const FVector2D& U, const FVector2D& V) { return U.X * V.Y - U.Y * V.X; }
/** Left of D (anticlockwise by 90 degrees). */
FVector2D Perp(const FVector2D& D) { return FVector2D(-D.Y, D.X); }

double SegDist(const FVector2D& P, const FVector2D& A, const FVector2D& B)
{
    const FVector2D AB = B - A;
    const double L2 = AB.SizeSquared();
    const double T = L2 > 1e-9 ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.0, 1.0) : 0.0;
    return (P - (A + AB * T)).Size();
}

/** Integer hash (no RNG stream: the same value for the same inputs on every machine). */
uint32 Mix(int32 X, int32 Y, int32 Z, uint32 Seed)
{
    uint32 H = Seed ^ 0x9E3779B9u;
    H = (H ^ uint32(X)) * 0x85EBCA6Bu; H ^= H >> 13;
    H = (H ^ uint32(Y)) * 0xC2B2AE35u; H ^= H >> 16;
    H = (H ^ uint32(Z)) * 0x27D4EB2Fu; H ^= H >> 15;
    H *= 0x165667B1u; H ^= H >> 16;
    return H;
}
float Hash01(int32 X, int32 Y, int32 Z, uint32 Seed) { return float(Mix(X, Y, Z, Seed) & 0xFFFFFFu) / float(0xFFFFFF); }

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
    return FMath::Lerp(FMath::Lerp(X00, X10, SY), FMath::Lerp(X01, X11, SY), SZ) * 2.f - 1.f;
}

/** Today's exterior paving the campus plan does not know: Calder Dr north of the junction, its carriageway (Road) and with
 *  both its sidewalks (Paved). */
bool ExteriorRoad(const FVector2D& P, float Pad)
{
    return P.Y > HouseCampusLayout::JunctionSouthY - Pad && P.X >= 8700.f - Pad && P.X <= 10300.f + Pad;
}
bool ExteriorPaved(const FVector2D& P, float Pad)
{
    return P.Y > HouseCampusLayout::JunctionSouthY - Pad && P.X >= 8450.f - Pad && P.X <= 10550.f + Pad;
}

/** A landform path (the pond loop, the Crow Ledge trail) lies within Pad of P. */
bool OnLandformPath(const FVector2D& P, float Pad)
{
    for (const HouseCalderLandform::FLandformPath& Path : HouseCalderLandform::LandformPaths())
    {
        const int32 Num = Path.Points.Num();
        const int32 Segs = Path.bClosed ? Num : Num - 1;
        for (int32 K = 0; K < Segs; ++K)
            if (SegDist(P, Path.Points[K], Path.Points[(K + 1) % Num]) <= Path.Width * .5f + Pad) return true;
    }
    return false;
}

/** Something people walk or drive on, or a building, lies within Pad of P: the campus plan's paving (roads, lots, walks,
 *  sidewalks, plazas, aprons; with bCurb also the 30 cm curb a road or lot has on its lawn side), Calder Dr north, a built
 *  stair or ramp, a landform path, or a plan building's footprint. */
bool Taken(const FVector2D& P, float Pad, bool bCurb)
{
    if (ExteriorPaved(P, Pad) || OnLandformPath(P, Pad)) return true;
    if (HouseCampusLayout::PavingNear(P, Pad + (bCurb ? CurbW : 0.f))) return true;
    if (HouseCalderFeatures::InStair(P, Pad) || HouseCalderFeatures::InRamp(P, Pad)) return true;
    for (const HouseCampusLayout::FBlock& Bl : HouseCampusLayout::Blocks()) if (Bl.Rect().ExpandBy(Pad).IsInside(P)) return true;
    return false;
}

bool TouchesFrozen(const FBox2D& Box)
{
    for (const FBox2D& R : HouseCalderTerrain::FrozenRects()) if (R.Intersect(Box)) return true;
    return false;
}

/** The plan rectangle Origin + AxA * [A0, A1] + AxB * [B0, B1] is clear of a frozen zone and of Taken ground (probed on a
 *  20 cm grid), grown by Pad. */
bool RectClear(const FVector2D& Origin, const FVector2D& AxA, const FVector2D& AxB, double A0, double A1, double B0, double B1, float Pad, bool bCurb)
{
    FBox2D Box(ForceInit);
    for (const double A : { A0, A1 }) for (const double B : { B0, B1 }) Box += Origin + AxA * A + AxB * B;
    if (TouchesFrozen(Box.ExpandBy(Pad))) return false;
    const int32 NA = FMath::Clamp(FMath::CeilToInt((A1 - A0) / 20.0), 1, 80), NB = FMath::Clamp(FMath::CeilToInt((B1 - B0) / 20.0), 1, 80);
    for (int32 I = 0; I <= NA; ++I)
        for (int32 J = 0; J <= NB; ++J)
            if (Taken(Origin + AxA * FMath::Lerp(A0, A1, double(I) / NA) + AxB * FMath::Lerp(B0, B1, double(J) / NB), Pad, bCurb)) return false;
    return true;
}

// ------------------------------------------------------------------ declared walls
bool IsRock(EWallKind K) { return K == EWallKind::RockBand || K == EWallKind::RockCut || K == EWallKind::RockLedge; }

/** The step across wall WI at P on its line (the ground 3 cm either side): the high side's normal and both heights; false
 *  where the step is under MinStep (nothing is built there). */
bool StepAt(int32 WI, const FVector2D& P, FVector2D& OutNh, float& OutLo, float& OutHi)
{
    const HouseCalderLandform::FWall& Wl = HouseCalderLandform::Walls()[WI];
    const FVector2D D = Wl.B - Wl.A;
    const double Len = D.Size();
    if (Len < 1.0) return false;
    const FVector2D Nl = Perp(D / Len);
    const float ZL = GroundAt(P + Nl * 3.0), ZR = GroundAt(P - Nl * 3.0);
    OutNh = ZL >= ZR ? Nl : -Nl;
    OutLo = FMath::Min(ZL, ZR);
    OutHi = FMath::Max(ZL, ZR);
    return OutHi - OutLo >= MinStep;
}

/** The walls with any part within Reach of Box. */
TArray<int32> WallsNear(const FBox2D& Box, double Reach)
{
    TArray<int32> Out;
    const TArray<HouseCalderLandform::FWall>& All = HouseCalderLandform::Walls();
    const FBox2D Grown = Box.ExpandBy(Reach);
    for (int32 WI = 0; WI < All.Num(); ++WI)
    {
        FBox2D WB(ForceInit);
        WB += All[WI].A;
        WB += All[WI].B;
        if (WB.Intersect(Grown)) Out.Add(WI);
    }
    return Out;
}

/** Where the ray From + Dir * t (0 < t <= MaxT) first crosses one of the walls Near with a real step there. */
bool RayWall(const TArray<int32>& Near, const FVector2D& From, const FVector2D& Dir, double MaxT, double& OutT, int32& OutWall)
{
    bool bHit = false;
    OutT = MaxT;
    for (const int32 WI : Near)
    {
        const HouseCalderLandform::FWall& Wl = HouseCalderLandform::Walls()[WI];
        const FVector2D WD = Wl.B - Wl.A;
        const double Den = Cross2(Dir, WD);
        if (FMath::Abs(Den) < 1e-9) continue;
        const FVector2D AF = Wl.A - From;
        const double T = Cross2(AF, WD) / Den, U = Cross2(AF, Dir) / Den;
        if (T <= 0.0 || T > OutT || U < 0.0 || U > 1.0) continue;
        FVector2D Nh;
        float Lo = 0.f, Hi = 0.f;
        if (!StepAt(WI, From + Dir * T, Nh, Lo, Hi)) continue;
        OutT = T;
        OutWall = WI;
        bHit = true;
    }
    return bHit;
}

/** How far in front of its line (on the low side) wall WI's built face may stand at P: a rock face's batter and relief
 *  (HouseCalderFeatures: min(H tan 8 deg, 45 cm, a rock band's 600) + 2 x 12 cm and its lip); a masonry wall's body and
 *  coping where paving along its high side sends the body to the low side (40 + 10 cm); else 0 (its face on the line). */
double FaceReach(int32 WI, const FVector2D& P)
{
    FVector2D Nh;
    float Lo = 0.f, Hi = 0.f;
    if (!StepAt(WI, P, Nh, Lo, Hi)) return 0.0;
    const EWallKind K = HouseCalderLandform::Walls()[WI].Kind;
    if (IsRock(K)) return FMath::Min(double(Hi - Lo) * RockBatterTan, K == EWallKind::RockBand ? 600.0 : 45.0) + 2.0 * RockAmp + 2.0;
    return Taken(P + Nh * 40.0, 75.f, false) ? 50.0 : 0.0;
}

/** A polyline with its arc length. */
struct FLine2
{
    TArray<FVector2D> P;
    TArray<double> Cum;
    explicit FLine2(const TArray<FVector2D>& Points) : P(Points)
    {
        Cum.Add(0.0);
        for (int32 K = 1; K < P.Num(); ++K) Cum.Add(Cum.Last() + (P[K] - P[K - 1]).Size());
    }
    double Length() const { return Cum.Last(); }
    /** The point S along the line and the line's direction there (needs two points or more). */
    void At(double S, FVector2D& OutP, FVector2D& OutDir) const
    {
        int32 K = 1;
        while (K + 1 < P.Num() && Cum[K] < S) ++K;
        const FVector2D D = P[K] - P[K - 1];
        const double L = FMath::Max(D.Size(), 1e-6);
        OutDir = D / L;
        OutP = P[K - 1] + OutDir * FMath::Clamp(S - Cum[K - 1], 0.0, L);
    }
};

// ------------------------------------------------------------------ primitives
/** A convex polygon facing Out, fanned from its first point. FHallMesh's rule, as the terrain tiles and the features use it:
 *  a triangle (A, B, C) faces the side its cross product points away from. Normals: per point (smooth), or null for the
 *  polygon's own. Mat == MatCount: collision only. */
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

/** A hexahedron: Low[4] round one end, High[4] round the other. Faces by Mask: 1 the High end, 2 the Low end, 4 << K side K
 *  (Low[K] to Low[K + 1]). Each face looks away from the solid's centre. */
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
constexpr int32 AllFaces = 63, NoBottom = 61;

/** A box in a plan frame: Origin + AxA * [A0, A1] + AxB * [B0, B1], from Z0 up to Z1. */
void PBox(FBucket& B, EMat Mat, EColl Coll, const FVector2D& Origin, const FVector2D& AxA, const FVector2D& AxB, double A0, double A1, double B0, double B1,
    double Z0, double Z1, int32 Mask = AllFaces, float AoLow = 1.f, float AoHigh = 1.f)
{
    const FVector2D C[4] = { Origin + AxA * A0 + AxB * B0, Origin + AxA * A1 + AxB * B0, Origin + AxA * A1 + AxB * B1, Origin + AxA * A0 + AxB * B1 };
    const FVector Low[4] = { At3(C[0], Z0), At3(C[1], Z0), At3(C[2], Z0), At3(C[3], Z0) };
    const FVector High[4] = { At3(C[0], Z1), At3(C[1], Z1), At3(C[2], Z1), At3(C[3], Z1) };
    Hexa(B, Mat, Coll, Low, High, Mask, AoLow, AoHigh);
}

/** The ring of directions round Axis. */
void Ring(const FVector& Axis, int32 Sides, TArray<FVector>& Out)
{
    const FVector Ref = FMath::Abs(Axis.Z) < .9 ? FVector::UpVector : FVector::ForwardVector;
    const FVector U = FVector::CrossProduct(Axis, Ref).GetSafeNormal(), W = FVector::CrossProduct(Axis, U);
    Out.Reset();
    for (int32 K = 0; K < Sides; ++K)
    {
        const double Ang = 2.0 * UE_DOUBLE_PI * K / Sides;
        Out.Add(U * FMath::Cos(Ang) + W * FMath::Sin(Ang));
    }
}

/** A round tube from A to C: its outside, or (bInside) its inside as seen from within. No end caps. */
void Tube(FBucket& B, EMat Mat, EColl Coll, const FVector& A, const FVector& C, float Radius, int32 Sides, bool bInside)
{
    const FVector Axis = (C - A).GetSafeNormal();
    if (Axis.IsNearlyZero()) return;
    TArray<FVector> Dirs;
    Ring(Axis, Sides, Dirs);
    const double Sg = bInside ? -1.0 : 1.0;
    for (int32 K = 0; K < Sides; ++K)
    {
        const int32 L = (K + 1) % Sides;
        const FVector Q[4] = { A + Dirs[K] * Radius, A + Dirs[L] * Radius, C + Dirs[L] * Radius, C + Dirs[K] * Radius };
        const FVector Nrm[4] = { Dirs[K] * Sg, Dirs[L] * Sg, Dirs[L] * Sg, Dirs[K] * Sg };
        Poly(B, Mat, Coll, Q, 4, (Dirs[K] + Dirs[L]) * Sg, Nrm);
    }
}

/** A flat disc facing Normal. */
void Disc(FBucket& B, EMat Mat, EColl Coll, const FVector& Centre, const FVector& Normal, float Radius, int32 Sides)
{
    TArray<FVector> Dirs, Pts;
    Ring(Normal.GetSafeNormal(), Sides, Dirs);
    for (const FVector& D : Dirs) Pts.Add(Centre + D * Radius);
    Poly(B, Mat, Coll, Pts.GetData(), Pts.Num(), Normal);
}

/** A convex outline draped on the ground + Lift (or, bRoad, on a road's top: HouseCalderTerrain::SurfaceZ, road class),
 *  split along the relief lattice by HouseCalderDrape so it follows the ground's own triangles; drawn only. */
void Drape(FBucket& B, EMat Mat, const TArray<FVector2D>& Outline, float Lift, bool bRoad, float Ao)
{
    FHallMesh Tmp;
    const FName Key(TEXT("CalderRoadsideDrape"));
    if (bRoad) HouseCalderDrape::Sheet(Tmp, Key, Outline, [Lift](float X, float Y) { return HouseCalderTerrain::SurfaceZ(X, Y, EPave::Road) + Lift; }, FHallUV::World(100.f));
    else HouseCalderDrape::Sheet(Tmp, Key, Outline, [Lift](float X, float Y) { return HouseExteriorSite::GroundHeight(X, Y) + Lift; }, FHallUV::World(100.f));
    const FHallSection* S = Tmp.Sections.Find(Key);
    if (!S) return;
    FDrawPart& D = B.Draw[Mat];
    const int32 Base0 = D.V.Num();
    for (int32 K = 0; K < S->V.Num(); ++K)
    {
        D.V.Add(S->V[K]);
        D.N.Add(S->N.IsValidIndex(K) ? S->N[K] : FVector::UpVector);
        D.Ao.Add(Ao);
    }
    for (const int32 Ix : S->I) D.I.Add(Base0 + Ix);
}

TArray<FVector2D> RectOutline(const FVector2D& Origin, const FVector2D& AxA, const FVector2D& AxB, double A0, double A1, double B0, double B1)
{
    return { Origin + AxA * A0 + AxB * B0, Origin + AxA * A1 + AxB * B0, Origin + AxA * A1 + AxB * B1, Origin + AxA * A0 + AxB * B1 };
}

/** A boulder: a lumpy, flattened sphere (its height Flat x R) sunk a third into the lowest ground under it. Its faces within
 *  40 deg of level join the walkable collision (a low one is stepped onto), the steeper ones the Unwalkable. */
void Boulder(FMeshes& M, const FVector2D& At, float R, float Flat, uint32 Seed, FStats& St)
{
    constexpr int32 Rings = 6, Segs = 9;
    const float Yaw = Hash01(1, 0, 0, Seed) * 2.f * UE_PI, Stretch = 1.f + .3f * Hash01(2, 0, 0, Seed);
    float Low = GroundAt(At);
    FVector2D LowAt = At;
    for (int32 K = 0; K < 6; ++K)
    {
        const FVector2D Q = At + FVector2D(FMath::Cos(K * UE_PI / 3.f), FMath::Sin(K * UE_PI / 3.f)) * (R * Stretch * .85f);
        const float G = GroundAt(Q);
        if (G < Low) { Low = G; LowAt = Q; }
    }
    const FVector C = At3(At, Low + R * Flat * .3f);
    const FQuat Turn(FVector::UpVector, Yaw);
    TArray<FVector> V;
    TArray<float> Ao;
    V.SetNum((Rings + 1) * Segs);
    Ao.SetNum(V.Num());
    for (int32 Rg = 0; Rg <= Rings; ++Rg)
        for (int32 Sg = 0; Sg < Segs; ++Sg)
        {
            const float Phi = UE_PI * Rg / Rings, Theta = 2.f * UE_PI * Sg / Segs + (Rg % 2) * UE_PI / Segs;
            const FVector Dir(FMath::Sin(Phi) * FMath::Cos(Theta), FMath::Sin(Phi) * FMath::Sin(Theta), FMath::Cos(Phi));
            FVector L = Dir * (R * (1.f + .26f * Value3(Dir * 170.f + FVector(double(Seed % 997u), 0.0, 0.0), 120.f, Seed)));
            L.X *= Stretch;
            L.Z *= Flat;
            const FVector P = C + Turn.RotateVector(L);
            V[Rg * Segs + Sg] = P;
            Ao[Rg * Segs + Sg] = .55f + .45f * FMath::Clamp(float(P.Z - Low) / FMath::Max(R * Flat, 1.f), 0.f, 1.f);
        }
    auto Vx = [&V](int32 Rg, int32 Sg) -> const FVector& { return V[FMath::Clamp(Rg, 0, Rings) * Segs + ((Sg % Segs) + Segs) % Segs]; };
    TArray<FVector> Nrm;
    Nrm.SetNum(V.Num());
    for (int32 Rg = 0; Rg <= Rings; ++Rg)
        for (int32 Sg = 0; Sg < Segs; ++Sg)
        {
            const FVector& P = Vx(Rg, Sg);
            FVector N = FVector::CrossProduct(Vx(Rg, Sg + 1) - Vx(Rg, Sg - 1), Vx(Rg + 1, Sg) - Vx(Rg - 1, Sg)).GetSafeNormal();
            if (N.IsNearlyZero()) N = (P - C).GetSafeNormal();
            if (FVector::DotProduct(N, P - C) < 0.0) N = -N;
            Nrm[Rg * Segs + Sg] = N;
        }
    FBucket& B = M.At(DetailFar, At);
    for (int32 Rg = 0; Rg < Rings; ++Rg)
        for (int32 Sg = 0; Sg < Segs; ++Sg)
        {
            const int32 S1 = (Sg + 1) % Segs;
            const int32 I0 = Rg * Segs + Sg, I1 = Rg * Segs + S1, I2 = (Rg + 1) * Segs + S1, I3 = (Rg + 1) * Segs + Sg;
            const FVector Q[4] = { V[I0], V[I1], V[I2], V[I3] };
            if (FMath::Max(FMath::Max(Q[0].Z, Q[1].Z), FMath::Max(Q[2].Z, Q[3].Z)) < Low - 6.0) continue;   // wholly in the ground
            const FVector N4[4] = { Nrm[I0], Nrm[I1], Nrm[I2], Nrm[I3] };
            const float A4[4] = { Ao[I0], Ao[I1], Ao[I2], Ao[I3] };
            const FVector Outward = (Q[0] + Q[1] + Q[2] + Q[3]) * .25 - C;
            FVector FaceN = FVector::CrossProduct(Q[2] - Q[0], Q[3] - Q[1]).GetSafeNormal();
            if (FVector::DotProduct(FaceN, Outward) < 0.0) FaceN = -FaceN;
            Poly(B, MatBoulder, FaceN.Z >= WalkableNz ? EColl::Walk : EColl::Block, Q, 4, Outward, N4, A4);
        }
    const float Reach = R * Stretch;
    Built().Boxes.Add(FBox2D(At - FVector2D(Reach, Reach), At + FVector2D(Reach, Reach)));
    RegisterFoot(At3(LowAt, Low), Low);
    ++St.Rocks;
}

/** A rock that would stand clear: off paving (and its curb), paths, stairs, buildings and frozen zones. */
bool RockClear(const FVector2D& At, float R)
{
    return !TouchesFrozen(FBox2D(At, At).ExpandBy(R * 1.3f)) && !Taken(At, R * 1.3f + 4.f, true);
}

// ------------------------------------------------------------------ the W-beam guardrail
/** The W-beam's section from its bottom edge to its top: height over the bottom, and how far it stands out toward the road
 *  from the bolting plane (the blockouts' face): two peaks 8.3 cm out, the bolted valley between, both edges turned back. */
constexpr int32 NumProfile = 9;
const FVector2D BeamProfile[NumProfile] = { FVector2D(0.0, 1.8), FVector2D(2.2, 4.6), FVector2D(7.8, 8.3), FVector2D(11.6, 6.2), FVector2D(15.55, 0.0),
    FVector2D(19.5, 6.2), FVector2D(23.3, 8.3), FVector2D(28.9, 4.6), FVector2D(31.1, 1.8) };
constexpr int32 ValleyIndex = 4;

/** The beam at one station: its face point, the line's direction, Out toward the road, the bolting plane's point, the
 *  ground, the turned-down end's twist and drop, and the section's points and front normals. */
struct FBeamSection
{
    double S = 0.0;
    FVector2D Face, Dir, Out, Bolt;
    float Ground = 0.f, Theta = 0.f, Top = 0.f, Mid = 0.f;
    FVector Ua, Oa, Pivot;
    FVector Pts[NumProfile], Nrm[NumProfile];
    FVector Corner(int32 Hi, int32 Front) const { return Pivot + Ua * ((Hi ? .5 : -.5) * BeamHeight) + Oa * (Front ? BeamDepth : 0.f); }
};

/** A rust streak down the beam's face from the bolt in the valley to its bottom edge, tapering, 1.2 mm proud. */
void Streak(FMeshes& M, const FBeamSection& C, double AlongOff, float Width0, float Width1)
{
    FBucket& B = M.At(DetailNear, C.Face);
    const FVector D3(C.Dir.X, C.Dir.Y, 0.0);
    for (int32 J = ValleyIndex; J > 0; --J)
    {
        const float F0 = float(ValleyIndex - J) / ValleyIndex, F1 = float(ValleyIndex - J + 1) / ValleyIndex;
        const float W0 = FMath::Lerp(Width0, Width1, F0), W1 = FMath::Lerp(Width0, Width1, F1);
        const FVector P0 = C.Pts[J] + C.Nrm[J] * .12 + D3 * AlongOff, P1 = C.Pts[J - 1] + C.Nrm[J - 1] * .12 + D3 * AlongOff;
        const FVector Q[4] = { P0 - D3 * W0, P0 + D3 * W0, P1 + D3 * W1, P1 - D3 * W1 };
        const FVector N4[4] = { C.Nrm[J], C.Nrm[J], C.Nrm[J - 1], C.Nrm[J - 1] };
        Poly(B, MatRust, EColl::None, Q, 4, C.Nrm[J] + C.Nrm[J - 1], N4);
    }
}

/** A rusted bolt head on the beam's valley at the section, AlongOff along it and HeightOff up its face. */
void BoltHead(FMeshes& M, const FBeamSection& C, double AlongOff, double HeightOff)
{
    const FVector D3(C.Dir.X, C.Dir.Y, 0.0);
    const FVector Centre = C.Pts[ValleyIndex] + D3 * AlongOff + C.Ua * HeightOff + C.Nrm[ValleyIndex] * .8;
    const FVector2D P(Centre.X, Centre.Y);
    PBox(M.At(DetailNear, P), MatRust, EColl::None, P, C.Dir, C.Out, -2.0, 2.0, -.8, .8, Centre.Z - 2.0, Centre.Z + 2.0, AllFaces);
}

/** A W-beam run with its face (the beam's front) along FaceLine and the road on RoadSide of the line's direction (+1 left,
 *  -1 right): posts every 1.905 m behind blockouts, a turned-down end 7.62 m long at each end. Every post, blockout and the
 *  beam's back stand clear of paving, paths, stairs, buildings and frozen zones, or the run is not built. */
void BuildGuardrail(FMeshes& M, const FString& Name, const TArray<FVector2D>& FaceLine, int32 RoadSide, FStats& St)
{
    if (FaceLine.Num() < 2)
    {
        ++St.GuardSkipped;
        return;
    }
    const FLine2 Line(FaceLine);
    const double Len = Line.Length();
    if (Len < 2.0 * TerminalLength + 2.0 * PostSpacing)
    {
        ++St.GuardSkipped;
        St.Notes.Add(FString::Printf(TEXT("CALDER_ROADSIDE_SKIP guardrail=\"%s\" reason=short (%.0f cm: no room for two turned-down ends)"), *Name, Len));
        return;
    }
    const int32 Samples = FMath::Max(2, FMath::RoundToInt(Len / (PanelLength / SamplesPerPanel) / SamplesPerPanel)) * SamplesPerPanel;
    TArray<FBeamSection> Sec;
    Sec.SetNum(Samples + 1);
    for (int32 K = 0; K <= Samples; ++K)
    {
        FBeamSection& C = Sec[K];
        C.S = Len * K / Samples;
        Line.At(C.S, C.Face, C.Dir);
        C.Out = Perp(C.Dir) * double(RoadSide);
        C.Bolt = C.Face - C.Out * BeamDepth;
        C.Ground = GroundAt(C.Face);
        // The turned-down end: over its 7.62 m the beam's top drops from 64 cm to 4 cm under the ground, and it twists (its
        // top leaning away from the road) so that it goes into the ground lying nearly flat.
        const float U01 = float(FMath::Clamp(1.0 - FMath::Min(C.S, Len - C.S) / TerminalLength, 0.0, 1.0));
        C.Theta = FMath::DegreesToRadians(TerminalTwistDeg) * FMath::SmoothStep(.25f, 1.f, U01);
        C.Top = FMath::Lerp(BeamTop, -4.f, U01);
        const FVector Out3(C.Out.X, C.Out.Y, 0.0);
        C.Ua = FVector::UpVector * FMath::Cos(C.Theta) - Out3 * FMath::Sin(C.Theta);
        C.Oa = Out3 * FMath::Cos(C.Theta) + FVector::UpVector * FMath::Sin(C.Theta);
        float Highest = -FLT_MAX;
        for (const FVector2D& Q : BeamProfile) Highest = FMath::Max(Highest, float((Q.X - BeamHeight * .5) * FMath::Cos(C.Theta) + Q.Y * FMath::Sin(C.Theta)));
        C.Mid = C.Ground + C.Top - Highest;
        C.Pivot = At3(C.Bolt, C.Mid);
        for (int32 J = 0; J < NumProfile; ++J)
        {
            C.Pts[J] = C.Pivot + C.Ua * (BeamProfile[J].X - BeamHeight * .5) + C.Oa * BeamProfile[J].Y;
            const FVector2D& Below = BeamProfile[FMath::Max(J - 1, 0)];
            const FVector2D& Above = BeamProfile[FMath::Min(J + 1, NumProfile - 1)];
            C.Nrm[J] = (C.Ua * -(Above.Y - Below.Y) + C.Oa * (Above.X - Below.X)).GetSafeNormal();
        }
    }
    // Clear of paving, paths, stairs, buildings and frozen zones: the beam's face and the posts' backs.
    for (const FBeamSection& C : Sec)
    {
        const FVector2D Back = C.Bolt - C.Out * (BlockAcross + PostAcross);
        FBox2D Box(ForceInit);
        Box += C.Face;
        Box += Back;
        if (TouchesFrozen(Box.ExpandBy(20.f)) || Taken(C.Face, 8.f, true) || Taken(Back, 8.f, true))
        {
            ++St.GuardSkipped;
            St.Notes.Add(FString::Printf(TEXT("CALDER_ROADSIDE_SKIP guardrail=\"%s\" reason=blocked at (%.0f,%.0f) (paving, a path, a stair, a building or a frozen zone)"), *Name, C.Face.X, C.Face.Y));
            return;
        }
    }
    const uint32 RunSeed = Mix(FMath::RoundToInt(FaceLine[0].X), FMath::RoundToInt(FaceLine[0].Y), 0, 0x6A11u);
    // The beam: the sheet's front (toward the road) and back, smooth across the W; a few older, darker panels.
    for (int32 K = 0; K < Samples; ++K)
    {
        const FBeamSection& A = Sec[K];
        const FBeamSection& Bs = Sec[K + 1];
        if (A.Top < -2.f && Bs.Top < -2.f) continue;   // wholly in the ground at the very end
        FBucket& Bk = M.At(DetailFar, (A.Face + Bs.Face) * .5);
        const int32 Panel = K / SamplesPerPanel;
        const EMat Mat = Mix(Panel, 0, 0, RunSeed) % 5u == 0u ? MatGalvOld : MatGalv;
        for (int32 J = 0; J + 1 < NumProfile; ++J)
        {
            const FVector F[4] = { A.Pts[J], Bs.Pts[J], Bs.Pts[J + 1], A.Pts[J + 1] };
            const FVector FN[4] = { A.Nrm[J], Bs.Nrm[J], Bs.Nrm[J + 1], A.Nrm[J + 1] };
            Poly(Bk, Mat, EColl::None, F, 4, FN[0] + FN[1] + FN[2] + FN[3], FN);
            const FVector Bv[4] = { F[0] - FN[0] * BeamSheet, F[1] - FN[1] * BeamSheet, F[2] - FN[2] * BeamSheet, F[3] - FN[3] * BeamSheet };
            const FVector BN[4] = { -FN[0], -FN[1], -FN[2], -FN[3] };
            Poly(Bk, Mat, EColl::None, Bv, 4, -(FN[0] + FN[1] + FN[2] + FN[3]), BN);
        }
        // Collision: the beam's envelope, Unwalkable; where a turned-down end lies nearly flat (its face within 40 deg of up),
        // walkable: it is stepped onto there.
        const FVector Low[4] = { A.Corner(0, 0), A.Corner(0, 1), A.Corner(1, 1), A.Corner(1, 0) };
        const FVector High[4] = { Bs.Corner(0, 0), Bs.Corner(0, 1), Bs.Corner(1, 1), Bs.Corner(1, 0) };
        const bool bFlatEnd = FMath::Min(A.Theta, Bs.Theta) >= FMath::DegreesToRadians(FlatBeamDeg);
        Hexa(Bk, MatCount, bFlatEnd ? EColl::Walk : EColl::Block, Low, High, AllFaces);
        FBox2D Box(ForceInit);
        for (const FVector& P : Low) Box += FVector2D(P.X, P.Y);
        for (const FVector& P : High) Box += FVector2D(P.X, P.Y);
        Built().Boxes.Add(Box.ExpandBy(3.f));
    }
    // Posts with blockouts where the beam is at full height; down the turned-down end the beam is bolted to shorter posts
    // directly (no blockout) while it is still nearly upright, then rides down unsupported to its anchor. Bolts and rust.
    int32 Posts = 0;
    for (int32 K = 0; K <= Samples; K += SamplesPerPost)
    {
        const FBeamSection& C = Sec[K];
        if (C.Top < 26.f || C.Theta > FMath::DegreesToRadians(20.f)) continue;
        const bool bTurnedDown = C.Top < BeamTop - 1.f;
        const double Behind = bTurnedDown ? 0.0 : BlockAcross;
        const FVector2D BlockC = C.Bolt - C.Out * (BlockAcross * .5);
        const FVector2D PostC = C.Bolt - C.Out * (Behind + PostAcross * .5);
        const FVector2D Along = C.Dir * (PostAlong * .5), Across = C.Out * (PostAcross * .5);
        float PostGround = GroundAt(PostC);
        for (const FVector2D& Q : { PostC - Along - Across, PostC + Along - Across, PostC + Along + Across, PostC - Along + Across }) PostGround = FMath::Min(PostGround, GroundAt(Q));
        FBucket& Bk = M.At(DetailFar, PostC);
        PBox(Bk, MatTimber, EColl::Block, PostC, C.Dir, C.Out, -PostAlong * .5, PostAlong * .5, -PostAcross * .5, PostAcross * .5, PostGround - PostBury, C.Mid + (bTurnedDown ? 6.f : 13.f), AllFaces, .55f, 1.f);
        if (!bTurnedDown) PBox(Bk, MatTimber, EColl::Block, BlockC, C.Dir, C.Out, -PostAlong * .5, PostAlong * .5, -BlockAcross * .5, BlockAcross * .5, C.Mid - BlockHalfHeight, C.Mid + BlockHalfHeight, AllFaces, .7f, .9f);
        BoltHead(M, C, 0.0, 0.0);
        const bool bSplice = !bTurnedDown && K % SamplesPerPanel == 0 && K > 0 && K < Samples;
        if (bSplice) for (const double Off : { -13.5, -4.5, 4.5, 13.5 }) BoltHead(M, C, Off, 0.0);
        // Rust run down from about half the bolts (deterministic by station), longer and wider at the splices.
        if (!bTurnedDown && Hash01(K, 3, 0, RunSeed) < (bSplice ? .8f : .45f)) Streak(M, C, (Hash01(K, 4, 0, RunSeed) - .5f) * 3.f, bSplice ? 4.f : 2.6f, bSplice ? 1.6f : .9f);
        if (bSplice && Hash01(K, 5, 0, RunSeed) < .5f) Streak(M, C, 13.5 * (Hash01(K, 6, 0, RunSeed) < .5f ? -1.0 : 1.0), 2.2f, .7f);
        Built().Boxes.Add(FBox2D(PostC - FVector2D(14.f, 14.f), PostC + FVector2D(14.f, 14.f)));
        const float PostFoot = GroundAt(PostC);
        RegisterFoot(At3(PostC, PostFoot), PostFoot);
        ++Posts;
    }
    // A stained concrete anchor where each turned-down end goes into the ground.
    for (const int32 K : { 0, Samples })
    {
        const FBeamSection& C = Sec[K];
        const FVector2D Centre = C.Bolt + C.Out * (BeamDepth * .5) + C.Dir * (K == 0 ? 25.0 : -25.0);
        float G0 = GroundAt(Centre);
        for (const double A : { -38.0, 38.0 }) for (const double B : { -26.0, 26.0 }) G0 = FMath::Min(G0, GroundAt(Centre + C.Dir * A + C.Out * B));
        PBox(M.At(DetailFar, Centre), MatConcrete, EColl::Walk, Centre, C.Dir, C.Out, -38.0, 38.0, -26.0, 26.0, G0 - 50.f, G0 + 5.f, AllFaces, .6f, 1.f);
        Built().Boxes.Add(FBox2D(Centre - FVector2D(46.f, 46.f), Centre + FVector2D(46.f, 46.f)));
        const float AnchorFoot = GroundAt(Centre);
        RegisterFoot(At3(Centre, AnchorFoot), AnchorFoot);
    }
    St.GuardCm += Len;
    St.Posts += Posts;
    St.Terminals += 2;
}

/** The turned-down end may be carried on past a run's end along Dir: the ground there is open (no paving, path, stair,
 *  building or frozen zone within 40 cm). */
bool ExtensionClear(const FVector2D& From, const FVector2D& Dir)
{
    for (double T = 50.0; T <= TerminalLength + 1.0; T += 50.0)
    {
        const FVector2D P = From + Dir * T;
        if (TouchesFrozen(FBox2D(P, P).ExpandBy(60.f)) || Taken(P, 40.f, true)) return false;
    }
    return true;
}

void BuildGuardrails(FMeshes& M, FStats& St)
{
    for (const HouseCalderLandform::FRoadsideRun& R : HouseCalderLandform::Guardrails())
    {
        if (R.bAtWall)
        {
            // The drop is a declared wall, which carries a parapet (HouseCalderFeatures), and its verge is the sidewalk.
            ++St.GuardSkipped;
            St.Notes.Add(FString::Printf(TEXT("CALDER_ROADSIDE_SKIP guardrail=\"%s\" reason=atWall (the declared wall's parapet guards its %.0f cm drop)"), *R.Name, R.Value));
            continue;
        }
        if (R.Points.Num() < 2 || R.Side == 0)
        {
            ++St.GuardSkipped;
            continue;
        }
        // The face on the verge stations (the road on the side away from the drop); the turned-down ends carried on 7.62 m
        // past both ends where the verge there is open, else laid within the run.
        TArray<FVector2D> Line = R.Points;
        const FVector2D D0 = (R.Points[0] - R.Points[1]).GetSafeNormal(), D1 = (R.Points.Last() - R.Points[R.Points.Num() - 2]).GetSafeNormal();
        if (ExtensionClear(R.Points[0], D0)) Line.Insert(R.Points[0] + D0 * TerminalLength, 0);
        if (ExtensionClear(R.Points.Last(), D1)) Line.Add(R.Points.Last() + D1 * TerminalLength);
        ++St.GuardRuns;
        BuildGuardrail(M, R.Name, Line, -R.Side, St);
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("NoCalderLookGuardrail")))
    {
        ++St.LookRuns;
        BuildGuardrail(M, TEXT("Calder Dr north, west (fill) side"), { FVector2D(LookFaceX, LookY0), FVector2D(LookFaceX, LookY1) }, -1, St);
    }
}

// ------------------------------------------------------------------ pipe rails on the "needs rail" drops
/** P lies on (or within 80 cm of) one of the new water structures: a crossing, the dock or the rock house shelf. Their
 *  builders rail them. */
bool OnStructure(const FVector2D& P)
{
    for (const HouseCalderLandform::FWaterCrossing& X : HouseCalderLandform::WaterCrossings())
    {
        const FVector2D Ax = X.Axis.GetSafeNormal();
        const FVector2D Rel = P - X.Centre;
        const double A = FVector2D::DotProduct(Rel, Ax), L = FVector2D::DotProduct(Rel, Perp(Ax));
        if (A >= -X.Back - 80.0 && A <= X.On + 80.0 && FMath::Abs(L) <= X.Width * .5 + 80.0) return true;
    }
    const HouseCalderLandform::FPondDock& Dock = HouseCalderLandform::PondDock();
    if (SegDist(P, Dock.Land, Dock.Tip) <= Dock.Width * .5 + 80.0) return true;
    const HouseCalderLandform::FRockHouse& Rh = HouseCalderLandform::RockHouse();
    return P.X >= Rh.X0 - 80.0 && P.X <= Rh.X1 + 80.0 && P.Y >= FMath::Min(Rh.WetY, Rh.FaceY) - 80.0 && P.Y <= FMath::Max(Rh.WetY, Rh.FaceY) + 80.0;
}

void BuildRails(FMeshes& M, FStats& St)
{
    for (const HouseCalderLandform::FRoadsideRun& R : HouseCalderLandform::NeedsRail())
    {
        if (R.Points.Num() < 2) continue;
        bool bStructure = false;
        for (const FVector2D& P : R.Points) bStructure |= OnStructure(P);
        if (bStructure) { ++St.RailsOnStructures; continue; }
        // Posts 25 cm out over the drop (on the line itself across an end), every 1.5 m at most, plumb on their own ground.
        const FVector2D Dir = (R.Points.Last() - R.Points[0]).GetSafeNormal();
        const FVector2D Offset = R.Side == 0 ? FVector2D::ZeroVector : Perp(Dir) * double(R.Side) * PipeRailOffset;
        TArray<FVector2D> Line;
        for (const FVector2D& P : R.Points) Line.Add(P + Offset);
        const FLine2 L2(Line);
        const int32 Gaps = FMath::Max(1, FMath::CeilToInt(L2.Length() / PipeRailSpacing));
        TArray<FVector> Bases;
        bool bClear = true;
        for (int32 K = 0; K <= Gaps; ++K)
        {
            FVector2D P, D;
            L2.At(L2.Length() * K / Gaps, P, D);
            bClear &= !TouchesFrozen(FBox2D(P, P).ExpandBy(10.f)) && !Taken(P, 4.f, false);
            Bases.Add(At3(P, GroundAt(P)));
        }
        if (!bClear)
        {
            ++St.RailsSkipped;
            St.Notes.Add(FString::Printf(TEXT("CALDER_ROADSIDE_SKIP rail=\"%s\" reason=blocked (paving, a path, a stair, a building or a frozen zone under a post)"), *R.Name));
            continue;
        }
        FBucket& B = M.At(DetailFar, Line[0]);
        for (const FVector& P : Bases)
        {
            Tube(B, MatGalvOld, EColl::Block, P - FVector(0, 0, 20), P + FVector(0, 0, PipeRailHeights[0] + 2.f), PipePostRadius, 8, false);
            Disc(B, MatGalvOld, EColl::None, P + FVector(0, 0, PipeRailHeights[0] + 2.f), FVector::UpVector, PipePostRadius, 8);
            Built().Boxes.Add(FBox2D(FVector2D(P.X, P.Y) - FVector2D(8.f, 8.f), FVector2D(P.X, P.Y) + FVector2D(8.f, 8.f)));
            RegisterFoot(P, float(P.Z));
        }
        for (int32 K = 0; K + 1 < Bases.Num(); ++K)
            for (const float H : PipeRailHeights)
                Tube(B, MatGalvOld, EColl::Block, Bases[K] + FVector(0, 0, H), Bases[K + 1] + FVector(0, 0, H), PipeRailRadius, 8, false);
        ++St.Rails;
        St.Posts += Bases.Num();
        St.RailCm += L2.Length();
    }
}

// ------------------------------------------------------------------ ditches
/** One 2 m piece of a ditch: its stations, the cut side's normal, the lining's extent across it (W0 toward the road or walk,
 *  W1 toward the cut, both from the ditch line) and whether a lining is built there. */
struct FDitchSeg { FVector2D A, B, Cut; double W0 = 0.0, W1 = 0.0; bool bBuilt = false; };
struct FDitchSpan { FString Name; int32 Side = 0; bool bWalk = false; TArray<FDitchSeg> Segs; };

/** From P on a ditch line toward the road or walk: how far the lining may reach, to 6 cm short of the paving, a path, a
 *  stair or a frozen zone, and short of the 30 cm curb where the paving is a campus road or lot; at most Cap. */
double RoadReach(const FVector2D& P, const FVector2D& Toward, double Cap)
{
    for (double T = 0.0; T <= Cap + CurbW + 8.0; T += 4.0)
    {
        const FVector2D Q = P + Toward * T;
        if (HouseCalderTerrain::InFrozen(float(Q.X), float(Q.Y))) return FMath::Min(Cap, T - 6.0);
        if (HouseCampusLayout::PaveKindAt(Q) == EPave::Road) return FMath::Min(Cap, T - CurbW - 2.0);
        if (Taken(Q, 0.f, false)) return FMath::Min(Cap, T - 6.0);
    }
    return Cap;
}

/** From P on a ditch line toward the cut: how far the lining may reach, to 4 cm short of a wall's built face (a rock face's
 *  foot stands out in front of its line), to where the ground starts to rise (18 cm over the line's), or to anything Taken;
 *  at most Cap. */
double CutReach(const FVector2D& P, const FVector2D& Cut, const TArray<int32>& Near, double Cap)
{
    double Reach = Cap;
    double TW = 0.0;
    int32 WI = INDEX_NONE;
    if (RayWall(Near, P, Cut, Cap + 120.0, TW, WI)) Reach = FMath::Min(Reach, TW - FaceReach(WI, P + Cut * TW) - 4.0);
    const float Z0 = GroundAt(P);
    for (double T = 4.0; T <= Reach; T += 4.0)
    {
        const FVector2D Q = P + Cut * T;
        if (GroundAt(Q) - Z0 > 18.f || HouseCalderTerrain::InFrozen(float(Q.X), float(Q.Y)) || Taken(Q, 0.f, false)) return T - 4.0;
    }
    return Reach;
}

/** Weep holes in a masonry wall that stands along a ditch on its cut side, its high side away from the ditch: every 2.5 m
 *  along the stretch beside the ditch (60 cm in from the wall's ends), 30 cm over the ground at its foot (a second row
 *  1.5 m up, staggered, on a wall 3 m tall or more); only where the step is 70 cm or more and the wall's body surely
 *  stands on its high side (no paving within 75 cm there), so its face is on the line. A rusted pipe stub 3.5 cm proud,
 *  dark inside, and the wet stain it leaves down the face. */
void WeepHoles(FMeshes& M, const HouseCalderLandform::FRoadsideRun& R, const TArray<int32>& Near, uint32 Seed, FStats& St)
{
    if (R.Points.Num() < 2 || R.Side == 0) return;
    const FVector2D RunDir = (R.Points.Last() - R.Points[0]).GetSafeNormal();
    const FVector2D Cut = Perp(RunDir) * double(R.Side);
    for (const int32 WI : Near)
    {
        const HouseCalderLandform::FWall& Wl = HouseCalderLandform::Walls()[WI];
        if (IsRock(Wl.Kind)) continue;
        const FVector2D WD = Wl.B - Wl.A;
        const double WLen = WD.Size();
        if (WLen < 200.0) continue;
        const FVector2D WU = WD / WLen;
        if (FMath::Abs(FVector2D::DotProduct(WU, RunDir)) < .94) continue;
        const double OffA = FVector2D::DotProduct(Wl.A - R.Points[0], Cut), OffB = FVector2D::DotProduct(Wl.B - R.Points[0], Cut);
        if (FMath::Min(OffA, OffB) < 10.0 || FMath::Max(OffA, OffB) > 160.0) continue;
        const double T0 = FVector2D::DotProduct(R.Points[0] - Wl.A, WU), T1 = FVector2D::DotProduct(R.Points.Last() - Wl.A, WU);
        const double Lo = FMath::Max(FMath::Min(T0, T1), 60.0), Hi = FMath::Min(FMath::Max(T0, T1), WLen - 60.0);
        int32 Hole = 0;
        for (double T = Lo + WeepSpacing * .5; T <= Hi; T += WeepSpacing * .5, ++Hole)
        {
            const FVector2D P = Wl.A + WU * T;
            FVector2D Nh;
            float ZLo = 0.f, ZHi = 0.f;
            if (!StepAt(WI, P, Nh, ZLo, ZHi) || ZHi - ZLo < WeepMinStep || FVector2D::DotProduct(Nh, Cut) < .9) continue;
            if (Taken(P + Nh * 40.0, 75.f, false) || Taken(P - Nh * 15.0, 4.f, false) || TouchesFrozen(FBox2D(P, P).ExpandBy(60.f))) continue;
            // Odd half-steps are the second row's (staggered), on a tall wall only.
            const bool bUpper = Hole % 2 == 1;
            if (bUpper && ZHi - ZLo < 300.f) continue;
            const float Z = ZLo + WeepLift + (bUpper ? WeepRow : 0.f);
            if (Z + WeepRadius + 20.f > ZHi) continue;
            const FVector2D Face = -Nh;   // toward the ditch
            const FVector F3(Face.X, Face.Y, 0.0), Side3(-Face.Y, Face.X, 0.0);
            const FVector C = At3(P, Z);
            FBucket& B = M.At(DetailNear, P);
            Tube(B, MatRust, EColl::None, C - F3 * 4.0, C + F3 * 3.5, WeepRadius + .8f, 8, false);
            Tube(B, MatVoid, EColl::None, C + F3 * 3.5, C - F3 * 4.0, WeepRadius, 8, true);
            Disc(B, MatVoid, EColl::None, C - F3 * 1.0, F3, WeepRadius, 8);
            // The stain: from under the stub down to the wall's foot, tapering, 3 mm proud of the face.
            const float Wide = 4.5f + 2.5f * Hash01(Hole, WI, 7, Seed), Drift = (Hash01(Hole, WI, 8, Seed) - .5f) * 6.f;
            const FVector Top = C + F3 * .3 - FVector(0, 0, WeepRadius + 1.f), Foot = At3(P, ZLo + LiningLift + .5f) + F3 * .3 + Side3 * Drift;
            const FVector Q[4] = { Top - Side3 * Wide, Top + Side3 * Wide, Foot + Side3 * (Wide * .45f), Foot - Side3 * (Wide * .45f) };
            Poly(B, MatStain, EColl::None, Q, 4, F3);
            ++St.WeepHoles;
        }
    }
}

/** The lining of one ditch run, and its rocks; returns its pieces for the culverts. */
void BuildDitch(FMeshes& M, const HouseCalderLandform::FRoadsideRun& R, int32 RunIndex, FDitchSpan& Out, FStats& St)
{
    Out.Name = R.Name;
    Out.Side = R.Side;
    Out.bWalk = R.bWalk;
    if (R.Points.Num() < 2 || R.Side == 0) return;
    const uint32 Seed = Mix(RunIndex, R.Points.Num(), 0, 0xD17Cu);
    FBox2D RunBox(ForceInit);
    for (const FVector2D& P : R.Points) RunBox += P;
    const TArray<int32> Near = WallsNear(RunBox, 300.0);
    WeepHoles(M, R, Near, Seed, St);
    if (R.bWalk && R.bAtWall)
    {
        // A walk in a walled cut: the wall is the cut, and its weep holes (if it is masonry) drain it; no swale.
        St.Notes.Add(FString::Printf(TEXT("CALDER_ROADSIDE_SKIP ditch=\"%s\" reason=walledCut (the cut is a declared wall: weep holes where masonry)"), *R.Name));
        return;
    }
    const double Cap = R.bWalk ? WalkLiningHalf : RoadLiningHalf;
    double Built0 = 0.0;
    for (int32 K = 0; K + 1 < R.Points.Num(); ++K)
    {
        FDitchSeg& S = Out.Segs.AddDefaulted_GetRef();
        S.A = R.Points[K];
        S.B = R.Points[K + 1];
        const double Len = (S.B - S.A).Size();
        if (Len < 1.0 || Len > MaxSegment) continue;   // a gap in the run (a stair or a crossing between its stations)
        const FVector2D D = (S.B - S.A) / Len;
        S.Cut = Perp(D) * double(R.Side);
        double Rr = Cap, Cr = Cap;
        for (const double F : { 0.0, .5, 1.0 })
        {
            const FVector2D P = FMath::Lerp(S.A, S.B, F);
            Rr = FMath::Min(Rr, RoadReach(P, -S.Cut, Cap));
            Cr = FMath::Min(Cr, CutReach(P, S.Cut, Near, Cap));
        }
        S.W0 = -Rr;
        S.W1 = Cr;
        const TArray<FVector2D> Outline = RectOutline(S.A, D, S.Cut, 0.0, Len, S.W0, S.W1);
        FBox2D Box(ForceInit);
        for (const FVector2D& P : Outline) Box += P;
        S.bBuilt = S.W1 - S.W0 >= MinLining && !TouchesFrozen(Box);
        if (!S.bBuilt) { St.HeldCm += Len; continue; }
        Drape(M.At(DetailFar, (S.A + S.B) * .5), MatRipRap, Outline, LiningLift, false, .85f);
        St.LiningCm += Len;
        Built0 += Len;
        // A few mossy cobbles in the lining, toward the cut side.
        if (Hash01(RunIndex, K, 1, Seed) < (R.bWalk ? .3f : .42f))
        {
            const int32 Count = Hash01(RunIndex, K, 2, Seed) < .4f ? 2 : 1;
            for (int32 Rk = 0; Rk < Count; ++Rk)
            {
                const double Room = S.W1 - S.W0;
                float Rad = FMath::Lerp(9.f, R.bWalk ? 13.f : 18.f, Hash01(K, Rk, 3, Seed));
                Rad = FMath::Min(Rad, float(Room - 10.0) * .5f);
                if (Rad < 7.f) continue;
                const double Lat = S.W1 - Rad - 6.0 - FMath::Max(0.0, Room - 2.0 * Rad - 12.0) * .5 * Hash01(K, Rk, 4, Seed);
                const FVector2D At = FMath::Lerp(S.A, S.B, .2 + .6 * Hash01(K, Rk, 5, Seed)) + S.Cut * Lat;
                if (RockClear(At, Rad)) Boulder(M, At, Rad, FMath::Lerp(.55f, .75f, Hash01(K, Rk, 6, Seed)), Mix(RunIndex, K, Rk, Seed), St);
            }
        }
    }
    if (Built0 > 0.0) ++St.Ditches;
    else
    {
        ++St.DitchesHeld;
        St.Notes.Add(FString::Printf(TEXT("CALDER_ROADSIDE_SKIP ditch=\"%s\" reason=noRoom (a rock face's foot, the curb or a wall leaves under %.0f cm of verge)"), *R.Name, MinLining));
    }
}

// ------------------------------------------------------------------ grates (catch basins and gutter inlets)
/** The frame (Origin, the long axis Ax) lies wholly on one paving, grown by BasinMargin: a campus road or lot (or, bExterior,
 *  Calder Dr north's carriageway), not frozen, no stair or ramp; level (bFlat, at Top: HouseCampusLayout::FlatPaveTop) all
 *  over, or draped all over. */
bool GrateFits(const FVector2D& C, const FVector2D& Ax, double HalfA, double HalfB, bool bExterior, bool& bFlat, float& Top)
{
    const FVector2D Ay = Perp(Ax);
    bool bAnyFlat = false, bAnyDrape = false;
    float FlatTop = 0.f;
    for (int32 I = 0; I <= 4; ++I)
        for (int32 J = 0; J <= 2; ++J)
        {
            const FVector2D P = C + Ax * FMath::Lerp(-HalfA - BasinMargin, HalfA + BasinMargin, I / 4.0) + Ay * FMath::Lerp(-HalfB - BasinMargin, HalfB + BasinMargin, J / 2.0);
            if (bExterior ? !ExteriorRoad(P, -2.f) : HouseCampusLayout::PaveKindAt(P) != EPave::Road) return false;
            if (HouseCalderTerrain::InFrozen(float(P.X), float(P.Y)) || HouseCalderFeatures::InStair(P) || HouseCalderFeatures::InRamp(P)) return false;
            float T = 0.f;
            if (!bExterior && HouseCampusLayout::FlatPaveTop(P, T))
            {
                if (bAnyFlat && FMath::Abs(T - FlatTop) > .5f) return false;
                bAnyFlat = true;
                FlatTop = T;
            }
            else bAnyDrape = true;
        }
    if (bAnyFlat && bAnyDrape) return false;   // across a flat lot's edge onto a draped road
    bFlat = bAnyFlat;
    Top = FlatTop;
    return true;
}

/** What a grate is set in: a flat lot or road (level at its top), a draped road (its top), or a ditch's lining (the ground). */
enum class EGrateOn : uint8 { Flat, Road, Ground };

/** A cast-iron grate in a stained concrete frame, flush: the frame 3.5 mm proud, the dark opening 7 mm, the iron rim, bars
 *  and stiffener 10.5 mm (2 mm over the lining's 1.2 cm in a ditch: at most 2.5 cm over the ground). No collision. */
void Grate(FMeshes& M, const FVector2D& C, const FVector2D& Ax, double FrameA, double FrameB, double GrateA, double GrateB, EGrateOn On, float Top)
{
    const FVector2D Ay = Perp(Ax);
    FBucket& Bk = M.At(DetailNear, C);
    const float Base = On == EGrateOn::Ground ? LiningLift + .2f : 0.f;
    auto Piece = [&](EMat Mat, double A0, double A1, double B0, double B1, float Lift)
    {
        const TArray<FVector2D> O = RectOutline(C, Ax, Ay, A0, A1, B0, B1);
        if (On == EGrateOn::Flat)
        {
            const FVector Q[4] = { At3(O[0], Top + Lift), At3(O[1], Top + Lift), At3(O[2], Top + Lift), At3(O[3], Top + Lift) };
            Poly(Bk, Mat, EColl::None, Q, 4, FVector::UpVector);
        }
        else Drape(Bk, Mat, O, Base + Lift, On == EGrateOn::Road, 1.f);
    };
    Piece(MatConcrete, -FrameA, FrameA, -FrameB, -GrateB, FrameLift);
    Piece(MatConcrete, -FrameA, FrameA, GrateB, FrameB, FrameLift);
    Piece(MatConcrete, -FrameA, -GrateA, -GrateB, GrateB, FrameLift);
    Piece(MatConcrete, GrateA, FrameA, -GrateB, GrateB, FrameLift);
    Piece(MatVoid, -GrateA, GrateA, -GrateB, GrateB, VoidLift);
    constexpr double Rim = 3.0, Bar = 1.3;
    Piece(MatIron, -GrateA, GrateA, -GrateB, -GrateB + Rim, IronLift);
    Piece(MatIron, -GrateA, GrateA, GrateB - Rim, GrateB, IronLift);
    Piece(MatIron, -GrateA, -GrateA + Rim, -GrateB + Rim, GrateB - Rim, IronLift);
    Piece(MatIron, GrateA - Rim, GrateA, -GrateB + Rim, GrateB - Rim, IronLift);
    const int32 Bars = FMath::Max(3, FMath::RoundToInt(GrateA * 2.0 / 7.5) - 1);
    for (int32 K = 1; K <= Bars; ++K)
    {
        const double A = -GrateA + 2.0 * GrateA * K / (Bars + 1);
        Piece(MatIron, A - Bar, A + Bar, -GrateB + Rim, GrateB - Rim, IronLift);
    }
    Piece(MatIron, -GrateA + Rim, GrateA - Rim, -1.2, 1.2, IronLift);
}

/** The nearest placement within BasinSearch of At (on a 10 cm grid) whose frame fits, with its long axis along X or Y,
 *  whichever needs the smaller move. */
bool PlaceGrate(const FVector2D& At, double HalfA, double HalfB, bool bExterior, FVector2D& OutC, FVector2D& OutAx, bool& bFlat, float& Top)
{
    struct FTry { FVector2D Off; double D = 0.0; };
    TArray<FTry> Tries;
    for (double DX = -BasinSearch; DX <= BasinSearch + .1; DX += 10.0)
        for (double DY = -BasinSearch; DY <= BasinSearch + .1; DY += 10.0)
        {
            const double D = FMath::Sqrt(DX * DX + DY * DY);
            if (D <= BasinSearch + .1) Tries.Add({ FVector2D(DX, DY), D });
        }
    Tries.Sort([](const FTry& L, const FTry& Rt) { return L.D != Rt.D ? L.D < Rt.D : (L.Off.Y != Rt.Off.Y ? L.Off.Y < Rt.Off.Y : L.Off.X < Rt.Off.X); });
    double Best = DBL_MAX;
    for (const FVector2D& Ax : { FVector2D(1.0, 0.0), FVector2D(0.0, 1.0) })
        for (const FTry& T : Tries)
        {
            if (T.D >= Best) break;
            bool bF = false;
            float Tp = 0.f;
            if (!GrateFits(At + T.Off, Ax, HalfA, HalfB, bExterior, bF, Tp)) continue;
            Best = T.D;
            OutC = At + T.Off;
            OutAx = Ax;
            bFlat = bF;
            Top = Tp;
            break;
        }
    return Best < DBL_MAX;
}

void BuildBasins(FMeshes& M, FStats& St)
{
    for (const HouseCalderLandform::FCatchBasin& Cb : HouseCalderLandform::CatchBasins())
    {
        const FVector2D At(Cb.At.X, Cb.At.Y);
        FVector2D C, Ax;
        bool bFlat = false;
        float Top = 0.f;
        if (!PlaceGrate(At, BasinFrameA, BasinFrameB, false, C, Ax, bFlat, Top))
        {
            ++St.BasinsSkipped;
            St.Notes.Add(FString::Printf(TEXT("CALDER_ROADSIDE_SKIP basin=\"%s\" at=(%.0f,%.0f) reason=noPaving (no place within %.0f cm where its frame lies wholly on one road or lot)"), *Cb.Name, At.X, At.Y, BasinSearch));
            continue;
        }
        Grate(M, C, Ax, BasinFrameA, BasinFrameB, BasinGrateA, BasinGrateB, bFlat ? EGrateOn::Flat : EGrateOn::Road, Top);
        ++St.Basins;
    }
}

// ------------------------------------------------------------------ culverts
/** A dark pipe mouth in a headwall's face (its plane through FaceC, facing Facing): a corrugated pipe's rusted lip 6 cm proud,
 *  black inside, its invert at Invert. */
void Mouth(FBucket& B, const FVector2D& FaceC, const FVector2D& Facing, float Invert, float Diameter)
{
    const FVector N3(Facing.X, Facing.Y, 0.0);
    const FVector Mc = At3(FaceC, Invert + Diameter * .5f);
    Tube(B, MatRust, EColl::Block, Mc - N3 * 3.0, Mc + N3 * 6.0, Diameter * .5f + 2.f, 14, false);
    Tube(B, MatVoid, EColl::None, Mc + N3 * 6.0, Mc - N3 * 3.0, Diameter * .5f, 14, true);
    Disc(B, MatVoid, EColl::None, Mc + N3 * .4, N3, Diameter * .5f, 14);
}

/** The inlet at a road ditch's low end: a headwall across the lining (its face just past the lining's end, facing up the
 *  ditch), as wide as the lining (it abuts the cut, and keeps off the curb), its top 22 cm over the pipe and never 75 cm or
 *  more over the ground in front; a concrete cap. False where the lining is too narrow for a 25 cm pipe or the ground under
 *  it is not open. */
bool BuildInlet(FMeshes& M, const FVector2D& End, const FVector2D& UpDitch, const FVector2D& Cut, double W0, double W1, EMat WallMat, FStats& St)
{
    const float Dia = FMath::Min(InletPipe, float(W1 - W0) - 30.f);
    if (Dia < 25.f) return false;
    if (!RectClear(End, Cut, UpDitch, W0, W1, -1.0 - HeadwallThick, -1.0, 2.f, false)) return false;
    float GMin = FLT_MAX, GMax = -FLT_MAX;
    for (const double L : { W0, (W0 + W1) * .5, W1 })
        for (const double T : { -1.0, -1.0 - HeadwallThick })
        {
            const float G = GroundAt(End + Cut * L + UpDitch * T);
            GMin = FMath::Min(GMin, G);
            GMax = FMath::Max(GMax, G);
        }
    const FVector2D FaceC = End + Cut * ((W0 + W1) * .5) - UpDitch * 1.0;
    const float GMouth = GroundAt(FaceC);
    const float Invert = GMouth + 1.5f;
    const float Top = FMath::Max(Invert + Dia + 22.f, GMax + 8.f);
    if (Top - GMouth >= 75.f) return false;
    FBucket& B = M.At(DetailFar, FaceC);
    PBox(B, WallMat, EColl::Block, End, Cut, UpDitch, W0, W1, -1.0 - HeadwallThick, -1.0, GMin - 40.f, Top - 5.f, AllFaces, .55f, .95f);
    PBox(B, MatConcrete, EColl::Walk, End, Cut, UpDitch, W0, W1, -1.0 - HeadwallThick - 3.0, -1.0 + 3.0, Top - 5.f, Top, AllFaces, .8f, 1.f);
    Mouth(B, FaceC, UpDitch, Invert, Dia);
    FBox2D Box(ForceInit);
    for (const FVector2D& P : RectOutline(End, Cut, UpDitch, W0, W1, -1.0 - HeadwallThick - 3.0, 9.0)) Box += P;
    Built().Boxes.Add(Box);
    RegisterFoot(At3(FaceC, GMouth), GMouth);
    ++St.Headwalls;
    ++St.Inlets;
    return true;
}

/** Where the lining at a ditch's low end is too narrow for a headwall (under 55 cm): a drop inlet in it, its grate 44 cm long
 *  across the lining's width in a 60 cm concrete frame, set on the stones 50 cm up the ditch from the lining's end. */
bool BuildDropInlet(FMeshes& M, const FVector2D& End, const FVector2D& UpDitch, const FVector2D& Cut, double W0, double W1, FStats& St)
{
    const double Half = (W1 - W0) * .5;
    if (Half < 14.0) return false;
    const FVector2D C = End + Cut * ((W0 + W1) * .5) + UpDitch * 50.0;
    if (TouchesFrozen(FBox2D(C, C).ExpandBy(40.f))) return false;
    Grate(M, C, UpDitch, 30.0, Half, 22.0, Half - 6.0, EGrateOn::Ground, 0.f);
    ++St.DropInlets;
    return true;
}

/** Where a rock face leaves no ditch: a gutter grate in the road on the ditch side, on the culvert's line, 34 cm in from the
 *  carriageway's edge. */
bool BuildGutterInlet(FMeshes& M, const HouseCalderLandform::FDitchCulvert& Cv, const FVector2D& ToDitch, FStats& St)
{
    const FVector2D Centre = Cv.Centre;
    bool bExterior = false;
    double Edge = -1.0;
    for (double T = 0.0; T <= Cv.Length * .5 + 200.0; T += 5.0)
    {
        const FVector2D Q = Centre + ToDitch * T;
        const bool bCampus = HouseCampusLayout::PaveKindAt(Q) == EPave::Road, bExt = ExteriorRoad(Q, 0.f);
        if (bCampus || bExt) { Edge = T; bExterior = bExt && !bCampus; }
        else if (Edge >= 0.0) break;
    }
    if (Edge < GutterOffset + GutterFrameB) return false;
    const FVector2D C = Centre + ToDitch * (Edge - GutterOffset);
    const FVector2D Ax = Perp(ToDitch);   // its long side along the kerb
    bool bFlat = false;
    float Top = 0.f;
    if (!GrateFits(C, Ax, GutterFrameA, GutterFrameB - BasinMargin, bExterior, bFlat, Top)) return false;
    Grate(M, C, Ax, GutterFrameA, GutterFrameB, GutterGrateA, GutterGrateB, bFlat ? EGrateOn::Flat : EGrateOn::Road, Top);
    ++St.GutterInlets;
    return true;
}

/** The outlet across the road: on the first open lawn past the paving (and its curb) on that side, moved along the road by up
 *  to 2 m and out by up to 1 m to find room. Its footprint (the headwall, its wings and the apron) must be open, with no
 *  declared wall in it or between it and the road; its ground no more than 40 cm over the verge (52 over the road's top) and
 *  no more than 3 m under it; and the ground ahead of it may rise no more than 35 cm over the apron (no mouth facing into a
 *  bank). A headwall 210 cm wide with 95 cm wings at 40 degrees, a concrete cap, the mouth, and a rip-rap apron with
 *  boulders. */
bool BuildOutlet(FMeshes& M, const HouseCalderLandform::FDitchCulvert& Cv, const FVector2D& Out, EMat WallMat, uint32 Seed, FStats& St)
{
    const FVector2D Lat = Perp(Out);
    const FVector2D Centre = Cv.Centre;
    double Clear = -1.0;
    for (double T = 0.0; T <= Cv.Length * .5 + 400.0; T += 5.0)
        if (!Taken(Centre + Out * T, 40.f, true) && !HouseCalderTerrain::InFrozen(float((Centre + Out * T).X), float((Centre + Out * T).Y))) { Clear = T; break; }
    if (Clear < 0.0) return false;
    float RoadTop = HouseCalderTerrain::SurfaceZ(float(Centre.X), float(Centre.Y), EPave::Road);
    HouseCampusLayout::FlatPaveTop(Centre, RoadTop);
    constexpr double WingLen = 95.0, WingThick = 25.0;
    const double WingSin = FMath::Sin(FMath::DegreesToRadians(40.0)), WingCos = FMath::Cos(FMath::DegreesToRadians(40.0));
    const double SpanHalf = OutletHalfWidth + WingLen * WingSin;
    for (const double Push : { 0.0, 50.0, 100.0 })
        for (const double Shift : { 0.0, 100.0, -100.0, 200.0, -200.0 })
        {
            const double T0 = Clear + Push;
            const FVector2D Origin = Centre + Lat * Shift;
            const FVector2D F = Origin + Out * (T0 + HeadwallThick);
            if (!RectClear(Origin, Out, Lat, T0, T0 + HeadwallThick + ApronLength, -SpanHalf, SpanHalf, 5.f, false)) continue;
            FBox2D Foot(ForceInit);
            for (const FVector2D& P : RectOutline(Origin, Out, Lat, T0, T0 + HeadwallThick + ApronLength, -SpanHalf, SpanHalf)) Foot += P;
            bool bWall = false;
            for (const int32 WI : WallsNear(Foot, 30.0))
            {
                const HouseCalderLandform::FWall& Wl = HouseCalderLandform::Walls()[WI];
                FVector2D Nh;
                float Lo = 0.f, Hi = 0.f;
                const FVector2D Mid = (Wl.A + Wl.B) * .5;
                if (StepAt(WI, Mid, Nh, Lo, Hi) || StepAt(WI, Wl.A + (Wl.B - Wl.A) * .1, Nh, Lo, Hi) || StepAt(WI, Wl.A + (Wl.B - Wl.A) * .9, Nh, Lo, Hi)) { bWall = true; break; }
            }
            double TW = 0.0;
            int32 WI2 = INDEX_NONE;
            FBox2D Pipe(ForceInit);
            Pipe += Centre;
            Pipe += F;
            if (bWall || RayWall(WallsNear(Pipe, 10.0), Centre, Out, T0 + HeadwallThick, TW, WI2)) continue;
            const float ZF = GroundAt(F);
            if (ZF > RoadTop + 52.f || ZF < RoadTop - 300.f) continue;   // (the road's top is 12 cm under its verge)
            if (GroundAt(F + Out * ApronLength) - ZF > OutletRiseMax) continue;
            // Build it.
            float GMin = FLT_MAX, GMax = -FLT_MAX;
            for (const double A : { T0, T0 + HeadwallThick })
                for (const double B : { -double(OutletHalfWidth), 0.0, double(OutletHalfWidth) })
                {
                    const float G = GroundAt(Origin + Out * A + Lat * B);
                    GMin = FMath::Min(GMin, G);
                    GMax = FMath::Max(GMax, G);
                }
            const float Invert = ZF + 2.f;
            const float Top = FMath::Max(Invert + OutletPipe + 26.f, GMax + 10.f);
            FBucket& Bk = M.At(DetailFar, F);
            PBox(Bk, WallMat, EColl::Block, Origin, Out, Lat, T0, T0 + HeadwallThick, -OutletHalfWidth, OutletHalfWidth, GMin - 40.f, Top - 6.f, AllFaces, .55f, .95f);
            PBox(Bk, MatConcrete, EColl::Walk, Origin, Out, Lat, T0 - 3.0, T0 + HeadwallThick + 4.0, -OutletHalfWidth - 4.0, OutletHalfWidth + 4.0, Top - 6.f, Top, AllFaces, .8f, 1.f);
            for (const double Sg : { -1.0, 1.0 })
            {
                const FVector2D W = Out * WingCos + Lat * (Sg * WingSin);
                const FVector2D Pw = Perp(W);
                const FVector2D R0 = F + Lat * (Sg * (OutletHalfWidth - WingThick * .5)), R1 = R0 + W * WingLen;
                const float G0 = GroundAt(R0), G1 = GroundAt(R1);
                const FVector Low[4] = { At3(R0 - Pw * (WingThick * .5), FMath::Min(G0, GMin) - 40.f), At3(R1 - Pw * (WingThick * .5), G1 - 40.f), At3(R1 + Pw * (WingThick * .5), G1 - 40.f), At3(R0 + Pw * (WingThick * .5), FMath::Min(G0, GMin) - 40.f) };
                const FVector High[4] = { At3(R0 - Pw * (WingThick * .5), Top - 8.f), At3(R1 - Pw * (WingThick * .5), G1 + 18.f), At3(R1 + Pw * (WingThick * .5), G1 + 18.f), At3(R0 + Pw * (WingThick * .5), Top - 8.f) };
                Hexa(Bk, WallMat, EColl::Block, Low, High, AllFaces, .55f, .9f);
                FBox2D WB(ForceInit);
                for (const FVector& P : Low) WB += FVector2D(P.X, P.Y);
                Built().Boxes.Add(WB.ExpandBy(4.f));
            }
            Mouth(Bk, F, Out, Invert, OutletPipe);
            // The apron: rip-rap fanning out from the face, and boulders along its sides and toe.
            const TArray<FVector2D> Apron = { F + Lat * (OutletHalfWidth - 30.0), F + Out * ApronLength + Lat * (OutletHalfWidth + 30.0),
                F + Out * ApronLength - Lat * (OutletHalfWidth + 30.0), F - Lat * (OutletHalfWidth - 30.0) };
            Drape(Bk, MatRipRap, Apron, 1.5f, false, .8f);
            for (int32 K = 0; K < 7; ++K)
            {
                const double Along = FMath::Lerp(40.0, ApronLength - 15.0, double(Hash01(K, 1, 0, Seed)));
                const double Half = FMath::Lerp(OutletHalfWidth - 30.0, OutletHalfWidth + 30.0, Along / ApronLength);
                const double Across = (K % 2 == 0 ? 1.0 : -1.0) * Half * FMath::Lerp(.55, .95, double(Hash01(K, 2, 0, Seed)));
                const float Rad = FMath::Lerp(15.f, 30.f, Hash01(K, 3, 0, Seed));
                const FVector2D At = F + Out * Along + Lat * Across;
                if (RockClear(At, Rad)) Boulder(M, At, Rad, FMath::Lerp(.55f, .72f, Hash01(K, 4, 0, Seed)), Mix(K, 9, 0, Seed), St);
            }
            FBox2D Box(ForceInit);
            for (const FVector2D& P : RectOutline(Origin, Out, Lat, T0 - 3.0, T0 + HeadwallThick + 10.0, -OutletHalfWidth - 4.0, OutletHalfWidth + 4.0)) Box += P;
            Built().Boxes.Add(Box);
            RegisterFoot(At3(F, ZF), ZF);
            ++St.Headwalls;
            ++St.Outlets;
            return true;
        }
    return false;
}

void BuildCulverts(FMeshes& M, const TArray<FDitchSpan>& Spans, FStats& St)
{
    for (int32 CI = 0; CI < HouseCalderLandform::DitchCulverts().Num(); ++CI)
    {
        const HouseCalderLandform::FDitchCulvert& Cv = HouseCalderLandform::DitchCulverts()[CI];
        const FVector2D ToDitch = Cv.Axis.GetSafeNormal() * double(Cv.Side == 0 ? 1 : Cv.Side);
        // 1930s mortared stone on the old county roads; stained concrete on the campus service roads.
        const bool bConcrete = Cv.Ditch.Contains(TEXT("Plant")) || Cv.Ditch.Contains(TEXT("Ashby"));
        const EMat WallMat = bConcrete ? MatConcrete : MatStone;
        // The inlet at the ditch's end nearest the culvert (its low end).
        const FDitchSpan* Span = nullptr;
        for (const FDitchSpan& S : Spans) if (!S.bWalk && S.Side == Cv.Side && S.Name == Cv.Ditch && S.Segs.Num()) { Span = &S; break; }
        bool bInlet = false;
        if (Span)
        {
            const bool bFirst = (Span->Segs[0].A - Cv.Centre).SizeSquared() <= (Span->Segs.Last().B - Cv.Centre).SizeSquared();
            const FDitchSeg& Seg = bFirst ? Span->Segs[0] : Span->Segs.Last();
            const FVector2D End = bFirst ? Seg.A : Seg.B;
            const FVector2D UpDitch = (bFirst ? Seg.B - Seg.A : Seg.A - Seg.B).GetSafeNormal();
            if (Seg.bBuilt) bInlet = BuildInlet(M, End, UpDitch, Seg.Cut, Seg.W0, Seg.W1, WallMat, St) || BuildDropInlet(M, End, UpDitch, Seg.Cut, Seg.W0, Seg.W1, St);
        }
        if (!bInlet && !BuildGutterInlet(M, Cv, ToDitch, St))
            St.Notes.Add(FString::Printf(TEXT("CALDER_ROADSIDE_SKIP inlet=\"%s\" reason=noRoom (no lining at the ditch's end and no carriageway edge for a gutter grate)"), *Cv.Ditch));
        if (!BuildOutlet(M, Cv, -ToDitch, WallMat, Mix(CI, 77, 0, 0xC0u), St))
        {
            ++St.OutletsSkipped;
            St.Notes.Add(FString::Printf(TEXT("CALDER_ROADSIDE_SKIP outlet=\"%s\" reason=noOpenGround (paving, a building, a wall, a frozen zone or rising ground across the road)"), *Cv.Under));
        }
    }
}

// ------------------------------------------------------------------ materials, meshes and components
// (The masters are looked up on every build, not cached in statics, as HouseCalderFeatures does: a raw pointer kept across
// worlds would dangle once the garbage collector unloads an asset nothing references between two loads of the map.)
UMaterialInterface* RockMaterial(UObject* Outer, const TCHAR* Id, float Tile, float Moss, const FLinearColor& Tint)
{
    UMaterialInterface* const Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderRock.M_CalderRock"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Base) return nullptr;
    UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Outer);
    for (const TCHAR* Kind : { TEXT("A"), TEXT("N"), TEXT("ARM") })
    {
        const FString Path = FString::Printf(TEXT("/Game/SeniorSendOff/Calder/Textures/%s/T_%s_%s.T_%s_%s"), Id, Id, Kind, Id, Kind);
        if (UTexture* Tex = LoadObject<UTexture>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
            Mid->SetTextureParameterValue(FCString::Strcmp(Kind, TEXT("A")) == 0 ? FName(TEXT("Albedo")) : (FCString::Strcmp(Kind, TEXT("N")) == 0 ? FName(TEXT("Normal")) : FName(TEXT("ARM"))), Tex);
        else UE_LOG(LogTemp, Warning, TEXT("CALDER_ROADSIDE texture %s is missing"), *Path);
    }
    Mid->SetScalarParameterValue(TEXT("TileSize"), Tile);
    Mid->SetScalarParameterValue(TEXT("MossAmount"), Moss);
    Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
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
        UE_LOG(LogTemp, Warning, TEXT("CALDER_ROADSIDE M_CalderRock is missing; the stone, timber and rip-rap fall back to the default material"));
    Out[MatGalv] = PlainMaterial(Outer, FLinearColor(.30f, .31f, .305f), .64f, .5f);         // galvanising gone dull grey
    Out[MatGalvOld] = PlainMaterial(Outer, FLinearColor(.215f, .21f, .195f), .76f, .32f);    // an older panel, the zinc chalky and dark
    Out[MatRust] = PlainMaterial(Outer, FLinearColor(.13f, .052f, .02f), .9f, .12f);
    Out[MatTimber] = RockMaterial(Outer, TEXT("rough_wood"), 140.f, .22f, FLinearColor(.5f, .45f, .4f));        // creosoted, weathered grey-brown
    Out[MatConcrete] = RockMaterial(Outer, TEXT("cracked_concrete"), 170.f, .3f, FLinearColor(.74f, .72f, .66f));  // stained
    Out[MatStone] = RockMaterial(Outer, TEXT("mossy_stone_wall"), 160.f, .5f, FLinearColor(.84f, .83f, .78f));     // 1930s mortared stone
    Out[MatRipRap] = RockMaterial(Outer, TEXT("river_small_rocks"), 110.f, .38f, FLinearColor(.6f, .58f, .52f));
    Out[MatBoulder] = RockMaterial(Outer, TEXT("lichen_rock"), 160.f, .55f, FLinearColor(.8f, .8f, .76f));
    Out[MatIron] = PlainMaterial(Outer, FLinearColor(.045f, .04f, .035f), .72f, .45f);      // cast iron, rust in its pores
    Out[MatVoid] = PlainMaterial(Outer, FLinearColor(.004f, .004f, .004f), 1.f, 0.f);
    Out[MatStain] = PlainMaterial(Outer, FLinearColor(.03f, .034f, .022f), .96f, 0.f);       // damp algae and lime
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
        Used.Add(FStaticMaterial(Mats[MI], MatSlots[MI]));
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
            Colors[Instance] = FVector4f(1.f, 1.f, 1.f, D.Ao[K]);   // M_CalderRock: A is the cavity occlusion
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

int32 HouseCalderRoadside::Build(AActor* Owner)
{
    namespace RI = HouseCalderRoadsideImpl;
    RI::Built() = RI::FBuilt();
    if (!Owner || !Owner->GetWorld()) return 0;
    if (!HouseCalderFeatures::Active())
    {
        UE_LOG(LogTemp, Display, TEXT("CALDER_ROADSIDE off (the Calder terrain is off or its relief is flat)"));
        return 0;
    }
    const double Start = FPlatformTime::Seconds();
    RI::FMeshes Meshes;
    RI::FStats St;
    RI::BuildGuardrails(Meshes, St);
    RI::BuildRails(Meshes, St);
    TArray<RI::FDitchSpan> Spans;
    const TArray<HouseCalderLandform::FRoadsideRun>& Runs = HouseCalderLandform::Ditches();
    for (int32 K = 0; K < Runs.Num(); ++K) RI::BuildDitch(Meshes, Runs[K], K, Spans.AddDefaulted_GetRef(), St);
    RI::BuildBasins(Meshes, St);
    RI::BuildCulverts(Meshes, Spans, St);

    const bool bDraw = Owner->GetWorld()->GetNetMode() != NM_DedicatedServer;
    UMaterialInterface* Mats[RI::MatCount] = {};
    if (bDraw) RI::Materials(Owner, Mats);
    int32 Drawn = 0, Parts = 0, CollTris = 0;
    for (int32 Detail = 0; Detail < RI::NumDetail; ++Detail)
        for (int32 Tile = 0; Tile < RI::NumTiles; ++Tile)
        {
            const RI::FBucket& B = Meshes.Buckets[Detail * RI::NumTiles + Tile];
            const FString Prefix = FString::Printf(TEXT("CalderRoadside_%02d"), Tile);
            if (Detail == RI::DetailFar)
            {
                RI::AddCollision(Owner, B.Walk, Prefix + TEXT("_Collision"), false);
                RI::AddCollision(Owner, B.Block, Prefix + TEXT("_CollisionUnwalkable"), true);
            }
            CollTris += (B.Walk.I.Num() + B.Block.I.Num()) / 3;
            if (!bDraw) continue;
            int32 Tris = 0;
            const FString Name = Prefix + TEXT("_") + RI::DetailNames[Detail];
            UStaticMesh* Mesh = RI::BuildMesh(B, Mats, Name, Tris);
            if (!Mesh) continue;
            UStaticMeshComponent* Draw = NewObject<UStaticMeshComponent>(Owner, *Name);
            Draw->SetMobility(EComponentMobility::Static);
            Draw->SetStaticMesh(Mesh);
            Draw->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Draw->SetCastShadow(false);
            Draw->bAffectDistanceFieldLighting = false;
            Draw->SetCullDistance(Detail == RI::DetailNear ? RI::NearCull : RI::FarCull);
            Draw->RegisterComponent();
            Drawn += Tris;
            ++Parts;
        }
    for (const FString& Note : St.Notes) UE_LOG(LogTemp, Display, TEXT("%s"), *Note);
    UE_LOG(LogTemp, Display, TEXT("CALDER_ROADSIDE guardrailM=%.0f posts=%d rails=%d ditches=%d basins=%d headwalls=%d tris=%d ms=%.0f guardRuns=%d lookRuns=%d guardSkipped=%d terminals=%d railM=%.0f railsOnStructures=%d railsSkipped=%d liningM=%.0f heldM=%.0f ditchesHeld=%d rocks=%d weepHoles=%d basinsSkipped=%d inlets=%d dropInlets=%d gutterInlets=%d outlets=%d outletsSkipped=%d meshes=%d collisionTris=%d"),
        St.GuardCm / 100.0, St.Posts, St.Rails, St.Ditches, St.Basins, St.Headwalls, Drawn, (FPlatformTime::Seconds() - Start) * 1000.0,
        St.GuardRuns, St.LookRuns, St.GuardSkipped, St.Terminals, St.RailCm / 100.0, St.RailsOnStructures, St.RailsSkipped, St.LiningCm / 100.0, St.HeldCm / 100.0,
        St.DitchesHeld, St.Rocks, St.WeepHoles, St.BasinsSkipped, St.Inlets, St.DropInlets, St.GutterInlets, St.Outlets, St.OutletsSkipped, Parts, CollTris);
    return Drawn;
}

bool HouseCalderRoadside::Occupied(const FVector2D& P, float Pad)
{
    for (const FBox2D& B : HouseCalderRoadsideImpl::Built().Boxes) if (B.ExpandBy(Pad).IsInside(P)) return true;
    return false;
}

void HouseCalderRoadside::FootPoints(TArray<FVector>& Out)
{
    Out.Append(HouseCalderRoadsideImpl::Built().Feet);
}
