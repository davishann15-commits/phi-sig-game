#include "HouseBlackHallLook.h"
#include "HouseBlackHall.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

using namespace HallSpec;

namespace
{
float HallSmooth(float A, float B, float X) { const float T = FMath::Clamp((X - A) / FMath::Max(1e-4f, B - A), 0.f, 1.f); return T * T * (3 - 2 * T); }

uint32 HallHash3(int32 X, int32 Y, int32 S)
{
    uint32 H = (uint32)X * 73856093u ^ (uint32)Y * 19349663u ^ (uint32)S * 83492791u;
    H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
    return H;
}
float HallRand01(int32 X, int32 Y, int32 S) { return (HallHash3(X, Y, S) & 0xFFFFFF) / float(0xFFFFFF); }

/** Value noise that wraps with an integer lattice period (for tiling textures). */
float HallPNoise(float X, float Y, int32 PX, int32 PY, int32 Seed)
{
    const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
    float FX = X - X0, FY = Y - Y0;
    FX = FX * FX * (3 - 2 * FX); FY = FY * FY * (3 - 2 * FY);
    auto L = [&](int32 IX, int32 IY) { return HallRand01(((IX % PX) + PX) % PX, ((IY % PY) + PY) % PY, Seed); };
    return FMath::Lerp(FMath::Lerp(L(X0, Y0), L(X0 + 1, Y0), FX), FMath::Lerp(L(X0, Y0 + 1), L(X0 + 1, Y0 + 1), FX), FY);
}
/** Tileable fbm over u,v in [0,1). */
float HallPFbm(float U, float V, int32 KX, int32 KY, int32 Oct, int32 Seed)
{
    float Sum = 0, Amp = .5f, Norm = 0;
    for (int32 O = 0; O < Oct; ++O)
    {
        Sum += HallPNoise(U * KX, V * KY, KX, KY, Seed + O * 17) * Amp; Norm += Amp;
        KX *= 2; KY *= 2; Amp *= .5f;
    }
    return Sum / Norm;
}

}

UMaterialInterface* HallHouseMat(const TCHAR* Name)
{
    return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/SeniorSendOff/House/Materials/%s.%s"), Name, Name));
}

UMaterialInstanceDynamic* HallMakeTextured(UHouseBlackHallSubsystem* Sub, FName Key, UTexture2D* Base, UTexture2D* Normal, float Rough,
    const FLinearColor& Tint, float NormalStrength, float Metal, const FLinearColor& Emission)
{
    UMaterialInterface* Parent = HallHouseMat(TEXT("M_House_Textured"));
    if (!Parent) return nullptr;
    UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Parent, Sub);
    if (Base) M->SetTextureParameterValue(TEXT("BaseMap"), Base);
    if (Normal) M->SetTextureParameterValue(TEXT("NormalMap"), Normal);
    M->SetScalarParameterValue(TEXT("UVScale"), 1.f);
    M->SetScalarParameterValue(TEXT("Roughness"), Rough);
    M->SetScalarParameterValue(TEXT("Metallic"), Metal);
    M->SetScalarParameterValue(TEXT("NormalStrength"), NormalStrength);
    M->SetVectorParameterValue(TEXT("Tint"), Tint);
    M->SetVectorParameterValue(TEXT("Emission"), Emission);
    Sub->Materials.Add(Key, M);
    return M;
}

UMaterialInstanceDynamic* HallMakePlain(UHouseBlackHallSubsystem* Sub, FName Key, const FLinearColor& Tint, float Rough, float Metal,
    const FLinearColor& Emission)
{
    UMaterialInterface* Parent = HallHouseMat(TEXT("M_House_Plain"));
    if (!Parent) return nullptr;
    UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Parent, Sub);
    M->SetVectorParameterValue(TEXT("Tint"), Tint);
    M->SetScalarParameterValue(TEXT("Roughness"), Rough);
    M->SetScalarParameterValue(TEXT("Metallic"), Metal);
    M->SetVectorParameterValue(TEXT("Emission"), Emission);
    Sub->Materials.Add(Key, M);
    return M;
}

