#include "HouseClubPole.h"
#include "StoryCampaign.h"
#include "StoryMovementComponent.h"
#include "HouseFurnitureInteractionComponent.h"
#include "HouseRoamingPig.h"
#include "SeniorDouli.h"
#include "SeniorSauce.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

AHouseClubPole::AHouseClubPole()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);
    SetNetUpdateFrequency(20.f);
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ClubPoleRoot"));
    SetRootComponent(SceneRoot);
    PoleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PolishedPole"));
    PoleMesh->SetupAttachment(SceneRoot);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    PoleMesh->SetStaticMesh(Cylinder.Object);
    PoleMesh->SetCollisionProfileName(TEXT("BlockAll"));
    PoleMesh->SetGenerateOverlapEvents(false);
    GatePivot = CreateDefaultSubobject<USceneComponent>(TEXT("SafetyGateHinge"));
    GatePivot->SetupAttachment(SceneRoot);
    GatePivot->SetRelativeLocation(FVector(-65,110,386.65f));
    GateVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SafetyGateVisual"));
    GateVisual->SetupAttachment(GatePivot);
    GateVisual->SetStaticMesh(Cube.Object);
    GateVisual->SetRelativeLocation(FVector(65,0,0));
    GateVisual->SetRelativeScale3D(FVector(1.3f,.05f,.06f));
    GateVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GateVisual->SetGenerateOverlapEvents(false);
    GateCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SafetyGateCollision"));
    GateCollision->SetupAttachment(SceneRoot);
    GateCollision->SetRelativeLocation(FVector(0,110,386.65f));
    GateCollision->SetBoxExtent(FVector(65,8,55));
    GateCollision->SetCollisionProfileName(TEXT("BlockAll"));
    GateCollision->SetGenerateOverlapEvents(false);
    GateCollision->SetHiddenInGame(true);
    Tags.Add(TEXT("HouseClubPole"));
}

void AHouseClubPole::BeginPlay()
{
    Super::BeginPlay();
    ClosedGateYaw = GatePivot->GetRelativeRotation().Yaw;
    if (bAutoFitPoleMesh)
    {
        const FVector Delta = PoleTop - PoleBase;
        PoleMesh->SetRelativeLocation((PoleBase + PoleTop) * .5f);
        PoleMesh->SetRelativeRotation(FRotationMatrix::MakeFromZ(Delta).Rotator());
        PoleMesh->SetRelativeScale3D(FVector(PoleRadius/50.f, PoleRadius/50.f, Delta.Size()/100.f));
    }
}

