// Calder Branch, Gander Pond and Calder Falls: the water and the structures that cross and hold it (see HouseCalderWater.h).
#include "HouseCalderWater.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderFootPoints.h"
#include "HouseCalderLandform.h"
#include "HouseCalderLandscape.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorSiteLayout.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalFogVolume.h"
#include "Engine/Level.h"
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

namespace HouseCalderWaterImpl
{
using HouseCalderLandform::EWallKind;

// ------------------------------------------------------------------ numbers
constexpr double Gravity = 981.0;           // cm/s^2: the curtains fall on true parabolas
constexpr float Tuck = 3.f;                 // a water sheet's edge sinks this far under its bank
constexpr float WalkCos = .76604444f;       // cos(40 deg): steeper collision faces are Unwalkable
constexpr float FaceFoot = 25.f;            // stone faces reach this far under the ground in front of them
constexpr float FootSink = 8.f;             // posts, piles, footings and stones are set this far into the ground (their feet)
constexpr float DeckLift = 3.6f;            // a stone deck rises over ground higher than its level by this (a walk's 3 cm, 6 mm)
constexpr double ParapetIn = 85.0;          // the dam's parapets: their inner faces this far out from its line (170 cm clear)
constexpr float ParapetRise = 100.f;        // 1986: 90 cm at least at a drop of 75 cm or more
constexpr float CopingThick = 10.f, CopingOver = 4.f;
constexpr double ParapetEndClear = 60.0;    // the parapets start this far along the crest from its ends (the loop arrives there)
constexpr float RailRise = 100.f;
constexpr float KerbRise = 20.f;
constexpr double KerbWidth = 25.0;
constexpr double HeadwallHalf = 200.0;      // the causeway's headwalls: this far along it either side of the pipe
constexpr double GlenStep = 50.0, PondStep = 100.0, CreekStep = 100.0;
constexpr float MouthFilm = 2.5f;           // Calder Branch's mouth: a riffle this deep over its gravel ...
constexpr double MouthReach = 900.0;        // ... within this of the pond's edge
constexpr float WaterCull = 150000.f, WorksCull = 60000.f;
constexpr bool bWorksCastShadow = true;     // the overhang must shade the rock house; water never casts
constexpr int32 AllFaces = 63, SidesOnly = 60, SidesAndTop = 61;

enum EWater : uint8 { WPond, WWeir, WCreek, WGlen0, WPool, WGlen2, WFalls, WCascade, WCount };
const TCHAR* const WaterSlots[WCount] = { TEXT("Pond"), TEXT("Weir"), TEXT("Creek"), TEXT("GlenUpper"), TEXT("PlungePool"), TEXT("GlenLower"), TEXT("Falls"), TEXT("Cascade") };
enum EWork : uint8 { KStone, KDressed, KCoping, KDeck, KGravel, KSandstone, KBoulder, KPlank, KTimber, KRust, KDark, KCount };
const TCHAR* const WorkSlots[KCount] = { TEXT("Stone"), TEXT("Dressed"), TEXT("Coping"), TEXT("Deck"), TEXT("Gravel"), TEXT("Sandstone"), TEXT("Boulder"), TEXT("Plank"), TEXT("Timber"), TEXT("Rust"), TEXT("Dark") };

/** Which collision a face joins: none, the walkable mesh (decks, sills, copings, stone tops) or the Unwalkable one (faces,
 *  rails, rock). A Walk face steeper than 40 degrees joins the Unwalkable mesh. */
enum class EColl : uint8 { None, Walk, Block };

// ------------------------------------------------------------------ meshes
struct FDrawPart { TArray<FVector> V, N; TArray<FLinearColor> C; TArray<int32> I; };
struct FCollPart { TArray<FVector> V; TArray<int32> I; };
struct FMeshSet
{
    TArray<FDrawPart> Parts;
    FCollPart Walk, Block;
    explicit FMeshSet(int32 Slots) { Parts.SetNum(Slots); }
    int32 Tris() const { int32 T = 0; for (const FDrawPart& D : Parts) T += D.I.Num() / 3; return T; }
};

struct FStats
{
    int32 Surfaces = 0, WaterTris = 0, Bridges = 0, Stones = 0, Rails = 0, Parapets = 0, Kerbs = 0, Feet = 0, Riprap = 0;
    int32 Bents = 0, Planks = 0, Bars = 0, Cells = 0, Wet = 0, Tucked = 0, RoadBridges = 0;
    double RailCm = 0.0, ParapetCm = 0.0, EdgeCm = 0.0;
    float MaxDepth = 0.f, WorstFoot = 0.f;
    TArray<FString> Notes;
};

// ------------------------------------------------------------------ ground and plan helpers, noise
float GroundAt(const FVector2D& P) { return HouseExteriorSite::GroundHeight(float(P.X), float(P.Y)); }
FVector Lift(const FVector2D& P, double Z) { return FVector(P.X, P.Y, Z); }
double Cross2(const FVector2D& U, const FVector2D& V) { return U.X * V.Y - U.Y * V.X; }
FVector2D Left(const FVector2D& D) { return FVector2D(-D.Y, D.X); }
FLinearColor Ao(float A) { return FLinearColor(1.f, 1.f, 1.f, A); }
double Smooth01(double T) { const double U = FMath::Clamp(T, 0.0, 1.0); return U * U * (3.0 - 2.0 * U); }
/** 0 at V = A, 1 at V = B (A may be greater than B), smoothstep between. */
double RampD(double V, double A, double B) { return Smooth01((V - A) / (B - A)); }
float DepthA(float Depth) { return FMath::Pow(FMath::Clamp(Depth / 100.f, 0.f, 1.f), .75f); }

double SegDist(const FVector2D& P, const FVector2D& A, const FVector2D& B)
{
    const FVector2D AB = B - A;
    const double L2 = AB.SizeSquared();
    const double T = L2 > 1e-9 ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.0, 1.0) : 0.0;
    return (P - (A + AB * T)).Size();
}

bool InOutline(const FVector2D& P, const TArray<FVector2D>& Outline)
{
    bool bIn = false;
    for (int32 I = 0, J = Outline.Num() - 1; I < Outline.Num(); J = I++)
    {
        const FVector2D& A = Outline[I];
        const FVector2D& B = Outline[J];
        if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X) bIn = !bIn;
    }
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
float Hash01(int32 A, int32 B, uint32 Seed) { return float(Mix(A, B, 0, Seed) & 0xFFFFFFu) / float(0xFFFFFF); }

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

/** Points from From to To, both included, the ones between on multiples of Step (either direction). */
TArray<double> Steps(double From, double To, double Step)
{
    TArray<double> Out;
    Out.Add(From);
    const double Sg = To >= From ? 1.0 : -1.0;
    double V = Sg > 0.0 ? FMath::FloorToDouble(From / Step) * Step + Step : FMath::CeilToDouble(From / Step) * Step - Step;
    for (; Sg > 0.0 ? V < To - .5 : V > To + .5; V += Sg * Step)
        if (FMath::Abs(V - From) > .5) Out.Add(V);
    if (FMath::Abs(To - Out.Last()) > .5) Out.Add(To);
    return Out;
}

// ------------------------------------------------------------------ primitives
/** A convex polygon facing Out, fanned from its first point (a triangle (A, B, C) faces the side its cross product points
 *  away from, as every Calder mesh). Normals: per point, or null for the polygon's own. Slot < 0: collision only. */
void Poly(FMeshSet& M, int32 Slot, EColl Coll, const FVector* P, int32 Num, const FVector& Out, const FVector* Normals = nullptr,
    const FLinearColor* Colors = nullptr, const FLinearColor& ColorAll = FLinearColor::White)
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
    auto Fan = [P, Num, &Face](TArray<int32>& Idx, int32 Base0)
    {
        for (int32 K = 1; K + 1 < Num; ++K)
        {
            const FVector X = FVector::CrossProduct(P[K] - P[0], P[K + 1] - P[0]);
            if (X.SizeSquared() < 1e-6) continue;
            if (FVector::DotProduct(X, Face) < 0.0) Idx.Append({ Base0, Base0 + K, Base0 + K + 1 });
            else Idx.Append({ Base0, Base0 + K + 1, Base0 + K });
        }
    };
    if (Slot >= 0)
    {
        FDrawPart& D = M.Parts[Slot];
        const int32 Base0 = D.V.Num();
        for (int32 K = 0; K < Num; ++K)
        {
            D.V.Add(P[K]);
            D.N.Add(Normals ? Normals[K] : Face);
            D.C.Add(Colors ? Colors[K] : ColorAll);
        }
        Fan(D.I, Base0);
    }
    if (Coll != EColl::None)
    {
        FCollPart& Part = (Coll == EColl::Walk && Face.Z >= WalkCos) ? M.Walk : M.Block;
        const int32 Base0 = Part.V.Num();
        for (int32 K = 0; K < Num; ++K) Part.V.Add(P[K]);
        Fan(Part.I, Base0);
    }
}

/** A hexahedron: Low[4] round one end, High[4] the other. Faces by Mask: 1 the High end, 2 the Low end, 4 << K side K
 *  (Low[K] to Low[K + 1]). Each face looks away from the solid's centre. */
