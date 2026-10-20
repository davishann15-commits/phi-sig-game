// Calder State exterior polish, step 3c (street furniture): YOU ARE HERE maps of the campus at the main junctions (each
// painted from the plan itself, its own spot starred), fingerposts pointing the way to the buildings, a trash can by every
// bench, bike racks with ten-speeds at the busy doors, freestanding notice boards thick with flyers, and fire hydrants on
// the road verges. No blue-light phones (not yet, in 1986).
#include "HouseCampusPolish.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusFallKit.h"
#include "HouseCampusInteriorsRooms.h"
#include "HouseCampusLayout.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorCampus.h"
#include "HouseExteriorSiteLayout.h"
#include "Engine/Texture2D.h"

namespace HouseCampusPolishStreetImpl
{
// Today's grade: only the hydrants on Stadium Dr's south walk (the stadium's apron ring, held at today's height) keep it.
constexpr float G = -60.f;
const FHallUV& Plain() { return HouseCampusFallKit::PlainUV(); }
FVector Up(float Z) { return FVector(0.f, 0.f, Z); }
FVector ReadRight(const FVector& N) { return FVector::CrossProduct(FVector::UpVector, -N).GetSafeNormal(); }
float GroundAt(const FVector2D& P) { return HouseExteriorSite::GroundHeight(P.X, P.Y); }
/** The lowest ground under a thing's feet (its posts, its slots): where it stands level on a slope (the Calder terrain). */
float LowestGround(std::initializer_list<FVector2D> Feet)
{
    float Z = FLT_MAX;
    for (const FVector2D& F : Feet) Z = FMath::Min(Z, GroundAt(F));
    return Z;
}

/** The map signs: where each stands and which way its face looks. */
struct FMapSign { FVector2D At; float Yaw; };
const TArray<FMapSign>& MapSigns()
{
    static const TArray<FMapSign> S = { { FVector2D(2780.f, -6880.f), 90.f }, { FVector2D(-10650.f, -6880.f), 90.f }, { FVector2D(-14200.f, -12050.f), 0.f }, { FVector2D(3300.f, -17480.f), 90.f } };
    return S;
}
/** The campus painted from the plan: lawns, roads and lots, walks, the stadium, the buildings with their names' initials,
 *  the tower; north up; a red star where the reader stands. */
void PaintMap(FHallImage& Img, const FVector2D& Here)
{
    constexpr int32 W = 512, H = 384;
    const float X0 = -21000.f, X1 = 20000.f, Y1 = -5200.f, Scale = (X1 - X0) / W;
    // Seen from above with north (+Y) up, +X runs to the left (Unreal's axes are left-handed): the map as the campus is.
    auto Px = [&](float X, float Y) { return FVector2D((X1 - X) / Scale, (Y1 - Y) / Scale); };
    // Paving and roads from the plan's own tests, sampled once on a half-resolution grid and shared by every sign.
    static const TArray<uint8> Ground = [&]
    {
        TArray<uint8> G0;
        G0.SetNumZeroed((W / 2) * (H / 2));
        for (int32 Y = 0; Y < H / 2; ++Y)
            for (int32 X = 0; X < W / 2; ++X)
            {
                const FVector2D P(X1 - (X * 2 + 1) * Scale, Y1 - (Y * 2 + 1) * Scale);
                G0[Y * (W / 2) + X] = HouseCampusLayout::OnTrafficLane(P) ? 1 : HouseCampusLayout::Occupied(P, 0.f) ? 2 : 0;
            }
        return G0;
    }();
    Img.Init(W, H, FLinearColor(.36f, .48f, .3f));
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const uint8 K = Ground[(Y / 2) * (W / 2) + X / 2];
            if (K == 1) Img.At(X, Y) = FLinearColor(.3f, .3f, .32f);
            else if (K == 2) Img.At(X, Y) = FLinearColor(.82f, .8f, .74f);
        }
    const FVector2D SC = Px(HouseCampus::StadiumX, HouseCampus::StadiumY);
    Img.Ellipse(SC, FVector2D(5600.f / Scale, 3800.f / Scale), 0.f, FLinearColor(.45f, .3f, .22f));
    Img.Ellipse(SC, FVector2D(3800.f / Scale, 2300.f / Scale), 0.f, FLinearColor(.2f, .42f, .2f));
    FHallTextStyle S; S.Height = 9.f; S.Stroke = 1.6f; S.Spacing = .2f;
    for (const HouseCampusLayout::FBlock& B : HouseCampusLayout::Blocks())
    {
        const FVector2D A = Px(B.X1, B.Y1), Bb = Px(B.X0, B.Y0);
        Img.Rect(A, Bb, FLinearColor(.3f, .1f, .38f));
        const FString Id = B.Id;
        HallDrawText(Img, Id, FVector2D((A.X + Bb.X) * .5f - HallTextWidth(Id, S) * .5f, (A.Y + Bb.Y) * .5f + 4.f), S, FLinearColor(.95f, .85f, .5f));
    }
    HallDrawText(Img, TEXT("STADIUM"), SC + FVector2D(-HallTextWidth(TEXT("STADIUM"), S) * .5f, 4.f), S, FLinearColor(.95f, .95f, .9f));
    const FVector2D T = Px(-9000.f, -10800.f);
    Img.Ellipse(T, FVector2D(4.f, 4.f), 0.f, FLinearColor(.75f, .6f, .2f));
    // The star, and its legend.
    const FVector2D Me = Px(Here.X, Here.Y);
    TArray<FVector2D> Star;
    for (int32 K = 0; K < 10; ++K) { const float A = PI * K / 5.f - HALF_PI, R = (K & 1) ? 4.f : 10.f; Star.Add(Me + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R); }
    Img.FillPolygon(Star, FLinearColor(.85f, .05f, .03f));
    Img.Rect(FVector2D(0, H - 40), FVector2D(W, H), FLinearColor(.9f, .88f, .8f));
    FHallTextStyle L; L.Height = 18.f; L.Stroke = 3.f; L.Spacing = .2f;
    HallDrawText(Img, TEXT("CALDER STATE UNIVERSITY"), FVector2D(12, H - 12), L, FLinearColor(.3f, .08f, .4f));
    Img.FillPolygon({ FVector2D(W - 150, H - 30), FVector2D(W - 144, H - 16), FVector2D(W - 158, H - 24), FVector2D(W - 142, H - 24), FVector2D(W - 156, H - 16) }, FLinearColor(.85f, .05f, .03f));
    HallDrawText(Img, TEXT("YOU ARE HERE"), FVector2D(W - 136, H - 14), FHallTextStyle{ 12.f, 2.f, .2f }, FLinearColor(.1f, .1f, .12f));
    // North, up.
    Img.FillPolygon({ FVector2D(W - 24, 14), FVector2D(W - 30, 34), FVector2D(W - 18, 34) }, FLinearColor(.1f, .1f, .12f));
    HallDrawText(Img, TEXT("N"), FVector2D(W - 28, 50), FHallTextStyle{ 12.f, 2.f, .2f }, FLinearColor(.1f, .1f, .12f));
}

