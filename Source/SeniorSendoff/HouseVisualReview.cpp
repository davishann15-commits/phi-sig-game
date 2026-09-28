// Opt-in Development campaign render review. It uses registered house cameras,
// an isolated save and user directory; it never changes normal gameplay.
#include "StoryCampaign.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "CoreGlobals.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif

namespace
{
constexpr uint64 MinimumWarmupFrames = 64;
struct FHouseVisualState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;
    TArray<TSharedPtr<FJsonValue>> Views;
    TArray<TSharedPtr<FJsonValue>> Results;
    FString Directory;
    FString PendingScreenshot;
    int32 Index = 0;
    int32 Quality = 1;
    int32 RequestedPages = 0;
    int32 Width = 0, Height = 0;
    double Started = 0, Changed = 0, Requested = 0;
    uint64 ChangedFrame = 0, CapturedWarmupFrames = 0;
    double CapturedWarmupSeconds = 0;
    float HorizontalFov = 0;
    bool bInitialized = false, bFinished = false;
};

int32 CVarInt(const TCHAR* Name)
{
    const IConsoleVariable* Value = IConsoleManager::Get().FindConsoleVariable(Name);
    return Value ? Value->GetInt() : -1;
}

void WriteReview(FHouseVisualState& State, const FString& Status, const FString& Detail)
{
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("status"), Status);
    Report->SetStringField(TEXT("detail"), Detail);
    Report->SetStringField(TEXT("scope"), TEXT("Assisted registered-camera sweep in the actual campaign house; not player traversal or a multiplayer session."));
    Report->SetStringField(TEXT("userDirectory"), FPaths::ProjectUserDir());
    Report->SetNumberField(TEXT("elapsedSeconds"), FPlatformTime::Seconds() - State.Started);
    Report->SetNumberField(TEXT("quality"), State.Quality);
    Report->SetNumberField(TEXT("actualShadowQuality"), CVarInt(TEXT("sg.ShadowQuality")));
    Report->SetNumberField(TEXT("requestedPageOverride"), State.RequestedPages);
    Report->SetNumberField(TEXT("actualPages"), CVarInt(TEXT("r.Shadow.Virtual.MaxPhysicalPages")));
    Report->SetNumberField(TEXT("virtualShadowsEnabled"), CVarInt(TEXT("r.Shadow.Virtual.Enable")));
    Report->SetNumberField(TEXT("expectedViews"), State.Views.Num());
    Report->SetNumberField(TEXT("minimumWarmupFrames"), MinimumWarmupFrames);
    Report->SetArrayField(TEXT("captures"), State.Results);
    FString Text;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text));
    if (!FFileHelper::SaveStringToFile(Text, *(State.Directory / TEXT("review.json"))))
    {
        UE_LOG(LogTemp, Error, TEXT("HOUSE_VISUAL_REVIEW_FAILED: Cannot write report"));
        State.bFinished = true;
        FPlatformMisc::RequestExitWithStatus(false, 1);
    }
}

void Finish(FHouseVisualState& State, bool bPassed, const FString& Detail)
{
    State.bFinished = true;
    WriteReview(State, bPassed ? TEXT("PASS") : TEXT("FAIL"), Detail);
    UE_LOG(LogTemp, Display, TEXT("HOUSE_VISUAL_REVIEW_%s: views=%d pages=%d %s"),
        bPassed ? TEXT("PASSED") : TEXT("FAILED"), State.Results.Num(),
        CVarInt(TEXT("r.Shadow.Virtual.MaxPhysicalPages")), *Detail);
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

bool Point(const TSharedPtr<FJsonObject>& View, const TCHAR* Field, FVector& Out)
{
    const TSharedPtr<FJsonObject>* P = nullptr;
    double X = 0, Y = 0, Z = 0;
    if (!View->TryGetObjectField(Field, P) || !P || !P->IsValid()
        || !(*P)->TryGetNumberField(TEXT("x"), X)
        || !(*P)->TryGetNumberField(TEXT("y"), Y)
        || !(*P)->TryGetNumberField(TEXT("z"), Z)) return false;
    Out = FVector(Z * 100, X * 100, Y * 100);
    return !Out.ContainsNaN();
}
}

