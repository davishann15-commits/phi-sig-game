// Calder State exterior polish: Calder, the small mountain college town south of the campus, in place of the twelve
// random far blocks (HouseExteriorCampus.cpp keeps those when the polish is off). Seen over the hedgerow from the campus
// and from the stands, in October 1986 at dusk:
// - three ridgelines behind the town, the near one wooded in fall colour, the two behind going blue in the haze (the
//   north side of the valley already has its own three, HouseExteriorCampus.cpp Mountains());
// - the valley town: Main Street's brick fronts with their shop windows lit, the inn, the courthouse dome, a white
//   steeple and a brick church tower, the old mill on the river with its stack, the river and its truss bridge;
// - frame houses along the edge of town and stepping up the lower slope, woodsmoke from a few chimneys;
// - the CALDER water tower on a knoll, the radio station's mast on the ridge, the fire lookout on its peak.
// All in the Skyline part (one material a section, in huge tiles, drawn as static meshes, never culled). Every light in
// it is emission (windows, the mast's red beacons).
#include "HouseCampusPolish.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusFallKit.h"
#include "HouseCampusInteriorsRooms.h"
#include "HouseCalderFarLand.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderMapTrees.h"
#include "Engine/Texture2D.h"

namespace HouseCampusPolishTownImpl
{
// Every height here stands on the valley floor (HouseCalderTerrain::ValleyZ: today's -60 grade, as it is with the Calder
// terrain off), read per vertex or at each building's own spot. With the Calder terrain ValleyZ is the outer land itself
// (HouseCalderFarLand: the town's valley, the river, the near ridge, the water tower's knob), drawn by HouseCalderOuterLand,
// so the ridges and valley-floor quads below are not built and everything stands on that ground.
constexpr float FootY = -46000.f, CrestY = -95000.f;   // the near ridge rises from the river's far bank to its crest
const FVector2D WaterTowerAt(10500.f, -50500.f);   // its feet stand 4 m out on each axis (WaterTower()); houses keep off
const FHallUV& Plain() { return HouseCampusFallKit::PlainUV(); }
FVector Up(float Z) { return FVector(0.f, 0.f, Z); }
const FVector North(0.f, 1.f, 0.f);   // the town faces the campus

/** The near ridge's crest height above the valley at X. */
float Crest(float X)
{
    return 9000.f + 2600.f * FMath::Sin(X / 23000.f + .4f) + 1600.f * FMath::Sin(X / 8000.f + 1.7f) + 700.f * FMath::Sin(X / 3100.f + 2.9f);
}
/** The ground of the near ridge at (X, Y): flat valley floor north of FootY, up the face to the crest, down its back.
 *  With the Calder terrain, the outer land there (which has its own near ridge). */
float Slope(float X, float Y)
{
    if (HouseCalderTerrain::Enabled()) return HouseCalderTerrain::ValleyZ(X, Y);
    const float G = HouseCalderTerrain::ValleyZ(X, Y);
    const float T = (FootY - Y) / (FootY - CrestY);
    if (T <= 0.f) return G;
    const float Edge = FMath::Clamp((150000.f - FMath::Abs(X)) / 30000.f, 0.f, 1.f);
    const float S = T <= 1.f ? T * T * (3.f - 2.f * T) : FMath::Max(.6f, 1.f - (T - 1.f) * 1.4f);
    // Spurs and hollows down the face: irregular (a regular wave read as one long crease in the low sun).
    const float Spurs = 1500.f * (HallFbm(X * .00009f, Y * .00011f, 3, 1986) - .5f) * 2.f * FMath::Clamp(T, 0.f, 1.f) * (1.f - FMath::Clamp(T, 0.f, 1.f)) * 4.f;
    return G + (Crest(X) * S + Spurs) * Edge;
}

/** The lowest ground under a footprint (its corners and middle): where its walls start, so no side stands in the air on
 *  a slope. Today (and with -NoCalderTerrain) the valley floor is level, so this is the floor itself. */
float Footing(float X0, float X1, float Y0, float Y1)
{
    float Z = HouseCalderTerrain::ValleyZ((X0 + X1) * .5f, (Y0 + Y1) * .5f);
    for (const float X : { X0, X1 })
        for (const float Y : { Y0, Y1 }) Z = FMath::Min(Z, HouseCalderTerrain::ValleyZ(X, Y));
    return Z;
}

/** A grid of windows on the north face (Y = Face) of a block X0..X1: dark glass, Lit of them lit warm (or cool). */
void Windows(FHallMesh& M, FRandomStream& R, float X0, float X1, float Face, float Z0, float Z1, float Bay, float Storey, float Lit)
{
    for (float Z = Z0 + Storey * .35f; Z + Storey * .45f <= Z1; Z += Storey)
        for (float X = X0 + Bay * .5f; X < X1 - Bay * .3f; X += Bay)
        {
            const FVector C(X, Face + 1.5f, Z + Storey * .2f);
            const float W = Bay * .26f, H = Storey * .27f;
            M.Quad(R.FRand() < Lit ? FName(TEXT("CPT_WinLit")) : FName(TEXT("CPT_WinDark")), C + FVector(-W, 0, -H), C + FVector(W, 0, -H), C + FVector(W, 0, H), C + FVector(-W, 0, H), North, Plain());
        }
}

/** A gable roof over X0..X1 x Y0..Y1 at eave Z, its ridge along X. */
void Gable(FHallMesh& M, FName Key, float X0, float X1, float Y0, float Y1, float Z, float Rise)
{
    const float Ym = (Y0 + Y1) * .5f;
    const FVector A(X0, Y0, Z), B(X1, Y0, Z), C(X1, Y1, Z), D(X0, Y1, Z), E(X0, Ym, Z + Rise), F(X1, Ym, Z + Rise);
    M.Quad(Key, A, B, F, E, FVector::CrossProduct(B - A, E - A).GetSafeNormal(), Plain());
    M.Quad(Key, C, D, E, F, FVector::CrossProduct(D - C, F - C).GetSafeNormal(), Plain());
    HouseCampusFallKit::Tri(M, Key, A, E, D, FVector(-1, 0, 0));
    HouseCampusFallKit::Tri(M, Key, B, C, F, FVector(1, 0, 0));
}

/** A four-sided spire from a square base of half-width Half at Z up to Apex. */
void Spire(FHallMesh& M, FName Key, const FVector2D& C, float Half, float Z, float Apex)
{
    const FVector T(C, Apex);
    const FVector Q[4] = { FVector(C.X - Half, C.Y - Half, Z), FVector(C.X + Half, C.Y - Half, Z), FVector(C.X + Half, C.Y + Half, Z), FVector(C.X - Half, C.Y + Half, Z) };
    for (int32 K = 0; K < 4; ++K) HouseCampusFallKit::Tri(M, Key, Q[K], Q[(K + 1) % 4], T, FVector::CrossProduct(Q[(K + 1) % 4] - Q[K], T - Q[K]).GetSafeNormal());
}

void Ridges(FHallMesh& M)
{
    // The near ridge as a surface from the valley floor over the crest (houses and the tower stand on it: Slope()).
    // (Shaded smooth: each vertex's normal from the slope itself, so the face reads as a hillside, not facets.)
    constexpr int32 NX = 160, NT = 16;
    FHallSection& Face = M.Sec(TEXT("CPT_FallForest"));
    const int32 Base = Face.V.Num();
    for (int32 I = 0; I <= NX; ++I)
        for (int32 J = 0; J <= NT; ++J)
        {
            const float X = -150000.f + 300000.f * I / NX, Y = FootY + (CrestY - FootY) * (J * 1.3f / NT);
            const float Dx = (Slope(X + 200.f, Y) - Slope(X - 200.f, Y)) / 400.f, Dy = (Slope(X, Y + 200.f) - Slope(X, Y - 200.f)) / 400.f;
            Face.V.Add(FVector(X, Y, Slope(X, Y)));
            Face.N.Add(FVector(-Dx, -Dy, 1.f).GetSafeNormal());
            Face.UV.Add(FVector2D(X / 4000.f, Y / 4000.f));
            Face.T.Add(FProcMeshTangent(1.f, 0.f, 0.f));
        }
    for (int32 I = 0; I < NX; ++I)
        for (int32 J = 0; J < NT; ++J)
        {
            const int32 A = Base + I * (NT + 1) + J, B = A + NT + 1;
            // J runs toward -Y (CrestY < FootY): wound so the fronts face up, toward the campus.
            Face.I.Append({ A, B, B + 1, A, B + 1, A + 1 });
        }
    // The valley floor beside the town's lawn (EX_Grass: X +-31000 down to Y -49000) and beside the campus ground (X +-32000,
    // at the grade south of Y 0), out to the ridge's ends: the ridge's foot, the river and the trees stand on ground, not the
    // void. Each corner on the valley floor there.
    for (const float Side : { -1.f, 1.f })
    {
        auto ValleyFloor = [&M, Side](float XIn, float Y0, float Y1)
        {
            const float XA = Side * XIn, XB = Side * 150000.f;
            M.Quad(TEXT("CPT_FallForest"), FVector(XA, Y0, HouseCalderTerrain::ValleyZ(XA, Y0)), FVector(XB, Y0, HouseCalderTerrain::ValleyZ(XB, Y0)),
                FVector(XB, Y1, HouseCalderTerrain::ValleyZ(XB, Y1)), FVector(XA, Y1, HouseCalderTerrain::ValleyZ(XA, Y1)), FVector::UpVector, FHallUV::World(4000.f));
        };
        ValleyFloor(31000.f, FootY, -32000.f);
        ValleyFloor(32000.f, -32000.f, 0.f);
    }
    // The two behind, curtains going blue (as the valley's north side has them).
    for (int32 Layer = 0; Layer < 2; ++Layer)
    {
        const float Y0 = Layer == 0 ? -104000.f : -150000.f, Depth = Layer == 0 ? 30000.f : 44000.f, H = Layer == 0 ? 21000.f : 34000.f, Span = Layer == 0 ? 220000.f : 320000.f;
        const FName K = Layer == 0 ? TEXT("CPT_RidgeMid") : TEXT("CPT_RidgeFar");
        constexpr int32 LX = 96, LY = 8;
        auto P = [&](int32 I, int32 J)
        {
            const float X = -Span + 2.f * Span * I / LX, T = float(J) / LY;
            const float Wave = .55f + .18f * FMath::Sin(X * .000047f + Layer * 2.3f + 1.f) + .11f * FMath::Sin(X * .000123f + Layer) + .07f * FMath::Cos(X * .000277f + 1.3f * Layer);
            const float Cross = FMath::Pow(FMath::Max(0.f, FMath::Sin(T * PI)), .65f);
            const float Edge = FMath::Clamp((Span - FMath::Abs(X)) / (Span * .2f), 0.f, 1.f);
            const float Y = Y0 - Depth * T;
            return FVector(X, Y, HouseCalderTerrain::ValleyZ(X, Y) + H * Wave * Cross * Edge);
        };
        for (int32 I = 0; I < LX; ++I)
            for (int32 J = 0; J < LY; ++J)
            {
                const FVector A = P(I, J), B = P(I + 1, J), C = P(I + 1, J + 1), D = P(I, J + 1);
                FVector N = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
                if (N.Z < 0.f) N = -N;
                M.Quad(K, A, B, C, D, N, FHallUV::World(3000.f));
            }
    }
}

/** A frame house on the ground at (X, Y) (on the slope its downhill side goes on down), with a chimney, sometimes smoking. */
void House(HouseCampusPolish::FParts& P, FRandomStream& R, float X, float Y)
{
    static const FName Sidings[] = { TEXT("CPT_White"), TEXT("CPT_SidingYellow"), TEXT("CPT_SidingBlue"), TEXT("CPT_BrickRed"), TEXT("CPT_White") };
    const float W = R.FRandRange(320.f, 400.f), D = R.FRandRange(360.f, 440.f), H = R.FRandRange(480.f, 600.f);
    const float X0 = X - W, X1 = X + W, Y0 = Y - D, Y1 = Y + D;
    const float Low = FMath::Min(FMath::Min(Slope(X0, Y0), Slope(X1, Y0)), FMath::Min(Slope(X0, Y1), Slope(X1, Y1)));
    const float High = FMath::Max(FMath::Max(Slope(X0, Y0), Slope(X1, Y0)), FMath::Max(Slope(X0, Y1), Slope(X1, Y1)));
    P.Skyline.BoxMinMax(Sidings[R.RandRange(0, 4)], FVector(X0, Y0, Low - 40.f), FVector(X1, Y1, High + H), Plain());
    Gable(P.Skyline, TEXT("CPT_RoofDark"), X0 - 30.f, X1 + 30.f, Y0 - 40.f, Y1 + 40.f, High + H, R.FRandRange(220.f, 320.f));
    Windows(P.Skyline, R, X0 + 40.f, X1 - 40.f, Y1, High + 40.f, High + H, 210.f, 260.f, .32f);
    if (R.FRand() < .5f)
    {
        const FVector Top(X1 - 85.f, Y, High + H + 330.f);
        P.Skyline.BoxMinMax(TEXT("CPT_BrickRed"), FVector(X1 - 120.f, Y - 40.f, High + H), Top + FVector(35.f, 40.f, 0.f), Plain());
        if (R.FRand() < .25f) P.Puffs.Add({ Top + Up(200.f), 320.f, FLinearColor(.5f, .5f, .52f, .09f), 1 });
    }
}

void Houses(HouseCampusPolish::FParts& P, FRandomStream& R)
{
    // Along the edge of town facing the campus, then up the ridge's lower slope beyond the river.
    for (const float Y : { -34300.f, -35900.f })
        for (float X = -26000.f; X < 26000.f; X += 1000.f)
            if (R.FRand() >= .25f && FMath::Abs(FMath::Fmod(X + 26000.f, 9000.f)) >= 900.f) House(P, R, X, Y);
    // A house that would stand on the water tower's legs is built into Spare and dropped, so the stream (and the rest of the
    // town after it) stays as it was.
    HouseCampusPolish::FParts Spare;
    for (float Y = -47600.f; Y > -56000.f; Y -= 1400.f)
        for (float X = -30000.f; X < 30000.f; X += 1100.f)
            if (R.FRand() < .45f - (-47600.f - Y) / 30000.f)
            {
                const float Hx = X + R.FRandRange(-250.f, 250.f), Hy = Y + R.FRandRange(-250.f, 250.f);
                const bool bOnTower = FMath::Abs(Hx - WaterTowerAt.X) < 900.f && FMath::Abs(Hy - WaterTowerAt.Y) < 950.f;
                House(bOnTower ? Spare : P, R, Hx, Hy);
            }
}

/** A hardwood in October (or a pine): a trunk and two or three crowns in its colour, standing on the ground at (X, Y). */
void Tree(HouseCampusPolish::FParts& P, FRandomStream& R, float X, float Y, float Scale = 1.f)
{
    static const FName Crowns[] = { TEXT("CPT_LeafRust"), TEXT("CPT_LeafGold"), TEXT("CPT_LeafRed"), TEXT("CPT_LeafGreen"), TEXT("CPT_LeafGold") };
    const float Z = Slope(X, Y), S = R.FRandRange(.8f, 1.35f) * Scale;
    FHallMesh Scratch;
    FHallMesh& Draw=HouseCalderMapTrees::Enabled()?Scratch:P.Skyline;
    if (R.FRand() < .22f)
    {
        // A pine: a dark cone over a short trunk.
        HouseCampusFallKit::Bar(Draw, TEXT("CPT_Trunk"), FVector(X, Y, Z - 20.f), FVector(X, Y, Z + 400.f * S), 24.f * S, 24.f * S, North);
        HouseCampusFallKit::Lathe(Draw, TEXT("CPT_LeafGreen"), FVector(X, Y, Z + 150.f * S), { FVector2D(260.f * S, 0.f), FVector2D(10.f, 1300.f * S) }, 7);
        if (HouseCalderMapTrees::Enabled()) HouseCalderMapTrees::TownTree(P.Skyline,Scratch,FVector(X,Y,Z),true,3);
        return;
    }
    HouseCampusFallKit::Bar(Draw, TEXT("CPT_Trunk"), FVector(X, Y, Z - 20.f), FVector(X, Y, Z + 420.f * S), 30.f * S, 30.f * S, North);
    const int32 Tone=R.RandRange(0,4);
    const FName Key = Crowns[Tone];
    for (int32 K = 0; K < 3; ++K)
        HouseCampusFallKit::Ball(Draw, Key, FVector(X + R.FRandRange(-120.f, 120.f) * S, Y + R.FRandRange(-120.f, 120.f) * S, Z + (520.f + K * 170.f) * S), (300.f - K * 50.f) * S, 6);
    if (HouseCalderMapTrees::Enabled()) HouseCalderMapTrees::TownTree(P.Skyline,Scratch,FVector(X,Y,Z),false,Tone);
}

void Trees(HouseCampusPolish::FParts& P, FRandomStream& R)
{
    // Among the houses at the edge of town, along the river, and thick up the slope above the houses.
    for (int32 K = 0; K < 70; ++K) Tree(P, R, R.FRandRange(-27000.f, 27000.f), R.FRandRange(-36800.f, -33300.f), .7f);
    for (int32 K = 0; K < 50; ++K) Tree(P, R, R.FRandRange(-40000.f, 40000.f), R.FRand() < .5f ? R.FRandRange(-46900.f, -45800.f) : R.FRandRange(-44200.f, -43200.f));
    for (int32 K = 0; K < 160; ++K) Tree(P, R, R.FRandRange(-45000.f, 45000.f), R.FRandRange(-60000.f, -47200.f));
}

void MainStreet(HouseCampusPolish::FParts& P, FRandomStream& R)
{
    // Brick fronts along Main Street (Y -39000), two and three storeys, parapets, the shop windows lit.
    static const FName Bricks[] = { TEXT("CPT_BrickRed"), TEXT("CPT_BrickTan"), TEXT("CPT_BrickRed"), TEXT("CPT_Stone") };
    float X = -7200.f;
    while (X < 7400.f)
    {
        const float W = R.FRandRange(700.f, 1200.f), H = 380.f * R.RandRange(2, 3) + 120.f;
        if (X + W > -3600.f && X < -1400.f) { X = -1400.f; continue; }   // the courthouse square
        const float G = HouseCalderTerrain::ValleyZ(X + (W - 20.f) * .5f, -39700.f);   // under the block's middle
        P.Skyline.BoxMinMax(Bricks[R.RandRange(0, 3)], FVector(X, -40400.f, FMath::Min(G, Footing(X, X + W - 20.f, -40400.f, -39000.f)) - 20.f), FVector(X + W - 20.f, -39000.f, G + H), Plain());
        P.Skyline.BoxMinMax(TEXT("CPT_Stone"), FVector(X - 10.f, -39010.f, G + H), FVector(X + W - 10.f, -38980.f, G + H + 60.f), Plain());   // the cornice
        P.Skyline.Quad(TEXT("CPT_WinLit"), FVector(X + 60.f, -38998.f, G + 40.f), FVector(X + W - 80.f, -38998.f, G + 40.f), FVector(X + W - 80.f, -38998.f, G + 260.f), FVector(X + 60.f, -38998.f, G + 260.f), North, Plain());
        Windows(P.Skyline, R, X, X + W - 20.f, -39000.f, G + 380.f, G + H, 180.f, 380.f, .25f);
        X += W;
    }
    // The Calder Inn: four storeys over Main Street, its name on the roof frame.
    const float InnG = HouseCalderTerrain::ValleyZ(2700.f, -40900.f);
    P.Skyline.BoxMinMax(TEXT("CPT_BrickTan"), FVector(1800.f, -41600.f, FMath::Min(InnG, Footing(1800.f, 3600.f, -41600.f, -40200.f)) - 20.f), FVector(3600.f, -40200.f, InnG + 1650.f), Plain());
    Windows(P.Skyline, R, 1800.f, 3600.f, -40200.f, InnG + 60.f, InnG + 1650.f, 200.f, 390.f, .35f);
    for (const float Px : { 1950.f, 3450.f }) P.Skyline.Cylinder(TEXT("CPT_Steel"), FVector(Px, -40400.f, InnG + 1650.f), FVector(Px, -40400.f, InnG + 1980.f), 8.f, 6, Plain());
    HouseCampusFallKit::Card(P.Skyline, TEXT("CPT_InnSign"), FVector(2700.f, -40390.f, InnG + 1890.f), FVector(1, 0, 0), FVector::UpVector, 1400.f, 180.f, North);
}

void Courthouse(HouseCampusPolish::FParts& P, FRandomStream& R)
{
    // The county courthouse on its square: a stone block, a portico, the drum, a copper dome and its lantern.
    const FVector2D C(-2500.f, -42200.f);
    const float G = HouseCalderTerrain::ValleyZ(C.X, C.Y);
    P.Skyline.BoxMinMax(TEXT("CPT_Stone"), FVector(C.X - 1500.f, C.Y - 1200.f, FMath::Min(G, Footing(C.X - 1500.f, C.X + 1500.f, C.Y - 1200.f, C.Y + 1200.f)) - 20.f), FVector(C.X + 1500.f, C.Y + 1200.f, G + 1400.f), Plain());
    Windows(P.Skyline, R, C.X - 1500.f, C.X + 1500.f, C.Y + 1200.f, G + 60.f, G + 1400.f, 300.f, 460.f, .12f);
    for (int32 K = 0; K < 6; ++K) P.Skyline.Cylinder(TEXT("CPT_White"), FVector(C.X - 625.f + K * 250.f, C.Y + 1420.f, G), FVector(C.X - 625.f + K * 250.f, C.Y + 1420.f, G + 1050.f), 40.f, 10, Plain());
    HouseCampusFallKit::Tri(P.Skyline, TEXT("CPT_Stone"), FVector(C.X - 760.f, C.Y + 1500.f, G + 1050.f), FVector(C.X + 760.f, C.Y + 1500.f, G + 1050.f), FVector(C.X, C.Y + 1500.f, G + 1380.f), North);
    P.Skyline.BoxMinMax(TEXT("CPT_Stone"), FVector(C.X - 760.f, C.Y + 1200.f, G + 1050.f), FVector(C.X + 760.f, C.Y + 1500.f, G + 1110.f), Plain());
    P.Skyline.Cylinder(TEXT("CPT_Stone"), FVector(C, G + 1400.f), FVector(C, G + 2000.f), 520.f, 24, Plain());
    P.Skyline.Dome(TEXT("CPT_Copper"), FVector(C, G + 2000.f), FVector::UpVector, 540.f, 620.f, 24);
    P.Skyline.Cylinder(TEXT("CPT_White"), FVector(C, G + 2600.f), FVector(C, G + 2900.f), 110.f, 12, Plain());
    P.Skyline.Dome(TEXT("CPT_Copper"), FVector(C, G + 2900.f), FVector::UpVector, 130.f, 140.f, 12);
}

void Churches(HouseCampusPolish::FParts& P)
{
    // The white church: clapboard, a square tower, the belfry and a tall spire.
    const FVector2D A(-8800.f, -38300.f);
    const float GA = HouseCalderTerrain::ValleyZ(A.X, A.Y);   // the whole church stands at its tower's foot
    P.Skyline.BoxMinMax(TEXT("CPT_White"), FVector(A.X - 600.f, A.Y - 2000.f, FMath::Min(GA, Footing(A.X - 600.f, A.X + 600.f, A.Y - 2000.f, A.Y)) - 20.f), FVector(A.X + 600.f, A.Y, GA + 900.f), Plain());
    Gable(P.Skyline, TEXT("CPT_RoofDark"), A.X - 660.f, A.X + 660.f, A.Y - 2050.f, A.Y + 40.f, GA + 900.f, 520.f);
    P.Skyline.BoxMinMax(TEXT("CPT_White"), FVector(A.X - 220.f, A.Y - 220.f, GA - 20.f), FVector(A.X + 220.f, A.Y + 220.f, GA + 2100.f), Plain());
    P.Skyline.BoxMinMax(TEXT("CPT_WinDark"), FVector(A.X - 120.f, A.Y + 221.f, GA + 1750.f), FVector(A.X + 120.f, A.Y + 223.f, GA + 2000.f), Plain());
    P.Skyline.BoxMinMax(TEXT("CPT_White"), FVector(A.X - 190.f, A.Y - 190.f, GA + 2100.f), FVector(A.X + 190.f, A.Y + 190.f, GA + 2500.f), Plain());
    Spire(P.Skyline, TEXT("CPT_White"), A, 180.f, GA + 2500.f, GA + 4300.f);
    // The brick church: a square tower with four pinnacles, a lit window over the door.
    const FVector2D B(7700.f, -40600.f);
    const float GB = HouseCalderTerrain::ValleyZ(B.X, B.Y);
    P.Skyline.BoxMinMax(TEXT("CPT_BrickRed"), FVector(B.X - 700.f, B.Y - 2200.f, FMath::Min(GB, Footing(B.X - 700.f, B.X + 700.f, B.Y - 2200.f, B.Y - 300.f)) - 20.f), FVector(B.X + 700.f, B.Y - 300.f, GB + 1000.f), Plain());
    Gable(P.Skyline, TEXT("CPT_RoofDark"), B.X - 760.f, B.X + 760.f, B.Y - 2260.f, B.Y - 240.f, GB + 1000.f, 600.f);
    P.Skyline.BoxMinMax(TEXT("CPT_BrickRed"), FVector(B.X - 320.f, B.Y - 320.f, GB - 20.f), FVector(B.X + 320.f, B.Y + 320.f, GB + 3000.f), Plain());
    P.Skyline.BoxMinMax(TEXT("CPT_Stone"), FVector(B.X - 340.f, B.Y - 340.f, GB + 3000.f), FVector(B.X + 340.f, B.Y + 340.f, GB + 3080.f), Plain());
    P.Skyline.Quad(TEXT("CPT_WinLit"), FVector(B.X - 80.f, B.Y + 322.f, GB + 300.f), FVector(B.X + 80.f, B.Y + 322.f, GB + 300.f), FVector(B.X + 80.f, B.Y + 322.f, GB + 760.f), FVector(B.X - 80.f, B.Y + 322.f, GB + 760.f), North, Plain());
    for (const float Sx : { -1.f, 1.f }) for (const float Sy : { -1.f, 1.f })
        Spire(P.Skyline, TEXT("CPT_Stone"), FVector2D(B.X + Sx * 280.f, B.Y + Sy * 280.f), 50.f, GB + 3080.f, GB + 3520.f);
}

void RiverAndMill(HouseCampusPolish::FParts& P, FRandomStream& R)
{
    // The river along the foot of the ridge, the truss bridge carrying the road south, the old mill on its bank.
    constexpr float RY0 = -45600.f, RY1 = -44400.f;
    // The water lies just over the valley floor (the exterior's EX_Grass box tops out at the grade - 3) and the ground beside
    // it; the river and its banks at the valley floor under the river's middle, the bridge at its own.
    const float G = HouseCalderTerrain::ValleyZ(0.f, (RY0 + RY1) * .5f);
    if (HouseCalderTerrain::Enabled())
    {
        // The outer land carves the river's trough and banks; the water stands in it at the river's level, which falls
        // 0.1 % east as the valley floor does (40 m pieces, 4 cm apart: never seen).
        for (float X = -60000.f; X < 60000.f; X += 4000.f)
        {
            const float Surface = float(HouseCalderFarLand::RiverSurface(X + 2000.f));
            P.Skyline.BoxMinMax(TEXT("CPT_River"), FVector(X, RY0, Surface - 9.f), FVector(X + 4000.f, RY1, Surface), Plain());
        }
    }
    else
    {
        P.Skyline.BoxMinMax(TEXT("CPT_River"), FVector(-60000.f, RY0, G - 10.f), FVector(60000.f, RY1, G - 1.f), Plain());
        for (const float Y : { RY0, RY1 }) P.Skyline.BoxMinMax(TEXT("CPT_RoofDark"), FVector(-60000.f, Y - 40.f, G - 10.f), FVector(60000.f, Y + 40.f, G + 5.f), Plain());   // the banks
    }
    // The deck stands 2.2 m over the river's floor and at least 30 cm over either bank 6 m beyond the water (with the
    // Calder terrain both banks are low: the far one about 0.4 m above the near one).
    const float BX = 1200.f, BridgeG = HouseCalderTerrain::ValleyZ(BX, (RY0 + RY1) * .5f);
    const float DeckZ = FMath::Max(BridgeG + 220.f, FMath::Max(HouseCalderTerrain::ValleyZ(BX, RY0 - 600.f), HouseCalderTerrain::ValleyZ(BX, RY1 + 600.f)) + 30.f);
    P.Skyline.BoxMinMax(TEXT("CPT_Steel"), FVector(BX - 260.f, RY0 - 600.f, DeckZ - 30.f), FVector(BX + 260.f, RY1 + 600.f, DeckZ), Plain());
    for (const float Ya : { RY0 - 600.f, RY1 + 40.f })   // its abutments
        P.Skyline.BoxMinMax(TEXT("CPT_Stone"), FVector(BX - 280.f, Ya, BridgeG - 20.f), FVector(BX + 280.f, Ya + 560.f, DeckZ - 30.f), Plain());
    for (const float Sx : { -260.f, 260.f })
        for (float Y = RY0 - 600.f; Y < RY1 + 600.f; Y += 300.f)
        {
            HouseCampusFallKit::Bar(P.Skyline, TEXT("CPT_Steel"), FVector(BX + Sx, Y, DeckZ), FVector(BX + Sx, Y + 300.f, DeckZ + 380.f), 12.f, 12.f, FVector(1, 0, 0));
            HouseCampusFallKit::Bar(P.Skyline, TEXT("CPT_Steel"), FVector(BX + Sx, Y, DeckZ + 380.f), FVector(BX + Sx, Y + 300.f, DeckZ + 380.f), 14.f, 14.f, FVector(1, 0, 0));
        }
    // The mill: four storeys of brick along the bank, its windows in long rows, and the stack.
    const float X0 = -24200.f, X1 = -17800.f, Y0 = -44200.f, Y1 = -42400.f, MillG = HouseCalderTerrain::ValleyZ((X0 + X1) * .5f, (Y0 + Y1) * .5f);
    P.Skyline.BoxMinMax(TEXT("CPT_BrickRed"), FVector(X0, Y0, FMath::Min(MillG, Footing(X0, X1, Y0, Y1)) - 20.f), FVector(X1, Y1, MillG + 1500.f), Plain());
    Gable(P.Skyline, TEXT("CPT_RoofDark"), X0 - 30.f, X1 + 30.f, Y0 - 30.f, Y1 + 30.f, MillG + 1500.f, 280.f);
    Windows(P.Skyline, R, X0, X1, Y1, MillG + 40.f, MillG + 1500.f, 260.f, 370.f, .2f);
    P.Skyline.BoxMinMax(TEXT("CPT_BrickRed"), FVector(-21400.f, Y1 - 200.f, MillG + 1500.f), FVector(-20800.f, Y1 + 200.f, MillG + 2400.f), Plain());   // the stair tower
    Spire(P.Skyline, TEXT("CPT_RoofDark"), FVector2D(-21100.f, Y1), 330.f, MillG + 2400.f, MillG + 2800.f);
    const FVector Stack(-17300.f, -43800.f, HouseCalderTerrain::ValleyZ(-17300.f, -43800.f));
    P.Skyline.Cylinder(TEXT("CPT_BrickRed"), Stack - Up(20.f), Stack + Up(3800.f), 170.f, 16, Plain());
    P.Skyline.Cylinder(TEXT("CPT_RoofDark"), Stack + Up(3700.f), Stack + Up(3820.f), 185.f, 16, Plain());
    for (int32 K = 0; K < 3; ++K) P.Puffs.Add({ Stack + Up(4000.f + K * 260.f) + FVector(K * 120.f, K * 60.f, 0.f), 460.f + K * 240.f, FLinearColor(.48f, .48f, .5f, .11f - K * .03f), 1 });
}

void WaterTower(HouseCampusPolish::FParts& P)
{
    // On a knoll above town: four braced legs, the balcony, the tank with CALDER on it, its roof and finial.
    const FVector2D C = WaterTowerAt;
    const float Ground = Slope(C.X, C.Y), Leg = 420.f, Top = 3000.f;
    TArray<FVector> Foot, Head;
    for (int32 K = 0; K < 4; ++K)
    {
        const float A = PI * .25f + K * HALF_PI;
        const FVector2D Off = FVector2D(FMath::Cos(A), FMath::Sin(A));
        Foot.Add(FVector(C + Off * Leg * 1.35f, Slope(C.X + Off.X * Leg * 1.35f, C.Y + Off.Y * Leg * 1.35f) - 30.f));
        Head.Add(FVector(C + Off * Leg, Ground + Top));
        HouseCampusFallKit::Bar(P.Skyline, TEXT("CPT_Steel"), Foot[K], Head[K], 34.f, 34.f, North);
    }
    for (const float T : { .33f, .66f })
        for (int32 K = 0; K < 4; ++K)
        {
            const FVector A = FMath::Lerp(Foot[K], Head[K], T), B = FMath::Lerp(Foot[(K + 1) % 4], Head[(K + 1) % 4], T);
            const FVector A2 = FMath::Lerp(Foot[K], Head[K], T + .33f), B2 = FMath::Lerp(Foot[(K + 1) % 4], Head[(K + 1) % 4], T + .33f);
            HouseCampusFallKit::Bar(P.Skyline, TEXT("CPT_Steel"), A, B, 14.f, 14.f, FVector::UpVector);
            HouseCampusFallKit::Bar(P.Skyline, TEXT("CPT_Steel"), A, B2, 8.f, 8.f, FVector::UpVector);
            HouseCampusFallKit::Bar(P.Skyline, TEXT("CPT_Steel"), B, A2, 8.f, 8.f, FVector::UpVector);
        }
    const FVector Tank(C, Ground + Top + 520.f);
    TArray<FVector2D> Prof;
    for (int32 J = 0; J <= 10; ++J) { const float A = -HALF_PI + PI * J / 10.f; Prof.Add(FVector2D(FMath::Max(1.f, 640.f * FMath::Cos(A)), 520.f * FMath::Sin(A))); }
    HouseCampusFallKit::Lathe(P.Skyline, TEXT("CPT_TankSilver"), Tank, Prof, 24);
    HouseCampusFallKit::Lathe(P.Skyline, TEXT("CPT_RoofDark"), Tank + Up(500.f), { FVector2D(260.f, 0.f), FVector2D(10.f, 260.f) }, 16);
    P.Skyline.Cylinder(TEXT("CPT_Steel"), Tank + Up(740.f), Tank + Up(900.f), 10.f, 8, Plain());
    for (int32 K = 0; K < 24; ++K)
    {
        const float A0 = 2.f * PI * K / 24.f, A1 = 2.f * PI * (K + 1) / 24.f;
        const FVector P0(C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * 700.f, Ground + Top + 40.f), P1(C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * 700.f, Ground + Top + 40.f);
        HouseCampusFallKit::Bar(P.Skyline, TEXT("CPT_Steel"), P0, P1, 60.f, 6.f, FVector::UpVector);
        HouseCampusFallKit::Bar(P.Skyline, TEXT("CPT_Steel"), P0 + Up(110.f), P1 + Up(110.f), 5.f, 5.f, FVector::UpVector);
    }
    HouseCampusFallKit::Card(P.Skyline, TEXT("CPT_TankText"), Tank + North * 646.f, FVector(1, 0, 0), FVector::UpVector, 760.f, 200.f, North);
}

void Ridgetop(HouseCampusPolish::FParts& P)
{
    // The radio station's mast on the ridge west of town, three red beacons up it (emission only)...
    {
        const float X = -26000.f, Y = CrestY + 1500.f;
        const FVector Base(X, Y, Slope(X, Y) - 50.f);
        const float H = 7200.f, Rr = 70.f;
        FVector Leg[3];
        for (int32 K = 0; K < 3; ++K) Leg[K] = FVector(FMath::Cos(K * 2.f * PI / 3.f) * Rr, FMath::Sin(K * 2.f * PI / 3.f) * Rr, 0.f);
        for (int32 K = 0; K < 3; ++K) HouseCampusFallKit::Bar(P.Skyline, TEXT("CPT_MastRed"), Base + Leg[K], Base + Leg[K] + Up(H), 9.f, 9.f, North);
        for (float Z = 0.f; Z < H; Z += 320.f)
            for (int32 K = 0; K < 3; ++K) HouseCampusFallKit::Bar(P.Skyline, TEXT("CPT_Steel"), Base + Leg[K] + Up(Z), Base + Leg[(K + 1) % 3] + Up(Z + 320.f), 4.f, 4.f, North);
        for (const float Z : { 2400.f, 4800.f, H + 60.f }) HouseCampusFallKit::Ball(P.Skyline, TEXT("CPT_Beacon"), Base + Up(Z), 110.f, 8);
    }
    // ...and the fire lookout on the peak east of it: steel legs, the cab with its windows all round, the hip roof.
    {
        float X = 14000.f, Best = -1.f;
        for (float Cx = 9000.f; Cx <= 22000.f; Cx += 500.f) if (Crest(Cx) > Best) { Best = Crest(Cx); X = Cx; }
        const float Y = CrestY, Z = Slope(X, Y);
        for (const float Sx : { -160.f, 160.f }) for (const float Sy : { -160.f, 160.f })
            HouseCampusFallKit::Bar(P.Skyline, TEXT("CPT_Steel"), FVector(X + Sx * 1.6f, Y + Sy * 1.6f, Z - 50.f), FVector(X + Sx, Y + Sy, Z + 1500.f), 10.f, 10.f, North);
        P.Skyline.BoxMinMax(TEXT("CPT_White"), FVector(X - 210.f, Y - 210.f, Z + 1500.f), FVector(X + 210.f, Y + 210.f, Z + 1580.f), Plain());
        P.Skyline.BoxMinMax(TEXT("CPT_WinDark"), FVector(X - 200.f, Y - 200.f, Z + 1580.f), FVector(X + 200.f, Y + 200.f, Z + 1760.f), Plain());
        Spire(P.Skyline, TEXT("CPT_RoofDark"), FVector2D(X, Y), 240.f, Z + 1760.f, Z + 1900.f);
    }
}

/** The near ridge's canopy in October: dark evergreen, rust and gold in blotches, bare grey here and there (seamless). */
void PaintFallForest(FHallImage& Img)
{
    constexpr int32 N = 256;
    Img.Init(N, N, FLinearColor::Black);
    auto F = [](float X, float Y, int32 Seed) { return HallFbm(X * .035f, Y * .035f, 4, Seed); };
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            // Blend four offset samples so the tile wraps without a seam.
            const float U = float(X) / N, V = float(Y) / N;
            auto S = [&](int32 Seed) { return F(X, Y, Seed) * (1 - U) * (1 - V) + F(X - N, Y, Seed) * U * (1 - V) + F(X, Y - N, Seed) * (1 - U) * V + F(X - N, Y - N, Seed) * U * V; };
            const float Kind = S(11), Shade = S(23);
            // The crowns: a fine dappling, each crown a few metres across at the ridge's scale.
            const float Crown = HallFbm(X * .21f, Y * .21f, 2, 37) * (1 - U) * (1 - V) + HallFbm((X - N) * .21f, Y * .21f, 2, 37) * U * (1 - V) + HallFbm(X * .21f, (Y - N) * .21f, 2, 37) * (1 - U) * V + HallFbm((X - N) * .21f, (Y - N) * .21f, 2, 37) * U * V;
            FLinearColor C = Kind < .38f ? FLinearColor(.05f, .09f, .05f) : Kind < .52f ? FLinearColor(.42f, .12f, .04f) : Kind < .64f ? FLinearColor(.55f, .38f, .07f) : Kind < .72f ? FLinearColor(.22f, .16f, .1f) : Kind < .82f ? FLinearColor(.5f, .08f, .05f) : FLinearColor(.13f, .14f, .06f);
            Img.At(X, Y) = C * (.55f + .45f * Shade) * (.6f + .7f * Crown);
        }
}
}

