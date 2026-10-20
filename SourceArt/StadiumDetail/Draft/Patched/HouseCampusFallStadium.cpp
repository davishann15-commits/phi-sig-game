// The Calder State football stadium on homecoming night, Saturday October 18 1986, just after the final whistle: the
// painted field (at 0.55 scale, all the old bowl holds) with its cinder track, slingshot goalposts and team benches; the
// incandescent bulb scoreboard (GEESE 24, DUNMORE 17, 00:00, QTR 4, HOMECOMING '86); the press box sign and bunting;
// litter on the stands, bedsheets on the podium wall and pennants round the rim; the north gates dressed with ticket
// booths, hay bales, corn shocks, pumpkins and jack-o'-lanterns; and out on the green, the tissue-paper homecoming float
// and the marching band's equipment trailer. Replaces the baseball diamond, the VOLS lettering and the orange T.
#include "HouseStadiumDetail.h"
#include "HouseCampusFallParts.h"
#include "HouseCampusFall.h"
#include "HouseCampusFallKit.h"
#include "HouseExteriorCampus.h"
#include "HouseCampusInteriors.h"
#include "HouseCampusPolish.h"
#include "HouseSurroundingsPolish.h"
#include "HouseExteriorSiteLayout.h"

namespace HouseCampusFallStadiumImpl
{
constexpr float Ground = -60.f, Turf = -56.f;
constexpr float BowlRX = 5600.f, BowlRY = 3800.f;
constexpr int32 BowlSeg = 40;
constexpr float FieldX = HouseCampus::StadiumX, FieldY = HouseCampus::StadiumY;
constexpr float Yard = 50.29f, HalfLength = 60.f * Yard, HalfWidth = 26.667f * Yard, Border = 160.f;

FVector Bowl(float A, float R, float Z) { return FVector(FieldX + FMath::Cos(A) * BowlRX * R, FieldY + FMath::Sin(A) * BowlRY * R, Ground + Z); }
/** Outward normal of the bowl's ellipse at angle A. */
FVector BowlOut(float A) { return FVector(FMath::Cos(A) / BowlRX, FMath::Sin(A) / BowlRY, 0.).GetSafeNormal(); }
/** The reader's right for someone facing horizontal direction F. */
FVector RightOf(const FVector& F) { return FVector(-F.Y, F.X, 0.).GetSafeNormal(); }
const FHallUV& Flat() { return HouseCampusFallKit::PlainUV(); }
FName Key(const TCHAR* Base, int32 Index) { return FName(*FString::Printf(TEXT("%s%d"), Base, Index)); }

/** An incandescent bulb: a small domed disc facing N. */
void Bulb(FHallMesh& M, FName K, const FVector& C, const FVector& N, const FVector& Right, float R)
{
    const FVector V = FVector::CrossProduct(N, Right);
    TArray<FVector> Rim;
    for (int32 J = 0; J < 8; ++J) { const float A = J * PI / 4.f; Rim.Add(C + Right * (FMath::Cos(A) * R) + V * (FMath::Sin(A) * R)); }
    HouseCampusFallKit::Fan(M, K, C + N * (R * .3f), Rim, N);
}

/** 5 x 7 bulb lettering: Origin is the face's bottom-left, Left/Top in cm along Right and up; dark bulbs fill each cell. */
void BulbText(FHallMesh& M, const FString& Text, const FVector& Origin, const FVector& Right, const FVector& N, float Left, float Top, float Pitch, FName OnKey)
{
    for (int32 Ch = 0; Ch < Text.Len(); ++Ch)
    {
        const uint8* Rows = HouseCampusFallKit::BulbGlyph(Text[Ch]);
        for (int32 Row = 0; Row < 7; ++Row)
            for (int32 Col = 0; Col < 5; ++Col)
            {
                const bool bOn = ((Rows[Row] >> (4 - Col)) & 1) != 0;
                const FVector C = Origin + Right * (Left + (Ch * 6 + Col + .5f) * Pitch) + FVector(0, 0, Top - (Row + .5f) * Pitch) + N * 1.5f;
                Bulb(M, bOn ? OnKey : FName(TEXT("CF_BulbOff")), C, N, Right, Pitch * .36f);
            }
    }
}
float BulbWidth(int32 Chars, float Pitch) { return (Chars * 6 - 1) * Pitch; }

/** A rough ellipsoid in a local frame (tissue-paper pomps when Pomp > 0). */
void Blob(FHallMesh& M, FName K, const FVector& C, const FVector& AxX, const FVector& AxY, const FVector& AxZ, const FVector& Radii, int32 Seg, int32 Rings, float Pomp, int32 Seed)
{
    auto Local = [&](float Theta, float Phi)
    {
        const FVector D(FMath::Cos(Phi) * FMath::Cos(Theta), FMath::Cos(Phi) * FMath::Sin(Theta), FMath::Sin(Phi));
        const float Bump = Pomp > 0.f ? 1.f + Pomp * (FMath::Abs(FMath::Sin(Theta * 9.f + Seed) * FMath::Sin(Phi * 7.f + Seed * .7f)) - .4f) : 1.f;
        return D * Bump;
    };
    auto World = [&](const FVector& D) { return C + AxX * (D.X * Radii.X) + AxY * (D.Y * Radii.Y) + AxZ * (D.Z * Radii.Z); };
    for (int32 J = 0; J < Rings; ++J)
        for (int32 I = 0; I < Seg; ++I)
        {
            const float T0 = 2.f * PI * I / Seg, T1 = 2.f * PI * (I + 1) / Seg;
            const float P0 = PI * J / Rings - HALF_PI, P1 = PI * (J + 1) / Rings - HALF_PI;
            const FVector D00 = Local(T0, P0), D10 = Local(T1, P0), D11 = Local(T1, P1), D01 = Local(T0, P1);
            const FVector Mid = (D00 + D10 + D11 + D01) * .25f;
            const FVector Nrm = (AxX * (Mid.X / Radii.X) + AxY * (Mid.Y / Radii.Y) + AxZ * (Mid.Z / Radii.Z)).GetSafeNormal();
            M.Quad(K, World(D00), World(D10), World(D11), World(D01), Nrm, Flat());
        }
}

/** A tube along Spine (its rings Radii wide), for the float's neck. */
void Tube(FHallMesh& M, FName K, const TArray<FVector>& Spine, const TArray<float>& Radii, int32 Seg)
{
    TArray<TArray<FVector>> Rings;
    for (int32 J = 0; J < Spine.Num(); ++J)
    {
        const FVector T = (Spine[FMath::Min(J + 1, Spine.Num() - 1)] - Spine[FMath::Max(J - 1, 0)]).GetSafeNormal();
        const FVector Side = FVector::CrossProduct(T, FVector::UpVector).IsNearlyZero() ? FVector(0, 1, 0) : FVector::CrossProduct(T, FVector::UpVector).GetSafeNormal();
        const FVector Up = FVector::CrossProduct(Side, T).GetSafeNormal();
        TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
        for (int32 I = 0; I < Seg; ++I) { const float A = 2.f * PI * I / Seg; Ring.Add(Spine[J] + (Side * FMath::Cos(A) + Up * FMath::Sin(A)) * Radii[J]); }
    }
    for (int32 J = 0; J + 1 < Rings.Num(); ++J)
        for (int32 I = 0; I < Seg; ++I)
        {
            const FVector &A = Rings[J][I], &B = Rings[J][(I + 1) % Seg], &C = Rings[J + 1][(I + 1) % Seg], &D = Rings[J + 1][I];
            M.Quad(K, A, B, C, D, ((A + B + C + D) * .25f - (Spine[J] + Spine[J + 1]) * .5f).GetSafeNormal(), Flat());
        }
}

/** A pumpkin on the ground (or on a bale) at Base; with Candle >= 0 it is a carved, lit jack-o'-lantern facing Yaw. */
void Pumpkin(HouseCampusFallParts::FParts& P, FHallMesh& M, const FVector& Base, float R, float Yaw, int32 Candle, int32 Style)
{
    const float H = R * 1.45f;
    HouseCampusFallKit::Pumpkin(M, TEXT("CF_Pumpkin"), TEXT("CF_Stem"), Base, R, H, 10, Yaw);
    if (Candle < 0) return;
    HouseCampusFallKit::JackFace(M, Key(TEXT("CF_Candle"), Candle % 3), Base, R * 1.005f, H, Yaw, Style);
    ++P.Lanterns;
}

/** A paper cup standing, or on its side. */
void Cup(FHallMesh& M, FName K, const FVector& Base, float Yaw, bool bLying)
{
    if (!bLying) { M.Cylinder(K, Base, Base + FVector(0, 0, 11.5f), 4.1f, 7, Flat()); return; }
    const FVector D = FRotator(0.f, Yaw, 0.f).Vector();
    M.Cylinder(K, Base + FVector(0, 0, 4) - D * 5.7f, Base + FVector(0, 0, 4) + D * 5.7f, 4.f, 7, Flat());
}

/** A homecoming programme: lying open flat, or tented where it fell. */
void Programme(FHallMesh& M, const FVector& Base, float Yaw, bool bTent)
{
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    const FVector X = Q.GetForwardVector(), Y = Q.GetRightVector();
    if (!bTent)
    {
        M.Quad(TEXT("CF_Program"), Base - X * 21.f - Y * 14.f, Base + X * 21.f - Y * 14.f, Base + X * 21.f + Y * 14.f, Base - X * 21.f + Y * 14.f, FVector::UpVector, Flat());
        return;
    }
    const FVector Ridge = FVector(0, 0, 11.f);
    for (const float S : { -1.f, 1.f })
    {
        const FVector Foot = Base + X * (S * 13.f);
        const FVector Nrm = (X * S * 11.f + FVector(0, 0, 13.f)).GetSafeNormal();
        M.Quad(TEXT("CF_Program"), Foot - Y * 14.f, Foot + Y * 14.f, Base + Ridge + Y * 14.f, Base + Ridge - Y * 14.f, Nrm, Flat());
    }
}

struct FArch { FVector Centre, Out, Along; };
/** The frame of the concourse arch on bowl segment K (its foot on the ground, Out away from the field). */
FArch ArchAt(int32 K)
{
    const float A = K * 2.f * PI / BowlSeg, B = (K + 1) * 2.f * PI / BowlSeg;
    const FVector A0 = Bowl(A, 1.f, 0.f), B0 = Bowl(B, 1.f, 0.f);
    FArch Arch;
    Arch.Out = FVector(FMath::Cos((A + B) * .5f), FMath::Sin((A + B) * .5f), 0.);
    Arch.Centre = (A0 + B0) * .5f + Arch.Out * 9.f;
    Arch.Along = (B0 - A0).GetSafeNormal();
    return Arch;
}

/** A small glowing bead (a string-light bulb): an octahedron. */
void Bead(FHallMesh& M, FName K, const FVector& C, float R)
{
    const FVector Px = FVector(R, 0, 0), Py = FVector(0, R, 0), Pz = FVector(0, 0, R * 1.3f);
    for (const float Sx : { -1.f, 1.f }) for (const float Sy : { -1.f, 1.f }) for (const float Sz : { -1.f, 1.f })
        HouseCampusFallKit::Tri(M, K, C + Px * Sx, C + Py * Sy, C + Pz * Sz, FVector(Sx, Sy, Sz).GetSafeNormal());
}

/** A cord with small triangular pennants hanging from it, A to B, sagging Sag; with bulbs, a string of lights along it too. */
void PennantString(HouseCampusFallParts::FParts& P, FHallMesh& M, const FVector& A, const FVector& B, float Sag, float Spacing, float Size, int32 ColourSeed, bool bBulbs = false)
{
    constexpr int32 Pieces = 10;
    for (int32 J = 0; J < Pieces; ++J)
        HouseCampusFallKit::Bar(M, TEXT("CF_Cord"), HouseCampusFallKit::Cord(A, B, J / float(Pieces), Sag), HouseCampusFallKit::Cord(A, B, (J + 1) / float(Pieces), Sag), 1.2f, 1.2f, FVector::UpVector);
    if (bBulbs)
    {
        const int32 Bulbs = FMath::Max(2, FMath::RoundToInt(FVector::Dist2D(A, B) / 45.f));
        for (int32 J = 1; J < Bulbs; ++J) Bead(M, TEXT("CF_StringBulb"), HouseCampusFallKit::Cord(A, B, J / float(Bulbs), Sag) - FVector(0, 0, 4.f), 2.6f);
    }
    const FVector Along = (B - A).GetSafeNormal2D();
    const int32 Count = FMath::Max(1, FMath::RoundToInt(FVector::Dist2D(A, B) / Spacing));
    for (int32 J = 0; J < Count; ++J)
    {
        HouseCampusFallParts::FCloth C;
        C.Front = C.Back = Key(TEXT("CF_Pennant"), (J + ColourSeed) % 3);
        C.Origin = HouseCampusFallKit::Cord(A, B, (J + .5f) / Count, Sag) - Along * (Size * .4f) - FVector(0, 0, 1.f);
        C.Right = Along; C.Down = -FVector::UpVector;
        C.Width = Size * .8f; C.Height = Size; C.Cols = 1; C.Rows = 2; C.Taper = 1.f; C.Amp = 1.3f;
        P.Cloths.Add(C);
    }
}

/** A sheet tied flat on a wall: centred at Top (its top edge's middle), Right for a reader facing it, facing out along N. */
void WallSheet(HouseCampusFallParts::FParts& P, int32 Kind, const FVector& Top, const FVector& Right, float Width, float Height)
{
    HouseCampusFallParts::FCloth C;
    C.Front = Key(TEXT("CF_Sheet"), Kind); C.Back = TEXT("CF_SheetBack");
    C.Origin = Top - Right * (Width * .5f);
    C.Right = Right; C.Down = -FVector::UpVector;
    C.Width = Width; C.Height = Height; C.Cols = 8; C.Rows = 4; C.Pinned = 1.f; C.Amp = .7f;
    P.Cloths.Add(C);
}

// ------------------------------------------------------------------ the field
void Field(HouseCampusFallParts::FParts& P)
{
    FHallMesh& M = P.Field;
    // Grass out to the track, the painted field over it, and the cinder track with its concrete kerb round the bowl.
    TArray<FVector> Rim;
    constexpr int32 Ring = 96;
    for (int32 K = 0; K < Ring; ++K) Rim.Add(Bowl(2.f * PI * K / Ring, .668f, Turf - Ground - .6f));
    HouseCampusFallKit::Fan(M, TEXT("CF_Grass"), FVector(FieldX, FieldY, Turf - .6f), Rim, FVector::UpVector);
    const float X0 = FieldX - HalfLength - Border, X1 = FieldX + HalfLength + Border, YN = FieldY + HalfWidth + Border, YS = FieldY - HalfWidth - Border;
    // Texture x runs east, y runs north (see PaintField).
    M.QuadUV(TEXT("CF_Field"), FVector(X0, YN, Turf), FVector(X1, YN, Turf), FVector(X1, YS, Turf), FVector(X0, YS, Turf), FVector::UpVector,
        FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0), FVector::ForwardVector);
    for (int32 K = 0; K < Ring; ++K)
    {
        const float A = 2.f * PI * K / Ring, B = 2.f * PI * (K + 1) / Ring;
        M.Quad(TEXT("CF_Track"), Bowl(A, .665f, 4.5f), Bowl(B, .665f, 4.5f), Bowl(B, .693f, 4.5f), Bowl(A, .693f, 4.5f), FVector::UpVector, Flat());
        HouseCampusFallKit::Bar(M, TEXT("CF_Concrete"), Bowl(A, .6635f, 6.f), Bowl(B, .6635f, 6.f), 9.f, 8.f, FVector::UpVector);
    }
    // Slingshot goalposts in gold on each end line, padded in purple, with streamers on the uprights.
    for (const float Out : { -1.f, 1.f })
    {
        const float EndX = FieldX + Out * HalfLength, Bx = EndX + Out * 170.f;
        TArray<FVector> Path = { FVector(Bx, FieldY, Turf), FVector(Bx, FieldY, Turf + 100.f) };
        for (int32 K = 1; K <= 6; ++K) { const float T = K / 6.f * HALF_PI; Path.Add(FVector(Bx - Out * 85.f * (1.f - FMath::Cos(T)), FieldY, Turf + 100.f + 85.f * FMath::Sin(T))); }
        for (int32 K = 1; K <= 6; ++K) { const float T = K / 6.f * HALF_PI; Path.Add(FVector(Bx - Out * (85.f + 85.f * FMath::Sin(T)), FieldY, Turf + 185.f + 85.f * (1.f - FMath::Cos(T)))); }
        Path.Add(FVector(EndX, FieldY, Turf + 282.f));
        for (int32 K = 1; K < Path.Num(); ++K)
        {
            const FVector D = (Path[K] - Path[K - 1]).GetSafeNormal();
            M.Cylinder(TEXT("CF_PostGold"), Path[K - 1] - D * 3.f, Path[K] + D * 3.f, 10.f, 10, Flat());
        }
        M.Cylinder(TEXT("CF_PostPad"), FVector(Bx, FieldY, Turf), FVector(Bx, FieldY, Turf + 150.f), 21.f, 12, Flat());
        M.Cylinder(TEXT("CF_PostGold"), FVector(EndX, FieldY - 228.f, Turf + 280.f), FVector(EndX, FieldY + 228.f, Turf + 280.f), 7.f, 10, Flat());
        for (const float Side : { -1.f, 1.f })
        {
            const FVector Foot(EndX, FieldY + Side * 225.f, Turf + 274.f);
            M.Cylinder(TEXT("CF_PostGold"), Foot, Foot + FVector(0, 0, 650.f), 6.f, 10, Flat());
            HouseCampusFallParts::FCloth Streamer;
            Streamer.Front = Streamer.Back = Side < 0 ? FName(TEXT("CF_Pennant0")) : FName(TEXT("CF_Pennant1"));
            Streamer.Origin = Foot + FVector(0, 0, 640.f) - FVector(0, 1, 0) * 4.f;
            Streamer.Right = FVector(0, 1, 0); Streamer.Down = FVector(Out * .5f, 0, -1).GetSafeNormal();
            Streamer.Width = 8.f; Streamer.Height = 75.f; Streamer.Cols = 1; Streamer.Rows = 5; Streamer.Amp = 2.2f;
            P.Cloths.Add(Streamer);
        }
        // Pylons at the corners of the end zone.
        for (const float X : { EndX, EndX - Out * 10.f * Yard })
            for (const float Y : { FieldY + HalfWidth + 6.f, FieldY - HalfWidth - 6.f })
                M.BoxMinMax(TEXT("CF_Pumpkin"), FVector(X - 5.f, Y - 5.f, Turf), FVector(X + 5.f, Y + 5.f, Turf + 46.f), Flat());
    }
    // Team benches behind each sideline: the Geese on the press-box side, Dunmore across on the scoreboard side.
    FRandomStream R(1018);
    for (const float Side : { 1.f, -1.f })
    {
        const float Y = FieldY + Side * (HalfWidth + 430.f);
        for (int32 B = -1; B <= 1; ++B)
        {
            const float BX = FieldX + B * 560.f;
            HouseCampusFallKit::Bar(M, TEXT("CF_Bench"), FVector(BX - 250.f, Y, Turf + 44.f), FVector(BX + 250.f, Y, Turf + 44.f), 32.f, 4.f, FVector::UpVector);
            HouseCampusFallKit::Bar(M, TEXT("CF_Bench"), FVector(BX - 250.f, Y + Side * 26.f, Turf + 22.f), FVector(BX + 250.f, Y + Side * 26.f, Turf + 22.f), 22.f, 3.f, FVector::UpVector);
            for (float LX = BX - 220.f; LX <= BX + 225.f; LX += 147.f)
                for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Bar(M, TEXT("CF_Bench"), FVector(LX, Y + S * 13.f, Turf), FVector(LX, Y + S * 13.f, Turf + 43.f), 3.f, 3.f, FVector::ForwardVector);
            // Towels and a cup or two left on the bench.
            for (int32 T = 0; T < 3; ++T)
            {
                const FVector At(BX + R.FRandRange(-220.f, 220.f), Y + R.FRandRange(-8.f, 8.f), Turf + 46.3f);
                if (R.FRand() < .5f) M.Quad(TEXT("CF_CupWhite"), At + FVector(-24, -14, 0), At + FVector(24, -14, 0), At + FVector(24, 14, 0), At + FVector(-24, 14, 0), FVector::UpVector, Flat());
                else Cup(M, TEXT("CF_CupWhite"), At, R.FRandRange(0.f, 360.f), R.FRand() < .4f);
            }
        }
        // Water coolers on a folding table at each end, and the cups dropped around them.
        for (const float End : { -1.f, 1.f })
        {
            const FVector Table(FieldX + End * 980.f, Y + Side * 10.f, Turf);
            M.BoxMinMax(TEXT("CF_Aluminium"), Table + FVector(-90, -35, 72), Table + FVector(90, 35, 76), Flat());
            for (const float LX : { -82.f, 82.f }) for (const float LY : { -28.f, 28.f }) HouseCampusFallKit::Bar(M, TEXT("CF_Aluminium"), Table + FVector(LX, LY, 0), Table + FVector(LX, LY, 72), 2.5f, 2.5f, FVector::ForwardVector);
            for (const float CX : { -45.f, 40.f })
            {
                const FVector Base = Table + FVector(CX, 0, 76);
                M.Cylinder(TEXT("CF_Cooler"), Base, Base + FVector(0, 0, 52), 21.f, 14, Flat());
                M.Cylinder(TEXT("CF_CupWhite"), Base + FVector(0, 0, 52), Base + FVector(0, 0, 58), 22.f, 14, Flat());
                M.Cylinder(TEXT("CF_Bench"), Base + FVector(0, -21, 9), Base + FVector(0, -26, 9), 2.f, 6, Flat());
            }
            for (int32 C = 0; C < 22; ++C)
            {
                const FVector At = Table + FVector(R.FRandRange(-160.f, 160.f), R.FRandRange(-130.f, 110.f) * Side, 0.4f);
                Cup(M, R.FRand() < .8f ? FName(TEXT("CF_CupWhite")) : FName(TEXT("CF_CupPurple")), At, R.FRandRange(0.f, 360.f), R.FRand() < .7f);
            }
        }
    }
    // Programmes and cups the wind has spread across the turf and the track.
    for (int32 K = 0; K < 70; ++K)
    {
        const float A = R.FRandRange(0.f, 2.f * PI), Rad = R.FRandRange(.42f, .68f);
        const FVector At = Bowl(A, Rad, (Rad > .665f ? 4.9f : Turf - Ground + .4f));
        if (K % 3 == 0) Programme(M, At, R.FRandRange(0.f, 360.f), K % 2 == 0);
        else Cup(M, TEXT("CF_CupWhite"), At, R.FRandRange(0.f, 360.f), true);
    }
}

