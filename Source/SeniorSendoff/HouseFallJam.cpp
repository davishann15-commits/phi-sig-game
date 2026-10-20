// The morning-after of the Fall Harvest Jam (feat. Project Jaystorm) on the east lawn. See HouseFallJam.h.
#include "HouseFallJam.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"

namespace HouseFallJamImpl
{
constexpr double Pi = 3.14159265358979323846;
constexpr float G = -60.f;                                  // lawn grade
// The field is the whole east lawn: from the side drive to the side-street sidewalk, from the front sidewalk (its lamp
// poles, bench and stop sign stay outside) up to the top of the flat ground behind the stage.
constexpr float ZX0 = 4050.f, ZX1 = 8400.f, ZY0 = -3660.f, ZY1 = -100.f;   // the field (trees keep out of it)
constexpr float FX0 = 4150.f, FX1 = 8300.f, FY0 = -3560.f, FY1 = -200.f;   // the fence line
constexpr float GateY0 = -1210.f, GateY1 = -910.f;          // the gap in the west fence, facing the drive and the side door
constexpr float HayGateX0 = 6700.f, HayGateX1 = 7100.f;     // the wide gap in the south fence the hay wagon came through
// The stage: plywood deck on scaffold, 100 cm up, facing the street (-y).
constexpr float SX0 = 5880.f, SX1 = 6734.f, SY0 = -900.f, SY1 = -412.f, SZ = G + 100.f;
constexpr float SCX = (SX0 + SX1) * .5f;
const FVector FirePit(7300, -1950, G);

// Keys. ST_* are HouseStreetScene's (cups, cans, chair webbing, rope).
const FName Straw(TEXT("FJ_Straw")), Twine(TEXT("FJ_Twine")), Plywood(TEXT("FJ_Plywood")), Scaffold(TEXT("FJ_Scaffold")), Skirt(TEXT("FJ_Skirt")),
    Speaker(TEXT("FJ_Speaker")), Grille(TEXT("FJ_Grille")), Tolex(TEXT("FJ_Tolex")), AmpGrille(TEXT("FJ_AmpGrille")), Chrome(TEXT("FJ_Chrome")),
    Shell(TEXT("FJ_DrumShell")), Head(TEXT("FJ_DrumHead")), Cymbal(TEXT("FJ_Cymbal")), Black(TEXT("FJ_Black")), Cable(TEXT("FJ_Cable")),
    GuitarRed(TEXT("FJ_GuitarRed")), Sunburst(TEXT("FJ_Sunburst")), Neck(TEXT("FJ_Neck")), Spruce(TEXT("FJ_Spruce")), Plush(TEXT("FJ_Plush")),
    Strings(TEXT("FJ_Strings")), Pumpkin(TEXT("FJ_Pumpkin")), PalePumpkin(TEXT("FJ_PalePumpkin")), Gourd(TEXT("FJ_Gourd")), Stem(TEXT("FJ_Stem")),
    Corn(TEXT("FJ_Corn")), CornLeaf(TEXT("FJ_CornLeaf")), Flannel(TEXT("FJ_Flannel")), Denim(TEXT("FJ_Denim")), Burlap(TEXT("FJ_Burlap")),
    Hat(TEXT("FJ_Hat")), Barrel(TEXT("FJ_Barrel")), Hoop(TEXT("FJ_Hoop")), Crate(TEXT("FJ_Crate")), Apple(TEXT("FJ_Apple")), Chalk(TEXT("FJ_Chalk")),
    Bulb(TEXT("FJ_Bulb")), DeadBulb(TEXT("FJ_DeadBulb")), Cord(TEXT("FJ_Cord")), Streamer0(TEXT("FJ_Streamer0")), Streamer1(TEXT("FJ_Streamer1")),
    Balloon0(TEXT("FJ_Balloon0")), Balloon1(TEXT("FJ_Balloon1")), TrashBag(TEXT("FJ_TrashBag")), Drum55(TEXT("FJ_Drum55")), Plate(TEXT("FJ_Plate")),
    Pizza(TEXT("FJ_Pizza")), Slat(TEXT("FJ_Slat")), TPost(TEXT("FJ_TPost")), Barricade(TEXT("FJ_Barricade")), TableTop(TEXT("FJ_TableTop")),
    TableLeg(TEXT("FJ_TableLeg")), Chair(TEXT("FJ_Chair")), Mixer(TEXT("FJ_Mixer")), Fader(TEXT("FJ_Fader")), Ember(TEXT("FJ_Ember")),
    Ash(TEXT("FJ_Ash")), Log(TEXT("FJ_Log")), Stone(TEXT("FJ_Stone")), KeyWhite(TEXT("FJ_KeyWhite")), Backdrop(TEXT("FJ_Backdrop")),
    Sweater(TEXT("FJ_Sweater")), Shoe(TEXT("FJ_Shoe")), Sole(TEXT("FJ_Sole")), Frisbee(TEXT("FJ_Frisbee")), Cooler(TEXT("FJ_Cooler")),
    CoolerLid(TEXT("FJ_CoolerLid")), Wire(TEXT("FJ_Wire")), Timber(TEXT("FJ_Timber")), Napkin(TEXT("FJ_Napkin")),
    Potty(TEXT("FJ_Potty")), PottyRoof(TEXT("FJ_PottyRoof")), Keg(TEXT("FJ_Keg")), TractorRed(TEXT("FJ_TractorRed")), Rim(TEXT("FJ_Rim")),
    Tyre(TEXT("FJ_Tyre")), Tarp(TEXT("FJ_Tarp")),
    CupRed(TEXT("ST_CupRed")), Can(TEXT("ST_CanSilver"));

float PeriodFor(FName K)
{
    if (K == Straw) return 90.f;
    if (K == Plywood) return 122.f;
    if (K == Flannel) return 40.f;
    if (K == Backdrop) return 488.f;
    return 100.f;
}

struct FRng
{
    uint32 S;
    explicit FRng(uint32 Seed) : S(Seed ? Seed : 1u) {}
    uint32 Next() { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
    double Unit() { return (Next() & 0xffffff) / 16777216.0; }
    double Range(double A, double B) { return A + (B - A) * Unit(); }
    bool Chance(double P) { return Unit() < P; }
    int32 Int(int32 N) { return (int32)(Next() % (uint32)FMath::Max(1, N)); }
};

/** Triangles into FHallMesh sections: world-planar UVs (FHallMesh::PlanarAxes convention) and front faces wound to the normal. */
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
    void Emit(FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& NA, const FVector& NB, const FVector& NC, const FVector2D* UV = nullptr)
    {
        const FVector Gn = FVector::CrossProduct(B - A, C - A);
        if (Gn.SizeSquared() < 1e-10) return;
        if (FVector::DotProduct(Gn, NA + NB + NC) < 0) { const FVector P[3] = { A, B, C }, N[3] = { NA, NB, NC }; Push(K, P, N, UV); }
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
        Emit(K, A, B, C, N, N, N, T0); Emit(K, A, C, D, N, N, N, T1);
    }
    /** Two-sided flat quad (leaves, streamers, napkins, cloth). */
    void Sheet(FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
    {
        FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        if (N.IsNearlyZero()) return;
        Quad(K, A, B, C, D, N); Quad(K, A, D, C, B, -N);
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
    void Box(FName K, const FVector& Min, const FVector& Max) { BoxAx(K, (Min + Max) * .5f, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), (Max - Min) * .5f); }
    /** Box of half size H centred at C, yawed (degrees) about z, then optionally tilted (degrees) about its local x. */
    void BoxYaw(FName K, const FVector& C, const FVector& H, double Yaw, double Tilt = 0.0)
    {
        const double Y = Yaw * Pi / 180.0, T = Tilt * Pi / 180.0;
        const FVector X(FMath::Cos(Y), FMath::Sin(Y), 0), Yv(-FMath::Sin(Y), FMath::Cos(Y), 0);
        const FVector Y2 = Yv * FMath::Cos(T) + FVector::UpVector * FMath::Sin(T), Z2 = FVector::UpVector * FMath::Cos(T) - Yv * FMath::Sin(T);
        BoxAx(K, C, X, Y2, Z2, H);
    }
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
    void Tube(FName K, const FVector& A, const FVector& B, double R, int32 Seg = 6) { Cone(K, A, B, R, R, Seg); }
    /** Flat disc facing N. */
    void Disc(FName K, const FVector& C, const FVector& N, double R, int32 Seg)
    {
        const FVector X = FVector::CrossProduct(N, FMath::Abs(N.Z) < .9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal(), Y = FVector::CrossProduct(N, X);
        for (int32 S = 0; S < Seg; ++S)
        {
            const double A0 = 2 * Pi * S / Seg, A1 = 2 * Pi * (S + 1) / Seg;
            Tri(K, C, C + (X * FMath::Cos(A0) + Y * FMath::Sin(A0)) * R, C + (X * FMath::Cos(A1) + Y * FMath::Sin(A1)) * R, N);
        }
    }
    /** Surface of revolution about an axis through Base: Profile (radius, distance along the axis), smooth across rings. */
    void Lathe(FName K, const FVector& Base, const FVector& Axis, const TArray<FVector2D>& P, int32 Seg)
    {
        const FVector Z = Axis.GetSafeNormal();
        const FVector X = FVector::CrossProduct(Z, FMath::Abs(Z.Z) < .9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal(), Y = FVector::CrossProduct(Z, X);
        const int32 N = P.Num();
        auto Unit = [](const FVector2D& V) { return V / FMath::Max(1e-6, V.Size()); };
        auto EdgeN = [&](int32 E) { const FVector2D D = P[E + 1] - P[E]; return Unit(FVector2D(D.Y, -D.X)); };
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
                const FVector D0 = X * FMath::Cos(A0) + Y * FMath::Sin(A0), D1 = X * FMath::Cos(A1) + Y * FMath::Sin(A1);
                auto At = [&](const FVector& D, const FVector2D& Q) { return Base + D * Q.X + Z * Q.Y; };
                auto Nm = [&](const FVector& D, const FVector2D& Q) { return (D * Q.X + Z * Q.Y).GetSafeNormal(); };
                const double U0 = A0 * P[E].X / Period, U1 = A1 * P[E].X / Period;
                const FVector2D T0[3] = { FVector2D(U0, P[E].Y / Period), FVector2D(U1, P[E].Y / Period), FVector2D(U1, P[E + 1].Y / Period) };
                const FVector2D T1[3] = { FVector2D(U0, P[E].Y / Period), FVector2D(U1, P[E + 1].Y / Period), FVector2D(U0, P[E + 1].Y / Period) };
                Emit(K, At(D0, P[E]), At(D1, P[E]), At(D1, P[E + 1]), Nm(D0, NA), Nm(D1, NA), Nm(D1, NB), T0);
                Emit(K, At(D0, P[E]), At(D1, P[E + 1]), At(D0, P[E + 1]), Nm(D0, NA), Nm(D1, NB), Nm(D0, NB), T1);
            }
        }
    }
    /** Sphere (squashed by ZScale) of radius R at C. */
    void Sphere(FName K, const FVector& C, double R, double ZScale = 1.0, int32 Seg = 10)
    {
        TArray<FVector2D> P;
        for (int32 I = 0; I <= 8; ++I) { const double T = Pi * I / 8; P.Add(FVector2D(R * FMath::Sin(T), -R * ZScale * FMath::Cos(T))); }
        Lathe(K, C, FVector::UpVector, P, Seg);
    }
    /** A flat shape with a star-shaped outline (local u across, v along), extruded Thick along N; Frame maps local to world. */
    void Extrude(FName K, const TArray<FVector2D>& Outline, double Thick, const FVector& O, const FVector& U, const FVector& V, const FVector& N)
    {
        FVector2D Ctr(0, 0);
        for (const FVector2D& P : Outline) Ctr = Ctr + P;
        Ctr /= FMath::Max(1, Outline.Num());
        auto W = [&](const FVector2D& P, double H) { return O + U * P.X + V * P.Y + N * H; };
        const int32 Cnt = Outline.Num();
        for (int32 I = 0; I < Cnt; ++I)
        {
            const FVector2D& A = Outline[I]; const FVector2D& B = Outline[(I + 1) % Cnt];
            Tri(K, W(Ctr, Thick), W(A, Thick), W(B, Thick), N);
            Tri(K, W(Ctr, 0), W(B, 0), W(A, 0), -N);
            const FVector2D E = B - A;
            const FVector SN = (U * E.Y - V * E.X).GetSafeNormal();
            Quad(K, W(A, 0), W(B, 0), W(B, Thick), W(A, Thick), SN);
        }
    }
    /** Polyline as a tube. */
    void Path(FName K, const TArray<FVector>& P, double R, int32 Seg = 5)
    {
        for (int32 I = 0; I + 1 < P.Num(); ++I) Cone(K, P[I], P[I + 1], R, R, Seg, I == 0, I + 2 == P.Num());
    }
};

double Dist2D(const FVector& A, const FVector& B) { return FMath::Sqrt(FMath::Square(A.X - B.X) + FMath::Square(A.Y - B.Y)); }

FVector Yawed(const FVector& O, double Yaw, double LX, double LY, double LZ)
{
    const double A = Yaw * Pi / 180.0;
    return O + FVector(LX * FMath::Cos(A) - LY * FMath::Sin(A), LX * FMath::Sin(A) + LY * FMath::Cos(A), LZ);
}

// ------------------------------------------------------------------ textures
void PaintStraw(FHallImage& Img, TArray<float>& Height)
{
    const int32 N = 256;
    Img.Init(N, N, FLinearColor(.55f, .42f, .17f)); Height.Init(.5f, N * N);
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            // Packed stalks: long streaks along u, broken up, with brighter and darker straws and dark gaps.
            const float Streak = HallNoise(X * .04f, Y * .9f, 301), Grain = HallNoise(X * .5f, Y * .5f, 302), Patch = HallFbm(X * .03f, Y * .03f, 2, 303);
            const float L = .55f + .5f * Streak + .18f * (Grain - .5f) + .22f * (Patch - .5f);
            const float Gap = Streak < .22f ? .55f : 1.f;
            Img.At(X, Y) = FLinearColor(.60f * L * Gap, .46f * L * Gap, .18f * L * Gap * (1.f - .3f * Patch));
            Height[Y * N + X] = .3f + .7f * Streak * Gap;
        }
}

void PaintPlywood(FHallImage& Img)
{
    const int32 N = 256;   // one 122 x 122 cm tile of the deck: a sheet with the grain, scuffs and a dark edge
    Img.Init(N, N, FLinearColor(.48f, .36f, .20f));
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const float Grain = HallNoise(X * .02f + HallFbm(X * .03f, Y * .03f, 2, 311) * 2.f, Y * .6f, 312);
            const float Scuff = HallFbm(X * .05f, Y * .05f, 3, 313);
            float L = .78f + .25f * Grain + .25f * (Scuff - .5f);
            if (X < 2 || Y < 2 || X > N - 3 || Y > N - 3) L *= .55f;
            Img.At(X, Y) = FLinearColor(.50f * L, .37f * L, .20f * L);
        }
}

void PaintFlannel(FHallImage& Img)
{
    const int32 N = 128;   // 40 cm of red-and-black buffalo check
    Img.Init(N, N, FLinearColor::Black);
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const bool A = (X / 32) % 2 == 0, B = (Y / 32) % 2 == 0;
            const float Wv = .9f + .2f * HallNoise(X * .8f, Y * .8f, 321);
            FLinearColor C = A == B ? (A ? FLinearColor(.55f, .04f, .03f) : FLinearColor(.025f, .02f, .02f)) : FLinearColor(.26f, .03f, .025f);
            Img.At(X, Y) = C * Wv;
        }
}

