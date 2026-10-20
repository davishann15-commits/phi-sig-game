// The look of homecoming weekend: Calder State purple and gold, the painted football field, hand-lettered bedsheets,
// lamp-post banners, the press-box sign, kiosk flyers, the band trailer and float lettering, and the glowing bits
// (scoreboard bulbs, jack-o'-lanterns, embers, string lights) as emission only. All painted in code.
#include "HouseStadiumDetail.h"
#include "HouseCampusFall.h"
#include "HouseCampusPolish.h"
#include "HouseCampusLayout.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace HouseCampusFallLookImpl
{
// School colours (linear).
const FLinearColor Purple(.105f, .028f, .235f), PurpleDeep(.05f, .012f, .11f), Gold(.78f, .54f, .12f), White(.86f, .85f, .8f), Ink(.012f, .01f, .016f);

/** Field geometry, shared with the stadium builder through the public functions at the end. */
constexpr float FieldCX = 2400.f, FieldCY = -13300.f, Yard = 50.29f;                   // 0.55-scale field (the bowl is small)
constexpr float HalfLength = 60.f * Yard, HalfWidth = 26.667f * Yard, Border = 160.f;    // 120 yd x 53.3 yd plus a border

// ------------------------------------------------------------------ painting helpers
/** Draws Text with its left baseline at Origin; handles ' # and a heart (U+2665), which the stroke font lacks. */
float PaintText(FHallImage& Img, const FString& Text, FVector2D Origin, const FHallTextStyle& S, const FLinearColor& C)
{
    const float U = S.Height / 6.f;
    float X = Origin.X;
    FString Run;
    auto Flush = [&]()
    {
        if (Run.IsEmpty()) return;
        HallDrawText(Img, Run, FVector2D(X, Origin.Y), S, C);
        X += HallTextWidth(Run, S) + S.Spacing * S.Height;
        Run.Reset();
    };
    for (int32 K = 0; K < Text.Len(); ++K)
    {
        const TCHAR Ch = Text[K];
        if (Ch == TEXT('\'')) { Flush(); Img.Stroke({ FVector2D(X + .5f * U, Origin.Y - 6.f * U), FVector2D(X + .15f * U, Origin.Y - 4.4f * U) }, S.Stroke, C); X += 1.3f * U; }
        else if (Ch == TEXT('#'))
        {
            Flush();
            const float W = 4.f * U * S.WidthScale;
            for (const float T : { .3f, .7f }) Img.Stroke({ FVector2D(X + W * T + .4f * U, Origin.Y - 6.f * U), FVector2D(X + W * T - .4f * U, Origin.Y) }, S.Stroke, C);
            for (const float T : { .35f, .68f }) Img.Stroke({ FVector2D(X, Origin.Y - 6.f * U * T), FVector2D(X + W, Origin.Y - 6.f * U * T) }, S.Stroke, C);
            X += W + S.Spacing * S.Height;
        }
        else if (Ch == TCHAR(0x2665))
        {
            Flush();
            const float R = 1.35f * U;
            const FVector2D L(X + R, Origin.Y - 4.3f * U), Rr(X + 3.f * R - .4f * U, Origin.Y - 4.3f * U);
            Img.Ellipse(L, FVector2D(R, R), 0.f, C); Img.Ellipse(Rr, FVector2D(R, R), 0.f, C);
            Img.FillPolygon({ FVector2D(X + .05f * U, Origin.Y - 3.9f * U), FVector2D(X + 4.f * R - .45f * U, Origin.Y - 3.9f * U), FVector2D(X + 2.f * R - .2f * U, Origin.Y + .2f * U) }, C);
            X += 4.f * R + S.Spacing * S.Height;
        }
        else Run.AppendChar(Ch);
    }
    Flush();
    return X - Origin.X;
}

float MeasureText(const FString& Text, const FHallTextStyle& S)
{
    FHallImage Scratch; Scratch.Init(1, 1, FLinearColor::Black);
    return PaintText(Scratch, Text, FVector2D(-1e6f, -1e6f), S, FLinearColor::Black);
}

void PaintCentered(FHallImage& Img, const FString& Text, float CX, float Baseline, FHallTextStyle S, const FLinearColor& C, float MaxWidth)
{
    // Squeeze until the ink (stroke ends included) fits; the heart and the blobs don't scale with WidthScale, so iterate.
    const float Room = MaxWidth - S.Stroke;
    float W = MeasureText(Text, S);
    for (int32 Pass = 0; Pass < 6 && W > Room; ++Pass)
    {
        S.WidthScale *= Room / W;
        S.Spacing *= Room / W;
        W = MeasureText(Text, S);
    }
    PaintText(Img, Text, FVector2D(CX - W * .5f, Baseline), S, C);
}

/** Paints Text into a scratch canvas, then lays it into Img centred at Center, rotated by Degrees (for field lettering). */
void PaintTextRotated(FHallImage& Img, const FString& Text, const FVector2D& Center, float Height, float Stroke, float Degrees, const FLinearColor& C, float WidthScale = 1.f)
{
    FHallTextStyle S; S.Height = Height; S.Stroke = Stroke; S.Spacing = .2f; S.WidthScale = WidthScale;
    const float W = MeasureText(Text, S) + Stroke * 2.f, H = Height + Stroke * 2.f;
    FHallImage T; T.Init(FMath::CeilToInt(W) + 4, FMath::CeilToInt(H) + 4, FLinearColor::Black);
    PaintText(T, Text, FVector2D(Stroke + 2.f, Height + Stroke + 2.f), S, FLinearColor::White);
    const float A = FMath::DegreesToRadians(Degrees), Ca = FMath::Cos(A), Sa = FMath::Sin(A);
    const float Reach = FMath::Sqrt(W * W + H * H) * .5f + 2.f;
    for (int32 Y = FMath::FloorToInt(Center.Y - Reach); Y <= FMath::CeilToInt(Center.Y + Reach); ++Y)
        for (int32 X = FMath::FloorToInt(Center.X - Reach); X <= FMath::CeilToInt(Center.X + Reach); ++X)
        {
            const float Dx = X - Center.X, Dy = Y - Center.Y;
            const float Tx = Ca * Dx + Sa * Dy + T.W * .5f, Ty = -Sa * Dx + Ca * Dy + T.H * .5f;
            if (Tx < 0.f || Ty < 0.f || Tx >= T.W - 1 || Ty >= T.H - 1) continue;
            const int32 X0 = FMath::FloorToInt(Tx), Y0 = FMath::FloorToInt(Ty);
            const float Fx = Tx - X0, Fy = Ty - Y0;
            const float L = FMath::Lerp(FMath::Lerp(T.At(X0, Y0).R, T.At(X0 + 1, Y0).R, Fx), FMath::Lerp(T.At(X0, Y0 + 1).R, T.At(X0 + 1, Y0 + 1).R, Fx), Fy);
            Img.Blend(X, Y, C, L);
        }
}

/** The Fighting Goose: a Canada goose's black head and neck with its white chinstrap, outlined in white, on a gold ring. */
void PaintGoose(FHallImage& Img, const FVector2D& C, float R, const FLinearColor& Field)
{
    Img.Ellipse(C, FVector2D(R, R), 0.f, Gold);
    Img.Ellipse(C, FVector2D(R * .86f, R * .86f), 0.f, Field);
    const TArray<FVector2D> Neck = { C + FVector2D(-.28f, .78f) * R, C + FVector2D(-.3f, .45f) * R, C + FVector2D(-.2f, .05f) * R, C + FVector2D(-.02f, -.28f) * R, C + FVector2D(.12f, -.42f) * R };
    const FVector2D Head = C + FVector2D(.2f, -.47f) * R;
    Img.Stroke(Neck, R * .3f, White);
    Img.Ellipse(Head, FVector2D(R * .3f, R * .2f), -12.f, White);
    Img.FillPolygon({ Head + FVector2D(.2f, -.08f) * R, Head + FVector2D(.58f, .02f) * R, Head + FVector2D(.2f, .12f) * R }, White);
    Img.Stroke(Neck, R * .21f, Ink);
    Img.Ellipse(Head, FVector2D(R * .25f, R * .155f), -12.f, Ink);
    Img.FillPolygon({ Head + FVector2D(.18f, -.05f) * R, Head + FVector2D(.5f, .02f) * R, Head + FVector2D(.18f, .09f) * R }, FLinearColor(.06f, .055f, .05f));
    Img.Ellipse(Head + FVector2D(-.07f, .07f) * R, FVector2D(R * .12f, R * .065f), 25.f, White);   // the chinstrap
    Img.Ellipse(Head + FVector2D(.08f, -.05f) * R, FVector2D(R * .03f, R * .03f), 0.f, Gold);       // a fierce eye
    // Wings raised behind the neck: The Fighting Geese.
    Img.FillPolygon({ C + FVector2D(-.25f, .2f) * R, C + FVector2D(-.72f, -.38f) * R, C + FVector2D(-.55f, .05f) * R, C + FVector2D(-.7f, .1f) * R, C + FVector2D(-.3f, .42f) * R }, Gold);
}

// ------------------------------------------------------------------ the field
void PaintField(FHallImage& Img)
{
    constexpr int32 W = 2048, H = 1024;
    // Image x runs east (+X) and image y runs north (+Y): seen from above in the engine (+Y down the screen) that is
    // the right way round. The stadium builder maps the texture the same way.
    const float MinX = FieldCX - HalfLength - Border, MinY = FieldCY - HalfWidth - Border;
    const float SizeX = 2.f * (HalfLength + Border), SizeY = 2.f * (HalfWidth + Border);
    auto Px = [&](float X, float Y) { return FVector2D((X - MinX) / SizeX * W, (Y - MinY) / SizeY * H); };
    auto Lx = [&](float X) { return (X - MinX) / SizeX * W; };
    auto Ly = [&](float Y) { return (Y - MinY) / SizeY * H; };
    auto Fill = [&](const FVector2D& A, const FVector2D& B, const FLinearColor& C, float Alpha)
    {
        Img.Rect(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)), C, Alpha);
    };
    const float PxPerCm = W / SizeX;
    Img.Init(W, H, FLinearColor::Black);
    const float GoalW = FieldCX - HalfLength + 10.f * Yard, GoalE = FieldCX + HalfLength - 10.f * Yard;
    const float SideN = FieldCY + HalfWidth, SideS = FieldCY - HalfWidth;
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float Wx = MinX + (X + .5f) / W * SizeX, Wy = MinY + (Y + .5f) / H * SizeY;
            const float N = HallFbm(X * .02f, Y * .02f, 4, 701), Fine = HallNoise(X * .7f, Y * .7f, 709);
            FLinearColor C(.045f, .115f, .04f);
            C = C * (.8f + .35f * N) * (.9f + .2f * Fine);
            const bool bInside = Wx > FieldCX - HalfLength && Wx < FieldCX + HalfLength && Wy < SideN && Wy > SideS;
            if (bInside)
            {
                // Mown in five-yard stripes; worn to dirt between the hashes and in front of the goals by late October.
                const int32 Stripe = FMath::FloorToInt((Wx - (FieldCX - HalfLength)) / (5.f * Yard));
                C = C * (Stripe % 2 ? 1.12f : .9f);
                const float Mid = FMath::Abs(Wy - FieldCY) / HalfWidth;
                const float Goal = FMath::Min(FMath::Abs(Wx - GoalW), FMath::Abs(Wx - GoalE)) / (12.f * Yard);
                const float Wear = FMath::Clamp((1.f - Mid * 1.6f) * .55f + FMath::Clamp(1.f - Goal, 0.f, 1.f) * (1.f - Mid) * .7f, 0.f, 1.f);
                const float Dirt = FMath::Clamp((Wear * 1.3f - .35f + (HallFbm(X * .035f, Y * .035f, 3, 711) - .5f) * 1.2f) * 1.6f, 0.f, .85f);
                C = FMath::Lerp(C, FLinearColor(.16f, .1f, .055f) * (.8f + .3f * Fine), Dirt);
            }
            Img.At(X, Y) = C;
        }
    // End zones in school purple with gold lettering.
    for (const float Side : { -1.f, 1.f })
    {
        const float X0 = Side < 0 ? FieldCX - HalfLength : GoalE, X1 = Side < 0 ? GoalW : FieldCX + HalfLength;
        Fill(Px(X0, SideN), Px(X1, SideS), Purple * .9f, .92f);
        Fill(Px(X0 + .6f * Yard, SideN - .6f * Yard), Px(X1 - .6f * Yard, SideN - 1.f * Yard), Gold, .9f);
        Fill(Px(X0 + .6f * Yard, SideS + 1.f * Yard), Px(X1 - .6f * Yard, SideS + .6f * Yard), Gold, .9f);
        const FVector2D C = Px((X0 + X1) * .5f, FieldCY);
        PaintTextRotated(Img, Side < 0 ? TEXT("CALDER") : HouseCampusFall::Team(), C, 5.2f * Yard * PxPerCm, .9f * Yard * PxPerCm, Side < 0 ? -90.f : 90.f, Gold, 1.05f);
    }
    // Lines: sidelines and end lines, goal lines, every five yards, and hash ticks every yard.
    const FLinearColor Chalk(.9f, .9f, .86f);
    auto LineX = [&](float X, float Width, float Y0, float Y1) { Fill(FVector2D(Lx(X) - Width * PxPerCm * .5f, Ly(Y0)), FVector2D(Lx(X) + Width * PxPerCm * .5f, Ly(Y1)), Chalk, .95f); };
    auto LineY = [&](float Y, float Width, float X0, float X1) { Fill(FVector2D(Lx(X0), Ly(Y) - Width * PxPerCm * .5f), FVector2D(Lx(X1), Ly(Y) + Width * PxPerCm * .5f), Chalk, .95f); };
    LineY(SideN + 30.f, 60.f, FieldCX - HalfLength - 30.f, FieldCX + HalfLength + 30.f);
    LineY(SideS - 30.f, 60.f, FieldCX - HalfLength - 30.f, FieldCX + HalfLength + 30.f);
    for (const float S : { -1.f, 1.f }) LineX(FieldCX + S * (HalfLength + 30.f), 60.f, SideN + 60.f, SideS - 60.f);
    for (int32 Y5 = 0; Y5 <= 20; ++Y5) LineX(GoalW + Y5 * 5.f * Yard, Y5 % 20 == 0 ? 11.f : 6.f, SideN, SideS);
    const float HashN = SideN - 20.f * Yard, HashS = SideS + 20.f * Yard;
    for (int32 Yd = 1; Yd < 100; ++Yd)
    {
        if (Yd % 5 == 0) continue;
        const float X = GoalW + Yd * Yard;
        for (const float Y : { HashN, HashS }) LineX(X, 5.f, Y + .5f * Yard, Y - .5f * Yard);
        LineX(X, 5.f, SideN, SideN - .5f * Yard);
        LineX(X, 5.f, SideS + .5f * Yard, SideS);
    }
    // Yard numbers with direction arrows, readable from each sideline.
    for (int32 Ten = 1; Ten <= 9; ++Ten)
    {
        const int32 Number = Ten <= 5 ? Ten * 10 : (10 - Ten) * 10;
        const float X = GoalW + Ten * 10.f * Yard;
        for (const float Side : { 1.f, -1.f })
        {
            const float Y = FieldCY + Side * (HalfWidth - 7.5f * Yard);
            const float NumberH = 2.2f * Yard * PxPerCm;
            PaintTextRotated(Img, FString::FromInt(Number), Px(X, Y), NumberH, NumberH * .17f, Side > 0 ? 0.f : 180.f, Chalk, 1.15f);
            if (Number < 50)
            {
                const float Toward = Ten < 5 ? -1.f : 1.f;
                const FVector2D Tip = Px(X + Toward * 2.6f * Yard, Y), Back = Px(X + Toward * 1.9f * Yard, Y);
                const float Half = .32f * Yard * PxPerCm;
                Img.FillPolygon({ Tip, Back + FVector2D(0, -Half), Back + FVector2D(0, Half) }, Chalk, .95f);
            }
        }
    }
    // Midfield: the Fighting Goose.
    PaintGoose(Img, Px(FieldCX, FieldCY), 7.f * Yard * PxPerCm, Purple);
    // Scuffs and divots across the paint.
    FRandomStream R(1018);
    for (int32 K = 0; K < 900; ++K)
    {
        const FVector2D P(R.FRandRange(Lx(GoalW - 4.f * Yard), Lx(GoalE + 4.f * Yard)), R.FRandRange(Ly(SideN), Ly(SideS)));
        Img.Ellipse(P, FVector2D(R.FRandRange(1.f, 4.f), R.FRandRange(.6f, 2.f)), R.FRandRange(0.f, 180.f), FLinearColor(.12f, .08f, .045f), R.FRandRange(.2f, .6f));
    }
}

