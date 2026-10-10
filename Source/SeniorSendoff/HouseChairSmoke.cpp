// Opt-in rendered game check for Room 102's real physics chairs.
// Start Chapter01_House with -HouseChairSmoke and an isolated -UserDir.
#include "HouseChairSmoke.h"
#include "HouseFurnitureInteractionComponent.h"
#include "StoryCampaign.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformMisc.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/Parse.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodyInstance.h"

namespace
{
const FName RoomChairTag(TEXT("SSORoom102Chair"));
const FName TestChairTag(TEXT("SSORoom102TestChair"));
const FName ReservationTag(TEXT("SSOFurnitureHeld"));

enum class EPhase : uint8
{
    Warmup, AlignFirst, WaitFirstGrab, Carry, WaitDrop,
    AlignSecond, WaitSecondGrab, WaitThrow, WatchThrow, Done
};

struct FState
{
    EPhase Phase = EPhase::Warmup;
    double Began = 0.0;
    double PhaseBegan = 0.0;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<UPrimitiveComponent> Chair;
    TWeakObjectPtr<UHouseFurnitureInteractionComponent> Challenger;
    TWeakObjectPtr<AHouseChairSmokeHitObserver> HitObserver;
    FVector CarryAway = FVector::ZeroVector;
    FVector CarryStart = FVector::ZeroVector;
    FVector ThrowStart = FVector::ZeroVector;
    FVector ThrowForward = FVector::ZeroVector;
    float MaxThrowSpeed = 0.f;
    float MaxForwardSpeed = 0.f;
    float MaxThrowTravel = 0.f;
    bool bReleasePending = false;
    bool bOldNotifyHit = false;
    bool bHitTracking = false;
    bool bThrowReleasedSeen = false;
    int32 ValidChairCount = 0;
};

bool SendButton(APlayerController* Controller, const FKey& Key, EInputEvent Event)
{
    if (!Controller || !Controller->PlayerInput || !FSlateApplication::IsInitialized()) return false;
    // The normal Controller -> PlayerInput -> pawn input-component key binding.
    // This is deliberately not a direct component call for E or LMB.
    const FInputKeyEventArgs Args(nullptr,
        FSlateApplication::Get().GetInputDeviceIdForMouse(),
        Key, Event, 1.f, false, 0);
    Controller->InputKey(Args);
    return true;
}

bool HasClearView(UWorld* World, AStoryFirstPersonCharacter* Pawn,
    UPrimitiveComponent* Chair, const FVector& View, const FVector& Target, float Range,
    FHitResult& OutHit, float& OutDistance)
{
    FCollisionQueryParams Query;
    Query.AddIgnoredActor(Pawn);
    OutDistance = FVector::Dist(View, Target);
    return OutDistance < Range &&
        World->LineTraceSingleByChannel(OutHit, View, Target, ECC_Visibility, Query) &&
        OutHit.GetComponent() == Chair;
}

bool GetLiveSeatTarget(UPrimitiveComponent* Chair, FVector& OutTarget)
{
    const UBodySetup* Setup = Chair ? Chair->GetBodySetup() : nullptr;
    if (!Setup || Setup->AggGeom.ConvexElems.IsEmpty()) return false;
    // UCX_..._00 is the seat. Bounds.Origin can be in the open space below it,
    // especially after the released chair rotates on the floor.
    const FKConvexElem& Seat = Setup->AggGeom.ConvexElems[0];
    if (Seat.VertexData.IsEmpty()) return false;
    FVector SeatCenter = FVector::ZeroVector;
    for (const FVector& Vertex : Seat.VertexData) SeatCenter += Vertex;
    SeatCenter /= Seat.VertexData.Num();
    OutTarget = Chair->GetComponentTransform().TransformPosition(
        Seat.GetTransform().TransformPosition(SeatCenter));
    return !OutTarget.ContainsNaN();
}

bool AimAtLiveSeat(UWorld* World, AStoryFirstPersonCharacter* Pawn,
    APlayerController* Controller, UPrimitiveComponent* Chair, float Range,
    bool bProbe, const TCHAR* Phase)
{
    FVector Target;
    if (!World || !Pawn || !Controller || !GetLiveSeatTarget(Chair, Target)) return false;
    FVector View;
    FRotator ViewRotation;
    Controller->GetPlayerViewPoint(View, ViewRotation);
    const FRotator Aim = (Target - View).Rotation();
    Controller->SetControlRotation(Aim);
    Pawn->SetActorRotation(FRotator(0.f, Aim.Yaw, 0.f));
    if (!bProbe) return true;

    // Use the same live controller view and visibility channel as ToggleGrab.
    Controller->GetPlayerViewPoint(View, ViewRotation);
    FCollisionQueryParams Query;
    Query.AddIgnoredActor(Pawn);
    Query.bTraceComplex = false;
    FHitResult Hit;
    const bool bHit = World->LineTraceSingleByChannel(Hit, View,
        View + ViewRotation.Vector() * Range, ECC_Visibility, Query);
    const bool bChairHit = bHit && Hit.GetComponent() == Chair;
    UE_LOG(LogTemp, Display, TEXT("HOUSE_CHAIR_PRE_E phase=%s chair=%s visible=%d seatTarget=%s view=%s viewRotation=%s hitActor=%s hitComponent=%s impact=%s distance=%.1f"),
        Phase, *GetNameSafe(Chair ? Chair->GetOwner() : nullptr), bChairHit ? 1 : 0,
        *Target.ToString(), *View.ToString(), *ViewRotation.ToString(),
        *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()),
        *Hit.ImpactPoint.ToString(), bHit ? FVector::Dist(View, Hit.ImpactPoint) : -1.f);
    return bChairHit;
}

