#include "HouseImpossibleHallway.h"
#include "HouseWorldScale.h"
#include "ExistingHouseDoor.h"
#include "HouseFurnitureInteractionComponent.h"
#include "HouseRoamingPig.h"
#include "StoryCampaign.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

AHouseImpossibleHallway::AHouseImpossibleHallway()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("HallwayOrigin"));
    PipeDrone = CreateDefaultSubobject<UAudioComponent>(TEXT("DistantPipeDrone"));
    PipeDrone->SetupAttachment(RootComponent);
    PipeDrone->SetRelativeLocation(FVector(240, 0, 124));
    PipeDrone->bAutoActivate = false;
    PipeDrone->bOverrideAttenuation = true;
    PipeDrone->AttenuationOverrides.bAttenuate = true;
    PipeDrone->AttenuationOverrides.bSpatialize = true;
    PipeDrone->AttenuationOverrides.AttenuationShapeExtents = FVector(320, 0, 0);
    PipeDrone->AttenuationOverrides.FalloffDistance = 260.f;
    PipeDrone->SetVolumeMultiplier(.45f);
    bReplicates = true;
    SetReplicateMovement(false);
}

void AHouseImpossibleHallway::BeginPlay()
{
    Super::BeginPlay();
    const float Scale = HouseWorldScale::Factor(GetWorld());
    PipeDrone->AttenuationOverrides.AttenuationShapeExtents *= Scale;
    PipeDrone->AttenuationOverrides.FalloffDistance *= Scale;
    for (TActorIterator<AExistingHouseDoor> It(GetWorld()); It; ++It)
        if (It->DoorId == EntranceDoorId) { EntranceDoor = *It; break; }
    if (GetNetMode() != NM_DedicatedServer)
        if (USoundBase* Sound = LoadObject<USoundBase>(nullptr,
            TEXT("/Game/SeniorSendOff/House/ImpossibleHallway/IH_PipeDrone.IH_PipeDrone")))
        { PipeDrone->SetSound(Sound); PipeDrone->Play(); }
}

bool AHouseImpossibleHallway::IsTravelerActive(const AStoryFirstPersonCharacter* Pawn) const
{
    const FTraveler* State = Travelers.Find(const_cast<AStoryFirstPersonCharacter*>(Pawn));
    return State && State->bActive;
}

bool AHouseImpossibleHallway::TranslateTraveler(AStoryFirstPersonCharacter* Pawn, const FVector& Delta)
{
    UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
    const FVector Velocity = Movement->Velocity;
    const FVector Before = Pawn->GetActorLocation();
    const FRotator Rotation = Pawn->GetActorRotation();
    AHouseRoamingPig* Pig = Pawn->CarriedHousePig();
    const FTransform HallTransform = GetActorTransform();
    const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius() / GetActorScale3D().Y;
    // Preserve the exact longitudinal module period. A held animal also has
    // to fit beside the original open door; prefer the unchanged endpoint,
    // then the smallest bounded lateral clearance, with real collision on.
    for (float SideCorrection : {0.f, -10.f, 10.f, -20.f, 20.f, -30.f, 30.f})
    {
        if (!Pig && SideCorrection != 0.f) break;
        const FVector Candidate = Delta + GetActorRightVector() * SideCorrection * GetActorScale3D().Y;
        const float TargetSide = HallTransform.InverseTransformPosition(Before + Candidate).Y;
        if (SideCorrection != 0.f && FMath::Abs(TargetSide) > CorridorHalfWidth - Radius - 2.f) continue;
        if (Pig && !Pig->CanTranslateWithCarrier(Candidate)) continue;
        // TeleportTo refreshes the actual pawn floor without sweeping across
        // intervening repeated modules, and retains its native fit test.
        if (!Pawn->TeleportTo(Before + Candidate, Rotation, false, false)) continue;
        const FVector ActualDelta = Pawn->GetActorLocation() - Before;
        if (Pig && !Pig->CanTranslateWithCarrier(ActualDelta))
        {
            // Unreal can adjust a teleport destination for the capsule. The
            // animal must fit that adjusted point before either carry cache
            // is translated, otherwise the next carry sweep starts embedded.
            const bool bRestored = Pawn->TeleportTo(Before, Rotation, false, false);
            Movement->Velocity = Velocity;
            if (!bRestored)
            {
                UE_LOG(LogTemp, Error, TEXT("HOUSE_IMPOSSIBLE_HALLWAY_REWIND_ROLLBACK_FAILED pawn=%s before=%s current=%s"),
                    *Pawn->GetName(), *Before.ToCompactString(), *Pawn->GetActorLocation().ToCompactString());
                return false;
            }
            continue;
        }
        Movement->Velocity = Velocity;
        if (Pig) Pig->TranslateWithCarrier(ActualDelta);
        if (Pawn->FurnitureInteraction) Pawn->FurnitureInteraction->TranslateHeldFurniture(ActualDelta);
        if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
            if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();
        Pawn->ForceNetUpdate();
        return true;
    }
    Movement->Velocity = Velocity;
    return false;
}

