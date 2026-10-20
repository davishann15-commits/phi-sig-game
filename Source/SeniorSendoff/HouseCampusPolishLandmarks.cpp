// Calder State exterior polish, step 3c (landmarks and homecoming): the bronze goose on its granite plinth in the quad
// (CLASS OF 1961), the library forecourt's fountain drained for the fall with leaves in its basin, the flagpole plaza with
// the school's flag; for homecoming, the alumni tent (WELCOME HOME ALUMNI: tables, coffee urns, cider jugs), crepe
// streamers twisted between the walk lamps, and the bonfire's site scaled to the green (rope stanchions round it, the
// grass burned in a wide ring); and the clock tower's base made to read on every face (steps up to its door, the ashlar
// coursing, beds of mums at its corners, benches round the plaza, the bronze plaque). No real flags or seals.
#include "HouseCampusPolish.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderTerrain.h"
#include "HouseCampusFall.h"
#include "HouseCampusFallKit.h"
#include "HouseCampusInteriorsRooms.h"
#include "HouseCampusLayout.h"
#include "HouseClockTowerMesh.h"
#include "HouseExteriorSiteLayout.h"
#include "Engine/Texture2D.h"

namespace HouseCampusPolishLandmarksImpl
{
// Today's grade and paving top: the clock tower's base is built on them and raised with the tower (HouseClockTowerSite::Lift);
// everything else stands on the ground where it is (the Calder terrain).
constexpr float G = -60.f, PaveZ = -57.f;
const FHallUV& Plain() { return HouseCampusFallKit::PlainUV(); }
FVector Up(float Z) { return FVector(0.f, 0.f, Z); }
FVector ReadRight(const FVector& N) { return FVector::CrossProduct(FVector::UpVector, -N).GetSafeNormal(); }
float GroundAt(const FVector2D& P) { return HouseExteriorSite::GroundHeight(P.X, P.Y); }
/** The lowest ground under a round footprint (its centre and sixteen points round it at R): where something built level
 *  stands on a slope, its uphill side let into the ground. Today's -60 on the flat campus. */
float LowestGround(const FVector2D& C, float R)
{
    float Z = GroundAt(C);
    for (int32 K = 0; K < 16; ++K)
    {
        const float A = 2.f * PI * K / 16.f;
        Z = FMath::Min(Z, GroundAt(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R));
    }
    return Z;
}

/** The first candidate that is clear of everything (and Radius of room), else none. */
bool Pick(const HouseCampusPolish::FParts& P, std::initializer_list<FVector2D> Candidates, float Radius, FVector2D& Out)
{
    for (const FVector2D& C : Candidates) if (P.Clear(C, Radius)) { Out = C; return true; }
    return false;
}

/** A goose standing alert on a granite plinth, its bronze gone brown-green; the class's plaque on the plinth's front. */
void GooseStatue(HouseCampusPolish::FParts& P, const FVector& Base, float Yaw)
{
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    const FVector F = Q.GetForwardVector(), S = Q.GetRightVector();
    P.Solid.Box(TEXT("CP_Granite"), FTransform(Q, Base + Up(12.f)), FVector(110.f, 80.f, 12.f), FHallUV::World(100.f));
    P.Solid.Box(TEXT("CP_Granite"), FTransform(Q, Base + Up(84.f)), FVector(90.f, 62.f, 62.f), FHallUV::World(100.f));
    P.Far.Box(TEXT("CP_Granite"), FTransform(Q, Base + Up(152.f)), FVector(96.f, 68.f, 6.f), Plain());
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_ClassPlaque"), Base + F * 90.6f + Up(88.f), ReadRight(F), FVector::UpVector, 90.f, 60.f, F);
    // The goose (about life and a half): a body, the long black neck up and forward, the head, the bill, the tail.
    FHallMesh& M = P.Far;
    const FVector B0 = Base + Up(158.f);
    auto At = [&](float X, float Y, float Z) { return B0 + F * X + S * Y + Up(Z); };
    M.Dome(TEXT("CP_Bronze"), At(-10.f, 0.f, 40.f), FVector::UpVector, 46.f, 34.f, 16);
    M.Dome(TEXT("CP_Bronze"), At(-10.f, 0.f, 40.f), -FVector::UpVector, 46.f, 26.f, 16);
    for (int32 K = 0; K < 3; ++K) M.Dome(TEXT("CP_Bronze"), At(-48.f + K * 40.f, 0.f, 42.f + K * 2.f), FVector::UpVector, 36.f, 30.f, 12);
    TArray<FVector> Spine;
    for (int32 K = 0; K <= 8; ++K) { const float T = K / 8.f; Spine.Add(At(30.f + 30.f * T + 10.f * FMath::Sin(T * PI), 0.f, 52.f + 80.f * T)); }
    for (int32 K = 0; K + 1 < Spine.Num(); ++K) M.Cylinder(TEXT("CP_Bronze"), Spine[K], Spine[K + 1], FMath::Lerp(15.f, 9.f, K / 8.f), 10, Plain());
    const FVector Head = Spine.Last() + F * 8.f;
    M.Dome(TEXT("CP_Bronze"), Head, F, 12.f, 20.f, 10);
    M.Dome(TEXT("CP_Bronze"), Head, -F, 12.f, 10.f, 10);
    M.Cylinder(TEXT("CP_Bronze"), Head + F * 16.f - Up(2.f), Head + F * 34.f - Up(5.f), 4.f, 8, Plain());   // the bill
    M.Cylinder(TEXT("CP_Bronze"), At(-60.f, 0.f, 50.f), At(-86.f, 0.f, 62.f), 10.f, 8, Plain());   // the tail
    for (const float Sd : { -1.f, 1.f })
    {
        M.Cylinder(TEXT("CP_Bronze"), At(-5.f, Sd * 14.f, 8.f), At(-5.f, Sd * 14.f, -2.f), 3.f, 6, Plain());
        M.Box(TEXT("CP_Bronze"), FTransform(Q, At(2.f, Sd * 14.f, -3.f)), FVector(10.f, 5.f, 1.f), Plain());
        M.Box(TEXT("CP_Bronze"), FTransform(Q * FRotator(10.f, 0.f, Sd * 8.f).Quaternion(), At(-14.f, Sd * 30.f, 56.f)), FVector(50.f, 4.f, 16.f), Plain());   // folded wings
    }
}

/** The forecourt fountain, drained for the season: a stone basin, the dry bowl on its pedestal, leaves in the bottom. */
void DryFountain(HouseCampusPolish::FParts& P, const FVector& C, float R)
{
    FHallMesh& S = P.Solid;
    constexpr int32 N = 32;
    for (int32 K = 0; K < N; ++K)
    {
        const float A0 = 2.f * PI * K / N, A1 = 2.f * PI * (K + 1) / N;
        const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0.f), D1(FMath::Cos(A1), FMath::Sin(A1), 0.f);
        S.Quad(TEXT("CP_Granite"), C + D0 * R, C + D1 * R, C + D1 * R + Up(46.f), C + D0 * R + Up(46.f), (D0 + D1).GetSafeNormal(), FHallUV::World(100.f));
        S.Quad(TEXT("CP_Granite"), C + D1 * (R - 30.f), C + D0 * (R - 30.f), C + D0 * (R - 30.f) + Up(46.f), C + D1 * (R - 30.f) + Up(46.f), -(D0 + D1).GetSafeNormal(), FHallUV::World(100.f));
        S.Quad(TEXT("CP_GraniteCap"), C + D0 * (R - 32.f) + Up(46.f), C + D1 * (R - 32.f) + Up(46.f), C + D1 * (R + 3.f) + Up(46.f), C + D0 * (R + 3.f) + Up(46.f), FVector::UpVector, FHallUV::World(100.f));
        P.Detail.Quad(TEXT("CP_BasinDry"), C + D0 * (R - 30.f) + Up(6.f), C + D1 * (R - 30.f) + Up(6.f), C + Up(6.f), C + Up(6.f), FVector::UpVector, FHallUV::World(160.f));
    }
    S.Cylinder(TEXT("CP_Granite"), C, C + Up(110.f), 22.f, 12, Plain());
    HouseCampusFallKit::Lathe(P.Far, TEXT("CP_Granite"), C + Up(110.f), { FVector2D(.1f, 0.f), FVector2D(30.f, 4.f), FVector2D(70.f, 22.f), FVector2D(74.f, 30.f), FVector2D(66.f, 30.f), FVector2D(.1f, 22.f) }, 20);
    P.Far.Cylinder(TEXT("CP_Bronze"), C + Up(140.f), C + Up(190.f), 3.f, 8, Plain());
    // Leaves down in the basin, more on the downwind side.
    FRandomStream Rn(31);
    static const FName Keys[] = { TEXT("CP_LeafRed"), TEXT("CP_LeafOrange"), TEXT("CP_LeafBrown"), TEXT("CP_LeafYellow") };
    for (int32 K = 0; K < 260; ++K)
    {
        const float A = Rn.FRandRange(0.f, 2.f * PI), D = FMath::Sqrt(Rn.FRand()) * (R - 36.f);
        const FVector Lp = C + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * D + Up(6.4f + Rn.FRand() * 3.f * (FMath::Cos(A) > 0.f ? 1.f : .3f));
        const FRotator Ro(Rn.FRandRange(-25.f, 25.f), Rn.FRandRange(0.f, 360.f), Rn.FRandRange(-25.f, 25.f));
        const FVector X = Ro.RotateVector(FVector(1, 0, 0)) * 4.f, Y = Ro.RotateVector(FVector(0, 1, 0)) * 3.f;
        P.Detail.Quad(Keys[Rn.RandRange(0, 3)], Lp - X - Y, Lp + X - Y, Lp + X + Y, Lp - X + Y, Ro.RotateVector(FVector::UpVector), Plain());
    }
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_FountainSign"), C + FVector(R + 1.5f, 0.f, 30.f), ReadRight(FVector(1, 0, 0)), FVector::UpVector, 40.f, 22.f, FVector(1, 0, 0));
}

