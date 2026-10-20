#include "StoryCampaign.h"
#include "HouseFurnitureCarrySmoke.h"
#include "HouseCarryTiersReview.h"
#include "HouseReactiveFixture.h"
#include "HouseFixtureSmoke.h"
#include "HouseGadgets.h"
#include "HouseInteractiveProp.h"
#include "HouseDamageSubsystem.h"
#include "SeniorPlayerPreferences.h"
#include "StoryMovementComponent.h"
#include "HouseFurnitureInteractionComponent.h"
#include "HouseBasketball.h"
#include "HouseBasketballHoop.h"
#include "HouseWoodcutting.h"
#include "ExistingHouseDoor.h"
#include "HouseRearPorch.h"
#include "HouseOxbloodMask.h"
#include "HousePokerTable.h"
#include "HouseRoamingPig.h"
#include "BoywickCharacter.h"
#include "BoywickRoom.h"
#include "HouseVentNetwork.h"
#include "HouseImpossibleHallway.h"
#include "HouseWorldScale.h"
#include "SeniorLobbyUI.h"
#include "SeniorCharacterRoster.h"
#include "SeniorBraxtonVisual.h"
#include "SeniorRunnerVisual.h"
#include "SeniorFixerVisual.h"
#include "SeniorDouli.h"
#include "SeniorSauce.h"
#include "SeniorDouliAnim.h"
#include "SeniorDouliIdle.h"
#include "SeniorPigHoldPose.h"
#include "SeniorFurnitureHoldPose.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Math/RotationMatrix.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "MoviePlayer.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HouseCampusInteriors.h"
#if WITH_EDITOR
void TickStorySmokeTest(UWorld* World);
void TickDouliSmokeTest(AStoryFirstPersonCharacter* Pawn);
void TickSauceSmokeTest(AStoryFirstPersonCharacter* Pawn);
#endif
#if !UE_BUILD_SHIPPING
void TickCombinedHouseSmoke(UWorld* World);
void TickHouseVisualReview(UWorld* World);
void TickHouseMaskReview(UWorld* World);
void TickExistingHouseDoorSmoke(UWorld* World);
void TickRoom101EntrySmoke(UWorld* World);
void TickNightClubSmoke(UWorld* World);
void TickNightClubEntranceSmoke(UWorld* World);
void TickHouseTraversalSmoke(UWorld* World);
void TickHouseChairSmoke(UWorld* World);
void TickHousePokerSmoke(UWorld* World);
void TickHousePigSmoke(UWorld* World);
void TickHouseImpossibleHallwaySmoke(UWorld* World);
void TickHouseHotTubWaterSmoke(UWorld* World);
void TickHouseInteractivitySmoke(UWorld* World);
void TickHouseLibrarySmoke(UWorld* World);
void TickHouseKitchenSmoke(UWorld* World);
void TickHouseTelevisionSmoke(UWorld* World);
void TickHouseVentSmoke(UWorld* World);
#endif

void UStoryCampaign::Init()
{
    Super::Init();
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("HouseBasketballSkillReview"))) SaveSlot = TEXT("SeniorSendoff_BasketballSkillAutomationOnly");
    if (FParse::Param(FCommandLine::Get(), TEXT("HouseWoodcuttingReview"))) SaveSlot = TEXT("SeniorSendoff_WoodcuttingAutomationOnly");
    if (FParse::Param(FCommandLine::Get(), TEXT("HouseBasketballReview"))) SaveSlot = TEXT("SeniorSendoff_BasketballAutomationOnly");
#endif
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseInteractivitySmoke"))) SaveSlot=TEXT("SeniorSendoff_HouseInteractivityAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseGlassSmoke"))) SaveSlot=TEXT("SeniorSendoff_HouseGlassAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseLibrarySmoke"))) SaveSlot=TEXT("SeniorSendoff_LibraryAutomationOnly");
#endif
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(),TEXT("CombinedHouseSmoke"))) SaveSlot=TEXT("SeniorSendoff_CombinedHouseAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseVisualReview"))) SaveSlot=TEXT("SeniorSendoff_HouseVisualAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseMaskReview"))) SaveSlot=TEXT("SeniorSendoff_HouseMaskAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("ExistingHouseDoorSmoke"))) SaveSlot=TEXT("SeniorSendoff_OriginalDoorAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("FrontDoubleDoorReview"))) SaveSlot=TEXT("SeniorSendoff_FrontDoorAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseConnectionsReview"))) SaveSlot=TEXT("SeniorSendoff_ConnectionsAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseRearPorchReview"))) SaveSlot=TEXT("SeniorSendoff_RearPorchAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseExteriorWalkReview"))) SaveSlot=TEXT("SeniorSendoff_ExteriorWalkAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseFrontBushReview"))) SaveSlot=TEXT("SeniorSendoff_FrontBushAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("NightClubEntranceSmoke"))) SaveSlot=TEXT("SeniorSendoff_NightClubEntryAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("NightClubSmoke"))) SaveSlot=TEXT("SeniorSendoff_NightClubAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("Room101EntrySmoke"))) SaveSlot=TEXT("SeniorSendoff_Room101AutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseClubPoleSmoke")) || FParse::Param(FCommandLine::Get(),TEXT("ClubPoleMultiplayerSmoke")))
        SaveSlot=TEXT("SeniorSendoff_ClubPoleAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseTraversalSmoke"))) SaveSlot=TEXT("SeniorSendoff_HouseTraversalAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseChairSmoke"))) SaveSlot=TEXT("SeniorSendoff_HouseChairAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HousePokerSmoke"))) SaveSlot=TEXT("SeniorSendoff_HousePokerAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseImpossibleHallwaySmoke"))) SaveSlot=TEXT("SeniorSendoff_HallwayAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseVentSmoke"))) SaveSlot=TEXT("SeniorSendoff_VentAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseHotTubWaterSmoke"))) SaveSlot=TEXT("SeniorSendoff_HotTubWaterAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HousePigSmoke")) || FParse::Param(FCommandLine::Get(),TEXT("HousePigStairSmoke"))
        || FParse::Param(FCommandLine::Get(),TEXT("HousePigUpgradeSmoke"))
        || FParse::Param(FCommandLine::Get(),TEXT("HousePigMotionSmoke"))
        || FParse::Param(FCommandLine::Get(),TEXT("HousePigAnimalSmoke"))) SaveSlot=TEXT("SeniorSendoff_HousePigAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("LobbyStartupSmoke"))) SaveSlot=TEXT("SeniorSendoff_LobbyStartupAutomationOnly");
#endif
#if WITH_EDITOR
    FString TestMode;
    if (FParse::Param(FCommandLine::Get(),TEXT("DouliTest"))) SaveSlot=TEXT("SeniorSendoff_DouliAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("SauceTest"))) SaveSlot=TEXT("SeniorSendoff_SauceAutomationOnly");
    if (FParse::Value(FCommandLine::Get(), TEXT("StorySmokeTest="), TestMode)) SaveSlot = TEXT("SeniorSendoff_AutomationOnly");
    else if (FParse::Value(FCommandLine::Get(), TEXT("LobbySmokeTest="), TestMode)) SaveSlot = TEXT("SeniorSendoff_LobbyAutomationOnly");
#endif
    Progress = Cast<UStorySave>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
    if (!Progress || Progress->Chapter < 1 || Progress->Chapter > 3 || Progress->Checkpoint < 0 || Progress->Checkpoint > 2)
        Progress = Cast<UStorySave>(UGameplayStatics::CreateSaveGameObject(UStorySave::StaticClass()));
    Progress->Difficulty = FMath::Clamp(Progress->Difficulty, 0, 2);
    SelectedDifficulty = Progress->Difficulty;
    BeforeHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UStoryCampaign::BeforeLoad);
    AfterHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UStoryCampaign::AfterLoad);
}
void UStoryCampaign::Shutdown()
{
    FCoreUObjectDelegates::PreLoadMap.Remove(BeforeHandle);
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(AfterHandle);
    Super::Shutdown();
}
TSubclassOf<AGameModeBase> UStoryCampaign::OverrideGameModeClass(TSubclassOf<AGameModeBase> GameModeClass,
    const FString& MapName, const FString& Options, const FString& Portal) const
{
    // The lobby widget requires this mode's controller and replicated party
    // state. Stale World Settings or inherited travel options must not replace
    // that contract. Unreal passes a PIE-prefixed map name during editor Play.
    const bool bLobby = MapName == TEXT("Lobby")
        || (MapName.StartsWith(TEXT("UEDPIE_")) && MapName.EndsWith(TEXT("_Lobby")));
    if (bLobby)
    {
        if (GameModeClass != ASeniorLobbyGameMode::StaticClass())
            UE_LOG(LogTemp, Warning, TEXT("LOBBY_GAME_MODE_REPAIRED: map=%s requested=%s; using SeniorLobbyGameMode"),
                *MapName, *GetNameSafe(GameModeClass.Get()));
        return ASeniorLobbyGameMode::StaticClass();
    }
    return Super::OverrideGameModeClass(GameModeClass, MapName, Options, Portal);
}
FName UStoryCampaign::ChapterMap(int32 Chapter)
{
    if (Chapter == 1) return FName(TEXT("/Game/Story/Maps/Chapter01_House"));
    return FName(*FString::Printf(TEXT("/Game/Story/Maps/Chapter%02d"), FMath::Clamp(Chapter, 1, 3)));
}
int32 UStoryCampaign::CurrentChapter() const
{
    const FString Map = UGameplayStatics::GetCurrentLevelName(this, true);
    if (Map == TEXT("Chapter01_House")) return 1;
    for (int32 Chapter = 1; Chapter <= 3; ++Chapter)
        if (Map == FString::Printf(TEXT("Chapter%02d"), Chapter)) return Chapter;
    return 0;
}
void UStoryCampaign::Notify(const FString& Message)
{
    Status = Message;
    StatusUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + 5.f : 5.f;
    if (GetWorld() && GetWorld()->GetNetMode() != NM_Client)
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (ASeniorLobbyController* PC = Cast<ASeniorLobbyController>(It->Get()); PC && !PC->IsLocalController())
                PC->ShowLobbyMessage(Message);
}
bool UStoryCampaign::CanManageStory() const
{
    return GetWorld() && GetWorld()->GetNetMode() != NM_Client && Progress;
}
void UStoryCampaign::ReplicateProgress()
{
    if (!CanManageStory()) return;
    if (AStoryGameState* State = GetWorld()->GetGameState<AStoryGameState>())
    {
        State->StoryChapter = Progress->Chapter;
        State->StoryCheckpoint = Progress->Checkpoint;
        State->StoryDifficulty = Progress->Difficulty;
        State->bStoryCompleted = Progress->bCompleted;
        State->ForceNetUpdate();
    }
}
bool UStoryCampaign::SaveProgress()
{
    if (!CanManageStory()) return false;
    ReplicateProgress();
    const bool bSaved = UGameplayStatics::SaveGameToSlot(Progress, SaveSlot, 0);
    if (!bSaved) Notify(TEXT("Could not save progress. Check available storage."));
    return bSaved;
}
FString UStoryCampaign::DifficultyName(int32 Difficulty)
{
    static const TCHAR* Names[] = { TEXT("EASY"), TEXT("NORMAL"), TEXT("HARD") };
    return Names[FMath::Clamp(Difficulty, 0, 2)];
}
void UStoryCampaign::SetDifficulty(int32 NewDifficulty)
{
    SelectedDifficulty = FMath::Clamp(NewDifficulty, 0, 2);
}
void UStoryCampaign::ResumeStory()
{
    if (!CanManageStory() || bTravelPending) return;
    Progress->Difficulty = SelectedDifficulty;
    if (SaveProgress()) Travel(Progress->Chapter);
}
void UStoryCampaign::NewStory()
{
    if (!CanManageStory() || bTravelPending) return;
    const int32 NewDifficulty = FMath::Clamp(SelectedDifficulty, 0, 2);
    Progress = Cast<UStorySave>(UGameplayStatics::CreateSaveGameObject(UStorySave::StaticClass()));
    Progress->Difficulty = NewDifficulty;
    if (SaveProgress()) Travel(1);
}
bool UStoryCampaign::ReachCheckpoint(int32 Chapter, int32 Index, FTransform Spawn)
{
    if (!CanManageStory() || bTravelPending || Progress->bCompleted || Chapter != CurrentChapter() || Index < 1 || Index > 2) return false;
    if (Chapter < Progress->Chapter || (Chapter == Progress->Chapter && Index <= Progress->Checkpoint)) return false;
    // A chapter cannot be skipped by opening a later map directly.
    if (Chapter != Progress->Chapter || Index != Progress->Checkpoint + 1) { Notify(TEXT("Reach the previous checkpoint first.")); return false; }
    Progress->Checkpoint = Index;
    Progress->Spawn = Spawn;
    Progress->bHasCheckpoint = true;
    if (SaveProgress()) Notify(FString::Printf(TEXT("Checkpoint %d saved"), Index));
    return true;
}
void UStoryCampaign::CompleteChapter()
{
    if (!CanManageStory() || bTravelPending || Progress->bCompleted || CurrentChapter() != Progress->Chapter || Progress->Checkpoint < 2) return;
    if (Progress->Chapter == 3)
    {
        Progress->bCompleted = true;
        if (SaveProgress()) Notify(TEXT("Senior Sendoff - story complete!"));
        return;
    }
    ++Progress->Chapter;
    Progress->Checkpoint = 0;
    Progress->bHasCheckpoint = false;
    Progress->Spawn = FTransform::Identity;
    // Keep the next chapter in memory even if storage fails, and report the failure.
    SaveProgress();
    Travel(Progress->Chapter);
}
void UStoryCampaign::Travel(int32 Chapter)
{
    if (!CanManageStory() || bTravelPending) return;
    bTravelPending = true;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (ASeniorLobbyController* PC = Cast<ASeniorLobbyController>(It->Get())) PC->ClientBeginStoryLoading(Chapter);
    if (APlayerController* PC = GetFirstLocalPlayerController())
        if (APawn* Pawn = PC->GetPawn()) Pawn->DisableInput(PC);
    const FName Destination = ChapterMap(Chapter);
    // Give the HUD a frame to paint before synchronous map travel begins.
    GetWorld()->GetTimerManager().SetTimer(TravelTimer, [this, Destination]() {
        UWorld* World = GetWorld();
        if (!World || World->GetNetMode() == NM_Client) { bTravelPending = false; return; }
        if (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer)
        {
            // Explicit game mode avoids carrying the lobby's game URL option forward.
            const FString URL = Destination.ToString() + TEXT("?game=/Script/SeniorSendoff.StoryGameMode");
            if (!World->ServerTravel(URL, true))
            {
                bTravelPending = false;
                if (ASeniorLobbyGameState* State = World->GetGameState<ASeniorLobbyGameState>())
                {
                    State->bStarting = false;
                    State->ForceNetUpdate();
                }
                for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
                    if (ASeniorLobbyController* PC = Cast<ASeniorLobbyController>(It->Get())) PC->ClientCancelStoryLoading();
                if (APlayerController* PC = GetFirstLocalPlayerController())
                    if (APawn* Pawn = PC->GetPawn()) Pawn->EnableInput(PC);
                Notify(TEXT("Could not enter the chapter. Return to the lobby and try again."));
            }
        }
        else UGameplayStatics::OpenLevel(this, Destination, true, TEXT("game=/Script/SeniorSendoff.StoryGameMode"));
    }, 0.25f, false);
}
void UStoryCampaign::BeforeLoad(const FString& MapName)
{
    if (IsRunningDedicatedServer() || !GetWorld() || GetWorld()->WorldType == EWorldType::PIE) return;
    FLoadingScreenAttributes Screen;
    Screen.bAutoCompleteWhenLoadingCompletes = true;
    Screen.MinimumLoadingScreenDisplayTime = 0.5f;
    int32 LoadingChapter = Progress ? Progress->Chapter : 1;
    for (int32 Chapter = 1; Chapter <= 3; ++Chapter)
        if (MapName.Contains(FString::Printf(TEXT("Chapter%02d"), Chapter))) LoadingChapter = Chapter;
    Screen.WidgetLoadingScreen = MakeSeniorLoadingWidget(LoadingChapter);
    GetMoviePlayer()->SetupLoadingScreen(Screen);
}
void UStoryCampaign::AfterLoad(UWorld* World) { if (World && World->GetGameInstance() == this) bTravelPending = false; }
void UStoryCampaign::RespawnAtCheckpoint()
{
    RespawnPlayerAtCheckpoint(GetFirstLocalPlayerController());
}
void UStoryCampaign::RespawnPlayerAtCheckpoint(APlayerController* PC)
{
    if (!CanManageStory() || bTravelPending || CurrentChapter() == 0 || Progress->bCompleted) return;
    AGameModeBase* GM = GetWorld()->GetAuthGameMode();
    if (!PC || !GM) return;
    if (APawn* OldPawn = PC->GetPawn()) { PC->UnPossess(); OldPawn->Destroy(); }
    GM->RestartPlayer(PC);
    Notify(CurrentChapter() != 1 && Progress->bHasCheckpoint ? TEXT("Returned to checkpoint") : TEXT("Returned to chapter start"));
}

void AStoryGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AStoryGameState, StoryChapter);
    DOREPLIFETIME(AStoryGameState, StoryCheckpoint);
    DOREPLIFETIME(AStoryGameState, StoryDifficulty);
    DOREPLIFETIME(AStoryGameState, bStoryCompleted);
}

AStoryGameMode::AStoryGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
    DefaultPawnClass = AStoryFirstPersonCharacter::StaticClass();
    PlayerControllerClass = ASeniorLobbyController::StaticClass();
    PlayerStateClass = ASeniorLobbyPlayerState::StaticClass();
    GameStateClass = AStoryGameState::StaticClass();
    bUseSeamlessTravel = true;
    HUDClass = AStoryHUD::StaticClass();
}

AStoryFirstPersonCharacter::AStoryFirstPersonCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UStoryMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.f;
    bReplicates = true;
    SetReplicateMovement(true);
    SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    // Match the validated measured-house door clearance while retaining a
    // human-height capsule; roster stature may later adjust only half-height.
    GetCapsuleComponent()->InitCapsuleSize(32.f, 90.f);
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = true;
    bUseControllerRotationRoll = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;

    // Keep movement's mesh offset and network smoothing intact. This pivot
    // poses every roster body, including the three attached MetaHuman visuals.
    CrawlVisualPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CrawlVisualPivot"));
    CrawlVisualPivot->SetupAttachment(GetCapsuleComponent());
    GetMesh()->SetupAttachment(CrawlVisualPivot);

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(0, 0, 64.f));
    FirstPersonCamera->bUsePawnControlRotation = true;

    // A full body is visible to teammates, with no head clipping in the owner's camera.
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    static ConstructorHelpers::FClassFinder<UAnimInstance> Anim(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
    GetMesh()->SetSkeletalMesh(Body.Object);
    GetMesh()->SetAnimInstanceClass(Anim.Class);
    GetMesh()->SetRelativeLocation(FVector(0, 0, -90));
    GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
    GetMesh()->SetOwnerNoSee(true);
    GetMesh()->bCastHiddenShadow = true;
    FallbackBody = Body.Object;
    FallbackAnimClass = Anim.Class;

    FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
    FirstPersonArms->SetupAttachment(FirstPersonCamera);
    FirstPersonArms->SetRelativeTransform(ArmsRelativeTransform);
    FirstPersonArms->SetOnlyOwnerSee(true);
    FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FirstPersonArms->SetGenerateOverlapEvents(false);
    FirstPersonArms->SetCastShadow(false);
    FirstPersonArms->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    FirstPersonArms->SetHiddenInGame(true);
    FurnitureInteraction = CreateDefaultSubobject<UHouseFurnitureInteractionComponent>(TEXT("FurnitureInteraction"));
    HouseMaskVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EquippedHouseMask"));
    HouseMaskVisual->SetupAttachment(GetCapsuleComponent());
    HouseMaskVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HouseMaskVisual->SetGenerateOverlapEvents(false);
    HouseMaskVisual->SetOwnerNoSee(true);
    HouseMaskVisual->SetVisibility(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> HouseMask(
        TEXT("/Game/Items/HouseMask/SM_HouseOxbloodMaskWorn.SM_HouseOxbloodMaskWorn"));
    HouseMaskVisual->SetStaticMesh(HouseMask.Object);

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> Context(TEXT("/Game/Input/IMC_Default.IMC_Default"));
    static ConstructorHelpers::FObjectFinder<UInputAction> Move(TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
    static ConstructorHelpers::FObjectFinder<UInputAction> Look(TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
    static ConstructorHelpers::FObjectFinder<UInputAction> MouseLook(TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
    static ConstructorHelpers::FObjectFinder<UInputAction> Jump(TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
    InputContext = Context.Object;
    MoveAction = Move.Object;
    LookAction = Look.Object;
    MouseLookAction = MouseLook.Object;
    JumpAction = Jump.Object;
}
void AStoryFirstPersonCharacter::BeginPlay()
{
    Super::BeginPlay();
    ApplySelectedCharacter();
    ConfigureLocalInput();
}
void AStoryFirstPersonCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (ClubPoleJourney.Pole) ClubPoleJourney.Pole->UpdateTraveller(this, DeltaSeconds);
    UpdateHouseImpossibleHallwayTraveler(this);
    UpdateBoywickBlessing(DeltaSeconds);
    const UStoryMovementComponent* StoryMovement = GetStoryMovement();
    const bool bCrawling = StoryMovement && StoryMovement->IsCrawling();
    if (FirstPersonArms)
        FirstPersonArms->SetHiddenInGame(!FirstPersonArms->GetSkeletalMeshAsset());
    if (HasAuthority() && bReplicatedCrawl != bCrawling)
    {
        bReplicatedCrawl = bCrawling;
        ForceNetUpdate();
    }
    CrawlVisualBlend = FMath::FInterpTo(CrawlVisualBlend, bCrawling ? 1.f : 0.f,
        DeltaSeconds, bCrawling ? 14.f : 9.f);
    if (!bCrawling && CrawlVisualBlend < .001f) CrawlVisualBlend = 0.f;
    if (CrawlVisualPivot)
    {
        const float Speed = GetVelocity().Size2D();
        if (bCrawling && Speed > 4.f) CrawlStrokePhase += Speed * DeltaSeconds / 38.f;
        const float Stroke = FMath::Sin(CrawlStrokePhase);
        const float Blend = CrawlVisualBlend;
        CrawlVisualPivot->SetRelativeLocation(FVector(
            Blend * (-.9f * StandingVisualHalfHeight + 2.f * Stroke),
            Blend * 1.6f * FMath::Cos(CrawlStrokePhase),
            Blend * (-5.f + 1.2f * FMath::Abs(Stroke))));
        CrawlVisualPivot->SetRelativeRotation(FRotator(
            Blend * (-87.f + 1.5f * Stroke),
            Blend * 1.8f * FMath::Cos(CrawlStrokePhase),
            Blend * 2.f * Stroke));
    }
    AHouseRoamingPig* HeldPig=CarriedHousePig();
    if (FirstPersonCamera)
    {
        if(IsLocallyControlled())
        {
        const float WantedFOV = SeniorPlayerPreferences::Get().FieldOfView;
        bool bUsePlayerFov = true;
#if !UE_BUILD_SHIPPING
        // The opt-in campaign render sweep fits the registered photo FOV.
        bUsePlayerFov = !FParse::Param(FCommandLine::Get(), TEXT("HouseVisualReview"));
#endif
        if (bUsePlayerFov && !FMath::IsNearlyEqual(FirstPersonCamera->FieldOfView, WantedFOV))
            FirstPersonCamera->SetFieldOfView(WantedFOV);
        }
        if(HeldPig || (FurnitureInteraction && FurnitureInteraction->GetCarryPoseBlend()>.001f))
        {
            // Pose against this frame's aim before the pig and arms tick.
            // A later camera-manager update must not rotate the camera basis
            // after IK has already solved the hand against the actual ankle.
            FirstPersonCamera->SetWorldRotation(GetBaseAimRotation());
        }
        // The capsule's crouch already lowers the viewpoint. A further eased
        // drop gives a sprint slide a readable low viewpoint without changing
        // its collision height or an observer's camera.
        const bool bSliding = StoryMovement && StoryMovement->IsSliding();
        // Bend down to reach the actual rear ankle before lifting it. The
        // envelope recovers before the suspended hold, preserving head-to-
        // floor clearance for the larger pig. Never move the view below a
        // forty-five-centimetre eye height above the character's foot plane.
        // This brief deep reach can grasp a standing pig's grounded rear hoof.
        const float ViewScale=GetCapsuleComponent()->GetComponentScale().Z;
        const float MaxPigBend=FMath::Clamp(GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()+BaseEyeHeight-45.f/ViewScale,0.f,115.f/ViewScale);
        const float PigBend=HeldPig?MaxPigBend*HeldPig->GetPickupBend():0.f;
        const float FurnitureBend=FurnitureInteraction
            ?FMath::Min(FurnitureInteraction->GetPickupBend()/ViewScale,MaxPigBend):0.f;
        const float TargetOffset = bCrawling ? -7.f : bSliding ? -36.f : -FMath::Max(PigBend,FurnitureBend);
        SlideCameraOffsetZ = FMath::FInterpTo(SlideCameraOffsetZ, TargetOffset,
            DeltaSeconds, bSliding || bCrawling ? 15.f : 10.f);
        FurnitureCameraLeanX=FMath::FInterpTo(FurnitureCameraLeanX,
            FurnitureInteraction?FurnitureInteraction->GetPickupLean()/ViewScale:0.f,DeltaSeconds,8.f);
        // Effort, footstep bob and throw lunge while handling furniture.
        FVector Effort=FurnitureInteraction?FurnitureInteraction->GetCameraEffortOffset()/ViewScale:FVector::ZeroVector;
        // A nearby crack or break jolts the view.
        if(IsLocallyControlled())
            if(const UHouseDamageSubsystem* Damage=GetWorld()->GetSubsystem<UHouseDamageSubsystem>())
                Effort+=Damage->SampleCameraImpact(FirstPersonCamera->GetComponentLocation())/ViewScale;
        FirstPersonCamera->SetRelativeLocation(FVector(FurnitureCameraLeanX+Effort.X+(bCrawling?8.f:0.f),
            Effort.Y, BaseEyeHeight + SlideCameraOffsetZ + Effort.Z));
    }
    const auto* Selection=GetPlayerState<ASeniorLobbyPlayerState>();
    const bool Equipped=Selection && Selection->CharacterIndex==0 && Selection->LoadoutIndex==0;
    const bool SauceEquipped=Selection && Selection->CharacterIndex==0 && Selection->LoadoutIndex==1;
    if (HasAuthority() && Equipped && !Douli)
    {
        FActorSpawnParameters P; P.Owner=this; P.Instigator=this;
        P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Douli=GetWorld()->SpawnActor<ASeniorDouli>(GetActorLocation(),GetActorRotation(),P);
    }
    if (HasAuthority() && !Equipped && Douli) { Douli->Destroy(); Douli=nullptr; }
    if (HasAuthority() && SauceEquipped && !SaucePacket)
    {
        FActorSpawnParameters P;P.Owner=this;P.Instigator=this;
        P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SaucePacket=GetWorld()->SpawnActor<ASeniorSaucePacket>(GetActorLocation(),GetActorRotation(),P);
    }
    if (HasAuthority() && !SauceEquipped && SaucePacket) { SaucePacket->Destroy();SaucePacket=nullptr; }
    float Motion=0;
    if (Douli)
    {
        const float Age=Douli->GetPhaseAge();
        if (Douli->Phase==EDouliPhase::Windup) Motion=Age<.18f
            ? -FMath::SmoothStep(0.f,.18f,Age)
            : FMath::Lerp(-1.f,1.f,FMath::SmoothStep(.18f,.32f,Age));
        else if (Douli->Phase==EDouliPhase::Outbound) Motion=1.f-FMath::SmoothStep(0.f,.28f,Age);
        else if (Douli->Phase==EDouliPhase::Catching) Motion=.25f*FMath::Sin(FMath::Clamp(Age/.24f,0.f,1.f)*PI);
    }
    const bool bReaching=Douli && !Douli->IsHeld() && (Douli->Phase!=EDouliPhase::Outbound || Douli->GetPhaseAge()>.2f);
    DouliFlightBlend=FMath::FInterpTo(DouliFlightBlend,bReaching?1.f:0.f,DeltaSeconds,12.f);
    UpdatePigCarryAnimation(DeltaSeconds,HeldPig);
    const bool bHandsBusy=HeldPig || PigCarryPoseBlend>.02f || (FurnitureInteraction && FurnitureInteraction->GetCarryPoseBlend()>.002f);
    // Evaluate actual contact even if the arm temporarily leaves the owner's
    // screen during a bend. Remote clients also pose their owner-only arms so
    // a carried pig does not lag a hidden stale wrist. Dedicated servers keep
    // the existing visibility policy and use the bounded reference reach.
    FirstPersonArms->VisibilityBasedAnimTickOption=bHandsBusy && GetNetMode()!=NM_DedicatedServer
        ?EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones
        :EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    // Keep reusable equipment alive and restore it after the hand returns.
    // Flying weapons retain their own visible flight and recovery behavior.
    if(Douli) Douli->SetActorHiddenInGame(bCrawling || (bHandsBusy && Douli->IsHeld()));
    if(SaucePacket) SaucePacket->SetActorHiddenInGame(bCrawling || (bHandsBusy && SaucePacket->Phase==ESeniorSaucePhase::Ready));
    if (BraxtonVisual) BraxtonVisual->SetDouliEquipped(Equipped && !bHandsBusy,Motion,DouliFlightBlend);
    if ((Equipped || bHandsBusy || CrawlVisualBlend>.001f) && FirstPersonArms->GetSkeletalMeshAsset() && !Cast<USeniorDouliAnim>(FirstPersonArms->GetAnimInstance()))
        FirstPersonArms->SetAnimInstanceClass(USeniorDouliAnim::StaticClass());
    if (auto* A=Cast<USeniorDouliAnim>(FirstPersonArms->GetAnimInstance()))
    {
        A->BaseSequence=CurrentArmsAnimation; A->bFirstPerson=true; A->bEquipped=Equipped && !bHandsBusy; A->Motion=Motion; A->FlightBlend=DouliFlightBlend;
        A->CrawlBlend=CrawlVisualBlend; A->CrawlPhase=CrawlStrokePhase;
        A->CrawlSpeed=bCrawling?GetVelocity().Size2D():0.f;
        A->bHoldingPig=HeldPig!=nullptr; A->bPigPlacing=HeldPig && HeldPig->IsPlacing();
        A->PigTargetActor=HeldPig;
        A->FurnitureCarryTarget=FurnitureInteraction;
        A->PigCarryBlend=PigCarryPoseBlend; A->PigGripBlend=PigFingerGripBlend;
        const FTransform Component=FirstPersonArms->GetComponentTransform();
        A->PigGripTarget=Component.InverseTransformPosition(PigGripWorld);
        A->PigGripDirection=Component.InverseTransformVectorNoScale(PigGripAxisWorld).GetSafeNormal();
        A->PigHandForward=Component.InverseTransformVectorNoScale(PigHandForwardWorld).GetSafeNormal();
    }
    if(BraxtonVisual) BraxtonVisual->SetPigCarryPose(PigCarryPoseBlend,PigFingerGripBlend,PigGripWorld,PigGripAxisWorld,PigHandForwardWorld,HeldPig && HeldPig->IsPlacing(),HeldPig);
    if (BraxtonVisual) BraxtonVisual->SetFurnitureCarryPose(FurnitureInteraction);
    if (RunnerVisual) RunnerVisual->SetFurnitureCarryPose(FurnitureInteraction);
    if (FixerVisual) FixerVisual->SetFurnitureCarryPose(FurnitureInteraction);
    if (BraxtonVisual) BraxtonVisual->SetMoveSpeed(GetVelocity().Size2D());
    else if (RunnerVisual) RunnerVisual->SetMoveSpeed(GetVelocity().Size2D());
    else if (FixerVisual) FixerVisual->SetMoveSpeed(GetVelocity().Size2D());
    else UpdateBodyAnimation();
    // The body pose runs on every viewing client. Only the movement state is
    // replicated; each animation worker receives scalar values from this tick.
    USkeletalMeshComponent* CrawlBody = BraxtonVisual ? BraxtonVisual->GetBodyMesh()
        : RunnerVisual ? RunnerVisual->GetBodyMesh()
        : FixerVisual ? FixerVisual->GetBodyMesh() : GetMesh();
    UAnimSequence* CrawlIdle = BraxtonVisual ? BraxtonVisual->GetIdleAnimation()
        : RunnerVisual ? RunnerVisual->GetIdleAnimation()
        : FixerVisual ? FixerVisual->GetIdleAnimation() : BodyIdle.Get();
    if (CrawlBody && CrawlBody->GetSkeletalMeshAsset())
    {
        if (CrawlVisualBlend > .001f && !Cast<USeniorDouliAnim>(CrawlBody->GetAnimInstance()))
            CrawlBody->SetAnimInstanceClass(USeniorDouliAnim::StaticClass());
        if (auto* Anim = Cast<USeniorDouliAnim>(CrawlBody->GetAnimInstance()))
        {
            Anim->CrawlBlend = CrawlVisualBlend;
            Anim->CrawlPhase = CrawlStrokePhase;
            Anim->CrawlSpeed = bCrawling ? GetVelocity().Size2D() : 0.f;
            if (CrawlVisualBlend > .001f && CrawlIdle) Anim->BaseSequence = CrawlIdle;
        }
    }
    if (bHouseMaskEquipped) UpdateHouseMaskVisual();
    if (FurnitureInteraction && FurnitureInteraction->IsBasketballCharging() && IsLocallyControlled())
        if (const auto* PC = Cast<APlayerController>(Controller); PC && (PC->bShowMouseCursor || PC->IsInputKeyDown(EKeys::Escape)))
            FurnitureInteraction->CancelBasketballShot();
    if (AppliedCharacterIndex == 0 && !BraxtonVisual && GetNetMode() != NM_DedicatedServer)
        ClothMotion.Update(GetMesh(), DeltaSeconds, GetActorRotation().Yaw, GetVelocity().Size2D());
#if WITH_EDITOR
    TickHouseFurnitureCarrySmoke(this);
    TickHouseCarryTiersReview(this);
    TickHouseBasketballReview(this);
    TickHouseBasketballSkillReview(this);
    TickHouseWoodcuttingReview(this);
    TickHouseRearPorchReview(this);
    TickDouliSmokeTest(this);
    TickSauceSmokeTest(this);
#endif
}
void AStoryFirstPersonCharacter::UpdatePigCarryAnimation(float DeltaSeconds,AHouseRoamingPig* HeldPig)
{
    if(HeldPig)
    {
        PigGripWorld=HeldPig->GetCarryGripPoint();
        PigGripAxisWorld=HeldPig->GetCarryGripDirection();
        // Keep the fingers across the leg. The hanging rear ankle points
        // sideways across the view, so camera-forward makes a stable palm
        // plane through the floor reach, lift, and suspended hold.
        const FVector ViewForward=FirstPersonCamera?FirstPersonCamera->GetForwardVector():GetActorForwardVector();
        const FVector LegAxis=PigGripAxisWorld.GetSafeNormal();
        PigHandForwardWorld=FVector::VectorPlaneProject(ViewForward,LegAxis).GetSafeNormal();
        if(PigHandForwardWorld.IsNearlyZero())
        {
            const FVector ViewUp=FirstPersonCamera?FirstPersonCamera->GetUpVector():FVector::UpVector;
            PigHandForwardWorld=FVector::VectorPlaneProject(ViewUp,LegAxis).GetSafeNormal();
            if(PigHandForwardWorld.IsNearlyZero())PigHandForwardWorld=FVector::VectorPlaneProject(GetActorRightVector(),LegAxis).GetSafeNormal();
        }
        const float Reach=HeldPig->GetCarryBlend();
        const float Target=FMath::SmoothStep(0.f,.15f,Reach);
        PigCarryPoseBlend=FMath::FInterpTo(PigCarryPoseBlend,Target,DeltaSeconds,18.f);
        PigFingerGripBlend=FMath::FInterpTo(PigFingerGripBlend,FMath::SmoothStep(.03f,.13f,Reach),DeltaSeconds,16.f);
        FirstPersonArms->AddTickPrerequisiteActor(HeldPig);
        if(USkeletalMeshComponent* Body=GetBraxtonBodyMesh())Body->AddTickPrerequisiteActor(HeldPig);
    }
    else
    {
        PigCarryPoseBlend=FMath::FInterpTo(PigCarryPoseBlend,0.f,DeltaSeconds,10.f);
        PigFingerGripBlend=FMath::FInterpTo(PigFingerGripBlend,0.f,DeltaSeconds,16.f);
        if(PigCarryPoseBlend<.002f)PigCarryPoseBlend=0.f;
        if(PigFingerGripBlend<.002f)PigFingerGripBlend=0.f;
    }
}
FVector AStoryFirstPersonCharacter::GetPigHandGripPoint() const
{
    if(!FirstPersonArms || !FirstPersonArms->GetSkeletalMeshAsset()
        || FirstPersonArms->GetBoneIndex(TEXT("hand_r"))==INDEX_NONE)return FVector::ZeroVector;
    const FTransform Mesh=FirstPersonArms->GetComponentTransform();
    const FTransform Hand=FirstPersonArms->GetSocketTransform(TEXT("hand_r")).GetRelativeTransform(Mesh);
    // The FK bone transform retains the FBX unit-conversion scale, while the
    // arm solver works in centimetres in component space. Match that exact
    // geometric construction here; scaling six centimetres by the imported
    // bone scale would measure a point several metres beyond the fingers.
    const FVector Contact=Hand.GetLocation()+Hand.TransformVectorNoScale(
        FSeniorPigHoldPose::GripOffset(FirstPersonArms->GetSkeletalMeshAsset()->GetRefSkeleton()));
    return Mesh.TransformPosition(Contact);
}
FVector AStoryFirstPersonCharacter::GetFurnitureHandGripPoint(bool Right) const
{
    if(!FirstPersonArms || !FirstPersonArms->GetSkeletalMeshAsset())return FVector::ZeroVector;
    const FName Bone=Right?TEXT("hand_r"):TEXT("hand_l");
    if(FirstPersonArms->GetBoneIndex(Bone)==INDEX_NONE)return FVector::ZeroVector;
    const FTransform Mesh=FirstPersonArms->GetComponentTransform();
    const FTransform Hand=FirstPersonArms->GetSocketTransform(Bone).GetRelativeTransform(Mesh);
    const FVector Contact=Hand.GetLocation()+Hand.TransformVectorNoScale(FSeniorFurnitureHoldPose::GripOffset(FirstPersonArms->GetSkeletalMeshAsset()->GetRefSkeleton(),Right));
    return Mesh.TransformPosition(Contact);
}
bool AStoryFirstPersonCharacter::CanReachPigGrip(const FVector& GripWorld,const FVector& LegAxisWorld) const
{
    if(!FirstPersonArms || !FirstPersonArms->GetSkeletalMeshAsset() || !FirstPersonCamera || !GetCapsuleComponent())return false;
    for(const TCHAR* Name:{TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r")})
        if(FirstPersonArms->GetBoneIndex(Name)==INDEX_NONE)return false;
    const USkeletalMesh* MeshAsset=FirstPersonArms->GetSkeletalMeshAsset();
    const FReferenceSkeleton& Ref=MeshAsset->GetRefSkeleton();
    const TArray<FTransform>& Components=FirstPersonArms->GetComponentSpaceTransforms();
    auto Bone=[&](const TCHAR* Name)
    {
        const int32 Index=Ref.FindBoneIndex(Name);
        // An invisible remote/dedicated FP mesh may never have evaluated an
        // animation. Its imported anatomical reference remains valid in cm.
        return Components.IsValidIndex(Index) && GetNetMode()!=NM_DedicatedServer
            ?Components[Index]:FSeniorDouliIdle::RefBone(Ref,Name);
    };
    const float ViewScale=GetCapsuleComponent()->GetComponentScale().Z;
    const float MaxBend=FMath::Clamp(GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()+BaseEyeHeight-45.f/ViewScale,0.f,115.f/ViewScale);
    // Predict the complete camera and mesh attachment, including view pitch.
    // This works before a remote camera has been updated and while a local
    // camera is already partway through its bend.
    FTransform BentCamera=FirstPersonCamera->GetComponentTransform();
    BentCamera.SetRotation(GetBaseAimRotation().Quaternion());
    FVector CameraLocal=FirstPersonCamera->GetRelativeLocation();
    CameraLocal.Z=BaseEyeHeight-MaxBend;
    BentCamera.SetLocation(GetCapsuleComponent()->GetComponentTransform().TransformPosition(CameraLocal));
    const FTransform BentArms=FirstPersonArms->GetRelativeTransform()*BentCamera;
    const FVector BentShoulder=BentArms.TransformPosition(Bone(TEXT("upperarm_r")).GetLocation());
    const FVector BentElbow=BentArms.TransformPosition(Bone(TEXT("lowerarm_r")).GetLocation());
    const FVector BentWrist=BentArms.TransformPosition(Bone(TEXT("hand_r")).GetLocation());
    // The imported FK locations are already in world centimetres. Bone scale
    // never participates in this length measurement. Require some elbow bend
    // at contact, rather than reaching at a locked elbow and then stretching.
    const float ArmReach=.98f*(FVector::Dist(BentShoulder,BentElbow)+FVector::Dist(BentElbow,BentWrist));
    const FVector LegAxis=LegAxisWorld.GetSafeNormal();
    FVector Fingers=FVector::VectorPlaneProject(BentCamera.GetRotation().GetAxisX(),LegAxis).GetSafeNormal();
    if(Fingers.IsNearlyZero())Fingers=FVector::VectorPlaneProject(BentCamera.GetRotation().GetAxisZ(),LegAxis).GetSafeNormal();
    if(Fingers.IsNearlyZero())Fingers=FVector::VectorPlaneProject(BentCamera.GetRotation().GetAxisY(),LegAxis).GetSafeNormal();
    const FQuat Palm=FRotationMatrix::MakeFromXY(Fingers,LegAxis).ToQuat();
    const FVector PalmOffset=Palm.RotateVector(FVector(6.f,0.f,-1.5f));
    const FVector DesiredWrist=GripWorld-PalmOffset;
    const float Distance=FVector::Dist(BentShoulder,DesiredWrist);
    const bool Reachable=Distance<=ArmReach;
#if !UE_BUILD_SHIPPING
    if(!Reachable && FParse::Param(FCommandLine::Get(),TEXT("HousePigMotionSmoke")))
    {
        static int32 FailedReachLogs=0;
        if(FailedReachLogs<96)
        {
            ++FailedReachLogs;
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_REACH_REJECT line=%d armReach=%.3f wristDistance=%.3f upperLength=%.3f lowerLength=%.3f bentShoulder=%s wristTarget=%s grip=%s legAxis=%s aim=%s playerFeet=%s bendMax=%.2f cameraLocal=%s meshScale=%s shoulderComponent=%s elbowComponent=%s handComponent=%s componentCount=%d carryPoseBlend=%.4f handScale=%s"),
                FailedReachLogs,ArmReach,Distance,FVector::Dist(BentShoulder,BentElbow),FVector::Dist(BentElbow,BentWrist),
                *BentShoulder.ToString(),*DesiredWrist.ToString(),*GripWorld.ToString(),*LegAxis.ToString(),*GetBaseAimRotation().ToString(),
                *(GetActorLocation()-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight())).ToString(),MaxBend,
                *FirstPersonCamera->GetRelativeLocation().ToString(),*BentArms.GetScale3D().ToString(),
                *Bone(TEXT("upperarm_r")).GetLocation().ToString(),*Bone(TEXT("lowerarm_r")).GetLocation().ToString(),*Bone(TEXT("hand_r")).GetLocation().ToString(),
                Components.Num(),PigCarryPoseBlend,*Bone(TEXT("hand_r")).GetScale3D().ToString());
        }
    }
#endif
    return Reachable;
}
void AStoryFirstPersonCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ClubPoleJourney.Pole)
    {
        if (HasAuthority()) ClubPoleJourney.Pole->AbortTraveller(this, TEXT("pawn ended play"));
        else ClubPoleJourney.Pole->RestoreTraveller(this);
    }
    if (Douli && HasAuthority()) Douli->Destroy();
    if (SaucePacket && HasAuthority()) SaucePacket->Destroy();
    if (BraxtonVisual) BraxtonVisual->Destroy();
    BraxtonVisual = nullptr;
    if (RunnerVisual) RunnerVisual->Destroy();
    RunnerVisual = nullptr;
    if (FixerVisual) FixerVisual->Destroy();
    FixerVisual = nullptr;
    Super::EndPlay(Reason);
}
void AStoryFirstPersonCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    ApplySelectedCharacter();
}
void AStoryFirstPersonCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    ApplySelectedCharacter();
}
void AStoryFirstPersonCharacter::NotifyControllerChanged()
{
    Super::NotifyControllerChanged();
    ApplySelectedCharacter();
    ConfigureLocalInput();
}
void AStoryFirstPersonCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();
    ApplySelectedCharacter();
    ConfigureLocalInput();
}
void AStoryFirstPersonCharacter::ApplySelectedCharacter()
{
    const auto* State = GetPlayerState<ASeniorLobbyPlayerState>();
    if (!State) return;
    const int32 Index = SeniorRoster::IsValidIndex(State->CharacterIndex) ? State->CharacterIndex : 0;
    if (AppliedCharacterIndex == Index) return;
    if (HouseMaskVisual) HouseMaskVisual->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    if (BraxtonVisual) { BraxtonVisual->Destroy(); BraxtonVisual = nullptr; }
    if (RunnerVisual) { RunnerVisual->Destroy(); RunnerVisual = nullptr; }
    if (FixerVisual) { FixerVisual->Destroy(); FixerVisual = nullptr; }
    GetMesh()->SetVisibility(true, false);
    GetMesh()->SetComponentTickEnabled(true);
    AppliedCharacterIndex = Index;
    CurrentBodyAnimation = nullptr;
    CurrentArmsAnimation = nullptr;
    BodyIdle = nullptr;
    BodyWalk = nullptr;
    bWalkingAnimation = false;

    if (USkeletalMesh* Body = SeniorRoster::Body(Index))
    {
        GetMesh()->SetSkeletalMesh(Body);
        ClothMotion.Bind(GetMesh(), GetActorRotation().Yaw);
        GetMesh()->SetRelativeRotation(FRotator::ZeroRotator);
        ApplyCharacterDimensions(Body);
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        BodyIdle = SeniorRoster::Idle(Index);
        BodyWalk = SeniorRoster::Walk(Index);
        UpdateBodyAnimation();
    }
    else
    {
        GetMesh()->SetSkeletalMesh(FallbackBody);
        GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
        GetMesh()->SetAnimInstanceClass(FallbackAnimClass);
        ApplyCharacterDimensions(FallbackBody);
    }

    USkeletalMesh* Arms = SeniorRoster::Arms(Index);
    FirstPersonArms->EmptyOverrideMaterials();
    FirstPersonArms->SetSkeletalMesh(Arms);
    FirstPersonArms->SetHiddenInGame(Arms == nullptr);
    if (Arms)
    {
        if (Index == 0 && !FParse::Param(FCommandLine::Get(), TEXT("BraxtonLegacyVisual")))
            if (UMaterialInterface* Hoodie = LoadObject<UMaterialInterface>(nullptr,
                TEXT("/Game/MetaHumans/BraxtonRebuild/MH_Braxton_Rebuild/Details/Hoodie/M_GrayHoodie")))
                for (int32 Slot = 0; Slot < Arms->GetMaterials().Num(); ++Slot)
                    if (Arms->GetMaterials()[Slot].MaterialSlotName == TEXT("Braxton_DetailedCloth"))
                        FirstPersonArms->SetMaterial(Slot, Hoodie);
        if (Index == 2 && !FParse::Param(FCommandLine::Get(), TEXT("FixerLegacyVisual")))
            if (UMaterialInterface* Skin = LoadObject<UMaterialInterface>(nullptr,
                TEXT("/Game/MetaHumans/Fixer/MH_Fixer/Details/MI_FixerFirstPersonSkin.MI_FixerFirstPersonSkin")))
                for (int32 Slot = 0; Slot < Arms->GetMaterials().Num(); ++Slot)
                    if (Arms->GetMaterials()[Slot].MaterialSlotName.ToString().Contains(TEXT("Skin")))
                        FirstPersonArms->SetMaterial(Slot, Skin);
        CurrentArmsAnimation = SeniorRoster::ArmsIdle(Index);
        FirstPersonArms->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        if (CurrentArmsAnimation) FirstPersonArms->PlayAnimation(CurrentArmsAnimation, true);
    }
    if (Index == 0 && GetNetMode() != NM_DedicatedServer
        && !FParse::Param(FCommandLine::Get(), TEXT("BraxtonLegacyVisual")))
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.ObjectFlags = RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ASeniorBraxtonVisual* NewVisual = GetWorld()->SpawnActor<ASeniorBraxtonVisual>(
            GetMesh()->GetComponentLocation(), GetMesh()->GetComponentRotation(), Params);
        if (NewVisual && NewVisual->InitializeVisual(false, true))
        {
            BraxtonVisual = NewVisual;
            NewVisual->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale);
            GetMesh()->SetVisibility(false, false);
            GetMesh()->SetComponentTickEnabled(false);
        }
        else if (NewVisual) NewVisual->Destroy();
    }
    if (Index == 1 && GetNetMode() != NM_DedicatedServer
        && !FParse::Param(FCommandLine::Get(), TEXT("RunnerLegacyVisual")))
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.ObjectFlags = RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ASeniorRunnerVisual* NewVisual = GetWorld()->SpawnActor<ASeniorRunnerVisual>(
            GetMesh()->GetComponentLocation(), GetMesh()->GetComponentRotation(), Params);
        if (NewVisual && NewVisual->InitializeVisual(false, true))
        {
            RunnerVisual = NewVisual;
            NewVisual->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale);
            GetMesh()->SetVisibility(false, false);
            GetMesh()->SetComponentTickEnabled(false);
        }
        else if (NewVisual) NewVisual->Destroy();
    }
    if (Index == 2 && GetNetMode() != NM_DedicatedServer
        && !FParse::Param(FCommandLine::Get(), TEXT("FixerLegacyVisual")))
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.ObjectFlags = RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ASeniorFixerVisual* NewVisual = GetWorld()->SpawnActor<ASeniorFixerVisual>(
            GetMesh()->GetComponentLocation(), GetMesh()->GetComponentRotation(), Params);
        if (NewVisual && NewVisual->InitializeVisual(false, true))
        {
            FixerVisual = NewVisual;
            NewVisual->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale);
            GetMesh()->SetVisibility(false, false);
            GetMesh()->SetComponentTickEnabled(false);
        }
        else if (NewVisual) NewVisual->Destroy();
    }
    UpdateHouseMaskVisual();
}
void AStoryFirstPersonCharacter::ApplyCharacterDimensions(USkeletalMesh* Body)
{
    if (!Body) return;
    const FBoxSphereBounds Bounds = Body->GetBounds();
    const bool bNativeFixer = AppliedCharacterIndex == 2 && !FParse::Param(FCommandLine::Get(), TEXT("FixerLegacyVisual"));
    const float Height = bNativeFixer ? ASeniorFixerVisual::HeightCm : FMath::Clamp(float(Bounds.BoxExtent.Z * 2.0), 140.0f, 220.0f);
    const float LowestPoint = bNativeFixer ? 0.f : float(Bounds.Origin.Z - Bounds.BoxExtent.Z);
    const float OldScaledHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float HalfHeight = Height * 0.5f;
    StandingVisualHalfHeight = HalfHeight;
    if (UStoryMovementComponent* Movement = GetStoryMovement())
        Movement->SetStandingCapsuleHalfHeight(HalfHeight);
    GetCapsuleComponent()->SetCapsuleHalfHeight(HalfHeight, false);
    // Apply one uniform scale to the pawn. The camera, arms, head attachments
    // and capsule inherit it, without baking a second scale into imported rigs.
    SetActorScale3D(FVector(GameplayScale));
    const float NewScaledHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    // Keep the feet in place when the server applies a different stature; clients receive that root position.
    if (HasAuthority() && !FMath::IsNearlyEqual(OldScaledHalfHeight, NewScaledHalfHeight))
    {
        AddActorWorldOffset(FVector(0, 0, NewScaledHalfHeight - OldScaledHalfHeight), false, nullptr, ETeleportType::TeleportPhysics);
    }
    GetMesh()->SetRelativeLocation(FVector(0, 0, -HalfHeight - LowestPoint));
    StandingMeshOffsetZ = GetMesh()->GetRelativeLocation().Z;
    // Network smoothing must preserve this character's new mesh-to-capsule offset.
    CacheInitialMeshOffset(GetMesh()->GetRelativeLocation(), GetMesh()->GetRelativeRotation());
    const float EyeHeightFromGround = Height * 0.935f;
    StandingEyeHeightFromGround = EyeHeightFromGround;
    BaseEyeHeight = EyeHeightFromGround - HalfHeight;
    FirstPersonCamera->SetRelativeLocation(FVector(0, 0, BaseEyeHeight));
    SlideCameraOffsetZ = 0;
    FTransform AdjustedArms = ArmsRelativeTransform;
    // FBX already contains each person's body scale. Only the attachment offset needs the same proportion.
    AdjustedArms.SetTranslation(ArmsRelativeTransform.GetTranslation() * (Height / 184.0f));
    FirstPersonArms->SetRelativeTransform(AdjustedArms);
}
void AStoryFirstPersonCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    // Engine crouch uses the class-default mesh offset. Roster members have
    // different measured heights, so retain each person's foot alignment.
    if (GetMesh() && GetCapsuleComponent())
    {
        FVector Offset = GetMesh()->GetRelativeLocation();
        Offset.Z = StandingMeshOffsetZ + StandingVisualHalfHeight
            - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
        GetMesh()->SetRelativeLocation(Offset);
        BaseTranslationOffset.Z = Offset.Z;
    }
}
void AStoryFirstPersonCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    if (GetMesh() && GetCapsuleComponent())
    {
        FVector Offset = GetMesh()->GetRelativeLocation();
        Offset.Z = StandingMeshOffsetZ + StandingVisualHalfHeight
            - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
        GetMesh()->SetRelativeLocation(Offset);
        BaseTranslationOffset.Z = Offset.Z;
        BaseEyeHeight = StandingEyeHeightFromGround
            - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    }
}
void AStoryFirstPersonCharacter::UpdateBodyAnimation()
{
    if (!BodyIdle && !BodyWalk) return;
    const float Speed = GetVelocity().Size2D();
    bWalkingAnimation = bWalkingAnimation ? Speed > 5.f : Speed > 12.f;
    UAnimSequence* Desired = bWalkingAnimation && BodyWalk ? BodyWalk.Get() : BodyIdle.Get();
    if (!Desired) Desired = BodyWalk;
    if (auto* Anim = Cast<USeniorDouliAnim>(GetMesh()->GetAnimInstance()))
    {
        Anim->BaseSequence = Desired;
        Anim->Speed = bWalkingAnimation ? Speed : 0.f;
        CurrentBodyAnimation = Desired;
        return;
    }
    if (Desired != CurrentBodyAnimation)
    {
        CurrentBodyAnimation = Desired;
        GetMesh()->PlayAnimation(Desired, true);
    }
}
void AStoryFirstPersonCharacter::ConfigureLocalInput()
{
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                if (InputContext) Subsystem->AddMappingContext(InputContext, 0);
}
void AStoryFirstPersonCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    ConfigureLocalInput();
    PlayerInputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AStoryFirstPersonCharacter::OnLeftMousePressed);
    PlayerInputComponent->BindKey(EKeys::LeftMouseButton,IE_Released,this,&AStoryFirstPersonCharacter::OnShootReleased);
    PlayerInputComponent->BindKey(EKeys::Gamepad_RightTrigger,IE_Pressed,this,&AStoryFirstPersonCharacter::ThrowEquippedWeapon);
    PlayerInputComponent->BindKey(EKeys::Gamepad_RightTrigger,IE_Released,this,&AStoryFirstPersonCharacter::OnShootReleased);
    PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AStoryFirstPersonCharacter::InteractWithExistingDoorOrFurniture);
    PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &AStoryFirstPersonCharacter::InteractWithExistingDoorOrFurniture);
    PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AStoryFirstPersonCharacter::ToggleHouseMask);
    PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Up, IE_Pressed, this, &AStoryFirstPersonCharacter::ToggleHouseMask);
    PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AStoryFirstPersonCharacter::ToggleHouseMaskCursor);
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AStoryFirstPersonCharacter::StartSprint);
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AStoryFirstPersonCharacter::StopSprint);
    PlayerInputComponent->BindKey(EKeys::Gamepad_LeftThumbstick, IE_Pressed, this, &AStoryFirstPersonCharacter::StartSprint);
    PlayerInputComponent->BindKey(EKeys::Gamepad_LeftThumbstick, IE_Released, this, &AStoryFirstPersonCharacter::StopSprint);
    PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Pressed, this, &AStoryFirstPersonCharacter::StartSlide);
    PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Released, this, &AStoryFirstPersonCharacter::StopSlide);
    PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &AStoryFirstPersonCharacter::ToggleCrawl);
    PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Down, IE_Pressed, this, &AStoryFirstPersonCharacter::ToggleCrawl);
    PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &AStoryFirstPersonCharacter::StartSlide);
    PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Released, this, &AStoryFirstPersonCharacter::StopSlide);
    bool bMouseMappedAndBound = false;
    if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (MoveAction) Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AStoryFirstPersonCharacter::Move);
        if (LookAction) Enhanced->BindAction(LookAction, ETriggerEvent::Triggered, this, &AStoryFirstPersonCharacter::Look);
        if (MouseLookAction && InputContext)
            for (const FEnhancedActionKeyMapping& Mapping : InputContext->GetMappings())
                if (Mapping.Action == MouseLookAction && Mapping.Key == EKeys::Mouse2D)
                {
                    Enhanced->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AStoryFirstPersonCharacter::LookMouse);
                    bMouseMappedAndBound = true;
                    break;
                }
        if (JumpAction)
        {
            Enhanced->BindAction(JumpAction, ETriggerEvent::Started, this, &AStoryFirstPersonCharacter::StartMovementJump);
            Enhanced->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
        }
    }
    // Keep a direct relative-axis path if an older copy of the mapping asset
    // is restored. Never bind it alongside Mouse2D, or every turn doubles.
    if (!bMouseMappedAndBound)
    {
        PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &AStoryFirstPersonCharacter::LookMouseX);
        PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &AStoryFirstPersonCharacter::LookMouseY);
        UE_LOG(LogTemp, Warning, TEXT("Story input: Mouse2D mapping absent; using MouseX/MouseY fallback."));
    }
}
AHouseOxbloodMaskPickup* AStoryFirstPersonCharacter::FocusedHouseMask() const
{
    if (!GetWorld() || bHasHouseMask || !FirstPersonCamera) return nullptr;
    const FVector View = FirstPersonCamera->GetComponentLocation();
    const FVector Facing = FirstPersonCamera->GetForwardVector();
    AHouseOxbloodMaskPickup* Best = nullptr;
    float BestScore = BIG_NUMBER;
    for (TActorIterator<AHouseOxbloodMaskPickup> It(GetWorld()); It; ++It)
    {
        const FVector To = It->InteractionLocation() - View;
        const float Distance = To.Size();
        if (Distance < 1.f || Distance > 270.f || FVector::DotProduct(Facing, To / Distance) < .66f) continue;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseMaskFocus), false);
        Query.AddIgnoredActor(this);
        Query.AddIgnoredActor(*It);
        FHitResult Obstruction;
        if (GetWorld()->LineTraceSingleByChannel(Obstruction, View, It->InteractionLocation(), ECC_Visibility, Query)) continue;
        const float AimOffset = FVector::CrossProduct(Facing, To).Size();
        const float Score = Distance + AimOffset * 3.f;
        if (Score < BestScore) { Best = *It; BestScore = Score; }
    }
    return Best;
}
AHousePokerTable* AStoryFirstPersonCharacter::FocusedHousePokerTable() const
{
    if (!GetWorld() || !FirstPersonCamera || !Controller) return nullptr;
    if (const APlayerController* PC = Cast<APlayerController>(Controller); PC && PC->bShowMouseCursor) return nullptr;
    if ((FurnitureInteraction && (FurnitureInteraction->bHoldingFurniture || FurnitureInteraction->FocusedFurnitureForInteraction()))
        || CarriedHousePig() || FocusedHousePigForInteraction() || FocusedHouseMask()) return nullptr;
    for (TActorIterator<AHousePokerTable> It(GetWorld()); It; ++It)
        if (It->IsFocusedBy(this)) return *It;
    return nullptr;
}
void AStoryFirstPersonCharacter::ServerDealHousePokerTable_Implementation(AHousePokerTable* Table)
{
    // The client supplies only a candidate. Aim, range, obstruction and the
    // existing held-furniture/mask priorities are checked on the server again.
    if (IsValid(Table) && FocusedHousePokerTable() == Table) Table->DealHand(this);
}
void AStoryFirstPersonCharacter::ReceiveHouseMask()
{
    if (!HasAuthority() || bHasHouseMask) return;
    bHasHouseMask = true;
    bHouseMaskEquipped = true;
    OnRep_HouseMask();
    ForceNetUpdate();
    ClientHouseMaskNotice(TEXT("Mask equipped: +25% damage. Press Tab, then click its corner icon to remove it."));
}
void AStoryFirstPersonCharacter::OnRep_HouseMask()
{
    UpdateHouseMaskVisual();
}
void AStoryFirstPersonCharacter::UpdateHouseMaskVisual()
{
    if (!HouseMaskVisual) return;
    const bool bVisible = bHouseMaskEquipped && GetNetMode() != NM_DedicatedServer;
    HouseMaskVisual->SetVisibility(bVisible);
    if (!bVisible) return;

    AActor* Native = BraxtonVisual ? BraxtonVisual->GetNativeCharacter() :
        RunnerVisual ? RunnerVisual->GetNativeCharacter() : FixerVisual ? FixerVisual->GetNativeCharacter() : nullptr;
    USkeletalMeshComponent* Face = nullptr;
    if (Native)
        for (USkeletalMeshComponent* Mesh : TInlineComponentArray<USkeletalMeshComponent*>(Native))
            if (Mesh && Mesh->GetFName() == TEXT("Face")) { Face = Mesh; break; }
    if (!Face) Face = BraxtonVisual ? BraxtonVisual->GetBodyMesh() :
        RunnerVisual ? RunnerVisual->GetBodyMesh() : FixerVisual ? FixerVisual->GetBodyMesh() : GetMesh();
    if (!Face || !Face->GetSkeletalMeshAsset() || !Face->DoesSocketExist(TEXT("head")))
    { HouseMaskVisual->SetVisibility(false); return; }
    // Fit only when the character changes. Skeletal attachment updates after
    // the head animation, so the mask follows looking, walking and turning.
    if (HouseMaskVisual->GetAttachParent() == Face && HouseMaskVisual->GetAttachSocketName() == TEXT("head")) return;
    const FReferenceSkeleton& Skeleton = Face->GetSkeletalMeshAsset()->GetRefSkeleton();
    const auto ReferenceBone = [&Skeleton](FName Name)
    {
        int32 Index = Skeleton.FindBoneIndex(Name);
        FTransform Transform = FTransform::Identity;
        while (Index != INDEX_NONE)
        {
            Transform *= Skeleton.GetRefBonePose()[Index];
            Index = Skeleton.GetParentIndex(Index);
        }
        return Transform;
    };
    const FTransform Head = ReferenceBone(TEXT("head"));
    const bool bForwardY = Native || Face->GetSkeletalMeshAsset() == FallbackBody;
    const FQuat FitRotation = bForwardY ? FQuat::Identity : FRotator(0, -90, 0).Quaternion();
    const FVector Forward = FitRotation.RotateVector(FVector(0, 1, 0));
    FVector Eyes = Head.GetTranslation() + Forward * 6.f + FVector(0, 0, 12);
    float Scale = .62f;
    if (!bForwardY && AppliedCharacterIndex >= 3 && AppliedCharacterIndex <= 7)
    {
        // The five Cobble faces have no eye bones. These measurements come
        // from their source iris centres and scale with the imported skeleton.
        const float SourceHeadZ[] = {159.9777f,160.0450f,159.9288f,160.0297f,160.0856f};
        const float EyeRise[] = {3.91275f,3.84393f,3.96454f,3.84943f,3.79112f};
        const float EyeSpan[] = {6.05599f,6.21710f,5.99844f,6.21710f,6.05599f};
        const int32 FitIndex = AppliedCharacterIndex - 3;
        const float Stature = float(Head.GetTranslation().Z) / SourceHeadZ[FitIndex];
        Eyes = Head.GetTranslation() + Forward * (10.01316f * Stature) + FVector(0, 0, EyeRise[FitIndex] * Stature);
        Scale = EyeSpan[FitIndex] * Stature / 11.6f * 1.08f;
    }
    if (Skeleton.FindBoneIndex(TEXT("FACIAL_L_Eye")) != INDEX_NONE &&
        Skeleton.FindBoneIndex(TEXT("FACIAL_R_Eye")) != INDEX_NONE)
    {
        const FVector Left = ReferenceBone(TEXT("FACIAL_L_Eye")).GetTranslation();
        const FVector Right = ReferenceBone(TEXT("FACIAL_R_Eye")).GetTranslation();
        Eyes = (Left + Right) * .5f;
        Scale = FMath::Clamp(float(FVector::Distance(Left, Right) / 11.6 * 1.08), .55f, .75f);
    }
    // Native faces and this mesh are both +Y forward. Align the open eye
    // apertures to the eyes, with leather slightly ahead of the eyelids.
    const FVector Origin = Eyes + Forward * 2.2f - FitRotation.RotateVector(FVector(0, 8, .8f) * Scale);
    const FTransform Fit(FitRotation, Origin, FVector(Scale));
    HouseMaskVisual->AttachToComponent(Face, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("head"));
    HouseMaskVisual->SetRelativeTransform(Fit.GetRelativeTransform(Head));
    HouseMaskVisual->SetCastShadow(true);
    HouseMaskVisual->SetCastHiddenShadow(true);
}
void AStoryFirstPersonCharacter::ToggleHouseMask()
{
    if (!IsLocallyControlled() || !bHasHouseMask) return;
    if (ASeniorLobbyController* PC=Cast<ASeniorLobbyController>(Controller); PC && PC->IsPauseMenuOpen()) return;
    ServerToggleHouseMask();
}
void AStoryFirstPersonCharacter::ToggleHouseMaskCursor()
{
    if (!IsLocallyControlled() || !bHasHouseMask) return;
    if (ASeniorLobbyController* PC=Cast<ASeniorLobbyController>(Controller)) PC->ToggleEquipmentCursor();
}
void AStoryFirstPersonCharacter::ServerToggleHouseMask_Implementation()
{
    if (!bHasHouseMask) return;
    bHouseMaskEquipped = !bHouseMaskEquipped;
    OnRep_HouseMask();
    ForceNetUpdate();
    ClientHouseMaskNotice(bHouseMaskEquipped ? TEXT("Oxblood mask equipped: +25% damage.") :
        TEXT("Oxblood mask removed: normal damage."));
}
void AStoryFirstPersonCharacter::ServerCollectHouseMask_Implementation(AHouseOxbloodMaskPickup* Pickup)
{
    if (IsValid(Pickup) && Pickup->CanCollect(this)) Pickup->Collect(this);
}
void AStoryFirstPersonCharacter::ClientHouseMaskNotice_Implementation(const FString& Message)
{
    if (UStoryCampaign* Story=GetGameInstance<UStoryCampaign>()) Story->Notify(Message);
}
AExistingHouseDoor* AStoryFirstPersonCharacter::FindFocusedExistingDoor(float MinimumFacing) const
{
    if (!GetWorld()) return nullptr;
    const FVector ViewLocation = FirstPersonCamera ? FirstPersonCamera->GetComponentLocation() : GetActorLocation();
    const FVector ViewForward = FirstPersonCamera ? FirstPersonCamera->GetForwardVector() : GetActorForwardVector();
    AExistingHouseDoor* BestDoor = nullptr;
    float BestScore = BIG_NUMBER;
    for (TActorIterator<AExistingHouseDoor> It(GetWorld()); It; ++It)
    {
        const FVector ToDoor = It->DoorwayCenter - ViewLocation;
        const float Distance = ToDoor.Size();
        if (Distance > 270.f || Distance < 1.f) continue;
        const float Facing = FVector::DotProduct(ViewForward, ToDoor / Distance);
        if (Facing < MinimumFacing) continue;
        if (MinimumFacing > .7f)
        {
            const FVector FlatForward = FVector(ViewForward.X, ViewForward.Y, 0.f).GetSafeNormal();
            const float SideDistance = FMath::Abs(FVector::CrossProduct(FlatForward, ToDoor).Z);
            if (SideDistance > (It->DoorGroup.IsNone() ? 78.f : 145.f)) continue;
        }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ExistingHouseDoorFocus), false);
        Query.AddIgnoredActor(this);
        if (AHouseRoamingPig* Pig=CarriedHousePig()) Query.AddIgnoredActor(Pig);
        if (Douli) Query.AddIgnoredActor(Douli.Get());
        if (SaucePacket) Query.AddIgnoredActor(SaucePacket.Get());
        FHitResult Obstruction;
        if (GetWorld()->LineTraceSingleByChannel(Obstruction, ViewLocation, It->DoorwayCenter,
                ECC_Visibility, Query))
        {
            const AExistingHouseDoor* HitDoor = Cast<AExistingHouseDoor>(Obstruction.GetActor());
            if (!HitDoor && Cast<AHouseReactiveFixture>(Obstruction.GetActor()))
                HitDoor = Cast<AExistingHouseDoor>(Obstruction.GetActor()->GetOwner());
            if (!HitDoor || (HitDoor != *It
                && (It->DoorGroup.IsNone() || HitDoor->DoorGroup != It->DoorGroup)))
                continue;
        }
        // Favor the door under the reticle. Adjacent room doors can be
        // closer to the player than the one they are looking straight at.
        const float AimOffset = FVector::CrossProduct(ViewForward, ToDoor).Size();
        const float Score = Distance + AimOffset * 3.f;
        if (Score < BestScore) { BestDoor = *It; BestScore = Score; }
    }
    return BestDoor;
}
AExistingHouseDoor* AStoryFirstPersonCharacter::FocusedDoorForInteraction() const
{
    return FindFocusedExistingDoor(CarriedHousePig()?.97f:.45f);
}
AHouseRoamingPig* AStoryFirstPersonCharacter::CarriedHousePig() const
{
    if (!GetWorld()) return nullptr;
    for (TActorIterator<AHouseRoamingPig> It(GetWorld());It;++It) if (It->Carrier==this) return *It;
    return nullptr;
}
AHouseRoamingPig* AStoryFirstPersonCharacter::FocusedHousePigForInteraction() const
{
    if (!GetWorld() || !Controller || CarriedHousePig()
        || (FurnitureInteraction && FurnitureInteraction->bHoldingFurniture)) return nullptr;
    if (const APlayerController* PC=Cast<APlayerController>(Controller); PC && PC->bShowMouseCursor) return nullptr;
    FVector View; FRotator Rotation; Controller->GetPlayerViewPoint(View,Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HousePigFocus),false,this);
    if (Douli) Query.AddIgnoredActor(Douli.Get());
    if (SaucePacket) Query.AddIgnoredActor(SaucePacket.Get());
    FHitResult Hit;
    GetWorld()->LineTraceSingleByChannel(Hit,View,View+Rotation.Vector()*220.f,ECC_Visibility,Query);
    if (AHouseRoamingPig* Pig=Cast<AHouseRoamingPig>(Hit.GetActor())) return Pig->CanPickup(const_cast<AStoryFirstPersonCharacter*>(this))?Pig:nullptr;
    // Small aim assistance makes the smaller, quicker pig usable without ever
    // selecting through a wall or an intervening furniture object.
    if (Hit.bBlockingHit) return nullptr;
    if (GetWorld()->SweepSingleByChannel(Hit,View,View+Rotation.Vector()*220.f,FQuat::Identity,ECC_Visibility,
        FCollisionShape::MakeSphere(16.f),Query))
        if (AHouseRoamingPig* Pig=Cast<AHouseRoamingPig>(Hit.GetActor()))
            return Pig->CanPickup(const_cast<AStoryFirstPersonCharacter*>(this))?Pig:nullptr;
    return nullptr;
}
void AStoryFirstPersonCharacter::ServerInteractHousePig_Implementation(AHouseRoamingPig* Pig)
{
    if (!IsValid(Pig)) return;
    if (Pig==CarriedHousePig())
    {
        if(Pig->IsPlacing() || Pig->GetCarryBlend()<.995f)return;
        if (!Pig->TryPlace(this)) ClientHouseMaskNotice(TEXT("Find a clear, level floor to put the pig down."));
        return;
    }
    if (!(GetStoryMovement() && GetStoryMovement()->IsCrawling())
        && !CarriedHousePig() && !(FurnitureInteraction && FurnitureInteraction->bHoldingFurniture)
        && FocusedHousePigForInteraction()==Pig) Pig->TryPickup(this);
}
bool AStoryFirstPersonCharacter::InteractWithExistingDoor(AExistingHouseDoor* Door)
{
    if (!Door) return false;
    if (Door->bLocked)
    {
        if (UStoryCampaign* Story = GetGameInstance<UStoryCampaign>())
        {
            // A campus door says why it is locked; every house door keeps its message.
            const ACampusDoor* Campus = Cast<ACampusDoor>(Door);
            Story->Notify(Campus && !Campus->LockedReason.IsEmpty() ? FString::Printf(TEXT("Locked: %s."), *Campus->LockedReason) : FString(TEXT("Locked: this area is not playable yet.")));
        }
        return true;
    }
    if (HasAuthority())
    {
        const bool bWasOpen = Door->IsOpen();
        if (!Door->Interact(this) && bWasOpen) ClientNotifyDoorwayOccupied();
    }
    else ServerInteractExistingDoor(Door);
    return true;
}
void AStoryFirstPersonCharacter::OnLeftMousePressed()
{
    if (!IsLocallyControlled()) return;
    if (GetStoryMovement() && GetStoryMovement()->IsCrawling()) return;
    // A HUD click must never throw a weapon or double-click a door behind it.
    if (APlayerController* PC=Cast<APlayerController>(Controller); PC && PC->bShowMouseCursor) return;
    // A held chair takes precedence over a door double-click and equipment.
    if (FurnitureInteraction && FurnitureInteraction->bHoldingFurniture)
    {
        LastClickedDoor.Reset();
        LastDoorClickTime = -1000.0;
        FurnitureInteraction->ThrowHeldFurniture();
        return;
    }
    // Deliberately aiming at scenery throws at that piece, even near a door.
    FCollisionQueryParams SceneryQuery(SCENE_QUERY_STAT(HouseWeaponTarget), false);
    SceneryQuery.AddIgnoredActor(this);
    if (Douli) SceneryQuery.AddIgnoredActor(Douli.Get());
    if (SaucePacket) SceneryQuery.AddIgnoredActor(SaucePacket.Get());
    const FVector ViewStart = FirstPersonCamera->GetComponentLocation();
    FHitResult SceneryHit;
    if (GetWorld()->LineTraceSingleByChannel(SceneryHit, ViewStart,
        ViewStart + GetBaseAimRotation().Vector()*5000.f, ECC_Visibility, SceneryQuery)
        && SceneryHit.GetActor() && SceneryHit.GetActor()->ActorHasTag(TEXT("HouseBreakable")))
    {
        LastClickedDoor.Reset();
        LastDoorClickTime = -1000.0;
        ThrowEquippedWeapon();
        return;
    }
    if (AExistingHouseDoor* Door = FindFocusedExistingDoor(.78f); Door && !Door->bLocked)
    {
        const double Now = GetWorld()->GetRealTimeSeconds();
        const bool bSameDoor = LastClickedDoor.Get() == Door ||
            (LastClickedDoor.IsValid() && !Door->DoorGroup.IsNone() && LastClickedDoor->DoorGroup == Door->DoorGroup);
        if (bSameDoor && Now - LastDoorClickTime <= .36)
        {
            InteractWithExistingDoor(Door);
            LastClickedDoor.Reset();
            LastDoorClickTime = -1000.0;
        }
        else
        {
            LastClickedDoor = Door;
            LastDoorClickTime = Now;
        }
        return;
    }
    LastClickedDoor.Reset();
    LastDoorClickTime = -1000.0;
    ThrowEquippedWeapon();
}
void AStoryFirstPersonCharacter::OnShootReleased()
{
    if (!IsLocallyControlled() || !FurnitureInteraction) return;
    if (const auto* PC = Cast<APlayerController>(Controller); PC && PC->bShowMouseCursor)
        FurnitureInteraction->CancelBasketballShot();
    else FurnitureInteraction->ReleaseBasketballShot();
}
AHouseReactiveFixture* AStoryFirstPersonCharacter::FocusedHouseFixture() const
{
    if (!GetWorld() || !Controller || !FirstPersonCamera) return nullptr;
    if (FurnitureInteraction && FurnitureInteraction->bHoldingFurniture) return nullptr;
    FCollisionQueryParams Query;
    Query.AddIgnoredActor(this);
    const FVector Start = FirstPersonCamera->GetComponentLocation();
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start,
        Start + GetBaseAimRotation().Vector()*300.f, ECC_Visibility, Query)) return nullptr;
    AHouseReactiveFixture* Fixture = Cast<AHouseReactiveFixture>(Hit.GetActor());
    return Fixture && !Fixture->IsBroken() ? Fixture : nullptr;
}
AHouseGadget* AStoryFirstPersonCharacter::FocusedHouseGadget() const
{
    if (!GetWorld() || !Controller || !FirstPersonCamera) return nullptr;
    if (FurnitureInteraction && FurnitureInteraction->bHoldingFurniture) return nullptr;
    if (CarriedHousePig()) return nullptr;
    FCollisionQueryParams Query;
    Query.AddIgnoredActor(this);
    const FVector Start = FirstPersonCamera->GetComponentLocation();
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start,
        Start + GetBaseAimRotation().Vector()*260.f, ECC_Visibility, Query)) return nullptr;
    AHouseGadget* Gadget = Cast<AHouseGadget>(Hit.GetActor());
    return Gadget && Gadget->IsInteractive() ? Gadget : nullptr;
}
void AStoryFirstPersonCharacter::ServerInteractHouseGadget_Implementation(AHouseGadget* Gadget)
{
    if (IsValid(Gadget) && FocusedHouseGadget()==Gadget)
        Gadget->Interact(this);
}
void AStoryFirstPersonCharacter::ServerInteractHouseFixture_Implementation(AHouseReactiveFixture* Fixture)
{
    if (IsValid(Fixture) && !CarriedHousePig() && FocusedHouseFixture()==Fixture)
        Fixture->Interact(this);
}
AHouseClubPole* AStoryFirstPersonCharacter::FocusedClubPole(bool& bUpper) const
{
    bUpper=false;
    if (!GetWorld() || ClubPoleJourney.Pole) return nullptr;
    for (TActorIterator<AHouseClubPole> It(GetWorld()); It; ++It)
        if (It->FindFocusedEnd(this,bUpper)) return *It;
    return nullptr;
}
void AStoryFirstPersonCharacter::ServerUseClubPole_Implementation(AHouseClubPole* Pole, bool bUpper)
{
    if (IsValid(Pole)) Pole->TryUse(this,bUpper);
}
void AStoryFirstPersonCharacter::ServerClubPoleSmokeReady_Implementation(AHouseClubPole* Pole)
{
    // The pole validates the opt-in flag, actual remote pawn and approach.
    // The RPC still comes through this pawn's normal owning connection.
    if (IsValid(Pole)) Pole->MarkMultiplayerSmokeClientReady(this);
}
void AStoryFirstPersonCharacter::ClientClubPoleNotice_Implementation(const FString& Message)
{
    if (UStoryCampaign* Story=GetGameInstance<UStoryCampaign>()) Story->Notify(Message);
}
void AStoryFirstPersonCharacter::SetClubPoleJourney(const FHouseClubPoleJourney& Journey)
{
    if (!HasAuthority()) return;
    const FHouseClubPoleJourney Previous=ClubPoleJourney;
    ClubPoleJourney=Journey;
    OnRep_ClubPoleJourney(Previous);
    ForceNetUpdate();
}
void AStoryFirstPersonCharacter::OnRep_ClubPoleJourney(const FHouseClubPoleJourney& Previous)
{
    if (Previous.Pole && (Previous.Pole!=ClubPoleJourney.Pole || Previous.Serial!=ClubPoleJourney.Serial))
        Previous.Pole->FinishReplicatedTraveller(this,Previous,ClubPoleJourney);
    if (ClubPoleJourney.Pole) ClubPoleJourney.Pole->ApplyJourney(this,ClubPoleJourney);
}
void AStoryFirstPersonCharacter::InteractWithExistingDoorOrFurniture()
{
    if (!IsLocallyControlled()) return;
    if (const APlayerController* PC=Cast<APlayerController>(Controller); PC && PC->bShowMouseCursor) return;
    if (ClubPoleJourney.Pole) return;
    bool bPoleUpper=false;
    if (AHouseClubPole* Pole=FocusedClubPole(bPoleUpper))
    { ServerUseClubPole(Pole,bPoleUpper); return; }
    FName VentPortal;
    if (AHouseVentNetwork* Vent = FocusedVentNetwork(VentPortal))
    {
        if (HasAuthority()) Vent->Interact(this, VentPortal);
        else ServerInteractVent(VentPortal);
        return;
    }
    if (GetStoryMovement() && GetStoryMovement()->IsCrawling())
    {
        InteractWithExistingDoor(FindFocusedExistingDoor(.45f));
        return;
    }
    if (AHouseRoamingPig* Pig=CarriedHousePig())
    {
        // A deliberate aim at a door still opens it while carrying a pig;
        // looking down at the floor uses the same E key to place the animal.
        if (InteractWithExistingDoor(FindFocusedExistingDoor(.97f))) return;
        ServerInteractHousePig(Pig); return;
    }
    if (FurnitureInteraction && FurnitureInteraction->bHoldingFurniture)
    {
        FurnitureInteraction->ToggleGrab();
        return;
    }
    if (AHouseOxbloodMaskPickup* Pickup = FocusedHouseMask())
    { ServerCollectHouseMask(Pickup); return; }
    if (AHouseGadget* Gadget = FocusedHouseGadget())
    { ServerInteractHouseGadget(Gadget); return; }
    if (AHouseReactiveFixture* Fixture = FocusedHouseFixture();
        Fixture && Fixture->FixtureKind != EHouseReactiveFixtureKind::Window)
    { ServerInteractHouseFixture(Fixture); return; }
    // Boywick under the reticle takes the E press before any furnishing or door.
    if (ABoywickCharacter* Boywick = FocusedBoywick())
    { ServerInteractBoywick(Boywick); return; }
    // A furnishing under the reticle wins over a nearby door's wider aim cone.
    if (FurnitureInteraction && FurnitureInteraction->FocusedFurnitureForInteraction())
    { FurnitureInteraction->ToggleGrab(); return; }
    if (AHouseRoamingPig* Pig=FocusedHousePigForInteraction())
    { ServerInteractHousePig(Pig); return; }
    if (AHousePokerTable* Table = FocusedHousePokerTable())
    { ServerDealHousePokerTable(Table); return; }
    if (InteractWithExistingDoor(FindFocusedExistingDoor(.45f))) return;
    if (FurnitureInteraction) FurnitureInteraction->ToggleGrab();
}
AHouseVentNetwork* AStoryFirstPersonCharacter::FocusedVentNetwork(FName& OutPortalId) const
{
    OutPortalId = NAME_None;
    if (!GetWorld()) return nullptr;
    for (TActorIterator<AHouseVentNetwork> It(GetWorld()); It; ++It)
    {
        OutPortalId = It->FindFocusedPortal(this);
        if (!OutPortalId.IsNone()) return *It;
    }
    return nullptr;
}
void AStoryFirstPersonCharacter::ServerInteractVent_Implementation(FName PortalId)
{
    if (PortalId.IsNone()) return;
    FName FocusedPortal;
    AHouseVentNetwork* Vent = FocusedVentNetwork(FocusedPortal);
    if (Vent && FocusedPortal == PortalId) Vent->Interact(this, PortalId);
}
void AStoryFirstPersonCharacter::ServerInteractExistingDoor_Implementation(AExistingHouseDoor* Door)
{
    if (!IsValid(Door)) return;
    // The client supplies only a candidate. Re-check its aim and unobstructed
    // view on the server before applying the authoritative door state.
    AExistingHouseDoor* Focused = FocusedDoorForInteraction();
    if (Focused != Door && (!Focused || Door->DoorGroup.IsNone()
        || Focused->DoorGroup != Door->DoorGroup)) return;
    const bool bWasOpen = Door->IsOpen();
    if (!Door->Interact(this) && bWasOpen) ClientNotifyDoorwayOccupied();
}
void AStoryFirstPersonCharacter::ClientNotifyDoorwayOccupied_Implementation()
{
    if (UStoryCampaign* Story = GetGameInstance<UStoryCampaign>())
        Story->Notify(TEXT("Step clear of the doorway to close it."));
}
UStoryMovementComponent* AStoryFirstPersonCharacter::GetStoryMovement() const
{
    return Cast<UStoryMovementComponent>(GetCharacterMovement());
}
bool AStoryFirstPersonCharacter::CanStartCrawl() const
{
    return !(FurnitureInteraction && FurnitureInteraction->bHoldingFurniture)
        && !CarriedHousePig();
}
bool AStoryFirstPersonCharacter::SetVentCrawlForced(bool bForced)
{
    if (!HasAuthority()) return false;
    if (bForced && !CanStartCrawl()) return false;
    UStoryMovementComponent* Movement = GetStoryMovement();
    if (!Movement) return false;
    if (bForced && !Movement->SetCrawlForced(true))
    {
        Movement->SetCrawlForced(false);
        return false;
    }
    if (!bForced) Movement->SetCrawlForced(false);
    bVentCrawlForced = bForced;
    ForceNetUpdate();
    return true;
}
void AStoryFirstPersonCharacter::OnRep_VentCrawlForced()
{
    if (UStoryMovementComponent* Movement = GetStoryMovement())
        Movement->SetCrawlForced(bVentCrawlForced);
}
void AStoryFirstPersonCharacter::OnRep_ReplicatedCrawl()
{
    if (UStoryMovementComponent* Movement = GetStoryMovement())
        Movement->SetSimulatedCrawl(bReplicatedCrawl);
}
void AStoryFirstPersonCharacter::StartSprint()
{
    if (ClubPoleJourney.Pole) return;
    if (auto* Movement = GetStoryMovement()) Movement->SetSprintHeld(true);
}
void AStoryFirstPersonCharacter::StopSprint()
{
    if (auto* Movement = GetStoryMovement()) Movement->SetSprintHeld(false);
}
void AStoryFirstPersonCharacter::StartSlide()
{
    if (ClubPoleJourney.Pole) return;
    if (auto* Movement = GetStoryMovement()) Movement->SetSlideHeld(true);
}
void AStoryFirstPersonCharacter::StopSlide()
{
    if (auto* Movement = GetStoryMovement()) Movement->SetSlideHeld(false);
}
void AStoryFirstPersonCharacter::ToggleCrawl()
{
    if (ClubPoleJourney.Pole) return;
    if (UStoryMovementComponent* Movement = GetStoryMovement())
    {
        const bool bNewRequest = !Movement->WantsToCrawl();
        if (bNewRequest && !CanStartCrawl())
        {
            if (UStoryCampaign* Story = GetGameInstance<UStoryCampaign>())
                Story->Notify(TEXT("Set down what you're carrying before crawling."));
            return;
        }
        Movement->SetCrawlRequested(bNewRequest);
    }
}
void AStoryFirstPersonCharacter::StartMovementJump()
{
    if (ClubPoleJourney.Pole) return;
    // Releasing crouch here lets the same input jump out of a slide. The move's
    // crouch and jump flags travel together to the server for prediction.
    if (auto* Movement = GetStoryMovement()) Movement->SetSlideHeld(false);
    Jump();
}
bool AStoryFirstPersonCharacter::CanJumpInternal_Implementation() const
{
    // Character checks jump input before its movement component applies the
    // requested uncrouch. Permit that one transition without allowing a jump
    // while the player is still holding the slide/crouch key.
    return !(GetStoryMovement() && GetStoryMovement()->IsCrawling())
        && (!IsCrouched() || !GetCharacterMovement()->bWantsToCrouch) && JumpIsAllowedInternal();
}
void AStoryFirstPersonCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AStoryFirstPersonCharacter,Douli);
    DOREPLIFETIME(AStoryFirstPersonCharacter,SaucePacket);
    DOREPLIFETIME(AStoryFirstPersonCharacter,bHasHouseMask);
    DOREPLIFETIME(AStoryFirstPersonCharacter,bHouseMaskEquipped);
    DOREPLIFETIME(AStoryFirstPersonCharacter,bVentCrawlForced);
    DOREPLIFETIME(AStoryFirstPersonCharacter,BoywickBlessing);
    DOREPLIFETIME(AStoryFirstPersonCharacter,BoywickBlessingUntil);
    DOREPLIFETIME(AStoryFirstPersonCharacter,ClubPoleJourney);
    DOREPLIFETIME_CONDITION(AStoryFirstPersonCharacter,bReplicatedCrawl,COND_SkipOwner);
}
void AStoryFirstPersonCharacter::ThrowEquippedWeapon()
{
    if (GetStoryMovement() && GetStoryMovement()->IsCrawling()) return;
    if(APlayerController* PC=Cast<APlayerController>(Controller);PC && PC->bShowMouseCursor)return;
    if(CarriedHousePig() || PigCarryPoseBlend>.02f)return;
    // The gamepad right trigger uses this same input action.
    if (FurnitureInteraction && FurnitureInteraction->bHoldingFurniture)
    {
        FurnitureInteraction->ThrowHeldFurniture();
        return;
    }
    ServerThrowEquippedWeapon();
}
void AStoryFirstPersonCharacter::ServerThrowEquippedWeapon_Implementation()
{
    if (GetStoryMovement() && GetStoryMovement()->IsCrawling()) return;
    if(CarriedHousePig())return;
    // Resolve a delayed client hold replication on the authoritative side too.
    if (FurnitureInteraction && FurnitureInteraction->bHoldingFurniture)
    {
        FurnitureInteraction->ThrowHeldFurniture();
        return;
    }
    const auto* State=GetPlayerState<ASeniorLobbyPlayerState>();
    if(!State || State->CharacterIndex!=0)return;
    if(State->LoadoutIndex==0 && Douli)Douli->BeginThrow();
    else if(State->LoadoutIndex==1 && SaucePacket)SaucePacket->BeginThrow();
}
void AStoryFirstPersonCharacter::ThrowDouli()
{
    if (GetStoryMovement() && GetStoryMovement()->IsCrawling()) return;
    // UI clicks must not also launch a weapon behind the menu.
    if (APlayerController* PC=Cast<APlayerController>(Controller); PC && PC->bShowMouseCursor) return;
    if(CarriedHousePig() || PigCarryPoseBlend>.02f)return;
    ServerThrowDouli();
}
void AStoryFirstPersonCharacter::ServerThrowDouli_Implementation()
{
    if (GetStoryMovement() && GetStoryMovement()->IsCrawling()) return;
    if(CarriedHousePig())return;
    if (Douli) Douli->BeginThrow();
}
FTransform AStoryFirstPersonCharacter::DouliGrip(bool bFirstPerson) const
{
    if (bFirstPerson && FirstPersonCamera)
    {
        const FTransform Camera=FirstPersonCamera->GetComponentTransform();
        // The first-person hat is smaller on screen, so its rim is about 13 cm
        // from the fingers. Follow the animated hand instead of a separate
        // camera-space motion curve that leaves the fingers in empty space.
        const FVector Hand=FirstPersonArms && FirstPersonArms->GetSkeletalMeshAsset()
            ? FirstPersonArms->GetSocketLocation(TEXT("hand_r"))
            : Camera.TransformPosition(FVector(50,35,-31));
        const FVector Center=Hand+Camera.GetUnitAxis(EAxis::X)*4.f
            -Camera.GetUnitAxis(EAxis::Y)*13.f+Camera.GetUnitAxis(EAxis::Z)*5.f;
        return FTransform(Camera.GetRotation()*FRotator(-6,0,0).Quaternion(),Center,FVector(.65f));
    }
    if (BraxtonVisual && BraxtonVisual->GetBodyMesh())
    {
        const auto* B=BraxtonVisual->GetBodyMesh();
        if (B->GetSkeletalMeshAsset())
        {
            const FQuat LocalRotation=FSeniorDouliIdle::HandFrame(
                B->GetSkeletalMeshAsset()->GetRefSkeleton())
                *FSeniorDouliIdle::PalmFrame().Inverse();
            const FVector LocalCenter=LocalRotation.RotateVector(FSeniorDouliIdle::HatOffset());
            const FTransform Hand=B->GetSocketTransform(TEXT("hand_r"));
            return FTransform(Hand.GetRotation()*LocalRotation,
                Hand.TransformPosition(LocalCenter),FVector::OneVector);
        }
    }
    return FTransform(GetActorRotation(),GetActorLocation()+GetActorForwardVector()*35+GetActorRightVector()*8+FVector(0,0,15));
}
USkeletalMeshComponent* AStoryFirstPersonCharacter::GetBraxtonBodyMesh() const
{
    return BraxtonVisual ? BraxtonVisual->GetBodyMesh() : nullptr;
}
FTransform AStoryFirstPersonCharacter::SauceGrip(bool bFirstPerson) const
{
    if(bFirstPerson && FirstPersonCamera)
        return FTransform(FirstPersonCamera->GetComponentQuat()*FRotator(0,180,0).Quaternion(),
            FirstPersonCamera->GetComponentTransform().TransformPosition(FVector(43,19,-14)));
    if(BraxtonVisual && BraxtonVisual->GetBodyMesh())
    {
        const FVector Hand=BraxtonVisual->GetBodyMesh()->GetSocketLocation(TEXT("hand_r"));
        return FTransform(GetActorRotation(),Hand-GetActorRightVector()*5+FVector(0,0,-5));
    }
    return FTransform(GetActorRotation(),GetActorLocation()+GetActorForwardVector()*35+GetActorRightVector()*10+FVector(0,0,12));
}
void AStoryFirstPersonCharacter::Move(const FInputActionValue& Value)
{
    // Use the actor's yaw-aligned basis. The camera can pitch independently, and
    // movement should never inherit that pitch or drift away from the view.
    const FVector2D Input = Value.Get<FVector2D>().GetClampedToMaxSize(1.f);
    if (Controller && !Input.IsNearlyZero(0.05f))
    {
        AddMovementInput(GetActorForwardVector(), Input.Y);
        AddMovementInput(GetActorRightVector(), Input.X);
    }
}
void AStoryFirstPersonCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();
    AddControllerYawInput(Input.X);
    AddControllerPitchInput(Input.Y * (SeniorPlayerPreferences::Get().bInvertY ? -1.f : 1.f));
}
void AStoryFirstPersonCharacter::LookMouse(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();
    const auto& Settings = SeniorPlayerPreferences::Get();
    // The controller applies its legacy 2.5x look scale after this call.
    // Bound a single frame's delta so a focus/capture hitch cannot whip the camera around.
    constexpr float MaxMouseLookPerFrame = 4.f;
    AddControllerYawInput(FMath::Clamp(Input.X * Settings.MouseSensitivity, -MaxMouseLookPerFrame, MaxMouseLookPerFrame));
    // This project's raw mouse-up delta and the controller's pitch input
    // scale disagree; reverse the mouse once here, then apply the user's
    // optional inversion. Gamepad look keeps its separate stick convention.
    AddControllerPitchInput(FMath::Clamp(-Input.Y * Settings.MouseSensitivity * (Settings.bInvertY ? -1.f : 1.f),
        -MaxMouseLookPerFrame, MaxMouseLookPerFrame));
}
void AStoryFirstPersonCharacter::LookMouseX(float AxisValue)
{
    AddControllerYawInput(FMath::Clamp(AxisValue * SeniorPlayerPreferences::Get().MouseSensitivity, -4.f, 4.f));
}
void AStoryFirstPersonCharacter::LookMouseY(float AxisValue)
{
    const auto& Settings = SeniorPlayerPreferences::Get();
    AddControllerPitchInput(FMath::Clamp(-AxisValue * Settings.MouseSensitivity * (Settings.bInvertY ? -1.f : 1.f), -4.f, 4.f));
}
void AStoryGameMode::InitGame(const FString& MapName, const FString& Options, FString& Error)
{
    Super::InitGame(MapName, Options, Error);
    if (MapName.Contains(TEXT("LoadingScreen")) || MapName.Contains(TEXT("Lobby"))) DefaultPawnClass = nullptr;
}
void AStoryGameMode::PreLogin(const FString& Options, const FString& Address,
    const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (ErrorMessage.IsEmpty()) ErrorMessage = TEXT("Story in progress. Ask the host to return to the lobby to join.");
}
void AStoryGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Story = GetGameInstance<UStoryCampaign>())
    {
        Story->bTravelPending = false;
        if (UGameplayStatics::GetCurrentLevelName(this, true) == TEXT("LoadingScreen")) Story->ResumeStory();
        else if (Story->CurrentChapter() == 0) return;
        else
        {
            if (AStoryGameState* State = GetGameState<AStoryGameState>(); State && Story->Progress)
            {
                State->StoryChapter = Story->Progress->Chapter;
                State->StoryCheckpoint = Story->Progress->Checkpoint;
                State->StoryDifficulty = Story->Progress->Difficulty;
                State->bStoryCompleted = Story->Progress->bCompleted;
            }
            TArray<AActor*> Starts;
            UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), Starts);
            if (Starts.Num()) FallLimit = Starts[0]->GetActorLocation().Z - 1200;
            // Masks are deliberately left on existing furniture. These are
            // visible surface centres, rather than the taller collision proxies.
            if (HasAuthority() && Story->CurrentChapter() == 1)
            {
                if (UGameplayStatics::GetCurrentLevelName(this, true) == TEXT("Chapter01_House"))
                {
                    TActorIterator<AHouseVentNetwork> ExistingVent(GetWorld());
                    if (!ExistingVent)
                    {
                        FActorSpawnParameters VentParams;
                        VentParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                        GetWorld()->SpawnActor<AHouseVentNetwork>(AHouseVentNetwork::StaticClass(),
                            HouseWorldScale::Transform(GetWorld(), FTransform::Identity), VentParams);
                    }
                }
                struct FMaskPlacement { FVector Surface; float Yaw; };
                const FMaskPlacement Places[] = {
                    { FVector(873, -230, 56), 18.f },       // Sofa end table
                    { FVector(-865, -245, 56), -24.f },     // Far side table
                    { FVector(-100, 802, 76), 0.f },        // Console beside trophies
                    { FVector(-2690, -213, 349.75f), 0.f }, // Upstairs leather ottoman
                };
                for (const FMaskPlacement& Place : Places)
                {
                    FActorSpawnParameters Params;
                    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                    GetWorld()->SpawnActor<AHouseOxbloodMaskPickup>(AHouseOxbloodMaskPickup::StaticClass(),
                        HouseWorldScale::Transform(GetWorld(), FTransform(FRotator(0, Place.Yaw, 0), Place.Surface)), Params);
                }
            }
            // The permanent objective HUD already explains the route; leave
            // the temporary notice area free for progress and errors.
        }
    }
}
void AStoryGameMode::RestartPlayer(AController* Player)
{
    if (!Player) return;
    auto* Story = GetGameInstance<UStoryCampaign>();
    if (Story && Story->CurrentChapter() != 1 && Story->Progress && Story->Progress->bHasCheckpoint && Story->CurrentChapter() == Story->Progress->Chapter)
    {
        FTransform Spawn = Story->Progress->Spawn;
        if (GameState)
        {
            const int32 PartyIndex = GameState->PlayerArray.IndexOfByKey(Player->PlayerState);
            const float SideOffset = PartyIndex == 1 ? 120.f : (PartyIndex == 2 ? -120.f : 0.f);
            Spawn.AddToTranslation(Spawn.GetRotation().GetRightVector() * SideOffset);
        }
        RestartPlayerAtTransform(Player, Spawn);
    }
    else Super::RestartPlayer(Player);
}
void AStoryGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if WITH_EDITOR
    TickStorySmokeTest(GetWorld());