void MapSign(HouseCampusPolish::FParts& P, const FMapSign& S, int32 Index)
{
    const FVector N = FRotator(0.f, S.Yaw, 0.f).Vector(), Rt = ReadRight(N);
    const FVector Base(S.At, LowestGround({ S.At - FVector2D(Rt) * 78.f, S.At + FVector2D(Rt) * 78.f }));   // its two posts
    for (const float U : { -78.f, 78.f }) P.Solid.Box(TEXT("CP_TrimPurple"), FTransform(FRotationMatrix::MakeFromXY(N, Rt).ToQuat(), Base + Rt * U + Up(110.f)), FVector(6.f, 6.f, 110.f), Plain());
    P.Solid.Box(TEXT("CP_TrimPurple"), FTransform(FRotationMatrix::MakeFromXY(N, Rt).ToQuat(), Base + Up(160.f)), FVector(6.f, 84.f, 62.f), Plain());
    HouseCampusFallKit::Card(P.Detail, *FString::Printf(TEXT("CP_Map%d"), Index), Base + N * 6.4f + Up(160.f), Rt, FVector::UpVector, 156.f, 117.f, N);
    P.Detail.Box(TEXT("CP_TrimPurple"), FTransform(FRotationMatrix::MakeFromXY(N, Rt).ToQuat(), Base + N * 4.f + Up(232.f)), FVector(16.f, 92.f, 6.f), Plain());   // its little roof
    HouseCampusFallKit::Card(P.Detail, TEXT("CP_MapBack"), Base - N * 6.4f + Up(160.f), ReadRight(-N), FVector::UpVector, 156.f, 117.f, -N);
}

