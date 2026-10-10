// The singing bass: a lofted, textured largemouth bass on a molded shield plaque, built at runtime.
#include "HouseBillyBass.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"
#include "Engine/Texture2D.h"
#include "Math/RandomStream.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
// ------------------------------------------------------------------ profile curves
struct FBbKey { float S, V; };

/** Cardinal-spline through keys (non-uniform S). */
float BbCurve(const TArray<FBbKey>& K, float S)
{
    if (S <= K[0].S) return K[0].V;
    if (S >= K.Last().S) return K.Last().V;
    int32 I = 0; while (I < K.Num() - 2 && S > K[I + 1].S) ++I;
    const FBbKey& A = K[I]; const FBbKey& B = K[I + 1];
    const float H = B.S - A.S, T = (S - A.S) / H;
    auto Slope = [&K](int32 J) { const int32 L = FMath::Max(0, J - 1), R = FMath::Min(K.Num() - 1, J + 1); return (K[R].V - K[L].V) / FMath::Max(1e-4f, K[R].S - K[L].S); };
    const float M0 = Slope(I) * H, M1 = Slope(I + 1) * H;
    const float T2 = T * T, T3 = T2 * T;
    return (2 * T3 - 3 * T2 + 1) * A.V + (T3 - 2 * T2 + T) * M0 + (-2 * T3 + 3 * T2) * B.V + (T3 - T2) * M1;
}

// Body: snout at Y = -15.5 (fish space, head toward -Y), tail stalk at Y = +10.
const float BbY0 = -15.5f, BbLen = 25.5f;
float BbY(float S) { return BbY0 + S * BbLen; }
float BbH(float S)  { static const TArray<FBbKey> K = { {0, 1.3f}, {.06f, 3.1f}, {.15f, 4.9f}, {.3f, 6.3f}, {.45f, 6.7f}, {.6f, 6.1f}, {.75f, 4.5f}, {.88f, 2.8f}, {1, 2.1f} }; return BbCurve(K, S); }
float BbZc(float S) { static const TArray<FBbKey> K = { {0, -1.1f}, {.15f, -.3f}, {.4f, .35f}, {.7f, .3f}, {1, .25f} }; return BbCurve(K, S); }
float BbWf(float S) { static const TArray<FBbKey> K = { {0, 1.f}, {.1f, 2.5f}, {.3f, 3.9f}, {.5f, 4.1f}, {.75f, 2.9f}, {.9f, 1.6f}, {1, 1.2f} }; return BbCurve(K, S); }
float BbWb(float S) { static const TArray<FBbKey> K = { {0, .5f}, {.15f, 1.f}, {.5f, 1.2f}, {.85f, .8f}, {1, .6f} }; return BbCurve(K, S); }
const float BbWall = 2.05f;                      // plaque face the body rests on
float BbXc(float S) { return BbWall + BbWb(S); }
const float BbMouthEnd = .235f;
/** Mouth line: the upper head is clamped above it, the jaw below it. */
float BbCut(float S)
{
    if (S <= .2f) return FMath::Lerp(-1.15f, -2.35f, S / .2f);
    return FMath::Lerp(-2.35f, BbZc(S) - BbH(S) - .6f, FMath::Clamp((S - .2f) / (BbMouthEnd - .2f), 0.f, 1.f));
}
FVector BbRing(float S, float Theta, float Grow = 1.f)
{
    const float C = FMath::Cos(Theta), Sn = FMath::Sin(Theta);
    const float E = .82f;                                            // superellipse: fuller than an ellipse
    const float Cx = FMath::Sign(C) * FMath::Pow(FMath::Abs(C), E), Sz = FMath::Sign(Sn) * FMath::Pow(FMath::Abs(Sn), E);
    const float W = (C >= 0 ? BbWf(S) : BbWb(S)) * Grow;
    return FVector(BbXc(S) + W * Cx, BbY(S), BbZc(S) + BbH(S) * Grow * Sz);
}

// ------------------------------------------------------------------ mesh helpers
void BbTri(FHallSection& S, const FVector& A, const FVector& B, const FVector& C, const FVector& NA, const FVector& NB, const FVector& NC,
    const FVector2D& UA, const FVector2D& UB, const FVector2D& UC)
{
    const int32 Base = S.V.Num();
    const FVector T = (B - A).GetSafeNormal();
    S.V.Append({ A, B, C }); S.N.Append({ NA, NB, NC }); S.UV.Append({ UA, UB, UC });
    S.T.Append({ FProcMeshTangent(T, false), FProcMeshTangent(T, false), FProcMeshTangent(T, false) });
    const FVector Avg = NA + NB + NC;
    if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Avg) < 0) S.I.Append({ Base, Base + 1, Base + 2 });
    else S.I.Append({ Base, Base + 2, Base + 1 });
}
void BbQuad(FHallSection& S, const FVector P[4], const FVector N[4], const FVector2D U[4])
{
    BbTri(S, P[0], P[1], P[2], N[0], N[1], N[2], U[0], U[1], U[2]);
    BbTri(S, P[0], P[2], P[3], N[0], N[2], N[3], U[0], U[2], U[3]);
}

