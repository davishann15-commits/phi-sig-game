// Runtime kit furniture (meshes built identically on every machine), focus highlight and the house detail subsystem.
#include "HouseDetail.h"
#include "HouseExterior.h"
#include "HouseWorldScale.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseFurnitureInteractionComponent.h"
#include "HouseGadgets.h"
#include "GameFramework/Pawn.h"
#include "StoryCampaign.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

static TAutoConsoleVariable<float> CVarHouseShadowCull(TEXT("house.ShadowCullDistance"), 0.f,
    TEXT("House lights further than this (cm) from the camera stop casting shadows until the player comes back (0 = never). Measured: no gain on top of the draw-distance cull, so off by default."));
static TAutoConsoleVariable<float> CVarHouseLightCull(TEXT("house.LightCullDistance"), 3200.f,
    TEXT("Max draw distance (cm) given to house lights in big open spaces (great room, long halls) that have none. 0 disables the cull."), ECVF_Default);
static TAutoConsoleVariable<float> CVarHouseLightCullSmall(TEXT("house.LightCullDistanceSmallRoom"), 0.f,
    TEXT("Opt-in (0 = off): shorter max draw distance (cm) for lights in enclosed rooms. 2000 saved ~3-5 ms a frame on the stairs, but the room test also caught great-room lights and the far end of the great room went dark, so it is off until that is fixed."), ECVF_Default);
static TAutoConsoleVariable<int32> CVarHouseFloorCull(TEXT("house.FloorLightCull"), 0,
    TEXT("1 = lights on another floor that the camera has no line of sight to fade out. Measured: no gain in the stairwell, and light leaking through the stair shaft visibly dimmed, so off by default."), ECVF_Default);

// =============================================================================================
// Kit geometry
namespace
{
const FHallUV HwPlainUV = FHallUV::World(50.f);
}

void HwFrustum(FHallMesh& M, FName Key, const FVector& Base, float R0, float R1, float H, int32 Seg, bool bInside, float V0, float V1)
{
    FHallSection& S = M.Sec(Key);
    const float Slope = (R0 - R1) / FMath::Max(1.f, H);
    for (int32 K = 0; K < Seg; ++K)
    {
        const float A0 = 2 * PI * K / Seg, A1 = 2 * PI * (K + 1) / Seg;
        const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
        const int32 B = S.V.Num();
        const FVector P[4] = { Base + D0 * R0, Base + D1 * R0, Base + D1 * R1 + FVector(0, 0, H), Base + D0 * R1 + FVector(0, 0, H) };
        const FVector N[4] = { (D0 + FVector(0, 0, Slope)).GetSafeNormal(), (D1 + FVector(0, 0, Slope)).GetSafeNormal(), (D1 + FVector(0, 0, Slope)).GetSafeNormal(), (D0 + FVector(0, 0, Slope)).GetSafeNormal() };
        const float U0 = K / float(Seg) * 3.f, U1 = (K + 1) / float(Seg) * 3.f;
        const FVector2D UV[4] = { FVector2D(U0, V1), FVector2D(U1, V1), FVector2D(U1, V0), FVector2D(U0, V0) };
        for (int32 Q = 0; Q < 4; ++Q) { S.V.Add(P[Q]); S.N.Add(bInside ? -N[Q] : N[Q]); S.UV.Add(UV[Q]); S.T.Add(FProcMeshTangent(FVector::CrossProduct(N[Q], FVector::UpVector), false)); }
        const FVector Nrm = bInside ? -(N[0] + N[1]) : (N[0] + N[1]);
        if (FVector::DotProduct(FVector::CrossProduct(P[1] - P[0], P[2] - P[0]), Nrm) < 0) S.I.Append({ B, B + 1, B + 2, B, B + 2, B + 3 });
        else S.I.Append({ B, B + 2, B + 1, B, B + 3, B + 2 });
    }
}
void HwDisc(FHallMesh& M, FName Key, const FVector& C, float R, int32 Seg, bool bUp)
{
    FHallSection& S = M.Sec(Key);
    const FVector N = bUp ? FVector::UpVector : -FVector::UpVector;
    const int32 B = S.V.Num();
    S.V.Add(C); S.N.Add(N); S.UV.Add(FVector2D(.5f, .5f)); S.T.Add(FProcMeshTangent(FVector::ForwardVector, false));
    for (int32 K = 0; K <= Seg; ++K)
    {
        const float A = 2 * PI * K / Seg;
        S.V.Add(C + FVector(FMath::Cos(A), FMath::Sin(A), 0) * R); S.N.Add(N); S.UV.Add(FVector2D(.5f + .5f * FMath::Cos(A), .5f + .5f * FMath::Sin(A))); S.T.Add(FProcMeshTangent(FVector::ForwardVector, false));
    }
    for (int32 K = 0; K < Seg; ++K)
    {
        const FVector P0 = S.V[B], P1 = S.V[B + 1 + K], P2 = S.V[B + 2 + K];
        if (FVector::DotProduct(FVector::CrossProduct(P1 - P0, P2 - P0), N) < 0) S.I.Append({ B, B + 1 + K, B + 2 + K });
        else S.I.Append({ B, B + 2 + K, B + 1 + K });
    }
}
/** Superellipsoid: a box with rounded edges (E near 0 = boxy, 1 = ellipsoid). Local X/Y/Z half sizes, placed by T. */
void HwSuperBox(FHallMesh& M, FName Key, const FTransform& T, const FVector& Half, float E1, float E2, int32 SegU, int32 SegV, FVector2D Tile)
{
    auto SC = [](float A, float E) { const float C = FMath::Cos(A); return FMath::Sign(C) * FMath::Pow(FMath::Abs(C), E); };
    auto SS = [](float A, float E) { const float S = FMath::Sin(A); return FMath::Sign(S) * FMath::Pow(FMath::Abs(S), E); };
    FHallSection& S = M.Sec(Key);
    const int32 Start = S.V.Num();
    for (int32 Rg = 0; Rg <= SegV; ++Rg)
    {
        const float Phi = -HALF_PI + PI * Rg / SegV;
        for (int32 K = 0; K <= SegU; ++K)
        {
            const float Th = -PI + 2 * PI * K / SegU;
            const FVector L(Half.X * SC(Phi, E1) * SC(Th, E2), Half.Y * SC(Phi, E1) * SS(Th, E2), Half.Z * SS(Phi, E1));
            FVector N(SC(Phi, 2 - E1) * SC(Th, 2 - E2) / Half.X, SC(Phi, 2 - E1) * SS(Th, 2 - E2) / Half.Y, SS(Phi, 2 - E1) / Half.Z);
            N = N.GetSafeNormal();
            const FVector Tan = T.TransformVectorNoScale(FVector(-FMath::Sin(Th), FMath::Cos(Th), 0).GetSafeNormal());
            S.V.Add(T.TransformPosition(L)); S.N.Add(T.TransformVectorNoScale(N)); S.UV.Add(FVector2D(K / float(SegU) * Tile.X, Rg / float(SegV) * Tile.Y)); S.T.Add(FProcMeshTangent(Tan, false));
        }
    }
    for (int32 Rg = 0; Rg < SegV; ++Rg)
        for (int32 K = 0; K < SegU; ++K)
        {
            const int32 A = Start + Rg * (SegU + 1) + K, B = A + 1, C = A + SegU + 1, D = C + 1;
            const FVector Nrm = S.N[A] + S.N[D];
            if (FVector::DotProduct(FVector::CrossProduct(S.V[B] - S.V[A], S.V[D] - S.V[A]), Nrm) < 0) S.I.Append({ A, B, D, A, D, C });
            else S.I.Append({ A, D, B, A, C, D });
        }
}

