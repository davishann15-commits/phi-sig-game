#include "StoryCampaign.h"
#include "SeniorPlayerPreferences.h"
#include "StoryMovementComponent.h"
#include "HouseFurnitureInteractionComponent.h"
#include "ExistingHouseDoor.h"
#include "HouseOxbloodMask.h"
#include "HousePokerTable.h"
#include "HouseRoamingPig.h"
#include "SeniorLobbyUI.h"
#include "SeniorCharacterRoster.h"
#include "SeniorBraxtonVisual.h"
#include "SeniorRunnerVisual.h"
#include "SeniorFixerVisual.h"
#include "SeniorDouli.h"
#include "SeniorSauce.h"
#include "SeniorDouliAnim.h"
#include "SeniorDouliIdle.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
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
#if WITH_EDITOR
void TickStorySmokeTest(UWorld* World);
void TickDouliSmokeTest(AStoryFirstPersonCharacter* Pawn);
void TickSauceSmokeTest(AStoryFirstPersonCharacter* Pawn);
#endif
#if !UE_BUILD_SHIPPING
void TickCombinedHouseSmoke(UWorld* World);
void TickHouseVisualReview(UWorld* World);
void TickExistingHouseDoorSmoke(UWorld* World);
void TickHouseTraversalSmoke(UWorld* World);
void TickHouseChairSmoke(UWorld* World);
void TickHousePokerSmoke(UWorld* World);
void TickHousePigSmoke(UWorld* World);
#endif

