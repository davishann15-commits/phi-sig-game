// Procedural classic cars for the house exterior. See HouseClassicCars.h.
// The body is a loft of cross-sections along the car (+X forward); each section is a closed loop whose shape comes from
// smooth profile curves (roof line, belt line, plan width, tumblehome, crown). Details (bumpers, grilles, lamps, trim,
// mirrors, wheels) are added on top. Units are real centimetres; the parked instances are scaled to the house.
#include "HouseClassicCars.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace ExClassicCarsImpl
{
enum EK : int32 { K_Paint, K_Paint2, K_Vinyl, K_Glass, K_Chrome, K_Trim, K_Dark, K_Tire, K_White, K_Rim, K_Mag, K_WheelPaint, K_Lens, K_Tail, K_Amber, K_Plate, K_Wood, K_WoodTrim, K_Disc, K_Shadow, K_ShadowEdge, K_LightRed, K_LightBlue, K_Count };
enum ESeg : uint8 { S_U, S_WL, S_B, S_G, S_C, S_T, S_R, S_I, S_F };
enum EGrille : int32 { G_Egg, G_Vert, G_Horiz, G_Mesh, G_Slats, G_Black };
enum EWheelKind : int32 { W_Hubcap, W_DogDish, W_Mag, W_Plastic };
enum EMirror : int32 { M_Round, M_Truck, M_Black, M_Body };
constexpr double CarPi = 3.14159265358979323846, CarHalfPi = CarPi / 2.0, TwoPi = CarPi * 2.0;
}

FName ExCarSlot(int32 Index)
{
    static const FName Names[ExClassicCarsImpl::K_Count] = {
        TEXT("CAR_Paint"), TEXT("CAR_Paint2"), TEXT("CAR_Vinyl"), TEXT("CAR_Glass"), TEXT("CAR_Chrome"), TEXT("CAR_Trim"), TEXT("CAR_Dark"),
        TEXT("CAR_Tire"), TEXT("CAR_Whitewall"), TEXT("CAR_Rim"), TEXT("CAR_Mag"), TEXT("CAR_WheelPaint"), TEXT("CAR_Lens"), TEXT("CAR_Tail"),
        TEXT("CAR_Amber"), TEXT("CAR_Plate"), TEXT("CAR_Wood"), TEXT("CAR_WoodTrim"), TEXT("CAR_Disc"), TEXT("CAR_Shadow"), TEXT("CAR_ShadowEdge"),
        TEXT("CAR_LightRed"), TEXT("CAR_LightBlue") };
    return Names[FMath::Clamp(Index, 0, (int32)ExClassicCarsImpl::K_Count - 1)];
}