void TickHouseVisualReview(UWorld* World)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("HouseVisualReview"))) return;
    static FHouseVisualState State;
    if (!World || State.bFinished) return;
    const double Now = FPlatformTime::Seconds();
    if (State.World.Get() != World)
    {
        State = FHouseVisualState();
        State.World = World;
        State.Started = Now;
        State.Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("HouseVisualReview"));
        IFileManager::Get().MakeDirectory(*State.Directory, true);
    }
    if (Now - State.Started > 600)
    { Finish(State, false, TEXT("Campaign render sweep timed out")); return; }
    if (!State.bInitialized)
    {
        // Require native path isolation rather than restoring settings over a
        // concurrently running user's game. The runner supplies a fresh UserDir.
        FString IsolatedUserDir;
        if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), IsolatedUserDir) || IsolatedUserDir.IsEmpty())
        { Finish(State, false, TEXT("Review requires an explicit isolated -UserDir")); return; }
        if (UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House"))
        { Finish(State, false, TEXT("Review requires the actual campaign house")); return; }
        APlayerController* PC = World->GetFirstPlayerController();
        if (!PC || !PC->GetPawn() || !World->GetGameViewport()) return;
        FString Source;
        TSharedPtr<FJsonObject> Layout;
        const TArray<TSharedPtr<FJsonValue>>* Views = nullptr;
        if (!FFileHelper::LoadFileToString(Source, *(FPaths::ProjectContentDir() / TEXT("HouseReview/house_walkthrough.json")))
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Layout)
            || !Layout.IsValid() || !Layout->TryGetArrayField(TEXT("views"), Views) || Views->IsEmpty())
        { Finish(State, false, TEXT("Registered house cameras are missing")); return; }
        State.Views = *Views;
        FParse::Value(FCommandLine::Get(), TEXT("HouseVisualQuality="), State.Quality);
        FParse::Value(FCommandLine::Get(), TEXT("HouseVisualPages="), State.RequestedPages);
        if (State.Quality < 0 || State.Quality > 3 || State.RequestedPages < 0 || State.RequestedPages > 8192)
        { Finish(State, false, TEXT("Invalid review quality or page budget")); return; }
        if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
        {
            Settings->SetOverallScalabilityLevel(State.Quality);
            Settings->SetResolutionScaleValueEx(100);
            Settings->ApplyNonResolutionSettings();
        }
        if (State.RequestedPages)
            IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.MaxPhysicalPages"))->Set(State.RequestedPages, ECVF_SetByConsole);
