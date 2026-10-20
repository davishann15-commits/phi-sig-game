// The campus around the stadium on homecoming night: the two dorms with posters, curtains, string lights, jack-o'-lanterns
// on the sills and bedsheets hung out of the windows; the school's banners on every walkway lamp and the blue-green haze of
// their mercury-vapour lenses; a flyer kiosk on a little plaza off the walk, with a concrete path down to the gates and
// the chalk messages on it; a phone booth, newspaper boxes and a rack of ten-speeds on the walk; leaf piles, a rake and
// leaf bags under the trees; and the burned-down pep-rally bonfire still smoking on the green.
//
// On the Calder terrain (SourceArt/CalderLandscape, step 1c) the round kiosk plaza stays level by design (the landform's pad
// meets it); the gate path with its edging, joints, chalk and arrow, the bonfire's ash bed and the strays round the leaf
// piles drape over the relief where it is not flat, and are built as before where it is (Dormant: at zero relief,
// everywhere). The props stay level (the booth, the boxes and the rack on their base point).
#include "HouseCampusFallParts.h"
#include "HouseCampusFall.h"
#include "HouseCampusFallKit.h"
#include "HouseExteriorCampus.h"
#include "HouseCampusLayout.h"
#include "HouseCalderDrape.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorSiteLayout.h"

namespace HouseCampusFallQuadImpl
{
constexpr float Ground = -60.f;   // today's campus grade; every placement now reads the ground there (HouseExteriorSite::GroundHeight)
const FHallUV& Flat() { return HouseCampusFallKit::PlainUV(); }
FName Key(const TCHAR* Base, int32 Index) { return FName(*FString::Printf(TEXT("%s%d"), Base, Index)); }
FVector RightOf(const FVector& F) { return FVector(-F.Y, F.X, 0.).GetSafeNormal(); }
uint32 Mix(int32 A, int32 B, int32 C)
{
    uint32 H = uint32(A) * 73856093u ^ uint32(B) * 19349663u ^ uint32(C) * 83492791u;
    H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
    return H;
}

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
/** A box round a point (Half each way). */
FBox2D Around(const FVector2D& C, float Half) { return FBox2D(C - FVector2D(Half, Half), C + FVector2D(Half, Half)); }
/** Lays a piece drawn level (Level: chalk lettering) onto the ground: each of its triangles becomes a sheet at Z(X, Y), cut
 *  along the relief lattice so every piece of it lies on one plane of the ground. */
void DrapeFlat(FHallMesh& Into, const FHallMesh& Level, TFunctionRef<float(float, float)> Z)
{
    for (const FName& K : Level.Order)
    {
        const FHallSection& S = Level.Sections.FindChecked(K);
        for (int32 I = 0; I + 2 < S.I.Num(); I += 3)
            HouseCalderDrape::Sheet(Into, K, { FVector2D(S.V[S.I[I]]), FVector2D(S.V[S.I[I + 1]]), FVector2D(S.V[S.I[I + 2]]) }, Z, Flat());
    }
}

void Bead(FHallMesh& M, FName K, const FVector& C, float R)
{
    const FVector Px(R, 0, 0), Py(0, R, 0), Pz(0, 0, R * 1.3f);
    for (const float Sx : { -1.f, 1.f }) for (const float Sy : { -1.f, 1.f }) for (const float Sz : { -1.f, 1.f })
        HouseCampusFallKit::Tri(M, K, C + Px * Sx, C + Py * Sy, C + Pz * Sz, FVector(Sx, Sy, Sz).GetSafeNormal());
}

/** A jack-o'-lantern, with its glow as a small flickering halo. */
void Lantern(HouseCampusFallParts::FParts& P, FHallMesh& M, const FVector& Base, float R, float Yaw, int32 Style)
{
    const float H = R * 1.45f;
    HouseCampusFallKit::Pumpkin(M, TEXT("CF_Pumpkin"), TEXT("CF_Stem"), Base, R, H, 10, Yaw);
    HouseCampusFallKit::JackFace(M, Key(TEXT("CF_Candle"), Style % 3), Base, R * 1.005f, H, Yaw, Style);
    P.Halos.Add({ Base + FRotator(0.f, Yaw, 0.f).Vector() * (R * 1.6f) + FVector(0, 0, H * .5f), R * 9.f, FLinearColor(1.f, .5f, .15f, .3f), 1.f });
    ++P.Lanterns;
}

/** One fallen leaf lying on a surface facing Up. */
void Leaf(FHallMesh& M, FRandomStream& R, const FVector& At, const FVector& Up)
{
    static const TCHAR* const Keys[] = { TEXT("ST_LeafOrange"), TEXT("ST_LeafBrown"), TEXT("ST_LeafYellow"), TEXT("ST_LeafRed") };
    const float Pick = R.FRand();
    const FName K = Keys[Pick < .34f ? 0 : Pick < .62f ? 1 : Pick < .82f ? 2 : 3];
    FVector U, V;
    FHallMesh::PlanarAxes(Up, U, V);
    const float A = R.FRandRange(0.f, 2.f * PI), L = R.FRandRange(8.f, 14.f), W = L * R.FRandRange(.55f, .75f);
    const FVector D = U * FMath::Cos(A) + V * FMath::Sin(A), S = FVector::CrossProduct(Up, D);
    const FVector Tip = At + D * (L * .5f) + Up * R.FRandRange(.1f, .6f), Base = At - D * (L * .5f);
    const FVector Lf = At + S * (W * .5f) + Up * .15f, Rt = At - S * (W * .5f) + Up * .15f;
    HouseCampusFallKit::Tri(M, K, Base, Lf, Tip, Up);
    HouseCampusFallKit::Tri(M, K, Base, Tip, Rt, Up);
}

/** A raked mound of leaves: a low brown body under a few hundred loose leaves. */
void LeafPile(FHallMesh& M, FRandomStream& R, const FVector& C, float Radius, float Height)
{
    M.Dome(TEXT("ST_LeafBrown"), C, FVector::UpVector, Radius * .92f, Height * .9f, 16);
    const int32 Count = int32(Radius * 7.5f);
    for (int32 K = 0; K < Count; ++K)
    {
        const float A = R.FRandRange(0.f, 2.f * PI), Rr = Radius * FMath::Sqrt(R.FRand());
        const float T = Rr / Radius, Z = Height * (1.f - T * T);
        const FVector Up = FVector(FMath::Cos(A) * 2.f * T * Height / Radius, FMath::Sin(A) * 2.f * T * Height / Radius, 1.f).GetSafeNormal();
        Leaf(M, R, C + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, Z + .4f), Up);
    }
    // Strays blown off round the edge: over the relief (not Dormant) each lies on the ground under it, not at the pile's foot,
    // turned to the ground's slope there.
    const bool bDrape = !Dormant(Around(FVector2D(C), Radius * 2.2f + 10.f));
    for (int32 K = 0; K < Count / 3; ++K)
    {
        const float A = R.FRandRange(0.f, 2.f * PI), Rr = Radius * R.FRandRange(1.f, 2.2f);
        FVector At = C + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, .4f);
        if (bDrape) At.Z = HouseExteriorSite::GroundHeight(At.X, At.Y) + .4f;
        Leaf(M, R, At, bDrape ? HouseExteriorSite::GroundNormal(At.X, At.Y) : FVector::UpVector);
    }
}

