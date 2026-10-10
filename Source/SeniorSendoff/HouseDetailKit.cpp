// Hallway kit look: painted signs, prints, labels and plain materials shared by the fixtures and kit furniture.
#include "HouseDetail.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
uint32 HwHash(int32 X, int32 Y, int32 S)
{
    uint32 H = (uint32)X * 73856093u ^ (uint32)Y * 19349663u ^ (uint32)S * 83492791u;
    H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
    return H;
}
float HwRand(int32 X, int32 Y, int32 S) { return (HwHash(X, Y, S) & 0xFFFFFF) / float(0xFFFFFF); }
float HwNoise(float X, float Y, int32 Seed)
{
    const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
    float FX = X - X0, FY = Y - Y0;
    FX = FX * FX * (3 - 2 * FX); FY = FY * FY * (3 - 2 * FY);
    return FMath::Lerp(FMath::Lerp(HwRand(X0, Y0, Seed), HwRand(X0 + 1, Y0, Seed), FX), FMath::Lerp(HwRand(X0, Y0 + 1, Seed), HwRand(X0 + 1, Y0 + 1, Seed), FX), FY);
}
float HwFbm(float X, float Y, int32 Oct, int32 Seed)
{
    float Sum = 0, Amp = .5f, Norm = 0;
    for (int32 O = 0; O < Oct; ++O) { Sum += HwNoise(X, Y, Seed + O * 31) * Amp; Norm += Amp; X *= 2.03f; Y *= 1.97f; Amp *= .5f; }
    return Sum / Norm;
}
FLinearColor HwMul(const FLinearColor& C, float F) { return FLinearColor(C.R * F, C.G * F, C.B * F, C.A); }

/** Simple block capitals, centred at (Cx, Baseline) with cap height H. */
void HwText(FHallImage& Img, const FString& Text, float Cx, float Baseline, float H, float Stroke, const FLinearColor& C, float WidthScale = .9f)
{
    FHallTextStyle T; T.Height = H; T.Stroke = Stroke; T.WidthScale = WidthScale; T.Spacing = .22f;
    const float W = HallTextWidth(Text, T);
    HallDrawText(Img, Text, FVector2D(Cx - W * .5f, Baseline), T, C);
}

UTexture2D* HwTex(UHouseBlackHallSubsystem* Sub, const FHallImage& Img, const TCHAR* Name, bool bWrap = false)
{
    UTexture2D* T = HallMakeTexture(Img, true, false, Name, bWrap);
    if (T) Sub->Textures.Add(FName(Name), T);
    return T;
}
UTexture2D* HwNormalTex(UHouseBlackHallSubsystem* Sub, const TArray<float>& Height, int32 W, int32 H, float Strength, const TCHAR* Name, bool bWrap = false)
{
    FHallImage N; HallHeightToNormal(Height, W, H, Strength, bWrap, N);
    UTexture2D* T = HallMakeTexture(N, false, true, Name, bWrap);
    if (T) Sub->Textures.Add(FName(Name), T);
    return T;
}