void PaintBackdrop(FHallImage& Img)
{
    // Two sheets of plywood painted on the lawn the afternoon before: FALL HARVEST JAM, the band, the date, leaves, a sheaf.
    const int32 W = 1024, H = 256;
    Img.Init(W, H, FLinearColor(.47f, .35f, .19f));
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float Grain = HallNoise(X * .015f, Y * .5f, 331), Wash = HallFbm(X * .01f, Y * .02f, 2, 332);
            Img.At(X, Y) = Img.At(X, Y) * (.82f + .22f * Grain + .2f * (Wash - .5f));
            if (X == W / 2 || X == W / 2 + 1) Img.At(X, Y) *= .6f;   // the seam between the sheets
        }
    const FLinearColor Cream(.86f, .80f, .62f), Rust(.62f, .18f, .04f), Gold(.85f, .58f, .08f), Green(.12f, .22f, .06f), Ink(.03f, .025f, .02f);
    // A cream field with a ragged brushed edge, the lettering over it.
    for (int32 Y = 18; Y < H - 18; ++Y)
        for (int32 X = 40; X < W - 40; ++X)
        {
            const float Edge = FMath::Min(FMath::Min(X - 40, W - 41 - X) / 18.f, FMath::Min(Y - 18, H - 19 - Y) / 10.f);
            const float Rag = HallFbm(X * .08f, Y * .08f, 2, 333);
            if (Edge + (Rag - .5f) * 1.2f > .5f) Img.At(X, Y) = Cream * (.9f + .2f * HallNoise(X * .3f, Y * .3f, 334));
        }
    FHallTextStyle S; S.Height = 56; S.Stroke = 10.f; S.Spacing = .18f;
    const FString WordA = TEXT("FALL"), WordB = TEXT("HARVEST"), WordC = TEXT("JAM");
    const float Gap = 30.f, WA = HallTextWidth(WordA, S), WB = HallTextWidth(WordB, S), WC = HallTextWidth(WordC, S);
    const float TX = (W - (WA + Gap + WB + Gap + WC)) * .5f;
    HallDrawText(Img, WordA, FVector2D(TX, 94), S, Green);
    HallDrawText(Img, WordB, FVector2D(TX + WA + Gap, 94), S, Rust);
    HallDrawText(Img, WordC, FVector2D(TX + WA + Gap + WB + Gap, 94), S, Green);
    FHallTextStyle Ft; Ft.Height = 40; Ft.Stroke = 7.f; Ft.Spacing = .2f;
    const FString Feat = TEXT("FEAT: PROJECT JAYSTORM");
    HallDrawText(Img, Feat, FVector2D((W - HallTextWidth(Feat, Ft)) * .5f, 150), Ft, Ink);
    FHallTextStyle D; D.Height = 26; D.Stroke = 5.f; D.Spacing = .2f;
    const FString Line2 = TEXT("1986   LIVE BANDS   HOT CIDER   HAYRIDES");
    D.WidthScale = FMath::Min(1.f, 860.f / HallTextWidth(Line2, D));
    HallDrawText(Img, Line2, FVector2D((W - HallTextWidth(Line2, D)) * .5f, 208), D, Ink);
    // Maple leaves in the corners and a wheat sheaf either side of the '86.
    auto Leaf = [&](float CX, float CY, float R, float Rot, const FLinearColor& C)
    {
        TArray<FVector2D> P;
        for (int32 I = 0; I < 36; ++I)
        {
            const float A = I / 36.f * 2 * 3.14159f;
            const float Lobe = .55f + .45f * FMath::Abs(FMath::Cos(A * 2.5f + .3f)) * (FMath::Cos(A) > -.4f ? 1.f : .6f);
            const float Rr = R * Lobe;
            P.Add(FVector2D(CX + Rr * FMath::Cos(A + Rot), CY + Rr * FMath::Sin(A + Rot)));
        }
        Img.FillPolygon(P, C);
        Img.Rect(FVector2D(CX - 1, CY), FVector2D(CX + 1, CY + R * 1.1f), Ink * .6f + C * .4f);
    };
    Leaf(58, 48, 26, .2f, Rust); Leaf(W - 58, 48, 26, -.4f, Gold); Leaf(58, H - 48, 24, 1.f, Gold); Leaf(W - 58, H - 48, 24, 2.f, Rust);
    for (int32 Side = -1; Side <= 1; Side += 2)
        for (int32 K = -3; K <= 3; ++K)
        {
            const float X0 = W / 2 + Side * 470, Y0 = 150, X1 = X0 + K * 6, Y1 = 92 + FMath::Abs(K) * 6;
            for (int32 T = 0; T <= 20; ++T) { const float F = T / 20.f; Img.Rect(FVector2D(X0 + (X1 - X0) * F - 1, Y0 + (Y1 - Y0) * F - 1), FVector2D(X0 + (X1 - X0) * F + 1, Y0 + (Y1 - Y0) * F + 1), Gold); }
            Img.Ellipse(FVector2D(X1, Y1 - 6), FVector2D(3.5f, 8), K * 12.f, Gold);
        }
    for (int32 Side = -1; Side <= 1; Side += 2) Img.Rect(FVector2D(W / 2 + Side * 470 - 14, 134), FVector2D(W / 2 + Side * 470 + 14, 140), Rust);
}

void PaintChalk(FHallImage& Img)
{
    const int32 W = 128, H = 176;
    Img.Init(W, H, FLinearColor(.06f, .065f, .06f));
    for (int32 Y = 0; Y < H; ++Y) for (int32 X = 0; X < W; ++X) Img.At(X, Y) = Img.At(X, Y) * (.8f + .5f * HallFbm(X * .06f, Y * .06f, 2, 341));
    Img.Rect(FVector2D(0, 0), FVector2D(W, 6), FLinearColor(.4f, .3f, .18f)); Img.Rect(FVector2D(0, H - 6), FVector2D(W, H), FLinearColor(.4f, .3f, .18f));
    Img.Rect(FVector2D(0, 0), FVector2D(6, H), FLinearColor(.4f, .3f, .18f)); Img.Rect(FVector2D(W - 6, 0), FVector2D(W, H), FLinearColor(.4f, .3f, .18f));
    const FLinearColor White(.85f, .85f, .8f), Yellow(.8f, .7f, .3f);
    FHallTextStyle S; S.Height = 26; S.Stroke = 4.f; S.Spacing = .2f;
    HallDrawText(Img, TEXT("CIDER"), FVector2D((W - HallTextWidth(TEXT("CIDER"), S)) * .5f, 46), S, White);
    S.Height = 16; S.Stroke = 3.f;
    HallDrawText(Img, TEXT("HOT OR COLD"), FVector2D((W - HallTextWidth(TEXT("HOT OR COLD"), S)) * .5f, 78), S, Yellow);
    S.Height = 30; S.Stroke = 5.f;
    HallDrawText(Img, TEXT("25 C"), FVector2D((W - HallTextWidth(TEXT("25 C"), S)) * .5f, 124), S, White);
    S.Height = 13; S.Stroke = 2.4f;
    HallDrawText(Img, TEXT("DONUTS 10 C"), FVector2D((W - HallTextWidth(TEXT("DONUTS 10 C"), S)) * .5f, 152), S, Yellow);
}

// ------------------------------------------------------------------ props
struct FBuild
{
    FWriter S, L, Gl;   // solid, litter, glow
    FRng R;
    FBuild(FHallMesh& Solid, FHallMesh& Litter, FHallMesh& Glow) : S(Solid), L(Litter), Gl(Glow), R(0xC0FFEEu) {}

    // --- fences
    void PostAt(const FVector& P)
    {
        S.Box(TPost, P + FVector(-2, -2, -40), P + FVector(2, 2, 152));
        S.Box(TPost, P + FVector(-1, -4.5, -40), P + FVector(1, 4.5, 152));
        S.Box(TPost, P + FVector(-7, -1, -22), P + FVector(7, 1, -8));   // the spade
    }
    /** Snow fence section between A and B: two wires and lath slats; Mode 0 standing, 1 leaning out, 2 down flat. */
    void FenceSection(const FVector& A, const FVector& B, int32 Mode, double OutSign)
    {
        const FVector D = (B - A).GetSafeNormal2D();
        const FVector Out = FVector(-D.Y, D.X, 0) * OutSign;
        const double Len = Dist2D(A, B);
        const double Tilt = Mode == 0 ? R.Range(-3, 3) : Mode == 1 ? R.Range(22, 34) : 84;
        const double T = Tilt * Pi / 180.0;
        const FVector Up = FVector::UpVector * FMath::Cos(T) + Out * FMath::Sin(T);
        FWriter& W = Mode == 2 ? L : S;
        for (double U = 6; U < Len - 3; U += 9.2)
        {
            const double Lean = R.Range(-2.5, 2.5) * Pi / 180.0;
            const FVector UpS = (Up + D * FMath::Sin(Lean)).GetSafeNormal();
            const FVector Base = A + D * U + FVector(0, 0, 2 + R.Range(0, 3));
            W.BoxAx(Slat, Base + UpS * 60, D, FVector::CrossProduct(UpS, D), UpS, FVector(1.9, .6, 60));
        }
        for (double Zw : { 32.0, 104.0 })
        {
            const FVector WA = A + Up * Zw + FVector(0, 0, 2), WB = B + Up * Zw + FVector(0, 0, 2);
            W.Tube(Wire, WA - Out * .9, WB - Out * .9, .35, 4);
        }
    }
    void Fences()
    {
        // Two runs round the lawn, broken at the gate in the west fence (toward the drive and the side door) and at the wide
        // hayride gate in the south fence (toward the street). The slats hang on the inside of the posts, wired on the outside.
        const TArray<TArray<FVector>> Runs = {
            { FVector(FX0, GateY0, G), FVector(FX0, FY0, G), FVector(HayGateX0, FY0, G) },
            { FVector(HayGateX1, FY0, G), FVector(FX1, FY0, G), FVector(FX1, FY1, G), FVector(FX0, FY1, G), FVector(FX0, GateY1, G) } };
        int32 Section = 0;
        for (const TArray<FVector>& C : Runs)
            for (int32 E = 0; E + 1 < C.Num(); ++E)
            {
                const FVector A = C[E], B = C[E + 1];
                const int32 N = FMath::Max(1, FMath::RoundToInt(Dist2D(A, B) / 245.0));
                const FVector D = (B - A) / N;
                for (int32 K = 0; K < N; ++K)
                {
                    const FVector PA = A + D * K, PB = A + D * (K + 1);
                    if (K == 0) PostAt(PA);
                    PostAt(PB);
                    // A few sections down (trampled flat on the street side and east of the stage, leaning by the gate and on
                    // the side-street side) and the rest standing.
                    const int32 Mode = Section == 15 || Section == 42 ? 2 : Section == 4 || Section == 31 ? 1 : 0;
                    FenceSection(PA, PB, Mode, 1.0);
                    ++Section;
                }
            }
        // The gate panels, unhooked and left just inside the gaps: the west one leaning, the hayride pair leaning and flat.
        FenceSection(FVector(FX0 + 30, GateY1 + 30, G), FVector(FX0 + 180, GateY1 + 30, G), 1, 1.0);
        FenceSection(FVector(HayGateX0 + 40, FY0 + 40, G), FVector(HayGateX0 + 240, FY0 + 30, G), 1, 1.0);
        FenceSection(FVector(HayGateX1 - 60, FY0 + 120, G), FVector(HayGateX1 + 120, FY0 + 200, G), 2, 1.0);
    }
    void CrowdBarricade(const FVector& C, double Yaw, bool bDown)
    {
        // 2 m galvanised panel: top and bottom rails, uprights, two flat feet. Down = on its face, flat on the grass.
        auto P = [&](double LX, double LY, double LZ) { return bDown ? Yawed(C, Yaw, LX, LZ * .98 + LY * .2, 2 + LY * .1 + (110 - LZ) * .02) : Yawed(C, Yaw, LX, LY, LZ); };
        FWriter& W = S;
        W.Tube(Barricade, P(-100, 0, 108), P(100, 0, 108), 2.2, 6);
        W.Tube(Barricade, P(-100, 0, 22), P(100, 0, 22), 2.2, 6);
        for (double X = -100; X <= 100.5; X += 14.3) W.Tube(Barricade, P(X, 0, 22), P(X, 0, 108), 1.0, 5);
        for (double X : { -100.0, 100.0 })
        {
            W.Tube(Barricade, P(X, 0, 2), P(X, 0, 110), 2.2, 6);
            W.Tube(Barricade, P(X, -32, 2), P(X, 32, 2), 2.0, 6);
        }
    }