float AHouseClubPole::ServerTime() const
{
    if (const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr)
        return State->GetServerWorldTimeSeconds();
    return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

FVector AHouseClubPole::LandingWorld(bool bUpper) const
{ return GetActorTransform().TransformPosition(bUpper ? UpperLanding : LowerLanding); }
FVector AHouseClubPole::FocusWorld(bool bUpper) const
{
    FVector Focus = GetActorTransform().TransformPosition(bUpper ? UpperShaft : LowerShaft);
    const FVector Pole = GetActorTransform().TransformPosition(bUpper ? FVector(PoleBase.X,PoleBase.Y,UpperShaft.Z) : PoleBase);
    Focus.X = Pole.X; Focus.Y = Pole.Y; Focus.Z += 100.f;
    return Focus;
}

bool AHouseClubPole::FindFocusedEnd(const AStoryFirstPersonCharacter* Pawn, bool& bUpper) const
{
    if (!IsValid(Pawn) || !Pawn->Controller || !Pawn->FirstPersonCamera || Pawn->IsActorBeingDestroyed()
        || Pawn->ClubPoleJourney.Pole || TravellerState.Contains(const_cast<AStoryFirstPersonCharacter*>(Pawn))
        || (Pawn->GetStoryMovement() && Pawn->GetStoryMovement()->IsCrawling())) return false;
    const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
    const FVector Feet = Pawn->GetActorLocation() - FVector(0,0,Capsule->GetScaledCapsuleHalfHeight());
    FVector LowerPole=GetActorTransform().TransformPosition(PoleBase);
    LowerPole.Z=LandingWorld(false).Z;
    const float LowerDistance = FVector::Dist2D(Feet,LowerPole);
    const float UpperDistance = FVector::Dist2D(Feet, LandingWorld(true));
    // Older saved instances can retain the original 85-cm override. The use
    // zone must cover a natural standing approach beside the visible shaft.
    const float Reach=FMath::Max(180.f,InteractionRadius);
    bUpper = FMath::Abs(Feet.Z - LandingWorld(true).Z) < 45.f && UpperDistance <= Reach;
    if (!bUpper && (FMath::Abs(Feet.Z - LandingWorld(false).Z) >= 45.f || LowerDistance > Reach)) return false;
    if (bUpper)
    {
        const FVector Outward=(LandingWorld(true)-GetActorTransform().TransformPosition(UpperShaft)).GetSafeNormal2D();
        const FVector Extent=GateCollision->Bounds.BoxExtent;
        const float GateDepth=FMath::Abs(Outward.X)*Extent.X+FMath::Abs(Outward.Y)*Extent.Y;
        // The top interaction is outside the closed gate. It never offers a
        // route across the fixed guard or requires walking into the aperture.
        if (FVector::DotProduct(Feet-GateCollision->GetComponentLocation(),Outward)
            < GateDepth+Capsule->GetScaledCapsuleRadius()-5.f) return false;
    }
    // PlayerController's GetPlayerViewPoint can return the camera manager's
    // previous-frame cache after movement/input. APawn::GetBaseAimRotation
    // calls it too. The first-person camera uses pawn control rotation, so
    // read current control rotation directly for prompt and server checks.
    const FVector View=Pawn->FirstPersonCamera->GetComponentLocation();
    const FRotator ViewRotation=Pawn->Controller->GetControlRotation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ClubPoleFocus),false,Pawn);
    Query.AddIgnoredComponent(GateCollision.Get());
    if (Pawn->Douli) Query.AddIgnoredActor(Pawn->Douli);
    if (Pawn->SaucePacket) Query.AddIgnoredActor(Pawn->SaucePacket);
    if (AHouseRoamingPig* Pig = Pawn->CarriedHousePig()) Query.AddIgnoredActor(Pig);
    FVector Target=FocusWorld(bUpper);
    const float FloorZ=LandingWorld(bUpper).Z;
    // A tall pole is a visible target, not one invisible low point. Standing
    // level aim, looking down at the entry, and looking slightly up all work.
    const float Heights[]={85.f,FMath::Clamp(float(View.Z-FloorZ),100.f,190.f),225.f};
    for (float Height:Heights)
    {
        Target.Z=FloorZ+Height;
        const FVector Delta=Target-View;
        if (Delta.Size()>Reach+150.f || FVector::DotProduct(ViewRotation.Vector(),Delta.GetSafeNormal())<.55f) continue;
        FHitResult Hit;
        const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,View,Target,ECC_Visibility,Query);
        if (!Blocked || Hit.GetComponent()==PoleMesh) return true;
    }
    return false;
}

FString AHouseClubPole::FocusDiagnostics(const AStoryFirstPersonCharacter* Pawn) const
{
    if (!IsValid(Pawn) || !Pawn->Controller || !Pawn->FirstPersonCamera) return TEXT("missing pawn/controller/camera");
    const UCapsuleComponent* Capsule=Pawn->GetCapsuleComponent();
    const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Capsule->GetScaledCapsuleHalfHeight());
    const bool Upper=FMath::Abs(Feet.Z-LandingWorld(true).Z)<45.f;
    const FVector Eye=Pawn->FirstPersonCamera->GetComponentLocation();
    const FRotator Aim=Pawn->Controller->GetControlRotation();
    FVector CachedEye;FRotator CachedAim;Pawn->Controller->GetPlayerViewPoint(CachedEye,CachedAim);
    const FVector Outward=(LandingWorld(true)-GetActorTransform().TransformPosition(UpperShaft)).GetSafeNormal2D();
    const FVector Extent=GateCollision->Bounds.BoxExtent;
    const float GateDepth=FMath::Abs(Outward.X)*Extent.X+FMath::Abs(Outward.Y)*Extent.Y;
    FString Report=FString::Printf(TEXT("feet=%s lowerDistance=%.3f upperDistance=%.3f reach=%.3f radius=%.3f gateOutward=%.3f gateRequired=%.3f gateExtent=%s actualEye=%s cachedEye=%s actualAim=%s cachedAim=%s"),
        *Feet.ToCompactString(),FVector::Dist2D(Feet,GetActorTransform().TransformPosition(PoleBase)),
        FVector::Dist2D(Feet,LandingWorld(true)),FMath::Max(180.f,InteractionRadius),Capsule->GetScaledCapsuleRadius(),
        FVector::DotProduct(Feet-GateCollision->GetComponentLocation(),Outward),GateDepth+Capsule->GetScaledCapsuleRadius()-5.f,
        *Extent.ToCompactString(),*Eye.ToCompactString(),*CachedEye.ToCompactString(),*Aim.ToCompactString(),*CachedAim.ToCompactString());
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ClubPoleFocusDiagnostic),false,Pawn);
    Query.AddIgnoredComponent(GateCollision.Get());
    if (Pawn->Douli) Query.AddIgnoredActor(Pawn->Douli);
    if (Pawn->SaucePacket) Query.AddIgnoredActor(Pawn->SaucePacket);
    if (AHouseRoamingPig* Pig=Pawn->CarriedHousePig()) Query.AddIgnoredActor(Pig);
    FVector Target=FocusWorld(Upper);
    const float Floor=LandingWorld(Upper).Z;
    const float Heights[]={85.f,FMath::Clamp(float(Eye.Z-Floor),100.f,190.f),225.f};
    for (float Height:Heights)
    {
        Target.Z=Floor+Height;
        const FVector Delta=Target-Eye;
        FHitResult Hit;
        GetWorld()->LineTraceSingleByChannel(Hit,Eye,Target,ECC_Visibility,Query);
        Report+=FString::Printf(TEXT(" targetHeight=%.1f dot=%.4f distance=%.3f blocked=%d hitActor=%s hitComponent=%s"),
            Height,FVector::DotProduct(Aim.Vector(),Delta.GetSafeNormal()),Delta.Size(),Hit.bBlockingHit,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));
    }
    return Report;
}

