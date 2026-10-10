#include "HouseBlackHallPaint.h"

#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "UObject/Package.h"

FLinearColor FHallImage::Sample(float U, float V) const
{
    if (W == 0 || H == 0) return FLinearColor::Black;
    float X = U * W - .5f, Y = V * H - .5f;
    const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
    const float FX = X - X0, FY = Y - Y0;
    auto Get = [&](int32 PX, int32 PY) -> const FLinearColor&
    {
        PX = ((PX % W) + W) % W; PY = ((PY % H) + H) % H;
        return Px[PY * W + PX];
    };
    const FLinearColor A = Get(X0, Y0) * (1 - FX) + Get(X0 + 1, Y0) * FX;
    const FLinearColor B = Get(X0, Y0 + 1) * (1 - FX) + Get(X0 + 1, Y0 + 1) * FX;
    return A * (1 - FY) + B * FY;
}

static bool HallInsidePolygon(const TArray<FVector2D>& P, double X, double Y)
{
    int32 Winding = 0;
    for (int32 I = 0, J = P.Num() - 1; I < P.Num(); J = I++)
    {
        const FVector2D& A = P[J]; const FVector2D& B = P[I];
        if (A.Y <= Y)
        {
            if (B.Y > Y && (B.X - A.X) * (Y - A.Y) - (X - A.X) * (B.Y - A.Y) > 0) ++Winding;
        }
        else if (B.Y <= Y && (B.X - A.X) * (Y - A.Y) - (X - A.X) * (B.Y - A.Y) < 0) --Winding;
    }
    return Winding != 0;
}

void FHallImage::FillPolygon(const TArray<FVector2D>& P, const FLinearColor& C, float Alpha)
{
    if (P.Num() < 3) return;
    FBox2D B(ForceInit); for (const FVector2D& V : P) B += V;
    const int32 X0 = FMath::Max(0, FMath::FloorToInt(B.Min.X)), X1 = FMath::Min(W - 1, FMath::CeilToInt(B.Max.X));
    const int32 Y0 = FMath::Max(0, FMath::FloorToInt(B.Min.Y)), Y1 = FMath::Min(H - 1, FMath::CeilToInt(B.Max.Y));
    for (int32 Y = Y0; Y <= Y1; ++Y)
        for (int32 X = X0; X <= X1; ++X)
        {
            int32 Hits = 0;
            for (int32 SY = 0; SY < 4; ++SY)
                for (int32 SX = 0; SX < 4; ++SX)
                    Hits += HallInsidePolygon(P, X + (SX + .5) / 4.0, Y + (SY + .5) / 4.0) ? 1 : 0;
            if (Hits) Blend(X, Y, C, Alpha * Hits / 16.f);
        }
}

void FHallImage::Stroke(const TArray<FVector2D>& P, float Width, const FLinearColor& C, float Alpha)
{
    if (P.Num() == 0) return;
    FBox2D B(ForceInit); for (const FVector2D& V : P) B += V;
    B = B.ExpandBy(Width * .5f + 2.f);
    const int32 X0 = FMath::Max(0, FMath::FloorToInt(B.Min.X)), X1 = FMath::Min(W - 1, FMath::CeilToInt(B.Max.X));
    const int32 Y0 = FMath::Max(0, FMath::FloorToInt(B.Min.Y)), Y1 = FMath::Min(H - 1, FMath::CeilToInt(B.Max.Y));
    if (X1 < X0 || Y1 < Y0) return;
    const int32 CW = X1 - X0 + 1;
    TArray<float> Cov; Cov.Init(0.f, CW * (Y1 - Y0 + 1));
    const float R = Width * .5f;
    for (int32 S = 0; S < FMath::Max(1, P.Num() - 1); ++S)
    {
        const FVector2D A = P[S], E = P.Num() > 1 ? P[S + 1] : P[S];
        FBox2D SB(ForceInit); SB += A; SB += E; SB = SB.ExpandBy(R + 1.5f);
        const FVector2D D = E - A; const double L2 = FMath::Max(1e-6, D.SizeSquared());
        for (int32 Y = FMath::Max(Y0, FMath::FloorToInt(SB.Min.Y)); Y <= FMath::Min(Y1, FMath::CeilToInt(SB.Max.Y)); ++Y)
            for (int32 X = FMath::Max(X0, FMath::FloorToInt(SB.Min.X)); X <= FMath::Min(X1, FMath::CeilToInt(SB.Max.X)); ++X)
            {
                const FVector2D Q(X + .5, Y + .5);
                const double T = FMath::Clamp(FVector2D::DotProduct(Q - A, D) / L2, 0.0, 1.0);
                const float Dist = (Q - (A + D * T)).Size();
                const float V = FMath::Clamp(R - Dist + .5f, 0.f, 1.f);
                float& Slot = Cov[(Y - Y0) * CW + (X - X0)];
                Slot = FMath::Max(Slot, V);
            }
    }
    for (int32 Y = Y0; Y <= Y1; ++Y)
        for (int32 X = X0; X <= X1; ++X)
            if (const float V = Cov[(Y - Y0) * CW + (X - X0)]) Blend(X, Y, C, V * Alpha);
}