void HwBuildKitProp(EHouseKitProp Kind, uint8 Variant, FHallMesh& M, TArray<FBox>& Col)
{
    const FHallUV UV = HwPlainUV;
    auto Box = [&](FName Key, const FVector& Min, const FVector& Max, const FHallUV& U = HwPlainUV) { M.BoxMinMax(Key, Min, Max, U); };
    switch (Kind)
    {
    case EHouseKitProp::Recliner:
    {
        // Black leather recliner: X forward (front), 90 wide, 92 deep, 100 tall. Soft parts are superellipsoids.
        const FHallUV Leather = FHallUV::World(60.f);
        Box(TEXT("HW_Black"), FVector(-42, -39, 0), FVector(38, 39, 8));                                                  // base
        Box(TEXT("HW_Leather"), FVector(-36, -30, 8), FVector(36, 30, 23), Leather);                                     // seat deck
        HwSuperBox(M, TEXT("HW_Leather"), FTransform(FVector(3, 0, 33)), FVector(33, 29.5f, 10), .45f, .3f, 24, 12);     // seat cushion
        for (const float Side : { -1.f, 1.f })
        {
            HwSuperBox(M, TEXT("HW_Leather"), FTransform(FVector(-1, Side * 38, 35)), FVector(43, 8, 33), .22f, .22f, 28, 14, FVector2D(4.f, 2.f)); // arm
            const FVector Cup(27, Side * 38, 67.6f);
            M.Cylinder(TEXT("HW_White"), Cup, Cup + FVector(0, 0, .8f), 6.4f, 18, UV);
            M.Cylinder(TEXT("HW_Black"), Cup + FVector(0, 0, -5.f), Cup + FVector(0, 0, .9f), 4.5f, 16, UV);
            if (Side < 0) M.Cylinder(TEXT("HW_Black"), Cup + FVector(0, 0, .9f), Cup + FVector(0, 0, 1.6f), 5.2f, 16, UV);  // lid on one holder
        }
        // Back leans away from the seat (positive pitch tips the top toward -X).
        const FRotator Lean(14.f, 0, 0);
        HwSuperBox(M, TEXT("HW_Leather"), FTransform(Lean, FVector(-30, 0, 60)), FVector(9, 31, 42), .25f, .22f, 28, 16, FVector2D(3.f, 2.f));   // back
        HwSuperBox(M, TEXT("HW_Leather"), FTransform(Lean, FVector(-19, 0, 88)), FVector(6.5f, 24, 12), .6f, .45f, 24, 12);                     // headrest
        HwSuperBox(M, TEXT("HW_Leather"), FTransform(Lean, FVector(-20, 0, 54)), FVector(5.5f, 27, 15), .6f, .45f, 24, 12);                     // lumbar
        // Footrest lip.
        HwSuperBox(M, TEXT("HW_Leather"), FTransform(FVector(38, 0, 21)), FVector(4.5f, 30, 9), .35f, .3f, 20, 10);
        Col.Add(FBox(FVector(-44, -46, 0), FVector(42, 46, 68)));
        Col.Add(FBox(FVector(-48, -31, 60), FVector(-12, 31, 104)));
        break;
    }
    case EHouseKitProp::GreenChair:
    {
        // Dark green steel chair with a vinyl seat; fan back of rods.
        const FName Fr = TEXT("HW_GreenFrame");
        const FVector Legs[4] = { FVector(19, 18, 0), FVector(19, -18, 0), FVector(-17, 20, 0), FVector(-17, -20, 0) };
        for (const FVector& L : Legs) M.Cylinder(Fr, L + FVector(0, 0, .5f), L * FVector(.85f, .85f, 1) + FVector(0, 0, 44), 1.3f, 8, UV);
        for (const FVector& L : Legs) M.Cylinder(TEXT("HW_Black"), L, L + FVector(0, 0, 1.2f), 1.8f, 8, UV);
        M.Cylinder(Fr, FVector(16, -15, 44), FVector(16, 15, 44), 1.1f, 8, UV);
        M.Cylinder(Fr, FVector(-14, -17, 44), FVector(-14, 17, 44), 1.1f, 8, UV);
        M.Cylinder(Fr, FVector(16, 15, 44), FVector(-14, 17, 44), 1.1f, 8, UV);
        M.Cylinder(Fr, FVector(16, -15, 44), FVector(-14, -17, 44), 1.1f, 8, UV);
        M.Cylinder(Fr, FVector(19, 18, 22), FVector(-17, 20, 22), .9f, 8, UV);
        M.Cylinder(Fr, FVector(19, -18, 22), FVector(-17, -20, 22), .9f, 8, UV);
        Box(TEXT("HW_GreenVinyl"), FVector(-16, -19, 44.5f), FVector(18, 19, 49));
        // Back rods fanning out to a curved top rail.
        for (int32 K = 0; K < 5; ++K)
        {
            const float Y0 = -12.f + 6.f * K, Y1 = -18.f + 9.f * K;
            M.Cylinder(Fr, FVector(-15, Y0, 48), FVector(-20, Y1, 90), .9f, 8, UV);
        }
        FVector Prev(-20, -22, 86);
        for (int32 K = 1; K <= 8; ++K)
        {
            const float T = -22.f + 44.f * K / 8.f;
            const FVector P(-20 - 3.f * FMath::Sin(PI * K / 8.f), T, 86 + 6.f * FMath::Sin(PI * K / 8.f));
            M.Cylinder(Fr, Prev, P, 1.3f, 8, UV); Prev = P;
        }
        M.Cylinder(Fr, FVector(-20, -22, 86), FVector(-17, -20, 44), 1.3f, 8, UV);
        M.Cylinder(Fr, FVector(-20, 22, 86), FVector(-17, 20, 44), 1.3f, 8, UV);
        Col.Add(FBox(FVector(-18, -21, 0), FVector(20, 21, 49)));
        Col.Add(FBox(FVector(-22, -23, 49), FVector(-14, 23, 93)));
        break;
    }
    case EHouseKitProp::Bench:
    {
        // Rustic pine bench / low table 120 x 45 x 45.
        const FHallUV Wood = FHallUV::World(90.f);
        Box(TEXT("HW_Pine"), FVector(-22, -60, 40), FVector(22, 60, 45.5f), Wood);
        Box(TEXT("HW_Pine"), FVector(-20, -58, 34), FVector(20, 58, 40), Wood);           // apron
        for (const float X : { -17.f, 17.f })
            for (const float Y : { -54.f, 54.f })
            {
                Box(TEXT("HW_Pine"), FVector(X - 3.5f, Y - 3.5f, 0), FVector(X + 3.5f, Y + 3.5f, 34), Wood);
                Box(TEXT("HW_Pine"), FVector(X - 4.5f, Y - 4.5f, 8), FVector(X + 4.5f, Y + 4.5f, 12), Wood);   // turned ring
                Box(TEXT("HW_Pine"), FVector(X - 4.2f, Y - 4.2f, 22), FVector(X + 4.2f, Y + 4.2f, 25), Wood);
            }
        Box(TEXT("HW_Pine"), FVector(-14, -56, 4), FVector(14, 56, 8), Wood);             // low shelf rail
        Col.Add(FBox(FVector(-22, -60, 0), FVector(22, 60, 45.5f)));
        break;
    }
    case EHouseKitProp::WickerBasket:
    {
        HwFrustum(M, TEXT("HW_Wicker"), FVector::ZeroVector, 19.f, 25.f, 46.f, 20, false, 0.f, 1.8f);
        HwFrustum(M, TEXT("HW_Wicker"), FVector(0, 0, 1.5f), 17.5f, 23.5f, 44.6f, 20, true, 0.f, 1.8f);
        HwDisc(M, TEXT("HW_Wicker"), FVector(0, 0, 0), 19.f, 20, false);
        HwDisc(M, TEXT("HW_Wicker"), FVector(0, 0, 1.5f), 17.5f, 20, true);
        HwFrustum(M, TEXT("HW_Tan"), FVector(0, 0, 44.6f), 25.4f, 25.4f, 2.4f, 20, false);
        HwFrustum(M, TEXT("HW_Tan"), FVector(0, 0, 44.6f), 23.1f, 23.1f, 2.4f, 20, true);
        HwFrustum(M, TEXT("HW_Tan"), FVector(0, 0, 22.f), 22.4f, 22.4f, 2.f, 20, false);
        // Rim top ring.
        FHallSection& S = M.Sec(TEXT("HW_Tan"));
        for (int32 K = 0; K < 20; ++K)
        {
            const float A0 = 2 * PI * K / 20, A1 = 2 * PI * (K + 1) / 20;
            const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
            const int32 B = S.V.Num();
            const FVector P[4] = { D0 * 23.1f + FVector(0, 0, 47), D1 * 23.1f + FVector(0, 0, 47), D1 * 25.4f + FVector(0, 0, 47), D0 * 25.4f + FVector(0, 0, 47) };
            for (int32 Q = 0; Q < 4; ++Q) { S.V.Add(P[Q]); S.N.Add(FVector::UpVector); S.UV.Add(FVector2D(P[Q].X / 50, P[Q].Y / 50)); S.T.Add(FProcMeshTangent(FVector::ForwardVector, false)); }
            if (FVector::DotProduct(FVector::CrossProduct(P[1] - P[0], P[2] - P[0]), FVector::UpVector) < 0) S.I.Append({ B, B + 1, B + 2, B, B + 2, B + 3 });
            else S.I.Append({ B, B + 2, B + 1, B, B + 3, B + 2 });
        }
        // Handles.
        for (const float Side : { -1.f, 1.f })
        {
            const FVector H0(Side * 20.f, -6, 40), H1(Side * 25.f, 0, 50), H2(Side * 20.f, 6, 40);
            M.Cylinder(TEXT("HW_Tan"), H0, H1, 1.2f, 8, UV); M.Cylinder(TEXT("HW_Tan"), H1, H2, 1.2f, 8, UV);
        }
        Col.Add(FBox(FVector(-25, -25, 0), FVector(25, 25, 47)));
        break;
    }
    case EHouseKitProp::TeaJug:
    {
        // Gallon jug of sweet tea.
        const float Fill = Variant == 1 ? 14.f : 23.f;
        Box(TEXT("HW_Tea"), FVector(-5, -5, 0), FVector(5, 5, Fill));
        Box(TEXT("HW_ClearPlastic"), FVector(-5.3f, -5.3f, 0), FVector(5.3f, 5.3f, 24.5f));
        M.Cylinder(TEXT("HW_ClearPlastic"), FVector(0, 0, 24.5f), FVector(0, 0, 27.5f), 2.4f, 12, UV, false);
        M.Cylinder(TEXT("HW_Red"), FVector(0, 0, 27.5f), FVector(0, 0, 30.f), 2.9f, 12, UV);
        Box(TEXT("HW_ClearPlastic"), FVector(2.5f, -1.2f, 18), FVector(7.5f, 1.2f, 24));     // handle
        // Label band.
        Box(TEXT("HW_White"), FVector(-5.45f, -5.45f, 6), FVector(5.45f, 5.45f, 15));
        Box(TEXT("HW_Red"), FVector(-5.5f, -5.5f, 8), FVector(5.5f, 5.5f, 13));
        Col.Add(FBox(FVector(-5.5f, -5.5f, 0), FVector(5.5f, 5.5f, 30)));
        break;
    }
    case EHouseKitProp::Pillow:
    {
        // Throw pillow: soft-cornered square in plan, lens profile, slightly squashed on one side.
        const bool bGrey = Variant == 1;
        const float R = bGrey ? 19.f : 25.f, H = bGrey ? 5.5f : 7.f;
        const FName Key = bGrey ? TEXT("HW_LinenGrey") : TEXT("HW_Linen");
        HwSuperBox(M, Key, FTransform(FRotator(0, bGrey ? 15.f : 0.f, 0), FVector(0, 0, H)), FVector(R, R * .92f, H), .9f, .5f, 24, 12, FVector2D(3.f, 1.5f));
        Col.Add(FBox(FVector(-R, -R, 0), FVector(R, R, H * 2.f)));
        break;
    }
    case EHouseKitProp::BlueBag:
    {
        // Stuffed laundry bag: lumpy domes and a tied neck.
        M.Dome(TEXT("HW_BluePlastic"), FVector(0, 0, 0), FVector::UpVector, 20.f, 26.f, 16);
        M.Dome(TEXT("HW_BluePlastic"), FVector(9, -6, 0), FVector::UpVector, 14.f, 20.f, 14);
        M.Dome(TEXT("HW_BluePlastic"), FVector(-8, 7, 0), FVector::UpVector, 13.f, 22.f, 14);
        HwDisc(M, TEXT("HW_BluePlastic"), FVector(0, 0, .2f), 20.f, 16, false);
        M.Cylinder(TEXT("HW_BluePlastic"), FVector(0, 0, 23), FVector(0, 0, 31), 3.5f, 10, UV);
        M.Cylinder(TEXT("HW_White"), FVector(0, 0, 25), FVector(0, 0, 26.5f), 4.2f, 10, UV);
        M.Dome(TEXT("HW_BluePlastic"), FVector(0, 0, 31), FVector::UpVector, 5.f, 5.f, 10);
        Col.Add(FBox(FVector(-21, -21, 0), FVector(21, 21, 30)));
        break;
    }
    case EHouseKitProp::WoodBoard:
    {
        const FHallUV Wood = FHallUV::World(90.f);
        Box(TEXT("HW_Pine"), FVector(-43, -4.5f, 0), FVector(43, 4.5f, 3.8f), Wood);
        Col.Add(FBox(FVector(-43, -4.5f, 0), FVector(43, 4.5f, 3.8f)));
        break;
    }
    case EHouseKitProp::DoorMat:
    {
        const bool bGrey = Variant == 1;
        FHallUV MatUV; MatUV.Mode = 1; MatUV.Frame = FTransform::Identity; MatUV.Period = bGrey ? 60.f : 30.f;
        M.BoxMinMax(bGrey ? TEXT("HW_MatGrey") : TEXT("HW_MatBlack"), FVector(-35, -22, 0), FVector(35, 22, 1.6f), MatUV);
        Col.Add(FBox(FVector(-35, -22, 0), FVector(35, 22, 1.6f)));
        break;
    }
    case EHouseKitProp::BucketLid:
    {
        // White five-gallon bucket lid, dropped face up: a flat disc with a raised rim and a shallow dish.
        HwFrustum(M, TEXT("HW_White"), FVector(0, 0, 0), 15.5f, 15.5f, 2.6f, 28, false);
        HwDisc(M, TEXT("HW_White"), FVector(0, 0, 0), 15.5f, 28, false);
        HwDisc(M, TEXT("HW_White"), FVector(0, 0, 2.6f), 15.5f, 28, true);
        HwFrustum(M, TEXT("HW_White"), FVector(0, 0, 2.6f), 13.2f, 12.6f, .6f, 28, true);
        HwDisc(M, TEXT("HW_White"), FVector(0, 0, 2.0f), 12.6f, 28, true);
        Col.Add(FBox(FVector(-15.5f, -15.5f, 0), FVector(15.5f, 15.5f, 2.6f)));
        break;
    }
    case EHouseKitProp::CrumpledShirt:
    {
        // A t-shirt dropped on the floor: a few overlapping soft lumps.
        HwSuperBox(M, TEXT("HW_Shirt"), FTransform(FRotator(0, 15, 0), FVector(0, 0, 3.2f)), FVector(24, 17, 3.4f), .9f, .95f, 20, 10);
        HwSuperBox(M, TEXT("HW_Shirt"), FTransform(FRotator(0, -30, 0), FVector(9, 7, 4.6f)), FVector(14, 9, 3.8f), .9f, .95f, 16, 8);
        HwSuperBox(M, TEXT("HW_Shirt"), FTransform(FRotator(0, 50, 0), FVector(-11, -5, 4.0f)), FVector(12, 8, 3.2f), .9f, .95f, 16, 8);
        HwSuperBox(M, TEXT("HW_Shirt"), FTransform(FRotator(8, 0, 0), FVector(-2, -12, 2.4f)), FVector(10, 6, 2.4f), .9f, .95f, 12, 6);
        Col.Add(FBox(FVector(-25, -19, 0), FVector(25, 19, 7.5f)));
        break;
    }
    default: break;
    }
}

