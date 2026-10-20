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
#include "HouseClubPole.h"
#include "StoryCampaign.generated.h"

class UCameraComponent;
class USceneComponent;
class UAnimSequence;
class UAnimInstance;
class USkeletalMesh;
class USkeletalMeshComponent;
class UInputAction;
class UInputMappingContext;
class UStoryMovementComponent;
class UHouseFurnitureInteractionComponent;
class AExistingHouseDoor;
class AHouseReactiveFixture;
class AHouseOxbloodMaskPickup;
class AHousePokerTable;
class AHouseRoamingPig;
class ABoywickCharacter;
class AHouseVentNetwork;
class ASeniorBraxtonVisual;
class ASeniorRunnerVisual;
class ASeniorFixerVisual;
class ASeniorDouli;
class ASeniorSaucePacket;
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
    virtual TSubclassOf<AGameModeBase> OverrideGameModeClass(TSubclassOf<AGameModeBase> GameModeClass,
        const FString& MapName, const FString& Options, const FString& Portal) const override;
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
    // Preserve each roster member's stature while reducing their whole in-game
    // body, collision and attached first-person view by five percent.
    static constexpr float GameplayScale = .95f;
    AStoryFirstPersonCharacter(const FObjectInitializer& ObjectInitializer);
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void OnRep_PlayerState() override;
    virtual void NotifyControllerChanged() override;
    virtual void PawnClientRestart() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual bool CanJumpInternal_Implementation() const override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Weapon") TObjectPtr<ASeniorDouli> Douli;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Weapon") TObjectPtr<ASeniorSaucePacket> SaucePacket;
    UPROPERTY(ReplicatedUsing=OnRep_HouseMask, BlueprintReadOnly, Category="House Mask") bool bHasHouseMask = false;
    UPROPERTY(ReplicatedUsing=OnRep_HouseMask, BlueprintReadOnly, Category="House Mask") bool bHouseMaskEquipped = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="House Mask") float HouseMaskDamageMultiplier = 1.25f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House Mask") TObjectPtr<class UStaticMeshComponent> HouseMaskVisual;
    UFUNCTION(BlueprintPure, Category="House Mask") float OutgoingDamageMultiplier() const
    { return (bHouseMaskEquipped ? HouseMaskDamageMultiplier : 1.f) * BoywickDamageMultiplier(); }
    // Boywick, the storytelling bedroom resident: focus for E, and the timed blessing he grants.
    UFUNCTION(BlueprintPure, Category="Boywick") ABoywickCharacter* FocusedBoywick() const;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Boywick") uint8 BoywickBlessing = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Boywick") float BoywickBlessingUntil = 0.f;
    void GrantBoywickBlessing(uint8 Kind, float Seconds);
    UFUNCTION(BlueprintPure, Category="Boywick") bool HasBoywickBlessing(uint8 Kind) const;
    UFUNCTION(BlueprintPure, Category="Boywick") float BoywickDamageMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Boywick") FString BoywickBlessingStatus() const;
    void UpdateBoywickBlessing(float DeltaSeconds);
    void ReceiveHouseMask();
    void ToggleHouseMask();
    AHouseOxbloodMaskPickup* FocusedHouseMask() const;
    UFUNCTION(BlueprintPure, Category="Poker") AHousePokerTable* FocusedHousePokerTable() const;
    UFUNCTION(BlueprintCallable, Category="Weapon") void ThrowEquippedWeapon();
    UFUNCTION(Server, Reliable) void ServerThrowEquippedWeapon();
    UFUNCTION(BlueprintCallable, Category="Weapon") void ThrowDouli();
    UFUNCTION(Server, Reliable) void ServerThrowDouli();
    FTransform DouliGrip(bool bFirstPerson) const;
    USkeletalMeshComponent* GetBraxtonBodyMesh() const;
    FTransform SauceGrip(bool bFirstPerson) const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="First Person") TObjectPtr<UCameraComponent> FirstPersonCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Movement|Crawl") TObjectPtr<USceneComponent> CrawlVisualPivot;
    UPROPERTY(ReplicatedUsing=OnRep_VentCrawlForced, BlueprintReadOnly, Category="Movement|Crawl") bool bVentCrawlForced = false;
    UFUNCTION(BlueprintPure, Category="Movement|Crawl") bool CanStartCrawl() const;
    UFUNCTION(BlueprintCallable, Category="Movement|Crawl") bool SetVentCrawlForced(bool bForced);
    AHouseVentNetwork* FocusedVentNetwork(FName& OutPortalId) const;
    AExistingHouseDoor* FocusedDoorForInteraction() const;
    AHouseReactiveFixture* FocusedHouseFixture() const;
    class AHouseGadget* FocusedHouseGadget() const;
    AHouseClubPole* FocusedClubPole(bool& bUpper) const;
    UPROPERTY(ReplicatedUsing=OnRep_ClubPoleJourney, BlueprintReadOnly, Category="Club Pole") FHouseClubPoleJourney ClubPoleJourney;
    void SetClubPoleJourney(const FHouseClubPoleJourney& Journey);
    UFUNCTION(Client, Reliable) void ClientClubPoleNotice(const FString& Message);
    UFUNCTION(Server, Reliable) void ServerInteractHouseGadget(class AHouseGadget* Gadget);
    UFUNCTION(Server, Reliable) void ServerUseClubPole(AHouseClubPole* Pole, bool bUpper);
    UFUNCTION(Server, Reliable) void ServerClubPoleSmokeReady(AHouseClubPole* Pole);
    UFUNCTION(Server, Reliable) void ServerInteractHouseFixture(AHouseReactiveFixture* Fixture);
    UFUNCTION(BlueprintPure, Category="Pig") AHouseRoamingPig* FocusedHousePigForInteraction() const;
    UFUNCTION(BlueprintPure, Category="Pig") AHouseRoamingPig* CarriedHousePig() const;
    // Measured contact inside the animated right palm, used by native carry
    // validation as well as future interaction effects.
    UFUNCTION(BlueprintPure, Category="Pig") FVector GetPigHandGripPoint() const;
    FVector GetFurnitureHandGripPoint(bool Right) const;
    bool CanReachPigGrip(const FVector& GripWorld,const FVector& LegAxisWorld=FVector::UpVector) const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="First Person") TObjectPtr<USkeletalMeshComponent> FirstPersonArms;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Furniture") TObjectPtr<UHouseFurnitureInteractionComponent> FurnitureInteraction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="First Person") FTransform ArmsRelativeTransform = FTransform(FRotator::ZeroRotator, FVector(8, 0, -155));
    UFUNCTION(BlueprintCallable, Category="Character") void ApplySelectedCharacter();
    UFUNCTION(BlueprintPure, Category="Movement") UStoryMovementComponent* GetStoryMovement() const;
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
    UPROPERTY(Transient) TObjectPtr<ASeniorRunnerVisual> RunnerVisual;
    UPROPERTY(Transient) TObjectPtr<ASeniorFixerVisual> FixerVisual;
    UPROPERTY(ReplicatedUsing=OnRep_ReplicatedCrawl) bool bReplicatedCrawl = false;
    int32 AppliedCharacterIndex = INDEX_NONE;
    bool bWalkingAnimation = false;
    float DouliFlightBlend = 0;
    float SlideCameraOffsetZ = 0;
    float CrawlVisualBlend = 0;
    float CrawlStrokePhase = 0;
    float StandingVisualHalfHeight = 90.f;
    float StandingEyeHeightFromGround = 154.f;
    float StandingMeshOffsetZ = -90.f;
    float FurnitureCameraLeanX = 0;
    float PigCarryPoseBlend = 0;
    float PigFingerGripBlend = 0;
    FVector PigGripWorld = FVector::ZeroVector;
    FVector PigGripAxisWorld = FVector::UpVector;
    FVector PigHandForwardWorld = FVector(0, -1, 0);
    void UpdatePigCarryAnimation(float DeltaSeconds, AHouseRoamingPig* HeldPig);
    FSeniorClothMotion ClothMotion;
    void ConfigureLocalInput();
    void StartSprint();
    void StopSprint();
    void StartSlide();
    void StopSlide();
    void ToggleCrawl();
    UFUNCTION() void OnRep_VentCrawlForced();
    UFUNCTION() void OnRep_ReplicatedCrawl();
    void StartMovementJump();
    void OnLeftMousePressed();
    void OnShootReleased();
    AExistingHouseDoor* FindFocusedExistingDoor(float MinimumFacing) const;
    bool InteractWithExistingDoor(AExistingHouseDoor* Door);
    void InteractWithExistingDoorOrFurniture();
    void ToggleHouseMaskCursor();
    void UpdateHouseMaskVisual();
    UFUNCTION() void OnRep_HouseMask();
    UFUNCTION() void OnRep_ClubPoleJourney(const FHouseClubPoleJourney& Previous);
    UFUNCTION(Server, Reliable) void ServerCollectHouseMask(AHouseOxbloodMaskPickup* Pickup);
    UFUNCTION(Server, Reliable) void ServerToggleHouseMask();
    UFUNCTION(Server, Reliable) void ServerDealHousePokerTable(AHousePokerTable* Table);
    UFUNCTION(Client, Reliable) void ClientHouseMaskNotice(const FString& Message);
    UFUNCTION(Server, Reliable) void ServerInteractExistingDoor(class AExistingHouseDoor* Door);
    UFUNCTION(Server, Reliable) void ServerInteractVent(FName PortalId);
    UFUNCTION(Server, Reliable) void ServerInteractHousePig(AHouseRoamingPig* Pig);
    UFUNCTION(Server, Reliable) void ServerInteractBoywick(ABoywickCharacter* Boywick);
    uint8 BoywickBlessingApplied = 0;
    float BoywickBaseWalk = 500.f, BoywickBaseSprint = 750.f, BoywickBaseJump = 520.f;
    UFUNCTION(Client, Reliable) void ClientNotifyDoorwayOccupied();
    TWeakObjectPtr<AExistingHouseDoor> LastClickedDoor;
    double LastDoorClickTime = -1000.0;
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void LookMouse(const FInputActionValue& Value);
    void LookMouseX(float AxisValue);
    void LookMouseY(float AxisValue);
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
    AStoryHUD();
    virtual void DrawHUD() override;
private:
    UPROPERTY(Transient) TObjectPtr<class UTexture2D> HouseMaskIcon;
    bool bWasPressed = false;
    bool bThrowTouchPressed = false;
};