#endif
#if !UE_BUILD_SHIPPING
    TickCombinedHouseSmoke(GetWorld());
    TickHouseVisualReview(GetWorld());
    TickHouseMaskReview(GetWorld());
    TickExistingHouseDoorSmoke(GetWorld());
    TickRoom101EntrySmoke(GetWorld());
    TickNightClubSmoke(GetWorld());
    TickNightClubEntranceSmoke(GetWorld());
    TickHouseTraversalSmoke(GetWorld());
    TickHouseChairSmoke(GetWorld());
    TickHousePokerSmoke(GetWorld());
    TickHousePigSmoke(GetWorld());
    TickHouseImpossibleHallwaySmoke(GetWorld());
    TickHouseHotTubWaterSmoke(GetWorld());
    TickHouseInteractivitySmoke(GetWorld());
    TickHouseGlassSmoke(GetWorld());
    TickHouseLibrarySmoke(GetWorld());
    TickHouseKitchenSmoke(GetWorld());
    TickHouseTelevisionSmoke(GetWorld());
    TickHouseVentSmoke(GetWorld());
#endif
    auto* Story = GetGameInstance<UStoryCampaign>();
    if (!Story || Story->CurrentChapter() == 0 || Story->bTravelPending || Story->Progress->bCompleted) return;
    TActorIterator<AHouseVentNetwork> VentIt(GetWorld());
    AHouseVentNetwork* Vent = VentIt ? *VentIt : nullptr;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (APlayerController* PC = It->Get(); PC && PC->HasClientLoadedCurrentWorld())
            if (APawn* Pawn = PC->GetPawn(); !Pawn ||
                (Pawn->GetActorLocation().Z < FallLimit && !(Vent && Vent->IsInDuct(Pawn))))
                Story->RespawnPlayerAtCheckpoint(PC);
}

