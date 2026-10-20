// The east porte-cochere, rebuilt from the photos of the real one. See HouseSideCanopy.h.
#include "HouseSideCanopy.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"

namespace HouseSideCanopyImpl
{
constexpr double Pi = 3.14159265358979323846;
// Site: the house's east face, the drive (x 3450..4050) passing under the canopy, the side door at y -1143..-980.
constexpr float WallX = 3340.f;
constexpr float EastX = 4150.f;                   // outer face of the entablature
constexpr float Y0 = -1500.f, Y1 = -620.f;        // outer faces of the entablature
constexpr float Ground = -60.f;
constexpr float BeamT = 56.f;
constexpr float FriezeZ0 = 330.f, FriezeZ1 = 390.f, CorniceZ1 = 410.f, SoffitZ1 = 418.f, FasciaZ1 = 436.f;
constexpr float Over = 50.f;                      // roof overhang beyond the frieze face
constexpr float Pitch = .28f;
constexpr float ColX = EastX - BeamT * .5f;       // column centres sit in the east beam's centre line
constexpr float PlinthTop = 30.f, KneeTop = 0.f;
constexpr float RidgeY = (Y0 + Y1) * .5f;         // -1060, the door's centre line
const float PairY[2][2] = { { -1470.f, -1380.f }, { -650.f, -740.f } };   // the column pairs at the south-east and north-east corners
const float StoopY0 = -1240.f, StoopY1 = -880.f, StoopX1 = 3470.f;     // the landing at the door

const FName White(TEXT("SC_White")), Iron(TEXT("SC_Iron")), Glass(TEXT("SC_Glass")), WallGlass(TEXT("SC_WallGlass")), Yellow(TEXT("SC_Yellow")), Grey(TEXT("SC_Grey")),
    Sign(TEXT("SC_SignFDC")), Flash(TEXT("SC_Flash")), Brick(TEXT("EX_Brick")), Stone(TEXT("EX_Stone")), Concrete(TEXT("EX_Concrete")),
    Shingle(TEXT("EX_ShingleBlack"));

float PeriodFor(FName K)
{
    if (K == Brick) return 192.f;      // HouseExterior's BrickUV
    if (K == Concrete) return 240.f;   // ConcreteUV
    return 100.f;
}

/** Writes triangles into FHallMesh sections with world-planar UVs (same axes as FHallMesh::PlanarAxes) and auto winding. */
struct FWriter
{
    FHallMesh& M;
    explicit FWriter(FHallMesh& In) : M(In) {}
    static void Axes(const FVector& N, FVector& U, FVector& V)
    {
        if (FMath::Abs(N.Z) > .7f) { U = FVector(1, 0, 0); V = FVector::CrossProduct(N, U).GetSafeNormal(); }
        else { U = FVector::CrossProduct(N, FVector::UpVector).GetSafeNormal(); V = FVector(0, 0, -1); }
    }
    void Push(FName K, const FVector P[3], const FVector N[3], const FVector2D* UV)
    {
        FHallSection& S = M.Sec(K);
        const int32 Base = S.V.Num();
        const float Period = PeriodFor(K);
        for (int32 I = 0; I < 3; ++I)
        {
            FVector U, V; Axes(N[I], U, V);
            S.V.Add(P[I]); S.N.Add(N[I]);
            S.UV.Add(UV ? UV[I] : FVector2D(FVector::DotProduct(P[I], U) / Period, FVector::DotProduct(P[I], V) / Period));
            S.T.Add(FProcMeshTangent(U, false));
        }
        S.I.Append({ Base, Base + 1, Base + 2 });
    }
    /** Triangle wound so its front faces its normals (UE front faces are wound opposite to the right-hand cross product). */
    void Emit(FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& NA, const FVector& NB, const FVector& NC, const FVector2D* UV = nullptr)
    {
        const FVector G = FVector::CrossProduct(B - A, C - A);
        if (G.SizeSquared() < 1e-10) return;
        if (FVector::DotProduct(G, NA + NB + NC) < 0) { const FVector P[3] = { A, B, C }, N[3] = { NA, NB, NC }; Push(K, P, N, UV); }
        else
        {
            const FVector P[3] = { A, C, B }, N[3] = { NA, NC, NB };
            FVector2D U2[3];
            if (UV) { U2[0] = UV[0]; U2[1] = UV[2]; U2[2] = UV[1]; }
            Push(K, P, N, UV ? U2 : nullptr);
        }
    }
    void Tri(FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& N) { Emit(K, A, B, C, N, N, N); }
    void Quad(FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& N) { Tri(K, A, B, C, N); Tri(K, A, C, D, N); }
    void QuadUV(FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& N, const FVector2D UV[4])
    {
        const FVector2D T0[3] = { UV[0], UV[1], UV[2] }, T1[3] = { UV[0], UV[2], UV[3] };
        Emit(K, A, B, C, N, N, N, T0);
        Emit(K, A, C, D, N, N, N, T1);
    }
    /** Sloped face: its normal is taken from the corners and made to point up. */
    void Slope(FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
    {
        FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        if (N.IsNearlyZero()) N = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
        if (N.Z < 0) N = -N;
        if ((C - D).IsNearlyZero()) Tri(K, A, B, C, N); else Quad(K, A, B, C, D, N);
    }
    void BoxAx(FName K, const FVector& C, const FVector& AX, const FVector& AY, const FVector& AZ, const FVector& H)
    {
        const FVector Ax[3] = { AX, AY, AZ };
        for (int32 A = 0; A < 3; ++A)
            for (int32 Sg = 0; Sg < 2; ++Sg)
            {
                const FVector N = Ax[A] * (Sg ? -1.0 : 1.0);
                const FVector U = Ax[(A + 1) % 3] * H[(A + 1) % 3], W = Ax[(A + 2) % 3] * H[(A + 2) % 3];
                const FVector Ctr = C + N * H[A];
                Quad(K, Ctr - U - W, Ctr + U - W, Ctr + U + W, Ctr - U + W, N);
            }
    }
    /** Axis-aligned box from world min/max. */
    void Box(FName K, const FVector& Min, const FVector& Max) { BoxAx(K, (Min + Max) * .5f, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), (Max - Min) * .5f); }
    /** Box whose local x runs from A to B (rails, ridge caps): Width across, Height up. */
    void Bar(FName K, const FVector& A, const FVector& B, float Width, float Height)
    {
        FVector X = B - A;
        const double L = X.Size();
        if (L < 1e-6) return;
        X /= L;
        FVector Y = FVector::CrossProduct(FVector::UpVector, X).GetSafeNormal();
        if (Y.IsNearlyZero()) Y = FVector(0, 1, 0);
        const FVector Z = FVector::CrossProduct(X, Y);
        BoxAx(K, (A + B) * .5f, X, Y, Z, FVector(L * .5, Width * .5, Height * .5));
    }
    /** Frustum (cylinder when RA == RB) from A to B. */
    void Cone(FName K, const FVector& A, const FVector& B, double RA, double RB, int32 Seg, bool bCapA = true, bool bCapB = true)
    {
        FVector Ax = B - A;
        const double L = Ax.Size();
        if (L < 1e-6) return;
        Ax /= L;
        FVector X = FVector::CrossProduct(Ax, FMath::Abs(Ax.Z) < .9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
        const FVector Y = FVector::CrossProduct(Ax, X);
        const double Tilt = (RA - RB) / L;
        for (int32 S = 0; S < Seg; ++S)
        {
            const double A0 = 2 * Pi * S / Seg, A1 = 2 * Pi * (S + 1) / Seg;
            const FVector D0 = X * FMath::Cos(A0) + Y * FMath::Sin(A0), D1 = X * FMath::Cos(A1) + Y * FMath::Sin(A1);
            const FVector N0 = (D0 + Ax * Tilt).GetSafeNormal(), N1 = (D1 + Ax * Tilt).GetSafeNormal();
            Emit(K, A + D0 * RA, A + D1 * RA, B + D1 * RB, N0, N1, N1);
            Emit(K, A + D0 * RA, B + D1 * RB, B + D0 * RB, N0, N1, N0);
        }
        for (int32 E = 0; E < 2; ++E)
        {
            if (!(E ? bCapB : bCapA)) continue;
            const double R = E ? RB : RA;
            if (R < 1e-3) continue;
            const FVector C = E ? B : A, N = E ? Ax : -Ax;
            for (int32 S = 0; S < Seg; ++S)
            {
                const double A0 = 2 * Pi * S / Seg, A1 = 2 * Pi * (S + 1) / Seg;
                Tri(K, C, C + (X * FMath::Cos(A0) + Y * FMath::Sin(A0)) * R, C + (X * FMath::Cos(A1) + Y * FMath::Sin(A1)) * R, N);
            }
        }
    }
    /** Surface of revolution about the vertical axis through (CX, CY): Profile holds (radius, z), smooth across the rings. */
    void Lathe(FName K, double CX, double CY, const TArray<FVector2D>& P, int32 Seg)
    {
        const int32 N = P.Num();
        auto Unit = [](const FVector2D& V) { return V / FMath::Max(1e-6, V.Size()); };
        auto EdgeN = [&](int32 E) { const FVector2D D = P[E + 1] - P[E]; return Unit(FVector2D(D.Y, -D.X)); };   // (radial, axial)
        const float Period = PeriodFor(K);
        for (int32 E = 0; E + 1 < N; ++E)
        {
            if ((P[E + 1] - P[E]).Size() < 1e-6) continue;
            FVector2D NA = EdgeN(E), NB = EdgeN(E);
            if (E > 0) NA = Unit(NA + EdgeN(E - 1));
            if (E + 2 < N) NB = Unit(NB + EdgeN(E + 1));
            for (int32 S = 0; S < Seg; ++S)
            {
                const double A0 = 2 * Pi * S / Seg, A1 = 2 * Pi * (S + 1) / Seg;
                const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
                auto At = [&](const FVector& D, const FVector2D& Q) { return FVector(CX + D.X * Q.X, CY + D.Y * Q.X, Q.Y); };
                auto Nm = [&](const FVector& D, const FVector2D& Q) { return (D * Q.X + FVector(0, 0, Q.Y)).GetSafeNormal(); };
                const double U0 = A0 * P[E].X / Period, U1 = A1 * P[E].X / Period;
                const FVector2D T0[3] = { FVector2D(U0, P[E].Y / Period), FVector2D(U1, P[E].Y / Period), FVector2D(U1, P[E + 1].Y / Period) };
                const FVector2D T1[3] = { FVector2D(U0, P[E].Y / Period), FVector2D(U1, P[E + 1].Y / Period), FVector2D(U0, P[E + 1].Y / Period) };
                Emit(K, At(D0, P[E]), At(D1, P[E]), At(D1, P[E + 1]), Nm(D0, NA), Nm(D1, NA), Nm(D1, NB), T0);
                Emit(K, At(D0, P[E]), At(D1, P[E + 1]), At(D0, P[E + 1]), Nm(D0, NA), Nm(D1, NB), Nm(D0, NB), T1);
            }
        }
    }
};

/** A Tuscan column: square plinth, torus base, tapered shaft, neck rings, echinus and abacus, from the plinth cap to the frieze. */
void Column(FWriter& W, double CX, double CY)
{
    W.Box(White, FVector(CX - 25, CY - 25, PlinthTop), FVector(CX + 25, CY + 25, PlinthTop + 6));
    W.Lathe(White, CX, CY, { FVector2D(24, 36), FVector2D(27, 38), FVector2D(28.5, 41), FVector2D(27.5, 44), FVector2D(24.5, 47), FVector2D(23.5, 50),
        FVector2D(23.5, 58), FVector2D(22.8, 150), FVector2D(22, 230), FVector2D(21, 300), FVector2D(21, 305), FVector2D(22.5, 307), FVector2D(22.5, 310),
        FVector2D(21, 312), FVector2D(21, 315), FVector2D(23.5, 317), FVector2D(26.5, 321), FVector2D(28, 325), FVector2D(28, 326) }, 40);
    W.Box(White, FVector(CX - 28, CY - 28, 326), FVector(CX + 28, CY + 28, FriezeZ0 + 1));
}

/** Raised rails and stiles, 3 cm proud of a frieze face, framing N recessed panels. */
void Panels(FWriter& W, const FVector& Origin, const FVector& Along, const FVector& Out, float Length, int32 N)
{
    const FVector Up(0, 0, 1);
    auto Strip = [&](float S0, float S1, float Z0, float Z1)
    {
        W.BoxAx(White, Origin + Along * ((S0 + S1) * .5f) + Out * 1.5f + Up * ((Z0 + Z1) * .5f), Along, Out, Up, FVector((S1 - S0) * .5f, 1.5f, (Z1 - Z0) * .5f));
    };
    Strip(0, Length, FriezeZ0, FriezeZ0 + 10);
    Strip(0, Length, FriezeZ1 - 10, FriezeZ1);
    const float Stile = 12.f, PanelW = (Length - Stile * (N + 1)) / N;
    for (int32 K = 0; K <= N; ++K) { const float S0 = K * (PanelW + Stile); Strip(S0, S0 + Stile, FriezeZ0 + 10, FriezeZ1 - 10); }
}

void Entablature(FWriter& W)
{
    // The ring beam on the three free sides (the house wall closes the fourth).
    W.Box(White, FVector(WallX, Y0, FriezeZ0), FVector(EastX, Y0 + BeamT, FriezeZ1));
    W.Box(White, FVector(WallX, Y1 - BeamT, FriezeZ0), FVector(EastX, Y1, FriezeZ1));
    W.Box(White, FVector(EastX - BeamT, Y0, FriezeZ0), FVector(EastX, Y1, FriezeZ1));
    // Paneled friezes outside and in.
    Panels(W, FVector(WallX, Y0, 0), FVector(1, 0, 0), FVector(0, -1, 0), EastX - WallX, 4);
    Panels(W, FVector(EastX, Y0, 0), FVector(0, 1, 0), FVector(1, 0, 0), Y1 - Y0, 4);
    Panels(W, FVector(EastX, Y1, 0), FVector(-1, 0, 0), FVector(0, 1, 0), EastX - WallX, 4);
    Panels(W, FVector(WallX, Y0 + BeamT, 0), FVector(1, 0, 0), FVector(0, 1, 0), EastX - BeamT - WallX, 4);
    Panels(W, FVector(EastX - BeamT, Y0 + BeamT, 0), FVector(0, 1, 0), FVector(-1, 0, 0), Y1 - Y0 - 2 * BeamT, 4);
    Panels(W, FVector(WallX, Y1 - BeamT, 0), FVector(1, 0, 0), FVector(0, -1, 0), EastX - BeamT - WallX, 4);
    // Two-step cornice.
    for (int32 Step = 0; Step < 2; ++Step)
    {
        const float P = Step ? 14.f : 8.f, Z0 = Step ? 398.f : FriezeZ1, Z1 = Step ? CorniceZ1 : 398.f;
        W.Box(White, FVector(WallX, Y0 - P, Z0), FVector(EastX + P, Y0 + BeamT, Z1));
        W.Box(White, FVector(WallX, Y1 - BeamT, Z0), FVector(EastX + P, Y1 + P, Z1));
        W.Box(White, FVector(EastX - BeamT, Y0 - P, Z0), FVector(EastX + P, Y1 + P, Z1));
    }
    // Flat ceiling inside the ring, the soffit over everything out to the eaves, and the fascia boards.
    W.Box(White, FVector(WallX, Y0 + BeamT, FriezeZ1), FVector(EastX - BeamT, Y1 - BeamT, CorniceZ1 + 2));
    W.Box(White, FVector(WallX, Y0 - Over - 4, CorniceZ1), FVector(EastX + Over + 4, Y1 + Over + 4, SoffitZ1));
    W.Box(White, FVector(WallX, Y0 - Over - 4, CorniceZ1), FVector(EastX + Over + 4, Y0 - Over, FasciaZ1));
    W.Box(White, FVector(WallX, Y1 + Over, CorniceZ1), FVector(EastX + Over + 4, Y1 + Over + 4, FasciaZ1));
    W.Box(White, FVector(EastX + Over, Y0 - Over - 4, CorniceZ1), FVector(EastX + Over + 4, Y1 + Over + 4, FasciaZ1));
}

/** Hip roof: ridge runs from the house wall out toward the east hip; black shingles, metal caps, flashing at the wall. */
void Roof(FWriter& W)
{
    const float YS = Y0 - Over, YN = Y1 + Over, XE = EastX + Over, ZE = FasciaZ1;
    const float Half = (YN - YS) * .5f, ZR = ZE + Half * Pitch, XR = XE - Half;
    W.Slope(Shingle, FVector(WallX, YS, ZE), FVector(XE, YS, ZE), FVector(XR, RidgeY, ZR), FVector(WallX, RidgeY, ZR));
    W.Slope(Shingle, FVector(XE, YN, ZE), FVector(WallX, YN, ZE), FVector(WallX, RidgeY, ZR), FVector(XR, RidgeY, ZR));
    W.Slope(Shingle, FVector(XE, YS, ZE), FVector(XE, YN, ZE), FVector(XR, RidgeY, ZR), FVector(XR, RidgeY, ZR));
    W.Bar(Flash, FVector(WallX, RidgeY, ZR + 2), FVector(XR, RidgeY, ZR + 2), 9, 3);
    W.Bar(Flash, FVector(XR, RidgeY, ZR + 2), FVector(XE, YS, ZE + 2), 9, 3);
    W.Bar(Flash, FVector(XR, RidgeY, ZR + 2), FVector(XE, YN, ZE + 2), 9, 3);
    W.Bar(Flash, FVector(WallX + 2, YS, ZE + 5), FVector(WallX + 2, RidgeY, ZR + 5), 3, 9);
    W.Bar(Flash, FVector(WallX + 2, RidgeY, ZR + 5), FVector(WallX + 2, YN, ZE + 5), 3, 9);
}

/** Brick stoop at the side door: three risers on the drive side and on both ends (the side path runs along the wall through it),
 *  white railings flanking the drive-side run. */
void Stoop(FWriter& W)
{
    auto Step = [&](float X1, float YA, float YB, float Top)
    {
        W.Box(Brick, FVector(WallX, YA, -100), FVector(X1, YB, Top - 4));
        W.Box(Concrete, FVector(WallX, YA - 1.5f, Top - 4), FVector(X1 + 1.5f, YB + 1.5f, Top));
    };
    Step(StoopX1 + 60, StoopY0 - 60, StoopY1 + 60, -41);
    Step(StoopX1 + 30, StoopY0 - 30, StoopY1 + 30, -23);
    Step(StoopX1, StoopY0, StoopY1, -5);
    for (float Y : { StoopY0 - 6, StoopY1 + 6 })
    {
        const float X0 = StoopX1 + 4, X1 = StoopX1 + 62;
        W.Box(White, FVector(X0 - 2, Y - 2, -5), FVector(X0 + 2, Y + 2, 88));
        W.Box(White, FVector(X1 - 2, Y - 2, -41), FVector(X1 + 2, Y + 2, 52));
        W.Bar(White, FVector(X0, Y, 86), FVector(X1, Y, 50), 4, 4);
        W.Bar(White, FVector(X0, Y, 46), FVector(X1, Y, 10), 3, 3);
    }
}

void WallLantern(FWriter& W, float Y)
{
    W.Box(Iron, FVector(WallX, Y - 5, 196), FVector(WallX + 6, Y + 5, 212));
    W.Box(Iron, FVector(WallX + 5, Y - 9, 184), FVector(WallX + 23, Y + 9, 187));
    for (float DX : { 7.f, 21.f }) for (float DY : { -8.f, 8.f }) W.Box(Iron, FVector(WallX + DX - 1, Y + DY - 1, 187), FVector(WallX + DX + 1, Y + DY + 1, 214));
    W.Box(WallGlass, FVector(WallX + 7.5f, Y - 7.5f, 188), FVector(WallX + 20.5f, Y + 7.5f, 212));
    W.Box(Iron, FVector(WallX + 4, Y - 10, 214), FVector(WallX + 24, Y + 10, 218));
    W.Box(Iron, FVector(WallX + 9, Y - 5, 218), FVector(WallX + 19, Y + 5, 224));
    W.Cone(Iron, FVector(WallX + 14, Y, 224), FVector(WallX + 14, Y, 231), 2.5, .6, 10);
}

void DoorDressing(FWriter& W)
{
    WallLantern(W, -1215.f);
    WallLantern(W, -910.f);
    // Mailbox, the fire-department-connection sign and the entry keypad.
    W.Box(Iron, FVector(WallX, -1250, 108), FVector(WallX + 12, -1226, 140));
    W.Box(Iron, FVector(WallX, -1251, 138), FVector(WallX + 13, -1225, 141));
    W.Box(White, FVector(WallX, -1300, 148), FVector(WallX + 1.5f, -1262, 174));
    const FVector2D UV[4] = { FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0) };
    W.QuadUV(Sign, FVector(WallX + 1.6f, -1262, 148), FVector(WallX + 1.6f, -1300, 148), FVector(WallX + 1.6f, -1300, 174), FVector(WallX + 1.6f, -1262, 174), FVector(1, 0, 0), UV);
    W.Box(Grey, FVector(WallX, -968, 118), FVector(WallX + 3, -952, 136));
}

void MeterAndBollards(FWriter& W)
{
    W.Box(Grey, FVector(WallX, -748, 60), FVector(WallX + 20, -712, 98));
    W.Cone(Grey, FVector(WallX + 10, -730, Ground), FVector(WallX + 10, -730, 60), 2.5, 2.5, 10);
    W.Cone(Grey, FVector(WallX + 20, -730, 80), FVector(WallX + 30, -730, 80), 5, 5, 10);
    for (float Y : { -775.f, -730.f, -685.f })
    {
        W.Cone(Yellow, FVector(3466, Y, Ground), FVector(3466, Y, 92), 6, 6, 14);
        W.Cone(Yellow, FVector(3466, Y, 92), FVector(3466, Y, 97), 6, 2.5, 14);
    }
}

void HangingLantern(FWriter& W)
{
    const double X = 3750, Y = RidgeY;
    W.Cone(Iron, FVector(X, Y, FriezeZ1 + 1), FVector(X, Y, 366), 1.2, 1.2, 8);
    W.Box(Iron, FVector(X - 11, Y - 11, 363), FVector(X + 11, Y + 11, 366));
    for (float SX : { -9.f, 9.f }) for (float SY : { -9.f, 9.f }) W.Box(Iron, FVector(X + SX - .8, Y + SY - .8, 334), FVector(X + SX + .8, Y + SY + .8, 363));
    W.Box(Glass, FVector(X - 8.5, Y - 8.5, 336), FVector(X + 8.5, Y + 8.5, 361));
    W.Box(Iron, FVector(X - 10, Y - 10, 332), FVector(X + 10, Y + 10, 334));
    W.Cone(Iron, FVector(X, Y, 332), FVector(X, Y, 326), 3, .5, 10);
}
}