/** The flagpole plaza: a round of paving flush with the lawn, the pole with its truck and halyard, the school's flag. */
void FlagPlaza(HouseCampusPolish::FParts& P, const FVector2D& C)
{
    // A level pad: a walk's paving over the lowest ground under the round (today -57, flush with the walks).
    const float Pad = LowestGround(C, 360.f) + HouseCalderTerrain::PaveOffset(HouseCalderTerrain::EPave::Walk);
    TArray<FVector> Rim;
    for (int32 K = 0; K < 32; ++K) { const float A = 2.f * PI * K / 32.f; Rim.Add(FVector(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 360.f, Pad)); }
    HouseCampusFallKit::Fan(P.Solid, TEXT("CP_Apron"), FVector(C, Pad), Rim, FVector::UpVector);
    for (int32 K = 0; K < 32; ++K)
    {
        P.Detail.Quad(TEXT("CP_Apron"), Rim[K] - Up(5.f), Rim[(K + 1) % 32] - Up(5.f), Rim[(K + 1) % 32], Rim[K], (FVector((Rim[K] + Rim[(K + 1) % 32]) * .5f) - FVector(C, Pad)).GetSafeNormal(), FHallUV::World(200.f));
        if (K % 4 == 0) HouseCampusFallKit::Bar(P.Detail, TEXT("CP_Joint"), FVector(C, Pad + .07f), Rim[K] + Up(.07f), 1.2f, .08f, FVector::UpVector);
    }
    const FVector Base(C, Pad);
    P.Solid.Box(TEXT("CP_Granite"), FTransform(Base + Up(20.f)), FVector(70.f, 70.f, 20.f), Plain());
    P.Solid.Cylinder(TEXT("CP_Aluminum"), Base + Up(40.f), Base + Up(1300.f), 9.f, 12, Plain());
    P.Far.Cylinder(TEXT("CP_Aluminum"), Base + Up(1300.f), Base + Up(1700.f), 6.f, 10, Plain());
    HouseCampusFallKit::Ball(P.Far, TEXT("CP_Brass"), Base + Up(1712.f), 14.f, 10);
    HouseCampusFallKit::Bar(P.Detail, TEXT("CP_Cord"), Base + FVector(9.f, 0.f, 120.f), Base + FVector(7.f, 0.f, 1690.f), .6f, .6f, FVector(0, 1, 0));
    // The school's flag on the halyard at the top: it flies (step 3d).
    P.Flutters.Add({ Base + Up(1690.f) + FVector(6.f, 0.f, 0.f), FVector(1.f, .3f, 0.f).GetSafeNormal(), 300.f, 190.f, TEXT("CP_SchoolFlag"), false });
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_FlagPlaque"), Base + FVector(70.6f, 0.f, 22.f), ReadRight(FVector(1, 0, 0)), FVector::UpVector, 60.f, 26.f, FVector(1, 0, 0));
}

