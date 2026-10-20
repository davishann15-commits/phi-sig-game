// The Calder landform's edges, overlooks and Crow Ledge (see HouseCalderEdges.h).
#include "HouseCalderEdges.h"
#include "HouseCalderMapTrees.h"
#include "HouseBlackHallMesh.h"
#include "HouseCalderFarLand.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderFootPoints.h"
#include "HouseCalderLandform.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderTerrainTiles.h"
#include "HouseDetail.h"
#include "HouseExteriorNature.h"
#include "HouseExteriorSiteLayout.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
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

namespace HouseCalderEdgesImpl
{
using HouseCalderLandform::EEdgeClosure;

constexpr int32 TilesPerSide = int32(2 * HouseCalderTerrain::Half / HouseCalderTerrainTiles::TileSize);   // 5, as the terrain
constexpr double EdgeLine = 31700.0;        // the closures' line: 3 m in from the square's edge
constexpr double SquareEdge = 32000.0;
constexpr double GroundClamp = 31995.0;     // ground is read inside the square only
constexpr double ColumnStep = 150.0;        // a band's columns along it
constexpr int32 FaceRows = 22, BackRows = 8; // rows up its visible (inner) face and up its back
constexpr float FootDepth = 40.f;           // a band reaches this far under the lowest ground under it ...
constexpr float MinRise = 210.f;            // ... and its top stands at least this far over the highest (2 m, and a margin)
constexpr double FaceLean = .105;           // tan(6 deg): the faces lean back
constexpr double CornerRunOn = 150.0;       // bands that meet at a corner each run on this far past it
constexpr double FreeRunOn = 60.0;          // a free end runs on this far (into the hedge's end where one stops there)
constexpr double ForestHideRadius = 4000.0; // distant-forest trees this close (in plan) to the Crow trail or the ledge are hidden
constexpr double CrowViewOpenToY = 19800.0;  // and those in the P8 view north of this (the old property's north edge)
constexpr float RailTop = 100.f, RailMid = 62.f, RailLow = 28.f;   // the trail fence's rails (the low one: no 36 cm gap under it)
constexpr float RailDrop = 75.f;            // a drop this deep beside the trail gets the fence

enum EMat : uint8 { MatSandstone, MatShale, MatFresh, MatBoulder, MatDressed, MatLedge, MatBronze, MatVerdigris, MatPaint, MatRust, MatRubber, MatSteel, MatTimber, MatIron, MatCount };
const TCHAR* const MatSlots[MatCount] = { TEXT("Sandstone"), TEXT("Shale"), TEXT("FreshRock"), TEXT("Boulder"), TEXT("Dressed"), TEXT("Ledge"), TEXT("Bronze"),
    TEXT("Verdigris"), TEXT("Paint"), TEXT("Rust"), TEXT("Rubber"), TEXT("Steel"), TEXT("Timber"), TEXT("Iron") };
/** Which collision a face joins: none, the walkable mesh, or the Unwalkable one (rock, rails, the furniture's hulls). */
enum class EColl : uint8 { None, Walk, Block };
/** The buckets: one per 128 m tile for the rock bands, then one per furnished spot. */
enum EArea : int32 { AreaOverlook = TilesPerSide * TilesPerSide, AreaBench, AreaCrow, AreaCount };

// ------------------------------------------------------------------ ground and noise
double GroundAt(const FVector2D& P)
{
    return HouseExteriorSite::GroundHeight(float(FMath::Clamp(P.X, -GroundClamp, GroundClamp)), float(FMath::Clamp(P.Y, -GroundClamp, GroundClamp)));
}
/** What a prop's foot stands on: the ground plus the paving there (HouseCalderTerrain::SurfaceZ: a road's carriageway is 12 cm
 *  under the grade datum). Calder Dr north of Y 19800 is the exterior's own side street (its asphalt ExPatch, X 8700..10300 to
 *  the edge, Grade - 12 on the relief), which the plan's paving does not list, so it is read here. */
double SurfaceAt(const FVector2D& P)
{
    if (P.X >= 8700.0 && P.X <= 10300.0 && P.Y >= 19800.0) return GroundAt(P) - 12.0;
    return HouseCalderTerrain::SurfaceZ(float(FMath::Clamp(P.X, -GroundClamp, GroundClamp)), float(FMath::Clamp(P.Y, -GroundClamp, GroundClamp)));
}
/** Registers a prop's foot for -CalderTerrainReview (the foot within -10..+3 cm of the surface under it). */
void Foot(const TCHAR* Who, const FVector2D& P, double FootZ)
{
    HouseCalderFootPoints::Register(Who, FVector(P.X, P.Y, FootZ), float(SurfaceAt(P)));
}
double Cross2(const FVector2D& U, const FVector2D& V) { return U.X * V.Y - U.Y * V.X; }
FVector At3(const FVector2D& P, double Z) { return FVector(P.X, P.Y, Z); }
FVector2D Right2(const FVector2D& Facing) { return FVector2D(Facing.Y, -Facing.X); }   // the right hand of someone facing Facing
FVector2D YawDir(double YawDeg) { return FVector2D(FMath::Cos(FMath::DegreesToRadians(YawDeg)), FMath::Sin(FMath::DegreesToRadians(YawDeg))); }

double SegDist(const FVector2D& P, const FVector2D& A, const FVector2D& B)
{
    const FVector2D AB = B - A;
    const double L2 = AB.SizeSquared();
    const double T = L2 > 1e-9 ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.0, 1.0) : 0.0;
    return (P - (A + AB * T)).Size();
}

double PolylineDist(const FVector2D& P, const TArray<FVector2D>& Line, bool bClosed)
{
    double Best = DBL_MAX;
    const int32 Num = Line.Num();
    for (int32 K = 0; K + 1 < Num + (bClosed ? 1 : 0); ++K) Best = FMath::Min(Best, SegDist(P, Line[K], Line[(K + 1) % Num]));
    if (Num == 1) Best = (P - Line[0]).Size();
    return Best;
}

double BoxDist(const FVector2D& P, const FBox2D& Box)
{
    const double DX = FMath::Max3(Box.Min.X - P.X, 0.0, P.X - Box.Max.X), DY = FMath::Max3(Box.Min.Y - P.Y, 0.0, P.Y - Box.Max.Y);
    return FMath::Sqrt(DX * DX + DY * DY);
}

bool InPolygon(const FVector2D& P, const TArray<FVector2D>& Poly)
{
    bool bIn = false;
    for (int32 K = 0, J = Poly.Num() - 1; K < Poly.Num(); J = K++)
        if ((Poly[K].Y > P.Y) != (Poly[J].Y > P.Y) && P.X < (Poly[J].X - Poly[K].X) * (P.Y - Poly[K].Y) / (Poly[J].Y - Poly[K].Y) + Poly[K].X) bIn = !bIn;
    return bIn;
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
float Hash01(int32 X, int32 Y, uint32 Seed) { return float(Mix(X, Y, 0, Seed) & 0xFFFFFFu) / float(0xFFFFFF); }

/** Smooth 1D value noise, -1..1. */
float Value1(double T, uint32 Seed)
{
    const int32 I = FMath::FloorToInt(T);
    const float F = float(T - I), Sm = F * F * (3.f - 2.f * F);
    return FMath::Lerp(Hash01(I, 0, Seed), Hash01(I + 1, 0, Seed), Sm) * 2.f - 1.f;
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
    return FMath::Lerp(FMath::Lerp(X00, X10, SY), FMath::Lerp(X01, X11, SY), SZ) * 2.f - 1.f;
}

// ------------------------------------------------------------------ meshes (per tile or spot)
struct FDrawPart { TArray<FVector> V, N; TArray<float> Ao; TArray<int32> I; };
struct FCollPart { TArray<FVector> V; TArray<int32> I; };
struct FBucket { FDrawPart Draw[MatCount]; FCollPart Walk, Block; };

struct FMeshes
{
    TArray<FBucket> Buckets;
    FMeshes() { Buckets.SetNum(AreaCount); }
    static int32 TileOf(const FVector2D& P)
    {
        const double HalfSpan = HouseCalderTerrain::Half, Tile = HouseCalderTerrainTiles::TileSize;
        const int32 TX = FMath::Clamp(FMath::FloorToInt((P.X + HalfSpan) / Tile), 0, TilesPerSide - 1);
        const int32 TY = FMath::Clamp(FMath::FloorToInt((P.Y + HalfSpan) / Tile), 0, TilesPerSide - 1);
        return TY * TilesPerSide + TX;
    }
    FBucket& Rock(const FVector2D& P) { return Buckets[TileOf(P)]; }
    FBucket& Area(EArea A) { return Buckets[int32(A)]; }
};

FString BucketName(int32 Index)
{
    if (Index < AreaOverlook) return FString::Printf(TEXT("CalderEdgeRock_%02d"), Index);
    return Index == AreaOverlook ? FString(TEXT("CalderOverlook")) : Index == AreaBench ? FString(TEXT("CalderPondBench")) : FString(TEXT("CalderCrowLedge"));
}
/** Cull distances: the rock bands show from anywhere in the square (it is 905 m corner to corner); the furniture from near. */
float BucketCull(int32 Index)
{
    if (Index < AreaOverlook) return 150000.f;
    return Index == AreaOverlook ? 12000.f : Index == AreaBench ? 9000.f : 15000.f;
}

/** A convex polygon facing Out, fanned from its first point (FHallMesh's winding rule, as HouseCalderFeatures uses it).
 *  Normals: per point, or null for the polygon's own. Mat == MatCount: collision only. */
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
        FCollPart& C = Coll == EColl::Walk ? B.Walk : B.Block;
        const int32 Base0 = C.V.Num();
        for (int32 K = 0; K < Num; ++K) C.V.Add(P[K]);
        Fan(C.I, Base0);
    }
}

void Quad(FBucket& B, EMat Mat, EColl Coll, const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const FVector& Out, float AoAll = 1.f)
{
    const FVector Q[4] = { P0, P1, P2, P3 };
    Poly(B, Mat, Coll, Q, 4, Out, nullptr, nullptr, AoAll);
}

/** A hexahedron: Low[4] round its base, High[4] above them. Faces by Mask: 1 top, 2 bottom, 4 << K side K. Each face looks
 *  away from the solid's centre. */
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

/** A box round C on the frame X, Y, Z (unit vectors) with half sizes HX, HY, HZ; its "top" is +Z. */
void Box3(FBucket& B, EMat Mat, EColl Coll, const FVector& C, const FVector& X, const FVector& Y, const FVector& Z, double HX, double HY, double HZ, int32 Mask = AllFaces, float AoLow = 1.f, float AoHigh = 1.f)
{
    const FVector Low[4] = { C - X * HX - Y * HY - Z * HZ, C + X * HX - Y * HY - Z * HZ, C + X * HX + Y * HY - Z * HZ, C - X * HX + Y * HY - Z * HZ };
    const FVector High[4] = { Low[0] + Z * (2.0 * HZ), Low[1] + Z * (2.0 * HZ), Low[2] + Z * (2.0 * HZ), Low[3] + Z * (2.0 * HZ) };
    Hexa(B, Mat, Coll, Low, High, Mask, AoLow, AoHigh);
}

/** A box in a plan frame: Along [A0, A1] x Across [S0, S1] from Origin, bottom ZLow, its top ZTop0 at A0 rising evenly to
 *  ZTop1 at A1. */
void PlanBox(FBucket& B, EMat Mat, EColl Coll, const FVector2D& Origin, const FVector2D& Along, const FVector2D& Across,
    double A0, double A1, double S0, double S1, double ZLow, double ZTop0, double ZTop1, int32 Mask, float AoLow = 1.f, float AoHigh = 1.f)
{
    const FVector2D C[4] = { Origin + Along * A0 + Across * S0, Origin + Along * A1 + Across * S0, Origin + Along * A1 + Across * S1, Origin + Along * A0 + Across * S1 };
    const double Z[4] = { ZTop0, ZTop1, ZTop1, ZTop0 };
    const FVector Low[4] = { At3(C[0], ZLow), At3(C[1], ZLow), At3(C[2], ZLow), At3(C[3], ZLow) };
    const FVector High[4] = { At3(C[0], Z[0]), At3(C[1], Z[1]), At3(C[2], Z[2]), At3(C[3], Z[3]) };
    Hexa(B, Mat, Coll, Low, High, Mask, AoLow, AoHigh);
}

/** A round bar (Sides sides) from A to C, radius R0 at A and R1 at C (a frustum when they differ). */
void Cylinder(FBucket& B, EMat Mat, EColl Coll, const FVector& A, const FVector& C, double R0, double R1, int32 Sides, bool bCapA, bool bCapC)
{
    const FVector Axis = (C - A).GetSafeNormal();
    if (Axis.IsNearlyZero() || Sides < 3) return;
    const FVector Ref = FMath::Abs(Axis.Z) < .9 ? FVector::UpVector : FVector::ForwardVector;
    const FVector U = FVector::CrossProduct(Axis, Ref).GetSafeNormal(), W = FVector::CrossProduct(Axis, U);
    TArray<FVector> Ring;
    Ring.SetNum(Sides);
    for (int32 K = 0; K < Sides; ++K)
    {
        const double Ang = 2.0 * UE_DOUBLE_PI * K / Sides;
        Ring[K] = U * FMath::Cos(Ang) + W * FMath::Sin(Ang);
    }
    const double Slope = (R0 - R1) / FMath::Max((C - A).Size(), 1e-3);
    for (int32 K = 0; K < Sides; ++K)
    {
        const int32 L = (K + 1) % Sides;
        const FVector Q[4] = { A + Ring[K] * R0, A + Ring[L] * R0, C + Ring[L] * R1, C + Ring[K] * R1 };
        const FVector NK = (Ring[K] + Axis * Slope).GetSafeNormal(), NL = (Ring[L] + Axis * Slope).GetSafeNormal();
        const FVector Nrm[4] = { NK, NL, NL, NK };
        Poly(B, Mat, Coll, Q, 4, Ring[K] + Ring[L], Nrm);
    }
    for (int32 End = 0; End < 2; ++End)
    {
        if (!(End == 0 ? bCapA : bCapC)) continue;
        const FVector& Centre = End == 0 ? A : C;
        const double R = End == 0 ? R0 : R1;
        TArray<FVector> Q;
        for (int32 K = 0; K < Sides; ++K) Q.Add(Centre + Ring[K] * R);
        Poly(B, Mat, Coll, Q.GetData(), Sides, End == 0 ? -Axis : Axis);
    }
}