// ------------------------------------------------------------------ the bulb scoreboard
void Scoreboard(HouseCampusFallParts::FParts& P)
{
    if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=2)return;
    FHallMesh& M = P.Score;
    constexpr float W = 1540.f, H = 770.f;
    const FVector Origin(1630.f, -16972.f, 1320.f), Right(1, 0, 0), N(0, 1, 0);
    // A black face over the old panel, the gold frame round it.
    M.Quad(TEXT("EX_Dark"), Origin, Origin + Right * W, Origin + Right * W + FVector(0, 0, H), Origin + FVector(0, 0, H), N, Flat());
    auto Frame = [&](float L, float B, float R2, float T, float Width)
    {
        const FVector O = Origin + N * 2.f;
        HouseCampusFallKit::Bar(M, TEXT("CF_Gold"), O + Right * L + FVector(0, 0, B), O + Right * R2 + FVector(0, 0, B), Width, 4.f, N);
        HouseCampusFallKit::Bar(M, TEXT("CF_Gold"), O + Right * L + FVector(0, 0, T), O + Right * R2 + FVector(0, 0, T), Width, 4.f, N);
        HouseCampusFallKit::Bar(M, TEXT("CF_Gold"), O + Right * L + FVector(0, 0, B), O + Right * L + FVector(0, 0, T), Width, 4.f, N);
        HouseCampusFallKit::Bar(M, TEXT("CF_Gold"), O + Right * R2 + FVector(0, 0, B), O + Right * R2 + FVector(0, 0, T), Width, 4.f, N);
    };
    Frame(-6.f, -6.f, W + 6.f, H + 6.f, 14.f);
    // HOMECOMING '86 across the top, the teams and the final score, the clock run out and the quarter.
    auto Centred = [&](const FString& Text, float CentreX, float Top, float Pitch, FName On)
    {
        BulbText(M, Text, Origin, Right, N, CentreX - BulbWidth(Text.Len(), Pitch) * .5f, Top, Pitch, On);
    };
    Centred(TEXT("HOMECOMING '86"), W * .5f, 728.f, 15.f, TEXT("CF_BulbOn"));
    Centred(HouseCampusFall::Team(), 330.f, 598.f, 12.f, TEXT("CF_BulbOn"));
    Centred(HouseCampusFall::Rival(), 1210.f, 598.f, 12.f, TEXT("CF_BulbOn"));
    Centred(TEXT("24"), 330.f, 492.f, 23.f, TEXT("CF_BulbOn"));
    Centred(TEXT("17"), 1210.f, 492.f, 23.f, TEXT("CF_BulbOn"));
    Centred(TEXT("00:00"), W * .5f, 500.f, 14.f, TEXT("CF_BulbOn"));
    Centred(TEXT("QTR 4"), W * .5f, 378.f, 12.f, TEXT("CF_BulbOn"));
    Frame(330.f - 160.f, 312.f, 330.f + 160.f, 508.f, 6.f);
    Frame(1210.f - 160.f, 312.f, 1210.f + 160.f, 508.f, 6.f);
    Frame(W * .5f - 225.f, 284.f, W * .5f + 225.f, 512.f, 6.f);
    // The school's name board along the bottom.
    HouseCampusFallKit::Card(M, TEXT("CF_PressSign"), Origin + Right * (W * .5f) + FVector(0, 0, 140.f) + N * 1.f, Right, FVector::UpVector, 1360.f, 170.f, N);
    // Marquee bulbs chasing round the edge.
    int32 Index = 0;
    auto Chase = [&](const FVector2D& At) { Bulb(M, Key(TEXT("CF_Chase"), Index++ % 3), Origin + Right * At.X + FVector(0, 0, At.Y) + N * 2.5f, N, Right, 6.f); };
    constexpr float Inset = 16.f, Step = 29.6f;
    for (float X = Inset; X <= W - Inset + 1.f; X += Step) Chase(FVector2D(X, H - Inset));
    for (float Z = H - Inset - Step; Z >= Inset - 1.f; Z -= Step) Chase(FVector2D(W - Inset, Z));
    for (float X = W - Inset - Step; X >= Inset - 1.f; X -= Step) Chase(FVector2D(X, Inset));
    for (float Z = Inset + Step; Z <= H - Inset - Step + 1.f; Z += Step) Chase(FVector2D(Inset, Z));
}

