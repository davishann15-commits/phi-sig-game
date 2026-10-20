// The campus clock tower, built in code: an original collegiate-Gothic design in campus brick and limestone. A stepped
// plinth and an ashlar base with a pointed doorway and date stone, a buttressed brick shaft with lancets, quoins and string
// courses, an open belfry with louvred lancets and its bells, a corbelled clock stage with four opal dials, a crenellated
// parapet with pinnacles, and a steep copper spire with lucarnes, a gilt finial and a weathervane. Copper drip stains,
// ivy, pigeons and crows are part of the look. The dial glow and the uplight on the stone are emission only (no lights).
#include "HouseClockTowerMesh.h"
#include "HouseCalderTerrain.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusInteriors.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace HouseClockTowerMeshImpl
{
constexpr float CX = HouseClockTowerSite::CenterX, CY = HouseClockTowerSite::CenterY;
/** The tower's foot in the world: campus grade raised onto Tower Knoll (HouseClockTowerSite::Lift), as in Center(). */
float GZ() { return HouseClockTowerSite::GroundZ + HouseClockTowerSite::Lift(); }
// Stage heights above the foot and half widths (cm).
constexpr float HBaseTop = 650.f, HBand1 = 1440.f, HBand2 = 2240.f, HBelfry = 2780.f, HClock = 3600.f;
constexpr float HCornice = 4400.f, HParapet = 4560.f, HMerlon = 4640.f, HSpire = 4460.f, HApex = 6750.f;
constexpr float BaseHalf = 330.f, ShaftHalf = 300.f, SpireHalf = 262.f;
constexpr float ClockHalf = HouseClockTowerSite::ClockHalf, HDial = HouseClockTowerSite::DialHeight, DialR = HouseClockTowerSite::DialRadius;

const FName KAshlar(TEXT("CT_Ashlar")), KBrick0(TEXT("CT_Brick0")), KBrick1(TEXT("CT_Brick1")), KBrick2(TEXT("CT_Brick2")), KBelfry(TEXT("CT_BelfryBrick"));
const FName KStoneLit(TEXT("CT_StoneLit")), KStone(TEXT("CT_Stone")), KClockStone(TEXT("CT_ClockStone")), KStoneDeep(TEXT("CT_StoneDeep"));
const FName KCopper(TEXT("CT_Copper")), KCopperTrim(TEXT("CT_CopperTrim")), KIron(TEXT("CT_Iron")), KGilt(TEXT("CT_Gilt"));
const FName KDialA(TEXT("CT_DialA")), KDialB(TEXT("CT_DialB")), KLancet(TEXT("CT_LancetGlass")), KLancetLit(TEXT("CT_LancetLit"));
const FName KLouvre(TEXT("CT_Louvre")), KOak(TEXT("CT_Oak")), KBronze(TEXT("CT_Bronze")), KInterior(TEXT("CT_Interior"));
const FName KCrow(TEXT("CT_Crow")), KPigeon(TEXT("CT_Pigeon")), KVine(TEXT("CT_Vine")), KLens(TEXT("CT_FloodLens"));
const FName KIvy[3] = { FName(TEXT("CT_IvyScarlet")), FName(TEXT("CT_IvyCrimson")), FName(TEXT("CT_IvyLate")) };

const FHallUV PlainUV = FHallUV::World(100.f), BrickUV = FHallUV::World(120.f), AshlarUV = FHallUV::World(240.f);
const FHallUV StoneUV = FHallUV::World(160.f), CopperUV = FHallUV::World(200.f);

FHallUV BoxUV(float Height, const FVector& Size)
{
    FHallUV UV; UV.Mode = 2; UV.Frame = FTransform(HouseClockTowerSite::Center(Height)); UV.BoxSize = Size;
    return UV;
}

// ------------------------------------------------------------------ frames and primitives
FVector FaceN(int32 F)
{
    static const FVector Normals[4] = { FVector(0, 1, 0), FVector(1, 0, 0), FVector(0, -1, 0), FVector(-1, 0, 0) };
    return Normals[((F % 4) + 4) % 4];
}
FVector FaceU(int32 F) { return FVector::CrossProduct(FaceN(F), FVector::UpVector); }
/** A point on face F: Dist out from the tower axis, U along the face (viewer's right), H above grade. */
FVector At(int32 F, float Dist, float U, float H) { return HouseClockTowerSite::Center(H) + FaceN(F) * Dist + FaceU(F) * U; }

void Tri(FHallMesh& M, FName Key, const FVector& A, const FVector& B, const FVector& C, const FVector& Nrm, const FHallUV& UV)
{
    FHallSection& Sec = M.Sec(Key);
    const int32 Base = Sec.V.Num();
    FVector UAx, VAx; FHallMesh::PlanarAxes(Nrm, UAx, VAx);
    for (const FVector& P : { A, B, C })
    {
        Sec.V.Add(P); Sec.N.Add(Nrm); Sec.UV.Add(M.ComputeUV(P, Nrm, UV)); Sec.T.Add(FProcMeshTangent(UAx, false));
    }
    if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Nrm) < 0) Sec.I.Append({ Base, Base + 1, Base + 2 });
    else Sec.I.Append({ Base, Base + 2, Base + 1 });
}

/** Box in a face's frame: Dist0..Dist1 out from the axis, U0..U1 along the face, H0..H1 above grade. Skip bit 0 = outer face. */
void FaceBox(FHallMesh& M, FName Key, int32 F, float Dist0, float Dist1, float U0, float U1, float H0, float H1, const FHallUV& UV, int32 Skip = 0)
{
    const FQuat Q = FRotationMatrix::MakeFromXZ(FaceN(F), FVector::UpVector).ToQuat();
    M.Box(Key, FTransform(Q, At(F, (Dist0 + Dist1) * .5f, (U0 + U1) * .5f, (H0 + H1) * .5f)),
        FVector(FMath::Abs(Dist1 - Dist0) * .5f, FMath::Abs(U1 - U0) * .5f, FMath::Abs(H1 - H0) * .5f), UV, Skip);
}

/** Box relative to the tower axis. */
void AxisBox(FHallMesh& M, FName Key, float X0, float X1, float Y0, float Y1, float H0, float H1, const FHallUV& UV, int32 Skip = 0)
{
    M.BoxMinMax(Key, FVector(CX + X0, CY + Y0, GZ() + H0), FVector(CX + X1, CY + Y1, GZ() + H1), UV, Skip);
}
void Square(FHallMesh& M, FName Key, float Half, float H0, float H1, const FHallUV& UV, int32 Skip = 0)
{
    AxisBox(M, Key, -Half, Half, -Half, Half, H0, H1, UV, Skip);
}
/** A square ring Inner..Outer round the axis (a course hollowed for the walk-in stair); its outer faces, top and bottom
 *  ledge are the solid square's. */
void SquareRing(FHallMesh& M, FName Key, float Outer, float Inner, float H0, float H1, const FHallUV& UV)
{
    AxisBox(M, Key, -Outer, Outer, Inner, Outer, H0, H1, UV);
    AxisBox(M, Key, -Outer, Outer, -Outer, -Inner, H0, H1, UV);
    AxisBox(M, Key, -Outer, -Inner, -Inner, Inner, H0, H1, UV, 4 | 8);
    AxisBox(M, Key, Inner, Outer, -Inner, Inner, H0, H1, UV, 4 | 8);
}
/** A solid square less a slot X0..X1 x Y0..Y1 (Y1 may reach the edge). */
void SquareSlot(FHallMesh& M, FName Key, float Half, float X0, float X1, float Y0, float Y1, float H0, float H1, const FHallUV& UV)
{
    const bool bInside = Y1 < Half - .5f;
    AxisBox(M, Key, -Half, Half, -Half, Y0, H0, H1, UV);
    if (bInside) AxisBox(M, Key, -Half, Half, Y1, Half, H0, H1, UV);
    AxisBox(M, Key, -Half, X0, Y0, Y1, H0, H1, UV, bInside ? 4 | 8 : 8);
    AxisBox(M, Key, X1, Half, Y0, Y1, H0, H1, UV, bInside ? 4 | 8 : 8);
}

/** Square-section bar from A to B; its thickness runs toward Facing. */
void Bar(FHallMesh& M, FName Key, const FVector& A, const FVector& B, float Width, float Thick, const FVector& Facing, const FHallUV& UV = PlainUV)
{
    const FVector Along = B - A;
    const float Len = Along.Size();
    if (Len < .01f) return;
    const FQuat Q = FRotationMatrix::MakeFromXZ(Along / Len, Facing).ToQuat();
    M.Box(Key, FTransform(Q, (A + B) * .5f), FVector(Len * .5f, Width * .5f, Thick * .5f), UV);
}

void Ball(FHallMesh& M, FName Key, const FVector& C, float R, int32 Seg)
{
    M.Dome(Key, C, FVector::UpVector, R, R, Seg);
    M.Dome(Key, C, -FVector::UpVector, R, R, Seg);
}

/** Pyramid on a square base (four triangles). */
void Pyramid(FHallMesh& M, FName Key, const FVector& BaseCenter, float Half, float Height, const FHallUV& UV)
{
    const FVector Apex = BaseCenter + FVector(0, 0, Height);
    for (int32 F = 0; F < 4; ++F)
    {
        const FVector N = FaceN(F), U = FaceU(F);
        const FVector Out = (N * Height + FVector::UpVector * Half).GetSafeNormal();
        Tri(M, Key, BaseCenter + N * Half - U * Half, BaseCenter + N * Half + U * Half, Apex, Out, UV);
    }
}

// Pointed (two-centred) arch: radius R, centres C = R - W either side of the middle.
float ArchRiseRC(float U, float R, float C) { const float D = FMath::Abs(U) + C; return FMath::Sqrt(FMath::Max(0.f, R * R - D * D)); }
float ArchR(float W, float Sharp) { return FMath::Max(.5f, Sharp) * 2.f * W; }
float ArchRise(float U, float W, float Sharp) { const float R = ArchR(W, Sharp); return ArchRiseRC(U, R, R - W); }

struct FOpening
{
    float U = 0.f, W = 40.f, Sill = 0.f, Spring = 100.f, Sharp = 1.f;
    FName Fill = NAME_None;
    float FillDepth = 0.f;
    bool bLouvres = false;
    /** Fill only the pointed head above the springing (the walk-in tower's door leaves fill the rest). */
    bool bFillArchOnly = false;
};