void HouseCampusPolish::TownLook(UHouseBlackHallSubsystem* Look)
{
    namespace TI = HouseCampusPolishTownImpl;
    using HouseCampusInteriorsImpl::PaintText;
    FHallImage Forest;
    TI::PaintFallForest(Forest);
    UTexture2D* Canopy = HallMakeTexture(Forest, true, false, TEXT("T_CPT_FallForest"), true);
    if (Canopy) Look->Textures.Add(TEXT("T_CPT_FallForest"), Canopy);
    HallMakeTextured(Look, TEXT("CPT_FallForest"), Canopy, nullptr, .95f, FLinearColor(.9f, .9f, .9f), 1.f, 0.f, FLinearColor(.006f, .006f, .008f));
    PaintText(Look, TEXT("CPT_TankText"), 512, 128, FLinearColor(.5f, .52f, .54f), FLinearColor(.07f, .09f, .2f), { { TEXT("CALDER"), .78f } }, false);
    PaintText(Look, TEXT("CPT_InnSign"), 1024, 128, FLinearColor(.02f, .02f, .02f), FLinearColor(.95f, .55f, .35f), { { TEXT("CALDER INN"), .78f } }, false, FLinearColor(1.2f, .5f, .25f));
    struct FSpec { const TCHAR* Key; FLinearColor Tint; float Rough, Metal; FLinearColor Emission; };
    const FSpec Plains[] = {
        { TEXT("CPT_RidgeMid"), FLinearColor(.075f, .105f, .14f), 1.f, 0.f, FLinearColor(.011f, .016f, .023f) },   // as the valley's north side
        { TEXT("CPT_RidgeFar"), FLinearColor(.12f, .15f, .20f), 1.f, 0.f, FLinearColor(.021f, .028f, .039f) },
        { TEXT("CPT_BrickRed"), FLinearColor(.24f, .09f, .06f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CPT_BrickTan"), FLinearColor(.42f, .32f, .22f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CPT_Stone"), FLinearColor(.45f, .43f, .39f), .85f, 0.f, FLinearColor::Black },
        { TEXT("CPT_RoofDark"), FLinearColor(.07f, .07f, .08f), .85f, 0.f, FLinearColor::Black },
        { TEXT("CPT_Copper"), FLinearColor(.18f, .36f, .3f), .6f, .3f, FLinearColor::Black },
        { TEXT("CPT_White"), FLinearColor(.66f, .66f, .64f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CPT_SidingYellow"), FLinearColor(.55f, .48f, .3f), .85f, 0.f, FLinearColor::Black },
        { TEXT("CPT_SidingBlue"), FLinearColor(.3f, .36f, .42f), .85f, 0.f, FLinearColor::Black },
        { TEXT("CPT_Steel"), FLinearColor(.32f, .33f, .34f), .55f, .7f, FLinearColor::Black },
        { TEXT("CPT_TankSilver"), FLinearColor(.5f, .52f, .54f), .5f, .4f, FLinearColor::Black },
        { TEXT("CPT_MastRed"), FLinearColor(.4f, .07f, .05f), .7f, .2f, FLinearColor::Black },
        { TEXT("CPT_River"), FLinearColor(.03f, .04f, .06f), .08f, .2f, FLinearColor(.008f, .01f, .018f) },
        { TEXT("CPT_WinDark"), FLinearColor(.025f, .03f, .04f), .2f, 0.f, FLinearColor(.004f, .005f, .008f) },
        { TEXT("CPT_WinLit"), FLinearColor(.9f, .7f, .4f), .3f, 0.f, FLinearColor(2.6f, 1.7f, .8f) },
        { TEXT("CPT_Beacon"), FLinearColor(.9f, .1f, .05f), .3f, 0.f, FLinearColor(30.f, 2.f, .8f) },
        { TEXT("CPT_Trunk"), FLinearColor(.1f, .08f, .06f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CPT_LeafRust"), FLinearColor(.4f, .13f, .04f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CPT_LeafGold"), FLinearColor(.55f, .38f, .08f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CPT_LeafRed"), FLinearColor(.45f, .07f, .04f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CPT_LeafGreen"), FLinearColor(.05f, .1f, .06f), .9f, 0.f, FLinearColor::Black },
    };
    for (const FSpec& S : Plains) HallMakePlain(Look, S.Key, S.Tint, S.Rough, S.Metal, S.Emission);
}

void HouseCampusPolish::Town(FParts& P)
{
    namespace TI = HouseCampusPolishTownImpl;
    FRandomStream R(1986 + 41);
    if (!HouseCalderTerrain::Enabled()) TI::Ridges(P.Skyline);   // the outer land is the ridges and the valley floor
    TI::Houses(P, R);
    TI::Trees(P, R);
    TI::MainStreet(P, R);
    TI::Courthouse(P, R);
    TI::Churches(P);
    TI::RiverAndMill(P, R);
    TI::WaterTower(P);
    TI::Ridgetop(P);
}
