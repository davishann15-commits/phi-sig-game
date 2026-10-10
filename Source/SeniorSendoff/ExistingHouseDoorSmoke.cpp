#include "ExistingHouseDoor.h"

#if !UE_BUILD_SHIPPING
#include "StoryCampaign.h"
#include "HouseReactiveFixture.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void TickExistingHouseDoorSmoke(UWorld* World)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ExistingHouseDoorSmoke"))) return;
    static int32 Step = 0;
    static double Started = 0;
    static double OverallStarted = 0;
    static double WarmupStartedWorld = 0;
    static uint64 WarmupStartedFrame = 0;
    static double CurtainStartedWorld = 0;
    static uint64 CurtainStartedFrame = 0;
    static TWeakObjectPtr<AExistingHouseDoor> TestDoor;
    static TWeakObjectPtr<AExistingHouseDoor> PorchLeft;
    static TWeakObjectPtr<AExistingHouseDoor> PorchRight;
    static int32 PorchPairIndex = 0;
    static int32 BlockedPorchPassages = 0;
    static TArray<TWeakObjectPtr<AExistingHouseDoor>> HousewideTargets;
    static int32 HousewideIndex = 0;
    static int32 InteractionAttempt = 0;
    static int32 LockedGaps = 0;
    static bool bTargetedRun = false;
    static TWeakObjectPtr<AHouseReactiveFixture> AttachedCurtain;
    if (!World || Step < 0) return;
    const FString Map = UGameplayStatics::GetCurrentLevelName(World, true);
    if (Map != TEXT("Chapter01_House")) return;
    auto Fail = [](const TCHAR* Message)
    {
        UE_LOG(LogTemp, Error, TEXT("EXISTING_HOUSE_DOOR_FAILED: %s"), Message);
        FPlatformMisc::RequestExitWithStatus(false, 1);
        Step = -1;
    };
    APlayerController* Controller = World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = Controller ? Cast<AStoryFirstPersonCharacter>(Controller->GetPawn()) : nullptr;
    if (!Pawn) return;
    auto GroupMatches = [World](const AExistingHouseDoor* Target, bool bExpectedOpen)
    {
        if (!Target) return false;
        if (Target->DoorGroup.IsNone()) return Target->IsOpen() == bExpectedOpen;
        int32 Members = 0;
        for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
            if (It->DoorGroup == Target->DoorGroup)
            {
                ++Members;
                if (It->IsOpen() != bExpectedOpen) return false;
            }
        return Members == 2;
    };
    auto PlaceBesideDoor = [Pawn, Controller, World](AExistingHouseDoor* Target, float SignedDistance,
        AHouseReactiveFixture* VisibilityTarget = nullptr)
    {
        FVector Along = Target->DoorwayCenter - Target->GetActorLocation();
        Along.Z = 0.f;
        Along.Normalize();
        const FVector Normal(-Along.Y, Along.X, 0.f);
        FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(ExistingDoorTestFloor), false);
        FloorQuery.AddIgnoredActor(Pawn);
        for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
        {
            FloorQuery.AddIgnoredActor(*It);
            // The pane is spawned as a separate actor attached to the leaf.
            // It is moving door geometry, not a surface supporting the feet.
            if (AHouseReactiveFixture* Pane = It->GetPorchGlassFixture()) FloorQuery.AddIgnoredActor(Pane);
        }
        for (const float Distance : {FMath::Abs(SignedDistance), 175.f, 135.f, 95.f})
        {
            if (Distance > FMath::Abs(SignedDistance) + 1.f) continue;
            FVector Position = Target->DoorwayCenter + Normal * (FMath::Sign(SignedDistance) * Distance);
            const FVector FloorStart(Position.X, Position.Y, Target->GetActorLocation().Z + 180.f);
            const FVector FloorEnd(Position.X, Position.Y, Target->GetActorLocation().Z - 100.f);
            FHitResult FloorHit;
            if (!World->LineTraceSingleByChannel(FloorHit, FloorStart, FloorEnd, ECC_Pawn, FloorQuery) ||
                FloorHit.ImpactNormal.Z < .65f ||
                FMath::Abs(FloorHit.ImpactPoint.Z - Target->GetActorLocation().Z) > 45.f)
                continue;
            Position.Z = FloorHit.ImpactPoint.Z + Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f;
            FCollisionQueryParams SightQuery(SCENE_QUERY_STAT(ExistingDoorTestSight), false);
            SightQuery.AddIgnoredActor(Pawn);
            FHitResult SightHit;
            const FVector ViewLocation = Position + FVector(0, 0, Pawn->BaseEyeHeight);
            const FVector AimPoint = VisibilityTarget
                ? VisibilityTarget->GetFixtureMesh()->Bounds.Origin : Target->DoorwayCenter;
            if (World->LineTraceSingleByChannel(SightHit, ViewLocation, AimPoint,
                    ECC_Visibility, SightQuery))
            {
                if (VisibilityTarget)
                {
                    if (SightHit.GetActor() != VisibilityTarget) continue;
                }
                else
                {
                    const AExistingHouseDoor* HitDoor = Cast<AExistingHouseDoor>(SightHit.GetActor());
                    // Native door focus recognizes the breakable pane owned by
                    // a glazed leaf; use the same rule in this placement check.
                    if (!HitDoor && Cast<AHouseReactiveFixture>(SightHit.GetActor()))
                        HitDoor = Cast<AExistingHouseDoor>(SightHit.GetActor()->GetOwner());
                    if (HitDoor != Target && (!HitDoor || Target->DoorGroup.IsNone() ||
                        HitDoor->DoorGroup != Target->DoorGroup))
                        continue;
                }
            }
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Pawn->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
            Controller->SetControlRotation((AimPoint - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
            UE_LOG(LogTemp, Display, TEXT("HOUSEWIDE_DOOR_TEST_POSITION: %s side=%.0f floor=%s"),
                *Target->DoorId.ToString(), FMath::Sign(SignedDistance) * Distance,
                *FloorHit.ImpactPoint.ToCompactString());
            return true;
        }
        UE_LOG(LogTemp, Error, TEXT("HOUSEWIDE_DOOR_NO_FLOOR: %s side=%.0f"),
            *Target->DoorId.ToString(), SignedDistance);
        return false;
    };
    // This check is used only by the explicitly selected-door run. The trace
    // crosses the closed doorway on the Pawn channel and includes fixed walls,
    // frames and the moving leaf. Real-sized capsule sweeps in both directions
    // also verify the unchanged player fits through the enlarged opening.
    auto PassageLaneMatches = [World, Pawn](AExistingHouseDoor* Target, bool bExpectedClosed)
    {
        if (!Target) return false;
        FVector Along = Target->DoorwayCenter - Target->GetActorLocation();
        Along.Z = 0.f;
        if (!Along.Normalize()) return false;
        const FVector Normal(-Along.Y, Along.X, 0.f);
        FVector ProbeCenter = Target->DoorwayCenter;
        if (!Target->DoorGroup.IsNone())
        {
            // A pair's shared DoorwayCenter lies at the meeting seam, where a
            // zero-width ray can pass between leaves. Test this actual leaf's
            // closed collision center, retaining that same lane when it opens.
            const UBoxComponent* Panel = Target->GetPanelCollision();
            if (!Panel || !Target->GetLeafPivot()) return false;
            FTransform ClosedPivot = Target->GetLeafPivot()->GetRelativeTransform();
            ClosedPivot.SetRotation(FRotator(0.f, Target->ClosedYaw, 0.f).Quaternion());
            ClosedPivot = ClosedPivot * Target->GetRootComponent()->GetComponentTransform();
            ProbeCenter = ClosedPivot.TransformPosition(Panel->GetRelativeLocation());
        }
        FCollisionQueryParams PassageQuery(SCENE_QUERY_STAT(TargetedExistingDoorPassage), false);
        PassageQuery.AddIgnoredActor(Pawn);
        FHitResult PassageHit;
        const bool bHit = World->LineTraceSingleByChannel(PassageHit,
            ProbeCenter - Normal * 80.f,
            ProbeCenter + Normal * 80.f, ECC_Pawn, PassageQuery);
        const AExistingHouseDoor* HitDoor = Cast<AExistingHouseDoor>(PassageHit.GetActor());
        const bool bHitTarget = HitDoor == Target;
        const bool bLineMatches = bExpectedClosed ? bHit && bHitTarget : !bHit;
        UE_LOG(LogTemp, Display,
            TEXT("TARGETED_EXISTING_DOOR_PASSAGE: %s state=%s passed=%d hit=%s component=%s point=%s lane=%s"),
            *Target->DoorId.ToString(), bExpectedClosed ? TEXT("closed") : TEXT("open"), bLineMatches,
            *GetNameSafe(PassageHit.GetActor()), *GetNameSafe(PassageHit.GetComponent()),
            *PassageHit.ImpactPoint.ToCompactString(), *ProbeCenter.ToCompactString());
        if (!bLineMatches) return false;
        FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(TargetedExistingDoorFloor), false, Pawn);
        for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
        {
            FloorQuery.AddIgnoredActor(*It);
            if (AHouseReactiveFixture* Pane = It->GetPorchGlassFixture()) FloorQuery.AddIgnoredActor(Pane);
        }
        double HighestFloor = -double(BIG_NUMBER);
        for (float Offset : {-80.f, 0.f, 80.f})
        {
            FVector Sample = ProbeCenter + Normal * Offset;
            Sample.Z = Target->GetActorLocation().Z;
            FHitResult Floor;
            const FVector FloorStart = Sample + FVector(0, 0, 45.f);
            const FVector FloorEnd = Sample - FVector(0, 0, 75.f);
            const bool bRayHit = World->LineTraceSingleByChannel(Floor, FloorStart, FloorEnd, ECC_Pawn, FloorQuery);
            const FHitResult RayFloor = Floor;
            const bool bRayWalkable = bRayHit && !Floor.bStartPenetrating && Floor.ImpactNormal.Z >= .65f
                && Pawn->GetCharacterMovement()->IsWalkable(Floor);
            bool bFloorHit = bRayHit;
            bool bFootprintSupport = false;
            const float FootRadius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
            if (!bRayWalkable)
            {
                // A narrow threshold gap can miss a zero-width ray while the
                // character's rounded capsule foot remains on its floor edges.
                // Match that actual footprint, rather than inventing a floor.
                const FVector SphereLift(0, 0, FootRadius);
                Floor = FHitResult();
                bFloorHit = World->SweepSingleByChannel(Floor, FloorStart + SphereLift,
                    FloorEnd + SphereLift, FQuat::Identity, ECC_Pawn,
                    FCollisionShape::MakeSphere(FootRadius), FloorQuery);
                bFootprintSupport = true;
            }
            const bool bWalkable = bFloorHit && !Floor.bStartPenetrating && Floor.ImpactNormal.Z >= .65f
                && Pawn->GetCharacterMovement()->IsWalkable(Floor);
            UE_LOG(LogTemp, Display,
                TEXT("TARGETED_EXISTING_DOOR_FLOOR_SAMPLE: %s state=%s offset=%.0f passed=%d mode=%s rayHit=%d hit=%d actor=%s component=%s point=%s normal=%s footRadius=%.1f startPenetrating=%d"),
                *Target->DoorId.ToString(), bExpectedClosed ? TEXT("closed") : TEXT("open"), Offset,
                bWalkable, bFootprintSupport ? TEXT("capsule_foot") : TEXT("line"), bRayHit, bFloorHit,
                *GetNameSafe(Floor.GetActor()), *GetNameSafe(Floor.GetComponent()),
                *Floor.ImpactPoint.ToCompactString(), *Floor.ImpactNormal.ToCompactString(), FootRadius,
                Floor.bStartPenetrating);
            if (!bWalkable)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("TARGETED_EXISTING_DOOR_FLOOR_FAILED: %s state=%s offset=%.0f hit=%d actor=%s component=%s point=%s normal=%s startPenetrating=%d from=%s to=%s rayActor=%s rayComponent=%s rayPoint=%s rayNormal=%s"),
                    *Target->DoorId.ToString(), bExpectedClosed ? TEXT("closed") : TEXT("open"), Offset,
                    bFloorHit, *GetNameSafe(Floor.GetActor()), *GetNameSafe(Floor.GetComponent()),
                    *Floor.ImpactPoint.ToCompactString(), *Floor.ImpactNormal.ToCompactString(),
                    Floor.bStartPenetrating, *FloorStart.ToCompactString(), *FloorEnd.ToCompactString(),
                    *GetNameSafe(RayFloor.GetActor()), *GetNameSafe(RayFloor.GetComponent()),
                    *RayFloor.ImpactPoint.ToCompactString(), *RayFloor.ImpactNormal.ToCompactString());
                return false;
            }
            HighestFloor = FMath::Max(HighestFloor, Floor.ImpactPoint.Z);
        }
        FVector CapsuleCenter = ProbeCenter;
        CapsuleCenter.Z = HighestFloor + Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f;
        const FCollisionShape Capsule = FCollisionShape::MakeCapsule(
            Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),
            Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        for (float Side : {-1.f, 1.f})
        {
            FHitResult CapsuleHit;
            const bool bBlocked = World->SweepSingleByChannel(CapsuleHit,
                CapsuleCenter + Normal * (Side * 80.f), CapsuleCenter - Normal * (Side * 80.f),
                FQuat::Identity, ECC_Pawn, Capsule, PassageQuery);
            const AExistingHouseDoor* CapsuleDoor = Cast<AExistingHouseDoor>(CapsuleHit.GetActor());
            const bool bDoorBlocks = CapsuleDoor == Target || (CapsuleDoor && !Target->DoorGroup.IsNone()
                && CapsuleDoor->DoorGroup == Target->DoorGroup);
            const bool bMatches = bExpectedClosed ? bBlocked && bDoorBlocks : !bBlocked;
            UE_LOG(LogTemp, Display,
                TEXT("TARGETED_EXISTING_DOOR_CAPSULE: %s state=%s side=%.0f passed=%d hit=%s center=%s radius=%.1f halfHeight=%.1f"),
                *Target->DoorId.ToString(), bExpectedClosed ? TEXT("closed") : TEXT("open"), Side, bMatches,
                *GetNameSafe(CapsuleHit.GetActor()), *CapsuleCenter.ToCompactString(),
                Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),
                Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            if (!bMatches) return false;
        }
        return true;
    };
    auto TargetedPassageMatches = [World, &PassageLaneMatches](AExistingHouseDoor* Target, bool bExpectedClosed)
    {
        if (!Target || !PassageLaneMatches(Target, bExpectedClosed)) return false;
        if (!Target->DoorGroup.IsNone())
            for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
                if (*It != Target && It->DoorGroup == Target->DoorGroup
                    && !PassageLaneMatches(*It, bExpectedClosed)) return false;
        return true;
    };
    if (!OverallStarted)
    {
        OverallStarted = Started = FPlatformTime::Seconds();
        WarmupStartedWorld = World->GetTimeSeconds();
        WarmupStartedFrame = GFrameCounter;
    }
    if (FPlatformTime::Seconds() - OverallStarted > 240) { Fail(TEXT("Timed out")); return; }

    if (Step == 0)
    {
        // Initial shader/asset work can spend seconds in just a few frames.
        // Wait for actual simulation and rendered frames before timing input.
        if (!World->GetGameViewport() || GFrameCounter - WarmupStartedFrame < 64
            || World->GetTimeSeconds() - WarmupStartedWorld < 1.0) return;
        UE_LOG(LogTemp, Display, TEXT("EXISTING_HOUSE_DOOR_WARMUP: frames=%llu worldSeconds=%.2f wallSeconds=%.2f"),
            static_cast<unsigned long long>(GFrameCounter - WarmupStartedFrame),
            World->GetTimeSeconds() - WarmupStartedWorld, FPlatformTime::Seconds() - OverallStarted);
        FString RequestedTargets;
        // Do not stop FParse at commas: this option is a comma-separated ID list.
        if (FParse::Value(FCommandLine::Get(), TEXT("ExistingHouseDoorTargets="), RequestedTargets, false))
        {
            bTargetedRun = true;
            TArray<FString> TargetNames;
            RequestedTargets.ParseIntoArray(TargetNames, TEXT(","), false);
            if (TargetNames.IsEmpty())
            { Fail(TEXT("ExistingHouseDoorTargets requires at least one named door")); return; }
            TSet<FName> RequestedIds;
            HousewideTargets.Reset();
            HousewideIndex = 0;
            for (FString& Name : TargetNames)
            {
                Name.TrimStartAndEndInline();
                const FName DoorId(*Name);
                if (Name.IsEmpty() || DoorId.IsNone() || RequestedIds.Contains(DoorId))
                { Fail(TEXT("Targeted door IDs must be named and unique")); return; }
                RequestedIds.Add(DoorId);
                AExistingHouseDoor* Target = nullptr;
                for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
                    if (It->DoorId == DoorId)
                    {
                        if (Target)
                        { Fail(TEXT("A targeted door ID resolves to more than one actor")); return; }
                        Target = *It;
                    }
                if (!Target || Target->bLocked || !Target->OriginalLeaf)
                {
                    UE_LOG(LogTemp, Error, TEXT("TARGETED_EXISTING_DOOR_INVALID: %s actor=%s locked=%d leaf=%s"),
                        *Name, *GetNameSafe(Target), Target ? Target->bLocked : false,
                        Target ? *GetNameSafe(Target->OriginalLeaf.Get()) : TEXT("none"));
                    Fail(TEXT("A targeted door is missing, locked or has no leaf asset"));
                    return;
                }
                HousewideTargets.Add(Target);
            }
            UE_LOG(LogTemp, Display, TEXT("TARGETED_EXISTING_DOOR_START: targets=%d ids=%s"),
                HousewideTargets.Num(), *RequestedTargets);
            // The targeted run does not require an old house-wide inventory,
            // the entrance TestDoor, or porch pairs. Reuse the deliberate E cycle.
            Started = FPlatformTime::Seconds();
            Step = 20;
            return;
        }
        int32 Total = 0, Locked = 0, Porch = 0, AddedHandles = 0;
        TSet<FName> SeenGroups;
        TSet<FName> SeenDoorIds;
        for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
        {
            ++Total;
            if (It->DoorId.IsNone() || SeenDoorIds.Contains(It->DoorId))
            { Fail(TEXT("A house door is unnamed or duplicates another door ID")); return; }
            SeenDoorIds.Add(It->DoorId);
            if (It->bLocked)
            {
                ++Locked;
                UE_LOG(LogTemp, Warning, TEXT("EXISTING_HOUSE_DOOR_LOCKED_GAP: %s"), *It->DoorId.ToString());
            }
            else if (It->DoorGroup.IsNone() || !SeenGroups.Contains(It->DoorGroup))
            {
                HousewideTargets.Add(*It);
                if (!It->DoorGroup.IsNone()) SeenGroups.Add(It->DoorGroup);
            }
            if (It->DoorId.ToString().StartsWith(TEXT("Porch_")))
            {
                ++Porch;
                if (!It->OriginalGlass || It->DoorGroup.IsNone())
                { Fail(TEXT("A porch panel is missing its original glass or pair")); return; }
            }
            if (It->AddedHandleMesh) ++AddedHandles;
            if (!It->OriginalLeaf) { Fail(TEXT("An extracted original leaf asset is missing")); return; }
            if (It->DoorId == TEXT("SM_OriginalLeaf_Entrance_m1_m1_Solid_01")) TestDoor = *It;
        }
        LockedGaps = Locked;
        if (Total < 39 || Porch != 8 || AddedHandles < 13 || !TestDoor.IsValid())
        { Fail(TEXT("Original room and porch door inventory or hardware is incomplete")); return; }
        FCollisionQueryParams PassageQuery(SCENE_QUERY_STAT(ExistingHousePassage), false);
        PassageQuery.AddIgnoredActor(Pawn);
        for (TActorIterator<AExistingHouseDoor> It(World); It; ++It) PassageQuery.AddIgnoredActor(*It);
        for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
        {
            if (It->bLocked) continue;
            FVector Along = It->DoorwayCenter - It->GetActorLocation();
            Along.Z = 0;
            if (!Along.Normalize()) continue;
            const FVector Normal(-Along.Y, Along.X, 0);
            FHitResult PassageHit;
            if (World->LineTraceSingleByChannel(PassageHit, It->DoorwayCenter - Normal * 80.f,
                It->DoorwayCenter + Normal * 80.f, ECC_Pawn, PassageQuery))
            {
                UE_LOG(LogTemp, Warning, TEXT("EXISTING_DOOR_PASSAGE_BLOCKED %s by %s at %s"),
                    *It->DoorId.ToString(), *GetNameSafe(PassageHit.GetActor()),
                    *PassageHit.ImpactPoint.ToCompactString());
                if (It->DoorId.ToString().StartsWith(TEXT("Porch_")))
                { Fail(TEXT("A fixed collision mesh still blocks a porch entrance")); return; }
            }
        }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ExistingHouseDoorClosed), false);
        Query.AddIgnoredActor(Pawn);
        const FVector Center = TestDoor->DoorwayCenter;
        FHitResult Hit;
        const bool bHit = World->LineTraceSingleByChannel(Hit, Center - FVector(100, 0, 0),
            Center + FVector(100, 0, 0), ECC_Pawn, Query);
        UE_LOG(LogTemp, Display, TEXT("EXISTING_DOOR_COLLISION_CLOSED hit=%d actor=%s component=%s point=%s"),
            bHit, *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), *Hit.ImpactPoint.ToCompactString());
        if (!bHit || Hit.GetActor() != TestDoor.Get())
        { Fail(TEXT("The original hall leaf does not block its closed doorway")); return; }
        FVector Near = TestDoor->DoorwayCenter + FVector(100, 0, 0);
        Near.Z = TestDoor->GetActorLocation().Z + Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Pawn->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
        Started = FPlatformTime::Seconds();
        Step = 1;
        return;
    }
    if (Step == 1)
    {
        if (FPlatformTime::Seconds() - Started < .25) return;
        if (TestDoor->IsOpen()) { Fail(TEXT("Room door opened when the player merely approached")); return; }
        Controller->SetControlRotation((TestDoor->DoorwayCenter - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        Started = FPlatformTime::Seconds();
        Step = 15;
        return;
    }
    if (Step == 15)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        FCollisionQueryParams FocusQuery(SCENE_QUERY_STAT(ExistingDoorFocusSmoke), false);
        FocusQuery.AddIgnoredActor(Pawn);
        FHitResult FocusHit;
        const FVector View = Pawn->FirstPersonCamera->GetComponentLocation();
        World->LineTraceSingleByChannel(FocusHit, View, TestDoor->DoorwayCenter,
            ECC_Visibility, FocusQuery);
        UE_LOG(LogTemp, Display, TEXT("EXISTING_DOOR_FOCUS_DEBUG target=%s selected=%s hit=%s camera=%s targetAt=%s facing=%.3f"),
            *TestDoor->DoorId.ToString(), *GetNameSafe(Pawn->FocusedDoorForInteraction()),
            *GetNameSafe(FocusHit.GetActor()), *View.ToCompactString(),
            *TestDoor->DoorwayCenter.ToCompactString(),
            FVector::DotProduct(Pawn->FirstPersonCamera->GetForwardVector(),
                (TestDoor->DoorwayCenter - View).GetSafeNormal()));
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
        Started = FPlatformTime::Seconds();
        Step = 151;
        return;
    }
    if (Step == 151)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        if (!TestDoor->IsOpen()) { Fail(TEXT("E did not open the focused room door")); return; }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ExistingHouseDoorOpening), false);
        Query.AddIgnoredActor(Pawn);
        const FVector Center = TestDoor->DoorwayCenter;
        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit, Center - FVector(100, 0, 0),
                Center + FVector(100, 0, 0), ECC_Pawn, Query) || Hit.GetActor() != TestDoor.Get())
        { Fail(TEXT("Opening room door became intangible before it moved clear")); return; }
        Started = FPlatformTime::Seconds();
        Step = 11;
        return;
    }
    if (Step == 11)
    {
        if (FPlatformTime::Seconds() - Started < .65) return;
        if (TestDoor->IsActorTickEnabled())
        { Fail(TEXT("Settled room door still ticks every frame")); return; }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ExistingHouseDoorOpen), false);
        Query.AddIgnoredActor(Pawn);
        const FVector Center = TestDoor->DoorwayCenter;
        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, Center - FVector(100, 0, 0),
                Center + FVector(100, 0, 0), ECC_Pawn, Query))
        { Fail(TEXT("The original hall doorway remains blocked after opening")); return; }
        FVector Clear = TestDoor->DoorwayCenter - FVector(150, 0, 0);
        Clear.Z = TestDoor->GetActorLocation().Z + Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Pawn->SetActorLocation(Clear, false, nullptr, ETeleportType::TeleportPhysics);
        Controller->SetControlRotation((TestDoor->DoorwayCenter - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        Started = FPlatformTime::Seconds();
        Step = 16;
        return;
    }
    if (Step == 16)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
        Started = FPlatformTime::Seconds();
        Step = 161;
        return;
    }
    if (Step == 161)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        if (TestDoor->IsOpen())
        { Fail(TEXT("E could not close the room door from its other side")); return; }
        Started = FPlatformTime::Seconds();
        Step = 2;
        return;
    }
    if (Step == 2)
    {
        if (FPlatformTime::Seconds() - Started < .25) return;
        if (TestDoor->IsOpen()) { Fail(TEXT("Door reopened without a new approach")); return; }
        AExistingHouseDoor* LockedDoor = nullptr;
        for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
            if (It->bLocked) { LockedDoor = *It; break; }
        if (!LockedDoor) { Fail(TEXT("Locked original door missing")); return; }
        if (LockedDoor->Interact(Pawn) || LockedDoor->IsOpen())
        { Fail(TEXT("Door to an unbuilt area opened")); return; }
        Controller->SetControlRotation((TestDoor->DoorwayCenter - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        Started = FPlatformTime::Seconds();
        Step = 3;
        return;
    }
    if (Step == 3)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        Controller->InputKey(FInputKeyParams(EKeys::LeftMouseButton, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::LeftMouseButton, IE_Released, 0.0));
        if (TestDoor->IsOpen()) { Fail(TEXT("One click opened a door")); return; }
        Started = FPlatformTime::Seconds();
        Step = 4;
        return;
    }
    if (Step == 4)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        Controller->InputKey(FInputKeyParams(EKeys::LeftMouseButton, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::LeftMouseButton, IE_Released, 0.0));
        Started = FPlatformTime::Seconds();
        Step = 5;
        return;
    }
    if (Step == 5)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        if (!TestDoor->IsOpen()) { Fail(TEXT("Double-click did not open the focused room door")); return; }
        Controller->InputKey(FInputKeyParams(EKeys::LeftMouseButton, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::LeftMouseButton, IE_Released, 0.0));
        Started = FPlatformTime::Seconds();
        Step = 9;
        return;
    }
    if (Step == 9)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        Controller->InputKey(FInputKeyParams(EKeys::LeftMouseButton, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::LeftMouseButton, IE_Released, 0.0));
        Started = FPlatformTime::Seconds();
        Step = 10;
        return;
    }
    if (Step == 10)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        if (TestDoor->IsOpen()) { Fail(TEXT("Double-click did not close the focused room door")); return; }
        PorchPairIndex = 0;
        Step = 6;
    }
    if (Step == 6)
    {
        if (PorchPairIndex == 4)
        {
            if (BlockedPorchPassages)
            { Fail(TEXT("One or more player-sized porch passages are blocked")); return; }
            Step = 20;
            return;
        }
        static const TCHAR* Groups[] = {
            TEXT("Porch_p0_m1_1"), TEXT("Porch_p0_m1_2"),
            TEXT("Porch_p0_p0_1"), TEXT("Porch_p0_p0_2")
        };
        PorchLeft.Reset(); PorchRight.Reset();
        for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
            if (It->DoorGroup == FName(Groups[PorchPairIndex]))
            {
                if (!PorchLeft.IsValid()) PorchLeft = *It;
                else PorchRight = *It;
            }
        if (!PorchLeft.IsValid() || !PorchRight.IsValid())
        { Fail(TEXT("A porch pair is missing")); return; }
        FVector Near = PorchLeft->DoorwayCenter - FVector(0, 100, 0);
        Near.Z = PorchLeft->GetActorLocation().Z + Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Pawn->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
        Controller->SetControlRotation((PorchLeft->DoorwayCenter - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        Started = FPlatformTime::Seconds();
        Step = 7;
        return;
    }
    if (Step == 7)
    {
        if (FPlatformTime::Seconds() - Started < .25) return;
        if (PorchLeft->IsOpen() || PorchRight->IsOpen())
        { Fail(TEXT("Porch doors opened without player interaction")); return; }
        FCollisionQueryParams ClosedQuery(SCENE_QUERY_STAT(PorchDoorBeforeInteraction), false);
        ClosedQuery.AddIgnoredActor(Pawn);
        for (AExistingHouseDoor* Panel : {PorchLeft.Get(), PorchRight.Get()})
        {
            const float PanelX = (Panel->GetActorLocation().X + Panel->DoorwayCenter.X) * .5f;
            FHitResult ClosedHit;
            World->LineTraceSingleByChannel(ClosedHit,
                FVector(PanelX, Panel->DoorwayCenter.Y - 100.f, Panel->DoorwayCenter.Z),
                FVector(PanelX, Panel->DoorwayCenter.Y + 100.f, Panel->DoorwayCenter.Z),
                ECC_Pawn, ClosedQuery);
            UE_LOG(LogTemp, Display, TEXT("PORCH_CLOSED_COLLISION_DEBUG panel=%s hit=%s component=%s"),
                *Panel->DoorId.ToString(), *GetNameSafe(ClosedHit.GetActor()), *GetNameSafe(ClosedHit.GetComponent()));
        }
        const FKey InteractionKey = (PorchPairIndex % 2) ? EKeys::Gamepad_FaceButton_Left : EKeys::E;
        Controller->InputKey(FInputKeyParams(InteractionKey, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(InteractionKey, IE_Released, 0.0));
        Started = FPlatformTime::Seconds();
        Step = 71;
        return;
    }
    if (Step == 71)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        if (!PorchLeft->IsOpen() || !PorchRight->IsOpen())
        { Fail(TEXT("Interaction did not open both original porch panels")); return; }
        Started = FPlatformTime::Seconds();
        Step = 17;
        return;
    }
    if (Step == 17)
    {
        FCollisionQueryParams Query(SCENE_QUERY_STAT(PorchDoorOpening), false);
        Query.AddIgnoredActor(Pawn);
        for (AExistingHouseDoor* Panel : {PorchLeft.Get(), PorchRight.Get()})
        {
            TArray<UBoxComponent*> Boxes;
            Panel->GetComponents<UBoxComponent>(Boxes);
            UBoxComponent* Box = nullptr;
            for (UBoxComponent* Candidate : Boxes)
                if (Candidate && Candidate->GetFName() == TEXT("MovingPanelCollision"))
                { Box = Candidate; break; }
            if (!Box || Box->GetCollisionEnabled() == ECollisionEnabled::NoCollision ||
                Box->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
            {
                Fail(TEXT("Opening porch panel has no solid player collision"));
                return;
            }
            const FVector Center = Box->GetComponentLocation();
            const FVector Across = Box->GetRightVector() * 30.f;
            FHitResult Hit;
            if (!Box->LineTraceComponent(Hit, Center - Across, Center + Across, Query))
            {
                UE_LOG(LogTemp, Error, TEXT("PORCH_OPENING_COLLISION_MISSING panel=%s mode=%d center=%s yaw=%.1f"),
                    *Panel->DoorId.ToString(), int32(Box->GetCollisionEnabled()),
                    *Center.ToCompactString(), Box->GetComponentRotation().Yaw);
                Fail(TEXT("Opening porch panel became intangible before it moved clear"));
                return;
            }
        }
        Step = 12;
        return;
    }
    if (Step == 12)
    {
        if (FPlatformTime::Seconds() - Started < .65) return;
        if (PorchLeft->IsActorTickEnabled() || PorchRight->IsActorTickEnabled())
        { Fail(TEXT("Settled porch panels still tick every frame")); return; }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(PorchDoorOpen), false);
        Query.AddIgnoredActor(Pawn);
        FHitResult Hit;
        const FVector Center = PorchLeft->DoorwayCenter;
        if (World->LineTraceSingleByChannel(Hit, Center - FVector(0, 100, 0),
                Center + FVector(0, 100, 0), ECC_Pawn, Query))
        { Fail(TEXT("An opened porch entrance is still blocked")); return; }
        FVector PassageStart = Center - FVector(0, 135, 0);
        FVector PassageEnd = Center + FVector(0, 135, 0);
        PassageStart.Z = PassageEnd.Z = PorchLeft->GetActorLocation().Z + Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f;
        bool bTraversable = false;
        for (float Side : {-105.f, -70.f, 0.f, 70.f, 105.f})
        {
            PassageStart.X = PassageEnd.X = Center.X + Side;
            const bool bBlocked = World->SweepSingleByChannel(Hit, PassageStart, PassageEnd, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),
                    Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), Query);
            if (PorchPairIndex == 0)
                UE_LOG(LogTemp, Display, TEXT("PORCH_CAPSULE_LANE group=%s side=%.1f blocked=%d actor=%s"),
                    *PorchLeft->DoorGroup.ToString(), Side, bBlocked, *GetNameSafe(Hit.GetActor()));
            if (!bBlocked) { bTraversable = true; break; }
        }
        if (!bTraversable)
        {
            UE_LOG(LogTemp, Error, TEXT("PORCH_CAPSULE_BLOCKED group=%s actor=%s point=%s"),
                *PorchLeft->DoorGroup.ToString(), *GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToString());
            ++BlockedPorchPassages;
        }
        FVector Clear = Center + FVector(0, 220, 0);
        Clear.Z = PorchLeft->GetActorLocation().Z + Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Pawn->SetActorLocation(Clear, false, nullptr, ETeleportType::TeleportPhysics);
        Controller->SetControlRotation((Center - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        Started = FPlatformTime::Seconds();
        Step = 18;
        return;
    }
    if (Step == 18)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
        Started = FPlatformTime::Seconds();
        Step = 181;
        return;
    }
    if (Step == 181)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        if (PorchLeft->IsOpen() || PorchRight->IsOpen())
        { Fail(TEXT("E did not close the porch pair from outside")); return; }
        Started = FPlatformTime::Seconds();
        Step = 8;
        return;
    }
    if (Step == 8)
    {
        if (FPlatformTime::Seconds() - Started < .65) return;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(PorchDoorClosed), false);
        Query.AddIgnoredActor(Pawn);
        for (AExistingHouseDoor* Door : {PorchLeft.Get(), PorchRight.Get()})
        {
            const float X = (Door->GetActorLocation().X + Door->DoorwayCenter.X) * .5f;
            const FVector A(X, Door->DoorwayCenter.Y - 100.f, Door->DoorwayCenter.Z);
            const FVector B(X, Door->DoorwayCenter.Y + 100.f, Door->DoorwayCenter.Z);
            FHitResult Hit;
            if (!World->LineTraceSingleByChannel(Hit, A, B, ECC_Pawn, Query) || Hit.GetActor() != Door)
            { Fail(TEXT("A closed porch glass panel does not block passage")); return; }
        }
        ++PorchPairIndex;
        Step = 6;
    }
    if (Step == 20)
    {
        if (HousewideIndex == HousewideTargets.Num())
        {
            if (bTargetedRun)
            {
                UE_LOG(LogTemp, Display, TEXT("EXISTING_HOUSE_DOOR_PASSED: targeted=%d named unlocked doorways; E open, close from opposite side, reopen and close; settled closed/open Pawn passage lines and real-sized capsule sweeps in both directions verified"),
                    HousewideTargets.Num());
            }
            else
            {
                UE_LOG(LogTemp, Display, TEXT("EXISTING_HOUSE_DOOR_PASSED: %d usable doorways opened, closed and reopened with E; %d locked door gaps; double-click, gamepad, collision and porch passages verified"),
                    HousewideTargets.Num(), LockedGaps);
            }
            FPlatformMisc::RequestExitWithStatus(false, 0);
            Step = -1;
            return;
        }
        AExistingHouseDoor* Target = HousewideTargets[HousewideIndex].Get();
        if (!Target || !GroupMatches(Target, false))
        { Fail(TEXT("Housewide target missing or unexpectedly open")); return; }
        if (bTargetedRun && !TargetedPassageMatches(Target, true))
        { Fail(TEXT("A targeted closed door does not block its own passage")); return; }
        // The navy cloth has its own normal E interaction. Gather it first so
        // the later E presses exercise the actual door rather than the curtain.
        AttachedCurtain.Reset();
        for (TActorIterator<AHouseReactiveFixture> It(World); It; ++It)
            if (It->FixtureKind == EHouseReactiveFixtureKind::Curtain && !It->IsOpen()
                && It->GetRootComponent()->GetAttachParent() == Target->GetLeafPivot())
            {
                AttachedCurtain = *It;
                break;
            }
        if (AttachedCurtain.IsValid())
        {
            if (!PlaceBesideDoor(Target, -220.f, AttachedCurtain.Get())
                && !PlaceBesideDoor(Target, 220.f, AttachedCurtain.Get()))
            { Fail(TEXT("Attached door curtain has no visible safe approach")); return; }
            Started = FPlatformTime::Seconds();
            CurtainStartedWorld = World->GetTimeSeconds();
            CurtainStartedFrame = GFrameCounter;
            Step = 201;
            return;
        }
        InteractionAttempt = 0;
        if (!PlaceBesideDoor(Target, 220.f))
        { Fail(TEXT("Usable door has no safe approach floor")); return; }
        Started = FPlatformTime::Seconds();
        Step = 21;
        return;
    }
    if (Step == 201)
    {
        if (World->GetTimeSeconds() - CurtainStartedWorld < .12
            || GFrameCounter - CurtainStartedFrame < 2) return;
        if (!AttachedCurtain.IsValid() || Pawn->FocusedHouseFixture() != AttachedCurtain.Get())
        { Fail(TEXT("Native E focus did not find the attached door curtain")); return; }
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
        Started = FPlatformTime::Seconds();
        CurtainStartedWorld = World->GetTimeSeconds();
        CurtainStartedFrame = GFrameCounter;
        Step = 202;
        return;
    }
    if (Step == 202)
    {
        const double SimulationAge = World->GetTimeSeconds() - CurtainStartedWorld;
        const double WallAge = FPlatformTime::Seconds() - Started;
        const uint64 Frames = GFrameCounter - CurtainStartedFrame;
        if (!AttachedCurtain.IsValid())
        { Fail(TEXT("Attached door curtain disappeared during gathering")); return; }
        if (SimulationAge < .12 || Frames < 2) return;
        if (!AttachedCurtain->IsOpen() || AttachedCurtain->GetCurtainProgress() < .95f)
        {
            if (SimulationAge > 5.0 || WallAge > 30.0 || Frames > 900)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("TARGETED_EXISTING_DOOR_CURTAIN_TIMEOUT: open=%d progress=%.3f frames=%llu worldSeconds=%.2f wallSeconds=%.2f"),
                    AttachedCurtain->IsOpen(), AttachedCurtain->GetCurtainProgress(),
                    static_cast<unsigned long long>(Frames), SimulationAge, WallAge);
                Fail(TEXT("Native E did not gather the attached door curtain"));
            }
            return;
        }
        UE_LOG(LogTemp, Display,
            TEXT("TARGETED_EXISTING_DOOR_CURTAIN_GATHERED: %s normal_E=1 progress=%.3f frames=%llu worldSeconds=%.2f"),
            *AttachedCurtain->FixtureId.ToString(), AttachedCurtain->GetCurtainProgress(),
            static_cast<unsigned long long>(Frames), SimulationAge);
        Step = 20;
        return;
    }
    if (Step == 21)
    {
        if (FPlatformTime::Seconds() - Started < .12) return;
        AExistingHouseDoor* Target = HousewideTargets[HousewideIndex].Get();
        if (!Target) { Fail(TEXT("Housewide target disappeared")); return; }
        if (!GroupMatches(Target, false))
        { Fail(TEXT("Door opened before housewide interaction")); return; }
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
        Started = FPlatformTime::Seconds();
        Step = 211;
        return;
    }
    if (Step == 211)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        AExistingHouseDoor* Target = HousewideTargets[HousewideIndex].Get();
        if (!GroupMatches(Target, true))
        {
            if (InteractionAttempt++ == 0)
            {
                if (!PlaceBesideDoor(Target, -220.f))
                { Fail(TEXT("Usable door has no safe approach floor on its other side")); return; }
                Started = FPlatformTime::Seconds();
                Step = 21;
                return;
            }
            UE_LOG(LogTemp, Error, TEXT("HOUSEWIDE_DOOR_UNREACHABLE: %s"), *Target->DoorId.ToString());
            Fail(TEXT("Could not deliberately open a usable door from either side"));
            return;
        }
        Started = FPlatformTime::Seconds();
        Step = 22;
        return;
    }
    if (Step == 22)
    {
        if (FPlatformTime::Seconds() - Started < .65) return;
        AExistingHouseDoor* Target = HousewideTargets[HousewideIndex].Get();
        if (bTargetedRun && !TargetedPassageMatches(Target, false))
        { Fail(TEXT("A targeted passage remains blocked after E opens its door")); return; }
        if (!PlaceBesideDoor(Target, InteractionAttempt == 0 ? -220.f : 220.f))
        { Fail(TEXT("Usable door has no safe floor for closing from its other side")); return; }
        Started = FPlatformTime::Seconds();
        Step = 23;
        return;
    }
    if (Step == 23)
    {
        if (FPlatformTime::Seconds() - Started < .12) return;
        AExistingHouseDoor* Target = HousewideTargets[HousewideIndex].Get();
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
        Started = FPlatformTime::Seconds();
        Step = 231;
        return;
    }
    if (Step == 231)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        AExistingHouseDoor* Target = HousewideTargets[HousewideIndex].Get();
        if (!GroupMatches(Target, false))
        {
            const AExistingHouseDoor* Focused = Pawn->FocusedDoorForInteraction();
            FCollisionQueryParams VisibilityQuery(SCENE_QUERY_STAT(ExistingDoorFailureTrace), false);
            VisibilityQuery.AddIgnoredActor(Pawn);
            FHitResult VisibilityHit;
            World->LineTraceSingleByChannel(VisibilityHit,
                Pawn->FirstPersonCamera->GetComponentLocation(), Target->DoorwayCenter,
                ECC_Visibility, VisibilityQuery);
            UE_LOG(LogTemp, Error, TEXT("HOUSEWIDE_DOOR_CLOSE_FAILED: %s target=%s focus=%s focusId=%s obstruction=%s pawn=%s center=%s"),
                *Target->DoorId.ToString(), *GetNameSafe(Target), *GetNameSafe(Focused),
                Focused ? *Focused->DoorId.ToString() : TEXT("none"),
                *GetNameSafe(VisibilityHit.GetActor()),
                *Pawn->GetActorLocation().ToCompactString(), *Target->DoorwayCenter.ToCompactString());
            Fail(TEXT("Could not close a usable door from its other side"));
            return;
        }
        Started = FPlatformTime::Seconds();
        Step = 24;
        return;
    }
    if (Step == 24)
    {
        if (FPlatformTime::Seconds() - Started < .65) return;
        AExistingHouseDoor* Target = HousewideTargets[HousewideIndex].Get();
        if (bTargetedRun && !TargetedPassageMatches(Target, true))
        { Fail(TEXT("A targeted passage is not blocked after closing from the opposite side")); return; }
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
        Started = FPlatformTime::Seconds();
        Step = 241;
        return;
    }
    if (Step == 241)
    {
        if (FPlatformTime::Seconds() - Started < .1) return;
        AExistingHouseDoor* Target = HousewideTargets[HousewideIndex].Get();
        if (!GroupMatches(Target, true))
        {
            UE_LOG(LogTemp, Error, TEXT("HOUSEWIDE_DOOR_REOPEN_FAILED: %s"), *Target->DoorId.ToString());
            Fail(TEXT("Could not reopen a usable door from its other side"));
            return;
        }
        Started = FPlatformTime::Seconds();
        Step = 25;
        return;
    }
    if (Step == 25)
    {
        if (FPlatformTime::Seconds() - Started < .65) return;
        AExistingHouseDoor* Target = HousewideTargets[HousewideIndex].Get();
        if (bTargetedRun && !TargetedPassageMatches(Target, false))
        { Fail(TEXT("A targeted passage remains blocked after reopening")); return; }
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
        Started = FPlatformTime::Seconds();
        Step = 251;
        return;
    }
    if (Step == 251)
    {
        // Targeted collision assertions must wait for the final swing to settle.
        if (FPlatformTime::Seconds() - Started < (bTargetedRun ? .65 : .1)) return;
        AExistingHouseDoor* Target = HousewideTargets[HousewideIndex].Get();
        if (!GroupMatches(Target, false))
        { Fail(TEXT("Housewide door could not return to its closed state")); return; }
        if (bTargetedRun && !TargetedPassageMatches(Target, true))
        { Fail(TEXT("A targeted door does not block passage after its final close")); return; }
        UE_LOG(LogTemp, Display, TEXT("HOUSEWIDE_DOOR_CYCLE_PASSED: %s (%d/%d)"),
            *Target->DoorId.ToString(), HousewideIndex + 1, HousewideTargets.Num());
        ++HousewideIndex;
        Step = 20;
    }
}
#endif
