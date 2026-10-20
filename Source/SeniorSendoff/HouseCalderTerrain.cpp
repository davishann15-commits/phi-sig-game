// The one ground (see HouseCalderTerrain.h): the gate, the 2 m relief lattice and the datums every placer reads.
#include "HouseCalderTerrain.h"
#include "HouseCalderRoads.h"
#include "HouseCalderLandform.h"
#include "HouseCampusLayout.h"
#include "HouseExteriorSiteLayout.h"
#include "Async/ParallelFor.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"

namespace HouseCalderTerrainImpl
{
constexpr int32 Lines = HouseCalderTerrain::Cells + 1;

struct FLattice
{
    TArray<float> Z;          // Lines x Lines relief values, row-major by Y then X
    uint32 Hash = 0;
    float Min = 0.f, Max = 0.f;
    int32 NonZero = 0;
    bool bBuilt = false;
};

FLattice& Lattice()
{
    static FLattice L;
    static FCriticalSection Lock;
    if (!L.bBuilt)
    {
        FScopeLock Guard(&Lock);
        if (!L.bBuilt)
        {
            L.Z.SetNumZeroed(Lines * Lines);
            // The baked lattice (HouseCalderLandformData.inl), corner by corner, held at 0 in the frozen zones.
            const bool bOn = HouseCalderTerrain::Enabled();
            const float Scale = HouseCalderTerrain::ReliefScale();
            ParallelFor(Lines, [&](int32 J)
            {
                const float Y = -HouseCalderTerrain::Half + J * HouseCalderTerrain::Cell;
                for (int32 I = 0; I < Lines; ++I)
                {
                    const float X = -HouseCalderTerrain::Half + I * HouseCalderTerrain::Cell;
                    const float R = bOn && !HouseCalderTerrain::InFrozen(X, Y) ? HouseCalderLandform::LatticeRelief(I, J) * Scale : 0.f;
                    L.Z[J * Lines + I] = R == 0.f ? 0.f : R;
                }
            });
            for (const float V : L.Z) { L.Min = FMath::Min(L.Min, V); L.Max = FMath::Max(L.Max, V); L.NonZero += V != 0.f ? 1 : 0; }
            L.Hash = FCrc::MemCrc32(L.Z.GetData(), L.Z.Num() * sizeof(float));
            L.bBuilt = true;
        }
    }
    return L;
}

FORCEINLINE float At(const FLattice& L, int32 I, int32 J) { return L.Z[J * Lines + I]; }

/** The lattice cell range a box touches (clamped to the square); false when the box is wholly outside it. */
bool CellRange(const FBox2D& Box, int32& I0, int32& I1, int32& J0, int32& J1)
{
    const float H = HouseCalderTerrain::Half, C = HouseCalderTerrain::Cell;
    if (Box.Max.X < -H || Box.Min.X > H || Box.Max.Y < -H || Box.Min.Y > H) return false;
    I0 = FMath::Clamp(FMath::FloorToInt((Box.Min.X + H) / C), 0, HouseCalderTerrain::Cells - 1);
    I1 = FMath::Clamp(FMath::FloorToInt((Box.Max.X + H) / C), 0, HouseCalderTerrain::Cells - 1);
    J0 = FMath::Clamp(FMath::FloorToInt((Box.Min.Y + H) / C), 0, HouseCalderTerrain::Cells - 1);
    J1 = FMath::Clamp(FMath::FloorToInt((Box.Max.Y + H) / C), 0, HouseCalderTerrain::Cells - 1);
    return true;
}

/** The relief at a point of a wall-split cell: the piece that holds it, the vertex-0 fan triangle that holds it, by
 *  barycentric weights. A point on a wall line belongs to the first piece that holds it. */
float PieceRelief(const TArray<TArray<FVector>>& Pieces, float X, float Y)
{
    const FVector2D P(X, Y);
    float BestDist = FLT_MAX, BestZ = 0.f;
    for (const TArray<FVector>& Poly : Pieces)
        for (int32 K = 1; K + 1 < Poly.Num(); ++K)
        {
            const FVector2D A(Poly[0]), B(Poly[K]), C(Poly[K + 1]);
            const float Den = (B.Y - C.Y) * (A.X - C.X) + (C.X - B.X) * (A.Y - C.Y);
            if (FMath::Abs(Den) < 1e-6f) continue;
            const float WA = ((B.Y - C.Y) * (P.X - C.X) + (C.X - B.X) * (P.Y - C.Y)) / Den;
            const float WB = ((C.Y - A.Y) * (P.X - C.X) + (A.X - C.X) * (P.Y - C.Y)) / Den;
            const float WC = 1.f - WA - WB;
            const float Z = WA * float(Poly[0].Z) + WB * float(Poly[K].Z) + WC * float(Poly[K + 1].Z);
            const float Outside = FMath::Max3(-WA, -WB, -WC);
            if (Outside <= 1e-4f) return Z;
            if (Outside < BestDist) { BestDist = Outside; BestZ = Z; }
        }
    return BestZ;   // a hair outside every triangle (rounding at an edge): the nearest one
}

struct FReady { TWeakObjectPtr<const UWorld> World; bool bPending = false; };
FReady& Ready() { static FReady R; return R; }
}

