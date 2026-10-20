// Calder State's walk-in buildings (see HouseCampusInteriors.h). Phase 2a: the shell and door system, on Mercer Library.
#include "HouseCampusInteriors.h"
#include "HouseFraternityRow.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseCampusInteriorsRooms.h"
#include "HouseCampusLayout.h"
#include "HouseCampusFallKit.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseCampusStaticMesh.h"
#include "HouseBlackHallPaint.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "HouseClockTower.h"
#include "HouseCampusFall.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "AudioMixerBlueprintLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace HouseCampusInteriorsImpl
{
constexpr float LoadRange = 3500.f, FreeRange = 6000.f, FreeAfter = 10.0;

const HouseCampusLayout::FBlock* Block(int32 Seed) { return HouseCampusLayout::Blocks().IsValidIndex(Seed) ? &HouseCampusLayout::Blocks()[Seed] : nullptr; }

FBox2D Rect(float X0, float X1, float Y0, float Y1) { return FBox2D(FVector2D(X0, Y0), FVector2D(X1, Y1)); }

/** A box in the canonical frame for side S's wall: U along the wall, depth D0..D1 in from the outer face, Z. */
FBox WallBox(const FBox2D& B, int32 S, float U0, float U1, float D0, float D1, float Z0, float Z1)
{
    switch (S)
    {
    case 0: return FBox(FVector(U0, B.Max.Y - D1, Z0), FVector(U1, B.Max.Y - D0, Z1));
    case 1: return FBox(FVector(U0, B.Min.Y + D0, Z0), FVector(U1, B.Min.Y + D1, Z1));
    case 2: return FBox(FVector(B.Min.X + D0, U0, Z0), FVector(B.Min.X + D1, U1, Z1));
    default: return FBox(FVector(B.Max.X - D1, U0, Z0), FVector(B.Max.X - D0, U1, Z1));
    }
}

/** The rectangle [U0,U1]x[Z0,Z1] minus Holes, as boxes: split into Z bands at every hole edge, and fill each band's U gaps. */
void Subtract(float U0, float U1, float Z0, float Z1, const TArray<FBox2D>& Holes, TFunctionRef<void(float, float, float, float)> Emit)
{
    TArray<float> Breaks = { Z0, Z1 };
    for (const FBox2D& H : Holes) { if (H.Min.Y > Z0 && H.Min.Y < Z1) Breaks.Add(H.Min.Y); if (H.Max.Y > Z0 && H.Max.Y < Z1) Breaks.Add(H.Max.Y); }
    Breaks.Sort();
    for (int32 I = 0; I + 1 < Breaks.Num(); ++I)
    {
        const float ZA = Breaks[I], ZB = Breaks[I + 1];
        if (ZB - ZA < .5f) continue;
        TArray<FVector2D> Blocked;
        for (const FBox2D& H : Holes) if (H.Min.Y < ZB - .1f && H.Max.Y > ZA + .1f) Blocked.Add(FVector2D(FMath::Max(H.Min.X, U0), FMath::Min(H.Max.X, U1)));
        Blocked.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
        float U = U0;
        for (const FVector2D& Bk : Blocked)
        {
            if (Bk.X > U + .5f) Emit(U, Bk.X, ZA, ZB);
            U = FMath::Max(U, Bk.Y);
        }
        if (U1 > U + .5f) Emit(U, U1, ZA, ZB);
    }
}
/** The same in plan: Rect minus Holes, as rectangles (slabs with stair wells). */
void SubtractPlan(const FBox2D& R, const TArray<FBox2D>& Holes, TFunctionRef<void(const FBox2D&)> Emit)
{
    TArray<FBox2D> Swapped;
    for (const FBox2D& H : Holes) Swapped.Add(FBox2D(FVector2D(H.Min.Y, H.Min.X), FVector2D(H.Max.Y, H.Max.X)));
    // Bands along X (as "Z"), gaps along Y (as "U").
    Subtract(R.Min.Y, R.Max.Y, R.Min.X, R.Max.X, Swapped, [&Emit](float Y0, float Y1, float X0, float X1) { Emit(FBox2D(FVector2D(X0, Y0), FVector2D(X1, Y1))); });
}

// ------------------------------------------------------------------ painted surfaces
void PaintText(UHouseBlackHallSubsystem* Look, FName Key, int32 W, int32 H, const FLinearColor& Bg, const FLinearColor& Ink, std::initializer_list<FTextLine> Lines, bool bBorder, const FLinearColor& Emission)
{
    FHallImage Img;
    Img.Init(W, H, Bg);
    if (bBorder) { Img.Rect(FVector2D(4, 4), FVector2D(W - 4, H - 4), Ink); Img.Rect(FVector2D(9, 9), FVector2D(W - 9, H - 9), Bg); }
    const int32 N = static_cast<int32>(Lines.size());
    int32 K = 0;
    for (const FTextLine& L : Lines)
    {
        FHallTextStyle S;
        S.Height = H * L.Height; S.Stroke = S.Height * .13f; S.Spacing = .18f;
        S.WidthScale = FMath::Min(1.f, (W - 40.f) / HallTextWidth(L.Text, S));
        const float Y = H * (N == 1 ? .5f : .3f + .45f * K / FMath::Max(1, N - 1)) + S.Height * .5f;
        HallDrawText(Img, L.Text, FVector2D((W - HallTextWidth(L.Text, S)) * .5f, Y), S, Ink);
        ++K;
    }
    UTexture2D* Tex = HallMakeTexture(Img, true, false, *Key.ToString(), false);
    if (Tex) Look->Textures.Add(Key, Tex);
    HallMakeTextured(Look, Key, Tex, nullptr, .5f, FLinearColor::White, 0.f, 0.f, Emission);
}
void PaintNoise(UHouseBlackHallSubsystem* Look, FName Key, const FLinearColor& Base, float Grain, float Rough, int32 Seed, int32 Chips, const FLinearColor& Chip)
{
    constexpr int32 N = 256;
    FHallImage Img;
    Img.Init(N, N, Base);
    FRandomStream R(Seed);
    for (int32 Y = 0; Y < N; ++Y) for (int32 X = 0; X < N; ++X) Img.At(X, Y) = Base * (1.f + Grain * (R.FRand() - .5f) + .5f * Grain * HallNoise(X * .04f, Y * .04f, Seed));
    for (int32 K = 0; K < Chips; ++K) { const int32 X = R.RandRange(0, N - 2), Y = R.RandRange(0, N - 2); Img.At(X, Y) = Chip; Img.At(X + 1, Y) = Chip * .9f; }
    UTexture2D* Tex = HallMakeTexture(Img, true, false, *Key.ToString(), true);
    if (Tex) Look->Textures.Add(Key, Tex);
    HallMakeTextured(Look, Key, Tex, nullptr, Rough);
}

// ------------------------------------------------------------------ the house's furniture (2b): what each kit piece is
/** A house mesh reused on campus: its asset in the basement library, the name the HUD uses, its mass, whether it blocks as
 *  decor, and FrontYaw: the yaw that turns the mesh's own front onto local +X (so a piece placed with yaw Y faces Y). The
 *  library's pieces are all modelled facing their local +Y. */
const FKitDef KitDefs[] = {
    { TEXT("SM_Lib_SixShelfBookcase"), TEXT("bookcase"), 60.f, true, -90.f },
    { TEXT("SM_Lib_BookShelfContents0"), TEXT("books"), 6.f, false, -90.f },
    { TEXT("SM_Lib_BookShelfContents1"), TEXT("books"), 6.f, false, -90.f },
    { TEXT("SM_Lib_BookShelfContents2"), TEXT("books"), 6.f, false, -90.f },
    { TEXT("SM_Lib_BookShelfContents3"), TEXT("books"), 6.f, false, -90.f },
    { TEXT("SM_Lib_BookShelfContents4"), TEXT("books"), 6.f, false, -90.f },
    { TEXT("SM_Lib_RoundedOakStudyTable"), TEXT("study table"), 40.f, true, -90.f },
    { TEXT("SM_Lib_CurvedWoodDeskChair"), TEXT("wooden chair"), 9.f, true, -90.f },
    { TEXT("SM_Lib_LeatherClubChair"), TEXT("club chair"), 30.f, true, -90.f },
    { TEXT("SM_Lib_LeatherLoveseat"), TEXT("loveseat"), 45.f, true, -90.f },
    { TEXT("SM_Lib_TableLamp"), TEXT("table lamp"), 3.f, true, -90.f },
    { TEXT("SM_Lib_FloorLamp"), TEXT("floor lamp"), 6.f, true, -90.f },
    { TEXT("SM_Lib_StudyPendant"), TEXT("pendant lamp"), 4.f, false, -90.f },
    { TEXT("SM_Lib_LongRug"), TEXT("rug"), 12.f, false, -90.f },
    { TEXT("SM_Lib_StudyRug"), TEXT("rug"), 10.f, false, -90.f },
    { TEXT("SM_Lib_EntryRug"), TEXT("door mat"), 4.f, false, -90.f },
    { TEXT("SM_Lib_FramedHistoricPhoto"), TEXT("framed photo"), 3.f, false, -90.f },
    { TEXT("SM_Lib_FramedGroupPhoto"), TEXT("framed photo"), 3.f, false, -90.f },
    { TEXT("SM_Lib_LighthouseCanvas"), TEXT("painting"), 3.f, false, -90.f },
    { TEXT("SM_Lib_StackedBooks"), TEXT("books"), 3.f, true, -90.f },
    { TEXT("SM_Lib_LoosePaperStack"), TEXT("papers"), 1.f, true, -90.f },
    { TEXT("SM_Lib_RedCoffeeBook"), TEXT("book"), 1.f, true, -90.f },
    { TEXT("SM_Lib_PaperCup"), TEXT("paper cup"), .1f, true, -90.f },
    { TEXT("SM_Lib_GrayBackpack"), TEXT("backpack"), 4.f, true, -90.f },
    { TEXT("SM_Lib_PlantRoundStand"), TEXT("plant"), 8.f, true, -90.f },
    { TEXT("SM_Lib_TallGrassPlant"), TEXT("plant"), 6.f, true, -90.f },
    { TEXT("SM_Lib_SmallPottedPlant"), TEXT("plant"), 2.f, true, -90.f },
    { TEXT("SM_Lib_RoundWoodSideTable"), TEXT("side table"), 8.f, true, -90.f },
    { TEXT("SM_Lib_TrestlePlankWorktable"), TEXT("worktable"), 35.f, true, -90.f },
    { TEXT("SM_Lib_OpenCardboardBox"), TEXT("box"), 2.f, true, -90.f },
    { TEXT("SM_Lib_DistressedCreamConsole"), TEXT("console table"), 25.f, true, -90.f },
    { TEXT("SM_Lib_FoldedBlueWhiteUmbrella"), TEXT("umbrella"), 1.f, true, -90.f },
    { TEXT("SM_Lib_GrayArmchair"), TEXT("armchair"), 25.f, true, -90.f },
    { TEXT("SM_Lib_ShelfFramedPhoto"), TEXT("framed photo"), 1.f, true, -90.f },
    // 2c. Sizes from Tools/claude/survey_campus_house_assets.py: the bedroom and kitchen casework is built at the house's 1.8
    // stretch like the library's; the kitchen chairs, the extinguisher and the small boxes are true size; the rec-room
    // pieces (NoodleWing) sit between, scaled to real dimensions (an 8 ft pool table, a 2.1 m sofa). Fronts face local +Y,
    // the arcade cabinet's -Y.
    { TEXT("SM_BWR_LoftFrame"), TEXT("loft bed"), 65.f, true, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_QuiltedMattress"), TEXT("mattress"), 12.f, true, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_WritingDesk"), TEXT("desk"), 23.f, true, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_Dresser"), TEXT("dresser"), 22.f, true, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_WireRack"), TEXT("wire rack"), 8.f, true, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_StorageBox"), TEXT("storage box"), 3.f, true, -90.f, TEXT("BoywickBedroom"), 1.f },
    { TEXT("SM_BWR_GreyFuton"), TEXT("futon"), 44.f, true, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_GreyPillow"), TEXT("pillow"), 1.f, true, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_PatternedPillow"), TEXT("pillow"), 1.f, true, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_Kitchen_GreenSpindleChair"), TEXT("chair"), 6.5f, true, -90.f, TEXT("ReferenceKitchen"), 1.f },
    { TEXT("SM_Kitchen_BlackBarStool"), TEXT("stool"), 6.f, true, -90.f, TEXT("ReferenceKitchen"), 1.f },
    { TEXT("SM_Kitchen_DiningTable"), TEXT("table"), 48.f, true, -90.f, TEXT("ReferenceKitchen") },
    { TEXT("SM_Kitchen_ServingCounter"), TEXT("serving counter"), 145.f, true, -90.f, TEXT("ReferenceKitchen") },
    { TEXT("SM_Kitchen_TrashCan"), TEXT("trash can"), 3.f, true, -90.f, TEXT("ReferenceKitchen"), .8f },
    { TEXT("SM_NW_BilliardTable"), TEXT("pool table"), 150.f, true, -90.f, TEXT("NoodleWing"), .75f },
    { TEXT("SM_NW_CueRack"), TEXT("cue rack"), 3.f, true, -90.f, TEXT("NoodleWing"), .75f },
    { TEXT("SM_NW_Arcade"), TEXT("arcade cabinet"), 90.f, true, 90.f, TEXT("NoodleWing"), .85f },
    { TEXT("SM_NW_LeatherSofa"), TEXT("sofa"), 50.f, true, -90.f, TEXT("NoodleWing"), .75f },
    { TEXT("SM_NW_TanRecliner"), TEXT("recliner"), 35.f, true, -90.f, TEXT("NoodleWing"), .8f },
    { TEXT("SM_NW_TVConsole"), TEXT("TV stand"), 30.f, true, -90.f, TEXT("NoodleWing"), .75f },
    { TEXT("SM_NW_FireExtinguisher"), TEXT("fire extinguisher"), 5.f, true, -90.f, TEXT("NoodleWing"), 1.f },
    { TEXT("SM_Lib_OctagonalCardTable"), TEXT("card table"), 85.f, true, -90.f },
    { TEXT("SM_Lib_GrayOfficeChair"), TEXT("office chair"), 14.f, true, -90.f, nullptr, .88f },
    { TEXT("SM_Lib_CoffeeChest"), TEXT("coffee chest"), 30.f, true, -90.f },
    { TEXT("SM_NW_SpindleChair"), TEXT("chair"), 7.f, true, -90.f, TEXT("NoodleWing"), 1.f },
    // The dorm rooms' clothes, towels and odds (the bedroom's 1.8 stretch, but shoes and shirts at about true size).
    { TEXT("SM_BWR_ChairJacket"), TEXT("jacket"), 1.f, false, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_LooseShirt0"), TEXT("shirt"), .3f, false, -90.f, TEXT("BoywickBedroom"), .6f },
    { TEXT("SM_BWR_BunchedOrangeTowel"), TEXT("towel"), .6f, false, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_DrapedTowel"), TEXT("towel"), .6f, false, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_Sneaker"), TEXT("sneaker"), .5f, true, -90.f, TEXT("BoywickBedroom"), .8f },
    { TEXT("SM_BWR_StripedRug"), TEXT("rug"), 8.f, false, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_TrashBin"), TEXT("wastebasket"), 2.f, true, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_LaundryBasket"), TEXT("laundry basket"), 3.f, true, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_DeskLamp"), TEXT("desk lamp"), 1.5f, true, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_LowDaybed"), TEXT("bed"), 40.f, true, -90.f, TEXT("BoywickBedroom") },
    { TEXT("SM_BWR_HangingLaundryBag"), TEXT("laundry bag"), 2.f, false, -90.f, TEXT("BoywickBedroom") },
};
static_assert(UE_ARRAY_COUNT(KitDefs) == static_cast<int32>(HouseCampusInteriors::EKit::Count), "one KitDef per EKit");
const FKitDef& KitDef(HouseCampusInteriors::EKit K) { return KitDefs[FMath::Clamp(static_cast<int32>(K), 0, static_cast<int32>(UE_ARRAY_COUNT(KitDefs)) - 1)]; }
const TCHAR* const KitStages[] = { TEXT(""), TEXT("_DamageV2"), TEXT("_HeavyV2"), TEXT("_BrokenV2") };
constexpr int32 KitStageCount = UE_ARRAY_COUNT(KitStages);

// ------------------------------------------------------------------ the interior kit (phase 2a set; 2b grows it)
const FHallUV& UVPlain() { static const FHallUV U = FHallUV::World(100.f); return U; }
const FHallUV& UVFine() { static const FHallUV U = FHallUV::World(60.f); return U; }
void Box(FHallMesh& M, FName K, const FVector& A, const FVector& B, const FHallUV& UV) { M.BoxMinMax(K, A.ComponentMin(B), A.ComponentMax(B), UV); }

/** Oak wainscot along a wall face: panels Height tall standing 1.5 cm proud of the face at Plane (the face of a wall along X
 *  at Y = Plane when bAlongX, else along Y at X = Plane), Out the side the room is on, from U0 to U1 less the Gaps
 *  (doorways); stiles every 60 cm and a cap rail on top. */