namespace
{
// ----------------------------------------------------------------------------- block wall
void HallBuildCmu(FHallImage& Base, TArray<float>& Height, int32 Res)
{
    const float P = Cmu, Course = 20.32f, Block = 40.64f, Joint = .95f;
    Base.Init(Res, Res, FLinearColor::Black);
    Height.SetNumZeroed(Res * Res);
    for (int32 Y = 0; Y < Res; ++Y)
        for (int32 X = 0; X < Res; ++X)
        {
            const float U01 = (X + .5f) / Res, V01 = (Y + .5f) / Res;
            const float U = U01 * P, V = V01 * P;
            const int32 C = FMath::FloorToInt(V / Course);
            const float VV = V - C * Course;
            float UU = U + ((C & 1) ? Block * .5f : 0.f);
            UU = FMath::Fmod(UU, P);
            const int32 B = FMath::FloorToInt(UU / Block);
            const float BU = UU - B * Block;
            const float DJ = FMath::Min(FMath::Min(BU, Block - BU), FMath::Min(VV, Course - VV));
            const float Face = HallSmooth(Joint * .5f - .05f, Joint * .5f + .65f, DJ);
            const int32 BlockId = (C & 3) * 2 + (B & 1);
            const float Tone = 1.f + (HallRand01(BlockId, 3, 11) - .5f) * .22f;
            const float Pore = HallPFbm(U01, V01, 48, 48, 3, 5);
            const float Fine = HallPNoise(U01 * 192, V01 * 192, 192, 192, 9);
            const float Pit = Fine > .86f ? (Fine - .86f) * 4.f : 0.f;
            const float Drip = HallPFbm(U01, V01, 4, 2, 3, 21);
            FLinearColor Paint(0.021f, 0.022f, 0.024f);
            Paint *= Tone * (0.92f + Pore * .16f) * (0.96f + Drip * .08f);
            Paint += FLinearColor(.01f, .01f, .01f) * Pit;
            const FLinearColor Mortar = FLinearColor(0.012f, 0.0125f, 0.013f) * (0.9f + Pore * .2f);
            Base.At(X, Y) = FMath::Lerp(Mortar, Paint, Face);
            Height[Y * Res + X] = Face * (1.f + (Pore - .5f) * .25f - Pit * .35f);
        }
}

// ----------------------------------------------------------------------------- floor (whole hall, not tiled)
void HallBuildFloor(FHallImage& Img, float X0, float Y0, float WCm, float HCm)
{
    const int32 W = Img.W, H = Img.H;
    const FLinearColor Dark(0.085f, 0.060f, 0.055f), Mid(0.175f, 0.125f, 0.113f), Light(0.225f, 0.175f, 0.160f);
    const FLinearColor Residue(0.30f, 0.285f, 0.275f);
    for (int32 J = 0; J < H; ++J)
        for (int32 I = 0; I < W; ++I)
        {
            const float X = X0 + (I + .5f) / W * WCm, Y = Y0 + (J + .5f) / H * HCm;
            const float N1 = HallFbm(X / 260.f, Y / 260.f, 5, 3);
            const float N2 = HallFbm(X / 48.f + 13.f, Y / 48.f, 4, 7);
            const float N3 = HallNoise(X / 2.2f, Y / 2.2f, 11);
            const float N4 = HallFbm(X / 14.f, Y / 14.f, 3, 29);
            FLinearColor C = FMath::Lerp(Dark, Mid, FMath::Clamp(N1 * 1.2f + (N2 - .5f) * .4f - .08f, 0.f, 1.f));
            C = FMath::Lerp(C, Light, HallSmooth(.62f, .9f, N2) * .35f);
            // Thin chalky film from dried cleaner: a broad haze, mop streaks and speckles rather than blotches.
            const float R0 = HallFbm(X / 140.f + 5.f, Y / 70.f - 3.f, 5, 17) + (N4 - .5f) * .35f;
            float Res = .15f + HallSmooth(.5f, .82f, R0) * .38f * (.55f + .45f * N3);
            Res += HallSmooth(.93f, .99f, HallNoise(X / 1.1f, Y / 1.1f, 23)) * .22f;
            const float Edge = FMath::Min(FMath::Min(X - X0, X0 + WCm - X), FMath::Min(Y - Y0, Y0 + HCm - Y));
            const float EdgeDirt = 1.f - HallSmooth(0.f, 45.f, Edge);
            C = FMath::Lerp(C, Residue, FMath::Clamp(Res, 0.f, .55f));
            C *= 1.f - EdgeDirt * .3f;
            C *= .93f + N3 * .1f + (N4 - .5f) * .08f;
            Img.At(I, J) = C;
        }
    auto ToPx = [&](const FVector2D& P) { return FVector2D((P.X - X0) / WCm * W, (P.Y - Y0) / HCm * H); };
    FRandomStream R(1981);
    // Rubber scuffs and chair drags.
    for (int32 K = 0; K < 2600; ++K)
    {
        const FVector2D Start(R.FRandRange(X0, X0 + WCm), R.FRandRange(Y0, Y0 + HCm));
        const float Ang = R.FRandRange(0.f, 2.f * PI), Len = R.FRandRange(6.f, 55.f), Curve = R.FRandRange(-.4f, .4f);
        TArray<FVector2D> Pts;
        for (int32 S = 0; S <= 6; ++S)
        {
            const float T = S / 6.f;
            const float A = Ang + Curve * T;
            Pts.Add(ToPx(Start + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Len * T));
        }
        const bool bDark = R.FRand() < .62f;
        Img.Stroke(Pts, R.FRandRange(.5f, 1.6f), bDark ? FLinearColor(.02f, .015f, .014f) : FLinearColor(.42f, .40f, .38f), bDark ? R.FRandRange(.15f, .45f) : R.FRandRange(.1f, .3f));
    }
    // Hairline cracks.
    for (int32 K = 0; K < 9; ++K)
    {
        FVector2D P(R.FRandRange(X0 + 100, X0 + WCm - 100), R.FRandRange(Y0 + 100, Y0 + HCm - 100));
        float A = R.FRandRange(0.f, 2.f * PI);
        TArray<FVector2D> Pts{ ToPx(P) };
        const int32 Steps = R.RandRange(20, 60);
        for (int32 S = 0; S < Steps; ++S)
        {
            A += R.FRandRange(-.5f, .5f);
            P += FVector2D(FMath::Cos(A), FMath::Sin(A)) * R.FRandRange(4.f, 9.f);
            Pts.Add(ToPx(P));
        }
        Img.Stroke(Pts, 1.4f, FLinearColor(.36f, .34f, .33f), .55f);
        Img.Stroke(Pts, .7f, FLinearColor(.015f, .01f, .01f), .8f);
    }
    // Spill marks.
    for (int32 K = 0; K < 70; ++K)
    {
        const FVector2D P = ToPx(FVector2D(R.FRandRange(X0, X0 + WCm), R.FRandRange(Y0, Y0 + HCm)));
        const float S = R.FRandRange(6.f, 40.f);
        Img.Ellipse(P, FVector2D(S * W / WCm, S * R.FRandRange(.5f, 1.f) * H / HCm), R.FRandRange(0.f, 180.f), FLinearColor(.03f, .02f, .017f), R.FRandRange(.06f, .18f));
    }
}

// ----------------------------------------------------------------------------- stage planks (300 x 300 cm tile, planks along u)
void HallBuildWood(FHallImage& Img, TArray<float>& Height)
{
    const int32 W = Img.W, H = Img.H;
    const float Period = 300.f, PlankW = 14.f;
    const int32 Rows = FMath::RoundToInt(Period / PlankW);
    const float RowH = Period / Rows;
    Height.SetNumZeroed(W * H);
    for (int32 J = 0; J < H; ++J)
        for (int32 I = 0; I < W; ++I)
        {
            const float U = (I + .5f) / W * Period, V = (J + .5f) / H * Period;
            const int32 Row = FMath::FloorToInt(V / RowH);
            const float RV = V - Row * RowH;
            // Two butt joints per row, at row-specific offsets (wraps with the tile).
            const float Off = HallRand01(Row, 1, 77) * Period;
            const float L1 = FMath::Fmod(U + Off, Period);
            const float JointU = FMath::Min(FMath::Min(L1, Period * .5f - FMath::Abs(L1 - Period * .5f)), FMath::Abs(L1 - Period * .5f));
            const int32 PlankId = Row * 2 + (L1 < Period * .5f ? 0 : 1);
            const float Seam = FMath::Min(HallSmooth(0.f, .45f, RV), HallSmooth(0.f, .45f, RowH - RV));
            const float End = HallSmooth(0.f, .35f, JointU);
            const float Tone = .78f + HallRand01(PlankId, 5, 31) * .44f;
            const float Grain = HallPNoise((I + .5f) / W * 6, (J + .5f) / H * 160 + PlankId * 3.1f, 6, 160, 41);
            const float Fine = HallPNoise((I + .5f) / W * 40, (J + .5f) / H * 640, 40, 640, 43);
            FLinearColor C = FLinearColor(0.125f, 0.060f, 0.030f) * Tone * (.82f + Grain * .3f) * (.93f + Fine * .14f);
            C *= .35f + .65f * Seam * End;
            Img.At(I, J) = C;
            Height[J * W + I] = Seam * End * (1.f + (Fine - .5f) * .08f);
        }
}

// ----------------------------------------------------------------------------- grimy white paint for doors/frames (u across, v top->bottom)
void HallBuildGrime(FHallImage& Img, float Amount, int32 Seed)
{
    const int32 W = Img.W, H = Img.H;
    const FLinearColor White(0.60f, 0.59f, 0.56f), Dirt(0.16f, 0.145f, 0.12f);
    for (int32 J = 0; J < H; ++J)
        for (int32 I = 0; I < W; ++I)
        {
            const float U = (I + .5f) / W, V = (J + .5f) / H;
            const float N = HallFbm(U * 6.f, V * 14.f, 5, Seed);
            const float NF = HallNoise(U * 70.f, V * 150.f, Seed + 3);
            float G = HallSmooth(1.f - .28f * Amount, 1.f, V) * (.35f + .65f * N) * Amount;
            G += HallSmooth(.6f, .85f, N) * .12f * Amount;
            G += HallSmooth(.82f, 1.f, NF) * .05f;
            FLinearColor C = FMath::Lerp(White * (.96f + N * .06f), Dirt, FMath::Clamp(G, 0.f, .9f));
            Img.At(I, J) = C;
        }
    FRandomStream R(Seed);
    // Kick marks at the bottom.
    for (int32 K = 0; K < int32(60 * Amount); ++K)
    {
        const FVector2D P(R.FRandRange(0.f, (float)W), H * R.FRandRange(.88f, .995f));
        const float L = R.FRandRange(4.f, 30.f) * W / 100.f;
        Img.Stroke({ P, P + FVector2D(L, R.FRandRange(-3.f, 3.f)) }, R.FRandRange(1.f, 3.f), FLinearColor(.02f, .018f, .016f), R.FRandRange(.25f, .6f));
    }
    // Hand smudges around the latch side and push zone.
    for (int32 K = 0; K < int32(45 * Amount); ++K)
    {
        const FVector2D P(W * R.FRandRange(.55f, .98f), H * R.FRandRange(.35f, .62f));
        Img.Ellipse(P, FVector2D(R.FRandRange(4.f, 14.f), R.FRandRange(6.f, 18.f)), R.FRandRange(0.f, 180.f), FLinearColor(.2f, .18f, .15f), R.FRandRange(.05f, .14f));
    }
    // Faint drip streaks.
    for (int32 K = 0; K < 14; ++K)
    {
        const float X = R.FRandRange(0.f, (float)W), Y0 = H * R.FRandRange(0.f, .7f);
        Img.Stroke({ FVector2D(X, Y0), FVector2D(X + R.FRandRange(-2.f, 2.f), Y0 + H * R.FRandRange(.05f, .25f)) }, R.FRandRange(1.f, 2.5f), FLinearColor(.35f, .33f, .3f), .12f);
    }
}

void HallBuildEquipmentSign(FHallImage& Img)
{
    Img.Init(512, 256, FLinearColor(0.72f, 0.72f, 0.70f));
    const FLinearColor Red(0.42f, 0.012f, 0.012f);
    Img.Rect(FVector2D(6, 6), FVector2D(506, 9), Red, .9f); Img.Rect(FVector2D(6, 247), FVector2D(506, 250), Red, .9f);
    auto Line = [&](const FString& T, float Y, float H, float Stroke)
    {
        FHallTextStyle S; S.Height = H; S.Stroke = Stroke; S.WidthScale = .66f; S.Spacing = .2f;
        const float Width = HallTextWidth(T, S);
        HallDrawText(Img, T, FVector2D(256 - Width * .5f, Y), S, Red);
    };
    Line(TEXT("ELECTRONIC EQUIPMENT ONLY"), 64, 30, 5.2f);
    Line(TEXT("NO STORAGE OF COMBUSTIBLE"), 122, 20, 3.0f);
    Line(TEXT("MATERIALS BY ORDER OF"), 166, 20, 3.0f);
    Line(TEXT("STATE FIRE MARSHAL"), 210, 20, 3.0f);
}

void HallBuildRestroomSign(FHallImage& Img)
{
    Img.Init(256, 256, FLinearColor(0.025f, 0.026f, 0.03f));
    const FLinearColor White(0.8f, 0.8f, 0.8f);
    // Two simple pictograms divided by a bar.
    Img.Ellipse(FVector2D(76, 58), FVector2D(17, 17), 0, White);
    Img.FillPolygon({ {50, 84}, {102, 84}, {106, 150}, {46, 150} }, White);
    Img.Rect(FVector2D(54, 150), FVector2D(70, 214), White); Img.Rect(FVector2D(82, 150), FVector2D(98, 214), White);
    Img.Rect(FVector2D(124, 40), FVector2D(132, 216), White);
    Img.Ellipse(FVector2D(180, 58), FVector2D(17, 17), 0, White);
    Img.FillPolygon({ {166, 84}, {194, 84}, {218, 168}, {142, 168} }, White);
    Img.Rect(FVector2D(160, 168), FVector2D(174, 214), White); Img.Rect(FVector2D(186, 168), FVector2D(200, 214), White);
}

void HallBuildVent(FHallImage& Img, TArray<float>& Height)
{
    Img.Init(256, 128, FLinearColor(.6f, .6f, .58f));
    Height.SetNumZeroed(256 * 128);
    for (int32 J = 0; J < 128; ++J)
        for (int32 I = 0; I < 256; ++I)
        {
            const bool bFrame = I < 12 || I > 243 || J < 12 || J > 115;
            const float L = FMath::Fmod(J - 12.f, 9.f) / 9.f;
            const float Louver = bFrame ? 1.f : (L < .75f ? 1.f - L * .9f : 0.f);
            Img.At(I, J) = bFrame ? FLinearColor(.62f, .62f, .6f) : FLinearColor(.6f, .6f, .58f) * (.15f + .85f * Louver);
            Height[J * 256 + I] = bFrame ? 1.f : (L < .75f ? L : 0.f);
        }
}
}