// ------------------------------------------------------------------ cloth and signs
void PaintSheet(FHallImage& Img, int32 Kind)
{
    constexpr int32 W = 512, H = 256;
    Img.Init(W, H, FLinearColor::Black);
    FRandomStream R(3100 + Kind);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            // An old bedsheet: off-white with a faint stripe or floral, folds and grubby edges.
            const float Fold = .9f + .1f * FMath::Sin(X * .045f + Kind) * FMath::Sin(Y * .03f + 1.3f) + .05f * FMath::Sin(Y * .11f);
            const float Stripe = Kind % 3 == 1 && (Y / 18) % 2 ? .93f : 1.f;
            const float Edge = FMath::Min(FMath::Min(X, W - 1 - X), FMath::Min(Y, H - 1 - Y)) < 6 ? .82f : 1.f;
            Img.At(X, Y) = FLinearColor(.74f, .71f, .64f) * Fold * Stripe * Edge * (.94f + .1f * HallNoise(X * .3f, Y * .3f, 3110 + Kind));
        }
    static const TCHAR* Lines[][2] = {
        { TEXT("BEAT"), TEXT("DUNMORE!") },
        { TEXT("HONK IF YOU"), TEXT("♥ THE GEESE") },
        { TEXT("GEESE #1"), TEXT("") },
        { TEXT("HOMECOMING"), TEXT("'86") },
        { TEXT("DUNMORE"), TEXT("GO HOME") },
        { TEXT("3RD FLOOR"), TEXT("♥ THE GEESE") },
    };
    const FLinearColor Paints[] = { Purple * 1.1f, FLinearColor(.5f, .03f, .02f), Ink, Purple * 1.1f, FLinearColor(.02f, .12f, .05f), Purple * 1.1f };
    const FLinearColor Accent = Kind % 2 ? Gold * .9f : FLinearColor(.5f, .03f, .02f);
    FHallTextStyle S; S.Stroke = 13.f; S.Spacing = .2f; S.EndBlob = 4.f; S.Wobble = 2.5f; S.Seed = 77 + Kind;
    const bool bTwo = Lines[Kind % 6][1][0] != 0;
    S.Height = bTwo ? 86.f : 120.f;
    // Painted shadow first (a quick second colour, slightly offset), then the letters.
    const float B1 = bTwo ? 112.f : 178.f, B2 = 222.f;
    for (const int32 Pass : { 0, 1 })
    {
        const FVector2D Off = Pass == 0 ? FVector2D(5.f, 4.f) : FVector2D::ZeroVector;
        const FLinearColor C = Pass == 0 ? Accent : Paints[Kind % 6];
        PaintCentered(Img, Lines[Kind % 6][0], W * .5f + Off.X, B1 + Off.Y, S, C, W - 40.f);
        if (bTwo) PaintCentered(Img, Lines[Kind % 6][1], W * .5f + Off.X, B2 + Off.Y, S, C, W - 40.f);
    }
    // Drips under some letters.
    for (int32 K = 0; K < 10; ++K)
    {
        const float X = R.FRandRange(40.f, W - 40.f), Y = R.FRandRange(bTwo ? 110.f : 175.f, bTwo ? 125.f : 185.f);
        Img.Stroke({ FVector2D(X, Y), FVector2D(X + R.FRandRange(-1.f, 1.f), Y + R.FRandRange(8.f, 30.f)) }, R.FRandRange(2.f, 4.f), Paints[Kind % 6], .8f);
    }
}

