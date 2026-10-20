// Fraternity Park Dr at dusk, Halloween 1986. See HouseStreetScene.h.
// Everything here is placed clear of the exterior walk review's routes (the front walk, the lawn loops at y -3500/-3520,
// x = -1200, and the near lane at y -4350 east of the walk), the road-floor traces at y -4800, and Codex's front-yard
// litter close-ups around (700, -2950). Nothing adds a light.
#include "HouseStreetScene.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusLayout.h"

namespace HouseStreetSceneImpl
{
constexpr double StPi = 3.14159265358979323846;
constexpr float Road = -72.f;        // street paving top (GF - 12)
constexpr float Walk = -57.f;        // sidewalk and kerb tops (GF + 3)
constexpr float Lawn = -60.f;        // front lawn grade
constexpr float NearKerb = -4170.f;  // road-side face of the near kerb
constexpr float FarKerb = -5530.f;   // road-side face of the far kerb
constexpr float CentreY = -4850.f;
constexpr float GutterW = 45.f;
constexpr float ParkW = 290.f;       // parking lane, kerb face to its white line
constexpr float WearZ = -71.45f;     // flat pieces on the paving (Codex's repairs top out at -71.62)
// The lens sits under the cobra head; the light goes just below it.
const FVector Lens(251.f, 0.f, 708.f);

/** Small deterministic generator, so the scene is identical on every machine. */
struct FStRng
{
    uint32 S;
    explicit FStRng(uint32 Seed) : S(Seed * 747796405u + 2891336453u) { if (!S) S = 1; }
    uint32 Next() { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
    double Frac() { return (Next() & 0xffffff) / 16777216.0; }
    double Range(double A, double B) { return A + (B - A) * Frac(); }
    bool Chance(double P) { return Frac() < P; }
};

float Hash2(int32 X, int32 Y, int32 Seed)
{
    uint32 H = (uint32)X * 374761393u + (uint32)Y * 668265263u + (uint32)Seed * 2246822519u;
    H = (H ^ (H >> 13)) * 1274126177u;
    H ^= H >> 16;
    return (H & 0xFFFFFF) / float(0xFFFFFF);
}
/** Value noise that repeats every PX lattice cells across and PY down (tileable textures). */
float PNoise(float X, float Y, int32 PX, int32 PY, int32 Seed)
{
    const int32 X0 = (int32)FMath::FloorToDouble(X), Y0 = (int32)FMath::FloorToDouble(Y);
    float FX = X - X0, FY = Y - Y0;
    FX = FX * FX * (3 - 2 * FX); FY = FY * FY * (3 - 2 * FY);
    auto W = [](int32 V, int32 P) { return P > 0 ? ((V % P) + P) % P : V; };
    const float A = Hash2(W(X0, PX), W(Y0, PY), Seed), B = Hash2(W(X0 + 1, PX), W(Y0, PY), Seed);
    const float C = Hash2(W(X0, PX), W(Y0 + 1, PY), Seed), D = Hash2(W(X0 + 1, PX), W(Y0 + 1, PY), Seed);
    const float AB = A + (B - A) * FX, CD = C + (D - C) * FX;
    return AB + (CD - AB) * FY;
}
float PFbm(float X, float Y, int32 PX, int32 PY, int32 Octaves, int32 Seed)
{
    float Sum = 0, Amp = .5f, Norm = 0;
    for (int32 O = 0; O < Octaves; ++O)
    {
        Sum += PNoise(X, Y, PX, PY, Seed + O * 31) * Amp; Norm += Amp;
        X *= 2; Y *= 2; PX *= 2; PY *= 2; Amp *= .5f;
    }
    return Sum / FMath::Max(Norm, 1e-4f);
}
float Smooth(float A, float B, float X)
{
    const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f);
    return T * T * (3 - 2 * T);
}
FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float T)
{
    return FLinearColor(A.R + (B.R - A.R) * T, A.G + (B.G - A.G) * T, A.B + (B.B - A.B) * T, A.A + (B.A - A.A) * T);
}

// ------------------------------------------------------------------ painting
/** Source-over onto a canvas that carries coverage in alpha (decal and smoke textures). */
void Over(FHallImage& I, int32 X, int32 Y, const FLinearColor& C, float A)
{
    if (X < 0 || Y < 0 || X >= I.W || Y >= I.H || A <= 0.f) return;
    A = FMath::Min(A, 1.f);
    FLinearColor& D = I.At(X, Y);
    const float Out = A + D.A * (1 - A);
    if (Out <= 1e-6f) return;
    const float K = D.A * (1 - A);
    D.R = (C.R * A + D.R * K) / Out; D.G = (C.G * A + D.G * K) / Out; D.B = (C.B * A + D.B * K) / Out; D.A = Out;
}
void Put(FHallImage& I, int32 X, int32 Y, const FLinearColor& C, float A, bool bAlphaCanvas)
{
    if (bAlphaCanvas) Over(I, X, Y, C, A); else I.Blend(X, Y, C, A);
}
/** Soft disc. Hard = 1 gives an anti-aliased edge, lower values feather it. */
void Dab(FHallImage& I, float CX, float CY, float R, const FLinearColor& C, float A, float Hard, bool bAlphaCanvas)
{
    const int32 X0 = FMath::Max(0, (int32)FMath::FloorToDouble(CX - R - 1)), X1 = FMath::Min(I.W - 1, (int32)FMath::CeilToDouble(CX + R + 1));
    const int32 Y0 = FMath::Max(0, (int32)FMath::FloorToDouble(CY - R - 1)), Y1 = FMath::Min(I.H - 1, (int32)FMath::CeilToDouble(CY + R + 1));
    for (int32 Y = Y0; Y <= Y1; ++Y)
        for (int32 X = X0; X <= X1; ++X)
        {
            const float D = FMath::Sqrt(FMath::Square(X + .5f - CX) + FMath::Square(Y + .5f - CY));
            float Cov = FMath::Clamp(R - D + .5f, 0.f, 1.f);
            if (Hard < 1.f) Cov *= 1.f - Smooth(R * Hard, R + .5f, D);
            if (Cov > 0) Put(I, X, Y, C, A * Cov, bAlphaCanvas);
        }
}
/** Hand brush along a polyline: width breathes along the stroke, coverage is merged so overlaps do not darken. */
void Brush(FHallImage& I, const TArray<FVector2D>& P, float Width, const FLinearColor& C, float A, int32 Seed, bool bAlphaCanvas, float Bristle = .16f)
{
    if (P.Num() == 0) return;
    float MinX = 1e9f, MinY = 1e9f, MaxX = -1e9f, MaxY = -1e9f;
    for (const FVector2D& V : P) { MinX = FMath::Min(MinX, (float)V.X); MinY = FMath::Min(MinY, (float)V.Y); MaxX = FMath::Max(MaxX, (float)V.X); MaxY = FMath::Max(MaxY, (float)V.Y); }
    const int32 X0 = FMath::Max(0, (int32)(MinX - Width - 3)), X1 = FMath::Min(I.W - 1, (int32)(MaxX + Width + 3));
    const int32 Y0 = FMath::Max(0, (int32)(MinY - Width - 3)), Y1 = FMath::Min(I.H - 1, (int32)(MaxY + Width + 3));
    if (X1 < X0 || Y1 < Y0) return;
    const int32 CW = X1 - X0 + 1;
    TArray<float> Cov; Cov.Init(0.f, CW * (Y1 - Y0 + 1));
    double Run = 0;
    auto Stamp = [&](float SX, float SY, float R)
    {
        for (int32 Y = FMath::Max(Y0, (int32)(SY - R - 1)); Y <= FMath::Min(Y1, (int32)(SY + R + 1)); ++Y)
            for (int32 X = FMath::Max(X0, (int32)(SX - R - 1)); X <= FMath::Min(X1, (int32)(SX + R + 1)); ++X)
            {
                const float D = FMath::Sqrt(FMath::Square(X + .5f - SX) + FMath::Square(Y + .5f - SY));
                const float V = FMath::Clamp(R - D + .5f, 0.f, 1.f);
                float& Slot = Cov[(Y - Y0) * CW + (X - X0)];
                Slot = FMath::Max(Slot, V);
            }
    };
    for (int32 S = 0; S < FMath::Max(1, P.Num() - 1); ++S)
    {
        const FVector2D A2 = P[S], B2 = P.Num() > 1 ? P[S + 1] : P[S];
        const double L = (B2 - A2).Size();
        const int32 N = FMath::Max(1, (int32)FMath::CeilToDouble(L / FMath::Max(1.f, Width * .18f)));
        for (int32 K = 0; K <= N; ++K)
        {
            const double T = (double)K / N;
            const FVector2D Q = A2 + (B2 - A2) * T;
            const float Breathe = .86f + .26f * PNoise((float)(Run + L * T) * .035f, 0, 0, 0, Seed);
            Stamp((float)Q.X, (float)Q.Y, Width * .5f * Breathe);
        }
        Run += L;
    }
    for (int32 Y = Y0; Y <= Y1; ++Y)
        for (int32 X = X0; X <= X1; ++X)
            if (const float V = Cov[(Y - Y0) * CW + (X - X0)])
            {
                const float Hair = 1.f - Bristle * Hash2(X, Y / 3, Seed + 5) * (V < 1.f ? 1.6f : 1.f);
                Put(I, X, Y, C, A * V * Hair, bAlphaCanvas);
            }
}
/** Glyph polylines from the house stroke font, laid out with a hand-lettered wobble. */
void HandText(TArray<TArray<FVector2D>>& Out, const FString& Text, float Left, float Baseline, float Height, float WidthScale, float Spacing, float Wobble, int32 Seed)
{
    const float U = Height / 6.f;
    float Cursor = 0;
    int32 Index = 0;
    for (TCHAR Ch : Text)
    {
        // Each letter leans and sits a little differently, as painted by hand.
        const float Lean = (Hash2(Index, 1, Seed) - .5f) * .08f, Lift = (Hash2(Index, 2, Seed) - .5f) * Height * .06f;
        const float Scale = 1.f + (Hash2(Index, 3, Seed) - .5f) * .07f;
        for (const TArray<FVector2D>& Line : HallGlyph(Ch))
        {
            TArray<FVector2D>& P = Out.AddDefaulted_GetRef();
            for (int32 I = 0; I < Line.Num(); ++I)
            {
                const int32 Steps = I == 0 ? 1 : FMath::Clamp((int32)FMath::CeilToDouble((Line[I] - Line[I - 1]).Size() * 2.5), 1, 16);
                for (int32 S = (I == 0 ? 0 : 1); S <= Steps; ++S)
                {
                    const FVector2D G = I == 0 ? Line[0] : Line[I - 1] + (Line[I] - Line[I - 1]) * ((double)S / Steps);
                    float X = Left + Cursor + (float)(G.X + G.Y * Lean) * U * WidthScale * Scale;
                    float Y = Baseline - Lift - (float)G.Y * U * Scale;
                    X += (PNoise(X * .045f, Y * .045f, 0, 0, Seed + Index) - .5f) * Wobble;
                    Y += (PNoise(Y * .045f + 7.1f, X * .045f, 0, 0, Seed + 11 + Index) - .5f) * Wobble;
                    P.Add(FVector2D(X, Y));
                }
            }
        }
        Cursor += (Ch == TEXT(' ') ? 2.6f : 4.f * WidthScale) * U + Spacing * Height;
        ++Index;
    }
}
float HandTextWidth(const FString& Text, float Height, float WidthScale, float Spacing)
{
    const float U = Height / 6.f;
    float W = 0;
    for (TCHAR Ch : Text) W += (Ch == TEXT(' ') ? 2.6f : 4.f * WidthScale) * U + Spacing * Height;
    return FMath::Max(0.f, W - Spacing * Height);
}
/** Paint drips under the lowest points of a painted stroke set. */
void Drips(FHallImage& I, const TArray<TArray<FVector2D>>& Strokes, float StrokeW, const FLinearColor& C, float Chance, float MaxLen, int32 Seed, float FloorY)
{
    FStRng R(Seed);
    for (const TArray<FVector2D>& P : Strokes)
        for (int32 K = 0; K < P.Num(); ++K)
        {
            const bool bLow = (K == 0 || P[K].Y >= P[K - 1].Y - .01) && (K + 1 == P.Num() || P[K].Y >= P[K + 1].Y - .01);
            if (!bLow || !R.Chance(Chance)) continue;
            const float Len = (float)R.Range(MaxLen * .25, MaxLen);
            const float X = (float)P[K].X + (float)R.Range(-StrokeW * .25, StrokeW * .25);
            const float Y0 = (float)P[K].Y + StrokeW * .3f;
            const float Y1 = FMath::Min(FloorY, Y0 + Len);
            for (float Y = Y0; Y <= Y1; Y += .7f)
            {
                const float T = (Y - Y0) / FMath::Max(1.f, Y1 - Y0);
                Dab(I, X + FMath::Sin(Y * .21f) * .4f, Y, StrokeW * (.22f - .07f * T), C, 1.f, 1.f, false);
            }
            Dab(I, X, Y1, StrokeW * .26f, C, 1.f, 1.f, false);
        }
}

// ------------------------------------------------------------------ geometry
struct FStWriter
{
    FHallMesh& M;
    explicit FStWriter(FHallMesh& In) : M(In) {}
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
    /** Box on arbitrary (orthonormal) axes. */
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
    void Box(FName K, const FVector& C, const FVector& H, double YawDeg = 0.0)
    {
        const double Y = YawDeg * StPi / 180.0;
        BoxAx(K, C, FVector(FMath::Cos(Y), FMath::Sin(Y), 0), FVector(-FMath::Sin(Y), FMath::Cos(Y), 0), FVector::UpVector, H);
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
            const double A0 = 2 * StPi * S / Seg, A1 = 2 * StPi * (S + 1) / Seg;
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
            for (int32 S = 0; S < Seg; ++S) { const double An = 2 * StPi * S / Seg; Ring.Add(C + (X * FMath::Cos(An) + Y * FMath::Sin(An)) * R); }
            Fan(K, C, Ring, E ? Ax : -Ax);
        }
    }
    /** Surface of revolution about Axis through Base: Profile holds (radius, height); hard creases unless bSmooth. */
    void Lathe(FName K, const FVector& Base, const FVector& Axis, const FVector& Ref, const TArray<FVector2D>& Profile, int32 Seg, bool bSmooth)
    {
        const FVector Z = Axis.GetSafeNormal();
        FVector X = (Ref - Z * FVector::DotProduct(Ref, Z)).GetSafeNormal();
        if (X.IsNearlyZero()) X = TangentFor(Z);
        const FVector Y = FVector::CrossProduct(Z, X);
        const int32 N = Profile.Num();
        auto EdgeN = [&](int32 E) { const FVector2D D = Profile[E + 1] - Profile[E]; return FVector2D(D.Y, -D.X); };   // (radial, axial) outward
        for (int32 E = 0; E + 1 < N; ++E)
        {
            if ((Profile[E + 1] - Profile[E]).Size() < 1e-6) continue;
            FVector2D NA = EdgeN(E), NB = NA;
            if (bSmooth)
            {
                if (E > 0) NA = NA / FMath::Max(1e-6, NA.Size()) + EdgeN(E - 1) / FMath::Max(1e-6, EdgeN(E - 1).Size());
                if (E + 2 < N) NB = NB / FMath::Max(1e-6, NB.Size()) + EdgeN(E + 1) / FMath::Max(1e-6, EdgeN(E + 1).Size());
            }
            for (int32 S = 0; S < Seg; ++S)
            {
                const double A0 = 2 * StPi * S / Seg, A1 = 2 * StPi * (S + 1) / Seg;
                const FVector D0 = X * FMath::Cos(A0) + Y * FMath::Sin(A0), D1 = X * FMath::Cos(A1) + Y * FMath::Sin(A1);
                const FVector P00 = Base + D0 * Profile[E].X + Z * Profile[E].Y, P10 = Base + D1 * Profile[E].X + Z * Profile[E].Y;
                const FVector P01 = Base + D0 * Profile[E + 1].X + Z * Profile[E + 1].Y, P11 = Base + D1 * Profile[E + 1].X + Z * Profile[E + 1].Y;
                const FVector N00 = (D0 * NA.X + Z * NA.Y).GetSafeNormal(), N10 = (D1 * NA.X + Z * NA.Y).GetSafeNormal();
                const FVector N01 = (D0 * NB.X + Z * NB.Y).GetSafeNormal(), N11 = (D1 * NB.X + Z * NB.Y).GetSafeNormal();
                Emit(K, P00, P10, P11, N00, N10, N11);
                Emit(K, P00, P11, P01, N00, N11, N01);
            }
        }
    }
    /** Round tube along a polyline (parallel-transported rings). */
    void Tube(FName K, const TArray<FVector>& P, double R, int32 Seg, bool bCaps = false)
    {
        const int32 N = P.Num();
        if (N < 2) return;
        TArray<FVector> T; T.SetNum(N);
        for (int32 I = 0; I < N; ++I)
        {
            FVector D = (I + 1 < N ? P[I + 1] : P[I]) - (I > 0 ? P[I - 1] : P[I]);
            if (!D.Normalize()) D = FVector::UpVector;
            T[I] = D;
        }
        FVector U = TangentFor(T[0]);
        TArray<TArray<FVector>> Rings, Norms;
        for (int32 I = 0; I < N; ++I)
        {
            if (I > 0)
            {
                U = U - T[I] * FVector::DotProduct(U, T[I]);
                if (!U.Normalize()) U = TangentFor(T[I]);
            }
            const FVector V = FVector::CrossProduct(T[I], U);
            // Mitre: widen the ring at a bend so the tube keeps its radius.
            double Scale = 1.0;
            if (I > 0 && I + 1 < N)
            {
                FVector A = P[I] - P[I - 1], B = P[I + 1] - P[I];
                if (A.Normalize() && B.Normalize()) Scale = 1.0 / FMath::Max(.6, FMath::Sqrt((1.0 + FVector::DotProduct(A, B)) * .5));
            }
            TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
            TArray<FVector>& Nr = Norms.AddDefaulted_GetRef();
            for (int32 S = 0; S < Seg; ++S)
            {
                const double An = 2 * StPi * S / Seg;
                const FVector D = U * FMath::Cos(An) + V * FMath::Sin(An);
                Ring.Add(P[I] + D * (R * (FMath::Abs(FVector::DotProduct(D, (I > 0 && I + 1 < N) ? (P[I + 1] - P[I - 1]).GetSafeNormal() : T[I])) > .99 ? 1.0 : Scale)));
                Nr.Add(D);
            }
        }
        for (int32 I = 0; I + 1 < N; ++I)
            for (int32 S = 0; S < Seg; ++S)
            {
                const int32 S1 = (S + 1) % Seg;
                Emit(K, Rings[I][S], Rings[I][S1], Rings[I + 1][S1], Norms[I][S], Norms[I][S1], Norms[I + 1][S1]);
                Emit(K, Rings[I][S], Rings[I + 1][S1], Rings[I + 1][S], Norms[I][S], Norms[I + 1][S1], Norms[I + 1][S]);
            }
        if (bCaps) { Fan(K, P[0], Rings[0], -T[0]); Fan(K, P[N - 1], Rings[N - 1], T[N - 1]); }
    }
    /** Loft through rings of equal size (convex cross-sections); normals point away from each ring's centre. */
    void Loft(FName K, const TArray<TArray<FVector>>& Rings, bool bCapStart, bool bCapEnd)
    {
        const int32 N = Rings.Num();
        if (N < 2) return;
        const int32 Seg = Rings[0].Num();
        TArray<FVector> C; C.SetNum(N);
        for (int32 I = 0; I < N; ++I) { FVector S = FVector::ZeroVector; for (const FVector& V : Rings[I]) S += V; C[I] = S / Seg; }
        for (int32 I = 0; I + 1 < N; ++I)
        {
            const FVector Ax = (C[I + 1] - C[I]).GetSafeNormal();
            auto Nrm = [&](int32 Ring, int32 S)
            {
                FVector D = Rings[Ring][S] - C[Ring];
                D -= Ax * FVector::DotProduct(D, Ax);
                return D.GetSafeNormal();
            };
            for (int32 S = 0; S < Seg; ++S)
            {
                const int32 S1 = (S + 1) % Seg;
                Emit(K, Rings[I][S], Rings[I][S1], Rings[I + 1][S1], Nrm(I, S), Nrm(I, S1), Nrm(I + 1, S1));
                Emit(K, Rings[I][S], Rings[I + 1][S1], Rings[I + 1][S], Nrm(I, S), Nrm(I + 1, S1), Nrm(I + 1, S));
            }
        }
        if (bCapStart) Fan(K, C[0], Rings[0], (C[0] - C[1]).GetSafeNormal());
        if (bCapEnd) Fan(K, C[N - 1], Rings[N - 1], (C[N - 1] - C[N - 2]).GetSafeNormal());
    }
    /** Ribbon along a flat polyline (mitred joints), one width per point, lying on the plane with normal Up. */
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