/** A fingerpost: a post with an arm toward each place named, its finial. */
void Fingerpost(HouseCampusPolish::FParts& P, const FVector2D& At, const TArray<TPair<FName, FVector2D>>& Arms)
{
    const FVector Base(At, GroundAt(At));
    P.Solid.Cylinder(TEXT("CP_TrimPurple"), Base, Base + Up(300.f), 6.f, 10, Plain());
    HouseCampusFallKit::Ball(P.Detail, TEXT("CP_Brass"), Base + Up(306.f), 8.f, 8);
    for (int32 K = 0; K < Arms.Num(); ++K)
    {
        const FVector Dir = FVector(Arms[K].Value - At, 0.f).GetSafeNormal(), N = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
        const FVector C = Base + Up(270.f - K * 30.f) + Dir * 62.f;
        P.Detail.Box(TEXT("CP_PaintWhite"), FTransform(FRotationMatrix::MakeFromXZ(Dir, FVector::UpVector).ToQuat(), C), FVector(56.f, 1.5f, 11.f), Plain());
        // The point, and the name on both faces (each reads from the post toward its end).
        HouseCampusFallKit::Tri(P.Detail, TEXT("CP_PaintWhite"), C + Dir * 56.f + Up(11.f), C + Dir * 56.f - Up(11.f), C + Dir * 70.f, N);
        HouseCampusFallKit::Tri(P.Detail, TEXT("CP_PaintWhite"), C + Dir * 56.f + Up(11.f), C + Dir * 70.f, C + Dir * 56.f - Up(11.f), -N);
        for (const float S : { 1.f, -1.f })
        {
            const FVector Face = N * S;
            HouseCampusFallKit::Card(P.Detail, Arms[K].Key, C + Face * 1.6f, ReadRight(Face), FVector::UpVector, 108.f, 18.f, Face);
        }
    }
}
/** A 1980s campus trash can: an aggregate drum, its dome lid with the opening, the liner showing. */
void TrashCan(HouseCampusPolish::FParts& P, const FVector& Base)
{
    P.Solid.Cylinder(TEXT("CP_Aggregate"), Base, Base + Up(82.f), 30.f, 16, FHallUV::World(80.f));
    P.Detail.Dome(TEXT("CP_TrimPurple"), Base + Up(82.f), FVector::UpVector, 32.f, 16.f, 14);
    P.Detail.Box(TEXT("CP_JunctionBox"), FTransform(Base + Up(90.f) + FVector(0, 0, 0)), FVector(18.f, 24.f, 7.f), Plain());
}
void BikeRack(HouseCampusPolish::FParts& P, const FVector& From, const FVector& Along, int32 Slots, FRandomStream& R)
{
    const FVector N = FVector::CrossProduct(FVector::UpVector, Along).GetSafeNormal();
    for (int32 K = 0; K < Slots; ++K)
    {
        const FVector B = From + Along * (K * 60.f);
        // An inverted U of galvanised tube.
        HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Galv"), B - N * 30.f, B - N * 30.f + Up(80.f), 4.f, 4.f, Along);
        HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Galv"), B + N * 30.f, B + N * 30.f + Up(80.f), 4.f, 4.f, Along);
        HouseCampusFallKit::Bar(P.Solid, TEXT("CP_Galv"), B - N * 30.f + Up(80.f), B + N * 30.f + Up(80.f), 4.f, 4.f, FVector::UpVector);
        if (R.FRand() < .55f)
        {
            static const FName Frames[] = { TEXT("CP_BikeRed"), TEXT("CP_BikeBlue"), TEXT("CP_BikeSilver"), TEXT("CP_TrimPurple") };
            const float Yaw = N.Rotation().Yaw + (R.FRand() < .5f ? 0.f : 180.f);
            HouseCampusFallKit::TenSpeed(P.Detail, Frames[R.RandRange(0, 3)], TEXT("CP_Tyre"), TEXT("CP_Chrome"), B + Along * 18.f + Up(1.f), Yaw);
        }
    }
}
void NoticeBoard(HouseCampusPolish::FParts& P, const FVector2D& At, float Yaw)
{
    const FVector N = FRotator(0.f, Yaw, 0.f).Vector(), Rt = ReadRight(N);
    const FVector Base(At, LowestGround({ At - FVector2D(Rt) * 100.f, At + FVector2D(Rt) * 100.f }));   // its two posts
    const FQuat Q = FRotationMatrix::MakeFromXY(N, Rt).ToQuat();
    for (const float U : { -100.f, 100.f }) P.Solid.Box(TEXT("CP_Wood"), FTransform(Q, Base + Rt * U + Up(115.f)), FVector(7.f, 7.f, 115.f), Plain());
    P.Solid.Box(TEXT("CP_Wood"), FTransform(Q, Base + Up(165.f)), FVector(5.f, 108.f, 62.f), Plain());
    for (const float S : { 1.f, -1.f }) HouseCampusFallKit::Card(P.Detail, TEXT("CP_FlyerBoard"), Base + N * (S * 5.6f) + Up(165.f), ReadRight(N * S), FVector::UpVector, 200.f, 112.f, N * S);
    // A little gable roof over it.
    for (const float S : { 1.f, -1.f })
        P.Detail.Quad(TEXT("CP_Shingle"), Base + Rt * -118.f + N * (S * 30.f) + Up(232.f), Base + Rt * 118.f + N * (S * 30.f) + Up(232.f), Base + Rt * 118.f + Up(254.f), Base + Rt * -118.f + Up(254.f), (N * S + Up(1.2f)).GetSafeNormal(), FHallUV::World(100.f));
}
void Hydrant(HouseCampusPolish::FParts& P, const FVector& Base)
{
    // A squat 1980s hydrant: barrel, bonnet, two hose nozzles and the pumper nozzle, chrome-yellow with a red bonnet.
    HouseCampusFallKit::Lathe(P.Solid, TEXT("CP_HydrantYellow"), Base, { FVector2D(14.f, 0.f), FVector2D(14.f, 6.f), FVector2D(11.f, 8.f), FVector2D(11.f, 58.f), FVector2D(13.f, 60.f), FVector2D(13.f, 64.f) }, 14);
    HouseCampusFallKit::Lathe(P.Detail, TEXT("CP_HydrantRed"), Base + Up(64.f), { FVector2D(12.f, 0.f), FVector2D(10.f, 8.f), FVector2D(4.f, 14.f), FVector2D(2.f, 18.f), FVector2D(.1f, 18.f) }, 12);
    for (int32 K = 0; K < 3; ++K)
    {
        const FVector D = FRotator(0.f, K * 90.f, 0.f).Vector();
        P.Detail.Cylinder(TEXT("CP_HydrantYellow"), Base + Up(42.f) + D * 10.f, Base + Up(42.f) + D * (K == 1 ? 20.f : 17.f), K == 1 ? 7.f : 4.5f, 10, Plain());
        P.Detail.Cylinder(TEXT("CP_HydrantRed"), Base + Up(42.f) + D * (K == 1 ? 20.f : 17.f), Base + Up(42.f) + D * (K == 1 ? 23.f : 19.f), K == 1 ? 7.6f : 5.f, 6, Plain());
    }
}
void PaintFlyers(FHallImage& Img, int32 Seed)
{
    constexpr int32 W = 256, H = 144;
    Img.Init(W, H, FLinearColor(.42f, .3f, .18f));
    FRandomStream R(Seed);
    static const FLinearColor Paper[] = { FLinearColor(.9f, .88f, .8f), FLinearColor(.95f, .85f, .4f), FLinearColor(.65f, .8f, .9f), FLinearColor(.95f, .6f, .65f), FLinearColor(.7f, .9f, .6f) };
    static const TCHAR* Lines[] = { TEXT("ROOMMATE WANTED"), TEXT("TUTOR - CALC I"), TEXT("BAND TONIGHT"), TEXT("LOST CAT"), TEXT("RIDE TO DUNMORE"), TEXT("SKI CLUB"), TEXT("TYPING 75C/PG"), TEXT("BEAT DUNMORE") };
    for (int32 K = 0; K < 26; ++K)
    {
        const FVector2D C(R.FRandRange(10.f, W - 30.f), R.FRandRange(10.f, H - 30.f)), S(R.FRandRange(26.f, 52.f), R.FRandRange(30.f, 60.f));
        Img.Rect(C, C + S, Paper[R.RandRange(0, 4)]);
        FHallTextStyle T; T.Height = 4.5f; T.Stroke = .9f; T.Spacing = .15f;
        T.WidthScale = FMath::Min(1.f, (S.X - 4.f) / HallTextWidth(Lines[K % 8], T));
        HallDrawText(Img, Lines[K % 8], C + FVector2D(2.f, 8.f), T, FLinearColor(.1f, .1f, .12f));
        for (float Y = 14.f; Y < S.Y - 4.f; Y += 5.f) Img.Rect(C + FVector2D(3.f, Y), C + FVector2D(S.X - 3.f, Y + .8f), FLinearColor(.35f, .35f, .38f));
        Img.Ellipse(C + FVector2D(S.X * .5f, 2.f), FVector2D(1.5f, 1.5f), 0.f, FLinearColor(.8f, .1f, .1f));
    }
}
}

