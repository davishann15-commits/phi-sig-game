// Opt-in checks only. No editor/map mutation, no normal-game automation.
#include "HouseClubPole.h"

#if !UE_BUILD_SHIPPING
#include "StoryCampaign.h"
#include "SeniorLobby.h"
#include "HouseNightClub.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/PlatformMisc.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundBase.h"

namespace
{
struct FClubPoleSmokeState
{
    TWeakObjectPtr<UWorld> World;
    double Started=0, PhaseStarted=0, WarmTime=0;
    uint64 WarmFrame=0;
    int32 Stage=0;
    bool Finished=false, Placed=false, SeenDown=false, SeenUp=false, BusyAttempted=false;
    bool LobbyConfigured=false,LobbyTravelStarted=false;
    FVector Scale=FVector::OneVector, LastPosition=FVector::ZeroVector;
    float Radius=0,HalfHeight=0,HighestSpeed=0;
    uint32 LastSerial=0;
    TWeakObjectPtr<AStoryFirstPersonCharacter> LastObservedPawn;
    int32 DownSamples=0,UpSamples=0;
    int32 AudioStartCountSeen=-1, MusicPlayingSamples=0, MaximumActiveLights=0;
    bool SawMusicStartPhase=false;
    float MaximumMusicStartPhaseError=0.f;
    double AmbienceDeadline=0;
    double VerifiedHostFinishAt=0;
    FString LastClubReadiness;
    double LastTickWall=0, SteadySince=0, FinalLandingDeadline=0;
    int32 SteadyFrames=0;
    bool ClientReadySent=false, FinalLandingObserved=false;
    float MaximumObservedStep=0.f;
    int32 BottomApproachPoint=0;
};

void Finish(FClubPoleSmokeState& State, bool bPass, bool bMP, UWorld* World, const FString& Why)
{
    UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_%s_%s mode=%d stage=%d downSamples=%d upSamples=%d peakSpeed=%.2f maxObservedStep=%.3f %s"),
        bMP?TEXT("MP"):TEXT("SMOKE"),bPass?TEXT("PASS"):TEXT("FAIL"),World?int32(World->GetNetMode()):-1,
        State.Stage,State.DownSamples,State.UpSamples,State.HighestSpeed,State.MaximumObservedStep,*Why);
    State.Finished=true;
    FPlatformMisc::RequestExitWithStatus(false,bPass?0:1);
}

bool Place(AStoryFirstPersonCharacter* Pawn, AHouseClubPole* Pole, bool bUpper, const FVector& Offset=FVector::ZeroVector)
{
    FVector Center=Pole->LandingWorld(bUpper)+Offset;
    Center.Z+=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ClubPoleSmokeSetup),false,Pawn);
    if (Pawn->GetWorld()->OverlapBlockingTestByChannel(Center,FQuat::Identity,ECC_Pawn,
        FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Query)) return false;
    Pawn->GetCharacterMovement()->StopMovementImmediately();
    Pawn->SetActorLocation(Center,false,nullptr,ETeleportType::TeleportPhysics);
    Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Pawn->ForceNetUpdate();
    return true;
}

bool PressE(APlayerController* PC, AStoryFirstPersonCharacter* Pawn, AHouseClubPole* Pole, bool bUpper)
{
    if (!PC || !PC->PlayerInput || PC->bShowMouseCursor) return false;
    PC->SetControlRotation((Pole->FocusWorld(bUpper)-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
    bool FocusUpper=false;
    if (Pawn->FocusedClubPole(FocusUpper)!=Pole || FocusUpper!=bUpper) return false;
    PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1.f));
    PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0.f));
    return true;
}

bool StandingAim(APlayerController* PC,AStoryFirstPersonCharacter* Pawn,AHouseClubPole* Pole,bool Upper,float YawOffset,float Pitch)
{
    if (!PC || !PC->PlayerInput || PC->bShowMouseCursor) return false;
    const FVector Shaft=Pole->GetActorTransform().TransformPosition(Pole->PoleBase);
    FRotator Aim=(Shaft-Pawn->GetActorLocation()).GetSafeNormal2D().Rotation();
    Aim.Yaw+=YawOffset;Aim.Pitch=Pitch;
    PC->SetControlRotation(Aim);
    bool FoundUpper=false;
    return Pawn->FocusedClubPole(FoundUpper)==Pole && FoundUpper==Upper;
}

