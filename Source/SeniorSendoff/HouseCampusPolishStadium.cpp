// Calder State exterior polish, step 3a: the stadium and its apron (see HouseCampusPolish.h). Over the exterior's bowl
// (HouseExteriorCampus, which with the polish on leaves real window openings in the outer wall and leaves the arch grilles
// and the flood heads to this file):
// - the facade: brick pilasters at every bay with stone bases and caps, a stone plinth course, sill and lintel courses, a
//   frieze under a real cornice (fascia, soffit, modillions) carved CALDER STATE STADIUM at the three approaches with the
//   dedication either side of the south one; a number over every arch (GATE n) and a section plate on every bay;
// - the openings: each bay's window opened onto the concourse (lit through the glass where the walk-in concourse is behind
//   it, dark elsewhere) with stone reveals, sill, lintel and keystone; iron gates in the blind arches, some with the grille
//   rolled half up on a lit vestibule, and ticket windows set into the wall;
// - the structure: steel outriggers, knee braces and a column under the press box's overhang, antennas and a dish on its
//   roof; the three flood lamp banks (frame, twelve lamps with visors, catwalk, ladder and cage, junction box, footing);
// - the apron: a concrete ring with joints and drains where nothing else is paved, worn grass and mud, leaf drifts against
//   the wall, overflowing barrels, chain-link and sawhorse barricades, two shuttered concession stands with hand-painted
//   price boards, a souvenir table, portable toilets, the two band buses and Dunmore's coach in the bus bay;
// - the South Lot after the tailgates: chairs, coolers, a canopy, crushed cans and a grill still smoking behind some of
//   the parked cars, shoe-polish on their windows, streamers on the antennas.
// Everything is code-made, deterministic, emission for glow, no lights.
#include "HouseStadiumDetail.h"
#include "HouseCampusPolish.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"
#include "HouseCalderTerrain.h"
#include "HouseCampusFallKit.h"
#include "HouseCampusInteriors.h"
#include "HouseCampusInteriorsRooms.h"
#include "HouseCampusLayout.h"
#include "HouseExteriorCampus.h"
#include "HouseExteriorSiteLayout.h"
#include "Engine/Texture2D.h"

namespace HouseCampusPolishStadiumImpl
{
constexpr float RX = 5600.f, RY = 3800.f, G = -60.f, LotZ = -72.f;
constexpr int32 Seg = 40;
const FHallUV& Plain() { return HouseCampusFallKit::PlainUV(); }
float Angle(int32 K) { return K * 2.f * PI / Seg; }
FVector Bowl(float A, float R, float Z) { return FVector(HouseCampus::StadiumX + FMath::Cos(A) * RX * R, HouseCampus::StadiumY + FMath::Sin(A) * RY * R, G + Z); }
/** The wall's height over the ground of a point (the wall stands on G). */
FVector Up(float Z) { return FVector(0.f, 0.f, Z); }

/** One bay of the bowl at R (1 = the outer wall): from vertex K to K+1, its direction and its outward normal. */
struct FChord
{
    FVector A, B, Along, N;
    float Len = 0.f;
    FChord(int32 K, float R)
    {
        A = Bowl(Angle(K), R, 0.f); B = Bowl(Angle(K + 1), R, 0.f);
        Along = (B - A).GetSafeNormal(); Len = FVector::Dist(A, B);
        N = FVector(Along.Y, -Along.X, 0.f);   // the bowl runs anticlockwise: Along's right is out
    }
    /** U along the bay, D out from its wall (negative: in), Z over the ground. */
    FVector At(float U, float D, float Z) const { return A + Along * U + N * D + Up(Z); }
    void Slab(FHallMesh& M, FName Key, float U0, float U1, float D0, float D1, float Z0, float Z1, const FHallUV& UV = Plain()) const
    {
        if (U1 - U0 < .05f || D1 - D0 < .05f || Z1 - Z0 < .05f) return;
        M.Box(Key, FTransform(Along.Rotation(), At((U0 + U1) * .5f, (D0 + D1) * .5f, (Z0 + Z1) * .5f)), FVector((U1 - U0) * .5f, (D1 - D0) * .5f, (Z1 - Z0) * .5f), UV);
    }
    /** A card on the bay's face: centre U, Z, out D, facing out. */
    void Card(FHallMesh& M, FName Key, float U, float D, float Z, float W, float H) const
    {
        HouseCampusFallKit::Card(M, Key, At(U, D, Z), -Along, FVector::UpVector, W, H, N);
    }
};
/** Not within Radius of any of the plan's walks (for props that stand on a road or a lot on purpose). */
bool OffWalks(const FVector2D& P, float Radius)
{
    static const TArray<FVector> Samples = [] { TArray<FVector> S; HouseCampusLayout::WalkSamples(S); return S; }();
    for (const FVector& S : Samples) if (FVector2D::DistSquared(P, FVector2D(S.X, S.Y)) < Radius * Radius) return false;
    return true;
}
/** Where the viewer's right is on a surface facing N (text and painted cards read this way). */
FVector ReadRight(const FVector& N) { return FVector::CrossProduct(FVector::UpVector, -N).GetSafeNormal(); }
/** Copies Src's sections into Dst. */
void AppendMesh(FHallMesh& Dst, const FHallMesh& Src)
{
    for (const FName& Key : Src.Order)
    {
        const FHallSection& From = Src.Sections[Key];
        FHallSection& To = Dst.Sec(Key);
        const int32 Base = To.V.Num();
        To.V.Append(From.V); To.N.Append(From.N); To.UV.Append(From.UV); To.T.Append(From.T);
        for (const int32 I : From.I) To.I.Add(Base + I);
    }
}
/** On the concrete apron round the wall (not the plan's own paving): the ring the terrain holds at today's height. */
bool OnApronRing(const FVector2D& P)
{
    const FVector2D D(P.X - HouseCampus::StadiumX, P.Y - HouseCampus::StadiumY);
    const bool bRing = FMath::Square(D.X / RX) + FMath::Square(D.Y / RY) > 1.f && FMath::Square(D.X / (RX + 470.f)) + FMath::Square(D.Y / (RY + 470.f)) < 1.f;
    return bRing && !HouseCampusLayout::Occupied(P, 6.f) && !HouseCampusLayout::OnTrafficLane(P);
}
/** The height to lay a patch at: the apron's top on the ring, else the ground (the Calder terrain). */
float GroundZAt(const FVector2D& P) { return OnApronRing(P) ? G + 3.f : HouseExteriorSite::GroundHeight(P.X, P.Y); }
/** The outward normal at vertex K (where bays K-1 and K meet). */
FVector VertexNormal(int32 K)
{
    const FChord L((K + Seg - 1) % Seg, 1.f), R(K % Seg, 1.f);
    return (L.N + R.N).GetSafeNormal();
}
/** The exterior's own frame for the arch on bay K (it uses the radial direction at the bay's middle, not the chord's). */
struct FArchFrame
{
    FVector Centre, Along, N;
    explicit FArchFrame(int32 K)
    {
        const float A = Angle(K), B = Angle(K + 1);
        const FVector A0 = Bowl(A, 1.f, 0.f), B0 = Bowl(B, 1.f, 0.f);
        N = FVector(FMath::Cos((A + B) * .5f), FMath::Sin((A + B) * .5f), 0.f);
        Centre = (A0 + B0) * .5f + N * 9.f;
        Along = (B0 - A0).GetSafeNormal();
    }
};
bool IsGate(int32 K) { return HouseCampusInteriors::StadiumGate(K); }
bool HasArch(int32 K) { return K % 2 == 0 && !IsGate(K) && !HouseStadiumDetail::Portal(K); }
/** The arch's gate number (the walk-in gates are 4 and 5: number K / 2, the east one 20). */
int32 GateNumber(int32 K) { return K == 0 ? 20 : K / 2; }
/** Blind arches whose grille is rolled half up on a lit vestibule, and bays with a ticket window set into the wall. */
bool GrilleUp(int32 K) { return K == 2 || K == 14 || K == 26 || K == 30 || K == 34; }
bool TicketWindow(int32 K) { return K == 1 || K == 7 || K == 11 || K == 29 || K == 31 || K == 39; }

/** Raised letters (geometry) on a surface: centred on C, reading along Right, facing N. */
void Letters(FHallMesh& M, FName Key, const FString& Text, const FVector& C, const FVector& Right, const FVector& N, float Cap, float Stroke)
{
    const float Width = Text.Len() * 5.f * Cap / 6.f - Cap / 6.f;
    HouseCampusFallKit::GlyphStrips(M, Key, Text, C - Right * (Width * .5f) - FVector(0, 0, Cap * .5f), Right, FVector::UpVector, N, Cap, Stroke);
}
/** An enamel plate with its letters: a thin box and raised letters, centred on C facing N. */
void Plate(FHallMesh& M, FName Back, FName Ink, const FString& Text, const FVector& C, const FVector& Right, const FVector& N, float W, float H, float Cap)
{
    M.Box(Back, FTransform(FRotationMatrix::MakeFromXZ(Right, FVector::UpVector).ToQuat(), C + N * .8f), FVector(W * .5f, .8f, H * .5f), Plain());
    Letters(M, Ink, Text, C + N * 1.7f, Right, N, Cap, Cap * .16f);
    HouseStadiumDetail::SignHardware(M,C+N*1.72f,Right,N,W,H);
}

// ------------------------------------------------------------------ materials
void PaintBrick(FHallImage& Img, const FLinearColor& Base, const FLinearColor& Mortar, int32 Seed)
{
    // 120 x 120 cm a tile: 16 courses of 7.5 cm, 40 cm stretchers in running bond, each brick its own shade.
    constexpr int32 N = 256;
    Img.Init(N, N, Mortar);
    const float Course = N / 16.f, Brick = N / 3.f;
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const int32 C = int32(Y / Course);
            const float Shift = (C & 1) ? Brick * .5f : 0.f, Fx = FMath::Fmod(X + Shift, Brick), Fy = Y - C * Course;
            if (Fx < 2.2f || Fy < 1.8f) continue;
            const int32 Id = C * 7 + int32((X + Shift) / Brick);
            const float V = .82f + .3f * HallNoise(Id * 3.1f, 7.7f, Seed) + .1f * HallFbm(X * .15f, Y * .15f, 3, Seed + 1);
            const float Burn = HallNoise(Id * 1.7f, 2.3f, Seed + 4) > .35f ? .72f : 1.f;
            Img.At(X, Y) = Base * V * Burn;
        }
}
void PaintAshlar(FHallImage& Img, const FLinearColor& Base, int32 Seed, float Grime)
{
    // Limestone in courses (one tile 120 x 120: four courses, blocks 60 long), a little weathering streaked down.
    constexpr int32 N = 256;
    Img.Init(N, N, Base);
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const int32 C = Y / 64;
            const float Shift = (C & 1) ? 64.f : 0.f, Fx = FMath::Fmod(X + Shift, 128.f), Fy = float(Y % 64);
            const bool bJoint = Fx < 1.6f || Fy < 1.4f;
            const float V = .9f + .14f * HallFbm(X * .05f, Y * .05f, 4, Seed) + .05f * HallNoise(X * .7f, Y * .7f, Seed + 2);
            const float Streak = Grime * FMath::Clamp(HallFbm(X * .25f, Y * .012f, 3, Seed + 3) - .2f, 0.f, 1.f);
            Img.At(X, Y) = bJoint ? Base * .55f : Base * V * (1.f - Streak);
        }
}
void PaintConcrete(FHallImage& Img, const FLinearColor& Base, int32 Seed, float Stains)
{
    constexpr int32 N = 256;
    Img.Init(N, N, Base);
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const float V = .9f + .12f * HallFbm(X * .04f, Y * .04f, 4, Seed) + .06f * HallNoise(X * .9f, Y * .9f, Seed + 1);
            const float Stain = Stains * FMath::Clamp(HallFbm(X * .02f + 9.f, Y * .02f, 3, Seed + 2) - .35f, 0.f, 1.f) * 1.6f;
            Img.At(X, Y) = Base * V * (1.f - Stain);
        }
}
/** The concourse seen through its window: the far block wall, the lit ceiling with its tube fittings, the floor in shadow. */
void PaintConcourseView(FHallImage& Img)
{
    constexpr int32 W = 256, H = 256;
    Img.Init(W, H, FLinearColor(.18f, .15f, .11f));
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float V = Y / float(H);
            FLinearColor C = FMath::Lerp(FLinearColor(.95f, .86f, .66f), FLinearColor(.42f, .36f, .27f), FMath::Clamp((V - .15f) / .7f, 0.f, 1.f));
            if (V < .14f) C = FLinearColor(.75f, .72f, .62f) * (.6f + V * 3.f);   // the ceiling
            if (V > .14f && V < .82f && (Y % 22 < 2 || (X + (Y / 22) * 20) % 40 < 2)) C *= .78f;   // block joints
            if (V > .82f) C = FLinearColor(.22f, .2f, .17f);   // the floor beyond the sill
            Img.At(X, Y) = C;
        }
    for (int32 K = 0; K < 3; ++K) Img.Rect(FVector2D(20 + K * 80, 10), FVector2D(70 + K * 80, 18), FLinearColor(1.4f, 1.35f, 1.2f));   // tubes
    for (int32 K = 1; K < 4; ++K) Img.Rect(FVector2D(K * 64 - 3, 0), FVector2D(K * 64 + 3, H), FLinearColor(.06f, .065f, .065f));   // the steel sash
    Img.Rect(FVector2D(0, 156), FVector2D(W, 161), FLinearColor(.06f, .065f, .065f));
    Img.Rect(FVector2D(150, 90), FVector2D(200, 150), FLinearColor(.36f, .12f, .45f));   // a GATE board on the far wall
    HallDrawText(Img, TEXT("GEESE"), FVector2D(155, 128), FHallTextStyle{ 12.f, 2.f, .2f, .85f }, FLinearColor(.9f, .75f, .25f));
}
void PaintDarkGlass(FHallImage& Img)
{
    constexpr int32 N = 128;
    Img.Init(N, N, FLinearColor(.02f, .025f, .03f));
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
        {
            const float Sky = FMath::Clamp(1.f - Y / float(N) * 1.6f, 0.f, 1.f) * .12f;
            const float Sheen = FMath::Max(0.f, 1.f - FMath::Abs((X - Y * .6f) - 40.f) / 14.f) * .06f;
            Img.At(X, Y) = FLinearColor(.025f + Sky * .7f + Sheen, .03f + Sky * .75f + Sheen, .04f + Sky + Sheen);
        }
}
/** Through a half-up grille: the gate's vestibule, lit, with a pair of turnstiles and the concourse floor beyond. */
void PaintVestibule(FHallImage& Img)
{
    constexpr int32 W = 256, H = 256;
    Img.Init(W, H, FLinearColor(.16f, .14f, .11f));
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float V = Y / float(H), Edge = FMath::Abs(X / float(W) - .5f) * 2.f;
            Img.At(X, Y) = FMath::Lerp(FLinearColor(.42f, .37f, .28f), FLinearColor(.12f, .1f, .08f), FMath::Clamp(V * 1.2f + Edge * .4f, 0.f, 1.f));
        }
    Img.Rect(FVector2D(96, 70), FVector2D(160, 200), FLinearColor(1.1f, .95f, .7f));   // the lit concourse through the inner doorway
    Img.Rect(FVector2D(100, 74), FVector2D(156, 80), FLinearColor(1.4f, 1.3f, 1.1f));
    Img.Rect(FVector2D(0, 200), FVector2D(W, H), FLinearColor(.1f, .09f, .08f));     // the floor
    for (const int32 X : { 66, 190 })
    {
        Img.Rect(FVector2D(X - 15, 148), FVector2D(X + 15, 222), FLinearColor(.05f, .05f, .055f));   // turnstile cabinets
        Img.Stroke({ FVector2D(X, 160), FVector2D(X + 42, 170) }, 4.f, FLinearColor(.55f, .55f, .58f));
        Img.Stroke({ FVector2D(X, 160), FVector2D(X - 32, 180) }, 4.f, FLinearColor(.55f, .55f, .58f));
        Img.Stroke({ FVector2D(X, 160), FVector2D(X - 4, 130) }, 4.f, FLinearColor(.55f, .55f, .58f));
    }
}
/** The ticket seller's window: the booth behind the glass, a roll of tickets, the price card. */
void PaintTicketBooth(FHallImage& Img)
{
    constexpr int32 W = 128, H = 128;
    Img.Init(W, H, FLinearColor(.85f, .76f, .55f));
    for (int32 Y = 0; Y < H; ++Y) for (int32 X = 0; X < W; ++X) Img.At(X, Y) *= .7f + .3f * (1.f - Y / float(H));
    Img.Rect(FVector2D(0, 92), FVector2D(W, H), FLinearColor(.35f, .25f, .15f));   // the counter
    Img.Ellipse(FVector2D(36, 86), FVector2D(12, 8), 0.f, FLinearColor(.75f, .2f, .2f));   // the ticket roll
    Img.Rect(FVector2D(66, 24), FVector2D(118, 70), FLinearColor(.95f, .93f, .86f));
    FHallTextStyle S; S.Height = 9.f; S.Stroke = 1.6f; S.Spacing = .2f;
    HallDrawText(Img, TEXT("RESERVED"), FVector2D(70, 38), S, FLinearColor(.1f, .1f, .12f));
    HallDrawText(Img, TEXT("$6.00"), FVector2D(76, 52), S, FLinearColor(.1f, .1f, .12f));
    HallDrawText(Img, TEXT("SOLD OUT"), FVector2D(70, 66), S, FLinearColor(.6f, .05f, .05f));
}
/** A hand-painted price board: lines of "ITEM PRICE", the cents sign drawn by hand. */
void PaintPriceBoard(FHallImage& Img, const TCHAR* Title, std::initializer_list<TPair<const TCHAR*, const TCHAR*>> Lines, int32 Seed)
{
    constexpr int32 W = 256, H = 320;
    Img.Init(W, H, FLinearColor(.86f, .84f, .76f));
    for (int32 Y = 0; Y < H; ++Y) for (int32 X = 0; X < W; ++X) Img.At(X, Y) *= .92f + .08f * HallFbm(X * .05f, Y * .05f, 3, Seed);
    Img.Rect(FVector2D(0, 0), FVector2D(W, 6), FLinearColor(.3f, .08f, .4f)); Img.Rect(FVector2D(0, H - 6), FVector2D(W, H), FLinearColor(.3f, .08f, .4f));
    FHallTextStyle T; T.Height = 30.f; T.Stroke = 5.f; T.Spacing = .2f; T.EndBlob = 2.5f; T.Wobble = 1.2f; T.Seed = Seed;
    T.WidthScale = FMath::Min(1.f, (W - 24.f) / HallTextWidth(Title, T));
    HallDrawText(Img, Title, FVector2D((W - HallTextWidth(Title, T)) * .5f, 46), T, FLinearColor(.32f, .08f, .42f));
    FHallTextStyle S; S.Height = 19.f; S.Stroke = 3.4f; S.Spacing = .18f; S.EndBlob = 1.6f; S.Wobble = 1.f; S.Seed = Seed + 1;
    float Y = 92.f;
    for (const TPair<const TCHAR*, const TCHAR*>& L : Lines)
    {
        FString Price = L.Value;
        const bool bCents = Price.EndsWith(TEXT("c"));
        if (bCents) Price.LeftChopInline(1);
        HallDrawText(Img, L.Key, FVector2D(14, Y), S, FLinearColor(.08f, .08f, .1f));
        const float PW = HallTextWidth(Price, S) + (bCents ? 14.f : 0.f);
        HallDrawText(Img, Price, FVector2D(W - 14 - PW, Y), S, FLinearColor(.6f, .06f, .05f));
        if (bCents)
        {
            // The cents sign: a small c with its stroke through it.
            const FVector2D C(W - 21.f, Y - 6.f);
            TArray<FVector2D> Arc;
            for (int32 K = 0; K <= 10; ++K) { const float A = FMath::DegreesToRadians(40.f + K * 28.f); Arc.Add(C + FVector2D(FMath::Cos(A), -FMath::Sin(A)) * 6.5f); }
            Img.Stroke(Arc, 2.6f, FLinearColor(.6f, .06f, .05f));
            Img.Stroke({ C + FVector2D(1.f, -11.f), C + FVector2D(-1.f, 11.f) }, 2.f, FLinearColor(.6f, .06f, .05f));
        }
        Y += 40.f;
    }
}
void PaintStripes(FHallImage& Img, const FLinearColor& A, const FLinearColor& B, const TCHAR* Text)
{
    // A sawhorse board: diagonal stripes either end, the owner's name stencilled in the middle.
    constexpr int32 W = 512, H = 64;
    Img.Init(W, H, FLinearColor(.85f, .84f, .8f));
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
            if (X < 110 || X > W - 110) Img.At(X, Y) = (int32((X + Y) / 22.f) % 2) ? A : B;
    FHallTextStyle S; S.Height = 22.f; S.Stroke = 4.f; S.Spacing = .2f;
    S.WidthScale = FMath::Min(1.f, 270.f / HallTextWidth(Text, S));
    HallDrawText(Img, Text, FVector2D((W - HallTextWidth(Text, S)) * .5f, 43), S, FLinearColor(.1f, .1f, .1f));
}
void PaintCorrugated(FHallImage& Img, const FLinearColor& Base)
{
    constexpr int32 N = 128;
    Img.Init(N, N, Base);
    for (int32 Y = 0; Y < N; ++Y)
        for (int32 X = 0; X < N; ++X)
            Img.At(X, Y) = Base * (.72f + .28f * FMath::Square(FMath::Sin(Y * PI / 8.f))) * (.94f + .06f * HallNoise(X * .3f, Y * .1f, 5));
}
/** A frieze panel: the limestone in its courses with the inscription cut into it (shadowed on top, lit underneath). */
void PaintFrieze(FHallImage& Img, int32 W, const TCHAR* Text, int32 Seed)
{
    constexpr int32 H = 128;
    const FLinearColor Stone(.62f, .58f, .5f);
    Img.Init(W, H, Stone);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const bool bJoint = (X % 128) < 2 || Y < 2 || Y > H - 3;
            const float V = .9f + .12f * HallFbm(X * .04f, Y * .04f, 4, Seed) + .05f * HallNoise(X * .6f, Y * .6f, Seed + 1);
            Img.At(X, Y) = bJoint ? Stone * .55f : Stone * V;
        }
    FHallTextStyle S; S.Height = 64.f; S.Stroke = 10.5f; S.Spacing = .3f;
    S.WidthScale = FMath::Min(1.f, (W - 60.f) / HallTextWidth(Text, S));
    const FVector2D At((W - HallTextWidth(Text, S)) * .5f, 96.f);
    HallDrawText(Img, Text, At + FVector2D(0.f, 1.6f), S, Stone * 1.15f);   // the lit lower lip of each cut
    HallDrawText(Img, Text, At, S, FLinearColor(.16f, .14f, .11f));
}
void PaintGrate(FHallImage& Img)
{
    constexpr int32 N = 64;
    Img.Init(N, N, FLinearColor(.015f, .015f, .015f));
    for (int32 Y = 3; Y < N - 3; ++Y)
        for (int32 X = 3; X < N - 3; ++X)
            if (X % 8 < 3 || Y < 6 || Y > N - 7) Img.At(X, Y) = FLinearColor(.12f, .1f, .09f);
}

