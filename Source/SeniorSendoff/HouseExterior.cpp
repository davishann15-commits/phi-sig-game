// Runtime-built exterior of the fraternity house and its night-time surroundings. See HouseExterior.h.
// The upstairs impossible hallway is trimmed in the assets (Saved/exterior_cut2.py) so it ends inside the left wing's front wall.
// Coordinates are world cm of the saved house (1 real m = 198 cm horizontally, 110 cm vertically).
#include "HouseExterior.h"
#include "HouseExteriorWalks.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseExteriorNature.h"
#include "HouseExteriorBrick.h"
#include "HouseExteriorCars.h"
#include "HouseLeafWind.h"
#include "HouseBasketballCourt.h"
#include "HouseBasketball.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCarryTiersReview.h"
#include "HouseDetail.h"
#include "StoryCampaign.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "RenderCore.h"
#include "Stats/Stats.h"
#include "UnrealClient.h"

namespace
{
using HouseExteriorSite::StreetY;
// ------------------------------------------------------------------ site levels
constexpr float GF = -60.f;          // front / street grade
constexpr float GR = -362.f;         // rear courtyard grade (= basement floor)
constexpr float ZTop = 640.f;        // brick stops, cornice starts
constexpr float ZEave = 692.f;       // cornice top = roof eave
constexpr float RoofPitch = .36f;    // 8:12 after the house's vertical squash
constexpr float SlopeY0 = 0.f, SlopeY1 = 1600.f;
float Grade(float X, float Y)
{
    if (Y <= SlopeY0) return GF;
    if (Y >= SlopeY1) return GR;
    return GF + (GR - GF) * ((Y - SlopeY0) / (SlopeY1 - SlopeY0));
}

// Fixed camera spots used by -HouseExteriorViews; the planting keeps trees away from them.
struct FExView { const TCHAR* Name; FVector Pos, Target; };
const FExView ExViews[] = {
    { TEXT("great_entrance"),  FVector(1184, 624, 185),     FVector(1184, 624, 185) + FRotator(3, -146, 0).Vector() * 500 },
    { TEXT("great_fireplace"), FVector(-267, 614, 185),     FVector(-267, 614, 185) + FRotator(2, -143, 0).Vector() * 500 },
    { TEXT("gallery_hall"),    FVector(-1317, -752, 503),   FVector(-817, -752, 495) },
    { TEXT("rear_hall_upper"), FVector(-2395, 693, 514),    FVector(-2395, 193, 514) },
    { TEXT("deck"),            FVector(1250, 1180, 177),    FVector(-1300, 1250, 120) },
    { TEXT("stair_window"),    FVector(-1789, 1109, 476),   FVector(-1789, 2500, 420) },
    { TEXT("balcony_out"),     FVector(-19, -1198, 517),    FVector(-19, -4000, 250) },
    { TEXT("porch_facade"),    FVector(-10, 2297, 314),     FVector(-10, 2297, 314) + FRotator(-9, -90, 0).Vector() * 500 },
    { TEXT("porch_end"),       FVector(-1832, 2188, 292),   FVector(-1832, 2188, 292) + FRotator(-5, -41, 0).Vector() * 500 },
    { TEXT("deck_edge"),       FVector(-500, 1500, 175),    FVector(-500, 2700, -320) },
    { TEXT("patio_doors"),     FVector(380, 1650, -190),    FVector(0, 930, -260) },
    { TEXT("front_street"),    FVector(-300, StreetY(-4500), 150),   FVector(-300, -1010, 300) },
    { TEXT("front_walk"),      FVector(-20, -2600, 120),    FVector(-20, -1300, 350) },
    { TEXT("front_lawn_overview"), FVector(4600, -6050, 2400), FVector(-250, -2800, 20) },
    { TEXT("lawn_toward_road"), FVector(-36.65, -2180, 130), FVector(-36.65, HouseExteriorSite::FrontRoadFarY, -60) },
    { TEXT("street_along"),    FVector(-7200, StreetY(-3650), 170),  FVector(6000, StreetY(-3650), 150) },
    { TEXT("right_end"),       FVector(6200, -2800, 140),   FVector(3340, -800, 250) },
    { TEXT("back_court"),      FVector(800, 3400, -190),    FVector(-500, 2200, -200) },
    { TEXT("back_stair"),      FVector(-2600, 3200, -190),  FVector(-1190, 1800, -100) },
    { TEXT("left_rear"),       FVector(-5500, 3500, -150),  FVector(-3400, 600, 300) },
    { TEXT("left_front"),      FVector(-5600, -4300, 130),  FVector(-2800, -2200, 380) },
    { TEXT("high"),            FVector(7000, -8000, 3200),  FVector(0, 0, 0) },
    { TEXT("yard_walk"),       FVector(-3900, -2400, 110),  FVector(-1500, -1500, 150) },
    { TEXT("nature_fall_tree"), FVector(700, -3550, 130),   FVector(2200, -2650, 480) },
    { TEXT("nature_shrubs"),    FVector(960, -2780, 80),   FVector(970, -2390, 12) },
    { TEXT("nature_grass"),     FVector(-1800, -3200, 90), FVector(-2400, -2650, -30) },
    { TEXT("detail_brick_window"), FVector(-1800, -2850, 150), FVector(-2100, -2200, 165) },
    { TEXT("detail_brick_arch"), FVector(5, 2150, -200), FVector(5, 1710, -105) },
    { TEXT("detail_grass_lull"), FVector(-1800, -3200, 35), FVector(-2200, -2880, -25) },
    { TEXT("detail_grass_gust"), FVector(-1800, -3200, 35), FVector(-2200, -2880, -25) },
    { TEXT("detail_cars"), FVector(-5960, -2740, 120), FVector(-5650, -2100, 60) },
};

// ------------------------------------------------------------------ materials
void ExPaintBrick(FHallImage& Img, TArray<float>& Height, const FLinearColor& Base, const FLinearColor& Mortar, int32 Seed)
{
    const int32 W = 256, H = 256;             // one tile = 120 x 120 cm: 16 courses of 7.5, bricks 40 long
    Img.Init(W, H, Mortar); Height.Init(0.f, W * H);
    const float CourseH = H / 16.f, BrickW = W / 3.f;
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const int32 Course = (int32)(Y / CourseH);
            const float Fy = Y - Course * CourseH;
            const float Shift = (Course & 1) ? BrickW * .5f : 0.f;
            const float Xs = FMath::Fmod(X + Shift, BrickW);
            const int32 BrickIdx = (int32)((X + Shift) / BrickW);
            if (Fy < 2.f || Xs < 2.f)
            {
                Img.At(X, Y) = Mortar * (.85f + .3f * HallNoise(X * .37f, Y * .37f, Seed + 3));
                continue;
            }
            const float V = HallNoise(BrickIdx * 7.31f + 1.13f, Course * 3.71f + .4f, Seed);
            const float G = HallFbm(X * .11f, Y * .11f, 3, Seed + 1);
            const float Edge = FMath::Min(FMath::Min(Fy - 2.f, CourseH - Fy), FMath::Min(Xs - 2.f, BrickW - Xs));
            FLinearColor C = Base * (.72f + .56f * V) * (.82f + .36f * G);
            if (V > .82f) C = C * .75f;                    // a few burnt headers
            Img.At(X, Y) = C;
            Height[Y * W + X] = FMath::Clamp(Edge / 2.f, 0.f, 1.f) * (.85f + .15f * G);
        }
}

void ExPaintShingle(FHallImage& Img, TArray<float>& Height, int32 Seed)
{
    const int32 W = 256, H = 256;             // tile = 100 x 100 cm: 3 courses, tabs 33 wide
    Img.Init(W, H, FLinearColor(.2f, .2f, .21f)); Height.Init(1.f, W * H);
    const float CourseH = H / 3.f, TabW = W / 3.f;
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const int32 Course = (int32)(Y / CourseH);
            const float Fy = Y - Course * CourseH;
            const float Shift = (Course & 1) ? TabW * .5f : 0.f;
            const float Xs = FMath::Fmod(X + Shift, TabW);
            const int32 Tab = (int32)((X + Shift) / TabW) + Course * 7;
            const float V = HallNoise(Tab * 5.17f + .3f, Course * 2.9f, Seed);
            const float G = HallFbm(X * .25f, Y * .25f, 2, Seed + 2);
            float Lum = .17f + .12f * V + .08f * G;
            if (Fy < 4.f) Lum *= .45f;                     // shadow under the course above
            if (Xs < 2.f && Fy > CourseH * .45f) Lum *= .6f;   // tab slot
            Img.At(X, Y) = FLinearColor(Lum, Lum, Lum * 1.05f);
            Height[Y * W + X] = (Fy < 4.f ? 0.f : 1.f) * ((Xs < 2.f && Fy > CourseH * .45f) ? .3f : 1.f);
        }
}

void ExPaintGrass(FHallImage& Img, int32 Seed)
{
    const int32 W = 256, H = 256;
    Img.Init(W, H, FLinearColor::Black);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float A = HallFbm(X * .05f, Y * .05f, 4, Seed), B = HallNoise(X * .9f, Y * .9f, Seed + 7);
            const float Patch = HallFbm(X * .015f, Y * .015f, 2, Seed + 9);
            FLinearColor C = FLinearColor::LerpUsingHSV(FLinearColor(.07f, .16f, .04f), FLinearColor(.15f, .17f, .05f), Patch);
            C = C * (.7f + .5f * A) * (.8f + .4f * B);
            Img.At(X, Y) = C;
        }
}

void ExPaintAsphalt(FHallImage& Img, int32 Seed)
{
    const int32 W = 256, H = 256;
    Img.Init(W, H, FLinearColor::Black);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float A = HallFbm(X * .07f, Y * .07f, 3, Seed), B = HallNoise(X * 1.3f, Y * 1.3f, Seed + 5);
            const float L = .055f + .035f * A + (B > .93f ? .06f : 0.f);
            Img.At(X, Y) = FLinearColor(L, L, L * 1.03f);
        }
}

void ExPaintConcrete(FHallImage& Img, int32 Seed)
{
    const int32 W = 256, H = 256;             // tile = 240 cm with a joint every 120
    Img.Init(W, H, FLinearColor::Black);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float A = HallFbm(X * .06f, Y * .06f, 3, Seed), B = HallNoise(X * .8f, Y * .8f, Seed + 4);
            float L = .42f + .12f * A + .05f * B;
            const int32 JX = X % 128, JY = Y % 128;
            if (JX < 2 || JY < 2) L *= .55f;
            Img.At(X, Y) = FLinearColor(L, L * .99f, L * .95f);
        }
}

void ExPaintFrieze(FHallImage& Img)
{
    const int32 W = 1024, H = 128;
    Img.Init(W, H, FLinearColor(.84f, .84f, .81f));
    // Keep the architectural fascia, without fraternity lettering.
}

void ExCreateLook(UHouseBlackHallSubsystem* Sub)
{
    if (!Sub || Sub->Materials.Contains(TEXT("EX_Brick"))) return;
    auto Plain = [&](const TCHAR* Key, const FLinearColor& C, float Rough, float Metal = 0.f, const FLinearColor& Em = FLinearColor::Black) { HallMakePlain(Sub, FName(Key), C, Rough, Metal, Em); };
    auto Tex = [&](const TCHAR* Key, const FHallImage& Img, const TArray<float>* Height, float Rough, float NormalStrength, const FLinearColor& Tint = FLinearColor::White)
    {
        UTexture2D* T = HallMakeTexture(Img, true, false, *FString::Printf(TEXT("T_%s"), Key), true);
        if (T) Sub->Textures.Add(FName(*FString::Printf(TEXT("T_%s"), Key)), T);
        UTexture2D* N = nullptr;
        if (Height)
        {
            FHallImage NImg; HallHeightToNormal(*Height, Img.W, Img.H, NormalStrength, true, NImg);
            N = HallMakeTexture(NImg, false, true, *FString::Printf(TEXT("T_%sN"), Key), true);
            if (N) Sub->Textures.Add(FName(*FString::Printf(TEXT("T_%sN"), Key)), N);
        }
        HallMakeTextured(Sub, FName(Key), T, N, Rough, Tint, N ? 1.f : 0.f);
        return T;
    };
    FHallImage Img; TArray<float> Height;
    ExPaintBrick(Img, Height, FLinearColor(.40f, .19f, .13f), FLinearColor(.58f, .55f, .50f), 11);
    UTexture2D* BrickT = Tex(TEXT("EX_Brick"), Img, &Height, .86f, 2.2f);
    // Chimneys: same bond, redder.
    if (BrickT)
    {
        UTexture2D* BrickN = Sub->Textures.FindRef(TEXT("T_EX_BrickN"));
        HallMakeTextured(Sub, TEXT("EX_BrickRed"), BrickT, BrickN, .88f, FLinearColor(1.25f, .78f, .72f), 1.f);
    }
    ExPaintShingle(Img, Height, 23); Tex(TEXT("EX_Shingle"), Img, &Height, .92f, 1.6f);
    ExPaintGrass(Img, 31); Tex(TEXT("EX_Grass"), Img, nullptr, .95f, 0.f);
    ExPaintAsphalt(Img, 41); Tex(TEXT("EX_Asphalt"), Img, nullptr, .9f, 0.f);
    ExPaintConcrete(Img, 51); Tex(TEXT("EX_Concrete"), Img, nullptr, .8f, 0.f);
    {
        FHallImage F; ExPaintFrieze(F);
        UTexture2D* T = HallMakeTexture(F, true, false, TEXT("T_EX_Frieze"), false);
        if (T) { Sub->Textures.Add(TEXT("T_EX_Frieze"), T); HallMakeTextured(Sub, TEXT("EX_Frieze"), T, nullptr, .5f, FLinearColor::White, 0.f); }
    }
    Plain(TEXT("EX_Trim"), FLinearColor(.84f, .84f, .80f), .5f);
    Plain(TEXT("EX_Stone"), FLinearColor(.60f, .58f, .52f), .7f);
    Plain(TEXT("EX_Glass"), FLinearColor(.015f, .02f, .035f), .12f, .35f);
    Plain(TEXT("EX_Wood"), FLinearColor(.10f, .065f, .04f), .75f);
    Plain(TEXT("EX_Dark"), FLinearColor(.05f, .045f, .04f), .9f);
    Plain(TEXT("EX_Leaf"), FLinearColor(.045f, .11f, .035f), .95f);
    Plain(TEXT("EX_LeafDark"), FLinearColor(.03f, .07f, .025f), .95f);
    Plain(TEXT("EX_Bark"), FLinearColor(.11f, .085f, .06f), .9f);
    Plain(TEXT("EX_Hedge"), FLinearColor(.035f, .09f, .03f), .95f);
    Plain(TEXT("EX_Metal"), FLinearColor(.16f, .16f, .17f), .45f, .8f);
    Plain(TEXT("EX_Black"), FLinearColor(.012f, .012f, .012f), .8f);
    Plain(TEXT("EX_Beige"), FLinearColor(.52f, .46f, .36f), .85f);
    Plain(TEXT("EX_Wall"), FLinearColor(.44f, .44f, .42f), .85f);
    Plain(TEXT("EX_Line"), FLinearColor(.85f, .85f, .82f), .7f);
    Plain(TEXT("EX_Yellow"), FLinearColor(.75f, .6f, .08f), .7f);
    Plain(TEXT("EX_Orange"), FLinearColor(.8f, .25f, .04f), .5f, .3f);
    Plain(TEXT("EX_Field"), FLinearColor(.05f, .13f, .04f), .95f);
    Plain(TEXT("EX_Green"), FLinearColor(.06f, .22f, .09f), .55f, .2f);
    Plain(TEXT("EX_Blue"), FLinearColor(.08f, .15f, .4f), .4f, .3f);
    Plain(TEXT("EX_Red"), FLinearColor(.45f, .04f, .03f), .35f, .4f);
    Plain(TEXT("EX_White"), FLinearColor(.78f, .78f, .76f), .35f, .3f);
    Plain(TEXT("EX_Grey"), FLinearColor(.22f, .22f, .23f), .35f, .5f);
    Plain(TEXT("EX_CarGlass"), FLinearColor(.02f, .025f, .035f), .1f, .4f);
    Plain(TEXT("EX_Rubber"), FLinearColor(.02f, .02f, .02f), .9f);
    Plain(TEXT("EX_Lamp"), FLinearColor(1.f, .85f, .6f), .4f, 0.f, FLinearColor(14.f, 10.f, 5.f));
    Plain(TEXT("EX_Flood"), FLinearColor(.9f, .95f, 1.f), .4f, 0.f, FLinearColor(30.f, 32.f, 40.f));
    Plain(TEXT("EX_Window"), FLinearColor(.9f, .8f, .6f), .5f, 0.f, FLinearColor(2.2f, 1.6f, .9f));
    Plain(TEXT("EX_Neon"), FLinearColor(.9f, .2f, .3f), .5f, 0.f, FLinearColor(6.f, .8f, 1.5f));
}