/** A wall slab on face F (outer plane Dist from the axis, U in -Half..Half, H0..H1, Depth thick) with pointed openings. */
void Wall(FHallMesh& M, FName Key, int32 F, float Dist, float Half, float H0, float H1, float Depth, TArray<FOpening> Openings, const FHallUV& UV, FName Reveal)
{
    Openings.Sort([](const FOpening& A, const FOpening& B) { return A.U < B.U; });
    const FVector N = FaceN(F), U = FaceU(F);
    float Cursor = -Half;
    // A face box's local +Y points to lower U (MakeFromXZ(N, Up)): skip bit 4 is its low-U side, 8 its high-U side, 16 its top.
    // Pier sides next to an opening and the sill top are left to the reveal quads, which would otherwise z-fight them.
    bool bAfterOpening = false;
    for (const FOpening& O : Openings)
    {
        if (O.U - O.W > Cursor + .5f) FaceBox(M, Key, F, Dist - Depth, Dist, Cursor, O.U - O.W, H0, H1, UV, 8 | (bAfterOpening ? 4 : 0));
        Cursor = O.U + O.W;
        bAfterOpening = true;
    }
    if (Half > Cursor + .5f) FaceBox(M, Key, F, Dist - Depth, Dist, Cursor, Half, H0, H1, UV, bAfterOpening ? 4 : 0);
    constexpr int32 Steps = 14;
    for (const FOpening& O : Openings)
    {
        if (O.Sill > H0 + .5f) FaceBox(M, Key, F, Dist - Depth, Dist, O.U - O.W, O.U + O.W, H0, O.Sill, UV, 16);
        for (int32 Step = 0; Step < Steps; ++Step)
        {
            const float Ua = -O.W + 2.f * O.W * Step / Steps, Ub = -O.W + 2.f * O.W * (Step + 1) / Steps;
            const float Za = FMath::Min(H1, O.Spring + ArchRise(Ua, O.W, O.Sharp)), Zb = FMath::Min(H1, O.Spring + ArchRise(Ub, O.W, O.Sharp));
            M.Quad(Key, At(F, Dist, O.U + Ua, Za), At(F, Dist, O.U + Ub, Zb), At(F, Dist, O.U + Ub, H1), At(F, Dist, O.U + Ua, H1), N, UV);
            M.Quad(Key, At(F, Dist - Depth, O.U + Ua, Za), At(F, Dist - Depth, O.U + Ub, Zb), At(F, Dist - Depth, O.U + Ub, H1), At(F, Dist - Depth, O.U + Ua, H1), -N, UV);
            FVector2D Down(Zb - Za, -(Ub - Ua)); Down.Normalize();
            M.Quad(Reveal, At(F, Dist, O.U + Ua, Za), At(F, Dist, O.U + Ub, Zb), At(F, Dist - Depth, O.U + Ub, Zb), At(F, Dist - Depth, O.U + Ua, Za),
                (U * Down.X + FVector::UpVector * Down.Y).GetSafeNormal(), PlainUV);
        }
        M.Quad(Reveal, At(F, Dist, O.U - O.W, O.Sill), At(F, Dist - Depth, O.U - O.W, O.Sill), At(F, Dist - Depth, O.U - O.W, O.Spring), At(F, Dist, O.U - O.W, O.Spring), U, PlainUV);
        M.Quad(Reveal, At(F, Dist, O.U + O.W, O.Sill), At(F, Dist - Depth, O.U + O.W, O.Sill), At(F, Dist - Depth, O.U + O.W, O.Spring), At(F, Dist, O.U + O.W, O.Spring), -U, PlainUV);
        M.Quad(Reveal, At(F, Dist, O.U - O.W, O.Sill), At(F, Dist, O.U + O.W, O.Sill), At(F, Dist - Depth, O.U + O.W, O.Sill), At(F, Dist - Depth, O.U - O.W, O.Sill), FVector::UpVector, PlainUV);
        if (!O.Fill.IsNone())
        {
            const float FD = Dist - O.FillDepth;
            if (!O.bFillArchOnly) M.Quad(O.Fill, At(F, FD, O.U - O.W, O.Sill), At(F, FD, O.U + O.W, O.Sill), At(F, FD, O.U + O.W, O.Spring), At(F, FD, O.U - O.W, O.Spring), N, PlainUV);
            for (int32 Step = 0; Step < Steps; ++Step)
            {
                const float Ua = -O.W + 2.f * O.W * Step / Steps, Ub = -O.W + 2.f * O.W * (Step + 1) / Steps;
                Tri(M, O.Fill, At(F, FD, O.U, O.Spring), At(F, FD, O.U + Ua, O.Spring + ArchRise(Ua, O.W, O.Sharp)), At(F, FD, O.U + Ub, O.Spring + ArchRise(Ub, O.W, O.Sharp)), N, PlainUV);
            }
        }
        if (O.bLouvres)
        {
            // Sloping boards shed the rain outward; both sides so they read from below too.
            const float R = ArchR(O.W, O.Sharp), C = R - O.W, Apex = O.Spring + ArchRise(0.f, O.W, O.Sharp);
            for (float Z = O.Sill + 16.f; Z < Apex - 14.f; Z += 23.f)
            {
                float HW = O.W - 1.f;
                if (Z + 6.f > O.Spring) HW = FMath::Sqrt(FMath::Max(0.f, R * R - FMath::Square(Z + 6.f - O.Spring))) - C - 1.f;
                if (HW < 5.f) break;
                const float D1 = Dist - Depth * .25f, D2 = Dist - Depth * .8f;
                const FVector Out = (N * 12.f + FVector::UpVector * (D1 - D2)).GetSafeNormal();
                const FVector A = At(F, D1, O.U - HW, Z - 6.f), B = At(F, D1, O.U + HW, Z - 6.f), C2 = At(F, D2, O.U + HW, Z + 6.f), D = At(F, D2, O.U - HW, Z + 6.f);
                M.Quad(KLouvre, A, B, C2, D, Out, PlainUV);
                M.Quad(KLouvre, A, B, C2, D, -Out, PlainUV);
            }
        }
    }
}

/** Label (hood) moulding over an opening: the arch outline pushed out by Offset, with short drops at the springing. */
void HoodMould(FHallMesh& M, FName Key, int32 F, float Dist, const FOpening& O, float Offset, float Width, float Proud, float Drop)
{
    const float R = ArchR(O.W, O.Sharp) + Offset, C = ArchR(O.W, O.Sharp) - O.W, W2 = R - C;
    constexpr int32 Steps = 12;
    FVector Prev = At(F, Dist + Proud * .5f, O.U - W2, O.Spring);
    for (int32 Step = 1; Step <= Steps; ++Step)
    {
        const float Uc = -W2 + 2.f * W2 * Step / Steps;
        const FVector P = At(F, Dist + Proud * .5f, O.U + Uc, O.Spring + ArchRiseRC(Uc, R, C));
        Bar(M, Key, Prev, P, Width, Proud, FaceN(F));
        Prev = P;
    }
    for (const float S : { -1.f, 1.f })
    {
        Bar(M, Key, At(F, Dist + Proud * .5f, O.U + S * W2, O.Spring + Width * .5f), At(F, Dist + Proud * .5f, O.U + S * W2, O.Spring - Drop), Width, Proud, FaceN(F));
        Bar(M, Key, At(F, Dist + Proud * .5f, O.U + S * (W2 + 4.f), O.Spring - Drop), At(F, Dist + Proud * .5f, O.U + S * (W2 + 4.f), O.Spring - Drop - 9.f), Width + 8.f, Proud, FaceN(F));
    }
}

/** Sloped weathering on a buttress set-off: the top slopes from the outer edge at H up to DIn at H + Rise. */
void Weathering(FHallMesh& M, FName Key, int32 F, float U0, float U1, float DIn, float DOut, float H, float Rise)
{
    const FVector N = FaceN(F), U = FaceU(F);
    const FVector Slope = (N * Rise + FVector::UpVector * (DOut - DIn)).GetSafeNormal();
    M.Quad(Key, At(F, DOut, U0, H), At(F, DOut, U1, H), At(F, DIn, U1, H + Rise), At(F, DIn, U0, H + Rise), Slope, StoneUV);
    Tri(M, Key, At(F, DOut, U0, H), At(F, DIn, U0, H), At(F, DIn, U0, H + Rise), -U, StoneUV);
    Tri(M, Key, At(F, DOut, U1, H), At(F, DIn, U1, H), At(F, DIn, U1, H + Rise), U, StoneUV);
    FaceBox(M, Key, F, DOut - 5.f, DOut + 2.f, U0 - 2.f, U1 + 2.f, H - 7.f, H, StoneUV);
}

/** Flat annulus in a dial plane. */
void Ring(FHallMesh& M, FName Key, const FVector& C, const FVector& N, const FVector& R, float R0, float R1, int32 Seg)
{
    for (int32 K = 0; K < Seg; ++K)
    {
        const float A0 = 2.f * PI * K / Seg, A1 = 2.f * PI * (K + 1) / Seg;
        const FVector D0 = FVector::UpVector * FMath::Cos(A0) + R * FMath::Sin(A0), D1 = FVector::UpVector * FMath::Cos(A1) + R * FMath::Sin(A1);
        M.Quad(Key, C + D0 * R0, C + D1 * R0, C + D1 * R1, C + D0 * R1, N, PlainUV);
    }
}

// ------------------------------------------------------------------ Roman numerals for the dials
struct FStroke { FVector2D A0, A1, B0, B1; };   // bottom edge A0..A1, top edge B0..B1 (glyph units, cap height 1)
/** Clockmaker's capitals: thick down-strokes, hairline up-strokes, bracket serifs. Returns the advance width. */
float Glyph(TCHAR Ch, float X, TArray<FStroke>& Out)
{
    auto Rect = [&](float X0, float X1, float Y0, float Y1) { Out.Add({ { X + X0, Y0 }, { X + X1, Y0 }, { X + X0, Y1 }, { X + X1, Y1 } }); };
    auto Slant = [&](float Bot0, float Bot1, float Top0, float Top1) { Out.Add({ { X + Bot0, 0.f }, { X + Bot1, 0.f }, { X + Top0, 1.f }, { X + Top1, 1.f } }); };
    if (Ch == TEXT('I'))
    {
        Rect(.07f, .21f, 0.f, 1.f); Rect(0.f, .28f, 0.f, .075f); Rect(0.f, .28f, .925f, 1.f);
        return .28f;
    }
    if (Ch == TEXT('V'))
    {
        Slant(.27f, .39f, .04f, .19f); Slant(.33f, .39f, .52f, .58f); Rect(0.f, .25f, .925f, 1.f); Rect(.44f, .64f, .925f, 1.f);
        return .64f;
    }
    Slant(.46f, .60f, .04f, .18f); Slant(.08f, .14f, .48f, .54f);
    Rect(0.f, .24f, .925f, 1.f); Rect(.40f, .62f, .925f, 1.f); Rect(0.f, .24f, 0.f, .075f); Rect(.40f, .64f, 0.f, .075f);
    return .64f;
}

/** A numeral centred at Center, its top toward UpDir, reading toward Right (all in the dial plane, facing N). */
void Numeral(FHallMesh& M, FName Key, const FString& Text, const FVector& Center, const FVector& Right, const FVector& UpDir, const FVector& N, float Cap)
{
    TArray<FStroke> Strokes;
    float X = 0.f;
    for (int32 K = 0; K < Text.Len(); ++K) X += Glyph(Text[K], X, Strokes) + (K + 1 < Text.Len() ? .07f : 0.f);
    const float Width = X;
    auto P = [&](const FVector2D& G) { return Center + Right * ((G.X - Width * .5f) * Cap) + UpDir * ((G.Y - .5f) * Cap); };
    for (const FStroke& S : Strokes) M.Quad(Key, P(S.A0), P(S.A1), P(S.B1), P(S.B0), N, PlainUV);
}

/** HallGlyph letters as square bars (date stone, weathervane). */
void Letters(FHallMesh& M, FName Key, const FString& Text, const FVector& Origin, const FVector& Right, const FVector& UpDir, const FVector& Facing, float Cap, float Width, float Thick)
{
    const float Unit = Cap / 6.f, Advance = 5.2f * Unit;
    FVector Start = Origin - Right * (Advance * Text.Len() - 1.2f * Unit) * .5f;
    for (int32 K = 0; K < Text.Len(); ++K)
    {
        for (const TArray<FVector2D>& Line : HallGlyph(Text[K]))
            for (int32 J = 1; J < Line.Num(); ++J)
            {
                const FVector A = Start + Right * (Line[J - 1].X * Unit) + UpDir * (Line[J - 1].Y * Unit);
                const FVector B = Start + Right * (Line[J].X * Unit) + UpDir * (Line[J].Y * Unit);
                Bar(M, Key, A - (B - A).GetSafeNormal() * Width * .4f, B + (B - A).GetSafeNormal() * Width * .4f, Width, Thick, Facing);
            }
        Start += Right * Advance;
    }
}

// ------------------------------------------------------------------ painted textures
uint32 Hash3(int32 X, int32 Y, int32 Seed)
{
    uint32 H = uint32(X) * 374761393u + uint32(Y) * 668265263u + uint32(Seed) * 2246822519u;
    H = (H ^ (H >> 13)) * 1274126177u;
    return H ^ (H >> 16);
}
float Hash01(int32 X, int32 Y, int32 Seed) { return (Hash3(X, Y, Seed) & 0xffffff) / 16777215.f; }
/** Value noise whose lattice wraps every PX x PY cells, so a texture painted with it tiles seamlessly. */
float TileNoise(float X, float Y, int32 PX, int32 PY, int32 Seed)
{
    const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
    float FX = X - X0, FY = Y - Y0;
    FX = FX * FX * (3.f - 2.f * FX); FY = FY * FY * (3.f - 2.f * FY);
    auto Lattice = [&](int32 A, int32 B) { return Hash01(((A % PX) + PX) % PX, ((B % PY) + PY) % PY, Seed); };
    return FMath::Lerp(FMath::Lerp(Lattice(X0, Y0), Lattice(X0 + 1, Y0), FX), FMath::Lerp(Lattice(X0, Y0 + 1), Lattice(X0 + 1, Y0 + 1), FX), FY);
}
/** Tiling fBm over U,V in 0..1. Aspect < 1 stretches features vertically (rain streaks). */
float TileFbm(float U, float V, int32 Cells, int32 Octaves, int32 Seed, float Aspect = 1.f)
{
    float Sum = 0.f, Amp = .5f, Norm = 0.f;
    int32 Cx = Cells;
    for (int32 O = 0; O < Octaves; ++O)
    {
        const int32 Cy = FMath::Max(1, FMath::RoundToInt(Cx * Aspect));
        Sum += Amp * TileNoise(U * Cx, V * Cy, Cx, Cy, Seed + O * 17);
        Norm += Amp; Amp *= .5f; Cx *= 2;
    }
    return Sum / Norm;
}