bool PlacePlayerForChair(UWorld* World, AStoryFirstPersonCharacter* Pawn,
    APlayerController* Controller, UPrimitiveComponent* Chair, FVector& OutAway)
{
    if (!World || !Pawn || !Controller || !Chair || !Pawn->GetCapsuleComponent()) return false;
    const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FVector Target;
    if (!GetLiveSeatTarget(Chair, Target)) return false;
    // ApplyCharacterDimensions replaces the constructor's 64 cm camera offset.
    // Predict the same eye the player's GetPlayerViewPoint/Grab trace will use.
    const FVector CameraOffset = Pawn->FirstPersonCamera
        ? Pawn->FirstPersonCamera->GetRelativeLocation() : FVector(0, 0, Pawn->BaseEyeHeight);
    const FVector ScaledCameraOffset = Pawn->GetActorQuat().RotateVector(
        CameraOffset * Pawn->GetActorScale3D());
    TArray<FVector> CandidateCenters;

    FString Requested;
    if (FParse::Value(FCommandLine::Get(), TEXT("HouseChairTestLocation="), Requested, false))
    {
        TArray<FString> Coordinates;
        Requested.ParseIntoArray(Coordinates, TEXT(","), true);
        if (Coordinates.Num() != 3) return false;
        // Optional Unreal-world *feet* coordinates, comma separated.
        FVector Feet;
        if (!FDefaultValueHelper::ParseDouble(Coordinates[0], Feet.X)
            || !FDefaultValueHelper::ParseDouble(Coordinates[1], Feet.Y)
            || !FDefaultValueHelper::ParseDouble(Coordinates[2], Feet.Z)) return false;
        if (Feet.ContainsNaN()) return false;
        CandidateCenters.Add(Feet + FVector(0, 0, HalfHeight + 5.f));
    }
    // A dropped or carried chair can occupy the original requested spot.
    // Keep the requested point first, then find another valid place around it.
    {
        for (int32 Index = 0; Index < 16; ++Index)
        {
            const float Angle = 2.f * PI * float(Index) / 16.f;
            const FVector Away(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
            const FVector GroundProbe = Target + Away * 190.f;
            FCollisionQueryParams FloorQuery;
            FloorQuery.AddIgnoredActor(Pawn);
            FloorQuery.AddIgnoredActor(Chair->GetOwner());
            FHitResult Floor;
            if (!World->LineTraceSingleByChannel(Floor, GroundProbe + FVector(0, 0, 200),
                GroundProbe - FVector(0, 0, 240), ECC_Visibility, FloorQuery)
                || Floor.ImpactNormal.Z < .65f) continue;
            CandidateCenters.Add(Floor.ImpactPoint + FVector(0, 0, HalfHeight + 5.f));
        }
    }

    const bool bDiagnose = !Requested.IsEmpty() || Chair->GetOwner()->ActorHasTag(TestChairTag);
    int32 CandidateIndex = 0;
    for (const FVector& Center : CandidateCenters)
    {
        ++CandidateIndex;
        const FVector PredictedView = Center + ScaledCameraOffset;
        if (bDiagnose)
            UE_LOG(LogTemp, Display, TEXT("HOUSE_CHAIR_PLACE_TRY chair=%s candidate=%d center=%s view=%s target=%s boundsExtent=%s capsuleRadius=%.1f capsuleHalfHeight=%.1f cameraOffset=%s range=%.1f"),
                *Chair->GetOwner()->GetName(), CandidateIndex, *Center.ToString(),
                *PredictedView.ToString(), *Target.ToString(), *Chair->Bounds.BoxExtent.ToString(),
                Radius, HalfHeight, *ScaledCameraOffset.ToString(), Pawn->FurnitureInteraction->GrabRange);
        FCollisionQueryParams SpaceQuery;
        SpaceQuery.AddIgnoredActor(Pawn);
        const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
        if (World->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn,
            Shape, SpaceQuery))
        {
            if (bDiagnose)
            {
                TArray<FOverlapResult> Overlaps;
                World->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_Pawn,
                    Shape, SpaceQuery);
                int32 BlockingCount = 0;
                for (const FOverlapResult& Overlap : Overlaps)
                    if (Overlap.bBlockingHit)
                    {
                        ++BlockingCount;
                        UE_LOG(LogTemp, Warning, TEXT("HOUSE_CHAIR_PLACE_BLOCKED chair=%s candidate=%d actor=%s component=%s"),
                            *Chair->GetOwner()->GetName(), CandidateIndex,
                            *GetNameSafe(Overlap.GetActor()), *GetNameSafe(Overlap.GetComponent()));
                    }
                if (!BlockingCount)
                    UE_LOG(LogTemp, Warning, TEXT("HOUSE_CHAIR_PLACE_BLOCKED chair=%s candidate=%d blockingTest=1 overlapDetails=0"),
                        *Chair->GetOwner()->GetName(), CandidateIndex);
            }
            continue;
        }
        FHitResult ViewHit;
        float ViewDistance = 0.f;
        if (!HasClearView(World, Pawn, Chair, PredictedView,
            Target, Pawn->FurnitureInteraction->GrabRange, ViewHit, ViewDistance))
        {
            if (bDiagnose)
                UE_LOG(LogTemp, Warning, TEXT("HOUSE_CHAIR_PLACE_NO_VIEW chair=%s candidate=%d distance=%.1f range=%.1f hitActor=%s hitComponent=%s hitPoint=%s"),
                    *Chair->GetOwner()->GetName(), CandidateIndex, ViewDistance,
                    Pawn->FurnitureInteraction->GrabRange, *GetNameSafe(ViewHit.GetActor()),
                    *GetNameSafe(ViewHit.GetComponent()), *ViewHit.ImpactPoint.ToString());
            continue;
        }
        if (!Pawn->SetActorLocation(Center, false, nullptr, ETeleportType::TeleportPhysics))
        {
            if (bDiagnose) UE_LOG(LogTemp, Warning, TEXT("HOUSE_CHAIR_PLACE_TELEPORT_FAILED chair=%s candidate=%d"),
                *Chair->GetOwner()->GetName(), CandidateIndex);
            continue;
        }
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        const FRotator Aim = (Target - PredictedView).Rotation();
        Controller->SetControlRotation(Aim);
        Pawn->SetActorRotation(FRotator(0.f, Aim.Yaw, 0.f));
        OutAway = (Center - Target).GetSafeNormal2D();
        UE_LOG(LogTemp, Display, TEXT("HOUSE_CHAIR_PLACE_OK chair=%s candidate=%d view=%s aim=%s"),
            *Chair->GetOwner()->GetName(), CandidateIndex, *PredictedView.ToString(), *Aim.ToString());
        return !OutAway.IsNearlyZero();
    }
    return false;
}