void PaintLampBanner(FHallImage& Img, int32 Kind)
{
    constexpr int32 W = 256, H = 512;
    Img.Init(W, H, Purple);
    for (int32 Y = 0; Y < H; ++Y) for (int32 X = 0; X < W; ++X) Img.At(X, Y) = Purple * (.9f + .15f * HallNoise(X * .2f, Y * .2f, 51 + Kind));
    Img.Rect(FVector2D(10, 10), FVector2D(W - 10, 22), Gold); Img.Rect(FVector2D(10, H - 22), FVector2D(W - 10, H - 10), Gold);
    FHallTextStyle S; S.Height = 46.f; S.Stroke = 8.f; S.Spacing = .18f;
    if (Kind == 0)
    {
        PaintGoose(Img, FVector2D(W * .5f, 190.f), 100.f, Purple);
        PaintCentered(Img, HouseCampusFall::Team(), W * .5f, 380.f, S, Gold, W - 30.f);
        S.Height = 26.f; S.Stroke = 5.f;
        PaintCentered(Img, TEXT("CALDER STATE"), W * .5f, 440.f, S, White, W - 30.f);
    }
    else
    {
        S.Height = 34.f; S.Stroke = 6.f;
        PaintCentered(Img, TEXT("HOME"), W * .5f, 120.f, S, White, W - 30.f);
        PaintCentered(Img, TEXT("COMING"), W * .5f, 175.f, S, White, W - 30.f);
        S.Height = 110.f; S.Stroke = 18.f;
        PaintCentered(Img, TEXT("'86"), W * .5f, 330.f, S, Gold, W - 30.f);
        S.Height = 26.f; S.Stroke = 5.f;
        PaintCentered(Img, TEXT("OCT 18"), W * .5f, 420.f, S, White, W - 30.f);
    }
}

