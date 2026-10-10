#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseNightClub.generated.h"

class UAudioComponent;
class UBoxComponent;
class UPostProcessComponent;
class USoundBase;
class USoundWave;
class USpotLightComponent;
class UPackageMap;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** A single replicated transport observation, rather than separately arriving
 * phase/sequence properties. Clients never use GameState's averaged offset for
 * this room's song after receiving a transport sample. */
USTRUCT()
struct FHouseClubMusicClock
{
    GENERATED_BODY()
    UPROPERTY() float ElapsedSeconds = 0.f;
    UPROPERTY() uint32 Serial = 0;
    UPROPERTY() bool bPaused = false;

    bool NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
    {
        Ar << ElapsedSeconds;
        Ar << Serial;
        uint8 PauseBit = bPaused ? 1 : 0;
        Ar.SerializeBits(&PauseBit, 1);
        if (Ar.IsLoading()) bPaused = PauseBit != 0;
        bOutSuccess = !Ar.IsError();
        return true;
    }
};

template<> struct TStructOpsTypeTraits<FHouseClubMusicClock> : TStructOpsTypeTraitsBase2<FHouseClubMusicClock>
{
    enum { WithNetSerializer = true };
};

/** An allowed, axis-aligned room volume in actor-local centimetres. */
USTRUCT(BlueprintType)
struct FHouseClubRoomRegion
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club") FVector Minimum = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club") FVector Maximum = FVector::ZeroVector;
};

/** A fixed optical origin and a convex room volume for its floor sweep.
 * SweepRegion must fit wholly inside one allowed room sub-box. */
USTRUCT(BlueprintType)
struct FHouseClubLightPlacement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club") bool Enabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club") FVector OpticalOrigin = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club") FHouseClubRoomRegion SweepRegion;
};

/** An additional entrance fade constrained to an allowed room volume. */
USTRUCT(BlueprintType)
struct FHouseClubDoorwayFade
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club") FVector Center = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club") FVector InwardNormal = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club") FHouseClubRoomRegion RoomRegion;
};

/** One music transport and six small lights for both vertically connected rooms.
 * Bounds and door points are actor-local centimetres. Music gain uses the actual
 * local camera, never radial attenuation that leaks through adjacent walls.
 */
UCLASS(Blueprintable)
class SENIORSENDOFF_API AHouseNightClub : public AActor
{
    GENERATED_BODY()
public:
    AHouseNightClub();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms") FVector LowerRoomMin = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms") FVector LowerRoomMax = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms") FVector UpperRoomMin = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms") FVector UpperRoomMax = FVector::ZeroVector;
    // The main rectangle remains the entrance/pole room. These are additional
    // allowed volumes, never its enclosing rectangle's excluded corners.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms") TArray<FHouseClubRoomRegion> LowerExtraRegions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms") TArray<FHouseClubRoomRegion> UpperExtraRegions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms") TArray<FHouseClubDoorwayFade> LowerAdditionalDoorways;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms") TArray<FHouseClubDoorwayFade> UpperAdditionalDoorways;
    // Entries0/1 are moving heads; entry2 is the strobe. Empty/disabled entries
    // retain the original main-room placement and sweep.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Lights") TArray<FHouseClubLightPlacement> LowerLightPlacements;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Lights") TArray<FHouseClubLightPlacement> UpperLightPlacements;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms") FVector LowerDoorCenter = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms") FVector UpperDoorCenter = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms") FVector LowerDoorInwardNormal = FVector(-1, 0, 0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms") FVector UpperDoorInwardNormal = FVector(-1, 0, 0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms", meta=(ClampMin="20", ClampMax="180")) float DoorFadeWidth = 95.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms", meta=(ClampMin="30", ClampMax="220")) float DoorHalfWidth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms") FVector PoleHoleCenter = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Rooms", meta=(ClampMin="20", ClampMax="200")) float PoleHoleRadius = 75.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Music") TObjectPtr<USoundBase> MusicLoop;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Music", meta=(ClampMin="60", ClampMax="160")) float MusicBPM = 120.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Music", meta=(ClampMin="1")) float MusicLoopSeconds = 16.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Music", meta=(ClampMin="0", ClampMax=".7")) float MusicVolume = .30f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Music", meta=(ClampMin="0", ClampMax=".25")) float HoleMusicGain = .10f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Lights", meta=(ClampMin="0", ClampMax="500")) float MovingLightIntensity = 430.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Lights", meta=(ClampMin="0", ClampMax="65")) float StrobeLightIntensity = 65.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Appearance", meta=(ClampMin="-1", ClampMax="2.5")) float ClubExposureBias = .25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Appearance", meta=(ClampMin="0", ClampMax="1.5")) float NeonBloomIntensity = .90f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Appearance", meta=(ClampMin="0", ClampMax="4")) float NeonBloomThreshold = .70f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Lights") bool bStrobeEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_EffectsEnabled, Category="Club|Lights") bool bClubEffectsEnabled = true;
    // Fixture optical origins are their lens exits, pointing along local +X.
    // The beam asset is a100cm cone, radius3 at its start and26 at its end.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Fixtures") TObjectPtr<UStaticMesh> MovingFixtureMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Fixtures") TObjectPtr<UStaticMesh> StrobeFixtureMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Fixtures") TObjectPtr<UStaticMesh> FixtureMountMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Fixtures") TObjectPtr<UStaticMesh> BeamMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Fixtures") TObjectPtr<UMaterialInterface> MovingFixtureMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Fixtures") TObjectPtr<UMaterialInterface> StrobeFixtureMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Fixtures") TObjectPtr<UMaterialInterface> FixtureMountMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Fixtures") TObjectPtr<UMaterialInterface> LensMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club|Fixtures") TObjectPtr<UMaterialInterface> BeamMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Club|Components") TObjectPtr<UAudioComponent> LocalMusic;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Club|Components") TArray<TObjectPtr<USpotLightComponent>> ClubLights;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Club|Components") TArray<TObjectPtr<UStaticMeshComponent>> ClubFixtures;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Club|Components") TArray<TObjectPtr<UStaticMeshComponent>> ClubFixtureMounts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Club|Components") TArray<TObjectPtr<UStaticMeshComponent>> ClubBeams;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Club|Components") TArray<TObjectPtr<UBoxComponent>> ClubPostProcessBounds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Club|Components") TArray<TObjectPtr<UPostProcessComponent>> ClubPostProcesses;
    UPROPERTY(ReplicatedUsing=OnRep_MusicEpoch, BlueprintReadOnly, Category="Club|Diagnostics") float MusicEpoch = 0.f;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Club|Diagnostics") bool bMusicEpochReady = false;
    UPROPERTY(ReplicatedUsing=OnRep_MusicClock) FHouseClubMusicClock MusicClock;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") bool DebugTransportClockReady = false;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") int32 DebugTransportClockSerial = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") float DebugTransportClockHalfRTT = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") float DebugTransportClockAgeSeconds = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") float DebugListenerGain = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") float DebugTargetGain = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") float DebugSongOffset = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") float DebugBeatPhase = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") float DebugStrobeIntensity = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") int32 DebugLightCount = 6;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") int32 DebugActiveLightCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") int32 DebugActiveFixtureCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") int32 DebugActiveMountCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") int32 DebugActiveBeamCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") int32 DebugActivePostProcessCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") int32 DebugAudioStartCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") float DebugLastAudioStartOffset = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") float DebugAudioPlaybackPhase = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") float DebugAudioPhaseError = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") int32 DebugAudioNativeSampleCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") int32 DebugAudioResyncCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") bool DebugAudioUsesNativeCursor = false;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Club|Diagnostics") float DebugLastAudioResyncError = 0.f;

    UFUNCTION(BlueprintPure, Category="Club|Music") float GetMusicGainAtWorld(FVector WorldPosition) const;
    UFUNCTION(BlueprintPure, Category="Club|Rooms") bool IsInsideClubRoomAtWorld(FVector WorldPosition, int32 RoomIndex) const;
    UFUNCTION(BlueprintPure, Category="Club|Music") float GetLoopPhaseSeconds() const;
    UFUNCTION(BlueprintPure, Category="Club|Music") float GetAudioPlaybackPhaseSeconds() const;
    UFUNCTION(BlueprintPure, Category="Club|Music") bool IsTransportClockReady() const;
    UFUNCTION(BlueprintCallable, Category="Club|Lights") void SetClubEffectsEnabled(bool bEnabled);
    UFUNCTION(CallInEditor, BlueprintCallable, Category="Club") void RefreshClub();