/** Local frame: X forward, Y left-to-right as UE has it, Z up. */
struct FStFrame
{
    FVector O = FVector::ZeroVector, X = FVector::ForwardVector, Y = FVector::RightVector, Z = FVector::UpVector;
    static FStFrame Yawed(const FVector& Origin, double YawDeg)
    {
        const double A = YawDeg * StPi / 180.0;
        FStFrame F; F.O = Origin;
        F.X = FVector(FMath::Cos(A), FMath::Sin(A), 0); F.Y = FVector(-FMath::Sin(A), FMath::Cos(A), 0); F.Z = FVector::UpVector;
        return F;
    }
    FVector P(double LX, double LY, double LZ) const { return O + X * LX + Y * LY + Z * LZ; }
    FVector P(const FVector& L) const { return P(L.X, L.Y, L.Z); }
    FVector D(const FVector& L) const { return X * L.X + Y * L.Y + Z * L.Z; }
    static FVector Rot(const FVector& V, const FVector& K, double A)
    {
        return V * FMath::Cos(A) + FVector::CrossProduct(K, V) * FMath::Sin(A) + K * (FVector::DotProduct(K, V) * (1 - FMath::Cos(A)));
    }
    /** The same frame turned by Angle (radians) about a world-space axis through Pivot. */
    FStFrame Turned(const FVector& Pivot, const FVector& Axis, double Angle) const
    {
        FStFrame F;
        const FVector K = Axis.GetSafeNormal();
        F.O = Pivot + Rot(O - Pivot, K, Angle);
        F.X = Rot(X, K, Angle); F.Y = Rot(Y, K, Angle); F.Z = Rot(Z, K, Angle);
        return F;
    }
};

// ------------------------------------------------------------------ textures
void PaintAsphalt(FHallImage& Img, TArray<float>& Height)
{
    // One 1024 px tile covers 500 cm (the street UVs): binder, two sizes of aggregate, pits and hairline cracks.
    const int32 N = 1024;
    Img.Init(N, N, FLinearColor::Black); Height.Init(0.f, N * N);
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const float U = X / (float)N, V = Y / (float)N;
            const float Blot = PFbm(U * 6, V * 6, 6, 6, 4, 11), Fine = PFbm(U * 96, V * 96, 96, 96, 2, 12);
            const float L = .060f + .022f * (Blot - .5f) + .012f * (Fine - .5f);
            Img.At(X, Y) = FLinearColor(L * .97f, L * .985f, L * 1.03f);
            Height[Y * N + X] = Fine * .15f;
        }
    // Aggregate on jittered grids that wrap with the tile.
    struct FGrid { int32 Cell; float Chance, R0, R1, L0, L1; int32 Seed; };
    for (const FGrid& G : { FGrid{ 5, .48f, .7f, 1.9f, .092f, .145f, 21 }, FGrid{ 15, .26f, 2.2f, 4.2f, .088f, .135f, 22 } })
    {
        const int32 Cells = N / G.Cell;
        for (int32 CY = 0; CY < Cells; ++CY)
            for (int32 CX = 0; CX < Cells; ++CX)
            {
                if (Hash2(CX, CY, G.Seed) > G.Chance) continue;
                const float PX = (CX + .15f + .7f * Hash2(CX, CY, G.Seed + 1)) * G.Cell, PY = (CY + .15f + .7f * Hash2(CX, CY, G.Seed + 2)) * G.Cell;
                const float R = G.R0 + (G.R1 - G.R0) * Hash2(CX, CY, G.Seed + 3), Lum = G.L0 + (G.L1 - G.L0) * Hash2(CX, CY, G.Seed + 4);
                const float Warm = Hash2(CX, CY, G.Seed + 5);
                const FLinearColor Stone(Lum * (1.02f + .06f * Warm), Lum, Lum * (1.0f - .07f * Warm));
                for (int32 DY = -(int32)R - 1; DY <= (int32)R + 1; ++DY)
                    for (int32 DX = -(int32)R - 1; DX <= (int32)R + 1; ++DX)
                    {
                        const int32 QX = (((int32)PX + DX) % N + N) % N, QY = (((int32)PY + DY) % N + N) % N;
                        const float D = FMath::Sqrt(FMath::Square((int32)PX + DX + .5f - PX) + FMath::Square((int32)PY + DY + .5f - PY));
                        const float Cov = FMath::Clamp(R - D + .5f, 0.f, 1.f);
                        if (Cov <= 0) continue;
                        Img.Blend(QX, QY, Stone, Cov * .85f);
                        float& H = Height[QY * N + QX];
                        H = FMath::Max(H, Cov * (.55f + .45f * (1 - D / FMath::Max(.5f, R))));
                    }
            }
    }
    // Pits and a few hairline cracks.
    for (int32 K = 0; K < 900; ++K)
    {
        const int32 PX = (int32)(Hash2(K, 1, 31) * N), PY = (int32)(Hash2(K, 2, 31) * N);
        Img.At(PX, PY) = Img.At(PX, PY) * .45f; Height[PY * N + PX] = -.6f;
    }
    FStRng R(77);
    for (int32 C = 0; C < 7; ++C)
    {
        double X = R.Range(0, N), Y = R.Range(0, N), H = R.Range(0, 2 * StPi);
        const int32 Len = (int32)R.Range(120, 420);
        for (int32 S = 0; S < Len; ++S)
        {
            H += R.Range(-.35, .35); X += FMath::Cos(H); Y += FMath::Sin(H);
            const int32 QX = (((int32)X) % N + N) % N, QY = (((int32)Y) % N + N) % N;
            Img.At(QX, QY) = Img.At(QX, QY) * .5f; Height[QY * N + QX] = -.5f;
        }
    }
}

void PaintGutter(FHallImage& Img)
{
    // 512 x 128 = 300 cm along the kerb x 45 cm out from it (V = 1 at the kerb face). Joints every 150 cm.
    const int32 W = 512, H = 128;
    Img.Init(W, H, FLinearColor::Black);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float U = X / (float)W, V = Y / (float)H;
            const float Cloud = PFbm(U * 8, V * 2, 8, 0, 3, 41), Grain = Hash2(X, Y, 42);
            float L = .30f + .07f * (Cloud - .5f) + .035f * (Grain - .5f);
            // Grime gathers toward the kerb where the water runs; a dark wet line hugs the face.
            L *= 1.f - .38f * Smooth(.45f, 1.f, V) * (.6f + .4f * PFbm(U * 24, V * 3, 24, 0, 2, 43));
            if (V > .93f) L *= .55f;
            const int32 J = X % (W / 2);
            if (J < 2 || J > W / 2 - 2) L *= .45f;
            Img.At(X, Y) = FLinearColor(L * 1.01f, L, L * .95f);
        }
    for (int32 K = 0; K < 14; ++K)   // tannin stains from leaves
    {
        const float CX = Hash2(K, 1, 44) * W, CY = H * (.35f + .6f * Hash2(K, 2, 44)), R = 5 + 9 * Hash2(K, 3, 44);
        for (int32 Rep = -1; Rep <= 1; ++Rep) Dab(Img, CX + Rep * W, CY, R, FLinearColor(.12f, .085f, .05f), .35f, .4f, false);
    }
}

void PaintPoleWood(FHallImage& Img, TArray<float>& Height)
{
    // 128 around x 512 up = one 200 cm tall band of a creosoted, sun-greyed pole: vertical grain and checks.
    const int32 W = 128, H = 512;
    Img.Init(W, H, FLinearColor::Black); Height.Init(0.f, W * H);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float U = X / (float)W, V = Y / (float)H;
            const float Grain = PFbm(U * 32, V * 2, 32, 2, 3, 51), Patch = PFbm(U * 4, V * 3, 4, 3, 3, 52);
            float L = .16f + .07f * (Grain - .5f) + .05f * (Patch - .5f);
            const float Grey = Smooth(.4f, .8f, Patch);
            FLinearColor C = Mix(FLinearColor(L * 1.25f, L * .98f, L * .72f), FLinearColor(L * 1.05f, L * 1.02f, L * .98f), Grey * .7f);
            Img.At(X, Y) = C;
            Height[Y * W + X] = Grain * .5f;
        }
    for (int32 K = 0; K < 9; ++K)   // drying checks
    {
        const int32 X = (int32)(Hash2(K, 1, 53) * W), Y0 = (int32)(Hash2(K, 2, 53) * H), Len = 60 + (int32)(Hash2(K, 3, 53) * 260);
        for (int32 S = 0; S < Len; ++S)
        {
            const int32 QX = ((X + (int32)(3 * FMath::Sin(S * .02f + K))) % W + W) % W, QY = (Y0 + S) % H;
            Img.At(QX, QY) = Img.At(QX, QY) * .35f; Height[QY * W + QX] = -.8f;
        }
    }
}

void PaintManhole(FHallImage& Img, TArray<float>& Height)
{
    // Cast-iron lid seen from above: a raised grid inside two rings and SEWER across the middle; worn high points shine.
    const int32 N = 256;
    Img.Init(N, N, FLinearColor(.05f, .046f, .042f)); Height.Init(0.f, N * N);
    const float C = N * .5f;
    FHallImage Text; Text.Init(N, N, FLinearColor::Black);
    FHallTextStyle S; S.Height = 30; S.Stroke = 6; S.Spacing = .22f;
    HallDrawText(Text, TEXT("SEWER"), FVector2D((N - HallTextWidth(TEXT("SEWER"), S)) * .5f, C + 15), S, FLinearColor::White);
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const float DX = X + .5f - C, DY = Y + .5f - C, R = FMath::Sqrt(DX * DX + DY * DY) / C;
            float Raised = 0;
            if (R > .9f) Raised = 1;
            else if (R > .8f) Raised = 0;
            else if (R > .74f) Raised = 1;
            else
            {
                const bool bBand = FMath::Abs(DY) < 24;
                if (bBand) Raised = Text.At(X, Y).R > .5f ? 1.f : 0.f;
                else
                {
                    const float GX = FMath::Fmod(FMath::Abs(DX + DY) / 1.414f, 22.f), GY = FMath::Fmod(FMath::Abs(DX - DY) / 1.414f, 22.f);
                    Raised = (GX < 5 || GY < 5) ? 1.f : 0.f;
                }
            }
            const float Wear = PFbm(X * .05f, Y * .05f, 0, 0, 3, 61);
            const float L = Raised > .5f ? .085f + .07f * Wear : .045f + .02f * Wear;
            Img.At(X, Y) = FLinearColor(L * 1.04f, L, L * .93f);
            Height[Y * N + X] = Raised;
        }
}

void PaintHalfRoad(FHallImage& Img, int32 Variant)
{
    // One 1000 x 680 cm decal from the kerb face (V = 0) to the centre line (V = 1). Continuous features use the same
    // seeds in every variant and repeat along U, so neighbouring segments join without seams; spots stay off the ends.
    const int32 W = 1024, H = 512;
    Img.Init(W, H, FLinearColor(0, 0, 0, 0));
    const float CmU = 1000.f / W, CmV = 680.f / H;
    FStRng R(9100 + Variant * 37);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float U = X / (float)W, Vc = Y * CmV;
            // Broad tone: darker oily patches and paler sun-bleached ones.
            const float Blot = PFbm(U * 7, Vc / 120.f, 7, 0, 3, 101);
            if (Blot > .54f) Over(Img, X, Y, FLinearColor(.030f, .030f, .032f), .30f * (Blot - .54f) / .46f);
            if (Blot < .40f) Over(Img, X, Y, FLinearColor(.14f, .138f, .132f), .22f * (.40f - Blot) / .40f);
            // Gutter grime and the wet line at the kerb.
            if (Vc < 60.f)
            {
                const float G = 1.f - Smooth(28.f, 58.f, Vc);
                Over(Img, X, Y, FLinearColor(.06f, .05f, .037f), G * (.30f + .32f * PFbm(U * 48, Vc * .09f, 48, 0, 3, 102)));
                if (Vc < 7.f) Over(Img, X, Y, FLinearColor(.026f, .024f, .02f), .55f * (1.f - Vc / 7.f));
            }
            // Wheel paths of the travel lane (polished, a touch darker) with streaks along the lane.
            for (const float Path : { ParkW + 115.f, ParkW + 275.f })
            {
                const float D = FMath::Abs(Vc - Path);
                if (D < 44.f)
                {
                    const float Band = 1.f - Smooth(20.f, 44.f, D);
                    Over(Img, X, Y, FLinearColor(.026f, .026f, .028f), Band * (.18f + .2f * PFbm(U * 9, Vc * .35f, 9, 0, 3, 103)));
                }
            }
            // The oil strip down the middle of the lane, and a faint one in the parking lane.
            const float OilD = FMath::Abs(Vc - (ParkW + 195.f));
            if (OilD < 30.f) Over(Img, X, Y, FLinearColor(.018f, .017f, .015f), (1.f - Smooth(10.f, 30.f, OilD)) * (.24f + .26f * PFbm(U * 30, Vc * .2f, 30, 0, 3, 104)));
            const float ParkOil = FMath::Abs(Vc - 150.f);
            if (ParkOil < 55.f) Over(Img, X, Y, FLinearColor(.025f, .023f, .02f), (1.f - Smooth(20.f, 55.f, ParkOil)) * .10f * PFbm(U * 14, Vc * .1f, 14, 0, 2, 105));
            // Grime along the centre line.
            if (Vc > 640.f) Over(Img, X, Y, FLinearColor(.04f, .038f, .035f), .14f * Smooth(640.f, 680.f, Vc));
        }
    // Oil drips in the lane centre: dense small spots.
    for (int32 K = 0; K < 260; ++K)
    {
        const float Ucm = (float)R.Range(12, 988), Vc = ParkW + 195.f + (float)(R.Range(-1, 1) * R.Range(0, 24));
        Dab(Img, Ucm / CmU, Vc / CmV, (float)R.Range(1.2, 4.5), FLinearColor(.012f, .011f, .01f), (float)R.Range(.35, .8), .5f, true);
    }
    // Where cars park: a couple of clustered stains under engines, with a faint halo.
    for (int32 Cl = 0; Cl < 2; ++Cl)
    {
        const float CU = (float)R.Range(150, 850) + (Cl ? 0.f : 0.f), CV = (float)R.Range(120, 200);
        Dab(Img, CU / CmU, CV / CmV, (float)R.Range(40, 70) / CmU, FLinearColor(.025f, .023f, .02f), .18f, .2f, true);
        const int32 Spots = (int32)R.Range(5, 12);
        for (int32 S = 0; S < Spots; ++S)
            Dab(Img, (CU + (float)R.Range(-55, 55)) / CmU, (CV + (float)R.Range(-25, 25)) / CmV, (float)R.Range(4, 22) / CmU,
                FLinearColor(.012f, .011f, .01f), (float)R.Range(.35, .7), .45f, true);
    }
    // A few lighter scuffs and debris specks.
    for (int32 K = 0; K < 120; ++K)
        Dab(Img, (float)R.Range(10, W - 10), (float)R.Range(2, H - 2), (float)R.Range(.6, 1.6), FLinearColor(.16f, .155f, .15f), (float)R.Range(.2, .5), 1.f, true);
}

void PaintLine(FHallImage& Img, bool bDouble)
{
    // Worn traffic paint: 1024 px = 1000 cm. Double yellow: two 10 cm lines 12 cm apart in a 64 cm strip.
    // White: one 10 cm line in a 32 cm strip. Ragged edges, missing flakes, cracks through it and road grime.
    const int32 W = 1024, H = bDouble ? 64 : 32;
    Img.Init(W, H, FLinearColor(0, 0, 0, 0));
    const FLinearColor Paint = bDouble ? FLinearColor(.60f, .43f, .045f) : FLinearColor(.62f, .62f, .60f);
    const FLinearColor Dirt = bDouble ? FLinearColor(.24f, .19f, .07f) : FLinearColor(.22f, .22f, .21f);
    const float Lines[2][2] = { { bDouble ? 16.f : 11.f, bDouble ? 26.f : 21.f }, { 38.f, 48.f } };
    const int32 Seed = bDouble ? 300 : 400;
    TArray<float> Cracks;
    for (int32 K = 0; K < 9; ++K) Cracks.Add(Hash2(K, 1, Seed) * W);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float U = X / (float)W;
            float Cov = 0;
            for (int32 L = 0; L < (bDouble ? 2 : 1); ++L)
            {
                const float E0 = Lines[L][0] + (PNoise(U * 240, L * 3.f, 240, 0, Seed + 1) - .5f) * 1.6f;
                const float E1 = Lines[L][1] + (PNoise(U * 240, L * 3.f + 1.5f, 240, 0, Seed + 2) - .5f) * 1.6f;
                Cov = FMath::Max(Cov, FMath::Clamp(FMath::Min(Y + .5f - E0, E1 - (Y + .5f)) + .5f, 0.f, 1.f));
            }
            if (Cov <= 0) continue;
            const float Flake = PFbm(U * 70, Y * .22f, 70, 0, 4, Seed + 3), Thin = PFbm(U * 9, Y * .05f, 9, 0, 2, Seed + 4);
            float A = Cov * Smooth(.28f, .46f, Flake) * (.55f + .4f * Thin) * (bDouble ? .92f : .85f);
            for (const float CX : Cracks) if (FMath::Abs(X + .5f - (CX + 6.f * FMath::Sin(Y * .4f + CX))) < .9f) A = 0;
            if (A <= 0) continue;
            Over(Img, X, Y, Mix(Paint, Dirt, .55f * PFbm(U * 35, Y * .3f, 35, 0, 3, Seed + 5)), A);
        }
}

