#include "HouseNightClub.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"

namespace
{
    float Smooth01(float Value)
    {
        const float T = FMath::Clamp(Value, 0.f, 1.f);
        return T * T * (3.f - 2.f * T);
    }

    float WrappedPhase(double Seconds, float LoopSeconds)
    {
        return float(FMath::Fmod(FMath::Max(0., Seconds), double(FMath::Max(1.f, LoopSeconds))));
    }

    float PhaseDistance(float First, float Second, float LoopSeconds)
    {
        const float Distance = FMath::Abs(First - Second);
        return FMath::Min(Distance, FMath::Max(1.f, LoopSeconds) - Distance);
    }

    void ConfigureClubMesh(UStaticMeshComponent* Mesh)
    {
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetCastShadow(false);
        Mesh->bAffectDynamicIndirectLighting = false;
        Mesh->bAffectDistanceFieldLighting = false;
        Mesh->SetReceivesDecals(false);
        Mesh->SetHiddenInGame(true);
        Mesh->SetVisibility(false);
    }

    FLinearColor ClubPalette(float Phase)
    {
        // HSV interpolation follows the short blue/violet arc between these
        // saturated colors, never the green/yellow side of a full rainbow.
        const FLinearColor Colors[] = {
            FLinearColor(325.f, .98f, 1.f).HSVToLinearRGB(),
            FLinearColor(190.f, .98f, 1.f).HSVToLinearRGB(),
            FLinearColor(270.f, .98f, 1.f).HSVToLinearRGB()
        };
        const float Cycle = FMath::Fmod(FMath::Max(0.f, Phase), 3.f);
        const int32 First = FMath::FloorToInt(Cycle);
        return FLinearColor::LerpUsingHSV(Colors[First], Colors[(First + 1) % 3], Smooth01(Cycle - First));
    }

    bool ValidClubRegion(const FHouseClubRoomRegion& Region)
    {
        const FVector Size = Region.Maximum - Region.Minimum;
        return !Region.Minimum.ContainsNaN() && !Region.Maximum.ContainsNaN()
            && Size.X > 1.f && Size.Y > 1.f && Size.Z > 1.f;
    }

    bool ClubRegionContains(const FHouseClubRoomRegion& Outer, const FHouseClubRoomRegion& Inner)
    {
        if (!ValidClubRegion(Outer) || !ValidClubRegion(Inner)) return false;
        const FBox Bounds(Outer.Minimum, Outer.Maximum);
        return Bounds.IsInsideOrOn(Inner.Minimum) && Bounds.IsInsideOrOn(Inner.Maximum);
    }
}

AHouseNightClub::AHouseNightClub()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ClubOrigin"));
    LocalMusic = CreateDefaultSubobject<UAudioComponent>(TEXT("PrivateRoomMusic"));
    LocalMusic->SetupAttachment(RootComponent);
    LocalMusic->bAutoActivate = false;
    LocalMusic->bAutoDestroy = false;
    LocalMusic->bAllowSpatialization = false;
    LocalMusic->bOverrideAttenuation = true;
    LocalMusic->AttenuationOverrides.bAttenuate = false;
    LocalMusic->AttenuationOverrides.bSpatialize = false;
    LocalMusic->SetVolumeMultiplier(0.f);
    for (int32 Room = 0; Room < 2; ++Room)
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const FName Name(*FString::Printf(TEXT("Room%dClubLight%d"), Room, Index));
            USpotLightComponent* Light = CreateDefaultSubobject<USpotLightComponent>(Name);
            Light->SetupAttachment(RootComponent);
            Light->SetMobility(EComponentMobility::Movable);
            Light->SetIntensityUnits(ELightUnits::Candelas);
            Light->SetIntensity(0.f);
            Light->SetCastShadows(false);
            Light->SetAffectTranslucentLighting(false);
            Light->SetUseInverseSquaredFalloff(true);
            Light->SetInnerConeAngle(Index == 2 ? 30.f : 8.f);
            Light->SetOuterConeAngle(Index == 2 ? 50.f : 16.f);
            Light->IndirectLightingIntensity = 0.f;
            Light->VolumetricScatteringIntensity = 0.f;
            Light->SetVisibility(false);
            ClubLights.Add(Light);
            const FName FixtureName(*FString::Printf(TEXT("Room%dClubFixture%d"), Room, Index));
            UStaticMeshComponent* Fixture = CreateDefaultSubobject<UStaticMeshComponent>(FixtureName);
            Fixture->SetupAttachment(Light);
            ConfigureClubMesh(Fixture);
            ClubFixtures.Add(Fixture);
            const FName MountName(*FString::Printf(TEXT("Room%dClubFixtureMount%d"), Room, Index));
            UStaticMeshComponent* Mount = CreateDefaultSubobject<UStaticMeshComponent>(MountName);
            // The ceiling plate does not rotate with the optical head.
            Mount->SetupAttachment(RootComponent);
            ConfigureClubMesh(Mount);
            ClubFixtureMounts.Add(Mount);
            if (Index < 2)
            {
                const FName BeamName(*FString::Printf(TEXT("Room%dClubBeam%d"), Room, Index));
                UStaticMeshComponent* Beam = CreateDefaultSubobject<UStaticMeshComponent>(BeamName);
                Beam->SetupAttachment(Light);
                ConfigureClubMesh(Beam);
                ClubBeams.Add(Beam);
            }
        }
    for (int32 Room = 0; Room < 2; ++Room)
    {
        const FName BoundsName(*FString::Printf(TEXT("Room%dClubPostProcessBounds"), Room));
        UBoxComponent* Bounds = CreateDefaultSubobject<UBoxComponent>(BoundsName);
        Bounds->SetupAttachment(RootComponent);
        // UE5.8 returns a {nullptr} physics-object array for NoCollision shapes.
        // Its post-process distance query cannot reach the shape fallback in
        // that case. QueryOnly provides valid bounds, while IgnoreAll keeps the
        // volume absent from every gameplay collision and interaction channel.
        Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
        Bounds->SetGenerateOverlapEvents(false);
        Bounds->SetCanEverAffectNavigation(false);
        Bounds->SetVisibility(false);
        Bounds->SetHiddenInGame(true);
        ClubPostProcessBounds.Add(Bounds);
        const FName ProcessName(*FString::Printf(TEXT("Room%dClubPostProcess"), Room));
        UPostProcessComponent* Process = CreateDefaultSubobject<UPostProcessComponent>(ProcessName);
        Process->SetupAttachment(Bounds);
        Process->bUnbound = false;
        Process->bEnabled = false;
        Process->Priority = 100.f;
        Process->BlendRadius = 35.f;
        Process->BlendWeight = 1.f;
        ClubPostProcesses.Add(Process);
    }
}

