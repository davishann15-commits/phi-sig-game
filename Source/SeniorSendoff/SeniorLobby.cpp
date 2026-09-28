#include "SeniorLobby.h"
#include "SeniorLobbyUI.h"
#include "SeniorSettingsPanel.h"
#include "SeniorCharacterRoster.h"
#include "StoryCampaign.h"
#include "Containers/Ticker.h"
#include "Engine/GameViewportClient.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/TouchInterface.h"
#include "IPAddress.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "SocketSubsystem.h"
#include "Widgets/SWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_EDITOR
void TickSeniorLobbySmokeTest(ASeniorLobbyController* Controller);
#endif
#if !UE_BUILD_SHIPPING
void TickSeniorLobbyStartupSmoke(ASeniorLobbyController* Controller);
#endif

namespace
{
    const FName LobbyMap(TEXT("/Game/Story/Maps/Lobby"));
    const TCHAR* LobbyOptions = TEXT("game=/Script/SeniorSendoff.SeniorLobbyGameMode");
    struct FLobbyRecovery { FString Message; bool bScheduled = false; bool bPartyRepairAttempted = false; };
    // Retain connection errors across replacement of the failed world's controller.
    TMap<TWeakObjectPtr<UGameInstance>, FLobbyRecovery> RecoveryState;
    struct FLocalSelections { int32 Character = 0; int32 Loadout = 0; };
    TMap<TWeakObjectPtr<UGameInstance>, FLocalSelections> LocalSelections;
    bool IsLobby(const UObject* Context) { return UGameplayStatics::GetCurrentLevelName(Context, true) == TEXT("Lobby"); }
}

ASeniorLobbyPlayerState::ASeniorLobbyPlayerState() { SetNetUpdateFrequency(10.f); }
void ASeniorLobbyPlayerState::OnRep_CharacterIndex()
{
    if (auto* Character = Cast<AStoryFirstPersonCharacter>(GetPawn())) Character->ApplySelectedCharacter();
}
void ASeniorLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASeniorLobbyPlayerState, CharacterIndex);
    DOREPLIFETIME(ASeniorLobbyPlayerState, LoadoutIndex);
    DOREPLIFETIME(ASeniorLobbyPlayerState, bReady);
    DOREPLIFETIME(ASeniorLobbyPlayerState, bIsHost);
}
void ASeniorLobbyPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
    Super::CopyProperties(NewPlayerState);
    if (auto* Target = Cast<ASeniorLobbyPlayerState>(NewPlayerState))
    {
        Target->CharacterIndex = CharacterIndex;
        Target->LoadoutIndex = LoadoutIndex;
        Target->bIsHost = bIsHost;
        Target->bReady = false;
    }
}
TArray<ASeniorLobbyPlayerState*> ASeniorLobbyGameState::GetMembers() const
{
    TArray<ASeniorLobbyPlayerState*> Members;
    for (APlayerState* State : PlayerArray)
        if (auto* Member = Cast<ASeniorLobbyPlayerState>(State))
            if (!Member->IsInactive() && !Member->IsOnlyASpectator()) Members.Add(Member);
    Members.Sort([](const ASeniorLobbyPlayerState& A, const ASeniorLobbyPlayerState& B)
    {
        if (A.bIsHost != B.bIsHost) return A.bIsHost;
        return A.GetPlayerId() < B.GetPlayerId();
    });
    return Members;
}
bool ASeniorLobbyGameState::CanStart() const
{
    const auto Members = GetMembers();
    if (bStarting || Members.IsEmpty() || Members.Num() > MaxPlayers) return false;
    bool bHasHost = false;
    for (const auto* Member : Members)
    {
        bHasHost |= Member->bIsHost;
        if (!Member->bReady) return false;
    }
    return bHasHost;
}
void ASeniorLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASeniorLobbyGameState, bStarting);
    DOREPLIFETIME(ASeniorLobbyGameState, bResumeStory);
    DOREPLIFETIME(ASeniorLobbyGameState, MaxPlayers);
}

