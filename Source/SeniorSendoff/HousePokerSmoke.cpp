// Opt-in native check. Requires the installed poker actor/cards in Chapter 1.
#include "HousePokerTable.h"
#include "StoryCampaign.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
struct FPokerSmoke
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AHousePokerTable> Table;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;
    TWeakObjectPtr<APlayerController> Controller;
    double Started = 0, StageStarted = 0;
    int32 Stage = 0, DealsPassed = 0, InitialSerial = 0, Presses = 0;
    TArray<int32> Previous;
    FRotator Aim = FRotator::ZeroRotator;
    FString CapturePath;
    int32 CaptureFrames = 0;
};

bool Press(APlayerController* Controller, EInputEvent Event)
{
    if (!Controller || !Controller->PlayerInput || !FSlateApplication::IsInitialized()) return false;
    const FInputKeyEventArgs Args(nullptr, FSlateApplication::Get().GetInputDeviceIdForMouse(), EKeys::E, Event, 1.f, false, 0);
    Controller->InputKey(Args);
    return true;
}

bool Place(UWorld* World, AStoryFirstPersonCharacter* Pawn, APlayerController* PC, AHousePokerTable* Table, FRotator& Aim)
{
    const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector CameraOffset = Pawn->FirstPersonCamera->GetComponentLocation() - Pawn->GetActorLocation();
    const TArray<FVector> Candidates = {FVector(0,-175,0), FVector(270,0,0), FVector(0,175,0),
        FVector(-270,0,0), FVector(65,-175,0), FVector(-65,-175,0), FVector(245,-90,0)};
    for (const FVector& Local : Candidates)
    {
        const FVector Probe = Table->GetActorTransform().TransformPosition(Local);
        FCollisionQueryParams Query;
        Query.AddIgnoredActor(Pawn); Query.AddIgnoredActor(Table);
        FHitResult Floor;
        if (!World->LineTraceSingleByChannel(Floor, Probe+FVector(0,0,30), Probe-FVector(0,0,80), ECC_Visibility, Query)
            || Floor.ImpactNormal.Z < .65f) continue;
        const FVector Center = Floor.ImpactPoint+FVector(0,0,Half+2.f);
        if (World->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(Radius,Half), Query)) continue;
        const FVector View = Center+CameraOffset;
        Aim = (Table->InteractionLocation()-View).Rotation();
        Pawn->SetActorLocation(Center, false, nullptr, ETeleportType::TeleportPhysics);
        Pawn->SetActorRotation(FRotator(0,Aim.Yaw,0));
        PC->SetControlRotation(Aim);
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_POKER_SMOKE_PLACED pawn=%s target=%s aim=%s"),*Center.ToString(),*Table->InteractionLocation().ToString(),*Aim.ToString());
        return true;
    }
    return false;
}

bool EvaluatorCheck()
{
    struct FCase { TArray<int32> Cards; FString Name; };
    const TArray<FCase> Cases = {
        {{0,9,10,11,12},TEXT("ROYAL FLUSH")}, {{0,1,2,3,4},TEXT("STRAIGHT FLUSH")},
        {{0,13,26,39,1},TEXT("FOUR OF A KIND")}, {{0,13,26,1,14},TEXT("FULL HOUSE")},
        {{0,2,5,8,11},TEXT("FLUSH")}, {{0,14,28,42,4},TEXT("STRAIGHT")},
        {{0,13,26,2,4},TEXT("THREE OF A KIND")}, {{0,13,1,14,4},TEXT("TWO PAIR")},
        {{0,13,2,4,6},TEXT("ONE PAIR")}, {{0,15,31,47,11},TEXT("HIGH CARD")},
        {{0,0,1,2,3},TEXT("")}, {{0,1,2,3,52},TEXT("")}};
    for (const FCase& Test : Cases)
        if (AHousePokerTable::DescribeHand(Test.Cards) != Test.Name) return false;
    return true;
}
}
#endif

