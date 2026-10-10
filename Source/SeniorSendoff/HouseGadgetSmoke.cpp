// Opt-in (-HouseGadgetSmoke) check that the hallway gadgets work: a pull station sets off the horns,
// a light switch kills the nearby hall lights, the singing bass sings, and the alarm resets itself.
#include "HouseCarryTiersReview.h"
#include "HouseDetail.h"
#include "HouseGadgets.h"
#include "StoryCampaign.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"

namespace
{
struct FHgSmokeState
{
    TWeakObjectPtr<UWorld> World;
    int32 Step = 0; double Started = 0, StepStarted = 0, PulledAt = 0; FString Dir;
    TWeakObjectPtr<AHouseGadget> Pull, Horn, Switch, Bass;
    TArray<TWeakObjectPtr<ULightComponent>> NearLights;
    int32 LitBefore = 0, LitOff = 0, LitAfter = 0, Shots = 0, Fails = 0, StrobeTicks = 0, StrobeLitTicks = 0;
    bool bShotLit = false, bShotDark = false;
    FString Notes;
};

AHouseGadget* HgFind(const TArray<TWeakObjectPtr<AHouseGadget>>& All, EHouseGadgetKind Kind, const FVector* Near = nullptr)
{
    AHouseGadget* Best = nullptr; float BestDist = 1e9f;
    for (const TWeakObjectPtr<AHouseGadget>& W : All)
        if (AHouseGadget* G = W.Get())
            if (G->Kind == Kind)
            {
                const float D = Near ? FVector::Dist(*Near, G->GetActorLocation()) : 0.f;
                if (!Best || D < BestDist) { Best = G; BestDist = D; }
            }
    return Best;
}

/** Stand ~110 cm in front of the gadget on the floor. */
void HgStandAt(AStoryFirstPersonCharacter* Pawn, const AHouseGadget* G, float Dist = 110.f, float Along = 0.f)
{
    UWorld* World = Pawn->GetWorld();
    const FTransform T = G->GetActorTransform();
    const FVector Spot = T.GetLocation() + T.GetUnitAxis(EAxis::X) * Dist + T.GetUnitAxis(EAxis::Y) * Along;
    FHitResult Hit;
    FCollisionQueryParams Q; Q.AddIgnoredActor(Pawn);
    float FloorZ = T.GetLocation().Z - 130.f;
    if (World->LineTraceSingleByChannel(Hit, Spot + FVector(0, 0, 40), Spot - FVector(0, 0, 400), ECC_Visibility, Q)) FloorZ = Hit.Location.Z;
    const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Pawn->SetActorLocation(FVector(Spot.X, Spot.Y, FloorZ + Half + 4.f), false, nullptr, ETeleportType::TeleportPhysics);
}

void HgAimAt(AStoryFirstPersonCharacter* Pawn, APlayerController* PC, const AHouseGadget* G)
{
    const FVector Eye = Pawn->FirstPersonCamera ? Pawn->FirstPersonCamera->GetComponentLocation() : Pawn->GetActorLocation() + FVector(0, 0, 60);
    FVector Target = G->GetActorLocation();
    if (G->Kind == EHouseGadgetKind::PullStation) Target += G->GetActorUpVector() * 7.f;
    PC->SetControlRotation((Target - Eye).Rotation());
}

void HgShot(FHgSmokeState& S, const TCHAR* Name)
{
    FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir, FString::Printf(TEXT("gadget_%s.png"), Name)), false, false);
    ++S.Shots;
}

void HgCheck(FHgSmokeState& S, bool bOk, const FString& What)
{
    if (!bOk) ++S.Fails;
    S.Notes += FString::Printf(TEXT(" %s=%s"), *What, bOk ? TEXT("ok") : TEXT("FAIL"));
    UE_LOG(LogTemp, Display, TEXT("HOUSE_GADGET_SMOKE check %s %s"), *What, bOk ? TEXT("ok") : TEXT("FAIL"));
}

int32 HgCountLit(const TArray<TWeakObjectPtr<ULightComponent>>& Lights)
{
    int32 N = 0;
    for (const TWeakObjectPtr<ULightComponent>& W : Lights) if (const ULightComponent* L = W.Get()) if (L->IsVisible()) ++N;
    return N;
}
}

