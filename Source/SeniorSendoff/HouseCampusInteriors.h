// Calder State's walk-in buildings (master job, Phase 2), built the way the house's own runtime rooms are (the black hall
// and the hallway detail): a world subsystem, a transient cosmetic actor that every machine builds the same way, and
// replicated gameplay actors (doors) that only the server spawns.
//
// - The outside of a walk-in building (walls with real window and door openings, the closed upper floors, cornice and
//   entrance) is part of the campus exterior (HouseExteriorCampus ShellBuilding), so it looks the same from the street.
// - The inside (plaster, floors, ceilings, stairs, partitions, fittings and furniture) belongs to AHouseCampusInteriors,
//   on lighting channel 1 so the shadowless street lights never leak through a wall. It is built when someone comes
//   near and freed when everyone has gone. A pool of at most 6 shadowless interior lights follows the local view inside.
// - Doors are ACampusDoor, the black hall's procedural door with a campus ID; a locked one carries the reason the HUD
//   shows.
// -NoCampusInteriors gives back the solid blocks. Everything is built at runtime; nothing is saved into the map.
#pragma once

#include "CoreMinimal.h"
#include "HouseBlackHallMesh.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "HouseBlackHall.h"
#include "HouseInteractiveProp.h"
#include "HouseCampusInteriors.generated.h"

struct FHallMesh;
struct FHallUV;
class UHouseBlackHallSubsystem;
class UProceduralMeshComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UAudioComponent;
class USoundWaveProcedural;
class USoundAttenuation;

