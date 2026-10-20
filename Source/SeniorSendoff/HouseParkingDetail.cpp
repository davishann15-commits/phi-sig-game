// Lived-in detail for the west parking lot. See HouseParkingDetail.h.
// Everything is placed from the same bay layout HouseParking paints, and keeps clear of the parking review's routes,
// floor samples, reserved bays and loading aisle. No lights are added.
#include "HouseParkingDetail.h"
#include "HouseParkingLot.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "Engine/Texture2D.h"

namespace HouseLotDetailImpl
{
constexpr float Surface = HouseParking::Surface;   // paving top
constexpr float Grass = -60.f;                       // lawn and mulch grade around the lot
constexpr float Mulch = -55.f;                       // top of HouseParking's mulch beds
constexpr double LotPi = 3.14159265358979323846;

// The bays exactly as HouseParking lays them out (HouseParkingLot.cpp, Bays()).
struct FBay { float A, B; bool bUpper, bAccessible; };
const FBay Bays[] = {
    { -6860.f, -6530.f, true, false }, { -6530.f, -6200.f, true, false }, { -6200.f, -5870.f, true, false },
    { -5870.f, -5540.f, true, false }, { -5540.f, -5210.f, true, false }, { -5210.f, -4880.f, true, false },
    { -4880.f, -4480.f, true, true }, { -4160.f, -3760.f, true, true },
    { -6860.f, -6530.f, false, false }, { -6530.f, -6200.f, false, false }, { -6200.f, -5870.f, false, false },
    { -5180.f, -4880.f, false, false },
    { -4880.f, -4510.f, false, false }, { -4510.f, -4140.f, false, false }, { -4140.f, -3770.f, false, false } };
constexpr float UpperAisleY = -950.f, LowerAisleY = -1850.f;   // bay ends on the drive aisle

/** Small deterministic generator, so the layout is identical on every machine. */
struct FLotRng
{
    uint32 S;
    explicit FLotRng(uint32 Seed) : S(Seed * 747796405u + 2891336453u) { if (!S) S = 1; }
    uint32 Next() { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
    double Frac() { return (Next() & 0xffffff) / 16777216.0; }
    double Range(double A, double B) { return A + (B - A) * Frac(); }
    bool Chance(double P) { return Frac() < P; }
};

/** Inside the open paving (clear of the west island, curbs and the blue accessible bays and aisle). */
bool OnPaving(double X, double Y, double Margin = 0.0)
{
    if (X < -6890 + Margin || X > -3727 - Margin || Y < -2474 + Margin || Y > -327 - Margin) return false;
    if (X > -4885 && X < -3755 && Y > -955) return false;
    return true;
}

struct FLotWriter
{
    FHallMesh& M;
    explicit FLotWriter(FHallMesh& In) : M(In) {}
    static FVector TangentFor(const FVector& N)
    {
        FVector T = FVector::CrossProduct(N, FMath::Abs(N.Z) < .9 ? FVector::UpVector : FVector::ForwardVector);
        if (!T.Normalize()) T = FVector::ForwardVector;
        return T;
    }
    void Push(FName K, const FVector P[3], const FVector N[3], const FVector2D* UV)
    {
        FHallSection& S = M.Sec(K);
        const int32 Base = S.V.Num();
        for (int32 I = 0; I < 3; ++I)
        {
            S.V.Add(P[I]); S.N.Add(N[I]);
            S.UV.Add(UV ? UV[I] : FVector2D((P[I].X + P[I].Y * .3) / 100.0, (P[I].Y - P[I].Z) / 100.0));
            S.T.Add(FProcMeshTangent(TangentFor(N[I]), false));
        }
        S.I.Append({ Base, Base + 1, Base + 2 });
    }
    /** Triangle with per-corner normals, wound so its front faces them (UE front face: cross(B-A, C-A) points away). */
    void Emit(FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& NA, const FVector& NB, const FVector& NC, const FVector2D* UV = nullptr)
    {
        const FVector G = FVector::CrossProduct(B - A, C - A);
        if (G.SizeSquared() < 1e-10) return;
        if (FVector::DotProduct(G, NA + NB + NC) < 0)
        {
            const FVector P[3] = { A, B, C }, N[3] = { NA, NB, NC };
            Push(K, P, N, UV);
        }
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
    void Fan(FName K, const FVector& Ctr, const TArray<FVector>& Ring, const FVector& N)
    {
        for (int32 I = 0; I < Ring.Num(); ++I) Tri(K, Ctr, Ring[I], Ring[(I + 1) % Ring.Num()], N);
    }
    /** Box turned by Yaw degrees about Z. */
    void Box(FName K, const FVector& C, const FVector& H, double YawDeg = 0.0)
    {
        const double Y = FMath::DegreesToRadians(YawDeg);
        const FVector Ax[3] = { FVector(FMath::Cos(Y), FMath::Sin(Y), 0), FVector(-FMath::Sin(Y), FMath::Cos(Y), 0), FVector::UpVector };
        for (int32 A = 0; A < 3; ++A)
            for (int32 Sg = 0; Sg < 2; ++Sg)
            {
                const FVector N = Ax[A] * (Sg ? -1.0 : 1.0);
                const FVector U = Ax[(A + 1) % 3] * H[(A + 1) % 3], W = Ax[(A + 2) % 3] * H[(A + 2) % 3];
                const FVector Ctr = C + N * H[A];
                Quad(K, Ctr - U - W, Ctr + U - W, Ctr + U + W, Ctr - U + W, N);
            }
    }
    /** Frustum (cylinder when RA == RB) from A to B with smooth sides and optional caps. */
    void Cone(FName K, const FVector& A, const FVector& B, double RA, double RB, int32 Seg, bool bCapA = true, bool bCapB = true)
    {
        FVector Ax = B - A;
        const double L = Ax.Size();
        if (L < 1e-6) return;
        Ax /= L;
        const FVector X = TangentFor(Ax), Y = FVector::CrossProduct(Ax, X);
        const double Tilt = (RA - RB) / L;
        for (int32 S = 0; S < Seg; ++S)
        {
            const double A0 = 2 * LotPi * S / Seg, A1 = 2 * LotPi * (S + 1) / Seg;
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
            const FVector C = E ? B : A;
            TArray<FVector> Ring;
            for (int32 S = 0; S < Seg; ++S) { const double An = 2 * LotPi * S / Seg; Ring.Add(C + (X * FMath::Cos(An) + Y * FMath::Sin(An)) * R); }
            Fan(K, C, Ring, E ? Ax : -Ax);
        }
    }
    /** Straight stroke of width W from A to B lying in the plane with normal N, with round joints. */
    void Stroke(FName K, const FVector& A, const FVector& B, double W, const FVector& N)
    {
        FVector Side = FVector::CrossProduct(N, B - A);
        if (!Side.Normalize()) return;
        Side *= W * .5;
        Quad(K, A - Side, B - Side, B + Side, A + Side, N);
    }
    void Dot(FName K, const FVector& C, double R, const FVector& N, int32 Seg = 8)
    {
        const FVector X = TangentFor(N), Y = FVector::CrossProduct(N, X);
        TArray<FVector> Ring;
        for (int32 S = 0; S < Seg; ++S) { const double An = 2 * LotPi * S / Seg; Ring.Add(C + (X * FMath::Cos(An) + Y * FMath::Sin(An)) * R); }
        Fan(K, C, Ring, N);
    }
    /** Ribbon along a flat polyline (mitred joints), one width per point. */
    void Ribbon(FName K, const TArray<FVector>& P, const TArray<double>& W)
    {
        const int32 N = P.Num();
        if (N < 2) return;
        TArray<FVector> L, R;
        for (int32 I = 0; I < N; ++I)
        {
            FVector D0 = I > 0 ? P[I] - P[I - 1] : P[1] - P[0];
            FVector D1 = I + 1 < N ? P[I + 1] - P[I] : P[I] - P[I - 1];
            D0.Z = 0; D1.Z = 0; D0.Normalize(); D1.Normalize();
            const FVector N0(-D0.Y, D0.X, 0), N1(-D1.Y, D1.X, 0);
            FVector Mi = N0 + N1;
            if (!Mi.Normalize()) Mi = N0;
            const double Scale = 1.0 / FMath::Max(.5, FVector::DotProduct(Mi, N0));
            L.Add(P[I] + Mi * (W[I] * .5 * Scale));
            R.Add(P[I] - Mi * (W[I] * .5 * Scale));
        }
        for (int32 I = 0; I + 1 < N; ++I) Quad(K, L[I], L[I + 1], R[I + 1], R[I], FVector::UpVector);
    }
};

/** Irregular flat blob (stain, puddle, patch of wear): a smoothed random outline as a fan. */
void Blob(FLotWriter& W, FLotRng& R, FName K, double X, double Y, double Z, double RX, double RY, double YawRad, double Rough, int32 N = 16)
{
    TArray<double> Rad;
    for (int32 I = 0; I < N; ++I) Rad.Add(1.0 + Rough * (R.Frac() * 2.0 - 1.0));
    for (int32 Pass = 0; Pass < 2; ++Pass)
    {
        TArray<double> S = Rad;
        for (int32 I = 0; I < N; ++I) Rad[I] = .25 * S[(I + N - 1) % N] + .5 * S[I] + .25 * S[(I + 1) % N];
    }
    const double C = FMath::Cos(YawRad), Sn = FMath::Sin(YawRad);
    TArray<FVector> Ring;
    for (int32 I = 0; I < N; ++I)
    {
        const double A = 2 * LotPi * I / N;
        const double LX = FMath::Cos(A) * RX * Rad[I], LY = FMath::Sin(A) * RY * Rad[I];
        Ring.Add(FVector(X + LX * C - LY * Sn, Y + LX * Sn + LY * C, Z));
    }
    W.Fan(K, FVector(X, Y, Z), Ring, FVector::UpVector);
}

/** Wandering crack from (X, Y); sealed cracks are wide glossy tar bands, open ones thin dark lines. */
void Crack(FLotWriter& W, FLotRng& R, double X, double Y, double Heading, double Length, bool bSealed, int32 Depth = 0)
{
    TArray<FVector> P; TArray<double> Wd;
    const double Z = Surface + (bSealed ? .24 : .23);
    const double Width = bSealed ? R.Range(3.5, 6.0) : R.Range(.7, 1.3);
    double H = Heading, Run = 0;
    if (!OnPaving(X, Y, 10)) return;
    P.Add(FVector(X, Y, Z)); Wd.Add(Width * .4);
    while (Run < Length)
    {
        H += R.Range(-.28, .28);
        const double Step = R.Range(12.0, 28.0);
        const double NX = X + FMath::Cos(H) * Step, NY = Y + FMath::Sin(H) * Step;
        if (!OnPaving(NX, NY, 10)) break;
        X = NX; Y = NY; Run += Step;
        P.Add(FVector(X, Y, Z)); Wd.Add(Width * R.Range(.75, 1.15));
        if (Depth < 1 && !bSealed && R.Chance(.06)) Crack(W, R, X, Y, H + (R.Chance(.5) ? 1.0 : -1.0) * R.Range(.6, 1.2), Length * .35, false, Depth + 1);
    }
    if (P.Num() < 2) return;
    Wd.Last() = Width * .3;
    W.Ribbon(bSealed ? FName(TEXT("LOT_Seal")) : FName(TEXT("LOT_Crack")), P, Wd);
}

/** Text in the house stroke font laid on a plane; Origin is the left end of the baseline as the reader sees it. */
double TextWidth(const FString& Text, double Height, double WidthScale, double Spacing)
{
    const double U = Height / 6.0;
    double Width = 0;
    for (TCHAR Ch : Text) Width += (Ch == TEXT(' ') ? 2.6 : 4.0 * WidthScale) * U + Spacing * Height;
    return FMath::Max(0.0, Width - Spacing * Height);
}
void PaintText(FLotWriter& W, FName K, const FString& Text, const FVector& Origin, const FVector& RightDir, const FVector& UpDir, const FVector& N,
    double Height, double WidthScale, double Spacing, double StrokeW)
{
    const double U = Height / 6.0;
    double Cursor = 0;
    for (TCHAR Ch : Text)
    {
        for (const TArray<FVector2D>& Line : HallGlyph(Ch))
        {
            TArray<FVector> P;
            for (const FVector2D& G : Line) P.Add(Origin + RightDir * (Cursor + G.X * U * WidthScale) + UpDir * (G.Y * U));
            for (int32 I = 0; I + 1 < P.Num(); ++I) W.Stroke(K, P[I], P[I + 1], StrokeW, N);
            for (const FVector& V : P) W.Dot(K, V, StrokeW * .5, N);
        }
        Cursor += (Ch == TEXT(' ') ? 2.6 : 4.0 * WidthScale) * U + Spacing * Height;
    }
}

void Leaf(FLotWriter& W, FLotRng& R, double X, double Y, double Z)
{
    static const TCHAR* Keys[] = { TEXT("LOT_LeafOrange"), TEXT("LOT_LeafBrown"), TEXT("LOT_LeafYellow"), TEXT("LOT_LeafRed") };
    const double Pick = R.Frac();
    const FName K = Keys[Pick < .35 ? 0 : Pick < .65 ? 1 : Pick < .85 ? 2 : 3];
    const double A = R.Range(0, 2 * LotPi), L = R.Range(5.0, 9.5), Wd = L * R.Range(.5, .7);
    const FVector D(FMath::Cos(A), FMath::Sin(A), 0), S(-D.Y, D.X, 0);
    const FVector C(X, Y, Z);
    const FVector Tip = C + D * (L * .5) + FVector(0, 0, R.Range(.05, .35)), Base = C - D * (L * .5);
    const FVector Lf = C + S * (Wd * .5) - D * (L * .05) + FVector(0, 0, .08), Rt = C - S * (Wd * .5) - D * (L * .05) + FVector(0, 0, .08);
    W.Tri(K, Base, Lf, Tip, FVector::UpVector);
    W.Tri(K, Base, Tip, Rt, FVector::UpVector);
}

/** Leaves drifted along a straight edge, bunched by a slow wave so they gather in clumps. */
void LeafEdge(FLotWriter& W, FLotRng& R, const FVector2D& A, const FVector2D& B, const FVector2D& Inward, double Band, double PerMetre, double Z)
{
    const double Len = (B - A).Size();
    const int32 N = (int32)(Len / 100.0 * PerMetre);
    const double Phase = R.Range(0, 6.28), Freq = R.Range(.006, .012);
    for (int32 I = 0; I < N; ++I)
    {
        const double T = R.Frac(), S = T * Len;
        const double Bunch = .5 + .5 * FMath::Sin(S * Freq + Phase);
        if (R.Frac() > .25 + .75 * Bunch * Bunch) continue;
        const FVector2D P = A + (B - A) * T + Inward * (R.Frac() * R.Frac() * Band);
        Leaf(W, R, P.X, P.Y, Z + R.Range(0, .25));
    }
}

void BuildWear(FHallMesh& Mesh)
{
    FLotWriter W(Mesh);
    FLotRng R(20261012);
    const FVector Up = FVector::UpVector;

    // Tyre polish: the wheel tracks down the drive aisle are worn smoother (a sheen more than a colour), fading toward the ends.
    for (const double TrackY : { -1700.0, -1545.0, -1255.0, -1100.0 })
    {
        TArray<FVector> P; TArray<double> Wd;
        const double Y0 = TrackY + R.Range(-20, 20);
        for (double X = -6650; X <= -4250; X += 60)
        {
            const double Fade = FMath::Clamp(1.0 - FMath::Abs(X + 5450.0) / 1250.0, 0.0, 1.0);
            P.Add(FVector(X, Y0 + 12 * FMath::Sin(X * .004 + TrackY), Surface + .12));
            Wd.Add(3.0 + 26.0 * FMath::Sqrt(Fade) * R.Range(.8, 1.1));
        }
        W.Ribbon(TEXT("LOT_Polish"), P, Wd);
    }

    // Repairs: a utility trench across the aisle and a squarish cut, both outlined with crack sealant.
    auto Patch = [&](double X0, double X1, double Y0, double Y1)
    {
        const double Z = Surface + .15, ZS = Surface + .25;
        W.Quad(TEXT("LOT_Patch"), FVector(X0, Y0, Z), FVector(X1, Y0, Z), FVector(X1, Y1, Z), FVector(X0, Y1, Z), Up);
        const FVector C[4] = { FVector(X0, Y0, ZS), FVector(X1, Y0, ZS), FVector(X1, Y1, ZS), FVector(X0, Y1, ZS) };
        for (int32 I = 0; I < 4; ++I)
        {
            const FVector A = C[I], B = C[(I + 1) % 4];
            const FVector Wob = FVector(R.Range(-1.5, 1.5), R.Range(-1.5, 1.5), 0);
            W.Stroke(TEXT("LOT_Seal"), A, (A + B) * .5 + Wob, R.Range(3.0, 4.5), Up);
            W.Stroke(TEXT("LOT_Seal"), (A + B) * .5 + Wob, B, R.Range(3.0, 4.5), Up);
            W.Dot(TEXT("LOT_Seal"), A, 2.2, Up);
        }
    };
    Patch(-6131, -6077, -1838, -962);
    Patch(-5335, -5160, -1680, -1555);
    Patch(-4300, -4080, -2210, -2125);

    // Oil: every everyday bay has an old engine stain where cars stand, some a second drip further back, and the aisle
    // collects a few drips of its own.
    for (const FBay& B : Bays)
    {
        if (B.bAccessible) continue;
        const double CX = (B.A + B.B) * .5 + R.Range(-35, 35);
        const double CY = B.bUpper ? -505 + R.Range(-45, 45) : -2315 + R.Range(-45, 45);
        const double RX = R.Range(20, 36), RY = R.Range(28, 46), Yaw = R.Range(-.4, .4);
        Blob(W, R, TEXT("LOT_OilHalo"), CX + R.Range(-6, 6), CY + R.Range(-6, 6), Surface + .17, RX * R.Range(1.5, 1.9), RY * R.Range(1.4, 1.8), Yaw, .32);
        Blob(W, R, TEXT("LOT_Oil"), CX, CY, Surface + .20, RX, RY, Yaw, .38);
        for (int32 D = 0, ND = 2 + (int32)(R.Frac() * 3); D < ND; ++D)
            W.Dot(TEXT("LOT_Oil"), FVector(CX + R.Range(-55, 55), CY + R.Range(-70, 70), Surface + .21), R.Range(2.0, 6.5), Up);
        if (R.Chance(.55))
        {
            const double BY = CY + (B.bUpper ? -1.0 : 1.0) * R.Range(130, 190);
            Blob(W, R, TEXT("LOT_OilHalo"), CX + R.Range(-20, 20), BY, Surface + .17, R.Range(14, 24), R.Range(14, 26), R.Range(0, 3.0), .35);
            Blob(W, R, TEXT("LOT_Oil"), CX + R.Range(-8, 8), BY, Surface + .20, R.Range(7, 13), R.Range(8, 15), R.Range(0, 3.0), .4);
        }
    }
    for (int32 D = 0; D < 26; ++D)
    {
        const double X = R.Range(-6800, -3900), Y = R.Range(-1800, -1000);
        if (OnPaving(X, Y)) W.Dot(TEXT("LOT_Oil"), FVector(X, Y, Surface + .21), R.Range(2.0, 7.0), Up);
    }

    // Cracks: long sealed runs down the aisle, transverse cracks off the curbs, and open hairlines (with branches).
    for (int32 I = 0; I < 6; ++I) Crack(W, R, R.Range(-6750, -4300), R.Range(-1720, -1080), R.Chance(.5) ? 0.0 : LotPi, R.Range(450, 1200), true);
    for (int32 I = 0; I < 6; ++I) Crack(W, R, R.Range(-6800, -4950), -340.0, -.5 * LotPi + R.Range(-.35, .35), R.Range(180, 520), I % 2 == 0);
    for (int32 I = 0; I < 6; ++I) Crack(W, R, R.Range(-6800, -3800), -2462.0, .5 * LotPi + R.Range(-.35, .35), R.Range(180, 520), I % 2 == 1);
    for (int32 I = 0; I < 10; ++I) Crack(W, R, R.Range(-6850, -3800), R.Range(-2400, -400), R.Range(0, 2 * LotPi), R.Range(120, 380), false);
    for (int32 I = 0; I < 14; ++I)   // a patch of alligator cracking where trucks turn in front of the empty lower bay
    {
        const double X = -6365 + R.Range(-80, 80), Y = -2330 + R.Range(-70, 70);
        Crack(W, R, X, Y, R.Range(0, 2 * LotPi), R.Range(30, 70), false, 1);
    }

    // Drains on the aisle crown with standing water, and two more puddles in the low corners.
    auto Puddle = [&](double X, double Y, double RX, double RY, double Yaw)
    {
        Blob(W, R, TEXT("LOT_Wet"), X, Y, Surface + .14, RX * 1.25, RY * 1.3, Yaw, .3, 20);
        Blob(W, R, TEXT("LOT_Puddle"), X, Y, Surface + .16, RX, RY, Yaw, .34, 20);
    };
    for (const FVector2D& G : { FVector2D(-6450, -1400), FVector2D(-4700, -1400) })
    {
        if (G.X < -5000) Puddle(G.X + 18, G.Y - 6, 95, 58, .2);
        const double Z0 = Surface + .22, Z1 = Surface + .27;
        W.Quad(TEXT("LOT_Iron"), FVector(G.X - 36, G.Y - 22, Z0), FVector(G.X + 36, G.Y - 22, Z0), FVector(G.X + 36, G.Y + 22, Z0), FVector(G.X - 36, G.Y + 22, Z0), Up);
        for (int32 S = 0; S < 8; ++S)
        {
            const double SX = G.X - 28 + S * 8;
            W.Quad(TEXT("LOT_Slot"), FVector(SX - 2.4, G.Y - 16, Z1), FVector(SX + 2.4, G.Y - 16, Z1), FVector(SX + 2.4, G.Y + 16, Z1), FVector(SX - 2.4, G.Y + 16, Z1), Up);
        }
    }
    Puddle(-3880, -2380, 70, 46, -.3);
    Puddle(-6300, -2425, 62, 38, .5);

    // Worn paint: tyres have smudged about half the bay lines where cars swing in from the aisle.
    {
        TArray<double> Seen;
        for (const FBay& B : Bays)
        {
            if (B.bAccessible) continue;
            for (const double LX : { (double)B.A, (double)B.B })
            {
                bool bDup = false;
                for (double S : Seen) bDup |= FMath::Abs(S - LX + (B.bUpper ? 0.0 : 10000.0)) < 1.0;
                Seen.Add(LX - (B.bUpper ? 0.0 : 10000.0));
                if (bDup || !R.Chance(.6)) continue;
                const double Y = B.bUpper ? UpperAisleY + R.Range(18, 70) : LowerAisleY - R.Range(18, 70);
                Blob(W, R, TEXT("LOT_Scuff"), LX + R.Range(-2, 2), Y, Surface + .42, R.Range(5, 9), R.Range(14, 30), R.Range(-.25, .25), .45, 12);
            }
        }
    }

    // Bay numbers at each everyday bay's aisle end, readable as you pull in.
    {
        int32 Number = 0;
        for (const FBay& B : Bays)
        {
            if (B.bAccessible) continue;
            ++Number;
            const FString T = FString::Printf(TEXT("%d"), Number);
            const double H = 42.0, Wd = TextWidth(T, H, 1.0, .22), CX = (B.A + B.B) * .5;
            const FVector RightDir = B.bUpper ? FVector(-1, 0, 0) : FVector(1, 0, 0), UpDir = B.bUpper ? FVector(0, 1, 0) : FVector(0, -1, 0);
            const FVector Origin(CX - RightDir.X * Wd * .5, B.bUpper ? UpperAisleY + 25.0 : LowerAisleY - 25.0, Surface + .32);
            PaintText(W, TEXT("EX_Line"), T, Origin, RightDir, UpDir, Up, H, 1.0, .22, 5.0);
        }
    }

    // Road text in the driveway: SLOW for cars coming in on the west half, STOP and a stop bar for cars leaving on the east half.
    {
        const double H = 150.0, WS = .45, Sp = .1, SW = 10.0;
        const double WSlow = TextWidth(TEXT("SLOW"), H, WS, Sp), WStop = TextWidth(TEXT("STOP"), H, WS, Sp);
        PaintText(W, TEXT("EX_Line"), TEXT("SLOW"), FVector(-5650 + WSlow * .5, -2860, Surface + .32), FVector(-1, 0, 0), FVector(0, 1, 0), Up, H, WS, Sp, SW);
        PaintText(W, TEXT("EX_Line"), TEXT("STOP"), FVector(-5350 - WStop * .5, -2330, Surface + .32), FVector(1, 0, 0), FVector(0, -1, 0), Up, H, WS, Sp, SW);
        const double Z = Surface + .32;
        W.Quad(TEXT("EX_Line"), FVector(-5492, -2545, Z), FVector(-5212, -2545, Z), FVector(-5212, -2515, Z), FVector(-5492, -2515, Z), Up);
    }

    // Fire-lane curb along the east edge: red paint on the face, bevel and top, worn through in places, with stencils.
    {
        const FVector NFace(-1, 0, 0), NBevel = FVector(-1, 0, 1).GetSafeNormal();
        double Y = -2470;
        while (Y < -335)
        {
            const double Len = R.Range(140, 420), Y1 = FMath::Min(-335.0, Y + Len);
            const FName K = R.Chance(.2) ? FName(TEXT("LOT_CurbRedWorn")) : FName(TEXT("LOT_CurbRed"));
            W.Quad(K, FVector(-3723.2, Y, Surface + .1), FVector(-3723.2, Y1, Surface + .1), FVector(-3723.2, Y1, -44.5), FVector(-3723.2, Y, -44.5), NFace);
            const FVector BO = NBevel * .15;
            W.Quad(K, FVector(-3723, Y, -44.5) + BO, FVector(-3723, Y1, -44.5) + BO, FVector(-3721.5, Y1, -43) + BO, FVector(-3721.5, Y, -43) + BO, NBevel);
            W.Quad(K, FVector(-3721.5, Y, -42.85), FVector(-3721.5, Y1, -42.85), FVector(-3701.5, Y1, -42.85), FVector(-3701.5, Y, -42.85), Up);
            Y = Y1 + R.Range(4, 30);
        }
        for (double TY = -2300; TY < -500; TY += 520)
            PaintText(W, TEXT("EX_Line"), TEXT("FIRE LANE"), FVector(-3723.35, TY, -55.2), FVector(0, 1, 0), FVector(0, 0, 1), NFace, 8.0, .85, .25, 1.3);
    }

    // Fallen leaves: drifts along every curb, piles in the corners, a scatter over the paving and on the walk to the house.
    {
        const double Z = Surface + .47;
        LeafEdge(W, R, FVector2D(-6880, -338), FVector2D(-4500, -338), FVector2D(0, -1), 22, 7, Z);
        LeafEdge(W, R, FVector2D(-4140, -338), FVector2D(-3740, -338), FVector2D(0, -1), 22, 7, Z);
        LeafEdge(W, R, FVector2D(-6880, -2462), FVector2D(-5820, -2462), FVector2D(0, 1), 26, 8, Z);
        LeafEdge(W, R, FVector2D(-5180, -2462), FVector2D(-3740, -2462), FVector2D(0, 1), 26, 8, Z);
        LeafEdge(W, R, FVector2D(-3738, -2455), FVector2D(-3738, -965), FVector2D(-1, 0), 24, 8, Z);
        LeafEdge(W, R, FVector2D(-6888, -2440), FVector2D(-6888, -360), FVector2D(1, 0), 20, 6, Z);
        for (const FVector2D& C : { FVector2D(-6875, -2455), FVector2D(-6875, -345), FVector2D(-3745, -2455), FVector2D(-5815, -2455) })
            for (int32 I = 0; I < 45; ++I)
            {
                const double A = R.Range(0, 2 * LotPi), Rr = 70.0 * R.Frac() * R.Frac();
                const double X = C.X + FMath::Cos(A) * Rr, Y = C.Y + FMath::Sin(A) * Rr;
                if (OnPaving(X, Y, -6)) Leaf(W, R, X, Y, Z + R.Range(0, .3));
            }
        for (int32 I = 0; I < 150; ++I)
        {
            const double X = R.Range(-6880, -3740), Y = R.Range(-2460, -340);
            if (OnPaving(X, Y)) Leaf(W, R, X, Y, Z + R.Range(0, .2));
        }
        for (int32 I = 0; I < 40; ++I) Leaf(W, R, R.Range(-4470, -3570), R.Range(-280, -112), -56.55 + R.Range(0, .2));
    }

    // Litter from last weekend: party cups and cans blown against the curbs.
    {
        auto Cup = [&](double X, double Y, double Yaw)
        {
            const FVector D(FMath::Cos(Yaw), FMath::Sin(Yaw), 0);
            const FVector C(X, Y, Surface);
            const FVector Base = C - D * 5.8 + FVector(0, 0, 3.0), Mouth = C + D * 5.8 + FVector(0, 0, 4.6);
            W.Cone(TEXT("LOT_CupRed"), Base, Mouth, 3.0, 4.6, 14, true, false);
            const FVector In = (Mouth - Base).GetSafeNormal();
            W.Dot(TEXT("LOT_CupInside"), Mouth - In * .6, 4.35, In, 14);
        };
        auto Can = [&](double X, double Y, double Yaw, const TCHAR* Body)
        {
            const FVector D(FMath::Cos(Yaw), FMath::Sin(Yaw), 0);
            const FVector C(X, Y, Surface + 3.3);
            W.Cone(Body, C - D * 5.2, C + D * 5.2, 3.3, 3.3, 12, false, false);
            W.Cone(TEXT("LOT_Can"), C + D * 5.2, C + D * 6.1, 3.3, 2.7, 12, false, true);
            W.Cone(TEXT("LOT_Can"), C - D * 5.2, C - D * 6.1, 3.3, 2.8, 12, false, true);
        };
        Cup(-6874, -1150, .9); Cup(-6110, -2452, 2.6); Cup(-3742, -1505, -1.2); Cup(-4960, -344, .3); Cup(-5560, -1720, 1.9);
        Can(-6876, -2100, 1.4, TEXT("LOT_CanRed")); Can(-5160, -2458, .2, TEXT("LOT_CanBlue")); Can(-3744, -780, 2.0, TEXT("LOT_CanRed")); Can(-6300, -345, -.6, TEXT("LOT_CanBlue"));
    }
}

/** Sign panel facing N: Center is the middle of the face, RightDir the reader's right; the back is plain aluminium. */
void Sign(FLotWriter& W, FName Face, const FVector& Center, const FVector& N, const FVector& RightDir, double Wd, double H)
{
    const FVector U(0, 0, 1);
    const FVector Back = Center - N * .4;
    const FVector R = RightDir * (Wd * .5), V = U * (H * .5);
    const FVector2D UV[4] = { FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0) };
    W.QuadUV(Face, Center - R - V, Center + R - V, Center + R + V, Center - R + V, N, UV);
    W.Quad(TEXT("LOT_SignBack"), Back - R - V, Back + R - V, Back + R + V, Back - R + V, -N);
    W.Quad(TEXT("LOT_SignBack"), Center - R + V, Center + R + V, Back + R + V, Back - R + V, U);
    W.Quad(TEXT("LOT_SignBack"), Center - R - V, Center + R - V, Back + R - V, Back - R - V, -U);
    W.Quad(TEXT("LOT_SignBack"), Center - R - V, Center - R + V, Back - R + V, Back - R - V, -RightDir);
    W.Quad(TEXT("LOT_SignBack"), Center + R - V, Center + R + V, Back + R + V, Back + R - V, RightDir);
}

void Post(FLotWriter& W, double X, double Y, double Z0, double Z1)
{
    W.Box(TEXT("LOT_Steel"), FVector(X, Y, (Z0 + Z1) * .5), FVector(2.5, 2.5, (Z1 - Z0) * .5));
}

void BuildProps(FHallMesh& Mesh)
{
    FLotWriter W(Mesh);
    FLotRng R(41512);
    const FVector Up = FVector::UpVector;

    // Concrete pier bases under the two lot lights, with base plate and anchor nuts.
    for (const FVector2D& P : { FVector2D(-7160, -2600), FVector2D(-3740, -2530) })
    {
        W.Cone(TEXT("LOT_Concrete"), FVector(P.X, P.Y, Grass - 8), FVector(P.X, P.Y, Grass + 66), 24, 24, 18, false, false);
        W.Cone(TEXT("LOT_Concrete"), FVector(P.X, P.Y, Grass + 66), FVector(P.X, P.Y, Grass + 70), 24, 21, 18, false, true);
        W.Box(TEXT("LOT_Steel"), FVector(P.X, P.Y, Grass + 70.8), FVector(15, 15, .8));
        for (int32 K = 0; K < 4; ++K)
        {
            const double A = LotPi * .25 + K * LotPi * .5;
            W.Box(TEXT("LOT_Steel"), FVector(P.X + 11.5 * FMath::Cos(A), P.Y + 11.5 * FMath::Sin(A), Grass + 72.6), FVector(1.4, 1.4, 1.2), 45.0 * K);
        }
    }

    // Two bollards at the mouth of the shared loading aisle (2 m clear between them for the walk to the house).
    for (const double X : { -4430.0, -4210.0 })
    {
        const double Y = -312, Z = Surface;
        W.Cone(TEXT("LOT_Concrete"), FVector(X, Y, Z - 3), FVector(X, Y, Z + 3), 16, 15, 16, false, true);
        W.Cone(TEXT("LOT_BollardYellow"), FVector(X, Y, Z), FVector(X, Y, Z + 100), 11, 11, 16, false, false);
        W.Cone(TEXT("LOT_BollardYellow"), FVector(X, Y, Z + 100), FVector(X, Y, Z + 104), 11, 8.5, 16, false, false);
        W.Cone(TEXT("LOT_BollardYellow"), FVector(X, Y, Z + 104), FVector(X, Y, Z + 106.5), 8.5, 3.5, 16, false, true);
        W.Cone(TEXT("LOT_Reflect"), FVector(X, Y, Z + 64), FVector(X, Y, Z + 70), 11.15, 11.15, 16, false, false);
        W.Cone(TEXT("LOT_Reflect"), FVector(X, Y, Z + 80), FVector(X, Y, Z + 88), 11.15, 11.15, 16, false, false);
    }

    // Signs: towing notice at the driveway, two reserved bays (the muscle car's included) and the fire lane.
    Post(W, -5905, -2700, Grass - 20, Grass + 230);
    Sign(W, TEXT("LOT_SignTow"), FVector(-5905, -2703.0, Grass + 196), FVector(0, -1, 0), FVector(-1, 0, 0), 45, 60);
    for (const FVector2D& P : { FVector2D(-5045, -262), FVector2D(-5375, -262) })
    {
        Post(W, P.X, P.Y, Mulch - 15, Mulch + 158);
        Sign(W, P.X > -5200 ? FName(TEXT("LOT_SignReserved1")) : FName(TEXT("LOT_SignReserved2")), FVector(P.X, P.Y - 3.0, Mulch + 132), FVector(0, -1, 0), FVector(-1, 0, 0), 30, 45);
    }
    Post(W, -3640, -2300, Grass - 20, Grass + 182);
    Sign(W, TEXT("LOT_SignFireLane"), FVector(-3643.0, -2300, Grass + 156), FVector(-1, 0, 0), FVector(0, 1, 0), 30, 45);

    // Dumpster on its pad between the lot and the house walk, lids down, front to the lot; two roll carts and bags beside it.
    {
        const double X = -3626, Y = -1500, Z = Grass + 8;
        W.Box(TEXT("LOT_Concrete"), FVector(-3628, Y, Grass - 4), FVector(66, 120, 12));
        auto P = [&](double LX, double LY, double LZ) { return FVector(X + LX, Y + LY, Z + LZ); };
        const double HW = 91, Bot = 7, Top = 120;
        // body: vertical back, front leaning out toward the lot
        W.Quad(TEXT("LOT_Dumpster"), P(40, -HW, Bot), P(40, HW, Bot), P(40, HW, Top), P(40, -HW, Top), FVector(1, 0, 0));
        const FVector NF = FVector(-(Top - Bot), 0, -20).GetSafeNormal();
        W.Quad(TEXT("LOT_Dumpster"), P(-42, -HW, Bot), P(-42, HW, Bot), P(-62, HW, Top), P(-62, -HW, Top), NF);
        W.Quad(TEXT("LOT_DumpsterDark"), P(-42, -HW, Bot), P(40, -HW, Bot), P(40, HW, Bot), P(-42, HW, Bot), -Up);
        for (const double S : { -1.0, 1.0 })
        {
            const FVector N(0, S, 0);
            W.Quad(TEXT("LOT_Dumpster"), P(-42, S * HW, Bot), P(40, S * HW, Bot), P(40, S * HW, Top), P(-62, S * HW, Top), N);
            W.Box(TEXT("LOT_Dumpster"), P(-18, S * (HW + 6), 78), FVector(40, 6, 8));                 // fork pocket
            W.Box(TEXT("LOT_DumpsterDark"), P(-58.3, S * (HW + 6), 78), FVector(.4, 4.5, 6));          // pocket opening
            W.Box(TEXT("LOT_DumpsterDark"), P(0, S * 70, Bot * .5), FVector(42, 4, Bot * .5));        // skid
            W.Box(TEXT("LOT_Dumpster"), P(40.5, S * (HW - 2), (Bot + Top) * .5), FVector(1.5, 2.5, (Top - Bot) * .5));   // corner rib
            // two lids, hinged at the back and resting a little proud of the rim
            const double Y0 = S > 0 ? 1.0 : -HW - 2, Y1 = S > 0 ? HW + 2 : -1.0;
            const FVector A = P(-65, Y0, Top + 1.5), B = P(43, Y0, Top + 6), C = P(43, Y1, Top + 6), D = P(-65, Y1, Top + 1.5);
            const FVector NL = FVector::CrossProduct(B - A, D - A).GetSafeNormal() * (FVector::CrossProduct(B - A, D - A).Z > 0 ? 1.0 : -1.0);
            W.Quad(TEXT("LOT_LidBlack"), A, B, C, D, NL);
            W.Quad(TEXT("LOT_LidBlack"), A - FVector(0, 0, 3), B - FVector(0, 0, 3), C - FVector(0, 0, 3), D - FVector(0, 0, 3), -NL);
            W.Quad(TEXT("LOT_LidBlack"), A, D, D - FVector(0, 0, 3), A - FVector(0, 0, 3), FVector(-1, 0, 0));
            W.Quad(TEXT("LOT_LidBlack"), A, B, B - FVector(0, 0, 3), A - FVector(0, 0, 3), FVector(0, -1, 0));
            W.Quad(TEXT("LOT_LidBlack"), D, C, C - FVector(0, 0, 3), D - FVector(0, 0, 3), FVector(0, 1, 0));
        }
        W.Box(TEXT("LOT_Dumpster"), P(41, 0, Top - 4), FVector(2, HW, 4));                              // hinge rail
        // rust weeping from the bottom seams
        auto FrontX = [&](double LZ) { return -42.0 - 20.0 * (LZ - Bot) / (Top - Bot) - .4; };   // just proud of the leaning front
        for (int32 K = 0; K < 7; ++K)
        {
            const double LY = R.Range(-HW + 10, HW - 10), H = R.Range(10, 34), Wd = R.Range(5, 14), LZ = Bot + 2;
            W.Quad(TEXT("LOT_Rust"), P(FrontX(LZ), LY - Wd * .5, LZ), P(FrontX(LZ), LY + Wd * .5, LZ), P(FrontX(LZ + H), LY + Wd * .3, LZ + H), P(FrontX(LZ + H), LY - Wd * .3, LZ + H), NF);
        }
        // trash bags slumped against the dumpster's south end
        for (int32 K = 0; K < 2; ++K)
        {
            const FVector C(X - 30 + K * 38, Y - HW - 34 - K * 6, Grass);
            const int32 Seg = 10, Rings = 6;
            TArray<FVector> Prev;
            for (int32 Rg = 0; Rg <= Rings; ++Rg)
            {
                const double Ph = LotPi * Rg / Rings;
                TArray<FVector> Ring;
                for (int32 S = 0; S < Seg; ++S)
                {
                    const double A = 2 * LotPi * S / Seg;
                    const double Bulge = 1.0 + .12 * FMath::Sin(A * 3 + K * 2.0 + Rg) + .08 * FMath::Cos(A * 2 - Rg * 1.3);
                    const double Rr = FMath::Sin(Ph) * 28.0 * Bulge * (Rg > Rings / 2 ? .85 : 1.0);
                    Ring.Add(C + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr * .9, 2.0 + 44.0 * (1 - FMath::Cos(Ph)) * .5 * (Rg > Rings / 2 ? .92 : 1.0)));
                }
                if (Rg > 0)
                    for (int32 S = 0; S < Seg; ++S)
                    {
                        const FVector A = Prev[S], B = Prev[(S + 1) % Seg], Cc = Ring[(S + 1) % Seg], D = Ring[S];
                        const FVector Mid = (A + B + Cc + D) * .25, Ctr = C + FVector(0, 0, 24);
                        const FVector N = (Mid - Ctr).GetSafeNormal();
                        W.Quad(TEXT("LOT_Bag"), A, B, Cc, D, N);
                    }
                Prev = Ring;
            }
            W.Cone(TEXT("LOT_Bag"), C + FVector(0, 0, 39), C + FVector(R.Range(-3, 3), 2, 52), 4.5, 1.8, 8, false, true);   // tied neck
        }
        // roll carts north of the dumpster, lids shut, wheels to the house
        for (int32 K = 0; K < 2; ++K)
        {
            const double CY = -1185 + K * 76, CXc = -3630;
            const FName Body = K == 0 ? FName(TEXT("LOT_CartGreen")) : FName(TEXT("LOT_CartBlack"));
            const double Zb = Grass + 3;
            auto Q = [&](double LX, double LY, double LZ) { return FVector(CXc + LX, CY + LY, Zb + LZ); };
            const double B0X = 27, B0Y = 29, T0X = 34, T0Y = 33, HZ = 98;
            const FVector BC[4] = { Q(-B0X, -B0Y, 0), Q(B0X, -B0Y, 0), Q(B0X, B0Y, 0), Q(-B0X, B0Y, 0) };
            const FVector TC[4] = { Q(-T0X, -T0Y, HZ), Q(T0X, -T0Y, HZ), Q(T0X, T0Y, HZ), Q(-T0X, T0Y, HZ) };
            for (int32 F = 0; F < 4; ++F)
            {
                const FVector Mid = (BC[F] + BC[(F + 1) % 4] + TC[F] + TC[(F + 1) % 4]) * .25;
                FVector N = Mid - Q(0, 0, HZ * .5); N.Z = .1 * N.Size(); N.Normalize();
                W.Quad(Body, BC[F], BC[(F + 1) % 4], TC[(F + 1) % 4], TC[F], N);
            }
            W.Quad(Body, BC[0], BC[1], BC[2], BC[3], -Up);
            W.Box(TEXT("LOT_CartBlack"), Q(0, 0, HZ + 2.5), FVector(T0X + 2, T0Y + 2, 2.5));                  // lid
            W.Box(TEXT("LOT_CartBlack"), Q(T0X + 4, 0, HZ - 3), FVector(2, T0Y - 4, 2));                       // handle
            for (const double S : { -1.0, 1.0 })
                W.Cone(TEXT("LOT_Rubber"), Q(B0X + 6, S * (B0Y - 2), 13), Q(B0X + 6, S * (B0Y + 5), 13), 13, 13, 14, true, true);
        }
    }
}

/** Painted sign face: an aluminium plate with a border and centred lines of text. */
struct FSignLine { const TCHAR* Text; float Baseline, Height; FLinearColor Colour; };
void SignTexture(UHouseBlackHallSubsystem* L, FName Key, int32 W, int32 H, const FLinearColor& Field, const FLinearColor& Border, std::initializer_list<FSignLine> Lines)
{
    FHallImage I; I.Init(W, H, Field);
    I.Rect(FVector2D(6, 6), FVector2D(W - 6, H - 6), Border);
    I.Rect(FVector2D(13, 13), FVector2D(W - 13, H - 13), Field);
    for (const FSignLine& Ln : Lines)
    {
        FHallTextStyle S; S.Height = Ln.Height; S.Stroke = FMath::Max(2.f, Ln.Height * .14f); S.Spacing = .14f;
        S.WidthScale = FMath::Min(1.f, (W - 34.f) / FMath::Max(1.f, HallTextWidth(Ln.Text, S)));
        HallDrawText(I, Ln.Text, FVector2D((W - HallTextWidth(Ln.Text, S)) * .5f, Ln.Baseline), S, Ln.Colour);
    }
    UTexture2D* T = HallMakeTexture(I, true, false, *Key.ToString(), false);
    if (T) { L->Textures.Add(Key, T); HallMakeTextured(L, Key, T, nullptr, .5f); }
    else HallMakePlain(L, Key, Field, .5f);
}
} // namespace HouseLotDetailImpl