// ----------------------------------------------------------------------------- wall wear atlas (tile-aligned variants)
namespace
{
const FLinearColor HallBareBlock(0.15f, 0.146f, 0.137f);
const FLinearColor HallOldCoat(0.068f, 0.067f, 0.064f);
const FLinearColor HallPaintBlack(0.03f, 0.03f, 0.032f);

/** Paint knocked off down to the block: ragged rim of an older coat, porous grey block inside, crisp dark paint edge. */
void HallWearChip(FHallImage& Paint, FHallImage& Height, const FVector2D& C, float Rad, FRandomStream& R)
{
    const int32 N = 16 + R.RandRange(0, 8);
    const float Rot = R.FRandRange(0.f, 2.f * PI), Stretch = R.FRandRange(.65f, 1.45f);
    const float P1 = R.FRandRange(0.f, 2.f * PI), P2 = R.FRandRange(0.f, 2.f * PI);
    TArray<FVector2D> Outer, Inner;
    for (int32 K = 0; K < N; ++K)
    {
        const float A = Rot + 2 * PI * K / N;
        const float Wave = 1.f + .22f * FMath::Sin(A * 2.f + P1) + .14f * FMath::Sin(A * 3.f + P2);
        const float Rr = Rad * Wave * R.FRandRange(.7f, 1.12f);
        const FVector2D D(FMath::Cos(A) * Stretch, FMath::Sin(A) / Stretch);
        Outer.Add(C + D * Rr);
        Inner.Add(C + D * Rr * R.FRandRange(.6f, .84f));
    }
    TArray<FVector2D> Ring = Outer; Ring.Add(Outer[0]);
    Paint.FillPolygon(Outer, HallOldCoat * R.FRandRange(.85f, 1.15f));
    Paint.FillPolygon(Inner, HallBareBlock * R.FRandRange(.8f, 1.15f));
    Paint.Stroke(Ring, 1.4f, FLinearColor(.006f, .006f, .007f), .6f);
    // Pores and grit of the exposed block.
    const int32 Pores = FMath::Clamp(int32(Rad * Rad * .05f), 1, 60);
    for (int32 K = 0; K < Pores; ++K)
    {
        const float A = R.FRandRange(0.f, 2.f * PI), D = Rad * .6f * FMath::Sqrt(R.FRand());
        const FVector2D Q = C + FVector2D(FMath::Cos(A) * Stretch, FMath::Sin(A) / Stretch) * D;
        if (R.FRand() < .7f) Paint.Ellipse(Q, FVector2D(R.FRandRange(.5f, 1.3f)), 0, FLinearColor(.035f, .034f, .032f), .8f);
        else Paint.Ellipse(Q, FVector2D(R.FRandRange(.4f, .9f)), 0, HallBareBlock * 1.35f, .6f);
    }
    Height.FillPolygon(Outer, FLinearColor(.5f - .14f, 0, 0));
    Height.FillPolygon(Inner, FLinearColor(.5f - .34f, 0, 0));
    // A fleck of paint left inside larger chips.
    if (Rad > 14.f && R.FRand() < .6f)
    {
        const FVector2D I = C + FVector2D(R.FRandRange(-.3f, .3f), R.FRandRange(-.3f, .3f)) * Rad;
        Paint.Ellipse(I, FVector2D(Rad * .18f, Rad * .12f), R.FRandRange(0.f, 180.f), FLinearColor(.02f, .021f, .023f));
        Height.Ellipse(I, FVector2D(Rad * .18f, Rad * .12f), 0, FLinearColor(.5f, 0, 0));
    }
    // Small satellite flakes along the rim.
    const int32 Flakes = R.RandRange(1, 4);
    for (int32 K = 0; K < Flakes; ++K)
    {
        const float A = R.FRandRange(0.f, 2.f * PI);
        const FVector2D Q = C + FVector2D(FMath::Cos(A) * Stretch, FMath::Sin(A) / Stretch) * Rad * R.FRandRange(1.15f, 1.6f);
        const FVector2D Fr(R.FRandRange(.8f, 2.4f), R.FRandRange(.6f, 1.6f));
        Paint.Ellipse(Q, Fr, R.FRandRange(0.f, 180.f), HallOldCoat * 1.3f, .9f);
        Height.Ellipse(Q, Fr, 0, FLinearColor(.5f - .2f, 0, 0));
    }
}

/** A point on a mortar joint of the 2 x 4 block tile (pixel space, cell-local). */
FVector2D HallJointPoint(int32 Size, FRandomStream& R)
{
    const float Course = Size / 4.f, Block = Size / 2.f;
    const int32 C = R.RandRange(0, 3);
    if (R.FRand() < .55f) return FVector2D(R.FRandRange(0.f, (float)Size), C * Course + (R.FRand() < .5f ? 0.f : Course) + R.FRandRange(-2.f, 2.f));
    const float Off = (C & 1) ? -Block * .5f : 0.f;
    const float X = Block * R.RandRange(0, 2) + Off + R.FRandRange(-2.f, 2.f);
    return FVector2D(FMath::Fmod(X + Size, (float)Size), C * Course + R.FRandRange(.1f, .9f) * Course);
}

void HallPaintWearCell(FHallImage& Paint, FHallImage& Height, int32 X0, int32 Y0, int32 Size, int32 Variant, int32 Seed)
{
    FRandomStream R(Seed);
    const float M = 22.f, Ppc = Size / Cmu;
    auto In = [&](float Margin) { return FVector2D(X0 + R.FRandRange(M + Margin, Size - M - Margin), Y0 + R.FRandRange(M + Margin, Size - M - Margin)); };
    auto Clamp = [&](FVector2D P, float Margin) { return FVector2D(FMath::Clamp((float)P.X, X0 + M + Margin, X0 + Size - M - Margin), FMath::Clamp((float)P.Y, Y0 + M + Margin, Y0 + Size - M - Margin)); };
    auto Chips = [&](int32 Count, float MinCm, float MaxCm)
    {
        for (int32 K = 0; K < Count; ++K)
        {
            const float Rad = R.FRandRange(MinCm, MaxCm) * Ppc;
            const FVector2D P = Clamp(FVector2D(X0, Y0) + HallJointPoint(Size, R), Rad);
            HallWearChip(Paint, Height, P, Rad, R);
        }
    };
    auto Gouge = [&]()
    {
        const float Len = R.FRandRange(8.f, 28.f) * Ppc, W = R.FRandRange(.5f, 1.1f) * Ppc;
        FVector2D P = In(Len * .5f);
        const float Ang = R.FRandRange(-.25f, .25f);
        TArray<FVector2D> Pts;
        for (int32 K = 0; K <= 4; ++K) Pts.Add(P + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * (Len * (K / 4.f - .5f)) + FVector2D(0, R.FRandRange(-1.5f, 1.5f)));
        Paint.Stroke(Pts, W + 3.f, FLinearColor(.16f, .155f, .15f), .35f);
        Paint.Stroke(Pts, W, HallBareBlock * .8f);
        Height.Stroke(Pts, W, FLinearColor(.5f - .45f, 0, 0));
    };
    auto Scuffs = [&](int32 Count)
    {
        for (int32 K = 0; K < Count; ++K)
        {
            const float Len = R.FRandRange(10.f, 35.f) * Ppc;
            const FVector2D P = In(10.f);
            const float Ang = R.FRandRange(-.3f, .3f);
            const FVector2D D(FMath::Cos(Ang), FMath::Sin(Ang));
            Paint.Stroke({ P - D * Len * .5f, P + FVector2D(0, R.FRandRange(-4.f, 4.f)), P + D * Len * .5f }, R.FRandRange(2.f, 6.f) * Ppc, FLinearColor(.07f, .068f, .065f), R.FRandRange(.12f, .28f));
        }
    };
    auto Clumps = [&](int32 Count, float DripChance)
    {
        for (int32 K = 0; K < Count; ++K)
        {
            const float Rad = R.FRandRange(.6f, 2.2f) * Ppc;
            const FVector2D P = In(Rad + 4.f);
            const FVector2D Rr(Rad * R.FRandRange(.8f, 1.3f), Rad * R.FRandRange(.7f, 1.1f));
            const float Spin = R.FRandRange(0.f, 180.f);
            Paint.Ellipse(P + FVector2D(Rad * .18f, Rad * .45f), Rr * 1.05f, Spin, FLinearColor(.003f, .003f, .003f), .55f);   // shadow under the blob
            Paint.Ellipse(P, Rr, Spin, HallPaintBlack);
            Paint.Ellipse(P - FVector2D(Rad * .28f, Rad * .32f), Rr * .42f, Spin, FLinearColor(.12f, .12f, .125f), .55f);     // catch-light on top
            Height.Ellipse(P, Rr, 0, FLinearColor(.5f + .6f, 0, 0));
            Height.Ellipse(P - FVector2D(Rad * .1f, Rad * .1f), Rr * .6f, 0, FLinearColor(.5f + 1.2f, 0, 0));
            if (R.FRand() < DripChance)
            {
                const float Len = FMath::Min(R.FRandRange(3.f, 16.f) * Ppc, float(Y0 + Size - M) - (float)P.Y - 4.f);
                if (Len < 6.f) continue;
                FVector2D A = P, B = P + FVector2D(R.FRandRange(-1.f, 1.f), Len * .5f), E = P + FVector2D(R.FRandRange(-1.5f, 1.5f), Len);
                Paint.Stroke({ A + FVector2D(1.2f, 0), B + FVector2D(1.2f, 0), E + FVector2D(1.2f, 1.5f) }, Rad * .8f, FLinearColor(.003f, .003f, .003f), .45f);
                Paint.Stroke({ A, B }, Rad * .9f, HallPaintBlack); Paint.Stroke({ B, E }, Rad * .55f, HallPaintBlack);
                Paint.Ellipse(E, FVector2D(Rad * .45f, Rad * .6f), 0, HallPaintBlack);
                Paint.Stroke({ B - FVector2D(Rad * .2f, 0), E - FVector2D(Rad * .15f, Rad * .3f) }, FMath::Max(1.f, Rad * .18f), FLinearColor(.1f, .1f, .105f), .45f);
                Paint.Ellipse(E - FVector2D(Rad * .15f, Rad * .2f), FVector2D(Rad * .16f, Rad * .22f), 0, FLinearColor(.12f, .12f, .125f), .5f);
                Height.Stroke({ A, B }, Rad * .9f, FLinearColor(.5f + .5f, 0, 0)); Height.Stroke({ B, E }, Rad * .55f, FLinearColor(.5f + .45f, 0, 0));
                Height.Ellipse(E, FVector2D(Rad * .45f, Rad * .6f), 0, FLinearColor(.5f + .6f, 0, 0));
            }
        }
    };
    auto Worn = [&](int32 Count, float MinCm, float MaxCm)
    {
        // Paint rubbed thin: soft, mottled grey showing through, with fine horizontal scratches.
        for (int32 K = 0; K < Count; ++K)
        {
            const float Rad = R.FRandRange(MinCm, MaxCm) * Ppc;
            const FVector2D C = In(FMath::Min(Rad * .6f, Size * .5f - M - 4.f));
            const float Squash = R.FRandRange(.45f, .8f);
            for (int32 J = 0; J < 140; ++J)
            {
                const float A = R.FRandRange(0.f, 2.f * PI), D = Rad * FMath::Pow(R.FRand(), .8f);
                const FVector2D Q = Clamp(C + FVector2D(FMath::Cos(A), FMath::Sin(A) * Squash) * D, 3.f);
                const float Fall = 1.f - D / Rad;
                Paint.Ellipse(Q, FVector2D(R.FRandRange(2.f, 7.f), R.FRandRange(1.5f, 4.f)), R.FRandRange(-15.f, 15.f), FLinearColor(.09f, .088f, .084f), R.FRandRange(.05f, .13f) * (.35f + Fall));
            }
            for (int32 J = 0; J < 10; ++J)
            {
                const FVector2D Q = Clamp(C + FVector2D(R.FRandRange(-Rad, Rad), R.FRandRange(-Rad, Rad) * Squash), 6.f);
                const float Len = R.FRandRange(4.f, 16.f) * Ppc;
                Paint.Stroke({ Q, Clamp(Q + FVector2D(Len, R.FRandRange(-2.f, 2.f)), 3.f) }, R.FRandRange(.6f, 1.2f), FLinearColor(.11f, .108f, .1f), R.FRandRange(.15f, .35f));
            }
        }
    };
    auto Tape = [&](int32 Count)
    {
        for (int32 K = 0; K < Count; ++K)
        {
            const FVector2D C = In(50.f);
            const float W = R.FRandRange(6.f, 13.f) * Ppc * .5f, H = R.FRandRange(2.5f, 4.f) * Ppc * .5f, A = R.FRandRange(-.6f, .6f);
            const FVector2D U(FMath::Cos(A), FMath::Sin(A)), V(-FMath::Sin(A), FMath::Cos(A));
            TArray<FVector2D> Poly;
            for (int32 E = 0; E <= 4; ++E) Poly.Add(C - U * W + V * (H * (E / 2.f - 1.f)) + U * R.FRandRange(-2.f, 2.f));
            for (int32 E = 0; E <= 4; ++E) Poly.Add(C + U * W + V * (H * (1.f - E / 2.f)) + U * R.FRandRange(-2.f, 2.f));
            TArray<FVector2D> Ring = Poly; Ring.Add(Poly[0]);
            Paint.Stroke(Ring, 2.f, FLinearColor(.08f, .06f, .035f), .6f);
            Paint.FillPolygon(Poly, FLinearColor(.5f, .43f, .29f) * R.FRandRange(.75f, 1.05f), .95f);
            Height.FillPolygon(Poly, FLinearColor(.62f, 0, 0));
        }
    };
    auto Holes = [&](int32 Count)
    {
        for (int32 K = 0; K < Count; ++K)
        {
            const FVector2D P = In(8.f);
            const float Rad = R.FRandRange(1.8f, 3.2f);
            HallWearChip(Paint, Height, P, Rad * 2.4f, R);
            Paint.Ellipse(P, FVector2D(Rad), 0, FLinearColor(.004f, .004f, .004f));
            Height.Ellipse(P, FVector2D(Rad), 0, FLinearColor(.5f - .8f, 0, 0));
        }
    };
    auto Water = [&]()
    {
        for (int32 K = 0; K < 9; ++K)
        {
            const float X = X0 + R.FRandRange(M + 10.f, Size - M - 10.f), Top = Y0 + Size - M - R.FRandRange(60.f, 300.f);
            Paint.Stroke({ FVector2D(X, Y0 + Size - M), FVector2D(X + R.FRandRange(-6.f, 6.f), (Y0 + Size - M + Top) * .5f), FVector2D(X + R.FRandRange(-8.f, 8.f), Top) },
                R.FRandRange(10.f, 34.f), FLinearColor(.1f, .095f, .088f), R.FRandRange(.08f, .16f));
        }
        for (int32 K = 0; K < 260; ++K)
        {
            const FVector2D P(X0 + R.FRandRange(M, Size - M), Y0 + Size - M - FMath::Pow(R.FRand(), 2.5f) * 120.f);
            Paint.Ellipse(P, FVector2D(R.FRandRange(.6f, 1.6f)), 0, FLinearColor(.16f, .155f, .145f), R.FRandRange(.12f, .3f));
        }
    };
    auto Peel = [&]()
    {
        const float Rad = R.FRandRange(13.f, 22.f) * Ppc;
        const FVector2D C = In(Rad);
        HallWearChip(Paint, Height, C, Rad, R);
        for (int32 K = 0; K < 9; ++K)
        {
            const float A = R.FRandRange(0.f, 2.f * PI);
            HallWearChip(Paint, Height, Clamp(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rad * R.FRandRange(.8f, 1.3f), 10.f), R.FRandRange(1.f, 4.f) * Ppc, R);
        }
        for (int32 K = 0; K < 5; ++K)
        {
            const FVector2D I = C + FVector2D(R.FRandRange(-.5f, .5f), R.FRandRange(-.5f, .5f)) * Rad;
            const FVector2D Rr(R.FRandRange(4.f, 12.f), R.FRandRange(3.f, 9.f));
            Paint.Ellipse(I, Rr, R.FRandRange(0.f, 180.f), FLinearColor(.02f, .021f, .023f));
            Height.Ellipse(I, Rr, 0, FLinearColor(.5f, 0, 0));
        }
    };
    switch (Variant)
    {
    case 0: Worn(1, 8.f, 14.f); Scuffs(7); Gouge(); Gouge(); Chips(5, .6f, 2.5f); break;
    case 1: Chips(14, .6f, 3.6f); Chips(3, 3.5f, 6.5f); Scuffs(2); break;
    case 2: Clumps(12, .5f); Chips(3, .5f, 2.f); break;
    case 3: Worn(2, 10.f, 18.f); Scuffs(4); Chips(3, .5f, 1.8f); break;
    case 4: Tape(3); Holes(6); Chips(4, .5f, 1.6f); break;
    case 5: Water(); Worn(1, 7.f, 12.f); Chips(5, .6f, 2.4f); Scuffs(2); break;
    case 6: Peel(); Chips(4, .5f, 2.f); break;
    case 7: Clumps(18, .65f); Scuffs(2); Gouge(); break;
    default: Worn(1, 6.f, 11.f); Chips(8, .5f, 3.f); Clumps(6, .45f); Gouge(); Holes(2); break;
    }
}
}