/** A rough boulder: a lat-long ellipsoid (radii R) over At, its surface pushed in and out by value noise, set so its lowest
 *  point lies at FootZ. */
void Boulder(FBucket& B, EMat Mat, EColl Coll, const FVector2D& At, double FootZ, const FVector& R, uint32 Seed, float Rough = .24f)
{
    constexpr int32 Around = 9, Rings = 6;
    FVector Grid[Rings + 1][Around];
    double Lowest = DBL_MAX;
    for (int32 J = 0; J <= Rings; ++J)
        for (int32 K = 0; K < Around; ++K)
        {
            const double Lat = UE_DOUBLE_PI * (double(J) / Rings - .5), Lon = 2.0 * UE_DOUBLE_PI * K / Around + .37 * J;
            const FVector Dir(FMath::Cos(Lat) * FMath::Cos(Lon), FMath::Cos(Lat) * FMath::Sin(Lon), FMath::Sin(Lat));
            const float Bump = 1.f + Rough * Value3(Dir * 100.0 + FVector(double(Seed % 977u), double(Seed % 613u), 0.0), 70.f, Seed) + .5f * Rough * Value3(Dir * 100.0, 31.f, Seed + 1);
            Grid[J][K] = FVector(Dir.X * R.X, Dir.Y * R.Y, Dir.Z * R.Z) * Bump;
            Lowest = FMath::Min(Lowest, Grid[J][K].Z);
        }
    const FVector Centre(At.X, At.Y, FootZ - Lowest);
    for (int32 J = 0; J <= Rings; ++J) for (int32 K = 0; K < Around; ++K) Grid[J][K] += Centre;
    for (int32 J = 0; J < Rings; ++J)
        for (int32 K = 0; K < Around; ++K)
        {
            const int32 L = (K + 1) % Around;
            const FVector Q[4] = { Grid[J][K], Grid[J][L], Grid[J + 1][L], Grid[J + 1][K] };
            const FVector Mid = (Q[0] + Q[1] + Q[2] + Q[3]) * .25;
            const float Ao = J == 0 ? .55f : J == 1 ? .75f : 1.f;
            Poly(B, Mat, Coll, Q, 4, Mid - Centre, nullptr, nullptr, Ao);
        }
}

/** Evenly spaced values from S0 to S1, no further apart than Spacing (both ends included). */
TArray<double> Spaced(double S0, double S1, double Spacing)
{
    TArray<double> Out;
    const int32 Gaps = FMath::Max(1, FMath::CeilToInt((S1 - S0) / Spacing));
    for (int32 K = 0; K <= Gaps; ++K) Out.Add(S0 + (S1 - S0) * K / Gaps);
    return Out;
}

// ------------------------------------------------------------------ stats
struct FStats
{
    int32 Segments = 0, Bands = 0, Returns = 0, Corners = 0, Columns = 0, Boulders = 0, FrozenHits = 0;
    double RockCm = 0.0;
    float MinRiseSeen = FLT_MAX, MaxBandTop = 0.f;
    int32 OverlookParts = 0, Glyphs = 0, Labels = 0, Benches = 0, Flags = 0, TrailRuns = 0, Trees = 0, TreeBatches = 0;
    int32 HiddenForest = 0, HedgeSegs = 0, HedgeRuleOk = 0;
    double TrailRailCm = 0.0, SkylineMs = 0.0;
    TArray<FString> Notes;   // logged once by Build
};

// ------------------------------------------------------------------ the edge closures (rock bands)
/** Level courses in absolute Z, a repeating stack of 24: sandstone beds (48-96 cm) stand out 6-17 cm, shale partings
 *  (18-42 cm) are weathered back 10-20 cm. */
struct FCourse { float Z0 = 0.f, Z1 = 0.f, Offset = 0.f; bool bShale = false; };
struct FStrata { TArray<FCourse> Courses; float Period = 1.f; };

FStrata MakeStrata(uint32 Seed)
{
    FStrata Sx;
    float Z = 0.f;
    for (int32 K = 0; K < 24; ++K)
    {
        const float H0 = Hash01(K, 1, Seed), H1 = Hash01(K, 2, Seed);
        const bool bShale = K % 3 == 1 || H0 < .16f;
        const float Thick = bShale ? FMath::Lerp(18.f, 42.f, H1) : FMath::Lerp(48.f, 96.f, H1);
        FCourse& C = Sx.Courses.AddDefaulted_GetRef();
        C.Z0 = Z;
        C.Z1 = Z + Thick;
        C.Offset = bShale ? -FMath::Lerp(10.f, 20.f, H0) : FMath::Lerp(6.f, 17.f, H0);
        C.bShale = bShale;
        Z += Thick;
    }
    Sx.Period = Z;
    return Sx;
}

/** The bedding's offset out of the face at (bedding) height Zb, eased over 5 cm either side of each course's edge; and whether
 *  Zb lies in shale. */
float Bedding(const FStrata& Sx, double Zb, bool& bShale)
{
    const float Local = float(Zb - FMath::FloorToDouble(Zb / Sx.Period) * Sx.Period);
    const int32 Num = Sx.Courses.Num();
    int32 K = 0;
    while (K + 1 < Num && Local >= Sx.Courses[K].Z1) ++K;
    const FCourse& C = Sx.Courses[K];
    bShale = C.bShale;
    constexpr float Ease = 5.f;
    if (Local - C.Z0 < Ease)
    {
        const FCourse& Below = Sx.Courses[(K + Num - 1) % Num];
        return FMath::Lerp(Below.Offset, C.Offset, FMath::SmoothStep(0.f, 1.f, .5f + .5f * (Local - C.Z0) / Ease));
    }
    if (C.Z1 - Local < Ease)
    {
        const FCourse& Above = Sx.Courses[(K + 1) % Num];
        return FMath::Lerp(Above.Offset, C.Offset, FMath::SmoothStep(0.f, 1.f, .5f + .5f * (C.Z1 - Local) / Ease));
    }
    return C.Offset;
}

struct FBandRange { double S0 = 0.0, S1 = 0.0; EEdgeClosure Kind = EEdgeClosure::RockBand; };

/** One band: the line from A along Dir (S from 0), In toward the square (its visible face); built from S0 - RunOnA to
 *  S1 + RunOnB. */
struct FBand
{
    FString Name;
    FVector2D A = FVector2D::ZeroVector, Dir = FVector2D(1, 0), In = FVector2D(0, -1);
    double S0 = 0.0, S1 = 0.0, RunOnA = 0.0, RunOnB = 0.0;
    TArray<FBandRange> Ranges;
    uint32 Seed = 0;
    bool bReturn = false;
    bool bFree[2] = { false, false };
    FVector2D End(int32 Index) const { return A + Dir * (Index == 0 ? S0 : S1); }
    EEdgeClosure KindAt(double S) const
    {
        const FBandRange* Best = nullptr;
        double BestD = DBL_MAX;
        for (const FBandRange& R : Ranges)
        {
            const double D = S < R.S0 ? R.S0 - S : S > R.S1 ? S - R.S1 : 0.0;
            if (D < BestD) { BestD = D; Best = &R; }
        }
        return Best ? Best->Kind : EEdgeClosure::RockBand;
    }
};

struct FColumn
{
    double S = 0.0;
    FVector2D P = FVector2D::ZeroVector;
    EEdgeClosure Kind = EEdgeClosure::RockBand;
    float Lo = 0.f, Hi = 0.f, Bottom = 0.f, Top = 0.f, TIn = 0.f, TOut = 0.f, Recess = 0.f, Dome = 0.f;
};

/** A point of a band's face: row Row of Rows up column C, on the inner (visible) face or the back. */
FVector FacePoint(const FBand& Bd, const FStrata& Sx, const FColumn& C, int32 Row, int32 Rows, bool bInner, bool& bShale, float& Ao)
{
    const float Z = FMath::Lerp(C.Bottom, C.Top, float(Row) / Rows);
    const FVector OnLine = At3(C.P, Z);
    bShale = false;
    float Bed = 0.f;
    if (C.Kind == EEdgeClosure::RockBand) Bed = Bedding(Sx, Z + .01 * C.S + 35.0 * Value1(C.S / 2600.0, Bd.Seed + 11u), bShale);
    const uint32 Sd = Bd.Seed + (bInner ? 0u : 50u);
    float Lump = 0.f;
    switch (C.Kind)
    {
    case EEdgeClosure::Rockslide: Lump = 24.f * Value3(OnLine, 120.f, Sd + 1u) + 9.f * Value3(OnLine, 45.f, Sd + 2u); break;
    case EEdgeClosure::BoulderChoke: Lump = 30.f * Value3(OnLine, 160.f, Sd + 3u) + 8.f * Value3(OnLine, 55.f, Sd + 4u); break;
    default: Lump = 6.f * Value3(OnLine, 85.f, Sd + 5u) + 3.5f * Value3(OnLine, 31.f, Sd + 6u); break;
    }
    const double Lean = FaceLean * (C.Top - Z);
    double U = bInner ? C.TIn + Lean + Bed + Lump - C.Recess : C.TOut + Lean + .5 * Bed + .6 * Lump - .5 * C.Recess;
    if (Row == Rows) U -= 6.0;   // the top edge weathered round
    U = FMath::Max(U, 8.0);
    const float Up = FMath::Clamp((Z - C.Lo) / 160.f, 0.f, 1.f);
    Ao = (.55f + .45f * Up) * (bShale ? .8f : 1.f) * (C.Recess > 0.f ? .86f : 1.f);
    return At3(C.P + Bd.In * (bInner ? U : -U), Z);
}

/** The columns of one band, every ColumnStep or less along it: their kind, the ground under their whole footprint (the face's
 *  foot leans out to TIn + 6 deg of its height), and their bottom and top. */
TArray<FColumn> BandColumns(const FBand& Bd, FStats& St)
{
    TArray<FColumn> Cols;
    const double From = Bd.S0 - Bd.RunOnA, To = Bd.S1 + Bd.RunOnB;
    const int32 Num = FMath::Max(1, FMath::CeilToInt((To - From) / ColumnStep));
    for (int32 K = 0; K <= Num; ++K)
    {
        FColumn C;
        C.S = From + (To - From) * K / Num;
        C.P = Bd.A + Bd.Dir * C.S;
        C.Kind = Bd.KindAt(C.S);
        const float N0 = Value1(C.S / 700.0, Bd.Seed + 21u), N1 = Value1(C.S / 900.0, Bd.Seed + 22u);
        C.TIn = C.Kind == EEdgeClosure::Rockslide ? 120.f + 40.f * N0 : C.Kind == EEdgeClosure::BoulderChoke ? 105.f + 35.f * N0 : 85.f + 25.f * N0;
        C.TOut = 95.f + 15.f * N1;
        // Vertical joints in the sandstone: a column now and then let back 14 cm (a fissure up the face).
        C.Recess = C.Kind == EEdgeClosure::RockBand && !Bd.bReturn && Mix(FMath::RoundToInt(C.S / ColumnStep), 0, 0, Bd.Seed + 23u) % 5u == 0u ? 14.f : 0.f;
        double Lo = DBL_MAX, Hi = -DBL_MAX;
        for (double DU = -(C.TOut + 40.0); DU <= C.TIn + 110.0; DU += 40.0)
            for (const double DS : { -.5 * ColumnStep, 0.0, .5 * ColumnStep })
            {
                const FVector2D Q = C.P + Bd.Dir * DS + Bd.In * DU;
                const double G = GroundAt(Q);
                Lo = FMath::Min(Lo, G);
                Hi = FMath::Max(Hi, G);
                if (HouseCalderTerrain::InFrozen(float(Q.X), float(Q.Y))) ++St.FrozenHits;
            }
        C.Lo = float(Lo);
        C.Hi = float(Hi);
        float Extra = 50.f + 130.f * (.5f + .5f * Value1(C.S / 1100.0, Bd.Seed + 24u)) + 45.f * (.5f + .5f * Value1(C.S / 330.0, Bd.Seed + 25u));
        if (C.Kind == EEdgeClosure::Rockslide) Extra += 40.f;   // a heap: the blocks spilled in front stay far under its top
        C.Top = C.Hi + MinRise + Extra;
        C.Bottom = C.Lo - FootDepth;
        C.Dome = 12.f + 30.f * (.5f + .5f * Value1(C.S / 500.0, Bd.Seed + 26u));
        Cols.Add(C);
    }
    return Cols;
}