namespace ExClassicCarsImpl
{
// ------------------------------------------------------------------ smooth curve through control points
struct FExCurve
{
    TArray<double> X, V, ML, MR;
    FExCurve() {}
    /** Flat triples: x, value, crease (1 = one-sided tangents, a deliberate kink). */
    static FExCurve T3(std::initializer_list<double> Flat)
    {
        TArray<FVector> P; const TArray<double> F(Flat);
        for (int32 K = 0; K + 2 < F.Num(); K += 3) P.Add(FVector(F[K], F[K + 1], F[K + 2]));
        FExCurve C; C.Init(P); return C;
    }
    /** Flat pairs: x, value. */
    static FExCurve T2(std::initializer_list<double> Flat)
    {
        TArray<FVector> P; const TArray<double> F(Flat);
        for (int32 K = 0; K + 1 < F.Num(); K += 2) P.Add(FVector(F[K], F[K + 1], 0));
        FExCurve C; C.Init(P); return C;
    }
    void Init(TArray<FVector> P)
    {
        P.Sort([](const FVector& A, const FVector& B) { return A.X < B.X; });
        const int32 N = P.Num();
        X.SetNum(N); V.SetNum(N); ML.SetNumZeroed(N); MR.SetNumZeroed(N);
        for (int32 I = 0; I < N; ++I) { X[I] = P[I].X; V[I] = P[I].Y; }
        if (N < 2) return;
        TArray<double> S;
        for (int32 I = 0; I + 1 < N; ++I) S.Add((V[I + 1] - V[I]) / FMath::Max(1e-6, X[I + 1] - X[I]));
        for (int32 I = 0; I < N; ++I)
        {
            if (I == 0) { ML[I] = MR[I] = S[0]; continue; }
            if (I == N - 1) { ML[I] = MR[I] = S[N - 2]; continue; }
            if (P[I].Z > .5) { ML[I] = S[I - 1]; MR[I] = S[I]; continue; }
            const double A = S[I - 1], B = S[I];
            double M = 0.0;
            if (A * B > 0)
            {
                const double H0 = X[I] - X[I - 1], H1 = X[I + 1] - X[I];
                M = (A * H1 + B * H0) / (H0 + H1);
                const double Lim = 3.0 * FMath::Min(FMath::Abs(A), FMath::Abs(B));
                M = FMath::Clamp(M, -Lim, Lim);
            }
            ML[I] = MR[I] = M;
        }
    }
    double operator()(double x) const
    {
        const int32 N = X.Num();
        if (N == 0) return 0.0;
        if (N == 1 || x <= X[0]) return V[0];
        if (x >= X[N - 1]) return V[N - 1];
        int32 I = 0;
        while (X[I + 1] < x) ++I;
        const double H = X[I + 1] - X[I], T = (x - X[I]) / H, Ts = T * T, Tc = Ts * T;
        return (2 * Tc - 3 * Ts + 1) * V[I] + (Tc - 2 * Ts + T) * H * MR[I] + (-2 * Tc + 3 * Ts) * V[I + 1] + (Tc - Ts) * H * ML[I + 1];
    }
};

// ------------------------------------------------------------------ spec
struct FCarLight { bool bRound = true; double Y = 0, Z = 0, R = 8, HW = 8, HH = 5; int32 Bezel = 0; };   // bezel 0 ring, 1 square, 2 hooded
struct FCarRear { double U0, U1, Z0, Z1; int32 Key; };
struct FCarTrim { double Z0, Z1, X0, X1; int32 Key; double Off; };

struct FCarSpec
{
    EExCar Type = EExCar::Yacht;
    double L = 500, W = 190, WB = 290, FO = 100, RW = 34, TW = 21, RR = 19, WW = 0, Clear = 16, Sill = 24, Arch = 40, ArchZ = 37;
    bool bSkirt = false;
    double RF = 10, RRE = 8, ZCF = 30, ZCR = 32, RC = 6;
    FExCurve Prof, Top, Belt, WTop, HalfW, Crown;
    FVector2D WS = FVector2D::ZeroVector, RG = FVector2D::ZeroVector, SG = FVector2D::ZeroVector, RoofZone = FVector2D::ZeroVector,
        Bed = FVector2D::ZeroVector, Stripes = FVector2D::ZeroVector;
    TArray<FVector2D> Pillars;
    int32 Roof = 0;                    // 0 none, 1 vinyl, 2 second colour
    bool bLouvers = false;
    TArray<double> Seams, Handles;
    bool bChromeBumper = true; FVector2D BF = FVector2D(30, 50), BR = FVector2D(32, 52); double BD = 7, BWrap = 1.8;
    TArray<FCarLight> FLights;
    int32 Grille = G_Egg; double GZ0 = 40, GZ1 = 70, GHW = 40;
    TArray<FCarRear> RLights; double PlateZ = 50;
    TArray<FCarTrim> Trims;
    bool bWood = false; double WX0 = 0, WX1 = 0, WZ0 = 0, WZ1 = 0;
    int32 Mirror = M_Round;
    bool bOrnament = false, bAntenna = false, bRoofRack = false, bScoop = false, bRWL = false;
    TArray<double> Exhaust;
    int32 Wheel = W_Hubcap; double Steer = 0; int32 GHTrim = K_Chrome;
    FVector2D Doors = FVector2D::ZeroVector;   // x range of the body sides painted in the second colour (police doors)
    bool bPolice = false;                       // roof light bar, pillar spotlight, push bar and POLICE on the doors
};
bool Has(const FVector2D& R) { return R.X != R.Y; }
bool In(const FVector2D& R, double X) { return Has(R) && X >= FMath::Min(R.X, R.Y) && X <= FMath::Max(R.X, R.Y); }

FExCurve ProfBoxy() { return FExCurve::T2({ 0, .965, .1, .99, .45, 1.0, .85, 1.0, 1.0, .985 }); }
FExCurve ProfRound50() { return FExCurve::T2({ 0, .9, .15, .955, .45, 1.0, .8, .985, 1.0, .95 }); }
FExCurve ProfShoulder() { return FExCurve::T2({ 0, .93, .12, .97, .5, .99, .84, 1.0, .9, .988, 1.0, .955 }); }
FCarLight Round(double Y, double Z, double R, int32 Bezel = 0) { FCarLight L; L.bRound = true; L.Y = Y; L.Z = Z; L.R = R; L.Bezel = Bezel; return L; }
FCarLight Rect(double Y, double Z, double HW, double HH) { FCarLight L; L.bRound = false; L.Y = Y; L.Z = Z; L.HW = HW; L.HH = HH; return L; }

FCarSpec SpecYacht()   // 1970s full-size four-door "land yacht" with a vinyl roof
{
    FCarSpec S; S.Type = EExCar::Yacht;
    S.L = 565; S.W = 201; S.WB = 307; S.FO = 108; S.RW = 35; S.TW = 21; S.RR = 19; S.WW = .18; S.Clear = 16; S.Sill = 24; S.Arch = 40; S.ArchZ = 37;
    S.RF = 10; S.RRE = 8; S.ZCF = 30; S.ZCR = 32; S.RC = 5.5; S.Prof = ProfBoxy();
    S.Top = FExCurve::T3({ 282.5, 86.5, 0, 276, 89, 0, 250, 90, 0, 130, 91.5, 0, 100, 93, 1, 25, 136, 1, -40, 138, 0, -95, 137, 0, -100, 135, 1, -150, 101, 1, -160, 100, 0, -275, 98.5, 0, -282.5, 92, 0 });
    S.Belt = FExCurve::T2({ 282.5, 84, 250, 86.5, 100, 88, 0, 89, -150, 93, -275, 94, -282.5, 89 });
    S.WTop = FExCurve::T2({ 282.5, .9, 130, .92, 100, .9, 85, .8, 0, .79, -100, .8, -150, .9, -282.5, .9 });
    S.HalfW = FExCurve::T2({ 282.5, 99, 170, 100.5, 0, 100.5, -200, 100.5, -282.5, 99.5 });
    S.Crown = FExCurve::T2({ 282.5, 2.0, -282.5, 2.0 });
    S.WS = FVector2D(100, 25); S.RG = FVector2D(-100, -150); S.SG = FVector2D(95, -86); S.Pillars = { FVector2D(-17, -28) };
    S.Roof = 1; S.RoofZone = FVector2D(25, -100);
    S.Seams = { 132, -22, -86 }; S.Handles = { -30, -94 };
    S.bChromeBumper = true; S.BF = FVector2D(30, 50); S.BR = FVector2D(32, 52); S.BD = 7; S.BWrap = 1.9;
    S.FLights = { Round(78, 68, 7.6), Round(58, 68, 7.6) };
    S.Grille = G_Egg; S.GZ0 = 40; S.GZ1 = 78; S.GHW = 46;
    S.RLights = { { 20, 999, 62, 80, K_Tail } }; S.PlateZ = 56;
    S.Trims = { { 24.5, 27, 280, -280, K_Chrome, .9 }, { 70, 71.6, 270, -272, K_Chrome, .6 } };
    S.Mirror = M_Round; S.bOrnament = true; S.bAntenna = true; S.Exhaust = { -60 }; S.Wheel = W_Hubcap; S.GHTrim = K_Chrome;
    return S;
}

FCarSpec SpecCruiser()   // 1980s full-size police sedan: black with a white roof and doors, light bar, spotlight, push bar
{
    FCarSpec S = SpecYacht();
    S.Type = EExCar::Cruiser;
    S.Roof = 2; S.RoofZone = FVector2D(25, -100);   // painted roof in the second colour instead of vinyl
    S.Doors = FVector2D(132, -86);   // both doors, seam to seam
    S.WW = 0; S.bOrnament = false; S.bAntenna = true; S.Wheel = W_DogDish; S.bPolice = true;
    return S;
}

FCarSpec SpecWagon()   // late-1970s full-size wagon with wood-grain sides and a roof rack
{
    FCarSpec S; S.Type = EExCar::Wagon;
    S.L = 560; S.W = 200; S.WB = 303; S.FO = 106; S.RW = 35; S.TW = 21; S.RR = 19; S.WW = 0; S.Clear = 16; S.Sill = 24; S.Arch = 40; S.ArchZ = 37;
    S.RF = 10; S.RRE = 6; S.ZCF = 30; S.ZCR = 32; S.RC = 5.5; S.Prof = ProfBoxy();
    S.Top = FExCurve::T3({ 280, 86.5, 0, 274, 89, 0, 250, 90, 0, 130, 91.5, 0, 100, 93, 1, 25, 137, 1, -60, 139, 0, -200, 138, 0, -258, 136, 0, -262, 133, 1, -272, 97, 1, -280, 92, 0 });
    S.Belt = FExCurve::T2({ 280, 84, 250, 87, 100, 90, 0, 92, -150, 95, -272, 95, -280, 90 });
    S.WTop = FExCurve::T2({ 280, .9, 130, .92, 100, .9, 85, .8, 0, .8, -250, .81, -270, .86, -280, .88 });
    S.HalfW = FExCurve::T2({ 280, 99, 170, 100, 0, 100, -200, 100, -280, 99.5 });
    S.Crown = FExCurve::T2({ 280, 2.0, -280, 2.0 });
    S.WS = FVector2D(100, 25); S.RG = FVector2D(-262, -272); S.SG = FVector2D(95, -246); S.Pillars = { FVector2D(-17, -28), FVector2D(-110, -124) };
    S.Seams = { 130, -22, -104 }; S.Handles = { -30, -112 };
    S.bChromeBumper = true; S.BF = FVector2D(30, 50); S.BR = FVector2D(32, 50); S.BD = 7; S.BWrap = 1.9;
    S.FLights = { Rect(77, 68, 8.5, 5.6), Rect(57, 68, 8.5, 5.6) };
    S.Grille = G_Vert; S.GZ0 = 42; S.GZ1 = 78; S.GHW = 46;
    S.RLights = { { -18, 999, 52, 90, K_Tail } }; S.PlateZ = 58;
    S.Trims = { { 24.5, 27, 278, -276, K_Chrome, .9 } };
    S.bWood = true; S.WX0 = 150; S.WX1 = -274; S.WZ0 = 40; S.WZ1 = 86;
    S.Mirror = M_Round; S.bAntenna = true; S.bRoofRack = true; S.Exhaust = { -60 }; S.Wheel = W_Hubcap; S.GHTrim = K_Chrome;
    return S;
}

FCarSpec SpecPickup()  // 1970s square-body pickup with a two-tone side band and an open bed
{
    FCarSpec S; S.Type = EExCar::Pickup;
    S.L = 520; S.W = 201; S.WB = 332; S.FO = 85; S.RW = 38; S.TW = 22; S.RR = 20; S.WW = 0; S.Clear = 22; S.Sill = 46; S.Arch = 45; S.ArchZ = 42;
    S.RF = 8; S.RRE = 4; S.ZCF = 44; S.ZCR = 50; S.RC = 4.5; S.Prof = ProfBoxy();
    S.Top = FExCurve::T3({ 260, 121, 0, 256, 123.5, 0, 240, 124.5, 0, 120, 125.5, 0, 100, 127, 1, 40, 178, 1, -48, 180, 0, -52, 176, 1, -57, 132, 1, -62, 128, 0, -254, 128, 0, -260, 124, 0 });
    S.Belt = FExCurve::T2({ 260, 118, 240, 121, 100, 124, 0, 125, -57, 126, -260, 125 });
    S.WTop = FExCurve::T2({ 260, .9, 120, .92, 100, .9, 85, .84, 0, .84, -50, .84, -57, .9, -260, .9 });
    S.HalfW = FExCurve::T2({ 260, 99.5, 170, 100.5, 0, 100.5, -260, 100.5 });
    S.Crown = FExCurve::T2({ 260, 1.5, -260, 1.5 });
    S.WS = FVector2D(100, 40); S.RG = FVector2D(-52, -57); S.SG = FVector2D(95, -44); S.Bed = FVector2D(-63, -253);
    S.Seams = { 118, -50 }; S.Handles = { -40 };
    S.bChromeBumper = true; S.BF = FVector2D(44, 66); S.BR = FVector2D(50, 66); S.BD = 6; S.BWrap = 1.75;
    S.FLights = { Round(76, 100, 9.0, 1) };
    S.Grille = G_Horiz; S.GZ0 = 72; S.GZ1 = 116; S.GHW = 88;
    S.RLights = { { -14, 999, 78, 116, K_Tail } }; S.PlateZ = 52;
    S.Trims = { { 94, 104, 256, -258, K_Paint2, .5 }, { 93.2, 94, 256, -258, K_Chrome, .7 }, { 104, 104.8, 256, -258, K_Chrome, .7 } };
    S.Mirror = M_Truck; S.bAntenna = true; S.Exhaust = { -70 }; S.Wheel = W_DogDish; S.GHTrim = K_Chrome;
    return S;
}

FCarSpec SpecHatch()   // 1980s boxy three-door hatchback with black bumpers
{
    FCarSpec S; S.Type = EExCar::Hatch;
    S.L = 405; S.W = 163; S.WB = 240; S.FO = 78; S.RW = 29; S.TW = 16; S.RR = 16.5; S.WW = 0; S.Clear = 13; S.Sill = 19; S.Arch = 33; S.ArchZ = 31;
    S.RF = 8; S.RRE = 6; S.ZCF = 24; S.ZCR = 26; S.RC = 4.0; S.Prof = ProfBoxy();
    S.Top = FExCurve::T3({ 202.5, 79, 0, 200, 81.5, 0, 190, 82.5, 0, 110, 84.5, 0, 85, 86, 1, 5, 134, 1, -100, 136, 0, -145, 134, 0, -148, 132, 1, -188, 98, 1, -198, 94, 0, -202.5, 88, 0 });
    S.Belt = FExCurve::T2({ 202.5, 76, 180, 79, 85, 84, 0, 88, -150, 92, -202.5, 90 });
    S.WTop = FExCurve::T2({ 202.5, .9, 110, .92, 85, .9, 70, .83, 0, .82, -145, .83, -190, .86, -202.5, .88 });
    S.HalfW = FExCurve::T2({ 202.5, 80.5, 120, 81.5, 0, 81.5, -202.5, 81.5 });
    S.Crown = FExCurve::T2({ 202.5, 1.5, -202.5, 1.5 });
    S.WS = FVector2D(85, 5); S.RG = FVector2D(-148, -188); S.SG = FVector2D(80, -132); S.Pillars = { FVector2D(-34, -48) };
    S.Seams = { 86, -40 }; S.Handles = { -30 };
    S.bChromeBumper = false; S.BF = FVector2D(24, 42); S.BR = FVector2D(26, 44); S.BD = 8; S.BWrap = 2.2;
    S.FLights = { Rect(55, 66, 12, 7.2) };
    S.Grille = G_Slats; S.GZ0 = 57; S.GZ1 = 74; S.GHW = 40;
    S.RLights = { { -26, 999, 66, 84, K_Tail }, { -34, -26, 66, 84, K_Amber } }; S.PlateZ = 48;
    S.Trims = { { 55, 60, 118, -160, K_Trim, 1.0 } };
    S.Mirror = M_Black; S.Exhaust = { -45 }; S.Wheel = W_Plastic; S.GHTrim = K_Trim;
    return S;
}

FCarSpec SpecFifties() // late-1950s two-tone pillarless hardtop: small fins, wide whitewalls, rear skirts
{
    FCarSpec S; S.Type = EExCar::Fifties;
    S.L = 535; S.W = 200; S.WB = 300; S.FO = 100; S.RW = 35; S.TW = 20; S.RR = 19; S.WW = .5; S.Clear = 17; S.Sill = 26; S.Arch = 40; S.ArchZ = 37;
    S.bSkirt = true; S.RF = 22; S.RRE = 14; S.ZCF = 30; S.ZCR = 34; S.RC = 9.0; S.Prof = ProfRound50();
    S.Top = FExCurve::T3({ 267.5, 92.5, 0, 262, 95.5, 0, 240, 96.5, 0, 120, 98, 0, 95, 100, 1, 30, 148, 1, -40, 150.5, 0, -85, 148, 0, -92, 145, 1, -140, 106, 1, -150, 104, 0, -255, 103, 0, -267.5, 96, 0 });
    S.Belt = FExCurve::T2({ 267.5, 90, 245, 94, 95, 97, 0, 98, -60, 96.5, -140, 103, -250, 106, -267.5, 100 });
    S.WTop = FExCurve::T2({ 267.5, .86, 120, .88, 95, .86, 80, .75, 0, .72, -92, .74, -140, .9, -267.5, .95 });
    S.HalfW = FExCurve::T2({ 267.5, 96, 167, 99, 60, 100, -60, 99.5, -200, 100, -267.5, 98 });
    S.Crown = FExCurve::T2({ 267.5, 3.0, -140, 1.0, -200, -6.0, -262, -11.0, -267.5, -11.0 });
    S.WS = FVector2D(95, 30); S.RG = FVector2D(-92, -140); S.SG = FVector2D(90, -84);
    S.Roof = 2; S.RoofZone = FVector2D(30, -92);
    S.Seams = { 126, -18, -84 }; S.Handles = { -26, -90 };
    S.bChromeBumper = true; S.BF = FVector2D(30, 56); S.BR = FVector2D(34, 56); S.BD = 9; S.BWrap = 1.6;
    S.FLights = { Round(80, 81, 9.0, 2) };
    S.Grille = G_Mesh; S.GZ0 = 44; S.GZ1 = 70; S.GHW = 66;
    S.RLights = { { -12, 999, 78, 100, K_Tail } }; S.PlateZ = 60;
    S.Trims = { { 71, 73, 240, -255, K_Chrome, .8 } };
    S.Mirror = M_Round; S.bOrnament = true; S.bAntenna = true; S.Exhaust = { -55, 55 }; S.Wheel = W_Hubcap; S.GHTrim = K_Chrome;
    return S;
}

FCarSpec SpecMuscle()  // original late-60s/early-70s fastback: long hood, louvred fastback, ducktail, stripes, mags
{
    FCarSpec S; S.Type = EExCar::Muscle;
    S.L = 470; S.W = 186; S.WB = 274; S.FO = 92; S.RW = 33.5; S.TW = 24; S.RR = 19; S.WW = 0; S.Clear = 12; S.Sill = 19; S.Arch = 38.5; S.ArchZ = 34.5;
    S.RF = 14; S.RRE = 6; S.ZCF = 25; S.ZCR = 30; S.RC = 7.0; S.Prof = ProfShoulder();
    S.Top = FExCurve::T3({ 235, 79, 0, 230, 81.5, 0, 210, 83, 0, 120, 86, 0, 95, 88, 1, 32, 122, 1, -20, 125, 0, -40, 124, 1, -150, 98, 1, -200, 94, 0, -226, 97.5, 0, -232, 96.5, 0, -235, 89, 0 });
    S.Belt = FExCurve::T2({ 235, 76, 200, 81, 100, 86, 0, 88, -110, 92, -200, 93, -235, 88 });
    S.WTop = FExCurve::T2({ 235, .84, 120, .86, 95, .82, 80, .7, 0, .66, -40, .66, -150, .78, -235, .88 });
    S.HalfW = FExCurve::T2({ 235, 89, 143, 92, 40, 91, -60, 92.5, -131, 93.5, -235, 92 });
    S.Crown = FExCurve::T2({ 235, -0.4, 150, -1.2, 100, 1.0, 60, 3.0, -235, 2.0 });
    S.WS = FVector2D(95, 32); S.RG = FVector2D(-40, -150); S.SG = FVector2D(90, -62); S.Stripes = FVector2D(7, 19); S.bLouvers = true;
    S.Seams = { 100, -58 }; S.Handles = { -48 };
    S.bChromeBumper = true; S.BF = FVector2D(33, 44); S.BR = FVector2D(36, 47); S.BD = 5; S.BWrap = 1.5;
    S.FLights = { Round(70, 63, 9.3) };
    S.Grille = G_Black; S.GZ0 = 46; S.GZ1 = 76; S.GHW = 84;
    S.RLights = { { 4, 999, 60, 80, K_Tail } }; S.PlateZ = 49;
    S.Mirror = M_Body; S.bScoop = true; S.bRWL = true; S.Exhaust = { -62, 62 }; S.Wheel = W_Mag; S.Steer = 10; S.GHTrim = K_Chrome;
    return S;
}

FCarSpec SpecFor(EExCar T)
{
    switch (T)
    {
    case EExCar::Wagon: return SpecWagon();
    case EExCar::Pickup: return SpecPickup();
    case EExCar::Hatch: return SpecHatch();
    case EExCar::Fifties: return SpecFifties();
    case EExCar::Muscle: return SpecMuscle();
    case EExCar::Cruiser: return SpecCruiser();
    default: return SpecYacht();
    }
}

// ------------------------------------------------------------------ mesh writer (UE winding: front faces are clockwise)
struct FCW
{
    FHallMesh& M;
    explicit FCW(FHallMesh& In) : M(In) {}
    static FVector2D UVFor(int32 K, const FVector& P, const FVector& N)
    {
        if (K == K_Wood) return FVector2D(P.X / 160.0, -P.Z / 160.0);
        return FVector2D((P.X + P.Y) / 100.0, -P.Z / 100.0);
    }
    void PushTri(int32 K, const FVector& A, const FVector& B, const FVector& C, const FVector& NA, const FVector& NB, const FVector& NC,
        const FVector2D* UV)
    {
        FHallSection& S = M.Sec(ExCarSlot(K));
        const int32 Base = S.V.Num();
        const FVector P[3] = { A, B, C }; const FVector N[3] = { NA, NB, NC };
        for (int32 I = 0; I < 3; ++I)
        {
            S.V.Add(P[I]); S.N.Add(N[I]);
            S.UV.Add(UV ? UV[I] : UVFor(K, P[I], N[I]));
            FVector T = P[(I + 1) % 3] - P[I]; T -= N[I] * FVector::DotProduct(T, N[I]);
            if (!T.Normalize()) T = FVector::CrossProduct(N[I], FVector::UpVector).GetSafeNormal();
            if (T.IsNearlyZero()) T = FVector::ForwardVector;
            S.T.Add(FProcMeshTangent(T, false));
        }
        S.I.Append({ Base, Base + 1, Base + 2 });
    }
    /** Flat triangle facing Hint. */
    void Tri(int32 K, FVector A, FVector B, FVector C, const FVector& Hint)
    {
        FVector Fn = FVector::CrossProduct(B - A, C - A);
        const double L = Fn.Size();
        if (L < 1e-9) return;
        Fn /= L;
        if (FVector::DotProduct(Fn, Hint) > 0) { Swap(B, C); Fn = -Fn; }
        const FVector N = -Fn;  // UE front face: cross(B-A, C-A) points away from the visible side
        PushTri(K, A, B, C, N, N, N, nullptr);
    }
    void Quad(int32 K, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Hint)
    {
        Tri(K, A, B, C, Hint); Tri(K, A, C, D, Hint);
    }
    /** Quad with per-corner normals. Each triangle is wound from its own corner normals, so slivers on twisted quads
     *  (wheel-arch lips, bed corners) still face outward instead of being back-face culled. */
    void QuadSmooth(int32 K, const FVector P[4], const FVector N[4], const FVector2D* UV = nullptr)
    {
        FVector2D U[4];
        for (int32 I = 0; I < 4; ++I) U[I] = UV ? UV[I] : UVFor(K, P[I], N[I]);
        auto Emit = [&](int32 A, int32 B, int32 C)
        {
            const FVector G = FVector::CrossProduct(P[B] - P[A], P[C] - P[A]);
            if (G.SizeSquared() < 1e-12) return;
            if (FVector::DotProduct(G, N[A] + N[B] + N[C]) > 0) Swap(B, C);   // UE front face: cross(B-A, C-A) points away from the normal
            const FVector2D T[3] = { U[A], U[B], U[C] };
            PushTri(K, P[A], P[B], P[C], N[A], N[B], N[C], T);
        };
        Emit(0, 1, 2);
        Emit(0, 2, 3);
    }
    void QuadFlat(int32 K, const FVector P[4], const FVector& N)
    {
        const FVector NN[4] = { N, N, N, N };
        QuadSmooth(K, P, NN);
    }
    void Box(int32 K, const FVector& C, const FVector& H, const FVector& AX = FVector::ForwardVector, const FVector& AY = FVector::RightVector,
        const FVector& AZ = FVector::UpVector)
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
    /** Revolve Prof [(along axis, radius)] round Axis through Ctr. */
    void Lathe(int32 K, const FVector& Ctr, FVector Axis, FVector Up, const TArray<FVector2D>& Prof, int32 Seg, double A0 = 0.0, double A1 = TwoPi,
        const TArray<int32>* Keys = nullptr)
    {
        Axis.Normalize();
        Up = Up - Axis * FVector::DotProduct(Up, Axis); Up.Normalize();
        const FVector Side = FVector::CrossProduct(Axis, Up);
        for (int32 S = 0; S < Seg; ++S)
        {
            const double T0 = A0 + (A1 - A0) * S / Seg, T1 = A0 + (A1 - A0) * (S + 1) / Seg;
            const FVector D0 = Up * FMath::Cos(T0) + Side * FMath::Sin(T0), D1 = Up * FMath::Cos(T1) + Side * FMath::Sin(T1);
            for (int32 I = 0; I + 1 < Prof.Num(); ++I)
            {
                const FVector2D P0 = Prof[I], P1 = Prof[I + 1];
                FVector2D N2(-(P1.Y - P0.Y), P1.X - P0.X);
                const double L = N2.Size();
                if (L < 1e-9) continue;
                N2 /= L;
                const FVector P[4] = { Ctr + Axis * P0.X + D0 * P0.Y, Ctr + Axis * P0.X + D1 * P0.Y, Ctr + Axis * P1.X + D1 * P1.Y, Ctr + Axis * P1.X + D0 * P1.Y };
                const FVector N[4] = { Axis * N2.X + D0 * N2.Y, Axis * N2.X + D1 * N2.Y, Axis * N2.X + D1 * N2.Y, Axis * N2.X + D0 * N2.Y };
                QuadSmooth(Keys ? (*Keys)[I] : K, P, N);
            }
        }
    }
    void Cyl(int32 K, const FVector& A, const FVector& B, double R, int32 Seg = 10)
    {
        const FVector Ax = B - A; const double L = Ax.Size();
        if (L < 1e-6) return;
        const FVector Up = FMath::Abs(Ax.Z) < .9 * L ? FVector::UpVector : FVector::ForwardVector;
        Lathe(K, A, Ax, Up, { FVector2D(0, .001), FVector2D(0, R), FVector2D(L, R), FVector2D(L, .001) }, Seg);
    }
};

// ------------------------------------------------------------------ builder
struct FSP { double Y, Z; uint8 Seg; };

struct FCarBuilder
{
    const FCarSpec& S;
    FCW Wr;
    double XF, XR, AXF, AXR;
    FCarBuilder(const FCarSpec& In, FHallMesh& M) : S(In), Wr(M)
    {
        XF = S.L / 2; XR = -S.L / 2; AXF = XF - S.FO; AXR = AXF - S.WB;
    }

