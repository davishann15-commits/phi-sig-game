#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/** Small CPU canvas used to paint the black hall's textures at runtime (linear colour). */
struct FHallImage
{
    int32 W = 0, H = 0;
    TArray<FLinearColor> Px;

    void Init(int32 InW, int32 InH, const FLinearColor& C) { W = InW; H = InH; Px.Init(C, W * H); }
    FLinearColor& At(int32 X, int32 Y) { return Px[Y * W + X]; }
    const FLinearColor& At(int32 X, int32 Y) const { return Px[Y * W + X]; }
    /** Wrapped bilinear sample, U/V in texture space (1 = full width). */
    FLinearColor Sample(float U, float V) const;
    void Blend(int32 X, int32 Y, const FLinearColor& C, float A)
    {
        if (X < 0 || Y < 0 || X >= W || Y >= H || A <= 0.f) return;
        FLinearColor& D = Px[Y * W + X];
        D = D + (C - D) * FMath::Min(A, 1.f);
    }
    /** Shapes, pixel coordinates (y down), anti-aliased. */
    void FillPolygon(const TArray<FVector2D>& P, const FLinearColor& C, float Alpha = 1.f);
    void Stroke(const TArray<FVector2D>& P, float Width, const FLinearColor& C, float Alpha = 1.f);
    void Ellipse(const FVector2D& Ctr, const FVector2D& Rad, float RotDeg, const FLinearColor& C, float Alpha = 1.f);
    void Rect(const FVector2D& Min, const FVector2D& Max, const FLinearColor& C, float Alpha = 1.f);
};

UTexture2D* HallMakeTexture(const FHallImage& Img, bool bSRGB, bool bNormalMap, const TCHAR* Name, bool bWrap = true);
float HallNoise(float X, float Y, int32 Seed);
float HallFbm(float X, float Y, int32 Octaves, int32 Seed);
/** Height field (W*H) to a tangent-space normal image (R = +u slope, G = +v slope, v grows downward). */
void HallHeightToNormal(const TArray<float>& Height, int32 W, int32 H, float Strength, bool bWrap, FHallImage& Out);

/** Simple stroke font (A-Z, 0-9 and a few marks). Glyph cell 4 x 6 units, cap height = Height. */
struct FHallTextStyle
{
    float Height = 20.f;       // px
    float Stroke = 3.f;        // px
    float Spacing = .25f;      // fraction of height between glyphs
    float WidthScale = 1.f;    // horizontal compression
    float EndBlob = 0.f;       // radius of painted blobs at stroke ends (px), for hand-lettered looks
    float Wobble = 0.f;        // px of hand-painted wobble
    int32 Seed = 1;
};
float HallTextWidth(const FString& Text, const FHallTextStyle& Style);
/** Origin = left end of the baseline (pixel space, y down). Returns the polylines (pixel space) if requested. */
void HallDrawText(FHallImage& Img, const FString& Text, const FVector2D& Origin, const FHallTextStyle& Style, const FLinearColor& C,
    TArray<TArray<FVector2D>>* OutStrokes = nullptr);
/** Glyph polylines in glyph units (x 0..4, y 0..6 up). */
const TArray<TArray<FVector2D>>& HallGlyph(TCHAR Ch);
