#pragma once
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "SeniorLobby.generated.h"

class SWidget;

UCLASS(BlueprintType)
class SENIORSENDOFF_API ASeniorLobbyPlayerState : public APlayerState
{
    GENERATED_BODY()
public:
    ASeniorLobbyPlayerState();
    UPROPERTY(ReplicatedUsing=OnRep_CharacterIndex, BlueprintReadOnly, Category="Lobby") int32 CharacterIndex = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby") int32 LoadoutIndex = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby") bool bReady = false;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby") bool bIsHost = false;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void CopyProperties(APlayerState* NewPlayerState) override;
    UFUNCTION() void OnRep_CharacterIndex();
};

UCLASS(BlueprintType)
class SENIORSENDOFF_API ASeniorLobbyGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby") bool bStarting = false;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby") bool bResumeStory = false;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby") int32 MaxPlayers = 3;
    UFUNCTION(BlueprintPure, Category="Lobby") TArray<ASeniorLobbyPlayerState*> GetMembers() const;
    UFUNCTION(BlueprintPure, Category="Lobby") bool CanStart() const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS(Blueprintable)
class SENIORSENDOFF_API ASeniorLobbyController : public APlayerController
{
    GENERATED_BODY()
public:
    ASeniorLobbyController();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void PostSeamlessTravel() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Lobby") void SetCharacter(int32 Index);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Lobby") void SetLoadout(int32 Index);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Lobby") void SetReady(bool bValue);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Lobby") void StartStory(bool bResume);
    UFUNCTION(Client, Reliable) void ShowLobbyMessage(const FString& Message);
    UFUNCTION(Client, Reliable) void ClientBeginStoryLoading(int32 Chapter);
    UFUNCTION(Client, Reliable) void ClientCancelStoryLoading();
    UFUNCTION(BlueprintCallable, Category="Lobby") void HostLAN();
    UFUNCTION(BlueprintCallable, Category="Lobby") void JoinLAN(const FString& Address);
    UFUNCTION(BlueprintCallable, Category="Lobby") void LeaveLobby();
    UFUNCTION(BlueprintCallable, Category="Lobby") void ReturnToLobby();
    UFUNCTION(BlueprintCallable, Category="Game") void TogglePauseMenu();
    UFUNCTION(BlueprintPure, Category="Game") bool IsPauseMenuOpen() const { return PauseWidget.IsValid(); }
    UFUNCTION(BlueprintPure, Category="Lobby") FString GetLobbyMessage() const;
    UFUNCTION(BlueprintPure, Category="Lobby") bool IsHost() const;
    bool IsPartyPrepared() const;
    bool CanStartStory() const;
    bool HasPartyInitializationFailed() const;
    void RetryPartyInitialization();
    UFUNCTION(BlueprintPure, Category="Lobby") FString GetHostAddress() const;
    static bool IsValidLANAddress(const FString& Address);
private:
    bool CanEditLobby() const;
    void RefreshPresentation();
    void RemoveLobbyWidget();
    void RemoveLoadingWidget();
    void CaptureGameplayMouse();
    void OpenPauseMenu();
    void ClosePauseMenu();
    void NetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Failure, const FString& Error);
    void TravelFailed(UWorld* World, ETravelFailure::Type Failure, const FString& Error);
    void RecoverToLobby(const FString& Message);
    TSharedPtr<SWidget> LobbyWidget;
    TSharedPtr<SWidget> LoadingWidget;
    TSharedPtr<SWidget> PauseWidget;
    bool bPausedWorldForMenu = false;
    bool bRestartConfirmationPending = false;
    FString LoadingDestination;
    FString LobbyMessage;
    double LobbyMessageUntil = 0;
    FString PresentedMap;
    FDelegateHandle NetworkFailureHandle, TravelFailureHandle;
    bool bConnectionPending = false;
    bool bAppliedLocalSelections = false;
    double PartyPreparationStartedAt = 0;
    bool bLoggedPartyPrepared = false;
    mutable FString CachedHostAddress;
    mutable double AddressCheckedAt = -10;
};

UCLASS(Blueprintable)
class SENIORSENDOFF_API ASeniorLobbyGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASeniorLobbyGameMode();
    virtual void InitGameState() override;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
    virtual void HandleSeamlessTravelPlayer(AController*& Player) override;
};