FString AHouseClubPole::FocusLabel(const AStoryFirstPersonCharacter* Pawn) const
{
    bool bUpper = false;
    if (!FindFocusedEnd(Pawn,bUpper)) return FString();
    if (Occupant && Occupant != Pawn) return TEXT("POLE IN USE");
    if (Pawn->CarriedHousePig()) return TEXT("E  SET DOWN PIG BEFORE USING POLE");
    return bUpper ? TEXT("E  SLIDE DOWN") : TEXT("E  CLIMB UP");
}

float AHouseClubPole::JourneyProgress(const FHouseClubPoleJourney& Journey) const
{
    // Authority's own world clock is monotonic and needs no network offset.
    // Clients use their receipt-anchored clock in UpdateTraveller instead.
    const float Time=GetWorld()?GetWorld()->GetTimeSeconds():0.f;
    return FMath::Clamp((Time-Journey.ServerStartTime)/FMath::Max(Journey.Duration,.01f),0.f,1.f);
}

FVector AHouseClubPole::SampleJourney(const FHouseClubPoleJourney& Journey, float Progress) const
{
    const bool bFromUpper = !Journey.bClimbing;
    const float HH = Journey.CapsuleHalfHeight + 2.f;
    const FVector ShaftA = GetActorTransform().TransformPosition(bFromUpper ? UpperShaft : LowerShaft) + FVector(0,0,HH);
    const FVector ShaftB = GetActorTransform().TransformPosition(bFromUpper ? LowerShaft : UpperShaft) + FVector(0,0,HH);
    const FVector Finish = LandingWorld(!bFromUpper) + FVector(0,0,HH);
    // Tangent-continuous ease at entry, descent/ascent, and stepping away.
    auto Ease = [](float X) { X=FMath::Clamp(X,0.f,1.f); return X*X*(3.f-2.f*X); };
    if (Progress < .18f)
    {
        float Length=0.f;
        FVector Previous=Journey.StartCenter;
        for (const FVector& Point:Journey.EntryPoints) { Length+=FVector::Dist(Previous,Point);Previous=Point; }
        Length+=FVector::Dist(Previous,ShaftA);
        float Remaining=Ease(Progress/.18f)*Length;
        Previous=Journey.StartCenter;
        for (int32 Index=0;Index<=Journey.EntryPoints.Num();++Index)
        {
            const FVector Point=Index<Journey.EntryPoints.Num()?Journey.EntryPoints[Index]:ShaftA;
            const float Segment=FVector::Dist(Previous,Point);
            if (Remaining<=Segment || Index==Journey.EntryPoints.Num())
                return FMath::Lerp(Previous,Point,Segment>.001f?FMath::Clamp(Remaining/Segment,0.f,1.f):1.f);
            Remaining-=Segment;Previous=Point;
        }
    }
    if (Progress < .84f) return FMath::Lerp(ShaftA,ShaftB,Ease((Progress-.18f)/.66f));
    return FMath::Lerp(ShaftB,Finish,Ease((Progress-.84f)/.16f));
}

bool AHouseClubPole::CapsuleClear(const AStoryFirstPersonCharacter* Pawn, const FVector& Center, bool bIgnoreGate, FHitResult* Hit) const
{
    if (!Pawn || !GetWorld()) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ClubPoleCapsuleClear),false,Pawn);
    if (bIgnoreGate) Query.AddIgnoredComponent(GateCollision.Get());
    if (Pawn->Douli) Query.AddIgnoredActor(Pawn->Douli);
    if (Pawn->SaucePacket) Query.AddIgnoredActor(Pawn->SaucePacket);
    const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
    FHitResult LocalHit;
    const bool Blocked = GetWorld()->SweepSingleByChannel(LocalHit,Center,Center+FVector(0,0,.01f),FQuat::Identity,
        ECC_Pawn,FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query);
    if (Hit) *Hit = LocalHit;
    return !Blocked;
}

