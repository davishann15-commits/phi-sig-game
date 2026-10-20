// Draping on the Calder terrain (see HouseCalderDrape.h).
#include "HouseCalderDrape.h"
#include "HouseBlackHallMesh.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderLandform.h"
#include "HouseCalderTerrain.h"

namespace HouseCalderDrapeImpl
{
using FPoly = TArray<FVector2D>;

/** A draped piece: convex, over one triangle of the ground. bWall: it lies in a cell a declared wall splits, where the
 *  ground steps at the wall line, so its vertices are read a hair (1 mm) in from its edges toward its centre: a vertex on a
 *  wall line then takes this piece's own side of the step, not the first piece that holds the point. */
struct FPiece { FPoly P; bool bWall = false; };

constexpr double Nudge = .1;   // cm

/** Keeps the part of a convex polygon where A*X + B*Y + C >= 0. */
void Clip(const FPoly& In, FPoly& Out, double A, double B, double C)
{
    Out.Reset();
    const int32 Num = In.Num();
    for (int32 K = 0; K < Num; ++K)
    {
        const FVector2D& P = In[K];
        const FVector2D& Q = In[(K + 1) % Num];
        const double FP = A * P.X + B * P.Y + C, FQ = A * Q.X + B * Q.Y + C;
        if (FP >= 0.0) Out.Add(P);
        if ((FP >= 0.0) != (FQ >= 0.0)) Out.Add(P + (Q - P) * (FP / (FP - FQ)));
    }
}

double SignedArea(const FPoly& P)
{
    double A = 0.0;
    for (int32 K = 0; K < P.Num(); ++K) A += P[K].X * P[(K + 1) % P.Num()].Y - P[(K + 1) % P.Num()].X * P[K].Y;
    return A * .5;
}

/** The half-plane left of P->Q (for an anticlockwise polygon, its inside) as A*X + B*Y + C >= 0, times Sign. */
void LeftOf(const FVector2D& P, const FVector2D& Q, double Sign, double& A, double& B, double& C)
{
    A = -(Q.Y - P.Y) * Sign;
    B = (Q.X - P.X) * Sign;
    C = ((Q.Y - P.Y) * P.X - (Q.X - P.X) * P.Y) * Sign;
}

/** The part of convex In inside the convex polygon Clipper (either winding). */
void ClipConvex(const FPoly& In, const FPoly& Clipper, FPoly& Out)
{
    const double Sign = SignedArea(Clipper) >= 0.0 ? 1.0 : -1.0;
    FPoly Cur = In, Next;
    for (int32 K = 0; K < Clipper.Num() && Cur.Num() >= 3; ++K)
    {
        double A, B, C;
        LeftOf(Clipper[K], Clipper[(K + 1) % Clipper.Num()], Sign, A, B, C);
        Clip(Cur, Next, A, B, C);
        Swap(Cur, Next);
    }
    Out = MoveTemp(Cur);
}

/** The parts of convex P outside convex Hole (each convex; none when P lies inside the hole). */
void Subtract(const FPoly& P, const FPoly& Hole, TArray<FPoly>& Out)
{
    const double Sign = SignedArea(Hole) >= 0.0 ? 1.0 : -1.0;
    FPoly Rest = P, Part, In;
    for (int32 K = 0; K < Hole.Num(); ++K)
    {
        double A, B, C;
        LeftOf(Hole[K], Hole[(K + 1) % Hole.Num()], Sign, A, B, C);
        Clip(Rest, Part, -A, -B, -C);   // outside this edge
        if (Part.Num() >= 3 && FMath::Abs(SignedArea(Part)) > .5) Out.Add(Part);
        Clip(Rest, In, A, B, C);        // inside it: on to the next edge
        if (In.Num() < 3 || FMath::Abs(SignedArea(In)) <= .5) return;
        Swap(Rest, In);
    }
    // What is left lies inside the hole.
}

FBox2D Bounds(const FPoly& P)
{
    FBox2D B(ForceInit);
    for (const FVector2D& V : P) B += V;
    return B;
}

/** The stair and ramp footprints (HouseCalderFeatures::Footprints) that meet Box. */
void HolesNear(const FBox2D& Box, TArray<const FPoly*>& Out)
{
    for (const FPoly& H : HouseCalderFeatures::Footprints()) if (Bounds(H).Intersect(Box)) Out.Add(&H);
}

/** The outline cut along the lattice lines and diagonals, and in a wall-split cell along its pieces' fan triangles (the
 *  ground's own triangles there), less the stair and ramp footprints. */
void PiecesEx(const FPoly& Outline, TArray<FPiece>& Out)
{
    if (Outline.Num() < 3) return;
    const FBox2D Box = Bounds(Outline);
    const double H = HouseCalderTerrain::Half, C = HouseCalderTerrain::Cell;
    const int32 I0 = FMath::FloorToInt((Box.Min.X + H) / C), I1 = FMath::FloorToInt((Box.Max.X + H) / C);
    const int32 J0 = FMath::FloorToInt((Box.Min.Y + H) / C), J1 = FMath::FloorToInt((Box.Max.Y + H) / C);
    TArray<FPiece> Raw;
    FPoly A, B, Cell, Side, Tri;
    for (int32 J = J0; J <= J1; ++J)
        for (int32 I = I0; I <= I1; ++I)
        {
            const double X0 = -H + I * C, Y0 = -H + J * C;
            // The outline inside this cell: X >= X0, X <= X0 + C, Y >= Y0, Y <= Y0 + C.
            Clip(Outline, A, 1, 0, -X0);
            Clip(A, B, -1, 0, X0 + C);
            Clip(B, A, 0, 1, -Y0);
            Clip(A, Cell, 0, -1, Y0 + C);
            if (Cell.Num() < 3) continue;
            const TArray<TArray<FVector>>* Walled = (I >= 0 && J >= 0 && I < HouseCalderTerrain::Cells && J < HouseCalderTerrain::Cells)
                ? HouseCalderLandform::WallPieces(I, J) : nullptr;
            if (Walled && HouseCalderTerrain::Enabled())
            {
                // Each piece's vertex-0 fan, as HouseCalderTerrain reads the ground there.
                for (const TArray<FVector>& Poly : *Walled)
                    for (int32 K = 1; K + 1 < Poly.Num(); ++K)
                    {
                        const FPoly T = { FVector2D(Poly[0]), FVector2D(Poly[K]), FVector2D(Poly[K + 1]) };
                        if (FMath::Abs(SignedArea(T)) < 1e-3) continue;
                        ClipConvex(Cell, T, Tri);
                        if (Tri.Num() >= 3 && FMath::Abs(SignedArea(Tri)) > .5) Raw.Add({ Tri, true });
                    }
                continue;
            }
            // Each side of the cell's (I,J)-(I+1,J+1) diagonal: (X - X0) - (Y - Y0) >= 0 and <= 0.
            for (const double S : { 1.0, -1.0 })
            {
                Clip(Cell, Side, S, -S, -S * X0 + S * Y0);
                if (Side.Num() >= 3 && FMath::Abs(SignedArea(Side)) > .5) Raw.Add({ Side, false });
            }
        }
    TArray<const FPoly*> Holes;
    HolesNear(Box, Holes);
    if (!Holes.Num()) { Out.Append(MoveTemp(Raw)); return; }
    for (FPiece& Piece : Raw)
    {
        TArray<FPoly> Keep = { Piece.P }, Next;
        const FBox2D PB = Bounds(Piece.P);
        for (const FPoly* Hole : Holes)
        {
            if (!Bounds(*Hole).Intersect(PB)) continue;
            Next.Reset();
            for (const FPoly& K : Keep) Subtract(K, *Hole, Next);
            Swap(Keep, Next);
            if (!Keep.Num()) break;
        }
        for (FPoly& K : Keep) Out.Add({ MoveTemp(K), Piece.bWall });
    }
}

/** Where a piece's vertex is read: the vertex itself, or in a wall-split cell a hair in toward the piece's centre. */
FVector2D ReadAt(const FPiece& Piece, const FVector2D& V, const FVector2D& Centre)
{
    if (!Piece.bWall) return V;
    const FVector2D In = Centre - V;
    const double L = In.Size();
    return L > Nudge * 2.0 ? V + In * (Nudge / L) : V;
}

/** One triangle facing up (FHallMesh's front-face rule: the cross product points away from the face normal). */
void Tri3(FHallMesh& M, FName Key, const FVector& A, const FVector& B, const FVector& C, const FHallUV& UV)
{
    FVector N = FVector::CrossProduct(B - A, C - A);
    if (N.SizeSquared() < 1e-6) return;
    const bool bDown = N.Z < 0.0;   // (A, B, C) already faces up when its cross product points down
    N = (bDown ? -N : N).GetSafeNormal();
    FHallSection& S = M.Sec(Key);
    const int32 Base = S.V.Num();
    FVector UAx, VAx;
    FHallMesh::PlanarAxes(N, UAx, VAx);
    for (const FVector& P : { A, B, C }) { S.V.Add(P); S.N.Add(N); S.UV.Add(M.ComputeUV(P, N, UV)); S.T.Add(FProcMeshTangent(UAx, false)); }
    if (bDown) S.I.Append({ Base, Base + 1, Base + 2 });
    else S.I.Append({ Base, Base + 2, Base + 1 });
}

/** The point where segment P->Q crosses segment A->B, as a fraction along P->Q (strictly inside both); false when none. */
bool Crossing(const FVector2D& P, const FVector2D& Q, const FVector2D& A, const FVector2D& B, double& OutT)
{
    const FVector2D R = Q - P, S = B - A;
    const double Den = R.X * S.Y - R.Y * S.X;
    if (FMath::Abs(Den) < 1e-9) return false;
    const FVector2D AP = A - P;
    const double T = (AP.X * S.Y - AP.Y * S.X) / Den, U = (AP.X * R.Y - AP.Y * R.X) / Den;
    if (T <= 1e-6 || T >= 1.0 - 1e-6 || U < 0.0 || U > 1.0) return false;
    OutT = T;
    return true;
}

bool InAnyHole(const FVector2D& P, const TArray<const FPoly*>& Holes)
{
    for (const FPoly* H : Holes)
    {
        const double Sign = SignedArea(*H) >= 0.0 ? 1.0 : -1.0;
        bool bIn = true;
        for (int32 K = 0; K < H->Num() && bIn; ++K)
        {
            double A, B, C;
            LeftOf((*H)[K], (*H)[(K + 1) % H->Num()], Sign, A, B, C);
            bIn = A * P.X + B * P.Y + C > 1e-6;
        }
        if (bIn) return true;
    }
    return false;
}
}