/** Grid loft (rows = stations, columns wrap). Normals from the grid; Flat[i][j] marks clamped (mouth) vertices. */
void BbLoft(FHallMesh& M, FName Skin, FName Inner, const TArray<TArray<FVector>>& G, const TArray<TArray<FVector2D>>& UV,
    const TArray<TArray<uint8>>& Flat, const FVector& FlatNormal, bool bCapStart, bool bCapEnd)
{
    const int32 NS = G.Num(), NR = G[0].Num();
    TArray<TArray<FVector>> N; N.SetNum(NS);
    for (int32 I = 0; I < NS; ++I)
    {
        N[I].SetNum(NR);
        FVector Ctr = FVector::ZeroVector; for (const FVector& P : G[I]) Ctr += P; Ctr /= NR;
        for (int32 J = 0; J < NR; ++J)
        {
            if (Flat[I][J]) { N[I][J] = FlatNormal; continue; }
            const FVector DS = G[FMath::Min(NS - 1, I + 1)][J] - G[FMath::Max(0, I - 1)][J];
            const FVector DT = G[I][(J + 1) % NR] - G[I][(J + NR - 1) % NR];
            FVector Nr = FVector::CrossProduct(DS, DT).GetSafeNormal();
            if (Nr.IsNearlyZero()) Nr = (G[I][J] - Ctr).GetSafeNormal();
            if (FVector::DotProduct(Nr, G[I][J] - Ctr) < 0) Nr = -Nr;
            N[I][J] = Nr;
        }
    }
    for (int32 I = 0; I + 1 < NS; ++I)
        for (int32 J = 0; J < NR; ++J)
        {
            const int32 J1 = (J + 1) % NR;
            const bool bInner = Flat[I][J] && Flat[I][J1] && Flat[I + 1][J] && Flat[I + 1][J1];
            const FVector P[4] = { G[I][J], G[I][J1], G[I + 1][J1], G[I + 1][J] };
            const FVector Nn[4] = { N[I][J], N[I][J1], N[I + 1][J1], N[I + 1][J] };
            FVector2D U[4] = { UV[I][J], UV[I][J1], UV[I + 1][J1], UV[I + 1][J] };
            if (J1 == 0) { U[1].Y += 0.f; }
            BbQuad(M.Sec(bInner ? Inner : Skin), P, Nn, U);
        }
    for (int32 End = 0; End < 2; ++End)
    {
        if ((End == 0 && !bCapStart) || (End == 1 && !bCapEnd)) continue;
        const int32 I = End ? NS - 1 : 0;
        FVector Ctr = FVector::ZeroVector; for (const FVector& P : G[I]) Ctr += P; Ctr /= NR;
        const FVector Axis = (End ? (G[NS - 1][0] - G[NS - 2][0]) : (G[0][0] - G[1][0])).GetSafeNormal();
        const FVector Tip = Ctr + Axis * (End ? .1f : .5f);
        for (int32 J = 0; J < NR; ++J)
        {
            const int32 J1 = (J + 1) % NR;
            BbTri(M.Sec(Skin), Tip, G[I][J], G[I][J1], Axis, (N[I][J] + Axis).GetSafeNormal(), (N[I][J1] + Axis).GetSafeNormal(), UV[I][J], UV[I][J], UV[I][J1]);
        }
    }
}

/** Two-sided fin strip between a base polyline and a tip polyline, in the plane with normal Nrm. */
void BbFin(FHallMesh& M, FName Key, const TArray<FVector>& Base, const TArray<FVector>& Tip, const FVector& Nrm, float Thick)
{
    const int32 Num = FMath::Min(Base.Num(), Tip.Num());
    FHallSection& S = M.Sec(Key);
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const FVector N = Side ? -Nrm : Nrm;
        const FVector Off = N * Thick * .5f;
        for (int32 K = 0; K + 1 < Num; ++K)
        {
            const float U0 = K / float(Num - 1), U1 = (K + 1) / float(Num - 1);
            const FVector P[4] = { Base[K] + Off, Base[K + 1] + Off, Tip[K + 1] + Off * .4f, Tip[K] + Off * .4f };
            const FVector Nn[4] = { N, N, N, N };
            const FVector2D U[4] = { FVector2D(U0, 0), FVector2D(U1, 0), FVector2D(U1, 1), FVector2D(U0, 1) };
            BbQuad(S, P, Nn, U);
        }
    }
    // Tip edge so the fin has a little thickness when seen edge-on.
    for (int32 K = 0; K + 1 < Num; ++K)
    {
        const FVector E = (Tip[K + 1] - Tip[K]).GetSafeNormal();
        FVector Out = FVector::CrossProduct(E, Nrm).GetSafeNormal();
        if (FVector::DotProduct(Out, Tip[K] - Base[K]) < 0) Out = -Out;
        const FVector Off = Nrm * Thick * .2f;
        const FVector P[4] = { Tip[K] - Off, Tip[K + 1] - Off, Tip[K + 1] + Off, Tip[K] + Off };
        const FVector Nn[4] = { Out, Out, Out, Out };
        const FVector2D U[4] = { FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 1), FVector2D(0, 1) };
        BbQuad(S, P, Nn, U);
    }
}

