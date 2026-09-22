// Real-world integration test, enabled only in Editor builds with -StorySmokeTest=Full|Seed|Resume.
// Uses an isolated save slot. It drives the actual collision triggers and map travel.
#include "StoryCampaign.h"
#if WITH_EDITOR
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void TickStorySmokeTest(UWorld* World)
{
    FString Mode;
    if (!FParse::Value(FCommandLine::Get(), TEXT("StorySmokeTest="), Mode)) return;
    static int32 Step = 0, ExpectedChapter = 1;
    static double Last = 0, Start = FPlatformTime::Seconds();
    auto Check = [](bool Condition, const TCHAR* Message) {
        if (!Condition) { UE_LOG(LogTemp, Error, TEXT("STORY_TEST_FAILED: %s"), Message); FPlatformMisc::RequestExitWithStatus(false, 1); }
        return Condition;
    };
    if (!Check(FPlatformTime::Seconds()-Start < 100, TEXT("Campaign test timed out"))) return;
    if (FPlatformTime::Seconds()-Last < 1) return;
    Last = FPlatformTime::Seconds();
    auto* Story = World->GetGameInstance<UStoryCampaign>();
    auto* PC = World->GetFirstPlayerController();
    if (!Story || !PC || !PC->GetPawn() || Story->bTravelPending || !Story->CurrentChapter()) return;
    auto Move = [World, PC, Check](int32 Index, bool Exit) {
        TArray<AActor*> Markers;
        UGameplayStatics::GetAllActorsOfClass(World, AStoryCheckpoint::StaticClass(), Markers);
        for (AActor* Actor : Markers)
        {
            auto* Marker = Cast<AStoryCheckpoint>(Actor);
            if (Marker->bChapterExit == Exit && (Exit || Marker->CheckpointIndex == Index))
            {
                PC->GetPawn()->SetActorLocation(Marker->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
                return;
            }
        }
        Check(false, TEXT("Checkpoint actor missing from world"));
    };
    if (Mode == TEXT("Resume"))
    {
        if (!Check(Story->CurrentChapter()==2 && Story->Progress->Chapter==2 && Story->Progress->Checkpoint==1, TEXT("Saved chapter/checkpoint not restored"))) return;
        if (!Check(FVector::Dist(PC->GetPawn()->GetActorLocation(), Story->Progress->Spawn.GetLocation())<160, TEXT("Saved spawn not restored"))) return;
        UE_LOG(LogTemp, Display, TEXT("STORY_TEST_PASSED: Relaunch resumed Chapter 2 checkpoint 1"));
        FPlatformMisc::RequestExitWithStatus(false, 0); return;
    }
    switch (Step)
    {
    case 0: Story->NewStory(); Step=1; break;
    case 1:
    {
        if (!Check(Story->CurrentChapter()==ExpectedChapter, TEXT("Incorrect sequential chapter"))) return;
        auto* Character = Cast<AStoryFirstPersonCharacter>(PC->GetPawn());
        if (!Check(Cast<ASeniorLobbyController>(PC) && PC->GetPlayerState<ASeniorLobbyPlayerState>(), TEXT("Shared lobby controller/player state missing in chapter"))) return;
        if (!Check(Character && Character->GetIsReplicated() && Character->GetMesh()->GetSkeletalMeshAsset(), TEXT("First-person pawn must be replicated and visible to teammates"))) return;
        if (ULocalPlayer* Local = PC->GetLocalPlayer())
        {
            auto* Input = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
            if (!Check(Input && Input->HasMappingContext(Character->InputContext), TEXT("Movement input mapping missing after travel"))) return;
        }
        Story->CompleteChapter();
        if (!Check(!Story->bTravelPending && !Story->Progress->bCompleted, TEXT("Exit allowed before checkpoints"))) return;
        Move(2, false); Step=2; break;
    }
    case 2:
        if (!Check(Story->Progress->Checkpoint==0, TEXT("Out-of-order checkpoint accepted"))) return;
        Move(1, false); Step=3; break;
    case 3:
    {
        if (!Check(Story->Progress->Checkpoint==1, TEXT("Checkpoint overlap did not save"))) return;
        auto* Team = World->GetGameState<AStoryGameState>();
        if (!Check(Team && Team->StoryChapter==ExpectedChapter && Team->StoryCheckpoint==1, TEXT("Team checkpoint replication state not updated"))) return;
        auto* Disk = Cast<UStorySave>(UGameplayStatics::LoadGameFromSlot(TEXT("SeniorSendoff_AutomationOnly"), 0));
        if (!Check(Disk && Disk->Chapter==ExpectedChapter && Disk->Checkpoint==1, TEXT("Checkpoint not persisted on disk"))) return;
        if (Mode==TEXT("Seed") && ExpectedChapter==2)
        {
            UE_LOG(LogTemp, Display, TEXT("STORY_TEST_PASSED: Seeded Chapter 2 checkpoint 1 for relaunch"));
            FPlatformMisc::RequestExitWithStatus(false, 0); return;
        }
        Story->RespawnAtCheckpoint(); Step=4; break;
    }
    case 4:
        if (!Check(FVector::Dist(PC->GetPawn()->GetActorLocation(), Story->Progress->Spawn.GetLocation())<160, TEXT("Checkpoint respawn incorrect"))) return;
        Move(2, false); Step=5; break;
    case 5:
        if (!Check(Story->Progress->Checkpoint==2, TEXT("Second checkpoint failed"))) return;
        Move(0, true); Step=6; break;
    case 6:
        if (ExpectedChapter==3)
        {
            if (!Check(Story->Progress->bCompleted, TEXT("Ending did not complete campaign"))) return;
            auto* Disk = Cast<UStorySave>(UGameplayStatics::LoadGameFromSlot(TEXT("SeniorSendoff_AutomationOnly"), 0));
            if (!Check(Disk && Disk->bCompleted, TEXT("Ending not persisted"))) return;
            UE_LOG(LogTemp, Display, TEXT("STORY_TEST_PASSED: Three chapters, ordered checkpoints, respawn, saves and ending"));
            FPlatformMisc::RequestExitWithStatus(false, 0); return;
        }
        ++ExpectedChapter; Step=1;
        UE_LOG(LogTemp, Display, TEXT("STORY_TEST: Entered chapter %d"), ExpectedChapter);
        break;
    }
}
#endif
