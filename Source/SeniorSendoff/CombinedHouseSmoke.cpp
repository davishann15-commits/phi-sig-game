// Development-only, opt-in integration check for the combined campaign house.
// UStoryCampaign must select an isolated save slot for -CombinedHouseSmoke before
// this GameMode tick hook runs; this file never writes a save directly.
#include "StoryCampaign.h"
#include "SeniorLobby.h"

#if !UE_BUILD_SHIPPING
#include "HouseFurnitureInteractionComponent.h"
#include "StoryMovementComponent.h"
#include "SeniorPlayerPreferences.h"
#include "EnhancedPlayerInput.h"
#include "InputActionValue.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
enum class ECombinedHousePhase : uint8
{
    WaitingForPawn,
    MouseLook,
    Sprint,
    Slide,
    Air,
    Grab,
    Drag,
    Finished
};

struct FCombinedHouseState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;
    TWeakObjectPtr<UPrimitiveComponent> Furniture;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> FurnitureCandidates;
    ECombinedHousePhase Phase = ECombinedHousePhase::WaitingForPawn;
    double StartedAt = 0.0;
    double PhaseStartedAt = 0.0;
    FVector Direction = FVector::ForwardVector;
    FVector SprintStart = FVector::ZeroVector;
    FVector FurnitureStart = FVector::ZeroVector;
    FVector DragDirection = FVector::ZeroVector;
    float SprintPeak = 0.0f;
    float MouseStartYaw = 0.0f;
    float MouseStartPitch = 0.0f;
    int32 MouseCheckStep = 0;
    bool bOriginalInvertY = false;
    float SlideEntrySpeed = 0.0f;
    FVector SlideStart = FVector::ZeroVector;
    float StandingEyeZ = 0.0f;
    float PreJumpSpeed = 0.0f;
    float LowestAirSpeed = BIG_NUMBER;
    bool bSawSlide = false;
    int32 SlideSample = -1;
    bool bSawFalling = false;
    bool bGrabViewAligned = false;
};

void FinishSmoke(FCombinedHouseState& State, bool bPassed, const FString& Detail)
{
    State.Phase = ECombinedHousePhase::Finished;
    if (bPassed) {
        UE_LOG(LogTemp, Display, TEXT("COMBINED_HOUSE_SMOKE_PASSED: %s"), *Detail);
    } else {
        UE_LOG(LogTemp, Error, TEXT("COMBINED_HOUSE_SMOKE_FAILED: %s"), *Detail);
    }
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

bool ReadRunway(FVector& OutStartFeet, FVector& OutDirection)
{
    const FString Path = FPaths::Combine(FPaths::ProjectContentDir(),
        TEXT("HouseReview/house_walkthrough.json"));
    FString Source;
    if (!FFileHelper::LoadFileToString(Source, *Path)) return false;

    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Root) || !Root.IsValid()) return false;
    const TArray<TSharedPtr<FJsonValue>>* Routes = nullptr;
    if (!Root->TryGetArrayField(TEXT("routes"), Routes)) return false;

    auto ReadPoint = [](const TSharedPtr<FJsonValue>& Value, FVector& Out) -> bool
    {
        const TSharedPtr<FJsonObject> Point = Value.IsValid() ? Value->AsObject() : nullptr;
        double X = 0.0, Y = 0.0, Z = 0.0;
        if (!Point.IsValid() || !Point->TryGetNumberField(TEXT("x"), X)
            || !Point->TryGetNumberField(TEXT("y"), Y)
            || !Point->TryGetNumberField(TEXT("z"), Z)) return false;
        // The canonical layout is already at playable scale: source X/Z/Y
        // becomes Unreal Y/X/Z, with metres converted to centimetres.
        Out = FVector(Z * 100.0, X * 100.0, Y * 100.0);
        return !Out.ContainsNaN();
    };

    for (const TSharedPtr<FJsonValue>& Value : *Routes)
    {
        const TSharedPtr<FJsonObject> Route = Value.IsValid() ? Value->AsObject() : nullptr;
        FString Id;
        if (!Route.IsValid() || !Route->TryGetStringField(TEXT("id"), Id)
            || Id != TEXT("LongHall_to_LibraryStair")) continue;
        const TArray<TSharedPtr<FJsonValue>>* Waypoints = nullptr;
        if (!Route->TryGetArrayField(TEXT("waypoints"), Waypoints) || Waypoints->Num() < 2) return false;
        FVector End;
        if (!ReadPoint((*Waypoints)[0], OutStartFeet) || !ReadPoint(Waypoints->Last(), End)) return false;
        OutDirection = End - OutStartFeet;
        OutDirection.Z = 0.0;
        OutDirection.Normalize();
        return !OutDirection.IsNearlyZero() && FVector::Dist2D(OutStartFeet, End) >= 900.0;
    }
    return false;
}

