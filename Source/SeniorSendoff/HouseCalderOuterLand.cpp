// The Calder outer land's drawn ground (see HouseCalderOuterLand.h).
#include "HouseCalderOuterLand.h"
#include "HouseCalderFarLand.h"
#include "HouseCalderLandform.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderRidgeForest.h"
#include "HouseExteriorSiteLayout.h"
#include "Async/ParallelFor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"

namespace HouseCalderOuterLandImpl
{
constexpr double SkirtDepth = 200.0;   // every ring's inner edge carries a 2 m skirt under it
constexpr double NormalStep = 100.0;   // vertex normals: central differences of the ground, the same step everywhere
// Rows added across the town's river (Y -45000) in rings 0 and 1: its banks, the trough, and both sides of the 3.5 m step
// the near ridge starts with at Y -45600, so the water shows between its banks and the far bank stands as a cut bank.
constexpr double RiverRows[] = { -44400.0, -44600.0, -44800.0, -45000.0, -45400.0, -45599.0, -45600.0 };

enum ESide : int32 { SideS = 0, SideN = 1, SideW = 2, SideE = 3 };
const TCHAR* const SideNames[4] = { TEXT("S"), TEXT("N"), TEXT("W"), TEXT("E") };

struct FRect { double X0, X1, Y0, Y1; };

/** A point on one side of a rectangle: where along that side it lies (X on S/N, Y on W/E), its order among points at the
 *  same spot (the two sides of a declared wall), and the surface there. */
struct FEdgePoint { double Along = 0.0, Order = 0.0; FVector P = FVector::ZeroVector, N = FVector::UpVector; FLinearColor C = FLinearColor::White; };
struct FEdges { TArray<FEdgePoint> Side[4]; };

/** A finer patch inside a ring (its rectangle on the ring's lines): a steep small landform the ring's step would cut. */
struct FPatch { FRect R; double Step; };

struct FMeshData
{
    TArray<FVector> V, N;
    TArray<FLinearColor> C;
    TArray<int32> I;
    int32 Vertical = 0;
};

FVector NormalAt(double X, double Y)
{
    const double DX = (HouseCalderFarLand::Ground(X + NormalStep, Y) - HouseCalderFarLand::Ground(X - NormalStep, Y)) / (2.0 * NormalStep);
    const double DY = (HouseCalderFarLand::Ground(X, Y + NormalStep) - HouseCalderFarLand::Ground(X, Y - NormalStep)) / (2.0 * NormalStep);
    return FVector(-DX, -DY, 1.0).GetSafeNormal();
}

/** M_CalderGround's layer masks (linear): R rock on the steepest (41-53 deg), G leaf litter (the forest floor) on every
 *  slope past 9-17 deg, the rest lawn, which the material turns to meadow by slope and by this MID's MeadowAmount. */
FLinearColor Paint(const FVector& N)
{
    const double Slope = 1.0 - N.Z;
    return FLinearColor(float(FMath::SmoothStep(0.25, 0.40, Slope)), float(FMath::SmoothStep(0.012, 0.045, Slope)), 0.f, 1.f);
}

int32 AddVertex(FMeshData& M, const FVector& P, const FVector& N, const FLinearColor& C)
{
    M.V.Add(P);
    M.N.Add(N);
    M.C.Add(C);
    return M.V.Num() - 1;
}

/** One triangle facing up (FHallMesh's convention, as the tiles: an up-facing triangle has a downward cross product);
 *  a vertical one (a wall's step carried across the seam, a skirt) gets both faces. */
void AddTri(FMeshData& M, int32 A, int32 B, int32 C)
{
    if (A == B || B == C || A == C) return;
    const FVector Cross = FVector::CrossProduct(M.V[B] - M.V[A], M.V[C] - M.V[A]);
    const double Len = Cross.Size();
    if (Len < 1e-6) return;
    if (FMath::Abs(Cross.Z) > 1e-3 * Len)
    {
        if (Cross.Z < 0.0) M.I.Append({ A, B, C });
        else M.I.Append({ A, C, B });
        return;
    }
    M.I.Append({ A, B, C });
    M.I.Append({ A, C, B });
    ++M.Vertical;
}

FEdgePoint EdgePoint(double Along, double Order, const FVector& P)
{
    FEdgePoint E;
    E.Along = Along;
    E.Order = Order;
    E.P = P;
    E.N = NormalAt(P.X, P.Y);
    E.C = Paint(E.N);
    return E;
}

/** The walkable square's edge as the walkable ground has it: every 2 m lattice vertex on the seam and every frozen
 *  rectangle's edge where it meets the seam (GroundHeight is pinned to today's grade inside them), at
 *  HouseExteriorSite::GroundHeight; and in a cell a declared wall splits, each piece's own vertices on the seam, as the
 *  tiles draw them (so a wall that reaches the edge carries its step into the outer land). */
FEdges SeamEdges()
{
    FEdges Out;
    const double H = HouseCalderTerrain::Half, Cell = HouseCalderTerrain::Cell;
    const int32 Cells = HouseCalderTerrain::Cells;
    const float Scale = HouseCalderTerrain::ReliefScale();
    for (int32 Side = 0; Side < 4; ++Side)
    {
        TArray<FEdgePoint>& Points = Out.Side[Side];
        const bool bAlongX = Side == SideS || Side == SideN;
        const double Fixed = (Side == SideS || Side == SideW) ? -H : H;
        auto AtGround = [&](double Along, double Order)
        {
            const float GX = float(bAlongX ? Along : Fixed), GY = float(bAlongX ? Fixed : Along);
            Points.Add(EdgePoint(Along, Order, FVector(GX, GY, HouseExteriorSite::GroundHeight(GX, GY))));
        };
        auto PiecesAt = [bAlongX, Side](int32 K)   // the wall pieces of the K-th lattice cell along this side
        {
            const int32 I = bAlongX ? K : (Side == SideW ? 0 : HouseCalderTerrain::Cells - 1);
            const int32 J = bAlongX ? (Side == SideS ? 0 : HouseCalderTerrain::Cells - 1) : K;
            return HouseCalderLandform::WallPieces(I, J);
        };
        for (int32 K = 0; K <= Cells; ++K)
        {
            const double A0 = -H + K * Cell;
            const TArray<TArray<FVector>>* Pieces = K < Cells ? PiecesAt(K) : nullptr;
            if (!Pieces) { AtGround(A0, A0 + .5 * Cell); continue; }
            for (const TArray<FVector>& Poly : *Pieces)
            {
                double Mid = 0.0;
                for (const FVector& V : Poly) Mid += bAlongX ? V.X : V.Y;
                Mid /= FMath::Max(1, Poly.Num());
                for (const FVector& V : Poly)
                {
                    if (FMath::Abs((bAlongX ? V.Y : V.X) - Fixed) > .01) continue;
                    const double Along = bAlongX ? V.X : V.Y;
                    const float GX = float(bAlongX ? Along : Fixed), GY = float(bAlongX ? Fixed : Along);
                    // As the tiles draw a piece: today's grade plus the piece's relief.
                    Points.Add(EdgePoint(Along, Mid, FVector(GX, GY, HouseExteriorSite::BaseGrade(GX, GY) + float(V.Z) * Scale)));
                }
            }
        }
        for (const FBox2D& R : HouseCalderTerrain::FrozenRects())
        {
            const double Lo = bAlongX ? R.Min.Y : R.Min.X, Hi = bAlongX ? R.Max.Y : R.Max.X;
            if (Fixed < Lo || Fixed > Hi) continue;
            for (const double Along : { bAlongX ? R.Min.X : R.Min.Y, bAlongX ? R.Max.X : R.Max.Y })
                if (Along > -H && Along < H && !PiecesAt(FMath::Clamp(int32((Along + H) / Cell), 0, Cells - 1))) AtGround(Along, Along);
        }
        Points.Sort([](const FEdgePoint& A, const FEdgePoint& B) { return A.Along != B.Along ? A.Along < B.Along : A.Order < B.Order; });
        TArray<FEdgePoint> Unique;
        for (const FEdgePoint& P : Points)
            if (!Unique.Num() || FMath::Abs(Unique.Last().Along - P.Along) > .01 || FMath::Abs(Unique.Last().P.Z - P.P.Z) > .01) Unique.Add(P);
        Points = MoveTemp(Unique);
    }
    return Out;
}

/** The largest height difference between the outer land's seam edge and the walkable ground (GroundHeight) along it, at
 *  every seam point and halfway between them (a wall's two sides excepted: GroundHeight gives one of them there). */
double SeamMaxGap(const FEdges& Seam)
{
    double Worst = 0.0;
    for (int32 Side = 0; Side < 4; ++Side)
    {
        const TArray<FEdgePoint>& P = Seam.Side[Side];
        for (int32 K = 0; K < P.Num(); ++K)
        {
            const bool bStep = (K > 0 && FMath::Abs(P[K - 1].Along - P[K].Along) < .01) || (K + 1 < P.Num() && FMath::Abs(P[K + 1].Along - P[K].Along) < .01);
            if (!bStep) Worst = FMath::Max(Worst, FMath::Abs(P[K].P.Z - HouseExteriorSite::GroundHeight(float(P[K].P.X), float(P[K].P.Y))));
            // (Between two points, only where neither is pinned by a frozen zone: GroundHeight jumps at a frozen edge, and
            // the walkable ground's own pieces there run straight between the same two points, as this edge does.)
            if (K + 1 < P.Num() && FMath::Abs(P[K + 1].Along - P[K].Along) >= .01
                && !HouseCalderTerrain::InFrozen(float(P[K].P.X), float(P[K].P.Y)) && !HouseCalderTerrain::InFrozen(float(P[K + 1].P.X), float(P[K + 1].P.Y)))
            {
                const FVector M = (P[K].P + P[K + 1].P) * .5;
                Worst = FMath::Max(Worst, FMath::Abs(M.Z - HouseExteriorSite::GroundHeight(float(M.X), float(M.Y))));
            }
        }
    }
    return Worst;
}

/** The points of Edge with Lo <= Along <= Hi, except that at Hi only the first one is taken (a wall's step at a cell
 *  corner belongs to the cell after it, so the two never overlap and never leave a gap). */
void EdgeRange(const TArray<FEdgePoint>& Edge, double Lo, double Hi, TArray<int32>& Out)
{
    Out.Reset();
    int32 First = 0, Last = Edge.Num();
    while (First < Last)   // the first point with Along >= Lo
    {
        const int32 Mid = (First + Last) / 2;
        if (Edge[Mid].Along < Lo - .001) First = Mid + 1;
        else Last = Mid;
    }
    for (int32 K = First; K < Edge.Num() && Edge[K].Along <= Hi + .001; ++K)
    {
        Out.Add(K);
        if (Edge[K].Along >= Hi - .001) break;
    }
}

/** A tensor grid's lines from A to B at Step, plus the given extra lines between them (sorted, unique). */
TArray<double> Lines(double A, double B, double Step, const TArray<double>& Extra)
{
    TArray<double> L;
    const int32 Num = FMath::RoundToInt((B - A) / Step);
    for (int32 K = 0; K <= Num; ++K) L.Add(A + K * Step);
    for (const double E : Extra)
        if (E > A && E < B && !L.Contains(E)) L.Add(E);
    L.Sort();
    return L;
}

/** Heights, normals and paint of every vertex of a tensor grid (the ground is pure: in parallel), skipping the vertices
 *  strictly inside Skip. */
void EvaluateGrid(const TArray<double>& XL, const TArray<double>& YL, const FRect* Skip, TArray<FVector>& GP, TArray<FVector>& GN, TArray<FLinearColor>& GC)
{
    const int32 CX = XL.Num(), CY = YL.Num();
    GP.SetNumZeroed(CX * CY);
    GN.SetNumZeroed(CX * CY);
    GC.SetNumZeroed(CX * CY);
    ParallelFor(CY, [&](int32 J)
    {
        for (int32 I = 0; I < CX; ++I)
        {
            if (Skip && XL[I] > Skip->X0 && XL[I] < Skip->X1 && YL[J] > Skip->Y0 && YL[J] < Skip->Y1) continue;
            const int32 K = J * CX + I;
            GP[K] = FVector(XL[I], YL[J], float(HouseCalderFarLand::Ground(XL[I], YL[J])));
            GN[K] = NormalAt(XL[I], YL[J]);
            GC[K] = Paint(GN[K]);
        }
    });
}

/** A grid's four outer edges (for the ring round it, or the next ring). */
void GridEdges(const TArray<double>& XL, const TArray<double>& YL, const TArray<FVector>& GP, const TArray<FVector>& GN, const TArray<FLinearColor>& GC, FEdges& Out)
{
    const int32 CX = XL.Num(), CY = YL.Num();
    auto Add = [&](int32 Side, double Along, int32 K)
    {
        FEdgePoint E;
        E.Along = Along;
        E.Order = Along;
        E.P = GP[K];
        E.N = GN[K];
        E.C = GC[K];
        Out.Side[Side].Add(E);
    };
    for (int32 Side = 0; Side < 4; ++Side) Out.Side[Side].Reset();
    for (int32 I = 0; I < CX; ++I) { Add(SideS, XL[I], I); Add(SideN, XL[I], (CY - 1) * CX + I); }
    for (int32 J = 0; J < CY; ++J) { Add(SideW, YL[J], J * CX); Add(SideE, YL[J], J * CX + CX - 1); }
}

/** A plain tensor grid over a patch: its quads into M, its edges out. Its lines are its own step plus every ring line
 *  through it, so the ring's cells round it find all their corners on its edges. */
void BuildPatch(const FPatch& Patch, const TArray<double>& RingXL, const TArray<double>& RingYL, FMeshData& M, FEdges& Out)
{
    const TArray<double> XL = Lines(Patch.R.X0, Patch.R.X1, Patch.Step, RingXL), YL = Lines(Patch.R.Y0, Patch.R.Y1, Patch.Step, RingYL);
    TArray<FVector> GP, GN;
    TArray<FLinearColor> GC;
    EvaluateGrid(XL, YL, nullptr, GP, GN, GC);
    const int32 CX = XL.Num(), Base = M.V.Num();
    for (int32 K = 0; K < GP.Num(); ++K) AddVertex(M, GP[K], GN[K], GC[K]);
    for (int32 J = 0; J + 1 < YL.Num(); ++J)
        for (int32 I = 0; I + 1 < CX; ++I)
        {
            const int32 A = Base + J * CX + I, B = A + 1, C = A + CX + 1, D = A + CX;
            AddTri(M, A, B, C);
            AddTri(M, A, C, D);
        }
    GridEdges(XL, YL, GP, GN, GC, Out);
}

/** One ring: a tensor grid over Outer minus Inner (and minus its finer patches). Cells along Inner's edges are fanned to
 *  InnerEdges' points and cells round a patch to the patch's; Outer's edge points are returned for the next ring. Cells
 *  go into Sectors[S/N/W/E] by the side of Inner they lie on. */
void BuildRing(const FRect& Inner, const FEdges& InnerEdges, const FRect& Outer, double Step, bool bRiverRows, const TArray<FPatch>& Patches,
    FEdges& OuterEdges, FMeshData (&Sectors)[4], int32& OutPatches)
{
    TArray<double> Extra;
    if (bRiverRows) for (const double Row : RiverRows) Extra.Add(Row);
    const TArray<double> XL = Lines(Outer.X0, Outer.X1, Step, TArray<double>()), YL = Lines(Outer.Y0, Outer.Y1, Step, Extra);
    const int32 CX = XL.Num(), CY = YL.Num();
    TArray<FVector> GP, GN;
    TArray<FLinearColor> GC;
    EvaluateGrid(XL, YL, &Inner, GP, GN, GC);
    auto SectorOf = [&Inner](double MX, double MY) { return MY < Inner.Y0 ? SideS : MY > Inner.Y1 ? SideN : MX < Inner.X0 ? SideW : SideE; };

    // The holes this ring leaves to finer ground: the inner square (index 0) and the patches on the ring's lines.
    struct FHole { FRect R; const FEdges* Edges; };
    TArray<FHole> Holes;
    Holes.Add({ Inner, &InnerEdges });
    TArray<FEdges> PatchEdges;
    PatchEdges.SetNum(Patches.Num());   // (never reallocated below: Holes keep pointers into it)
    for (int32 P = 0; P < Patches.Num(); ++P)
    {
        const FRect& R = Patches[P].R;
        const bool bOnLines = XL.Contains(R.X0) && XL.Contains(R.X1) && YL.Contains(R.Y0) && YL.Contains(R.Y1);
        const bool bInRing = R.X0 >= Outer.X0 && R.X1 <= Outer.X1 && R.Y0 >= Outer.Y0 && R.Y1 <= Outer.Y1
            && (R.X1 <= Inner.X0 || R.X0 >= Inner.X1 || R.Y1 <= Inner.Y0 || R.Y0 >= Inner.Y1);
        if (!bOnLines || !bInRing) { UE_LOG(LogTemp, Warning, TEXT("CALDER_OUTERLAND patch %d is off the ring's lines or not inside the ring; skipped"), P); continue; }
        BuildPatch(Patches[P], XL, YL, Sectors[SectorOf((R.X0 + R.X1) * .5, (R.Y0 + R.Y1) * .5)], PatchEdges[P]);
        Holes.Add({ R, &PatchEdges[P] });
        ++OutPatches;
    }

    TArray<int32> GridIndex[4];
    TMap<int64, int32> EdgeIndex[4];
    for (int32 S = 0; S < 4; ++S) GridIndex[S].Init(INDEX_NONE, CX * CY);
    auto Grid = [&](int32 Sector, int32 I, int32 J)
    {
        int32& At = GridIndex[Sector][J * CX + I];
        if (At == INDEX_NONE) At = AddVertex(Sectors[Sector], GP[J * CX + I], GN[J * CX + I], GC[J * CX + I]);
        return At;
    };
    auto Edge = [&](int32 Sector, int32 Hole, int32 Side, int32 K)
    {
        const int64 Key = (int64(Hole) << 40) | (int64(Side) << 32) | int64(K);
        if (const int32* Found = EdgeIndex[Sector].Find(Key)) return *Found;
        const FEdgePoint& P = Holes[Hole].Edges->Side[Side][K];
        return EdgeIndex[Sector].Add(Key, AddVertex(Sectors[Sector], P.P, P.N, P.C));
    };

    TArray<int32> Range;
    for (int32 J = 0; J + 1 < CY; ++J)
        for (int32 I = 0; I + 1 < CX; ++I)
        {
            const double X0 = XL[I], X1 = XL[I + 1], Y0 = YL[J], Y1 = YL[J + 1];
            bool bInHole = false;
            for (const FHole& H : Holes) bInHole |= X0 >= H.R.X0 && X1 <= H.R.X1 && Y0 >= H.R.Y0 && Y1 <= H.R.Y1;
            if (bInHole) continue;
            const int32 Sector = SectorOf((X0 + X1) * .5, (Y0 + Y1) * .5);
            FMeshData& M = Sectors[Sector];
            // A cell with an edge on a hole's edge: the hole side it meets and its own two corners away from it.
            int32 Hole = INDEX_NONE, Side = INDEX_NONE, QA = 0, QB = 0;
            double Lo = 0.0, Hi = 0.0;
            for (int32 K = 0; K < Holes.Num() && Hole == INDEX_NONE; ++K)
            {
                const FRect& R = Holes[K].R;
                const bool bAcross = X0 >= R.X0 && X1 <= R.X1, bAlong = Y0 >= R.Y0 && Y1 <= R.Y1;
                if (bAcross && Y1 == R.Y0) { Side = SideS; QA = Grid(Sector, I, J); QB = Grid(Sector, I + 1, J); Lo = X0; Hi = X1; }
                else if (bAcross && Y0 == R.Y1) { Side = SideN; QA = Grid(Sector, I, J + 1); QB = Grid(Sector, I + 1, J + 1); Lo = X0; Hi = X1; }
                else if (bAlong && X1 == R.X0) { Side = SideW; QA = Grid(Sector, I, J); QB = Grid(Sector, I, J + 1); Lo = Y0; Hi = Y1; }
                else if (bAlong && X0 == R.X1) { Side = SideE; QA = Grid(Sector, I + 1, J); QB = Grid(Sector, I + 1, J + 1); Lo = Y0; Hi = Y1; }
                if (Side != INDEX_NONE) Hole = K;
            }
            if (Hole == INDEX_NONE)
            {
                const int32 A = Grid(Sector, I, J), B = Grid(Sector, I + 1, J), C = Grid(Sector, I + 1, J + 1), D = Grid(Sector, I, J + 1);
                AddTri(M, A, B, C);
                AddTri(M, A, C, D);
                continue;
            }
            // Fan the cell from its two far corners to the hole edge's points: the first half to QA, the rest to QB.
            const TArray<FEdgePoint>& EdgePoints = Holes[Hole].Edges->Side[Side];
            EdgeRange(EdgePoints, Lo, Hi, Range);
            if (Range.Num() < 2) continue;
            const double Half = (Lo + Hi) * .5;
            int32 Split = 0;
            for (int32 K = 0; K < Range.Num(); ++K)
                if (FMath::Abs(EdgePoints[Range[K]].Along - Half) < FMath::Abs(EdgePoints[Range[Split]].Along - Half)) Split = K;
            for (int32 K = 0; K < Split; ++K) AddTri(M, Edge(Sector, Hole, Side, Range[K]), Edge(Sector, Hole, Side, Range[K + 1]), QA);
            AddTri(M, Edge(Sector, Hole, Side, Range[Split]), QB, QA);
            for (int32 K = Split; K + 1 < Range.Num(); ++K) AddTri(M, Edge(Sector, Hole, Side, Range[K]), Edge(Sector, Hole, Side, Range[K + 1]), QB);
        }

    // Skirts: 2 m down from every segment of the inner edge (both faces), in that side's sector.
    for (int32 Side = 0; Side < 4; ++Side)
    {
        const TArray<FEdgePoint>& P = InnerEdges.Side[Side];
        FMeshData& M = Sectors[Side];
        int32 PrevLow = INDEX_NONE;
        for (int32 K = 0; K < P.Num(); ++K)
        {
            const int32 Low = AddVertex(M, P[K].P - FVector(0.0, 0.0, SkirtDepth), P[K].N, P[K].C);
            if (K > 0 && FMath::Abs(P[K].Along - P[K - 1].Along) >= .01)
            {
                AddTri(M, Edge(Side, 0, Side, K - 1), Edge(Side, 0, Side, K), Low);
                AddTri(M, Edge(Side, 0, Side, K - 1), Low, PrevLow);
            }
            PrevLow = Low;
        }
    }
    GridEdges(XL, YL, GP, GN, GC, OuterEdges);
}

UStaticMesh* BuildMesh(const FMeshData& T, UMaterialInterface* Material, const FString& Name)
{
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
    const FPolygonGroupID Group = Description.CreatePolygonGroup();
    Slots[Group] = TEXT("Ground");
    TArray<FVertexInstanceID> Instances;
    Instances.Reserve(T.V.Num());
    for (int32 K = 0; K < T.V.Num(); ++K)
    {
        const FVertexID Vertex = Description.CreateVertex();
        Positions[Vertex] = FVector3f(T.V[K]);
        const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
        const FVector Tangent = (FVector(1, 0, 0) - T.N[K] * T.N[K].X).GetSafeNormal();
        Normals[Instance] = FVector3f(T.N[K]);
        Tangents[Instance] = FVector3f(Tangent);
        Signs[Instance] = 1.f;
        Colors[Instance] = FVector4f(T.C[K].R, T.C[K].G, T.C[K].B, T.C[K].A);
        // M_CalderGround projects its textures by world position; the UV is metres, kept at full precision this far out.
        UVs.Set(Instance, 0, FVector2f(float(T.V[K].X / 100.0), float(T.V[K].Y / 100.0)));
        Instances.Add(Instance);
    }
    for (int32 K = 0; K + 2 < T.I.Num(); K += 3)
        Description.CreatePolygon(Group, TArray<FVertexInstanceID, TInlineAllocator<3>>{ Instances[T.I[K]], Instances[T.I[K + 1]], Instances[T.I[K + 2]] });
    UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UStaticMesh::StaticClass(), FName(*Name)), RF_Transient);
    Mesh->NeverStream = true;
    Mesh->bAllowCPUAccess = false;
    Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Ground")