void HouseCampusPolish::StreetFurnitureLook(UHouseBlackHallSubsystem* Look)
{
    namespace SI = HouseCampusPolishStreetImpl;
    using HouseCampusInteriorsImpl::PaintText;
    FHallImage Img;
    for (int32 I = 0; I < SI::MapSigns().Num(); ++I)
    {
        SI::PaintMap(Img, SI::MapSigns()[I].At);
        const FString Key = FString::Printf(TEXT("CP_Map%d"), I);
        UTexture2D* T = HallMakeTexture(Img, true, false, *FString::Printf(TEXT("T_%s"), *Key), false);
        if (T) Look->Textures.Add(FName(*FString::Printf(TEXT("T_%s"), *Key)), T);
        HallMakeTextured(Look, FName(*Key), T, nullptr, .4f, FLinearColor::White, 0.f, 0.f, FLinearColor(.05f, .05f, .045f));
    }
    PaintText(Look, TEXT("CP_MapBack"), 256, 192, FLinearColor(.3f, .08f, .4f), FLinearColor(.95f, .78f, .25f), { { TEXT("CALDER STATE"), .16f }, { TEXT("UNIVERSITY"), .12f }, { TEXT("FOUNDED 1891"), .08f } }, true);
    SI::PaintFlyers(Img, 91);
    {
        UTexture2D* T = HallMakeTexture(Img, true, false, TEXT("T_CP_FlyerBoard"), false);
        if (T) Look->Textures.Add(TEXT("T_CP_FlyerBoard"), T);
        HallMakeTextured(Look, TEXT("CP_FlyerBoard"), T, nullptr, .85f, FLinearColor::White, 0.f, 0.f, FLinearColor(.015f, .013f, .01f));
    }
    const TCHAR* Names[][2] = { { TEXT("CP_ArmLibrary"), TEXT("MERCER LIBRARY") }, { TEXT("CP_ArmMain"), TEXT("OLD MAIN") }, { TEXT("CP_ArmHarlow"), TEXT("HARLOW HALL") },
        { TEXT("CP_ArmUnion"), TEXT("GOOSE UNION") }, { TEXT("CP_ArmStadium"), TEXT("STADIUM") }, { TEXT("CP_ArmScience"), TEXT("HOLLIS SCIENCE") }, { TEXT("CP_ArmTower"), TEXT("CLOCK TOWER") },
        { TEXT("CP_ArmCommons"), TEXT("THE COMMONS") }, { TEXT("CP_ArmAshby"), TEXT("ASHBY HALL") }, { TEXT("CP_ArmKerr"), TEXT("KERR HALL") } };
    for (const auto& N : Names) PaintText(Look, N[0], 384, 64, FLinearColor(.86f, .85f, .8f), FLinearColor(.3f, .08f, .4f), { { N[1], .5f } }, false);
    struct FSpec { const TCHAR* Key; FLinearColor Tint; float Rough, Metal; };
    const FSpec Plains[] = {
        { TEXT("CP_Aggregate"), FLinearColor(.5f, .47f, .42f), .9f, 0.f }, { TEXT("CP_HydrantYellow"), FLinearColor(.75f, .55f, .03f), .45f, .2f },
        { TEXT("CP_HydrantRed"), FLinearColor(.5f, .05f, .03f), .45f, .2f }, { TEXT("CP_BikeRed"), FLinearColor(.5f, .04f, .03f), .35f, .3f },
        { TEXT("CP_BikeBlue"), FLinearColor(.05f, .12f, .4f), .35f, .3f }, { TEXT("CP_BikeSilver"), FLinearColor(.55f, .56f, .57f), .3f, .8f },
    };
    for (const FSpec& S : Plains) HallMakePlain(Look, S.Key, S.Tint, S.Rough, S.Metal);
}

