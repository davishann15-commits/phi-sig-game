// Calder State's 1986 street details (see HouseCampusStreetDetail.h).
//
// Lanes follow right-hand traffic in the engine's frame: a driver heading +X has +Y on the right, heading +Y has -X on
// the right. So Calder Dr's northbound (outbound) lane is its west half, and a driver's sign stands on that driver's right.
//
// On the Calder terrain (SourceArt/CalderLandscape, step 1c) the paint (the stop bar, the bus bay's yellow kerb) drapes over
// the road and the kerb where the relief under it is not flat, and is built as before where it is (Dormant: at zero relief,
// everywhere). Inlets and manholes stay small flat plates at their centre, turned to the road's slope there. Where the road or
// lot under them is built flat instead (HouseCampusLayout::FlatPaveTop), the stop bar, inlets and manholes lie level on it.
#include "HouseCampusStreetDetail.h"
#include "HouseCampusLayout.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCalderDrape.h"
#include "HouseCalderTerrain.h"
#include "HouseCampusFallKit.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseSurroundingsPolish.h"
#include "Engine/Texture2D.h"

namespace HouseCampusStreetDetailImpl
{
// Today's grade: only the signal poles on Fraternity Park Dr's kerbs (a frozen zone) still stand on it as a literal.
constexpr float Ground = -60.f;
const FName Metal(TEXT("EX_Metal")), Back(TEXT("CS_SignBack")), Housing(TEXT("CS_SignalHousing")), Dark(TEXT("EX_Dark"));

/** The ground (a verge, an island, a lawn) at P, and the top of the road and of the walk there (the Calder terrain: today
 *  -60, -72 and -57 everywhere on the campus). */
float GroundAt(const FVector2D& P) { return HouseExteriorSite::GroundHeight(P.X, P.Y); }
float RoadTopAt(const FVector2D& P) { return HouseCalderTerrain::SurfaceZ(P.X, P.Y, HouseCalderTerrain::EPave::Road); }
float WalkTopAt(const FVector2D& P) { return HouseCalderTerrain::SurfaceZ(P.X, P.Y, HouseCalderTerrain::EPave::Walk); }

/** Step 1c's gate: a piece keeps today's build where the relief under it and one lattice cell round it is 0, where its
 *  footprint straddles a frozen zone's edge (its frozen part never moves; the same test as HouseCampusLayout's) or lies
 *  wholly in one (today's ground there); only elsewhere is it draped. At zero relief nothing is ever draped. */
bool Dormant(const FBox2D& Box)
{
    if (HouseCalderTerrain::ReliefFlat(Box.ExpandBy(HouseCalderTerrain::Cell)) || HouseCalderTerrain::BoxFrozen(Box)) return true;
    const FVector2D C = Box.GetCenter();
    const FVector2D Points[] = { Box.Min, FVector2D(C.X, Box.Min.Y), FVector2D(Box.Max.X, Box.Min.Y), FVector2D(Box.Min.X, C.Y), C,
                                 FVector2D(Box.Max.X, C.Y), FVector2D(Box.Min.X, Box.Max.Y), FVector2D(C.X, Box.Max.Y), Box.Max };
    int32 In = 0, Out = 0;
    for (const FVector2D& P : Points) (HouseCalderTerrain::InFrozen(P.X, P.Y) ? In : Out)++;
    return In > 0 && Out > 0;
}

// ------------------------------------------------------------------ painted sign faces
struct FLine { const TCHAR* Text; float Height, Y; FLinearColor Ink; };
/** A sign face: background, a border inset from the edge, lines of lettering (Height and Y as fractions of the face), an
 *  optional arrow (Arrow = -1 left, +1 right, 0 none) along the bottom. */
void Face(UHouseBlackHallSubsystem* Look, FName Key, int32 W, int32 H, const FLinearColor& Bg, const FLinearColor& Border, std::initializer_list<FLine> Lines, int32 Arrow = 0)
{
    FHallImage Img;
    Img.Init(W, H, Bg);
    const float B = FMath::Max(3.f, W * .02f);
    Img.Rect(FVector2D(B, B), FVector2D(W - B, H - B), Border);
    Img.Rect(FVector2D(B * 2.f, B * 2.f), FVector2D(W - B * 2.f, H - B * 2.f), Bg);
    for (const FLine& L : Lines)
    {
        FHallTextStyle S;
        S.Height = H * L.Height; S.Stroke = S.Height * .14f; S.Spacing = .18f;
        S.WidthScale = FMath::Min(1.f, (W - B * 6.f) / HallTextWidth(L.Text, S));
        HallDrawText(Img, L.Text, FVector2D((W - HallTextWidth(L.Text, S)) * .5f, H * L.Y), S, L.Ink);
    }
    if (Arrow != 0)
    {
        const float Y = H * .82f, X0 = W * .28f, X1 = W * .72f, T = H * .05f;
        Img.Rect(FVector2D(X0, Y - T), FVector2D(X1, Y + T), Border);
        const float Tip = Arrow > 0 ? X1 + H * .1f : X0 - H * .1f, Root = Arrow > 0 ? X1 - H * .02f : X0 + H * .02f;
        for (int32 Py = int32(Y - H * .1f); Py <= int32(Y + H * .1f); ++Py)
        {
            const float Frac = 1.f - FMath::Abs(Py - Y) / (H * .1f);
            const float Lo = FMath::Min(Root, FMath::Lerp(Root, Tip, Frac)), Hi = FMath::Max(Root, FMath::Lerp(Root, Tip, Frac));
            for (int32 Px = int32(Lo); Px <= int32(Hi); ++Px) if (Px >= 0 && Px < W && Py >= 0 && Py < H) Img.At(Px, Py) = Border;
        }
    }
    UTexture2D* Tex = HallMakeTexture(Img, true, false, *Key.ToString(), false);
    if (Tex) Look->Textures.Add(Key, Tex);
    HallMakeTextured(Look, Key, Tex, nullptr, .45f, FLinearColor::White, 0.f, 0.f, Bg * .02f);
}

// ------------------------------------------------------------------ geometry
/** A galvanized square sign post from below the ground at At to Top (world Z). */
void Post(FHallMesh& M, const FVector2D& At, float Top, float Half = 3.f)
{
    M.BoxMinMax(Metal, FVector(At.X - Half, At.Y - Half, GroundAt(At) - 20.f), FVector(At.X + Half, At.Y + Half, Top), HouseCampusFallKit::PlainUV());
}
/** A sign panel on a post: Face on the front (N), plain backing behind; Right is the reader's right. */
void Panel(FHallMesh& M, FName Key, const FVector& C, const FVector& N, const FVector& Right, float W, float H)
{
    HouseCampusFallKit::Card(M, Key, C + N * .6f, Right, FVector::UpVector, W, H, N);
    M.Box(Back, FTransform(FRotationMatrix::MakeFromXZ(N, FVector::UpVector).Rotator(), C - N * .4f), FVector(.5f, W * .5f, H * .5f), HouseCampusFallKit::PlainUV());
}
FVector RightOf(const FVector& N) { return FVector::CrossProduct(FVector::UpVector, -N).GetSafeNormal(); }   // the reader faces -N
/** A post with one panel at eye height (its centre CentreAbove the ground at At), facing N. */
void PostSign(FHallMesh& M, FName Key, const FVector2D& At, const FVector& N, float W, float H, float CentreAbove = 210.f)
{
    const float CentreZ = GroundAt(At) + CentreAbove;
    Post(M, At, CentreZ + H * .5f - 4.f);
    Panel(M, Key, FVector(At, CentreZ) + N * 4.f, N, RightOf(N), W, H);
}
/** The octagonal STOP sign (the street's own EX_StopSign face when it exists, so both read alike). */
void StopSign(FHallMesh& M, FName Key, const FVector2D& At, const FVector& N)
{
    const float Z = GroundAt(At) + 215.f;
    Post(M, At, Z + 30.f);
    const FVector C = FVector(At, Z) + N * 4.6f, Right = RightOf(N);
    FHallSection& S = M.Sec(Key);
    const int32 Base = S.V.Num();
    S.V.Add(C); S.N.Add(N); S.UV.Add(FVector2D(.5f, .5f)); S.T.Add(FProcMeshTangent(Right, false));
    for (int32 I = 0; I < 8; ++I)
    {
        const float A = FMath::DegreesToRadians(22.5f + 45.f * I);
        const FVector2D Q(FMath::Cos(A) * 37.f, FMath::Sin(A) * 37.f);
        S.V.Add(C + Right * Q.X + FVector::UpVector * Q.Y); S.N.Add(N); S.UV.Add(FVector2D(.5f + Q.X / 80.f, .5f - Q.Y / 80.f)); S.T.Add(FProcMeshTangent(Right, false));
    }
    for (int32 I = 0; I < 8; ++I)
    {
        const int32 A = Base + 1 + I, B = Base + 1 + (I + 1) % 8;
        if (FVector::DotProduct(FVector::CrossProduct(S.V[A] - C, S.V[B] - C), N) < 0) S.I.Append({ Base, A, B }); else S.I.Append({ Base, B, A });
    }
    M.Box(Back, FTransform(FRotationMatrix::MakeFromXZ(N, FVector::UpVector).Rotator(), C - N * 1.f), FVector(.5f, 36.f, 36.f), HouseCampusFallKit::PlainUV());
}
/** A two-sided green street-name blade on a post; Along is the direction of the street it names. */
void Blade(FHallMesh& M, FName Key, const FVector2D& At, const FVector& Along, float Z)
{
    const FVector N = FVector::CrossProduct(FVector::UpVector, Along).GetSafeNormal();
    for (const float S : { 1.f, -1.f }) HouseCampusFallKit::Card(M, Key, FVector(At, Z) + N * (S * 1.2f), Along * S, FVector::UpVector, 150.f, 22.f, N * S);
    M.Box(Back, FTransform(FRotationMatrix::MakeFromXZ(N, FVector::UpVector).Rotator(), FVector(At, Z)), FVector(1.f, 75.f, 11.f), HouseCampusFallKit::PlainUV());
}
/** A 1980s three-section signal head (red over amber over green) facing N, its top at Top; Lit 0 red, 1 amber, 2 green. */
void SignalHead(FHallMesh& M, const FVector& Top, const FVector& N, int32 Lit)
{
    const FRotator R = FRotationMatrix::MakeFromXZ(N, FVector::UpVector).Rotator();
    M.Box(Housing, FTransform(R, Top - FVector(0, 0, 52.f)), FVector(14.f, 17.f, 52.f), HouseCampusFallKit::PlainUV());
    M.Box(Dark, FTransform(R, Top - FVector(0, 0, 52.f) - N * 14.5f), FVector(.6f, 25.f, 58.f), HouseCampusFallKit::PlainUV());   // backplate
    for (int32 K = 0; K < 3; ++K)
    {
        const FVector C = Top - FVector(0, 0, 18.f + 34.f * K) + N * 14.5f;
        const FName Lens = K == Lit ? FName(*FString::Printf(TEXT("CS_LensOn%d"), K)) : FName(*FString::Printf(TEXT("CS_LensOff%d"), K));
        M.Cylinder(Lens, C, C + N * 1.f, 11.f, 16, HouseCampusFallKit::PlainUV());
        // Tunnel visor over each lens.
        M.Box(Housing, FTransform(R, C + N * 8.f + FVector(0, 0, 10.f)), FVector(8.f, 13.f, 1.2f), HouseCampusFallKit::PlainUV());
    }
}
/** A lettered pedestrian head (DON'T WALK lit in orange, WALK dark) facing N. */
void PedHead(FHallMesh& M, const FVector& C, const FVector& N)
{
    const FRotator R = FRotationMatrix::MakeFromXZ(N, FVector::UpVector).Rotator();
    M.Box(Housing, FTransform(R, C), FVector(12.f, 22.f, 22.f), HouseCampusFallKit::PlainUV());
    M.Box(Dark, FTransform(R, C + N * 12.2f), FVector(.3f, 19.f, 19.f), HouseCampusFallKit::PlainUV());
    const FVector Right = RightOf(N), Face = C + N * 12.7f;
    HouseCampusFallKit::GlyphStrips(M, TEXT("CS_DontWalkLit"), TEXT("DON'T"), Face + FVector(0, 0, 3.f) - Right * 16.f, Right, FVector::UpVector, N, 8.f, 1.4f);
    // The font has no apostrophe: its tick, in the fourth of DON'T's places.
    const float U = 8.f / 6.f;
    const FVector Tick = Face + FVector(0, 0, 3.f) - Right * 16.f + Right * (15.f * U);
    HouseCampusFallKit::Bar(M, TEXT("CS_DontWalkLit"), Tick + Right * (2.1f * U) + FVector(0, 0, 6.2f * U), Tick + Right * (1.6f * U) + FVector(0, 0, 4.4f * U), 1.4f, .2f, N);
    HouseCampusFallKit::GlyphStrips(M, TEXT("CS_DontWalkLit"), TEXT("WALK"), Face + FVector(0, 0, -9.f) - Right * 12.6f, Right, FVector::UpVector, N, 8.f, 1.4f);
}
/** A 1980s coin parking meter on the kerb: post, head with a domed top and a dark window. */
void Meter(FHallMesh& M, const FVector2D& At, const FVector& N)
{
    const FRotator R = FRotationMatrix::MakeFromXZ(N, FVector::UpVector).Rotator();
    const float G0 = GroundAt(At);
    M.Cylinder(Metal, FVector(At, WalkTopAt(At) - 10.f), FVector(At, G0 + 108.f), 3.f, 8, HouseCampusFallKit::PlainUV());
    const FVector H(At, G0 + 125.f);
    M.Box(TEXT("CS_MeterBody"), FTransform(R, H), FVector(9.f, 9.f, 17.f), HouseCampusFallKit::PlainUV());
    M.Dome(TEXT("CS_MeterBody"), H + FVector(0, 0, 17.f), FVector::UpVector, 9.f, 8.f, 10);
    M.Box(Dark, FTransform(R, H + N * 9.2f + FVector(0, 0, 4.f)), FVector(.4f, 6.f, 6.f), HouseCampusFallKit::PlainUV());
}
/** A storm inlet in the gutter, flat on the road's top at its centre. Over the relief (not Dormant) it is still one small flat
 *  plate at its centre, turned to the road's slope there (level, a 1 m inlet on a 12% grade would stand 6 cm proud at one
 *  end and sink 6 cm at the other), except where the road under it is built flat (HouseCampusLayout::FlatPaveTop: a lot, or
 *  Calder Dr and the west road, kept at today's height across the frozen street's edge): level on that top. */
void Grate(FHallMesh& Flat, const FVector2D& C, const FVector2D& Size)
{
    float Z = RoadTopAt(C);
    const bool bAlongX = Size.X > Size.Y;
    if (Dormant(FBox2D(C - Size * .5f, C + Size * .5f)) || HouseCampusLayout::FlatPaveTop(C, Z))
    {
        Flat.BoxMinMax(Metal, FVector(C.X - Size.X * .5f, C.Y - Size.Y * .5f, Z), FVector(C.X + Size.X * .5f, C.Y + Size.Y * .5f, Z + .4f), HouseCampusFallKit::PlainUV());
        for (float T = -.4f; T <= .41f; T += .1f)
        {
            const FVector2D P = C + (bAlongX ? FVector2D(Size.X * T, 0.f) : FVector2D(0.f, Size.Y * T));
            const FVector2D H = bAlongX ? FVector2D(1.5f, Size.Y * .4f) : FVector2D(Size.X * .4f, 1.5f);
            Flat.BoxMinMax(Dark, FVector(P.X - H.X, P.Y - H.Y, Z + .4f), FVector(P.X + H.X, P.Y + H.Y, Z + .5f), HouseCampusFallKit::PlainUV());
        }
        return;
    }
    const FQuat Q = FQuat::FindBetweenNormals(FVector::UpVector, HouseExteriorSite::GroundNormal(C.X, C.Y));
    const FVector O(C, Z);
    Flat.Box(Metal, FTransform(Q, O + Q.RotateVector(FVector(0.f, 0.f, .2f))), FVector(Size * .5f, .2f), HouseCampusFallKit::PlainUV());
    for (float T = -.4f; T <= .41f; T += .1f)
    {
        const FVector2D Off = bAlongX ? FVector2D(Size.X * T, 0.f) : FVector2D(0.f, Size.Y * T);
        const FVector2D H = bAlongX ? FVector2D(1.5f, Size.Y * .4f) : FVector2D(Size.X * .4f, 1.5f);
        Flat.Box(Dark, FTransform(Q, O + Q.RotateVector(FVector(Off, .45f))), FVector(H, .05f), HouseCampusFallKit::PlainUV());
    }
}
/** A manhole cover in a lane, flat on the road's top at its centre; over the relief, turned to the road's slope there, except
 *  where the road or lot under it is built flat (HouseCampusLayout::FlatPaveTop): level on that top. */
void Manhole(FHallMesh& Flat, const FVector2D& C)
{
    float Z = RoadTopAt(C);
    if (Dormant(FBox2D(C - FVector2D(33.f, 33.f), C + FVector2D(33.f, 33.f))) || HouseCampusLayout::FlatPaveTop(C, Z))
    {
        Flat.Cylinder(Metal, FVector(C, Z), FVector(C, Z + .5f), 33.f, 20, HouseCampusFallKit::PlainUV());
        for (float D = -24.f; D <= 24.f; D += 8.f) Flat.BoxMinMax(Dark, FVector(C.X - 26.f, C.Y + D - 1.f, Z + .5f), FVector(C.X + 26.f, C.Y + D + 1.f, Z + .6f), HouseCampusFallKit::PlainUV());
        return;
    }
    const FVector N = HouseExteriorSite::GroundNormal(C.X, C.Y), O(C, Z);
    const FQuat Q = FQuat::FindBetweenNormals(FVector::UpVector, N);
    Flat.Cylinder(Metal, O, O + N * .5f, 33.f, 20, HouseCampusFallKit::PlainUV());
    for (float D = -24.f; D <= 24.f; D += 8.f) Flat.Box(Dark, FTransform(Q, O + Q.RotateVector(FVector(0.f, D, .55f))), FVector(26.f, 1.f, .05f), HouseCampusFallKit::PlainUV());
}
}