bool AHouseClubPole::IsEndpointClear(const AStoryFirstPersonCharacter* Pawn, bool bUpper, FString& Why) const
{
    if (!IsValid(Pawn)) { Why=TEXT("No player"); return false; }
    const FVector Floor = LandingWorld(bUpper);
    const FVector Center = Floor + FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f);
    FHitResult Hit;
    if (!CapsuleClear(Pawn,Center,false,&Hit))
    { Why=FString::Printf(TEXT("Landing blocked by %s / %s"),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent())); return false; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ClubPoleLandingFloor),false,Pawn);
    if (!GetWorld()->LineTraceSingleByChannel(Hit,Floor+FVector(0,0,20),Floor-FVector(0,0,25),ECC_Pawn,Query)
        || Hit.bStartPenetrating || Hit.ImpactNormal.Z < .7f || FMath::Abs(Hit.ImpactPoint.Z-Floor.Z)>8.f)
    { Why=FString::Printf(TEXT("Landing lacks level floor: %s point=%s"),*GetNameSafe(Hit.GetActor()),*Hit.ImpactPoint.ToCompactString()); return false; }
    return true;
}

bool AHouseClubPole::CheckRoute(const AStoryFirstPersonCharacter* Pawn, const FHouseClubPoleJourney& Journey, FString& Why) const
{
    if (!IsEndpointClear(Pawn,Journey.bClimbing,Why)) return false;
    const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ClubPoleEntireRoute),false,Pawn);
    Query.AddIgnoredComponent(GateCollision.Get()); // The sole authorized exception.
    if (Pawn->Douli) Query.AddIgnoredActor(Pawn->Douli);
    if (Pawn->SaucePacket) Query.AddIgnoredActor(Pawn->SaucePacket);
    FVector Previous=Journey.StartCenter;
    for (int32 Index=1;Index<=64;++Index)
    {
        const FVector Next=SampleJourney(Journey,float(Index)/64.f);
        FHitResult Hit;
        if (GetWorld()->SweepSingleByChannel(Hit,Previous,Next,FQuat::Identity,ECC_Pawn,
            FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query))
        {
            Why=FString::Printf(TEXT("Route blocked segment=%d by %s / %s at %s penetration=%d"),Index,
                *GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToCompactString(),Hit.bStartPenetrating);
            return false;
        }
        Previous=Next;
    }
    return true;
}