void Cleanup(FState& State)
{
    if (AStoryFirstPersonCharacter* Pawn = State.Pawn.Get())
        if (Pawn->FurnitureInteraction && Pawn->FurnitureInteraction->bHoldingFurniture)
            Pawn->FurnitureInteraction->ReleaseGrab();
    if (UHouseFurnitureInteractionComponent* Challenger = State.Challenger.Get())
        Challenger->DestroyComponent();
    if (UPrimitiveComponent* Chair = State.Chair.Get(); State.bHitTracking && Chair)
    {
        if (AHouseChairSmokeHitObserver* Observer = State.HitObserver.Get())
            Chair->OnComponentHit.RemoveDynamic(Observer, &AHouseChairSmokeHitObserver::OnChairHit);
        Chair->SetNotifyRigidBodyCollision(State.bOldNotifyHit);
    }
    if (AHouseChairSmokeHitObserver* Observer = State.HitObserver.Get()) Observer->Destroy();
    State.Phase = EPhase::Done;
}

void Finish(FState& State, bool bPassed, const FString& Detail)
{
    UE_LOG(LogTemp, Display, TEXT("HOUSE_CHAIR_SMOKE_RESULT passed=%d chairs=%d maxSpeed=%.1f maxForwardSpeed=%.1f throwTravel=%.1f hits=%d detail=%s"),
        bPassed ? 1 : 0, State.ValidChairCount, State.MaxThrowSpeed,
        State.MaxForwardSpeed, State.MaxThrowTravel,
        State.HitObserver.IsValid() ? State.HitObserver->HitCount : 0, *Detail);
    Cleanup(State);
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}
}
#endif