void HouseCampusStreetDetail::CreateLook(UHouseBlackHallSubsystem* Look)
{
    namespace SD = HouseCampusStreetDetailImpl;
    if (!Look || !HouseCampusLayout::Enabled() || Look->Materials.Contains(TEXT("CS_SpeedLimit25"))) return;
    const FLinearColor White(.82f, .83f, .78f), Black(.02f, .02f, .02f), Green(.012f, .09f, .045f), Red(.43f, .013f, .009f), Ink(.86f, .87f, .79f);
    SD::Face(Look, TEXT("CS_SpeedLimit25"), 256, 320, White, Black, { { TEXT("SPEED"), .15f, .25f, Black }, { TEXT("LIMIT"), .15f, .45f, Black }, { TEXT("25"), .36f, .88f, Black } });
    SD::Face(Look, TEXT("CS_OneWay"), 512, 170, Black, White, { { TEXT("ONE WAY"), .32f, .55f, White } }, 1);
    SD::Face(Look, TEXT("CS_StadiumParking"), 512, 256, Green, Ink, { { TEXT("STADIUM"), .22f, .32f, Ink }, { TEXT("PARKING"), .22f, .62f, Ink } }, -1);
    SD::Face(Look, TEXT("CS_VisitorParking"), 512, 256, Green, Ink, { { TEXT("VISITOR PARKING"), .17f, .3f, Ink }, { TEXT("HEALTH CENTER"), .14f, .58f, Ink } }, 1);
    SD::Face(Look, TEXT("CS_PermitParking"), 384, 384, White, Red, { { TEXT("PERMIT"), .14f, .26f, Red }, { TEXT("PARKING"), .14f, .45f, Red }, { TEXT("ONLY"), .14f, .64f, Red }, { TEXT("TOW-AWAY ZONE"), .07f, .86f, Black } });
    SD::Face(Look, TEXT("CS_OverflowParking"), 512, 256, Green, Ink, { { TEXT("OVERFLOW"), .22f, .32f, Ink }, { TEXT("PARKING"), .22f, .62f, Ink } });
    SD::Face(Look, TEXT("CS_BladeFounders"), 1024, 150, Green, Ink, { { TEXT("FOUNDERS WAY"), .5f, .78f, Ink } });
    SD::Face(Look, TEXT("CS_BladeFratPark"), 1024, 150, Green, Ink, { { TEXT("FRATERNITY PARK DR"), .5f, .78f, Ink } });
    if (!Look->Materials.Contains(TEXT("EX_StopSign")))
        SD::Face(Look, TEXT("CS_Stop"), 256, 256, Red, Ink, { { TEXT("STOP"), .3f, .66f, Ink } });
    HallMakePlain(Look, TEXT("CS_SignBack"), FLinearColor(.18f, .19f, .19f), .55f, .6f);
    HallMakePlain(Look, TEXT("CS_SignalHousing"), FLinearColor(.5f, .38f, .03f), .6f);      // federal yellow, weathered
    HallMakePlain(Look, TEXT("CS_MeterBody"), FLinearColor(.16f, .17f, .16f), .4f, .7f);
    HallMakePlain(Look, TEXT("CS_CurbYellow"), FLinearColor(.62f, .45f, .04f), .8f);
    // Lenses: lit ones glow by emission only (no lights); dark ones are deep glass.
    HallMakePlain(Look, TEXT("CS_LensOn0"), FLinearColor(1.f, .1f, .05f), .2f, 0.f, FLinearColor(28.f, 2.f, .6f));
    HallMakePlain(Look, TEXT("CS_LensOn1"), FLinearColor(1.f, .6f, .1f), .2f, 0.f, FLinearColor(24.f, 11.f, 1.f));
    HallMakePlain(Look, TEXT("CS_LensOn2"), FLinearColor(.2f, 1.f, .7f), .2f, 0.f, FLinearColor(3.f, 22.f, 13.f));
    HallMakePlain(Look, TEXT("CS_LensOff0"), FLinearColor(.12f, .015f, .01f), .15f);
    HallMakePlain(Look, TEXT("CS_LensOff1"), FLinearColor(.13f, .07f, .01f), .15f);
    HallMakePlain(Look, TEXT("CS_LensOff2"), FLinearColor(.01f, .07f, .05f), .15f);
    HallMakePlain(Look, TEXT("CS_DontWalkLit"), FLinearColor(1.f, .5f, .15f), .3f, 0.f, FLinearColor(20.f, 7.f, 1.5f));
}