    // --- stage
    void Stage()
    {
        const double PostX[] = { SX0, SCX, SX1 }, PostY[] = { SY0, SY1 };
        for (double X : PostX) for (double Y : PostY) S.Tube(Scaffold, FVector(X, Y, G - 20), FVector(X, Y, SZ - 2), 2.4, 8);
        for (double Y : PostY) for (double Z : { G + 12, SZ - 6 }) S.Tube(Scaffold, FVector(SX0, Y, Z), FVector(SX1, Y, Z), 2.0, 6);
        for (double X : PostX) for (double Z : { G + 12, SZ - 6 }) S.Tube(Scaffold, FVector(X, SY0, Z), FVector(X, SY1, Z), 2.0, 6);
        S.Tube(Scaffold, FVector(SX0, SY0, G + 12), FVector(SCX, SY0, SZ - 6), 1.6, 6);
        S.Tube(Scaffold, FVector(SX1, SY0, G + 12), FVector(SCX, SY0, SZ - 6), 1.6, 6);
        // Plywood sheets, 122 x 244, with the seams showing.
        for (int32 Col = 0; Col < 7; ++Col)
            for (int32 Row = 0; Row < 2; ++Row)
            {
                const double X0 = SX0 + Col * 122, Y0 = SY0 + Row * 244;
                S.Box(Plywood, FVector(X0 + .8, Y0 + .8, SZ - 2), FVector(X0 + 121.2, Y0 + 243.2, SZ));
            }
        // The black skirt: hung on the front and sides, one length pulled down and lying on the grass.
        S.Box(Skirt, FVector(SX0 - 1, SY0 - 2, G + 1), FVector(SX1 - 190, SY0 - .5, SZ - 1));
        S.Box(Skirt, FVector(SX0 - 2, SY0 - 2, G + 1), FVector(SX0 - .5, SY1, SZ - 1));
        S.Box(Skirt, FVector(SX1 + .5, SY0 - 2, G + 1), FVector(SX1 + 2, SY1, SZ - 1));
        L.Sheet(Skirt, FVector(SX1 - 190, SY0 - 4, G + 1.5), FVector(SX1 + 10, SY0 - 4, G + 1.5), FVector(SX1 + 30, SY0 - 150, G + 1.5), FVector(SX1 - 160, SY0 - 120, G + 1.5));
        // Steps at the west end (two risers of 33 and the deck).
        S.Box(Plywood, FVector(SX0 - 40, -800, G - 10), FVector(SX0, -700, SZ - 33));
        S.Box(Plywood, FVector(SX0 - 80, -800, G - 10), FVector(SX0 - 40, -700, SZ - 66));
        for (double X : { SX0 - 40, SX0 - 80 }) for (double Y : { -800.0, -700.0 }) S.Tube(Scaffold, FVector(X, Y, G - 10), FVector(X, Y, SZ - (X < SX0 - 60 ? 68 : 35)), 1.8, 6);
        // Backdrop: two scaffold uprights, a top ledger and the painted sheets, with crepe streamers from the ledger ends.
        for (double X : { SX0 + 20, SX1 - 20 }) S.Tube(Scaffold, FVector(X, SY1 - 6, SZ), FVector(X, SY1 - 6, SZ + 300), 2.4, 8);
        S.Tube(Scaffold, FVector(SX0 + 20, SY1 - 6, SZ + 296), FVector(SX1 - 20, SY1 - 6, SZ + 296), 2.0, 6);
        S.Tube(Scaffold, FVector(SX0 + 20, SY1 - 6, SZ + 140), FVector(SX1 - 20, SY1 - 6, SZ + 140), 2.0, 6);
        const double BX0 = SCX - 244, BX1 = SCX + 244, BZ0 = SZ + 150, BZ1 = SZ + 272, BY = SY1 - 8;
        S.Box(Plywood, FVector(BX0, BY - 2, BZ0), FVector(BX1, BY, BZ1));
        // Seen from the field (looking +y) the viewer's right is -x, so u runs from the high-x end.
        const FVector2D UV[4] = { FVector2D(1, 1), FVector2D(0, 1), FVector2D(0, 0), FVector2D(1, 0) };
        S.QuadUV(Backdrop, FVector(BX0, BY - 2.2, BZ0), FVector(BX1, BY - 2.2, BZ0), FVector(BX1, BY - 2.2, BZ1), FVector(BX0, BY - 2.2, BZ1), FVector(0, -1, 0), UV);
        Streamer(FVector(SX0 + 20, SY1 - 10, SZ + 296), 150, 0);
        Streamer(FVector(SX1 - 20, SY1 - 10, SZ + 296), 120, 1);
        // PA stacks on the front corners, wedges at the lip, the drum riser.
        for (double X : { SX0 + 10, SX1 - 70 })
        {
            S.Box(Speaker, FVector(X, SY0 + 4, SZ), FVector(X + 60, SY0 + 60, SZ + 62));
            S.Box(Speaker, FVector(X + 3, SY0 + 8, SZ + 62), FVector(X + 57, SY0 + 56, SZ + 134));
            S.Box(Grille, FVector(X + 5, SY0 + 3, SZ + 6), FVector(X + 55, SY0 + 4, SZ + 56));
            S.Box(Grille, FVector(X + 8, SY0 + 7, SZ + 66), FVector(X + 52, SY0 + 8, SZ + 100));
            S.Box(Black, FVector(X + 14, SY0 + 6, SZ + 104), FVector(X + 46, SY0 + 9, SZ + 126));
        }
        for (double X : { SX0 + 200, SX1 - 230 }) S.BoxYaw(Speaker, FVector(X, SY0 + 22, SZ + 15), FVector(26, 18, 15), 0, 38);
        S.Box(Plywood, FVector(SCX - 120, SY1 - 180, SZ), FVector(SCX + 120, SY1 - 10, SZ + 20));
    }
    void Streamer(const FVector& Top, double Len, int32 Kind)
    {
        // Twisted crepe strip hanging from a point, drifting a little in the direction it was blown.
        const FName K = Kind ? Streamer1 : Streamer0;
        const int32 N = 7;
        FVector P = Top;
        for (int32 I = 0; I < N; ++I)
        {
            const double A0 = I * .9, A1 = (I + 1) * .9;
            const FVector Q = P + FVector(R.Range(-6, 6), R.Range(-6, 6), -Len / N);
            const FVector W0(FMath::Cos(A0) * 1.6, FMath::Sin(A0) * 1.6, 0), W1(FMath::Cos(A1) * 1.6, FMath::Sin(A1) * 1.6, 0);
            L.Sheet(K, P - W0, P + W0, Q + W1, Q - W1);
            P = Q;
        }
    }

    // --- backline
    void DrumKit(const FVector& C)
    {
        // C is the centre of the riser top. Kick along y, snare left, hi-hat far left, toms over the kick, floor tom right.
        const FVector Kick(C.X, C.Y - 40, C.Z + 29);
        S.Cone(Shell, Kick + FVector(0, 20, 0), Kick + FVector(0, -20, 0), 29, 29, 20, false, false);
        S.Disc(Head, Kick + FVector(0, -20.2, 0), FVector(0, -1, 0), 29, 20);
        S.Disc(Head, Kick + FVector(0, 20.2, 0), FVector(0, 1, 0), 29, 20);
        for (double Y : { -20.0, 20.0 }) S.Cone(Chrome, Kick + FVector(0, Y - 1.5, 0), Kick + FVector(0, Y + 1.5, 0), 30.5, 30.5, 20, false, false);
        for (double X : { -34.0, 34.0 }) S.Tube(Chrome, Kick + FVector(X * .7, -12, -10), FVector(Kick.X + X, Kick.Y - 22, C.Z), 1.2, 5);
        L.Box(Chrome, Kick + FVector(-4, 18, -29), Kick + FVector(4, 30, -26));   // pedal
        // Rack toms over the kick, tilted toward the player.
        for (double X : { -17.0, 17.0 })
        {
            const double Rr = X < 0 ? 14 : 16;
            const FVector Top(Kick.X + X, Kick.Y + 6, C.Z + 86 + (X < 0 ? 0 : 2));
            const FVector Axis = FVector(0, .3, 1).GetSafeNormal();
            S.Cone(Shell, Top - Axis * 22, Top, Rr, Rr, 16, true, false);
            S.Disc(Head, Top + Axis * .2, Axis, Rr, 16);
            S.Tube(Chrome, Kick + FVector(0, 0, 29), Top - Axis * 22 + FVector(0, 0, -2), 1.3, 5);
        }
        // Snare on its stand, floor tom on legs.
        const FVector Snare(C.X - 52, C.Y - 70, C.Z + 72);
        S.Cone(Chrome, Snare, Snare + FVector(0, 0, 14), 18, 18, 18, false, false);
        S.Disc(Head, Snare + FVector(0, 0, 14.2), FVector::UpVector, 18, 18);
        S.Disc(Head, Snare - FVector(0, 0, .2), -FVector::UpVector, 18, 18);
        S.Tube(Chrome, FVector(Snare.X, Snare.Y, C.Z), Snare + FVector(0, 0, 4), 1.4, 5);
        for (int32 I = 0; I < 3; ++I) { const double A = I * 2.09; S.Tube(Chrome, FVector(Snare.X, Snare.Y, C.Z + 30), FVector(Snare.X + FMath::Cos(A) * 22, Snare.Y + FMath::Sin(A) * 22, C.Z), 1.0, 4); }
        const FVector Floor(C.X + 62, C.Y - 60, C.Z + 32);
        S.Cone(Shell, Floor, Floor + FVector(0, 0, 36), 20, 20, 18, false, false);
        S.Disc(Head, Floor + FVector(0, 0, 36.2), FVector::UpVector, 20, 18);
        for (int32 I = 0; I < 3; ++I) { const double A = I * 2.09 + .5; S.Tube(Chrome, Floor + FVector(FMath::Cos(A) * 19, FMath::Sin(A) * 19, 20), FVector(Floor.X + FMath::Cos(A) * 24, Floor.Y + FMath::Sin(A) * 24, C.Z), 1.0, 4); }
        // Hi-hat, crash and ride on stands; the throne behind the kick.
        auto Stand = [&](const FVector& Foot, double H) { L.Tube(Chrome, Foot, Foot + FVector(0, 0, H), 1.3, 5); for (int32 I = 0; I < 3; ++I) { const double A = I * 2.09 + .3; L.Tube(Chrome, Foot + FVector(0, 0, 26), Foot + FVector(FMath::Cos(A) * 24, FMath::Sin(A) * 24, 0), 1.0, 4); } };
        const FVector HH(C.X - 92, C.Y - 40, C.Z);
        Stand(HH, 112);
        L.Disc(Cymbal, HH + FVector(0, 0, 110), FVector::UpVector, 17, 16); L.Disc(Cymbal, HH + FVector(0, 0, 109.5), -FVector::UpVector, 17, 16);
        L.Disc(Cymbal, HH + FVector(0, 0, 114), FVector::UpVector, 17, 16); L.Disc(Cymbal, HH + FVector(0, 0, 113.5), -FVector::UpVector, 17, 16);
        L.Box(Chrome, HH + FVector(-4, -30, 0), HH + FVector(4, -6, 3));
        const FVector Crash(C.X - 70, C.Y + 10, C.Z);
        Stand(Crash, 136);
        const FVector CrN = FVector(.25, -.2, 1).GetSafeNormal();
        L.Disc(Cymbal, Crash + FVector(0, 0, 136), CrN, 21, 18); L.Disc(Cymbal, Crash + FVector(0, 0, 135.6), -CrN, 21, 18);
        const FVector Ride(C.X + 76, C.Y + 6, C.Z);
        Stand(Ride, 118);
        const FVector RdN = FVector(-.2, -.2, 1).GetSafeNormal();
        L.Disc(Cymbal, Ride + FVector(0, 0, 118), RdN, 26, 18); L.Disc(Cymbal, Ride + FVector(0, 0, 117.6), -RdN, 26, 18);
        const FVector Throne(C.X, C.Y + 48, C.Z);
        for (int32 I = 0; I < 3; ++I) { const double A = I * 2.09 + 1.; S.Tube(Chrome, Throne + FVector(0, 0, 40), Throne + FVector(FMath::Cos(A) * 20, FMath::Sin(A) * 20, 0), 1.2, 4); }
        S.Tube(Chrome, Throne + FVector(0, 0, 30), Throne + FVector(0, 0, 52), 1.6, 5);
        S.Cone(Black, Throne + FVector(0, 0, 52), Throne + FVector(0, 0, 60), 16, 17, 14);
    }
    void ComboAmp(const FVector& C, double Yaw)
    {
        // 62 x 26 x 48, grille cloth on the upper front, chrome control strip, a strap handle.
        S.BoxYaw(Tolex, C + FVector(0, 0, 24), FVector(31, 13, 24), Yaw);
        S.BoxYaw(AmpGrille, Yawed(C, Yaw, 0, -13.6, 21), FVector(27, .5, 15), Yaw);
        S.BoxYaw(Chrome, Yawed(C, Yaw, 0, -13.4, 43), FVector(27, .6, 3), Yaw);
        S.BoxYaw(Black, Yawed(C, Yaw, 0, 0, 49.5), FVector(9, 2, 1.5), Yaw);
    }
    void BassStack(const FVector& C, double Yaw)
    {
        S.BoxYaw(Tolex, C + FVector(0, 0, 33), FVector(30, 20, 33), Yaw);
        S.BoxYaw(Grille, Yawed(C, Yaw, 0, -20.6, 33), FVector(27, .5, 30), Yaw);
        for (double X : { -14.0, 14.0 }) for (double Z : { 19.0, 47.0 }) S.BoxYaw(Chrome, Yawed(C, Yaw, X, -21.3, Z), FVector(.6, .4, .6), Yaw);
        S.BoxYaw(Tolex, C + FVector(0, 0, 73), FVector(30, 15, 7), Yaw);
        S.BoxYaw(Chrome, Yawed(C, Yaw, 0, -15.6, 73), FVector(28, .5, 5), Yaw);
        for (int32 K = 0; K < 6; ++K) L.BoxYaw(Black, Yawed(C, Yaw, -20 + K * 8, -16.4, 73), FVector(1.6, .6, 1.6), Yaw);
    }
    /** Electric guitar or bass, body centre at C, neck pointing along local +y, lying back by Tilt degrees about local x. */
    void Guitar(const FVector& C, double Yaw, double Tilt, bool bBass, FName Finish)
    {
        const double Y = Yaw * Pi / 180.0, T = Tilt * Pi / 180.0;
        const FVector U(FMath::Cos(Y), FMath::Sin(Y), 0), Yv(-FMath::Sin(Y), FMath::Cos(Y), 0);
        const FVector V = Yv * FMath::Cos(T) + FVector::UpVector * FMath::Sin(T), N = FVector::UpVector * FMath::Cos(T) - Yv * FMath::Sin(T);
        const double Sc = bBass ? 1.08 : 1.0;
        const TArray<FVector2D> Body = { FVector2D(0, -20), FVector2D(9, -19.5), FVector2D(16, -15), FVector2D(19.5, -7), FVector2D(18, 2), FVector2D(14, 7), FVector2D(13.5, 11),
            FVector2D(17, 15), FVector2D(19, 20), FVector2D(16, 26), FVector2D(11, 29), FVector2D(8, 24), FVector2D(5, 21), FVector2D(0, 20),
            FVector2D(-5, 21), FVector2D(-8, 24), FVector2D(-11, 29), FVector2D(-16, 26), FVector2D(-19, 20), FVector2D(-17, 15), FVector2D(-13.5, 11),
            FVector2D(-14, 7), FVector2D(-18, 2), FVector2D(-19.5, -7), FVector2D(-16, -15), FVector2D(-9, -19.5) };
        TArray<FVector2D> Scaled; for (const FVector2D& P : Body) Scaled.Add(P * Sc);
        L.Extrude(Finish, Scaled, 4.5, C - N * 2.25, U, V, N);
        const double NeckL = bBass ? 62 : 46, Top = 4.5 * .5;
        L.BoxAx(Neck, C + V * (20 + NeckL * .5) + N * (Top + 1.0), U, V, N, FVector(2.6, NeckL * .5, 1.0));
        L.BoxAx(Black, C + V * (20 + NeckL + 8) + N * (Top + .8), U, V, N, FVector(4, 8, .7));
        for (int32 K = 0; K < (bBass ? 4 : 6); ++K)
        {
            const double X = (bBass ? -3.0 + K * 2.0 : -2.5 + K * 1.0);
            L.BoxAx(Strings, C + U * X + V * (6 + NeckL * .5) + N * (Top + 2.6), U, V, N, FVector(.08, 14 + NeckL * .5, .08));
        }
        L.BoxAx(Black, C + V * 2 + N * (Top + .8), U, V, N, FVector(4, 1.6, .8));
        L.BoxAx(Black, C + V * 10 + N * (Top + .8), U, V, N, FVector(4, 1.6, .8));
        L.BoxAx(Chrome, C + V * (-8) + N * (Top + .9), U, V, N, FVector(4.5, 1.2, .9));
        for (double X : { 12.0, 16.0 }) L.Cone(Black, C + U * X + V * (-10) + N * Top, C + U * X + V * (-10) + N * (Top + 1.8), 1.6, 1.6, 8);
    }
    void GuitarStand(const FVector& Foot, double Yaw)
    {
        // A-frame tube stand; the guitar on it leans back 14 degrees.
        for (double X : { -9.0, 9.0 }) L.Tube(Black, Yawed(Foot, Yaw, X * 2.2, 10, 0), Yawed(Foot, Yaw, X * .3, -8, 70), 1.0, 5);
        L.Tube(Black, Yawed(Foot, Yaw, -20, 10, 0), Yawed(Foot, Yaw, 20, 10, 0), 1.0, 5);
        for (double X : { -8.0, 8.0 }) L.Tube(Black, Yawed(Foot, Yaw, X, 10, 0), Yawed(Foot, Yaw, X, -6, 22), 1.0, 5);
        Guitar(Yawed(Foot, Yaw, 0, 2, 26), Yaw, 76, false, GuitarRed);
    }
    void Banjo(const FVector& C, double Yaw, double Tilt)
    {
        const double Y = Yaw * Pi / 180.0, T = Tilt * Pi / 180.0;
        const FVector U(FMath::Cos(Y), FMath::Sin(Y), 0), Yv(-FMath::Sin(Y), FMath::Cos(Y), 0);
        const FVector V = Yv * FMath::Cos(T) + FVector::UpVector * FMath::Sin(T), N = FVector::UpVector * FMath::Cos(T) - Yv * FMath::Sin(T);
        L.Cone(Chrome, C - N * 4, C + N * 4, 17, 17, 20, false, false);
        L.Disc(Head, C + N * 4.1, N, 17, 20); L.Disc(Barrel, C - N * 4.1, -N, 17, 20);
        L.BoxAx(Neck, C + V * (17 + 26) + N * 2.5, U, V, N, FVector(2.2, 26, 1.0));
        L.BoxAx(Black, C + V * (17 + 52 + 7) + N * 2.5, U, V, N, FVector(3.5, 7, .7));
        for (int32 K = 0; K < 5; ++K) L.BoxAx(Strings, C + U * (-2 + K) + V * 30 + N * 5.2, U, V, N, FVector(.08, 40, .08));
    }
    void Fiddle(const FVector& C, double Yaw)
    {
        const double Y = Yaw * Pi / 180.0;
        const FVector U(FMath::Cos(Y), FMath::Sin(Y), 0), V(-FMath::Sin(Y), FMath::Cos(Y), 0), N = FVector::UpVector;
        const TArray<FVector2D> Body = { FVector2D(0, -18), FVector2D(7, -17), FVector2D(11, -11), FVector2D(9, -5), FVector2D(7, -1), FVector2D(7, 3), FVector2D(9, 7), FVector2D(10, 12), FVector2D(6, 17), FVector2D(0, 18),
            FVector2D(-6, 17), FVector2D(-10, 12), FVector2D(-9, 7), FVector2D(-7, 3), FVector2D(-7, -1), FVector2D(-9, -5), FVector2D(-11, -11), FVector2D(-7, -17) };
        L.Extrude(Sunburst, Body, 3.5, C, U, V, N);
        L.BoxAx(Neck, C + V * 29 + N * 3, U, V, N, FVector(1.2, 11, .7));
        L.Sphere(Neck, C + V * 41 + N * 3.2, 1.8);
        for (int32 K = 0; K < 4; ++K) L.BoxAx(Strings, C + U * (-1.2 + K * .8) + V * 14 + N * 4.6, U, V, N, FVector(.06, 25, .06));
        L.BoxAx(Black, C + V * 2 + N * 4.2, U, V, N, FVector(2.5, .6, .9));
    }
    void Keyboard(const FVector& C, double Yaw)
    {
        // X-stand of square tube, the keyboard on it: black body, a strip of white keys, the black keys in their groups.
        for (double Sgn : { -1.0, 1.0 })
        {
            L.Tube(Black, Yawed(C, Yaw, -40 * Sgn, -22, 0), Yawed(C, Yaw, 40 * Sgn, 14, 86), 1.3, 5);
            L.Tube(Black, Yawed(C, Yaw, -40 * Sgn, 22, 0), Yawed(C, Yaw, 40 * Sgn, -14, 86), 1.3, 5);
        }
        S.BoxYaw(Black, C + FVector(0, 0, 91), FVector(66, 16, 4.5), Yaw);
        S.BoxYaw(KeyWhite, Yawed(C, Yaw, 0, -8.5, 96.2), FVector(60, 7, .8), Yaw);
        for (int32 Oct = 0; Oct < 5; ++Oct)
            for (int32 K : { 0, 1, 3, 4, 5 })
            {
                const double X = -60 + (Oct * 7 + K + .67) * (120.0 / 35.0);
                L.BoxYaw(Black, Yawed(C, Yaw, X, -5.5, 97.4), FVector(.75, 4, .7), Yaw);
            }
        for (int32 K = 0; K < 8; ++K) L.BoxYaw(Fader, Yawed(C, Yaw, -50 + K * 6, 6, 96), FVector(.8, .8, .6), Yaw);
    }
    void MicStand(const FVector& Foot, double Yaw, bool bDown)
    {
        if (bDown)
        {
            // Knocked over: the base on its side, the tube and boom along the deck.
            L.Cone(Black, Foot + FVector(0, 0, 7), Yawed(Foot, Yaw, 2, 0, 7), 14, 14, 14);
            L.Tube(Chrome, Yawed(Foot, Yaw, 2, 0, 7), Yawed(Foot, Yaw, 110, 0, 2), 1.2, 5);
            L.Tube(Chrome, Yawed(Foot, Yaw, 110, 0, 2), Yawed(Foot, Yaw, 160, 30, 2), 1.0, 5);
            L.Cone(Black, Yawed(Foot, Yaw, 160, 30, 2), Yawed(Foot, Yaw, 176, 40, 2), 1.6, 2.4, 8);
            return;
        }
        L.Cone(Black, Foot, Foot + FVector(0, 0, 2.5), 15, 14, 14);
        L.Tube(Chrome, Foot + FVector(0, 0, 2.5), Foot + FVector(0, 0, 150), 1.2, 5);
        const FVector Tip = Yawed(Foot, Yaw, 0, -52, 174);
        L.Tube(Chrome, Foot + FVector(0, 0, 150), Tip, 1.0, 5);
        L.Cone(Black, Tip, Yawed(Foot, Yaw, 0, -68, 180), 1.6, 2.4, 8);
        L.Cone(Chrome, Yawed(Foot, Yaw, 0, -68, 180), Yawed(Foot, Yaw, 0, -73, 182), 2.4, 1.8, 8);
    }
    void CableOnDeck(const FVector& From, int32 Pieces, double Z)
    {
        TArray<FVector> P; FVector Q = From; double A = R.Range(0, 2 * Pi);
        P.Add(FVector(Q.X, Q.Y, Z));
        for (int32 I = 0; I < Pieces; ++I)
        {
            A += R.Range(-1.1, 1.1);
            Q += FVector(FMath::Cos(A), FMath::Sin(A), 0) * R.Range(25, 60);
            Q.X = FMath::Clamp(Q.X, (double)SX0 + 20, (double)SX1 - 20); Q.Y = FMath::Clamp(Q.Y, (double)SY0 + 20, (double)SY1 - 40);
            P.Add(FVector(Q.X, Q.Y, Z));
        }
        L.Path(Cable, P, .9, 4);
    }
    void Backline()
    {
        const FVector Riser(SCX, SY1 - 95, SZ + 20);
        DrumKit(Riser);
        ComboAmp(FVector(SX0 + 140, SY1 - 60, SZ), 12);
        BassStack(FVector(SX1 - 160, SY1 - 70, SZ), -8);
        Banjo(FVector(SX0 + 118, SY1 - 94, SZ + 24), 95, 62);
        Guitar(FVector(SX1 - 196, SY1 - 110, SZ + 30), -75, 68, true, Sunburst);
        GuitarStand(FVector(SX0 + 230, SY0 + 190, SZ), 160);
        Keyboard(FVector(SX0 + 260, SY1 - 160, SZ), -20);
        MicStand(FVector(SX0 + 150, SY0 + 70, SZ), 0, false);
        MicStand(FVector(SCX, SY0 + 60, SZ), 0, false);
        MicStand(FVector(SX1 - 150, SY0 + 80, SZ), 25, true);
        // The open guitar case by the amp, the acoustic still in it; a cooler and cups on the deck.
        {
            const FVector Cs(SX0 + 60, SY1 - 230, SZ);
            S.BoxYaw(Black, Cs + FVector(0, 0, 6), FVector(52, 20, 6), 70);
            L.BoxYaw(Plush, Cs + FVector(0, 0, 12.2), FVector(50, 18, .3), 70);
            S.BoxYaw(Black, Yawed(Cs, 70, 0, 20, 36), FVector(52, 5.5, 20), 70, 70);
            const double Y = 70 * Pi / 180.0;
            const FVector U(FMath::Cos(Y), FMath::Sin(Y), 0), V(-FMath::Sin(Y), FMath::Cos(Y), 0);
            const TArray<FVector2D> Dread = { FVector2D(0, -24), FVector2D(12, -23), FVector2D(19, -16), FVector2D(19, -4), FVector2D(14, 2), FVector2D(13, 6), FVector2D(16, 12), FVector2D(14, 19), FVector2D(8, 23), FVector2D(0, 24),
                FVector2D(-8, 23), FVector2D(-14, 19), FVector2D(-16, 12), FVector2D(-13, 6), FVector2D(-14, 2), FVector2D(-19, -4), FVector2D(-19, -16), FVector2D(-12, -23) };
            L.Extrude(Spruce, Dread, 9, Cs + U * (-14) + FVector(0, 0, 12.5), U, V, FVector::UpVector);
            L.Cone(Black, Cs + U * (-14) + FVector(0, 0, 21.6), Cs + U * (-14) + FVector(0, 0, 21.7), 4.5, 4.5, 12);
            L.BoxAx(Neck, Cs + U * 20 + FVector(0, 0, 17), U, V, FVector::UpVector, FVector(22, 2.4, 1.0));
        }
        S.Box(Cooler, FVector(SX1 - 120, SY1 - 60, SZ), FVector(SX1 - 60, SY1 - 24, SZ + 38));
        S.Box(CoolerLid, FVector(SX1 - 122, SY1 - 62, SZ + 38), FVector(SX1 - 58, SY1 - 22, SZ + 44));
        for (int32 I = 0; I < 12; ++I) CableOnDeck(FVector(R.Range(SX0 + 60, SX1 - 60), R.Range(SY0 + 40, SY1 - 60), 0), 5, SZ + .8);
        // The snake from the stage lip down to the mixer table.
        L.Path(Cable, { FVector(SCX + 30, SY0 + 10, SZ + 1), FVector(SCX + 30, SY0 - 2, SZ), FVector(SCX + 34, SY0 - 6, G + 2), FVector(SCX + 60, SY0 - 300, G + 1.5), FVector(SCX + 20, SY0 - 600, G + 1.5), FVector(SCX - 20, SY0 - 790, G + 1.5) }, 2.0, 6);
    }