UStaticMesh* HwBuildStaticMesh(const FHallMesh& Mesh, const TCHAR* Name, UHouseBlackHallSubsystem* Look, const TArray<FBox>& Collision)
{
    UStaticMesh* SM = NewObject<UStaticMesh>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UStaticMesh::StaticClass(), FName(Name)), RF_Transient);
    SM->bAllowCPUAccess = true;
    SM->NeverStream = true;
    FMeshDescription MD;
    FStaticMeshAttributes Attr(MD); Attr.Register();
    TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
    TVertexInstanceAttributesRef<FVector3f> Nrm = Attr.GetVertexInstanceNormals();
    TVertexInstanceAttributesRef<FVector3f> Tan = Attr.GetVertexInstanceTangents();
    TVertexInstanceAttributesRef<float> BiSign = Attr.GetVertexInstanceBinormalSigns();
    TVertexInstanceAttributesRef<FVector2f> UVs = Attr.GetVertexInstanceUVs();
    UVs.SetNumChannels(1);
    TPolygonGroupAttributesRef<FName> Slots = Attr.GetPolygonGroupMaterialSlotNames();
    TArray<FStaticMaterial>& Mats = SM->GetStaticMaterials();
    for (const FName& Key : Mesh.Order)
    {
        const FHallSection& S = Mesh.Sections[Key];
        if (S.I.Num() == 0) continue;
        const FPolygonGroupID Group = MD.CreatePolygonGroup();
        Slots[Group] = Key;
        Mats.Add(FStaticMaterial(Look->Mat(Key), Key, Key));
        TArray<FVertexID> Ids; Ids.Reserve(S.V.Num());
        for (const FVector& V : S.V) { const FVertexID Id = MD.CreateVertex(); Pos[Id] = FVector3f(V); Ids.Add(Id); }
        for (int32 T = 0; T + 2 < S.I.Num(); T += 3)
        {
            TArray<FVertexInstanceID> Inst;
            for (int32 K = 0; K < 3; ++K)
            {
                const int32 Idx = S.I[T + K];
                const FVertexInstanceID I = MD.CreateVertexInstance(Ids[Idx]);
                Nrm[I] = FVector3f(S.N.IsValidIndex(Idx) ? S.N[Idx] : FVector::UpVector);
                Tan[I] = FVector3f(S.T.IsValidIndex(Idx) ? S.T[Idx].TangentX : FVector::ForwardVector);
                BiSign[I] = 1.f;
                UVs.Set(I, 0, FVector2f(S.UV.IsValidIndex(Idx) ? S.UV[Idx] : FVector2D::ZeroVector));
                Inst.Add(I);
            }
            MD.CreatePolygon(Group, Inst);
        }
    }
    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = false;
    Params.bFastBuild = true;
    Params.bAllowCpuAccess = true;
    Params.bMarkPackageDirty = false;
    Params.bCommitMeshDescription = false;
    if (!SM->BuildFromMeshDescriptions({ &MD }, Params))
    {
        UE_LOG(LogTemp, Warning, TEXT("HOUSE_KIT mesh build failed for %s"), Name);
        return nullptr;
    }
    SM->CreateBodySetup();
    if (UBodySetup* BS = SM->GetBodySetup())
    {
        BS->CollisionTraceFlag = CTF_UseSimpleAsComplex;
        BS->AggGeom.EmptyElements();
        for (const FBox& B : Collision)
        {
            FKBoxElem E;
            E.Center = B.GetCenter();
            E.X = B.GetSize().X; E.Y = B.GetSize().Y; E.Z = B.GetSize().Z;
            BS->AggGeom.BoxElems.Add(E);
        }
        BS->bGenerateMirroredCollision = false;
        BS->InvalidatePhysicsData();
        BS->CreatePhysicsMeshes();
    }
    return SM;
}