bool AHouseClubPole::TryUse(AStoryFirstPersonCharacter* Pawn, bool bUpper)
{
    if (!HasAuthority() || !IsValid(Pawn) || Occupant || Pawn->ClubPoleJourney.Pole
        || !Pawn->Controller || Pawn->Controller->GetPawn()!=Pawn || Pawn->IsActorBeingDestroyed()) return false;
    bool FocusUpper=false;
    if (!FindFocusedEnd(Pawn,FocusUpper) || FocusUpper!=bUpper) return false;
    if (UStoryMovementComponent* Movement=Pawn->GetStoryMovement(); !Movement || Movement->IsCrawling() || Movement->IsSliding()
        || Movement->MovementMode!=MOVE_Walking) return false;
    if (AHouseRoamingPig* Pig=Pawn->CarriedHousePig())
    {
        if (!Pig->TryPlace(Pawn)) Pawn->ClientClubPoleNotice(TEXT("Put the pig on a clear floor before using the pole."));
        return false; // Wait for the genuine lowering animation to finish.
    }
    if (Pawn->FurnitureInteraction && Pawn->FurnitureInteraction->bHoldingFurniture)
        Pawn->FurnitureInteraction->ReleaseGrab();
    const FVector ShaftDelta=GetActorTransform().TransformVector(UpperShaft-LowerShaft);
    if (ShaftDelta.Z<150.f || ShaftDelta.Z>700.f || ShaftDelta.Size2D()>2.f)
    { Pawn->ClientClubPoleNotice(TEXT("The pole route is unavailable.")); return false; }
    FHouseClubPoleJourney Journey;
    Journey.Pole=this;
    Journey.Serial=Pawn->ClubPoleJourney.Serial+1;
    Journey.bClimbing=!bUpper;
    // Give the atomic reservation/journey a short LAN replication head start.
    Journey.ServerStartTime=ServerTime()+.18f;
    Journey.Duration=bUpper?SlideSeconds:ClimbSeconds;
    Journey.StartCenter=Pawn->GetActorLocation();
    Journey.CapsuleHalfHeight=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector EntryLanding=LandingWorld(bUpper)+FVector(0,0,Journey.CapsuleHalfHeight+2.f);
    const FVector ShaftEntry=GetActorTransform().TransformPosition(bUpper?UpperShaft:LowerShaft)
        +FVector(0,0,Journey.CapsuleHalfHeight+2.f);
    if (!bUpper)
    {
        FVector PoleCenter=GetActorTransform().TransformPosition(PoleBase);
        PoleCenter.Z=EntryLanding.Z;
        const FVector Nearest=FMath::ClosestPointOnSegment(PoleCenter,Journey.StartCenter,EntryLanding);
        const float Clearance=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius()+PoleRadius+10.f;
        if (FVector::Dist2D(PoleCenter,Nearest)<Clearance)
        {
            const FVector Outward=(EntryLanding-ShaftEntry).GetSafeNormal2D();
            const FVector Side(Outward.Y,-Outward.X,0.f);
            const FVector Delta=Journey.StartCenter-PoleCenter;
            const float Sign=FVector::DotProduct(Delta,Side)<0.f?-1.f:1.f;
            const float SideDistance=FMath::Max(110.f,Clearance+25.f);
            Journey.EntryPoints.Add(PoleCenter+Side*(Sign*SideDistance)
                +Outward*FMath::Min(float(FVector::DotProduct(Delta,Outward)),0.f));
            Journey.EntryPoints.Add(EntryLanding+Side*(Sign*SideDistance));
        }
    }
    // Align outside the top gate before entering its narrow opening. Lower
    // approaches use the same alignment, with a side route if behind the pole.
    Journey.EntryPoints.Add(EntryLanding);
    float EntryLength=0.f;
    FVector Previous=Journey.StartCenter;
    for (const FVector& Point:Journey.EntryPoints) { EntryLength+=FVector::Dist(Previous,Point);Previous=Point; }
    EntryLength+=FVector::Dist(Previous,ShaftEntry);
    Journey.Duration+=FMath::Max(0.f,EntryLength-160.f)/180.f;
    FString Why;
    if (!CheckRoute(Pawn,Journey,Why))
    {
        UE_LOG(LogTemp,Warning,TEXT("CLUB_POLE_REJECT direction=%s %s"),bUpper?TEXT("down"):TEXT("up"),*Why);
        Pawn->ClientClubPoleNotice(TEXT("Step clear of the pole. Its path or landing is blocked."));
        return false;
    }
    Occupant=Pawn;
    Pawn->SetClubPoleJourney(Journey);
    ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_START mode=%d pawn=%s direction=%s serial=%u duration=%.2f start=%s destination=%s"),
        int32(GetNetMode()),*Pawn->GetName(),bUpper?TEXT("down"):TEXT("up"),Journey.Serial,Journey.Duration,
        *Journey.StartCenter.ToCompactString(),*SampleJourney(Journey,1).ToCompactString());
    return true;
}

void AHouseClubPole::ApplyJourney(AStoryFirstPersonCharacter* Pawn, const FHouseClubPoleJourney& Journey)
{
    if (!IsValid(Pawn) || TravellerState.Contains(Pawn)) return;
    UCharacterMovementComponent* Movement=Pawn->GetCharacterMovement();
    FTravellerRestore Restore;
    Restore.Controller=Cast<APlayerController>(Pawn->Controller);
    Restore.PreviousMode=uint8(Movement->MovementMode);
    Restore.PreviousCustomMode=Movement->CustomMovementMode;
    Restore.bIgnoreCorrections=Movement->bIgnoreClientMovementErrorChecksAndCorrection;
    Restore.bIgnoreClientCorrections=Movement->bClientIgnoreMovementCorrections;
    Restore.bAcceptClientPosition=Movement->bServerAcceptClientAuthoritativePosition;
    Restore.Serial=Journey.Serial;
    Restore.LastPosition=Pawn->GetActorLocation();
    Restore.Journey=Journey;
    const float EstimatedAge=ServerTime()-Journey.ServerStartTime;
    // GameState's first clock estimate can be seconds wrong during cold load.
    // It can seed only a plausible LAN age; it never drives a later frame.
    Restore.LocalElapsed=FMath::Clamp(EstimatedAge,-.18f,.15f);
    Restore.bAlreadyIgnoredGate=Pawn->GetCapsuleComponent()->GetMoveIgnoreComponents().Contains(GateCollision);
    Pawn->GetCapsuleComponent()->IgnoreComponentWhenMoving(GateCollision,true);
    if (APlayerController* PC=Restore.Controller.Get(); PC && Pawn->IsLocallyControlled())
    { PC->SetIgnoreMoveInput(true); Restore.bIgnoredMove=true; }
    Movement->StopMovementImmediately();
    Movement->bIgnoreClientMovementErrorChecksAndCorrection=true;
    Movement->bClientIgnoreMovementCorrections=true;
    Movement->bServerAcceptClientAuthoritativePosition=false;
    Movement->DisableMovement();
    if (UStoryMovementComponent* Story=Pawn->GetStoryMovement())
    { Story->SetSprintHeld(false); Story->SetSlideHeld(false); }
    TravellerState.Add(Pawn,Restore);
    UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_REPLICA_BEGIN mode=%d local=%d pawn=%s serial=%u serverEstimate=%.6f serverStart=%.6f estimatedAge=%.6f anchoredAge=%.6f location=%s"),
        int32(GetNetMode()),Pawn->IsLocallyControlled(),*Pawn->GetName(),Journey.Serial,ServerTime(),Journey.ServerStartTime,
        EstimatedAge,Restore.LocalElapsed,*Pawn->GetActorLocation().ToCompactString());
}