void AHouseNightClub::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHouseNightClub, MusicEpoch);
    DOREPLIFETIME(AHouseNightClub, bMusicEpochReady);
    DOREPLIFETIME(AHouseNightClub, MusicClock);
    DOREPLIFETIME(AHouseNightClub, bClubEffectsEnabled);
}

void AHouseNightClub::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    PositionLights();
}

void AHouseNightClub::RefreshClub()
{
    PositionLights();
    if (GetWorld() && GetWorld()->IsGameWorld()) UpdateLights(FVector::ZeroVector, false);
}

void AHouseNightClub::BeginPlay()
{
    Super::BeginPlay();
    PositionLights();
    // Saving the wave as looping also makes the imported asset reusable. The
    // runtime safeguard prevents a replaced filler wave silently playing once.
    if (USoundWave* Wave = Cast<USoundWave>(MusicLoop)) Wave->bLooping = true;
    LocalMusic->SetSound(MusicLoop);
    // The mixer cursor includes the requested seek offset and the actual
    // sample frames played. It is independent of GameState clock corrections.
    LocalMusic->OnAudioPlaybackPercentNative.AddUObject(this, &AHouseNightClub::OnNativeAudioPlaybackPercent);
    if (HasAuthority())
    {
        MusicEpoch = ServerNow();
        bMusicEpochReady = true;
        PublishMusicClock();
        ForceNetUpdate();
    }
    if (GetNetMode() == NM_DedicatedServer)
    {
        for (USpotLightComponent* Light : ClubLights) if (Light) Light->SetVisibility(false);
        HideFixtureMeshes();
    }
}

void AHouseNightClub::EndPlay(const EEndPlayReason::Type Reason)
{
    if (LocalMusic)
    {
        LocalMusic->OnAudioPlaybackPercentNative.RemoveAll(this);
        LocalMusic->Stop();
    }
    Super::EndPlay(Reason);
}

float AHouseNightClub::ServerNow() const
{
    const UWorld* World = GetWorld();
    if (!World) return 0.f;
    if (const AGameStateBase* State = World->GetGameState()) return State->GetServerWorldTimeSeconds();
    return World->GetTimeSeconds();
}

float AHouseNightClub::GetLoopPhaseSeconds() const
{
    return WrappedPhase(TransportElapsedSeconds(), MusicLoopSeconds);
}

bool AHouseNightClub::IsTransportClockReady() const
{
    return bMusicEpochReady && (HasAuthority() || bClientMusicClockValid);
}

float AHouseNightClub::TransportElapsedSeconds() const
{
    if (HasAuthority()) return FMath::Max(0.f, ServerNow() - MusicEpoch);
    if (!bClientMusicClockValid) return 0.f;
    const UWorld* World = GetWorld();
    const double Advance = MusicClock.bPaused || (World && World->IsPaused()) ? 0.
        : FMath::Max(0., FPlatformTime::Seconds() - MusicClockReceiptWallSeconds);
    return MusicClockReceiptElapsedSeconds + float(Advance);
}

void AHouseNightClub::PublishMusicClock()
{
    if (!HasAuthority() || !bMusicEpochReady) return;
    const double Now = FPlatformTime::Seconds();
    const bool bPaused = GetWorld() && GetWorld()->IsPaused();
    if (MusicClock.Serial != 0 && Now < NextMusicClockPublishWallSeconds && MusicClock.bPaused == bPaused) return;
    MusicClock.ElapsedSeconds = FMath::Max(0.f, ServerNow() - MusicEpoch);
    MusicClock.bPaused = bPaused;
    ++MusicClock.Serial;
    if (MusicClock.Serial == 0) ++MusicClock.Serial;
    NextMusicClockPublishWallSeconds = Now + .25;
    DebugTransportClockReady = true;
    DebugTransportClockSerial = int32(MusicClock.Serial);
    ForceNetUpdate();
}

void AHouseNightClub::OnRep_MusicClock()
{
    if (HasAuthority() || MusicClock.Serial == 0 || !FMath::IsFinite(MusicClock.ElapsedSeconds)
        || MusicClock.ElapsedSeconds < 0.f || MusicClock.Serial == LastReceivedMusicClockSerial) return;
    const UWorld* World = GetWorld();
    const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    const APlayerState* PlayerState = Controller ? Controller->PlayerState : nullptr;
    // The local player's exact ping is a round trip. Clamp the compensation so
    // transient startup ping spikes cannot place the song seconds into future.
    const float PingMs = PlayerState ? PlayerState->GetPingInMilliseconds() : 0.f;
    DebugTransportClockHalfRTT = FMath::IsFinite(PingMs) ? FMath::Clamp(PingMs * .0005f, 0.f, .12f) : 0.f;
    MusicClockReceiptWallSeconds = FPlatformTime::Seconds();
    MusicClockReceiptElapsedSeconds = MusicClock.ElapsedSeconds
        + (MusicClock.bPaused ? 0.f : DebugTransportClockHalfRTT);
    LastReceivedMusicClockSerial = MusicClock.Serial;
    bClientMusicClockValid = true;
    DebugTransportClockReady = true;
    DebugTransportClockSerial = int32(MusicClock.Serial);
    DebugTransportClockAgeSeconds = 0.f;
}