// ------------------------------------------------------------------ the dorms
void Dorm(HouseCampusFallParts::FParts& P, const FBox2D& B, float Height, int32 Seed, int32& SheetKind)
{
    FHallMesh& M = P.Dorms;
    // The same window grid as HouseCampus::Windows on the +Y face (toward the street).
    const int32 Floors = FMath::Clamp(FMath::FloorToInt(Height / 420.f), 2, 7);
    const float Story = (Height - 50.f) / Floors, Z0 = Ground + HouseCampusLayout::BlockDZ(Seed) + 90.f;   // on the building's pad
    const float FaceY = B.Max.Y + 5.f, X0 = B.Min.X + 160.f, Length = (B.Max.X - 160.f) - X0;
    const FVector N(0, 1, 0), R(1, 0, 0);
    int32 Sheets = 0;
    for (int32 Floor = 0; Floor < Floors; ++Floor)
        for (int32 I = 0; I * 360 + 220 < Length; ++I)
        {
            const FVector Sill(X0 + I * 360.f + 87.5f, FaceY, Z0 + Floor * Story);   // bottom middle of the window
            const uint32 H = Mix(I, Floor, Seed);
            // Stone pilasters stand proud of the wall every 1200 from 65 in: where the nearest one is, relative to the window.
            const float FromCorner = Sill.X - B.Min.X;
            const float Pilaster = FMath::RoundToFloat((FromCorner - 65.f) / 1200.f) * 1200.f + 65.f - FromCorner;
            auto In = [&](float X, float Z, float Depth) { return Sill + R * X + FVector(0, 0, Z) + N * Depth; };
            const bool bLit = HouseCampusFall::WindowLit(I, Floor, Seed);
            if (bLit)
            {
                const int32 Kind = int32(H % 6);
                // Inside the glass (the pane is at +3, the mullions from +5): posters, curtains, a felt pennant, lights.
                if (Kind == 0 || Kind == 3 || Kind == 4)
                {
                    const float PX = Kind == 3 ? 30.f : -32.f, PZ = Kind == 3 ? 150.f : 55.f;
                    M.Quad(Key(TEXT("CF_Poster"), int32((H >> 4) % 4)), In(PX - 22.f, PZ - 32.f, 4.6f), In(PX + 22.f, PZ - 32.f, 4.6f), In(PX + 22.f, PZ + 32.f, 4.6f), In(PX - 22.f, PZ + 32.f, 4.6f), N, Flat());
                }
                if (Kind == 1)
                    for (const float S : { -1.f, 1.f })
                        M.Quad(TEXT("CF_Curtain"), In(S * 58.f, 12.f, 4.5f), In(S * 26.f, 12.f, 4.5f), In(S * 26.f, 200.f, 4.5f), In(S * 58.f, 200.f, 4.5f), N, Flat());
                if (Kind == 3) HouseCampusFallKit::Tri(M, Key(TEXT("CF_Pennant"), int32(H >> 7) % 2), In(-56.f, 70.f, 4.7f), In(-56.f, 40.f, 4.7f), In(-6.f, 55.f, 4.7f), N);
                if (Kind == 2 || Kind == 4)
                    for (int32 K = 0; K <= 8; ++K)
                    {
                        const float T = K / 8.f;
                        Bead(M, TEXT("CF_StringBulb"), In(-54.f + 108.f * T, 192.f - 12.f * 4.f * T * (1.f - T), 5.4f), 1.9f);
                    }
            }
            // A few rooms have run a string of lights round the outside of the frame.
            if (bLit && (H >> 20) % 6 == 0)
            {
                const FVector Ring[] = { In(-100.f, -6.f, 23.f), In(-100.f, 226.f, 23.f), In(100.f, 226.f, 23.f), In(100.f, -6.f, 23.f) };
                for (int32 Edge = 0; Edge < 3; ++Edge)
                    for (int32 K = 0; K < 9; ++K) Bead(M, Edge % 2 ? TEXT("CF_StringBulb") : Key(TEXT("CF_Chase"), K % 3), FMath::Lerp(Ring[Edge], Ring[Edge + 1], (K + .5f) / 9.f), 2.6f);
            }
            // Jack-o'-lanterns on the sill (the sill's top is 18 above the window's foot).
            if ((bLit && H % 5 == 0) || (!bLit && H % 11 == 0)) Lantern(P, M, In(Pilaster > 0.f ? -40.f : 40.f, 18.f, 13.f), 11.f, 90.f + float(int32(H >> 3) % 20 - 10), int32(H >> 12));
            // Bedsheets hung out under a few windows above the ground floor.
            if (Floor >= 1 && (H >> 16) % 5 < 2 && Sheets < 7 && FMath::Abs(Pilaster) > 135.f)
            {
                HouseCampusFallParts::FCloth C;
                const int32 Kinds[] = { 1, 5, 0, 3, 2 };
                C.Front = Key(TEXT("CF_Sheet"), Kinds[SheetKind++ % 5]); C.Back = TEXT("CF_SheetBack");
                C.Origin = Sill + N * 24.f - R * 104.f - FVector(0, 0, 4.f);
                C.Right = R; C.Down = -FVector::UpVector;
                C.Width = 208.f; C.Height = 150.f; C.Cols = 8; C.Rows = 5; C.Pinned = .35f; C.Amp = 1.1f;
                P.Cloths.Add(C);
                ++Sheets;
            }
        }
}

