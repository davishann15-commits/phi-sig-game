#include "SeniorDouli.h"
#include "StoryCampaign.h"
#include "SeniorLobby.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ASeniorDouli::ASeniorDouli()
{
    bReplicates = true; SetReplicateMovement(true);
    SetNetUpdateFrequency(60); SetMinNetUpdateFrequency(30);
    bNetUseOwnerRelevancy = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Hat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Douli"));
    Hat->SetupAttachment(RootComponent);
    Hat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/Weapons/Douli/SM_Douli.SM_Douli"));
    Hat->SetStaticMesh(Mesh.Object);
    FirstPersonHat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonDouli"));
    FirstPersonHat->SetupAttachment(RootComponent); FirstPersonHat->SetStaticMesh(Mesh.Object);
    FirstPersonHat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FirstPersonHat->SetOnlyOwnerSee(true); FirstPersonHat->SetCastShadow(false);
}
void ASeniorDouli::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASeniorDouli, Phase); DOREPLIFETIME(ASeniorDouli, PhaseStart);
}
float ASeniorDouli::GetPhaseAge() const
{
    const AGameStateBase* GS=GetWorld()->GetGameState();
    return FMath::Max(0.f, float(GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds())-PhaseStart);
}
void ASeniorDouli::SetPhase(EDouliPhase Value)
{
    Phase=Value; PhaseStart=GetWorld()->GetTimeSeconds(); OnRep_Phase(); ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("DOULI_PHASE %s %d"), *GetName(), int32(Value));
}
void ASeniorDouli::OnRep_Phase()
{
    if (Phase==EDouliPhase::Outbound)
    {
        Spin=0;
        ReleaseVisualStart=FirstPersonHat->GetComponentTransform();
        bCatchVisualStartValid=false;
    }
    else if (Phase==EDouliPhase::Catching && !bCatchVisualStartValid)
    {
        CatchVisualStart=Hat->GetComponentTransform();
        bCatchVisualStartValid=true;
    }
}
void ASeniorDouli::DamageHit(const FHitResult& Hit)
{
    auto* Pawn=Cast<AStoryFirstPersonCharacter>(GetOwner());
    AActor* Target=Hit.GetActor();
    const APawn* OtherPawn=Cast<APawn>(Target);
    if(HasAuthority() && Pawn && Target && Target->CanBeDamaged()
        && !(OtherPawn && OtherPawn->IsPlayerControlled())
        && (OtherPawn || Target->ActorHasTag(TEXT("Enemy"))))
    {
        UGameplayStatics::ApplyPointDamage(Target,Damage,FlightDirection,Hit,Pawn->GetController(),this,UDamageType::StaticClass());
        UE_LOG(LogTemp,Display,TEXT("DOULI_HIT %s damage=%.1f"),*Target->GetName(),Damage);
    }
}
bool ASeniorDouli::BeginThrow()
{
    AStoryFirstPersonCharacter* Pawn=Cast<AStoryFirstPersonCharacter>(GetOwner());
    const auto* State=Pawn ? Pawn->GetPlayerState<ASeniorLobbyPlayerState>() : nullptr;
    if (!HasAuthority() || Phase!=EDouliPhase::Ready || !Pawn || !Pawn->GetController()
        || !State || State->CharacterIndex!=0 || State->LoadoutIndex!=0) return false;
    DistanceTravelled=0; SetPhase(EDouliPhase::Windup); return true;
}
FString ASeniorDouli::StatusText() const
{
    switch(Phase) {
    case EDouliPhase::Ready: return TEXT("dǒulì  •  THROW [LMB / RT]");
    case EDouliPhase::Windup: return TEXT("dǒulì  •  THROWING");
    case EDouliPhase::Outbound: return TEXT("dǒulì  •  IN FLIGHT");
    case EDouliPhase::Returning: return TEXT("dǒulì  •  RETURNING");
    default: return TEXT("dǒulì  •  CATCHING"); }
}
void ASeniorDouli::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AStoryFirstPersonCharacter* Pawn=Cast<AStoryFirstPersonCharacter>(GetOwner());
    if (!IsValid(Pawn)) { if(HasAuthority()) Destroy(); return; }
    const FTransform WorldGrip=Pawn->DouliGrip(false);
    const FTransform CameraGrip=Pawn->DouliGrip(true);
    const bool bLocal=Pawn->IsLocallyControlled();
    if (HasAuthority())
    {
        const float Age=GetPhaseAge();
        if (IsHeld()) SetActorTransform(WorldGrip);
        if (Phase==EDouliPhase::Windup && Age>=.32f)
        {
            const FRotator Aim=Pawn->GetControlRotation();
            const FVector View=Pawn->GetPawnViewLocation();
            // The replicated projectile starts at the third-person hand so
            // other players do not see it snap to the owner's camera. A short
            // owner-only first-person visual bridges the camera grip to that
            // world flight during the release frames below.
            const FVector Launch=WorldGrip.GetLocation();
            FCollisionQueryParams Q(SCENE_QUERY_STAT(DouliRelease),false,Pawn);Q.AddIgnoredActor(this);
            FHitResult Obstacle,AimHit;
            FVector AimPoint=View+Aim.Vector()*MaxRange;
            if(GetWorld()->LineTraceSingleByChannel(AimHit,View,AimPoint,ECC_Visibility,Q)) AimPoint=AimHit.ImpactPoint;
            FlightDirection=(AimPoint-Launch).GetSafeNormal();
            // Sweep to the release hand first, so the brim cannot start behind a wall.
            if(GetWorld()->SweepSingleByChannel(Obstacle,View,Launch,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(10.f),Q))
            { SetActorLocation(Obstacle.Location);DamageHit(Obstacle);SetPhase(EDouliPhase::Returning); }
            else { SetActorLocation(Launch);SetPhase(EDouliPhase::Outbound); }
        }
        else if (Phase==EDouliPhase::Outbound)
        {
            const float Step=FMath::Min(ThrowSpeed*DeltaSeconds, MaxRange-DistanceTravelled);
            const FVector Start=GetActorLocation(), End=Start+FlightDirection*Step;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(DouliFlight),false,Pawn); Query.AddIgnoredActor(this);
            FHitResult Hit;
            const bool bHit=GetWorld()->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(10.f),Query);
            SetActorLocation(bHit ? Hit.Location : End); DistanceTravelled+=Step;
            if (bHit) DamageHit(Hit);
            if (bHit || DistanceTravelled>=MaxRange || Age>1.2f) SetPhase(EDouliPhase::Returning);
        }
        else if (Phase==EDouliPhase::Returning)
        {
            const FVector Target=bLocal ? CameraGrip.GetLocation() : WorldGrip.GetLocation();
            const FVector To=Target-GetActorLocation();
            // Recall is harmless and ignores scenery, preventing a stuck or lost weapon.
            const float Speed=ReturnSpeed+Age*900.f;
            if (To.Size()<=Speed*DeltaSeconds+10 || Age>3.f)
            {
                CatchVisualStart=Hat->GetComponentTransform();
                bCatchVisualStartValid=true;
                SetActorTransform(WorldGrip);
                SetPhase(EDouliPhase::Catching);
            }
            else SetActorLocation(GetActorLocation()+To.GetSafeNormal()*Speed*DeltaSeconds);
        }
        else if (Phase==EDouliPhase::Catching && Age>=.24f) SetPhase(EDouliPhase::Ready);
    }
    const bool bHeld=IsHeld();
    const bool bReleaseBridge=bLocal && Phase==EDouliPhase::Outbound && GetPhaseAge()<.14f;
    Hat->SetOwnerNoSee(bHeld || bReleaseBridge);
    FirstPersonHat->SetVisibility(bLocal && (bHeld || bReleaseBridge));
    if (bHeld)
    {
        if (Phase==EDouliPhase::Catching && bCatchVisualStartValid)
        {
            const float U=FMath::SmoothStep(0.f,.24f,GetPhaseAge());
            FTransform Visual;
            Visual.Blend(CatchVisualStart,bLocal ? CameraGrip : WorldGrip,U);
            if (bLocal) FirstPersonHat->SetWorldTransform(Visual);
            else Hat->SetWorldTransform(Visual);
        }
        else
        {
            Hat->SetWorldTransform(WorldGrip);
            FirstPersonHat->SetWorldTransform(CameraGrip);
        }
    }
    else
    {
        Spin=FMath::Fmod(Spin+DeltaSeconds*1500.f,360.f);
        Hat->SetRelativeLocation(FVector::ZeroVector);
        const FQuat FlightRotation=FRotator(4.f*FMath::Sin(Spin*.017f),Spin,5.f).Quaternion();
        if (bReleaseBridge)
        {
            const float U=FMath::SmoothStep(0.f,.14f,GetPhaseAge());
            FTransform Visual;
            Visual.Blend(ReleaseVisualStart,
                FTransform(FlightRotation,Hat->GetComponentLocation(),FVector::OneVector),U);
            FirstPersonHat->SetWorldTransform(Visual);
            Hat->SetWorldRotation(FlightRotation);
            Hat->SetWorldScale3D(FVector::OneVector);
        }
        else
        {
            Hat->SetWorldRotation(FlightRotation);
            Hat->SetWorldScale3D(FVector::OneVector);
        }
    }
}