void HouseCalderDrape::Pieces(const TArray<FVector2D>& Outline, TArray<TArray<FVector2D>>& Out)
{
    TArray<HouseCalderDrapeImpl::FPiece> Parts;
    HouseCalderDrapeImpl::PiecesEx(Outline, Parts);
    for (HouseCalderDrapeImpl::FPiece& P : Parts) Out.Add(MoveTemp(P.P));
}

TArray<FVector2D> HouseCalderDrape::Densify(const TArray<FVector2D>& Outline)
{
    TArray<FVector2D> Out;
    const double H = HouseCalderTerrain::Half, C = HouseCalderTerrain::Cell;
    for (int32 K = 0; K < Outline.Num(); ++K)
    {
        const FVector2D P = Outline[K], Q = Outline[(K + 1) % Outline.Num()];
        const FVector2D D = Q - P;
        TArray<double> Ts;
        // Crossings of X = -H + i*C, Y = -H + j*C and the diagonals Y - X = k*C (every cell's diagonal lies on one).
        auto Lines = [&Ts](double A0, double A1, double Origin, double Step)
        {
            if (FMath::Abs(A1 - A0) < 1e-9) return;
            const double Lo = FMath::Min(A0, A1), Hi = FMath::Max(A0, A1);
            for (double V = Origin + FMath::CeilToDouble((Lo - Origin) / Step) * Step; V < Hi; V += Step)
            {
                const double T = (V - A0) / (A1 - A0);
                if (T > 1e-6 && T < 1.0 - 1e-6) Ts.Add(T);
            }
        };
        Lines(P.X, Q.X, -H, C);
        Lines(P.Y, Q.Y, -H, C);
        Lines(P.Y - P.X, Q.Y - Q.X, 0.0, C);
        Ts.Sort();
        Out.Add(P);
        for (const double T : Ts) Out.Add(P + D * T);
    }
    return Out;
}