void AHouseImpossibleHallway::UpdateTraveler(AStoryFirstPersonCharacter* Pawn)
{
    if (!IsValid(Pawn) || !Pawn->HasAuthority() || !Pawn->GetController() || !EntranceDoor.IsValid()) return;
    const FVector Forward = GetActorForwardVector();
    // Module dimensions describe actor-local scenery. Keeping traveler math in
    // that same space preserves the seamless period when the house is resized.
    const FVector Offset = GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
    const FVector Scale = GetActorScale3D();
    float Depth = Offset.X;
    const float Side = Offset.Y;
    const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() / Scale.Z;
    const float FootHeight = Offset.Z - Half;
    const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius() / Scale.X;
    FTraveler* Existing = Travelers.Find(Pawn);
    // Only this doorway's upstairs passage participates. Pawns below it or in
    // the neighboring rooms retain their normal world position.
    if (FMath::Abs(Side) > CorridorHalfWidth + Radius || FootHeight < -35.f || FootHeight > CorridorHeight)
    {
        if (Existing) Existing->bActive = false;
        return;
    }
    if (!Existing)
    {
        if (Depth < Radius + 35.f || Depth > LoopDepth + 60.f || !EntranceDoor->IsOpen()) return;
        Existing = &Travelers.Add(Pawn);
        Existing->bActive = true;
        Existing->LastDepth = Depth;
    }
    FTraveler& State = *Existing;
    if (Depth < 12.f)
    {
        State = FTraveler();
        State.LastDepth = Depth;
        return;
    }
    if (!State.bActive)
    {
        if (Depth < Radius + 35.f || Depth > LoopDepth + 60.f || !EntranceDoor->IsOpen()) return;
        State.bActive = true;
        State.bTurned = false;
        State.LastDepth = Depth;
    }
    VirtualTravel += FMath::Max(0.f, Depth - State.LastDepth) * Scale.X;
    FVector Aim = Pawn->GetBaseAimRotation().Vector();
    Aim.Z = 0.f;
    const float Facing = FVector::DotProduct(Aim.GetSafeNormal(), Forward);
    float Shift = 0.f;
    const bool bReturning = Facing < .57f && !State.bTurned;
    if (bReturning)
    {
        // Rewind before the rear doorway can enter even the widest player FOV.
        // Horizontal aim leaves looking down and jumping completely ordinary.
        while (Depth - Shift >= ReturnMinDepth + ModuleLength) Shift += ModuleLength;
    }
    else if (Facing > .88f) State.bTurned = false;

    // The body's entire travel stays in the original empty room. The scenery
    // beyond it supplies the long sightline; nothing is spawned while walking.
    while (Depth - Shift >= LoopDepth) Shift += ModuleLength;
    if (Shift > 0.f && TranslateTraveler(Pawn, -Forward * Shift * Scale.X))
    {
        if (bReturning) { State.bTurned = true; ++TotalReturns; }
        if (Depth >= LoopDepth) ++TotalLoops;
        Depth = GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation()).X;
    }
    else if (Shift == 0.f && bReturning)
    {
        State.bTurned = true;
        ++TotalReturns;
    }
    State.LastDepth = Depth;
    for (auto It = Travelers.CreateIterator(); It; ++It)
        if (!It.Key().IsValid()) It.RemoveCurrent();
}

void UpdateHouseImpossibleHallwayTraveler(AStoryFirstPersonCharacter* Pawn)
{
    if (!Pawn || !Pawn->HasAuthority()) return;
    for (TActorIterator<AHouseImpossibleHallway> It(Pawn->GetWorld()); It; ++It) It->UpdateTraveler(Pawn);
}