// ------------------------------------------------------------------ geometry
struct FExWin { float U0, U1, Z0, Z1; int32 Kind; };   // 0 scanned window (opening + trim), 1 blind window (dark glass), 2 door, 3 plain hole
struct FExWall
{
    int32 Axis;        // 0: plane x = Face, runs along y;  1: plane y = Face, runs along x
    float Sign;        // outward normal sign
    float Face;        // exterior face coordinate
    float Thick;       // inward thickness
    float U0, U1, Z0, Z1;
    float MuntinD;     // depth of the sash bars for scanned windows (0 = Thick + 22)
    TArray<FExWin> Wins;
};

struct FExBuilder
{
    UWorld* World = nullptr;
    UHouseBlackHallSubsystem* Look = nullptr;
    AHouseExteriorDetail* Owner = nullptr;
    FHallMesh House, Grounds, Near, Far, CourtDecor, Walks, WalkDecor, BrickDecor;
    FRandomStream Rng = FRandomStream(20261009);
    struct FLightSpec { FVector Pos; FLinearColor Color; float Lumens, Radius, Draw; };
    TArray<FLightSpec> LightSpecs;
    struct FNoGo { float X0, X1, Y0, Y1; };
    TArray<FNoGo> NoGo;
    TArray<FBox> BlockBoxes;

    const FName Brick = TEXT("EX_Brick"), BrickRed = TEXT("EX_BrickRed"), Trim = TEXT("EX_Trim"), Stone = TEXT("EX_Stone"), Shingle = TEXT("EX_Shingle"),
        Glass = TEXT("EX_Glass"), Grass = TEXT("EX_Grass"), Asphalt = TEXT("EX_Asphalt"), Concrete = TEXT("EX_Concrete"), Wood = TEXT("EX_Wood"), Dark = TEXT("EX_Dark"),
        Leaf = TEXT("EX_Leaf"), LeafDark = TEXT("EX_LeafDark"), Bark = TEXT("EX_Bark"), Hedge = TEXT("EX_Hedge"), Metal = TEXT("EX_Metal"), Black = TEXT("EX_Black"),
        Beige = TEXT("EX_Beige"), Wall = TEXT("EX_Wall"), Line = TEXT("EX_Line"), Yellow = TEXT("EX_Yellow"), Orange = TEXT("EX_Orange"), Field = TEXT("EX_Field"),
        Green = TEXT("EX_Green"), Blue = TEXT("EX_Blue"), Red = TEXT("EX_Red"), White = TEXT("EX_White"), Grey = TEXT("EX_Grey"), CarGlass = TEXT("EX_CarGlass"),
        Rubber = TEXT("EX_Rubber"), Lamp = TEXT("EX_Lamp"), Flood = TEXT("EX_Flood"), Window = TEXT("EX_Window"), Neon = TEXT("EX_Neon"), Frieze = TEXT("EX_Frieze");
    const FHallUV BrickUV = FHallUV::World(192.f), ShingleUV = FHallUV::World(100.f), GrassUV = FHallUV::World(180.f), AsphaltUV = FHallUV::World(500.f),
        ConcreteUV = FHallUV::World(240.f), PlainUV = FHallUV::World(100.f);