void TickHousePokerSmoke(UWorld* World)
{
#if !UE_BUILD_SHIPPING
    if (!World || !FParse::Param(FCommandLine::Get(),TEXT("HousePokerSmoke"))
        || UGameplayStatics::GetCurrentLevelName(World,true) != TEXT("Chapter01_House")) return;
    static FPokerSmoke S;
    if (S.World.Get() != World) { S = FPokerSmoke(); S.World = World; }
    if (S.Stage < 0) return;
    const double Now = FPlatformTime::Seconds();
    if (!S.Started) S.Started = Now;
    auto Finish = [&](bool Pass, const FString& Why)
    {
        UE_LOG(LogTemp,Display,
            TEXT("HOUSE_POKER_SMOKE_RESULT passed=%d deals=%d actualEPresses=%d detail=%s"),Pass?1:0,S.DealsPassed,S.Presses,*Why);
        S.Stage = -1;
        FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
    };
    if (Now-S.Started > 35.0) { Finish(false,TEXT("Timed out waiting for native input/focus/settled cards")); return; }
    APlayerController* PC = World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = PC ? Cast<AStoryFirstPersonCharacter>(PC->GetPawn()) : nullptr;
    if (!Pawn || !World->GetGameViewport() || !PC->PlayerInput) return;
    if (S.Stage == 0)
    {
        if (Now-S.Started < 3.0) return;
        if (!EvaluatorCheck()) { Finish(false,TEXT("Poker category/invalid-hand evaluator check failed")); return; }
        TActorIterator<AHousePokerTable> It(World);
        if (It) S.Table = *It;
        AHousePokerTable* Table = S.Table.Get();
        if (!Table || !Table->CardMesh || !Table->CardMaterial) { Finish(false,TEXT("No configured poker table/cards in the installed map")); return; }
        S.Pawn = Pawn; S.Controller = PC;
        if (!Place(World,Pawn,PC,Table,S.Aim)) { Finish(false,TEXT("No clear standing approach near the table")); return; }
        S.Stage=1; S.StageStarted=Now; return;
    }
    AHousePokerTable* Table = S.Table.Get();
    if (!Table || S.Pawn.Get()!=Pawn) { Finish(false,TEXT("Table or possessed pawn changed")); return; }
    if (S.Stage == 1)
    {
        if (Now-S.StageStarted < .2) return;
        if (Pawn->FocusedHousePokerTable()!=Table || Table->InteractionPrompt()!=TEXT("E  DEAL FIVE CARDS"))
        { Finish(false,TEXT("Initial focused E prompt unavailable")); return; }
        S.InitialSerial=Table->GetDealSerial();
        PC->SetControlRotation(S.Aim+FRotator(0,180,0));
        S.Stage=2; S.StageStarted=Now; return;
    }
    if (S.Stage == 2)
    {
        if (Now-S.StageStarted < .1) return;
        if (Pawn->FocusedHousePokerTable()) { Finish(false,TEXT("Poker focus persists while looking away")); return; }
        if (!Press(PC,IE_Pressed)) { Finish(false,TEXT("Native E input unavailable")); return; }
        ++S.Presses; S.Stage=3; S.StageStarted=Now; return;
    }
    if (S.Stage == 3)
    {
        Press(PC,IE_Released);
        if (Table->GetDealSerial()!=S.InitialSerial) { Finish(false,TEXT("E dealt while not focused")); return; }
        PC->SetControlRotation(S.Aim);
        S.Stage=4; S.StageStarted=Now; return;
    }
    if (S.Stage == 4)
    {
        if (Now-S.StageStarted < .15) return;
        if (Pawn->FocusedHousePokerTable()!=Table) { Finish(false,TEXT("Focus lost before repeated deal")); return; }
        if (!Press(PC,IE_Pressed)) { Finish(false,TEXT("Native E key dispatch failed")); return; }
        ++S.Presses; S.Stage=5; S.StageStarted=Now; return;
    }
    if (S.Stage == 5)
    {
        Press(PC,IE_Released);
        S.Stage=6; return;
    }
    if (S.Stage == 6)
    {
        if (Now-S.StageStarted < 1.6) return;
        const TArray<int32> Hand=Table->GetHandIndices();
        TSet<int32> Unique;
        for (int32 Card : Hand) { if (Card<0 || Card>=52) { Finish(false,TEXT("Invalid card index")); return; } Unique.Add(Card); }
        TArray<int32> Sorted=Hand, Prior=S.Previous; Sorted.Sort(); Prior.Sort();
        if (Hand.Num()!=5 || Unique.Num()!=5 || Table->GetVisibleCardCount()!=5 || !Table->AreCardsSettled()
            || Table->GetDealSerial()!=S.InitialSerial+S.DealsPassed+1 || Table->IsActorTickEnabled()
            || Table->HandName().IsEmpty() || (S.DealsPassed>0 && Sorted==Prior))
        { Finish(false,TEXT("Expected five unique settled cards, a new hand, valid category, and idle tick disabled")); return; }
        if (Pawn->FocusedHousePokerTable()!=Table || Table->InteractionPrompt()!=TEXT("E  DEAL AGAIN"))
        { Finish(false,TEXT("Settled hand or repeat E prompt unavailable")); return; }
        FString Indices;
        for (int32 Card : Hand) { if (!Indices.IsEmpty()) Indices+=TEXT(","); Indices+=FString::FromInt(Card); }
        ++S.DealsPassed; S.Previous=Hand;
        UE_LOG(LogTemp,Display,TEXT("HOUSE_POKER_SMOKE_HAND deal=%d serial=%d indices=[%s] category=%s visible=%d settled=1 idleTick=0 prompt=%s"),
            S.DealsPassed,Table->GetDealSerial(),*Indices,*Table->HandName(),Table->GetVisibleCardCount(),*Table->InteractionPrompt());
        if (S.DealsPassed<3) { S.Stage=4; S.StageStarted=Now; return; }
        S.CapturePath = FPaths::ProjectSavedDir()/TEXT("HousePokerSmoke/poker_hand.png");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(S.CapturePath), true);
        IFileManager::Get().Delete(*S.CapturePath, false, true);
        FScreenshotRequest::RequestScreenshot(S.CapturePath, true, false);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_POKER_SMOKE_CAPTURE_REQUEST indices=[%s] path=%s"),*Indices,*S.CapturePath);
        S.CaptureFrames=0; S.Stage=8; S.StageStarted=Now; return;
    }
    if (S.Stage == 8)
    {
        // Keep the third hand and its exact camera/HUD view while the native
        // screenshot is rendered. The opt-in harness alone requests captures.
        ++S.CaptureFrames;
        if (S.CaptureFrames<3 || Now-S.StageStarted<.4) return;
        if (IFileManager::Get().FileSize(*S.CapturePath)<=0)
        {
            if (Now-S.StageStarted>5.) Finish(false,TEXT("Third-hand native screenshot was not saved"));
            return;
        }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_POKER_SMOKE_CAPTURE_SAVED frames=%d path=%s"),S.CaptureFrames,*S.CapturePath);
        PC->SetControlRotation(S.Aim+FRotator(0,180,0));
        S.Stage=7; S.StageStarted=Now; return;
    }
    if (S.Stage == 7 && Now-S.StageStarted>.15)
    {
        if (Pawn->FocusedHousePokerTable()) { Finish(false,TEXT("Poker HUD focus persists after leaving the table view")); return; }
        Finish(true,TEXT("Actual E input repeated three deals; five distinct cards, different hands, categories, settled poses, prompt gating, and idle tick verified"));
    }
#endif
}