/** Tube along a path (rings perpendicular to the path), optionally closed into a loop. */
void BbTube(FHallMesh& M, FName Key, const TArray<FVector>& Path, const TArray<float>& Radius, int32 Seg, bool bLoop, bool bCapEnd)
{
    const int32 Num = Path.Num();
    TArray<FVector> Tan; Tan.SetNum(Num);
    for (int32 I = 0; I < Num; ++I)
    {
        const FVector A = bLoop ? Path[(I + Num - 1) % Num] : Path[FMath::Max(0, I - 1)];
        const FVector B = bLoop ? Path[(I + 1) % Num] : Path[FMath::Min(Num - 1, I + 1)];
        Tan[I] = (B - A).GetSafeNormal();
    }
    FVector Nx = FVector::CrossProduct(Tan[0], FMath::Abs(Tan[0].Z) < .9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
    TArray<TArray<FVector>> G; G.SetNum(Num); TArray<TArray<FVector>> NN; NN.SetNum(Num);
    for (int32 I = 0; I < Num; ++I)
    {
        Nx = (Nx - Tan[I] * FVector::DotProduct(Nx, Tan[I])).GetSafeNormal();   // parallel transport
        const FVector Ny = FVector::CrossProduct(Tan[I], Nx);
        for (int32 J = 0; J < Seg; ++J)
        {
            const float A = 2 * PI * J / Seg;
            const FVector D = Nx * FMath::Cos(A) + Ny * FMath::Sin(A);
            G[I].Add(Path[I] + D * Radius[I]); NN[I].Add(D);
        }
    }
    FHallSection& S = M.Sec(Key);
    const int32 Rows = bLoop ? Num : Num - 1;
    for (int32 I = 0; I < Rows; ++I)
        for (int32 J = 0; J < Seg; ++J)
        {
            const int32 I1 = (I + 1) % Num, J1 = (J + 1) % Seg;
            const FVector P[4] = { G[I][J], G[I][J1], G[I1][J1], G[I1][J] };
            const FVector Nn[4] = { NN[I][J], NN[I][J1], NN[I1][J1], NN[I1][J] };
            const FVector2D U[4] = { FVector2D(J / float(Seg), I / float(Num)), FVector2D((J + 1) / float(Seg), I / float(Num)), FVector2D((J + 1) / float(Seg), (I + 1) / float(Num)), FVector2D(J / float(Seg), (I + 1) / float(Num)) };
            BbQuad(S, P, Nn, U);
        }
    if (bCapEnd && !bLoop)
        for (int32 J = 0; J < Seg; ++J)
            BbTri(S, Path[Num - 1], G[Num - 1][J], G[Num - 1][(J + 1) % Seg], Tan[Num - 1], Tan[Num - 1], Tan[Num - 1], FVector2D(.5f, 1), FVector2D(0, 1), FVector2D(1, 1));
}

void BbSphere(FHallMesh& M, FName Key, const FVector& C, float R, int32 Seg)
{
    M.Dome(Key, C, FVector::UpVector, R, R, Seg);
    M.Dome(Key, C, -FVector::UpVector, R, R, Seg);
}

/** Beveled ring between an outer and an inner outline (same point count): side walls and a sloped front. */
void BbMolding(FHallMesh& M, FName Key, const TArray<FVector2D>& Outer, const TArray<FVector2D>& Inner, float OuterDepth, float InnerDepth, float InnerFloor)
{
    auto W = [](const FVector2D& P, float X) { return FVector(X, P.X, P.Y); };   // outline in (Y, Z), depth along X
    FHallSection& S = M.Sec(Key);
    const int32 Num = Outer.Num();
    FVector2D Ctr(0, 0); for (const FVector2D& P : Outer) Ctr += P; Ctr /= Num;
    for (int32 K = 0; K < Num; ++K)
    {
        const int32 K1 = (K + 1) % Num;
        // Sloped front.
        {
            const FVector P[4] = { W(Outer[K], OuterDepth), W(Outer[K1], OuterDepth), W(Inner[K1], InnerDepth), W(Inner[K], InnerDepth) };
            FVector N = FVector::CrossProduct(P[1] - P[0], P[3] - P[0]).GetSafeNormal();
            if (N.X < 0) N = -N;
            const FVector Nn[4] = { N, N, N, N };
            const FVector2D U[4] = { FVector2D(K / float(Num), 0), FVector2D(K1 / float(Num), 0), FVector2D(K1 / float(Num), .1f), FVector2D(K / float(Num), .1f) };
            BbQuad(S, P, Nn, U);
        }
        // Outer wall.
        {
            const FVector2D E = Outer[K1] - Outer[K];
            FVector2D Out2(E.Y, -E.X); Out2.Normalize();
            if (FVector2D::DotProduct(Out2, (Outer[K] + Outer[K1]) * .5f - Ctr) < 0) Out2 = -Out2;
            const FVector N(0, Out2.X, Out2.Y);
            const FVector P[4] = { W(Outer[K], 0), W(Outer[K1], 0), W(Outer[K1], OuterDepth), W(Outer[K], OuterDepth) };
            const FVector Nn[4] = { N, N, N, N };
            const FVector2D U[4] = { FVector2D(0, 0), FVector2D(.2f, 0), FVector2D(.2f, .05f), FVector2D(0, .05f) };
            BbQuad(S, P, Nn, U);
        }
        // Inner wall down to the face.
        {
            const FVector2D E = Inner[K1] - Inner[K];
            FVector2D In2(E.Y, -E.X); In2.Normalize();
            if (FVector2D::DotProduct(In2, (Inner[K] + Inner[K1]) * .5f - Ctr) > 0) In2 = -In2;
            const FVector N(0, In2.X, In2.Y);
            const FVector P[4] = { W(Inner[K], InnerFloor), W(Inner[K1], InnerFloor), W(Inner[K1], InnerDepth), W(Inner[K], InnerDepth) };
            const FVector Nn[4] = { N, N, N, N };
            const FVector2D U[4] = { FVector2D(0, 0), FVector2D(.2f, 0), FVector2D(.2f, .05f), FVector2D(0, .05f) };
            BbQuad(S, P, Nn, U);
        }
    }
}

/** Shield outline (Y, Z): a rectangle with scalloped (concave) corners, like the photo. */
TArray<FVector2D> BbShield(float HalfW, float HalfH, float Notch)
{
    TArray<FVector2D> P;
    const FVector2D Corners[4] = { { HalfW, HalfH }, { -HalfW, HalfH }, { -HalfW, -HalfH }, { HalfW, -HalfH } };
    for (int32 C = 0; C < 4; ++C)
    {
        const FVector2D K = Corners[C];
        const FVector2D Sx(FMath::Sign(K.X), 0), Sz(0, FMath::Sign(K.Y));
        // Arc centred on the corner, from the edge along Z to the edge along Y (concave scallop).
        const bool bFwd = (C % 2 == 0);
        for (int32 I = 0; I <= 6; ++I)
        {
            float A = HALF_PI * I / 6.f; if (!bFwd) A = HALF_PI - A;
            const FVector2D Arc = K - Sx * Notch * FMath::Sin(A) - Sz * Notch * FMath::Cos(A);
            P.Add(Arc);
        }
    }
    // Order the four corner arcs counter-clockwise around the centre.
    P.Sort([](const FVector2D& A, const FVector2D& B) { return FMath::Atan2(A.Y, A.X) < FMath::Atan2(B.Y, B.X); });
    return P;
}

/** Same shape inset by D (scaled about the centre, notch kept). */
TArray<FVector2D> BbShieldInset(float HalfW, float HalfH, float Notch, float D) { return BbShield(HalfW - D, HalfH - D, Notch); }

// ------------------------------------------------------------------ textures
UTexture2D* BbTex(UHouseBlackHallSubsystem* Sub, const FHallImage& Img, const TCHAR* Name, bool bWrap)
{
    UTexture2D* T = HallMakeTexture(Img, true, false, Name, bWrap);
    if (T) Sub->Textures.Add(FName(Name), T);
    return T;
}
UTexture2D* BbNormal(UHouseBlackHallSubsystem* Sub, const TArray<float>& Height, int32 W, int32 H, float Strength, const TCHAR* Name, bool bWrap)
{
    FHallImage N; HallHeightToNormal(Height, W, H, Strength, bWrap, N);
    UTexture2D* T = HallMakeTexture(N, false, true, Name, bWrap);
    if (T) Sub->Textures.Add(FName(Name), T);
    return T;
}
FLinearColor BbLerp(const FLinearColor& A, const FLinearColor& B, float T) { return A + (B - A) * FMath::Clamp(T, 0.f, 1.f); }
float BbSmooth(float A, float B, float X) { const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f); return T * T * (3 - 2 * T); }