// ------------------------------------------------------------------ signs and labels
void HwPaintFireFace(FHallImage& Img)
{
    // Pull station front: red with a white "FIRE" band and arrows.
    Img.Init(128, 128, FLinearColor(.55f, .02f, .012f));
    Img.Rect(FVector2D(10, 10), FVector2D(118, 118), FLinearColor(.5f, .018f, .01f));
    Img.Rect(FVector2D(14, 14), FVector2D(114, 114), FLinearColor(.58f, .024f, .014f));
    HwText(Img, TEXT("FIRE"), 64, 52, 22, 4.5f, FLinearColor(.92f, .92f, .9f), .95f);
    HwText(Img, TEXT("PULL DOWN"), 64, 108, 9, 2.f, FLinearColor(.92f, .92f, .9f), .9f);
    // Pull bar recess.
    Img.Rect(FVector2D(26, 62), FVector2D(102, 86), FLinearColor(.3f, .012f, .008f));
    Img.Rect(FVector2D(29, 65), FVector2D(99, 83), FLinearColor(.88f, .88f, .86f));
    Img.Rect(FVector2D(29, 79), FVector2D(99, 83), FLinearColor(.6f, .6f, .58f));
}
void HwPaintHornFace(FHallImage& Img)
{
    Img.Init(128, 128, FLinearColor(.55f, .02f, .012f));
    // Strobe lens (white) at the top and a horn grille below.
    Img.Rect(FVector2D(18, 12), FVector2D(110, 50), FLinearColor(.86f, .87f, .9f));
    Img.Rect(FVector2D(22, 16), FVector2D(106, 46), FLinearColor(.95f, .95f, .97f));
    for (int32 Y = 62; Y < 112; Y += 8) Img.Rect(FVector2D(26, Y), FVector2D(102, Y + 3), FLinearColor(.25f, .01f, .006f));
    HwText(Img, TEXT("FIRE"), 64, 122, 9, 2.2f, FLinearColor(.92f, .92f, .9f));
}
void HwPaintExitFace(FHallImage& Img, int32 Arrow)
{
    // White box with red letters; Arrow 0 none, -1 left, +1 right.
    Img.Init(256, 128, FLinearColor(.86f, .86f, .84f));
    Img.Rect(FVector2D(4, 4), FVector2D(252, 124), FLinearColor(.9f, .9f, .88f));
    const FLinearColor Red(.9f, .03f, .012f);
    const float Shift = Arrow < 0 ? 18.f : (Arrow > 0 ? -18.f : 0.f);
    HwText(Img, TEXT("EXIT"), 128 + Shift, 94, 60, 11.f, Red, .92f);
    if (Arrow != 0)
    {
        const float X = Arrow < 0 ? 26.f : 230.f, D = Arrow < 0 ? 1.f : -1.f;
        Img.FillPolygon({ FVector2D(X, 64), FVector2D(X + D * 22, 42), FVector2D(X + D * 22, 54), FVector2D(X + D * 40, 54), FVector2D(X + D * 40, 74), FVector2D(X + D * 22, 74), FVector2D(X + D * 22, 86) }, Red);
    }
}
void HwPaintPlaqueLabel(FHallImage& Img, int32 Seed)
{
    // Black engraved name plate: two faint lines of lettering.
    Img.Init(256, 80, FLinearColor(.012f, .012f, .013f));
    FRandomStream R(Seed);
    const FLinearColor Ink(.17f, .16f, .14f);
    const TCHAR* Names[] = { TEXT("D GRANTHAM"), TEXT("T OSBORNE"), TEXT("J WHITLOCK"), TEXT("M PRUITT"), TEXT("C DELANEY"), TEXT("R HAVILAND"), TEXT("B SORRELL"), TEXT("K FARRIS") };
    const TCHAR* Lines2[] = { TEXT("HOUSE MANAGER"), TEXT("FALL 2026"), TEXT("ROOM CAPTAIN"), TEXT("SENIOR"), TEXT("PLEDGE ED"), TEXT("SOCIAL CHAIR") };
    HwText(Img, Names[R.RandRange(0, 7)], 128, 38, 18, 2.6f, Ink, .85f);
    HwText(Img, Lines2[R.RandRange(0, 5)], 128, 66, 13, 2.f, HwMul(Ink, .8f), .85f);
}
void HwPaintThermostat(FHallImage& Img)
{
    Img.Init(128, 96, FLinearColor(.86f, .86f, .83f));
    Img.Rect(FVector2D(14, 14), FVector2D(84, 60), FLinearColor(.45f, .52f, .4f));
    Img.Rect(FVector2D(17, 17), FVector2D(81, 57), FLinearColor(.58f, .66f, .5f));
    HwText(Img, TEXT("68"), 44, 48, 24, 4.f, FLinearColor(.12f, .14f, .1f), .8f);
    HwText(Img, TEXT("HEAT"), 60, 26, 7, 1.5f, FLinearColor(.2f, .22f, .18f));
    Img.Ellipse(FVector2D(100, 30), FVector2D(8, 8), 0, FLinearColor(.72f, .72f, .7f));
    Img.Ellipse(FVector2D(100, 56), FVector2D(8, 8), 0, FLinearColor(.72f, .72f, .7f));
    Img.Rect(FVector2D(20, 70), FVector2D(108, 84), FLinearColor(.78f, .78f, .75f));
}
void HwPaintPanelFace(FHallImage& Img, TArray<float>& Height)
{
    // Breaker panel door: brushed grey, seam lines and a label.
    const int32 W = 192, H = 320;
    Img.Init(W, H, FLinearColor(.5f, .52f, .53f));
    Height.SetNumZeroed(W * H);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float Brush = HwNoise(X * .07f, Y * 2.1f, 11) * .08f - .04f;
            Img.At(X, Y) = HwMul(Img.At(X, Y), 1.f + Brush);
            const bool bDoor = X > 14 && X < W - 14 && Y > 18 && Y < H - 18;
            Height[Y * W + X] = bDoor ? 1.f : 0.f;
        }
    Img.Rect(FVector2D(13, 17), FVector2D(W - 13, H - 17), FLinearColor(.22f, .23f, .24f));
    Img.Rect(FVector2D(16, 20), FVector2D(W - 16, H - 20), FLinearColor(.52f, .54f, .55f));
    // Latch and label.
    Img.Rect(FVector2D(W - 36, H * .5f - 10), FVector2D(W - 22, H * .5f + 10), FLinearColor(.3f, .31f, .32f));
    Img.Rect(FVector2D(40, 34), FVector2D(W - 40, 62), FLinearColor(.8f, .8f, .76f));
    HwText(Img, TEXT("MAIN 200A"), W * .5f, 55, 13, 2.2f, FLinearColor(.1f, .1f, .1f));
    Img.Rect(FVector2D(60, H - 60), FVector2D(W - 60, H - 40), FLinearColor(.85f, .15f, .1f));
    HwText(Img, TEXT("DANGER"), W * .5f, H - 45, 11, 2.f, FLinearColor(.95f, .95f, .9f));
}
void HwPaintCabinetStrip(FHallImage& Img)
{
    Img.Init(256, 32, FLinearColor(.86f, .86f, .84f));
    HwText(Img, TEXT("FIRE EXTINGUISHER"), 128, 24, 15, 2.8f, FLinearColor(.78f, .05f, .02f), .82f);
}
void HwPaintExtinguisherLabel(FHallImage& Img)
{
    Img.Init(64, 128, FLinearColor(.9f, .9f, .86f));
    Img.Rect(FVector2D(4, 4), FVector2D(60, 30), FLinearColor(.78f, .06f, .03f));
    HwText(Img, TEXT("ABC"), 32, 24, 12, 2.4f, FLinearColor(.95f, .95f, .9f));
    for (int32 Y = 40; Y < 120; Y += 9) Img.Rect(FVector2D(8, Y), FVector2D(56, Y + 3), FLinearColor(.25f, .25f, .25f));
    Img.Rect(FVector2D(8, 96), FVector2D(56, 116), FLinearColor(.1f, .1f, .1f));
}
void HwPaintLevelSign(FHallImage& Img, int32 Level)
{
    Img.Init(256, 384, FLinearColor(.9f, .9f, .88f));
    Img.Rect(FVector2D(0, 0), FVector2D(256, 384), FLinearColor(.05f, .05f, .05f));
    Img.Rect(FVector2D(10, 10), FVector2D(246, 374), FLinearColor(.92f, .92f, .9f));
    Img.Rect(FVector2D(10, 10), FVector2D(246, 60), FLinearColor(.05f, .05f, .05f));
    HwText(Img, TEXT("LEVEL"), 128, 50, 32, 6.f, FLinearColor(.95f, .95f, .93f), .8f);
    HwText(Img, FString::FromInt(Level), 128, 330, 230, 40.f, FLinearColor(.05f, .05f, .05f), .95f);
}
void HwPaintRegister(FHallImage& Img, TArray<float>& Height)
{
    // Floor-level wall register: frame and vertical louvers.
    const int32 W = 256, H = 128;
    Img.Init(W, H, FLinearColor(.78f, .78f, .76f));
    Height.SetNumZeroed(W * H);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const bool bFrame = X < 14 || X > W - 15 || Y < 14 || Y > H - 15;
            const float L = FMath::Fmod((float)X, 12.f) / 12.f;
            const float Louver = bFrame ? 1.f : (L < .7f ? 1.f - L * .8f : .05f);
            Img.At(X, Y) = bFrame ? FLinearColor(.8f, .8f, .78f) : HwMul(FLinearColor(.74f, .74f, .72f), .2f + .8f * Louver);
            Height[Y * W + X] = bFrame ? 1.f : (L < .7f ? L : 0.f);
        }
}
void HwPaintMat(FHallImage& Img, TArray<float>& Height, bool bGrey)
{
    const int32 W = 256, H = 256;
    Img.Init(W, H, bGrey ? FLinearColor(.17f, .17f, .18f) : FLinearColor(.02f, .02f, .02f));
    Height.SetNumZeroed(W * H);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            if (bGrey)
            {
                const float F = HwFbm(X * .12f, Y * .12f, 3, 5);
                Img.At(X, Y) = HwMul(FLinearColor(.19f, .19f, .2f), .8f + .4f * F);
                Height[Y * W + X] = F * .4f;
            }
            else
            {
                // Diamond tread.
                const float U = FMath::Fmod((X + Y) / 24.f, 1.f), V = FMath::Fmod((X - Y + 1024) / 24.f, 1.f);
                const float D = FMath::Max(FMath::Abs(U - .5f), FMath::Abs(V - .5f));
                const float Bump = D < .32f ? 1.f : 0.f;
                Img.At(X, Y) = HwMul(FLinearColor(.025f, .025f, .025f), .7f + .5f * Bump);
                Height[Y * W + X] = Bump;
            }
        }
    const float Border = 12.f;
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
            if (X < Border || Y < Border || X >= W - Border || Y >= H - Border) { Img.At(X, Y) = bGrey ? FLinearColor(.12f, .12f, .13f) : FLinearColor(.03f, .03f, .03f); Height[Y * W + X] = .6f; }
}
void HwPaintWicker(FHallImage& Img, TArray<float>& Height)
{
    const int32 W = 256, H = 256;
    Img.Init(W, H, FLinearColor(.35f, .22f, .1f));
    Height.SetNumZeroed(W * H);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            // Over-under weave: horizontal strands 16 px tall, vertical uprights 24 px apart.
            const int32 Row = Y / 16, Col = X / 24;
            const float RowF = FMath::Fmod((float)Y, 16.f) / 16.f, ColF = FMath::Fmod((float)X, 24.f) / 24.f;
            const bool bOver = ((Row + Col) & 1) == 0;
            const float Strand = FMath::Sin(RowF * PI);
            const float Upright = ColF < .25f ? FMath::Sin(ColF * 4.f * PI) : 0.f;
            float Hh = bOver ? Strand * .9f : Upright * .9f;
            if (!bOver && ColF >= .25f) Hh = Strand * .55f;
            const float Tone = .75f + .35f * Hh + .12f * (HwRand(Row, Col, 3) - .5f);
            Img.At(X, Y) = HwMul(FLinearColor(.4f, .25f, .11f), Tone);
            Height[Y * W + X] = Hh;
        }
}
void HwPaintWood(FHallImage& Img, TArray<float>& Height, const FLinearColor& Base, int32 Seed)
{
    const int32 W = 256, H = 256;
    Img.Init(W, H, Base);
    Height.SetNumZeroed(W * H);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float Grain = HwFbm(X * .35f, Y * .02f, 4, Seed);
            const float Ring = FMath::Sin((X * .05f + Grain * 6.f)) * .5f + .5f;
            Img.At(X, Y) = HwMul(Base, .72f + .45f * Ring * Grain + .1f * Grain);
            Height[Y * W + X] = Ring * .5f;
        }
}
void HwPaintLeather(FHallImage& Img, TArray<float>& Height)
{
    const int32 W = 256, H = 256;
    Img.Init(W, H, FLinearColor(.028f, .027f, .028f));
    Height.SetNumZeroed(W * H);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float Pebble = HwFbm(X * .5f, Y * .5f, 3, 21);
            Img.At(X, Y) = HwMul(FLinearColor(.045f, .044f, .046f), .8f + .5f * Pebble);
            Height[Y * W + X] = Pebble * .35f;
        }
}