void Hexa(FMeshSet& M, int32 Slot, EColl Coll, const FVector (&Low)[4], const FVector (&High)[4], int32 Mask, float AoLow = 1.f, float AoHigh = 1.f)
{
    FVector Centre(0.0);
    for (int32 K = 0; K < 4; ++K) Centre += Low[K] + High[K];
    Centre /= 8.0;
    auto Emit = [&](const FVector (&Q)[4], const float (&A)[4])
    {
        const FLinearColor Cols[4] = { Ao(A[0]), Ao(A[1]), Ao(A[2]), Ao(A[3]) };
        Poly(M, Slot, Coll, Q, 4, (Q[0] + Q[1] + Q[2] + Q[3]) * .25 - Centre, nullptr, Cols);
    };
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

/** A plan frame: origin, unit direction U and its left normal S. */
struct FFrame
{
    FVector2D O = FVector2D::ZeroVector, U = FVector2D(1.0, 0.0), S = FVector2D(0.0, 1.0);
    FFrame() = default;
    FFrame(const FVector2D& InO, const FVector2D& InU) : O(InO), U(InU.GetSafeNormal()), S(Left(InU.GetSafeNormal())) {}
    FVector2D At(double A, double L) const { return O + U * A + S * L; }
};

/** A box in a plan frame: along [A0, A1], across [L0, L1], from Z0 to Z1. */
void FrameBox(FMeshSet& M, int32 Slot, EColl Coll, const FFrame& F, double A0, double A1, double L0, double L1, double Z0, double Z1,
    int32 Mask = AllFaces, float AoLow = 1.f, float AoHigh = 1.f)
{
    const FVector2D C[4] = { F.At(A0, L0), F.At(A1, L0), F.At(A1, L1), F.At(A0, L1) };
    const FVector Low[4] = { Lift(C[0], Z0), Lift(C[1], Z0), Lift(C[2], Z0), Lift(C[3], Z0) };
    const FVector High[4] = { Lift(C[0], Z1), Lift(C[1], Z1), Lift(C[2], Z1), Lift(C[3], Z1) };
    Hexa(M, Slot, Coll, Low, High, Mask, AoLow, AoHigh);
}

/** A squared timber or bar from A to B: Width across (horizontal), Height in the plane of A, B and the vertical. A vertical
 *  member takes SideHint as its across direction. */
void Beam(FMeshSet& M, int32 Slot, EColl Coll, const FVector& A, const FVector& B, double Width, double Height, const FVector2D& SideHint, float AoAll = 1.f)
{
    const FVector Axis = (B - A).GetSafeNormal();
    if (Axis.IsNearlyZero()) return;
    FVector Side = FVector::CrossProduct(FVector::UpVector, Axis);
    if (Side.SizeSquared() < 1e-4) Side = FVector(SideHint.X, SideHint.Y, 0.0);
    Side = Side.GetSafeNormal();
    const FVector Up = FVector::CrossProduct(Axis, Side).GetSafeNormal();
    const FVector Sw = Side * (Width * .5), Uh = Up * (Height * .5);
    const FVector Low[4] = { A - Sw - Uh, A + Sw - Uh, A + Sw + Uh, A - Sw + Uh };
    const FVector High[4] = { B - Sw - Uh, B + Sw - Uh, B + Sw + Uh, B - Sw + Uh };
    Hexa(M, Slot, Coll, Low, High, AllFaces, AoAll, AoAll);
}

/** A round member (Sides sides) from A to B; bInside: its faces look inward (a pipe's bore). */
void Pipe(FMeshSet& M, int32 Slot, EColl Coll, const FVector& A, const FVector& B, float Radius, int32 Sides, bool bCapA, bool bCapB,
    float AoAll = 1.f, bool bInside = false)
{
    const FVector Axis = (B - A).GetSafeNormal();
    if (Axis.IsNearlyZero() || Sides < 3) return;
    const FVector Ref = FMath::Abs(Axis.Z) < .9 ? FVector::UpVector : FVector::ForwardVector;
    const FVector U = FVector::CrossProduct(Axis, Ref).GetSafeNormal(), W = FVector::CrossProduct(Axis, U);
    TArray<FVector, TInlineAllocator<16>> Ring;
    for (int32 K = 0; K < Sides; ++K)
    {
        const double Ang = 2.0 * UE_DOUBLE_PI * K / Sides;
        Ring.Add(U * FMath::Cos(Ang) + W * FMath::Sin(Ang));
    }
    const double Sg = bInside ? -1.0 : 1.0;
    for (int32 K = 0; K < Sides; ++K)
    {
        const int32 L = (K + 1) % Sides;
        const FVector Q[4] = { A + Ring[K] * Radius, A + Ring[L] * Radius, B + Ring[L] * Radius, B + Ring[K] * Radius };
        const FVector Nrm[4] = { Ring[K] * Sg, Ring[L] * Sg, Ring[L] * Sg, Ring[K] * Sg };
        Poly(M, Slot, Coll, Q, 4, (Ring[K] + Ring[L]) * Sg, Nrm, nullptr, Ao(AoAll));
    }
    for (int32 End = 0; End < 2; ++End)
    {
        if (!(End == 0 ? bCapA : bCapB)) continue;
        const FVector& At = End == 0 ? A : B;
        TArray<FVector, TInlineAllocator<16>> Q;
        for (int32 K = 0; K < Sides; ++K) Q.Add(At + Ring[K] * Radius);
        Poly(M, Slot, Coll, Q.GetData(), Sides, End == 0 ? -Axis : Axis, nullptr, nullptr, Ao(AoAll));
    }
}

/** A foot point: registered with the Calder foot-point registry (-CalderTerrainReview holds it within -10..+3 cm of the
 *  surface) and counted. */
void Foot(const FVector2D& P, float FootZ, FStats& St, float PlacerSurface=MAX_flt)
{
    const float Surface = PlacerSurface==MAX_flt?GroundAt(P):PlacerSurface;
    HouseCalderFootPoints::Register(TEXT("CalderWater"), Lift(P, FootZ), Surface);
    ++St.Feet;
    St.WorstFoot = FMath::Max(St.WorstFoot, FMath::Abs(FootZ - Surface));
}

/** A rough boulder or stepping stone: Sides sides, its base following the ground FootSink under it (its foot registered), its
 *  top at TopZ (flat enough to stand on), bulging a little at a third of its height. Collision: its top walkable, its sides
 *  Unwalkable (by slope). */
void Boulder(FMeshSet& M, int32 Slot, const FVector2D& C, float Radius, float TopZ, float YawDeg, uint32 Seed, FStats& St)
{
    constexpr int32 Sides = 10;
    const float RingR[4] = { .9f, 1.04f, .97f, .78f };
    const float GroundC = GroundAt(C);
    const float BaseC = GroundC - FootSink;
    if (TopZ < BaseC + 6.f) return;
    FVector V[4][Sides];
    FVector N[4][Sides];
    const double Yaw = FMath::DegreesToRadians(double(YawDeg));
    for (int32 K = 0; K < Sides; ++K)
    {
        const double Ang = Yaw + 2.0 * UE_DOUBLE_PI * (K + .35 * (Hash01(K, 7, Seed) - .5)) / Sides;
        const FVector2D Dir(FMath::Cos(Ang), FMath::Sin(Ang));
        for (int32 R = 0; R < 4; ++R)
        {
            const double Rad = Radius * RingR[R] * (.9 + .2 * Hash01(K, R, Seed + 17u));
            const FVector2D P = C + Dir * Rad;
            double Z = 0.0;
            if (R == 0) Z = GroundAt(P) - FootSink;
            else if (R == 1) Z = FMath::Lerp(double(BaseC), double(TopZ), .35);
            else if (R == 2) Z = TopZ - FMath::Min(6.f, (TopZ - BaseC) * .2f);
            else Z = TopZ - .8;
            V[R][K] = Lift(P, Z);
            const double Up = R == 0 ? .15 : (R == 1 ? .05 : (R == 2 ? .45 : .85));
            N[R][K] = (FVector(Dir.X, Dir.Y, 0.0) * (1.0 - Up) + FVector::UpVector * Up).GetSafeNormal();
        }
    }
    for (int32 R = 0; R < 3; ++R)
        for (int32 K = 0; K < Sides; ++K)
        {
            const int32 L = (K + 1) % Sides;
            const FVector Q[4] = { V[R][K], V[R][L], V[R + 1][L], V[R + 1][K] };
            const FVector Nq[4] = { N[R][K], N[R][L], N[R + 1][L], N[R + 1][K] };
            const float A0 = R == 0 ? .5f : .8f, A1 = R == 2 ? 1.f : .8f;
            const FLinearColor Cq[4] = { Ao(A0), Ao(A0), Ao(A1), Ao(A1) };
            const FVector Mid = (Q[0] + Q[1] + Q[2] + Q[3]) * .25;
            Poly(M, Slot, EColl::Walk, Q, 4, Mid - Lift(C, Mid.Z), Nq, Cq);
        }
    const FVector Top = Lift(C, TopZ);
    for (int32 K = 0; K < Sides; ++K)
    {
        const int32 L = (K + 1) % Sides;
        const FVector Q[3] = { Top, V[3][K], V[3][L] };
        const FVector Nq[3] = { FVector::UpVector, N[3][K], N[3][L] };
        Poly(M, Slot, EColl::Walk, Q, 3, FVector::UpVector, Nq);
    }
    Foot(C, BaseC, St);
}

// ------------------------------------------------------------------ the plan (the landform's data, computed once)
/** A straight run of a mitred prism along a polyline (the dam's crest, the causeway, the pond's edge). Its columns run straight
 *  across; the end columns lie on the mitre lines with the next run: T(F, C) = lerp(KStart * C, Len + KEnd * C, F). */
struct FSeg
{
    FVector2D A = FVector2D::ZeroVector, B = FVector2D::ZeroVector, Dir = FVector2D(1.0, 0.0), Nrm = FVector2D(0.0, 1.0);
    double Len = 0.0, G0 = 0.0, KStart = 0.0, KEnd = 0.0;
    double T(double F, double C) const { return FMath::Lerp(KStart * C, Len + KEnd * C, F); }
    FVector2D At(double F, double C) const { return A + Dir * T(F, C) + Nrm * C; }
    /** P's offset across (C) and its fraction F between the end columns. */
    void Locate(const FVector2D& P, double& OutF, double& OutC) const
    {
        const FVector2D R = P - A;
        OutC = FVector2D::DotProduct(R, Nrm);
        const double T0 = KStart * OutC, T1 = Len + KEnd * OutC;
        OutF = (FVector2D::DotProduct(R, Dir) - T0) / FMath::Max(T1 - T0, 1e-6);
    }
    bool Covers(const FVector2D& P, double Half) const
    {
        double F = 0.0, C = 0.0;
        Locate(P, F, C);
        return F >= 0.0 && F <= 1.0 && FMath::Abs(C) <= Half;
    }
};

/** The runs along Pts, mitred at every joint (and at the closing joint when bClosed). */
TArray<FSeg> MitredRuns(const TArray<FVector2D>& Pts, bool bClosed = false)
{
    TArray<FSeg> Out;
    double G = 0.0;
    const int32 Num = Pts.Num();
    for (int32 K = 0; K < (bClosed ? Num : Num - 1); ++K)
    {
        FSeg S;
        S.A = Pts[K];
        S.B = Pts[(K + 1) % Num];
        S.Len = (S.B - S.A).Size();
        if (S.Len < 1.0) continue;
        S.Dir = (S.B - S.A) / S.Len;
        S.Nrm = Left(S.Dir);
        S.G0 = G;
        G += S.Len;
        Out.Add(S);
    }
    const int32 Runs = Out.Num();
    for (int32 K = 0; K < (bClosed ? Runs : Runs - 1); ++K)
    {
        FSeg& P0 = Out[K];
        FSeg& P1 = Out[(K + 1) % Runs];
        const FVector2D Mt = (P0.Nrm + P1.Nrm).GetSafeNormal();
        if (Mt.IsNearlyZero()) continue;
        P0.KEnd = -Cross2(P0.Nrm, Mt) / Cross2(P0.Dir, Mt);
        P1.KStart = -Cross2(P1.Nrm, Mt) / Cross2(P1.Dir, Mt);
    }
    return Out;
}

struct FDamPlan
{
    bool bOn = false;
    TArray<FSeg> Runs;   // 0: DAM[0] to the notch, 1: the notch (the arch over the weir), 2: the notch to DAM[2]
    double Total = 0.0, Half = 120.0, ArchR = 0.0;
    float DeckZ = 0.f, WaterZ = 0.f, SillZ = 0.f, WeirZ = 0.f, SpringZ = 0.f, CrownZ = 0.f;
    TArray<int32> RunOns;   // Walls() of kind Dam beyond the crest's ends
};

struct FCausewayPlan
{
    bool bOn = false;
    TArray<FSeg> Runs;
    double Total = 0.0, Half = 120.0, JointG = 0.0, JointFace = 120.0;
    float DeckZ = 0.f, WaterZ = 0.f, InvertZ = 0.f, PipeR = 22.5f;
    FVector2D Joint = FVector2D::ZeroVector, JointDir = FVector2D(1.0, 0.0), JointNrm = FVector2D(0.0, 1.0);
};

struct FHeadRun { double X0 = 0.0, X1 = 0.0; float Top = 0.f; };
struct FHeadPlan
{
    bool bOn = false;
    double Cx = 0.0, YFront = 0.0, YBack = 0.0, YEnd = 0.0, XW = 0.0, XE = 0.0, HalfOpen = 290.0, R = 0.0;
    float InvZ = 0.f, SpringZ = 0.f, CrownZ = 0.f, WaterZ = 0.f;
    TArray<FHeadRun> Runs;
    /** The arch's soffit at X (inside the opening). */
    double Soffit(double X) const
    {
        const double U = X - Cx;
        return CrownZ - R + FMath::Sqrt(FMath::Max(0.0, R * R - U * U));
    }
};

struct FPlan
{
    float PondZ = 0.f;
    TArray<FVector2D> Pond, Crest;
    double PondArea = 0.0;
    FVector2D MouthA = FVector2D::ZeroVector, MouthOut = FVector2D(0.0, 1.0);   // the pond edge Calder Branch enters by, and its outward normal
    TArray<FVector2D> CreekAt;
    TArray<float> CreekZ, CreekHalf;
    TArray<FVector2D> GlenAt;
    TArray<float> GlenZ, GlenHalf;
    int32 ReachFirst[3] = { 0, 0, 0 }, ReachLast[3] = { 0, 0, 0 };
    double FallsY = 0.0, CascY = 0.0, CurtainY = 0.0, FaceY = 0.0;
    float FallsTopZ = 0.f, CascTopZ = 0.f;
    double FallsXA = 0.0, FallsXB = 0.0, CascXA = 0.0, CascXB = 0.0;
    FVector2D FallsFlow = FVector2D(0.0, -1.0), CascFlow = FVector2D(0.0, -1.0);
    FVector2D RaceA = FVector2D::ZeroVector, RaceB = FVector2D::ZeroVector;
    float RaceHalf = 120.f;
    FDamPlan Dam;
    FCausewayPlan Way;
    FHeadPlan Head;
};

/** In points whose Y falls monotonically (indices First..Last inclusive): the I with Y[I] >= Y >= Y[I + 1], clamped. */
int32 SpanAtY(const TArray<FVector2D>& Pts, int32 First, int32 Last, double Y)
{
    if (Last <= First) return First;
    if (Y >= Pts[First].Y) return First;
    if (Y <= Pts[Last].Y) return Last - 1;
    int32 Lo = First, Hi = Last;
    while (Hi - Lo > 1)
    {
        const int32 Mid = (Lo + Hi) / 2;
        if (Pts[Mid].Y >= Y) Lo = Mid; else Hi = Mid;
    }
    return Lo;
}

/** Vals interpolated by latitude between those points (water levels go by latitude), held at the ends. */
float InterpY(const TArray<FVector2D>& Pts, const TArray<float>& Vals, int32 First, int32 Last, double Y)
{
    if (!Vals.Num()) return 0.f;
    if (Last <= First || Y >= Pts[First].Y) return Vals[First];
    if (Y <= Pts[Last].Y) return Vals[Last];
    const int32 I = SpanAtY(Pts, First, Last, Y);
    const double Den = Pts[I].Y - Pts[I + 1].Y;
    const double T = Den > 1e-9 ? (Pts[I].Y - Y) / Den : 0.0;
    return FMath::Lerp(Vals[I], Vals[I + 1], float(T));
}

float GlenLevel(const FPlan& Pl, int32 Reach, double Y, float* OutHalf = nullptr)
{
    if (!Pl.GlenAt.Num()) return 0.f;
    if (OutHalf) *OutHalf = InterpY(Pl.GlenAt, Pl.GlenHalf, Pl.ReachFirst[Reach], Pl.ReachLast[Reach], Y);
    return InterpY(Pl.GlenAt, Pl.GlenZ, Pl.ReachFirst[Reach], Pl.ReachLast[Reach], Y);
}
int32 ReachOf(const FPlan& Pl, double Y) { return Y > Pl.FallsY ? 0 : (Y > Pl.CascY ? 1 : 2); }

double GlenDist(const FPlan& Pl, const FVector2D& P)
{
    const int32 N = Pl.GlenAt.Num();
    if (N < 2) return DBL_MAX;
    const int32 K = SpanAtY(Pl.GlenAt, 0, N - 1, P.Y);
    double Best = DBL_MAX;
    for (int32 I = FMath::Max(0, K - 30); I <= FMath::Min(N - 2, K + 30); ++I) Best = FMath::Min(Best, SegDist(P, Pl.GlenAt[I], Pl.GlenAt[I + 1]));
    return Best;
}

float CreekLevel(const FPlan& Pl, double Y) { return Pl.CreekAt.Num() ? InterpY(Pl.CreekAt, Pl.CreekZ, 0, Pl.CreekAt.Num() - 1, Y) : 0.f; }
float CreekHalfAt(const FPlan& Pl, double Y) { return Pl.CreekAt.Num() ? InterpY(Pl.CreekAt, Pl.CreekHalf, 0, Pl.CreekAt.Num() - 1, Y) : 0.f; }
double CreekDist(const FPlan& Pl, const FVector2D& P)
{
    double Best = DBL_MAX;
    for (int32 I = 0; I + 1 < Pl.CreekAt.Num(); ++I) Best = FMath::Min(Best, SegDist(P, Pl.CreekAt[I], Pl.CreekAt[I + 1]));
    return Best;
}
double PondEdgeDist(const FPlan& Pl, const FVector2D& P)
{
    double Best = DBL_MAX;
    for (int32 I = 0; I < Pl.Pond.Num(); ++I) Best = FMath::Min(Best, SegDist(P, Pl.Pond[I], Pl.Pond[(I + 1) % Pl.Pond.Num()]));
    return Best;
}

/** The dam's line (its crest polyline, held flat past its ends): the glen lies south of it. */
double DamLineY(const FPlan& Pl, double X)
{
    const TArray<FVector2D>& C = Pl.Crest;
    if (C.Num() < 2) return DBL_MAX;
    if (X <= C[0].X) return C[0].Y;
    for (int32 I = 0; I + 1 < C.Num(); ++I)
        if (X <= C[I + 1].X) return FMath::Lerp(C[I].Y, C[I + 1].Y, (X - C[I].X) / FMath::Max(C[I + 1].X - C[I].X, 1e-6));
    return C.Last().Y;
}

/** The floor a structure gives the water at P (the dam's crest and its notch's sill, the causeway, the glen headwall and its
 *  barrel), or -FLT_MAX where there is none. */
float BuiltFloor(const FPlan& Pl, const FVector2D& P)
{
    float Best = -FLT_MAX;
    if (Pl.Dam.bOn)
        for (int32 K = 0; K < Pl.Dam.Runs.Num(); ++K)
            if (Pl.Dam.Runs[K].Covers(P, Pl.Dam.Half)) Best = FMath::Max(Best, K == 1 ? Pl.Dam.SillZ : Pl.Dam.DeckZ);
    if (Pl.Way.bOn)
        for (const FSeg& S : Pl.Way.Runs)
            if (S.Covers(P, Pl.Way.Half)) Best = FMath::Max(Best, Pl.Way.DeckZ);
    const FHeadPlan& H = Pl.Head;
    if (H.bOn)
    {
        const double Off = FMath::Abs(P.X - H.Cx);
        if (P.X >= H.XW && P.X <= H.XE && P.Y <= H.YFront && P.Y >= H.YBack)
        {
            float Top = H.CrownZ;
            for (const FHeadRun& R : H.Runs) if (P.X >= R.X0 && P.X <= R.X1) Top = FMath::Max(Top, R.Top);
            Best = FMath::Max(Best, Off < H.HalfOpen ? H.InvZ : Top);
        }
        else if (P.Y < H.YBack && P.Y >= H.YEnd && Off <= H.HalfOpen + 40.0)
            Best = FMath::Max(Best, Off < H.HalfOpen ? H.InvZ : float(H.CrownZ + 60.f));
    }
    return Best;
}
float FloorAt(const FPlan& Pl, const FVector2D& P) { return FMath::Max(GroundAt(P), BuiltFloor(Pl, P)); }

/** The wet extent across a ledge's lip (the ground just upstream of its line lower than the water), from its centre. */
void LipSpan(const FPlan& Pl, const FVector2D& Centre, double Y, double Upstream, float TopZ, double Limit, double& OutXA, double& OutXB)
{
    OutXA = OutXB = Centre.X;
    for (double X = Centre.X; X > Centre.X - Limit && GroundAt(FVector2D(X, Y + Upstream)) < TopZ - .5f; X -= 5.0) OutXA = X;
    for (double X = Centre.X; X < Centre.X + Limit && GroundAt(FVector2D(X, Y + Upstream)) < TopZ - .5f; X += 5.0) OutXB = X;
    (void)Pl;
}

const FPlan& Plan()
{
    static const FPlan Out = []
    {
        namespace L = HouseCalderLandform;
        FPlan P;
        P.PondZ = L::PondWaterZ();
        P.Pond = L::PondOutline();
        for (int32 I = 0; I < P.Pond.Num(); ++I) P.PondArea += Cross2(P.Pond[I], P.Pond[(I + 1) % P.Pond.Num()]) * .5;
        for (const L::FWaterPoint& W : L::CreekPoints()) { P.CreekAt.Add(W.At); P.CreekZ.Add(W.WaterZ); P.CreekHalf.Add(W.HalfWidth); }
        for (const L::FWaterPoint& W : L::GlenPoints()) { P.GlenAt.Add(W.At); P.GlenZ.Add(W.WaterZ); P.GlenHalf.Add(W.HalfWidth); }
        const L::FWaterLedge& Falls = L::CalderFalls();
        const L::FWaterLedge& Casc = L::Cascade();
        const L::FRockHouse& House = L::RockHouse();
        P.FallsY = Falls.LipA.Y;
        P.CascY = Casc.LipA.Y;
        P.CurtainY = Falls.CurtainY;
        P.FaceY = House.FaceY;
        P.FallsFlow = Falls.Flow.GetSafeNormal();
        P.CascFlow = Casc.Flow.GetSafeNormal();
        // The reaches: the dam to the falls' lip, the plunge pool to the cascade, the cascade's foot to the square's edge.
        for (int32 R = 0; R < 3; ++R) { P.ReachFirst[R] = INDEX_NONE; P.ReachLast[R] = INDEX_NONE; }
        for (int32 I = 0; I < P.GlenAt.Num(); ++I)
        {
            const int32 R = ReachOf(P, P.GlenAt[I].Y);
            if (P.ReachFirst[R] == INDEX_NONE) P.ReachFirst[R] = I;
            P.ReachLast[R] = I;
        }
        for (int32 R = 0; R < 3; ++R) if (P.ReachFirst[R] == INDEX_NONE) { P.ReachFirst[R] = 0; P.ReachLast[R] = 0; }
        if (P.Pond.Num() >= 3 && P.CreekAt.Num())
        {
            // The pond edge Calder Branch crosses (nearest its last point, the inlet), with its outward normal.
            const FVector2D Inlet = P.CreekAt.Last();
            double Best = DBL_MAX;
            for (int32 I = 0; I < P.Pond.Num(); ++I)
            {
                const FVector2D A = P.Pond[I], B = P.Pond[(I + 1) % P.Pond.Num()];
                const double D = SegDist(Inlet, A, B);
                if (D >= Best) continue;
                Best = D;
                P.MouthA = A;
                P.MouthOut = Left((B - A).GetSafeNormal()) * (P.PondArea < 0.0 ? 1.0 : -1.0);
            }
        }
        if (P.GlenAt.Num() >= 2)
        {
            P.FallsTopZ = GlenLevel(P, 0, P.FallsY);
            P.CascTopZ = GlenLevel(P, 1, P.CascY);
            float FallsHalf = 0.f, CascHalf = 0.f;
            GlenLevel(P, 0, P.FallsY + 1.0, &FallsHalf);
            GlenLevel(P, 1, P.CascY + 1.0, &CascHalf);
            LipSpan(P, (Falls.LipA + Falls.LipB) * .5, P.FallsY, 3.0, P.FallsTopZ, FallsHalf + 120.0, P.FallsXA, P.FallsXB);
            P.FallsXA = FMath::Min(P.FallsXA, Falls.LipA.X);
            P.FallsXB = FMath::Max(P.FallsXB, Falls.LipB.X);
            LipSpan(P, (Casc.LipA + Casc.LipB) * .5, P.CascY, 3.0, P.CascTopZ, CascHalf + 120.0, P.CascXA, P.CascXB);
            P.CascXA = FMath::Min(P.CascXA, Casc.LipA.X);
            P.CascXB = FMath::Max(P.CascXB, Casc.LipB.X);
        }
        // The dam: its crest DAM[0] -> DAM[1] -> DAM[2], the weir notch NotchWidth wide centred on DAM[1] (the arch spans it).
        const L::FPondDam& PD = L::PondDam();
        P.Crest = PD.Crest;
        if (PD.Race.Num() >= 2)
        {
            P.RaceA = PD.Race[0];
            P.RaceB = PD.Race.Last() + (PD.Race.Last() - PD.Race[0]).GetSafeNormal() * 220.0;
            P.RaceHalf = PD.RaceHalfWidth;
        }
        if (PD.Crest.Num() == 3)
        {
            FDamPlan& D = P.Dam;
            const FVector2D D0 = (PD.Crest[1] - PD.Crest[0]).GetSafeNormal(), D1 = (PD.Crest[2] - PD.Crest[1]).GetSafeNormal();
            const FVector2D N0 = PD.Crest[1] - D0 * (PD.NotchWidth * .5), N1 = PD.Crest[1] + D1 * (PD.NotchWidth * .5);
            D.Runs = MitredRuns({ PD.Crest[0], N0, N1, PD.Crest[2] });
            for (const FSeg& S : D.Runs) D.Total += S.Len;
            D.Half = PD.CrestWidth * .5;
            D.DeckZ = PD.CrestZ;
            D.WaterZ = P.PondZ;
            D.SillZ = P.PondZ - 8.f;      // the weir's stone sill: the race runs over it 8 cm deep ...
            D.WeirZ = P.PondZ - 4.f;      // ... drawing down 4 cm to its downstream edge
            D.SpringZ = P.PondZ + 5.f;    // the arch springs just over the water
            D.CrownZ = D.DeckZ - 25.f;    // and its soffit rises to 25 cm under the deck
            if (D.Runs.Num() == 3)
            {
                const double Span = D.Runs[1].Len, Rise = D.CrownZ - D.SpringZ;
                D.ArchR = (Span * Span * .25 + Rise * Rise) / (2.0 * FMath::Max(Rise, 1.0));
                D.bOn = Rise > 10.0;
            }
            const TArray<L::FWall>& All = L::Walls();
            for (int32 WI = 0; WI < All.Num(); ++WI)
            {
                if (All[WI].Kind != EWallKind::Dam) continue;
                const FVector2D Mid = (All[WI].A + All[WI].B) * .5;
                if (SegDist(Mid, PD.Crest[0], PD.Crest[1]) < 5.0 || SegDist(Mid, PD.Crest[1], PD.Crest[2]) < 5.0) continue;
                D.RunOns.Add(WI);
            }
        }
        // The loop's causeway over Calder Branch: the loop's own causeway stretch (the pipe at the crossing's centre).
        for (const L::FWaterCrossing& X : L::WaterCrossings())
        {
            if (X.Kind != L::EWaterCrossing::CausewayCulvert) continue;
            FCausewayPlan& W = P.Way;
            TArray<FVector2D> Line;
            for (const L::FLandformPath& Path : L::LandformPaths())
            {
                for (int32 I = 0; I < Path.Stretches.Num() && I + 1 < Path.Points.Num(); ++I)
                {
                    if (Path.Stretches[I] != L::EPathStretch::Causeway) continue;
                    TArray<FVector2D> Run = { Path.Points[I] };
                    int32 J = I;
                    while (J < Path.Stretches.Num() && J + 1 < Path.Points.Num() && Path.Stretches[J] == L::EPathStretch::Causeway) Run.Add(Path.Points[++J]);
                    bool bHere = false;
                    for (const FVector2D& Q : Run) bHere |= (Q - X.Centre).Size() < 60.0;
                    if (bHere && !Line.Num()) Line = Run;
                    I = J - 1;
                }
            }
            if (Line.Num() < 2) Line = { X.Centre - X.Axis * X.Back, X.Centre, X.Centre + X.Axis * X.On };
            W.Runs = MitredRuns(Line);
            if (!W.Runs.Num()) break;
            for (const FSeg& S : W.Runs) W.Total += S.Len;
            W.Half = X.Width * .5;
            W.DeckZ = X.DeckZ;
            W.WaterZ = X.WaterZ;
            W.InvertZ = X.InvertZ;
            W.PipeR = FMath::Max(10.f, X.PipeDiameter * .5f);
            // The pipe's joint: the line's vertex at the crossing's centre (or the centre's place on the line).
            W.Joint = X.Centre;
            W.JointDir = W.Runs[0].Dir;
            double BestD = DBL_MAX;
            for (int32 K = 0; K < W.Runs.Num(); ++K)
            {
                const FSeg& S = W.Runs[K];
                const double T = FMath::Clamp(FVector2D::DotProduct(X.Centre - S.A, S.Dir), 0.0, S.Len);
                const double D = (S.A + S.Dir * T - X.Centre).Size();
                if (D >= BestD) continue;
                BestD = D;
                W.Joint = S.A + S.Dir * T;
                W.JointG = S.G0 + T;
                W.JointDir = S.Dir;
                if (T < 1.0 && K > 0) W.JointDir = (W.Runs[K - 1].Dir + S.Dir).GetSafeNormal();
                if (T > S.Len - 1.0 && K + 1 < W.Runs.Num()) W.JointDir = (S.Dir + W.Runs[K + 1].Dir).GetSafeNormal();
            }
            W.JointNrm = Left(W.JointDir);
            double CosHalf = 1.0;
            for (const FSeg& S : W.Runs) CosHalf = FMath::Min(CosHalf, FMath::Abs(FVector2D::DotProduct(S.Nrm, W.JointNrm)));
            W.JointFace = W.Half / FMath::Max(CosHalf, .5);
            W.bOn = true;
            break;
        }
        // The glen's headwall on the hedge line: a tall arch over the water with its grate, wings to the hedge's ends.
        {
            const L::FGlenHeadwall& G = L::GlenHeadwall();
            FHeadPlan& H = P.Head;
            H.Cx = G.Centre.X;
            H.YFront = G.Centre.Y + 30.0;
            H.YBack = G.Centre.Y - 150.0;
            H.YEnd = G.Centre.Y - 550.0;
            H.XW = G.WingWestX - 30.0;
            H.XE = G.WingEastX + 30.0;
            H.HalfOpen = FMath::Max(100.0, double(G.GrateWidth) * .5);
            H.WaterZ = G.WaterZ;
            H.InvZ = G.InvertZ - 5.f;
            H.SpringZ = G.WaterZ + 130.f;
            H.CrownZ = H.SpringZ + 140.f;
            const double Rise = H.CrownZ - H.SpringZ;
            H.R = (H.HalfOpen * H.HalfOpen + Rise * Rise) / (2.0 * Rise);
            // Every top stands 2 m over the highest ground within 1.5 m of it (both sides), the middle at least 1.1 m over the
            // arch's crown; outward the wings step up with the glen's sides (never down).
            auto HighGround = [&H](double X0, double X1)
            {
                float Hi = -FLT_MAX;
                for (double X = X0; X <= X1 + .1; X += 25.0)
                    for (const double Y : { H.YFront + 150.0, H.YFront + 3.0, H.YBack - 3.0, H.YBack - 150.0 })
                        Hi = FMath::Max(Hi, GroundAt(FVector2D(X, Y)));
                return Hi;
            };
            auto Round5 = [](float Z) { return FMath::CeilToFloat(Z / 5.f) * 5.f; };
            const double C0 = H.Cx - H.HalfOpen - 150.0, C1 = H.Cx + H.HalfOpen + 150.0;
            const float Mid = Round5(FMath::Max(H.CrownZ + 110.f, FMath::Max(HighGround(H.Cx - G.FloorHalfWidth, H.Cx + G.FloorHalfWidth) + 220.f, HighGround(C0 - 150.0, C1 + 150.0) + 200.f)));
            H.Runs.Add({ C0, C1, Mid });
            float Prev = Mid;
            for (double X = C0; X > H.XW + .5; )
            {
                const double X0 = FMath::Max(H.XW, X - 200.0);
                Prev = Round5(FMath::Max(Prev, HighGround(X0 - 150.0, X + 150.0) + 200.f));
                H.Runs.Insert({ X0, X, Prev }, 0);
                X = X0;
            }
            Prev = Mid;
            for (double X = C1; X < H.XE - .5; )
            {
                const double X1 = FMath::Min(H.XE, X + 200.0);
                Prev = Round5(FMath::Max(Prev, HighGround(X - 150.0, X1 + 150.0) + 200.f));
                H.Runs.Add({ X, X1, Prev });
                X = X1;
            }
            H.bOn = H.XE - H.XW > 2.0 * H.HalfOpen;
        }
        return P;
    }();
    return Out;
}

// ------------------------------------------------------------------ the water sheets
struct FClip { FVector2D O = FVector2D::ZeroVector, N = FVector2D(0.0, 1.0); float Z = 0.f; };
struct FSheetStats { int32 Cells = 0, Wet = 0, Tucked = 0, Clipped = 0, Tris = 0; float MaxDepth = 0.f; };

/** A water sheet on the grid Xs x Ys. Level(P, Z) says whether P belongs to the body and always sets the body's level there;
 *  Floor(P) is what the water lies on. A vertex is wet where it belongs and its floor lies below the level; a cell with a wet
 *  corner is drawn, and its other corners sink Tuck under their floor (never over the level), so the sheet's edge always dives
 *  under its bank. Rows for which Fixed(J) holds are designed edges (a ledge's line): they keep the level, and read their
 *  floor RowDY(J) in. Cells are clipped to the half-planes Clips (keeping dot(P - O, N) >= 0; new corners take the clip's Z).
 *  Colour: R = Foam(P, depth) (foam and riffles), A = the depth (0 shallow .. 1 at 100 cm). */
void EmitSheet(FMeshSet& M, int32 Slot, const TArray<double>& Xs, const TArray<double>& Ys,
    TFunctionRef<bool(const FVector2D&, float&)> Level, TFunctionRef<float(const FVector2D&)> Floor,
    TFunctionRef<float(const FVector2D&, float)> Foam, TFunctionRef<bool(int32)> Fixed, TFunctionRef<double(int32)> RowDY,
    const TArray<FClip>& Clips, FSheetStats& Out)
{
    const int32 NX = Xs.Num(), NY = Ys.Num();
    if (NX < 2 || NY < 2) return;
    struct FVert { float Z = 0.f, LevelZ = 0.f, FloorZ = 0.f; FLinearColor Col = FLinearColor::Black; bool bIn = false, bWet = false, bDone = false; };
    TArray<FVert> Grid;
    Grid.SetNum(NX * NY);
    for (int32 Row = 0; Row < NY; ++Row)
        for (int32 Col = 0; Col < NX; ++Col)
        {
            FVert& V = Grid[Row * NX + Col];
            V.bIn = Level(FVector2D(Xs[Col], Ys[Row]), V.LevelZ);
        }
    auto ColourAt = [&Foam](const FVector2D& P, float Depth)
    {
        return FLinearColor(FMath::Clamp(Foam(P, Depth), 0.f, 1.f), 0.f, 0.f, DepthA(Depth));
    };
    auto Need = [&](int32 Col, int32 Row) -> FVert&
    {
        FVert& V = Grid[Row * NX + Col];
        if (V.bDone) return V;
        V.bDone = true;
        const FVector2D P(Xs[Col], Ys[Row]);
        V.FloorZ = Floor(P + FVector2D(0.0, RowDY(Row)));
        V.bWet = V.bIn && V.FloorZ < V.LevelZ;
        V.Z = (V.bWet || Fixed(Row)) ? V.LevelZ : FMath::Min(V.LevelZ, V.FloorZ - Tuck);
        const float Depth = FMath::Max(0.f, V.Z - V.FloorZ);
        V.Col = ColourAt(P, Depth);
        if (V.bWet) { ++Out.Wet; Out.MaxDepth = FMath::Max(Out.MaxDepth, Depth); }
        else if (V.Z < V.LevelZ - .01f) ++Out.Tucked;
        return V;
    };
    struct FPt { FVector2D P; float Z = 0.f; FLinearColor Col; };
    TArray<FPt, TInlineAllocator<12>> Cur, Next;
    for (int32 Row = 0; Row + 1 < NY; ++Row)
        for (int32 Col = 0; Col + 1 < NX; ++Col)
        {
            const int32 K00 = Row * NX + Col, K10 = K00 + 1, K01 = K00 + NX, K11 = K01 + 1;
            if (!(Grid[K00].bIn || Grid[K10].bIn || Grid[K01].bIn || Grid[K11].bIn)) continue;
            const FVert& Va = Need(Col, Row);
            const FVert& Vb = Need(Col + 1, Row);
            const FVert& Vc = Need(Col + 1, Row + 1);
            const FVert& Vd = Need(Col, Row + 1);
            if (!(Va.bWet || Vb.bWet || Vc.bWet || Vd.bWet)) continue;
            const FVector2D Mid((Xs[Col] + Xs[Col + 1]) * .5, (Ys[Row] + Ys[Row + 1]) * .5);
            if (HouseCalderTerrain::InFrozen(float(Mid.X), float(Mid.Y))) continue;
            Cur.Reset();
            Cur.Add({ FVector2D(Xs[Col], Ys[Row]), Va.Z, Va.Col });
            Cur.Add({ FVector2D(Xs[Col + 1], Ys[Row]), Vb.Z, Vb.Col });
            Cur.Add({ FVector2D(Xs[Col + 1], Ys[Row + 1]), Vc.Z, Vc.Col });
            Cur.Add({ FVector2D(Xs[Col], Ys[Row + 1]), Vd.Z, Vd.Col });
            bool bClipped = false;
            for (const FClip& Cl : Clips)
            {
                bool bAllIn = true;
                for (const FPt& Q : Cur) bAllIn &= FVector2D::DotProduct(Q.P - Cl.O, Cl.N) >= 0.0;
                if (bAllIn) continue;
                bClipped = true;
                Next.Reset();
                for (int32 K = 0; K < Cur.Num(); ++K)
                {
                    const FPt& Pa = Cur[K];
                    const FPt& Pb = Cur[(K + 1) % Cur.Num()];
                    const double Fa = FVector2D::DotProduct(Pa.P - Cl.O, Cl.N), Fb = FVector2D::DotProduct(Pb.P - Cl.O, Cl.N);
                    if (Fa >= 0.0) Next.Add(Pa);
                    if ((Fa >= 0.0) != (Fb >= 0.0))
                    {
                        FPt Cut;
                        Cut.P = Pa.P + (Pb.P - Pa.P) * (Fa / (Fa - Fb));
                        Cut.Z = Cl.Z;
                        Cut.Col = ColourAt(Cut.P, FMath::Max(0.f, Cut.Z - Floor(Cut.P)));
                        Next.Add(Cut);
                    }
                }
                Swap(Cur, Next);
                if (Cur.Num() < 3) break;
            }
            if (Cur.Num() < 3) continue;
            FVector Pts[12], Nrm[12];
            FLinearColor Cols[12];
            const int32 Num = FMath::Min(Cur.Num(), 12);
            for (int32 K = 0; K < Num; ++K) { Pts[K] = Lift(Cur[K].P, Cur[K].Z); Nrm[K] = FVector::UpVector; Cols[K] = Cur[K].Col; }
            const int32 Before = M.Parts[Slot].I.Num();
            Poly(M, Slot, EColl::None, Pts, Num, FVector::UpVector, Nrm, Cols);
            Out.Tris += (M.Parts[Slot].I.Num() - Before) / 3;
            ++Out.Cells;
            Out.Clipped += bClipped ? 1 : 0;
        }
}

/** A falling sheet: it leaves the edge Top (points across, in order) along Out at Speed (pushed Lip out first), and falls on a
 *  parabola to BottomZ (under the water it falls into); its middle bows out by Bow at the bottom and it spreads by Spread of its
 *  width. Both faces are drawn (the rock house sees the falls from behind). Colour: R foam from FoamTop at the edge to 1 (down
 *  to FoamMid between when FoamMid >= 0: white at the lip and the foot, dark and streaked between), A Depth. Tear: the share
 *  of its columns that break into strands partway down (the two at each end always fray), so the rock shows through.
 *  Corrugate: each point across leaves at its own speed (Speed x 1 +- Corrugate / 2), so the sheet ridges as it falls. */
struct FCurtain
{
    TArray<FVector> Top;
    FVector2D Out = FVector2D(0.0, -1.0);
    float Speed = 100.f, Lip = 0.f, BottomZ = 0.f, Bow = 0.f, Spread = 0.f, FoamTop = .7f, FoamMid = -1.f, Depth = .3f, Tear = 0.f, Corrugate = 0.f;
    int32 Rows = 16;
    uint32 Seed = 0u;
};

/** Where a curtain lands (the centre of its edge's foot, ignoring the bow). */
FVector2D Landing(const FVector& Edge, const FVector2D& Out, float Lip, float Speed, float BottomZ)
{
    const double T = FMath::Sqrt(FMath::Max(0.0, 2.0 * (Edge.Z - BottomZ) / Gravity));
    return FVector2D(Edge.X, Edge.Y) + Out * (Lip + Speed * T);
}

int32 EmitCurtain(FMeshSet& M, int32 Slot, const FCurtain& Cu)
{
    const int32 NC = Cu.Top.Num();
    if (NC < 2 || Cu.Rows < 2) return 0;
    const FVector Mid = (Cu.Top[0] + Cu.Top.Last()) * .5;
    const FVector2D Across = FVector2D(Cu.Top.Last() - Cu.Top[0]).GetSafeNormal();
    const double HalfSpan = FMath::Max(1.0, FVector2D(Cu.Top.Last() - Cu.Top[0]).Size() * .5);
    const bool bLipRow = Cu.Lip > .5f;
    const int32 NR = Cu.Rows + (bLipRow ? 1 : 0);
    TArray<FVector> V;
    V.SetNum(NC * NR);
    for (int32 K = 0; K < NC; ++K)
    {
        const FVector& E = Cu.Top[K];
        const double U = FMath::Clamp(FVector2D::DotProduct(FVector2D(E - Mid), Across) / HalfSpan, -1.0, 1.0);
        const double TEnd = FMath::Sqrt(FMath::Max(1e-4, 2.0 * (E.Z - Cu.BottomZ) / Gravity));
        const double SpeedK = Cu.Speed * (1.0 + Cu.Corrugate * (Hash01(K, 4, Cu.Seed) - .5));
        for (int32 R = 0; R < NR; ++R)
        {
            if (bLipRow && R == 0) { V[K * NR + R] = E; continue; }
            const int32 Step = bLipRow ? R - 1 : R;
            const double T = TEnd * double(Step) / (Cu.Rows - 1), F = T / TEnd;
            const double OutD = Cu.Lip + SpeedK * T + Cu.Bow * (1.0 - U * U) * F;
            const FVector2D P = FVector2D(E.X, E.Y) + Cu.Out * OutD + Across * (U * HalfSpan * Cu.Spread * F);
            V[K * NR + R] = Lift(P, E.Z - .5 * Gravity * T * T - (bLipRow && Step == 0 ? .5 : 0.0));
        }
    }
    auto Vx = [&V, NC, NR](int32 K, int32 R) -> const FVector& { return V[FMath::Clamp(K, 0, NC - 1) * NR + FMath::Clamp(R, 0, NR - 1)]; };
    const FVector Facing(Cu.Out.X, Cu.Out.Y, .25);
    TArray<FVector> N;
    N.SetNum(V.Num());
    for (int32 K = 0; K < NC; ++K)
        for (int32 R = 0; R < NR; ++R)
        {
            FVector Nv = FVector::CrossProduct(Vx(K + 1, R) - Vx(K - 1, R), Vx(K, R + 1) - Vx(K, R - 1)).GetSafeNormal();
            if (FVector::DotProduct(Nv, Facing) < 0.0) Nv = -Nv;
            N[K * NR + R] = Nv.IsNearlyZero() ? FVector(Cu.Out.X, Cu.Out.Y, 0.0) : Nv;
        }
    // FoamMid: by the height fallen (F * F; the rows are even in time): white over the top eighth, dark from there to 70%,
    // whitening into the foot
    auto FoamAt = [&Cu](float F)
    {
        if (Cu.FoamMid < 0.f) return Cu.FoamTop + (1.f - Cu.FoamTop) * F;
        const float H = F * F;
        return H < .7f ? FMath::Lerp(Cu.FoamTop, Cu.FoamMid, float(Smooth01(H / .12f))) : FMath::Lerp(Cu.FoamMid, 1.f, float(Smooth01((H - .7f) / .3f)));
    };
    // Static tapered tails read as hanging cloth. In the opt-in animated
    // material trial, extend the same noncolliding columns to the pool and
    // let the shader's moving mask break the liquid into falling strands.
    const bool AnimatedJets = HouseCalderLandscape::Enabled()
        && FParse::Param(FCommandLine::Get(), TEXT("CalderFallsJetsTrial"));
    const float Tear = AnimatedJets ? 0.f : Cu.Tear;
    // Where each column's sheet ends (the fraction down its rows; > 1: it reaches the bottom).
    TArray<float> TearAt;
    TearAt.Init(2.f, NC - 1);
    if (Tear > 0.f)
        for (int32 K = 0; K + 1 < NC; ++K)
        {
            // the ends fray first; inside, single columns open (never two side by side) from a quarter of the height down
            const int32 FromEnd = FMath::Min(K, NC - 2 - K);
            if (FromEnd < 2) TearAt[K] = .3f + .2f * FromEnd + .15f * Hash01(K, 1, Cu.Seed);
            else if (FromEnd > 2 && TearAt[K - 1] > 1.f && Hash01(K, 2, Cu.Seed) < Tear) TearAt[K] = .55f + .35f * Hash01(K, 3, Cu.Seed);
        }
    const int32 Before = M.Parts[Slot].I.Num();
    for (int32 K = 0; K + 1 < NC; ++K)
        for (int32 R = 0; R + 1 < NR; ++R)
        {
            const int32 A = K * NR + R, B = (K + 1) * NR + R;
            const float F0 = float(R) / (NR - 1), F1 = float(R + 1) / (NR - 1);
            if (F0 >= TearAt[K]) break;
            const float R0 = FoamAt(F0), R1 = FoamAt(F1);
            const FVector Front[4] = { N[A], N[B], N[B + 1], N[A + 1] };
            const FVector Back[4] = { -N[A], -N[B], -N[B + 1], -N[A + 1] };
            const FVector Avg = Front[0] + Front[1] + Front[2] + Front[3];
            if (F1 > TearAt[K])
            {
                // the strand's end: it narrows to a point (a tail, not a cut edge)
                const FVector Tail[3] = { V[A], V[B], (V[A + 1] + V[B + 1]) * .5 };
                const FVector TailN = ((N[A + 1] + N[B + 1]) * .5).GetSafeNormal();
                const FVector TailFront[3] = { N[A], N[B], TailN };
                const FVector TailBack[3] = { -N[A], -N[B], -TailN };
                const FLinearColor Ct[3] = { FLinearColor(R0, 0, 0, Cu.Depth), FLinearColor(R0, 0, 0, Cu.Depth), FLinearColor(R1, 0, 0, Cu.Depth) };
                Poly(M, Slot, EColl::None, Tail, 3, Avg, TailFront, Ct);
                Poly(M, Slot, EColl::None, Tail, 3, -Avg, TailBack, Ct);
                break;
            }
            const FVector Q[4] = { V[A], V[B], V[B + 1], V[A + 1] };
            const FLinearColor Cq[4] = { FLinearColor(R0, 0, 0, Cu.Depth), FLinearColor(R0, 0, 0, Cu.Depth), FLinearColor(R1, 0, 0, Cu.Depth), FLinearColor(R1, 0, 0, Cu.Depth) };
            Poly(M, Slot, EColl::None, Q, 4, Avg, Front, Cq);
            Poly(M, Slot, EColl::None, Q, 4, -Avg, Back, Cq);
        }
    if (AnimatedJets)
    {
        UE_LOG(LogTemp, Display, TEXT("CALDER_FALLS_JETS_GEOMETRY slot=%d staticTails=0 triangles=%d collision=none"), Slot, (M.Parts[Slot].I.Num() - Before) / 3);
    }
    return (M.Parts[Slot].I.Num() - Before) / 3;
}

/** Foam round the stepping stones of one crossing (the water breaking on them). */
float StoneFoam(const FVector2D& P, int32 Crossing)
{
    float R = 0.f;
    for (const HouseCalderLandform::FSteppingStone& S : HouseCalderLandform::SteppingStones())
    {
        if (S.Crossing != Crossing) continue;
        const double D = (P - S.At).Size() - S.Radius;
        if (D < 45.0) R = FMath::Max(R, float(.55 * (1.0 - Smooth01(D / 45.0))));
    }
    return R;
}

void LogBody(const TCHAR* Name, const FSheetStats& S, FStats& St)
{
    UE_LOG(LogTemp, Display, TEXT("CALDER_WATER_BODY name=%s cells=%d tris=%d wet=%d tucked=%d clipped=%d maxDepth=%.1f"), Name, S.Cells, S.Tris, S.Wet, S.Tucked, S.Clipped, S.MaxDepth);
    St.Surfaces += S.Cells > 0 ? 1 : 0;
    St.WaterTris += S.Tris;
    St.Cells += S.Cells;
    St.Wet += S.Wet;
    St.Tucked += S.Tucked;
    St.MaxDepth = FMath::Max(St.MaxDepth, S.MaxDepth);
}

/** Gander Pond, its spillway race and the weir notch (one flat sheet at the pond's level, drawing down 4 cm over the sill),
 *  and the weir's sheet falling 1.4 m to the outlet pool. */
void PondWater(FMeshSet& M, const FPlan& Pl, FStats& St)
{
    if (Pl.Pond.Num() < 3) return;
    const FDamPlan& D = Pl.Dam;
    FBox2D Box(ForceInit);
    for (const FVector2D& P : Pl.Pond) Box += P;
    Box += Pl.RaceA;
    Box += Pl.RaceB;
    const double X0 = FMath::FloorToDouble((Box.Min.X - 200.0) / PondStep) * PondStep, X1 = FMath::CeilToDouble((Box.Max.X + 200.0) / PondStep) * PondStep;
    const double Y0 = FMath::CeilToDouble((Box.Max.Y + 200.0) / PondStep) * PondStep, Y1 = FMath::FloorToDouble((Box.Min.Y - 100.0) / PondStep) * PondStep;
    const TArray<double> Xs = Steps(X0, X1, PondStep), Ys = Steps(Y0, Y1, PondStep);
    auto LevelAt = [&Pl, &D](const FVector2D& P, float& OutZ)
    {
        OutZ = Pl.PondZ;
        if (D.bOn)
        {
            double F = 0.0, C = 0.0;
            D.Runs[1].Locate(P, F, C);
            if (F >= 0.0 && F <= 1.0 && FMath::Abs(C) <= D.Half + 1.0)
            {
                OutZ = FMath::Lerp(D.WeirZ, D.WaterZ, float(FMath::Clamp((C + D.Half) / (2.0 * D.Half), 0.0, 1.0)));
                return true;
            }
        }
        return InOutline(P, Pl.Pond) || SegDist(P, Pl.RaceA, Pl.RaceB) <= Pl.RaceHalf + 50.0;
    };
    auto FloorFn = [&Pl](const FVector2D& P) { return FloorAt(Pl, P); };
    const FVector2D Inlet = Pl.CreekAt.Num() ? Pl.CreekAt.Last() : FVector2D(1e9, 1e9);
    auto FoamFn = [&Pl, &D, Inlet](const FVector2D& P, float)
    {
        float R = .25f * float(1.0 - Smooth01((P - Inlet).Size() / 350.0));
        if (D.bOn && SegDist(P, Pl.RaceA, Pl.RaceB) < Pl.RaceHalf + 60.0)
        {
            double F = 0.0, C = 0.0;
            D.Runs[1].Locate(P, F, C);
            R = FMath::Max(R, .35f * float(RampD(C, D.Half + 250.0, -D.Half)));
        }
        return R;
    };
    TArray<FClip> Clips;
    if (D.bOn) Clips.Add({ D.Runs[1].A - D.Runs[1].Nrm * D.Half, D.Runs[1].Nrm, D.WeirZ });   // the weir's downstream edge
    FSheetStats S;
    EmitSheet(M, WPond, Xs, Ys, LevelAt, FloorFn, FoamFn, [](int32) { return false; }, [](int32) { return 0.0; }, Clips, S);
    LogBody(TEXT("pond"), S, St);
    if (!D.bOn) return;
    // The weir's sheet: over the sill's downstream edge, 1.4 m down to the outlet pool.
    FCurtain Cu;
    const FSeg& N = D.Runs[1];
    const int32 Cols = FMath::Max(2, FMath::CeilToInt(N.Len / 20.0));
    for (int32 K = 0; K <= Cols; ++K) Cu.Top.Add(Lift(N.At(double(K) / Cols, -D.Half), D.WeirZ));
    Cu.Out = -N.Nrm;
    Cu.Speed = 75.f;
    Cu.Lip = 6.f;
    Cu.Bow = 6.f;
    Cu.Spread = .03f;
    Cu.FoamTop = .55f;
    Cu.Depth = .3f;
    Cu.Rows = 10;
    const FVector2D Land = Landing(Lift(N.At(.5, -D.Half), D.WeirZ), Cu.Out, Cu.Lip, Cu.Speed, D.WeirZ - 140.f);
    Cu.BottomZ = GlenLevel(Pl, 0, Land.Y) - 6.f;
    const int32 Tris = EmitCurtain(M, WWeir, Cu);
    St.WaterTris += Tris;
    St.Surfaces += Tris > 0 ? 1 : 0;
}

/** Calder Branch above the pond: its level by latitude, in three stretches (each its own mesh, for culling); over the gravel of
 *  its mouth (the last 9 m before the pond, where the landform's bed rises over the water) a 2.5 cm riffle. It ends on the
 *  pond's edge 1.5 cm over the pond. */
void CreekWater(FMeshSet* const (&Chunks)[3], const FPlan& Pl, FStats& St)
{
    if (Pl.CreekAt.Num() < 2) return;
    const double Top = Pl.CreekAt[0].Y, Bottom = Pl.CreekAt.Last().Y - 100.0;
    const double Bands[4] = { Top, FMath::Lerp(Top, Bottom, 1.0 / 3.0), FMath::Lerp(Top, Bottom, 2.0 / 3.0), Bottom };
    const FCausewayPlan& W = Pl.Way;
    const FVector2D Downstream = (W.JointNrm * (W.JointNrm.Y < 0.0 ? 1.0 : -1.0));   // the creek runs south
    const FVector2D UpFace = W.Joint - Downstream * W.JointFace, DownFace = W.Joint + Downstream * W.JointFace;
    auto InMouth = [&Pl](const FVector2D& P)
    {
        return !InOutline(P, Pl.Pond) && PondEdgeDist(Pl, P) < MouthReach && CreekDist(Pl, P) <= CreekHalfAt(Pl, P.Y) - 20.0 && BuiltFloor(Pl, P) == -FLT_MAX;
    };
    auto LevelAt = [&Pl, &InMouth](const FVector2D& P, float& OutZ)
    {
        OutZ = CreekLevel(Pl, P.Y);
        if (CreekDist(Pl, P) > CreekHalfAt(Pl, P.Y) + 150.0) return false;
        if (InMouth(P)) OutZ = FMath::Max(OutZ, GroundAt(P) + MouthFilm);
        return true;
    };
    auto FloorFn = [&Pl](const FVector2D& P) { return FloorAt(Pl, P); };
    auto FoamFn = [&Pl, &W, &InMouth, UpFace, DownFace, Downstream](const FVector2D& P, float)
    {
        float R = .07f + .15f * FMath::Max(0.f, Value3(FVector(0.0, P.Y, 0.0), 700.f, 9201u));
        if (InMouth(P)) R = FMath::Max(R, .45f);
        R = FMath::Max(R, StoneFoam(P, 0));
        if (W.bOn)
        {
            R = FMath::Max(R, .3f * float(1.0 - Smooth01((P - UpFace).Size() / 220.0)));
            if (FVector2D::DotProduct(P - DownFace, Downstream) > -20.0) R = FMath::Max(R, .6f * float(1.0 - Smooth01((P - DownFace).Size() / 260.0)));
        }
        return R;
    };
    TArray<FClip> Clips;
    Clips.Add({ Pl.MouthA, Pl.MouthOut, Pl.PondZ + 1.5f });   // the pond's edge: the creek gives way to the pond
    FSheetStats Total;
    for (int32 C = 0; C < 3; ++C)
    {
        double MinX = DBL_MAX, MaxX = -DBL_MAX;
        for (const FVector2D& P : Pl.CreekAt)
            if (P.Y <= Bands[C] + 2000.0 && P.Y >= Bands[C + 1] - 2000.0) { MinX = FMath::Min(MinX, P.X); MaxX = FMath::Max(MaxX, P.X); }
        if (MinX > MaxX) continue;
        const TArray<double> Xs = Steps(FMath::FloorToDouble((MinX - 600.0) / CreekStep) * CreekStep, FMath::CeilToDouble((MaxX + 600.0) / CreekStep) * CreekStep, CreekStep);
        const TArray<double> Ys = Steps(Bands[C], Bands[C + 1], CreekStep);
        FSheetStats S;
        EmitSheet(*Chunks[C], WCreek, Xs, Ys, LevelAt, FloorFn, FoamFn, [](int32) { return false; }, [](int32) { return 0.0; }, Clips, S);
        Total.Cells += S.Cells; Total.Tris += S.Tris; Total.Wet += S.Wet; Total.Tucked += S.Tucked; Total.Clipped += S.Clipped;
        Total.MaxDepth = FMath::Max(Total.MaxDepth, S.MaxDepth);
    }
    LogBody(TEXT("creek"), Total, St);
}

/** The glen below the dam: three reaches (the outlet pool to the falls' lip, the plunge pool to the cascade, the cascade's foot to
 *  the square's edge), the brink over the caprock, Calder Falls' curtain and the cascade. */
void GlenWater(FMeshSet& M, const FPlan& Pl, FStats& St)
{
    if (Pl.GlenAt.Num() < 2) return;
    const FDamPlan& D = Pl.Dam;
    const FHeadPlan& H = Pl.Head;
    auto FloorFn = [&Pl](const FVector2D& P) { return FloorAt(Pl, P); };
    const double DamY = Pl.GlenAt[0].Y;
    // The curtains' feet (for the foam round them).
    FVector2D WeirFoot0(1e9, 1e9), WeirFoot1(1e9, 1e9);
    if (D.bOn)
    {
        const FSeg& N = D.Runs[1];
        WeirFoot0 = Landing(Lift(N.At(0.0, -D.Half), D.WeirZ), -N.Nrm, 6.f, 75.f, D.WeirZ - 140.f);
        WeirFoot1 = Landing(Lift(N.At(1.0, -D.Half), D.WeirZ), -N.Nrm, 6.f, 75.f, D.WeirZ - 140.f);
    }
    const float FallsEdgeZ = Pl.FallsTopZ - 2.f;
    const float PoolZ = GlenLevel(Pl, 1, Pl.CurtainY - 100.0);
    const FVector2D FallsFoot0 = Landing(FVector(Pl.FallsXA, Pl.CurtainY, FallsEdgeZ), Pl.FallsFlow, 0.f, 115.f, PoolZ);
    const FVector2D FallsFoot1 = Landing(FVector(Pl.FallsXB, Pl.CurtainY, FallsEdgeZ), Pl.FallsFlow, 0.f, 115.f, PoolZ);
    const float CascLowZ = GlenLevel(Pl, 2, Pl.CascY - 100.0);
    const FVector2D CascFoot0 = Landing(FVector(Pl.CascXA, Pl.CascY, Pl.CascTopZ), Pl.CascFlow, 12.f, 140.f, CascLowZ);
    const FVector2D CascFoot1 = Landing(FVector(Pl.CascXB, Pl.CascY, Pl.CascTopZ), Pl.CascFlow, 12.f, 140.f, CascLowZ);

    auto Reach = [&](int32 R, int32 Slot, double YTop, double YBottom, bool bFirstFixed, bool bLastFixed, TFunctionRef<float(const FVector2D&, float)> Foam, const TCHAR* Name)
    {
        double MinX = DBL_MAX, MaxX = -DBL_MAX;
        for (int32 I = Pl.ReachFirst[R]; I <= Pl.ReachLast[R]; ++I) { MinX = FMath::Min(MinX, Pl.GlenAt[I].X); MaxX = FMath::Max(MaxX, Pl.GlenAt[I].X); }
        const double Reach0 = R == 0 ? 450.0 : 0.0;
        const TArray<double> Xs = Steps(FMath::FloorToDouble((MinX - 1000.0 - Reach0) / GlenStep) * GlenStep, FMath::CeilToDouble((MaxX + 1000.0 + Reach0) / GlenStep) * GlenStep, GlenStep);
        const TArray<double> Ys = Steps(YTop, YBottom, GlenStep);
        const int32 Last = Ys.Num() - 1;
        auto LevelAt = [&Pl, R, Reach0, DamY](const FVector2D& P, float& OutZ)
        {
            float Half = 0.f;
            OutZ = GlenLevel(Pl, R, P.Y, &Half);
            if (R == 0 && P.Y >= DamLineY(Pl, P.X)) return false;
            if (R == 2 && P.Y < -HouseCalderTerrain::Half + 50.0) return false;   // the square's edge: the sheet dives into the bed
            const double Extra = Reach0 * (1.0 - RampD(P.Y, DamY - 300.0, DamY - 1000.0));
            return GlenDist(Pl, P) <= Half + 120.0 + Extra;
        };
        auto Fixed = [bFirstFixed, bLastFixed, Last](int32 Row) { return (bFirstFixed && Row == 0) || (bLastFixed && Row == Last); };
        auto RowDY = [bFirstFixed, bLastFixed, Last](int32 Row) { return bFirstFixed && Row == 0 ? -3.0 : (bLastFixed && Row == Last ? 3.0 : 0.0); };
        FSheetStats S;
        EmitSheet(M, Slot, Xs, Ys, LevelAt, FloorFn, Foam, Fixed, RowDY, TArray<FClip>(), S);
        LogBody(Name, S, St);
    };
    double DamTop = DamY;
    for (const FVector2D& C : Pl.Crest) DamTop = FMath::Max(DamTop, C.Y);
    Reach(0, WGlen0, FMath::CeilToDouble(DamTop / GlenStep) * GlenStep, Pl.FallsY, false, true,
        [&Pl, WeirFoot0, WeirFoot1](const FVector2D& P, float)
        {
            float R = .08f + .08f * FMath::Max(0.f, Value3(FVector(P.X, P.Y, 0.0), 420.f, 9311u));
            R = FMath::Max(R, .95f * float(1.0 - Smooth01(SegDist(P, WeirFoot0, WeirFoot1) / 300.0)));
            return R + .3f * float(RampD(P.Y, Pl.FallsY + 500.0, Pl.FallsY));
        }, TEXT("glenUpper"));
    Reach(1, WPool, Pl.FallsY, Pl.CascY, true, true,
        [&Pl, FallsFoot0, FallsFoot1](const FVector2D& P, float)
        {
            float R = .12f + FMath::Max(0.f, float(1.0 - Smooth01(SegDist(P, FallsFoot0, FallsFoot1) / 450.0)));
            return R + .25f * float(RampD(P.Y, Pl.CascY + 400.0, Pl.CascY));
        }, TEXT("plungePool"));
    Reach(2, WGlen2, Pl.CascY, -HouseCalderTerrain::Half, true, false,
        [&H, CascFoot0, CascFoot1](const FVector2D& P, float)
        {
            float R = .1f + .1f * FMath::Max(0.f, Value3(FVector(P.X, P.Y, 0.0), 520.f, 9312u));
            R = FMath::Max(R, .9f * float(1.0 - Smooth01(SegDist(P, CascFoot0, CascFoot1) / 350.0)));
            R = FMath::Max(R, StoneFoam(P, 1));
            if (H.bOn && FMath::Abs(P.X - H.Cx) < H.HalfOpen && P.Y > H.YFront) R = FMath::Max(R, .35f * float(1.0 - Smooth01((P.Y - H.YFront) / 200.0)));
            return R;
        }, TEXT("glenLower"));

    // The brink: the lip's water running out over the caprock to the overhang's edge (2 cm down), then the falls.
    TArray<double> Xs = Steps(Pl.FallsXA, Pl.FallsXB, GlenStep);
    const double BrinkYs[4] = { Pl.FallsY, FMath::Lerp(Pl.FallsY, Pl.CurtainY, 1.0 / 3.0), FMath::Lerp(Pl.FallsY, Pl.CurtainY, 2.0 / 3.0), Pl.CurtainY };
    int32 BrinkTris = 0;
    for (int32 J = 0; J < 3; ++J)
        for (int32 I = 0; I + 1 < Xs.Num(); ++I)
        {
            FVector Q[4];
            FLinearColor Cq[4];
            const double Ys2[4] = { BrinkYs[J], BrinkYs[J], BrinkYs[J + 1], BrinkYs[J + 1] };
            const double Xs2[4] = { Xs[I], Xs[I + 1], Xs[I + 1], Xs[I] };
            for (int32 K = 0; K < 4; ++K)
            {
                const double F = (Pl.FallsY - Ys2[K]) / FMath::Max(1.0, Pl.FallsY - Pl.CurtainY);
                Q[K] = FVector(Xs2[K], Ys2[K], Pl.FallsTopZ - 2.0 * F);
                Cq[K] = FLinearColor(float(.3 + .4 * F), 0.f, 0.f, .2f);
            }
            const FVector Up[4] = { FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector };
            const int32 Before = M.Parts[WGlen0].I.Num();
            Poly(M, WGlen0, EColl::None, Q, 4, FVector::UpVector, Up, Cq);
            BrinkTris += (M.Parts[WGlen0].I.Num() - Before) / 3;
        }
    St.WaterTris += BrinkTris;
    // Calder Falls: 6.8 m from the overhang's edge into the plunge pool, bowed, spreading, white.
    {
        FCurtain Cu;
        for (const double X : Xs) Cu.Top.Add(FVector(X, Pl.CurtainY, FallsEdgeZ));
        Cu.Out = Pl.FallsFlow;
        Cu.Speed = 115.f;
        Cu.Bow = 22.f;
        Cu.Spread = .12f;
        Cu.BottomZ = PoolZ - 6.f;
        Cu.FoamTop = 1.f;
        Cu.FoamMid = .6f;
        Cu.Depth = .95f;   // dark water between the streaks (a low A shows the bed albedo, stretched down a vertical sheet)
        Cu.Tear = .4f;
        Cu.Corrugate = .35f;
        Cu.Seed = 6101u;
        Cu.Rows = 18;
        const int32 Tris = EmitCurtain(M, WFalls, Cu);
        St.WaterTris += Tris;
        St.Surfaces += Tris > 0 ? 1 : 0;
    }
    // The cascade: 2 m over its ledge, pushed 12 cm out from the rock.
    {
        FCurtain Cu;
        for (const double X : Steps(Pl.CascXA, Pl.CascXB, GlenStep)) Cu.Top.Add(FVector(X, Pl.CascY, Pl.CascTopZ));
        Cu.Out = Pl.CascFlow;
        Cu.Speed = 140.f;
        Cu.Lip = 12.f;
        Cu.Bow = 12.f;
        Cu.Spread = .06f;
        Cu.BottomZ = CascLowZ - 6.f;
        Cu.FoamTop = .85f;
        Cu.FoamMid = .6f;
        Cu.Depth = .95f;
        Cu.Tear = .25f;
        Cu.Corrugate = .3f;
        Cu.Seed = 6102u;
        Cu.Rows = 12;
        const int32 Tris = EmitCurtain(M, WCascade, Cu);
        St.WaterTris += Tris;
        St.Surfaces += Tris > 0 ? 1 : 0;
    }
}

// ------------------------------------------------------------------ the dam
float DamDeck(const FDamPlan& D, const FVector2D& P) { return FMath::Max(D.DeckZ, GroundAt(P) + DeckLift); }

double ArchSoffit(const FDamPlan& D, double F)
{
    const double Span = D.Runs[1].Len, U = F * Span - Span * .5;
    return D.CrownZ - D.ArchR + FMath::Sqrt(FMath::Max(0.0, D.ArchR * D.ArchR - U * U));
}

/** A low stone kerb closing a small declared step (the dam's run-ons past its ends): its body on the low side, its top 3 cm
 *  over the high ground (flush with a walk there), its face down to 25 cm under the low ground. */
void Kerb(FMeshSet& M, const HouseCalderLandform::FWall& Wl, FStats& St)
{
    const FVector2D Span = Wl.B - Wl.A;
    const double Len = Span.Size();
    if (Len < 5.0) return;
    const FVector2D Dir = Span / Len, Nl = Left(Dir);
    const int32 Cols = FMath::Max(1, FMath::CeilToInt(Len / 25.0));
    double Sum = 0.0;
    for (int32 K = 0; K <= Cols; ++K)
    {
        const FVector2D P = Wl.A + Dir * (Len * K / Cols);
        Sum += GroundAt(P + Nl * 3.0) - GroundAt(P - Nl * 3.0);
    }
    const FVector2D LowSide = Sum > 0.0 ? -Nl : Nl;
    auto HighZ = [&LowSide](const FVector2D& P) { return GroundAt(P - LowSide * 3.0) + 3.f; };
    for (int32 K = 0; K < Cols; ++K)
    {
        const FVector2D A = Wl.A + Dir * (Len * K / Cols), B = Wl.A + Dir * (Len * (K + 1) / Cols);
        const FVector2D A2 = A + LowSide * 30.0, B2 = B + LowSide * 30.0;
        const float Ta = HighZ(A), Tb = HighZ(B);
        const float La = FMath::Min(GroundAt(A2), GroundAt(A + LowSide * 3.0)) - FaceFoot, Lb = FMath::Min(GroundAt(B2), GroundAt(B + LowSide * 3.0)) - FaceFoot;
        { const FVector Q[4] = { Lift(A, Ta), Lift(B, Tb), Lift(B2, Tb), Lift(A2, Ta) }; Poly(M, KCoping, EColl::Walk, Q, 4, FVector::UpVector, nullptr, nullptr, Ao(1.f)); }
        { const FVector Q[4] = { Lift(A2, La), Lift(B2, Lb), Lift(B2, Tb), Lift(A2, Ta) }; const FLinearColor Cq[4] = { Ao(.5f), Ao(.5f), Ao(1.f), Ao(1.f) }; Poly(M, KStone, EColl::Block, Q, 4, FVector(LowSide.X, LowSide.Y, 0.0), nullptr, Cq); }
        if (K == 0) { const FVector Q[4] = { Lift(A, La), Lift(A2, La), Lift(A2, Ta), Lift(A, Ta) }; Poly(M, KStone, EColl::Block, Q, 4, FVector(-Dir.X, -Dir.Y, 0.0), nullptr, nullptr, Ao(.7f)); }
        if (K + 1 == Cols) { const FVector Q[4] = { Lift(B, Lb), Lift(B2, Lb), Lift(B2, Tb), Lift(B, Tb) }; Poly(M, KStone, EColl::Block, Q, 4, FVector(Dir.X, Dir.Y, 0.0), nullptr, nullptr, Ao(.7f)); }
    }
    ++St.Kerbs;
}

/** A landing off a crest end where the dam's line runs on past it (in place of a kerb over the run-on's step): the crest's
 *  width, the run-on's length out from the end, its top straight from the crest's end edge to the ground at its far edge, its
 *  two sides faced down into the ground. It covers the run-on's step and the crest's end lip, so the walk comes onto the crest
 *  with no step to trip on (a step-up is also where a client and its server resolve a walk differently). */
void RunOnLanding(FMeshSet& M, const FDamPlan& D, bool bStart, double Len, FStats& St)
{
    const FSeg& S = bStart ? D.Runs[0] : D.Runs.Last();
    const double Fo = bStart ? 0.0 : 1.0;
    const FVector2D Out = bStart ? -S.Dir : S.Dir;
    const double Hf = D.Half;
    const double Across[7] = { -Hf, -Hf * 2.0 / 3.0, -Hf / 3.0, 0.0, Hf / 3.0, Hf * 2.0 / 3.0, Hf };   // the crest's own columns
    constexpr int32 Along = 5;
    FVector V[7][Along + 1];
    for (int32 I = 0; I < 7; ++I)
    {
        const FVector2D E = S.At(Fo, Across[I]), Far = E + Out * Len;
        const float Ze = DamDeck(D, E), Zf = GroundAt(Far);
        for (int32 J = 0; J <= Along; ++J)
        {
            const double F = double(J) / Along;
            V[I][J] = Lift(E + Out * (Len * F), FMath::Lerp(Ze, Zf, float(F)));
        }
    }
    for (int32 I = 0; I < 6; ++I)
        for (int32 J = 0; J < Along; ++J)
        {
            const FVector Q[4] = { V[I][J], V[I + 1][J], V[I + 1][J + 1], V[I][J + 1] };
            Poly(M, KDeck, EColl::Walk, Q, 4, FVector::UpVector, nullptr, nullptr, Ao(.95f));
        }
    for (const int32 I : { 0, 6 })
    {
        const double Sg = I == 0 ? -1.0 : 1.0;
        const FVector Side(S.Nrm.X * Sg, S.Nrm.Y * Sg, 0.0);
        for (int32 J = 0; J < Along; ++J)
        {
            const FVector2D Pa(V[I][J]), Pb(V[I][J + 1]);
            const float Ga = GroundAt(Pa), Gb = GroundAt(Pb);
            if (V[I][J].Z <= Ga + .5f && V[I][J + 1].Z <= Gb + .5f) continue;
            const FVector Q[4] = { Lift(Pa, Ga - FaceFoot), Lift(Pb, Gb - FaceFoot), V[I][J + 1], V[I][J] };
            const FLinearColor Cq[4] = { Ao(.45f), Ao(.45f), Ao(1.f), Ao(1.f) };
            Poly(M, KStone, EColl::Block, Q, 4, Side, nullptr, Cq);
        }
    }
    ++St.Kerbs;   // the run-on's closure (a landing here, not a kerb)
}

/** Which crest end a run-on wall leaves from: 0 the start, 1 the end, -1 neither (more than 5 cm off both). */
int32 RunOnEnd(const FDamPlan& D, const HouseCalderLandform::FWall& Wl)
{
    const FVector2D E0 = D.Runs[0].At(0.0, 0.0), E1 = D.Runs.Last().At(1.0, 0.0);
    const double D0 = FMath::Min((Wl.A - E0).Size(), (Wl.B - E0).Size()), D1 = FMath::Min((Wl.A - E1).Size(), (Wl.B - E1).Size());
    if (FMath::Min(D0, D1) > 5.0) return -1;
    return D0 <= D1 ? 0 : 1;
}

/** The stone dam: its body under the crest walk (a mossy masonry face down to the outlet pool), the weir notch (its sill, the
 *  spillway face under the weir's sheet), the stone-arch footbridge over it (the soffit, the spandrels, the voussoir ring, a
 *  concrete keystone), the 100 cm parapets with concrete copings and end piers, and a landing over each run-on step. */
void BuildDam(FMeshSet& M, const FPlan& Pl, FStats& St)
{
    const FDamPlan& D = Pl.Dam;
    if (!D.bOn) return;
    const double Hf = D.Half;
    const double Across[7] = { -Hf, -Hf * 2.0 / 3.0, -Hf / 3.0, 0.0, Hf / 3.0, Hf * 2.0 / 3.0, Hf };
    bool bLanding[2] = { false, false };   // a run-on landing covers that end (its end face is not drawn)
    for (const int32 WI : D.RunOns)
    {
        const int32 End = RunOnEnd(D, HouseCalderLandform::Walls()[WI]);
        if (End >= 0) bLanding[End] = true;
    }
    for (int32 R = 0; R < D.Runs.Num(); ++R)
    {
        const FSeg& S = D.Runs[R];
        const bool bArch = R == 1;
        const int32 Cols = FMath::Max(2, FMath::CeilToInt(S.Len / (bArch ? 16.0 : 50.0)));
        for (int32 K = 0; K < Cols; ++K)
        {
            const double F0 = double(K) / Cols, F1 = double(K + 1) / Cols;
            // The crest walk (it rises over the ground where the glen's sides stand higher at the ends).
            for (int32 L = 0; L + 1 < 7; ++L)
            {
                const FVector2D Pa = S.At(F0, Across[L]), Pb = S.At(F1, Across[L]), Pc = S.At(F1, Across[L + 1]), Pd = S.At(F0, Across[L + 1]);
                const FVector Q[4] = { Lift(Pa, DamDeck(D, Pa)), Lift(Pb, DamDeck(D, Pb)), Lift(Pc, DamDeck(D, Pc)), Lift(Pd, DamDeck(D, Pd)) };
                Poly(M, KDeck, EColl::Walk, Q, 4, FVector::UpVector, nullptr, nullptr, Ao(1.f));
            }
            const double Za = bArch ? ArchSoffit(D, F0) : 0.0, Zb = bArch ? ArchSoffit(D, F1) : 0.0;
            for (const double Sg : { -1.0, 1.0 })
            {
                const FVector2D Pa = S.At(F0, Sg * Hf), Pb = S.At(F1, Sg * Hf);
                const FVector Out3(S.Nrm.X * Sg, S.Nrm.Y * Sg, 0.0);
                const float Ga = GroundAt(Pa), Gb = GroundAt(Pb);
                if (bArch)
                {
                    // The spandrel over the arch, the sill's face under the water (downstream: the spillway face), the ring.
                    { const FVector Q[4] = { Lift(Pa, Za), Lift(Pb, Zb), Lift(Pb, DamDeck(D, Pb)), Lift(Pa, DamDeck(D, Pa)) }; Poly(M, KDressed, EColl::Block, Q, 4, Out3, nullptr, nullptr, Ao(.85f)); }
                    { const FVector Q[4] = { Lift(Pa, Ga - FaceFoot), Lift(Pb, Gb - FaceFoot), Lift(Pb, D.SillZ), Lift(Pa, D.SillZ) }; const FLinearColor Cq[4] = { Ao(.4f), Ao(.4f), Ao(.75f), Ao(.75f) }; Poly(M, KStone, EColl::Block, Q, 4, Out3, nullptr, Cq); }
                    const FVector2D Pa3 = S.At(F0, Sg * (Hf + 3.0)), Pb3 = S.At(F1, Sg * (Hf + 3.0));
                    { const FVector Q[4] = { Lift(Pa3, Za), Lift(Pb3, Zb), Lift(Pb3, Zb + 24.0), Lift(Pa3, Za + 24.0) }; Poly(M, KDressed, EColl::Block, Q, 4, Out3, nullptr, nullptr, Ao(.9f)); }
                    { const FVector Q[4] = { Lift(Pa, Za), Lift(Pb, Zb), Lift(Pb3, Zb), Lift(Pa3, Za) }; Poly(M, KDressed, EColl::Block, Q, 4, -FVector::UpVector, nullptr, nullptr, Ao(.6f)); }
                    { const FVector Q[4] = { Lift(Pa, Za + 24.0), Lift(Pb, Zb + 24.0), Lift(Pb3, Zb + 24.0), Lift(Pa3, Za + 24.0) }; Poly(M, KDressed, EColl::Block, Q, 4, FVector::UpVector, nullptr, nullptr, Ao(.9f)); }
                }
                else
                {
                    const float Ta = DamDeck(D, Pa), Tb = DamDeck(D, Pb);
                    if (Ta > Ga + .5f || Tb > Gb + .5f)
                    {
                        const FVector Q[4] = { Lift(Pa, Ga - FaceFoot), Lift(Pb, Gb - FaceFoot), Lift(Pb, Tb), Lift(Pa, Ta) };
                        const FLinearColor Cq[4] = { Ao(.45f), Ao(.45f), Ao(1.f), Ao(1.f) };
                        Poly(M, KStone, EColl::Block, Q, 4, Out3, nullptr, Cq);
                    }
                }
            }
            if (bArch)
            {
                // The soffit (dark under the arch) and the sill's top under the race.
                { const FVector Q[4] = { Lift(S.At(F0, -Hf), Za), Lift(S.At(F1, -Hf), Zb), Lift(S.At(F1, Hf), Zb), Lift(S.At(F0, Hf), Za) }; Poly(M, KDressed, EColl::Block, Q, 4, -FVector::UpVector, nullptr, nullptr, Ao(.4f)); }
                { const FVector Q[4] = { Lift(S.At(F0, -Hf), D.SillZ), Lift(S.At(F1, -Hf), D.SillZ), Lift(S.At(F1, Hf), D.SillZ), Lift(S.At(F0, Hf), D.SillZ) }; Poly(M, KStone, EColl::Walk, Q, 4, FVector::UpVector, nullptr, nullptr, Ao(.6f)); }
            }
        }
        // The abutments facing into the notch (between the sill and the springing), and the dam's two ends.
        if (R == 0 || R == 2)
        {
            const double Fe = R == 0 ? 1.0 : 0.0;
            const FVector Into(S.Dir.X * (R == 0 ? 1.0 : -1.0), S.Dir.Y * (R == 0 ? 1.0 : -1.0), 0.0);
            const FVector Q[4] = { Lift(S.At(Fe, -Hf), D.SillZ - 1.0), Lift(S.At(Fe, Hf), D.SillZ - 1.0), Lift(S.At(Fe, Hf), D.SpringZ), Lift(S.At(Fe, -Hf), D.SpringZ) };
            Poly(M, KDressed, EColl::Block, Q, 4, Into, nullptr, nullptr, Ao(.5f));
            const double Fo = R == 0 ? 0.0 : 1.0;
            for (int32 L = 0; L + 1 < 7 && !bLanding[R == 0 ? 0 : 1]; ++L)
            {
                const FVector2D Pa = S.At(Fo, Across[L]), Pb = S.At(Fo, Across[L + 1]);
                const float Ta = DamDeck(D, Pa), Tb = DamDeck(D, Pb), Ga = GroundAt(Pa), Gb = GroundAt(Pb);
                if (Ta <= Ga + .5f && Tb <= Gb + .5f) continue;
                const FVector Qe[4] = { Lift(Pa, Ga - FaceFoot), Lift(Pb, Gb - FaceFoot), Lift(Pb, Tb), Lift(Pa, Ta) };
                Poly(M, KStone, EColl::Block, Qe, 4, -Into, nullptr, nullptr, Ao(.7f));
            }
        }
    }
    // The keystone on both faces.
    {
        const FSeg& N = D.Runs[1];
        const FFrame F(N.A, N.Dir);
        const double Mid = N.T(.5, 0.0), Zk = ArchSoffit(D, .5);
        for (const double Sg : { -1.0, 1.0 })
            FrameBox(M, KCoping, EColl::Block, F, Mid - 16.0, Mid + 16.0, FMath::Min(Sg * (Hf - 1.0), Sg * (Hf + 6.0)), FMath::Max(Sg * (Hf - 1.0), Sg * (Hf + 6.0)), Zk - 3.0, Zk + 34.0, AllFaces, .8f, 1.f);
    }
    // A trash rack across the notch's upstream face: rusted round bars every 12 cm on two flat bars, from under the sill up into
    // the arch, the race running through it. Under the arch the notch is 63 cm high at its crown: without it, a crawlway from
    // the race onto the weir's 1.4 m drop (MASTER_PROMPT: no gap of 36 cm or more under any wall).
    {
        const FSeg& N = D.Runs[1];
        const double Up = FVector2D::DotProduct(Pl.RaceA - N.At(.5, 0.0), N.Nrm) >= 0.0 ? 1.0 : -1.0;   // the pond's side
        const double Lat = Up * (Hf + 5.0);
        const double A0 = N.T(0.0, Lat), A1 = N.T(1.0, Lat);
        const int32 Bars = FMath::Max(2, FMath::FloorToInt((A1 - A0 - 12.0) / 12.0) + 1);
        for (int32 K = 0; K < Bars; ++K)
        {
            const double F = FMath::Clamp((6.0 + 12.0 * K) / FMath::Max(A1 - A0, 1.0), 0.0, 1.0);
            const FVector2D At = N.At(F, Lat);
            Pipe(M, KRust, EColl::None, Lift(At, D.SillZ - 3.0), Lift(At, ArchSoffit(D, F) + 3.0), 1.6f, 6, false, false, .8f);
            ++St.Bars;
        }
        const FFrame Fr(N.A, N.Dir);
        for (const double Zf : { double(D.SillZ) + 16.0, double(D.SpringZ) + 2.0 })
            FrameBox(M, KRust, EColl::None, Fr, A0, A1, FMath::Min(Lat - 2.5, Lat + 2.5), FMath::Max(Lat - 2.5, Lat + 2.5), Zf - 3.0, Zf + 3.0, AllFaces, .7f, .8f);
        FrameBox(M, -1, EColl::Block, Fr, A0, A1, FMath::Min(Lat - 3.0, Lat + 3.0), FMath::Max(Lat - 3.0, Lat + 3.0), D.SillZ - 10.0, D.CrownZ + 5.0, AllFaces);
    }
    // The parapets: 100 cm over the crest both sides (more where the crest rises at its ends), concrete copings, end piers.
    const double G0 = ParapetEndClear, G1 = D.Total - ParapetEndClear;
    for (const double Sg : { -1.0, 1.0 })
    {
        const double Cin = Sg * ParapetIn, Cout = Sg * Hf;
        float TopEnd[2] = { 0.f, 0.f };
        for (int32 R = 0; R < D.Runs.Num(); ++R)
        {
            const FSeg& S = D.Runs[R];
            const double Fa = FMath::Clamp((G0 - S.G0) / S.Len, 0.0, 1.0), Fb = FMath::Clamp((G1 - S.G0) / S.Len, 0.0, 1.0);
            if (Fb - Fa < 1e-4) continue;
            const int32 Cols = FMath::Max(1, FMath::CeilToInt((Fb - Fa) * S.Len / 50.0));
            for (int32 K = 0; K < Cols; ++K)
            {
                const double F0 = FMath::Lerp(Fa, Fb, double(K) / Cols), F1 = FMath::Lerp(Fa, Fb, double(K + 1) / Cols);
                const FVector2D Ia = S.At(F0, Cin), Ib = S.At(F1, Cin), Oa = S.At(F0, Cout), Ob = S.At(F1, Cout);
                const float Dia = DamDeck(D, Ia), Dib = DamDeck(D, Ib), Doa = DamDeck(D, Oa), Dob = DamDeck(D, Ob);
                const float Top = FMath::Max(D.DeckZ + ParapetRise, FMath::Max(FMath::Max(Dia, Dib), FMath::Max(Doa, Dob)) + 92.f);
                const float Body = Top - CopingThick;
                if (R == 0 && K == 0) TopEnd[0] = Top;
                TopEnd[1] = Top;
                const FVector InwardN(-S.Nrm.X * Sg, -S.Nrm.Y * Sg, 0.0);
                { const FVector Q[4] = { Lift(Ia, Dia), Lift(Ib, Dib), Lift(Ib, Body), Lift(Ia, Body) }; const FLinearColor Cq[4] = { Ao(.6f), Ao(.6f), Ao(1.f), Ao(1.f) }; Poly(M, KStone, EColl::Block, Q, 4, InwardN, nullptr, Cq); }
                { const FVector Q[4] = { Lift(Oa, Doa), Lift(Ob, Dob), Lift(Ob, Body), Lift(Oa, Body) }; Poly(M, KStone, EColl::Block, Q, 4, -InwardN, nullptr, nullptr, Ao(1.f)); }
                const FVector2D Ca = S.At(F0, Cin - Sg * CopingOver), Cb = S.At(F1, Cin - Sg * CopingOver), Cc = S.At(F1, Cout + Sg * CopingOver), Cd = S.At(F0, Cout + Sg * CopingOver);
                const FVector Low[4] = { Lift(Ca, Body), Lift(Cb, Body), Lift(Cc, Body), Lift(Cd, Body) };
                const FVector High[4] = { Lift(Ca, Top), Lift(Cb, Top), Lift(Cc, Top), Lift(Cd, Top) };
                Hexa(M, KCoping, EColl::Walk, Low, High, AllFaces, .75f, 1.f);
                St.ParapetCm += S.Len * (F1 - F0);
            }
        }
        // End piers (square, 10 cm proud of the coping) where each parapet stops short of the crest's ends.
        for (int32 End = 0; End < 2; ++End)
        {
            const double Ge = End == 0 ? G0 : G1;
            for (const FSeg& S : D.Runs)
            {
                if (Ge < S.G0 - .5 || Ge > S.G0 + S.Len + .5) continue;
                const FFrame F(S.A, S.Dir);
                const double T = Ge - S.G0;
                const double L0 = FMath::Min(Cin - Sg * 6.0, Cout + Sg * 3.0), L1 = FMath::Max(Cin - Sg * 6.0, Cout + Sg * 3.0);
                const float Base = DamDeck(D, F.At(T, (L0 + L1) * .5)) - 2.f, PierTop = TopEnd[End] + 12.f;
                FrameBox(M, KDressed, EColl::Walk, F, T - 24.0, T + 24.0, L0, L1, Base, PierTop, SidesOnly, .7f, 1.f);
                FrameBox(M, KCoping, EColl::Walk, F, T - 28.0, T + 28.0, L0 - 4.0, L1 + 4.0, PierTop, PierTop + 8.0, AllFaces, .8f, 1.f);
                break;
            }
        }
        ++St.Parapets;
    }
    for (const int32 WI : D.RunOns)
    {
        const HouseCalderLandform::FWall& Wl = HouseCalderLandform::Walls()[WI];
        const int32 End = RunOnEnd(D, Wl);
        if (End < 0) Kerb(M, Wl, St);
        else RunOnLanding(M, D, End == 0, (Wl.B - Wl.A).Size(), St);
    }
    ++St.Bridges;   // the stone-arch footbridge
}

/** Gander Pond's mortared stone edge at the waterline (5 cm over the water, 45 cm onto the shore), open where Calder Branch
 *  enters, where the race leaves, at the dock and along the dam. */
void BuildPondEdge(FMeshSet& M, const FPlan& Pl, FStats& St)
{
    if (Pl.Pond.Num() < 3) return;
    const double OutSign = Pl.PondArea < 0.0 ? 1.0 : -1.0;   // the left normal points out of a clockwise outline
    const TArray<FSeg> Edges = MitredRuns(Pl.Pond, true);
    const HouseCalderLandform::FPondDock& Dock = HouseCalderLandform::PondDock();
    const float Z = Pl.PondZ + 5.f;
    auto Open = [&Pl, &Dock](const FVector2D& P)
    {
        if (CreekDist(Pl, P) < 520.0 || SegDist(P, Pl.RaceA, Pl.RaceB) < Pl.RaceHalf + 180.0) return true;
        if (SegDist(P, Dock.Land, Dock.Tip) < Dock.Width * .5 + 60.0) return true;
        for (int32 I = 0; I + 1 < Pl.Crest.Num(); ++I) if (SegDist(P, Pl.Crest[I], Pl.Crest[I + 1]) < 300.0) return true;
        return false;
    };
    // Every 50 cm column round the outline (consecutive columns share their mitred edges), open or built.
    struct FEdgeCol { int32 Edge = 0; double F0 = 0.0, F1 = 0.0; bool bOpen = false; };
    TArray<FEdgeCol> Cols;
    for (int32 E = 0; E < Edges.Num(); ++E)
    {
        const int32 Num = FMath::Max(1, FMath::CeilToInt(Edges[E].Len / 50.0));
        for (int32 K = 0; K < Num; ++K)
        {
            FEdgeCol& C = Cols.AddDefaulted_GetRef();
            C.Edge = E;
            C.F0 = double(K) / Num;
            C.F1 = double(K + 1) / Num;
            C.bOpen = Open(Edges[E].At((C.F0 + C.F1) * .5, OutSign * 20.0));
        }
    }
    const double Cw = OutSign * -6.0, Cl = OutSign * 45.0;
    for (int32 I = 0; I < Cols.Num(); ++I)
    {
        const FEdgeCol& C = Cols[I];
        if (C.bOpen) continue;
        const FSeg& S = Edges[C.Edge];
        const double F0 = C.F0, F1 = C.F1;
        const bool bPrevOpen = Cols[(I + Cols.Num() - 1) % Cols.Num()].bOpen, bNextOpen = Cols[(I + 1) % Cols.Num()].bOpen;
        const FVector Out3(S.Nrm.X * OutSign, S.Nrm.Y * OutSign, 0.0);
        const FVector2D Wa = S.At(F0, Cw), Wb = S.At(F1, Cw), La = S.At(F0, Cl), Lb = S.At(F1, Cl);
        const float Za = FMath::Max(Z, GroundAt(La) + .5f), Zb = FMath::Max(Z, GroundAt(Lb) + .5f);
        const float Ba = GroundAt(Wa) - 15.f, Bb = GroundAt(Wb) - 15.f;
        { const FVector Q[4] = { Lift(Wa, Z), Lift(Wb, Z), Lift(Lb, Zb), Lift(La, Za) }; Poly(M, KStone, EColl::Walk, Q, 4, FVector::UpVector, nullptr, nullptr, Ao(1.f)); }
        { const FVector Q[4] = { Lift(Wa, Ba), Lift(Wb, Bb), Lift(Wb, Z), Lift(Wa, Z) }; const FLinearColor Cq[4] = { Ao(.4f), Ao(.4f), Ao(.9f), Ao(.9f) }; Poly(M, KStone, EColl::Block, Q, 4, -Out3, nullptr, Cq); }
        const float Ga = GroundAt(La), Gb = GroundAt(Lb);
        if (Ga < Za - .3f || Gb < Zb - .3f) { const FVector Q[4] = { Lift(La, Ga - 10.f), Lift(Lb, Gb - 10.f), Lift(Lb, Zb), Lift(La, Za) }; Poly(M, KStone, EColl::Block, Q, 4, Out3, nullptr, nullptr, Ao(.8f)); }
        const FVector Along(S.Dir.X, S.Dir.Y, 0.0);
        if (bPrevOpen) { const FVector Q[4] = { Lift(Wa, Ba), Lift(La, Ga - 10.f), Lift(La, Za), Lift(Wa, Z) }; Poly(M, KStone, EColl::Block, Q, 4, -Along, nullptr, nullptr, Ao(.7f)); }
        if (bNextOpen) { const FVector Q[4] = { Lift(Wb, Bb), Lift(Lb, Gb - 10.f), Lift(Lb, Zb), Lift(Wb, Z) }; Poly(M, KStone, EColl::Block, Q, 4, Along, nullptr, nullptr, Ao(.7f)); }
        St.EdgeCm += S.Len * (F1 - F0);
    }
}

// ------------------------------------------------------------------ the causeway
float WayDeck(const FCausewayPlan& W, const FVector2D& P) { return FMath::Max(W.DeckZ, GroundAt(P) + DeckLift); }

/** The loop's stone causeway over Calder Branch: a dry-laid embankment under a gravel deck with stone kerbs, two mossy stone
 *  headwalls (their tops 50 cm over the deck) round a rusted 45 cm pipe with a dark bore, rip-rap at the outfall. */
/** A ramp off a deck's end where the deck stands over the ground there (a lip a walk trips on, and where a client and its server
 *  resolve the step differently): from the deck's end edge (its own N column points E with their deck heights Ze) out along Out,
 *  each column as far as the ground takes to rise to the deck (40 to 200 cm: a landing into the rising bank) or, where it stays
 *  lower, 8 cm for every cm of step (60 to 200 cm), straight to the ground there; its two sides faced down into the ground
 *  (SideN: the side of column 0). False (nothing built) where no column stands 5 mm over the ground. */
bool EndRamp(FMeshSet& M, int32 Slot, const FVector2D* E, const float* Ze, int32 N, const FVector2D& Out, const FVector2D& SideN)
{
    constexpr int32 Along = 6;
    TArray<FVector> V;
    V.SetNum(N * (Along + 1));
    bool bLip = false;
    for (int32 I = 0; I < N; ++I)
    {
        const float Rise = Ze[I] - GroundAt(E[I]);
        bLip |= Rise > .5f;
        double Len = 0.0;
        for (double Sd = 40.0; Sd <= 200.0 && Len <= 0.0; Sd += 10.0)
            if (GroundAt(E[I] + Out * Sd) >= Ze[I] - .5f) Len = Sd;
        if (Len <= 0.0) Len = FMath::Clamp(8.0 * FMath::Max(0.0, double(Rise)), 60.0, 200.0);
        const float Zf = GroundAt(E[I] + Out * Len);
        for (int32 J = 0; J <= Along; ++J)
        {
            const double F = double(J) / Along;
            V[I * (Along + 1) + J] = Lift(E[I] + Out * (Len * F), FMath::Lerp(Ze[I], Zf, float(F)));
        }
    }
    if (!bLip) return false;
    auto Vx = [&V](int32 I, int32 J) -> const FVector& { return V[I * (Along + 1) + J]; };
    for (int32 I = 0; I + 1 < N; ++I)
        for (int32 J = 0; J < Along; ++J)
        {
            const FVector Q[4] = { Vx(I, J), Vx(I + 1, J), Vx(I + 1, J + 1), Vx(I, J + 1) };
            Poly(M, Slot, EColl::Walk, Q, 4, FVector::UpVector, nullptr, nullptr, Ao(.95f));
        }
    for (const int32 I : { 0, N - 1 })
    {
        const FVector2D Sd2 = I == 0 ? SideN : -SideN;
        const FVector Side(Sd2.X, Sd2.Y, 0.0);
        for (int32 J = 0; J < Along; ++J)
        {
            const FVector2D Pa(Vx(I, J)), Pb(Vx(I, J + 1));
            const float Ga = GroundAt(Pa), Gb = GroundAt(Pb);
            if (Vx(I, J).Z <= Ga + .5f && Vx(I, J + 1).Z <= Gb + .5f) continue;
            const FVector Q[4] = { Lift(Pa, Ga - FaceFoot), Lift(Pb, Gb - FaceFoot), Vx(I, J + 1), Vx(I, J) };
            const FLinearColor Cq[4] = { Ao(.45f), Ao(.45f), Ao(1.f), Ao(1.f) };
            Poly(M, KDressed, EColl::Block, Q, 4, Side, nullptr, Cq);
        }
    }
    return true;
}

void BuildCauseway(FMeshSet& M, const FPlan& Pl, FStats& St)
{
    const FCausewayPlan& W = Pl.Way;
    if (!W.bOn) return;
    const double Hf = W.Half;
    const double Across[7] = { -Hf, -Hf * 2.0 / 3.0, -Hf / 3.0, 0.0, Hf / 3.0, Hf * 2.0 / 3.0, Hf };
    auto AtHeadwall = [&W](double G) { return FMath::Abs(G - W.JointG) < HeadwallHalf; };
    for (int32 R = 0; R < W.Runs.Num(); ++R)
    {
        const FSeg& S = W.Runs[R];
        const int32 Cols = FMath::Max(2, FMath::CeilToInt(S.Len / 40.0));
        for (int32 K = 0; K < Cols; ++K)
        {
            const double F0 = double(K) / Cols, F1 = double(K + 1) / Cols;
            const bool bHead = AtHeadwall(S.G0 + S.Len * (F0 + F1) * .5);
            for (int32 L = 0; L + 1 < 7; ++L)
            {
                const FVector2D Pa = S.At(F0, Across[L]), Pb = S.At(F1, Across[L]), Pc = S.At(F1, Across[L + 1]), Pd = S.At(F0, Across[L + 1]);
                const FVector Q[4] = { Lift(Pa, WayDeck(W, Pa)), Lift(Pb, WayDeck(W, Pb)), Lift(Pc, WayDeck(W, Pc)), Lift(Pd, WayDeck(W, Pd)) };
                Poly(M, KGravel, EColl::Walk, Q, 4, FVector::UpVector, nullptr, nullptr, Ao(1.f));
            }
            for (const double Sg : { -1.0, 1.0 })
            {
                const FVector Out3(S.Nrm.X * Sg, S.Nrm.Y * Sg, 0.0);
                const FVector2D Pa = S.At(F0, Sg * Hf), Pb = S.At(F1, Sg * Hf);
                const float Ta = WayDeck(W, Pa), Tb = WayDeck(W, Pb), Ga = GroundAt(Pa), Gb = GroundAt(Pb);
                if (!bHead && (Ta > Ga + .5f || Tb > Gb + .5f))
                {
                    const FVector Q[4] = { Lift(Pa, Ga - FaceFoot), Lift(Pb, Gb - FaceFoot), Lift(Pb, Tb), Lift(Pa, Ta) };
                    const FLinearColor Cq[4] = { Ao(.45f), Ao(.45f), Ao(1.f), Ao(1.f) };
                    Poly(M, KDressed, EColl::Block, Q, 4, Out3, nullptr, Cq);
                }
                if (bHead) continue;
                // The kerb along the edge.
                const FVector2D Ia = S.At(F0, Sg * (Hf - KerbWidth)), Ib = S.At(F1, Sg * (Hf - KerbWidth));
                const float Ka = FMath::Max(Ta, WayDeck(W, Ia)) + KerbRise, Kb = FMath::Max(Tb, WayDeck(W, Ib)) + KerbRise;
                { const FVector Q[4] = { Lift(Ia, Ka), Lift(Ib, Kb), Lift(Pb, Kb), Lift(Pa, Ka) }; Poly(M, KCoping, EColl::Walk, Q, 4, FVector::UpVector, nullptr, nullptr, Ao(1.f)); }
                { const FVector Q[4] = { Lift(Ia, WayDeck(W, Ia) - 1.f), Lift(Ib, WayDeck(W, Ib) - 1.f), Lift(Ib, Kb), Lift(Ia, Ka) }; Poly(M, KDressed, EColl::Block, Q, 4, -Out3, nullptr, nullptr, Ao(.85f)); }
                { const FVector Q[4] = { Lift(Pa, Ta - 1.f), Lift(Pb, Tb - 1.f), Lift(Pb, Kb), Lift(Pa, Ka) }; Poly(M, KDressed, EColl::Block, Q, 4, Out3, nullptr, nullptr, Ao(1.f)); }
                // The kerb's ends at the headwalls and at the causeway's ends.
                const bool bEndA = (R == 0 && K == 0) || AtHeadwall(S.G0 + S.Len * F0 - 1.0), bEndB = (R + 1 == W.Runs.Num() && K + 1 == Cols) || AtHeadwall(S.G0 + S.Len * F1 + 1.0);
                const FVector Along(S.Dir.X, S.Dir.Y, 0.0);
                if (bEndA) { const FVector Q[4] = { Lift(Ia, Ta - 1.f), Lift(Pa, Ta - 1.f), Lift(Pa, Ka), Lift(Ia, Ka) }; Poly(M, KDressed, EColl::Block, Q, 4, -Along, nullptr, nullptr, Ao(.85f)); }
                if (bEndB) { const FVector Q[4] = { Lift(Ib, Tb - 1.f), Lift(Pb, Tb - 1.f), Lift(Pb, Kb), Lift(Ib, Kb) }; Poly(M, KDressed, EColl::Block, Q, 4, Along, nullptr, nullptr, Ao(.85f)); }
            }
        }
        // The causeway's own ends: where it stands over the ground, a ramp off it to the ground (its end face under the ramp).
        if (R == 0 || R + 1 == W.Runs.Num())
            for (const double Fe : { 0.0, 1.0 })
            {
                if ((Fe < .5 && R != 0) || (Fe > .5 && R + 1 != W.Runs.Num())) continue;
                const FVector2D Away2 = Fe < .5 ? -S.Dir : S.Dir;
                FVector2D Edge[7];
                float EdgeZ[7];
                for (int32 L = 0; L < 7; ++L) { Edge[L] = S.At(Fe, Across[L]); EdgeZ[L] = WayDeck(W, Edge[L]); }
                if (EndRamp(M, KGravel, Edge, EdgeZ, 7, Away2, -S.Nrm)) ++St.Kerbs;
            }
    }
    // The headwalls (along the joint's direction, 15 cm proud of the faces) with the pipe's mouth through each.
    const FFrame F(W.Joint, W.JointDir);
    const float Zc = W.InvertZ + W.PipeR, Hole = W.PipeR + 3.f;
    float Bottom = FLT_MAX;
    for (const double Sg : { -1.0, 1.0 })
        for (double A = -HeadwallHalf; A <= HeadwallHalf + .1; A += 25.0)
            for (const double Off : { -30.0, 15.0 }) Bottom = FMath::Min(Bottom, GroundAt(F.At(A, Sg * (W.JointFace + Off))));
    Bottom = FMath::Min(Bottom - FaceFoot, Zc - Hole - 20.f);
    const float HeadTop = W.DeckZ + 40.f;
    // The square round the hole the face's ring of quads reaches (inside the face, clear of its bottom and top).
    const float Square = FMath::Max(Hole + 8.f, FMath::Min(60.f, FMath::Min(Zc - Bottom - 5.f, HeadTop - Zc - 5.f)));
    for (const double Sg : { -1.0, 1.0 })
    {
        const double C0 = Sg * (W.JointFace - 30.0), C1 = Sg * (W.JointFace + 15.0);
        const double Lo = FMath::Min(C0, C1), Hi = FMath::Max(C0, C1);
        const FVector OutN(F.S.X * Sg, F.S.Y * Sg, 0.0);
        // The box but its outer face (which has the hole): the inner face over the deck, the ends, the top (under the coping).
        FrameBox(M, KStone, EColl::Block, F, -HeadwallHalf, HeadwallHalf, Lo, Hi, Bottom, HeadTop, Sg > 0.0 ? (4 << 0) | (4 << 1) | (4 << 3) : (4 << 1) | (4 << 2) | (4 << 3), .5f, 1.f);
        FrameBox(M, KCoping, EColl::Walk, F, -HeadwallHalf - 4.0, HeadwallHalf + 4.0, Lo - 4.0, Hi + 4.0, HeadTop, HeadTop + 10.0, AllFaces, .8f, 1.f);
        // The outer face: a ring of quads from the hole to a square round it, and the rest of the face round the square.
        const double Face = C1;
        auto FP = [&F, Face](double A, double Z) { const FVector2D P = F.At(A, Face); return FVector(P.X, P.Y, Z); };
        constexpr int32 Seg = 16;
        for (int32 K = 0; K < Seg; ++K)
        {
            const double A0 = 2.0 * UE_DOUBLE_PI * K / Seg, A1 = 2.0 * UE_DOUBLE_PI * (K + 1) / Seg;
            auto OnSquare = [Square](double Ang) { const double Cx = FMath::Cos(Ang), Cz = FMath::Sin(Ang); const double M2 = FMath::Max(FMath::Abs(Cx), FMath::Abs(Cz)); return FVector2D(Cx / M2 * Square, Cz / M2 * Square); };
            const FVector2D S0 = OnSquare(A0), S1 = OnSquare(A1);
            const FVector Q[4] = { FP(Hole * FMath::Cos(A0), Zc + Hole * FMath::Sin(A0)), FP(Hole * FMath::Cos(A1), Zc + Hole * FMath::Sin(A1)), FP(S1.X, Zc + S1.Y), FP(S0.X, Zc + S0.Y) };
            Poly(M, KStone, EColl::None, Q, 4, OutN, nullptr, nullptr, Ao(.8f));
            // (a square's corner falls between two ring points: its triangle)
            for (const double Corner : { .25, .75, 1.25, 1.75 })
            {
                const double Ac = Corner * UE_DOUBLE_PI;
                if (Ac > A0 + 1e-6 && Ac < A1 - 1e-6)
                {
                    const FVector2D Sc = OnSquare(Ac);
                    const FVector T3[3] = { FP(Hole * FMath::Cos(A1), Zc + Hole * FMath::Sin(A1)), FP(Sc.X, Zc + Sc.Y), FP(S1.X, Zc + S1.Y) };
                    Poly(M, KStone, EColl::None, T3, 3, OutN, nullptr, nullptr, Ao(.8f));
                    const FVector T4[3] = { FP(Hole * FMath::Cos(A1), Zc + Hole * FMath::Sin(A1)), FP(S0.X, Zc + S0.Y), FP(Sc.X, Zc + Sc.Y) };
                    Poly(M, KStone, EColl::None, T4, 3, OutN, nullptr, nullptr, Ao(.8f));
                }
            }
        }
        const FVector Rest[4][4] = {
            { FP(-HeadwallHalf, Bottom), FP(-Square, Bottom), FP(-Square, HeadTop), FP(-HeadwallHalf, HeadTop) },
            { FP(Square, Bottom), FP(HeadwallHalf, Bottom), FP(HeadwallHalf, HeadTop), FP(Square, HeadTop) },
            { FP(-Square, Bottom), FP(Square, Bottom), FP(Square, Zc - Square), FP(-Square, Zc - Square) },
            { FP(-Square, Zc + Square), FP(Square, Zc + Square), FP(Square, HeadTop), FP(-Square, HeadTop) } };
        for (const auto& Q : Rest) Poly(M, KStone, EColl::None, Q, 4, OutN, nullptr, nullptr, Ao(.85f));
        // Its collision: one plain face (nobody fits a 45 cm pipe).
        const FVector Cq[4] = { FP(-HeadwallHalf, Bottom), FP(HeadwallHalf, Bottom), FP(HeadwallHalf, HeadTop), FP(-HeadwallHalf, HeadTop) };
        Poly(M, -1, EColl::Block, Cq, 4, OutN);
        // The pipe's rusted stub (6 cm proud) and its lip.
        const FVector Mouth = FP(0.0, Zc), StubEnd = Mouth + OutN * 6.0;
        Pipe(M, KRust, EColl::None, Mouth, StubEnd, Hole, 16, false, false, .7f);
        for (int32 K = 0; K < Seg; ++K)
        {
            const double A0 = 2.0 * UE_DOUBLE_PI * K / Seg, A1 = 2.0 * UE_DOUBLE_PI * (K + 1) / Seg;
            const FVector Ax = FVector(F.U.X, F.U.Y, 0.0), Up = FVector::UpVector;
            const FVector Q[4] = { StubEnd + (Ax * FMath::Cos(A0) + Up * FMath::Sin(A0)) * W.PipeR, StubEnd + (Ax * FMath::Cos(A1) + Up * FMath::Sin(A1)) * W.PipeR,
                StubEnd + (Ax * FMath::Cos(A1) + Up * FMath::Sin(A1)) * Hole, StubEnd + (Ax * FMath::Cos(A0) + Up * FMath::Sin(A0)) * Hole };
            Poly(M, KRust, EColl::None, Q, 4, OutN, nullptr, nullptr, Ao(.8f));
        }
    }
    // The bore: dark, right through, the creek's water in its bottom.
    {
        const FVector2D Pn = F.At(0.0, -(W.JointFace + 21.0)), Ps = F.At(0.0, W.JointFace + 21.0);
        Pipe(M, KDark, EColl::None, Lift(Pn, Zc), Lift(Ps, Zc), W.PipeR, 16, false, false, .1f, true);
    }
    // Rip-rap at the outfall (the downstream face: the creek runs south).
    const double Down = F.S.Y < 0.0 ? 1.0 : -1.0;
    for (int32 K = 0; K < 9; ++K)
    {
        const double A = -170.0 + 340.0 * Hash01(K, 1, 7711u), Off = 30.0 + 160.0 * Hash01(K, 2, 7711u);
        if (FMath::Abs(A) < 45.0 && Off < 140.0) continue;   // the pipe's outflow stays clear
        const FVector2D C = F.At(A, Down * (W.JointFace + 15.0 + Off));
        const float Rad = 15.f + 14.f * Hash01(K, 3, 7711u);
        Boulder(M, KBoulder, C, Rad, GroundAt(C) + Rad * (.45f + .2f * Hash01(K, 4, 7711u)), 360.f * Hash01(K, 5, 7711u), 7800u + K, St);
        ++St.Riprap;
    }
    ++St.Bridges;   // the causeway (the pipe culvert in its stone headwalls)
}

// ------------------------------------------------------------------ the dock
/** The weathered timber fishing dock: plank deck, stringers and pile caps on piles set in the bed, a low curb where it is
 *  rail-less (the water under it no deeper than 60 cm there), a 100 cm timber rail both sides and across its end where the
 *  drop reaches 75 cm. One plank is gone. */
void BuildDock(FMeshSet& M, const FPlan& Pl, FStats& St)
{
    const HouseCalderLandform::FPondDock& D = HouseCalderLandform::PondDock();
    const FVector2D Span = D.Tip - D.Land;
    const double Len = Span.Size();
    if (Len < 100.0) return;
    const FFrame F(D.Land, Span);
    const double Hf = D.Width * .5, EdgeS = FVector2D::DotProduct(D.Edge - D.Land, F.U);
    double RailS = EdgeS + D.RailFromEdge;
    for (double S = 0.0; S <= Len; S += 10.0)
    {
        float Low = FLT_MAX;
        for (const double L : { -Hf - 30.0, -Hf + 10.0, Hf - 10.0, Hf + 30.0 }) Low = FMath::Min(Low, GroundAt(F.At(S, L)));
        if (D.DeckZ - Low >= 70.f) { RailS = FMath::Min(RailS, S); break; }
    }
    RailS = FMath::Clamp(RailS, 60.0, Len - 40.0);
    const float Z = D.DeckZ;
    // The planks (14.5 cm on a 16 cm pitch; each a hair low by its own amount, worn), one missing over the water.
    const int32 Missing = FMath::FloorToInt((EdgeS + 120.0) / 16.0);
    for (int32 K = 0; K * 16.0 + 14.5 <= Len + .1; ++K)
    {
        if (K == Missing && K * 16.0 < RailS - 40.0) continue;
        const float Sink = .6f * Hash01(K, 3, 5150u);
        FrameBox(M, KPlank, EColl::None, F, K * 16.0, K * 16.0 + 14.5, -Hf, Hf, Z - 5.0 - Sink, Z - Sink, AllFaces, .6f, 1.f);
        ++St.Planks;
    }
    for (const double L : { -70.0, 70.0 }) FrameBox(M, KTimber, EColl::None, F, 0.0, Len, L - 6.0, L + 6.0, Z - 27.0, Z - 5.0, AllFaces, .45f, .7f);
    // Piles and their caps; those at and past the rail's start rise on as its posts.
    TArray<double> Stations = { 35.0, FMath::Max(80.0, EdgeS - 40.0), EdgeS + 90.0, RailS, Len - 8.0 };
    Stations.Sort();
    double PrevS = -1e9;
    for (const double S : Stations)
    {
        if (S - PrevS < 50.0 || S < 0.0 || S > Len) continue;
        PrevS = S;
        FrameBox(M, KTimber, EColl::None, F, S - 8.0, S + 8.0, -Hf - 6.0, Hf + 6.0, Z - 42.0, Z - 27.0, AllFaces, .45f, .7f);
        for (const double L : { -(Hf - 8.0), Hf - 8.0 })
        {
            const FVector2D P = F.At(S, L);
            const float Base = GroundAt(P) - FootSink;
            const bool bPost = S >= RailS - 1.0;
            const float PileTop = bPost ? Z + RailRise : Z - 5.f;
            if (PileTop <= Base + 5.f) continue;
            Pipe(M, KTimber, EColl::None, Lift(P, Base), Lift(P, PileTop), 9.f, 8, false, true, .7f);
            Foot(P, Base, St);
            if (Z - 5.f > Base + 5.f) FrameBox(M, -1, EColl::Block, F, S - 9.0, S + 9.0, L - 9.0, L + 9.0, Base, Z - 5.0, SidesOnly);
        }
    }
    // The deck's walking surface (one face) and the low curbs where it is rail-less.
    FrameBox(M, -1, EColl::Walk, F, 0.0, Len, -Hf, Hf, Z - 1.0, Z, 1);
    FrameBox(M, -1, EColl::Block, F, 0.0, Len, -Hf - 6.0, Hf + 6.0, Z - 42.0, Z - 1.0, SidesOnly | 2);   // its underside
    for (const double Sg : { -1.0, 1.0 })
        FrameBox(M, KTimber, EColl::Walk, F, 0.0, RailS, FMath::Min(Sg * (Hf - 10.0), Sg * Hf), FMath::Max(Sg * (Hf - 10.0), Sg * Hf), Z - .5, Z + 9.0, AllFaces, .6f, .9f);
    // The rail: top and mid rails between the posts, both sides and across the end.
    const double EndS = Len - 8.0;
    for (const double Sg : { -1.0, 1.0 })
    {
        const double L = Sg * (Hf - 8.0);
        for (const float H : { RailRise - 4.f, RailRise * .5f })
            Beam(M, KTimber, EColl::None, Lift(F.At(RailS, L), Z + H), Lift(F.At(EndS, L), Z + H), 9.0, 8.0, F.S, .8f);
        FrameBox(M, -1, EColl::Block, F, RailS, EndS + 5.0, FMath::Min(L - Sg * 5.0, L + Sg * 5.0), FMath::Max(L - Sg * 5.0, L + Sg * 5.0), Z, Z + RailRise, AllFaces);
        St.RailCm += EndS - RailS;
        ++St.Rails;
    }
    for (const float H : { RailRise - 4.f, RailRise * .5f })
        Beam(M, KTimber, EColl::None, Lift(F.At(EndS, -(Hf - 8.0)), Z + H), Lift(F.At(EndS, Hf - 8.0), Z + H), 9.0, 8.0, F.U, .8f);
    FrameBox(M, -1, EColl::Block, F, EndS - 5.0, EndS + 5.0, -Hf + 3.0, Hf - 3.0, Z, Z + RailRise, AllFaces);
    St.RailCm += 2.0 * (Hf - 8.0);
    ++St.Rails;
    UE_LOG(LogTemp, Display, TEXT("CALDER_WATER_DOCK len=%.0f railFrom=%.0f (data %.0f) deck=%.1f tipDepth=%.1f"), Len, RailS, EdgeS + D.RailFromEdge, Z, D.TipDepth);
    (void)Pl;
}

// ------------------------------------------------------------------ the glen: caprock, rock house rail, footbridge, headwall
/** The caprock's top at X (the overhang's walking surface): the ground just above the falls' line, 3.5 cm up (over the
 *  features' rock lip); under the lip's water 6 cm below the brink; a raised lip just outside it that hides the water's sides. */
float OverTop(const FPlan& Pl, double X)
{
    const float Base = GroundAt(FVector2D(X, Pl.FaceY + 3.0)) + 3.5f;
    if (X >= Pl.FallsXA + 12.0 && X <= Pl.FallsXB - 12.0) return FMath::Min(Base, Pl.FallsTopZ - 6.f);
    if (X >= Pl.FallsXA - 40.0 && X <= Pl.FallsXB + 40.0) return FMath::Max(Base, Pl.FallsTopZ + 3.f);
    return Base;
}

/** The sandstone caprock over the rock house: from 40 cm behind the falls' line out to the overhang's edge (3 m in the middle,
 *  tapering at its ends), its brink rounded, a drip edge under its front, its underside rising back to the shale face. Its top
 *  is walkable; its faces and underside are Unwalkable rock. */
void BuildOverhang(FMeshSet& M, const FPlan& Pl, FStats& St)
{
    const HouseCalderLandform::FRockHouse& H = HouseCalderLandform::RockHouse();
    if (H.X1 - H.X0 < 200.f) return;
    const double Proj0 = Pl.FaceY - Pl.CurtainY;
    TArray<double> Xs = Steps(H.X0, H.X1, 50.0);
    for (const double X : { Pl.FallsXA - 40.0, Pl.FallsXA, Pl.FallsXA + 12.0, Pl.FallsXB - 12.0, Pl.FallsXB, Pl.FallsXB + 40.0 }) if (X > H.X0 && X < H.X1) Xs.Add(X);
    Xs.Sort();
    for (int32 K = Xs.Num() - 1; K > 0; --K) if (Xs[K] - Xs[K - 1] < 2.0) Xs.RemoveAt(K);
    constexpr int32 NP = 10;
    TArray<FVector> V;
    TArray<float> A;
    V.SetNum(Xs.Num() * NP);
    A.SetNum(Xs.Num() * NP);
    const float AoP[NP] = { 1.f, 1.f, .95f, .8f, .7f, .55f, .35f, .3f, .3f, .3f };
    for (int32 I = 0; I < Xs.Num(); ++I)
    {
        const double X = Xs[I];
        const bool bLip = X >= Pl.FallsXA - 60.0 && X <= Pl.FallsXB + 60.0;
        double Proj = Proj0 * (.3 + .7 * Smooth01(FMath::Min(X - H.X0, H.X1 - X) / 400.0));
        if (!bLip) Proj += 22.0 * Value3(FVector(X, 0.0, 0.0), 160.f, 9401u);
        const double Front = Pl.FaceY - Proj, Back = Pl.FaceY + 40.0;
        const float Top = OverTop(Pl, X);
        const double Prof[NP][2] = {
            { Back, Top }, { Front + 30.0, Top - 2.0 }, { Front + 8.0, Top - 9.0 }, { Front, Top - 26.0 }, { Front - 4.0, Top - 95.0 },
            { Front + 6.0, Top - 170.0 }, { Front + 55.0, Top - 158.0 }, { (Front + Pl.FaceY) * .5, Top - 140.0 }, { Pl.FaceY, Top - 122.0 }, { Back, Top - 122.0 } };
        for (int32 P = 0; P < NP; ++P)
        {
            double Y = Prof[P][0], Z = Prof[P][1];
            const float N = Value3(FVector(X, Y, Z), 90.f, 9402u + P);
            if (P >= 3 && P <= 5 && !(bLip && P == 3)) Y -= 9.0 * N;     // the front: weathered in and out
            if (P >= 6 && P <= 8) Z -= 12.0 * N;                         // the underside: hanging lumps
            V[I * NP + P] = FVector(X, Y, Z);
            A[I * NP + P] = AoP[P] * (.85f + .15f * N);
        }
    }
    for (int32 I = 0; I + 1 < Xs.Num(); ++I)
        for (int32 P = 0; P < NP; ++P)
        {
            const int32 Q0 = P, Q1 = (P + 1) % NP;
            const FVector Q[4] = { V[I * NP + Q0], V[(I + 1) * NP + Q0], V[(I + 1) * NP + Q1], V[I * NP + Q1] };
            const FLinearColor Cq[4] = { Ao(A[I * NP + Q0]), Ao(A[(I + 1) * NP + Q0]), Ao(A[(I + 1) * NP + Q1]), Ao(A[I * NP + Q1]) };
            // Outward: up for the top, toward -Y for the front, down for the underside, +Y for the buried back.
            const FVector Out = P <= 1 ? FVector::UpVector : (P <= 4 ? FVector(0.0, -1.0, .2) : (P <= 7 ? -FVector::UpVector : (P == 8 ? -FVector::UpVector : FVector(0.0, 1.0, 0.0))));
            Poly(M, KSandstone, P <= 1 ? EColl::Walk : EColl::Block, Q, 4, Out, nullptr, Cq);
        }
    // The two ends: fans from the profile's centre.
    for (const int32 I : { 0, Xs.Num() - 1 })
    {
        FVector C(0.0);
        for (int32 P = 0; P < NP; ++P) C += V[I * NP + P];
        C /= double(NP);
        const FVector Out(I == 0 ? -1.0 : 1.0, 0.0, 0.0);
        for (int32 P = 0; P < NP; ++P)
        {
            const FVector T3[3] = { C, V[I * NP + P], V[I * NP + (P + 1) % NP] };
            Poly(M, KSandstone, EColl::Block, T3, 3, Out, nullptr, nullptr, Ao(.6f));
        }
    }
    (void)St;
}

/** The rock house's rail: a rusted 100 cm pipe rail (posts, top and mid rails) on the pool side of the path behind the falls, a
 *  little out on the shelf's edge so the path keeps its width by the shale face. */
void BuildRockRail(FMeshSet& M, const FPlan& Pl, FStats& St)
{
    const HouseCalderLandform::FRockHouse& H = HouseCalderLandform::RockHouse();
    const double Y = H.RailA.Y - 12.0, X0 = FMath::Min(H.RailA.X, H.RailB.X), X1 = FMath::Max(H.RailA.X, H.RailB.X);
    if (X1 - X0 < 50.0) return;
    const int32 Gaps = FMath::Max(1, FMath::CeilToInt((X1 - X0) / 150.0));
    TArray<FVector> Bases;
    for (int32 K = 0; K <= Gaps; ++K)
    {
        const FVector2D P(FMath::Lerp(X0, X1, double(K) / Gaps), Y);
        Bases.Add(Lift(P, GroundAt(P)));
    }
    for (int32 K = 0; K < Bases.Num(); ++K)
    {
        const FVector2D P(Bases[K].X, Bases[K].Y);
        Pipe(M, KRust, EColl::None, Bases[K] - FVector(0, 0, FootSink), Bases[K] + FVector(0, 0, RailRise + 2.0), 2.6f, 8, false, true, .8f);
        Foot(P, float(Bases[K].Z) - FootSink, St);
    }
    for (int32 K = 0; K + 1 < Bases.Num(); ++K)
    {
        const bool bFirst = K == 0, bLast = K + 2 == Bases.Num();
        Pipe(M, KRust, EColl::None, Bases[K] + FVector(0, 0, RailRise), Bases[K + 1] + FVector(0, 0, RailRise), 2.2f, 8, bFirst, bLast, .9f);
        Pipe(M, KRust, EColl::None, Bases[K] + FVector(0, 0, RailRise * .5), Bases[K + 1] + FVector(0, 0, RailRise * .5), 1.8f, 8, bFirst, bLast, .8f);
        const FVector Lo0 = Bases[K], Lo1 = Bases[K + 1];
        const FVector LowQ[4] = { Lo0 + FVector(0, -3, -FootSink), Lo1 + FVector(0, -3, -FootSink), Lo1 + FVector(0, 3, -FootSink), Lo0 + FVector(0, 3, -FootSink) };
        const FVector HighQ[4] = { Lo0 + FVector(0, -3, RailRise), Lo1 + FVector(0, -3, RailRise), Lo1 + FVector(0, 3, RailRise), Lo0 + FVector(0, 3, RailRise) };
        Hexa(M, -1, EColl::Block, LowQ, HighQ, AllFaces);
        St.RailCm += (Lo1 - Lo0).Size2D();
    }
    ++St.Rails;
    (void)Pl;
}

/** Ashby Lane's concrete arch and stone parapets. The creek bed remains below the independent deck. */
const HouseCalderLandform::FWaterCrossing* RoadBridge()
{
    for (const HouseCalderLandform::FWaterCrossing& Crossing : HouseCalderLandform::WaterCrossings())
        if (Crossing.Kind == HouseCalderLandform::EWaterCrossing::RoadBridge) return &Crossing;
    return nullptr;
}
float RoadBridgeVergeZ(const HouseCalderLandform::FWaterCrossing& Crossing)
{
    return Crossing.DeckZ - .02f * (Crossing.Width * .5f - 45.f) + 12.f;
}
float RoadBridgeTop(const HouseCalderLandform::FWaterCrossing& Crossing, double Along, double Across, bool bVerge)
{
    const float Nominal=bVerge?RoadBridgeVergeZ(Crossing):Crossing.DeckZ-float(.02*FMath::Abs(Across));
    const FFrame Frame(Crossing.Centre,Crossing.Axis);
    float Result=Nominal;
    // Blend the deck into the actual baked approach datums over 3 m. Points
    // exactly on a retaining line can use its other sector; the deck must meet
    // the paving that was actually built there, including that small variation.
    for(const double End:{-double(Crossing.Back),double(Crossing.On)})
    {
        const double Weight=1.-Smooth01(FMath::Abs(Along-End)/300.);
        if(Weight<=0.) continue;
        float Target=0.f;
        if(bVerge)
        {
            Target=-MAX_flt;
            for(const double Edge:{End-3.,End,End+3.})
                for(const double L:{Crossing.Width*.5-45.,Crossing.Width*.5-22.5,Crossing.Width*.5})
                    Target=FMath::Max(Target,GroundAt(Frame.At(Edge,Across<0.?-L:L)));
        }
        else
        {
            const FVector2D P=Frame.At(End,Across);
            Target=HouseCalderTerrain::SurfaceZ(P.X,P.Y,HouseCalderTerrain::EPave::Road);
        }
        Result+=float(Weight)*(Target-Nominal);
    }
    return Result;
}
float RoadBridgeBottom(const HouseCalderLandform::FWaterCrossing& Crossing, const FFrame& Frame, double Along, double Across, float Top)
{
    const double Middle = (Crossing.On - Crossing.Back) * .5, Half = (Crossing.On + Crossing.Back) * .5;
    const double T = (Along - Middle) / Half;
    const float Nominal = Crossing.DeckZ - 35.f - float(300.0 * T * T) - float(.02 * FMath::Min(FMath::Abs(Across), Crossing.Width * .5 - 45.0));
    const FVector2D P = Frame.At(Along, Across);
    const float Surface = HouseCalderTerrain::SurfaceZ(float(P.X), float(P.Y));
    // Buried parts stop 8 cm inside their supporting surface; the visible
    // soffit follows the concrete arch and never fills the creek opening.
    return FMath::Min(Top - FootSink, FMath::Max(Nominal, Surface - FootSink));
}
void BuildRoadBridge(FMeshSet& M, FStats& St)
{
    const HouseCalderLandform::FWaterCrossing* Crossing = RoadBridge();
    if (!Crossing) return;
    const FFrame Frame(Crossing->Centre, Crossing->Axis);
    const double Start = -Crossing->Back, End = Crossing->On, HalfClear = Crossing->Width * .5;
    const double PavedHalf = HalfClear - 45.0, Outer = HalfClear + 40.0;
    const double Widths[] = { -Outer, -HalfClear, -PavedHalf, 0.0, PavedHalf, HalfClear, Outer };
    const int32 Segments = FMath::Max(1, FMath::CeilToInt((End - Start) / 50.0));
    for (int32 K = 0; K < Segments; ++K)
    {
        const double A0 = FMath::Lerp(Start, End, double(K) / Segments), A1 = FMath::Lerp(Start, End, double(K + 1) / Segments);
        for (int32 Strip = 0; Strip < 6; ++Strip)
        {
            const double L0 = Widths[Strip], L1 = Widths[Strip + 1];
            const bool bVerge = FMath::Abs((L0 + L1) * .5) >= PavedHalf;
            const FVector2D Points[4] = { Frame.At(A0,L0), Frame.At(A1,L0), Frame.At(A1,L1), Frame.At(A0,L1) };
            const double Along[4] = { A0,A1,A1,A0 }, Across[4] = { L0,L0,L1,L1 };
            FVector Low[4], High[4], BlockHigh[4];
            for (int32 V = 0; V < 4; ++V)
            {
                const float Top = RoadBridgeTop(*Crossing,Along[V],Across[V],bVerge);
                High[V] = Lift(Points[V], Top);
                Low[V] = Lift(Points[V], RoadBridgeBottom(*Crossing, Frame, Along[V], Across[V], Top));
                BlockHigh[V] = High[V] - FVector(0,0,1);
            }
            Hexa(M,KDeck,EColl::None,Low,High,AllFaces,.65f,1.f);
            Poly(M,-1,EColl::Walk,High,4,FVector::UpVector);
            Hexa(M,-1,EColl::Block,Low,BlockHigh,SidesOnly | 2);
        }
    }
    for (const double Side : { -1.0, 1.0 })
    {
        const double L0 = Side < 0 ? -Outer : HalfClear, L1 = Side < 0 ? -HalfClear : Outer;
        for(int32 K=0;K<Segments;++K)
        {
            const double A0=FMath::Lerp(Start,End,double(K)/Segments),A1=FMath::Lerp(Start,End,double(K+1)/Segments);
            const double Along[4]={A0,A1,A1,A0},Across[4]={L0,L0,L1,L1};
            FVector Low[4],High[4],CopingLow[4],CopingHigh[4],BlockHigh[4];
            for(int32 V=0;V<4;++V)
            {
                const float Base=RoadBridgeTop(*Crossing,Along[V],Side*HalfClear,true),Top=Base+107.f;
                Low[V]=Lift(Frame.At(Along[V],Across[V]),Base-1.f);
                High[V]=Lift(Frame.At(Along[V],Across[V]),Top-CopingThick);
                const double OverAcross=Across[V]+((V<2)?-CopingOver:CopingOver);
                CopingLow[V]=Lift(Frame.At(Along[V],OverAcross),Top-CopingThick);
                CopingHigh[V]=Lift(Frame.At(Along[V],OverAcross),Top);
                BlockHigh[V]=CopingHigh[V]-FVector(0,0,1);
            }
            Hexa(M,KStone,EColl::None,Low,High,AllFaces);
            Hexa(M,KCoping,EColl::None,CopingLow,CopingHigh,AllFaces);
            Hexa(M,-1,EColl::Block,Low,BlockHigh,SidesOnly|2);
            Poly(M,-1,EColl::Walk,CopingHigh,4,FVector::UpVector);
        }
        ++St.Parapets;
        St.ParapetCm += End - Start;
    }
    for (const double Along : { Start,End })
        for (const double Across : { -220.0,0.0,220.0 })
        {
            const FVector2D P = Frame.At(Along,Across);
            const float Support=HouseCalderTerrain::SurfaceZ(P.X,P.Y,HouseCalderTerrain::EPave::Road);
            Foot(P,RoadBridgeBottom(*Crossing,Frame,Along,Across,RoadBridgeTop(*Crossing,Along,Across,false)),St,Support);
        }
    ++St.Bridges;
    ++St.RoadBridges;
    St.Notes.Add(FString::Printf(TEXT("CALDER_ROAD_BRIDGE built span=%.0f width=%.0f deck=%.1f archCrown=%.1f parapet=107"),
        End - Start,Crossing->Width,Crossing->DeckZ,Crossing->DeckZ - 35.f));
}

/** The timber pony-truss footbridge below the falls (P6). */
void BuildTimberBridge(FMeshSet& M, const FPlan& Pl, FStats& St)
{
    const HouseCalderLandform::FWaterCrossing* X = nullptr;
    for (const HouseCalderLandform::FWaterCrossing& C : HouseCalderLandform::WaterCrossings()) if (C.Kind == HouseCalderLandform::EWaterCrossing::TimberFootbridge) X = &C;
    if (!X) return;
    const FFrame F(X->Centre, X->Axis);
    const double S0 = -X->Back, S1 = X->On, Hf = X->Width * .5;
    const float Z = X->DeckZ;
    constexpr double TrussL = 86.0, PlankHalf = 92.0;
    // The trusses: from 40 cm in at each end, held 80 cm clear of the shale face (Pl.FaceY) at the north end.
    double T0 = S0 + 40.0, T1 = S1 - 40.0;
    for (const double Sg : { -1.0, 1.0 })
    {
        const FVector2D A = F.At(0.0, Sg * (TrussL + 7.0)), B = F.At(1.0, Sg * (TrussL + 7.0));
        const double Dy = B.Y - A.Y;
        if (Dy > 1e-6) T1 = FMath::Min(T1, (Pl.FaceY - 80.0 - A.Y) / Dy);
        if (Dy < -1e-6) T0 = FMath::Max(T0, (Pl.FaceY - 80.0 - A.Y) / Dy);
    }
    if (T1 - T0 < 400.0) { T0 = S0 + 40.0; T1 = S1 - 40.0; }
    // The deck: planks across, stringers under them.
    for (int32 K = 0; S0 + K * 16.0 + 14.5 <= S1 + .1; ++K)
    {
        const double A = S0 + K * 16.0;
        const float Sink = .5f * Hash01(K, 4, 6150u);
        FrameBox(M, KPlank, EColl::None, F, A, A + 14.5, -PlankHalf, PlankHalf, Z - 5.0 - Sink, Z - Sink, AllFaces, .6f, 1.f);
        ++St.Planks;
    }
    for (const double L : { -55.0, 0.0, 55.0 }) FrameBox(M, KTimber, EColl::None, F, S0, S1, L - 6.0, L + 6.0, Z - 20.0, Z - 5.0, AllFaces, .4f, .6f);
    FrameBox(M, -1, EColl::Walk, F, S0, S1, -(TrussL - 7.0), TrussL - 7.0, Z - 1.0, Z, 1);
    // The deck's underside (planks, stringers, floor beams) solid to anyone wading under it.
    FrameBox(M, -1, EColl::Block, F, S0, S1, -(TrussL + 14.0), TrussL + 14.0, Z - 40.0, Z - 1.0, SidesOnly | 2);
    // Panel points, floor beams, the trusses.
    const int32 Panels = FMath::Max(4, FMath::RoundToInt((T1 - T0) / 200.0));
    const double Mid = (S0 + S1) * .5;
    for (int32 K = 0; K <= Panels; ++K)
    {
        const double P = FMath::Lerp(T0, T1, double(K) / Panels);
        FrameBox(M, KTimber, EColl::None, F, P - 8.0, P + 8.0, -(TrussL + 14.0), TrussL + 14.0, Z - 40.0, Z - 20.0, AllFaces, .4f, .6f);
    }
    for (const double Sg : { -1.0, 1.0 })
    {
        const double L = Sg * TrussL;
        FrameBox(M, KTimber, EColl::None, F, T0, T1, L - 7.0, L + 7.0, Z - 30.0, Z + 6.0, AllFaces, .5f, .8f);          // bottom chord
        FrameBox(M, KTimber, EColl::None, F, T0, T1, L - 7.0, L + 7.0, Z + 100.0, Z + 112.0, AllFaces, .8f, 1.f);       // top chord
        FrameBox(M, KTimber, EColl::None, F, T0, T1, L - 4.0, L + 4.0, Z + 50.0, Z + 58.0, AllFaces, .7f, .9f);         // mid rail
        for (int32 K = 0; K <= Panels; ++K)
        {
            const double P = FMath::Lerp(T0, T1, double(K) / Panels);
            FrameBox(M, KTimber, EColl::None, F, P - 6.0, P + 6.0, L - 6.0, L + 6.0, Z + 6.0, Z + 100.0, SidesOnly, .7f, .9f);
            if (K == Panels) continue;
            const double Pn = FMath::Lerp(T0, T1, double(K + 1) / Panels);
            const bool bLeftHalf = (P + Pn) * .5 < Mid;
            const FVector2D At0 = F.At(bLeftHalf ? Pn : P, L), At1 = F.At(bLeftHalf ? P : Pn, L);
            Beam(M, KTimber, EColl::None, Lift(At0, Z + 6.0), Lift(At1, Z + 100.0), 10.0, 12.0, F.S, .8f);
        }
        FrameBox(M, -1, EColl::Block, F, T0, T1, L - 7.0, L + 7.0, Z - 30.0, Z + 112.0, AllFaces);
        St.RailCm += T1 - T0;
        ++St.Rails;
        // Where the deck runs on past the truss, the drop beside it must stay under 75 cm.
        for (double S = S0; S <= S1; S += 20.0)
        {
            if (S >= T0 && S <= T1) continue;
            const float Drop = Z - GroundAt(F.At(S, Sg * (Hf + 25.0)));
            if (Drop >= 75.f) { St.Notes.Add(FString::Printf(TEXT("CALDER_WATER_NOTE footbridge deck at s=%.0f side=%+.0f has a %.0f cm drop past its truss"), S, Sg, Drop)); break; }
        }
    }
    // The bents: on the dry floor (5 cm over the water at least), the first place out from the pool where one fits.
    for (const double Dir : { -1.0, 1.0 })
    {
        for (double S = Dir * 760.0; Dir > 0.0 ? S < T1 - 60.0 : S > T0 + 60.0; S += Dir * 20.0)
        {
            const FVector2D Pa = F.At(S, -60.0), Pb = F.At(S, 60.0);
            const float Ga = GroundAt(Pa), Gb = GroundAt(Pb);
            const float Wa = GlenLevel(Pl, ReachOf(Pl, Pa.Y), Pa.Y), Wb = GlenLevel(Pl, ReachOf(Pl, Pb.Y), Pb.Y);
            if (Ga < Wa + 5.f || Gb < Wb + 5.f || Z - 58.f - FMath::Max(Ga, Gb) < 25.f) continue;
            for (const FVector2D& P : { Pa, Pb })
            {
                const float G = GroundAt(P);
                const FFrame Fp(P, F.U);
                FrameBox(M, KBoulder, EColl::Walk, Fp, -20.0, 20.0, -20.0, 20.0, G - FootSink, G + 10.0, AllFaces, .5f, .9f);
                Foot(P, G - FootSink, St);
                FrameBox(M, KTimber, EColl::Block, Fp, -9.0, 9.0, -9.0, 9.0, G + 10.0, Z - 58.0, SidesOnly, .6f, .8f);
            }
            FrameBox(M, KTimber, EColl::None, F, S - 9.0, S + 9.0, -(TrussL + 14.0), TrussL + 14.0, Z - 58.0, Z - 40.0, AllFaces, .5f, .7f);
            Beam(M, KTimber, EColl::None, Lift(Pa, GroundAt(Pa) + 14.0), Lift(Pb, Z - 62.0), 8.0, 10.0, F.U, .6f);
            Beam(M, KTimber, EColl::None, Lift(Pb, GroundAt(Pb) + 14.0), Lift(Pa, Z - 62.0), 8.0, 10.0, F.U, .6f);
            ++St.Bents;
            break;
        }
    }
    ++St.Bridges;
}

/** The headwall where the glen leaves the walkable square (on the campus hedge line): a mossy stone wall from wing to wing (its
 *  tops 2 m over the ground near them, stepping up the glen's sides), a dark arch over the water with a dressed ring and
 *  keystone, a short barrel behind it (black inside), a rusted iron grate across the arch (one bar pushed in, sticks caught
 *  at the waterline). */
void BuildHeadwall(FMeshSet& M, const FPlan& Pl, FStats& St)
{
    const FHeadPlan& H = Pl.Head;
    if (!H.bOn) return;
    const double O0 = H.Cx - H.HalfOpen, O1 = H.Cx + H.HalfOpen;
    const float RoofZ = H.CrownZ + 60.f;   // the barrel's roof behind the wall
    float Bottom = FLT_MAX;
    for (double X = H.XW; X <= H.XE + .1; X += 25.0)
        for (const double Y : { H.YFront + 3.0, H.YBack - 3.0, H.YEnd - 3.0 }) Bottom = FMath::Min(Bottom, GroundAt(FVector2D(X, Y)));
    Bottom = FMath::Min(Bottom, H.InvZ) - FaceFoot;
    // A face over [X0, X1] from Bottom up to Top, round a hole: bArch, the arch (the soffit over it, the invert under it);
    // else the barrel's box behind the wall (its walls 40 cm thick, its roof at RoofZ, down to Bottom).
    auto Face = [&](double Y, const FVector& Out, double X0, double X1, float Top, bool bArch)
    {
        const double HoleHalf = bArch ? H.HalfOpen : H.HalfOpen + 40.0;
        TArray<double> Xs = Steps(X0, X1, 25.0);
        for (const double X : { H.Cx - HoleHalf, H.Cx + HoleHalf }) if (X > X0 && X < X1) Xs.Add(X);
        Xs.Sort();
        for (int32 K = 0; K + 1 < Xs.Num(); ++K)
        {
            const double Xa = Xs[K], Xb = Xs[K + 1];
            if (Xb - Xa < .5) continue;
            const bool bHole = (Xa + Xb) * .5 > H.Cx - HoleHalf && (Xa + Xb) * .5 < H.Cx + HoleHalf;
            if (bHole)
            {
                const double Za = bArch ? H.Soffit(Xa) : RoofZ, Zb = bArch ? H.Soffit(Xb) : RoofZ;
                if (Top > FMath::Max(Za, Zb)) { const FVector Q[4] = { FVector(Xa, Y, Za), FVector(Xb, Y, Zb), FVector(Xb, Y, Top), FVector(Xa, Y, Top) }; Poly(M, KStone, EColl::None, Q, 4, Out, nullptr, nullptr, Ao(.85f)); }
                if (bArch) { const FVector Q[4] = { FVector(Xa, Y, Bottom), FVector(Xb, Y, Bottom), FVector(Xb, Y, H.InvZ), FVector(Xa, Y, H.InvZ) }; Poly(M, KStone, EColl::None, Q, 4, Out, nullptr, nullptr, Ao(.4f)); }
            }
            else { const FVector Q[4] = { FVector(Xa, Y, Bottom), FVector(Xb, Y, Bottom), FVector(Xb, Y, Top), FVector(Xa, Y, Top) }; const FLinearColor Cq[4] = { Ao(.4f), Ao(.4f), Ao(1.f), Ao(1.f) }; Poly(M, KStone, EColl::None, Q, 4, Out, nullptr, Cq); }
        }
    };
    const FFrame F(FVector2D(0.0, 0.0), FVector2D(1.0, 0.0));
    for (int32 R = 0; R < H.Runs.Num(); ++R)
    {
        const FHeadRun& Run = H.Runs[R];
        Face(H.YFront, FVector(0.0, 1.0, 0.0), Run.X0, Run.X1, Run.Top, true);
        Face(H.YBack, FVector(0.0, -1.0, 0.0), Run.X0, Run.X1, Run.Top, false);
        // The coping, and the step faces where the next run stands higher (or the wall ends).
        FrameBox(M, KCoping, EColl::Block, F, Run.X0 - (R == 0 ? 4.0 : 0.0), Run.X1 + (R + 1 == H.Runs.Num() ? 4.0 : 0.0), H.YBack - 4.0, H.YFront + 4.0, Run.Top, Run.Top + 10.0, AllFaces, .8f, 1.f);
        for (int32 End = 0; End < 2; ++End)
        {
            const int32 Nb = End == 0 ? R - 1 : R + 1;
            const float Low = Nb >= 0 && Nb < H.Runs.Num() ? H.Runs[Nb].Top + 10.f : Bottom;
            if (Low >= Run.Top) continue;
            const double X = End == 0 ? Run.X0 : Run.X1;
            const FVector Q[4] = { FVector(X, H.YBack, Low), FVector(X, H.YFront, Low), FVector(X, H.YFront, Run.Top), FVector(X, H.YBack, Run.Top) };
            Poly(M, KStone, EColl::None, Q, 4, FVector(End == 0 ? -1.0 : 1.0, 0.0, 0.0), nullptr, nullptr, Ao(.8f));
        }
        // Its collision: the whole run solid (the grate closes the arch in front of it anyway).
        FrameBox(M, -1, EColl::Block, F, Run.X0, Run.X1, H.YBack, H.YFront, Bottom, Run.Top + 10.0, AllFaces);
    }
    // The opening through the wall and on through the barrel: soffit, sides, invert (black).
    const TArray<double> Ox = Steps(O0, O1, 25.0);
    for (int32 K = 0; K + 1 < Ox.Num(); ++K)
    {
        const double Za = H.Soffit(Ox[K]), Zb = H.Soffit(Ox[K + 1]);
        const FVector Q[4] = { FVector(Ox[K], H.YFront, Za), FVector(Ox[K + 1], H.YFront, Zb), FVector(Ox[K + 1], H.YEnd, Zb), FVector(Ox[K], H.YEnd, Za) };
        Poly(M, KDark, EColl::None, Q, 4, -FVector::UpVector, nullptr, nullptr, Ao(.1f));
    }
    for (const double Sg : { -1.0, 1.0 })
    {
        const double X = H.Cx + Sg * H.HalfOpen;
        const FVector Q[4] = { FVector(X, H.YFront, H.InvZ), FVector(X, H.YEnd, H.InvZ), FVector(X, H.YEnd, H.SpringZ), FVector(X, H.YFront, H.SpringZ) };
        Poly(M, KDark, EColl::None, Q, 4, FVector(-Sg, 0.0, 0.0), nullptr, nullptr, Ao(.1f));
        // The barrel's outside behind the wall.
        const double Xo = H.Cx + Sg * (H.HalfOpen + 40.0);
        const FVector Qo[4] = { FVector(Xo, H.YBack, Bottom), FVector(Xo, H.YEnd, Bottom), FVector(Xo, H.YEnd, RoofZ), FVector(Xo, H.YBack, RoofZ) };
        Poly(M, KStone, EColl::Block, Qo, 4, FVector(Sg, 0.0, 0.0), nullptr, nullptr, Ao(.7f));
    }
    { const FVector Q[4] = { FVector(O0, H.YFront, H.InvZ), FVector(O1, H.YFront, H.InvZ), FVector(O1, H.YEnd, H.InvZ), FVector(O0, H.YEnd, H.InvZ) }; Poly(M, KDark, EColl::None, Q, 4, FVector::UpVector, nullptr, nullptr, Ao(.1f)); }
    FrameBox(M, KStone, EColl::Block, F, O0 - 40.0, O1 + 40.0, H.YEnd, H.YBack, RoofZ - 10.0, RoofZ, 1, .7f, .9f);
    Face(H.YEnd, FVector(0.0, -1.0, 0.0), O0 - 40.0, O1 + 40.0, RoofZ, true);
    // The ring (dressed, 5 cm proud, 40 cm deep) and the keystone.
    for (int32 K = 0; K + 1 < Ox.Num(); ++K)
    {
        const double Za = H.Soffit(Ox[K]), Zb = H.Soffit(Ox[K + 1]);
        const double Yf = H.YFront + 5.0;
        { const FVector Q[4] = { FVector(Ox[K], Yf, Za), FVector(Ox[K + 1], Yf, Zb), FVector(Ox[K + 1], Yf, Zb + 40.0), FVector(Ox[K], Yf, Za + 40.0) }; Poly(M, KDressed, EColl::None, Q, 4, FVector(0.0, 1.0, 0.0), nullptr, nullptr, Ao(.85f)); }
        { const FVector Q[4] = { FVector(Ox[K], H.YFront, Za), FVector(Ox[K + 1], H.YFront, Zb), FVector(Ox[K + 1], Yf, Zb), FVector(Ox[K], Yf, Za) }; Poly(M, KDressed, EColl::None, Q, 4, -FVector::UpVector, nullptr, nullptr, Ao(.5f)); }
        { const FVector Q[4] = { FVector(Ox[K], H.YFront, Za + 40.0), FVector(Ox[K + 1], H.YFront, Zb + 40.0), FVector(Ox[K + 1], Yf, Zb + 40.0), FVector(Ox[K], Yf, Za + 40.0) }; Poly(M, KDressed, EColl::None, Q, 4, FVector::UpVector, nullptr, nullptr, Ao(.9f)); }
    }
    for (const double Xe : { O0, O1 })
    {
        const FVector Q[4] = { FVector(Xe, H.YFront, H.SpringZ), FVector(Xe, H.YFront + 5.0, H.SpringZ), FVector(Xe, H.YFront + 5.0, H.SpringZ + 40.0), FVector(Xe, H.YFront, H.SpringZ + 40.0) };
        Poly(M, KDressed, EColl::None, Q, 4, FVector(Xe < H.Cx ? -1.0 : 1.0, 0.0, 0.0), nullptr, nullptr, Ao(.8f));
    }
    FrameBox(M, KDressed, EColl::None, F, H.Cx - 22.0, H.Cx + 22.0, H.YFront, H.YFront + 9.0, H.CrownZ - 4.0, H.CrownZ + 48.0, AllFaces, .8f, 1.f);
    // The grate: round bars every 15 cm, three flat bars, rusted; one bar pushed in at its foot.
    const double Yg = H.YFront - 15.0;
    const int32 Bars = FMath::Max(1, FMath::FloorToInt((2.0 * H.HalfOpen - 15.0) / 15.0) + 1);
    const int32 Bent = FMath::FloorToInt(Bars * .62);
    for (int32 K = 0; K < Bars; ++K)
    {
        const double X = O0 + 7.5 + 15.0 * K;
        if (X >= O1) break;
        const double Zt = H.Soffit(X) + 4.0;
        if (K == Bent)
        {
            const double Zk = H.InvZ + 75.0;
            Pipe(M, KRust, EColl::None, FVector(X + 3.0, Yg - 16.0, H.InvZ - 4.0), FVector(X, Yg, Zk), 2.4f, 6, false, false, .7f);
            Pipe(M, KRust, EColl::None, FVector(X, Yg, Zk), FVector(X, Yg, Zt), 2.4f, 6, false, false, .8f);
        }
        else Pipe(M, KRust, EColl::None, FVector(X, Yg, H.InvZ - 4.0), FVector(X, Yg, Zt), 2.4f, 6, false, false, .8f);
        ++St.Bars;
    }
    for (const double Zf : { H.InvZ + 70.0, H.InvZ + 150.0, H.InvZ + 230.0 })
    {
        if (Zf + 6.0 >= H.CrownZ) continue;
        // where the soffit stands over the bar: |X - Cx| <= sqrt(R^2 - (Zf + 6 - CrownZ + R)^2)
        const double Dz = Zf + 6.0 - (H.CrownZ - H.R);
        const double Reach = Zf + 6.0 <= H.SpringZ ? H.HalfOpen : FMath::Sqrt(FMath::Max(0.0, H.R * H.R - Dz * Dz));
        FrameBox(M, KRust, EColl::None, F, H.Cx - Reach, H.Cx + Reach, Yg - 3.0, Yg + 3.0, Zf - 4.0, Zf + 4.0, AllFaces, .7f, .8f);
    }
    FrameBox(M, -1, EColl::Block, F, O0, O1, Yg - 4.0, Yg + 4.0, H.InvZ - 10.0, H.CrownZ + 5.0, AllFaces);
    // Sticks caught on the grate at the waterline.
    const float Wz = H.WaterZ;
    Pipe(M, KTimber, EColl::None, FVector(H.Cx - 185.0, Yg + 9.0, Wz - 1.0), FVector(H.Cx - 55.0, Yg + 6.0, Wz + 3.0), 3.2f, 6, true, true, .6f);
    Pipe(M, KTimber, EColl::None, FVector(H.Cx + 15.0, Yg + 7.0, Wz - 2.0), FVector(H.Cx + 160.0, Yg + 22.0, Wz + 1.0), 2.4f, 6, true, true, .6f);
    Pipe(M, KTimber, EColl::None, FVector(H.Cx + 92.0, Yg + 6.0, Wz + 1.0), FVector(H.Cx + 104.0, Yg + 4.0, Wz + 70.0), 2.0f, 6, true, true, .6f);
}

// ------------------------------------------------------------------ materials, meshes and components
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
        else UE_LOG(LogTemp, Warning, TEXT("CALDER_WATER texture %s is missing"), *Path);
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

/** One look of M_CalderWater: the flow (direction and speed), the ripples, the foam, the colours. */
struct FWaterLook
{
    FVector2D Flow = FVector2D(0.0, -1.0);
    float Speed = 35.f, RippleTile = 180.f, Ripple = .5f, Riffle = 1.5f, Foam = 1.f, FoamLum = 4.5f, FoamTile = 90.f, Rough = .04f;
    FLinearColor Shallow = FLinearColor(.55f, .36f, .18f, 1.f), Deep = FLinearColor(.010f, .013f, .008f, 1.f);
    FLinearColor FoamColor = FLinearColor(.75f, .74f, .70f, 1.f);   // M_CalderWater's default
};

UMaterialInterface* WaterMaterial(UObject* Outer, const FWaterLook& L)
{
    UMaterialInterface* Base = nullptr;
    if (HouseCalderLandscape::Enabled()) Base = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderStream.M_CalderStream"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderWater.M_CalderWater"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Base) return nullptr;
    UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Outer);
    Mid->SetVectorParameterValue(TEXT("FlowDir"), FLinearColor(float(L.Flow.X), float(L.Flow.Y), 0.f, 0.f));
    Mid->SetScalarParameterValue(TEXT("FlowSpeed"), L.Speed);
    Mid->SetScalarParameterValue(TEXT("RippleTileSize"), L.RippleTile);
    Mid->SetScalarParameterValue(TEXT("RippleStrength"), L.Ripple);
    Mid->SetScalarParameterValue(TEXT("RiffleBoost"), L.Riffle);
    Mid->SetScalarParameterValue(TEXT("FoamStrength"), L.Foam);
    Mid->SetScalarParameterValue(TEXT("FoamLumScale"), L.FoamLum);
    Mid->SetScalarParameterValue(TEXT("FoamTileSize"), L.FoamTile);
    Mid->SetScalarParameterValue(TEXT("Roughness"), L.Rough);
    Mid->SetVectorParameterValue(TEXT("ShallowTint"), L.Shallow);
    Mid->SetVectorParameterValue(TEXT("DeepColor"), L.Deep);
    Mid->SetVectorParameterValue(TEXT("FoamColor"), L.FoamColor);
    if (HouseCalderLandscape::Enabled())
    {
        Mid->SetScalarParameterValue(TEXT("FoamRoughness"), .08f);
        Mid->SetScalarParameterValue(TEXT("Specular"), .25f);
    }
    return Mid;
}