/** Builds one band: its inner face (FaceRows rows), its back (BackRows), its domed top and its end caps; all Unwalkable. */
void EmitBand(FMeshes& M, const FBand& Bd, const TArray<FColumn>& Cols, FStats& St)
{
    const int32 NC = Cols.Num();
    if (NC < 2) return;
    const FStrata Sx = MakeStrata(Bd.Seed + 7u);
    TArray<FVector> Inner, Back, Crest;
    TArray<float> InnerAo, BackAo;
    TArray<uint8> InnerShale;
    Inner.SetNum(NC * (FaceRows + 1));
    InnerAo.SetNum(NC * (FaceRows + 1));
    InnerShale.SetNum(NC * (FaceRows + 1));
    Back.SetNum(NC * (BackRows + 1));
    BackAo.SetNum(NC * (BackRows + 1));
    Crest.SetNum(NC);
    for (int32 C = 0; C < NC; ++C)
    {
        for (int32 R = 0; R <= FaceRows; ++R)
        {
            bool bShale = false;
            float Ao = 1.f;
            Inner[C * (FaceRows + 1) + R] = FacePoint(Bd, Sx, Cols[C], R, FaceRows, true, bShale, Ao);
            InnerAo[C * (FaceRows + 1) + R] = Ao;
            InnerShale[C * (FaceRows + 1) + R] = bShale ? 1 : 0;
        }
        for (int32 R = 0; R <= BackRows; ++R)
        {
            bool bShale = false;
            float Ao = 1.f;
            Back[C * (BackRows + 1) + R] = FacePoint(Bd, Sx, Cols[C], R, BackRows, false, bShale, Ao);
            BackAo[C * (BackRows + 1) + R] = Ao * .9f;
        }
        const FVector& InTop = Inner[C * (FaceRows + 1) + FaceRows];
        const FVector& BackTop = Back[C * (BackRows + 1) + BackRows];
        Crest[C] = (InTop + BackTop) * .5 + FVector(0, 0, Cols[C].Dome);
        St.MinRiseSeen = FMath::Min(St.MinRiseSeen, Cols[C].Top - Cols[C].Hi);
        St.MaxBandTop = FMath::Max(St.MaxBandTop, Cols[C].Top - Cols[C].Hi);
    }
    const FVector In3(Bd.In.X, Bd.In.Y, 0.0);
    auto MatOf = [&Cols](int32 C, bool bShale) -> EMat
    {
        switch (Cols[C].Kind)
        {
        case EEdgeClosure::Rockslide: return MatFresh;
        case EEdgeClosure::BoulderChoke: return MatBoulder;
        default: return bShale ? MatShale : MatSandstone;
        }
    };
    // Smooth normals over each face's grid.
    auto GridNormals = [NC](const TArray<FVector>& V, int32 Rows, const FVector& Out)
    {
        TArray<FVector> Nrm;
        Nrm.SetNum(V.Num());
        auto Vx = [&V, Rows, NC](int32 C, int32 R) -> const FVector& { return V[FMath::Clamp(C, 0, NC - 1) * (Rows + 1) + FMath::Clamp(R, 0, Rows)]; };
        for (int32 C = 0; C < NC; ++C)
            for (int32 R = 0; R <= Rows; ++R)
            {
                FVector N = FVector::CrossProduct(Vx(C + 1, R) - Vx(C - 1, R), Vx(C, R + 1) - Vx(C, R - 1)).GetSafeNormal();
                if (FVector::DotProduct(N, Out) < 0.0) N = -N;
                Nrm[C * (Rows + 1) + R] = N.IsNearlyZero() ? Out : N;
            }
        return Nrm;
    };
    const TArray<FVector> InnerN = GridNormals(Inner, FaceRows, In3), BackN = GridNormals(Back, BackRows, -In3);
    for (int32 C = 0; C + 1 < NC; ++C)
    {
        FBucket& B = M.Rock((Cols[C].P + Cols[C + 1].P) * .5);
        for (int32 R = 0; R < FaceRows; ++R)
        {
            const int32 K00 = C * (FaceRows + 1) + R, K10 = (C + 1) * (FaceRows + 1) + R;
            const FVector Q[4] = { Inner[K00], Inner[K10], Inner[K10 + 1], Inner[K00 + 1] };
            const FVector N4[4] = { InnerN[K00], InnerN[K10], InnerN[K10 + 1], InnerN[K00 + 1] };
            const float A4[4] = { InnerAo[K00], InnerAo[K10], InnerAo[K10 + 1], InnerAo[K00 + 1] };
            Poly(B, MatOf(C, InnerShale[K00] != 0), EColl::Block, Q, 4, In3 + FVector(0, 0, .05), N4, A4);
        }
        for (int32 R = 0; R < BackRows; ++R)
        {
            const int32 K00 = C * (BackRows + 1) + R, K10 = (C + 1) * (BackRows + 1) + R;
            const FVector Q[4] = { Back[K00], Back[K10], Back[K10 + 1], Back[K00 + 1] };
            const FVector N4[4] = { BackN[K00], BackN[K10], BackN[K10 + 1], BackN[K00 + 1] };
            const float A4[4] = { BackAo[K00], BackAo[K10], BackAo[K10 + 1], BackAo[K00 + 1] };
            Poly(B, MatOf(C, false), EColl::Block, Q, 4, -In3 + FVector(0, 0, .05), N4, A4);
        }
        // The top: from the face's weathered edge up over the crest and down to the back's.
        const FVector& I0 = Inner[C * (FaceRows + 1) + FaceRows];
        const FVector& I1 = Inner[(C + 1) * (FaceRows + 1) + FaceRows];
        const FVector& B0 = Back[C * (BackRows + 1) + BackRows];
        const FVector& B1 = Back[(C + 1) * (BackRows + 1) + BackRows];
        Quad(B, MatOf(C, false), EColl::Block, I0, I1, Crest[C + 1], Crest[C], In3 + FVector(0, 0, 1.5));
        Quad(B, MatOf(C, false), EColl::Block, Crest[C], Crest[C + 1], B1, B0, -In3 + FVector(0, 0, 1.5));
    }
    // End caps: the inner face's edge column to the back's, row by row, and the top's triangle.
    for (int32 End = 0; End < 2; ++End)
    {
        const int32 C = End == 0 ? 0 : NC - 1;
        FBucket& B = M.Rock(Cols[C].P);
        const FVector Out(End == 0 ? -Bd.Dir.X : Bd.Dir.X, End == 0 ? -Bd.Dir.Y : Bd.Dir.Y, 0.0);
        for (int32 R = 0; R < FaceRows; ++R)
        {
            const int32 BR0 = FMath::RoundToInt(float(R) * BackRows / FaceRows), BR1 = FMath::RoundToInt(float(R + 1) * BackRows / FaceRows);
            const FVector Q[4] = { Inner[C * (FaceRows + 1) + R], Inner[C * (FaceRows + 1) + R + 1], Back[C * (BackRows + 1) + BR1], Back[C * (BackRows + 1) + BR0] };
            if (BR0 == BR1) { const FVector T[3] = { Q[0], Q[1], Q[2] }; Poly(B, MatOf(C, false), EColl::Block, T, 3, Out, nullptr, nullptr, .7f); }
            else Poly(B, MatOf(C, false), EColl::Block, Q, 4, Out, nullptr, nullptr, .7f);
        }
        const FVector T[3] = { Inner[C * (FaceRows + 1) + FaceRows], Crest[C], Back[C * (BackRows + 1) + BackRows] };
        Poly(B, MatOf(C, false), EColl::Block, T, 3, Out + FVector(0, 0, .3), nullptr, nullptr, .8f);
    }
    St.RockCm += Cols.Last().S - Cols[0].S;
    St.Columns += NC;
}

/** Blocks spilled from the rockslide onto Calder Dr: well clear of the face's foot and far under its top (Unwalkable). */
void SpillBlocks(FMeshes& M, const FBand& Bd, const TArray<FColumn>& Cols, FStats& St)
{
    for (const FBandRange& R : Bd.Ranges)
    {
        if (R.Kind != EEdgeClosure::Rockslide) continue;
        constexpr int32 Count = 9;
        for (int32 K = 0; K < Count; ++K)
        {
            const double S = R.S0 + (R.S1 - R.S0) * (K + .5 + .6 * (Hash01(K, 3, Bd.Seed) - .5)) / Count;
            // The face's foot round S (its furthest reach into the square over the neighbouring columns), then out from it.
            double FaceReach = 0.0;
            for (const FColumn& C : Cols) if (FMath::Abs(C.S - S) < 2.0 * ColumnStep) FaceReach = FMath::Max(FaceReach, double(C.TIn) + FaceLean * (C.Top - C.Bottom) + 40.0);
            const float Rad = FMath::Lerp(32.f, 100.f, Hash01(K, 4, Bd.Seed));
            const double U = FaceReach + 150.0 + Rad + 230.0 * Hash01(K, 5, Bd.Seed);
            const FVector2D P = Bd.A + Bd.Dir * S + Bd.In * U;
            const FVector Radii(Rad * FMath::Lerp(.85f, 1.3f, Hash01(K, 6, Bd.Seed)), Rad * FMath::Lerp(.8f, 1.15f, Hash01(K, 7, Bd.Seed)), Rad * .68f);
            // Bedded 8 cm into the road (its carriageway, SurfaceZ) at its lowest point.
            const double FootZ = SurfaceAt(P) - 8.0;
            Boulder(M.Rock(P), MatFresh, EColl::Block, P, FootZ, Radii, Bd.Seed + 100u + uint32(K), .3f);
            Foot(TEXT("CalderEdgeBlocks"), P, FootZ);
            ++St.Boulders;
        }
    }
}

void BuildBands(FMeshes& M, FStats& St)
{
    // Group the closures by edge: each edge's closures lie on one straight line (X +-31700 or Y 31700) and share their ends.
    TArray<FBand> Bands;
    for (const HouseCalderLandform::FEdgeClosure& Cl : HouseCalderLandform::EdgeClosures())
    {
        if (Cl.Points.Num() < 2) continue;
        ++St.Segments;
        int32 Index = Bands.IndexOfByPredicate([&Cl](const FBand& Bd) { return Bd.Name == Cl.Edge; });
        if (Index == INDEX_NONE)
        {
            Index = Bands.AddDefaulted();
            FBand& Fresh = Bands[Index];
            Fresh.Name = Cl.Edge;
            const bool bAlongX = FMath::Abs(Cl.Points.Last().Y - Cl.Points[0].Y) < 1.0;
            Fresh.Dir = bAlongX ? FVector2D(1, 0) : FVector2D(0, 1);
            Fresh.A = bAlongX ? FVector2D(-SquareEdge, Cl.Points[0].Y) : FVector2D(Cl.Points[0].X, -SquareEdge);
            Fresh.In = bAlongX ? FVector2D(0, Cl.Points[0].Y > 0.0 ? -1.0 : 1.0) : FVector2D(Cl.Points[0].X > 0.0 ? -1.0 : 1.0, 0);
            Fresh.Seed = 4100u + uint32(Index) * 97u;
            Fresh.S0 = DBL_MAX;
            Fresh.S1 = -DBL_MAX;
        }
        FBand& Bd = Bands[Index];
        FBandRange R;
        R.S0 = DBL_MAX;
        R.S1 = -DBL_MAX;
        R.Kind = Cl.Kind;
        for (const FVector2D& P : Cl.Points)
        {
            const double S = FVector2D::DotProduct(P - Bd.A, Bd.Dir);
            R.S0 = FMath::Min(R.S0, S);
            R.S1 = FMath::Max(R.S1, S);
        }
        Bd.Ranges.Add(R);
        Bd.S0 = FMath::Min(Bd.S0, R.S0);
        Bd.S1 = FMath::Max(Bd.S1, R.S1);
    }
    // Ends: one that meets another band's end (a corner) runs on past it; a free one turns out to the world edge.
    TArray<FBand> Wings;
    for (int32 BI = 0; BI < Bands.Num(); ++BI)
        for (int32 End = 0; End < 2; ++End)
        {
            FBand& Bd = Bands[BI];
            const FVector2D E = Bd.End(End);
            bool bCorner = false;
            for (int32 Other = 0; Other < Bands.Num(); ++Other)
                if (Other != BI) for (int32 OE = 0; OE < 2; ++OE) bCorner |= (Bands[Other].End(OE) - E).Size() < 60.0;
            (End == 0 ? Bd.RunOnA : Bd.RunOnB) = bCorner ? CornerRunOn : FreeRunOn;
            if (bCorner) { ++St.Corners; continue; }
            Bd.bFree[End] = true;
            FBand W;
            W.Name = Bd.Name + TEXT(" (return)");
            W.A = E;
            W.Dir = -Bd.In;
            W.In = End == 0 ? -Bd.Dir : Bd.Dir;
            W.S0 = 0.0;
            W.S1 = SquareEdge - EdgeLine - 10.0;
            W.Ranges.Add({ W.S0, W.S1, EEdgeClosure::RockBand });
            W.Seed = Bd.Seed + 31u + uint32(End);
            W.bReturn = true;
            Wings.Add(W);
            ++St.Returns;
        }
    Bands.Append(Wings);
    for (const FBand& Bd : Bands)
    {
        const TArray<FColumn> Cols = BandColumns(Bd, St);
        EmitBand(M, Bd, Cols, St);
        SpillBlocks(M, Bd, Cols, St);
        ++St.Bands;
        float BandMinRise = FLT_MAX;
        for (const FColumn& C : Cols) BandMinRise = FMath::Min(BandMinRise, C.Top - C.Hi);
        FString Kinds;
        for (const FBandRange& R : Bd.Ranges)
            Kinds += FString::Printf(TEXT("%s%s %.0f..%.0f"), Kinds.IsEmpty() ? TEXT("") : TEXT(", "),
                R.Kind == EEdgeClosure::Rockslide ? TEXT("rockslide") : R.Kind == EEdgeClosure::BoulderChoke ? TEXT("boulder choke") : TEXT("rock band"), R.S0, R.S1);
        const FVector2D From = Bd.A + Bd.Dir * (Bd.S0 - Bd.RunOnA), To = Bd.A + Bd.Dir * (Bd.S1 + Bd.RunOnB);
        St.Notes.Add(FString::Printf(TEXT("CALDER_EDGES_BAND edge=\"%s\" from=(%.0f,%.0f) to=(%.0f,%.0f) meters=%.1f columns=%d minRiseCm=%.0f free=%d,%d kinds=[%s]"),
            *Bd.Name, From.X, From.Y, To.X, To.Y, (To - From).Size() / 100.0, Cols.Num(), BandMinRise, Bd.bFree[0] ? 1 : 0, Bd.bFree[1] ? 1 : 0, *Kinds));
    }
}