void HallCreateWearAtlas(UHouseBlackHallSubsystem* Sub)
{
    if (!Sub->CmuBase.IsValid() || !Sub->CmuNormal.IsValid()) return;
    const int32 Tile = Sub->CmuBase->W, Grid = HallWear::Grid, Size = Tile * Grid;
    FHallImage Base; Base.Init(Size, Size, FLinearColor::Black);
    FHallImage Paint; Paint.Init(Size, Size, FLinearColor(0, 0, 0, 0));
    FHallImage Height; Height.Init(Size, Size, FLinearColor(.5f, 0, 0, 1));
    for (int32 V = 0; V < Grid * Grid; ++V)
        HallPaintWearCell(Paint, Height, (V % Grid) * Tile, (V / Grid) * Tile, Tile, HallWear::CellTypes[V], 7001 + V * 131);
    FHallImage Normal; Normal.Init(Size, Size, FLinearColor(.5f, .5f, 1.f));
    for (int32 Y = 0; Y < Size; ++Y)
        for (int32 X = 0; X < Size; ++X)
        {
            const int32 TX = X % Tile, TY = Y % Tile;
            FLinearColor B = Sub->CmuBase->At(TX, TY);
            const FLinearColor& Pp = Paint.At(X, Y);
            if (Pp.A > .001f)
            {
                const float Shade = FMath::Clamp(B.GetLuminance() / 0.021f, .55f, 1.25f);
                const FLinearColor Col = (Pp / FMath::Max(Pp.A, 1e-3f)) * (.8f + .2f * Shade);
                B = FMath::Lerp(B, Col, FMath::Clamp(Pp.A, 0.f, 1.f)); B.A = 1.f;
            }
            Base.At(X, Y) = B;
            const FLinearColor N0 = Sub->CmuNormal->At(TX, TY);
            const float HX = (Height.At(FMath::Min(X + 1, Size - 1), Y).R - Height.At(FMath::Max(X - 1, 0), Y).R) * .5f;
            const float HY = (Height.At(X, FMath::Min(Y + 1, Size - 1)).R - Height.At(X, FMath::Max(Y - 1, 0)).R) * .5f;
            const FVector N = FVector(N0.R * 2 - 1 - HX * 2.4f, N0.G * 2 - 1 - HY * 2.4f, FMath::Max(.2f, N0.B * 2 - 1)).GetSafeNormal();
            Normal.At(X, Y) = FLinearColor(N.X * .5f + .5f, N.Y * .5f + .5f, N.Z * .5f + .5f, 1.f);
        }
    UTexture2D* BaseT = HallMakeTexture(Base, true, false, TEXT("T_BH_WallWear"), false);
    UTexture2D* NormalT = HallMakeTexture(Normal, false, true, TEXT("T_BH_WallWearN"), false);
    Sub->Textures.Add(TEXT("WallWear"), BaseT);
    HallMakeTextured(Sub, TEXT("WallWear"), BaseT, NormalT, .5f, FLinearColor::White, 1.f);
}