/** Vertical water needs a world-XZ projection: the old bed shader stretches
 *  brown gravel down a vertical face. Retain that appearance in the A/B path. */
UMaterialInterface* FallsMaterial(UObject* Outer, float Top, float Bottom, float Speed, UMaterialInterface* Fallback)
{
    if (!HouseCalderLandscape::Enabled()) return Fallback;
    const bool JetsTrial = FParse::Param(FCommandLine::Get(), TEXT("CalderFallsJetsTrial"));
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
        JetsTrial ? TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderFallsJetsTrial.M_CalderFallsJetsTrial")
            : TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderFalls.M_CalderFalls"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Base)
    {
        UE_LOG(LogTemp, Warning, TEXT("CALDER_FALLS missing opaque falls material; retaining original water"));
        return Fallback;
    }
    UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Outer);
    Mid->SetScalarParameterValue(TEXT("FallTopZ"), Top);
    Mid->SetScalarParameterValue(TEXT("FallBottomZ"), Bottom);
    Mid->SetScalarParameterValue(TEXT("FlowSpeed"), Speed);
    if (JetsTrial)
    {
        UE_LOG(LogTemp, Display, TEXT("CALDER_FALLS_JETS trial=1 material=%s collisionWaterQueriesUnchanged=1 roughness=0.06"), *Base->GetName());
    }
    UE_LOG(LogTemp, Display, TEXT("CALDER_FALLS material=opaque top=%.1f bottom=%.1f flowCmPerS=%.0f newLights=0"), Top, Bottom, Speed);
    return Mid;
}