// =============================================================================================
// Kit prop
namespace
{
struct FHwKitTuning { const TCHAR* Name; float Mass; bool bBreakable; float Health; };
const FHwKitTuning HwTuning[(int32)EHouseKitProp::Count] = {
    { TEXT("leather recliner"), 48.f, false, 30.f },
    { TEXT("green vinyl chair"), 6.5f, true, 16.f },
    { TEXT("pine bench"), 16.f, true, 24.f },
    { TEXT("wicker basket"), 2.2f, true, 6.f },
    { TEXT("jug of sweet tea"), 4.f, true, 4.f },
    { TEXT("pillow"), .9f, false, 8.f },
    { TEXT("laundry bag"), 3.5f, false, 8.f },
    { TEXT("scrap two-by-four"), 2.4f, false, 10.f },
    { TEXT("hand vacuum"), 2.5f, true, 6.f },
    { TEXT("door mat"), 1.2f, false, 10.f },
    { TEXT("bucket lid"), .4f, false, 6.f },
    { TEXT("crumpled shirt"), .3f, false, 6.f },
};
}

AHouseKitProp::AHouseKitProp()
{
    bBreakable = false;
}

void AHouseKitProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(AHouseKitProp, KitKind, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AHouseKitProp, KitVariant, COND_InitialOnly);
}