AStoryCheckpoint::AStoryCheckpoint()
{
    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
    SetRootComponent(Trigger);
    Trigger->SetBoxExtent(FVector(115, 115, 160));
    Trigger->SetCollisionProfileName(TEXT("Trigger"));
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Trigger->OnComponentBeginOverlap.AddDynamic(this, &AStoryCheckpoint::Enter);
    Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
    Marker->SetupAttachment(Trigger);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Marker->SetStaticMesh(Mesh.Object);
    Marker->SetRelativeLocation(FVector(0, 0, -92));
    Marker->SetRelativeScale3D(FVector(2.2, 2.2, 0.08));
    Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(Trigger);
    Label->SetRelativeLocation(FVector(0, 0, 150));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(45);
    RespawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RespawnPoint"));
    RespawnPoint->SetupAttachment(Trigger);
    RespawnPoint->SetRelativeLocation(FVector(-200, 0, 10));
}
void AStoryCheckpoint::BeginPlay()
{
    Super::BeginPlay();
    Label->SetText(FText::FromString(bChapterExit ? (Chapter == 3 ? TEXT("FINISH STORY") : TEXT("NEXT CHAPTER")) : FString::Printf(TEXT("CHECKPOINT %d"), CheckpointIndex)));
    // The chapter-one porch exit is a real doorway. Hide its placeholder
    // checkered cylinder and oversized floating text without changing its trigger.
    if (bChapterExit && Chapter == 1)
    {
        Marker->SetVisibility(false);
        Label->SetVisibility(false);
    }
}
void AStoryCheckpoint::Enter(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!HasAuthority()) return;
    APawn* Pawn = Cast<APawn>(Other);
    if (!Pawn || !Pawn->IsPlayerControlled()) return;
    auto* Story = GetGameInstance<UStoryCampaign>();
    if (!Story || Story->bTravelPending || Story->CurrentChapter() != Chapter) return;
    if (bChapterExit)
    {
        if (Story->Progress->Checkpoint < RequiredCheckpoint) Story->Notify(TEXT("Reach both checkpoints before leaving this chapter."));
        else Story->CompleteChapter();
    }
    else Story->ReachCheckpoint(Chapter, CheckpointIndex, RespawnPoint->GetComponentTransform());
}