// =============================================================================================
void HallPaintXXVII(FHallImage& Img, const FVector2D& Center, float H, const FLinearColor& Color, float WidthScale)
{
    // Glyph outlines in cap-height units (x right, y up), Roman serif construction.
    auto X = [&](float OX, TArray<TArray<FVector2D>>& Out)
    {
        Out.Add({ {OX + .03f, 1.f}, {OX + .21f, 1.f}, {OX + .70f, 0.f}, {OX + .52f, 0.f} });
        Out.Add({ {OX + .61f, 1.f}, {OX + .68f, 1.f}, {OX + .11f, 0.f}, {OX + .04f, 0.f} });
        Out.Add({ {OX - .04f, .965f}, {OX + .30f, .965f}, {OX + .30f, 1.f}, {OX - .04f, 1.f} });
        Out.Add({ {OX + .50f, .965f}, {OX + .79f, .965f}, {OX + .79f, 1.f}, {OX + .50f, 1.f} });
        Out.Add({ {OX - .06f, 0.f}, {OX + .22f, 0.f}, {OX + .22f, .035f}, {OX - .06f, .035f} });
        Out.Add({ {OX + .42f, 0.f}, {OX + .78f, 0.f}, {OX + .78f, .035f}, {OX + .42f, .035f} });
        return .74f;
    };
    auto V = [&](float OX, TArray<TArray<FVector2D>>& Out)
    {
        Out.Add({ {OX + .03f, 1.f}, {OX + .22f, 1.f}, {OX + .41f, 0.f}, {OX + .34f, 0.f} });
        Out.Add({ {OX + .60f, 1.f}, {OX + .66f, 1.f}, {OX + .42f, 0.f}, {OX + .37f, 0.f} });
        Out.Add({ {OX - .04f, .965f}, {OX + .30f, .965f}, {OX + .30f, 1.f}, {OX - .04f, 1.f} });
        Out.Add({ {OX + .50f, .965f}, {OX + .76f, .965f}, {OX + .76f, 1.f}, {OX + .50f, 1.f} });
        return .72f;
    };
    auto I = [&](float OX, TArray<TArray<FVector2D>>& Out)
    {
        Out.Add({ {OX + .075f, 0.f}, {OX + .245f, 0.f}, {OX + .245f, 1.f}, {OX + .075f, 1.f} });
        Out.Add({ {OX - .01f, .965f}, {OX + .33f, .965f}, {OX + .33f, 1.f}, {OX - .01f, 1.f} });
        Out.Add({ {OX - .01f, 0.f}, {OX + .33f, 0.f}, {OX + .33f, .035f}, {OX - .01f, .035f} });
        return .32f;
    };
    TArray<TArray<FVector2D>> Shapes;
    float Cursor = 0;
    Cursor += X(Cursor, Shapes) + .08f;
    Cursor += X(Cursor, Shapes) + .08f;
    Cursor += V(Cursor, Shapes) + .08f;
    Cursor += I(Cursor, Shapes) + .08f;
    Cursor += I(Cursor, Shapes);
    const float Width = Cursor;
    for (TArray<FVector2D>& Shape : Shapes)
    {
        for (FVector2D& P : Shape) P = FVector2D(Center.X + (P.X - Width * .5f) * H * WidthScale, Center.Y + (.5f - P.Y) * H);
        Img.FillPolygon(Shape, Color);
    }
}