void FHallImage::Ellipse(const FVector2D& Ctr, const FVector2D& Rad, float RotDeg, const FLinearColor& C, float Alpha)
{
    const float RMax = FMath::Max(Rad.X, Rad.Y) + 2.f;
    const float Cs = FMath::Cos(FMath::DegreesToRadians(RotDeg)), Sn = FMath::Sin(FMath::DegreesToRadians(RotDeg));
    const int32 X0 = FMath::Max(0, FMath::FloorToInt(Ctr.X - RMax)), X1 = FMath::Min(W - 1, FMath::CeilToInt(Ctr.X + RMax));
    const int32 Y0 = FMath::Max(0, FMath::FloorToInt(Ctr.Y - RMax)), Y1 = FMath::Min(H - 1, FMath::CeilToInt(Ctr.Y + RMax));
    const float RMin = FMath::Max(.5f, FMath::Min(Rad.X, Rad.Y));
    for (int32 Y = Y0; Y <= Y1; ++Y)
        for (int32 X = X0; X <= X1; ++X)
        {
            const float DX = X + .5f - Ctr.X, DY = Y + .5f - Ctr.Y;
            const float LX = DX * Cs + DY * Sn, LY = -DX * Sn + DY * Cs;
            const float Q = FMath::Sqrt(FMath::Square(LX / FMath::Max(.5f, Rad.X)) + FMath::Square(LY / FMath::Max(.5f, Rad.Y)));
            const float Dist = (Q - 1.f) * RMin;
            const float V = FMath::Clamp(.5f - Dist, 0.f, 1.f);
            if (V > 0) Blend(X, Y, C, V * Alpha);
        }
}

void FHallImage::Rect(const FVector2D& Min, const FVector2D& Max, const FLinearColor& C, float Alpha)
{
    const int32 X0 = FMath::Max(0, FMath::FloorToInt(Min.X)), X1 = FMath::Min(W - 1, FMath::CeilToInt(Max.X));
    const int32 Y0 = FMath::Max(0, FMath::FloorToInt(Min.Y)), Y1 = FMath::Min(H - 1, FMath::CeilToInt(Max.Y));
    for (int32 Y = Y0; Y <= Y1; ++Y)
        for (int32 X = X0; X <= X1; ++X)
        {
            const float CX = FMath::Clamp(FMath::Min(X + 1.f, (float)Max.X) - FMath::Max((float)X, (float)Min.X), 0.f, 1.f);
            const float CY = FMath::Clamp(FMath::Min(Y + 1.f, (float)Max.Y) - FMath::Max((float)Y, (float)Min.Y), 0.f, 1.f);
            if (CX * CY > 0) Blend(X, Y, C, CX * CY * Alpha);
        }
}

static float HallHash(int32 X, int32 Y, int32 Seed)
{
    uint32 H = (uint32)X * 374761393u + (uint32)Y * 668265263u + (uint32)Seed * 2246822519u;
    H = (H ^ (H >> 13)) * 1274126177u;
    H ^= H >> 16;
    return (H & 0xFFFFFF) / float(0xFFFFFF);
}

float HallNoise(float X, float Y, int32 Seed)
{
    const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
    float FX = X - X0, FY = Y - Y0;
    FX = FX * FX * (3 - 2 * FX); FY = FY * FY * (3 - 2 * FY);
    const float A = HallHash(X0, Y0, Seed), B = HallHash(X0 + 1, Y0, Seed), C = HallHash(X0, Y0 + 1, Seed), D = HallHash(X0 + 1, Y0 + 1, Seed);
    return FMath::Lerp(FMath::Lerp(A, B, FX), FMath::Lerp(C, D, FX), FY);
}

