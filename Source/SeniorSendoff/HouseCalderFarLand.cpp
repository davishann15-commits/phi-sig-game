// The Calder outer land (see HouseCalderFarLand.h): landform.py's "THE OUTER LAND (Far)" ported line for line, and its
// self-test (-CalderFarLandSelfTest: evaluates the port on the 2000 points Saved/CalderWork/plan/farland_port_check.py
// wrote to farland_expected.csv, logs CALDER_FARLAND_SELFTEST maxErr=... and ends the run).
//
// Porting rules (landform.py's header): doubles only; + - * / sqrt floor and the polynomial smoothstep, evaluated in the
// python's order (the module builds with -ffp-contract=off, so a*b+c is never fused); the only noise is hash32/vnoise/fbm
// in uint32 arithmetic (wrapping), lattice coordinates floor()ed then taken mod 2^32. The polyline arc lengths are the
// python's own values (python's hypot is not libm's). The one place the port differs ON PURPOSE is the seam: the
// python's seam_value() is its analytic walkable ground, the game's is HouseExteriorSite::GroundHeight (the baked 2 m
// lattice, the authority); the port check emulates that seam in python, so both sides use the same one. The walls' steps
// on the seam (SeamSteps) are found at first use by the same bisection the python runs, on the game's own seam.
#include "HouseCalderFarLand.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorSiteLayout.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace HouseCalderFarLandImpl
{
/** The walkable ground at a seam point (|X| or |Y| = 32000): landform.py's seam_value() after the clamp. */
double SeamGround(double X, double Y);
}