#if WITH_EDITORONLY_DATA
        , TEXT("Ground")
#endif
    ));
    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = false;
    Params.bFastBuild = true;
    Params.bAllowCpuAccess = false;
    Params.bMarkPackageDirty = false;
    Params.bCommitMeshDescription = false;
    UStaticMesh::FBuildMeshDescriptionsLODParams LODParams;
    LODParams.bUseFullPrecisionUVs = true;
    Params.PerLODOverrides.Add(LODParams);
    return Mesh->BuildFromMeshDescriptions({ &Description }, Params) ? Mesh : nullptr;
}

UMaterialInterface* GroundMaterial(UObject* Outer)
{
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderGround.M_CalderGround"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Base)
    {
        UE_LOG(LogTemp, Warning, TEXT("CALDER_OUTERLAND M_CalderGround is missing; the outer land falls back to the default material"));
        return nullptr;
    }
    UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Outer, TEXT("CalderOuterLand_Ground"));
    // The valley's fields in October: withered meadow rather than mown lawn, wherever the litter and rock leave lawn.
    if (Mid) Mid->SetScalarParameterValue(TEXT("MeadowAmount"), .5f);
    return Mid;
}

/** The rings (each rectangle's edges on the lines of both rings that meet there). Ring 0 reaches 160 m further south
 *  than elsewhere so the near ridge's lower slope, where the town's houses and trees stand, is drawn at 8 m; ring 1 (16 m:
 *  the cove walls' spurs are 70 m noise) reaches west to take in Gosling Knob's summit and its campus-facing flanks (its
 *  west and south feet, beyond X -128000 and Y -102400, are ring 2's; its skyline from campus stays within 0.1 deg);
 *  ring 2 is 64 m (the mid and far ridges' crests stay within ~0.3 deg of the true skyline from campus; at 128 m they
 *  were off by up to 1.2 deg). */
