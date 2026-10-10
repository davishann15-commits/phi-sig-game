// Opt-in rendered review of the live Chapter 1 vent network. This never saves the map.
#include "HouseVentNetwork.h"
#include "StoryCampaign.h"
#include "StoryMovementComponent.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UnrealClient.h"

namespace
{
struct FVentView
{
    FString Name;
    FName Portal;
    FIntPoint Cell = FIntPoint::ZeroValue;
    float Yaw = 0.f;
    FVector CameraOffset = FVector::ZeroVector;
    FVector LookOffset = FVector::ZeroVector;
    bool bDuct = false;
    bool bExternal = false;
};

struct FVentReviewState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;
    TWeakObjectPtr<AHouseVentNetwork> Network;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<ACameraActor> Camera;
    TWeakObjectPtr<APointLight> DiagnosticLight;
    TArray<FVentView> Views;
    TMap<TWeakObjectPtr<UPrimitiveComponent>, bool> OriginalOwnerNoSee;
    FString Directory;
    FString PendingScreenshot;
    int32 Index = 0;
    double Started = 0.0;
    double Changed = 0.0;
    double Requested = 0.0;
    uint64 ChangedFrame = 0;
    bool bInitialized = false;
    bool bEntering = false;
    bool bEntered = false;
    bool bTurnTraversed = false;
    int32 TurnPhase = 0;
    double LastTurnProgress = 0.0;
    float BestTurnDistance = BIG_NUMBER;
    int32 MotionSamples = 0;
    double LastMotionSample = 0.0;
    FVector FirstHandR = FVector::ZeroVector;
    FVector FirstHandL = FVector::ZeroVector;
    FVector FirstFootR = FVector::ZeroVector;
    FVector FirstFootL = FVector::ZeroVector;
    int32 OpenFloorPhase = 0;
    double OpenFloorChanged = 0.0;
    bool bFinished = false;
};

void Finish(FVentReviewState& State, bool bPassed, const FString& Reason)
{
    State.bFinished = true;
    UE_LOG(LogTemp, Display, TEXT("HOUSE_VENT_VISUAL_%s captures=%d/%d %s"),
        bPassed ? TEXT("PASSED") : TEXT("FAILED"), State.Index, State.Views.Num(), *Reason);
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

FVector CellCenter(const AHouseVentNetwork* Network, const FIntPoint Cell)
{
    if (!Network) return FVector::ZeroVector;
    const TArray<FIntPoint> Cells = Network->GetRouteCells();
    const TArray<FVector> Centers = Network->GetDuctCellCenters();
    for (int32 Index = 0; Index < Cells.Num() && Index < Centers.Num(); ++Index)
        if (Cells[Index] == Cell) return Centers[Index];
    return FVector::ZeroVector;
}

void SetOwnedBodyVisibility(FVentReviewState& State, UWorld* World,
    const AStoryFirstPersonCharacter* Pawn, bool bExternal)
{
    if (!bExternal)
    {
        for (const auto& Entry : State.OriginalOwnerNoSee)
            if (UPrimitiveComponent* Component = Entry.Key.Get())
                Component->SetOwnerNoSee(Entry.Value);
        State.OriginalOwnerNoSee.Empty();
        return;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor != Pawn && Actor->GetOwner() != Pawn) continue;
        TInlineComponentArray<UPrimitiveComponent*> Components;
        Actor->GetComponents(Components);
        for (UPrimitiveComponent* Component : Components)
            if (Component)
            {
                const TWeakObjectPtr<UPrimitiveComponent> Key(Component);
                if (!State.OriginalOwnerNoSee.Contains(Key))
                    State.OriginalOwnerNoSee.Add(Key, Component->bOwnerNoSee);
                Component->SetOwnerNoSee(false);
            }
    }
}