float AHouseNightClub::GetAudioPlaybackPhaseSeconds() const
{
    if (!bLocalMusicStarted) return 0.f;
    if (bLocalMusicPaused) return PausedAudioPlaybackPhase;
    const double Now = FPlatformTime::Seconds();
    if (LastNativePlaybackWallSeconds > 0. && Now - LastNativePlaybackWallSeconds <= .35)
        return WrappedPhase(LastNativePlaybackPhase + FMath::Max(0., Now - LastNativePlaybackWallSeconds), MusicLoopSeconds);
    // Some platforms do not publish playback-percent callbacks. Interpolating
    // from the last actual seek with wall time still detects game-clock drift.
    return WrappedPhase(DebugLastAudioStartOffset + FMath::Max(0., Now - AudioStartWallSeconds), MusicLoopSeconds);
}

void AHouseNightClub::OnNativeAudioPlaybackPercent(const UAudioComponent* Component, const USoundWave* Wave, float Percent)
{
    if (!bLocalMusicStarted || Component != LocalMusic.Get() || Wave != Cast<USoundWave>(MusicLoop)
        || !FMath::IsFinite(Percent) || Percent < 0.f) return;
    const double Now = FPlatformTime::Seconds();
    const float Phase = WrappedPhase(double(Percent) * FMath::Max(1.f, MusicLoopSeconds), MusicLoopSeconds);
    const float StartEstimate = WrappedPhase(DebugLastAudioStartOffset + FMath::Max(0., Now - AudioStartWallSeconds), MusicLoopSeconds);
    // Audio-thread callbacks queued for the stopped voice can arrive after a
    // reseek. Ignore those old cursors during the short new-voice warmup.
    if (Now - AudioStartWallSeconds < .35 && PhaseDistance(Phase, StartEstimate, MusicLoopSeconds) > .35f) return;
    LastNativePlaybackPhase = Phase;
    LastNativePlaybackWallSeconds = Now;
    ++DebugAudioNativeSampleCount;
}

void AHouseNightClub::StartLocalMusicAtPhase(float Phase, bool bResync)
{
    const double Now = FPlatformTime::Seconds();
    if (bResync)
    {
        ++DebugAudioResyncCount;
        DebugLastAudioResyncError = DebugAudioPhaseError;
        LastAudioResyncWallSeconds = Now;
        LocalMusic->Stop();
    }
    DebugLastAudioStartOffset = WrappedPhase(Phase, MusicLoopSeconds);
    AudioStartWallSeconds = Now;
    LastNativePlaybackWallSeconds = 0.;
    AudioDriftStartedWallSeconds = -1.;
    DebugAudioUsesNativeCursor = false;
    DebugAudioPlaybackPhase = DebugLastAudioStartOffset;
    DebugAudioPhaseError = 0.f;
    bLocalMusicStarted = true;
    bLocalMusicPaused = false;
    LocalMusic->SetVolumeMultiplier(MusicVolume * DebugListenerGain);
    LocalMusic->FadeIn(.045f, 1.f, DebugLastAudioStartOffset);
    if ((GetWorld() && GetWorld()->IsPaused()) || (!HasAuthority() && MusicClock.bPaused))
    {
        bLocalMusicPaused = true;
        PausedAudioPlaybackPhase = DebugLastAudioStartOffset;
        AudioPauseWallSeconds = Now;
        LocalMusic->SetPaused(true);
    }
    ++DebugAudioStartCount;
#if !UE_BUILD_SHIPPING
    if (bResync && (FParse::Param(FCommandLine::Get(), TEXT("NightClubSmoke"))
        || FParse::Param(FCommandLine::Get(), TEXT("ClubPoleMultiplayerSmoke"))))
        UE_LOG(LogTemp, Display,
            TEXT("NIGHTCLUB_AUDIO_RESYNC mode=%d wall=%.6f server=%.6f epoch=%.6f phase=%.6f oldError=%.6f resyncs=%d nativeSamples=%d"),
            int32(GetNetMode()), Now, ServerNow(), MusicEpoch, DebugLastAudioStartOffset,
            DebugLastAudioResyncError, DebugAudioResyncCount, DebugAudioNativeSampleCount);
#endif
}

bool AHouseNightClub::IsInsideRoomLocal(const FVector& Point, int32 RoomIndex) const
{
    if (RoomIndex < 0 || RoomIndex > 1 || Point.ContainsNaN()) return false;
    FHouseClubRoomRegion Main;
    Main.Minimum = RoomIndex == 0 ? LowerRoomMin : UpperRoomMin;
    Main.Maximum = RoomIndex == 0 ? LowerRoomMax : UpperRoomMax;
    if (ValidClubRegion(Main) && FBox(Main.Minimum, Main.Maximum).IsInsideOrOn(Point)) return true;
    const TArray<FHouseClubRoomRegion>& Extra = RoomIndex == 0 ? LowerExtraRegions : UpperExtraRegions;
    for (const FHouseClubRoomRegion& Region : Extra)
        if (ValidClubRegion(Region) && FBox(Region.Minimum, Region.Maximum).IsInsideOrOn(Point)) return true;
    return false;
}

bool AHouseNightClub::IsInsideClubRoomAtWorld(FVector WorldPosition, int32 RoomIndex) const
{
    return IsInsideRoomLocal(GetActorTransform().InverseTransformPosition(WorldPosition), RoomIndex);
}

FBox AHouseNightClub::GetRoomEnclosingBounds(int32 RoomIndex) const
{
    FHouseClubRoomRegion Main;
    Main.Minimum = RoomIndex == 0 ? LowerRoomMin : UpperRoomMin;
    Main.Maximum = RoomIndex == 0 ? LowerRoomMax : UpperRoomMax;
    FBox Bounds(ForceInit);
    if (ValidClubRegion(Main)) { Bounds += Main.Minimum; Bounds += Main.Maximum; }
    const TArray<FHouseClubRoomRegion>& Extra = RoomIndex == 0 ? LowerExtraRegions : UpperExtraRegions;
    for (const FHouseClubRoomRegion& Region : Extra)
        if (ValidClubRegion(Region)) { Bounds += Region.Minimum; Bounds += Region.Maximum; }
    return Bounds;
}