    double Wd(double x) const
    {
        double W = S.HalfW(x);
        if (x > XF - S.RF) { const double D = x - (XF - S.RF); W += -S.RF + FMath::Sqrt(FMath::Max(0.0, S.RF * S.RF - D * D)); }
        if (x < XR + S.RRE) { const double D = (XR + S.RRE) - x; W += -S.RRE + FMath::Sqrt(FMath::Max(0.0, S.RRE * S.RRE - D * D)); }
        return W;
    }
    double Zc(double x) const
    {
        const double A = AXF + S.Arch, B = AXR - S.Arch;
        if (x > A) { const double T = (x - A) / (XF - A); return S.Clear + (S.ZCF - S.Clear) * T * T; }
        if (x < B) { const double T = (B - x) / (B - XR); return S.Clear + (S.ZCR - S.Clear) * T * T; }
        return S.Clear;
    }
    double Za(double x) const
    {
        double Z = FMath::Max(S.Sill, Zc(x) + 4.0);
        for (int32 K = 0; K < 2; ++K)
        {
            const double Ax = K ? AXR : AXF;
            if (S.bSkirt && K == 1) continue;
            const double D = x - Ax;
            if (FMath::Abs(D) < S.Arch) Z = FMath::Max(Z, S.ArchZ + FMath::Sqrt(S.Arch * S.Arch - D * D));
        }
        return Z;
    }
    bool InBed(double x) const { return Has(S.Bed) && x > FMath::Min(S.Bed.X, S.Bed.Y) && x < FMath::Max(S.Bed.X, S.Bed.Y); }
    void TopYs(double YR, TArray<double>& Out) const
    {
        Out.Reset();
        if (Has(S.Stripes))
        {
            const double Y0 = S.Stripes.X, Y1 = S.Stripes.Y;
            Out.Append({ YR * .74, FMath::Max(Y1 + 8.0, YR * .48), Y1, Y0, Y0 * .5, 0.0 });
        }
        else Out.Append({ YR * .75, YR * .5, YR * .25, 0.0 });
    }
    double SideY(double x, double z) const
    {
        const double W = Wd(x), Zb = S.Belt(x);
        const double T = FMath::Max(0.0, (z - S.Sill) / FMath::Max(1.0, Zb - S.Sill));
        return W * S.Prof(FMath::Min(T, 1.0));
    }

    void Section(double x, TArray<FSP>& P) const
    {
        P.Reset();
        const double W = Wd(x), ZC = Zc(x), ZA = Za(x), ZB = S.Belt(x);
        double ZT = S.Top(x);
        const double Win = W * .62;
        P.Add({ 0.0, ZC, S_U }); P.Add({ Win, ZC, S_U }); P.Add({ Win, ZA, S_WL }); P.Add({ W * S.Prof(0) - .2, ZA, S_WL });
        const int32 NB = 9;
        for (int32 K = 0; K <= NB; ++K)
        {
            const double Z = ZA + (ZB - ZA) * K / NB;
            const double T = FMath::Max(0.0, (Z - S.Sill) / FMath::Max(1.0, ZB - S.Sill));
            P.Add({ W * S.Prof(FMath::Min(1.0, T)), Z, S_B });
        }
        const double YB = W * S.Prof(1.0) - .8;
        TArray<double> Ys;
        if (InBed(x))
        {
            const double YI = YB - 7.0, ZF = S.Sill + 26.0;
            for (int32 K = 1; K <= 4; ++K) { const double F = K / 4.0; P.Add({ YB + (YI - YB) * F, ZB + 1.0 - .6 * FMath::Abs(F - .5), S_R }); }
            for (int32 K = 1; K <= 4; ++K) { const double F = K / 4.0; P.Add({ YI - .4 * F, ZB + (ZF - ZB) * F, S_I }); }
            TopYs(YI - .4, Ys);
            for (double Y : Ys) P.Add({ Y, ZF, S_F });
            return;
        }
        ZT = FMath::Max(ZT, ZB + 2.5);
        const double Crown = S.Crown(x);
        const double YR = W * S.WTop(x);
        const double H = ZT - FMath::Max(Crown, 0.0) - ZB;
        double RCZ = FMath::Max(.6, FMath::Min(S.RC, .45 * H));
        const double RCY = FMath::Min(1.4 * RCZ + 2.0, .35 * YR);
        const double ZEdge = ZT - Crown;
        double ZS = ZEdge - RCZ;
        if (ZS < ZB + .3) { ZS = ZB + .3; RCZ = FMath::Max(.3, ZEdge - ZS); }
        for (int32 K = 1; K <= 4; ++K) { const double F = K / 4.0; P.Add({ YB + (YR + RCY - YB) * F, ZB + (ZS - ZB) * F, S_G }); }
        for (int32 K = 1; K <= 4; ++K) { const double Ph = CarHalfPi * K / 4.0; P.Add({ YR + RCY * FMath::Cos(Ph), ZS + RCZ * FMath::Sin(Ph), S_C }); }
        TopYs(YR, Ys);
        for (double Y : Ys) P.Add({ Y, ZT - Crown * FMath::Square(Y / FMath::Max(1e-3, YR)), S_T });
    }

