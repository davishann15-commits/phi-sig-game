// Homecoming weekend on the front campus, Saturday October 18 1986, at dusk: the stadium as a college football ground
// for the (invented) Calder State Fighting Geese in purple and gold, the bulb scoreboard and the after-game mess, the
// gate dressed for homecoming, and the campus around it in peak autumn (dorm windows, sheets, jack-o'-lanterns, the
// front green with its kiosk, bikes, leaf piles and a smouldering pep-rally bonfire). Built at runtime beside the
// exterior; nothing is saved into the map and no lights are added (the three stadium lights only reach further).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "Async/Future.h"
#include "ProceduralMeshComponent.h"
#include "HouseCampusFallParts.h"
#include "HouseCampusFall.generated.h"

class UAudioComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UHouseBlackHallSubsystem;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class USoundAttenuation;
class USoundWaveProcedural;
struct FHallMesh;

namespace HouseCampusFall
{
/** Off with -NoCampusFall (the campus and stadium as they were, for A/B checks). */
bool Enabled();
/** Where the homecoming float, the band's trailer and the bonfire stand (X, Y, and the radius they keep), once built. */
FVector FloatSpot();
FVector TrailerSpot();
FVector BonfireSpot();
/** The invented school, its team and the homecoming opponent (no real schools, teams or marks). */
inline const TCHAR* School() { return TEXT("CALDER STATE"); }
inline const TCHAR* Team() { return TEXT("GEESE"); }
inline const TCHAR* Rival() { return TEXT("DUNMORE"); }
/** Campus blocks (by their build seed) that are dorms: more windows lit, sheets, jack-o'-lanterns. */
bool IsDorm(int32 Seed);
/** Whether a campus window is lit (the original 1-in-5 pattern, or about 2-in-5 for a dorm). */
bool WindowLit(int32 Index, int32 Floor, int32 Seed);
/** Registers the CF_ materials and painted textures (school colours, field, banners, sheets, signs). Idempotent. */
void CreateLook(UHouseBlackHallSubsystem* Look);
/** Material key for the stadium bench beams of a seat row (purple, with gold bands). */
FName SeatKey(int32 Row);
/** Campus walkway lamp lens (mercury vapour, blue-green white). */
FName MercuryLensKey();
/** The three stadium lights: how far they reach, and where they sit (drawn in toward the field the floods aim at). */
float StadiumLightRadius();
FVector StadiumLightAt(const FVector& Lamp);
/** A few of the campus trees in full colour for homecoming: -1 leaves a tree alone, 0 turns it orange, 1 red. Matched by
    where the tree stands; the trees themselves never move. */
int32 AutumnTone(const FVector& At);
/** The leaf material that replaces Slot on a tree of that tone (NAME_None for bark and anything else). */
FName AutumnLeafKey(FName Slot, int32 Tone);
}

/** The homecoming dressing and its small motions: fluttering pennants and sheets, flickering jack-o'-lanterns and bonfire
    embers, chasing scoreboard bulbs, glowing mast and lamp halos, bonfire smoke, drifting leaves, and a distant drumline. */
UCLASS(NotPlaceable, Transient)
class SENIORSENDOFF_API AHouseCampusFall : public AActor
{
    GENERATED_BODY()
public:
    AHouseCampusFall();
    static AHouseCampusFall* SpawnFor(AActor* Exterior, UHouseBlackHallSubsystem* Look);
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    /** Starts the drumline now (console CampusFall.Drums). */
    void PlayDrums(const TCHAR* Trigger);
    /** The listener is inside a walk-in building: the drumline comes through the windows, muffled and quieter. */
    void SetIndoors(bool bInside);

    int32 Triangles = 0, Parts = 0, Lanterns = 0, DrumPlays = 0;