void HouseCalderDrape::Sheet(FHallMesh& M, FName Key, const TArray<FVector2D>& Outline, TFunctionRef<float(float, float)> Z, const FHallUV& UV)
{
    TArray<HouseCalderDrapeImpl::FPiece> Parts;
    HouseCalderDrapeImpl::PiecesEx(Outline, Parts);
    TArray<FVector> V;
    for (const HouseCalderDrapeImpl::FPiece& Piece : Parts)
    {
        const TArray<FVector2D>& P = Piece.P;
        FVector2D Centre(0.0);
        for (const FVector2D& Q : P) Centre += Q;
        Centre /= double(P.Num());
        V.Reset();
        for (const FVector2D& Q : P) { const FVector2D R = HouseCalderDrapeImpl::ReadAt(Piece, Q, Centre); V.Add(FVector(Q, Z(R.X, R.Y))); }
        for (int32 K = 1; K + 1 < V.Num(); ++K) HouseCalderDrapeImpl::Tri3(M, Key, V[0], V[K], V[K + 1], UV);
    }
}

void HouseCalderDrape::Slab(FHallMesh& M, FName Key, const TArray<FVector2D>& Outline, TFunctionRef<float(float, float)> TopZ, TFunctionRef<float(float, float)> BottomZ, const FHallUV& UV, bool bSides)
{
    Sheet(M, Key, Outline, TopZ, UV);
    if (!bSides) return;
    TArray<const HouseCalderDrapeImpl::FPoly*> Holes;
    HouseCalderDrapeImpl::HolesNear(HouseCalderDrapeImpl::Bounds(Outline).ExpandBy(1.0), Holes);
    const TArray<HouseCalderLandform::FWall>* Walls = HouseCalderTerrain::Enabled() ? &HouseCalderLandform::Walls() : nullptr;
    const TArray<FVector2D> Ring = Densify(Outline);
    const bool bCCW = HouseCalderDrapeImpl::SignedArea(Outline) > 0.0;
    const double H = HouseCalderTerrain::Half, C = HouseCalderTerrain::Cell;
    TArray<double> Ts;
    for (int32 K = 0; K < Ring.Num(); ++K)
    {
        const FVector2D P = Ring[K], Q = Ring[(K + 1) % Ring.Num()];
        const FVector2D D = (Q - P).GetSafeNormal();
        if (D.IsNearlyZero()) continue;
        const FVector Out = bCCW ? FVector(D.Y, -D.X, 0.f) : FVector(-D.Y, D.X, 0.f);   // away from the inside
        // Split where a declared wall crosses this edge (the ground steps there), and read each part's ends a hair in from
        // them inside a wall-split cell, so each side of a step takes its own height.
        Ts.Reset();
        Ts.Add(0.0);
        const FVector2D Mid = (P + Q) * .5;
        const int32 MI = FMath::FloorToInt((Mid.X + H) / C), MJ = FMath::FloorToInt((Mid.Y + H) / C);
        const bool bWallCell = Walls && MI >= 0 && MJ >= 0 && MI < HouseCalderTerrain::Cells && MJ < HouseCalderTerrain::Cells
            && HouseCalderLandform::WallPieces(MI, MJ) != nullptr;
        if (bWallCell)
            for (const HouseCalderLandform::FWall& W : *Walls)
            {
                double T = 0.0;
                if (HouseCalderDrapeImpl::Crossing(P, Q, W.A, W.B, T)) Ts.Add(T);
            }
        Ts.Add(1.0);
        Ts.Sort();
        for (int32 S = 0; S + 1 < Ts.Num(); ++S)
        {
            const FVector2D A = P + (Q - P) * Ts[S], B = P + (Q - P) * Ts[S + 1];
            const double Len = (B - A).Size();
            if (Len < .05) continue;
            if (Holes.Num() && HouseCalderDrapeImpl::InAnyHole((A + B) * .5, Holes)) continue;   // a stair or ramp stands there
            const double In = bWallCell ? FMath::Min(HouseCalderDrapeImpl::Nudge, Len * .25) : 0.0;
            const FVector2D RA = A + D * In, RB = B - D * In;
            const FVector AT(A, TopZ(RA.X, RA.Y)), BT(B, TopZ(RB.X, RB.Y)), AB(A, BottomZ(RA.X, RA.Y)), BB(B, BottomZ(RB.X, RB.Y));
            if (AT.Z - AB.Z < .05f && BT.Z - BB.Z < .05f) continue;
            M.Quad(Key, AT, BT, BB, AB, Out, UV);
        }
    }
}

TArray<FVector2D> HouseCalderDrape::RectOutline(const FBox2D& R)
{
    return { R.Min, FVector2D(R.Max.X, R.Min.Y), R.Max, FVector2D(R.Min.X, R.Max.Y) };
}

TArray<FVector2D> HouseCalderDrape::RunOutline(const FVector2D& A, const FVector2D& B, float Width)
{
    const FVector2D D = (B - A).GetSafeNormal();
    const FVector2D S(-D.Y * Width * .5f, D.X * Width * .5f);
    return { A - S, B - S, B + S, A + S };
}

TArray<FVector2D> HouseCalderDrape::SectorPiece(const FVector2D& Centre, float R0, float R1, float Deg0, float Deg1)
{
    auto At = [&Centre](float Deg, float R) { const float T = FMath::DegreesToRadians(Deg); return Centre + FVector2D(FMath::Cos(T), FMath::Sin(T)) * R; };
    return { At(Deg0, R0), At(Deg0, R1), At(Deg1, R1), At(Deg1, R0) };
}
