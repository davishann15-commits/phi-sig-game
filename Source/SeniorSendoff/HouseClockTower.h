// The campus clock tower: a runtime-built collegiate-Gothic landmark (HouseClockTowerMesh) whose four opal dials keep
// the clock, whose belfry rings the Westminster Quarters (HouseClockBells) from one spatialised source, and whose crows
// leave the parapet on the hour. Built locally on every machine beside the exterior; nothing is saved into the map and
// no lights are added.
#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Math/RandomStream.h"
#include "GameFramework/Actor.h"
#include "HouseClockTowerBells.h"
#include "HouseClockTower.generated.h"

class UAudioComponent;
class UHouseBlackHallSubsystem;
class UProceduralMeshComponent;
class USoundAttenuation;
class USoundWaveProcedural;
struct FHallMesh;

/** Hour is 1..12. bQuarter is true for the :15/:30/:45 chimes and false for the full chime at the hour (the one followed
    by the hour count). Fired when the first bell of the chime sounds. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHouseClockTowerChimeEvent, int32, Hour, bool, bQuarter);

UCLASS(NotPlaceable, Transient)
class SENIORSENDOFF_API AHouseClockTower : public AActor
{
    GENERATED_BODY()
public:
    AHouseClockTower();
    /** Spawns and builds the tower for the exterior's world (skipped with -NoClockTower). Visuals and sound are skipped on
        a dedicated server, where the schedule and the event still run. */
    static AHouseClockTower* SpawnFor(AActor* Exterior, UHouseBlackHallSubsystem* Look);
    static AHouseClockTower* Find(const UWorld* World);
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    /** Listen here for chimes (gameplay hooks, quests, ambience). */
    UPROPERTY(BlueprintAssignable, Category = "Clock Tower")
    FHouseClockTowerChimeEvent OnClockTowerChime;

    /** Rings now: Quarter 1..3 = :15/:30/:45, 4 = the hour. Hour12 0 takes the hour from the clock. */
    void Chime(int32 Quarter, int32 Hour12, const TCHAR* Trigger);
    /** A single bell, once (the unscripted toll). */
    void Toll(const TCHAR* Trigger);
    /** Sends the crows off the parapet. */
    void BurstCrows(const TCHAR* Trigger);
    /** Seconds since midnight on the tower's clock: the local wall clock (the game has no clock of its own), or -ClockTime=. */
    double ClockSeconds() const;
    /** The time one dial's hands show, read back from the hand components (seconds since 12:00, 0..43200). */
    bool ReadHands(int32 Face, double& OutSeconds12) const;

    int32 Triangles = 0, Parts = 0, ChimesStarted = 0, ChimesFinished = 0, Tolls = 0, StrikesStruck = 0, AudioPlays = 0, CrowBursts = 0;
    int32 LastChimeHour12 = 0, LastChimeQuarter = 0, LastHourCount = 0, LastHourExpected = 0;
    bool bAudioConsumed = false;

private:
    struct FChime
    {
        TArray<HouseClockBells::FStrike> Strikes;
        float Length = 0.f;
        int32 Quarter = 0, Hour12 = 0, HourCount = 0;
        bool bToll = false;
        FString Trigger;
    };
    struct FCrow
    {
        FTransform Perch, Pose;
        float Delay = 0.f, Duration = 0.f, Radius = 0.f, Height = 0.f, Omega = 0.f, Phase = 0.f, Flap = 0.f, Fold = 1.f;
        double Start = -1.0;
    };

    void Build(UHouseBlackHallSubsystem* Look);
    UProceduralMeshComponent* MakePart(const FHallMesh& M, const FString& Name, bool bCollision, bool bMovable, float CullDistance, UHouseBlackHallSubsystem* Look, USceneComponent* Parent = nullptr);
    int32 CurrentHour12() const;
    void Queue(FChime&& Chime);
    void StartTimeline(double Now, TArray<int16>&& Pcm);
    void UpdateHands(bool bForce);
    void UpdateSchedule(double Now);
    void UpdateStrikes(double Now);
    void UpdateBells(double Now);
    void UpdateCrows(double Now);
    void WriteCrows();
    void UpdateAudio(double Now, float Dt);
    void FeedVoice(int32 Index, double Now);
    float IndoorTarget() const;
    bool HasSound() const { return Voices.Num() > 0; }
    void LogHands(const TCHAR* Why) const;
    /** -ClockTowerReview: checks the build, the hands and a chime, writes Saved/ClockTowerReview (non-shipping only). */
    void TickReview(double Now);

    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> HourHands;
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> MinuteHands;
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Bells;
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Clappers;
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Hammers;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Vane;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Crows;
    /** Two belfry voices take turns, so a new chime never cuts the ring-out of the last one. */
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Voices;
    UPROPERTY(Transient) TArray<TObjectPtr<USoundWaveProcedural>> VoiceWaves;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> BellAttenuation;

    // Switches (see SourceArt/ClockTower/README.md).
    float ChimeEvery = 0.f, TollEvery = 420.f, Volume = 1.f, Falloff = 90000.f;
    bool bClockOverride = false, bChimeNow = false, bReview = false;
    double ClockOffset = 0.0;

    // Schedule and the chime being rung.
    FRandomStream Rng;
    int64 LastQuarterIndex = -1;
    int64 LastHandSecond = -1;
    double NextEvery = 0.0, NextToll = 0.0, NextHandsLog = 0.0, BuiltAt = 0.0, PendingSince = 0.0;
    int32 EveryCycle = 0, ChimeSeed = 1;
    bool bPending = false, bMixing = false, bActive = false;
    FChime Pending, Active;
    TFuture<TArray<int16>> MixFuture;
    double ActiveStart = 0.0;
    int32 NextStrike = 0, HourStrikesRung = 0;
    // The mixed PCM is handed to the voice's wave half a second at a time (queueing a whole minute at once makes the
    // procedural wave shuffle megabytes on the audio thread every callback).
    struct FFeed { TArray<int16> Pcm; int32 Fed = 0, QueuedBytes = 0; bool bLive = false, bRecording = false; };
    FFeed Feeds[2];
    int32 Voice = -1;
    // A chime asked for while another is ringing is owed, not lost.
    int32 OwedQuarter = 0, OwedHour12 = 0;
    FString OwedTrigger;
    double LastChimeEnd = -1e9;

    // Bells, hammers, crows, vane, listener.
    double LastStrikeAt[5] = { -100.0, -100.0, -100.0, -100.0, -100.0 };
    float LastStrikeVelocity[5] = { 1.f, 1.f, 1.f, 1.f, 1.f };
    float HammerYaw[5] = { 0.f, 0.f, 0.f, 0.f, 0.f };
    bool bBellMoving[5] = { false, false, false, false, false };
    TArray<FCrow> CrowState;
    double NextFidget = 0.0;
    bool bCrowsFlying = false;
    float IndoorBlend = 0.f;

    int32 ReviewPhase = 0;
    double ReviewStarted = 0.0;
    bool bRecording = false, bReviewHourHeard = false;
    FString ReviewNotes;
    TWeakObjectPtr<class ACameraActor> ReviewCamera;
    int32 ReviewShot = 0;
    double ReviewChimeStart = -1.0;
};