float HallFbm(float X, float Y, int32 Octaves, int32 Seed)
{
    float Sum = 0, Amp = .5f, Norm = 0;
    for (int32 O = 0; O < Octaves; ++O)
    {
        Sum += HallNoise(X, Y, Seed + O * 31) * Amp; Norm += Amp;
        X *= 2.03f; Y *= 2.03f; Amp *= .5f;
    }
    return Sum / FMath::Max(Norm, 1e-4f);
}

void HallHeightToNormal(const TArray<float>& Height, int32 W, int32 H, float Strength, bool bWrap, FHallImage& Out)
{
    Out.Init(W, H, FLinearColor(.5f, .5f, 1.f, 1.f));
    auto Get = [&](int32 X, int32 Y)
    {
        if (bWrap) { X = ((X % W) + W) % W; Y = ((Y % H) + H) % H; }
        else { X = FMath::Clamp(X, 0, W - 1); Y = FMath::Clamp(Y, 0, H - 1); }
        return Height[Y * W + X];
    };
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float DX = (Get(X + 1, Y) - Get(X - 1, Y)) * .5f * Strength;
            const float DY = (Get(X, Y + 1) - Get(X, Y - 1)) * .5f * Strength;
            const FVector N = FVector(-DX, -DY, 1.f).GetSafeNormal();
            Out.At(X, Y) = FLinearColor(N.X * .5f + .5f, N.Y * .5f + .5f, N.Z * .5f + .5f, 1.f);
        }
}

UTexture2D* HallMakeTexture(const FHallImage& Img, bool bSRGB, bool bNormalMap, const TCHAR* Name, bool bWrap)
{
    if (Img.W <= 0 || Img.H <= 0) return nullptr;
    UTexture2D* T = UTexture2D::CreateTransient(Img.W, Img.H, PF_B8G8R8A8, MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), FName(Name)));
    if (!T) return nullptr;
    T->SRGB = bSRGB;
    T->CompressionSettings = bNormalMap ? TC_Normalmap : TC_Default;
    T->AddressX = T->AddressY = bWrap ? TA_Wrap : TA_Clamp;
    T->Filter = TF_Trilinear;
    T->LODGroup = TEXTUREGROUP_World;
    FTexturePlatformData* PD = T->GetPlatformData();
    if (!PD || PD->Mips.Num() == 0) return nullptr;
    TArray<FLinearColor> Level = Img.Px;
    int32 LW = Img.W, LH = Img.H;
    for (int32 Mip = 0; ; ++Mip)
    {
        TArray<FColor> Encoded; Encoded.SetNumUninitialized(LW * LH);
        for (int32 I = 0; I < LW * LH; ++I)
        {
            FLinearColor C = Level[I]; C.A = 1.f;
            Encoded[I] = bSRGB ? C.ToFColor(true) : C.QuantizeRound();
        }
        FTexture2DMipMap* Target = nullptr;
        if (Mip == 0) Target = &PD->Mips[0];
        else { Target = new FTexture2DMipMap(LW, LH, 1); PD->Mips.Add(Target); }
        Target->SizeX = LW; Target->SizeY = LH;
        Target->BulkData.Lock(LOCK_READ_WRITE);
        void* Data = Target->BulkData.Realloc(int64(LW) * LH * 4);
        FMemory::Memcpy(Data, Encoded.GetData(), int64(LW) * LH * 4);
        Target->BulkData.Unlock();
        if ((LW == 1 && LH == 1) || Mip >= 11) break;
        const int32 NW = FMath::Max(1, LW / 2), NH = FMath::Max(1, LH / 2);
        TArray<FLinearColor> Next; Next.SetNumUninitialized(NW * NH);
        for (int32 Y = 0; Y < NH; ++Y)
            for (int32 X = 0; X < NW; ++X)
            {
                const int32 SX0 = FMath::Min(X * 2, LW - 1), SX1 = FMath::Min(X * 2 + 1, LW - 1);
                const int32 SY0 = FMath::Min(Y * 2, LH - 1), SY1 = FMath::Min(Y * 2 + 1, LH - 1);
                Next[Y * NW + X] = (Level[SY0 * LW + SX0] + Level[SY0 * LW + SX1] + Level[SY1 * LW + SX0] + Level[SY1 * LW + SX1]) * .25f;
            }
        Level = MoveTemp(Next); LW = NW; LH = NH;
    }
    T->UpdateResource();
    return T;
}