// ------------------------------------------------------------------ the stands
void Stands(HouseCampusFallParts::FParts& P)
{
    FHallMesh& M = P.Stands;
    const bool bBenches = HouseSurroundings::Enabled();
    // What 9,000 people leave behind: cups, programmes, popcorn boxes, crumpled napkins, hand pennants and cushions.
    FRandomStream R(19861018);
    for (int32 K = 0; K < 1100; ++K)
    {
        const float A = R.FRandRange(0.f, 2.f * PI);
        const float Aisle = FMath::RoundToFloat(A / (PI / 4.f)) * (PI / 4.f);
        const int32 Row = R.RandRange(0, 11), Spot = R.RandRange(0, 9), Kind = R.RandRange(0, 9);
        if (FMath::Abs(A - Aisle) < .035f) continue;
        const int32 DetailSegment=FMath::Clamp(FMath::FloorToInt(A/(2.f*PI/BowlSeg)),0,BowlSeg-1);
        if(HouseStadiumDetail::Aisle(DetailSegment)||HouseStadiumDetail::PortalBand(DetailSegment,Row)||HouseStadiumDetail::CrossAisle(DetailSegment,Row))continue;
        // (None where the walk-in stadium has cut the rows away for its vomitory.)
        if (HouseCampusInteriors::StadiumVomitory(FMath::Clamp(FMath::FloorToInt(A / (2.f * PI / BowlSeg)), 0, BowlSeg - 1), Row)) continue;
        const float R0 = .69f + Row * .024f, Tread = 240.f + Row * 63.f;
        const bool bOnBench = bBenches && Spot >= 4 && Spot < 7;
        const float Rad = bOnBench ? R0 + .01f : Spot < 4 ? R0 + R.FRandRange(.0025f, .0072f) : R0 + R.FRandRange(.0135f, .021f);
        const FVector At = Bowl(A, Rad, Tread + (bOnBench ? (HouseStadiumDetail::Enabled()?45.f:12.f) : 0.f) + .3f);
        const float Yaw = R.FRandRange(0.f, 360.f);
        if (Kind < 4) Cup(M, Kind == 3 ? FName(TEXT("CF_CupPurple")) : FName(TEXT("CF_CupWhite")), At, Yaw, Kind % 2 == 0);
        else if (Kind < 6) Programme(M, At, Yaw, Kind == 5 && !bOnBench);
        else if (Kind == 6)
        {
            const FQuat Q = FRotator(R.FRand() < .5f ? 0.f : 90.f, Yaw, 0.f).Quaternion();
            M.Box(TEXT("CF_Popcorn"), FTransform(Q, At + FVector(0, 0, Q.RotateVector(FVector(0, 0, 8)).Z > 4. ? 8.f : 4.f)), FVector(4.5f, 3.5f, 8.f), Flat());
        }
        else if (Kind == 7) M.Box(TEXT("CF_CupWhite"), FTransform(FRotator(R.FRandRange(0.f, 60.f), Yaw, R.FRandRange(0.f, 60.f)), At + FVector(0, 0, 3)), FVector(4.f, 3.5f, 3.f), Flat());
        else if (Kind == 8)
        {
            const FVector D = FRotator(0.f, Yaw, 0.f).Vector(), S = RightOf(D);
            HouseCampusFallKit::Bar(M, TEXT("CF_Rake"), At - D * 22.f + FVector(0, 0, .8f), At + D * 30.f + FVector(0, 0, .8f), 1.2f, 1.2f, FVector::UpVector);
            HouseCampusFallKit::Tri(M, Key(TEXT("CF_Pennant"), R.RandRange(0, 1)), At - D * 20.f + FVector(0, 0, 1.3f), At - D * 20.f + S * 13.f + FVector(0, 0, 1.3f), At + D * 22.f + S * 2.f + FVector(0, 0, 1.3f), FVector::UpVector);
        }
        else if (bOnBench&&!HouseStadiumDetail::Enabled()) M.Box(TEXT("CF_Purple"), FTransform(FRotator(0.f, A * 180.f / PI + 90.f + R.FRandRange(-12.f, 12.f), 0.f), At + FVector(0, 0, 2.f)), FVector(17.f, 15.f, 2.f), Flat());
        else Cup(M, TEXT("CF_CupWhite"), At, Yaw, true);
    }
    // Bedsheets tied along the podium wall at the front of the stands.
    const TPair<int32, int32> Sheets[] = { { 6, 0 }, { 8, 3 }, { 11, 1 }, { 13, 5 }, { 17, 2 }, { 22, 4 }, { 27, 0 }, { 33, 3 }, { 37, 1 }, { 2, 2 } };
    for (const TPair<int32, int32>& S : Sheets)
    {
        if (HouseCampusInteriors::StadiumTunnel(S.Key)) continue;   // the players' tunnel comes out there
        const float A = S.Key * 2.f * PI / BowlSeg, B = (S.Key + 1) * 2.f * PI / BowlSeg;
        const FVector A0 = Bowl(A, .69f, 225.f), B0 = Bowl(B, .69f, 225.f);
        const FVector Mid = (A0 + B0) * .5f, In = -BowlOut((A + B) * .5f);
        WallSheet(P, S.Value, Mid + In * 7.f, (B0 - A0).GetSafeNormal2D(), 300.f, 150.f);
    }
    // Pennants round the rim, post to post.
    for (int32 K = 0; K < BowlSeg; K += 2)
    {
        if (HouseCampusInteriors::StadiumOpen() && K >= 6 && K <= 12) continue;   // they would hang through the walk-in press box
        const float A = K * 2.f * PI / BowlSeg, B = (K + 2) * 2.f * PI / BowlSeg;
        PennantString(P, M, Bowl(A, 1.012f, 1215.f), Bowl(B, 1.012f, 1215.f), 32.f, 72.f, 52.f, K);
    }
    if(!(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=2))
    {
    // The press box: lit windows toward the field, the stadium's name above them and bunting below.
    const FVector N(0, -1, 0), Right(-1, 0, 0);
    constexpr float FaceY = FieldY + 3300.f;
    M.Quad(TEXT("EX_Window"), FVector(300, FaceY - 1.5f, 1180), FVector(4500, FaceY - 1.5f, 1180), FVector(4500, FaceY - 1.5f, 1430), FVector(300, FaceY - 1.5f, 1430), N, Flat());
    for (float X = 300.f; X <= 4501.f; X += 300.f) HouseCampusFallKit::Bar(M, TEXT("EX_Trim"), FVector(X, FaceY - 4.f, 1176), FVector(X, FaceY - 4.f, 1434), 9.f, 5.f, N);
    for (const float Z : { 1176.f, 1434.f }) HouseCampusFallKit::Bar(M, TEXT("EX_Trim"), FVector(296, FaceY - 4.f, Z), FVector(4504, FaceY - 4.f, Z), 9.f, 6.f, N);
    HouseCampusFallKit::Card(M, TEXT("CF_PressSign"), FVector(FieldX, FaceY - 3.f, 1612.f), Right, FVector::UpVector, 2200.f, 275.f, N);
    for (int32 K = 0; K < 11; ++K)
    {
        const FVector Top(450.f + K * 390.f, FaceY - 6.f, 1168.f);
        const FName Bands[] = { TEXT("CF_Pennant0"), TEXT("CF_Pennant2"), TEXT("CF_Pennant1") };
        for (int32 Band = 0; Band < 3; ++Band)
        {
            const float R0 = 95.f - Band * 32.f, R1 = R0 - 32.f;
            for (int32 J = 0; J < 10; ++J)
            {
                const float A0 = PI + PI * J / 10.f, A1 = PI + PI * (J + 1) / 10.f;
                auto At = [&](float Ang, float Rr) { return Top + Right * (FMath::Cos(Ang) * Rr) + FVector(0, 0, FMath::Sin(Ang) * Rr) - N * (Band * .3f); };
                if (R1 > 1.f) M.Quad(Bands[Band], At(A0, R0), At(A1, R0), At(A1, R1), At(A0, R1), N, Flat());
                else HouseCampusFallKit::Tri(M, Bands[Band], At(A0, R0), At(A1, R0), Top - N * (Band * .3f), N);
            }
        }
    }
    // Its street side: the stadium's name across the glass where the orange T was.
    HouseCampusFallKit::Card(M, TEXT("CF_PressSign"), FVector(FieldX, FieldY + 4074.f, 1460.f), FVector(1, 0, 0), FVector::UpVector, 2400.f, 300.f, FVector(0, 1, 0));
    }
    // Haze round the three flood heads (mesh glow, not more lights).
    if (HouseCampusPolish::Enabled())
    {
        // With the exterior polish's lamp banks (HouseCampusPolish::FloodBanks): a tight glow on every lens, a softer one
        // round each bank and the wide haze below it.
        TArray<HouseCampusPolish::FFloodBank> Banks;
        HouseCampusPolish::FloodBanks(Banks);
        int32 DetailBank=0;
        for (const HouseCampusPolish::FFloodBank& B : Banks)
        {
            int32 DetailLens=0;
            for (const FVector& L : B.Lenses){if(!(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=3&&DetailBank==0&&DetailLens==0))P.Halos.Add({ L + B.Facing * 6.f, 260.f, FLinearColor(.85f, .9f, 1.f, .5f), 0.f });++DetailLens;}
            ++DetailBank;
            P.Halos.Add({ B.Centre + B.Facing * 20.f, 900.f, FLinearColor(.8f, .86f, 1.f, .3f), 0.f });
            P.Halos.Add({ B.Centre + B.Facing * 400.f - FVector(0, 0, 120.f), 3400.f, FLinearColor(.55f, .6f, .72f, .14f), 0.f });
        }
    }
    else
        for (const float Deg : { 35.f, 90.f, 145.f })
        {
            const FVector Top = Bowl(FMath::DegreesToRadians(Deg), 1.04f, 3900.f);
            P.Halos.Add({ Top + FVector(0, -30, 50), 1300.f, FLinearColor(.8f, .86f, 1.f, .6f), 0.f });
            P.Halos.Add({ Top + FVector(0, -150, -40), 3400.f, FLinearColor(.55f, .6f, .72f, .16f), 0.f });
        }
}