    void Stations(TArray<double>& Out) const
    {
        TArray<double> Xs;
        for (int32 K = 0; K < 9; ++K)
        {
            const double A = CarHalfPi * K / 8;
            Xs.Add(XF - S.RF * (1 - FMath::Cos(A))); Xs.Add(XR + S.RRE * (1 - FMath::Cos(A)));
        }
        for (double Ax : { AXF, AXR })
        {
            for (int32 K = 0; K < 13; ++K) Xs.Add(Ax + S.Arch * FMath::Cos(CarPi * K / 12));
            Xs.Add(Ax + S.Arch + .5); Xs.Add(Ax - S.Arch - .5);
        }
        for (double X : S.Top.X) Xs.Add(X);
        for (const FVector2D& R : { S.WS, S.RG, S.SG, S.RoofZone, S.Doors }) if (Has(R)) { Xs.Add(R.X); Xs.Add(R.Y); }
        for (const FVector2D& R : S.Pillars) { Xs.Add(R.X); Xs.Add(R.Y); }
        if (Has(S.Bed)) Xs.Append({ S.Bed.X + .01, S.Bed.Y - .01, S.Bed.X - 1.5, S.Bed.Y + 1.5 });
        Xs.Sort();
        Out.Reset();
        for (double X : Xs)
        {
            if (X < XR || X > XF) continue;
            if (Out.Num() == 0) { Out.Add(X); continue; }
            while (X - Out.Last() > 20.0) Out.Add(Out.Last() + (X - Out.Last()) / FMath::CeilToDouble((X - Out.Last()) / 20.0));
            if (X - Out.Last() > .4) Out.Add(X); else Out.Last() = X;
        }
    }

    int32 Material(uint8 Seg, double X, double YQ, double ZQ, int32 Row) const
    {
        const bool bRoofZ = S.Roof != 0 && In(S.RoofZone, X);
        const int32 RoofKey = S.Roof == 1 ? K_Vinyl : K_Paint2;
        switch (Seg)
        {
        case S_U: case S_WL: return K_Dark;
        case S_B: return Has(S.Doors) && In(S.Doors, X) ? K_Paint2 : K_Paint;
        case S_R: case S_I: return K_Paint;
        case S_F: return K_Trim;
        case S_G:
        {
            bool bPillar = false;
            for (const FVector2D& P : S.Pillars) bPillar |= In(P, X);
            if (In(S.SG, X) && !bPillar) return Row == 0 ? S.GHTrim : K_Glass;
            return bRoofZ ? RoofKey : K_Paint;
        }
        case S_C: return bRoofZ ? RoofKey : K_Paint;
        case S_T:
            if (In(S.WS, X)) return K_Glass;
            if (In(S.RG, X))
            {
                if (S.bLouvers && FMath::Abs(YQ) < 50 && (int32)((X - FMath::Min(S.RG.X, S.RG.Y)) / 7.0) % 2 == 0) return K_Dark;
                return K_Glass;
            }
            if (Has(S.Stripes) && FMath::Abs(YQ) >= S.Stripes.X && FMath::Abs(YQ) <= S.Stripes.Y) return K_Paint2;
            if (bRoofZ) return RoofKey;
            return K_Paint;
        default: return K_Paint;
        }
    }

    void Body()
    {
        TArray<double> X; Stations(X);
        const int32 NS = X.Num();
        TArray<FSP> Half; Section(X[0], Half);
        const int32 NH = Half.Num(), NL = 2 * NH - 1;
        TArray<FVector> G; G.SetNum(NS * NL);
        TArray<uint8> SG; SG.SetNum(NS * NL);
        for (int32 I = 0; I < NS; ++I)
        {
            Section(X[I], Half);
            check(Half.Num() == NH);
            for (int32 J = 0; J < NH; ++J) { G[I * NL + J] = FVector(X[I], Half[J].Y, Half[J].Z); SG[I * NL + J] = Half[J].Seg; }
            for (int32 K = 0; K < NH - 1; ++K)
            {
                const FSP& P = Half[NH - 2 - K];
                G[I * NL + NH + K] = FVector(X[I], -P.Y, P.Z); SG[I * NL + NH + K] = P.Seg;
            }
        }
        auto At = [&](int32 I, int32 J) -> const FVector& { return G[I * NL + J]; };
        TArray<FVector> N; N.SetNum(NS * NL);
        for (int32 I = 0; I < NS; ++I)
        {
            FVector Ctr = FVector::ZeroVector;
            for (int32 J = 0; J < NL; ++J) Ctr += At(I, J);
            Ctr /= NL;
            for (int32 J = 0; J < NL; ++J)
            {
                const FVector A = At(FMath::Min(I + 1, NS - 1), J) - At(FMath::Max(I - 1, 0), J);
                FVector B = At(I, FMath::Min(J + 1, NL - 1)) - At(I, FMath::Max(J - 1, 0));
                for (int32 K = 2; B.Size() < 1e-3 && K < 6; ++K) B = At(I, FMath::Min(J + K, NL - 1)) - At(I, FMath::Max(J - K, 0));
                FVector Nn = FVector::CrossProduct(B, A);
                if (!Nn.Normalize()) Nn = FVector::UpVector;
                FVector D = At(I, J) - Ctr; D.X = 0;
                const uint8 Sg = SG[I * NL + J];
                if (Sg == S_I) D = -D;
                if (Sg == S_F) D = FVector::UpVector;
                if (FVector::DotProduct(Nn, D) < 0) Nn = -Nn;
                N[I * NL + J] = Nn;
            }
        }
        const int32 HalfIdx = NL / 2;
        int32 GFirst = 0;
        for (int32 J = 0; J <= HalfIdx; ++J) if (SG[J] == S_G) { GFirst = J; break; }
        // (bed stations have no G points; their quads never use the row)
        for (int32 I = 0; I + 1 < NS; ++I)
            for (int32 J = 0; J + 1 < NL; ++J)
            {
                const uint8 S0 = SG[I * NL + J], S1 = SG[I * NL + J + 1];
                const int32 JJ = J < HalfIdx ? J : NL - 2 - J;
                const uint8 Seg = J < HalfIdx ? S1 : S0;
                const double XQ = .5 * (At(I, J).X + At(I + 1, J).X);
                const double YQ = .25 * (At(I, J).Y + At(I + 1, J).Y + At(I, J + 1).Y + At(I + 1, J + 1).Y);
                const double ZQ = .25 * (At(I, J).Z + At(I + 1, J).Z + At(I, J + 1).Z + At(I + 1, J + 1).Z);
                int32 GF = GFirst;
                for (int32 K = 0; K <= HalfIdx; ++K) if (SG[I * NL + K] == S_G) { GF = K; break; }
                const int32 Row = Seg == S_G ? (JJ + 1) - GF : 0;
                const int32 Mat = Material(Seg, XQ, YQ, ZQ, Row);
                const FVector Q[4] = { At(I, J), At(I + 1, J), At(I + 1, J + 1), At(I, J + 1) };
                FVector Nq[4] = { N[I * NL + J], N[(I + 1) * NL + J], N[(I + 1) * NL + J + 1], N[I * NL + J + 1] };
                if (Mat == K_Glass || Seg == S_R || Seg == S_I || Seg == S_F)
                {
                    FVector Fn = FVector::CrossProduct(Q[2] - Q[0], Q[3] - Q[1]);
                    if (Fn.Normalize())
                    {
                        if (FVector::DotProduct(Fn, Nq[0] + Nq[1] + Nq[2] + Nq[3]) < 0) Fn = -Fn;
                        for (FVector& V : Nq) V = Fn;
                    }
                }
                Wr.QuadSmooth(Mat, Q, Nq);
            }
        for (int32 E = 0; E < 2; ++E)
        {
            const int32 I = E ? 0 : NS - 1; const double Sg = E ? -1.0 : 1.0;
            FVector C = FVector::ZeroVector;
            for (int32 J = 0; J < NL; ++J) C += At(I, J);
            C /= NL; C.Y = 0;
            for (int32 J = 0; J + 1 < NL; ++J) Wr.Tri(K_Paint, C, At(I, J), At(I, J + 1), FVector(Sg, 0, 0));
        }
    }

    // ------------------------------------------------------------- face / side helpers
    FVector EndPoint(bool bFront, double U, double Z, double Off, FVector& OutN) const
    {
        const double R = bFront ? S.RF : S.RRE, XE = bFront ? XF : XR, Sg = bFront ? 1.0 : -1.0;
        const double XC = XE - Sg * R;
        const double WC = SideY(XC, Z) - R;
        if (U <= WC) { OutN = FVector(Sg, 0, 0); return FVector(XE + Sg * Off, U, Z); }
        const double Ph = (U - WC) / R;
        if (Ph <= CarHalfPi)
        {
            OutN = FVector(Sg * FMath::Cos(Ph), FMath::Sin(Ph), 0);
            return FVector(XC + Sg * (R + Off) * FMath::Cos(Ph), WC + (R + Off) * FMath::Sin(Ph), Z);
        }
        const double Run = (Ph - CarHalfPi) * R;
        const double X = XC - Sg * Run;
        OutN = FVector(0, 1, 0);
        return FVector(X, SideY(X, Z) + Off, Z);
    }
    double FaceHalf(bool bFront, double Z) const { return SideY(bFront ? XF - S.RF : XR + S.RRE, Z) - (bFront ? S.RF : S.RRE); }

    void EndPatch(int32 K, bool bFront, double U0, double U1, double Z0, double Z1, int32 NU = 6, bool bMirror = true, double Off = .35)
    {
        for (int32 Side = 0; Side < (bMirror ? 2 : 1); ++Side)
        {
            const FVector Fl(1, Side ? -1 : 1, 1);
            for (int32 Kk = 0; Kk < NU; ++Kk)
            {
                const double A0 = U0 + (U1 - U0) * Kk / NU, A1 = U0 + (U1 - U0) * (Kk + 1) / NU;
                FVector N0, N1, Tmp;
                const FVector P00 = EndPoint(bFront, A0, Z0, Off, N0), P10 = EndPoint(bFront, A1, Z0, Off, N1);
                const FVector P11 = EndPoint(bFront, A1, Z1, Off, Tmp), P01 = EndPoint(bFront, A0, Z1, Off, Tmp);
                const FVector Q[4] = { P00 * Fl, P10 * Fl, P11 * Fl, P01 * Fl };
                const FVector Nq[4] = { N0 * Fl, N1 * Fl, N1 * Fl, N0 * Fl };
                Wr.QuadSmooth(K, Q, Nq);
            }
        }
    }

    /** Solid strip proud of the end face (bumpers, bezels): outer face at offset D with top/bottom faces back to the body. */
    void Band(int32 K, bool bFront, double U0, double U1, double Z0, double Z1, double D, int32 NU = 8)
    {
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const FVector Fl(1, Side ? -1 : 1, 1);
            FVector PA0, PA1, PB0, PB1, NA; bool bPrev = false;
            for (int32 Kk = 0; Kk <= NU; ++Kk)
            {
                const double U = U0 + (U1 - U0) * Kk / NU;
                FVector Nn, Tmp;
                const FVector O0 = EndPoint(bFront, U, Z0, D, Nn) * Fl, O1 = EndPoint(bFront, U, Z1, D, Tmp) * Fl;
                const FVector I0 = EndPoint(bFront, U, Z0, 0, Tmp) * Fl, I1 = EndPoint(bFront, U, Z1, 0, Tmp) * Fl;
                Nn *= Fl;
                if (bPrev)
                {
                    const FVector F[4] = { PA0, O0, O1, PA1 }; const FVector FN[4] = { NA, Nn, Nn, NA };
                    Wr.QuadSmooth(K, F, FN);
                    const FVector T[4] = { PA1, O1, I1, PB1 }; Wr.QuadFlat(K, T, FVector::UpVector);
                    const FVector B[4] = { PA0, PB0, I0, O0 }; Wr.QuadFlat(K, B, -FVector::UpVector);
                }
                PA0 = O0; PA1 = O1; PB0 = I0; PB1 = I1; NA = Nn; bPrev = true;
            }
            FVector Td = FVector::CrossProduct(NA, FVector::UpVector);
            if (FVector::DotProduct(Td, PA0 - PB0) < -1) Td = -Td;
            Wr.Quad(K, PA0, PA1, PB1, PB0, Td);
        }
    }