/** Campus brick in English bond: stretcher and header courses (7.5 cm), 1 cm joints, some burnt headers, rain grime. */
void PaintBrick(FHallImage& Img, TArray<float>& Height, int32 W, int32 H, float WidthCm, float HeightCm, int32 Seed)
{
    Img.Init(W, H, FLinearColor::Black); Height.Init(0.f, W * H);
    const FLinearColor Body(.30f, .105f, .062f), Burnt(.115f, .052f, .04f), Mortar(.36f, .335f, .30f);
    for (int32 Yp = 0; Yp < H; ++Yp)
        for (int32 Xp = 0; Xp < W; ++Xp)
        {
            const float U = (Xp + .5f) / W, V = (Yp + .5f) / H, Xc = U * WidthCm, Yc = V * HeightCm;
            const int32 Course = FMath::FloorToInt(Yc / 7.5f);
            const float Fy = Yc - Course * 7.5f;
            const bool bHeader = (Course & 1) == 1;
            const float Len = bHeader ? 12.f : 24.f, Xs = Xc + (bHeader ? 6.f : 0.f);
            const int32 PerCourse = FMath::Max(1, FMath::RoundToInt(WidthCm / Len));
            const int32 Index = FMath::FloorToInt(Xs / Len) % PerCourse;
            const float Fx = Xs - FMath::FloorToFloat(Xs / Len) * Len;
            const float Edge = FMath::Min(FMath::Min(Fy, 7.5f - Fy), FMath::Min(Fx, Len - Fx));
            const float Grime = TileFbm(U, V, 6, 4, Seed + 3, .35f), Fine = TileFbm(U, V, 64, 2, Seed + 9);
            FLinearColor C;
            if (Edge < .5f)
            {
                C = Mortar * (.78f + .3f * Fine);
                Height[Yp * W + Xp] = 0.f;
            }
            else
            {
                const float Tone = Hash01(Index, Course, Seed);
                C = Body * (.72f + .52f * Tone);
                C.R *= .94f + .12f * Hash01(Index, Course, Seed + 1);
                if (bHeader && Hash01(Index, Course, Seed + 5) < .38f) C = FMath::Lerp(C, Burnt, .75f);
                C = C * (.86f + .28f * Fine);
                Height[Yp * W + Xp] = FMath::Clamp((Edge - .5f) / 1.1f, 0.f, 1.f) * (.86f + .14f * Fine);
            }
            Img.At(Xp, Yp) = C * (.76f + .34f * Grime);
        }
}

/** Rusticated limestone ashlar: 60 x 30 cm blocks, chamfered joints, each block its own tone, darker toward its foot. */
void PaintAshlar(FHallImage& Img, TArray<float>& Height, int32 W, int32 H, float WidthCm, float HeightCm, float BlockW, float BlockH, float Chamfer, int32 Seed)
{
    Img.Init(W, H, FLinearColor::Black); Height.Init(0.f, W * H);
    const FLinearColor Stone(.50f, .46f, .385f), Joint(.16f, .15f, .13f);
    for (int32 Yp = 0; Yp < H; ++Yp)
        for (int32 Xp = 0; Xp < W; ++Xp)
        {
            const float U = (Xp + .5f) / W, V = (Yp + .5f) / H, Xc = U * WidthCm, Yc = V * HeightCm;
            const int32 Row = FMath::FloorToInt(Yc / BlockH);
            const float Fy = Yc - Row * BlockH, Xs = Xc + ((Row & 1) ? BlockW * .5f : 0.f);
            const int32 PerRow = FMath::Max(1, FMath::RoundToInt(WidthCm / BlockW));
            const int32 Index = FMath::FloorToInt(Xs / BlockW) % PerRow;
            const float Fx = Xs - FMath::FloorToFloat(Xs / BlockW) * BlockW;
            const float Edge = FMath::Min(FMath::Min(Fy, BlockH - Fy), FMath::Min(Fx, BlockW - Fx));
            const float Fine = TileFbm(U, V, 96, 2, Seed + 2), Cloud = TileFbm(U, V, 8, 4, Seed + 4, .5f);
            if (Edge < .6f) { Img.At(Xp, Yp) = Joint * (.8f + .4f * Fine); continue; }
            const float T = FMath::Clamp((Edge - .6f) / FMath::Max(.1f, Chamfer), 0.f, 1.f);
            const float Tone = .86f + .22f * Hash01(Index, Row, Seed);
            FLinearColor C = Stone * Tone * (.9f + .18f * Fine) * (.84f + .26f * Cloud);
            C = C * (1.f - .1f * FMath::Clamp(Fy / BlockH, 0.f, 1.f));      // soot settles toward each block's foot
            C = FMath::Lerp(C * .82f, C, T);                                 // chamfers sit in shadow
            Img.At(Xp, Yp) = C;
            Height[Yp * W + Xp] = T * (.88f + .12f * Fine);
        }
}

/** Verdigris washed down from copper: streaks start at Y0 (cm from the top) and fade over Length. */
void Streak(FHallImage& Img, float WidthCm, float HeightCm, float Xc, float Y0, float Length, float Width, float Strength)
{
    const FLinearColor Patina(.13f, .30f, .24f);
    const float PxX = Img.W / WidthCm, PxY = Img.H / HeightCm;
    const int32 Y1 = FMath::Clamp(FMath::RoundToInt((Y0 + Length) * PxY), 0, Img.H);
    for (int32 Yp = FMath::Clamp(FMath::RoundToInt(Y0 * PxY), 0, Img.H); Yp < Y1; ++Yp)
    {
        const float T = (Yp / PxY - Y0) / Length;
        const float Wiggle = 1.5f * FMath::Sin(Yp * .07f + Xc) + .8f * FMath::Sin(Yp * .19f + Xc * 3.f);
        const float Half = Width * (1.f - .55f * T) * .5f, Mid = Xc + Wiggle * (1.f - T);
        const float A = Strength * FMath::Square(1.f - T);
        for (int32 Xp = FMath::FloorToInt((Mid - Half - 2.f) * PxX); Xp <= FMath::CeilToInt((Mid + Half + 2.f) * PxX); ++Xp)
        {
            const float D = FMath::Abs(Xp / PxX - Mid);
            Img.Blend(Xp, Yp, Patina, A * FMath::Clamp((Half + 1.f - D) / 2.f, 0.f, 1.f));
        }
    }
}

void PaintClockStone(FHallImage& Img, TArray<float>& Height, int32 Seed)
{
    // One face of the clock stage, 624 x 800 cm, top of the texture at the cornice.
    constexpr float WCm = 624.f, HCm = 800.f;
    PaintAshlar(Img, Height, 512, 1024, WCm, HCm, 78.f, 40.f, 1.2f, Seed);
    FRandomStream R(Seed);
    for (float X = 8.f; X < WCm; X += R.FRandRange(14.f, 34.f)) Streak(Img, WCm, HCm, X, 0.f, R.FRandRange(40.f, 260.f), R.FRandRange(3.f, 11.f), R.FRandRange(.25f, .6f));
    // Under the dial's copper bezel (centre 400 cm down, radius 199 cm).
    for (float X = -185.f; X <= 185.f; X += R.FRandRange(7.f, 18.f))
    {
        const float Bottom = 400.f + FMath::Sqrt(FMath::Max(0.f, 199.f * 199.f - X * X));
        Streak(Img, WCm, HCm, WCm * .5f + X, Bottom - 2.f, R.FRandRange(60.f, HCm - Bottom), R.FRandRange(3.f, 10.f), R.FRandRange(.3f, .75f) * (1.f - FMath::Abs(X) / 260.f));
    }
}

void PaintBelfryBrick(FHallImage& Img, TArray<float>& Height, int32 Seed)
{
    // One face of the belfry, 600 x 820 cm, top at the clock stage's corbel table.
    constexpr float WCm = 600.f, HCm = 820.f;
    PaintBrick(Img, Height, 512, 1024, WCm, HCm, Seed);
    FRandomStream R(Seed + 1);
    for (float X = 6.f; X < WCm; X += R.FRandRange(16.f, 40.f)) Streak(Img, WCm, HCm, X, 50.f, R.FRandRange(60.f, 420.f), R.FRandRange(3.f, 9.f), R.FRandRange(.18f, .45f));
    for (const float Sill : { 300.f - 200.f, 300.f + 200.f, 300.f })
        for (int32 K = 0; K < 5; ++K) Streak(Img, WCm, HCm, Sill + R.FRandRange(-40.f, 40.f), 740.f, R.FRandRange(40.f, 80.f), R.FRandRange(4.f, 9.f), .45f);
}

void PaintCopper(FHallImage& Img, TArray<float>& Height, int32 Seed)
{
    constexpr int32 S = 512;
    Img.Init(S, S, FLinearColor::Black); Height.Init(0.f, S * S);
    const FLinearColor Patina(.12f, .29f, .23f), Deep(.05f, .13f, .105f), Bronze(.15f, .085f, .045f);
    for (int32 Yp = 0; Yp < S; ++Yp)
        for (int32 Xp = 0; Xp < S; ++Xp)
        {
            const float U = (Xp + .5f) / S, V = (Yp + .5f) / S;
            const float Rain = TileFbm(U, V, 24, 4, Seed, .2f), Blotch = TileFbm(U, V, 6, 4, Seed + 5), Fine = TileFbm(U, V, 96, 2, Seed + 9);
            FLinearColor C = FMath::Lerp(Deep, Patina, FMath::Clamp(Rain * 1.6f - .25f, 0.f, 1.f));
            C = FMath::Lerp(C, Bronze, FMath::Clamp((Blotch - .62f) * 3.f, 0.f, .7f));
            Img.At(Xp, Yp) = C * (.85f + .3f * Fine);
            Height[Yp * S + Xp] = Rain * .6f + Fine * .4f;
        }
}

UTexture2D* MakeTex(UHouseBlackHallSubsystem* Look, const TCHAR* Name, const FHallImage& Img, bool bWrap)
{
    UTexture2D* T = HallMakeTexture(Img, true, false, Name, bWrap);
    if (T) Look->Textures.Add(FName(Name), T);
    return T;
}
UTexture2D* MakeNormal(UHouseBlackHallSubsystem* Look, const TCHAR* Name, const TArray<float>& Height, int32 W, int32 H, float Strength, bool bWrap)
{
    FHallImage N; HallHeightToNormal(Height, W, H, Strength, bWrap, N);
    UTexture2D* T = HallMakeTexture(N, false, true, Name, bWrap);
    if (T) Look->Textures.Add(FName(Name), T);
    return T;
}