/** The alumni tent: a white frame tent with its banner, two rows of tables with the coffee and the cider. */
void AlumniTent(HouseCampusPolish::FParts& P, const FVector& C, float Yaw)
{
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    const FVector F = Q.GetForwardVector(), S = Q.GetRightVector();
    auto L = [&](float X, float Y, float Z) { return C + F * X + S * Y + Up(Z); };
    constexpr float HX = 300.f, HY = 450.f, Eave = 240.f, Ridge = 360.f;
    for (const float X : { -HX, HX }) for (float Y = -HY; Y <= HY + 1.f; Y += 300.f) HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Aluminum"), L(X, Y, 0.f), L(X, Y, Eave), 5.f, 5.f, F);
    for (float Y = -HY; Y <= HY + 1.f; Y += 300.f) HouseCampusFallKit::Bar(P.Far, TEXT("CP_Aluminum"), L(0.f, Y, Ridge), L(0.f, Y, Eave + 2.f) + F * 0.f, 2.f, 2.f, F);
    for (const float Sx : { -1.f, 1.f })
    {
        P.Far.Quad(TEXT("CP_TentWhite"), L(Sx * HX, -HY - 20.f, Eave), L(Sx * HX, HY + 20.f, Eave), L(0.f, HY + 20.f, Ridge), L(0.f, -HY - 20.f, Ridge), (F * Sx * (Ridge - Eave) + Up(HX)).GetSafeNormal(), FHallUV::World(200.f));
        P.Far.Quad(TEXT("CP_TentWhite"), L(Sx * HX, HY + 20.f, Eave), L(Sx * HX, -HY - 20.f, Eave), L(0.f, -HY - 20.f, Ridge), L(0.f, HY + 20.f, Ridge), -(F * Sx * (Ridge - Eave) + Up(HX)).GetSafeNormal(), FHallUV::World(200.f));
        // The scalloped valance down each side.
        for (float Y = -HY - 20.f; Y < HY + 20.f; Y += 60.f)
        {
            const FVector A = L(Sx * HX, Y, Eave), B = L(Sx * HX, Y + 60.f, Eave), M = L(Sx * HX, Y + 30.f, Eave - 22.f);
            HouseCampusFallKit::Tri(P.Detail, (int32(Y / 60.f) & 1) ? TEXT("CP_CanopyPurple") : TEXT("CP_TentWhite"), A, B, M, F * Sx);
            HouseCampusFallKit::Tri(P.Detail, (int32(Y / 60.f) & 1) ? TEXT("CP_CanopyPurple") : TEXT("CP_TentWhite"), A, M, B, -F * Sx);
        }
    }
    for (const float Sy : { -1.f, 1.f })
    {
        HouseCampusFallKit::Tri(P.Far, TEXT("CP_TentWhite"), L(-HX, Sy * (HY + 20.f), Eave), L(HX, Sy * (HY + 20.f), Eave), L(0.f, Sy * (HY + 20.f), Ridge), S * Sy);
        HouseCampusFallKit::Tri(P.Far, TEXT("CP_TentWhite"), L(-HX, Sy * (HY + 20.f), Eave), L(0.f, Sy * (HY + 20.f), Ridge), L(HX, Sy * (HY + 20.f), Eave), -S * Sy);
    }
    // The banner across the open front (+F side's eave).
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_AlumniBanner"), L(HX + 2.f, 0.f, Eave - 32.f), ReadRight(F), FVector::UpVector, 760.f, 56.f, F);
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_AlumniBanner"), L(HX + 1.f, 0.f, Eave - 32.f), ReadRight(-F), FVector::UpVector, 760.f, 56.f, -F);
    // Tables: the coffee urns and the cider jugs, cups and doughnuts.
    for (const float Y : { -220.f, 220.f })
    {
        const FVector T = L(60.f, Y, 0.f);
        P.Solid.Box(TEXT("CP_TableTop"), FTransform(Q, T + Up(74.f)), FVector(38.f, 120.f, 2.f), Plain());
        P.Detail.Box(TEXT("CP_TableCloth"), FTransform(Q, T + Up(60.f)), FVector(39.f, 121.f, 14.f), Plain());
        for (const float Yy : { -100.f, 100.f }) for (const float Xx : { -30.f, 30.f }) HouseCampusFallKit::Bar(P.Detail, TEXT("CP_Aluminum"), T + F * Xx + S * Yy, T + F * Xx + S * Yy + Up(72.f), 2.f, 2.f, F);
        if (Y < 0.f)
            for (const float Yy : { -70.f, 0.f })
            {
                const FVector U = T + S * Yy + Up(76.f);
                HouseCampusFallKit::Lathe(P.Detail, TEXT("CP_Chrome"), U, { FVector2D(14.f, 0.f), FVector2D(16.f, 6.f), FVector2D(16.f, 46.f), FVector2D(12.f, 52.f), FVector2D(.1f, 56.f) }, 14);
                P.Detail.Box(TEXT("CP_SteelDark"), FTransform(Q, U + F * 17.f + Up(12.f)), FVector(3.f, 2.f, 2.f), Plain());   // the tap
            }
        else
            for (int32 K = 0; K < 5; ++K)
            {
                const FVector J = T + S * (-80.f + K * 32.f) + Up(76.f);
                HouseCampusFallKit::Lathe(P.Detail, TEXT("CP_Cider"), J, { FVector2D(10.f, 0.f), FVector2D(11.f, 4.f), FVector2D(11.f, 20.f), FVector2D(6.f, 27.f), FVector2D(3.f, 30.f), FVector2D(.1f, 31.f) }, 12);
            }
        for (int32 K = 0; K < 10; ++K) P.Detail.Cylinder(TEXT("CP_White"), T + F * -20.f + S * (-110.f + K * 22.f) + Up(76.f), T + F * -20.f + S * (-110.f + K * 22.f) + Up(86.f), 3.6f, 8, Plain());
    }
    P.Pools.Add({ C + Up(2.f) - FVector(0, 0, 0), 420.f, FLinearColor(1.f, .8f, .55f), .25f });
}

