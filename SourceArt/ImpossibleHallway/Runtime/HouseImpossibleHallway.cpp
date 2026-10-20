#include "HouseImpossibleHallway.h"
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
    // TeleportTo refreshes the floor without sweeping across the intervening
    // modules. The exact period keeps all nearby visible surfaces in place.
    if (!Pawn->TeleportTo(Before + Delta, Pawn->GetActorRotation(), false, false)) return false;
    const FVector ActualDelta = Pawn->GetActorLocation() - Before;
    Movement->Velocity = Velocity;
    if (AHouseRoamingPig* Pig = Pawn->CarriedHousePig()) Pig->TranslateWithCarrier(ActualDelta);
    if (Pawn->FurnitureInteraction) Pawn->FurnitureInteraction->TranslateHeldFurniture(ActualDelta);
    if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();
    Pawn->ForceNetUpdate();
    return true;
}

void AHouseImpossibleHallway::UpdateTraveler(AStoryFirstPersonCharacter* Pawn)
{
    if (!IsValid(Pawn) || !Pawn->HasAuthority() || !Pawn->GetController() || !EntranceDoor.IsValid()) return;
    const FVector Forward = GetActorForwardVector();
    const FVector Offset = Pawn->GetActorLocation() - GetActorLocation();
    float Depth = FVector::DotProduct(Offset, Forward);
    const float Side = FVector::DotProduct(Offset, GetActorRightVector());
    const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float FootHeight = Offset.Z - Half;
    const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
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
    VirtualTravel += FMath::Max(0.f, Depth - State.LastDepth);
    FVector Aim = Pawn->GetBaseAimRotation().Vector();
    Aim.Z = 0.f;
    const float Facing = FVector::DotProduct(Aim.GetSafeNormal(), Forward);
    float Shift = 0.f;
    if (Facing < .57f && !State.bTurned)
    {
        // Rewind before the rear doorway can enter even the widest player FOV.
        // Horizontal aim leaves looking down and jumping completely ordinary.
        State.bTurned = true;
        ++TotalReturns;
        while (Depth - Shift >= ReturnMinDepth + ModuleLength) Shift += ModuleLength;
    }
    else if (Facing > .88f) State.bTurned = false;

    // The body's entire travel stays in the original empty room. The scenery
    // beyond it supplies the long sightline; nothing is spawned while walking.
    while (Depth - Shift >= LoopDepth) Shift += ModuleLength;
    if (Shift > 0.f && TranslateTraveler(Pawn, -Forward * Shift))
    {
        if (Depth >= LoopDepth) ++TotalLoops;
        Depth = FVector::DotProduct(Pawn->GetActorLocation() - GetActorLocation(), Forward);
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