namespace HouseSideCanopy
{
using namespace HouseSideCanopyImpl;

void CreateLook(UHouseBlackHallSubsystem* L)
{
    if (!L || L->Materials.Contains(White)) return;
    HallMakePlain(L, White, FLinearColor(.83f, .825f, .79f), .55f);
    HallMakePlain(L, Iron, FLinearColor(.02f, .02f, .022f), .5f, .5f);
    HallMakePlain(L, Glass, FLinearColor(.95f, .75f, .45f), .25f, 0.f, FLinearColor(6.f, 3.6f, 1.4f));
    // The door's wall lanterns have no light of their own, so their glass burns brighter.
    HallMakePlain(L, WallGlass, FLinearColor(.95f, .75f, .45f), .25f, 0.f, FLinearColor(22.f, 12.5f, 4.6f));
    HallMakePlain(L, Yellow, FLinearColor(.72f, .52f, .04f), .5f, .2f);
    HallMakePlain(L, Grey, FLinearColor(.45f, .45f, .47f), .5f, .6f);
    HallMakePlain(L, Flash, FLinearColor(.08f, .08f, .085f), .4f, .6f);
    // A generic fire-department-connection sign: red lettering on white, red border.
    FHallImage Img; Img.Init(128, 80, FLinearColor(.55f, .03f, .02f));
    Img.Rect(FVector2D(4, 4), FVector2D(124, 76), FLinearColor(.88f, .88f, .85f));
    FHallTextStyle S; S.Height = 17; S.Stroke = 3.f; S.Spacing = .2f;
    const FLinearColor Red(.55f, .03f, .02f);
    HallDrawText(Img, TEXT("FIRE DEPT"), FVector2D((128 - HallTextWidth(TEXT("FIRE DEPT"), S)) * .5f, 34), S, Red);
    S.Height = 13; S.Stroke = 2.6f;
    HallDrawText(Img, TEXT("CONNECTION"), FVector2D((128 - HallTextWidth(TEXT("CONNECTION"), S)) * .5f, 62), S, Red);
    if (UTexture2D* T = HallMakeTexture(Img, true, false, TEXT("T_SC_SignFDC"), false))
    {
        L->Textures.Add(TEXT("T_SC_SignFDC"), T);
        HallMakeTextured(L, Sign, T, nullptr, .5f, FLinearColor::White, 0.f);
    }
}

void Build(FHallMesh& House)
{
    FWriter W(House);
    // Concrete pad under the whole canopy (the drive's asphalt runs up to it on both sides).
    W.Box(Concrete, FVector(WallX, Y0, -100), FVector(EastX, Y1, -56));
    // Brick plinths under the column pairs, the knee wall between them, and the columns.
    for (int32 Corner = 0; Corner < 2; ++Corner)
    {
        const float YA = FMath::Min(PairY[Corner][0], PairY[Corner][1]), YB = FMath::Max(PairY[Corner][0], PairY[Corner][1]);
        W.Box(Brick, FVector(4082, YA - 34, -420), FVector(4154, YB + 34, PlinthTop - 8));
        W.Box(Stone, FVector(4076, YA - 40, PlinthTop - 8), FVector(4160, YB + 40, PlinthTop));
        for (int32 K = 0; K < 2; ++K) Column(W, ColX, PairY[Corner][K]);
    }
    W.Box(Brick, FVector(4100, PairY[0][1] + 34, -420), FVector(4136, PairY[1][1] - 34, KneeTop - 8));
    W.Box(Stone, FVector(4094, PairY[0][1] + 34, KneeTop - 8), FVector(4142, PairY[1][1] - 34, KneeTop));
    Entablature(W);
    Roof(W);
    Stoop(W);
    DoorDressing(W);
    MeterAndBollards(W);
    HangingLantern(W);
}

FVector LanternLight() { return FVector(3750, RidgeY, 347); }
}