/** Skin: U along the fish (0 snout .. 1 tail stalk), V top (0) to belly (1). */
void BbPaintSkin(FHallImage& Img, TArray<float>& Height)
{
    const int32 W = 512, H = 256;
    Img.Init(W, H, FLinearColor::Black); Height.Init(.5f, W * H);
    const FLinearColor Back(.045f, .095f, .03f), Flank(.27f, .37f, .09f), Side(.52f, .56f, .18f), Belly(.80f, .80f, .66f);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float U = X / float(W), V = Y / float(H);
            FLinearColor C = Back;
            C = BbLerp(C, Flank, BbSmooth(.16f, .42f, V));
            C = BbLerp(C, Side, BbSmooth(.42f, .6f, V));
            C = BbLerp(C, Belly, BbSmooth(.6f, .74f, V));
            // Mottled back and the dark lateral band of blotches.
            const float M = HallFbm(U * 16.f, V * 8.f, 4, 711);
            const float Upper = 1.f - BbSmooth(.45f, .62f, V);
            C = C * (1.f - .5f * Upper * BbSmooth(.5f, .68f, M));
            const float Band = FMath::Exp(-FMath::Square((V - .46f) / .07f)) * BbSmooth(.2f, .3f, U);
            const float Blotch = BbSmooth(.42f, .6f, HallFbm(U * 22.f, V * 5.f, 3, 333));
            C = C * (1.f - .55f * Band * Blotch);
            // Head: smoother and a little lighter on the cheek.
            if (U < .22f) C = BbLerp(C, C * 1.15f, (1.f - U / .22f) * BbSmooth(.3f, .55f, V));
            C.A = 1.f;
            Img.At(X, Y) = C;
        }
    // Scales (rows staggered, overlapping toward the tail) on the body only.
    const float SW = 9.f, SH = 7.f;
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float U = X / float(W);
            const float Fade = BbSmooth(.2f, .27f, U);
            if (Fade <= 0.f) continue;
            const int32 Row = FMath::FloorToInt(Y / SH);
            const float Off = (Row % 2) ? SW * .5f : 0.f;
            const float Lx = FMath::Fmod(X + Off, SW) / SW, Ly = FMath::Fmod(float(Y), SH) / SH;
            // Crescent: bright near the free (tail-side) edge of each scale.
            const float D = FMath::Sqrt(FMath::Square((Lx - .15f) * 1.1f) + FMath::Square(Ly - .5f));
            const float Ridge = BbSmooth(.95f, .7f, D);
            Height[Y * W + X] = .5f + .5f * Ridge * Fade;
            FLinearColor C = Img.At(X, Y) * (1.f - .14f * (1.f - Ridge) * Fade); C.A = 1.f;
            Img.At(X, Y) = C;
        }
    // Belly spots.
    FRandomStream R(9031);
    for (int32 K = 0; K < 90; ++K)
    {
        const float U = R.FRandRange(.22f, .95f), V = R.FRandRange(.6f, .88f);
        const float Rad = R.FRandRange(1.5f, 3.2f);
        Img.Ellipse(FVector2D(U * W, V * H), FVector2D(Rad * 1.2f, Rad), 0.f, FLinearColor(.16f, .26f, .09f), .75f);
    }
    // Gill cover: two arcs behind the cheek.
    for (int32 Arc = 0; Arc < 2; ++Arc)
    {
        TArray<FVector2D> Line;
        const float U0 = Arc ? .175f : .215f;
        for (int32 I = 0; I <= 16; ++I)
        {
            const float T = I / 16.f;
            const float V = FMath::Lerp(.18f, .82f, T);
            Line.Add(FVector2D((U0 + .035f * FMath::Sin(PI * T)) * W, V * H));
        }
        Img.Stroke(Line, Arc ? 1.5f : 2.5f, FLinearColor(.05f, .07f, .03f), Arc ? .45f : .8f);
        for (const FVector2D& P : Line) { const int32 Ix = FMath::Clamp(int32(P.X), 0, W - 1), Iy = FMath::Clamp(int32(P.Y), 0, H - 1); Height[Iy * W + Ix] = .2f; }
    }
    // Pale lip along the mouth line.
    TArray<FVector2D> Lip; for (int32 I = 0; I <= 10; ++I) Lip.Add(FVector2D(I / 10.f * .2f * W, (.62f + .05f * I / 10.f) * H));
    Img.Stroke(Lip, 4.f, FLinearColor(.7f, .7f, .55f), .5f);
}