bool AHouseClubPole::SweepTravellerRoute(AStoryFirstPersonCharacter* Pawn,FTravellerRestore& Restore,float Progress)
{
    Progress=FMath::Clamp(FMath::Max(Restore.Progress,Progress),0.f,1.f);
    const int32 Steps=FMath::Max(1,FMath::CeilToInt((Progress-Restore.Progress)/.025f));
    const float From=Restore.Progress;
    // Follow the actual entry/shaft/exit curve even when an end packet arrives
    // early. A single diagonal sweep could incorrectly cross the upper floor.
    for (int32 Index=1;Index<=Steps;++Index)
    {
        const float StepProgress=FMath::Lerp(From,Progress,float(Index)/Steps);
        const FVector Target=SampleJourney(Restore.Journey,StepProgress);
        FHitResult Hit;
        Pawn->SetActorLocation(Target,true,&Hit,ETeleportType::None);
        if (Hit.bStartPenetrating || (Hit.bBlockingHit && FVector::Dist(Pawn->GetActorLocation(),Target)>2.f))
            return false;
        Restore.Progress=StepProgress;
    }
    const FVector Actual=Pawn->GetActorLocation();
    Restore.MaximumStep=FMath::Max(Restore.MaximumStep,float(FVector::Dist(Actual,Restore.LastPosition)));
    Restore.LastPosition=Actual;
    ++Restore.Samples;
    return true;
}

void AHouseClubPole::FinishReplicatedTraveller(AStoryFirstPersonCharacter* Pawn,const FHouseClubPoleJourney& Previous,const FHouseClubPoleJourney& Finished)
{
    FTravellerRestore* Restore=TravellerState.Find(Pawn);
    if (!Restore) return;
    const bool bSuccessful=Finished.bCompleted && Finished.Serial==Previous.Serial && !Finished.Pole;
    if (HasAuthority() || !bSuccessful)
    {
        RestoreTraveller(Pawn);
        return;
    }
    const FVector Expected=SampleJourney(Previous,1.f);
    const float Distance=FVector::Dist(Pawn->GetActorLocation(),Expected);
    UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_RECONCILE_BEGIN mode=%d local=%d pawn=%s serial=%u successful=1 progress=%.6f distance=%.3f current=%s expected=%s"),
        int32(GetNetMode()),Pawn->IsLocallyControlled(),*Pawn->GetName(),Previous.Serial,Restore->Progress,Distance,
        *Pawn->GetActorLocation().ToCompactString(),*Expected.ToCompactString());
    if (Distance<=2.f || Restore->Progress>=1.f) { RestoreTraveller(Pawn);return; }
    // Keep the capsule, gate exception and input reservation until the final
    // validated route segment has completed, rather than releasing midshaft.
    Restore->bFinishing=true;
    Restore->FinishElapsed=0.f;
    Restore->FinishFromProgress=Restore->Progress;
}