FHouseClubLightPlacement AHouseNightClub::GetLightPlacement(int32 RoomIndex, int32 LightIndex) const
{
    FHouseClubLightPlacement Placement;
    Placement.SweepRegion.Minimum = RoomIndex == 0 ? LowerRoomMin : UpperRoomMin;
    Placement.SweepRegion.Maximum = RoomIndex == 0 ? LowerRoomMax : UpperRoomMax;
    const FVector Size = Placement.SweepRegion.Maximum - Placement.SweepRegion.Minimum;
    const float XFraction = LightIndex == 0 ? .24f : LightIndex == 1 ? .76f : .50f;
    const float YFraction = LightIndex == 0 ? .30f : LightIndex == 1 ? .70f : .52f;
    Placement.OpticalOrigin = FVector(Placement.SweepRegion.Minimum.X + Size.X * XFraction,
        Placement.SweepRegion.Minimum.Y + Size.Y * YFraction, Placement.SweepRegion.Maximum.Z - 54.f);
    const TArray<FHouseClubLightPlacement>& Authored = RoomIndex == 0 ? LowerLightPlacements : UpperLightPlacements;
    if (!Authored.IsValidIndex(LightIndex) || !Authored[LightIndex].Enabled) return Placement;
    const FHouseClubLightPlacement& Candidate = Authored[LightIndex];
    if (!ValidClubRegion(Candidate.SweepRegion) || Candidate.OpticalOrigin.ContainsNaN()
        || Candidate.SweepRegion.Maximum.Z - Candidate.SweepRegion.Minimum.Z < 124.f
        || !FBox(Candidate.SweepRegion.Minimum, Candidate.SweepRegion.Maximum).IsInsideOrOn(Candidate.OpticalOrigin))
        return Placement;
    bool bSafeSweep = ClubRegionContains(Placement.SweepRegion, Candidate.SweepRegion);
    const TArray<FHouseClubRoomRegion>& Extra = RoomIndex == 0 ? LowerExtraRegions : UpperExtraRegions;
    for (const FHouseClubRoomRegion& Region : Extra)
        bSafeSweep |= ClubRegionContains(Region, Candidate.SweepRegion);
    if (!bSafeSweep) return Placement;
    Placement = Candidate;
    Placement.OpticalOrigin.Z = FMath::Clamp(Placement.OpticalOrigin.Z,
        Placement.SweepRegion.Minimum.Z + 70.f, Placement.SweepRegion.Maximum.Z - 54.f);
    return Placement;
}

float AHouseNightClub::RoomGain(const FVector& Point, int32 RoomIndex) const
{
    if (!IsInsideRoomLocal(Point, RoomIndex)) return 0.f;
    const FVector Minimum = RoomIndex == 0 ? LowerRoomMin : UpperRoomMin;
    const FVector Maximum = RoomIndex == 0 ? LowerRoomMax : UpperRoomMax;
    auto FadeAtDoor = [&](const FVector& Door, const FVector& Inward,
        const FHouseClubRoomRegion& Region)
    {
        if (!ValidClubRegion(Region) || Door.ContainsNaN() || Inward.ContainsNaN()
            || !FBox(Region.Minimum, Region.Maximum).IsInsideOrOn(Point)) return 1.f;
        const FVector Normal = Inward.GetSafeNormal2D();
        if (Normal.IsNearlyZero()) return 1.f;
        const FVector Offset = Point - Door;
        const float Depth = FVector::DotProduct(Offset, Normal);
        const FVector AlongWall(-Normal.Y, Normal.X, 0.f);
        const float Side = FMath::Abs(FVector::DotProduct(Offset, AlongWall));
        const float NearDoor = 1.f - Smooth01((Side - DoorHalfWidth) / 45.f);
        const FVector DoorOnRoom(FMath::Clamp(Door.X, Region.Minimum.X, Region.Maximum.X),
            FMath::Clamp(Door.Y, Region.Minimum.Y, Region.Maximum.Y), Door.Z);
        const float BoundaryDepth = FMath::Max(0.f, FVector::DotProduct(DoorOnRoom - Door, Normal));
        const float FadeLength = FMath::Max(1.f, FMath::Max(20.f, DoorFadeWidth) - BoundaryDepth);
        return FMath::Lerp(1.f, Smooth01((Depth - BoundaryDepth) / FadeLength), NearDoor);
    };
    FHouseClubRoomRegion Main; Main.Minimum = Minimum; Main.Maximum = Maximum;
    float Gain = FadeAtDoor(RoomIndex == 0 ? LowerDoorCenter : UpperDoorCenter,
        RoomIndex == 0 ? LowerDoorInwardNormal : UpperDoorInwardNormal, Main);
    const TArray<FHouseClubDoorwayFade>& Entrances = RoomIndex == 0
        ? LowerAdditionalDoorways : UpperAdditionalDoorways;
    const TArray<FHouseClubRoomRegion>& Extra = RoomIndex == 0 ? LowerExtraRegions : UpperExtraRegions;
    for (const FHouseClubDoorwayFade& Entry : Entrances)
    {
        bool bAllowed = ClubRegionContains(Main, Entry.RoomRegion);
        for (const FHouseClubRoomRegion& Region : Extra)
            bAllowed |= ValidClubRegion(Region) && ClubRegionContains(Region, Entry.RoomRegion);
        if (bAllowed) Gain = FMath::Min(Gain, FadeAtDoor(Entry.Center, Entry.InwardNormal, Entry.RoomRegion));
    }
    return Gain;
}

float AHouseNightClub::GetMusicGainAtWorld(FVector WorldPosition) const
{
    const FVector Point = GetActorTransform().InverseTransformPosition(WorldPosition);
    const float Lower = RoomGain(Point, 0);
    const float Upper = RoomGain(Point, 1);
    float Hole = 0.f;
    if (Point.Z > LowerRoomMax.Z && Point.Z < UpperRoomMin.Z)
    {
        const float Radius = FVector2D(Point.X - PoleHoleCenter.X, Point.Y - PoleHoleCenter.Y).Size();
        if (Radius < PoleHoleRadius)
            Hole = HoleMusicGain * (1.f - Smooth01(Radius / FMath::Max(20.f, PoleHoleRadius)));
    }
    return FMath::Clamp(FMath::Max3(Lower, Upper, Hole), 0.f, 1.f);
}