/** Crepe-paper streamers twisted between two lamp heads, purple and gold, sagging. */
void Streamer(HouseCampusPolish::FParts& P, const FVector& A, const FVector& B, int32 Seed)
{
    const int32 N = FMath::Clamp(int32(FVector::Dist(A, B) / 40.f), 8, 70);
    const float Sag = FVector::Dist(A, B) * .08f;
    for (int32 Strand = 0; Strand < 2; ++Strand)
        for (int32 K = 0; K < N; ++K)
        {
            const float T0 = K / float(N), T1 = (K + 1) / float(N);
            const FVector P0 = HouseCampusFallKit::Cord(A, B, T0, Sag), P1 = HouseCampusFallKit::Cord(A, B, T1, Sag);
            const FVector Along = (P1 - P0).GetSafeNormal(), Side0 = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal();
            const float Tw0 = T0 * 26.f + Strand * PI * .5f + Seed, Tw1 = T1 * 26.f + Strand * PI * .5f + Seed;
            const FVector W0 = (Side0 * FMath::Cos(Tw0) + FVector::UpVector * FMath::Sin(Tw0)) * 3.5f, W1 = (Side0 * FMath::Cos(Tw1) + FVector::UpVector * FMath::Sin(Tw1)) * 3.5f;
            const FName Key = Strand ? TEXT("CP_StreamerGold") : TEXT("CP_StreamerPurple");
            const FVector Nrm = FVector::CrossProduct(Along, W0).GetSafeNormal();
            P.Detail.Quad(Key, P0 - W0, P1 - W1, P1 + W1, P0 + W0, Nrm, Plain());
            P.Detail.Quad(Key, P0 + W0, P1 + W1, P1 - W1, P0 - W0, -Nrm, Plain());
        }
}