void PaintWide(FHallImage& Img, int32 W, int32 H, const FLinearColor& Field, const FString& Big, const FString& Small, const FLinearColor& BigColor, const FLinearColor& SmallColor, bool bGeese)
{
    Img.Init(W, H, Field);
    for (int32 Y = 0; Y < H; ++Y) for (int32 X = 0; X < W; ++X) Img.At(X, Y) = Field * (.92f + .12f * HallNoise(X * .15f, Y * .15f, 901 + W));
    Img.Rect(FVector2D(0, H * .06f), FVector2D(W, H * .1f), Gold, .9f);
    Img.Rect(FVector2D(0, H * .9f), FVector2D(W, H * .94f), Gold, .9f);
    const float Side = bGeese ? H * 1.02f : 20.f;
    if (bGeese) for (const float X : { H * .5f, W - H * .5f }) PaintGoose(Img, FVector2D(X, H * .5f), H * .38f, Field);
    FHallTextStyle S; S.Height = Small.IsEmpty() ? H * .5f : H * .4f; S.Stroke = S.Height * .14f; S.Spacing = .2f;
    PaintCentered(Img, Big, W * .5f, Small.IsEmpty() ? H * .74f : H * .54f, S, BigColor, W - 2.f * Side);
    if (!Small.IsEmpty())
    {
        S.Height = H * .2f; S.Stroke = S.Height * .14f;
        PaintCentered(Img, Small, W * .5f, H * .82f, S, SmallColor, W - 2.f * Side);
    }
}

