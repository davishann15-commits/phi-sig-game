// Calder State exterior polish, step 3b (buildings): each campus building its own character over the exterior's shell
// (HouseExteriorCampus, which gives every one the same box, base course, cornice, window grid and entrance). From each
// building's identity:
// - massing and roofs: Mercer Library's copper dome on its drum, Old Main's slate mansard with dormers and its cupola and
//   goose weathervane, Harlow's and the Commons' pitched roofs with chimneys, Calder Hall's steep collegiate gable with
//   crenellated ends, Hollis's observatory dome and fume stacks, the Union's lit glass lantern over its atrium, the
//   fieldhouse's vault, Whitcomb's sawtooth studio lights, the music hall's gable with its round window, Pratt's water
//   tank, the plant's smokestack, Alumni Hall's hipped slate roof and dormers, penthouses and TV antennas elsewhere;
// - entrances: a pediment over the classical doors, wall lanterns beside the others (emission);
// - body detail: fire escapes on the old dorms and halls, autumn ivy on the old stone and brick, window wells, meters;
// - windows (closed floors only, the open floors are real rooms): blinds at all heights, curtains, a few sashes up, window
//   air conditioners and box fans in the dorms.
// Built in each building's canonical frame (main entrance on +Y) from the plan and the interiors' own window grid, then
// turned to face the way it does. Roofs and everything above the ground floor are drawn only (nobody can reach them).
#include "HouseCampusPolish.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusFallKit.h"
#include "HouseCampusInteriors.h"
#include "HouseCampusLayout.h"
#include "HouseExteriorCampus.h"
#include "Engine/Texture2D.h"