void HallPaintDog(FHallImage& Img, const FVector2D& C, float S, const FLinearColor& Body, const FLinearColor& Accent, int32 Pose, bool bMirror)
{
    const FLinearColor Ink(0.012f, 0.010f, 0.010f);
    const FLinearColor Shade = Body * .62f, Belly = FMath::Lerp(Body, FLinearColor(.8f, .78f, .72f), .35f);
    auto P = [&](float X, float Y) { return FVector2D(C.X + (bMirror ? -X : X) * S, C.Y - Y * S); };
    auto Rot = [&](float Deg) { return bMirror ? -Deg : Deg; };
    // Pose tables: arm (elbow, paw) and leg (knee, foot) end points.
    const float ArmUp = (Pose % 3 == 0) ? .36f : (Pose % 3 == 1 ? .30f : .20f);
    const float Kick = (Pose % 2 == 0) ? 1.f : -.4f;
    struct FPart { int32 Kind; TArray<FVector2D> Pts; FVector2D Ctr, Rad; float RotDeg, Width; FLinearColor Col; };
    TArray<FPart> Parts;
    auto Limb = [&](TArray<FVector2D> Pts, float Width, const FLinearColor& Col) { Parts.Add({ 0, Pts, FVector2D(), FVector2D(), 0, Width, Col }); };
    auto Blob = [&](FVector2D Ctr, FVector2D Rad, float Deg, const FLinearColor& Col) { Parts.Add({ 1, {}, Ctr, Rad, Deg, 0, Col }); };
    Limb({ P(-.13f, -.10f), P(-.22f, -.03f), P(-.25f, .07f) }, .05f, Body);                                  // tail
    Limb({ P(.05f, -.15f), P(.13f + .07f * Kick, -.25f), P(.20f + .1f * Kick, -.36f + .08f * FMath::Max(0.f, Kick)) }, .10f, Shade); // back leg
    Blob(P(.24f + .1f * Kick, -.38f + .08f * FMath::Max(0.f, Kick)), FVector2D(.075f, .042f) * S, Rot(25 * Kick), Shade);
    Limb({ P(.10f, .15f), P(.20f, .10f + .05f * (Pose % 2)), P(.29f, .19f + .06f * (Pose % 2)) }, .085f, Shade);  // back arm
    Blob(P(.30f, .21f + .06f * (Pose % 2)), FVector2D(.048f, .048f) * S, 0, Shade);
    Blob(P(0.f, .02f), FVector2D(.15f, .22f) * S, Rot(8), Body);                                             // torso
    Blob(P(.03f, -.03f), FVector2D(.08f, .13f) * S, Rot(8), Belly);
    Limb({ P(-.06f, -.15f), P(-.10f, -.30f), P(-.08f, -.44f) }, .10f, Body);                                 // front leg
    Blob(P(-.04f, -.46f), FVector2D(.08f, .042f) * S, 0, Body);
    Limb({ P(-.08f, .14f), P(-.20f, ArmUp - .12f), P(-.24f, ArmUp) }, .085f, Body);                           // waving arm
    Blob(P(-.245f, ArmUp + .02f), FVector2D(.05f, .05f) * S, 0, Body);
    Blob(P(.04f, .33f), FVector2D(.15f, .125f) * S, 0, Body);                                                // head
    Blob(P(.17f, .30f), FVector2D(.095f, .065f) * S, Rot(-6), Belly);                                        // muzzle
    Blob(P(-.06f, .30f), FVector2D(.055f, .12f) * S, Rot(-18), Shade);                                       // ear
    // Outline pass, then colour pass.
    for (const FPart& Part : Parts)
    {
        if (Part.Kind == 0) Img.Stroke(Part.Pts, (Part.Width + .035f) * S, Ink);
        else Img.Ellipse(Part.Ctr, Part.Rad + FVector2D(.018f * S), Part.RotDeg, Ink);
    }
    for (const FPart& Part : Parts)
    {
        if (Part.Kind == 0) Img.Stroke(Part.Pts, Part.Width * S, Part.Col);
        else Img.Ellipse(Part.Ctr, Part.Rad, Part.RotDeg, Part.Col);
    }
    // Face details and neckerchief.
    Img.Ellipse(P(.255f, .325f), FVector2D(.034f, .027f) * S, 0, Ink);
    Img.Ellipse(P(.085f, .37f), FVector2D(.022f, .028f) * S, 0, Ink);
    Img.Ellipse(P(.091f, .378f), FVector2D(.007f, .008f) * S, 0, FLinearColor(.85f, .85f, .85f));
    Img.Stroke({ P(.13f, .27f), P(.17f, .255f), P(.21f, .262f) }, .014f * S, Ink);
    TArray<FVector2D> Kerchief{ P(-.09f, .215f), P(.13f, .215f), P(.03f, .12f) };
    Img.FillPolygon(Kerchief, Ink);
    TArray<FVector2D> KerchiefIn{ P(-.065f, .205f), P(.105f, .205f), P(.03f, .14f) };
    Img.FillPolygon(KerchiefIn, Accent);
}

void HallPaintTEmblem(FHallImage& Img, const FVector2D& C, float Ppc, int32 Variant)
{
    const FLinearColor Red(0.45f, 0.014f, 0.012f), White(0.74f, 0.74f, 0.72f);
    TArray<FBox2D> Rects; // cm, y up, centered
    const float Flip = Variant == 2 ? -1.f : 1.f;
    auto R = [&](float X0, float Y0, float X1, float Y1) { Rects.Add(FBox2D(FVector2D(X0, FMath::Min(Y0 * Flip, Y1 * Flip)), FVector2D(X1, FMath::Max(Y0 * Flip, Y1 * Flip)))); };
    R(-22.f, 23.f, 22.f, 34.f);         // bar
    R(-6.f, -34.f, 6.f, 23.5f);          // stem
    if (Variant == 1) R(-14.f, 3.f, 14.f, 10.f); // crossed T
    auto ToPx = [&](const FVector2D& P) { return FVector2D(C.X + P.X * Ppc, C.Y - P.Y * Ppc); };
    for (const FBox2D& B : Rects)
    {
        const FBox2D O = B.ExpandBy(2.2f);
        Img.Rect(ToPx(FVector2D(O.Min.X, O.Max.Y)), ToPx(FVector2D(O.Max.X, O.Min.Y)), White);
    }
    for (const FBox2D& B : Rects) Img.Rect(ToPx(FVector2D(B.Min.X, B.Max.Y)), ToPx(FVector2D(B.Max.X, B.Min.Y)), Red);
}