// ------------------------------------------------------------------ raised stroke lettering (the bronze plates)
/** A 1930s plate's letters as strokes on an 8 x 12 grid (half units; the cap height is 12, the advance 11): polylines, '|'
 *  between them. */
struct FGlyph { TCHAR Char; const char* Strokes; };
const FGlyph Glyphs[] = {
    { TEXT('A'), "0 0 4 12 8 0|2 5 6 5" }, { TEXT('B'), "0 0 0 12 6 12 8 10 8 8 6 6 0 6|6 6 8 4 8 2 6 0 0 0" },
    { TEXT('C'), "8 10 6 12 2 12 0 10 0 2 2 0 6 0 8 2" }, { TEXT('D'), "0 0 0 12 5 12 8 9 8 3 5 0 0 0" },
    { TEXT('E'), "8 12 0 12 0 0 8 0|0 6 6 6" }, { TEXT('F'), "8 12 0 12 0 0|0 6 6 6" },
    { TEXT('G'), "8 10 6 12 2 12 0 10 0 2 2 0 6 0 8 2 8 5 5 5" }, { TEXT('H'), "0 0 0 12|8 0 8 12|0 6 8 6" },
    { TEXT('I'), "2 12 6 12|4 12 4 0|2 0 6 0" }, { TEXT('J'), "8 12 8 2 6 0 2 0 0 2" },
    { TEXT('K'), "0 0 0 12|8 12 0 4|3 7 8 0" }, { TEXT('L'), "0 12 0 0 8 0" },
    { TEXT('M'), "0 0 0 12 4 6 8 12 8 0" }, { TEXT('N'), "0 0 0 12 8 0 8 12" },
    { TEXT('O'), "2 0 0 2 0 10 2 12 6 12 8 10 8 2 6 0 2 0" }, { TEXT('P'), "0 0 0 12 6 12 8 10 8 8 6 6 0 6" },
    { TEXT('Q'), "2 0 0 2 0 10 2 12 6 12 8 10 8 2 6 0 2 0|5 3 8 0" }, { TEXT('R'), "0 0 0 12 6 12 8 10 8 8 6 6 0 6|4 6 8 0" },
    { TEXT('S'), "8 10 6 12 2 12 0 10 0 8 2 6 6 6 8 4 8 2 6 0 2 0 0 2" }, { TEXT('T'), "0 12 8 12|4 12 4 0" },
    { TEXT('U'), "0 12 0 2 2 0 6 0 8 2 8 12" }, { TEXT('V'), "0 12 4 0 8 12" },
    { TEXT('W'), "0 12 2 0 4 8 6 0 8 12" }, { TEXT('X'), "0 0 8 12|0 12 8 0" },
    { TEXT('Y'), "0 12 4 6 8 12|4 6 4 0" }, { TEXT('Z'), "0 12 8 12 0 0 8 0" },
    { TEXT('0'), "2 0 0 2 0 10 2 12 6 12 8 10 8 2 6 0 2 0" }, { TEXT('1'), "2 10 4 12 4 0|2 0 6 0" },
    { TEXT('2'), "0 10 2 12 6 12 8 10 8 8 0 0 8 0" }, { TEXT('3'), "0 10 2 12 6 12 8 10 8 8 6 6 4 6|6 6 8 4 8 2 6 0 2 0 0 2" },
    { TEXT('4'), "6 0 6 12 0 4 8 4" }, { TEXT('5'), "8 12 0 12 0 7 6 7 8 5 8 2 6 0 2 0 0 2" },
    { TEXT('6'), "8 10 6 12 2 12 0 10 0 2 2 0 6 0 8 2 8 4 6 6 0 6" }, { TEXT('7'), "0 12 8 12 3 0" },
    { TEXT('8'), "2 6 0 8 0 10 2 12 6 12 8 10 8 8 6 6 2 6 0 4 0 2 2 0 6 0 8 2 8 4 6 6" },
    { TEXT('9'), "0 2 2 0 6 0 8 2 8 10 6 12 2 12 0 10 0 8 2 6 8 6" },
    { TEXT('\''), "4 12 4 9" }, { TEXT('.'), "4 0 4 1" }, { TEXT('-'), "2 6 6 6" }, { TEXT(','), "4 1 3 -2" }, { TEXT('/'), "0 0 8 12" },
};
constexpr float GlyphAdvance = 11.f, SpaceAdvance = 8.f, GlyphHeight = 12.f;

/** The polylines of one character (empty for a space or an unknown one). */
TArray<TArray<FVector2D>> GlyphLines(TCHAR Ch)
{
    TArray<TArray<FVector2D>> Lines;
    const FGlyph* Found = nullptr;
    for (const FGlyph& G : Glyphs) if (G.Char == Ch) { Found = &G; break; }
    if (!Found) return Lines;
    TArray<FVector2D>* Cur = &Lines.AddDefaulted_GetRef();
    double Pending = 0.0;
    bool bHave = false;
    int32 Sign = 1, Value = 0;
    bool bInNumber = false;
    auto Flush = [&]()
    {
        if (!bInNumber) return;
        const double V = double(Sign * Value);
        if (!bHave) { Pending = V; bHave = true; }
        else { Cur->Add(FVector2D(Pending, V)); bHave = false; }
        bInNumber = false;
        Sign = 1;
        Value = 0;
    };
    for (const char* C = Found->Strokes; *C; ++C)
    {
        if (*C == '-') { Flush(); Sign = -1; bInNumber = true; }
        else if (*C >= '0' && *C <= '9') { Value = Value * 10 + (*C - '0'); bInNumber = true; }
        else if (*C == '|') { Flush(); Cur = &Lines.AddDefaulted_GetRef(); }
        else Flush();
    }
    Flush();
    return Lines;
}

float LetteringWidth(const FString& Str, float H)
{
    float Units = 0.f;
    for (int32 K = 0; K < Str.Len(); ++K) Units += Str[K] == TEXT(' ') ? SpaceAdvance : GlyphAdvance;
    if (Str.Len() > 0) Units -= GlyphAdvance - 8.f;   // no spacing after the last letter
    return Units * H / GlyphHeight;
}

/** A raised stroke from P to Q on a plate (in the plate's U, V cm from Origin): Width wide, Proud high. */
void Stroke(FBucket& B, EMat Mat, const FVector& Origin, const FVector& Ux, const FVector& Uy, const FVector& N, const FVector2D& P, const FVector2D& Q, double Width, double Proud)
{
    FVector2D D = Q - P;
    const double Len = D.Size();
    D = Len > 1e-4 ? D / Len : FVector2D(1, 0);
    const FVector Along = (Ux * D.X + Uy * D.Y).GetSafeNormal();
    const FVector Across = FVector::CrossProduct(N, Along).GetSafeNormal();
    const FVector2D Mid = (P + Q) * .5;
    const FVector C = Origin + Ux * Mid.X + Uy * Mid.Y + N * (Proud * .5);
    Box3(B, Mat, EColl::None, C, Along, Across, N, Len * .5 + Width * .5, Width * .5, Proud * .5, SidesAndTop);
}

/** Raised letters: Str on the plate from (U, V) (its baseline: Align -1 its start, 0 its middle, 1 its end), H tall. Returns the
 *  number of letters drawn. */
int32 Lettering(FBucket& B, EMat Mat, const FVector& Origin, const FVector& Ux, const FVector& Uy, const FVector& N, double U, double V, const FString& Str, float H, int32 Align, double Proud = .32)
{
    const double Scale = H / GlyphHeight, Width = FMath::Max(.16, .13 * H);
    double X = U - (Align < 0 ? 0.0 : Align == 0 ? LetteringWidth(Str, H) * .5 : LetteringWidth(Str, H));
    int32 Drawn = 0;
    for (int32 K = 0; K < Str.Len(); ++K)
    {
        const TCHAR Ch = Str[K];
        if (Ch == TEXT(' ')) { X += SpaceAdvance * Scale; continue; }
        for (const TArray<FVector2D>& Line : GlyphLines(Ch))
            for (int32 J = 0; J + 1 < Line.Num(); ++J)
                Stroke(B, Mat, Origin, Ux, Uy, N, FVector2D(X + Line[J].X * Scale, V + Line[J].Y * Scale), FVector2D(X + Line[J + 1].X * Scale, V + Line[J + 1].Y * Scale), Width, Proud);
        X += GlyphAdvance * Scale;
        ++Drawn;
    }
    return Drawn;
}

// ------------------------------------------------------------------ Founders' Overlook: the panorama table and the coin viewer
const HouseCalderLandform::FSpot* FindSpot(const TArray<HouseCalderLandform::FSpot>& Spots, const TCHAR* Key)
{
    for (const HouseCalderLandform::FSpot& S : Spots) if (S.Name.Contains(Key)) return &S;
    return nullptr;
}

/** The declared Parapet wall nearest Near: its line (A, B). */
bool ParapetLine(const FVector2D& Near, FVector2D& OutA, FVector2D& OutB)
{
    double Best = 600.0;
    bool bFound = false;
    for (const HouseCalderLandform::FWall& Wl : HouseCalderLandform::Walls())
    {
        if (Wl.Kind != HouseCalderLandform::EWallKind::Parapet) continue;
        const double D = SegDist(Near, Wl.A, Wl.B);
        if (D < Best) { Best = D; OutA = Wl.A; OutB = Wl.B; bFound = true; }
    }
    return bFound;
}

/** The skyline's elevation (degrees) seen from Eye along azimuth AzDeg: the outer land and the walkable ground to 3 km. */
float SkylineElevation(const FVector& Eye, double AzDeg)
{
    const double CA = FMath::Cos(FMath::DegreesToRadians(AzDeg)), SA = FMath::Sin(FMath::DegreesToRadians(AzDeg));
    double Best = -90.0;
    for (double D = 400.0; D < 300000.0; D *= 1.08)
    {
        const double Z = HouseCalderFarLand::Ground(Eye.X + CA * D, Eye.Y + SA * D);
        Best = FMath::Max(Best, FMath::RadiansToDegrees(FMath::Atan2(Z - Eye.Z, D)));
    }
    return float(Best);
}

double AzimuthTo(const FVector2D& From, const FVector2D& To)
{
    double Az = FMath::RadiansToDegrees(FMath::Atan2(To.Y - From.Y, To.X - From.X));
    while (Az < 0.0) Az += 360.0;
    return Az;
}