/** Fin membrane: neutral, tinted per fin. U along the fin base, V base (0) to edge (1). */
void BbPaintFin(FHallImage& Img, TArray<float>& Height)
{
    const int32 W = 256, H = 128;
    Img.Init(W, H, FLinearColor(.78f, .78f, .74f)); Height.Init(.5f, W * H);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float V = Y / float(H);
            const float Ray = FMath::Fmod(float(X), 16.f);
            const float RayD = FMath::Abs(Ray - 8.f) / 8.f;               // 0 on a ray
            const float IsRay = BbSmooth(.25f, .05f, RayD);
            FLinearColor C = Img.At(X, Y);
            C = C * (1.f - .45f * IsRay) * (1.f - .3f * BbSmooth(.75f, 1.f, V));
            C = C * (.9f + .1f * HallFbm(X / 20.f, Y / 6.f, 3, 42));
            C.A = 1.f;
            Img.At(X, Y) = C;
            Height[Y * W + X] = .5f + .4f * IsRay;
        }
}

void BbPaintWood(FHallImage& Img, TArray<float>& Height)
{
    const int32 W = 512, H = 256;
    Img.Init(W, H, FLinearColor::Black); Height.Init(.5f, W * H);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float U = X / float(W), V = Y / float(H);
            const float Warp = HallFbm(U * 3.f, V * 2.f, 3, 12) * 6.f;
            const float Grain = .5f + .5f * FMath::Sin((V * 34.f + Warp) * PI);
            const float Fine = HallFbm(U * 40.f, V * 160.f, 2, 77);
            FLinearColor C = BbLerp(FLinearColor(.36f, .16f, .05f), FLinearColor(.55f, .28f, .09f), Grain * .7f + Fine * .3f);
            C.A = 1.f;
            Img.At(X, Y) = C;
            Height[Y * W + X] = .5f + .15f * Grain;
        }
}

void BbPaintPlate(FHallImage& Img)
{
    const int32 W = 512, H = 80;
    Img.Init(W, H, FLinearColor(.72f, .55f, .22f));
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            FLinearColor C = Img.At(X, Y) * (.85f + .25f * HallNoise(X / 3.f, Y / 40.f, 5));   // brushed
            C.A = 1.f; Img.At(X, Y) = C;
        }
    Img.Rect(FVector2D(6, 6), FVector2D(W - 6, 9), FLinearColor(.95f, .8f, .45f), .6f);
    FHallTextStyle T; T.Height = 38.f; T.Stroke = 6.f; T.Spacing = .3f; T.WidthScale = .85f;
    const FString Text = TEXT("GONE FISHIN");
    const float Tw = HallTextWidth(Text, T);
    HallDrawText(Img, Text, FVector2D((W - Tw) * .5f, H * .5f + T.Height * .5f), T, FLinearColor(.05f, .04f, .02f));
}

void BbCreateLook(UHouseBlackHallSubsystem* Sub)
{
    if (Sub->Materials.Contains(TEXT("HW_BassSkin"))) return;
    FHallImage Img; TArray<float> Height;
    BbPaintSkin(Img, Height);
    HallMakeTextured(Sub, TEXT("HW_BassSkin"), BbTex(Sub, Img, TEXT("T_HW_BassSkin"), false), BbNormal(Sub, Height, 512, 256, 2.2f, TEXT("T_HW_BassSkinN"), false), .32f, FLinearColor::White, 1.f);
    BbPaintFin(Img, Height);
    UTexture2D* Fin = BbTex(Sub, Img, TEXT("T_HW_BassFin"), false);
    UTexture2D* FinN = BbNormal(Sub, Height, 256, 128, 2.f, TEXT("T_HW_BassFinN"), false);
    HallMakeTextured(Sub, TEXT("HW_BassFinDark"), Fin, FinN, .4f, FLinearColor(.2f, .3f, .1f), 1.f);
    HallMakeTextured(Sub, TEXT("HW_BassFinPale"), Fin, FinN, .4f, FLinearColor(.86f, .84f, .62f), 1.f);
    BbPaintWood(Img, Height);
    HallMakeTextured(Sub, TEXT("HW_BassWood"), BbTex(Sub, Img, TEXT("T_HW_BassWood"), true), BbNormal(Sub, Height, 512, 256, 1.2f, TEXT("T_HW_BassWoodN"), true), .3f);
    BbPaintPlate(Img);
    HallMakeTextured(Sub, TEXT("HW_BassPlate"), BbTex(Sub, Img, TEXT("T_HW_BassPlate"), false), nullptr, .3f, FLinearColor::White, 1.f, .8f);
    HallMakePlain(Sub, TEXT("HW_BassRim"), FLinearColor(.07f, .035f, .018f), .48f);
    HallMakePlain(Sub, TEXT("HW_BassMouth"), FLinearColor(.18f, .02f, .02f), .5f);
    HallMakePlain(Sub, TEXT("HW_BassEye"), FLinearColor(.006f, .006f, .006f), .05f);
    HallMakePlain(Sub, TEXT("HW_BassIris"), FLinearColor(.55f, .42f, .1f), .3f, .3f);
    HallMakePlain(Sub, TEXT("HW_BassButton"), FLinearColor(.7f, .03f, .02f), .3f);
    HallMakePlain(Sub, TEXT("HW_BassSanta"), FLinearColor(.6f, .02f, .02f), .92f);
    HallMakePlain(Sub, TEXT("HW_BassFur"), FLinearColor(.9f, .88f, .84f), 1.f);
    HallMakePlain(Sub, TEXT("HW_BassRibbon"), FLinearColor(.65f, .02f, .03f), .35f);
    HallMakePlain(Sub, TEXT("HW_BassBell"), FLinearColor(.85f, .65f, .2f), .2f, 1.f);
    HallMakePlain(Sub, TEXT("HW_BassPeg"), FLinearColor(.02f, .02f, .02f), .6f);
}

