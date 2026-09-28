// Opt-in Development-player regression: uses the same start predicate as Slate,
// travels through the cooked default lobby, and never writes the production save.
#include "SeniorLobby.h"
#include "StoryCampaign.h"
#if !UE_BUILD_SHIPPING
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

void TickSeniorLobbyStartupSmoke(ASeniorLobbyController* PC)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("LobbyStartupSmoke"))) return;
    static int32 Step = 0;
    static double Started = FPlatformTime::Seconds(), Changed = Started;
    static double HouseReadyAt = -1;
    static bool bInjected = false, bDone = false;
    if (bDone) return;
    const double Now = FPlatformTime::Seconds();
    auto Check = [&](bool Condition, const TCHAR* Description)
    {
        if (!Condition)
        {
            UE_LOG(LogTemp, Error, TEXT("LOBBY_STARTUP_FAILED: %s"), Description);
            bDone = true;
            FPlatformMisc::RequestExitWithStatus(false, 1);
        }
        return Condition;
    };
    float VisualWarmup = 0;
    FParse::Value(FCommandLine::Get(), TEXT("LobbyStartupVisualWarmup="), VisualWarmup);
    if (!Check(FMath::IsFinite(VisualWarmup) && VisualWarmup >= 0 && VisualWarmup <= 20,
        TEXT("Invalid optional house render warm-up"))) return;
    if (!Check(Now - Started < 45 + VisualWarmup, TEXT("Default lobby/story startup timed out"))) return;
    const FString Map = UGameplayStatics::GetCurrentLevelName(PC, true);
    auto* Story = PC->GetGameInstance<UStoryCampaign>();
    if (!Check(Story != nullptr, TEXT("Campaign game instance is missing"))) return;
    auto* Player = PC->GetPlayerState<ASeniorLobbyPlayerState>();
    auto* State = PC->GetWorld()->GetGameState<ASeniorLobbyGameState>();
    if (Step == 0 && Map == TEXT("Lobby") && PC->IsPartyPrepared())
    {
        // Cover both native map loading and editor PIE name resolution. A
        // chapter's own mode must remain available for gameplay and travel.
        if (!Check(Story->OverrideGameModeClass(AGameModeBase::StaticClass(), TEXT("Lobby"), TEXT(""), TEXT(""))
                == ASeniorLobbyGameMode::StaticClass()
            && Story->OverrideGameModeClass(AGameModeBase::StaticClass(), TEXT("UEDPIE_0_Lobby"), TEXT(""), TEXT(""))
                == ASeniorLobbyGameMode::StaticClass()
            && Story->OverrideGameModeClass(AStoryGameMode::StaticClass(), TEXT("Chapter01_House"), TEXT(""), TEXT(""))
                == AStoryGameMode::StaticClass(), TEXT("Lobby mode contract or chapter mode preservation failed"))) return;
        if (FParse::Param(FCommandLine::Get(), TEXT("LobbyStartupRecoveryTest")) && !bInjected)
        {
            // Reproduce the exact UI gate: a controller exists but the party
            // array never registered it. A fresh native lobby must recover it.
            State->PlayerArray.Reset();
            bInjected = true;
            if (!Check(!PC->IsPartyPrepared() && !PC->CanStartStory(), TEXT("Incomplete party did not disable Start Game"))) return;
            UE_LOG(LogTemp, Display, TEXT("LOBBY_STARTUP_TEST: injected missing party registration"));
            return;
        }
        if (!Check(State->GetMembers().Num() == 1 && !Player->bReady && PC->CanStartStory(),
            TEXT("Solo Start Game is not enabled without Ready Up"))) return;
        PC->SetCharacter(2);
        PC->SetLoadout(1);
        if (!Check(!Player->bReady && PC->CanStartStory(), TEXT("Changing a solo loadout disabled Start Game"))) return;
        UE_LOG(LogTemp, Display, TEXT("LOBBY_STARTUP_TEST: default lobby ready; Slate Start Game predicate enabled"));
        Step = 1; Changed = Now;
    }
    else if (Step == 1 && Now - Changed > 3)
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("LobbyStartupCapture")))
            FScreenshotRequest::RequestScreenshot(TEXT("LobbyStartupReady.png"), true, false);
        Step = 5; Changed = Now;
    }
    else if (Step == 5 && Now - Changed > 1)
    {
        PC->StartStory(false);
        if (!Check(Story->bTravelPending && State && State->bStarting, TEXT("Solo story start did not schedule travel"))) return;
        Step = 2; Changed = Now;
    }
    else if (Step == 2 && Map == TEXT("Chapter01_House") && PC->GetPawn() && Now - Changed > 3)
    {
        auto* Pawn = Cast<AStoryFirstPersonCharacter>(PC->GetPawn());
        if (!Check(Pawn && Pawn->GetController() == PC && Player && Player->CharacterIndex == 2
            && Player->LoadoutIndex == 1 && !Story->bTravelPending,
            TEXT("House did not finish loading a possessed pawn with the selected loadout"))) return;
        if (!Check(PC->GetLobbyMessage().IsEmpty()
            && Story->StatusUntil <= PC->GetWorld()->GetTimeSeconds(),
            TEXT("Lobby feedback leaked into the house HUD"))) return;
        // Optional rendered-entry diagnosis waits from actual possession, not
        // from the earlier lobby Start call. It does not change production
        // travel, quality or streaming, and keeps the early default capture.
        if (HouseReadyAt < 0) HouseReadyAt = Now;
        if (Now - HouseReadyAt < VisualWarmup) return;
        auto Value = [](const TCHAR* Name)
        {
            const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name);
            return Var ? Var->GetInt() : -1;
        };
        UE_LOG(LogTemp, Display, TEXT("LOBBY_STARTUP_HOUSE_RENDER_STATE: warmup=%.2f GI=%d shadows=%d lumen=%d"),
            Now - HouseReadyAt, Value(TEXT("sg.GlobalIlluminationQuality")),
            Value(TEXT("sg.ShadowQuality")), Value(TEXT("r.Lumen.DiffuseIndirect.Allow")));
        if (FParse::Param(FCommandLine::Get(), TEXT("LobbyStartupCapture")))
            FScreenshotRequest::RequestScreenshot(TEXT("LobbyStartupHouse.png"), true, false);
        UE_LOG(LogTemp, Display, TEXT("LOBBY_STARTUP_TEST: house loaded, pawn possessed, travel completed"));
        Step = 3; Changed = Now;
    }
    else if (Step == 3 && Now - Changed > 3)
    {
        PC->ReturnToLobby();
        Step = 4; Changed = Now;
    }
    else if (Step == 4 && Map == TEXT("Lobby") && PC->IsPartyPrepared() && Now - Changed > 2)
    {
        if (!Check(!Player->bReady && !State->bStarting && PC->CanStartStory()
            && Player->CharacterIndex == 2 && Player->LoadoutIndex == 1,
            TEXT("Returning to the lobby lost selections or disabled solo restart"))) return;
        UE_LOG(LogTemp, Display, TEXT("LOBBY_STARTUP_PASSED: default lobby, solo start, possessed house pawn, lobby return%s"),
            bInjected ? TEXT(", missing-registration recovery") : TEXT(""));
        bDone = true;
        FPlatformMisc::RequestExitWithStatus(false, 0);
    }
}
#else
void TickSeniorLobbyStartupSmoke(ASeniorLobbyController*) {}
#endif