// ------------------------------------------------------------------ lamps: school banners and mercury haze
void Lamps(HouseCampusFallParts::FParts& P)
{
    FHallMesh& M = P.Lamps;
    auto Lamp = [&](float X, float Y)
    {
        // The two lamps the east stands were built over stay as they are.
        if (FMath::Square((X - HouseCampus::StadiumX) / 5900.f) + FMath::Square((Y - HouseCampus::StadiumY) / 4100.f) < 1.f) return;
        const float LampFoot = HouseExteriorSite::GroundHeight(X, Y);   // where the lamp's post stands (HouseExteriorCampus CampusLamp)
        for (const float Z : { 283.f, 447.f }) HouseCampusFallKit::Bar(M, TEXT("EX_Metal"), FVector(X + 5.f, Y, LampFoot + Z), FVector(X + 96.f, Y, LampFoot + Z), 3.f, 3.f, FVector::UpVector);
        M.Cylinder(TEXT("EX_Metal"), FVector(X + 96.f, Y, LampFoot + 440.f), FVector(X + 96.f, Y, LampFoot + 452.f), 3.f, 6, Flat());
        HouseCampusFallKit::Card(M, TEXT("CF_LampBanner0"), FVector(X + 52.f, Y + 1.2f, LampFoot + 365.f), FVector(1, 0, 0), FVector::UpVector, 80.f, 160.f, FVector(0, 1, 0));
        HouseCampusFallKit::Card(M, TEXT("CF_LampBanner1"), FVector(X + 52.f, Y - 1.2f, LampFoot + 365.f), FVector(-1, 0, 0), FVector::UpVector, 80.f, 160.f, FVector(0, -1, 0));
        P.Halos.Add({ FVector(X, Y, LampFoot + 503.f), 170.f, FLinearColor(.55f, .92f, .85f, .5f), 0.f });
        P.Halos.Add({ FVector(X, Y, LampFoot + 470.f), 680.f, FLinearColor(.35f, .65f, .62f, .08f), 0.f });
    };
    if (HouseCampusLayout::Enabled())
    {
        // The campus plan's walk lamps (HouseCampusLayout::LampPoints) get the same banners and haze.
        TArray<FVector2D> At; HouseCampusLayout::LampPoints(At);
        for (const FVector2D& L : At) Lamp(L.X, L.Y);
        return;
    }
    for (float X = -16000.f; X < 17000.f; X += 1900.f) Lamp(X, -5900.f);
    for (const float X : { -11300.f, 7300.f }) for (float Y = -9000.f; Y > -25000.f; Y -= 2800.f) Lamp(X, Y);
}

// ------------------------------------------------------------------ the plaza, the gate path and the chalk
void Kiosk(HouseCampusFallParts::FParts& P, const FVector& Base)
{
    FHallMesh& M = P.Green;
    constexpr float R = 52.f;
    constexpr int32 Seg = 20;
    M.Cylinder(TEXT("CF_Kiosk"), Base, Base + FVector(0, 0, 38.f), R + 5.f, Seg, Flat());
    // Layers of flyers round the drum; u runs clockwise seen from above so each reads left to right from outside.
    for (int32 K = 0; K < Seg; ++K)
    {
        const float A0 = 2.f * PI * K / Seg, A1 = 2.f * PI * (K + 1) / Seg;
        const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0.f), D1(FMath::Cos(A1), FMath::Sin(A1), 0.f);
        const FVector P0 = Base + D0 * R, P1 = Base + D1 * R;
        const float U0 = 2.f * (1.f - K / float(Seg)), U1 = 2.f * (1.f - (K + 1) / float(Seg));
        M.QuadUV(TEXT("CF_Flyers"), P0 + FVector(0, 0, 38.f), P1 + FVector(0, 0, 38.f), P1 + FVector(0, 0, 232.f), P0 + FVector(0, 0, 232.f), (D0 + D1).GetSafeNormal(),
            FVector2D(U0, 1.f), FVector2D(U1, 1.f), FVector2D(U1, 0.f), FVector2D(U0, 0.f), (P0 - P1).GetSafeNormal());
    }
    M.Cylinder(TEXT("CF_Kiosk"), Base + FVector(0, 0, 232.f), Base + FVector(0, 0, 252.f), R + 4.f, Seg, Flat());
    M.Cylinder(TEXT("CF_KioskCap"), Base + FVector(0, 0, 252.f), Base + FVector(0, 0, 258.f), R + 16.f, Seg, Flat());
    M.Dome(TEXT("CF_KioskCap"), Base + FVector(0, 0, 258.f), FVector::UpVector, R + 12.f, 46.f, Seg);
    HouseCampusFallKit::Ball(M, TEXT("CF_Gold"), Base + FVector(0, 0, 312.f), 8.f, 10);
    M.Cylinder(TEXT("CF_KioskCap"), Base + FVector(0, 0, 300.f), Base + FVector(0, 0, 306.f), 3.f, 6, Flat());
    P.Solid.Cylinder(TEXT("CF_Kiosk"), Base, Base + FVector(0, 0, 258.f), R + 6.f, 10, Flat());
}

void Chalk(FHallMesh& M, const FString& Text, const FVector& Centre, const FVector& Facing, float Cap, int32 Colour, int32 Seed)
{
    // Text on the ground, upright for someone walking along Facing; centred on Centre.
    const FVector Right = RightOf(Facing), Ahead = Facing.GetSafeNormal2D();
    const float Width = Text.Len() * 5.f * Cap / 6.f - Cap / 6.f;
    HouseCampusFallKit::GlyphStrips(M, Key(TEXT("CF_Chalk"), Colour), Text, Centre - Right * (Width * .5f) - Ahead * (Cap * .5f) + FVector(0, 0, .35f), Right, Ahead, FVector::UpVector, Cap, Cap * .13f, Seed);
}

