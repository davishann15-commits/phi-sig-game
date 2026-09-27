#include "SeniorSauce.h"
#include "SeniorSauceVisual.h"
#include "StoryCampaign.h"
#include "SeniorLobby.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

void USeniorSauceSlowComponent::ApplySpeed(float Speed)
{
    if(auto* Character=Cast<ACharacter>(GetOwner()))
        if(auto* Movement=Character->GetCharacterMovement())Movement->MaxWalkSpeed=Speed;
}
void USeniorSauceSlowComponent::AddSource()
{
    if(!GetOwner() || !GetOwner()->HasAuthority())return;
    if(Sources++==0)
    {
        const auto* Character=Cast<ACharacter>(GetOwner());
        OriginalSpeed=Character && Character->GetCharacterMovement()?Character->GetCharacterMovement()->MaxWalkSpeed:0.f;
        if(OriginalSpeed>0.f)ApplySpeed(OriginalSpeed*.45f);
    }
}
void USeniorSauceSlowComponent::RemoveSource()
{
    if(!GetOwner() || !GetOwner()->HasAuthority() || Sources<=0)return;
    if(--Sources==0)
    {
        if(OriginalSpeed>0.f)ApplySpeed(OriginalSpeed);
        OriginalSpeed=0.f;
    }
}
void USeniorSauceSlowComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Sources>0 && OriginalSpeed>0.f && GetOwner() && GetOwner()->HasAuthority())ApplySpeed(OriginalSpeed);
    Super::EndPlay(Reason);
}

ASeniorSaucePuddle::ASeniorSaucePuddle()
{
    bReplicates=true;SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick=false;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("PuddleRoot"));
    Area=CreateDefaultSubobject<USphereComponent>(TEXT("SlowArea"));
    Area->SetupAttachment(RootComponent);Area->SetRelativeLocation(FVector(0,0,65));
    Area->InitSphereRadius(Radius);
    Area->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Area->SetCollisionResponseToAllChannels(ECR_Ignore);
    Area->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
    Area->SetGenerateOverlapEvents(true);
    Area->OnComponentBeginOverlap.AddDynamic(this,&ASeniorSaucePuddle::Enter);
    Area->OnComponentEndOverlap.AddDynamic(this,&ASeniorSaucePuddle::Leave);
    UStaticMesh* Disc=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    for(int32 I=0;I<5;++I)
    {
        auto* Piece=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("SauceBlob%d"),I));
        Piece->SetupAttachment(RootComponent);Piece->SetStaticMesh(Disc);
        Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        const float Angle=I*2.f*PI/5.f;
        Piece->SetRelativeLocation(FVector(FMath::Cos(Angle)*36.f,FMath::Sin(Angle)*39.f,1.1f+I*.14f));
        Piece->SetRelativeScale3D(FVector(I==0?2.45f:1.72f,I==0?2.1f:1.35f,.045f));
        Splashes.Add(Piece);
    }
}
void ASeniorSaucePuddle::BeginPlay()
{
    Super::BeginPlay();
    Area->SetSphereRadius(Radius);
    UMaterialInterface* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Weapons/Sauce/M_SauceColor.M_SauceColor"));
    for(int32 I=0;I<Splashes.Num();++I)
    {
        if(!Base || !Splashes[I])continue;
        auto* Material=UMaterialInstanceDynamic::Create(Base,this);
        Material->SetVectorParameterValue(TEXT("BaseColor"),I%2==0?FLinearColor(.75f,.29f,.025f):FLinearColor(.91f,.45f,.04f));
        Material->SetScalarParameterValue(TEXT("Roughness"),.28f);
        Splashes[I]->SetMaterial(0,Material);
    }
    if(HasAuthority())
    {
        SetLifeSpan(Duration);
        TArray<AActor*> Inside;Area->GetOverlappingActors(Inside,ACharacter::StaticClass());
        for(AActor* Actor:Inside)AddEnemy(Actor);
    }
}
void ASeniorSaucePuddle::AddEnemy(AActor* Actor)
{
    if(!HasAuthority())return;
    auto* Character=Cast<ACharacter>(Actor);
    if(!Character || Character->IsPlayerControlled() ||
        (!Character->ActorHasTag(TEXT("Enemy")) && !Character->GetController()))return;
    if(Affected.Contains(Character))return;
    auto* Slow=Character->FindComponentByClass<USeniorSauceSlowComponent>();
    if(!Slow)
    {
        Slow=NewObject<USeniorSauceSlowComponent>(Character);
        Character->AddInstanceComponent(Slow);Slow->RegisterComponent();
    }
    Slow->AddSource();Affected.Add(Character,Slow);
    UE_LOG(LogTemp,Display,TEXT("SAUCE_SLOWED %s speed=%.1f"),*Character->GetName(),Character->GetCharacterMovement()->MaxWalkSpeed);
}
void ASeniorSaucePuddle::RemoveEnemy(AActor* Actor)
{
    auto* Character=Cast<ACharacter>(Actor);
    if(!Character)return;
    if(auto* Slow=Affected.Find(Character))if(Slow->IsValid())Slow->Get()->RemoveSource();
    Affected.Remove(Character);
}
void ASeniorSaucePuddle::Enter(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32,bool,const FHitResult&)
{ AddEnemy(Other); }
void ASeniorSaucePuddle::Leave(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32)
{ RemoveEnemy(Other); }
void ASeniorSaucePuddle::EndPlay(const EEndPlayReason::Type Reason)
{
    if(HasAuthority())
        for(const auto& Pair:Affected)if(Pair.Value.IsValid())Pair.Value->RemoveSource();
    Affected.Empty();Super::EndPlay(Reason);
}