void AHouseKitProp::ApplyKit()
{
    UHouseDetailSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UHouseDetailSubsystem>() : nullptr;
    if (!Sub || KitKind >= (uint8)EHouseKitProp::Count) return;
    const FHwKitTuning& T = HwTuning[KitKind];
    PropName = T.Name;
    PropId = FName(*FString::Printf(TEXT("Kit_%s_%s"), T.Name, *GetName()));
    MassKg = T.Mass;
    bBreakable = T.bBreakable;
    BreakHealth = T.Health;
    bMovable = true;
    bWallMounted = false;
    ImpactBreakSpeed = 700.f;
    if (UStaticMesh* Mesh = Sub->KitMesh((EHouseKitProp)KitKind, KitVariant)) IntactMesh = Mesh;
    RefreshProp();
}

void AHouseKitProp::BeginPlay()
{
    if (!IntactMesh) ApplyKit();
    Super::BeginPlay();
}

// =============================================================================================
// Subsystem
bool UHouseDetailSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UHouseBlackHallSubsystem* UHouseDetailSubsystem::Look() const
{
    return GetWorld() ? GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>() : nullptr;
}

UStaticMesh* UHouseDetailSubsystem::KitMesh(EHouseKitProp Kind, uint8 Variant)
{
    const uint8 Key = (uint8)Kind | (Variant << 6);
    if (TObjectPtr<UStaticMesh>* Found = KitMeshes.Find(Key)) return *Found;
    UHouseBlackHallSubsystem* L = Look();
    if (!L) return nullptr;
    L->CreateLook();
    HwCreateLook(L);
    FHallMesh M; TArray<FBox> Col;
    HwBuildKitProp(Kind, Variant, M, Col);
    UStaticMesh* Result = HwBuildStaticMesh(M, *FString::Printf(TEXT("SM_HouseKit_%d_%d"), (int32)Kind, Variant), L, Col);
    KitMeshes.Add(Key, Result);
    return Result;
}

void UHouseDetailSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (UGameplayStatics::GetCurrentLevelName(&InWorld, true) != TEXT("Chapter01_House")) return;
    if (FParse::Param(FCommandLine::Get(), TEXT("NoHouseDetail"))) return;
    const double Start = FPlatformTime::Seconds();
    ApplyRenderingTweaks(InWorld);
    // The old static Santa bass prop is replaced by the animated singing bass (AHouseGadget); keep only one on the wall.
    for (TActorIterator<AHouseInteractiveProp> It(&InWorld); It; ++It)
        if (It->IntactMesh && It->IntactMesh->GetName().Contains(TEXT("SantaBass")))
        {
            It->SetActorHiddenInGame(true);
            It->SetActorEnableCollision(false);
            It->SetActorTickEnabled(false);
        }
    if (UHouseBlackHallSubsystem* L = Look())
    {
        L->CreateLook();
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.ObjectFlags |= RF_Transient;
        if (AHouseHallwayDetail* H = InWorld.SpawnActor<AHouseHallwayDetail>(AHouseHallwayDetail::StaticClass(),
            HouseWorldScale::Transform(&InWorld, FTransform::Identity), Params))
        {
            Hallway = H;
            H->Build(L);
        }
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("NoHouseExterior")))
        if (UHouseBlackHallSubsystem* L = Look())
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Params.ObjectFlags |= RF_Transient;
            if (AHouseExteriorDetail* E = InWorld.SpawnActor<AHouseExteriorDetail>(AHouseExteriorDetail::StaticClass(), FTransform::Identity, Params))
            {
                Exterior = E;
                E->Build(L);
            }
        }
    if (InWorld.GetNetMode() != NM_Client) { SpawnKitProps(InWorld); SpawnGadgets(InWorld); }
    if (UMaterialInterface* GlassParent = HallHouseMat(TEXT("M_House_Glass")))
    {
        HighlightMaterial = UMaterialInstanceDynamic::Create(GlassParent, this);
        HighlightMaterial->SetVectorParameterValue(TEXT("Tint"), FLinearColor(1.f, .85f, .5f));
        HighlightMaterial->SetScalarParameterValue(TEXT("Opacity"), .18f);
        HighlightMaterial->SetScalarParameterValue(TEXT("Roughness"), .6f);
        HighlightMaterial->SetVectorParameterValue(TEXT("Emission"), FLinearColor(.6f, .45f, .2f));
    }
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UHouseDetailSubsystem::TickHighlight));
    UE_LOG(LogTemp, Display, TEXT("HOUSE_DETAIL ready hallway=%d kitprops=%d gadgets=%d culledLights=%d ms=%.0f"), Hallway.IsValid() ? Hallway->Placed : -1, KitProps.Num(), Gadgets.Num(), CulledLights, (FPlatformTime::Seconds() - Start) * 1000.0);
}

void UHouseDetailSubsystem::SpawnGadgets(UWorld& World)
{
    AHouseHallwayDetail* H = Hallway.Get();
    if (!H) return;
    for (const FHouseGadgetSpec& Spec : H->GadgetSpecs)
    {
        const FTransform Frame = HouseWorldScale::Transform(&World, Spec.Frame);
        AHouseGadget* G = World.SpawnActorDeferred<AHouseGadget>(AHouseGadget::StaticClass(), Frame, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!G) continue;
        G->Kind = Spec.Kind;
        G->Variant = Spec.Variant;
        G->FinishSpawning(Frame);
        Gadgets.Add(G);
    }
}

void UHouseDetailSubsystem::TriggerFireAlarm(float Seconds)
{
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client) return;
    for (const TWeakObjectPtr<AHouseGadget>& W : Gadgets)
        if (AHouseGadget* G = W.Get()) if (G->Kind == EHouseGadgetKind::HornStrobe) G->SetAlarm(true);
    World->GetTimerManager().SetTimer(AlarmTimer, FTimerDelegate::CreateWeakLambda(this, [this]
    {
        for (const TWeakObjectPtr<AHouseGadget>& W : Gadgets)
            if (AHouseGadget* G = W.Get()) G->SetAlarm(false);
    }), Seconds, false);
}

void UHouseDetailSubsystem::Deinitialize()
{
    if (TickerHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
    TickerHandle.Reset();
    Super::Deinitialize();
}

void UHouseDetailSubsystem::ApplyRenderingTweaks(UWorld& World)
{
    // Lights far from the player cost Lumen scene lighting and shadow pages every frame; give house lights a draw distance.
    // Enclosed rooms and corridors get the shorter distance; a light counts as in a big open room (great room) when most of
    // 8 horizontal traces around it run more than 9 m before hitting a wall.
    const float Cull = CVarHouseLightCull.GetValueOnGameThread();
    const float CullSmall = FMath::Min(Cull, CVarHouseLightCullSmall.GetValueOnGameThread());
    int32 Small = 0;
    if (Cull > 0.f)
        for (TObjectIterator<ULightComponent> It; It; ++It)
        {
            ULightComponent* L = *It;
            if (!IsValid(L) || L->GetWorld() != &World || L->IsA<UDirectionalLightComponent>() || L->MaxDrawDistance > 0.f) continue;
            float Draw = Cull;
            if (CullSmall > 0.f && CullSmall < Cull)
            {
                FCollisionQueryParams Q(SCENE_QUERY_STAT(HouseLightRoom), false, L->GetOwner());
                const FVector P = L->GetComponentLocation() - FVector(0, 0, 60.f);
                TArray<float, TInlineAllocator<8>> Free;
                for (int32 K = 0; K < 8; ++K)
                {
                    const FVector Dir = FRotator(0.f, K * 45.f, 0.f).Vector();
                    FHitResult Hit;
                    Free.Add(World.LineTraceSingleByChannel(Hit, P, P + Dir * 3000.f, ECC_Visibility, Q) ? Hit.Distance : 3000.f);
                }
                Free.Sort();
                if (Free[4] < 900.f) { Draw = CullSmall; ++Small; }
            }
            L->MaxDrawDistance = Draw;
            L->MaxDistanceFadeRange = Draw * .25f;
            L->MarkRenderStateDirty();
            ++CulledLights;
        }
    UE_LOG(LogTemp, Display, TEXT("HOUSE_LIGHT_CULL lights=%d enclosed=%d far=%.0f near=%.0f"), CulledLights, Small, Cull, CullSmall);
    for (TObjectIterator<ULightComponent> It; It; ++It)
    {
        ULightComponent* L = *It;
        if (IsValid(L) && L->GetWorld() == &World && !L->IsA<UDirectionalLightComponent>() && L->CastShadows) ShadowLights.Add(L);
    }
    for (TObjectIterator<ULightComponent> It; It; ++It)
    {
        ULightComponent* L = *It;
        if (!IsValid(L) || L->GetWorld() != &World || L->IsA<UDirectionalLightComponent>() || L->Mobility != EComponentMobility::Movable) continue;
        if (L->GetOwner() && (L->GetOwner()->IsA<AHouseGadget>() || L->GetOwner()->IsA<APawn>())) continue;   // strobes, flashlights
        FFloorLight F; F.Light = L;
        FloorLights.Add(F);
    }
}

int32 UHouseDetailSubsystem::FloorCulledNow() const
{
    int32 N = 0;
    for (const FFloorLight& F : FloorLights) if (F.Fade <= 0.f) ++N;
    return N;
}

void UHouseDetailSubsystem::TickFloorCull(UWorld& World, float DeltaTime)
{
    const bool bOn = CVarHouseFloorCull.GetValueOnGameThread() != 0;
    APlayerController* PC = World.GetFirstPlayerController();
    if (!PC || !PC->PlayerCameraManager || !FloorLights.Num()) return;
    const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
    // A few line-of-sight checks per frame, round robin.
    FloorClock += DeltaTime;
    if (FloorClock >= .05f)
    {
        FloorClock = 0.f;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(HouseFloorCull), false, PC->GetPawn());
        for (int32 K = 0; K < 8 && FloorLights.Num(); ++K)
        {
            FloorCursor = (FloorCursor + 1) % FloorLights.Num();
            FFloorLight& F = FloorLights[FloorCursor];
            ULightComponent* L = F.Light.Get();
            if (!L) continue;
            const FVector P = L->GetComponentLocation();
            bool bBlocked = false;
            if (bOn && FMath::Abs(P.Z - Eye.Z) > 220.f)
            {
                const FVector Dir = (P - Eye).GetSafeNormal();
                const FVector End = P - Dir * 35.f;   // stop short of the lamp's own shade
                FHitResult Hit;
                bBlocked = World.LineTraceSingleByChannel(Hit, Eye, End, ECC_Visibility, Q) && Hit.Distance < FVector::Dist(Eye, End) - 5.f;
                if (bBlocked && L->GetOwner() && Hit.GetActor() == L->GetOwner()) bBlocked = false;
            }
            F.Blocked = bBlocked ? F.Blocked + 1 : 0;
            F.bWantOn = F.Blocked < 2;
        }
    }
    for (FFloorLight& F : FloorLights)
    {
        ULightComponent* L = F.Light.Get();
        if (!L) continue;
        const float Target = (F.bWantOn || !bOn) ? 1.f : 0.f;
        if (F.Fade == Target) continue;
        if (F.Fade >= 1.f) F.Base = L->Intensity;          // starting to fade out: remember the designed brightness
        if (F.Fade <= 0.f) { L->bAffectsWorld = true; L->MarkRenderStateDirty(); }
        F.Fade = FMath::FInterpConstantTo(F.Fade, Target, DeltaTime, 2.5f);
        L->SetIntensity(F.Base * F.Fade);
        if (F.Fade <= 0.f) { L->bAffectsWorld = false; L->MarkRenderStateDirty(); }
    }
}