// ------------------------------------------------------------------ framed prints (original compositions)
void HwPaintDalmatian(FHallImage& Img)
{
    const int32 W = 384, H = 448;
    Img.Init(W, H, FLinearColor(.1f, .07f, .05f));
    // Dark parlour: panelled wall, brick hearth glow on the right.
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float V = Y / float(H);
            FLinearColor C = FMath::Lerp(FLinearColor(.12f, .09f, .06f), FLinearColor(.05f, .035f, .025f), V);
            if (X > W * .68f && Y < H * .62f) C = FMath::Lerp(C, FLinearColor(.32f, .14f, .06f), .5f + .5f * HwNoise(X * .08f, Y * .08f, 4));
            Img.At(X, Y) = C;
        }
    Img.Rect(FVector2D(W * .68f, H * .18f), FVector2D(W * .92f, H * .6f), FLinearColor(.25f, .1f, .05f));
    Img.Rect(FVector2D(W * .72f, H * .24f), FVector2D(W * .88f, H * .58f), FLinearColor(.55f, .3f, .08f));   // fire glow
    Img.Ellipse(FVector2D(W * .8f, H * .48f), FVector2D(W * .07f, H * .09f), 0, FLinearColor(.95f, .55f, .15f));
    // Red rug.
    Img.FillPolygon({ FVector2D(W * .02f, H * .66f), FVector2D(W * .98f, H * .64f), FVector2D(W * .98f, H * .98f), FVector2D(W * .02f, H * .98f) }, FLinearColor(.42f, .06f, .05f));
    for (int32 K = 0; K < 6; ++K) Img.Rect(FVector2D(W * .02f, H * (.7f + K * .05f)), FVector2D(W * .98f, H * (.7f + K * .05f) + 2), FLinearColor(.3f, .05f, .04f), .5f);
    // Mother dog lying down (white body with black spots).
    const FLinearColor Coat(.9f, .88f, .84f), Spot(.06f, .06f, .06f);
    Img.Ellipse(FVector2D(W * .42f, H * .76f), FVector2D(W * .3f, H * .11f), -6, Coat);
    Img.Ellipse(FVector2D(W * .2f, H * .66f), FVector2D(W * .09f, H * .07f), -18, Coat);      // chest/neck
    Img.Ellipse(FVector2D(W * .13f, H * .58f), FVector2D(W * .075f, H * .06f), 0, Coat);       // head
    Img.FillPolygon({ FVector2D(W * .07f, H * .56f), FVector2D(W * .02f, H * .6f), FVector2D(W * .06f, H * .64f), FVector2D(W * .1f, H * .61f) }, Coat); // muzzle
    Img.Ellipse(FVector2D(W * .19f, H * .55f), FVector2D(W * .03f, H * .045f), 20, Spot);       // ear
    Img.Ellipse(FVector2D(W * .12f, H * .575f), FVector2D(3, 3), 0, Spot);                      // eye
    Img.Ellipse(FVector2D(W * .035f, H * .605f), FVector2D(4, 3), 0, Spot);                     // nose
    FRandomStream R(77);
    for (int32 K = 0; K < 60; ++K)
    {
        const FVector2D P(W * R.FRandRange(.14f, .7f), H * R.FRandRange(.67f, .85f));
        if ((P.X - W * .42f) * (P.X - W * .42f) / (W * .3f * W * .3f) + (P.Y - H * .76f) * (P.Y - H * .76f) / (H * .11f * H * .11f) > .85f) continue;
        Img.Ellipse(P, FVector2D(R.FRandRange(3.f, 9.f), R.FRandRange(3.f, 8.f)), R.FRandRange(0.f, 180.f), Spot);
    }
    for (int32 K = 0; K < 8; ++K) Img.Ellipse(FVector2D(W * R.FRandRange(.15f, .25f), H * R.FRandRange(.6f, .7f)), FVector2D(R.FRandRange(2.f, 5.f)), 0, Spot);
    // Front legs.
    Img.Ellipse(FVector2D(W * .3f, H * .86f), FVector2D(W * .1f, H * .03f), 8, Coat);
    Img.Ellipse(FVector2D(W * .15f, H * .84f), FVector2D(W * .09f, H * .03f), -5, Coat);
    // Pup curled against her.
    Img.Ellipse(FVector2D(W * .66f, H * .86f), FVector2D(W * .11f, H * .055f), 0, Coat);
    Img.Ellipse(FVector2D(W * .74f, H * .82f), FVector2D(W * .045f, H * .04f), 0, Coat);
    Img.Ellipse(FVector2D(W * .77f, H * .83f), FVector2D(2.5f, 2.5f), 0, Spot);
    for (int32 K = 0; K < 14; ++K) Img.Ellipse(FVector2D(W * R.FRandRange(.57f, .75f), H * R.FRandRange(.83f, .9f)), FVector2D(R.FRandRange(2.f, 4.f)), 0, Spot);
    // Signature.
    HwText(Img, TEXT("B HAVILAND"), W * .82f, H * .96f, 9, 1.4f, FLinearColor(.5f, .4f, .3f), .8f);
}
void HwPaintDalmatianMat(FHallImage& Img)
{
    // Olive mat with red inner line and three stamp windows underneath.
    const int32 W = 512, H = 640;
    Img.Init(W, H, FLinearColor(.16f, .22f, .16f));
    Img.Rect(FVector2D(56, 44), FVector2D(W - 56, H - 170), FLinearColor(.45f, .08f, .06f));
    FHallImage Pic; HwPaintDalmatian(Pic);
    for (int32 Y = 50; Y < H - 176; ++Y)
        for (int32 X = 62; X < W - 62; ++X)
            Img.At(X, Y) = Pic.Sample((X - 62) / float(W - 124), (Y - 50) / float(H - 226));
    const FLinearColor StampBg[3] = { FLinearColor(.75f, .7f, .55f), FLinearColor(.6f, .15f, .12f), FLinearColor(.2f, .25f, .45f) };
    for (int32 K = 0; K < 3; ++K)
    {
        const float X0 = 90 + K * 125, Y0 = H - 140;
        Img.Rect(FVector2D(X0, Y0), FVector2D(X0 + 100, Y0 + 90), FLinearColor(.45f, .08f, .06f));
        Img.Rect(FVector2D(X0 + 6, Y0 + 6), FVector2D(X0 + 94, Y0 + 84), StampBg[K]);
        Img.Ellipse(FVector2D(X0 + 50, Y0 + 40), FVector2D(22, 16), 0, FLinearColor(.9f, .88f, .84f));
        Img.Ellipse(FVector2D(X0 + 42, Y0 + 36), FVector2D(4, 4), 0, FLinearColor(.06f, .06f, .06f));
        Img.Ellipse(FVector2D(X0 + 58, Y0 + 44), FVector2D(3, 3), 0, FLinearColor(.06f, .06f, .06f));
        HwText(Img, TEXT("USA 29"), X0 + 50, Y0 + 78, 10, 1.6f, FLinearColor(.1f, .1f, .1f));
    }
    Img.Rect(FVector2D(W * .5f - 60, H - 36), FVector2D(W * .5f + 60, H - 22), FLinearColor(.72f, .6f, .25f));
}
void HwPaintLighthouse(FHallImage& Img)
{
    const int32 W = 768, H = 256;
    Img.Init(W, H, FLinearColor(.4f, .5f, .62f));
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float V = Y / float(H);
            FLinearColor C;
            if (V < .42f)
            {
                const float Cloud = HwFbm(X * .01f, Y * .03f, 4, 8);
                C = FMath::Lerp(FLinearColor(.35f, .45f, .6f), FLinearColor(.72f, .76f, .82f), FMath::Clamp((Cloud - .35f) * 2.f, 0.f, 1.f) * (1 - V));
            }
            else
            {
                const float Wave = HwFbm(X * .05f, Y * .3f, 3, 9);
                C = FMath::Lerp(FLinearColor(.08f, .2f, .32f), FLinearColor(.3f, .45f, .55f), Wave * .8f);
                if (Wave > .62f) C = FMath::Lerp(C, FLinearColor(.8f, .85f, .88f), (Wave - .62f) * 2.5f);
            }
            Img.At(X, Y) = C;
        }
    // Rocky headland on the right with the tower.
    TArray<FVector2D> Rock;
    for (int32 K = 0; K <= 20; ++K) { const float T = K / 20.f; Rock.Add(FVector2D(W * (.52f + .48f * T), H * (.62f - .2f * FMath::Sin(T * PI) * (1 - T * .3f)) + HwNoise(K * .7f, 0, 3) * 10)); }
    Rock.Add(FVector2D(W, H)); Rock.Add(FVector2D(W * .52f, H));
    Img.FillPolygon(Rock, FLinearColor(.14f, .11f, .09f));
    for (int32 K = 0; K < 200; ++K)
    {
        const FVector2D P(W * (.55f + .44f * HwRand(K, 1, 5)), H * (.56f + .42f * HwRand(K, 2, 5)));
        if (P.Y < H * .5f) continue;
        Img.Ellipse(P, FVector2D(4 + 8 * HwRand(K, 3, 5), 2 + 4 * HwRand(K, 4, 5)), 0, HwMul(FLinearColor(.26f, .22f, .18f), .6f + .8f * HwRand(K, 5, 5)), .7f);
    }
    // Lighthouse: white tapered tower, red lantern.
    const float BX = W * .8f, BY = H * .5f;
    Img.FillPolygon({ FVector2D(BX - 14, BY), FVector2D(BX + 14, BY), FVector2D(BX + 9, BY - 70), FVector2D(BX - 9, BY - 70) }, FLinearColor(.9f, .9f, .88f));
    Img.FillPolygon({ FVector2D(BX - 16, BY - 70), FVector2D(BX + 16, BY - 70), FVector2D(BX + 16, BY - 76), FVector2D(BX - 16, BY - 76) }, FLinearColor(.1f, .1f, .1f));
    Img.Rect(FVector2D(BX - 9, BY - 92), FVector2D(BX + 9, BY - 76), FLinearColor(.75f, .1f, .08f));
    Img.Rect(FVector2D(BX - 6, BY - 90), FVector2D(BX + 6, BY - 78), FLinearColor(1.f, .9f, .6f));
    Img.FillPolygon({ FVector2D(BX - 11, BY - 92), FVector2D(BX + 11, BY - 92), FVector2D(BX, BY - 104) }, FLinearColor(.6f, .08f, .06f));
    Img.Rect(FVector2D(BX - 30, BY - 2), FVector2D(BX + 30, BY + 20), FLinearColor(.86f, .86f, .84f));  // keeper's house
    Img.FillPolygon({ FVector2D(BX - 33, BY - 2), FVector2D(BX + 33, BY - 2), FVector2D(BX, BY - 22) }, FLinearColor(.5f, .12f, .1f));
    // Surf at the rocks.
    for (int32 K = 0; K < 40; ++K) Img.Ellipse(FVector2D(W * (.5f + .3f * HwRand(K, 6, 7)), H * (.6f + .15f * HwRand(K, 7, 7))), FVector2D(14 + 20 * HwRand(K, 8, 7), 3), 0, FLinearColor(.85f, .9f, .92f), .35f);
}
void HwPaintCorgiPool(FHallImage& Img)
{
    const int32 W = 512, H = 384;
    Img.Init(W, H, FLinearColor(.1f, .06f, .035f));
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float Lamp = FMath::Clamp(1.f - FVector2D::Distance(FVector2D(X, Y), FVector2D(W * .5f, H * .55f)) / (W * .6f), 0.f, 1.f);
            Img.At(X, Y) = HwMul(FLinearColor(.16f, .1f, .05f), .25f + Lamp * 1.1f);
        }
    // Hanging lamp.
    Img.Rect(FVector2D(W * .5f - 2, 0), FVector2D(W * .5f + 2, H * .12f), FLinearColor(.05f, .04f, .03f));
    Img.FillPolygon({ FVector2D(W * .5f - 70, H * .22f), FVector2D(W * .5f + 70, H * .22f), FVector2D(W * .5f + 24, H * .11f), FVector2D(W * .5f - 24, H * .11f) }, FLinearColor(.2f, .5f, .3f));
    Img.Rect(FVector2D(W * .5f - 64, H * .215f), FVector2D(W * .5f + 64, H * .235f), FLinearColor(1.f, .9f, .6f));
    // Pool table (perspective trapezoid) with rails and balls.
    Img.FillPolygon({ FVector2D(W * .12f, H * .62f), FVector2D(W * .88f, H * .62f), FVector2D(W * .98f, H * .9f), FVector2D(W * .02f, H * .9f) }, FLinearColor(.3f, .16f, .08f));
    Img.FillPolygon({ FVector2D(W * .16f, H * .64f), FVector2D(W * .84f, H * .64f), FVector2D(W * .93f, H * .87f), FVector2D(W * .07f, H * .87f) }, FLinearColor(.08f, .35f, .16f));
    const FLinearColor BallC[7] = { FLinearColor(.9f, .8f, .1f), FLinearColor(.1f, .2f, .8f), FLinearColor(.8f, .1f, .1f), FLinearColor(.4f, .1f, .5f), FLinearColor(.9f, .45f, .1f), FLinearColor(.1f, .5f, .2f), FLinearColor(.95f, .95f, .9f) };
    for (int32 K = 0; K < 7; ++K) Img.Ellipse(FVector2D(W * (.3f + .06f * K + .03f * (K % 2)), H * (.72f + .05f * (K % 3))), FVector2D(7, 7), 0, BallC[K]);
    Img.Rect(FVector2D(W * .2f, H * .79f), FVector2D(W * .62f, H * .8f), FLinearColor(.75f, .6f, .35f)); // cue
    // Three corgis around the table.
    auto Corgi = [&](float CX, float CY, float S, bool bFlip)
    {
        const FLinearColor Fur(.85f, .5f, .18f), White(.95f, .92f, .86f), Dark(.08f, .06f, .05f);
        const float D = bFlip ? -1.f : 1.f;
        Img.Ellipse(FVector2D(CX, CY), FVector2D(S * .55f, S * .3f), 0, Fur);                       // body
        Img.Ellipse(FVector2D(CX, CY + S * .12f), FVector2D(S * .5f, S * .16f), 0, White);          // chest/belly
        Img.Ellipse(FVector2D(CX + D * S * .5f, CY - S * .25f), FVector2D(S * .26f, S * .24f), 0, Fur);   // head
        Img.FillPolygon({ FVector2D(CX + D * S * .62f, CY - S * .22f), FVector2D(CX + D * S * .85f, CY - S * .12f), FVector2D(CX + D * S * .66f, CY - S * .05f) }, White); // muzzle
        Img.FillPolygon({ FVector2D(CX + D * S * .36f, CY - S * .4f), FVector2D(CX + D * S * .3f, CY - S * .72f), FVector2D(CX + D * S * .5f, CY - S * .46f) }, Fur);   // ear
        Img.FillPolygon({ FVector2D(CX + D * S * .56f, CY - S * .44f), FVector2D(CX + D * S * .62f, CY - S * .74f), FVector2D(CX + D * S * .7f, CY - S * .44f) }, Fur);
        Img.Ellipse(FVector2D(CX + D * S * .58f, CY - S * .3f), FVector2D(S * .035f, S * .035f), 0, Dark);
        Img.Ellipse(FVector2D(CX + D * S * .86f, CY - S * .14f), FVector2D(S * .04f, S * .03f), 0, Dark);
        for (const float LX : { -.3f, -.1f, .15f, .35f }) Img.Rect(FVector2D(CX + LX * S - S * .05f, CY + S * .2f), FVector2D(CX + LX * S + S * .05f, CY + S * .42f), Fur);
    };
    Corgi(W * .22f, H * .5f, 80, false);
    Corgi(W * .72f, H * .48f, 76, true);
    Corgi(W * .5f, H * .93f, 70, false);
    HwText(Img, TEXT("RACK EM"), W * .5f, H * .36f, 18, 3.f, FLinearColor(.75f, .65f, .35f), .8f);
}
void HwPaintComposite(FHallImage& Img)
{
    const int32 W = 512, H = 400;
    Img.Init(W, H, FLinearColor(.86f, .82f, .7f));
    Img.Rect(FVector2D(0, 0), FVector2D(W, 54), FLinearColor(.3f, .06f, .06f));
    HwText(Img, TEXT("TENNESSEE   SPRING 2026"), W * .5f, H - 14, 13, 2.2f, FLinearColor(.3f, .06f, .06f), .8f);
    FRandomStream R(9);
    for (int32 Row = 0; Row < 4; ++Row)
        for (int32 Col = 0; Col < 7; ++Col)
        {
            const FVector2D C(40 + Col * 72, 92 + Row * 72);
            Img.Ellipse(C, FVector2D(24, 29), 0, FLinearColor(.25f, .18f, .12f));
            Img.Ellipse(C, FVector2D(22, 27), 0, FLinearColor(.5f, .45f, .38f));
            const float Skin = R.FRandRange(.45f, .85f);
            Img.Ellipse(C + FVector2D(0, 2), FVector2D(10, 13), 0, FLinearColor(Skin, Skin * .78f, Skin * .62f));
            Img.Ellipse(C + FVector2D(0, -8), FVector2D(11, 6), 0, FLinearColor(.1f, .08f, .06f) * R.FRandRange(.5f, 2.f));
            Img.Rect(C + FVector2D(-14, 14), C + FVector2D(14, 27), FLinearColor(.12f, .12f, .14f));
            Img.Rect(C + FVector2D(-20, 32), C + FVector2D(20, 36), FLinearColor(.35f, .3f, .25f));
        }
}
void HwPaintMountains(FHallImage& Img)
{
    const int32 W = 512, H = 320;
    Img.Init(W, H, FLinearColor(.9f, .6f, .4f));
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
            Img.At(X, Y) = FMath::Lerp(FLinearColor(.25f, .3f, .55f), FLinearColor(.95f, .6f, .35f), FMath::Pow(Y / float(H), 1.5f));
    Img.Ellipse(FVector2D(W * .3f, H * .5f), FVector2D(26, 26), 0, FLinearColor(1.f, .9f, .7f));
    const FLinearColor Layers[4] = { FLinearColor(.5f, .38f, .5f), FLinearColor(.32f, .24f, .36f), FLinearColor(.18f, .14f, .22f), FLinearColor(.08f, .07f, .1f) };
    for (int32 L = 0; L < 4; ++L)
    {
        TArray<FVector2D> Poly;
        for (int32 K = 0; K <= 32; ++K) { const float T = K / 32.f; Poly.Add(FVector2D(W * T, H * (.55f + L * .1f) - HwFbm(T * 6 + L * 3, L, 3, 40 + L) * H * (.3f - L * .05f))); }
        Poly.Add(FVector2D(W, H)); Poly.Add(FVector2D(0, H));
        Img.FillPolygon(Poly, Layers[L]);
    }
}
void HwPaintPicture(FHallImage& Img, int32 Kind)
{
    switch (Kind)
    {
    case 0: HwPaintDalmatianMat(Img); break;
    case 1: HwPaintLighthouse(Img); break;
    case 2: HwPaintCorgiPool(Img); break;
    case 3: HwPaintComposite(Img); break;
    default: HwPaintMountains(Img); break;
    }
}

