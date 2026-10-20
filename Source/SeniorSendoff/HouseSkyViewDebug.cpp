// TEMPORARY sky view: see HouseSkyViewDebug.h. Delete both files to remove it.
#include "HouseSkyViewDebug.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace HouseSkyViewDebugImpl
{
constexpr double DoubleTapSeconds = .4;   // two Space presses closer than this toggle the view
constexpr float Height = 2600.f;          // cm above the player at zoom 1 ...
constexpr float Back = 1300.f;            // ... and behind: about 63 degrees down at the player
constexpr float BlendSeconds = .4f;
constexpr int32 HintKey = 7731;

bool SmokeRun() { return FParse::Param(FCommandLine::Get(), TEXT("HouseSkyViewSmoke")); }

// Inert in the automated runs: the reviews, smokes and view captures drive the camera (and a few tap Space) themselves.
bool Automated()
{
    if (SmokeRun()) return false;
    const TCHAR* Cmd = FCommandLine::Get();
    return FCString::Stristr(Cmd, TEXT("Review")) != nullptr || FCString::Stristr(Cmd, TEXT("Smoke")) != nullptr
        || FCString::Stristr(Cmd, TEXT("HouseExteriorViews")) != nullptr || FCString::Stristr(Cmd, TEXT("NoSkyView")) != nullptr;
}

// Self-test only: feed a real Space key event through the player's input, like the hallway smoke does.
bool Tap(APlayerController* PC, EInputEvent Event)
{
    if (!PC || !PC->PlayerInput || !FSlateApplication::IsInitialized()) return false;
    PC->InputKey(FInputKeyEventArgs(nullptr, FSlateApplication::Get().GetInputDeviceIdForMouse(), EKeys::SpaceBar, Event, 1.f, false, 0));
    return true;
}
}
namespace Sky = HouseSkyViewDebugImpl;

bool UHouseSkyViewDebugSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHouseSkyViewDebugSubsystem::Deinitialize()
{
    if (IsValid(Camera)) Camera->Destroy();
    Camera = nullptr;
    Super::Deinitialize();
}

void UHouseSkyViewDebugSubsystem::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UWorld* World = GetWorld();
    if (!World || Sky::Automated()) return;
    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC || !PC->IsLocalController() || !PC->PlayerInput) return;
    APawn* Pawn = PC->GetPawn();
    const double Now = FPlatformTime::Seconds();
    if (Sky::SmokeRun()) Smoke(PC, Pawn);

    if (PC->WasInputKeyJustPressed(EKeys::SpaceBar))
    {
        if (Now - LastSpace < Sky::DoubleTapSeconds)
        {
            LastSpace = -100.0;
            if (bUp) Leave(PC, Pawn);
            else if (Pawn && PC->GetViewTarget() == Pawn) Enter(PC, Pawn);
        }
        else LastSpace = Now;
    }
    if (bUp && (!Pawn || !Camera || PC->GetViewTarget() != Camera.Get()))
    {
        // Somebody else took the view (respawn, a scripted camera) or the player is gone: stand down quietly.
        bUp = false;
        LeftAt = Now;
    }
    if (bUp)
    {
        if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) Zoom = FMath::Max(.3f, Zoom / 1.2f);
        if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) Zoom = FMath::Min(4.f, Zoom * 1.2f);
        Place(PC, Pawn, DeltaSeconds);
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(Sky::HintKey, 1.f, FColor(255, 214, 120),
                FString::Printf(TEXT("SKY VIEW (temporary)   %.0f m up   tap Space twice to come back   mouse wheel zooms"), Zoom * Sky::Height / 100.f));
    }
    else if (Camera && Now - LeftAt > Sky::BlendSeconds + .6)
    {
        Camera->Destroy();
        Camera = nullptr;
    }
}

void UHouseSkyViewDebugSubsystem::Enter(APlayerController* PC, APawn* Pawn)
{
    UWorld* World = GetWorld();
    if (!World || !PC || !Pawn) return;
    if (!Camera)
    {
        FActorSpawnParameters P;
        P.ObjectFlags |= RF_Transient;
        P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, P);
        if (!Camera) return;
        if (UCameraComponent* Cam = Camera->GetCameraComponent())
        {
            Cam->bConstrainAspectRatio = false;
            Cam->SetFieldOfView(80.f);
            // Keep the player's exposure and grade so the switch does not change the look of the evening.
            if (const UCameraComponent* Eye = Pawn->FindComponentByClass<UCameraComponent>()) Cam->PostProcessSettings = Eye->PostProcessSettings;
        }
    }
    bUp = true;
    Place(PC, Pawn, -1.f);
    PC->SetViewTargetWithBlend(Camera.Get(), Sky::BlendSeconds, VTBlend_EaseInOut, 2.f);
    UE_LOG(LogTemp, Display, TEXT("HOUSE_SKY_VIEW on at %s"), *Camera->GetActorLocation().ToString());
}