void PaintCrack(FHallImage& Img, int32 Seed)
{
    // A sealed transverse crack: 1024 px = 680 cm along, 64 px = 40 cm across. Glossy tar band with a squeegee smear.
    const int32 W = 1024, H = 64;
    Img.Init(W, H, FLinearColor(0, 0, 0, 0));
    FStRng R(Seed);
    TArray<FVector2D> P;
    double Y = H * .5, Drift = 0;
    for (double X = W * .015; X < W * .985; X += 4)
    {
        Drift = FMath::Clamp(Drift * .9 + R.Range(-1.3, 1.3), -2.0, 2.0);
        if (Y > H * .5 + 14.0) Drift -= .6;
        if (Y < H * .5 - 14.0) Drift += .6;
        Y = FMath::Clamp(Y + Drift, H * .5 - 21.0, H * .5 + 21.0);
        P.Add(FVector2D(X, Y + R.Range(-.8, .8)));
    }
    const FLinearColor Tar(.012f, .012f, .012f);
    Brush(Img, P, 19.f, Tar, .42f, Seed + 1, true, .3f);
    Brush(Img, P, 9.f, Tar, .95f, Seed + 2, true, .05f);
    if (R.Chance(.7))
    {
        TArray<FVector2D> Branch;
        const int32 From = (int32)R.Range(20, P.Num() - 40);
        double BX = P[From].X, BY = P[From].Y;
        const double Dir = R.Chance(.5) ? 1.0 : -1.0;
        for (int32 S = 0; S < 26; ++S) { BX += 5; BY = FMath::Clamp(BY + Dir * R.Range(.2, 1.4), 4.0, H - 4.0); Branch.Add(FVector2D(BX, BY)); }
        Brush(Img, Branch, 7.f, Tar, .9f, Seed + 3, true, .1f);
    }
    // Taper the ends.
    for (int32 Yy = 0; Yy < H; ++Yy)
        for (int32 X = 0; X < W; ++X)
        {
            const float E = FMath::Min(X, W - 1 - X) / (W * .03f);
            if (E < 1.f) Img.At(X, Yy).A *= E;
        }
}

void PaintPatch(FHallImage& Img)
{
    // Square utility patch: newer, darker asphalt with straight, slightly ragged edges and a sealed border.
    const int32 N = 256;
    Img.Init(N, N, FLinearColor(0, 0, 0, 0));
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const float E = FMath::Min(FMath::Min(X + .5f, N - X - .5f), FMath::Min(Y + .5f, N - Y - .5f)) - 6.f + (PNoise(X * .08f, Y * .08f, 0, 0, 501) - .5f) * 4.f;
            if (E <= 0) continue;
            const float A = FMath::Clamp(E, 0.f, 1.f);
            float L = .040f + .008f * (PFbm(X * .03f, Y * .03f, 0, 0, 3, 502) - .5f);
            if (Hash2(X, Y, 503) > .93f) L = .10f;
            if (E < 6.f) L = .016f;
            Over(Img, X, Y, FLinearColor(L, L, L * 1.03f), A * .96f);
        }
}

void PaintOil(FHallImage& Img)
{
    const int32 N = 256;
    Img.Init(N, N, FLinearColor(0, 0, 0, 0));
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const float DX = (X + .5f) / N * 2 - 1, DY = (Y + .5f) / N * 2 - 1;
            const float Ang = FMath::Atan2(DY, DX);
            const float Edge = .62f + .3f * PFbm(FMath::Cos(Ang) * 2.2f + 3.f, FMath::Sin(Ang) * 2.2f + 3.f, 0, 0, 3, 601);
            const float R = FMath::Sqrt(DX * DX + DY * DY) / Edge;
            if (R >= 1.f) continue;
            const float A = (1.f - Smooth(.45f, 1.f, R)) * (.55f + .3f * PFbm(X * .04f, Y * .04f, 0, 0, 3, 602));
            Over(Img, X, Y, FLinearColor(.012f, .011f, .01f), A);
        }
}

void PaintPuff(FHallImage& Img)
{
    // Smoke puff: soft, lumpy and thin at the rim; colour stays white (the material tints it).
    const int32 N = 256;
    Img.Init(N, N, FLinearColor(1, 1, 1, 0));
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const float DX = (X + .5f) / N * 2 - 1, DY = (Y + .5f) / N * 2 - 1;
            const float R2 = DX * DX + DY * DY;
            if (R2 >= 1.f) continue;
            const float Lumps = PFbm(X * .025f, Y * .025f, 0, 0, 4, 701);
            const float A = FMath::Square(1.f - R2) * FMath::Clamp(.25f + 1.1f * (Lumps - .35f), 0.f, 1.f);
            Img.At(X, Y) = FLinearColor(1, 1, 1, FMath::Clamp(A, 0.f, 1.f));
        }
}

/** The hand-painted bedsheet banner (front). 2048 x 192 over 1100 x 104 cm. */
void PaintBanner(FHallImage& Img, FHallImage& Back)
{
    const int32 W = 2048, H = 192;
    const FLinearColor Sheet(.80f, .775f, .715f);
    Img.Init(W, H, Sheet);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            // Weave, soft creases from folding, a grubby edge and the stitched hems.
            const float Weave = 1.f + .025f * (Hash2(X, Y, 801) - .5f) + ((X % 3 == 0) ? -.01f : 0.f);
            const float Crease = 1.f - .06f * Smooth(.55f, .9f, PNoise(X * .006f, Y * .02f, 0, 0, 802)) - .05f * Smooth(.92f, 1.f, FMath::Abs(FMath::Sin(X * .0061f + 1.3f)));
            const float Edge = FMath::Min(FMath::Min(X, W - 1 - X) / 40.f, FMath::Min(Y, H - 1 - Y) / 14.f);
            const float Grime = 1.f - .10f * (1.f - FMath::Clamp(Edge, 0.f, 1.f)) * PFbm(X * .02f, Y * .05f, 0, 0, 2, 803);
            float L = Weave * Crease * Grime;
            if (Y == 11 || Y == 12 || Y == H - 12 || Y == H - 13) L *= .88f;
            if ((Y == 9 || Y == H - 10) && (X / 5) % 2 == 0) L *= .9f;
            Img.At(X, Y) = Sheet * L;
        }
    const FLinearColor Black(.018f, .017f, .016f), Orange(.86f, .30f, .025f), Green(.10f, .20f, .05f), Purple(.22f, .05f, .26f);
    // Pumpkins at both ends.
    auto Pumpkin = [&](float CX, float CY, float R, int32 Seed)
    {
        Img.Ellipse(FVector2D(CX, CY + 2), FVector2D(R * 1.18f + 5, R + 5), 0, Black);
        for (int32 K = -2; K <= 2; ++K)
            Img.Ellipse(FVector2D(CX + K * R * .34f, CY), FVector2D(R * (.62f - .07f * FMath::Abs(K)), R * (1.f - .05f * FMath::Abs(K))), 0,
                K % 2 == 0 ? Orange : FLinearColor(.78f, .25f, .02f));
        for (int32 K = -1; K <= 1; K += 2)
        {
            TArray<FVector2D> Rib;
            for (int32 S = 0; S <= 10; ++S) { const float T = S / 10.f; Rib.Add(FVector2D(CX + K * R * .3f + K * FMath::Sin(T * 3.14159f) * R * .12f, CY - R * .92f + T * R * 1.84f)); }
            Brush(Img, Rib, 4.f, FLinearColor(.5f, .14f, .01f), .9f, Seed, false);
        }
        Brush(Img, { FVector2D(CX - 2, CY - R * .9f), FVector2D(CX + 3, CY - R * 1.18f), FVector2D(CX + 10, CY - R * 1.3f) }, 9.f, Green, 1.f, Seed + 1, false);
    };
    // Maple leaves where the bats used to be.
    auto Leaf = [&](float CX, float CY, float S, float Tilt, const FLinearColor& C)
    {
        TArray<FVector2D> P;
        for (int32 I = 0; I < 36; ++I)
        {
            const float A = I / 36.f * 2 * 3.14159f;
            const float Lobe = .55f + .45f * FMath::Abs(FMath::Cos(A * 2.5f + .3f)) * (FMath::Cos(A) > -.4f ? 1.f : .6f);
            P.Add(FVector2D(CX + S * Lobe * FMath::Cos(A + Tilt), CY + S * Lobe * FMath::Sin(A + Tilt)));
        }
        Img.FillPolygon(P, C);
        Brush(Img, { FVector2D(CX, CY), FVector2D(CX - S * .9f * FMath::Sin(Tilt), CY + S * .9f * FMath::Cos(Tilt)) }, 3.f, Black, .8f, 861, false);
    };
    const FLinearColor Gold(.85f, .58f, .08f), Rust(.60f, .17f, .03f);
    Pumpkin(112, 104, 54, 811);
    Pumpkin(W - 112, 104, 54, 812);
    Leaf(262, 44, 40, -.15f, Rust); Leaf(330, 128, 24, .25f, Gold); Leaf(W - 262, 46, 40, .18f, Gold); Leaf(W - 338, 132, 22, -.2f, Rust); Leaf(1010, 24, 20, .1f, Rust);
    // FALL and JAM in green over orange, HARVEST in rust over gold; the band in purple under it, the date small at the bottom.
    const float CapH = 80, WScale = .84f, Space = .26f;
    const FString Word1 = TEXT("FALL"), Word2 = TEXT("HARVEST"), Word3 = TEXT("JAM");
    const float GapW = 56.f;
    const float W1 = HandTextWidth(Word1, CapH, WScale, Space), W2 = HandTextWidth(Word2, CapH, WScale, Space), W3 = HandTextWidth(Word3, CapH, WScale, Space);
    const float Left = (W - (W1 + GapW + W2 + GapW + W3)) * .5f, Base = 100.f;
    TArray<TArray<FVector2D>> S1, S2, S3, S4;
    HandText(S1, Word1, Left, Base, CapH, WScale, Space, 5.f, 821);
    HandText(S2, Word2, Left + W1 + GapW, Base, CapH, WScale, Space, 5.f, 822);
    HandText(S1, Word3, Left + W1 + GapW + W2 + GapW, Base, CapH, WScale, Space, 5.f, 824);
    for (const TArray<FVector2D>& P : S1) Brush(Img, P, 22.f, Orange, 1.f, 832, false, .05f);
    for (const TArray<FVector2D>& P : S2) Brush(Img, P, 22.f, Gold, 1.f, 831, false, .05f);
    for (const TArray<FVector2D>& P : S1) Brush(Img, P, 13.f, Green, 1.f, 834, false, .03f);
    for (const TArray<FVector2D>& P : S2) Brush(Img, P, 13.f, Rust, 1.f, 833, false, .03f);
    Drips(Img, S1, 13.f, Green, .4f, 16.f, 842, 118.f);
    Drips(Img, S2, 13.f, Rust, .45f, 16.f, 841, 118.f);
    const FString Feat = TEXT("FEAT: PROJECT JAYSTORM");
    const float FeatH = 34.f, FW = HandTextWidth(Feat, FeatH, .9f, .3f);
    HandText(S4, Feat, (W - FW) * .5f, 148.f, FeatH, .9f, .3f, 2.f, 825);
    for (const TArray<FVector2D>& P : S4) Brush(Img, P, 9.f, Purple, 1.f, 836, false, .03f);
    const FString Date = TEXT("SAT OCT 18 1986");
    const float DateH = 20.f, DW = HandTextWidth(Date, DateH, .9f, .3f);
    HandText(S3, Date, (W - DW) * .5f, 180.f, DateH, .9f, .3f, 1.5f, 823);
    for (const TArray<FVector2D>& P : S3) Brush(Img, P, 5.f, Purple, 1.f, 835, false);
    // Flicked spatter.
    FStRng R(851);
    for (int32 K = 0; K < 70; ++K)
        Dab(Img, (float)R.Range(180, W - 180), (float)R.Range(18, H - 18), (float)R.Range(.8, 2.6), R.Chance(.5) ? Rust : Gold, .9f, 1.f, false);
    // Back: the same sheet with the paint bleeding faintly through (same UVs, so it reads mirrored from behind).
    Back.Init(W / 2, H / 2, Sheet);
    for (int32 Y = 0; Y < H / 2; ++Y)
        for (int32 X = 0; X < W / 2; ++X)
        {
            FLinearColor F = (Img.At(X * 2, Y * 2) + Img.At(X * 2 + 1, Y * 2) + Img.At(X * 2, Y * 2 + 1) + Img.At(X * 2 + 1, Y * 2 + 1)) * .25f;
            Back.At(X, Y) = Mix(Sheet * .97f, F, .22f);
        }
}

struct FSignLine { const TCHAR* Text; float Baseline, Height; };
void PaintSign(FHallImage& Img, int32 W, int32 H, const FLinearColor& Field, const FLinearColor& Ink, std::initializer_list<FSignLine> Lines, float Border)
{
    Img.Init(W, H, Ink);
    Img.Rect(FVector2D(Border, Border), FVector2D(W - Border, H - Border), Field);
    for (const FSignLine& L : Lines)
    {
        FHallTextStyle S; S.Height = L.Height; S.Stroke = FMath::Max(1.5f, L.Height * .16f); S.Spacing = .2f;
        S.WidthScale = FMath::Min(1.f, (W - 2 * Border - 8) / FMath::Max(1.f, HallTextWidth(L.Text, S)));
        HallDrawText(Img, L.Text, FVector2D((W - HallTextWidth(L.Text, S)) * .5f, L.Baseline), S, Ink);
    }
}

void PaintNewspaper(FHallImage& Img)
{
    // Front page in the box window: masthead, the big headline, a photo and columns of grey type.
    const int32 W = 256, H = 320;
    const FLinearColor Paper(.72f, .70f, .64f), Ink(.03f, .03f, .035f), Type(.36f, .35f, .33f);
    Img.Init(W, H, Paper);
    FHallTextStyle M; M.Height = 22; M.Stroke = 3.6f; M.Spacing = .14f;
    M.WidthScale = FMath::Min(1.f, 236.f / HallTextWidth(TEXT("CAMPUS COURIER"), M));
    HallDrawText(Img, TEXT("CAMPUS COURIER"), FVector2D((W - HallTextWidth(TEXT("CAMPUS COURIER"), M)) * .5f, 34), M, Ink);
    Img.Rect(FVector2D(8, 40), FVector2D(W - 8, 42), Ink);
    FHallTextStyle D; D.Height = 7; D.Stroke = 1.2f; D.Spacing = .2f;
    HallDrawText(Img, TEXT("SATURDAY OCT 18 1986"), FVector2D(10, 52), D, Ink);
    HallDrawText(Img, TEXT("25 CENTS"), FVector2D(W - 10 - HallTextWidth(TEXT("25 CENTS"), D), 52), D, Ink);
    Img.Rect(FVector2D(8, 56), FVector2D(W - 8, 57), Ink);
    FHallTextStyle Hd; Hd.Height = 30; Hd.Stroke = 6.f; Hd.Spacing = .12f;
    Hd.WidthScale = FMath::Min(1.f, 236.f / HallTextWidth(TEXT("HARVEST"), Hd));
    HallDrawText(Img, TEXT("HARVEST"), FVector2D((W - HallTextWidth(TEXT("HARVEST"), Hd)) * .5f, 96), Hd, Ink);
    Hd.WidthScale = FMath::Min(1.f, 236.f / HallTextWidth(TEXT("WEEKEND"), Hd));
    HallDrawText(Img, TEXT("WEEKEND"), FVector2D((W - HallTextWidth(TEXT("WEEKEND"), Hd)) * .5f, 134), Hd, Ink);
    Img.Rect(FVector2D(10, 146), FVector2D(150, 240), FLinearColor(.25f, .24f, .23f));
    Img.Ellipse(FVector2D(80, 205), FVector2D(34, 27), 0, FLinearColor(.5f, .48f, .45f));
    Img.Rect(FVector2D(76, 172), FVector2D(84, 182), FLinearColor(.15f, .15f, .14f));
    for (int32 L = 0; L < 26; ++L)
    {
        const float Y = 150.f + L * 6.2f;
        if (Y > H - 8) break;
        if (Y < 244) Img.Rect(FVector2D(158, Y), FVector2D(W - 10.f - (L % 5 == 4 ? 30.f : 0.f), Y + 2.4f), Type);
        else { Img.Rect(FVector2D(10, Y), FVector2D(122, Y + 2.4f), Type); Img.Rect(FVector2D(134, Y), FVector2D(W - 10.f, Y + 2.4f), Type); }
    }
}

void PaintBag(FHallImage& Img)
{
    // Kraft charcoal sack wrapped once around (U), bottom to rolled top (V).
    const int32 W = 256, H = 256;
    Img.Init(W, H, FLinearColor(.40f, .28f, .16f));
    for (int32 Y = 0; Y < H; ++Y) for (int32 X = 0; X < W; ++X) Img.At(X, Y) = Img.At(X, Y) * (.94f + .1f * PFbm(X * .05f, Y * .05f, 0, 0, 2, 901));
    for (int32 Face = 0; Face < 2; ++Face)
    {
        const float X0 = Face * 128.f + 8.f;
        Img.Rect(FVector2D(X0, 70), FVector2D(X0 + 112, 150), FLinearColor(.03f, .03f, .03f));
        Img.Rect(FVector2D(X0, 150), FVector2D(X0 + 112, 160), FLinearColor(.55f, .06f, .02f));
        FHallTextStyle S; S.Height = 16; S.Stroke = 3; S.Spacing = .15f;
        S.WidthScale = FMath::Min(1.f, 104.f / HallTextWidth(TEXT("CHARCOAL"), S));
        HallDrawText(Img, TEXT("CHARCOAL"), FVector2D(X0 + (112 - HallTextWidth(TEXT("CHARCOAL"), S)) * .5f, 100), S, FLinearColor(.9f, .75f, .2f));
        S.Height = 11; S.Stroke = 2; S.WidthScale = FMath::Min(1.f, 104.f / HallTextWidth(TEXT("BRIQUETS"), S));
        HallDrawText(Img, TEXT("BRIQUETS"), FVector2D(X0 + (112 - HallTextWidth(TEXT("BRIQUETS"), S)) * .5f, 124), S, FLinearColor(.85f, .82f, .75f));
        Img.FillPolygon({ FVector2D(X0 + 44, 145), FVector2D(X0 + 56, 128), FVector2D(X0 + 52, 138), FVector2D(X0 + 62, 126), FVector2D(X0 + 68, 145) }, FLinearColor(.85f, .35f, .03f));
    }
}