void BuildOverlook(FMeshes& M, FStats& St)
{
    const TArray<HouseCalderLandform::FSpot>& Spots = HouseCalderLandform::Overlooks();
    const HouseCalderLandform::FSpot* Plaque = FindSpot(Spots, TEXT("plaque"));
    const HouseCalderLandform::FSpot* Viewer = FindSpot(Spots, TEXT("coin viewer"));
    const HouseCalderLandform::FSpot* Stand = FindSpot(Spots, TEXT("standing spot"));
    if (!Plaque || !Viewer || !Stand) { St.Notes.Add(TEXT("CALDER_EDGES_OVERLOOK skipped reason=spots-missing")); return; }
    const FVector2D PlaqueAt(Plaque->At.X, Plaque->At.Y), ViewerAt(Viewer->At.X, Viewer->At.Y), StandAt(Stand->At.X, Stand->At.Y);
    FVector2D WA, WB;
    if (!ParapetLine(PlaqueAt, WA, WB)) { St.Notes.Add(TEXT("CALDER_EDGES_OVERLOOK skipped reason=no-parapet")); return; }
    const FVector2D WD = (WB - WA).GetSafeNormal();
    FVector2D Terrace(-WD.Y, WD.X);   // the terrace side of the parapet line (where the standing spot is)
    if (FVector2D::DotProduct(StandAt - WA, Terrace) < 0.0) Terrace = -Terrace;
    auto OnLine = [&WA, &WD](const FVector2D& P) { return WA + WD * FVector2D::DotProduct(P - WA, WD); };
    FBucket& B = M.Area(AreaOverlook);
    const FVector Up = FVector::UpVector;

    // ---- the panorama table: a dressed sandstone pedestal 85 cm in from the parapet's line (its 40 cm body and 10 cm coping
    // stand on the terrace side, so 12 cm stay clear), the bronze plate on its capstone, tilted 22 degrees toward the reader.
    // (The export's plaque spot lies on that line's 25 cm strip, inside the parapet's body: the table stands just behind it.)
    const FVector2D Facing = -Terrace;                    // the reader faces the view
    const FVector2D Right = Right2(Facing);
    const FVector2D Ped = OnLine(PlaqueAt) + Terrace * 85.0;
    double G = DBL_MAX;
    for (const FVector2D& O : { FVector2D(-30, -30), FVector2D(30, -30), FVector2D(30, 30), FVector2D(-30, 30), FVector2D(0, 0) }) G = FMath::Min(G, SurfaceAt(Ped + O));
    const double Tilt = FMath::DegreesToRadians(22.0);
    const FVector Ux(Right.X, Right.Y, 0.0);
    const FVector Uv = FVector(Facing.X, Facing.Y, 0.0) * FMath::Cos(Tilt) + Up * FMath::Sin(Tilt);   // up the plate, away from the reader
    const FVector N = FVector::CrossProduct(Ux, Uv).GetSafeNormal();
    const FVector CapTop = At3(Ped, G + 101.0);
    constexpr double CapThick = 7.0, CapW = 72.0, CapD = 52.0, PlateW = 66.0, PlateD = 46.0, PlateThick = 1.1;
    // The column, its top following the capstone's underside.
    {
        const double Under = CapTop.Z - CapThick / FMath::Cos(Tilt);
        const double TanT = FMath::Tan(Tilt);
        PlanBox(B, MatDressed, EColl::Block, Ped, Facing, Right, -17.0, 17.0, -22.0, 22.0, G - 8.0, Under - 17.0 * TanT + .5, Under + 17.0 * TanT + .5, SidesAndTop, .6f, 1.f);
        // A plinth course at its foot, set 8 cm into the terrace.
        PlanBox(B, MatDressed, EColl::Block, Ped, Facing, Right, -21.0, 21.0, -26.0, 26.0, G - 8.0, G + 9.0, G + 9.0, SidesAndTop, .5f, .8f);
        for (const double A : { -21.0, 21.0 }) for (const double L : { -26.0, 26.0 }) Foot(TEXT("CalderOverlookTable"), Ped + Facing * A + Right * L, G - 8.0);
        St.OverlookParts += 2;
        // Verdigris run down the column's front face from under the plate.
        for (int32 K = 0; K < 5; ++K)
        {
            const double L = -18.0 + 9.0 * K + 3.0 * (Hash01(K, 1, 911u) - .5);
            const double W = FMath::Lerp(1.0, 2.6, double(Hash01(K, 2, 911u)));
            const double Len = FMath::Lerp(14.0, 46.0, double(Hash01(K, 3, 911u)));
            const double ZTop = Under - 17.0 * TanT - 1.0, ZLow = ZTop - Len;
            const FVector2D Face = Ped - Facing * 17.15;
            const FVector P0 = At3(Face + Right * (L - W * .5), ZTop), P1 = At3(Face + Right * (L + W * .5), ZTop);
            const FVector P2 = At3(Face + Right * (L + W * .15), ZLow), P3 = At3(Face + Right * (L - W * .15), ZLow);
            Quad(B, MatVerdigris, EColl::None, P0, P1, P2, P3, FVector(-Facing.X, -Facing.Y, 0.0), .8f);
        }
    }
    // The capstone and the plate.
    Box3(B, MatDressed, EColl::Block, CapTop - N * (CapThick * .5), Ux, Uv, N, CapW * .5, CapD * .5, CapThick * .5, AllFaces, .7f, 1.f);
    const FVector PlateBase = CapTop + N * PlateThick;   // the plate's face
    Box3(B, MatBronze, EColl::None, CapTop + N * (PlateThick * .5), Ux, Uv, N, PlateW * .5, PlateD * .5, PlateThick * .5, SidesOnly);
    Quad(B, MatVerdigris, EColl::None, PlateBase - Ux * (PlateW * .5) - Uv * (PlateD * .5), PlateBase + Ux * (PlateW * .5) - Uv * (PlateD * .5),
        PlateBase + Ux * (PlateW * .5) + Uv * (PlateD * .5), PlateBase - Ux * (PlateW * .5) + Uv * (PlateD * .5), N, .85f);
    St.OverlookParts += 2;
    // The plate's own frame: U from its left edge, V from its front edge (cm).
    const FVector PlateOrigin = PlateBase - Ux * (PlateW * .5) - Uv * (PlateD * .5);
    constexpr double Rim = 1.2;
    Stroke(B, MatBronze, PlateOrigin, Ux, Uv, N, FVector2D(Rim * .5, Rim * .5), FVector2D(PlateW - Rim * .5, Rim * .5), Rim, .35);
    Stroke(B, MatBronze, PlateOrigin, Ux, Uv, N, FVector2D(Rim * .5, PlateD - Rim * .5), FVector2D(PlateW - Rim * .5, PlateD - Rim * .5), Rim, .35);
    Stroke(B, MatBronze, PlateOrigin, Ux, Uv, N, FVector2D(Rim * .5, Rim * .5), FVector2D(Rim * .5, PlateD - Rim * .5), Rim, .35);
    Stroke(B, MatBronze, PlateOrigin, Ux, Uv, N, FVector2D(PlateW - Rim * .5, Rim * .5), FVector2D(PlateW - Rim * .5, PlateD - Rim * .5), Rim, .35);
    St.Glyphs += Lettering(B, MatBronze, PlateOrigin, Ux, Uv, N, PlateW * .5, 40.6, TEXT("FOUNDERS' OVERLOOK"), 3.0f, 0, .4);
    St.Glyphs += Lettering(B, MatBronze, PlateOrigin, Ux, Uv, N, PlateW * .5, 37.4, TEXT("ERECTED BY THE CLASS OF 1931"), 1.4f, 0);
    St.Glyphs += Lettering(B, MatBronze, PlateOrigin, Ux, Uv, N, PlateW * .5, 2.9, TEXT("CALDER STATE TEACHERS COLLEGE"), 1.3f, 0);
    Stroke(B, MatBronze, PlateOrigin, Ux, Uv, N, FVector2D(9.0, 36.4), FVector2D(PlateW - 9.0, 36.4), .22, .3);
    Stroke(B, MatBronze, PlateOrigin, Ux, Uv, N, FVector2D(9.0, 5.4), FVector2D(PlateW - 9.0, 5.4), .22, .3);

    // The skyline as it is seen from the standing spot, across the half circle the reader faces (left edge: Facing + 90 deg).
    {
        const double Start = FPlatformTime::Seconds();
        const FVector Eye = At3(StandAt, Stand->GroundZ + 170.0);
        const double FacingAz = AzimuthTo(FVector2D::ZeroVector, Facing), AzLeft = FacingAz + 90.0;
        constexpr int32 Steps = 45;   // every 4 degrees
        TArray<float> Elev;
        for (int32 K = 0; K <= Steps; ++K) Elev.Add(SkylineElevation(Eye, AzLeft - 180.0 * K / Steps));
        St.SkylineMs = (FPlatformTime::Seconds() - Start) * 1000.0;
        constexpr double BandLo = 15.5, BandHi = 31.0, ElevLo = 5.0, ElevHi = 25.0, Margin = 2.4;
        auto UOfAz = [&](double Az)
        {
            double Rel = AzLeft - Az;
            while (Rel < 0.0) Rel += 360.0;
            while (Rel >= 360.0) Rel -= 360.0;
            return Margin + (PlateW - 2.0 * Margin) * Rel / 180.0;
        };
        auto VOfElev = [&](double E) { return BandLo + (BandHi - BandLo) * FMath::Clamp((E - ElevLo) / (ElevHi - ElevLo), 0.0, 1.0); };
        TArray<FVector2D> Line;
        for (int32 K = 0; K <= Steps; ++K) Line.Add(FVector2D(UOfAz(AzLeft - 180.0 * K / Steps), VOfElev(Elev[K])));
        for (int32 K = 0; K + 1 < Line.Num(); ++K) Stroke(B, MatBronze, PlateOrigin, Ux, Uv, N, Line[K], Line[K + 1], .28, .32);
        auto ElevAtAz = [&](double Az)
        {
            double Rel = AzLeft - Az;
            while (Rel < 0.0) Rel += 360.0;
            const double F = FMath::Clamp(Rel / 180.0 * Steps, 0.0, double(Steps));
            const int32 I = FMath::Min(FMath::FloorToInt(F), Steps - 1);
            return FMath::Lerp(double(Elev[I]), double(Elev[I + 1]), F - I);
        };
        // The skyline's highest (or lowest) sample between two bearings (both within the half circle).
        auto Extreme = [&](double AzA, double AzB, bool bMax)
        {
            double BestAz = AzA, BestE = bMax ? -1e9 : 1e9;
            for (int32 K = 0; K <= Steps; ++K)
            {
                const double Az = AzLeft - 180.0 * K / Steps;
                if (Az < FMath::Min(AzA, AzB) - .01 || Az > FMath::Max(AzA, AzB) + .01) continue;
                if (bMax ? Elev[K] > BestE : Elev[K] < BestE) { BestE = Elev[K]; BestAz = Az; }
            }
            return BestAz;
        };
        // The named places (all invented): their bearings from the standing spot. Gosling Knob's summit, the notch between it
        // and its shoulder, the far ridge seen past the shoulder and the near ridge's crown are read off the skyline; the river's
        // water gap and the town (Main St) are fixed points of the landform (landform.py WATER_GAP_X, MAIN_ST_Y).
        struct FLabel { FString Text; double Az; bool bSkyline; };
        const double AzRight = AzLeft - 180.0;
        auto InArc = [&](double Az) { while (Az > AzLeft) Az -= 360.0; while (Az < AzRight) Az += 360.0; return Az; };
        TArray<FLabel> Labels = {
            { TEXT("GOSLING KNOB"), Extreme(InArc(182.0), InArc(204.0), true), true },
            { TEXT("LANTERN KNOB"), Extreme(InArc(250.0), InArc(300.0), true), true },
            { TEXT("COLDHEARTH GAP"), InArc(AzimuthTo(StandAt, FVector2D(65200.0, -89000.0))), true },
            { TEXT("PALE RIDGE"), Extreme(InArc(226.0), InArc(250.0), true), true },
            { TEXT("WIDOWS NOTCH"), Extreme(InArc(204.0), InArc(224.0), false), true },
            { TEXT("CALDER"), InArc(AzimuthTo(StandAt, FVector2D(0.0, -39700.0))), false },
        };
        // Rows: two above the skyline, two below it. A label takes the first row where it overlaps no other label (1 cm apart),
        // its tick (to the skyline, or a short arrow down to the valley for the town) crosses no label, and no tick crosses it.
        constexpr float LabelH = 1.2f;
        const double RowV[4] = { 32.3, 34.5, 11.6, 9.2 };
        struct FPlaced { double U0, U1, V0, V1; };
        TArray<FPlaced> Boxes;
        struct FTick { double U, V0, V1; };
        TArray<FTick> Ticks;
        for (const FLabel& Lb : Labels)
        {
            if (Lb.Az > AzLeft + .01 || Lb.Az < AzRight - .01) continue;
            const double U = UOfAz(Lb.Az), Wd = LetteringWidth(Lb.Text, LabelH);
            const double U0 = FMath::Clamp(U - Wd * .5, Margin, PlateW - Margin - Wd);
            const double SkyV = VOfElev(ElevAtAz(Lb.Az));
            int32 Chosen = INDEX_NONE;
            FTick T{ U, 0.0, 0.0 };
            for (const int32 Row : { 0, 2, 1, 3 })
            {
                if (!Lb.bSkyline && Row < 2) continue;
                const FPlaced Box{ U0 - .5, U0 + Wd + .5, RowV[Row] - .3, RowV[Row] + LabelH + .3 };
                FTick Try{ U, 0.0, 0.0 };
                if (!Lb.bSkyline) { Try.V0 = Box.V1; Try.V1 = Box.V1 + 1.8; }
                else if (Row < 2) { Try.V0 = SkyV + .5; Try.V1 = Box.V0; }
                else { Try.V0 = Box.V1; Try.V1 = SkyV - .5; }
                bool bOk = Try.V1 > Try.V0;
                for (const FPlaced& O : Boxes)
                {
                    bOk &= Box.U1 < O.U0 || Box.U0 > O.U1 || Box.V1 < O.V0 || Box.V0 > O.V1;
                    bOk &= Try.U < O.U0 || Try.U > O.U1 || Try.V1 < O.V0 || Try.V0 > O.V1;
                }
                for (const FTick& O : Ticks) bOk &= O.U < Box.U0 || O.U > Box.U1 || O.V1 < Box.V0 || O.V0 > Box.V1;
                if (!bOk) continue;
                Chosen = Row;
                Boxes.Add(Box);
                T = Try;
                break;
            }
            if (Chosen == INDEX_NONE)
            {
                St.Notes.Add(FString::Printf(TEXT("CALDER_EDGES_LABEL name=\"%s\" az=%.1f placed=0 (no free row)"), *Lb.Text, Lb.Az));
                continue;
            }
            Ticks.Add(T);
            St.Glyphs += Lettering(B, MatBronze, PlateOrigin, Ux, Uv, N, U0, RowV[Chosen], Lb.Text, LabelH, -1);
            Stroke(B, MatBronze, PlateOrigin, Ux, Uv, N, FVector2D(T.U, T.V0), FVector2D(T.U, T.V1), .2, .3);
            if (!Lb.bSkyline)
            {
                // the town's arrow head
                Stroke(B, MatBronze, PlateOrigin, Ux, Uv, N, FVector2D(T.U, T.V1), FVector2D(T.U - .7, T.V1 - .9), .2, .3);
                Stroke(B, MatBronze, PlateOrigin, Ux, Uv, N, FVector2D(T.U, T.V1), FVector2D(T.U + .7, T.V1 - .9), .2, .3);
            }
            ++St.Labels;
            St.Notes.Add(FString::Printf(TEXT("CALDER_EDGES_LABEL name=\"%s\" az=%.1f elevDeg=%.1f u=%.1f row=%d"), *Lb.Text, Lb.Az, ElevAtAz(Lb.Az), U, Chosen));
        }
    }

    // ---- the coin viewer: heavy painted steel on a pipe post, the paint chipped to rust; no brand. Its optics point along the
    // export's yaw (205: Gosling Knob), 80 cm in from the parapet's line, clear of its coping.
    {
        const FVector2D Vp = OnLine(ViewerAt) + Terrace * 80.0;
        double GV = DBL_MAX;
        for (const FVector2D& O : { FVector2D(-18, -18), FVector2D(18, -18), FVector2D(18, 18), FVector2D(-18, 18), FVector2D(0, 0) }) GV = FMath::Min(GV, SurfaceAt(Vp + O));
        Foot(TEXT("CalderCoinViewer"), Vp, GV - 1.5);
        const FVector2D F2 = YawDir(Viewer->Yaw), R2 = Right2(F2);
        const FVector F(F2.X, F2.Y, 0.0), R(R2.X, R2.Y, 0.0);
        const FVector Base = At3(Vp, GV);
        Box3(B, MatPaint, EColl::Block, Base + Up * .5, R, F, Up, 17.0, 17.0, 2.0, SidesAndTop, .6f, .9f);   // the base plate, its foot 1.5 cm down
        for (int32 K = 0; K < 4; ++K)
        {
            const FVector Bolt = Base + R * (K % 2 ? 13.0 : -13.0) + F * (K / 2 ? 13.0 : -13.0) + Up * 3.0;
            Cylinder(B, MatRust, EColl::None, Bolt - Up * .6, Bolt + Up * 1.0, 1.4, 1.2, 6, false, true);
        }
        Cylinder(B, MatPaint, EColl::None, Base + Up * 2.4, Base + Up * 16.0, 15.0, 7.5, 12, false, false);   // the cast foot
        Cylinder(B, MatPaint, EColl::None, Base + Up * 16.0, Base + Up * 104.0, 6.0, 6.0, 12, false, false);   // the post
        Cylinder(B, MatPaint, EColl::None, Base + Up * 104.0, Base + Up * 113.0, 8.5, 8.5, 12, true, true);    // the swivel collar
        Box3(B, MatPaint, EColl::None, Base + Up * 115.0, R, F, Up, 16.0, 4.5, 2.0);                           // the yoke
        for (const double Side : { -14.5, 14.5 }) Box3(B, MatPaint, EColl::None, Base + R * Side + Up * 124.0, R, F, Up, 1.5, 5.0, 9.0);
        // The head, pitched 6 degrees down toward the town: an octagonal body along its optics.
        const double Pitch = FMath::DegreesToRadians(6.0);
        const FVector Hf = (F * FMath::Cos(Pitch) - Up * FMath::Sin(Pitch)).GetSafeNormal();
        const FVector Hu = FVector::CrossProduct(R, Hf).GetSafeNormal() * (FVector::DotProduct(FVector::CrossProduct(R, Hf), Up) < 0.0 ? -1.0 : 1.0);
        const FVector Hc = Base + Up * 134.0 + F * 2.0;
        constexpr double HalfW = 13.0, HalfH = 11.0, Cham = 4.0, HalfL = 20.0;
        const FVector2D Oct[8] = { FVector2D(HalfW - Cham, -HalfH), FVector2D(HalfW, -HalfH + Cham), FVector2D(HalfW, HalfH - Cham), FVector2D(HalfW - Cham, HalfH),
            FVector2D(-HalfW + Cham, HalfH), FVector2D(-HalfW, HalfH - Cham), FVector2D(-HalfW, -HalfH + Cham), FVector2D(-HalfW + Cham, -HalfH) };
        FVector Front[8], Rear[8];
        for (int32 K = 0; K < 8; ++K)
        {
            Front[K] = Hc + Hf * HalfL + R * Oct[K].X + Hu * Oct[K].Y;
            Rear[K] = Hc - Hf * HalfL + R * Oct[K].X + Hu * Oct[K].Y;
        }
        for (int32 K = 0; K < 8; ++K)
        {
            const int32 L = (K + 1) % 8;
            const FVector2D Mid2 = (Oct[K] + Oct[L]) * .5;
            Quad(B, MatPaint, EColl::None, Rear[K], Rear[L], Front[L], Front[K], R * Mid2.X + Hu * Mid2.Y);
        }
        Poly(B, MatPaint, EColl::None, Front, 8, Hf);
        Poly(B, MatPaint, EColl::None, Rear, 8, -Hf);
        // Objectives (dark glass far back in their tubes), the visor and the rubber eyecups, the coin box, the handles.
        for (const double Side : { -7.0, 7.0 })
        {
            const FVector O0 = Hc + Hf * HalfL + R * Side, O1 = O0 + Hf * 6.5;
            Cylinder(B, MatPaint, EColl::None, O0, O1, 6.2, 6.4, 12, false, false);
            Cylinder(B, MatRubber, EColl::None, O1 - Hf * 2.5, O1 - Hf * 2.4, 5.8, 5.8, 12, false, true);
            const FVector E0 = Hc - Hf * (HalfL + 3.0) + R * Side * .93, E1 = E0 - Hf * 5.5;
            Cylinder(B, MatRubber, EColl::None, E0, E1, 3.4, 3.9, 10, false, true);
        }
        Box3(B, MatPaint, EColl::None, Hc - Hf * (HalfL + 1.5) + Hu * 1.0, R, Hf, Hu, 15.0, 1.5, 7.5);
        Box3(B, MatPaint, EColl::None, Hc + R * (HalfW + 4.0) - Hf * 4.0 - Hu * 2.0, R, Hf, Hu, 4.0, 7.0, 6.0);
        Box3(B, MatSteel, EColl::None, Hc + R * (HalfW + 8.15) - Hf * 4.0 - Hu * 1.0, R, Hf, Hu, .3, 3.5, 2.6);
        Box3(B, MatRubber, EColl::None, Hc + R * (HalfW + 8.35) - Hf * 4.0 - Hu * .4, R, Hf, Hu, .2, 1.4, .18);   // the coin slot
        for (const double Side : { -1.0, 1.0 })
        {
            const FVector H0 = Hc + R * (Side * HalfW) - Hf * 12.0 + Hu * 3.0;
            const double Reach = Side > 0.0 ? 9.0 : 8.0;
            Cylinder(B, MatSteel, EColl::None, H0, H0 + R * (Side * (Reach + 4.0)), 1.6, 1.6, 8, false, true);
            if (Side < 0.0) Cylinder(B, MatRubber, EColl::None, H0 + R * (Side * 4.0), H0 + R * (Side * (Reach + 3.5)), 2.2, 2.2, 8, true, true);
        }
        // Chipped paint: rust where hands, rain and the coin box wore it (on the post, the foot's rim and the head's sides).
        for (int32 K = 0; K < 26; ++K)
        {
            const float H0 = Hash01(K, 11, 977u), H1 = Hash01(K, 12, 977u), H2 = Hash01(K, 13, 977u);
            const double Sz = FMath::Lerp(1.2, 3.6, double(H2));
            if (K < 12)
            {
                const double Ang = 2.0 * UE_DOUBLE_PI * H0;
                const FVector Radial = R * FMath::Cos(Ang) + F * FMath::Sin(Ang);
                const FVector Tan = FVector::CrossProduct(Up, Radial);
                const FVector C = Base + Up * FMath::Lerp(22.0, 100.0, double(H1)) + Radial * 6.12;
                Quad(B, MatRust, EColl::None, C - Tan * Sz * .5 - Up * Sz * .35, C + Tan * Sz * .5 - Up * Sz * .35, C + Tan * Sz * .5 + Up * Sz * .35, C - Tan * Sz * .5 + Up * Sz * .35, Radial, .9f);
            }
            else
            {
                const double Side = K % 2 ? 1.0 : -1.0;
                const FVector C = Hc + R * (Side * (HalfW + .12)) + Hf * FMath::Lerp(-17.0, 17.0, double(H0)) + Hu * FMath::Lerp(-7.0, 7.0, double(H1));
                Quad(B, MatRust, EColl::None, C - Hf * Sz * .5 - Hu * Sz * .3, C + Hf * Sz * .5 - Hu * Sz * .3, C + Hf * Sz * .5 + Hu * Sz * .3, C - Hf * Sz * .5 + Hu * Sz * .3, R * Side, .9f);
            }
        }
        // Its collision: the post and the head as hulls (Unwalkable).
        Box3(B, MatCount, EColl::Block, Base + Up * 58.0, R, F, Up, 9.0, 9.0, 56.0);
        Box3(B, MatCount, EColl::Block, Hc, R, Hf, Hu, HalfW + 8.0, HalfL + 9.0, HalfH + 1.0);
        St.OverlookParts += 1;
    }
}