// ==================================================================================== BEGIN PORTABLE MATH (landform.py)
// (Plain C++ on double/int64/uint32 and FMath only, so it can be compiled and checked outside the engine.)
namespace HouseCalderFarLandImpl
{
constexpr double WORLD = 32000.0;
constexpr double GF = -60.0;
constexpr double GR = -362.0;
constexpr double RIVER_Z = -2400.0;
constexpr double WATER_GAP_X = 60000.0;
constexpr double HEAD_Z = -470.0;
constexpr double CREEK_WZ0 = 1700.0;
constexpr double GK_X = 2400.0 + 110000.0 * -0.906307787;    // Gosling Knob: yaw 205 from the frat-lawn eye, 1.1 km out
constexpr double GK_Y = -3750.0 + 110000.0 * -0.422618262;
constexpr double GK_Z = GF + 38000.0;                          // its summit, 380 m above the campus datum
constexpr double GK_R = 73000.0;                               // its foot (river level), 730 m round the summit
constexpr double GK_CAP = 0.15;                                // SL's rounding: the summit dome and the foot, 15 % each
constexpr double GK_SH_X = GK_X + 38000.0 * 0.422618262;       // its shoulder, 380 m SSE toward the near ridge
constexpr double GK_SH_Y = GK_Y + 38000.0 * -0.906307787;
constexpr double GK_SH_Z = GF + 30000.0;
constexpr double GK_SH_R = 58000.0;
constexpr double SEAM_FAN = 1.0;              // off the seam a wall's step fans out over 2 * SEAM_FAN * d beyond the wall
constexpr double SEAM_FAN_MAX = 15000.0;      // ... up to 300 m wide
constexpr double SEAM_STEP_PAD = 0.05;        // a wall's step is taken across 1.1 mm round it
constexpr double NOTCH_SOFT = 1000.0;         // NE_NOTCH's and FAR_ROAD's arc lengths turn across 10 m inside a bend
constexpr double WATER_TOWER_X = 10500.0, WATER_TOWER_Y = -50500.0;
constexpr double WATER_TOWER_Z = GF + 260.0;
constexpr double ROAD_SPUR_X = 13600.0, ROAD_SPUR_Y = 35000.0;
constexpr double PI_D = 3.141592653589793;   // python's math.pi

struct FPolyline { const double (*P)[2]; const double* Cum; int32 Num; };
// The Calder Branch valley (NE_NOTCH) and Calder Dr beyond the edge (FAR_ROAD), with the python's arc lengths.
constexpr double NeNotchPts[4][2] = { { 29400.0, 32000.0 }, { 36000.0, 42000.0 }, { 60000.0, 62000.0 }, { 100000.0, 82000.0 } };
constexpr double NeNotchCum[4] = { 0.0, 11981.652640600127, 43222.651344226746, 87944.01089422253 };
constexpr double FarRoadPts[6][2] = { { 9500.0, 32000.0 }, { 9550.0, 34500.0 }, { 10300.0, 36700.0 }, { 12400.0, 38300.0 }, { 16500.0, 39600.0 }, { 24000.0, 41500.0 } };
constexpr double FarRoadCum[6] = { 0.0, 2500.4999500099975, 4824.82780984389, 7464.903566332707, 11766.06619985402, 19502.9906534079 };
// The town's valley profile (TOWN: s = -Y), parabolic vertical curves of up to 900 cm at the inner knots.
constexpr double TownKnots[9][2] = { { 32000.0, -350.0 }, { 33500.0, -1400.0 }, { 36000.0, -1650.0 }, { 39700.0, -1900.0 }, { 43500.0, -2200.0 },
    { 44400.0, -2340.0 }, { 45000.0, -2400.0 }, { 45600.0, -2340.0 }, { 47000.0, -2250.0 } };
constexpr double TownVC = 900.0;
// The declared walls that reach the seam (landform.py SEAM_WALLS): the side (0 S, 1 N, 2 W, 3 E) and a bracket holding the step.
struct FSeamWall { int32 Side; double Lo, Hi; };
constexpr FSeamWall SeamWalls[5] = { { 0, 25400.0, 25600.0 }, { 0, 29200.0, 29400.0 }, { 1, 10600.0, 10700.0 }, { 2, -30100.0, -29900.0 }, { 3, -30100.0, -29900.0 } };

// ------------------------------------------------------------------ maths (landform.py "maths (port 1:1)")
inline double Clamp(double V, double A, double B) { return V < A ? A : (V > B ? B : V); }
inline double PyMax(double A, double B) { return B > A ? B : A; }   // python max(a, b): the first of equals
inline double PyMin(double A, double B) { return B < A ? B : A; }

inline double S(double T)
{
    if (T <= 0.0) return 0.0;
    if (T >= 1.0) return 1.0;
    return T * T * (3.0 - 2.0 * T);
}
inline double Ramp(double V, double A, double B) { return S((V - A) / (B - A)); }
inline double Lerp(double A, double B, double T) { return A + (B - A) * T; }
inline double SMin(double A, double B, double K)
{
    const double H = Clamp(0.5 + 0.5 * (B - A) / K, 0.0, 1.0);
    return B + (A - B) * H - K * H * (1.0 - H);
}
inline double SMax(double A, double B, double K) { return -SMin(-A, -B, K); }
inline double RampIn(double T, double A)
{
    if (T <= 0.0) return 0.0;
    if (T >= 1.0) T = 1.0;
    const double V = T < A ? T * T / (2.0 * A) : T - 0.5 * A;
    return V / (1.0 - 0.5 * A);
}
/** landform.py SL: C1 'smooth-linear' 0..1, parabolic for the first and last fraction A, straight between. */
inline double SL(double T, double A)
{
    if (T <= 0.0) return 0.0;
    if (T >= 1.0) return 1.0;
    const double K = 1.0 / (2.0 * A * (1.0 - A));
    if (T < A) return K * T * T;
    if (T > 1.0 - A) return 1.0 - K * (1.0 - T) * (1.0 - T);
    return (T - 0.5 * A) / (1.0 - A);
}
inline void SegProj(double PX, double PY, double AX, double AY, double BX, double BY, double& OutT, double& OutD)
{
    const double DX = BX - AX, DY = BY - AY;
    const double L2 = DX * DX + DY * DY;
    double T = 0.0;
    if (L2 > 0.0) T = Clamp(((PX - AX) * DX + (PY - AY) * DY) / L2, 0.0, 1.0);
    const double QX = AX + DX * T - PX, QY = AY + DY * T - PY;
    OutT = T;
    OutD = FMath::Sqrt(QX * QX + QY * QY);
}
inline double EllipseRho(double PX, double PY, double CX, double CY, double RX, double RY)
{
    const double UX = (PX - CX) / RX, UY = (PY - CY) / RY;
    return FMath::Sqrt(UX * UX + UY * UY);
}

inline uint32 Hash32(int64 IX, int64 IY, int64 Seed)
{
    uint32 H = uint32(IX) * 0x8DA6B343u + uint32(IY) * 0xD8163841u + uint32(Seed) * 0xCB1AB31Fu + 0x9E3779B9u;
    H ^= H >> 15;
    H *= 0x2C1B3C6Du;
    H ^= H >> 12;
    H *= 0x297A2D39u;
    H ^= H >> 15;
    return H;
}
inline double HVal(int64 IX, int64 IY, int64 Seed) { return double(Hash32(IX, IY, Seed)) / 4294967295.0 * 2.0 - 1.0; }

/** C1 value noise in [-1, 1]: lattice values from Hash32, smoothstep-interpolated. */
double VNoise(double X, double Y, double CellSize, int64 Seed)
{
    const double FX = X / CellSize, FY = Y / CellSize;
    const double FX0 = FMath::FloorToDouble(FX), FY0 = FMath::FloorToDouble(FY);
    const double TX = S(FX - FX0), TY = S(FY - FY0);
    const int64 IX = int64(FX0), IY = int64(FY0);
    const double A = HVal(IX, IY, Seed), B = HVal(IX + 1, IY, Seed), C = HVal(IX, IY + 1, Seed), D = HVal(IX + 1, IY + 1, Seed);
    return Lerp(Lerp(A, B, TX), Lerp(C, D, TX), TY);
}

double Fbm(double X, double Y, double CellSize, int32 Octaves, int64 Seed)
{
    double Sum = 0.0, Amp = 1.0, Norm = 0.0;
    for (int32 O = 0; O < Octaves; ++O)
    {
        Sum += Amp * VNoise(X, Y, CellSize, Seed + O * 1013);
        Norm += Amp;
        Amp *= 0.5;
        CellSize *= 0.5;
    }
    return Sum / Norm;
}

/** landform.py Profile.__call__ for the town's knots. */
double Town(double Sv)
{
    constexpr int32 Num = 9;
    if (Sv <= TownKnots[0][0]) return TownKnots[0][1];
    if (Sv >= TownKnots[Num - 1][0]) return TownKnots[Num - 1][1];
    int32 I = 0;
    while (I < Num - 2 && Sv > TownKnots[I + 1][0]) ++I;
    const double S0 = TownKnots[I][0], Z0 = TownKnots[I][1], S1 = TownKnots[I + 1][0], Z1 = TownKnots[I + 1][1];
    double Z = Z0 + (Z1 - Z0) * (Sv - S0) / (S1 - S0);
    for (int32 J = I; J <= I + 1; ++J)
    {
        if (!(0 < J && J < Num - 1)) continue;
        const double SA = TownKnots[J - 1][0], ZA = TownKnots[J - 1][1];
        const double SB = TownKnots[J][0], ZB = TownKnots[J][1];
        const double SC = TownKnots[J + 1][0], ZC = TownKnots[J + 1][1];
        const double G1 = (ZB - ZA) / (SB - SA), G2 = (ZC - ZB) / (SC - SB);
        const double Len = PyMin(PyMin(TownVC, 0.9 * (SB - SA)), 0.9 * (SC - SB));
        const double U = Sv - (SB - 0.5 * Len);
        if (0.0 <= U && U <= Len)
        {
            const double Lin = ZB + (Sv <= SB ? G1 : G2) * (Sv - SB);
            const double Par = (ZB - G1 * 0.5 * Len) + G1 * U + (G2 - G1) * U * U / (2.0 * Len);
            Z += Par - Lin;
        }
    }
    return Z;
}

/** landform.py polyline_soft: distance of the nearest point and a continuous arc length (every segment's own clamped
 *  projection weighted 1 - S((D_i - D) / K) by how much further it lies than the nearest, averaged). */
void PolylineSoft(const FPolyline& Line, double X, double Y, double K, double& OutD, double& OutS)
{
    double SegD[8], SegS[8];
    double Best = 1e30;
    for (int32 I = 0; I + 1 < Line.Num; ++I)
    {
        double T, D;
        SegProj(X, Y, Line.P[I][0], Line.P[I][1], Line.P[I + 1][0], Line.P[I + 1][1], T, D);
        SegD[I] = D;
        SegS[I] = Line.Cum[I] + T * (Line.Cum[I + 1] - Line.Cum[I]);
        if (D < Best) Best = D;
    }
    double SW = 0.0, WS = 0.0;
    for (int32 I = 0; I + 1 < Line.Num; ++I)
    {
        const double W = 1.0 - S((SegD[I] - Best) / K);
        SW += W * SegS[I];
        WS += W;
    }
    OutD = Best;
    OutS = SW / WS;
}
const FPolyline NeNotchLine{ NeNotchPts, NeNotchCum, 4 };
const FPolyline FarRoadLine{ FarRoadPts, FarRoadCum, 6 };

// ------------------------------------------------------------------ THE OUTER LAND (landform.py, same names)
double SeamValue(double X, double Y) { return SeamGround(Clamp(X, -WORLD, WORLD), Clamp(Y, -WORLD, WORLD)); }

/** landform.py seam_point: seam_value at T along one side (0 S, 1 N: T is X; 2 W, 3 E: T is Y). */
double SeamAt(int32 Side, double T)
{
    if (Side == 0) return SeamValue(T, -WORLD);
    if (Side == 1) return SeamValue(T, WORLD);
    if (Side == 2) return SeamValue(-WORLD, T);
    return SeamValue(WORLD, T);
}

/** landform.py seam_steps: each SeamWalls bracket halved toward its step (the half that changes more) down to 0.1 mm,
 *  padded by SEAM_STEP_PAD each way; ZLo = the seam at Lo, H = the seam at Hi - ZLo. Found once, on this seam (the
 *  lattice), as the python finds its own. */
struct FSeamStep { int32 Side; double Lo, Hi, ZLo, H; };
struct FSeamSteps { FSeamStep Step[5]; };
const FSeamSteps& SeamSteps()
{
    static const FSeamSteps Steps = []
    {
        FSeamSteps Out;
        for (int32 K = 0; K < 5; ++K)
        {
            const int32 Side = SeamWalls[K].Side;
            double Lo = SeamWalls[K].Lo, Hi = SeamWalls[K].Hi;
            double FLo = SeamAt(Side, Lo), FHi = SeamAt(Side, Hi);
            while (Hi - Lo > 0.01)
            {
                const double Mid = 0.5 * (Lo + Hi);
                const double FM = SeamAt(Side, Mid);
                if (FMath::Abs(FM - FLo) >= FMath::Abs(FHi - FM)) { Hi = Mid; FHi = FM; }
                else { Lo = Mid; FLo = FM; }
            }
            Lo -= SEAM_STEP_PAD;
            Hi += SEAM_STEP_PAD;
            const double ZLo = SeamAt(Side, Lo);
            Out.Step[K] = FSeamStep{ Side, Lo, Hi, ZLo, SeamAt(Side, Hi) - ZLo };
        }
        return Out;
    }();
    return Steps;
}

/** landform.py seam_side: the seam along one side at T, D (> 0) from it, continuous: across a wall's [Lo, Hi] its value
 *  at Lo, beyond Hi the step fanned out over [Hi, Hi + 2 W] (W = SEAM_FAN * D, at most SEAM_FAN_MAX). */
double SeamSide(int32 Side, double T, double D)
{
    const FSeamSteps& Steps = SeamSteps();
    bool bIn = false;
    double Z = 0.0;
    for (const FSeamStep& St : Steps.Step)
        if (St.Side == Side && St.Lo < T && T < St.Hi) { Z = St.ZLo; bIn = true; }
    if (!bIn) Z = SeamAt(Side, T);
    const double W = PyMin(SEAM_FAN * D, SEAM_FAN_MAX);
    for (const FSeamStep& St : Steps.Step)
        if (St.Side == Side) Z += St.H * (S((T - St.Hi) / (2.0 * W)) - (T >= St.Hi ? 1.0 : 0.0));
    return Z;
}

/** landform.py seam_fan: the walkable ground extruded from the seam, continuous off it; beyond a corner the two sides'
 *  values there blended by direction. */
double SeamFan(double X, double Y)
{
    const double AX = FMath::Abs(X), AY = FMath::Abs(Y);
    const double DX = AX > WORLD ? AX - WORLD : 0.0;
    const double DY = AY > WORLD ? AY - WORLD : 0.0;
    const int32 SX = X < 0.0 ? 2 : 3;
    const int32 SY = Y < 0.0 ? 0 : 1;
    if (DX > 0.0 && DY > 0.0)
    {
        const double D = FMath::Sqrt(DX * DX + DY * DY);
        return Lerp(SeamSide(SX, Clamp(Y, -WORLD, WORLD), D), SeamSide(SY, Clamp(X, -WORLD, WORLD), D), DY / (DX + DY));
    }
    if (DX > 0.0) return SeamSide(SX, Y, DX);
    if (DY > 0.0) return SeamSide(SY, X, DY);
    return SeamValue(X, Y);
}

double TownFloor(double X, double Y) { return Town(-Y) - 0.001 * X * Ramp(Y, -36000.0, -44400.0); }

double NearRidge(double X, double Y)
{
    if (Y > -45600.0) return -1e9;
    const double YC = -89000.0 + 4000.0 * VNoise(X, 0.0, 26000.0, 4101);
    const double H = 30500.0 + 1700.0 * Fbm(X, 0.0, 22000.0, 2, 4102);
    const double Wid = 2000.0 + 0.30 * PyMax(0.0, -45600.0 - Y);
    const double Gap = 1.0 - 0.93 * (1.0 - Ramp(FMath::Abs(X - WATER_GAP_X - 0.12 * (-45600.0 - Y)), 0.3 * Wid, Wid));
    const double T = (-45600.0 - Y) / (-45600.0 - YC);
    double Shape;
    if (T <= 1.0)
    {
        const double TT = Clamp(T, 0.0, 1.0);
        Shape = 0.55 * TT + 0.45 * S(TT);
        Shape += 0.10 * Fbm(X, Y, 9000.0, 2, 4103) * 4.0 * TT * (1.0 - TT);
    }
    else
        Shape = 1.0 - 0.35 * S((T - 1.0) / 0.6);
    return RIVER_Z + H * Shape * Gap;
}

/** landform.py ridge_band_w: Z and (OutW) the weight that fades it in over the outer tenth of its half width. */
double RidgeBand(double X, double Y, double YC0, double AmpY, double H0, double AmpH, double Width, int64 Seed, double& OutW)
{
    const double YC = YC0 + AmpY * VNoise(X, 0.0, 40000.0, Seed);
    const double U = FMath::Abs(Y - YC) / Width;
    if (U >= 1.0) { OutW = 0.0; return -1e9; }
    const double H = H0 + AmpH * Fbm(X, 0.0, 30000.0, 2, Seed + 1);
    OutW = Ramp(U, 1.0, 0.9);
    return RIVER_Z + H * (1.0 - S(U));
}

double CoveWall(double X, double Y, double Base)
{
    const double AX = FMath::Abs(X);
    if (AX <= WORLD) return -1e9;
    const int32 Side = X > 0.0 ? 1 : -1;
    const double Toe = WORLD + 30000.0 * Ramp(Y, -33000.0, -42000.0);
    if (AX <= Toe) return -1e9;
    const double D = 65000.0 + 25000.0 * VNoise(Y, 1000.0 * Side, 30000.0, 4201 + Side);
    const double Crest = WORLD + D + 30000.0 * Ramp(Y, -33000.0, -42000.0);
    const double Hc = GF + 21500.0 + 6500.0 * Fbm(Y, 2000.0 * Side, 24000.0, 2, 4211 + Side);
    const double U = (AX - Toe) / (Crest - Toe);
    double W;
    if (U <= 1.0)
    {
        W = 0.35 * U + 0.65 * S(U);
        W += 0.12 * Fbm(X, Y, 7000.0, 2, 4221) * 4.0 * U * (1.0 - U);
    }
    else
        W = 1.0 - 0.4 * S((U - 1.0) / 0.6);
    double Z = Base + (Hc - Base) * W;
    Z = Lerp(Z, -1e5, Ramp(Y, -62000.0, -76000.0));
    return Z;
}

/** landform.py water_tower_knob: Z and (OutW) the weight that fades it in over its outer 15 %. */
double WaterTowerKnob(double X, double Y, double& OutW)
{
    const double R = EllipseRho(X, Y, WATER_TOWER_X, WATER_TOWER_Y, 3200.0, 2600.0);
    if (R >= 1.0) { OutW = 0.0; return -1e9; }
    OutW = Ramp(R, 1.0, 0.85);
    return Lerp(WATER_TOWER_Z, RIVER_Z, Ramp(R, 0.25, 1.0));
}

/** landform.py gosling_knob: the broad knob closing the town valley's west end (a rounded summit dome, 33-deg flanks to
 *  river level 730 m out, a shoulder 380 m SSE, the notch where the two domes meet); OutW fades it in over the outer 15 %
 *  of either foot. */
double GoslingKnob(double X, double Y, double& OutW)
{
    const double R = EllipseRho(X, Y, GK_X, GK_Y, GK_R, GK_R);
    const double RS = EllipseRho(X, Y, GK_SH_X, GK_SH_Y, GK_SH_R, GK_SH_R);
    if (R >= 1.0 && RS >= 1.0) { OutW = 0.0; return -1e9; }
    const double K = RIVER_Z + (GK_Z - RIVER_Z) * (1.0 - SL(R, GK_CAP));
    const double Sh = RIVER_Z + (GK_SH_Z - RIVER_Z) * (1.0 - SL(RS, GK_CAP));
    OutW = PyMax(Ramp(R, 1.0, 1.0 - GK_CAP), Ramp(RS, 1.0, 1.0 - GK_CAP));
    return SMax(K, Sh, 1500.0);
}

double NeNotch(double X, double Y, double Z, double FloorAdd)
{
    if (Y < 26000.0 || X < 20000.0) return Z;
    double DN, SN;
    PolylineSoft(NeNotchLine, X, Y, NOTCH_SOFT, DN, SN);
    const double Floor = CREEK_WZ0 + 0.02 * SN + FloorAdd;
    return SMin(Z, Floor + DN * 0.45, 2500.0);
}

/** landform.py outer_floor: the floor every side rises from (E = SeamFan(X, Y)). */
double OuterFloor(double X, double Y, double E)
{
    if (Y > WORLD) return E + 6000.0 * RampIn((Y - WORLD) / 40000.0, 0.3);
    if (Y < -WORLD) return Lerp(E, TownFloor(X, Y), Ramp(Y, -32000.0, -33500.0));
    return E;
}

double NorthBase(double X)
{
    if (FMath::Abs(X) <= WORLD) return SeamValue(X, WORLD);
    const double Base = SeamValue(X, WORLD);
    const double CW = CoveWall(X, WORLD, Base);
    return NeNotch(X, WORLD, PyMax(Base, CW), 600.0);
}

/** landform.py outer_at: the outer land with no seam blend, given E = SeamFan(X, Y). */
double OuterAt(double X, double Y, double E)
{
    double Z = OuterFloor(X, Y, E);
    if (FMath::Abs(X) > WORLD)
    {
        const double CW = CoveWall(X, Y, Z);   // rises from the floor itself, so it meets it at its toe
        if (CW > Z) Z = CW;
    }
    if (Y > 26000.0)
    {
        // the Calder Branch valley (in full north of the square, faded in up the east side)
        const double NZ = NeNotch(X, Y, Z, 600.0);
        const double WN = Ramp(Y, 26000.0, WORLD);
        Z = WN >= 1.0 ? NZ : Lerp(Z, NZ, WN);
    }
    if (Y > WORLD)
    {
        // the road spur (Calder Dr curves round its north side) and the far road bench
        const double SB = SeamValue(9500.0, WORLD);
        Z = SMax(Z, SB + 3600.0 * (1.0 - Ramp(EllipseRho(X, Y, ROAD_SPUR_X, ROAD_SPUR_Y, 4200.0, 3600.0), 0.15, 1.0)), 800.0);
        double D, Sv;
        PolylineSoft(FarRoadLine, X, Y, NOTCH_SOFT, D, Sv);
        if (D < 4000.0)
        {
            const double RZ = SB + 0.06 * Sv;
            if (D < 700.0) Z = RZ;
            else Z = Lerp(RZ, Z, S((D - 700.0) / 3300.0));
        }
        return Z;
    }
    double Wt[5];
    const double Forms[5] = { NearRidge(X, Y), RidgeBand(X, Y, -127000.0, 8000.0, 45000.0, 4000.0, 34000.0, 4301, Wt[1]),
        RidgeBand(X, Y, -178000.0, 10000.0, 67500.0, 6500.0, 44000.0, 4401, Wt[2]), GoslingKnob(X, Y, Wt[3]), WaterTowerKnob(X, Y, Wt[4]) };
    Wt[0] = Ramp(Y, -45600.0, -47600.0);
    for (int32 K = 0; K < 5; ++K)
        if (Wt[K] > 0.0)
        {
            const double M = SMax(Z, Forms[K], 1500.0);
            Z = Wt[K] >= 1.0 ? M : Lerp(Z, M, Wt[K]);
        }
    // the Calder Branch below the headwall: a ravine through the lower bluff to the river
    if (-46500.0 < Y && Y < -WORLD && 20000.0 < X && X < 36000.0)
    {
        const double DG = FMath::Abs(X - 27500.0 - 0.08 * (-WORLD - Y));
        const double RZ = Lerp(GF + HEAD_Z, RIVER_Z + 40.0, Ramp(Y, -32000.0, -44000.0));
        Z = Lerp(SMin(Z, RZ + DG * 0.5, 600.0), Z, Ramp(Y, -45000.0, -46500.0));
    }
    return Z;
}

double Outer(double X, double Y) { return OuterAt(X, Y, SeamFan(X, Y)); }

/** landform.py far_blend: the seam blend's width (25 m below the bluff, 60 m elsewhere, turning round the south corners). */
double FarBlend(double X, double Y)
{
    if (Y >= -WORLD) return 6000.0;
    const double AX = FMath::Abs(X);
    if (AX <= WORLD) return 2500.0;
    const double DX = AX - WORLD;
    const double DY = -WORLD - Y;
    return Lerp(6000.0, 2500.0, DY / (DX + DY));
}

/** landform.py far() beyond the square (the caller handles the inside). */
double Far(double X, double Y)
{
    const double AX = FMath::Abs(X), AY = FMath::Abs(Y);
    const double DX = AX > WORLD ? AX - WORLD : 0.0;
    const double DY = AY > WORLD ? AY - WORLD : 0.0;
    const double D = FMath::Sqrt(DX * DX + DY * DY);
    const double E = SeamFan(X, Y);
    const double O = OuterAt(X, Y, E);
    return Lerp(E, O, S(D / FarBlend(X, Y)));
}

/** landform.py ex_ridges (today's sheets, HouseExteriorCampus.cpp Mountains(), re-based on the foothills). */
double ExRidges(double X, double Y)
{
    constexpr double Sheets[3][3] = { { 32000.0, 19000.0, 8000.0 }, { 65000.0, 28000.0, 18500.0 }, { 112000.0, 40000.0, 34000.0 } };
    double Best = -1e9;
    for (int32 Layer = 0; Layer < 3; ++Layer)
    {
        const double Y0 = Sheets[Layer][0], Dp = Sheets[Layer][1], H = Sheets[Layer][2];
        const double L = double(Layer);
        const double T = (Y - Y0 - 1600.0 * FMath::Sin(X * 0.00008 + L)) / Dp;
        if (T < 0.0 || T > 1.0 || FMath::Abs(X) > 145000.0) continue;
        const double Wave = .55 + .18 * FMath::Sin(X * .000057 + L * 1.9) + .11 * FMath::Sin(X * .000131 + L) + .07 * FMath::Cos(X * .000291 + 1.1 * L);
        const double Cross = FMath::Pow(PyMax(0.0, FMath::Sin(T * PI_D)), .65);
        const double Edge = Clamp((145000.0 - FMath::Abs(X)) / 20000.0, 0.0, 1.0);
        double Z = GR + H * Wave * Cross * Edge + (NorthBase(X) - GR);
        if (Layer == 0) Z = NeNotch(X, Y, Z, 900.0);
        Best = PyMax(Best, Z);
    }
    return Best;
}
}
// ====================================================================================== END PORTABLE MATH