void CreateLook(UHouseBlackHallSubsystem* Look)
{
    if (!Look || Look->Materials.Contains(KBrick0)) return;
    // Warm campus floods below the tower, imitated by a little emission that fades up the shaft (stepping at the
    // string courses, which hide the steps). No light components.
    const FLinearColor Uplight(1.f, .58f, .28f);
    FHallImage Img; TArray<float> Height;
    PaintBrick(Img, Height, 512, 512, 120.f, 120.f, 1897);
    UTexture2D* BrickT = MakeTex(Look, TEXT("T_CT_Brick"), Img, true);
    UTexture2D* BrickN = MakeNormal(Look, TEXT("T_CT_BrickN"), Height, Img.W, Img.H, 2.4f, true);
    HallMakeTextured(Look, KBrick0, BrickT, BrickN, .86f, FLinearColor::White, 1.f, 0.f, Uplight * 1.2f);
    HallMakeTextured(Look, KBrick1, BrickT, BrickN, .86f, FLinearColor::White, 1.f, 0.f, Uplight * .55f);
    HallMakeTextured(Look, KBrick2, BrickT, BrickN, .86f, FLinearColor::White, 1.f, 0.f, Uplight * .22f);
    PaintAshlar(Img, Height, 512, 512, 240.f, 240.f, 60.f, 30.f, 3.f, 1898);
    HallMakeTextured(Look, KAshlar, MakeTex(Look, TEXT("T_CT_Ashlar"), Img, true), MakeNormal(Look, TEXT("T_CT_AshlarN"), Height, Img.W, Img.H, 3.f, true),
        .8f, FLinearColor::White, 1.f, 0.f, Uplight * 1.3f);
    PaintBelfryBrick(Img, Height, 1899);
    HallMakeTextured(Look, KBelfry, MakeTex(Look, TEXT("T_CT_Belfry"), Img, false), MakeNormal(Look, TEXT("T_CT_BelfryN"), Height, Img.W, Img.H, 1.6f, false), .86f);
    PaintClockStone(Img, Height, 1900);
    // The dials spill a faint warmth onto the stone around them.
    HallMakeTextured(Look, KClockStone, MakeTex(Look, TEXT("T_CT_ClockStone"), Img, false), MakeNormal(Look, TEXT("T_CT_ClockStoneN"), Height, Img.W, Img.H, 1.8f, false),
        .78f, FLinearColor::White, 1.f, 0.f, FLinearColor(.30f, .19f, .085f));
    PaintCopper(Img, Height, 1901);
    HallMakeTextured(Look, KCopper, MakeTex(Look, TEXT("T_CT_Copper"), Img, true), MakeNormal(Look, TEXT("T_CT_CopperN"), Height, Img.W, Img.H, 1.2f, true), .55f, FLinearColor::White, 1.f, .25f);

    HallMakePlain(Look, KStoneLit, FLinearColor(.52f, .485f, .41f), .74f, 0.f, Uplight * 1.1f);
    HallMakePlain(Look, KStone, FLinearColor(.52f, .485f, .41f), .74f);
    HallMakePlain(Look, KStoneDeep, FLinearColor(.12f, .11f, .095f), .9f);
    HallMakePlain(Look, KCopperTrim, FLinearColor(.14f, .32f, .26f), .5f, .3f);
    HallMakePlain(Look, KIron, FLinearColor(.016f, .016f, .018f), .55f, .6f);
    HallMakePlain(Look, KGilt, FLinearColor(.86f, .63f, .24f), .28f, 1.f);
    // Backlit opal glass: the panes alternate a few percent so the dial reads as leaded glass, not a decal.
    HallMakePlain(Look, KDialA, FLinearColor(.86f, .80f, .66f), .35f, 0.f, FLinearColor(24.f, 15.6f, 7.2f));
    HallMakePlain(Look, KDialB, FLinearColor(.86f, .80f, .66f), .35f, 0.f, FLinearColor(22.1f, 14.4f, 6.6f));
    HallMakePlain(Look, KLancet, FLinearColor(.012f, .016f, .024f), .12f, .35f);
    HallMakePlain(Look, KLancetLit, FLinearColor(.5f, .35f, .2f), .3f, 0.f, FLinearColor(.95f, .56f, .24f));
    HallMakePlain(Look, KLouvre, FLinearColor(.075f, .072f, .068f), .8f);
    HallMakePlain(Look, KOak, FLinearColor(.085f, .052f, .03f), .8f);
    HallMakePlain(Look, KBronze, FLinearColor(.42f, .27f, .12f), .33f, .95f, FLinearColor(.02f, .013f, .006f));
    HallMakePlain(Look, KInterior, FLinearColor(.03f, .028f, .025f), .95f);
    HallMakePlain(Look, KCrow, FLinearColor(.011f, .011f, .015f), .45f);
    HallMakePlain(Look, KPigeon, FLinearColor(.17f, .17f, .19f), .8f);
    HallMakePlain(Look, KVine, FLinearColor(.07f, .045f, .03f), .9f);
    HallMakePlain(Look, KLens, FLinearColor(1.f, .85f, .6f), .3f, 0.f, FLinearColor(5.f, 3.4f, 1.9f));
    // Boston ivy turning for autumn: scarlet, crimson and a few late green leaves, lit a little by the same floods.
    // Their glow is the leaf colour caught in the same floods as the stone behind them.
    HallMakePlain(Look, KIvy[0], FLinearColor(.34f, .04f, .022f), .78f, 0.f, FLinearColor(1.15f, .14f, .05f));
    HallMakePlain(Look, KIvy[1], FLinearColor(.2f, .018f, .03f), .78f, 0.f, FLinearColor(.72f, .055f, .085f));
    HallMakePlain(Look, KIvy[2], FLinearColor(.06f, .085f, .025f), .8f, 0.f, FLinearColor(.16f, .24f, .05f));
    // -ClockDialGlow= and -ClockUplight= scale the dial glow and the uplight (tuning; see SourceArt/ClockTower/README.md).
    float DialGlow = 1.f, UplightScale = 1.f;
    FParse::Value(FCommandLine::Get(), TEXT("ClockDialGlow="), DialGlow);
    FParse::Value(FCommandLine::Get(), TEXT("ClockUplight="), UplightScale);
    auto Scale = [Look](FName Key, float By)
    {
        UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Look->Materials.FindRef(Key));
        FLinearColor E;
        if (M && By != 1.f && M->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Emission")), E)) M->SetVectorParameterValue(TEXT("Emission"), E * FMath::Max(0.f, By));
    };
    for (const FName Key : { KDialA, KDialB, KClockStone }) Scale(Key, DialGlow);
    for (const FName Key : { KAshlar, KStoneLit, KBrick0, KBrick1, KBrick2, KIvy[0], KIvy[1], KIvy[2] }) Scale(Key, UplightScale);
}

// ------------------------------------------------------------------ the tower
void Plinth(FHallMesh& M)
{
    Square(M, KStoneLit, 395.f, -40.f, 25.f, StoneUV);   // stays clear of the walk (Y -10400) and the tree band (Y -11200)
    Square(M, KStoneLit, 372.f, 25.f, 50.f, StoneUV);
    Square(M, KStoneLit, 350.f, 50.f, 75.f, StoneUV);
}

void BaseStage(FHallMesh& M, FHallMesh& Detail, bool bOpen)
{
    // Ashlar core; the +Y face is a wall with the pointed doorway. Walk-in (bOpen): no top or bottom face, which were a plate
    // across the shaft at the base course (hidden under it) and one on the plinth's top.
    Square(M, KAshlar, BaseHalf, 75.f, HBaseTop, AshlarUV, bOpen ? (1 << 2) | (1 << 4) | (1 << 5) : 1 << 2);
    FOpening Door; Door.U = 0.f; Door.W = 95.f; Door.Sill = 75.f; Door.Spring = 330.f; Door.Sharp = .9f; Door.Fill = KOak; Door.FillDepth = 36.f;
    Door.bFillArchOnly = bOpen;   // below the springing, the campus interiors' door leaves (in this plane, Dist 294)
    Wall(M, KAshlar, 0, BaseHalf, BaseHalf, 75.f, HBaseTop, 60.f, { Door }, AshlarUV, KStoneLit);
    if (bOpen)
    {
        // The open shell's own inside: the ashlar box faces out only, so wherever the interiors' lining (258..260) is not
        // built on this machine (freed after a visit, not yet built, or the leaves out of a client's relevancy) the doorway
        // shows a dark shaft rather than the campus through back-face-culled walls. Behind the lining when it is built.
        constexpr float InnerFace = BaseHalf - 60.f;   // the door wall's inner face (270)
        for (int32 F = 1; F < 4; ++F)
            M.Quad(KInterior, At(F, InnerFace, -InnerFace, 75.f), At(F, InnerFace, InnerFace, 75.f), At(F, InnerFace, InnerFace, HBaseTop),
                At(F, InnerFace, -InnerFace, HBaseTop), -FaceN(F), PlainUV);
    }
    HoodMould(M, KStoneLit, 0, BaseHalf, Door, 14.f, 13.f, 9.f, 26.f);
    // Weathered base course (walk-in: a ring round the shaft, its ledge the same).
    if (bOpen) SquareRing(M, KStoneLit, 346.f, 260.f, 618.f, 664.f, StoneUV);
    else Square(M, KStoneLit, 346.f, 618.f, 664.f, StoneUV);
    // Date stone over the door, with an invented year.
    FaceBox(Detail, KStoneLit, 0, BaseHalf, BaseHalf + 6.f, -84.f, 84.f, 512.f, 574.f, StoneUV);
    Letters(Detail, KStoneDeep, TEXT("MDCCCXCVII"), At(0, BaseHalf + 6.2f, 0.f, 536.f), FaceU(0), FVector::UpVector, FaceN(0), 13.f, 1.9f, 1.f);
    Letters(Detail, KStoneDeep, TEXT("ERECTED"), At(0, BaseHalf + 6.2f, 0.f, 557.f), FaceU(0), FVector::UpVector, FaceN(0), 7.f, 1.2f, 1.f);
    // Door: boards, strap hinges, ring handles. Walk-in, the leaves below the springing are the campus interiors' doors
    // (HouseCampusInteriorsTower), plank leaves that carry these boards, the two lower straps and the rings in the same planes
    // and materials (EHallDoorStyle::Plank); only the tympanum's stay here.
    const float DoorFace = BaseHalf - Door.FillDepth + .6f, BoardFoot = bOpen ? Door.Spring : 75.f;
    for (float U = -84.f; U <= 84.f; U += 14.f) Bar(Detail, KStoneDeep, At(0, DoorFace, U, BoardFoot), At(0, DoorFace, U, Door.Spring + ArchRise(U, Door.W, Door.Sharp) - 2.f), 1.2f, .8f, FaceN(0));
    for (const float S : { -1.f, 1.f })
    {
        for (const float Z : { 135.f, 255.f, 370.f }) if (!bOpen || Z > Door.Spring) Bar(Detail, KIron, At(0, DoorFace + .6f, S * 90.f, Z), At(0, DoorFace + .6f, S * 18.f, Z), 5.f, 1.4f, FaceN(0));
        if (bOpen) continue;
        const FVector Hub = At(0, DoorFace + 2.f, S * 13.f, 205.f);
        for (int32 K = 0; K < 10; ++K)
        {
            const float A0 = 2.f * PI * K / 10, A1 = 2.f * PI * (K + 1) / 10;
            Bar(Detail, KIron, Hub + (FVector::UpVector * FMath::Cos(A0) + FaceU(0) * FMath::Sin(A0)) * 7.f, Hub + (FVector::UpVector * FMath::Cos(A1) + FaceU(0) * FMath::Sin(A1)) * 7.f, 1.4f, 1.4f, FaceN(0));
        }
    }
    Bar(Detail, KIron, At(0, DoorFace + .4f, 0.f, BoardFoot), At(0, DoorFace + .4f, 0.f, Door.Spring + ArchRise(0.f, Door.W, Door.Sharp) - 2.f), 4.f, 1.2f, FaceN(0));
}

void Shaft(FHallMesh& M, bool bOpen)
{
    // Three bands of brick, each a little less lit than the one below; lancets light the stair inside.
    for (int32 F = 0; F < 4; ++F)
    {
        FOpening Low; Low.U = 0.f; Low.W = 32.f; Low.Sill = 880.f; Low.Spring = 1240.f; Low.Sharp = 1.5f; Low.Fill = F == 1 ? KLancetLit : KLancet; Low.FillDepth = 18.f;
        Wall(M, KBrick0, F, ShaftHalf, ShaftHalf, HBaseTop + 10.f, HBand1, 40.f, { Low }, BrickUV, KStoneLit);
        HoodMould(M, KStoneLit, F, ShaftHalf, Low, 7.f, 8.f, 6.f, 16.f);
        FaceBox(M, KStoneLit, F, ShaftHalf - 4.f, ShaftHalf + 9.f, -44.f, 44.f, 862.f, 880.f, StoneUV);
        TArray<FOpening> Pair;
        for (const float U : { -62.f, 62.f })
        {
            FOpening O; O.U = U; O.W = 26.f; O.Sill = 1680.f; O.Spring = 2060.f; O.Sharp = 1.5f; O.Fill = (F == 0 && U < 0.f) ? KLancetLit : KLancet; O.FillDepth = 16.f;
            Pair.Add(O);
            HoodMould(M, KStone, F, ShaftHalf, O, 7.f, 7.f, 6.f, 14.f);
        }
        Wall(M, KBrick1, F, ShaftHalf, ShaftHalf, HBand1 + 22.f, HBand2, 40.f, Pair, BrickUV, KStone);
        FaceBox(M, KStone, F, ShaftHalf - 4.f, ShaftHalf + 9.f, -100.f, 100.f, 1662.f, 1680.f, StoneUV);
    }
    if (bOpen)
    {
        // Walk-in (HouseCampusInteriorsTower): the third band as four 40 cm walls like the two below (no plate across the
        // shaft), the string courses as rings round it; the same outside.
        for (int32 Face = 0; Face < 4; ++Face) Wall(M, KBrick2, Face, ShaftHalf, ShaftHalf, HBand2 + 22.f, HBelfry - 40.f, 40.f, {}, BrickUV, KStone);
        SquareRing(M, KStoneLit, 318.f, ShaftHalf - 40.f, HBand1, HBand1 + 22.f, StoneUV);
        SquareRing(M, KStone, 316.f, ShaftHalf - 40.f, HBand2, HBand2 + 22.f, StoneUV);
        // Under the belfry the stair climbs through a slot by the +Y wall (X -165..165, Y 165..260); west of it, over the last
        // landing and the head of the flight below (X -260..-165, Y 110..260), the course is a 12 cm slab for headroom (the
        // landing is 2540: 228 cm to its underside, where the full course would leave 200). Outer faces, ledge and top as the
        // solid course's; its top (2780) is still the belfry floor's collision where the interiors' slab is not.
        const float CourseFoot = HBelfry - 40.f;
        AxisBox(M, KStone, -324.f, 324.f, -324.f, 110.f, CourseFoot, HBelfry, StoneUV);
        AxisBox(M, KStone, -324.f, 324.f, 260.f, 324.f, CourseFoot, HBelfry, StoneUV);
        AxisBox(M, KStone, -324.f, -260.f, 110.f, 260.f, CourseFoot, HBelfry, StoneUV, 4 | 8);
        AxisBox(M, KStone, -260.f, -165.f, 110.f, 260.f, HBelfry - 12.f, HBelfry, StoneUV, 4 | 8);
        AxisBox(M, KStone, -165.f, 165.f, 110.f, 165.f, CourseFoot, HBelfry, StoneUV, 8);
        AxisBox(M, KStone, 165.f, 324.f, 110.f, 260.f, CourseFoot, HBelfry, StoneUV, 4 | 8);
    }
    else
    {
        Square(M, KBrick2, ShaftHalf, HBand2 + 22.f, HBelfry - 40.f, BrickUV);
        // String courses (the uplight steps hide behind them).
        Square(M, KStoneLit, 318.f, HBand1, HBand1 + 22.f, StoneUV);
        Square(M, KStone, 316.f, HBand2, HBand2 + 22.f, StoneUV);
        Square(M, KStone, 324.f, HBelfry - 40.f, HBelfry, StoneUV);
    }
    // Set-back buttresses, two at each corner, stepping in at the string courses under sloped weatherings.
    struct FStage { float H0, H1, Proj; FName Key; };
    const FStage Stages[] = { { 75.f, HBaseTop, 75.f, KAshlar }, { HBaseTop, HBand1, 75.f, KBrick0 }, { HBand1, HBand2, 50.f, KBrick1 }, { HBand2, HBelfry - 40.f, 25.f, KBrick2 } };
    for (int32 F = 0; F < 4; ++F)
        for (const float S : { -1.f, 1.f })
        {
            const float U0 = S < 0.f ? -255.f : 195.f, U1 = U0 + 60.f;
            for (int32 K = 0; K < int32(UE_ARRAY_COUNT(Stages)); ++K)
            {
                const FStage& St = Stages[K];
                const FHallUV& StageUV = St.Key == KAshlar ? AshlarUV : BrickUV;
                // Where the next stage steps back, the outer part of this one stops at the foot of the weathering.
                const float Next = K + 1 < int32(UE_ARRAY_COUNT(Stages)) ? Stages[K + 1].Proj : 0.f;
                const bool bSetOff = Next < St.Proj;
                FaceBox(M, St.Key, F, ShaftHalf - 2.f, ShaftHalf + (bSetOff ? Next : St.Proj), U0, U1, St.H0, St.H1, StageUV);
                if (bSetOff) FaceBox(M, St.Key, F, ShaftHalf + Next, ShaftHalf + St.Proj, U0, U1, St.H0, St.H1 - 30.f, StageUV);
            }
            Weathering(M, KStoneLit, F, U0, U1, ShaftHalf + 50.f, ShaftHalf + 75.f, HBand1 - 30.f, 30.f);
            Weathering(M, KStone, F, U0, U1, ShaftHalf + 25.f, ShaftHalf + 50.f, HBand2 - 30.f, 30.f);
            Weathering(M, KStone, F, U0, U1, ShaftHalf, ShaftHalf + 25.f, HBelfry - 70.f, 30.f);
        }
    // Quoins at the four arrises, long and short in turn.
    for (const float SX : { -1.f, 1.f })
        for (const float SY : { -1.f, 1.f })
        {
            int32 Course = 0;
            for (float Z = HBaseTop + 16.f; Z + 28.f < HBelfry - 40.f; Z += 32.f, ++Course)
            {
                if ((Z > HBand1 - 30.f && Z < HBand1 + 24.f) || (Z > HBand2 - 30.f && Z < HBand2 + 24.f)) continue;
                const FName Key = Z < HBand1 ? KStoneLit : KStone;
                const float LX = (Course & 1) ? 40.f : 22.f, LY = (Course & 1) ? 22.f : 40.f;
                AxisBox(M, Key, SX > 0.f ? ShaftHalf - LX : -ShaftHalf - 3.f, SX > 0.f ? ShaftHalf + 3.f : -ShaftHalf + LX,
                    SY > 0.f ? ShaftHalf - 6.f : -ShaftHalf - 3.f, SY > 0.f ? ShaftHalf + 3.f : -ShaftHalf + 6.f, Z, Z + 28.f, StoneUV);
                AxisBox(M, Key, SX > 0.f ? ShaftHalf - 6.f : -ShaftHalf - 3.f, SX > 0.f ? ShaftHalf + 3.f : -ShaftHalf + 6.f,
                    SY > 0.f ? ShaftHalf - LY : -ShaftHalf - 3.f, SY > 0.f ? ShaftHalf + 3.f : -ShaftHalf + LY, Z, Z + 28.f, StoneUV);
            }
        }
}