struct FRingSpec { FRect Outer; double Step; bool bRiverRows; };
const FRingSpec Rings[3] = {
    { { -48000.0, 48000.0, -64000.0, 48000.0 }, 800.0, true },
    { { -128000.0, 102400.0, -102400.0, 102400.0 }, 1600.0, true },
    { { -320000.0, 320000.0, -320000.0, 153600.0 }, 6400.0, false },
};
/** Finer patches, on their ring's lines: the water tower's knob (26 m high, 64 m across; the tower's legs stand on it) at
 *  4 m in ring 0, and Gosling Knob's summit dome and upper flanks (landform.py gosling_knob(): 380 m above the campus,
 *  33-deg flanks to river level 730 m round the summit, the 110 m dome within this patch) at 8 m in ring 1. Keep them in
 *  step with landform.py if those forms move. */
TArray<FPatch> RingPatches(int32 Ring)
{
    TArray<FPatch> P;
    if (Ring == 0) P.Add({ { 7200.0, 13600.0, -53600.0, -47200.0 }, 400.0 });
    if (Ring == 1) P.Add({ { -121600.0, -73600.0, -70400.0, -28800.0 }, 800.0 });
    return P;
}
}

void HouseCalderOuterLand::Build(AActor* Owner)
{
    namespace OL = HouseCalderOuterLandImpl;
    if (!HouseCalderTerrain::Enabled() || !Owner || !Owner->GetWorld() || Owner->GetWorld()->GetNetMode() == NM_DedicatedServer) return;
    const double Start = FPlatformTime::Seconds();
    // Warm every lazily built table the ground reads (the lattice, the wall pieces) on this thread before ParallelFor.
    HouseExteriorSite::GroundHeight(0.f, 0.f);
    HouseCalderFarLand::Ground(0.0, -40000.0);

    OL::FEdges Edges = OL::SeamEdges();
    const double SeamGap = OL::SeamMaxGap(Edges);
    int32 SeamPoints = 0;
    for (const TArray<OL::FEdgePoint>& SidePoints : Edges.Side) SeamPoints += SidePoints.Num();

    UMaterialInterface* Material = OL::GroundMaterial(Owner);
    OL::FRect Inner{ -HouseCalderTerrain::Half, HouseCalderTerrain::Half, -HouseCalderTerrain::Half, HouseCalderTerrain::Half };
    int32 Tris = 0, Verts = 0, Meshes = 0, Vertical = 0, Patches = 0;
    for (int32 R = 0; R < 3; ++R)
    {
        OL::FMeshData Sectors[4];
        OL::FEdges Next;
        OL::BuildRing(Inner, Edges, OL::Rings[R].Outer, OL::Rings[R].Step, OL::Rings[R].bRiverRows, OL::RingPatches(R), Next, Sectors, Patches);
        for (int32 S = 0; S < 4; ++S)
        {
            const OL::FMeshData& M = Sectors[S];
            if (M.I.Num() < 3) continue;
            const FString Name = FString::Printf(TEXT("CalderOuterLand_R%d_%s"), R, OL::SideNames[S]);
            UStaticMesh* Mesh = OL::BuildMesh(M, Material, Name);
            if (!Mesh) { UE_LOG(LogTemp, Warning, TEXT("CALDER_OUTERLAND %s failed to build"), *Name); continue; }
            UStaticMeshComponent* Draw = NewObject<UStaticMeshComponent>(Owner, *Name);
            Draw->SetMobility(EComponentMobility::Static);
            Draw->SetStaticMesh(Mesh);
            Draw->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Draw->SetCanEverAffectNavigation(false);
            Draw->SetCastShadow(false);
            Draw->bAffectDistanceFieldLighting = false;
            Draw->RegisterComponent();
            Tris += M.I.Num() / 3;
            Verts += M.V.Num();
            Vertical += M.Vertical;
            ++Meshes;
        }
        Edges = MoveTemp(Next);
        Inner = OL::Rings[R].Outer;
    }
    UE_LOG(LogTemp, Display, TEXT("CALDER_OUTERLAND rings=3 meshes=%d patches=%d tris=%d verts=%d verticalTris=%d seamPoints=%d seamMaxGap=%.4f extent=X%.0f..%.0f,Y%.0f..%.0f ms=%.0f"),
        Meshes, Patches, Tris, Verts, Vertical, SeamPoints, SeamGap, OL::Rings[2].Outer.X0, OL::Rings[2].Outer.X1, OL::Rings[2].Outer.Y0, OL::Rings[2].Outer.Y1,
        (FPlatformTime::Seconds() - Start) * 1000.0);
    HouseCalderRidgeForest::Build(Owner);
}