namespace HouseCampusInteriors
{
bool Enabled();
/** Walk-in floors of a plan building (by seed, the index into HouseCampusLayout::Blocks()); 0 keeps it a solid block. */
int32 OpenFloors(int32 Seed);
/** The exterior window grid's floors and storey height, and the walking surface of floor K (0 = ground). */
int32 Floors(float Height);
float Story(float Height);
float FloorTop(float Height, int32 K);
/** Where the closed upper volume starts: the ceiling over the top open floor. */
float OpenTop(int32 Seed);
/** The building's own frame: its footprint centred on the origin with the main entrance on +Y, and that frame's placement. */
FBox2D CanonRect(int32 Seed);
FTransform Frame(int32 Seed);
/** An opening in an outer wall of the canonical frame. Side 0 = +Y (main), 1 = -Y, 2 = -X, 3 = +X. U runs along the wall's
 *  own axis (X for sides 0 and 1, Y for 2 and 3); Z is the canonical frame's height (today's grade at -60). */
struct FOpening { int32 Side; float U0, U1, Z0, Z1; };
void DoorOpenings(int32 Seed, TArray<FOpening>& Out);
/** The plan face (N, S, E or W) has a real door opening of a walk-in building (the exterior then builds no dummy entrance). */
bool RealDoorOn(int32 Seed, TCHAR Face);
/** The exterior grid's window columns on a side (U0..U1), and each window's bottom on floor K. */
void WindowColumns(int32 Seed, int32 Side, TArray<FVector2D>& Out);
float WindowSill(float Height, int32 K);
constexpr float WindowHeight = 212.f;
/** A door opening takes this window's place (it is neither cut nor drawn). */
bool WindowReplaced(int32 Seed, int32 Side, int32 K, float U0, float U1);
/** This open floor's window is real glass into a built room (false: wall, with an interior-mapped window). */
bool WindowOpen(int32 Seed, int32 Side, int32 K, float U0, float U1);
/** One skin of the open floors' outer walls, Depth0..Depth1 cm in from the outer face, with every open window and door
 *  cut out (canonical frame). The exterior builds the outer skin, the interiors the plaster one. */
void OpenWalls(FHallMesh& M, FName Key, int32 Seed, float Depth0, float Depth1, const FHallUV& UV);
/** How far P is from the inside of the nearest walk-in building (0 inside): the indoor test for exposure and sound. */
float DistanceOutside(const FVector& P);
/** Seeds: a plan block's index, or FreeSeedBase + n for the n-th entry of the registry when it is free-standing (the
 *  stadium, the clock tower: not a plan block, world space). */
constexpr int32 FreeSeedBase = 1000;
/** Every walk-in interior's seed (enabled ones only), the seed of an interior by its Id, its world box (streaming) and the
 *  indoor test against one interior. */
void InteriorSeeds(TArray<int32>& Out);
int32 SeedFor(const TCHAR* Id);
FBox InteriorBox(int32 Seed);
bool InsideInterior(int32 Seed, const FVector& P);
/** The walking height of floor K of an interior in the world (plan block: FloorTop on its pad; free-standing: its own). */
float InteriorFloorZ(int32 Seed, int32 K);
/** OpenTop in the world (on the building's pad), and how far an interior stands above today's grade: a plan block's pad,
 *  the clock tower's knoll lift, 0 for the stadium (the Calder terrain; all 0 with it off). FloorTop and OpenTop stay in
 *  the building's own (canonical) frame. */
float OpenTopWorld(int32 Seed);
float BaseDZ(int32 Seed);
const TCHAR* InteriorId(int32 Seed);
/** Materials (CI_*). Idempotent. */
void CreateLook(UHouseBlackHallSubsystem* Look);
/** The closed floors' windows show interior-mapped rooms (CI_Room<n>Lit/Dark; M_CampusInteriorWindow is loaded and the
 *  interiors are on). The room a window shows, 0..3: a dorm room, an office, a classroom, a lounge. */
bool RoomWindows();
int32 WindowRoom(int32 Seed, int32 Side, int32 Column, int32 Floor);

/** The walk-in stadium (HouseCampusInteriorsStadium.cpp). The exterior builds the bowl (HouseExteriorCampus); while the
 *  stadium is walk-in it asks these which of its pieces to cut or build open. K is a bowl segment (0..39, 9 degrees each,
 *  anticlockwise from +X), Row a seat row (0..11). */
bool StadiumOpen();
/** The walk-in clock tower (HouseCampusInteriorsTower.cpp, "TWR"): while its inside is open (the interiors on, the tower
 *  there) the tower's shell is built open for it (HouseClockTowerMesh: frames for the plates across its shaft, leaves for
 *  its door, a slot in its belfry floor), the same from outside. */
bool ClockTowerOpen();
/** A working gate: the segment's outer wall is cut for walking (GATE 4, GATE 5). */
bool StadiumGate(int32 K);
/** The concourse is behind this segment's outer wall (its window band is lit). */
bool StadiumConcourse(int32 K);
/** This seat row of this segment is cut away for the vomitory and its landing. */
bool StadiumVomitory(int32 K, int32 Row);
/** The players' tunnel goes through this segment's podium wall. */
bool StadiumTunnel(int32 K);
/** In place of the solid pieces: a gate segment's outer wall with its opening, the stone arch round it and the grille half
 *  up in the lunette (A0, B0 the segment's ends at the ground, N its outward normal); the podium wall round the tunnel's
 *  mouth (facing the field, NIn); the hollow press box; and the brick stair tower that climbs to it. Exterior meshes. */
void StadiumGateWall(FHallMesh& M, int32 K, const FVector& A0, const FVector& B0, const FVector& N, bool bWindow = false);
/** The window over each segment's arch (fractions along the segment, heights over the ground): the concourse's glazing,
 *  and with the exterior polish on (HouseCampusPolish) a real opening in the outer wall of every segment (bWindow above). */
constexpr float StadiumWinT0 = .32f, StadiumWinT1 = .68f, StadiumWinZ0 = 450.f, StadiumWinZ1 = 660.f;
void StadiumTunnelWall(FHallMesh& M, int32 K, const FVector& A0, const FVector& B0, const FVector& NIn, float Top);
void StadiumPressShell(FHallMesh& M);
void StadiumTower(FHallMesh& M);
/** A point of the bowl: at Degrees round it, R of the way out (1 = the outer wall), on the ground. */
FVector2D StadiumPoint(float Degrees, float R);

/** A walk through a Tier B or C building for the interiors review, worked out from its plan (canonical frame): Kind 0 walk
 *  there, 1 try the locked door named Door from there. Shot names a screenshot to take there (empty: none). Nothing for
 *  other buildings. */
struct FTourStop { FVector2D At; int32 Kind = 0; FString Name, Door, Shot; float LookYaw = 0.f; };
void GenericTour(int32 Seed, TArray<FTourStop>& Out);
bool IsGeneric(int32 Seed);

/** The house's own furniture, reused inside the campus buildings (the basement library's meshes, materials and damage
 *  stages). The order is replicated (ACampusProp::KitIndex): only add at the end. */
enum class EKit : uint8
{
    Bookcase, Books0, Books1, Books2, Books3, Books4, StudyTable, DeskChair, ClubChair, Loveseat, TableLamp, FloorLamp,
    Pendant, LongRug, StudyRug, EntryRug, HistoricPhoto, GroupPhoto, Lighthouse, StackedBooks, PaperStack, RedBook,
    PaperCup, Backpack, PlantStand, TallGrass, SmallPlant, SideTable, Worktable, Cardboard, Console, Umbrella, Armchair,
    ShelfPhoto,
    // 2c: from the other house rooms (BoywickBedroom, ReferenceKitchen, NoodleWing) and more of the library.
    LoftFrame, QuiltedMattress, WritingDesk, Dresser, WireRack, StorageBox, GreyFuton, GreyPillow, PatternedPillow,
    GreenSpindleChair, BarStool, DiningTable, ServingCounter, TrashCan, BilliardTable, CueRack, Arcade, LeatherSofa,
    TanRecliner, TVConsole, FireExtinguisher, OctagonalCardTable, GrayOfficeChair, CoffeeChest, SpindleChair,
    // 2c: the dorm rooms' things (BoywickBedroom).
    ChairJacket, LooseShirt, BunchedTowel, DrapedTowel, Sneaker, StripedRug, TrashBin, LaundryBasket, DeskLamp, LowDaybed,
    HangingLaundryBag,
    Count
};
/** Most house meshes are modelled 1.8x wide (the house's own proportions); on campus they are scaled back by this in X and
 *  Y. A few (kitchen chairs, true-size pieces) are 1:1; KitScale says which. */
constexpr float KitScaleXY = 1.f / 1.8f;
FVector KitScale(EKit Kit);
/** One piece of the kit in a building's canonical frame: decor (instanced, fixed) or a prop (ACampusProp, server-spawned). */
struct FKitPlace { EKit Kit; FTransform T; bool bProp; };
}