/** The gate path's top over the ground there (3.6, as the plaza's), plus Above. */
float PathTopAt(float X, float Y, float Above) { return HouseExteriorSite::GroundHeight(X, Y) + 3.6f + Above; }

void GatePath(HouseCampusFallParts::FParts& P, const FVector& Kiosk)
{
    FHallMesh& M = P.Walks;
    // The round plaza is level by design, at the ground under the kiosk (the landform's pad meets it). The path is level at the
    // ground under its middle where the relief under it is flat (Dormant: today, everywhere); elsewhere it drapes over the
    // relief with its edging, joints, chalk and arrow, each at today's height over the ground (PathTopAt).
    const float Top = HouseExteriorSite::GroundHeight(Kiosk.X, Kiosk.Y) + 3.6f;
    // A round plaza about the kiosk off the main walk, and a straight path south to the gates.
    TArray<FVector> Rim;
    for (int32 K = 0; K < 32; ++K) { const float A = 2.f * PI * K / 32.f; Rim.Add(Kiosk + FVector(FMath::Cos(A) * 300.f, FMath::Sin(A) * 300.f, Top - Kiosk.Z)); }
    HouseCampusFallKit::Fan(M, TEXT("CF_Concrete"), FVector(Kiosk.X, Kiosk.Y, Top), Rim, FVector::UpVector);
    for (int32 K = 0; K < 32; ++K) HouseCampusFallKit::Bar(M, TEXT("CF_Concrete"), Rim[K] - FVector(0, 0, 2.f), Rim[(K + 1) % 32] - FVector(0, 0, 2.f), 8.f, 6.f, FVector::UpVector);
    const float X0 = Kiosk.X - 150.f, X1 = Kiosk.X + 150.f, Y0 = Kiosk.Y - 270.f, Y1 = -8930.f;
    const float PathTop = HouseExteriorSite::GroundHeight(Kiosk.X, (Y0 + Y1) * .5f) + 3.6f;
    const bool bDrape = !Dormant(FBox2D(FVector2D(X0 - 4.f, Y1), FVector2D(X1 + 4.f, Y0)));
    if (!bDrape)
        M.Quad(TEXT("CF_Concrete"), FVector(X0, Y0, PathTop - .1f), FVector(X1, Y0, PathTop - .1f), FVector(X1, Y1, PathTop - .1f), FVector(X0, Y1, PathTop - .1f), FVector::UpVector, Flat());
    else
        HouseCalderDrape::Sheet(M, TEXT("CF_Concrete"), HouseCalderDrape::RectOutline(FBox2D(FVector2D(X0, Y1), FVector2D(X1, Y0))), [](float Sx, float Sy) { return PathTopAt(Sx, Sy, -.1f); }, Flat());
    // Its edging: a bar 8 wide and 6 deep along each side (draped: a slab from 5 under the path's top to 1 over it).
    for (const float X : { X0, X1 })
    {
        if (!bDrape)
            HouseCampusFallKit::Bar(M, TEXT("CF_Concrete"), FVector(X, Y0, PathTop - 2.f), FVector(X, Y1, PathTop - 2.f), 8.f, 6.f, FVector::UpVector);
        else
            HouseCalderDrape::Slab(M, TEXT("CF_Concrete"), HouseCalderDrape::RunOutline(FVector2D(X, Y0), FVector2D(X, Y1), 8.f), [](float Sx, float Sy) { return PathTopAt(Sx, Sy, 1.f); },
                [](float Sx, float Sy) { return PathTopAt(Sx, Sy, -5.f); }, Flat());
    }
    // Expansion joints every few metres.
    for (float Y = Y0 - 180.f; Y > Y1; Y -= 180.f)
    {
        if (!bDrape)
            HouseCampusFallKit::Bar(M, TEXT("EX_Dark"), FVector(X0, Y, PathTop + .05f), FVector(X1, Y, PathTop + .05f), 1.5f, .1f, FVector::UpVector);
        else
            HouseCalderDrape::Sheet(M, TEXT("EX_Dark"), HouseCalderDrape::RunOutline(FVector2D(X0, Y), FVector2D(X1, Y), 1.5f), [](float Sx, float Sy) { return PathTopAt(Sx, Sy, .05f); }, Flat());
    }
    // Chalk from Friday's pep rally, readable walking down to the stadium (draped: each word drawn level, then laid on).
    const FVector South(0, -1, 0);
    const float CX = Kiosk.X;
    auto PathChalk = [&M, &South, PathTop, bDrape](const TCHAR* Text, float AtX, float AtY, float Cap, int32 Colour, int32 Seed)
    {
        if (!bDrape)
        {
            Chalk(M, Text, FVector(AtX, AtY, PathTop), South, Cap, Colour, Seed);
            return;
        }
        FHallMesh Level;
        Chalk(Level, Text, FVector(AtX, AtY, 0.f), South, Cap, Colour, Seed);
        DrapeFlat(M, Level, [](float Sx, float Sy) { return PathTopAt(Sx, Sy, .35f); });
    };
    PathChalk(TEXT("BEAT"), CX, -8380.f, 46.f, 0, 3);
    PathChalk(TEXT("DUNMORE!"), CX, -8450.f, 34.f, 2, 4);
    PathChalk(TEXT("GO GEESE"), CX - 10.f, -7650.f, 38.f, 1, 5);
    PathChalk(TEXT("HONK!"), CX + 30.f, -7050.f, 44.f, 3, 6);
    PathChalk(TEXT("HOMECOMING"), CX, -7950.f, 25.f, 0, 7);
    PathChalk(TEXT("86"), CX, -8010.f, 40.f, 2, 8);
    PathChalk(TEXT("CLASS OF 87"), CX - 5.f, -6800.f, 23.f, 3, 9);
    Chalk(M, TEXT("PEP RALLY FRI 8PM"), FVector(Kiosk.X, Kiosk.Y + 230.f, Top), FVector(0, 1, 0), 16.f, 0, 10);   // on the level plaza
    // An arrow toward the gates.
    const FVector A0(CX, -8620.f, PathTop + .4f), A1(CX, -8820.f, PathTop + .4f);
    if (!bDrape)
    {
        HouseCampusFallKit::Bar(M, TEXT("CF_Chalk2"), A0, A1, 6.f, .2f, FVector::UpVector);
        for (const float S : { -1.f, 1.f }) HouseCampusFallKit::Bar(M, TEXT("CF_Chalk2"), A1, A1 + FVector(S * 45.f, 50.f, 0), 6.f, .2f, FVector::UpVector);
    }
    else
    {
        auto ArrowZ = [](float Sx, float Sy) { return PathTopAt(Sx, Sy, .4f); };
        HouseCalderDrape::Sheet(M, TEXT("CF_Chalk2"), HouseCalderDrape::RunOutline(FVector2D(A0), FVector2D(A1), 6.f), ArrowZ, Flat());
        for (const float S : { -1.f, 1.f }) HouseCalderDrape::Sheet(M, TEXT("CF_Chalk2"), HouseCalderDrape::RunOutline(FVector2D(A1), FVector2D(A1) + FVector2D(S * 45.f, 50.f), 6.f), ArrowZ, Flat());
    }
    // A chalk goose on the (level) plaza: body, neck, head.
    const FVector G(Kiosk.X - 170.f, Kiosk.Y - 110.f, Top + .4f);
    TArray<FVector> Body;
    for (int32 K = 0; K <= 16; ++K) { const float A = 2.f * PI * K / 16.f; Body.Add(G + FVector(FMath::Cos(A) * 34.f, FMath::Sin(A) * 18.f, 0)); }
    for (int32 K = 0; K < 16; ++K) HouseCampusFallKit::Bar(M, TEXT("CF_Chalk0"), Body[K], Body[K + 1], 3.f, .2f, FVector::UpVector);
    HouseCampusFallKit::Bar(M, TEXT("CF_Chalk0"), G + FVector(-28.f, -8.f, 0), G + FVector(-48.f, -50.f, 0), 3.f, .2f, FVector::UpVector);
    HouseCampusFallKit::Bar(M, TEXT("CF_Chalk0"), G + FVector(-48.f, -50.f, 0), G + FVector(-66.f, -54.f, 0), 3.f, .2f, FVector::UpVector);
}