void Belfry(FHallMesh& M, bool bOpen)
{
    const FHallUV UV = BoxUV((HBelfry + HClock) * .5f, FVector(600.f, 600.f, HClock - HBelfry));
    for (int32 F = 0; F < 4; ++F)
    {
        FOpening Arch; Arch.U = 0.f; Arch.W = 105.f; Arch.Sill = 2860.f; Arch.Spring = 3260.f; Arch.Sharp = .75f;
        TArray<FOpening> Openings = { Arch };
        for (const float U : { -200.f, 200.f })
        {
            FOpening L; L.U = U; L.W = 34.f; L.Sill = 2880.f; L.Spring = 3220.f; L.Sharp = 1.4f; L.bLouvres = true;
            Openings.Add(L);
            HoodMould(M, KStone, F, ShaftHalf, L, 7.f, 8.f, 7.f, 14.f);
        }
        Wall(M, KBelfry, F, ShaftHalf, ShaftHalf, HBelfry, HClock, 45.f, Openings, UV, KStone);
        HoodMould(M, KStone, F, ShaftHalf, Arch, 11.f, 13.f, 10.f, 24.f);
        // Attached shafts in the arch jambs, with bases and capitals.
        for (const float S : { -1.f, 1.f })
        {
            M.Cylinder(KStone, At(F, ShaftHalf + 7.f, S * 115.f, 2875.f), At(F, ShaftHalf + 7.f, S * 115.f, 3242.f), 8.f, 10, PlainUV);
            FaceBox(M, KStone, F, ShaftHalf, ShaftHalf + 17.f, S * 115.f - 11.f, S * 115.f + 11.f, 2860.f, 2876.f, StoneUV);
            FaceBox(M, KStone, F, ShaftHalf, ShaftHalf + 18.f, S * 115.f - 12.f, S * 115.f + 12.f, 3242.f, 3262.f, StoneUV);
        }
        // A low stone balustrade across the open arch.
        FaceBox(M, KStone, F, ShaftHalf - 30.f, ShaftHalf - 14.f, -105.f, 105.f, 2948.f, 2958.f, StoneUV);
        for (float U = -90.f; U <= 90.f; U += 22.5f) M.Cylinder(KStone, At(F, ShaftHalf - 22.f, U, 2860.f), At(F, ShaftHalf - 22.f, U, 2948.f), 4.5f, 8, PlainUV);
        // Corbel table carrying the clock stage.
        for (float U = -290.f; U <= 291.f; U += 41.4f)
        {
            FaceBox(M, KStone, F, ShaftHalf - 4.f, ClockHalf + 4.f, U - 8.f, U + 8.f, 3572.f, HClock, StoneUV);
            FaceBox(M, KStone, F, ShaftHalf - 4.f, ShaftHalf + 8.f, U - 6.f, U + 6.f, 3550.f, 3572.f, StoneUV);
        }
    }
    // Floor and the timber bell frame.
    if (bOpen)
    {
        // Walk-in: the slot the stair climbs through (X -165..165, from Y 165 to the +Y wall), its trap lid standing open
        // against that wall, two ledges on its underside. No rail is drawn along the slot: one would show between the slats
        // of the +X face's louvred lancet in the Belfry review view, where the lid, against the +Y wall's inner face, is hidden
        // from every review view. The guard along the slot, like all the belfry's collision, is the walk-in tower's
        // (HouseCampusInteriorsTower's blockers, never drawn).
        SquareSlot(M, KInterior, ShaftHalf - 40.f, -165.f, 165.f, 165.f, ShaftHalf - 40.f, HBelfry - 10.f, HBelfry + 6.f, PlainUV);
        const float Deck = HBelfry + 6.f;
        AxisBox(M, KOak, -163.f, 163.f, 249.f, 254.f, Deck, Deck + 86.f, PlainUV);
        for (const float Ledge : { 14.f, 70.f }) AxisBox(M, KOak, -156.f, 156.f, 245.5f, 249.f, Deck + Ledge - 6.f, Deck + Ledge + 6.f, PlainUV);
    }
    else Square(M, KInterior, ShaftHalf - 40.f, HBelfry - 10.f, HBelfry + 6.f, PlainUV);
    for (const float SX : { -1.f, 1.f })
    {
        for (const float SY : { -1.f, 1.f })
        {
            AxisBox(M, KOak, SX * 100.f - 9.f, SX * 100.f + 9.f, SY * 150.f - 9.f, SY * 150.f + 9.f, HBelfry + 6.f, 3250.f, PlainUV);
            AxisBox(M, KOak, SX * 228.f - 8.f, SX * 228.f + 8.f, SY * 228.f - 8.f, SY * 228.f + 8.f, HBelfry + 6.f, 3330.f, PlainUV);
        }
        AxisBox(M, KOak, SX * 100.f - 10.f, SX * 100.f + 10.f, -165.f, 165.f, 3220.f, 3250.f, PlainUV);
        AxisBox(M, KOak, -240.f, 240.f, SX * 228.f - 9.f, SX * 228.f + 9.f, 3300.f, 3322.f, PlainUV);
        AxisBox(M, KOak, SX * 228.f - 9.f, SX * 228.f + 9.f, -240.f, 240.f, 3322.f, 3344.f, PlainUV);
        AxisBox(M, KOak, -110.f, 110.f, SX * 150.f - 8.f, SX * 150.f + 8.f, 2900.f, 2916.f, PlainUV);
    }
    for (const float SY : { -1.f, 1.f }) AxisBox(M, KOak, -240.f, 240.f, SY * 190.f - 7.f, SY * 190.f + 7.f, 3285.f, 3300.f, PlainUV);
}

void ClockStage(FHallMesh& M)
{
    Square(M, KClockStone, ClockHalf, HClock, HCornice, BoxUV((HClock + HCornice) * .5f, FVector(ClockHalf * 2.f, ClockHalf * 2.f, HCornice - HClock)));
    for (int32 F = 0; F < 4; ++F)
    {
        // Square stone surround to each dial.
        FaceBox(M, KStone, F, ClockHalf - 2.f, ClockHalf + 7.f, -234.f, 234.f, HDial + 214.f, HDial + 234.f, StoneUV);
        FaceBox(M, KStone, F, ClockHalf - 2.f, ClockHalf + 7.f, -234.f, 234.f, HDial - 234.f, HDial - 214.f, StoneUV);
        FaceBox(M, KStone, F, ClockHalf - 2.f, ClockHalf + 7.f, -234.f, -214.f, HDial - 214.f, HDial + 214.f, StoneUV);
        FaceBox(M, KStone, F, ClockHalf - 2.f, ClockHalf + 7.f, 214.f, 234.f, HDial - 214.f, HDial + 214.f, StoneUV);
        // Machicolation corbels under the cornice.
        for (float U = -296.f; U <= 297.f; U += 34.8f) FaceBox(M, KStone, F, ClockHalf - 4.f, 333.f, U - 7.f, U + 7.f, 4370.f, HCornice, StoneUV);
        // Parapet and merlons with copings.
        FaceBox(M, KStone, F, 306.f, 330.f, -330.f, 330.f, 4445.f, HParapet, StoneUV);
        for (float U = -294.f; U <= 295.f; U += 65.3f)
        {
            FaceBox(M, KStone, F, 306.f, 330.f, U - 20.f, U + 20.f, HParapet, HMerlon, StoneUV);
            FaceBox(M, KStone, F, 303.f, 333.f, U - 23.f, U + 23.f, HMerlon, HMerlon + 8.f, StoneUV);
        }
    }
    Square(M, KStone, 337.f, HCornice, 4445.f, StoneUV);
    // Engaged shafts at the corners.
    for (const float SX : { -1.f, 1.f })
        for (const float SY : { -1.f, 1.f })
            M.Cylinder(KStone, HouseClockTowerSite::Center(HClock) + FVector(SX * (ClockHalf - 3.f), SY * (ClockHalf - 3.f), 0.f),
                HouseClockTowerSite::Center(4370.f) + FVector(SX * (ClockHalf - 3.f), SY * (ClockHalf - 3.f), 0.f), 15.f, 12, PlainUV);
    // Corner pinnacles with crockets, stone caps and gilt knobs.
    for (const float SX : { -1.f, 1.f })
        for (const float SY : { -1.f, 1.f })
        {
            const FVector P(CX + SX * 318.f, CY + SY * 318.f, GZ());
            M.BoxMinMax(KStone, P + FVector(-38.f, -38.f, 4445.f), P + FVector(38.f, 38.f, 5080.f), StoneUV);
            M.BoxMinMax(KStone, P + FVector(-44.f, -44.f, 5070.f), P + FVector(44.f, 44.f, 5085.f), StoneUV);
            Pyramid(M, KStone, P + FVector(0.f, 0.f, 5085.f), 42.f, 300.f, StoneUV);
            for (int32 Hip = 0; Hip < 4; ++Hip)
            {
                const FVector D(Hip < 2 ? 1.f : -1.f, (Hip % 2) ? 1.f : -1.f, 0.f);
                for (int32 K = 1; K <= 3; ++K)
                {
                    const float T = K / 4.f;
                    const FVector C = P + FVector(0.f, 0.f, 5085.f + 300.f * T) + D * 42.f * (1.f - T);
                    M.BoxMinMax(KStone, C - FVector(5.f, 5.f, 4.f) + D * 3.f, C + FVector(5.f, 5.f, 6.f) + D * 3.f, StoneUV);
                }
            }
            Ball(M, KGilt, P + FVector(0.f, 0.f, 5396.f), 10.f, 10);
        }
}