bool HouseCalderTerrain::Enabled()
{
    static const bool bOn = HouseCampusLayout::Enabled() && !FParse::Param(FCommandLine::Get(), TEXT("NoCalderTerrain"));
    return bOn;
}

float HouseCalderTerrain::ReliefScale()
{
    static const float Scale = []
    {
        float S = 1.f;
        FParse::Value(FCommandLine::Get(), TEXT("CalderReliefScale="), S);
        return FMath::Clamp(S, 0.f, 1.f);
    }();
    return Scale;
}

bool HouseCalderTerrain::InFrozen(float X, float Y)
{
    const FVector2D P(X, Y);
    for (const FBox2D& R : HouseCalderLandform::FrozenRects()) if (P.X >= R.Min.X && P.X <= R.Max.X && P.Y >= R.Min.Y && P.Y <= R.Max.Y) return true;
    return false;
}

bool HouseCalderTerrain::BoxFrozen(const FBox2D& Box)
{
    constexpr float Tol = .5f;
    for (const FBox2D& R : HouseCalderLandform::FrozenRects())
        if (Box.Min.X >= R.Min.X - Tol && Box.Max.X <= R.Max.X + Tol && Box.Min.Y >= R.Min.Y - Tol && Box.Max.Y <= R.Max.Y + Tol) return true;
    return false;
}
const TArray<FBox2D>& HouseCalderTerrain::FrozenRects() { return HouseCalderLandform::FrozenRects(); }

float HouseCalderTerrain::Relief(float X, float Y)
{
    if (!Enabled() || InFrozen(X, Y)) return 0.f;
    if (X >= -Half && X <= Half && Y >= -Half && Y <= Half) return ReliefLattice(X, Y);
    // Beyond the square: the outer land as HouseCalderOuterLand draws it (HouseCalderFarLand::Ground, whose seam already
    // carries -CalderReliefScale through the lattice; scaling again would step the ground at the seam).
    const float R = float(HouseCalderLandform::Relief(X, Y));
    return R == 0.f ? 0.f : R;   // never -0.0f: the lattice hash and the dormancy tests compare exact zeros
}

float HouseCalderTerrain::ReliefLattice(float X, float Y)
{
    if (!Enabled()) return 0.f;
    if (X < -Half || X > Half || Y < -Half || Y > Half) return Relief(X, Y);
    const HouseCalderTerrainImpl::FLattice& L = HouseCalderTerrainImpl::Lattice();
    if (L.NonZero == 0) return 0.f;
    const float FX = (X + Half) / Cell, FY = (Y + Half) / Cell;
    const int32 I = FMath::Clamp(FMath::FloorToInt(FX), 0, Cells - 1), J = FMath::Clamp(FMath::FloorToInt(FY), 0, Cells - 1);
    if (const TArray<TArray<FVector>>* Pieces = HouseCalderLandform::WallPieces(I, J))
        return HouseCalderTerrainImpl::PieceRelief(*Pieces, X, Y) * ReliefScale();
    const float U = FX - I, V = FY - J;
    const float A = HouseCalderTerrainImpl::At(L, I, J), C = HouseCalderTerrainImpl::At(L, I + 1, J + 1);
    // Written as A + steps, not as a weighted sum, so a flat cell gives its corners' value exactly.
    if (U >= V)
    {
        const float B = HouseCalderTerrainImpl::At(L, I + 1, J);
        return A + (B - A) * U + (C - B) * V;
    }
    const float D = HouseCalderTerrainImpl::At(L, I, J + 1);
    return A + (C - D) * U + (D - A) * V;
}

float HouseCalderTerrain::ReliefMin(const FBox2D& Box)
{
    if (!Enabled()) return 0.f;
    const HouseCalderTerrainImpl::FLattice& L = HouseCalderTerrainImpl::Lattice();
    int32 I0, I1, J0, J1;
    if (!HouseCalderTerrainImpl::CellRange(Box, I0, I1, J0, J1)) return FMath::Min(FMath::Min(Relief(Box.Min.X, Box.Min.Y), Relief(Box.Max.X, Box.Max.Y)), FMath::Min(Relief(Box.Min.X, Box.Max.Y), Relief(Box.Max.X, Box.Min.Y)));
    if (L.NonZero == 0) return 0.f;
    float M = FLT_MAX;
    for (int32 J = J0; J <= J1 + 1; ++J) for (int32 I = I0; I <= I1 + 1; ++I) M = FMath::Min(M, HouseCalderTerrainImpl::At(L, I, J));
    for (int32 J = J0; J <= J1; ++J) for (int32 I = I0; I <= I1; ++I)
        if (const TArray<TArray<FVector>>* Pieces = HouseCalderLandform::WallPieces(I, J))
            for (const TArray<FVector>& Poly : *Pieces) for (const FVector& V : Poly) M = FMath::Min(M, float(V.Z) * ReliefScale());
    return M;
}