// ------------------------------------------------------------------ the gate
void TicketBooth(HouseCampusFallParts::FParts& P, const FVector& Base, const FVector& Facing)
{
    FHallMesh& M = P.Gate;
    const FVector F = Facing.GetSafeNormal2D(), S = RightOf(F);
    const FQuat Q = FRotationMatrix::MakeFromXZ(F, FVector::UpVector).ToQuat();
    M.Box(TEXT("CF_Booth"), FTransform(Q, Base + FVector(0, 0, 115)), FVector(75, 85, 115), Flat());
    P.Solid.Box(TEXT("CF_Booth"), FTransform(Q, Base + FVector(0, 0, 115)), FVector(75, 85, 115), Flat());
    M.Box(TEXT("CF_Purple"), FTransform(Q, Base + F * 8.f + FVector(0, 0, 236)), FVector(98, 106, 6), Flat());
    M.Box(TEXT("CF_Gold"), FTransform(Q, Base + F * 8.f + FVector(0, 0, 228)), FVector(99, 107, 2.5f), Flat());
    // The window, lit from inside, with its counter and the ticket sign over it.
    const FVector Face = Base + F * 75.5f;
    M.Quad(TEXT("EX_Window"), Face - S * 55.f + FVector(0, 0, 105), Face + S * 55.f + FVector(0, 0, 105), Face + S * 55.f + FVector(0, 0, 185), Face - S * 55.f + FVector(0, 0, 185), F, Flat());
    for (const float Z : { 103.f, 187.f }) HouseCampusFallKit::Bar(M, TEXT("CF_Gold"), Face - S * 58.f + FVector(0, 0, Z), Face + S * 58.f + FVector(0, 0, Z), 4.f, 5.f, F);
    HouseCampusFallKit::Bar(M, TEXT("CF_Booth"), Face + F * 10.f - S * 60.f + FVector(0, 0, 100), Face + F * 10.f + S * 60.f + FVector(0, 0, 100), 24.f, 4.f, FVector::UpVector);
    HouseCampusFallKit::Card(M, TEXT("CF_TicketSign"), Face + F * 1.f + FVector(0, 0, 207), -S, FVector::UpVector, 140.f, 35.f, F);
}