// ------------------------------------------------------------------ drywall damage decals
void HwPaintDamage(FHallImage& Img, TArray<float>& Height, const FLinearColor& Wall, int32 Kind, int32 Seed)
{
    const int32 W = 256, H = 256;
    Img.Init(W, H, Wall);
    Height.SetNumZeroed(W * H);
    FRandomStream R(Seed);
    const FLinearColor Gypsum(.72f, .7f, .66f), Paper(.6f, .56f, .48f), Hole(.02f, .018f, .016f), Compound(.78f, .74f, .7f);
    auto Blob = [&](const FVector2D& C, float Rad, int32 N, float Jag) { TArray<FVector2D> P; const float Rot = R.FRandRange(0.f, 6.28f); for (int32 K = 0; K < N; ++K) { const float A = Rot + 6.2832f * K / N; const float Rr = Rad * R.FRandRange(1.f - Jag, 1.f + Jag); P.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr); } return P; };
    if (Kind == 0)
    {
        // Punched hole: torn paper rim, gypsum, black cavity.
        const FVector2D C(W * .5f, H * .52f);
        const TArray<FVector2D> Rim = Blob(C, 70, 14, .35f), Gyp = Blob(C, 48, 12, .3f), Cav = Blob(C, 30, 10, .4f);
        Img.FillPolygon(Rim, Paper); Img.FillPolygon(Gyp, Gypsum); Img.FillPolygon(Cav, Hole);
        Height.Init(0.f, W * H);
        // Heights: rim raised (torn paper), cavity sunk.
        FHallImage Hm; Hm.Init(W, H, FLinearColor(0, 0, 0, 0));
        Hm.FillPolygon(Rim, FLinearColor(.4f, 0, 0)); Hm.FillPolygon(Gyp, FLinearColor(-.3f, 0, 0)); Hm.FillPolygon(Cav, FLinearColor(-1.f, 0, 0));
        for (int32 I = 0; I < W * H; ++I) Height[I] = Hm.Px[I].R;
        for (int32 K = 0; K < 24; ++K)
        {
            const float A = R.FRandRange(0.f, 6.28f);
            Img.Stroke({ C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 60, C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R.FRandRange(75.f, 100.f) }, R.FRandRange(1.f, 2.5f), HwMul(Wall, .75f), .6f);
        }
        for (int32 K = 0; K < 60; ++K) Img.Ellipse(C + FVector2D(R.FRandRange(-45.f, 45.f), R.FRandRange(-45.f, 45.f)), FVector2D(R.FRandRange(1.f, 3.f)), 0, HwMul(Gypsum, R.FRandRange(.7f, 1.1f)), .6f);
    }
    else if (Kind == 1)
    {
        // Patch of joint compound sanded but never painted: a shade lighter than the wall, feathered edges, faint tape lines.
        const FLinearColor Mud = FMath::Lerp(Wall, FLinearColor(.78f, .74f, .7f), .45f);
        const FVector2D Min(W * .2f, H * .24f), Max(W * .82f, H * .78f);
        for (int32 K = 0; K < 160; ++K)
            Img.Ellipse(FVector2D(R.FRandRange(Min.X - 10, Max.X + 10), R.FRandRange(Min.Y - 10, Max.Y + 10)), FVector2D(R.FRandRange(14.f, 36.f), R.FRandRange(8.f, 20.f)), R.FRandRange(0.f, 180.f), Mud, .22f);
        Img.Rect(Min + FVector2D(8, 8), Max - FVector2D(8, 8), Mud, .85f);
        for (int32 K = 0; K < 300; ++K)
        {
            const FVector2D P(R.FRandRange(Min.X, Max.X), R.FRandRange(Min.Y, Max.Y));
            Img.Ellipse(P, FVector2D(R.FRandRange(6.f, 18.f), R.FRandRange(1.f, 2.5f)), R.FRandRange(-25.f, 25.f), HwMul(Mud, R.FRandRange(.9f, 1.06f)), .3f);
        }
        // Tape seams showing through.
        Img.Rect(FVector2D(Min.X + 20, (Min.Y + Max.Y) * .5f - 1), FVector2D(Max.X - 20, (Min.Y + Max.Y) * .5f + 1), HwMul(Mud, .9f), .5f);
        for (int32 Y = 0; Y < H; ++Y) for (int32 X = 0; X < W; ++X) Height[Y * W + X] = (X > Min.X && X < Max.X && Y > Min.Y && Y < Max.Y) ? .12f : 0.f;
    }
    else if (Kind == 2)
    {
        // Scuffs and a gouge: grey smears near the baseboard height.
        for (int32 K = 0; K < 14; ++K)
        {
            const FVector2D P(R.FRandRange(30.f, W - 30.f), R.FRandRange(60.f, H - 40.f));
            const float Len = R.FRandRange(30.f, 110.f), A = R.FRandRange(-.4f, .4f);
            Img.Stroke({ P, P + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Len }, R.FRandRange(3.f, 12.f), HwMul(Wall, R.FRandRange(.45f, .8f)), R.FRandRange(.2f, .5f));
        }
        const FVector2D G(W * .55f, H * .5f);
        Img.Stroke({ G, G + FVector2D(60, -8) }, 5.f, Gypsum);
        Img.Stroke({ G, G + FVector2D(60, -8) }, 2.f, Paper * .8f);
        FHallImage Hm; Hm.Init(W, H, FLinearColor(0, 0, 0, 0));
        Hm.Stroke({ G, G + FVector2D(60, -8) }, 5.f, FLinearColor(-.5f, 0, 0));
        for (int32 I = 0; I < W * H; ++I) Height[I] = Hm.Px[I].R;
    }
    else
    {
        // Two smaller punches side by side (fist marks).
        for (int32 K = 0; K < 2; ++K)
        {
            const FVector2D C(W * (.32f + .36f * K), H * (.5f + .08f * (K ? -1 : 1)));
            const TArray<FVector2D> Rim = Blob(C, 34, 12, .35f), Cav = Blob(C, 17, 9, .4f);
            Img.FillPolygon(Rim, Gypsum); Img.FillPolygon(Cav, Hole);
            FHallImage Hm; Hm.Init(W, H, FLinearColor(0, 0, 0, 0));
            Hm.FillPolygon(Rim, FLinearColor(-.3f, 0, 0)); Hm.FillPolygon(Cav, FLinearColor(-1.f, 0, 0));
            for (int32 I = 0; I < W * H; ++I) Height[I] += Hm.Px[I].R;
            for (int32 J = 0; J < 10; ++J) { const float A = R.FRandRange(0.f, 6.28f); Img.Stroke({ C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 30, C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R.FRandRange(38.f, 52.f) }, 1.5f, HwMul(Wall, .7f), .6f); }
        }
    }
    // Grime fade so the square never reads as a square.
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float EdgeX = FMath::Min(X, W - 1 - X) / float(W), EdgeY = FMath::Min(Y, H - 1 - Y) / float(H);
            const float Edge = FMath::Clamp(FMath::Min(EdgeX, EdgeY) * 10.f, 0.f, 1.f);
            Img.At(X, Y) = FMath::Lerp(Wall, Img.At(X, Y), Edge);
            Height[Y * W + X] *= Edge;
        }
}
}

