#include "Room101EntrySmoke.h"

#if !UE_BUILD_SHIPPING
#include "ExistingHouseDoor.h"
#include "HouseWorldScale.h"
#include "HouseInteractiveProp.h"
#include "HouseReactiveFixture.h"
#include "StoryCampaign.h"
#include "StoryMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void TickRoom101EntrySmoke(UWorld* World)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("Room101EntrySmoke"))) return;
    static int32 Stage = 0;
    static double WallStarted = 0, WarmWorld = 0, ActionWorld = 0, LastProgressWorld = 0;
    static uint64 WarmFrame = 0;
    static TWeakObjectPtr<AExistingHouseDoor> Door101, Door102;
    static TArray<FVector> Route;
    static int32 Waypoint = 0;
    static float BestDistance = BIG_NUMBER, Radius = 0, HalfHeight = 0;
    static FVector CharacterScale = FVector::OneVector, ClosedWalkStart = FVector::ZeroVector;
    if (!World || Stage < 0 || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    APlayerController* Controller = World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = Controller ? Cast<AStoryFirstPersonCharacter>(Controller->GetPawn()) : nullptr;
    if (!Pawn || !Pawn->FirstPersonCamera || !World->GetGameViewport()) return;
    UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
    const double Now = World->GetTimeSeconds();
    auto Fail = [&](const FString& Why)
    {
        UE_LOG(LogTemp, Error, TEXT("ROOM101_ENTRY_FAILED: stage=%d %s pawn=%s velocity=%s mode=%d"),
            Stage, *Why, *Pawn->GetActorLocation().ToCompactString(), *Pawn->GetVelocity().ToCompactString(),
            int32(Movement->MovementMode));
        Movement->StopMovementImmediately();
        Stage = -1;
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };
    if (!WallStarted)
    {
        WallStarted = FPlatformTime::Seconds();
        WarmWorld = Now;
        WarmFrame = GFrameCounter;
        if (AGameModeBase* GameMode = World->GetAuthGameMode()) GameMode->SetActorTickInterval(0.f);
    }
    if (FPlatformTime::Seconds() - WallStarted > 180.) { Fail(TEXT("Bounded actual-map test timed out")); return; }
    auto Normal = [](AExistingHouseDoor* Door)
    {
        const FVector Along = (Door->DoorwayCenter - Door->GetActorLocation()).GetSafeNormal2D();
        return FVector(-Along.Y, Along.X, 0);
    };
    auto FloorAt = [&](const FVector& XY, float& FloorZ)
    {
        const float Expected = .8f * HouseWorldScale::Factor(World);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(Room101PhysicalFloor), false, Pawn);
        // A shut leaf is not a surface supporting the player's feet.
        for (TActorIterator<AExistingHouseDoor> It(World); It; ++It) Query.AddIgnoredActor(*It);
        FHitResult Hit;
        const bool Found = World->LineTraceSingleByChannel(Hit, FVector(XY.X, XY.Y, Expected + 40.f),
            FVector(XY.X, XY.Y, Expected - 30.f), ECC_Pawn, Query);
        const bool Good = Found && !Hit.bStartPenetrating && Movement->IsWalkable(Hit)
            && FMath::Abs(Hit.ImpactPoint.Z - Expected) < 12.f;
        FloorZ = Hit.ImpactPoint.Z;
        if (!Good) UE_LOG(LogTemp, Error, TEXT("ROOM101_FLOOR_FAILED: xy=%s actor=%s component=%s impact=%s normal=%s"),
            *XY.ToCompactString(), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()),
            *Hit.ImpactPoint.ToCompactString(), *Hit.ImpactNormal.ToCompactString());
        return Good;
    };
    auto PlaceApproach = [&](AExistingHouseDoor* Door)
    {
        FVector Position = Door->DoorwayCenter + Normal(Door) * 135.f;
        float FloorZ = 0;
        if (!FloorAt(Position, FloorZ)) return false;
        Position.Z = FloorZ + HalfHeight + 2.f;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(Room101ApproachClear), false, Pawn);
        if (World->OverlapBlockingTestByChannel(Position, FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(Radius, HalfHeight), Query)) return false;
        Movement->StopMovementImmediately();
        Pawn->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
        Movement->SetMovementMode(MOVE_Walking);
        Controller->SetControlRotation((Door->DoorwayCenter - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        UE_LOG(LogTemp, Display, TEXT("ROOM101_TEST_APPROACH: id=%s position=%s"),
            *Door->DoorId.ToString(), *Position.ToCompactString());
        return true;
    };
    auto Passage = [&](AExistingHouseDoor* Door, bool Closed)
    {
        const FVector N = Normal(Door);
        FVector Center = Door->DoorwayCenter;
        float FloorZ = 0;
        if (!FloorAt(Center, FloorZ)) return false;
        Center.Z = FloorZ + HalfHeight + 2.f;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(Room101DoorPhysicalPassage), false, Pawn);
        for (float Sign : {-1.f, 1.f})
        {
            FHitResult Hit;
            const bool Blocked = World->SweepSingleByChannel(Hit, Center + N * (Sign * 90.f),
                Center - N * (Sign * 90.f), FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Radius, HalfHeight), Query);
            const bool Passed = Closed ? Blocked && Hit.GetActor() == Door : !Blocked;
            UE_LOG(LogTemp, Display, TEXT("ROOM101_DOOR_PASSAGE: id=%s state=%s direction=%.0f passed=%d actor=%s component=%s impact=%s penetration=%d"),
                *Door->DoorId.ToString(), Closed ? TEXT("closed") : TEXT("open"), Sign, Passed,
                *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()),
                *Hit.ImpactPoint.ToCompactString(), Hit.bStartPenetrating);
            if (!Passed) return false;
        }
        return true;
    };
    auto PressDoor = [&](AExistingHouseDoor* Door)
    {
        Controller->SetControlRotation((Door->DoorwayCenter - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        // Input follows the same focus and occupied-doorway checks as human E.
        if (Pawn->FocusedDoorForInteraction() != Door) return false;
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
        ActionWorld = Now;
        return true;
    };
    if (Stage > 0 && (!Pawn->GetActorScale3D().Equals(CharacterScale, .001f)
        || !FMath::IsNearlyEqual(Radius, Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(), .01f)
        || !FMath::IsNearlyEqual(HalfHeight, Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), .01f)))
    { Fail(TEXT("Player dimensions changed during the room check")); return; }

    if (Stage == 0)
    {
        if (GFrameCounter - WarmFrame < 64 || Now - WarmWorld < 1.) return;
        FString UserDir;
        if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || UserDir.IsEmpty())
        { Fail(TEXT("This check requires its own -UserDir")); return; }
        Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
        HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        CharacterScale = Pawn->GetActorScale3D();
        if (UStoryMovementComponent* StoryMovement = Pawn->GetStoryMovement())
        { StoryMovement->SetSprintHeld(false); StoryMovement->SetSlideHeld(false); }
        for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
        {
            if (It->DoorId == TEXT("SM_OriginalLeaf_Entrance_m1_m2_Solid_01"))
            { if (Door101.IsValid()) { Fail(TEXT("Duplicate Room 101 door")); return; } Door101 = *It; }
            if (It->DoorId == TEXT("SM_OriginalLeaf_Entrance_m1_m2_Solid_03"))
            { if (Door102.IsValid()) { Fail(TEXT("Duplicate Room 102 door")); return; } Door102 = *It; }
        }
        if (!Door101.IsValid() || !Door102.IsValid() || Door101->bLocked || Door102->bLocked)
        { Fail(TEXT("Room 101 or 102 is missing or locked")); return; }
        if (Door101->IsOpen() || Door102->IsOpen()) { Fail(TEXT("Doors must start closed")); return; }
        const FVector AuthoredRoute[] = {
            FVector(-2465,-1212.7,.8), FVector(-2670,-1212.7,.8), FVector(-2870,-1212.7,.8),
            FVector(-2870,-1130,.8), FVector(-2670,-1130,.8), FVector(-2670,-850,.8),
            FVector(-2670,-620,.8), FVector(-2580,-850,.8), FVector(-2510,-1160,.8),
            FVector(-2465,-1212.7,.8), FVector(-2215,-1212.7,.8)
        };
        for (const FVector& Point : AuthoredRoute)
        {
            const FVector Live = HouseWorldScale::Point(World, Point);
            float FloorZ = 0;
            if (!FloorAt(Live, FloorZ)) { Fail(TEXT("The normal room route lacks a supporting floor")); return; }
            Route.Add(Live);
        }
        // Include the floor, ceiling and all four boundaries of the room. A
        // capsule sweep uses the current player size and actual map collision.
        FVector RoomCenter = HouseWorldScale::Point(World, FVector(-2670,-890,.8));
        RoomCenter.Z += HalfHeight + 2.f;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(Room101Perimeter), false, Pawn);
        // The room already contains beds, a couch and small movable objects.
        // Exclude those only while identifying its structural boundaries;
        // every actual walking input below retains all normal collisions.
        for (TActorIterator<AHouseInteractiveProp> It(World); It; ++It) Query.AddIgnoredActor(*It);
        for (TActorIterator<AHouseReactiveFixture> It(World); It; ++It) Query.AddIgnoredActor(*It);
        FHitResult Ceiling;
        if (!World->LineTraceSingleByChannel(Ceiling, RoomCenter,
            RoomCenter + FVector(0,0,400.f), ECC_Pawn, Query)
            || Ceiling.ImpactNormal.Z > -.5f
            || FMath::Abs(Ceiling.ImpactPoint.Z - 262.24f * HouseWorldScale::Factor(World)) > 16.f)
        { Fail(TEXT("Room 101 needs a normal-height solid ceiling")); return; }
        const FVector Outside[] = {
            FVector(-3070,-890,.8), FVector(-2310,-890,.8),
            FVector(-2670,-1380,.8), FVector(-2670,-420,.8)
        };
        const float ExpectedWall[] = {-3015.f, -2368.8f, -1313.1f, -473.4f};
        for (int32 Side = 0; Side < UE_ARRAY_COUNT(Outside); ++Side)
        {
            FVector End = HouseWorldScale::Point(World, Outside[Side]);
            End.Z = RoomCenter.Z;
            FHitResult Wall;
            if (!World->SweepSingleByChannel(Wall, RoomCenter, End, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Radius, HalfHeight), Query) || Wall.bStartPenetrating
                || FMath::Abs((Side < 2 ? Wall.ImpactPoint.X : Wall.ImpactPoint.Y)
                    - ExpectedWall[Side] * HouseWorldScale::Factor(World)) > 30.f)
            { Fail(FString::Printf(TEXT("Room perimeter side %d does not block the player"), Side)); return; }
            UE_LOG(LogTemp, Display, TEXT("ROOM101_WALL_PASSED: side=%d actor=%s component=%s impact=%s"),
                Side, *GetNameSafe(Wall.GetActor()), *GetNameSafe(Wall.GetComponent()), *Wall.ImpactPoint.ToCompactString());
        }
        UE_LOG(LogTemp, Display, TEXT("ROOM101_ENTRY_START: houseScale=%.3f playerScale=%s capsuleRadius=%.2f capsuleHalfHeight=%.2f roomWidth=%.2f roomLength=%.2f ceiling=%.2f"),
            HouseWorldScale::Factor(World), *CharacterScale.ToCompactString(), Radius, HalfHeight,
            (3015.f-2368.8f)*HouseWorldScale::Factor(World), (1313.1f-473.4f)*HouseWorldScale::Factor(World), Ceiling.ImpactPoint.Z);
        if (!PlaceApproach(Door102.Get())) { Fail(TEXT("Room 102 lacks a clear hallway approach")); return; }
        ActionWorld = Now; Stage = 1; return;
    }
    if (Stage == 1)
    {
        if (Now - ActionWorld < .3) return;
        if (!Passage(Door102.Get(), true) || !PressDoor(Door102.Get()))
        { Fail(TEXT("Room 102 closed passage or E input failed")); return; }
        Stage = 2; return;
    }
    if (Stage == 2)
    {
        if (Now - ActionWorld < 1.) return;
        if (!Door102->IsOpen() || !Passage(Door102.Get(), false) || !PressDoor(Door102.Get()))
        { Fail(TEXT("Room 102 did not open a clear passage or close with E")); return; }
        Stage = 3; return;
    }
    if (Stage == 3)
    {
        if (Now - ActionWorld < 1.) return;
        if (Door102->IsOpen() || !Passage(Door102.Get(), true))
        { Fail(TEXT("Room 102 does not remain physically closed")); return; }
        if (!PlaceApproach(Door101.Get()) || !Passage(Door101.Get(), true))
        { Fail(TEXT("Room 101 lacks a clear hallway approach or closed blocker")); return; }
        ClosedWalkStart = Pawn->GetActorLocation();
        ActionWorld = Now; Stage = 4; return;
    }
    if (Stage == 4)
    {
        if (Now - ActionWorld < .8)
        { Pawn->AddMovementInput(-Normal(Door101.Get()), 1.f, true); return; }
        const float Remaining = FVector::DotProduct(Pawn->GetActorLocation() - Door101->DoorwayCenter,
            Normal(Door101.Get()));
        if (Door101->IsOpen() || Remaining < Radius || FVector::Dist2D(ClosedWalkStart, Pawn->GetActorLocation()) < 15.f)
        { Fail(TEXT("Walking must approach and stop at the closed Room 101 leaf")); return; }
        Movement->StopMovementImmediately();
        UE_LOG(LogTemp, Display, TEXT("ROOM101_CLOSED_WALK_PASSED: distanceFromLeaf=%.2f travel=%.2f"),
            Remaining, FVector::Dist2D(ClosedWalkStart, Pawn->GetActorLocation()));
        if (!PressDoor(Door101.Get())) { Fail(TEXT("Native E focus did not select Room 101")); return; }
        Stage = 5; return;
    }
    if (Stage == 5)
    {
        if (Now - ActionWorld < 1.) return;
        if (!Door101->IsOpen() || !Passage(Door101.Get(), false))
        { Fail(TEXT("Room 101 still blocks its doorway after opening")); return; }
        Waypoint = 0; BestDistance = BIG_NUMBER; LastProgressWorld = Now; Stage = 6;
    }
    if (Stage == 6)
    {
        if (Waypoint == Route.Num())
        {
            Movement->StopMovementImmediately();
            Controller->SetControlRotation((Door101->DoorwayCenter - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
            ActionWorld = Now; Stage = 61; return;
        }
        const FVector Position = Pawn->GetActorLocation();
        const FVector To = Route[Waypoint] - Position;
        const float Distance = To.Size2D();
        const float FloorZ = Route[Waypoint].Z;
        if (FMath::Abs(Position.Z - HalfHeight - FloorZ) > 12.f || !Movement->IsMovingOnGround())
        { Fail(TEXT("Player left the normal supporting room floor")); return; }
        if (Distance < 18.f)
        {
            UE_LOG(LogTemp, Display, TEXT("ROOM101_WALK_WAYPOINT: index=%d position=%s doorOpen=%d"),
                Waypoint, *Position.ToCompactString(), Door101->IsOpen());
            ++Waypoint; BestDistance = BIG_NUMBER; LastProgressWorld = Now; return;
        }
        if (Distance + 2.f < BestDistance) { BestDistance = Distance; LastProgressWorld = Now; }
        if (Now - LastProgressWorld > 5.)
        { Fail(FString::Printf(TEXT("Real walking stalled at room waypoint %d target=%s"), Waypoint, *Route[Waypoint].ToCompactString())); return; }
        Controller->SetControlRotation(To.GetSafeNormal2D().Rotation());
        Pawn->AddMovementInput(To.GetSafeNormal2D(), Distance < 80.f ? .35f : 1.f, true);
        return;
    }
    if (Stage == 61)
    {
        // Give normal pawn/camera ticking a frame to turn back toward the door.
        if (Now - ActionWorld < .2) return;
        if (!PressDoor(Door101.Get()))
        { Fail(TEXT("Native E could not close Room 101 after walking back outside")); return; }
        Stage = 7; return;
    }
    if (Stage == 7)
    {
        if (Now - ActionWorld < 1.) return;
        if (Door101->IsOpen() || !Passage(Door101.Get(), true))
        { Fail(TEXT("Room 101 did not close and block after the physical return route")); return; }
        UE_LOG(LogTemp, Display, TEXT("ROOM101_ENTRY_PASSED: doors=2 closed_block_both_directions=1 native_E_open_close=1 real_room_walk_waypoints=%d solid_walls=4 normal_ceiling=1 player_dimensions_unchanged=1"), Route.Num());
        Stage = -1;
        FPlatformMisc::RequestExitWithStatus(false, 0);
    }
}
#endif