void Spire(FHallMesh& M)
{
    const FVector Base = HouseClockTowerSite::Center(HSpire), Apex = HouseClockTowerSite::Center(HApex);
    const float Rise = HApex - HSpire;
    for (int32 F = 0; F < 4; ++F)
    {
        const FVector N = FaceN(F), U = FaceU(F);
        const FVector Out = (N * Rise + FVector::UpVector * SpireHalf).GetSafeNormal();
        Tri(M, KCopper, Base + N * SpireHalf - U * SpireHalf, Base + N * SpireHalf + U * SpireHalf, Apex, Out, CopperUV);
        // Bellcast eave kicking out over the parapet walk.
        const float EaveH = 135.f, InHalf = SpireHalf * (1.f - EaveH / Rise);
        const FVector Kick = (N * 1.f + FVector::UpVector * .55f).GetSafeNormal();
        M.Quad(KCopper, Base + N * 292.f - U * 292.f + FVector(0, 0, -14.f), Base + N * 292.f + U * 292.f + FVector(0, 0, -14.f),
            Base + N * InHalf + U * InHalf + FVector(0, 0, EaveH), Base + N * InHalf - U * InHalf + FVector(0, 0, EaveH), Kick, CopperUV);
        // Standing-seam ribs running to the apex.
        for (int32 K = 1; K < 8; ++K)
        {
            const float Uk = -SpireHalf + 2.f * SpireHalf * K / 8.f;
            Bar(M, KCopperTrim, Base + N * (SpireHalf + 1.5f) + U * Uk, Apex + Out * 1.5f, 3.2f, 3.f, Out);
        }
        // A lucarne on each face, louvred.
        const float LH = 4880.f, LD = 250.f;
        FaceBox(M, KCopper, F, 150.f, LD, -40.f, 40.f, LH, LH + 120.f, CopperUV);
        M.Quad(KLouvre, At(F, LD + .5f, -28.f, LH + 14.f), At(F, LD + .5f, 28.f, LH + 14.f), At(F, LD + .5f, 28.f, LH + 100.f), At(F, LD + .5f, -28.f, LH + 100.f), N, PlainUV);
        for (float Z = LH + 24.f; Z < LH + 98.f; Z += 14.f) FaceBox(M, KCopperTrim, F, LD, LD + 3.f, -28.f, 28.f, Z, Z + 3.f, PlainUV);
        Tri(M, KCopper, At(F, LD, -42.f, LH + 120.f), At(F, LD, 42.f, LH + 120.f), At(F, LD, 0.f, LH + 182.f), N, CopperUV);
        for (const float S : { -1.f, 1.f })
            M.Quad(KCopper, At(F, LD + 6.f, S * 48.f, LH + 116.f), At(F, 140.f, S * 48.f, LH + 116.f), At(F, 140.f, 0.f, LH + 184.f), At(F, LD + 6.f, 0.f, LH + 184.f),
                (U * S * 62.f + FVector::UpVector * 48.f).GetSafeNormal(), CopperUV);
    }
    // Hip rolls.
    for (const float SX : { -1.f, 1.f })
        for (const float SY : { -1.f, 1.f })
            Bar(M, KCopperTrim, Base + FVector(SX * SpireHalf, SY * SpireHalf, 0.f), Apex, 7.f, 7.f, (FVector(SX, SY, 0.f).GetSafeNormal() * Rise + FVector::UpVector * SpireHalf).GetSafeNormal());
    // Gilt finial and the fixed cardinal arms of the vane.
    M.Cylinder(KCopperTrim, Apex - FVector(0, 0, 40.f), Apex + FVector(0, 0, 18.f), 16.f, 12, PlainUV);
    Ball(M, KGilt, Apex + FVector(0, 0, 52.f), 32.f, 16);
    M.Cylinder(KGilt, Apex + FVector(0, 0, 80.f), Apex + FVector(0, 0, 282.f), 4.f, 8, PlainUV);
    Ball(M, KGilt, Apex + FVector(0, 0, 286.f), 8.f, 10);
    const FVector Arms = Apex + FVector(0, 0, 150.f);
    Bar(M, KIron, Arms - FVector(72.f, 0, 0), Arms + FVector(72.f, 0, 0), 3.f, 3.f, FVector::UpVector);
    Bar(M, KIron, Arms - FVector(0, 72.f, 0), Arms + FVector(0, 72.f, 0), 3.f, 3.f, FVector::UpVector);
    struct FPoint { TCHAR Ch; FVector Dir; };
    // Campus north is -Y (the fraternity house faces it); the sun sets toward -X.
    const FPoint Points[] = { { TEXT('N'), FVector(0, -1, 0) }, { TEXT('S'), FVector(0, 1, 0) }, { TEXT('E'), FVector(1, 0, 0) }, { TEXT('W'), FVector(-1, 0, 0) } };
    for (const FPoint& Pt : Points)
        Letters(M, KIron, FString(1, &Pt.Ch), Arms + Pt.Dir * 78.f + FVector(0, 0, -10.f), FVector::CrossProduct(Pt.Dir, FVector::UpVector), FVector::UpVector, Pt.Dir, 22.f, 2.6f, 1.6f);
}

void Dials(FHallMesh& M)
{
    for (int32 F = 0; F < 4; ++F)
    {
        const FVector N = FaceN(F), R = FaceU(F), C = At(F, ClockHalf, 0.f, HDial);
        auto P = [&](float Radius, float Angle, float Off) { return C + N * Off + (FVector::UpVector * FMath::Cos(Angle) + R * FMath::Sin(Angle)) * Radius; };
        // Opal glass in twelve leaded panes, the chapter ring offset against the centre.
        for (int32 Pane = 0; Pane < 12; ++Pane)
            for (int32 Sub = 0; Sub < 4; ++Sub)
            {
                const float A0 = (Pane * 4 + Sub) * PI / 24.f - PI / 12.f, A1 = A0 + PI / 24.f;
                Tri(M, Pane % 2 ? KDialA : KDialB, C + N * 2.f, P(110.f, A0, 2.f), P(110.f, A1, 2.f), N, PlainUV);
                M.Quad(Pane % 2 ? KDialB : KDialA, P(110.f, A0, 2.f), P(110.f, A1, 2.f), P(DialR, A1, 2.f), P(DialR, A0, 2.f), N, PlainUV);
            }
        // Iron: tracery, chapter ring, minute track, numerals.
        const FVector Iron = N * 3.4f;
        Ring(M, KIron, C + Iron, N, R, 108.5f, 111.5f, 96);
        Ring(M, KIron, C + Iron, N, R, 59.f, 61.f, 64);
        Ring(M, KIron, C + Iron, N, R, 165.2f, 166.8f, 120);
        Ring(M, KIron, C + Iron, N, R, 176.1f, 177.9f, 120);
        for (int32 K = 0; K < 12; ++K)
        {
            const float A = K * PI / 6.f;
            const FVector D = FVector::UpVector * FMath::Cos(A) + R * FMath::Sin(A), T = FVector::CrossProduct(N, D).GetSafeNormal();
            M.Quad(KIron, C + Iron + D * 22.f - T * 1.3f, C + Iron + D * 22.f + T * 1.3f, C + Iron + D * 109.f + T * 1.3f, C + Iron + D * 109.f - T * 1.3f, N, PlainUV);
        }
        for (int32 K = 0; K < 60; ++K)
        {
            const float A = K * PI / 30.f, HalfW = K % 5 == 0 ? 1.6f : .65f;
            const FVector D = FVector::UpVector * FMath::Cos(A) + R * FMath::Sin(A), T = FVector::CrossProduct(N, D).GetSafeNormal();
            M.Quad(KIron, C + Iron + D * 166.f - T * HalfW, C + Iron + D * 166.f + T * HalfW, C + Iron + D * 177.f + T * HalfW, C + Iron + D * 177.f - T * HalfW, N, PlainUV);
        }
        static const TCHAR* Numerals[12] = { TEXT("XII"), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IIII"), TEXT("V"), TEXT("VI"), TEXT("VII"), TEXT("VIII"), TEXT("IX"), TEXT("X"), TEXT("XI") };
        for (int32 K = 0; K < 12; ++K)
        {
            // Tower-clock style: each numeral stands on the centre, so the lower ones read upside down.
            const float A = K * PI / 6.f;
            const FVector Out = FVector::UpVector * FMath::Cos(A) + R * FMath::Sin(A), Along = -FVector::UpVector * FMath::Sin(A) + R * FMath::Cos(A);
            Numeral(M, KIron, Numerals[K], C + Iron + Out * 138.f, Along, Out, N, 36.f);
        }
        // Copper bezel and spandrels out to the square stone surround.
        const FVector Bez = N * 5.5f;
        Ring(M, KCopperTrim, C + Bez, N, R, DialR, 199.f, 96);
        for (int32 K = 0; K < 96; ++K)
        {
            const float A0 = 2.f * PI * K / 96, A1 = 2.f * PI * (K + 1) / 96;
            const FVector D0 = FVector::UpVector * FMath::Cos(A0) + R * FMath::Sin(A0), D1 = FVector::UpVector * FMath::Cos(A1) + R * FMath::Sin(A1);
            M.Quad(KCopperTrim, C + D0 * 199.f, C + D1 * 199.f, C + Bez + D1 * 199.f, C + Bez + D0 * 199.f, (D0 + D1).GetSafeNormal(), PlainUV);
            M.Quad(KCopperTrim, C + N * 2.f + D0 * DialR, C + N * 2.f + D1 * DialR, C + Bez + D1 * DialR, C + Bez + D0 * DialR, -(D0 + D1).GetSafeNormal(), PlainUV);
        }
        for (int32 K = 0; K < 48; ++K)
        {
            const float A0 = 2.f * PI * K / 48, A1 = 2.f * PI * (K + 1) / 48;
            auto Edge = [&](float A) { const float S = 214.f / FMath::Max(FMath::Abs(FMath::Cos(A)), FMath::Abs(FMath::Sin(A))); return C + N * 3.f + (FVector::UpVector * FMath::Cos(A) + R * FMath::Sin(A)) * S; };
            M.Quad(KCopperTrim, P(199.f, A0, 3.f), P(199.f, A1, 3.f), Edge(A1), Edge(A0), N, PlainUV);
        }
    }
}

void Ivy(FHallMesh& M)
{
    // Boston ivy climbing the ashlar either side of the door and round the north-west corner.
    FRandomStream R(41117);
    struct FPatch { int32 F; float U0, U1; };
    const FPatch Patches[] = { { 0, -318.f, -150.f }, { 0, 150.f, 300.f }, { 3, 140.f, 318.f }, { 1, 210.f, 318.f } };
    for (const FPatch& Patch : Patches)
        for (int32 Vine = 0; Vine < 5; ++Vine)
        {
            const FVector N = FaceN(Patch.F), U = FaceU(Patch.F);
            float Uc = R.FRandRange(Patch.U0, Patch.U1), H = 75.f;
            const float Top = R.FRandRange(220.f, 600.f);
            auto Surface = [](float U) { return FMath::Abs(U) >= 195.f && FMath::Abs(U) <= 255.f ? ShaftHalf + 76.2f : BaseHalf + 1.2f; };
            FVector Prev = At(Patch.F, Surface(Uc), Uc, H);
            while (H < Top)
            {
                H += R.FRandRange(14.f, 26.f);
                Uc = FMath::Clamp(Uc + R.FRandRange(-10.f, 10.f), Patch.U0, Patch.U1);
                const FVector Q = At(Patch.F, Surface(Uc), Uc, H);
                Bar(M, KVine, Prev, Q, 2.f, 1.4f, N);
                const int32 Leaves = H < Top - 60.f ? 3 : 1;
                for (int32 L = 0; L < Leaves; ++L)
                {
                    // A rounded three-lobed leaf (r = .72 + .28 cos 3a), tipped out from the wall on its stalk.
                    const float S = R.FRandRange(6.f, 10.f) * (H < 300.f ? 1.f : .8f), Spin = R.FRandRange(-.9f, .9f);
                    // Each leaf sits on the surface under its own spot (wall or buttress face), not the vine's.
                    // Random draws in a fixed order (argument evaluation order is unspecified).
                    const float LeafU = FMath::Clamp(Uc + R.FRandRange(-15.f, 15.f), Patch.U0, Patch.U1);
                    const float LeafH = H + R.FRandRange(-9.f, 9.f);
                    const float Lift = R.FRandRange(1.5f, 5.f);
                    const FVector Leaf = At(Patch.F, Surface(LeafU), LeafU, LeafH) + N * Lift;
                    const FVector Ax = U * FMath::Cos(Spin) + FVector::UpVector * FMath::Sin(Spin), Ay = (-U * FMath::Sin(Spin) + FVector::UpVector * FMath::Cos(Spin) + N * .4f).GetSafeNormal();
                    const FVector LeafN = FVector::CrossProduct(Ax, Ay).GetSafeNormal() * (FVector::DotProduct(FVector::CrossProduct(Ax, Ay), N) < 0.f ? -1.f : 1.f);
                    const float Pick = R.FRand();
                    const FName Key = Pick < .55f ? KIvy[0] : Pick < .9f ? KIvy[1] : KIvy[2];
                    TArray<FVector> Rim;
                    constexpr int32 RimPoints = 12;
                    for (int32 P = 0; P < RimPoints; ++P)
                    {
                        const float A = PI * .5f + P * 2.f * PI / RimPoints, Rad = (.72f + .28f * FMath::Cos(3.f * (A - PI * .5f))) * S;
                        Rim.Add(Leaf + Ax * FMath::Cos(A) * Rad + Ay * (FMath::Sin(A) * Rad + .2f * S));
                    }
                    for (int32 P = 0; P < RimPoints; ++P) Tri(M, Key, Leaf + Ay * .2f * S, Rim[P], Rim[(P + 1) % RimPoints], LeafN, PlainUV);
                }
                Prev = Q;
            }
        }
}

void Fixtures(FHallMesh& M)
{
    // The floods that "light" the stone: small bronze cans on the plinth, aimed up, lens glowing (emission only).
    for (int32 F = 0; F < 4; ++F)
        for (const float U : { -250.f, 250.f })
        {
            const FVector Base = At(F, 388.f, U, 25.f);
            const FVector Aim = (FaceN(F) * -.45f + FVector::UpVector).GetSafeNormal();
            M.Cylinder(KIron, Base, Base + Aim * 26.f, 10.f, 10, PlainUV);
            M.Cylinder(KLens, Base + Aim * 26.f, Base + Aim * 27.f, 8.5f, 10, PlainUV);
            Bar(M, KIron, Base - FVector(0, 0, 4.f), Base + FVector(0, 0, 4.f), 12.f, 12.f, FaceN(F));
        }
}

/** Raw-triangle writer for the birds (flat shaded; constant topology). */
struct FBirdWriter
{
    TArray<FVector>& V; TArray<FVector>& N; TArray<int32>& I;
    void Add(const FVector& A, const FVector& B, const FVector& C, const FVector& Out)
    {
        const int32 Base = V.Num();
        const FVector Nrm = Out.GetSafeNormal();
        V.Append({ A, B, C }); N.Append({ Nrm, Nrm, Nrm });
        if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Nrm) < 0) I.Append({ Base, Base + 1, Base + 2 });
        else I.Append({ Base, Base + 2, Base + 1 });
    }
    void Both(const FVector& A, const FVector& B, const FVector& C)
    {
        const FVector Face = FVector::CrossProduct(B - A, C - A);
        Add(A, B, C, Face); Add(A, B, C, -Face);
    }
};