void UHouseSkyViewDebugSubsystem::Leave(APlayerController* PC, APawn* Pawn)
{
    bUp = false;
    LeftAt = FPlatformTime::Seconds();
    if (PC && Pawn) PC->SetViewTargetWithBlend(Pawn, Sky::BlendSeconds, VTBlend_EaseInOut, 2.f);
    else if (PC) PC->SetViewTarget(PC);
    UE_LOG(LogTemp, Display, TEXT("HOUSE_SKY_VIEW off"));
}

void UHouseSkyViewDebugSubsystem::Place(APlayerController* PC, APawn* Pawn, float DeltaSeconds)
{
    if (!Camera || !Pawn || !PC) return;
    const FVector Focus = Pawn->GetActorLocation() + FVector(0.f, 0.f, 60.f);
    const FVector Forward = FRotator(0.f, PC->GetControlRotation().Yaw, 0.f).Vector();
    const FVector Want = Focus - Forward * (Sky::Back * Zoom) + FVector(0.f, 0.f, Sky::Height * Zoom);
    const FVector Eye = DeltaSeconds > 0.f ? FMath::VInterpTo(Camera->GetActorLocation(), Want, DeltaSeconds, 10.f) : Want;
    Camera->SetActorLocationAndRotation(Eye, (Focus - Eye).Rotation());
}

// -HouseSkyViewSmoke: double tap, check the view went up, shoot, double tap again, check it came back, shoot, quit.
void UHouseSkyViewDebugSubsystem::Smoke(APlayerController* PC, APawn* Pawn)
{
    UWorld* World = GetWorld();
    if (!World || !PC) return;
    const double T = World->GetTimeSeconds();
    auto Shot = [](const TCHAR* Name)
    {
        const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HouseSkyView")));
        IFileManager::Get().MakeDirectory(*Dir, true);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(Dir, FString::Printf(TEXT("%s.png"), Name)), false, false);
    };
    auto Check = [this](bool bOk, const FString& What)
    {
        if (!bOk) ++SmokeFails;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_SKY_VIEW_SMOKE %s %s"), *What, bOk ? TEXT("PASS") : TEXT("FAIL"));
    };
    const FVector Eye = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
    const float PawnZ = Pawn ? float(Pawn->GetActorLocation().Z) : 0.f;
    const float Above = float(Eye.Z) - PawnZ;
    switch (SmokeStep)
    {
    case 0: if (T > 6.0) { Sky::Tap(PC, IE_Pressed); ++SmokeStep; } break;     // let the exterior settle first
    case 1: Sky::Tap(PC, IE_Released); ++SmokeStep; break;
    case 2: Sky::Tap(PC, IE_Pressed); ++SmokeStep; break;
    case 3: Sky::Tap(PC, IE_Released); SmokeAt = T; ++SmokeStep; break;
    case 4:
        if (T - SmokeAt > 2.5)
        {
            Check(bUp && Camera && PC->GetViewTarget() == Camera.Get(), TEXT("double tap lifts the view"));
            Check(Above > 1500.f, FString::Printf(TEXT("camera %.0f cm above the player"), Above));
            Shot(TEXT("sky_up"));
            ++SmokeStep;
        }
        break;
    case 5: if (T - SmokeAt > 4.0 && !FScreenshotRequest::IsScreenshotRequested()) { Sky::Tap(PC, IE_Pressed); ++SmokeStep; } break;
    case 6: Sky::Tap(PC, IE_Released); ++SmokeStep; break;
    case 7: Sky::Tap(PC, IE_Pressed); ++SmokeStep; break;
    case 8: Sky::Tap(PC, IE_Released); SmokeAt = T; ++SmokeStep; break;
    case 9:
        if (T - SmokeAt > 2.5)
        {
            Check(!bUp && Pawn && PC->GetViewTarget() == Pawn, TEXT("second double tap returns to the player"));
            Check(Camera == nullptr, TEXT("sky camera cleaned up"));
            Check(Above < 200.f, FString::Printf(TEXT("eye back at the player (%.0f cm above)"), Above));
            Shot(TEXT("sky_down"));
            ++SmokeStep;
        }
        break;
    case 10:
        if (T - SmokeAt > 4.0 && !FScreenshotRequest::IsScreenshotRequested())
        {
            UE_LOG(LogTemp, Display, TEXT("HOUSE_SKY_VIEW_SMOKE %s fails=%d"), SmokeFails ? TEXT("FAIL") : TEXT("PASS"), SmokeFails);
            SmokeAt = T;
            ++SmokeStep;
        }
        break;
    case 11: if (T - SmokeAt > 1.0) { FPlatformMisc::RequestExit(false); ++SmokeStep; } break;
    default: break;
    }
}