namespace HouseCampusPolishBuildingsImpl
{
constexpr float G = -60.f;
const FHallUV& Plain() { return HouseCampusFallKit::PlainUV(); }
FVector Up(float Z) { return FVector(0.f, 0.f, Z); }
FVector ReadRight(const FVector& N) { return FVector::CrossProduct(FVector::UpVector, -N).GetSafeNormal(); }

/** Appends Src to Dst turned and moved by T. */
void AppendTransformed(FHallMesh& Dst, const FHallMesh& Src, const FTransform& T)
{
    for (const FName& Key : Src.Order)
    {
        const FHallSection& From = Src.Sections[Key];
        FHallSection& To = Dst.Sec(Key);
        const int32 Base = To.V.Num();
        for (int32 I = 0; I < From.V.Num(); ++I)
        {
            To.V.Add(T.TransformPosition(From.V[I]));
            To.N.Add(T.TransformVectorNoScale(From.N[I]));
            To.UV.Add(From.UV[I]);
            To.T.Add(FProcMeshTangent(T.TransformVectorNoScale(From.T[I].TangentX), From.T[I].bFlipTangentY));
        }
        for (const int32 I : From.I) To.I.Add(Base + I);
    }
}

/** One building in its canonical frame: W wide (X), D deep (Y), its main face on +Y; its roof deck's top. */
struct FBld
{
    int32 Seed = 0;
    const HouseCampusLayout::FBlock* B = nullptr;
    float W = 0.f, D = 0.f, Roof = 0.f, Height = 0.f;
    FName Body;
    FTransform T;
    FBld(int32 InSeed) : Seed(InSeed), B(&HouseCampusLayout::Blocks()[InSeed])
    {
        const TCHAR Main = B->Faces[0];
        const bool bTurned = Main == TEXT('E') || Main == TEXT('W');
        W = bTurned ? B->Y1 - B->Y0 : B->X1 - B->X0;
        D = bTurned ? B->X1 - B->X0 : B->Y1 - B->Y0;
        Height = B->Height;
        Roof = G + Height + 38.f;
        Body = B->Style == 1 ? FName(TEXT("EX_CampusLimestone")) : B->Style == 2 ? FName(TEXT("EX_CampusWarmBrick")) : FName(TEXT("EX_Brick"));
        T = HouseCampusLayout::Frame(Seed);   // turned to face the way it does, on its level pad (the Calder terrain)
    }
    FString Id() const { return B->Id; }
};

/** The lowest (or highest) ground beside side Side of building B, in its canonical frame (the floor at G), over U0..U1 along
 *  the side and D0..D1 out from its face (HouseCampus::FoundationGround; asked only where the relief round it is not flat). */
float FootGround(const FBld& B, int32 Side, float U0, float U1, float D0, float D1, bool bHighest)
{
    float Best = bHighest ? -FLT_MAX : FLT_MAX;
    for (int32 I = 0; I <= 4; ++I)
        for (int32 J = 0; J <= 2; ++J)
        {
            const float U = FMath::Lerp(U0, U1, I / 4.f), Dist = FMath::Lerp(D0, D1, J / 2.f);
            const float Plane = Side == 0 ? B.D * .5f + Dist : Side == 1 ? -B.D * .5f - Dist : Side == 2 ? -B.W * .5f - Dist : B.W * .5f + Dist;
            const float Z = HouseCampus::FoundationGround(B.Seed, Side < 2 ? FVector2D(U, Plane) : FVector2D(Plane, U));
            Best = bHighest ? FMath::Max(Best, Z) : FMath::Min(Best, Z);
        }
    return Best;
}

// ------------------------------------------------------------------ roofs
/** A pitched roof over the deck: ridge along X at Rise, eaves overhanging; the gable ends in the walls' material. */
void Gable(FHallMesh& M, const FBld& B, float Rise, FName Cover, bool bEndsCrenellated)
{
    const float HW = B.W * .5f + 25.f, HD = B.D * .5f + 45.f, Z0 = B.Roof + 70.f, Z1 = Z0 + Rise;
    const FVector UV0(0, 0, 0);
    for (const float S : { 1.f, -1.f })
    {
        const FVector A(-HW, S * HD, Z0 - 12.f), Bb(HW, S * HD, Z0 - 12.f), C(HW, 0.f, Z1), Dd(-HW, 0.f, Z1);
        const FVector N = FVector(0.f, S * Rise, HD).GetSafeNormal();
        M.Quad(Cover, A, Bb, C, Dd, N, FHallUV::World(150.f));
        M.Quad(Cover, A - Up(10.f), Dd - Up(10.f), C - Up(10.f), Bb - Up(10.f), -N, FHallUV::World(150.f));   // the eave's underside
    }
    for (const float S : { -1.f, 1.f })
    {
        const float X = S * (B.W * .5f + 2.f);
        // The gable end wall up to the roof, standing proud of it when crenellated.
        const float Extra = bEndsCrenellated ? 70.f : 0.f;
        TArray<FVector> Rim = { FVector(X, -B.D * .5f, B.Roof - 40.f), FVector(X, B.D * .5f, B.Roof - 40.f), FVector(X, B.D * .5f, Z0 + Extra - 10.f), FVector(X, 0.f, Z1 + Extra), FVector(X, -B.D * .5f, Z0 + Extra - 10.f) };
        HouseCampusFallKit::Fan(M, B.Body, FVector(X, 0.f, (B.Roof + Z1) * .5f), Rim, FVector(S, 0.f, 0.f));
        if (bEndsCrenellated)
        {
            for (int32 K = -3; K <= 3; ++K)
            {
                const float Y = K * B.D / 7.f, Zt = FMath::Lerp(Z1 + Extra, Z0 + Extra - 10.f, FMath::Abs(Y) / (B.D * .5f));
                if (K % 2 == 0) M.Box(B.Body, FTransform(FVector(X, Y, Zt + 30.f)), FVector(22.f, B.D / 15.f, 30.f), FHallUV::World(100.f));
            }
            for (const float Y : { -B.D * .5f, B.D * .5f }) M.Box(TEXT("CP_Ashlar"), FTransform(FVector(X, Y, Z0 + 120.f)), FVector(30.f, 30.f, 180.f), FHallUV::World(120.f));   // pinnacles
            for (const float Y : { -B.D * .5f, B.D * .5f }) HouseCampusFallKit::Lathe(M, TEXT("CP_Ashlar"), FVector(X, Y, Z0 + 300.f), { FVector2D(34.f, 0.f), FVector2D(.1f, 110.f) }, 4);
        }
        else
        {
            // Stone coping up the rakes.
            for (const float Sy : { -1.f, 1.f }) HouseCampusFallKit::Bar(M, TEXT("CP_Ashlar"), FVector(X, Sy * (B.D * .5f + 40.f), Z0 - 6.f), FVector(X, 0.f, Z1 + 8.f), 36.f, 18.f, FVector(S, 0, 0));
        }
    }
    HouseCampusFallKit::Bar(M, Cover, FVector(-HW, 0.f, Z1 + 4.f), FVector(HW, 0.f, Z1 + 4.f), 16.f, 10.f, FVector::UpVector);   // ridge
}
/** A hipped roof: four planes to a ridge along the longer side. */
void Hip(FHallMesh& M, const FBld& B, float Rise, FName Cover)
{
    const float HW = B.W * .5f + 30.f, HD = B.D * .5f + 30.f, Z0 = B.Roof + 70.f, Z1 = Z0 + Rise;
    const bool bAlongX = HW >= HD;
    const float R = bAlongX ? HW - HD : HD - HW;
    const FVector Ra = bAlongX ? FVector(-R, 0, Z1) : FVector(0, -R, Z1), Rb = bAlongX ? FVector(R, 0, Z1) : FVector(0, R, Z1);
    const FVector C[4] = { FVector(-HW, -HD, Z0), FVector(HW, -HD, Z0), FVector(HW, HD, Z0), FVector(-HW, HD, Z0) };
    auto Plane = [&](const FVector& P0, const FVector& P1, const FVector& Q1, const FVector& Q0)
    {
        const FVector Mid = (P0 + P1) * .5f;
        FVector N = FVector::CrossProduct(P1 - P0, Q0 - P0).GetSafeNormal();
        if (FVector::DotProduct(N, FVector(Mid.X, Mid.Y, 0.f)) < 0.f) N = -N;
        if (FVector::DistSquared(Q0, Q1) < 1.f) HouseCampusFallKit::Tri(M, Cover, P0, P1, Q0, N, FHallUV::World(150.f));
        else M.Quad(Cover, P0, P1, Q1, Q0, N, FHallUV::World(150.f));
    };
    if (bAlongX)
    {
        Plane(C[0], C[1], Rb, Ra); Plane(C[2], C[3], Ra, Rb);
        Plane(C[1], C[2], Rb, Rb); Plane(C[3], C[0], Ra, Ra);
    }
    else
    {
        Plane(C[1], C[2], Rb, Ra); Plane(C[3], C[0], Ra, Rb);
        Plane(C[0], C[1], Ra, Ra); Plane(C[2], C[3], Rb, Rb);
    }
    HouseCampusFallKit::Bar(M, Cover, Ra + Up(5.f), Rb + Up(5.f), 14.f, 10.f, FVector::UpVector);
    M.BoxMinMax(Cover, FVector(-HW, -HD, B.Roof - 30.f), FVector(HW, HD, Z0), FHallUV::World(150.f));   // the eaves' band
}
/** A slate mansard: steep lower slopes from the eaves, a flat top; dormers in line with the window columns below. */
void Mansard(FHallMesh& M, const FBld& B)
{
    const float HW = B.W * .5f + 30.f, HD = B.D * .5f + 30.f, In = 120.f, Z0 = B.Roof + 60.f, Z1 = Z0 + 400.f;
    const FVector Lo[4] = { FVector(-HW, -HD, Z0), FVector(HW, -HD, Z0), FVector(HW, HD, Z0), FVector(-HW, HD, Z0) };
    const FVector Hi[4] = { FVector(-HW + In, -HD + In, Z1), FVector(HW - In, -HD + In, Z1), FVector(HW - In, HD - In, Z1), FVector(-HW + In, HD - In, Z1) };
    for (int32 E = 0; E < 4; ++E)
    {
        const int32 F = (E + 1) % 4;
        const FVector Mid = (Lo[E] + Lo[F]) * .5f;
        FVector N = FVector::CrossProduct(Lo[F] - Lo[E], Hi[E] - Lo[E]).GetSafeNormal();
        if (FVector::DotProduct(N, FVector(Mid.X, Mid.Y, 0.f)) < 0.f) N = -N;
        M.Quad(TEXT("CP_Slate"), Lo[E], Lo[F], Hi[F], Hi[E], N, FHallUV::World(150.f));
        HouseCampusFallKit::Bar(M, TEXT("CP_Ashlar"), Hi[E] + Up(6.f), Hi[F] + Up(6.f), 22.f, 14.f, FVector::UpVector);   // the curb
    }
    M.Quad(TEXT("CP_Slate"), Hi[0], Hi[1], Hi[2], Hi[3], FVector::UpVector, FHallUV::World(150.f));
    M.BoxMinMax(TEXT("CP_Ashlar"), FVector(-HW - 6.f, -HD - 6.f, B.Roof - 30.f), FVector(HW + 6.f, HD + 6.f, Z0), FHallUV::World(120.f));   // the cornice
    // Dormers on the long sides.
    TArray<FVector2D> Cols;
    HouseCampusInteriors::WindowColumns(B.Seed, 0, Cols);
    for (const float S : { 1.f, -1.f })
        for (int32 I = 0; I < Cols.Num(); I += 2)
        {
            const float X = (Cols[I].X + Cols[I].Y) * .5f;
            if (FMath::Abs(X) > HW - 160.f) continue;
            const FVector Face(X, S * (HD - 20.f), Z0 + 60.f);
            const FVector N(0.f, S, 0.f);
            M.Box(TEXT("CP_PaintWhite"), FTransform(Face + Up(110.f) - N * 40.f), FVector(70.f, 45.f, 110.f), Plain());
            HouseCampusFallKit::Card(M, (I / 2) % 3 == 0 ? TEXT("EX_Window") : TEXT("EX_Glass"), Face + N * 5.2f + Up(105.f), ReadRight(N), FVector::UpVector, 90.f, 140.f, N);
            for (const float Sx : { -1.f, 1.f })
                M.Quad(TEXT("CP_Slate"), Face + FVector(Sx * 82.f, S * 8.f, 214.f), Face + FVector(0.f, S * 8.f, 268.f), Face + FVector(0.f, -S * 90.f, 268.f), Face + FVector(Sx * 82.f, -S * 90.f, 214.f), FVector(Sx * .6f, 0.f, .8f).GetSafeNormal(), Plain());
            HouseCampusFallKit::Tri(M, TEXT("CP_PaintWhite"), Face + FVector(-74.f, S * 6.f, 220.f), Face + FVector(74.f, S * 6.f, 220.f), Face + FVector(0.f, S * 6.f, 262.f), N);
        }
}
/** Old Main's cupola: a square base with louvres, an octagonal lantern, a copper dome, the goose weathervane. */
void Cupola(FHallMesh& M, FHallMesh& Glow, const FVector& Base)
{
    M.Box(TEXT("CP_PaintWhite"), FTransform(Base + Up(130.f)), FVector(170.f, 170.f, 130.f), Plain());
    M.Box(TEXT("CP_PaintWhite"), FTransform(Base + Up(268.f)), FVector(186.f, 186.f, 10.f), Plain());
    for (int32 S = 0; S < 4; ++S)
    {
        const FVector N = FRotator(0.f, S * 90.f, 0.f).Vector();
        HouseCampusFallKit::Card(M, TEXT("CP_Louvre"), Base + N * 171.f + Up(130.f), ReadRight(N), FVector::UpVector, 150.f, 190.f, N);
    }
    TArray<FVector2D> Prof = { { 120.f, 0.f }, { 120.f, 210.f }, { 132.f, 214.f }, { 132.f, 226.f }, { .1f, 226.f } };
    HouseCampusFallKit::Lathe(M, TEXT("CP_PaintWhite"), Base + Up(278.f), Prof, 8);
    for (int32 K = 0; K < 8; ++K)
    {
        const float A = (K + .5f) * PI / 4.f;
        const FVector N(FMath::Cos(A), FMath::Sin(A), 0.f);
        HouseCampusFallKit::Card(Glow, TEXT("CP_CupolaLight"), Base + Up(278.f + 110.f) + N * 112.f, ReadRight(N), FVector::UpVector, 64.f, 150.f, N);
    }
    M.Dome(TEXT("CP_CopperGreen"), Base + Up(504.f), FVector::UpVector, 134.f, 150.f, 16);
    M.Cylinder(TEXT("CP_Brass"), Base + Up(650.f), Base + Up(760.f), 3.f, 8, Plain());
    HouseCampusFallKit::Ball(M, TEXT("CP_Brass"), Base + Up(668.f), 12.f, 8);
    for (int32 K = 0; K < 4; ++K)   // N, E, S, W arms
    {
        const FVector Dd = FRotator(0.f, K * 90.f, 0.f).Vector();
        HouseCampusFallKit::Bar(M, TEXT("CP_Brass"), Base + Up(700.f), Base + Up(700.f) + Dd * 70.f, 2.f, 2.f, FVector::UpVector);
    }
    // The goose: a flat silhouette in profile on top, an arrow under it.
    const FVector V = Base + Up(770.f), X(1, 0, 0), Z(0, 0, 1);
    HouseCampusFallKit::Bar(M, TEXT("CP_Brass"), V - X * 80.f, V + X * 80.f, 3.f, 3.f, FVector(0, 1, 0));
    const TArray<FVector2D> Goose = { { -55.f, 8.f }, { -20.f, 30.f }, { 25.f, 32.f }, { 40.f, 40.f }, { 48.f, 70.f }, { 58.f, 78.f }, { 70.f, 74.f }, { 60.f, 66.f }, { 54.f, 44.f }, { 48.f, 22.f }, { 20.f, 10.f }, { -40.f, 4.f } };
    TArray<FVector> Rim;
    for (const FVector2D& Pt : Goose) Rim.Add(V + X * Pt.X + Z * Pt.Y);
    HouseCampusFallKit::Fan(M, TEXT("CP_Brass"), V + X * 10.f + Z * 20.f, Rim, FVector(0, 1, 0));
    HouseCampusFallKit::Fan(M, TEXT("CP_Brass"), V + X * 10.f + Z * 20.f, Rim, FVector(0, -1, 0));
}
/** Mercer's dome: a limestone drum with pilasters, the copper dome, its lantern. */
void LibraryDome(FHallMesh& M, const FVector& Base, float R)
{
    M.Cylinder(TEXT("EX_CampusLimestone"), Base, Base + Up(280.f), R, 32, FHallUV::World(120.f));
    for (int32 K = 0; K < 16; ++K)
    {
        const float A = K * PI / 8.f;
        const FVector N(FMath::Cos(A), FMath::Sin(A), 0.f);
        M.Box(TEXT("CP_Ashlar"), FTransform(FRotationMatrix::MakeFromX(N).ToQuat(), Base + N * (R + 8.f) + Up(140.f)), FVector(10.f, 22.f, 140.f), Plain());
        if (K % 2) HouseCampusFallKit::Card(M, TEXT("EX_Glass"), Base + N * (R + 1.f) + Up(150.f), ReadRight(N), FVector::UpVector, 70.f, 150.f, N);
    }
    M.Cylinder(TEXT("CP_Ashlar"), Base + Up(280.f), Base + Up(320.f), R + 30.f, 32, Plain());
    M.Dome(TEXT("CP_CopperGreen"), Base + Up(320.f), FVector::UpVector, R + 10.f, R * .8f, 24);
    M.Cylinder(TEXT("CP_PaintWhite"), Base + Up(320.f + R * .78f), Base + Up(320.f + R * .78f + 140.f), 70.f, 12, Plain());
    M.Dome(TEXT("CP_CopperGreen"), Base + Up(320.f + R * .78f + 140.f), FVector::UpVector, 80.f, 70.f, 12);
    M.Cylinder(TEXT("CP_Brass"), Base + Up(320.f + R * .78f + 205.f), Base + Up(320.f + R * .78f + 300.f), 3.f, 8, Plain());
}
void Chimney(FHallMesh& M, FName Body, const FVector& Base, float H)
{
    M.Box(Body, FTransform(Base + Up(H * .5f)), FVector(65.f, 45.f, H * .5f), FHallUV::World(100.f));
    M.Box(TEXT("CP_Ashlar"), FTransform(Base + Up(H + 6.f)), FVector(72.f, 52.f, 6.f), Plain());
    for (const float X : { -30.f, 30.f }) M.Cylinder(TEXT("CP_Terracotta"), Base + FVector(X, 0.f, 0.f) + Up(H + 12.f), Base + FVector(X, 0.f, 0.f) + Up(H + 52.f), 13.f, 10, Plain());
}
void Penthouse(FHallMesh& M, const FBld& B, const FVector& C, float W, float D, float H)
{
    M.BoxMinMax(B.Body, C - FVector(W * .5f, D * .5f, 0.f), C + FVector(W * .5f, D * .5f, H), FHallUV::World(100.f));
    M.BoxMinMax(TEXT("CP_Ashlar"), C - FVector(W * .5f + 10.f, D * .5f + 10.f, -H), C + FVector(W * .5f + 10.f, D * .5f + 10.f, H + 14.f), Plain());
    HouseCampusFallKit::Card(M, TEXT("CP_SteelDoor"), C + FVector(0.f, D * .5f + .6f, 105.f), ReadRight(FVector(0, 1, 0)), FVector::UpVector, 90.f, 210.f, FVector(0, 1, 0));
}
void TVAntenna(FHallMesh& M, const FVector& Base, float H, float Yaw)
{
    M.Cylinder(TEXT("CP_Aluminum"), Base, Base + Up(H), 2.f, 6, Plain());
    const FVector Boom = FRotator(0.f, Yaw, 0.f).Vector(), Cross = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
    HouseCampusFallKit::Bar(M, TEXT("CP_Aluminum"), Base + Up(H) - Boom * 90.f, Base + Up(H) + Boom * 70.f, 1.5f, 1.5f, FVector::UpVector);
    for (int32 K = 0; K < 7; ++K)
    {
        const float L = 70.f - K * 6.f;
        HouseCampusFallKit::Bar(M, TEXT("CP_Aluminum"), Base + Up(H) - Boom * (80.f - K * 22.f) - Cross * L * .5f, Base + Up(H) - Boom * (80.f - K * 22.f) + Cross * L * .5f, 1.f, 1.f, FVector::UpVector);
    }
    for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Bar(M, TEXT("CP_Aluminum"), Base + Up(H * .6f), Base + Cross * (S * H * .5f) + Up(2.f), .5f, .5f, FVector::UpVector);
}
void Observatory(FHallMesh& M, const FVector& Base)
{
    M.Cylinder(TEXT("EX_CampusWarmBrick"), Base, Base + Up(200.f), 280.f, 24, FHallUV::World(100.f));
    M.Cylinder(TEXT("CP_Ashlar"), Base + Up(200.f), Base + Up(214.f), 292.f, 24, Plain());
    M.Dome(TEXT("CP_Aluminum"), Base + Up(214.f), FVector::UpVector, 286.f, 270.f, 20);
    // The shutter's slit, closed, from the crown down to the south.
    for (int32 J = 0; J < 9; ++J)
    {
        const float A0 = J * PI / 18.f, A1 = (J + 1) * PI / 18.f;
        auto P = [&](float A, float S) { return Base + Up(214.f) + FVector(0.f, -FMath::Sin(A) * 288.f, FMath::Cos(A) * 272.f) + FVector(S * 38.f, 0.f, 0.f); };
        M.Quad(TEXT("CP_SteelDark"), P(A0, -1.f), P(A0, 1.f), P(A1, 1.f), P(A1, -1.f), FVector(0.f, -FMath::Sin((A0 + A1) * .5f), FMath::Cos((A0 + A1) * .5f)), Plain());
    }
}
void FumeStacks(FHallMesh& M, const FVector& Base, int32 Count)
{
    for (int32 K = 0; K < Count; ++K)
    {
        const FVector P = Base + FVector(K * 160.f, (K % 2) * 60.f, 0.f);
        M.Cylinder(TEXT("CP_Galv"), P, P + Up(420.f + (K % 3) * 60.f), 22.f, 12, Plain());
        M.Cylinder(TEXT("CP_SteelDark"), P + Up(410.f + (K % 3) * 60.f), P + Up(424.f + (K % 3) * 60.f), 34.f, 12, Plain());
        for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Bar(M, TEXT("CP_SteelDark"), P + Up(300.f), P + FVector(S * 120.f, 80.f, 2.f), .5f, .5f, FVector::UpVector);
    }
}
void Lantern(FHallMesh& M, FHallMesh& Glow, const FVector& C, float S, float H)
{
    // The Union's atrium lantern: a glazed box, lit, under a low glass hip.
    for (int32 K = 0; K < 4; ++K)
    {
        const FVector N = FRotator(0.f, K * 90.f, 0.f).Vector();
        HouseCampusFallKit::Card(Glow, TEXT("CP_LanternGlass"), C + N * S + Up(H * .5f), ReadRight(N), FVector::UpVector, S * 2.f, H, N);
        for (int32 J = -3; J <= 3; ++J) HouseCampusFallKit::Bar(M, TEXT("CP_SteelDark"), C + N * (S + 1.f) + ReadRight(N) * (J * S / 3.5f), C + N * (S + 1.f) + ReadRight(N) * (J * S / 3.5f) + Up(H), 6.f, 3.f, N);
    }
    M.BoxMinMax(TEXT("CP_SteelDark"), C - FVector(S + 10.f, S + 10.f, -H), C + FVector(S + 10.f, S + 10.f, H + 12.f), Plain());
    for (int32 K = 0; K < 4; ++K)
    {
        const FVector A = C + FRotator(0.f, K * 90.f + 45.f, 0.f).Vector() * (S * 1.41f) + Up(H + 12.f), Bv = C + FRotator(0.f, K * 90.f + 135.f, 0.f).Vector() * (S * 1.41f) + Up(H + 12.f);
        const FVector Top = C + Up(H + 12.f + S * .45f);
        const FVector N = FVector::CrossProduct(Bv - A, Top - A).GetSafeNormal();
        HouseCampusFallKit::Tri(Glow, TEXT("CP_LanternGlass"), A, Bv, Top, FVector::DotProduct(N, FVector::UpVector) > 0.f ? N : -N);
    }
}
void Vault(FHallMesh& M, const FBld& B, float Rise)
{
    // A segmental vault along Y in standing-seam metal; the ends' lunette windows.
    const float Span = B.W * .5f + 20.f, R = (Span * Span + Rise * Rise) / (2.f * Rise), Z0 = B.Roof + 40.f, Cz = Z0 + Rise - R;
    const float Half = FMath::Asin(Span / R);
    constexpr int32 N = 16;
    for (int32 K = 0; K < N; ++K)
    {
        const float A0 = -Half + 2.f * Half * K / N, A1 = -Half + 2.f * Half * (K + 1) / N;
        const FVector P0(R * FMath::Sin(A0), 0.f, Cz + R * FMath::Cos(A0)), P1(R * FMath::Sin(A1), 0.f, Cz + R * FMath::Cos(A1));
        const FVector Nrm = FVector(FMath::Sin((A0 + A1) * .5f), 0.f, FMath::Cos((A0 + A1) * .5f));
        const float HD = B.D * .5f + 30.f;
        M.Quad(TEXT("CP_StandingSeam"), P0 - FVector(0, HD, 0), P1 - FVector(0, HD, 0), P1 + FVector(0, HD, 0), P0 + FVector(0, HD, 0), Nrm, FHallUV::World(120.f));
    }
    for (const float S : { -1.f, 1.f })
    {
        TArray<FVector> Rim;
        for (int32 K = 0; K <= N; ++K) { const float A = -Half + 2.f * Half * K / N; Rim.Add(FVector(R * FMath::Sin(A), S * (B.D * .5f + 2.f), Cz + R * FMath::Cos(A))); }
        HouseCampusFallKit::Fan(M, B.Body, FVector(0.f, S * (B.D * .5f + 2.f), Z0), Rim, FVector(0.f, S, 0.f), false);
        // A thermal window in the lunette: a half-round of glass with two mullions.
        TArray<FVector> Win;
        for (int32 K = 0; K <= 12; ++K) { const float A = PI * K / 12.f; Win.Add(FVector(FMath::Cos(A) * Span * .55f, S * (B.D * .5f + 3.f), Z0 + 10.f + FMath::Sin(A) * Rise * .7f)); }
        HouseCampusFallKit::Fan(M, TEXT("EX_Glass"), FVector(0.f, S * (B.D * .5f + 3.f), Z0 + 10.f), Win, FVector(0.f, S, 0.f), false);
        for (const float X : { -Span * .18f, Span * .18f }) HouseCampusFallKit::Bar(M, TEXT("CP_SteelDark"), FVector(X, S * (B.D * .5f + 4.f), Z0 + 10.f), FVector(X, S * (B.D * .5f + 4.f), Z0 + 10.f + Rise * .66f), 8.f, 3.f, FVector(0, S, 0));
    }
}
void Sawtooth(FHallMesh& M, FHallMesh& Glow, const FBld& B, int32 Teeth, float Rise)
{
    // North lights: each tooth a glazed face toward +Y (the north here) and a roof sloping back.
    const float HW = B.W * .5f, Step = B.D / Teeth, Z0 = B.Roof + 40.f;
    for (int32 K = 0; K < Teeth; ++K)
    {
        const float Y1 = B.D * .5f - K * Step, Y0 = Y1 - Step;
        HouseCampusFallKit::Card(Glow, TEXT("CP_StudioGlass"), FVector(0.f, Y1, Z0 + Rise * .5f), ReadRight(FVector(0, 1, 0)), FVector::UpVector, B.W - 40.f, Rise - 20.f, FVector(0, 1, 0));
        for (float X = -HW + 60.f; X < HW; X += 120.f) HouseCampusFallKit::Bar(M, TEXT("CP_SteelDark"), FVector(X, Y1 + 2.f, Z0), FVector(X, Y1 + 2.f, Z0 + Rise), 5.f, 3.f, FVector(0, 1, 0));
        const FVector A(-HW - 20.f, Y1 + 10.f, Z0 + Rise), Bv(HW + 20.f, Y1 + 10.f, Z0 + Rise), C(HW + 20.f, Y0, Z0), Dv(-HW - 20.f, Y0, Z0);
        M.Quad(TEXT("CP_StandingSeam"), A, Bv, C, Dv, FVector(0.f, -Rise, Step).GetSafeNormal(), FHallUV::World(120.f));
        for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Tri(M, B.Body, FVector(S * (HW + 1.f), Y1, Z0 + Rise), FVector(S * (HW + 1.f), Y1, Z0), FVector(S * (HW + 1.f), Y0, Z0), FVector(S, 0.f, 0.f));
    }
}
void WaterTank(FHallMesh& M, const FVector& Base)
{
    for (int32 K = 0; K < 4; ++K)
    {
        const FVector Off = FRotator(0.f, 45.f + K * 90.f, 0.f).Vector() * 190.f;
        HouseCampusFallKit::Bar(M, TEXT("CP_SteelDark"), Base + Off, Base + Off * .8f + Up(420.f), 12.f, 12.f, FVector::UpVector);
        const FVector Off2 = FRotator(0.f, 45.f + (K + 1) * 90.f, 0.f).Vector() * 190.f;
        HouseCampusFallKit::Bar(M, TEXT("CP_SteelDark"), Base + Off, Base + Off2 * .9f + Up(380.f), 3.f, 3.f, FVector::UpVector);
        HouseCampusFallKit::Bar(M, TEXT("CP_SteelDark"), Base + Off2, Base + Off * .9f + Up(380.f), 3.f, 3.f, FVector::UpVector);
    }
    M.Cylinder(TEXT("CP_SteelDark"), Base + Up(420.f), Base + Up(432.f), 200.f, 20, Plain());
    M.Cylinder(TEXT("CP_TankWood"), Base + Up(432.f), Base + Up(752.f), 182.f, 24, FHallUV::World(100.f));
    for (int32 J = 0; J < 6; ++J) M.Cylinder(TEXT("CP_SteelDark"), Base + Up(450.f + J * 52.f), Base + Up(454.f + J * 52.f), 184.f, 24, Plain());
    HouseCampusFallKit::Lathe(M, TEXT("CP_TankWood"), Base + Up(752.f), { FVector2D(196.f, 0.f), FVector2D(.1f, 140.f) }, 24);
    M.Cylinder(TEXT("CP_Brass"), Base + Up(890.f), Base + Up(930.f), 4.f, 8, Plain());
}
void Smokestack(FHallMesh& M, const FVector& Base, float H)
{
    HouseCampusFallKit::Lathe(M, TEXT("CP_StackBrick"), Base, { FVector2D(150.f, 0.f), FVector2D(92.f, H), FVector2D(.1f, H) }, 20);
    for (const float F : { .32f, .64f, .93f })
    {
        const float R = FMath::Lerp(150.f, 92.f, F) + 6.f;
        M.Cylinder(TEXT("CP_Ashlar"), Base + Up(H * F), Base + Up(H * F + 40.f), R, 20, Plain());
    }
    M.Cylinder(TEXT("CP_SteelDark"), Base + Up(H - 4.f), Base + Up(H + 22.f), 104.f, 20, Plain());
    for (float Z = 200.f; Z < H - 40.f; Z += 36.f) HouseCampusFallKit::Bar(M, TEXT("CP_SteelDark"), Base + FVector(-20.f, -FMath::Lerp(150.f, 92.f, Z / H) - 6.f, Z), Base + FVector(20.f, -FMath::Lerp(150.f, 92.f, Z / H) - 6.f, Z), 2.f, 2.f, FVector::UpVector);
}

// ------------------------------------------------------------------ the walls
/** A fire escape on side S (2 -X, 3 +X) at window column Col: landings at every floor above the ground, the stairs between,
 *  railings, the drop ladder hung up at the first landing. */
void FireEscape(FHallMesh& M, const FBld& B, int32 Side, int32 Col)
{
    TArray<FVector2D> Cols;
    HouseCampusInteriors::WindowColumns(B.Seed, Side, Cols);
    if (!Cols.IsValidIndex(Col)) return;
    const float Sx = Side == 2 ? -1.f : 1.f, X = Sx * (B.W * .5f), U = (Cols[Col].X + Cols[Col].Y) * .5f;
    const int32 Floors = HouseCampusInteriors::Floors(B.Height);
    const FVector N(Sx, 0.f, 0.f);
    auto At = [&](float Y, float Out, float Z) { return FVector(X + Sx * Out, Y, Z); };
    for (int32 F = 1; F < Floors; ++F)
    {
        const float Z = HouseCampusInteriors::WindowSill(B.Height, F) - 12.f;
        // The landing: grating on a frame, its rail.
        M.BoxMinMax(TEXT("CP_Grating"), FVector(FMath::Min(X, X + Sx * 120.f), U - 160.f, Z - 3.f), FVector(FMath::Max(X, X + Sx * 120.f), U + 160.f, Z), Plain());
        for (const float Y : { U - 160.f, U + 160.f }) HouseCampusFallKit::Bar(M, TEXT("CP_FireEscape"), At(Y, 120.f, Z), At(Y, 120.f, Z + 100.f), 3.f, 3.f, N);
        HouseCampusFallKit::Bar(M, TEXT("CP_FireEscape"), At(U - 160.f, 120.f, Z + 100.f), At(U + 160.f, 120.f, Z + 100.f), 3.f, 3.f, FVector::UpVector);
        HouseCampusFallKit::Bar(M, TEXT("CP_FireEscape"), At(U - 160.f, 120.f, Z + 50.f), At(U + 160.f, 120.f, Z + 50.f), 2.f, 2.f, FVector::UpVector);
        for (const float Y : { U - 160.f, U + 160.f }) HouseCampusFallKit::Bar(M, TEXT("CP_FireEscape"), At(Y, 0.f, Z + 100.f), At(Y, 120.f, Z + 100.f), 3.f, 3.f, FVector::UpVector);
        for (const float Y : { U - 150.f, U + 150.f }) HouseCampusFallKit::Bar(M, TEXT("CP_FireEscape"), At(Y, 4.f, Z - 2.f), At(Y, 4.f, Z - 70.f), 4.f, 4.f, N);   // brackets
        // The stair down to the landing below (or the drop ladder at the first).
        const bool bRight = F % 2 == 1;
        if (F > 1)
        {
            const float Zb = HouseCampusInteriors::WindowSill(B.Height, F - 1) - 12.f;
            const float Y0 = bRight ? U + 150.f : U - 150.f, Y1 = bRight ? U - 60.f : U + 60.f;
            for (const float Out : { 40.f, 110.f }) HouseCampusFallKit::Bar(M, TEXT("CP_FireEscape"), At(Y0, Out, Z), At(Y1, Out, Zb), 3.f, 14.f, N);
            for (int32 K = 1; K < 14; ++K)
            {
                const float T = K / 14.f;
                const FVector A = FMath::Lerp(At(Y0, 42.f, Z), At(Y1, 42.f, Zb), T), Bv = FMath::Lerp(At(Y0, 108.f, Z), At(Y1, 108.f, Zb), T);
                HouseCampusFallKit::Bar(M, TEXT("CP_Grating"), A, Bv, 18.f, 2.f, FVector::UpVector);
            }
            HouseCampusFallKit::Bar(M, TEXT("CP_FireEscape"), At(Y0, 112.f, Z + 90.f), At(Y1, 112.f, Zb + 90.f), 2.5f, 2.5f, N);
        }
        else
        {
            for (const float Y : { U + 100.f, U + 140.f }) HouseCampusFallKit::Bar(M, TEXT("CP_FireEscape"), At(Y, 110.f, Z - 40.f), At(Y, 110.f, Z - 260.f), 2.5f, 2.5f, N);
            for (float Z2 = Z - 60.f; Z2 > Z - 260.f; Z2 -= 30.f) HouseCampusFallKit::Bar(M, TEXT("CP_FireEscape"), At(U + 100.f, 110.f, Z2), At(U + 140.f, 110.f, Z2), 2.f, 2.f, FVector::UpVector);
        }
    }
}
/** A spread of Boston ivy, turned for autumn, on a wall facing N from the ground up: a ragged patch. */
void Ivy(FHallMesh& M, FRandomStream& R, const FVector& Base, const FVector& Along, const FVector& N, float W, float H)
{
    TArray<FVector> Rim;
    const FVector C = Base + Up(H * .45f) + N * 1.2f;
    for (int32 K = 0; K < 28; ++K)
    {
        const float A = 2.f * PI * K / 28.f;
        float X = FMath::Cos(A) * W * .5f, Y = FMath::Sin(A) * H * .55f;
        X *= .75f + .35f * R.FRand(); Y *= .7f + .4f * R.FRand();
        Y = FMath::Max(Y, -H * .45f);   // it starts at the ground
        Rim.Add(C + Along * X + Up(Y));
    }
    HouseCampusFallKit::Fan(M, TEXT("CP_Ivy"), C, Rim, N);
}
void Blinds(FHallMesh& M, FRandomStream& R, const FVector& Bottom, const FVector& Along, const FVector& N, bool bDorm)
{
    // Bottom: the window's bottom middle, 5 cm out from the wall (as the exterior places its windows); its glass is 3 cm
    // further out. A treatment just in front of the glass, behind the mullion.
    const FVector Glass = Bottom + N * 3.5f, Rt = ReadRight(N);
    const float Roll = R.FRand();
    if (Roll < .3f)
    {
        const float Down = R.FRandRange(50.f, 190.f);
        HouseCampusFallKit::Card(M, R.FRand() < .7f ? TEXT("CP_BlindsWhite") : TEXT("CP_BlindsTan"), Glass + Up(212.f - 8.f - Down * .5f), Rt, FVector::UpVector, 158.f, Down, N);
    }
    else if (Roll < .45f)
    {
        static const FName Curtains[] = { TEXT("CP_CurtainRed"), TEXT("CP_CurtainBlue"), TEXT("CP_CurtainGold"), TEXT("CP_CurtainFloral") };
        const FName C = Curtains[R.RandRange(0, 3)];
        for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Card(M, C, Glass + Rt * (S * 58.f) + Up(106.f), Rt, FVector::UpVector, 44.f, 196.f, N);
    }
    else if (Roll < .5f)
    {
        // A sash raised: the dark gap at the bottom, the lower sash's rail up where it stopped.
        const float Open = R.FRandRange(25.f, 60.f);
        HouseCampusFallKit::Card(M, TEXT("CP_OpenGap"), Glass + N * .2f + Up(8.f + Open * .5f), Rt, FVector::UpVector, 158.f, Open, N);
        HouseCampusFallKit::Bar(M, TEXT("EX_Trim"), Glass + N * 3.f - Rt * 80.f + Up(8.f + Open), Glass + N * 3.f + Rt * 80.f + Up(8.f + Open), 5.f, 6.f, N);
    }
    if (bDorm)
    {
        const float Roll2 = R.FRand();
        if (Roll2 < .12f)
        {
            // A window air conditioner on the sill, sticking out.
            const FQuat Q = FRotationMatrix::MakeFromXZ(N, FVector::UpVector).ToQuat();
            M.Box(TEXT("CP_ACUnit"), FTransform(Q, Bottom + N * 22.f + Up(34.f)), FVector(26.f, 32.f, 22.f), Plain());
            HouseCampusFallKit::Card(M, TEXT("CP_ACGrille"), Bottom + N * 48.2f + Up(34.f), Rt, FVector::UpVector, 58.f, 38.f, N);
            M.Box(TEXT("CP_SteelDark"), FTransform(Q, Bottom + N * 30.f + Up(10.f)), FVector(20.f, 30.f, 2.f), Plain());
        }
        else if (Roll2 < .2f)
        {
            // A box fan in the lower sash.
            HouseCampusFallKit::Card(M, TEXT("CP_BoxFan"), Glass + N * .4f + Up(40.f), Rt, FVector::UpVector, 56.f, 56.f, N);
        }
    }
}
void Meters(FHallMesh& M, const FVector& At, const FVector& Along, const FVector& N)
{
    // The electric meter (a glass dome on its base) over the gas meter and its pipes.
    const FQuat Q = FRotationMatrix::MakeFromXZ(N, FVector::UpVector).ToQuat();
    M.Box(TEXT("CP_JunctionBox"), FTransform(Q, At + N * 6.f + Up(165.f)), FVector(6.f, 18.f, 24.f), Plain());
    M.Cylinder(TEXT("CP_MeterGlass"), At + N * 12.f + Up(168.f), At + N * 22.f + Up(168.f), 11.f, 12, Plain());
    HouseCampusFallKit::Bar(M, TEXT("CP_SteelDark"), At + N * 4.f + Up(190.f), At + N * 4.f + Up(420.f), 5.f, 5.f, N);
    M.Box(TEXT("CP_GasMeter"), FTransform(Q, At + N * 16.f + Along * 70.f + Up(70.f)), FVector(14.f, 20.f, 22.f), Plain());
    HouseCampusFallKit::Bar(M, TEXT("CP_GasPipe"), At + N * 14.f + Along * 70.f + Up(-2.f), At + N * 14.f + Along * 70.f + Up(48.f), 5.f, 5.f, N);
    HouseCampusFallKit::Bar(M, TEXT("CP_GasPipe"), At + N * 8.f + Along * 70.f + Up(92.f), At + N * 8.f + Along * 70.f + Up(120.f), 4.f, 4.f, N);
}
void WindowWell(FHallMesh& M, const FVector& Base, const FVector& Along, const FVector& N)
{
    // A corrugated half-round well in front of a basement window, its grate over it, the window down in it.
    TArray<FVector2D> Arc;
    for (int32 K = 0; K <= 10; ++K) { const float A = PI * K / 10.f; Arc.Add(FVector2D(FMath::Cos(A) * 60.f, FMath::Sin(A) * 48.f)); }
    for (int32 K = 0; K < 10; ++K)
    {
        const FVector A = Base + Along * Arc[K].X + N * Arc[K].Y, Bv = Base + Along * Arc[K + 1].X + N * Arc[K + 1].Y;
        const FVector Out = (Along * (Arc[K].X + Arc[K + 1].X) + N * (Arc[K].Y + Arc[K + 1].Y)).GetSafeNormal();
        M.Quad(TEXT("CP_Galv"), A + Up(-6.f), Bv + Up(-6.f), Bv + Up(22.f), A + Up(22.f), Out, Plain());
    }
    HouseCampusFallKit::Card(M, TEXT("CP_WellGrate"), Base + N * 24.f + Up(22.5f), Along, N * -1.f, 120.f, 48.f, FVector::UpVector);
    HouseCampusFallKit::Card(M, TEXT("CP_WindowDark"), Base + N * 12.6f + Up(36.f), ReadRight(N), FVector::UpVector, 90.f, 40.f, N);
}

// ------------------------------------------------------------------ materials
void PaintSlate(FHallImage& Img, const FLinearColor& Base, int32 Seed)
{
    constexpr int32 N = 256;
    Img.Init(N, N, Base * .6f);
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const int32 Row = Y / 21;
            const float Shift = (Row & 1) ? 16.f : 0.f;
            const int32 Id = Row * 13 + int32((X + Shift) / 32.f);
            const bool bGap = FMath::Fmod(X + Shift, 32.f) < 1.5f || (Y % 21) < 1;
            const float V = .82f + .25f * HallNoise(Id * 1.3f, 3.1f, Seed) + .06f * HallNoise(X * .5f, Y * .5f, Seed + 1);
            Img.At(X, Y) = bGap ? Base * .45f : Base * V * (1.f - .25f * ((Y % 21) / 21.f));
        }
}
void PaintIvy(FHallImage& Img, int32 Seed)
{
    // Boston ivy in October: overlapping three-lobed leaves in crimson, scarlet and a few late greens on dark stems.
    constexpr int32 N = 256;
    Img.Init(N, N, FLinearColor(.06f, .03f, .02f));
    FRandomStream R(Seed);
    static const FLinearColor Leaves[] = { FLinearColor(.42f, .04f, .03f), FLinearColor(.55f, .1f, .03f), FLinearColor(.3f, .03f, .04f), FLinearColor(.6f, .22f, .04f), FLinearColor(.12f, .2f, .05f) };
    for (int32 K = 0; K < 900; ++K)
    {
        const FVector2D C(R.FRandRange(0.f, N), R.FRandRange(0.f, N));
        const FLinearColor Col = Leaves[R.RandRange(0, 4)] * R.FRandRange(.7f, 1.15f);
        const float S = R.FRandRange(5.f, 10.f), Rot = R.FRandRange(0.f, 360.f);
        for (int32 L = -1; L <= 1; ++L) Img.Ellipse(C + FVector2D(FMath::Cos(FMath::DegreesToRadians(Rot + L * 50.f)), FMath::Sin(FMath::DegreesToRadians(Rot + L * 50.f))) * S * .5f, FVector2D(S * .6f, S * .4f), Rot + L * 50.f, Col);
    }
}
void PaintBlinds(FHallImage& Img, const FLinearColor& C)
{
    constexpr int32 N = 64;
    Img.Init(N, N, C);
    for (int32 Y = 0; Y < N; ++Y) for (int32 X = 0; X < N; ++X) Img.At(X, Y) = C * ((Y % 4) == 0 ? .62f : .92f + .08f * ((Y % 4) / 4.f));
}
void PaintCurtain(FHallImage& Img, const FLinearColor& C, bool bFloral, int32 Seed)
{
    constexpr int32 N = 64;
    Img.Init(N, N, C);
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const float Fold = .75f + .25f * FMath::Sin(X * PI / 8.f);
            Img.At(X, Y) = C * Fold;
            if (bFloral && HallNoise(X * .4f, Y * .4f, Seed) > .55f) Img.At(X, Y) = FLinearColor(.8f, .7f, .4f) * Fold;
        }
}
void PaintGrille(FHallImage& Img, const FLinearColor& Base, bool bRound)
{
    constexpr int32 N = 64;
    Img.Init(N, N, Base);
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            if (bRound)
            {
                const float D = FVector2D::Distance(FVector2D(X, Y), FVector2D(32, 32));
                Img.At(X, Y) = D > 29.f ? Base : (int32(D) % 4 < 1 || (X % 8) == 0) ? Base * .4f : FLinearColor(.05f, .05f, .06f);
            }
            else Img.At(X, Y) = (Y % 5 < 2) ? Base * .45f : Base;
        }
}
void PaintLouvre(FHallImage& Img)
{
    constexpr int32 W = 64, H = 96;
    Img.Init(W, H, FLinearColor(.8f, .8f, .77f));
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float Dx = (X - 32.f) / 24.f, Dy = (Y - 40.f) / 24.f;
            const bool bOpening = (FMath::Abs(Dx) < 1.f && Y > 40 && Y < 88) || (Dx * Dx + Dy * Dy < 1.f && Y <= 40);
            if (bOpening) Img.At(X, Y) = (Y % 6 < 2) ? FLinearColor(.55f, .55f, .52f) : FLinearColor(.05f, .05f, .05f);
        }
}
void Paint(UHouseBlackHallSubsystem* Look, const TCHAR* Key, const FHallImage& Img, float Rough, const FLinearColor& Emission = FLinearColor::Black, bool bWrap = true)
{
    UTexture2D* T = HallMakeTexture(Img, true, false, *FString::Printf(TEXT("T_%s"), Key), bWrap);
    if (T) Look->Textures.Add(FName(*FString::Printf(TEXT("T_%s"), Key)), T);
    HallMakeTextured(Look, FName(Key), T, nullptr, Rough, FLinearColor::White, 0.f, 0.f, Emission);
}
}