void AHouseNightClub::OnRep_MusicEpoch()
{
    // A late join begins at the transport's current sample rather than song 0.
    if (LocalMusic) LocalMusic->Stop();
    bLocalMusicStarted = false;
    bLocalMusicPaused = false;
    DebugListenerGain = 0.f;
    LastNativePlaybackWallSeconds = 0.;
    AudioDriftStartedWallSeconds = -1.;
}

void AHouseNightClub::OnRep_EffectsEnabled()
{
    if (!bClubEffectsEnabled)
    {
        for (USpotLightComponent* Light : ClubLights) if (Light)
        {
            Light->SetIntensity(0.f);
            Light->SetVisibility(false);
        }
        for (UPostProcessComponent* Process : ClubPostProcesses) if (Process) Process->bEnabled = false;
        HideFixtureMeshes();
        DebugActiveLightCount = 0;
        DebugActivePostProcessCount = 0;
    }
}

void AHouseNightClub::SetClubEffectsEnabled(bool bEnabled)
{
    if (!HasAuthority()) return;
    bClubEffectsEnabled = bEnabled;
    OnRep_EffectsEnabled();
    ForceNetUpdate();
}

void AHouseNightClub::PositionLights()
{
    for (int32 Room = 0; Room < 2; ++Room)
    {
        const FBox EnclosingBounds = GetRoomEnclosingBounds(Room);
        if (ClubPostProcessBounds.IsValidIndex(Room) && ClubPostProcessBounds[Room])
        {
            ClubPostProcessBounds[Room]->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            ClubPostProcessBounds[Room]->SetCollisionResponseToAllChannels(ECR_Ignore);
            ClubPostProcessBounds[Room]->SetGenerateOverlapEvents(false);
            ClubPostProcessBounds[Room]->SetCanEverAffectNavigation(false);
            ClubPostProcessBounds[Room]->SetRelativeLocation(EnclosingBounds.IsValid ? EnclosingBounds.GetCenter() : FVector::ZeroVector);
            ClubPostProcessBounds[Room]->SetBoxExtent(EnclosingBounds.IsValid ? EnclosingBounds.GetExtent() : FVector::ZeroVector, false);
        }
        if (ClubPostProcesses.IsValidIndex(Room) && ClubPostProcesses[Room])
        {
            UPostProcessComponent* Process = ClubPostProcesses[Room];
            Process->bUnbound = false;
            Process->Priority = 100.f;
            Process->BlendRadius = 35.f;
            Process->Settings.bOverride_AutoExposureBias = true;
            Process->Settings.AutoExposureBias = FMath::Clamp(ClubExposureBias, -1.f, 2.5f);
            Process->Settings.bOverride_BloomIntensity = true;
            Process->Settings.BloomIntensity = FMath::Clamp(NeonBloomIntensity, 0.f, 1.5f);
            Process->Settings.bOverride_BloomThreshold = true;
            Process->Settings.BloomThreshold = FMath::Clamp(NeonBloomThreshold, 0.f, 4.f);
        }
        for (int32 Index = 0; Index < 3; ++Index)
        {
            if (!ClubLights.IsValidIndex(Room * 3 + Index)) continue;
            USpotLightComponent* Light = ClubLights[Room * 3 + Index];
            if (!Light) continue;
            const FHouseClubLightPlacement Placement = GetLightPlacement(Room, Index);
            Light->SetRelativeLocation(Placement.OpticalOrigin);
            Light->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
            Light->SetInnerConeAngle(Index == 2 ? 30.f : 8.f);
            Light->SetOuterConeAngle(Index == 2 ? 50.f : 16.f);
            // Spotlight cones stay below the ceiling with no costly shadows,
            // GI injection, volumetric fog, or unlimited attenuation radii.
            Light->SetAttenuationRadius(FMath::Clamp(Placement.SweepRegion.Maximum.Z - Placement.SweepRegion.Minimum.Z + 70.f, 180.f, 480.f)
                * GetActorScale3D().GetAbsMax());
            if (ClubFixtureMounts.IsValidIndex(Room * 3 + Index) && ClubFixtureMounts[Room * 3 + Index])
            {
                ClubFixtureMounts[Room * 3 + Index]->SetRelativeLocation(Light->GetRelativeLocation());
                ClubFixtureMounts[Room * 3 + Index]->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
            }
        }
    }
    DebugLightCount = ClubLights.Num();
    ConfigureFixtureMeshes();
}

void AHouseNightClub::ConfigureFixtureMeshes()
{
    FixtureLensMaterials.SetNum(ClubFixtures.Num());
    BeamMaterials.SetNum(ClubBeams.Num());
    const bool bGameWorld = GetWorld() && GetWorld()->IsGameWorld();
    const bool bPreview = !bGameWorld && bClubEffectsEnabled;
    for (int32 Index = 0; Index < ClubFixtures.Num(); ++Index)
    {
        UStaticMeshComponent* Fixture = ClubFixtures[Index];
        if (!Fixture) continue;
        const bool bStrobe = Index % 3 == 2;
        Fixture->SetStaticMesh(bStrobe ? StrobeFixtureMesh.Get() : MovingFixtureMesh.Get());
        Fixture->SetRelativeTransform(FTransform::Identity);
        FixtureLensMaterials[Index] = nullptr;
        UMaterialInterface* Casing = bStrobe ? StrobeFixtureMaterial.Get() : MovingFixtureMaterial.Get();
        const TArray<FName> Slots = Fixture->GetMaterialSlotNames();
        for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
        {
            Fixture->SetMaterial(Slot, nullptr);
            if (Slots[Slot].ToString().Contains(TEXT("Lens"), ESearchCase::IgnoreCase))
            {
                // Assets keep all authored housing/ring materials by default.
                // Only the optical slot receives the animated lens material.
                if (LensMaterial)
                {
                    Fixture->SetMaterial(Slot, LensMaterial.Get());
                    if (bGameWorld)
                    {
                        FixtureLensMaterials[Index] = Fixture->CreateDynamicMaterialInstance(Slot, LensMaterial.Get());
                        if (FixtureLensMaterials[Index]) FixtureLensMaterials[Index]->SetFlags(RF_Transient);
                    }
                }
            }
            else if (Casing) Fixture->SetMaterial(Slot, Casing);
        }
        if (ClubFixtureMounts.IsValidIndex(Index) && ClubFixtureMounts[Index])
        {
            UStaticMeshComponent* Mount = ClubFixtureMounts[Index];
            Mount->SetStaticMesh(FixtureMountMesh.Get());
            if (FixtureMountMaterial)
                for (int32 Slot = 0; Slot < Mount->GetNumMaterials(); ++Slot)
                    Mount->SetMaterial(Slot, FixtureMountMaterial.Get());
        }
        SetFixtureAppearance(Index, bPreview, bStrobe ? FLinearColor(.65f, .8f, 1.f)
            : ClubPalette(float(Index) * .75f), bStrobe ? .12f : .60f);
    }
    for (int32 Index = 0; Index < ClubBeams.Num(); ++Index)
    {
        UStaticMeshComponent* Beam = ClubBeams[Index];
        if (!Beam) continue;
        Beam->SetStaticMesh(BeamMesh.Get());
        Beam->SetRelativeLocation(FVector::ZeroVector);
        Beam->SetRelativeRotation(FRotator::ZeroRotator);
        Beam->SetMaterial(0, BeamMaterial.Get());
        BeamMaterials[Index] = nullptr;
        if (bGameWorld && BeamMaterial)
        {
            BeamMaterials[Index] = Beam->CreateDynamicMaterialInstance(0, BeamMaterial.Get());
            if (BeamMaterials[Index]) BeamMaterials[Index]->SetFlags(RF_Transient);
        }
        const FHouseClubLightPlacement Placement = GetLightPlacement(Index / 2, Index % 2);
        SetBeamAppearance(Index, bPreview, ClubPalette(float(Index) * .75f), .30f,
            FMath::Max(1.f, Placement.OpticalOrigin.Z - Placement.SweepRegion.Minimum.Z - 4.f));
    }
}