    /** A cloth (pennant, sheet, streamer) in the shared flutter mesh: its front and back vertex blocks and its rest frame. */
    struct FFlutter
    {
        int32 Section = 0, First = 0, BackSection = 0, BackFirst = 0, Cols = 0, Rows = 0;
        FVector Origin = FVector::ZeroVector, Right = FVector::ForwardVector, Down = -FVector::UpVector, Normal = FVector::RightVector;
        float Width = 0.f, Height = 0.f, Amp = 1.f, Phase = 0.f, Pinned = 0.f, Taper = 0.f;
    };
    struct FPuff { FVector P = FVector::ZeroVector, V = FVector::ZeroVector; float Age = 0.f, Life = 1.f, Size = 20.f, Spin = 0.f, SpinRate = 0.f; bool bLive = false; };
    struct FLeaf { FVector P = FVector::ZeroVector; float Yaw = 0.f, Tumble = 0.f, Rest = 0.f, Phase = 0.f, Fall = 50.f; };

private:
    void Build(UHouseBlackHallSubsystem* Look, const TArray<FVector>& Trees);
    UProceduralMeshComponent* MakePart(const FHallMesh& M, const FString& Name, bool bCollision, float CullDistance, UHouseBlackHallSubsystem* Look);
    UProceduralMeshComponent* MakeDynamic(const TCHAR* Name);
    void BuildFlutter(UHouseBlackHallSubsystem* Look, const TArray<HouseCampusFallParts::FCloth>& In);
    void BuildHalos(UHouseBlackHallSubsystem* Look, const TArray<HouseCampusFallParts::FHalo>& In);
    void BuildSmoke(UHouseBlackHallSubsystem* Look, const FVector& Origin);
    void BuildLeaves(UHouseBlackHallSubsystem* Look, const FBox& Area);
    void BuildGlow(UHouseBlackHallSubsystem* Look);
    void BuildSound();
    void UpdateFlutter(double Now, const FVector& Camera);
    void UpdateGlow(double Now);
    void UpdateHalos(const FVector& Camera, const FRotator& CameraRotation);
    void UpdateSmoke(float Dt, const FVector& Camera, const FRotator& CameraRotation);
    void UpdateLeaves(float Dt, const FVector& Camera);
    void UpdateSound(double Now);
    void FeedDrums();
    float Rand();

    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Flutter;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Halos;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Smoke;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Leaves;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMaterials;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Drums;
    bool bDrumsIndoors = false;
    UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> DrumWave;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> DrumAttenuation;

    // Flutter: every cloth lives in one movable mesh, one section per material.
    TArray<FFlutter> Cloths;
    TArray<TArray<FVector>> FlutterPos, FlutterNrm;
    FBox ClothBounds = FBox(ForceInit);
    double NextFlutter = 0.0, NextGlow = 0.0;
    // Glow: each animated key's base emission and how it moves (0 candle, 1 ember, 2 marquee chase).
    TArray<FLinearColor> GlowBase;
    TArray<int32> GlowKind;
    // Halos: one camera-facing quad each.
    TArray<HouseCampusFallParts::FHalo> HaloSpots;
    TArray<FVector> HaloPos, HaloNrm;
    TArray<FLinearColor> HaloCol;
    // Bonfire smoke.
    TArray<FPuff> Puffs;
    TArray<FVector> SmokePos, SmokeNrm;
    TArray<FLinearColor> SmokeCol;
    TArray<FVector2D> SmokeUV;
    TArray<FProcMeshTangent> SmokeTan;
    FVector SmokeOrigin = FVector::ZeroVector;
    float SmokeSpawn = 0.f, SmokeClock = 0.f;
    // Leaves coming down over the green, four colours.
    TArray<FLeaf> LeafState;
    TArray<TArray<FVector>> LeafPos, LeafNrm;
    FBox LeafArea = FBox(ForceInit);
    // Drumline: a cadence from the stadium now and then.
    TArray<int16> DrumPcm;
    TFuture<TArray<int16>> DrumMix;
    int32 DrumFed = 0, DrumQueued = 0;
    double NextDrums = 0.0, DrumsEvery = 0.0;
    bool bDrumsLive = false, bDrumsReady = false, bDrumsConsumed = false, bDrumsRecording = false;
    // What moved since the last stats line (logged every 20 s while anything does).
    int32 FlutterUpdates = 0, LeafUpdates = 0, SmokeUpdates = 0, GlowUpdates = 0;
    float FlutterPeak = 0.f;
    double NextStats = 0.0;
    FRandomStream Rng;
};
