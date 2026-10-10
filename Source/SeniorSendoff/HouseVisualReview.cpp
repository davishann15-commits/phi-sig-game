// Opt-in Development campaign render review. It uses registered house cameras,
// an isolated save and user directory; it never changes normal gameplay.
#include "StoryCampaign.h"
#include "ExistingHouseDoor.h"
#include "HouseReactiveFixture.h"
#include "HouseInteractiveProp.h"
#include "HouseWorldScale.h"
#include "Components/SceneComponent.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreGlobals.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
        FString LayoutPath = FPaths::ProjectContentDir() / TEXT("HouseReview/house_walkthrough.json");
        FParse::Value(FCommandLine::Get(), TEXT("HouseVisualLayout="), LayoutPath);
        if (!FFileHelper::LoadFileToString(Source, *LayoutPath)
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Layout)
            || !Layout.IsValid() || !Layout->TryGetArrayField(TEXT("views"), Views) || Views->IsEmpty())
        { Finish(State, false, TEXT("Registered house cameras are missing")); return; }
        FString ViewFilter;
        FParse::Value(FCommandLine::Get(), TEXT("HouseVisualFilter="), ViewFilter);
        if (ViewFilter.IsEmpty()) State.Views = *Views;
        else
        {
            for (const TSharedPtr<FJsonValue>& ViewValue : *Views)
            {
                const TSharedPtr<FJsonObject> View = ViewValue->AsObject();
                FString Name;
                if (View.IsValid() && View->TryGetStringField(TEXT("name"), Name)
                    && Name.Contains(ViewFilter, ESearchCase::IgnoreCase))
                    State.Views.Add(ViewValue);
            }
            if (State.Views.IsEmpty())
            { Finish(State, false, TEXT("Requested view filter matched no registered cameras")); return; }
        }
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
        if (FParse::Param(FCommandLine::Get(), TEXT("HouseLandingCollisionCheck")))
        {
            // Sweep a player-sized capsule across three parts of the new
            // guard. The adjacent stair lane must remain unobstructed by it.
            const float Lanes[] = { 1075.f, 1150.f, 1190.f, 935.f };
            for (int32 Lane = 0; Lane < UE_ARRAY_COUNT(Lanes); ++Lane)
            {
                FHitResult Hit;
                FCollisionQueryParams Params(SCENE_QUERY_STAT(HouseLandingGuardCheck), false);
                Params.AddIgnoredActor(State.Pawn.Get());
                const bool bHit = World->SweepSingleByChannel(Hit,
                    HouseWorldScale::Point(World, FVector(-2090.f, Lanes[Lane], 410.f)),
                    HouseWorldScale::Point(World, FVector(-1940.f, Lanes[Lane], 410.f)), FQuat::Identity,
                    ECC_Pawn, FCollisionShape::MakeCapsule(30.f, 88.f), Params);
                const UStaticMeshComponent* Mesh = bHit ? Cast<UStaticMeshComponent>(Hit.GetComponent()) : nullptr;
                const bool bGuard = Mesh && Mesh->GetStaticMesh()
                    && Mesh->GetStaticMesh()->GetFName() == FName(TEXT("SM_S03_UpperLandingGuard"));
                UE_LOG(LogTemp, Display,
                    TEXT("HOUSE_LANDING_COLLISION: lane=%.0f blocked=%d guard=%d actor=%s impact=%s"),
                    Lanes[Lane], int32(bHit), int32(bGuard),
                    bHit && Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("none"),
                    *Hit.ImpactPoint.ToCompactString());
                if ((Lane < 3 && !bGuard) || (Lane == 3 && bGuard))
                {
                    Finish(State, false, TEXT("Landing guard collision or stair access failed"));
                    return;
                }
            }
        }
        State.Pawn->GetCharacterMovement()->DisableMovement();
        State.Pawn->SetActorEnableCollision(false); // Teleported photo poses must not trigger checkpoints.
        if (FParse::Param(FCommandLine::Get(), TEXT("HouseVisualSourcePose")))
        {
            // Opt-in fit audit: compare every extracted room leaf in its FBX
            // source pose without editing or saving the campaign map. Porch
            // doors retain their built placement for this comparison.
            int32 PreviewedRoomLeaves = 0;
            for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
                if (It->DoorGroup.IsNone())
                {
                    It->ClosedYaw = 0.f;
                    It->OnConstruction(It->GetActorTransform());
                    ++PreviewedRoomLeaves;
                }
            UE_LOG(LogTemp, Display, TEXT("HOUSE_VISUAL_SOURCE_POSE: roomLeaves=%d"), PreviewedRoomLeaves);
        }
        FString PreviewDoorId;
        if (FParse::Value(FCommandLine::Get(), TEXT("HouseVisualPreviewDoor="), PreviewDoorId))
        {
            float PreviewYaw = 0.f;
            if (!FParse::Value(FCommandLine::Get(), TEXT("HouseVisualPreviewYaw="), PreviewYaw)
                || !FMath::IsFinite(PreviewYaw))
            { Finish(State, false, TEXT("Door preview requires a finite yaw")); return; }
            bool bFoundPreviewDoor = false;
            for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
                if (It->DoorId.ToString() == PreviewDoorId && It->DoorGroup.IsNone())
                {
                    const FVector CenterOffset = It->DoorwayCenter - It->GetActorLocation();
                    It->DoorwayCenter = It->GetActorLocation()
                        + FRotator(0.f, PreviewYaw - It->ClosedYaw, 0.f).RotateVector(CenterOffset);
                    It->ClosedYaw = PreviewYaw;
                    It->OnConstruction(It->GetActorTransform());
                    UE_LOG(LogTemp, Display, TEXT("HOUSE_VISUAL_PREVIEW_DOOR: id=%s yaw=%.1f center=%s"),
                        *PreviewDoorId, PreviewYaw, *It->DoorwayCenter.ToCompactString());
                    bFoundPreviewDoor = true;
                    break;
                }
            if (!bFoundPreviewDoor)
            { Finish(State, false, TEXT("Requested room door preview ID was not found")); return; }
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("HouseVisualOpenPorch")))
        {
            // Opt-in visual review of the actual animated porch leaves. Place
            // the collision-disabled reviewer near each pair and use normal
            // interaction, then let the camera warmup finish their swings.
            for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
                if (!It->DoorGroup.IsNone() && !It->IsOpen())
                {
                    State.Pawn->SetActorLocation(It->DoorwayCenter + FVector(0, 220, 0),
                        false, nullptr, ETeleportType::TeleportPhysics);
                    It->Interact(State.Pawn.Get());
                }
        }
        State.Pawn->FirstPersonCamera->bConstrainAspectRatio = false;
        State.Pawn->FirstPersonCamera->bOverrideAspectRatioAxisConstraint = true;
        State.Pawn->FirstPersonCamera->SetAspectRatioAxisConstraint(AspectRatio_MaintainXFOV);
        PC->SetViewTarget(State.Pawn.Get());
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        State.bInitialized = true;
        // Asset preparation can take minutes on a cold house cache. Measure
        // the camera sweep from readiness, after that preparation completes.
        State.Started = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Display, TEXT("HOUSE_VISUAL_REVIEW_READY: views=%d quality=%d pages=%d userDir=%s"),
            State.Views.Num(), State.Quality, CVarInt(TEXT("r.Shadow.Virtual.MaxPhysicalPages")), *FPaths::ProjectUserDir());
    }
    // Targeted runtime gate for the reference porch door, attached curtain and
    // landing guard. Opt-in only; it leaves the saved map and normal play alone.
    if (FParse::Param(FCommandLine::Get(), TEXT("HouseFloorPorchCheck")))
    {
        static int32 Phase = 0;
        static double Since = 0;
        static TWeakObjectPtr<AExistingHouseDoor> Door;
        static TWeakObjectPtr<AHouseReactiveFixture> Curtain;
        static TArray<TWeakObjectPtr<AHouseInteractiveProp>> SeatProps;
        static TArray<FVector> SeatStarts;
        if (Phase == 0)
        {
            for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
                if (It->DoorId == FName(TEXT("SM_OriginalLeaf_Upper_m1_p0_Solid_01"))) Door = *It;
            for (TActorIterator<AHouseReactiveFixture> It(World); It; ++It)
                if (It->FixtureId == FName(TEXT("FrontPorchNavyDoorCurtain"))) Curtain = *It;
            if (!Door.IsValid() || !Curtain.IsValid() || !Door->bLocalLeafSpace
                || Curtain->GetRootComponent()->GetAttachParent() != Door->GetLeafPivot())
            { Finish(State, false, TEXT("Porch door or attached curtain configuration failed")); return; }
            // Native player-sized block on the closed leaf, tested from both sides.
            for (int32 Side = -1; Side <= 1; Side += 2)
            {
                FHitResult Hit;
                FCollisionQueryParams Q(SCENE_QUERY_STAT(HFPClosedDoor), false);
                Q.AddIgnoredActor(State.Pawn.Get()); Q.AddIgnoredActor(Curtain.Get());
                World->SweepSingleByChannel(Hit, Door->DoorwayCenter + FVector(0, Side*130, 0),
                    Door->DoorwayCenter - FVector(0, Side*130, 0), FQuat::Identity,
                    ECC_Pawn, FCollisionShape::MakeCapsule(25, 70), Q);
                if (Hit.GetActor() != Door.Get())
                { Finish(State, false, TEXT("Closed porch door does not block from both sides")); return; }
            }
            State.Pawn->SetActorLocation(Door->DoorwayCenter + FVector(0, 200, 0), false, nullptr, ETeleportType::TeleportPhysics);
            if (!Door->Interact(State.Pawn.Get()))
            { Finish(State, false, TEXT("Native porch door interaction failed")); return; }
            Since = Now; Phase = 1; return;
        }
        if (Phase == 1)
        {
            if (Now-Since < 1.1) return;
            const USceneComponent* Pivot = Door->GetLeafPivot();
            if (!Door->IsOpen() || !FMath::IsNearlyEqual(Pivot->GetRelativeRotation().Yaw, 90.f, .1f)
                || !Curtain->GetActorLocation().Equals(Pivot->GetComponentLocation(), .01f))
            { Finish(State, false, TEXT("Porch swing or curtain hinge alignment failed")); return; }
            FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(HFPOpenDoor), false);
            Q.AddIgnoredActor(State.Pawn.Get()); Q.AddIgnoredActor(Curtain.Get());
            World->SweepSingleByChannel(Hit, Door->DoorwayCenter+FVector(0,130,0),
                Door->DoorwayCenter-FVector(0,130,0), FQuat::Identity,
                ECC_Pawn, FCollisionShape::MakeCapsule(25,70),Q);
            if (Hit.GetActor()==Door.Get())
            { Finish(State, false, TEXT("Open porch leaf still blocks its passage")); return; }
            if (!Curtain->Interact(State.Pawn.Get()))
            { Finish(State, false, TEXT("Curtain interaction failed")); return; }
            Since=Now; Phase=2; return;
        }
        if (Phase == 2)
        {
            if(Now-Since<1.6) return;
            if(!Curtain->IsOpen() || Curtain->GetCurtainProgress()<.95f)
            { Finish(State,false,TEXT("Curtain did not gather")); return; }
            State.Pawn->SetActorLocation(Door->DoorwayCenter+FVector(0,-120,-50),false,nullptr,ETeleportType::TeleportPhysics);
            if(!Curtain->Interact(State.Pawn.Get()))
            { Finish(State,false,TEXT("Gathered curtain cannot be closed")); return; }
            State.Pawn->SetActorLocation(Door->DoorwayCenter+FVector(0,200,0),false,nullptr,ETeleportType::TeleportPhysics);
            Door->Interact(State.Pawn.Get()); Since=Now; Phase=3; return;
        }
        if (Phase == 3)
        {
            if(Now-Since<1.6) return;
            if(Door->IsOpen() || Curtain->IsOpen() || Curtain->GetCurtainProgress()>.05f || !FMath::IsNearlyZero(Door->GetLeafPivot()->GetRelativeRotation().Yaw,.1f))
            { Finish(State,false,TEXT("Porch leaf did not close smoothly to its frame")); return; }
            FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(HFPWindowGuard),false);
            Q.AddIgnoredActor(State.Pawn.Get());
            World->LineTraceSingleByChannel(Hit,HouseWorldScale::Point(World,FVector(-2210,1170,408)),HouseWorldScale::Point(World,FVector(-2210,1270,408)),ECC_Pawn,Q);
            const UStaticMeshComponent* C=Cast<UStaticMeshComponent>(Hit.GetComponent());
            if(!C || !C->GetStaticMesh() || C->GetStaticMesh()->GetFName()!=FName(TEXT("SM_HFP_LandingGuard")))
            { Finish(State,false,TEXT("Landing window guard lacks collision")); return; }
            int32 Furnishings=0;
            for(TActorIterator<AHouseInteractiveProp> It(World);It;++It)
                if(It->IntactMesh && It->IntactMesh->GetName().StartsWith(TEXT("SM_HFP_")))
                {
                    if(!It->DamagedMesh || !It->HeavilyDamagedMesh || !It->BrokenMesh || !It->bMovable || !It->bBreakable)
                    { Finish(State,false,TEXT("Porch furnishing lost its interaction or damage stages")); return; }
                    ++Furnishings;
                    if(It->IntactMesh->GetFName()==FName(TEXT("SM_HFP_Chair")) || It->IntactMesh->GetFName()==FName(TEXT("SM_HFP_SeatCushion")))
                    { SeatProps.Add(*It);SeatStarts.Add(It->GetActorLocation());It->WakeForInteraction(State.Pawn.Get()); }
                }
            if(Furnishings!=3)
            { Finish(State,false,TEXT("Expected porch furniture and loose cushion are missing")); return; }
            Since=Now; Phase=4; return;
        }
        if(Phase==4)
        {
            if(Now-Since<10.0) return;
            if(SeatProps.Num()!=2)
            { Finish(State,false,TEXT("Chair physics targets missing")); return; }
            for(int32 i=0;i<SeatProps.Num();++i)
            {
                if(!SeatProps[i].IsValid() || SeatProps[i]->DamageStage!=0 || FMath::Abs(SeatProps[i]->GetActorLocation().Z-SeatStarts[i].Z)>15.f
                    || SeatProps[i]->Body->GetComponentVelocity().Size()>50.f)
                { Finish(State,false,TEXT("Porch chair or cushion popped upward or failed to settle")); return; }
            }
            UE_LOG(LogTemp,Display,TEXT("HFP_RUNTIME_PASS closed_block_both_sides=1 open_passage=1 curtain_opens_and_closes=1 hinge_attachment=1 guard_blocks=1 interactive_furnishings=3 chair_and_cushion_settle=1"));
            Phase=5;
        }
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
    Position = HouseWorldScale::Point(World, Position);
    Target = HouseWorldScale::Point(World, Target);
    if (State.Changed == 0)
    {
        PC->GetViewportSize(State.Width, State.Height);
        if (State.Width <= 0 || State.Height <= 0)
        { Finish(State, false, TEXT("No rendered viewport (NullRHI is not a valid review)")); return; }
        const float Aspect = float(State.Width) / State.Height;
        State.HorizontalFov = FMath::RadiansToDegrees(2 * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VerticalFov * .5)) * Aspect));
        const FRotator Look = (Target - Position).Rotation();
        State.Pawn->SetActorLocationAndRotation(Position - FVector(0, 0, State.Pawn->BaseEyeHeight * State.Pawn->GetActorScale3D().Z),
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