void HayStack(HouseCampusFallParts::FParts& P, const FVector& Base, const FVector& Along, int32 Seed)
{
    FHallMesh& M = P.Gate;
    const float Yaw = Along.Rotation().Yaw;
    HouseCampusFallKit::HayBale(M, TEXT("FJ_Straw"), TEXT("FJ_Twine"), Base - Along * 47.f, Yaw + 2.f);
    HouseCampusFallKit::HayBale(M, TEXT("FJ_Straw"), TEXT("FJ_Twine"), Base + Along * 47.f, Yaw - 3.f);
    HouseCampusFallKit::HayBale(M, TEXT("FJ_Straw"), TEXT("FJ_Twine"), Base + FVector(0, 0, 38), Yaw + 6.f);
    P.Solid.BoxMinMax(TEXT("FJ_Straw"), Base - FVector(95, 95, 0), Base + FVector(95, 95, 38), Flat());
    const FVector Out = FVector::CrossProduct(FVector::UpVector, Along);
    Pumpkin(P, M, Base + FVector(0, 0, 76), 17.f, Out.Rotation().Yaw + 8.f, Seed, Seed);
    Pumpkin(P, M, Base + Out * 55.f - Along * 60.f, 21.f, Out.Rotation().Yaw - 12.f, Seed + 1, Seed + 1);
    Pumpkin(P, M, Base + Out * 50.f + Along * 40.f, 14.f, 30.f * Seed, -1, 0);
    Pumpkin(P, M, Base + FVector(0, 0, 38) + Along * 70.f + Out * 10.f, 11.f, 70.f * Seed, -1, 0);
}

