// Opt-in traversal proof for the impossible hallway. Only the initial approach
// is placed by the harness; every later position comes from normal movement and
// the production hallway's loop / return behavior.
#include "HouseImpossibleHallway.h"
#include "HouseWorldScale.h"
#include "ExistingHouseDoor.h"
#include "StoryCampaign.h"
#include "StoryMovementComponent.h"
#include "HouseRoamingPig.h"
#include "HouseFurnitureInteractionComponent.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
struct FHallwaySmoke
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AHouseImpossibleHallway> Hall;
    TWeakObjectPtr<AExistingHouseDoor> Door;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;
    TWeakObjectPtr<AHouseRoamingPig> CarryPig;
    TWeakObjectPtr<AActor> PhysicsCube;
    TWeakObjectPtr<UStaticMeshComponent> CubeBody;
    FTransform OriginalPigTransform;
    bool bPigSaved = false, bCarryLifted = false, bPigPlaced = false;
    bool bPhysicsGrabbed = false, bPhysicsReleased = false;
    int32 BeforePigLoops = 0, PigLoops = 0, BeforeCubeLoops = 0, CubeLoops = 0;
    int32 PigHoldFrames = 0, CubeHoldFrames = 0;
    float PigGripMaxDistance = 0, CubeMaxDistance = 0, CubeHoldDistance = 0, PickupPitch = 0;
    float PigTurnStartDepth = 0, PigReturnDistance = 0, PigPlaceGap = 0;
    bool bPigPlaceGrounded = false;
    int32 Stage = 0, LastStage = INDEX_NONE;
    double WallStarted = 0, Started = 0, StageStarted = 0, LastSample = 0;
    int32 InitialLoops = 0, WalkLoops = 0, BeforeSprintLoops = 0, SprintLoops = 0;
    int32 BeforeCrouchLoops = 0, CrouchLoops = 0, BeforeJumpLoops = 0, JumpLoops = 0;
    int32 BeforeTurnReturns = 0, TurnReturns = 0, ComponentCount = 0, InstanceCount = 0;
    int32 ActorCount = 0, EndActorCount = 0, GroundedWalkFrames = 0, WalkFrames = 0;
    int32 ScreenshotFrames = 0, OpenPresses = 0, ClosePresses = 0;
    float StandingHalfHeight = 0, NativeStandingHalfHeight = 0, ObservedStandingHalfHeight = 0;
    float InitialGravity = 0, InitialMaxWalkSpeed = 0;
    float InitialTravel = 0, WalkTravel = 0, WalkTopSpeed = 0, SprintTopSpeed = 0;
    float CrouchTopSpeed = 0, JumpPeak = 0, WallSide = 0, WallSpeed = 0;
    float ReturnDoorDistance = 0, WalkAirSeconds = 0;
    bool bClosedBlocked = false, bExited = false, bReentered = false;
    bool bCrouched = false, bStood = false, bJumped = false, bLanded = false;
    bool bLateralBlocked = false, bBackwardExit = false, bClosedAgain = false;
    bool bSprintHeld = false, bCrouchHeld = false, bJumpHeld = false, bEHeld = false;
    FString Evidence, CapturePath;
    TArray<FString> Captures;
};

bool HallwayKey(APlayerController* PC, const FKey& Key, EInputEvent Event)
{
    if (!PC || !PC->PlayerInput || !FSlateApplication::IsInitialized()) return false;
    PC->InputKey(FInputKeyEventArgs(nullptr,
        FSlateApplication::Get().GetInputDeviceIdForMouse(), Key, Event, 1.f, false, 0));
    return true;
}

FVector HallwayLocal(const AHouseImpossibleHallway* Hall, const AStoryFirstPersonCharacter* Pawn)
{
    return Hall->GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
}

float HallwayFeetZ(const AStoryFirstPersonCharacter* Pawn)
{
    return Pawn->GetActorLocation().Z - Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

void HallwayCounts(AHouseImpossibleHallway* Hall, int32& Components, int32& Instances)
{
    TArray<UActorComponent*> Parts;
    Hall->GetComponents(Parts);
    Components = Parts.Num();
    Instances = 0;
    for (UActorComponent* Part : Parts)
        if (const UInstancedStaticMeshComponent* Mesh = Cast<UInstancedStaticMeshComponent>(Part))
            Instances += Mesh->GetInstanceCount();
}

int32 HallwayActorCount(UWorld* World)
{
    int32 Count = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
        if (IsValid(*It) && !It->IsActorBeingDestroyed()) ++Count;
    return Count;
}

bool HallwayPlaceStart(UWorld* World, AHouseImpossibleHallway* Hall,
    AStoryFirstPersonCharacter* Pawn, APlayerController* PC)
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ImpossibleHallwaySetup), false, Pawn);
    for (TActorIterator<APawn> It(World); It; ++It) Query.AddIgnoredActor(*It);
    const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Scale = Hall->GetActorScale3D().Z;
    int32 Rejects = 0;
    // Restored hallway clutter can occupy the old single setup spot. Select
    // an actual clear floor point in the same approach, always outside the
    // closed door; later progress still comes only from normal player input.
    for (float Depth : {-170.f, -140.f, -200.f, -230.f, -260.f})
        for (float Side : {0.f, -20.f, 20.f, -40.f, 40.f})
        {
            const FVector Feet = Hall->GetActorTransform().TransformPosition(FVector(Depth, Side, 0.f));
            FHitResult Floor;
            const bool bFloor = World->LineTraceSingleByChannel(Floor,
                Feet + FVector(0, 0, 45.f * Scale), Feet - FVector(0, 0, 80.f * Scale), ECC_Pawn, Query);
            if (!bFloor || Floor.ImpactNormal.Z < .8f
                || FMath::Abs(Floor.ImpactPoint.Z - Hall->GetActorLocation().Z) > 10.f * Scale)
            {
                if (Rejects++ < 12)
                    UE_LOG(LogTemp, Display, TEXT("HOUSE_IMPOSSIBLE_HALLWAY_SETUP_REJECT depth=%.1f side=%.1f probe=%s floorHit=%d actor=%s component=%s point=%s normal=%s floorDelta=%.2f"),
                        Depth, Side, *Feet.ToCompactString(), bFloor, *GetNameSafe(Floor.GetActor()),
                        *GetNameSafe(Floor.GetComponent()), *Floor.ImpactPoint.ToCompactString(),
                        *Floor.ImpactNormal.ToCompactString(), Floor.ImpactPoint.Z - Hall->GetActorLocation().Z);
                continue;
            }
            const FVector Center = Floor.ImpactPoint + FVector(0, 0, Half + 2.15f);
            TArray<FOverlapResult> Overlaps;
            if (World->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Radius, Half), Query))
            {
                for (const FOverlapResult& Overlap : Overlaps)
                    if (Overlap.bBlockingHit && Rejects++ < 12)
                        UE_LOG(LogTemp, Display, TEXT("HOUSE_IMPOSSIBLE_HALLWAY_SETUP_REJECT depth=%.1f side=%.1f center=%s blockedActor=%s component=%s"),
                            Depth, Side, *Center.ToCompactString(), *GetNameSafe(Overlap.GetActor()),
                            *GetNameSafe(Overlap.GetComponent()));
                continue;
            }
            Pawn->SetActorLocation(Center, false, nullptr, ETeleportType::TeleportPhysics);
            Pawn->SetActorRotation(FRotator(0, Hall->GetActorRotation().Yaw, 0));
            PC->SetControlRotation(FRotator(0, Hall->GetActorRotation().Yaw, 0));
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
            UE_LOG(LogTemp, Display, TEXT("HOUSE_IMPOSSIBLE_HALLWAY_SMOKE_START center=%s floor=%s yaw=%.1f depth=%.1f side=%.1f rejects=%d"),
                *Center.ToCompactString(), *Floor.ImpactPoint.ToCompactString(), Hall->GetActorRotation().Yaw,
                Depth, Side, Rejects);
            return true;
        }
    return false;
}

bool HallwayClosedTrace(UWorld* World, AHouseImpossibleHallway* Hall,
    AExistingHouseDoor* Door, APawn* Pawn)
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ImpossibleHallwayClosed), false, Pawn);
    FHitResult Hit;
    const FVector Across = Hall->GetActorTransform().TransformVector(FVector(90.f, 0.f, 0.f));
    const bool bHit = World->LineTraceSingleByChannel(Hit,
        Door->DoorwayCenter - Across, Door->DoorwayCenter + Across, ECC_Pawn, Query);
    return bHit && Hit.GetActor() == Door;
}