void PaintFlyer(FHallImage& Img, int32 Kind)
{
    const int32 W = 128, H = 160;
    const FLinearColor Paper = Kind == 0 ? FLinearColor(.80f, .36f, .06f) : Kind == 1 ? FLinearColor(.78f, .78f, .74f) : FLinearColor(.70f, .72f, .40f);
    const FLinearColor Ink(.03f, .03f, .03f);
    Img.Init(W, H, Paper);
    TArray<TArray<FVector2D>> S;
    if (Kind == 0)
    {
        HandText(S, TEXT("FALL"), 36, 28, 15, .8f, .18f, 1.2f, 911);
        HandText(S, TEXT("HARVEST"), 20, 50, 15, .8f, .18f, 1.2f, 912);
        HandText(S, TEXT("JAM"), 42, 72, 15, .82f, .2f, 1.2f, 917);
        HandText(S, TEXT("FEAT PROJECT"), 30, 88, 8, .8f, .2f, .5f, 918);
        HandText(S, TEXT("JAYSTORM"), 32, 103, 11, .8f, .2f, .6f, 919);
        HandText(S, TEXT("SAT OCT 18"), 24, 120, 9.5f, .8f, .22f, .8f, 913);
        HandText(S, TEXT("1800 FRAT PARK DR"), 10, 134, 7, .78f, .2f, .5f, 914);
        for (const TArray<FVector2D>& P : S) Brush(Img, P, 2.6f, Ink, 1.f, 916, false, .05f);
        Img.Ellipse(FVector2D(64, 147), FVector2D(10, 8), 0, Ink);
    }
    else
    {
        HandText(S, Kind == 1 ? TEXT("ROOMMATE") : TEXT("GUITAR"), 12, 30, 15, .8f, .2f, .8f, 921 + Kind);
        HandText(S, Kind == 1 ? TEXT("WANTED") : TEXT("LESSONS"), 16, 54, 15, .8f, .2f, .8f, 923 + Kind);
        for (const TArray<FVector2D>& P : S) Brush(Img, P, 2.4f, Ink, 1.f, 925, false, .05f);
        for (int32 L = 0; L < 5; ++L) Img.Rect(FVector2D(14, 70.f + L * 9), FVector2D(W - 14.f - (L % 2) * 18, 71.6f + L * 9), FLinearColor(.25f, .25f, .25f));
        for (int32 T = 0; T < 7; ++T)   // tear-off phone tabs
        {
            const float X = 6.f + T * 17.f;
            Img.Rect(FVector2D(X, 122), FVector2D(X + .8f, H - 2.f), FLinearColor(.4f, .4f, .38f));
            if (T == 2 || T == 5) Img.Rect(FVector2D(X + 1, 122), FVector2D(X + 16, H - 1.f), FLinearColor(.13f, .10f, .075f));
            else for (int32 K = 0; K < 3; ++K) Img.Rect(FVector2D(X + 6, 128.f + K * 9), FVector2D(X + 8, 134.f + K * 9), FLinearColor(.2f, .2f, .2f));
        }
    }
}

void PaintBox(FHallImage& Img)
{
    // News box band: paper name across the top front, white on the box colour.
    const int32 W = 512, H = 96;
    Img.Init(W, H, FLinearColor(.03f, .14f, .06f));
    FHallTextStyle S; S.Height = 44; S.Stroke = 8; S.Spacing = .14f;
    S.WidthScale = FMath::Min(1.f, 480.f / HallTextWidth(TEXT("CAMPUS COURIER"), S));
    HallDrawText(Img, TEXT("CAMPUS COURIER"), FVector2D((W - HallTextWidth(TEXT("CAMPUS COURIER"), S)) * .5f, 68), S, FLinearColor(.86f, .86f, .82f));
    Img.Rect(FVector2D(10, 80), FVector2D(W - 10, 84), FLinearColor(.86f, .86f, .82f));
}

// ------------------------------------------------------------------ props: built once in local space, placed by frame
void AppendTransformed(FHallMesh& Dst, const FHallMesh& Src, const FStFrame& F, const FVector& Scale = FVector(1, 1, 1))
{
    for (const FName& Key : Src.Order)
    {
        const FHallSection& S = Src.Sections[Key];
        FHallSection& D = Dst.Sec(Key);
        const int32 Base = D.V.Num();
        for (int32 I = 0; I < S.V.Num(); ++I)
        {
            D.V.Add(F.P(S.V[I] * Scale));
            D.N.Add(F.D(FVector(S.N[I].X / Scale.X, S.N[I].Y / Scale.Y, S.N[I].Z / Scale.Z)).GetSafeNormal());
            D.UV.Add(S.UV[I]);
            D.T.Add(FProcMeshTangent(F.D(S.T[I].TangentX * Scale).GetSafeNormal(), S.T[I].bFlipTangentY));
        }
        for (int32 I = 0; I < S.I.Num(); ++I) D.I.Add(Base + S.I[I]);
    }
}

/** The campus plan cuts the far sidewalk where its drives cross it; no leaf lies over those gaps (it would float above the
 *  ramps and the asphalt). The leaf's random draws are still made, so every other leaf stays where it was. */
bool LeafOverDriveGap(double X, double Y)
{
    if (Y < -5810.0 || Y > -5540.0 || !HouseCampusLayout::Enabled()) return false;
    static const TArray<FVector2D> Gaps = [] { TArray<FVector2D> G; HouseCampusLayout::FarSidewalkGaps(G); return G; }();
    for (const FVector2D& G : Gaps) if (X > G.X && X < G.Y) return true;
    return false;
}

void Leaf(FStWriter& W, FStRng& R, double X, double Y, double Z)
{
    static const TCHAR* Keys[] = { TEXT("ST_LeafOrange"), TEXT("ST_LeafBrown"), TEXT("ST_LeafYellow"), TEXT("ST_LeafRed") };
    const double Pick = R.Frac();
    const FName K = Keys[Pick < .34 ? 0 : Pick < .68 ? 1 : Pick < .86 ? 2 : 3];
    const double A = R.Range(0, 2 * StPi), L = R.Range(5.0, 10.0), Wd = L * R.Range(.5, .72);
    const FVector D(FMath::Cos(A), FMath::Sin(A), 0), S(-D.Y, D.X, 0);
    const FVector C(X, Y, Z);
    const FVector Tip = C + D * (L * .5) + FVector(0, 0, R.Range(.05, .4)), Base = C - D * (L * .5);
    const FVector Lf = C + S * (Wd * .5) - D * (L * .05) + FVector(0, 0, .1), Rt = C - S * (Wd * .5) - D * (L * .05) + FVector(0, 0, .1);
    if (LeafOverDriveGap(X, Y)) return;
    W.Tri(K, Base, Lf, Tip, FVector::UpVector);
    W.Tri(K, Base, Tip, Rt, FVector::UpVector);
}
/** Leaves drifted along a straight edge, bunched by a slow wave so they gather in clumps. */
void LeafEdge(FStWriter& W, FStRng& R, const FVector2D& A, const FVector2D& B, const FVector2D& Inward, double Band, double PerMetre, double Z)
{
    const double Len = (B - A).Size();
    const int32 N = (int32)(Len / 100.0 * PerMetre);
    const double Phase = R.Range(0, 6.28), Freq = R.Range(.004, .009);
    for (int32 I = 0; I < N; ++I)
    {
        const double T = R.Frac(), S = T * Len;
        const double Bunch = .5 + .5 * FMath::Sin(S * Freq + Phase);
        if (R.Frac() > .2 + .8 * Bunch * Bunch) continue;
        const FVector2D P = A + (B - A) * T + Inward * (R.Frac() * R.Frac() * Band);
        Leaf(W, R, P.X, P.Y, Z + R.Range(0, .3));
    }
}

void BuildCup(FHallMesh& M)
{
    // A 16 oz party cup: red outside, white inside, rolled rim. Origin at the bottom centre, axis +Z.
    FStWriter W(M);
    constexpr int32 Seg = 14;
    W.Cone(TEXT("ST_CupRed"), FVector(0, 0, 0), FVector(0, 0, 12), 3.0, 4.7, Seg, true, false);
    const double RB = 2.72, RT = 4.46, Z0 = .6, Z1 = 11.9, Rise = (RT - RB) / (Z1 - Z0);
    TArray<FVector> Floor;
    for (int32 S = 0; S < Seg; ++S)
    {
        const double A0 = 2 * StPi * S / Seg, A1 = 2 * StPi * (S + 1) / Seg;
        const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
        const FVector N0 = (-D0 + FVector(0, 0, Rise)).GetSafeNormal(), N1 = (-D1 + FVector(0, 0, Rise)).GetSafeNormal();
        W.Emit(TEXT("ST_CupWhite"), D0 * RB + FVector(0, 0, Z0), D1 * RB + FVector(0, 0, Z0), D1 * RT + FVector(0, 0, Z1), N0, N1, N1);
        W.Emit(TEXT("ST_CupWhite"), D0 * RB + FVector(0, 0, Z0), D1 * RT + FVector(0, 0, Z1), D0 * RT + FVector(0, 0, Z1), N0, N1, N0);
        Floor.Add(D0 * RB + FVector(0, 0, Z0));
    }
    W.Fan(TEXT("ST_CupWhite"), FVector(0, 0, Z0), Floor, FVector::UpVector);
    TArray<FVector> Rim;
    for (int32 S = 0; S <= Seg; ++S) { const double A = 2 * StPi * S / Seg; Rim.Add(FVector(FMath::Cos(A) * 4.72, FMath::Sin(A) * 4.72, 12.0)); }
    W.Tube(TEXT("ST_CupRed"), Rim, .38, 6);
}

void BuildKeg(FHallMesh& M, bool bTapped)
{
    // Stainless half-barrel: 59 cm tall, two rolling rings, chimes with handle cut-outs, Sankey valve. Origin at the bottom.
    FStWriter W(M);
    const FVector O(0, 0, 0), Z(0, 0, 1), X(1, 0, 0);
    W.Lathe(TEXT("ST_KegDull"), O, Z, X, { FVector2D(0, 2.4), FVector2D(13.5, 2.4), FVector2D(17.4, 1.1), FVector2D(19.3, 0), FVector2D(20.1, .5), FVector2D(20.15, 7), FVector2D(19.7, 7.6) }, 24, false);
    W.Lathe(TEXT("ST_Keg"), O, Z, X, { FVector2D(19.7, 7.6), FVector2D(20.4, 9.6), FVector2D(20.8, 14), FVector2D(21.35, 16.6), FVector2D(21.35, 18), FVector2D(20.8, 20.6),
        FVector2D(20.5, 29.5), FVector2D(20.8, 38.5), FVector2D(21.35, 41.2), FVector2D(21.35, 42.6), FVector2D(20.8, 45.2), FVector2D(20.4, 49.6), FVector2D(19.7, 51.4) }, 24, true);
    W.Lathe(TEXT("ST_KegDull"), O, Z, X, { FVector2D(19.7, 51.4), FVector2D(20.15, 52), FVector2D(20.15, 58.6), FVector2D(19.4, 59.2), FVector2D(18.6, 58.4) }, 24, false);
    W.Lathe(TEXT("ST_Keg"), O, Z, X, { FVector2D(18.6, 58.4), FVector2D(17.2, 57.2), FVector2D(13.5, 56.2), FVector2D(6, 55.6), FVector2D(3.6, 55.6) }, 24, true);
    W.Cone(TEXT("ST_KegValve"), FVector(0, 0, 55.4), FVector(0, 0, 57.8), 3.6, 3.4, 16, false, true);
    for (int32 K = 0; K < 2; ++K)
    {
        const double A = K * StPi;
        const FVector D(FMath::Cos(A), FMath::Sin(A), 0), T(-FMath::Sin(A), FMath::Cos(A), 0);
        W.BoxAx(TEXT("ST_PumpBlack"), D * 20.0 + FVector(0, 0, 55.4), D, T, Z, FVector(.45, 6.0, 1.8));
    }
    if (!bTapped) return;
    W.Cone(TEXT("ST_KegValve"), FVector(0, 0, 57.8), FVector(0, 0, 62.2), 5.0, 5.0, 16, true, true);
    W.Box(TEXT("ST_PumpBlack"), FVector(7.4, 0, 61.2), FVector(4.2, 1.3, 1.1));
    W.Cone(TEXT("ST_PumpBlack"), FVector(0, 0, 62.2), FVector(0, 0, 83.5), 2.5, 2.5, 12, false, true);
    W.Cone(TEXT("ST_KegValve"), FVector(0, 0, 83.5), FVector(0, 0, 88.5), .55, .55, 8, false, false);
    W.Cone(TEXT("ST_PumpBlack"), FVector(-6.5, 0, 88.5), FVector(6.5, 0, 88.5), 1.2, 1.2, 10, true, true);
    W.Tube(TEXT("ST_Hose"), { FVector(4.8, 1.5, 60.5), FVector(12, 3, 67), FVector(22, 4, 71), FVector(31, 4, 70.5), FVector(35, 3, 65), FVector(36.5, 2, 56) }, .8, 8);
    W.Box(TEXT("ST_PumpBlack"), FVector(36.5, 2, 52), FVector(1.6, 1.6, 4.2));
    W.Cone(TEXT("ST_PumpBlack"), FVector(36.5, 2, 47.8), FVector(36.5, 2, 45.5), 1.0, .7, 8, true, true);
    W.Box(TEXT("ST_PumpBlack"), FVector(36.5, 4.1, 55.2), FVector(.9, .6, 3.4));
}

void BuildTub(FHallMesh& M)
{
    // Grey plastic trash can full of ice, the keg standing in it. Origin at the tub's bottom centre.
    FStWriter W(M);
    const FVector O(0, 0, 0), Z(0, 0, 1), X(1, 0, 0);
    W.Lathe(TEXT("ST_TubGrey"), O, Z, X, { FVector2D(0, .6), FVector2D(25.4, .6), FVector2D(26.1, 0), FVector2D(26.7, 2.2), FVector2D(29.5, 63.5), FVector2D(31.0, 64.2),
        FVector2D(31.3, 66.8), FVector2D(29.7, 67.4), FVector2D(28.7, 66.2), FVector2D(28.2, 59) }, 28, false);
    TArray<FVector> Disc;
    for (int32 S = 0; S < 28; ++S) { const double A = 2 * StPi * S / 28; Disc.Add(FVector(FMath::Cos(A) * 28.3, FMath::Sin(A) * 28.3, 58.6)); }
    W.Fan(TEXT("ST_Ice"), FVector(0, 0, 58.6), Disc, FVector::UpVector);
    FStRng R(4401);
    for (int32 K = 0; K < 80; ++K)
    {
        const double A = R.Range(0, 2 * StPi), Rad = R.Range(21.8, 27.4), Sz = R.Range(1.1, 1.9);
        const FVector C(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, 58.6 + R.Range(0, 2.4));
        const FVector Ax = FStFrame::Rot(FVector(FMath::Cos(A * 3), FMath::Sin(A * 3), 0), FVector(FMath::Sin(A), -FMath::Cos(A), 0), R.Range(-.6, .6));
        const FVector Az = FStFrame::Rot(FVector::UpVector, FVector(FMath::Sin(A), -FMath::Cos(A), 0), R.Range(-.6, .6)).GetSafeNormal();
        FVector Ay = FVector::CrossProduct(Az, Ax); Ay.Normalize();
        const FVector Axx = FVector::CrossProduct(Ay, Az);
        W.BoxAx(TEXT("ST_Ice"), C, Axx, Ay, Az, FVector(Sz, Sz * R.Range(.8, 1.2), Sz * R.Range(.7, 1.1)));
    }
}

/** Folding aluminium lawn chair with woven webbing. Local: +X is where the sitter faces, origin on the ground. */
void BuildChair(FHallMesh& M, const TCHAR* WebAcross, const TCHAR* WebAlong)
{
    FStWriter W(M);
    const FName Frame(TEXT("ST_ChairFrame"));
    const double R = 1.2;
    auto SeatZ = [](double X) { return 31.0 + (X + 20.0) / 42.0 * 5.0; };
    for (int32 Sd = -1; Sd <= 1; Sd += 2)
    {
        const double Y = Sd * 27.5;
        W.Tube(Frame, { FVector(22, Y, 36), FVector(-20, Y, 31) }, R, 6);                          // seat rail
        W.Tube(Frame, { FVector(-20, Y, 31), FVector(-31, Y, 88) }, R, 6);                         // back post
        W.Tube(Frame, { FVector(24, Y, 1.2), FVector(22, Y, 36), FVector(23, Sd * 28.6, 56) }, R, 6); // front leg up into the arm post
        W.Tube(Frame, { FVector(-24, Y, 1.2), FVector(-12, Y, 33.4) }, R, 6);                      // rear leg
        W.Box(TEXT("ST_ArmPad"), FVector(.5, Sd * 29.6, 56.9), FVector(24.8, 2.8, .8));            // arm rest
        W.Tube(Frame, { FVector(-24.6, Y, 55.5), FVector(-24.6, Sd * 29.4, 56.2) }, R * .8, 6);    // arm to back post
    }
    W.Tube(Frame, { FVector(22, -27.5, 36), FVector(22, 27.5, 36) }, R, 6);
    W.Tube(Frame, { FVector(-20, -27.5, 31), FVector(-20, 27.5, 31) }, R, 6);
    W.Tube(Frame, { FVector(-31, -27.5, 88), FVector(-31, 27.5, 88) }, R, 6);
    W.Tube(Frame, { FVector(24, -27.5, 1.2), FVector(24, 27.5, 1.2) }, R, 6);
    W.Tube(Frame, { FVector(-24, -27.5, 1.2), FVector(-24, 27.5, 1.2) }, R, 6);
    // Two-sided strap: a sagging band from A to B, Wd wide, lifted Lift along N.
    auto Strap = [&](const TCHAR* Key, const FVector& A, const FVector& B, const FVector& Across, const FVector& N, double Wd, double Sag)
    {
        const int32 Steps = 4;
        for (int32 S = 0; S < Steps; ++S)
        {
            const double T0 = (double)S / Steps, T1 = (double)(S + 1) / Steps;
            const FVector P0 = A + (B - A) * T0 - N * (Sag * 4 * T0 * (1 - T0)), P1 = A + (B - A) * T1 - N * (Sag * 4 * T1 * (1 - T1));
            const FVector H = Across * (Wd * .5);
            W.Quad(Key, P0 - H, P1 - H, P1 + H, P0 + H, N);
            W.Quad(Key, P0 - H - N * .25, P1 - H - N * .25, P1 + H - N * .25, P0 + H - N * .25, -N);
        }
    };
    const FVector Up(0, 0, 1);
    for (int32 K = 0; K < 6; ++K)   // seat, across
    {
        const double X = -16.5 + K * 7.0;
        Strap(WebAcross, FVector(X, -27.5, SeatZ(X) + .4), FVector(X, 27.5, SeatZ(X) + .4), FVector(1, 0, 0), Up, 5.4, 1.6);
    }
    for (int32 K = 0; K < 6; ++K)   // seat, along
    {
        const double Y = -22.5 + K * 9.0;
        Strap(WebAlong, FVector(22, Y, 36.7), FVector(-20, Y, 31.7), FVector(0, 1, 0), Up, 5.4, 1.4);
    }
    const FVector BackUp = FVector(-11, 0, 57).GetSafeNormal(), BackN = FVector(57, 0, 11).GetSafeNormal();
    for (int32 K = 0; K < 7; ++K)   // back, across
    {
        const FVector C = FVector(-20, 0, 31) + BackUp * (6.0 + K * 7.6) + BackN * .4;
        Strap(WebAcross, C + FVector(0, -27.5, 0), C + FVector(0, 27.5, 0), BackUp, BackN, 5.4, -1.4);
    }
    for (int32 K = 0; K < 6; ++K)   // back, along
    {
        const double Y = -22.5 + K * 9.0;
        Strap(WebAlong, FVector(-20, Y, 31) + BackN * .7, FVector(-31, Y, 88) + BackN * .7, FVector(0, 1, 0), BackN, 5.4, -1.2);
    }
}

