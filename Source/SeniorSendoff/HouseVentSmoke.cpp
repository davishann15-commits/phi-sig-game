// Opt-in gameplay check for the real Chapter 1 vent facades and crawl route.
#include "HouseVentNetwork.h"
#include "HouseWorldScale.h"
#include "StoryCampaign.h"
#include "StoryMovementComponent.h"

#if !UE_BUILD_SHIPPING
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
struct FVentSmokeState
{
    double Started = 0.0;
    double PhaseStarted = 0.0;
    double LastRouteProgress = 0.0;
    int32 Phase = 0;
    int32 Portal = 0;
    int32 RouteCell = -5;
    float BestRouteDistance = BIG_NUMBER;
    TArray<FName> Ids;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;
    TWeakObjectPtr<AHouseVentNetwork> Network;
};

void FinishVentSmoke(FVentSmokeState& State, bool bPassed, const FString& Reason)
{
    if (bPassed)
    {
        UE_LOG(LogTemp, Display, TEXT("HOUSE_VENT_SMOKE_PASSED portals=%d routeCell=%d: %s"),
            State.Portal, State.RouteCell, *Reason);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("HOUSE_VENT_SMOKE_FAILED portals=%d routeCell=%d: %s"),
            State.Portal, State.RouteCell, *Reason);
    }
    State.Phase = -1;
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void Advance(FVentSmokeState& State, int32 NextPhase, double Now)
{
    State.Phase = NextPhase;
    State.PhaseStarted = Now;
}

bool AimAtFacade(APlayerController* Controller, AStoryFirstPersonCharacter* Pawn,
    AHouseVentNetwork* Network, FName Portal)
{
    const FVector Foot = Network->GetPortalRoomApproach(Portal);
    if (Foot.IsNearlyZero()) return false;
    const FVector Center = Foot + FVector(0.f, 0.f,
        Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3.f);
    const FVector Target = Network->GetPortalFacadeLocation(Portal);
    const FRotator BodyRotation(0.f, (Target - Center).Rotation().Yaw, 0.f);
    if (!Pawn->TeleportTo(Center, BodyRotation, false, false)) return false;
    FVector Eye; FRotator EyeRotation;
    Controller->GetPlayerViewPoint(Eye, EyeRotation);
    Controller->SetControlRotation((Target - Eye).Rotation());
    return true;
}

FVector RouteTarget(const AHouseVentNetwork* Network, int32 X)
{
    const TArray<FIntPoint> Cells = Network->GetRouteCells();
    const TArray<FVector> Centers = Network->GetDuctCellCenters();
    for (int32 Index = 0; Index < Cells.Num() && Index < Centers.Num(); ++Index)
        if (Cells[Index] == FIntPoint(X, 0)) return Centers[Index];
    return FVector::ZeroVector;
}

FString TransitionState(const AStoryFirstPersonCharacter* Pawn,
    const AHouseVentNetwork* Network, const UStoryMovementComponent* Movement)
{
    const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
    return FString::Printf(TEXT("pawn=%s duct=%d crawl=%d grounded=%d mode=%d radius=%.2f halfHeight=%.2f worldTime=%.3f"),
        *Pawn->GetActorLocation().ToString(), Network->IsInDuct(Pawn),
        Movement->IsCrawling(), Movement->IsMovingOnGround(), int32(Movement->MovementMode),
        Capsule->GetUnscaledCapsuleRadius(), Capsule->GetUnscaledCapsuleHalfHeight(),
        Pawn->GetWorld()->GetTimeSeconds());
}
}

