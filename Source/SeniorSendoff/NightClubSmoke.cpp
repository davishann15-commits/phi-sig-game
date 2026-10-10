#include "NightClubSmoke.h"

#if !UE_BUILD_SHIPPING
#include "HouseNightClub.h"
#include "HouseInteractiveProp.h"
#include "HouseWorldScale.h"
#include "StoryCampaign.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundWave.h"
#include "UnrealClient.h"

namespace
{
constexpr float SettledAudioPhaseToleranceSeconds = .25f;

struct FClubCamera
{
    FString Name;
    FVector Position, Target;
    bool bClub = false;
};
struct FClubSmokeState
{
    int32 Stage = 0, CameraIndex = 0, AudioStep = 0, IntermediateFadeSamples = 0, IntermediateFadeInSamples = 0, ObservedStrobeFrames = 0;
    uint64 WarmFrame = 0, ChangedFrame = 0;
    double Started = 0, Changed = 0, LastFrame = 0, SampleStarted = 0;
    float PreviousGain = 0;
    TWeakObjectPtr<AHouseNightClub> Club;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;
    TArray<FClubCamera> Cameras;
    TArray<FClubCamera> ExcludedFootprintCameras;
    TArray<double> FrameTimes;
    TArray<TSharedPtr<FJsonValue>> CollisionResults, AudioResults, LightResults, PerformanceResults;
    FString Directory;
    FVector AudioDoor, AudioNormal;
    FRotator FirstBeamRotation = FRotator::ZeroRotator;
    FLinearColor FirstBeamColor = FLinearColor::Black;
    float MaximumBeamRotationChange = 0.f, MaximumBeamColorChange = 0.f, MaximumObservedStrobe = 0.f;
    bool bExpanded = false, bWrapLayout = false, bCheckingAdditionalDoor = false;
    int32 AdditionalEntryFadeInFrames = 0;
    TArray<FVector> WalkTargets;
    int32 WalkRoom = 0, WalkLeg = 0, WalkSamples = 0, WalkSeamCrossings = 0, ExcludedCameraIndex = 0;
    double WalkStarted = 0.;
    FVector WalkStart = FVector::ZeroVector, WalkPrevious = FVector::ZeroVector;
    float WalkFloorZ = 0.f, MaximumWalkStep = 0.f;
};

TSharedRef<FJsonObject> PositionRecord(const FString& Name, const FVector& P)
{
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), Name);
    Result->SetNumberField(TEXT("x"), P.X);
    Result->SetNumberField(TEXT("y"), P.Y);
    Result->SetNumberField(TEXT("z"), P.Z);
    return Result;
}

int32 CVarInt(const TCHAR* Name)
{
    IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name);
    return Variable ? Variable->GetInt() : -1;
}

void PutReport(FClubSmokeState& S, const FString& Status, const FString& Detail, UWorld* World)
{
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("status"), Status);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("scope"), TEXT("Rendered Chapter01_House floor and rail queries, actual listener music fades and light budget, real wall-clock frame pacing. Pole traversal and LAN replication are checked separately."));
    Report->SetStringField(TEXT("userDirectory"), FPaths::ProjectUserDir());
    Report->SetNumberField(TEXT("elapsedSeconds"), FPlatformTime::Seconds() - S.Started);
    Report->SetNumberField(TEXT("globalWarmupFrames"), 120);
    Report->SetNumberField(TEXT("perCameraWarmupFrames"), 120);
    Report->SetNumberField(TEXT("perCameraWarmupSeconds"), 5);
    Report->SetNumberField(TEXT("perCameraSampleSeconds"), 8);
    Report->SetNumberField(TEXT("vsync"), CVarInt(TEXT("r.VSync")));
    Report->SetNumberField(TEXT("dynamicResolutionMode"),CVarInt(TEXT("r.DynamicRes.OperationMode")));
    if (IConsoleVariable* Percentage=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
        Report->SetNumberField(TEXT("screenPercentage"),Percentage->GetFloat());
    if (IConsoleVariable* Cap = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
        Report->SetNumberField(TEXT("fpsCap"), Cap->GetFloat());
    const TCHAR* QualityNames[] = {TEXT("sg.ViewDistanceQuality"), TEXT("sg.AntiAliasingQuality"),
        TEXT("sg.ShadowQuality"), TEXT("sg.GlobalIlluminationQuality"), TEXT("sg.ReflectionQuality"),
        TEXT("sg.PostProcessQuality"), TEXT("sg.TextureQuality"), TEXT("sg.EffectsQuality"), TEXT("sg.FoliageQuality")};
    TSharedRef<FJsonObject> Quality = MakeShared<FJsonObject>();
    for (const TCHAR* Name : QualityNames) Quality->SetNumberField(Name, CVarInt(Name));
    Report->SetObjectField(TEXT("quality"), Quality);
    if (World && World->GetGameViewport() && World->GetGameViewport()->Viewport)
    {
        const FIntPoint Size = World->GetGameViewport()->Viewport->GetSizeXY();
        Report->SetNumberField(TEXT("renderWidth"), Size.X);
        Report->SetNumberField(TEXT("renderHeight"), Size.Y);
    }
    Report->SetArrayField(TEXT("floorAndRailChecks"), S.CollisionResults);
    Report->SetArrayField(TEXT("musicChecks"), S.AudioResults);
    Report->SetArrayField(TEXT("lightChecks"), S.LightResults);
    Report->SetArrayField(TEXT("framePacing"), S.PerformanceResults);
    Report->SetNumberField(TEXT("fadeOutIntermediateFrames"),S.IntermediateFadeSamples);
    Report->SetNumberField(TEXT("fadeInIntermediateFrames"),S.IntermediateFadeInSamples);
    Report->SetNumberField(TEXT("movingBeamAngleChangeDegrees"),S.MaximumBeamRotationChange);
    Report->SetNumberField(TEXT("movingBeamColorChange"),S.MaximumBeamColorChange);
    Report->SetNumberField(TEXT("observedStrobeFrames"),S.ObservedStrobeFrames);
    Report->SetNumberField(TEXT("maximumObservedStrobeIntensity"),S.MaximumObservedStrobe);
    if (S.Club.IsValid())
    {
        USoundWave* Wave = Cast<USoundWave>(S.Club->MusicLoop);
        Report->SetStringField(TEXT("musicAsset"), GetPathNameSafe(Wave));
        Report->SetNumberField(TEXT("musicChannels"), Wave ? Wave->NumChannels : 0);
        Report->SetBoolField(TEXT("musicAssetLoops"), Wave && Wave->bLooping);
        Report->SetNumberField(TEXT("musicDurationSeconds"), Wave ? Wave->GetDuration() : 0);
        Report->SetNumberField(TEXT("musicLoopPeriodSeconds"), S.Club->MusicLoopSeconds);
        Report->SetNumberField(TEXT("settledAudioPhaseToleranceSeconds"), SettledAudioPhaseToleranceSeconds);
        Report->SetNumberField(TEXT("nativeAudioPlaybackSamples"), S.Club->DebugAudioNativeSampleCount);
        Report->SetNumberField(TEXT("audioClockResyncCount"), S.Club->DebugAudioResyncCount);
        Report->SetNumberField(TEXT("lastAudioResyncErrorSeconds"), S.Club->DebugLastAudioResyncError);
        Report->SetNumberField(TEXT("musicBPM"), S.Club->MusicBPM);
        Report->SetNumberField(TEXT("activeRealLightLimit"), 3);
        Report->SetNumberField(TEXT("totalRealClubLightLimit"), 6);
        Report->SetNumberField(TEXT("maximumStrobeIntensity"), S.Club->StrobeLightIntensity);
    }
    FString Text;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text));
    FFileHelper::SaveStringToFile(Text, *(S.Directory / TEXT("verification.json")));
}
}

