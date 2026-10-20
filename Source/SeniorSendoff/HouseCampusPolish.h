// Calder State, Phase 3 of the master job: exterior polish at eye level. The stadium's facade, openings, structure and
// apron (3a); each campus building's character and the ground (3b); landmarks, homecoming dressing and street furniture
// (3c); light pools and life (3d). Built at runtime by a transient actor beside the exterior, the same on every machine,
// with no lights (glow is emission and emissive decals). Off with -NoCampusPolish (or -NoCampusLayout).
//
// Each area is its own file (HouseCampusPolish<Area>.cpp) and adds to FParts:
//   Solid  - collides (BlockAll, camera ignored), drawn, no shadow; things you walk into.
//   Detail - drawn, no collision, culled beyond 90 m (trim, signs, litter, grass tufts).
//   Far    - drawn, no collision, never culled (roofs, the stadium's facade: what makes the skyline).
//   Glow   - emissive, no collision (lit signs, lenses, embers).
//   Ground - drawn, no collision, culled beyond 90 m: what lies everywhere on the ground (walk joints and cracks, leaves,
//            mulch rings), in large tiles so its few materials are not repeated in every small one.
//   Lawn   - the mown stripes, in large tiles, culled beyond 160 m.
//   Skyline - the town of Calder and the ridges behind it: drawn, never culled, in tiles 1 km square.
//   Pools  - emissive ground decals (M_LightPoolDecal): the light a lamp, a window or a fire would throw.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Async/Future.h"
#include "HouseBlackHallMesh.h"
#include "HouseCampusPolish.generated.h"

class UDecalComponent;
class UStaticMeshComponent;
class UHouseBlackHallSubsystem;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;

namespace HouseCampusPolish
{
bool Enabled();
/** An emissive light pool on the ground: centre (on the ground), radius, colour and strength (1 = a street lamp's). */
struct FPool { FVector At; float Radius; FLinearColor Tint; float Strength; };
/** A parked car of the exterior's (read only): its transform and its mesh's local bounds (nose along local +X). */
struct FCar { FTransform T; FBox Box; };
/** A soft camera-facing glow or a puff (HouseCampusFall's halo material): haze, smoke. */
struct FPuff { FVector At; float Size; FLinearColor Color; int32 Kind = 0; };   // Kind 0 still haze, 1 drifting smoke
/** Cloth that moves in the wind: a flag or a streamer, a strip of quads hanging from (or tied at) its edge. */
struct FFlutter { FVector Root; FVector Along; float Length, Width; FName Key; bool bHanging = true; };
struct FParts
{
    FHallMesh Solid, Detail, Glow;
    /** Drawn, no collision, never culled by distance (roofs, the stadium's facade: the skyline). */
    FHallMesh Far;
    /** The campus lawns' mown stripes: drawn, no collision, culled beyond 160 m (the terrain's own grass shows past that). */
    FHallMesh Lawn;
    /** What lies everywhere on the ground (joints, cracks, leaves, mulch): drawn, no collision, culled beyond 90 m. Its own
     *  part in large tiles: in Detail's small ones its handful of materials became a draw in every tile. */
    FHallMesh Ground;
    /** The town of Calder and the ridges behind it (HouseCampusPolishTown.cpp): drawn, never culled, in huge tiles. */
    FHallMesh Skyline;
    TArray<FPool> Pools;
    TArray<FPuff> Puffs;
    TArray<FFlutter> Flutters;
    /** Where the exterior's trees stand (X, Y, scale) and its parked cars: read only, never moved. */
    TArray<FVector> Trees;
    TArray<FCar> Cars;
    /** Ground spots a solid prop must keep off (review routes, doors, gates): centre, radius. */
    TArray<FVector> KeepClear;
    bool Clear(const FVector2D& P, float Radius) const;
};
/** With the polish on, the stadium's outer wall on a segment that is not a gate: the brick with the window opening (the
 *  exterior calls this in place of its solid panel; HouseCampusPolishStadium dresses the opening). */
void StadiumWall(FHallMesh& M, int32 K, const FVector& A0, const FVector& B0);
/** The outdoor soundscape's sounds, synthesised (HouseCampusPolishSound.cpp): 0 crowd murmur, 1 wind, 2 crickets, 3 a bus
 *  idling (loops); 4 a car horn honking for the Geese, 5 a car door (one-shots). Mono at SoundRate. */
constexpr int32 SoundRate = 32000;
void SoundPcm(int32 Kind, TArray<int16>& Out);
/** The three flood lamp banks: the centre of the bank's lenses, the way it faces (down toward the field) and its lenses. */
struct FFloodBank { FVector Centre, Facing; TArray<FVector> Lenses; };
void FloodBanks(TArray<FFloodBank>& Out);
// The areas.
void Stadium(FParts& P);
void Buildings(FParts& P);
void Ground(FParts& P);
void Landmarks(FParts& P);
void StreetFurniture(FParts& P);
void LightPools(FParts& P);
void Town(FParts& P);
/** The areas' materials (CP_*). Idempotent. */
void CreateLook(UHouseBlackHallSubsystem* Look);
void StadiumLook(UHouseBlackHallSubsystem* Look);
void BuildingsLook(UHouseBlackHallSubsystem* Look);
void GroundLook(UHouseBlackHallSubsystem* Look);
void LandmarksLook(UHouseBlackHallSubsystem* Look);
void StreetFurnitureLook(UHouseBlackHallSubsystem* Look);
void TownLook(UHouseBlackHallSubsystem* Look);
}