void BuildGrill(FHallMesh& M)
{
    // 22 inch kettle grill, lid off, coals going, burgers and hot dogs on. Origin on the ground under the bowl.
    FStWriter W(M);
    const FVector O(0, 0, 0), Z(0, 0, 1), X(1, 0, 0);
    W.Lathe(TEXT("ST_GrillEnamel"), O, Z, X, { FVector2D(0, 43.5), FVector2D(9, 44.3), FVector2D(16, 46.6), FVector2D(21.5, 50), FVector2D(25.5, 54.5), FVector2D(27.8, 60),
        FVector2D(28.6, 66), FVector2D(28.7, 70), FVector2D(29.1, 70.6), FVector2D(28.4, 70.9), FVector2D(27.8, 69.8), FVector2D(27.6, 65.5), FVector2D(26.8, 60.5),
        FVector2D(24.6, 55.2), FVector2D(20.8, 50.9), FVector2D(15.5, 47.5), FVector2D(8.8, 45.2), FVector2D(0, 44.4) }, 32, true);
    // Ash catcher dish and the triangular rack between the legs.
    W.Lathe(TEXT("ST_GrillSteel"), FVector(0, 0, 23), Z, X, { FVector2D(0, 0), FVector2D(9.5, 0), FVector2D(12.5, 3.5), FVector2D(12.2, 3.8), FVector2D(9.2, .5), FVector2D(0, .5) }, 18, false);
    FVector Feet[3];
    for (int32 K = 0; K < 3; ++K)
    {
        const double A = StPi * .5 + K * 2 * StPi / 3;
        const FVector D(FMath::Cos(A), FMath::Sin(A), 0);
        const bool bWheel = K > 0;
        const FVector Top = D * 22.0 + FVector(0, 0, 51), Foot = D * 34.0 + FVector(0, 0, bWheel ? 8.5 : 1.0);
        W.Tube(TEXT("ST_GrillSteel"), { Top, Foot }, 1.3, 8, true);
        Feet[K] = D * 30.0 + FVector(0, 0, 22);
        if (bWheel)
        {
            const FVector T(-D.Y, D.X, 0);
            W.Cone(TEXT("ST_GrillWheel"), Foot - T * 1.8, Foot + T * 1.8, 8.3, 8.3, 16, true, true);
            W.Cone(TEXT("ST_GrillSteel"), Foot - T * 2.2, Foot + T * 2.2, 2.0, 2.0, 8, true, true);
        }
    }
    for (int32 K = 0; K < 3; ++K) W.Tube(TEXT("ST_GrillSteel"), { Feet[K], Feet[(K + 1) % 3] }, .6, 6);
    // Bakelite handles on the bowl.
    for (int32 K = 0; K < 2; ++K)
    {
        const double A = K * StPi;
        const FVector D(FMath::Cos(A), FMath::Sin(A), 0), T(-D.Y, D.X, 0);
        W.BoxAx(TEXT("ST_PumpBlack"), D * 32.5 + FVector(0, 0, 66.5), D, T, Z, FVector(1.3, 7.5, 1.6));
        for (int32 S = -1; S <= 1; S += 2) W.Tube(TEXT("ST_GrillSteel"), { D * 28.8 + T * (S * 6.0) + FVector(0, 0, 66.5), D * 31.4 + T * (S * 6.0) + FVector(0, 0, 66.5) }, .5, 6);
    }
    // The coal bed: a mound of briquettes, some glowing, some ashed over.
    TArray<FVector> Bed;
    for (int32 S = 0; S < 20; ++S) { const double A = 2 * StPi * S / 20; Bed.Add(FVector(FMath::Cos(A) * 19.5, FMath::Sin(A) * 19.5, 51.2)); }
    W.Fan(TEXT("ST_CoalBlack"), FVector(0, 0, 51.6), Bed, FVector::UpVector);
    FStRng R(5150);
    for (int32 K = 0; K < 58; ++K)
    {
        const double A = R.Range(0, 2 * StPi), Rad = 19.0 * FMath::Sqrt(R.Frac());
        const double H = 52.4 + 5.0 * (1.0 - FMath::Square(Rad / 19.5)) + R.Range(-.6, .6);
        const double Pick = R.Frac();
        const TCHAR* Key = Pick < .38 ? TEXT("ST_CoalGlow") : Pick < .52 ? TEXT("ST_CoalHot") : Pick < .84 ? TEXT("ST_CoalAsh") : TEXT("ST_CoalBlack");
        W.Box(Key, FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, H), FVector(R.Range(2.0, 2.6), R.Range(2.0, 2.6), R.Range(1.2, 1.6)), R.Range(0, 90));
    }
    // Cooking grate, with dinner on it.
    TArray<FVector> Ring;
    for (int32 S = 0; S <= 32; ++S) { const double A = 2 * StPi * S / 32; Ring.Add(FVector(FMath::Cos(A) * 26.4, FMath::Sin(A) * 26.4, 66.4)); }
    W.Tube(TEXT("ST_GrillSteel"), Ring, .45, 6);
    for (int32 K = -9; K <= 9; ++K)
    {
        const double Y = K * 2.75, H = FMath::Sqrt(FMath::Max(0.0, 26.4 * 26.4 - Y * Y));
        W.Tube(TEXT("ST_GrillSteel"), { FVector(-H, Y, 66.6), FVector(H, Y, 66.6) }, .32, 5);
    }
    for (const FVector& P : { FVector(-9, -8, 67), FVector(5, -11, 67), FVector(-4, 6, 67) })
        W.Cone(TEXT("ST_Burger"), P, P + FVector(0, 0, 1.7), 5.6, 5.4, 14, true, true);
    for (int32 K = 0; K < 3; ++K)
    {
        const FVector C(11 + K * 3.3, 5 + K * 1.2, 67.9);
        W.Tube(TEXT("ST_HotDog"), { C - FVector(0, 7, 0), C + FVector(0, 7, 0) }, 1.3, 8, true);
    }
}

void BuildLid(FHallMesh& M)
{
    // The kettle lid on its own: origin at the rim centre, dome up +Z, handle on top.
    FStWriter W(M);
    const FVector O(0, 0, 0), Z(0, 0, 1), X(1, 0, 0);
    W.Lathe(TEXT("ST_GrillEnamel"), O, Z, X, { FVector2D(29.0, 0), FVector2D(29.2, 1.5), FVector2D(28.2, 6), FVector2D(25.5, 11.5), FVector2D(20.5, 16.5), FVector2D(13, 20.3), FVector2D(0, 22),
        FVector2D(0, 21.2), FVector2D(12.6, 19.5), FVector2D(19.8, 15.8), FVector2D(24.7, 11), FVector2D(27.4, 5.8), FVector2D(28.3, 1.2), FVector2D(28.4, 0) }, 32, true);
    W.Box(TEXT("ST_PumpBlack"), FVector(0, 0, 26), FVector(7.5, 1.4, 1.5));
    for (int32 S = -1; S <= 1; S += 2) W.Tube(TEXT("ST_GrillSteel"), { FVector(S * 5.5, 0, 21.8), FVector(S * 5.5, 0, 25) }, .5, 6);
    W.Cone(TEXT("ST_GrillSteel"), FVector(9, 9, 20.4), FVector(9, 9, 21.4), 4, 4, 12, false, true);
}

void BuildCooler(FHallMesh& M)
{
    FStWriter W(M);
    W.Box(TEXT("ST_CoolerWhite"), FVector(0, 0, 15.5), FVector(29, 17, 14.5));
    W.Box(TEXT("ST_CoolerWhite"), FVector(0, 0, .7), FVector(27.5, 15.5, .7));
    W.Box(TEXT("ST_CoolerRed"), FVector(0, 0, 33.5), FVector(30, 18, 3.5));
    W.Box(TEXT("ST_CoolerRed"), FVector(0, 0, 37.4), FVector(26, 14, .5));
    for (int32 S = -1; S <= 1; S += 2)
    {
        W.Box(TEXT("ST_CoolerWhite"), FVector(S * 30.4, 0, 24), FVector(1.4, 7, 1.2));
        W.Box(TEXT("ST_CoolerWhite"), FVector(S * 29.8, -6, 21.5), FVector(.8, .8, 2.5));
        W.Box(TEXT("ST_CoolerWhite"), FVector(S * 29.8, 6, 21.5), FVector(.8, .8, 2.5));
    }
}

void BuildBag(FHallMesh& M)
{
    // Kraft charcoal sack, 30 x 15 x 42 cm with a rolled top; the label wraps around (texture U).
    FStWriter W(M);
    const double HX = 15, HY = 7.5, H = 42;
    const FVector C[4] = { FVector(HX, -HY, 0), FVector(HX, HY, 0), FVector(-HX, HY, 0), FVector(-HX, -HY, 0) };
    const double Edge[5] = { 0, 2 * HY, 2 * HY + 2 * HX, 4 * HY + 2 * HX, 4 * HY + 4 * HX };
    const double Per = Edge[4];
    for (int32 K = 0; K < 4; ++K)
    {
        const FVector A = C[K], B = C[(K + 1) % 4];
        const FVector Nn = ((A + B) * .5).GetSafeNormal();
        const FVector2D UV[4] = { FVector2D(Edge[K] / Per, 1), FVector2D(Edge[K + 1] / Per, 1), FVector2D(Edge[K + 1] / Per, 0), FVector2D(Edge[K] / Per, 0) };
        W.QuadUV(TEXT("ST_Bag"), A, B, B + FVector(0, 0, H), A + FVector(0, 0, H), Nn, UV);
    }
    W.Quad(TEXT("ST_Bag"), C[0] + FVector(0, 0, H), C[1] + FVector(0, 0, H), C[2] + FVector(0, 0, H), C[3] + FVector(0, 0, H), FVector::UpVector);
    W.Cone(TEXT("ST_Bag"), FVector(-HX, 0, H + 1.6), FVector(HX, 0, H + 1.6), 2.4, 2.4, 10, true, true);
}

/** Cobra-head luminaire on its arm, for a pole whose surface is PoleR from the axis at the bracket. */
void CobraHead(FStWriter& W, double PoleR)
{
    const FName Hw(TEXT("ST_PoleHardware"));
    W.Cone(Hw, FVector(0, 0, 690), FVector(0, 0, 700), PoleR + .6, PoleR + .6, 14, true, true);
    W.Cone(Hw, FVector(0, 0, 648), FVector(0, 0, 656), PoleR + .7, PoleR + .7, 14, true, true);
    W.Tube(Hw, { FVector(PoleR, 0, 697), FVector(50, 0, 712), FVector(100, 0, 722), FVector(160, 0, 727.5), FVector(203, 0, 729) }, 2.4, 8);
    W.Tube(Hw, { FVector(PoleR, 0, 652), FVector(45, 0, 682), FVector(86, 0, 713) }, 1.6, 6);
    // Housing: rounded top, flat underside, tapering to the nose.
    const double Xs[] = { 196, 204, 216, 232, 250, 268, 282, 292, 298 };
    const double Ws[] = { 7.5, 10.5, 14, 16, 16.5, 16, 14.5, 11, 6 };
    const double Hs[] = { 6.5, 8, 10, 11, 11.5, 11, 9.5, 7, 4 };
    const double ZB = 722;
    TArray<TArray<FVector>> Rings;
    for (int32 I = 0; I < 9; ++I)
    {
        TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
        for (int32 K = 0; K <= 8; ++K)
        {
            const double T = StPi * K / 8.0;
            Ring.Add(FVector(Xs[I], Ws[I] * FMath::Cos(T), ZB + Hs[I] * FMath::Pow(FMath::Max(0.0, FMath::Sin(T)), .6)));
        }
        Ring.Add(FVector(Xs[I], -Ws[I] * .92, ZB - 1.4));
        Ring.Add(FVector(Xs[I], Ws[I] * .92, ZB - 1.4));
    }
    W.Loft(TEXT("ST_CobraHousing"), Rings, true, true);
    // Drop refractor underneath: the glowing sodium lens.
    const double Lx[] = { 214, 224, 238, 252, 266, 278, 288 };
    const double Lw[] = { 8, 12, 13.5, 14, 13.5, 12, 8 };
    const double Ld[] = { 3, 6.5, 8.5, 9, 8.5, 6.5, 3 };
    TArray<TArray<FVector>> LensRings;
    for (int32 I = 0; I < 7; ++I)
    {
        TArray<FVector>& Ring = LensRings.AddDefaulted_GetRef();
        for (int32 K = 0; K <= 8; ++K)
        {
            const double T = StPi + StPi * K / 8.0;
            Ring.Add(FVector(Lx[I], Lw[I] * FMath::Cos(T), ZB - .9 + Ld[I] * FMath::Sin(T)));
        }
    }
    W.Loft(TEXT("ST_SodiumLens"), LensRings, true, true);
}

/** Stroke-font text built as flat strips on a plane (lit sign letters). Origin is the left end of the baseline. */
void GlyphQuads(FStWriter& W, FName K, const FString& Text, const FVector& Origin, const FVector& Right, const FVector& Up, const FVector& N,
    double Height, double WidthScale, double Spacing, double StrokeW)
{
    const double U = Height / 6.0;
    double Cursor = 0;
    for (TCHAR Ch : Text)
    {
        for (const TArray<FVector2D>& Line : HallGlyph(Ch))
        {
            TArray<FVector> P;
            for (const FVector2D& G : Line) P.Add(Origin + Right * (Cursor + G.X * U * WidthScale) + Up * (G.Y * U));
            for (int32 I = 0; I + 1 < P.Num(); ++I)
            {
                FVector Side = FVector::CrossProduct(N, P[I + 1] - P[I]);
                if (!Side.Normalize()) continue;
                Side *= StrokeW * .5;
                W.Quad(K, P[I] - Side, P[I + 1] - Side, P[I + 1] + Side, P[I] + Side, N);
            }
            for (const FVector& V : P)
            {
                TArray<FVector> Ring;
                for (int32 S = 0; S < 8; ++S) { const double A = 2 * StPi * S / 8; Ring.Add(V + (Right * FMath::Cos(A) + Up * FMath::Sin(A)) * (StrokeW * .5)); }
                W.Fan(K, V, Ring, N);
            }
        }
        Cursor += (Ch == TEXT(' ') ? 2.6 : 4.0 * WidthScale) * U + Spacing * Height;
    }
}
} // namespace HouseStreetSceneImpl