bool CheckStandingFocus(APlayerController* PC,AStoryFirstPersonCharacter* Pawn,AHouseClubPole* Pole,bool Upper,FString& Why)
{
    // Keep the natural standing eye height and several ordinary aim poses.
    // No camera placement or exact FocusWorld reticle pose is used here.
    const FVector2D Aims[]={FVector2D(-28,0),FVector2D(28,0),FVector2D(0,-28),FVector2D(0,28)};
    for (const FVector2D& Aim:Aims)
        if (!StandingAim(PC,Pawn,Pole,Upper,float(Aim.X),float(Aim.Y)))
        {
            Why=FString::Printf(TEXT("natural standing %s focus unavailable yawOffset=%.1f pitch=%.1f location=%s eye=%s"),
                Upper?TEXT("upper"):TEXT("lower"),Aim.X,Aim.Y,*Pawn->GetActorLocation().ToCompactString(),
                *Pawn->FirstPersonCamera->GetComponentLocation().ToCompactString());
            Why+=TEXT(" ")+Pole->FocusDiagnostics(Pawn);
            return false;
        }
    if (StandingAim(PC,Pawn,Pole,Upper,180.f,0.f))
    { Why=TEXT("pole focus incorrectly accepted facing away: ")+Pole->FocusDiagnostics(Pawn);return false; }
    UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_NATURAL_FOCUS_PASS endpoint=%s standingAims=4 facingAwayRejected=1 location=%s eye=%s"),
        Upper?TEXT("upper"):TEXT("lower"),*Pawn->GetActorLocation().ToCompactString(),
        *Pawn->FirstPersonCamera->GetComponentLocation().ToCompactString());
    return true;
}

bool PressStandingE(APlayerController* PC,AStoryFirstPersonCharacter* Pawn,AHouseClubPole* Pole,bool Upper)
{
    if (!StandingAim(PC,Pawn,Pole,Upper,22.f,5.f)) return false;
    PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1.f));
    PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0.f));
    return true;
}

void Advance(FClubPoleSmokeState& State,int32 Stage,double Now)
{ State.Stage=Stage;State.PhaseStarted=Now; }

void Observe(FClubPoleSmokeState& State,AStoryFirstPersonCharacter* Pawn,float Delta)
{
    if (!Pawn->ClubPoleJourney.Pole) return;
    const bool Up=Pawn->ClubPoleJourney.bClimbing;
    if (Up) { State.SeenUp=true;++State.UpSamples; }
    else { State.SeenDown=true;++State.DownSamples; }
    if (State.LastObservedPawn.Get()==Pawn && State.LastSerial==Pawn->ClubPoleJourney.Serial && Delta>.001f)
    {
        State.HighestSpeed=FMath::Max(State.HighestSpeed,float(FVector::Dist(Pawn->GetActorLocation(),State.LastPosition)/Delta));
        State.MaximumObservedStep=FMath::Max(State.MaximumObservedStep,float(FVector::Dist(Pawn->GetActorLocation(),State.LastPosition)));
    }
    State.LastSerial=Pawn->ClubPoleJourney.Serial;
    State.LastObservedPawn=Pawn;
    State.LastPosition=Pawn->GetActorLocation();
}

bool LandingMatches(AStoryFirstPersonCharacter* Pawn,AHouseClubPole* Pole,bool Upper)
{
    const FVector Expected=Pole->LandingWorld(Upper)+FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f);
    return FVector::Dist(Pawn->GetActorLocation(),Expected)<7.f;
}