void NewsBox(HouseCampusFallParts::FParts& P, const FVector& Base, const FVector& Facing, FName Paint)
{
    FHallMesh& M = P.Green;
    const FVector F = Facing.GetSafeNormal2D(), S = RightOf(F);
    const FQuat Q = FRotationMatrix::MakeFromXZ(F, FVector::UpVector).ToQuat();
    M.Box(Paint, FTransform(Q, Base + FVector(0, 0, 61.f)), FVector(21.f, 25.f, 37.f), Flat());
    M.Box(Paint, FTransform(Q, Base + FVector(0, 0, 99.5f) + F * 1.f), FVector(22.f, 26.f, 1.5f), Flat());
    M.Box(TEXT("EX_Metal"), FTransform(Q, Base + FVector(0, 0, 12.f)), FVector(12.f, 14.f, 12.f), Flat());
    const FVector Face = Base + F * 21.3f;
    M.Quad(TEXT("CF_PhoneGlass"), Face - S * 19.f + FVector(0, 0, 54.f), Face + S * 19.f + FVector(0, 0, 54.f), Face + S * 19.f + FVector(0, 0, 90.f), Face - S * 19.f + FVector(0, 0, 90.f), F, Flat());
    HouseCampusFallKit::Card(M, TEXT("ST_NewsFront"), Face + F * .3f + FVector(0, 0, 72.f), -S, FVector::UpVector, 30.f, 33.f, F);
    HouseCampusFallKit::Card(M, TEXT("ST_NewsName"), Face + F * .3f + FVector(0, 0, 43.f), -S, FVector::UpVector, 40.f, 9.5f, F);
    M.Box(TEXT("CF_Chrome"), FTransform(Q, Face + F * 1.5f + S * 14.f + FVector(0, 0, 45.f)), FVector(1.5f, 4.f, 6.f), Flat());
    P.Solid.Box(Paint, FTransform(Q, Base + FVector(0, 0, 50.f)), FVector(22.f, 26.f, 50.f), Flat());
}