bool PlacePlayerBesideFurniture(UWorld* World, AStoryFirstPersonCharacter* Pawn,
    const TArray<TWeakObjectPtr<UPrimitiveComponent>>& Candidates,
    UPrimitiveComponent*& OutBody, FVector& OutAwayDirection)
{
    if (!World || !Pawn || !Pawn->GetCapsuleComponent()) return false;
    const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    constexpr float StandDistance = 190.0f;

    for (const TWeakObjectPtr<UPrimitiveComponent>& Candidate : Candidates)
    {
        UPrimitiveComponent* Body = Candidate.Get();
        AActor* FurnitureActor = Body ? Body->GetOwner() : nullptr;
        if (!Body || !FurnitureActor || !Body->IsSimulatingPhysics()) continue;
        const FVector Target = Body->Bounds.Origin;
        for (int32 Index = 0; Index < 12; ++Index)
        {
            const float Angle = 2.0f * PI * static_cast<float>(Index) / 12.0f;
            const FVector Away(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
            const FVector GroundProbe = Target + Away * StandDistance;
            FCollisionQueryParams FloorQuery;
            FloorQuery.AddIgnoredActor(Pawn);
            FloorQuery.AddIgnoredActor(FurnitureActor);
            FHitResult Floor;
            if (!World->LineTraceSingleByChannel(Floor, GroundProbe + FVector(0.0, 0.0, 180.0),
                GroundProbe - FVector(0.0, 0.0, 240.0), ECC_Visibility, FloorQuery)
                || Floor.ImpactNormal.Z < 0.65f) continue;

            const FVector StandCenter = Floor.ImpactPoint + FVector(0.0, 0.0, HalfHeight + 5.0f);
            FCollisionQueryParams SpaceQuery;
            SpaceQuery.AddIgnoredActor(Pawn);
            if (World->OverlapBlockingTestByChannel(StandCenter, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Radius, HalfHeight), SpaceQuery)) continue;

            // Only choose a position from which the actual player trace can
            // reach this body, including any real walls or other furniture.
            const FVector PredictedView = StandCenter + FVector(0.0, 0.0, 64.0f);
            FCollisionQueryParams ViewQuery;
            ViewQuery.AddIgnoredActor(Pawn);
            FHitResult ViewHit;
            if (!World->LineTraceSingleByChannel(ViewHit, PredictedView, Target,
                ECC_Visibility, ViewQuery) || ViewHit.GetComponent() != Body) continue;

            if (!Pawn->SetActorLocation(StandCenter, false, nullptr, ETeleportType::TeleportPhysics)) continue;
            OutBody = Body;
            OutAwayDirection = Away;
            return true;
        }
    }
    return false;
}
}