void AHouseNightClub::HideFixtureMeshes()
{
    for (int32 Index = 0; Index < ClubFixtures.Num(); ++Index)
        SetFixtureAppearance(Index, false, FLinearColor::Black, 0.f);
    for (int32 Index = 0; Index < ClubBeams.Num(); ++Index)
        SetBeamAppearance(Index, false, FLinearColor::Black, 0.f, 1.f);
    DebugActiveFixtureCount = 0;
    DebugActiveMountCount = 0;
    DebugActiveBeamCount = 0;
}

void AHouseNightClub::SetFixtureAppearance(int32 LightIndex, bool bVisible, FLinearColor Color, float Strength)
{
    if (ClubFixtures.IsValidIndex(LightIndex) && ClubFixtures[LightIndex])
    {
        UStaticMeshComponent* Fixture = ClubFixtures[LightIndex];
        const bool bShow = bVisible && Fixture->GetStaticMesh();
        Fixture->SetHiddenInGame(!bShow);
        Fixture->SetVisibility(bShow);
    }
    if (ClubFixtureMounts.IsValidIndex(LightIndex) && ClubFixtureMounts[LightIndex])
    {
        UStaticMeshComponent* Mount = ClubFixtureMounts[LightIndex];
        const bool bShow = bVisible && Mount->GetStaticMesh();
        Mount->SetHiddenInGame(!bShow);
        Mount->SetVisibility(bShow);
    }
    if (FixtureLensMaterials.IsValidIndex(LightIndex) && FixtureLensMaterials[LightIndex])
    {
        FixtureLensMaterials[LightIndex]->SetVectorParameterValue(TEXT("LensColor"), Color);
        FixtureLensMaterials[LightIndex]->SetScalarParameterValue(TEXT("LensStrength"), bVisible ? Strength : 0.f);
    }
}

void AHouseNightClub::SetBeamAppearance(int32 BeamIndex, bool bVisible, FLinearColor Color, float Strength, float Length)
{
    if (!ClubBeams.IsValidIndex(BeamIndex) || !ClubBeams[BeamIndex]) return;
    UStaticMeshComponent* Beam = ClubBeams[BeamIndex];
    const bool bShow = bVisible && Beam->GetStaticMesh() && BeamMaterial;
    Beam->SetHiddenInGame(!bShow);
    Beam->SetVisibility(bShow);
    const float Scale = FMath::Max(1.f, Length) / 100.f;
    const float RadialScale = Scale * FMath::Tan(FMath::DegreesToRadians(16.f)) / .26f;
    Beam->SetRelativeScale3D(FVector(Scale, RadialScale, RadialScale));
    if (BeamMaterials.IsValidIndex(BeamIndex) && BeamMaterials[BeamIndex])
    {
        BeamMaterials[BeamIndex]->SetVectorParameterValue(TEXT("BeamColor"), Color);
        BeamMaterials[BeamIndex]->SetScalarParameterValue(TEXT("BeamStrength"), bShow ? Strength : 0.f);
    }
}