/** The bonfire's site, scaled to the green: the grass burned in a wide ring, rope on stanchions round it, the sign. */
void BonfireSite(HouseCampusPolish::FParts& P, const FVector2D& C)
{
    FRandomStream R(1018);
    // The burned ring is flat at the fire's ground; each stanchion and the sign stand on the ground where they are.
    const float Scorch = GroundAt(C) + .7f;
    TArray<FVector> Rim;
    for (int32 K = 0; K < 40; ++K)
    {
        const float A = 2.f * PI * K / 40.f, Rr = 520.f * (.82f + .25f * R.FRand());
        Rim.Add(FVector(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr, Scorch));
    }
    HouseCampusFallKit::Fan(P.Detail, TEXT("CP_Scorch"), FVector(C, Scorch), Rim, FVector::UpVector);
    constexpr int32 Posts = 14;
    constexpr float Ring = 680.f;
    TArray<FVector> Tops;
    for (int32 K = 0; K < Posts; ++K)
    {
        const float A = 2.f * PI * K / Posts;
        const FVector2D Foot = C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Ring;
        const FVector Base(Foot, GroundAt(Foot));
        if (K == 3) continue;   // an opening for the crew
        P.Solid.Cylinder(TEXT("CP_Brass"), Base, Base + Up(92.f), 3.5f, 8, Plain());
        P.Solid.Cylinder(TEXT("CP_SteelDark"), Base, Base + Up(4.f), 18.f, 12, Plain());
        HouseCampusFallKit::Ball(P.Detail, TEXT("CP_Brass"), Base + Up(95.f), 5.f, 8);
        Tops.Add(Base + Up(88.f));
    }
    for (int32 K = 0; K < Tops.Num(); ++K)
    {
        const FVector& A0 = Tops[K];
        const FVector& A1 = Tops[(K + 1) % Tops.Num()];   // round to the first post again; the crew's opening is the one long gap
        if (FVector::Dist(A0, A1) > 2.f * PI * Ring / Posts * 1.5f) continue;
        for (int32 J = 0; J < 8; ++J)
            HouseCampusFallKit::Bar(P.Detail, TEXT("CP_RopeRed"), HouseCampusFallKit::Cord(A0, A1, J / 8.f, 22.f), HouseCampusFallKit::Cord(A0, A1, (J + 1) / 8.f, 22.f), 3.f, 3.f, FVector::UpVector);
    }
    const FVector2D SignAt = C + FVector2D(1.f, -.2f).GetSafeNormal() * (Ring + 60.f);
    const FVector Sign(SignAt, GroundAt(SignAt));
    P.Solid.Cylinder(TEXT("CP_Wood"), Sign, Sign + Up(120.f), 4.f, 8, Plain());
    const FVector N = (FVector(Sign.X - C.X, Sign.Y - C.Y, 0.f)).GetSafeNormal();
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_BonfireSign"), Sign + N * 4.5f + Up(110.f), ReadRight(N), FVector::UpVector, 70.f, 46.f, N);
}

/** The clock tower's base: steps up to its door on the lawn face, mums at the corners, benches round the plaza, the bronze
 *  plaque. (The tower itself, its plinth, ashlar, door, date stone and lit lancets are HouseClockTower's.) */