void HouseLotDetail::CreateLook(UHouseBlackHallSubsystem* L)
{
    if (!L || L->Materials.Contains(TEXT("LOT_Oil"))) return;
    auto Plain = [&](const TCHAR* K, const FLinearColor& C, float Rough, float Metal = 0.f) { HallMakePlain(L, FName(K), C, Rough, Metal); };
    Plain(TEXT("LOT_Oil"), FLinearColor(.030f, .028f, .025f), .42f);
    Plain(TEXT("LOT_OilHalo"), FLinearColor(.050f, .048f, .045f), .7f);
    Plain(TEXT("LOT_Seal"), FLinearColor(.011f, .011f, .011f), .32f);
    Plain(TEXT("LOT_Crack"), FLinearColor(.019f, .018f, .016f), .95f);
    Plain(TEXT("LOT_Patch"), FLinearColor(.048f, .048f, .050f), .82f);
    Plain(TEXT("LOT_Polish"), FLinearColor(.069f, .069f, .071f), .6f);
    Plain(TEXT("LOT_Puddle"), FLinearColor(.010f, .011f, .013f), .03f);
    Plain(TEXT("LOT_Wet"), FLinearColor(.034f, .033f, .032f), .28f);
    Plain(TEXT("LOT_Scuff"), FLinearColor(.050f, .049f, .047f), .8f);
    Plain(TEXT("LOT_Iron"), FLinearColor(.055f, .050f, .046f), .62f, .55f);
    Plain(TEXT("LOT_Slot"), FLinearColor(.004f, .004f, .004f), 1.f);
    Plain(TEXT("LOT_LeafOrange"), FLinearColor(.30f, .10f, .018f), .8f);
    Plain(TEXT("LOT_LeafBrown"), FLinearColor(.11f, .055f, .02f), .85f);
    Plain(TEXT("LOT_LeafYellow"), FLinearColor(.42f, .29f, .04f), .8f);
    Plain(TEXT("LOT_LeafRed"), FLinearColor(.24f, .035f, .018f), .8f);
    Plain(TEXT("LOT_CupRed"), FLinearColor(.45f, .02f, .02f), .38f);
    Plain(TEXT("LOT_CupInside"), FLinearColor(.80f, .80f, .78f), .45f);
    Plain(TEXT("LOT_Can"), FLinearColor(.62f, .63f, .65f), .28f, .9f);
    Plain(TEXT("LOT_CanRed"), FLinearColor(.40f, .03f, .03f), .3f, .6f);
    Plain(TEXT("LOT_CanBlue"), FLinearColor(.04f, .09f, .32f), .3f, .6f);
    Plain(TEXT("LOT_CurbRed"), FLinearColor(.36f, .03f, .02f), .72f);
    Plain(TEXT("LOT_CurbRedWorn"), FLinearColor(.21f, .085f, .06f), .86f);
    Plain(TEXT("LOT_Concrete"), FLinearColor(.33f, .32f, .30f), .92f);
    Plain(TEXT("LOT_Steel"), FLinearColor(.50f, .51f, .52f), .42f, .9f);
    Plain(TEXT("LOT_SignBack"), FLinearColor(.42f, .43f, .44f), .5f, .8f);
    Plain(TEXT("LOT_BollardYellow"), FLinearColor(.62f, .43f, .02f), .45f);
    Plain(TEXT("LOT_Reflect"), FLinearColor(.78f, .78f, .75f), .22f);
    Plain(TEXT("LOT_Dumpster"), FLinearColor(.028f, .085f, .045f), .55f, .3f);
    Plain(TEXT("LOT_DumpsterDark"), FLinearColor(.010f, .012f, .010f), .8f);
    Plain(TEXT("LOT_LidBlack"), FLinearColor(.016f, .016f, .016f), .6f);
    Plain(TEXT("LOT_Rust"), FLinearColor(.17f, .065f, .025f), .9f);
    Plain(TEXT("LOT_Bag"), FLinearColor(.012f, .012f, .013f), .3f);
    Plain(TEXT("LOT_CartGreen"), FLinearColor(.03f, .11f, .05f), .6f);
    Plain(TEXT("LOT_CartBlack"), FLinearColor(.02f, .02f, .02f), .6f);
    Plain(TEXT("LOT_Rubber"), FLinearColor(.015f, .015f, .015f), .9f);
    const FLinearColor White(.82f, .82f, .78f), Red(.42f, .02f, .02f), Ink(.02f, .02f, .02f), Green(.02f, .14f, .07f);
    HouseLotDetailImpl::SignTexture(L, TEXT("LOT_SignTow"), 256, 340, White, Red, {
        { TEXT("RESIDENT"), 58.f, 34.f, Red }, { TEXT("PARKING ONLY"), 104.f, 26.f, Red },
        { TEXT("UNAUTHORIZED"), 168.f, 20.f, Ink }, { TEXT("VEHICLES"), 202.f, 20.f, Ink }, { TEXT("WILL BE TOWED"), 236.f, 20.f, Ink },
        { TEXT("AT OWNERS"), 284.f, 17.f, Ink }, { TEXT("EXPENSE"), 312.f, 17.f, Ink } });
    HouseLotDetailImpl::SignTexture(L, TEXT("LOT_SignReserved1"), 256, 384, White, Green, {
        { TEXT("RESERVED"), 92.f, 40.f, Green }, { TEXT("CHAPTER"), 214.f, 32.f, Ink }, { TEXT("PRESIDENT"), 270.f, 32.f, Ink } });
    HouseLotDetailImpl::SignTexture(L, TEXT("LOT_SignReserved2"), 256, 384, White, Green, {
        { TEXT("RESERVED"), 92.f, 40.f, Green }, { TEXT("HOUSE"), 214.f, 32.f, Ink }, { TEXT("DIRECTOR"), 270.f, 32.f, Ink } });
    HouseLotDetailImpl::SignTexture(L, TEXT("LOT_SignFireLane"), 256, 384, White, Red, {
        { TEXT("NO"), 84.f, 46.f, Red }, { TEXT("PARKING"), 150.f, 34.f, Red }, { TEXT("FIRE"), 250.f, 46.f, Red }, { TEXT("LANE"), 320.f, 46.f, Red } });
}

void HouseLotDetail::Build(FHallMesh& Wear, FHallMesh& Props)
{
    HouseLotDetailImpl::BuildWear(Wear);
    HouseLotDetailImpl::BuildProps(Props);
    UE_LOG(LogTemp, Display, TEXT("PARKING_LOT_DETAIL wearTris=%d propTris=%d lights=0"), Wear.NumTriangles(), Props.NumTriangles());
}
