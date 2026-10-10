// Opt-in, three-process LAN check. Each process records its own replicated view.
// Run with -VentMultiplayerSmoke in a listen lobby and two connected clients.
#include "HouseVentNetwork.h"
#include "SeniorLobby.h"
#include "StoryCampaign.h"
#include "StoryMovementComponent.h"

#if !UE_BUILD_SHIPPING
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
const FName PortalIds[3] = { TEXT("PigstyVent"), TEXT("BlackHallNorthVent"), TEXT("BlackHallEastVent") };
const FIntPoint MoveCells[3] = { FIntPoint(-5, 0), FIntPoint(5, 0), FIntPoint(5, 5) };

struct FVentMultiplayerState
{
    TWeakObjectPtr<UWorld> World;
    double Started = 0.0;
    double PhaseStarted = 0.0;
    double AllExitsAt = 0.0;
    int32 LocalPhase = 0;
    bool bLobbyStarted = false;
    bool bPlaced = false;
    bool bPassed = false;
    bool Entered[3] = {};
    bool Moved[3] = {};
    bool Exited[3] = {};
};

void Finish(FVentMultiplayerState& State, bool bPassed, const FString& Detail)
{
    UE_LOG(LogTemp, Display, TEXT("VENT_MP_%s mode=%d %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"),
        State.World.IsValid() ? int32(State.World->GetNetMode()) : -1, *Detail);
    State.bPassed = true;
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void Advance(FVentMultiplayerState& State, int32 Phase, double Now)
{
    State.LocalPhase = Phase;
    State.PhaseStarted = Now;
}

FVector CellCenter(const AHouseVentNetwork* Vent, const FIntPoint& Cell)
{
    const TArray<FIntPoint> Cells = Vent->GetRouteCells();
    const TArray<FVector> Centers = Vent->GetDuctCellCenters();
    for (int32 Index = 0; Index < Cells.Num() && Index < Centers.Num(); ++Index)
        if (Cells[Index] == Cell) return Centers[Index];
    return FVector::ZeroVector;
}

bool PressInteraction(APlayerController* Controller)
{
    if (!Controller || Controller->bShowMouseCursor || !Controller->PlayerInput) return false;
    // InputKey queues the event; its immediate return value only checks action
    // mappings and can be false for this project's direct BindKey(E) handler.
    Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, IE_Pressed, 1.f));
    Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, IE_Released, 0.f));
    return true;
}