// ---------------------------------------------------------------------------
// Stroke font
namespace
{
struct FHallFont
{
    TMap<TCHAR, TArray<TArray<FVector2D>>> Glyphs;
    FHallFont()
    {
        const TCHAR* Defs[][2] = {
            {TEXT("A"), TEXT("0,0 2,6 4,0|0.8,2.4 3.2,2.4")},
            {TEXT("B"), TEXT("0,0 0,6 3,6 4,5.2 4,3.8 3,3 0,3|3,3 4,2.2 4,0.8 3,0 0,0")},
            {TEXT("C"), TEXT("4,5 3,6 1,6 0,5 0,1 1,0 3,0 4,1")},
            {TEXT("D"), TEXT("0,0 0,6 2.5,6 4,4.5 4,1.5 2.5,0 0,0")},
            {TEXT("E"), TEXT("4,6 0,6 0,0 4,0|0,3 3,3")},
            {TEXT("F"), TEXT("4,6 0,6 0,0|0,3 3,3")},
            {TEXT("G"), TEXT("4,5 3,6 1,6 0,5 0,1 1,0 3,0 4,1 4,2.6 2.4,2.6")},
            {TEXT("H"), TEXT("0,0 0,6|4,0 4,6|0,3 4,3")},
            {TEXT("I"), TEXT("2,0 2,6|1,6 3,6|1,0 3,0")},
            {TEXT("J"), TEXT("4,6 4,1 3,0 1,0 0,1")},
            {TEXT("K"), TEXT("0,0 0,6|4,6 0,2.2|1.3,3.3 4,0")},
            {TEXT("L"), TEXT("0,6 0,0 4,0")},
            {TEXT("M"), TEXT("0,0 0,6 2,3 4,6 4,0")},
            {TEXT("N"), TEXT("0,0 0,6 4,0 4,6")},
            {TEXT("O"), TEXT("1,0 0,1 0,5 1,6 3,6 4,5 4,1 3,0 1,0")},
            {TEXT("P"), TEXT("0,0 0,6 3,6 4,5 4,4 3,3 0,3")},
            {TEXT("Q"), TEXT("1,0 0,1 0,5 1,6 3,6 4,5 4,1 3,0 1,0|2.5,1.5 4,-0.3")},
            {TEXT("R"), TEXT("0,0 0,6 3,6 4,5 4,4 3,3 0,3|2,3 4,0")},
            {TEXT("S"), TEXT("4,5 3,6 1,6 0,5 0,4 1,3 3,3 4,2 4,1 3,0 1,0 0,1")},
            {TEXT("T"), TEXT("0,6 4,6|2,6 2,0")},
            {TEXT("U"), TEXT("0,6 0,1 1,0 3,0 4,1 4,6")},
            {TEXT("V"), TEXT("0,6 2,0 4,6")},
            {TEXT("W"), TEXT("0,6 1,0 2,4 3,0 4,6")},
            {TEXT("X"), TEXT("0,0 4,6|0,6 4,0")},
            {TEXT("Y"), TEXT("0,6 2,3 4,6|2,3 2,0")},
            {TEXT("Z"), TEXT("0,6 4,6 0,0 4,0")},
            {TEXT("0"), TEXT("1,0 0,1 0,5 1,6 3,6 4,5 4,1 3,0 1,0")},
            {TEXT("1"), TEXT("1,5 2,6 2,0|1,0 3,0")},
            {TEXT("2"), TEXT("0,5 1,6 3,6 4,5 4,4 0,0 4,0")},
            {TEXT("3"), TEXT("0,5 1,6 3,6 4,5 4,4 3,3 4,2 4,1 3,0 1,0 0,1|1.5,3 3,3")},
            {TEXT("4"), TEXT("3,0 3,6 0,2 4,2")},
            {TEXT("5"), TEXT("4,6 0,6 0,3.5 3,3.5 4,2.5 4,1 3,0 0,0")},
            {TEXT("6"), TEXT("4,5 3,6 1,6 0,5 0,1 1,0 3,0 4,1 4,2 3,3 0,3")},
            {TEXT("7"), TEXT("0,6 4,6 1.5,0")},
            {TEXT("8"), TEXT("1,3 0,4 0,5 1,6 3,6 4,5 4,4 3,3 1,3 0,2 0,1 1,0 3,0 4,1 4,2 3,3")},
            {TEXT("9"), TEXT("0,1 1,0 3,0 4,1 4,5 3,6 1,6 0,5 0,4 1,3 4,3")},
            {TEXT("."), TEXT("2,0 2,0.35")},
            {TEXT("-"), TEXT("1,3 3,3")},
            {TEXT("/"), TEXT("0,0 4,6")},
            {TEXT("!"), TEXT("2,6 2,1.6|2,0.3 2,0")},
            {TEXT("&"), TEXT("4,0 1,4 1,5 2,6 3,5 3,4 0,2 0,1 1,0 2,0 4,2")},
        };
        for (const auto& Def : Defs)
        {
            TArray<TArray<FVector2D>> Lines;
            TArray<FString> Parts; FString(Def[1]).ParseIntoArray(Parts, TEXT("|"));
            for (const FString& Part : Parts)
            {
                TArray<FString> Pts; Part.ParseIntoArrayWS(Pts);
                TArray<FVector2D>& Line = Lines.AddDefaulted_GetRef();
                for (const FString& Pt : Pts)
                {
                    FString A, B; Pt.Split(TEXT(","), &A, &B);
                    Line.Add(FVector2D(FCString::Atof(*A), FCString::Atof(*B)));
                }
            }
            Glyphs.Add(Def[0][0], MoveTemp(Lines));
        }
        Glyphs.Add(TEXT(' '), {});
    }
};
}

