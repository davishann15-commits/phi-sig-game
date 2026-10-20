#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseExteriorNature.h"
#include "HouseLeafWind.generated.h"

class UHouseBlackHallSubsystem;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UAudioComponent;
class USoundWave;
class USoundWaveProcedural;
class ACameraActor;
class APlayerController;

struct FHouseWindLeaf
{
    FVector Position = FVector::ZeroVector;
    FVector Normal = FVector::UpVector;
    FVector LastGroundQuery = FVector::ZeroVector;
    FVector Velocity = FVector::ZeroVector;
    float Age = -1.f, GroundZ = 0.f, Size = 1.f, Phase = 0.f;
    float Threshold = .3f, Drag = 1.f, Yaw = 0.f, Roll = 0.f, Pitch = 0.f;
    int32 Palette = 0, Instance = 0;
};

struct FHouseFallingLeaf
{
    FVector Position=FVector::ZeroVector, Velocity=FVector::ZeroVector, Surface=FVector::ZeroVector, Normal=FVector::UpVector;
    FVector WoodlandOrigin=FVector::ZeroVector;
    float Age=-1.f, Delay=0.f, Size=1.f, Phase=0.f, Yaw=0.f, Roll=0.f, Pitch=0.f, RestAge=0.f;
    int32 Pool=0, Instance=0;
    bool bGroundKnown=false, bLanded=false, bWoodland=false;
};

/** Small local cosmetic pool. No leaf collision, rigid bodies, replication traffic or lights. */
UCLASS(NotPlaceable, Transient)
class SENIORSENDOFF_API AHouseLeafWind : public AActor
{
    GENERATED_BODY()
public:
    AHouseLeafWind();
    void Initialize(UHouseBlackHallSubsystem* Look, const TArray<FTransform>& FallenLeaves, const TArray<HouseNature::FTreeCanopy>& Canopies);
    /** Opt-in pass-three extension; never changes tree transforms or the original yard RNG. */
    void EnableWoodlandEffects(UHouseBlackHallSubsystem* Look, const TArray<FName>& SwayMaterialKeys, const TArray<FTransform>& WoodlandTrees);
    /** Add an owned foliage MID to the existing gust clock without changing
     *  the original materials, RNG streams or woodland sound. */
    void RegisterWindMaterial(UMaterialInstanceDynamic* Material, float Baseline);
    /** Empty integration hook: no party/street ambience track currently exists in this exterior. */
    float GetWoodlandPartyStreetGain() const { return WoodlandPartyStreetGain; }
    void ShedLeaves(const FTransform& Tree, uint8 Kind, uint8 Variant, const FVector& Direction, int32 Seed);
    int32 ActiveFallingLeaves() const;
    int32 ShedEvents=0, LandedLeaves=0;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    bool FindGround(const FVector& Point, FVector& Ground, FVector& Normal) const;
    bool RecycleLeaf(FHouseWindLeaf& Leaf, const FVector& Viewer, const FVector& Forward);
    void AudioPlayback(const UAudioComponent* Component, const USoundWave* Wave, float Percent);
    void Review(APlayerController* Player, double Now);
    void ReviewView(APlayerController* Player, const FVector& From, const FVector& To);
    void TickFallingLeaves(float Dt, const FVector& Viewer, const FVector& Wind);
    bool IsGroundArea(const FVector& Point) const;
    /** The woods' height there, else HouseNature::ApproximateGroundHeight (which carries the Calder relief). */
    float GroundHeight(const FVector& Point) const;
    void TickWoodland(float Dt, double Now, const FVector& Viewer, const FVector& Wind);
    void BuildWoodlandAudio();
    void PlayWoodlandSound(int32 Kind, const FVector& At, float Gain);

    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pools;
    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> FallingPools;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> WindMaterials;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Rustle;
    UPROPERTY(Transient) TObjectPtr<USoundWave> RustleWave;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> ReviewCamera;
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> WoodlandAudio;
    UPROPERTY(Transient) TArray<TObjectPtr<USoundWaveProcedural>> WoodlandWaves;
    TArray<float> WindAmplitudes;
    TArray<FVector> Seeds;
    TArray<FHouseWindLeaf> Leaves;
    TArray<FHouseFallingLeaf> FallingLeaves;
    TArray<HouseNature::FTreeCanopy> TreeCanopies;
    int32 ShedCursors[3]={0,0,0};
    float HitRustle=0.f;
    FVector HitSoundPosition=FVector::ZeroVector;
    FRandomStream Random{202610102};
    FRandomStream WoodlandRandom{2026101341};
    FRandomStream WoodlandAudioRandom{2026101342};
    TArray<FVector> WoodlandSources, NearbyWoodlandSources;
    TArray<TArray<int16>> WoodlandPcm;
    FVector WoodlandCachePosition = FVector(0, 0, -100000);
    float WoodlandGain = 0.f, WoodlandPartyStreetGain = 1.f;
    float WoodlandLeafCountdown = .3f, WoodlandSoundCountdown = 4.f, WoodlandCacheCountdown = 0.f;
    float WoodlandSoundRemaining[4] = {0.f, 0.f, 0.f, 0.f};
    double WoodlandLastReport = -10.;
    int32 WoodlandShedLeaves = 0, WoodlandLandedLeaves = 0, WoodlandSoundEvents = 0, WoodlandRegisteredMaterials = 0;
    bool bWoodlandEnabled = false, bWoodlandAudioEnabled = false, bWoodlandDiagnostics = false;
    float Gust = 0.f, ListenerGain = 0.f, RustleVolume = 0.f, LastNearby = 0.f;
    bool bReady = false, bSoundStarted = false, bLeavesVisible = false;
    int32 AudioSamples = 0, MovingLeaves = 0, GroundChecks = 0;

    double ReviewStart = -1., ReviewLastReport = 0.;
    int32 ReviewStage = 0;
    float ReviewMinGust = 1.f, ReviewMaxGust = 0.f, ReviewTravel = 0.f;
    float ReviewOutsideVolume = 0.f, ReviewInsideVolume = 1.f;
    TArray<FVector> ReviewPositions;
};