void TickVentMultiplayer(UWorld* World, ELevelTick, float)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("VentMultiplayerSmoke"))
        || !World || !World->IsGameWorld()) return;
    static FVentMultiplayerState State;
    const double Now = FPlatformTime::Seconds();
    if (State.World.Get() != World)
    {
        State = FVentMultiplayerState();
        State.World = World;
        State.Started = State.PhaseStarted = Now;
        UE_LOG(LogTemp, Display, TEXT("VENT_MP_WORLD map=%s mode=%d"),
            *UGameplayStatics::GetCurrentLevelName(World, true), int32(World->GetNetMode()));
    }
    if (State.bPassed) return;
    if (Now - State.Started > 180.0)
    { Finish(State, false, TEXT("world timeout")); return; }

    const FString Map = UGameplayStatics::GetCurrentLevelName(World, true);
    if (Map == TEXT("Lobby"))
    {
        if (World->GetNetMode() != NM_ListenServer || State.bLobbyStarted) return;
        ASeniorLobbyGameState* Lobby = World->GetGameState<ASeniorLobbyGameState>();
        if (!Lobby || Lobby->GetMembers().Num() != 3 || Now - State.Started < 2.0) return;
        TArray<ASeniorLobbyController*> Controllers;
        ASeniorLobbyController* Host = nullptr;
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
            if (ASeniorLobbyController* PC = Cast<ASeniorLobbyController>(It->Get()))
            {
                if (PC->IsLocalController()) Host = PC;
                else Controllers.Add(PC);
            }
        if (!Host || Controllers.Num() != 2) return;
        Controllers.Sort([](const ASeniorLobbyController& A, const ASeniorLobbyController& B)
        {
            return A.GetPlayerState<APlayerState>()->GetPlayerId()
                < B.GetPlayerState<APlayerState>()->GetPlayerId();
        });
        Controllers.Insert(Host, 0);
        for (int32 Index = 0; Index < 3; ++Index)
        {
            Controllers[Index]->SetCharacter(Index);
            Controllers[Index]->SetReady(true);
        }
        if (!Lobby->CanStart())
        { Finish(State, false, TEXT("three connected players could not ready in lobby")); return; }
        UE_LOG(LogTemp, Display, TEXT("VENT_MP_LOBBY_READY members=3 uniqueCharacters=3"));
        State.bLobbyStarted = true;
        Host->StartStory(false);
        if (!Lobby->bStarting)
            Finish(State, false, TEXT("host could not start story travel"));
        return;
    }
    if (Map != TEXT("Chapter01_House")) return;
    if (State.AllExitsAt > 0.0)
    {
        // Clients exit first; retain the host's complete observation even
        // after their pawns disconnect from the finished session.
        const double Delay = World->GetNetMode() == NM_ListenServer ? 4.0 : 1.0;
        if (Now - State.AllExitsAt > Delay)
            Finish(State, true, TEXT("saw all three enter, crawl, replicate position, and exit"));
        return;
    }

    TActorIterator<AHouseVentNetwork> VentIt(World);
    AHouseVentNetwork* Vent = VentIt ? *VentIt : nullptr;
    AStoryFirstPersonCharacter* Pawns[3] = {};
    for (TActorIterator<AStoryFirstPersonCharacter> It(World); It; ++It)
        if (const ASeniorLobbyPlayerState* Player = It->GetPlayerState<ASeniorLobbyPlayerState>())
            if (Player->CharacterIndex >= 0 && Player->CharacterIndex < 3)
                Pawns[Player->CharacterIndex] = *It;
    APlayerController* LocalController = nullptr;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        if (APlayerController* Candidate = It->Get(); Candidate && Candidate->IsLocalController())
            LocalController = Candidate;
    AStoryFirstPersonCharacter* LocalPawn = LocalController
        ? Cast<AStoryFirstPersonCharacter>(LocalController->GetPawn()) : nullptr;
    const ASeniorLobbyPlayerState* LocalPlayer = LocalPawn
        ? LocalPawn->GetPlayerState<ASeniorLobbyPlayerState>() : nullptr;
    if (!Vent || Vent->NumActivePortals() != 6 || !Pawns[0] || !Pawns[1] || !Pawns[2]
        || !LocalPawn || !LocalPlayer || LocalPlayer->CharacterIndex < 0
        || LocalPlayer->CharacterIndex > 2) return;
    const int32 LocalIndex = LocalPlayer->CharacterIndex;
    if (Pawns[LocalIndex] != LocalPawn)
    { Finish(State, false, TEXT("duplicate or mismatched player character index")); return; }

    if (World->GetNetMode() == NM_ListenServer && !State.bPlaced && Now - State.Started > 2.0)
    {
        for (int32 Index = 0; Index < 3; ++Index)
        {
            AStoryFirstPersonCharacter* Pawn = Pawns[Index];
            const FVector Foot = Vent->GetPortalRoomApproach(PortalIds[Index]);
            if (Foot.IsNearlyZero())
            { Finish(State, false, FString::Printf(TEXT("room approach missing for player %d"), Index)); return; }
            const FVector Location = Foot + FVector(0, 0,
                Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3.f);
            const FRotator Facing(0, (Vent->GetPortalFacadeLocation(PortalIds[Index]) - Location).Rotation().Yaw, 0);
            if (!Pawn->TeleportTo(Location, Facing, false, false))
            { Finish(State, false, FString::Printf(TEXT("room teleport failed for player %d"), Index)); return; }
            Pawn->ForceNetUpdate();
            UE_LOG(LogTemp, Display, TEXT("VENT_MP_PLACED player=%d portal=%s location=%s"),
                Index, *PortalIds[Index].ToString(), *Pawn->GetActorLocation().ToString());
        }
        State.bPlaced = true;
    }

    // Each process independently checks its local pawn and both remote replicas.
    for (int32 Index = 0; Index < 3; ++Index)
    {
        AStoryFirstPersonCharacter* Pawn = Pawns[Index];
        const UStoryMovementComponent* Movement = Pawn->GetStoryMovement();
        const bool bInside = Vent->IsInDuct(Pawn);
        const bool bCrawling = Movement && Movement->IsCrawling();
        const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
        if (!State.Entered[Index] && bInside && bCrawling && Pawn->bVentCrawlForced
            && Capsule->GetUnscaledCapsuleRadius() <= 18.5f
            && Capsule->GetUnscaledCapsuleHalfHeight() <= 19.5f)
        {
            State.Entered[Index] = true;
            UE_LOG(LogTemp, Display, TEXT("VENT_MP_SEEN_ENTRY observer=%d player=%d local=%d location=%s radius=%.1f half=%.1f"),
                LocalIndex, Index, Pawn == LocalPawn, *Pawn->GetActorLocation().ToString(),
                Capsule->GetUnscaledCapsuleRadius(), Capsule->GetUnscaledCapsuleHalfHeight());
        }
        if (State.Entered[Index] && !State.Moved[Index] && bInside && bCrawling
            && FVector::Dist2D(Pawn->GetActorLocation(), Vent->GetPortalDuctLocation(PortalIds[Index])) > 55.f)
        {
            State.Moved[Index] = true;
            UE_LOG(LogTemp, Display, TEXT("VENT_MP_SEEN_MOVE observer=%d player=%d local=%d location=%s"),
                LocalIndex, Index, Pawn == LocalPawn, *Pawn->GetActorLocation().ToString());
        }
        if (State.Moved[Index] && !State.Exited[Index] && !bInside && !bCrawling && !Pawn->bVentCrawlForced
            && FVector::Dist(Pawn->GetActorLocation(), Vent->GetPortalFacadeLocation(PortalIds[Index])) < 280.f)
        {
            State.Exited[Index] = true;
            UE_LOG(LogTemp, Display, TEXT("VENT_MP_SEEN_EXIT observer=%d player=%d local=%d location=%s"),
                LocalIndex, Index, Pawn == LocalPawn, *Pawn->GetActorLocation().ToString());
        }
    }
    auto All = [](const bool Flags[3]) { return Flags[0] && Flags[1] && Flags[2]; };
    if (State.LocalPhase >= 8 && All(State.Exited))
    {
        State.AllExitsAt = Now;
        UE_LOG(LogTemp, Display, TEXT("VENT_MP_ALL_EXITED observer=%d"), LocalIndex);
        return;
    }

    if (State.LocalPhase == 0)
    {
        // GetPortalRoomApproach deliberately searches for another free floor
        // point once this player occupies the first one.
        const FVector Facade = Vent->GetPortalFacadeLocation(PortalIds[LocalIndex]);
        if (FVector::Dist2D(LocalPawn->GetActorLocation(), Facade) > 180.f
            || FMath::Abs(LocalPawn->GetActorLocation().Z - Facade.Z) > 220.f) return;
        UE_LOG(LogTemp, Display, TEXT("VENT_MP_LOCAL_READY player=%d portal=%s"), LocalIndex, *PortalIds[LocalIndex].ToString());
        Advance(State, 1, Now);
    }
    if (State.LocalPhase == 1)
    {
        FVector Eye; FRotator View;
        LocalController->GetPlayerViewPoint(Eye, View);
        LocalController->SetControlRotation((Vent->GetPortalFacadeLocation(PortalIds[LocalIndex]) - Eye).Rotation());
        if (Now - State.PhaseStarted > .65)
        {
            const FName Focused = Vent->FindFocusedPortal(LocalPawn, .2f);
            if (Focused != PortalIds[LocalIndex])
            {
                Finish(State, false, FString::Printf(TEXT("local player %d cannot focus %s (focused=%s)"),
                    LocalIndex, *PortalIds[LocalIndex].ToString(), *Focused.ToString()));
                return;
            }
            if (!PressInteraction(LocalController))
            { Finish(State, false, FString::Printf(TEXT("player %d E input was not handled"), LocalIndex)); return; }
            UE_LOG(LogTemp, Display, TEXT("VENT_MP_INPUT_ENTER player=%d portal=%s"), LocalIndex, *Focused.ToString());
            Advance(State, 2, Now);
        }
    }
    if (State.LocalPhase == 2 && State.Entered[LocalIndex]) Advance(State, 3, Now);
    if (State.LocalPhase == 3 && All(State.Entered))
    {
        UE_LOG(LogTemp, Display, TEXT("VENT_MP_ALL_ENTERED observer=%d"), LocalIndex);
        Advance(State, 4, Now);
    }
    if (State.LocalPhase == 4)
    {
        const FVector Target = CellCenter(Vent, MoveCells[LocalIndex]);
        if (Target.IsNearlyZero())
        { Finish(State, false, TEXT("adjacent crawl route cell missing")); return; }
        if (FVector::Dist2D(LocalPawn->GetActorLocation(), Vent->GetPortalDuctLocation(PortalIds[LocalIndex])) > 75.f)
        { Advance(State, 5, Now); return; }
        LocalPawn->AddMovementInput((Target - LocalPawn->GetActorLocation()).GetSafeNormal2D(), 1.f);
    }
    if (State.LocalPhase == 5 && All(State.Moved))
    {
        UE_LOG(LogTemp, Display, TEXT("VENT_MP_ALL_MOVED observer=%d"), LocalIndex);
        Advance(State, 6, Now);
    }
    if (State.LocalPhase == 6)
    {
        const FVector Target = Vent->GetPortalDuctLocation(PortalIds[LocalIndex]);
        if (FVector::Dist2D(LocalPawn->GetActorLocation(), Target) < 15.f)
        {
            LocalPawn->GetCharacterMovement()->StopMovementImmediately();
            Advance(State, 7, Now);
            return;
        }
        LocalPawn->AddMovementInput((Target - LocalPawn->GetActorLocation()).GetSafeNormal2D(), 1.f);
    }
    if (State.LocalPhase == 7 && Now - State.PhaseStarted > .3)
    {
        const FName Focused = Vent->FindFocusedPortal(LocalPawn, -.5f);
        if (Focused != PortalIds[LocalIndex])
        { Finish(State, false, FString::Printf(TEXT("player %d could not focus duct exit (focused=%s)"), LocalIndex, *Focused.ToString())); return; }
        if (!PressInteraction(LocalController))
        { Finish(State, false, FString::Printf(TEXT("player %d exit E input was not handled"), LocalIndex)); return; }
        UE_LOG(LogTemp, Display, TEXT("VENT_MP_INPUT_EXIT player=%d portal=%s"), LocalIndex, *Focused.ToString());
        Advance(State, 8, Now);
    }
    if (State.LocalPhase >= 2 && Now - State.PhaseStarted > 25.0)
        Finish(State, false, FString::Printf(TEXT("player %d stalled in phase %d entry=%d move=%d exit=%d"),
            LocalIndex, State.LocalPhase, State.Entered[LocalIndex], State.Moved[LocalIndex], State.Exited[LocalIndex]));
}

struct FVentMultiplayerRegistration
{
    FVentMultiplayerRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&TickVentMultiplayer); }
};
FVentMultiplayerRegistration Registration;
}
#endif