void TickNightClubSmoke(UWorld* World)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("NightClubSmoke"))) return;
    static FClubSmokeState S;
    if (!World || S.Stage < 0 || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    APlayerController* PC = World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = PC ? Cast<AStoryFirstPersonCharacter>(PC->GetPawn()) : nullptr;
    if (!Pawn || !Pawn->FirstPersonCamera || !World->GetGameViewport()) return;
    const double Now = FPlatformTime::Seconds();
    if (!S.Started)
    {
        S.Started = S.Changed = Now;
        S.WarmFrame = GFrameCounter;
        S.Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("NightClub"));
        IFileManager::Get().MakeDirectory(*S.Directory, true);
        if (AGameModeBase* Mode = World->GetAuthGameMode()) Mode->SetActorTickInterval(0.f);
    }
    auto Fail = [&](const FString& Why)
    {
        UE_LOG(LogTemp, Error, TEXT("NIGHT_CLUB_SMOKE_FAILED: stage=%d %s"), S.Stage, *Why);
        PutReport(S, TEXT("FAIL"), Why, World);
        S.Stage = -1;
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };
    if (Now - S.Started > 480.) { Fail(TEXT("Bounded rendered test timed out")); return; }
    auto SetCamera = [&](const FVector& P, const FVector& Target)
    {
        // Use the game's actual first-person owner so the render includes its
        // normal hands and hides its own third-person head, just like gameplay.
        const FVector CameraOffset = Pawn->FirstPersonCamera->GetComponentLocation() - Pawn->GetActorLocation();
        Pawn->SetActorLocation(P - CameraOffset, false, nullptr, ETeleportType::TeleportPhysics);
        PC->SetControlRotation((Target - P).Rotation());
    };
    auto RecordAudio = [&](const FString& Name, const FVector& P, bool Audible)
    {
        AHouseNightClub* Club = S.Club.Get();
        TSharedRef<FJsonObject> Result = PositionRecord(Name, P);
        const float Target = Club->GetMusicGainAtWorld(P);
        const bool Playing = Club->LocalMusic && Club->LocalMusic->IsPlaying();
        const float Volume = Club->LocalMusic ? Club->LocalMusic->VolumeMultiplier : -1.f;
        const float SharedPhase = Club->GetLoopPhaseSeconds();
        const float PlaybackPhase = Club->GetAudioPlaybackPhaseSeconds();
        const float RawPhaseDifference = FMath::Abs(PlaybackPhase - SharedPhase);
        const float PhaseError = FMath::Min(RawPhaseDifference,
            FMath::Max(0.f, Club->MusicLoopSeconds - RawPhaseDifference));
        const bool NativePhasePassed = Club->DebugAudioNativeSampleCount > 0
            && Club->DebugAudioUsesNativeCursor && FMath::IsFinite(PlaybackPhase)
            && PlaybackPhase >= 0.f && PlaybackPhase < Club->MusicLoopSeconds
            && PhaseError <= SettledAudioPhaseToleranceSeconds;
        Result->SetNumberField(TEXT("targetGain"), Target);
        Result->SetNumberField(TEXT("actualListenerGain"), Club->DebugListenerGain);
        Result->SetNumberField(TEXT("actualAudioVolume"), Volume);
        Result->SetBoolField(TEXT("audioComponentPlaying"), Playing);
        Result->SetNumberField(TEXT("loopOffsetSeconds"), SharedPhase);
        Result->SetNumberField(TEXT("actualMixerPlaybackPhaseSeconds"), PlaybackPhase);
        Result->SetNumberField(TEXT("actualMixerPhaseErrorSeconds"), PhaseError);
        Result->SetNumberField(TEXT("cachedMixerPhaseErrorSeconds"), Club->DebugAudioPhaseError);
        Result->SetNumberField(TEXT("nativePlaybackSampleCount"), Club->DebugAudioNativeSampleCount);
        Result->SetBoolField(TEXT("usesFreshNativePlaybackCursor"), Club->DebugAudioUsesNativeCursor);
        Result->SetBoolField(TEXT("nativePlaybackPhaseRequired"), Audible);
        Result->SetBoolField(TEXT("nativePlaybackPhasePassed"), NativePhasePassed);
        Result->SetNumberField(TEXT("clockResyncCount"), Club->DebugAudioResyncCount);
        Result->SetNumberField(TEXT("lastClockResyncErrorSeconds"), Club->DebugLastAudioResyncError);
        Result->SetNumberField(TEXT("startCount"), Club->DebugAudioStartCount);
        Result->SetNumberField(TEXT("lastStartOffsetSeconds"), Club->DebugLastAudioStartOffset);
        const bool Passed = Audible ? Target > .95f && Club->DebugListenerGain > .90f && Playing && Volume > .01f && NativePhasePassed
            : Target < .0001f && Club->DebugListenerGain < .0001f && Volume < .0001f && !Playing;
        Result->SetBoolField(TEXT("passed"), Passed);
        S.AudioResults.Add(MakeShared<FJsonValueObject>(Result));
        UE_LOG(LogTemp, Display, TEXT("NIGHT_CLUB_AUDIO: %s passed=%d target=%.3f gain=%.3f volume=%.3f playing=%d expected=%.3f audible=%.3f phaseError=%.3f native=%d samples=%d resyncs=%d"),
            *Name, Passed, Target, Club->DebugListenerGain, Volume, Playing, SharedPhase, PlaybackPhase,
            PhaseError, Club->DebugAudioUsesNativeCursor, Club->DebugAudioNativeSampleCount, Club->DebugAudioResyncCount);
        return Passed;
    };
    auto CheckLights = [&](const FString& Name, bool Inside)
    {
        AHouseNightClub* Club = S.Club.Get();
        bool Passed = Club->bClubEffectsEnabled && Club->ClubLights.Num() == 6
            && Club->DebugActiveLightCount <= 3 && Club->DebugActiveLightCount == (Inside ? 3 : 0);
        int32 Visible = 0;
        for (USpotLightComponent* Light : Club->ClubLights)
        {
            if (!Light) { Passed = false; continue; }
            if (Light->IsVisible()) ++Visible;
            if (Light->CastShadows || Light->IndirectLightingIntensity > .001f || Light->VolumetricScatteringIntensity > .001f)
                Passed = false;
        }
        Passed = Passed && Visible == (Inside ? 3 : 0) && Club->StrobeLightIntensity <= 65.f;
        int32 VisibleFixtures = 0, VisibleMounts = 0, VisibleBeams = 0;
        bool SafeVisualMeshes = Club->ClubFixtures.Num() == 6 && Club->ClubFixtureMounts.Num() == 6
            && Club->ClubBeams.Num() == 4;
        auto CheckVisualMeshes = [&](const TArray<TObjectPtr<UStaticMeshComponent>>& Meshes, int32& VisibleCount)
        {
            for (UStaticMeshComponent* Mesh : Meshes)
            {
                if (!Mesh) { SafeVisualMeshes = false; continue; }
                if (Mesh->IsVisible() && !Mesh->bHiddenInGame && Mesh->GetStaticMesh()) ++VisibleCount;
                if (Mesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision || Mesh->CastShadow
                    || Mesh->CanEverAffectNavigation() || Mesh->bAffectDynamicIndirectLighting
                    || Mesh->bAffectDistanceFieldLighting) SafeVisualMeshes = false;
            }
        };
        CheckVisualMeshes(Club->ClubFixtures, VisibleFixtures);
        CheckVisualMeshes(Club->ClubFixtureMounts, VisibleMounts);
        CheckVisualMeshes(Club->ClubBeams, VisibleBeams);
        Passed = Passed && SafeVisualMeshes && VisibleFixtures == (Inside ? 3 : 0)
            && VisibleMounts == (Inside ? 3 : 0) && VisibleBeams == (Inside ? 2 : 0)
            && Club->DebugActiveFixtureCount == VisibleFixtures && Club->DebugActiveMountCount == VisibleMounts
            && Club->DebugActiveBeamCount == VisibleBeams;
        int32 ActiveProcesses=0,EffectiveProcesses=0;
        const FVector Listener=PC->PlayerCameraManager?PC->PlayerCameraManager->GetCameraLocation():Pawn->FirstPersonCamera->GetComponentLocation();
        Passed=Passed && Club->ClubPostProcesses.Num()==2 && Club->ClubPostProcessBounds.Num()==2
            && Club->DebugActivePostProcessCount==(Inside?1:0);
        for(UPostProcessComponent* Process:Club->ClubPostProcesses)
        {
            if(!Process){Passed=false;continue;}
            const FPostProcessVolumeProperties Properties=Process->GetProperties();
            if(Process->bUnbound || Properties.bIsUnbound)Passed=false;
            if(Process->bEnabled)++ActiveProcesses;
            if(Properties.bIsEnabled)
            {
                float Distance=FLT_MAX;
                if(Process->EncompassesPoint(Listener,0.f,&Distance)&&Distance<=.01f)++EffectiveProcesses;
                else Passed=false;
            }
        }
        Passed=Passed && ActiveProcesses==(Inside?1:0) && EffectiveProcesses==(Inside?1:0);
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("name"), Name);
        Result->SetBoolField(TEXT("passed"), Passed);
        Result->SetNumberField(TEXT("visibleClubLights"), Visible);
        Result->SetNumberField(TEXT("diagnosticActiveLights"), Club->DebugActiveLightCount);
        Result->SetNumberField(TEXT("visibleClubFixtures"), VisibleFixtures);
        Result->SetNumberField(TEXT("visibleClubFixtureMounts"), VisibleMounts);
        Result->SetNumberField(TEXT("visibleClubBeams"), VisibleBeams);
        Result->SetNumberField(TEXT("diagnosticActiveFixtures"), Club->DebugActiveFixtureCount);
        Result->SetNumberField(TEXT("diagnosticActiveMounts"), Club->DebugActiveMountCount);
        Result->SetNumberField(TEXT("diagnosticActiveBeams"), Club->DebugActiveBeamCount);
        Result->SetBoolField(TEXT("visualMeshesHaveNoCollisionShadowsNavigationOrGI"), SafeVisualMeshes);
        Result->SetNumberField(TEXT("strobeIntensityAtSample"), Club->DebugStrobeIntensity);
        Result->SetNumberField(TEXT("activeClubPostProcesses"),ActiveProcesses);
        Result->SetNumberField(TEXT("effectiveClubPostProcesses"),EffectiveProcesses);
        Result->SetNumberField(TEXT("diagnosticActivePostProcesses"),Club->DebugActivePostProcessCount);
        S.LightResults.Add(MakeShared<FJsonValueObject>(Result));
        return Passed;
    };
    auto BeginExpansionWalk = [&](int32 Room)
    {
        AHouseNightClub* Club = S.Club.Get();
        const float LocalFloor = Room == 0 ? Club->LowerRoomMin.Z : Club->UpperRoomMin.Z;
        const FVector Floor = Club->GetActorTransform().TransformPosition(S.bWrapLayout
            ? FVector(-2050.f,-1390.f,LocalFloor) : FVector(-1550.f,-1235.f,LocalFloor));
        const FVector Destination = Club->GetActorTransform().TransformPosition(S.bWrapLayout
            ? FVector(-2370.f,-1640.f,LocalFloor+145.f) : FVector(-1100.f,-1235.f,LocalFloor+145.f));
        if(S.bWrapLayout) S.WalkTargets={FVector(-2050,-1640,LocalFloor),FVector(-2370,-1640,LocalFloor),
            FVector(-2050,-1640,LocalFloor),FVector(-2050,-1390,LocalFloor)};
        if (!Club->IsInsideClubRoomAtWorld(Destination,Room))
        { Fail(TEXT("The approved real-walk destination is not inside the added wing")); return false; }
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->SetActorEnableCollision(true);
        // One initial approach placement per floor, followed exclusively by
        // normal character movement out across the seam and back again.
        Pawn->SetActorLocation(Floor+FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3.f),
            false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation(FRotator::ZeroRotator);
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(NightClubRealWalkStart),false,Pawn);
        if (World->OverlapBlockingTestByChannel(Pawn->GetActorLocation(),FQuat::Identity,ECC_Pawn,
            Pawn->GetCapsuleComponent()->GetCollisionShape(),Query))
        { Fail(TEXT("A player cannot stand at the approved approach to the club join")); return false; }
        S.WalkRoom = Room;
        S.WalkLeg = 0;
        S.WalkSamples = S.WalkSeamCrossings = 0;
        S.WalkFloorZ = Floor.Z;
        S.WalkStart = S.WalkPrevious = Pawn->GetActorLocation();
        S.MaximumWalkStep = 0.f;
        S.WalkStarted = Now;
        S.Stage = 9;
        return true;
    };

    if (S.Stage == 0)
    {
        if (GFrameCounter - S.WarmFrame < 120 || Now - S.Changed < 5.) return;
        FString UserDir;
        if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || UserDir.IsEmpty())
        { Fail(TEXT("Opt-in check requires its own -UserDir")); return; }
        if (FParse::Param(FCommandLine::Get(), TEXT("nosound")))
        { Fail(TEXT("The actual playback check requires audio enabled")); return; }
        for (TActorIterator<AHouseNightClub> It(World); It; ++It)
        {
            if (S.Club.IsValid()) { Fail(TEXT("Duplicate music transports in the actual house")); return; }
            S.Club = *It;
        }
        if (!S.Club.IsValid()) { Fail(TEXT("The actual house has no configured club")); return; }
        AHouseNightClub* Club = S.Club.Get();
        USoundWave* Wave = Cast<USoundWave>(Club->MusicLoop);
        if (!Wave || Wave->NumChannels != 2 || !Wave->bLooping || Wave->GetDuration() < 4.f
            || !Club->LocalMusic || Club->ClubLights.Num() != 6 || !Club->bClubEffectsEnabled)
        { Fail(TEXT("Stereo loop, actual audio component, or club FX are not configured")); return; }
        const FTransform ClubTransform = Club->GetActorTransform();
        const FVector LowerMin = ClubTransform.TransformPosition(Club->LowerRoomMin);
        const FVector LowerMax = ClubTransform.TransformPosition(Club->LowerRoomMax);
        const FVector UpperMin = ClubTransform.TransformPosition(Club->UpperRoomMin);
        const FVector UpperMax = ClubTransform.TransformPosition(Club->UpperRoomMax);
        const FVector Hole = ClubTransform.TransformPosition(Club->PoleHoleCenter);
        if (!LowerMin.Equals(FVector(-2212.51019,-1509.02675,.88), 8.f)
            || !UpperMin.Equals(FVector(-2212.51019,-1509.02675,332.53), 8.f)
            || !FMath::IsNearlyEqual(Hole.X, -1935.f, 3.f) || !FMath::IsNearlyEqual(Hole.Y,-1175.f,3.f))
        { Fail(TEXT("Club is not in the approved room across Boy Wick and directly below it")); return; }
        if(Club->ClubPostProcesses.Num()!=2 || Club->ClubPostProcessBounds.Num()!=2)
        { Fail(TEXT("Both bounded club post-process components must exist"));return; }
        for(int32 Room=0;Room<2;++Room)
        {
            UPostProcessComponent* Process=Club->ClubPostProcesses[Room];
            UBoxComponent* Bounds=Club->ClubPostProcessBounds[Room];
            const FVector Center=Room==0?(LowerMin+LowerMax)*.5f:(UpperMin+UpperMax)*.5f;
            float Distance=FLT_MAX;
            bool Safe=Process && Bounds && !Process->bUnbound && !Process->GetProperties().bIsUnbound
                && Process->GetAttachParent()==Bounds && Bounds->GetCollisionEnabled()==ECollisionEnabled::QueryOnly
                && !Bounds->GetGenerateOverlapEvents() && !Bounds->CanEverAffectNavigation();
            if(Bounds)for(int32 Channel=0;Channel<int32(ECC_MAX);++Channel)
                if(Bounds->GetCollisionResponseToChannel(ECollisionChannel(Channel))!=ECR_Ignore)Safe=false;
            const bool Encompassed=Process && Process->EncompassesPoint(Center,0.f,&Distance);
            const bool Passed=Safe && Encompassed && Distance<=.01f;
            TSharedRef<FJsonObject> Result=PositionRecord(Room?TEXT("upperPostProcessRealBounds"):TEXT("lowerPostProcessRealBounds"),Center);
            Result->SetBoolField(TEXT("passed"),Passed);
            Result->SetBoolField(TEXT("encompassesActualRoomCenter"),Encompassed);
            Result->SetNumberField(TEXT("distanceToRoomCenter"),Distance);
            Result->SetBoolField(TEXT("noBlockingOverlapOrNavigation"),Safe);
            S.LightResults.Add(MakeShared<FJsonValueObject>(Result));
            UE_LOG(LogTemp,Display,TEXT("NIGHT_CLUB_POST_PROCESS_BOUNDS: room=%d passed=%d encompasses=%d distance=%.3f safeQueryOnly=%d"),Room,Passed,Encompassed,Distance,Safe);
            if(!Passed){Fail(TEXT("Club post-process does not encompass its actual room, is unbound, or affects collision/navigation"));return;}
        }
        int32 OrdinaryLights=0;
        for (TActorIterator<AActor> It(World);It;++It)
        {
            if (*It==Club) continue;
            TInlineComponentArray<ULightComponent*> Lights;
            It->GetComponents(Lights);
            for (ULightComponent* Light:Lights)
            {
                const FVector P=Light->GetComponentLocation();
                const bool InLower=Club->IsInsideClubRoomAtWorld(P,0);
                const bool InUpper=Club->IsInsideClubRoomAtWorld(P,1);
                if (!InLower && !InUpper) continue;
                ++OrdinaryLights;
                TSharedRef<FJsonObject> Result=PositionRecord(TEXT("ordinaryRoomLightKeptDim"),P);
                Result->SetStringField(TEXT("actor"),It->GetName());
                Result->SetStringField(TEXT("component"),Light->GetName());
                Result->SetNumberField(TEXT("intensity"),Light->Intensity);
                const bool Dim=!Light->IsVisible() || Light->Intensity<=20.01f;
                Result->SetBoolField(TEXT("passed"),Dim);
                S.LightResults.Add(MakeShared<FJsonValueObject>(Result));
                if (!Dim) { Fail(TEXT("An existing normal room light was left bright inside the club")); return; }
            }
        }
        if (OrdinaryLights>2) { Fail(TEXT("Too many ordinary real lights remain in the club rooms")); return; }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(NightClubFloors), false, Pawn);
        FCollisionQueryParams FloorQuery=Query;
        // Identify supporting structural floor under sofas and the DJ booth.
        // Full player/rail/pole sweeps retain every furniture collision.
        for (TActorIterator<AHouseInteractiveProp> It(World);It;++It) FloorQuery.AddIgnoredActor(*It);
        auto FloorCheck = [&](const FString& Name, const FVector& P, bool ExpectedFloor)
        {
            FHitResult Hit;
            const bool Blocked = World->LineTraceSingleByChannel(Hit, P + FVector(0,0,18),
                P - FVector(0,0,22), ECC_Pawn, FloorQuery);
            const bool Passed = ExpectedFloor ? Blocked && !Hit.bStartPenetrating && Hit.ImpactNormal.Z > .70f
                && FMath::Abs(Hit.ImpactPoint.Z - P.Z) < 8.f : !Blocked;
            TSharedRef<FJsonObject> Result = PositionRecord(Name, P);
            Result->SetBoolField(TEXT("expectedFloor"), ExpectedFloor);
            Result->SetBoolField(TEXT("passed"), Passed);
            Result->SetBoolField(TEXT("blocked"), Blocked);
            Result->SetStringField(TEXT("actor"), GetNameSafe(Hit.GetActor()));
            Result->SetStringField(TEXT("component"), GetNameSafe(Hit.GetComponent()));
            Result->SetNumberField(TEXT("hitZ"), Hit.ImpactPoint.Z);
            S.CollisionResults.Add(MakeShared<FJsonValueObject>(Result));
            UE_LOG(LogTemp, Display, TEXT("NIGHT_CLUB_FLOOR: %s expected=%d passed=%d actor=%s component=%s hitZ=%.2f"),
                *Name, ExpectedFloor, Passed, *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.ImpactPoint.Z);
            return Passed;
        };
        for (int32 Room = 0; Room < 2; ++Room)
        {
            const FVector Min = Room == 0 ? LowerMin : UpperMin;
            const FVector Max = Room == 0 ? LowerMax : UpperMax;
            for (int32 X = 0; X < 4; ++X) for (int32 Y = 0; Y < 4; ++Y)
            {
                FVector P(FMath::Lerp(Min.X + 55., Max.X - 55., X / 3.),
                    FMath::Lerp(Min.Y + 55., Max.Y - 55., Y / 3.), Min.Z);
                if (Room == 1 && FVector2D(P.X - Hole.X, P.Y - Hole.Y).Size() < 115.f) continue;
                if (!FloorCheck(FString::Printf(TEXT("%sGrid%d_%d"), Room ? TEXT("upper") : TEXT("lower"), X,Y),P,true))
                { Fail(TEXT("Club floor has a collision gap outside its pole hole")); return; }
            }
        }
        for (int32 Index = 0; Index < 8; ++Index)
        {
            const float Angle = Index * PI * .25f;
            const FVector P(Hole.X + FMath::Cos(Angle) * 160.f, Hole.Y + FMath::Sin(Angle) * 160.f, UpperMin.Z);
            if (!FloorCheck(FString::Printf(TEXT("upperHoleRing%d"),Index),P,true))
            { Fail(TEXT("Unsafe gap around the upper pole hole")); return; }
        }
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float Angle = Index * PI * .5f + PI * .25f;
            const FVector P(Hole.X + FMath::Cos(Angle)*42.f,Hole.Y+FMath::Sin(Angle)*42.f,UpperMin.Z);
            if (!FloorCheck(FString::Printf(TEXT("upperActualHole%d"),Index),P,false))
            { Fail(TEXT("The round pole opening is still blocked by the old slab collision")); return; }
        }
        // The gate entrance faces +Y. The other three sides must physically
        // stop a real player-sized capsule at the annular safety rail.
        const FVector RailDirections[] = {FVector(1,0,0), FVector(-1,0,0), FVector(0,-1,0)};
        const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        S.bWrapLayout = !Club->LowerAdditionalDoorways.IsEmpty();
        S.bExpanded = !Club->LowerExtraRegions.IsEmpty() || !Club->UpperExtraRegions.IsEmpty();
        FVector EnclosingMaximum = LowerMax;
        if(S.bWrapLayout)
        {
            if(!Club->UpperExtraRegions.IsEmpty() || Club->LowerExtraRegions.Num()!=2)
            {Fail(TEXT("The wrap layout must have one original upstairs room and two lower extension volumes"));return;}
            for(int32 R=0;R<2;++R)
            {
                const FHouseClubRoomRegion& Region=Club->LowerExtraRegions[R];
                for(int32 X=0;X<4;++X)for(int32 Y=0;Y<4;++Y)
                {
                    const FVector P=ClubTransform.TransformPosition(FVector(
                        FMath::Lerp(Region.Minimum.X+55.,Region.Maximum.X-55.,X/3.),
                        FMath::Lerp(Region.Minimum.Y+55.,Region.Maximum.Y-55.,Y/3.),Region.Minimum.Z));
                    if(!FloorCheck(FString::Printf(TEXT("lowerWrap%dGrid%d_%d"),R,X,Y),P,true))
                    {Fail(TEXT("Missing structural floor in the former chair room or its corner join"));return;}
                }
            }
            const FVector Centers[]={FVector(-2050,-1509.02675,LowerMin.Z),FVector(-2152.04,-1640,LowerMin.Z)};
            const FVector Normals[]={FVector(0,-1,0),FVector(-1,0,0)};
            for(int32 Join=0;Join<2;++Join)for(int32 Lane=0;Lane<3;++Lane)
            {
                const FVector Side(-Normals[Join].Y,Normals[Join].X,0);
                const FVector Center=Centers[Join]+Side*((Lane-1)*30.f);
                for(float Offset:{-2.f,0.f,2.f})
                    if(!FloorCheck(FString::Printf(TEXT("wrapJoin%dLane%dFloor%+.0f"),Join,Lane,Offset),Center+Normals[Join]*Offset,true))
                    {Fail(TEXT("A floor gap remains at an internal wrap join"));return;}
                const FVector Start=Center-Normals[Join]*(Radius+35.f)+FVector(0,0,HalfHeight+3.f);
                const FVector End=Center+Normals[Join]*(Radius+35.f)+FVector(0,0,HalfHeight+3.f);
                FHitResult Hit;
                const bool Blocked=World->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Pawn,
                    FCollisionShape::MakeCapsule(Radius,HalfHeight),FloorQuery);
                const bool Passed=!Blocked && Club->GetMusicGainAtWorld(Start+FVector(0,0,55))>.95f
                    && Club->GetMusicGainAtWorld(End+FVector(0,0,55))>.95f;
                auto Result=PositionRecord(FString::Printf(TEXT("wrapJoin%dCapsuleLane%d"),Join,Lane),Start);
                Result->SetBoolField(TEXT("passed"),Passed);Result->SetStringField(TEXT("blockingActor"),GetNameSafe(Hit.GetActor()));
                S.CollisionResults.Add(MakeShared<FJsonValueObject>(Result));
                if(!Passed){Fail(TEXT("The main room and former chair room lack a clear, continuously audible player passage"));return;}
            }
            const FVector BoundaryCenters[]={FVector(-1490,-1200,LowerMin.Z),FVector(-1970,-1640,LowerMin.Z),
                FVector(-2450,-2146.65,LowerMin.Z),FVector(-2750.44,-1850,LowerMin.Z)};
            const FVector Outward[]={FVector(1,0,0),FVector(1,0,0),FVector(0,-1,0),FVector(-1,0,0)};
            for(int32 Edge=0;Edge<4;++Edge)
            {
                const FVector Center=BoundaryCenters[Edge]+FVector(0,0,HalfHeight+3.f);FHitResult Hit;
                const bool Blocked=World->SweepSingleByChannel(Hit,Center-Outward[Edge]*(Radius+35.f),
                    Center+Outward[Edge]*(Radius+35.f),FQuat::Identity,ECC_Pawn,
                    FCollisionShape::MakeCapsule(Radius,HalfHeight),FloorQuery);
                const bool Passed=Blocked&&!Hit.bStartPenetrating;
                auto Result=PositionRecord(FString::Printf(TEXT("wrapPerimeter%d"),Edge),Center);Result->SetBoolField(TEXT("passed"),Passed);
                Result->SetStringField(TEXT("actor"),GetNameSafe(Hit.GetActor()));S.CollisionResults.Add(MakeShared<FJsonValueObject>(Result));
                if(!Passed){Fail(TEXT("A restored or reused club perimeter wall does not block the player"));return;}
            }
            for(int32 Room=0;Room<2;++Room)
            {
                const FVector P=ClubTransform.TransformPosition(FVector(-1110,-1230,(Room?Club->UpperRoomMin.Z:Club->LowerRoomMin.Z)+145.f));
                if(Club->IsInsideClubRoomAtWorld(P,Room)||Club->GetMusicGainAtWorld(P)>.0001f)
                {Fail(TEXT("The removed exterior annex remains an audible club volume"));return;}
                S.ExcludedFootprintCameras.Add({Room?TEXT("RemovedUpperAnnexSilent"):TEXT("RemovedLowerAnnexSilent"),P,P+FVector(-200,0,0),false});
            }
            const auto& Entry=Club->LowerAdditionalDoorways[0];float Previous=-1.f;
            for(float Depth:{-15.f,0.f,15.f,35.f,65.f,100.f,125.f})
            {
                FVector P=Entry.Center+Entry.InwardNormal*Depth;P.Z=Club->LowerRoomMin.Z+145.f;
                const float Gain=Club->GetMusicGainAtWorld(ClubTransform.TransformPosition(P));
                if(Gain+0.001f<Previous || (Depth<=0 && Gain>.0001f) || (Depth>=100 && Gain<.99f))
                {Fail(TEXT("The second downstairs entry does not fade only on the inside of its doorway"));return;}
                auto Result=PositionRecord(TEXT("loungeEntrySpatialFade"),P);Result->SetBoolField(TEXT("passed"),true);
                Result->SetNumberField(TEXT("targetGain"),Gain);S.AudioResults.Add(MakeShared<FJsonValueObject>(Result));Previous=Gain;
            }
        }
        else
        {
        for (int32 Room = 0; Room < 2; ++Room)
        {
            const TArray<FHouseClubRoomRegion>& Regions = Room == 0 ? Club->LowerExtraRegions : Club->UpperExtraRegions;
            const FVector MainMin = Room == 0 ? LowerMin : UpperMin;
            const FVector MainMax = Room == 0 ? LowerMax : UpperMax;
            for (int32 RegionIndex = 0; RegionIndex < Regions.Num(); ++RegionIndex)
            {
                const FVector Min = ClubTransform.TransformPosition(Regions[RegionIndex].Minimum);
                const FVector Max = ClubTransform.TransformPosition(Regions[RegionIndex].Maximum);
                if (Max.X - Min.X <= 100.f || Max.Y - Min.Y <= 100.f || Max.Z - Min.Z <= 124.f)
                { Fail(TEXT("An authored extra room volume is invalid or too narrow for the expansion audit")); return; }
                if (Room == 0) EnclosingMaximum.X = FMath::Max(EnclosingMaximum.X, Max.X);
                for (int32 X = 0; X < 4; ++X) for (int32 Y = 0; Y < 4; ++Y)
                {
                    const FVector P(FMath::Lerp(Min.X + 55., Max.X - 55., X / 3.),
                        FMath::Lerp(Min.Y + 55., Max.Y - 55., Y / 3.), Min.Z);
                    if (!FloorCheck(FString::Printf(TEXT("%sWing%dGrid%d_%d"), Room ? TEXT("upper") : TEXT("lower"),RegionIndex,X,Y),P,true))
                    { Fail(TEXT("The added club wing has a missing or uneven structural floor")); return; }
                }
                // The approved wing joins the old east edge. Sweep an actual
                // player capsule across three clear structural lanes and
                // query floor just before/on/after the slab join.
                const float SeamX = MainMax.X;
                const float SeamMinY = FMath::Max(MainMin.Y, Min.Y);
                const float SeamMaxY = FMath::Min(MainMax.Y, Max.Y);
                if (!FMath::IsNearlyEqual(Min.X, SeamX, 3.f) || SeamMaxY - SeamMinY < 100.f)
                { Fail(TEXT("The new wing does not join the approved east edge of its original club room")); return; }
                for (int32 Lane = 0; Lane < 3; ++Lane)
                {
                    const float Y = FMath::Lerp(SeamMinY + 70.f, SeamMaxY - 70.f, Lane / 2.f);
                    for (float Offset : {-2.f, 0.f, 2.f})
                        if (!FloorCheck(FString::Printf(TEXT("%sWing%dSeam%d_%+.0f"),Room?TEXT("upper"):TEXT("lower"),RegionIndex,Lane,Offset),
                            FVector(SeamX+Offset,Y,Min.Z),true))
                        { Fail(TEXT("A floor collision gap remains at the old/new club join")); return; }
                    FHitResult Hit;
                    const FVector Start(SeamX - Radius - 35.f,Y,Min.Z + HalfHeight + 3.f);
                    const FVector End(SeamX + Radius + 35.f,Y,Start.Z);
                    const bool Blocked = World->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Pawn,
                        FCollisionShape::MakeCapsule(Radius,HalfHeight),FloorQuery);
                    const float StartGain = Club->GetMusicGainAtWorld(Start+FVector(0,0,55.f));
                    const float EndGain = Club->GetMusicGainAtWorld(End+FVector(0,0,55.f));
                    TSharedRef<FJsonObject> Result = PositionRecord(FString::Printf(TEXT("%sWing%dClearSeamLane%d"),
                        Room?TEXT("upper"):TEXT("lower"),RegionIndex,Lane),Start);
                    const bool Passed = !Blocked && StartGain > .95f && EndGain > .95f;
                    Result->SetBoolField(TEXT("passed"),Passed);
                    Result->SetBoolField(TEXT("capsuleBlocked"),Blocked);
                    Result->SetNumberField(TEXT("mainSideMusicGain"),StartGain);
                    Result->SetNumberField(TEXT("wingSideMusicGain"),EndGain);
                    Result->SetStringField(TEXT("blockingActor"),GetNameSafe(Hit.GetActor()));
                    S.CollisionResults.Add(MakeShared<FJsonValueObject>(Result));
                    if (!Passed) { Fail(TEXT("A structural wall or unwanted audio fade remains across the club join")); return; }
                }
                const FVector EdgePoints[] = {FVector(Max.X,(Min.Y+Max.Y)*.5f,Min.Z),
                    FVector((Min.X+Max.X)*.5f,Max.Y,Min.Z)};
                const FVector EdgeNormals[] = {FVector(1,0,0),FVector(0,1,0)};
                for (int32 Edge = 0; Edge < 2; ++Edge)
                {
                    const FVector Center = EdgePoints[Edge] + FVector(0,0,HalfHeight+3.f);
                    FHitResult Hit;
                    const bool Blocked = World->SweepSingleByChannel(Hit,Center-EdgeNormals[Edge]*(Radius+35.f),
                        Center+EdgeNormals[Edge]*(Radius+35.f),FQuat::Identity,ECC_Pawn,
                        FCollisionShape::MakeCapsule(Radius,HalfHeight),FloorQuery);
                    const bool Passed = Blocked && !Hit.bStartPenetrating;
                    TSharedRef<FJsonObject> Result = PositionRecord(FString::Printf(TEXT("%sWing%dBoundary%d"),
                        Room?TEXT("upper"):TEXT("lower"),RegionIndex,Edge),Center);
                    Result->SetBoolField(TEXT("passed"),Passed);
                    Result->SetStringField(TEXT("actor"),GetNameSafe(Hit.GetActor()));
                    Result->SetStringField(TEXT("component"),GetNameSafe(Hit.GetComponent()));
                    S.CollisionResults.Add(MakeShared<FJsonValueObject>(Result));
                    if (!Passed) { Fail(TEXT("The new wing or excluded notch is missing a solid player barrier")); return; }
                }
            }
            if (!Regions.IsEmpty())
            {
                const FVector Notch = ClubTransform.TransformPosition(FVector(-1250.f,-1000.f,
                    (Room == 0 ? Club->LowerRoomMin.Z : Club->UpperRoomMin.Z)+145.f));
                if (Club->IsInsideClubRoomAtWorld(Notch,Room) || Club->GetMusicGainAtWorld(Notch) > .0001f)
                { Fail(TEXT("The stepped club's excluded corner was included in its audible footprint")); return; }
                S.ExcludedFootprintCameras.Add({Room?TEXT("UpstairsExcludedNotch"):TEXT("DownstairsExcludedNotch"),
                    Notch,Notch+FVector(-100,-100,-30),false});
            }
        }
        }
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const FVector Center(Hole.X,Hole.Y,UpperMin.Z+HalfHeight+2.f);
            FHitResult Hit;
            const bool Blocked = World->SweepSingleByChannel(Hit,Center+RailDirections[Index]*175.f,
                Center+RailDirections[Index]*60.f,FQuat::Identity,ECC_Pawn,
                FCollisionShape::MakeCapsule(Radius,HalfHeight),Query);
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("name"),FString::Printf(TEXT("safetyRailSide%d"),Index));
            Result->SetBoolField(TEXT("passed"),Blocked && !Hit.bStartPenetrating);
            Result->SetStringField(TEXT("actor"),GetNameSafe(Hit.GetActor()));
            Result->SetStringField(TEXT("component"),GetNameSafe(Hit.GetComponent()));
            S.CollisionResults.Add(MakeShared<FJsonValueObject>(Result));
            if (!Blocked || Hit.bStartPenetrating) { Fail(TEXT("The upper safety rail is missing working collision")); return; }
        }
        // Strict mathematical room isolation includes the room directly across
        // the hallway, unchanged 101, and outside every wall at both elevations.
        for (const FVector& P : {HouseWorldScale::Point(World,FVector(-2750,-1050,145)),
            FVector(-2966,-1188,485), FVector(EnclosingMaximum.X+85.f,(LowerMin.Y+LowerMax.Y)*.5f,LowerMin.Z+145.f), FVector(-1935,-1550,155),
            FVector(-1935,-900,485), FVector(-2280,-1250,485)})
            if (Club->GetMusicGainAtWorld(P) > .0001f) { Fail(FString::Printf(TEXT("Club music leaks outside the current room bounds at %s"),*P.ToCompactString())); return; }
        const FVector HoleListener(Hole.X+25.f,Hole.Y,(LowerMax.Z+UpperMin.Z)*.5f);
        const float HoleGain = Club->GetMusicGainAtWorld(HoleListener);
        if (HoleGain <= .001f || HoleGain > .25f) { Fail(TEXT("The pole-hole bridge lacks its faint shared music gain")); return; }
        TSharedRef<FJsonObject> HoleAudio = PositionRecord(TEXT("faintPoleHoleMusic"),HoleListener);
        HoleAudio->SetNumberField(TEXT("targetGain"),HoleGain);
        HoleAudio->SetBoolField(TEXT("passed"),true);
        S.AudioResults.Add(MakeShared<FJsonValueObject>(HoleAudio));
        // Benchmark settings are local to this fresh test UserDir. Normal
        // player settings and the campaign level are never saved or changed.
        if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
        {
            Settings->SetOverallScalabilityLevel(1);
            Settings->SetFrameRateLimit(0.f);
            Settings->SetVSyncEnabled(false);
            Settings->ApplyNonResolutionSettings();
        }
        if (IConsoleVariable* Cap = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))) Cap->Set(0.f,ECVF_SetByCode);
        if (IConsoleVariable* VSync = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))) VSync->Set(0,ECVF_SetByCode);
        Pawn->GetCharacterMovement()->DisableMovement();
        Pawn->SetActorEnableCollision(false);
        S.Pawn = Pawn;
        S.AudioDoor = ClubTransform.TransformPosition(Club->LowerDoorCenter);
        S.AudioNormal = ClubTransform.TransformVectorNoScale(Club->LowerDoorInwardNormal).GetSafeNormal2D();
        S.AudioDoor.Z = LowerMin.Z + 145.f;
        const FVector P = S.AudioDoor+S.AudioNormal*125.f;
        SetCamera(P,P+S.AudioNormal*300.f);
        S.Changed = Now;
        S.Stage = 1;
        return;
    }
    if (!S.Club.IsValid()) { Fail(TEXT("Club actor disappeared during audit")); return; }
    if (S.Stage == 1)
    {
        if (S.Club->DebugListenerGain>.03f && S.Club->DebugListenerGain<.97f) ++S.IntermediateFadeInSamples;
        // Let the new mixer voice publish its native cursor and settle after
        // entering. This does not change the separate frame-rate protocol.
        if (Now-S.Changed < 1.2) return;
        const FVector P = S.AudioDoor+S.AudioNormal*125.f;
        if (!RecordAudio(TEXT("lowerDoorInterior"),P,true) || !CheckLights(TEXT("lowerInteriorBudget"),true))
        { Fail(TEXT("Downstairs music playback or real-light budget failed")); return; }
        S.Changed=Now;
        S.PreviousGain=S.Club->DebugListenerGain;
        S.Stage=2;
        return;
    }
    if (S.Stage == 2)
    {
        const float Alpha=FMath::Clamp(float((Now-S.Changed)/2.5),0.f,1.f);
        const float Depth=FMath::Lerp(125.f,-10.f,Alpha);
        const FVector P=S.AudioDoor+S.AudioNormal*Depth;
        SetCamera(P,P+S.AudioNormal*300.f);
        const float Gain=S.Club->DebugListenerGain;
        if (Gain>.03f && Gain<.97f) ++S.IntermediateFadeSamples;
        if (S.AudioStep++ % 5 == 0)
        {
            TSharedRef<FJsonObject> Result=PositionRecord(TEXT("walkingDoorFade"),P);
            Result->SetNumberField(TEXT("depthInsideDoorCm"),Depth);
            Result->SetNumberField(TEXT("actualListenerGain"),Gain);
            Result->SetNumberField(TEXT("audioVolume"),S.Club->LocalMusic->VolumeMultiplier);
            S.AudioResults.Add(MakeShared<FJsonValueObject>(Result));
        }
        S.PreviousGain=Gain;
        if (Alpha<1.f) return;
        if (S.IntermediateFadeSamples<3) { Fail(TEXT("Music crossed the doorway without a gradual audible fade")); return; }
        S.Changed=Now; S.Stage=3;
        return;
    }
    if (S.Stage == 3)
    {
        if (Now-S.Changed<.5) return;
        const FVector P=S.AudioDoor-S.AudioNormal*10.f;
        if (!RecordAudio(TEXT("hallwaySilent"),P,false) || !CheckLights(TEXT("hallwayClubLightsOff"),false))
        { Fail(TEXT("Club music or real lights remain active in the hallway")); return; }
        if(S.bCheckingAdditionalDoor)
        {
            const FVector Inside=S.AudioDoor+S.AudioNormal*125.f;SetCamera(Inside,Inside+S.AudioNormal*300.f);
            S.Changed=Now;S.Stage=11;return;
        }
        const FVector BoywickCamera(-2966,-1188,477.53);
        SetCamera(BoywickCamera,BoywickCamera+FVector(-200,100,-30));
        S.Changed=Now; S.Stage=8;
        return;
    }
    if (S.Stage == 11)
    {
        if(S.Club->DebugListenerGain>.03f&&S.Club->DebugListenerGain<.97f)++S.AdditionalEntryFadeInFrames;
        if(Now-S.Changed<1.2)return;
        const FVector P=S.AudioDoor+S.AudioNormal*125.f;
        if(S.AdditionalEntryFadeInFrames<1 || !RecordAudio(TEXT("loungeEntryActualFadeBackIn"),P,true)
            || !CheckLights(TEXT("loungeEntryActualEffects"),true))
        {Fail(TEXT("The former chair-room doorway does not fade the actual mixer voice back in"));return;}
        S.bCheckingAdditionalDoor=false;
        const FClubCamera& Camera=S.ExcludedFootprintCameras[0];SetCamera(Camera.Position,Camera.Target);
        S.ExcludedCameraIndex=0;S.Changed=Now;S.Stage=10;return;
    }
    if (S.Stage == 8)
    {
        if (Now-S.Changed<.6) return;
        const FVector P=Pawn->FirstPersonCamera->GetComponentLocation();
        if (!RecordAudio(TEXT("BoyWickRoomSilent"),P,false) || !CheckLights(TEXT("BoyWickRoomClubLightsOff"),false))
        { Fail(TEXT("Music or club lights leak into Boy Wick's protected room")); return; }
        const FTransform T=S.Club->GetActorTransform();
        const FVector Min=T.TransformPosition(S.Club->UpperRoomMin), Max=T.TransformPosition(S.Club->UpperRoomMax);
        const FVector PUpper(Min.X+(Max.X-Min.X)*.74f,Min.Y+(Max.Y-Min.Y)*.75f,Min.Z+145.f);
        SetCamera(PUpper,FVector(-1935,-1175,Min.Z+80));
        S.Changed=Now; S.Stage=4;
        return;
    }
    if (S.Stage == 4)
    {
        if (S.Club->DebugListenerGain>.03f && S.Club->DebugListenerGain<.97f) ++S.IntermediateFadeInSamples;
        if (Now-S.Changed<1.2) return;
        const FVector P=Pawn->FirstPersonCamera->GetComponentLocation();
        if (!RecordAudio(TEXT("upperRoomSharedSong"),P,true) || !CheckLights(TEXT("upperInteriorBudget"),true))
        { Fail(TEXT("Upstairs music playback or real-light budget failed")); return; }
        AHouseNightClub* Club=S.Club.Get();
        const float Offset=Club->DebugLastAudioStartOffset;
        const float Elapsed=World->GetTimeSeconds()-Club->MusicEpoch;
        if (Club->DebugAudioStartCount<2 || Offset<=.05f || Offset>Club->MusicLoopSeconds)
        { Fail(TEXT("Entering upstairs restarted the song at zero rather than its shared transport phase")); return; }
        if (S.IntermediateFadeInSamples<1) { Fail(TEXT("Entering the room did not produce any gradual music fade-in frames")); return; }
        UE_LOG(LogTemp,Display,TEXT("NIGHT_CLUB_SHARED_TRANSPORT: restartOffset=%.3f elapsed=%.3f starts=%d"),Offset,Elapsed,Club->DebugAudioStartCount);
        for (int32 Repeat=0;Repeat<2;++Repeat)
        {
            S.Cameras.Add({FString::Printf(TEXT("NormalRoom101_%d"),Repeat+1),
                FVector(-3060,-1140,160),FVector(-2710,-800,125),false});
            S.Cameras.Add({FString::Printf(TEXT("ClubDownstairs_%d"),Repeat+1),
                FVector(-1695,-1410,155),FVector(-2050,-1130,95),true});
            S.Cameras.Add({FString::Printf(TEXT("ClubUpstairs_%d"),Repeat+1),
                FVector(-1695,-1410,487.53),FVector(-2050,-1130,427.53),true});
        }
        for (int32 Room=0;Room<2;++Room)
        {
            const TArray<FHouseClubRoomRegion>& Regions=Room==0?Club->LowerExtraRegions:Club->UpperExtraRegions;
            if(Regions.IsEmpty())continue;
            const FHouseClubRoomRegion& Region=Regions[0];
            const FVector Size=Region.Maximum-Region.Minimum;
            const FVector CameraLocal(Region.Minimum.X+Size.X*.35f,Region.Minimum.Y+Size.Y*.25f,Region.Minimum.Z+145.f);
            const FVector TargetLocal(Region.Maximum.X-55.f,Region.Minimum.Y+Size.Y*.56f,Region.Minimum.Z+90.f);
            S.Cameras.Add({Room?TEXT("WingUpstairsEastDJ"):TEXT("WingDownstairsEastDJ"),
                Club->GetActorTransform().TransformPosition(CameraLocal),
                Club->GetActorTransform().TransformPosition(TargetLocal),true});
        }
        if(S.bExpanded)
        {
            BeginExpansionWalk(Club->LowerExtraRegions.IsEmpty()?1:0);
            return;
        }
        const FClubCamera& Camera=S.Cameras[0];
        SetCamera(Camera.Position,Camera.Target);
        S.Changed=Now;S.ChangedFrame=GFrameCounter;S.Stage=5;
        return;
    }
    if (S.Stage == 9)
    {
        const FVector Current=Pawn->GetActorLocation();
        const FVector Local=S.Club->GetActorTransform().InverseTransformPosition(Current);
        const float Step=FVector::Distance(Current,S.WalkPrevious);
        S.MaximumWalkStep=FMath::Max(S.MaximumWalkStep,Step);
        if(Step>.05f)++S.WalkSamples;
        const FVector PreviousLocal=S.Club->GetActorTransform().InverseTransformPosition(S.WalkPrevious);
        if(S.bWrapLayout)
        {
            const bool Vertical=S.WalkLeg==0||S.WalkLeg==3;
            const float Plane=Vertical?-1509.02675f:-2152.04f;
            const float Before=(Vertical?PreviousLocal.Y:PreviousLocal.X)-Plane;
            const float After=(Vertical?Local.Y:Local.X)-Plane;
            if(Before*After<=0.f && FMath::Abs(Before-After)>.001f)++S.WalkSeamCrossings;
        }
        else if((S.WalkLeg==0 && PreviousLocal.X<-1490.f && Local.X>=-1490.f)
            || (S.WalkLeg==1 && PreviousLocal.X>-1490.f && Local.X<=-1490.f))++S.WalkSeamCrossings;
        S.WalkPrevious=Current;
        if(Now-S.WalkStarted>12.){Fail(TEXT("Normal player movement could not cross the club join within12seconds"));return;}
        const float HalfHeight=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        if(Now-S.WalkStarted>.25)
        {
            const FVector Eye=Pawn->FirstPersonCamera->GetComponentLocation();
            if(!Pawn->GetActorEnableCollision() || !Pawn->GetCharacterMovement()->IsMovingOnGround()
                || FMath::Abs(Current.Z-S.WalkFloorZ-HalfHeight)>12.f)
            {Fail(TEXT("The player fell or lost grounded collision during the actual club seam walk"));return;}
            if(S.Club->GetMusicGainAtWorld(Eye)<.95f || S.Club->DebugListenerGain<.90f
                || S.Club->DebugActiveLightCount!=3 || S.Club->DebugActivePostProcessCount!=1)
            {Fail(TEXT("The internal club join incorrectly faded music or disabled room effects during real movement"));return;}
        }
        const bool Arrived=S.bWrapLayout?FVector::Dist2D(Local,S.WalkTargets[S.WalkLeg])<8.f:
            (S.WalkLeg==0?Local.X>=-1110.f:Local.X<=-1540.f);
        if(!Arrived)
        {
            const FVector To=S.bWrapLayout?(S.WalkTargets[S.WalkLeg]-Local).GetSafeNormal2D():FVector(S.WalkLeg==0?1.f:-1.f,0,0);
            const FVector Direction=S.Club->GetActorTransform().TransformVectorNoScale(To);
            const float Input=S.bWrapLayout&&FVector::Dist2D(Local,S.WalkTargets[S.WalkLeg])<90.f?.35f:1.f;
            Pawn->AddMovementInput(Direction,Input,true);
            return;
        }
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        const FString Name=S.bWrapLayout?FString::Printf(TEXT("LowerRealWrapWalk%d"),S.WalkLeg):
            FString::Printf(TEXT("%sRealSeamWalk%s"),S.WalkRoom?TEXT("upper"):TEXT("lower"),S.WalkLeg?TEXT("Return"):TEXT("Outward"));
        const bool Passed=S.WalkSamples>=4 && S.WalkSeamCrossings>=1;
        TSharedRef<FJsonObject> Result=PositionRecord(Name,Current);
        Result->SetBoolField(TEXT("passed"),Passed);
        Result->SetBoolField(TEXT("usedAddMovementInput"),true);
        Result->SetBoolField(TEXT("playerCollisionStayedEnabled"),Pawn->GetActorEnableCollision());
        Result->SetNumberField(TEXT("movementSamples"),S.WalkSamples);
        Result->SetNumberField(TEXT("actualSeamCrossings"),S.WalkSeamCrossings);
        Result->SetNumberField(TEXT("maximumMovementStepCm"),S.MaximumWalkStep);
        Result->SetNumberField(TEXT("distanceWalkedCm"),FVector::Distance(S.WalkStart,Current));
        Result->SetNumberField(TEXT("durationSeconds"),Now-S.WalkStarted);
        S.CollisionResults.Add(MakeShared<FJsonValueObject>(Result));
        UE_LOG(LogTemp,Display,TEXT("NIGHT_CLUB_REAL_SEAM_WALK: %s passed=%d samples=%d crossings=%d start=%s end=%s seconds=%.2f"),
            *Name,Passed,S.WalkSamples,S.WalkSeamCrossings,*S.WalkStart.ToCompactString(),*Current.ToCompactString(),Now-S.WalkStarted);
        if(!Passed || !RecordAudio(Name,Pawn->FirstPersonCamera->GetComponentLocation(),true) || !CheckLights(Name,true))
        {Fail(TEXT("The actual player did not complete a grounded, audible walk across the internal club join"));return;}
        if(S.bWrapLayout?S.WalkLeg<S.WalkTargets.Num()-1:S.WalkLeg==0)
        {
            ++S.WalkLeg;S.WalkSamples=S.WalkSeamCrossings=0;
            S.WalkStart=S.WalkPrevious=Current;S.MaximumWalkStep=0.f;S.WalkStarted=Now;
            PC->SetControlRotation(FRotator(0.f,180.f,0.f));
            return;
        }
        if(S.WalkRoom==0 && !S.Club->UpperExtraRegions.IsEmpty())
        {BeginExpansionWalk(1);return;}
        Pawn->GetCharacterMovement()->DisableMovement();
        Pawn->SetActorEnableCollision(false);
        if(S.bWrapLayout)
        {
            const auto& Entry=S.Club->LowerAdditionalDoorways[0];
            FVector Door=Entry.Center;Door.Z=S.Club->LowerRoomMin.Z+145.f;
            S.AudioDoor=S.Club->GetActorTransform().TransformPosition(Door);
            S.AudioNormal=S.Club->GetActorTransform().TransformVectorNoScale(Entry.InwardNormal);
            S.bCheckingAdditionalDoor=true;S.IntermediateFadeSamples=0;S.AdditionalEntryFadeInFrames=0;
            const FVector P=S.AudioDoor+S.AudioNormal*125.f;SetCamera(P,P+S.AudioNormal*300.f);
            S.Changed=Now;S.Stage=1;return;
        }
        if(!S.ExcludedFootprintCameras.IsEmpty())
        {
            const FClubCamera& Camera=S.ExcludedFootprintCameras[0];
            SetCamera(Camera.Position,Camera.Target);
            S.ExcludedCameraIndex=0;S.Changed=Now;S.Stage=10;return;
        }
        const FClubCamera& Camera=S.Cameras[0];
        SetCamera(Camera.Position,Camera.Target);
        S.Changed=Now;S.ChangedFrame=GFrameCounter;S.Stage=5;
        return;
    }
    if (S.Stage == 10)
    {
        if(Now-S.Changed<.8)return;
        const FClubCamera& Camera=S.ExcludedFootprintCameras[S.ExcludedCameraIndex];
        if(!RecordAudio(Camera.Name,Pawn->FirstPersonCamera->GetComponentLocation(),false)
            || !CheckLights(Camera.Name,false))
        {Fail(TEXT("Music, fixture beams, real lights or club post-process leaked into the excluded notch"));return;}
        ++S.ExcludedCameraIndex;
        if(S.ExcludedCameraIndex<S.ExcludedFootprintCameras.Num())
        {
            const FClubCamera& Next=S.ExcludedFootprintCameras[S.ExcludedCameraIndex];
            SetCamera(Next.Position,Next.Target);S.Changed=Now;return;
        }
        const FClubCamera& First=S.Cameras[0];SetCamera(First.Position,First.Target);
        S.Changed=Now;S.ChangedFrame=GFrameCounter;S.Stage=5;
        return;
    }
    if (S.Stage == 5)
    {
        if (Now-S.Changed<5. || GFrameCounter-S.ChangedFrame<120) return;
        const FClubCamera& Camera=S.Cameras[S.CameraIndex];
        if (!CheckLights(Camera.Name,Camera.bClub)) { Fail(TEXT("Light culling or shadow budget differs at a benchmark camera")); return; }
        if (!RecordAudio(Camera.Name,Pawn->FirstPersonCamera->GetComponentLocation(),Camera.bClub))
        { Fail(TEXT("Actual music playback differs from the benchmark room's audible footprint")); return; }
        if (CVarInt(TEXT("sg.ShadowQuality")) != 1 || CVarInt(TEXT("sg.EffectsQuality")) != 1)
        { Fail(TEXT("Benchmark graphics quality changed between locations")); return; }
        S.FrameTimes.Reset();S.SampleStarted=S.LastFrame=Now;S.Stage=6;
        UE_LOG(LogTemp,Display,TEXT("NIGHT_CLUB_FPS_SAMPLE_START: %s warmFrames=%llu warmSeconds=%.2f"),
            *Camera.Name,GFrameCounter-S.ChangedFrame,Now-S.Changed);
        return;
    }
    if (S.Stage == 6)
    {
        if (S.Cameras[S.CameraIndex].bClub)
        {
            const int32 Base=S.Cameras[S.CameraIndex].Name.Contains(TEXT("Upstairs"))?3:0;
            const USpotLightComponent* Beam=S.Club->ClubLights[Base];
            const USpotLightComponent* Strobe=S.Club->ClubLights[Base+2];
            if (S.FrameTimes.IsEmpty())
            { S.FirstBeamRotation=Beam->GetRelativeRotation();S.FirstBeamColor=Beam->GetLightColor(); }
            S.MaximumBeamRotationChange=FMath::Max(S.MaximumBeamRotationChange,float(
                (Beam->GetRelativeRotation()-S.FirstBeamRotation).GetNormalized().GetManhattanDistance(FRotator::ZeroRotator)));
            const FLinearColor ColorDifference=Beam->GetLightColor()-S.FirstBeamColor;
            S.MaximumBeamColorChange=FMath::Max(S.MaximumBeamColorChange,
                FMath::Abs(ColorDifference.R)+FMath::Abs(ColorDifference.G)+FMath::Abs(ColorDifference.B));
            S.MaximumObservedStrobe=FMath::Max(S.MaximumObservedStrobe,Strobe->Intensity);
            if (Strobe->Intensity>.01f) ++S.ObservedStrobeFrames;
            if (Strobe->Intensity>65.01f) { Fail(TEXT("Actual strobe exceeded the soft flash intensity limit")); return; }
        }
        S.FrameTimes.Add((Now-S.LastFrame)*1000.);S.LastFrame=Now;
        if (Now-S.SampleStarted<8.) return;
        const FClubCamera& Camera=S.Cameras[S.CameraIndex];
        if (S.FrameTimes.Num()<8) { Fail(TEXT("Too few rendered frames for an honest frame-rate sample")); return; }
        if (!RecordAudio(Camera.Name+TEXT("AfterEightSeconds"),Pawn->FirstPersonCamera->GetComponentLocation(),Camera.bClub))
        { Fail(TEXT("Actual mixer playback or its shared song phase failed after the rendered sample")); return; }
        TArray<double> Sorted=S.FrameTimes;Sorted.Sort();
        const double Median=Sorted[Sorted.Num()/2];
        const double P95=Sorted[FMath::Min(Sorted.Num()-1,FMath::FloorToInt(Sorted.Num()*.95))];
        const double FPS=S.FrameTimes.Num()/(Now-S.SampleStarted);
        TSharedRef<FJsonObject> Result=PositionRecord(Camera.Name,Camera.Position);
        Result->SetNumberField(TEXT("sampleSeconds"),Now-S.SampleStarted);
        Result->SetNumberField(TEXT("frameCount"),S.FrameTimes.Num());
        Result->SetNumberField(TEXT("averageFPS"),FPS);
        Result->SetNumberField(TEXT("medianFrameMs"),Median);
        Result->SetNumberField(TEXT("p95FrameMs"),P95);
        Result->SetBoolField(TEXT("clubEffectsEnabled"),S.Club->bClubEffectsEnabled);
        Result->SetNumberField(TEXT("activeClubLights"),S.Club->DebugActiveLightCount);
        Result->SetNumberField(TEXT("actualAudioVolume"),S.Club->LocalMusic->VolumeMultiplier);
        S.PerformanceResults.Add(MakeShared<FJsonValueObject>(Result));
        UE_LOG(LogTemp,Display,TEXT("NIGHT_CLUB_FPS_RESULT: %s averageFPS=%.2f medianMs=%.2f p95Ms=%.2f frames=%d seconds=%.2f"),
            *Camera.Name,FPS,Median,P95,S.FrameTimes.Num(),Now-S.SampleStarted);
        ++S.CameraIndex;
        if (S.CameraIndex<S.Cameras.Num())
        {
            const FClubCamera& Next=S.Cameras[S.CameraIndex];SetCamera(Next.Position,Next.Target);
            S.Changed=Now;S.ChangedFrame=GFrameCounter;S.Stage=5;return;
        }
        if (S.MaximumBeamRotationChange<5.f || S.MaximumBeamColorChange<.05f || S.ObservedStrobeFrames<1)
        { Fail(TEXT("Rendered club lights did not actually move, change color, and produce their short soft strobe")); return; }
        PutReport(S,TEXT("PASS"),FString::Printf(TEXT("Actual club floors, round opening and guard collision; stereo mixer playback aligned with the shared song phase, doorway fade and strict adjacent room isolation; six shadowless real lights, three active;%d rendered frame-rate samples. Expanded rooms additionally checked with real seam walks, wing barriers and excluded-notch isolation."),S.Cameras.Num()),World);
        UE_LOG(LogTemp,Display,TEXT("NIGHT_CLUB_SMOKE_PASSED: floorChecks=%d audioChecks=%d performanceSamples=%d"),
            S.CollisionResults.Num(),S.AudioResults.Num(),S.PerformanceResults.Num());
        S.Stage=-1;FPlatformMisc::RequestExitWithStatus(false,0);
    }
}
#else
void TickNightClubSmoke(UWorld*) {}
#endif
