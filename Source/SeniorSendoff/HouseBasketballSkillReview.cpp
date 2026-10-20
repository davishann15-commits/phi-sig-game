#include "HouseBasketballHoop.h"
#if !UE_BUILD_SHIPPING
#include "HouseBasketball.h"
#include "HouseBasketballCourt.h"
#include "HouseFurnitureInteractionComponent.h"
#include "StoryCampaign.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
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

void TickHouseBasketballSkillReview(AStoryFirstPersonCharacter* Pawn)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("HouseBasketballSkillReview"))
        || !Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled()) return;
    UWorld* W = Pawn->GetWorld();
    if (UGameplayStatics::GetCurrentLevelName(W, true) != TEXT("Chapter01_House")) return;
    auto* PC = Cast<APlayerController>(Pawn->GetController());
    auto* Mover = Pawn->FurnitureInteraction.Get(); if (!PC || !Mover) return;
    struct FState
    {
        TWeakObjectPtr<UWorld> World; TWeakObjectPtr<AHouseBasketball> Ball;
        TWeakObjectPtr<AHouseBasketballHoop> Hoops[2];
        int Phase = 0, Case = 0, BeforeScore = 0, BeforeContacts = 0, Way = 0;
        double Start = 0, Total = 0; float Ideal = 0, Power = 0, Peak = 0;
        bool Rebounded = false, Captured = false;
        FString Dir, Log;
    };
    static FState S; const double Now = W->GetTimeSeconds();
    auto Note = [&](const FString& T) { S.Log += T + TEXT("\n"); UE_LOG(LogTemp, Display, TEXT("BASKETBALL_SKILL_CHECK %s"), *T); };
    auto Capture = [&](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir, Name), false, false); };
    auto Finish = [&](bool Pass, const FString& Why)
    {
        S.Phase = -1; Note((Pass ? TEXT("PASS ") : TEXT("FAIL ")) + Why);
        FFileHelper::SaveStringToFile(S.Log, *FPaths::Combine(S.Dir, TEXT("result.txt")));
        FPlatformMisc::RequestExitWithStatus(false, Pass ? 0 : 1);
    };
    auto Next = [&](int Phase) { S.Phase = Phase; S.Start = Now; S.Captured = false; };
    auto PlaceBall = [&](const FVector& At, const FVector& Velocity = FVector::ZeroVector)
    {
        auto* Ball = S.Ball.Get(); Mover->ReleaseGrab();
        Ball->Body->SetWorldLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
        Ball->Body->SetPhysicsLinearVelocity(Velocity); Ball->Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        Ball->Body->SetEnableGravity(true); Ball->Body->WakeAllRigidBodies();
    };
    auto Stand = [&](float X, float Y)
    {
        Pawn->SetActorLocation(FVector(X, Y, HouseCourt::Floor + Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2), false, nullptr, ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately(); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    };
    if (S.World.Get() != W)
    {
        S = FState(); S.World = W; S.Total = S.Start = Now;
        S.Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BasketballSkillReview"));
        IFileManager::Get().MakeDirectory(*S.Dir, true);
        if (auto* G = UGameUserSettings::GetGameUserSettings()) { G->SetOverallScalabilityLevel(2); G->ApplyNonResolutionSettings(); }
    }
    if (S.Phase < 0) return;
    if (Now - S.Total > 125) { Finish(false, TEXT("Timed out")); return; }
    if (S.Phase == 0)
    {
        if (Now - S.Start < 8) return;
        int Balls = 0, Hoops = 0; float Best = BIG_NUMBER;
        for (TActorIterator<AHouseBasketball> It(W); It; ++It)
        {
            ++Balls; const float D = FVector::DistSquared(It->Body->GetComponentLocation(), HouseCourt::BallHome());
            if (D < Best) { Best = D; S.Ball = *It; }
        }
        for (TActorIterator<AHouseBasketballHoop> It(W); It; ++It) { ++Hoops; S.Hoops[It->End()] = *It; }
        if (Balls != 3 || Hoops != 2) { Finish(false, TEXT("Expected 3 balls and 2 live hoops")); return; }
        FCollisionQueryParams Q(SCENE_QUERY_STAT(SkillRimOpening), false, Pawn); Q.AddIgnoredActor(S.Ball.Get());
        for (int I = 0; I < 2; ++I)
        {
            const FVector R = HouseCourt::Rim(I); FHitResult Hit;
            if (W->SweepSingleByChannel(Hit, R + FVector(0, 0, 70), R - FVector(0, 0, 70), FQuat::Identity,
                ECC_WorldDynamic, FCollisionShape::MakeSphere(12.4f), Q))
            { Finish(false, TEXT("Rim opening is blocked")); return; }
        }
        Note(TEXT("Three shared balls; both live rims have a clear ball-sized opening")); Next(1); return;
    }
    auto* Ball = S.Ball.Get(); auto* Hoop = S.Hoops[0].Get();
    if (!Ball || !Hoop) { Finish(false, TEXT("Test actor disappeared")); return; }
    if (S.Phase == 1)
    {
        const float X = S.Case == 1 ? 400 : -400;
        Stand(X, 2820); PlaceBall(FVector(X, 2890, HouseCourt::Floor + 15));
        PC->SetControlRotation((Ball->Body->GetComponentLocation() - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        Next(2); return;
    }
    if (S.Phase == 2)
    {
        if (Now - S.Start < .5) return;
        PC->SetControlRotation((Ball->Body->GetComponentLocation() - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        if (Mover->FocusedFurnitureForInteraction() != Ball) { Finish(false, TEXT("Normal pickup focus failed")); return; }
        Mover->ToggleGrab(); Next(3); return;
    }
    const int End = S.Case == 1 ? 1 : 0;
    auto AimAtRim = [&]()
    {
        FRotator Aim = (HouseCourt::Rim(End) - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation();
        if (S.Case == 4) Aim.Yaw += 15;
        PC->SetControlRotation(Aim);
    };
    if (S.Phase == 3)
    {
        if (Now - S.Start < 1.1) return;
        if (Mover->GetHeldBody() != Ball->Body) { Finish(false, TEXT("Normal pickup failed")); return; }
        AimAtRim(); Next(4); return;
    }
    if (S.Phase == 4)
    {
        AimAtRim();
        if (S.Case == 0 && Now - S.Start > .2 && !S.Captured)
        { S.Captured = true; Capture(TEXT("01_ShotMeter.png")); }
        if (Now - S.Start < 1.3) return;
        S.Ideal = Mover->GetBasketballIdealPower();
        if (S.Ideal < 0 || S.Ideal > .8f) { Finish(false, TEXT("No playable timing band at normal shot range")); return; }
        S.Power = S.Ideal + (S.Case == 2 ? -.16f : S.Case == 3 ? .20f : 0);
        S.BeforeScore = Ball->LeftBaskets + Ball->RightBaskets;
        if (!Mover->BeginBasketballShot() || !Mover->bHoldingFurniture)
        { Finish(false, TEXT("Pressing shoot did not charge while retaining the ball")); return; }
        Next(5); return;
    }
    if (S.Phase == 5)
    {
        AimAtRim();
        if (S.Case == 5 && Now - S.Start > .25)
        {
            Mover->CancelBasketballShot();
            if (Mover->IsBasketballCharging() || !Mover->bHoldingFurniture) { Finish(false, TEXT("Cancelling shot dropped or threw ball")); return; }
            Note(TEXT("Charge cancel keeps the ball; E set-down clears charging"));
            Mover->ToggleGrab(); Next(7); return;
        }
        S.Ideal = Mover->GetBasketballIdealPower();
        S.Power = S.Ideal + (S.Case == 2 ? -.16f : S.Case == 3 ? .20f : 0);
        if (Mover->GetBasketballPower() < S.Power) return;
        Note(FString::Printf(TEXT("Case %d release power %.3f / target %.3f, aim=%s"), S.Case, Mover->GetBasketballPower(), S.Ideal, *PC->GetControlRotation().ToCompactString()));
        Mover->ReleaseBasketballShot();
        if (Mover->bHoldingFurniture || Mover->IsBasketballCharging()) { Finish(false, TEXT("Release did not immediately shoot")); return; }
        Note(Mover->GetBasketballFeedback()); Next(6); return;
    }
    if (S.Phase == 6)
    {
        const bool Made = Ball->LeftBaskets + Ball->RightBaskets > S.BeforeScore;
        if (S.Case < 2 && Made)
        {
            Note(FString::Printf(TEXT("Case %d: timed, aimed shot crossed actual rim downward"), S.Case));
            Capture(S.Case == 0 ? TEXT("02_LeftBasket.png") : TEXT("03_RightBasket.png"));
            ++S.Case; Next(1); return;
        }
        if (S.Case >= 2 && Made) { Finish(false, TEXT("Bad timing or mis-aim was auto-corrected into a score")); return; }
        if (Now - S.Start > 3.5)
        {
            if (S.Case < 2) { Note(Ball->Body->GetComponentLocation().ToString()); Finish(false, TEXT("Correctly timed aimed shot missed")); return; }
            Note(FString::Printf(TEXT("Case %d: intentional %s missed without score"), S.Case, S.Case == 2 ? TEXT("early release") : S.Case == 3 ? TEXT("late release") : TEXT("15 degree mis-aim")));
            ++S.Case; Next(1); return;
        }
        return;
    }
    if (S.Phase == 7)
    {
        if (Now - S.Start < .9) return;
        if (Mover->bHoldingFurniture || Mover->IsBasketballCharging() || Ball->Body->ComponentHasTag(TEXT("SSOFurnitureHeld")))
        { Finish(false, TEXT("Set-down left a stale hold/charge")); return; }
        Stand(-820, 2820);
        PC->SetControlRotation((HouseCourt::Rim(0) - FVector(0, 0, 22) - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        Capture(TEXT("04_NetRest.png"));
        UAudioMixerBlueprintLibrary::StartRecordingOutput(W, 12);
        S.BeforeScore = Hoop->RimHits; S.Peak = 0;
        PlaceBall(HouseCourt::Rim(0) + FVector(0, -24.2f, 65)); Next(8); return;
    }
    if (S.Phase == 8)
    {
        S.Peak = FMath::Max(S.Peak, Hoop->NetDisplacement());
        if (Ball->Body->GetPhysicsLinearVelocity().Z > 80) S.Rebounded = true;
        if (Hoop->RimHits > S.BeforeScore && !S.Captured) { S.Captured = true; Capture(TEXT("05_RimRebound.png")); }
        if (Now - S.Start < 1.5) return;
        if (Hoop->RimHits <= S.BeforeScore || !S.Rebounded || Hoop->AudioEvents == 0)
        { Finish(false, TEXT("Rounded rim failed physical rebound or clank playback")); return; }
        Note(FString::Printf(TEXT("Rounded rim rebounded physically; clank spawned. Net tug %.2f cm"), S.Peak));
        S.BeforeScore = Hoop->Swishes; S.BeforeContacts = Hoop->NetContacts; S.Peak = 0;
        PlaceBall(HouseCourt::Rim(0) + FVector(0, 0, 70), FVector(0, 0, -300)); Ball->MarkShot(); Next(9); return;
    }
    if (S.Phase == 9)
    {
        S.Peak = FMath::Max(S.Peak, Hoop->NetDisplacement());
        if (Hoop->NetContacts > S.BeforeContacts && !S.Captured) { S.Captured = true; Capture(TEXT("06_NetBallContact.png")); }
        if (Now - S.Start < 1.3) return;
        if (Hoop->Swishes <= S.BeforeScore || Hoop->NetContacts <= S.BeforeContacts || S.Peak < 2 || Hoop->PinError() > .01f)
        { Finish(false, TEXT("Ball did not deform the anchored net and trigger swish")); return; }
        Note(FString::Printf(TEXT("Through-net contact: %d particle contacts, %.2f cm deformation, pins %.4f cm; swish playback"), Hoop->NetContacts - S.BeforeContacts, S.Peak, Hoop->PinError()));
        S.BeforeContacts = Hoop->NetContacts; S.Peak = 0;
        PlaceBall(HouseCourt::Rim(0) + FVector(0, -65, -27), FVector(0, 800, 0)); Next(10); return;
    }
    if (S.Phase == 10)
    {
        S.Peak = FMath::Max(S.Peak, Hoop->NetDisplacement());
        if (Hoop->NetContacts > S.BeforeContacts && !S.Captured) { S.Captured = true; Capture(TEXT("07_NetSideHit.png")); }
        if (Now - S.Start < 1.2) return;
        if (Hoop->NetContacts <= S.BeforeContacts || S.Peak < 3 || Hoop->PinError() > .01f)
        { Finish(false, TEXT("Side contact did not move the net")); return; }
        Note(FString::Printf(TEXT("Side-hit net reaction without a score: %.2f cm, pinned attachments unchanged"), S.Peak));
        S.BeforeScore = Hoop->BoardHits;
        PlaceBall(HouseCourt::Rim(0) + FVector(30, 0, 65), FVector(-850, 0, 0)); Next(11); return;
    }
    if (S.Phase == 11)
    {
        if (Now - S.Start < 1.5) return;
        if (Hoop->BoardHits <= S.BeforeScore) { Finish(false, TEXT("Backboard had no impact event")); return; }
        Note(TEXT("Physical backboard hit and separate thump playback"));
        UAudioMixerBlueprintLibrary::StopRecordingOutput(W, EAudioRecordingExportType::WavFile, TEXT("CourtImpactAudio"), S.Dir);
        Next(12); return;
    }
    if (S.Phase == 12)
    {
        if (Now - S.Start < 3) return;
        if (!FMath::IsFinite(Hoop->NetDisplacement()) || Hoop->NetDisplacement() > 5 || Hoop->PinError() > .01f)
        { Finish(false, TEXT("Net did not settle after contact")); return; }
        Note(FString::Printf(TEXT("Net settled to %.2f cm; audio components spawned %d"), Hoop->NetDisplacement(), Hoop->AudioEvents));
        Capture(TEXT("08_NetSettled.png")); PlaceBall(FVector(0, 2820, -1450)); Next(13); return;
    }
    if (S.Phase == 13)
    {
        if (Now - S.Start < .3) return;
        if (FVector::Dist2D(Ball->Body->GetComponentLocation(), HouseCourt::BallHome()) > 25 || Ball->Body->GetComponentLocation().Z < HouseCourt::Floor)
        { Finish(false, TEXT("Lost ball recovery regressed")); return; }
        Note(TEXT("Same ball recovered; no duplicate spawn"));
        Stand(-380, 1750); S.Way = 0; Next(14); return;
    }
    if (S.Phase == 14)
    {
        const FVector Goals[] = {FVector(-380, 3400, HouseCourt::Floor), FVector(800, 3400, HouseCourt::Floor),
            FVector(800, 2200, HouseCourt::Floor), FVector(-380, 1750, HouseCourt::Floor)};
        const FVector Feet = Pawn->GetActorLocation() - FVector(0, 0, Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        FVector To = Goals[S.Way] - Feet;
        if (To.Size2D() < 25)
        {
            if (++S.Way == 4) { Finish(true, TEXT("Both timed shots, early/late/mis-aim misses, cancel/set-down, rim rebounds/clanks, backboard, net contact/swish/settling, recovery and porch-court walking passed. Multiplayer not tested.")); return; }
            S.Start = Now; return;
        }
        if (Now - S.Start > 20 || FMath::Abs(Feet.Z - HouseCourt::Floor) > 35)
        { Finish(false, TEXT("Court walking route blocked or floor gap")); return; }
        To.Z = 0; Pawn->AddMovementInput(To.GetSafeNormal(), 1); PC->SetControlRotation(To.Rotation());
    }
}
#else
void TickHouseBasketballSkillReview(AStoryFirstPersonCharacter*) {}
#endif