const TArray<TArray<FVector2D>>& HallGlyph(TCHAR Ch)
{
    static FHallFont Font;
    static TArray<TArray<FVector2D>> Empty;
    const TArray<TArray<FVector2D>>* G = Font.Glyphs.Find(FChar::ToUpper(Ch));
    return G ? *G : Empty;
}

static float HallAdvance(TCHAR Ch, const FHallTextStyle& S)
{
    const float U = S.Height / 6.f;
    return (Ch == TEXT(' ') ? 2.6f : 4.f * S.WidthScale) * U + S.Spacing * S.Height;
}

float HallTextWidth(const FString& Text, const FHallTextStyle& Style)
{
    float Width = 0;
    for (TCHAR Ch : Text) Width += HallAdvance(Ch, Style);
    return FMath::Max(0.f, Width - Style.Spacing * Style.Height);
}

void HallDrawText(FHallImage& Img, const FString& Text, const FVector2D& Origin, const FHallTextStyle& Style, const FLinearColor& C,
    TArray<TArray<FVector2D>>* OutStrokes)
{
    const float U = Style.Height / 6.f;
    float Cursor = 0;
    int32 Index = 0;
    for (TCHAR Ch : Text)
    {
        for (const TArray<FVector2D>& Line : HallGlyph(Ch))
        {
            TArray<FVector2D> P;
            for (int32 I = 0; I < Line.Num(); ++I)
            {
                const FVector2D G = Line[I];
                // Subdivide so a wobble reads as a hand-painted edge.
                if (I > 0 && Style.Wobble > 0)
                {
                    const FVector2D Prev = Line[I - 1];
                    const int32 Steps = FMath::Clamp(FMath::CeilToInt((G - Prev).Size() * 2), 1, 12);
                    for (int32 S = 1; S < Steps; ++S)
                    {
                        const FVector2D M = FMath::Lerp(Prev, G, S / float(Steps));
                        FVector2D Q(Origin.X + Cursor + M.X * U * Style.WidthScale, Origin.Y - M.Y * U);
                        Q.X += (HallNoise(Q.X * .08f, Q.Y * .08f, Style.Seed + Index) - .5f) * Style.Wobble;
                        Q.Y += (HallNoise(Q.Y * .08f, Q.X * .08f, Style.Seed + 7 + Index) - .5f) * Style.Wobble;
                        P.Add(Q);
                    }
                }
                FVector2D Q(Origin.X + Cursor + G.X * U * Style.WidthScale, Origin.Y - G.Y * U);
                if (Style.Wobble > 0)
                {
                    Q.X += (HallNoise(Q.X * .08f, Q.Y * .08f, Style.Seed + Index) - .5f) * Style.Wobble;
                    Q.Y += (HallNoise(Q.Y * .08f, Q.X * .08f, Style.Seed + 7 + Index) - .5f) * Style.Wobble;
                }
                P.Add(Q);
            }
            Img.Stroke(P, Style.Stroke, C);
            if (Style.EndBlob > 0 && P.Num() > 1)
            {
                Img.Ellipse(P[0], FVector2D(Style.EndBlob), 0, C);
                Img.Ellipse(P.Last(), FVector2D(Style.EndBlob), 0, C);
            }
            if (OutStrokes) OutStrokes->Add(P);
        }
        Cursor += HallAdvance(Ch, Style);
        ++Index;
    }
}