#if WITH_EDITOR
        FAssetCompilingManager::Get().FinishAllCompilation();
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
#endif
        State.Pawn = Cast<AStoryFirstPersonCharacter>(PC->GetPawn());
        if (!State.Pawn.IsValid() || !State.Pawn->FirstPersonCamera)
        { Finish(State, false, TEXT("Actual story first-person camera is unavailable")); return; }
        // Keep the real view owner, character and held weapon. A detached
        // camera exposes owner-hidden third-person head/hair at the first pose.
        State.Pawn->GetCharacterMovement()->DisableMovement();
        State.Pawn->SetActorEnableCollision(false); // Teleported photo poses must not trigger checkpoints.
        State.Pawn->FirstPersonCamera->bConstrainAspectRatio = false;
        State.Pawn->FirstPersonCamera->bOverrideAspectRatioAxisConstraint = true;
        State.Pawn->FirstPersonCamera->SetAspectRatioAxisConstraint(AspectRatio_MaintainXFOV);
        PC->SetViewTarget(State.Pawn.Get());
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        State.bInitialized = true;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_VISUAL_REVIEW_READY: views=%d quality=%d pages=%d userDir=%s"),
            State.Views.Num(), State.Quality, CVarInt(TEXT("r.Shadow.Virtual.MaxPhysicalPages")), *FPaths::ProjectUserDir());
    }
    if (!State.Views.IsValidIndex(State.Index))
    { Finish(State, State.Results.Num() == State.Views.Num(), TEXT("Registered campaign camera sweep complete")); return; }
    APlayerController* PC = World->GetFirstPlayerController();
    const TSharedPtr<FJsonObject> View = State.Views[State.Index]->AsObject();
    FString Name;
    double VerticalFov = 0;
    FVector Position, Target;
    if (!View.IsValid() || !View->TryGetStringField(TEXT("name"), Name)
        || Name.IsEmpty() || FPaths::GetCleanFilename(Name) != Name
        || !Point(View, TEXT("position"), Position) || !Point(View, TEXT("target"), Target)
        || !View->TryGetNumberField(TEXT("fieldOfView"), VerticalFov)
        || !FMath::IsFinite(VerticalFov) || VerticalFov <= 1 || VerticalFov >= 179
        || !PC || !State.Pawn.IsValid())
    { Finish(State, false, TEXT("Invalid camera definition or native player viewport")); return; }
    if (State.Changed == 0)
    {
        PC->GetViewportSize(State.Width, State.Height);
        if (State.Width <= 0 || State.Height <= 0)
        { Finish(State, false, TEXT("No rendered viewport (NullRHI is not a valid review)")); return; }
        const float Aspect = float(State.Width) / State.Height;
        State.HorizontalFov = FMath::RadiansToDegrees(2 * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VerticalFov * .5)) * Aspect));
        const FRotator Look = (Target - Position).Rotation();
        State.Pawn->SetActorLocationAndRotation(Position - FVector(0, 0, State.Pawn->BaseEyeHeight),
            FRotator(0, Look.Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
        PC->SetControlRotation(Look);
        State.Pawn->FirstPersonCamera->SetAspectRatio(Aspect);
        State.Pawn->FirstPersonCamera->SetFieldOfView(State.HorizontalFov);
        State.PendingScreenshot.Reset();
        // Compilation above may have blocked this tick. Start the wait when
        // the camera is actually placed, rather than using its earlier clock.
        State.Changed = FPlatformTime::Seconds();
        State.ChangedFrame = GFrameCounter;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_VISUAL_REVIEW_VIEW: %d %s"), State.Index, *Name);
        return;
    }
    if (Now - State.Changed < 2.5 || GFrameCounter - State.ChangedFrame < MinimumWarmupFrames) return;
    if (State.PendingScreenshot.IsEmpty())
    {
        int32 Width = 0, Height = 0;
        PC->GetViewportSize(Width, Height);
        if (Width != State.Width || Height != State.Height)
        { State.Changed = 0; return; }
        State.PendingScreenshot = State.Directory / (Name + TEXT(".png"));
        if (!IFileManager::Get().Delete(*State.PendingScreenshot, false, true))
        { Finish(State, false, TEXT("Cannot clear previous screenshot")); return; }
        State.CapturedWarmupFrames = GFrameCounter - State.ChangedFrame;
        State.CapturedWarmupSeconds = FPlatformTime::Seconds() - State.Changed;
        FScreenshotRequest::RequestScreenshot(State.PendingScreenshot, false, false);
        State.Requested = Now;
        return;
    }
    if (Now - State.Requested < .5) return;
    if (FVector::Distance(State.Pawn->FirstPersonCamera->GetComponentLocation(), Position) > .05f
        || !FMath::IsNearlyEqual(State.Pawn->FirstPersonCamera->FieldOfView, State.HorizontalFov, .01f))
    { Finish(State, false, TEXT("Actual first-person view drifted from its registered review pose")); return; }
    if (IFileManager::Get().FileSize(*State.PendingScreenshot) <= 0)
    {
        if (Now - State.Requested < 15) return;
        Finish(State, false, TEXT("Native screenshot did not complete")); return;
    }
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("view"), Name);
    Result->SetStringField(TEXT("path"), State.PendingScreenshot);
    Result->SetNumberField(TEXT("width"), State.Width);
    Result->SetNumberField(TEXT("height"), State.Height);
    Result->SetNumberField(TEXT("horizontalFov"), State.HorizontalFov);
    Result->SetNumberField(TEXT("actualPages"), CVarInt(TEXT("r.Shadow.Virtual.MaxPhysicalPages")));
    Result->SetNumberField(TEXT("warmupFrames"), State.CapturedWarmupFrames);
    Result->SetNumberField(TEXT("warmupSeconds"), State.CapturedWarmupSeconds);
    State.Results.Add(MakeShared<FJsonValueObject>(Result));
    ++State.Index;
    State.Changed = 0;
    WriteReview(State, TEXT("RUNNING"), Name);
}
#endif