void TickHouseVentSmoke(UWorld* World)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("HouseVentSmoke")) || !World
        || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    static FVentSmokeState State;
    if (State.Phase < 0) return;
    const double WallNow = FPlatformTime::Seconds();
    // Vent transitions run on the world's TimerManager. Shader stalls must not
    // age the phase faster than the real transition's game-time timer.
    const double Now = World->GetTimeSeconds();
    if (State.Started == 0.0) { State.Started = WallNow; State.PhaseStarted = Now; }
    if (WallNow - State.Started > 150.0)
    { FinishVentSmoke(State, false, TEXT("Timed out")); return; }

    APlayerController* Controller = World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = Controller
        ? Cast<AStoryFirstPersonCharacter>(Controller->GetPawn()) : nullptr;
    TActorIterator<AHouseVentNetwork> VentIt(World);
    AHouseVentNetwork* Network = VentIt ? *VentIt : nullptr;
    if (!Pawn || !Network || !Pawn->HasAuthority()) return;
    UStoryMovementComponent* Movement = Pawn->GetStoryMovement();
    if (!Movement) { FinishVentSmoke(State, false, TEXT("Story movement component missing")); return; }

    if (State.Phase == 0)
    {
        if (Now - State.PhaseStarted < 2.0) return;
        // The production GameMode ticks at 4 Hz. Drive this opt-in movement
        // test every frame so AddMovementInput persists like a held player key.
        if (AGameModeBase* Mode = World->GetAuthGameMode()) Mode->SetActorTickInterval(0.f);
        State.Pawn = Pawn;
        State.Network = Network;
        State.Ids = Network->GetPortalIds();
        const float HouseScale = HouseWorldScale::Factor(World);
        if (!Network->GetActorScale3D().Equals(FVector(HouseScale), .001f))
        { FinishVentSmoke(State, false, TEXT("Duct geometry did not retain the house's authored resize")); return; }
        UE_LOG(LogTemp, Display, TEXT("HOUSE_VENT_SMOKE_SCALE house=%.3f duct=%s character=%s"),
            HouseScale, *Network->GetActorScale3D().ToString(), *Pawn->GetActorScale3D().ToString());
        if (State.Ids.Num() != 6 || Network->NumActivePortals() != 6
            || Network->GetDuctSectionCount() < 30)
        { FinishVentSmoke(State, false, TEXT("Expected six live facades and connected duct sections")); return; }
        // Flood every cardinal neighbour to ensure no portal sits on an island.
        const TArray<FIntPoint> Cells = Network->GetRouteCells();
        TSet<FIntPoint> All, Reached;
        for (const FIntPoint Cell : Cells) All.Add(Cell);
        TArray<FIntPoint> Queue;
        Queue.Add(Cells[0]); Reached.Add(Cells[0]);
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
            for (const FIntPoint Step : { FIntPoint(1,0), FIntPoint(-1,0), FIntPoint(0,1), FIntPoint(0,-1) })
            {
                const FIntPoint Neighbour = Queue[Head] + Step;
                if (All.Contains(Neighbour) && !Reached.Contains(Neighbour))
                { Reached.Add(Neighbour); Queue.Add(Neighbour); }
            }
        if (Reached.Num() != All.Num())
        { FinishVentSmoke(State, false, TEXT("Duct route graph is disconnected")); return; }
        Advance(State, 1, Now);
    }

    if (State.Phase == 1)
    {
        if (State.Portal >= State.Ids.Num())
        { State.RouteCell = -5; State.BestRouteDistance = BIG_NUMBER; Advance(State, 6, Now); return; }
        const FName Id = State.Ids[State.Portal];
        if (!AimAtFacade(Controller, Pawn, Network, Id))
        { FinishVentSmoke(State, false, FString::Printf(TEXT("Could not reach room approach for %s"), *Id.ToString())); return; }
        Advance(State, 2, Now);
        return;
    }

    if (State.Phase == 2 && Now - State.PhaseStarted >= .2)
    {
        FVector Eye; FRotator ViewRotation;
        Controller->GetPlayerViewPoint(Eye, ViewRotation);
        Controller->SetControlRotation((Network->GetPortalFacadeLocation(State.Ids[State.Portal]) - Eye).Rotation());
        Advance(State, 20, Now);
        return;
    }

    if (State.Phase == 20 && Now - State.PhaseStarted >= .15)
    {
        const FName Id = State.Ids[State.Portal];
        const FName Focused = Network->FindFocusedPortal(Pawn, .2f);
        const bool bAccepted = Focused == Id && Network->Interact(Pawn, Id);
        if (!bAccepted)
        {
            FVector Eye; FRotator ViewRotation;
            Controller->GetPlayerViewPoint(Eye, ViewRotation);
            FinishVentSmoke(State, false, FString::Printf(
                TEXT("Entry failed at %s: focused=%s eye=%s view=%s pawn=%s"),
                *Id.ToString(), *Focused.ToString(), *Eye.ToString(),
                *ViewRotation.ToString(), *Pawn->GetActorLocation().ToString()));
            return;
        }
        Advance(State, 3, Now);
        return;
    }

    if (State.Phase == 3 && Now - State.PhaseStarted >= .4)
    {
        const FName Id = State.Ids[State.Portal];
        if (!Network->IsInDuct(Pawn) || !Movement->IsCrawling()
            || Pawn->GetCapsuleComponent()->GetUnscaledCapsuleRadius() > 18.f
            || Pawn->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() > 19.f)
        {
            if (Now - State.PhaseStarted < 2.0) return;
            FinishVentSmoke(State, false, FString::Printf(TEXT("Did not arrive prone in duct from %s: %s"),
                *Id.ToString(), *TransitionState(Pawn, Network, Movement)));
            return;
        }
        if (Network->FindFocusedPortal(Pawn, -.5f) != Id || !Network->Interact(Pawn, Id))
        { FinishVentSmoke(State, false, FString::Printf(TEXT("Exit interaction failed at %s"), *Id.ToString())); return; }
        Advance(State, 4, Now);
        return;
    }

    if (State.Phase == 4 && Now - State.PhaseStarted >= .6)
    {
        if (Network->IsInDuct(Pawn) || Movement->IsCrawling() || !Movement->IsMovingOnGround())
        {
            if (Now - State.PhaseStarted < 2.0) return;
            FinishVentSmoke(State, false, FString::Printf(TEXT("Did not return standing from %s: %s"),
                *State.Ids[State.Portal].ToString(), *TransitionState(Pawn, Network, Movement)));
            return;
        }
        ++State.Portal;
        Advance(State, 1, Now);
        return;
    }

    if (State.Phase == 6)
    {
        const FName Pig = TEXT("PigstyVent");
        if (!AimAtFacade(Controller, Pawn, Network, Pig))
        { FinishVentSmoke(State, false, TEXT("Could not set up cross-duct crawl")); return; }
        Advance(State, 7, Now);
        return;
    }

    if (State.Phase == 7 && Now - State.PhaseStarted >= .2)
    {
        FVector Eye; FRotator ViewRotation;
        Controller->GetPlayerViewPoint(Eye, ViewRotation);
        Controller->SetControlRotation((Network->GetPortalFacadeLocation(TEXT("PigstyVent")) - Eye).Rotation());
        Advance(State, 70, Now);
        return;
    }

    if (State.Phase == 70 && Now - State.PhaseStarted >= .15)
    {
        if (!Network->Interact(Pawn, TEXT("PigstyVent")))
        { FinishVentSmoke(State, false, TEXT("Cross-duct entry failed")); return; }
        Advance(State, 8, Now);
        return;
    }

    if (State.Phase == 8 && Now - State.PhaseStarted >= .4)
    {
        if (!Network->IsInDuct(Pawn) || !Movement->IsCrawling())
        {
            if (Now - State.PhaseStarted < 2.0) return;
            FinishVentSmoke(State, false, FString::Printf(TEXT("Cross-duct crawl did not start: %s"),
                *TransitionState(Pawn, Network, Movement)));
            return;
        }
        State.LastRouteProgress = Now;
        Advance(State, 9, Now);
    }

    if (State.Phase == 9)
    {
        if (!Network->IsInDuct(Pawn) || !Movement->IsCrawling())
        { FinishVentSmoke(State, false, TEXT("Left the enclosed duct before its far exit")); return; }
        if (State.RouteCell > 6) { Advance(State, 10, Now); return; }
        const FVector Target = RouteTarget(Network, State.RouteCell);
        if (Target.IsNearlyZero())
        { FinishVentSmoke(State, false, TEXT("Expected trunk route cell missing")); return; }
        const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), Target);
        if (Distance < 13.f)
        {
            ++State.RouteCell;
            State.BestRouteDistance = BIG_NUMBER;
            State.LastRouteProgress = Now;
            return;
        }
        if (Distance < State.BestRouteDistance - 2.f)
        { State.BestRouteDistance = Distance; State.LastRouteProgress = Now; }
        if (Now - State.LastRouteProgress > 3.f)
        { FinishVentSmoke(State, false, FString::Printf(TEXT("Blocked in duct near cell %d"), State.RouteCell)); return; }
        Pawn->AddMovementInput((Target - Pawn->GetActorLocation()).GetSafeNormal2D(), 1.f);
        return;
    }

    if (State.Phase == 10)
    {
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        if (Network->FindFocusedPortal(Pawn, -.5f) != TEXT("BlackHallNorthVent")
            || !Network->Interact(Pawn, TEXT("BlackHallNorthVent")))
        { FinishVentSmoke(State, false, TEXT("Far duct exit not usable")); return; }
        Advance(State, 11, Now);
        return;
    }

    if (State.Phase == 11 && Now - State.PhaseStarted >= .6)
    {
        if (Network->IsInDuct(Pawn) || Movement->IsCrawling() || !Movement->IsMovingOnGround())
        {
            if (Now - State.PhaseStarted < 2.0) return;
            FinishVentSmoke(State, false, FString::Printf(TEXT("Far duct exit did not return to the house: %s"),
                *TransitionState(Pawn, Network, Movement)));
            return;
        }
        FinishVentSmoke(State, true, TEXT("All six facades enter/exit and pigsty-to-hall route was crawled under collision"));
    }
}
#endif