ASeniorLobbyController::ASeniorLobbyController() { PrimaryActorTick.bCanEverTick = true; }
void ASeniorLobbyController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    PartyPreparationStartedAt = FPlatformTime::Seconds();
    if (GEngine)
    {
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ASeniorLobbyController::NetworkFailed);
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ASeniorLobbyController::TravelFailed);
    }
    for (auto It = RecoveryState.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
    if (auto* Recovery = RecoveryState.Find(GetGameInstance()))
    {
        LobbyMessage = Recovery->Message;
        LobbyMessageUntil = FPlatformTime::Seconds() + 20;
        if (!Recovery->bScheduled && !Recovery->bPartyRepairAttempted) RecoveryState.Remove(GetGameInstance());
    }
    RefreshPresentation();
}
void ASeniorLobbyController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsLocalController()) return;
    if (PresentedMap != UGameplayStatics::GetCurrentLevelName(this, true)) RefreshPresentation();
    if (auto* State = GetPlayerState<ASeniorLobbyPlayerState>())
    {
        if (!bAppliedLocalSelections)
        {
            bAppliedLocalSelections = true;
            if (HasAuthority())
                if (const auto* Saved = LocalSelections.Find(GetGameInstance()))
                {
                    State->CharacterIndex = SeniorRoster::IsValidIndex(Saved->Character) ? Saved->Character : 0;
                    State->LoadoutIndex = FMath::Clamp(Saved->Loadout,0,1);
                    State->OnRep_CharacterIndex();
                    State->ForceNetUpdate();
                }
        }
        auto& Saved = LocalSelections.FindOrAdd(GetGameInstance());
        Saved.Character = State->CharacterIndex; Saved.Loadout = State->LoadoutIndex;
    }
    if (LoadingWidget.IsValid() && PresentedMap == LoadingDestination && GetPawn()) RemoveLoadingWidget();
    if (IsLobby(this) && !bConnectionPending)
    {
        if (IsPartyPrepared())
        {
            if (auto* Recovery = RecoveryState.Find(GetGameInstance()))
                if (!Recovery->bScheduled) RecoveryState.Remove(GetGameInstance());
        }
        else if (HasPartyInitializationFailed() && GetNetMode() == NM_Standalone)
        {
            auto& Recovery = RecoveryState.FindOrAdd(GetGameInstance());
            if (!Recovery.bPartyRepairAttempted)
            {
                Recovery.bPartyRepairAttempted = true;
                RetryPartyInitialization();
            }
        }
    }
#if !UE_BUILD_SHIPPING
    TickSeniorLobbyStartupSmoke(this);
#endif
#if WITH_EDITOR
    TickSeniorLobbySmokeTest(this);