private:
    UFUNCTION() void OnRep_MusicEpoch();
    UFUNCTION() void OnRep_MusicClock();
    UFUNCTION() void OnRep_EffectsEnabled();
    float ServerNow() const;
    float TransportElapsedSeconds() const;
    void PublishMusicClock();
    float RoomGain(const FVector& Point, int32 RoomIndex) const;
    bool IsInsideRoomLocal(const FVector& Point, int32 RoomIndex) const;
    FBox GetRoomEnclosingBounds(int32 RoomIndex) const;
    FHouseClubLightPlacement GetLightPlacement(int32 RoomIndex, int32 LightIndex) const;
    void UpdateLocalMusic(float DeltaSeconds, const FVector& ListenerPosition, bool bHasListener);
    void UpdateLights(const FVector& ListenerPosition, bool bHasListener);
    void PositionLights();
    void ConfigureFixtureMeshes();
    void HideFixtureMeshes();
    void SetFixtureAppearance(int32 LightIndex, bool bVisible, FLinearColor Color, float Strength);
    void SetBeamAppearance(int32 BeamIndex, bool bVisible, FLinearColor Color, float Strength, float Length);
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> FixtureLensMaterials;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> BeamMaterials;
    void StartLocalMusicAtPhase(float Phase, bool bResync);
    void OnNativeAudioPlaybackPercent(const UAudioComponent* Component, const USoundWave* Wave, float Percent);
    bool bLocalMusicStarted = false;
    bool bPostProcessDiagnosticLogged = false;
    double AudioStartWallSeconds = 0.;
    double LastNativePlaybackWallSeconds = 0.;
    double AudioDriftStartedWallSeconds = -1.;
    double LastAudioResyncWallSeconds = -1.;
    double NextAudioDiagnosticWallSeconds = 0.;
    float LastNativePlaybackPhase = 0.f;
    bool bLocalMusicPaused = false;
    float PausedAudioPlaybackPhase = 0.f;
    double AudioPauseWallSeconds = 0.;
    double MusicClockReceiptWallSeconds = 0.;
    double NextMusicClockPublishWallSeconds = 0.;
    float MusicClockReceiptElapsedSeconds = 0.f;
    uint32 LastReceivedMusicClockSerial = 0;
    bool bClientMusicClockValid = false;
};