    // --- the field
    void FoldingTable(const FVector& C, double Yaw, bool bCollapsed)
    {
        if (bCollapsed)
        {
            S.BoxYaw(TableTop, C + FVector(0, 0, 9), FVector(91, 38, 2), Yaw, 7);
            for (double X : { -70.0, 70.0 }) L.BoxYaw(TableLeg, Yawed(C, Yaw, X, 0, 5), FVector(1.5, 30, 1.5), Yaw);
            return;
        }
        S.BoxYaw(TableTop, C + FVector(0, 0, 74), FVector(91, 38, 2), Yaw);
        for (double X : { -78.0, 78.0 }) for (double Y : { -28.0, 28.0 }) S.Tube(TableLeg, Yawed(C, Yaw, X, Y, 0), Yawed(C, Yaw, X * .9, Y, 72), 1.4, 5);
    }
    void FoldingChair(const FVector& C, double Yaw, int32 Mode)
    {
        // Brown steel folding chair. Mode 0 standing, 1 on its back, 2 on its side.
        auto P = [&](double LX, double LY, double LZ)
        {
            if (Mode == 1) return Yawed(C, Yaw, LX, -LZ * .9 + LY * .3, 3 + LY * .9 + LZ * .1);
            if (Mode == 2) return Yawed(C, Yaw, LZ * .95 + LX * .2, LY, 3 + LX * .9);
            return Yawed(C, Yaw, LX, LY, LZ);
        };
        FWriter& W = Mode == 0 ? S : L;
        auto Bx = [&](double LX, double LY, double LZ, double HX, double HY, double HZ)
        {
            const FVector Ctr = P(LX, LY, LZ), AX = (P(LX + 1, LY, LZ) - Ctr).GetSafeNormal(), AY = (P(LX, LY + 1, LZ) - Ctr).GetSafeNormal(), AZ = (P(LX, LY, LZ + 1) - Ctr).GetSafeNormal();
            W.BoxAx(Chair, Ctr, AX, AY, AZ, FVector(HX, HY, HZ));
        };
        Bx(0, 0, 45, 21, 20, 1.2);          // seat
        Bx(0, 19, 86, 21, 1.2, 14);         // back
        for (double X : { -19.0, 19.0 })
        {
            W.Tube(Chair, P(X, 18, 0), P(X, 16, 100), 1.0, 4);
            W.Tube(Chair, P(X, -18, 0), P(X, -16, 46), 1.0, 4);
        }
        W.Tube(Chair, P(-19, -18, 2), P(19, -18, 2), 1.0, 4);
        W.Tube(Chair, P(-19, 18, 2), P(19, 18, 2), 1.0, 4);
    }
    void HayBale(const FVector& C, double Yaw, int32 Mode)
    {
        // 90 x 45 x 38, two twine bands. Mode 0 flat, 1 on its end, 2 on its side.
        const FVector H = Mode == 0 ? FVector(45, 22.5, 19) : Mode == 1 ? FVector(19, 22.5, 45) : FVector(45, 19, 22.5);
        const FVector Ctr = C + FVector(0, 0, H.Z);
        S.BoxYaw(Straw, Ctr, H, Yaw);
        for (double Off : { -1.0, 1.0 })
        {
            const double Along = Off * H.X * .36;
            const FVector A = Yawed(Ctr, Yaw, Along, 0, 0);
            const FVector X(FMath::Cos(Yaw * Pi / 180), FMath::Sin(Yaw * Pi / 180), 0), Y(-X.Y, X.X, 0);
            L.BoxAx(Twine, A, X, Y, FVector::UpVector, FVector(.8, H.Y + .4, H.Z + .4));
        }
    }
    void PumpkinAt(const FVector& C, double Rr, double Hh, FName K, double Yaw, double Squash = 1.0)
    {
        const int32 Rings = 9, Seg = 28;
        FHallMesh& Mesh = L.M;
        FHallSection& Sec = Mesh.Sec(K);
        const int32 Base = Sec.V.Num();
        const double Y = Yaw * Pi / 180.0;
        for (int32 I = 0; I <= Rings; ++I)
        {
            const double T = (double)I / Rings;
            const double Prof = FMath::Sqrt(FMath::Max(0.0, 1 - FMath::Square(2 * T - 1))) * (1 - .12 * FMath::Square(FMath::Max(0.0, T - .75) * 4));
            for (int32 Sg = 0; Sg <= Seg; ++Sg)
            {
                const double A = 2 * Pi * Sg / Seg + Y;
                const double Rib = 1 - .07 * FMath::Cos(10 * (A - Y));
                const double Rad = FMath::Max(.3, Rr * Prof * Rib);
                const FVector P(C.X + Rad * FMath::Cos(A), C.Y + Rad * FMath::Sin(A), C.Z + Hh * T * Squash);
                FVector N(FMath::Cos(A) * (1 + .4 * FMath::Cos(10 * (A - Y))), FMath::Sin(A) * (1 + .4 * FMath::Cos(10 * (A - Y))), (2 * T - 1) * 1.1 * Rr / FMath::Max(1.0, Hh));
                N = N.GetSafeNormal();
                Sec.V.Add(P); Sec.N.Add(N); Sec.UV.Add(FVector2D((double)Sg / Seg * 2, T)); Sec.T.Add(FProcMeshTangent(FVector(-FMath::Sin(A), FMath::Cos(A), 0), false));
            }
        }
        for (int32 I = 0; I < Rings; ++I)
            for (int32 Sg = 0; Sg < Seg; ++Sg)
            {
                const int32 A = Base + I * (Seg + 1) + Sg, B = A + 1, Cc = A + Seg + 1, D = Cc + 1;
                Sec.I.Append({ A, D, B, A, Cc, D });
            }
        L.Cone(Stem, FVector(C.X, C.Y, C.Z + Hh * Squash - 1), FVector(C.X + 2, C.Y + 1, C.Z + Hh * Squash + 7), 2.6, 1.6, 7, false, true);
    }
    void CornShock(const FVector& C)
    {
        for (int32 I = 0; I < 13; ++I)
        {
            const double A = R.Range(0, 2 * Pi), Rb = R.Range(2, 11), Rt = R.Range(28, 52), H = R.Range(200, 245);
            const FVector Foot(C.X + Rb * FMath::Cos(A), C.Y + Rb * FMath::Sin(A), C.Z), Top(C.X + Rt * FMath::Cos(A), C.Y + Rt * FMath::Sin(A), C.Z + H);
            L.Cone(Corn, Foot, Top, 1.7, .5, 5, false, false);
            for (int32 Lf = 0; Lf < 2; ++Lf)
            {
                const double F = R.Range(.45, .8), LA = A + R.Range(-1.2, 1.2), Len = R.Range(40, 70);
                const FVector At = Foot + (Top - Foot) * F;
                const FVector Tip = At + FVector(FMath::Cos(LA) * Len, FMath::Sin(LA) * Len, -Len * .45);
                const FVector Side(-FMath::Sin(LA) * 3.5, FMath::Cos(LA) * 3.5, 0);
                L.Sheet(CornLeaf, At - Side, At + Side, Tip + Side * .3, Tip - Side * .3);
            }
        }
        L.Cone(Twine, C + FVector(0, 0, 118), C + FVector(0, 0, 123), 14, 14, 12, false, false);
    }
    void Scarecrow(const FVector& Foot, double Yaw)
    {
        S.BoxYaw(Timber, Foot + FVector(0, 0, 100), FVector(4.5, 4.5, 130), Yaw);
        S.BoxYaw(Timber, Foot + FVector(0, 0, 185), FVector(78, 3, 3), Yaw);
        S.BoxYaw(Flannel, Foot + FVector(0, 0, 157), FVector(20, 11, 28), Yaw);
        for (double Sgn : { -1.0, 1.0 })
        {
            S.BoxYaw(Flannel, Yawed(Foot, Yaw, Sgn * 48, 0, 185), FVector(29, 7, 7), Yaw);
            for (int32 I = 0; I < 5; ++I) L.Cone(Straw, Yawed(Foot, Yaw, Sgn * 77, R.Range(-3, 3), 185 + R.Range(-3, 3)), Yawed(Foot, Yaw, Sgn * (84 + R.Range(0, 8)), R.Range(-6, 6), 180 + R.Range(-6, 6)), 1.2, .4, 4, false, false);
            S.BoxYaw(Denim, Yawed(Foot, Yaw, Sgn * 9, 0, 92), FVector(8, 8, 38), Yaw, Sgn * 3);
            for (int32 I = 0; I < 5; ++I) L.Cone(Straw, Yawed(Foot, Yaw, Sgn * 9 + R.Range(-4, 4), R.Range(-4, 4), 54), Yawed(Foot, Yaw, Sgn * 9 + R.Range(-8, 8), R.Range(-8, 8), 44), 1.2, .4, 4, false, false);
        }
        L.Sphere(Burlap, Foot + FVector(0, 0, 210), 14, .95, 12);
        L.BoxYaw(Black, Yawed(Foot, Yaw, -5, -13.5, 213), FVector(2, .5, 2), Yaw);
        L.BoxYaw(Black, Yawed(Foot, Yaw, 5, -13.5, 213), FVector(2, .5, 2), Yaw);
        L.BoxYaw(Black, Yawed(Foot, Yaw, 0, -13.6, 204), FVector(5, .4, .6), Yaw);
        L.Cone(Hat, Foot + FVector(0, 0, 221), Foot + FVector(0, 0, 222.5), 26, 26, 16);
        L.Cone(Hat, Foot + FVector(0, 0, 222.5), Foot + FVector(0, 0, 236), 15, 12, 14);
        for (int32 I = 0; I < 8; ++I) L.Cone(Straw, Foot + FVector(R.Range(-4, 4), R.Range(-4, 4), 196), Foot + FVector(R.Range(-14, 14), R.Range(-14, 14), 186 + R.Range(0, 6)), 1.0, .4, 4, false, false);
    }
    void CiderStand(const FVector& C)
    {
        // The barrel on a pair of crates, tap to the front (-y); chalkboard A-frame beside it; apple crates.
        for (double X : { -24.0, 24.0 }) S.Box(Crate, C + FVector(X - 18, -24, 0), C + FVector(X + 18, 24, 34));
        const FVector A(C.X - 35, C.Y, C.Z + 34 + 30), B(C.X + 35, C.Y, C.Z + 64);
        S.Lathe(Barrel, A, FVector(1, 0, 0), { FVector2D(0, 0), FVector2D(25, 0), FVector2D(28, 14), FVector2D(30, 35), FVector2D(28, 56), FVector2D(25, 70), FVector2D(0, 70) }, 18);
        for (double X : { 8.0, 35.0, 62.0 }) S.Cone(Hoop, A + FVector(X - 1.5, 0, 0), A + FVector(X + 1.5, 0, 0), 30.4 - FMath::Abs(X - 35) * .11, 30.4 - FMath::Abs(X - 35) * .11, 18, false, false);
        L.Cone(Hoop, FVector(C.X + 36, C.Y, C.Z + 46), FVector(C.X + 44, C.Y, C.Z + 46), 1.6, 1.6, 8);
        L.Box(Hoop, FVector(C.X + 42, C.Y - .8, C.Z + 46), FVector(C.X + 44, C.Y + .8, C.Z + 53));
        (void)B;
        // Chalkboard A-frame.
        const FVector Sg(C.X - 110, C.Y + 10, C.Z);
        S.BoxYaw(Crate, Sg + FVector(0, 12, 38), FVector(27, 1.2, 38), 0, -18);
        S.BoxYaw(Crate, Sg + FVector(0, -12, 38), FVector(27, 1.2, 38), 0, 18);
        const double Tl = 18 * Pi / 180.0;
        const FVector Nf = FVector(0, -FMath::Cos(Tl), FMath::Sin(Tl)), Vf = FVector(0, FMath::Sin(Tl), FMath::Cos(Tl));
        const FVector Ctr = Sg + FVector(0, -12, 38) + Nf * 1.3;
        const FVector2D UV[4] = { FVector2D(1, 1), FVector2D(0, 1), FVector2D(0, 0), FVector2D(1, 0) };   // faces -y: u from the high-x end
        S.QuadUV(Chalk, Ctr - FVector(25, 0, 0) - Vf * 35, Ctr + FVector(25, 0, 0) - Vf * 35, Ctr + FVector(25, 0, 0) + Vf * 35, Ctr - FVector(25, 0, 0) + Vf * 35, Nf, UV);
        // Apple crates, one tipped and spilling.
        for (int32 I = 0; I < 3; ++I)
        {
            const FVector Cc = C + FVector(90 + I * 56, -30 + (I % 2) * 40, 0);
            const double Yaw = R.Range(-15, 15);
            S.BoxYaw(Crate, Cc + FVector(0, 0, 14), FVector(25, 18, 14), Yaw);
            L.BoxYaw(Barrel, Cc + FVector(0, 0, 26.5), FVector(23, 16, .6), Yaw);
            for (int32 K = 0; K < 14; ++K) L.Sphere(Apple, Yawed(Cc, Yaw, R.Range(-19, 19), R.Range(-12, 12), 29 + R.Range(0, 4)), 3.8, 1, 8);
        }
        for (int32 K = 0; K < 9; ++K) L.Sphere(Apple, C + FVector(R.Range(60, 260), R.Range(-110, 90), 3.8), 3.8, 1, 8);
        for (int32 K = 0; K < 2; ++K) S.Cone(Plate, C + FVector(-60 + K * 14, 36, 34), C + FVector(-60 + K * 14, 36, 34 + 14), 4, 4.4, 10, false, false);
    }
    // --- the rest of the lawn
    /** Chemical toilet: a tall box with a capped roof and skid base, the door on local +x (open: swung out on its hinge), a vent stack. */
    void PortaJohn(const FVector& C, double Yaw, bool bOpen)
    {
        S.BoxYaw(Potty, C + FVector(0, 0, 112), FVector(56, 56, 112), Yaw);
        S.BoxYaw(PottyRoof, C + FVector(0, 0, 228), FVector(60, 60, 5), Yaw);
        S.BoxYaw(PottyRoof, C + FVector(0, 0, 4), FVector(62, 62, 4), Yaw);
        S.Tube(Potty, Yawed(C, Yaw, -40, -40, 220), Yawed(C, Yaw, -40, -40, 262), 5, 6);
        for (double Sg : { -1.0, 1.0 }) for (double Z : { 60.0, 120.0, 180.0 }) L.BoxYaw(PottyRoof, Yawed(C, Yaw, 0, Sg * 56.6, Z), FVector(40, .6, 1.2), Yaw);
        if (bOpen)
        {
            const FVector Hinge = Yawed(C, Yaw, 56, -40, 0);
            const double DY = Yaw + 70;
            S.BoxYaw(Potty, Yawed(Hinge, DY, 0, 36, 108), FVector(1.8, 36, 100), DY);
            L.BoxYaw(Chrome, Yawed(Hinge, DY, 2.5, 62, 100), FVector(1.2, 4, 1.2), DY);
        }
        else
        {
            L.BoxYaw(PottyRoof, Yawed(C, Yaw, 56.4, 0, 108), FVector(.5, 36, 100), Yaw);
            L.BoxYaw(Chrome, Yawed(C, Yaw, 57.6, 26, 100), FVector(1.2, 4, 1.2), Yaw);
        }
    }
    /** A galvanised stock tank with two kegs standing in it, the ice long gone; the cup table beside it and the cups everywhere. */
    void KegStation(const FVector& C, double Yaw)
    {
        S.Lathe(Barricade, C, FVector::UpVector, { FVector2D(0, 0), FVector2D(60, 0), FVector2D(60, 58), FVector2D(62, 60), FVector2D(58, 60), FVector2D(58, 4), FVector2D(0, 4) }, 20);
        const TArray<FVector2D> KegProf = { FVector2D(0, 0), FVector2D(18, 0), FVector2D(20, 3), FVector2D(20, 12), FVector2D(20.6, 14), FVector2D(20, 48), FVector2D(20.6, 50), FVector2D(20, 56), FVector2D(18, 59), FVector2D(13, 59), FVector2D(13, 56), FVector2D(0, 56) };
        const FVector KegA = C + FVector(-22, 10, 4), KegB = C + FVector(24, -14, 4);
        S.Lathe(Keg, KegA, FVector::UpVector, KegProf, 16);
        S.Lathe(Keg, KegB, FVector(.14, -.06, 1).GetSafeNormal(), KegProf, 16);
        for (const FVector& Kc : { KegA, KegB })
        {
            L.Tube(Chrome, Kc + FVector(0, 0, 59), Kc + FVector(0, 0, 70), 2.2, 6);
            L.Tube(Chrome, Kc + FVector(0, 0, 70), Kc + FVector(14, 0, 74), 1.4, 5);
            L.Path(Cable, { Kc + FVector(0, 0, 70), Kc + FVector(-20, 30, 50), Kc + FVector(-50 + R.Range(-10, 10), 70, 2), Kc + FVector(-70, 110, 2) }, .7, 5);
        }
        FoldingTable(Yawed(C, Yaw, 170, 0, 0), Yaw + 90, false);
        for (int32 I = 0; I < 3; ++I)
        {
            const FVector Stack = Yawed(C, Yaw, 170 + R.Range(-20, 20), R.Range(-60, 60), 76);
            L.Cone(CupRed, Stack, Stack + FVector(0, 0, 12 + I * 6), 3.6, 4.6, 8, true, false);
        }
        for (int32 I = 0; I < 16; ++I)
        {
            const FVector P = Yawed(C, Yaw, R.Range(-120, 260), R.Range(-120, 120), 0);
            const double A = R.Range(0, 2 * Pi);
            L.Cone(CupRed, P + FVector(0, 0, 4.2), P + FVector(FMath::Cos(A) * 12, FMath::Sin(A) * 12, 4.4), 3.6, 4.6, 8, true, false);
        }
    }
    /** A 3 x 3 m pop-up canopy with a blue top; one leg buckled, so that corner sags, with the merch tables under it. */
    void Canopy(const FVector& C, double Yaw)
    {
        const double H = 215;
        const FVector Leg[4] = { Yawed(C, Yaw, -150, -150, 0), Yawed(C, Yaw, 150, -150, 0), Yawed(C, Yaw, 150, 150, 0), Yawed(C, Yaw, -150, 150, 0) };
        const FVector Knee = Yawed(C, Yaw, -140, 140, 110);
        const FVector Top[4] = { Leg[0] + FVector(0, 0, H), Leg[1] + FVector(0, 0, H), Leg[2] + FVector(0, 0, H), Yawed(C, Yaw, -110, 120, 95) };
        for (int32 I = 0; I < 4; ++I)
        {
            if (I == 3) { S.Tube(Scaffold, Leg[3], Knee, 1.8, 6); S.Tube(Scaffold, Knee, Top[3], 1.8, 6); }
            else S.Tube(Scaffold, Leg[I], Top[I], 1.8, 6);
            S.Tube(Scaffold, Top[I], Top[(I + 1) % 4], 1.4, 6);
        }
        const FVector Peak = (Top[0] + Top[1] + Top[2] + Top[3]) * .25 + FVector(0, 0, 70);
        for (int32 I = 0; I < 4; ++I)
        {
            const FVector A = Top[I], B = Top[(I + 1) % 4];
            L.Tube(Scaffold, A, Peak, 1.2, 5);
            const FVector N = FVector::CrossProduct(B - A, Peak - A).GetSafeNormal();
            L.Tri(Tarp, A, B, Peak, N); L.Tri(Tarp, A, Peak, B, -N);
            L.Sheet(Tarp, A + FVector(0, 0, -.5), B + FVector(0, 0, -.5), B + FVector(0, 0, -22), A + FVector(0, 0, -22));
        }
        FoldingTable(Yawed(C, Yaw, 30, -20, 0), Yaw + 4, false);
        FoldingTable(Yawed(C, Yaw, -50, 100, 0), Yaw - 40, true);
        FoldingChair(Yawed(C, Yaw, 100, -110, 0), Yaw + 200, 0);
        FoldingChair(Yawed(C, Yaw, -100, -70, 0), Yaw + 150, 1);
        for (int32 I = 0; I < 5; ++I)
        {
            const FVector Cp = Yawed(C, Yaw, 30 + R.Range(-80, 80), -20 + R.Range(-30, 30), 76);
            L.Cone(CupRed, Cp, Cp + FVector(0, 0, 12), 3.6, 4.6, 8, true, false);
        }
    }
    /** The portable generator that ran the PA: a red box on a tube frame with its exhaust and outlet box. */
    void Generator(const FVector& C, double Yaw)
    {
        S.BoxYaw(TractorRed, C + FVector(0, 0, 34), FVector(36, 24, 22), Yaw);
        S.BoxYaw(Black, C + FVector(0, 0, 60), FVector(20, 16, 4), Yaw);
        for (double X : { -34.0, 34.0 }) for (double Y : { -22.0, 22.0 }) S.Tube(Scaffold, Yawed(C, Yaw, X, Y, 2), Yawed(C, Yaw, X, Y, 12), 1.4, 5);
        for (double Y : { -26.0, 26.0 }) S.Tube(Scaffold, Yawed(C, Yaw, -38, Y, 62), Yawed(C, Yaw, 38, Y, 62), 1.4, 5);
        L.Tube(Black, Yawed(C, Yaw, 20, 0, 64), Yawed(C, Yaw, 20, 0, 82), 2.0, 6);
        L.BoxYaw(Mixer, Yawed(C, Yaw, -37, 8, 40), FVector(1.5, 6, 5), Yaw);
    }
    /** A tyre (ring) with a rim disc, axis along Yd, hub at the wheel centre. */
    void TyreAt(const FVector& Hub, const FVector& Yd, double Rr, double HalfW, double RimR)
    {
        S.Lathe(Tyre, Hub - Yd * HalfW, Yd, { FVector2D(RimR, 0), FVector2D(Rr - 4, 0), FVector2D(Rr, 4), FVector2D(Rr, HalfW * 2 - 4), FVector2D(Rr - 4, HalfW * 2), FVector2D(RimR, HalfW * 2) }, 18);
        S.Lathe(Rim, Hub - Yd * (HalfW - 3), Yd, { FVector2D(0, 0), FVector2D(RimR + .5, 0), FVector2D(RimR + .5, HalfW * 2 - 6), FVector2D(0, HalfW * 2 - 6) }, 12);
    }
    /** A small 1970s farm tractor in red: big rear wheels, hood, seat, roll bar, exhaust stack. Origin under the rear axle, +x forward. */
    void Tractor(const FVector& C, double Yaw)
    {
        auto P = [&](double X, double Y, double Z) { return Yawed(C, Yaw, X, Y, Z); };
        const FVector Xd(FMath::Cos(Yaw * Pi / 180), FMath::Sin(Yaw * Pi / 180), 0), Yd(-Xd.Y, Xd.X, 0);
        for (double Sg : { -1.0, 1.0 })
        {
            TyreAt(P(0, Sg * 92, 78), Yd, 78, 22, 40);
            TyreAt(P(235, Sg * 68, 36), Yd, 36, 11, 16);
            S.BoxYaw(TractorRed, P(-10, Sg * 92, 161), FVector(56, 25, 4), Yaw);        // fender top
            S.BoxYaw(TractorRed, P(-10, Sg * 69, 124), FVector(56, 2, 37), Yaw);         // fender inner plate
            S.Tube(Black, P(-22, Sg * 42, 76), P(-22, Sg * 42, 226), 3, 6);               // roll bar
        }
        S.Tube(Black, P(0, -70, 78), P(0, 70, 78), 9, 8);
        S.Tube(Black, P(235, -58, 36), P(235, 58, 36), 5, 6);
        S.BoxYaw(TractorRed, P(90, 0, 60), FVector(150, 22, 14), Yaw);                   // chassis
        S.BoxYaw(Black, P(40, 0, 76), FVector(42, 50, 2), Yaw);                          // platform
        S.BoxYaw(TractorRed, P(165, 0, 110), FVector(85, 32, 30), Yaw);                  // hood
        S.BoxYaw(Black, P(251, 0, 108), FVector(1.5, 28, 26), Yaw);                      // grille
        for (double Sg : { -1.0, 1.0 }) S.Cone(Head, P(250, Sg * 18, 124), P(253.5, Sg * 18, 124), 6, 6, 10);   // lamps
        S.BoxYaw(Black, P(0, 0, 100), FVector(20, 22, 6), Yaw);                          // seat
        S.BoxYaw(Black, P(-18, 0, 122), FVector(3, 20, 16), Yaw);
        S.Tube(Black, P(-22, -42, 226), P(-22, 42, 226), 3, 6);
        S.Tube(Black, P(80, 0, 96), P(68, 0, 126), 2.2, 6);                               // steering column
        S.Lathe(Black, P(66, 0, 126), (Xd * -.5 + FVector::UpVector).GetSafeNormal(), { FVector2D(15, 0), FVector2D(19, 0), FVector2D(19, 2.4), FVector2D(15, 2.4) }, 16);
        S.Tube(Black, P(200, 18, 140), P(200, 18, 262), 3.5, 8);                          // exhaust stack
        S.Tube(Black, P(200, 18, 150), P(200, 18, 200), 6.5, 8);                          // muffler
        S.Tube(Black, P(140, -18, 140), P(140, -18, 190), 5, 8);                          // air cleaner
        S.Bar(Black, P(-40, 0, 48), P(-84, 0, 48), 6, 4);                                 // drawbar
    }
    /** Flatbed hay wagon: plank deck on a steel frame, slatted head and tail boards, side rails, four small wheels, the tongue out front.
     *  Origin under the deck centre, +x forward; bales and loose straw on the deck. */
    void HayWagon(const FVector& C, double Yaw)
    {
        auto P = [&](double X, double Y, double Z) { return Yawed(C, Yaw, X, Y, Z); };
        const FVector Xd(FMath::Cos(Yaw * Pi / 180), FMath::Sin(Yaw * Pi / 180), 0), Yd(-Xd.Y, Xd.X, 0);
        S.BoxYaw(Timber, C + FVector(0, 0, 92), FVector(245, 110, 4), Yaw);
        for (int32 K = 1; K < 11; ++K) L.BoxYaw(Log, P(0, -110 + K * 20, 96.3), FVector(245, .4, .3), Yaw);
        for (double Sg : { -1.0, 1.0 })
        {
            S.BoxYaw(Scaffold, P(0, Sg * 96, 84), FVector(240, 3, 4), Yaw);
            for (double X : { -150.0, 150.0 }) TyreAt(P(X, Sg * 122, 34), Yd, 34, 9, 14);
            S.Tube(Timber, P(-240, Sg * 108, 150), P(240, Sg * 108, 150), 2, 6);           // side rail
            for (double X : { -240.0, -120.0, 0.0, 120.0, 240.0 }) S.Tube(Timber, P(X, Sg * 108, 96), P(X, Sg * 108, 152), 2.5, 6);
        }
        for (double X : { -150.0, 150.0 }) S.Tube(Black, P(X, -120, 34), P(X, 120, 34), 4, 6);
        for (double X : { -245.0, 245.0 })
        {
            for (double Y : { -80.0, 0.0, 80.0 }) S.Tube(Timber, P(X, Y, 96), P(X, Y, 196), 2.5, 6);
            for (int32 J = 0; J < 3; ++J) S.BoxYaw(Slat, P(X, 0, 122 + J * 30), FVector(1.6, 100, 4), Yaw);
        }
        S.Bar(Black, P(245, 0, 62), P(400, 0, 58), 7, 5);
        S.Tube(Black, P(400, -8, 58), P(400, 8, 58), 3.5, 6);
        HayBale(P(-120, -40, 96), Yaw + 5, 0); HayBale(P(-120, 50, 96), Yaw - 3, 0); HayBale(P(30, 10, 96), Yaw + 90, 0); HayBale(P(-120, 5, 134), Yaw + 2, 0);
        HayBale(P(150, -30, 96), Yaw + 80, 2);
        for (int32 I = 0; I < 9; ++I)
        {
            const FVector Q = P(R.Range(-230, 230), R.Range(-100, 100), 96.6 + R.Range(0, 1));
            const double A = R.Range(0, 360);
            L.Sheet(Straw, Yawed(Q, A, -22, -14, 0), Yawed(Q, A, 22, -12, 0), Yawed(Q, A, 20, 16, .4), Yawed(Q, A, -18, 12, .2));
        }
        for (int32 I = 0; I < 7; ++I)
        {
            const FVector Q = P(R.Range(-300, 60), R.Range(-220, 220), G - C.Z + .6);
            const double A = R.Range(0, 360);
            L.Sheet(Straw, Yawed(Q, A, -26, -16, 0), Yawed(Q, A, 26, -14, 0), Yawed(Q, A, 24, 18, .3), Yawed(Q, A, -22, 14, .2));
        }
        PumpkinAt(P(90, -60, 96), 11, 17, Pumpkin, 40);
    }
    /** The hayride, parked: the wagon at C and the tractor hitched ahead of it. */
    void Hayride(const FVector& C, double Yaw)
    {
        HayWagon(C, Yaw);
        Tractor(Yawed(C, Yaw, 492, 0, 0), Yaw);
    }
    /** The pumpkin pile for the toss, on a bed of straw: a ring, a second layer and one on top, a few rolled away. */
    void PumpkinPile(const FVector& C)
    {
        S.Cone(Straw, C, C + FVector(0, 0, 6), 112, 100, 18, false, true);
        for (int32 I = 0; I < 9; ++I)
        {
            const double A = I * 2 * Pi / 9;
            PumpkinAt(C + FVector(FMath::Cos(A) * 58, FMath::Sin(A) * 58, 5), 14 + R.Range(-2, 3), 20 + R.Range(-3, 4), I % 4 == 3 ? PalePumpkin : Pumpkin, R.Range(0, 360));
        }
        for (int32 I = 0; I < 4; ++I)
        {
            const double A = I * Pi / 2 + .4;
            PumpkinAt(C + FVector(FMath::Cos(A) * 28, FMath::Sin(A) * 28, 24), 13, 19, Pumpkin, R.Range(0, 360));
        }
        PumpkinAt(C + FVector(0, 0, 44), 12, 18, Pumpkin, 80);
        PumpkinAt(C + FVector(150, 40, 0), 12, 18, Pumpkin, 10); PumpkinAt(C + FVector(-140, 90, 0), 10, 15, Gourd, 200); PumpkinAt(C + FVector(60, -160, 0), 13, 19, PalePumpkin, 300);
    }
    /** A cornhole board: plywood 120 x 60 propped on two legs with the hole, a few bean bags about. Local +x is the high end. */
    void Cornhole(const FVector& C, double Yaw)
    {
        // The board pitches up toward its high end, so build it from BoxAx with a pitched frame.
        const double A = Yaw * Pi / 180.0, T = 11.0 * Pi / 180.0;
        const FVector X(FMath::Cos(A), FMath::Sin(A), 0), Y(-X.Y, X.X, 0);
        const FVector Xp = X * FMath::Cos(T) + FVector::UpVector * FMath::Sin(T), Zp = FVector::UpVector * FMath::Cos(T) - X * FMath::Sin(T);
        S.BoxAx(Plywood, C + Xp * 0 + Zp * 1.6 + FVector(0, 0, 12), Xp, Y, Zp, FVector(60, 30, 1.6));
        L.Disc(Black, C + Xp * 30 + Zp * 3.3 + FVector(0, 0, 12), Zp, 7.5, 14);
        for (double Yl : { -26.0, 26.0 }) S.Tube(Timber, Yawed(C, Yaw, 52, Yl, 0), C + Xp * 52 + Y * Yl + Zp * 0 + FVector(0, 0, 12), 1.6, 5);
        for (int32 I = 0; I < 3; ++I) L.BoxYaw(I % 2 ? Balloon0 : Denim, Yawed(C, Yaw, R.Range(-120, 40), R.Range(-70, 70), 2), FVector(7, 7, 2), R.Range(0, 180));
    }
    /** The painted sheet from the stage backdrop again, half size, on two stakes outside the gate facing the drive. */
    void GateSign()
    {
        const double X = FX0 - 60, Y0 = GateY0 - 300, Y1 = GateY0 - 56, Z0 = G + 110, Z1 = Z0 + 61;
        for (double Y : { Y0 + 12, Y1 - 12 }) S.Box(Timber, FVector(X - 3, Y - 3, G - 30), FVector(X + 3, Y + 3, Z1 + 10));
        S.Box(Plywood, FVector(X + 3, Y0, Z0), FVector(X + 5, Y1, Z1));
        // Faces -x: a viewer on the drive looks +x, right is +y, so u runs with y.
        const FVector2D UV[4] = { FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0) };
        S.QuadUV(Backdrop, FVector(X + 2.8, Y0, Z0), FVector(X + 2.8, Y1, Z0), FVector(X + 2.8, Y1, Z1), FVector(X + 2.8, Y0, Z1), FVector(-1, 0, 0), UV);
    }
    void FirePitAt(const FVector& C)
    {
        for (int32 I = 0; I < 15; ++I)
        {
            const double A = 2 * Pi * I / 15 + R.Range(-.1, .1), Rr = 56 + R.Range(-4, 4);
            S.BoxYaw(Stone, FVector(C.X + Rr * FMath::Cos(A), C.Y + Rr * FMath::Sin(A), C.Z + 7), FVector(11 + R.Range(-2, 3), 8, 7), A * 180 / Pi + R.Range(-20, 20));
        }
        L.Disc(Ash, C + FVector(0, 0, 1.5), FVector::UpVector, 48, 18);
        for (int32 I = 0; I < 4; ++I)
        {
            const double A = R.Range(0, 2 * Pi);
            L.Cone(Log, C + FVector(FMath::Cos(A) * -26, FMath::Sin(A) * -26, 6 + I * 2), C + FVector(FMath::Cos(A) * 30, FMath::Sin(A) * 30, 10 + I * 3), 5, 4, 7);
        }
        for (int32 I = 0; I < 9; ++I) Gl.BoxYaw(Ember, C + FVector(R.Range(-14, 14), R.Range(-14, 14), 4), FVector(3, 2, 1.2), R.Range(0, 180));
    }
    void StringLights()
    {
        // Timber poles: the four inner ones round the stage field from the first party, and an outer ring a metre inside
        // the new fence every ten metres or so, cords between neighbours and spokes in to the inner ring.
        const TArray<FVector> Poles = { FVector(5300, -2250, G), FVector(7500, -2250, G), FVector(7500, -1000, G), FVector(5300, -1000, G) };   // the north pair stands just short of the stage, so no cord crosses the sign
        const TArray<FVector> Outer = { FVector(4260, -3450, G), FVector(5230, -3450, G), FVector(6200, -3450, G), FVector(7170, -3450, G),
            FVector(8190, -3450, G), FVector(8190, -2400, G), FVector(8190, -1300, G), FVector(8190, -310, G), FVector(6800, -310, G),
            FVector(5800, -310, G), FVector(5230, -310, G), FVector(4260, -310, G), FVector(4260, -1400, G), FVector(4260, -2400, G) };
        auto Pole = [&](const FVector& P)
        {
            S.Box(Timber, P + FVector(-2, -4.5, -40), P + FVector(2, 4.5, 330));
            L.Tube(Wire, P + FVector(0, 0, 326), P + FVector(0, 60, 326), .3, 4);
            S.Box(Timber, P + FVector(-3, -3, 326), P + FVector(3, 3, 334));
        };
        for (const FVector& P : Poles) Pole(P);
        for (const FVector& P : Outer) Pole(P);
        auto Run = [&](const FVector& A, const FVector& B, bool bLive)
        {
            const double Len = Dist2D(A, B), Sag = Len * Len / 55000.0;
            const int32 N = FMath::Max(8, (int32)(Len / 90));
            TArray<FVector> P;
            for (int32 I = 0; I <= N; ++I)
            {
                const double T = (double)I / N;
                P.Add(FMath::Lerp(A, B, T) + FVector(0, 0, 328 - Sag * 4 * T * (1 - T)));
            }
            L.Path(Cord, P, .45, 4);
            const int32 Bulbs = (int32)(Len / 55);
            for (int32 I = 1; I < Bulbs; ++I)
            {
                const double T = (double)I / Bulbs;
                const FVector Q = FMath::Lerp(A, B, T) + FVector(0, 0, 328 - Sag * 4 * T * (1 - T));
                const bool bLit = bLive && !R.Chance(.12);
                if (R.Chance(.04)) continue;   // a socket with no bulb
                L.Cone(Black, Q, Q - FVector(0, 0, 3), .9, 1.3, 6, false, false);
                (bLit ? Gl : L).Lathe(bLit ? Bulb : DeadBulb, Q - FVector(0, 0, 3), -FVector::UpVector, { FVector2D(1.3, 0), FVector2D(2.6, 2), FVector2D(3.4, 4.5), FVector2D(3.1, 7), FVector2D(1.8, 8.6), FVector2D(0, 9.2) }, 6);
            }
        };
        Run(Poles[0], Poles[1], true);
        Run(Poles[1], Poles[2], true);
        // The north pair is strung to the backdrop uprights instead of across to each other: a 22 m span would sag in front of the sign.
        Run(Poles[2], FVector(SX1 - 20, SY1 - 6, G), true);
        Run(Poles[3], FVector(SX0 + 20, SY1 - 6, G), true);
        Run(Poles[3], Poles[0], true);
        Run(Poles[0], Poles[2], true);
        for (int32 I = 0; I < Outer.Num(); ++I) Run(Outer[I], Outer[(I + 1) % Outer.Num()], true);
        Run(Poles[0], Outer[1], true); Run(Poles[1], Outer[3], true); Run(Poles[2], Outer[6], true); Run(Poles[3], Outer[12], true);
        // The other diagonal came down in the night: it hangs from its pole and lies across the grass, bulbs and all.
        {
            const FVector A = Poles[1], B = Poles[3];
            TArray<FVector> P;
            const FVector Dir = (B - A).GetSafeNormal2D();
            for (int32 I = 0; I <= 10; ++I) { const double T = (double)I / 10; P.Add(A + Dir * (T * 420) + FVector(0, 0, 328 * (1 - T) * (1 - T) + 1)); }
            for (int32 I = 1; I <= 22; ++I) { const double T = (double)I / 22; P.Add(A + Dir * (420 + T * 1700) + FVector(FMath::Sin(T * 9) * 40, FMath::Cos(T * 7) * 40, 1)); }
            L.Path(Cord, P, .45, 4);
            for (int32 I = 2; I < P.Num(); I += 2)
            {
                L.Cone(Black, P[I], P[I] + FVector(1.5, 0, 0), .9, 1.3, 6, false, false);
                if (!R.Chance(.3)) L.Lathe(DeadBulb, P[I] + FVector(1.5, 0, 0), FVector(1, 0, 0), { FVector2D(1.3, 0), FVector2D(2.6, 2), FVector2D(3.4, 4.5), FVector2D(3.1, 7), FVector2D(1.8, 8.6), FVector2D(0, 9.2) }, 7);
            }
        }
        for (int32 I = 0; I < 19; ++I)
        {
            const double T = R.Range(.1, .9);
            const bool bOuter = I >= 7;
            int32 E = bOuter ? R.Int(Outer.Num()) : R.Int(3);
            if (!bOuter && E == 2) E = 3;   // no cord between the north pair
            const FVector A = bOuter ? Outer[E] : Poles[E], B = bOuter ? Outer[(E + 1) % Outer.Num()] : Poles[(E + 1) % 4];
            const double Len = Dist2D(A, B), Sag = Len * Len / 55000.0;
            Streamer(FMath::Lerp(A, B, T) + FVector(0, 0, 326 - Sag * 4 * T * (1 - T)), R.Range(90, 160), I % 2);
        }
    }
    bool OnStage(double X, double Y) const { return X > SX0 - 90 && X < SX1 + 40 && Y > SY0 - 60 && Y < SY1 + 20; }
    bool NearFire(double X, double Y) const { return Dist2D(FVector(X, Y, 0), FirePit) < 95; }
    /** Ground the big props stand on (and the police car that drove in): random litter keeps out of these. */
    bool Clear(double X, double Y) const
    {
        static const FBox2D Keep[] = {
            FBox2D(FVector2D(6720, -3480), FVector2D(7020, -2820)),   // the cruiser inside the hayride gate
            FBox2D(FVector2D(7690, -3190), FVector2D(8010, -2110)),   // tractor and wagon
            FBox2D(FVector2D(4220, -3380), FVector2D(4470, -2960)),   // the toilets
            FBox2D(FVector2D(4540, -2800), FVector2D(4940, -2580)),   // the keg tank and cup table
            FBox2D(FVector2D(5020, -3330), FVector2D(5380, -2970)),   // the canopy
            FBox2D(FVector2D(7480, -3520), FVector2D(7720, -3280)),   // the pumpkin pile
            FBox2D(FVector2D(4390, -1180), FVector2D(4560, -940)),    // the ticket table
        };
        for (const FBox2D& B : Keep) if (B.IsInside(FVector2D(X, Y))) return false;
        return !OnStage(X, Y) && !NearFire(X, Y);
    }
    FVector Spot(double Margin = 40)
    {
        for (int32 Tries = 0; Tries < 24; ++Tries)
        {
            const double X = R.Range(FX0 + Margin, FX1 - Margin), Y = R.Range(FY0 + Margin, FY1 - Margin);
            if (Clear(X, Y)) return FVector(X, Y, G);
        }
        return FVector(SCX, FY0 + 300, G);
    }
    TArray<FVector2D> DrumProfile() const
    {
        return { FVector2D(0, 0), FVector2D(29, 0), FVector2D(29, 24), FVector2D(30.5, 26), FVector2D(29, 28), FVector2D(29, 60), FVector2D(30.5, 62), FVector2D(29, 64), FVector2D(29, 88), FVector2D(27, 88), FVector2D(27, 86) };
    }
    void Litter()
    {
        // Cups, cans, plates, napkins and pizza boxes; thicker near the stage and the fire.
        for (int32 I = 0; I < 560; ++I)
        {
            FVector P = Spot(20);
            if (I % 3 == 0) P = FVector(FMath::Clamp(SCX + R.Range(-700, 700), (double)FX0 + 20, (double)FX1 - 20), FMath::Clamp(SY0 - R.Range(60, 900), (double)FY0 + 20, (double)FY1 - 20), G);
            if (!Clear(P.X, P.Y)) continue;
            const double Yaw = R.Range(0, 2 * Pi);
            if (R.Chance(.55)) L.Cone(CupRed, P, P + FVector(0, 0, 12), 3.6, 4.6, 8, true, false);
            else L.Cone(CupRed, P + FVector(0, 0, 4.2), P + FVector(FMath::Cos(Yaw) * 12, FMath::Sin(Yaw) * 12, 4.4), 3.6, 4.6, 8, true, false);
        }
        for (int32 I = 0; I < 230; ++I)
        {
            const FVector P = Spot(20);
            const double Yaw = R.Range(0, 2 * Pi);
            if (I % 7 == 0) L.BoxYaw(Can, P + FVector(0, 0, 2), FVector(3.6, 3.4, 2), Yaw * 180 / Pi);
            else if (R.Chance(.4)) L.Cone(Can, P, P + FVector(0, 0, 12.2), 3.3, 3.3, 8);
            else L.Cone(Can, P + FVector(0, 0, 3.3), P + FVector(FMath::Cos(Yaw) * 12.2, FMath::Sin(Yaw) * 12.2, 3.3), 3.3, 3.3, 8);
        }
        for (int32 I = 0; I < 60; ++I) { const FVector P = Spot(30); L.Cone(Plate, P, P + FVector(0, 0, .7), 11, 11.5, 12); }
        for (int32 I = 0; I < 56; ++I)
        {
            const FVector P = Spot(30); const double Yaw = R.Range(0, 2 * Pi);
            L.Sheet(Napkin, Yawed(P, Yaw * 180 / Pi, -7, -7, .6), Yawed(P, Yaw * 180 / Pi, 7, -8, .6), Yawed(P, Yaw * 180 / Pi, 8, 7, 1.4), Yawed(P, Yaw * 180 / Pi, -8, 8, .9));
        }
        for (int32 I = 0; I < 10; ++I)
        {
            const FVector P = Spot(60); const double Yaw = R.Range(0, 360);
            S.BoxYaw(Pizza, P + FVector(0, 0, 2.2), FVector(22, 22, 2.2), Yaw);
            if (I % 2 == 0) S.BoxYaw(Pizza, Yawed(P, Yaw, 0, 22, 4.4 + 21), FVector(22, 1, 22), Yaw, 100 - 90);
        }
        // Chairs and tables: a few still up, most knocked about.
        const FVector ChairSpots[] = { FVector(5650, -1480, G), FVector(5790, -1430, G), FVector(6950, -1420, G), FVector(7080, -1500, G), FVector(6400, -1800, G), FVector(5300, -2050, G), FVector(7600, -1200, G), FVector(6100, -2250, G), FVector(7150, -2300, G), FVector(5900, -1950, G) };
        for (int32 I = 0; I < 10; ++I) FoldingChair(ChairSpots[I], R.Range(0, 360), I % 3 == 0 ? 0 : I % 3);
        FoldingTable(FVector(5650, -2250, G), 8, false);
        FoldingTable(FVector(7450, -1700, G), -30, true);
        for (int32 I = 0; I < 7; ++I) { const FVector Cp(5650 + R.Range(-80, 80), -2250 + R.Range(-30, 30), G + 76); L.Cone(CupRed, Cp, Cp + FVector(0, 0, 12), 3.6, 4.6, 8, true, false); }
        // Trash: a drum by the gate with bags heaped on it, another on its side by the far fence, bags along the west fence.
        const TArray<FVector2D> DrumProf = DrumProfile();
        S.Lathe(Drum55, FVector(4260, -1420, G), FVector::UpVector, DrumProf, 16);
        for (int32 I = 0; I < 3; ++I) L.Sphere(TrashBag, FVector(4260 + R.Range(-14, 14), -1420 + R.Range(-14, 14), G + 88 + I * 8), 24, .6, 10);
        S.Lathe(Drum55, FVector(7700, -620, G + 29), FVector(1, .2, 0).GetSafeNormal(), DrumProf, 16);
        for (int32 I = 0; I < 9; ++I) { const FVector P(7700 + R.Range(70, 160), -620 + R.Range(-60, 60), G); if (I % 2) L.Cone(CupRed, P + FVector(0, 0, 4.2), P + FVector(R.Range(-12, 12), R.Range(-12, 12), 4.4), 3.6, 4.6, 8, true, false); else L.Cone(Can, P + FVector(0, 0, 3.3), P + FVector(R.Range(-12, 12), R.Range(-12, 12), 3.3), 3.3, 3.3, 8); }
        for (int32 I = 0; I < 6; ++I) S.Sphere(TrashBag, FVector(FX0 + 40 + R.Range(-10, 10), -2050 - I * 48 + R.Range(-8, 8), G + 14), 26 + R.Range(-4, 4), .6, 10);
        // Balloons: deflated on strings at the gate posts, a few half-full on the grass.
        for (int32 I = 0; I < 4; ++I)
        {
            const FVector Post = FVector(FX0, I < 2 ? GateY0 : GateY1, G + 150) + FVector(0, (I % 2 ? 6 : -6), 0);
            const FVector Bl = Post + FVector(R.Range(-10, 10), R.Range(-10, 10), -40 - R.Range(0, 30));
            L.Tube(Wire, Post, Bl, .2, 3);
            L.Sphere(I % 2 ? Balloon0 : Balloon1, Bl, 7, .35, 8);
        }
        for (int32 I = 0; I < 7; ++I) { const FVector P = Spot(50); L.Sphere(I % 2 ? Balloon0 : Balloon1, P + FVector(0, 0, 6), 9, .7, 10); }
        // A sweater over the fence, a lost sneaker, a frisbee.
        L.BoxYaw(Sweater, FVector(FX0, -2100, G + 118), FVector(2.5, 24, 14), 0);
        L.BoxYaw(Sweater, FVector(FX0, -2100, G + 128), FVector(5, 24, 2), 0);
        L.BoxYaw(Shoe, FVector(6480, -1240, G + 5), FVector(14, 5, 3.5), 40); L.BoxYaw(Sole, FVector(6480, -1240, G + 1.4), FVector(14.5, 5.3, 1.4), 40);
        L.Cone(Frisbee, FVector(7020, -1150, G), FVector(7020, -1150, G + 2.4), 13, 12.5, 14);
    }
    void Field()
    {
        // Hay-bale seating in two arcs facing the stage, and a ring round the fire; pumpkins on some.
        const double Arc1[] = { 5500, 5650, 5950, 6250, 6560, 6880, 7060 }, Arc2[] = { 5420, 5760, 6020, 6620, 6960 };
        for (int32 I = 0; I < 7; ++I) HayBale(FVector(Arc1[I], -1250 + (I - 3) * (I - 3) * 9, G), R.Range(-14, 14), 0);
        HayBale(FVector(5650, -1250 + 4 * 9, G + 38), R.Range(-6, 6), 0);
        for (int32 I = 0; I < 5; ++I) HayBale(FVector(Arc2[I], -1540 + FMath::Abs(I - 2) * 14, G), R.Range(-20, 20), I == 2 ? 1 : 0);
        for (int32 I = 0; I < 5; ++I)
        {
            const double A = -.3 + I * 1.25;
            HayBale(FirePit + FVector(FMath::Cos(A) * 150, FMath::Sin(A) * 150, 0), A * 180 / Pi + 90 + R.Range(-10, 10), I == 3 ? 2 : 0);
        }
        FirePitAt(FirePit);
        PumpkinAt(FVector(5950, -1250, G + 38), 13, 20, Pumpkin, 20);
        PumpkinAt(FVector(6880, -1250 + 9, G + 38), 11, 17, PalePumpkin, 70);
        PumpkinAt(FVector(5760, -1526, G + 38), 9, 14, Gourd, 120);
        PumpkinAt(FVector(SX0 - 60, SY0 - 40, G), 17, 26, Pumpkin, 0);
        PumpkinAt(FVector(SX0 - 20, SY0 - 70, G), 12, 18, Pumpkin, 40);
        PumpkinAt(FVector(SX1 + 40, SY0 - 50, G), 16, 25, Pumpkin, 200);
        PumpkinAt(FVector(SX1 + 10, SY0 - 85, G), 10, 15, PalePumpkin, 300);
        PumpkinAt(FVector(5440, -2140, G), 14, 22, Pumpkin, 10);
        PumpkinAt(FVector(5110, -1680, G), 15, 24, Pumpkin, 90);
        PumpkinAt(FVector(5150, -1180, G), 12, 18, Gourd, 180);
        // The one that got thrown: flat, with the pieces round it.
        PumpkinAt(FVector(6700, -1700, G), 18, 24, Pumpkin, 0, .35);
        for (int32 I = 0; I < 7; ++I) L.BoxYaw(Pumpkin, FVector(6700 + R.Range(-45, 45), -1700 + R.Range(-45, 45), G + 2), FVector(R.Range(3, 7), R.Range(2, 5), 1.5), R.Range(0, 180));
        CornShock(FVector(SX0 - 70, SY0 - 10, G)); CornShock(FVector(SX1 + 70, SY0 - 10, G));
        CornShock(FVector(FX0 + 60, GateY0 - 50, G)); CornShock(FVector(FX0 + 60, GateY1 + 50, G));
        CornShock(FVector(5600, -2330, G));
        Scarecrow(FVector(5160, -1750, G), 100);
        CiderStand(FVector(5500, -2150, G));
        // Crowd barricades across the stage front, one knocked over.
        for (int32 I = 0; I < 4; ++I) CrowdBarricade(FVector(SCX - 330 + I * 220, SY0 - 150 + R.Range(-8, 8), G), R.Range(-6, 6), I == 2);
        // Mixer table out front with the board, a chair and a cooler.
        FoldingTable(FVector(SCX - 20, -1720, G), 2, false);
        S.Box(Mixer, FVector(SCX - 55, -1740, G + 76), FVector(SCX + 15, -1700, G + 83));
        for (int32 K = 0; K < 12; ++K) L.Box(Fader, FVector(SCX - 50 + K * 5.5, -1726 + (K % 3) * 4, G + 83), FVector(SCX - 48.5 + K * 5.5, -1724.5 + (K % 3) * 4, G + 84.2));
        FoldingChair(FVector(SCX - 20, -1650, G), 180, 0);
        S.Box(Cooler, FVector(SCX + 30, -1750, G), FVector(SCX + 90, -1714, G + 38));
        S.Box(CoolerLid, FVector(SCX + 28, -1752, G + 38), FVector(SCX + 92, -1712, G + 44));
        Fiddle(FVector(6560, -1250 + 9, G + 38.5), 25);

        // --- the rest of the lawn. The gate: ticket table and chair with the cash box, a chute of barricades (one knocked
        // over), the sign on stakes outside, corn either side of the gap (above).
        FoldingTable(FVector(4450, -1060, G), 90, false);
        FoldingChair(FVector(4540, -1060, G), 180, 0);
        S.Box(Mixer, FVector(4428, -1010, G + 76), FVector(4472, -978, G + 90));
        for (int32 I = 0; I < 2; ++I) CrowdBarricade(FVector(4330, I ? -870 : -1250, G), 0, false);
        CrowdBarricade(FVector(4620, -1270, G), 25, true);
        GateSign();
        // Facilities along the west fence: three chemical toilets (one door open), the keg tank with its cup table, the merch canopy.
        PortaJohn(FVector(4300, -3300, G), 0, false); PortaJohn(FVector(4300, -3170, G), 0, true); PortaJohn(FVector(4300, -3040, G), 0, false);
        KegStation(FVector(4650, -2700, G), 10);
        Canopy(FVector(5200, -3150, G), 5);
        // The hayride parked along the east fence, straw trailing off the deck; the pumpkin pile for the toss by the wide gate.
        Hayride(FVector(7850, -2900, G), 90);
        PumpkinPile(FVector(7600, -3400, G));
        // Seating spread over the new ground: a back arc of bales facing the stage, pairs and a stack elsewhere, pumpkins on some.
        const double Back[] = { 4900, 5260, 5620, 5980, 6340, 6700 };
        for (int32 I = 0; I < 6; ++I) HayBale(FVector(Back[I], -2850 + (I - 2.5) * (I - 2.5) * 12, G), R.Range(-18, 18), I == 4 ? 1 : 0);
        const FVector Pairs[] = { FVector(4700, -3000, G), FVector(4790, -2940, G), FVector(7900, -2000, G), FVector(7980, -2100, G), FVector(7800, -900, G),
            FVector(7900, -980, G), FVector(5000, -500, G), FVector(5100, -540, G), FVector(7500, -500, G), FVector(7580, -560, G), FVector(6000, -3300, G),
            FVector(6100, -3330, G), FVector(4500, -1500, G), FVector(5500, -3400, G), FVector(8100, -1500, G) };
        for (const FVector& Pb : Pairs) HayBale(Pb, R.Range(0, 360), 0);
        HayBale(FVector(4600, -2000, G), 0, 0); HayBale(FVector(4690, -2000, G), 0, 0); HayBale(FVector(4645, -2000, G + 38), 0, 0);
        PumpkinAt(FVector(4790, -2940, G + 38), 12, 18, Pumpkin, 30); PumpkinAt(FVector(7900, -2000, G + 38), 10, 16, PalePumpkin, 200);
        PumpkinAt(FVector(5620, -2850 + 3, G + 38), 13, 20, Pumpkin, 90); PumpkinAt(FVector(4420, -2300, G), 15, 23, Pumpkin, 0);
        PumpkinAt(FVector(8150, -1620, G), 11, 17, Gourd, 60); PumpkinAt(FVector(6500, -3450, G), 14, 21, Pumpkin, 300);
        PumpkinAt(FVector(4645, -2000, G + 76), 11, 16, Pumpkin, 150);
        // Corn shocks at the far corners and the hayride gate.
        CornShock(FVector(FX0 + 90, FY0 + 90, G)); CornShock(FVector(FX1 - 90, FY0 + 90, G)); CornShock(FVector(FX1 - 90, FY1 - 90, G)); CornShock(FVector(FX0 + 90, FY1 - 90, G));
        CornShock(FVector(HayGateX0 - 60, FY0 + 50, G)); CornShock(FVector(HayGateX1 + 60, FY0 + 50, G));
        // The generator behind the stage, its cable up to the PA.
        Generator(FVector(7000, -330, G), 20);
        L.Path(Cable, { FVector(6964, -322, G + 40), FVector(6900, -380, G + 2), FVector(6760, -430, G + 2), FVector(6740, -450, SZ + 2), FVector(SX1 - 40, SY0 + 30, SZ + 2) }, .8, 5);
        // More chairs and tables out on the new ground, two more trash drums.
        const FVector MoreChairs[] = { FVector(4900, -3350, G), FVector(5600, -3450, G), FVector(6300, -2900, G), FVector(7700, -1600, G), FVector(8000, -600, G), FVector(4500, -600, G), FVector(5300, -2500, G), FVector(7300, -3100, G) };
        for (int32 I = 0; I < 8; ++I) FoldingChair(MoreChairs[I], R.Range(0, 360), I % 3);
        FoldingTable(FVector(6500, -3350, G), 95, false);
        FoldingTable(FVector(5700, -3250, G), 40, true);
        S.Lathe(Drum55, FVector(7320, -3470, G), FVector::UpVector, DrumProfile(), 16);
        for (int32 I = 0; I < 2; ++I) L.Sphere(TrashBag, FVector(7320 + R.Range(-12, 12), -3470 + R.Range(-12, 12), G + 88 + I * 8), 24, .6, 10);
        S.Lathe(Drum55, FVector(8220, -1200, G), FVector::UpVector, DrumProfile(), 16);
        // The quieter corners: a pizza table with its chairs, more bale seating, the cornhole boards facing each other.
        FoldingTable(FVector(5000, -1400, G), 15, false);
        for (int32 I = 0; I < 3; ++I) S.BoxYaw(Pizza, Yawed(FVector(5000, -1400, G + 76 + 2.2 + I * 4.4), 15, R.Range(-20, 20), R.Range(-8, 8), 0), FVector(22, 22, 2.2), 15 + R.Range(-12, 12));
        FoldingChair(FVector(4900, -1300, G), 150, 0); FoldingChair(FVector(5110, -1480, G), 320, 2);
        HayBale(FVector(5300, -1000, G), 20, 0); HayBale(FVector(5400, -950, G), 5, 0);
        HayBale(FVector(7700, -1350, G), 0, 0); HayBale(FVector(7790, -1350, G), 0, 0); HayBale(FVector(7745, -1350, G + 38), 0, 0);
        HayBale(FVector(8000, -2500, G), 70, 0); HayBale(FVector(5700, -600, G), 100, 2);
        Cornhole(FVector(7000, -1150, G), 0); Cornhole(FVector(7700, -1150, G), 180);
    }
};
}

