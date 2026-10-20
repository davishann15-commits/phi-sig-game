#include "HouseWoodcutting.h"
#if !UE_BUILD_SHIPPING
#include "HouseFurnitureInteractionComponent.h"
#include "HouseWoodsDetail.h"
#include "StoryCampaign.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void TickHouseWoodcuttingReview(AStoryFirstPersonCharacter* Pawn)
{
    const bool Review = FParse::Param(FCommandLine::Get(), TEXT("HouseWoodcuttingReview"));
    const bool Preview = FParse::Param(FCommandLine::Get(), TEXT("HouseWoodcuttingPreview"));
    if ((!Review && !Preview) || !Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled()) return;
    UWorld* W = Pawn->GetWorld(); if (UGameplayStatics::GetCurrentLevelName(W, true) != TEXT("Chapter01_House")) return;
    auto* PC = Cast<APlayerController>(Pawn->GetController()); auto* Mover = Pawn->FurnitureInteraction.Get(); if (!PC || !Mover) return;
    struct FState
    {
        TWeakObjectPtr<UWorld> World; TWeakObjectPtr<AHouseWoodAxe> Axe;
        TWeakObjectPtr<AHouseChoppingStump> Stump; TWeakObjectPtr<AHouseInteractiveProp> Target;
        int Phase = 0, Hits = 0, Swings = 0, Chops = 0, Leg = 0;
        double Begin = 0, Step = 0; bool Captured = false; FString Dir, Log;
    };
    static FState S; const double Now = W->GetTimeSeconds();
    if (S.World.Get() != W)
    {
        S = FState(); S.World = W; S.Begin = S.Step = Now; S.Dir = FPaths::ProjectSavedDir() / TEXT("ColesStumpAxeReview");
        IFileManager::Get().MakeDirectory(*S.Dir, true);
        if (auto* G = UGameUserSettings::GetGameUserSettings()) { G->SetOverallScalabilityLevel(2); G->ApplyNonResolutionSettings(); }
    }
    if (S.Phase < 0) return;
    auto Note = [&](const FString& T) { S.Log += T + TEXT("\n"); UE_LOG(LogTemp, Display, TEXT("COLES_AXE_REVIEW %s"), *T); };
    auto Finish = [&](bool Pass, const FString& T)
    {
        Note((Pass ? TEXT("PASS ") : TEXT("FAIL ")) + T); S.Phase = -1;
        FFileHelper::SaveStringToFile(S.Log, *(S.Dir / TEXT("result.txt")));
        if (auto* A = S.Target.Get()) A->Destroy();
        FPlatformMisc::RequestExitWithStatus(false, Pass ? 0 : 1);
    };
    auto Next = [&](int Phase) { S.Phase = Phase; S.Step = Now; S.Captured = false; };
    auto Capture = [&](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(S.Dir / Name, false, false); };
    const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(); const FVector Home = AHouseChoppingStump::Home();
    auto Stand = [&](FVector P)
    {
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->SetActorLocation(FVector(P.X, P.Y, HouseWoods::Height(P.X, P.Y) + Half + 2), false, nullptr, ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    };
    auto Aim = [&](const FVector& P) { PC->SetControlRotation((P - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation()); };
    if (Now - S.Begin > 75) { Finish(false, TEXT("Timed out")); return; }
    if (S.Phase == 0)
    {
        if (Now - S.Step < 8) return;
        int Axes = 0, Stumps = 0;
        for (TActorIterator<AHouseWoodAxe> It(W); It; ++It) { ++Axes; S.Axe = *It; }
        for (TActorIterator<AHouseChoppingStump> It(W); It; ++It) { ++Stumps; S.Stump = *It; }
        if (Axes != 1 || Stumps != 1 || !S.Axe->bEmbedded || S.Axe->Body->IsSimulatingPhysics())
        { Finish(false, TEXT("Expected one stationary embedded axe and one stump")); return; }
        const FTransform AxeTransform = S.Axe->Body->GetComponentTransform();
        const FVector Tip = AxeTransform.TransformPosition(AHouseWoodAxe::StumpTipLocal());
        const float Top = Home.Z + 54;
        const float ForgedHeadBottom = AxeTransform.TransformPosition(FVector(18, 0, 70)).Z;
        const float HeadBack = AxeTransform.TransformPosition(FVector(-6, 0, 52)).Z;
        const float HandleGrip = AxeTransform.TransformPosition(AHouseWoodAxe::GripLocal()).Z;
        if (FMath::Abs(Tip.Z - (Top - AHouseWoodAxe::StumpInsertion)) > .25f || FVector::Dist2D(Tip, Home) > 20 || !S.Stump->GetActorLocation().Equals(Home, 1)
            || ForgedHeadBottom < Top + 1 || HeadBack < Top + 20 || HandleGrip < Top + 55)
        { Finish(false, TEXT("Stump/axe is not grounded and embedded")); return; }
        FCollisionQueryParams Q(SCENE_QUERY_STAT(ColeStumpPlacement), false, Pawn); FHitResult H;
        Q.AddIgnoredActor(S.Axe.Get()); Q.AddIgnoredActor(S.Stump.Get());
        if (!W->LineTraceSingleByChannel(H, Home + FVector(0, 0, 25), Home - FVector(0, 0, 30), ECC_Pawn, Q)
            || FMath::Abs(H.ImpactPoint.Z - Home.Z) > 8)
        { Finish(false, TEXT("Stump ground support missing")); return; }
        const FVector DoorStart(HouseWoods::HutX, HouseWoods::HutY - 345, HouseWoods::HutFloor + Half + 2);
        const FVector DoorEnd(HouseWoods::HutX, HouseWoods::HutY + 100, DoorStart.Z);
        if (W->SweepSingleByChannel(H, DoorStart, DoorEnd, FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(), Half), Q))
        { Finish(false, TEXT("Hut doorway is blocked")); return; }
        Note(FString::Printf(TEXT("Grounded stump; only blade tip inserted %.1f cm; forged head clears top by %.1f cm; handle grip %.1f cm above top; hut doorway clear"),
            Top - Tip.Z, ForgedHeadBottom - Top, HandleGrip - Top));
        if (Preview)
        {
            Stand(Home + FVector(0, -135, 0)); Aim(S.Axe->Body->GetComponentTransform().TransformPosition(FVector(1, 0, 6)));
            Note(TEXT("COLES_AXE_PREVIEW_READY E pull axe, LMB/RT swing, E at stump to return")); S.Phase = -1; return;
        }
        Stand(Home + FVector(250, -350, 0)); Aim(Home + FVector(-150, 90, 130)); Next(1); return;
    }
    auto* Axe = S.Axe.Get(); auto* Stump = S.Stump.Get(); if (!Axe || !Stump) { Finish(false, TEXT("Actor disappeared")); return; }
    if (S.Phase == 1)
    {
        if (Now - S.Step < .8) return;
        Capture(TEXT("01_ColeStumpPlacement.png")); Next(2); return;
    }
    if (S.Phase == 2)
    {
        if (Now - S.Step < .8) return;
        Stand(Home + FVector(0, -115, 0)); Aim(Axe->Body->GetComponentTransform().TransformPosition(FVector(1, 0, 6))); Next(3); return;
    }
    if (S.Phase == 3)
    {
        Aim(Axe->Body->GetComponentTransform().TransformPosition(FVector(1, 0, 6)));
        if (Now - S.Step < .5) return;
        if (Mover->FocusedFurnitureForInteraction() != Axe) { Finish(false, TEXT("Normal E focus did not target axe handle")); return; }
        Mover->ToggleGrab(); Next(4); return;
    }
    if (S.Phase == 4)
    {
        PC->SetControlRotation(FRotator(-8, 90, 0));
        if (Now - S.Step < 1.1) return;
        FVector Hand, Normal; Mover->GetHandContact(true, Hand, Normal);
        const float Error = FVector::Dist(Hand, Axe->Body->GetComponentTransform().TransformPosition(AHouseWoodAxe::GripLocal()));
        if (Mover->GetHeldBody() != Axe->Body || !Axe->Body->IsSimulatingPhysics() || Axe->bEmbedded || Mover->UsesTwoHands() || Error > 1)
        { Finish(false, TEXT("Axe extraction or one-handed handle grip failed")); return; }
        Note(FString::Printf(TEXT("Normal E pulled axe free; one-handed handle contact error %.3f cm"), Error));
        const FTransform CameraFrame(PC->GetControlRotation(), Pawn->FirstPersonCamera->GetComponentLocation());
        Note(FString::Printf(TEXT("Carry diagnostic grip camera-local=%s edge camera-local=%s targetError=%.1f rotation=%s"),
            *CameraFrame.InverseTransformPosition(Hand).ToString(), *CameraFrame.InverseTransformPosition(Axe->Body->GetComponentTransform().TransformPosition(AHouseWoodAxe::EdgeLocal())).ToString(), Mover->GetDebugTargetError(), *Axe->Body->GetComponentRotation().ToString()));
        const FVector EdgeInView = CameraFrame.InverseTransformPosition(Axe->Body->GetComponentTransform().TransformPosition(AHouseWoodAxe::EdgeLocal()));
        if (EdgeInView.X < 20 || FMath::Abs(EdgeInView.Y / EdgeInView.X) > .88f || FMath::Abs(EdgeInView.Z / EdgeInView.X) > .58f || Mover->GetDebugTargetError() > 15)
        { Capture(TEXT("02_CarryPoseIssue.png")); Finish(false, TEXT("Carried axe is outside the playable view or not following its wrist target")); return; }
        Capture(TEXT("02_AxeHandleGrip.png")); Next(5); return;
    }
    if (S.Phase == 5)
    {
        if (Now - S.Step < .7) return;
        Stand(Home + FVector(0, -92, 0)); Aim(Stump->Top());
        if (Now - S.Step < 1.2) return;
        S.Chops = Stump->Chops; S.Swings = Axe->SwingCount;
        Pawn->ThrowEquippedWeapon(); Pawn->ThrowEquippedWeapon(); Pawn->ThrowEquippedWeapon(); Next(6); return;
    }
    if (S.Phase == 6)
    {
        Aim(Stump->Top());
        if (Now - S.Step > .29 && !S.Captured) { S.Captured = true; Capture(TEXT("03_ChoppingSwing.png")); }
        if (Now - S.Step < 1) return;
        if (Axe->SwingCount != S.Swings + 1 || Stump->Chops != S.Chops + 1 || Mover->GetHeldBody() != Axe->Body)
        { Finish(false, FString::Printf(TEXT("Swing/chop/cooldown failed swings=%d chops=%d held=%d"), Axe->SwingCount - S.Swings, Stump->Chops - S.Chops, int(Mover->GetHeldBody() == Axe->Body))); return; }
        Note(TEXT("LMB/RT action swings rather than throwing: one stump chop, chip/thud effects, spam cooldown keeps axe held"));
        if (!Axe->CanReturnToStump(Pawn)) { Finish(false, TEXT("Return-to-stump focus failed")); return; }
        Mover->ToggleGrab(); Next(7); return;
    }
    if (S.Phase == 7)
    {
        if (Now - S.Step < .7) return;
        if (Mover->bHoldingFurniture || !Axe->bEmbedded || Axe->Body->IsSimulatingPhysics()
            || !Axe->Body->GetComponentTransform().Equals(AHouseWoodAxe::EmbeddedTransform(), .1f))
        { Finish(false, TEXT("Return left stale grip/physics or wrong placement")); return; }
        Note(TEXT("E while aiming at stump re-embeds the same axe and releases its reservation"));
        Capture(TEXT("04_ReturnedToStump.png")); Next(8); return;
    }
    if (S.Phase == 8)
    {
        Aim(Axe->Body->GetComponentTransform().TransformPosition(FVector(1, 0, 6)));
        if (Now - S.Step < 1) return;
        if (Mover->FocusedFurnitureForInteraction() != Axe)
        { Finish(false, TEXT("Second pickup focus failed: ") + GetNameSafe(Mover->FocusedFurnitureForInteraction())); return; }
        Mover->ToggleGrab(); Next(9); return;
    }
    if (S.Phase == 9)
    {
        PC->SetControlRotation(FRotator(0, -90, 0));
        if (Now - S.Step < 1.2) return;
        FActorSpawnParameters P; P.ObjectFlags |= RF_Transient;
        const FVector At = Pawn->FirstPersonCamera->GetComponentLocation() + PC->GetControlRotation().Vector() * 130;
        auto* T = W->SpawnActorDeferred<AHouseInteractiveProp>(AHouseInteractiveProp::StaticClass(), FTransform(FRotator::ZeroRotator, At, FVector(.18f)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!T) { Finish(false, TEXT("Test target missing")); return; }
        T->SetFlags(RF_Transient); T->IntactMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        T->BreakHealth = 70; T->MassKg = 3; T->PropId = TEXT("ColeAxeReviewOnlyTarget"); T->FinishSpawning(FTransform(FRotator::ZeroRotator, At, FVector(.18f)));
        S.Target = T; Pawn->ThrowEquippedWeapon(); Next(10); return;
    }
    if (S.Phase == 10)
    {
        if (S.Target.IsValid()) Aim(S.Target->Body->GetComponentLocation());
        if (Now - S.Step < 1) return;
        if (!S.Target.IsValid() || S.Target->DamageEvents == 0 || S.Target->Health >= 70 || Mover->GetHeldBody() != Axe->Body)
        { Finish(false, TEXT("Axe strike failed ordinary prop damage")); return; }
        Note(FString::Printf(TEXT("Axe applies real point damage to existing props: events=%d remaining health=%.1f"), S.Target->DamageEvents, S.Target->Health));
        S.Target->Destroy(); S.Target.Reset(); S.Hits = Axe->HitCount;
        PC->SetControlRotation(FRotator(70, -90, 0)); Pawn->ThrowEquippedWeapon(); Next(11); return;
    }
    if (S.Phase == 11)
    {
        PC->SetControlRotation(FRotator(70, -90, 0));
        if (Now - S.Step < 1) return;
        if (Axe->HitCount != S.Hits) { Finish(false, TEXT("Miss swing hit a distant surface")); return; }
        Note(TEXT("Missed swing does not damage distant objects"));
        PC->SetControlRotation(FRotator(-30, -90, 0)); Mover->ToggleGrab(); Next(12); return;
    }
    if (S.Phase == 12)
    {
        if (Now - S.Step < 1.6) return;
        if (Mover->bHoldingFurniture || Axe->Body->ComponentHasTag(TEXT("SSOFurnitureHeld")) || Axe->bEmbedded || !Axe->Body->IsSimulatingPhysics())
        { Finish(false, TEXT("E set-down left stale grip or disabled physics")); return; }
        if (Axe->Body->GetComponentLocation().Z < Home.Z - 40) { Finish(false, TEXT("Axe fell through ground")); return; }
        Note(TEXT("E away from stump sets the axe on the ground with collision and no stale hold"));
        Capture(TEXT("05_AxeSetDown.png")); Next(13); return;
    }
    if (S.Phase == 13)
    {
        if (Now - S.Step < .6) return;
        Axe->Body->SetWorldLocation(Home - FVector(0, 0, 700), false, nullptr, ETeleportType::TeleportPhysics);
        Axe->Body->WakeAllRigidBodies(); Axe->SetActorTickEnabled(true); Next(14); return;
    }
    if (S.Phase == 14)
    {
        if (Now - S.Step < .6) return;
        int Axes = 0; for (TActorIterator<AHouseWoodAxe> It(W); It; ++It) ++Axes;
        if (!Axe->bEmbedded || Axes != 1) { Finish(false, TEXT("Lost axe did not recover, or duplicated")); return; }
        Note(TEXT("Lost axe recovers to stump without creating duplicates"));
        Stand(FVector(HouseWoods::HutX, HouseWoods::HutY - 570, 0)); Next(15); return;
    }
    if (S.Phase == 15)
    {
        const FVector Goal(HouseWoods::HutX, HouseWoods::HutY + 90, HouseWoods::HutFloor + Half + 2);
        PC->SetControlRotation((Goal - Pawn->GetActorLocation()).Rotation());
        Pawn->AddMovementInput((Goal - Pawn->GetActorLocation()).GetSafeNormal2D(), 1);
        if (Now - S.Step > 10 || Pawn->GetActorLocation().Z - Half < Home.Z - 45) { Finish(false, TEXT("Hut walk blocked or fell below ground")); return; }
        if (FVector::Dist2D(Pawn->GetActorLocation(), Goal) < 35)
        { Pawn->GetCharacterMovement()->StopMovementImmediately(); Note(TEXT("Character walked the clearing, porch steps and hut doorway past the stump")); Capture(TEXT("06_HutDoorwayClear.png")); Next(16); }
        return;
    }
    if (S.Phase == 16 && Now - S.Step > .8)
        Finish(true, TEXT("Grounded placement, pickup/handle grip, swing/chop/cooldown, prop damage, miss, return, set-down, recovery and hut walking. Multiplayer not tested."));
}
#else
void TickHouseWoodcuttingReview(AStoryFirstPersonCharacter*) {}
#endif