void TowerBase(HouseCampusPolish::FParts& P)
{
    constexpr float CX = -9000.f, CY = -10800.f, Half = 330.f, Plinth = 395.f, Sill = 75.f;
    // Built at today's plaza datum and raised with the tower, its plaza and its inside onto Tower Knoll (0 with the terrain off).
    const float Knoll = HouseClockTowerSite::Lift(), Paving = PaveZ + Knoll, Foot = G + Knoll;
    // Four steps of 18 cm from the plaza on the north face, then a landing level with the door's sill over the plinth's
    // three 25 cm tiers (which it covers, so nothing between the top step and the door is a slot or a 25 cm step); cheek
    // walls either side. (The rise is a difference of two datums that lift together: it does not change.)
    const float Rise = (G + Sill - PaveZ) / 4.f;
    for (int32 K = 0; K < 4; ++K)
        P.Solid.BoxMinMax(TEXT("CP_Granite"), FVector(CX - 110.f, CY + Plinth - 4.f, Paving - 4.f), FVector(CX + 110.f, CY + Plinth + (4 - K) * 34.f, Paving + Rise * (K + 1)), FHallUV::World(100.f));
    P.Solid.BoxMinMax(TEXT("CP_Granite"), FVector(CX - 110.f, CY + Half, Paving - 4.f), FVector(CX + 110.f, CY + Plinth, Foot + Sill), FHallUV::World(100.f));
    for (const float S : { -1.f, 1.f }) P.Solid.BoxMinMax(TEXT("CP_Granite"), FVector(CX + S * 121.f - 11.f, CY + Half, Paving - 4.f), FVector(CX + S * 121.f + 11.f, CY + Plinth + 136.f, Foot + Sill + 20.f), FHallUV::World(100.f));
    // Beds of mums at the four corners of the plinth, in rust, gold and plum.
    static const FName Mums[] = { TEXT("CP_MumRust"), TEXT("CP_MumGold"), TEXT("CP_MumPlum") };
    FRandomStream R(1897);
    // (On the west corners only: the east ones would stand in the Union's forecourt, the way to its west doors.)
    for (const float Sx : { -1.f })
        for (const float Sy : { -1.f, 1.f })
        {
            const FVector C(CX + Sx * (Plinth + 90.f), CY + Sy * (Plinth + 90.f), Paving);
            P.Solid.BoxMinMax(TEXT("CP_Granite"), C - FVector(75.f, 75.f, 4.f), C + FVector(75.f, 75.f, 28.f), FHallUV::World(100.f));
            P.Detail.BoxMinMax(TEXT("CP_Mulch"), C - FVector(67.f, 67.f, -28.f), C + FVector(67.f, 67.f, 29.f), FHallUV::World(100.f));
            for (int32 K = 0; K < 9; ++K)
            {
                const FVector M = C + FVector((K % 3 - 1) * 42.f + R.FRandRange(-6.f, 6.f), (K / 3 - 1) * 42.f + R.FRandRange(-6.f, 6.f), 29.f);
                P.Detail.Dome(Mums[(K + int32(Sx + Sy * 2.f + 3.f)) % 3], M, FVector::UpVector, R.FRandRange(22.f, 28.f), R.FRandRange(22.f, 30.f), 10);
            }
        }
    // Benches facing the tower on the plaza's west diagonals (the east side is the Union's forecourt), and the plaque on
    // the east face.
    // (On the Calder terrain the knoll's two ramps arrive on the plaza's west side by 155 and 205 degrees: the benches stand
    // further round, at 130 and 230.)
    const bool bKnollRamps = HouseCalderFeatures::Active();
    for (const float Deg : { bKnollRamps ? 130.f : 155.f, bKnollRamps ? 230.f : 205.f })
    {
        const float A = FMath::DegreesToRadians(Deg);
        const FVector D(FMath::Cos(A), FMath::Sin(A), 0.f), Tan(-D.Y, D.X, 0.f);
        const FVector B = FVector(CX, CY, Paving) + D * 700.f;
        const FQuat Q = FRotationMatrix::MakeFromXY(-D, Tan).ToQuat();
        P.Solid.Box(TEXT("CP_Wood"), FTransform(Q, B + Up(45.f)), FVector(22.f, 90.f, 3.f), Plain());
        P.Detail.Box(TEXT("CP_Wood"), FTransform(Q * FRotator(-12.f, 0.f, 0.f).Quaternion(), B + D * 20.f + Up(72.f)), FVector(3.f, 90.f, 15.f), Plain());
        for (const float S : { -75.f, 75.f }) P.Solid.Box(TEXT("CP_Iron"), FTransform(Q, B + Tan * S + Up(22.f)), FVector(20.f, 3.f, 22.f), Plain());
    }
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_TowerPlaque"), FVector(CX + Half + .8f, CY + 120.f, Foot + 175.f), ReadRight(FVector(1, 0, 0)), FVector::UpVector, 84.f, 56.f, FVector(1, 0, 0));
}
}