void AHouseNightClub::UpdateLocalMusic(float DeltaSeconds, const FVector& ListenerPosition, bool bHasListener)
{
    if (LocalMusic && bLocalMusicStarted)
    {
        // A listen host can pause its world while the remote client's world
        // still ticks. The mixer must freeze with the replicated transport.
        const bool bPaused = (GetWorld() && GetWorld()->IsPaused())
            || (!HasAuthority() && MusicClock.bPaused);
        if (bPaused && !bLocalMusicPaused)
        {
            PausedAudioPlaybackPhase = GetAudioPlaybackPhaseSeconds();
            AudioPauseWallSeconds = FPlatformTime::Seconds();
            bLocalMusicPaused = true;
            LocalMusic->SetPaused(true);
        }
        else if (!bPaused && bLocalMusicPaused)
        {
            // Exclude the paused interval from the fallback playback cursor.
            AudioStartWallSeconds += FMath::Max(0., FPlatformTime::Seconds() - AudioPauseWallSeconds);
            LastNativePlaybackWallSeconds = 0.;
            bLocalMusicPaused = false;
            LocalMusic->SetPaused(false);
        }
    }
    DebugSongOffset = GetLoopPhaseSeconds();
    DebugTargetGain = bHasListener ? GetMusicGainAtWorld(ListenerPosition) : 0.f;
    // Approaching/leaving the real doorway gives a smooth geometric fade.
    // Once outside both precise room footprints, the gain is exactly zero.
    DebugListenerGain = DebugTargetGain <= KINDA_SMALL_NUMBER ? 0.f
        : FMath::FInterpTo(DebugListenerGain, DebugTargetGain, DeltaSeconds, 9.f);
    if (!LocalMusic || !MusicLoop || !IsTransportClockReady()) return;
    if (DebugListenerGain > .0005f)
    {
        if (!bLocalMusicStarted)
        {
            StartLocalMusicAtPhase(DebugSongOffset, false);
        }
        else
        {
            LocalMusic->SetVolumeMultiplier(MusicVolume * DebugListenerGain);
            const double Now = FPlatformTime::Seconds();
            DebugAudioPlaybackPhase = GetAudioPlaybackPhaseSeconds();
            DebugAudioPhaseError = PhaseDistance(DebugAudioPlaybackPhase, DebugSongOffset, MusicLoopSeconds);
            DebugAudioUsesNativeCursor = LastNativePlaybackWallSeconds > 0.
                && Now - LastNativePlaybackWallSeconds <= .35;
            if (DebugAudioPhaseError > .20f)
            {
                if (AudioDriftStartedWallSeconds < 0.) AudioDriftStartedWallSeconds = Now;
                // A short sustained-error gate and cooldown avoid reseeking on
                // ordinary mixer-block jitter, while fixing corrected startup
                // clocks and actual long frame stalls on every local listener.
                if (Now - AudioDriftStartedWallSeconds >= .18 && Now - AudioStartWallSeconds > .35
                    && (LastAudioResyncWallSeconds < 0. || Now - LastAudioResyncWallSeconds > .8))
                    StartLocalMusicAtPhase(DebugSongOffset, true);
            }
            else AudioDriftStartedWallSeconds = -1.;
#if !UE_BUILD_SHIPPING
            if (Now >= NextAudioDiagnosticWallSeconds
                && (FParse::Param(FCommandLine::Get(), TEXT("NightClubSmoke"))
                    || FParse::Param(FCommandLine::Get(), TEXT("ClubPoleMultiplayerSmoke"))))
            {
                NextAudioDiagnosticWallSeconds = Now + 1.;
                UE_LOG(LogTemp, Display,
                    TEXT("NIGHTCLUB_AUDIO_CURSOR mode=%d wall=%.6f server=%.6f epoch=%.6f expected=%.6f audible=%.6f error=%.6f native=%d samples=%d resyncs=%d playing=%d clockReady=%d clockSerial=%d clockAge=%.6f halfRTT=%.6f"),
                    int32(GetNetMode()), Now, ServerNow(), MusicEpoch, DebugSongOffset,
                    DebugAudioPlaybackPhase, DebugAudioPhaseError, int32(DebugAudioUsesNativeCursor),
                    DebugAudioNativeSampleCount, DebugAudioResyncCount, int32(LocalMusic->IsPlaying()),
                    int32(IsTransportClockReady()), DebugTransportClockSerial, DebugTransportClockAgeSeconds, DebugTransportClockHalfRTT);
            }
#endif
        }
    }
    else
    {
        LocalMusic->SetVolumeMultiplier(0.f);
        if (bLocalMusicStarted)
        {
            LocalMusic->Stop();
            bLocalMusicStarted = false;
            bLocalMusicPaused = false;
            LastNativePlaybackWallSeconds = 0.;
            AudioDriftStartedWallSeconds = -1.;
        }
        DebugAudioPlaybackPhase = 0.f;
        DebugAudioPhaseError = 0.f;
        DebugAudioUsesNativeCursor = false;
    }
}