    // ---------------------------------------------------------------- primitives
    static void Box(FHallMesh& M, FName K, float X0, float X1, float Y0, float Y1, float Z0, float Z1, const FHallUV& UV, int32 Skip = 0)
    {
        M.BoxMinMax(K, FVector(FMath::Min(X0, X1), FMath::Min(Y0, Y1), FMath::Min(Z0, Z1)), FVector(FMath::Max(X0, X1), FMath::Max(Y0, Y1), FMath::Max(Z0, Z1)), UV, Skip);
    }
    /** One triangle, front facing N (any winding). */
    static void Tri(FHallMesh& M, FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& N, const FHallUV& UV)
    {
        FHallSection& S = M.Sec(K);
        const int32 Base = S.V.Num();
        FVector UAx, VAx; FHallMesh::PlanarAxes(N, UAx, VAx);
        for (const FVector& P : { A, B, C }) { S.V.Add(P); S.N.Add(N); S.UV.Add(M.ComputeUV(P, N, UV)); S.T.Add(FProcMeshTangent(UAx, false)); }
        if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), N) < 0) S.I.Append({ Base, Base + 1, Base + 2 });
        else S.I.Append({ Base, Base + 2, Base + 1 });
    }
    /** Flat quad with an explicit normal (triangles when D == C). */
    static void Face(FHallMesh& M, FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FHallUV& UV)
    {
        FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        if (N.IsNearlyZero()) N = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
        M.Quad(K, A, B, C, D, N, UV);
    }
    /** Box rotated so its local Y runs from A to B (rails, raking cornices). */
    static void Bar(FHallMesh& M, FName K, const FVector& A, const FVector& B, float HalfX, float HalfZ, const FHallUV& UV)
    {
        const FVector Dir = (B - A).GetSafeNormal();
        const FQuat Q = FRotationMatrix::MakeFromYZ(Dir, FVector::UpVector).ToQuat();
        M.Box(K, FTransform(Q, (A + B) * .5f), FVector(HalfX, (B - A).Size() * .5f, HalfZ), UV);
    }
    static float R(FRandomStream& S, float A, float B) { return A + (B - A) * S.FRand(); }

    // ---------------------------------------------------------------- walls
    FVector WP(const FExWall& W, float U, float D, float Z) const
    {
        const float C = W.Face - W.Sign * D;
        return W.Axis == 0 ? FVector(C, U, Z) : FVector(U, C, Z);
    }
    FVector WN(const FExWall& W) const { return W.Axis == 0 ? FVector(W.Sign, 0, 0) : FVector(0, W.Sign, 0); }
    void WallBox(FName K, const FExWall& W, float U0, float U1, float D0, float D1, float Z0, float Z1, const FHallUV& UV)
    {
        const FVector A = WP(W, U0, D0, Z0), B = WP(W, U1, D1, Z1);
        House.BoxMinMax(K, A.ComponentMin(B), A.ComponentMax(B), UV);
    }
    void WindowDetail(const FExWall& W, const FExWin& X)
    {
        if (X.Kind == 3) return;
        const float U0 = X.U0, U1 = X.U1, Z0 = X.Z0, Z1 = X.Z1;
        if (X.Kind == 2)
        {
            WallBox(Trim, W, U0, U0 + 6, 2, 14, Z0, Z1, PlainUV);
            WallBox(Trim, W, U1 - 6, U1, 2, 14, Z0, Z1, PlainUV);
            WallBox(Trim, W, U0, U1, 2, 14, Z1 - 6, Z1, PlainUV);
            if (Z1 - Z0 > 245.f) WallBox(Trim, W, U0 + 6, U1 - 6, 2, 14, Z0 + 228, Z0 + 236, PlainUV);
            WallBox(Stone, W, U0 - 4, U1 + 4, -2, W.Thick, Z0 - 4, Z0 + 1, PlainUV);
            return;
        }
        const float F = 5.f;
        WallBox(Trim, W, U0, U0 + F, 2, 12, Z0, Z1, PlainUV);
        WallBox(Trim, W, U1 - F, U1, 2, 12, Z0, Z1, PlainUV);
        WallBox(Trim, W, U0, U1, 2, 12, Z1 - F, Z1, PlainUV);
        WallBox(Trim, W, U0, U1, 2, 12, Z0, Z0 + F, PlainUV);
        WallBox(Stone, W, U0 - 8, U1 + 8, -6, 14, Z0 - 7, Z0 + 1, PlainUV);
        const float Dm = X.Kind == 1 ? 9.f : (W.MuntinD > 0.f ? W.MuntinD : W.Thick + 22.f);
        const float Wd = U1 - U0 - 2 * F, Ht = Z1 - Z0 - 2 * F, BarW = 1.6f;
        if (Wd > 90.f)
            for (int32 K = 1; K <= 2; ++K)
            {
                const float U = U0 + F + Wd * K / 3.f;
                WallBox(Trim, W, U - BarW, U + BarW, Dm - 1, Dm + 1, Z0 + F, Z1 - F, PlainUV);
            }
        if (Ht > 90.f)
            for (int32 K = 1; K <= 5; ++K)
            {
                const float Z = Z0 + F + Ht * K / 6.f, B = K == 3 ? 3.f : BarW;
                WallBox(Trim, W, U0 + F, U1 - F, Dm - 1, Dm + 1, Z - B, Z + B, PlainUV);
            }
        else WallBox(Trim, W, U0 + F, U1 - F, Dm - 1, Dm + 1, (Z0 + Z1) * .5f - BarW, (Z0 + Z1) * .5f + BarW, PlainUV);
        if (X.Kind == 1)
            House.Quad(Glass, WP(W, U0 + F, 10, Z0 + F), WP(W, U1 - F, 10, Z0 + F), WP(W, U1 - F, 10, Z1 - F), WP(W, U0 + F, 10, Z1 - F), WN(W), PlainUV);
    }
    void BuildWall(const FExWall& W)
    {
        TArray<float> Us = { W.U0, W.U1 };
        for (const FExWin& X : W.Wins) { Us.Add(X.U0); Us.Add(X.U1); }
        Us.Sort();
        for (int32 I = 0; I + 1 < Us.Num(); ++I)
        {
            const float A = Us[I], B = Us[I + 1];
            if (B - A < .5f) continue;
            const float Mid = (A + B) * .5f;
            TArray<TPair<float, float>> Cuts;
            for (const FExWin& X : W.Wins) if (Mid > X.U0 && Mid < X.U1) Cuts.Add(TPair<float, float>(X.Z0, X.Z1));
            Cuts.Sort([](const TPair<float, float>& P, const TPair<float, float>& Q) { return P.Key < Q.Key; });
            float Z = W.Z0;
            for (const TPair<float, float>& C : Cuts)
            {
                if (C.Key > Z + .5f) WallBox(Brick, W, A, B, 0, W.Thick, Z, C.Key, BrickUV);
                Z = FMath::Max(Z, C.Value);
            }
            if (W.Z1 > Z + .5f) WallBox(Brick, W, A, B, 0, W.Thick, Z, W.Z1, BrickUV);
        }
        for (const FExWin& X : W.Wins)
        {
            WindowDetail(W, X);
            const float Top=FMath::Min(W.Z1-.3f,X.Z1+22.f);
            if(Top-X.Z1>8.f)
                HouseBrick::Header(BrickDecor,WP(W,FMath::Max(W.U0,X.U0-7.f),-.12f,X.Z1+.15f),
                    WP(W,FMath::Min(W.U1,X.U1+7.f),-.12f,X.Z1+.15f),WN(W),Top-X.Z1);
        }
        // cast-stone water table just below the main floor
        if (W.Z0 < -22.f && W.Z1 > 0.f) WallBox(Stone, W, W.U0 - 6, W.U1 + 6, -6, 2, -22, -8, PlainUV);
    }
    /** Cornice band at the top of a wall; x-running bands are a hair taller than y-running ones so their overlaps never share a plane. */
    void Cornice(const FExWall& W, float Ext0, float Ext1)
    {
        const float Z0 = W.Axis == 1 ? 640.f : 641.f, Z1 = W.Axis == 1 ? 692.f : 691.f;
        WallBox(Trim, W, W.U0 - Ext0, W.U1 + Ext1, -60, 0, Z0, Z1, PlainUV);
        WallBox(Trim, W, W.U0 - Ext0, W.U1 + Ext1, -62, -60, Z0 + 20, Z0 + 30, PlainUV);   // thin bed mould line
    }

    // ---------------------------------------------------------------- roofs
    static void SlopeQuadM(FHallMesh& M, FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FHallUV& UV)
    {
        FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        if (N.IsNearlyZero()) N = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
        if (N.Z < 0) N = -N;
        if (C.Equals(D)) Tri(M, K, A, B, C, N, UV);
        else M.Quad(K, A, B, C, D, N, UV);
    }
    void SlopeQuad(FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FHallUV& UV) { SlopeQuadM(House, K, A, B, C, D, UV); }
    void HipRoof(float X0, float X1, float Y0, float Y1, float Over, float ZE, float P) { HipRoofM(House, X0, X1, Y0, Y1, Over, ZE, P); }
    void HipRoofM(FHallMesh& M, float X0, float X1, float Y0, float Y1, float Over, float ZE, float P)
    {
        const float x0 = X0 - Over, x1 = X1 + Over, y0 = Y0 - Over, y1 = Y1 + Over;
        const float W = x1 - x0, D = y1 - y0;
        if (W >= D)
        {
            const float Half = D * .5f, ZR = ZE + Half * P, CY = (y0 + y1) * .5f;
            const FVector R0(x0 + Half, CY, ZR), R1(x1 - Half, CY, ZR);
            SlopeQuadM(M, Shingle, FVector(x0, y0, ZE), FVector(x1, y0, ZE), R1, R0, ShingleUV);
            SlopeQuadM(M, Shingle, FVector(x1, y1, ZE), FVector(x0, y1, ZE), R0, R1, ShingleUV);
            SlopeQuadM(M, Shingle, FVector(x0, y1, ZE), FVector(x0, y0, ZE), R0, R0, ShingleUV);
            SlopeQuadM(M, Shingle, FVector(x1, y0, ZE), FVector(x1, y1, ZE), R1, R1, ShingleUV);
            Bar(M, Metal, R0 + FVector(0, 0, 2), R1 + FVector(0, 0, 2), 6, 3, PlainUV);
        }
        else
        {
            const float Half = W * .5f, ZR = ZE + Half * P, CX = (x0 + x1) * .5f;
            const FVector R0(CX, y0 + Half, ZR), R1(CX, y1 - Half, ZR);
            SlopeQuadM(M, Shingle, FVector(x0, y1, ZE), FVector(x0, y0, ZE), R0, R1, ShingleUV);
            SlopeQuadM(M, Shingle, FVector(x1, y0, ZE), FVector(x1, y1, ZE), R1, R0, ShingleUV);
            SlopeQuadM(M, Shingle, FVector(x0, y0, ZE), FVector(x1, y0, ZE), R0, R0, ShingleUV);
            SlopeQuadM(M, Shingle, FVector(x1, y1, ZE), FVector(x0, y1, ZE), R1, R1, ShingleUV);
            Bar(M, Metal, R0 + FVector(0, 0, 2), R1 + FVector(0, 0, 2), 6, 3, PlainUV);
        }
    }
    /** Roof surface height of a hipped roof at (X,Y) (for chimneys). */
    static float HipZ(float X, float Y, float X0, float X1, float Y0, float Y1, float Over, float ZE, float P)
    {
        const float x0 = X0 - Over, x1 = X1 + Over, y0 = Y0 - Over, y1 = Y1 + Over;
        const float Dx = FMath::Min(X - x0, x1 - X), Dy = FMath::Min(Y - y0, y1 - Y);
        return ZE + FMath::Max(0.f, FMath::Min(Dx, Dy)) * P;
    }
    void Chimney(float CX, float CY, float ZBase, float ZTopC, float W, float D)
    {
        Box(House, BrickRed, CX - W / 2, CX + W / 2, CY - D / 2, CY + D / 2, ZBase, ZTopC, BrickUV);
        Box(House, Stone, CX - W / 2 - 8, CX + W / 2 + 8, CY - D / 2 - 8, CY + D / 2 + 8, ZTopC, ZTopC + 10, PlainUV);
        Box(House, Black, CX - W / 4, CX - W / 4 + 22, CY - 11, CY + 11, ZTopC + 10, ZTopC + 32, PlainUV);
        Box(House, Black, CX + W / 4 - 22, CX + W / 4, CY - 11, CY + 11, ZTopC + 10, ZTopC + 32, PlainUV);
    }

    // ---------------------------------------------------------------- arcade (round arches between brick piers, wall runs along x)
    void Arcade(const TArray<float>& Piers, float Y0, float Y1, float ZB, float ZSpring, float ZT)
    {
        for (int32 I = 0; I + 1 < Piers.Num(); I += 2) Box(House, Brick, Piers[I], Piers[I + 1], Y0, Y1, ZB, ZT, BrickUV);
        for (int32 I = 1; I + 2 < Piers.Num(); I += 2)
        {
            const float A = Piers[I], B = Piers[I + 1];
            const float Rd = (B - A) * .5f, Cx = (A + B) * .5f;
            HouseBrick::Arch(BrickDecor,Cx,ZSpring,Rd,Y1+.12f,1.f,ZT);
            HouseBrick::Arch(BrickDecor,Cx,ZSpring,Rd,Y0-.12f,-1.f,ZT);
            const int32 N = 14;
            for (int32 K = 0; K < N; ++K)
            {
                const float T0 = PI * K / N, T1 = PI * (K + 1) / N;
                const float Xa = Cx + Rd * FMath::Cos(T0), Xb = Cx + Rd * FMath::Cos(T1);
                const float Za = ZSpring + Rd * FMath::Sin(T0), Zb = ZSpring + Rd * FMath::Sin(T1);
                // outer face (+y) and inner face (-y)
                House.Quad(Brick, FVector(Xa, Y1, ZT), FVector(Xb, Y1, ZT), FVector(Xb, Y1, Zb), FVector(Xa, Y1, Za), FVector(0, 1, 0), BrickUV);
                House.Quad(Brick, FVector(Xa, Y0, ZT), FVector(Xb, Y0, ZT), FVector(Xb, Y0, Zb), FVector(Xa, Y0, Za), FVector(0, -1, 0), BrickUV);
                // intrados
                const FVector Nm = -FVector(FMath::Cos((T0 + T1) * .5f), 0, FMath::Sin((T0 + T1) * .5f));
                House.Quad(Stone, FVector(Xa, Y0, Za), FVector(Xb, Y0, Zb), FVector(Xb, Y1, Zb), FVector(Xa, Y1, Za), Nm, PlainUV);
            }
        }
    }

    // ---------------------------------------------------------------- the house shell
    void BuildHouse()
    {
        TArray<FExWall> Walls;
        auto W = [&](int32 Axis, float Sign, float FaceC, float Thick, float U0, float U1, float Z0, float Z1, std::initializer_list<FExWin> Wins, float MuntinD = 0.f)
        {
            FExWall X; X.Axis = Axis; X.Sign = Sign; X.Face = FaceC; X.Thick = Thick; X.U0 = U0; X.U1 = U1; X.Z0 = Z0; X.Z1 = Z1; X.MuntinD = MuntinD;
            for (const FExWin& Wn : Wins) X.Wins.Add(Wn);
            Walls.Add(X);
        };
        const float ZB = -400.f;
        // Centre block front (faces -y). Outside the portico it rises to the cornice; inside, the scanned balcony sits above z 310.
        W(1, -1, -1010, 45, -1420, -830, ZB, ZTop, { {-1172, -1016, 37, 264, 0}, {-1167, -1003, 398, 562, 0} });
        // The central entrance has its own fitted walnut double-door frame.
        // Keep the opening free of the old fixed glass transom bars.
        W(1, -1, -1010, 45, -830, 790, ZB, 296, { {-548, -412, 39, 240, 0}, {-186, 113, -2, 270, 3}, {408, 596, 39, 240, 0} });
        W(1, -1, -1010, 45, 790, 1360, ZB, ZTop, { {1006, 1135, 37, 252, 0}, {1003, 1167, 398, 562, 0} });
        // Left wing: front (with the hole the impossible hallway leaves through), inner face, end, rear (jogged at the stair hall).
        W(1, -1, -2200, 55, -3400, -1420, ZB, ZTop, { {-3150, -3000, 40, 240, 1}, {-2850, -2700, 40, 240, 1}, {-1900, -1750, 40, 240, 1},
            {-3150, -3000, 398, 562, 1}, {-2850, -2700, 398, 562, 1}, {-1900, -1750, 398, 562, 1} });
        W(0, 1, -1420, 50, -2200, -1010, ZB, ZTop, { {-1900, -1750, 40, 240, 1}, {-1400, -1250, 40, 240, 1}, {-1900, -1750, 398, 562, 1}, {-1400, -1250, 398, 562, 1} });
        W(0, -1, -3400, 70, -2200, 1540, ZB, ZTop, { {-39, 157, 415, 559, 0}, {1042, 1238, 417, 562, 0}, {-1470, -1316, 395, 580, 1},
            {-39, 157, 40, 240, 1}, {1042, 1238, 40, 240, 1}, {-1470, -1316, 40, 240, 1}, {-900, -720, 40, 240, 1}, {-900, -720, 415, 559, 1}, {-1950, -1800, 40, 240, 1}, {-1950, -1800, 415, 559, 1} });
        W(1, 1, 1540, 55, -3400, -2150, ZB, ZTop, { {-3114, -2922, 418, 562, 0}, {-2650, -2460, 418, 562, 1}, {-3114, -2922, 40, 240, 1}, {-2650, -2460, 40, 240, 1},
            {-3114, -2922, -250, -110, 1}, {-2650, -2460, -250, -110, 1} });
        W(1, 1, 1480, 60, -2150, -1530, ZB, ZTop, { {-2521, -2341, 374, 602, 0}, {-1868, -1710, 366, 538, 0}, {-1868, -1710, 40, 240, 1}, {-1868, -1710, -250, -110, 1} });
        W(0, 1, -2150, 45, 1480, 1540, ZB, ZTop, {});
        // Right wing: inner face, front, end (with the porte-cochere door), rear.
        W(0, -1, 1360, 55, -2130, -1010, ZB, ZTop, { {-1877, -1677, 425, 570, 0}, {-1877, -1677, 40, 240, 1} }, 65.f);
        W(1, -1, -2130, 60, 1360, 3340, ZB, ZTop, { {2305, 2448, 386, 547, 0}, {1700, 1850, 386, 547, 1}, {2900, 3050, 386, 547, 1},
            {1700, 1850, 40, 240, 1}, {2305, 2448, 40, 240, 1}, {2900, 3050, 40, 240, 1} });
        W(0, 1, 3340, 60, -2130, 1540, ZB, ZTop, { {-1143, -980, -2, 222, 2}, {-446, -264, 41, 214, 0}, {289, 472, 41, 214, 0}, {1014, 1197, 41, 214, 0},
            {-1806, -1612, 430, 574, 0}, {1111, 1305, 430, 575, 0}, {-446, -264, 410, 570, 1}, {289, 472, 410, 570, 1} });
        W(1, 1, 1540, 55, 1530, 3340, ZB, ZTop, { {1753, 1902, 50, 192, 0}, {2853, 3002, 47, 210, 0}, {1567, 1742, 427, 572, 0}, {2089, 2289, 428, 572, 0},
            {2853, 3002, 427, 572, 1}, {2089, 2289, 47, 210, 1}, {1753, 1902, -250, -110, 1}, {2853, 3002, -250, -110, 1} });
        for (const FExWall& X : Walls) BuildWall(X);
        // Cornices, extended round outside corners.
        auto Cor = [&](int32 I, float E0, float E1) { Cornice(Walls[I], E0, E1); };
        Cor(0, 60, 0); Cor(2, 0, 0); Cor(3, 60, 60); Cor(4, 60, 0); Cor(5, 60, 60); Cor(6, 60, 60); Cor(7, 0, 190); Cor(8, 0, 60);
        Cor(9, 60, 0); Cor(10, 60, 60); Cor(11, 60, 60); Cor(12, 190, 60);
        // Walls above the porch roof (wings facing the pergola) and the deck-end skins.
        Box(House, Brick, -1470, -1400, 950, 1480, 372, ZTop, BrickUV);
        Box(House, Brick, 1400, 1470, 950, 1540, 372, ZTop, BrickUV);
        Box(House, Trim, -1400, -1340, 950, 1540, 641, 691, PlainUV);
        Box(House, Trim, 1340, 1400, 950, 1600, 641, 691, PlainUV);
        Box(House, Trim, -1400, 1400, 950, 1010, 640, 692, PlainUV);
        Box(House, Brick, -1530, -1470, 1480, 1760, ZB, 380, BrickUV);
        Box(House, Brick, 1470, 1530, 1540, 1760, ZB, 380, BrickUV);
        Box(House, Stone, -1538, -1462, 1480, 1768, 380, 392, PlainUV);
        Box(House, Stone, 1462, 1538, 1540, 1768, 380, 392, PlainUV);
        // Roofs.
        HipRoof(-3400, -1420, -2200, 1540, 60, ZEave, RoofPitch);
        HipRoof(-1420, 1360, -1010, 950, 60, ZEave, RoofPitch);
        HipRoof(1360, 3340, -2130, 1540, 60, ZEave, RoofPitch);
        Chimney(-2600, 300, HipZ(-2600, 300, -3400, -1420, -2200, 1540, 60, ZEave, RoofPitch) - 60, 1180, 130, 70);
        Chimney(2350, -500, HipZ(2350, -500, 1360, 3340, -2130, 1540, 60, ZEave, RoofPitch) - 60, 1230, 130, 70);
        Chimney(900, 600, HipZ(900, 600, -1420, 1360, -1010, 950, 60, ZEave, RoofPitch) - 60, 1110, 110, 70);
        BuildPortico();
        BuildPorch();
        BuildPorteCochere();
    }

    void BuildPortico()
    {
        // Open porch ends: the columns support the balcony without tall brick side walls.
        Box(House, Stone, -836, 796, -1491, -1010, -22, -8, PlainUV);
        // The photo-matched concrete forecourt supplies the floor and centered steps.
        // Retain the fitted threshold through the wall at the walnut front doors.
        Box(House, Stone, -200, 130, -1012, -918, -30, -1, PlainUV);
        // Ceiling under the balcony, four columns with bases and capitals.
        Box(House, Trim, -805, 765, -1485, -1010, 296, 306, PlainUV);
        for (float CX : { -680.f, -400.f, 260.f, 620.f })
        {
            House.Cylinder(Trim, FVector(CX, -1410, 10), FVector(CX, -1410, 272), 34, 16, PlainUV, false);
            House.Cylinder(Trim, FVector(CX, -1410, 272), FVector(CX, -1410, 284), 40, 16, PlainUV, true);
            Box(House, Trim, CX - 45, CX + 45, -1455, -1365, -2, 10, PlainUV);
            Box(House, Trim, CX - 45, CX + 45, -1455, -1365, 284, 296, PlainUV);
        }
        // Entablature over the scanned balcony (frieze with the chapter name), cornice and pediment.
        Box(House, Trim, -830, 790, -1485, -1010, 628, 691, PlainUV);
        House.QuadUV(Frieze, FVector(-700, -1486, 636), FVector(660, -1486, 636), FVector(660, -1486, 686), FVector(-700, -1486, 686), FVector(0, -1, 0),
            FVector2D(1, 1), FVector2D(0, 1), FVector2D(0, 0), FVector2D(1, 0), FVector(-1, 0, 0));
        Box(House, Trim, -850, 810, -1510, -1010, 692, 712, PlainUV);
        const float Apex = 712.f + 810.f * .42f;
        const FVector TL(-830, -1485, 712), TR(790, -1485, 712), TA(-20, -1485, Apex);
        Tri(House, Trim, TL, TR, TA, FVector(0, -1, 0), PlainUV);                     // tympanum
        const FVector BL(-850, -1510, 712), BR(810, -1510, 712), BA(-20, -1510, Apex + 10);
        const FVector KL(-850, 40, 712), KR(810, 40, 712), KA(-20, 40, Apex + 10);
        SlopeQuad(Shingle, BL, BA, KA, KL, ShingleUV);
        SlopeQuad(Shingle, BA, BR, KR, KA, ShingleUV);
        Bar(House, Trim, BL + FVector(0, 0, -2), BA + FVector(0, 0, -2), 14, 12, PlainUV);  // raking cornices
        Bar(House, Trim, BA + FVector(0, 0, -2), BR + FVector(0, 0, -2), 14, 12, PlainUV);
        // Porch lanterns either side of the door and a hanging lantern.
        for (float LX : { -260.f, 190.f })
        {
            Box(House, Metal, LX - 10, LX + 10, -1012, -1000, 170, 210, PlainUV);
            Box(House, Lamp, LX - 7, LX + 7, -1010, -1003, 178, 202, PlainUV);
            LightSpecs.Add({ FVector(LX, -1030, 190), FLinearColor(1.f, .78f, .5f), 1300.f, 600.f, 8000.f });
        }
        Box(House, Metal, -40, 0, -1265, -1225, 240, 296, PlainUV);
        Box(House, Lamp, -34, -6, -1259, -1231, 246, 276, PlainUV);
        LightSpecs.Add({ FVector(-20, -1245, 230), FLinearColor(1.f, .8f, .55f), 1800.f, 1600.f, 8000.f });
    }

    void BuildPorch()
    {
        // Brick wall under the deck, opened in front of the basement's four French patio pairs (HallLayout::Pairs, authored x 603 / 0 / -603 / -1092,
        // frames 270 / 270 / 270 / 230, scaled by 1.1), then the end walls, patio slab and the deck's underside.
        {
            FExWall X; X.Axis = 1; X.Sign = 1.f; X.Face = 965.f; X.Thick = 60.f; X.U0 = -1400.f; X.U1 = 1400.f; X.Z0 = -400.f; X.Z1 = -22.f; X.MuntinD = 0.f;
            for (const FVector2D& D : { FVector2D(663.f, 140.f), FVector2D(0.f, 140.f), FVector2D(-663.f, 140.f), FVector2D(-1201.f, 118.f) })
                X.Wins.Add({ D.X - D.Y, D.X + D.Y, -362.f, -116.f, 2 });
            BuildWall(X);
        }
        Box(House, Brick, -1470, -1400, 965, 1720, -400, -22, BrickUV);
        Box(House, Brick, 1400, 1470, 965, 1720, -400, -22, BrickUV);
        Box(House, Concrete, -1470, 1470, 905, 1720, -400, GR + 3, ConcreteUV);
        Box(House, Dark, -1400, 1400, 965, 1720, -30, -24, PlainUV);
        // Arcade under the deck edge (piers under the porch columns, round arches between).
        Arcade({ -1470, -1330, -880, -620, -150, 110, 580, 840, 1330, 1470 }, 1560, 1720, -400, -290, -22);
        // Patio and pergola lights.
        for (float LX : { -700.f, 700.f })
        {
            Box(House, Lamp, LX - 15, LX + 15, 1330, 1360, -40, -30, PlainUV);
            LightSpecs.Add({ FVector(LX, 1345, -60), FLinearColor(1.f, .8f, .55f), 1000.f, 900.f, 4500.f });
            LightSpecs.Add({ FVector(LX, 1330, 300), FLinearColor(1.f, .8f, .55f), 700.f, 800.f, 4500.f });
        }
        LightSpecs.Add({ FVector(-1620, 1560, 520), FLinearColor(.85f, .9f, 1.f), 2500.f, 1800.f, 6000.f });
        LightSpecs.Add({ FVector(1620, 1560, 520), FLinearColor(.85f, .9f, 1.f), 2500.f, 1800.f, 6000.f });
    }

    void BuildPorteCochere()
    {
        Box(House, Dark, 3340, 4150, -1500, -620, 300, 340, PlainUV);
        Box(House, Brick, 4110, 4150, -1500, -620, 340, 380, BrickUV);
        Box(House, Brick, 3340, 4150, -1500, -1460, 340, 380, BrickUV);
        Box(House, Brick, 3340, 4150, -660, -620, 340, 380, BrickUV);
        Box(House, Stone, 3340, 4158, -1508, -612, 380, 388, PlainUV);
        Box(House, Trim, 3340, 4170, -1520, -600, 296, 318, PlainUV);
        for (float PY : { -1450.f, -670.f })
        {
            Box(House, Brick, 4060, 4120, PY - 30, PY + 30, -400, 296, BrickUV);
            Box(House, Stone, 4052, 4128, PY - 38, PY + 38, -22, -8, PlainUV);
        }
        Box(House, Concrete, 3340, 3470, -1220, -900, -100, -5, ConcreteUV);
        Box(House, Stone, 3228, 3345, -1150, -975, -30, -1, PlainUV);
        Box(House, Concrete, 3470, 3510, -1220, -900, -100, -35, ConcreteUV);
        Box(House, Metal, 3730, 3770, -1080, -1040, 270, 300, PlainUV);
        Box(House, Lamp, 3736, 3764, -1074, -1046, 250, 272, PlainUV);
        LightSpecs.Add({ FVector(3750, -1060, 240), FLinearColor(1.f, .8f, .55f), 2500.f, 900.f, 8000.f });
    }

};