void UHouseDetailSubsystem::TickShadowCull(UWorld& World, float DeltaTime)
{
    // Shadow pages for lights in other rooms are the biggest remaining GPU cost; drop them while far away (300 cm hysteresis).
    ShadowCullClock += DeltaTime;
    if (ShadowCullClock < .5f || World.GetNetMode() == NM_DedicatedServer) return;
    ShadowCullClock = 0.f;
    const float Cull = CVarHouseShadowCull.GetValueOnGameThread();
    APlayerController* PC = World.GetFirstPlayerController();
    if (!PC || !PC->PlayerCameraManager) return;
    const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
    for (const TWeakObjectPtr<ULightComponent>& W : ShadowLights)
    {
        ULightComponent* L = W.Get();
        if (!L) continue;
        const float Dist = FVector::Dist(Eye, L->GetComponentLocation());
        const bool bWant = Cull <= 0.f || Dist < Cull - 150.f || (L->CastShadows && Dist < Cull + 150.f);
        if (L->CastShadows != bWant) L->SetCastShadows(bWant);
    }
}

void UHouseDetailSubsystem::FixCheckpointLabels(UWorld& World)
{
    // The floating checkpoint text read mirrored from behind; back it with a plate and a second face.
    for (TActorIterator<AStoryCheckpoint> It(&World); It; ++It)
    {
        AStoryCheckpoint* C = *It;
        if (!C->Label || !C->Label->IsVisible() || C->Label->Text.IsEmpty()) continue;
        UTextRenderComponent* Front = C->Label;
        if (Front->Text.ToString() == TEXT("Text")) continue;
        UTextRenderComponent* Back = NewObject<UTextRenderComponent>(C);
        Back->SetupAttachment(Front);
        Back->SetFont(Front->Font);
        Back->SetTextMaterial(Front->TextMaterial);
        Back->SetHorizontalAlignment(Front->HorizontalAlignment);
        Back->SetVerticalAlignment(Front->VerticalAlignment);
        Back->SetWorldSize(Front->WorldSize);
        Back->SetTextRenderColor(Front->TextRenderColor);
        Back->SetText(Front->Text);
        Back->SetRelativeLocationAndRotation(FVector(-1.2f, 0, 0), FRotator(0, 180.f, 0));
        Back->RegisterComponent();
        // Size the plate from the text's own local bounds so it hides the far face completely.
        const FBoxSphereBounds Local = Front->CalcBounds(FTransform::Identity);
        UStaticMeshComponent* Plate = NewObject<UStaticMeshComponent>(C);
        Plate->SetupAttachment(Front);
        if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))) Plate->SetStaticMesh(Cube);
        Plate->SetRelativeLocation(FVector(-.6f, Local.Origin.Y, Local.Origin.Z));
        Plate->SetRelativeScale3D(FVector(.006f, (Local.BoxExtent.Y * 2.f + 12.f) / 100.f, (Local.BoxExtent.Z * 2.f + 8.f) / 100.f));
        Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Plate->SetCastShadow(false);
        if (UHouseBlackHallSubsystem* L = Look()) { L->CreateLook(); HwCreateLook(L); Plate->SetMaterial(0, L->Mat(TEXT("HW_Black"))); }
        Plate->RegisterComponent();
    }
}