/** Builds the polish parts once, beside the exterior, and runs their small motions and the outdoor soundscape. */
UCLASS(NotPlaceable, Transient)
class SENIORSENDOFF_API AHouseCampusPolish : public AActor
{
    GENERATED_BODY()
public:
    AHouseCampusPolish();
    virtual void Tick(float DeltaSeconds) override;
    static AHouseCampusPolish* SpawnFor(AActor* Exterior, UHouseBlackHallSubsystem* Look);
    int32 Triangles = 0, Sections = 0, PoolCount = 0, PuffCount = 0, FlutterCount = 0, SoundsStarted = 0;
private:
    void Build(UHouseBlackHallSubsystem* Look, const TArray<FVector>& Trees, const TArray<HouseCampusPolish::FCar>& Cars);
    void MakePart(const FHallMesh& M, const TCHAR* Name, bool bCollision, float CullDistance, UHouseBlackHallSubsystem* Look, float Tile, float SpreadTile = 0.f);
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;   // the solid part's collision (not drawn)
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Drawn;        // every part's tiles, as runtime static meshes
    int32 Components = 0;
    UPROPERTY(Transient) TArray<TObjectPtr<UDecalComponent>> PoolDecals;
    // Step 3d: haze and smoke (camera-facing puffs), flags and streamers in the wind, the outdoor soundscape.
    void BuildPuffs(UHouseBlackHallSubsystem* Look, const TArray<HouseCampusPolish::FPuff>& In);
    void UpdatePuffs(const FVector& Camera, const FRotator& CameraRotation, float Time, float Cycle);
    void BuildFlutters(UHouseBlackHallSubsystem* Look, const TArray<HouseCampusPolish::FFlutter>& In);
    void UpdateFlutters(float Time);
    TArray<HouseCampusPolish::FPuff> PuffSpots;
    TArray<HouseCampusPolish::FFlutter> FlutterSpots;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> PuffMesh;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> FlutterMesh;
    TArray<FVector> PuffPos, PuffNrm;
    TArray<FLinearColor> PuffCol;
    TMap<FName, int32> FlutterSection;
    TMap<FName, TArray<int32>> FlutterByKey;   // the flutters drawn by each section, fixed at build
    TArray<TArray<FVector>> FlutterPos, FlutterNrm;
    bool SoundOn() const;
    class UAudioComponent* StartSound(int32 Kind, const FVector& At, bool bSpatial, bool bLoop, float Volume);
    void UpdateSound(const FVector& View, float DeltaSeconds);
    struct FFeed { TWeakObjectPtr<class UAudioComponent> Comp; TWeakObjectPtr<class USoundWaveProcedural> Wave; int32 Kind = 0; int32 Cursor = 0; bool bLoop = true; float Base = 1.f, Level = -1.f; double Until = 0.0; };
    TArray<FFeed> Feeds;
    TArray<TArray<int16>> Pcm;
    TFuture<TArray<TArray<int16>>> PcmTask;   // the loops, synthesised off the game thread
    UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> NearFalloff;
    UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> FarFalloff;
    UPROPERTY(Transient) TObjectPtr<class UAudioComponent> Wind;
    UPROPERTY(Transient) TObjectPtr<class UAudioComponent> Crickets;
    float CampusLevel = 0.f, IndoorLevel = 1.f;
    double NextHonk = 0.0, NextDoor = 0.0, Started = -1.0;
    bool bLoopsStarted = false;
    FRandomStream SoundRng = FRandomStream(1986);
};