// ------------------------------------------------------------------ grounds and surroundings
struct FExRect { float X0, X1, Y0, Y1; };

void ExSubtract(TArray<FExRect>& Pieces, const FExRect& Cut)
{
    TArray<FExRect> Out;
    for (const FExRect& P : Pieces)
    {
        if (Cut.X1 <= P.X0 || Cut.X0 >= P.X1 || Cut.Y1 <= P.Y0 || Cut.Y0 >= P.Y1) { Out.Add(P); continue; }
        if (Cut.X0 > P.X0) Out.Add({ P.X0, Cut.X0, P.Y0, P.Y1 });
        if (Cut.X1 < P.X1) Out.Add({ Cut.X1, P.X1, P.Y0, P.Y1 });
        const float MX0 = FMath::Max(P.X0, Cut.X0), MX1 = FMath::Min(P.X1, Cut.X1);
        if (Cut.Y0 > P.Y0) Out.Add({ MX0, MX1, P.Y0, Cut.Y0 });
        if (Cut.Y1 < P.Y1) Out.Add({ MX0, MX1, Cut.Y1, P.Y1 });
    }
    Pieces = Out;
}
void ExSplitY(TArray<FExRect>& Pieces, float Y)
{
    TArray<FExRect> Out;
    for (const FExRect& P : Pieces)
    {
        if (Y > P.Y0 + 1.f && Y < P.Y1 - 1.f) { Out.Add({ P.X0, P.X1, P.Y0, Y }); Out.Add({ P.X0, P.X1, Y, P.Y1 }); }
        else Out.Add(P);
    }
    Pieces = Out;
}
void ExSplitX(TArray<FExRect>& Pieces, float X)
{
    TArray<FExRect> Out;
    for (const FExRect& P : Pieces)
    {
        if (X > P.X0 + 1.f && X < P.X1 - 1.f) { Out.Add({ P.X0, X, P.Y0, P.Y1 }); Out.Add({ X, P.X1, P.Y0, P.Y1 }); }
        else Out.Add(P);
    }
    Pieces = Out;
}

/** Grid of quads following the site grade (+Raise). Skirts close the sides down to the grade when the patch sits off the ground plane. */
void ExPatch(FHallMesh& M, FName K, const FExRect& R, float Cell, float Raise, const FHallUV& UV, bool bSkirt, FName SkirtKey = NAME_None)
{
    const int32 NX = FMath::Max(1, FMath::CeilToInt((R.X1 - R.X0) / Cell)), NY = FMath::Max(1, FMath::CeilToInt((R.Y1 - R.Y0) / Cell));
    auto P = [&](int32 I, int32 J) { const float X = R.X0 + (R.X1 - R.X0) * I / NX, Y = R.Y0 + (R.Y1 - R.Y0) * J / NY; return FVector(X, Y, Grade(X, Y) + Raise); };
    for (int32 J = 0; J < NY; ++J)
        for (int32 I = 0; I < NX; ++I)
        {
            const FVector A = P(I, J), B = P(I + 1, J), C = P(I + 1, J + 1), D = P(I, J + 1);
            FVector N = FVector::CrossProduct(B - A, D - A).GetSafeNormal(); if (N.Z < 0) N = -N;
            M.Quad(K, A, B, C, D, N, UV);
        }
    if (!bSkirt) return;
    const FName SK = SkirtKey.IsNone() ? K : SkirtKey;
    auto Skirt = [&](const FVector& A, const FVector& B, const FVector& Nrm)
    {
        const FVector A2(A.X, A.Y, Grade(A.X, A.Y) - 40.f), B2(B.X, B.Y, Grade(B.X, B.Y) - 40.f);
        M.Quad(SK, A, B, B2, A2, Nrm, UV);
    };
    for (int32 I = 0; I < NX; ++I) { Skirt(P(I, 0), P(I + 1, 0), FVector(0, -1, 0)); Skirt(P(I + 1, NY), P(I, NY), FVector(0, 1, 0)); }
    for (int32 J = 0; J < NY; ++J) { Skirt(P(NX, J), P(NX, J + 1), FVector(1, 0, 0)); Skirt(P(0, J + 1), P(0, J), FVector(-1, 0, 0)); }
}

struct FExSite
{
    FExBuilder& B;
    FHallMesh& G;
    TArray<FTransform> TreeA, TreeB, Bush, Poles, CarsA, CarsB, CarsC, Chairs;
    explicit FExSite(FExBuilder& InB) : B(InB), G(InB.Grounds) {}

    bool Free(float X, float Y) const
    {
        for (const FExBuilder::FNoGo& N : B.NoGo) if (X > N.X0 && X < N.X1 && Y > N.Y0 && Y < N.Y1) return false;
        return true;
    }
    void NoGo(float X0, float X1, float Y0, float Y1) { B.NoGo.Add({ X0, X1, Y0, Y1 }); }
    void Slab(FName K, float X0, float X1, float Y0, float Y1, float Z0, float Z1, const FHallUV& UV) { FExBuilder::Box(G, K, X0, X1, Y0, Y1, Z0, Z1, UV); }
    /** Thin painted line lying on a flat surface. */
    void Mark(FName K, float X0, float X1, float Y0, float Y1, float Z) { G.Quad(K, FVector(X0, Y0, Z), FVector(X1, Y0, Z), FVector(X1, Y1, Z), FVector(X0, Y1, Z), FVector::UpVector, B.PlainUV); }
    void Pole(float X, float Y, float Yaw)
    {
        const float Z = Grade(X, Y);
        Poles.Add(FTransform(FRotator(0, Yaw, 0), FVector(X, Y, Z)));
        const FVector Head = FVector(X, Y, Z) + FRotator(0, Yaw, 0).RotateVector(FVector(270, 0, 640));
        B.LightSpecs.Add({ Head, FLinearColor(.95f, .93f, .85f), 9000.f, 1900.f, 5000.f });
    }
    void Car(int32 Kind, float X, float Y, float Yaw)
    {
        const FTransform T(FRotator(0, Yaw, 0), FVector(X, Y, Grade(X, Y)));
        (Kind == 0 ? CarsA : Kind == 1 ? CarsB : CarsC).Add(T);
        NoGo(X - 500, X + 500, Y - 300, Y + 300);
    }

    void Terrain()
    {
        const float E = 32000.f;
        TArray<FExRect> Pieces = { { -E, E, -E, E } };
        for (const FExRect& Cut : { FExRect{ -3400, -1420, -2200, 1540 }, FExRect{ -1420, 1360, -1010, 1720 }, FExRect{ 1360, 3340, -2130, 1540 },
            FExRect{ -E, 8700, StreetY(-4350), StreetY(-2950) }, FExRect{ 8700, 10300, -E, E } }) ExSubtract(Pieces, Cut);
        ExSplitY(Pieces, SlopeY0); ExSplitY(Pieces, SlopeY1);
        for (float S : { -7000.f, 7000.f }) ExSplitX(Pieces, S);
        for (float S : { -5000.f, 5000.f }) ExSplitY(Pieces, S);
        for (const FExRect& P : Pieces)
        {
            const bool bNear = P.X1 <= 7000.f && P.X0 >= -7000.f && P.Y1 <= 5000.f && P.Y0 >= -5000.f;
            const bool bSlope = P.Y0 >= SlopeY0 - 1.f && P.Y1 <= SlopeY1 + 1.f;
            ExPatch(G, B.Grass, P, bNear ? 200.f : (bSlope ? 400.f : 1600.f), 0.f, B.GrassUV, false);
        }
    }