void Wainscot(FHallMesh& M, bool bAlongX, float Plane, float Out, float U0, float U1, float Z, const TArray<FVector2D>& Gaps, float Height)
{
    TArray<FBox2D> Holes;
    for (const FVector2D& G : Gaps) Holes.Add(FBox2D(FVector2D(G.X, Z - 1.f), FVector2D(G.Y, Z + Height + 10.f)));
    auto Piece = [&](FName K, float A, float B, float ZA, float ZB, float D0, float D1)
    {
        const float P0 = Plane + Out * D0, P1 = Plane + Out * D1;
        if (bAlongX) Box(M, K, FVector(A, FMath::Min(P0, P1), ZA), FVector(B, FMath::Max(P0, P1), ZB));
        else Box(M, K, FVector(FMath::Min(P0, P1), A, ZA), FVector(FMath::Max(P0, P1), B, ZB));
    };
    Subtract(U0, U1, Z, Z + Height + 6.f, Holes, [&](float A, float B, float ZA, float ZB)
    {
        if (ZA > Z + 1.f || B - A < 2.f) return;   // only full-height stretches (the doorway bands are cut out)
        Piece(TEXT("CI_Oak"), A, B, Z, Z + Height, 0.f, 1.5f);
        Piece(TEXT("CI_OakDark"), A, B, Z + Height, Z + Height + 6.f, 0.f, 3.f);   // cap rail
        Piece(TEXT("CI_OakDark"), A, B, Z, Z + 12.f, 0.f, 2.f);                    // base
        for (float U = A + 30.f; U < B - 10.f; U += 60.f) Piece(TEXT("CI_OakDark"), U - 2.f, U + 2.f, Z + 12.f, Z + Height, 0.f, 2.f);
    });
}
/** A partition along X at Y (or along Y at X when bAlongY), PartT thick, with doorways (U ranges, open up to Z of the head). */
void Partition(FHallMesh& M, FName K, bool bAlongY, float At, float U0, float U1, float Z0, float Z1, std::initializer_list<FVector2D> Doors, float Head)
{
    PartitionDoors(M, K, bAlongY, At, U0, U1, Z0, Z1, TArray<FVector2D>(Doors), Head);
}
void PartitionDoors(FHallMesh& M, FName K, bool bAlongY, float At, float U0, float U1, float Z0, float Z1, const TArray<FVector2D>& Doors, float Head)
{
    TArray<FBox2D> Holes;
    for (const FVector2D& D : Doors) Holes.Add(FBox2D(FVector2D(D.X, Z0 - 1.f), FVector2D(D.Y, Z0 + Head)));
    Subtract(U0, U1, Z0, Z1, Holes, [&](float A, float B, float ZA, float ZB)
    {
        if (bAlongY) Box(M, K, FVector(At - PartT * .5f, A, ZA), FVector(At + PartT * .5f, B, ZB));
        else Box(M, K, FVector(A, At - PartT * .5f, ZA), FVector(B, At + PartT * .5f, ZB));
    });
    // Oak wainscot on both faces, round the doorways.
    for (const float S : { -1.f, 1.f })
    {
        TArray<FVector2D> G;
        for (const FVector2D& D : Doors) G.Add(FVector2D(D.X - 9.f, D.Y + 9.f));
        Wainscot(M, !bAlongY, At + S * PartT * .5f, S, U0, U1, Z0, G);
    }
    // Oak casings round each doorway, both faces.
    for (const FVector2D& D : Doors)
        for (const float S : { -1.f, 1.f })
        {
            const float F = At + S * (PartT * .5f + .8f);
            auto C = [&](float A, float B, float ZA, float ZB)
            {
                if (bAlongY) Box(M, TEXT("CI_OakDark"), FVector(F - .8f, A, ZA), FVector(F + .8f, B, ZB));
                else Box(M, TEXT("CI_OakDark"), FVector(A, F - .8f, ZA), FVector(B, F + .8f, ZB));
            };
            C(D.X - 9.f, D.X, Z0, Z0 + Head + 9.f); C(D.Y, D.Y + 9.f, Z0, Z0 + Head + 9.f); C(D.X - 9.f, D.Y + 9.f, Z0 + Head, Z0 + Head + 9.f);
        }
}
/** A slab under floor Top, with wells; the finish (FloorKey) on top and the ceiling (CeilKey) under it. */
void FloorSlab(FHallMesh& M, const FBox2D& R, float Top, float Thick, const TArray<FBox2D>& Holes, FName FloorKey, FName CeilKey, const FHallUV& FloorUV)
{
    SubtractPlan(R, Holes, [&](const FBox2D& P)
    {
        Box(M, CeilKey, FVector(P.Min, Top - Thick), FVector(P.Max, Top - .3f));
        Box(M, FloorKey, FVector(P.Min, Top - .3f), FVector(P.Max, Top), FloorUV);
    });
}
void BlockWall(FHallMesh& M, FName K, bool bAlongY, float At, float U0, float U1, float Z0, float Z1, const TArray<FVector2D>& Doors,
    float Head, const TArray<FBox2D>& Openings, FName FrameKey, const FHallUV& UV)
{
    // A box between U A..B, heights ZA..ZB, and depths D0..D1 from the wall's centre line.
    auto Piece = [&](FName Key, float A, float B, float ZA, float ZB, float D0, float D1, const FHallUV& PieceUV)
    {
        if (bAlongY) Box(M, Key, FVector(At + FMath::Min(D0, D1), A, ZA), FVector(At + FMath::Max(D0, D1), B, ZB), PieceUV);
        else Box(M, Key, FVector(A, At + FMath::Min(D0, D1), ZA), FVector(B, At + FMath::Max(D0, D1), ZB), PieceUV);
    };
    TArray<FBox2D> Holes(Openings);
    for (const FVector2D& D : Doors) Holes.Add(FBox2D(FVector2D(D.X, Z0 - 1.f), FVector2D(D.Y, Z0 + Head)));
    const float H = PartT * .5f;
    Subtract(U0, U1, Z0, Z1, Holes, [&](float A, float B, float ZA, float ZB) { Piece(K, A, B, ZA, ZB, -H, H, UV); });
    // The cove base, both faces, round the doorways.
    TArray<FBox2D> DoorHoles;
    for (const FVector2D& D : Doors) DoorHoles.Add(FBox2D(FVector2D(D.X - 5.f, Z0 - 1.f), FVector2D(D.Y + 5.f, Z0 + Head)));
    for (const float S : { -1.f, 1.f })
        Subtract(U0, U1, Z0, Z0 + 10.f, DoorHoles, [&](float A, float B, float ZA, float ZB) { if (ZA <= Z0 + 1.f) Piece(TEXT("CI_Rubber"), A, B, Z0, Z0 + 10.f, S * H, S * (H + .6f), UVPlain()); });
    // Hollow-metal frames: a 5 cm face proud of each wall face, and a thin liner on the jambs and head.
    auto Frame = [&](float A, float B, float ZA, float ZB)
    {
        for (const float S : { -1.f, 1.f })
        {
            Piece(FrameKey, A - 5.f, A, ZA, ZB + 5.f, S * H, S * (H + 1.5f), UVPlain());
            Piece(FrameKey, B, B + 5.f, ZA, ZB + 5.f, S * H, S * (H + 1.5f), UVPlain());
            Piece(FrameKey, A - 5.f, B + 5.f, ZB, ZB + 5.f, S * H, S * (H + 1.5f), UVPlain());
        }
        Piece(FrameKey, A, A + .3f, ZA, ZB, -H, H, UVPlain());
        Piece(FrameKey, B - .3f, B, ZA, ZB, -H, H, UVPlain());
        Piece(FrameKey, A, B, ZB - .3f, ZB, -H, H, UVPlain());
    };
    for (const FVector2D& D : Doors) Frame(D.X, D.Y, Z0, Z0 + Head);
    for (const FBox2D& O : Openings)
    {
        Frame(O.Min.X, O.Max.X, O.Min.Y, O.Max.Y);
        Piece(FrameKey, O.Min.X, O.Max.X, O.Min.Y, O.Min.Y + .3f, -H, H, UVPlain());   // and its sill
    }
}

/** A straight flight from (X0..X1, YBottom, ZBottom) rising toward YTop to ZTop; solid treads (the pawn can use any step). */
void Stair(FHallMesh& M, float X0, float X1, float YBottom, float YTop, float ZBottom, float ZTop)
{
    const int32 N = FMath::CeilToInt((ZTop - ZBottom) / 17.5f);
    const float Rise = (ZTop - ZBottom) / N, Run = (YTop - YBottom) / N;
    for (int32 K = 0; K < N; ++K)
    {
        const float YA = YBottom + Run * K, YB = YBottom + Run * (K + 1), Z = ZBottom + Rise * (K + 1);
        Box(M, TEXT("CI_StairTread"), FVector(X0, FMath::Min(YA, YB), ZBottom - 10.f), FVector(X1, FMath::Max(YA, YB), Z));
        // Rubber nosing strip.
        const float YN = YA + Run * .08f;
        Box(M, TEXT("CI_Rubber"), FVector(X0 + 2.f, FMath::Min(YA, YN), Z), FVector(X1 - 2.f, FMath::Max(YA, YN), Z + .25f));
    }
}
/** A solid oak balustrade (a guard the pawn cannot fall past) with a brass top rail. */
void Balustrade(FHallMesh& M, const FVector2D& A, const FVector2D& B, float Z)
{
    const FVector2D Mn = A.ComponentMin(B) - FVector2D(3.f), Mx = A.ComponentMax(B) + FVector2D(3.f);
    Box(M, TEXT("CI_Oak"), FVector(Mn, Z), FVector(Mx, Z + 96.f));
    Box(M, TEXT("CI_Brass"), FVector(Mn - FVector2D(1.f), Z + 96.f), FVector(Mx + FVector2D(1.f), Z + 102.f));
}
void Troffer(FHallMesh& Glow, FHallMesh& Clutter, const FVector& C, bool bAlongX, FName Key)
{
    const FVector H = bAlongX ? FVector(60.f, 30.f, 0.f) : FVector(30.f, 60.f, 0.f);
    Glow.Quad(Key.IsNone() ? FName(TEXT("CI_Troffer")) : Key, C + FVector(-H.X, -H.Y, 0), C + FVector(H.X, -H.Y, 0), C + FVector(H.X, H.Y, 0), C + FVector(-H.X, H.Y, 0), FVector(0, 0, -1), UVPlain());
    Box(Clutter, TEXT("CI_TrofferFrame"), C - FVector(H.X + 3.f, H.Y + 3.f, -.2f), C + FVector(H.X + 3.f, H.Y + 3.f, 2.f));
}
void Table(FHallMesh& M, const FVector& C, float L, float W, float H, bool bAlongX)
{
    const FVector E = bAlongX ? FVector(L * .5f, W * .5f, 0) : FVector(W * .5f, L * .5f, 0);
    Box(M, TEXT("CI_Oak"), C + FVector(-E.X, -E.Y, H - 4.f), C + FVector(E.X, E.Y, H));
    for (const float SX : { -1.f, 1.f }) for (const float SY : { -1.f, 1.f })
        Box(M, TEXT("CI_OakDark"), C + FVector(SX * (E.X - 8.f) - 3.f, SY * (E.Y - 8.f) - 3.f, 0), C + FVector(SX * (E.X - 8.f) + 3.f, SY * (E.Y - 8.f) + 3.f, H - 4.f));
    Box(M, TEXT("CI_OakDark"), C + FVector(-E.X + 8.f, -E.Y + 8.f, H - 12.f), C + FVector(E.X - 8.f, E.Y - 8.f, H - 4.f));   // apron
}
/** A spindle-back oak library chair facing Facing (unit, in XY). */
void Chair(FHallMesh& M, const FVector& C, const FVector& Facing)
{
    const FVector F = Facing.GetSafeNormal2D(), R = FVector(-F.Y, F.X, 0);
    auto P = [&](float A, float B, float Z) { return C + F * A + R * B + FVector(0, 0, Z); };
    const FRotator Rot = F.Rotation();
    M.Box(TEXT("CI_Oak"), FTransform(Rot, P(0, 0, 44.f)), FVector(21.f, 21.f, 2.f), UVPlain());
    for (const float A : { -17.f, 17.f }) for (const float B : { -17.f, 17.f }) M.Box(TEXT("CI_OakDark"), FTransform(Rot, P(A, B, 21.f)), FVector(1.6f, 1.6f, 21.f), UVPlain());
    for (const float B : { -17.f, 17.f }) M.Box(TEXT("CI_OakDark"), FTransform(Rot, P(-19.f, B, 68.f)), FVector(1.6f, 1.6f, 24.f), UVPlain());
    M.Box(TEXT("CI_Oak"), FTransform(Rot, P(-19.f, 0, 88.f)), FVector(2.f, 20.f, 4.f), UVPlain());
    for (const float B : { -9.f, 0.f, 9.f }) M.Box(TEXT("CI_OakDark"), FTransform(Rot, P(-19.f, B, 66.f)), FVector(.9f, .9f, 19.f), UVPlain());
}
/** A study carrel against a wall: desk, side panels, a hutch shelf and a green-shaded banker's lamp. Back is toward -Facing. */
void Carrel(FHallMesh& M, FHallMesh& Glow, const FVector& C, const FVector& Facing)
{
    const FVector F = Facing.GetSafeNormal2D(), R = FVector(-F.Y, F.X, 0);
    const FRotator Rot = F.Rotation();
    auto P = [&](float A, float B, float Z) { return C + F * A + R * B + FVector(0, 0, Z); };
    M.Box(TEXT("CI_Oak"), FTransform(Rot, P(0, 0, 74.f)), FVector(32.f, 50.f, 2.f), UVPlain());
    for (const float B : { -50.f, 50.f }) M.Box(TEXT("CI_OakDark"), FTransform(Rot, P(0, B, 66.f)), FVector(32.f, 1.5f, 66.f), UVPlain());
    M.Box(TEXT("CI_OakDark"), FTransform(Rot, P(-31.f, 0, 100.f)), FVector(1.5f, 50.f, 32.f), UVPlain());
    M.Box(TEXT("CI_Oak"), FTransform(Rot, P(-20.f, 0, 112.f)), FVector(12.f, 48.f, 1.5f), UVPlain());
    // Lamp: brass foot and stem, the green glass shade (lit), a pull chain.
    const FVector Base = P(-14.f, 30.f, 76.f);
    M.Cylinder(TEXT("CI_Brass"), Base, Base + FVector(0, 0, 2.f), 7.f, 12, UVPlain());
    M.Cylinder(TEXT("CI_Brass"), Base, Base + FVector(0, 0, 32.f), 1.f, 8, UVPlain());
    Glow.Box(TEXT("CI_LampShade"), FTransform(Rot, Base + FVector(0, 0, 34.f) + F * 4.f), FVector(6.f, 14.f, 4.f), UVPlain());
    Glow.Box(TEXT("CI_Bulb"), FTransform(Rot, Base + FVector(0, 0, 31.f) + F * 4.f), FVector(4.f, 11.f, .6f), UVPlain());
    Chair(M, P(48.f, 0, 0), -F);
}
/** A card catalog: an oak case of small drawers on a plinth (H tall, W wide, facing Facing). */
void Catalog(FHallMesh& M, const FVector& C, const FVector& Facing, float W, float H)
{
    const FVector F = Facing.GetSafeNormal2D();
    const FRotator Rot = F.Rotation();
    M.Box(TEXT("CI_OakDark"), FTransform(Rot, C + FVector(0, 0, 6.f)), FVector(23.f, W * .5f, 6.f), UVPlain());
    M.Box(TEXT("CI_Oak"), FTransform(Rot, C + FVector(0, 0, 12.f + H * .5f)), FVector(22.f, W * .5f, H * .5f), UVPlain());
    FHallUV DrawerUV; DrawerUV.Mode = 2; DrawerUV.Frame = FTransform(Rot, C + FVector(0, 0, 12.f + H * .5f) + F * 22.2f); DrawerUV.BoxSize = FVector(.2f, W, H);
    M.Box(TEXT("CI_CatalogFront"), FTransform(Rot, C + FVector(0, 0, 12.f + H * .5f) + F * 22.2f), FVector(.2f, W * .5f - 1.f, H * .5f - 1.f), DrawerUV);
    M.Box(TEXT("CI_OakDark"), FTransform(Rot, C + FVector(0, 0, 13.f + H)), FVector(25.f, W * .5f + 2.f, 1.5f), UVPlain());
}
/** Steel library shelving: a double-faced range of L along Along, with a row of book spines on each shelf, both faces. */
void Shelving(FHallMesh& Solid, FHallMesh& Books, const FVector& A, const FVector& Along, float L, float Z, float H)
{
    const FVector U = Along.GetSafeNormal2D(), N = FVector(-U.Y, U.X, 0);
    const FRotator Rot = U.Rotation();
    const FVector C = A + U * (L * .5f) + FVector(0, 0, Z);
    Solid.Box(TEXT("CI_Steel"), FTransform(Rot, C + FVector(0, 0, H * .5f)), FVector(L * .5f, 2.f, H * .5f), UVPlain());   // spine
    // Uprights every ~91 cm, the last at the range's end.
    const int32 Bays = FMath::Max(1, FMath::CeilToInt(L / 91.f));
    for (int32 K = 0; K <= Bays; ++K)
        Solid.Box(TEXT("CI_Steel"), FTransform(Rot, A + U * (L * K / Bays) + FVector(0, 0, Z + H * .5f)), FVector(1.f, 30.f, H * .5f), UVPlain());
    for (int32 K = 0; K < 7; ++K)
    {
        const float SZ = Z + 8.f + K * 30.f;
        Solid.Box(TEXT("CI_Steel"), FTransform(Rot, C + FVector(0, 0, SZ)), FVector(L * .5f, 29.f, .8f), UVPlain());
        if (K == 6) continue;
        for (const float S : { -1.f, 1.f })
        {
            // A strip of the spine atlas, 24 tall, a little in from the shelf edge; UVs pick a different band per shelf.
            const FVector F0 = A + N * (S * 26.f) + FVector(0, 0, SZ + .8f), F1 = F0 + U * L;
            const float V0 = FMath::Fmod(K * .17f + (S > 0 ? .5f : 0.f) + FMath::Abs(A.X + A.Y) * .0001f, .75f);
            Books.QuadUV(TEXT("CI_Books"), F0, F1, F1 + FVector(0, 0, 24.f), F0 + FVector(0, 0, 24.f), N * S,
                FVector2D(0, V0 + .25f), FVector2D(L / 300.f, V0 + .25f), FVector2D(L / 300.f, V0), FVector2D(0, V0), U);
        }
    }
}
/** A hanging sign: a painted panel on two rods from the ceiling, readable both ways. */
void HangingSign(FHallMesh& M, FName Key, const FVector& C, const FVector& Facing, float W, float H, float Ceiling)
{
    const FVector N = Facing.GetSafeNormal2D(), R = FVector::CrossProduct(FVector::UpVector, -N).GetSafeNormal();
    for (const float S : { 1.f, -1.f }) HouseCampusFallKit::Card(M, Key, C + N * (S * 1.2f), R * S, FVector::UpVector, W, H, N * S);
    for (const float S : { -1.f, 1.f }) M.Cylinder(TEXT("CI_Brass"), C + R * (S * W * .4f) + FVector(0, 0, H * .5f), FVector((C + R * (S * W * .4f)).X, (C + R * (S * W * .4f)).Y, Ceiling), .6f, 6, UVPlain());
}
TArray<FTransform>*& ClockSink() { static TArray<FTransform>* Sink = nullptr; return Sink; }
void SetClockSink(TArray<FTransform>* Sink) { ClockSink() = Sink; }
void WallClock(FHallMesh& Clutter, const FVector& C, const FVector& Out, float R)
{
    // A schoolroom clock: a steel rim, the dial; its hands are added by the interiors actor (they turn).
    const FVector N = Out.GetSafeNormal2D(), Right = FVector::CrossProduct(FVector::UpVector, -N).GetSafeNormal();
    Clutter.Cylinder(TEXT("CI_Steel"), C, C + N * 4.f, R + 1.5f, 20, UVPlain());
    // The dial: a disc in 24 wedges, the painted dial's white circle (radius 124 of 128) mapped onto it.
    const FVector Face = C + N * 4.1f;
    for (int32 K = 0; K < 24; ++K)
    {
        const float A0 = K * PI / 12.f, A1 = (K + 1) * PI / 12.f;
        const FVector2D D0(FMath::Cos(A0), FMath::Sin(A0)), D1(FMath::Cos(A1), FMath::Sin(A1));
        const FVector P0 = Face + (Right * D0.X + FVector::UpVector * D0.Y) * R, P1 = Face + (Right * D1.X + FVector::UpVector * D1.Y) * R;
        const FVector2D U0(.5f + D0.X * .484f, .5f - D0.Y * .484f), U1(.5f + D1.X * .484f, .5f - D1.Y * .484f);
        Clutter.QuadUV(TEXT("CI_ClockFace"), Face, P0, P1, Face, N, FVector2D(.5f, .5f), U0, U1, FVector2D(.5f, .5f), Right);
    }
    if (TArray<FTransform>* Sink = ClockSink()) Sink->Add(FTransform(FRotationMatrix::MakeFromXZ(N, FVector::UpVector).Rotator(), C + N * 4.3f, FVector(R)));
}
void WallPlaque(FHallMesh& M, FName Key, const FVector& C, const FVector& Out, float W, float H)
{
    const FVector N = Out.GetSafeNormal2D(), R = FVector::CrossProduct(FVector::UpVector, -N).GetSafeNormal();
    HouseCampusFallKit::Card(M, Key, C + N * .7f, R, FVector::UpVector, W, H, N);
}