namespace HouseFallJam
{
using namespace HouseFallJamImpl;

bool Inside(float X, float Y) { return X > ZX0 && X < ZX1 && Y > ZY0 && Y < ZY1; }

void CreateLook(UHouseBlackHallSubsystem* L)
{
    if (!L || L->Materials.Contains(Straw)) return;
    auto Plain = [&](FName Key, const FLinearColor& C, float Rough, float Metal = 0.f, const FLinearColor& Em = FLinearColor::Black) { HallMakePlain(L, Key, C, Rough, Metal, Em); };
    auto Tex = [&](FName Key, const FHallImage& Img, const TArray<float>* Height, float Strength, float Rough, bool bWrap)
    {
        UTexture2D* T = HallMakeTexture(Img, true, false, *FString::Printf(TEXT("T_%s"), *Key.ToString()), bWrap);
        UTexture2D* N = nullptr;
        if (Height)
        {
            FHallImage NImg; HallHeightToNormal(*Height, Img.W, Img.H, Strength, bWrap, NImg);
            N = HallMakeTexture(NImg, false, true, *FString::Printf(TEXT("T_%sN"), *Key.ToString()), bWrap);
            if (N) L->Textures.Add(FName(*FString::Printf(TEXT("T_%sN"), *Key.ToString())), N);
        }
        if (T) L->Textures.Add(FName(*FString::Printf(TEXT("T_%s"), *Key.ToString())), T);
        HallMakeTextured(L, Key, T, N, Rough, FLinearColor::White, N ? 1.f : 0.f);
    };
    FHallImage Img; TArray<float> Height;
    PaintStraw(Img, Height); Tex(Straw, Img, &Height, 1.8f, .95f, true);
    PaintPlywood(Img); Tex(Plywood, Img, nullptr, 0.f, .8f, true);
    PaintFlannel(Img); Tex(Flannel, Img, nullptr, 0.f, .9f, true);
    PaintBackdrop(Img); Tex(Backdrop, Img, nullptr, 0.f, .75f, false);
    PaintChalk(Img); Tex(Chalk, Img, nullptr, 0.f, .9f, false);
    Plain(Twine, FLinearColor(.5f, .4f, .2f), .9f);
    Plain(Scaffold, FLinearColor(.35f, .35f, .36f), .5f, .7f);
    Plain(Skirt, FLinearColor(.02f, .02f, .02f), .9f);
    Plain(Speaker, FLinearColor(.03f, .03f, .03f), .95f);
    Plain(Grille, FLinearColor(.07f, .07f, .07f), .85f);
    Plain(Tolex, FLinearColor(.022f, .02f, .02f), .7f);
    Plain(AmpGrille, FLinearColor(.32f, .22f, .13f), .9f);
    Plain(Chrome, FLinearColor(.75f, .75f, .77f), .25f, 1.f);
    Plain(Shell, FLinearColor(.42f, .04f, .03f), .3f, .15f);
    Plain(Head, FLinearColor(.82f, .80f, .74f), .6f);
    Plain(Cymbal, FLinearColor(.72f, .52f, .22f), .35f, .9f);
    Plain(Black, FLinearColor(.02f, .02f, .02f), .5f);
    Plain(Cable, FLinearColor(.015f, .015f, .015f), .6f);
    Plain(GuitarRed, FLinearColor(.55f, .05f, .03f), .2f);
    Plain(Sunburst, FLinearColor(.38f, .17f, .04f), .25f);
    Plain(Neck, FLinearColor(.46f, .30f, .14f), .6f);
    Plain(Spruce, FLinearColor(.72f, .58f, .33f), .45f);
    Plain(Plush, FLinearColor(.28f, .03f, .04f), .95f);
    Plain(Strings, FLinearColor(.7f, .7f, .68f), .4f, .8f);
    Plain(Pumpkin, FLinearColor(.85f, .30f, .03f), .45f);
    Plain(PalePumpkin, FLinearColor(.75f, .60f, .36f), .5f);
    Plain(Gourd, FLinearColor(.25f, .36f, .10f), .5f);
    Plain(Stem, FLinearColor(.30f, .27f, .12f), .8f);
    Plain(Corn, FLinearColor(.56f, .45f, .22f), .9f);
    Plain(CornLeaf, FLinearColor(.50f, .42f, .20f), .9f);
    Plain(Denim, FLinearColor(.14f, .19f, .34f), .9f);
    Plain(Burlap, FLinearColor(.55f, .45f, .28f), .95f);
    Plain(Hat, FLinearColor(.66f, .55f, .30f), .9f);
    Plain(Barrel, FLinearColor(.28f, .18f, .10f), .8f);
    Plain(Hoop, FLinearColor(.12f, .11f, .10f), .5f, .7f);
    Plain(Crate, FLinearColor(.50f, .40f, .25f), .85f);
    Plain(Apple, FLinearColor(.60f, .08f, .05f), .3f);
    Plain(Bulb, FLinearColor(.95f, .8f, .55f), .3f, 0.f, FLinearColor(24.f, 14.f, 5.f));
    Plain(DeadBulb, FLinearColor(.4f, .38f, .34f), .2f);
    Plain(Cord, FLinearColor(.02f, .02f, .02f), .7f);
    Plain(Streamer0, FLinearColor(.72f, .28f, .04f), .9f);
    Plain(Streamer1, FLinearColor(.35f, .14f, .04f), .9f);
    Plain(Balloon0, FLinearColor(.55f, .05f, .05f), .35f);
    Plain(Balloon1, FLinearColor(.72f, .36f, .03f), .35f);
    Plain(TrashBag, FLinearColor(.03f, .03f, .03f), .45f);
    Plain(Drum55, FLinearColor(.10f, .22f, .38f), .6f, .4f);
    Plain(Plate, FLinearColor(.82f, .82f, .78f), .7f);
    Plain(Pizza, FLinearColor(.58f, .44f, .24f), .9f);
    Plain(Slat, FLinearColor(.48f, .33f, .16f), .9f);
    Plain(TPost, FLinearColor(.10f, .20f, .10f), .6f, .4f);
    Plain(Barricade, FLinearColor(.5f, .5f, .52f), .4f, .8f);
    Plain(TableTop, FLinearColor(.62f, .60f, .52f), .6f);
    Plain(TableLeg, FLinearColor(.35f, .35f, .36f), .5f, .7f);
    Plain(Chair, FLinearColor(.26f, .14f, .08f), .5f, .4f);
    Plain(Mixer, FLinearColor(.12f, .12f, .13f), .5f, .3f);
    Plain(Fader, FLinearColor(.85f, .85f, .82f), .5f);
    Plain(Ember, FLinearColor(.9f, .25f, .05f), .8f, 0.f, FLinearColor(7.f, 1.6f, .25f));
    Plain(Ash, FLinearColor(.30f, .29f, .28f), .95f);
    Plain(Log, FLinearColor(.04f, .035f, .03f), .9f);
    Plain(Stone, FLinearColor(.40f, .38f, .35f), .85f);
    Plain(KeyWhite, FLinearColor(.85f, .85f, .82f), .4f);
    Plain(Sweater, FLinearColor(.18f, .28f, .50f), .95f);
    Plain(Shoe, FLinearColor(.80f, .80f, .78f), .6f);
    Plain(Sole, FLinearColor(.08f, .08f, .08f), .8f);
    Plain(Frisbee, FLinearColor(.88f, .50f, .05f), .4f);
    Plain(Cooler, FLinearColor(.74f, .74f, .71f), .45f);
    Plain(CoolerLid, FLinearColor(.45f, .03f, .025f), .4f);
    Plain(Wire, FLinearColor(.30f, .30f, .30f), .5f, .8f);
    Plain(Timber, FLinearColor(.45f, .34f, .20f), .9f);
    Plain(Napkin, FLinearColor(.85f, .85f, .82f), .9f);
    Plain(Potty, FLinearColor(.40f, .60f, .56f), .6f);
    Plain(PottyRoof, FLinearColor(.78f, .80f, .78f), .6f);
    Plain(Keg, FLinearColor(.62f, .63f, .65f), .35f, .85f);
    Plain(TractorRed, FLinearColor(.55f, .07f, .04f), .5f, .1f);
    Plain(Rim, FLinearColor(.80f, .74f, .55f), .5f, .2f);
    Plain(Tyre, FLinearColor(.03f, .03f, .03f), .9f);
    Plain(Tarp, FLinearColor(.07f, .12f, .32f), .9f);
}

void Build(FHallMesh& Solid, FHallMesh& Litter, FHallMesh& Glow)
{
    FBuild B(Solid, Litter, Glow);
    B.Fences();
    B.Stage();
    B.Backline();
    B.Field();
    B.StringLights();
    B.Litter();
}
}