void HouseCampusPolish::StreetFurniture(FParts& P)
{
    namespace SI = HouseCampusPolishStreetImpl;
    for (int32 I = 0; I < SI::MapSigns().Num(); ++I) if (P.Clear(SI::MapSigns()[I].At, 30.f)) SI::MapSign(P, SI::MapSigns()[I], I);
    // Fingerposts at the promenade's two big junctions.
    auto Centre = [](const TCHAR* Id) { return HouseCampusLayout::Blocks()[HouseCampusLayout::Find(Id)].Centre(); };
    if (P.Clear(FVector2D(-11250.f, -6900.f), 30.f))
        SI::Fingerpost(P, FVector2D(-11250.f, -6900.f), { { TEXT("CP_ArmLibrary"), Centre(TEXT("LIB")) }, { TEXT("CP_ArmTower"), FVector2D(-9000.f, -10800.f) }, { TEXT("CP_ArmMain"), Centre(TEXT("MAIN")) }, { TEXT("CP_ArmHarlow"), Centre(TEXT("HAR")) } });
    if (P.Clear(FVector2D(2050.f, -6900.f), 30.f))
        SI::Fingerpost(P, FVector2D(2050.f, -6900.f), { { TEXT("CP_ArmStadium"), FVector2D(HouseCampus::StadiumX, HouseCampus::StadiumY) }, { TEXT("CP_ArmUnion"), Centre(TEXT("UNI")) }, { TEXT("CP_ArmCommons"), Centre(TEXT("COM")) }, { TEXT("CP_ArmAshby"), Centre(TEXT("ASH")) } });
    // A trash can by every bench, at the end of its seat.
    {
        TArray<FTransform> Benches;
        HouseCampusLayout::BenchPoints(Benches);
        for (const FTransform& B : Benches)
        {
            const FVector At = B.TransformPosition(FVector(135.f, 10.f, 0.f));
            if (P.Clear(FVector2D(At), 35.f)) SI::TrashCan(P, FVector(At.X, At.Y, SI::GroundAt(FVector2D(At)) + .5f));
        }
    }
    // Bike racks off the busy doors.
    {
        FRandomStream R(10);
        const TPair<FVector2D, FVector> Racks[] = { { FVector2D(-7700.f, -6950.f), FVector(1, 0, 0) }, { FVector2D(-14300.f, -9800.f), FVector(0, 1, 0) }, { FVector2D(-11900.f, -14000.f), FVector(1, 0, 0) }, { FVector2D(15900.f, -6950.f), FVector(1, 0, 0) } };
        for (const TPair<FVector2D, FVector>& K : Racks)
        {
            bool bOK = true;
            for (int32 S = 0; S < 5; ++S) bOK &= P.Clear(K.Key + FVector2D(K.Value) * (S * 60.f), 45.f);
            const FVector2D Step = FVector2D(K.Value) * 60.f;   // the five slots
            if (bOK) SI::BikeRack(P, FVector(K.Key, SI::LowestGround({ K.Key, K.Key + Step, K.Key + Step * 2.f, K.Key + Step * 3.f, K.Key + Step * 4.f })), K.Value, 5, R);
        }
    }
    // Notice boards: by the Union's forecourt and outside Kerr Hall.
    for (const TPair<FVector2D, float>& N : { TPair<FVector2D, float>(FVector2D(-8900.f, -13350.f), 0.f), TPair<FVector2D, float>(FVector2D(10600.f, -13900.f), 180.f), TPair<FVector2D, float>(FVector2D(-3400.f, -6950.f), 90.f) })
        if (P.Clear(N.Key, 130.f)) SI::NoticeBoard(P, N.Key, N.Value);
    // Hydrants on the verges: Calder Dr both sides, Stadium Dr, the south lane.
    {
        int32 Count = 0;
        for (float Y = -7300.f; Y > -17000.f; Y -= 2900.f)
            for (const float X : { 8340.f, 10660.f })
                if (P.Clear(FVector2D(X, Y), 25.f)) { SI::Hydrant(P, FVector(X, Y, SI::GroundAt(FVector2D(X, Y)))); ++Count; }
        // (The south lane's row stood on the flat datum; on the Calder terrain it stands on the ground, which the South Lot
        // drive's mouth lifts 18 cm at X 3600.)
        for (float X = 0.f; X < 7600.f; X += 3600.f)
            if (P.Clear(FVector2D(X, -19960.f), 25.f)) { SI::Hydrant(P, FVector(X, -19960.f, HouseCalderTerrain::Enabled() ? SI::GroundAt(FVector2D(X, -19960.f)) : SI::G)); ++Count; }
        UE_LOG(LogTemp, Verbose, TEXT("CAMPUS_POLISH hydrants=%d"), Count);
    }
}