void PaintFlyers(FHallImage& Img)
{
    // The kiosk wrap: layers of stapled flyers, torn corners and tear-off tabs (invented bands and campus events).
    constexpr int32 W = 1024, H = 512;
    Img.Init(W, H, FLinearColor(.16f, .14f, .1f));
    static const TCHAR* Flyers[][3] = {
        { TEXT("LIVE TONIGHT"), TEXT("THE PAPER LANTERNS"), TEXT("THE ANNEX 9PM") },
        { TEXT("FALL FORMAL"), TEXT("SAT OCT 25"), TEXT("STUDENT UNION") },
        { TEXT("LOST CAT"), TEXT("GREY - ANSWERS TO MILO"), TEXT("CALL 555 0142") },
        { TEXT("HAUNTED HAYRIDE"), TEXT("OCT 31"), TEXT("BUS FROM GATE 4") },
        { TEXT("ROOMMATE WANTED"), TEXT("NO SMOKERS"), TEXT("120 A MONTH") },
        { TEXT("HOMECOMING DANCE"), TEXT("TONIGHT"), TEXT("DJ AND PUNCH") },
        { TEXT("SKI CLUB"), TEXT("FIRST MEETING"), TEXT("TUES 7PM") },
        { TEXT("BAKE SALE"), TEXT("PROCEEDS TO BAND"), TEXT("FRIDAY") },
        { TEXT("STUDY GROUP"), TEXT("CHEM 101"), TEXT("LIBRARY RM 2") },
        { TEXT("GO GEESE"), TEXT("BEAT DUNMORE"), TEXT("SAT OCT 18") },
        { TEXT("TYPING"), TEXT("TERM PAPERS"), TEXT("1 A PAGE") },
        { TEXT("CONCERT"), TEXT("MIDNIGHT CHORUS"), TEXT("FRI 8PM") },
    };
    const FLinearColor Papers[] = { FLinearColor(.75f, .72f, .62f), FLinearColor(.75f, .5f, .55f), FLinearColor(.5f, .68f, .75f), FLinearColor(.78f, .7f, .25f),
        FLinearColor(.45f, .72f, .45f), FLinearColor(.78f, .45f, .22f), FLinearColor(.72f, .72f, .7f) };
    FRandomStream R(1986);
    for (int32 K = 0; K < 34; ++K)
    {
        const float Fw = R.FRandRange(120.f, 190.f), Fh = R.FRandRange(150.f, 230.f);
        const FVector2D P(R.FRandRange(-20.f, W - Fw + 20.f), R.FRandRange(-30.f, H - Fh + 30.f));
        const FLinearColor Paper = Papers[R.RandRange(0, 6)];
        Img.Rect(P + FVector2D(3, 4), P + FVector2D(Fw + 3, Fh + 4), FLinearColor(.05f, .04f, .03f), .5f);
        Img.Rect(P, P + FVector2D(Fw, Fh), Paper);
        const int32 Which = R.RandRange(0, 11);
        FHallTextStyle S; S.Height = Fw * .13f; S.Stroke = S.Height * .16f; S.Spacing = .18f; S.Wobble = R.FRandRange(0.f, 1.2f); S.Seed = K;
        const FLinearColor Inkk = R.FRand() < .7f ? Ink : FLinearColor(.4f, .03f, .03f);
        PaintCentered(Img, Flyers[Which][0], P.X + Fw * .5f, P.Y + Fh * .25f, S, Inkk, Fw - 12.f);
        S.Height *= .62f; S.Stroke *= .7f;
        PaintCentered(Img, Flyers[Which][1], P.X + Fw * .5f, P.Y + Fh * .5f, S, Inkk, Fw - 12.f);
        PaintCentered(Img, Flyers[Which][2], P.X + Fw * .5f, P.Y + Fh * .68f, S, Inkk, Fw - 12.f);
        if (R.FRand() < .5f) for (float X = P.X + 6.f; X < P.X + Fw - 10.f; X += 14.f) Img.Rect(FVector2D(X, P.Y + Fh - 26.f), FVector2D(X + 1.5f, P.Y + Fh), FLinearColor(.2f, .2f, .2f), .7f);
        Img.Ellipse(P + FVector2D(Fw * .5f, 5.f), FVector2D(4.f, 1.5f), 0.f, FLinearColor(.5f, .5f, .52f));   // staple
    }
}
}

// ------------------------------------------------------------------ public
int32 HouseCampusFall::AutumnTone(const FVector& At)
{
    if (!Enabled()) return -1;
    // The campus trees you can see from the street and the green (the WoodsPass census): red maple and red oak, and
    // the birches gone orange.
    struct FPick { float X, Y; int32 Tone; };
    static const FPick Picks[] = { { -7277.f, -5900.f, 1 }, { 5559.f, -8315.f, 1 }, { -2424.f, -5900.f, 0 }, { 7365.f, -5900.f, 0 }, { -8320.f, -9540.f, 0 } };
    for (const FPick& P : Picks) if (FMath::Abs(At.X - P.X) < 40.f && FMath::Abs(At.Y - P.Y) < 40.f) return P.Tone;
    return -1;
}
FName HouseCampusFall::AutumnLeafKey(FName Slot, int32 Tone)
{
    const FString S = Slot.ToString();
    const bool bMaple = S == TEXT("EX_NatureGold") || S == TEXT("EX_NatureCopper") || S == TEXT("EX_NatureRust") || S == TEXT("EX_NatureGreenLeaf");
    const bool bOak = S.StartsWith(TEXT("EX_NatureOakLeaf")), bBirch = S.StartsWith(TEXT("EX_NatureBirchLeaf"));
    if (bMaple) return Tone == 1 ? FName(TEXT("CF_MapleRed")) : FName(TEXT("EX_NatureCopper"));
    if (bOak) return Tone == 1 ? FName(TEXT("CF_OakRed")) : FName(TEXT("EX_NatureOakLeaf1"));
    if (bBirch) return FName(TEXT("EX_NatureBirchLeaf1"));
    return NAME_None;
}