void HwCreateLook(UHouseBlackHallSubsystem* Sub)
{
    if (!Sub || Sub->Materials.Contains(TEXT("HW_Red"))) return;
    auto Plain = [&](const TCHAR* Key, const FLinearColor& C, float Rough, float Metal = 0.f, const FLinearColor& Em = FLinearColor::Black) { HallMakePlain(Sub, FName(Key), C, Rough, Metal, Em); };
    Plain(TEXT("HW_Red"), FLinearColor(.55f, .02f, .012f), .45f);
    Plain(TEXT("HW_White"), FLinearColor(.84f, .84f, .82f), .5f);
    Plain(TEXT("HW_Lens"), FLinearColor(.9f, .9f, .95f), .25f, 0.f, FLinearColor(.2f, .2f, .22f));
    Plain(TEXT("HW_PanelGrey"), FLinearColor(.5f, .52f, .53f), .45f, .3f);
    Plain(TEXT("HW_Steel"), FLinearColor(.6f, .61f, .62f), .35f, .85f);
    Plain(TEXT("HW_Black"), FLinearColor(.02f, .02f, .02f), .85f);
    Plain(TEXT("HW_GreenVinyl"), FLinearColor(.03f, .26f, .16f), .38f);
    Plain(TEXT("HW_GreenFrame"), FLinearColor(.03f, .08f, .06f), .4f, .5f);
    Plain(TEXT("HW_Chrome"), FLinearColor(.8f, .8f, .82f), .2f, 1.f);
    Plain(TEXT("HW_BluePlastic"), FLinearColor(.16f, .3f, .6f), .3f);
    Plain(TEXT("HW_Cap"), FLinearColor(.9f, .9f, .9f), .4f);
    Plain(TEXT("HW_FrameDark"), FLinearColor(.07f, .045f, .03f), .5f);
    Plain(TEXT("HW_FrameGold"), FLinearColor(.6f, .45f, .15f), .35f, .6f);
    Plain(TEXT("HW_Purple"), FLinearColor(.3f, .08f, .5f), .35f);
    Plain(TEXT("HW_Cavity"), FLinearColor(.5f, .5f, .5f), .8f);
    Plain(TEXT("HW_Led"), FLinearColor(1.f, .05f, .02f), .4f, 0.f, FLinearColor(30.f, .6f, .3f));
    Plain(TEXT("HW_Shirt"), FLinearColor(.72f, .66f, .55f), .9f);
    Plain(TEXT("HW_RailBlack"), FLinearColor(.035f, .035f, .037f), .42f, .35f);
    Plain(TEXT("HW_Tan"), FLinearColor(.5f, .34f, .18f), .6f);
    if (UMaterialInterface* GlassParent = HallHouseMat(TEXT("M_House_Glass")))
    {
        UMaterialInstanceDynamic* Tea = UMaterialInstanceDynamic::Create(GlassParent, Sub);
        Tea->SetVectorParameterValue(TEXT("Tint"), FLinearColor(.5f, .17f, .025f));
        Tea->SetScalarParameterValue(TEXT("Opacity"), .78f);
        Tea->SetScalarParameterValue(TEXT("Roughness"), .15f);
        Sub->Materials.Add(TEXT("HW_Tea"), Tea);
        UMaterialInstanceDynamic* Clear = UMaterialInstanceDynamic::Create(GlassParent, Sub);
        Clear->SetVectorParameterValue(TEXT("Tint"), FLinearColor(.8f, .85f, .9f));
        Clear->SetScalarParameterValue(TEXT("Opacity"), .3f);
        Clear->SetScalarParameterValue(TEXT("Roughness"), .1f);
        Sub->Materials.Add(TEXT("HW_ClearPlastic"), Clear);
    }
    auto Painted = [&](const TCHAR* Key, FHallImage& Img, float Rough, UTexture2D* Normal = nullptr, float NormalStrength = 1.f, const FLinearColor& Em = FLinearColor::Black)
    {
        UTexture2D* T = HwTex(Sub, Img, *FString::Printf(TEXT("T_%s"), Key));
        HallMakeTextured(Sub, FName(Key), T, Normal, Rough, FLinearColor::White, NormalStrength, 0.f, Em);
    };
    FHallImage Img; TArray<float> Height;
    HwPaintFireFace(Img); Painted(TEXT("HW_FireFace"), Img, .45f);
    HwPaintHornFace(Img); Painted(TEXT("HW_HornFace"), Img, .45f);
    HwPaintExitFace(Img, 0); Painted(TEXT("HW_Exit0"), Img, .4f, nullptr, 1.f, FLinearColor(.9f, .9f, .9f));
    HwPaintExitFace(Img, -1); Painted(TEXT("HW_ExitL"), Img, .4f, nullptr, 1.f, FLinearColor(.9f, .9f, .9f));
    HwPaintExitFace(Img, 1); Painted(TEXT("HW_ExitR"), Img, .4f, nullptr, 1.f, FLinearColor(.9f, .9f, .9f));
    for (int32 K = 0; K < 4; ++K) { HwPaintPlaqueLabel(Img, 300 + K * 7); Painted(*FString::Printf(TEXT("HW_Plaque%d"), K), Img, .35f); }
    HwPaintThermostat(Img); Painted(TEXT("HW_Thermostat"), Img, .4f, nullptr, 1.f, FLinearColor(.03f, .04f, .02f));
    HwPaintPanelFace(Img, Height); Painted(TEXT("HW_PanelFace"), Img, .4f, HwNormalTex(Sub, Height, Img.W, Img.H, 2.5f, TEXT("T_HW_PanelFaceN")), .8f);
    HwPaintCabinetStrip(Img); Painted(TEXT("HW_CabinetStrip"), Img, .5f);
    HwPaintExtinguisherLabel(Img); Painted(TEXT("HW_ExtLabel"), Img, .5f);
    HwPaintLevelSign(Img, 4); Painted(TEXT("HW_LevelSign4"), Img, .45f);
    HwPaintLevelSign(Img, 3); Painted(TEXT("HW_LevelSign3"), Img, .45f);
    HwPaintRegister(Img, Height); Painted(TEXT("HW_Register"), Img, .4f, HwNormalTex(Sub, Height, Img.W, Img.H, 3.f, TEXT("T_HW_RegisterN")), 1.f);
    HwPaintMat(Img, Height, false); Painted(TEXT("HW_MatBlack"), Img, .8f, HwNormalTex(Sub, Height, Img.W, Img.H, 2.f, TEXT("T_HW_MatBlackN"), true), 1.f);
    HwPaintMat(Img, Height, true); Painted(TEXT("HW_MatGrey"), Img, .95f, HwNormalTex(Sub, Height, Img.W, Img.H, 2.f, TEXT("T_HW_MatGreyN"), true), .7f);
    HwPaintWicker(Img, Height);
    {
        UTexture2D* T = HallMakeTexture(Img, true, false, TEXT("T_HW_Wicker"), true); Sub->Textures.Add(TEXT("T_HW_Wicker"), T);
        HallMakeTextured(Sub, TEXT("HW_Wicker"), T, HwNormalTex(Sub, Height, Img.W, Img.H, 3.f, TEXT("T_HW_WickerN"), true), .7f, FLinearColor::White, 1.f);
    }
    HwPaintWood(Img, Height, FLinearColor(.42f, .24f, .1f), 5);
    {
        UTexture2D* T = HallMakeTexture(Img, true, false, TEXT("T_HW_Oak"), true); Sub->Textures.Add(TEXT("T_HW_Oak"), T);
        HallMakeTextured(Sub, TEXT("HW_Oak"), T, HwNormalTex(Sub, Height, Img.W, Img.H, 1.2f, TEXT("T_HW_OakN"), true), .55f, FLinearColor::White, .6f);
    }
    HwPaintWood(Img, Height, FLinearColor(.55f, .38f, .18f), 9);
    {
        UTexture2D* T = HallMakeTexture(Img, true, false, TEXT("T_HW_Pine"), true); Sub->Textures.Add(TEXT("T_HW_Pine"), T);
        HallMakeTextured(Sub, TEXT("HW_Pine"), T, HwNormalTex(Sub, Height, Img.W, Img.H, 1.2f, TEXT("T_HW_PineN"), true), .6f, FLinearColor::White, .6f);
    }
    HwPaintLeather(Img, Height);
    {
        UTexture2D* T = HallMakeTexture(Img, true, false, TEXT("T_HW_Leather"), true); Sub->Textures.Add(TEXT("T_HW_Leather"), T);
        HallMakeTextured(Sub, TEXT("HW_Leather"), T, HwNormalTex(Sub, Height, Img.W, Img.H, 1.5f, TEXT("T_HW_LeatherN"), true), .42f, FLinearColor::White, .7f);
    }
    if (UTexture2D* Fabric = LoadObject<UTexture2D>(nullptr, TEXT("/Game/SeniorSendOff/House/Textures/T_Fabric_Normal.T_Fabric_Normal")))
    {
        HallMakeTextured(Sub, TEXT("HW_Linen"), nullptr, Fabric, .92f, FLinearColor(.8f, .78f, .74f), .8f);
        HallMakeTextured(Sub, TEXT("HW_LinenGrey"), nullptr, Fabric, .92f, FLinearColor(.5f, .5f, .5f), .8f);
    }
    else
    {
        Plain(TEXT("HW_Linen"), FLinearColor(.8f, .78f, .74f), .92f);
        Plain(TEXT("HW_LinenGrey"), FLinearColor(.5f, .5f, .5f), .92f);
    }
    const TCHAR* PicKeys[5] = { TEXT("HW_PicDalmatian"), TEXT("HW_PicLighthouse"), TEXT("HW_PicCorgi"), TEXT("HW_PicComposite"), TEXT("HW_PicMountains") };
    for (int32 K = 0; K < 5; ++K) { HwPaintPicture(Img, K); Painted(PicKeys[K], Img, K == 1 || K == 4 ? .3f : .55f); }
}

/** Damage decal material for a wall colour (created on demand, cached by key). */
UMaterialInterface* HwDamageMaterial(UHouseBlackHallSubsystem* Sub, const FLinearColor& Wall, float Rough, int32 Kind, int32 Seed)
{
    const FName Key(*FString::Printf(TEXT("HW_Dmg_%d_%d_%d"), Kind, Seed, FMath::RoundToInt(Wall.GetLuminance() * 1000)));
    if (TObjectPtr<UMaterialInterface>* Found = Sub->Materials.Find(Key)) return *Found;
    FHallImage Img; TArray<float> Height;
    HwPaintDamage(Img, Height, Wall, Kind, Seed);
    UTexture2D* T = HallMakeTexture(Img, true, false, *(TEXT("T_") + Key.ToString()), false);
    UTexture2D* N = HwNormalTex(Sub, Height, Img.W, Img.H, 2.2f, *(TEXT("T_") + Key.ToString() + TEXT("N")));
    Sub->Textures.Add(FName(*(TEXT("T_") + Key.ToString())), T);
    return HallMakeTextured(Sub, Key, T, N, Rough, FLinearColor::White, 1.f);
}
