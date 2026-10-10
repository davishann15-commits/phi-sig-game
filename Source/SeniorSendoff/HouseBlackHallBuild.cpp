#include "HouseBlackHall.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HouseWorldScale.h"
#include "HouseExitSign.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallLayout.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ProceduralMeshComponent.h"

using namespace HallSpec;
using HallLayout::Casing;
using HallLayout::Liner;
using HallLayout::WallT;
using HallLayout::DoorTop;

namespace
{
struct FHallWallFrame
{
    FVector Face, A, Nrm;
    FVector P(float Along, float D, float Z) const { return FVector(Face.X, Face.Y, 0) + A * Along + Nrm * D + FVector(0, 0, Z); }
};
struct FHallHole { float A0, A1, Top; float Depth = 0.f; };
const int32 HallSkipTopBottom = (1 << 4) | (1 << 5);
}

void AHouseBlackHallDetail::BuildArchitecture()
{
    UWorld* World = GetWorld();
    UHouseBlackHallSubsystem* Sub = World->GetSubsystem<UHouseBlackHallSubsystem>();
    if (!Sub) return;
    FHallMesh Solid, Decor, Props;
    const FHallUV CmuUV = FHallUV::World(Cmu, CmuOffset());
    const FHallUV Plain = FHallUV::World(50.f);

    auto WBox = [&](FHallMesh& M, FName Key, const FHallWallFrame& W, float A0, float A1, float D0, float D1, float Z0, float Z1, const FHallUV& UV, int32 Skip = 0)
    {
        const FVector P0 = W.P(A0, D0, Z0), P1 = W.P(A1, D1, Z1);
        M.BoxMinMax(Key, P0.ComponentMin(P1), P0.ComponentMax(P1), UV, Skip);
    };
    auto FrameUV = [&](float Top)
    {
        FHallUV U = FHallUV::World(Top - ZF + 9.f); U.bTopDown = true; U.TopZ = Top + 9.f;
        return U;
    };

    const FHallWallFrame North{ FVector(0, YN, 0), FVector(1, 0, 0), FVector(0, -1, 0) };
    const FHallWallFrame South{ FVector(0, YS, 0), FVector(1, 0, 0), FVector(0, 1, 0) };
    const FHallWallFrame East{ FVector(XE, 0, 0), FVector(0, 1, 0), FVector(-1, 0, 0) };
    const FHallWallFrame West{ FVector(XW, 0, 0), FVector(0, 1, 0), FVector(1, 0, 0) };
    const FHallWallFrame RecessBack{ FVector(XE + 76.f, 0, 0), FVector(0, 1, 0), FVector(-1, 0, 0) };

    // ---------------------------------------------------------------- wall bodies
    auto WallBody = [&](const FHallWallFrame& W, float A0, float A1, TArray<FHallHole> Holes)
    {
        Holes.Sort([](const FHallHole& L, const FHallHole& R) { return L.A0 < R.A0; });
        float Cursor = A0;
        for (const FHallHole& H : Holes)
        {
            if (H.A0 > Cursor) WBox(Solid, TEXT("CMU"), W, Cursor, H.A0, -WallT, 0, ZF, ZC, CmuUV, HallSkipTopBottom);
            WBox(Solid, TEXT("CMU"), W, H.A0, H.A1, -WallT, 0, H.Top, ZC, CmuUV, 1 << 4);
            if (H.Depth > 0) WBox(Solid, TEXT("CMU"), W, H.A0, H.A1, -WallT, -H.Depth, ZF, H.Top, CmuUV, HallSkipTopBottom);
            Cursor = H.A1;
        }
        if (A1 > Cursor) WBox(Solid, TEXT("CMU"), W, Cursor, A1, -WallT, 0, ZF, ZC, CmuUV, HallSkipTopBottom);
    };
    TArray<FHallHole> NorthHoles;
    for (const HallLayout::FPairSpec& P : HallLayout::Pairs)
        NorthHoles.Add({ P.Center - P.FrameWidth * .5f + Casing, P.Center + P.FrameWidth * .5f - Casing, DoorTop });
    const FHallHole Service{ -522.f, -368.f, -116.f }, Recess{ -230.f, 370.f, -95.f }, Exit{ 432.f, 548.f, DoorTop }, Equip{ 659.f, 753.f, DoorTop, 6.f };
    const FHallHole Library{ 568.f, 732.f, -118.f }, Storage{ 118.f, 306.f, -118.f }, WestDoor{ -506.f, -370.f, -118.f };
    WallBody(North, XW - WallT, XE + WallT, NorthHoles);
    WallBody(South, XW - WallT, XE + WallT, {});
    WallBody(East, YS, YN, { Service, Recess, Exit, Equip });
    WallBody(West, YS, YN, { Library, Storage, WestDoor });

    // Pilasters under the ceiling beams.
    const float PilasterX[4] = { -900.f, -306.f, 306.f, 900.f };
    for (const float C : PilasterX)
    {
        WBox(Solid, TEXT("CMU"), North, C - 60.f, C + 60.f, 0, 36.f, ZF, ZC, CmuUV, HallSkipTopBottom);
        WBox(Solid, TEXT("CMU"), South, C - 60.f, C + 60.f, 0, 36.f, ZF, ZC, CmuUV, HallSkipTopBottom);
    }
    // Bulkhead over patio pairs 2 and 3 (photo 2).
    WBox(Solid, TEXT("CMU"), North, -960.f, 366.f, 0, 36.f, -85.f, ZC, CmuUV, 1 << 4);
    // XXVII recess: back wall, returns and a dropped soffit with two cans.
    WBox(Solid, TEXT("CMU"), RecessBack, Recess.A0 - WallT, Recess.A1 + WallT, -WallT, 0, ZF, ZC, CmuUV, HallSkipTopBottom);
    WBox(Solid, TEXT("CMU"), East, Recess.A0 - WallT, Recess.A0, -76.f, 0, ZF, ZC, CmuUV, HallSkipTopBottom);
    WBox(Solid, TEXT("CMU"), East, Recess.A1, Recess.A1 + WallT, -76.f, 0, ZF, ZC, CmuUV, HallSkipTopBottom);
    WBox(Solid, TEXT("CMU"), East, Recess.A0, Recess.A1, -76.f, -WallT, Recess.Top, ZC, CmuUV, 1 << 4);

    // ---------------------------------------------------------------- floor and stage
    {
        FHallUV FloorUV; FloorUV.Mode = 2; FloorUV.Frame = FTransform(FVector(38.f, 0, ZF - 8.f)); FloorUV.BoxSize = FVector(2524.f, 1584.f, 16.f);
        Solid.BoxMinMax(TEXT("Floor"), FVector(XW, YS, ZF - 16.f), FVector(XE + 76.f, YN, ZF), FloorUV, 47);
        const FHallUV WoodUV = FHallUV::World(300.f);
        Decor.BoxMinMax(TEXT("StageFascia"), FVector(-810.f, YS, ZF), FVector(774.f, -470.f, -299.4f), WoodUV, HallSkipTopBottom);
        Decor.BoxMinMax(TEXT("StageWood"), FVector(-810.f, YS, -299.4f), FVector(774.f, -470.f, -299.f), WoodUV, 1 << 5);
        Decor.BoxMinMax(TEXT("MetalDark"), FVector(-810.f, -471.4f, -301.2f), FVector(774.f, -469.6f, -298.8f), Plain);
    }

    // ---------------------------------------------------------------- frames / casings
    auto Frame = [&](const FHallWallFrame& W, const FHallHole& H, float Depth, bool bExterior, float StopD0, float StopD1)
    {
        const FHallUV U = FrameUV(H.Top);
        WBox(Decor, TEXT("FrameWhite"), W, H.A0 - Casing, H.A0, 0, 2.f, ZF, H.Top + Casing, U);
        WBox(Decor, TEXT("FrameWhite"), W, H.A1, H.A1 + Casing, 0, 2.f, ZF, H.Top + Casing, U);
        WBox(Decor, TEXT("FrameWhite"), W, H.A0 - Casing, H.A1 + Casing, 0, 2.f, H.Top, H.Top + Casing, U);
        WBox(Decor, TEXT("FrameWhite"), W, H.A0, H.A0 + Liner, -Depth, 0, ZF, H.Top, U);
        WBox(Decor, TEXT("FrameWhite"), W, H.A1 - Liner, H.A1, -Depth, 0, ZF, H.Top, U);
        WBox(Decor, TEXT("FrameWhite"), W, H.A0, H.A1, -Depth, 0, H.Top - Liner, H.Top, U);
        if (StopD1 > StopD0)
        {
            WBox(Decor, TEXT("FrameWhite"), W, H.A0 + Liner, H.A0 + Liner + 1.5f, StopD0, StopD1, ZF, H.Top - Liner, U);
            WBox(Decor, TEXT("FrameWhite"), W, H.A1 - Liner - 1.5f, H.A1 - Liner, StopD0, StopD1, ZF, H.Top - Liner, U);
            WBox(Decor, TEXT("FrameWhite"), W, H.A0 + Liner, H.A1 - Liner, StopD0, StopD1, H.Top - Liner - 1.5f, H.Top - Liner, U);
        }
        if (bExterior)
        {
            WBox(Decor, TEXT("FrameWhite"), W, H.A0 - Casing, H.A0, -Depth - 2.f, -Depth, ZF, H.Top + Casing, U);
            WBox(Decor, TEXT("FrameWhite"), W, H.A1, H.A1 + Casing, -Depth - 2.f, -Depth, ZF, H.Top + Casing, U);
            WBox(Decor, TEXT("FrameWhite"), W, H.A0 - Casing, H.A1 + Casing, -Depth - 2.f, -Depth, H.Top, H.Top + Casing, U);
            WBox(Decor, TEXT("Aluminum"), W, H.A0, H.A1, -Depth, 0, ZF, ZF + 1.2f, Plain);
        }
    };
    for (const FHallHole& H : NorthHoles) Frame(North, H, WallT, true, -5.3f, -3.5f);
    Frame(East, Exit, WallT, true, -36.6f, -34.6f);
    Frame(East, Equip, Equip.Depth, false, 0, 0);
    Frame(East, Service, WallT, false, 0, 0);
    Frame(West, Library, WallT, false, 0, 0);
    Frame(West, Storage, WallT, false, 0, 0);
    Frame(West, WestDoor, WallT, false, 0, 0);

    // ---------------------------------------------------------------- baseboards (white vinyl, 10 cm)
    auto BaseSpans = [&](const FHallWallFrame& W, float A0, float A1, TArray<FVector2D> Cuts)
    {
        Cuts.Sort([](const FVector2D& L, const FVector2D& R) { return L.X < R.X; });
        float Cursor = A0;
        for (const FVector2D& C : Cuts)
        {
            if (C.X > Cursor) WBox(Decor, TEXT("Baseboard"), W, Cursor, C.X, 0, 1.2f, ZF, ZF + 10.f, Plain);
            Cursor = FMath::Max(Cursor, C.Y);
        }
        if (A1 > Cursor) WBox(Decor, TEXT("Baseboard"), W, Cursor, A1, 0, 1.2f, ZF, ZF + 10.f, Plain);
    };
    {
        TArray<FVector2D> NCuts, SCuts;
        for (const float C : PilasterX) { NCuts.Add(FVector2D(C - 60.f, C + 60.f)); SCuts.Add(FVector2D(C - 60.f, C + 60.f)); }
        for (const FHallHole& H : NorthHoles) NCuts.Add(FVector2D(H.A0 - Casing, H.A1 + Casing));
        SCuts.Add(FVector2D(-810.f, 774.f));
        BaseSpans(North, XW, XE, NCuts);
        BaseSpans(South, XW, XE, SCuts);
        for (const float C : PilasterX)
            for (const FHallWallFrame* W : { &North, &South })
            {
                WBox(Decor, TEXT("Baseboard"), *W, C - 61.2f, C + 61.2f, 36.f, 37.2f, ZF, ZF + 10.f, Plain);
                WBox(Decor, TEXT("Baseboard"), *W, C - 61.2f, C - 60.f, 0, 36.f, ZF, ZF + 10.f, Plain);
                WBox(Decor, TEXT("Baseboard"), *W, C + 60.f, C + 61.2f, 0, 36.f, ZF, ZF + 10.f, Plain);
            }
        BaseSpans(East, YS, YN, { FVector2D(Service.A0 - Casing, Service.A1 + Casing), FVector2D(Recess.A0, Recess.A1),
            FVector2D(Exit.A0 - Casing, Exit.A1 + Casing), FVector2D(Equip.A0 - Casing, Equip.A1 + Casing) });
        BaseSpans(West, YS, YN, { FVector2D(Library.A0 - Casing, Library.A1 + Casing), FVector2D(Storage.A0 - Casing, Storage.A1 + Casing),
            FVector2D(WestDoor.A0 - Casing, WestDoor.A1 + Casing) });
        BaseSpans(RecessBack, Recess.A0, Recess.A1, {});
        WBox(Decor, TEXT("Baseboard"), East, Recess.A0, Recess.A0 + 1.2f, -76.f, 0, ZF, ZF + 10.f, Plain);
        WBox(Decor, TEXT("Baseboard"), East, Recess.A1 - 1.2f, Recess.A1, -76.f, 0, ZF, ZF + 10.f, Plain);
    }

    // ---------------------------------------------------------------- painted murals and emblems
    auto Panel = [&](FName Key, const FHallPanelSpec& Spec)
    {
        UMaterialInterface* M = HallMakePanelMaterial(Sub, Key, Spec);
        if (!M) return;
        const FVector Nrm = FVector::CrossProduct(Spec.UAx, Spec.VAx).GetSafeNormal();
        const FVector O = Spec.Origin + Nrm * .2f;
        const FVector A = O, B = O + Spec.UAx * Spec.WidthCm, C = B + Spec.VAx * Spec.HeightCm, D = O + Spec.VAx * Spec.HeightCm;
        Decor.QuadUV(Key, A, B, C, D, Nrm, FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1), Spec.UAx);
    };
    const FLinearColor PaintRed(0.52f, 0.018f, 0.014f);
    {
        // XXVII on the recess back wall (photo 1).
        FHallPanelSpec S; S.Origin = FVector(XE + 76.f, -125.f, ZF + 215.f); S.UAx = FVector(0, 1, 0); S.VAx = FVector(0, 0, -1);
        S.WidthCm = 390.f; S.HeightCm = 95.f; S.PxPerCm = 4.f;
        S.Painter = [PaintRed](FHallImage& Img, float Ppc) { HallPaintXXVII(Img, FVector2D(195.f * Ppc, 33.f * Ppc), 54.f * Ppc, PaintRed, 1.12f); };
        Panel(TEXT("PanelXXVII"), S);
    }
    {
        // Preserve the six dancing dogs, with no fraternity lettering above them.
        FHallPanelSpec S; S.Origin = FVector(XW, 95.f, ZF + 250.f); S.UAx = FVector(0, -1, 0); S.VAx = FVector(0, 0, -1);
        S.WidthCm = 430.f; S.HeightCm = 160.f; S.PxPerCm = 3.2f;
        S.Painter = [PaintRed](FHallImage& Img, float Ppc)
        {
            const FLinearColor Red(0.40f, 0.02f, 0.016f), Yellow(0.80f, 0.52f, 0.02f), White(0.78f, 0.78f, 0.74f);
            const FLinearColor Body[6] = { Red, Yellow, White, Red, Yellow, White };
            const FLinearColor Accent[6] = { White, Red, Red, Yellow, Red, Red };
            for (int32 K = 0; K < 6; ++K)
                HallPaintDog(Img, FVector2D((215.f + (K - 2.5f) * 50.f) * Ppc, 108.f * Ppc), 46.f * Ppc, Body[K], Accent[K], K, K % 2 == 1);
        };
        Panel(TEXT("PanelDogs"), S);
    }
    for (int32 K = 0; K < 3; ++K)
    {
        // Red T emblems with white outline on the stage-wall pilasters (photo 6).
        const float C = PilasterX[K];
        FHallPanelSpec S; S.Origin = FVector(C - 60.f, YS + 36.f, ZF + 230.f); S.UAx = FVector(1, 0, 0); S.VAx = FVector(0, 0, -1);
        S.WidthCm = 120.f; S.HeightCm = 110.f; S.PxPerCm = 4.f;
        S.Painter = [K](FHallImage& Img, float Ppc) { HallPaintTEmblem(Img, FVector2D(60.f * Ppc, 55.f * Ppc), Ppc, K); };
        Panel(*FString::Printf(TEXT("PanelT%d"), K), S);
    }

    // ---------------------------------------------------------------- worn paint: chips, scuffs, gouges and paint clumps
    // Tile-aligned decal quads a hair in front of the block faces. Each covers one block tile (2 blocks x 4 courses) and samples a
    // cell of the WallWear atlas, whose background is the same block texture, so the edges vanish into the wall.
    FHallMesh Wear;
    int32 WearCells = 0;
    {
        struct FHallWearFace { FHallWallFrame W; float A0 = 0.f, A1 = 0.f, Z0 = 0.f, Z1 = 0.f; TArray<FBox2D> Cut, Avoid; };
        TArray<FHallWearFace> WearFaces;
        auto AddWearFace = [&WearFaces](const FHallWallFrame& InW, float InA0, float InA1, float InZ0, float InZ1) -> FHallWearFace&
        {
            FHallWearFace& F = WearFaces.AddDefaulted_GetRef();
            F.W = InW; F.A0 = InA0; F.A1 = InA1; F.Z0 = InZ0; F.Z1 = InZ1;
            return F;
        };
        auto WearRect = [](float RA0, float RA1, float RZ0, float RZ1) { return FBox2D(FVector2D(RA0, RZ0), FVector2D(RA1, RZ1)); };
        const float HoleCut = Casing * .5f;                     // cut edges stay hidden behind the casings
        const float BulkA0 = -960.f, BulkA1 = 366.f, BulkZ = -85.f;
        const FHallWallFrame NorthFront{ FVector(0, YN - 36.f, 0), FVector(1, 0, 0), FVector(0, -1, 0) };
        const FHallWallFrame SouthFront{ FVector(0, YS + 36.f, 0), FVector(1, 0, 0), FVector(0, 1, 0) };
        {
            FHallWearFace& F = AddWearFace(North, XW, XE, ZF, ZC);
            for (const float PC : PilasterX) F.Cut.Add(WearRect(PC - 60.f, PC + 60.f, ZF, ZC));
            for (const FHallHole& H : NorthHoles) F.Cut.Add(WearRect(H.A0 - HoleCut, H.A1 + HoleCut, ZF, H.Top + HoleCut));
            F.Cut.Add(WearRect(BulkA0, BulkA1, BulkZ, ZC));
        }
        AddWearFace(NorthFront, BulkA0, BulkA1, BulkZ, ZC);
        {
            FHallWearFace& F = AddWearFace(South, XW, XE, ZF, ZC);
            for (const float PC : PilasterX) F.Cut.Add(WearRect(PC - 60.f, PC + 60.f, ZF, ZC));
            F.Cut.Add(WearRect(-810.f, 774.f, ZF, -299.f));
        }
        for (int32 K = 0; K < 4; ++K)
        {
            const float PC = PilasterX[K];
            const float NTop = (PC - 60.f >= BulkA0 - 1.f && PC + 60.f <= BulkA1 + 1.f) ? BulkZ : ZC;
            AddWearFace(NorthFront, PC - 60.f, PC + 60.f, ZF, NTop);
            FHallWearFace& SF = AddWearFace(SouthFront, PC - 60.f, PC + 60.f, ZF, ZC);
            if (K < 3) SF.Avoid.Add(WearRect(PC - 60.f, PC + 60.f, ZF + 120.f, ZF + 230.f));     // painted T emblem
            for (const float Side : { -1.f, 1.f })
            {
                // Pilaster returns: the corners take the most knocks.
                const FHallWallFrame SideW{ FVector(PC + Side * 60.f, 0, 0), FVector(0, 1, 0), FVector(Side, 0, 0) };
                AddWearFace(SideW, YN - 36.f, YN, ZF, NTop);
                AddWearFace(SideW, YS, YS + 36.f, ZF, ZC);
            }
        }
        {
            FHallWearFace& F = AddWearFace(East, YS, YN, ZF, ZC);
            for (const FHallHole* H : { &Service, &Exit, &Equip }) F.Cut.Add(WearRect(H->A0 - HoleCut, H->A1 + HoleCut, ZF, H->Top + HoleCut));
            F.Cut.Add(WearRect(Recess.A0, Recess.A1, ZF, Recess.Top));
        }
        {
            FHallWearFace& F = AddWearFace(RecessBack, Recess.A0, Recess.A1, ZF, Recess.Top);
            F.Avoid.Add(WearRect(-125.f, 265.f, ZF + 120.f, ZF + 215.f));                          // XXVII
            AddWearFace(FHallWallFrame{ FVector(0, Recess.A0, 0), FVector(1, 0, 0), FVector(0, 1, 0) }, XE, XE + 76.f, ZF, Recess.Top);
            AddWearFace(FHallWallFrame{ FVector(0, Recess.A1, 0), FVector(1, 0, 0), FVector(0, -1, 0) }, XE, XE + 76.f, ZF, Recess.Top);
        }
        {
            FHallWearFace& F = AddWearFace(West, YS, YN, ZF, ZC);
            for (const FHallHole* H : { &Library, &Storage, &WestDoor }) F.Cut.Add(WearRect(H->A0 - HoleCut, H->A1 + HoleCut, ZF, H->Top + HoleCut));
            F.Avoid.Add(WearRect(-335.f, 95.f, ZF + 90.f, ZF + 250.f));                            // PHI SIGMA KAPPA mural
        }

        auto WearOverlap = [](const FBox2D& L, const FBox2D& Q)
        {
            return L.Min.X < Q.Max.X - .01f && Q.Min.X < L.Max.X - .01f && L.Min.Y < Q.Max.Y - .01f && Q.Min.Y < L.Max.Y - .01f;
        };
        FRandomStream WearRng(20261006);
        const float RowChance[4] = { .58f, .42f, .3f, .2f };
        for (const FHallWearFace& F : WearFaces)
        {
            const FVector UAx = FVector::CrossProduct(F.W.Nrm, FVector::UpVector).GetSafeNormal();
            const float Sgn = FVector::DotProduct(F.W.A, UAx);
            const float Off = FVector::DotProduct(FVector(F.W.Face.X, F.W.Face.Y, 0), UAx);
            if (FMath::Abs(Sgn) < .5f) continue;
            auto UDist = [Off, Sgn](float Along) { return Off + Sgn * Along; };
            const float UMin = FMath::Min(UDist(F.A0), UDist(F.A1)), UMax = FMath::Max(UDist(F.A0), UDist(F.A1));
            const int32 IU0 = FMath::FloorToInt(UMin / Cmu), IU1 = FMath::FloorToInt((UMax - .01f) / Cmu);
            int32 Prev = -1;
            for (int32 Row = 0; Row < 4; ++Row)
            {
                const float RZ0 = FMath::Max(F.Z0, ZF + Row * Cmu), RZ1 = FMath::Min(F.Z1, ZF + (Row + 1) * Cmu);
                if (RZ1 - RZ0 < 8.f) continue;
                for (int32 IU = IU0; IU <= IU1; ++IU)
                {
                    const float EA = (IU * Cmu - Off) / Sgn, EB = ((IU + 1) * Cmu - Off) / Sgn;
                    const float CA0 = FMath::Max(F.A0, FMath::Min(EA, EB)), CA1 = FMath::Min(F.A1, FMath::Max(EA, EB));
                    if (CA1 - CA0 < 15.f) continue;
                    if (WearRng.FRand() >= RowChance[Row]) continue;
                    const FBox2D CellBox(FVector2D(CA0, RZ0), FVector2D(CA1, RZ1));
                    bool bAvoid = false;
                    for (const FBox2D& Av : F.Avoid) bAvoid = bAvoid || WearOverlap(CellBox, Av);
                    if (bAvoid) continue;
                    TArray<FBox2D> Pieces;
                    Pieces.Add(CellBox);
                    for (const FBox2D& Cu : F.Cut)
                    {
                        TArray<FBox2D> Kept;
                        for (const FBox2D& Pc : Pieces)
                        {
                            if (!WearOverlap(Pc, Cu)) { Kept.Add(Pc); continue; }
                            if (Cu.Min.X > Pc.Min.X) Kept.Add(FBox2D(Pc.Min, FVector2D(Cu.Min.X, Pc.Max.Y)));
                            if (Cu.Max.X < Pc.Max.X) Kept.Add(FBox2D(FVector2D(Cu.Max.X, Pc.Min.Y), Pc.Max));
                            const float MX0 = FMath::Max(Pc.Min.X, Cu.Min.X), MX1 = FMath::Min(Pc.Max.X, Cu.Max.X);
                            if (Cu.Min.Y > Pc.Min.Y) Kept.Add(FBox2D(FVector2D(MX0, Pc.Min.Y), FVector2D(MX1, Cu.Min.Y)));
                            if (Cu.Max.Y < Pc.Max.Y) Kept.Add(FBox2D(FVector2D(MX0, Cu.Max.Y), FVector2D(MX1, Pc.Max.Y)));
                        }
                        Pieces = MoveTemp(Kept);
                    }
                    Pieces.RemoveAll([](const FBox2D& Pc) { return Pc.Max.X - Pc.Min.X < 1.f || Pc.Max.Y - Pc.Min.Y < 1.f; });
                    float Area = 0.f;
                    for (const FBox2D& Pc : Pieces) Area += (Pc.Max.X - Pc.Min.X) * (Pc.Max.Y - Pc.Min.Y);
                    if (Area < 500.f) continue;
                    int32 Cands[HallWear::Cells];
                    int32 NumCands = 0;
                    for (int32 V = 0; V < HallWear::Cells; ++V)
                        if (V != Prev && HallWear::RowAllows(Row, HallWear::CellTypes[V])) Cands[NumCands++] = V;
                    if (NumCands == 0) continue;
                    const int32 Var = Cands[WearRng.RandRange(0, NumCands - 1)];
                    Prev = Var;
                    const float CellX = float(Var % HallWear::Grid), CellY = float(Var / HallWear::Grid);
                    for (const FBox2D& Pc : Pieces)
                    {
                        const FVector2D Corner[4] = { FVector2D(Pc.Min.X, Pc.Min.Y), FVector2D(Pc.Max.X, Pc.Min.Y), FVector2D(Pc.Max.X, Pc.Max.Y), FVector2D(Pc.Min.X, Pc.Max.Y) };
                        FVector Pos[4];
                        FVector2D Tex[4];
                        for (int32 Q = 0; Q < 4; ++Q)
                        {
                            Pos[Q] = F.W.P(Corner[Q].X, .15f, Corner[Q].Y);
                            const float U01 = UDist(Corner[Q].X) / Cmu - IU;
                            const float V01 = (ZF - Corner[Q].Y) / Cmu + (Row + 1);
                            Tex[Q] = FVector2D((CellX + U01) / HallWear::Grid, (CellY + V01) / HallWear::Grid);
                        }
                        Wear.QuadUV(TEXT("WallWear"), Pos[0], Pos[1], Pos[2], Pos[3], F.W.Nrm, Tex[0], Tex[1], Tex[2], Tex[3], UAx);
                    }
                    ++WearCells;
                }
            }
        }
    }

    // ---------------------------------------------------------------- electrical, fire alarm, signage
    auto SwitchPlate = [&](const FHallWallFrame& W, float A, float Z, int32 Gangs)
    {
        const float HalfW = 3.6f + (Gangs - 1) * 2.3f;
        WBox(Decor, TEXT("PlasticWhite"), W, A - HalfW, A + HalfW, 0, .6f, Z - 5.8f, Z + 5.8f, Plain);
        for (int32 G = 0; G < Gangs; ++G)
        {
            const float Off = (G - (Gangs - 1) * .5f) * 4.6f;
            WBox(Decor, TEXT("PlasticWhite"), W, A + Off - .5f, A + Off + .5f, .6f, 1.4f, Z - 1.2f, Z + 1.2f, Plain);
        }
    };
    auto Outlet = [&](const FHallWallFrame& W, float A, float Z, bool bSurfaceBox)
    {
        const float D = bSurfaceBox ? 4.8f : 0.f;
        if (bSurfaceBox) WBox(Decor, TEXT("Conduit"), W, A - 5.2f, A + 5.2f, 0, D, Z - 6.2f, Z + 6.2f, Plain);
        WBox(Decor, TEXT("PlasticWhite"), W, A - 3.6f, A + 3.6f, D, D + .6f, Z - 5.8f, Z + 5.8f, Plain);
        for (const float Dz : { -2.4f, 2.4f }) WBox(Decor, TEXT("OutletFace"), W, A - 1.8f, A + 1.8f, D + .6f, D + .9f, Z + Dz - 1.6f, Z + Dz + 1.6f, Plain);
    };
    auto Vent = [&](const FHallWallFrame& W, float A, float Z, float Width, float Height)
    {
        FHallUV U; U.Mode = 2; const FVector C = W.P(A, .75f, Z); U.Frame = FTransform(C);
        const FVector Size = (W.A * Width + W.Nrm * 1.5f + FVector(0, 0, Height)).GetAbs();
        U.BoxSize = Size;
        Decor.BoxMinMax(TEXT("Vent"), C - Size * .5f, C + Size * .5f, U);
    };
    // North wall (photo 2).
    WBox(Decor, TEXT("AlarmRed"), North, -425.f, -415.f, 0, 4.6f, ZF + 113.f, ZF + 127.f, Plain);           // pull station
    WBox(Decor, TEXT("PlasticWhite"), North, -423.f, -417.f, 4.6f, 5.6f, ZF + 116.f, ZF + 118.f, Plain);
    WBox(Decor, TEXT("AlarmRed"), North, -426.f, -414.f, 0, 6.f, ZF + 198.f, ZF + 211.f, Plain);           // horn / strobe
    WBox(Decor, TEXT("StrobeLens"), North, -423.f, -417.f, 6.f, 7.f, ZF + 207.f, ZF + 210.f, Plain);
    Vent(North, 1092.f, -71.f, 60.f, 22.f);
    Outlet(North, 1092.f, ZF + 40.f, false);
    Outlet(North, -230.f, ZF + 40.f, false);
    // Conduit along the north wall just above the door heads, jogging around each pilaster.
    {
        const float Z = -93.f, R = 1.1f, Off = 1.2f + R;
        float Cursor = XW;
        auto Run = [&](const FVector& A, const FVector& B)
        {
            Decor.Cylinder(TEXT("Conduit"), A, B, R, 8, Plain, false);
            const float Len = (B - A).Size();
            for (float T = 40.f; T < Len - 20.f; T += 120.f)
            {
                const FVector P = A + (B - A).GetSafeNormal() * T;
                Decor.Box(TEXT("Conduit"), FTransform(FRotationMatrix::MakeFromX((B - A).GetSafeNormal()).Rotator(), P), FVector(1.f, R + .5f, R + .5f), Plain);
            }
        };
        for (const float C : PilasterX)
        {
            const float L = C - 60.f - Off, Rr = C + 60.f + Off;
            Run(FVector(Cursor, YN - Off, Z), FVector(L, YN - Off, Z));
            Run(FVector(L, YN - Off, Z), FVector(L, YN - 36.f - Off, Z));
            Run(FVector(L, YN - 36.f - Off, Z), FVector(Rr, YN - 36.f - Off, Z));
            Run(FVector(Rr, YN - 36.f - Off, Z), FVector(Rr, YN - Off, Z));
            Cursor = Rr;
        }
        Run(FVector(Cursor, YN - Off, Z), FVector(XE, YN - Off, Z));
        WBox(Decor, TEXT("Conduit"), North, -806.f, -794.f, 0, 5.5f, Z - 6.f, Z + 6.f, Plain);                 // junction box
        WBox(Decor, TEXT("PlasticWhite"), North, -805.f, -795.f, 5.5f, 6.f, Z - 5.f, Z + 5.f, Plain);
    }
    // East wall (photo 1).
    SwitchPlate(East, 395.f, ZF + 118.f, 1);
    SwitchPlate(East, 600.f, ZF + 122.f, 2);
    Vent(East, -100.f, -71.f, 60.f, 22.f);
    Outlet(RecessBack, 160.f, ZF + 38.f, true);
    Outlet(RecessBack, 250.f, ZF + 38.f, true);
    Decor.Cylinder(TEXT("Conduit"), RecessBack.P(165.2f, 2.4f, ZF + 38.f), RecessBack.P(244.8f, 2.4f, ZF + 38.f), 1.1f, 8, Plain, false);
    // EXIT sign above the exit door.
    {
        const float SA = (Exit.A0 + Exit.A1) * .5f, SZ = DoorTop + Casing + 17.f;
        if (World->GetAuthGameMode())
        {
            const FVector Center = East.P(SA, 3.1f, SZ);
            const FTransform Transform = HouseWorldScale::Transform(World, FTransform(
                FRotationMatrix::MakeFromXY(FVector::CrossProduct(East.Nrm, FVector::UpVector), East.Nrm).Rotator(), Center));
            if (AHouseExitSign* Sign = World->SpawnActorDeferred<AHouseExitSign>(AHouseExitSign::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
            {
                Sign->PropId = TEXT("Exit_BlackHall"); Sign->Configure(0, false); Sign->FinishSpawning(Transform);
            }
        }
    }
    // Recess cans + their light on XXVII.
    for (const float A : { -40.f, 180.f })
    {
        const FVector C = East.P(A, -40.f, Recess.Top);
        Decor.Cylinder(TEXT("PlasticWhite"), C + FVector(0, 0, -.8f), C, 8.5f, 20, Plain);
        Decor.Cylinder(TEXT("CanGlow"), C + FVector(0, 0, -.9f), C + FVector(0, 0, -.8f), 6.2f, 20, Plain);
        USpotLightComponent* Spot = NewObject<USpotLightComponent>(this);
        Spot->SetupAttachment(RootComponent);
        Spot->SetRelativeLocationAndRotation(C + FVector(0, 0, -3.f), FRotator(-72.f, 0.f, 0.f));
        Spot->SetIntensityUnits(ELightUnits::Lumens);
        Spot->SetIntensity(650.f * FMath::Square(HouseWorldScale::Factor(World)));
        Spot->SetLightColor(FLinearColor(1.f, .86f, .7f));
        Spot->SetAttenuationRadius(420.f * HouseWorldScale::Factor(World));
        Spot->SetInnerConeAngle(18.f);
        Spot->SetOuterConeAngle(42.f);
        Spot->SetCastShadows(true);
        Spot->RegisterComponent();
        Extras.Add(Spot);
    }
    // Open strip fixtures also light the white ceiling in the photos; add soft fill just under each run.
    for (const float X : { -885.f, -291.f, 321.f, 915.f })
        for (const float Y : { -432.f, 432.f })
        {
            UPointLightComponent* Fill = NewObject<UPointLightComponent>(this);
            Fill->SetupAttachment(RootComponent);
            Fill->SetRelativeLocation(FVector(X, Y, -72.f));
            Fill->SetIntensityUnits(ELightUnits::Lumens);
            Fill->SetIntensity(420.f * FMath::Square(HouseWorldScale::Factor(World)));
            Fill->SetLightColor(FLinearColor(.88f, .93f, 1.f));
            Fill->SetAttenuationRadius(560.f * HouseWorldScale::Factor(World));
            Fill->SetSourceLength(120.f * HouseWorldScale::Factor(World));
            Fill->SetCastShadows(false);
            Fill->RegisterComponent();
            Extras.Add(Fill);
        }
    // West wall (photo 3).
    WBox(Decor, TEXT("Thermostat"), West, -626.f, -614.f, 0, 3.f, ZF + 145.f, ZF + 154.f, Plain);
    {
        FHallUV U; U.Mode = 2; const FVector C = West.P(-545.f, .25f, ZF + 165.f); U.Frame = FTransform(C); U.BoxSize = FVector(.5f, 15.f, 15.f);
        Decor.BoxMinMax(TEXT("SignRestroom"), C - U.BoxSize * .5f, C + U.BoxSize * .5f, U);
    }
    SwitchPlate(West, 330.f, ZF + 118.f, 1);
    Outlet(West, -150.f, ZF + 38.f, false);
    // South wall.
    Outlet(South, -1100.f, ZF + 38.f, false);
    Outlet(South, 1000.f, ZF + 38.f, false);

    // Ceiling: smoke detectors and dome cameras.
    for (const FVector2D P : { FVector2D(-600, 250), FVector2D(600, -250), FVector2D(0, 450), FVector2D(-1000, -300), FVector2D(1000, 300) })
        Decor.Cylinder(TEXT("PlasticWhite"), FVector(P.X, P.Y, ZC - 3.6f), FVector(P.X, P.Y, ZC), 6.8f, 20, Plain);
    for (const FVector P : { FVector(-1180, -740, ZC), FVector(1180, 740, ZC), FVector(-306, 0, -80), FVector(306, -500, -80) })
    {
        Decor.Cylinder(TEXT("PlasticWhite"), P + FVector(0, 0, -1.2f), P, 7.2f, 20, Plain);
        Decor.Dome(TEXT("DomeBlack"), P + FVector(0, 0, -1.2f), FVector(0, 0, -1), 5.8f, 5.2f, 16);
    }

    // ---------------------------------------------------------------- floor tape
    auto Tape = [&](FName Key, FVector2D A, FVector2D B, float Width, int32 Seed, float GapChance)
    {
        FRandomStream R(Seed);
        const FVector2D Dir = (B - A).GetSafeNormal(), Side(-Dir.Y, Dir.X);
        const float Len = (B - A).Size();
        float T = 0;
        while (T < Len)
        {
            float Piece = FMath::Min(Len - T, R.FRandRange(40.f, 160.f));
            const bool bGap = R.FRand() < GapChance;
            if (!bGap)
            {
                const FVector2D P0 = A + Dir * T, P1 = A + Dir * (T + Piece);
                const float Z = ZF + .12f + R.FRand() * .03f;
                const float Wob = R.FRandRange(-.4f, .4f);
                const FVector C0(P0 - Side * Width * .5f, Z), C1(P1 - Side * (Width * .5f + Wob), Z), C2(P1 + Side * (Width * .5f - Wob), Z), C3(P0 + Side * Width * .5f, Z);
                Decor.Quad(Key, C0, C1, C2, C3, FVector::UpVector, Plain);
            }
            T += Piece + (bGap ? R.FRandRange(4.f, 25.f) : 0.f);
        }
    };
    // Black "Y" (photos 2 and 4).
    const FVector2D J(120.f, 260.f);
    Tape(TEXT("TapeBlack"), J, FVector2D(-330.f, 560.f), 4.8f, 1, 0.f);
    Tape(TEXT("TapeBlack"), J, FVector2D(600.f, 520.f), 4.8f, 2, 0.f);
    Tape(TEXT("TapeBlack"), J, FVector2D(-120.f, -380.f), 4.8f, 3, 0.f);
    // Blue tape court lines near the stage and west wall (photos 3 and 6), worn in places.
    auto Rect = [&](FVector2D A, FVector2D B, FVector2D C, FVector2D D, int32 Seed)
    {
        Tape(TEXT("TapeBlue"), A, B, 4.8f, Seed, .12f); Tape(TEXT("TapeBlue"), B, C, 4.8f, Seed + 1, .12f);
        Tape(TEXT("TapeBlue"), C, D, 4.8f, Seed + 2, .12f); Tape(TEXT("TapeBlue"), D, A, 4.8f, Seed + 3, .12f);
    };
    Rect(FVector2D(-1150, -420), FVector2D(-700, -440), FVector2D(-690, -90), FVector2D(-1140, -70), 10);
    Rect(FVector2D(-640, -430), FVector2D(-240, -440), FVector2D(-250, -150), FVector2D(-650, -160), 20);
    Tape(TEXT("TapeBlue"), FVector2D(-690, -90), FVector2D(-250, 150), 4.8f, 30, .15f);
    Tape(TEXT("TapeBlue"), FVector2D(-1100, 120), FVector2D(-760, 140), 4.8f, 31, .2f);

    // ---------------------------------------------------------------- props from the photos
    // Folded ping-pong table leaning in the XXVII recess (photos 1 and 4).
    {
        const float Lean = FMath::DegreesToRadians(4.f);
        const FVector Up(FMath::Sin(Lean), 0, FMath::Cos(Lean)), Nn(-FMath::Cos(Lean), 0, FMath::Sin(Lean)), Ay(0, 1, 0);
        const FVector Base(XE + 52.f, 66.f, ZF);
        const FRotator Rot = FRotationMatrix::MakeFromXZ(Nn, Up).Rotator();
        auto TB = [&](FName Key, float N, float Y, float U, float HN, float HY, float HU)
        {
            Props.Box(Key, FTransform(Rot, Base + Nn * N + Ay * Y + Up * U), FVector(HN, HY, HU), Plain);
        };
        TB(TEXT("TableBoard"), 9.f, 0, 92.f, .8f, 76.f, 68.5f);
        TB(TEXT("TableBoard"), -9.f, 0, 92.f, .8f, 76.f, 68.5f);
        TB(TEXT("TableTop"), 7.9f, 0, 92.f, .3f, 75.5f, 68.f);
        TB(TEXT("TableTop"), -7.9f, 0, 92.f, .3f, 75.5f, 68.f);
        TB(TEXT("TableFrame"), 10.8f, 0, 92.f + 66.f, 1.8f, 74.f, 2.f);
        TB(TEXT("TableFrame"), 10.8f, 0, 92.f - 66.f, 1.8f, 74.f, 2.f);
        TB(TEXT("TableFrame"), 10.8f, -72.f, 92.f, 1.8f, 2.f, 66.f);
        TB(TEXT("TableFrame"), 10.8f, 72.f, 92.f, 1.8f, 2.f, 66.f);
        TB(TEXT("TableStrut"), 11.8f, 0, 64.f, 1.2f, 70.f, 1.6f);
        TB(TEXT("TableStrut"), 11.8f, 0, 120.f, 1.2f, 70.f, 1.6f);
        TB(TEXT("TableStrut"), 11.8f, 0, 92.f, 1.2f, 1.6f, 64.f);
        TB(TEXT("LabelWhite"), 11.2f, 40.f, 100.f, .2f, 9.f, 6.f);
        for (const float Y : { -50.f, 50.f })
        {
            Props.Cylinder(TEXT("TableFrame"), Base + Nn * 14.f + Ay * Y + Up * 34.f, Base + Nn * 14.f + Ay * Y + Up * 146.f, 1.6f, 10, Plain);
            TB(TEXT("TableFrame"), 0, Y * 1.2f, 22.f, 11.f, 1.8f, 1.8f);
        }
        Props.Cylinder(TEXT("TableFrame"), Base + Nn * 14.f + Ay * -50.f + Up * 40.f, Base + Nn * 14.f + Ay * 50.f + Up * 40.f, 1.4f, 10, Plain);
        TB(TEXT("TableFrame"), 0, 0, 12.f, 12.f, 60.f, 1.6f);
        for (const float N : { -12.f, 12.f })
            for (const float Y : { -56.f, 56.f })
                Props.Cylinder(TEXT("RubberBlack"), Base + Nn * N + Ay * (Y - 1.6f) + Up * 5.f, Base + Nn * N + Ay * (Y + 1.6f) + Up * 5.f, 5.f, 14, Plain);
    }
    // Boots by the exit door (photo 1).
    for (int32 K = 0; K < 2; ++K)
    {
        const FVector Base(XE - 18.f, 392.f + K * 13.f, ZF);
        const FRotator R(0, 178.f + K * 9.f, 0);
        auto BB = [&](FName Key, float X, float Z, float HX, float HY, float HZ)
        {
            Props.Box(Key, FTransform(R, Base + R.RotateVector(FVector(X, 0, Z))), FVector(HX, HY, HZ), Plain);
        };
        BB(TEXT("RubberBlack"), 0, 1.2f, 13.5f, 5.f, 1.2f);
        BB(TEXT("BootRubber"), 0, 6.5f, 12.8f, 4.8f, 4.2f);
        BB(TEXT("BootTan"), -6.f, 18.f, 6.5f, 4.6f, 8.5f);
        BB(TEXT("BootTan"), 3.f, 11.5f, 5.f, 4.4f, 1.5f);
    }
    // Orange bucket and scrub-brush pole by patio pair 2 (photo 2).
    {
        const FVector B(180.f, YN - 21.f, ZF);
        Props.Cylinder(TEXT("BucketOrange"), B, B + FVector(0, 0, 36.f), 14.5f, 24, Plain, false);
        Props.Cylinder(TEXT("BucketOrange"), B, B + FVector(0, 0, .5f), 13.5f, 24, Plain);
        Props.Cylinder(TEXT("BucketOrange"), B + FVector(0, 0, 34.f), B + FVector(0, 0, 36.5f), 15.2f, 24, Plain, false);
        Props.Cylinder(TEXT("BucketInside"), B + FVector(0, 0, 33.f), B + FVector(0, 0, 33.2f), 14.f, 24, Plain);
        for (const float Z : { 9.f, 22.f }) Props.Cylinder(TEXT("BucketOrange"), B + FVector(0, 0, Z), B + FVector(0, 0, Z + 1.f), 14.9f, 24, Plain, false);
        FVector Prev = B + FVector(-15.f, 0, 34.f);
        for (int32 K = 1; K <= 8; ++K)
        {
            const float A = PI * K / 8.f;
            const FVector P = B + FVector(-15.f * FMath::Cos(A), -4.f * FMath::Sin(A), 34.f + 12.f * FMath::Sin(A));
            Props.Cylinder(TEXT("MetalDark"), Prev, P, .35f, 6, Plain, false);
            Prev = P;
        }
        const FVector Foot(392.f, YN - 92.f, ZF), Top(334.f, YN - 37.f, ZF + 152.f);
        const FVector Ax = (Top - Foot).GetSafeNormal();
        const float Len = (Top - Foot).Size();
        for (float T = 6.f; T < Len; T += 30.f)
        {
            Props.Cylinder(TEXT("PoleYellow"), Foot + Ax * T, Foot + Ax * FMath::Min(Len, T + 25.f), 1.4f, 10, Plain);
            if (T + 25.f < Len) Props.Cylinder(TEXT("RubberBlack"), Foot + Ax * (T + 25.f), Foot + Ax * FMath::Min(Len, T + 30.f), 1.5f, 10, Plain);
        }
        const FRotator HeadRot(0, -40.f, 0);
        Props.Box(TEXT("RubberBlack"), FTransform(HeadRot, Foot + FVector(0, 0, 5.f)), FVector(13.f, 4.f, 2.4f), Plain);
        Props.Box(TEXT("Bristle"), FTransform(HeadRot, Foot + FVector(0, 0, 1.4f)), FVector(12.5f, 3.6f, 1.4f), Plain);
    }
    // TV on a rolling stand, jugs and extension poles on the stage (photo 6).
    {
        const float SZ = -299.f;
        const FVector C(-600.f, -700.f, SZ);
        for (const float X : { -32.f, 32.f })
        {
            Props.BoxMinMax(TEXT("BezelBlack"), C + FVector(X - 3.f, -32.f, 7.f), C + FVector(X + 3.f, 32.f, 12.f), Plain);
            Props.BoxMinMax(TEXT("BezelBlack"), C + FVector(X - 2.5f, -3.f, 12.f), C + FVector(X + 2.5f, 3.f, 150.f), Plain);
            for (const float Y : { -29.f, 29.f })
                Props.Cylinder(TEXT("RubberBlack"), C + FVector(X - 1.5f, Y, 4.f), C + FVector(X + 1.5f, Y, 4.f), 3.8f, 12, Plain);
        }
        Props.BoxMinMax(TEXT("BezelBlack"), C + FVector(-36.f, -2.f, 60.f), C + FVector(36.f, 2.f, 64.f), Plain);
        Props.BoxMinMax(TEXT("BezelBlack"), C + FVector(-73.f, 3.f, 86.f), C + FVector(73.f, 9.f, 170.f), Plain);
        Props.BoxMinMax(TEXT("ScreenBlack"), C + FVector(-71.5f, 9.f, 87.5f), C + FVector(71.5f, 9.2f, 168.5f), Plain);
        for (int32 K = 0; K < 3; ++K)
        {
            const FVector J0(235.f + K * 18.f, -735.f + (K % 2) * 6.f, SZ);
            Props.BoxMinMax(TEXT("JugBlue"), J0 + FVector(-7.5f, -4.8f, 0), J0 + FVector(7.5f, 4.8f, 24.f), Plain);
            Props.Cylinder(TEXT("JugBlue"), J0 + FVector(3.f, 0, 24.f), J0 + FVector(3.f, 0, 28.f), 2.2f, 10, Plain);
            Props.Cylinder(TEXT("CapWhite"), J0 + FVector(3.f, 0, 28.f), J0 + FVector(3.f, 0, 30.5f), 2.4f, 10, Plain);
            Props.BoxMinMax(TEXT("JugBlue"), J0 + FVector(-6.f, -1.5f, 18.f), J0 + FVector(-1.f, 1.5f, 27.f), Plain);
        }
        auto Pole = [&](const FVector& Foot, const FVector& Top)
        {
            const FVector Ax = (Top - Foot).GetSafeNormal();
            const float Len = (Top - Foot).Size();
            Props.Cylinder(TEXT("PoleWhite"), Foot + Ax * 6.f, Top - Ax * 30.f, 1.3f, 10, Plain);
            Props.Cylinder(TEXT("GripBlue"), Top - Ax * 30.f, Top, 1.6f, 10, Plain);
            Props.Cylinder(TEXT("GripBlue"), Foot + Ax * (Len * .45f), Foot + Ax * (Len * .45f + 12.f), 1.6f, 10, Plain);
            Props.Box(TEXT("RubberBlack"), FTransform(FRotator(0, 10.f, 0), Foot + FVector(0, 0, 3.f)), FVector(2.f, 16.f, 3.f), Plain);
        };
        Pole(FVector(180.f, -712.f, SZ), FVector(196.f, YS + 3.f, SZ + 212.f));
        Pole(FVector(410.f, -705.f, SZ), FVector(372.f, YS + 39.f, SZ + 206.f));
    }

    // ---------------------------------------------------------------- outside: landing slabs and invisible bounds (no exterior scenery)
    const FHallUV ConcUV = FHallUV::World(200.f);
    Solid.BoxMinMax(TEXT("Concrete"), FVector(XW, YN + WallT, ZF - 16.f), FVector(XE, YN + 300.f, ZF), ConcUV, 0);
    // The saved east stairwell owns this landing and its perimeter when present.
    // Keep the old dead-end bounds for maps without the extension.
    bool bHasEastStairwell = false;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("NoodleWingStairwell"))) { bHasEastStairwell = true; break; }
    if (!bHasEastStairwell)
        Solid.BoxMinMax(TEXT("Concrete"), FVector(XE + WallT, Exit.A0 - 30.f, ZF - 16.f), FVector(XE + 240.f, Exit.A1 + 30.f, ZF), ConcUV, 0);
    auto Blocker = [&](const FVector& Min, const FVector& Max)
    {
        UBoxComponent* Box = NewObject<UBoxComponent>(this);
        Box->SetupAttachment(RootComponent);
        Box->SetMobility(EComponentMobility::Static);
        Box->SetBoxExtent((Max - Min) * .5f);
        Box->SetRelativeLocation((Min + Max) * .5f);
        Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        Box->SetHiddenInGame(true);
        Box->RegisterComponent();
        Extras.Add(Box);
    };
    // The patio outside the French pairs is real ground once the house exterior is built (AHouseExteriorDetail); these walls
    // used to stand 1.2 m out and rose through the back deck above, so they only bound the landing when the exterior is off.
    if (FParse::Param(FCommandLine::Get(), TEXT("NoHouseExterior")))
    {
        Blocker(FVector(XW - 40.f, YN + 300.f, ZF), FVector(XE + 40.f, YN + 330.f, ZC + 400.f));
        Blocker(FVector(XW - 40.f, YN + WallT, ZF), FVector(XW, YN + 330.f, ZC + 400.f));
        Blocker(FVector(XE, YN + WallT, ZF), FVector(XE + 40.f, YN + 330.f, ZC + 400.f));
    }
    if (!bHasEastStairwell)
    {
        Blocker(FVector(XE + 240.f, Exit.A0 - 60.f, ZF), FVector(XE + 270.f, Exit.A1 + 60.f, ZC + 400.f));
        Blocker(FVector(XE + WallT, Exit.A0 - 60.f, ZF), FVector(XE + 270.f, Exit.A0 - 30.f, ZC + 400.f));
        Blocker(FVector(XE + WallT, Exit.A1 + 30.f, ZF), FVector(XE + 270.f, Exit.A1 + 60.f, ZC + 400.f));
    }

    // ---------------------------------------------------------------- build components
    auto Make = [&](const FHallMesh& M, const TCHAR* Name, bool bCollision)
    {
        UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, Name);
        PM->SetupAttachment(RootComponent);
        PM->SetMobility(EComponentMobility::Static);
        PM->bUseComplexAsSimpleCollision = true;
        PM->bUseAsyncCooking = false;
        if (bCollision) PM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        else PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PM->RegisterComponent();
        M.Build(PM, [Sub](FName Key) { return Sub->Mat(Key); }, bCollision);
        Meshes.Add(PM);
        BuiltTriangles += M.NumTriangles();
    };
    Make(Solid, TEXT("HallSolid"), true);
    Make(Decor, TEXT("HallDecor"), false);
    Make(Props, TEXT("HallProps"), true);
    Make(Wear, TEXT("HallWear"), false);
    if (Meshes.Num() > 0 && Meshes.Last()) Meshes.Last()->SetCastShadow(false);
    Notes += FString::Printf(TEXT(" wear=%d"), WearCells);
}