// ------------------------------------------------------------------ Mercer Library (canonical frame: main doors on +Y)
// Footprint X -2500..2500 (the east facade's width), Y -1700..1700; inside X -2470..2470, Y -1670..1670.
// Floor 0 (ground, top -55): lobby and circulation down the middle, periodicals and the card catalog hall at the front
// corners, the reference reading room and the carrels at the back corners, the staff workroom (locked) at the back.
// Floor 1 (the stacks, top 359): steel ranges, carrels along the windows, group study, the copy room (locked).
void LibraryLightAnchors(int32 Seed, float Height, TArray<FVector>& Out)
{
    const float Z0 = HouseCampusInteriors::FloorTop(Height, 0) + 300.f, Z1 = HouseCampusInteriors::FloorTop(Height, 1) + 300.f;
    for (const FVector2D& P : { FVector2D(0, 1100), FVector2D(0, 150), FVector2D(-1535, 935), FVector2D(-1535, -735), FVector2D(1535, 935), FVector2D(1535, -735), FVector2D(0, -820) })   // the last lights the corridor between the stairs and, through its glass, the workroom
        Out.Add(FVector(P, Z0));
    for (const FVector2D& P : { FVector2D(-1500, 900), FVector2D(1500, 900), FVector2D(0, 900), FVector2D(-1500, -700), FVector2D(1500, -700), FVector2D(0, -1300) })
        Out.Add(FVector(P, Z1));
}
void LibraryStairWells(TArray<FBox2D>& Out) { Out.Add(Rect(-550.f, -150.f, -1000.f, -300.f)); Out.Add(Rect(150.f, 550.f, -1000.f, -300.f)); }

/** Six-shelf bookcases, each with one of the house's filled shelf sets, against the reading room's inner walls. */
void LibraryBookcases(const FKitter& Kit, float F0)
{
    using EKit = HouseCampusInteriors::EKit;
    const FBox B = Kit.Sub ? Kit.Sub->KitBounds(EKit::Bookcase) : FBox(ForceInit);
    if (!B.IsValid) return;
    // The case's width is its longer side, its depth the shorter one (the front faces along the shorter side).
    const FVector E = B.GetSize();
    const float W = FMath::Max(E.X, E.Y) + 2.f, D = FMath::Min(E.X, E.Y);
    int32 N = 0;
    auto Row = [&](const FVector2D& From, const FVector2D& Along, float Length, float Yaw)
    {
        const FVector2D Out(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
        for (float T = W * .5f; T + W * .5f <= Length; T += W)
        {
            const FVector2D C = From + Along * T + Out * (D * .5f + 3.5f);   // clear of the wainscot's 3 cm cap rail
            Kit.Floor(EKit::Bookcase, C, F0, Yaw, false);
            Kit.FillBookcase(C, F0, Yaw, N++);
        }
    };
    // Each run starts 3.5 cm clear of the wall it butts into and stops clear of the doorway casings (9 cm).
    Row(FVector2D(-2466.5f, 200.f - PartT * .5f), FVector2D(1, 0), 757.f, -90.f);      // north wall, west of its doorway
    Row(FVector2D(-1291.f, 200.f - PartT * .5f), FVector2D(1, 0), 687.f, -90.f);       // north wall, east of it
    Row(FVector2D(-600.f - PartT * .5f, -1666.5f), FVector2D(0, 1), 656.f, 180.f);     // the workroom's side wall
    Row(FVector2D(-600.f - PartT * .5f, -999.f), FVector2D(0, 1), 731.f, 180.f);       // the wall to the circulation area, up to its doorway
}

constexpr float LibEastWall = 740.f;

void BuildLibrary(int32 Seed, float Height, FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit)
{
    using EKit = HouseCampusInteriors::EKit;
    const float F0 = HouseCampusInteriors::FloorTop(Height, 0), F1 = HouseCampusInteriors::FloorTop(Height, 1), Top = HouseCampusInteriors::OpenTop(Seed);
    const float C0 = F1 - 30.f, C1 = Top;   // ceilings over floor 0 and floor 1
    const FBox2D In = Rect(-2470.f, 2470.f, -1670.f, 1670.f);
    // Shell inside: plaster skin, floors and ceilings.
    HouseCampusInteriors::OpenWalls(Solid, TEXT("CI_Plaster"), Seed, OuterSkin, WallT, UVPlain());
    FloorSlab(Solid, In, F0, 70.f, {}, TEXT("CI_Terrazzo"), TEXT("CI_Plaster"), FHallUV::World(220.f));
    TArray<FBox2D> Wells; LibraryStairWells(Wells);
    FloorSlab(Solid, In, F1, 30.f, Wells, TEXT("CI_Carpet"), TEXT("CI_CeilingTile"), FHallUV::World(180.f));
    Box(Solid, TEXT("CI_CeilingTile"), FVector(In.Min, C1 - .5f), FVector(In.Max, C1), FHallUV::World(122.f));
    // Oak wainscot round the outer walls of both floors (under the sills, round the doors), and an oak stool and casing at
    // every open window inside.
    for (int32 K = 0; K < 2; ++K)
    {
        const float Z = K == 0 ? F0 : F1;
        // Round the doors on the ground floor; floor 1 has none in these walls.
        Wainscot(Solid, true, In.Max.Y, -1.f, In.Min.X, In.Max.X, Z, K == 0 ? TArray<FVector2D>{ FVector2D(-129.f, 129.f) } : TArray<FVector2D>{}, 60.f);
        Wainscot(Solid, true, In.Min.Y, 1.f, In.Min.X, In.Max.X, Z, K == 0 ? TArray<FVector2D>{ FVector2D(1.f, 109.f) } : TArray<FVector2D>{}, 60.f);
        Wainscot(Solid, false, In.Min.X, 1.f, In.Min.Y, In.Max.Y, Z, {}, 60.f);
        Wainscot(Solid, false, In.Max.X, -1.f, In.Min.Y, In.Max.Y, Z, {}, 60.f);
        for (int32 Side = 0; Side < 4; ++Side)
        {
            TArray<FVector2D> Cols;
            HouseCampusInteriors::WindowColumns(Seed, Side, Cols);
            const float Sill = HouseCampusInteriors::WindowSill(Height, K), Head = Sill + HouseCampusInteriors::WindowHeight;
            for (const FVector2D& C : Cols)
            {
                if (HouseCampusInteriors::WindowReplaced(Seed, Side, K, C.X, C.Y)) continue;
                // In the frame: the wall's inner face, the inward normal, and a box spanning U0..U1 between depths D0..D1.
                auto Trim = [&](float U0, float U1, float Z0, float Z1, float D0, float D1, FName Key)
                {
                    switch (Side)
                    {
                    case 0: Box(Solid, Key, FVector(U0, In.Max.Y - D1, Z0), FVector(U1, In.Max.Y - D0, Z1)); break;
                    case 1: Box(Solid, Key, FVector(U0, In.Min.Y + D0, Z0), FVector(U1, In.Min.Y + D1, Z1)); break;
                    case 2: Box(Solid, Key, FVector(In.Min.X + D0, U0, Z0), FVector(In.Min.X + D1, U1, Z1)); break;
                    default: Box(Solid, Key, FVector(In.Max.X - D1, U0, Z0), FVector(In.Max.X - D0, U1, Z1)); break;
                    }
                };
                Trim(C.X - 6.f, C.Y + 6.f, Sill - 4.f, Sill, -6.f, 8.f, TEXT("CI_Oak"));               // stool
                Trim(C.X - 9.f, C.X, Sill, Head + 9.f, 0.f, 2.f, TEXT("CI_OakDark"));                    // casings
                Trim(C.Y, C.Y + 9.f, Sill, Head + 9.f, 0.f, 2.f, TEXT("CI_OakDark"));
                Trim(C.X - 9.f, C.Y + 9.f, Head, Head + 9.f, 0.f, 2.f, TEXT("CI_OakDark"));
            }
        }
    }

    // ---- floor 0 partitions (doorways open, the staff workroom's door is a real locked door)
    // The east walls stand at X 740, not 600: the window grid is not symmetric about the axis, and at 600 they would meet
    // the outer walls in the middle of a window (column 540..715).
    Partition(Solid, TEXT("CI_PlasterWall"), false, -1000.f, -600.f, LibEastWall, F0, C0, { FVector2D(-45.f, 45.f) }, 213.f);    // workroom front (its door is 213 tall)
    Partition(Solid, TEXT("CI_PlasterWall"), true, -600.f, -1670.f, -1000.f, F0, C0, {});                                          // workroom sides
    Partition(Solid, TEXT("CI_PlasterWall"), true, LibEastWall, -1670.f, -1000.f, F0, C0, {});
    Partition(Solid, TEXT("CI_PlasterWall"), true, -600.f, -1000.f, 200.f, F0, C0, { FVector2D(-250.f, 50.f) });                 // reading room / circulation
    Partition(Solid, TEXT("CI_PlasterWall"), true, LibEastWall, -1000.f, 200.f, F0, C0, { FVector2D(-250.f, 50.f) });           // carrels / circulation
    Partition(Solid, TEXT("CI_PlasterWall"), false, 200.f, -2470.f, -600.f, F0, C0, { FVector2D(-1700.f, -1300.f) });           // periodicals / reading room
    Partition(Solid, TEXT("CI_PlasterWall"), false, 200.f, LibEastWall, 2470.f, F0, C0, { FVector2D(1300.f, 1700.f) });         // catalog hall / carrels
    Partition(Solid, TEXT("CI_PlasterWall"), true, -600.f, 200.f, 1670.f, F0, C0, { FVector2D(500.f, 1450.f) }, 260.f);         // periodicals, a wide arch to the lobby
    Partition(Solid, TEXT("CI_PlasterWall"), true, LibEastWall, 200.f, 1670.f, F0, C0, { FVector2D(500.f, 1450.f) }, 260.f);   // catalog hall
    // Stairs up to the stacks (two flights between the reading room and the carrels), oak balustrades round the wells above.
    for (const FBox2D& W : Wells)
    {
        Stair(Solid, W.Min.X + 4.f, W.Max.X - 4.f, W.Max.Y, W.Min.Y, F0, F1);
        Box(Solid, TEXT("CI_OakDark"), FVector(W.Min.X - 6.f, W.Min.Y, F0), FVector(W.Min.X + 4.f, W.Max.Y, F0 + 90.f));       // stringer walls
        Box(Solid, TEXT("CI_OakDark"), FVector(W.Max.X - 4.f, W.Min.Y, F0), FVector(W.Max.X + 6.f, W.Max.Y, F0 + 90.f));
        Balustrade(Solid, FVector2D(W.Min.X, W.Max.Y), FVector2D(W.Max.X, W.Max.Y), F1);
        Balustrade(Solid, FVector2D(W.Min.X, W.Min.Y), FVector2D(W.Min.X, W.Max.Y), F1);
        Balustrade(Solid, FVector2D(W.Max.X, W.Min.Y), FVector2D(W.Max.X, W.Max.Y), F1);
    }
    // ---- floor 0 furniture
    // Circulation desk: an oak U-counter with a raised transaction ledge, facing the lobby.
    Box(Solid, TEXT("CI_Oak"), FVector(-460.f, 220.f, F0), FVector(460.f, 290.f, F0 + 104.f));
    Box(Solid, TEXT("CI_Oak"), FVector(-460.f, -60.f, F0), FVector(-390.f, 220.f, F0 + 104.f));
    Box(Solid, TEXT("CI_Oak"), FVector(390.f, -60.f, F0), FVector(460.f, 220.f, F0 + 104.f));
    Box(Solid, TEXT("CI_OakDark"), FVector(-470.f, 270.f, F0 + 104.f), FVector(470.f, 300.f, F0 + 108.f));
    Box(Clutter, TEXT("CI_Paper"), FVector(-60.f, 240.f, F0 + 104.f), FVector(-20.f, 270.f, F0 + 105.f));                         // date-due slips
    Box(Clutter, TEXT("CI_Steel"), FVector(10.f, 245.f, F0 + 104.f), FVector(24.f, 255.f, F0 + 116.f));                           // the date stamp
    Box(Clutter, TEXT("CI_Rubber"), FVector(8.f, 243.f, F0 + 104.f), FVector(26.f, 257.f, F0 + 106.f));
    HangingSign(Clutter, TEXT("CI_SignCirculation"), FVector(0, 180.f, C0 - 60.f), FVector(0, 1, 0), 200.f, 34.f, C0);
    HangingSign(Clutter, TEXT("CI_SignLibrary"), FVector(0, 1300.f, C0 - 70.f), FVector(0, 1, 0), 300.f, 44.f, C0);
    // Card catalog hall: three rows of cabinets, a consulting table with stools.
    for (const float X : { 900.f, 1500.f, 2100.f })
        for (const float Y : { 650.f, 1050.f })
            Catalog(Solid, FVector(X, Y, F0), FVector(0, 1, 0));
    Table(Solid, FVector(1500.f, 1420.f, F0), 280.f, 60.f, 92.f);
    HangingSign(Clutter, TEXT("CI_SignCatalog"), FVector(1500.f, 400.f, C0 - 60.f), FVector(-1, 0, 0), 220.f, 34.f, C0);
    // Periodicals: slanted magazine racks against the outer wall, low tables, a microfiche reader on a cart.
    for (float X = -2300.f; X < -800.f; X += 300.f)
    {
        Box(Solid, TEXT("CI_Oak"), FVector(X, 1600.f, F0), FVector(X + 260.f, 1660.f, F0 + 160.f));
        for (int32 K = 0; K < 3; ++K) Clutter.Quad(TEXT("CI_Magazines"), FVector(X + 8.f, 1598.f - 8.f * K, F0 + 30.f + 45.f * K), FVector(X + 252.f, 1598.f - 8.f * K, F0 + 30.f + 45.f * K),
            FVector(X + 252.f, 1600.f, F0 + 72.f + 45.f * K), FVector(X + 8.f, 1600.f, F0 + 72.f + 45.f * K), FVector(0, -1, .2f).GetSafeNormal(), FHallUV::World(90.f));
    }
    // Two reading groups from the house's own den: a rug, a pair of leather club chairs round a side table with a lamp.
    for (const float X : { -2000.f, -1300.f })
    {
        Kit.Floor(EKit::StudyRug, FVector2D(X, 900.f), F0, 0.f, false);
        Kit.Floor(EKit::SideTable, FVector2D(X, 900.f), F0, 0.f, true);
        Kit.Floor(EKit::TableLamp, FVector2D(X, 900.f), F0 + Kit.Height(EKit::SideTable), -90.f, true);
        Kit.Floor(EKit::ClubChair, FVector2D(X - 120.f, 900.f), F0, 0.f, true);
        Kit.Floor(EKit::ClubChair, FVector2D(X + 120.f, 900.f), F0, 180.f, true);
        Kit.Floor(EKit::FloorLamp, FVector2D(X + (X < -1600.f ? -110.f : 110.f), 1130.f), F0, -90.f, true);
    }
    Kit.Floor(EKit::RedBook, FVector2D(-1985.f, 1180.f), F0, 30.f, true);
    Kit.Floor(EKit::Backpack, FVector2D(-1180.f, 760.f), F0, 200.f, true);
    Box(Solid, TEXT("CI_Steel"), FVector(-2400.f, 300.f, F0), FVector(-2280.f, 420.f, F0 + 80.f));
    Box(Solid, TEXT("CI_Beige"), FVector(-2395.f, 305.f, F0 + 80.f), FVector(-2285.f, 415.f, F0 + 140.f));
    Glow.Quad(TEXT("CI_FicheScreen"), FVector(-2284.9f, 320.f, F0 + 92.f), FVector(-2284.9f, 400.f, F0 + 92.f), FVector(-2284.9f, 400.f, F0 + 132.f), FVector(-2284.9f, 320.f, F0 + 132.f), FVector(1, 0, 0), UVPlain());
    HangingSign(Clutter, TEXT("CI_SignPeriodicals"), FVector(-1500.f, 400.f, C0 - 60.f), FVector(1, 0, 0), 220.f, 34.f, C0);
    // Reference reading room: long oak tables with chairs, reference shelving round the walls, QUIET PLEASE.
    int32 Pick = 0;
    for (const float Y : { -1250.f, -650.f, -50.f })
    {
        Table(Solid, FVector(-1550.f, Y, F0), 320.f, 110.f);
        for (const float X : { -1650.f, -1450.f }) for (const float S : { -1.f, 1.f }) Kit.Floor(EKit::DeskChair, FVector2D(X, Y + S * 80.f), F0, S > 0.f ? -90.f : 90.f, true);
        Kit.Floor(EKit::TableLamp, FVector2D(-1550.f, Y + 18.f), F0 + 76.f, -90.f, true);
        Kit.Floor(Pick % 2 ? EKit::StackedBooks : EKit::PaperStack, FVector2D(-1660.f, Y - 25.f), F0 + 76.f, 15.f + 40.f * Pick, true);
        if (Pick != 1) Kit.Floor(EKit::RedBook, FVector2D(-1430.f, Y + 20.f), F0 + 76.f, -20.f, true);
        Kit.Floor(EKit::Pendant, FVector2D(-1550.f, Y), C0 - Kit.Height(EKit::Pendant), 0.f, false);
        ++Pick;
    }
    Kit.Floor(EKit::PaperCup, FVector2D(-1420.f, -1225.f), F0 + 76.f, 0.f, true);
    // Reference bookcases (the house's six-shelf cases, filled) along the inner walls; the outer wall keeps its windows.
    LibraryBookcases(Kit, F0);
    WallPlaque(Clutter, TEXT("CI_SignQuiet"), FVector(-600.f - PartT * .5f, -100.f, F0 + 262.f), FVector(-1, 0, 0), 70.f, 30.f);   // over the doorway, clear of the cases
    HangingSign(Clutter, TEXT("CI_SignReference"), FVector(-1500.f, 120.f, C0 - 60.f), FVector(0, 1, 0), 220.f, 34.f, C0);
    // Carrels: two rows back to back in the middle of the room and one along the back wall.
    for (float X = 820.f; X < 2300.f; X += 110.f)
    {
        Carrel(Solid, Glow, FVector(X, -1590.f, F0), FVector(0, 1, 0));
        Carrel(Solid, Glow, FVector(X, -780.f, F0), FVector(0, -1, 0));
        Carrel(Solid, Glow, FVector(X, -660.f, F0), FVector(0, 1, 0));
    }
    HangingSign(Clutter, TEXT("CI_SignCarrels"), FVector(1500.f, 120.f, C0 - 60.f), FVector(0, 1, 0), 220.f, 34.f, C0);
    // Staff workroom (seen only through its door's glass): a worktable, a book truck.
    Kit.Floor(EKit::Worktable, FVector2D(0, -1420.f), F0, 90.f, false);
    Kit.Floor(EKit::StackedBooks, FVector2D(-40.f, -1420.f), F0 + Kit.Height(EKit::Worktable), 10.f, false);
    Kit.Floor(EKit::Cardboard, FVector2D(-450.f, -1560.f), F0, 25.f, false);
    Kit.Floor(EKit::Cardboard, FVector2D(420.f, -1580.f), F0, -10.f, false);
    WallPlaque(Clutter, TEXT("CI_SignStaff"), FVector(80.f, -992.f, F0 + 160.f), FVector(0, 1, 0), 40.f, 14.f);
    WallClock(Clutter, FVector(0.f, -992.5f, F0 + 264.f), FVector(0, 1, 0), 18.f);   // over the workroom door, down the corridor between the stairs
    HangingSign(Clutter, TEXT("CI_SignStacksUp"), FVector(0, -330.f, C0 - 50.f), FVector(0, 1, 0), 140.f, 30.f, C0);   // over the corridor between the stairs
    // The lobby: a mat inside the doors, a runner down the middle, palms either side of the doors, photographs of the
    // college on the walls; the circulation desk's lamp, books waiting to be shelved, a cup of coffee.
    Kit.Floor(EKit::EntryRug, FVector2D(0, 1540.f), F0, 0.f, false);
    Kit.Floor(EKit::LongRug, FVector2D(0, 960.f), F0, 90.f, false);   // the runner's long side along Y, doors to desk
    for (const float S : { -1.f, 1.f }) Kit.Floor(EKit::PlantStand, FVector2D(S * 360.f, 1580.f), F0, -90.f, true);
    Kit.Floor(EKit::Umbrella, FVector2D(250.f, 1625.f), F0, 10.f, true);
    Kit.Wall(EKit::HistoricPhoto, FVector(-600.f + PartT * .5f, 1560.f, F0 + 165.f), FVector(1, 0, 0));
    Kit.Wall(EKit::GroupPhoto, FVector(600.f - PartT * .5f, 1560.f, F0 + 165.f), FVector(-1, 0, 0));
    Kit.Wall(EKit::Lighthouse, FVector(-600.f + PartT * .5f, 350.f, F0 + 170.f), FVector(1, 0, 0));
    Kit.Wall(EKit::HistoricPhoto, FVector(600.f - PartT * .5f, 350.f, F0 + 165.f), FVector(-1, 0, 0));
    Kit.Floor(EKit::TableLamp, FVector2D(330.f, 245.f), F0 + 104.f, 90.f, true);
    Kit.Floor(EKit::StackedBooks, FVector2D(-300.f, 245.f), F0 + 104.f, 5.f, true);
    Kit.Floor(EKit::PaperCup, FVector2D(200.f, 240.f), F0 + 104.f, 0.f, true);
    // Carrels: the things students leave at them.
    // (X, the carrel's desk centre Y, which way it faces): a book on the desk, a backpack on the floor by the chair.
    int32 Left = 0;
    for (const FVector& P : { FVector(930.f, -1590.f, 1.f), FVector(1370.f, -1590.f, 1.f), FVector(1810.f, -780.f, -1.f), FVector(1150.f, -660.f, 1.f) })
    {
        Kit.Floor(Left < 2 ? EKit::StackedBooks : EKit::RedBook, FVector2D(P.X - 10.f, P.Y), F0 + 76.f, 40.f * Left, true);
        Kit.Floor(EKit::Backpack, FVector2D(P.X + 48.f, P.Y + P.Z * 66.f), F0, 90.f + 50.f * Left, true);
        ++Left;
    }
    Kit.Floor(EKit::TallGrass, FVector2D(2370.f, 110.f), F0, 0.f, true);
    Kit.Floor(EKit::TallGrass, FVector2D(-2370.f, 1500.f), F0, 0.f, true);
    // Troffers on both floors.
    for (float X = -2200.f; X <= 2200.f; X += 440.f)
        for (float Y = -1450.f; Y <= 1450.f; Y += 360.f)
        {
            bool bOverWell = false;
            for (const FBox2D& W : Wells) bOverWell |= W.ExpandBy(FVector2D(36.f, 66.f)).IsInside(FVector2D(X, Y));
            if (!bOverWell) Troffer(Glow, Clutter, FVector(X, Y, C0 - .6f), false);   // no ceiling over the stair wells
            Troffer(Glow, Clutter, FVector(X, Y, C1 - .5f - .6f), false);              // under the 0.5 cm tiles
        }

    // ---- floor 1: the stacks
    Partition(Solid, TEXT("CI_PlasterWall"), false, -1350.f, -600.f, LibEastWall, F1, C1, { FVector2D(-45.f, 45.f) }, 213.f);     // copy room
    Partition(Solid, TEXT("CI_PlasterWall"), true, -600.f, -1670.f, -1350.f, F1, C1, {});
    Partition(Solid, TEXT("CI_PlasterWall"), true, LibEastWall, -1670.f, -1350.f, F1, C1, {});
    WallPlaque(Clutter, TEXT("CI_SignCopy"), FVector(80.f, -1342.f, F1 + 160.f), FVector(0, 1, 0), 40.f, 14.f);
    WallClock(Clutter, FVector(-220.f, -1342.5f, F1 + 250.f), FVector(0, 1, 0), 16.f);
    // Ranges: rows along X in the front half and at the back corners, 90 cm aisles; a main aisle down the middle.
    for (float Y = 300.f; Y < 1450.f; Y += 150.f)
    {
        Shelving(Solid, Clutter, FVector(-2300.f, Y, 0), FVector(1, 0, 0), 1700.f, F1);
        Shelving(Solid, Clutter, FVector(600.f, Y, 0), FVector(1, 0, 0), 1700.f, F1);
    }
    for (float Y = -1500.f; Y < -1050.f; Y += 150.f)
    {
        Shelving(Solid, Clutter, FVector(-2300.f, Y, 0), FVector(1, 0, 0), 1500.f, F1);
        Shelving(Solid, Clutter, FVector(800.f, Y, 0), FVector(1, 0, 0), 1500.f, F1);
    }
    // Group study tables in the middle band, carrels along the side windows.
    for (const float X : { -1700.f, 1700.f })
    {
        Table(Solid, FVector(X, -400.f, F1), 260.f, 110.f);
        for (const float DX : { -80.f, 80.f }) for (const float S : { -1.f, 1.f }) Kit.Floor(EKit::DeskChair, FVector2D(X + DX, -400.f + S * 80.f), F1, S > 0.f ? -90.f : 90.f, true);
        Kit.Floor(EKit::TableLamp, FVector2D(X, -382.f), F1 + 76.f, -90.f, true);
        Kit.Floor(EKit::StackedBooks, FVector2D(X - 90.f, -420.f), F1 + 76.f, 70.f, true);
        Kit.Floor(EKit::Backpack, FVector2D(X + (X < 0.f ? 190.f : -190.f), -250.f), F1, X < 0.f ? 160.f : 20.f, true);
    }
    for (float Y = -900.f; Y < 100.f; Y += 120.f)
    {
        Carrel(Solid, Glow, FVector(-2400.f, Y, F1), FVector(1, 0, 0));
        Carrel(Solid, Glow, FVector(2400.f, Y, F1), FVector(-1, 0, 0));
    }
    HangingSign(Clutter, TEXT("CI_SignStacks"), FVector(0, 200.f, C1 - 60.f), FVector(0, 1, 0), 260.f, 40.f, C1);
}
void LibraryOpenings(int32 Seed, float Height, TArray<HouseCampusInteriors::FOpening>& Out)
{
    const float F0 = HouseCampusInteriors::FloorTop(Height, 0);
    Out.Add({ 0, -120.f, 120.f, F0, F0 + 213.f });   // the main doors: a pair of glazed oak leaves, 2.4 m across, 213 tall like the leaves
    Out.Add({ 1, 10.f, 100.f, F0, F0 + 213.f });      // the staff door at the back, into the workroom (clear of the pilaster at U -35)
}
void LibraryDoors(int32 Seed, float Height, TArray<FDoorSpec>& Out)
{
    const FBox2D B = HouseCampusInteriors::CanonRect(Seed);
    TArray<HouseCampusInteriors::FOpening> Openings;
    LibraryOpenings(Seed, Height, Openings);
    MainDoorPair(TEXT("LIB"), B, Openings[0], Out);
    BackDoor(TEXT("LIB"), TEXT("StaffEntrance"), B, Openings[1], TEXT("STAFF ENTRANCE - deliveries at the front desk"), Out);
    const float F0 = HouseCampusInteriors::FloorTop(Height, 0), F1 = HouseCampusInteriors::FloorTop(Height, 1), Face = PartT * .5f + 1.f;
    const uint8 Steel = AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::KickPlate;
    Out.Add({ TEXT("Workroom"), FVector(-45.f + .4f, -1000.f - Face, F0), 0.f, EHallDoorStyle::Equipment, 89.2f, -1.f, Steel, FVector(0, -1000.f, F0 + 105.f), NAME_None, TEXT("STAFF ONLY - the library workroom") });
    Out.Add({ TEXT("CopyRoom"), FVector(-45.f + .4f, -1350.f - Face, F1), 0.f, EHallDoorStyle::Equipment, 89.2f, -1.f, Steel, FVector(0, -1350.f, F1 + 105.f), NAME_None, TEXT("COPY ROOM - staff only after 5 PM") });
}
void LibraryLook(UHouseBlackHallSubsystem* Look)
{
    const FLinearColor SignBg(.03f, .03f, .035f), Gold(.75f, .58f, .2f), Cream(.82f, .8f, .72f);
    PaintText(Look, TEXT("CI_SignLibrary"), 768, 112, SignBg, Gold, { { TEXT("MERCER LIBRARY"), .42f } }, true);
    PaintText(Look, TEXT("CI_SignCirculation"), 512, 88, SignBg, Gold, { { TEXT("CIRCULATION"), .45f } }, true);
    PaintText(Look, TEXT("CI_SignCatalog"), 512, 88, SignBg, Gold, { { TEXT("CARD CATALOG"), .45f } }, true);
    PaintText(Look, TEXT("CI_SignPeriodicals"), 512, 88, SignBg, Gold, { { TEXT("PERIODICALS"), .45f } }, true);
    PaintText(Look, TEXT("CI_SignReference"), 512, 88, SignBg, Gold, { { TEXT("REFERENCE"), .45f } }, true);
    PaintText(Look, TEXT("CI_SignCarrels"), 512, 88, SignBg, Gold, { { TEXT("QUIET STUDY"), .45f } }, true);
    PaintText(Look, TEXT("CI_SignStacks"), 512, 88, SignBg, Gold, { { TEXT("STACKS  A - Z"), .45f } }, true);
    PaintText(Look, TEXT("CI_SignStacksUp"), 512, 110, SignBg, Cream, { { TEXT("STACKS"), .3f }, { TEXT("SECOND FLOOR"), .22f } }, true);
    PaintText(Look, TEXT("CI_SignQuiet"), 256, 110, Cream, SignBg, { { TEXT("QUIET"), .3f }, { TEXT("PLEASE"), .3f } }, true);
    PaintText(Look, TEXT("CI_SignStaff"), 256, 90, SignBg, Cream, { { TEXT("STAFF ONLY"), .38f } }, true);
    PaintText(Look, TEXT("CI_SignCopy"), 256, 90, SignBg, Cream, { { TEXT("COPY ROOM"), .38f } }, true);
}
FInteriorDef LibraryDef() { return { TEXT("LIB"), 2, &BuildLibrary, &LibraryLightAnchors, &LibraryOpenings, &LibraryDoors, &LibraryLook }; }

// ------------------------------------------------------------------ the registry
const TArray<FInteriorDef>& Interiors()
{
    // Mercer Library (2a, 2b); the other walk-in buildings join in 2c and 2d.
    static const TArray<FInteriorDef> All = []
    {
        TArray<FInteriorDef> Defs = { LibraryDef(), HarlowDef(), ScienceDef(), UnionDef(), StadiumDef(), TowerDef() };
        GenericDefs(Defs);   // Tier B and C (2d)
        RowDefs(Defs);       // Append only: preserve all existing standalone seeds.
        return Defs;
    }();
    return All;
}
const FInteriorDef* InteriorFor(int32 Seed)
{
    if (Seed >= HouseCampusInteriors::FreeSeedBase)
    {
        const int32 I = Seed - HouseCampusInteriors::FreeSeedBase;
        return Interiors().IsValidIndex(I) && Interiors()[I].Bounds ? &Interiors()[I] : nullptr;
    }
    const HouseCampusLayout::FBlock* B = Block(Seed);
    if (!B) return nullptr;
    for (const FInteriorDef& D : Interiors()) if (!D.Bounds && FCString::Strcmp(D.Id, B->Id) == 0) return &D;
    return nullptr;
}
void MainDoorPair(const TCHAR* Id, const FBox2D& Canon, const HouseCampusInteriors::FOpening& O, TArray<FDoorSpec>& Out)
{
    // A glazed oak pair, swinging in. Like the black hall's patio pairs: the hinge line 1 cm inside the inner face.
    const float YIn = Canon.Max.Y - WallT - 1.f, LeafW = (O.U1 - O.U0 - 1.2f) * .5f;
    const FName Group(*FString::Printf(TEXT("Campus_%s_Main"), Id));
    const FVector Center((O.U0 + O.U1) * .5f, Canon.Max.Y - 15.f, O.Z0 + 105.f);
    Out.Add({ TEXT("MainA"), FVector(O.U0 + .4f, YIn, O.Z0), 0.f, EHallDoorStyle::French, LeafW, -1.f, uint8(AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::Closer), Center, Group, FString() });
    Out.Add({ TEXT("MainB"), FVector(O.U1 - .4f, YIn, O.Z0), 180.f, EHallDoorStyle::French, LeafW, 1.f, uint8(AHouseBlackHallDoor::Lever), Center, Group, FString() });
}
void BackDoor(const TCHAR* Id, const TCHAR* Name, const FBox2D& Canon, const HouseCampusInteriors::FOpening& O, const TCHAR* Reason, TArray<FDoorSpec>& Out)
{
    // One steel leaf in the back wall (the canonical frame turned round, as the main pair's A leaf).
    const float YIn = Canon.Min.Y + WallT + 1.f;
    Out.Add({ Name, FVector(O.U1 - .4f, YIn, O.Z0), 180.f, EHallDoorStyle::Equipment, O.U1 - O.U0 - .8f, -1.f, uint8(AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::KickPlate),
        FVector((O.U0 + O.U1) * .5f, Canon.Min.Y + 15.f, O.Z0 + 105.f), NAME_None, Reason ? FString(Reason) : FString() });
}
void SidePair(const TCHAR* Id, const TCHAR* Prefix, const FBox2D& Canon, const HouseCampusInteriors::FOpening& O, const FString& Reason, TArray<FDoorSpec>& Out)
{
    // The opening's U axis (+X on sides 0 and 1, +Y on 2 and 3) and the way in. Leaf A hangs on the U0 jamb and runs along +U
    // (its yaw), leaf B on the U1 jamb the other way; each one's RoomSign is the way in seen in its own frame (local +Y is +U
    // turned 90 degrees). As MainDoorPair on side 0.
    const FVector2D UDir = O.Side < 2 ? FVector2D(1, 0) : FVector2D(0, 1);
    const FVector2D In = O.Side == 0 ? FVector2D(0, -1) : O.Side == 1 ? FVector2D(0, 1) : O.Side == 2 ? FVector2D(1, 0) : FVector2D(-1, 0);
    const FVector2D Edge = O.Side == 0 ? FVector2D(0, Canon.Max.Y) : O.Side == 1 ? FVector2D(0, Canon.Min.Y) : O.Side == 2 ? FVector2D(Canon.Min.X, 0) : FVector2D(Canon.Max.X, 0);
    auto At = [&](float U, float Depth) { const FVector2D P = Edge + In * Depth + UDir * U; return FVector(P, O.Z0); };
    const float YawA = UDir.X > .5f ? 0.f : 90.f, SignA = FVector2D::DotProduct(In, FVector2D(-UDir.Y, UDir.X)) > 0.f ? 1.f : -1.f;
    const float LeafW = (O.U1 - O.U0 - 1.2f) * .5f;
    const FName Group(*FString::Printf(TEXT("Campus_%s_%s"), Id, Prefix));
    const FVector Center = At((O.U0 + O.U1) * .5f, 15.f) + FVector(0, 0, 105.f);
    Out.Add({ FName(*FString::Printf(TEXT("%sA"), Prefix)), At(O.U0 + .4f, WallT + 1.f), YawA, EHallDoorStyle::French, LeafW, SignA, uint8(AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::Closer), Center, Group, Reason });
    Out.Add({ FName(*FString::Printf(TEXT("%sB"), Prefix)), At(O.U1 - .4f, WallT + 1.f), YawA + 180.f, EHallDoorStyle::French, LeafW, -SignA, uint8(AHouseBlackHallDoor::Lever), Center, Group, Reason });
}
void InnerDoor(TArray<FDoorSpec>& Out, FName Name, bool bAlongY, float At, float U0, float U1, float Z, float RoomDir, EHallDoorStyle Style,
    uint8 Flags, const FString& Reason, bool bOpen)
{
    // The leaf runs from the U0 jamb along +U (yaw 0 along X, 90 along Y); RoomSign is the swing side in the leaf's own frame
    // (its local +Y is world +Y for a wall along X, world -X for one along Y). The hinge line sits 1 cm past the wall face.
    const float Face = PartT * .5f + 1.f, Mid = (U0 + U1) * .5f;
    const float Sign = bAlongY ? -RoomDir : RoomDir;
    const FVector Hinge = bAlongY ? FVector(At + RoomDir * Face, U0 + .4f, Z) : FVector(U0 + .4f, At + RoomDir * Face, Z);
    const FVector Center = bAlongY ? FVector(At, Mid, Z + 105.f) : FVector(Mid, At, Z + 105.f);
    FDoorSpec D{ Name, Hinge, bAlongY ? 90.f : 0.f, Style, U1 - U0 - .8f, Sign, Flags, Center, NAME_None, Reason };
    D.bOpen = bOpen;
    Out.Add(D);
}
}

