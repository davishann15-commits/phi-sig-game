#pragma once

#include "CoreMinimal.h"
#include "HouseBlackHallPaint.h"

class UHouseBlackHallSubsystem;
class UMaterialInterface;
class UTexture2D;

namespace HallSpec
{
    // Interior envelope of the existing hall (cm, world).
    constexpr float XW = -1224.f, XE = 1224.f, YS = -792.f, YN = 792.f, ZF = -329.f, ZC = -44.f;
    constexpr float Cmu = 81.28f;            // texture period: 2 blocks x 4 courses
    inline FVector2D CmuOffset() { return FVector2D(0.f, ZF / Cmu); }
}

/** Worn-paint decal atlas: Grid x Grid cells, each one block tile (Cmu) with its own painted wear over the same block background. */
namespace HallWear
{
    constexpr int32 Grid = 4, Cells = Grid * Grid;
    /** Wear type per cell: 0 scuffs+gouges, 1 heavy chipping, 2 clumps+drips, 3 rubbed thin, 4 tape+holes, 5 water stains, 6 peel, 7 heavy clumps, 8 mixed. */
    constexpr int32 CellTypes[Cells] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 1, 0, 2, 7, 8, 1, 6 };
    /** Which wear types suit a block-tile row (0 = floor course). */
    inline bool RowAllows(int32 Row, int32 Type)
    {
        const int32 Mask = Row == 0 ? 363 : (Row == 1 ? 479 : 406);
        return ((Mask >> Type) & 1) != 0;
    }
}

/** Panel painted over the block wall: Origin is the top-left corner seen from the room; UAx runs to the viewer's right, VAx down. */
struct FHallPanelSpec
{
    FVector Origin = FVector::ZeroVector;
    FVector UAx = FVector(1, 0, 0);
    FVector VAx = FVector(0, 0, -1);
    float WidthCm = 100.f, HeightCm = 100.f, PxPerCm = 4.f;
    /** Paints colour (premultiplied over transparent black, coverage in alpha) in pixel space. */
    TFunction<void(FHallImage& Paint, float PxPerCm)> Painter;
    float Roughness = .5f;
};

class UMaterialInstanceDynamic;
/** House master materials (M_House_Textured / M_House_Plain / M_House_Glass) by short name. */
UMaterialInterface* HallHouseMat(const TCHAR* Name);
/** Dynamic instances registered on the subsystem under Key. */
UMaterialInstanceDynamic* HallMakeTextured(UHouseBlackHallSubsystem* Sub, FName Key, UTexture2D* Base, UTexture2D* Normal, float Rough,
    const FLinearColor& Tint = FLinearColor::White, float NormalStrength = 1.f, float Metal = 0.f, const FLinearColor& Emission = FLinearColor::Black);
UMaterialInstanceDynamic* HallMakePlain(UHouseBlackHallSubsystem* Sub, FName Key, const FLinearColor& Tint, float Rough, float Metal = 0.f,
    const FLinearColor& Emission = FLinearColor::Black);

void HallCreateLook(UHouseBlackHallSubsystem* Sub);
void HallCreateWearAtlas(UHouseBlackHallSubsystem* Sub);
UMaterialInterface* HallMakePanelMaterial(UHouseBlackHallSubsystem* Sub, FName Key, const FHallPanelSpec& Spec);

/** Painted motifs used on the hall walls. */
void HallPaintXXVII(FHallImage& Img, const FVector2D& Center, float CapHeightPx, const FLinearColor& Color, float WidthScale = 1.f);
void HallPaintDog(FHallImage& Img, const FVector2D& Center, float SizePx, const FLinearColor& Body, const FLinearColor& Accent, int32 Pose, bool bMirror);
void HallPaintTEmblem(FHallImage& Img, const FVector2D& Center, float PxPerCm, int32 Variant);