void UHouseDetailSubsystem::SpawnKitProps(UWorld& World)
{
    struct FPlace { EHouseKitProp Kind; uint8 Variant; FVector At; float Yaw; float Lift; };
    const FPlace Places[] = {
        // Upstairs rear hall: recliner nook by the hall-end rooms, mats at the doors, a board by the end door.
        { EHouseKitProp::Recliner, 0, FVector(-2088, -955, 300), 180.f, 0.f },
        { EHouseKitProp::GreenChair, 0, FVector(-2066, -858, 300), 180.f, 0.f },
        { EHouseKitProp::DoorMat, 0, FVector(-2195, -1332, 300), 0.f, 0.f },
        { EHouseKitProp::WoodBoard, 0, FVector(-2068, -1290, 300), 95.f, 0.f },
        { EHouseKitProp::DoorMat, 1, FVector(-2268, -443, 300), 90.f, 0.f },
        { EHouseKitProp::DoorMat, 0, FVector(-2096, -348, 300), 90.f, 0.f },
        { EHouseKitProp::DoorMat, 1, FVector(-2268, 637, 300), 90.f, 0.f },
        // Stair landing: bench with the basket and tea jugs, pillows and laundry bags.
        { EHouseKitProp::Bench, 0, FVector(-2284, 1000, 300), 0.f, 0.f },
        { EHouseKitProp::WickerBasket, 0, FVector(-2284, 968, 300), 0.f, 45.6f },
        { EHouseKitProp::TeaJug, 0, FVector(-2278, 1026, 300), 10.f, 45.6f },
        { EHouseKitProp::TeaJug, 1, FVector(-2290, 1042, 300), -25.f, 45.6f },
        { EHouseKitProp::Pillow, 0, FVector(-2262, 1112, 300), 30.f, 0.f },
        { EHouseKitProp::Pillow, 1, FVector(-2236, 1160, 300), -20.f, 0.f },
        { EHouseKitProp::BlueBag, 0, FVector(-2296, 1180, 300), 0.f, 0.f },
        { EHouseKitProp::BlueBag, 0, FVector(-2290, 1236, 300), 40.f, 0.f },
        // Far upstairs hall.
        { EHouseKitProp::DoorMat, 0, FVector(2088, -385, 310), 90.f, 0.f },
        { EHouseKitProp::DoorMat, 1, FVector(1953, -376, 310), 90.f, 0.f },
        { EHouseKitProp::Pillow, 0, FVector(1900, -1300, 310), 15.f, 0.f },
        { EHouseKitProp::BlueBag, 0, FVector(2200, 1100, 310), 0.f, 0.f },
        { EHouseKitProp::WoodBoard, 0, FVector(1800, -1400, 310), 20.f, 0.f },
        // Stairwell mid landings (by the window): a bucket lid, a dropped shirt, a board.
        { EHouseKitProp::BucketLid, 0, FVector(-1448, 1222, 169), 0.f, 0.f },
        { EHouseKitProp::CrumpledShirt, 0, FVector(-1500, 1218, 169), 15.f, 0.f },
        { EHouseKitProp::WoodBoard, 0, FVector(-1436, 1000, 169), 90.f, 0.f },
        { EHouseKitProp::CrumpledShirt, 0, FVector(-1450, 1210, -165), -60.f, 0.f },
        { EHouseKitProp::BlueBag, 0, FVector(-1445, 900, -165), 20.f, 0.f },
        // Ground-floor long hall.
        { EHouseKitProp::DoorMat, 0, FVector(-2287, -1348, 0), 90.f, 0.f },
        { EHouseKitProp::DoorMat, 1, FVector(-2287, -1141, 0), 90.f, 0.f },
        { EHouseKitProp::DoorMat, 0, FVector(-2287, 580, 0), 90.f, 0.f },
        { EHouseKitProp::WoodBoard, 0, FVector(-2100, -1300, 0), 95.f, 0.f },
    };
    FCollisionQueryParams Q(SCENE_QUERY_STAT(HouseKitFloor), false);
    for (const FPlace& P : Places)
    {
        FHitResult Hit;
        const float Scale = HouseWorldScale::Factor(&World);
        FVector At = HouseWorldScale::Point(&World, P.At);
        for (const TWeakObjectPtr<AHouseKitProp>& Spawned : KitProps) if (Spawned.IsValid()) Q.AddIgnoredActor(Spawned.Get());
        if (World.LineTraceSingleByChannel(Hit, At + FVector(0, 0, 120.f * Scale), At - FVector(0, 0, 200.f * Scale), ECC_Visibility, Q)) At.Z = Hit.ImpactPoint.Z;
        At.Z += (P.Lift + .5f) * Scale;
        const FTransform T(FRotator(0, P.Yaw, 0), At, FVector(Scale));
        AHouseKitProp* Prop = World.SpawnActorDeferred<AHouseKitProp>(AHouseKitProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Prop) continue;
        Prop->KitKind = (uint8)P.Kind;
        Prop->KitVariant = P.Variant;
        Prop->ApplyKit();
        Prop->FinishSpawning(T);
        KitProps.Add(Prop);
    }
}

bool UHouseDetailSubsystem::TickHighlight(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World) return true;
    // Checkpoint labels get their text in AStoryCheckpoint::BeginPlay, which runs after the
    // subsystem's OnWorldBeginPlay, so back them on the first engine tick instead.
    if (!bLabelsFixed && World->HasBegunPlay()) { bLabelsFixed = true; FixCheckpointLabels(*World); }
    TickShadowCull(*World, DeltaTime);
    TickFloorCull(*World, DeltaTime);
    AActor* Focus = nullptr;
    if (APlayerController* PC = World->GetFirstPlayerController())
        if (AStoryFirstPersonCharacter* Pawn = Cast<AStoryFirstPersonCharacter>(PC->GetPawn()))
            if (Pawn->FurnitureInteraction && !Pawn->FurnitureInteraction->bHoldingFurniture)
                Focus = Pawn->FurnitureInteraction->FocusedFurnitureForInteraction();
    UPrimitiveComponent* Prim = Focus ? Cast<UPrimitiveComponent>(Focus->GetRootComponent()) : nullptr;
    if (Prim != Highlighted.Get())
    {
        if (UPrimitiveComponent* Old = Highlighted.Get()) if (UMeshComponent* OldMesh = Cast<UMeshComponent>(Old)) OldMesh->SetOverlayMaterial(nullptr);
        Highlighted = Prim;
        HighlightClock = 0.f;
        if (UMeshComponent* NewMesh = Cast<UMeshComponent>(Prim)) if (HighlightMaterial) NewMesh->SetOverlayMaterial(HighlightMaterial);
    }
    if (Prim && HighlightMaterial)
    {
        HighlightClock += DeltaTime;
        const float Pulse = .5f + .5f * FMath::Sin(HighlightClock * 5.f);
        HighlightMaterial->SetVectorParameterValue(TEXT("Emission"), FLinearColor(1.f, .78f, .4f) * (.35f + .45f * Pulse));
        HighlightMaterial->SetScalarParameterValue(TEXT("Opacity"), .12f + .1f * Pulse);
    }
    return true;
}