// ------------------------------------------------------------------ public API
bool HouseCampusInteriors::Enabled()
{
    // The command line is fixed for the process; this is asked many times a frame.
    static const bool bOn = HouseCampusLayout::Enabled() && !FParse::Param(FCommandLine::Get(), TEXT("NoCampusInteriors"));
    return bOn;
}

int32 HouseCampusInteriors::OpenFloors(int32 Seed)
{
    if (!Enabled()) return 0;
    if (Seed >= FreeSeedBase) { const HouseCampusInteriorsImpl::FInteriorDef* D = HouseCampusInteriorsImpl::InteriorFor(Seed); return D ? D->OpenFloors : 0; }
    const HouseCampusLayout::FBlock* B = HouseCampusInteriorsImpl::Block(Seed);
    if (!B) return 0;
    for (const HouseCampusInteriorsImpl::FInteriorDef& D : HouseCampusInteriorsImpl::Interiors()) if (FCString::Strcmp(D.Id, B->Id) == 0) return FMath::Min(D.OpenFloors, Floors(B->Height));
    return 0;
}
int32 HouseCampusInteriors::Floors(float Height) { return FMath::Clamp(FMath::FloorToInt(Height / 420.f), 2, 7); }
float HouseCampusInteriors::Story(float Height) { return (Height - 50.f) / Floors(Height); }
float HouseCampusInteriors::FloorTop(float Height, int32 K) { return HouseCampusInteriorsImpl::Ground + 5.f + K * Story(Height); }
float HouseCampusInteriors::OpenTop(int32 Seed)
{
    if (Seed >= FreeSeedBase) return InteriorBox(Seed).Max.Z;
    const HouseCampusLayout::FBlock* B = HouseCampusInteriorsImpl::Block(Seed);
    if (!B) return 0.f;
    const int32 Open = OpenFloors(Seed);
    return Open >= Floors(B->Height) ? HouseCampusInteriorsImpl::Ground + B->Height : FloorTop(B->Height, Open) - 30.f;
}
FBox2D HouseCampusInteriors::CanonRect(int32 Seed)
{
    const HouseCampusLayout::FBlock* B = HouseCampusInteriorsImpl::Block(Seed);
    if (!B) return FBox2D(ForceInit);
    const TCHAR Main = B->Faces[0];
    const bool bTurned = Main == TEXT('E') || Main == TEXT('W');
    const float W = bTurned ? B->Y1 - B->Y0 : B->X1 - B->X0, D = bTurned ? B->X1 - B->X0 : B->Y1 - B->Y0;
    return FBox2D(FVector2D(-W * .5f, -D * .5f), FVector2D(W * .5f, D * .5f));
}
void HouseCampusInteriors::InteriorSeeds(TArray<int32>& Out)
{
    if (!Enabled()) return;
    for (int32 Seed = 0; Seed < HouseCampusLayout::Blocks().Num(); ++Seed) if (OpenFloors(Seed) > 0) Out.Add(Seed);
    for (int32 I = 0; I < HouseCampusInteriorsImpl::Interiors().Num(); ++I)
        if (HouseCampusInteriorsImpl::Interiors()[I].Bounds && HouseCampusInteriorsImpl::Interiors()[I].OpenFloors > 0) Out.Add(FreeSeedBase + I);
}
int32 HouseCampusInteriors::SeedFor(const TCHAR* Id)
{
    for (int32 I = 0; I < HouseCampusInteriorsImpl::Interiors().Num(); ++I)
        if (HouseCampusInteriorsImpl::Interiors()[I].Bounds && FCString::Strcmp(HouseCampusInteriorsImpl::Interiors()[I].Id, Id) == 0) return FreeSeedBase + I;
    return HouseCampusLayout::Find(Id);
}
const TCHAR* HouseCampusInteriors::InteriorId(int32 Seed)
{
    if (const HouseCampusInteriorsImpl::FInteriorDef* D = HouseCampusInteriorsImpl::InteriorFor(Seed)) return D->Id;
    const HouseCampusLayout::FBlock* B = HouseCampusInteriorsImpl::Block(Seed);
    return B ? B->Id : TEXT("?");
}
FBox HouseCampusInteriors::InteriorBox(int32 Seed)
{
    if (Seed >= FreeSeedBase)
    {
        const HouseCampusInteriorsImpl::FInteriorDef* D = HouseCampusInteriorsImpl::InteriorFor(Seed);
        return D && D->Bounds ? D->Bounds() : FBox(ForceInit);
    }
    const HouseCampusLayout::FBlock* B = HouseCampusInteriorsImpl::Block(Seed);
    if (!B) return FBox(ForceInit);
    const float DZ = HouseCampusLayout::BlockDZ(Seed);
    return FBox(FVector(B->Rect().Min, HouseCampusInteriorsImpl::Ground - 70.f + DZ), FVector(B->Rect().Max, OpenTop(Seed) + DZ));
}
bool HouseCampusInteriors::InsideInterior(int32 Seed, const FVector& P)
{
    const HouseCampusInteriorsImpl::FInteriorDef* D = Seed >= FreeSeedBase ? HouseCampusInteriorsImpl::InteriorFor(Seed) : nullptr;
    if (D && D->Inside) return D->Inside(P);
    return InteriorBox(Seed).IsInsideOrOn(P);
}
float HouseCampusInteriors::InteriorFloorZ(int32 Seed, int32 K)
{
    if (Seed >= FreeSeedBase)
    {
        const HouseCampusInteriorsImpl::FInteriorDef* D = HouseCampusInteriorsImpl::InteriorFor(Seed);
        if (D && D->FloorZ) return D->FloorZ(K);
        // (Not reached today: the stadium and the tower both give their floors.) Else the ground under the interior's middle.
        const FVector Mid = D && D->Bounds ? D->Bounds().GetCenter() : FVector::ZeroVector;
        return D && D->Bounds ? HouseExteriorSite::GroundHeight(Mid.X, Mid.Y) : HouseCampusInteriorsImpl::Ground;
    }
    const HouseCampusLayout::FBlock* B = HouseCampusInteriorsImpl::Block(Seed);
    return B ? FloorTop(B->Height, K) + HouseCampusLayout::BlockDZ(Seed) : 0.f;
}
float HouseCampusInteriors::OpenTopWorld(int32 Seed) { return Seed >= FreeSeedBase ? InteriorBox(Seed).Max.Z : OpenTop(Seed) + HouseCampusLayout::BlockDZ(Seed); }
float HouseCampusInteriors::BaseDZ(int32 Seed)
{
    if (Seed < FreeSeedBase) return HouseCampusLayout::BlockDZ(Seed);
    const HouseCampusInteriorsImpl::FInteriorDef* D = HouseCampusInteriorsImpl::InteriorFor(Seed);
    return D && FCString::Strcmp(D->Id, TEXT("TWR")) == 0 ? HouseCalderTerrain::TowerLift() : 0.f;   // the stadium keeps its Z
}
FTransform HouseCampusInteriors::Frame(int32 Seed)
{
    if (Seed >= FreeSeedBase) return FTransform::Identity;
    return HouseCampusInteriorsImpl::Block(Seed) ? HouseCampusLayout::Frame(Seed) : FTransform::Identity;
}
void HouseCampusInteriors::DoorOpenings(int32 Seed, TArray<FOpening>& Out)
{
    if (OpenFloors(Seed) <= 0) return;
    const HouseCampusLayout::FBlock* B = HouseCampusInteriorsImpl::Block(Seed);
    const HouseCampusInteriorsImpl::FInteriorDef* Def = HouseCampusInteriorsImpl::InteriorFor(Seed);
    if (B && Def && Def->Openings) Def->Openings(Seed, B->Height, Out);
}
bool HouseCampusInteriors::RealDoorOn(int32 Seed, TCHAR Face)
{
    const HouseCampusLayout::FBlock* B = HouseCampusInteriorsImpl::Block(Seed);
    if (!B || OpenFloors(Seed) <= 0) return false;
    const FVector N = FRotator(0.f, -HouseCampusLayout::FaceYaw(B->Faces[0]), 0.f).RotateVector(HouseCampusLayout::FaceNormal(Face));
    const int32 Side = N.Y > .5f ? 0 : N.Y < -.5f ? 1 : N.X < 0.f ? 2 : 3;
    TArray<FOpening> Doors;
    DoorOpenings(Seed, Doors);
    for (const FOpening& D : Doors) if (D.Side == Side) return true;
    return false;
}
void HouseCampusInteriors::WindowColumns(int32 Seed, int32 Side, TArray<FVector2D>& Out)
{
    const FBox2D B = CanonRect(Seed);
    const float U0 = Side < 2 ? B.Min.X : B.Min.Y, Len = (Side < 2 ? B.Max.X - B.Min.X : B.Max.Y - B.Min.Y) - 320.f;
    for (int32 I = 0; I * 360 + 220 < Len; ++I) Out.Add(FVector2D(U0 + 160.f + I * 360.f, U0 + 160.f + I * 360.f + 175.f));
}
float HouseCampusInteriors::WindowSill(float Height, int32 K) { return HouseCampusInteriorsImpl::Ground + 90.f + K * Story(Height); }
bool HouseCampusInteriors::WindowOpen(int32 Seed, int32 Side, int32 K, float U0, float U1)
{
    const HouseCampusInteriorsImpl::FInteriorDef* D = HouseCampusInteriorsImpl::InteriorFor(Seed);
    return !D || !D->WindowOpen || D->WindowOpen(Seed, Side, K, U0, U1);
}
bool HouseCampusInteriors::WindowReplaced(int32 Seed, int32 Side, int32 K, float U0, float U1)
{
    const HouseCampusLayout::FBlock* B = HouseCampusInteriorsImpl::Block(Seed);
    if (!B) return false;
    TArray<FOpening> Doors;
    DoorOpenings(Seed, Doors);
    const float Z0 = WindowSill(B->Height, K), Z1 = Z0 + WindowHeight;
    for (const FOpening& D : Doors) if (D.Side == Side && D.U0 < U1 + 30.f && D.U1 > U0 - 30.f && D.Z0 < Z1 && D.Z1 > Z0) return true;
    return false;
}
void HouseCampusInteriors::OpenWalls(FHallMesh& M, FName Key, int32 Seed, float Depth0, float Depth1, const FHallUV& UV)
{
    const HouseCampusLayout::FBlock* Blk = HouseCampusInteriorsImpl::Block(Seed);
    const int32 Open = OpenFloors(Seed);
    if (!Blk || Open <= 0) return;
    const FBox2D B = CanonRect(Seed);
    const float Z0 = HouseCampusInteriorsImpl::Ground - 70.f, Z1 = OpenTop(Seed);
    TArray<FOpening> Doors;
    DoorOpenings(Seed, Doors);
    for (int32 S = 0; S < 4; ++S)
    {
        TArray<FBox2D> Holes;
        TArray<FVector2D> Cols;
        WindowColumns(Seed, S, Cols);
        for (int32 K = 0; K < Open; ++K)
            for (const FVector2D& C : Cols)
                if (!WindowReplaced(Seed, S, K, C.X, C.Y) && WindowOpen(Seed, S, K, C.X, C.Y)) Holes.Add(FBox2D(FVector2D(C.X, WindowSill(Blk->Height, K)), FVector2D(C.Y, WindowSill(Blk->Height, K) + WindowHeight)));
        for (const FOpening& D : Doors) if (D.Side == S) Holes.Add(FBox2D(FVector2D(D.U0, D.Z0 - 1.f), FVector2D(D.U1, D.Z1)));
        // Sides 0 and 1 run the whole length (they hold the corners); 2 and 3 run between them.
        const float UA = S < 2 ? B.Min.X + Depth0 : B.Min.Y + Depth1, UB = S < 2 ? B.Max.X - Depth0 : B.Max.Y - Depth1;
        HouseCampusInteriorsImpl::Subtract(UA, UB, Z0, Z1, Holes, [&](float U0, float U1, float ZA, float ZB)
        {
            const FBox Bx = HouseCampusInteriorsImpl::WallBox(B, S, U0, U1, Depth0, Depth1, ZA, ZB);
            M.BoxMinMax(Key, Bx.Min, Bx.Max, UV);
        });
    }
}
float HouseCampusInteriors::DistanceOutside(const FVector& P)
{
    float Best = FLT_MAX;
    if (!Enabled()) return Best;
    // The walk-in interiors never change in a process: work out their boxes once (this runs every frame). Their boxes stand on
    // the buildings' pads, which are analytic and fixed for the process (HouseCalderTerrain::PadDZ), so the first call may
    // come at any time.
    struct FIndoor { int32 Seed; FBox Box; bool bShaped; };
    static const TArray<FIndoor> Indoors = []
    {
        TArray<FIndoor> Out;
        TArray<int32> Seeds;
        InteriorSeeds(Seeds);
        for (const int32 Seed : Seeds)
        {
            const HouseCampusInteriorsImpl::FInteriorDef* D = Seed >= FreeSeedBase ? HouseCampusInteriorsImpl::InteriorFor(Seed) : nullptr;
            Out.Add({ Seed, InteriorBox(Seed), D && D->Inside });
        }
        return Out;
    }();
    for (const FIndoor& I : Indoors)
    {
        const float D = FMath::Sqrt(I.Box.ComputeSquaredDistanceToPoint(P));
        // A shaped interior (the stadium's ring of rooms) counts as inside only by its own test; within its box but not in
        // it (on the field) is outside, a little.
        Best = FMath::Min(Best, !I.bShaped ? D : InsideInterior(I.Seed, P) ? 0.f : FMath::Max(D, 300.f));
    }
    return Best;
}

