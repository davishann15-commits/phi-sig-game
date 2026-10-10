#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseLeafWind.generated.h"

class UHouseBlackHallSubsystem;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UAudioComponent;
class USoundWave;
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

/** Small local cosmetic pool. No leaf collision, rigid bodies, replication traffic or lights. */
UCLASS(NotPlaceable, Transient)
class SENIORSENDOFF_API AHouseLeafWind : public AActor
{
    GENERATED_BODY()
public:
    AHouseLeafWind();
    void Initialize(UHouseBlackHallSubsystem* Look, const TArray<FTransform>& FallenLeaves);
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    bool FindGround(const FVector& Point, FVector& Ground, FVector& Normal) const;
    bool RecycleLeaf(FHouseWindLeaf& Leaf, const FVector& Viewer, const FVector& Forward);
    void AudioPlayback(const UAudioComponent* Component, const USoundWave* Wave, float Percent);
    void Review(APlayerController* Player, double Now);
    void ReviewView(APlayerController* Player, const FVector& From, const FVector& To);

    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pools;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> WindMaterials;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Rustle;
    UPROPERTY(Transient) TObjectPtr<USoundWave> RustleWave;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> ReviewCamera;
    TArray<float> WindAmplitudes;
    TArray<FVector> Seeds;
    TArray<FHouseWindLeaf> Leaves;
    FRandomStream Random{202610102};
    float Gust = 0.f, ListenerGain = 0.f, RustleVolume = 0.f, LastNearby = 0.f;
    bool bReady = false, bSoundStarted = false, bLeavesVisible = false;
    int32 AudioSamples = 0, MovingLeaves = 0, GroundChecks = 0;

    double ReviewStart = -1., ReviewLastReport = 0.;
    int32 ReviewStage = 0;
    float ReviewMinGust = 1.f, ReviewMaxGust = 0.f, ReviewTravel = 0.f;
    float ReviewOutsideVolume = 0.f, ReviewInsideVolume = 1.f;
    TArray<FVector> ReviewPositions;
};