void AHouseClubPole::UpdateFinishedTraveller(AStoryFirstPersonCharacter* Pawn,float DeltaSeconds)
{
    FTravellerRestore* Restore=TravellerState.Find(Pawn);
    if (!Restore || !Restore->bFinishing || !IsValid(Pawn)) return;
    Pawn->GetCharacterMovement()->DisableMovement();
    Pawn->GetCharacterMovement()->Velocity=FVector::ZeroVector;
    Restore->FinishElapsed+=FMath::Max(0.f,DeltaSeconds);
    // Normal end-packet lag is tiny. For a delayed join, preserve a reasonable
    // travel speed rather than teleporting the remainder of a whole ride.
    const float Seconds=FMath::Clamp((1.f-Restore->FinishFromProgress)*Restore->Journey.Duration,.18f,2.f);
    const float T=FMath::Clamp(Restore->FinishElapsed/Seconds,0.f,1.f);
    const float Progress=FMath::Lerp(Restore->FinishFromProgress,1.f,T*T*(3.f-2.f*T));
    if (!SweepTravellerRoute(Pawn,*Restore,Progress))
    {
        UE_LOG(LogTemp,Warning,TEXT("CLUB_POLE_RECONCILE_BLOCKED pawn=%s serial=%u location=%s"),
            *Pawn->GetName(),Restore->Serial,*Pawn->GetActorLocation().ToCompactString());
        RestoreTraveller(Pawn); // A new local obstruction still obeys gravity.
        return;
    }
    if (T>=1.f)
    {
        UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_RECONCILE_COMPLETE pawn=%s serial=%u location=%s"),
            *Pawn->GetName(),Restore->Serial,*Pawn->GetActorLocation().ToCompactString());
        RestoreTraveller(Pawn);
    }
}

void AHouseClubPole::RestoreTraveller(AStoryFirstPersonCharacter* Pawn)
{
    FTravellerRestore Restore;
    if (!Pawn || !TravellerState.RemoveAndCopyValue(Pawn,Restore)) return;
    UCharacterMovementComponent* Movement=Pawn->GetCharacterMovement();
    if (!Restore.bAlreadyIgnoredGate) Pawn->GetCapsuleComponent()->IgnoreComponentWhenMoving(GateCollision,false);
    if (Restore.bIgnoredMove && Restore.Controller.IsValid()) Restore.Controller->SetIgnoreMoveInput(false);
    Movement->bIgnoreClientMovementErrorChecksAndCorrection=Restore.bIgnoreCorrections;
    Movement->bClientIgnoreMovementCorrections=Restore.bIgnoreClientCorrections;
    Movement->bServerAcceptClientAuthoritativePosition=Restore.bAcceptClientPosition;
    Movement->StopMovementImmediately();
    // On cancellation inside the shaft let real gravity land the capsule.
    FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ClubPoleRestoreFloor),false,Pawn);
    Query.AddIgnoredComponent(GateCollision.Get());
    FHitResult Floor;
    const bool Ground=GetWorld()->LineTraceSingleByChannel(Floor,Feet+FVector(0,0,4),Feet-FVector(0,0,12),ECC_Pawn,Query)
        && Floor.ImpactNormal.Z>.7f;
    Movement->SetMovementMode(Ground?MOVE_Walking:MOVE_Falling);
    UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_REPLICA_END mode=%d local=%d pawn=%s serial=%u samples=%d maxStep=%.3f location=%s"),
        int32(GetNetMode()),Pawn->IsLocallyControlled(),*Pawn->GetName(),Restore.Serial,Restore.Samples,Restore.MaximumStep,
        *Pawn->GetActorLocation().ToCompactString());
}

void AHouseClubPole::CompleteTraveller(AStoryFirstPersonCharacter* Pawn)
{
    if (!HasAuthority() || !Pawn || Occupant!=Pawn) return;
    bLastCompletedUp=Pawn->ClubPoleJourney.bClimbing;
    ++CompletedJourneys;
    UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_COMPLETE mode=%d pawn=%s direction=%s completed=%d location=%s"),
        int32(GetNetMode()),*Pawn->GetName(),bLastCompletedUp?TEXT("up"):TEXT("down"),CompletedJourneys,*Pawn->GetActorLocation().ToCompactString());
    FHouseClubPoleJourney End=Pawn->ClubPoleJourney; End.Pole=nullptr; End.bCompleted=true;
    Pawn->SetClubPoleJourney(End);
    Occupant=nullptr;
    ForceNetUpdate();
}

void AHouseClubPole::AbortTraveller(AStoryFirstPersonCharacter* Pawn, const TCHAR* Why)
{
    if (!HasAuthority() || !Pawn || Occupant!=Pawn) return;
    ++AbortedJourneys;
    UE_LOG(LogTemp,Warning,TEXT("CLUB_POLE_ABORT pawn=%s reason=%s"),*GetNameSafe(Pawn),Why);
    FHouseClubPoleJourney End=Pawn->ClubPoleJourney; End.Pole=nullptr; End.bCompleted=false;
    Pawn->SetClubPoleJourney(End);
    Occupant=nullptr;
    ForceNetUpdate();
}