namespace HouseCampusInteriorsImpl
{
UMaterialInterface* RoomWindowMaterial()
{
    // Loaded again whenever it was unloaded with an earlier world (a second visit to the map); given up on only if missing.
    static TWeakObjectPtr<UMaterialInterface> Cached;
    static bool bMissing = false;
    if (!Cached.IsValid() && !bMissing)
    {
        Cached = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/CampusInterior/M_CampusInteriorWindow.M_CampusInteriorWindow"), nullptr, LOAD_NoWarn | LOAD_Quiet);
        bMissing = !Cached.IsValid();
    }
    return Cached.Get();
}
/** The 2 x 2 room atlas the window material reads: each 256 tile has the back wall in its inner 80%, and its border holds
 *  the side walls (left column), the ceiling (top rows) and the floor (bottom rows). Rooms: a dorm room, an office, a
 *  classroom, a lounge, all of 1986. */
UTexture2D* PaintRoomAtlas()
{
    constexpr int32 T = 256, N = T * 2;
    FHallImage Img;
    Img.Init(N, N, FLinearColor::Black);
    struct FRoomLook { FLinearColor Wall, Side, Ceiling, Floor; };
    const FRoomLook Looks[4] = {
        { FLinearColor(.62f, .58f, .46f), FLinearColor(.5f, .47f, .37f), FLinearColor(.7f, .69f, .64f), FLinearColor(.16f, .18f, .22f) },   // dorm: cinder block, blue carpet
        { FLinearColor(.55f, .5f, .4f), FLinearColor(.45f, .4f, .32f), FLinearColor(.72f, .71f, .66f), FLinearColor(.2f, .13f, .07f) },     // office: beige, brown carpet
        { FLinearColor(.6f, .6f, .52f), FLinearColor(.5f, .5f, .43f), FLinearColor(.74f, .73f, .68f), FLinearColor(.45f, .42f, .34f) },    // classroom: vinyl tile
        { FLinearColor(.5f, .38f, .26f), FLinearColor(.4f, .3f, .2f), FLinearColor(.7f, .68f, .62f), FLinearColor(.3f, .29f, .27f) },      // lounge: panelling
    };
    FRandomStream R(1986);
    for (int32 K = 0; K < 4; ++K)
    {
        const FVector2D O((K % 2) * T, (K / 2) * T);
        auto Box = [&](float X0, float Y0, float X1, float Y1, const FLinearColor& C) { Img.Rect(O + FVector2D(X0, Y0), O + FVector2D(X1, Y1), C); };
        const FRoomLook& L = Looks[K];
        Box(0, 0, T, T, L.Side);
        Box(20, 0, T, 22, L.Ceiling);
        Box(20, T - 22, T, T, L.Floor);
        Box(24, 24, T - 2, T - 24, L.Wall);
        Box(70, 26, 186, 34, FLinearColor(1.f, .97f, .9f));                       // a ceiling light along the top of the back wall
        switch (K)
        {
        case 0:   // dorm: bunk bed, posters, a desk with a lamp, a pennant
            Box(30, 150, 140, 230, FLinearColor(.25f, .1f, .08f)); Box(32, 148, 138, 160, FLinearColor(.6f, .58f, .5f));
            Box(40, 60, 90, 120, FLinearColor(.12f, .2f, .45f)); Box(44, 66, 86, 80, FLinearColor(.8f, .7f, .2f));
            Box(150, 70, 210, 140, FLinearColor(.5f, .08f, .12f));
            Box(160, 170, 240, 182, FLinearColor(.3f, .2f, .1f)); Box(200, 150, 212, 170, FLinearColor(1.f, .85f, .5f));
            Img.FillPolygon({ O + FVector2D(100, 50), O + FVector2D(140, 60), O + FVector2D(100, 70) }, FLinearColor(.35f, .15f, .5f));
            break;
        case 1:   // office: bookcase, framed diploma, filing cabinet, desk lamp
            Box(30, 50, 100, 230, FLinearColor(.18f, .1f, .05f));
            for (int32 Row = 0; Row < 5; ++Row)
                for (int32 X = 34; X < 96; X += R.RandRange(3, 6))
                    Box(X, 56 + Row * 34, X + 3, 82 + Row * 34, FLinearColor(R.FRandRange(.1f, .5f), R.FRandRange(.05f, .3f), R.FRandRange(.05f, .25f)));
            Box(130, 70, 170, 100, FLinearColor(.5f, .38f, .15f)); Box(134, 74, 166, 96, FLinearColor(.85f, .82f, .72f));
            Box(190, 130, 235, 230, FLinearColor(.35f, .36f, .34f)); for (int32 D = 0; D < 4; ++D) Box(208, 140 + D * 22, 218, 144 + D * 22, FLinearColor(.6f, .6f, .58f));
            Box(110, 175, 185, 185, FLinearColor(.28f, .16f, .07f)); Box(150, 155, 160, 175, FLinearColor(1.f, .9f, .6f));
            break;
        case 2:   // classroom: chalkboard with writing, a clock, a map
            Box(40, 70, 220, 150, FLinearColor(.06f, .13f, .08f)); Box(38, 150, 222, 156, FLinearColor(.4f, .3f, .15f));
            for (int32 Line = 0; Line < 5; ++Line) Box(50, 82 + Line * 13, 50 + R.RandRange(60, 150), 85 + Line * 13, FLinearColor(.75f, .75f, .7f));
            Img.Ellipse(O + FVector2D(128, 48), FVector2D(10, 10), 0.f, FLinearColor(.9f, .9f, .85f));
            Box(60, 175, 200, 182, FLinearColor(.3f, .25f, .15f));   // desk row
            break;
        default:  // lounge: doors along the wall, a notice board, a couch
            for (const float X : { 40.f, 170.f }) { Box(X, 80, X + 50, 230, FLinearColor(.3f, .17f, .08f)); Box(X + 40, 150, X + 45, 158, FLinearColor(.6f, .5f, .2f)); }
            Box(105, 80, 160, 130, FLinearColor(.45f, .32f, .18f));
            for (int32 Card = 0; Card < 6; ++Card) Box(108 + (Card % 3) * 17, 84 + (Card / 3) * 22, 120 + (Card % 3) * 17, 100 + (Card / 3) * 22, FLinearColor(.8f, .78f, R.FRandRange(.4f, .75f)));
            Box(95, 190, 170, 225, FLinearColor(.35f, .12f, .06f));
            break;
        }
    }
    return HallMakeTexture(Img, true, false, TEXT("CI_RoomAtlas"), false);
}
}

bool HouseCampusInteriors::RoomWindows()
{
    const bool bReady = Enabled() && HouseCampusInteriorsImpl::RoomWindowMaterial() != nullptr;
    static bool bLogged = false;
    if (!bLogged) { bLogged = true; UE_LOG(LogTemp, Display, TEXT("CAMPUS_ROOM_WINDOWS ready=%d (interiors=%d material=%d)"), bReady ? 1 : 0, Enabled() ? 1 : 0, HouseCampusInteriorsImpl::RoomWindowMaterial() ? 1 : 0); }
    return bReady;
}

int32 HouseCampusInteriors::WindowRoom(int32 Seed, int32 Side, int32 Column, int32 Floor)
{
    const uint32 H = (uint32)(Seed * 2654435761u) ^ (uint32)(Side * 40503 + Column * 7919 + Floor * 104729);
    const uint32 Pick = (H ^ (H >> 13)) % 8u;
    // Dorms are mostly dorm rooms with a lounge on each floor; the rest offices and classrooms, now and then a lounge.
    if (HouseCampusLayout::IsDorm(Seed)) return Pick == 0 ? 3 : 0;
    return Pick < 4 ? 1 : Pick < 7 ? 2 : 3;
}