AHouseChairSmokeHitObserver::AHouseChairSmokeHitObserver()
{
    PrimaryActorTick.bCanEverTick = false;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}

void AHouseChairSmokeHitObserver::OnChairHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (NormalImpulse.SizeSquared() > 1.f) ++HitCount;
}

void TickHouseChairSmoke(UWorld* World)
{
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(), TEXT("HouseChairSmoke"))) return;
    static FState State;
    if (!World || State.Phase == EPhase::Done) return;
    const double Now = FPlatformTime::Seconds();
    if (State.Began == 0.0)
    {
        State.Began = Now;
        State.PhaseBegan = Now;
        FString UserDir;
        if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || UserDir.IsEmpty())
        { Finish(State, false, TEXT("An isolated -UserDir is required")); return; }
        if (UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House"))
        { Finish(State, false, TEXT("Start the native Chapter01_House game map")); return; }
        if (AGameModeBase* Mode = World->GetAuthGameMode()) Mode->SetActorTickInterval(0.f);
    }
    if (Now - State.Began > 26.0)
    { Finish(State, false, TEXT("Timed out")); return; }
    APlayerController* Controller = World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = Controller ? Cast<AStoryFirstPersonCharacter>(Controller->GetPawn()) : nullptr;
    if (!Controller || !Pawn || !Pawn->FurnitureInteraction)
    {
        if (Now - State.Began > 10.0) Finish(State, false, TEXT("Player or furniture input did not spawn"));
        return;
    }
    State.Controller = Controller;
    State.Pawn = Pawn;
    UHouseFurnitureInteractionComponent* Interaction = Pawn->FurnitureInteraction;
    UPrimitiveComponent* Chair = State.Chair.Get();

    switch (State.Phase)
    {
    case EPhase::Warmup:
    {
        if (Now - State.Began < 8.0) return; // Let the room's physical chairs settle.
        TArray<UPrimitiveComponent*> Candidates;
        TArray<UPrimitiveComponent*> Preferred;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor || !Actor->ActorHasTag(RoomChairTag)) continue;
            UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
            UBodySetup* Setup = Body ? Body->GetBodySetup() : nullptr;
            const int32 ConvexCount = Setup ? Setup->AggGeom.ConvexElems.Num() : 0;
            if (!Body || !Body->IsSimulatingPhysics() || ConvexCount < 1 ||
                !Actor->GetIsReplicated() || !Actor->IsReplicatingMovement())
            {
                Finish(State, false, FString::Printf(TEXT("Room 102 chair %s lacks simulated replicated root/convex collision (%d convexes)"),
                    *Actor->GetName(), ConvexCount));
                return;
            }
            ++State.ValidChairCount;
            UE_LOG(LogTemp, Display, TEXT("HOUSE_CHAIR_SMOKE_CHAIR name=%s massKg=%.2f convexes=%d awake=%d"),
                *Actor->GetName(), Body->GetMass(), ConvexCount, Body->IsAnyRigidBodyAwake() ? 1 : 0);
            if (Actor->ActorHasTag(TestChairTag)) Preferred.Add(Body);
            else Candidates.Add(Body);
        }
        if (State.ValidChairCount < 1)
        { Finish(State, false, TEXT("No SSORoom102Chair actors found")); return; }
        Preferred.Append(Candidates);
        for (UPrimitiveComponent* Body : Preferred)
        {
            if (PlacePlayerForChair(World, Pawn, Controller, Body, State.CarryAway))
            { State.Chair = Body; break; }
        }
        if (!State.Chair.IsValid())
        { Finish(State, false, TEXT("No Room 102 chair has a clear player-sized grab position")); return; }
        State.Phase = EPhase::AlignFirst;
        State.PhaseBegan = Now;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_CHAIR_SMOKE_READY chair=%s"), *State.Chair->GetOwner()->GetName());
        return;
    }
    case EPhase::AlignFirst:
        if (Now - State.PhaseBegan < .2)
        { AimAtLiveSeat(World, Pawn, Controller, Chair, Interaction->GrabRange, false, TEXT("first")); return; }
        if (!AimAtLiveSeat(World, Pawn, Controller, Chair, Interaction->GrabRange,
            true, TEXT("first")))
        {
            if (Now - State.PhaseBegan > .8)
                Finish(State, false, TEXT("Live seat is not in the first grab's visibility trace"));
            return;
        }
        if (!SendButton(Controller, EKeys::E, IE_Pressed))
        { Finish(State, false, TEXT("Cannot dispatch E through controller input")); return; }
        State.bReleasePending = true;
        State.Phase = EPhase::WaitFirstGrab;
        State.PhaseBegan = Now;
        return;
    case EPhase::WaitFirstGrab:
        if (State.bReleasePending)
        { SendButton(Controller, EKeys::E, IE_Released); State.bReleasePending = false; return; }
        if (Now - State.PhaseBegan < .12) return;
        if (!Interaction->bHoldingFurniture || !Chair || !Chair->ComponentHasTag(ReservationTag))
        { Finish(State, false, TEXT("E did not grab and reserve the chosen chair")); return; }
        // A second grip component with the same valid view must not steal the held body.
        {
            FVector ViewLocation;
            FRotator ViewRotation;
            Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
            FCollisionQueryParams Query;
            Query.AddIgnoredActor(Pawn);
            FHitResult Hit;
            if (!World->LineTraceSingleByChannel(Hit, ViewLocation,
                ViewLocation + ViewRotation.Vector() * Interaction->GrabRange,
                ECC_Visibility, Query) || Hit.GetComponent() != Chair)
            { Finish(State, false, TEXT("Reserved chair was not in the competing grip's view")); return; }
            UHouseFurnitureInteractionComponent* Challenger = NewObject<UHouseFurnitureInteractionComponent>(Pawn);
            Pawn->AddInstanceComponent(Challenger);
            Challenger->RegisterComponent();
            State.Challenger = Challenger;
            Challenger->ToggleGrab();
            if (Challenger->bHoldingFurniture)
            { Finish(State, false, TEXT("A competing grip stole the reserved chair")); return; }
            Challenger->DestroyComponent();
            State.Challenger.Reset();
        }
        State.CarryStart = Chair->GetComponentLocation();
        State.Phase = EPhase::Carry;
        State.PhaseBegan = Now;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_CHAIR_SMOKE_GRABBED reserved=1"));
        return;
    case EPhase::Carry:
        if (!Chair || !Interaction->bHoldingFurniture || !Chair->ComponentHasTag(ReservationTag))
        { Finish(State, false, TEXT("Lost held chair during carry")); return; }
        if (Now - State.PhaseBegan < .60)
        { Pawn->AddMovementInput(State.CarryAway, .6f, true); return; }
        if (FVector::Dist(Chair->GetComponentLocation(), State.CarryStart) < 12.f)
        { Finish(State, false, TEXT("Held chair did not follow player movement")); return; }
        UE_LOG(LogTemp, Display, TEXT("HOUSE_CHAIR_SMOKE_CARRIED displacement=%.1f"),
            FVector::Dist(Chair->GetComponentLocation(), State.CarryStart));
        SendButton(Controller, EKeys::E, IE_Pressed);
        State.bReleasePending = true;
        State.Phase = EPhase::WaitDrop;
        State.PhaseBegan = Now;
        return;
    case EPhase::WaitDrop:
        if (State.bReleasePending)
        { SendButton(Controller, EKeys::E, IE_Released); State.bReleasePending = false; return; }
        if (Now - State.PhaseBegan < .15) return;
        if (!Chair || Interaction->bHoldingFurniture || Chair->ComponentHasTag(ReservationTag))
        { Finish(State, false, TEXT("E did not release the chair and clear its reservation")); return; }
        if (!PlacePlayerForChair(World, Pawn, Controller, Chair, State.CarryAway))
        { Finish(State, false, TEXT("Cannot reach dropped chair for repeated grab")); return; }
        State.Phase = EPhase::AlignSecond;
        State.PhaseBegan = Now;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_CHAIR_SMOKE_DROPPED reservationCleared=1"));
        return;
    case EPhase::AlignSecond:
        if (Now - State.PhaseBegan < .2)
        { AimAtLiveSeat(World, Pawn, Controller, Chair, Interaction->GrabRange, false, TEXT("second")); return; }
        if (!AimAtLiveSeat(World, Pawn, Controller, Chair, Interaction->GrabRange,
            true, TEXT("second")))
        {
            if (Now - State.PhaseBegan > .8)
                Finish(State, false, TEXT("Live seat is not in the repeated grab's visibility trace"));
            return;
        }
        SendButton(Controller, EKeys::E, IE_Pressed);
        State.bReleasePending = true;
        State.Phase = EPhase::WaitSecondGrab;
        State.PhaseBegan = Now;
        return;
    case EPhase::WaitSecondGrab:
        if (State.bReleasePending)
        { SendButton(Controller, EKeys::E, IE_Released); State.bReleasePending = false; return; }
        if (Now - State.PhaseBegan < .12) return;
        if (!Chair || !Interaction->bHoldingFurniture || !Chair->ComponentHasTag(ReservationTag))
        { Finish(State, false, TEXT("Repeated E grab failed after drop")); return; }
        State.HitObserver = World->SpawnActor<AHouseChairSmokeHitObserver>();
        if (!State.HitObserver.IsValid())
        { Finish(State, false, TEXT("Could not monitor physical throw impact")); return; }
        if (FBodyInstance* Instance = Chair->GetBodyInstance())
            State.bOldNotifyHit = Instance->bNotifyRigidBodyCollision;
        Chair->SetNotifyRigidBodyCollision(true);
        Chair->OnComponentHit.AddDynamic(State.HitObserver.Get(), &AHouseChairSmokeHitObserver::OnChairHit);
        State.bHitTracking = true;
        State.ThrowStart = Chair->GetComponentLocation();
        State.ThrowForward = Controller->GetControlRotation().Vector().GetSafeNormal2D();
        if (!SendButton(Controller, EKeys::LeftMouseButton, IE_Pressed))
        { Finish(State, false, TEXT("Cannot dispatch LMB through controller input")); return; }
        State.bReleasePending = true;
        State.Phase = EPhase::WaitThrow;
        State.PhaseBegan = Now;
        return;
    case EPhase::WaitThrow:
        if (State.bReleasePending)
        { SendButton(Controller, EKeys::LeftMouseButton, IE_Released); State.bReleasePending = false; return; }
        if (!Chair) { Finish(State, false, TEXT("Chair disappeared on throw")); return; }
        if (!Interaction->bHoldingFurniture && !Chair->ComponentHasTag(ReservationTag))
        {
            if (!State.bThrowReleasedSeen)
            {
                State.bThrowReleasedSeen = true;
                // Ignore contact while the physics handle was carrying the chair.
                if (AHouseChairSmokeHitObserver* Observer = State.HitObserver.Get()) Observer->HitCount = 0;
            }
            const FVector Velocity = Chair->GetPhysicsLinearVelocity();
            State.MaxThrowSpeed = FMath::Max(State.MaxThrowSpeed, Velocity.Size());
            State.MaxForwardSpeed = FMath::Max(State.MaxForwardSpeed,
                FVector::DotProduct(Velocity, State.ThrowForward));
            State.MaxThrowTravel = FMath::Max(State.MaxThrowTravel,
                FVector::DotProduct(Chair->GetComponentLocation() - State.ThrowStart, State.ThrowForward));
        }
        if (Now - State.PhaseBegan < .15) return;
        if (Interaction->bHoldingFurniture || Chair->ComponentHasTag(ReservationTag))
        { Finish(State, false, TEXT("LMB did not release held chair before equipment/door action")); return; }
        State.Phase = EPhase::WatchThrow;
        State.PhaseBegan = Now;
        return;
    case EPhase::WatchThrow:
    {
        if (!Chair) { Finish(State, false, TEXT("Chair disappeared in flight")); return; }
        const FVector Velocity = Chair->GetPhysicsLinearVelocity();
        State.MaxThrowSpeed = FMath::Max(State.MaxThrowSpeed, Velocity.Size());
        State.MaxForwardSpeed = FMath::Max(State.MaxForwardSpeed,
            FVector::DotProduct(Velocity, State.ThrowForward));
        State.MaxThrowTravel = FMath::Max(State.MaxThrowTravel,
            FVector::DotProduct(Chair->GetComponentLocation() - State.ThrowStart, State.ThrowForward));
        if (Now - State.PhaseBegan < 1.6) return;
        if (State.MaxThrowSpeed < 350.f || State.MaxForwardSpeed < 250.f || State.MaxThrowTravel < 30.f)
        { Finish(State, false, TEXT("Throw lacked forward awake velocity or travel")); return; }
        if (!State.HitObserver.IsValid() || State.HitObserver->HitCount < 1)
        { Finish(State, false, TEXT("Thrown chair did not generate a physical impact")); return; }
        Finish(State, true, TEXT("E grab, reserved carry, E drop, repeated grab, LMB throw and physical impact passed"));
        return;
    }
    case EPhase::Done: return;
    }
#endif
}