void AppendBird(const FTransform& Pose, float Flap, float Fold, TArray<FVector>& V, TArray<FVector>& N, TArray<int32>& I)
{
    FBirdWriter W{ V, N, I };
    auto P = [&](const FVector& L) { return Pose.TransformPosition(L); };
    const FVector Nose = P(FVector(21, 0, 4)), Top = P(FVector(4, 0, 8)), Bottom = P(FVector(3, 0, -5)), Left = P(FVector(4, -6.5f, 1.5f)), Right = P(FVector(4, 6.5f, 1.5f)), Tail = P(FVector(-13, 0, 2));
    const FVector Mid = P(FVector(4, 0, 1.5f));
    auto Body = [&](const FVector& A, const FVector& B, const FVector& C) { W.Add(A, B, C, (A + B + C) / 3.f - Mid); };
    Body(Nose, Top, Right); Body(Nose, Right, Bottom); Body(Nose, Bottom, Left); Body(Nose, Left, Top);
    Body(Tail, Right, Top); Body(Tail, Bottom, Right); Body(Tail, Left, Bottom); Body(Tail, Top, Left);
    W.Both(P(FVector(-11, -2.5f, 2)), P(FVector(-27, -7, 1)), P(FVector(-27, 7, 1)));
    W.Both(P(FVector(-11, -2.5f, 2)), P(FVector(-27, 7, 1)), P(FVector(-11, 2.5f, 2)));
    const float Phi = Flap * FMath::DegreesToRadians(55.f);
    for (const float S : { -1.f, 1.f })
    {
        auto Wing = [&](float X, float Span)
        {
            const FVector Open(X, S * (6.f + Span * FMath::Cos(Phi)), 3.f + Span * FMath::Sin(Phi));
            const FVector Folded(X - Span * .72f, S * (5.f + Span * .08f), 5.5f + Span * .04f);
            return P(FMath::Lerp(Open, Folded, FMath::Clamp(Fold, 0.f, 1.f)));
        };
        const FVector A = Wing(9, 0), B = Wing(-6, 0), C = Wing(5, 22), D = Wing(-9, 21), E = Wing(-14, 46);
        W.Both(A, B, C); W.Both(B, D, C); W.Both(C, D, E);
    }
}

void Pigeons(FHallMesh& M)
{
    // A few pigeons that never leave the belfry sill.
    const struct { int32 F; float U, Yaw; } Spots[] = { { 0, -262.f, 20.f }, { 0, 150.f, -35.f }, { 1, -140.f, 10.f }, { 1, 268.f, 40.f }, { 3, 60.f, -15.f }, { 2, -250.f, 25.f } };
    for (const auto& Spot : Spots)
    {
        const FVector N = FaceN(Spot.F);
        const FRotator Facing = N.Rotation() + FRotator(0.f, Spot.Yaw, 0.f);
        TArray<FVector> V, Nrm; TArray<int32> I;
        AppendBird(FTransform(Facing, At(Spot.F, 314.f, Spot.U, HBelfry + 4.f), FVector(.7f, .78f, .8f)), 0.f, 1.f, V, Nrm, I);
        FHallSection& Sec = M.Sec(KPigeon);
        const int32 Base = Sec.V.Num();
        for (int32 K = 0; K < V.Num(); ++K) { Sec.V.Add(V[K]); Sec.N.Add(Nrm[K]); Sec.UV.Add(FVector2D::ZeroVector); Sec.T.Add(FProcMeshTangent(FVector::ForwardVector, false)); }
        for (const int32 Index : I) Sec.I.Add(Base + Index);
    }
}

void BuildAll(FHallMesh& Solid, FHallMesh& Upper, FHallMesh& Detail, FHallMesh& DialMesh)
{
    // Walk-in (the campus interiors' clock tower, HouseCampusInteriorsTower): the plates across the shaft become frames, the
    // door below its springing becomes a pair of leaves and the belfry floor gets the stair's slot; the outside is the same.
    // Off (-NoCampusInteriors, -NoCampusLayout, -NoClockTower): exactly the closed tower.
    const bool bOpen = HouseCampusInteriors::ClockTowerOpen();
    Plinth(Solid);
    BaseStage(Solid, Detail, bOpen);
    Shaft(Solid, bOpen);
    Belfry(Upper, bOpen);
    ClockStage(Upper);
    Spire(Upper);
    Dials(DialMesh);
    Ivy(Detail);
    Fixtures(Detail);
    Pigeons(Detail);
}

// ------------------------------------------------------------------ moving parts
TArray<FVector2D> Circle(float Radius, const FVector2D& Center, int32 Seg)
{
    TArray<FVector2D> Out;
    for (int32 K = 0; K < Seg; ++K) Out.Add(Center + FVector2D(FMath::Cos(2.f * PI * K / Seg), FMath::Sin(2.f * PI * K / Seg)) * Radius);
    return Out;
}
void HandPiece(FHallMesh& M, const TArray<FVector2D>& Outline, float Depth, float Lift)
{
    M.Prism(KIron, Outline, FVector(Lift, 0.f, 0.f), FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0), Depth, PlainUV);
}

void Lathe(FHallMesh& M, FName Key, const TArray<FVector2D>& Profile, int32 Seg)
{
    // Profile is (radius, z) from the top of the outside, round the lip, back up the inside.
    for (int32 J = 0; J + 1 < Profile.Num(); ++J)
    {
        const FVector2D A = Profile[J], B = Profile[J + 1], T = B - A;
        const FVector2D Out2 = FVector2D(-T.Y, T.X).GetSafeNormal();
        for (int32 K = 0; K < Seg; ++K)
        {
            const float A0 = 2.f * PI * K / Seg, A1 = 2.f * PI * (K + 1) / Seg;
            const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0.f), D1(FMath::Cos(A1), FMath::Sin(A1), 0.f);
            const FVector Nrm = ((D0 + D1).GetSafeNormal() * Out2.X + FVector::UpVector * Out2.Y).GetSafeNormal();
            if (A.X < .01f) Tri(M, Key, FVector(0, 0, A.Y), D1 * B.X + FVector(0, 0, B.Y), D0 * B.X + FVector(0, 0, B.Y), Nrm, PlainUV);
            else if (B.X < .01f) Tri(M, Key, D0 * A.X + FVector(0, 0, A.Y), D1 * A.X + FVector(0, 0, A.Y), FVector(0, 0, B.Y), Nrm, PlainUV);
            else M.Quad(Key, D0 * A.X + FVector(0, 0, A.Y), D1 * A.X + FVector(0, 0, A.Y), D1 * B.X + FVector(0, 0, B.Y), D0 * B.X + FVector(0, 0, B.Y), Nrm, PlainUV);
        }
    }
}
}

// ------------------------------------------------------------------ public
FVector HouseClockTowerMesh::FaceNormal(int32 Face) { return HouseClockTowerMeshImpl::FaceN(Face); }
FVector HouseClockTowerMesh::FaceRight(int32 Face) { return HouseClockTowerMeshImpl::FaceU(Face); }
FVector HouseClockTowerMesh::DialCenter(int32 Face) { return HouseClockTowerMeshImpl::At(Face, HouseClockTowerSite::ClockHalf, 0.f, HouseClockTowerSite::DialHeight); }
void HouseClockTowerMesh::CreateLook(UHouseBlackHallSubsystem* Look) { HouseClockTowerMeshImpl::CreateLook(Look); }
void HouseClockTowerMesh::Build(FHallMesh& Solid, FHallMesh& Upper, FHallMesh& Detail, FHallMesh& Dials) { HouseClockTowerMeshImpl::BuildAll(Solid, Upper, Detail, Dials); }

void HouseClockTowerMesh::ShaftLining(FHallMesh& M, FHallMesh& Panes, FName Key, FName RevealKey, FName Glass, FName GlassLit, FName DoorBack)
{
    namespace CT = HouseClockTowerMeshImpl;
    // The shell's own openings (BaseStage's door, Shaft's low lancet and pair), unfilled.
    CT::FOpening Door; Door.U = 0.f; Door.W = 95.f; Door.Sill = 75.f; Door.Spring = 330.f; Door.Sharp = .9f;
    CT::FOpening Low; Low.U = 0.f; Low.W = 32.f; Low.Sill = 880.f; Low.Spring = 1240.f; Low.Sharp = 1.5f;
    TArray<CT::FOpening> Lancets = { Low };
    for (const float U : { -62.f, 62.f })
    {
        CT::FOpening O; O.U = U; O.W = 26.f; O.Sill = 1680.f; O.Spring = 2060.f; O.Sharp = 1.5f;
        Lancets.Add(O);
    }
    for (int32 F = 0; F < NumFaces; ++F)
    {
        if (F == 0)
        {
            // The door wall: below the base course the lining is 12 thick, closing the gap to the door wall's inner face (270).
            CT::Wall(M, Key, F, 270.f, 258.f, 75.f, CT::HBaseTop, 12.f, { Door }, CT::BrickUV, RevealKey);
            CT::Wall(M, Key, F, 260.f, 258.f, CT::HBaseTop, CT::HBelfry, 2.f, Lancets, CT::BrickUV, RevealKey);
            // The tympanum over the leaves from inside: the shell's oak fill there (Dist 294) faces out only, so without this
            // the head of the doorway is a hole onto the plaza seen from within. A fan like the fill's, 0.2 cm inside it.
            const FVector Inward = -CT::FaceN(F);
            constexpr int32 Fan = 14;
            for (int32 K = 0; K < Fan; ++K)
            {
                const float Ua = -Door.W + 2.f * Door.W * K / Fan, Ub = -Door.W + 2.f * Door.W * (K + 1) / Fan;
                CT::Tri(M, DoorBack, CT::At(F, 293.8f, Door.U, Door.Spring), CT::At(F, 293.8f, Door.U + Ua, Door.Spring + CT::ArchRise(Ua, Door.W, Door.Sharp)),
                    CT::At(F, 293.8f, Door.U + Ub, Door.Spring + CT::ArchRise(Ub, Door.W, Door.Sharp)), Inward, CT::PlainUV);
            }
        }
        else CT::Wall(M, Key, F, 260.f, 258.f, 75.f, CT::HBelfry, 2.f, Lancets, CT::BrickUV, RevealKey);
        for (const CT::FOpening& O : Lancets)
        {
            // Lit as the shell's fills are (Shaft): the +X face's low lancet, and the left one of the door face's pair.
            const bool bLowOne = O.W > 30.f, bLit = bLowOne ? F == 1 : (F == 0 && O.U < 0.f);
            const float D = bLowOne ? 281.f : 283.f;   // 1 cm inside the fills (Dist 282 and 284)
            const FName PaneKey = bLit ? GlassLit : Glass;
            const FVector In = -CT::FaceN(F), Right = CT::FaceU(F);
            Panes.Quad(PaneKey, CT::At(F, D, O.U - O.W, O.Sill), CT::At(F, D, O.U + O.W, O.Sill), CT::At(F, D, O.U + O.W, O.Spring), CT::At(F, D, O.U - O.W, O.Spring), In, CT::PlainUV);
            // The reveal from the lining back to the glass (Dist 260..D), 0.2 cm inside the shell's own: that one is its
            // outside stone (CT_StoneLit round the low lancets, glowing with the uplight; CT_Stone round the pair), which the
            // pool does not light. Jambs, sill, and the arch's soffit with the pane's fan below.
            const float Jamb = O.W - .2f, SillZ = O.Sill + .2f;
            M.Quad(RevealKey, CT::At(F, 260.f, O.U - Jamb, SillZ), CT::At(F, D, O.U - Jamb, SillZ), CT::At(F, D, O.U - Jamb, O.Spring), CT::At(F, 260.f, O.U - Jamb, O.Spring), Right, CT::PlainUV);
            M.Quad(RevealKey, CT::At(F, 260.f, O.U + Jamb, SillZ), CT::At(F, D, O.U + Jamb, SillZ), CT::At(F, D, O.U + Jamb, O.Spring), CT::At(F, 260.f, O.U + Jamb, O.Spring), -Right, CT::PlainUV);
            M.Quad(RevealKey, CT::At(F, 260.f, O.U - Jamb, SillZ), CT::At(F, 260.f, O.U + Jamb, SillZ), CT::At(F, D, O.U + Jamb, SillZ), CT::At(F, D, O.U - Jamb, SillZ), FVector::UpVector, CT::PlainUV);
            constexpr int32 Steps = 14;
            for (int32 Step = 0; Step < Steps; ++Step)
            {
                const float Ua = -O.W + 2.f * O.W * Step / Steps, Ub = -O.W + 2.f * O.W * (Step + 1) / Steps;
                const float Za = O.Spring + CT::ArchRise(Ua, O.W, O.Sharp), Zb = O.Spring + CT::ArchRise(Ub, O.W, O.Sharp);
                CT::Tri(Panes, PaneKey, CT::At(F, D, O.U, O.Spring), CT::At(F, D, O.U + Ua, Za), CT::At(F, D, O.U + Ub, Zb), In, CT::PlainUV);
                FVector2D Down(Zb - Za, -(Ub - Ua));
                Down.Normalize();
                M.Quad(RevealKey, CT::At(F, 260.f, O.U + Ua, Za - .2f), CT::At(F, 260.f, O.U + Ub, Zb - .2f), CT::At(F, D, O.U + Ub, Zb - .2f), CT::At(F, D, O.U + Ua, Za - .2f),
                    (Right * Down.X + FVector::UpVector * Down.Y).GetSafeNormal(), CT::PlainUV);
            }
        }
    }
}