void HouseCampusInteriors::CreateLook(UHouseBlackHallSubsystem* Look)
{
    namespace CI = HouseCampusInteriorsImpl;
    if (!Look || !Enabled() || Look->Materials.Contains(TEXT("CI_Plaster"))) return;
    // The closed floors' interior-mapped windows: one atlas, four rooms, each lit or dark.
    if (UMaterialInterface* WindowMat = CI::RoomWindowMaterial())
    {
        UTexture2D* Atlas = CI::PaintRoomAtlas();
        if (Atlas) Look->Textures.Add(TEXT("CI_RoomAtlas"), Atlas);
        for (int32 K = 0; K < 4; ++K)
            for (const bool bLit : { true, false })
            {
                UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(WindowMat, Look);
                if (Atlas) M->SetTextureParameterValue(TEXT("RoomAtlas"), Atlas);
                M->SetScalarParameterValue(TEXT("Room"), K);
                M->SetScalarParameterValue(TEXT("Depth"), 1.25f + .12f * K);
                M->SetScalarParameterValue(TEXT("Lit"), bLit ? 1.f : .045f);
                M->SetScalarParameterValue(TEXT("GlowIntensity"), 2.2f);
                M->SetScalarParameterValue(TEXT("Aspect"), 196.f / 159.f);   // the glazing inside the window's 8 cm frame
                Look->Materials.Add(FName(*FString::Printf(TEXT("CI_Room%d%s"), K, bLit ? TEXT("Lit") : TEXT("Dark"))), M);
            }
    }
    HallMakePlain(Look, TEXT("CI_Plaster"), FLinearColor(.56f, .52f, .44f), .85f);
    HallMakePlain(Look, TEXT("CI_PlasterWall"), FLinearColor(.5f, .47f, .38f), .85f);
    HallMakePlain(Look, TEXT("CI_Oak"), FLinearColor(.24f, .13f, .055f), .55f);
    HallMakePlain(Look, TEXT("CI_OakDark"), FLinearColor(.12f, .06f, .025f), .5f);
    HallMakePlain(Look, TEXT("CI_StairTread"), FLinearColor(.2f, .19f, .17f), .7f);
    HallMakePlain(Look, TEXT("CI_Rubber"), FLinearColor(.02f, .02f, .02f), .8f);
    HallMakePlain(Look, TEXT("CI_Brass"), FLinearColor(.55f, .38f, .12f), .3f, .9f);
    HallMakePlain(Look, TEXT("CI_Steel"), FLinearColor(.36f, .34f, .29f), .5f, .3f);    // library shelving: putty enamel
    HallMakePlain(Look, TEXT("CI_Beige"), FLinearColor(.5f, .46f, .36f), .5f);
    HallMakePlain(Look, TEXT("CI_Paper"), FLinearColor(.7f, .68f, .6f), .9f);
    HallMakePlain(Look, TEXT("CI_TrofferFrame"), FLinearColor(.6f, .6f, .58f), .5f, .2f);
    // Lit things glow by emission only.
    HallMakePlain(Look, TEXT("CI_Troffer"), FLinearColor(.9f, .93f, .95f), .4f, 0.f, FLinearColor(5.5f, 6.f, 6.2f));
    HallMakePlain(Look, TEXT("CI_LampShade"), FLinearColor(.03f, .2f, .07f), .25f, 0.f, FLinearColor(.25f, 1.6f, .4f));
    HallMakePlain(Look, TEXT("CI_Bulb"), FLinearColor(1.f, .85f, .6f), .3f, 0.f, FLinearColor(30.f, 20.f, 9.f));
    HallMakePlain(Look, TEXT("CI_FicheScreen"), FLinearColor(.2f, .25f, .2f), .2f, 0.f, FLinearColor(1.6f, 2.f, 1.4f));
    CI::PaintNoise(Look, TEXT("CI_Terrazzo"), FLinearColor(.46f, .43f, .38f), .18f, .35f, 1986, 4200, FLinearColor(.12f, .1f, .09f));
    CI::PaintNoise(Look, TEXT("CI_Carpet"), FLinearColor(.12f, .16f, .14f), .3f, .95f, 1987);
    {
        // Acoustic ceiling tile: 61 cm squares with pinholes and a grid.
        constexpr int32 N = 128;
        FHallImage Img;
        Img.Init(N, N, FLinearColor(.62f, .61f, .57f));
        FRandomStream R(61);
        for (int32 K = 0; K < 700; ++K) Img.At(R.RandRange(2, N - 3), R.RandRange(2, N - 3)) = FLinearColor(.4f, .39f, .36f);
        Img.Rect(FVector2D(0, 0), FVector2D(N, 2), FLinearColor(.45f, .45f, .43f));
        Img.Rect(FVector2D(0, 0), FVector2D(2, N), FLinearColor(.45f, .45f, .43f));
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_CeilingTile"), true);
        if (Tex) Look->Textures.Add(TEXT("CI_CeilingTile"), Tex);
        HallMakeTextured(Look, TEXT("CI_CeilingTile"), Tex, nullptr, .9f);
    }
    {
        // Book spines: bands of cloth and paper bindings, varied widths, heights and colours, with title blocks.
        constexpr int32 W = 512, H = 512;
        FHallImage Img;
        Img.Init(W, H, FLinearColor(.02f, .02f, .02f));
        FRandomStream R(1891);
        const FLinearColor Cloth[] = { FLinearColor(.25f, .04f, .03f), FLinearColor(.04f, .08f, .2f), FLinearColor(.05f, .14f, .07f), FLinearColor(.3f, .22f, .1f),
                                       FLinearColor(.18f, .1f, .05f), FLinearColor(.4f, .36f, .28f), FLinearColor(.12f, .04f, .14f), FLinearColor(.06f, .06f, .06f) };
        for (int32 Band = 0; Band < 4; ++Band)
        {
            int32 X = 0;
            while (X < W)
            {
                const int32 BW = R.RandRange(5, 16), Top = Band * 128 + R.RandRange(0, 30);
                const FLinearColor C = Cloth[R.RandRange(0, 7)] * R.FRandRange(.7f, 1.3f);
                Img.Rect(FVector2D(X, Top), FVector2D(FMath::Min(W, X + BW - 1), Band * 128 + 127), C);
                if (R.FRand() < .7f) Img.Rect(FVector2D(X + 1, Top + 12), FVector2D(FMath::Min(W, X + BW - 2), Top + 22), FLinearColor(.55f, .45f, .2f));
                X += BW;
            }
        }
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_Books"), true);
        if (Tex) Look->Textures.Add(TEXT("CI_Books"), Tex);
        HallMakeTextured(Look, TEXT("CI_Books"), Tex, nullptr, .8f);
    }
    {
        // Card catalog fronts: a grid of small oak drawers with brass label holders and pulls.
        constexpr int32 W = 256, H = 256;
        FHallImage Img;
        Img.Init(W, H, FLinearColor(.2f, .11f, .045f));
        for (int32 Row = 0; Row < 8; ++Row)
            for (int32 Col = 0; Col < 6; ++Col)
            {
                const float X0 = Col * W / 6.f, Y0 = Row * H / 8.f;
                Img.Rect(FVector2D(X0 + 2, Y0 + 2), FVector2D(X0 + W / 6.f - 2, Y0 + H / 8.f - 2), FLinearColor(.26f, .15f, .06f));
                Img.Rect(FVector2D(X0 + W / 12.f - 8, Y0 + 7), FVector2D(X0 + W / 12.f + 8, Y0 + 13), FLinearColor(.55f, .42f, .18f));
                Img.Rect(FVector2D(X0 + W / 12.f - 6, Y0 + 8), FVector2D(X0 + W / 12.f + 6, Y0 + 12), FLinearColor(.75f, .72f, .62f));
                Img.Rect(FVector2D(X0 + W / 12.f - 3, Y0 + 19), FVector2D(X0 + W / 12.f + 3, Y0 + 24), FLinearColor(.6f, .45f, .16f));
            }
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_CatalogFront"), false);
        if (Tex) Look->Textures.Add(TEXT("CI_CatalogFront"), Tex);
        HallMakeTextured(Look, TEXT("CI_CatalogFront"), Tex, nullptr, .5f);
    }
    {
        // Magazine covers on the racks.
        constexpr int32 W = 256, H = 128;
        FHallImage Img;
        Img.Init(W, H, FLinearColor(.1f, .1f, .1f));
        FRandomStream R(1986);
        for (int32 K = 0; K < 10; ++K)
        {
            const float X0 = K * W / 10.f;
            const FLinearColor C(R.FRandRange(.1f, .7f), R.FRandRange(.1f, .6f), R.FRandRange(.1f, .6f));
            Img.Rect(FVector2D(X0 + 1, 4), FVector2D(X0 + W / 10.f - 1, H - 4), C);
            Img.Rect(FVector2D(X0 + 3, 8), FVector2D(X0 + W / 10.f - 3, 18), FLinearColor(.85f, .82f, .75f));
            Img.Rect(FVector2D(X0 + 4, 40), FVector2D(X0 + W / 10.f - 4, 90), C * .4f + FLinearColor(.3f, .28f, .25f));
        }
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_Magazines"), true);
        if (Tex) Look->Textures.Add(TEXT("CI_Magazines"), Tex);
        HallMakeTextured(Look, TEXT("CI_Magazines"), Tex, nullptr, .6f);
    }
    CI::FixturesLook(Look);
    {
        // The wall clocks' dial: twelve numerals and the minute ticks on white, a black ring.
        FHallImage Img; Img.Init(256, 256, FLinearColor(.05f, .05f, .05f));
        Img.Ellipse(FVector2D(128, 128), FVector2D(124, 124), 0.f, FLinearColor(.82f, .81f, .76f));
        for (int32 K = 0; K < 60; ++K)
        {
            const float A = K * PI / 30.f;
            const FVector2D D(FMath::Sin(A), -FMath::Cos(A));
            Img.Stroke({ FVector2D(128, 128) + D * (K % 5 ? 112.f : 104.f), FVector2D(128, 128) + D * 118.f }, K % 5 ? 1.5f : 3.5f, FLinearColor(.05f, .05f, .05f));
        }
        FHallTextStyle S; S.Height = 20.f; S.Stroke = 3.f; S.Spacing = .15f;
        for (int32 H = 1; H <= 12; ++H)
        {
            const float A = H * PI / 6.f;
            const FString T = FString::FromInt(H);
            const FVector2D At = FVector2D(128, 128) + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * 84.f;
            HallDrawText(Img, T, FVector2D(At.X - HallTextWidth(T, S) * .5f, At.Y + 10.f), S, FLinearColor(.05f, .05f, .05f));
        }
        UTexture2D* Tex = HallMakeTexture(Img, true, false, TEXT("CI_ClockFace"), false);
        if (Tex) Look->Textures.Add(TEXT("CI_ClockFace"), Tex);
        HallMakeTextured(Look, TEXT("CI_ClockFace"), Tex, nullptr, .3f);
    }
    // Each walk-in building's own signs and finishes.
    for (const CI::FInteriorDef& D : CI::Interiors()) if (D.Look) D.Look(Look);
    // Open-floor window glass: the house's glass master, thinner.
    if (UMaterialInterface* GlassParent = HallHouseMat(TEXT("M_House_Glass")))
    {
        UMaterialInstanceDynamic* G = UMaterialInstanceDynamic::Create(GlassParent, Look);
        G->SetVectorParameterValue(TEXT("Tint"), FLinearColor(.02f, .025f, .03f));
        G->SetScalarParameterValue(TEXT("Opacity"), .22f);
        G->SetScalarParameterValue(TEXT("Roughness"), .05f);
        G->SetScalarParameterValue(TEXT("Metallic"), 0.f);
        Look->Materials.Add(TEXT("CI_Glass"), G);
    }
    else HallMakePlain(Look, TEXT("CI_Glass"), FLinearColor(.015f, .02f, .03f), .1f, .3f);
}

// ------------------------------------------------------------------ the door
void ACampusDoor::BeginPlay()
{
    Super::BeginPlay();
    // The leaf is lit with the room it belongs to: an inner door by the pool only; a door in an outer wall by both. A main
    // pair is always in an outer wall (the free-standing stadium's too, which is on no plan block).
    const FString GroupName=DoorGroup.ToString();
    bool bOuter = GroupName.EndsWith(TEXT("_Main"));
    if(HouseFraternityRow::Enabled())for(const auto& H:HouseFraternityRow::Houses())
        if(GroupName==FString::Printf(TEXT("Campus_%s_Rear"),H.Id))bOuter=true;
    for (const HouseCampusLayout::FBlock& B : HouseCampusLayout::Blocks())
        if (B.Rect().ExpandBy(40.f).IsInside(FVector2D(DoorwayCenter)) && !B.Rect().ExpandBy(-40.f).IsInside(FVector2D(DoorwayCenter))) bOuter = true;
    bOuterDoor = bOuter;
    TInlineComponentArray<UPrimitiveComponent*> Parts(this);
    for (UPrimitiveComponent* C : Parts) if (C) C->SetLightingChannels(bOuter, true, false);
    // An inner door shows with its building's inside (a client may receive the door before or after that is built).
    FString Id = DoorId.ToString().RightChop(7);   // Campus_<Id>_<Name>
    Id = Id.Left(FMath::Max(0, Id.Find(TEXT("_"))));
    UCampusInteriorsSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UCampusInteriorsSubsystem>() : nullptr;
    const AHouseCampusInteriors* Interiors = Sub ? Sub->GetInteriors() : nullptr;
    ShowForBuilt(Interiors && Interiors->Loaded().Contains(HouseCampusInteriors::SeedFor(*Id)));
}

void ACampusDoor::ShowForBuilt(bool bBuilt)
{
    if (bOuterDoor) return;
    if (bBuilt)
    {
        for (const TWeakObjectPtr<UPrimitiveComponent>& C : HiddenUnbuilt) if (C.IsValid()) C->SetHiddenInGame(false);
        HiddenUnbuilt.Reset();
        return;
    }
    TInlineComponentArray<UPrimitiveComponent*> Parts(this);
    for (UPrimitiveComponent* C : Parts) if (C && !C->bHiddenInGame) { C->SetHiddenInGame(true); HiddenUnbuilt.Add(C); }
}

void ACampusDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ACampusDoor, LockedReason, COND_InitialOnly);
}

// ------------------------------------------------------------------ the house's furniture on campus
FVector HouseCampusInteriors::KitScale(EKit Kit)
{
    const float S = HouseCampusInteriorsImpl::KitDef(Kit).ScaleXY;
    return FVector(S > 0.f ? S : KitScaleXY, S > 0.f ? S : KitScaleXY, 1.f);
}

void UCampusInteriorsSubsystem::LoadKit()
{
    if (bKitLoaded) return;
    bKitLoaded = true;
    namespace CI = HouseCampusInteriorsImpl;
    const int32 N = static_cast<int32>(HouseCampusInteriors::EKit::Count);
    KitMeshes.SetNum(N * CI::KitStageCount);
    int32 Loaded = 0;
    for (int32 K = 0; K < N; ++K)
        for (int32 S = 0; S < CI::KitStageCount; ++S)
        {
            const FString Name = FString(CI::KitDefs[K].Asset) + CI::KitStages[S];
            const TCHAR* Folder = CI::KitDefs[K].Folder ? CI::KitDefs[K].Folder : TEXT("BasementLibrary");
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SeniorSendOff/House/%s/%s.%s"), Folder, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
            KitMeshes[K * CI::KitStageCount + S] = Mesh;
            Loaded += Mesh ? 1 : 0;
        }
    UE_LOG(LogTemp, Display, TEXT("CAMPUS_KIT loaded=%d of %d (pieces=%d)"), Loaded, N * CI::KitStageCount, N);
}

UStaticMesh* UCampusInteriorsSubsystem::KitMesh(HouseCampusInteriors::EKit Kit, int32 Stage)
{
    LoadKit();
    const int32 I = static_cast<int32>(Kit) * HouseCampusInteriorsImpl::KitStageCount + Stage;
    return KitMeshes.IsValidIndex(I) ? KitMeshes[I].Get() : nullptr;
}

FBox UCampusInteriorsSubsystem::KitBounds(HouseCampusInteriors::EKit Kit)
{
    const UStaticMesh* Mesh = KitMesh(Kit, 0);
    if (!Mesh) return FBox(ForceInit);
    const FBox B = Mesh->GetBoundingBox();
    const FVector S = HouseCampusInteriors::KitScale(Kit);
    return FBox(B.Min * S, B.Max * S);
}

ACampusProp::ACampusProp()
{
    bBreakable = true;
}

void ACampusProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ACampusProp, KitIndex, COND_InitialOnly);
}

void ACampusProp::ApplyKit()
{
    UCampusInteriorsSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UCampusInteriorsSubsystem>() : nullptr;
    if (!Sub || KitIndex >= static_cast<uint8>(HouseCampusInteriors::EKit::Count)) return;
    const HouseCampusInteriorsImpl::FKitDef& D = HouseCampusInteriorsImpl::KitDefs[KitIndex];
    const HouseCampusInteriors::EKit K = static_cast<HouseCampusInteriors::EKit>(KitIndex);
    PropName = D.Name;
    PropId = FName(*FString::Printf(TEXT("Campus_%s_%s"), D.Asset + 7, *GetName()));   // past "SM_Lib_"
    MassKg = D.Mass;
    bMovable = true;
    bBreakable = true;
    bWallMounted = false;
    DamageCategory = TEXT("Furniture");
    IntactMesh = Sub->KitMesh(K, 0);
    DamagedMesh = Sub->KitMesh(K, 1);
    HeavilyDamagedMesh = Sub->KitMesh(K, 2);
    BrokenMesh = Sub->KitMesh(K, 3);
    Tags.AddUnique(TEXT("SSOCampusDamage"));   // a campus break is not house damage
    // The kit's scale is part of the piece: a client that joins after a break must crush from it, not from 1.
    SetOriginalScale(HouseCampusInteriors::KitScale(K));
    // Lit by the building's own pool (channel 1) like the rest of the inside; outdoors it joins channel 0 too.
    Body->SetLightingChannels(false, true, false);
    Body->SetCastShadow(false);
    RefreshProp();
}

void ACampusProp::BeginPlay()
{
    if (!IntactMesh) ApplyKit();
    Super::BeginPlay();
    // Furniture inside a building is drawn within 60 m, like its fixed decor (from farther it is behind walls or too small to
    // see); one carried out onto the campus is not culled (UpdateIndoorLighting switches it as it goes in and out).
    TInlineComponentArray<UPrimitiveComponent*> Parts(this);
    for (UPrimitiveComponent* C : Parts) if (C) C->SetCullDistance(6000.f);   // (outside, UpdateIndoorLighting lifts it)
}

// ------------------------------------------------------------------ the interiors actor
AHouseCampusInteriors::AHouseCampusInteriors()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .2f;
    bReplicates = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

TArray<int32> AHouseCampusInteriors::Loaded() const
{
    TArray<int32> Out;
    for (const FBuilt& B : Built) Out.Add(B.Seed);
    return Out;
}

int32 AHouseCampusInteriors::ActiveLights() const
{
    int32 N = 0;
    for (const UPointLightComponent* L : Lights) if (L && L->IsVisible()) ++N;
    return N;
}