    void Streets()
    {
        const float E = 32000.f;
        // Fraternity Park Dr (flat, in front) and Pat Head Summitt St (runs down the slope at the +x end).
        ExPatch(G, B.Asphalt, { -E, 8700, StreetY(-4350), StreetY(-2950) }, 4000.f, -12.f, B.AsphaltUV, false);
        ExPatch(G, B.Asphalt, { 8700, 10300, -E, E }, 400.f, -12.f, B.AsphaltUV, true, B.Concrete);
        for (float X = -E + 400; X < 8400; X += 800) Mark(B.Yellow, X, X + 300, StreetY(-3658), StreetY(-3642), GF - 11.5f);
        for (float Y = -E + 400; Y < E; Y += 800) { const float Z = Grade(9500, Y + 150) - 11.5f; if ((Y + 300 > StreetY(-4350) && Y < StreetY(-2950)) || (Y > -400 && Y < 2000)) continue; Mark(B.Yellow, 9492, 9508, Y, Y + 300, Z); }
        Mark(B.Line, -E, 8600, StreetY(-2985), StreetY(-2975), GF - 11.5f); Mark(B.Line, -E, 8600, StreetY(-4325), StreetY(-4315), GF - 11.5f);
        // Sidewalks with curbs (gaps at the drives).
        auto Walk = [&](float X0, float X1, float Y0, float Y1) { Slab(B.Concrete, X0, X1, Y0, Y1, GF - 40, GF + 3, B.ConcreteUV); };
        Walk(-E, -5800, StreetY(-2950), StreetY(-2700)); Walk(-5200, 3450, StreetY(-2950), StreetY(-2700)); Walk(4050, 8450, StreetY(-2950), StreetY(-2700));
        Walk(-E, -5000, StreetY(-4600), StreetY(-4350)); Walk(-4400, 8450, StreetY(-4600), StreetY(-4350));
        ExPatch(G, B.Concrete, { 8450, 8700, -E, StreetY(-4600) }, 400.f, 3.f, B.ConcreteUV, true);
        ExPatch(G, B.Concrete, { 8450, 8700, StreetY(-2700), 2500 }, 400.f, 3.f, B.ConcreteUV, true);
        ExPatch(G, B.Concrete, { 8450, 8700, 3100, E }, 400.f, 3.f, B.ConcreteUV, true);
        ExPatch(G, B.Concrete, { 10300, 10550, -E, E }, 400.f, 3.f, B.ConcreteUV, true);
        Slab(B.Concrete, -E, 8700, StreetY(-4350), StreetY(-4330), GF - 40, GF + 3, B.ConcreteUV);
        Slab(B.Concrete, -E, 8700, StreetY(-2970), StreetY(-2950), GF - 40, GF + 3, B.ConcreteUV);
        // Drives and lots.
        Slab(B.Asphalt, 3450, 4050, StreetY(-2990), -620, GF - 40, GF + 2, B.AsphaltUV);
        ExPatch(G, B.Asphalt, { 3450, 4050, -620, 1900 }, 200.f, 2.f, B.AsphaltUV, true);
        Slab(B.Asphalt, 3450, 6600, 1900, 4500, GR - 40, GR + 2, B.AsphaltUV);
        Slab(B.Asphalt, 6600, 8720, 2500, 3100, GR - 40, GR + 2, B.AsphaltUV);
        Slab(B.Asphalt, -7200, -3700, -2500, -300, GF - 40, GF + 2, B.AsphaltUV);
        Slab(B.Asphalt, -5800, -5200, StreetY(-2990), -2500, GF - 40, GF + 2, B.AsphaltUV);
        Slab(B.Asphalt, -9000, 0, StreetY(-7400), StreetY(-4600), GF - 40, GF + 2, B.AsphaltUV);
        Slab(B.Asphalt, -5000, -4400, StreetY(-4600), StreetY(-4330), GF - 40, GF + 2, B.AsphaltUV);
        for (float X = -7000; X < -3700; X += 500) { Mark(B.Line, X - 5, X + 5, -2450, -1900, GF + 2.5f); Mark(B.Line, X - 5, X + 5, -900, -350, GF + 2.5f); }
        for (float X = -8800; X < 0; X += 500) { Mark(B.Line, X - 5, X + 5, StreetY(-7350), StreetY(-6800), GF + 2.5f); Mark(B.Line, X - 5, X + 5, StreetY(-5250), StreetY(-4700), GF + 2.5f); }
        for (float X = 3600; X < 6600; X += 500) Mark(B.Line, X - 5, X + 5, 3950, 4450, GR + 2.5f);
        // The new concrete approach is aligned to the centered front stair.
        BuildHouseExteriorWalks(B.Walks, B.WalkDecor);
        // Keep the side-door walk and the courtyard routes.
        Slab(B.Concrete, 3340, 3450, HouseExteriorSite::FrontSidewalkInnerY, -1220, GF - 40, GF + 3, B.ConcreteUV);
        NoGo(-E, 8700, StreetY(-4700), StreetY(-2600)); NoGo(8300, 10700, -E, E); NoGo(3300, 4200, StreetY(-3000), 2000); NoGo(3300, 6700, 1800, 4600); NoGo(6500, 8800, 2400, 3200);
        NoGo(-7300, -3600, StreetY(-3000), -200); NoGo(-9100, 100, StreetY(-7500), StreetY(-4500)); NoGo(-1500, 1500, 1800, 3900); NoGo(-500, 420, HouseExteriorSite::FrontSidewalkInnerY-80, -1400); NoGo(3200, 3600, HouseExteriorSite::FrontSidewalkInnerY-80, -1100);
        // Keep the expanded front lawn open rather than filling the old road strip with random trees.
        NoGo(-3550, 3300, HouseExteriorSite::FrontSidewalkInnerY, -2550);
        NoGo(-3800, 4500, -2550, 2200);
    }

    void Courtyard()
    {
        // Photo-matched enclosed court. The old porch-to-court stair stays removed.
        BuildHouseBasketballCourt(G, B.CourtDecor, B.Look);
        for (float X : { -994.f, 994.f })
        {
            G.Cylinder(B.Black, FVector(X, 3820, GR+340), FVector(X, 3820, GR+470), 3.5f, 8, B.PlainUV);
            FExBuilder::Box(B.CourtDecor, B.Grey, X-19, X+19, 3780, 3825, GR+461, GR+476, B.PlainUV);
            FExBuilder::Box(B.CourtDecor, B.Flood, X-16, X+16, 3783, 3818, GR+459, GR+461, B.PlainUV);
            B.LightSpecs.Add({ FVector(X, 3780, GR+449), FLinearColor(.92f, .90f, .82f), 20000.f, 3200.f, 6500.f });
        }
        // Small service lamps wash the condenser and the two grassy banks.
        for (float Side : {-1.f, 1.f})
        {
            FExBuilder::Box(B.CourtDecor, B.Grey, Side<0?-1530:1503, Side<0?-1503:1530, 2500, 2535, GR+315, GR+332, B.PlainUV);
            FExBuilder::Box(B.CourtDecor, B.Flood, Side<0?-1532:1530, Side<0?-1530:1532, 2504, 2531, GR+317, GR+330, B.PlainUV);
            B.LightSpecs.Add({ FVector(Side*1550, 2518, GR+323), FLinearColor(.85f, .90f, 1.f), 5000.f, 1500.f, 5500.f });
        }
        NoGo(-3500, 3500, 1750, 4320);
        // Dumpster at the back corner of the rear lot. Keep the bike rack off the new walking path.
        FExBuilder::Box(G, B.Green, 5900, 6300, 4150, 4450, GR + 12, GR + 150, B.PlainUV);
        FExBuilder::Box(G, B.Green, 5890, 6310, 4140, 4460, GR + 150, GR + 165, B.PlainUV);
        for (float WX : { 5940.f, 6260.f }) for (float WY : { 4180.f, 4420.f }) G.Cylinder(B.Rubber, FVector(WX, WY - 6, GR + 8), FVector(WX, WY + 6, GR + 8), 8, 8, B.PlainUV);
        FExBuilder::Box(B.Walks, TEXT("EX_WalkConcrete"), 4210, 4390, -2070, -1630, GF-25, GF+3, B.ConcreteUV);
        for (float RY = -2020; RY < -1720; RY += 70)
        {
            G.Cylinder(B.Metal, FVector(4235, RY, GF+3), FVector(4235, RY, GF+78), 2.5f, 6, B.PlainUV);
            G.Cylinder(B.Metal, FVector(4300, RY, GF+3), FVector(4300, RY, GF+78), 2.5f, 6, B.PlainUV);
            G.Cylinder(B.Metal, FVector(4235, RY, GF+78), FVector(4300, RY, GF+78), 2.5f, 6, B.PlainUV);
        }
        NoGo(1700, 2600, 2000, 3200); NoGo(5800, 6400, 4100, 4500);
    }