#endif
}
void ASeniorLobbyController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClosePauseMenu();
    RemoveLobbyWidget();
    RemoveLoadingWidget();
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    }
    Super::EndPlay(EndPlayReason);
}
void ASeniorLobbyController::PostSeamlessTravel() { Super::PostSeamlessTravel(); RefreshPresentation(); }
void ASeniorLobbyController::OnPossess(APawn* InPawn) { Super::OnPossess(InPawn); RefreshPresentation(); }
void ASeniorLobbyController::RefreshPresentation()
{
    if (!IsLocalController()) return;
    if (PauseWidget.IsValid()) ClosePauseMenu();
    const FString CurrentMap = UGameplayStatics::GetCurrentLevelName(this, true);
    if (CurrentMap == TEXT("Lobby") && PresentedMap != CurrentMap)
        PartyPreparationStartedAt = FPlatformTime::Seconds();
    PresentedMap = CurrentMap;
    const bool bInLobby = PresentedMap == TEXT("Lobby");
    bShowMouseCursor = bInLobby;
    bEnableClickEvents = bInLobby;
    bEnableTouchEvents = bInLobby;
    ResetIgnoreMoveInput(); ResetIgnoreLookInput();
    if (bInLobby)
    {
        ActivateTouchInterface(nullptr);
        if (GetWorld() && GetWorld()->GetGameViewport())
            GetWorld()->GetGameViewport()->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
        if (!LobbyWidget.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
        {
            LobbyWidget = MakeSeniorLobbyWidget(this);
            GetWorld()->GetGameViewport()->AddViewportWidgetContent(LobbyWidget.ToSharedRef(), 100);
        }
        FInputModeUIOnly Input;
        if (LobbyWidget.IsValid()) Input.SetWidgetToFocus(LobbyWidget);
        Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Input);
    }
    else
    {
        RemoveLobbyWidget();
        CaptureGameplayMouse();
#if PLATFORM_IOS || PLATFORM_ANDROID
        ActivateTouchInterface(LoadObject<UTouchInterface>(nullptr,
            TEXT("/Engine/MobileResources/HUD/DefaultVirtualJoysticks.DefaultVirtualJoysticks")));
#endif
    }
}
void ASeniorLobbyController::RemoveLobbyWidget()
{
    if (!LobbyWidget.IsValid()) return;
    if (GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(LobbyWidget.ToSharedRef());
    LobbyWidget.Reset();
}
void ASeniorLobbyController::RemoveLoadingWidget()
{
    if (LoadingWidget.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(LoadingWidget.ToSharedRef());
    LoadingWidget.Reset();
    if (GetWorld() && !IsLobby(this) && !PauseWidget.IsValid()) CaptureGameplayMouse();
}
void ASeniorLobbyController::CaptureGameplayMouse()
{
    if (!IsLocalController() || !GetWorld() || IsLobby(this) || PauseWidget.IsValid()) return;
    bShowMouseCursor = false;
    bEnableClickEvents = false;
    bEnableTouchEvents = false;
    FInputModeGameOnly Mode;
    Mode.SetConsumeCaptureMouseDown(false);
    SetInputMode(Mode);
    if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
    {
        Viewport->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
        Viewport->SetMouseLockMode(EMouseLockMode::LockOnCapture);
    }
    if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
}
void ASeniorLobbyController::OpenPauseMenu()
{
    if (!IsLocalController() || !GetWorld() || IsLobby(this) || PauseWidget.IsValid()) return;
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    if (!Viewport) return;
    bRestartConfirmationPending = false;
    PauseWidget = SNew(SOverlay)
        + SOverlay::Slot()[SNew(SBorder).BorderBackgroundColor(FLinearColor(0.005f, 0.008f, 0.012f, .80f))]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
          [SNew(SBox).WidthOverride(1050).HeightOverride(650)
            [SNew(SBorder).Padding(20).BorderBackgroundColor(FLinearColor(.035f, .045f, .055f, .98f))
              [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(12, 2, 12, 12)
                  [SNew(STextBlock).Text(FText::FromString(TEXT("PAUSED  |  SENIOR SEND-OFF")))]
                + SVerticalBox::Slot().AutoHeight().Padding(12, 0, 12, 12)
                  [SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
                      [SNew(SButton).Text(FText::FromString(TEXT("RESUME  (ESC)")))
                        .OnClicked_Lambda([this]() { ClosePauseMenu(); return FReply::Handled(); })]
                    + SHorizontalBox::Slot().AutoWidth()
                      [SNew(SButton).Text(FText::FromString(TEXT("RETURN TO LOBBY")))
                        .OnClicked_Lambda([this]() { ReturnToLobby(); return FReply::Handled(); })]
                    + SHorizontalBox::Slot().AutoWidth().Padding(12, 0, 0, 0)
                      [SNew(SButton).Text(FText::FromString(TEXT("CHECKPOINT")))
                        .IsEnabled_Lambda([this]() { return HasAuthority(); })
                        .OnClicked_Lambda([this]() {
                            ClosePauseMenu();
                            if (UStoryCampaign* Story = GetGameInstance<UStoryCampaign>())
                                Story->RespawnAtCheckpoint();
                            return FReply::Handled();
                        })]
                    + SHorizontalBox::Slot().AutoWidth().Padding(12, 0, 0, 0)
                      [SNew(SButton).Text_Lambda([this]() { return FText::FromString(
                          bRestartConfirmationPending ? TEXT("CONFIRM RESTART") : TEXT("RESTART STORY")); })
                        .IsEnabled_Lambda([this]() { return HasAuthority(); })
                        .OnClicked_Lambda([this]() {
                            if (!bRestartConfirmationPending) bRestartConfirmationPending = true;
                            else
                            {
                                ClosePauseMenu();
                                if (UStoryCampaign* Story = GetGameInstance<UStoryCampaign>()) Story->NewStory();
                            }
                            return FReply::Handled();
                        })]]
                + SVerticalBox::Slot().FillHeight(1)[MakeSeniorSettingsPanel()]]]];
    Viewport->AddViewportWidgetContent(PauseWidget.ToSharedRef(), 300);
    bPausedWorldForMenu = GetNetMode() == NM_Standalone && SetPause(true);
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    SetInputMode(Mode);
    Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
}
void ASeniorLobbyController::ClosePauseMenu()
{
    if (!PauseWidget.IsValid()) return;
    if (GetWorld() && GetWorld()->GetGameViewport())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(PauseWidget.ToSharedRef());
    PauseWidget.Reset();
    bRestartConfirmationPending = false;
    if (bPausedWorldForMenu) SetPause(false);
    bPausedWorldForMenu = false;
    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();
    CaptureGameplayMouse();
}
void ASeniorLobbyController::TogglePauseMenu()
{
    if (PauseWidget.IsValid()) ClosePauseMenu();
    else OpenPauseMenu();
}
void ASeniorLobbyController::ClientBeginStoryLoading_Implementation(int32 Chapter)
{
    if (!IsLocalController()) return;
    RemoveLoadingWidget();
    LoadingDestination = Chapter == 1 ? TEXT("Chapter01_House") :
        FString::Printf(TEXT("Chapter%02d"), FMath::Clamp(Chapter, 1, 3));
    if (GetWorld() && GetWorld()->GetGameViewport())
    {
        LoadingWidget = MakeSeniorLoadingWidget(Chapter);
        GetWorld()->GetGameViewport()->AddViewportWidgetContent(LoadingWidget.ToSharedRef(), 200);
    }
}
void ASeniorLobbyController::ClientCancelStoryLoading_Implementation() { RemoveLoadingWidget(); }
bool ASeniorLobbyController::InputKey(const FInputKeyEventArgs& Params)
{
    if (Params.Key == EKeys::Escape && Params.Event == IE_Pressed && IsLocalController() && !IsLobby(this))
    { TogglePauseMenu(); return true; }
    return Super::InputKey(Params);
}
bool ASeniorLobbyController::CanEditLobby() const
{
    const auto* State = GetWorld() ? GetWorld()->GetGameState<ASeniorLobbyGameState>() : nullptr;
    return HasAuthority() && IsLobby(this) && State && !State->bStarting && GetPlayerState<ASeniorLobbyPlayerState>();
}
void ASeniorLobbyController::SetCharacter_Implementation(int32 Index)
{
    if (!CanEditLobby()) return;
    if (!SeniorRoster::IsValidIndex(Index)) { ShowLobbyMessage(TEXT("Choose one of the eight available characters.")); return; }
    auto* State = GetPlayerState<ASeniorLobbyPlayerState>();
    if (State->CharacterIndex != Index) State->LoadoutIndex = 0;
    State->CharacterIndex = Index; State->bReady = false; State->ForceNetUpdate();
    State->OnRep_CharacterIndex();
}
void ASeniorLobbyController::SetLoadout_Implementation(int32 Index)
{
    if (!CanEditLobby()) return;
    if (Index < 0 || Index > 1) { ShowLobbyMessage(TEXT("Choose one of this character's two signature weapons.")); return; }
    auto* State = GetPlayerState<ASeniorLobbyPlayerState>();
    State->LoadoutIndex = Index; State->bReady = false; State->ForceNetUpdate();
}
void ASeniorLobbyController::SetReady_Implementation(bool bValue)
{
    if (!CanEditLobby()) return;
    auto* State = GetPlayerState<ASeniorLobbyPlayerState>();
    State->bReady = bValue; State->ForceNetUpdate();
    ShowLobbyMessage(bValue ? TEXT("Ready. The host can start once everyone is ready.") : TEXT("Choose your character and loadout, then ready up."));
}
bool ASeniorLobbyController::IsHost() const
{
    if (const auto* State = GetPlayerState<ASeniorLobbyPlayerState>()) return State->bIsHost;
    return HasAuthority() && IsLocalController();
}
bool ASeniorLobbyController::IsPartyPrepared() const
{
    const auto* State = GetWorld() ? GetWorld()->GetGameState<ASeniorLobbyGameState>() : nullptr;
    const auto* Local = GetPlayerState<ASeniorLobbyPlayerState>();
    return State && Local && State->GetMembers().Contains(Local);
}
bool ASeniorLobbyController::CanStartStory() const
{
    if (!IsLobby(this) || !IsHost() || !IsPartyPrepared()) return false;
    const auto* State = GetWorld()->GetGameState<ASeniorLobbyGameState>();
    const auto* Story = GetGameInstance<UStoryCampaign>();
    if (State->bStarting || !Story || Story->bTravelPending) return false;
    // The UI and authority use the same predicate; solo play requires no ready step.
    return State->CanStart() || State->GetMembers().Num() == 1;
}
bool ASeniorLobbyController::HasPartyInitializationFailed() const
{
    return IsLobby(this) && !bConnectionPending && !IsPartyPrepared()
        && PartyPreparationStartedAt > 0 && FPlatformTime::Seconds() - PartyPreparationStartedAt >= 10;
}
void ASeniorLobbyController::RetryPartyInitialization()
{
    if (!IsLocalController() || !IsLobby(this) || !HasPartyInitializationFailed()) return;
    const auto* World = GetWorld();
    UE_LOG(LogTemp, Warning, TEXT("LOBBY_INITIALIZATION_RECOVERY: mode=%s state=%s player=%s; reopening the native solo lobby"),
        *GetNameSafe(World ? World->GetAuthGameMode() : nullptr),
        *GetNameSafe(World ? World->GetGameState() : nullptr), *GetNameSafe(PlayerState));
    // Absolute travel clears stale PIE/URL game-mode options. Never manufacture
    // a replicated party locally or bypass multiplayer readiness.
    LeaveLobby();
}
void ASeniorLobbyController::StartStory_Implementation(bool bResume)
{
    if (!HasAuthority() || !IsLocalController() || !CanEditLobby()) return;
    auto* State = GetWorld()->GetGameState<ASeniorLobbyGameState>();
    if (!CanStartStory()) { ShowLobbyMessage(TEXT("The host cannot start this party yet.")); return; }
    // A solo player has no other party members to coordinate with. Keep the
    // ready gate for multiplayer, but let the main Start Game button work alone.
    if (!State->CanStart())
    {
        const auto Members = State->GetMembers();
        auto* Host = GetPlayerState<ASeniorLobbyPlayerState>();
        if (Members.Num() == 1 && Members[0] == Host && Host->bIsHost)
        {
            Host->bReady = true;
            Host->ForceNetUpdate();
        }
    }
    if (!State->CanStart()) { ShowLobbyMessage(TEXT("The host cannot start this party yet.")); return; }
    auto* Story = GetGameInstance<UStoryCampaign>();
    if (!Story) { ShowLobbyMessage(TEXT("Story setup is unavailable. Return to the lobby and try again.")); return; }
    if (bResume && (!State->bResumeStory || !Story->Progress || Story->Progress->bCompleted))
    { ShowLobbyMessage(TEXT("No unfinished checkpoint is available. Start a new story.")); return; }
    State->bStarting = true; State->ForceNetUpdate();
    if (bResume) Story->ResumeStory(); else Story->NewStory();
    if (!Story->bTravelPending)
    {
        State->bStarting = false; State->ForceNetUpdate();
        ShowLobbyMessage(Story->Status.IsEmpty() ? TEXT("Could not start the story. Please try again.") : Story->Status);
    }
}
void ASeniorLobbyController::ShowLobbyMessage_Implementation(const FString& Message)
{
    LobbyMessage = Message.Left(240);
    LobbyMessageUntil = FPlatformTime::Seconds() + 6;
    if (auto* Story = GetGameInstance<UStoryCampaign>())
    { Story->Status = LobbyMessage; Story->StatusUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + 5.f : 5.f; }
}
FString ASeniorLobbyController::GetLobbyMessage() const
{
    return LobbyMessageUntil == 0 || FPlatformTime::Seconds() < LobbyMessageUntil ? LobbyMessage : FString();
}
bool ASeniorLobbyController::IsValidLANAddress(const FString& Address)
{
    // A literal IPv4 and optional port only; no URL options or executable console text.
    if (Address.IsEmpty() || Address.Len() > 21) return false;
    for (TCHAR Char : Address)
        if (!((Char >= TEXT('0') && Char <= TEXT('9')) || Char == TEXT('.') || Char == TEXT(':'))) return false;
    FString IP = Address, Port;
    if (Address.Split(TEXT(":"), &IP, &Port))
    {
        if (Port.IsEmpty() || Port.Len() > 5 || Port.Contains(TEXT(":"))) return false;
        for (TCHAR Char : Port) if (Char < TEXT('0') || Char > TEXT('9')) return false;
        const int32 Number = FCString::Atoi(*Port);
        if (Number < 1 || Number > 65535) return false;
    }
    TArray<FString> Octets; IP.ParseIntoArray(Octets, TEXT("."), false);
    if (Octets.Num() != 4) return false;
    for (const FString& Octet : Octets)
        if (Octet.IsEmpty() || Octet.Len() > 3 || FCString::Atoi(*Octet) > 255) return false;
    const int32 First = FCString::Atoi(*Octets[0]);
    return First > 0 && First < 224;
}
FString ASeniorLobbyController::GetHostAddress() const
{
    if (!IsHost() && GetWorld()) return GetWorld()->URL.Host;
    if (FPlatformTime::Seconds() - AddressCheckedAt < 5) return CachedHostAddress;
    AddressCheckedAt = FPlatformTime::Seconds();
    auto* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TArray<TSharedPtr<FInternetAddr>> Addresses; FString Best;
    if (Sockets && Sockets->GetLocalAdapterAddresses(Addresses))
        for (const auto& Address : Addresses)
        {
            if (!Address.IsValid()) continue;
            const FString Value = Address->ToString(false);
            if (!IsValidLANAddress(Value) || Value.StartsWith(TEXT("127.")) || Value.StartsWith(TEXT("169.254."))) continue;
            if (Best.IsEmpty()) Best = Value;
            if (Value.StartsWith(TEXT("192.168.")) || Value.StartsWith(TEXT("10."))) { Best = Value; break; }
        }
    CachedHostAddress = Best.IsEmpty() ? TEXT("Check Wi-Fi settings for your IP") : Best;
    return CachedHostAddress;
}
void ASeniorLobbyController::HostLAN()
{
    if (!IsLocalController() || !IsLobby(this) || bConnectionPending) return;
    LobbyMessageUntil = 0;
    if (GetNetMode() == NM_ListenServer) { LobbyMessage = TEXT("Already hosting. Share your address with your friends."); return; }
    if (GetNetMode() == NM_Client) { LobbyMessage = TEXT("Leave this lobby before hosting your own."); return; }
    bConnectionPending = true; LobbyMessage = TEXT("Opening your local Wi-Fi lobby...");
    UGameplayStatics::OpenLevel(this, LobbyMap, true, FString(TEXT("listen?")) + LobbyOptions);
}
void ASeniorLobbyController::JoinLAN(const FString& Address)
{
    if (!IsLocalController() || !IsLobby(this) || bConnectionPending) return;
    LobbyMessageUntil = 0;
    const FString Clean = Address.TrimStartAndEnd();
    if (!IsValidLANAddress(Clean)) { LobbyMessage = TEXT("Enter the host's IPv4 address, for example 192.168.1.25 (optional :7777)."); return; }
    if (GetNetMode() != NM_Standalone) { LobbyMessage = TEXT("Leave your current lobby before joining another host."); return; }
    bConnectionPending = true; LobbyMessage = TEXT("Connecting to ") + Clean + TEXT("...");
    ClientTravel(Clean, TRAVEL_Absolute);
}
void ASeniorLobbyController::LeaveLobby()
{
    if (IsLocalController()) UGameplayStatics::OpenLevel(this, LobbyMap, true, LobbyOptions);
}
void ASeniorLobbyController::ReturnToLobby()
{
    if (!IsLocalController() || !GetWorld()) return;
    ClosePauseMenu();
    if (GetNetMode() == NM_ListenServer) GetWorld()->ServerTravel(LobbyMap.ToString() + TEXT("?") + LobbyOptions, true);
    else LeaveLobby();
}
void ASeniorLobbyController::NetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Failure, const FString& Error)
{
    if (!IsLocalController() || (World && World != GetWorld())) return;
    if (!World)
    {
        // Pending-connection failures have no world. Match their driver so another PIE session cannot disconnect us.
        const FWorldContext* Context = GEngine ? GEngine->GetWorldContextFromWorld(GetWorld()) : nullptr;
        if (!Context || !Context->PendingNetGame || Context->PendingNetGame->NetDriver != Driver) return;
    }
    // A remote player leaving must not disconnect the host and the rest of the party.
    if (GetNetMode() == NM_ListenServer && !bConnectionPending) return;
    RecoverToLobby(Error.IsEmpty() ? TEXT("Connection lost. Check that everyone is on the same Wi-Fi and the host is still playing.")
        : FString(TEXT("Could not stay connected: ")) + Error.Left(180));
}
void ASeniorLobbyController::TravelFailed(UWorld* World, ETravelFailure::Type Failure, const FString& Error)
{
    if (!IsLocalController() || (World && World != GetWorld())) return;
    RecoverToLobby(FString(TEXT("Could not load the session. ")) + Error.Left(180));
}
void ASeniorLobbyController::RecoverToLobby(const FString& Message)
{
    bConnectionPending = false; LobbyMessage = Message;
    LobbyMessageUntil = FPlatformTime::Seconds() + 20;
    RemoveLoadingWidget();
    if (auto* Story = GetGameInstance<UStoryCampaign>()) Story->bTravelPending = false;
    auto* Instance = GetGameInstance(); if (!Instance) return;
    auto& State = RecoveryState.FindOrAdd(Instance); State.Message = Message;
    if (State.bScheduled) return; State.bScheduled = true;
    const TWeakObjectPtr<UGameInstance> WeakInstance(Instance);
    // Let Unreal finish disconnecting; a world timer would disappear with the failed world.
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakInstance](float)
    {
        auto* Current = WeakInstance.Get(); auto* Pending = RecoveryState.Find(WeakInstance);
        if (!Current || !Pending) return false;
        Pending->bScheduled = false;
        if (auto* PC = Cast<ASeniorLobbyController>(Current->GetFirstLocalPlayerController()))
        {
            PC->LobbyMessage = Pending->Message;
            PC->LobbyMessageUntil = FPlatformTime::Seconds() + 20;
            if (IsLobby(Current) && Current->GetWorld() && Current->GetWorld()->GetNetMode() == NM_Standalone) return false;
        }
        if (Current->GetWorld()) UGameplayStatics::OpenLevel(Current, LobbyMap, true, LobbyOptions);
        return false;
    }), 0.5f);
}