void ShowDiagnosticDuctMaterial(AHouseVentNetwork* Network)
{
    FString MaterialPath = TEXT("/Game/SeniorSendOff/House/BasementLibrary/M_Lib_SilverMetal.M_Lib_SilverMetal");
    FParse::Value(FCommandLine::Get(), TEXT("HouseVentReviewMaterial="), MaterialPath);
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (!Material) Material = UMaterial::GetDefaultMaterial(MD_Surface);
    TInlineComponentArray<UProceduralMeshComponent*> Meshes;
    Network->GetComponents(Meshes);
    for (UProceduralMeshComponent* Mesh : Meshes)
        if (Mesh && Mesh->GetName().StartsWith(TEXT("VentDuct")))
            for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
                Mesh->SetMaterial(Index, Material);
    UE_LOG(LogTemp, Display, TEXT("HOUSE_VENT_VISUAL_MATERIAL %s"), *Material->GetPathName());
}

void Run(UWorld* World)
{
    if (!World || !World->IsGameWorld()
        || !FParse::Param(FCommandLine::Get(), TEXT("HouseVentVisualReview"))
        || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    const bool bProduction = FParse::Param(FCommandLine::Get(), TEXT("HouseVentVisualProduction"));
    static FVentReviewState State;
    if (State.bFinished) return;
    const double Now = FPlatformTime::Seconds();
    if (State.World.Get() != World)
    {
        State = FVentReviewState();
        State.World = World;
        State.Started = Now;
        State.Directory = FPaths::ConvertRelativePathToFull(
            FPaths::ProjectSavedDir() / TEXT("HouseVentVisualReview"));
        IFileManager::Get().MakeDirectory(*State.Directory, true);
    }
    if (Now - State.Started > 360.0)
    { Finish(State, false, TEXT("Review timed out")); return; }

    if (!State.bInitialized)
    {
        FString IsolatedUserDir;
        if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), IsolatedUserDir)
            || IsolatedUserDir.IsEmpty())
        { Finish(State, false, TEXT("An isolated -UserDir is required")); return; }
        APlayerController* PC = World->GetFirstPlayerController();
        AStoryFirstPersonCharacter* Pawn = PC
            ? Cast<AStoryFirstPersonCharacter>(PC->GetPawn()) : nullptr;
        TActorIterator<AHouseVentNetwork> It(World);
        AHouseVentNetwork* Network = It ? *It : nullptr;
        if (!Pawn || !Network || !Pawn->HasAuthority() || !World->GetGameViewport()) return;
        if (Network->NumActivePortals() != 6)
        {
            if (Now - State.Started < 15.0) return;
            Finish(State, false, TEXT("Six live house facades were not available"));
            return;
        }
        State.Pawn = Pawn;
        State.Network = Network;
        State.Controller = PC;
        State.Camera = World->SpawnActor<ACameraActor>();
        State.DiagnosticLight = World->SpawnActor<APointLight>();
        if (!State.Camera.IsValid() || !State.DiagnosticLight.IsValid())
        { Finish(State, false, TEXT("Could not create temporary review camera and light")); return; }
        State.Camera->GetCameraComponent()->SetFieldOfView(90.f);
        UPointLightComponent* Light = CastChecked<UPointLightComponent>(
            State.DiagnosticLight->GetLightComponent());
        Light->SetIntensity(5500.f);
        Light->SetAttenuationRadius(420.f);
        Light->SetCastShadows(false);
        Light->SetVisibility(false);
        // The actual first-person view at every facade, then both player and
        // external body views inside the connected duct and its 90-degree bend.
        for (const FName Id : Network->GetPortalIds())
        {
            FVentView View;
            View.Name = FString(TEXT("Entrance_")) + Id.ToString();
            View.Portal = Id;
            State.Views.Add(View);
        }
        auto AddDuct = [&](const TCHAR* Name, FIntPoint Cell, float Yaw,
            bool bExternal, FVector CameraOffset, FVector LookOffset)
        {
            FVentView View;
            View.Name = Name;
            View.Cell = Cell;
            View.Yaw = Yaw;
            View.bDuct = true;
            View.bExternal = bExternal;
            View.CameraOffset = CameraOffset;
            View.LookOffset = LookOffset;
            State.Views.Add(View);
        };
        AddDuct(TEXT("Duct_Entry_FirstPerson"), FIntPoint(-6, 0), 0.f,
            false, FVector::ZeroVector, FVector::ZeroVector);
        AddDuct(TEXT("Duct_Straight_FirstPerson"), FIntPoint(0, 0), 0.f,
            false, FVector::ZeroVector, FVector::ZeroVector);
        AddDuct(TEXT("Duct_Straight_Body"), FIntPoint(0, 0), 0.f,
            true, FVector(180.f, 20.f, 34.f), FVector(0.f, 0.f, 0.f));
        AddDuct(TEXT("Duct_Corner_FirstPerson"), FIntPoint(6, 0), 45.f,
            false, FVector::ZeroVector, FVector::ZeroVector);
        AddDuct(TEXT("Duct_Corner_45_Body"), FIntPoint(6, 0), 45.f,
            true, FVector(0.f, 170.f, 34.f), FVector(0.f, 0.f, 0.f));
        AddDuct(TEXT("Duct_Corner_90_Body"), FIntPoint(6, 0), 90.f,
            true, FVector(0.f, 170.f, 34.f), FVector(0.f, 0.f, 0.f));
        AddDuct(TEXT("Duct_AfterTurn_Body"), FIntPoint(6, 1), 90.f,
            true, FVector(0.f, 170.f, 34.f), FVector(0.f, 0.f, 0.f));
        State.bInitialized = true;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_VENT_VISUAL_READY views=%d production=%d output=%s"),
            State.Views.Num(), bProduction, *State.Directory);
    }

    AStoryFirstPersonCharacter* Pawn = State.Pawn.Get();
    AHouseVentNetwork* Network = State.Network.Get();
    APlayerController* PC = State.Controller.Get();
    ACameraActor* Camera = State.Camera.Get();
    APointLight* Light = State.DiagnosticLight.Get();
    if (!Pawn || !Network || !PC || !Camera || !Light)
    { Finish(State, false, TEXT("Review actor was lost")); return; }
    if (State.Index >= State.Views.Num())
    {
        const FName North(TEXT("BlackHallNorthVent"));
        if (State.OpenFloorPhase == 0)
        {
            SetOwnedBodyVisibility(State, World, Pawn, false);
            Light->GetLightComponent()->SetVisibility(false);
            PC->SetViewTarget(Pawn);
            const FVector Center = CellCenter(Network, FIntPoint(6, 0));
            const float HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            const FVector At(Center.X, Center.Y,
                Center.Z - Network->ClearHeight * .5f + HalfHeight + 2.f);
            if (!Pawn->TeleportTo(At, FRotator(0.f, -90.f, 0.f), false, false))
            { Finish(State, false, TEXT("Could not reach north vent exit marker")); return; }
            PC->SetControlRotation(FRotator(0.f, -90.f, 0.f));
            State.OpenFloorPhase = 10;
            State.OpenFloorChanged = Now;
            return;
        }
        if (Now - State.OpenFloorChanged < .45) return;
        if (State.OpenFloorPhase == 10)
        {
            const FName Focused = Network->FindFocusedPortal(Pawn, -.5f);
            if (Focused != North || !Network->Interact(Pawn, North))
            {
                Finish(State, false, FString::Printf(TEXT("North vent exit rejected; focused=%s"),
                    *Focused.ToString()));
                return;
            }
            State.OpenFloorPhase = 1;
            State.OpenFloorChanged = Now;
            return;
        }
        if (State.OpenFloorPhase == 1)
        {
            if (Network->IsInDuct(Pawn) || Pawn->GetStoryMovement()->IsCrawling())
            { Finish(State, false, TEXT("Did not return standing to the black hall")); return; }
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::C, IE_Pressed, 1.f));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::C, IE_Released, 0.f));
            State.OpenFloorPhase = 2;
            State.OpenFloorChanged = Now;
            return;
        }
        if (State.OpenFloorPhase == 2)
        {
            if (!Pawn->GetStoryMovement()->IsCrawling())
            { Finish(State, false, TEXT("C key did not start crawl on open floor")); return; }
            UE_LOG(LogTemp, Display, TEXT("HOUSE_VENT_VISUAL_OPEN_FLOOR_C_STARTED at=%s"),
                *Pawn->GetActorLocation().ToCompactString());
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::C, IE_Pressed, 1.f));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::C, IE_Released, 0.f));
            State.OpenFloorPhase = 3;
            State.OpenFloorChanged = Now;
            return;
        }
        if (Pawn->GetStoryMovement()->IsCrawling())
        { Finish(State, false, TEXT("Second C key did not restore standing")); return; }
        UE_LOG(LogTemp, Display, TEXT("HOUSE_VENT_VISUAL_OPEN_FLOOR_C_PASSED"));
        Finish(State, true, TEXT("Rendered captures, collision turn, and open-floor C crawl passed"));
        return;
    }
    const FVentView& View = State.Views[State.Index];

    if (View.bDuct && !State.bEntered)
    {
        if (!State.bEntering)
        {
            const FName Portal(TEXT("PigstyVent"));
            const FVector Foot = Network->GetPortalRoomApproach(Portal);
            const FVector Facade = Network->GetPortalFacadeLocation(Portal);
            if (Foot.IsNearlyZero())
            { Finish(State, false, TEXT("Pigsty room approach is blocked")); return; }
            Pawn->SetVentCrawlForced(false);
            const FVector At = Foot + FVector(0.f, 0.f,
                Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3.f);
            if (!Pawn->TeleportTo(At, (Facade - At).Rotation(), false, false))
            { Finish(State, false, TEXT("Could not reach pigsty entrance")); return; }
            PC->SetViewTarget(Pawn);
            PC->SetControlRotation((Facade - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
            State.bEntering = true;
            State.Changed = Now;
            return;
        }
        if (Now - State.Changed < .4) return;
        if (!Network->IsInDuct(Pawn))
        {
            if (!Network->Interact(Pawn, TEXT("PigstyVent")))
            { Finish(State, false, TEXT("Live pigsty entrance interaction was rejected")); return; }
            State.Changed = Now;
            return;
        }
        if (!Pawn->GetStoryMovement() || !Pawn->GetStoryMovement()->IsCrawling())
        { Finish(State, false, TEXT("Live entrance did not establish crawling")); return; }
        State.bEntered = true;
        if (!bProduction) ShowDiagnosticDuctMaterial(Network);
        State.Changed = 0.0;
        State.bEntering = false;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_VENT_VISUAL_ENTERED pigsty=%s"),
            *Pawn->GetActorLocation().ToCompactString());
    }

    // Start in the west trunk, crawl to the junction under actual collision,
    // stop for a low-view screenshot, then crawl north through the turn.
    if (State.Index == 9 && !State.bTurnTraversed)
    {
        if (!Network->IsInDuct(Pawn) || !Pawn->GetStoryMovement()
            || !Pawn->GetStoryMovement()->IsCrawling())
        { Finish(State, false, TEXT("Crawler left the duct before the 90-degree turn")); return; }
        const FVector Corner = CellCenter(Network, FIntPoint(6, 0));
        const FVector After = CellCenter(Network, FIntPoint(6, 1));
        if (Corner.IsNearlyZero() || After.IsNearlyZero())
        { Finish(State, false, TEXT("90-degree route cells are missing")); return; }
        if (State.TurnPhase == 0)
        {
            const FVector Before = CellCenter(Network, FIntPoint(5, 0));
            const float HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            const FVector At(Before.X, Before.Y,
                Before.Z - Network->ClearHeight * .5f + HalfHeight + 2.f);
            if (!Pawn->TeleportTo(At, FRotator::ZeroRotator, false, false))
            { Finish(State, false, TEXT("Could not reach the turn approach")); return; }
            SetOwnedBodyVisibility(State, World, Pawn, false);
            PC->SetViewTarget(Pawn);
            PC->SetControlRotation(FRotator::ZeroRotator);
            Light->GetLightComponent()->SetVisibility(!bProduction);
            Light->SetActorLocation(Pawn->GetActorLocation() + FVector(12.f, 0.f, 15.f));
            State.TurnPhase = 1;
            State.LastTurnProgress = Now;
            State.BestTurnDistance = BIG_NUMBER;
            UE_LOG(LogTemp, Display, TEXT("HOUSE_VENT_VISUAL_TURN_START at=%s"),
                *Pawn->GetActorLocation().ToCompactString());
            return;
        }
        if (State.TurnPhase == 1 || State.TurnPhase == 3)
        {
            if (State.MotionSamples < 2 && Pawn->GetVelocity().Size2D() > 4.f
                && Now - State.LastMotionSample >= .22)
                if (USkeletalMeshComponent* Body = Pawn->GetBraxtonBodyMesh())
                {
                    const FTransform Actor = Pawn->GetActorTransform();
                    const FVector HandR = Actor.InverseTransformPosition(Body->GetSocketLocation(TEXT("hand_r")));
                    const FVector HandL = Actor.InverseTransformPosition(Body->GetSocketLocation(TEXT("hand_l")));
                    const FVector FootR = Actor.InverseTransformPosition(Body->GetSocketLocation(TEXT("foot_r")));
                    const FVector FootL = Actor.InverseTransformPosition(Body->GetSocketLocation(TEXT("foot_l")));
                    if (State.MotionSamples == 0)
                    {
                        State.FirstHandR = HandR;
                        State.FirstHandL = HandL;
                        State.FirstFootR = FootR;
                        State.FirstFootL = FootL;
                    }
                    else UE_LOG(LogTemp, Display,
                        TEXT("HOUSE_VENT_VISUAL_MOTION handR=%.2f handL=%.2f footR=%.2f footL=%.2f speed=%.1f"),
                        FVector::Distance(HandR, State.FirstHandR),
                        FVector::Distance(HandL, State.FirstHandL),
                        FVector::Distance(FootR, State.FirstFootR),
                        FVector::Distance(FootL, State.FirstFootL),
                        Pawn->GetVelocity().Size2D());
                    ++State.MotionSamples;
                    State.LastMotionSample = Now;
                }
            const FVector Target = State.TurnPhase == 1 ? Corner : After;
            const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), Target);
            if (Distance < 12.f)
            {
                Pawn->GetCharacterMovement()->StopMovementImmediately();
                if (State.TurnPhase == 1)
                {
                    PC->SetControlRotation(FRotator(0.f, 45.f, 0.f));
                    State.TurnPhase = 2;
                    State.Changed = Now;
                    State.ChangedFrame = GFrameCounter;
                    UE_LOG(LogTemp, Display, TEXT("HOUSE_VENT_VISUAL_TURN_CORNER at=%s"),
                        *Pawn->GetActorLocation().ToCompactString());
                }
                else
                {
                    State.bTurnTraversed = true;
                    State.Changed = 0.0;
                    UE_LOG(LogTemp, Display, TEXT("HOUSE_VENT_VISUAL_TURN_PASSED at=%s"),
                        *Pawn->GetActorLocation().ToCompactString());
                }
                return;
            }
            if (Distance < State.BestTurnDistance - .5f)
            { State.BestTurnDistance = Distance; State.LastTurnProgress = Now; }
            if (Now - State.LastTurnProgress > 5.0)
            {
                Finish(State, false, FString::Printf(TEXT("Blocked during 90-degree turn phase %d distance=%.1f"),
                    State.TurnPhase, Distance));
                return;
            }
            Pawn->AddMovementInput((Target - Pawn->GetActorLocation()).GetSafeNormal2D(), 1.f);
            Light->SetActorLocation(Pawn->GetActorLocation() + FVector(12.f, 0.f, 15.f));
            return;
        }
        if (State.TurnPhase == 2)
        {
            const FString Path = State.Directory / TEXT("Duct_ActualCorner_FirstPerson.png");
            if (State.PendingScreenshot.IsEmpty())
            {
                if (Now - State.Changed < 2.5 || GFrameCounter - State.ChangedFrame < 64) return;
                FScreenshotRequest::RequestScreenshot(Path, false, false);
                State.PendingScreenshot = Path;
                State.Requested = Now;
                return;
            }
            if (IFileManager::Get().FileSize(*State.PendingScreenshot) <= 0)
            {
                if (Now - State.Requested < 15.0) return;
                Finish(State, false, TEXT("Actual-turn screenshot was not saved"));
                return;
            }
            UE_LOG(LogTemp, Display, TEXT("HOUSE_VENT_VISUAL_CAPTURED actualCorner=%s"),
                *State.PendingScreenshot);
            State.PendingScreenshot.Reset();
            State.Changed = 0.0;
            State.TurnPhase = 3;
            State.LastTurnProgress = Now;
            State.BestTurnDistance = BIG_NUMBER;
            PC->SetControlRotation(FRotator(0.f, 90.f, 0.f));
            return;
        }
    }

    if (State.Changed == 0.0)
    {
        SetOwnedBodyVisibility(State, World, Pawn, View.bExternal);
        Light->GetLightComponent()->SetVisibility(View.bDuct && !bProduction);
        if (!View.bDuct)
        {
            const FVector Foot = Network->GetPortalRoomApproach(View.Portal);
            const FVector Facade = Network->GetPortalFacadeLocation(View.Portal);
            if (Foot.IsNearlyZero())
            { Finish(State, false, FString::Printf(TEXT("No clear approach for %s"), *View.Portal.ToString())); return; }
            const FVector At = Foot + FVector(0.f, 0.f,
                Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3.f);
            if (!Pawn->TeleportTo(At, (Facade - At).Rotation(), false, false))
            { Finish(State, false, FString::Printf(TEXT("Could not photograph %s"), *View.Portal.ToString())); return; }
            PC->SetViewTarget(Pawn);
            PC->SetControlRotation((Facade - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        }
        else
        {
            const FVector Center = CellCenter(Network, View.Cell);
            if (Center.IsNearlyZero())
            { Finish(State, false, FString::Printf(TEXT("Route cell missing for %s"), *View.Name)); return; }
            const float HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            const FVector At(Center.X, Center.Y,
                Center.Z - Network->ClearHeight * .5f + HalfHeight + 2.f);
            if (!Pawn->TeleportTo(At, FRotator(0.f, View.Yaw, 0.f), false, false))
            { Finish(State, false, FString::Printf(TEXT("Could not place crawler at %s"), *View.Name)); return; }
            PC->SetControlRotation(FRotator(0.f, View.Yaw, 0.f));
            if (View.bExternal)
            {
                const FVector Cam = Center + View.CameraOffset;
                const FVector Look = Center + View.LookOffset;
                Camera->SetActorLocationAndRotation(Cam, (Look - Cam).Rotation());
                PC->SetViewTarget(Camera);
                Light->SetActorLocation(Cam);
            }
            else
            {
                PC->SetViewTarget(Pawn);
                Light->SetActorLocation(Pawn->GetActorLocation() + FVector(12.f, 0.f, 15.f));
            }
        }
        State.Changed = Now;
        State.ChangedFrame = GFrameCounter;
        UE_LOG(LogTemp, Display,
            TEXT("HOUSE_VENT_VISUAL_VIEW %d %s pawn=%s capsule=(%.1f,%.1f) crawling=%d"),
            State.Index, *View.Name, *Pawn->GetActorLocation().ToCompactString(),
            Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),
            Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
            Pawn->GetStoryMovement() && Pawn->GetStoryMovement()->IsCrawling());
        return;
    }
    if (Now - State.Changed < 2.5 || GFrameCounter - State.ChangedFrame < 64) return;
    if (State.PendingScreenshot.IsEmpty())
    {
        int32 Width = 0, Height = 0;
        PC->GetViewportSize(Width, Height);
        if (Width <= 0 || Height <= 0)
        { Finish(State, false, TEXT("A rendered viewport is required")); return; }
        State.PendingScreenshot = State.Directory / (View.Name + TEXT(".png"));
        FScreenshotRequest::RequestScreenshot(State.PendingScreenshot, false, false);
        State.Requested = Now;
        return;
    }
    if (IFileManager::Get().FileSize(*State.PendingScreenshot) <= 0)
    {
        if (Now - State.Requested < 15.0) return;
        Finish(State, false, FString::Printf(TEXT("Screenshot missing: %s"), *View.Name));
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("HOUSE_VENT_VISUAL_CAPTURED %s path=%s"),
        *View.Name, *State.PendingScreenshot);
    ++State.Index;
    State.PendingScreenshot.Reset();
    State.Changed = 0.0;
}

const FDelegateHandle VentVisualTick = FWorldDelegates::OnWorldPostActorTick.AddLambda(
    [](UWorld* World, ELevelTick, float) { Run(World); });
}
#endif