void PhoneBooth(HouseCampusFallParts::FParts& P, const FVector& Base, const FVector& Facing)
{
    FHallMesh& M = P.Green;
    const FVector F = Facing.GetSafeNormal2D(), S = RightOf(F), Z = FVector::UpVector;
    const FQuat Q = FRotationMatrix::MakeFromXZ(F, Z).ToQuat();
    constexpr float Half = 44.f, Tall = 226.f;
    for (const float A : { -1.f, 1.f }) for (const float B : { -1.f, 1.f })
        HouseCampusFallKit::Bar(M, TEXT("CF_PhoneFrame"), Base + F * (A * Half) + S * (B * Half), Base + F * (A * Half) + S * (B * Half) + Z * Tall, 6.f, 6.f, F);
    M.Box(TEXT("CF_PhoneFrame"), FTransform(Q, Base + Z * (Tall + 5.f)), FVector(Half + 6.f, Half + 6.f, 5.f), Flat());
    // Glass on the back and sides, kick panels below, the sign band round the top.
    auto Panel = [&](const FVector& Out, const FVector& Along)
    {
        const FVector C = Base + Out * Half;
        M.Box(TEXT("CF_PhoneFrame"), FTransform(FRotationMatrix::MakeFromXZ(Out, Z).ToQuat(), C + Z * 14.f), FVector(1.5f, Half - 3.f, 14.f), Flat());
        for (const float Side : { 1.f, -1.f })
            M.Quad(TEXT("CF_PhoneGlass"), C + Out * (Side * .6f) - Along * (Half - 3.f) + Z * 30.f, C + Out * (Side * .6f) + Along * (Half - 3.f) + Z * 30.f,
                C + Out * (Side * .6f) + Along * (Half - 3.f) + Z * 196.f, C + Out * (Side * .6f) - Along * (Half - 3.f) + Z * 196.f, Out * Side, Flat());
    };
    Panel(-F, S); Panel(S, F); Panel(-S, F);
    for (const FVector& Out : { F, -F, S, -S })
    {
        const FVector Along = RightOf(-Out);
        M.Box(TEXT("ST_PhoneSignBlue"), FTransform(FRotationMatrix::MakeFromXZ(Out, Z).ToQuat(), Base + Out * (Half + 1.f) + Z * 213.f), FVector(2.f, Half + 3.f, 11.f), Flat());
        const float Cap = 10.f, Width = 9.f * 5.f * Cap / 6.f - Cap / 6.f;
        HouseCampusFallKit::GlyphStrips(M, TEXT("ST_PhoneSignText"), TEXT("TELEPHONE"), Base + Out * (Half + 3.2f) - Along * (Width * .5f) + Z * 208.f, Along, Z, Out, Cap, 1.5f);
    }
    // The open bi-fold door, the phone on the back wall and the lit ceiling.
    for (const float Side : { -1.f, 1.f })
    {
        const FVector Hinge = Base + F * Half + S * (Side * Half), Leaf = (F * .8f - S * Side * .6f).GetSafeNormal();
        M.Quad(TEXT("CF_PhoneGlass"), Hinge + Z * 20.f, Hinge + Leaf * 40.f + Z * 20.f, Hinge + Leaf * 40.f + Z * 196.f, Hinge + Z * 196.f, FVector::CrossProduct(Leaf, Z) * Side, Flat());
        HouseCampusFallKit::Bar(M, TEXT("CF_PhoneFrame"), Hinge + Leaf * 40.f, Hinge + Leaf * 40.f + Z * 200.f, 3.f, 3.f, F);
    }
    M.Box(TEXT("CF_PhoneFrame"), FTransform(Q, Base - F * (Half - 9.f) + Z * 135.f), FVector(7.f, 15.f, 24.f), Flat());
    M.Box(TEXT("EX_Dark"), FTransform(Q, Base - F * (Half - 16.f) + Z * 138.f + S * 9.f), FVector(2.f, 3.f, 10.f), Flat());
    M.Box(TEXT("EX_Dark"), FTransform(Q, Base - F * (Half - 18.f) + Z * 95.f), FVector(12.f, 22.f, 1.5f), Flat());
    M.Quad(TEXT("EX_Window"), Base + Z * (Tall - 2.f) - F * 20.f - S * 20.f, Base + Z * (Tall - 2.f) + F * 20.f - S * 20.f, Base + Z * (Tall - 2.f) + F * 20.f + S * 20.f, Base + Z * (Tall - 2.f) - F * 20.f + S * 20.f, -Z, Flat());
    P.Halos.Add({ Base + Z * 190.f, 220.f, FLinearColor(.9f, .9f, .82f, .12f), 0.f });
    P.Solid.Box(TEXT("CF_PhoneFrame"), FTransform(Q, Base + Z * (Tall * .5f)), FVector(Half, Half, Tall * .5f), Flat());
}

void BikeRack(HouseCampusFallParts::FParts& P, const FVector& From, int32 Slots, FRandomStream& R)
{
    FHallMesh& M = P.Green;
    // An old comb rack: two rails and a row of uprights; ten-speeds nosed in from both sides.
    const float Length = (Slots - 1) * 55.f + 40.f;
    for (const float Z : { 8.f, 46.f }) HouseCampusFallKit::Bar(M, TEXT("EX_Metal"), From + FVector(-20.f, 0, Z), From + FVector(Length - 20.f, 0, Z), 3.f, 3.f, FVector::UpVector);
    for (int32 K = 0; K < Slots; ++K)
        for (const float S : { -4.f, 4.f })
        {
            const FVector Foot = From + FVector(K * 55.f + S, 0, 0);
            HouseCampusFallKit::Bar(M, TEXT("EX_Metal"), Foot, Foot + FVector(0, 0, 48.f), 2.f, 2.f, FVector::ForwardVector);
        }
    static const TCHAR* const Paints[] = { TEXT("CF_BikeRed"), TEXT("CF_BikeBlue"), TEXT("CF_BikeYellow"), TEXT("CF_BikeSilver") };
    for (int32 K = 0; K < Slots; ++K)
    {
        if (R.FRand() < .3f) continue;
        const float Side = R.FRand() < .5f ? 1.f : -1.f;
        // Front wheel in the comb, the rest of the bike out on its side of the rack.
        const FVector At = From + FVector(K * 55.f + R.FRandRange(-3.f, 3.f), Side * 40.f, 0);
        HouseCampusFallKit::TenSpeed(M, Paints[R.RandRange(0, 3)], TEXT("CF_Tyre"), TEXT("CF_Chrome"), At, Side > 0 ? -90.f + R.FRandRange(-4.f, 4.f) : 90.f + R.FRandRange(-4.f, 4.f));
    }
    P.Solid.BoxMinMax(TEXT("EX_Metal"), From + FVector(-30.f, -100.f, 0), From + FVector(Length, 100.f, 90.f), Flat());
}

