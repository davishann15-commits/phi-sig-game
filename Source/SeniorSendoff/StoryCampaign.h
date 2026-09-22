#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "SeniorLobby.h"
#include "SeniorClothMotion.h"
#include "StoryCampaign.generated.h"

class UCameraComponent;
class UAnimSequence;
class UAnimInstance;
class USkeletalMesh;
class USkeletalMeshComponent;
class UInputAction;
class UInputMappingContext;
class ASeniorBraxtonVisual;
class ASeniorDouli;
struct FInputActionValue;

UCLASS()
class SENIORSENDOFF_API UStorySave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Chapter = 1;
    UPROPERTY(SaveGame) int32 Checkpoint = 0;
    UPROPERTY(SaveGame) FTransform Spawn = FTransform::Identity;
    UPROPERTY(SaveGame) bool bHasCheckpoint = false;
    UPROPERTY(SaveGame) bool bCompleted = false;
    UPROPERTY(SaveGame) int32 Difficulty = 1;
};

UCLASS(BlueprintType)
class SENIORSENDOFF_API UStoryCampaign : public UGameInstance
{
    GENERATED_BODY()
public:
    virtual void Init() override;
    virtual void Shutdown() override;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UStorySave> Progress;
    UPROPERTY(BlueprintReadOnly) bool bTravelPending = false;
    UPROPERTY(BlueprintReadOnly) FString Status;
    UPROPERTY(BlueprintReadOnly) float StatusUntil = 0;
    UPROPERTY(BlueprintReadOnly) int32 SelectedDifficulty = 1;
    UFUNCTION(BlueprintCallable) void SetDifficulty(int32 NewDifficulty);
    UFUNCTION(BlueprintPure) static FString DifficultyName(int32 Difficulty);
    UFUNCTION(BlueprintCallable) void ResumeStory();
    UFUNCTION(BlueprintCallable) void NewStory();
    UFUNCTION(BlueprintCallable) bool ReachCheckpoint(int32 Chapter, int32 Index, FTransform Spawn);
    UFUNCTION(BlueprintCallable) void CompleteChapter();
    UFUNCTION(BlueprintCallable) void RespawnAtCheckpoint();
    void RespawnPlayerAtCheckpoint(APlayerController* Player);
    UFUNCTION(BlueprintPure) int32 CurrentChapter() const;
    UFUNCTION(BlueprintPure) static FName ChapterMap(int32 Chapter);
    void Notify(const FString& Message);
private:
    bool SaveProgress();
    bool CanManageStory() const;
    void ReplicateProgress();
    void Travel(int32 Chapter);
    void BeforeLoad(const FString& MapName);
    void AfterLoad(UWorld* World);
    FDelegateHandle BeforeHandle, AfterHandle;
    FTimerHandle TravelTimer;
    FString SaveSlot = TEXT("SeniorSendoff_Story_v1");
};

UCLASS()
class SENIORSENDOFF_API AStoryGameState : public ASeniorLobbyGameState
{
    GENERATED_BODY()
public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 StoryChapter = 1;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 StoryCheckpoint = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 StoryDifficulty = 1;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bStoryCompleted = false;
};

UCLASS(Blueprintable)
class SENIORSENDOFF_API AStoryGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AStoryGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& Error) override;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void RestartPlayer(AController* Player) override;
private:
    float FallLimit = -1500;
};

UCLASS(Blueprintable)
class SENIORSENDOFF_API AStoryFirstPersonCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AStoryFirstPersonCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void OnRep_PlayerState() override;
    virtual void NotifyControllerChanged() override;
    virtual void PawnClientRestart() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Weapon") TObjectPtr<ASeniorDouli> Douli;
    UFUNCTION(BlueprintCallable, Category="Weapon") void ThrowDouli();
    UFUNCTION(Server, Reliable) void ServerThrowDouli();
    FTransform DouliGrip(bool bFirstPerson) const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="First Person") TObjectPtr<UCameraComponent> FirstPersonCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="First Person") TObjectPtr<USkeletalMeshComponent> FirstPersonArms;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="First Person") FTransform ArmsRelativeTransform = FTransform(FRotator::ZeroRotator, FVector(8, 0, -155));
    UFUNCTION(BlueprintCallable, Category="Character") void ApplySelectedCharacter();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputMappingContext> InputContext;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> MoveAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> LookAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> MouseLookAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> JumpAction;
private:
    void ApplyCharacterDimensions(USkeletalMesh* Body);
    void UpdateBodyAnimation();
    UPROPERTY(Transient) TObjectPtr<USkeletalMesh> FallbackBody;
    UPROPERTY(Transient) TSubclassOf<UAnimInstance> FallbackAnimClass;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> BodyIdle;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> BodyWalk;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> CurrentBodyAnimation;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> CurrentArmsAnimation;
    UPROPERTY(Transient) TObjectPtr<ASeniorBraxtonVisual> BraxtonVisual;
    int32 AppliedCharacterIndex = INDEX_NONE;
    bool bWalkingAnimation = false;
    float DouliFlightBlend = 0;
    FSeniorClothMotion ClothMotion;
    void ConfigureLocalInput();
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
};

UCLASS(Blueprintable)
class SENIORSENDOFF_API AStoryCheckpoint : public AActor
{
    GENERATED_BODY()
public:
    AStoryCheckpoint();
    virtual void BeginPlay() override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UBoxComponent> Trigger;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> Marker;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UTextRenderComponent> Label;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class USceneComponent> RespawnPoint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story") int32 Chapter = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story") int32 CheckpointIndex = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story") bool bChapterExit = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story") int32 RequiredCheckpoint = 2;
private:
    UFUNCTION() void Enter(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp,
                          int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
};

UCLASS()
class SENIORSENDOFF_API AStoryHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
private:
    bool bWasPressed = false;
    bool bConfirmRestart = false;
    bool bThrowTouchPressed = false;
};