/** A campus door: the black hall's procedural door with a campus ID ("Campus_<building>_<name>"). A locked one carries the
 *  reason the HUD shows (STAFF ONLY, CLOSED SATURDAY ...), replicated with it. */
UCLASS()
class SENIORSENDOFF_API ACampusDoor : public AHouseBlackHallDoor
{
    GENERATED_BODY()
public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    /** Puts the leaf on the interior lighting channel (and the street's too, for a door in an outer wall). */
    virtual void BeginPlay() override;
    UPROPERTY(Replicated) FString LockedReason;
    /** In a building's outer wall (part of its facade), or inside it. */
    bool bOuterDoor = false;
    /** An inner door is drawn only while its building's inside is built on this machine (master job, Phase 4): the 161 doors
     *  stand all the time, and an inner one is otherwise only work for the renderer. Its collision stays. Only what this
     *  hid is shown again. */
    void ShowForBuilt(bool bBuilt);
private:
    TArray<TWeakObjectPtr<UPrimitiveComponent>> HiddenUnbuilt;
};

/** A campus furnishing made from one of the house's own prop meshes, with its damage stages: it moves, carries and breaks
 *  like the house's furniture. The server spawns it when a walk-in building's inside is built and removes it when that is
 *  freed; only the kit index replicates and every machine picks the same meshes. Its breaks are not house damage
 *  (SSOCampusDamage). */
UCLASS(NotPlaceable)
class SENIORSENDOFF_API ACampusProp : public AHouseInteractiveProp
{
    GENERATED_BODY()
public:
    ACampusProp();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    /** Assigns the kit meshes and tuning for KitIndex (server before FinishSpawning, clients in BeginPlay). */
    void ApplyKit();
    UPROPERTY(Replicated) uint8 KitIndex = 0;
    /** The building it belongs to and its place in that building's kit list (server only): a rebuild never respawns a
     *  place whose prop is still about. */
    int32 Seed = -1;
    int32 Slot = -1;
};

/** Builds the insides of the walk-in buildings near any local view or pawn, frees them when everyone has gone, and runs the
 *  pooled interior lights. Transient, not replicated, the same on every machine. */