// ------------------------------------------------------------------ the pond overlook's bench
void BuildPondBench(FMeshes& M, FStats& St)
{
    const HouseCalderLandform::FSpot* Spot = FindSpot(HouseCalderLandform::Overlooks(), TEXT("Gander Pond"));
    const HouseCalderLandform::FLandformPath* Loop = nullptr;
    for (const HouseCalderLandform::FLandformPath& P : HouseCalderLandform::LandformPaths()) if (P.Name.Contains(TEXT("Gander Pond"))) Loop = &P;
    const TArray<FVector2D>& Pond = HouseCalderLandform::PondOutline();
    if (!Spot || !Loop || Loop->Points.Num() < 2 || Pond.Num() < 3) { St.Notes.Add(TEXT("CALDER_EDGES_BENCH skipped reason=data-missing")); return; }
    const FVector2D S(Spot->At.X, Spot->At.Y);
    // The loop's stretch nearest the spot: the bench stands off its far side from the water, square to it, facing the pond.
    const int32 Num = Loop->Points.Num();
    double Best = DBL_MAX;
    FVector2D A = Loop->Points[0], Bp = Loop->Points[1];
    for (int32 K = 0; K + 1 < Num + (Loop->bClosed ? 1 : 0); ++K)
    {
        const double D = SegDist(S, Loop->Points[K], Loop->Points[(K + 1) % Num]);
        if (D < Best) { Best = D; A = Loop->Points[K]; Bp = Loop->Points[(K + 1) % Num]; }
    }
    FVector2D PondMid(0, 0);
    for (const FVector2D& P : Pond) PondMid += P;
    PondMid /= double(Pond.Num());
    const FVector2D T = (Bp - A).GetSafeNormal();
    FVector2D Away(-T.Y, T.X);
    if (FVector2D::DotProduct(PondMid - S, Away) > 0.0) Away = -Away;
    const FVector2D OnPath = A + T * FVector2D::DotProduct(S - A, T);
    const double HalfDepth = 30.0;
    const FVector2D C = OnPath + Away * (Loop->Width * .5 + 40.0 + HalfDepth);
    if (InPolygon(C, Pond) || PolylineDist(C, Loop->Points, Loop->bClosed) < Loop->Width * .5 + 30.0)
    {
        St.Notes.Add(TEXT("CALDER_EDGES_BENCH skipped reason=no-room"));
        return;
    }
    const FVector2D Face = -Away, Side = Right2(Face);   // the sitter faces the water
    const FVector F(Face.X, Face.Y, 0.0), R(Side.X, Side.Y, 0.0), Up = FVector::UpVector;
    FBucket& B = M.Area(AreaBench);
    // Feet: each leg reaches the ground under it (set 3 cm in); the seat is level, 44 cm over the feet's mean ground but never
    // under 38 cm over the highest (the shore falls toward the water, so the front legs stand a little longer).
    constexpr double HalfLen = 90.0, EndAt = 78.0;
    double GMax = -DBL_MAX, GSum = 0.0;
    for (const double L : { -EndAt, EndAt })
        for (const double D : { -HalfDepth + 4.0, HalfDepth - 4.0 })
        {
            const double Under = SurfaceAt(C + Side * L + Face * D);
            GMax = FMath::Max(GMax, Under);
            GSum += Under;
        }
    const double Seat = FMath::Max(GSum * .25 + 44.0, GMax + 38.0);
    for (const double L : { -EndAt, EndAt })
    {
        // A cast-iron end frame: the front leg, the rear leg running on up as the back's post, the seat bearer and the arm.
        const FVector2D E = C + Side * L;
        const double GFront = SurfaceAt(E + Face * (HalfDepth - 4.0)), GBack = SurfaceAt(E - Face * (HalfDepth - 4.0));
        Foot(TEXT("CalderPondBench"), E + Face * (HalfDepth - 4.0), GFront - 3.0);
        Foot(TEXT("CalderPondBench"), E - Face * (HalfDepth - 4.0), GBack - 3.0);
        Box3(B, MatIron, EColl::None, At3(E + Face * (HalfDepth - 4.0), (GFront - 3.0 + Seat) * .5), R, F, Up, 2.2, 2.6, (Seat - GFront + 3.0) * .5);
        const FVector RearFoot = At3(E - Face * (HalfDepth - 4.0), GBack - 3.0), RearTop = At3(E - Face * (HalfDepth + 6.0), Seat + 42.0);
        const FVector RearAxis = (RearTop - RearFoot).GetSafeNormal();
        Box3(B, MatIron, EColl::None, (RearFoot + RearTop) * .5, R, FVector::CrossProduct(RearAxis, R).GetSafeNormal(), RearAxis, 2.2, 2.6, (RearTop - RearFoot).Size() * .5);
        Box3(B, MatIron, EColl::None, At3(E, Seat - 4.0), R, F, Up, 2.0, HalfDepth, 2.0);
        Box3(B, MatIron, EColl::None, At3(E - Face * 3.0, Seat + 20.0), R, F, Up, 2.4, HalfDepth - 2.0, 1.6);
        Box3(B, MatIron, EColl::None, At3(E + Face * (HalfDepth - 4.0), Seat + 10.0), R, F, Up, 2.0, 2.0, 10.0);
        // Rust bleeding at the feet.
        Box3(B, MatRust, EColl::None, At3(E + Face * (HalfDepth - 4.0), GFront + 3.0), R, F, Up, 2.4, 2.8, 5.0, SidesOnly);
    }
    // Silvered slats: three on the seat, two on the back (the upper one split and sagging at one end).
    for (int32 K = 0; K < 3; ++K)
    {
        const double D = -HalfDepth + 7.0 + K * 13.5;
        Box3(B, MatTimber, EColl::None, At3(C + Face * D, Seat + 1.8), R, F, Up, HalfLen, 5.5, 1.8);
    }
    const double BackTilt = FMath::DegreesToRadians(14.0);
    const FVector BackUp = (Up * FMath::Cos(BackTilt) - F * FMath::Sin(BackTilt)).GetSafeNormal();
    const FVector BackOut = FVector::CrossProduct(R, BackUp).GetSafeNormal() * (FVector::DotProduct(FVector::CrossProduct(R, BackUp), F) < 0.0 ? -1.0 : 1.0);
    const FVector BackBase = At3(C - Face * (HalfDepth + 1.0), Seat + 6.0);
    Box3(B, MatTimber, EColl::None, BackBase + BackUp * 14.0, R, BackOut, BackUp, HalfLen, 1.6, 5.0);
    // The upper back slat, split: a long piece and a short one sagging 2 cm.
    Box3(B, MatTimber, EColl::None, BackBase + BackUp * 30.0 - R * 21.0, R, BackOut, BackUp, HalfLen - 21.0, 1.6, 5.0);
    {
        const FVector P0 = BackBase + BackUp * 30.0 + R * (HalfLen - 40.0), P1 = BackBase + BackUp * 28.0 + R * HalfLen;
        const FVector Ax = (P1 - P0).GetSafeNormal();
        Box3(B, MatTimber, EColl::None, (P0 + P1) * .5, Ax, BackOut, FVector::CrossProduct(Ax, BackOut).GetSafeNormal() * (FVector::CrossProduct(Ax, BackOut).Z < 0.0 ? -1.0 : 1.0), (P1 - P0).Size() * .5, 1.6, 5.0);
    }
    // A small bronze plate on the lower back slat.
    {
        const FVector PlateC = BackBase + BackUp * 14.0 + BackOut * 1.75;
        Box3(B, MatBronze, EColl::None, PlateC, R, BackOut, BackUp, 10.0, .15, 2.6, AllFaces);
        const FVector Origin = PlateC + BackOut * .15 - R * 10.0 - BackUp * 2.6;
        // (the text reads left to right for someone facing the bench: R points to their left)
        St.Glyphs += Lettering(B, MatBronze, Origin + R * 20.0, -R, BackUp, BackOut, 10.0, 1.9, TEXT("CLASS OF 1958"), 1.3f, 0, .18);
    }
    // Collision: the seat and the back as hulls; the seat stays walkable (anyone may stand on a bench).
    Box3(B, MatCount, EColl::Walk, At3(C, (GMax - 20.0 + Seat + 3.6) * .5), R, F, Up, HalfLen, HalfDepth, (Seat + 3.6 - GMax + 20.0) * .5);
    Box3(B, MatCount, EColl::Block, BackBase + BackUp * 22.0, R, BackOut, BackUp, HalfLen, 3.0, 17.0);
    ++St.Benches;
    St.Notes.Add(FString::Printf(TEXT("CALDER_EDGES_BENCH at=(%.0f,%.0f) facingYaw=%.0f seatZ=%.0f offPath=%.0f"), C.X, C.Y, AzimuthTo(FVector2D::ZeroVector, Face), Seat, PolylineDist(C, Loop->Points, Loop->bClosed)));
}