void CornShock(FHallMesh& M, const FVector& Base, float Lean)
{
    const TArray<FVector2D> Profile = { { 0.f, 0.f }, { 34.f, 0.f }, { 27.f, 45.f }, { 16.f, 110.f }, { 13.f, 135.f }, { 22.f, 170.f }, { 30.f, 205.f }, { 4.f, 218.f }, { 0.f, 220.f } };
    HouseCampusFallKit::Lathe(M, TEXT("FJ_Straw"), Base, Profile, 9, 1.f);
    M.Cylinder(TEXT("FJ_Twine"), Base + FVector(0, 0, 128), Base + FVector(0, 0, 136), 15.f, 9, Flat());
    // Loose stalks fanning out of the top.
    for (int32 K = 0; K < 7; ++K)
    {
        const float A = K * 2.f * PI / 7.f + Lean;
        const FVector From = Base + FVector(FMath::Cos(A) * 12.f, FMath::Sin(A) * 12.f, 190.f);
        HouseCampusFallKit::Bar(M, TEXT("FJ_Straw"), From, From + FVector(FMath::Cos(A) * 30.f, FMath::Sin(A) * 30.f, 70.f), 2.f, 2.f, FVector::UpVector);
    }
}

void OpenGate(FHallMesh& M, const FArch& Arch)
{
    for (const float Side : { -1.f, 1.f })
    {
        const FVector Hinge = Arch.Centre + Arch.Out * 20.f + Arch.Along * (Side * 110.f);
        const float Open = FMath::DegreesToRadians(72.f);
        const FVector Leaf = (-Arch.Along * Side * FMath::Cos(Open) + Arch.Out * FMath::Sin(Open)).GetSafeNormal();
        const FVector Base = Hinge + FVector(0, 0, 4.f);
        for (const float Z : { 10.f, 140.f, 268.f }) HouseCampusFallKit::Bar(M, TEXT("CF_Purple"), Base + FVector(0, 0, Z), Base + Leaf * 108.f + FVector(0, 0, Z), 4.f, 5.f, FVector::UpVector);
        for (int32 K = 0; K <= 8; ++K)
        {
            const FVector Foot = Base + Leaf * (K * 13.5f);
            HouseCampusFallKit::Bar(M, TEXT("CF_Purple"), Foot, Foot + FVector(0, 0, K == 0 || K == 8 ? 275.f : 288.f), K == 0 || K == 8 ? 5.f : 2.6f, K == 0 || K == 8 ? 5.f : 2.6f, Leaf);
            if (K > 0 && K < 8) HouseCampusFallKit::Tri(M, TEXT("CF_Gold"), Foot + FVector(0, 0, 286) - Leaf * 3.f, Foot + FVector(0, 0, 286) + Leaf * 3.f, Foot + FVector(0, 0, 297), FVector::CrossProduct(Leaf, FVector::UpVector));
        }
    }
}

void Gate(HouseCampusFallParts::FParts& P)
{
    FHallMesh& M = P.Gate;
    int32 Seed = 0;
    for (const int32 K : { 8, 10 })
    {
        const FArch Arch = ArchAt(K);
        OpenGate(M, Arch);
        // GATE 4 / GATE 5 on a board hung in the arch.
        const FVector Board = Arch.Centre + Arch.Out * 34.f + FVector(0, 0, 330.f);
        const FVector Right = RightOf(-Arch.Out);
        M.Box(TEXT("CF_Purple"), FTransform(FRotationMatrix::MakeFromXZ(Right, FVector::UpVector).ToQuat(), Board), FVector(92.f, 3.f, 24.f), Flat());
        for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Bar(M, TEXT("EX_Metal"), Board + Right * (S * 75.f) + FVector(0, 0, 24), Board + Right * (S * 75.f) + FVector(0, 0, 40) - Arch.Out * 14.f, 1.5f, 1.5f, Arch.Out);
        const FString Label = K == 8 ? TEXT("GATE 4") : TEXT("GATE 5");
        const float Cap = 26.f, Width = Label.Len() * 5.f * Cap / 6.f - Cap / 6.f;
        HouseCampusFallKit::GlyphStrips(M, TEXT("CF_Gold"), Label, Board + Arch.Out * 3.3f - Right * (Width * .5f) - FVector(0, 0, Cap * .5f), Right, FVector::UpVector, Arch.Out, Cap, 3.4f);
        // Hay, corn shocks and pumpkins either side.
        for (const float S : { -1.f, 1.f })
        {
            HayStack(P, Arch.Centre + Arch.Out * 150.f + Arch.Along * (S * 245.f), Arch.Along, Seed);
            Seed += 2;
            CornShock(M, Arch.Centre + Arch.Out * 52.f + Arch.Along * (S * 168.f), S + K);
        }
    }
    // Ticket booths out in front, a pennant string from booth to mast to booth.
    TicketBooth(P, FVector(3330.f, -9030.f, Ground), FVector(.12f, 1.f, 0.f));
    TicketBooth(P, FVector(1560.f, -8975.f, Ground), FVector(-.1f, 1.f, 0.f));
    const FVector Mast(FieldX, FieldY + 3952.f, Ground + 420.f);
    PennantString(P, M, FVector(3330.f, -9030.f, Ground + 236.f) + FVector(-75.f, 0, 0), Mast, 30.f, 60.f, 44.f, 1, true);
    PennantString(P, M, Mast, FVector(1560.f, -8975.f, Ground + 236.f) + FVector(75.f, 0, 0), 30.f, 60.f, 44.f, 2, true);
    // And lights strung along the wall between the gates, arch to arch.
    for (const int32 K : { 8, 9, 10 })
    {
        const float A = K * 2.f * PI / BowlSeg, B = (K + 1) * 2.f * PI / BowlSeg;
        PennantString(P, M, Bowl(A, 1.f, 700.f) + BowlOut(A) * 14.f, Bowl(B, 1.f, 700.f) + BowlOut(B) * 14.f, 45.f, 75.f, 40.f, K, true);
    }
    for (const float A : { 0.f, 2.1f, 4.2f }) CornShock(M, Mast + FVector(FMath::Cos(A) * 52.f, FMath::Sin(A) * 52.f, -420.f), A);
    Pumpkin(P, M, Mast + FVector(-30.f, 70.f, -420.f), 19.f, 95.f, 2, 1);
    Pumpkin(P, M, Mast + FVector(45.f, 60.f, -420.f), 15.f, 80.f, 0, 2);
    // The sheets on the wall between the gates.
    for (const TPair<int32, int32>& S : { TPair<int32, int32>(9, 0), TPair<int32, int32>(11, 1), TPair<int32, int32>(7, 4) })
    {
        const float A = S.Key * 2.f * PI / BowlSeg, B = (S.Key + 1) * 2.f * PI / BowlSeg;
        const float SheetZ = HouseCampusInteriors::StadiumOpen() ? 935.f : 1015.f;   // under the walk-in press box's floor
        const FVector A0 = Bowl(A, 1.f, SheetZ), B0 = Bowl(B, 1.f, SheetZ);
        const FVector Out = FVector(FMath::Cos((A + B) * .5f), FMath::Sin((A + B) * .5f), 0.);
        WallSheet(P, S.Value, (A0 + B0) * .5f + Out * 10.f, RightOf(-Out), 520.f, 260.f);
    }
}

// ------------------------------------------------------------------ on the green: the float and the band trailer
/** Where the float, the trailer and the bonfire were put (HouseCampusFall::FloatSpot and the others). */
FVector* Spots() { static FVector S[3] = { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector }; return S; }
/** The ground under the lowest of a vehicle's feet (its wheels, skirt corners or jack: X ahead, Y to the right in its own
 *  frame, turned by Yaw about At), so on a slope it rests on its lowest wheel and never floats. */