// =============================================================================================
void HallCreateLook(UHouseBlackHallSubsystem* Sub)
{
    // Block wall.
    TArray<float> Height;
    TSharedPtr<FHallImage> CmuBase = MakeShared<FHallImage>(), CmuNormal = MakeShared<FHallImage>();
    HallBuildCmu(*CmuBase, Height, 512);
    HallHeightToNormal(Height, 512, 512, 2.4f, true, *CmuNormal);
    Sub->CmuBase = CmuBase; Sub->CmuNormal = CmuNormal;
    UTexture2D* CmuB = HallMakeTexture(*CmuBase, true, false, TEXT("T_BH_Cmu"));
    UTexture2D* CmuN = HallMakeTexture(*CmuNormal, false, true, TEXT("T_BH_CmuN"));
    Sub->Textures.Add(TEXT("Cmu"), CmuB); Sub->Textures.Add(TEXT("CmuN"), CmuN);
    HallMakeTextured(Sub, TEXT("CMU"), CmuB, CmuN, .5f, FLinearColor::White, 1.f);
    HallCreateWearAtlas(Sub);

    // Floor (unique over hall + recess).
    FHallImage Floor; Floor.Init(2048, 1024, FLinearColor::Black);
    HallBuildFloor(Floor, XW, YS, 2524.f, 1584.f);
    UTexture2D* FloorT = HallMakeTexture(Floor, true, false, TEXT("T_BH_Floor"), false);
    Sub->Textures.Add(TEXT("Floor"), FloorT);
    HallMakeTextured(Sub, TEXT("Floor"), FloorT, nullptr, .42f, FLinearColor::White, 0.f);

    // Stage planks.
    FHallImage Wood; Wood.Init(1024, 512, FLinearColor::Black);
    TArray<float> WoodH; HallBuildWood(Wood, WoodH);
    FHallImage WoodN; HallHeightToNormal(WoodH, 1024, 512, 3.f, true, WoodN);
    UTexture2D* WoodT = HallMakeTexture(Wood, true, false, TEXT("T_BH_Wood"));
    UTexture2D* WoodNT = HallMakeTexture(WoodN, false, true, TEXT("T_BH_WoodN"));
    Sub->Textures.Add(TEXT("Wood"), WoodT); Sub->Textures.Add(TEXT("WoodN"), WoodNT);
    HallMakeTextured(Sub, TEXT("StageWood"), WoodT, WoodNT, .5f);
    HallMakeTextured(Sub, TEXT("StageFascia"), WoodT, WoodNT, .6f, FLinearColor(.55f, .5f, .48f));

    // Painted doors / frames.
    FHallImage G1; G1.Init(512, 1024, FLinearColor::White); HallBuildGrime(G1, .7f, 101);
    FHallImage G2; G2.Init(512, 1024, FLinearColor::White); HallBuildGrime(G2, 1.6f, 202);
    FHallImage G3; G3.Init(256, 1024, FLinearColor::White); HallBuildGrime(G3, .55f, 303);
    HallMakeTextured(Sub, TEXT("DoorWhite"), HallMakeTexture(G1, true, false, TEXT("T_BH_DoorGrime"), false), nullptr, .42f);
    HallMakeTextured(Sub, TEXT("ExitWhite"), HallMakeTexture(G2, true, false, TEXT("T_BH_ExitGrime"), false), nullptr, .45f);
    HallMakeTextured(Sub, TEXT("FrameWhite"), HallMakeTexture(G3, true, false, TEXT("T_BH_FrameGrime"), true), nullptr, .45f);

    FHallImage SignE; HallBuildEquipmentSign(SignE);
    HallMakeTextured(Sub, TEXT("SignEquipment"), HallMakeTexture(SignE, true, false, TEXT("T_BH_SignEquip"), false), nullptr, .35f);
    FHallImage SignR; HallBuildRestroomSign(SignR);
    HallMakeTextured(Sub, TEXT("SignRestroom"), HallMakeTexture(SignR, true, false, TEXT("T_BH_SignRestroom"), false), nullptr, .4f);
    FHallImage Vent; TArray<float> VentH; HallBuildVent(Vent, VentH);
    FHallImage VentN; HallHeightToNormal(VentH, 256, 128, 4.f, false, VentN);
    HallMakeTextured(Sub, TEXT("Vent"), HallMakeTexture(Vent, true, false, TEXT("T_BH_Vent"), false), HallMakeTexture(VentN, false, true, TEXT("T_BH_VentN"), false), .5f);

    // Plain finishes.
    HallMakePlain(Sub, TEXT("Baseboard"), FLinearColor(.6f, .6f, .58f), .38f);
    HallMakePlain(Sub, TEXT("MetalDark"), FLinearColor(.06f, .05f, .042f), .42f, .7f);
    HallMakePlain(Sub, TEXT("Aluminum"), FLinearColor(.55f, .55f, .56f), .35f, 1.f);
    HallMakePlain(Sub, TEXT("Chrome"), FLinearColor(.72f, .72f, .72f), .18f, 1.f);
    HallMakePlain(Sub, TEXT("KickBlack"), FLinearColor(.016f, .016f, .017f), .5f);
    HallMakePlain(Sub, TEXT("Conduit"), FLinearColor(.34f, .35f, .36f), .45f, .9f);
    HallMakePlain(Sub, TEXT("PlasticWhite"), FLinearColor(.66f, .66f, .64f), .4f);
    HallMakePlain(Sub, TEXT("PlasticBeige"), FLinearColor(.55f, .52f, .45f), .45f);
    HallMakePlain(Sub, TEXT("AlarmRed"), FLinearColor(.42f, .012f, .01f), .38f);
    HallMakePlain(Sub, TEXT("ExitGlow"), FLinearColor(.6f, .02f, .01f), .4f, 0.f, FLinearColor(48.f, 1.2f, .4f));
    HallMakePlain(Sub, TEXT("ExitFace"), FLinearColor(.04f, .004f, .004f), .3f, 0.f, FLinearColor(.5f, 0.f, 0.f));
    HallMakePlain(Sub, TEXT("StrobeLens"), FLinearColor(.8f, .8f, .8f), .2f, 0.f, FLinearColor(2.f, 2.f, 2.f));
    HallMakePlain(Sub, TEXT("CanGlow"), FLinearColor(.9f, .85f, .75f), .3f, 0.f, FLinearColor(30.f, 26.f, 20.f));
    HallMakePlain(Sub, TEXT("TapeBlack"), FLinearColor(.012f, .012f, .013f), .32f);
    HallMakePlain(Sub, TEXT("TapeBlue"), FLinearColor(.025f, .07f, .32f), .45f);
    HallMakePlain(Sub, TEXT("BucketOrange"), FLinearColor(.85f, .16f, .01f), .45f);
    HallMakePlain(Sub, TEXT("BucketInside"), FLinearColor(.25f, .05f, .005f), .6f);
    HallMakePlain(Sub, TEXT("PoleYellow"), FLinearColor(.85f, .55f, .0f), .4f);
    HallMakePlain(Sub, TEXT("RubberBlack"), FLinearColor(.02f, .02f, .02f), .7f);
    HallMakePlain(Sub, TEXT("PoleWhite"), FLinearColor(.65f, .66f, .68f), .35f, .5f);
    HallMakePlain(Sub, TEXT("GripBlue"), FLinearColor(.03f, .12f, .45f), .5f);
    HallMakePlain(Sub, TEXT("JugBlue"), FLinearColor(.04f, .2f, .55f), .25f);
    HallMakePlain(Sub, TEXT("CapWhite"), FLinearColor(.7f, .7f, .7f), .4f);
    HallMakePlain(Sub, TEXT("TableBoard"), FLinearColor(.42f, .43f, .45f), .6f);
    HallMakePlain(Sub, TEXT("TableTop"), FLinearColor(.02f, .05f, .16f), .4f);
    HallMakePlain(Sub, TEXT("TableFrame"), FLinearColor(.62f, .63f, .65f), .4f, .7f);
    HallMakePlain(Sub, TEXT("TableStrut"), FLinearColor(.02f, .1f, .45f), .4f, .3f);
    HallMakePlain(Sub, TEXT("LabelWhite"), FLinearColor(.75f, .74f, .7f), .5f);
    HallMakePlain(Sub, TEXT("ScreenBlack"), FLinearColor(.006f, .006f, .008f), .12f);
    HallMakePlain(Sub, TEXT("BezelBlack"), FLinearColor(.015f, .015f, .017f), .4f);
    HallMakePlain(Sub, TEXT("BootTan"), FLinearColor(.28f, .14f, .055f), .65f);
    HallMakePlain(Sub, TEXT("BootRubber"), FLinearColor(.025f, .03f, .05f), .55f);
    HallMakePlain(Sub, TEXT("DomeBlack"), FLinearColor(.01f, .01f, .012f), .1f);
    HallMakePlain(Sub, TEXT("Bristle"), FLinearColor(.5f, .38f, .05f), .8f);
    HallMakePlain(Sub, TEXT("Thermostat"), FLinearColor(.7f, .7f, .68f), .35f);
    HallMakePlain(Sub, TEXT("OutletFace"), FLinearColor(.5f, .49f, .46f), .4f);
    HallMakePlain(Sub, TEXT("ExteriorDark"), FLinearColor(.06f, .06f, .065f), .85f);

    // Litter: generic drink-can labels (no real brands), cups, paper, packaging.
    {
        struct FCanDesign { FLinearColor Base, Band, Text; const TCHAR* Word; };
        const FCanDesign Designs[5] = {
            { FLinearColor(.55f, .56f, .58f), FLinearColor(.03f, .12f, .55f), FLinearColor(.8f, .8f, .82f), TEXT("LIGHT") },
            { FLinearColor(.58f, .02f, .02f), FLinearColor(.75f, .74f, .72f), FLinearColor(.58f, .02f, .02f), TEXT("COLA") },
            { FLinearColor(.74f, .73f, .7f), FLinearColor(.6f, .42f, .1f), FLinearColor(.4f, .02f, .02f), TEXT("LAGER") },
            { FLinearColor(.02f, .02f, .025f), FLinearColor(.25f, .6f, .05f), FLinearColor(.8f, .8f, .8f), TEXT("SELTZER") },
            { FLinearColor(.55f, .4f, .12f), FLinearColor(.12f, .05f, .02f), FLinearColor(.75f, .6f, .3f), TEXT("ALE") },
        };
        for (int32 K = 0; K < 5; ++K)
        {
            const FCanDesign& D = Designs[K];
            FHallImage Label; Label.Init(512, 256, D.Base);
            const FLinearColor Silver(.6f, .61f, .63f);
            Label.Rect(FVector2D(0, 0), FVector2D(512, 16), Silver); Label.Rect(FVector2D(0, 240), FVector2D(512, 256), Silver);
            if (K == 1)
            {
                TArray<FVector2D> Wave;
                for (int32 X = 0; X <= 512; X += 8) Wave.Add(FVector2D(X, 150 + 26 * FMath::Sin(X / 512.f * 4 * PI)));
                Label.Stroke(Wave, 22.f, D.Band);
            }
            else
            {
                Label.Rect(FVector2D(0, 78), FVector2D(512, 178), D.Band);
                Label.Rect(FVector2D(0, 70), FVector2D(512, 74), D.Text); Label.Rect(FVector2D(0, 182), FVector2D(512, 186), D.Text);
            }
            FHallTextStyle T; T.Height = 46.f; T.Stroke = 8.f; T.WidthScale = .7f; T.Spacing = .2f;
            const float W = HallTextWidth(D.Word, T);
            for (const float U : { 128.f, 384.f }) HallDrawText(Label, D.Word, FVector2D(U - W * .5f, 152.f), T, D.Text);
            HallMakeTextured(Sub, *FString::Printf(TEXT("CanLabel%d"), K), HallMakeTexture(Label, true, false, *FString::Printf(TEXT("T_BH_CanLabel%d"), K)), nullptr, .3f, FLinearColor::White, 0.f, .55f);
        }
        HallMakePlain(Sub, TEXT("CanTop"), FLinearColor(.62f, .62f, .64f), .28f, 1.f);
        HallMakePlain(Sub, TEXT("CupRed"), FLinearColor(.55f, .012f, .012f), .35f);
        HallMakePlain(Sub, TEXT("CupInside"), FLinearColor(.76f, .76f, .74f), .4f);
        HallMakePlain(Sub, TEXT("PaperWhite"), FLinearColor(.72f, .71f, .67f), .92f);
        HallMakePlain(Sub, TEXT("PaperStained"), FLinearColor(.6f, .52f, .38f), .9f);
        HallMakePlain(Sub, TEXT("Cardboard"), FLinearColor(.33f, .2f, .1f), .85f);
        HallMakePlain(Sub, TEXT("CardboardDark"), FLinearColor(.16f, .09f, .045f), .85f);
        HallMakePlain(Sub, TEXT("FoilOrange"), FLinearColor(.72f, .26f, .02f), .3f, .6f);
        HallMakePlain(Sub, TEXT("FoilSilver"), FLinearColor(.6f, .6f, .62f), .25f, 1.f);
        HallMakePlain(Sub, TEXT("BottleClear"), FLinearColor(.55f, .64f, .7f), .08f);
        HallMakePlain(Sub, TEXT("CapBlue"), FLinearColor(.03f, .12f, .5f), .4f);
        HallMakePlain(Sub, TEXT("LabelBlue"), FLinearColor(.05f, .2f, .6f), .4f);
        HallMakePlain(Sub, TEXT("Steel"), FLinearColor(.58f, .59f, .61f), .36f, .45f);
        HallMakePlain(Sub, TEXT("PumpBlack"), FLinearColor(.018f, .018f, .02f), .45f);
        HallMakePlain(Sub, TEXT("HoseClear"), FLinearColor(.7f, .68f, .55f), .2f);
        HallMakePlain(Sub, TEXT("HoleDark"), FLinearColor(.01f, .01f, .01f), .9f);
        HallMakePlain(Sub, TEXT("TubBlue"), FLinearColor(.02f, .13f, .45f), .45f);
        HallMakePlain(Sub, TEXT("Ice"), FLinearColor(.62f, .7f, .74f), .08f);
    }

    if (UMaterialInterface* GlassParent = HallHouseMat(TEXT("M_House_Glass")))
    {
        UMaterialInstanceDynamic* Glass = UMaterialInstanceDynamic::Create(GlassParent, Sub);
        Glass->SetVectorParameterValue(TEXT("Tint"), FLinearColor(.012f, .015f, .018f));
        Glass->SetScalarParameterValue(TEXT("Opacity"), .5f);
        Glass->SetScalarParameterValue(TEXT("Roughness"), .04f);
        Glass->SetScalarParameterValue(TEXT("Metallic"), 0.f);
        Sub->Materials.Add(TEXT("Glass"), Glass);
    }
    if (UMaterialInterface* Concrete = HallHouseMat(TEXT("MI_SSO_S03WornConcrete"))) Sub->Materials.Add(TEXT("Concrete"), Concrete);
}