// ------------------------------------------------------------------ Crow Ledge: its flags and the trail's fence
const HouseCalderLandform::FLandformPath* CrowTrail()
{
    for (const HouseCalderLandform::FLandformPath& P : HouseCalderLandform::LandformPaths()) if (P.Name.Contains(TEXT("Crow"))) return &P;
    return nullptr;
}

FBox2D LedgeBox()
{
    FBox2D Box(ForceInit);
    for (const FVector2D& P : HouseCalderLandform::CrowLedge().Outline) Box += P;
    return Box;
}

void BuildCrowLedge(FMeshes& M, FStats& St)
{
    FBucket& B = M.Area(AreaCrow);
    const FBox2D Ledge = LedgeBox();
    const HouseCalderLandform::FLandformPath* Trail = CrowTrail();
    if (!Ledge.bIsValid || !Trail || Trail->Points.Num() < 2) { St.Notes.Add(TEXT("CALDER_EDGES_CROW skipped reason=data-missing")); return; }
    // Sandstone flags on the ledge's floor, clear of the rock lips (50 cm in from the drop and the corners' walls, 90 cm from the
    // rock faces behind): scenery 2-3 cm proud, no collision (the ground under them is the floor), a few missing.
    {
        const double X0 = Ledge.Min.X + 90.0, X1 = Ledge.Max.X - 50.0, Y0 = Ledge.Min.Y + 50.0, Y1 = Ledge.Max.Y - 90.0;
        const int32 NX = FMath::Max(1, FMath::RoundToInt((X1 - X0) / 105.0)), NY = FMath::Max(1, FMath::RoundToInt((Y1 - Y0) / 90.0));
        auto Line = [](double V0, double V1, int32 Count, int32 I, uint32 Seed)
        {
            if (I <= 0) return V0;
            if (I >= Count) return V1;
            return V0 + (V1 - V0) * I / Count + 18.0 * (Hash01(I, 0, Seed) - .5);
        };
        for (int32 J = 0; J < NY; ++J)
            for (int32 I = 0; I < NX; ++I)
            {
                if (Hash01(I, J, 1201u) < .09f) continue;
                const double SX0 = Line(X0, X1, NX, I, 1202u + J % 2) + 2.0, SX1 = Line(X0, X1, NX, I + 1, 1202u + J % 2) - 2.0;
                const double SY0 = Line(Y0, Y1, NY, J, 1203u) + 2.0, SY1 = Line(Y0, Y1, NY, J + 1, 1203u) - 2.0;
                if (SX1 - SX0 < 20.0 || SY1 - SY0 < 20.0) continue;
                const FVector2D Corner[4] = { FVector2D(SX0, SY0), FVector2D(SX1, SY0), FVector2D(SX1, SY1), FVector2D(SX0, SY1) };
                double GLo = DBL_MAX, GHi = -DBL_MAX;
                FVector2D LowAt = Corner[0];
                for (const FVector2D& P : Corner)
                {
                    const double G = SurfaceAt(P);
                    if (G < GLo) LowAt = P;
                    GLo = FMath::Min(GLo, G);
                    GHi = FMath::Max(GHi, G);
                }
                if (GHi - GLo > 6.0) continue;   // only where the floor is level
                const double Top = GHi + 1.9 + 1.0 * Hash01(I, J, 1204u);
                FVector Low[4], High[4];
                for (int32 K = 0; K < 4; ++K)
                {
                    const FVector2D Jit(4.0 * (Hash01(I * 4 + K, J, 1205u) - .5), 4.0 * (Hash01(I * 4 + K, J, 1206u) - .5));
                    const FVector2D P = Corner[K] + Jit, Inset = (FVector2D((SX0 + SX1) * .5, (SY0 + SY1) * .5) - P).GetSafeNormal() * 1.5;
                    Low[K] = At3(P, GLo - 1.5);
                    High[K] = At3(P + Inset, Top);
                }
                Hexa(B, MatLedge, EColl::None, Low, High, SidesAndTop, .75f, 1.f);
                Foot(TEXT("CalderCrowFlags"), LowAt, GLo - 1.5);
                ++St.Flags;
            }
    }
    // The trail's fence: along each of its cut walls, wherever the trail's side drops 75 cm or more within 1 m (the trail is the
    // wall's high side), a weathered timber post-and-rail fence 30 cm in from the wall line, on the rock's top.
    for (const HouseCalderLandform::FWall& Wl : HouseCalderLandform::Walls())
    {
        if (!Wl.Name.Contains(TEXT("Crow Ledge trail")) || Wl.Kind != HouseCalderLandform::EWallKind::RockCut) continue;
        const FVector2D Span = Wl.B - Wl.A;
        const double Len = Span.Size();
        if (Len < 100.0) continue;
        const FVector2D Dir = Span / Len;
        FVector2D Nt(-Dir.Y, Dir.X);
        if (FVector2D::DotProduct(Trail->Points[0] - Wl.A, Nt) < 0.0) Nt = -Nt;   // toward the trail
        TArray<double> Flagged;
        for (const double S : Spaced(0.0, Len, 50.0))
        {
            const FVector2D P = Wl.A + Dir * S;
            const double Hi = GroundAt(P + Nt * 3.0);
            double Lo = DBL_MAX;
            for (const double O : { 30.0, 60.0, 100.0 }) Lo = FMath::Min(Lo, GroundAt(P - Nt * O));
            if (Hi - Lo >= RailDrop) Flagged.Add(S);
        }
        int32 K = 0;
        while (K < Flagged.Num())
        {
            int32 E = K;
            while (E + 1 < Flagged.Num() && Flagged[E + 1] - Flagged[E] <= 150.0) ++E;
            double S0 = FMath::Max(0.0, Flagged[K] - 100.0), S1 = FMath::Min(Len, Flagged[E] + 100.0);
            // Run on to the wall's end where the run comes within 1.2 m of it (the ledge's own rail takes over there).
            if (S0 < 120.0) S0 = 0.0;
            if (Len - S1 < 120.0) S1 = Len;
            K = E + 1;
            const TArray<double> Posts = Spaced(S0, S1, 190.0);
            TArray<FVector> Bases;
            for (int32 PI0 = 0; PI0 < Posts.Num(); ++PI0)
            {
                const FVector2D P = Wl.A + Dir * Posts[PI0] + Nt * 30.0;
                const FVector BaseAt = At3(P, GroundAt(P) + 2.0);
                Bases.Add(BaseAt);
                Foot(TEXT("CalderCrowFence"), P, GroundAt(P));   // where it comes out of the ground (set 33 cm into the cut's rock top)
                // A split post, leaning a degree or two.
                const FVector Lean = (FVector::UpVector + FVector(Dir.X, Dir.Y, 0.0) * (.035 * (Hash01(PI0, 1, 1301u) - .5)) + FVector(Nt.X, Nt.Y, 0.0) * (.03 * (Hash01(PI0, 2, 1301u) - .5))).GetSafeNormal();
                const FVector Across(Nt.X, Nt.Y, 0.0);
                const FVector Along = FVector::CrossProduct(Across, Lean).GetSafeNormal();
                Box3(B, MatTimber, EColl::Block, BaseAt + Lean * ((108.0 - 35.0) * .5), Along, Across, Lean, 5.5, 5.5, (108.0 + 35.0) * .5, SidesAndTop, .7f, 1.f);
            }
            for (int32 J = 0; J + 1 < Bases.Num(); ++J)
                for (const float H : { RailTop, RailMid, RailLow })
                {
                    const FVector P0 = Bases[J] + FVector(Nt.X, Nt.Y, 0.0) * 8.0 + FVector(0, 0, H), P1 = Bases[J + 1] + FVector(Nt.X, Nt.Y, 0.0) * 8.0 + FVector(0, 0, H);
                    const FVector Ax = (P1 - P0).GetSafeNormal();
                    const FVector Across(Nt.X, Nt.Y, 0.0);
                    FVector RailUp = FVector::CrossProduct(Ax, Across).GetSafeNormal();
                    if (RailUp.Z < 0.0) RailUp = -RailUp;
                    Box3(B, MatTimber, EColl::Block, (P0 + P1) * .5, Ax, Across, RailUp, (P1 - P0).Size() * .5 + 4.0, 2.5, 4.5, AllFaces, .8f, 1.f);
                }
            St.TrailRailCm += S1 - S0;
            ++St.TrailRuns;
            St.Notes.Add(FString::Printf(TEXT("CALDER_EDGES_TRAILRAIL wall=\"%s\" from=(%.0f,%.0f) to=(%.0f,%.0f) posts=%d"), *Wl.Name,
                (Wl.A + Dir * S0).X, (Wl.A + Dir * S0).Y, (Wl.A + Dir * S1).X, (Wl.A + Dir * S1).Y, Posts.Num()));
        }
    }
}

// ------------------------------------------------------------------ Crow Ledge: woods trees round the trail
struct FTreePlan { HouseNature::ETreeKind Kind; int32 Variant; TArray<FTransform> Xf; TArray<FVector> Feet; };

/** P8's eye on Crow Ledge (the eye spot, else the ledge's south edge). */
FVector2D CrowEye()
{
    const FBox2D Ledge = LedgeBox();
    FVector2D Eye = Ledge.bIsValid ? FVector2D(Ledge.GetCenter().X, Ledge.Min.Y + 100.0) : FVector2D::ZeroVector;
    for (const HouseCalderLandform::FSpot& S : HouseCalderLandform::EyeSpots()) if (S.Name.StartsWith(TEXT("P8"))) Eye = FVector2D(S.At.X, S.At.Y);
    return Eye;
}

/** P lies in the P8 view south from the ledge: 55 degrees either side of -Y, widened by 7 m (a tree's crown). */
bool InCrowView(const FVector2D& Eye, const FVector2D& P)
{
    const FVector2D ViewDir = YawDir(270.0);
    const FVector2D V = P - Eye;
    return FVector2D::DotProduct(V, ViewDir) > -300.0 && FMath::Abs(Cross2(ViewDir, V)) < FMath::Tan(FMath::DegreesToRadians(55.0)) * FMath::Max(0.0, FVector2D::DotProduct(V, ViewDir)) + 700.0;
}

/** Woods trees with trunk collision where the distant forest is hidden: on a jittered 6.5 m grid within 22 m of the trail and
 *  18 m round the ledge, off the trail's bench (3.3 m from its line), 1.6 m from any declared wall, off stairs, Calder Dr and
 *  the frozen zones, on ground no steeper than 1:1, and never in the P8 view south from the ledge (InCrowView). */