    void Neighbours()
    {
        FHallMesh& N = B.Near; FHallMesh& F = B.Far;
        auto WindowBand = [&](FHallMesh& M, float X0, float X1, float Y0, float Y1, float Z0, float Z1, int32 Lit)
        {
            // Dark glass strips 2 cm proud of each face, with a few lit panes.
            auto Strip = [&](const FVector& A, const FVector& Bv, const FVector& Nrm, float Len)
            {
                const FVector Dir = (Bv - A).GetSafeNormal();
                for (float S = 120; S + 220 < Len; S += 320)
                {
                    const FVector P0 = A + Dir * S, P1 = A + Dir * (S + 220);
                    const bool bLit = Lit > 0 && (int32)(S / 320) % Lit == 1;
                    M.Quad(bLit ? B.Window : B.Glass, P0 + FVector(0, 0, Z0), P1 + FVector(0, 0, Z0), P1 + FVector(0, 0, Z1), P0 + FVector(0, 0, Z1), Nrm, B.PlainUV);
                }
            };
            Strip(FVector(X0, Y0 - 2, 0), FVector(X1, Y0 - 2, 0), FVector(0, -1, 0), X1 - X0);
            Strip(FVector(X1, Y1 + 2, 0), FVector(X0, Y1 + 2, 0), FVector(0, 1, 0), X1 - X0);
            Strip(FVector(X0 - 2, Y1, 0), FVector(X0 - 2, Y0, 0), FVector(-1, 0, 0), Y1 - Y0);
            Strip(FVector(X1 + 2, Y0, 0), FVector(X1 + 2, Y1, 0), FVector(1, 0, 0), Y1 - Y0);
        };
        // Phi Kappa Psi: modern beige block behind-left.
        FExBuilder::Box(N, B.Beige, -11500, -6500, 1000, 4500, GR - 60, GR + 900, B.PlainUV);
        FExBuilder::Box(N, B.Dark, -11540, -6460, 960, 4540, GR + 900, GR + 930, B.PlainUV);
        WindowBand(N, -11500, -6500, 1000, 4500, GR + 110, GR + 270, 4); WindowBand(N, -11500, -6500, 1000, 4500, GR + 470, GR + 630, 3);
        FExBuilder::Box(N, B.Dark, -9100, -8900, 990, 1000, GR + 3, GR + 220, B.PlainUV);
        B.LightSpecs.Add({ FVector(-9000, 900, GR + 250), FLinearColor(1.f, .8f, .55f), 1200.f, 900.f, 7000.f });
        NoGo(-11800, -6200, 700, 4800);
        // Student Health Center across the street, with its lot in front.
        FExBuilder::Box(N, B.Wall, -9500, -500, StreetY(-11600), StreetY(-7600), GF - 60, GF + 1100, B.PlainUV);
        FExBuilder::Box(N, B.Dark, -9540, -460, StreetY(-11640), StreetY(-7560), GF + 1100, GF + 1140, B.PlainUV);
        for (int32 K = 0; K < 3; ++K) WindowBand(N, -9500, -500, StreetY(-11600), StreetY(-7600), GF + 100 + K * 360, GF + 260 + K * 360, 3 + K);
        FExBuilder::Box(N, B.Dark, -5300, -4700, StreetY(-7600), StreetY(-7000), GF + 300, GF + 340, B.PlainUV);
        for (float PX : { -5250.f, -4750.f }) G.Cylinder(B.Metal, FVector(PX, StreetY(-7050), GF), FVector(PX, StreetY(-7050), GF + 300), 12, 8, B.PlainUV);
        B.LightSpecs.Add({ FVector(-5000, StreetY(-7300), GF + 280), FLinearColor(.9f, .95f, 1.f), 3000.f, 1500.f, 9000.f });
        NoGo(-9800, -200, StreetY(-11900), StreetY(-7300));
        // Alpha Tau Omega and Sigma Alpha Epsilon further along the street: brick, hipped roofs, white columns.
        auto FratHouse = [&](FHallMesh& M, float X0, float X1, float Y0, float Y1, float H, bool bColumnsNorth)
        {
            FExBuilder::Box(M, B.Brick, X0, X1, Y0, Y1, GF - 60, GF + H, B.BrickUV);
            FExBuilder::Box(M, B.Trim, X0 - 50, X1 + 50, Y0 - 50, Y1 + 50, GF + H - 45, GF + H, B.PlainUV);
            B.HipRoofM(M, X0, X1, Y0, Y1, 50, GF + H, RoofPitch);
            const float CY = bColumnsNorth ? Y1 + 160 : Y0 - 160;
            FExBuilder::Box(M, B.Trim, (X0 + X1) * .5f - 700, (X0 + X1) * .5f + 700, bColumnsNorth ? Y1 : CY - 40, bColumnsNorth ? CY + 40 : Y0, GF + H - 120, GF + H - 60, B.PlainUV);
            for (int32 K = 0; K < 4; ++K)
            {
                const float CX = (X0 + X1) * .5f - 540 + K * 360;
                M.Cylinder(B.Trim, FVector(CX, CY, GF - 20), FVector(CX, CY, GF + H - 120), 28, 10, B.PlainUV);
            }
            for (int32 Lv = 0; Lv < 2; ++Lv)
                for (float WX = X0 + 300; WX < X1 - 200; WX += 420)
                {
                    const float WY = bColumnsNorth ? Y1 + 2 : Y0 - 2, WZ = GF + 60 + Lv * 330;
                    const FVector Nrm = bColumnsNorth ? FVector(0, 1, 0) : FVector(0, -1, 0);
                    M.Quad(((int32)(WX / 420) + Lv) % 3 == 0 ? B.Window : B.Glass, FVector(WX, WY, WZ), FVector(WX + 150, WY, WZ), FVector(WX + 150, WY, WZ + 200), FVector(WX, WY, WZ + 200), Nrm, B.PlainUV);
                }
        };
        FratHouse(N, -17500, -12000, StreetY(-9500), StreetY(-5200), 760, true);
        FratHouse(N, -24000, -18500, -4500, -600, 720, false);
        FratHouse(F, -26500, -21000, 1500, 5500, 740, false);
        NoGo(-17800, -11700, StreetY(-9800), StreetY(-4900)); NoGo(-24300, -18200, -4800, -300); NoGo(-26800, -20700, 1200, 5800);
        // Lindsey Nelson Stadium: faceted grandstand behind the house, light towers, a glow over the field.
        const FVector SC(2500, 12500, 0); const float Rd = 6000.f;
        const int32 NF = 12; const float A0 = FMath::DegreesToRadians(205.f), A1 = FMath::DegreesToRadians(335.f);
        for (int32 K = 0; K < NF; ++K)
        {
            const float T0 = A0 + (A1 - A0) * K / NF, T1 = A0 + (A1 - A0) * (K + 1) / NF;
            const FVector P0 = SC + FVector(FMath::Cos(T0), FMath::Sin(T0), 0) * Rd, P1 = SC + FVector(FMath::Cos(T1), FMath::Sin(T1), 0) * Rd;
            const FVector Q0 = SC + FVector(FMath::Cos(T0), FMath::Sin(T0), 0) * (Rd - 700), Q1 = SC + FVector(FMath::Cos(T1), FMath::Sin(T1), 0) * (Rd - 700);
            FVector Nrm = FVector::CrossProduct(P1 - P0, FVector::UpVector).GetSafeNormal(); if (FVector::DotProduct(Nrm, P0 - SC) < 0) Nrm = -Nrm;
            N.Quad(B.Brick, P0 + FVector(0, 0, GR - 60), P1 + FVector(0, 0, GR - 60), P1 + FVector(0, 0, GR + 820), P0 + FVector(0, 0, GR + 820), Nrm, B.BrickUV);
            N.Quad(B.Stone, P0 + FVector(0, 0, GR + 820), P1 + FVector(0, 0, GR + 820), P1 + FVector(0, 0, GR + 880), P0 + FVector(0, 0, GR + 880), Nrm, B.PlainUV);
            N.Quad(B.Stone, P0 + FVector(0, 0, GR + 880), P1 + FVector(0, 0, GR + 880), Q1 + FVector(0, 0, GR + 880), Q0 + FVector(0, 0, GR + 880), FVector::UpVector, B.PlainUV);
            N.Quad(B.Dark, Q0 + FVector(0, 0, GR + 880), Q1 + FVector(0, 0, GR + 880), Q1 + FVector(0, 0, GR + 1300), Q0 + FVector(0, 0, GR + 1300), Nrm, B.PlainUV);
            N.Quad(B.Wall, Q0 + FVector(0, 0, GR + 1300), Q1 + FVector(0, 0, GR + 1300), Q1 + FVector(0, 0, GR + 2000), Q0 + FVector(0, 0, GR + 2000), Nrm, B.PlainUV);
            FExBuilder::Tri(N, B.Dark, Q1 + FVector(0, 0, GR + 2000), Q0 + FVector(0, 0, GR + 2000), SC + FVector(0, 0, GR + 2000), FVector::UpVector, B.PlainUV);
            N.Cylinder(B.Wall, P0 + FVector(0, 0, GR + 880), P0 + FVector(0, 0, GR + 1300), 30, 8, B.PlainUV);
            for (float S = 150; S + 150 < (P1 - P0).Size(); S += 420)
            {
                const FVector W0 = P0 + (P1 - P0).GetSafeNormal() * S + Nrm * 2, W1 = W0 + (P1 - P0).GetSafeNormal() * 160;
                N.Quad((K + (int32)(S / 420)) % 4 == 0 ? B.Window : B.Glass, W0 + FVector(0, 0, GR + 300), W1 + FVector(0, 0, GR + 300), W1 + FVector(0, 0, GR + 520), W0 + FVector(0, 0, GR + 520), Nrm, B.PlainUV);
            }
        }
        for (float Deg : { 225.f, 270.f, 315.f })
        {
            const float T = FMath::DegreesToRadians(Deg);
            const FVector Base = SC + FVector(FMath::Cos(T), FMath::Sin(T), 0) * (Rd - 1100) + FVector(0, 0, GR);
            N.Cylinder(B.Metal, Base, Base + FVector(0, 0, 3600), 35, 8, B.PlainUV);
            const FVector In = (SC - Base).GetSafeNormal2D();
            const FVector Ctr = Base + FVector(0, 0, 3500) + In * 60;
            N.Box(B.Flood, FTransform(FRotationMatrix::MakeFromX(In).ToQuat(), Ctr), FVector(30, 320, 180), B.PlainUV);
            B.LightSpecs.Add({ Ctr + In * 200 + FVector(0, 0, -200), FLinearColor(.85f, .9f, 1.f), 60000.f, 6000.f, 16000.f });
        }
        NoGo(-4500, 9500, 6000, 20000);
        // Haslam Field beyond Pat Head Summitt: fenced practice field with its own poles.
        ExPatch(F, B.Field, { 10800, 19500, -7500, 6500 }, 2000.f, 2.f, B.PlainUV, false);
        for (float X = 10800; X <= 19500; X += 580)
            for (float Y : { -7500.f, 6500.f }) F.Cylinder(B.Metal, FVector(X, Y, Grade(X, Y)), FVector(X, Y, Grade(X, Y) + 240), 3, 6, B.PlainUV);
        for (float Y = -7500; Y <= 6500; Y += 580) F.Cylinder(B.Metal, FVector(10800, Y, Grade(10800, Y)), FVector(10800, Y, Grade(10800, Y) + 240), 3, 6, B.PlainUV);
        FExBuilder::Bar(F, B.Metal, FVector(10800, -7500, GF + 240), FVector(10800, 0, GF + 240), 2, 2, B.PlainUV);
        FExBuilder::Bar(F, B.Metal, FVector(10800, 0, GF + 240), FVector(10800, 1600, GR + 240), 2, 2, B.PlainUV);
        FExBuilder::Bar(F, B.Metal, FVector(10800, 1600, GR + 240), FVector(10800, 6500, GR + 240), 2, 2, B.PlainUV);
        FExBuilder::Bar(F, B.Metal, FVector(10800, -7500, GF + 240), FVector(19500, -7500, GF + 240), 2, 2, B.PlainUV);
        FExBuilder::Bar(F, B.Metal, FVector(10800, 6500, GR + 240), FVector(19500, 6500, GR + 240), 2, 2, B.PlainUV);
        NoGo(10600, 19700, -7700, 6700);
        // Distant campus: dark blocks with a few lit windows, well inside the fog.
        FRandomStream R2(77);
        for (int32 K = 0; K < 26; ++K)
        {
            const float Ang = 2 * PI * K / 26 + R2.FRand() * .2f, Dist = 24000 + R2.FRand() * 5000;
            const float CX = FMath::Cos(Ang) * Dist, CY = FMath::Sin(Ang) * Dist;
            if (CY > 6000 && FMath::Abs(CX - 2500) < 9000) continue;
            const float HW = 1200 + R2.FRand() * 1800, HD = 1000 + R2.FRand() * 1500, H = 700 + R2.FRand() * 1600;
            const float ZB = Grade(CX, CY) - 100;
            FExBuilder::Box(F, R2.FRand() < .5f ? B.Brick : B.Wall, CX - HW, CX + HW, CY - HD, CY + HD, ZB, ZB + H, B.BrickUV);
            for (int32 Wn = 0; Wn < 6; ++Wn)
            {
                const float WX = CX - HW + 200 + R2.FRand() * (2 * HW - 500), WZ = ZB + 150 + R2.FRand() * (H - 350);
                const float WY = CY < 0 ? CY + HD + 2 : CY - HD - 2; const FVector Nrm(0, CY < 0 ? 1.f : -1.f, 0);
                F.Quad(B.Window, FVector(WX, WY, WZ), FVector(WX + 140, WY, WZ), FVector(WX + 140, WY, WZ + 180), FVector(WX, WY, WZ + 180), Nrm, B.PlainUV);
            }
        }
    }

    void Boundary()
    {
        // Hedges just inside the playable edge, gaps where the streets run through.
        const float Eg = 18800.f;
        auto HedgeRun = [&](float X0, float X1, float Y0, float Y1)
        {
            const float Z = Grade((X0 + X1) * .5f, (Y0 + Y1) * .5f);
            FExBuilder::Box(G, B.Hedge, X0, X1, Y0, Y1, Z - 20, Z + 190, B.PlainUV);
        };
        HedgeRun(-Eg, 8650, -Eg - 60, -Eg + 60); HedgeRun(10350, Eg, -Eg - 60, -Eg + 60);
        HedgeRun(-Eg, 8650, Eg - 60, Eg + 60); HedgeRun(10350, Eg, Eg - 60, Eg + 60);
        HedgeRun(-Eg - 60, -Eg + 60, -Eg, StreetY(-4400)); HedgeRun(-Eg - 60, -Eg + 60, StreetY(-2900), Eg);
        HedgeRun(Eg - 60, Eg + 60, -Eg, Eg);
    }

    bool NearView(float X, float Y) const
    {
        for (const FExView& V : ExViews) if (!FString(V.Name).StartsWith(TEXT("nature_")) && !FString(V.Name).StartsWith(TEXT("detail_")) && FMath::Abs(V.Pos.X - X) < 900.f && FMath::Abs(V.Pos.Y - Y) < 900.f) return true;
        return false;
    }
    void Plants()
    {
        // Foundation shrubs along the front faces, trees on a jittered grid wherever nothing else is (never on a camera spot).
        for (const FExView& V : ExViews) if (!FString(V.Name).StartsWith(TEXT("nature_")) && !FString(V.Name).StartsWith(TEXT("detail_"))) NoGo(V.Pos.X - 800, V.Pos.X + 800, V.Pos.Y - 800, V.Pos.Y + 800);
        auto Shrubs = [&](float X0, float X1, float Y, float Step)
        {
            for (float X = X0; X <= X1; X += Step) Bush.Add(FTransform(FRotator(0, B.Rng.FRand() * 360, 0), FVector(X + FExBuilder::R(B.Rng, -15, 15), Y, Grade(X, Y)), FVector(FExBuilder::R(B.Rng, .8f, 1.2f), FExBuilder::R(B.Rng, .8f, 1.2f), FExBuilder::R(B.Rng, .8f, 1.15f))));
        };
        Shrubs(-1300, -570, -2390, 160); Shrubs(500, 1240, -2390, 160);
        Shrubs(-3300, -2650, -2550, 160); Shrubs(-2150, -1550, -2550, 160);
        Shrubs(1480, 3250, -2480, 160);
        for (float Y = -1900; Y <= 1300; Y += 180) Bush.Add(FTransform(FRotator(0, B.Rng.FRand() * 360, 0), FVector(-3660, Y, Grade(-3660, Y))));
        for (float Y = -1360; Y <= -100; Y += 180) Bush.Add(FTransform(FRotator(0, B.Rng.FRand() * 360, 0), FVector(4260, Y, Grade(4260, Y))));
        auto Tree = [&](float X, float Y, bool bBig)
        {
            if (NearView(X, Y)) return;
            const float S = FExBuilder::R(B.Rng, .85f, 1.25f);
            (bBig ? TreeA : TreeB).Add(FTransform(FRotator(0, B.Rng.FRand() * 360, 0), FVector(X, Y, Grade(X, Y) - 5), FVector(S, S, S * FExBuilder::R(B.Rng, .9f, 1.1f))));
        };
        Tree(-3000, -2600, true); Tree(2200, -2650, true);
        Tree(-4200, 1200, true); Tree(4300, 800, true); Tree(-3170, 4450, false); Tree(3200, 4450, true); Tree(-4800, -2600, false);
        for (float X = -18000; X < 8000; X += 2400)
        {
            if (FMath::Abs(X + 5500) < 800 || FMath::Abs(X - 3750) < 800 || FMath::Abs(X) < 1500) continue;
            Tree(X + FExBuilder::R(B.Rng, -200, 200), StreetY(-2600), false);
            Tree(X + 1200 + FExBuilder::R(B.Rng, -200, 200), StreetY(-4700), false);
        }
        for (float X = -27000; X < 27000; X += 1900)
            for (float Y = -27000; Y < 27000; Y += 1900)
            {
                const float PX = X + FExBuilder::R(B.Rng, -600, 600), PY = Y + FExBuilder::R(B.Rng, -600, 600);
                const float Dist = FMath::Max(FMath::Abs(PX), FMath::Abs(PY));
                const float Chance = Dist > 19500.f ? .42f : .16f;
                if (B.Rng.FRand() > Chance) continue;
                if (Dist <= 19500.f && !Free(PX, PY)) continue;
                if (Dist > 19500.f && PY > 6000 && FMath::Abs(PX - 2500) < 9500) continue;
                Tree(PX, PY, B.Rng.FRand() < .5f);
            }
    }

    void Lamps()
    {
        for (float X = -15000; X <= 6000; X += 3000) Pole(X + 1500, StreetY(-2640), -90);
        for (float X = -13500; X <= 7500; X += 3000) Pole(X, StreetY(-4660), 90);
        for (float Y = -15000; Y <= 15000; Y += 3000) { if (Y > StreetY(-4600) && Y < StreetY(-2500)) continue; Pole(8420, Y, 0); }
        Pole(-5500, -1400, 180); Pole(-4200, -2300, 90); Pole(5000, 3200, 180); Pole(-7000, StreetY(-5500), 180); Pole(-2500, StreetY(-6500), 180); Pole(-6500, StreetY(-7400), 0);
    }

    void Vehicles()
    {
        int32 K = 0;
        auto Row = [&](float X0, float X1, float Step, float Y, float Yaw, float Skip)
        {
            for (float X = X0; X <= X1; X += Step) { if (B.Rng.FRand() < Skip) continue; Car(K++ % 3, X, Y, Yaw + FExBuilder::R(B.Rng, -2, 2)); }
        };
        Row(-9000, 6000, 1300, StreetY(-4150), 0, .45f);       // kerb parking across the street
        Row(-6750, -3950, 500, -2150, 90, .3f);                 // front lot, two rows
        Row(-6750, -3950, 500, -650, -90, .3f);
        Row(3800, 6300, 500, 4100, 90, .4f);                     // rear lot
        Row(-8550, -250, 500, StreetY(-7050), 90, .35f);        // health-centre lot
        Row(-8550, -250, 500, StreetY(-4950), -90, .35f);
    }