// ======================================================================== public
namespace HouseStreetScene
{
using HouseStreetSceneImpl::FStWriter;
using HouseStreetSceneImpl::FStFrame;
using HouseStreetSceneImpl::FStRng;

void CreateLook(UHouseBlackHallSubsystem* L)
{
    if (!L || L->Materials.Contains(TEXT("ST_PoleWood"))) return;
    auto Plain = [&](const TCHAR* Key, const FLinearColor& C, float Rough, float Metal = 0.f, const FLinearColor& Em = FLinearColor::Black) { HallMakePlain(L, FName(Key), C, Rough, Metal, Em); };
    auto Tex = [&](const TCHAR* Key, const FHallImage& Img, const TArray<float>* Height, float Strength, float Rough, bool bWrap, const FLinearColor& Tint = FLinearColor::White)
    {
        UTexture2D* T = HallMakeTexture(Img, true, false, *FString::Printf(TEXT("T_%s"), Key), bWrap);
        UTexture2D* N = nullptr;
        if (Height)
        {
            FHallImage NImg; HallHeightToNormal(*Height, Img.W, Img.H, Strength, bWrap, NImg);
            N = HallMakeTexture(NImg, false, true, *FString::Printf(TEXT("T_%sN"), Key), bWrap);
            if (N) L->Textures.Add(FName(*FString::Printf(TEXT("T_%sN"), Key)), N);
        }
        if (T) L->Textures.Add(FName(*FString::Printf(TEXT("T_%s"), Key)), T);
        HallMakeTextured(L, FName(Key), T, N, Rough, Tint, N ? 1.f : 0.f);
    };
    FHallImage Img; TArray<float> Height;
    // The street paving itself: replaces the plain noise tile on every street, drive and lot that uses it.
    HouseStreetSceneImpl::PaintAsphalt(Img, Height);
    Tex(TEXT("EX_StreetAsphalt"), Img, &Height, 1.6f, .86f, true);
    HouseStreetSceneImpl::PaintGutter(Img); Tex(TEXT("ST_Gutter"), Img, nullptr, 0.f, .84f, true);
    HouseStreetSceneImpl::PaintPoleWood(Img, Height);
    Tex(TEXT("ST_PoleWood"), Img, &Height, 2.2f, .88f, true);
    Tex(TEXT("ST_Crossarm"), Img, &Height, 2.2f, .86f, true, FLinearColor(1.15f, 1.1f, 1.05f));
    HouseStreetSceneImpl::PaintManhole(Img, Height); Tex(TEXT("ST_Manhole"), Img, &Height, 3.f, .5f, false);
    {
        FHallImage Front, Back;
        HouseStreetSceneImpl::PaintBanner(Front, Back);
        Tex(TEXT("ST_Banner"), Front, nullptr, 0.f, .88f, false);
        Tex(TEXT("ST_BannerBack"), Back, nullptr, 0.f, .9f, false);
    }
    HouseStreetSceneImpl::PaintNewspaper(Img); Tex(TEXT("ST_NewsFront"), Img, nullptr, 0.f, .85f, false);
    HouseStreetSceneImpl::PaintBox(Img); Tex(TEXT("ST_NewsName"), Img, nullptr, 0.f, .45f, false);
    HouseStreetSceneImpl::PaintBag(Img); Tex(TEXT("ST_Bag"), Img, nullptr, 0.f, .9f, true);
    for (int32 K = 0; K < 3; ++K) { HouseStreetSceneImpl::PaintFlyer(Img, K); Tex(*FString::Printf(TEXT("ST_Flyer%d"), K), Img, nullptr, 0.f, .9f, false); }
    HouseStreetSceneImpl::PaintSign(Img, 128, 160, FLinearColor(.82f, .82f, .80f), FLinearColor(.02f, .02f, .02f),
        { { TEXT("SPEED"), 40.f, 20.f }, { TEXT("LIMIT"), 66.f, 20.f }, { TEXT("25"), 140.f, 56.f } }, 5);
    Tex(TEXT("ST_SignSpeed"), Img, nullptr, 0.f, .5f, false);
    HouseStreetSceneImpl::PaintSign(Img, 128, 192, FLinearColor(.82f, .82f, .80f), FLinearColor(.42f, .02f, .02f),
        { { TEXT("NO"), 46.f, 30.f }, { TEXT("PARKING"), 86.f, 22.f }, { TEXT("ANY"), 122.f, 20.f }, { TEXT("TIME"), 150.f, 20.f } }, 5);
    Img.FillPolygon({ FVector2D(30, 170), FVector2D(92, 170), FVector2D(92, 162), FVector2D(108, 175), FVector2D(92, 188), FVector2D(92, 180), FVector2D(30, 180) }, FLinearColor(.42f, .02f, .02f));
    Tex(TEXT("ST_SignNoPark"), Img, nullptr, 0.f, .5f, false);
    {
        FHallImage Card; Card.Init(128, 128, FLinearColor(.80f, .80f, .77f));
        Card.Rect(FVector2D(0, 0), FVector2D(128, 26), FLinearColor(.03f, .10f, .40f));
        FHallTextStyle S; S.Height = 12; S.Stroke = 2.2f; S.Spacing = .18f;
        HallDrawText(Card, TEXT("TELEPHONE"), FVector2D((128 - HallTextWidth(TEXT("TELEPHONE"), S)) * .5f, 19), S, FLinearColor(.9f, .9f, .9f));
        S.Height = 7; S.Stroke = 1.3f;
        const TCHAR* Lines[] = { TEXT("LOCAL CALL 25 CENTS"), TEXT("DEPOSIT COINS"), TEXT("THEN DIAL"), TEXT("OPERATOR DIAL 0"), TEXT("EMERGENCY 911") };
        for (int32 K = 0; K < 5; ++K)
        {
            S.WidthScale = FMath::Min(1.f, 118.f / HallTextWidth(Lines[K], S));
            HallDrawText(Card, Lines[K], FVector2D((128 - HallTextWidth(Lines[K], S)) * .5f, 44.f + K * 18), S, K == 4 ? FLinearColor(.5f, .03f, .02f) : FLinearColor(.05f, .05f, .06f));
        }
        Tex(TEXT("ST_PhoneCard"), Card, nullptr, 0.f, .6f, false);
    }
    // Street furniture and road pieces.
    Plain(TEXT("ST_InletVoid"), FLinearColor(.006f, .006f, .006f), .95f);
    Plain(TEXT("ST_Iron"), FLinearColor(.055f, .05f, .045f), .55f, .5f);
    Plain(TEXT("ST_IronRim"), FLinearColor(.10f, .075f, .055f), .6f, .4f);
    Plain(TEXT("ST_Hydrant"), FLinearColor(.40f, .028f, .018f), .45f, .1f);
    Plain(TEXT("ST_HydrantCap"), FLinearColor(.55f, .55f, .53f), .4f, .6f);
    Plain(TEXT("ST_Chain"), FLinearColor(.30f, .30f, .30f), .5f, .9f);
    Plain(TEXT("ST_SignPost"), FLinearColor(.035f, .10f, .05f), .5f, .5f);
    Plain(TEXT("ST_SignBack"), FLinearColor(.45f, .45f, .46f), .4f, .9f);
    Plain(TEXT("ST_LeafOrange"), FLinearColor(.42f, .12f, .02f), .85f);
    Plain(TEXT("ST_LeafBrown"), FLinearColor(.14f, .07f, .03f), .9f);
    Plain(TEXT("ST_LeafYellow"), FLinearColor(.55f, .38f, .05f), .85f);
    Plain(TEXT("ST_LeafRed"), FLinearColor(.32f, .04f, .02f), .85f);
    Plain(TEXT("ST_Paper"), FLinearColor(.66f, .64f, .58f), .9f);
    Plain(TEXT("ST_CanSilver"), FLinearColor(.55f, .55f, .56f), .35f, .9f);
    // Poles, lamps and wires.
    Plain(TEXT("ST_Insulator"), FLinearColor(.38f, .46f, .42f), .12f);
    Plain(TEXT("ST_Wire"), FLinearColor(.012f, .012f, .012f), .5f);
    Plain(TEXT("ST_Cable"), FLinearColor(.018f, .018f, .018f), .75f);
    Plain(TEXT("ST_PoleHardware"), FLinearColor(.22f, .22f, .23f), .5f, .8f);
    Plain(TEXT("ST_CobraHousing"), FLinearColor(.44f, .45f, .46f), .42f, .6f);
    // A bright, saturated low-pressure-orange refractor so the heads read as lit against the dusk sky.
    Plain(TEXT("ST_SodiumLens"), FLinearColor(1.f, .6f, .26f), .3f, 0.f, FLinearColor(64.f, 19.f, 2.4f));
    Plain(TEXT("ST_LotPole"), FLinearColor(.30f, .30f, .31f), .4f, .85f);
    Plain(TEXT("ST_PierConcrete"), FLinearColor(.36f, .35f, .33f), .85f);
    Plain(TEXT("ST_PoleTag"), FLinearColor(.62f, .62f, .64f), .35f, .9f);
    // Party.
    // Brushed stainless: rough enough that the dusk sky's dark blue does not swallow the silver.
    Plain(TEXT("ST_Keg"), FLinearColor(.72f, .73f, .74f), .36f, 1.f);
    Plain(TEXT("ST_KegDull"), FLinearColor(.48f, .48f, .49f), .45f, 1.f);
    Plain(TEXT("ST_KegValve"), FLinearColor(.72f, .72f, .74f), .2f, 1.f);
    Plain(TEXT("ST_PumpBlack"), FLinearColor(.02f, .02f, .02f), .42f);
    Plain(TEXT("ST_Hose"), FLinearColor(.07f, .08f, .09f), .25f);
    Plain(TEXT("ST_TubGrey"), FLinearColor(.15f, .17f, .19f), .55f);
    Plain(TEXT("ST_Ice"), FLinearColor(.58f, .68f, .76f), .08f);
    Plain(TEXT("ST_CupRed"), FLinearColor(.50f, .012f, .012f), .32f);
    Plain(TEXT("ST_CupWhite"), FLinearColor(.78f, .78f, .76f), .4f);
    Plain(TEXT("ST_ChairFrame"), FLinearColor(.62f, .63f, .65f), .3f, 1.f);
    Plain(TEXT("ST_WebGreen"), FLinearColor(.04f, .22f, .09f), .6f);
    Plain(TEXT("ST_WebWhite"), FLinearColor(.72f, .72f, .68f), .6f);
    Plain(TEXT("ST_WebOrange"), FLinearColor(.62f, .22f, .03f), .6f);
    Plain(TEXT("ST_WebBrown"), FLinearColor(.15f, .075f, .033f), .6f);
    Plain(TEXT("ST_WebYellow"), FLinearColor(.68f, .52f, .07f), .6f);
    Plain(TEXT("ST_WebBlue"), FLinearColor(.04f, .10f, .38f), .6f);
    Plain(TEXT("ST_ArmPad"), FLinearColor(.70f, .70f, .66f), .45f);
    Plain(TEXT("ST_GrillEnamel"), FLinearColor(.012f, .012f, .013f), .3f);
    Plain(TEXT("ST_GrillSteel"), FLinearColor(.35f, .35f, .36f), .45f, .9f);
    Plain(TEXT("ST_GrillWheel"), FLinearColor(.02f, .02f, .02f), .7f);
    Plain(TEXT("ST_CoalGlow"), FLinearColor(.9f, .3f, .06f), .8f, 0.f, FLinearColor(10.f, 2.4f, .35f));
    Plain(TEXT("ST_CoalHot"), FLinearColor(1.f, .45f, .1f), .8f, 0.f, FLinearColor(26.f, 7.5f, 1.3f));
    Plain(TEXT("ST_CoalAsh"), FLinearColor(.30f, .29f, .28f), .95f);
    Plain(TEXT("ST_CoalBlack"), FLinearColor(.025f, .025f, .025f), .9f);
    Plain(TEXT("ST_Burger"), FLinearColor(.09f, .045f, .025f), .7f);
    Plain(TEXT("ST_HotDog"), FLinearColor(.33f, .11f, .05f), .45f);
    Plain(TEXT("ST_FluidCan"), FLinearColor(.72f, .55f, .04f), .4f, .3f);
    Plain(TEXT("ST_FluidCap"), FLinearColor(.45f, .03f, .02f), .4f);
    Plain(TEXT("ST_CoolerWhite"), FLinearColor(.74f, .74f, .71f), .45f);
    Plain(TEXT("ST_CoolerRed"), FLinearColor(.45f, .03f, .025f), .4f);
    Plain(TEXT("ST_Rope"), FLinearColor(.42f, .36f, .25f), .9f);
    // Payphone and news box.
    Plain(TEXT("ST_PhonePost"), FLinearColor(.09f, .11f, .13f), .5f, .6f);
    Plain(TEXT("ST_PhoneSteel"), FLinearColor(.55f, .56f, .58f), .3f, 1.f);
    Plain(TEXT("ST_PhoneBlack"), FLinearColor(.015f, .015f, .016f), .4f);
    Plain(TEXT("ST_PhoneChrome"), FLinearColor(.75f, .75f, .77f), .15f, 1.f);
    Plain(TEXT("ST_PhoneTube"), FLinearColor(.9f, .95f, 1.f), .3f, 0.f, FLinearColor(7.f, 8.f, 9.f));
    Plain(TEXT("ST_PhoneSignBlue"), FLinearColor(.02f, .06f, .28f), .4f, 0.f, FLinearColor(.02f, .06f, .32f));
    Plain(TEXT("ST_PhoneSignText"), FLinearColor(.9f, .92f, .95f), .4f, 0.f, FLinearColor(3.2f, 3.4f, 3.8f));
    Plain(TEXT("ST_PhoneBook"), FLinearColor(.72f, .58f, .05f), .8f);
    Plain(TEXT("ST_NewsPaint"), FLinearColor(.03f, .14f, .06f), .45f, .2f);
    Plain(TEXT("ST_NewsCoin"), FLinearColor(.6f, .6f, .62f), .3f, 1.f);

    // Decals (alpha textures on the M_StreetDecal parent).
    auto Decal = [&](const TCHAR* Key, const FHallImage& I, float Rough, float Opacity, bool bWrap)
    {
        UTexture2D* T = MakeTexture(I, true, false, *FString::Printf(TEXT("T_%s"), Key), bWrap);
        if (T) L->Textures.Add(FName(*FString::Printf(TEXT("T_%s"), Key)), T);
        MakeDecalMaterial(L, FName(Key), T, Rough, Opacity, FLinearColor::White);
    };
    for (int32 V = 0; V < 3; ++V) { HouseStreetSceneImpl::PaintHalfRoad(Img, V); Decal(*FString::Printf(TEXT("STD_Half%d"), V), Img, .55f, 1.f, true); }
    HouseStreetSceneImpl::PaintLine(Img, true); Decal(TEXT("STD_Yellow"), Img, .6f, 1.f, true);
    HouseStreetSceneImpl::PaintLine(Img, false); Decal(TEXT("STD_White"), Img, .6f, 1.f, true);
    HouseStreetSceneImpl::PaintCrack(Img, 1201); Decal(TEXT("STD_Crack0"), Img, .28f, 1.f, false);
    HouseStreetSceneImpl::PaintCrack(Img, 1202); Decal(TEXT("STD_Crack1"), Img, .28f, 1.f, false);
    HouseStreetSceneImpl::PaintPatch(Img); Decal(TEXT("STD_Patch"), Img, .62f, 1.f, false);
    HouseStreetSceneImpl::PaintOil(Img); Decal(TEXT("STD_Oil"), Img, .3f, .9f, false);
    Decal(TEXT("STD_Wet"), Img, .08f, .75f, false);
    HouseStreetSceneImpl::PaintPuff(Img);
    if (UTexture2D* Puff = MakeTexture(Img, true, false, TEXT("T_ST_Puff"), false)) { L->Textures.Add(TEXT("T_ST_Puff"), Puff); MakeSmokeMaterial(L, TEXT("ST_Smoke"), Puff); }
}

bool DecalsReady(UHouseBlackHallSubsystem* Look)
{
    return Look && Look->Materials.Contains(TEXT("STD_Half0")) && Look->Materials.Contains(TEXT("STD_Yellow"));
}

FVector LensOffset() { return HouseStreetSceneImpl::Lens; }
FLinearColor SodiumColor() { return FLinearColor(1.f, .5f, .14f); }

bool IsStreetPole(float X, float Y)
{
    // Lamps(): the near kerb poles stand at y -3840, the far ones at y -5860, the side street's at x 8420.
    return FMath::Abs(Y + 3840.f) < 60.f || FMath::Abs(Y + 5860.f) < 60.f || FMath::Abs(X - 8420.f) < 60.f;
}

void StreetPoleMesh(FHallMesh& M, TArray<FBox>& Col)
{
    // A creosoted wooden utility pole: crossarm with three insulators, neutral and telephone cable below, and the
    // sodium cobra head on an upswept arm. Local +X points at the road; origin on the ground.
    FStWriter W(M);
    const double Z0 = -30, Z1 = 1070, R0 = 15, R1 = 11.5;
    auto PoleR = [&](double Z) { return R0 + (R1 - R0) * (Z - Z0) / (Z1 - Z0); };
    constexpr int32 Seg = 12, Rows = 11;
    for (int32 J = 0; J < Rows; ++J)
        for (int32 K = 0; K < Seg; ++K)
        {
            const double ZA = Z0 + (Z1 - Z0) * J / Rows, ZB = Z0 + (Z1 - Z0) * (J + 1) / Rows;
            const double A0 = 2 * HouseStreetSceneImpl::StPi * K / Seg, A1 = 2 * HouseStreetSceneImpl::StPi * (K + 1) / Seg;
            const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
            const FVector N0 = (D0 + FVector(0, 0, (R0 - R1) / (Z1 - Z0))).GetSafeNormal(), N1 = (D1 + FVector(0, 0, (R0 - R1) / (Z1 - Z0))).GetSafeNormal();
            const FVector P00 = D0 * PoleR(ZA) + FVector(0, 0, ZA), P10 = D1 * PoleR(ZA) + FVector(0, 0, ZA);
            const FVector P01 = D0 * PoleR(ZB) + FVector(0, 0, ZB), P11 = D1 * PoleR(ZB) + FVector(0, 0, ZB);
            const double U0 = 2.0 * K / Seg, U1 = 2.0 * (K + 1) / Seg;
            const FVector2D T0[3] = { FVector2D(U0, -ZA / 200.0), FVector2D(U1, -ZA / 200.0), FVector2D(U1, -ZB / 200.0) };
            const FVector2D T1[3] = { FVector2D(U0, -ZA / 200.0), FVector2D(U1, -ZB / 200.0), FVector2D(U0, -ZB / 200.0) };
            W.Emit(TEXT("ST_PoleWood"), P00, P10, P11, N0, N1, N1, T0);
            W.Emit(TEXT("ST_PoleWood"), P00, P11, P01, N0, N1, N0, T1);
        }
    W.Cone(TEXT("ST_PoleHardware"), FVector(0, 0, Z1), FVector(0, 0, Z1 + 2.5), R1 + .3, R1 - 1, 12, false, true);
    // Crossarm, braces and pin insulators.
    W.Box(TEXT("ST_Crossarm"), FVector(0, 22, 986), FVector(125, 5, 6));
    for (int32 S = -1; S <= 1; S += 2) W.Tube(TEXT("ST_PoleHardware"), { FVector(S * 70.0, 21, 980.5), FVector(0, 13, 933) }, .9, 6);
    W.Box(TEXT("ST_PoleHardware"), FVector(0, 13.2, 986), FVector(4, 1.2, 7));
    const TArray<FVector2D> Insulator = { FVector2D(0, 0), FVector2D(2.2, 0), FVector2D(2.2, 1.5), FVector2D(5.2, 3), FVector2D(5.0, 4.2), FVector2D(3.2, 4.6), FVector2D(4.6, 6.6),
        FVector2D(4.4, 7.6), FVector2D(2.9, 8), FVector2D(3.8, 10), FVector2D(3.4, 11), FVector2D(1.8, 12.2), FVector2D(0, 12.4) };
    for (const FVector& P : { FVector(-105, 22, 992), FVector(105, 22, 992), FVector(0, 0, Z1 + 2.5) })
    {
        W.Cone(TEXT("ST_PoleHardware"), P - FVector(0, 0, 1.5), P + FVector(0, 0, 1), 1.1, 1.1, 6, false, false);
        W.Lathe(TEXT("ST_Insulator"), P, FVector::UpVector, FVector::ForwardVector, Insulator, 10, false);
    }
    // Neutral on a spool, telephone cable clamp, pole tag.
    W.Box(TEXT("ST_PoleHardware"), FVector(-PoleR(905) - 1.5, 0, 905), FVector(2, 2.2, 6));
    W.Cone(TEXT("ST_Insulator"), FVector(-PoleR(905) - 5, -4, 905), FVector(-PoleR(905) - 5, 4, 905), 3.8, 3.8, 10, true, true);
    W.Box(TEXT("ST_PoleHardware"), FVector(-PoleR(760) - 1.5, 0, 760), FVector(2.2, 2.6, 2.4));
    W.Box(TEXT("ST_PoleTag"), FVector(PoleR(175) + .25, 0, 175), FVector(.3, 4, 5));
    HouseStreetSceneImpl::CobraHead(W, PoleR(695));
    Col.Add(FBox(FVector(-16, -16, 0), FVector(16, 16, Z1)));
}

void LotPoleMesh(FHallMesh& M, TArray<FBox>& Col)
{
    // Galvanised steel pole on a concrete pier with the same cobra head (lots, campus drives).
    FStWriter W(M);
    W.Cone(TEXT("ST_PierConcrete"), FVector(0, 0, -8), FVector(0, 0, 69.5), 23.5, 23.5, 16, false, true);
    W.Box(TEXT("ST_LotPole"), FVector(0, 0, 70.4), FVector(14.5, 14.5, .8));
    W.Cone(TEXT("ST_LotPole"), FVector(0, 0, 70), FVector(0, 0, 760), 10, 7, 12, false, true);
    HouseStreetSceneImpl::CobraHead(W, 10.0 - 3.0 * (695.0 - 70.0) / 690.0);
    Col.Add(FBox(FVector(-24, -24, -8), FVector(24, 24, 70)));
    Col.Add(FBox(FVector(-10, -10, 70), FVector(10, 10, 760)));
}

void BuildWires(FHallMesh& M, const TArray<FTransform>& Poles)
{
    // Three phase wires on the insulators, the neutral and a fat telephone cable, sagging between neighbouring poles.
    FStWriter W(M);
    TArray<FTransform> Runs[3];
    for (const FTransform& T : Poles)
    {
        const FVector L = T.GetLocation();
        const int32 Run = FMath::Abs(L.Y + 3840.f) < 60.f ? 0 : FMath::Abs(L.Y + 5860.f) < 60.f ? 1 : FMath::Abs(L.X - 8420.f) < 60.f ? 2 : -1;
        if (Run >= 0) Runs[Run].Add(T);
    }
    struct FLine { FVector At; double Sag, Radius; const TCHAR* Key; };
    const FLine Lines[] = {
        { FVector(-105, 22, 1004.4), 44, .9, TEXT("ST_Wire") }, { FVector(105, 22, 1004.4), 44, .9, TEXT("ST_Wire") }, { FVector(0, 0, 1084.9), 40, .9, TEXT("ST_Wire") },
        { FVector(-21, 0, 905), 52, .8, TEXT("ST_Wire") }, { FVector(-19, 0, 757), 70, 2.0, TEXT("ST_Cable") } };
    int32 Spans = 0;
    for (int32 Run = 0; Run < 3; ++Run)
    {
        TArray<FTransform>& P = Runs[Run];
        P.Sort([Run](const FTransform& A, const FTransform& B) { return Run < 2 ? A.GetLocation().X < B.GetLocation().X : A.GetLocation().Y < B.GetLocation().Y; });
        for (int32 I = 0; I + 1 < P.Num(); ++I)
        {
            if (FVector::Dist(P[I].GetLocation(), P[I + 1].GetLocation()) > 3300.f) continue;
            ++Spans;
            for (const FLine& L : Lines)
            {
                const FVector A = P[I].TransformPosition(L.At), B = P[I + 1].TransformPosition(L.At);
                TArray<FVector> Pts;
                for (int32 S = 0; S <= 18; ++S)
                {
                    const double T = S / 18.0;
                    Pts.Add(A + (B - A) * T - FVector(0, 0, L.Sag * 4 * T * (1 - T)));
                }
                W.Tube(L.Key, Pts, L.Radius, 6);
            }
        }
    }
    UE_LOG(LogTemp, Display, TEXT("HOUSE_STREET_WIRES spans=%d lines=%d"), Spans, Spans * (int32)UE_ARRAY_COUNT(Lines));
}

void BuildRoad(FHallMesh& Flat, FHallMesh& Clean, FHallMesh& Solid)
{
    FStWriter F(Flat), Cl(Clean), S(Solid);
    FStRng R(31337);
    const double Pi = HouseStreetSceneImpl::StPi;
    constexpr float NearKerb = HouseStreetSceneImpl::NearKerb, FarKerb = HouseStreetSceneImpl::FarKerb, Road = HouseStreetSceneImpl::Road, Walk = HouseStreetSceneImpl::Walk;
    constexpr float WearZ = HouseStreetSceneImpl::WearZ, GutterW = HouseStreetSceneImpl::GutterW;
    // Concrete gutter pans along both kerbs; 300 cm pieces whose joints line up with the kerb joints (x = 100 + 150k).
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const double Face = Side ? FarKerb : NearKerb, Out = Side ? 1.0 : -1.0;
        for (double X0 = -20000; X0 < 8450; X0 += 300)
        {
            const double X1 = FMath::Min(X0 + 300.0, 8450.0);
            const FVector A(X0, Face, WearZ), B(X1, Face, WearZ), C(X1, Face + Out * GutterW, WearZ), D(X0, Face + Out * GutterW, WearZ);
            const FVector2D UV[4] = { FVector2D(0, 1), FVector2D((X1 - X0) / 300.0, 1), FVector2D((X1 - X0) / 300.0, 0), FVector2D(0, 0) };
            F.QuadUV(TEXT("ST_Gutter"), A, B, C, D, FVector::UpVector, UV);
        }
    }
    // Kerb inlets: an opening in the kerb face with a steel lintel; on the near side a grate in the gutter too.
    auto Inlet = [&](double X, bool bFar, bool bGrate)
    {
        const double Face = bFar ? FarKerb : NearKerb, Out = bFar ? 1.0 : -1.0;
        const FVector N(0, Out, 0);
        Cl.Quad(TEXT("ST_InletVoid"), FVector(X - 46, Face + Out * .3, Road + 1.2), FVector(X + 46, Face + Out * .3, Road + 1.2), FVector(X + 46, Face + Out * .3, Walk - 3.4), FVector(X - 46, Face + Out * .3, Walk - 3.4), N);
        Cl.Box(TEXT("ST_Iron"), FVector(X, Face + Out * .7, Walk - 1.9), FVector(50, .7, 1.6));
        if (!bGrate) return;
        Cl.Box(TEXT("ST_Iron"), FVector(X, Face + Out * 25, -71.05), FVector(43, 25, .32));
        for (double XX = X - 36; XX <= X + 36.1; XX += 9) Cl.Box(TEXT("ST_InletVoid"), FVector(XX, Face + Out * 25, -70.7), FVector(2.2, 21, .05));
    };
    for (double X : { -2250.0, 1850.0, 6200.0 }) Inlet(X, false, true);
    for (double X : { -8100.0, -4200.0, 1200.0, 7200.0 }) Inlet(X, true, false);
    // Manholes with cast lids, small valve covers and a utility vault lid.
    auto Manhole = [&](double X, double Y)
    {
        Cl.Cone(TEXT("ST_IronRim"), FVector(X, Y, -71.6), FVector(X, Y, -71.0), 38, 37, 28, false, true);
        Cl.Cone(TEXT("ST_Iron"), FVector(X, Y, -71.0), FVector(X, Y, -70.8), 33.5, 33.2, 28, false, false);
        FHallSection& Sec = Clean.Sec(TEXT("ST_Manhole"));
        const int32 Base = Sec.V.Num();
        Sec.V.Add(FVector(X, Y, -70.8)); Sec.N.Add(FVector::UpVector); Sec.UV.Add(FVector2D(.5, .5)); Sec.T.Add(FProcMeshTangent(FVector::ForwardVector, false));
        for (int32 K = 0; K < 28; ++K)
        {
            const double A = 2 * Pi * K / 28;
            Sec.V.Add(FVector(X + FMath::Cos(A) * 33.2, Y + FMath::Sin(A) * 33.2, -70.8)); Sec.N.Add(FVector::UpVector);
            Sec.UV.Add(FVector2D(.5 + .49 * FMath::Cos(A), .5 - .49 * FMath::Sin(A))); Sec.T.Add(FProcMeshTangent(FVector::ForwardVector, false));
        }
        for (int32 K = 0; K < 28; ++K) Sec.I.Append({ Base, Base + 1 + (K + 1) % 28, Base + 1 + K });
    };
    for (const FVector2D& P : { FVector2D(-1050, -5160), FVector2D(4900, -5100), FVector2D(-6100, -5060) }) Manhole(P.X, P.Y);
    for (const FVector2D& P : { FVector2D(560, -4440), FVector2D(2620, -4890), FVector2D(-2600, -4700), FVector2D(-4800, -5300), FVector2D(3900, -5420) })
    {
        Cl.Cone(TEXT("ST_IronRim"), FVector(P.X, P.Y, -71.6), FVector(P.X, P.Y, -71.1), 13.5, 13, 16, false, true);
        Cl.Cone(TEXT("ST_Iron"), FVector(P.X, P.Y, -71.1), FVector(P.X, P.Y, -70.95), 10.5, 10.3, 16, false, true);
    }
    Cl.Box(TEXT("ST_IronRim"), FVector(5300, -4262, -71.3), FVector(52, 32, .3));
    Cl.Box(TEXT("ST_Iron"), FVector(5300, -4262, -70.95), FVector(48, 28, .1));
    for (int32 K = -3; K <= 3; ++K) Cl.Box(TEXT("ST_IronRim"), FVector(5300 + K * 12.0, -4262, -70.83), FVector(.8, 26, .05));
    // Fallen leaves: drifts along both gutters, around the inlets, scattered over the parking lanes and the sidewalks.
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const double Face = Side ? FarKerb : NearKerb, Out = Side ? 1.0 : -1.0;
        HouseStreetSceneImpl::LeafEdge(Cl, R, FVector2D(-9000, Face + Out * 1.5), FVector2D(8400, Face + Out * 1.5), FVector2D(0, Out), 42, 26, WearZ + .12);
        for (int32 K = 0; K < 650; ++K)
            HouseStreetSceneImpl::Leaf(Cl, R, R.Range(-9000, 8400), Face + Out * R.Range(45, 300), Road + .45 + R.Range(0, .2));
        const double WalkIn = Side ? -5800.0 : -3900.0, WalkOut = Side ? -5550.0 : -4150.0;
        for (int32 K = 0; K < 220; ++K) HouseStreetSceneImpl::Leaf(Cl, R, R.Range(-9000, 8400), R.Range(FMath::Min(WalkIn, WalkOut) + 8, FMath::Max(WalkIn, WalkOut) - 8), Walk + .15);
        HouseStreetSceneImpl::LeafEdge(Cl, R, FVector2D(-9000, WalkIn + Out * 2), FVector2D(8400, WalkIn + Out * 2), FVector2D(0, Out), 30, 7, Walk + .12);
    }
    for (double X : { -2250.0, 1850.0, 6200.0 }) for (int32 K = 0; K < 45; ++K) HouseStreetSceneImpl::Leaf(Cl, R, X + R.Range(-70, 70), NearKerb - R.Range(2, 40) * R.Frac(), WearZ + .35 + R.Range(0, .4));
    // Crushed cans and a newspaper page caught against the kerb.
    for (const FVector& P : { FVector(640, -4520, Road + 1.3), FVector(-3720, -5470, Road + 1.3), FVector(2860, -4190, WearZ + 1.3) })
        Cl.Box(TEXT("ST_CanSilver"), P, FVector(5.5, 3.2, 1.2), R.Range(0, 180));
    Cl.Box(TEXT("ST_Paper"), FVector(-2930, NearKerb - 22, WearZ + .5), FVector(24, 16, .15), 12);

    // ---- Street furniture (collision).
    // A fire hydrant by the side drive, pumper nozzle to the street.
    {
        const FStFrame H = FStFrame::Yawed(FVector(3185, -4092, Walk), -90);
        S.Cone(TEXT("ST_Hydrant"), H.P(0, 0, 0), H.P(0, 0, 4), 15.5, 15.5, 16, false, true);
        S.Cone(TEXT("ST_Hydrant"), H.P(0, 0, 4), H.P(0, 0, 56), 11.5, 10.8, 16, false, false);
        S.Cone(TEXT("ST_Hydrant"), H.P(0, 0, 56), H.P(0, 0, 59), 13.5, 13.5, 16, true, true);
        S.Lathe(TEXT("ST_HydrantCap"), H.P(0, 0, 59), FVector::UpVector, H.X, { FVector2D(13, 0), FVector2D(12.5, 3), FVector2D(10, 7.5), FVector2D(5.5, 10.5), FVector2D(0, 11.5) }, 16, true);
        S.Cone(TEXT("ST_HydrantCap"), H.P(0, 0, 70), H.P(0, 0, 74.5), 3.2, 2.6, 5, false, true);
        S.Cone(TEXT("ST_Hydrant"), H.P(9, 0, 36), H.P(20, 0, 36), 7.2, 7.2, 14, false, false);
        S.Cone(TEXT("ST_HydrantCap"), H.P(20, 0, 36), H.P(24.5, 0, 36), 7.8, 7.8, 5, true, true);
        for (int32 Sd = -1; Sd <= 1; Sd += 2)
        {
            S.Cone(TEXT("ST_Hydrant"), H.P(0, Sd * 9.5, 42), H.P(0, Sd * 18, 42), 4.6, 4.6, 12, false, false);
            S.Cone(TEXT("ST_HydrantCap"), H.P(0, Sd * 18, 42), H.P(0, Sd * 21.5, 42), 5.2, 5.2, 5, true, true);
            Cl.Tube(TEXT("ST_Chain"), { H.P(1, Sd * 20, 37.5), H.P(6, Sd * 15, 31), H.P(9, Sd * 10.5, 34) }, .35, 4);
        }
    }
    // Two signs facing oncoming traffic on the near kerb.
    auto SignPost = [&](double X, double Y, const TCHAR* Face, double W2, double H2, double Top)
    {
        S.Box(TEXT("ST_SignPost"), FVector(X + 1.5, Y, Walk + Top * .5 - 30), FVector(1.2, 2.6, Top * .5 + 30));
        const double Z1 = Walk + Top, Z0 = Z1 - H2;
        const FVector2D UV[4] = { FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0) };
        S.QuadUV(Face, FVector(X - .3, Y - W2 * .5, Z0), FVector(X - .3, Y + W2 * .5, Z0), FVector(X - .3, Y + W2 * .5, Z1), FVector(X - .3, Y - W2 * .5, Z1), FVector(-1, 0, 0), UV);
        S.Quad(TEXT("ST_SignBack"), FVector(X + .1, Y - W2 * .5, Z0), FVector(X + .1, Y + W2 * .5, Z0), FVector(X + .1, Y + W2 * .5, Z1), FVector(X + .1, Y - W2 * .5, Z1), FVector(1, 0, 0));
    };
    SignPost(-2620, -3962, TEXT("ST_SignSpeed"), 60, 75, 245);
    SignPost(3345, -3962, TEXT("ST_SignNoPark"), 30, 45, 215);
    // Flyers stapled to the two poles in front of the house (the party flyer among them).
    auto Flyer = [&](double PX, double PY, double Ang, double Z, int32 Kind)
    {
        const double Rp = 14.6, Arc = 21.0 / Rp;
        FHallSection& Sec = Clean.Sec(FName(*FString::Printf(TEXT("ST_Flyer%d"), Kind)));
        const int32 Base = Sec.V.Num();
        for (int32 K = 0; K <= 4; ++K)
        {
            const double A = Ang - Arc * .5 + Arc * K / 4.0;
            const FVector D(FMath::Cos(A), FMath::Sin(A), 0);
            for (int32 Row = 0; Row < 2; ++Row)
            {
                Sec.V.Add(FVector(PX, PY, 0) + D * Rp + FVector(0, 0, Row ? Z + 14 : Z - 14));
                Sec.N.Add(D); Sec.UV.Add(FVector2D(1.0 - K / 4.0, Row ? 0.0 : 1.0)); Sec.T.Add(FProcMeshTangent(FVector(D.Y, -D.X, 0), false));
            }
        }
        for (int32 K = 0; K < 4; ++K)
        {
            const int32 A0 = Base + K * 2, A1 = A0 + 2;
            Sec.I.Append({ A0, A0 + 1, A1 + 1, A0, A1 + 1, A1 });
        }
    };
    // Pole at (-1500, -3840): the street is toward -y (angle -90 degrees).
    Flyer(-1500, -3840, -Pi * .5 - .15, -60.0 + 166, 0);
    Flyer(-1500, -3840, -Pi * .5 + .55, -60.0 + 132, 1);
    Flyer(1500, -3840, -Pi * .5 + .2, -60.0 + 158, 0);
    Flyer(1500, -3840, -Pi * .5 - .6, -60.0 + 196, 2);
}

