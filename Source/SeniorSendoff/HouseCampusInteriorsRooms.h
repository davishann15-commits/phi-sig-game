// Calder State's walk-in buildings: the room-building helpers and the house-kit placer every building's file shares
// (HouseCampusInteriors*.cpp), and the registry of walk-in buildings. Internal to the campus interiors.
//
// A walk-in building is an FInteriorDef: which plan block it is, how many floors are open, and functions that build its
// inside (in its canonical frame, main doors on +Y), place its pooled interior lights, cut its outer-wall door openings
// and ask for its doors. HouseCampusInteriors.cpp runs them (streaming, lights, doors, the shell's openings).
#pragma once

#include "CoreMinimal.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallMesh.h"
#include "HouseCampusInteriors.h"
#include "HouseCampusLayout.h"

class UHouseBlackHallSubsystem;

namespace HouseCampusInteriorsImpl
{
constexpr float Ground = -60.f, WallT = 30.f, OuterSkin = 20.f, PartT = 15.f;

const HouseCampusLayout::FBlock* Block(int32 Seed);
FBox2D Rect(float X0, float X1, float Y0, float Y1);
/** The rectangle [U0,U1]x[Z0,Z1] minus Holes, as boxes: split into Z bands at every hole edge, and fill each band's U gaps. */
void Subtract(float U0, float U1, float Z0, float Z1, const TArray<FBox2D>& Holes, TFunctionRef<void(float, float, float, float)> Emit);
void SubtractPlan(const FBox2D& R, const TArray<FBox2D>& Holes, TFunctionRef<void(const FBox2D&)> Emit);

// ---- painted materials
struct FTextLine { const TCHAR* Text; float Height; };
/** A painted sign (lines of capital text, centred) as material Key. */
void PaintText(UHouseBlackHallSubsystem* Look, FName Key, int32 W, int32 H, const FLinearColor& Bg, const FLinearColor& Ink, std::initializer_list<FTextLine> Lines, bool bBorder, const FLinearColor& Emission = FLinearColor::Black);
/** A noisy flat finish (terrazzo, carpet, linoleum) as material Key. */
void PaintNoise(UHouseBlackHallSubsystem* Look, FName Key, const FLinearColor& Base, float Grain, float Rough, int32 Seed, int32 Chips = 0, const FLinearColor& Chip = FLinearColor::Black);

// ---- the room kit (FHallMesh, canonical frame)
const FHallUV& UVPlain();
const FHallUV& UVFine();
void Box(FHallMesh& M, FName K, const FVector& A, const FVector& B, const FHallUV& UV = UVPlain());
/** Oak wainscot along a wall face (see the definition). */
void Wainscot(FHallMesh& M, bool bAlongX, float Plane, float Out, float U0, float U1, float Z, const TArray<FVector2D>& Gaps, float Height = 82.f);
/** A partition along X at Y (or along Y at X when bAlongY), PartT thick, with doorways (U ranges, open up to Head), oak
 *  casings and wainscot. */
void Partition(FHallMesh& M, FName K, bool bAlongY, float At, float U0, float U1, float Z0, float Z1, std::initializer_list<FVector2D> Doors, float Head = 228.f);
/** The same with the doorways in an array (built at runtime). */
void PartitionDoors(FHallMesh& M, FName K, bool bAlongY, float At, float U0, float U1, float Z0, float Z1, const TArray<FVector2D>& Doors, float Head = 228.f);
void FloorSlab(FHallMesh& M, const FBox2D& R, float Top, float Thick, const TArray<FBox2D>& Holes, FName FloorKey, FName CeilKey, const FHallUV& FloorUV);
/** A painted cinderblock partition (no wainscot): like Partition, with hollow-metal frames (FrameKey) round the doorways and
 *  any Openings (U,Z boxes: pass-throughs, borrowed lights), and a black vinyl cove base on both faces. */
void BlockWall(FHallMesh& M, FName K, bool bAlongY, float At, float U0, float U1, float Z0, float Z1, const TArray<FVector2D>& Doors,
    float Head = 213.f, const TArray<FBox2D>& Openings = {}, FName FrameKey = TEXT("CI_FrameBrown"), const FHallUV& UV = UVPlain());
void Stair(FHallMesh& M, float X0, float X1, float YBottom, float YTop, float ZBottom, float ZTop);
void Balustrade(FHallMesh& M, const FVector2D& A, const FVector2D& B, float Z);
/** A 2x4 fluorescent troffer in the ceiling at C (its lens facing down); Key is the lens (CI_TrofferFlicker for a dying tube). */
void Troffer(FHallMesh& Glow, FHallMesh& Clutter, const FVector& C, bool bAlongX, FName Key = NAME_None);
void Table(FHallMesh& M, const FVector& C, float L, float W, float H = 76.f, bool bAlongX = true);
void Chair(FHallMesh& M, const FVector& C, const FVector& Facing);
void Carrel(FHallMesh& M, FHallMesh& Glow, const FVector& C, const FVector& Facing);
void Catalog(FHallMesh& M, const FVector& C, const FVector& Facing, float W = 150.f, float H = 135.f);
void Shelving(FHallMesh& Solid, FHallMesh& Books, const FVector& A, const FVector& Along, float L, float Z, float H = 214.f);
void HangingSign(FHallMesh& M, FName Key, const FVector& C, const FVector& Facing, float W, float H, float Ceiling);
void WallPlaque(FHallMesh& M, FName Key, const FVector& C, const FVector& Out, float W, float H);

// ---- 1986 fixtures the house has no mesh for (HouseCampusInteriorsFixtures.cpp); C is on the floor (or the wall centre for
// wall pieces), Facing / Out the way the front faces.
void CrtTelevision(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing, bool bConsole);
void Chalkboard(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Out, float W, float H, FName Writing);
void TabletDesk(FHallMesh& Solid, const FVector& C, const FVector& Facing);
void Lockers(FHallMesh& Solid, FHallMesh& Clutter, const FVector& A, const FVector& Along, int32 Count, const FVector& Out);
void VendingMachine(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing, bool bSnacks);
void PayPhone(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Out);
void MailboxWall(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Out, float W, float H);
void LabBench(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Along, float L);
void FumeHood(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Out);
/** Backglass: its painted game (an invented one; CI_Backglass, GOOSE CHASE, when None). */
void Pinball(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing, FName Backglass = NAME_None);
void Laundry(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing, bool bDryer);
void BulletinBoard(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Out, float W, float H, FName Pinned);
void ExitSign(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing);
void Radiator(FHallMesh& Solid, const FVector& C, const FVector& Out, float W);
void WaterFountain(FHallMesh& Solid, const FVector& C, const FVector& Out);
void AshUrn(FHallMesh& Solid, const FVector& C);
void OverheadProjector(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing);
void Terminal(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Facing);
void Typewriter(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Facing);
void DeskPhone(FHallMesh& Solid, const FVector& C, const FVector& Facing, bool bRotary);
void DirectoryBoard(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Out, FName Board);
void Boombox(FHallMesh& Solid, const FVector& C, const FVector& Facing);
void MiniFridge(FHallMesh& Solid, const FVector& C, const FVector& Facing);
void MilkCrateShelf(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, const FVector& Facing, int32 Seed);
void Helmet(FHallMesh& Solid, const FVector& C, const FVector& Facing);
void Streamer(FHallMesh& Clutter, const FVector& A, const FVector& B, bool bGold);
/** A wall clock on the wall at C (its face's centre) facing Out: the rim and the face. Its hands belong to the interiors
 *  actor, which turns them to the clock tower's time; while a building is being built its clocks are collected here. */
void WallClock(FHallMesh& Clutter, const FVector& C, const FVector& Out, float R = 16.f);
void SetClockSink(TArray<FTransform>* Sink);
/** Their materials (CI_CrtScreen, CI_Chalk*, CI_PoBoxes, CI_RideBoard, CI_Vending*, CI_ExitFace, CI_GreenScreen,
 *  CI_TrofferFlicker ...). Idempotent. Use CI_TrofferFlicker in place of CI_Troffer for a tube on its way out. */
void FixturesLook(UHouseBlackHallSubsystem* Look);

// ---- the house's furniture (HouseCampusInteriors::EKit)
/** A house mesh reused on campus: its asset in the basement library, the name the HUD uses, its mass, whether it blocks as
 *  decor, and FrontYaw: the yaw that turns the mesh's own front onto local +X (so a piece placed with yaw Y faces Y). */
struct FKitDef
{
    const TCHAR* Asset; const TCHAR* Name; float Mass; bool bCollide; float FrontYaw;
    /** The asset's folder under /Game/SeniorSendOff/House (nullptr: BasementLibrary), and its X/Y scale on campus (0: 1/1.8). */
    const TCHAR* Folder = nullptr;
    float ScaleXY = 0.f;
};
const FKitDef& KitDef(HouseCampusInteriors::EKit K);

/** Places kit pieces in a building's canonical frame from their real (campus-scale) bounds. Yaws are the usual ones
 *  (0 = +X, 90 = +Y) and say which way the piece's front faces. */
struct FKitter
{
    UCampusInteriorsSubsystem* Sub = nullptr;
    TArray<HouseCampusInteriors::FKitPlace>* Out = nullptr;
    /** The pose that puts the local point Anchor (of the scaled mesh) on Target, its front facing Yaw. */
    static FTransform Pose(HouseCampusInteriors::EKit K, const FVector& Target, float Yaw, const FVector& Anchor)
    {
        const FQuat Q = FRotator(0.f, Yaw + KitDef(K).FrontYaw, 0.f).Quaternion();
        return FTransform(Q, Target - Q.RotateVector(Anchor), HouseCampusInteriors::KitScale(K));
    }
    /** Stands K on the surface at Z, its footprint centred on C, its front facing Yaw. */
    void Floor(HouseCampusInteriors::EKit K, const FVector2D& C, float Z, float Yaw, bool bProp) const
    {
        const FBox B = Sub ? Sub->KitBounds(K) : FBox(ForceInit);
        if (!B.IsValid || !Out) return;
        Out->Add({ K, Pose(K, FVector(C, Z), Yaw, FVector(B.GetCenter().X, B.GetCenter().Y, B.Min.Z)), bProp });
    }
    /** Hangs K on a wall: its back flat on the wall at C (C.Z is its centre height), its front along OutDir. */
    void Wall(HouseCampusInteriors::EKit K, const FVector& C, const FVector& OutDir, bool bProp = false) const
    {
        const FBox B = Sub ? Sub->KitBounds(K) : FBox(ForceInit);
        if (!B.IsValid || !Out) return;
        // The mesh's own front, in its local space, and the back face's centre opposite it.
        const FVector Front = FRotator(0.f, -KitDef(K).FrontYaw, 0.f).RotateVector(FVector::ForwardVector);
        const FVector E = B.GetExtent();
        const FVector Back = B.GetCenter() - Front * (FMath::Abs(Front.X) * E.X + FMath::Abs(Front.Y) * E.Y);
        Out->Add({ K, Pose(K, C, FMath::RadiansToDegrees(FMath::Atan2(OutDir.Y, OutDir.X)), Back), bProp });
    }
    /** Fills a six-shelf case stood at C on Z facing Yaw (as Floor placed it) with the house's shelf rows: each row mesh is
     *  in the case's own frame and stands on a board, 2 cm up and then every 30 cm (the house's loose books, surveyed from
     *  its bookcases by Tools/claude/survey_campus_shelves.py, sit the same way). One or two rows a shelf, as many as fit
     *  the 118 cm inside; the lower five shelves, the top one clear. */
    void FillBookcase(const FVector2D& C, float Z, float Yaw, int32 Variety) const
    {
        if (!Sub || !Out) return;
        const FBox Case = Sub->KitBounds(HouseCampusInteriors::EKit::Bookcase);
        if (!Case.IsValid) return;
        const FTransform CaseT = Pose(HouseCampusInteriors::EKit::Bookcase, FVector(C, Z), Yaw, FVector(Case.GetCenter().X, Case.GetCenter().Y, Case.Min.Z));
        for (int32 Shelf = 0; Shelf < 5; ++Shelf)
        {
            float Used = 0.f;   // mesh units along the case's x, past the rows already on this shelf
            for (int32 Try = 0; Try < 2; ++Try)
            {
                const HouseCampusInteriors::EKit Row = static_cast<HouseCampusInteriors::EKit>(static_cast<int32>(HouseCampusInteriors::EKit::Books0) + (Variety * 3 + Shelf * 2 + Try * 4) % 5);
                const FBox RB = Sub->KitBounds(Row);
                if (!RB.IsValid) break;
                const float W = (RB.Max.X - RB.Min.X) / HouseCampusInteriors::KitScale(Row).X;
                if (Used + W > 118.f) break;
                const FVector Local(Used, 0.f, 2.f + 29.95f * Shelf);
                Out->Add({ Row, FTransform(CaseT.GetRotation(), CaseT.TransformPosition(Local), CaseT.GetScale3D()), false });
                Used += W + 1.5f;
            }
        }
    }
    /** K's height and depth (campus scale), for stacking and wall clearance. */
    float Height(HouseCampusInteriors::EKit K) const { const FBox B = Sub ? Sub->KitBounds(K) : FBox(ForceInit); return B.IsValid ? B.Max.Z - B.Min.Z : 0.f; }
};


// ---- whole furnished rooms (HouseCampusInteriorsRoomKits.cpp). R is the room's inside rect (canonical), Z its floor,
// Front the side the board, counter or TV is on (0 +Y, 1 -Y, 2 -X, 3 +X); each keeps a clear way in from the far side.
void RoomClassroom(FHallMesh& Solid, FHallMesh& Clutter, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, FName Board, int32 Seed);
void RoomOffice(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, int32 Seed);
void RoomLounge(FHallMesh& Solid, FHallMesh& Glow, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, int32 Seed);
void RoomDining(FHallMesh& Solid, FHallMesh& Glow, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, int32 Seed);
void RoomSeminar(FHallMesh& Solid, FHallMesh& Clutter, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, FName Board, int32 Seed);
void RoomReception(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, FName Sign, int32 Seed);
void RoomWaiting(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, int32 Seed);
void TrophyCase(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Out, float W);

// ---- the registry
/** A door an interior asks for, in its building's canonical frame (an ACampusDoor; the server spawns it). Reason empty =
 *  unlocked; otherwise the HUD's "Locked: <Reason>." */
struct FDoorSpec
{
    FName Name; FVector Hinge; float Yaw; EHallDoorStyle Style; float Width; float RoomSign; uint8 Flags; FVector Center;
    FName Group; FString Reason;
    /** It stands open when the building loads (a dorm room with its door propped). */
    bool bOpen = false;
    /** The leaves' height (the clock tower's pair is 255, to its doorway's springing). */
    float Height = 213.f;
};
/** A sound an interior makes, at a place in its canonical frame: 0 a dripping tap, 1 a TV crowd, 2 the postgame radio show
 *  (synthesised: HouseCampusInteriorsSound.cpp). */
struct FSoundSpot { int32 Kind = 0; FVector At = FVector::ZeroVector; };
/** The sounds' looping PCM (mono, SoundRate); kind 3 is the room tone inside every building (HVAC, fluorescent buzz). */
constexpr int32 SoundRate = 32000;
void SoundPcm(int32 Kind, TArray<int16>& Out);
/** A walk-in building. */
struct FInteriorDef
{
    const TCHAR* Id;
    int32 OpenFloors;
    /** The inside: plaster skin, floors, rooms, fittings (Solid collides; Clutter and Glow do not) and the house kit. */
    void (*Build)(int32 Seed, float Height, FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit);
    /** Where the pooled interior lights may stand (canonical); the nearest ones to the view are lit. */
    void (*Anchors)(int32 Seed, float Height, TArray<FVector>& Out);
    /** Its door openings in the outer walls (the shell cuts them; the main ones first). */
    void (*Openings)(int32 Seed, float Height, TArray<HouseCampusInteriors::FOpening>& Out);
    /** Its doors, outer and inner. */
    void (*Doors)(int32 Seed, float Height, TArray<FDoorSpec>& Out);
    /** Its own painted signs and finishes (CI_*), added to the shared look once. */
    void (*Look)(UHouseBlackHallSubsystem* Look);
    /** A free-standing interior (not a plan block: the stadium, the clock tower) works in world space (Frame is identity,
     *  Height 0): its world bounds (streaming), the precise indoor test if it is not a box, and the walking height of its
     *  floor K (the review). All nullptr for a plan block. */
    FBox (*Bounds)() = nullptr;
    bool (*Inside)(const FVector& World) = nullptr;
    float (*FloorZ)(int32 Floor) = nullptr;
    /** A plan block whose open floors are only partly open (a Tier C lobby): whether the window column U0..U1 on Side of
     *  floor K is real glass into a built room. A closed one stays solid wall with an interior-mapped window, like the
     *  closed floors. nullptr: every window of an open floor is real. */
    bool (*WindowOpen)(int32 Seed, int32 Side, int32 Floor, float U0, float U1) = nullptr;
    /** Its sounds (2e). */
    void (*Sounds)(int32 Seed, float Height, TArray<FSoundSpot>& Out) = nullptr;
    /** Collision only, never drawn: a hidden part of its own, built and freed with the rest (the clock tower's belfry
     *  guards and its well's jump stops). */
    void (*Blockers)(int32 Seed, float Height, FHallMesh& Out) = nullptr;
    /** Its pooled lights are only for a view inside it: a view outside never takes its anchors, and it never stands in for
     *  a farther building that would lend a view outside three of its ground-floor anchors (the clock tower, whose box
     *  takes in the open belfry and the plinth round the shaft). */
    bool bLightsInsideOnly = false;
    /** Anchor's height above its floor when classifying pooled lights. Older definitions retain300cm. */
    float LightAnchorFloorOffset = 300.f;
    /** Human interiors opt in; wandering wildlife outside does not keep these rooms loaded. */
    bool bPlayerProximityOnly = false;
};
const TArray<FInteriorDef>& Interiors();
const FInteriorDef* InteriorFor(int32 Seed);
/** The glazed oak French pair in a main opening on side 0 (the hinge lines 1 cm inside the inner face). */
void MainDoorPair(const TCHAR* Id, const FBox2D& Canon, const HouseCampusInteriors::FOpening& O, TArray<FDoorSpec>& Out);
/** A single steel door in an opening on side 1 (the back), locked with Reason. */
void BackDoor(const TCHAR* Id, const TCHAR* Name, const FBox2D& Canon, const HouseCampusInteriors::FOpening& O, const TCHAR* Reason, TArray<FDoorSpec>& Out);
/** A glazed oak pair in an outer-wall opening on any side (side 0 is MainDoorPair's), hinged 1 cm inside the inner face,
 *  swinging in; its leaves are <Prefix>A and <Prefix>B. Reason empty = unlocked. */
void SidePair(const TCHAR* Id, const TCHAR* Prefix, const FBox2D& Canon, const HouseCampusInteriors::FOpening& O, const FString& Reason, TArray<FDoorSpec>& Out);
/** A door filling the doorway U0..U1 of a partition (along X at At, or along Y when bAlongY) on floor Z, hinged on the U0
 *  jamb and swinging into the side RoomDir (+1: toward +Y, or +X; -1 the other way). Reason empty = unlocked. */
void InnerDoor(TArray<FDoorSpec>& Out, FName Name, bool bAlongY, float At, float U0, float U1, float Z, float RoomDir, EHallDoorStyle Style,
    uint8 Flags, const FString& Reason, bool bOpen = false);

// ---- small things from Harlow Hall the other buildings use too (HouseCampusInteriorsHarlow.cpp)
void HarPoster(FHallMesh& M, FName Key, const FVector& C, const FVector& Out, float W, float H);
void HarPennant(FHallMesh& M, FName Key, const FVector& C, const FVector& Out, float Len, float H);
void HarPizzaBox(FHallMesh& M, const FVector& C, float Yaw);
void HarCan(FHallMesh& M, const FVector& C, bool bRed, bool bDown, float Yaw);
void HarTissueBox(FHallMesh& M, const FVector& C, int32 Seed);
void HarFootball(FHallMesh& M, const FVector& C, const FVector& Along);
/** A student room's own frame: A across from its west wall, B in from its door wall toward its window. */
struct FHarFrame
{
    float XL = 0.f, XR = 0.f, YDoor = 0.f, Dir = 1.f, Depth = 0.f;
    FVector2D P(float A, float B) const { return FVector2D(XL + A, YDoor + Dir * B); }
    FVector V(float A, float B, float Z) const { return FVector(P(A, B), Z); }
    FVector Dv(float DA, float DB) const { return FVector(DA, Dir * DB, 0.f).GetSafeNormal(); }
    float Yaw(float DA, float DB) const { return FMath::RadiansToDegrees(FMath::Atan2(Dir * DB, DA)); }
    float W() const { return XR - XL; }
};
/** A furnished dorm room in frame F (its door 22..115 in from A=0 on the B=0 wall, its window centred at WinA on the far
 *  wall): the bunk, desks, dresser, crates, fridge, posters, pennant, and its occupants' things by Style (0..5). */
void HarFurnish(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, const FHarFrame& F, float Z, float WinA, int32 Style, int32 Number);
/** A board painted with lines of text down it (a directory, a list), LineH pixels a line (HouseCampusInteriorsScience.cpp). */
void SciPaintLines(UHouseBlackHallSubsystem* Look, FName Key, int32 W, int32 H, const FLinearColor& Bg, const FLinearColor& Ink, std::initializer_list<const TCHAR*> Lines, float LineH, const FLinearColor& Emission = FLinearColor::Black);

// Each walk-in building's definition, in its own file.
FInteriorDef LibraryDef();
FInteriorDef HarlowDef();
FInteriorDef ScienceDef();
FInteriorDef UnionDef();
FInteriorDef StadiumDef();
FInteriorDef TowerDef();
/** Tier B and C (HouseCampusInteriorsGeneric.cpp): every one of them, from its plan spec. */
void GenericDefs(TArray<FInteriorDef>& Out);
void RowDefs(TArray<FInteriorDef>& Out);
}