// ------------------------------------------------------------------ the bonfire
void Bonfire(HouseCampusFallParts::FParts& P, const FVector& C, FRandomStream& R)
{
    FHallMesh& M = P.Green;
    // Over the relief (not Dormant) the ash bed (the scorch) drapes over the ground, and the stones, logs, embers and bales
    // stand at today's height over the ground under each (Seat); elsewhere all of it stands on the ground under the middle.
    const bool bDrape = !Dormant(Around(FVector2D(C), 400.f));
    auto Seat = [&C, bDrape](const FVector& At) { return bDrape ? FVector(At.X, At.Y, HouseExteriorSite::GroundHeight(At.X, At.Y) + (At.Z - C.Z)) : At; };
    // The ash bed inside a ring of fieldstones.
    TArray<FVector> Bed;
    for (int32 K = 0; K < 24; ++K) { const float A = 2.f * PI * K / 24.f; Bed.Add(C + FVector(FMath::Cos(A) * 158.f, FMath::Sin(A) * 158.f, 1.2f)); }
    if (!bDrape) HouseCampusFallKit::Fan(M, TEXT("CF_Ash"), C + FVector(0, 0, 6.f), Bed, FVector::UpVector);
    else
        for (int32 K = 0; K < 24; ++K)
        {
            // Each of the fan's triangles is a plane over the ground: 6 over it at the middle, 1.2 at the rim.
            const FVector2D Hub(C), E0(Bed[K]), E1(Bed[(K + 1) % 24]), Mid = (E0 + E1) * .5f - Hub;
            const double MidSq = Mid.SizeSquared();
            HouseCalderDrape::Sheet(M, TEXT("CF_Ash"), { Hub, E0, E1 }, [&Hub, &Mid, MidSq](float Sx, float Sy)
                { return HouseExteriorSite::GroundHeight(Sx, Sy) + 6.f - 4.8f * float(FVector2D::DotProduct(FVector2D(Sx, Sy) - Hub, Mid) / MidSq); }, Flat());
        }
    for (int32 K = 0; K < 16; ++K)
    {
        const float A = 2.f * PI * K / 16.f + R.FRandRange(-.06f, .06f);
        M.Box(TEXT("CF_Fieldstone"), FTransform(FRotator(R.FRandRange(-8.f, 8.f), A * 180.f / PI + R.FRandRange(-20.f, 20.f), R.FRandRange(-8.f, 8.f)), Seat(C + FVector(FMath::Cos(A) * 172.f, FMath::Sin(A) * 172.f, 10.f))),
            FVector(R.FRandRange(18.f, 26.f), R.FRandRange(14.f, 20.f), R.FRandRange(10.f, 15.f)), Flat());
    }
    // What's left of the stack: charred logs fallen inward, their ends still glowing, and embers in the ash.
    for (int32 K = 0; K < 11; ++K)
    {
        const float A = 2.f * PI * K / 11.f + R.FRandRange(-.2f, .2f);
        const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.f);
        const FVector Foot = C + Out * R.FRandRange(95.f, 140.f) + FVector(0, 0, 8.f), Head = C + Out * R.FRandRange(0.f, 30.f) + FVector(0, 0, R.FRandRange(18.f, 60.f));
        const FVector FootAt = Seat(Foot), HeadAt = Seat(Head);
        M.Cylinder(TEXT("CF_Char"), FootAt, HeadAt, R.FRandRange(7.f, 11.f), 7, Flat());
        M.Cylinder(Key(TEXT("CF_Ember"), K % 3), HeadAt - (HeadAt - FootAt).GetSafeNormal() * 2.f, HeadAt + (HeadAt - FootAt).GetSafeNormal() * 3.f, 6.5f, 7, Flat());
    }
    for (int32 K = 0; K < 70; ++K)
    {
        const float A = R.FRandRange(0.f, 2.f * PI), Rr = 120.f * FMath::Sqrt(R.FRand());
        M.Box(Key(TEXT("CF_Ember"), K % 3), FTransform(FRotator(R.FRandRange(0.f, 40.f), R.FRandRange(0.f, 360.f), 0.f), Seat(C + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, 6.5f))), FVector(R.FRandRange(3.f, 7.f), R.FRandRange(2.f, 5.f), 2.f), Flat());
    }
    P.Halos.Add({ C + FVector(0, 0, 45.f), 520.f, FLinearColor(1.f, .42f, .1f, .26f), 1.f });
    P.Halos.Add({ C + FVector(0, 0, 25.f), 220.f, FLinearColor(1.f, .5f, .15f, .4f), 1.f });
    // Bales dragged round for sitting.
    for (int32 K = 0; K < 4; ++K)
    {
        const float A = 2.f * PI * K / 4.f + .5f;
        HouseCampusFallKit::HayBale(M, TEXT("FJ_Straw"), TEXT("FJ_Twine"), Seat(C + FVector(FMath::Cos(A) * 330.f, FMath::Sin(A) * 330.f, 0.f)), A * 180.f / PI + 90.f);
    }
    P.Bonfire = C + FVector(0, 0, 30.f);
}
}