AStoryHUD::AStoryHUD()
{
    static ConstructorHelpers::FObjectFinder<UTexture2D> Icon(
        TEXT("/Game/Items/HouseMask/T_HouseMaskIcon.T_HouseMaskIcon"));
    HouseMaskIcon = Icon.Object;
}

void AStoryHUD::DrawHUD()
{
    Super::DrawHUD();
    auto* Story = GetGameInstance<UStoryCampaign>();
    if (!Canvas || !Story || !Story->Progress) return;
    const AStoryGameState* Team = GetWorld()->GetGameState<AStoryGameState>();
    const int32 Checkpoint = Team ? Team->StoryCheckpoint : Story->Progress->Checkpoint;
    const int32 Difficulty = Team ? Team->StoryDifficulty : Story->Progress->Difficulty;
    const bool bCompleted = Team ? Team->bStoryCompleted : Story->Progress->bCompleted;
    const float W = Canvas->SizeX, H = Canvas->SizeY;
    auto* ArmedPawn=PlayerOwner ? Cast<AStoryFirstPersonCharacter>(PlayerOwner->GetPawn()) : nullptr;
    const float S = FMath::Clamp(W / 1400.f, 0.65f, 1.25f);
    const FLinearColor Ink(0.025f, 0.035f, 0.07f, .92f), Gold(1.f, .78f, .35f, 1.f);
    auto Text = [this, S](const FString& T, float X, float Y, FLinearColor C, float Scale = 1.f) {
        DrawText(T, C, X, Y, GEngine->GetMediumFont(), S * Scale);
    };
    if (Story->bTravelPending || Story->CurrentChapter() == 0)
    {
        DrawRect(Ink, 0, 0, W, H);
        Text(TEXT("SENIOR SENDOFF"), W * .3f, H * .4f, Gold, 2);
        Text(TEXT("Loading your story..."), W * .3f, H * .52f, FLinearColor::White);
        return;
    }
    DrawRect(Ink, 18*S, 18*S, 540*S, Story->CurrentChapter()==1 ? 140*S : 112*S);
    Text(TEXT("SENIOR SENDOFF"), 34*S, 30*S, Gold, 1.25f);
    if (Story->CurrentChapter() == 1)
    {
        Text(FString::Printf(TEXT("HOUSE    |    %s"), *UStoryCampaign::DifficultyName(Difficulty)),
            34*S, 66*S, FLinearColor::White);
        Text(TEXT("Explore the house."), 34*S, 98*S, FLinearColor(.7f,.8f,.9f));
    }
    else
    {
        Text(FString::Printf(TEXT("CHAPTER %d / 3    |    CHECKPOINT %d / 2    |    %s"), Story->CurrentChapter(),
            Checkpoint, *UStoryCampaign::DifficultyName(Difficulty)), 34*S, 66*S, FLinearColor::White);
        Text(TEXT("Reach both markers, then the chapter exit."), 34*S, 98*S, FLinearColor(.7f,.8f,.9f));
    }
    if (Story->CurrentChapter() == 1)
    {
        if (const UHouseDamageSubsystem* Damage = GetWorld()->GetSubsystem<UHouseDamageSubsystem>())
            Text(FString::Printf(TEXT("HOUSE DAMAGE  |  %d BROKEN"), Damage->GetTotalBrokenCount()),
                34*S, 126*S, Gold, .9f);
        DrawRect(Ink, 18*S, H-97*S, 548*S, 28*S);
        Text(ArmedPawn && ArmedPawn->CarriedHousePig()
            ? TEXT("Carry pig  |  E place on clear floor  |  Aim at door + E open / close")
            : ArmedPawn && ArmedPawn->FurnitureInteraction && ArmedPawn->FurnitureInteraction->bHoldingFurniture
                && ArmedPawn->FurnitureInteraction->GetHeldBody() && Cast<AHouseBasketball>(ArmedPawn->FurnitureInteraction->GetHeldBody()->GetOwner())
            ? TEXT("Basketball  |  Hold LMB / right trigger, release in green  |  E set down")
            : ArmedPawn && ArmedPawn->FurnitureInteraction && ArmedPawn->FurnitureInteraction->GetHeldBody()
                && Cast<AHouseWoodAxe>(ArmedPawn->FurnitureInteraction->GetHeldBody()->GetOwner())
            ? TEXT("Chopping axe  |  LMB / right trigger swing  |  E set down / return to stump")
            : ArmedPawn && ArmedPawn->FurnitureInteraction && ArmedPawn->FurnitureInteraction->bHoldingFurniture
            ? (ArmedPawn->FurnitureInteraction->GetCarrySize()==ESSOCarrySize::Large?TEXT("Dragging  |  E lower and let go  |  LMB / right trigger shove")
                : ArmedPawn->FurnitureInteraction->GetCarrySize()==ESSOCarrySize::Medium?TEXT("Two-hand carry  |  E set down  |  LMB / right trigger toss")
                : TEXT("One-hand carry  |  E set down  |  LMB / right trigger throw"))
            : TEXT("Shift sprint  |  Ctrl slide  |  C crawl  |  Space jump  |  E interact  |  Double-click doors"),
            29*S, H-93*S, FLinearColor(.8f,.85f,.9f), .8f);
        if (ArmedPawn && ArmedPawn->FurnitureInteraction && PlayerOwner && !PlayerOwner->bShowMouseCursor)
        {
            const bool bHolding = ArmedPawn->FurnitureInteraction->bHoldingFurniture;
            // This same focused trace takes priority over nearby doors for E.
            AActor* Focused = !bHolding ? ArmedPawn->FurnitureInteraction->FocusedFurnitureForInteraction() : nullptr;
            const bool bFocused = Focused != nullptr;
            if (bHolding || bFocused)
            {
                const ESSOCarrySize HeldSize = ArmedPawn->FurnitureInteraction->GetCarrySize();
                FString Prompt = bHolding ? (HeldSize==ESSOCarrySize::Large?TEXT("E LET GO  |  LMB SHOVE")
                    : HeldSize==ESSOCarrySize::Medium?TEXT("E SET DOWN  |  LMB TOSS"):TEXT("E SET DOWN  |  LMB THROW")) : TEXT("E PICK UP FURNITURE");
                if (const AHouseInteractiveProp* Prop = Cast<AHouseInteractiveProp>(Focused))
                {
                    const ESSOCarrySize FocusedSize = UHouseFurnitureInteractionComponent::ClassifyFurniture(Prop->Body, Prop);
                    Prompt = Prop->IsClothFlag() ? TEXT("E PULL DOWN")
                        : Prop->bWallMounted && !Prop->IsDetached() ? TEXT("E PULL FROM WALL")
                        : FocusedSize==ESSOCarrySize::Large ? TEXT("E LIFT ONE SIDE AND DRAG")
                        : FocusedSize==ESSOCarrySize::Medium ? TEXT("E PICK UP (TWO HANDS)") : TEXT("E PICK UP");
                    if (Prop->DamageStage == 1) Prompt += TEXT("  |  CRACKED");
                    else if (Prop->DamageStage == 2) Prompt += TEXT("  |  DAMAGED");
                    else if (Prop->bBroken) Prompt += TEXT("  |  BROKEN");
                    const FString Label = Prop->PropName.ToUpper();
                    if (!Label.IsEmpty() && Label.Len() <= 26 && Label != TEXT("FURNITURE") && Label != TEXT("LOOSE FURNISHING") && !Label.Contains(TEXT("_")))
                        Prompt += TEXT("  |  ") + Label;
                }
                const AHouseBasketball* Ball = Cast<AHouseBasketball>(bHolding && ArmedPawn->FurnitureInteraction->GetHeldBody()
                    ? ArmedPawn->FurnitureInteraction->GetHeldBody()->GetOwner() : Focused);
                if (Ball) Prompt = bHolding ? TEXT("AIM AT RIM  |  HOLD & RELEASE LMB / RT") : TEXT("E PICK UP BASKETBALL");
                if (auto* Axe = Cast<AHouseWoodAxe>(bHolding && ArmedPawn->FurnitureInteraction->GetHeldBody()
                    ? ArmedPawn->FurnitureInteraction->GetHeldBody()->GetOwner() : Focused))
                    Prompt = bHolding ? (Axe->CanReturnToStump(ArmedPawn) ? TEXT("E RETURN TO STUMP  |  LMB / RT SWING") : TEXT("E SET DOWN  |  LMB / RT SWING"))
                        : Axe->bEmbedded ? TEXT("E PULL AXE FROM STUMP") : TEXT("E PICK UP AXE");
                float PromptWidth = bHolding ? 280*S : 350*S;
                { float TW = 0, TH = 0; GetTextSize(Prompt, TW, TH, GEngine->GetMediumFont(), S * .82f); PromptWidth = FMath::Max(PromptWidth, TW + 26*S); }
                const float PromptX = (W - PromptWidth) * .5f;
                DrawRect(Ink, PromptX, H*.5f + 30*S, PromptWidth, 28*S);
                Text(Prompt,
                    PromptX + 12*S, H*.5f + 34*S, Gold, .82f);
            }
        }
    }
    if (ArmedPawn && ArmedPawn->FurnitureInteraction && PlayerOwner && !PlayerOwner->bShowMouseCursor)
    {
        const auto* Mover = ArmedPawn->FurnitureInteraction.Get();
        const auto* Ball = Mover->GetHeldBody() ? Cast<AHouseBasketball>(Mover->GetHeldBody()->GetOwner()) : nullptr;
        const float MeterWidth = 270 * S, X = (W - MeterWidth) * .5f, Y = H * .5f + 82 * S;
        if (Ball)
        {
            const float Ideal = Mover->GetBasketballIdealPower(), Power = Mover->GetBasketballPower();
            const FLinearColor Green(.24f, .95f, .50f), Orange(1.f, .60f, .25f);
            DrawRect(Ink, X - 12 * S, Y - 10 * S, MeterWidth + 24 * S, 69 * S);
            Text(Mover->IsBasketballCharging() ? TEXT("RELEASE IN GREEN") : TEXT("HOLD TO CHARGE"), X, Y - 4 * S, FLinearColor::White, .78f);
            DrawRect(FLinearColor(.12f, .15f, .18f), X, Y + 21 * S, MeterWidth, 11 * S);
            if (Ideal >= 0)
            {
                const float Lo = FMath::Max(0.f, Ideal - AHouseBasketball::GreenHalfWidth);
                const float Hi = FMath::Min(1.f, Ideal + AHouseBasketball::GreenHalfWidth);
                DrawRect(Green, X + MeterWidth * Lo, Y + 21 * S, MeterWidth * (Hi - Lo), 11 * S);
            }
            if (Mover->IsBasketballCharging())
                DrawRect(Ideal >= 0 && FMath::Abs(Power - Ideal) <= AHouseBasketball::GreenHalfWidth ? Green : Orange,
                    X + MeterWidth * Power - 2 * S, Y + 17 * S, 4 * S, 19 * S);
            Text(Ideal >= 0 ? TEXT("EARLY = SHORT     LATE = LONG") : TEXT("AIM AT A HOOP TO SEE THE TARGET BAND"), X, Y + 40 * S, Gold, .65f);
        }
        else if (!Mover->GetBasketballFeedback().IsEmpty())
            Text(Mover->GetBasketballFeedback(), X + 48 * S, Y, Mover->GetBasketballFeedback().Contains(TEXT("GREEN"))
                ? FLinearColor(.24f, .95f, .50f) : Gold, 1.f);
    }
    if (Story->CurrentChapter() == 1 && PlayerOwner)
        for (TActorIterator<AHouseRoamingPig> It(GetWorld()); It; ++It)
        {
            FVector LabelWorld; FVector2D Screen;
            if (!It->ShouldShowHealthTo(PlayerOwner,LabelWorld)
                || !PlayerOwner->ProjectWorldLocationToScreen(LabelWorld,Screen,true)
                || Screen.X < 10*S || Screen.X > W-10*S || Screen.Y < 15*S || Screen.Y > H-15*S) continue;
            const float Width=118*S, X=Screen.X-Width*.5f, Y=Screen.Y;
            const bool ShowMood=It->IsCarried()||It->Mood==EHousePigMood::Sleeping;
            DrawRect(Ink,X-5*S,Y-4*S,Width+10*S,(ShowMood?53.f:40.f)*S);
            Text(FString::Printf(TEXT("%s  %d / %d"),*It->PigName,FMath::CeilToInt(It->Health),FMath::RoundToInt(It->MaxHealth)),
                X+3*S,Y,FLinearColor::White,.72f);
            DrawRect(FLinearColor(.12f,.14f,.13f,1),X,Y+23*S,Width,7*S);
            const float Fraction=FMath::Clamp(It->Health/FMath::Max(1.f,It->MaxHealth),0.f,1.f);
            DrawRect(Fraction>.35f?FLinearColor(.42f,.72f,.39f,1):FLinearColor(.9f,.32f,.2f,1),X,Y+23*S,Width*Fraction,7*S);
            if (ShowMood) Text(It->IsCarried()?TEXT("Carried"):TEXT("Sleeping"),X+3*S,Y+33*S,Gold,.62f);
        }
    BoywickDrawOverlay(this, ArmedPawn, PlayerOwner, S, W, H, Ink, Gold);
    if (ArmedPawn && ArmedPawn->bHasHouseMask)
    {
        const float MaskX = W - 100*S, MaskY = 72*S, MaskSize = 76*S;
        const FLinearColor MaskTint = ArmedPawn->bHouseMaskEquipped ? FLinearColor::White :
            FLinearColor(.48f, .5f, .54f, 1.f);
        DrawRect(Ink, MaskX, MaskY, MaskSize, 88*S);
        if (HouseMaskIcon)
            DrawTexture(HouseMaskIcon, MaskX+5*S, MaskY+3*S, 66*S, 66*S,
                0, 0, 1, 1, MaskTint);
        Text(ArmedPawn->bHouseMaskEquipped ? TEXT("ON +25%") : TEXT("OFF"),
            MaskX+8*S, MaskY+68*S, ArmedPawn->bHouseMaskEquipped ? Gold : FLinearColor::White, .66f);
#if !(PLATFORM_IOS || PLATFORM_ANDROID)
        Text(TEXT("TAB: CLICK MASK"), W-179*S, MaskY+92*S,
            FLinearColor(.8f,.85f,.9f), .62f);
#endif
    }
    if (ArmedPawn && !ArmedPawn->bHasHouseMask)
    {
        if (ArmedPawn->FocusedHouseMask())
        {
            DrawRect(Ink, W*.5f-124*S, H*.5f+25*S, 248*S, 32*S);
            Text(TEXT("E  PICK UP OXBLOOD MASK"), W*.5f-112*S, H*.5f+29*S, Gold, .85f);
        }
    }
    if (ArmedPawn && PlayerOwner && !PlayerOwner->bShowMouseCursor && !ArmedPawn->CarriedHousePig())
        if (AHouseGadget* Gadget = ArmedPawn->FocusedHouseGadget())
        {
            const FString GadgetLabel = Gadget->FocusLabel();
            float GW = 0.f, GH = 0.f;
            GetTextSize(GadgetLabel, GW, GH, GEngine->GetMediumFont(), S * .85f);
            DrawRect(Ink, W*.5f-GW*.5f-14*S, H*.5f+25*S, GW+28*S, 32*S);
            Text(GadgetLabel, W*.5f-GW*.5f, H*.5f+29*S, Gold, .85f);
        }
    if (ArmedPawn && PlayerOwner && !PlayerOwner->bShowMouseCursor && !ArmedPawn->CarriedHousePig())
        if (AHouseReactiveFixture* Fixture = ArmedPawn->FocusedHouseFixture())
        {
            DrawRect(Ink, W*.5f-135*S, H*.5f+25*S, 270*S, 32*S);
            Text(Fixture->FocusLabel(), W*.5f-122*S, H*.5f+29*S, Gold, .85f);
        }
    if (ArmedPawn && !ArmedPawn->FocusedHouseFixture() && !ArmedPawn->FocusedHouseGadget()
        && !(ArmedPawn->FurnitureInteraction && (ArmedPawn->FurnitureInteraction->bHoldingFurniture
            || ArmedPawn->FurnitureInteraction->FocusedFurnitureForInteraction()))
        && !ArmedPawn->FocusedHousePokerTable() && !ArmedPawn->FocusedHousePigForInteraction() && !ArmedPawn->FocusedBoywick()
        && !ArmedPawn->ClubPoleJourney.Pole)
    {
        if (AExistingHouseDoor* Door = ArmedPawn->FocusedDoorForInteraction())
        {
            const FString Prompt = Door->bLocked ? TEXT("LOCKED")
                : Door->IsOpen() ? TEXT("E  CLOSE DOOR") : TEXT("E  OPEN DOOR");
            DrawRect(Ink, W*.5f-101*S, H*.5f+25*S, 202*S, 32*S);
            Text(Prompt, W*.5f-86*S, H*.5f+29*S, Door->bLocked ? FLinearColor::White : Gold, .9f);
        }
    }
    if (ArmedPawn && PlayerOwner && !PlayerOwner->bShowMouseCursor)
    {
        AHouseRoamingPig* HeldPig=ArmedPawn->CarriedHousePig();
        AHouseRoamingPig* FocusedPig=ArmedPawn->FocusedHousePigForInteraction();
        if (FocusedPig || (HeldPig && !ArmedPawn->FocusedDoorForInteraction()))
        {
            const FString Prompt=HeldPig
                ? HeldPig->IsPlacing()?FString::Printf(TEXT("PLACING %s..."),*HeldPig->PigName.ToUpper())
                : HeldPig->GetCarryBlend()<.995f?FString::Printf(TEXT("LIFTING %s..."),*HeldPig->PigName.ToUpper())
                : FString::Printf(TEXT("E  PLACE %s ON FLOOR"),*HeldPig->PigName.ToUpper())
                : FString::Printf(TEXT("E  PICK UP %s"),*FocusedPig->PigName.ToUpper());
            DrawRect(Ink,W*.5f-142*S,H*.5f+25*S,284*S,32*S);
            Text(Prompt,W*.5f-129*S,H*.5f+29*S,Gold,.85f);
        }
    }
    if (ArmedPawn)
        if (AHousePokerTable* Table = ArmedPawn->FocusedHousePokerTable())
        {
            const float PromptX = W*.5f-150*S;
            const bool bShowHand = Table->HasHand() && !Table->IsDealing();
            DrawRect(Ink, PromptX, H*.5f+25*S, 300*S, (bShowHand ? 62.f : 32.f)*S);
            Text(Table->InteractionPrompt(), PromptX+14*S, H*.5f+29*S, Gold, .9f);
            if (bShowHand) Text(Table->HandName(), PromptX+14*S, H*.5f+57*S, FLinearColor::White, .88f);
        }
    if (ArmedPawn && PlayerOwner && !PlayerOwner->bShowMouseCursor)
    {
        FName PortalId;
        if (AHouseVentNetwork* Vent = ArmedPawn->FocusedVentNetwork(PortalId))
        {
            const FString Prompt = Vent->IsInDuct(ArmedPawn)
                ? FString::Printf(TEXT("E  EXIT TO %s"), *Vent->PortalDisplayName(PortalId).ToUpper())
                : ArmedPawn->CanStartCrawl() ? TEXT("E  ENTER VENT")
                : TEXT("SET DOWN ITEM TO ENTER VENT");
            DrawRect(Ink, W*.5f-185*S, H*.5f+25*S, 370*S, 32*S);
            Text(Prompt, W*.5f-172*S, H*.5f+29*S, Gold, .82f);
        }
    }
    if (ArmedPawn && PlayerOwner && !PlayerOwner->bShowMouseCursor)
    {
        bool bUpper=false;
        AHouseClubPole* Pole=ArmedPawn->FocusedClubPole(bUpper);
        const FString Prompt=ArmedPawn->ClubPoleJourney.Pole
            ? ArmedPawn->ClubPoleJourney.bClimbing ? TEXT("CLIMBING UP...") : TEXT("SLIDING DOWN...")
            : Pole ? Pole->FocusLabel(ArmedPawn) : FString();
        if (!Prompt.IsEmpty())
        {
            float PW=0,PH=0; GetTextSize(Prompt,PW,PH,GEngine->GetMediumFont(),S*.85f);
            DrawRect(Ink,W*.5f-PW*.5f-14*S,H*.5f+25*S,PW+28*S,32*S);
            Text(Prompt,W*.5f-PW*.5f,H*.5f+29*S,Gold,.85f);
        }
    }
    if (ArmedPawn && (ArmedPawn->Douli || ArmedPawn->SaucePacket))
    {
        DrawLine(W*.5f-5,H*.5f,W*.5f+5,H*.5f,FLinearColor::White);
        DrawLine(W*.5f,H*.5f-5,W*.5f,H*.5f+5,FLinearColor::White);
        if(!ArmedPawn->CarriedHousePig())
        {
        DrawRect(Ink,18*S,H-60*S,340*S,40*S);
        Text(ArmedPawn->SaucePacket?ArmedPawn->SaucePacket->StatusText():ArmedPawn->Douli->StatusText(),30*S,H-49*S,Gold,.85f);
#if PLATFORM_IOS || PLATFORM_ANDROID
        DrawRect(Ink,W-150*S,H-100*S,130*S,65*S);
        Text(TEXT("THROW"),W-125*S,H-79*S,Gold);
#endif
        }
    }
    if (GetWorld()->GetTimeSeconds() < Story->StatusUntil)
    {
        DrawRect(Ink, 18*S, 142*S, 700*S, 48*S);
        Text(Story->Status, 34*S, 154*S, FLinearColor::White);
    }
    if (bCompleted)
    {
        DrawRect(Ink, W*.15f, H*.32f, W*.7f, H*.3f);
        Text(TEXT("STORY COMPLETE"), W*.22f, H*.38f, Gold, 2);
        Text(TEXT("All three chapters finished. Thanks for playing!"), W*.22f, H*.49f, FLinearColor::White);
    }
    const float BX = W - 245*S, BW = 225*S, BH = 42*S;
    DrawRect(Ink, BX, 18*S, BW, BH);
    Text(TEXT("Pause / settings [Esc]"), BX+10*S, 28*S, FLinearColor::White);
    float X=0, Y=0; bool Pressed=false;
    if (PlayerOwner)
    {
        bool bThrowTouch=false;
        for(int32 Finger=0;Finger<int32(ETouchIndex::MAX_TOUCHES);++Finger)
        {
            float TX=0,TY=0;bool Down=false;
            PlayerOwner->GetInputTouchState(ETouchIndex::Type(Finger),TX,TY,Down);
            bThrowTouch|=Down && TX>=W-150*S && TX<=W-20*S && TY>=H-100*S && TY<=H-35*S;
        }
        if(bThrowTouch && !bThrowTouchPressed && ArmedPawn && (ArmedPawn->Douli || ArmedPawn->SaucePacket)) ArmedPawn->ServerThrowEquippedWeapon();
        bThrowTouchPressed=bThrowTouch;
        PlayerOwner->GetInputTouchState(ETouchIndex::Touch1, X, Y, Pressed);
        const bool bMaskTouch = Pressed;
        if (!Pressed) { PlayerOwner->GetMousePosition(X,Y); Pressed = PlayerOwner->IsInputKeyDown(EKeys::LeftMouseButton); }
        if (Pressed && !bWasPressed)
        {
            if (ArmedPawn && ArmedPawn->bHasHouseMask &&
                X>=W-100*S && X<=W-24*S && Y>=72*S && Y<=160*S &&
                (PlayerOwner->bShowMouseCursor || bMaskTouch))
            {
                if (ASeniorLobbyController* PC=Cast<ASeniorLobbyController>(PlayerOwner);
                    !PC || !PC->IsPauseMenuOpen())
                {
                    ArmedPawn->ToggleHouseMask();
                    if (PC && PC->bShowMouseCursor) PC->ToggleEquipmentCursor();
                }
            }
            if (ArmedPawn && (ArmedPawn->Douli || ArmedPawn->SaucePacket) && X>=W-150*S && Y>=H-100*S && X<=W-20*S && Y<=H-35*S)
                ArmedPawn->ServerThrowEquippedWeapon();
            if (X >= BX && X <= BX+BW && Y >= 18*S && Y <= 18*S+BH)
                if (ASeniorLobbyController* PC = Cast<ASeniorLobbyController>(PlayerOwner)) PC->TogglePauseMenu();
        }
    }
    bWasPressed=Pressed;
}