    void SideRanges(double X0, double X1, double Z, TArray<FVector2D>& Out) const
    {
        Out.Reset();
        const double Lo = FMath::Min(X0, X1), Hi = FMath::Max(X0, X1);
        const int32 N = (int32)((Hi - Lo) / 2) + 2;
        double Start = 0; bool bOpen = false;
        for (int32 K = 0; K < N; ++K)
        {
            const double X = Lo + (Hi - Lo) * K / (N - 1);
            const bool bOk = Za(X) < Z - .5;
            if (bOk && !bOpen) { Start = X; bOpen = true; }
            if (!bOk && bOpen) { if (X - 2 - Start > 1) Out.Add(FVector2D(Start, X - 2)); bOpen = false; }
        }
        if (bOpen && Hi - Start > 1) Out.Add(FVector2D(Start, Hi));
    }

    void SideStrip(int32 K, double X0, double X1, double Z0, double Z1, double Off = .25, int32 NZ = 4, int32 NX = 0, bool bSolid = false)
    {
        TArray<FVector2D> R; SideRanges(X0, X1, Z0, R);
        for (const FVector2D& Rg : R)
        {
            const double A = Rg.X, B = Rg.Y;
            const int32 Nx = NX > 0 ? NX : FMath::Max(1, (int32)((B - A) / 25));
            for (int32 Side = 0; Side < 2; ++Side)
            {
                const double Sg = Side ? -1.0 : 1.0;
                for (int32 I = 0; I < Nx; ++I)
                {
                    const double XA = A + (B - A) * I / Nx, XB = A + (B - A) * (I + 1) / Nx;
                    for (int32 Kz = 0; Kz < NZ; ++Kz)
                    {
                        const double ZA_ = Z0 + (Z1 - Z0) * Kz / NZ, ZB_ = Z0 + (Z1 - Z0) * (Kz + 1) / NZ;
                        const FVector Q[4] = {
                            FVector(XA, Sg * (SideY(XA, ZA_) + Off), ZA_), FVector(XB, Sg * (SideY(XB, ZA_) + Off), ZA_),
                            FVector(XB, Sg * (SideY(XB, ZB_) + Off), ZB_), FVector(XA, Sg * (SideY(XA, ZB_) + Off), ZB_) };
                        Wr.QuadFlat(K, Q, FVector(0, Sg, 0));
                    }
                    if (bSolid)
                        for (int32 E = 0; E < 2; ++E)
                        {
                            const double Z = E ? Z0 : Z1;
                            const FVector Q[4] = {
                                FVector(XA, Sg * (SideY(XA, Z) + Off), Z), FVector(XB, Sg * (SideY(XB, Z) + Off), Z),
                                FVector(XB, Sg * (SideY(XB, Z) - .5), Z), FVector(XA, Sg * (SideY(XA, Z) - .5), Z) };
                            Wr.QuadFlat(K, Q, FVector(0, 0, E ? -1.0 : 1.0));
                        }
                }
                if (bSolid)  // end caps
                    for (int32 E = 0; E < 2; ++E)
                    {
                        const double Xe = E ? B : A;
                        const FVector Q[4] = {
                            FVector(Xe, Sg * (SideY(Xe, Z0) - .5), Z0), FVector(Xe, Sg * (SideY(Xe, Z0) + Off), Z0),
                            FVector(Xe, Sg * (SideY(Xe, Z1) + Off), Z1), FVector(Xe, Sg * (SideY(Xe, Z1) - .5), Z1) };
                        Wr.QuadFlat(K, Q, FVector(E ? 1.0 : -1.0, 0, 0));
                    }
            }
        }
    }