void AHouseClubPole::UpdateTraveller(AStoryFirstPersonCharacter* Pawn, float DeltaSeconds)
{
    if (!IsValid(Pawn) || Pawn->ClubPoleJourney.Pole!=this) return;
    const FHouseClubPoleJourney& Journey=Pawn->ClubPoleJourney;
    if (!TravellerState.Contains(Pawn)) ApplyJourney(Pawn,Journey);
    if (HasAuthority() && (!Pawn->Controller || Pawn->Controller->GetPawn()!=Pawn || Pawn->IsActorBeingDestroyed()))
    { AbortTraveller(Pawn,TEXT("death, disconnect, or possession loss")); return; }
    UCharacterMovementComponent* Movement=Pawn->GetCharacterMovement();
    Movement->DisableMovement();
    Movement->Velocity=FVector::ZeroVector;
    FTravellerRestore* Restore=TravellerState.Find(Pawn);
    if (!Restore) return;
    Restore->LocalElapsed+=FMath::Max(0.f,DeltaSeconds);
    const float Progress=HasAuthority()?JourneyProgress(Journey)
        :FMath::Clamp(Restore->LocalElapsed/FMath::Max(Journey.Duration,.01f),0.f,1.f);
    // All world collision stays enabled; only the reserved gate is ignored.
    if (!SweepTravellerRoute(Pawn,*Restore,Progress))
    {
        if (HasAuthority()) AbortTraveller(Pawn,TEXT("a dynamic obstruction entered the collision-tested route"));
        return;
    }
    if (HasAuthority() && Progress>=1.f) CompleteTraveller(Pawn);
}

void AHouseClubPole::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TArray<TWeakObjectPtr<AStoryFirstPersonCharacter>> Finishing;
    for (const auto& Pair:TravellerState) if (Pair.Value.bFinishing) Finishing.Add(Pair.Key);
    for (auto Pawn:Finishing) if (Pawn.IsValid()) UpdateFinishedTraveller(Pawn.Get(),DeltaSeconds);
    const float TargetYaw=ClosedGateYaw+(Occupant?GateOpenYaw:0.f);
    FRotator Rotation=GatePivot->GetRelativeRotation();
    Rotation.Yaw=FMath::FInterpTo(Rotation.Yaw,TargetYaw,DeltaSeconds,7.f);
    GatePivot->SetRelativeRotation(Rotation);
    if (HasAuthority() && Occupant && (!IsValid(Occupant) || !Occupant->Controller || Occupant->Controller->GetPawn()!=Occupant))
    {
        if (IsValid(Occupant)) AbortTraveller(Occupant,TEXT("traveller no longer possessed"));
        else { Occupant=nullptr; ++AbortedJourneys; ForceNetUpdate(); }
    }
}

void AHouseClubPole::MarkMultiplayerSmokeClientReady(AStoryFirstPersonCharacter* Pawn)
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority() || !FParse::Param(FCommandLine::Get(),TEXT("ClubPoleMultiplayerSmoke"))
        || !IsValid(Pawn) || !Pawn->Controller || Pawn->IsLocallyControlled() || Pawn->ClubPoleJourney.Pole) return;
    if (FVector::Dist2D(Pawn->GetActorLocation(),LandingWorld(false))>180.f
        || FMath::Abs(Pawn->GetActorLocation().Z-LandingWorld(false).Z)>130.f) return;
    bMultiplayerSmokeClientReady=true;
    UE_LOG(LogTemp,Display,TEXT("CLUB_POLE_MP_REMOTE_READY_RECEIVED pawn=%s location=%s"),
        *Pawn->GetName(),*Pawn->GetActorLocation().ToCompactString());
#endif
}

void AHouseClubPole::EndPlay(const EEndPlayReason::Type Reason)
{
    if (HasAuthority() && IsValid(Occupant)) AbortTraveller(Occupant,TEXT("pole removed or world ended"));
    TArray<TWeakObjectPtr<AStoryFirstPersonCharacter>> Pawns; TravellerState.GetKeys(Pawns);
    for (auto Pawn : Pawns) if (Pawn.IsValid()) RestoreTraveller(Pawn.Get());
    Super::EndPlay(Reason);
}

void AHouseClubPole::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHouseClubPole,LowerLanding); DOREPLIFETIME(AHouseClubPole,UpperLanding);
    DOREPLIFETIME(AHouseClubPole,LowerShaft); DOREPLIFETIME(AHouseClubPole,UpperShaft);
    DOREPLIFETIME(AHouseClubPole,PoleBase); DOREPLIFETIME(AHouseClubPole,PoleTop);
    DOREPLIFETIME(AHouseClubPole,Occupant); DOREPLIFETIME(AHouseClubPole,CompletedJourneys);
    DOREPLIFETIME(AHouseClubPole,AbortedJourneys); DOREPLIFETIME(AHouseClubPole,bLastCompletedUp);
}