void AHouseNightClub::UpdateLights(const FVector& ListenerPosition, bool bHasListener)
{
    const FVector LocalListener = GetActorTransform().InverseTransformPosition(ListenerPosition);
    const float Time = TransportElapsedSeconds();
    const float BeatSeconds = 60.f / FMath::Max(60.f, MusicBPM);
    DebugBeatPhase = FMath::Fmod(Time / BeatSeconds, 1.f);
    DebugActiveLightCount = 0;
    DebugActiveFixtureCount = 0;
    DebugActiveMountCount = 0;
    DebugActiveBeamCount = 0;
    DebugActivePostProcessCount = 0;
    DebugStrobeIntensity = 0.f;
    // Exactly one soft65ms flash per two beats:1Hz at the filler track's
    // 120 BPM. There is no rapid white full-screen flash or camera exposure edit.
    const float FlashPhase = FMath::Fmod(Time, BeatSeconds * 2.f);
    const float Flash = bStrobeEnabled && FlashPhase < .065f
        ? FMath::Sin(PI * FlashPhase / .065f) : 0.f;
    const float Pulse = (.45f + .55f * FMath::Exp(-DebugBeatPhase * 6.f)) * (1.f - .30f * Flash);
    for (int32 Room = 0; Room < 2; ++Room)
    {
        const bool bRoomVisible = bHasListener && IsInsideRoomLocal(LocalListener, Room);
        const bool bEnabled = bRoomVisible && bClubEffectsEnabled && IsTransportClockReady();
        if (ClubPostProcesses.IsValidIndex(Room) && ClubPostProcesses[Room])
        {
            // A bounded component also has an outer blend halo. Gate that halo
            // to the precise room, so exposure/bloom never alters the hallway
            // or Boy Wick even when the camera is close to an adjoining wall.
            ClubPostProcesses[Room]->bEnabled = bEnabled;
            if (bEnabled) ++DebugActivePostProcessCount;
        }
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const int32 LightIndex = Room * 3 + Index;
            const int32 BeamIndex = Room * 2 + Index;
            if (!ClubLights.IsValidIndex(LightIndex)) continue;
            USpotLightComponent* Light = ClubLights[LightIndex];
            if (!Light) continue;
            Light->SetVisibility(bEnabled);
            if (!bEnabled)
            {
                Light->SetIntensity(0.f);
                SetFixtureAppearance(LightIndex, false, FLinearColor::Black, 0.f);
                if (Index < 2) SetBeamAppearance(BeamIndex, false, FLinearColor::Black, 0.f, 1.f);
                continue;
            }
            ++DebugActiveLightCount;
            const FHouseClubLightPlacement Placement = GetLightPlacement(Room, Index);
            if (Index == 2)
            {
                const float Intensity = FMath::Clamp(StrobeLightIntensity, 0.f, 65.f) * Flash;
                const FLinearColor Color = FMath::Lerp(FLinearColor(1.f, .88f, .64f),
                    FLinearColor(.55f, .76f, 1.f), .5f + .5f * FMath::Sin(Time * .18f + Room));
                Light->SetIntensity(Intensity);
                Light->SetLightColor(Color);
                Light->SetRelativeLocation(Placement.OpticalOrigin);
                Light->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
                SetFixtureAppearance(LightIndex, true, Color, .015f + .985f * Flash);
                DebugStrobeIntensity = FMath::Max(DebugStrobeIntensity, Intensity);
            }
            else
            {
                const FVector Minimum = Placement.SweepRegion.Minimum;
                const FVector Maximum = Placement.SweepRegion.Maximum;
                const FVector Center = (Minimum + Maximum) * .5f;
                const FVector Size = Maximum - Minimum;
                const float MarginX = FMath::Min(100.f, Size.X * .30f);
                const float MarginY = FMath::Min(100.f, Size.Y * .30f);
                const float TravelX = FMath::Max(0.f, Size.X * .5f - MarginX);
                const float TravelY = FMath::Max(0.f, Size.Y * .5f - MarginY);
                const float Phase = Time * .65f + Index * PI + Room * .91f;
                // Both ends of the cone stay within the room: sources sit
                // below the ceiling, and sweeping floor targets have a wide
                // wall margin. There is no volumetric fog or extra beam light.
                const FVector Target(Center.X + FMath::Sin(Phase) * TravelX * .72f,
                    Center.Y + FMath::Cos(Phase * .82f + .4f) * TravelY * .66f, Minimum.Z + 4.f);
                const FVector NominalOptical = ClubFixtureMounts.IsValidIndex(LightIndex)
                    && ClubFixtureMounts[LightIndex] ? ClubFixtureMounts[LightIndex]->GetRelativeLocation()
                    : Placement.OpticalOrigin;
                // The fixed yoke's axle is12.4cm behind the lens in the
                // straight-down pose. Swing the lens around that axle so the
                // moving housing remains attached to its stationary mount.
                const FVector Hinge = NominalOptical + FVector(0.f, 0.f, 12.4f);
                const FVector Direction = (Target - Hinge).GetSafeNormal();
                const FVector OpticalPosition = Hinge + Direction * 12.4f;
                const float Distance = (Target - OpticalPosition).Size();
                const FLinearColor Color = ClubPalette(Time * .18f + Index * 1.1f + Room * .6f);
                Light->SetRelativeLocation(OpticalPosition);
                Light->SetRelativeRotation(Direction.Rotation());
                Light->SetAttenuationRadius(FMath::Clamp(Distance + 35.f, 180.f, 600.f)
                    * GetActorScale3D().GetAbsMax());
                Light->SetLightColor(Color);
                Light->SetIntensity(FMath::Clamp(MovingLightIntensity, 0.f, 500.f) * Pulse);
                SetFixtureAppearance(LightIndex, true, Color, .2f + .8f * Pulse);
                SetBeamAppearance(BeamIndex, true, Color, .65f * Pulse, Distance);
                if (ClubBeams.IsValidIndex(BeamIndex) && ClubBeams[BeamIndex]
                    && ClubBeams[BeamIndex]->IsVisible()) ++DebugActiveBeamCount;
            }
            if (ClubFixtures.IsValidIndex(LightIndex) && ClubFixtures[LightIndex]
                && ClubFixtures[LightIndex]->IsVisible()) ++DebugActiveFixtureCount;
            if (ClubFixtureMounts.IsValidIndex(LightIndex) && ClubFixtureMounts[LightIndex]
                && ClubFixtureMounts[LightIndex]->IsVisible()) ++DebugActiveMountCount;
        }
    }
}

void AHouseNightClub::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    PublishMusicClock();
    DebugTransportClockReady = IsTransportClockReady();
    DebugTransportClockAgeSeconds = HasAuthority() || !bClientMusicClockValid ? 0.f
        : float(FMath::Max(0., FPlatformTime::Seconds() - MusicClockReceiptWallSeconds));
    if (GetNetMode() == NM_DedicatedServer) return;
    APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    const bool bHasListener = Controller && Controller->IsLocalController() && Controller->PlayerCameraManager;
    const FVector Listener = bHasListener ? Controller->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
    UpdateLocalMusic(DeltaSeconds, Listener, bHasListener);
    UpdateLights(Listener, bHasListener);
#if !UE_BUILD_SHIPPING
    if (!bPostProcessDiagnosticLogged && bHasListener && DebugActivePostProcessCount > 0
        && FParse::Param(FCommandLine::Get(), TEXT("NightClubSmoke")))
    {
        bPostProcessDiagnosticLogged = true;
        for (int32 Room = 0; Room < ClubPostProcesses.Num(); ++Room)
            if (ClubPostProcesses[Room] && ClubPostProcessBounds.IsValidIndex(Room) && ClubPostProcessBounds[Room])
            {
                const FVector Center = ClubPostProcessBounds[Room]->GetComponentLocation();
                float Distance = FLT_MAX;
                const bool bContains = ClubPostProcesses[Room]->EncompassesPoint(Center, 0.f, &Distance);
                const FPostProcessVolumeProperties Properties = ClubPostProcesses[Room]->GetProperties();
                UE_LOG(LogTemp, Display,
                    TEXT("NIGHTCLUB_PP_BOUNDS room=%d center=%s encompasses=%d distance=%.3f enabled=%d unbound=%d priority=%.1f collision=%d pawnResponse=%d"),
                    Room, *Center.ToCompactString(), int32(bContains), Distance,
                    int32(Properties.bIsEnabled), int32(Properties.bIsUnbound), Properties.Priority,
                    int32(ClubPostProcessBounds[Room]->GetCollisionEnabled()),
                    int32(ClubPostProcessBounds[Room]->GetCollisionResponseToChannel(ECC_Pawn)));
            }
    }
#endif
}