UCLASS(NotPlaceable, Transient)
class SENIORSENDOFF_API AHouseCampusInteriors : public AActor
{
    GENERATED_BODY()
public:
    AHouseCampusInteriors();
    virtual void Tick(float DeltaSeconds) override;
    /** Seeds of the buildings whose insides are built right now. */
    TArray<int32> Loaded() const;
    void BuildNow(int32 Seed);
    /** Parts of this building still waiting to be drawn as static meshes. */
    int32 PendingConversions(int32 Seed) const;
    void Free(int32 Seed);
    /** Pooled interior lights switched on right now (at most 6). */
    int32 ActiveLights() const;
    static constexpr int32 LightPool = 6;
    /** Building Seed's wall clocks against the clock tower's time: their count, and the worst hand's error in degrees (-1
     *  while it is not built, or built less than 2 s ago and its hands not yet set). */
    float ClockError(int32 Seed, int32& OutClocks) const;
    /** What building Seed is built of right now (false while it is not built): its mesh parts and triangles, the house kit
     *  pieces it placed, its wall clocks and their hands, its sounds; and its solid parts that have collision. */
    struct FStats { int32 Parts = 0, Triangles = 0, Decor = 0, Clocks = 0, Hands = 0, Sounds = 0, PlayingSounds = 0, DrawnParts = 0, SolidParts = 0, CollidingParts = 0; TArray<const UMaterialInterface*> Materials; };
    bool Stats(int32 Seed, FStats& Out) const;
private:
    struct FBuilt
    {
        int32 Seed = -1; double LastNear = 0.0, BuiltAt = 0.0; int32 Triangles = 0, Decor = 0;
        TArray<TObjectPtr<UProceduralMeshComponent>> Parts;
        /** The parts as runtime static meshes, made a tick or so after the build (ConvertPending); the procedural part is
         *  then hidden and keeps only its collision. */
        TArray<TObjectPtr<UStaticMeshComponent>> Drawn;
        TArray<TObjectPtr<UStaticMeshComponent>> Kit;
        TArray<TWeakObjectPtr<ACampusProp>> Props;
        /** Its wall clocks (world) and their hands, two a clock (hour, minute); its sounds. */
        TArray<FTransform> Clocks;
        TArray<TObjectPtr<UProceduralMeshComponent>> Hands;
        TArray<TObjectPtr<UAudioComponent>> Sounds;
    };
    TArray<FBuilt> Built;
    /** Parts waiting to be drawn as static meshes, one converted a tick (so a building's arrival is not one long frame). */
    /** A piece of a part (at most 12,000 triangles, so no conversion is a long frame); the part's procedural mesh is hidden
     *  when its last piece is drawn. */
    struct FPendingStatic { int32 Seed = -1; TWeakObjectPtr<UProceduralMeshComponent> Part; FHallMesh Mesh; bool bLast = false; };
    TArray<FPendingStatic> PendingStatic;
    double NextConvert = 0.0;
    void ConvertPending();
    /** Shows (or hides) a building's inner doors with its inside. */
    void ShowDoors(int32 Seed, bool bBuilt);
    /** Props carried out of their building (or held) when it was freed: they stay where they are (server). */
    TArray<TWeakObjectPtr<ACampusProp>> Orphans;
    /** Things inside a walk-in building are lit by its pool (channel 1) only; outside, by the street (channel 0). Pawns,
     *  the house's props and campus props switch as they go in and out. */
    void UpdateIndoorLighting();
    /** Another physics body (a house prop, someone else's campus prop) rests inside: freeing the floors would drop it. */
    bool FreeWouldDrop(int32 Seed) const;
    UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;
    double NextCheck = 0.0;
    void UpdateLights(const FVector& View);
    /** The wall clocks' hands, to the clock tower's time (once a second). */
    void UpdateClocks();
    double NextClock = 0.0;
    // The tower's time the hands were last set to (radians, as ClockHandAngles gives them) and when, in world time.
    float ShownHourA = 0.f, ShownMinuteA = 0.f;
    double ShownAt = -1.0;
    /** Sound (2e): the room tone while the view is inside, the buildings' own sounds while they are built, the drumline
     *  muffled indoors. Every sound is a looping procedural wave fed from its synthesised PCM. */
    bool SoundOn() const;
    UAudioComponent* StartSound(int32 Kind, const FVector& At, bool bSpatial);
    void UpdateSound(const FVector& View, float DeltaSeconds);
    struct FFeed { TWeakObjectPtr<UAudioComponent> Comp; TWeakObjectPtr<USoundWaveProcedural> Wave; int32 Kind = 0; int32 Cursor = 0; int64 Queued = 0; bool bHeard = false; };
    TArray<FFeed> Feeds;
    TArray<TArray<int16>> Pcm;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Ambience;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> SoundFalloff;
    float AmbienceLevel = 0.f;
    int32 IndoorState = -1;
    /** -CampusSoundRecord=<seconds> (development builds): the mix recorded once the view stands by a sound. */
    double RecordUntil = -1.0;
    bool bRecorded = false;
public:
    int32 SoundsStarted = 0, ClocksBuilt = 0;
};

UCLASS()
class SENIORSENDOFF_API UCampusInteriorsSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    AHouseCampusInteriors* GetInteriors() const { return Interiors.Get(); }
    const TArray<TWeakObjectPtr<ACampusDoor>>& GetDoors() const { return Doors; }
    /** A kit mesh: Stage 0 intact, 1 damaged, 2 heavily damaged, 3 broken (null where the house has no such stage). */
    UStaticMesh* KitMesh(HouseCampusInteriors::EKit Kit, int32 Stage = 0);
    /** The intact mesh's local bounds at campus scale. */
    FBox KitBounds(HouseCampusInteriors::EKit Kit);
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> KitMeshes;
    bool bKitLoaded = false;
    void LoadKit();
    void SpawnDoors(UWorld& World);
    TWeakObjectPtr<AHouseCampusInteriors> Interiors;
    TArray<TWeakObjectPtr<ACampusDoor>> Doors;
};