void TickClubPoleSmoke(UWorld* World,ELevelTick,float Delta)
{
    const bool MP=FParse::Param(FCommandLine::Get(),TEXT("ClubPoleMultiplayerSmoke"));
    if (!MP && !FParse::Param(FCommandLine::Get(),TEXT("HouseClubPoleSmoke"))) return;
    if (!World || !World->IsGameWorld()) return;
    const FString Map=UGameplayStatics::GetCurrentLevelName(World,true);
    if (Map!=TEXT("Chapter01_House") && !(MP && Map==TEXT("Lobby"))) return;
    static FClubPoleSmokeState State;
    const double Now=FPlatformTime::Seconds();
    if (State.World.Get()!=World)
    {
        State=FClubPoleSmokeState(); State.World=World;
        State.Started=State.PhaseStarted=State.WarmTime=Now; State.WarmFrame=GFrameCounter;
    }
    if (State.Finished) return;
    const bool bSteadyTick=State.LastTickWall>0 && Now-State.LastTickWall<.25 && Delta<=.25f;
    State.LastTickWall=Now;
    auto Fail=[&](const FString& Why) { Finish(State,false,MP,World,Why); };
    if (Now-State.Started>200.) { Fail(TEXT("bounded test timeout")); return; }
    if (MP && Map==TEXT("Lobby"))
    {
        // Join through the game's real lobby policy. Story's PreLogin rejects
        // late joins; this test must never introduce a production exemption.
        if (World->GetNetMode()!=NM_ListenServer || State.LobbyTravelStarted) return;
        ASeniorLobbyGameState* Lobby=World->GetGameState<ASeniorLobbyGameState>();
        if (!Lobby || Lobby->GetMembers().Num()!=2 || Now-State.Started<2.) return;
        ASeniorLobbyController* Host=nullptr;
        ASeniorLobbyController* Remote=nullptr;
        for (FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It)
            if (ASeniorLobbyController* Controller=Cast<ASeniorLobbyController>(It->Get()))
            {
                if (Controller->IsLocalController()) Host=Controller;
                else if (!Remote) Remote=Controller;
                else { Fail(TEXT("LAN smoke requires exactly one connected remote player"));return; }
            }
        if (!Host || !Remote || !Remote->HasClientLoadedCurrentWorld()) return;
        if (!State.LobbyConfigured)
        {
            Host->SetCharacter(0); Remote->SetCharacter(1);
            Host->SetReady(true); Remote->SetReady(true);
            State.LobbyConfigured=true;
            UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_MP_LOBBY_READY members=2 uniqueCharacters=2"));
        }
        if (!Lobby->CanStart() || !Host->CanStartStory()) return;
        State.LobbyTravelStarted=true;
        Host->StartStory(false);
        if (!Lobby->bStarting) Fail(TEXT("ready host could not begin normal story travel"));
        else UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_MP_LOBBY_TRAVEL normalStartStory=1"));
        return;
    }
    APlayerController* PC=nullptr;
    for (FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It)
        if (It->Get() && It->Get()->IsLocalController()) { PC=It->Get();break; }
    AStoryFirstPersonCharacter* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    if (!Pawn || !Pawn->FirstPersonCamera || !PC->PlayerInput || !World->GetGameViewport()) return;
    TActorIterator<AHouseClubPole> PoleIt(World);
    AHouseClubPole* Pole=PoleIt?*PoleIt:nullptr;
    if (!Pole) { if (Now-State.Started>20.) Fail(TEXT("saved map has no club pole"));return; }
    AHouseNightClub* Club=nullptr;
    if (MP)
    {
        int32 ClubCount=0;
        for (TActorIterator<AHouseNightClub> It(World);It;++It) { Club=*It; ++ClubCount; }
        const bool bTransportReady=Club && Club->IsTransportClockReady();
        const bool bClubReady=ClubCount==1 && Club && Club->MusicLoop && Club->LocalMusic && Club->bMusicEpochReady && bTransportReady;
        const FString Readiness=FString::Printf(TEXT("clubCount=%d actor=%s MusicLoop=%s LocalMusic=%s bEpochReady=%d epoch=%.6f begun=%d replicates=%d alwaysRelevant=%d role=%d remoteRole=%d transportReady=%d ready=%d"),
            ClubCount,*GetNameSafe(Club),*GetPathNameSafe(Club?Club->MusicLoop.Get():nullptr),
            *GetPathNameSafe(Club?Club->LocalMusic.Get():nullptr),Club && Club->bMusicEpochReady,
            Club?double(Club->MusicEpoch):0.,Club && Club->HasActorBegunPlay(),Club && Club->GetIsReplicated(),
            Club && Club->bAlwaysRelevant,Club?int32(Club->GetLocalRole()):-1,Club?int32(Club->GetRemoteRole()):-1,bTransportReady,bClubReady);
        if (Readiness!=State.LastClubReadiness)
        {
            UE_LOG(LogTemp,Display,TEXT("CLUB_AMBIENCE_MP_READINESS mode=%d elapsed=%.3f %s clockSerial=%u halfRTT=%.6f clockAge=%.6f"),
                int32(World->GetNetMode()),Now-State.Started,*Readiness,Club?uint32(Club->DebugTransportClockSerial):0u,
                Club?double(Club->DebugTransportClockHalfRTT):0.,Club?double(Club->DebugTransportClockAgeSeconds):0.);
            State.LastClubReadiness=Readiness;
        }
        if (ClubCount>1) { Fail(TEXT("multiplayer ambience has duplicate club music transports: ")+Readiness);return; }
        // Cold map/shader loading can stall both processes before their first
        // packet exchange. Keep the overall 200-second bound, and give actual
        // replicated readiness a bounded 60-second window with exact evidence.
        if (!bClubReady)
        {
            if (Now-State.Started>60.) Fail(TEXT("replicated club transport/music/epoch not ready: ")+Readiness
                +FString::Printf(TEXT(" clockSerial=%u halfRTT=%.6f clockAge=%.6f"),Club?uint32(Club->DebugTransportClockSerial):0u,
                    Club?double(Club->DebugTransportClockHalfRTT):0.,Club?double(Club->DebugTransportClockAgeSeconds):0.));
            return;
        }
        if (Club->LocalMusic->GetSound()!=Club->MusicLoop.Get())
        { Fail(TEXT("local audio component is not assigned to the club's shared song"));return; }
        const float Loop=FMath::Max(1.f,Club->MusicLoopSeconds);
        State.MaximumActiveLights=FMath::Max(State.MaximumActiveLights,Club->DebugActiveLightCount);
        if (Club->DebugActiveLightCount>3) { Fail(TEXT("more than three real lights active in the local club room"));return; }
        // Observe at the start-count transition, before the 64-frame warmup,
        // rather than comparing an old start offset to a later moving phase.
        if (Club->DebugAudioStartCount!=State.AudioStartCountSeen)
        {
            if (Club->DebugAudioStartCount>0)
            {
                const float Offset=Club->DebugLastAudioStartOffset;
                const float Phase=Club->GetLoopPhaseSeconds();
                if (!FMath::IsFinite(Offset) || Offset<0.f || Offset>=Loop)
                { Fail(TEXT("audio join offset is outside the shared loop"));return; }
                float Error=FMath::Abs(Phase-Offset);
                Error=FMath::Min(Error,Loop-Error);
                const bool SawTransition=State.AudioStartCountSeen>=0;
                // An initial already-playing observation still proves a valid
                // join offset. Epoch and current server clock are logged for
                // cross-process comparison without inventing its start time.
                State.SawMusicStartPhase=true;
                if (SawTransition)
                {
                    State.MaximumMusicStartPhaseError=FMath::Max(State.MaximumMusicStartPhaseError,Error);
                    // Both values come from this observed audio-start tick.
                    // Allow one bounded frame/replica delay, not song drift.
                    if (Error>FMath::Max(.75f,Delta*2.f))
                    { Fail(FString::Printf(TEXT("audio join did not use shared phase: offset=%.3f phase=%.3f error=%.3f"),Offset,Phase,Error));return; }
                }
                const AGameStateBase* GameState=World->GetGameState();
                UE_LOG(LogTemp,Display,TEXT("CLUB_AMBIENCE_MP_START mode=%d wall=%.6f server=%.6f epoch=%.6f phase=%.6f offset=%.6f startCount=%d phaseError=%.6f observedTransition=%d playing=%d gain=%.3f lights=%d"),
                    int32(World->GetNetMode()),Now,GameState?GameState->GetServerWorldTimeSeconds():double(World->GetTimeSeconds()),
                    Club->MusicEpoch,Phase,Offset,Club->DebugAudioStartCount,Error,SawTransition,
                    Club->LocalMusic->IsPlaying(),Club->DebugListenerGain,Club->DebugActiveLightCount);
            }
            State.AudioStartCountSeen=Club->DebugAudioStartCount;
        }
        if (Club->DebugTargetGain>.1f && Club->DebugListenerGain>.05f && Club->LocalMusic->IsPlaying())
            ++State.MusicPlayingSamples;
        // Observe received journeys immediately. Actions still wait for the
        // local warmup below, so a slower client's renderer cannot hide an
        // otherwise correctly replicated host descent from the test.
        for (TActorIterator<AStoryFirstPersonCharacter> It(World);It;++It)
            if (It->GetPlayerState()) Observe(State,*It,Delta);
    }
    if (State.Stage==0 && (GFrameCounter-State.WarmFrame<64 || Now-State.WarmTime<1.)) return;
    FString UserDir;
    if (!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir) || UserDir.IsEmpty())
    { Fail(TEXT("test requires an isolated -UserDir"));return; }

    if (MP)
    {
        if (World->GetNetMode()!=NM_ListenServer && World->GetNetMode()!=NM_Client)
        { Fail(TEXT("LAN test requires a real listen server and a separate connected client"));return; }
        const bool Host=World->GetNetMode()==NM_ListenServer;
        // Cache the host's fully validated result, then leave its listen socket
        // alive long enough to deliver final pawn/counter state to the owner.
        // This must precede the two-player guard: the successful client may
        // disconnect while the host is draining that last replication window.
        if (Host && State.VerifiedHostFinishAt)
        {
            if (Now>=State.VerifiedHostFinishAt)
                Finish(State,true,true,World,TEXT("host slide, remote-owner climb, shared reservation, replicated movement and restored input; final replication grace completed"));
            return;
        }
        TArray<AStoryFirstPersonCharacter*> Players;
        for (TActorIterator<AStoryFirstPersonCharacter> It(World);It;++It) if (It->GetPlayerState()) Players.Add(*It);
        if (Players.Num()<2) return;
        AStoryFirstPersonCharacter* Other=nullptr;
        for (auto* Candidate:Players) if (Candidate!=Pawn) { Other=Candidate;break; }
        if (!Other) return;
        if (Host && !State.Placed)
        {
            // Test setup only: authoritative initial placement, then every trip
            // uses the real owned-pawn RPC and physical route.
            if (!Place(Pawn,Pole,true) || !Place(Other,Pole,false,FVector(-75,0,0)))
            { Fail(TEXT("LAN initial approaches are obstructed"));return; }
            State.Placed=true; Advance(State,1,Now);
            UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_MP_SETUP host=%s remote=%s"),*Pawn->GetName(),*Other->GetName());
        }
        if (!Host && State.Stage==0 && FVector::Dist2D(Pawn->GetActorLocation(),Pole->LandingWorld(false))<180.f
            && FMath::Abs(Pawn->GetActorLocation().Z-Pole->LandingWorld(false).Z)<130.f) Advance(State,1,Now);
        if (State.Stage==1)
        {
            if (!State.SteadySince || !bSteadyTick)
            { State.SteadySince=Now;State.SteadyFrames=0; }
            else ++State.SteadyFrames;
            if (!Host && !State.ClientReadySent && State.SteadyFrames>=64 && Now-State.SteadySince>=5.)
            {
                Pawn->ServerClubPoleSmokeReady(Pole);
                State.ClientReadySent=true;
                const AGameStateBase* GameState=World->GetGameState();
                UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_MP_REMOTE_READY_SENT steadyFrames=%d steadySeconds=%.3f server=%.6f local=%.6f epoch=%.6f location=%s"),
                    State.SteadyFrames,Now-State.SteadySince,GameState?GameState->GetServerWorldTimeSeconds():double(World->GetTimeSeconds()),
                    double(World->GetTimeSeconds()),Club->MusicEpoch,*Pawn->GetActorLocation().ToCompactString());
            }
        }
        if (Host && State.Stage==1 && Pole->bMultiplayerSmokeClientReady
            && State.SteadyFrames>=64 && Now-State.SteadySince>=5.)
        {
            if (!PressE(PC,Pawn,Pole,true)) { Fail(TEXT("host top E prompt/input unavailable"));return; }
            Advance(State,2,Now);
        }
        if (!Host && !State.BusyAttempted && Other->ClubPoleJourney.Pole==Pole)
        {
            // A competing client request cannot displace the reserved host.
            PC->SetControlRotation((Pole->FocusWorld(false)-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
            Pawn->ServerUseClubPole(Pole,false); State.BusyAttempted=true;
            UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_MP_BUSY_REQUEST sent=1"));
        }
        if (Host && State.Stage==2 && Pole->CompletedJourneys>=1 && !Pawn->ClubPoleJourney.Pole)
        {
            if (!LandingMatches(Pawn,Pole,false)) { Fail(TEXT("host descent did not arrive downstairs"));return; }
            Advance(State,3,Now);
        }
        if (Host && State.Stage==3)
        {
            if (FVector::Dist2D(Pawn->GetActorLocation(),Pole->LandingWorld(false))>=105.f)
            { Pawn->GetCharacterMovement()->StopMovementImmediately(); Advance(State,4,Now); }
            else Pawn->AddMovementInput(FVector(1,0,0),1.f);
        }
        if (!Host && State.Stage==1 && Pole->CompletedJourneys>=1 && !Other->ClubPoleJourney.Pole
            && FVector::Dist2D(Other->GetActorLocation(),Pole->LandingWorld(false))>95.f) Advance(State,2,Now);
        if (!Host && State.Stage==2)
        {
            const FVector DeltaTo=Pole->LandingWorld(false)-Pawn->GetActorLocation();
            if (DeltaTo.Size2D()<9.f)
            {
                Pawn->GetCharacterMovement()->StopMovementImmediately();
                if (!PressE(PC,Pawn,Pole,false)) { Fail(TEXT("remote client's lower E prompt/input unavailable"));return; }
                Advance(State,3,Now);
            }
            else Pawn->AddMovementInput(DeltaTo.GetSafeNormal2D(),FMath::Clamp(float(DeltaTo.Size2D())/80.f,.12f,1.f));
        }
        if (!Host && State.Stage==3 && Pole->CompletedJourneys>=2 && !Pawn->ClubPoleJourney.Pole)
        {
            const FVector Expected=Pole->LandingWorld(true)+FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f);
            const float Distance=FVector::Dist(Pawn->GetActorLocation(),Expected);
            if (!State.FinalLandingObserved)
            {
                State.FinalLandingObserved=true; State.FinalLandingDeadline=Now+2.;
                UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_MP_FINAL_LANDING_BEGIN mode=%d distance=%.3f current=%s expected=%s modeRestored=%d inputIgnored=%d"),
                    int32(World->GetNetMode()),Distance,*Pawn->GetActorLocation().ToCompactString(),*Expected.ToCompactString(),
                    Pawn->GetCharacterMovement()->MovementMode==MOVE_Walking,PC->IsMoveInputIgnored());
            }
            if (!LandingMatches(Pawn,Pole,true) || Pawn->GetCharacterMovement()->MovementMode!=MOVE_Walking || PC->IsMoveInputIgnored())
            {
                if (Now>=State.FinalLandingDeadline)
                    Fail(FString::Printf(TEXT("remote owner failed upper landing after 2 seconds reconciliation: distance=%.3f current=%s expected=%s movementMode=%d inputIgnored=%d"),
                        Distance,*Pawn->GetActorLocation().ToCompactString(),*Expected.ToCompactString(),
                        int32(Pawn->GetCharacterMovement()->MovementMode),PC->IsMoveInputIgnored()));
                return;
            }
            UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_MP_FINAL_LANDING_PASS distance=%.3f settledSeconds=%.3f current=%s expected=%s"),
                Distance,Now-(State.FinalLandingDeadline-2.),*Pawn->GetActorLocation().ToCompactString(),*Expected.ToCompactString());
            Advance(State,4,Now);
        }
        if (State.Stage==4 && Pole->CompletedJourneys>=2 && !Pole->Occupant)
        {
            if (Pole->AbortedJourneys || !State.SeenDown || !State.SeenUp || State.DownSamples<4 || State.UpSamples<4)
            { Fail(TEXT("both peers must observe replicated up/down movement without aborts"));return; }
            if (State.HighestSpeed>1500.f || State.MaximumObservedStep>150.f)
            { Fail(TEXT("replicated traversal made implausibly large movement steps"));return; }
            if (!Host && !State.BusyAttempted) { Fail(TEXT("client did not exercise occupied-pole validation"));return; }
            if (Pawn->GetCharacterMovement()->MovementMode!=MOVE_Walking || PC->IsMoveInputIgnored())
            { Fail(TEXT("normal walking/input was not restored"));return; }
            if (!State.AmbienceDeadline) State.AmbienceDeadline=Now+4.;
            if (!Club || State.MusicPlayingSamples<12 || !Club->LocalMusic->IsPlaying()
                || Club->DebugListenerGain<=.05f || !State.SawMusicStartPhase || State.MaximumActiveLights!=3)
            {
                if (Now>State.AmbienceDeadline)
                    Fail(FString::Printf(TEXT("bounded multiplayer ambience check failed: playingSamples=%d currentPlaying=%d gain=%.3f startPhaseObserved=%d maxLights=%d"),
                        State.MusicPlayingSamples,Club&&Club->LocalMusic?Club->LocalMusic->IsPlaying():false,
                        Club?Club->DebugListenerGain:0.f,State.SawMusicStartPhase,State.MaximumActiveLights));
                return;
            }
            if(FParse::Param(FCommandLine::Get(),TEXT("ClubHallNetworkReview")))
            {
                const float Lounge=Club->GetMusicGainAtWorld(FVector(-2390,-1830,490));
                const float Dance=Club->GetMusicGainAtWorld(FVector(-1850,-1830,490));
                const float Hall=Club->GetMusicGainAtWorld(FVector(-2395,-1400,490));
                if(Club->UpperExtraRegions.Num()!=3||Club->UpperAdditionalDoorways.Num()!=1||Club->ClubLights.Num()!=6||Lounge<.9f||Dance<.9f||Hall>.001f)
                { Fail(TEXT("new club wing regions or music boundaries differ on this peer"));return; }
                UE_LOG(LogTemp,Display,TEXT("CLUB_HALL_NETWORK_PASS mode=%d regions=%d lights=%d newRoomGain=%.3f newDanceGain=%.3f hallwayGain=%.3f"),int32(World->GetNetMode()),Club->UpperExtraRegions.Num(),Club->ClubLights.Num(),Lounge,Dance,Hall);
            }
            const AGameStateBase* GameState=World->GetGameState();
            UE_LOG(LogTemp,Display,TEXT("CLUB_AMBIENCE_MP_PASS mode=%d wall=%.6f server=%.6f epoch=%.6f phase=%.6f startOffset=%.6f maxStartError=%.6f playingSamples=%d currentPlaying=%d gain=%.3f activeLights=%d maxLights=%d"),
                int32(World->GetNetMode()),Now,GameState?GameState->GetServerWorldTimeSeconds():double(World->GetTimeSeconds()),
                Club->MusicEpoch,Club->GetLoopPhaseSeconds(),Club->DebugLastAudioStartOffset,State.MaximumMusicStartPhaseError,
                State.MusicPlayingSamples,Club->LocalMusic->IsPlaying(),Club->DebugListenerGain,
                Club->DebugActiveLightCount,State.MaximumActiveLights);
            if (Host)
            {
                State.VerifiedHostFinishAt=Now+3.;
                UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_MP_HOST_VALIDATED finalReplicationGraceSeconds=3"));
            }
            else Finish(State,true,true,World,TEXT("host slide, remote-owner climb, shared reservation, replicated movement and restored input"));
        }
        return;
    }

    if (World->GetNetMode()!=NM_Standalone) { Fail(TEXT("single-player smoke requires standalone mode"));return; }
    Observe(State,Pawn,Delta);
    if (State.Stage==0)
    {
        State.Scale=Pawn->GetActorScale3D();
        State.Radius=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
        State.HalfHeight=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        FString Why;
        if (!Pole->IsEndpointClear(Pawn,false,Why) || !Pole->IsEndpointClear(Pawn,true,Why)) { Fail(Why);return; }
        const FVector Outward=(Pole->LandingWorld(true)-Pole->GetActorTransform().TransformPosition(Pole->UpperShaft)).GetSafeNormal2D();
        const FVector Side(Outward.Y,-Outward.X,0.f);
        if (!Place(Pawn,Pole,true,Side*110.f)) { Fail(TEXT("natural upper approach is obstructed"));return; }
        FHitResult Gate;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ClubPoleAccidentalFall),false,Pawn);
        const FVector From=Pole->LandingWorld(true)+FVector(0,0,State.HalfHeight+2);
        const FVector To=Pole->GetActorTransform().TransformPosition(Pole->UpperShaft)+FVector(0,0,State.HalfHeight+2);
        if (!World->SweepSingleByChannel(Gate,From,To,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(State.Radius,State.HalfHeight),Query)
            || Gate.GetComponent()!=Pole->GateCollision)
        { Fail(FString::Printf(TEXT("closed safety gate did not physically block accidental entry (%s / %s)"),*GetNameSafe(Gate.GetActor()),*GetNameSafe(Gate.GetComponent())));return; }
        UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_GATE_BLOCK_PASS actor=%s component=%s"),*GetNameSafe(Gate.GetActor()),*GetNameSafe(Gate.GetComponent()));
        const FVector PoleCenter=Pole->GetActorTransform().TransformPosition(Pole->PoleBase);
        // Supporting ring floor around all sides of the actual aperture.
        for (int32 Index=0;Index<12;++Index)
        {
            const float Angle=Index*2.f*PI/12.f;
            FVector Ring=PoleCenter+FVector(FMath::Cos(Angle)*135.f,FMath::Sin(Angle)*135.f,0);
            Ring.Z=Pole->LandingWorld(true).Z;
            FHitResult Floor;
            Query.AddIgnoredComponent(Pole->GateCollision.Get());
            if (!World->LineTraceSingleByChannel(Floor,Ring+FVector(0,0,18),Ring-FVector(0,0,15),ECC_Pawn,Query)
                || Floor.ImpactNormal.Z<.7f || FMath::Abs(Floor.ImpactPoint.Z-Ring.Z)>8.f)
            { Fail(FString::Printf(TEXT("aperture perimeter missing floor index=%d point=%s"),Index,*Ring.ToCompactString()));return; }
        }
        // Let ordinary capsule/camera updates settle after setup placement.
        // The standing focus checks then exercise real approach state.
        Advance(State,11,Now);
    }
    if (State.Stage==11 && Now-State.PhaseStarted>.15)
    {
        FString Why;
        if (!CheckStandingFocus(PC,Pawn,Pole,true,Why)) { Fail(Why);return; }
        Advance(State,10,Now);
    }
    if (State.Stage==10)
    {
        // Walk along the outside of the closed guard to its ordinary approach;
        // no setup teleport places the player at the exact usable endpoint.
        const FVector To=Pole->LandingWorld(true)-Pawn->GetActorLocation();
        if (To.Size2D()<7.f)
        {
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            State.LastPosition=Pawn->GetActorLocation();
            UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_NATURAL_WALK_PASS endpoint=upper location=%s"),*Pawn->GetActorLocation().ToCompactString());
            Advance(State,1,Now);
        }
        else Pawn->AddMovementInput(To.GetSafeNormal2D(),FMath::Clamp(float(To.Size2D())/80.f,.12f,1.f));
    }
    if (State.Stage==1)
    {
        const FVector Direction=(Pole->GetActorTransform().TransformPosition(Pole->UpperShaft)-Pole->LandingWorld(true)).GetSafeNormal2D();
        Pawn->AddMovementInput(Direction,1.f);
        if (Now-State.PhaseStarted>.55)
        {
            const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,State.HalfHeight);
            if (FMath::Abs(Feet.Z-Pole->LandingWorld(true).Z)>8.f || FVector::Dist2D(Pawn->GetActorLocation(),State.LastPosition)>18.f)
            { Fail(TEXT("walking into the closed gate allowed an accidental fall"));return; }
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            if (!PressStandingE(PC,Pawn,Pole,true)) { Fail(TEXT("natural standing E slide prompt/input unavailable"));return; }
            Advance(State,2,Now);
        }
    }
    if (State.Stage==2 && Pole->CompletedJourneys==1 && !Pawn->ClubPoleJourney.Pole)
    {
        if (!State.SeenDown || State.DownSamples<12 || !LandingMatches(Pawn,Pole,false))
        { Fail(TEXT("slide lacked sampled continuous movement or missed lower landing"));return; }
        Advance(State,3,Now);
    }
    if (State.Stage==3)
    {
        FVector Center=Pole->GetActorTransform().TransformPosition(Pole->PoleBase);
        Center.Z=Pole->LandingWorld(false).Z;
        const FVector Outward=(Pole->LandingWorld(false)-Pole->GetActorTransform().TransformPosition(Pole->LowerShaft)).GetSafeNormal2D();
        const FVector Side(Outward.Y,-Outward.X,0.f);
        const FVector WalkPoints[]={Center+Side*110.f+Outward*20.f,Center+Side*110.f-Outward*100.f,Center-Outward*100.f};
        const FVector To=WalkPoints[FMath::Min(State.BottomApproachPoint,2)]-Pawn->GetActorLocation();
        if (To.Size2D()<8.f)
        {
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            if (++State.BottomApproachPoint>=3)
            {
                FString Why;
                if (!CheckStandingFocus(PC,Pawn,Pole,false,Why)) { Fail(Why);return; }
                UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_NATURAL_WALK_PASS endpoint=lower approach=rearOfSolidShaft location=%s"),*Pawn->GetActorLocation().ToCompactString());
                if (!PressStandingE(PC,Pawn,Pole,false)) { Fail(TEXT("natural standing E climb prompt/input unavailable"));return; }
                Advance(State,4,Now);
            }
        }
        else Pawn->AddMovementInput(To.GetSafeNormal2D(),FMath::Clamp(float(To.Size2D())/80.f,.12f,1.f));
    }
    if (State.Stage==4 && Pole->CompletedJourneys==2 && !Pawn->ClubPoleJourney.Pole)
    {
        if (!State.SeenUp || State.UpSamples<12 || !LandingMatches(Pawn,Pole,true) || Pole->AbortedJourneys)
        { Fail(TEXT("climb lacked sampled continuous movement or missed upper landing"));return; }
        if (!Pawn->GetActorScale3D().Equals(State.Scale,.001f)
            || !FMath::IsNearlyEqual(State.Radius,Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),.01f)
            || !FMath::IsNearlyEqual(State.HalfHeight,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),.01f)
            || Pawn->GetCharacterMovement()->MovementMode!=MOVE_Walking || PC->IsMoveInputIgnored()
            || Pawn->GetCapsuleComponent()->GetMoveIgnoreComponents().Contains(Pole->GateCollision))
        { Fail(TEXT("scale, capsule, gate collision, walking or input failed restoration"));return; }
        Finish(State,true,false,World,TEXT("closed gate/perimeter collision, actual E slide down and E climb up passed"));
    }
}

struct FClubPoleSmokeRegistration
{
    FClubPoleSmokeRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&TickClubPoleSmoke); }
};
FClubPoleSmokeRegistration Registration;
}
#endif