void HouseCampusPolish::LandmarksLook(UHouseBlackHallSubsystem* Look)
{
    using HouseCampusInteriorsImpl::PaintText;
    PaintText(Look, TEXT("CP_ClassPlaque"), 256, 170, FLinearColor(.16f, .12f, .07f), FLinearColor(.66f, .52f, .26f), { { TEXT("THE CALDER GOOSE"), .14f }, { TEXT("GIVEN BY THE"), .1f }, { TEXT("CLASS OF 1961"), .16f }, { TEXT("HONK FOREVER"), .09f } }, true);
    PaintText(Look, TEXT("CP_TowerPlaque"), 256, 170, FLinearColor(.16f, .12f, .07f), FLinearColor(.66f, .52f, .26f), { { TEXT("THE CALDER TOWER"), .14f }, { TEXT("RAISED 1897"), .12f }, { TEXT("RESTORED BY THE"), .09f }, { TEXT("CLASS OF 1961"), .12f } }, true);
    PaintText(Look, TEXT("CP_FountainSign"), 128, 70, FLinearColor(.85f, .84f, .78f), FLinearColor(.1f, .1f, .12f), { { TEXT("DRAINED"), .26f }, { TEXT("FOR THE SEASON"), .16f } }, true);
    PaintText(Look, TEXT("CP_FlagPlaque"), 192, 80, FLinearColor(.16f, .12f, .07f), FLinearColor(.66f, .52f, .26f), { { TEXT("CALDER STATE"), .26f }, { TEXT("FOUNDED 1891"), .2f } }, false);
    PaintText(Look, TEXT("CP_AlumniBanner"), 1024, 76, FLinearColor(.3f, .08f, .4f), FLinearColor(.95f, .78f, .25f), { { TEXT("WELCOME HOME ALUMNI  -  HOMECOMING '86"), .5f } }, false, FLinearColor(.02f, .005f, .03f));
    PaintText(Look, TEXT("CP_BonfireSign"), 192, 128, FLinearColor(.85f, .82f, .7f), FLinearColor(.5f, .05f, .03f), { { TEXT("BONFIRE SITE"), .2f }, { TEXT("KEEP BEHIND"), .14f }, { TEXT("THE ROPE"), .14f }, { TEXT("CAMPUS SAFETY"), .1f } }, true);
    {
        // The school's flag: purple, a gold goose in flight on a white disc, CALDER STATE across the fly.
        FHallImage Img;
        Img.Init(256, 160, FLinearColor(.24f, .06f, .32f));
        Img.Ellipse(FVector2D(78, 80), FVector2D(52, 52), 0.f, FLinearColor(.86f, .84f, .78f));
        Img.FillPolygon({ { 44, 92 }, { 70, 70 }, { 98, 66 }, { 108, 50 }, { 118, 46 }, { 114, 56 }, { 104, 70 }, { 112, 92 }, { 84, 98 } }, FLinearColor(.78f, .58f, .16f));
        FHallTextStyle S; S.Height = 22.f; S.Stroke = 4.f; S.Spacing = .2f;
        HallDrawText(Img, TEXT("CALDER"), FVector2D(146, 74), S, FLinearColor(.86f, .72f, .26f));
        HallDrawText(Img, TEXT("STATE"), FVector2D(150, 108), S, FLinearColor(.86f, .72f, .26f));
        UTexture2D* T = HallMakeTexture(Img, true, false, TEXT("T_CP_SchoolFlag"), false);
        if (T) Look->Textures.Add(TEXT("T_CP_SchoolFlag"), T);
        HallMakeTextured(Look, TEXT("CP_SchoolFlag"), T, nullptr, .85f, FLinearColor::White, 0.f, 0.f, FLinearColor(.02f, .01f, .02f));
    }
    struct FSpec { const TCHAR* Key; FLinearColor Tint; float Rough, Metal; FLinearColor Emission; };
    const FSpec Plains[] = {
        { TEXT("CP_Granite"), FLinearColor(.38f, .37f, .36f), .7f, 0.f, FLinearColor::Black },
        { TEXT("CP_GraniteCap"), FLinearColor(.46f, .45f, .43f), .6f, 0.f, FLinearColor::Black },
        { TEXT("CP_Bronze"), FLinearColor(.2f, .17f, .1f), .45f, .85f, FLinearColor::Black },
        { TEXT("CP_BasinDry"), FLinearColor(.3f, .29f, .27f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CP_Cord"), FLinearColor(.75f, .74f, .7f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CP_TentWhite"), FLinearColor(.78f, .77f, .74f), .8f, 0.f, FLinearColor(.06f, .055f, .045f) },
        { TEXT("CP_TableCloth"), FLinearColor(.72f, .7f, .66f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CP_Cider"), FLinearColor(.45f, .22f, .05f), .2f, 0.f, FLinearColor(.02f, .008f, .0f) },
        { TEXT("CP_Scorch"), FLinearColor(.025f, .022f, .02f), .95f, 0.f, FLinearColor::Black },
        { TEXT("CP_RopeRed"), FLinearColor(.45f, .05f, .04f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CP_Mortar"), FLinearColor(.36f, .34f, .3f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CP_MumRust"), FLinearColor(.5f, .16f, .05f), .85f, 0.f, FLinearColor::Black },
        { TEXT("CP_MumGold"), FLinearColor(.7f, .5f, .08f), .85f, 0.f, FLinearColor::Black },
        { TEXT("CP_MumPlum"), FLinearColor(.3f, .06f, .2f), .85f, 0.f, FLinearColor::Black },
    };
    for (const FSpec& S : Plains) HallMakePlain(Look, S.Key, S.Tint, S.Rough, S.Metal, S.Emission);
}

void HouseCampusPolish::Landmarks(FParts& P)
{
    namespace LI = HouseCampusPolishLandmarksImpl;
    FVector2D At;
    // The goose on the lowest ground under its plinth (136: the plinth's half diagonal); the fountain on the forecourt's paving.
    if (LI::Pick(P, { FVector2D(-11900.f, -11450.f), FVector2D(-12600.f, -11500.f), FVector2D(-11300.f, -10150.f) }, 160.f, At)) LI::GooseStatue(P, FVector(At, LI::LowestGround(At, 136.f)), 90.f);
    // (On the Calder terrain the forecourt's east edge is its wall over the Senior Steps' railed ramp: the fountain stands
    // 4 m further west, inside the forecourt.)
    const FVector2D Fountain = HouseCalderFeatures::Active() ? FVector2D(-15120.f, -11420.f) : FVector2D(-14720.f, -11360.f);
    LI::DryFountain(P, FVector(Fountain, HouseCalderTerrain::SurfaceZ(Fountain.X, Fountain.Y, HouseCalderTerrain::EPave::Walk)), 230.f);
    P.KeepClear.Add(FVector(Fountain, 260.f));
    if (LI::Pick(P, { FVector2D(-600.f, -7700.f), FVector2D(500.f, -7400.f), FVector2D(6900.f, -7500.f), FVector2D(-800.f, -8600.f) }, 380.f, At)) { LI::FlagPlaza(P, At); P.KeepClear.Add(FVector(At, 380.f)); }
    if (LI::Pick(P, { FVector2D(6700.f, -8500.f), FVector2D(-500.f, -8700.f), FVector2D(7200.f, -7700.f), FVector2D(1300.f, -7200.f) }, 560.f, At)) { LI::AlumniTent(P, FVector(At, LI::LowestGround(At, 541.f)), 0.f); P.KeepClear.Add(FVector(At, 560.f)); }   // its lowest leg (541: to the corner poles)
    if (HouseCampusFall::Enabled() && HouseCampusFall::BonfireSpot().Z > 0.f) LI::BonfireSite(P, FVector2D(HouseCampusFall::BonfireSpot()));
    // Streamers from lamp to lamp along the promenade and the quad.
    {
        TArray<FVector2D> Lamps;
        HouseCampusLayout::LampPoints(Lamps);
        int32 Seed = 0;
        for (int32 I = 0; I < Lamps.Num(); ++I)
        {
            int32 Best = -1;
            float BestD = 2400.f;
            for (int32 J = 0; J < Lamps.Num(); ++J)
            {
                if (J == I || (Lamps[J].X < Lamps[I].X) || (Lamps[J].X == Lamps[I].X && Lamps[J].Y <= Lamps[I].Y)) continue;
                const float D = FVector2D::Distance(Lamps[I], Lamps[J]);
                if (D < BestD) { BestD = D; Best = J; }
            }
            if (Best < 0) continue;
            const FVector2D Mid = (Lamps[I] + Lamps[Best]) * .5f;
            if (HouseCampusLayout::OnTrafficLane(Mid) || Mid.Y > -5600.f) continue;
            // Never through a building (two lamps either side of a corner).
            bool bThrough = false;
            for (int32 K = 1; K < 12 && !bThrough; ++K)
            {
                const FVector2D Q = FMath::Lerp(Lamps[I], Lamps[Best], K / 12.f);
                for (const HouseCampusLayout::FBlock& Blk : HouseCampusLayout::Blocks()) if (Blk.Rect().ExpandBy(60.f).IsInside(Q)) { bThrough = true; break; }
            }
            if (bThrough) continue;
            LI::Streamer(P, FVector(Lamps[I], LI::GroundAt(Lamps[I]) + 470.f), FVector(Lamps[Best], LI::GroundAt(Lamps[Best]) + 470.f), Seed++);
        }
    }
    LI::TowerBase(P);
}