    void BuildAll()
    {
        Terrain(); Streets(); Courtyard(); Neighbours(); Boundary(); Lamps(); Vehicles(); Plants();
    }
};

// ------------------------------------------------------------------ instanced meshes (local space, origin on the ground)
void ExPoleMesh(FExBuilder& B, FHallMesh& M, TArray<FBox>& Col)
{
    M.Cylinder(B.Metal, FVector(0, 0, -30), FVector(0, 0, 640), 9, 8, FHallUV::World(100.f), true);
    M.Cylinder(B.Metal, FVector(0, 0, 0), FVector(0, 0, 40), 16, 8, FHallUV::World(100.f), true);
    M.Cylinder(B.Metal, FVector(0, 0, 630), FVector(220, 0, 655), 5, 6, FHallUV::World(100.f), true);
    FExBuilder::Box(M, B.Metal, 200, 330, -22, 22, 640, 668, B.PlainUV);
    FExBuilder::Box(M, B.Flood, 235, 300, -10, 10, 637, 641, B.PlainUV);
    Col.Add(FBox(FVector(-10, -10, 0), FVector(10, 10, 640)));
}
void ExCarMesh(FExBuilder& B, FHallMesh& M, FName Paint, TArray<FBox>& Col)
{
    HouseCars::Build(M,Paint==B.Red?0:Paint==B.White?1:2,Col);
}

void ExChairMesh(FExBuilder& B, FHallMesh& M, TArray<FBox>& Col)
{
    FExBuilder::Box(M, B.White, -42, 42, -40, 40, 44, 48, B.PlainUV);
    FExBuilder::Box(M, B.White, -42, 42, 36, 44, 48, 92, B.PlainUV);
    for (float LX : { -38.f, 38.f }) for (float LY : { -36.f, 36.f }) FExBuilder::Box(M, B.Metal, LX - 2, LX + 2, LY - 2, LY + 2, 0, 44, B.PlainUV);
    Col.Add(FBox(FVector(-42, -40, 0), FVector(42, 44, 92)));
}
} // namespace

// ------------------------------------------------------------------ actor
AHouseExteriorDetail::AHouseExteriorDetail()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ExteriorRoot"));
    RootComponent->SetMobility(EComponentMobility::Static);
    bReplicates = false;
}

void AHouseExteriorDetail::Build(UHouseBlackHallSubsystem* Look)
{
    UWorld* World = GetWorld();
    if (!World || !Look) return;
    const double Start = FPlatformTime::Seconds();
    ExCreateLook(Look);
    HouseBrick::CreateLook(Look);
    HouseCars::CreateLook(Look);
    HouseNature::CreateLook(Look);
    CreateHouseExteriorWalkLook(Look);
    FExBuilder B; B.World = World; B.Look = Look; B.Owner = this;
    B.BuildHouse();
    FExSite Site(B);
    Site.BuildAll();
    // ---------------------------------------------------------------- components
    auto Make = [&](const FHallMesh& M, const TCHAR* Name, bool bCollision)
    {
        if (M.NumTriangles() == 0) return;
        UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, Name);
        PM->SetMobility(EComponentMobility::Static);
        PM->bUseComplexAsSimpleCollision = true;
        PM->bUseAsyncCooking = false;
        if (bCollision) { PM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName); PM->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); }
        else PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PM->RegisterComponent();
        M.Build(PM, [Look](FName Key) { return Look->Mat(Key); }, bCollision);
        Meshes.Add(PM);
        Triangles += M.NumTriangles();
    };
    Make(B.House, TEXT("ExteriorHouse"), true);
    Make(B.Grounds, TEXT("ExteriorGrounds"), true);
    Make(B.Near, TEXT("ExteriorNear"), true);
    Make(B.Far, TEXT("ExteriorFar"), false);
    Make(B.CourtDecor, TEXT("ExteriorCourtDecor"), false);
    Make(B.Walks, TEXT("ExteriorWalks"), true);
    Make(B.WalkDecor, TEXT("ExteriorWalkDecor"), false);
    Make(B.BrickDecor, TEXT("ExteriorBrickDecor"), false);
    // A replicated gameplay actor is spawned only by the authority. The static
    // court and its collision are generated identically on every machine.
    if (World->GetNetMode() != NM_Client)
    {
        FActorSpawnParameters BallParams;
        BallParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        BallParams.ObjectFlags |= RF_Transient;
        World->SpawnActor<AHouseBasketball>(AHouseBasketball::StaticClass(),
            FTransform(FRotator::ZeroRotator, HouseCourt::BallHome(), FVector(.248f)), BallParams);
    }
    auto Instance = [&](const TCHAR* Name, TFunctionRef<void(FHallMesh&, TArray<FBox>&)> Builder, const TArray<FTransform>& Xforms, bool bCollision, float CullEnd)
    {
        if (Xforms.Num() == 0) return;
        FHallMesh M; TArray<FBox> Col;
        Builder(M, Col);
        UStaticMesh* SM = HwBuildStaticMesh(M, *FString::Printf(TEXT("SM_Exterior_%s"), Name), Look, Col);
        if (!SM) return;
        KeepAlive.Add(SM);
        UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("Exterior%s"), Name));
        H->SetStaticMesh(SM);
        H->SetMobility(EComponentMobility::Static);
        if (bCollision) { H->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName); H->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); }
        else H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        const bool bFoliage = FString(Name).StartsWith(TEXT("Tree")) || FString(Name).StartsWith(TEXT("Bush")) || FString(Name).StartsWith(TEXT("Grass")) || FString(Name).StartsWith(TEXT("Leaves"));
        H->SetCullDistances(bFoliage ? (int32)(CullEnd * .72f) : 0, (int32)CullEnd);
        H->SetCastShadow(false);
        H->RegisterComponent();
        for (const FTransform& T : Xforms) H->AddInstance(T, true);
        Instanced.Add(H);
        Instances += Xforms.Num();
        Triangles += M.NumTriangles() * Xforms.Num();
    };
    // Several shapes and fall palettes reuse the existing tree/shrub positions and trunk collision.
    for (int32 Variant = 0; Variant < 3; ++Variant)
    {
        TArray<FTransform> Big, Small, Shrubs;
        for (int32 I = 0; I < Site.TreeA.Num(); ++I) if (I % 3 == Variant) Big.Add(Site.TreeA[I]);
        for (int32 I = 0; I < Site.TreeB.Num(); ++I) if (I % 3 == Variant) Small.Add(Site.TreeB[I]);
        for (int32 I = 0; I < Site.Bush.Num(); ++I) if (I % 3 == Variant) Shrubs.Add(Site.Bush[I]);
        Instance(*FString::Printf(TEXT("TreeBigFall%d"), Variant), [&](FHallMesh& M, TArray<FBox>& C) { HouseNature::Tree(M, true, Variant, C); }, Big, true, 29000.f);
        Instance(*FString::Printf(TEXT("TreeSmallFall%d"), Variant), [&](FHallMesh& M, TArray<FBox>& C) { HouseNature::Tree(M, false, Variant, C); }, Small, true, 26000.f);
        Instance(*FString::Printf(TEXT("BushDetailed%d"), Variant), [&](FHallMesh& M, TArray<FBox>& C) { HouseNature::Shrub(M, Variant, C); }, Shrubs, true, 12000.f);
    }
    TArray<FTransform> GrassGreen, GrassDry, Leaves;
    HouseNature::Scatter(GrassGreen, GrassDry, Leaves, Site.TreeA, Site.TreeB, Site.Bush);
    for (int32 Variant = 0; Variant < 3; ++Variant)
    {
        TArray<FTransform> Green, Dry, Litter;
        for (int32 I = 0; I < GrassGreen.Num(); ++I) if (I % 3 == Variant) Green.Add(GrassGreen[I]);
        for (int32 I = 0; I < GrassDry.Num(); ++I) if (I % 3 == Variant) Dry.Add(GrassDry[I]);
        for (int32 I = 0; I < Leaves.Num(); ++I) if (I % 3 == Variant) Litter.Add(Leaves[I]);
        Instance(*FString::Printf(TEXT("GrassGreen%d"), Variant), [&](FHallMesh& M, TArray<FBox>& C) { HouseNature::GrassClump(M, false, Variant); }, Green, false, 4400.f);
        Instance(*FString::Printf(TEXT("GrassDry%d"), Variant), [&](FHallMesh& M, TArray<FBox>& C) { HouseNature::GrassClump(M, true, Variant); }, Dry, false, 4400.f);
        Instance(*FString::Printf(TEXT("LeavesFallen%d"), Variant), [&](FHallMesh& M, TArray<FBox>& C) { HouseNature::LeafLitter(M, Variant); }, Litter, false, 2600.f);
    }
    if (World->GetNetMode() != NM_DedicatedServer)
    {
        FActorSpawnParameters WindParams;
        WindParams.ObjectFlags |= RF_Transient;
        WindParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (AHouseLeafWind* Wind = World->SpawnActor<AHouseLeafWind>(AHouseLeafWind::StaticClass(), FTransform::Identity, WindParams))
            Wind->Initialize(Look, Leaves);
    }
    Instance(TEXT("Pole"), [&](FHallMesh& M, TArray<FBox>& C) { ExPoleMesh(B, M, C); }, Site.Poles, true, 25000.f);
    Instance(TEXT("CarRed"), [&](FHallMesh& M, TArray<FBox>& C) { ExCarMesh(B, M, B.Red, C); }, Site.CarsA, true, 20000.f);
    Instance(TEXT("CarWhite"), [&](FHallMesh& M, TArray<FBox>& C) { ExCarMesh(B, M, B.White, C); }, Site.CarsB, true, 20000.f);
    Instance(TEXT("CarGrey"), [&](FHallMesh& M, TArray<FBox>& C) { ExCarMesh(B, M, B.Grey, C); }, Site.CarsC, true, 20000.f);
    Instance(TEXT("Chair"), [&](FHallMesh& M, TArray<FBox>& C) { ExChairMesh(B, M, C); }, Site.Chairs, true, 8000.f);
    // ---------------------------------------------------------------- lights
    for (const FExBuilder::FLightSpec& L : B.LightSpecs)
    {
        UPointLightComponent* P = NewObject<UPointLightComponent>(this);
        P->SetupAttachment(RootComponent);
        P->SetMobility(EComponentMobility::Movable);
        P->SetIntensityUnits(ELightUnits::Lumens);
        P->SetIntensity(L.Lumens);
        P->SetLightColor(L.Color);
        P->SetAttenuationRadius(L.Radius);
        P->SetSourceRadius(6.f);
        P->SetCastShadows(false);
        P->MaxDrawDistance = L.Draw;
        P->MaxDistanceFadeRange = L.Draw * .2f;
        P->RegisterComponent();
        P->SetWorldLocation(L.Pos);
        PointLights.Add(P);
    }
    Lights = PointLights.Num();
    // ---------------------------------------------------------------- invisible blockers: railing remnants and the playable edge
    for (const FBox& Bx : B.BlockBoxes)
    {
        UBoxComponent* C = NewObject<UBoxComponent>(this);
        C->SetupAttachment(RootComponent);
        C->SetBoxExtent(Bx.GetExtent());
        C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        C->RegisterComponent();
        C->SetWorldLocation(Bx.GetCenter());
        Blockers.Add(C);
    }
    const float Edge = 19200.f;
    for (int32 K = 0; K < 4; ++K)
    {
        UBoxComponent* C = NewObject<UBoxComponent>(this);
        C->SetupAttachment(RootComponent);
        const bool bX = K < 2; const float Sgn = (K & 1) ? 1.f : -1.f;
        C->SetBoxExtent(bX ? FVector(100, Edge + 400, 3000) : FVector(Edge + 400, 100, 3000));
        C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        C->RegisterComponent();
        C->SetWorldLocation(bX ? FVector(Sgn * Edge, 0, 1500) : FVector(0, Sgn * Edge, 1500));
        Blockers.Add(C);
    }
    // ---------------------------------------------------------------- readable night exterior: reuse the moon and ambient sky, with no extra local lights
    for (TActorIterator<ADirectionalLight> It(World); It; ++It)
    {
        if (UDirectionalLightComponent* D = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
        {
            Notes += FString::Printf(TEXT(" moon(%.2f->24.0)"), D->Intensity);
            // The map uses a fixed indoor exposure. Two lux left the open yard nearly black.
            D->SetIntensity(24.f);
            D->SetLightColor(FLinearColor(.78f, .86f, 1.f));
            D->DynamicShadowDistanceMovableLight = 5000.f;
            D->DynamicShadowCascades = 1;
            D->MarkRenderStateDirty();
        }
        It->SetActorRotation(FRotator(-38.f, 115.f, 0.f));
    }
    for (TActorIterator<ASkyLight> It(World); It; ++It)
    {
        if (USkyLightComponent* Sky = It->GetLightComponent())
        {
            // Lift the shaded grass and side paths using the existing captured sky.
            // The interior exposure and all room/club fixtures remain unchanged.
            const float OldIntensity = Sky->Intensity;
            Sky->SetIntensity(FMath::Max(.8f, OldIntensity * 2.f));
            Notes += FString::Printf(TEXT(" sky(%.2f->%.2f)"), OldIntensity, Sky->Intensity);
        }
    }
    for (TActorIterator<APostProcessVolume> It(World); It; ++It)
    {
        const FPostProcessSettings& PS = It->Settings;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR ppv=%s unbound=%d prio=%.1f expMethod=%d(%d) min=%.2f(%d) max=%.2f(%d) bias=%.2f(%d)"), *It->GetName(), It->bUnbound ? 1 : 0, It->Priority,
            (int32)PS.AutoExposureMethod, PS.bOverride_AutoExposureMethod ? 1 : 0, PS.AutoExposureMinBrightness, PS.bOverride_AutoExposureMinBrightness ? 1 : 0,
            PS.AutoExposureMaxBrightness, PS.bOverride_AutoExposureMaxBrightness ? 1 : 0, PS.AutoExposureBias, PS.bOverride_AutoExposureBias ? 1 : 0);
    }
    // ---------------------------------------------------------------- distance fade so the world ends in haze rather than a void
    if (!FParse::Param(FCommandLine::Get(), TEXT("NoHouseFog")))
    {
        FActorSpawnParameters SP; SP.ObjectFlags |= RF_Transient; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector(0, 0, GR), FRotator::ZeroRotator, SP))
        {
            if (UExponentialHeightFogComponent* FC = Fog->GetComponent())
            {
                FC->FogDensity = .016f;
                FC->FogHeightFalloff = .08f;
                FC->FogInscatteringLuminance = FLinearColor(.02f, .026f, .05f);
                FC->StartDistance = 2600.f;
                FC->FogMaxOpacity = .97f;
                FC->FogCutoffDistance = 0.f;
                FC->MarkRenderStateDirty();
            }
            Notes += TEXT(" fog");
        }
    }
    UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR built tris=%d instances=%d lights=%d house=%d grounds=%d near=%d far=%d ms=%.0f%s"), Triangles, Instances, Lights,
        B.House.NumTriangles(), B.Grounds.NumTriangles(), B.Near.NumTriangles(), B.Far.NumTriangles(), (FPlatformTime::Seconds() - Start) * 1000.0, *Notes);
}