void AHouseCampusInteriors::BuildNow(int32 Seed)
{
    for (const FBuilt& B : Built) if (B.Seed == Seed) return;
    const HouseCampusLayout::FBlock* Blk = HouseCampusInteriorsImpl::Block(Seed);
    UHouseBlackHallSubsystem* Look = GetWorld() ? GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>() : nullptr;
    const HouseCampusInteriorsImpl::FInteriorDef* Def = HouseCampusInteriorsImpl::InteriorFor(Seed);
    if (!Def || !Look || HouseCampusInteriors::OpenFloors(Seed) <= 0) return;
    const float Height = Blk ? Blk->Height : 0.f;
    const TCHAR* Id = Def->Id;
    const double Start = FPlatformTime::Seconds();
    HouseCampusInteriors::CreateLook(Look);
    FHallMesh Solid, Clutter, Glow;
    TArray<HouseCampusInteriors::FKitPlace> Kit;
    UCampusInteriorsSubsystem* Sub = GetWorld()->GetSubsystem<UCampusInteriorsSubsystem>();
    const HouseCampusInteriorsImpl::FKitter Kitter{ Sub, &Kit };
    TArray<FTransform> ClockXf;
    HouseCampusInteriorsImpl::SetClockSink(&ClockXf);
    if (Def->Build) Def->Build(Seed, Height, Solid, Clutter, Glow, Kitter);
    HouseCampusInteriorsImpl::SetClockSink(nullptr);
    FBuilt Entry;
    Entry.Seed = Seed;
    Entry.LastNear = GetWorld()->GetTimeSeconds();
    Entry.BuiltAt = Entry.LastNear;
    const FTransform Frame = HouseCampusInteriors::Frame(Seed);
    auto Make = [&](const FHallMesh& M, const TCHAR* Part, bool bCollision)
    {
        if (M.NumTriangles() == 0) return;
        // Unique names: a rebuild soon after a free must not reconstruct the freed component in place.
        UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), FName(*FString::Printf(TEXT("CampusInterior_%s_%s"), Id, Part))));
        PM->SetupAttachment(GetRootComponent());
        PM->SetWorldTransform(Frame);
        PM->SetMobility(EComponentMobility::Movable);
        PM->SetCastShadow(false);
        PM->bUseComplexAsSimpleCollision = true;
        PM->bUseAsyncCooking = true;   // one cook off the game thread (the build starts 35 m out, long before anyone is inside)
        // Lit only by the interior pool (channel 1); the street lights on channel 0 never reach in through a wall.
        PM->LightingChannels.bChannel0 = false;
        PM->LightingChannels.bChannel1 = true;
        if (bCollision) { PM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName); PM->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); }
        else PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PM->RegisterComponent();
        M.Build(PM, [Look](FName Key) { return Look->Mat(Key); }, bCollision);
        Entry.Parts.Add(PM);
        Entry.Triangles += M.NumTriangles();
        if (GetNetMode() != NM_DedicatedServer)
        {
            TArray<FHallMesh> Pieces = HouseCampusStaticMesh::Split(M, 12000);
            for (int32 K = 0; K < Pieces.Num(); ++K) PendingStatic.Add({ Seed, PM, MoveTemp(Pieces[K]), K == Pieces.Num() - 1 });
        }
    };
    Make(Solid, TEXT("Solid"), true);
    Make(Clutter, TEXT("Clutter"), false);
    Make(Glow, TEXT("Glow"), false);
    // Collision that is never drawn (the clock tower's belfry guards): a hidden part of its own, built like the solid one
    // and freed with it, with no static copy and not counted in the building's triangles (as HouseCampusFall's blockers).
    if (Def->Blockers)
    {
        FHallMesh Guards;
        Def->Blockers(Seed, Height, Guards);
        if (Guards.NumTriangles() > 0)
        {
            UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), FName(*FString::Printf(TEXT("CampusInterior_%s_Blockers"), Id))));
            PM->SetupAttachment(GetRootComponent());
            PM->SetWorldTransform(Frame);
            PM->SetMobility(EComponentMobility::Movable);
            PM->SetCastShadow(false);
            PM->bUseComplexAsSimpleCollision = true;
            PM->bUseAsyncCooking = true;
            PM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
            PM->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
            PM->SetVisibility(false);
            PM->SetHiddenInGame(true);
            PM->RegisterComponent();
            Guards.Build(PM, [Look](FName Key) { return Look->Mat(Key); }, true);
            Entry.Parts.Add(PM);
        }
    }
    // The house's furniture: fixed decor as plain components (every machine), props only on the server
    // (they replicate, and move and break like the house's own).
    int32 Props = 0;
    for (int32 Slot = 0; Slot < Kit.Num(); ++Slot)
    {
        const HouseCampusInteriors::FKitPlace& K = Kit[Slot];
        const FTransform T = K.T * Frame;
        if (K.bProp)
        {
            if (GetNetMode() == NM_Client) continue;
            // A prop carried out (or held) when the inside was last freed is still about: do not spawn its twin.
            Orphans.RemoveAll([](const TWeakObjectPtr<ACampusProp>& O) { return !O.IsValid(); });
            if (Orphans.ContainsByPredicate([Seed, Slot](const TWeakObjectPtr<ACampusProp>& O) { return O->Seed == Seed && O->Slot == Slot; })) continue;
            ACampusProp* P = GetWorld()->SpawnActorDeferred<ACampusProp>(ACampusProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (!P) continue;
            P->KitIndex = static_cast<uint8>(K.Kit);
            P->Seed = Seed;
            P->Slot = Slot;
            P->ApplyKit();
            P->FinishSpawning(T);
            Entry.Props.Add(P);
            ++Props;
            continue;
        }
        UStaticMesh* Mesh = Sub ? Sub->KitMesh(K.Kit) : nullptr;
        if (!Mesh) continue;
        // One plain component a piece, as the house places them (its materials are not set up for instanced drawing).
        UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
        C->SetupAttachment(GetRootComponent());
        C->SetMobility(EComponentMobility::Movable);
        C->SetStaticMesh(Mesh);
        C->SetWorldTransform(T);
        C->SetCastShadow(false);
        C->SetLightingChannels(false, true, false);
        C->LDMaxDrawDistance = 6000.f;
        if (HouseCampusInteriorsImpl::KitDefs[static_cast<int32>(K.Kit)].bCollide) { C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName); C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); }
        else C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->RegisterComponent();
        Entry.Kit.Add(C);
        ++Entry.Decor;
    }
    // The wall clocks' hands: an hour hand and a minute hand each, black, turning about the dial's centre (UpdateClocks).
    for (const FTransform& Local : ClockXf)
    {
        const FTransform World = Local * Frame;
        Entry.Clocks.Add(World);
        const float R = Local.GetScale3D().X;
        for (const float Length : { R * .55f, R * .82f })
        {
            FHallMesh Hand;
            Hand.BoxMinMax(TEXT("CI_Epoxy"), FVector(0.f, -.045f * R, -.12f * R), FVector(.6f, .045f * R, Length), HouseCampusInteriorsImpl::UVPlain());
            UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), FName(*FString::Printf(TEXT("CampusClockHand_%s"), Id))));
            PM->SetupAttachment(GetRootComponent());
            PM->SetMobility(EComponentMobility::Movable);
            PM->SetCastShadow(false);
            PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            PM->LightingChannels.bChannel0 = false;
            PM->LightingChannels.bChannel1 = true;
            PM->RegisterComponent();
            Hand.Build(PM, [Look](FName Key) { return Look->Mat(Key); }, false);
            PM->SetWorldTransform(FTransform(World.GetRotation(), World.GetLocation() + World.GetRotation().GetForwardVector() * (Length < R * .7f ? .2f : .9f)));
            Entry.Hands.Add(PM);
        }
        ++ClocksBuilt;
    }
    // Its sounds.
    if (Def->Sounds && SoundOn())
    {
        TArray<HouseCampusInteriorsImpl::FSoundSpot> Spots;
        Def->Sounds(Seed, Height, Spots);
        for (const HouseCampusInteriorsImpl::FSoundSpot& S : Spots)
            if (UAudioComponent* A = StartSound(S.Kind, Frame.TransformPosition(S.At), true)) Entry.Sounds.Add(A);
    }
    UE_LOG(LogTemp, Display, TEXT("CAMPUS_INTERIOR_BUILT building=%s seed=%d parts=%d tris=%d decor=%d props=%d clocks=%d sounds=%d ms=%.1f"), Id, Seed, Entry.Parts.Num(), Entry.Triangles,
        Entry.Decor, Props, Entry.Clocks.Num(), Entry.Sounds.Num(), (FPlatformTime::Seconds() - Start) * 1000.0);
    for (const FTransform& C : Entry.Clocks)
    {
        const FVector P = C.GetLocation(), N = C.GetRotation().GetForwardVector();
        UE_LOG(LogTemp, Verbose, TEXT("CAMPUS_INTERIOR_CLOCK building=%s at=(%.0f,%.0f,%.0f) facing=(%.2f,%.2f)"), Id, P.X, P.Y, P.Z, N.X, N.Y);
    }
    NextClock = 0.0;   // set the new hands now
    Built.Add(MoveTemp(Entry));
    NextConvert = GetWorld()->GetTimeSeconds() + .2;   // not on the build's own frame
    ShowDoors(Seed, true);
}

void AHouseCampusInteriors::Free(int32 Seed)
{
    for (int32 I = 0; I < Built.Num(); ++I)
    {
        if (Built[I].Seed != Seed) continue;
        for (UProceduralMeshComponent* PM : Built[I].Parts) if (PM) PM->DestroyComponent();
        for (UStaticMeshComponent* C : Built[I].Drawn) if (C) C->DestroyComponent();
        PendingStatic.RemoveAll([Seed](const FPendingStatic& P) { return P.Seed == Seed; });
        ShowDoors(Seed, false);
        for (UStaticMeshComponent* C : Built[I].Kit) if (C) C->DestroyComponent();
        for (UProceduralMeshComponent* PM : Built[I].Hands) if (PM) PM->DestroyComponent();
        for (UAudioComponent* A : Built[I].Sounds) if (A) { A->Stop(); A->DestroyComponent(); }
        // Server: the props go with the inside, except one someone is holding or has carried out, which stays where it is.
        for (const TWeakObjectPtr<ACampusProp>& P : Built[I].Props)
        {
            if (!P.IsValid()) continue;
            if ((P->Body && P->Body->ComponentHasTag(TEXT("SSOFurnitureHeld"))) || HouseCampusInteriors::DistanceOutside(P->GetActorLocation()) > 0.f) { Orphans.AddUnique(P); continue; }
            P->Destroy();
        }
        // Orphans of this building that were brought back inside and put down go with it too (their place respawns them).
        for (int32 O = Orphans.Num() - 1; O >= 0; --O)
        {
            ACampusProp* P = Orphans[O].Get();
            if (!P) { Orphans.RemoveAt(O); continue; }
            if (P->Seed != Seed || (P->Body && P->Body->ComponentHasTag(TEXT("SSOFurnitureHeld"))) || HouseCampusInteriors::DistanceOutside(P->GetActorLocation()) > 0.f) continue;
            if (!HouseCampusInteriors::InsideInterior(Seed, P->GetActorLocation())) continue;
            P->Destroy();
            Orphans.RemoveAt(O);
        }
        UE_LOG(LogTemp, Display, TEXT("CAMPUS_INTERIOR_FREED seed=%d"), Seed);
        Built.RemoveAt(I);
        return;
    }
}

int32 AHouseCampusInteriors::PendingConversions(int32 Seed) const
{
    int32 N = 0;
    for (const FPendingStatic& P : PendingStatic) N += P.Seed == Seed ? 1 : 0;
    return N;
}

void AHouseCampusInteriors::ShowDoors(int32 Seed, bool bBuilt)
{
    const HouseCampusInteriorsImpl::FInteriorDef* Def = HouseCampusInteriorsImpl::InteriorFor(Seed);
    if (!Def) return;
    const FString Prefix = FString::Printf(TEXT("Campus_%s_"), Def->Id);
    for (TActorIterator<ACampusDoor> It(GetWorld()); It; ++It) if (It->DoorId.ToString().StartsWith(Prefix)) It->ShowForBuilt(bBuilt);
}

void AHouseCampusInteriors::ConvertPending()
{
    // One part a tick: draw it as a runtime static mesh (its draws cached by the renderer, where the procedural part's are
    // gathered every frame) and hide the procedural one, which keeps its collision.
    if (PendingStatic.Num() == 0) return;
    const FPendingStatic P = MoveTemp(PendingStatic[0]);
    PendingStatic.RemoveAt(0);
    UProceduralMeshComponent* PM = P.Part.Get();
    FBuilt* B = Built.FindByPredicate([&P](const FBuilt& E) { return E.Seed == P.Seed; });
    UHouseBlackHallSubsystem* Look = GetWorld() ? GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>() : nullptr;
    if (!PM || !B || !B->Parts.Contains(PM) || !Look) return;
    const double Start = FPlatformTime::Seconds();
    UStaticMesh* Mesh = HouseCampusStaticMesh::Build(P.Mesh, FName(*(TEXT("SM_") + PM->GetName())), Look);
    if (!Mesh) return;
    UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), FName(*(PM->GetName() + TEXT("_Drawn")))));
    C->SetupAttachment(GetRootComponent());
    C->SetMobility(EComponentMobility::Movable);
    C->SetWorldTransform(PM->GetComponentTransform());
    C->SetStaticMesh(Mesh);
    C->SetCastShadow(false);
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->LightingChannels.bChannel0 = false;
    C->LightingChannels.bChannel1 = true;
    C->RegisterComponent();
    if (P.bLast) PM->SetVisibility(false);
    B->Drawn.Add(C);
    UE_LOG(LogTemp, Display, TEXT("CAMPUS_INTERIOR_STATIC %s tris=%d ms=%.1f"), *PM->GetName(), P.Mesh.NumTriangles(), (FPlatformTime::Seconds() - Start) * 1000.0);
}

void AHouseCampusInteriors::UpdateLights(const FVector& View)
{
    // The pool: up to six shadowless lights at the anchors nearest the view, only while the view is inside a building.
    if (Lights.Num() == 0)
        for (int32 K = 0; K < LightPool; ++K)
        {
            UPointLightComponent* L = NewObject<UPointLightComponent>(this, *FString::Printf(TEXT("CampusInteriorLight%d"), K));
            L->SetupAttachment(GetRootComponent());
            L->SetMobility(EComponentMobility::Movable);
            L->SetCastShadows(false);
            L->LightingChannels.bChannel0 = false;
            L->LightingChannels.bChannel1 = true;
            L->SetIntensityUnits(ELightUnits::Lumens);
            L->SetIntensity(3200.f);
            L->SetAttenuationRadius(1150.f);
            L->SetLightColor(FLinearColor(1.f, .93f, .82f));
            L->SetVisibility(false);
            L->RegisterComponent();
            Lights.Add(L);
        }
    // Inside a building: its anchors on the view's own floor (a slab between would let a shadowless light shine through).
    // Just outside one (within 25 m): three of its nearest ground-floor anchors, so the rooms seen through the doors and
    // windows are lit too; they are on channel 1, so the street gets no light from them.
    TArray<FVector> Anchors;
    int32 Budget = LightPool;
    const FBuilt* Outside = nullptr;
    float OutsideDist = 2500.f;
    // Which floor an anchor or the view is on: a plan block by its storeys (the world height less its pad); a free-standing
    // interior by height bands. It takes world heights: a plan block's anchors are in its frame, so their pad is added first.
    auto FloorOf = [](int32 Seed, float Z)
    {
        if (const HouseCampusLayout::FBlock* Blk = HouseCampusInteriorsImpl::Block(Seed))
            return FMath::FloorToInt((Z - HouseCampusLayout::BlockDZ(Seed) - HouseCampusInteriors::FloorTop(Blk->Height, 0) + 20.f) / HouseCampusInteriors::Story(Blk->Height));
        for (int32 K = 8; K > 0; --K) if (Z > HouseCampusInteriors::InteriorFloorZ(Seed, K) - 20.f) return K;
        return 0;
    };
    for (const FBuilt& B : Built)
    {
        const HouseCampusInteriorsImpl::FInteriorDef* Def = HouseCampusInteriorsImpl::InteriorFor(B.Seed);
        if (!Def || !Def->Anchors) continue;
        const HouseCampusLayout::FBlock* Blk = HouseCampusInteriorsImpl::Block(B.Seed);
        const float Height = Blk ? Blk->Height : 0.f;
        if (!HouseCampusInteriors::InsideInterior(B.Seed, View))
        {
            const float D = FMath::Sqrt(HouseCampusInteriors::InteriorBox(B.Seed).ComputeSquaredDistanceToPoint(View));
            if (D < OutsideDist && !Def->bLightsInsideOnly) { OutsideDist = D; Outside = &B; }
            continue;
        }
        const int32 ViewFloor = FloorOf(B.Seed, View.Z - 150.f);
        TArray<FVector> Local;
        Def->Anchors(B.Seed, Height, Local);
        const FTransform Frame = HouseCampusInteriors::Frame(B.Seed);
        for (const FVector& P : Local)
            if (FloorOf(B.Seed, P.Z + HouseCampusLayout::BlockDZ(B.Seed) - Def->LightAnchorFloorOffset) == ViewFloor) Anchors.Add(Frame.TransformPosition(P));
    }
    if (Anchors.Num() == 0 && Outside)
    {
        const HouseCampusInteriorsImpl::FInteriorDef* Def = HouseCampusInteriorsImpl::InteriorFor(Outside->Seed);
        const HouseCampusLayout::FBlock* Blk = HouseCampusInteriorsImpl::Block(Outside->Seed);
        TArray<FVector> Local;
        Def->Anchors(Outside->Seed, Blk ? Blk->Height : 0.f, Local);
        const FTransform Frame = HouseCampusInteriors::Frame(Outside->Seed);
        for (const FVector& P : Local) if (FloorOf(Outside->Seed, P.Z + HouseCampusLayout::BlockDZ(Outside->Seed) - Def->LightAnchorFloorOffset) == 0) Anchors.Add(Frame.TransformPosition(P));
        Budget = 3;
    }
    Anchors.Sort([&View](const FVector& A, const FVector& B) { return FVector::DistSquared(A, View) < FVector::DistSquared(B, View); });
    for (int32 K = 0; K < Lights.Num(); ++K)
    {
        const bool bOn = K < Budget && Anchors.IsValidIndex(K);
        if (bOn) Lights[K]->SetWorldLocation(Anchors[K]);
        if (Lights[K]->IsVisible() != bOn) Lights[K]->SetVisibility(bOn);
    }
}

void AHouseCampusInteriors::UpdateIndoorLighting()
{
    // Pawns, the house's props and the campus props: inside a walk-in building they take its pool (channel 1) and not the
    // street (the shell casts no shadow, so channel 0 would light them through the roof); outside, the street. A campus
    // prop keeps channel 1 outside too. Only components whose setting differs are touched.
    auto Apply = [](AActor* A, bool bCampus)
    {
        const bool bIn = HouseCampusInteriors::DistanceOutside(A->GetActorLocation()) <= 0.f;
        const bool bCh0 = !bIn, bCh1 = bIn || bCampus;
        TInlineComponentArray<UPrimitiveComponent*> Parts(A);
        for (UPrimitiveComponent* C : Parts)
        {
            if (C && (C->LightingChannels.bChannel0 != bCh0 || C->LightingChannels.bChannel1 != bCh1)) C->SetLightingChannels(bCh0, bCh1, false);
            // A campus prop is culled at 60 m inside a building, not out on the campus.
            if (C && bCampus && C->LDMaxDrawDistance != (bIn ? 6000.f : 0.f)) C->SetCullDistance(bIn ? 6000.f : 0.f);
        }
    };
    for (TActorIterator<AHouseInteractiveProp> It(GetWorld()); It; ++It) Apply(*It, It->IsA<ACampusProp>());
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        Apply(*It, false);
        TArray<AActor*> Held;
        It->GetAttachedActors(Held, true, true);
        for (AActor* H : Held) if (H && !H->IsA<AHouseInteractiveProp>()) Apply(H, false);
    }
}

bool AHouseCampusInteriors::FreeWouldDrop(int32 Seed) const
{
    for (TActorIterator<AHouseInteractiveProp> It(GetWorld()); It; ++It)
    {
        if (!HouseCampusInteriors::InsideInterior(Seed, It->GetActorLocation())) continue;
        // This building's own props (and its orphans brought home) go with it; anything else would fall.
        if (const ACampusProp* P = Cast<ACampusProp>(*It)) if (P->Seed == Seed) continue;
        return true;
    }
    return false;
}