void BuildParty(FHallMesh& Props, FHallMesh& Litter)
{
    using HouseStreetSceneImpl::AppendTransformed;
    const double Pi = HouseStreetSceneImpl::StPi;
    const float Lawn = HouseStreetSceneImpl::Lawn;
    FHallMesh Keg, Tapped, Tub, Grill, Lid, Cooler, Bag, Cup, Crushed;
    HouseStreetSceneImpl::BuildKeg(Keg, false);
    HouseStreetSceneImpl::BuildKeg(Tapped, true);
    HouseStreetSceneImpl::BuildTub(Tub);
    HouseStreetSceneImpl::BuildGrill(Grill);
    HouseStreetSceneImpl::BuildLid(Lid);
    HouseStreetSceneImpl::BuildCooler(Cooler);
    HouseStreetSceneImpl::BuildBag(Bag);
    HouseStreetSceneImpl::BuildCup(Cup);
    AppendTransformed(Crushed, Cup, FStFrame(), FVector(1, .42, 1));

    // The keg station right of the walk: one tapped in a tub of ice (tap toward the walk), a full spare, an empty rolled over.
    AppendTransformed(Props, Tub, FStFrame::Yawed(FVector(1080, -2780, Lawn), 20));
    AppendTransformed(Props, Tapped, FStFrame::Yawed(FVector(1080, -2780, Lawn + 4), 180));
    const FStFrame Spare = FStFrame::Yawed(FVector(1218, -2702, Lawn), 40);
    AppendTransformed(Props, Keg, Spare);
    {
        const double Yaw = 25 * Pi / 180;
        const FVector Axis(FMath::Cos(Yaw), FMath::Sin(Yaw), 0);
        FStFrame F; F.Z = Axis; F.X = FVector::UpVector; F.Y = FVector::CrossProduct(F.Z, F.X);
        F.O = FVector(1398, -2795, Lawn + 21.35) - Axis * 29.6;
        AppendTransformed(Props, Keg, F);
    }
    // The grill nearer the street so its smoke drifts across the yard; lid leaning on the bowl, coals bag and fluid beside it.
    const FStFrame GrillF = FStFrame::Yawed(FVector(1520, -3140, Lawn), 30);
    AppendTransformed(Props, Grill, GrillF);
    {
        FStFrame F;
        F.Z = GrillF.D(FVector(-.96, 0, .26)).GetSafeNormal(); F.X = GrillF.Y; F.Y = FVector::CrossProduct(F.Z, F.X);
        F.O = GrillF.P(-35, 0, 28);
        AppendTransformed(Props, Lid, F);
    }
    AppendTransformed(Props, Bag, FStFrame::Yawed(FVector(1620, -3082, Lawn), 18));
    {
        FStWriter W(Props);
        const FVector C(1588, -3182, Lawn);
        W.Cone(TEXT("ST_FluidCan"), C, C + FVector(0, 0, 21), 4.2, 4.2, 14, true, true);
        W.Cone(TEXT("ST_FluidCap"), C + FVector(1.5, 0, 21), C + FVector(1.5, 0, 24), 1.6, 1.4, 10, true, true);
    }
    // Webbed lawn chairs pulled round the grill, one knocked over backwards; two more and the cooler left of the walk.
    struct FChairAt { double X, Y, Yaw; const TCHAR* Across; const TCHAR* Along; bool bTipped; };
    const FChairAt Chairs[] = {
        { 1255, -3175, 8, TEXT("ST_WebGreen"), TEXT("ST_WebWhite"), false },
        { 1300, -2975, -35, TEXT("ST_WebOrange"), TEXT("ST_WebBrown"), false },
        { 1050, -3030, -13, TEXT("ST_WebBlue"), TEXT("ST_WebWhite"), false },
        { 1170, -3330, 150, TEXT("ST_WebYellow"), TEXT("ST_WebWhite"), true },
        { -880, -2770, -40, TEXT("ST_WebGreen"), TEXT("ST_WebYellow"), false },
        { -690, -2895, 140, TEXT("ST_WebOrange"), TEXT("ST_WebWhite"), false } };
    TArray<FStFrame> ChairFrames;
    for (const FChairAt& C : Chairs)
    {
        FHallMesh Chair; HouseStreetSceneImpl::BuildChair(Chair, C.Across, C.Along);
        FStFrame F = FStFrame::Yawed(FVector(C.X, C.Y, Lawn), C.Yaw);
        if (C.bTipped) F = F.Turned(F.P(-24, 0, 0), F.Y, -84.7 * Pi / 180);
        AppendTransformed(Props, Chair, F);
        ChairFrames.Add(F);
    }
    const FStFrame CoolerF = FStFrame::Yawed(FVector(-760, -2690, Lawn), 15);
    AppendTransformed(Props, Cooler, CoolerF);

    // Red cups: standing, on their sides and stepped on, everywhere the party went.
    FStRng R(6061);
    struct FObst { double X, Y, Rad; };
    const FObst Obst[] = { { 1080, -2780, 38 }, { 1218, -2702, 26 }, { 1398, -2795, 36 }, { 1520, -3140, 46 }, { 1620, -3082, 24 }, { 1588, -3182, 10 },
        { 1255, -3175, 44 }, { 1300, -2975, 44 }, { 1050, -3030, 44 }, { 1225, -3360, 60 }, { -880, -2770, 44 }, { -690, -2895, 44 }, { -760, -2690, 40 },
        { -1450, -3500, 30 }, { 1700, -3520, 30 }, { -2350, -3250, 48 }, { 2600, -3350, 52 }, { 2200, -2650, 42 } };
    auto Free = [&](double X, double Y)
    {
        if (X > -445 && X < 372) return false;   // the front walk
        for (const FObst& O : Obst) if (FMath::Square(X - O.X) + FMath::Square(Y - O.Y) < FMath::Square(O.Rad + 6)) return false;
        return true;
    };
    int32 Cups = 0;
    auto Upright = [&](const FVector& P, double YawDeg) { AppendTransformed(Litter, Cup, FStFrame::Yawed(P, YawDeg)); ++Cups; };
    auto Lying = [&](double X, double Y, double Z, bool bCrushed)
    {
        const double Yaw = R.Range(0, 2 * Pi), Tilt = 8.06 * Pi / 180;
        const FVector D(FMath::Cos(Yaw), FMath::Sin(Yaw), 0);
        FStFrame F;
        F.Z = (D * FMath::Cos(Tilt) + FVector(0, 0, FMath::Sin(Tilt))).GetSafeNormal();
        F.X = FVector::CrossProduct(F.Z, FVector::UpVector).GetSafeNormal();
        F.Y = FVector::CrossProduct(F.Z, F.X);
        F.O = FVector(X, Y, Z + (bCrushed ? 1.3 : 2.95));
        AppendTransformed(Litter, bCrushed ? Crushed : Cup, F);
        ++Cups;
    };
    auto Scatter = [&](double X0, double X1, double Y0, double Y1, int32 Count, double Z)
    {
        for (int32 Tries = 0, N = 0; N < Count && Tries < Count * 30; ++Tries)
        {
            const double X = R.Range(X0, X1), Y = R.Range(Y0, Y1);
            if (!Free(X, Y)) continue;
            const double Pick = R.Frac();
            if (Pick < .24) Upright(FVector(X, Y, Z), R.Range(0, 360));
            else Lying(X, Y, Z, Pick > .8);
            ++N;
        }
    };
    Scatter(940, 1720, -3420, -2630, 30, Lawn);
    Scatter(-1150, -470, -3030, -2570, 10, Lawn);
    Scatter(-1700, 2250, -3880, -3570, 8, Lawn);
    Scatter(-700, 2100, -4135, -3915, 6, HouseStreetSceneImpl::Walk);
    for (int32 K = 0; K < 4; ++K) Lying(R.Range(-900, 1750), HouseStreetSceneImpl::NearKerb - R.Range(8, 32), HouseStreetSceneImpl::WearZ, R.Chance(.4));
    for (int32 K = 0; K < 2; ++K) Lying(R.Range(-380, 300), R.Range(-3885, -3800), HouseStreetSceneImpl::Walk, false);
    // On the chair arms, the spare keg's top and the cooler lid, with a fresh sleeve of cups.
    Upright(ChairFrames[0].P(6, 29.6, 57.7), 0);
    Upright(ChairFrames[4].P(-4, -29.6, 57.7), 0);
    Upright(Spare.P(9, 3, 55.8), 0);
    Upright(Spare.P(-7, -8, 55.9), 0);
    Upright(CoolerF.P(-15, 6, 37.9), 0);
    Upright(CoolerF.P(12, -5, 37.9), 0);
    for (int32 K = 0; K < 10; ++K) Upright(CoolerF.P(2, 9, 37.9 + K * 1.25), 0);
    UE_LOG(LogTemp, Display, TEXT("HOUSE_STREET_PARTY kegs=3 chairs=%d grill=1 cups=%d banner=FALL HARVEST JAM feat PROJECT JAYSTORM"), (int32)UE_ARRAY_COUNT(Chairs), Cups);
}