TArray<FTreePlan> PlanTrees()
{
    TArray<FTreePlan> Plans;
    Plans.Add({ HouseNature::ETreeKind::Oak, 0, {}, {} });
    Plans.Add({ HouseNature::ETreeKind::Oak, 1, {}, {} });
    Plans.Add({ HouseNature::ETreeKind::Maple, 2, {}, {} });
    const HouseCalderLandform::FLandformPath* Trail = CrowTrail();
    const FBox2D Ledge = LedgeBox();
    if (!Trail || !Ledge.bIsValid) return Plans;
    const FVector2D Eye = CrowEye();
    FBox2D Area(ForceInit);
    for (const FVector2D& P : Trail->Points) Area += P;
    Area += Ledge;
    Area = Area.ExpandBy(2400.0);
    TArray<const HouseCalderLandform::FWall*> NearWalls;
    for (const HouseCalderLandform::FWall& Wl : HouseCalderLandform::Walls())
        if (FBox2D(FVector2D(FMath::Min(Wl.A.X, Wl.B.X), FMath::Min(Wl.A.Y, Wl.B.Y)), FVector2D(FMath::Max(Wl.A.X, Wl.B.X), FMath::Max(Wl.A.Y, Wl.B.Y))).ExpandBy(200.0).Intersect(Area)) NearWalls.Add(&Wl);
    constexpr double Pitch = 650.0;
    const int32 I0 = FMath::FloorToInt(Area.Min.X / Pitch), I1 = FMath::CeilToInt(Area.Max.X / Pitch);
    const int32 J0 = FMath::FloorToInt(Area.Min.Y / Pitch), J1 = FMath::CeilToInt(Area.Max.Y / Pitch);
    for (int32 J = J0; J <= J1; ++J)
        for (int32 I = I0; I <= I1; ++I)
        {
            const FVector2D P(I * Pitch + 440.0 * (Hash01(I, J, 1401u) - .5), J * Pitch + 440.0 * (Hash01(I, J, 1402u) - .5));
            const double DTrail = PolylineDist(P, Trail->Points, false), DLedge = BoxDist(P, Ledge);
            if (DTrail < 330.0 || DLedge < 260.0) continue;
            if (DTrail > 2200.0 && DLedge > 1800.0) continue;
            if (P.X > 8150.0 || FMath::Abs(P.Y) > 31000.0 || FMath::Abs(P.X) > 31000.0) continue;   // Calder Dr and the edge bands
            if (InCrowView(Eye, P)) continue;
            if (HouseCalderTerrain::InFrozen(float(P.X), float(P.Y)) || HouseCalderFeatures::InStair(P, 150.f) || HouseCalderFeatures::InRamp(P, 150.f)) continue;
            bool bNearWall = false;
            for (const HouseCalderLandform::FWall* Wl : NearWalls) bNearWall |= SegDist(P, Wl->A, Wl->B) < 160.0;
            if (bNearWall) continue;
            const double GX = (GroundAt(P + FVector2D(100, 0)) - GroundAt(P - FVector2D(100, 0))) / 200.0, GY = (GroundAt(P + FVector2D(0, 100)) - GroundAt(P - FVector2D(0, 100))) / 200.0;
            if (GX * GX + GY * GY > 1.0) continue;
            double Seat = DBL_MAX;
            FVector2D SeatAt = P;
            for (const FVector2D& O : { FVector2D(0, 0), FVector2D(60, 60), FVector2D(-60, 60), FVector2D(60, -60), FVector2D(-60, -60) })
            {
                const double Under = SurfaceAt(P + O);
                if (Under < Seat) { Seat = Under; SeatAt = P + O; }
            }
            const float Pick = Hash01(I, J, 1403u);
            FTreePlan& Plan = Plans[Pick < .3f ? 0 : Pick < .55f ? 1 : 2];
            const float Scale = FMath::Lerp(1.1f, 1.45f, Hash01(I, J, 1404u));
            Plan.Xf.Add(FTransform(FRotator(0.f, 360.f * Hash01(I, J, 1405u), 0.f), At3(P, Seat - 4.0), FVector(Scale, Scale, Scale * FMath::Lerp(.92f, 1.08f, Hash01(I, J, 1406u)))));
            Plan.Feet.Add(FVector(SeatAt.X, SeatAt.Y, Seat - 4.0));
        }
    return Plans;
}

int32 PlantTrees(AActor* Owner, UHouseBlackHallSubsystem* Look, FStats& St)
{
    if (!Look) { St.Notes.Add(TEXT("CALDER_EDGES_TREES skipped reason=no-look")); return 0; }
    int32 Planted = 0;
    for (const FTreePlan& Plan : PlanTrees())
    {
        if (!Plan.Xf.Num()) continue;
        FHallMesh Mesh;
        TArray<FBox> Trunks;
        HouseNature::Tree(Mesh, Plan.Kind, Plan.Variant, Trunks);
        const FString Name = FString::Printf(TEXT("CalderCrowTree%s%d"), HouseNature::TreeName(Plan.Kind), Plan.Variant);
        UStaticMesh* SM = HwBuildStaticMesh(Mesh, *FString::Printf(TEXT("SM_%s"), *Name), Look, Trunks);
        if (!SM) continue;
        SM=HouseCalderMapTrees::Mesh(Owner,Look,Name,SM,Plan.Xf);
        UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner, *Name);
        H->SetStaticMesh(SM);
        H->SetMobility(EComponentMobility::Static);
        H->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        H->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        H->SetCullDistances(int32(26000.f * .72f), 26000);
        H->SetCastShadow(false);
        H->bAffectDistanceFieldLighting = false;
        H->RegisterComponent();
        for (const FTransform& T : Plan.Xf) H->AddInstance(T, true);
        for (const FVector& F : Plan.Feet) Foot(TEXT("CalderCrowTrees"), FVector2D(F.X, F.Y), F.Z);   // the trunk's lowest side
        Planted += Plan.Xf.Num();
        ++St.TreeBatches;
        St.Notes.Add(FString::Printf(TEXT("CALDER_EDGES_TREES batch=%s instances=%d trunkBoxes=%d"), *Name, Plan.Xf.Num(), Trunks.Num()));
    }
    return Planted;
}

// ------------------------------------------------------------------ the hedges (built by the exterior: checked here)
/** The exported hedge segments (<= 2 m, outside the gaps) against the game's ground: bottom 20 cm under the lowest ground under
 *  each, top 190 cm over the highest (to 1 cm). */
void CheckHedges(FStats& St)
{
    for (const HouseCalderLandform::FHedgeSegment& H : HouseCalderLandform::HedgeSegments())
    {
        ++St.HedgeSegs;
        double Lo = DBL_MAX, Hi = -DBL_MAX;
        for (const double F : { 0.0, .25, .5, .75, 1.0 }) { const double G = GroundAt(H.A + (H.B - H.A) * F); Lo = FMath::Min(Lo, G); Hi = FMath::Max(Hi, G); }
        St.HedgeRuleOk += (H.bFrozen || ((H.B - H.A).Size() <= 200.5 && H.BottomZ <= Lo - 19.0 && H.TopZ >= Hi + 189.0)) ? 1 : 0;
    }
}

// ------------------------------------------------------------------ materials, meshes and components
// (The masters are looked up on every build, not cached in statics: a raw pointer kept across worlds would dangle.)
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
        else UE_LOG(LogTemp, Warning, TEXT("CALDER_EDGES texture %s is missing"), *Path);
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
        UE_LOG(LogTemp, Warning, TEXT("CALDER_EDGES M_CalderRock is missing; the rock and stone fall back to the default material"));
    Out[MatSandstone] = RockMaterial(Outer, TEXT("rock_06"), 380.f, .45f, FLinearColor(.74f, .69f, .6f));       // weathered buff sandstone
    Out[MatShale] = RockMaterial(Outer, TEXT("rock_face_04"), 300.f, .22f, FLinearColor(.6f, .58f, .56f));     // dark shale partings
    Out[MatFresh] = RockMaterial(Outer, TEXT("rock_face_04"), 260.f, .06f, FLinearColor(.95f, .9f, .84f));     // the rockslide's broken rock
    Out[MatBoulder] = RockMaterial(Outer, TEXT("lichen_rock"), 240.f, .45f, FLinearColor(.8f, .8f, .76f));     // the creek's old boulders
    Out[MatDressed] = RockMaterial(Outer, TEXT("rock_01"), 120.f, .2f, FLinearColor(.78f, .74f, .66f));        // the pedestal's dressed sandstone
    Out[MatLedge] = RockMaterial(Outer, TEXT("rock_06"), 260.f, .55f, FLinearColor(.7f, .66f, .58f));          // Crow Ledge's flags
    Out[MatTimber] = RockMaterial(Outer, TEXT("rough_wood"), 140.f, .12f, FLinearColor(.72f, .7f, .66f));      // silvered old timber
    Out[MatBronze] = PlainMaterial(Outer, FLinearColor(.075f, .05f, .028f), .5f, .85f);     // dark old bronze
    Out[MatVerdigris] = PlainMaterial(Outer, FLinearColor(.06f, .12f, .095f), .82f, .15f);  // its patina
    Out[MatPaint] = PlainMaterial(Outer, FLinearColor(.028f, .055f, .045f), .68f, .25f);    // faded park-green enamel
    Out[MatRust] = PlainMaterial(Outer, FLinearColor(.07f, .026f, .011f), .92f, .2f);
    Out[MatRubber] = PlainMaterial(Outer, FLinearColor(.012f, .012f, .012f), .86f, 0.f);
    Out[MatSteel] = PlainMaterial(Outer, FLinearColor(.16f, .155f, .145f), .55f, .8f);      // worn bare steel
    Out[MatIron] = PlainMaterial(Outer, FLinearColor(.025f, .024f, .024f), .78f, .45f);     // cast iron, near black
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

bool HouseCalderEdges::Active()
{
    return HouseCalderFeatures::Active();
}

bool HouseCalderEdges::HidesForestDistant(const FVector& At)
{
    if (!Active()) return false;
    namespace EI = HouseCalderEdgesImpl;
    static const TArray<FVector2D> Trail = []
    {
        const HouseCalderLandform::FLandformPath* P = EI::CrowTrail();
        return P ? P->Points : TArray<FVector2D>();
    }();
    static const FBox2D Ledge = EI::LedgeBox();
    static const FVector2D Eye = EI::CrowEye();
    const FVector2D P(At.X, At.Y);
    if ((Trail.Num() && EI::PolylineDist(P, Trail, false) < EI::ForestHideRadius) || (Ledge.bIsValid && EI::BoxDist(P, Ledge) < EI::ForestHideRadius)) return true;
    // the P8 view stays open down the slope to the old property's north edge (its frozen woods keep every tree, and so does
    // the forest on either side of the property)
    return Ledge.bIsValid && P.Y > EI::CrowViewOpenToY && EI::InCrowView(Eye, P) && !HouseCalderTerrain::InFrozen(float(P.X), float(P.Y));
}

int32 HouseCalderEdges::Build(AActor* Owner, UHouseBlackHallSubsystem* Look, const TArray<FTransform>& ForestDistant)
{
    namespace EI = HouseCalderEdgesImpl;
    if (!Owner || !Owner->GetWorld()) return 0;
    if (!Active())
    {
        UE_LOG(LogTemp, Display, TEXT("CALDER_EDGES off (the Calder terrain is off or its relief is flat)"));
        return 0;
    }
    const double Start = FPlatformTime::Seconds();
    EI::FMeshes Meshes;
    EI::FStats St;
    EI::BuildBands(Meshes, St);
    EI::BuildOverlook(Meshes, St);
    EI::BuildPondBench(Meshes, St);
    EI::BuildCrowLedge(Meshes, St);
    EI::CheckHedges(St);
    for (const FTransform& T : ForestDistant) St.HiddenForest += HidesForestDistant(T.GetLocation()) ? 1 : 0;

    const bool bDraw = Owner->GetWorld()->GetNetMode() != NM_DedicatedServer;
    UMaterialInterface* Mats[EI::MatCount] = {};
    if (bDraw) EI::Materials(Owner, Mats);
    int32 Drawn = 0, Parts = 0, CollTris = 0;
    for (int32 Index = 0; Index < EI::AreaCount; ++Index)
    {
        const EI::FBucket& B = Meshes.Buckets[Index];
        const FString Prefix = EI::BucketName(Index);
        EI::AddCollision(Owner, B.Walk, Prefix + TEXT("_Collision"), false);
        EI::AddCollision(Owner, B.Block, Prefix + TEXT("_CollisionUnwalkable"), true);
        CollTris += (B.Walk.I.Num() + B.Block.I.Num()) / 3;
        if (!bDraw) continue;
        int32 Tris = 0;
        UStaticMesh* Mesh = EI::BuildMesh(B, Mats, Prefix, Tris);
        if (!Mesh) continue;
        UStaticMeshComponent* Draw = NewObject<UStaticMeshComponent>(Owner, *Prefix);
        Draw->SetMobility(EComponentMobility::Static);
        Draw->SetStaticMesh(Mesh);
        Draw->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Draw->SetCastShadow(false);
        Draw->bAffectDistanceFieldLighting = false;
        Draw->SetCullDistance(EI::BucketCull(Index));
        Draw->RegisterComponent();
        Drawn += Tris;
        ++Parts;
    }
    St.Trees = EI::PlantTrees(Owner, Look, St);
    for (const FString& Note : St.Notes) UE_LOG(LogTemp, Display, TEXT("%s"), *Note);
    UE_LOG(LogTemp, Display, TEXT("CALDER_EDGES segments=%d rockM=%.0f hedgeSegs=%d hiddenForestDistant=%d plantedTrees=%d ms=%.0f bands=%d returns=%d corners=%d columns=%d minRiseCm=%.0f maxRiseCm=%.0f boulders=%d tris=%d collisionTris=%d meshes=%d overlookParts=%d glyphs=%d labels=%d skylineMs=%.0f benches=%d flags=%d trailRailRuns=%d trailRailM=%.1f treeBatches=%d hedgeRuleOk=%d/%d frozenHits=%d"),
        St.Segments, St.RockCm / 100.0, St.HedgeSegs, St.HiddenForest, St.Trees, (FPlatformTime::Seconds() - Start) * 1000.0,
        St.Bands, St.Returns, St.Corners, St.Columns, St.MinRiseSeen == FLT_MAX ? 0.f : St.MinRiseSeen, St.MaxBandTop, St.Boulders, Drawn, CollTris, Parts,
        St.OverlookParts, St.Glyphs, St.Labels, St.SkylineMs, St.Benches, St.Flags, St.TrailRuns, St.TrailRailCm / 100.0, St.TreeBatches,
        St.HedgeRuleOk, St.HedgeSegs, St.FrozenHits);
    return Drawn;
}