double HouseCalderOuterLand::LowestDrawn(double X, double Y)
{
    namespace OL = HouseCalderOuterLandImpl;
    const double H = HouseCalderTerrain::Half;
    if (FMath::Abs(X) <= H && FMath::Abs(Y) <= H) return double(HouseExteriorSite::GroundHeight(float(X), float(Y)));
    // The innermost ring whose rectangle holds (X, Y) (beyond ring 2 nothing is drawn), or its finer patch there.
    int32 Ring = 0;
    while (Ring < 3 && !(X >= OL::Rings[Ring].Outer.X0 && X <= OL::Rings[Ring].Outer.X1 && Y >= OL::Rings[Ring].Outer.Y0 && Y <= OL::Rings[Ring].Outer.Y1)) ++Ring;
    if (Ring == 3) return HouseCalderFarLand::Ground(X, Y);
    double OX = OL::Rings[Ring].Outer.X0, OY = OL::Rings[Ring].Outer.Y0, Step = OL::Rings[Ring].Step;
    // A cell beside the ring's inner edge (or a patch) is fanned to that edge's points: the 2 m lattice on the seam, the
    // inner ring's (or the patch's) lines further out.
    double Fine = Ring == 0 ? double(HouseCalderTerrain::Cell) : OL::Rings[Ring - 1].Step;
    for (const OL::FPatch& Patch : OL::RingPatches(Ring))
    {
        Fine = FMath::Min(Fine, Patch.Step);
        if (X >= Patch.R.X0 && X <= Patch.R.X1 && Y >= Patch.R.Y0 && Y <= Patch.R.Y1) { OX = Patch.R.X0; OY = Patch.R.Y0; Step = Patch.Step; }
    }
    const double X0 = OX + FMath::FloorToDouble((X - OX) / Step) * Step, Y0 = OY + FMath::FloorToDouble((Y - OY) / Step) * Step;
    TArray<double> Ys;
    for (double V = Y0; V <= Y0 + Step + .5; V += Fine) Ys.Add(V);
    for (const double Row : OL::RiverRows) if (Row > Y0 && Row < Y0 + Step) Ys.Add(Row);
    double Low = HouseCalderFarLand::Ground(X, Y);
    for (double U = X0; U <= X0 + Step + .5; U += Fine)
        for (const double V : Ys) Low = FMath::Min(Low, HouseCalderFarLand::Ground(U, V));
    return Low;
}