bool HouseCampusFall::Enabled() { return !FParse::Param(FCommandLine::Get(), TEXT("NoCampusFall")); }
bool HouseCampusFall::IsDorm(int32 Seed) { return Enabled() && (HouseCampusLayout::Enabled() ? HouseCampusLayout::IsDorm(Seed) : (Seed == 1 || Seed == 4)); }
bool HouseCampusFall::WindowLit(int32 Index, int32 Floor, int32 Seed)
{
    if (!IsDorm(Seed)) return (Index + Floor + Seed) % 5 == 0;
    return (Index * 3 + Floor * 2 + Seed) % 5 < 2;   // a dorm on homecoming night: about two windows in five
}
FName HouseCampusFall::SeatKey(int32 Row) { return Row % 4 == 3 ? FName(TEXT("CF_SeatGold")) : FName(TEXT("CF_SeatPurple")); }
FName HouseCampusFall::MercuryLensKey() { return FName(TEXT("CF_MercuryLens")); }
float HouseCampusFall::StadiumLightRadius() { return Enabled() ? 6500.f : 5000.f; }
FVector HouseCampusFall::StadiumLightAt(const FVector& Lamp)
{
    // The floods aim down at the field: each light moves a third of the way in from its mast, so with the longer reach the
    // field is lit to the centre line while the spill on the street stays where it was.
    if (!Enabled()) return Lamp;
    const FVector Field(HouseCampusFallLookImpl::FieldCX, HouseCampusFallLookImpl::FieldCY, Lamp.Z);
    return FMath::Lerp(Lamp, Field, .35f);
}