    // ------------------------------------------------------------- lamps, wheels, details
    void RoundLight(bool bFront, double Y, double Z, double R, int32 Bezel)
    {
        const double Sg = bFront ? 1.0 : -1.0, XE = bFront ? XF : XR;
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const FVector C(XE + Sg * .3, Side ? -Y : Y, Z), Ax(Sg, 0, 0);
            if (Bezel == 1)
            {
                Wr.Box(K_Chrome, C + Ax * .6, FVector(.6, R + 3.2, R + 3.2));
                Wr.Box(K_Dark, C + Ax * 1.0, FVector(.4, R + 1.4, R + 1.4));
            }
            else if (Bezel == 2) Wr.Lathe(K_Chrome, C, Ax, FVector::UpVector, { FVector2D(0, R + 3.6), FVector2D(2.6, R + 3.2), FVector2D(3.0, R + 1.2), FVector2D(2.0, R) }, 24);
            else Wr.Lathe(K_Chrome, C, Ax, FVector::UpVector, { FVector2D(0, R + 2.4), FVector2D(1.4, R + 2.2), FVector2D(1.8, R + .8), FVector2D(1.2, R) }, 20);
            Wr.Lathe(K_Lens, C, Ax, FVector::UpVector, { FVector2D(1.2, R), FVector2D(2.0, R * .75), FVector2D(2.5, R * .35), FVector2D(2.6, .01) }, 20);
        }
    }
    void RectLight(bool bFront, double Y, double Z, double HW, double HH)
    {
        const double Sg = bFront ? 1.0 : -1.0, XE = bFront ? XF : XR;
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const FVector C(XE + Sg * .8, Side ? -Y : Y, Z);
            Wr.Box(K_Chrome, C, FVector(.8, HW + 1.6, HH + 1.6));
            Wr.Box(K_Lens, C + FVector(Sg * .9, 0, 0), FVector(.25, HW, HH));
        }
    }

    void Wheel(const FVector& C, double Sgn, double Steer)
    {
        const FVector Axis(-FMath::Sin(Steer) * Sgn, Sgn * FMath::Cos(Steer), 0), Up = FVector::UpVector;
        const FVector Side = FVector::CrossProduct(Axis, Up);
        const double R = S.RW, Hw = S.TW / 2, RR = S.RR, SW = R - RR;
        TArray<FVector2D> Prof = { FVector2D(-Hw + .5, RR + .6), FVector2D(-Hw - 1.2, RR + SW * .4), FVector2D(-Hw - .4, R - 3.5), FVector2D(-Hw + 3.2, R),
            FVector2D(Hw - 3.2, R), FVector2D(Hw + .4, R - 3.5) };
        TArray<int32> Keys = { K_Tire, K_Tire, K_Tire, K_Tire, K_Tire };
        if (S.WW > 0)
        {
            Prof.Append({ FVector2D(Hw + 1.0, RR + SW * (.18 + S.WW * .62)), FVector2D(Hw + 1.25, RR + SW * .2), FVector2D(Hw - .4, RR + .6) });
            Keys.Append({ K_Tire, K_White, K_White });
        }
        else
        {
            Prof.Append({ FVector2D(Hw + 1.2, RR + SW * .4), FVector2D(Hw - .4, RR + .6) });
            Keys.Append({ K_Tire, K_Tire });
        }
        Wr.Lathe(K_Tire, C, Axis, Up, Prof, 22, 0, TwoPi, &Keys);
        if (S.bRWL)
            for (int32 K = 0; K < 14; ++K)
            {
                const double A0 = TwoPi * K / 14, A1 = A0 + TwoPi / 14 * .55;
                Wr.Lathe(K_White, C, Axis, Up, { FVector2D(Hw + 1.3, RR + SW * .68), FVector2D(Hw + 1.3, RR + SW * .5) }, 3, A0, A1);   // outward-facing letters
            }
        Wr.Lathe(K_Disc, C, Axis, Up, { FVector2D(Hw - 1.0, RR), FVector2D(-Hw + 1.0, RR) }, 18);
        auto Radial = [&](int32 K, double Ang, double Along, double Rad, const FVector& Half)
        {
            const FVector D = Up * FMath::Cos(Ang) + Side * FMath::Sin(Ang);
            Wr.Box(K, C + Axis * Along + D * Rad, Half, D, FVector::CrossProduct(Axis, D), Axis);
        };
        switch (S.Wheel)
        {
        case W_Hubcap:
            Wr.Lathe(K_Chrome, C, Axis, Up, { FVector2D(Hw - .4, RR + .8), FVector2D(Hw + .9, RR - .6), FVector2D(Hw + 1.6, RR * .82), FVector2D(Hw + 2.2, RR * .62),
                FVector2D(Hw + 1.9, RR * .55), FVector2D(Hw + 2.9, RR * .38), FVector2D(Hw + 3.4, RR * .16), FVector2D(Hw + 3.6, .01) }, 22);
            Wr.Lathe(K_Dark, C, Axis, Up, { FVector2D(Hw + 2.05, RR * .6), FVector2D(Hw + 1.95, RR * .57) }, 22);
            for (int32 K = 0; K < 12; ++K) Radial(K_Dark, TwoPi * K / 12, Hw + 1.8, RR * .72, FVector(RR * .12, .35, .3));
            Wr.Lathe(K_Trim, C, Axis, Up, { FVector2D(Hw + 3.62, RR * .12), FVector2D(Hw + 3.66, .01) }, 12);
            break;
        case W_DogDish:
            Wr.Lathe(K_WheelPaint, C, Axis, Up, { FVector2D(Hw - .6, RR + .4), FVector2D(Hw - .6, RR - 1.5), FVector2D(Hw - 2.2, RR * .8), FVector2D(Hw - 2.4, RR * .5), FVector2D(Hw - 1.8, .01) }, 20);
            for (int32 K = 0; K < 8; ++K) Radial(K_Dark, TwoPi * K / 8, Hw - 2.2, RR * .68, FVector(2.2, 1.6, .3));
            Wr.Lathe(K_Chrome, C, Axis, Up, { FVector2D(Hw - 1.8, RR * .5), FVector2D(Hw - .6, RR * .46), FVector2D(Hw + .4, RR * .3), FVector2D(Hw + .8, .01) }, 20);
            break;
        case W_Mag:
            Wr.Lathe(K_Chrome, C, Axis, Up, { FVector2D(Hw - .2, RR + .7), FVector2D(Hw + .3, RR - .4), FVector2D(Hw - .6, RR - 2.6) }, 22);
            Wr.Lathe(K_Mag, C, Axis, Up, { FVector2D(Hw - .6, RR - 2.6), FVector2D(Hw - 1.4, RR * .75), FVector2D(Hw - 1.0, RR * .4), FVector2D(Hw - .2, RR * .25), FVector2D(Hw + .4, .01) }, 22);
            for (int32 K = 0; K < 5; ++K) Radial(K_Dark, TwoPi * K / 5, Hw - 1.0, RR * .58, FVector(RR * .22, 2.0, .35));
            Wr.Lathe(K_Chrome, C, Axis, Up, { FVector2D(Hw + .42, RR * .18), FVector2D(Hw + 1.2, RR * .1), FVector2D(Hw + 1.3, .01) }, 12);
            for (int32 K = 0; K < 5; ++K)
            {
                const double A = TwoPi * (K + .5) / 5;
                Wr.Box(K_Chrome, C + Axis * (Hw - .2) + (Up * FMath::Cos(A) + Side * FMath::Sin(A)) * RR * .32, FVector(.8, .8, .8));
            }
            break;
        default:
            Wr.Lathe(K_Rim, C, Axis, Up, { FVector2D(Hw - .4, RR + .5), FVector2D(Hw + .6, RR - 1.0), FVector2D(Hw + 1.2, RR * .7), FVector2D(Hw + 1.6, RR * .3), FVector2D(Hw + 1.7, .01) }, 20);
            for (int32 K = 0; K < 16; ++K) Radial(K_Dark, TwoPi * K / 16, Hw + 1.0, RR * .62, FVector(RR * .18, .5, .3));
            break;
        }
        Wr.Lathe(K_Disc, C, Axis, Up, { FVector2D(Hw - 5, RR - 2), FVector2D(Hw - 5, .01) }, 14);
    }

    void PlateQuad()
    {
        const double Z0 = S.PlateZ, Z1 = S.PlateZ + 15.2;
        FVector N, Tmp;
        const FVector Q[4] = { EndPoint(false, 15.2, Z1, .8, N), EndPoint(false, -15.2, Z1, .8, Tmp), EndPoint(false, -15.2, Z0, .8, Tmp), EndPoint(false, 15.2, Z0, .8, Tmp) };
        const FVector NN[4] = { N, N, N, N };
        const int32 Pick = (int32)S.Type % 4;
        const double U0 = (Pick % 2) * .5, V0 = (Pick / 2) * .5;
        // Seen from behind the car, +Y is the reader's right: the +Y corners take the right edge of the plate image.
        const FVector2D UV[4] = { FVector2D(U0 + .5, V0), FVector2D(U0, V0), FVector2D(U0, V0 + .5), FVector2D(U0 + .5, V0 + .5) };
        Wr.QuadSmooth(K_Plate, Q, NN, UV);
    }

    void Details()
    {
        const bool bHatch = S.Type == EExCar::Hatch;
        for (double Xs : S.Seams)
        {
            if (InBed(Xs)) continue;
            SideStrip(K_Dark, Xs - .35, Xs + .35, Za(Xs) + 2, S.Belt(Xs) - 1, .15, 6, 1);
        }
        for (double Xh : S.Handles)
        {
            const double Zh = S.Belt(Xh) - 8;
            SideStrip(bHatch ? K_Trim : K_Chrome, Xh - 8, Xh + 8, Zh - 1.5, Zh + 1.5, 1.3, 1, 1, true);
        }
        for (const FCarTrim& T : S.Trims) SideStrip(T.Key, T.X0, T.X1, T.Z0, T.Z1, T.Off, 1, 0, T.Off > .55);
        if (S.Type == EExCar::Yacht || S.Type == EExCar::Wagon || S.Type == EExCar::Pickup || S.Type == EExCar::Fifties)
        {
            double Zm = S.Belt(XF - 20) - 16;
            SideStrip(K_Amber, XF - 26, XF - 14, Zm - 2.5, Zm + 2.5, .6, 1, 1, true);
            Zm = S.Belt(XR + 20) - 16;
            SideStrip(K_Tail, XR + 14, XR + 26, Zm - 2.5, Zm + 2.5, .6, 1, 1, true);
        }
        if (S.Type == EExCar::Muscle)
        {
            SideStrip(K_Dark, -72, -98, 58, 72, .4, 2, 2);
            SideStrip(K_Paint, -70, -100, 72, 74.5, 1.6, 1, 2, true);
            Band(K_Trim, true, 0, FaceHalf(true, 28) + 10, Zc(XF) - 2, Zc(XF) + 3, 2.5, 6);
        }
        if (S.bSkirt) SideStrip(K_Chrome, AXR + S.Arch, AXR - S.Arch, S.Sill + 1, S.Sill + 3, .8, 1, 0, true);
        if (S.bWood)
        {
            SideStrip(K_Wood, S.WX0, S.WX1, S.WZ0, S.WZ1, .3, 3);
            SideStrip(K_WoodTrim, S.WX0, S.WX1, S.WZ1 - .2, S.WZ1 + 2.4, .6, 1, 0, true);
            SideStrip(K_WoodTrim, S.WX0, S.WX1, S.WZ0 - 2.4, S.WZ0 + .2, .6, 1, 0, true);
        }
        // mirrors
        {
            const double Xm = S.WS.X - 9, Zb = S.Belt(Xm), Ym = SideY(Xm, Zb);
            for (int32 Side = 0; Side < 2; ++Side)
            {
                const double Sg = Side ? -1.0 : 1.0;
                if (S.Mirror == M_Truck)
                {
                    const FVector P(Xm - 4, Sg * (Ym + 17), Zb + 16);
                    Wr.Box(K_Chrome, P, FVector(1.2, 8, 12)); Wr.Box(K_Glass, P - FVector(1.3, 0, 0), FVector(.2, 7, 11));
                    Wr.Cyl(K_Chrome, FVector(Xm, Sg * (Ym - 1), Zb + 6), P + FVector(0, -Sg * 7, -10), .9, 8);
                    Wr.Cyl(K_Chrome, FVector(Xm, Sg * (Ym - 1), Zb + 22), P + FVector(0, -Sg * 7, 10), .9, 8);
                }
                else if (S.Mirror == M_Round)
                {
                    const FVector P(Xm - 2, Sg * (Ym + 9), Zb + 9);
                    Wr.Lathe(K_Chrome, P, FVector(1, 0, 0), FVector::UpVector, { FVector2D(-1.5, .01), FVector2D(-1.5, 6.5), FVector2D(2.5, 6.0), FVector2D(4.5, 2.5), FVector2D(4.8, .01) }, 14);
                    Wr.Lathe(K_Glass, P - FVector(1.6, 0, 0), FVector(-1, 0, 0), FVector::UpVector, { FVector2D(0, 6.2), FVector2D(.1, .01) }, 14);
                    Wr.Cyl(K_Chrome, FVector(Xm + 2, Sg * (Ym - 1), Zb + 1), P + FVector(2, 0, -3), 1.0, 8);
                }
                else
                {
                    const FVector P(Xm - 2, Sg * (Ym + 7), Zb + 7);
                    Wr.Box(S.Mirror == M_Body ? K_Paint : K_Trim, P, FVector(5, 6.5, 4.5));
                    Wr.Box(K_Glass, P - FVector(5.1, 0, 0), FVector(.2, 5.6, 3.6));
                }
            }
        }
        // front: grille, lamps, bumper
        if (S.Grille == G_Black)
        {
            EndPatch(K_Dark, true, 0, S.GHW, S.GZ0, S.GZ1, 4);
            Band(K_Chrome, true, 0, S.GHW + 1.5, S.GZ1, S.GZ1 + 1.6, .8, 4);
            Band(K_Chrome, true, 0, S.GHW + 1.5, S.GZ0 - 1.6, S.GZ0, .8, 4);
        }
        else
        {
            EndPatch(K_Dark, true, 0, S.GHW, S.GZ0, S.GZ1, 3);
            const int32 NH = S.Grille == G_Egg ? 4 : S.Grille == G_Horiz ? 6 : S.Grille == G_Mesh ? 7 : S.Grille == G_Slats ? 4 : 1;
            const int32 Bar = S.Grille == G_Slats ? K_Trim : K_Chrome;
            for (int32 K = 0; K < NH; ++K)
            {
                const double Z = NH == 1 ? (S.GZ0 + S.GZ1) / 2 : (S.GZ0 + 2.5) + (S.GZ1 - S.GZ0 - 5.0) * K / (NH - 1);
                Band(Bar, true, 0, S.GHW - 1, Z - .7, Z + .7, 1.2, 2);
            }
            if (S.Grille == G_Egg || S.Grille == G_Vert || S.Grille == G_Mesh)
            {
                const int32 NV = S.Grille == G_Egg ? 8 : S.Grille == G_Vert ? 14 : 16;
                for (int32 K = 1; K < NV; ++K)
                    for (int32 Side = 0; Side < 2; ++Side)
                    {
                        FVector Nn;
                        FVector P = EndPoint(true, S.GHW * K / NV, (S.GZ0 + S.GZ1) / 2, 1.0, Nn);
                        if (Side) P.Y = -P.Y;
                        Wr.Box(K_Chrome, P, FVector(.9, .55, (S.GZ1 - S.GZ0) / 2 - 1.5));
                    }
            }
            if (S.Grille != G_Slats)
            {
                Band(K_Chrome, true, 0, S.GHW + 1.6, S.GZ1, S.GZ1 + 1.8, 1.6, 3);
                Band(K_Chrome, true, 0, S.GHW + 1.6, S.GZ0 - 1.8, S.GZ0, 1.6, 3);
                for (int32 Side = 0; Side < 2; ++Side)
                {
                    FVector Nn;
                    FVector P = EndPoint(true, S.GHW + .8, (S.GZ0 + S.GZ1) / 2, .8, Nn);
                    if (Side) P.Y = -P.Y;
                    Wr.Box(K_Chrome, P, FVector(.8, .9, (S.GZ1 - S.GZ0) / 2 + 1.8));
                }
            }
        }
        for (const FCarLight& L : S.FLights)
        {
            if (L.bRound) RoundLight(true, L.Y, L.Z, L.R, L.Bezel);
            else RectLight(true, L.Y, L.Z, L.HW, L.HH);
        }
        const int32 BumperKey = S.bChromeBumper ? K_Chrome : K_Trim;
        Band(BumperKey, true, 0, S.RF * S.BWrap + FaceHalf(true, S.BF.X), S.BF.X, S.BF.Y, S.BD, 12);
        if (bHatch)
            for (int32 Side = 0; Side < 2; ++Side)
                Wr.Box(K_Amber, FVector(XF + S.BD + .3, Side ? -56 : 56, (S.BF.X + S.BF.Y) / 2), FVector(.3, 7, 3));
        // rear: lamps, plate, bumper, exhaust
        for (const FCarRear& R : S.RLights)
        {
            const double WCR = FaceHalf(false, R.Z0);
            const double U0 = R.U0 < 0 ? WCR + R.U0 : R.U0;
            const double U1 = R.U1 > 500 ? WCR + S.RRE : WCR + R.U1;
            EndPatch(R.Key, false, U0, U1, R.Z0, R.Z1, 6, true, .5);
            Band(bHatch ? K_Trim : K_Chrome, false, U0 - 1.2, U1, R.Z1, R.Z1 + 1.2, .9, 4);
            Band(bHatch ? K_Trim : K_Chrome, false, U0 - 1.2, U1, R.Z0 - 1.2, R.Z0, .9, 4);
        }
        PlateQuad();
        Band(BumperKey, false, 0, S.RRE * S.BWrap + FaceHalf(false, S.BR.X), S.BR.X, S.BR.Y, S.BD, 12);
        for (double Ye : S.Exhaust)
        {
            const double Z = Zc(XR) + 4;
            Wr.Cyl(K_Chrome, FVector(XR - 3, Ye, Z), FVector(XR + 14, Ye, Z), 3.4, 12);
            Wr.Cyl(K_Dark, FVector(XR - 3.05, Ye, Z), FVector(XR - 2.0, Ye, Z), 2.6, 12);
        }
        if (S.bOrnament)
        {
            const double Z = S.Top(XF - 8);
            Wr.Box(K_Chrome, FVector(XF - 9, 0, Z + 2.5), FVector(5, .6, 2.5));
            Wr.Box(K_Chrome, FVector(XF - 6, 0, Z + 5.5), FVector(2.2, .5, 2.0));
        }
        if (S.bAntenna)
        {
            const double Xa = AXF - S.Arch - 6, Zb = S.Belt(Xa), Ya = -(SideY(Xa, Zb) - 9);
            Wr.Cyl(K_Chrome, FVector(Xa, Ya, Zb), FVector(Xa - 6, Ya, Zb + 78), .35, 6);
            Wr.Cyl(K_Chrome, FVector(Xa, Ya, Zb - .5), FVector(Xa, Ya, Zb + 1.5), 1.6, 8);
        }
        if (S.bRoofRack)
        {
            const double Zr = S.Top(-120), Yr = Wd(-120) * S.WTop(-120) - 4;
            for (int32 Side = 0; Side < 2; ++Side)
            {
                const double Sg = Side ? -1.0 : 1.0;
                Wr.Box(K_Chrome, FVector(-125, Sg * Yr, Zr + 5), FVector(110, 1.2, 1.2));
                for (double Xl : { -20.0, -125.0, -230.0 }) Wr.Box(K_Trim, FVector(Xl, Sg * Yr, S.Top(Xl) + 2), FVector(3, 1.6, 2.6));
            }
            for (double Xl : { -60.0, -190.0 }) Wr.Box(K_Chrome, FVector(Xl, 0, S.Top(Xl) + 6.2), FVector(1.2, Yr, 1.2));
        }
        if (S.bScoop)
        {
            const double Zh = S.Top(170);
            Wr.Box(K_Dark, FVector(170, 0, Zh + 3.0), FVector(26, 15, 3.4));
            Wr.Box(K_Trim, FVector(196.5, 0, Zh + 3.4), FVector(.6, 12, 2.2));
        }
        if (Has(S.Bed))
        {
            EndPatch(K_Chrome, false, 0, 10, 112, 115, 1, true, .7);
            const double Zf = S.Sill + 26;
            for (int32 K = -4; K <= 4; ++K)
                Wr.Box(K_Dark, FVector((S.Bed.X + S.Bed.Y) / 2, K * 16.0, Zf + .6), FVector(FMath::Abs(S.Bed.Y - S.Bed.X) / 2 - 2, 1.0, .6));
        }
        for (int32 Axle = 0; Axle < 2; ++Axle)
            for (int32 Side = 0; Side < 2; ++Side)
            {
                const double Ax = Axle ? AXR : AXF, Sg = Side ? -1.0 : 1.0;
                const double Yc = Sg * (SideY(Ax, S.RW) - S.TW / 2 - 1.8);
                Wheel(FVector(Ax, Yc, S.RW), Sg, Axle == 0 ? FMath::DegreesToRadians(S.Steer) : 0.0);
            }
        if (S.bPolice) Police();
    }

    /** Police fit: roof light bar (chrome feet, black centre, red and blue domes), a spotlight on the driver's pillar,
     *  a push bar on the front bumper and POLICE in stroke lettering standing just off both front doors. */
    void Police()
    {
        const double Xb = -30, Zr = S.Top(Xb);
        for (double Yf : { -38.0, 38.0 }) Wr.Box(K_Chrome, FVector(Xb, Yf, Zr + 2.6), FVector(4, 3, 2.6));
        Wr.Box(K_Chrome, FVector(Xb, 0, Zr + 5.6), FVector(9.4, 56, .6));
        Wr.Box(K_Trim, FVector(Xb, 0, Zr + 11.8), FVector(9, 12, 5.6));
        Wr.Box(K_LightRed, FVector(Xb, -34, Zr + 11.8), FVector(9.6, 22, 6));
        Wr.Box(K_LightBlue, FVector(Xb, 34, Zr + 11.8), FVector(9.6, 22, 6));
        // Spotlight: lamp ahead of the pillar, handle through it.
        const double Xs = S.WS.Y + 6, Zs = S.Belt(Xs) + 22, Ys = -(SideY(Xs, S.Belt(Xs)) + 6);
        Wr.Cyl(K_Chrome, FVector(Xs - 2, Ys, Zs), FVector(Xs + 16, Ys, Zs), 7.4, 12);
        Wr.Cyl(K_Lens, FVector(Xs + 16, Ys, Zs), FVector(Xs + 17.2, Ys, Zs), 6.8, 12);
        Wr.Cyl(K_Chrome, FVector(Xs - 6, Ys + 14, Zs), FVector(Xs - 6, Ys - 4, Zs), 1.1, 6);
        Wr.Cyl(K_Chrome, FVector(Xs - 6, Ys - 4, Zs), FVector(Xs - 2, Ys, Zs), 1.1, 6);
        // Push bar.
        const double Xp = XF + 9, Zp0 = S.BF.X - 4, Zp1 = S.BF.Y + 24;
        for (double Yp : { -46.0, 46.0 })
        {
            Wr.Box(K_Trim, FVector(Xp, Yp, (Zp0 + Zp1) / 2), FVector(2.2, 2.2, (Zp1 - Zp0) / 2));
            Wr.Box(K_Trim, FVector(XF + 3.5, Yp, Zp0 + 12), FVector(6, 1.6, 1.6));
        }
        for (double Zp : { Zp0 + 12, Zp1 - 6 }) Wr.Box(K_Trim, FVector(Xp, 0, Zp), FVector(2.2, 47, 2.2));
        // POLICE on the front doors (seams at x 132 and -22): 12 cm letters, read left to right from either side.
        const FString Word = TEXT("POLICE");
        const double Hh = 12.0, U = Hh / 6.0, Adv = 4.0 * U + .22 * Hh, Width = Word.Len() * Adv - .22 * Hh, Z0 = 44.0;
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const double Sg = Side ? 1.0 : -1.0;        // -y is the driver's side; a viewer there reads toward -x
            const double Dir = Side ? 1.0 : -1.0;
            const double XStart = 55.0 - Dir * Width / 2;
            double Cursor = 0;
            for (TCHAR Ch : Word)
            {
                for (const TArray<FVector2D>& Line : HallGlyph(Ch))
                    for (int32 I = 0; I + 1 < Line.Num(); ++I)
                    {
                        const double XA = XStart + Dir * (Cursor + Line[I].X * U), ZA = Z0 + Line[I].Y * U;
                        const double XB = XStart + Dir * (Cursor + Line[I + 1].X * U), ZB = Z0 + Line[I + 1].Y * U;
                        const double Ym = Sg * (SideY((XA + XB) / 2, (ZA + ZB) / 2) + .5);
                        const FVector A(XA, Ym, ZA), Bv(XB, Ym, ZB);
                        FVector Ax = Bv - A;
                        const double L = Ax.Size();
                        if (L < 1e-3) continue;
                        Ax /= L;
                        const FVector Ny(0, Sg, 0), Az = FVector::CrossProduct(Ax, Ny).GetSafeNormal();
                        Wr.Box(K_Trim, (A + Bv) / 2, FVector(L / 2 + .9, .45, .9), Ax, Ny, Az);
                    }
                Cursor += Adv;
            }
        }
    }

    /** Contact shadow on the paving under the body: a dark core and a lighter rim reaching just past the body outline.
     *  The car origin sits 1 cm into the paving, so the shadow lies at local z 1.3 (about 0.4 cm above the ground). */
    void GroundShadow()
    {
        const double Z = 1.3, In = 12.0, Out = 3.0;
        auto Half = [&](double X) { return Wd(X) * S.Prof(0.0); };
        const double XA = XR + In, XB = XF - In;
        const int32 N = FMath::Max(8, (int32)((XB - XA) / 30.0));
        const FVector Up = FVector::UpVector;
        for (int32 K = 0; K < N; ++K)
        {
            const double A = XA + (XB - XA) * K / N, B = XA + (XB - XA) * (K + 1) / N;
            const double IA = FMath::Max(1.0, Half(A) - In), IB = FMath::Max(1.0, Half(B) - In);
            const double OA = Half(A) + Out, OB = Half(B) + Out;
            Wr.Quad(K_Shadow, FVector(A, -IA, Z), FVector(B, -IB, Z), FVector(B, IB, Z), FVector(A, IA, Z), Up);
            for (int32 Side = 0; Side < 2; ++Side)
            {
                const double Sg = Side ? -1.0 : 1.0;
                Wr.Quad(K_ShadowEdge, FVector(A, Sg * IA, Z), FVector(B, Sg * IB, Z), FVector(B, Sg * OB, Z), FVector(A, Sg * OA, Z), Up);
            }
        }
        // Rims across the nose and tail, with corner pieces joining them to the side rims.
        for (int32 E = 0; E < 2; ++E)
        {
            const double XI = E ? XA : XB, XO = E ? XR - Out : XF + Out;
            const double HI = FMath::Max(1.0, Half(XI) - In), HO = Half(E ? XR : XF) + Out, HS = Half(XI) + Out;
            Wr.Quad(K_ShadowEdge, FVector(XI, -HI, Z), FVector(XO, -HO, Z), FVector(XO, HO, Z), FVector(XI, HI, Z), Up);
            for (int32 Side = 0; Side < 2; ++Side)
            {
                const double Sg = Side ? -1.0 : 1.0;
                Wr.Tri(K_ShadowEdge, FVector(XI, Sg * HI, Z), FVector(XI, Sg * HS, Z), FVector(XO, Sg * HO, Z), Up);
            }
        }
    }

    void Collision(TArray<FBox>& Out) const
    {
        const double BeltLo = FMath::Min(S.Belt(0), S.Belt(XF - 30));
        Out.Add(FBox(FVector(XR - S.BD, -S.W / 2 + 2, S.Clear), FVector(XF + S.BD, S.W / 2 - 2, BeltLo)));
        const double YG = S.W * .4;
        switch (S.Type)
        {
        case EExCar::Pickup:
        {
            const double Zf = S.Sill + 26, Rail = S.Belt(-150);
            Out[0].Max.Z = Zf;
            Out.Add(FBox(FVector(-57, -S.W / 2 + 2, Zf), FVector(XF + S.BD, S.W / 2 - 2, BeltLo)));
            Out.Add(FBox(FVector(-57, -YG, BeltLo), FVector(100, YG, 178)));
            Out.Add(FBox(FVector(-253, S.W / 2 - 9, Zf), FVector(-63, S.W / 2 - 1, Rail)));
            Out.Add(FBox(FVector(-253, -S.W / 2 + 1, Zf), FVector(-63, -S.W / 2 + 9, Rail)));
            Out.Add(FBox(FVector(-260, -S.W / 2 + 1, Zf), FVector(-253, S.W / 2 - 1, S.Top(-258))));
            break;
        }
        case EExCar::Muscle:
            Out.Add(FBox(FVector(-40, -YG, BeltLo), FVector(95, YG, 122)));
            Out.Add(FBox(FVector(-150, -YG, BeltLo), FVector(-40, YG, 108)));
            break;
        case EExCar::Wagon:
            Out.Add(FBox(FVector(-272, -YG, BeltLo), FVector(100, YG, 136)));
            break;
        default:
            Out.Add(FBox(FVector(FMath::Min(S.RG.X, S.RG.Y) + 10, -YG, BeltLo), FVector(S.WS.X - 15, YG, S.Top(-30) - 3)));
            break;
        }
    }
};