namespace HouseCampusFallStadiumImpl { FVector* Spots(); }
void HouseCampusFallParts::Campus(FParts& P)
{
    FRandomStream R(10181986);
    // The dorms: block 1 (west of the lot) and block 4 (east end), both facing the street. Under the campus plan they are
    // Harlow and Ashby halls (both still face north), built with their own plan seeds.
    int32 SheetKind = 0;
    if (HouseCampusLayout::Enabled())
    {
        for (const TCHAR* Id : { TEXT("HAR"), TEXT("ASH") })
        {
            const int32 Seed = HouseCampusLayout::Find(Id);
            const HouseCampusLayout::FBlock& D = HouseCampusLayout::Blocks()[Seed];
            HouseCampusFallQuadImpl::Dorm(P, D.Rect(), D.Height, Seed, SheetKind);
        }
    }
    else
    {
        HouseCampusFallQuadImpl::Dorm(P, FBox2D(FVector2D(-10600.f, -9200.f), FVector2D(-6800.f, -6600.f)), 2540.f, 1, SheetKind);
        HouseCampusFallQuadImpl::Dorm(P, FBox2D(FVector2D(12350.f, -10250.f), FVector2D(16650.f, -6650.f)), 2540.f, 4, SheetKind);
    }
    HouseCampusFallQuadImpl::Lamps(P);
    // The plaza with its kiosk, the walk furniture beside it, and the path to the gates.
    // Under the campus plan the promenade passes at Y -6500, so the kiosk and its round plaza stand a metre nearer the street.
    const float KioskY = HouseCampusLayout::Enabled() ? -6230.f : -6330.f;
    const FVector Kiosk(2400.f, KioskY, HouseExteriorSite::GroundHeight(2400.f, KioskY));
    HouseCampusFallQuadImpl::Kiosk(P, Kiosk);
    HouseCampusFallQuadImpl::GatePath(P, Kiosk);
    // Under the campus plan the mid-block crosswalk lands at X 2250..2550, so the news boxes stand west of the phone booth.
    // The booth and the boxes stand on the walk's top, the rack on the grass.
    const float NewsX = HouseCampusLayout::Enabled() ? -650.f : 0.f;
    auto OnWalk = [](float X, float Y) { return FVector(X, Y, HouseCalderTerrain::SurfaceZ(X, Y, HouseCalderTerrain::EPave::Walk)); };
    HouseCampusFallQuadImpl::PhoneBooth(P, OnWalk(1950.f, -6045.f), FVector(0, 1, 0));
    HouseCampusFallQuadImpl::NewsBox(P, OnWalk(2240.f + NewsX, -6060.f), FVector(0, 1, 0), TEXT("CF_NewsBlue"));
    HouseCampusFallQuadImpl::NewsBox(P, OnWalk(2310.f + NewsX, -6064.f), FVector(.05f, 1, 0), TEXT("CF_NewsRed"));
    HouseCampusFallQuadImpl::NewsBox(P, OnWalk(2378.f + NewsX, -6060.f), FVector(-.04f, 1, 0), TEXT("ST_NewsPaint"));
    const float RackY = HouseCampusLayout::Enabled() ? -6120.f : -6270.f;
    HouseCampusFallQuadImpl::BikeRack(P, FVector(1180.f, RackY, HouseExteriorSite::GroundHeight(1180.f, RackY) + .5f), 9, R);
    // Leaves raked into piles under the trees on the green and along the street row, a rake and the bags.
    int32 Piles = 0;
    for (const FVector& T : P.Trees)
    {
        if (T.X < -3000.f || T.X > 7600.f || T.Y > -5800.f || T.Y < -8700.f || Piles >= 7) continue;
        for (int32 K = 0; K < 2; ++K)
        {
            const float A = R.FRandRange(0.f, 2.f * PI), D = R.FRandRange(260.f, 420.f);
            const FVector2D At(T.X + FMath::Cos(A) * D, T.Y + FMath::Sin(A) * D);
            if (At.Y > -6150.f) continue;   // not on the walk
            if (HouseCampusLayout::Enabled() && HouseCampusLayout::Occupied(At, 130.f)) continue;   // nor on the plan's walks
            HouseCampusFallQuadImpl::LeafPile(P.Green, R, FVector(At, HouseExteriorSite::GroundHeight(At.X, At.Y)), R.FRandRange(70.f, 120.f), R.FRandRange(30.f, 52.f));
            ++Piles;
        }
        // Leaves fallen under the crown.
        for (int32 K = 0; K < 160; ++K)
        {
            const float A = R.FRandRange(0.f, 2.f * PI), D = 600.f * FMath::Sqrt(R.FRand());
            const FVector2D LeafAt(T.X + FMath::Cos(A) * D, T.Y + FMath::Sin(A) * D);
            const FVector At(LeafAt, HouseExteriorSite::GroundHeight(LeafAt.X, LeafAt.Y) + .5f);
            if (At.Y < -5880.f || At.Y > -5700.f) HouseCampusFallQuadImpl::Leaf(P.Green, R, At, FVector::UpVector);
        }
    }
    const FVector2D PileAt = P.FirstClear({ FVector2D(4300.f, -7050.f), FVector2D(4700.f, -6900.f), FVector2D(3300.f, -7300.f) }, 260.f);
    const FVector Pile(PileAt, HouseExteriorSite::GroundHeight(PileAt.X, PileAt.Y));
    // Over the relief (not Dormant) the second pile and the bags stand on the ground under each, not at the big pile's foot.
    const bool bPilesDrape = !HouseCampusFallQuadImpl::Dormant(HouseCampusFallQuadImpl::Around(PileAt, 460.f));
    auto OwnGround = [bPilesDrape](const FVector& At) { return bPilesDrape ? FVector(At.X, At.Y, HouseExteriorSite::GroundHeight(At.X, At.Y)) : At; };
    HouseCampusFallQuadImpl::LeafPile(P.Green, R, Pile, 115.f, 50.f);
    HouseCampusFallQuadImpl::LeafPile(P.Green, R, OwnGround(Pile + FVector(260.f, 120.f, 0)), 80.f, 34.f);
    {
        // The rake leaning on the bigger pile, and three full leaf bags.
        const FVector Grip = Pile + FVector(-60.f, 150.f, 128.f), Foot = Pile + FVector(-30.f, 40.f, 40.f);
        HouseCampusFallKit::Bar(P.Green, TEXT("CF_Rake"), Foot, Grip, 3.f, 3.f, FVector::UpVector);
        const FVector Along = (Foot - Grip).GetSafeNormal(), Across = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal();
        for (int32 K = -6; K <= 6; ++K) HouseCampusFallKit::Bar(P.Green, TEXT("CF_KioskCap"), Foot, Foot + Along * 30.f + Across * (K * 4.5f), 1.f, .8f, FVector::UpVector);
        for (int32 K = 0; K < 3; ++K)
        {
            const FVector Bag = OwnGround(Pile + FVector(-200.f - K * 52.f, -60.f + K * 18.f, 0));
            P.Green.Box(TEXT("CF_PaperBag"), FTransform(FRotator(0.f, 10.f * K, K == 2 ? 6.f : 0.f), Bag + FVector(0, 0, 42.f)), FVector(22.f, 15.f, 42.f), HouseCampusFallKit::PlainUV());
            P.Green.Box(TEXT("CF_PaperBag"), FTransform(FRotator(0.f, 10.f * K, 0.f), Bag + FVector(0, 0, 88.f)), FVector(19.f, 11.f, 5.f), HouseCampusFallKit::PlainUV());
        }
    }
    // The pep-rally bonfire, burned down and smoking, on open grass.
    const FVector2D FireAt = P.FirstClear({ FVector2D(3750.f, -7700.f), FVector2D(3900.f, -7350.f), FVector2D(6500.f, -7900.f) }, 520.f);
    HouseCampusFallStadiumImpl::Spots()[2] = FVector(FireAt, 520.f);
    HouseCampusFallQuadImpl::Bonfire(P, FVector(FireAt, HouseExteriorSite::GroundHeight(FireAt.X, FireAt.Y)), R);
    // Leaves come down over the front green: from the lowest ground under it (today's grade is -60 all over it) to 9 m over
    // the highest.
    const FBox2D LeafXY(FVector2D(-300.f, -8900.f), FVector2D(7200.f, -5600.f));
    P.LeafBox = FBox(FVector(LeafXY.Min, HouseCampusFallQuadImpl::Ground + HouseCalderTerrain::ReliefMin(LeafXY)), FVector(LeafXY.Max, 900.f + HouseCalderTerrain::ReliefMax(LeafXY)));
}