float LowestFoot(const FVector2D& At, float Yaw, std::initializer_list<FVector2D> Feet)
{
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    const FVector F = Q.GetForwardVector(), S = Q.GetRightVector();
    float Low = FLT_MAX;
    for (const FVector2D& Foot : Feet)
    {
        const FVector W = FVector(At, 0.) + F * Foot.X + S * Foot.Y;
        Low = FMath::Min(Low, HouseExteriorSite::GroundHeight(W.X, W.Y));
    }
    return Low;
}
void Float(HouseCampusFallParts::FParts& P, const FVector& At, float Yaw)
{
    FHallMesh& M = P.Green;
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    const FVector F = Q.GetForwardVector(), S = Q.GetRightVector(), Z = FVector::UpVector;
    auto L = [&](float X, float Y, float H) { return At + F * X + S * Y + Z * H; };
    // The flatbed, its skirt and the gold fringe.
    M.Box(TEXT("CF_FloatDeck"), FTransform(Q, L(0, 0, 84)), FVector(400, 130, 6), Flat());
    P.Solid.Box(TEXT("CF_FloatDeck"), FTransform(Q, L(0, 0, 45)), FVector(400, 130, 45), Flat());
    for (const float Side : { -1.f, 1.f })
    {
        HouseCampusFallKit::Card(M, TEXT("CF_FloatSkirt"), L(0, Side * 131.f, 44.f), RightOf(-S * Side), Z, 800.f, 88.f, S * Side);
        HouseCampusFallKit::Bar(M, TEXT("CF_TissueGold"), L(-402, Side * 133.f, 86.f), L(402, Side * 133.f, 86.f), 5.f, 9.f, Z);
        M.Quad(TEXT("CF_TissuePurple"), L(Side * 401.f, -131, 0), L(Side * 401.f, 131, 0), L(Side * 401.f, 131, 90), L(Side * 401.f, -131, 90), F * Side, Flat());
    }
    HouseCampusFallKit::Bar(M, TEXT("CF_Tyre"), L(400, 0, 38), L(585, 0, 38), 8.f, 8.f, Z);
    M.Cylinder(TEXT("CF_Tyre"), L(560, 0, 0), L(560, 0, 38), 4.f, 8, Flat());
    // The Fighting Goose in chicken wire and tissue: brown body, pale tail, black neck and head with the white chinstrap,
    // purple and gold wings up. Modelled about the deck and scaled up to fill it.
    constexpr float G = 1.25f;
    auto Goose = [&](float X, float Y, float H) { return L(X * G, Y * G, 90.f + (H - 90.f) * G); };
    // With the exterior polish on (step 3c): stuffed tissue pomps all over, a continuous neck, a defined head and bill,
    // and a torn patch on the flank where the chicken wire shows.
    const bool bPomps = HouseCampusPolish::Enabled();
    const FName Brown = bPomps ? TEXT("CF_PompBrown") : TEXT("CF_TissueBrown"), White = bPomps ? TEXT("CF_PompWhite") : TEXT("CF_TissueWhite");
    const FName Black = bPomps ? TEXT("CF_PompBlack") : TEXT("CF_TissueBlack"), Purple = bPomps ? TEXT("CF_PompPurple") : TEXT("CF_TissuePurple"), GoldPomp = bPomps ? TEXT("CF_PompGold") : TEXT("CF_TissueGold");
    Blob(M, Brown, Goose(-40, 0, 225), F, S, Z, FVector(225, 112, 102) * G, 22, 12, bPomps ? .03f : .06f, 1);
    Blob(M, White, Goose(-235, 0, 215), F, S, Z, FVector(62, 72, 58) * G, 12, 8, .07f, 2);
    Blob(M, Black, Goose(-290, 0, 248), F, S, Z, FVector(40, 58, 18) * G, 12, 6, .05f, 3);
    if (bPomps)
    {
        // The neck: one smooth S from the breast up and forward to the head, thinning as it goes.
        TArray<FVector> Spine;
        TArray<float> Radii;
        const FVector P0 = Goose(130, 0, 262), P1 = Goose(215, 0, 300), P2 = Goose(205, 0, 420), P3 = Goose(262, 0, 492);
        for (int32 K = 0; K <= 12; ++K)
        {
            const float T = K / 12.f, U = 1.f - T;
            Spine.Add(P0 * (U * U * U) + P1 * (3.f * U * U * T) + P2 * (3.f * U * T * T) + P3 * (T * T * T));
            Radii.Add(FMath::Lerp(40.f, 25.f, T) * G);
        }
        Tube(M, Black, Spine, Radii, 14);
        Blob(M, Black, P0, F, S, Z, FVector(54, 50, 48) * G, 14, 8, .03f, 4);   // where it meets the breast
    }
    else
    {
        const FVector Neck[] = { Goose(140, 0, 268), Goose(185, 0, 320), Goose(212, 0, 375), Goose(228, 0, 425), Goose(245, 0, 470) };
        for (int32 K = 0; K < 5; ++K) Blob(M, Black, Neck[K], F, S, Z, FVector(44.f - K * 3.f, 40.f - K * 2.f, 46.f) * G, 12, 8, .06f, 4 + K);
    }
    const FVector Head = Goose(282, 0, 500);
    Blob(M, Black, Head, F, S, Z, FVector(60, 34, 36) * G, 14, 8, .05f, 9);
    for (const float Side : { -1.f, 1.f }) Blob(M, White, Head + (F * -12.f + S * (Side * 19.f) - Z * 13.f) * G, F, S, Z, FVector(33, 18, 24) * G, 10, 6, .05f, 10);
    if (bPomps)
    {
        // The bill: a tapered wedge, its tip turned down a little; the white chinstrap wrapping under the jaw.
        const FVector B0 = Head + (F * 48.f - Z * 6.f) * G, B1 = Head + (F * 102.f - Z * 14.f) * G;
        for (int32 K = 0; K < 6; ++K)
        {
            const float T0 = K / 6.f, T1 = (K + 1) / 6.f;
            const float W0 = FMath::Lerp(17.f, 5.f, T0) * G, W1 = FMath::Lerp(17.f, 5.f, T1) * G, H0 = FMath::Lerp(13.f, 4.f, T0) * G, H1 = FMath::Lerp(13.f, 4.f, T1) * G;
            const FVector C0 = FMath::Lerp(B0, B1, T0), C1 = FMath::Lerp(B0, B1, T1);
            for (int32 E = 0; E < 4; ++E)
            {
                const FVector D0 = E == 0 ? S * W0 + Z * H0 : E == 1 ? -S * W0 + Z * H0 : E == 2 ? -S * W0 - Z * H0 : S * W0 - Z * H0;
                const FVector D1 = E == 0 ? -S * W0 + Z * H0 : E == 1 ? -S * W0 - Z * H0 : E == 2 ? S * W0 - Z * H0 : S * W0 + Z * H0;
                const FVector E0 = D0 * (W1 / W0), E1 = D1 * (W1 / W0);
                M.Quad(Black, C0 + D0, C0 + D1, C1 + E1, C1 + E0, (D0 + D1).GetSafeNormal(), Flat());
            }
        }
        Blob(M, White, Head + (F * 8.f - Z * 30.f) * G, F, S, Z, FVector(30, 30, 12) * G, 12, 6, .04f, 15);
    }
    else Blob(M, Black, Head + (F * 62.f - Z * 4.f) * G, F, S, Z, FVector(36, 13, 10) * G, 10, 6, 0.f, 11);
    for (const float Side : { -1.f, 1.f }) Blob(M, TEXT("CF_Gold"), Head + (F * 22.f + S * (Side * 31.f) + Z * 9.f) * G, F, S, Z, FVector(6, 3, 5) * G, 6, 4, 0.f, 12);
    for (const float Side : { -1.f, 1.f })
    {
        const FVector Root = Goose(-10, Side * 100.f, 300.f);
        const FVector Span = (-F * .35f + S * (Side * .42f) + Z * .84f).GetSafeNormal();
        const FVector Thin = FVector::CrossProduct(Span, F).GetSafeNormal(), Chord = FVector::CrossProduct(Thin, Span).GetSafeNormal();
        Blob(M, Purple, Root + Span * (150.f * G), Span, Chord, Thin, FVector(165, 70, 13) * G, 18, 8, .05f, 13);
        Blob(M, GoldPomp, Root + Span * (255.f * G) - Chord * (30.f * G), Span, Chord, Thin, FVector(78, 52, 11) * G, 12, 6, .05f, 14);
    }
    if (bPomps)
    {
        // The torn patch on the near flank: a ragged hole, dark inside, the chicken wire across it, scraps of tissue hanging.
        const FVector C = Goose(-60, 0, 205) + S * (114.f * G) * 1.02f;
        TArray<FVector> Hole;
        FRandomStream R(61);
        for (int32 K = 0; K < 14; ++K)
        {
            const float A = 2.f * PI * K / 14.f, Rr = R.FRandRange(32.f, 52.f);
            Hole.Add(C + F * (FMath::Cos(A) * Rr * 1.3f) + Z * (FMath::Sin(A) * Rr) + S * 1.f);
        }
        HouseCampusFallKit::Fan(M, TEXT("CF_ChickenWire"), C + S * 1.f, Hole, S);
        for (int32 K = 0; K < 14; K += 2)
        {
            const FVector Top = Hole[K] + S * .5f, Dn = (Hole[K] - C).GetSafeNormal() * 4.f - Z * R.FRandRange(10.f, 22.f);
            M.Quad(Brown, Top - F * 5.f, Top + F * 5.f, Top + F * 4.f + Dn, Top - F * 4.f + Dn, S, Flat());
        }
    }
    // String lights round the deck edge.
    const FVector Corners[] = { L(-404, -134, 93), L(404, -134, 93), L(404, 134, 93), L(-404, 134, 93) };
    for (int32 K = 0; K < 4; ++K)
    {
        const FVector A = Corners[K], B = Corners[(K + 1) % 4];
        const int32 Count = FMath::RoundToInt(FVector::Dist(A, B) / 32.f);
        for (int32 J = 0; J < Count; ++J) Bead(M, TEXT("CF_StringBulb"), FMath::Lerp(A, B, (J + .5f) / Count) + FVector(0, 0, 3.f * FMath::Sin(J * 1.3f)), 2.8f);
    }
    P.Halos.Add({ L(0, 0, 160), 900.f, FLinearColor(1.f, .7f, .35f, .07f), 0.f });
    // BEAT DUNMORE! on a frame at the back.
    const FVector Sign = L(-345, 0, 245);
    for (const float Side : { -1.f, 1.f }) HouseCampusFallKit::Bar(M, TEXT("CF_FloatDeck"), L(-345, Side * 140.f, 90), L(-345, Side * 140.f, 330), 5.f, 5.f, F);
    HouseCampusFallKit::Card(M, TEXT("CF_Sheet0"), Sign - F * 3.f, RightOf(F), Z, 280.f, 140.f, -F);
    HouseCampusFallKit::Card(M, TEXT("CF_SheetBack"), Sign - F * 2.f, RightOf(-F), Z, 280.f, 140.f, F);
}