ASeniorSaucePacket::ASeniorSaucePacket()
{
    bReplicates=true;SetReplicateMovement(true);bNetUseOwnerRelevancy=true;
    SetNetUpdateFrequency(60);SetMinNetUpdateFrequency(30);
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("PacketRoot"));
}
void ASeniorSaucePacket::BeginPlay()
{
    Super::BeginPlay();
    WorldPacket=SeniorSauceVisual::Build(this,RootComponent,false);
    FirstPersonPacket=SeniorSauceVisual::Build(this,RootComponent,true);
}
void ASeniorSaucePacket::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ASeniorSaucePacket,Phase);
}
bool ASeniorSaucePacket::BeginThrow()
{
    auto* Pawn=Cast<AStoryFirstPersonCharacter>(GetOwner());
    const auto* State=Pawn?Pawn->GetPlayerState<ASeniorLobbyPlayerState>():nullptr;
    if(!HasAuthority() || Phase!=ESeniorSaucePhase::Ready || !Pawn || !Pawn->GetController() ||
        !State || State->CharacterIndex!=0 || State->LoadoutIndex!=1)return false;
    const FRotator Aim=Pawn->GetControlRotation();
    const FVector View=Pawn->GetPawnViewLocation();
    const FVector Launch=View+Aim.RotateVector(FVector(43,8,-22));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SauceLaunch),false,Pawn);Query.AddIgnoredActor(this);
    FHitResult Obstacle;
    Phase=ESeniorSaucePhase::Flying;Distance=0;Spin=0;
    Velocity=Aim.Vector()*ThrowSpeed+FVector(0,0,170);
    if(GetWorld()->SweepSingleByChannel(Obstacle,View,Launch,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(5),Query))
    {
        SetActorLocation(Obstacle.Location);Burst(&Obstacle);
    }
    else SetActorLocation(Launch);
    ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("SAUCE_THROW %s"),*Pawn->GetName());
    return true;
}
FString ASeniorSaucePacket::StatusText() const
{
    switch(Phase)
    {
    case ESeniorSaucePhase::Ready:return TEXT("SMACKDOWN SAUCE  •  THROW [LMB / RT]");
    case ESeniorSaucePhase::Flying:return TEXT("SMACKDOWN SAUCE  •  IN FLIGHT");
    default:return TEXT("SMACKDOWN SAUCE  •  NEXT PACKET");
    }
}
void ASeniorSaucePacket::Burst(const FHitResult* Impact)
{
    if(!HasAuthority())return;
    auto* Pawn=Cast<AStoryFirstPersonCharacter>(GetOwner());
    if(Impact && Pawn)
    {
        AActor* Target=Impact->GetActor();
        const APawn* OtherPawn=Cast<APawn>(Target);
        if(Target && Target->CanBeDamaged() && !(OtherPawn && OtherPawn->IsPlayerControlled()) &&
            (OtherPawn || Target->ActorHasTag(TEXT("Enemy"))))
            UGameplayStatics::ApplyPointDamage(Target,8.f,Velocity.GetSafeNormal(),*Impact,Pawn->GetController(),this,UDamageType::StaticClass());
    }
    FVector Point=Impact?FVector(Impact->ImpactPoint):GetActorLocation();
    FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(SauceGround),false,this);
    if(Pawn)GroundQuery.AddIgnoredActor(Pawn);
    if(Impact && Impact->GetActor())GroundQuery.AddIgnoredActor(Impact->GetActor());
    FHitResult Ground;
    if(GetWorld()->LineTraceSingleByChannel(Ground,Point+FVector(0,0,80),Point-FVector(0,0,1100),ECC_Visibility,GroundQuery)
        && Ground.ImpactNormal.Z>.45f)Point=Ground.ImpactPoint;
    FActorSpawnParameters Params;Params.Owner=Pawn;Params.Instigator=Pawn;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if(auto* Puddle=GetWorld()->SpawnActor<ASeniorSaucePuddle>(Point+FVector(0,0,2),FRotator::ZeroRotator,Params))
        UE_LOG(LogTemp,Display,TEXT("SAUCE_SPLASH %s radius=%f duration=%f"),*Puddle->GetName(),Puddle->Radius,Puddle->Duration);
    Phase=ESeniorSaucePhase::Recovering;
    ReadyAt=GetWorld()->GetTimeSeconds()+Cooldown;
    ForceNetUpdate();
}
void ASeniorSaucePacket::SetPacketVisible(USceneComponent* Visual,bool Visible,bool HideOwner)
{
    if(!Visual)return;
    TArray<USceneComponent*> Pieces;Visual->GetChildrenComponents(true,Pieces);
    for(USceneComponent* Piece:Pieces)if(auto* Primitive=Cast<UPrimitiveComponent>(Piece))
    { Primitive->SetVisibility(Visible);Primitive->SetOwnerNoSee(HideOwner); }
}
void ASeniorSaucePacket::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* Pawn=Cast<AStoryFirstPersonCharacter>(GetOwner());
    if(!IsValid(Pawn)){if(HasAuthority())Destroy();return;}
    if(HasAuthority())
    {
        if(Phase==ESeniorSaucePhase::Ready)SetActorTransform(Pawn->SauceGrip(false));
        else if(Phase==ESeniorSaucePhase::Flying)
        {
            Velocity.Z-=980.f*DeltaSeconds;
            const FVector Start=GetActorLocation(),End=Start+Velocity*DeltaSeconds;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(SauceFlight),false,Pawn);Query.AddIgnoredActor(this);
            FHitResult Hit;
            const bool bHit=GetWorld()->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(5),Query);
            Distance+=FVector::Distance(Start,bHit?Hit.Location:End);
            SetActorLocation(bHit?Hit.Location:End);
            if(bHit || Distance>1800.f || Velocity.Z < -1700.f)Burst(bHit?&Hit:nullptr);
        }
        else if(GetWorld()->GetTimeSeconds()>=ReadyAt)
        { Phase=ESeniorSaucePhase::Ready;ForceNetUpdate(); }
    }
    const bool bHeld=Phase==ESeniorSaucePhase::Ready;
    const bool bFlight=Phase==ESeniorSaucePhase::Flying;
    SetPacketVisible(WorldPacket,bHeld||bFlight,bHeld);
    SetPacketVisible(FirstPersonPacket,bHeld && Pawn->IsLocallyControlled(),false);
    if(bHeld)
    {
        if(WorldPacket)WorldPacket->SetWorldTransform(Pawn->SauceGrip(false));
        if(FirstPersonPacket)FirstPersonPacket->SetWorldTransform(Pawn->SauceGrip(true));
    }
    else if(bFlight && WorldPacket)
    {
        Spin=FMath::Fmod(Spin+DeltaSeconds*650.f,360.f);
        WorldPacket->SetRelativeTransform(FTransform(FRotator(10,Spin,8),FVector::ZeroVector));
    }
}