void AHouseCampusInteriors::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UWorld* W = GetWorld();
    if (!W) return;
    const double Now = W->GetTimeSeconds();
    FVector View = FVector(1e9f);
    TArray<FVector> Interest, PlayerInterest;
    if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(W, 0)) { View = PCM->GetCameraLocation(); Interest.Add(View); PlayerInterest.Add(View); }
    for (TActorIterator<APawn> It(W); It; ++It)
    {
        Interest.Add(It->GetActorLocation());
        if(It->IsPlayerControlled()||It->GetPlayerState()!=nullptr)PlayerInterest.Add(It->GetActorLocation());
    }
    static const bool bAll = FParse::Param(FCommandLine::Get(), TEXT("CampusInteriorsAll"));
    if (Now >= NextCheck)
    {
        NextCheck = Now + .5;
        UpdateIndoorLighting();
        TArray<int32> Seeds;
        HouseCampusInteriors::InteriorSeeds(Seeds);
        for (const int32 Seed : Seeds)
        {
            const FBox Box = HouseCampusInteriors::InteriorBox(Seed);
            const FBox2D R(FVector2D(Box.Min), FVector2D(Box.Max));
            float Near = FLT_MAX;
            const auto* Definition=HouseCampusInteriorsImpl::InteriorFor(Seed);
            const TArray<FVector>& Relevant=Definition&&Definition->bPlayerProximityOnly?PlayerInterest:Interest;
            for (const FVector& P : Relevant) Near = FMath::Min(Near, FMath::Sqrt(R.ComputeSquaredDistanceToPoint(FVector2D(P.X, P.Y))));
            FBuilt* Have = Built.FindByPredicate([Seed](const FBuilt& B) { return B.Seed == Seed; });
            if (bAll || Near < HouseCampusInteriorsImpl::LoadRange)
            {
                if (!Have) BuildNow(Seed);
                else Have->LastNear = Now;
            }
            else if (Have && Near > HouseCampusInteriorsImpl::FreeRange && Now - Have->LastNear > HouseCampusInteriorsImpl::FreeAfter)
            {
                if (!FreeWouldDrop(Seed)) Free(Seed);
                else Have->LastNear = Now;   // keep its floors while something else rests on them
            }
            else if (Have && Near <= HouseCampusInteriorsImpl::FreeRange) Have->LastNear = Now;
        }
    }
    UpdateLights(View);
    if (Now >= NextConvert) { NextConvert = Now + .2; ConvertPending(); }
    if (Now >= NextClock) { NextClock = Now + 1.0; UpdateClocks(); }
    UpdateSound(View, DeltaSeconds);
    // A few fluorescent tubes on their way out: the shared CI_TrofferFlicker material stutters now and then (the same on
    // every machine is not needed: it is cosmetic).
    if (UHouseBlackHallSubsystem* Look = W->GetSubsystem<UHouseBlackHallSubsystem>())
        if (UMaterialInstanceDynamic* Flicker = Cast<UMaterialInstanceDynamic>(Look->Mat(TEXT("CI_TrofferFlicker"))))
        {
            const float Phase = FMath::Frac(float(Now) * .37f);
            const bool bOut = (Phase > .62f && Phase < .66f) || (Phase > .70f && Phase < .715f) || FMath::Frac(float(Now) * 7.3f) < .04f;
            Flicker->SetVectorParameterValue(TEXT("Emission"), bOut ? FLinearColor(.4f, .45f, .5f) : FLinearColor(5.5f, 6.f, 6.2f));
        }
}

namespace HouseCampusInteriorsImpl
{
/** The hour and minute hands' angles (radians, clockwise from 12) for the clock tower's time (the local wall clock, or its
 *  -ClockTime= override); without the tower, the wall clock. */
void ClockHandAngles(const UWorld* W, float& HourA, float& MinuteA)
{
    const AHouseClockTower* Tower = AHouseClockTower::Find(W);
    const double Clock = Tower ? Tower->ClockSeconds() : FDateTime::Now().GetTimeOfDay().GetTotalSeconds();
    HourA = float(FMath::DegreesToRadians(FMath::Fmod(Clock, 43200.0) / 3600.0 * 30.0));
    MinuteA = float(FMath::DegreesToRadians(FMath::Fmod(Clock, 3600.0) / 60.0 * 6.0));
}
}
bool AHouseCampusInteriors::Stats(int32 Seed, FStats& Out) const
{
    Out = FStats();
    const FBuilt* B = Built.FindByPredicate([Seed](const FBuilt& E) { return E.Seed == Seed; });
    if (!B) return false;
    Out.Parts = B->Parts.Num(); Out.Triangles = B->Triangles; Out.Decor = B->Decor;
    for (const TObjectPtr<UProceduralMeshComponent>& PM : B->Parts) Out.DrawnParts += PM && !PM->IsVisible() && B->Drawn.Num() > 0 ? 1 : 0;
    Out.Clocks = B->Clocks.Num(); Out.Hands = B->Hands.Num(); Out.Sounds = B->Sounds.Num();
    for (const TObjectPtr<UAudioComponent>& A : B->Sounds) Out.PlayingSounds += A && A->IsPlaying() ? 1 : 0;
    for (const UProceduralMeshComponent* PM : B->Parts)
    {
        if (!PM) continue;
        for (int32 I = 0; I < PM->GetNumMaterials(); ++I) Out.Materials.AddUnique(PM->GetMaterial(I));
        if (!PM->GetName().Contains(TEXT("_Solid"))) continue;
        ++Out.SolidParts;
        // Collision that is really there: the cooked body made it into the physics scene (a body setup alone is always
        // there, cooked or not).
        const FBodyInstance* Body = PM->GetBodyInstance();
        if (PM->GetCollisionEnabled() != ECollisionEnabled::NoCollision && Body && Body->IsValidBodyInstance()) ++Out.CollidingParts;
    }
    return true;
}

float AHouseCampusInteriors::ClockError(int32 Seed, int32& OutClocks) const
{
    OutClocks = 0;
    const FBuilt* B = Built.FindByPredicate([Seed](const FBuilt& E) { return E.Seed == Seed; });
    // Against the tower's time the hands were last set to, which must be at most two seconds old in game time (so a stall
    // of the whole process, when the wall clock runs on and the game does not, is not read as a slow clock).
    const double Now = GetWorld()->GetTimeSeconds();
    if (!B || Now - B->BuiltAt < 2.0 || ShownAt < B->BuiltAt || Now - ShownAt > 2.0) return -1.f;
    const float HourA = ShownHourA, MinuteA = ShownMinuteA;
    float Worst = 0.f;
    for (int32 C = 0; C < B->Clocks.Num(); ++C)
    {
        const FVector N = B->Clocks[C].GetRotation().GetForwardVector(), Right = FVector::CrossProduct(FVector::UpVector, -N).GetSafeNormal();
        for (int32 H = 0; H < 2; ++H)
        {
            const UProceduralMeshComponent* PM = B->Hands.IsValidIndex(C * 2 + H) ? B->Hands[C * 2 + H].Get() : nullptr;
            if (!PM) return 180.f;   // a clock without its hands
            if (FVector::Dist(PM->GetComponentLocation(), B->Clocks[C].GetLocation()) > 15.f) return 90.f;   // a hand off its dial
            const FVector Along = PM->GetComponentQuat().GetUpVector();
            const float Shown = FMath::Atan2(FVector::DotProduct(Along, Right), FVector::DotProduct(Along, FVector::UpVector));
            const float Want = H == 0 ? HourA : MinuteA;
            Worst = FMath::Max(Worst, FMath::Abs(FMath::RadiansToDegrees(FMath::FindDeltaAngleRadians(Want, Shown))));
        }
        ++OutClocks;
    }
    return Worst;
}

void AHouseCampusInteriors::UpdateClocks()
{
    float HourA = 0.f, MinuteA = 0.f;
    HouseCampusInteriorsImpl::ClockHandAngles(GetWorld(), HourA, MinuteA);
    ShownHourA = HourA; ShownMinuteA = MinuteA; ShownAt = GetWorld()->GetTimeSeconds();
    for (const FBuilt& B : Built)
        for (int32 C = 0; C < B.Clocks.Num(); ++C)
        {
            const FTransform& T = B.Clocks[C];
            const FVector N = T.GetRotation().GetForwardVector(), Up = FVector::UpVector, Right = FVector::CrossProduct(Up, -N).GetSafeNormal();
            for (int32 H = 0; H < 2; ++H)
            {
                UProceduralMeshComponent* PM = B.Hands.IsValidIndex(C * 2 + H) ? B.Hands[C * 2 + H].Get() : nullptr;
                if (!PM) continue;
                const float A = H == 0 ? HourA : MinuteA;
                const FVector Dir = Up * FMath::Cos(A) + Right * FMath::Sin(A);
                PM->SetWorldRotation(FRotationMatrix::MakeFromXZ(N, Dir).Rotator());
            }
        }
}

bool AHouseCampusInteriors::SoundOn() const
{
    static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("NoCampusSound"));
    return !bOff && GEngine && GEngine->UseSound() && GetNetMode() != NM_DedicatedServer;
}

UAudioComponent* AHouseCampusInteriors::StartSound(int32 Kind, const FVector& At, bool bSpatial)
{
    if (!SoundOn() || Kind < 0 || Kind >= 4) return nullptr;
    if (Pcm.Num() < 4)
    {
        const double SynthStart = FPlatformTime::Seconds();
        Pcm.SetNum(4);
        for (int32 K = 0; K < 4; ++K) HouseCampusInteriorsImpl::SoundPcm(K, Pcm[K]);
        UE_LOG(LogTemp, Display, TEXT("CAMPUS_INTERIOR_SOUND synthesised loops=4 ms=%.1f"), (FPlatformTime::Seconds() - SynthStart) * 1000.0);
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("CampusSoundDump")))
        {
            // Each loop as a 16-bit mono WAV, for listening to (and measuring) without the game.
            static const TCHAR* Names[] = { TEXT("Drip"), TEXT("TVCrowd"), TEXT("RadioShow"), TEXT("RoomTone") };
            for (int32 K = 0; K < 4; ++K)
            {
                TArray<uint8> Wav;
                auto U32 = [&Wav](uint32 V) { for (int32 B = 0; B < 4; ++B) Wav.Add(uint8(V >> (8 * B))); };
                auto U16 = [&Wav](uint16 V) { Wav.Add(uint8(V)); Wav.Add(uint8(V >> 8)); };
                const uint32 Bytes = uint32(Pcm[K].Num() * sizeof(int16)), Rate = uint32(HouseCampusInteriorsImpl::SoundRate);
                Wav.Append(reinterpret_cast<const uint8*>("RIFF"), 4); U32(36 + Bytes); Wav.Append(reinterpret_cast<const uint8*>("WAVEfmt "), 8);
                U32(16); U16(1); U16(1); U32(Rate); U32(Rate * 2); U16(2); U16(16);
                Wav.Append(reinterpret_cast<const uint8*>("data"), 4); U32(Bytes);
                Wav.Append(reinterpret_cast<const uint8*>(Pcm[K].GetData()), int32(Bytes));
                const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CampusInteriors") / FString::Printf(TEXT("CampusSound_%s.wav"), Names[K]));
                FFileHelper::SaveArrayToFile(Wav, *Path);
                UE_LOG(LogTemp, Display, TEXT("CAMPUS_INTERIOR_SOUND dumped=%s seconds=%.2f"), *Path, Pcm[K].Num() / float(Rate));
            }
        }
#endif
    }
    if (bSpatial && !SoundFalloff)
    {
        // Close sounds: full within 2.5 m, gone by 13 m; muffled with distance.
        SoundFalloff = NewObject<USoundAttenuation>(this, NAME_None, RF_Transient);
        FSoundAttenuationSettings& A = SoundFalloff->Attenuation;
        A.bAttenuate = true; A.bSpatialize = true;
        A.AttenuationShape = EAttenuationShape::Sphere;
        A.AttenuationShapeExtents = FVector(250.f, 0.f, 0.f);
        A.FalloffDistance = 1050.f;
        A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
        A.bAttenuateWithLPF = true; A.LPFRadiusMin = 300.f; A.LPFRadiusMax = 1200.f; A.LPFFrequencyAtMin = 20000.f; A.LPFFrequencyAtMax = 3000.f;
    }
    UAudioComponent* C = NewObject<UAudioComponent>(this, NAME_None, RF_Transient);
    C->SetupAttachment(GetRootComponent());
    C->bAutoActivate = false;
    C->bAutoDestroy = false;
    C->bAllowSpatialization = bSpatial;
    if (bSpatial) C->AttenuationSettings = SoundFalloff;
    C->RegisterComponent();
    C->SetWorldLocation(At);
    USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(C, NAME_None, RF_Transient);
    W->SetSampleRate(HouseCampusInteriorsImpl::SoundRate);
    W->NumChannels = 1;
    W->Duration = INDEFINITELY_LOOPING_DURATION;
    W->SoundGroup = SOUNDGROUP_Default;
    W->bLooping = false;
    W->bRequiresStopFade = true;
    W->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
    FFeed F;
    F.Comp = C; F.Wave = W; F.Kind = Kind;
    // Start each copy of a sound somewhere else in its loop.
    // (Through int32: a negative float converted straight to uint32 is undefined, and gave every copy the same start.)
    F.Cursor = Pcm[Kind].Num() > 0 ? int32((uint32(int32(At.X * 7.f + At.Y * 13.f)) * 2654435761u) % uint32(Pcm[Kind].Num())) : 0;
    Feeds.Add(F);
    C->SetSound(W);
    C->Play();
    ++SoundsStarted;
    return C;
}

void AHouseCampusInteriors::UpdateSound(const FVector& View, float DeltaSeconds)
{
    if (!SoundOn()) return;
    // Inside: the room tone fades in, and the drumline from the stadium comes through the windows.
    const bool bInside = HouseCampusInteriors::DistanceOutside(View) < 1.f;
    if (IndoorState != int32(bInside))
    {
        IndoorState = int32(bInside);
        for (TActorIterator<AHouseCampusFall> It(GetWorld()); It; ++It) It->SetIndoors(bInside);
    }
    AmbienceLevel = FMath::FInterpConstantTo(AmbienceLevel, bInside ? .3f : 0.f, DeltaSeconds, .5f);
    if (AmbienceLevel > .01f && !Ambience) Ambience = StartSound(3, View, false);
    if (Ambience)
    {
        Ambience->SetVolumeMultiplier(FMath::Max(.001f, AmbienceLevel));
        if (AmbienceLevel <= .005f) { Ambience->Stop(); Ambience->DestroyComponent(); Ambience = nullptr; }
    }
    // Keep every looping wave a second ahead.
    Feeds.RemoveAll([](const FFeed& F) { return !F.Comp.IsValid() || !F.Wave.IsValid(); });
    for (FFeed& F : Feeds)
    {
        const TArray<int16>& Loop = Pcm.IsValidIndex(F.Kind) ? Pcm[F.Kind] : Pcm[0];
        if (Loop.Num() == 0) continue;
        while (F.Wave->GetAvailableAudioByteCount() < HouseCampusInteriorsImpl::SoundRate * 2)
        {
            const int32 Count = FMath::Min(HouseCampusInteriorsImpl::SoundRate / 2, Loop.Num() - F.Cursor);
            F.Wave->QueueAudio(reinterpret_cast<const uint8*>(Loop.GetData() + F.Cursor), Count * sizeof(int16));
            F.Cursor = (F.Cursor + Count) % Loop.Num();
            F.Queued += Count * int64(sizeof(int16));
        }
        const int64 Played = F.Queued - F.Wave->GetAvailableAudioByteCount();
        if (!F.bHeard && Played > HouseCampusInteriorsImpl::SoundRate * int64(sizeof(int16)))
        {
            F.bHeard = true;
            UE_LOG(LogTemp, Display, TEXT("CAMPUS_INTERIOR_SOUND kind=%d playing=%d playedSeconds=%.1f distance=%.0f"), F.Kind, int32(F.Comp->IsPlaying()),
                Played / float(HouseCampusInteriorsImpl::SoundRate * sizeof(int16)), FVector::Dist(View, F.Comp->GetComponentLocation()));
        }
    }
#if !UE_BUILD_SHIPPING
    // The proof that it is heard in the game: the mix, recorded for a few seconds once the view stands within 3.5 m of a
    // placed sound (the drip, a TV, a radio).
    static int32 RecordSeconds = -1;
    if (RecordSeconds < 0) { RecordSeconds = 0; FParse::Value(FCommandLine::Get(), TEXT("CampusSoundRecord="), RecordSeconds); }
    if (RecordSeconds > 0 && !bRecorded)
    {
        const double Now = GetWorld()->GetAudioTimeSeconds();
        if (RecordUntil < 0.0)
        {
            for (const FFeed& F : Feeds)
                if (F.Kind != 3 && F.bHeard && FVector::Dist(View, F.Comp->GetComponentLocation()) < 350.f)
                {
                    UAudioMixerBlueprintLibrary::StartRecordingOutput(this, float(RecordSeconds) + 2.f);
                    RecordUntil = Now + RecordSeconds;
                    UE_LOG(LogTemp, Display, TEXT("CAMPUS_INTERIOR_SOUND recording kind=%d distance=%.0f indoors=%d"), F.Kind, FVector::Dist(View, F.Comp->GetComponentLocation()), IndoorState);
                    break;
                }
        }
        else if (Now >= RecordUntil)
        {
            const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CampusInteriors"));
            IFileManager::Get().MakeDirectory(*Dir, true);
            UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, TEXT("CampusInteriorSound"), Dir);
            bRecorded = true;
            UE_LOG(LogTemp, Display, TEXT("CAMPUS_INTERIOR_SOUND recorded=%s"), *(Dir / TEXT("CampusInteriorSound.wav")));
        }
    }
#endif
}

// ------------------------------------------------------------------ the subsystem
void UCampusInteriorsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (UGameplayStatics::GetCurrentLevelName(&InWorld, true) != TEXT("Chapter01_House") || !HouseCampusInteriors::Enabled()) return;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;
    LoadKit();
    Interiors = InWorld.SpawnActor<AHouseCampusInteriors>(AHouseCampusInteriors::StaticClass(), FTransform::Identity, Params);
    if (InWorld.GetNetMode() != NM_Client) SpawnDoors(InWorld);
}

void UCampusInteriorsSubsystem::SpawnDoors(UWorld& World)
{
    // House-only door checks count every door in the world; campus doors stay out of those runs.
    for (const TCHAR* P : { TEXT("ExistingHouseDoorSmoke"), TEXT("DoorSwingAudit"), TEXT("DoorSwingReview"), TEXT("DoorSwingNetworkReview"), TEXT("FrontDoubleDoorReview") })
        if (FParse::Param(FCommandLine::Get(), P)) return;
    TArray<int32> Seeds;
    HouseCampusInteriors::InteriorSeeds(Seeds);
    for (const int32 Seed : Seeds)
    {
        const HouseCampusLayout::FBlock* Blk = HouseCampusInteriorsImpl::Block(Seed);
        const FTransform Frame = HouseCampusInteriors::Frame(Seed);
        const HouseCampusInteriorsImpl::FInteriorDef* Def = HouseCampusInteriorsImpl::InteriorFor(Seed);
        if (!Def || !Def->Doors) continue;
        TArray<HouseCampusInteriorsImpl::FDoorSpec> Specs;
        Def->Doors(Seed, Blk ? Blk->Height : 0.f, Specs);
        for (const HouseCampusInteriorsImpl::FDoorSpec& D : Specs)
        {
            const FTransform T = FTransform(FRotator(0, D.Yaw, 0), D.Hinge) * Frame;
            ACampusDoor* Door = World.SpawnActorDeferred<ACampusDoor>(ACampusDoor::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (!Door) continue;
            Door->Setup(D.Style, D.Width, D.Height, D.RoomSign, D.Flags);
            Door->SetStartsOpen(D.bOpen);
            Door->DoorwayCenter = Frame.TransformPosition(D.Center);
            Door->DoorGroup = D.Group;
            Door->DoorId = FName(*FString::Printf(TEXT("Campus_%s_%s"), Def->Id, *D.Name.ToString()));
            Door->bLocked = !D.Reason.IsEmpty();
            Door->LockedReason = D.Reason;
            Door->Tags.Add(TEXT("SSOCampusDamage"));
            if (FCString::Strcmp(Def->Id, TEXT("TWR")) == 0) Door->bAlwaysRelevant = true;   // the landmark's door, seen from anywhere
            Door->FinishSpawning(T);
            Doors.Add(Door);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("CAMPUS_INTERIOR_DOORS spawned=%d"), Doors.Num());
}