// ------------------------------------------------------------------ textures
void PaintPlates(FHallImage& Img)
{
    const int32 W = 512, H = 256;
    Img.Init(W, H, FLinearColor(.86f, .86f, .82f));
    const TCHAR* Texts[4] = { TEXT("PSK 1977"), TEXT("VOL 1969"), TEXT("SRS 1983"), TEXT("SND 1958") };
    for (int32 P = 0; P < 4; ++P)
    {
        const float X0 = (P % 2) * 256.f, Y0 = (P / 2) * 128.f;
        Img.Rect(FVector2D(X0 + 4, Y0 + 4), FVector2D(X0 + 252, Y0 + 124), FLinearColor(.08f, .14f, .38f));
        Img.Rect(FVector2D(X0 + 9, Y0 + 9), FVector2D(X0 + 247, Y0 + 119), FLinearColor(.88f, .88f, .84f));
        FHallTextStyle T; T.Height = 40.f; T.Stroke = 6.5f; T.Spacing = .15f; T.WidthScale = .8f;   // 8 characters fit inside the border
        const float TW = HallTextWidth(Texts[P], T);
        HallDrawText(Img, Texts[P], FVector2D(X0 + 128 - TW * .5f, Y0 + 92), T, FLinearColor(.06f, .1f, .32f));
        FHallTextStyle S2; S2.Height = 14.f; S2.Stroke = 2.5f; S2.Spacing = .3f;
        const float SW = HallTextWidth(TEXT("CLASSIC"), S2);
        HallDrawText(Img, TEXT("CLASSIC"), FVector2D(X0 + 128 - SW * .5f, Y0 + 28), S2, FLinearColor(.5f, .1f, .08f));
    }
}