void HouseCampusStreetDetail::Build(FHallMesh& Solid, FHallMesh& Flat)
{
    namespace SD = HouseCampusStreetDetailImpl;
    if (!HouseCampusLayout::Enabled()) return;
    const FVector North(0, 1, 0), South(0, -1, 0), East(1, 0, 0), West(-1, 0, 0);

    // Calder Dr: STOP and a stop bar for traffic leaving campus (the west half), SPEED LIMIT 25 for traffic coming in.
    SD::StopSign(Solid, HouseSurroundings::Enabled() ? FName(TEXT("EX_StopSign")) : FName(TEXT("CS_Stop")), FVector2D(8670.f, -5950.f), South);
    {
        // The stop bar is flat on the road's top at its middle; over the relief (not Dormant) its paint drapes on the road, unless
        // the road is built flat there (Calder Dr straddles the frozen street's edge: HouseCampusLayout::FlatPaveTop).
        const FBox2D StopBar(FVector2D(8700.f, -5980.f), FVector2D(9480.f, -5920.f));
        float StopZ = SD::RoadTopAt(FVector2D(9090.f, -5950.f));
        if (SD::Dormant(StopBar) || HouseCampusLayout::FlatPaveTop(StopBar.GetCenter(), StopZ))
            Flat.BoxMinMax(TEXT("EX_Line"), FVector(8700.f, -5980.f, StopZ + .5f), FVector(9480.f, -5920.f, StopZ + .7f), HouseCampusFallKit::PlainUV());
        else
            HouseCalderDrape::Sheet(Flat, TEXT("EX_Line"), HouseCalderDrape::RectOutline(StopBar), [](float Sx, float Sy) { return SD::RoadTopAt(FVector2D(Sx, Sy)) + .7f; }, HouseCampusFallKit::PlainUV());
    }
    SD::PostSign(Solid, TEXT("CS_SpeedLimit25"), FVector2D(10620.f, -7300.f), North, 61.f, 76.f);
    // Stadium Dr westbound (its right side is the south walk) and Founders Way southbound (its right side is the east verge).
    SD::PostSign(Solid, TEXT("CS_SpeedLimit25"), FVector2D(6600.f, -20100.f), East, 61.f, 76.f);
    SD::PostSign(Solid, TEXT("CS_SpeedLimit25"), FVector2D(-19980.f, -6800.f), North, 61.f, 76.f);
    // The bus loop's island: ONE WAY for the cars arriving along Stadium Dr; keep the island on your left.
    {
        const FVector2D Island(-700.f, -19300.f);
        const float IslandZ = SD::GroundAt(Island);
        SD::Post(Solid, Island, IslandZ + 210.f);
        SD::Panel(Solid, TEXT("CS_OneWay"), FVector(-696.f, -19300.f, IslandZ + 190.f), East, SD::RightOf(East), 91.f, 30.f);
    }
    // Street-name blades where Founders Way meets Fraternity Park Dr (on the campus corner).
    {
        const FVector2D Corner(-20000.f, -5900.f);
        const float CornerZ = SD::GroundAt(Corner);
        SD::Post(Solid, Corner, CornerZ + 330.f);
        SD::Blade(Solid, TEXT("CS_BladeFounders"), Corner, FVector(0, 1, 0), CornerZ + 300.f);
        SD::Blade(Solid, TEXT("CS_BladeFratPark"), Corner, FVector(1, 0, 0), CornerZ + 324.f);
    }
    // Parking signs at each lot's way in.
    SD::PostSign(Solid, TEXT("CS_StadiumParking"), FVector2D(4700.f, -20100.f), North, 120.f, 60.f, 190.f);
    SD::PostSign(Solid, TEXT("CS_VisitorParking"), FVector2D(-5420.f, -5930.f), North, 120.f, 60.f, 190.f);
    SD::PostSign(Solid, TEXT("CS_PermitParking"), FVector2D(14880.f, -11300.f), West, 46.f, 46.f, 200.f);
    SD::PostSign(Solid, TEXT("CS_OverflowParking"), FVector2D(4380.f, -26470.f), North, 120.f, 60.f, 190.f);

    // Coin meters at the visitor lot's stall heads, on the curbs.
    for (const float X : { -6265.f, -5725.f }) SD::Meter(Solid, FVector2D(X, -6685.f), South);
    for (const float X : { -6265.f, -5725.f, -4645.f }) SD::Meter(Solid, FVector2D(X, -8515.f), North);

    // The signalised mid-block crossing on the stadium axis: a pole on the campus kerb with a mast arm over both lanes,
    // a head for each direction showing green (emission only), a lettered DON'T WALK head and a push button.
    {
        const FVector2D Pole(2720.f, -5660.f);
        Solid.Cylinder(SD::Metal, FVector(Pole, SD::Ground - 20.f), FVector(Pole, SD::Ground + 640.f), 10.f, 12, HouseCampusFallKit::PlainUV());
        Solid.Cylinder(SD::Metal, FVector(Pole, SD::Ground + 620.f), FVector(Pole.X, -4380.f, SD::Ground + 620.f), 5.f, 10, HouseCampusFallKit::PlainUV());
        HouseCampusFallKit::Bar(Solid, SD::Metal, FVector(Pole, SD::Ground + 520.f), FVector(Pole.X, -5200.f, SD::Ground + 618.f), 3.f, 3.f, West);   // brace
        SD::SignalHead(Solid, FVector(Pole.X, -5150.f, SD::Ground + 612.f), East, 2);   // westbound traffic (the campus half) looks this way
        SD::SignalHead(Solid, FVector(Pole.X, -4560.f, SD::Ground + 612.f), West, 2);   // eastbound
        SD::PedHead(Solid, FVector(Pole, SD::Ground + 300.f) + North * 12.f, North);
        Solid.BoxMinMax(TEXT("CS_SignalHousing"), FVector(Pole.X - 22.f, Pole.Y - 6.f, SD::Ground + 105.f), FVector(Pole.X - 10.f, Pole.Y + 6.f, SD::Ground + 125.f), HouseCampusFallKit::PlainUV());
        Solid.Cylinder(SD::Metal, FVector(Pole.X - 22.f, Pole.Y, SD::Ground + 115.f), FVector(Pole.X - 25.f, Pole.Y, SD::Ground + 115.f), 3.f, 8, HouseCampusFallKit::PlainUV());
        // Its partner on the house kerb, in line with it: a pedestal pole on the hydrant's line, 75 cm behind the
        // near kerb face (-4170; the walk runs back to -3900), so the sidewalk's through path stays clear, and east
        // of the curb ramp (X 2250..2550, Y -4320..-4170). Its lettered head faces south, for the people waiting on
        // the campus kerb, as the campus head faces north for the house side; its push button faces west, toward the
        // ramp's landing and the front walk, as the campus pole's does.
        const FVector2D HousePole(Pole.X, -4095.f);
        constexpr float HousePoleR = 7.f;
        Solid.Cylinder(SD::Metal, FVector(HousePole, SD::Ground - 20.f), FVector(HousePole, SD::Ground + 340.f), HousePoleR, 12, HouseCampusFallKit::PlainUV());
        Solid.Dome(SD::Metal, FVector(HousePole, SD::Ground + 340.f), FVector::UpVector, HousePoleR, 4.f, 12);
        SD::PedHead(Solid, FVector(HousePole, SD::Ground + 300.f) + South * 12.f, South);
        Solid.BoxMinMax(TEXT("CS_SignalHousing"), FVector(HousePole.X - HousePoleR - 12.f, HousePole.Y - 6.f, SD::Ground + 105.f), FVector(HousePole.X - HousePoleR, HousePole.Y + 6.f, SD::Ground + 125.f), HouseCampusFallKit::PlainUV());
        Solid.Cylinder(SD::Metal, FVector(HousePole.X - HousePoleR - 12.f, HousePole.Y, SD::Ground + 115.f), FVector(HousePole.X - HousePoleR - 15.f, HousePole.Y, SD::Ground + 115.f), 3.f, 8, HouseCampusFallKit::PlainUV());
    }

    // Storm inlets at the gutters and manholes in the lanes.
    for (const float Y : { -7000.f, -10000.f, -13000.f, -16400.f }) SD::Grate(Flat, FVector2D(8735.f, Y), FVector2D(50.f, 100.f));
    for (const float Y : { -7600.f, -11800.f, -14800.f, -16800.f }) SD::Grate(Flat, FVector2D(10265.f, Y), FVector2D(50.f, 100.f));
    for (const float X : { 1000.f, 2000.f, 6500.f }) { SD::Grate(Flat, FVector2D(X, -19765.f), FVector2D(100.f, 50.f)); SD::Grate(Flat, FVector2D(X + 700.f, -18235.f), FVector2D(100.f, 50.f)); }
    for (const FVector2D& C : { FVector2D(9100.f, -8800.f), FVector2D(9900.f, -12500.f), FVector2D(9100.f, -15600.f), FVector2D(2500.f, -19400.f), FVector2D(5600.f, -22050.f), FVector2D(1400.f, -23300.f), FVector2D(-20375.f, -14000.f), FVector2D(-6000.f, -26025.f), FVector2D(6000.f, -26025.f) })
        SD::Manhole(Flat, C);

    // The bus bay's curb is painted yellow (buses only): flat, on the gutter's and the kerb's tops at the bay's middle. Over the
    // relief (not Dormant) it drapes along the kerb: the face from 5 mm over the gutter to 2 mm over the kerb's top all along
    // it (a thin slab), and the kerb's top.
    {
        const FBox2D FacePaint(FVector2D(300.f, -17852.f), FVector2D(5200.f, -17850.5f)), TopPaint(FVector2D(300.f, -17850.f), FVector2D(5200.f, -17835.f));
        if (SD::Dormant(FBox2D(FacePaint.Min, TopPaint.Max)))
        {
            const FVector2D Bay(2750.f, -17850.f);
            const float GutterZ = SD::RoadTopAt(Bay), KerbZ = SD::WalkTopAt(Bay);
            Flat.BoxMinMax(TEXT("CS_CurbYellow"), FVector(300.f, -17852.f, GutterZ + .5f), FVector(5200.f, -17850.5f, KerbZ + .2f), HouseCampusFallKit::PlainUV());
            Flat.BoxMinMax(TEXT("CS_CurbYellow"), FVector(300.f, -17850.f, KerbZ), FVector(5200.f, -17835.f, KerbZ + .3f), HouseCampusFallKit::PlainUV());
        }
        else
        {
            HouseCalderDrape::Slab(Flat, TEXT("CS_CurbYellow"), HouseCalderDrape::RectOutline(FacePaint), [](float Sx, float Sy) { return SD::WalkTopAt(FVector2D(Sx, Sy)) + .2f; },
                [](float Sx, float Sy) { return SD::RoadTopAt(FVector2D(Sx, Sy)) + .5f; }, HouseCampusFallKit::PlainUV());
            HouseCalderDrape::Sheet(Flat, TEXT("CS_CurbYellow"), HouseCalderDrape::RectOutline(TopPaint), [](float Sx, float Sy) { return SD::WalkTopAt(FVector2D(Sx, Sy)) + .3f; }, HouseCampusFallKit::PlainUV());
        }
    }
}