void HouseCalderOuterLand::SeatSheet(TArray<FVector>& V, int32 NX, int32 NY)
{
    constexpr double Under = 200.0;   // as deep as the outer land's skirts
    const double H = HouseCalderTerrain::Half;
    if (NX < 1 || NY < 1 || V.Num() != (NX + 1) * (NY + 1)) return;
    auto At = [&V, NY](int32 I, int32 J) -> FVector& { return V[I * (NY + 1) + J]; };
    int32 RimSunk = 0, FrontSunk = 0;
    TArray<double> Built;
    for (const FVector& P : V) Built.Add(P.Z);
    // The rim: no edge of the sheet hangs over the ground (the sheet's own skirt, as a heightfield's would be).
    for (int32 I = 0; I <= NX; ++I)
        for (int32 J = 0; J <= NY; ++J)
            if (I == 0 || I == NX || J == 0 || J == NY)
            {
                FVector& P = At(I, J);
                const double Seated = FMath::Min(P.Z, LowestDrawn(P.X, P.Y) - Under);
                RimSunk += Seated < P.Z ? 1 : 0;
                P.Z = Seated;
            }
    // Row 0 inside the walkable square: the triangles round vertex (I, 0) reach over columns I - 1..I + 1 and rise to row 1
    // at columns I and I + 1. Each stays under the lowest walkable ground in that strip (the lattice's corners and wall
    // pieces there: HouseCalderTerrain::ReliefMin over today's level -362 grade north of the house) up to the seam.
    for (int32 I = 0; I <= NX; ++I)
    {
        FVector& P = At(I, 0);
        if (P.Y >= H) continue;
        const double XA = FMath::Max(At(FMath::Max(I - 1, 0), 0).X, -H), XB = FMath::Min(At(FMath::Min(I + 1, NX), 0).X, H);
        if (XA > XB) continue;   // its triangles keep off the square
        double YA = P.Y;
        for (int32 K = FMath::Max(I - 1, 0); K <= FMath::Min(I + 1, NX); ++K) YA = FMath::Min(YA, At(K, 0).Y);
        YA = FMath::Max(YA, -H);
        const FBox2D Strip(FVector2D(XA, YA), FVector2D(XB, H));
        float Grade = HouseExteriorSite::BaseGrade(float(XA), float(YA));
        for (const double GX : { XA, XB }) for (const double GY : { YA, H }) Grade = FMath::Min(Grade, HouseExteriorSite::BaseGrade(float(GX), float(GY)));
        const double Cap = double(Grade) + double(HouseCalderTerrain::ReliefMin(Strip)) - Under;
        double Z = FMath::Min(P.Z, Cap);
        for (int32 K = I; K <= FMath::Min(I + 1, NX); ++K)
        {
            const FVector& Q = At(K, 1);
            if (Q.Y <= H) continue;
            const double T = (H - P.Y) / (Q.Y - P.Y);   // where the edge to (K, 1) crosses the seam
            Z = FMath::Min(Z, (Cap - Q.Z * T) / (1.0 - T));
        }
        FrontSunk += Z < P.Z ? 1 : 0;
        P.Z = Z;
    }
    double Deepest = 0.0;
    for (int32 K = 0; K < V.Num(); ++K) Deepest = FMath::Max(Deepest, Built[K] - V[K].Z);
    UE_LOG(LogTemp, Display, TEXT("CALDER_RIDGE_SEAT vertices=%d rimSunk=%d frontRowInSquareSunk=%d deepest=%.0f"), V.Num(), RimSunk, FrontSunk, Deepest);
}