float HouseCalderTerrain::ReliefMax(const FBox2D& Box)
{
    if (!Enabled()) return 0.f;
    const HouseCalderTerrainImpl::FLattice& L = HouseCalderTerrainImpl::Lattice();
    int32 I0, I1, J0, J1;
    if (!HouseCalderTerrainImpl::CellRange(Box, I0, I1, J0, J1)) return FMath::Max(FMath::Max(Relief(Box.Min.X, Box.Min.Y), Relief(Box.Max.X, Box.Max.Y)), FMath::Max(Relief(Box.Min.X, Box.Max.Y), Relief(Box.Max.X, Box.Min.Y)));
    if (L.NonZero == 0) return 0.f;
    float M = -FLT_MAX;
    for (int32 J = J0; J <= J1 + 1; ++J) for (int32 I = I0; I <= I1 + 1; ++I) M = FMath::Max(M, HouseCalderTerrainImpl::At(L, I, J));
    for (int32 J = J0; J <= J1; ++J) for (int32 I = I0; I <= I1; ++I)
        if (const TArray<TArray<FVector>>* Pieces = HouseCalderLandform::WallPieces(I, J))
            for (const TArray<FVector>& Poly : *Pieces) for (const FVector& V : Poly) M = FMath::Max(M, float(V.Z) * ReliefScale());
    return M;
}

bool HouseCalderTerrain::ReliefFlat(const FBox2D& Box)
{
    if (!Enabled()) return true;
    if (HouseCalderTerrainImpl::Lattice().NonZero == 0) return true;
    return ReliefMin(Box) == 0.f && ReliefMax(Box) == 0.f;
}

uint32 HouseCalderTerrain::Hash() { return Enabled() ? HouseCalderTerrainImpl::Lattice().Hash : 0u; }

float HouseCalderTerrain::PaveOffset(EPave Kind)
{
    switch (Kind)
    {
    case EPave::Road: return -12.f;
    case EPave::Walk: return 3.f;
    case EPave::Apron: return 5.f;
    default: return 0.f;
    }
}

float HouseCalderTerrain::SurfaceZ(float X, float Y, EPave Kind)
{
    float Offset = PaveOffset(Kind);
    if (Kind == EPave::Road) HouseCalderRoads::PaveOffsetAt(FVector2D(X, Y), Offset);
    return HouseExteriorSite::GroundHeight(X, Y) + Offset;
}
float HouseCalderTerrain::SurfaceZ(float X, float Y) { return SurfaceZ(X, Y, HouseCampusLayout::PaveKindAt(FVector2D(X, Y))); }

float HouseCalderTerrain::PadDZ(int32 Seed) { return Enabled() ? float(HouseCalderLandform::PadDZ(Seed)) * ReliefScale() : 0.f; }
float HouseCalderTerrain::TowerLift() { return Enabled() ? float(HouseCalderLandform::TowerLift()) * ReliefScale() : 0.f; }
float HouseCalderTerrain::ValleyZ(float X, float Y) { return Enabled() ? float(HouseCalderLandform::Valley(X, Y)) : -60.f; }
float HouseCalderTerrain::RidgeBase(float X, float Y) { return Enabled() ? float(HouseCalderLandform::RidgeBase(X, Y)) : -362.f; }

bool HouseCalderTerrain::CollisionReady(const UWorld* World)
{
    const HouseCalderTerrainImpl::FReady& R = HouseCalderTerrainImpl::Ready();
    return !Enabled() || !R.bPending || R.World.Get() != World;
}

void HouseCalderTerrain::SetCollisionReady(const UWorld* World, bool bIsReady)
{
    HouseCalderTerrainImpl::FReady& R = HouseCalderTerrainImpl::Ready();
    R.World = World;
    R.bPending = !bIsReady;
}

void HouseCalderTerrain::LogSummary()
{
    if (!Enabled()) { UE_LOG(LogTemp, Display, TEXT("CALDER_TERRAIN off (-NoCalderTerrain or -NoCampusLayout)")); return; }
    const HouseCalderTerrainImpl::FLattice& L = HouseCalderTerrainImpl::Lattice();
    UE_LOG(LogTemp, Display, TEXT("CALDER_TERRAIN hash=%08x lattice=%dx%d cell=%.0f nonzero=%d relief=%.1f..%.1f tower=%.1f scale=%.2f"),
        L.Hash, HouseCalderTerrainImpl::Lines, HouseCalderTerrainImpl::Lines, Cell, L.NonZero, L.Min, L.Max, TowerLift(), ReliefScale());
}