ASeniorLobbyGameMode::ASeniorLobbyGameMode()
{
    DefaultPawnClass = nullptr; PlayerControllerClass = ASeniorLobbyController::StaticClass();
    PlayerStateClass = ASeniorLobbyPlayerState::StaticClass(); GameStateClass = ASeniorLobbyGameState::StaticClass();
    HUDClass = nullptr; bUseSeamlessTravel = true;
}
void ASeniorLobbyGameMode::InitGameState()
{
    Super::InitGameState(); if (GameSession) GameSession->MaxPlayers = 3;
    if (auto* State = GetGameState<ASeniorLobbyGameState>())
    {
        State->MaxPlayers = 3;
        if (const auto* Story = GetGameInstance<UStoryCampaign>())
            State->bResumeStory = Story->Progress && !Story->Progress->bCompleted
                && (Story->Progress->Chapter > 1 || Story->Progress->bHasCheckpoint);
    }
}
void ASeniorLobbyGameMode::PreLogin(const FString& Options, const FString& Address,
    const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    const auto* State = GetGameState<ASeniorLobbyGameState>();
    if (State && State->bStarting) ErrorMessage = TEXT("This group has already started the chapter.");
    else if (State && State->GetMembers().Num() >= State->MaxPlayers) ErrorMessage = TEXT("This lobby is full (3 players maximum).");
    else if (UGameplayStatics::HasOption(Options, TEXT("SpectatorOnly"))) ErrorMessage = TEXT("Spectator connections are not supported in the lobby.");
}
void ASeniorLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer); auto* State = GetGameState<ASeniorLobbyGameState>();
    if (State && (State->bStarting || State->GetMembers().Num() > State->MaxPlayers))
    {
        if (GameSession) GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("This lobby is full or has already started.")));
        return;
    }
    if (auto* Member = NewPlayer->GetPlayerState<ASeniorLobbyPlayerState>())
    { Member->bIsHost = NewPlayer->IsLocalController(); Member->bReady = false; Member->ForceNetUpdate(); }
    if (auto* PC = Cast<ASeniorLobbyController>(NewPlayer))
        PC->ShowLobbyMessage(NewPlayer->IsLocalController() ? TEXT("Choose your character and loadout. Play solo or host a local Wi-Fi lobby.")
            : TEXT("Connected. Choose your character and loadout while the host prepares the story."));
}
void ASeniorLobbyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    // The lobby is a viewport widget; chapter game modes spawn the gameplay pawns.
}
void ASeniorLobbyGameMode::HandleSeamlessTravelPlayer(AController*& Player)
{
    Super::HandleSeamlessTravelPlayer(Player);
    if (auto* Member = Player ? Player->GetPlayerState<ASeniorLobbyPlayerState>() : nullptr)
    {
        Member->bReady = false;
        if (auto* PC = Cast<APlayerController>(Player)) Member->bIsHost = PC->IsLocalController();
        Member->ForceNetUpdate();
    }
}