void BuildBanner(FHallMesh& M)
{
    // A painted bedsheet hung across the portico's entablature, above Codex's column flags: tied to the dentil course every
    // 137 cm (it sags a little between ties), bottom corners roped round the outer columns.
    FStWriter W(M);
    const double Pi = HouseStreetSceneImpl::StPi;
    constexpr int32 NX = 64, NZ = 8;
    const double X0 = -567, X1 = 533, ZTop = 690, ZBot = 586, BaseY = -1505;
    auto Pos = [&](double U, double V)
    {
        const double X = X0 + (X1 - X0) * U;
        const double Tie = U * 8.0 - FMath::FloorToDouble(U * 8.0);
        const double Sag = 2.2 * FMath::Square(FMath::Sin(Tie * Pi)) * (1.0 - V * .6);
        const double Z = ZTop - Sag - (ZTop - ZBot) * V + V * V * 2.5 * FMath::Sin(X * .029 + 1.1);
        const double Y = BaseY + 1.6 * FMath::Sin(X * .041 + V * 1.7) * (.35 + .65 * V) + .7 * FMath::Sin(X * .13 + 2.3) * V;
        return FVector(X, Y, Z);
    };
    auto Nrm = [&](double U, double V)
    {
        const FVector DU = Pos(FMath::Min(1.0, U + .004), V) - Pos(FMath::Max(0.0, U - .004), V);
        const FVector DV = Pos(U, FMath::Min(1.0, V + .02)) - Pos(U, FMath::Max(0.0, V - .02));
        return (FVector::CrossProduct(DU, DV) * -1.0).GetSafeNormal();
    };
    for (int32 J = 0; J < NZ; ++J)
        for (int32 I = 0; I < NX; ++I)
        {
            const double U0 = (double)I / NX, U1 = (double)(I + 1) / NX, V0 = (double)J / NZ, V1 = (double)(J + 1) / NZ;
            const FVector P00 = Pos(U0, V0), P10 = Pos(U1, V0), P11 = Pos(U1, V1), P01 = Pos(U0, V1);
            const FVector N00 = Nrm(U0, V0), N10 = Nrm(U1, V0), N11 = Nrm(U1, V1), N01 = Nrm(U0, V1);
            // Seen from the street (looking +y) world +x is on the viewer's left, so the painting runs from X1 to X0.
            const FVector2D T0[3] = { FVector2D(1 - U0, V0), FVector2D(1 - U1, V0), FVector2D(1 - U1, V1) }, T1[3] = { FVector2D(1 - U0, V0), FVector2D(1 - U1, V1), FVector2D(1 - U0, V1) };
            W.Emit(TEXT("ST_Banner"), P00, P10, P11, N00, N10, N11, T0);
            W.Emit(TEXT("ST_Banner"), P00, P11, P01, N00, N11, N01, T1);
            const FVector B(0, .3, 0);
            W.Emit(TEXT("ST_BannerBack"), P00 + B, P10 + B, P11 + B, N00 * -1.0, N10 * -1.0, N11 * -1.0, T0);
            W.Emit(TEXT("ST_BannerBack"), P00 + B, P11 + B, P01 + B, N00 * -1.0, N11 * -1.0, N01 * -1.0, T1);
        }
    for (int32 K = 0; K <= 8; ++K)
    {
        const FVector P = Pos(K / 8.0, 0);
        W.Tube(TEXT("ST_Rope"), { P + FVector(0, -.4, -1.2), P + FVector(0, -.9, 3.2), FVector(P.X, -1499, ZTop + 4.6), FVector(P.X, -1489, ZTop + 3.6) }, .55, 6);
    }
    // Bottom corners roped to the outer column shafts (columns at x -668 and 635, y -1414, shaft radius 55 here).
    for (int32 Sd = 0; Sd < 2; ++Sd)
    {
        const FVector Corner = Pos(Sd ? 1.0 : 0.0, 1.0);
        const double CX = Sd ? 634.73 : -668.11;
        const FVector Knot(CX + (Sd ? -55.6 : 55.6) * .9, -1414 - 55.6 * .44, 571);
        W.Tube(TEXT("ST_Rope"), { Corner + FVector(0, -.3, .8), (Corner + Knot) * .5 + FVector(0, -2, -4), Knot }, .6, 6);
        TArray<FVector> Ring;
        for (int32 S = 0; S <= 24; ++S) { const double A = 2 * Pi * S / 24; Ring.Add(FVector(CX + FMath::Cos(A) * 56.1, -1414 + FMath::Sin(A) * 56.1, 571 + .8 * FMath::Sin(A * 2))); }
        W.Tube(TEXT("ST_Rope"), Ring, .6, 6);
    }
}

void BuildCorner(FHallMesh& M)
{
    // The corner by the side drive: a coin newspaper box and a lit payphone, both facing the sidewalk.
    FStWriter W(M);
    const float Walk = HouseStreetSceneImpl::Walk;
    {
        const FStFrame F = FStFrame::Yawed(FVector(4115, -3938, Walk), -90);
        auto Bx = [&](const TCHAR* K, double X, double Y, double Z, double HX, double HY, double HZ) { W.BoxAx(K, F.P(X, Y, Z), F.X, F.Y, F.Z, FVector(HX, HY, HZ)); };
        for (int32 SX = -1; SX <= 1; SX += 2)
        {
            for (int32 SY = -1; SY <= 1; SY += 2) Bx(TEXT("ST_NewsPaint"), SX * 16, SY * 20, 16.5, 1.2, 1.2, 16.5);
            Bx(TEXT("ST_NewsPaint"), SX * 16, 0, 1.2, 1.2, 21, 1.2);
        }
        Bx(TEXT("ST_NewsPaint"), 0, 0, 66, 19, 24, 34);
        Bx(TEXT("ST_NewsPaint"), 0, 0, 101.4, 20.5, 25, 1.4);
        // Someone facing the box looks along -F.X, so the text runs along -F.Y.
        const FVector2D UV[4] = { FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0) };
        W.QuadUV(TEXT("ST_NewsName"), F.P(19.05, 22, 88.5), F.P(19.05, -22, 88.5), F.P(19.05, -22, 99), F.P(19.05, 22, 99), F.X, UV);
        W.QuadUV(TEXT("ST_NewsFront"), F.P(19.1, 16.5, 48), F.P(19.1, -16.5, 48), F.P(19.1, -16.5, 84), F.P(19.1, 16.5, 84), F.X, UV);
        Bx(TEXT("ST_NewsCoin"), 19.5, 0, 85.2, .5, 18.2, .8);
        Bx(TEXT("ST_NewsCoin"), 19.5, 0, 46.8, .5, 18.2, .8);
        for (int32 SY = -1; SY <= 1; SY += 2) Bx(TEXT("ST_NewsCoin"), 19.5, SY * 17.6, 66, .5, .8, 18.4);
        Bx(TEXT("ST_NewsCoin"), 20.6, 0, 42, .7, 9, .7);
        for (int32 SY = -1; SY <= 1; SY += 2) Bx(TEXT("ST_NewsCoin"), 19.8, SY * 8.5, 42, 1, .6, .6);
        Bx(TEXT("ST_NewsCoin"), 8, 25.5, 90, 5, 1.5, 6.5);
        Bx(TEXT("ST_PhoneBlack"), 8, 27.05, 93, 2.2, .1, .4);
    }
    {
        const FStFrame F = FStFrame::Yawed(FVector(4268, -3928, Walk), -90);
        auto Bx = [&](const TCHAR* K, double X, double Y, double Z, double HX, double HY, double HZ) { W.BoxAx(K, F.P(X, Y, Z), F.X, F.Y, F.Z, FVector(HX, HY, HZ)); };
        Bx(TEXT("ST_PhonePost"), 0, 0, .8, 13, 13, .8);
        Bx(TEXT("ST_PhonePost"), 0, 0, 52.8, 5, 5, 52);
        Bx(TEXT("ST_PhoneSteel"), -16.5, 0, 150, 1.5, 33, 48);
        for (int32 SY = -1; SY <= 1; SY += 2) Bx(TEXT("ST_PhoneSteel"), 0, SY * 34.5, 150, 18, 1.5, 48);
        Bx(TEXT("ST_PhoneSteel"), 2, 0, 199.5, 21, 37, 1.6);
        Bx(TEXT("ST_PhoneSignBlue"), 21, 0, 210.6, 2, 34, 9.5);
        HouseStreetSceneImpl::GlyphQuads(W, TEXT("ST_PhoneSignText"), TEXT("TELEPHONE"), F.P(23.08, 29.2, 206.2), F.Y * -1.0, F.Z, F.X, 9.0, .75, .25, 1.35);
        W.Cone(TEXT("ST_PhoneTube"), F.P(8, -27, 196.3), F.P(8, 27, 196.3), 1.3, 1.3, 8, true, true);
        Bx(TEXT("ST_PhoneSteel"), -1.5, 0, 112, 13.5, 31, 1.2);
        W.BoxAx(TEXT("ST_PhoneBook"), F.P(-5, -14, 116.1), FStFrame::Rot(F.X, F.Z, .14), FStFrame::Rot(F.Y, F.Z, .14), F.Z, FVector(10, 13, 2.9));
        Bx(TEXT("ST_PhoneChrome"), -12, 0, 152, 3, 10.5, 25);
        for (int32 Rw = 0; Rw < 4; ++Rw) for (int32 Cl = 0; Cl < 3; ++Cl) Bx(TEXT("ST_PhoneSteel"), -8.6, (1 - Cl) * 3.4, 158 - Rw * 3.6, .4, 1.3, 1.3);
        const FVector2D UV[4] = { FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0) };
        W.QuadUV(TEXT("ST_PhoneCard"), F.P(-8.95, 8.5, 162.5), F.P(-8.95, -8.5, 162.5), F.P(-8.95, -8.5, 176), F.P(-8.95, 8.5, 176), F.X, UV);
        Bx(TEXT("ST_PhoneBlack"), -8.7, -6, 178.5, .4, 1.6, .4);
        Bx(TEXT("ST_PhoneBlack"), -8.6, 0, 133, .6, 3, 2);
        Bx(TEXT("ST_PhoneChrome"), -9.5, 13.5, 163, 1.5, 2, 4);
        W.Tube(TEXT("ST_PhoneBlack"), { F.P(-7, 13.5, 129), F.P(-5.8, 13.5, 150), F.P(-7, 13.5, 171) }, 2.2, 8, true);
        W.Cone(TEXT("ST_PhoneBlack"), F.P(-8, 13.5, 172), F.P(-3.6, 13.5, 172.6), 3.6, 3.3, 10, true, true);
        W.Cone(TEXT("ST_PhoneBlack"), F.P(-8, 13.5, 128), F.P(-3.6, 13.5, 127.4), 3.4, 3.1, 10, true, true);
        W.Tube(TEXT("ST_PhoneChrome"), { F.P(-9, 10.5, 131), F.P(-6, 8, 122), F.P(-8.5, 4, 117), F.P(-11, 1, 121), F.P(-11.6, 3, 127) }, .8, 6);
    }
}

FVector GrillSmokeOrigin()
{
    return FStFrame::Yawed(FVector(1520, -3140, HouseStreetSceneImpl::Lawn), 30).P(0, 0, 74);
}

void RoadDecals(TArray<FDecal>& Out)
{
    const float NearKerb = HouseStreetSceneImpl::NearKerb, FarKerb = HouseStreetSceneImpl::FarKerb, Centre = HouseStreetSceneImpl::CentreY;
    const float Park = HouseStreetSceneImpl::ParkW, Road = HouseStreetSceneImpl::Road;
    FStRng R(8801);
    auto Add = [&](const TCHAR* Mat, double X, double Y, double Yaw, double HalfAcross, double HalfAlong, int32 Sort)
    {
        Out.Add({ FName(Mat), FVector(X, Y, Road), (float)Yaw, FVector(14, HalfAcross, HalfAlong), Sort });
    };
    // Lane wear, kerb to centre line on each side, 10 m at a time from beyond the lot to the junction.
    for (int32 Side = 0; Side < 2; ++Side)
        for (double X0 = -9000; X0 < 8450; X0 += 1000)
        {
            const double X1 = FMath::Min(X0 + 1000.0, 8450.0);
            const int32 V = FMath::Min(2, (int32)(HouseStreetSceneImpl::Hash2((int32)X0, Side, 77) * 3.f));
            Add(*FString::Printf(TEXT("STD_Half%d"), V), (X0 + X1) * .5, Side ? FarKerb + 340 : NearKerb - 340, Side ? 180 : 0, 340, (X1 - X0) * .5, 0);
        }
    // Worn markings: the double yellow centre line and the parking-lane lines.
    for (double X0 = MarkingsStartX; X0 < 8400; X0 += 1000)
    {
        const double X1 = FMath::Min(X0 + 1000.0, 8400.0), XC = (X0 + X1) * .5, H = (X1 - X0) * .5;
        Add(TEXT("STD_Yellow"), XC, Centre, 0, 32, H, 2);
        Add(TEXT("STD_White"), XC, NearKerb - Park, 0, 16, H, 2);
        Add(TEXT("STD_White"), XC, FarKerb + Park, 180, 16, H, 2);
    }
    // Sealed cracks: transverse every 8-20 m (some right across), and a wandering joint crack beside the centre line.
    for (double X = -8800; X < 8300; X += R.Range(800, 2000))
    {
        const int32 Halves = R.Chance(.35) ? 3 : (R.Chance(.5) ? 1 : 2);
        if (Halves & 1) Add(R.Chance(.5) ? TEXT("STD_Crack0") : TEXT("STD_Crack1"), X + R.Range(-30, 30), NearKerb - 340 - R.Range(0, 20), 90 + R.Range(-6, 6), 20, 340, 3);
        if (Halves & 2) Add(R.Chance(.5) ? TEXT("STD_Crack0") : TEXT("STD_Crack1"), X + R.Range(-30, 30), FarKerb + 340 + R.Range(0, 20), 270 + R.Range(-6, 6), 20, 340, 3);
    }
    for (double X = -8500; X < 8000; X += R.Range(1200, 2600))
        Add(R.Chance(.5) ? TEXT("STD_Crack0") : TEXT("STD_Crack1"), X, Centre + R.Range(-34, 34), R.Range(-3, 3), 20, 340, 3);
    // Patches: squares round the new manholes, two service trenches from the kerbs and a couple of overlays.
    for (const FVector2D& P : { FVector2D(-1050, -5160), FVector2D(4900, -5100), FVector2D(-6100, -5060) }) Add(TEXT("STD_Patch"), P.X, P.Y, 45 + R.Range(-8, 8), 72, 72, 1);
    Add(TEXT("STD_Patch"), 300, NearKerb - 265, 90, 46, 265, 1);
    Add(TEXT("STD_Patch"), -3700, FarKerb + 210, 90, 52, 210, 1);
    Add(TEXT("STD_Patch"), 5600, -4980, 2, 160, 260, 1);
    Add(TEXT("STD_Patch"), -6900, -4420, -1, 110, 170, 1);
    // Oil under the cars at the near kerb (engines forward, +x), a few drips in the lanes, damp patches at the inlets.
    for (const double X : { KerbCarX[0] + 175.0, KerbCarX[1] + 170.0, KerbCarX[2] + 140.0 }) Add(TEXT("STD_Oil"), X, NearKerb - 135, R.Range(0, 180), 46, 58, 4);
    for (int32 K = 0; K < 10; ++K) Add(TEXT("STD_Oil"), R.Range(-8500, 8000), R.Chance(.5) ? R.Range(-4720, -4560) : R.Range(-5140, -4980), R.Range(0, 180), R.Range(14, 30), R.Range(18, 40), 4);
    for (const double X : { -2250.0, 1850.0, 6200.0 }) Add(TEXT("STD_Wet"), X, NearKerb - 55, R.Range(-10, 10), 60, 105, 5);
    for (const double X : { -8100.0, -4200.0, 1200.0, 7200.0 }) Add(TEXT("STD_Wet"), X, FarKerb + 55, R.Range(-10, 10), 60, 105, 5);
}
} // namespace HouseStreetScene