void UStoryCampaign::Init()
{
    Super::Init();
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(),TEXT("CombinedHouseSmoke"))) SaveSlot=TEXT("SeniorSendoff_CombinedHouseAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseVisualReview"))) SaveSlot=TEXT("SeniorSendoff_HouseVisualAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("ExistingHouseDoorSmoke"))) SaveSlot=TEXT("SeniorSendoff_OriginalDoorAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseTraversalSmoke"))) SaveSlot=TEXT("SeniorSendoff_HouseTraversalAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HouseChairSmoke"))) SaveSlot=TEXT("SeniorSendoff_HouseChairAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HousePokerSmoke"))) SaveSlot=TEXT("SeniorSendoff_HousePokerAutomationOnly");
    if (FParse::Param(FCommandLine::Get(),TEXT("HousePigSmoke")) || FParse::Param(FCommandLine::Get(),TEXT("HousePigStairSmoke"))
        || FParse::Param(FCommandLine::Get(),TEXT("HousePigUpgradeSmoke"))) SaveSlot=TEXT("SeniorSendoff_HousePigAutomationOnly");
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
    Notify(Progress->bHasCheckpoint ? TEXT("Returned to checkpoint") : TEXT("Returned to chapter start"));
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
        TEXT("/Game/Items/HouseMask/SM_HouseOxbloodMask.SM_HouseOxbloodMask"));
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
    if (IsLocallyControlled() && FirstPersonCamera)
    {
        const float WantedFOV = SeniorPlayerPreferences::Get().FieldOfView;
        bool bUsePlayerFov = true;
#if !UE_BUILD_SHIPPING
        // The opt-in campaign render sweep fits the registered photo FOV.
        bUsePlayerFov = !FParse::Param(FCommandLine::Get(), TEXT("HouseVisualReview"));
#endif
        if (bUsePlayerFov && !FMath::IsNearlyEqual(FirstPersonCamera->FieldOfView, WantedFOV))
            FirstPersonCamera->SetFieldOfView(WantedFOV);
        // The capsule's crouch already lowers the viewpoint. A further eased
        // drop gives a sprint slide a readable low viewpoint without changing
        // its collision height or an observer's camera.
        const bool bSliding = GetStoryMovement() && GetStoryMovement()->IsSliding();
        const float TargetOffset = bSliding ? -36.f : 0.f;
        SlideCameraOffsetZ = FMath::FInterpTo(SlideCameraOffsetZ, TargetOffset,
            DeltaSeconds, bSliding ? 15.f : 10.f);
        FirstPersonCamera->SetRelativeLocation(FVector(0, 0, BaseEyeHeight + SlideCameraOffsetZ));
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
    if (BraxtonVisual) BraxtonVisual->SetDouliEquipped(Equipped,Motion,DouliFlightBlend);
    if (Equipped && FirstPersonArms->GetSkeletalMeshAsset() && !Cast<USeniorDouliAnim>(FirstPersonArms->GetAnimInstance()))
        FirstPersonArms->SetAnimInstanceClass(USeniorDouliAnim::StaticClass());
    if (auto* A=Cast<USeniorDouliAnim>(FirstPersonArms->GetAnimInstance()))
    {
        A->BaseSequence=CurrentArmsAnimation; A->bFirstPerson=true; A->bEquipped=Equipped; A->Motion=Motion; A->FlightBlend=DouliFlightBlend;
    }
    if (BraxtonVisual) BraxtonVisual->SetMoveSpeed(GetVelocity().Size2D());
    else if (RunnerVisual) RunnerVisual->SetMoveSpeed(GetVelocity().Size2D());
    else if (FixerVisual) FixerVisual->SetMoveSpeed(GetVelocity().Size2D());
    else UpdateBodyAnimation();
    if (bHouseMaskEquipped) UpdateHouseMaskVisual();
    if (AppliedCharacterIndex == 0 && !BraxtonVisual && GetNetMode() != NM_DedicatedServer)
        ClothMotion.Update(GetMesh(), DeltaSeconds, GetActorRotation().Yaw, GetVelocity().Size2D());
#if WITH_EDITOR
    TickDouliSmokeTest(this);
    TickSauceSmokeTest(this);
#endif
}
void AStoryFirstPersonCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
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
        FirstPersonArms->SetRelativeTransform(ArmsRelativeTransform);
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
            NewVisual->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
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
            NewVisual->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
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
            NewVisual->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
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
    const float OldHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    const float HalfHeight = Height * 0.5f;
    GetCapsuleComponent()->SetCapsuleHalfHeight(HalfHeight, false);
    // Keep the feet in place when the server applies a different stature; clients receive that root position.
    if (HasAuthority() && !FMath::IsNearlyEqual(OldHalfHeight, HalfHeight))
    {
        AddActorWorldOffset(FVector(0, 0, (HalfHeight - OldHalfHeight) * GetActorScale3D().Z), false, nullptr, ETeleportType::TeleportPhysics);
    }
    GetMesh()->SetRelativeLocation(FVector(0, 0, -HalfHeight - LowestPoint));
    // Network smoothing must preserve this character's new mesh-to-capsule offset.
    CacheInitialMeshOffset(GetMesh()->GetRelativeLocation(), GetMesh()->GetRelativeRotation());
    const float EyeHeightFromGround = Height * 0.935f;
    BaseEyeHeight = EyeHeightFromGround - HalfHeight;
    FirstPersonCamera->SetRelativeLocation(FVector(0, 0, BaseEyeHeight));
    SlideCameraOffsetZ = 0;
    FTransform AdjustedArms = ArmsRelativeTransform;
    // FBX already contains each person's body scale. Only the attachment offset needs the same proportion.
    AdjustedArms.SetTranslation(ArmsRelativeTransform.GetTranslation() * (Height / 184.0f));
    FirstPersonArms->SetRelativeTransform(AdjustedArms);
}
void AStoryFirstPersonCharacter::UpdateBodyAnimation()
{
    if (!BodyIdle && !BodyWalk) return;
    const float Speed = GetVelocity().Size2D();
    bWalkingAnimation = bWalkingAnimation ? Speed > 5.f : Speed > 12.f;
    UAnimSequence* Desired = bWalkingAnimation && BodyWalk ? BodyWalk.Get() : BodyIdle.Get();
    if (!Desired) Desired = BodyWalk;
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
    PlayerInputComponent->BindKey(EKeys::Gamepad_RightTrigger,IE_Pressed,this,&AStoryFirstPersonCharacter::ThrowEquippedWeapon);
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
    USkeletalMeshComponent* FaceBody = BraxtonVisual ? BraxtonVisual->GetBodyMesh() :
        RunnerVisual ? RunnerVisual->GetBodyMesh() : FixerVisual ? FixerVisual->GetBodyMesh() : GetMesh();
    if (!FaceBody) return;
    const FVector Forward = FaceBody->GetComponentTransform().TransformVectorNoScale(FVector(0,-1,0)).GetSafeNormal();
    const FVector Head = FaceBody->DoesSocketExist(TEXT("head")) ?
        FaceBody->GetSocketLocation(TEXT("head")) : FaceBody->GetComponentLocation() + FVector(0,0,160);
    HouseMaskVisual->SetWorldLocationAndRotation(Head + Forward * 8.f + FVector(0,0,12.f),
        FRotationMatrix::MakeFromYZ(Forward, FVector::UpVector).Rotator());
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
        if (!Pig->TryPlace(this)) ClientHouseMaskNotice(TEXT("Find a clear, level floor to put the pig down."));
        return;
    }
    if (!CarriedHousePig() && !(FurnitureInteraction && FurnitureInteraction->bHoldingFurniture)
        && FocusedHousePigForInteraction()==Pig) Pig->TryPickup(this);
}
bool AStoryFirstPersonCharacter::InteractWithExistingDoor(AExistingHouseDoor* Door)
{
    if (!Door) return false;
    if (Door->bLocked)
    {
        if (UStoryCampaign* Story = GetGameInstance<UStoryCampaign>())
            Story->Notify(TEXT("Locked: this area is not playable yet."));
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
void AStoryFirstPersonCharacter::InteractWithExistingDoorOrFurniture()
{
    if (!IsLocallyControlled()) return;
    if (const APlayerController* PC=Cast<APlayerController>(Controller); PC && PC->bShowMouseCursor) return;
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
    // A chair actually under the reticle wins over a nearby door's wider aim cone.
    if (FurnitureInteraction && FurnitureInteraction->FocusedFurnitureForInteraction())
    { FurnitureInteraction->ToggleGrab(); return; }
    if (AHouseRoamingPig* Pig=FocusedHousePigForInteraction())
    { ServerInteractHousePig(Pig); return; }
    if (AHousePokerTable* Table = FocusedHousePokerTable())
    { ServerDealHousePokerTable(Table); return; }
    if (InteractWithExistingDoor(FindFocusedExistingDoor(.45f))) return;
    if (FurnitureInteraction) FurnitureInteraction->ToggleGrab();
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
void AStoryFirstPersonCharacter::StartSprint()
{
    if (auto* Movement = GetStoryMovement()) Movement->SetSprintHeld(true);
}
void AStoryFirstPersonCharacter::StopSprint()
{
    if (auto* Movement = GetStoryMovement()) Movement->SetSprintHeld(false);
}
void AStoryFirstPersonCharacter::StartSlide()
{
    if (auto* Movement = GetStoryMovement()) Movement->SetSlideHeld(true);
}
void AStoryFirstPersonCharacter::StopSlide()
{
    if (auto* Movement = GetStoryMovement()) Movement->SetSlideHeld(false);
}
void AStoryFirstPersonCharacter::StartMovementJump()
{
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
    return (!IsCrouched() || !GetCharacterMovement()->bWantsToCrouch) && JumpIsAllowedInternal();
}
void AStoryFirstPersonCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AStoryFirstPersonCharacter,Douli);
    DOREPLIFETIME(AStoryFirstPersonCharacter,SaucePacket);
    DOREPLIFETIME(AStoryFirstPersonCharacter,bHasHouseMask);
    DOREPLIFETIME(AStoryFirstPersonCharacter,bHouseMaskEquipped);
}
void AStoryFirstPersonCharacter::ThrowEquippedWeapon()
{
    if(APlayerController* PC=Cast<APlayerController>(Controller);PC && PC->bShowMouseCursor)return;
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
    // UI clicks must not also launch a weapon behind the menu.
    if (APlayerController* PC=Cast<APlayerController>(Controller); PC && PC->bShowMouseCursor) return;
    ServerThrowDouli();
}
void AStoryFirstPersonCharacter::ServerThrowDouli_Implementation()
{
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
            // Four masks sit on different routes so each member of a party
            // can discover one without sharing a single-use pickup.
            if (HasAuthority() && Story->CurrentChapter() == 1)
            {
                const FVector Places[] = {
                    FVector(1116, 691, 8),      // Great room
                    FVector(2320, -272, 8),     // Dining room
                    FVector(-2646, -1012, 8),   // Ground-floor bedroom
                    FVector(2467, -1498, 321),  // Upstairs bedroom
                };
                for (const FVector& Place : Places)
                {
                    FActorSpawnParameters Params;
                    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                    GetWorld()->SpawnActor<AHouseOxbloodMaskPickup>(Place, FRotator::ZeroRotator, Params);
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
    if (Story && Story->Progress && Story->Progress->bHasCheckpoint && Story->CurrentChapter() == Story->Progress->Chapter)
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
    TickExistingHouseDoorSmoke(GetWorld());
    TickHouseTraversalSmoke(GetWorld());
    TickHouseChairSmoke(GetWorld());
    TickHousePokerSmoke(GetWorld());
    TickHousePigSmoke(GetWorld());
#endif
    auto* Story = GetGameInstance<UStoryCampaign>();
    if (!Story || Story->CurrentChapter() == 0 || Story->bTravelPending || Story->Progress->bCompleted) return;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (APlayerController* PC = It->Get(); PC && PC->HasClientLoadedCurrentWorld())
            if (!PC->GetPawn() || PC->GetPawn()->GetActorLocation().Z < FallLimit) Story->RespawnPlayerAtCheckpoint(PC);
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
    DrawRect(Ink, 18*S, 18*S, 540*S, 112*S);
    Text(TEXT("SENIOR SENDOFF"), 34*S, 30*S, Gold, 1.25f);
    Text(FString::Printf(TEXT("CHAPTER %d / 3    |    CHECKPOINT %d / 2    |    %s"), Story->CurrentChapter(),
        Checkpoint, *UStoryCampaign::DifficultyName(Difficulty)), 34*S, 66*S, FLinearColor::White);
    Text(TEXT("Reach both markers, then the chapter exit."), 34*S, 98*S, FLinearColor(.7f,.8f,.9f));
    if (Story->CurrentChapter() == 1)
    {
        DrawRect(Ink, 18*S, H-97*S, 548*S, 28*S);
        Text(ArmedPawn && ArmedPawn->CarriedHousePig()
            ? TEXT("Carry pig  |  E place on clear floor  |  Aim at door + E open / close")
            : ArmedPawn && ArmedPawn->FurnitureInteraction && ArmedPawn->FurnitureInteraction->bHoldingFurniture
            ? TEXT("E drop furniture  |  LMB / right trigger throw")
            : TEXT("Shift sprint  |  Ctrl slide  |  Space jump  |  E move furniture / doors  |  Double-click doors"),
            29*S, H-93*S, FLinearColor(.8f,.85f,.9f), .8f);
        if (ArmedPawn && ArmedPawn->FurnitureInteraction && PlayerOwner && !PlayerOwner->bShowMouseCursor)
        {
            const bool bHolding = ArmedPawn->FurnitureInteraction->bHoldingFurniture;
            // This same focused trace takes priority over nearby doors for E.
            const bool bFocused = !bHolding
                && ArmedPawn->FurnitureInteraction->FocusedFurnitureForInteraction();
            if (bHolding || bFocused)
            {
                const float PromptWidth = bHolding ? 280*S : 210*S;
                const float PromptX = (W - PromptWidth) * .5f;
                DrawRect(Ink, PromptX, H*.5f + 30*S, PromptWidth, 28*S);
                Text(bHolding ? TEXT("E DROP  |  LMB THROW") : TEXT("E PICK UP FURNITURE"),
                    PromptX + 12*S, H*.5f + 34*S, Gold, .82f);
            }
        }
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
    if (ArmedPawn && !ArmedPawn->FocusedHousePokerTable() && !ArmedPawn->FocusedHousePigForInteraction())
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
            const FString Prompt=HeldPig?FString::Printf(TEXT("E  PLACE %s ON FLOOR"),*HeldPig->PigName.ToUpper()):
                FString::Printf(TEXT("E  PICK UP %s"),*FocusedPig->PigName.ToUpper());
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
    if (ArmedPawn && (ArmedPawn->Douli || ArmedPawn->SaucePacket))
    {
        DrawRect(Ink,18*S,H-60*S,340*S,40*S);
        Text(ArmedPawn->SaucePacket?ArmedPawn->SaucePacket->StatusText():ArmedPawn->Douli->StatusText(),30*S,H-49*S,Gold,.85f);
        DrawLine(W*.5f-5,H*.5f,W*.5f+5,H*.5f,FLinearColor::White);
        DrawLine(W*.5f,H*.5f-5,W*.5f,H*.5f+5,FLinearColor::White);
#if PLATFORM_IOS || PLATFORM_ANDROID
        DrawRect(Ink,W-150*S,H-100*S,130*S,65*S);
        Text(TEXT("THROW"),W-125*S,H-79*S,Gold);
#endif
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