void BandTrailer(HouseCampusFallParts::FParts& P, const FVector& At, float Yaw)
{
    FHallMesh& M = P.Green;
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    const FVector F = Q.GetForwardVector(), S = Q.GetRightVector(), Z = FVector::UpVector;
    auto L = [&](float X, float Y, float H) { return At + F * X + S * Y + Z * H; };
    M.Box(TEXT("CF_TrailerBody"), FTransform(Q, L(0, 0, 178)), FVector(325, 120, 115), Flat());
    P.Solid.Box(TEXT("CF_TrailerBody"), FTransform(Q, L(0, 0, 178)), FVector(325, 120, 115), Flat());
    M.Box(TEXT("CF_Gold"), FTransform(Q, L(0, 0, 66)), FVector(327, 122, 4), Flat());
    M.Box(TEXT("CF_Gold"), FTransform(Q, L(0, 0, 291)), FVector(327, 122, 3), Flat());
    for (const float Side : { -1.f, 1.f })
    {
        HouseCampusFallKit::Card(M, TEXT("CF_TrailerSide"), L(10, Side * 121.f, 180.f), RightOf(-S * Side), Z, 560.f, 140.f, S * Side);
        for (const float X : { -40.f, 40.f })
        {
            M.Cylinder(TEXT("CF_Tyre"), L(X, Side * 112.f, 36), L(X, Side * 132.f, 36), 35.f, 16, Flat());
            M.Cylinder(TEXT("CF_Chrome"), L(X, Side * 132.f, 36), L(X, Side * 134.f, 36), 15.f, 12, Flat());
        }
        M.Box(TEXT("CF_TrailerBody"), FTransform(Q, L(0, Side * 124.f, 76)), FVector(85, 12, 3), Flat());
    }
    // The back door down as a ramp, the dark inside, a bass drum and two cases on the grass.
    M.Quad(TEXT("EX_Dark"), L(-326, -105, 70), L(-326, 105, 70), L(-326, 105, 280), L(-326, -105, 280), -F, Flat());
    const FVector Hinge = L(-326, 0, 66), Lip = L(-520, 0, 2);
    M.Box(TEXT("CF_Aluminium"), FTransform(FRotationMatrix::MakeFromXY((Lip - Hinge).GetSafeNormal(), S).ToQuat(), (Hinge + Lip) * .5f), FVector(FVector::Dist(Hinge, Lip) * .5f, 104.f, 2.f), Flat());
    M.Cylinder(TEXT("CF_Purple"), L(-560, -60, 46), L(-560, -18, 46), 44.f, 18, Flat());
    M.Cylinder(TEXT("CF_CupWhite"), L(-560, -61, 46), L(-560, -59, 46), 43.f, 18, Flat());
    M.Cylinder(TEXT("CF_CupWhite"), L(-560, -19, 46), L(-560, -17, 46), 43.f, 18, Flat());
    M.Box(TEXT("EX_Dark"), FTransform(FRotator(0.f, Yaw + 20.f, 0.f), L(-600, 70, 26)), FVector(40, 30, 26), Flat());
    M.Box(TEXT("EX_Dark"), FTransform(FRotator(0.f, Yaw - 8.f, 0.f), L(-585, 75, 70)), FVector(34, 26, 18), Flat());
    HouseCampusFallKit::Bar(M, TEXT("CF_Tyre"), L(325, 0, 70), L(470, 0, 55), 8.f, 8.f, Z);
    M.Cylinder(TEXT("CF_Tyre"), L(450, 0, 0), L(450, 0, 56), 4.f, 8, Flat());
}
}

void HouseCampusFallParts::Stadium(FParts& P)
{
    HouseCampusFallStadiumImpl::Field(P);
    HouseStadiumDetail::FieldCollision(P.Field,P.Solid);
    if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=2)for(float FlagX:{700.f,3700.f}){HouseCampusFallParts::FCloth Flag;Flag.Front=Flag.Back=TEXT("SD_Fabric");Flag.Origin={FlagX+5,-9730,2925};Flag.Right={1,0,0};Flag.Down={0,0,-1};Flag.Width=130;Flag.Height=85;Flag.Cols=6;Flag.Rows=4;Flag.Amp=.9f;Flag.Pinned=1.f;P.Cloths.Add(Flag);}
    HouseCampusFallStadiumImpl::Scoreboard(P);
    HouseCampusFallStadiumImpl::Stands(P);
    HouseCampusFallStadiumImpl::Gate(P);
    // On the green, where no tree stands: the float side-on to the quad, the band trailer by the west gate.
    const FVector2D FloatAt = P.FirstClear({ FVector2D(5300.f, -8150.f), FVector2D(5650.f, -7700.f), FVector2D(4700.f, -8250.f), FVector2D(3900.f, -7900.f) }, 520.f);
    HouseCampusFallStadiumImpl::Spots()[0] = FVector(FloatAt, 520.f);
    // Each stands on the ground under its lowest foot: the float's skirt corners and tow-bar jack, the trailer's wheels and jack.
    const float FloatZ = HouseCampusFallStadiumImpl::LowestFoot(FloatAt, -38.f, { FVector2D(-400.f, -131.f), FVector2D(400.f, -131.f), FVector2D(400.f, 131.f), FVector2D(-400.f, 131.f), FVector2D(560.f, 0.f) });
    HouseCampusFallStadiumImpl::Float(P, FVector(FloatAt, FloatZ), -38.f);
    const FVector2D TrailerAt = P.FirstClear({ FVector2D(1000.f, -8150.f), FVector2D(1500.f, -7800.f), FVector2D(700.f, -7550.f) }, 430.f);
    HouseCampusFallStadiumImpl::Spots()[1] = FVector(TrailerAt, 430.f);
    const float TrailerZ = HouseCampusFallStadiumImpl::LowestFoot(TrailerAt, 15.f, { FVector2D(-40.f, -122.f), FVector2D(40.f, -122.f), FVector2D(-40.f, 122.f), FVector2D(40.f, 122.f), FVector2D(450.f, 0.f) });
    HouseCampusFallStadiumImpl::BandTrailer(P, FVector(TrailerAt, TrailerZ), 15.f);
}

FVector HouseCampusFall::FloatSpot() { return HouseCampusFallStadiumImpl::Spots()[0]; }
FVector HouseCampusFall::TrailerSpot() { return HouseCampusFallStadiumImpl::Spots()[1]; }
FVector HouseCampusFall::BonfireSpot() { return HouseCampusFallStadiumImpl::Spots()[2]; }