void HouseCampusPolish::BuildingsLook(UHouseBlackHallSubsystem* Look)
{
    namespace BI = HouseCampusPolishBuildingsImpl;
    FHallImage Img;
    BI::PaintSlate(Img, FLinearColor(.12f, .13f, .15f), 71); BI::Paint(Look, TEXT("CP_Slate"), Img, .55f);
    BI::PaintSlate(Img, FLinearColor(.16f, .12f, .1f), 72); BI::Paint(Look, TEXT("CP_Shingle"), Img, .85f);
    BI::PaintIvy(Img, 73); BI::Paint(Look, TEXT("CP_Ivy"), Img, .75f);
    BI::PaintBlinds(Img, FLinearColor(.75f, .74f, .7f)); BI::Paint(Look, TEXT("CP_BlindsWhite"), Img, .6f, FLinearColor(.02f, .02f, .018f));
    BI::PaintBlinds(Img, FLinearColor(.6f, .5f, .36f)); BI::Paint(Look, TEXT("CP_BlindsTan"), Img, .6f, FLinearColor(.015f, .012f, .008f));
    BI::PaintCurtain(Img, FLinearColor(.4f, .06f, .05f), false, 1); BI::Paint(Look, TEXT("CP_CurtainRed"), Img, .9f, FLinearColor(.01f, .002f, .001f));
    BI::PaintCurtain(Img, FLinearColor(.1f, .16f, .32f), false, 2); BI::Paint(Look, TEXT("CP_CurtainBlue"), Img, .9f);
    BI::PaintCurtain(Img, FLinearColor(.6f, .45f, .14f), false, 3); BI::Paint(Look, TEXT("CP_CurtainGold"), Img, .9f, FLinearColor(.012f, .009f, .003f));
    BI::PaintCurtain(Img, FLinearColor(.35f, .4f, .3f), true, 4); BI::Paint(Look, TEXT("CP_CurtainFloral"), Img, .9f);
    BI::PaintGrille(Img, FLinearColor(.55f, .53f, .48f), false); BI::Paint(Look, TEXT("CP_ACGrille"), Img, .6f);
    BI::PaintGrille(Img, FLinearColor(.75f, .74f, .7f), true); BI::Paint(Look, TEXT("CP_BoxFan"), Img, .6f);
    BI::PaintGrille(Img, FLinearColor(.2f, .2f, .2f), false); BI::Paint(Look, TEXT("CP_WellGrate"), Img, .6f);
    BI::PaintLouvre(Img); BI::Paint(Look, TEXT("CP_Louvre"), Img, .6f, FLinearColor::Black, false);
    struct FSpec { const TCHAR* Key; FLinearColor Tint; float Rough, Metal; FLinearColor Emission; };
    const FSpec Plains[] = {
        { TEXT("CP_CopperGreen"), FLinearColor(.18f, .36f, .3f), .55f, .2f, FLinearColor::Black },
        { TEXT("CP_PaintWhite"), FLinearColor(.75f, .74f, .7f), .6f, 0.f, FLinearColor::Black },
        { TEXT("CP_Brass"), FLinearColor(.55f, .4f, .14f), .35f, .9f, FLinearColor::Black },
        { TEXT("CP_CupolaLight"), FLinearColor(.9f, .75f, .45f), .5f, 0.f, FLinearColor(.5f, .38f, .18f) },
        { TEXT("CP_Terracotta"), FLinearColor(.45f, .18f, .1f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CP_SteelDoor"), FLinearColor(.25f, .26f, .25f), .5f, .4f, FLinearColor::Black },
        { TEXT("CP_StandingSeam"), FLinearColor(.24f, .3f, .28f), .4f, .6f, FLinearColor::Black },
        { TEXT("CP_TankWood"), FLinearColor(.28f, .2f, .13f), .85f, 0.f, FLinearColor::Black },
        { TEXT("CP_StackBrick"), FLinearColor(.3f, .12f, .08f), .85f, 0.f, FLinearColor::Black },
        { TEXT("CP_LanternGlass"), FLinearColor(.8f, .7f, .5f), .2f, 0.f, FLinearColor(.45f, .34f, .18f) },
        { TEXT("CP_StudioGlass"), FLinearColor(.5f, .52f, .55f), .15f, 0.f, FLinearColor(.08f, .08f, .09f) },
        { TEXT("CP_FireEscape"), FLinearColor(.05f, .05f, .05f), .6f, .5f, FLinearColor::Black },
        { TEXT("CP_OpenGap"), FLinearColor(.01f, .01f, .01f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CP_ACUnit"), FLinearColor(.55f, .52f, .45f), .5f, .3f, FLinearColor::Black },
        { TEXT("CP_MeterGlass"), FLinearColor(.5f, .55f, .55f), .1f, 0.f, FLinearColor::Black },
        { TEXT("CP_GasMeter"), FLinearColor(.45f, .45f, .42f), .5f, .4f, FLinearColor::Black },
        { TEXT("CP_GasPipe"), FLinearColor(.55f, .45f, .06f), .5f, .3f, FLinearColor::Black },
        { TEXT("CP_LampGlow"), FLinearColor(.9f, .7f, .4f), .4f, 0.f, FLinearColor(3.f, 2.1f, 1.f) },
    };
    for (const FSpec& S : Plains) HallMakePlain(Look, S.Key, S.Tint, S.Rough, S.Metal, S.Emission);
}

void HouseCampusPolish::Buildings(FParts& P)
{
    namespace BI = HouseCampusPolishBuildingsImpl;
    for (int32 Seed = 0; Seed < HouseCampusLayout::Blocks().Num(); ++Seed)
    {
        const BI::FBld B(Seed);
        const FString Id = B.Id();
        FHallMesh M, Glow, Dm, S;   // far (roofs, walls), glow, detail (windows' things), solid (what stands on the ground)
        FRandomStream R(4100 + Seed * 31);
        TArray<HouseCampusInteriors::FOpening> Doors;
        HouseCampusInteriors::DoorOpenings(Seed, Doors);
        auto DoorNear = [&Doors](int32 Side, float U0, float U1) { for (const HouseCampusInteriors::FOpening& D : Doors) if (D.Side == Side && D.U0 < U1 + 60.f && D.U1 > U0 - 60.f) return true; return false; };
        // The exterior's stand-in side entrances (a face the plan's walks reach with no real door behind it): glass doors and
        // a canopy 8 m wide at the middle of that face, which the walls' dressing must keep off too.
        TArray<FVector2D> StandIns;
        for (int32 F = 1; B.B->Faces[F]; ++F)
            if (!HouseCampusInteriors::RealDoorOn(Seed, B.B->Faces[F]))
            {
                const FVector Nf = HouseCampusLayout::FaceNormal(B.B->Faces[F]);
                const FBox2D Rc = B.B->Rect();
                StandIns.Add(Rc.GetCenter() + FVector2D(Nf.X * Rc.GetExtent().X, Nf.Y * Rc.GetExtent().Y));
            }
        auto StandInNear = [&](int32 Side, float U, float Reach)
        {
            const float Plane = Side == 0 ? B.D * .5f : Side == 1 ? -B.D * .5f : Side == 2 ? -B.W * .5f : B.W * .5f;
            const FVector2D At(B.T.TransformPosition(Side < 2 ? FVector(U, Plane, 0.f) : FVector(Plane, U, 0.f)));
            for (const FVector2D& C : StandIns) if (FVector2D::Distance(At, C) < 420.f + Reach) return true;
            return false;
        };
        // On sloping ground the exterior carries the shell down below the floor (HouseCampus::Foundation), and what stands
        // at a wall's foot here goes down to the ground with it. Where the relief round the building is flat (today) bSlope
        // is false and nothing below differs.
        HouseCampus::FFoundation Fd;
        const bool bSlope = HouseCampus::Foundation(Seed, Fd);
        // The roof and what stands on it.
        if (Id == TEXT("LIB")) BI::LibraryDome(M, FVector(0.f, 0.f, B.Roof + 70.f), FMath::Min(B.W, B.D) * .17f);
        else if (Id == TEXT("MAIN")) { BI::Mansard(M, B); BI::Cupola(M, Glow, FVector(0.f, 0.f, B.Roof + 460.f)); BI::Chimney(M, B.Body, FVector(-B.W * .32f, -B.D * .2f, B.Roof + 380.f), 260.f); }
        else if (Id == TEXT("HAR")) { BI::Gable(M, B, B.D * .18f, TEXT("CP_Slate"), false); for (const float X : { -B.W * .3f, 0.f, B.W * .3f }) BI::Chimney(M, B.Body, FVector(X, -B.D * .12f, B.Roof + 70.f), B.D * .18f + 140.f); }
        else if (Id == TEXT("COM")) { BI::Hip(M, B, 420.f, TEXT("CP_StandingSeam")); for (const float X : { -B.W * .2f, B.W * .15f }) BI::FumeStacks(M, FVector(X, -B.D * .25f, B.Roof + 60.f), 1); }
        else if (Id == TEXT("ASH")) { BI::Penthouse(M, B, FVector(B.W * .2f, -B.D * .1f, B.Roof), 620.f, 480.f, 320.f); for (int32 K = 0; K < 7; ++K) BI::TVAntenna(M, FVector(R.FRandRange(-B.W * .4f, B.W * .4f), R.FRandRange(-B.D * .35f, B.D * .35f), B.Roof), R.FRandRange(220.f, 380.f), R.FRandRange(0.f, 180.f)); }
        else if (Id == TEXT("SCI")) { BI::Observatory(M, FVector(-B.W * .25f, -B.D * .05f, B.Roof)); BI::FumeStacks(M, FVector(B.W * .1f, -B.D * .3f, B.Roof), 5); }
        else if (Id == TEXT("HUM")) BI::Gable(M, B, B.D * .42f, TEXT("CP_Slate"), true);
        else if (Id == TEXT("UNI")) BI::Lantern(M, Glow, FVector(-82.5f, -82.5f, B.Roof + 40.f), 300.f, 240.f);
        else if (Id == TEXT("KER")) { BI::Penthouse(M, B, FVector(-B.W * .1f, 0.f, B.Roof), 800.f, 500.f, 360.f); BI::TVAntenna(M, FVector(B.W * .3f, B.D * .3f, B.Roof), 600.f, 30.f); }
        else if (Id == TEXT("FLD")) BI::Vault(M, B, 560.f);
        else if (Id == TEXT("ART")) BI::Sawtooth(M, Glow, B, 4, 260.f);
        else if (Id == TEXT("MUS"))
        {
            BI::Gable(M, B, B.D * .3f, TEXT("CP_Shingle"), false);
            // The round window in each gable.
            for (const float Sg : { -1.f, 1.f })
            {
                const FVector C(Sg * (B.W * .5f + 4.f), 0.f, B.Roof + 70.f + B.D * .3f * .45f);
                TArray<FVector> Rim;
                for (int32 K = 0; K < 24; ++K) { const float A = 2.f * PI * K / 24.f; Rim.Add(C + FVector(0.f, FMath::Cos(A) * 130.f, FMath::Sin(A) * 130.f)); }
                HouseCampusFallKit::Fan(Glow, TEXT("CP_CupolaLight"), C, Rim, FVector(Sg, 0.f, 0.f));
                for (int32 K = 0; K < 24; ++K) HouseCampusFallKit::Bar(M, TEXT("CP_Ashlar"), Rim[K] + FVector(Sg * 2.f, 0, 0), Rim[(K + 1) % 24] + FVector(Sg * 2.f, 0, 0), 16.f, 6.f, FVector(Sg, 0, 0));
                for (int32 K = 0; K < 4; ++K) HouseCampusFallKit::Bar(M, TEXT("CP_SteelDark"), C + FVector(Sg * 3.f, 0, 0), Rim[K * 6] + FVector(Sg * 3.f, 0, 0), 4.f, 3.f, FVector(Sg, 0, 0));
            }
        }
        else if (Id == TEXT("ENG")) { BI::WaterTank(M, FVector(B.W * .22f, -B.D * .15f, B.Roof)); BI::Penthouse(M, B, FVector(-B.W * .25f, B.D * .1f, B.Roof), 500.f, 400.f, 300.f); }
        else if (Id == TEXT("PLT"))
        {
            BI::Smokestack(M, FVector(B.W * .3f, -B.D * .5f - 260.f, BI::G), 3600.f);
            // Its solid base: down to 30 cm under the lowest ground round it where that lies below the floor.
            float StackFoot = BI::G - 5.f;
            if (bSlope)
            {
                const FVector2D StackAt(B.W * .3f, -B.D * .5f - 260.f);
                float StackLow = FLT_MAX;
                for (const FVector2D& Off : { FVector2D(0.f, 0.f), FVector2D(150.f, 0.f), FVector2D(-150.f, 0.f), FVector2D(0.f, 150.f), FVector2D(0.f, -150.f) })
                    StackLow = FMath::Min(StackLow, HouseCampus::FoundationGround(Seed, StackAt + Off));
                if (StackLow < BI::G - 5.f) StackFoot = StackLow - 30.f;
            }
            S.Cylinder(TEXT("CP_StackBrick"), FVector(B.W * .3f, -B.D * .5f - 260.f, StackFoot), FVector(B.W * .3f, -B.D * .5f - 260.f, BI::G + 300.f), 148.f, 20, FHallUV::World(100.f));
            for (int32 K = 0; K < 3; ++K) P.Puffs.Add({ B.T.TransformPosition(FVector(B.W * .3f, -B.D * .5f - 260.f, BI::G + 3700.f + K * 160.f)), 320.f + K * 180.f, FLinearColor(.5f, .5f, .5f, .12f - K * .03f), 1 });
            BI::Hip(M, B, 450.f, TEXT("CP_StandingSeam"));
        }
        else if (Id == TEXT("ALU")) { BI::Hip(M, B, 520.f, TEXT("CP_Slate")); BI::Chimney(M, B.Body, FVector(B.W * .25f, 0.f, B.Roof + 70.f), 620.f); }
        else BI::Penthouse(M, B, FVector(0.f, -B.D * .2f, B.Roof), 500.f, 400.f, 280.f);   // the health center
        // The walls: a fire escape on the old dorms and halls, ivy on the old stone and brick, window wells, the meters.
        if (Id == TEXT("HAR") || Id == TEXT("ASH") || Id == TEXT("MAIN") || Id == TEXT("HUM"))
        {
            TArray<FVector2D> Cols;
            const int32 Side = Id == TEXT("ASH") ? 3 : 2;
            HouseCampusInteriors::WindowColumns(Seed, Side, Cols);
            // The middle column, or the nearest one clear of a stand-in entrance.
            for (int32 K = 0; K < Cols.Num(); ++K)
            {
                const int32 Col = Cols.Num() / 2 + ((K & 1) ? -(K + 1) / 2 : K / 2);
                if (!Cols.IsValidIndex(Col) || StandInNear(Side, (Cols[Col].X + Cols[Col].Y) * .5f, 200.f)) continue;
                BI::FireEscape(M, B, Side, Col);
                break;
            }
        }
        if (Id == TEXT("LIB") || Id == TEXT("MAIN") || Id == TEXT("HUM") || Id == TEXT("ALU") || Id == TEXT("HAR"))
            for (int32 K = 0; K < 4; ++K)
            {
                const int32 Side = R.RandRange(1, 3);
                const FVector N = Side == 1 ? FVector(0, -1, 0) : Side == 2 ? FVector(-1, 0, 0) : FVector(1, 0, 0);
                const FVector Along = BI::ReadRight(N);
                const float Len = Side == 1 ? B.W : B.D;
                const float U = R.FRandRange(-Len * .4f, Len * .4f), Wd = R.FRandRange(300.f, 700.f);
                const FVector Base = N * ((Side == 1 ? B.D : B.W) * .5f + 12.f) + Along * U + BI::Up(BI::G);
                // Side's U runs along +X or +Y; Along may run the other way.
                const float Uc = FVector::DotProduct(Base, Side < 2 ? FVector(1, 0, 0) : FVector(0, 1, 0));
                if (DoorNear(Side, Uc - Wd * .5f, Uc + Wd * .5f) || StandInNear(Side, Uc, Wd * .5f)) continue;
                const float IvyTall = R.FRandRange(400.f, B.Height * .7f);
                if (!bSlope) { BI::Ivy(M, R, Base, Along, N, Wd, IvyTall); continue; }
                // On sloping ground it climbs from the lowest ground at its foot. Over the basement's service door it is
                // still drawn (so the stream's draws stay the same) but thrown away.
                FVector IvyFoot = Base;
                float IvyH = IvyTall;
                const float IvyLow = BI::FootGround(B, Side, Uc - Wd * .5f, Uc + Wd * .5f, 12.f, 60.f, false);
                if (IvyLow < BI::G - 5.f) { IvyFoot.Z = IvyLow; IvyH += BI::G - IvyLow; }
                FHallMesh OverDoor;
                BI::Ivy(Fd.DoorSide == Side && FMath::Abs(Fd.DoorU - Uc) < Wd * .5f + 100.f ? OverDoor : M, R, IvyFoot, Along, N, Wd, IvyH);
            }
        if (!DoorNear(1, B.W * .5f - 220.f, B.W * .5f - 20.f) && !StandInNear(1, B.W * .5f - 120.f, 100.f))
        {
            // On sloping ground the meters go down to the ground at the gas pipe (on the plinth or basement face, a little
            // nearer the corner, clear of the basement's last window).
            FVector MeterAt(B.W * .5f - 120.f, -B.D * .5f - 12.f, BI::G);
            if (bSlope)
            {
                const float MeterLow = BI::FootGround(B, 1, B.W * .5f - 180.f, B.W * .5f - 160.f, 10.f, 20.f, false);
                if (MeterLow < BI::G - 5.f) MeterAt = FVector(B.W * .5f - 100.f, -B.D * .5f - 12.f, MeterLow);
            }
            BI::Meters(Dm, MeterAt, FVector(-1, 0, 0), FVector(0, -1, 0));
        }
        // Windows on the closed floors: blinds, curtains, a few sashes up, the dorms' air conditioners and fans.
        const int32 Floors = HouseCampusInteriors::Floors(B.Height), Open = HouseCampusInteriors::OpenFloors(Seed);
        const bool bDorm = HouseCampusLayout::IsDorm(Seed);
        if (Open > 0)
            for (int32 Side = 0; Side < 4; ++Side)
            {
                const FVector N = Side == 0 ? FVector(0, 1, 0) : Side == 1 ? FVector(0, -1, 0) : Side == 2 ? FVector(-1, 0, 0) : FVector(1, 0, 0);
                const FVector Along = Side < 2 ? FVector(1, 0, 0) : FVector(0, 1, 0);
                const float Plane = Side == 0 ? B.D * .5f : Side == 1 ? -B.D * .5f : Side == 2 ? -B.W * .5f : B.W * .5f;
                TArray<FVector2D> Cols;
                HouseCampusInteriors::WindowColumns(Seed, Side, Cols);
                for (int32 F = Open; F < Floors; ++F)
                    for (int32 I = 0; I < Cols.Num(); ++I)
                    {
                        const float U = (Cols[I].X + Cols[I].Y) * .5f, Z = HouseCampusInteriors::WindowSill(B.Height, F);
                        const FVector Bottom = (Side < 2 ? FVector(U, Plane, 0.f) : FVector(Plane, U, 0.f)) + N * 5.f + BI::Up(Z);
                        BI::Blinds(Dm, R, Bottom, Along, N, bDorm);
                    }
                // Window wells in front of every other ground-floor window of the old buildings' back and ends.
                if (Side > 0 && (bDorm || Id == TEXT("MAIN") || Id == TEXT("HUM")))
                    for (int32 I = 0; I < Cols.Num(); I += 2)
                    {
                        const float U = (Cols[I].X + Cols[I].Y) * .5f;
                        if (DoorNear(Side, Cols[I].X, Cols[I].Y) || HouseCampusInteriors::WindowReplaced(Seed, Side, 0, Cols[I].X, Cols[I].Y) || StandInNear(Side, U, 100.f)) continue;
                        // On sloping ground a well only where the ground there is still the floor's (lower, the exterior's
                        // basement storey has its own windows; higher, the well would be buried).
                        if (bSlope && (BI::FootGround(B, Side, Cols[I].X - 30.f, Cols[I].Y + 30.f, 12.f, 60.f, false) < BI::G - 5.f || BI::FootGround(B, Side, Cols[I].X - 30.f, Cols[I].Y + 30.f, 12.f, 60.f, true) > BI::G + 5.f)) continue;
                        BI::WindowWell(Dm, (Side < 2 ? FVector(U, Plane, 0.f) : FVector(Plane, U, 0.f)) + N * 12.f + BI::Up(BI::G), Along, N);
                    }
            }
        // Lanterns beside the main doors; a pediment over the classical ones.
        for (const HouseCampusInteriors::FOpening& D : Doors)
        {
            if (D.Side != 0) continue;
            const float Y = B.D * .5f + 55.f;
            for (const float U : { D.U0 - 120.f, D.U1 + 120.f })
            {
                const FVector L(U, Y, D.Z1 + 30.f);
                // A backplate on the wall and the arm out to the lantern.
                M.BoxMinMax(TEXT("CP_SteelDark"), FVector(U - 9.f, B.D * .5f, L.Z - 16.f), FVector(U + 9.f, B.D * .5f + 2.f, L.Z + 16.f), BI::Plain());
                M.BoxMinMax(TEXT("CP_SteelDark"), FVector(U - 3.f, B.D * .5f, L.Z - 3.f), FVector(U + 3.f, Y - 8.f, L.Z + 3.f), BI::Plain());
                M.Box(TEXT("CP_SteelDark"), FTransform(L + FVector(0, -8.f, 0)), FVector(8.f, 8.f, 3.f), BI::Plain());
                M.Cylinder(TEXT("CP_SteelDark"), L + FVector(0, 12.f, -6.f), L + FVector(0, 12.f, 40.f), 13.f, 6, BI::Plain());
                Glow.Cylinder(TEXT("CP_LampGlow"), L + FVector(0, 12.f, 0.f), L + FVector(0, 12.f, 34.f), 11.f, 6, BI::Plain());
                P.Pools.Add({ B.T.TransformPosition(FVector(U, Y + 120.f, BI::G + 2.f)), 260.f, FLinearColor(1.f, .75f, .45f), .35f });
            }
            if (Id == TEXT("MAIN") || Id == TEXT("ALU") || Id == TEXT("LIB"))
            {
                const float Z0 = BI::G + 360.f, Cx = (D.U0 + D.U1) * .5f, Hw = (D.U1 - D.U0) * .5f + 260.f;
                M.BoxMinMax(TEXT("CP_Ashlar"), FVector(Cx - Hw, B.D * .5f, Z0), FVector(Cx + Hw, B.D * .5f + 60.f, Z0 + 40.f), BI::Plain());
                HouseCampusFallKit::Tri(M, TEXT("CP_Ashlar"), FVector(Cx - Hw, B.D * .5f + 50.f, Z0 + 40.f), FVector(Cx + Hw, B.D * .5f + 50.f, Z0 + 40.f), FVector(Cx, B.D * .5f + 50.f, Z0 + 40.f + Hw * .32f), FVector(0, 1, 0));
                for (const float Sd : { -1.f, 1.f })
                {
                    const FVector A(Cx + Sd * Hw, B.D * .5f + 56.f, Z0 + 40.f), Top(Cx, B.D * .5f + 56.f, Z0 + 40.f + Hw * .32f);
                    HouseCampusFallKit::Bar(M, TEXT("CP_Ashlar"), A + FVector(Sd * 10.f, 0, -4.f), Top + FVector(0, 0, 10.f), 22.f, 14.f, FVector(0, 1, 0));
                    S.Cylinder(TEXT("CP_Ashlar"), FVector(Cx + Sd * (Hw - 60.f), B.D * .5f + 30.f, BI::G - 2.f), FVector(Cx + Sd * (Hw - 60.f), B.D * .5f + 30.f, Z0), 30.f, 16, BI::Plain());   // engaged columns
                }
            }
        }
        BI::AppendTransformed(P.Far, M, B.T);
        BI::AppendTransformed(P.Glow, Glow, B.T);
        BI::AppendTransformed(P.Detail, Dm, B.T);
        BI::AppendTransformed(P.Solid, S, B.T);
    }
}