/** Mirror a mesh Y -> -Y (head along +Y) and fix the winding. */
void BbMirrorY(FHallMesh& M)
{
    for (auto& Pair : M.Sections)
    {
        FHallSection& S = Pair.Value;
        for (FVector& V : S.V) V.Y = -V.Y;
        for (FVector& N : S.N) N.Y = -N.Y;
        for (FProcMeshTangent& T : S.T) T.TangentX.Y = -T.TangentX.Y;
        for (int32 I = 0; I + 2 < S.I.Num(); I += 3) Swap(S.I[I + 1], S.I[I + 2]);
    }
}
void BbOffset(FHallMesh& M, const FVector& D)
{
    for (auto& Pair : M.Sections) for (FVector& V : Pair.Value.V) V -= D;
}
FVector BbTop(float S) { return BbRing(S, HALF_PI); }
FVector BbBottom(float S) { return BbRing(S, -HALF_PI); }
FVector BbFront(float S, float Z)
{
    // Point on the hall-facing side of the body at height Z.
    const float Sn = FMath::Clamp((Z - BbZc(S)) / BbH(S), -.98f, .98f);
    return BbRing(S, FMath::Asin(Sn));
}
}

float HwBassMouth(float T)
{
    // Mirrors the song in HouseSfx: 16 beats of .3 s; rests at beats 7 and 15.
    static const int32 Notes[] = { 64, 67, 69, 67, 64, 62, 64, -1, 64, 67, 69, 72, 69, 67, 64, -1 };
    if (T < 0.f) return 0.f;
    const int32 K = FMath::FloorToInt(T / .3f);
    if (K >= int32(UE_ARRAY_COUNT(Notes)) || Notes[K] < 0) return 0.f;
    const float F = FMath::Fmod(T, .3f) / .27f;
    return F >= 1.f ? 0.f : FMath::Sin(PI * F) * (Notes[K] >= 69 ? 1.f : .7f);
}