void TickCombinedHouseSmoke(UWorld* World)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("CombinedHouseSmoke"))) return;
    static FCombinedHouseState State;
    if (!World || State.Phase == ECombinedHousePhase::Finished) return;
    const double Now = FPlatformTime::Seconds();
    if (State.World.Get() != World)
    {
        State = FCombinedHouseState();
        State.World = World;
        State.StartedAt = Now;
        State.PhaseStartedAt = Now;
    }
    if (Now - State.StartedAt > 30.0)
    {
        FinishSmoke(State, false, TEXT("Timed out during combined house movement and furniture check"));
        return;
    }
    if (UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House"))
    {
        FinishSmoke(State, false, TEXT("Expected the Chapter01_House map"));
        return;
    }
    if (!World->GetAuthGameMode())
    {
        FinishSmoke(State, false, TEXT("Smoke must run on the authoritative GameMode"));
        return;
    }

    if (State.Phase == ECombinedHousePhase::WaitingForPawn)
    {
        APlayerController* Controller = World->GetFirstPlayerController();
        AStoryFirstPersonCharacter* Pawn = Controller ? Cast<AStoryFirstPersonCharacter>(Controller->GetPawn()) : nullptr;
        UStoryMovementComponent* Movement = Pawn ? Pawn->GetStoryMovement() : nullptr;
        if (!Pawn || !Movement)
        {
            if (Now - State.PhaseStartedAt > 8.0)
                FinishSmoke(State, false, TEXT("Story first-person pawn or StoryMovementComponent did not spawn"));
            return;
        }
        if (!Pawn->GetIsReplicated() || !Pawn->FurnitureInteraction)
        {
            FinishSmoke(State, false, TEXT("Story pawn is missing replication or furniture interaction"));
            return;
        }
        UE_LOG(LogTemp, Display, TEXT("COMBINED_HOUSE_SPAWN_VIEW location=%s pawnYaw=%.1f control=%s view=%s"),
            *Pawn->GetActorLocation().ToString(), Pawn->GetActorRotation().Yaw,
            *Controller->GetControlRotation().ToString(), *Pawn->GetPawnViewLocation().ToString());
        int32 MouseMappings = 0;
        if (Pawn->InputContext && Pawn->MouseLookAction)
            for (const FEnhancedActionKeyMapping& Mapping : Pawn->InputContext->GetMappings())
                if (Mapping.Action == Pawn->MouseLookAction && Mapping.Key == EKeys::Mouse2D)
                {
                    ++MouseMappings;
                    if (!Mapping.Modifiers.IsEmpty())
                    {
                        FinishSmoke(State, false, TEXT("Mouse2D has a mapping modifier that can invert the saved Y preference twice"));
                        return;
                    }
                }
        if (MouseMappings != 1)
        {
            FinishSmoke(State, false, FString::Printf(TEXT("Expected one saved Mouse2D look mapping; found %d"), MouseMappings));
            return;
        }
        if (ASeniorLobbyController* LobbyController = Cast<ASeniorLobbyController>(Controller);
            LobbyController && LobbyController->IsLocalController() && World->GetGameViewport())
        {
            LobbyController->TogglePauseMenu();
            const bool bOpened = LobbyController->IsPauseMenuOpen() && LobbyController->bShowMouseCursor;
            LobbyController->TogglePauseMenu();
            if (!bOpened || LobbyController->IsPauseMenuOpen() || LobbyController->bShowMouseCursor)
            {
                FinishSmoke(State, false, TEXT("Gameplay pause/settings did not open and recapture the mouse"));
                return;
            }
        }

        FString RequestedFurniture;
        FParse::Value(FCommandLine::Get(), TEXT("CombinedHouseFurniture="), RequestedFurniture);
        int32 ValidFurniture = 0;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor || !Actor->ActorHasTag(TEXT("SSOMovableFurniture"))) continue;
            UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
            if (Body && Body->IsSimulatingPhysics() && Actor->GetIsReplicated() && Actor->IsReplicatingMovement())
            {
                ++ValidFurniture;
                // A furniture repair can request its actual tagged body. Keep
                // the shared population check, floor/trace checks and player
                // interaction rather than substituting a synthetic physics box.
                if (!RequestedFurniture.IsEmpty())
                {
                    if (Actor->ActorHasTag(FName(*RequestedFurniture)))
                        State.FurnitureCandidates.Add(Body);
                    continue;
                }
                // Prefer the entrance-floor chair, then try the other scanned
                // pieces if its reachable side happens to be obstructed.
                if (Actor->ActorHasTag(TEXT("GR_ClubChair1")))
                    State.FurnitureCandidates.Insert(Body, 0);
                else
                    State.FurnitureCandidates.Add(Body);
            }
        }
        if (ValidFurniture < 4)
        {
            FinishSmoke(State, false, FString::Printf(TEXT("Expected four tagged, simulated, replicated furniture bodies; found %d"), ValidFurniture));
            return;
        }
        if (State.FurnitureCandidates.IsEmpty())
        {
            FinishSmoke(State, false, FString::Printf(TEXT("Requested tagged furniture body is absent: %s"), *RequestedFurniture));
            return;
        }

        FVector StartFeet;
        if (!ReadRunway(StartFeet, State.Direction))
        {
            FinishSmoke(State, false, TEXT("Measured long-hall runway is absent or too short in house_walkthrough.json"));
            return;
        }
        const float HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const FVector StartCenter = StartFeet + FVector(0.0, 0.0, HalfHeight + 5.0);
        FCollisionQueryParams FloorQuery;
        FloorQuery.AddIgnoredActor(Pawn);
        FHitResult Floor;
        if (!World->LineTraceSingleByChannel(Floor, StartCenter,
            StartFeet - FVector(0.0, 0.0, 60.0), ECC_Visibility, FloorQuery)
            || Floor.ImpactNormal.Z < 0.65f || FMath::Abs(Floor.ImpactPoint.Z - StartFeet.Z) > 35.0f)
        {
            FinishSmoke(State, false, TEXT("Measured long-hall start has no walkable floor"));
            return;
        }
        if (!Pawn->SetActorLocation(StartCenter, false, nullptr, ETeleportType::TeleportPhysics))
        {
            FinishSmoke(State, false, TEXT("Could not place story pawn at measured long-hall start"));
            return;
        }
        const FRotator RunwayRotation = State.Direction.Rotation();
        Pawn->SetActorRotation(RunwayRotation);
        Controller->SetControlRotation(RunwayRotation);
        Movement->StopMovementImmediately();
        Movement->SetMovementMode(MOVE_Walking);
        Movement->SetSlideHeld(false);
        Movement->SetSprintHeld(false);
        World->GetAuthGameMode()->SetActorTickInterval(0.0f); // Drive input every movement frame.
        State.Pawn = Pawn;
        State.SprintStart = Pawn->GetActorLocation();
        State.MouseStartYaw = Controller->GetControlRotation().Yaw;
        State.bOriginalInvertY = SeniorPlayerPreferences::Get().bInvertY;
        State.MouseCheckStep = 0;
        State.Phase = ECombinedHousePhase::MouseLook;
        State.PhaseStartedAt = Now;
        UE_LOG(LogTemp, Display, TEXT("COMBINED_HOUSE_SMOKE: Mouse mapping, pause recapture, four furniture bodies and measured runway found"));
        return;
    }

    AStoryFirstPersonCharacter* Pawn = State.Pawn.Get();
    if (State.Phase == ECombinedHousePhase::MouseLook)
    {
        APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
        UEnhancedPlayerInput* Input = Controller ? Cast<UEnhancedPlayerInput>(Controller->PlayerInput) : nullptr;
        if (!Controller || !Input || !Pawn->MouseLookAction)
        {
            FinishSmoke(State, false, TEXT("Local Enhanced Input mouse-look path is unavailable"));
            return;
        }
        if (State.MouseCheckStep == 0)
        {
            const float Turned=FMath::Abs(FMath::FindDeltaAngleDegrees(
                State.MouseStartYaw,Controller->GetControlRotation().Yaw));
            if (Turned < .2f)
                Input->InjectInputForAction(Pawn->MouseLookAction,FInputActionValue(FVector2D(8,0)));
            else
            {
                Controller->SetControlRotation(State.Direction.Rotation());
                SeniorPlayerPreferences::SetInvertY(false);
                State.MouseStartPitch=Controller->GetControlRotation().Pitch;
                State.MouseCheckStep=1;
            }
            return;
        }
        const float DeltaPitch=FMath::FindDeltaAngleDegrees(
            State.MouseStartPitch,Controller->GetControlRotation().Pitch);
        if (State.MouseCheckStep==1 && DeltaPitch>=.2f)
        {
            Controller->SetControlRotation(State.Direction.Rotation());
            SeniorPlayerPreferences::SetInvertY(true);
            State.MouseStartPitch=Controller->GetControlRotation().Pitch;
            State.MouseCheckStep=2;
            return;
        }
        if (State.MouseCheckStep==2 && DeltaPitch<=-.2f)
        {
            SeniorPlayerPreferences::SetInvertY(State.bOriginalInvertY);
            Controller->SetControlRotation(State.Direction.Rotation());
            Pawn->SetActorRotation(State.Direction.Rotation());
            Pawn->GetStoryMovement()->SetSprintHeld(true);
            State.SprintStart=Pawn->GetActorLocation();
            State.Phase=ECombinedHousePhase::Sprint;
            State.PhaseStartedAt=Now;
            UE_LOG(LogTemp,Display,TEXT("COMBINED_HOUSE_SMOKE: Mouse yaw and both Y directions verified"));
            return;
        }
        if (Now-State.PhaseStartedAt>2.5)
        {
            SeniorPlayerPreferences::SetInvertY(State.bOriginalInvertY);
            FinishSmoke(State,false,FString::Printf(TEXT("Mouse look direction failed (step %d, pitch %.2f)"),State.MouseCheckStep,DeltaPitch));
            return;
        }
        Input->InjectInputForAction(Pawn->MouseLookAction,FInputActionValue(FVector2D(0,8)));
        return;
    }
    UStoryMovementComponent* Movement = Pawn ? Pawn->GetStoryMovement() : nullptr;
    if (!Pawn || !Movement)
    {
        FinishSmoke(State, false, TEXT("Story pawn or movement component disappeared during smoke"));
        return;
    }
    const double PhaseTime = Now - State.PhaseStartedAt;
    const float HorizontalSpeed = Movement->GetHorizontalSpeed();

    switch (State.Phase)
    {
    case ECombinedHousePhase::Sprint:
        Pawn->AddMovementInput(State.Direction, 1.0f, true);
        State.SprintPeak = FMath::Max(State.SprintPeak, HorizontalSpeed);
        if (HorizontalSpeed >= FMath::Max(600.0f, Movement->SprintSpeed * 0.8f)
            && PhaseTime >= 0.35 && FVector::Dist2D(State.SprintStart, Pawn->GetActorLocation()) >= 180.0f)
        {
            State.SlideEntrySpeed = HorizontalSpeed;
            State.SlideStart = Pawn->GetActorLocation();
            State.StandingEyeZ = Pawn->FirstPersonCamera->GetComponentLocation().Z;
            Movement->SetSprintHeld(false);
            Movement->SetSlideHeld(true);
            State.Phase = ECombinedHousePhase::Slide;
            State.PhaseStartedAt = Now;
            UE_LOG(LogTemp, Display, TEXT("COMBINED_HOUSE_SMOKE: Sprint reached %.0f cm/s"), State.SprintPeak);
        }
        else if (PhaseTime > 1.5)
            FinishSmoke(State, false, FString::Printf(TEXT("Sprint failed to gain speed/ground distance (peak %.0f cm/s)"), State.SprintPeak));
        break;

    case ECombinedHousePhase::Slide:
        Pawn->AddMovementInput(State.Direction, 0.25f, true);
        State.bSawSlide |= Movement->IsSliding();
        if (const int32 Sample=FMath::FloorToInt(PhaseTime*10.0); Sample > State.SlideSample)
        {
            State.SlideSample=Sample;
            UE_LOG(LogTemp,Display,TEXT("COMBINED_HOUSE_SLIDE_SAMPLE t=%.2f speed=%.0f travel=%.0f sliding=%d loc=%s"),
                PhaseTime,HorizontalSpeed,FVector::Dist2D(State.SlideStart,Pawn->GetActorLocation()),
                Movement->IsSliding()?1:0,*Pawn->GetActorLocation().ToString());
        }
        if (State.bSawSlide && PhaseTime >= 0.70)
        {
            State.PreJumpSpeed = HorizontalSpeed;
            const float Travel=FVector::Dist2D(State.SlideStart,Pawn->GetActorLocation());
            const float CameraDrop=State.StandingEyeZ-Pawn->FirstPersonCamera->GetComponentLocation().Z;
            if (Travel<400.f || CameraDrop<55.f)
            {
                FinishSmoke(State,false,FString::Printf(TEXT("Slide lacks distance or low camera: %.0f cm, drop %.0f cm"),Travel,CameraDrop));
                break;
            }
            if (State.PreJumpSpeed < Movement->SlideEndSpeed + 20.0f)
            {
                FinishSmoke(State, false, TEXT("Slide lost its entry momentum too quickly"));
                break;
            }
            Movement->SetSlideHeld(false);
            Pawn->Jump();
            State.Phase = ECombinedHousePhase::Air;
            State.PhaseStartedAt = Now;
            UE_LOG(LogTemp, Display, TEXT("COMBINED_HOUSE_SMOKE: Slide %.0f cm, camera drop %.0f cm, retained %.0f cm/s before jump"),Travel,CameraDrop,State.PreJumpSpeed);
        }
        else if (PhaseTime > 1.2)
            FinishSmoke(State, false, TEXT("Slide never entered the sliding state"));
        break;

    case ECombinedHousePhase::Air:
        Pawn->AddMovementInput(State.Direction, 1.0f, true);
        if (Movement->IsFalling())
        {
            State.bSawFalling = true;
            State.LowestAirSpeed = FMath::Min(State.LowestAirSpeed, HorizontalSpeed);
        }
        if (PhaseTime >= 0.22 && State.bSawFalling)
        {
            Pawn->StopJumping();
            if (State.LowestAirSpeed < FMath::Max(Movement->AirWishSpeed + 40.0f, State.PreJumpSpeed * 0.72f))
            {
                FinishSmoke(State, false, FString::Printf(TEXT("Jump erased horizontal momentum (air minimum %.0f cm/s)"), State.LowestAirSpeed));
                break;
            }
            UPrimitiveComponent* Furniture = nullptr;
            if (!PlacePlayerBesideFurniture(World, Pawn, State.FurnitureCandidates,
                Furniture, State.DragDirection))
            {
                FinishSmoke(State, false, TEXT("No tagged chair has a clear standing position and player-visible collision"));
                break;
            }
            State.Furniture = Furniture;
            State.FurnitureStart = Furniture->GetComponentLocation();
            Movement->StopMovementImmediately();
            Movement->SetMovementMode(MOVE_Walking);
            State.Phase = ECombinedHousePhase::Grab;
            State.PhaseStartedAt = Now;
            UE_LOG(LogTemp, Display, TEXT("COMBINED_HOUSE_SMOKE: Jump kept %.0f cm/s; approaching %s for player interaction"),
                State.LowestAirSpeed, *Furniture->GetOwner()->GetName());
        }
        else if (PhaseTime > 1.0)
            FinishSmoke(State, false, TEXT("Jump never entered falling movement"));
        break;

    case ECombinedHousePhase::Grab:
    {
        UPrimitiveComponent* Furniture = State.Furniture.Get();
        APlayerController* Controller = Cast<APlayerController>(Pawn->GetController());
        if (!Furniture || !Controller || !Pawn->FurnitureInteraction)
        {
            FinishSmoke(State, false, TEXT("Chair, controller, or grab component disappeared"));
            break;
        }
        if (PhaseTime < 0.15f) break; // Allow the camera to update after teleport.
        FVector ViewLocation;
        FRotator ViewRotation;
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
        const FVector Target = Furniture->Bounds.Origin;
        const FRotator Aim = (Target - ViewLocation).Rotation();
        Controller->SetControlRotation(Aim);
        Pawn->SetActorRotation(FRotator(0.0f, Aim.Yaw, 0.0f));
        if (!State.bGrabViewAligned)
        {
            State.bGrabViewAligned = true;
            State.PhaseStartedAt = Now;
            break;
        }
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
        FCollisionQueryParams Query;
        Query.AddIgnoredActor(Pawn);
        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit, ViewLocation,
            ViewLocation + ViewRotation.Vector() * Pawn->FurnitureInteraction->GrabRange,
            ECC_Visibility, Query) || Hit.GetComponent() != Furniture)
        {
            FinishSmoke(State, false, TEXT("The player view trace cannot hit the tagged chair"));
            break;
        }
        Pawn->FurnitureInteraction->ToggleGrab();
        if (!Pawn->FurnitureInteraction->bHoldingFurniture
            || !Furniture->ComponentHasTag(TEXT("SSOFurnitureHeld")))
        {
            FinishSmoke(State, false, TEXT("Player E interaction did not grab the chair"));
            break;
        }
        State.FurnitureStart = Furniture->GetComponentLocation();
        State.Phase = ECombinedHousePhase::Drag;
        State.PhaseStartedAt = Now;
        UE_LOG(LogTemp, Display, TEXT("COMBINED_HOUSE_SMOKE: Player grabbed %s"),
            *Furniture->GetOwner()->GetName());
        break;
    }

    case ECombinedHousePhase::Drag:
    {
        UPrimitiveComponent* Furniture = State.Furniture.Get();
        if (!Furniture || !Pawn->FurnitureInteraction || !Pawn->FurnitureInteraction->bHoldingFurniture)
        {
            FinishSmoke(State, false, TEXT("Player lost the furniture while dragging"));
            break;
        }
        if (PhaseTime < 0.35f) Pawn->AddMovementInput(State.DragDirection, 0.35f, true);
        const float Displacement = FVector::Dist(State.FurnitureStart, Furniture->GetComponentLocation());
        if (PhaseTime >= 0.45f && Displacement >= 10.0f)
        {
            Pawn->FurnitureInteraction->ToggleGrab();
            if (Pawn->FurnitureInteraction->bHoldingFurniture
                || Furniture->ComponentHasTag(TEXT("SSOFurnitureHeld")))
                FinishSmoke(State, false, TEXT("Player could not release the furniture"));
            else
                FinishSmoke(State, true, FString::Printf(TEXT("Four tagged physics bodies; sprint %.0f, slide %.0f, air minimum %.0f cm/s; player grabbed, dragged %.1f cm, and released %s"),
                    State.SprintPeak, State.SlideEntrySpeed, State.LowestAirSpeed, Displacement, *Furniture->GetOwner()->GetName()));
        }
        else if (PhaseTime > 1.5f)
            FinishSmoke(State, false, FString::Printf(TEXT("Player grabbed furniture but drag moved it only %.1f cm"), Displacement));
        break;
    }

    default:
        break;
    }
}
#endif