// ------------------------------------------------------------------ -HouseExteriorViews: fixed views, screenshots and frame-time probes
namespace
{
struct FExViewState
{
    TWeakObjectPtr<UWorld> World; TWeakObjectPtr<ACameraActor> Cam;
    int32 View = -1, Frames = 0; double Started = 0, StepStarted = 0; bool bShot = false;
    double SumMs = 0, MaxMs = 0, SumGame = 0, SumRender = 0;
    FString Dir, Report;
};
}

namespace
{
/** Walks a route with downward traces (floor continuity) and knee/chest traces (blockers) and logs one line per route. */
void ExProbeRoute(UWorld* World, AActor* Ignore, const TCHAR* Name, const TArray<FVector>& Waypoints)
{
    FCollisionQueryParams QP(SCENE_QUERY_STAT(ExteriorProbe), true); QP.AddIgnoredActor(Ignore);
    float MaxStep = 0.f, PrevFloor = Waypoints[0].Z; int32 Gaps = 0, Blocks = 0, Samples = 0; FString Worst, StepAt;
    FVector Prev = Waypoints[0];
    for (int32 W = 0; W + 1 < Waypoints.Num(); ++W)
    {
        const FVector A = Waypoints[W], Bw = Waypoints[W + 1];
        const int32 N = FMath::Max(1, FMath::CeilToInt((Bw - A).Size2D() / 50.f));
        for (int32 K = (W == 0 ? 0 : 1); K <= N; ++K)
        {
            const FVector P = FMath::Lerp(A, Bw, (float)K / N);
            FHitResult Hit;
            const FVector Top(P.X, P.Y, PrevFloor + 140.f);
            const bool bFloor = World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 700), ECC_Visibility, QP);
            ++Samples;
            if (!bFloor) { ++Gaps; if (Worst.IsEmpty()) Worst = FString::Printf(TEXT("gap@%.0f,%.0f"), P.X, P.Y); continue; }
            const float Step = FMath::Abs(Hit.ImpactPoint.Z - PrevFloor);
            if (Samples > 1 && Step > MaxStep) { MaxStep = Step; if (Step > 45.f) StepAt = FString::Printf(TEXT("step%.0f@%.0f,%.0f(%s)"), Step, P.X, P.Y, *GetNameSafe(Hit.GetComponent())); }
            const FVector Here(P.X, P.Y, Hit.ImpactPoint.Z);
            if (Samples > 1)
                for (float H : { 70.f, 150.f })
                {
                    FHitResult Blk;
                    if (World->LineTraceSingleByChannel(Blk, Prev + FVector(0, 0, H), Here + FVector(0, 0, H), ECC_Visibility, QP))
                    { ++Blocks; if (Worst.IsEmpty()) Worst = FString::Printf(TEXT("block@%.0f,%.0f(%s)"), P.X, P.Y, *GetNameSafe(Blk.GetComponent())); }
                }
            PrevFloor = Hit.ImpactPoint.Z; Prev = Here;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR_PATH %s samples=%d gaps=%d blocks=%d maxstep=%.0f %s %s"), Name, Samples, Gaps, Blocks, MaxStep, *Worst, *StepAt);
}
/** Sweeps the pawn's own capsule across the deck and the patio and logs where it stops and what stopped it. */
void ExProbeSweeps(UWorld* World, ACharacter* Pawn)
{
    UCapsuleComponent* Cap = Pawn->GetCapsuleComponent();
    if (!Cap) return;
    const float Rad = Cap->GetScaledCapsuleRadius(), Half = Cap->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams QP(SCENE_QUERY_STAT(ExteriorSweep), false); QP.AddIgnoredActor(Pawn);
    FCollisionResponseParams RP; Cap->InitSweepCollisionParams(QP, RP);
    const ECollisionChannel Ch = Cap->GetCollisionObjectType();
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Rad - 1.f, Half - 1.f);
    auto Run = [&](const TCHAR* Name, float Floor, const FVector& Dir, float From, float To, float Across0, float Across1, float Step)
    {
        TMap<FString, int32> Who; FString Line; int32 Clear = 0, N = 0;
        for (float C = Across0; C <= Across1 + .1f; C += Step)
        {
            const bool bY = FMath::Abs(Dir.Y) > .5f;
            const FVector A = bY ? FVector(C, From, Floor + Half + 4.f) : FVector(From, C, Floor + Half + 4.f);
            const FVector Bv = bY ? FVector(C, To, Floor + Half + 4.f) : FVector(To, C, Floor + Half + 4.f);
            FHitResult Hit; ++N;
            if (World->SweepSingleByChannel(Hit, A, Bv, FQuat::Identity, Ch, Shape, QP, RP))
            {
                const FString Key = FString::Printf(TEXT("%s/%s%s"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.bStartPenetrating ? TEXT("(start)") : TEXT(""));
                Who.FindOrAdd(Key)++;
                Line += FString::Printf(TEXT(" %.0f:%.0f"), C, bY ? Hit.Location.Y : Hit.Location.X);
            }
            else { ++Clear; Line += FString::Printf(TEXT(" %.0f:-"), C); }
        }
        FString W; for (const auto& P : Who) W += FString::Printf(TEXT(" [%s x%d]"), *P.Key, P.Value);
        UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR_SWEEP %s radius=%.0f half=%.0f rays=%d clear=%d stops:%s blockers:%s"), Name, Rad, Half, N, Clear, *Line, *W);
    };
    Run(TEXT("deck_to_railing"), 0.f, FVector(0, 1, 0), 1000.f, 1800.f, -1360.f, 1360.f, 40.f);
    Run(TEXT("deck_along_mid"), 0.f, FVector(1, 0, 0), -1380.f, 1380.f, 1400.f, 1560.f, 40.f);
    Run(TEXT("patio_to_court"), -362.f, FVector(0, 1, 0), 1000.f, 2400.f, -1360.f, 1360.f, 40.f);
    Run(TEXT("patio_along"), -362.f, FVector(1, 0, 0), -1360.f, 1360.f, 1000.f, 1500.f, 50.f);
}
void ExProbeAll(UWorld* World, AActor* Ignore)
{
    ExProbeRoute(World, Ignore, TEXT("front_door_to_street"), { FVector(-36, -700, 0), FVector(-36, -1100, 0), FVector(-36, -1700, 0), FVector(-36, -2800, 0), FVector(-36, StreetY(-3650), 0), FVector(-36, StreetY(-4500), 0) });
    ExProbeRoute(World, Ignore, TEXT("patio_door_to_court"), { FVector(0, 950, -362), FVector(0, 1200, -362), FVector(-380, 1500, -362), FVector(-380, 1800, -362), FVector(-380, 3000, -362) });
    ExProbeRoute(World, Ignore, TEXT("side_door_and_drive"), { FVector(3200, -1060, 0), FVector(3600, -1060, 0), FVector(3750, -2000, 0), FVector(3750, -2800, 0), FVector(3750, -1000, 0), FVector(3750, 1000, 0), FVector(3750, 2500, 0), FVector(5000, 4000, 0) });
    ExProbeRoute(World, Ignore, TEXT("perimeter"), { FVector(0, -1700, 0), FVector(-1300, -1700, 0), FVector(-1300, -2400, 0), FVector(-3600, -2400, 0), FVector(-3600, 1700, 0), FVector(-1700, 1700, 0),
        FVector(-1700, 1900, 0), FVector(1700, 1900, 0), FVector(1700, 1700, 0), FVector(3600, 1700, 0), FVector(3600, -2300, 0), FVector(1250, -2300, 0), FVector(1250, -1700, 0), FVector(0, -1700, 0) });
    ExProbeRoute(World, Ignore, TEXT("to_the_edge"), { FVector(-200, -2500, 0), FVector(-8000, -2500, 0), FVector(-17000, -2500, 0) });
}
}

void TickHouseExteriorViews(AStoryFirstPersonCharacter* Pawn)
{
    if (!Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled()) return;
    UWorld* World = Pawn->GetWorld();
    if (!World || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
    if (!PC) return;
    static FExViewState S;
    const double Now = World->GetTimeSeconds();
    if (S.World.Get() != World)
    {
        S = FExViewState(); S.World = World; S.Started = S.StepStarted = Now;
        S.Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HouseExteriorViews")));
        IFileManager::Get().MakeDirectory(*S.Dir, true);
        S.Report = TEXT("view,avg_ms,max_ms,game_ms,render_ms\n");
        return;
    }
    if (Now - S.Started < 6.0) return;                       // let shaders and the exterior settle
    if (!S.Cam.IsValid())
    {
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ACameraActor* C = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, P);
        if (!C) return;
        if (Pawn->FirstPersonCamera) C->GetCameraComponent()->PostProcessSettings = Pawn->FirstPersonCamera->PostProcessSettings;
        C->GetCameraComponent()->SetFieldOfView(85.f);
        C->GetCameraComponent()->bConstrainAspectRatio = false;
        S.Cam = C;
        Pawn->SetActorHiddenInGame(true);
        if (UCharacterMovementComponent* Move = Pawn->GetCharacterMovement()) Move->SetMovementMode(MOVE_None);
        // The pawn (and its gown) must not sit in front of the camera: park it high above the map like the stair survey does.
        Pawn->SetActorLocation(FVector(0, 0, 12000), false, nullptr, ETeleportType::TeleportPhysics);
        S.View = 0; S.StepStarted = Now; S.Frames = 0; S.SumMs = S.MaxMs = S.SumGame = S.SumRender = 0; S.bShot = false;
        ExProbeAll(World, Pawn);
        ExProbeSweeps(World, Pawn);
    }
    {
        FString Filter;
        if (FParse::Value(FCommandLine::Get(), TEXT("ExteriorViewFilter="), Filter, false) && !Filter.IsEmpty())
        {
            TArray<FString> Keys; Filter.ParseIntoArray(Keys, TEXT(","));
            auto Wanted = [&](const TCHAR* Name) { for (const FString& K : Keys) if (FString(Name).Contains(K)) return true; return false; };
            while (S.View < (int32)UE_ARRAY_COUNT(ExViews) && !Wanted(ExViews[S.View].Name)) ++S.View;
        }
    }
    if (S.View >= (int32)UE_ARRAY_COUNT(ExViews))
    {
        if (FScreenshotRequest::IsScreenshotRequested()) return;
        FFileHelper::SaveStringToFile(S.Report, *FPaths::Combine(S.Dir, TEXT("frametimes.csv")));
        UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR_VIEWS done\n%s"), *S.Report);
        S.View = 1000;
        if (Now - S.StepStarted > 2.0) FPlatformMisc::RequestExit(false);
        return;
    }
    const FExView& V = ExViews[S.View];
    ACameraActor* C = S.Cam.Get();
    C->SetActorLocationAndRotation(V.Pos, (V.Target - V.Pos).Rotation());
    if (PC->GetViewTarget() != C) PC->SetViewTarget(C);
    const double Age = Now - S.StepStarted;
    if (Age < 2.0) return;
    if (S.Frames < 45)
    {
        const double Ms = FApp::GetDeltaTime() * 1000.0;
        ++S.Frames; S.SumMs += Ms; S.MaxMs = FMath::Max(S.MaxMs, Ms);
        S.SumGame += FPlatformTime::ToMilliseconds(GGameThreadTime); S.SumRender += FPlatformTime::ToMilliseconds(GRenderThreadTime);
        return;
    }
    if (!S.bShot)
    {
        {
            FHitResult Hit; FCollisionQueryParams QP(SCENE_QUERY_STAT(ExteriorViews), true); QP.AddIgnoredActor(Pawn);
            const bool bHit = World->LineTraceSingleByChannel(Hit, V.Pos, V.Pos + (V.Target - V.Pos).GetSafeNormal() * 20000.f, ECC_Visibility, QP);
            TArray<FOverlapResult> Ov; FCollisionShape Sph = FCollisionShape::MakeSphere(30.f);
            World->OverlapMultiByChannel(Ov, V.Pos, FQuat::Identity, ECC_Visibility, Sph, QP);
            FString Inside;
            for (const FOverlapResult& O : Ov) Inside += FString::Printf(TEXT(" %s/%s"), *GetNameSafe(O.GetActor()), *GetNameSafe(O.GetComponent()));
            UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR_VIEW %s hit=%s/%s dist=%.0f inside=%s"), V.Name, bHit ? *GetNameSafe(Hit.GetActor()) : TEXT("-"),
                bHit ? *GetNameSafe(Hit.GetComponent()) : TEXT("-"), bHit ? Hit.Distance : -1.f, *Inside);
        }
        const double N = S.Frames;
        S.Report += FString::Printf(TEXT("%s,%.2f,%.2f,%.2f,%.2f\n"), V.Name, S.SumMs / N, S.MaxMs, S.SumGame / N, S.SumRender / N);
        UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR_FPS view=%s avg_ms=%.2f max_ms=%.2f game_ms=%.2f render_ms=%.2f fps=%.1f"), V.Name, S.SumMs / N, S.MaxMs, S.SumGame / N, S.SumRender / N, 1000.0 / FMath::Max(.01, S.SumMs / N));
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir, FString::Printf(TEXT("%02d_%s.png"), S.View, V.Name)), false, false);
        S.bShot = true;
        return;
    }
    if (FScreenshotRequest::IsScreenshotRequested()) return;
    ++S.View; S.StepStarted = Now; S.Frames = 0; S.SumMs = S.MaxMs = S.SumGame = S.SumRender = 0; S.bShot = false;
}