UMaterialInterface* HallMakePanelMaterial(UHouseBlackHallSubsystem* Sub, FName Key, const FHallPanelSpec& Spec)
{
    if (!Sub->CmuBase.IsValid()) return nullptr;
    const int32 W = FMath::Max(8, FMath::RoundToInt(Spec.WidthCm * Spec.PxPerCm));
    const int32 H = FMath::Max(8, FMath::RoundToInt(Spec.HeightCm * Spec.PxPerCm));
    FHallImage Base; Base.Init(W, H, FLinearColor::Black);
    FHallImage Normal; Normal.Init(W, H, FLinearColor(.5f, .5f, 1.f));
    FVector UAx, VAx;
    // Same planar axes the wall uses for this facing.
    const FVector Nrm = FVector::CrossProduct(Spec.UAx, Spec.VAx).GetSafeNormal();
    UAx = FVector::CrossProduct(Nrm, FVector::UpVector).GetSafeNormal(); VAx = FVector(0, 0, -1);
    for (int32 J = 0; J < H; ++J)
        for (int32 I = 0; I < W; ++I)
        {
            const FVector P = Spec.Origin + Spec.UAx * ((I + .5f) / Spec.PxPerCm) + Spec.VAx * ((J + .5f) / Spec.PxPerCm);
            const float U = FVector::DotProduct(P, UAx) / Cmu + CmuOffset().X;
            const float V = FVector::DotProduct(P, VAx) / Cmu + CmuOffset().Y;
            const float FU = U - FMath::FloorToFloat(U), FV = V - FMath::FloorToFloat(V);
            Base.At(I, J) = Sub->CmuBase->Sample(FU, FV);
            Normal.At(I, J) = Sub->CmuNormal->Sample(FU, FV);
        }
    FHallImage Paint; Paint.Init(W, H, FLinearColor(0, 0, 0, 0));
    if (Spec.Painter) Spec.Painter(Paint, Spec.PxPerCm);
    for (int32 J = 0; J < H; ++J)
        for (int32 I = 0; I < W; ++I)
        {
            const FLinearColor& Pp = Paint.At(I, J);
            if (Pp.A <= 0.001f) continue;
            FLinearColor& B = Base.At(I, J);
            // Paint over block: keep a trace of the block's light and dark so it reads as painted on masonry.
            const float Shade = FMath::Clamp(B.GetLuminance() / 0.021f, .55f, 1.25f);
            const float Wear = .9f + HallNoise(I * .35f, J * .35f, 5) * .1f;
            const FLinearColor Col = (Pp / FMath::Max(Pp.A, 1e-3f)) * (.8f + .2f * Shade) * Wear;
            B = FMath::Lerp(B, Col, FMath::Clamp(Pp.A, 0.f, 1.f));
            B.A = 1.f;
        }
    UTexture2D* BaseT = HallMakeTexture(Base, true, false, *FString::Printf(TEXT("T_BH_%s"), *Key.ToString()), false);
    UTexture2D* NormalT = HallMakeTexture(Normal, false, true, *FString::Printf(TEXT("T_BH_%sN"), *Key.ToString()), false);
    Sub->Textures.Add(Key, BaseT);
    return HallMakeTextured(Sub, Key, BaseT, NormalT, Spec.Roughness, FLinearColor::White, 1.f);
}