void HwBuildBillyBass(UHouseBlackHallSubsystem* Look, FHwBassParts& Out)
{
    BbCreateLook(Look);
    const FHallUV Plain = FHallUV::World(20.f);

    // ---------------- plaque (built directly in gadget space, symmetric)
    const float HalfW = 18.5f, HalfH = 12.5f, Notch = 3.2f, Rim = 1.6f;
    const TArray<FVector2D> Outer = BbShield(HalfW, HalfH, Notch);
    const TArray<FVector2D> Inner = BbShieldInset(HalfW, HalfH, Notch, Rim);
    FHallUV WoodUV = FHallUV::Local(FTransform::Identity, 2.f * HalfW);
    Out.Plaque.Prism(TEXT("HW_BassRim"), Outer, FVector(0, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0), .8f, Plain);   // back board
    Out.Plaque.Prism(TEXT("HW_BassWood"), Inner, FVector(.8f, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0), BbWall - .8f, WoodUV);
    BbMolding(Out.Plaque, TEXT("HW_BassRim"), Outer, Inner, 1.9f, 2.55f, BbWall - .05f);
    // Nameplate (reads left to right from the hall: viewer's right is -Y) and the red button.
    {
        const float PW = 7.5f, Z0 = -11.0f, Z1 = -8.9f, X = BbWall + .25f;
        Out.Plaque.BoxMinMax(TEXT("HW_BassRim"), FVector(BbWall, -PW - .25f, Z0 - .25f), FVector(X - .02f, PW + .25f, Z1 + .25f), Plain);
        Out.Plaque.QuadUV(TEXT("HW_BassPlate"), FVector(X, PW, Z1), FVector(X, -PW, Z1), FVector(X, -PW, Z0), FVector(X, PW, Z0), FVector(1, 0, 0),
            FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1), FVector(0, -1, 0));
        Out.Plaque.Cylinder(TEXT("HW_BassButton"), FVector(BbWall, PW + 2.2f, -9.95f), FVector(BbWall + .75f, PW + 2.2f, -9.95f), .85f, 16, Plain);
        Out.Plaque.Cylinder(TEXT("HW_BassRim"), FVector(BbWall, PW + 2.2f, -9.95f), FVector(BbWall + .35f, PW + 2.2f, -9.95f), 1.15f, 16, Plain);
        // Little speaker grille dots under the plate.
        for (int32 K = -2; K <= 2; ++K) Out.Plaque.Cylinder(TEXT("HW_BassPeg"), FVector(BbWall - .02f, K * .9f, -11.8f), FVector(BbWall + .05f, K * .9f, -11.8f), .28f, 8, Plain, true);
    }
    // Two hanger screws at the top, left and right.
    for (int32 Sd = -1; Sd <= 1; Sd += 2)
        Out.Plaque.Cylinder(TEXT("HW_BassBell"), FVector(1.8f, Sd * 13.f, 9.6f), FVector(2.75f, Sd * 13.f, 9.6f), .45f, 10, Plain);

    // ---------------- fish (fish space: head toward -Y), mirrored at the end so the head points left from the hall
    FHallMesh Body, Jaw, Tail;
    const int32 NS = 30, NR = 28;
    {
        TArray<TArray<FVector>> G; TArray<TArray<FVector2D>> UV; TArray<TArray<uint8>> Flat;
        G.SetNum(NS + 1); UV.SetNum(NS + 1); Flat.SetNum(NS + 1);
        for (int32 I = 0; I <= NS; ++I)
        {
            const float S = I / float(NS);
            for (int32 J = 0; J < NR; ++J)
            {
                const float Th = 2 * PI * J / NR;
                FVector P = BbRing(S, Th);
                const float V = FMath::Clamp(.5f - (P.Z - BbZc(S)) / (2.f * BbH(S)), 0.f, 1.f);
                bool bFlat = false;
                if (S < BbMouthEnd) { const float Cut = BbCut(S); if (P.Z < Cut) { P.Z = Cut; bFlat = true; } }
                G[I].Add(P); UV[I].Add(FVector2D(S, V)); Flat[I].Add(bFlat ? 1 : 0);
            }
        }
        BbLoft(Body, TEXT("HW_BassSkin"), TEXT("HW_BassMouth"), G, UV, Flat, FVector(0, 0, -1), true, true);
    }
    // Lower jaw: the part of the head below the mouth line, a touch fuller so the lip shows.
    {
        const int32 JS = 10;
        TArray<TArray<FVector>> G; TArray<TArray<FVector2D>> UV; TArray<TArray<uint8>> Flat;
        G.SetNum(JS + 1); UV.SetNum(JS + 1); Flat.SetNum(JS + 1);
        for (int32 I = 0; I <= JS; ++I)
        {
            const float S = BbMouthEnd * I / float(JS);
            for (int32 J = 0; J < NR; ++J)
            {
                const float Th = 2 * PI * J / NR;
                FVector P = BbRing(S, Th, 1.04f);
                P.Y -= .35f * (1.f - S / BbMouthEnd);                        // lower jaw juts a little past the snout
                const float V = FMath::Clamp(.5f - (P.Z - BbZc(S)) / (2.f * BbH(S)), 0.f, 1.f);
                const float Cut = BbCut(S);
                bool bFlat = false;
                if (P.Z > Cut) { P.Z = Cut; bFlat = true; }
                G[I].Add(P); UV[I].Add(FVector2D(S, V)); Flat[I].Add(bFlat ? 1 : 0);
            }
        }
        BbLoft(Jaw, TEXT("HW_BassSkin"), TEXT("HW_BassMouth"), G, UV, Flat, FVector(0, 0, 1), true, false);
        // A pale tongue and throat inside.
        Jaw.BoxMinMax(TEXT("HW_BassMouth"), FVector(BbXc(.12f) - .5f, BbY(.07f), BbCut(.12f) - .6f), FVector(BbXc(.12f) + .7f, BbY(.18f), BbCut(.12f) - .05f), Plain);
    }
    const FVector SideN(1, 0, 0);
    // Spiny dorsal fin (sawtooth spines leaning back) and the soft dorsal behind it.
    {
        TArray<FVector> Base, Tip;
        const int32 Spines = 10;
        for (int32 K = 0; K <= Spines * 2; ++K)
        {
            const float T = K / float(Spines * 2), S = FMath::Lerp(.3f, .56f, T);
            const FVector B = BbTop(S) - FVector(0, 0, .7f) + FVector(.2f, 0, 0);
            const float Hgt = (K % 2 == 0) ? 3.6f * FMath::Sin(PI * FMath::Lerp(.15f, .95f, T)) + .4f : 1.9f * FMath::Sin(PI * FMath::Lerp(.15f, .95f, T)) + .3f;
            Base.Add(B); Tip.Add(B + FVector(0, .9f, Hgt + .7f));
        }
        BbFin(Body, TEXT("HW_BassFinDark"), Base, Tip, SideN, .3f);
        Base.Reset(); Tip.Reset();
        for (int32 K = 0; K <= 10; ++K)
        {
            const float T = K / 10.f, S = FMath::Lerp(.57f, .82f, T);
            const FVector B = BbTop(S) - FVector(0, 0, .7f) + FVector(.2f, 0, 0);
            const float Hgt = 3.1f * FMath::Sin(PI * FMath::Lerp(.12f, .85f, T)) + .6f;
            Base.Add(B); Tip.Add(B + FVector(0, .8f, Hgt + .7f));
        }
        BbFin(Body, TEXT("HW_BassFinDark"), Base, Tip, SideN, .3f);
    }
    // Anal fin under the rear of the body; pelvic fin hanging under the chest.
    {
        TArray<FVector> Base, Tip;
        for (int32 K = 0; K <= 8; ++K)
        {
            const float T = K / 8.f, S = FMath::Lerp(.68f, .84f, T);
            const FVector B = BbBottom(S) + FVector(.3f, 0, .7f);
            Base.Add(B); Tip.Add(B + FVector(0, 1.1f, -(2.4f * FMath::Sin(PI * FMath::Lerp(.2f, .9f, T)) + .8f)));
        }
        BbFin(Body, TEXT("HW_BassFinPale"), Base, Tip, SideN, .3f);
        Base.Reset(); Tip.Reset();
        for (int32 K = 0; K <= 6; ++K)
        {
            const float T = K / 6.f, S = FMath::Lerp(.3f, .4f, T);
            const FVector B = BbBottom(S) + FVector(1.2f, 0, .6f);
            Base.Add(B); Tip.Add(B + FVector(.4f, 2.2f, -(2.6f * FMath::Sin(PI * FMath::Lerp(.3f, .95f, T)) + .7f)));
        }
        BbFin(Body, TEXT("HW_BassFinPale"), Base, Tip, FVector(1, 0, .15f).GetSafeNormal(), .25f);
    }
    // Pectoral fin: a fan just behind the gill cover, held slightly off the flank.
    {
        TArray<FVector> Base, Tip;
        const FVector Root = BbFront(.27f, -1.2f) + FVector(.25f, 0, 0);
        for (int32 K = 0; K <= 8; ++K)
        {
            const float T = K / 8.f;
            const FVector B = Root + FVector(0, 0, FMath::Lerp(-.9f, .9f, T));
            const float A = FMath::Lerp(-35.f, 15.f, T) * PI / 180.f;
            const float L = 4.8f * (.75f + .25f * FMath::Sin(PI * T));
            Base.Add(B); Tip.Add(B + FVector(.9f * (1.f - T), L * FMath::Cos(A), L * FMath::Sin(A)));
        }
        BbFin(Body, TEXT("HW_BassFinPale"), Base, Tip, FVector(1, -.12f, 0).GetSafeNormal(), .22f);
    }
    // Eye: gold iris ring with a glossy black ball, set into the head.
    {
        const float S = .115f, Z = BbZc(S) + BbH(S) * .38f;
        const FVector P = BbFront(S, Z);
        const float Sn = FMath::Clamp((Z - BbZc(S)) / BbH(S), -.98f, .98f), C = FMath::Sqrt(1.f - Sn * Sn);
        const FVector N = FVector(C / BbWf(S), 0, Sn / BbH(S)).GetSafeNormal();
        Body.Dome(TEXT("HW_BassIris"), P - N * .25f, N, 1.55f, .5f, 20);
        Body.Dome(TEXT("HW_BassEye"), P - N * .1f, N, 1.15f, .85f, 20);
        Body.Dome(TEXT("HW_BassFur"), P + N * .55f + FVector(0, -.3f, .35f), N, .22f, .12f, 8);    // highlight glint
    }
    // Nostril dot.
    Body.Dome(TEXT("HW_BassEye"), BbFront(.04f, BbZc(.04f) + BbH(.04f) * .3f), FVector(1, -.4f, .3f).GetSafeNormal(), .28f, .12f, 8);
    // Tail fin (flaps on its own hinge at the stalk).
    const FVector TailRoot(BbXc(1.f), BbY(1.f) - .4f, BbZc(1.f));
    {
        TArray<FVector> Base, Tip;
        const int32 NumT = 16;
        for (int32 K = 0; K <= NumT; ++K)
        {
            const float T = K / float(NumT);
            const float Zb = FMath::Lerp(-BbH(1.f) * .9f, BbH(1.f) * .9f, T);
            const FVector B = TailRoot + FVector(0, -.9f, Zb);
            // Forked outline: lower lobe tip, notch in the middle, upper lobe tip.
            const float Fork = FMath::Abs(T - .5f) * 2.f;                          // 0 at the notch, 1 at the lobe tips
            const float Reach = FMath::Lerp(4.6f, 8.2f, FMath::Pow(Fork, .7f));
            const float Zt = FMath::Lerp(-6.4f, 6.8f, T) + BbZc(1.f);
            Base.Add(B); Tip.Add(FVector(TailRoot.X, TailRoot.Y + Reach, Zt));
        }
        BbFin(Tail, TEXT("HW_BassFinDark"), Base, Tip, SideN, .35f);
    }
    // Ribbon with a bell around the tail stalk.
    {
        const float S = .9f;
        TArray<FVector> Loop; TArray<float> R;
        for (int32 K = 0; K < 24; ++K) { FVector P = BbRing(S, 2 * PI * K / 24.f, 1.06f); Loop.Add(P); R.Add(.32f); }
        BbTube(Body, TEXT("HW_BassRibbon"), Loop, R, 8, true, false);
        const FVector Knot = BbFront(S, BbZc(S) - BbH(S) * .55f) + FVector(.3f, 0, 0);
        BbSphere(Body, TEXT("HW_BassBell"), Knot + FVector(.5f, 0, -.9f), .7f, 14);
        Body.BoxMinMax(TEXT("HW_BassPeg"), Knot + FVector(1.0f, -.35f, -1.55f), Knot + FVector(1.15f, .35f, -1.4f), Plain);   // bell slot
    }
    // Santa hat on the head: fur brim, a floppy cone that droops back toward the tail, a pompom.
    {
        const float S = .17f;
        const FVector Base = BbTop(S) + FVector(.5f, 0, -.9f);
        TArray<FVector> Brim; TArray<float> BR;
        for (int32 K = 0; K < 24; ++K)
        {
            const float A = 2 * PI * K / 24.f;
            Brim.Add(Base + FVector(FMath::Cos(A) * 3.6f, FMath::Sin(A) * 3.9f, FMath::Sin(A) * .7f));   // tilted back with the head
            BR.Add(1.05f);
        }
        BbTube(Body, TEXT("HW_BassFur"), Brim, BR, 10, true, false);
        TArray<FVector> Path; TArray<float> Rad;
        const int32 Steps = 14;
        for (int32 K = 0; K <= Steps; ++K)
        {
            const float T = K / float(Steps);
            const float Up = 9.f * FMath::Sin(T * PI * .62f);
            const float Back = 2.f * T + 7.5f * T * T * T;
            const float Droop = -3.5f * FMath::Max(0.f, T - .6f) * FMath::Max(0.f, T - .6f) * 6.f;
            Path.Add(Base + FVector(.6f * T, Back, Up + Droop + .2f));
            Rad.Add(FMath::Lerp(3.35f, .55f, FMath::Pow(T, .8f)));
        }
        BbTube(Body, TEXT("HW_BassSanta"), Path, Rad, 16, false, true);
        BbSphere(Body, TEXT("HW_BassFur"), Path.Last() + (Path.Last() - Path[Steps - 1]).GetSafeNormal() * .9f, 1.55f, 14);
    }

    // Pivots (fish space): the body swings out from near the tail, the jaw from below the eye, the tail at the stalk.
    FVector BodyPivot(BbWall + .2f, BbY(.86f), BbZc(.86f));
    FVector JawPivot(BbXc(.2f), BbY(.205f), BbCut(.2f));
    FVector TailPivot = TailRoot;
    // A peg hides the swing joint.
    Out.Plaque.Cylinder(TEXT("HW_BassPeg"), FVector(BbWall - .1f, -BodyPivot.Y, BodyPivot.Z), FVector(BbWall + .9f, -BodyPivot.Y, BodyPivot.Z), .7f, 12, Plain);

    BbMirrorY(Body); BbMirrorY(Jaw); BbMirrorY(Tail);
    BodyPivot.Y = -BodyPivot.Y; JawPivot.Y = -JawPivot.Y; TailPivot.Y = -TailPivot.Y;
    BbOffset(Body, BodyPivot); BbOffset(Jaw, JawPivot); BbOffset(Tail, TailPivot);
    Out.Body = MoveTemp(Body); Out.Jaw = MoveTemp(Jaw); Out.Tail = MoveTemp(Tail);
    Out.BodyPivot = BodyPivot; Out.JawPivot = JawPivot; Out.TailPivot = TailPivot;
    Out.HeadSign = 1.f;
}