double HouseCalderFarLandImpl::SeamGround(double X, double Y)
{
    // The walkable ground the walkable tiles draw (the lattice's triangle surface, or a wall-split cell's piece).
    return double(HouseExteriorSite::GroundHeight(float(X), float(Y)));
}

double HouseCalderFarLand::Ground(double X, double Y)
{
    if (FMath::Abs(X) <= World && FMath::Abs(Y) <= World) return double(HouseExteriorSite::GroundHeight(float(X), float(Y)));
    return HouseCalderFarLandImpl::Far(X, Y);
}
double HouseCalderFarLand::Outer(double X, double Y) { return HouseCalderFarLandImpl::Outer(X, Y); }
double HouseCalderFarLand::TownFloor(double X, double Y) { return HouseCalderFarLandImpl::TownFloor(X, Y); }
double HouseCalderFarLand::SeamValue(double X, double Y) { return HouseCalderFarLandImpl::SeamValue(X, Y); }
double HouseCalderFarLand::NorthBase(double X) { return HouseCalderFarLandImpl::NorthBase(X); }
double HouseCalderFarLand::NeNotch(double X, double Y, double Z, double FloorAdd) { return HouseCalderFarLandImpl::NeNotch(X, Y, Z, FloorAdd); }
double HouseCalderFarLand::ExRidges(double X, double Y) { return HouseCalderFarLandImpl::ExRidges(X, Y); }
double HouseCalderFarLand::SkylineGround(double X, double Y)
{
    const double G = Ground(X, Y);
    return Y > World ? HouseCalderFarLandImpl::PyMax(G, ExRidges(X, Y)) : G;
}
double HouseCalderFarLand::RiverSurface(double X) { return RiverDatum + 40.0 - 0.001 * X; }