void HouseCampusFall::CreateLook(UHouseBlackHallSubsystem* Look)
{
    const FLinearColor Purple = HouseCampusFallLookImpl::Purple;
    if (!Look || !Enabled()) return;
    // The stands' treads turn to weathered concrete under the purple and gold benches (the key stays the stadium's own).
    HallMakePlain(Look, TEXT("EX_CampusSeats"), FLinearColor(.25f, .245f, .23f), .9f);
    if (Look->Materials.Contains(TEXT("CF_Field"))) return;
    // Red leaves for the red maple and red oak: the copper atlas cells under a tint (the foliage master's own parameter).
    auto Redden = [Look](const TCHAR* From, const TCHAR* To)
    {
        UMaterialInstanceDynamic* Source = Cast<UMaterialInstanceDynamic>(Look->Materials.FindRef(FName(From)));
        if (!Source || !Source->Parent) return;
        UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Source->Parent, Look);
        M->CopyParameterOverrides(Source);
        FLinearColor Tint = FLinearColor::White;
        Source->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Tint")), Tint);
        M->SetVectorParameterValue(TEXT("Tint"), Tint * FLinearColor(.65f, .35f, .9f));
        Look->Materials.Add(FName(To), M);
    };
    Redden(TEXT("EX_NatureCopper"), TEXT("CF_MapleRed"));
    Redden(TEXT("EX_NatureOakLeaf1"), TEXT("CF_OakRed"));
    auto Tex = [Look](const TCHAR* Key, const FHallImage& Img, float Rough, bool bWrap, const FLinearColor& Emission = FLinearColor::Black)
    {
        UTexture2D* T = HallMakeTexture(Img, true, false, *FString::Printf(TEXT("T_%s"), Key), bWrap);
        if (T) Look->Textures.Add(FName(*FString::Printf(TEXT("T_%s"), Key)), T);
        HallMakeTextured(Look, FName(Key), T, nullptr, Rough, FLinearColor::White, 0.f, 0.f, Emission);
    };
    FHallImage Img;
    HouseCampusFallLookImpl::PaintField(Img);
    HouseStadiumDetail::WeatherField(Img);
    Tex(TEXT("CF_Field"), Img, .92f, false, FLinearColor(.006f, .009f, .006f));
    for (int32 K = 0; K < 6; ++K)
    {
        HouseCampusFallLookImpl::PaintSheet(Img, K);
        Tex(*FString::Printf(TEXT("CF_Sheet%d"), K), Img, .9f, false, FLinearColor(.05f, .045f, .035f));
    }
    for (int32 K = 0; K < 2; ++K) { HouseCampusFallLookImpl::PaintLampBanner(Img, K); Tex(*FString::Printf(TEXT("CF_LampBanner%d"), K), Img, .8f, false, FLinearColor(.035f, .02f, .05f)); }
    HouseCampusFallLookImpl::PaintWide(Img, 1024, 128, Purple, TEXT("CALDER STATE STADIUM"), TEXT(""), HouseCampusFallLookImpl::Gold, HouseCampusFallLookImpl::White, true);
    Tex(TEXT("CF_PressSign"), Img, .6f, false, FLinearColor(.12f, .07f, .1f));
    HouseCampusFallLookImpl::PaintWide(Img, 1024, 256, Purple, TEXT("CALDER STATE"), TEXT("MARCHING GEESE"), HouseCampusFallLookImpl::White, HouseCampusFallLookImpl::Gold, true);
    Tex(TEXT("CF_TrailerSide"), Img, .5f, false);
    HouseCampusFallLookImpl::PaintWide(Img, 1024, 128, HouseCampusFallLookImpl::Gold * .9f, TEXT("HOMECOMING '86"), TEXT("THE GEESE TAKE FLIGHT"), Purple, Purple, false);
    Tex(TEXT("CF_FloatSkirt"), Img, .85f, false, FLinearColor(.05f, .04f, .02f));
    HouseCampusFallLookImpl::PaintWide(Img, 512, 128, FLinearColor(.72f, .7f, .62f), TEXT("TICKETS"), TEXT("SOLD OUT"), Purple, FLinearColor(.5f, .03f, .02f), false);
    Tex(TEXT("CF_TicketSign"), Img, .8f, false, FLinearColor(.06f, .05f, .04f));
    HouseCampusFallLookImpl::PaintFlyers(Img); Tex(TEXT("CF_Flyers"), Img, .85f, true, FLinearColor(.02f, .018f, .014f));

    struct FPlain { const TCHAR* Key; FLinearColor Tint; float Rough, Metal; FLinearColor Emission; };
    const FLinearColor Gold = HouseCampusFallLookImpl::Gold, White = HouseCampusFallLookImpl::White;
    const FPlain Plains[] = {
        { TEXT("CF_Purple"), Purple, .6f, 0.f, Purple * .05f },
        { TEXT("CF_Gold"), Gold, .45f, .2f, Gold * .04f },
        { TEXT("CF_SeatPurple"), Purple * 1.2f, .5f, .1f, Purple * .12f },
        { TEXT("CF_SeatGold"), Gold, .45f, .2f, Gold * .08f },
        { TEXT("CF_Concrete"), FLinearColor(.42f, .41f, .39f), .9f, 0.f, FLinearColor(.01f, .01f, .012f) },
        { TEXT("CF_Grass"), FLinearColor(.03f, .075f, .033f), .95f, 0.f, FLinearColor::Black },
        { TEXT("CF_Track"), FLinearColor(.16f, .065f, .04f), .95f, 0.f, FLinearColor::Black },
        { TEXT("CF_TrackLine"), FLinearColor(.82f, .82f, .78f), .8f, 0.f, FLinearColor(.02f, .02f, .02f) },
        { TEXT("CF_PostGold"), FLinearColor(.88f, .66f, .1f), .4f, .25f, FLinearColor(.16f, .11f, .015f) },
        { TEXT("CF_PostPad"), Purple, .7f, 0.f, Purple * .1f },
        { TEXT("CF_Aluminium"), FLinearColor(.55f, .56f, .58f), .35f, .9f, FLinearColor::Black },
        { TEXT("CF_Cooler"), FLinearColor(.75f, .5f, .06f), .45f, 0.f, FLinearColor(.02f, .015f, .003f) },
        { TEXT("CF_Bench"), FLinearColor(.5f, .5f, .52f), .4f, .8f, FLinearColor::Black },
        { TEXT("CF_BulbOn"), FLinearColor(1.f, .85f, .55f), .3f, 0.f, FLinearColor(30.f, 19.f, 7.f) },
        { TEXT("CF_BulbOff"), FLinearColor(.05f, .04f, .025f), .3f, 0.f, FLinearColor(.02f, .012f, .004f) },
        { TEXT("CF_SheetBack"), FLinearColor(.66f, .64f, .58f), .9f, 0.f, FLinearColor(.04f, .036f, .03f) },
        { TEXT("CF_Pennant0"), Purple * 1.3f, .7f, 0.f, Purple * .4f },
        { TEXT("CF_Pennant1"), Gold, .6f, 0.f, Gold * .18f },
        { TEXT("CF_Pennant2"), White, .7f, 0.f, White * .12f },
        { TEXT("CF_Cord"), FLinearColor(.08f, .07f, .06f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CF_Kiosk"), FLinearColor(.035f, .07f, .045f), .7f, 0.f, FLinearColor::Black },
        { TEXT("CF_KioskCap"), FLinearColor(.06f, .06f, .065f), .5f, .6f, FLinearColor::Black },
        { TEXT("CF_TrailerBody"), Purple, .45f, .2f, Purple * .03f },
        { TEXT("CF_Tyre"), FLinearColor(.015f, .015f, .015f), .85f, 0.f, FLinearColor::Black },
        { TEXT("CF_Chrome"), FLinearColor(.7f, .7f, .72f), .2f, 1.f, FLinearColor::Black },
        { TEXT("CF_FloatDeck"), FLinearColor(.12f, .085f, .05f), .85f, 0.f, FLinearColor::Black },
        // Tissue on the float, caught in its own string lights.
        { TEXT("CF_TissueWhite"), FLinearColor(.85f, .84f, .8f), .95f, 0.f, FLinearColor(.28f, .27f, .24f) },
        { TEXT("CF_TissueBlack"), FLinearColor(.035f, .032f, .035f), .95f, 0.f, FLinearColor(.014f, .013f, .015f) },
        { TEXT("CF_TissueBrown"), FLinearColor(.34f, .26f, .17f), .95f, 0.f, FLinearColor(.075f, .055f, .035f) },
        { TEXT("CF_TissuePurple"), Purple * 1.4f, .95f, 0.f, Purple * .7f },
        { TEXT("CF_TissueGold"), Gold, .95f, 0.f, Gold * .3f },
        { TEXT("CF_Booth"), FLinearColor(.62f, .6f, .55f), .7f, 0.f, FLinearColor(.015f, .015f, .013f) },
        { TEXT("CF_Pumpkin"), FLinearColor(.85f, .3f, .03f), .6f, 0.f, FLinearColor(.03f, .01f, .0f) },
        { TEXT("CF_Stem"), FLinearColor(.12f, .14f, .06f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CF_StringBulb"), FLinearColor(1.f, .85f, .55f), .3f, 0.f, FLinearColor(16.f, 9.5f, 3.4f) },
        { TEXT("CF_Ash"), FLinearColor(.12f, .115f, .11f), .95f, 0.f, FLinearColor::Black },
        { TEXT("CF_Char"), FLinearColor(.02f, .018f, .016f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CF_Fieldstone"), FLinearColor(.3f, .28f, .25f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CF_CupWhite"), FLinearColor(.8f, .8f, .78f), .5f, 0.f, FLinearColor(.02f, .02f, .02f) },
        { TEXT("CF_CupPurple"), Purple * 1.2f, .5f, 0.f, FLinearColor::Black },
        { TEXT("CF_Program"), FLinearColor(.75f, .6f, .2f), .7f, 0.f, FLinearColor(.01f, .008f, .003f) },
        { TEXT("CF_Popcorn"), FLinearColor(.6f, .06f, .04f), .7f, 0.f, FLinearColor::Black },
        { TEXT("CF_PopcornKernel"), FLinearColor(.8f, .72f, .45f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CF_PaperBag"), FLinearColor(.45f, .33f, .2f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CF_Rake"), FLinearColor(.2f, .14f, .08f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CF_BikeRed"), FLinearColor(.45f, .04f, .03f), .35f, .3f, FLinearColor::Black },
        { TEXT("CF_BikeBlue"), FLinearColor(.04f, .1f, .4f), .35f, .3f, FLinearColor::Black },
        { TEXT("CF_BikeYellow"), FLinearColor(.75f, .6f, .06f), .35f, .3f, FLinearColor::Black },
        { TEXT("CF_BikeSilver"), FLinearColor(.5f, .52f, .55f), .3f, .6f, FLinearColor::Black },
        { TEXT("CF_Poster0"), FLinearColor(.6f, .1f, .1f), .8f, 0.f, FLinearColor(.5f, .12f, .08f) },
        { TEXT("CF_Poster1"), FLinearColor(.1f, .2f, .5f), .8f, 0.f, FLinearColor(.12f, .2f, .45f) },
        { TEXT("CF_Poster2"), FLinearColor(.7f, .6f, .2f), .8f, 0.f, FLinearColor(.55f, .42f, .12f) },
        { TEXT("CF_Poster3"), Purple * 1.4f, .8f, 0.f, FLinearColor(.25f, .08f, .4f) },
        { TEXT("CF_Curtain"), FLinearColor(.5f, .4f, .25f), .9f, 0.f, FLinearColor(.9f, .62f, .3f) },
        { TEXT("CF_PhoneFrame"), FLinearColor(.6f, .61f, .62f), .35f, .85f, FLinearColor::Black },
        { TEXT("CF_PhoneGlass"), FLinearColor(.03f, .035f, .04f), .1f, .3f, FLinearColor(.06f, .07f, .08f) },
        { TEXT("CF_NewsBlue"), FLinearColor(.04f, .09f, .3f), .5f, .3f, FLinearColor::Black },
        { TEXT("CF_NewsRed"), FLinearColor(.45f, .05f, .03f), .5f, .3f, FLinearColor::Black },
        { TEXT("CF_MercuryLens"), FLinearColor(.85f, .95f, 1.f), .4f, 0.f, FLinearColor(18.f, 28.f, 30.f) },
    };
    for (const FPlain& P : Plains) HallMakePlain(Look, P.Key, P.Tint, P.Rough, P.Metal, P.Emission);
    if (HouseCampusPolish::Enabled())
    {
        // Tissue pomps: rows of little paper rosettes stuffed into chicken wire (for the float, step 3c of the master job,
        // so only with the polish on); one grey texture, tinted per colour.
        FHallImage Pomp;
        constexpr int32 N = 128;
        Pomp.Init(N, N, FLinearColor(.25f, .25f, .25f));
        FRandomStream PR(1986);
        for (int32 Y = 0; Y < 8; ++Y)
            for (int32 X = 0; X < 8; ++X)
            {
                const FVector2D C(X * 16.f + 8.f + ((Y & 1) ? 8.f : 0.f) + PR.FRandRange(-2.f, 2.f), Y * 16.f + 8.f + PR.FRandRange(-2.f, 2.f));
                for (int32 K = 0; K < 7; ++K)
                {
                    const float A = K * 2.f * PI / 7.f + PR.FRandRange(0.f, .6f);
                    Pomp.Ellipse(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 4.f, FVector2D(5.5f, 3.2f), FMath::RadiansToDegrees(A), FLinearColor(.75f, .75f, .75f) * PR.FRandRange(.8f, 1.1f));
                }
                Pomp.Ellipse(C, FVector2D(3.f, 3.f), 0.f, FLinearColor(1.f, 1.f, 1.f));
            }
        UTexture2D* PT = HallMakeTexture(Pomp, true, false, TEXT("T_CF_Pomp"), true);
        if (PT) Look->Textures.Add(TEXT("T_CF_Pomp"), PT);
        const TPair<const TCHAR*, FLinearColor> Pomps[] = { { TEXT("CF_PompWhite"), FLinearColor(1.1f, 1.08f, 1.02f) }, { TEXT("CF_PompBlack"), FLinearColor(.07f, .065f, .07f) },
            { TEXT("CF_PompBrown"), FLinearColor(.48f, .36f, .24f) }, { TEXT("CF_PompPurple"), Purple * 1.9f }, { TEXT("CF_PompGold"), Gold * 1.35f } };
        for (const TPair<const TCHAR*, FLinearColor>& T : Pomps) HallMakeTextured(Look, T.Key, PT, nullptr, .95f, T.Value, 0.f, 0.f, T.Value * .12f);
        // Chicken wire over the dark inside of the float, where the tissue tore.
        FHallImage Wire;
        Wire.Init(N, N, FLinearColor(.02f, .02f, .02f));
        for (int32 Y = 0; Y < 6; ++Y)
            for (int32 X = 0; X < 6; ++X)
            {
                const FVector2D C(X * 22.f + ((Y & 1) ? 11.f : 0.f), Y * 22.f);
                TArray<FVector2D> Hex;
                for (int32 K = 0; K <= 6; ++K) { const float A = K * PI / 3.f + PI / 6.f; Hex.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 12.f); }
                Wire.Stroke(Hex, 1.6f, FLinearColor(.55f, .56f, .55f));
            }
        UTexture2D* WT = HallMakeTexture(Wire, true, false, TEXT("T_CF_ChickenWire"), true);
        if (WT) Look->Textures.Add(TEXT("T_CF_ChickenWire"), WT);
        HallMakeTextured(Look, TEXT("CF_ChickenWire"), WT, nullptr, .6f, FLinearColor::White, 0.f, .6f);
    }
    // Chalk on the walks, a little lit by the walk lamps.
    const FLinearColor Chalks[] = { FLinearColor(.8f, .8f, .78f), FLinearColor(.8f, .45f, .55f), FLinearColor(.75f, .7f, .3f), FLinearColor(.4f, .6f, .8f) };
    for (int32 K = 0; K < 4; ++K) HallMakePlain(Look, FName(*FString::Printf(TEXT("CF_Chalk%d"), K)), Chalks[K], .95f, 0.f, Chalks[K] * .06f);
    // Animated glows: three candle keys (so the lanterns don't flicker in step), three ember keys, three chase keys.
    for (int32 K = 0; K < 3; ++K)
    {
        HallMakePlain(Look, FName(*FString::Printf(TEXT("CF_Candle%d"), K)), FLinearColor(1.f, .6f, .2f), .5f, 0.f, FLinearColor(14.f, 6.5f, 1.4f));
        HallMakePlain(Look, FName(*FString::Printf(TEXT("CF_Ember%d"), K)), FLinearColor(.4f, .1f, .02f), .8f, 0.f, FLinearColor(9.f, 2.4f, .35f));
        HallMakePlain(Look, FName(*FString::Printf(TEXT("CF_Chase%d"), K)), FLinearColor(1.f, .85f, .55f), .3f, 0.f, FLinearColor(30.f, 19.f, 7.f));
    }
    HouseStadiumDetail::CreateLook(Look);
}