void WaterMaterials(UObject* Outer, const FPlan& Pl, TArray<UMaterialInterface*>& Out)
{
    if (!LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderWater.M_CalderWater"), nullptr, LOAD_NoWarn | LOAD_Quiet))
        UE_LOG(LogTemp, Warning, TEXT("CALDER_WATER M_CalderWater is missing; the water falls back to the default material"));
    Out.SetNumZeroed(WCount);
    auto Dir = [](const FVector2D& From, const FVector2D& To) { return (To - From).GetSafeNormal(); };
    FWaterLook Pond;   // still, near black, the faintest drift toward the race
    Pond.Flow = Dir(Pl.RaceA, Pl.RaceB);
    Pond.Speed = 5.f; Pond.RippleTile = 260.f; Pond.Ripple = .22f; Pond.Rough = .03f;
    Pond.Shallow = FLinearColor(.45f, .3f, .16f, 1.f); Pond.Deep = FLinearColor(.006f, .008f, .006f, 1.f);
    Out[WPond] = WaterMaterial(Outer, Pond);
    FWaterLook Creek;
    Creek.Flow = Pl.CreekAt.Num() >= 2 ? Dir(Pl.CreekAt[0], Pl.CreekAt.Last()) : FVector2D(0.0, -1.0);
    Creek.Speed = 55.f; Creek.RippleTile = 150.f; Creek.Ripple = .45f;
    Out[WCreek] = WaterMaterial(Outer, Creek);
    auto ReachDir = [&Pl, &Dir](int32 R) { return Pl.GlenAt.Num() >= 2 ? Dir(Pl.GlenAt[Pl.ReachFirst[R]], Pl.GlenAt[Pl.ReachLast[R]]) : FVector2D(0.0, -1.0); };
    FWaterLook Upper;
    Upper.Flow = ReachDir(0); Upper.Speed = 70.f; Upper.RippleTile = 160.f; Upper.Ripple = .5f;
    Out[WGlen0] = WaterMaterial(Outer, Upper);
    FWaterLook Pool;   // the plunge pool: slow, dark, stirred, foaming at the falls' foot
    Pool.Flow = ReachDir(1); Pool.Speed = 35.f; Pool.RippleTile = 140.f; Pool.Ripple = .6f; Pool.Foam = 1.3f; Pool.Riffle = 2.f;
    Out[WPool] = WaterMaterial(Outer, Pool);
    FWaterLook Lower;
    Lower.Flow = ReachDir(2); Lower.Speed = 90.f; Lower.RippleTile = 150.f; Lower.Ripple = .55f;
    Out[WGlen2] = WaterMaterial(Outer, Lower);
    FWaterLook Sheet;   // the weir's sheet: fast, white, streaked down its face
    Sheet.Flow = Pl.FallsFlow; Sheet.Speed = 380.f; Sheet.RippleTile = 120.f; Sheet.Ripple = .8f; Sheet.Foam = 1.6f; Sheet.FoamLum = 9.f; Sheet.FoamTile = 60.f; Sheet.Rough = .2f;
    FWaterLook Fall = Sheet;   // the falls and the cascade: dark, wet water streaked with foam, white at the lip and the foot
    Fall.Foam = 1.6f; Fall.FoamLum = 3.5f; Fall.FoamTile = 35.f; Fall.Rough = .12f;   // FoamLum 3.5: about half the pattern foams (white streaks, dark gaps)
    Fall.FoamColor = FLinearColor(.84f, .86f, .88f, 1.f);   // cold white water, not the warm grey of a lazy creek's scum
    Out[WFalls] = FallsMaterial(Outer, Pl.FallsTopZ, GlenLevel(Pl, 1, Pl.CurtainY - 100.0), Fall.Speed, WaterMaterial(Outer, Fall));
    FWaterLook Casc = Fall;
    Casc.Flow = Pl.CascFlow; Casc.Speed = 300.f;
    Out[WCascade] = FallsMaterial(Outer, Pl.CascTopZ, GlenLevel(Pl, 2, Pl.CascY - 100.0), Casc.Speed, WaterMaterial(Outer, Casc));
    FWaterLook Weir = Sheet;
    Weir.Flow = Pl.Dam.bOn ? -Pl.Dam.Runs[1].Nrm : FVector2D(0.0, -1.0);
    Weir.Speed = 220.f; Weir.Foam = 1.3f;
    Out[WWeir] = FallsMaterial(Outer, Pl.Dam.WeirZ, Pl.Dam.WeirZ - 140.f, Weir.Speed, WaterMaterial(Outer, Weir));
}

/** Four bounded, non-colliding volumes. Their whole XY envelopes avoid the
 *  frozen zones; the three existing WoodlandHollowMist actors are untouched. */
void WaterMist(AActor* Owner, const FPlan& Pl)
{
    if (!HouseCalderLandscape::Enabled()) return;
    UWorld* World = Owner->GetWorld();
    auto Add = [Owner, World](const TCHAR* Label, const FVector& Position, float Radius, bool bSpray)
    {
        for (const FBox2D& Rect : HouseCalderTerrain::FrozenRects())
        {
            const FVector2D Nearest(FMath::Clamp(Position.X, Rect.Min.X, Rect.Max.X), FMath::Clamp(Position.Y, Rect.Min.Y, Rect.Max.Y));
            if ((FVector2D(Position) - Nearest).SizeSquared() <= FMath::Square(double(Radius)))
            {
                UE_LOG(LogTemp, Warning, TEXT("CALDER_WATER_MIST rejected=%s overlapsFrozen=1"), Label);
                return false;
            }
        }
        FActorSpawnParameters Params;
        Params.Owner = Owner;
        Params.Name = MakeUniqueObjectName(Owner->GetLevel(), ALocalFogVolume::StaticClass(), FName(Label));
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ALocalFogVolume* Volume = World->SpawnActor<ALocalFogVolume>(ALocalFogVolume::StaticClass(), Position, FRotator::ZeroRotator, Params);
        if (!Volume) return false;
        Volume->Tags.Add(TEXT("CalderHollowMist"));
        Volume->SetActorEnableCollision(false);
        Volume->SetActorTickEnabled(false);
        Volume->SetActorScale3D(FVector(Radius / ULocalFogVolumeComponent::GetBaseVolumeSize()));
        if (ULocalFogVolumeComponent* Fog = Volume->GetComponent())
        {
            Fog->RadialFogExtinction = bSpray ? .22f : .08f;
            Fog->HeightFogExtinction = bSpray ? .55f : .32f;
            Fog->HeightFogFalloff = bSpray ? 500.f : 1600.f;
            Fog->HeightFogOffset = bSpray ? .15f : .025f;
            Fog->FogAlbedo = FLinearColor(.44f, .50f, .54f);
            Fog->FogEmissive = bSpray ? FLinearColor(.09f, .12f, .14f) : FLinearColor(.025f, .035f, .045f);
            Fog->FogPhaseG = .12f;
            Fog->FogStartDistance = 200.f;
            Fog->MarkRenderStateDirty();
        }
        UE_LOG(LogTemp, Display, TEXT("CALDER_WATER_MIST name=%s at=%s radius=%.0f spray=%d collision=0 frozenOverlap=0 newLights=0"),
            *Volume->GetName(), *Position.ToString(), Radius, int32(bSpray));
        return true;
    };
    int32 Count = 0;
    if (Pl.Pond.Num())
    {
        FVector2D Centre(0.0);
        for (const FVector2D& Point : Pl.Pond) Centre += Point;
        Centre /= Pl.Pond.Num();
        Count += Add(TEXT("CalderHollowMistPond"), Lift(Centre, Pl.PondZ + 28.f), 1400.f, false);
    }
    const float PoolZ = GlenLevel(Pl, 1, Pl.CurtainY - 100.0);
    const FVector2D FallsFoot = Landing(FVector((Pl.FallsXA + Pl.FallsXB) * .5, Pl.CurtainY, Pl.FallsTopZ - 2.f), Pl.FallsFlow, 0.f, 115.f, PoolZ);
    Count += Add(TEXT("CalderHollowMistFalls"), Lift(FallsFoot, PoolZ + 95.f), 750.f, true);
    const float LowZ = GlenLevel(Pl, 2, Pl.CascY - 100.0);
    const FVector2D CascFoot = Landing(FVector((Pl.CascXA + Pl.CascXB) * .5, Pl.CascY, Pl.CascTopZ), Pl.CascFlow, 12.f, 140.f, LowZ);
    Count += Add(TEXT("CalderHollowMistCascade"), Lift(CascFoot, LowZ + 40.f), 650.f, true);
    const int32 Middle = (Pl.ReachFirst[2] + Pl.ReachLast[2]) / 2;
    if (Pl.GlenAt.IsValidIndex(Middle))
        Count += Add(TEXT("CalderHollowMistLowerGlen"), Lift(Pl.GlenAt[Middle], Pl.GlenZ[Middle] + 28.f), 1100.f, false);
    UE_LOG(LogTemp, Display, TEXT("CALDER_WATER_MIST count=%d existingWoodlandHollowsUnchanged=1 globalAtmosphereUnchanged=1"), Count);
}

void WorkMaterials(UObject* Outer, TArray<UMaterialInterface*>& Out)
{
    if (!LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderRock.M_CalderRock"), nullptr, LOAD_NoWarn | LOAD_Quiet))
        UE_LOG(LogTemp, Warning, TEXT("CALDER_WATER M_CalderRock is missing; the structures fall back to the default material"));
    Out.SetNumZeroed(KCount);
    Out[KStone] = RockMaterial(Outer, TEXT("mossy_stone_wall"), 200.f, .6f, FLinearColor(.78f, .78f, .74f));       // damp, mossy 1930s masonry
    Out[KDressed] = RockMaterial(Outer, TEXT("stacked_stone_wall"), 170.f, .4f, FLinearColor(.84f, .82f, .78f));
    Out[KCoping] = RockMaterial(Outer, TEXT("cracked_concrete"), 150.f, .35f, FLinearColor(.72f, .71f, .68f));     // stained concrete
    Out[KDeck] = RockMaterial(Outer, TEXT("concrete_pavement"), 180.f, .2f, FLinearColor(.7f, .69f, .66f));
    Out[KGravel] = RockMaterial(Outer, TEXT("brown_mud_rocks_01"), 140.f, .15f, FLinearColor(.85f, .8f, .74f));
    Out[KSandstone] = RockMaterial(Outer, TEXT("rock_face_04"), 380.f, .5f, FLinearColor(.92f, .82f, .66f));       // the warm caprock
    Out[KBoulder] = RockMaterial(Outer, TEXT("lichen_rock"), 120.f, .55f, FLinearColor(.85f, .85f, .82f));
    Out[KPlank] = RockMaterial(Outer, TEXT("weathered_planks"), 160.f, .3f, FLinearColor(.72f, .68f, .62f));       // grey, weathered
    Out[KTimber] = RockMaterial(Outer, TEXT("rough_wood"), 140.f, .35f, FLinearColor(.6f, .55f, .48f));
    Out[KRust] = RockMaterial(Outer, TEXT("rock_06"), 45.f, .05f, FLinearColor(.5f, .24f, .11f));                  // pitted rust
    Out[KDark] = PlainMaterial(Outer, FLinearColor(.006f, .006f, .006f), 1.f, 0.f);                               // the culverts' dark
}

UStaticMesh* BuildMesh(const FMeshSet& Set, const TArray<UMaterialInterface*>& Mats, const TCHAR* const* SlotNames, const FString& Name, int32& OutTris)
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
    for (int32 MI = 0; MI < Set.Parts.Num(); ++MI)
    {
        const FDrawPart& D = Set.Parts[MI];
        if (D.I.Num() < 3) continue;
        const FPolygonGroupID Group = Description.CreatePolygonGroup();
        Slots[Group] = SlotNames[MI];
        Used.Add(FStaticMaterial(Mats.IsValidIndex(MI) ? Mats[MI] : nullptr, SlotNames[MI]));
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
            Colors[Instance] = FVector4f(D.C[K].R, D.C[K].G, D.C[K].B, D.C[K].A);
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

/** A drawn component (no collision): culled at Cull, shadows as asked. Returns its triangles. */
int32 AddDraw(AActor* Owner, const FMeshSet& Set, const TArray<UMaterialInterface*>& Mats, const TCHAR* const* SlotNames, const FString& Name, float Cull, bool bShadow, int32& InOutComponents)
{
    int32 Tris = 0;
    UStaticMesh* Mesh = BuildMesh(Set, Mats, SlotNames, Name, Tris);
    if (!Mesh) return 0;
    UStaticMeshComponent* Draw = NewObject<UStaticMeshComponent>(Owner, *Name);
    Draw->SetMobility(EComponentMobility::Static);
    Draw->SetStaticMesh(Mesh);
    Draw->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Draw->SetCastShadow(bShadow);
    Draw->bAffectDistanceFieldLighting = false;
    Draw->SetCullDistance(Cull);
    Draw->RegisterComponent();
    ++InOutComponents;
    return Tris;
}

/** How many of a set's points (drawn and collision) lie in a frozen zone (must be 0: nothing new stands there). */
int32 FrozenPoints(const FMeshSet& Set)
{
    int32 N = 0;
    auto Count = [&N](const TArray<FVector>& Vs) { for (const FVector& V : Vs) N += HouseCalderTerrain::InFrozen(float(V.X), float(V.Y)) ? 1 : 0; };
    for (const FDrawPart& D : Set.Parts) Count(D.V);
    Count(Set.Walk.V);
    Count(Set.Block.V);
    return N;
}

/** A hidden procedural copy for collision (BlockAll, the camera ignores it), complex as simple, cooked now. */
int32 AddCollision(AActor* Owner, const FCollPart& C, const FString& Name, bool bUnwalkable)
{
    if (C.I.Num() < 3) return 0;
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
    return C.I.Num() / 3;
}
}

bool HouseCalderWater::Active()
{
    // The landform's water data are absolute heights for the relief at scale 1 (a -CalderReliefScale bisect run leaves the
    // dam to HouseCalderFeatures' generic walls).
    return HouseCalderFeatures::Active() && FMath::IsNearlyEqual(HouseCalderTerrain::ReliefScale(), 1.f);
}

bool HouseCalderWater::BuildsDamWalls()
{
    return Active() && HouseCalderWaterImpl::Plan().Dam.bOn;
}

bool HouseCalderWater::BuildsRoadBridgeWalls()
{
    return Active() && HouseCalderWaterImpl::RoadBridge() != nullptr;
}

int32 HouseCalderWater::Build(AActor* Owner)
{
    namespace WI = HouseCalderWaterImpl;
    if (!Owner || !Owner->GetWorld()) return 0;
    if (!Active())
    {
        UE_LOG(LogTemp, Display, TEXT("CALDER_WATER off (the Calder terrain is off or its relief is flat)"));
        return 0;
    }
    const double Start = FPlatformTime::Seconds();
    const WI::FPlan& Pl = WI::Plan();
    WI::FStats St;
    WI::FMeshSet PondW(WI::WCount), GlenW(WI::WCount), Creek0(WI::WCount), Creek1(WI::WCount), Creek2(WI::WCount);
    WI::FMeshSet PondK(WI::KCount), GlenK(WI::KCount), RoadK(WI::KCount);
    // The water.
    WI::PondWater(PondW, Pl, St);
    WI::FMeshSet* const Chunks[3] = { &Creek0, &Creek1, &Creek2 };
    WI::CreekWater(Chunks, Pl, St);
    WI::GlenWater(GlenW, Pl, St);
    // The structures.
    WI::BuildDam(PondK, Pl, St);
    WI::BuildPondEdge(PondK, Pl, St);
    WI::BuildCauseway(PondK, Pl, St);
    WI::BuildDock(PondK, Pl, St);
    int32 StoneIx = 0;
    for (const HouseCalderLandform::FSteppingStone& S : HouseCalderLandform::SteppingStones())
    {
        WI::Boulder(S.Crossing == 0 ? PondK : GlenK, WI::KBoulder, S.At, S.Radius, S.TopZ, S.Yaw, 7300u + StoneIx++, St);
        ++St.Stones;
    }
    WI::BuildOverhang(GlenK, Pl, St);
    WI::BuildRockRail(GlenK, Pl, St);
    WI::BuildTimberBridge(GlenK, Pl, St);
    WI::BuildRoadBridge(RoadK, St);
    WI::BuildHeadwall(GlenK, Pl, St);

    const bool bDraw = Owner->GetWorld()->GetNetMode() != NM_DedicatedServer;
    int32 Components = 0, WaterTris = 0, WorksTris = 0, CollisionTris = 0;
    const int32 Frozen = WI::FrozenPoints(PondW) + WI::FrozenPoints(GlenW) + WI::FrozenPoints(Creek0) + WI::FrozenPoints(Creek1) + WI::FrozenPoints(Creek2)
        + WI::FrozenPoints(PondK) + WI::FrozenPoints(GlenK) + WI::FrozenPoints(RoadK);
    if (Frozen) UE_LOG(LogTemp, Warning, TEXT("CALDER_WATER_NOTE %d points stand in a frozen zone"), Frozen);
    CollisionTris += WI::AddCollision(Owner, PondK.Walk, TEXT("CalderWaterWorks_Pond_Collision"), false);
    CollisionTris += WI::AddCollision(Owner, PondK.Block, TEXT("CalderWaterWorks_Pond_CollisionUnwalkable"), true);
    CollisionTris += WI::AddCollision(Owner, GlenK.Walk, TEXT("CalderWaterWorks_Glen_Collision"), false);
    CollisionTris += WI::AddCollision(Owner, GlenK.Block, TEXT("CalderWaterWorks_Glen_CollisionUnwalkable"), true);
    CollisionTris += WI::AddCollision(Owner, RoadK.Walk, TEXT("CalderWaterWorks_RoadBridge_Collision"), false);
    CollisionTris += WI::AddCollision(Owner, RoadK.Block, TEXT("CalderWaterWorks_RoadBridge_CollisionUnwalkable"), true);
    if (bDraw)
    {
        TArray<UMaterialInterface*> WaterMats, WorkMats;
        WI::WaterMaterials(Owner, Pl, WaterMats);
        WI::WorkMaterials(Owner, WorkMats);
        WaterTris += WI::AddDraw(Owner, PondW, WaterMats, WI::WaterSlots, TEXT("CalderWater_Pond"), WI::WaterCull, false, Components);
        WaterTris += WI::AddDraw(Owner, GlenW, WaterMats, WI::WaterSlots, TEXT("CalderWater_Glen"), WI::WaterCull, false, Components);
        WaterTris += WI::AddDraw(Owner, Creek0, WaterMats, WI::WaterSlots, TEXT("CalderWater_Creek_0"), WI::WaterCull, false, Components);
        WaterTris += WI::AddDraw(Owner, Creek1, WaterMats, WI::WaterSlots, TEXT("CalderWater_Creek_1"), WI::WaterCull, false, Components);
        WaterTris += WI::AddDraw(Owner, Creek2, WaterMats, WI::WaterSlots, TEXT("CalderWater_Creek_2"), WI::WaterCull, false, Components);
        WorksTris += WI::AddDraw(Owner, PondK, WorkMats, WI::WorkSlots, TEXT("CalderWaterWorks_Pond"), WI::WorksCull, WI::bWorksCastShadow, Components);
        WorksTris += WI::AddDraw(Owner, GlenK, WorkMats, WI::WorkSlots, TEXT("CalderWaterWorks_Glen"), WI::WorksCull, WI::bWorksCastShadow, Components);
        WorksTris += WI::AddDraw(Owner, RoadK, WorkMats, WI::WorkSlots, TEXT("CalderWaterWorks_RoadBridge"), WI::WorksCull, WI::bWorksCastShadow, Components);
        WI::WaterMist(Owner, Pl);
    }
    for (const FString& Note : St.Notes) UE_LOG(LogTemp, Display, TEXT("%s"), *Note);
    UE_LOG(LogTemp, Display, TEXT("CALDER_WATER surfaces=%d tris=%d bridges=%d stones=%d rails=%d ms=%.0f waterTris=%d worksTris=%d collisionTris=%d components=%d parapets=%d parapetMeters=%.0f railMeters=%.0f kerbs=%d edgeMeters=%.0f planks=%d bents=%d grateBars=%d riprap=%d cells=%d wetVerts=%d tucked=%d maxDepth=%.0f feet=%d worstFootGap=%.1f frozenPoints=%d roadBridges=%d dam=%d causeway=%d headwall=%d"),
        St.Surfaces, WaterTris + WorksTris, St.Bridges, St.Stones, St.Rails, (FPlatformTime::Seconds() - Start) * 1000.0, WaterTris, WorksTris, CollisionTris, Components,
        St.Parapets, St.ParapetCm / 100.0, St.RailCm / 100.0, St.Kerbs, St.EdgeCm / 100.0, St.Planks, St.Bents, St.Bars, St.Riprap, St.Cells, St.Wet, St.Tucked, St.MaxDepth,
        St.Feet, St.WorstFoot, Frozen, St.RoadBridges, int32(Pl.Dam.bOn), int32(Pl.Way.bOn), int32(Pl.Head.bOn));
    return WaterTris + WorksTris;
}

bool HouseCalderWater::DeckAt(const FVector2D& P, float& OutZ)
{
    if (!Active()) return false;
    namespace WI = HouseCalderWaterImpl;
    const WI::FPlan& Pl = WI::Plan();
    if (Pl.Dam.bOn)
        for (const WI::FSeg& S : Pl.Dam.Runs)
            if (S.Covers(P, Pl.Dam.Half)) { OutZ = WI::DamDeck(Pl.Dam, P); return true; }
    if (Pl.Way.bOn)
        for (const WI::FSeg& S : Pl.Way.Runs)
            if (S.Covers(P, Pl.Way.Half))
            {
                double F = 0.0, C = 0.0;
                S.Locate(P, F, C);
                const bool bKerb = FMath::Abs(C) >= Pl.Way.Half - WI::KerbWidth && FMath::Abs(S.G0 + S.Len * F - Pl.Way.JointG) >= WI::HeadwallHalf;
                OutZ = WI::WayDeck(Pl.Way, P) + (bKerb ? WI::KerbRise : 0.f);
                return true;
            }
    const HouseCalderLandform::FPondDock& Dk = HouseCalderLandform::PondDock();
    {
        const WI::FFrame F(Dk.Land, Dk.Tip - Dk.Land);
        const double A = FVector2D::DotProduct(P - Dk.Land, F.U), L = FVector2D::DotProduct(P - Dk.Land, F.S);
        if (A >= 0.0 && A <= (Dk.Tip - Dk.Land).Size() && FMath::Abs(L) <= Dk.Width * .5) { OutZ = Dk.DeckZ; return true; }
    }
    for (const HouseCalderLandform::FWaterCrossing& X : HouseCalderLandform::WaterCrossings())
    {
        const WI::FFrame F(X.Centre, X.Axis);
        const double A = FVector2D::DotProduct(P - X.Centre, F.U), L = FVector2D::DotProduct(P - X.Centre, F.S);
        if (A < -X.Back || A > X.On) continue;
        if (X.Kind == HouseCalderLandform::EWaterCrossing::TimberFootbridge && FMath::Abs(L) <= 79.0)
        { OutZ = X.DeckZ; return true; }
        if (X.Kind == HouseCalderLandform::EWaterCrossing::RoadBridge && FMath::Abs(L) <= X.Width * .5 + 40.0 + WI::CopingOver)
        {
            const bool bVerge=FMath::Abs(L)>=X.Width*.5-45.-.001;
            OutZ=WI::RoadBridgeTop(X,A,L,bVerge)+(FMath::Abs(L)>=X.Width*.5-WI::CopingOver?107.f:0.f);
            return true;
        }
    }
    for (const HouseCalderLandform::FSteppingStone& S : HouseCalderLandform::SteppingStones())
        if ((P - S.At).Size() <= S.Radius * .78) { OutZ = S.TopZ; return true; }
    return false;
}

bool HouseCalderWater::WaterAt(const FVector2D& P, float& OutZ)
{
    if (!Active()) return false;
    namespace WI = HouseCalderWaterImpl;
    const WI::FPlan& Pl = WI::Plan();
    float DeckZ = 0.f;
    if (DeckAt(P, DeckZ)) return false;
    const float Ground = WI::FloorAt(Pl, P);
    // PondWater clips its sheet at the downstream face of the weir. The race's
    // distance envelope extends beyond that face; using the envelope alone
    // reported the pond level over the much lower outlet pool.
    const WI::FDamPlan& Dam = Pl.Dam;
    const bool bPondSide = !Dam.bOn || FVector2D::DotProduct(P - (Dam.Runs[1].A - Dam.Runs[1].Nrm * Dam.Half), Dam.Runs[1].Nrm) >= 0.0;
    float PondZ = Pl.PondZ;
    bool bPond = Pl.Pond.Num() >= 3 && (WI::InOutline(P, Pl.Pond) || WI::SegDist(P, Pl.RaceA, Pl.RaceB) <= Pl.RaceHalf + 50.0);
    if (Dam.bOn && bPondSide)
    {
        double Along = 0.0, Across = 0.0;
        Dam.Runs[1].Locate(P, Along, Across);
        if (Along >= 0.0 && Along <= 1.0 && FMath::Abs(Across) <= Dam.Half + 1.0)
        {
            PondZ = FMath::Lerp(Dam.WeirZ, Dam.WaterZ, float(FMath::Clamp((Across + Dam.Half) / (2.0 * Dam.Half), 0.0, 1.0)));
            bPond = true;
        }
    }
    if (bPondSide && bPond && Ground < PondZ) { OutZ = PondZ; return true; }
    if (Pl.CreekAt.Num() >= 2 && P.Y >= Pl.CreekAt.Last().Y - WI::MouthReach && P.Y <= Pl.CreekAt[0].Y
        && !WI::InOutline(P, Pl.Pond) && WI::CreekDist(Pl, P) <= WI::CreekHalfAt(Pl, P.Y) + 150.0)
    {
        const float Z = WI::CreekLevel(Pl, P.Y);
        if (Ground < Z) { OutZ = Z; return true; }
    }
    if (Pl.GlenAt.Num() >= 2 && P.Y < WI::DamLineY(Pl, P.X) && P.Y >= -HouseCalderTerrain::Half + 50.0)
    {
        float Half = 0.f;
        const int32 R = WI::ReachOf(Pl, P.Y);
        const float Z = WI::GlenLevel(Pl, R, P.Y, &Half);
        const double Extra = R == 0 ? 450.0 * (1.0 - WI::RampD(P.Y, Pl.GlenAt[0].Y - 300.0, Pl.GlenAt[0].Y - 1000.0)) : 0.0;
        if (WI::GlenDist(Pl, P) <= Half + 120.0 + Extra && Ground < Z) { OutZ = Z; return true; }
    }
    return false;
}