// ====================================================================================== the self-test
#if !UE_BUILD_SHIPPING
namespace HouseCalderFarLandImpl
{
struct FSelfTestState { TWeakObjectPtr<UWorld> World; double Start = 0.0; bool bDone = false; };
FSelfTestState& SelfTestState() { static FSelfTestState State; return State; }

/** Evaluates the port on the expected points and logs CALDER_FARLAND_SELFTEST. Returns true on PASS. */
bool RunSelfTest()
{
    const double Start = FPlatformTime::Seconds();
    FString Path;
    if (!FParse::Value(FCommandLine::Get(), TEXT("CalderFarLandExpected="), Path))
        Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Saved/CalderWork/plan/farland_expected.csv"));
    TArray<FString> Lines;
    if (!HouseCalderTerrain::Enabled() || !FFileHelper::LoadFileToStringArray(Lines, *Path))
    {
        UE_LOG(LogTemp, Error, TEXT("CALDER_FARLAND_SELFTEST result=FAIL reason=%s path=%s"),
            HouseCalderTerrain::Enabled() ? TEXT("no-expected-file") : TEXT("terrain-off (the seam needs the lattice)"), *Path);
        return false;
    }
    const TCHAR* Names[4] = { TEXT("ground"), TEXT("townFloor"), TEXT("northBase"), TEXT("exRidges") };
    double MaxErr[4] = { 0.0, 0.0, 0.0, 0.0 };
    FVector2D Worst[4] = { FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), FVector2D(0.0, 0.0), FVector2D(0.0, 0.0) };
    int32 Points = 0, Bad = 0;
    FString Actual = TEXT("x,y,ground,town_floor,north_base,ex_ridges\n"), Made;
    for (const FString& Line : Lines)
    {
        if (Line.StartsWith(TEXT("#")) && Made.IsEmpty()) Made = Line;   // which landform.py and which bake it was made from
        if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT("x"))) continue;
        TArray<FString> Cells;
        Line.ParseIntoArray(Cells, TEXT(","), true);
        if (Cells.Num() < 6) { ++Bad; continue; }
        const double X = FCString::Atod(*Cells[0]), Y = FCString::Atod(*Cells[1]);
        const double Got[4] = { HouseCalderFarLand::Ground(X, Y), HouseCalderFarLand::TownFloor(X, Y), HouseCalderFarLand::NorthBase(X),
            Y > HouseCalderFarLand::World ? HouseCalderFarLand::ExRidges(X, Y) : -1e9 };
        for (int32 K = 0; K < 4; ++K)
        {
            const double Want = FCString::Atod(*Cells[2 + K]);
            if (Want <= -1e8 && Got[K] <= -1e8) continue;
            const double Err = FMath::Abs(Got[K] - Want);
            if (!(Err <= MaxErr[K])) { MaxErr[K] = Err; Worst[K] = FVector2D(X, Y); }   // (a NaN counts as the worst)
        }
        Actual += FString::Printf(TEXT("%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n"), X, Y, Got[0], Got[1], Got[2], Got[3]);
        ++Points;
    }
    const double All = FMath::Max(FMath::Max(MaxErr[0], MaxErr[1]), FMath::Max(MaxErr[2], MaxErr[3]));
    const bool bPass = Points >= 2000 && Bad == 0 && All < 0.5;
    const FString Dir = FPaths::ProjectSavedDir() / TEXT("CalderFarLandSelfTest");
    FFileHelper::SaveStringToFile(Actual, *(Dir / TEXT("actual.csv")));
    const FString Summary = FString::Printf(TEXT("%s points=%d bad=%d maxErr=%.6f cm (ground %.6f at (%.0f,%.0f), townFloor %.6f, northBase %.6f, exRidges %.6f) limit=0.5 ms=%.1f"),
        bPass ? TEXT("PASS") : TEXT("FAIL"), Points, Bad, All, MaxErr[0], Worst[0].X, Worst[0].Y, MaxErr[1], MaxErr[2], MaxErr[3], (FPlatformTime::Seconds() - Start) * 1000.0);
    FFileHelper::SaveStringToFile(Summary + TEXT("\n"), *(Dir / TEXT("result.txt")));
    UE_LOG(LogTemp, Display, TEXT("CALDER_FARLAND_SELFTEST maxErr=%.6f points=%d result=%s %s"), All, Points, bPass ? TEXT("PASS") : TEXT("FAIL"), *Summary);
    for (int32 K = 0; K < 4; ++K)
        UE_LOG(LogTemp, Display, TEXT("CALDER_FARLAND_SELFTEST %s maxErr=%.6f at (%.3f,%.3f)"), Names[K], MaxErr[K], Worst[K].X, Worst[K].Y);
    UE_LOG(LogTemp, Display, TEXT("CALDER_FARLAND_SELFTEST expected=%s %s"), *Path, *Made);
    return bPass;
}

void SelfTestTick(UWorld* InWorld, ELevelTick, float)
{
    if (!InWorld || !InWorld->IsGameWorld() || InWorld->GetNetMode() == NM_Client) return;
    static const bool bWanted = FParse::Param(FCommandLine::Get(), TEXT("CalderFarLandSelfTest"));
    if (!bWanted) return;
    FSelfTestState& State = SelfTestState();
    const double Now = FPlatformTime::Seconds();
    if (State.World.Get() != InWorld) { State = FSelfTestState(); State.World = InWorld; State.Start = Now; }
    if (State.bDone || Now - State.Start < 3.0) return;   // after the exterior (and its CALDER_OUTERLAND line) has built
    State.bDone = true;
    RunSelfTest();
    FPlatformMisc::RequestExitWithStatus(false, 0);
}

struct FSelfTestRegistration { FSelfTestRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&SelfTestTick); } };
FSelfTestRegistration SelfTestRegistration;
}
#endif