void Paint(UHouseBlackHallSubsystem* Look, const TCHAR* Key, const FHallImage& Img, float Rough, const FLinearColor& Emission = FLinearColor::Black, bool bWrap = true, float Metal = 0.f)
{
    UTexture2D* T = HallMakeTexture(Img, true, false, *FString::Printf(TEXT("T_%s"), Key), bWrap);
    if (T) Look->Textures.Add(FName(*FString::Printf(TEXT("T_%s"), Key)), T);
    HallMakeTextured(Look, FName(Key), T, nullptr, Rough, FLinearColor::White, 0.f, Metal, Emission);
}
}

void HouseCampusPolish::StadiumLook(UHouseBlackHallSubsystem* Look)
{
    namespace SI = HouseCampusPolishStadiumImpl;
    FHallImage Img;
    SI::PaintBrick(Img, FLinearColor(.36f, .12f, .07f), FLinearColor(.42f, .39f, .34f), 31); SI::Paint(Look, TEXT("CP_BrickDeep"), Img, .85f);
    SI::PaintAshlar(Img, FLinearColor(.62f, .58f, .5f), 41, .25f); SI::Paint(Look, TEXT("CP_Ashlar"), Img, .8f);
    SI::PaintAshlar(Img, FLinearColor(.5f, .47f, .41f), 43, .45f); SI::Paint(Look, TEXT("CP_AshlarDark"), Img, .85f);
    SI::PaintConcrete(Img, FLinearColor(.48f, .47f, .44f), 51, .35f); SI::Paint(Look, TEXT("CP_Apron"), Img, .88f);
    SI::PaintConcourseView(Img); SI::Paint(Look, TEXT("CP_ConcourseView"), Img, .6f, FLinearColor(.2f, .17f, .12f), false);
    SI::PaintDarkGlass(Img); SI::Paint(Look, TEXT("CP_WindowDark"), Img, .08f, FLinearColor::Black, false);
    SI::PaintVestibule(Img); SI::Paint(Look, TEXT("CP_Vestibule"), Img, .6f, FLinearColor(.42f, .38f, .3f), false);
    SI::PaintTicketBooth(Img); SI::Paint(Look, TEXT("CP_TicketBooth"), Img, .5f, FLinearColor(.65f, .56f, .42f), false);
    SI::PaintPriceBoard(Img, TEXT("GEESE GRILL"), { { TEXT("HOT DOGS"), TEXT("75c") }, { TEXT("POPCORN"), TEXT("50c") }, { TEXT("POP"), TEXT("50c") }, { TEXT("COFFEE"), TEXT("35c") }, { TEXT("CANDY"), TEXT("25c") }, { TEXT("PEANUTS"), TEXT("40c") } }, 3);
    SI::Paint(Look, TEXT("CP_PriceBoardA"), Img, .85f, FLinearColor::Black, false);
    SI::PaintPriceBoard(Img, TEXT("CIDER MILL"), { { TEXT("HOT CIDER"), TEXT("50c") }, { TEXT("DONUTS"), TEXT("3/$1") }, { TEXT("COFFEE"), TEXT("35c") }, { TEXT("COCOA"), TEXT("45c") }, { TEXT("CARAMEL APPLE"), TEXT("60c") } }, 9);
    SI::Paint(Look, TEXT("CP_PriceBoardB"), Img, .85f, FLinearColor::Black, false);
    SI::PaintStripes(Img, FLinearColor(.75f, .3f, .03f), FLinearColor(.85f, .84f, .8f), TEXT("CALDER STATE PHYSICAL PLANT")); SI::Paint(Look, TEXT("CP_Sawhorse"), Img, .7f, FLinearColor::Black, false);
    SI::PaintStripes(Img, FLinearColor(.75f, .3f, .03f), FLinearColor(.85f, .84f, .8f), TEXT("BUS LOADING - NO PARKING")); SI::Paint(Look, TEXT("CP_SawhorseBus"), Img, .7f, FLinearColor::Black, false);
    SI::PaintCorrugated(Img, FLinearColor(.5f, .5f, .48f)); SI::Paint(Look, TEXT("CP_Shutter"), Img, .55f, FLinearColor::Black, true, .4f);
    SI::PaintCorrugated(Img, FLinearColor(.28f, .28f, .27f)); SI::Paint(Look, TEXT("CP_Grille"), Img, .5f, FLinearColor::Black, true, .5f);
    SI::PaintGrate(Img); SI::Paint(Look, TEXT("CP_Grate"), Img, .6f, FLinearColor::Black, true, .3f);
    SI::PaintFrieze(Img, 1024, TEXT("CALDER STATE STADIUM"), 61); SI::Paint(Look, TEXT("CP_FriezeName"), Img, .8f, FLinearColor::Black, false);
    SI::PaintFrieze(Img, 768, TEXT("CALDER STATE STADIUM"), 62); SI::Paint(Look, TEXT("CP_FriezeNameShort"), Img, .8f, FLinearColor::Black, false);
    SI::PaintFrieze(Img, 1024, TEXT("ERECTED BY THE ALUMNI"), 63); SI::Paint(Look, TEXT("CP_FriezeErected"), Img, .8f, FLinearColor::Black, false);
    SI::PaintFrieze(Img, 1024, HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=5?TEXT("DEDICATED OCTOBER 17 1931"):TEXT("DEDICATED OCTOBER 22 1927"), 64); SI::Paint(Look, TEXT("CP_FriezeDedicated"), Img, .8f, FLinearColor::Black, false);
    HouseCampusInteriorsImpl::PaintText(Look, TEXT("CP_SouvenirSign"), 256, 96, FLinearColor(.86f, .84f, .74f), FLinearColor(.32f, .08f, .42f), { { TEXT("SOUVENIRS"), .34f }, { TEXT("PENNANTS $2  BUTTONS 50C"), .16f } }, true);
    HouseCampusInteriorsImpl::PaintText(Look, TEXT("CP_StandSignA"), 512, 96, FLinearColor(.3f, .08f, .4f), FLinearColor(.95f, .78f, .25f), { { TEXT("HOT DOGS - POPCORN - POP"), .42f } }, false);
    HouseCampusInteriorsImpl::PaintText(Look, TEXT("CP_StandSignB"), 512, 96, FLinearColor(.95f, .78f, .25f), FLinearColor(.3f, .08f, .4f), { { TEXT("HOT CIDER & DONUTS"), .42f } }, false);
    HouseCampusInteriorsImpl::PaintText(Look, TEXT("CP_ToiletSign"), 128, 64, FLinearColor(.9f, .9f, .88f), FLinearColor(.08f, .1f, .3f), { { TEXT("UNIT 14"), .3f }, { TEXT("SANITIZED"), .2f } }, false);
    HouseCampusInteriorsImpl::PaintText(Look, TEXT("CP_BandSign"), 512, 128, FLinearColor(.92f, .9f, .82f), FLinearColor(.32f, .08f, .42f), { { TEXT("MARCHING GEESE"), .34f }, { TEXT("BAND - DO NOT LEAVE INSTRUMENTS"), .16f } }, false);
    HouseCampusInteriorsImpl::PaintText(Look, TEXT("CP_DestSign"), 256, 48, FLinearColor(.02f, .02f, .02f), FLinearColor(1.f, .7f, .2f), { { TEXT("CHARTER"), .55f } }, false, FLinearColor(1.2f, .8f, .3f));
    struct FPlainSpec { const TCHAR* Key; FLinearColor Tint; float Rough, Metal; FLinearColor Emission; };
    const FPlainSpec Plains[] = {
        { TEXT("CP_Incised"), FLinearColor(.12f, .1f, .08f), .7f, 0.f, FLinearColor::Black },
        { TEXT("CP_EnamelPurple"), FLinearColor(.2f, .05f, .27f), .3f, 0.f, FLinearColor(.01f, .002f, .014f) },
        { TEXT("CP_EnamelGold"), FLinearColor(.78f, .58f, .16f), .35f, .3f, FLinearColor(.03f, .022f, .006f) },
        { TEXT("CP_EnamelCream"), FLinearColor(.8f, .78f, .7f), .4f, 0.f, FLinearColor::Black },
        { TEXT("CP_Iron"), FLinearColor(.04f, .04f, .045f), .55f, .6f, FLinearColor::Black },
        { TEXT("CP_Steel"), FLinearColor(.24f, .27f, .26f), .5f, .5f, FLinearColor::Black },
        { TEXT("CP_SteelDark"), FLinearColor(.1f, .11f, .11f), .55f, .5f, FLinearColor::Black },
        { TEXT("CP_Galv"), FLinearColor(.45f, .46f, .45f), .4f, .8f, FLinearColor::Black },
        { TEXT("CP_FloodCan"), FLinearColor(.16f, .17f, .17f), .45f, .6f, FLinearColor::Black },
        { TEXT("CP_FloodLens"), FLinearColor(.9f, .93f, 1.f), .1f, 0.f, FLinearColor(30.f, 32.f, 36.f) },
        { TEXT("CP_Grating"), FLinearColor(.08f, .085f, .085f), .7f, .4f, FLinearColor::Black },
        { TEXT("CP_JunctionBox"), FLinearColor(.36f, .38f, .37f), .5f, .4f, FLinearColor::Black },
        { TEXT("CP_Footing"), FLinearColor(.42f, .41f, .38f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CP_Joint"), FLinearColor(.07f, .07f, .065f), .95f, 0.f, FLinearColor::Black },
        { TEXT("CP_WornGrass"), FLinearColor(.17f, .15f, .08f), .95f, 0.f, FLinearColor::Black },
        { TEXT("CP_Mud"), FLinearColor(.07f, .05f, .03f), .35f, 0.f, FLinearColor::Black },
        { TEXT("CP_LeafRed"), FLinearColor(.42f, .07f, .03f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CP_LeafOrange"), FLinearColor(.6f, .25f, .04f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CP_LeafBrown"), FLinearColor(.22f, .12f, .05f), .85f, 0.f, FLinearColor::Black },
        { TEXT("CP_LeafYellow"), FLinearColor(.62f, .48f, .08f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CP_DrumPurple"), FLinearColor(.17f, .05f, .22f), .55f, .2f, FLinearColor::Black },
        { TEXT("CP_DrumGold"), FLinearColor(.62f, .44f, .1f), .55f, .2f, FLinearColor::Black },
        { TEXT("CP_DrumGrey"), FLinearColor(.25f, .26f, .25f), .6f, .3f, FLinearColor::Black },
        { TEXT("CP_White"), FLinearColor(.8f, .79f, .76f), .6f, 0.f, FLinearColor::Black },
        { TEXT("CP_Paper"), FLinearColor(.72f, .7f, .64f), .9f, 0.f, FLinearColor::Black },
        { TEXT("CP_PopcornRed"), FLinearColor(.55f, .05f, .04f), .7f, 0.f, FLinearColor::Black },
        { TEXT("CP_Plywood"), FLinearColor(.72f, .7f, .64f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CP_TrimPurple"), FLinearColor(.2f, .05f, .26f), .6f, 0.f, FLinearColor::Black },
        { TEXT("CP_Wood"), FLinearColor(.35f, .24f, .14f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CP_TableTop"), FLinearColor(.78f, .77f, .72f), .5f, 0.f, FLinearColor::Black },
        { TEXT("CP_ToiletBody"), FLinearColor(.12f, .3f, .32f), .55f, 0.f, FLinearColor::Black },
        { TEXT("CP_ToiletRoof"), FLinearColor(.75f, .75f, .72f), .5f, 0.f, FLinearColor::Black },
        { TEXT("CP_Vacant"), FLinearColor(.05f, .4f, .08f), .4f, 0.f, FLinearColor::Black },
        { TEXT("CP_BusYellow"), FLinearColor(.75f, .48f, .02f), .45f, 0.f, FLinearColor::Black },
        { TEXT("CP_BusBlack"), FLinearColor(.02f, .02f, .02f), .5f, 0.f, FLinearColor::Black },
        { TEXT("CP_BusGlass"), FLinearColor(.035f, .04f, .05f), .3f, 0.f, FLinearColor::Black },
        { TEXT("CP_Tyre"), FLinearColor(.025f, .025f, .025f), .85f, 0.f, FLinearColor::Black },
        { TEXT("CP_Chrome"), FLinearColor(.6f, .6f, .62f), .2f, 1.f, FLinearColor::Black },
        { TEXT("CP_CoachWhite"), FLinearColor(.72f, .72f, .7f), .35f, 0.f, FLinearColor::Black },
        { TEXT("CP_CoachStripe"), FLinearColor(.3f, .04f, .05f), .4f, 0.f, FLinearColor::Black },
        { TEXT("CP_LampRed"), FLinearColor(.4f, .02f, .02f), .3f, 0.f, FLinearColor(.3f, .01f, .01f) },
        { TEXT("CP_LampAmber"), FLinearColor(.6f, .3f, .02f), .3f, 0.f, FLinearColor(.2f, .08f, .005f) },
        { TEXT("CP_WebPurple"), FLinearColor(.26f, .07f, .34f), .7f, 0.f, FLinearColor::Black },
        { TEXT("CP_WebGold"), FLinearColor(.7f, .52f, .12f), .7f, 0.f, FLinearColor::Black },
        { TEXT("CP_WebGreen"), FLinearColor(.1f, .3f, .14f), .7f, 0.f, FLinearColor::Black },
        { TEXT("CP_Aluminum"), FLinearColor(.55f, .56f, .57f), .35f, .9f, FLinearColor::Black },
        { TEXT("CP_CoolerRed"), FLinearColor(.5f, .06f, .05f), .45f, 0.f, FLinearColor::Black },
        { TEXT("CP_CoolerGreen"), FLinearColor(.08f, .26f, .12f), .45f, 0.f, FLinearColor::Black },
        { TEXT("CP_CanSilver"), FLinearColor(.6f, .6f, .62f), .3f, .9f, FLinearColor::Black },
        { TEXT("CP_CanRed"), FLinearColor(.55f, .06f, .05f), .3f, .5f, FLinearColor::Black },
        { TEXT("CP_CanBlue"), FLinearColor(.08f, .14f, .45f), .3f, .5f, FLinearColor::Black },
        { TEXT("CP_CanopyPurple"), FLinearColor(.24f, .06f, .32f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CP_CanopyGold"), FLinearColor(.72f, .54f, .13f), .8f, 0.f, FLinearColor::Black },
        { TEXT("CP_GrillBlack"), FLinearColor(.02f, .02f, .02f), .5f, .3f, FLinearColor::Black },
        { TEXT("CP_Embers"), FLinearColor(.5f, .1f, .02f), .9f, 0.f, FLinearColor(9.f, 2.2f, .3f) },
        { TEXT("CP_Ash"), FLinearColor(.3f, .29f, .27f), .95f, 0.f, FLinearColor::Black },
        { TEXT("CP_ShoePolish"), FLinearColor(.86f, .86f, .84f), .7f, 0.f, FLinearColor(.02f, .02f, .02f) },
        { TEXT("CP_StreamerPurple"), FLinearColor(.3f, .06f, .4f), .7f, 0.f, FLinearColor(.01f, .002f, .014f) },
        { TEXT("CP_StreamerGold"), FLinearColor(.78f, .58f, .14f), .7f, 0.f, FLinearColor(.02f, .015f, .004f) },
    };
    for (const FPlainSpec& P : Plains) HallMakePlain(Look, P.Key, P.Tint, P.Rough, P.Metal, P.Emission);
    HouseStadiumDetail::SignLook(Look,true);
}

// ------------------------------------------------------------------ the outer wall's bays
void HouseCampusPolish::StadiumWall(FHallMesh& M, int32 K, const FVector& A0, const FVector& B0)
{
    // The brick with this bay's window cut through it (the exterior's own wall, built here when the polish is on).
    const FVector Along = (B0 - A0).GetSafeNormal(), N(Along.Y, -Along.X, 0.f);
    const float Len = FVector::Dist(A0, B0);
    TArray<FBox2D> Holes = { FBox2D(FVector2D(Len * HouseCampusInteriors::StadiumWinT0, HouseCampusInteriors::StadiumWinZ0), FVector2D(Len * HouseCampusInteriors::StadiumWinT1, HouseCampusInteriors::StadiumWinZ1)) };
    if(HouseStadiumDetail::Aisle(K))Holes.Add(FBox2D(FVector2D(Len*.5f-135.f,900.f),FVector2D(Len*.5f+135.f,1110.f)));
    if(HouseStadiumDetail::Portal(K))Holes.Add(FBox2D(FVector2D(Len*.5f-175.f,-1.f),FVector2D(Len*.5f+175.f,420.f)));
    HouseCampusInteriorsImpl::Subtract(0.f, Len, 0.f, 1050.f, Holes, [&](float UA, float UB, float ZA, float ZB)
    {
        M.Quad(TEXT("EX_Brick"), A0 + Along * UA + FVector(0, 0, ZA), A0 + Along * UB + FVector(0, 0, ZA), A0 + Along * UB + FVector(0, 0, ZB), A0 + Along * UA + FVector(0, 0, ZB), N, FHallUV::World(100.f));
    });
}

namespace HouseCampusPolishStadiumImpl
{
void Facade(HouseCampusPolish::FParts& P)
{
    FHallMesh& D = P.Far;
    FHallMesh& S = P.Solid;
    const FHallUV Brick = FHallUV::World(100.f), Stone = FHallUV::World(120.f);
    for (int32 K = 0; K < Seg; ++K)
    {
        const FChord C(K, 1.f);
        const float Mid = C.Len * .5f;
        const bool bGate = IsGate(K);
        // The plinth course (stopping at a gate's opening) and the courses: sill, lintel, the frieze under the cornice.
        TArray<FBox2D> Openings;
        if (bGate) Openings.Add(FBox2D(FVector2D(Mid - 125.f, -1.f), FVector2D(Mid + 125.f, 300.f)));
        if(HouseStadiumDetail::Portal(K))Openings.Add(FBox2D(FVector2D(Mid-175.f,-1.f),FVector2D(Mid+175.f,420.f)));
        HouseCampusInteriorsImpl::Subtract(0.f, C.Len, 0.f, 92.f, Openings, [&](float UA, float UB, float ZA, float ZB) { C.Slab(S, TEXT("CP_AshlarDark"), UA, UB, 0.f, 6.f, ZA, ZB, Stone); });
        if(HouseStadiumDetail::Portal(K)){C.Slab(D,TEXT("CP_Ashlar"),0.f,Mid-175.f,0.f,6.f,92.f,100.f,Stone);C.Slab(D,TEXT("CP_Ashlar"),Mid+175.f,C.Len,0.f,6.f,92.f,100.f,Stone);}else C.Slab(D, TEXT("CP_Ashlar"), 0.f, C.Len, 0.f, 6.f, 92.f, 100.f, Stone);                                // the plinth's cap
        C.Slab(D, TEXT("CP_Ashlar"), 0.f, C.Len, 0.f, 7.f, HouseCampusInteriors::StadiumWinZ0 - 14.f, HouseCampusInteriors::StadiumWinZ0, Stone);   // sill course
        C.Slab(D, TEXT("CP_Ashlar"), 0.f, C.Len, 0.f, 6.f, 720.f, 736.f, Stone);                               // lintel course
        if(!HouseStadiumDetail::Aisle(K))
        {
        C.Slab(D, TEXT("CP_Ashlar"), 0.f, C.Len, 0.f, 3.f, 930.f, 1040.f, Stone);                              // the frieze
        C.Slab(D, TEXT("CP_Ashlar"), 0.f, C.Len, 0.f, 9.f, 918.f, 930.f, Stone);                               // its bed moulding
        // The cornice: the rim's overhang gets a fascia and a soffit, with modillions under it every 1.2 m.
        {
            const FChord O(K, 1.045f);
            const float Out = FVector::DotProduct(O.A - C.A, C.N);
            O.Slab(D, TEXT("CP_Ashlar"), -2.f, O.Len + 2.f, -1.f, 4.f, 1000.f, 1062.f, Stone);
            const FVector S0 = C.At(0.f, 9.f, 1000.f), S1 = C.At(C.Len, 9.f, 1000.f), S2 = O.At(O.Len, -1.f, 1000.f), S3 = O.At(0.f, -1.f, 1000.f);
            D.Quad(TEXT("CP_Ashlar"), S0, S1, S2, S3, -FVector::UpVector, Stone);
            for (float U = 60.f; U < C.Len - 30.f; U += 120.f)
            {
                C.Slab(D, TEXT("CP_Ashlar"), U - 9.f, U + 9.f, 3.f, FMath::Min(Out - 1.f, 34.f), 978.f, 1000.f, Stone);
                C.Slab(D, TEXT("CP_Ashlar"), U - 9.f, U + 9.f, 3.f, 14.f, 960.f, 978.f, Stone);
            }
        }
        }
        // Pilasters at the bay's start: brick shaft on a stone base, a stone cap under the frieze.
        {
            const FVector V = C.A, Nv = VertexNormal(K), Tv(-Nv.Y, Nv.X, 0.f);
            const FQuat Q = FRotationMatrix::MakeFromXZ(Tv, FVector::UpVector).ToQuat();
            D.Box(TEXT("CP_BrickDeep"), FTransform(Q, V + Nv * 13.f + Up(510.f)), FVector(34.f, 13.f, 410.f), Brick);
            S.Box(TEXT("CP_AshlarDark"), FTransform(Q, V + Nv * 17.f + Up(50.f)), FVector(42.f, 17.f, 50.f), Stone);
            D.Box(TEXT("CP_Ashlar"), FTransform(Q, V + Nv * 16.f + Up(905.f)), FVector(40.f, 16.f, 13.f), Stone);
        }
        // The window: stone reveals back to the glass, a projecting sill, a lintel with its keystone; through the glass,
        // the lit concourse where the walk-in concourse is behind this bay (its own steel sash shows when it is built), dark
        // glass and a steel sash elsewhere.
        {
            const float U0 = C.Len * HouseCampusInteriors::StadiumWinT0, U1 = C.Len * HouseCampusInteriors::StadiumWinT1;
            const float Z0 = HouseCampusInteriors::StadiumWinZ0, Z1 = HouseCampusInteriors::StadiumWinZ1;
            const FChord Skin(K, .995f);
            const float Depth = FVector::DotProduct(C.A - Skin.A, C.N) + 11.f;   // the concourse's glazing sits 12 cm in from its skin
            C.Slab(D, TEXT("CP_Ashlar"), U0, U0 + 6.f, -Depth, 0.f, Z0, Z1, Stone);
            C.Slab(D, TEXT("CP_Ashlar"), U1 - 6.f, U1, -Depth, 0.f, Z0, Z1, Stone);
            C.Slab(D, TEXT("CP_Ashlar"), U0, U1, -Depth, 0.f, Z1 - 6.f, Z1, Stone);
            C.Slab(D, TEXT("CP_Ashlar"), U0, U1, -Depth, 0.f, Z0, Z0 + 4.f, Stone);
            C.Slab(D, TEXT("CP_AshlarDark"), U0 - 16.f, U1 + 16.f, -2.f, 12.f, Z0 - 10.f, Z0 + 1.f, Stone);
            C.Slab(D, TEXT("CP_Ashlar"), U0 - 18.f, U1 + 18.f, 0.f, 5.f, Z1, Z1 + 30.f, Stone);
            C.Slab(D, TEXT("CP_AshlarDark"), (U0 + U1) * .5f - 16.f, (U0 + U1) * .5f + 16.f, 0.f, 8.f, Z1 - 6.f, Z1 + 40.f, Stone);
            const bool bLit = HouseCampusInteriors::StadiumConcourse(K);
            const FVector Back = C.At((U0 + U1) * .5f, -Depth + .5f, (Z0 + Z1) * .5f);
            HouseCampusFallKit::Card(bLit ? P.Glow : D, bLit ? TEXT("CP_ConcourseView") : TEXT("CP_WindowDark"), Back, ReadRight(C.N), FVector::UpVector, U1 - U0 - 10.f, Z1 - Z0 - 8.f, C.N);
            if (!bLit)
            {
                for (int32 J = 1; J < 4; ++J) { const float U = FMath::Lerp(U0, U1, J / 4.f); C.Slab(D, TEXT("CP_Steel"), U - 2.5f, U + 2.5f, -Depth + 1.f, -Depth + 6.f, Z0, Z1); }
                C.Slab(D, TEXT("CP_Steel"), U0, U1, -Depth + 1.f, -Depth + 6.f, Z0 + (Z1 - Z0) * .62f - 2.5f, Z0 + (Z1 - Z0) * .62f + 2.5f);
            }
        }
        // A section plate on every bay, by its start at eye level.
        {
            const FString Sec = FString::Printf(TEXT("SEC %d"), K + 1);
            Plate(D, TEXT("CP_EnamelPurple"), TEXT("CP_EnamelGold"), Sec, C.At(75.f, 6.f, 235.f), ReadRight(C.N), C.N, 70.f, 24.f, 12.f);
        }
        // The blind arches: iron gates, the gate's number in the tympanum; some rolled half up on a lit vestibule.
        if (HasArch(K))
        {
            const FArchFrame Ar(K);
            const FVector Infill = Ar.Centre + Ar.N * 15.6f;
            if (GrilleUp(K))
            {
                // The vestibule behind, lit, then the rolling grille's slats from the springing down to 150, its bottom bar.
                HouseCampusFallKit::Card(P.Glow, TEXT("CP_Vestibule"), Infill + Up(152.f), -Ar.Along, FVector::UpVector, 222.f, 288.f, Ar.N);
                D.Box(TEXT("CP_Grille"), FTransform(FRotationMatrix::MakeFromXZ(Ar.Along, FVector::UpVector).ToQuat(), Ar.Centre + Ar.N * 17.f + Up(222.f)), FVector(112.f, 1.f, 73.f), FHallUV::World(60.f));
                HouseCampusFallKit::Bar(D, TEXT("CP_Iron"), Ar.Centre + Ar.N * 18.5f - Ar.Along * 112.f + Up(150.f), Ar.Centre + Ar.N * 18.5f + Ar.Along * 112.f + Up(150.f), 6.f, 3.f, Ar.N);
                for (const float Sd : { -1.f, 1.f }) HouseCampusFallKit::Bar(S, TEXT("CP_Iron"), Ar.Centre + Ar.N * 18.f + Ar.Along * (Sd * 108.f) + Up(4.f), Ar.Centre + Ar.N * 18.f + Ar.Along * (Sd * 108.f) + Up(295.f), 6.f, 6.f, Ar.N);
            }
            else
            {
                for (int32 J = -3; J <= 3; ++J)
                {
                    const FVector B = Ar.Centre + Ar.Along * (J * 31.f) + Ar.N * 17.f;
                    HouseCampusFallKit::Bar(S, TEXT("CP_Iron"), B + Up(6.f), B + Up(286.f), 3.6f, 3.6f, Ar.N);
                    HouseCampusFallKit::Lathe(D, TEXT("CP_Iron"), B + Up(286.f), { FVector2D(4.f, 0.f), FVector2D(.1f, 14.f) }, 4);
                }
                for (const float Z : { 30.f, 150.f, 262.f }) HouseCampusFallKit::Bar(S, TEXT("CP_Iron"), Ar.Centre + Ar.N * 18.5f - Ar.Along * 104.f + Up(Z), Ar.Centre + Ar.N * 18.5f + Ar.Along * 104.f + Up(Z), 5.f, 3.f, Ar.N);
            }
            Plate(D, TEXT("CP_EnamelCream"), TEXT("CP_Incised"), FString::Printf(TEXT("GATE %d"), GateNumber(K)), Infill + Ar.N * 1.5f + Up(338.f), -Ar.Along, Ar.N, 104.f, 36.f, 22.f);
        }
        // Ticket windows set into the wall on a few plain bays, near the gates and the bus bay.
        if (TicketWindow(K))
        {
            const float U = Mid;
            C.Slab(D, TEXT("CP_Ashlar"), U - 52.f, U + 52.f, 0.f, 6.f, 92.f, 104.f, Stone);
            C.Slab(D, TEXT("CP_Ashlar"), U - 52.f, U - 40.f, 0.f, 6.f, 104.f, 196.f, Stone);
            C.Slab(D, TEXT("CP_Ashlar"), U + 40.f, U + 52.f, 0.f, 6.f, 104.f, 196.f, Stone);
            C.Slab(D, TEXT("CP_Ashlar"), U - 52.f, U + 52.f, 0.f, 6.f, 196.f, 212.f, Stone);
            C.Card(P.Glow, TEXT("CP_TicketBooth"), U, .6f, 150.f, 80.f, 92.f);
            C.Slab(S, TEXT("CP_Wood"), U - 46.f, U + 46.f, 0.f, 26.f, 100.f, 105.f);   // the counter
            for (int32 J = -3; J <= 3; ++J) C.Slab(D, TEXT("CP_Iron"), U + J * 11.f - 1.f, U + J * 11.f + 1.f, 3.f, 5.f, 112.f, 194.f);
            Plate(D, TEXT("CP_EnamelPurple"), TEXT("CP_EnamelGold"), TEXT("TICKETS"), C.At(U, 6.f, 232.f), ReadRight(C.N), C.N, 110.f, 30.f, 16.f);
        }
        // The stadium's name on the frieze at the three approaches; the dedication either side of the south one.
        const TCHAR* Frieze = (K == 0 || K == 20) ? TEXT("CP_FriezeNameShort") : K == 30 ? TEXT("CP_FriezeName") : K == 29 ? TEXT("CP_FriezeErected") : K == 31 ? TEXT("CP_FriezeDedicated") : nullptr;
        if (Frieze&&!HouseStadiumDetail::Aisle(K)) C.Card(D, Frieze, Mid, 3.2f, 985.f, C.Len - 40.f, 106.f);
    }
}

void PressBoxSteel(HouseCampusPolish::FParts& P)
{
    FHallMesh& D = P.Far;
    const float Outer = HouseCampus::StadiumY + 4050.f - 14.f, Under = G + 1106.f;
    auto WallY = [](float X) { const float T = (X - HouseCampus::StadiumX) / RX; return HouseCampus::StadiumY + RY * FMath::Sqrt(FMath::Max(0.f, 1.f - T * T)); };
    // An outrigger at every bay of the press box, its knee brace down to the wall; a steel beam along the outer edge.
    for (float X = HouseCampus::StadiumX - 2200.f; X <= HouseCampus::StadiumX + 2201.f; X += 440.f)
    {
        const float Wy = WallY(X);
        HouseCampusFallKit::Bar(D, TEXT("CP_Steel"), FVector(X, Wy + 2.f, Under - 8.f), FVector(X, Outer, Under - 8.f), 14.f, 20.f, FVector(1, 0, 0));
        HouseCampusFallKit::Bar(D, TEXT("CP_Steel"), FVector(X, Wy + 6.f, G + 770.f), FVector(X, FMath::Lerp(Wy, Outer, .72f), Under - 16.f), 10.f, 12.f, FVector(1, 0, 0));
        D.Box(TEXT("CP_SteelDark"), FTransform(FVector(X, Wy + 3.f, G + 770.f)), FVector(14.f, 3.f, 22.f), Plain());   // the shoe on the wall
    }
    HouseCampusFallKit::Bar(D, TEXT("CP_Steel"), FVector(HouseCampus::StadiumX - 2300.f, Outer, Under - 10.f), FVector(HouseCampus::StadiumX + 2300.f, Outer, Under - 10.f), 22.f, 20.f, FVector(0, 1, 0));
    // A column under the west end, where the overhang is deepest, on its pier, braced back to the wall.
    {
        const float X = HouseCampus::StadiumX - 2160.f, Y = Outer - 20.f;
        if (P.Clear(FVector2D(X, Y), 40.f))
        {
        P.Solid.Box(TEXT("CP_Footing"), FTransform(FVector(X, Y, G + 10.f)), FVector(30.f, 30.f, 16.f), Plain());
        P.Solid.Box(TEXT("CP_Steel"), FTransform(FVector(X, Y, (G + 26.f + Under) * .5f)), FVector(13.f, 13.f, (Under - G - 26.f) * .5f), Plain());
        HouseCampusFallKit::Bar(D, TEXT("CP_Steel"), FVector(X, Y - 10.f, G + 620.f), FVector(X, WallY(X) + 4.f, G + 960.f), 9.f, 9.f, FVector(1, 0, 0));
        }
    }
    // On the roof: two masts with their crossarms and dipoles, and the television dish.
    const float Roof = G + 1980.f;
    for (const FVector& M : { FVector(HouseCampus::StadiumX - 1300.f, HouseCampus::StadiumY + 3700.f, Roof), FVector(HouseCampus::StadiumX + 1650.f, HouseCampus::StadiumY + 3620.f, Roof) })
    {
        const bool bTall = M.X < HouseCampus::StadiumX;
        const float H = bTall ? 760.f : 480.f;
        D.Cylinder(TEXT("CP_Galv"), M, M + Up(H), 5.f, 8, Plain());
        D.Box(TEXT("CP_SteelDark"), FTransform(M + Up(4.f)), FVector(26.f, 26.f, 4.f), Plain());
        for (int32 L = 0; L < (bTall ? 4 : 2); ++L)
        {
            const float Z = H * (.45f + L * .16f), W = 90.f - L * 14.f;
            HouseCampusFallKit::Bar(D, TEXT("CP_Galv"), M + Up(Z) - FVector(W, 0, 0), M + Up(Z) + FVector(W, 0, 0), 3.f, 3.f, FVector(0, 1, 0));
            for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Bar(D, TEXT("CP_Galv"), M + Up(Z) + FVector(S * W, 0, 0), M + Up(Z) + FVector(S * W, -60.f, 0), 1.6f, 1.6f, FVector(0, 0, 1));
        }
        for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Bar(D, TEXT("CP_SteelDark"), M + Up(H * .7f), M + FVector(S * 260.f, 120.f, 2.f), .6f, .6f, FVector(0, 0, 1));   // guy wires
        if (!bTall)
        {
            // The dish, aimed south-west over the bowl.
            const FVector Hub = M + Up(H * .62f) + FVector(-30.f, -40.f, 0.f), Aim = FVector(-.5f, -.75f, .42f).GetSafeNormal();
            TArray<FVector2D> Prof;
            for (int32 J = 0; J <= 6; ++J) { const float R = J * 16.f; Prof.Add(FVector2D(R, R * R / 520.f)); }
            FHallMesh Dish;
            HouseCampusFallKit::Lathe(Dish, TEXT("CP_White"), FVector::ZeroVector, Prof, 18);
            const FQuat Q = FRotationMatrix::MakeFromZ(Aim).ToQuat();
            for (TPair<FName, FHallSection>& Part : Dish.Sections)
                for (int32 V = 0; V < Part.Value.V.Num(); ++V) { Part.Value.V[V] = Hub + Q.RotateVector(Part.Value.V[V]); Part.Value.N[V] = Q.RotateVector(Part.Value.N[V]); }
            AppendMesh(D, Dish);
            HouseCampusFallKit::Bar(D, TEXT("CP_Galv"), Hub, Hub + Aim * 70.f, 2.f, 2.f, FVector(0, 0, 1));
        }
    }
}

void FloodBank(HouseCampusPolish::FParts& P, const HouseCampusPolish::FFloodBank& B, float Deg)
{
    FHallMesh& D = P.Far;
    const FVector Base = Bowl(FMath::DegreesToRadians(Deg), 1.04f, 0.f), Top = Base + Up(3900.f);
    const FVector F = B.Facing, Flat = FVector(F.X, F.Y, 0.f).GetSafeNormal(), Right = FVector::CrossProduct(FVector::UpVector, Flat).GetSafeNormal();
    const FVector UpT = FVector::CrossProduct(F, Right).GetSafeNormal();   // the bank's own up, tilted with it
    // The frame: a rectangle of tube round the lamps, cross members between the rows, a yoke back to the pole.
    const FVector C = B.Centre - F * 30.f;
    const float HW = 250.f, HH = 150.f;
    auto At = [&](float X, float Y) { return C + Right * X + UpT * Y; };
    for (const float Y : { -HH, -50.f, 50.f, HH }) HouseCampusFallKit::Bar(D, TEXT("CP_Galv"), At(-HW, Y), At(HW, Y), 7.f, 7.f, F);
    for (const float X : { -HW, -125.f, 0.f, 125.f, HW }) HouseCampusFallKit::Bar(D, TEXT("CP_Galv"), At(X, -HH), At(X, HH), 7.f, 7.f, F);
    for (const float X : { -120.f, 120.f }) HouseCampusFallKit::Bar(D, TEXT("CP_Galv"), At(X, -HH * .3f), Top + Up(40.f), 9.f, 9.f, FVector::UpVector);
    // The lamps: a can, the lens (emission) and a visor over it.
    int32 DetailLens=0;
    for (const FVector& L : B.Lenses)
    {
        D.Cylinder(TEXT("CP_FloodCan"), L - F * 40.f, L - F * 1.f, 26.f, 14, Plain());
        FName LensKey(TEXT("CP_FloodLens"));if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=3){if(Deg==35.f&&DetailLens==0)LensKey=TEXT("SD_DeadBulb");if(Deg==145.f&&DetailLens==1)LensKey=TEXT("SD_FloodFlicker");}
        P.Glow.Cylinder(LensKey, L - F * 1.2f, L, 23.f, 14, Plain());++DetailLens;
        HouseCampusFallKit::Bar(D, TEXT("CP_FloodCan"), L + UpT * 28.f - F * 30.f, L + UpT * 30.f + F * 14.f, 56.f, 2.f, UpT);
    }
    // The catwalk below the bank: grating, toe boards, a rail round the front and sides.
    const float CZ = Top.Z - 150.f;
    const FVector CC = FVector(Top.X, Top.Y, CZ) + Flat * 70.f;
    D.Box(TEXT("CP_Grating"), FTransform(FRotationMatrix::MakeFromXY(Flat, Right).ToQuat(), CC), FVector(65.f, 275.f, 2.5f), Plain());
    for (const float Y : { -275.f, 275.f }) for (const float X : { -65.f, 65.f })
        HouseCampusFallKit::Bar(D, TEXT("CP_Galv"), CC + Flat * X + Right * Y, CC + Flat * X + Right * Y + Up(108.f), 4.f, 4.f, Flat);
    for (const float Z : { 55.f, 108.f })
    {
        HouseCampusFallKit::Bar(D, TEXT("CP_Galv"), CC + Flat * 65.f - Right * 275.f + Up(Z), CC + Flat * 65.f + Right * 275.f + Up(Z), 4.f, 4.f, FVector::UpVector);
        for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Bar(D, TEXT("CP_Galv"), CC - Flat * 65.f + Right * (S * 275.f) + Up(Z), CC + Flat * 65.f + Right * (S * 275.f) + Up(Z), 4.f, 4.f, FVector::UpVector);
    }
    for (float X = -60.f; X <= 61.f; X += 60.f) HouseCampusFallKit::Bar(D, TEXT("CP_SteelDark"), CC + Flat * X, FVector(Top.X, Top.Y, CZ - 60.f), 6.f, 6.f, FVector::UpVector);   // brackets
    // The ladder up the pole's back, its cage from 2.4 m up, and the junction box and conduit at its foot. The pole behind
    // the press box rises through the box (a cased column inside it), so its ladder starts on the box's roof.
    const bool bPressBox = FMath::IsNearlyEqual(Deg, 90.f);
    const float L0 = bPressBox ? 1960.f : 250.f, Cage0 = bPressBox ? 2100.f : 480.f;
    const FVector Back = -Flat, LBase = Base + Back * 34.f;
    for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Bar(D, TEXT("CP_Galv"), LBase + Right * (S * 21.f) + Up(L0), LBase + Right * (S * 21.f) + Up(CZ - G + 100.f), 5.f, 3.f, Back);
    for (float Z = L0 + 30.f; Z < CZ - G; Z += 30.f) HouseCampusFallKit::Bar(D, TEXT("CP_Galv"), LBase - Right * 21.f + Up(Z), LBase + Right * 21.f + Up(Z), 2.6f, 2.6f, FVector::UpVector);
    TArray<FVector> Hoop;
    for (int32 J = 0; J <= 8; ++J) { const float A = PI * J / 8.f; Hoop.Add(Right * (FMath::Cos(A) * 36.f) + Back * (FMath::Sin(A) * 40.f)); }
    for (float Z = Cage0; Z < CZ - G; Z += 110.f)
        for (int32 J = 0; J < 8; ++J) HouseCampusFallKit::Bar(D, TEXT("CP_Galv"), LBase + Hoop[J] + Up(Z), LBase + Hoop[J + 1] + Up(Z), 4.f, 1.2f, FVector::UpVector);
    for (const int32 J : { 0, 2, 4, 6, 8 }) HouseCampusFallKit::Bar(D, TEXT("CP_Galv"), LBase + Hoop[J] + Up(Cage0), LBase + Hoop[J] + Up(CZ - G - 20.f), 1.2f, 4.f, Back);
    // The footing and the junction box only where they leave the walks and the routes round the stadium clear.
    if (!P.Clear(FVector2D(Base), 60.f)) return;
    P.Solid.Cylinder(TEXT("CP_Footing"), Base - Up(4.f), Base + Up(38.f), 52.f, 16, Plain());
    const FVector Box = Base + Right * 38.f + Up(140.f);
    P.Solid.Box(TEXT("CP_JunctionBox"), FTransform(FRotationMatrix::MakeFromXY(Right, Flat).ToQuat(), Box), FVector(14.f, 24.f, 34.f), Plain());
    HouseCampusFallKit::Bar(D, TEXT("CP_SteelDark"), Box + Up(34.f), Base + Right * 24.f + Up(900.f), 4.f, 4.f, Flat);
    HouseCampusFallKit::Bar(D, TEXT("CP_SteelDark"), Box - Up(34.f), Base + Right * 38.f + Up(36.f), 4.f, 4.f, Flat);
    Plate(D, TEXT("CP_EnamelCream"), TEXT("CP_Incised"), TEXT("DANGER 480V"), Box + Right * 14.5f + Up(10.f), ReadRight(Right), Right, 40.f, 14.f, 4.f);
}

// ------------------------------------------------------------------ the apron
/** A ring of concrete round the wall where nothing else is paved (the plan's walks, roads and lots stay theirs). */
void Apron(HouseCampusPolish::FParts& P)
{
    constexpr int32 Cols = 3, Bands = 4;
    constexpr float BandD = 120.f, Top = G + 3.f, Bottom = G - 4.f;
    auto Corner = [](int32 K, float T, float Dd)
    {
        const FChord C(K, 1.f);
        const FVector A = C.A + VertexNormal(K) * Dd, B = C.B + VertexNormal(K + 1) * Dd;
        return FMath::Lerp(A, B, T);
    };
    const FBox2D Tower(FVector2D(4680.f, -9660.f), FVector2D(4980.f, -9080.f));
    TSet<int32> Cells;
    auto Id = [](int32 K, int32 Col, int32 Band) { return ((K + Seg) % Seg) * 100 + Col * 10 + Band; };
    for (int32 K = 0; K < Seg; ++K)
        for (int32 Col = 0; Col < Cols; ++Col)
            for (int32 Band = 0; Band < Bands; ++Band)
            {
                bool bOK = true;
                for (const float T : { 0.f, .5f, 1.f })
                    for (const float Dd : { 0.f, .5f, 1.f })
                    {
                        const FVector Q = Corner(K, (Col + T) / Cols, (Band + Dd) * BandD + (Band == 0 && Dd == 0.f ? 0.f : 0.f));
                        const FVector2D Q2(Q.X, Q.Y);
                        if (HouseCampusLayout::Occupied(Q2, 6.f) || HouseCampusLayout::OnTrafficLane(Q2) || Tower.ExpandBy(10.f).IsInside(Q2)) bOK = false;
                    }
                if (bOK) Cells.Add(Id(K, Col, Band));
            }
    const FHallUV UV = FHallUV::World(240.f);
    FRandomStream R(1927);
    for (const int32 Cell : Cells)
    {
        const int32 K = Cell / 100, Col = (Cell / 10) % 10, Band = Cell % 10;
        const FVector P00 = Corner(K, Col / float(Cols), Band * BandD), P10 = Corner(K, (Col + 1) / float(Cols), Band * BandD);
        const FVector P11 = Corner(K, (Col + 1) / float(Cols), (Band + 1) * BandD), P01 = Corner(K, Col / float(Cols), (Band + 1) * BandD);
        auto Z = [&](const FVector& V, float H) { return FVector(V.X, V.Y, H); };
        P.Solid.Quad(TEXT("CP_Apron"), Z(P00, Top), Z(P10, Top), Z(P11, Top), Z(P01, Top), FVector::UpVector, UV);
        // Joints along its edges (shared edges are drawn by both: a hairline wider), and the slab's edge where no neighbour is.
        const FVector Corners[4] = { P00, P10, P11, P01 };
        const int32 Neigh[4] = { Band > 0 ? Id(K, Col, Band - 1) : -1, Col + 1 < Cols ? Id(K, Col + 1, Band) : Id(K + 1, 0, Band), Band + 1 < Bands ? Id(K, Col, Band + 1) : -1, Col > 0 ? Id(K, Col - 1, Band) : Id(K - 1, Cols - 1, Band) };
        for (int32 E = 0; E < 4; ++E)
        {
            const FVector A = Corners[E], B = Corners[(E + 1) % 4];
            if (E != 0 || Band > 0) HouseCampusFallKit::Bar(P.Detail, TEXT("CP_Joint"), Z(A, Top + .06f), Z(B, Top + .06f), 1.4f, .1f, FVector::UpVector);
            if (Neigh[E] < 0 ? E != 0 : !Cells.Contains(Neigh[E]))
            {
                const FVector Out = (Z(A, 0.f) + Z(B, 0.f)) * .5f - (Z(P00, 0.f) + Z(P11, 0.f)) * .5f;
                P.Detail.Quad(TEXT("CP_Apron"), Z(A, Bottom), Z(B, Bottom), Z(B, Top), Z(A, Top), Out.GetSafeNormal(), UV);
            }
        }
        // A drain in the second band on every fifth bay.
        if (Band == 1 && Col == 1 && K % 5 == 2)
        {
            const FVector Mid = (P00 + P10 + P11 + P01) * .25f;
            const FChord C(K, 1.f);
            HouseCampusFallKit::Card(P.Detail, TEXT("CP_Grate"), Z(Mid, Top + .15f), C.Along, -C.N, 46.f, 46.f, FVector::UpVector);
        }
    }
}

/** A drift of leaves against the wall: a wedge of leaves deepest at the wall. */
void LeafDrift(FHallMesh& M, FRandomStream& R, const FVector& At, const FVector& Along, const FVector& Out, float Length, float Z)
{
    static const FName Keys[] = { TEXT("CP_LeafRed"), TEXT("CP_LeafOrange"), TEXT("CP_LeafBrown"), TEXT("CP_LeafYellow"), TEXT("CP_LeafBrown") };
    const int32 Count = int32(Length * .9f);
    for (int32 I = 0; I < Count; ++I)
    {
        const float U = R.FRandRange(-.5f, .5f) * Length, Dd = FMath::Square(R.FRand()) * 70.f;
        const float Hh = (1.f - Dd / 70.f) * (1.f - FMath::Square(2.f * U / Length)) * 9.f * R.FRand();
        const FVector C = At + Along * U + Out * (Dd + 3.f) + Up(Z - G + .4f + Hh);
        const FRotator Ro(R.FRandRange(-35.f, 35.f), R.FRandRange(0.f, 360.f), R.FRandRange(-35.f, 35.f));
        const FVector X = Ro.RotateVector(FVector(1, 0, 0)) * R.FRandRange(3.f, 5.f), Y = Ro.RotateVector(FVector(0, 1, 0)) * R.FRandRange(2.2f, 3.6f);
        M.Quad(Keys[R.RandRange(0, 4)], C - X - Y, C + X - Y, C + X + Y, C - X + Y, Ro.RotateVector(FVector::UpVector), Plain());
    }
}
/** An irregular flat patch (worn grass, mud) on the ground at Z. */
void Patch(FHallMesh& M, FName Key, FRandomStream& R, const FVector2D& C, float Rx, float Ry, float Yaw, float Z)
{
    TArray<FVector> Rim;
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    for (int32 K = 0; K < 20; ++K)
    {
        const float A = 2.f * PI * K / 20.f, Wob = .75f + .25f * R.FRand();
        Rim.Add(FVector(C, Z) + Q.RotateVector(FVector(FMath::Cos(A) * Rx * Wob, FMath::Sin(A) * Ry * Wob, 0.f)));
    }
    HouseCampusFallKit::Fan(M, Key, FVector(C, Z), Rim, FVector::UpVector);
}

/** Litter: a cup, a crumpled napkin, a popcorn box, a flattened can. Kind picks. */
void Litter(FHallMesh& M, FRandomStream& R, const FVector& At, int32 Kind)
{
    const float Yaw = R.FRandRange(0.f, 360.f);
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    switch (Kind % 4)
    {
    case 0: M.Cylinder(TEXT("CP_White"), At + Up(4.f) + Q.RotateVector(FVector(-6, 0, 0)), At + Up(4.f) + Q.RotateVector(FVector(6, 0, 0)), 4.f, 8, Plain()); break;   // a cup on its side
    case 1: HouseCampusFallKit::Ball(M, TEXT("CP_Paper"), At + Up(2.5f), 3.f, 5); break;
    case 2: M.Box(TEXT("CP_PopcornRed"), FTransform(Q, At + Up(5.f)), FVector(5.f, 4.f, 6.f), Plain()); break;
    default: M.Cylinder(R.FRand() < .5f ? TEXT("CP_CanRed") : TEXT("CP_CanSilver"), At + Up(1.5f) + Q.RotateVector(FVector(-5, 0, 0)), At + Up(1.5f) + Q.RotateVector(FVector(5, 0, 0)), 3.2f, 8, Plain()); break;   // crushed flat
    }
}
void Barrel(HouseCampusPolish::FParts& P, FRandomStream& R, const FVector& Base, int32 Style)
{
    static const FName Paint[] = { TEXT("CP_DrumPurple"), TEXT("CP_DrumGold"), TEXT("CP_DrumGrey") };
    P.Solid.Cylinder(Paint[Style % 3], Base, Base + Up(88.f), 29.f, 16, Plain());
    for (const float Z : { 28.f, 60.f }) P.Detail.Cylinder(Paint[Style % 3], Base + Up(Z), Base + Up(Z + 2.f), 30.f, 16, Plain());
    const FVector Face = FVector(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal2D();
    if (!Face.IsNearlyZero()) Letters(P.Detail, TEXT("CP_White"), TEXT("TRASH"), Base + Face * 29.6f + Up(46.f), ReadRight(Face), Face, 9.f, 1.6f);
    // Overflowing: a heap of cups, papers and boxes over the rim, more round the foot.
    for (int32 I = 0; I < 16; ++I) Litter(P.Detail, R, Base + FVector(R.FRandRange(-22.f, 22.f), R.FRandRange(-22.f, 22.f), 86.f + R.FRandRange(0.f, 14.f)), R.RandRange(0, 3));
    for (int32 I = 0; I < 7; ++I) { const float A = R.FRandRange(0.f, 2.f * PI), D = R.FRandRange(34.f, 80.f); Litter(P.Detail, R, Base + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f), R.RandRange(0, 3)); }
}
/** A chain-link barricade panel on its feet: frame, diamond mesh (it blocks like the frame), two rubber feet. */
void ChainLink(HouseCampusPolish::FParts& P, const FVector& A, const FVector& B)
{
    const FVector Along = (B - A).GetSafeNormal(), N(-Along.Y, Along.X, 0.f);
    const float Len = FVector::Dist(A, B), H = 180.f;
    for (const float Z : { 8.f, H }) HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Galv"), A + Up(Z), B + Up(Z), 4.f, 4.f, N);
    for (const float U : { 0.f, Len }) HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Galv"), A + Along * U + Up(8.f), A + Along * U + Up(H), 4.f, 4.f, N);
    for (float S = -H; S < Len; S += 9.f)
    {
        // The diamonds: two families of wires at 45 degrees, clipped to the frame.
        for (const float Dir : { 1.f, -1.f })
        {
            float U0 = S, Z0 = 8.f, U1 = S + (H - 8.f), Z1 = H;
            if (Dir < 0.f) { U0 = S + (H - 8.f); U1 = S; }
            auto Clip = [&](float& U, float& Z, float OU, float OZ) { if (U < 0.f) { Z += (0.f - U) * (OZ - Z) / (OU - U); U = 0.f; } if (U > Len) { Z += (Len - U) * (OZ - Z) / (OU - U); U = Len; } };
            float CU0 = U0, CZ0 = Z0, CU1 = U1, CZ1 = Z1;
            Clip(CU0, CZ0, U1, Z1); Clip(CU1, CZ1, U0, Z0);
            if (CU0 < 0.f || CU0 > Len || CU1 < 0.f || CU1 > Len || FMath::Abs(CU1 - CU0) < 1.f) continue;
            HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Galv"), A + Along * CU0 + Up(CZ0), A + Along * CU1 + Up(CZ1), .5f, .5f, N);
        }
    }
    for (const float U : { 0.f, Len }) P.Solid.Box(TEXT("CP_BusBlack"), FTransform(FRotationMatrix::MakeFromXY(N, Along).ToQuat(), A + Along * U + Up(6.f)), FVector(30.f, 9.f, 6.f), Plain());
}
/** A sawhorse barricade: two A-frame legs and the striped board. */
void Sawhorse(HouseCampusPolish::FParts& P, const FVector& C, float Yaw, FName Board)
{
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    auto L = [&](float X, float Y, float Z) { return C + Q.RotateVector(FVector(X, Y, Z)); };
    for (const float X : { -110.f, 110.f })
        for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Wood"), L(X, S * 32.f, 0.f), L(X, 0.f, 100.f), 5.f, 4.f, Q.RotateVector(FVector(1, 0, 0)));
    P.Solid.Box(TEXT("CP_Wood"), FTransform(Q, L(0.f, 0.f, 100.f)), FVector(125.f, 4.f, 4.f), Plain());
    for (const float S : { 1.f, -1.f })
    {
        const FVector N = Q.RotateVector(FVector(0, S, 0));
        HouseCampusFallKit::Card(P.Detail, Board, L(0.f, S * 3.f, 84.f), ReadRight(N), FVector::UpVector, 250.f, 30.f, N);
    }
    P.Detail.Box(TEXT("CP_Wood"), FTransform(Q, L(0.f, 0.f, 84.f)), FVector(125.f, 2.f, 15.f), Plain());
}
/** A shuttered concession stand: plywood box, the shutter down, the counter, its sign and its price board. */
void Stand(HouseCampusPolish::FParts& P, const FVector& C, const FVector& Facing, FName Sign, FName Prices)
{
    const FVector F = Facing.GetSafeNormal2D(), Rt = FVector::CrossProduct(FVector::UpVector, -F).GetSafeNormal();   // the reader's right
    const FQuat Q = FRotationMatrix::MakeFromXY(F, -Rt).ToQuat();
    P.Solid.Box(TEXT("CP_Plywood"), FTransform(Q, C + Up(120.f)), FVector(100.f, 150.f, 120.f), FHallUV::World(120.f));
    P.Detail.Box(TEXT("CP_TrimPurple"), FTransform(Q, C + Up(242.f)), FVector(108.f, 158.f, 4.f), Plain());
    P.Detail.Box(TEXT("CP_TrimPurple"), FTransform(Q, C + Up(4.f)), FVector(101.f, 151.f, 4.f), Plain());
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_Shutter"), C + F * 100.6f + Up(150.f), Rt, FVector::UpVector, 240.f, 110.f, F);
    P.Solid.Box(TEXT("CP_Wood"), FTransform(Q, C + F * 112.f + Up(96.f)), FVector(14.f, 130.f, 3.f), Plain());
    HouseCampusFallKit::Card(P.Detail, Sign, C + F * 101.f + Up(222.f), Rt, FVector::UpVector, 280.f, 52.f, F);
    HouseCampusFallKit::Card(P.Detail, Prices, C + F * 100.f + Rt * 165.f + Up(120.f), Rt, FVector::UpVector, 64.f, 80.f, F);   // propped beside the shutter
    P.Detail.Box(TEXT("CP_Wood"), FTransform(Q, C + F * 97.f + Rt * 165.f + Up(78.f)), FVector(3.f, 33.f, 41.f), Plain());
}
/** The souvenir table: folding table, pennants and buttons laid out, a rack of pennants hung, the sign, a cash box. */
void Souvenirs(HouseCampusPolish::FParts& P, FRandomStream& R, const FVector& C, float Yaw)
{
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    auto L = [&](float X, float Y, float Z) { return C + Q.RotateVector(FVector(X, Y, Z)); };
    P.Solid.Box(TEXT("CP_TableTop"), FTransform(Q, L(0, 0, 74)), FVector(91.f, 38.f, 2.f), Plain());
    for (const float X : { -80.f, 80.f }) for (const float Y : { -30.f, 30.f }) HouseCampusFallKit::Bar(P.Detail, TEXT("CP_Aluminum"), L(X, Y, 0), L(X, Y, 72), 2.f, 2.f, Q.RotateVector(FVector(1, 0, 0)));
    for (int32 I = 0; I < 9; ++I)
    {
        const FVector A = L(-75.f + I * 18.f, -20.f + (I % 2) * 18.f, 76.2f), Dir = Q.RotateVector(FVector(0.2f, 1.f, 0.f)).GetSafeNormal();
        const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir) * 6.f;
        HouseCampusFallKit::Tri(P.Detail, I % 2 ? TEXT("CP_StreamerGold") : TEXT("CP_StreamerPurple"), A - Side, A + Side, A + Dir * 30.f, FVector::UpVector);
    }
    for (int32 I = 0; I < 12; ++I) P.Detail.Cylinder(I % 3 ? TEXT("CP_EnamelPurple") : TEXT("CP_EnamelGold"), L(40.f + (I % 4) * 9.f, 8.f + (I / 4) * 9.f, 76.f), L(40.f + (I % 4) * 9.f, 8.f + (I / 4) * 9.f, 77.f), 3.5f, 10, Plain());
    P.Detail.Box(TEXT("CP_SteelDark"), FTransform(Q, L(-60.f, 22.f, 82.f)), FVector(14.f, 9.f, 6.f), Plain());   // the cash box
    // The rack behind: two posts and a rail, pennants hung from it.
    for (const float X : { -95.f, 95.f }) HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Wood"), L(X, 60.f, 0), L(X, 60.f, 190.f), 5.f, 5.f, Q.RotateVector(FVector(1, 0, 0)));
    HouseCampusFallKit::Bar(P.Detail, TEXT("CP_Wood"), L(-95.f, 60.f, 186.f), L(95.f, 60.f, 186.f), 4.f, 4.f, FVector::UpVector);
    for (int32 I = 0; I < 7; ++I)
    {
        const FVector Top = L(-80.f + I * 26.f, 60.f, 184.f), Side = Q.RotateVector(FVector(10.f, 0, 0));
        HouseCampusFallKit::Tri(P.Detail, I % 2 ? TEXT("CP_StreamerGold") : TEXT("CP_StreamerPurple"), Top - Side, Top + Side, Top - Up(48.f + R.FRandRange(0.f, 8.f)), Q.RotateVector(FVector(0, -1, 0)));
    }
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_SouvenirSign"), L(0.f, 58.f, 210.f), ReadRight(Q.RotateVector(FVector(0, -1, 0))), FVector::UpVector, 150.f, 56.f, Q.RotateVector(FVector(0, -1, 0)));
    HouseCampusFallKit::Bar(P.Detail, TEXT("CP_Wood"), L(0.f, 61.f, 186.f), L(0.f, 61.f, 238.f), 3.f, 3.f, Q.RotateVector(FVector(0, 1, 0)));
}
void Toilet(HouseCampusPolish::FParts& P, const FVector& C, float Yaw, int32 Index)
{
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    auto L = [&](float X, float Y, float Z) { return C + Q.RotateVector(FVector(X, Y, Z)); };
    const FVector F = Q.RotateVector(FVector(1, 0, 0)), Rt = Q.RotateVector(FVector(0, 1, 0));
    P.Solid.Box(TEXT("CP_ToiletBody"), FTransform(Q, L(0, 0, 112)), FVector(56.f, 56.f, 112.f), Plain());
    P.Detail.Box(TEXT("CP_ToiletRoof"), FTransform(Q, L(0, 0, 228)), FVector(60.f, 60.f, 5.f), Plain());
    P.Detail.Dome(TEXT("CP_ToiletRoof"), L(0, 0, 232), FVector::UpVector, 50.f, 14.f, 10);
    P.Detail.Cylinder(TEXT("CP_ToiletBody"), L(-30, -30, 230), L(-30, -30, 262), 4.f, 8, Plain());
    P.Detail.Box(TEXT("CP_ToiletBody"), FTransform(Q, L(57, 0, 100)), FVector(1.5f, 44.f, 95.f), Plain());   // the door
    P.Detail.Box(TEXT("CP_Vacant"), FTransform(Q, L(58.8f, 26, 120)), FVector(.6f, 6.f, 3.f), Plain());
    P.Detail.Box(TEXT("CP_Chrome"), FTransform(Q, L(59.f, 34, 98)), FVector(1.5f, 2.f, 8.f), Plain());
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_ToiletSign"), L(58.6f, 0, 178), ReadRight(F), FVector::UpVector, 40.f, 20.f, F);
    if (Index % 2) P.Detail.Box(TEXT("CP_LampRed"), FTransform(Q, L(58.8f, 26, 120)), FVector(.7f, 6.f, 3.f), Plain());   // OCCUPIED
    (void)Rt;
}

// ------------------------------------------------------------------ buses
/** A side profile (X back from the front bumper, Z up) extruded across a vehicle's width, centred on Front's line. */
void Profile(FHallMesh& M, FName Key, const FVector& Front, const FVector& F, const FVector& Rt, const TArray<FVector2D>& Outline, float HalfW)
{
    M.Prism(Key, Outline, Front - Rt * HalfW, F, FVector::UpVector, Rt, HalfW * 2.f, FHallUV::World(200.f));
}
void SchoolBus(HouseCampusPolish::FParts& P, const FVector& Front, const FVector& Facing, const TCHAR* Name)
{
    // A conventional school bus 10.9 m long: the hood and cowl ahead of the body, the body's rounded roof, rows of windows,
    // black rub rails, the name, dual rear wheels; the band's sign in a window.
    const FVector F = Facing.GetSafeNormal2D(), Rt = FVector::CrossProduct(FVector::UpVector, F).GetSafeNormal();
    const FQuat Q = FRotationMatrix::MakeFromXY(F, Rt).ToQuat();
    auto L = [&](float X, float Y, float Z) { return Front + F * X + Rt * Y + Up(Z); };   // X back from the bumper (negative), Y right
    constexpr float W = 122.f;
    Profile(P.Solid, TEXT("CP_BusYellow"), Front, F, Rt, { { -200.f, 42.f }, { -200.f, 282.f }, { -214.f, 304.f }, { -240.f, 313.f }, { -1062.f, 313.f }, { -1086.f, 304.f }, { -1090.f, 282.f }, { -1090.f, 42.f } }, W);
    Profile(P.Solid, TEXT("CP_BusYellow"), Front, F, Rt, { { -4.f, 50.f }, { -4.f, 118.f }, { -22.f, 143.f }, { -62.f, 152.f }, { -202.f, 156.f }, { -202.f, 50.f } }, 96.f);
    P.Detail.Box(TEXT("CP_BusBlack"), FTransform(Q, L(-2.f, 0.f, 68.f)), FVector(6.f, 116.f, 13.f), Plain());                 // bumper
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_BusBlack"), L(-4.6f, 0.f, 96.f), ReadRight(F), FVector::UpVector, 110.f, 40.f, F);   // grille
    for (const float S : { -1.f, 1.f })
    {
        P.Detail.Cylinder(TEXT("CP_Chrome"), L(-4.f, S * 70.f, 104.f), L(-2.f, S * 70.f, 104.f), 10.f, 12, Plain());           // headlamps (off)
        P.Detail.Cylinder(TEXT("CP_LampAmber"), L(-201.f, S * 100.f, 300.f), L(-199.f, S * 100.f, 300.f), 7.f, 10, Plain());   // roof warning lamps (off)
        P.Detail.Cylinder(TEXT("CP_LampRed"), L(-201.f, S * 70.f, 300.f), L(-199.f, S * 70.f, 300.f), 7.f, 10, Plain());
        HouseCampusFallKit::Bar(P.Detail, TEXT("CP_BusBlack"), L(-190.f, S * (W + 2.f), 230.f), L(-150.f, S * (W + 26.f), 230.f), 2.f, 2.f, FVector::UpVector);   // mirror arm
        P.Detail.Box(TEXT("CP_BusBlack"), FTransform(Q, L(-150.f, S * (W + 28.f), 225.f)), FVector(3.f, 9.f, 20.f), Plain());
        const FVector N = Rt * S;
        for (int32 J = 0; J < 9; ++J)
        {
            const float X = -260.f - J * 90.f;
            HouseCampusFallKit::Card(P.Detail, TEXT("CP_BusGlass"), L(X, S * (W + .5f), 238.f), ReadRight(N), FVector::UpVector, 74.f, 62.f, N);
        }
        if (S > 0.f) HouseCampusFallKit::Card(P.Detail, TEXT("CP_BusGlass"), L(-222.f, S * (W + .5f), 190.f), ReadRight(N), FVector::UpVector, 34.f, 230.f, N);   // the door's glass
        for (const float Z : { 120.f, 160.f, 196.f }) P.Detail.Box(TEXT("CP_BusBlack"), FTransform(Q, L(-645.f, S * (W + .8f), Z)), FVector(440.f, 1.f, 2.5f), Plain());
        Letters(P.Detail, TEXT("CP_BusBlack"), Name, L(-650.f, S * (W + 1.2f), 178.f), ReadRight(N), N, 15.f, 2.6f);
        if (S > 0.f) HouseCampusFallKit::Card(P.Detail, TEXT("CP_BandSign"), L(-570.f, S * (W + .9f), 238.f), ReadRight(N), FVector::UpVector, 160.f, 40.f, N);
        P.Solid.Cylinder(TEXT("CP_Tyre"), L(-120.f, S * 76.f, 50.f), L(-120.f, S * 100.f, 50.f), 50.f, 16, Plain());
        P.Solid.Cylinder(TEXT("CP_Tyre"), L(-860.f, S * 70.f, 50.f), L(-860.f, S * 124.f, 50.f), 50.f, 16, Plain());
        for (const float X : { -120.f, -860.f }) P.Detail.Cylinder(TEXT("CP_Chrome"), L(X, S * (S > 0.f ? 124.5f : 124.5f), 50.f), L(X, S * 125.5f, 50.f), 22.f, 12, Plain());
    }
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_BusGlass"), L(-199.5f, -50.f, 222.f), ReadRight(F), FVector::UpVector, 92.f, 104.f, F);   // windshield, two panes
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_BusGlass"), L(-199.5f, 50.f, 222.f), ReadRight(F), FVector::UpVector, 92.f, 104.f, F);
    Letters(P.Detail, TEXT("CP_BusBlack"), TEXT("CALDER STATE"), L(-199.f, 0.f, 290.f), ReadRight(F), F, 15.f, 2.6f);
    P.Detail.Box(TEXT("CP_BusYellow"), FTransform(Q, L(-640.f, 0.f, 316.f)), FVector(40.f, 34.f, 4.f), Plain());   // roof hatch
    for (const float S : { -1.f, 1.f }) P.Glow.Cylinder(TEXT("CP_LampRed"), L(-1091.f, S * 92.f, 110.f), L(-1090.5f, S * 92.f, 110.f), 8.f, 10, Plain());
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_BusGlass"), L(-1090.5f, 0.f, 190.f), ReadRight(-F), FVector::UpVector, 110.f, 200.f, -F);   // the emergency door
}
void Coach(HouseCampusPolish::FParts& P, const FVector& Front, const FVector& Facing)
{
    // A charter coach in a plain livery: white, the raked windscreen, the tinted window band, a thin maroon stripe, CHARTER
    // in its destination sign, the bug-eye mirrors.
    const FVector F = Facing.GetSafeNormal2D(), Rt = FVector::CrossProduct(FVector::UpVector, F).GetSafeNormal();
    const FQuat Q = FRotationMatrix::MakeFromXY(F, Rt).ToQuat();
    auto L = [&](float X, float Y, float Z) { return Front + F * X + Rt * Y + Up(Z); };
    constexpr float W = 127.f;
    Profile(P.Solid, TEXT("CP_CoachWhite"), Front, F, Rt, { { 0.f, 36.f }, { 0.f, 116.f }, { -6.f, 142.f }, { -26.f, 322.f }, { -52.f, 352.f }, { -104.f, 363.f }, { -1172.f, 363.f }, { -1196.f, 350.f }, { -1201.f, 322.f }, { -1201.f, 36.f } }, W);
    // The windscreen on the rake, the destination sign over it.
    {
        const FVector A0 = L(-6.5f, -W + 8.f, 145.f), A1 = L(-6.5f, W - 8.f, 145.f), B1 = L(-26.5f, W - 8.f, 318.f), B0 = L(-26.5f, -W + 8.f, 318.f);
        const FVector N = FVector::CrossProduct(A1 - A0, B0 - A0).GetSafeNormal() * (FVector::DotProduct(FVector::CrossProduct(A1 - A0, B0 - A0), F) < 0.f ? -1.f : 1.f);
        P.Detail.Quad(TEXT("CP_BusGlass"), A0 + N * .6f, A1 + N * .6f, B1 + N * .6f, B0 + N * .6f, N, Plain());
        HouseCampusFallKit::Bar(P.Detail, TEXT("CP_BusBlack"), (A0 + A1) * .5f + N, (B0 + B1) * .5f + N, 3.f, 1.f, N);
        HouseCampusFallKit::Card(P.Glow, TEXT("CP_DestSign"), L(-27.f, 0.f, 300.f) + N * 1.4f, ReadRight(N), (B0 - A0).GetSafeNormal(), 150.f, 26.f, N);
    }
    for (const float S : { -1.f, 1.f })
    {
        const FVector N = Rt * S;
        P.Detail.Box(TEXT("CP_BusGlass"), FTransform(Q, L(-640.f, S * (W + .4f), 268.f)), FVector(520.f, .6f, 60.f), Plain());
        for (float X = -230.f; X > -1150.f; X -= 140.f) P.Detail.Box(TEXT("CP_BusBlack"), FTransform(Q, L(X, S * (W + .8f), 268.f)), FVector(2.f, .5f, 60.f), Plain());
        P.Detail.Box(TEXT("CP_CoachStripe"), FTransform(Q, L(-600.f, S * (W + .6f), 192.f)), FVector(596.f, .6f, 4.f), Plain());
        P.Detail.Box(TEXT("CP_BusBlack"), FTransform(Q, L(-600.f, S * (W + .5f), 46.f)), FVector(598.f, .6f, 10.f), Plain());   // the rocker panel
        for (const float X : { -170.f, -880.f, -990.f }) P.Solid.Cylinder(TEXT("CP_Tyre"), L(X, S * 104.f, 52.f), L(X, S * 128.f, 52.f), 52.f, 16, Plain());
        for (const float X : { -170.f, -880.f, -990.f }) P.Detail.Cylinder(TEXT("CP_Chrome"), L(X, S * 128.f, 52.f), L(X, S * 129.f, 52.f), 24.f, 12, Plain());
        if (S > 0.f)
        {
            Letters(P.Detail, TEXT("CP_CoachStripe"), TEXT("DUNMORE"), L(-150.f, S * (W + .8f), 168.f), ReadRight(N), N, 12.f, 2.f);
            HouseCampusFallKit::Card(P.Detail, TEXT("CP_BusGlass"), L(-75.f, S * (W + .5f), 190.f), ReadRight(N), FVector::UpVector, 80.f, 280.f, N);   // the door
        }
        // Bug-eye mirrors on their arms.
        HouseCampusFallKit::Bar(P.Detail, TEXT("CP_BusBlack"), L(-30.f, S * (W - 10.f), 300.f), L(30.f, S * (W + 20.f), 250.f), 3.f, 3.f, FVector::UpVector);
        P.Detail.Box(TEXT("CP_BusBlack"), FTransform(Q, L(32.f, S * (W + 22.f), 236.f)), FVector(4.f, 9.f, 18.f), Plain());
        P.Detail.Cylinder(TEXT("CP_Chrome"), L(-2.f, S * 96.f, 92.f), L(.5f, S * 96.f, 92.f), 9.f, 10, Plain());
        P.Glow.Cylinder(TEXT("CP_LampRed"), L(-1201.5f, S * 100.f, 110.f), L(-1201.f, S * 100.f, 110.f), 8.f, 10, Plain());
    }
    P.Detail.Box(TEXT("CP_BusBlack"), FTransform(Q, L(2.f, 0.f, 64.f)), FVector(5.f, W - 4.f, 18.f), Plain());
    P.Detail.Box(TEXT("CP_BusBlack"), FTransform(Q, L(-1203.f, 0.f, 220.f)), FVector(2.f, 70.f, 80.f), Plain());   // the engine louvres at the back
}

// ------------------------------------------------------------------ the tailgates
void LawnChair(HouseCampusPolish::FParts& P, FRandomStream& R, const FVector& C, float Yaw, bool bTipped)
{
    // Solid (its own frame collides, as the coolers do): a chair you walk into, not through.
    static const FName Webs[] = { TEXT("CP_WebPurple"), TEXT("CP_WebGold"), TEXT("CP_WebGreen") };
    const FName Web = Webs[R.RandRange(0, 2)];
    const FRotator Ro(bTipped ? -80.f : 0.f, Yaw, 0.f);
    const FQuat Q = Ro.Quaternion();
    auto L = [&](float X, float Y, float Z) { return C + Q.RotateVector(FVector(X, Y, Z)) + Up(bTipped ? 26.f : 0.f); };
    for (const float Y : { -26.f, 26.f })
    {
        HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Aluminum"), L(-22.f, Y, 0.f), L(24.f, Y, 38.f), 2.2f, 2.2f, Q.RotateVector(FVector(0, 1, 0)));
        HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Aluminum"), L(24.f, Y, 0.f), L(-22.f, Y, 38.f), 2.2f, 2.2f, Q.RotateVector(FVector(0, 1, 0)));
        HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Aluminum"), L(-22.f, Y, 38.f), L(-30.f, Y, 86.f), 2.2f, 2.2f, Q.RotateVector(FVector(0, 1, 0)));
        HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Aluminum"), L(-20.f, Y, 58.f), L(20.f, Y, 58.f), 2.2f, 2.2f, Q.RotateVector(FVector(0, 0, 1)));   // arm
    }
    for (int32 J = 0; J < 5; ++J) HouseCampusFallKit::Bar(P.Solid, Web, L(-20.f + J * 10.f, -26.f, 38.f), L(-20.f + J * 10.f, 26.f, 38.f), 6.f, .5f, Q.RotateVector(FVector(0, 0, 1)));
    for (int32 J = 0; J < 5; ++J) HouseCampusFallKit::Bar(P.Solid, Web, L(-24.f - J * 1.3f, -26.f, 46.f + J * 9.f), L(-24.f - J * 1.3f, 26.f, 46.f + J * 9.f), 6.f, .5f, Q.RotateVector(FVector(1, 0, .15f)));
}
void Cooler(HouseCampusPolish::FParts& P, const FVector& C, float Yaw, bool bRed, bool bOpen)
{
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    P.Solid.Box(bRed ? TEXT("CP_CoolerRed") : TEXT("CP_CoolerGreen"), FTransform(Q, C + Up(18.f)), FVector(30.f, 19.f, 18.f), Plain());
    if (bOpen) P.Detail.Box(TEXT("CP_White"), FTransform(Q * FRotator(0.f, 0.f, 75.f).Quaternion(), C + Q.RotateVector(FVector(0, 22.f, 0)) + Up(44.f)), FVector(30.f, 19.f, 3.f), Plain());
    else P.Detail.Box(TEXT("CP_White"), FTransform(Q, C + Up(38.f)), FVector(31.f, 20.f, 3.f), Plain());
}
void Canopy(HouseCampusPolish::FParts& P, const FVector& C, float Yaw)
{
    // A pop-up canopy 3 m square: four legs and a peaked roof in purple and gold panels, a scalloped valance.
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    auto L = [&](float X, float Y, float Z) { return C + Q.RotateVector(FVector(X, Y, Z)); };
    const float H = 210.f, Peak = 270.f, S = 150.f;
    for (const float X : { -S, S }) for (const float Y : { -S, S }) HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Aluminum"), L(X, Y, 0.f), L(X, Y, H), 3.f, 3.f, Q.RotateVector(FVector(1, 0, 0)));
    const FVector Corners[4] = { L(-S, -S, H), L(S, -S, H), L(S, S, H), L(-S, S, H) };
    for (int32 E = 0; E < 4; ++E)
    {
        const FVector A = Corners[E], B = Corners[(E + 1) % 4], Top = L(0.f, 0.f, Peak);
        const FVector N = FVector::CrossProduct(B - A, Top - A).GetSafeNormal() * (FVector::DotProduct(FVector::CrossProduct(B - A, Top - A), FVector::UpVector) < 0.f ? -1.f : 1.f);
        HouseCampusFallKit::Tri(P.Detail, E % 2 ? TEXT("CP_CanopyGold") : TEXT("CP_CanopyPurple"), A, B, Top, N);
        HouseCampusFallKit::Tri(P.Detail, E % 2 ? TEXT("CP_CanopyGold") : TEXT("CP_CanopyPurple"), A, Top, B, -N);
        const FVector Out = ((A + B) * .5f - L(0, 0, H)).GetSafeNormal();
        for (int32 J = 0; J < 6; ++J)
        {
            const FVector V0 = FMath::Lerp(A, B, J / 6.f), V1 = FMath::Lerp(A, B, (J + 1) / 6.f), Vm = (V0 + V1) * .5f - Up(26.f);
            HouseCampusFallKit::Tri(P.Detail, E % 2 ? TEXT("CP_CanopyPurple") : TEXT("CP_CanopyGold"), V0, V1, Vm, Out);
            HouseCampusFallKit::Tri(P.Detail, E % 2 ? TEXT("CP_CanopyPurple") : TEXT("CP_CanopyGold"), V0, Vm, V1, -Out);
        }
    }
}
void Grill(HouseCampusPolish::FParts& P, const FVector& C)
{
    // A kettle grill on three legs, its lid hung on the side, coals still glowing, smoke going up.
    const FVector Bowl0 = C + Up(66.f);
    HouseCampusFallKit::Lathe(P.Solid, TEXT("CP_GrillBlack"), Bowl0, { FVector2D(.1f, -22.f), FVector2D(14.f, -20.f), FVector2D(23.f, -10.f), FVector2D(26.f, 4.f), FVector2D(25.f, 6.f) }, 18);
    for (int32 J = 0; J < 3; ++J) { const float A = J * 2.f * PI / 3.f; HouseCampusFallKit::Bar(P.Solid, TEXT("CP_GrillBlack"), Bowl0 + FVector(FMath::Cos(A) * 18.f, FMath::Sin(A) * 18.f, -14.f), C + FVector(FMath::Cos(A) * 32.f, FMath::Sin(A) * 32.f, 0.f), 2.f, 2.f, FVector::UpVector); }
    P.Glow.Cylinder(TEXT("CP_Embers"), Bowl0 - Up(2.f), Bowl0 - Up(1.f), 20.f, 16, Plain());
    for (int32 J = 0; J < 12; ++J) HouseCampusFallKit::Ball(P.Detail, TEXT("CP_Ash"), Bowl0 + FVector(FMath::Cos(J * 2.4f) * (J % 4) * 4.f, FMath::Sin(J * 2.4f) * (J % 4) * 4.f, 0.f), 2.5f, 5);
    HouseCampusFallKit::Lathe(P.Solid, TEXT("CP_GrillBlack"), Bowl0 + FVector(30.f, 0.f, -40.f), { FVector2D(25.f, 0.f), FVector2D(22.f, 10.f), FVector2D(12.f, 18.f), FVector2D(.1f, 20.f) }, 14);
    for (int32 J = 0; J < 5; ++J) P.Puffs.Add({ Bowl0 + Up(30.f + J * 55.f) + FVector(J * 9.f, J * 4.f, 0.f), 70.f + J * 40.f, FLinearColor(.45f, .44f, .42f, .22f - J * .03f), 1 });
}
void ShoePolish(HouseCampusPolish::FParts& P, const HouseCampusPolish::FCar& Car, const TCHAR* Text, int32 Seed)
{
    // White shoe-polish on both side windows, hand-lettered.
    const FBox& B = Car.Box;
    const float Len = B.Max.X - B.Min.X, Ht = B.Max.Z - B.Min.Z;
    for (const float S : { -1.f, 1.f })
    {
        const float Y = S > 0.f ? B.Max.Y - .02f * (B.Max.Y - B.Min.Y) : B.Min.Y + .02f * (B.Max.Y - B.Min.Y);
        const FVector N = Car.T.TransformVectorNoScale(FVector(0, S, 0)).GetSafeNormal(), Along = ReadRight(N);
        const float Cap = FMath::Min(16.f, Len * .32f / FMath::Max(1, FCString::Strlen(Text)) * 1.2f);
        const float WinZ = B.Min.Z + (Ht < 150.f ? Ht * .74f : Ht < 180.f ? Ht * .6f : Ht * .62f);
        const FVector C = Car.T.TransformPosition(FVector(B.Min.X + Len * .52f, Y * .94f, WinZ)) + N * 1.f;
        const float W = FCString::Strlen(Text) * 5.f * Cap / 6.f;
        HouseCampusFallKit::GlyphStrips(P.Detail, TEXT("CP_ShoePolish"), Text, C - Along * (W * .5f) - FVector(0, 0, Cap * .5f), Along, FVector::UpVector, N, Cap, Cap * .2f, Seed);
    }
}
void AntennaStreamers(HouseCampusPolish::FParts& P, const HouseCampusPolish::FCar& Car)
{
    const FBox& B = Car.Box;
    const FVector Root = Car.T.TransformPosition(FVector(B.Min.X + (B.Max.X - B.Min.X) * .22f, B.Min.Y * .82f, B.Min.Z + (B.Max.Z - B.Min.Z) * .62f));
    const FVector Tip = Root + Up(95.f);
    HouseCampusFallKit::Bar(P.Detail, TEXT("CP_Chrome"), Root, Tip, .6f, .6f, FVector(1, 0, 0));
    P.Flutters.Add({ Tip, Car.T.TransformVectorNoScale(FVector(-1, 0, 0)), 70.f, 4.f, TEXT("CP_StreamerPurple"), false });
    P.Flutters.Add({ Tip - Up(4.f), Car.T.TransformVectorNoScale(FVector(-1, .3f, 0)).GetSafeNormal(), 62.f, 4.f, TEXT("CP_StreamerGold"), false });
}
}

// ------------------------------------------------------------------ the flood banks (also read by HouseCampusFall's halos)
void HouseCampusPolish::FloodBanks(TArray<FFloodBank>& Out)
{
    namespace SI = HouseCampusPolishStadiumImpl;
    for (const float Deg : { 35.f, 90.f, 145.f })
    {
        const FVector Top = SI::Bowl(FMath::DegreesToRadians(Deg), 1.04f, 3900.f);
        const FVector Flat = (FVector(HouseCampus::StadiumX, HouseCampus::StadiumY, Top.Z) - Top).GetSafeNormal2D();
        FFloodBank B;
        B.Facing = (Flat * FMath::Cos(FMath::DegreesToRadians(24.f)) - FVector::UpVector * FMath::Sin(FMath::DegreesToRadians(24.f))).GetSafeNormal();
        B.Centre = Top + Flat * 60.f + SI::Up(150.f);
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Flat).GetSafeNormal(), UpT = FVector::CrossProduct(B.Facing, Right).GetSafeNormal();
        for (int32 Row = 0; Row < 3; ++Row)
            for (int32 Col = 0; Col < 4; ++Col)
                B.Lenses.Add(B.Centre + Right * ((Col - 1.5f) * 125.f) + UpT * ((Row - 1.f) * 100.f));
        Out.Add(B);
    }
}

void HouseCampusPolish::Stadium(FParts& P)
{
    namespace SI = HouseCampusPolishStadiumImpl;
    // What solid props keep off: the free-roam route round the north-east (HouseFreeRoamReview), the walk-in stadium's
    // route outside (beside and under the press box, to GATE 5), the stair tower's door, the gates' fronts.
    {
        const TArray<FVector2D> Route = { { 8575.f, -13300.f }, { 7851.f, -11731.f }, { 6886.f, -10637.f }, { 5483.f, -9804.f }, { 4300.f, -8800.f }, { 2400.f, -8650.f }, { 2400.f, -6700.f } };
        for (int32 I = 0; I + 1 < Route.Num(); ++I)
            for (float T = 0.f; T <= 1.f; T += 100.f / FMath::Max(100.f, FVector2D::Distance(Route[I], Route[I + 1]))) P.KeepClear.Add(FVector(FMath::Lerp(Route[I], Route[I + 1], T), 220.f));
        for (float X = 1700.f; X <= 4800.f; X += 100.f) P.KeepClear.Add(FVector(X, -9200.f, 260.f));
        P.KeepClear.Add(FVector(4600.f, -9500.f, 300.f));
        for (int32 K = 0; K < SI::Seg; ++K) if (SI::IsGate(K)||HouseStadiumDetail::Portal(K)) { const SI::FArchFrame A(K); P.KeepClear.Add(FVector(FVector2D(A.Centre + A.N * 200.f), 320.f)); }
    }
    SI::Facade(P);
    SI::PressBoxSteel(P);
    {
        TArray<FFloodBank> Banks;
        FloodBanks(Banks);
        const float Degs[] = { 35.f, 90.f, 145.f };
        for (int32 I = 0; I < Banks.Num(); ++I) SI::FloodBank(P, Banks[I], Degs[I]);
    }
    SI::Apron(P);
    FRandomStream R(1018);
    // Leaf drifts against the wall, mud and worn grass where people crossed.
    for (int32 K = 0; K < SI::Seg; ++K)
    {
        const SI::FChord C(K, 1.f);
        for (int32 J = 0; J < 2; ++J)
        {
            const float U = R.FRandRange(.15f, .85f) * C.Len;
            const FVector At = C.At(U, 0.f, 0.f);
            if (R.FRand() < .35f || (SI::IsGate(K) && FMath::Abs(U - C.Len * .5f) < 200.f)) continue;
            if (HouseCampusLayout::Occupied(FVector2D(At + C.N * 40.f), 0.f)) continue;
            SI::LeafDrift(P.Detail, R, At, C.Along, C.N, R.FRandRange(80.f, 220.f), SI::G + 3.f);
        }
    }
    {
        FRandomStream M(77);
        const FVector2D Mud[] = { { 1260.f, -9420.f }, { 3560.f, -9440.f }, { 2050.f, -17320.f }, { 3600.f, -17300.f }, { 7300.f, -12300.f } };
        for (const FVector2D& C : Mud) if (!HouseCampusLayout::Occupied(C, 40.f)) SI::Patch(P.Detail, TEXT("CP_Mud"), M, C, M.FRandRange(70.f, 120.f), M.FRandRange(40.f, 70.f), M.FRandRange(0.f, 180.f), SI::GroundZAt(C) + .35f);
        const FVector2D Worn[] = { { 7700.f, -11900.f }, { 7300.f, -11300.f }, { 1500.f, -17550.f }, { 4200.f, -17500.f }, { -2300.f, -14300.f }, { 6200.f, -16000.f }, { 5300.f, -9450.f }, { -400.f, -10900.f } };
        for (const FVector2D& C : Worn) if (!HouseCampusLayout::Occupied(C, 60.f) && !SI::OnApronRing(C)) SI::Patch(P.Detail, TEXT("CP_WornGrass"), M, C, M.FRandRange(140.f, 260.f), M.FRandRange(60.f, 110.f), M.FRandRange(0.f, 180.f), HouseExteriorSite::GroundHeight(C.X, C.Y) + .7f);
    }
    // The props round the apron (each only where it is clear of the plan's paving, the routes and the gates).
    auto Place = [&P](const FVector2D& At, float Radius) { return P.Clear(At, Radius); };
    {
        int32 Style = 0;
        const FVector2D Barrels[] = { { 1180.f, -9180.f }, { 3640.f, -9220.f }, { 950.f, -17420.f }, { 2700.f, -17430.f }, { 4400.f, -17380.f }, { -2600.f, -13900.f }, { 7600.f, -14400.f }, { 6500.f, -10700.f }, { -1500.f, -11300.f }, { 5600.f, -16700.f } };
        for (const FVector2D& B : Barrels) if (Place(B, 60.f)) SI::Barrel(P, R, FVector(B, SI::GroundZAt(B)), Style++);
    }
    {
        // Chain-link panels closing the service arch at GATE 12 (segment 24), a sawhorse in front of them; sawhorses at the
        // bus bay.
        const SI::FArchFrame A(24);
        const FVector C = A.Centre + A.N * 230.f;
        if (Place(FVector2D(C), 200.f))
        {
            SI::ChainLink(P, FVector(FVector2D(C - A.Along * 300.f), SI::G + 3.f), FVector(FVector2D(C), SI::G + 3.f));
            SI::ChainLink(P, FVector(FVector2D(C), SI::G + 3.f), FVector(FVector2D(C + A.Along * 300.f), SI::G + 3.f));
            SI::Sawhorse(P, FVector(FVector2D(C + A.N * 110.f), SI::G + 3.f), A.Along.Rotation().Yaw, TEXT("CP_Sawhorse"));
        }
        for (const float X : { 450.f, 4850.f }) if (SI::OffWalks(FVector2D(X, -18030.f), 160.f)) SI::Sawhorse(P, FVector(X, -18030.f, SI::LotZ), 90.f, TEXT("CP_SawhorseBus"));
    }
    {
        // The two concession stands, shuttered, facing out over the apron: on the south side by the bus bay and at the
        // south-east by the lot drive.
        const SI::FChord S1(28, 1.f), S2(33, 1.f);
        const FVector C1 = S1.At(S1.Len * .5f, 260.f, 3.f), C2 = S2.At(S2.Len * .5f, 260.f, 3.f);
        if (Place(FVector2D(C1), 190.f)) SI::Stand(P, C1, S1.N, TEXT("CP_StandSignA"), TEXT("CP_PriceBoardA"));
        if (Place(FVector2D(C2), 190.f)) SI::Stand(P, C2, S2.N, TEXT("CP_StandSignB"), TEXT("CP_PriceBoardB"));
        // The souvenir table off the ticket plaza's west end, the toilets on the south-east apron.
        if (Place(FVector2D(950.f, -9060.f), 120.f)) SI::Souvenirs(P, R, FVector(950.f, -9060.f, SI::GroundZAt(FVector2D(950.f, -9060.f))), 180.f);
        const SI::FChord T(35, 1.f);
        for (int32 I = 0; I < 4; ++I)
        {
            const FVector C = T.At(T.Len * .2f + I * 125.f, 330.f, 3.f);
            if (Place(FVector2D(C), 90.f)) SI::Toilet(P, C, T.N.Rotation().Yaw, I);
        }
    }
    // The band's two buses and Dunmore's coach in the bus bay, noses west toward the loop.
    SI::SchoolBus(P, FVector(700.f, -18025.f, SI::LotZ), FVector(-1, 0, 0), TEXT("CALDER STATE UNIVERSITY"));
    SI::SchoolBus(P, FVector(1950.f, -18025.f, SI::LotZ), FVector(-1, 0, 0), TEXT("CALDER STATE UNIVERSITY"));
    SI::Coach(P, FVector(3300.f, -18025.f, SI::LotZ), FVector(-1, 0, 0));
    // The South Lot after the tailgates: behind some of the parked cars, in the aisle.
    {
        TArray<const FCar*> Lot;
        for (const FCar& C : P.Cars)
        {
            const FVector L = C.T.GetLocation();
            if (L.X > -1300.f && L.X < 8200.f && L.Y > -25050.f && L.Y < -20550.f && C.Box.IsValid) Lot.Add(&C);
        }
        Lot.Sort([](const FCar& A, const FCar& B) { return A.T.GetLocation().X < B.T.GetLocation().X; });
        for (const FCar* C : Lot)
            UE_LOG(LogTemp, Verbose, TEXT("CAMPUS_POLISH_CAR at=%s scale=%s box=%s..%s"), *C->T.GetLocation().ToCompactString(), *C->T.GetScale3D().ToCompactString(), *C->Box.Min.ToCompactString(), *C->Box.Max.ToCompactString());
        static const TCHAR* Polish[] = { TEXT("GO GEESE"), TEXT("HONK!"), TEXT("BEAT DUNMORE"), TEXT("GEESE 24-17"), TEXT("HONK FOR THE GEESE"), TEXT("CLASS OF 61"), TEXT("GO GEESE GO") };
        int32 Party = 0;
        for (int32 I = 0; I < Lot.Num(); ++I)
        {
            const FCar& C = *Lot[I];
            SI::ShoePolish(P, C, Polish[I % UE_ARRAY_COUNT(Polish)], 40 + I);
            SI::AntennaStreamers(P, C);
            if (I % 2) continue;
            // The tailgate: behind the car (its back to the aisle), chairs round a cooler, cans, a canopy or a grill.
            const FVector Back = C.T.TransformVectorNoScale(FVector(-1, 0, 0)).GetSafeNormal2D(), Side = FVector::CrossProduct(FVector::UpVector, Back);
            const FVector Rear = C.T.TransformPosition(FVector(C.Box.Min.X, 0.f, 0.f));
            const FVector Behind = FVector(Rear.X, Rear.Y, SI::LotZ) + Back * 150.f;
            const FVector G0(Behind.X, Behind.Y, HouseCalderTerrain::SurfaceZ(Behind.X, Behind.Y, HouseCalderTerrain::EPave::Road));   // on the lot's paving
            if (!HouseCampusLayout::OnTrafficLane(FVector2D(G0)) || !SI::OffWalks(FVector2D(G0), 260.f)) continue;
            const float Yaw = Back.Rotation().Yaw;
            SI::LawnChair(P, R, G0 + Side * 90.f, Yaw - 120.f, false);
            SI::LawnChair(P, R, G0 - Side * 90.f, Yaw + 120.f, Party % 3 == 1);
            SI::LawnChair(P, R, G0 + Back * 90.f, Yaw + 180.f + R.FRandRange(-20.f, 20.f), false);
            SI::Cooler(P, G0 - Back * 10.f, Yaw + 90.f, Party % 2 == 0, Party % 3 == 0);
            for (int32 J = 0; J < 8; ++J) SI::Litter(P.Detail, R, G0 + FVector(R.FRandRange(-140.f, 140.f), R.FRandRange(-140.f, 140.f), 0.f), 3);
            if (Party % 3 == 0) SI::Canopy(P, G0 + Back * 20.f, Yaw);
            if (Party == 1) SI::Grill(P, G0 + Side * 160.f + Back * 40.f);
            ++Party;
        }
    }
}