void TickHouseGadgetSmoke(AStoryFirstPersonCharacter* Pawn)
{
    if (!Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled()) return;
    UWorld* World = Pawn->GetWorld();
    if (!World || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
    if (!PC) return;
    static FHgSmokeState S;
    const double Now = World->GetTimeSeconds();
    if (S.World.Get() != World)
    {
        S = FHgSmokeState(); S.World = World; S.Started = S.StepStarted = Now;
        S.Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HouseGadgetSmoke")));
        IFileManager::Get().MakeDirectory(*S.Dir, true);
    }
    if (S.Step < 0) return;
    const double InStep = Now - S.StepStarted;
    auto Next = [Now]() { ++S.Step; S.StepStarted = Now; };
    UHouseDetailSubsystem* Detail = World->GetSubsystem<UHouseDetailSubsystem>();
    switch (S.Step)
    {
    case 0: // warm up, pick the gadgets
        if (InStep < 5.0 || !Detail) return;
        {
            const TArray<TWeakObjectPtr<AHouseGadget>>& All = Detail->GetGadgets();
            int32 Counts[int32(EHouseGadgetKind::Count)] = { 0 };
            for (const TWeakObjectPtr<AHouseGadget>& W : All) if (const AHouseGadget* G = W.Get()) ++Counts[int32(G->Kind)];
            UE_LOG(LogTemp, Display, TEXT("HOUSE_GADGET_SMOKE gadgets=%d pull=%d horn=%d switch=%d bass=%d"), All.Num(),
                Counts[0], Counts[1], Counts[2], Counts[3]);
            S.Pull = HgFind(All, EHouseGadgetKind::PullStation);
            const FVector PullAt = S.Pull.IsValid() ? S.Pull->GetActorLocation() : FVector::ZeroVector;
            S.Horn = HgFind(All, EHouseGadgetKind::HornStrobe, &PullAt);
            S.Switch = HgFind(All, EHouseGadgetKind::LightSwitch);
            S.Bass = HgFind(All, EHouseGadgetKind::BillyBass);
            HgCheck(S, S.Pull.IsValid() && S.Horn.IsValid() && S.Switch.IsValid() && S.Bass.IsValid(), TEXT("found_all_kinds"));
            if (S.Fails) { S.Step = 99; S.StepStarted = Now; return; }
            HgStandAt(Pawn, S.Pull.Get());
        }
        Next(); return;
    case 1: // aim at the pull station
        if (InStep < 0.4) return;
        HgAimAt(Pawn, PC, S.Pull.Get());
        Next(); return;
    case 2: // focus prompt, screenshot, pull
        if (InStep < 0.4) return;
        HgCheck(S, Pawn->FocusedHouseGadget() == S.Pull.Get(), TEXT("pull_focus"));
        HgShot(S, TEXT("pull_focus"));
        Next(); return;
    case 3:
        if (InStep < 0.3 || FScreenshotRequest::IsScreenshotRequested()) return;
        HgCheck(S, S.Pull->Interact(Pawn), TEXT("pull_interact"));
        S.PulledAt = Now;
        Next(); return;
    case 4: // station pulled, horns on; move to the nearest horn
        if (InStep < 0.8) return;
        HgCheck(S, S.Pull->State == 1, TEXT("pull_state"));
        {
            int32 On = 0, Off = 0;
            for (const TWeakObjectPtr<AHouseGadget>& W : Detail->GetGadgets())
                if (const AHouseGadget* G = W.Get()) if (G->Kind == EHouseGadgetKind::HornStrobe) (G->State ? On : Off)++;
            UE_LOG(LogTemp, Display, TEXT("HOUSE_GADGET_SMOKE horns on=%d off=%d"), On, Off);
            HgCheck(S, On > 0 && Off == 0, TEXT("horns_on"));
        }
        HgShot(S, TEXT("pull_pulled"));
        HgStandAt(Pawn, S.Horn.Get());
        Next(); return;
    case 5:
        if (InStep < 0.4) return;
        HgAimAt(Pawn, PC, S.Horn.Get());
        Next(); return;
    case 6: // one shot while the strobe is lit, one while it is dark; count the duty cycle
        if (FScreenshotRequest::IsScreenshotRequested()) return;
        ++S.StrobeTicks;
        if (S.Horn->bStrobeLit) ++S.StrobeLitTicks;
        if (S.Horn->bStrobeLit && !S.bShotLit) { S.bShotLit = true; HgShot(S, TEXT("horn_flash")); return; }
        if (!S.Horn->bStrobeLit && S.bShotLit && !S.bShotDark) { S.bShotDark = true; HgShot(S, TEXT("horn_dark")); return; }
        if (InStep < 2.2) return;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_GADGET_SMOKE strobe lit=%d of %d ticks"), S.StrobeLitTicks, S.StrobeTicks);
        HgCheck(S, S.bShotLit && S.bShotDark && S.StrobeLitTicks > 0, TEXT("strobe_flashes"));
        Next();
        return;
    case 7: // light switch
        if (InStep < 0.5 || FScreenshotRequest::IsScreenshotRequested()) return;
        HgStandAt(Pawn, S.Switch.Get());
        Next(); return;
    case 8:
        if (InStep < 0.4) return;
        HgAimAt(Pawn, PC, S.Switch.Get());
        {
            S.NearLights.Reset();
            const FVector Here = S.Switch->GetActorLocation();
            for (TObjectIterator<ULightComponent> It; It; ++It)
            {
                ULightComponent* L = *It;
                if (!IsValid(L) || L->GetWorld() != World || !L->IsRegistered() || L->IsA<UDirectionalLightComponent>()) continue;
                const FVector D = L->GetComponentLocation() - Here;
                if (FMath::Abs(D.Z) > 260.f || D.Size2D() > 480.f) continue;
                S.NearLights.Add(L);
            }
            S.LitBefore = HgCountLit(S.NearLights);
        }
        Next(); return;
    case 9:
        if (InStep < 0.4) return;
        HgCheck(S, Pawn->FocusedHouseGadget() == S.Switch.Get(), TEXT("switch_focus"));
        HgShot(S, TEXT("switch_before"));
        HgCheck(S, S.Switch->Interact(Pawn), TEXT("switch_interact"));
        Next(); return;
    case 10:
        if (InStep < 0.6 || FScreenshotRequest::IsScreenshotRequested()) return;
        S.LitOff = HgCountLit(S.NearLights);
        UE_LOG(LogTemp, Display, TEXT("HOUSE_GADGET_SMOKE switch lights near=%d litBefore=%d litOff=%d"), S.NearLights.Num(), S.LitBefore, S.LitOff);
        HgCheck(S, S.Switch->State == 1 && S.LitOff < S.LitBefore, TEXT("switch_off"));
        HgShot(S, TEXT("switch_off"));
        Next(); return;
    case 11:
        if (InStep < 0.5 || FScreenshotRequest::IsScreenshotRequested()) return;
        S.Switch->Interact(Pawn);
        Next(); return;
    case 12:
        if (InStep < 0.5) return;
        S.LitAfter = HgCountLit(S.NearLights);
        HgCheck(S, S.Switch->State == 0 && S.LitAfter == S.LitBefore, TEXT("switch_restored"));
        HgStandAt(Pawn, S.Bass.Get(), 80.f);
        if (PC->MyHUD) PC->MyHUD->bShowHUD = false;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_GADGET_SMOKE bass at %s"), *S.Bass->GetActorLocation().ToString());
        Next(); return;
    case 13:
        if (InStep < 0.4) return;
        HgAimAt(Pawn, PC, S.Bass.Get());
        Next(); return;
    case 14:
        if (InStep < 0.4) return;
        HgCheck(S, Pawn->FocusedHouseGadget() == S.Bass.Get(), TEXT("bass_focus"));
        HgShot(S, TEXT("bass_idle"));
        HgCheck(S, S.Bass->Interact(Pawn), TEXT("bass_interact"));
        Next(); return;
    case 15:
        if (InStep < 1.32 || FScreenshotRequest::IsScreenshotRequested()) return;
        HgCheck(S, S.Bass->State == 1, TEXT("bass_singing"));
        HgShot(S, TEXT("bass_singing"));
        Next(); return;
    case 16:
        if (InStep < 6.0) return;
        HgCheck(S, S.Bass->State == 0, TEXT("bass_reset"));
        HgStandAt(Pawn, S.Bass.Get(), 70.f, 55.f);
        Next(); return;
    case 17:
        if (InStep < 0.4) return;
        HgAimAt(Pawn, PC, S.Bass.Get());
        Next(); return;
    case 18:
        if (InStep < 0.4) return;
        HgShot(S, TEXT("bass_angle"));
        S.Bass->Interact(Pawn);
        Next(); return;
    case 19:
        if (InStep < 1.62 || FScreenshotRequest::IsScreenshotRequested()) return;
        HgShot(S, TEXT("bass_angle_singing"));
        Next(); return;
    case 20:
        if (InStep < 0.5 || FScreenshotRequest::IsScreenshotRequested()) return;
        if (PC->MyHUD) PC->MyHUD->bShowHUD = true;
        HgStandAt(Pawn, S.Bass.Get(), 230.f, -40.f);
        Next(); return;
    case 21:
        if (InStep < 0.4) return;
        HgAimAt(Pawn, PC, S.Bass.Get());
        Next(); return;
    case 22:
        if (InStep < 6.0) return;
        if (PC->MyHUD) PC->MyHUD->bShowHUD = false;
        HgShot(S, TEXT("bass_context"));
        Next(); return;
    case 23: // the alarm resets itself 24 s after the pull
        if (Now - S.PulledAt < 26.0) return;
        {
            int32 On = 0;
            for (const TWeakObjectPtr<AHouseGadget>& W : Detail->GetGadgets())
                if (const AHouseGadget* G = W.Get()) if (G->Kind == EHouseGadgetKind::HornStrobe && G->State) ++On;
            HgCheck(S, On == 0 && S.Pull->State == 0, TEXT("alarm_reset"));
        }
        Next(); return;
    default:
        UE_LOG(LogTemp, Display, TEXT("HOUSE_GADGET_SMOKE_DONE fails=%d shots=%d seconds=%.0f%s"), S.Fails, S.Shots, Now - S.Started, *S.Notes);
        S.Step = -1;
        FPlatformMisc::RequestExitWithStatus(false, S.Fails ? 1 : 0);
        return;
    }
}