bool HallwayTryPigSetup(UWorld* World, AHouseImpossibleHallway* Hall,
    AHouseRoamingPig* Pig, AStoryFirstPersonCharacter* Pawn, APlayerController* PC,
    float& PickupPitch)
{
    // Only the test animal is positioned for a reachable, clear lift. The
    // player remains where their actual backwards movement brought them.
    Pig->SetActorTickEnabled(false);
    Pig->GetCharacterMovement()->StopMovementImmediately();
    Pig->GetCharacterMovement()->DisableMovement();
    Pig->Mood = EHousePigMood::Sniffing;
    const FVector Feet = Pawn->GetActorLocation() - FVector(0, 0,
        Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    int32 FloorCandidates = 0, RangeCandidates = 0, ReachCandidates = 0, ClearSightCandidates = 0;
    int32 DiagnosticLines = 0;
    // Match the proven rear-ankle interaction course, inverted: leave the
    // player where they physically stopped and position the animal so its
    // actual animated ankle is 25-80 cm from the player, rather than assuming
    // a distant body center is a reachable hand contact.
    for (float Radius : {40.f, 50.f, 60.f, 70.f, 80.f, 35.f, 30.f, 25.f})
        for (float YawOffset : {0.f, 90.f, -90.f, 180.f})
            for (float Angle : {-90.f, -135.f, 135.f, 180.f, 90.f, -45.f, 45.f, 0.f})
            {
                const float Yaw = Hall->GetActorRotation().Yaw + YawOffset;
                Pig->SetActorRotation(FRotator(0, Yaw, 0));
                const FVector AnkleOffset = Pig->GetCarryGripPoint() - Pig->GetActorLocation();
                const FVector PlayerFromAnkle = FRotator(0, Yaw + Angle, 0).Vector() * Radius;
                FVector Candidate = Feet - PlayerFromAnkle - FVector(AnkleOffset.X, AnkleOffset.Y, 0.f);
                Candidate.Z = Feet.Z;
                FCollisionQueryParams Query(SCENE_QUERY_STAT(HallwayCarrySetup), false, Pig);
                Query.AddIgnoredActor(Pawn);
                FHitResult Floor;
                if (!World->LineTraceSingleByChannel(Floor, Candidate + FVector(0, 0, 35),
                        Candidate - FVector(0, 0, 70), ECC_Pawn, Query)
                    || Floor.ImpactNormal.Z < .8f) continue;
                const float Half = Pig->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                Pig->SetActorLocationAndRotation(Floor.ImpactPoint + FVector(0, 0, Half + 2.15f),
                    FRotator(0, Yaw, 0), false, nullptr,
                    ETeleportType::TeleportPhysics);
                ++FloorCandidates;
                const FRotator Aim = (Pig->GetInteractionLocation() - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation();
                PC->SetControlRotation(Aim);
                if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
                FVector View; FRotator ViewRotation;
                PC->GetPlayerViewPoint(View, ViewRotation);
                const FVector ToInteraction = Pig->GetInteractionLocation() - View;
                const bool bInRange = ToInteraction.SizeSquared() <= FMath::Square(220.f);
                const bool bFacing = FVector::DotProduct(ToInteraction.GetSafeNormal(), ViewRotation.Vector()) >= .72f;
                const bool bReachable = Pawn->CanReachPigGrip(Pig->GetCarryGripPoint(), Pig->GetCarryGripDirection());
                RangeCandidates += bInRange && bFacing ? 1 : 0;
                ReachCandidates += bReachable ? 1 : 0;
                FHitResult SightHit, PawnHit, LiftHit;
                const bool bSightBlocked = World->LineTraceSingleByChannel(SightHit, View,
                    Pig->GetInteractionLocation(), ECC_Visibility, Query);
                const bool bPawnBlocked = World->LineTraceSingleByChannel(PawnHit, View,
                    Pig->GetInteractionLocation(), ECC_Pawn, Query);
                ClearSightCandidates += !bSightBlocked && !bPawnBlocked ? 1 : 0;
                const FVector LiftOffset = FRotator(0, Yaw, 0).RotateVector(FVector(8.5f * Pig->VisualScale, 0, 0));
                const bool bLiftBlocked = World->SweepSingleByChannel(LiftHit, Pig->GetActorLocation() + LiftOffset,
                    Pig->GetActorLocation() + LiftOffset + FVector(0, 0, 90.f * Pig->VisualScale),
                    FRotator(0, Yaw, 0).Quaternion(), ECC_Pawn,
                    FCollisionShape::MakeBox(FVector(78, 29, 45) * Pig->VisualScale), Query);
                if (!Pig->TryPickup(Pawn))
                {
                    if (DiagnosticLines++ < 12)
                        UE_LOG(LogTemp, Display,
                            TEXT("HOUSE_IMPOSSIBLE_HALLWAY_CARRY_REJECT name=%s radius=%.1f yawOffset=%.1f angle=%.1f feet=%s ankle=%s view=%s range=%.1f facing=%d reachable=%d sightBlocked=%d sightActor=%s pawnBlocked=%d pawnActor=%s liftBlocked=%d liftActor=%s"),
                            *Pig->PigName, Radius, YawOffset, Angle, *Pig->FeetLocation().ToCompactString(),
                            *Pig->GetCarryGripPoint().ToCompactString(), *View.ToCompactString(), ToInteraction.Size(),
                            bFacing ? 1 : 0, bReachable ? 1 : 0, bSightBlocked ? 1 : 0, *GetNameSafe(SightHit.GetActor()),
                            bPawnBlocked ? 1 : 0, *GetNameSafe(PawnHit.GetActor()), bLiftBlocked ? 1 : 0, *GetNameSafe(LiftHit.GetActor()));
                    continue;
                }
                Pig->SetActorTickEnabled(true);
                PickupPitch = Aim.Pitch;
                UE_LOG(LogTemp, Display,
                    TEXT("HOUSE_IMPOSSIBLE_HALLWAY_CARRY_SETUP name=%s feet=%s ankle=%s radius=%.1f yawOffset=%.1f angle=%.1f pitch=%.1f reachable=%d"),
                    *Pig->PigName, *Pig->FeetLocation().ToCompactString(), *Pig->GetCarryGripPoint().ToCompactString(),
                    Radius, YawOffset, Angle, PickupPitch, bReachable ? 1 : 0);
                return true;
            }
    UE_LOG(LogTemp, Display,
        TEXT("HOUSE_IMPOSSIBLE_HALLWAY_CARRY_SETUP_FAILED name=%s floorCandidates=%d rangeCandidates=%d reachableCandidates=%d clearSightCandidates=%d playerFeet=%s lastAnkle=%s"),
        *Pig->PigName, FloorCandidates, RangeCandidates, ReachCandidates, ClearSightCandidates,
        *Feet.ToCompactString(), *Pig->GetCarryGripPoint().ToCompactString());
    Pig->SetActorTickEnabled(true);
    Pig->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    return false;
}

bool HallwaySpawnPhysicsCube(UWorld* World, AHouseImpossibleHallway* Hall,
    AStoryFirstPersonCharacter* Pawn, APlayerController* PC,
    TWeakObjectPtr<AActor>& OutActor, TWeakObjectPtr<UStaticMeshComponent>& OutBody,
    float& AimPitch)
{
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Mesh) return false;
    FActorSpawnParameters Params;
    Params.ObjectFlags = RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Cube = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
    if (!Cube) return false;
    UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(Cube, TEXT("HallwayPhysicsTestCube"));
    Cube->AddInstanceComponent(Body);
    Cube->SetRootComponent(Body);
    Body->SetMobility(EComponentMobility::Movable);
    Body->SetStaticMesh(Mesh);
    Body->SetWorldScale3D(FVector(.4f));
    Body->SetCollisionProfileName(TEXT("BlockAll"));
    Body->RegisterComponent();
    FVector Center = Pawn->GetActorLocation() + Hall->GetActorForwardVector() * 140.f;
    Center.Z = Hall->GetActorLocation().Z + 22.f;
    Cube->SetActorLocation(Center, false, nullptr, ETeleportType::TeleportPhysics);
    Cube->Tags.Add(TEXT("SSOMovableFurniture"));
    Body->SetSimulatePhysics(true);
    Body->SetEnableGravity(true);
    const FRotator Aim = (Center - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation();
    PC->SetControlRotation(Aim);
    if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
    OutActor = Cube; OutBody = Body; AimPitch = Aim.Pitch;
    return Body->IsSimulatingPhysics();
}
}
#endif

static void RunHouseImpossibleHallwaySmoke(UWorld* World)
{
#if !UE_BUILD_SHIPPING
    if (!World || !FParse::Param(FCommandLine::Get(), TEXT("HouseImpossibleHallwaySmoke"))
        || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    static FHallwaySmoke S;
    if (S.World.Get() != World)
    {
        S = FHallwaySmoke(); S.World = World;
        S.WallStarted = FPlatformTime::Seconds();
        S.Started = World->GetTimeSeconds();
        S.LastSample = S.Started;
        if (!FParse::Value(FCommandLine::Get(), TEXT("HouseImpossibleHallwayEvidence="), S.Evidence, false))
            S.Evidence = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HouseImpossibleHallwaySmoke"));
        S.Evidence = FPaths::ConvertRelativePathToFull(S.Evidence);
        IFileManager::Get().MakeDirectory(*S.Evidence, true);
    }
    if (S.Stage < 0) return;
    APlayerController* PC = World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = PC ? Cast<AStoryFirstPersonCharacter>(PC->GetPawn()) : nullptr;
    const double Now = World->GetTimeSeconds();
    const float Delta = FMath::Clamp(float(Now - S.LastSample), 0.f, .25f);
    S.LastSample = Now;

    auto Finish = [&](bool bPass, const FString& Detail)
    {
        if (PC)
        {
            if (S.bEHeld) HallwayKey(PC, EKeys::E, IE_Released);
            if (S.bSprintHeld) HallwayKey(PC, EKeys::LeftShift, IE_Released);
            if (S.bCrouchHeld) HallwayKey(PC, EKeys::LeftControl, IE_Released);
            if (S.bJumpHeld) HallwayKey(PC, EKeys::SpaceBar, IE_Released);
        }
        S.EndActorCount = HallwayActorCount(World);
        TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
        Report->SetBoolField(TEXT("passed"), bPass);
        Report->SetStringField(TEXT("detail"), Detail);
        Report->SetNumberField(TEXT("failedOrFinishedStage"), S.Stage);
        Report->SetNumberField(TEXT("seconds"), Now - S.Started);
        Report->SetNumberField(TEXT("authoredHouseScale"), HouseWorldScale::Factor(World));
        if (const AHouseImpossibleHallway* Hall = S.Hall.Get())
            Report->SetNumberField(TEXT("worldModuleLengthCm"), Hall->ModuleLength * Hall->GetActorScale3D().X);
        Report->SetStringField(TEXT("movementMethod"), TEXT("One setup placement outside the closed door; subsequent progress uses stock CharacterMovement input, native E / sprint / crouch / jump keys and production hallway translations. No harness progress teleports."));
        Report->SetNumberField(TEXT("walkLoops"), S.WalkLoops);
        Report->SetNumberField(TEXT("walkVirtualTravelCm"), S.WalkTravel);
        Report->SetNumberField(TEXT("walkTopSpeed"), S.WalkTopSpeed);
        Report->SetNumberField(TEXT("groundedWalkFrames"), S.GroundedWalkFrames);
        Report->SetNumberField(TEXT("walkFrames"), S.WalkFrames);
        Report->SetNumberField(TEXT("sprintLoops"), S.SprintLoops);
        Report->SetNumberField(TEXT("sprintTopSpeed"), S.SprintTopSpeed);
        Report->SetNumberField(TEXT("crouchLoops"), S.CrouchLoops);
        Report->SetNumberField(TEXT("crouchTopSpeed"), S.CrouchTopSpeed);
        Report->SetNumberField(TEXT("jumpLoops"), S.JumpLoops);
        Report->SetNumberField(TEXT("jumpPeakCm"), S.JumpPeak);
        Report->SetNumberField(TEXT("turnReturns"), S.TurnReturns);
        Report->SetNumberField(TEXT("returnDoorDistanceCm"), S.ReturnDoorDistance);
        Report->SetNumberField(TEXT("lateralWallPositionCm"), S.WallSide);
        Report->SetNumberField(TEXT("lateralWallSpeed"), S.WallSpeed);
        Report->SetNumberField(TEXT("corridorComponentCount"), S.ComponentCount);
        Report->SetNumberField(TEXT("corridorInstanceCount"), S.InstanceCount);
        Report->SetNumberField(TEXT("worldActorCountBefore"), S.ActorCount);
        Report->SetNumberField(TEXT("worldActorCountAfter"), S.EndActorCount);
        Report->SetBoolField(TEXT("closedDoorBlocked"), S.bClosedBlocked);
        Report->SetBoolField(TEXT("forwardExit"), S.bExited);
        Report->SetBoolField(TEXT("reentry"), S.bReentered);
        Report->SetBoolField(TEXT("crouched"), S.bCrouched);
        Report->SetBoolField(TEXT("stood"), S.bStood);
        Report->SetNumberField(TEXT("initialStandingHalfHeightCm"), S.StandingHalfHeight);
        Report->SetNumberField(TEXT("nativeDefaultStandingHalfHeightCm"), S.NativeStandingHalfHeight);
        Report->SetNumberField(TEXT("observedUncrouchedHalfHeightCm"), S.ObservedStandingHalfHeight);
        Report->SetBoolField(TEXT("jumped"), S.bJumped);
        Report->SetBoolField(TEXT("landed"), S.bLanded);
        Report->SetBoolField(TEXT("lateralWallBlocked"), S.bLateralBlocked);
        Report->SetBoolField(TEXT("backwardExit"), S.bBackwardExit);
        Report->SetBoolField(TEXT("closedAgain"), S.bClosedAgain);
        Report->SetNumberField(TEXT("nativeOpenKeyPresses"), S.OpenPresses);
        Report->SetNumberField(TEXT("nativeCloseKeyPresses"), S.ClosePresses);
        Report->SetNumberField(TEXT("carriedPigLoops"), S.PigLoops);
        Report->SetNumberField(TEXT("carriedPigHoldFrames"), S.PigHoldFrames);
        Report->SetNumberField(TEXT("carriedPigMaxHandGapCm"), S.PigGripMaxDistance);
        Report->SetNumberField(TEXT("carriedPigActualReturnDistanceCm"), S.PigReturnDistance);
        Report->SetNumberField(TEXT("carriedPigPlacementHandGapCm"), S.PigPlaceGap);
        Report->SetBoolField(TEXT("carriedPigPlacementGrounded"), S.bPigPlaceGrounded);
        Report->SetBoolField(TEXT("carriedPigLiftedNormally"), S.bCarryLifted);
        Report->SetBoolField(TEXT("carriedPigPlacedNormally"), S.bPigPlaced);
        Report->SetNumberField(TEXT("heldPhysicsLoops"), S.CubeLoops);
        Report->SetNumberField(TEXT("heldPhysicsFrames"), S.CubeHoldFrames);
        Report->SetNumberField(TEXT("heldPhysicsMaxViewDistanceCm"), S.CubeMaxDistance);
        Report->SetBoolField(TEXT("physicsGrabbedNormally"), S.bPhysicsGrabbed);
        Report->SetBoolField(TEXT("physicsReleasedNormally"), S.bPhysicsReleased);
        if (Pawn) Report->SetStringField(TEXT("finalPawnCenter"), Pawn->GetActorLocation().ToString());
        TArray<TSharedPtr<FJsonValue>> CaptureValues;
        for (const FString& Path : S.Captures) CaptureValues.Add(MakeShared<FJsonValueString>(Path));
        Report->SetArrayField(TEXT("captures"), CaptureValues);
        FString Json;
        FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *FPaths::Combine(S.Evidence, TEXT("result.json")));
        UE_LOG(LogTemp, Display, TEXT("HOUSE_IMPOSSIBLE_HALLWAY_SMOKE_RESULT passed=%d stage=%d walkLoops=%d returns=%d sprintLoops=%d crouchLoops=%d jumpLoops=%d detail=%s"),
            bPass ? 1 : 0, S.Stage, S.WalkLoops, S.TurnReturns, S.SprintLoops, S.CrouchLoops, S.JumpLoops, *Detail);
        S.Stage = -1;
        FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
    };
    auto Stage = [&](int32 Next)
    { S.Stage = Next; S.StageStarted = Now; };
    if (FPlatformTime::Seconds() - S.WallStarted > 180.0)
    { Finish(false, TEXT("Native hallway traversal exceeded its 180 second wall timeout")); return; }
    if (!Pawn || !Pawn->FirstPersonCamera || !World->GetGameViewport() || !PC->PlayerInput) return;
    if (S.Stage != S.LastStage)
    {
        S.LastStage = S.Stage;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_IMPOSSIBLE_HALLWAY_SMOKE_STAGE stage=%d seconds=%.2f pawn=%s"),
            S.Stage, Now - S.Started, *Pawn->GetActorLocation().ToCompactString());
    }
    if (S.Stage == 0)
    {
        if (Now - S.Started < 3.f) return;
        int32 HallCount = 0;
        for (TActorIterator<AHouseImpossibleHallway> It(World); It; ++It)
        { ++HallCount; S.Hall = *It; }
        AHouseImpossibleHallway* Hall = S.Hall.Get();
        if (HallCount != 1 || !Hall)
        { Finish(false, TEXT("Expected exactly one configured impossible hallway in the house")); return; }
        if (!Hall->GetActorScale3D().Equals(FVector(HouseWorldScale::Factor(World)), .001f))
        { Finish(false, TEXT("Impossible corridor geometry does not match the saved house scale")); return; }
        for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
            if (It->DoorId == Hall->EntranceDoorId)
            {
                if (S.Door.IsValid())
                { Finish(false, TEXT("Entrance door ID resolves to more than one actor")); return; }
                S.Door = *It;
            }
        AExistingHouseDoor* Door = S.Door.Get();
        if (!Door || Door->bLocked || Door->IsOpen() || !Door->OriginalLeaf)
        { Finish(false, TEXT("Entrance must start as the unlocked, closed original end door")); return; }
        if (!HallwayPlaceStart(World, Hall, Pawn, PC))
        { Finish(false, TEXT("Initial approach has no clear walkable floor outside the door")); return; }
        S.Pawn = Pawn;
        S.StandingHalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const ACharacter* DefaultCharacter = Pawn->GetClass()->GetDefaultObject<ACharacter>();
        S.NativeStandingHalfHeight = DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()
            * Pawn->GetCapsuleComponent()->GetShapeScale();
        S.InitialGravity = Pawn->GetCharacterMovement()->GravityScale;
        S.InitialMaxWalkSpeed = Pawn->GetCharacterMovement()->MaxWalkSpeed;
        S.InitialLoops = Hall->TotalLoops; S.InitialTravel = Hall->VirtualTravel;
        HallwayCounts(Hall, S.ComponentCount, S.InstanceCount);
        S.ActorCount = HallwayActorCount(World);
        Stage(1); return;
    }

    AHouseImpossibleHallway* Hall = S.Hall.Get();
    AExistingHouseDoor* Door = S.Door.Get();
    if (!Hall || !Door || S.Pawn.Get() != Pawn)
    { Finish(false, TEXT("Hallway, entrance, or possessed pawn changed during traversal")); return; }
    UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
    UStoryMovementComponent* StoryMovement = Pawn->GetStoryMovement();
    const FVector Forward = Hall->GetActorForwardVector();
    const FVector Right = Hall->GetActorRightVector();
    const FVector Local = HallwayLocal(Hall, Pawn);
    const float FeetZ = HallwayFeetZ(Pawn);
    const float FloorZ = Hall->GetActorLocation().Z;
    const float Age = float(Now - S.StageStarted);
    int32 Components = 0, Instances = 0;
    HallwayCounts(Hall, Components, Instances);
    if (Components != S.ComponentCount || Instances != S.InstanceCount)
    { Finish(false, TEXT("Corridor geometry grew or was replaced while virtual distance increased")); return; }
    if (PC->bShowMouseCursor || PC->IsLookInputIgnored() || PC->IsMoveInputIgnored())
    { Finish(false, TEXT("Hallway changed gameplay mouse capture or blocked movement / look input")); return; }
    if (!FMath::IsNearlyEqual(Movement->GravityScale, S.InitialGravity)
        || !FMath::IsNearlyEqual(Movement->MaxWalkSpeed, S.InitialMaxWalkSpeed))
    { Finish(false, TEXT("Hallway overwrote the player's movement or gravity settings")); return; }
    if (FeetZ < FloorZ - 12.f || FeetZ > FloorZ + Hall->CorridorHeight * Hall->GetActorScale3D().Z)
    { Finish(false, TEXT("Player escaped the corridor floor / ceiling bounds")); return; }
    if (Local.X > 0.f && FMath::Abs(Local.Y) > Hall->CorridorHalfWidth + 3.f)
    { Finish(false, TEXT("Player passed through a corridor side wall")); return; }

    auto RequestCapture = [&](const TCHAR* Name)
    {
        if (FScreenshotRequest::IsScreenshotRequested()) return false;
        S.CapturePath = FPaths::Combine(S.Evidence, Name);
        IFileManager::Get().Delete(*S.CapturePath, false, true);
        FScreenshotRequest::RequestScreenshot(S.CapturePath, false, false);
        S.ScreenshotFrames = 0;
        return true;
    };
    auto CaptureReady = [&]()
    {
        ++S.ScreenshotFrames;
        if (FScreenshotRequest::IsScreenshotRequested() || S.ScreenshotFrames < 3
            || IFileManager::Get().FileSize(*S.CapturePath) < 1024) return false;
        S.Captures.Add(S.CapturePath);
        S.CapturePath.Reset();
        return true;
    };
    auto Aim = [&](float Offset)
    { PC->SetControlRotation(FRotator(0.f, Hall->GetActorRotation().Yaw + Offset, 0.f)); };

    if (S.Stage == 1)
    {
        if (Age < .3f) return;
        if (!Movement->IsMovingOnGround() || Hall->IsTravelerActive(Pawn)
            || !HallwayClosedTrace(World, Hall, Door, Pawn))
        { Finish(false, TEXT("Closed entrance has no solid original leaf or normal grounded approach")); return; }
        Stage(2); return;
    }
    if (S.Stage == 2)
    {
        Pawn->AddMovementInput(Forward, 1.f, true);
        if (Local.X > -20.f || Hall->IsTravelerActive(Pawn) || Hall->TotalLoops != S.InitialLoops)
        { Finish(false, TEXT("Walking entered the illusion through the closed door")); return; }
        if (Age < 1.3f) return;
        if (FVector::DotProduct(Movement->Velocity, Forward) > 5.f
            || !HallwayClosedTrace(World, Hall, Door, Pawn))
        { Finish(false, TEXT("Closed leaf did not physically stop normal walking")); return; }
        S.bClosedBlocked = true;
        PC->SetControlRotation((Door->DoorwayCenter - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        Stage(3); return;
    }
    if (S.Stage == 3)
    {
        if (Age < .15f) return;
        if (Pawn->FocusedDoorForInteraction() != Door)
        { Finish(false, TEXT("Existing door lost its real E interaction prompt")); return; }
        if (!HallwayKey(PC, EKeys::E, IE_Pressed))
        { Finish(false, TEXT("Native E key could not reach the existing entrance")); return; }
        S.bEHeld = true; ++S.OpenPresses;
        Stage(4); return;
    }
    if (S.Stage == 4)
    {
        if (S.bEHeld) { HallwayKey(PC, EKeys::E, IE_Released); S.bEHeld = false; }
        if (Age < .8f) return;
        if (!Door->IsOpen())
        { Finish(false, TEXT("Real E interaction did not open the original entrance")); return; }
        Aim(0);
        Stage(5); return;
    }
    if (S.Stage == 5)
    {
        Pawn->AddMovementInput(Forward, 1.f, true);
        if (Age > .8f)
        {
            ++S.WalkFrames;
            if (Movement->IsMovingOnGround()) { ++S.GroundedWalkFrames; S.WalkAirSeconds = 0; }
            else S.WalkAirSeconds += Delta;
            S.WalkTopSpeed = FMath::Max(S.WalkTopSpeed, float(Movement->Velocity.Size2D()));
            if (S.WalkAirSeconds > .25f || FMath::Abs(FeetZ - FloorZ - 2.15f) > 8.f)
            { Finish(false, TEXT("Looping ordinary walking lost its grounded floor or changed foot height")); return; }
        }
        if (Age < 10.3f) return;
        S.WalkLoops = Hall->TotalLoops - S.InitialLoops;
        S.WalkTravel = Hall->VirtualTravel - S.InitialTravel;
        if (!Hall->IsTravelerActive(Pawn) || S.WalkLoops < 5 || S.WalkTravel < 1800.f
            || S.WalkTopSpeed < 400.f || S.GroundedWalkFrames < 30
            || S.GroundedWalkFrames < .97f * S.WalkFrames)
        { Finish(false, TEXT("Ten seconds of actual walking did not produce five grounded infinite loops at normal speed")); return; }
        Stage(6); return;
    }
    if (S.Stage == 6)
    {
        if (Age < .7f) return;
        if (!RequestCapture(TEXT("forward_dark_hallway.png"))) return;
        Stage(7); return;
    }
    if (S.Stage == 7)
    {
        if (!CaptureReady())
        { if (Age > 8.f) Finish(false, TEXT("Forward hallway screenshot did not complete")); return; }
        S.BeforeTurnReturns = Hall->TotalReturns;
        Stage(8); return;
    }
    if (S.Stage == 8)
    {
        // Gradual normal view rotation, without possessing a test camera or
        // replacing the player's look mappings, so the return occurs in view.
        Aim(FMath::Min(180.f, Age * 100.f));
        if (Age < 2.1f) return;
        S.TurnReturns = Hall->TotalReturns - S.BeforeTurnReturns;
        S.ReturnDoorDistance = FVector::Dist2D(Pawn->GetActorLocation(), Door->DoorwayCenter);
        if (S.TurnReturns < 1 || S.ReturnDoorDistance > 300.f * HouseWorldScale::Factor(World) || Local.X < 30.f
            || !Hall->IsTravelerActive(Pawn) || !Movement->IsMovingOnGround()
            || FMath::Abs(FMath::FindDeltaAngleDegrees(PC->GetControlRotation().Yaw,
                Hall->GetActorRotation().Yaw + 180.f)) > 1.f)
        { Finish(false, TEXT("Looking back did not leave the real entrance immediately behind with free normal view")); return; }
        if (!RequestCapture(TEXT("turned_back_entrance.png"))) return;
        Stage(9); return;
    }
    if (S.Stage == 9)
    {
        if (!CaptureReady())
        { if (Age > 8.f) Finish(false, TEXT("Turned entrance screenshot did not complete")); return; }
        Stage(10); return;
    }
    if (S.Stage == 10)
    {
        Pawn->AddMovementInput(-Forward, 1.f, true);
        if (Local.X > -140.f)
        { if (Age > 5.f) Finish(false, TEXT("Walking toward the returned door could not exit into the real house")); return; }
        if (Hall->IsTravelerActive(Pawn) || !Movement->IsMovingOnGround())
        { Finish(false, TEXT("Player stayed trapped in the illusion after crossing the original entrance")); return; }
        S.bExited = true; Stage(11); return;
    }
    if (S.Stage == 11)
    {
        Aim(180.f * (1.f - FMath::Clamp(Age / .9f, 0.f, 1.f)));
        if (Age < 1.f) return;
        Stage(12); return;
    }
    if (S.Stage == 12)
    {
        Pawn->AddMovementInput(Forward, 1.f, true);
        if (!Hall->IsTravelerActive(Pawn) || Local.X < 100.f)
        { if (Age > 6.f) Finish(false, TEXT("Physical reentry did not reactivate the hallway")); return; }
        S.bReentered = true; S.BeforeSprintLoops = Hall->TotalLoops;
        if (!HallwayKey(PC, EKeys::LeftShift, IE_Pressed))
        { Finish(false, TEXT("Native sprint key unavailable")); return; }
        S.bSprintHeld = true; Stage(13); return;
    }
    if (S.Stage == 13)
    {
        Pawn->AddMovementInput(Forward, 1.f, true);
        S.SprintTopSpeed = FMath::Max(S.SprintTopSpeed, float(Movement->Velocity.Size2D()));
        if (Age < 2.f) return;
        S.SprintLoops = Hall->TotalLoops - S.BeforeSprintLoops;
        if (!StoryMovement || !StoryMovement->IsSprinting() || S.SprintLoops < 3
            || S.SprintTopSpeed < 650.f || !Movement->IsMovingOnGround())
        { Finish(false, TEXT("Normal sprint did not keep looping at the existing sprint speed")); return; }
        HallwayKey(PC, EKeys::LeftShift, IE_Released); S.bSprintHeld = false;
        Stage(14); return;
    }
    if (S.Stage == 14)
    {
        if (Age < 1.f) return;
        if (Movement->Velocity.Size2D() > 5.f)
        { Finish(false, TEXT("Stock braking failed after stopping movement input")); return; }
        if (!HallwayKey(PC, EKeys::LeftControl, IE_Pressed))
        { Finish(false, TEXT("Native crouch key unavailable")); return; }
        S.bCrouchHeld = true; Stage(15); return;
    }
    if (S.Stage == 15)
    {
        if (Age < .4f) return;
        if (!Pawn->bIsCrouched || Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
            > S.StandingHalfHeight - 15.f)
        { Finish(false, TEXT("Existing crouch did not lower the collision capsule")); return; }
        S.bCrouched = true; S.BeforeCrouchLoops = Hall->TotalLoops;
        Stage(16); return;
    }
    if (S.Stage == 16)
    {
        Pawn->AddMovementInput(Forward, 1.f, true);
        S.CrouchTopSpeed = FMath::Max(S.CrouchTopSpeed, float(Movement->Velocity.Size2D()));
        if (Age < 2.2f) return;
        S.CrouchLoops = Hall->TotalLoops - S.BeforeCrouchLoops;
        if (!Pawn->bIsCrouched || S.CrouchLoops < 1 || S.CrouchTopSpeed < 180.f
            || S.CrouchTopSpeed > 310.f || !Movement->IsMovingOnGround())
        { Finish(false, TEXT("Crouched traversal lost its loop, normal speed, or floor")); return; }
        HallwayKey(PC, EKeys::LeftControl, IE_Released); S.bCrouchHeld = false;
        Stage(17); return;
    }
    if (S.Stage == 17)
    {
        if (Age < .7f) return;
        S.ObservedStandingHalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        // Engine UnCrouch restores the Character class default capsule, which
        // can differ slightly from the selected roster's runtime stature. Both
        // are existing legitimate standing dimensions; the hallway must not
        // change them merely to satisfy the harness's original-height check.
        const bool bRestoredRuntime = FMath::IsNearlyEqual(S.ObservedStandingHalfHeight, S.StandingHalfHeight, .5f);
        const bool bRestoredNative = FMath::IsNearlyEqual(S.ObservedStandingHalfHeight, S.NativeStandingHalfHeight, .5f);
        const TCHAR* Reason = Pawn->bIsCrouched ? TEXT("still_crouched")
            : (!bRestoredRuntime && !bRestoredNative) ? TEXT("unexpected_capsule_height")
            : bRestoredRuntime ? TEXT("runtime_standing_height_restored") : TEXT("native_class_default_restored");
        UE_LOG(LogTemp, Display,
            TEXT("HOUSE_IMPOSSIBLE_HALLWAY_UNCROUCH crouched=%d actualHalf=%.3f initialHalf=%.3f nativeDefaultHalf=%.3f grounded=%d feetZ=%.3f reason=%s"),
            Pawn->bIsCrouched ? 1 : 0, S.ObservedStandingHalfHeight, S.StandingHalfHeight,
            S.NativeStandingHalfHeight, Movement->IsMovingOnGround() ? 1 : 0, FeetZ, Reason);
        if (Pawn->bIsCrouched || (!bRestoredRuntime && !bRestoredNative) || !Movement->IsMovingOnGround())
        { Finish(false, FString::Printf(TEXT("Player did not regain native grounded standing after crouching: %s"), Reason)); return; }
        S.bStood = true; S.BeforeJumpLoops = Hall->TotalLoops;
        if (!HallwayKey(PC, EKeys::SpaceBar, IE_Pressed))
        { Finish(false, TEXT("Native jump key unavailable")); return; }
        S.bJumpHeld = true; Stage(18); return;
    }
    if (S.Stage == 18)
    {
        if (S.bJumpHeld && Age > .15f) { HallwayKey(PC, EKeys::SpaceBar, IE_Released); S.bJumpHeld = false; }
        Pawn->AddMovementInput(Forward, 1.f, true);
        S.bJumped |= Movement->IsFalling();
        S.JumpPeak = FMath::Max(S.JumpPeak, FeetZ - FloorZ - 2.15f);
        if (Age < 1.8f) return;
        S.JumpLoops = Hall->TotalLoops - S.BeforeJumpLoops;
        S.bLanded = Movement->IsMovingOnGround() && FMath::Abs(FeetZ - FloorZ - 2.15f) < 8.f;
        if (!S.bJumped || S.JumpPeak < 5.f || !S.bLanded || S.JumpLoops < 1)
        { Finish(false, TEXT("Real jump could not move, loop, and land naturally under the corridor ceiling")); return; }
        Stage(19); return;
    }
    if (S.Stage == 19)
    {
        Pawn->AddMovementInput(Right, 1.f, true);
        if (Age < 1.3f) return;
        const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
        S.WallSide = float(Local.Y * Hall->GetActorScale3D().Y);
        S.WallSpeed = FMath::Abs(FVector::DotProduct(Movement->Velocity, Right));
        const float WallHalfWidth = Hall->CorridorHalfWidth * Hall->GetActorScale3D().Y;
        if (S.WallSide < WallHalfWidth - Radius - 12.f
            || S.WallSide > WallHalfWidth - Radius + 5.f || S.WallSpeed > 10.f)
        { Finish(false, TEXT("Visible side wall did not physically stop sideways walking at capsule clearance")); return; }
        S.bLateralBlocked = true; Stage(20); return;
    }
    if (S.Stage == 20)
    {
        if (Local.Y > 4.f)
        {
            Pawn->AddMovementInput(-Right, .2f, true);
            if (Age > 5.f) Finish(false, TEXT("Player could not walk back off the lateral wall"));
            return;
        }
        Stage(21); return;
    }
    if (S.Stage == 21)
    {
        // Backpedal while the view still faces into the corridor. The illusion
        // must leave a physical route out even without a turn-back trigger.
        Pawn->AddMovementInput(-Forward, 1.f, true);
        if (Local.X > -140.f)
        { if (Age > 5.f) Finish(false, TEXT("Backward walking was trapped by the looping corridor")); return; }
        if (Hall->IsTravelerActive(Pawn))
        { Finish(false, TEXT("Backward exit failed to release the traveler")); return; }
        S.bBackwardExit = true; Stage(22); return;
    }
    if (S.Stage == 22)
    {
        if (Age < .7f) return;
        Aim(0);
        if (Pawn->FocusedDoorForInteraction() != Door || !Door->IsOpen())
        { Finish(false, TEXT("Returned house entrance lost its normal close interaction")); return; }
        if (!HallwayKey(PC, EKeys::E, IE_Pressed))
        { Finish(false, TEXT("Native E close key unavailable")); return; }
        S.bEHeld = true; ++S.ClosePresses; Stage(23); return;
    }
    if (S.Stage == 23)
    {
        if (S.bEHeld) { HallwayKey(PC, EKeys::E, IE_Released); S.bEHeld = false; }
        if (Age < .8f) return;
        if (Door->IsOpen() || !HallwayClosedTrace(World, Hall, Door, Pawn))
        { Finish(false, TEXT("Real door could not close solidly after exiting the infinite hallway")); return; }
        S.bClosedAgain = true;
        S.EndActorCount = HallwayActorCount(World);
        if (S.EndActorCount != S.ActorCount)
        { Finish(false, TEXT("World actor count changed during bounded infinite corridor traversal")); return; }
        UE_LOG(LogTemp, Display, TEXT("HOUSE_IMPOSSIBLE_HALLWAY_MAIN_CHECKS_PASSED walkLoops=%d returns=%d fixedActors=%d continuingToCarry=1"),
            S.WalkLoops, S.TurnReturns, S.ActorCount);
        Stage(24); return;
    }
    if (S.Stage == 24)
    {
        // Give a right-hand-carried pig clearance past the existing open leaf.
        // Reach the left side by actual walking, preserving the native door.
        if (Local.Y > -30.f)
        { Pawn->AddMovementInput(-Right, .1f, true); if (Age > 4.f) Finish(false, TEXT("Could not walk left for the native carried-pig doorway clearance")); return; }
        if (FMath::Abs(FVector::DotProduct(Movement->Velocity, Right)) > 5.f) return;
        if (!HallwayKey(PC, EKeys::E, IE_Pressed))
        { Finish(false, TEXT("Native E could not reopen the entrance for the carry checks")); return; }
        S.bEHeld = true; ++S.OpenPresses; Stage(25); return;
    }
    if (S.Stage == 25)
    {
        if (S.bEHeld) { HallwayKey(PC, EKeys::E, IE_Released); S.bEHeld = false; }
        if (Age < .8f) return;
        if (!Door->IsOpen())
        { Finish(false, TEXT("Entrance did not reopen for normal object traversal")); return; }
        for (TActorIterator<AHouseRoamingPig> It(World); It; ++It)
            if (!It->IsCarried() && It->Health > 0.f
                && (!S.CarryPig.IsValid() || It->VisualScale < S.CarryPig->VisualScale)) S.CarryPig = *It;
        AHouseRoamingPig* Pig = S.CarryPig.Get();
        if (!Pig)
        { Finish(false, TEXT("No living installed pig available for an actual lift and hallway loop")); return; }
        S.OriginalPigTransform = Pig->GetActorTransform(); S.bPigSaved = true;
        if (!HallwayTryPigSetup(World, Hall, Pig, Pawn, PC, S.PickupPitch))
        { Finish(false, TEXT("No clear, physically reachable rear-leg pickup at the actual entrance approach")); return; }
        Stage(26); return;
    }
    if (S.Stage == 26)
    {
        AHouseRoamingPig* Pig = S.CarryPig.Get();
        if (!Pig || Pig->Carrier != Pawn)
        { Finish(false, TEXT("Actual pig pickup released before completing the normal hand lift")); return; }
        if (Pig->GetCarryPhase() != EHousePigCarryPhase::Held)
        { if (Age > 5.f) Finish(false, TEXT("Actual pig never completed its animated rear-leg lift")); return; }
        S.bCarryLifted = true;
        S.BeforePigLoops = Hall->TotalLoops;
        Stage(27); return;
    }
    if (S.Stage == 27)
    {
        AHouseRoamingPig* Pig = S.CarryPig.Get();
        if (!Pig || Pig->Carrier != Pawn || Pig->GetCarryPhase() != EHousePigCarryPhase::Held)
        { Finish(false, TEXT("Carried pig was released while restoring the normal forward view")); return; }
        PC->SetControlRotation(FRotator(S.PickupPitch * (1.f - FMath::Clamp(Age / .65f, 0.f, 1.f)),
            Hall->GetActorRotation().Yaw, 0.f));
        if (Age < .8f) return;
        Stage(28); return;
    }
    if (S.Stage == 28)
    {
        AHouseRoamingPig* Pig = S.CarryPig.Get();
        if (!Pig || Pig->Carrier != Pawn || Pig->GetCarryPhase() != EHousePigCarryPhase::Held)
        { Finish(false, TEXT("Real walking or the hallway period translation dropped the carried pig")); return; }
        Pawn->AddMovementInput(Forward, .65f, true);
        if (Age > .3f)
        {
            ++S.PigHoldFrames;
            const float Gap = FVector::Dist(Pawn->GetPigHandGripPoint(), Pig->GetCarryGripPoint());
            S.PigGripMaxDistance = FMath::Max(S.PigGripMaxDistance, Gap);
            if (Gap > 30.f || FVector::Dist(Pig->GetActorLocation(), Pawn->GetActorLocation()) > 300.f)
            {
                UE_LOG(LogTemp, Display,
                    TEXT("HOUSE_IMPOSSIBLE_HALLWAY_CARRY_CONTACT_FAILED depth=%.2f side=%.2f loopsSincePickup=%d handGap=%.2f carrierDistance=%.2f pig=%s pawn=%s"),
                    Local.X, Local.Y, Hall->TotalLoops-S.BeforePigLoops, Gap,
                    FVector::Dist(Pig->GetActorLocation(), Pawn->GetActorLocation()),
                    *Pig->GetActorLocation().ToCompactString(), *Pawn->GetActorLocation().ToCompactString());
                Finish(false, Hall->TotalLoops > S.BeforePigLoops
                    ? TEXT("After a walked loop, rendered hand contact or carrier distance failed")
                    : TEXT("Before the first loop, the carried-pig doorway approach lost rendered hand contact or carrier clearance"));
                return;
            }
        }
        if (Age < 3.f) return;
        S.PigLoops = Hall->TotalLoops - S.BeforePigLoops;
        if (S.PigLoops < 1 || S.PigHoldFrames < 25)
        { Finish(false, TEXT("The real held pig did not complete a physically walked infinite loop")); return; }
        Stage(45); return;
    }
    if (S.Stage == 45)
    {
        // Center the carrier in the wider corridor before the animal turns.
        // The loop test does not force a wide pig through the older door leaf.
        AHouseRoamingPig* Pig = S.CarryPig.Get();
        if (!Pig || Pig->Carrier != Pawn)
        { Finish(false, TEXT("The carried pig was lost while walking to the return-door clearance")); return; }
        if (Local.Y < 0.f)
        { Pawn->AddMovementInput(Right, .1f, true); if (Age > 4.f) Finish(false, TEXT("Could not physically center the carried pig before turning")); return; }
        if (FMath::Abs(FVector::DotProduct(Movement->Velocity, Right)) > 5.f) return;
        if (Local.X < Hall->ReturnMinDepth + Hall->ModuleLength + 15.f)
        { Pawn->AddMovementInput(Forward, .1f, true); if (Age > 6.f) Finish(false, TEXT("Could not physically reach a full carried-pig return period")); return; }
        if (Movement->Velocity.Size2D() > 5.f) return;
        S.PigTurnStartDepth = Local.X;
        Stage(29); return;
    }
    if (S.Stage == 29)
    {
        AHouseRoamingPig* Pig = S.CarryPig.Get();
        if (!Pig || Pig->Carrier != Pawn || Pig->GetCarryPhase() != EHousePigCarryPhase::Held)
        { Finish(false, TEXT("Turning back or its rewind dropped the held pig")); return; }
        Aim(FMath::Min(180.f, Age * 110.f));
        if (Age < 1.9f) return;
        S.PigReturnDistance = (S.PigTurnStartDepth - Local.X) * Hall->GetActorScale3D().X;
        if (S.PigReturnDistance < Hall->ModuleLength * Hall->GetActorScale3D().X - 2.f)
        { Finish(false, TEXT("Turning with the held pig did not actually rewind one physical module")); return; }
        if (FVector::Dist(Pig->GetActorLocation(), Pawn->GetActorLocation()) > 300.f)
        { Finish(false, TEXT("Turn-back translation separated the pig from its carrier")); return; }
        Stage(30); return;
    }
    if (S.Stage == 30)
    {
        AHouseRoamingPig* Pig = S.CarryPig.Get();
        if (!Pig || Pig->Carrier != Pawn || Pig->GetCarryPhase() != EHousePigCarryPhase::Held)
        { Finish(false, TEXT("The carried pig was lost after its actual hallway loop and turn-back")); return; }
        // The rewind puts the carrier beside the open original door. Placing
        // toward that leaf must retain its normal physical rejection. Face
        // into the corridor and walk to a clear lowering area instead.
        Aim(180.f * (1.f - FMath::Clamp(Age / .9f, 0.f, 1.f)));
        if (Age < 1.f) return;
        Stage(47); return;
    }
    if (S.Stage == 47)
    {
        AHouseRoamingPig* Pig = S.CarryPig.Get();
        if (!Pig || Pig->Carrier != Pawn || Pig->GetCarryPhase() != EHousePigCarryPhase::Held)
        { Finish(false, TEXT("The carried pig was lost while physically reaching the corridor placement area")); return; }
        if (Local.X < 270.f)
        {
            Pawn->AddMovementInput(Forward, .18f, true);
            if (Age > 4.f) Finish(false, TEXT("Could not physically walk the carried pig away from the open leaf for floor placement"));
            return;
        }
        if (Movement->Velocity.Size2D() > 5.f) return;
        const float Gap = FVector::Dist(Pawn->GetPigHandGripPoint(), Pig->GetCarryGripPoint());
        S.PigGripMaxDistance = FMath::Max(S.PigGripMaxDistance, Gap);
        S.PigPlaceGap = Gap;
        S.bPigPlaceGrounded = Movement->IsMovingOnGround();
        if (Gap > 30.f || !Movement->IsMovingOnGround())
        {
            UE_LOG(LogTemp, Display, TEXT("HOUSE_IMPOSSIBLE_HALLWAY_CARRY_PLACE_CONTACT_FAILED depth=%.2f side=%.2f handGap=%.2f grounded=%d movementMode=%d feetZ=%.2f floorZ=%.2f actualReturnCm=%.2f pawn=%s pig=%s"),
                Local.X, Local.Y, Gap, S.bPigPlaceGrounded, int32(Movement->MovementMode), FeetZ, FloorZ, S.PigReturnDistance,
                *Pawn->GetActorLocation().ToCompactString(), *Pig->GetActorLocation().ToCompactString());
            Finish(false, TEXT("Normal hand contact or grounded movement was lost before placing the rewound pig")); return;
        }
        UE_LOG(LogTemp, Display, TEXT("HOUSE_IMPOSSIBLE_HALLWAY_CARRY_PLACE_APPROACH depth=%.2f side=%.2f handGap=%.2f grounded=%d actualReturnCm=%.2f pawn=%s pig=%s"),
            Local.X, Local.Y, Gap, S.bPigPlaceGrounded, S.PigReturnDistance,
            *Pawn->GetActorLocation().ToCompactString(), *Pig->GetActorLocation().ToCompactString());
        Stage(31); return;
    }
    if (S.Stage == 31)
    {
        if (Age < .5f) return;
        AHouseRoamingPig* Pig = S.CarryPig.Get();
        if (!Pig || !Pig->TryPlace(Pawn))
        { Finish(false, TEXT("Carried pig could not begin normal floor placement inside the corridor after its rewind")); return; }
        Stage(32); return;
    }
    if (S.Stage == 32)
    {
        AHouseRoamingPig* Pig = S.CarryPig.Get();
        if (!Pig)
        { Finish(false, TEXT("Installed pig vanished during its animated placement")); return; }
        if (Pig->IsCarried())
        { if (Age > 3.f) Finish(false, TEXT("Actual pig placement did not lower and release on the floor")); return; }
        S.bPigPlaced = true;
        Pig->SetActorTransform(S.OriginalPigTransform, false, nullptr, ETeleportType::TeleportPhysics);
        Pig->GetCharacterMovement()->StopMovementImmediately();
        Pig->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Pig->RequestFeeding();
        S.bPigSaved = false;
        Stage(46); return;
    }
    if (S.Stage == 46)
    {
        // The player still exits by normal movement after the animal has been
        // placed and restored; there is no test progress teleport to the cube.
        Pawn->AddMovementInput(-Forward, 1.f, true);
        if (Local.X > -140.f)
        { if (Age > 5.f) Finish(false, TEXT("Could not physically exit after placing the looped pig")); return; }
        if (Hall->IsTravelerActive(Pawn))
        { Finish(false, TEXT("Player remained in the illusion after the post-placement exit")); return; }
        Stage(33); return;
    }
    if (S.Stage == 33)
    {
        Aim(180.f * (1.f - FMath::Clamp(Age / .9f, 0.f, 1.f)));
        if (Age < 1.f) return;
        Stage(34); return;
    }
    if (S.Stage == 34)
    {
        Pawn->AddMovementInput(Forward, 1.f, true);
        if (!Hall->IsTravelerActive(Pawn) || Local.X < 100.f)
        { if (Age > 5.f) Finish(false, TEXT("Player could not physically reenter for the physics handle check")); return; }
        Stage(35); return;
    }
    if (S.Stage == 35)
    {
        if (Age < .8f) return;
        if (!Pawn->FurnitureInteraction || Pawn->FurnitureInteraction->bHoldingFurniture
            || !HallwaySpawnPhysicsCube(World, Hall, Pawn, PC, S.PhysicsCube, S.CubeBody, S.PickupPitch))
        { Finish(false, TEXT("Could not create a transient tagged, simulated furniture test body")); return; }
        Stage(36); return;
    }
    if (S.Stage == 36)
    {
        if (Age < .25f) return;
        if (Pawn->FurnitureInteraction->FocusedFurnitureForInteraction() != S.PhysicsCube.Get())
        { Finish(false, TEXT("Existing furniture trace did not focus the visible physical test body")); return; }
        S.CubeHoldDistance = FVector::Dist(S.CubeBody->GetComponentLocation(), Pawn->FirstPersonCamera->GetComponentLocation());
        Pawn->FurnitureInteraction->ToggleGrab();
        if (!Pawn->FurnitureInteraction->bHoldingFurniture)
        { Finish(false, TEXT("Existing physics handle did not grab the real tagged simulated body")); return; }
        S.bPhysicsGrabbed = true; S.BeforeCubeLoops = Hall->TotalLoops;
        Stage(37); return;
    }
    if (S.Stage == 37)
    {
        if (!Pawn->FurnitureInteraction->bHoldingFurniture)
        { Finish(false, TEXT("Physics hold dropped while restoring the normal corridor view")); return; }
        PC->SetControlRotation(FRotator(S.PickupPitch * (1.f - FMath::Clamp(Age / .65f, 0.f, 1.f)),
            Hall->GetActorRotation().Yaw, 0.f));
        if (Age < .9f) return;
        Stage(38); return;
    }
    if (S.Stage == 38)
    {
        UStaticMeshComponent* Body = S.CubeBody.Get();
        if (!Pawn->FurnitureInteraction->bHoldingFurniture || !Body || !Body->IsSimulatingPhysics())
        { Finish(false, TEXT("A real walked loop dropped the physics handle or disabled furniture physics")); return; }
        Pawn->AddMovementInput(Forward, .65f, true);
        ++S.CubeHoldFrames;
        const float Distance = FVector::Dist(Body->GetComponentLocation(), Pawn->FirstPersonCamera->GetComponentLocation());
        S.CubeMaxDistance = FMath::Max(S.CubeMaxDistance, Distance);
        if (Distance > 400.f || Body->GetComponentLocation().Z < FloorZ - 10.f)
        { Finish(false, TEXT("Loop translation separated the held physics body from its view or floor")); return; }
        if (Age < 2.5f) return;
        S.CubeLoops = Hall->TotalLoops - S.BeforeCubeLoops;
        if (S.CubeLoops < 1 || S.CubeHoldFrames < 25)
        { Finish(false, TEXT("Real physics hold did not complete a physically walked infinite loop")); return; }
        Stage(39); return;
    }
    if (S.Stage == 39)
    {
        UStaticMeshComponent* Body = S.CubeBody.Get();
        if (!Pawn->FurnitureInteraction->bHoldingFurniture || !Body || !Body->IsSimulatingPhysics())
        {
            FCollisionQueryParams Query(SCENE_QUERY_STAT(HallwayPhysicsTurnFailure), false, Pawn);
            Query.AddIgnoredActor(S.PhysicsCube.Get());
            const FVector View = Pawn->FirstPersonCamera->GetComponentLocation();
            const FVector Direction = Pawn->GetBaseAimRotation().Vector();
            FHitResult Obstacle;
            const bool bBlocked = World->LineTraceSingleByChannel(Obstacle, View,
                View + Direction * S.CubeHoldDistance, ECC_Visibility, Query);
            const float Clear = bBlocked ? FVector::DotProduct(Obstacle.ImpactPoint - View, Direction) - 25.f : BIG_NUMBER;
            UE_LOG(LogTemp, Display,
                TEXT("HOUSE_IMPOSSIBLE_HALLWAY_PHYSICS_TURN_FAILED depth=%.2f side=%.2f pitch=%.2f holdDistance=%.2f wallClear=%.2f nativeMinimum=%.2f obstacle=%s existingWallReleaseBoundary=%d"),
                Local.X, Local.Y, PC->GetControlRotation().Pitch, S.CubeHoldDistance, Clear,
                Pawn->FurnitureInteraction->MinimumHoldDistance, *GetNameSafe(Obstacle.GetActor()),
                bBlocked && Clear < Pawn->FurnitureInteraction->MinimumHoldDistance ? 1 : 0);
            Finish(false, TEXT("Turning released the physics handle; the trace diagnostic records any existing minimum-clearance wall guard")); return;
        }
        // A level-held object normally releases when turned into a nearby wall:
        // this 110 cm half-width is less than the native 120 cm hold minimum.
        // Lower the view naturally first, so the same untouched wall guard has
        // sufficient diagonal clearance while testing the actual rewind.
        const float TurnAge = FMath::Max(0.f, Age - .5f);
        PC->SetControlRotation(FRotator(-45.f * FMath::Clamp(Age / .5f, 0.f, 1.f),
            Hall->GetActorRotation().Yaw + FMath::Min(180.f, TurnAge * 110.f), 0.f));
        if (Age < 2.4f) return;
        Pawn->FurnitureInteraction->ReleaseGrab();
        if (Pawn->FurnitureInteraction->bHoldingFurniture)
        { Finish(false, TEXT("Existing furniture release failed after a hallway rewind")); return; }
        S.bPhysicsReleased = true; Stage(40); return;
    }
    if (S.Stage == 40)
    {
        if (Age < 1.2f) return;
        UStaticMeshComponent* Body = S.CubeBody.Get();
        if (!Body || !Body->IsSimulatingPhysics() || Body->GetComponentLocation().Z < FloorZ + 15.f)
        { Finish(false, TEXT("Released furniture fell through the physical corridor floor")); return; }
        if (AActor* Cube = S.PhysicsCube.Get()) Cube->Destroy();
        S.PhysicsCube.Reset(); S.CubeBody.Reset();
        Stage(41); return;
    }
    if (S.Stage == 41)
    {
        Pawn->AddMovementInput(-Forward, 1.f, true);
        if (Local.X > -140.f)
        { if (Age > 5.f) Finish(false, TEXT("Could not walk out after releasing the looped physical furniture")); return; }
        Stage(42); return;
    }
    if (S.Stage == 42)
    {
        if (Age < .7f) return;
        PC->SetControlRotation((Door->DoorwayCenter - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        Stage(43); return;
    }
    if (S.Stage == 43)
    {
        if (Age < .15f) return;
        if (Pawn->FocusedDoorForInteraction() != Door || !Door->IsOpen()
            || !HallwayKey(PC, EKeys::E, IE_Pressed))
        { Finish(false, TEXT("Existing entrance did not retain normal close interaction after both carried objects")); return; }
        S.bEHeld = true; ++S.ClosePresses; Stage(44); return;
    }
    if (S.Stage == 44)
    {
        if (S.bEHeld) { HallwayKey(PC, EKeys::E, IE_Released); S.bEHeld = false; }
        if (Age < .8f) return;
        S.EndActorCount = HallwayActorCount(World);
        if (Door->IsOpen() || !HallwayClosedTrace(World, Hall, Door, Pawn)
            || Hall->IsTravelerActive(Pawn) || S.EndActorCount != S.ActorCount)
        { Finish(false, TEXT("Final closed doorway, traveler exit, or bounded actor count failed after object traversal")); return; }
        Finish(true, TEXT("Original door blocked and opened with native E; ten seconds of stock walking produced bounded infinite loops; turning returned the real entrance immediately behind; exit/reentry, sprint, crouch, jump and lateral collision retained normal input and movement. An actually lifted pig and a physics-handle-held simulated body each crossed real walked loops and a turn-back rewind without losing carry, then placed/released normally. Geometry and actor counts stayed bounded."));
        return;
    }
#else
    (void)World;
#endif
}

void TickHouseImpossibleHallwaySmoke(UWorld* World)
{
#if !UE_BUILD_SHIPPING
    if (!World || !FParse::Param(FCommandLine::Get(), TEXT("HouseImpossibleHallwaySmoke"))) return;
    // Observe the final body, hand, physics handle and camera transforms from
    // the same frame rather than mixing GameMode's earlier tick with new IK.
    static TWeakObjectPtr<UWorld> TestWorld;
    TestWorld = World;
    static const FDelegateHandle PostTick = FWorldDelegates::OnWorldPostActorTick.AddLambda(
        [](UWorld* TickedWorld, ELevelTick, float)
        { if (TickedWorld == TestWorld.Get()) RunHouseImpossibleHallwaySmoke(TickedWorld); });
    (void)PostTick;
#else
    (void)World;
#endif
}