void PaintWood(FHallImage& Img)
{
    const int32 N = 256;
    Img.Init(N, N, FLinearColor::Black);
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const float Warp = HallFbm(X * .012f, Y * .06f, 3, 71) * 26.f;
            const float Grain = .5f + .5f * FMath::Sin((Y + Warp) * .55f);
            const float Fine = HallNoise(X * .9f, Y * 2.2f, 73);
            const float Plank = (Y % 64) < 2 ? .55f : 1.f;
            const float L = (.65f + .35f * Grain) * (.85f + .25f * Fine) * Plank;
            Img.At(X, Y) = FLinearColor(.30f * L, .15f * L, .062f * L);
        }
}
void CreateLook(UHouseBlackHallSubsystem* Sub)
{
    if (!Sub || Sub->Materials.Contains(TEXT("CAR_Chrome"))) return;
    // Body paint uses the project's clear-coat car material when it is present (glossy lacquer over the colour coat).
    UMaterialInterface* ClearCoat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/ExteriorCars/M_CarPaint.M_CarPaint"));
    auto Paint = [&](FName Key, const FLinearColor& C, float Rough, float Metal)
    {
        if (ClearCoat)
            if (UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(ClearCoat, Sub))
            {
                M->SetVectorParameterValue(TEXT("Tint"), C);
                M->SetScalarParameterValue(TEXT("Roughness"), Rough);
                M->SetScalarParameterValue(TEXT("Metallic"), Metal);
                M->SetVectorParameterValue(TEXT("Emission"), FLinearColor::Black);
                Sub->Materials.Add(Key, M);
                return;
            }
        HallMakePlain(Sub, Key, C, Rough * .8f, Metal);
    };
    auto Plain = [&](const TCHAR* Key, const FLinearColor& C, float Rough, float Metal = 0.f, const FLinearColor& Em = FLinearColor::Black)
    { HallMakePlain(Sub, FName(Key), C, Rough, Metal, Em); };
    Paint(TEXT("CAR_Paint"), FLinearColor(.2f, .025f, .03f), .38f, .2f);
    Paint(TEXT("CAR_Paint2"), FLinearColor(.8f, .8f, .76f), .4f, .05f);
    Paint(TEXT("CAR_WheelPaint"), FLinearColor(.2f, .025f, .03f), .45f, .2f);
    Plain(TEXT("CAR_Vinyl"), FLinearColor(.018f, .018f, .018f), .78f);
    Plain(TEXT("CAR_Glass"), FLinearColor(.004f, .005f, .006f), .03f, 0.f);
    Plain(TEXT("CAR_Chrome"), FLinearColor(.86f, .87f, .89f), .06f, 1.f);
    Plain(TEXT("CAR_Trim"), FLinearColor(.02f, .02f, .02f), .55f);
    Plain(TEXT("CAR_Dark"), FLinearColor(.006f, .006f, .006f), .8f);
    Plain(TEXT("CAR_Tire"), FLinearColor(.022f, .022f, .022f), .9f);
    Plain(TEXT("CAR_Whitewall"), FLinearColor(.74f, .74f, .71f), .7f);
    Plain(TEXT("CAR_Rim"), FLinearColor(.6f, .61f, .63f), .3f, .9f);
    Plain(TEXT("CAR_Mag"), FLinearColor(.18f, .185f, .19f), .35f, .85f);
    Plain(TEXT("CAR_Lens"), FLinearColor(.75f, .77f, .8f), .05f, .2f);
    Plain(TEXT("CAR_Tail"), FLinearColor(.35f, .01f, .01f), .12f);
    Plain(TEXT("CAR_Amber"), FLinearColor(.7f, .3f, .01f), .12f);
    Plain(TEXT("CAR_WoodTrim"), FLinearColor(.5f, .38f, .22f), .45f);
    Plain(TEXT("CAR_Disc"), FLinearColor(.12f, .12f, .13f), .5f, .7f);
    Plain(TEXT("CAR_Shadow"), FLinearColor(.003f, .003f, .003f), 1.f);
    Plain(TEXT("CAR_ShadowEdge"), FLinearColor(.022f, .022f, .023f), 1.f);
    // Police light-bar domes, lit (emissive only: no lights are added for them).
    Plain(TEXT("CAR_LightRed"), FLinearColor(.6f, .02f, .01f), .12f, 0.f, FLinearColor(30.f, 1.4f, .5f));
    Plain(TEXT("CAR_LightBlue"), FLinearColor(.02f, .05f, .5f), .12f, 0.f, FLinearColor(1.2f, 4.f, 40.f));
    {
        FHallImage Img; PaintPlates(Img);
        UTexture2D* T = HallMakeTexture(Img, true, false, TEXT("T_CAR_Plate"), false);
        if (T) { Sub->Textures.Add(TEXT("T_CAR_Plate"), T); HallMakeTextured(Sub, TEXT("CAR_Plate"), T, nullptr, .45f, FLinearColor::White, 0.f); }
        else Plain(TEXT("CAR_Plate"), FLinearColor(.8f, .8f, .76f), .5f);
    }
    {
        FHallImage Img; PaintWood(Img);
        UTexture2D* T = HallMakeTexture(Img, true, false, TEXT("T_CAR_Wood"), true);
        if (T) { Sub->Textures.Add(TEXT("T_CAR_Wood"), T); HallMakeTextured(Sub, TEXT("CAR_Wood"), T, nullptr, .45f, FLinearColor::White, 0.f); }
        else Plain(TEXT("CAR_Wood"), FLinearColor(.3f, .15f, .06f), .5f);
    }
    // Period paint colours (enamel and a few metallics) and vinyl roof colours, swapped onto the cars per paint scheme.
    struct FPaint { const TCHAR* Name; FLinearColor C; float Rough, Metal; bool bVinyl; };
    const FPaint Paints[] = {
        { TEXT("Maroon"), FLinearColor(.20f, .025f, .03f), .38f, .25f, false }, { TEXT("Navy"), FLinearColor(.03f, .05f, .14f), .38f, .4f, false },
        { TEXT("Cream"), FLinearColor(.70f, .62f, .44f), .42f, .05f, false }, { TEXT("DarkGreen"), FLinearColor(.03f, .11f, .06f), .38f, .4f, false },
        { TEXT("Brown"), FLinearColor(.13f, .07f, .035f), .4f, .45f, false }, { TEXT("Beige"), FLinearColor(.55f, .47f, .33f), .42f, .1f, false },
        { TEXT("BabyBlue"), FLinearColor(.30f, .45f, .62f), .4f, .1f, false }, { TEXT("HarvestGold"), FLinearColor(.45f, .30f, .06f), .42f, .15f, false },
        { TEXT("Red"), FLinearColor(.45f, .03f, .025f), .38f, .2f, false }, { TEXT("Avocado"), FLinearColor(.16f, .20f, .06f), .42f, .15f, false },
        { TEXT("White"), FLinearColor(.78f, .78f, .75f), .4f, .05f, false }, { TEXT("Silver"), FLinearColor(.45f, .46f, .48f), .4f, .85f, false },
        { TEXT("Turquoise"), FLinearColor(.10f, .38f, .38f), .38f, .15f, false }, { TEXT("Coral"), FLinearColor(.70f, .25f, .18f), .38f, .1f, false },
        { TEXT("Mint"), FLinearColor(.40f, .62f, .50f), .4f, .08f, false }, { TEXT("Orange"), FLinearColor(.80f, .22f, .02f), .32f, .25f, false },
        { TEXT("Black"), FLinearColor(.012f, .012f, .014f), .3f, .3f, false },
        { TEXT("VinylBlack"), FLinearColor(.018f, .018f, .018f), .78f, 0.f, true }, { TEXT("VinylWhite"), FLinearColor(.66f, .66f, .62f), .75f, 0.f, true },
        { TEXT("VinylBrown"), FLinearColor(.09f, .055f, .03f), .75f, 0.f, true },
    };
    for (const FPaint& P : Paints)
    {
        const FName Key(*FString::Printf(TEXT("CARP_%s"), P.Name));
        if (P.bVinyl) HallMakePlain(Sub, Key, P.C, P.Rough, P.Metal);
        else Paint(Key, P.C, P.Rough, P.Metal);
    }
    UE_LOG(LogTemp, Display, TEXT("HOUSE_CLASSIC_CARS look ready clearCoat=%d paints=%d"), ClearCoat ? 1 : 0, (int32)UE_ARRAY_COUNT(Paints));
}

void BuildCar(EExCar Type, FHallMesh& Mesh, TArray<FBox>& Collision)
{
    const FCarSpec Spec = SpecFor(Type);
    FCarBuilder B(Spec, Mesh);
    B.Body();
    B.Details();
    B.GroundShadow();
    B.Collision(Collision);
}
} // namespace ExClassicCarsImpl

// ------------------------------------------------------------------ public
void ExCreateCarLook(UHouseBlackHallSubsystem* Sub)
{
    ExClassicCarsImpl::CreateLook(Sub);
}

const TArray<FExCarScheme>& ExCarSchemes()
{
    static const TArray<FExCarScheme> Schemes = []
    {
        TArray<FExCarScheme> A;
        auto Key = [](const TCHAR* N) { return N ? FName(*FString::Printf(TEXT("CARP_%s"), N)) : FName(NAME_None); };
        auto Add = [&](EExCar T, const TCHAR* Paint, const TCHAR* Paint2, const TCHAR* Vinyl, const TCHAR* Wheel)
        {
            FExCarScheme S; S.Type = T; S.Paint = Key(Paint); S.Paint2 = Key(Paint2); S.Vinyl = Key(Vinyl); S.WheelPaint = Key(Wheel); A.Add(S);
        };
        Add(EExCar::Yacht, TEXT("Maroon"), nullptr, TEXT("VinylBlack"), nullptr);
        Add(EExCar::Yacht, TEXT("Navy"), nullptr, TEXT("VinylWhite"), nullptr);
        Add(EExCar::Yacht, TEXT("Cream"), nullptr, TEXT("VinylBrown"), nullptr);
        Add(EExCar::Yacht, TEXT("DarkGreen"), nullptr, TEXT("VinylBlack"), nullptr);
        Add(EExCar::Wagon, TEXT("Brown"), nullptr, nullptr, nullptr);
        Add(EExCar::Wagon, TEXT("Beige"), nullptr, nullptr, nullptr);
        Add(EExCar::Wagon, TEXT("BabyBlue"), nullptr, nullptr, nullptr);
        Add(EExCar::Pickup, TEXT("HarvestGold"), TEXT("Cream"), nullptr, TEXT("White"));
        Add(EExCar::Pickup, TEXT("Red"), TEXT("White"), nullptr, TEXT("White"));
        Add(EExCar::Pickup, TEXT("Avocado"), TEXT("Cream"), nullptr, TEXT("Avocado"));
        Add(EExCar::Hatch, TEXT("Red"), nullptr, nullptr, nullptr);
        Add(EExCar::Hatch, TEXT("White"), nullptr, nullptr, nullptr);
        Add(EExCar::Hatch, TEXT("Silver"), nullptr, nullptr, nullptr);
        Add(EExCar::Hatch, TEXT("BabyBlue"), nullptr, nullptr, nullptr);
        Add(EExCar::Fifties, TEXT("Turquoise"), TEXT("White"), nullptr, nullptr);
        Add(EExCar::Fifties, TEXT("Coral"), TEXT("White"), nullptr, nullptr);
        Add(EExCar::Fifties, TEXT("Mint"), TEXT("White"), nullptr, nullptr);
        Add(EExCar::Muscle, TEXT("Orange"), TEXT("White"), nullptr, nullptr);
        Add(EExCar::Cruiser, TEXT("Black"), TEXT("White"), nullptr, TEXT("Black"));
        return A;
    }();
    return Schemes;
}

const TCHAR* ExCarTypeName(EExCar Type)
{
    static const TCHAR* Names[] = { TEXT("Yacht"), TEXT("Wagon"), TEXT("Pickup"), TEXT("Hatch"), TEXT("Fifties"), TEXT("Muscle"), TEXT("Cruiser") };
    return Names[FMath::Clamp((int32)Type, 0, (int32)UE_ARRAY_COUNT(Names) - 1)];
}

void ExBuildCar(EExCar Type, FHallMesh& Mesh, TArray<FBox>& Collision)
{
    ExClassicCarsImpl::BuildCar(Type, Mesh, Collision);
}

float ExCarLength(EExCar Type)
{
    return (float)ExClassicCarsImpl::SpecFor(Type).L;
}

float ExCarFrontTyreReach(EExCar Type, float Height)
{
    const ExClassicCarsImpl::FCarSpec S = ExClassicCarsImpl::SpecFor(Type);
    const double R = S.RW, D = FMath::Clamp((double)Height, 0.0, R);
    return (float)(S.L / 2 - S.FO + FMath::Sqrt(FMath::Max(0.0, R * R - (R - D) * (R - D))));
}