void HouseClockTowerMesh::HourHand(FHallMesh& M)
{
    // A spade hand with a counterweight, as on old turret clocks.
    HouseClockTowerMeshImpl::HandPiece(M, HouseClockTowerMeshImpl::Circle(11.f, FVector2D(0.f, -30.f), 16), 2.4f, 0.f);
    HouseClockTowerMeshImpl::HandPiece(M, { { -3.6f, -30.f }, { 3.6f, -30.f }, { 3.f, 78.f }, { -3.f, 78.f } }, 2.4f, 0.f);
    HouseClockTowerMeshImpl::HandPiece(M, { { 0.f, 68.f }, { 14.f, 80.f }, { 12.5f, 94.f }, { 0.f, 118.f }, { -12.5f, 94.f }, { -14.f, 80.f } }, 2.4f, 0.f);
    HouseClockTowerMeshImpl::HandPiece(M, HouseClockTowerMeshImpl::Circle(8.f, FVector2D::ZeroVector, 14), 4.f, 0.f);
}

void HouseClockTowerMesh::MinuteHand(FHallMesh& M)
{
    HouseClockTowerMeshImpl::HandPiece(M, HouseClockTowerMeshImpl::Circle(8.5f, FVector2D(0.f, -46.f), 14), 2.2f, 0.f);
    HouseClockTowerMeshImpl::HandPiece(M, { { -3.f, -46.f }, { 3.f, -46.f }, { 3.2f, 0.f }, { 2.2f, 150.f }, { -2.2f, 150.f }, { -3.2f, 0.f } }, 2.2f, 0.f);
    HouseClockTowerMeshImpl::HandPiece(M, { { 0.f, 54.f }, { 7.5f, 67.f }, { 0.f, 82.f }, { -7.5f, 67.f } }, 2.2f, 0.f);
    HouseClockTowerMeshImpl::HandPiece(M, { { -4.8f, 146.f }, { 4.8f, 146.f }, { 0.f, 174.f } }, 2.2f, 0.f);
    HouseClockTowerMeshImpl::HandPiece(M, HouseClockTowerMeshImpl::Circle(6.5f, FVector2D::ZeroVector, 14), 3.6f, 0.f);
}

FVector HouseClockTowerMesh::BellPivot(int32 Bell)
{
    // The hour bell hangs in the middle where the arches frame it; the quarter bells in the corners behind the lancets.
    static const FVector2D Corners[4] = { { 190.f, 190.f }, { -190.f, 190.f }, { -190.f, -190.f }, { 190.f, -190.f } };
    if (Bell >= 4) return HouseClockTowerSite::Center(3236.f);
    return HouseClockTowerSite::Center(3292.f) + FVector(Corners[Bell].X, Corners[Bell].Y, 0.f);
}

float HouseClockTowerMesh::BellDiameter(int32 Bell)
{
    static const float Sizes[5] = { 58.f, 66.f, 74.f, 92.f, 165.f };
    return Sizes[FMath::Clamp(Bell, 0, 4)];
}

void HouseClockTowerMesh::Bell(FHallMesh& M, float D)
{
    TArray<FVector2D> Profile;
    const float Outer[][2] = { { 0.f, -.06f }, { .17f, -.065f }, { .26f, -.10f }, { .29f, -.18f }, { .30f, -.30f }, { .33f, -.45f }, { .38f, -.58f }, { .45f, -.68f }, { .50f, -.75f }, { .505f, -.78f },
        { .48f, -.80f }, { .43f, -.77f }, { .38f, -.70f }, { .31f, -.58f }, { .26f, -.42f }, { .24f, -.26f }, { .20f, -.15f }, { 0.f, -.13f } };
    for (const auto& P : Outer) Profile.Add(FVector2D(P[0] * D, P[1] * D));
    HouseClockTowerMeshImpl::Lathe(M, HouseClockTowerMeshImpl::KBronze, Profile, 28);
    // Crown and the oak headstock on its gudgeons (the swing axis is X).
    M.BoxMinMax(HouseClockTowerMeshImpl::KBronze, FVector(-.05f * D, -.05f * D, -.07f * D), FVector(.05f * D, .05f * D, -.01f * D), HouseClockTowerMeshImpl::PlainUV);
    M.BoxMinMax(HouseClockTowerMeshImpl::KOak, FVector(-.56f * D, -.08f * D, -.02f * D), FVector(.56f * D, .08f * D, .11f * D), HouseClockTowerMeshImpl::PlainUV);
    M.Cylinder(HouseClockTowerMeshImpl::KIron, FVector(-.66f * D, 0.f, 0.f), FVector(.66f * D, 0.f, 0.f), FMath::Max(1.5f, .025f * D), 8, HouseClockTowerMeshImpl::PlainUV);
}

void HouseClockTowerMesh::Clapper(FHallMesh& M, float D)
{
    M.Cylinder(HouseClockTowerMeshImpl::KIron, FVector(0, 0, -.12f * D), FVector(0, 0, -.66f * D), FMath::Max(1.2f, .022f * D), 8, HouseClockTowerMeshImpl::PlainUV);
    HouseClockTowerMeshImpl::Ball(M, HouseClockTowerMeshImpl::KIron, FVector(0, 0, -.68f * D), .075f * D, 10);
    M.Cylinder(HouseClockTowerMeshImpl::KIron, FVector(0, 0, -.70f * D), FVector(0, 0, -.79f * D), FMath::Max(1.f, .03f * D), 8, HouseClockTowerMeshImpl::PlainUV);
}

void HouseClockTowerMesh::HammerPlacement(int32 Bell, FVector& OutPivot, FVector& OutAway)
{
    // The hour bell's hammer works on its -Y side, so it swings across the belfry view (which looks in from +X); the
    // quarter bells' hammers face the middle of the frame.
    const float D = BellDiameter(Bell);
    FVector Inward = HouseClockTowerSite::Center(0.f) - BellPivot(Bell); Inward.Z = 0.f;
    OutAway = Bell >= 4 ? FVector(0.f, -1.f, 0.f) : Inward.GetSafeNormal();
    const FVector SoundBow = BellPivot(Bell) + OutAway * (.5f * D) - FVector(0.f, 0.f, .75f * D);
    OutPivot = SoundBow + OutAway * (.18f * D + 12.f) + FVector(0.f, 0.f, .28f * D + 14.f);
}

void HouseClockTowerMesh::Hammer(FHallMesh& M, float D)
{
    const float Out = .18f * D + 12.f, Drop = .28f * D + 14.f, Head = .045f * D + 2.f;
    const FVector HeadAt(-Out + Head, 0.f, -Drop);
    HouseClockTowerMeshImpl::Bar(M, HouseClockTowerMeshImpl::KIron, FVector(0.f, 0.f, 3.f), HeadAt, .03f * D + 1.5f, .03f * D + 1.5f, FVector(0.f, 1.f, 0.f));
    M.Box(HouseClockTowerMeshImpl::KIron, FTransform(HeadAt), FVector(Head, Head * 1.1f, Head * 1.25f), HouseClockTowerMeshImpl::PlainUV);
    M.Cylinder(HouseClockTowerMeshImpl::KIron, FVector(0.f, -Head * 1.4f, 0.f), FVector(0.f, Head * 1.4f, 0.f), .025f * D + 1.5f, 8, HouseClockTowerMeshImpl::PlainUV);
}

FVector HouseClockTowerMesh::VanePivot() { return HouseClockTowerSite::Center(HouseClockTowerMeshImpl::HApex + 214.f); }

void HouseClockTowerMesh::Vane(FHallMesh& M)
{
    // A gilt arrow with a scrolled tail; its head points where the wind blows from.
    HouseClockTowerMeshImpl::Bar(M, HouseClockTowerMeshImpl::KGilt, FVector(-84.f, 0, 0), FVector(72.f, 0, 0), 2.6f, 2.6f, FVector::UpVector);
    M.Prism(HouseClockTowerMeshImpl::KGilt, { { 60.f, -13.f }, { 100.f, 0.f }, { 60.f, 13.f } }, FVector(0, -.7f, 0), FVector(1, 0, 0), FVector(0, 0, 1), FVector(0, 1, 0), 1.4f, HouseClockTowerMeshImpl::PlainUV);
    M.Prism(HouseClockTowerMeshImpl::KGilt, { { -100.f, -22.f }, { -64.f, -9.f }, { -64.f, 9.f }, { -100.f, 22.f } }, FVector(0, -.6f, 0), FVector(1, 0, 0), FVector(0, 0, 1), FVector(0, 1, 0), 1.2f, HouseClockTowerMeshImpl::PlainUV);
    M.Cylinder(HouseClockTowerMeshImpl::KGilt, FVector(0, 0, -9.f), FVector(0, 0, 9.f), 6.f, 10, HouseClockTowerMeshImpl::PlainUV);
}

void HouseClockTowerMesh::CrowPerches(TArray<FTransform>& Out)
{
    // On the merlons and the pinnacle bases, facing out with a little variety.
    FRandomStream R(7331);
    for (int32 F = 0; F < 4; ++F)
        for (const int32 Merlon : { 1, 4, 8 })
        {
            const float U = -294.f + 65.3f * Merlon;
            const FVector P = HouseClockTowerMeshImpl::At(F, 318.f, U, HouseClockTowerMeshImpl::HMerlon + 8.f);
            // The corner merlons' crows turn away from their own pinnacle, so open wings never clip it.
            float Yaw = R.FRandRange(-20.f, 20.f);
            if (Merlon == 1) Yaw = -FMath::Abs(Yaw);
            else if (Merlon == 8) Yaw = FMath::Abs(Yaw);
            const FRotator Facing = HouseClockTowerMeshImpl::FaceN(F).Rotation() + FRotator(0.f, Yaw, 0.f);
            Out.Add(FTransform(Facing, P + FVector(0, 0, 5.5f)));
        }
}

void HouseClockTowerMesh::AppendCrow(const FTransform& Pose, float Flap, float Fold, TArray<FVector>& V, TArray<FVector>& N, TArray<int32>& I)
{
    HouseClockTowerMeshImpl::AppendBird(Pose, Flap, Fold, V, N, I);
}

float HouseClockTowerSite::Lift() { return HouseCalderTerrain::TowerLift(); }
