#include "HouseRoamingPig.h"
#include "ExistingHouseDoor.h"
#include "StoryCampaign.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

AHousePigSnackPile::AHousePigSnackPile()
{
    bReplicates=true;
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickInterval=.25f;
    Cheetos=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("EdibleCheetos"));
    SetRootComponent(Cheetos);
    Cheetos->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Cheetos->SetGenerateOverlapEvents(false);
    Cheetos->SetCanEverAffectNavigation(false);
    Cheetos->SetCastShadow(false);
}
void AHousePigSnackPile::RefreshFood()
{
    Cheetos->SetStaticMesh(CheetoMesh); Cheetos->ClearInstances();
    FRandomStream Random(ScatterSeed);
    const int32 Count=FMath::Clamp(FoodCount,1,24);
    const int32 Remaining=HasActorBegunPlay()?FMath::Clamp(FoodRemaining,0,Count):Count;
    float FloorOffset=0.f;
    if (GetWorld())
    {
        FCollisionQueryParams Ground(SCENE_QUERY_STAT(PigSnackFloor),false,this);
        for (TActorIterator<APawn> It(GetWorld());It;++It) Ground.AddIgnoredActor(*It);
        FHitResult Floor;
        if (GetWorld()->LineTraceSingleByChannel(Floor,GetActorLocation()+FVector(0,0,5),GetActorLocation()-FVector(0,0,10),ECC_Visibility,Ground)
            && Floor.ImpactNormal.Z>.8f)
            FloorOffset=GetActorTransform().InverseTransformPosition(Floor.ImpactPoint).Z;
    }
    const FBox Bounds=CheetoMesh?CheetoMesh->GetBoundingBox():FBox(FVector(-2.5f,-.8f,0),FVector(2.5f,.8f,1.6f));
    for (int32 i=0;i<Count;++i)
    {
        const float Angle=Random.FRandRange(0,2*PI), Radius=FMath::Sqrt(Random.FRand())*ScatterRadius;
        const float FloorGap=Random.FRandRange(.02f,.1f);
        const FRotator Rotation(0,Random.FRandRange(0,360),Random.FRandRange(-8,8));
        const float Size=Random.FRandRange(.85f,1.18f);
        float Lowest=BIG_NUMBER;
        for (int32 Corner=0;Corner<8;++Corner)
        {
            const FVector V((Corner&1)?Bounds.Max.X:Bounds.Min.X,(Corner&2)?Bounds.Max.Y:Bounds.Min.Y,(Corner&4)?Bounds.Max.Z:Bounds.Min.Z);
            Lowest=FMath::Min(Lowest,Rotation.RotateVector(V*Size).Z);
        }
        const FVector Position(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,FloorOffset-Lowest+FloorGap);
        if (i<Remaining) Cheetos->AddInstance(FTransform(Rotation,Position,FVector(Size)));
    }
}
void AHousePigSnackPile::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); RefreshFood(); }
void AHousePigSnackPile::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority()) FoodRemaining=FMath::Clamp(FoodCount,1,24);
    RefreshFood();
}
void AHousePigSnackPile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority()) return;
    bool Occupied=false;
    for (TActorIterator<AHouseRoamingPig> It(GetWorld());It;++It)
        if (It->Health>0 && !It->IsCarried() && It->IsInsidePigsty() && It->SnackPile==this)
        { Occupied=true; break; }
    // A bounded instance buffer is reused. Empty rooms do not create food or
    // accumulate missed refills that would burst when a pig returns.
    if (!Occupied) { RefillElapsed=0.f; return; }
    RefillElapsed+=DeltaSeconds;
    if (RefillElapsed<FMath::Max(.1f,RefillInterval)) return;
    RefillElapsed=0.f;
    const int32 Added=FMath::Min(FMath::Clamp(RefillAmount,1,24),FMath::Clamp(FoodCount,1,24)-FoodRemaining);
    if (Added<=0) return;
    FoodRemaining+=Added; FoodRespawns+=Added; RefreshFood(); ForceNetUpdate();
}
void AHousePigSnackPile::OnRep_Food() { RefreshFood(); }
bool AHousePigSnackPile::Consume(AHouseRoamingPig* Pig)
{
    if (!HasAuthority() || !IsValid(Pig) || Pig->IsCarried() || Pig->Health<=0 || !Pig->IsInsidePigsty()
        || FoodRemaining<=0 || FVector::Dist2D(Pig->FeetLocation(),GetActorLocation())>145.f) return false;
    --FoodRemaining; RefreshFood(); Pig->HealFromSnack(); ForceNetUpdate(); return true;
}
FVector AHousePigSnackPile::FeedingFeet(int32 Seed) const
{
    return GetActorTransform().TransformPosition((Seed%2)?FeedingOffsetA:FeedingOffsetB);
}
void AHousePigSnackPile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AHousePigSnackPile,FoodRemaining);
}

AHouseRoamingPig::AHouseRoamingPig()
{
    bReplicates=true; SetReplicateMovement(true); SetNetUpdateFrequency(15.f);
    PrimaryActorTick.bCanEverTick=true;
    AutoPossessAI=EAutoPossessAI::Disabled;
    bUseControllerRotationYaw=false;
    GetCapsuleComponent()->InitCapsuleSize(30.f,48.f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
    UCharacterMovementComponent* Movement=GetCharacterMovement();
    Movement->bRunPhysicsWithNoController=true;
    Movement->bOrientRotationToMovement=false;
    Movement->MaxWalkSpeed=WalkSpeed; Movement->MaxStepHeight=42.f;
    Movement->MaxAcceleration=850.f; Movement->BrakingDecelerationWalking=1000.f;
    Movement->SetWalkableFloorAngle(48.f);
    VisualRoot=CreateDefaultSubobject<USceneComponent>(TEXT("PigVisualRoot"));
    VisualRoot->SetupAttachment(GetCapsuleComponent()); VisualRoot->SetRelativeLocation(FVector(0,0,-47));
    auto MakePart=[&](const FName Name,USceneComponent* Parent)
    {
        UStaticMeshComponent* Part=CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(Parent); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false); Part->SetCanEverAffectNavigation(false);
        return Part;
    };
    Body=MakePart(TEXT("PigBody"),VisualRoot);
    Head=MakePart(TEXT("PigHead"),VisualRoot); Head->SetRelativeLocation(FVector(60,0,76));
    for (int32 i=0;i<4;++i)
    {
        UStaticMeshComponent* Upper=MakePart(*FString::Printf(TEXT("UpperLeg%d"),i),VisualRoot);
        Upper->SetRelativeLocation(FVector(i<2?35:-35,i%2?21:-21,48));
        UpperLegs.Add(Upper);
        UStaticMeshComponent* Lower=MakePart(*FString::Printf(TEXT("LowerLeg%d"),i),Upper);
        Lower->SetRelativeLocation(FVector(0,0,-25)); LowerLegs.Add(Lower);
    }
    WeaponHitBox=CreateDefaultSubobject<UBoxComponent>(TEXT("PigWeaponHitBox"));
    WeaponHitBox->SetupAttachment(VisualRoot); WeaponHitBox->SetRelativeLocation(FVector(8.5f,0,46));
    WeaponHitBox->SetBoxExtent(FVector(78,27,44));
    WeaponHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WeaponHitBox->SetCollisionObjectType(ECC_WorldDynamic);
    WeaponHitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    WeaponHitBox->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    WeaponHitBox->SetGenerateOverlapEvents(false);
    SetCanBeDamaged(true);
}
void AHouseRoamingPig::RefreshMeshes()
{
    VisualScale=FMath::Clamp(VisualScale,.4f,1.f);
    GetCapsuleComponent()->SetCapsuleSize(30.f*VisualScale,48.f*VisualScale);
    VisualRoot->SetRelativeScale3D(FVector(VisualScale));
    VisualRoot->SetRelativeLocation(FVector(0,0,-48.f*VisualScale));
    Body->SetStaticMesh(BodyMesh); Head->SetStaticMesh(HeadMesh);
    for (int32 i=0;i<4;++i) { UpperLegs[i]->SetStaticMesh(UpperLegMesh); LowerLegs[i]->SetStaticMesh(LowerLegMesh); }
}
void AHouseRoamingPig::OnRep_Appearance() { RefreshMeshes(); }
void AHouseRoamingPig::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); RefreshMeshes(); }

void AHouseRoamingPig::BuildDefaultGraph()
{
    if (!RoamGraph.IsEmpty()) return;
    const TArray<FVector> Base={FVector(-2680,-1185,302.3),FVector(-2765,-1110,302.3),FVector(-2585,-1110,302.3),
        FVector(-2825,-920,323.3),FVector(-2600,-935,302.3),FVector(-2860,-1200,302.3),FVector(-2460,-1245,302.3),
        FVector(-2334.8,-1245,302.3),FVector(-2177,-1245,302.3),
        FVector(-2177,-850,302.3),FVector(-2177,-350,302.3),FVector(-2177,300,302.3),FVector(-2177,735,302.3),
        FVector(-2214.8,854.8,310)};
    for (int32 i=0;i<Base.Num();++i)
    {
        FHousePigRoamNode Node; Node.Feet=Base[i]; Node.bInsidePigsty=i<7;
        if (i==7) Node.GateDoorId=PigstyDoorId;
        if (i==Base.Num()-1) Node.GateDoorId=TEXT("SM_OriginalLeaf_Upper_p0_m2_Solid_04");
        RoamGraph.Add(Node);
    }
    auto Connect=[&](int32 A,int32 B) { RoamGraph[A].Links.AddUnique(B); RoamGraph[B].Links.AddUnique(A); };
    for (int32 i=1;i<7;++i) Connect(0,i);
    Connect(1,3); Connect(2,4);
    for (int32 i=6;i<Base.Num()-1;++i) Connect(i,i+1);
    // The registered stair route already used by real house walkthrough tests.
    // Each step still requires a safe floor, body sweep and open gate at runtime.
    FString Text; TSharedPtr<FJsonObject> Root; const TArray<TSharedPtr<FJsonValue>>* Routes=nullptr;
    if (!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("HouseReview/house_walkthrough.json")))
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root)
        || !Root->TryGetArrayField(TEXT("routes"),Routes)) return;
    for (const TSharedPtr<FJsonValue>& Value:*Routes)
    {
        const TSharedPtr<FJsonObject> Route=Value->AsObject(); FString Id;
        const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
        if (!Route.IsValid() || !Route->TryGetStringField(TEXT("id"),Id) || Id!=TEXT("UpperCapturedLanding_to_Entrance")
            || !Route->TryGetArrayField(TEXT("waypoints"),Points)) continue;
        int32 Previous=Base.Num()-1;
        for (const TSharedPtr<FJsonValue>& Point:*Points)
        {
            const TSharedPtr<FJsonObject> P=Point->AsObject(); double X,Y,Z;
            if (!P.IsValid() || !P->TryGetNumberField(TEXT("x"),X) || !P->TryGetNumberField(TEXT("y"),Y) || !P->TryGetNumberField(TEXT("z"),Z)) continue;
            FHousePigRoamNode Node; Node.Feet=FVector(100*Z,100*X,100*Y); Node.bInsidePigsty=false;
            const int32 Index=RoamGraph.Add(Node); Connect(Previous,Index); Previous=Index;
        }
        for (const FVector& Feet:{FVector(-2212.2,-258.3,8),FVector(-2196,-679.5,8),FVector(-1899,-679.5,8),
            FVector(-1602,-715.5,8),FVector(-1449,-715.5,8),FVector(-1323,-715.5,8),FVector(-1251,-715.5,8),FVector(-1071,-715.5,8)})
        {
            FHousePigRoamNode Node; Node.Feet=Feet; Node.bInsidePigsty=false;
            if (FMath::Abs(Feet.X+1323.f)<.1f) Node.GateDoorId=TEXT("SM_OriginalLeaf_Entrance_m1_m1_Solid_01");
            const int32 Index=RoamGraph.Add(Node); Connect(Previous,Index); Previous=Index;
        }
        break;
    }
}
void AHouseRoamingPig::BeginPlay()
{
    Super::BeginPlay(); RefreshMeshes(); BuildDefaultGraph(); Personality.Initialize(PersonalitySeed);
    if (HasAuthority()) Health=FMath::Max(1.f,MaxHealth);
    LastFeet=LastProgressFeet=FeetLocation(); bWasInside=IsInsidePigsty();
    VisualTime=Personality.FRandRange(0,6.f);
    ActivityUntil=GetWorld()->GetTimeSeconds()+Personality.FRandRange(.3f,1.8f);
}
FVector AHouseRoamingPig::FeetLocation() const { return GetActorLocation()-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()); }
bool AHouseRoamingPig::IsInsidePigsty() const
{
    const FVector Feet=FeetLocation(); return Feet.X>=PigstyMinimum.X && Feet.X<=PigstyMaximum.X
        && Feet.Y>=PigstyMinimum.Y && Feet.Y<=PigstyMaximum.Y && Feet.Z>=PigstyMinimum.Z && Feet.Z<=PigstyMaximum.Z;
}
bool AHouseRoamingPig::DoorAllows(FName Id) const
{
    if (Id.IsNone()) return true;
    for (TActorIterator<AExistingHouseDoor> It(GetWorld());It;++It)
        if (It->DoorId==Id) return !It->bLocked && It->IsOpen() && !It->IsActorTickEnabled();
    return false;
}
bool AHouseRoamingPig::EdgeAllows(int32 From,int32 To) const
{
    if (!RoamGraph.IsValidIndex(From)||!RoamGraph.IsValidIndex(To)) return false;
    if (RoamGraph[From].bInsidePigsty!=RoamGraph[To].bInsidePigsty && !DoorAllows(PigstyDoorId)) return false;
    return DoorAllows(RoamGraph[From].GateDoorId)&&DoorAllows(RoamGraph[To].GateDoorId);
}
int32 AHouseRoamingPig::NearestNode() const
{
    int32 Best=INDEX_NONE; float Distance=BIG_NUMBER; const bool Inside=IsInsidePigsty();
    for (int32 i=0;i<RoamGraph.Num();++i)
    {
        if (Inside!=RoamGraph[i].bInsidePigsty) continue;
        const float D=FVector::DistSquared(FeetLocation(),RoamGraph[i].Feet);
        if (D<Distance) { Distance=D; Best=i; }
    }
    return Best;
}
bool AHouseRoamingPig::RequestRoamNode(int32 Node)
{
    if (!HasAuthority() || Health<=0 || IsCarried() || !RoamGraph.IsValidIndex(Node)) return false;
    WakeUp();
    const int32 Start=NearestNode(); if (Start==INDEX_NONE) return false;
    TArray<int32> Previous; Previous.Init(INDEX_NONE,RoamGraph.Num());
    TArray<int32> Queue={Start}; Previous[Start]=Start;
    for (int32 q=0;q<Queue.Num()&&Previous[Node]==INDEX_NONE;++q)
        for (int32 Next:RoamGraph[Queue[q]].Links)
            if (RoamGraph.IsValidIndex(Next)&&Previous[Next]==INDEX_NONE&&EdgeAllows(Queue[q],Next))
            { Previous[Next]=Queue[q]; Queue.Add(Next); }
    if (Previous[Node]==INDEX_NONE) return false;
    TArray<int32> Back={Node}; for (int32 i=Node;i!=Start;) { i=Previous[i]; Back.Add(i); }
    Path.Reset(); for (int32 i=Back.Num()-1;i>=0;--i) Path.Add(Back[i]);
    PathCursor=0; bLocalGoal=false; bBedIntent=bFeedIntent=false; StuckSeconds=NoProgressSeconds=0; BestGoalDistance=BIG_NUMBER;
    Mood=GetWorld()->GetTimeSeconds()<FleeUntil?EHousePigMood::Fleeing:
        Personality.FRand()<.55f?EHousePigMood::Running:EHousePigMood::Walking;
    return true;
}
bool AHouseRoamingPig::RequestFeeding()
{
    if (!HasAuthority()||Health<=0||IsCarried()||!IsInsidePigsty()||!IsValid(SnackPile)||SnackPile->FoodRemaining<=0) return false;
    WakeUp();
    Path.Reset(); PathCursor=0; LocalGoal=SnackPile->FeedingFeet(PersonalitySeed);
    bLocalGoal=true; bFeedIntent=true; bBedIntent=false; Mood=EHousePigMood::Walking;
    StuckSeconds=NoProgressSeconds=0; BestGoalDistance=BIG_NUMBER;
    return true;
}
bool AHouseRoamingPig::RequestSleep()
{
    if (!HasAuthority() || Health<=0 || IsCarried() || !IsInsidePigsty()) return false;
    // Approach the low east edge before stepping onto the mattress. Separate
    // authored bed locations reserve one sleeping place for each individual.
    Path.Reset(); PathCursor=0; bFeedIntent=false; bBedIntent=true;
    bBedApproach=FeetLocation().X>BedLocation.X+85.f;
    LocalGoal=bBedApproach?FVector(BedLocation.X+140.f,BedLocation.Y,BedLocation.Z-21.f):BedLocation;
    bLocalGoal=true; Mood=EHousePigMood::Walking;
    ActivityUntil=0; FleeUntil=0;
    StuckSeconds=NoProgressSeconds=0; BestGoalDistance=BIG_NUMBER;
    return true;
}
bool AHouseRoamingPig::IsSleepingOnBed() const
{
    return Mood==EHousePigMood::Sleeping && !IsCarried() && IsInsidePigsty()
        && FVector::Dist2D(FeetLocation(),BedLocation)<30.f && FMath::Abs(FeetLocation().Z-BedLocation.Z)<9.f;
}
void AHouseRoamingPig::WakeUp()
{
    if (Mood==EHousePigMood::Sleeping) ++WakeEvents;
    if (Mood==EHousePigMood::Sleeping || Mood==EHousePigMood::Resting)
    {
        Mood=EHousePigMood::Sniffing;
        ActivityUntil=GetWorld()->GetTimeSeconds()+1.f;
    }
}
FVector AHouseRoamingPig::GetInteractionLocation() const
{
    return FeetLocation()+FVector(0,0,(Mood==EHousePigMood::Sleeping?30.f:55.f)*VisualScale);
}
bool AHouseRoamingPig::CanPickup(APawn* Player) const
{
    if (!IsValid(Player) || Player==this || Health<=0 || IsCarried() || !Player->GetController() || !GetWorld()) return false;
    FVector View; FRotator Facing; Player->GetController()->GetPlayerViewPoint(View,Facing);
    const FVector To=GetInteractionLocation()-View;
    if (To.ContainsNaN() || To.SizeSquared()>FMath::Square(220.f) || FVector::DotProduct(To.GetSafeNormal(),Facing.Vector())<.72f) return false;
    const AStoryFirstPersonCharacter* StoryPlayer=Cast<AStoryFirstPersonCharacter>(Player);
    if (StoryPlayer&&!StoryPlayer->CanReachPigGrip(GetCarryGripPoint(),GetCarryGripDirection())) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PigPickupView),false,this); Query.AddIgnoredActor(Player);
    FHitResult Hit;
    FVector LiftTarget;
    // The original house's moving door panels deliberately block Pawn rather
    // than Visibility. A visible hitbox alone must never permit a grab through
    // a closed physical leaf, glass panel, or other player-blocking barrier.
    return !GetWorld()->LineTraceSingleByChannel(Hit,View,GetInteractionLocation(),ECC_Visibility,Query)
        && !GetWorld()->LineTraceSingleByChannel(Hit,View,GetInteractionLocation(),ECC_Pawn,Query)
        && CarryLiftClear(Player,LiftTarget);
}
bool AHouseRoamingPig::CarryLiftClear(APawn* Player,FVector& LiftTarget) const
{
    LiftTarget=GetActorLocation()+FVector(0,0,90.f*VisualScale);
    const FRotator Rotation(0,GetActorRotation().Yaw,0);
    const FVector Offset=Rotation.RotateVector(FVector(8.5f*VisualScale,0,0));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PigPickupLift),false,this); Query.AddIgnoredActor(Player);
    FHitResult Hit;
    // Lift in the existing orientation before turning sideways. Adjacent
    // sleepers remain solid; the pig never rotates its long body into a peer.
    return !GetWorld()->SweepSingleByChannel(Hit,GetActorLocation()+Offset,LiftTarget+Offset,
        Rotation.Quaternion(),ECC_Pawn,FCollisionShape::MakeBox(FVector(78,29,45)*VisualScale),Query);
}
bool AHouseRoamingPig::TryPickup(APawn* Player)
{
    if (!HasAuthority() || !CanPickup(Player)) return false;
    for (TActorIterator<AHouseRoamingPig> It(GetWorld());It;++It) if (It->Carrier==Player) return false;
    if (!CarryLiftClear(Player,CarryLiftTarget)) return false;
    bCarryLifting=true;
    WakeUp();
    Path.Reset(); PathCursor=0; bLocalGoal=bBedIntent=bFeedIntent=bBedApproach=false;
    GetCharacterMovement()->StopMovementImmediately();
    Carrier=Player; OnRep_Carrier(); Mood=EHousePigMood::Carried;
    CarryPhase=EHousePigCarryPhase::Reaching; CarryPhaseStarted=GetWorld()->GetTimeSeconds(); CarryBlend=0.f;
    CarryStartPosition=GetActorLocation(); CarryStartRotation=GetActorQuat();
    CarryLastPlayerVelocity=Player->GetVelocity(); CarrySway=CarrySwayVelocity=FVector::ZeroVector;
    ++Pickups; ForceNetUpdate(); return true;
}
void AHouseRoamingPig::OnRep_Carrier()
{
    if (APawn* Previous=CarryTickOwner.Get())
    {
        RemoveTickPrerequisiteActor(Previous);
        if (const ACharacter* Character=Cast<ACharacter>(Previous))
            RemoveTickPrerequisiteComponent(Character->GetCharacterMovement());
    }
    CarryTickOwner=Carrier;
    if (IsCarried())
    {
        AddTickPrerequisiteActor(Carrier);
        if (const ACharacter* Character=Cast<ACharacter>(Carrier))
            AddTickPrerequisiteComponent(Character->GetCharacterMovement());
        GetCharacterMovement()->StopMovementImmediately(); GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        GetCapsuleComponent()->IgnoreActorWhenMoving(Carrier,true);
    }
    else
    {
        GetCapsuleComponent()->ClearMoveIgnoreActors();
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        if (Health>0) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
}
void AHouseRoamingPig::ReleaseCarrier()
{
    if (!HasAuthority()) return;
    if (IsValid(Carrier))
    { GetCapsuleComponent()->IgnoreActorWhenMoving(Carrier,false); RemoveTickPrerequisiteActor(Carrier); }
    Carrier=nullptr; bCarryLifting=false; CarryPhase=EHousePigCarryPhase::None; CarryBlend=0.f;
    // Movement capsules return upright only after a supported lowering.
    SetActorRotation(FRotator(0,GetActorRotation().Yaw,0)); OnRep_Carrier();
    if (Health>0) { Mood=EHousePigMood::Sniffing; ActivityUntil=GetWorld()->GetTimeSeconds()+1.5f; }
    ForceNetUpdate();
}
float AHouseRoamingPig::GetPickupBend() const
{
    if (!GetWorld()) return 0.f;
    const float Age=FMath::Max(0.f,GetWorld()->GetTimeSeconds()-CarryPhaseStarted);
    if (CarryPhase==EHousePigCarryPhase::Reaching) return FMath::SmoothStep(0.f,.30f,Age);
    if (CarryPhase==EHousePigCarryPhase::Lifting) return 1.f-FMath::SmoothStep(0.f,.85f,Age);
    if (CarryPhase==EHousePigCarryPhase::Lowering) return FMath::Sin(FMath::Clamp(Age/.85f,0.f,1.f)*PI)*.45f;
    return 0.f;
}
FVector AHouseRoamingPig::GetCarryGripPoint() const
{
    return LowerLegs.IsValidIndex(2)?LowerLegs[2]->GetComponentTransform().TransformPosition(FVector(0,0,-20)):
        GetActorLocation();
}
FVector AHouseRoamingPig::GetCarryGripDirection() const
{
    return LowerLegs.IsValidIndex(2)?LowerLegs[2]->GetUpVector():GetActorUpVector();
}
bool AHouseRoamingPig::MoveCarrySafely(const FVector& Target,const FQuat& Rotation,APawn* Player)
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PigCarryBody),false,this); Query.AddIgnoredActor(Player);
    const FCollisionShape Shape=FCollisionShape::MakeBox(FVector(78,29,45)*VisualScale);
    const FVector LocalCenter=FVector(8.5f,0,-2.f)*VisualScale;
    const FVector Start=GetActorLocation(); const FQuat StartRotation=GetActorQuat();
    const int32 Steps=FMath::Clamp(FMath::CeilToInt(FMath::RadiansToDegrees(StartRotation.AngularDistance(Rotation))/4.f),1,24);
    bool Complete=true;
    for (int32 Step=1;Step<=Steps;++Step)
    {
        const float Alpha=float(Step)/Steps;
        const FVector Position=FMath::Lerp(Start,Target,Alpha);
        const FQuat Facing=FQuat::Slerp(StartRotation,Rotation,Alpha).GetNormalized();
        const FVector Center=Position+Facing.RotateVector(LocalCenter);
        FHitResult Hit;
        const FVector CurrentCenter=GetActorLocation()+GetActorQuat().RotateVector(LocalCenter);
        if (GetWorld()->OverlapBlockingTestByChannel(Center,Facing,ECC_Pawn,Shape,Query)
            || GetWorld()->SweepSingleByChannel(Hit,CurrentCenter,Center,Facing,ECC_Pawn,Shape,Query))
        {
            if (GetWorld()->GetTimeSeconds()-CarryBlockedLogAt>1.f)
            {
                CarryBlockedLogAt=GetWorld()->GetTimeSeconds();
                TArray<FOverlapResult> Overlaps;
                GetWorld()->OverlapMultiByChannel(Overlaps,Center,Facing,ECC_Pawn,Shape,Query);
                FString Blockers=GetNameSafe(Hit.GetActor());
                for (const FOverlapResult& Overlap:Overlaps) if (Overlap.bBlockingHit)
                    Blockers+=TEXT(" ")+GetNameSafe(Overlap.GetActor());
                UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_CARRY_BLOCK name=%s phase=%d root=%s desired=%s facing=%s blockers=%s"),
                    *PigName,int32(CarryPhase),*GetActorLocation().ToString(),*Position.ToString(),*Facing.Rotator().ToString(),*Blockers);
            }
            Complete=false; break;
        }
        SetActorLocationAndRotation(Position,Facing,false);
    }
    return Complete;
}
void AHouseRoamingPig::UpdateCarry(float DeltaSeconds)
{
    if (!IsValid(Carrier) || !Carrier->GetController()) { ReleaseCarrier(); return; }
    const float Now=GetWorld()->GetTimeSeconds(), Age=FMath::Max(0.f,Now-CarryPhaseStarted);
    if (CarryPhase==EHousePigCarryPhase::Reaching)
    {
        CarryBlend=.15f*FMath::SmoothStep(0.f,.30f,Age);
        const AStoryFirstPersonCharacter* StoryPlayer=Cast<AStoryFirstPersonCharacter>(Carrier);
        // A local pickup must reach the physical ankle before the body leaves
        // the floor. Remote server pawns do not evaluate first-person bones.
        const bool HandAtAnkle=!StoryPlayer||!Carrier->IsLocallyControlled()
            ||FVector::DistSquared(StoryPlayer->GetPigHandGripPoint(),GetCarryGripPoint())<=FMath::Square(7.f);
        if (Age>=.60f&&HandAtAnkle)
        {
            CarryPhase=EHousePigCarryPhase::Lifting; CarryPhaseStarted=Now;
            CarryStartPosition=GetActorLocation(); CarryStartRotation=GetActorQuat();
            CarryStartGrip=GetCarryGripPoint(); ForceNetUpdate();
        }
        else if (Age>=1.2f) ReleaseCarrier();
        return;
    }
    if (CarryPhase==EHousePigCarryPhase::Lowering)
    {
        const float Alpha=FMath::SmoothStep(0.f,.85f,Age);
        CarryBlend=1.f-Alpha;
        const FVector Target=FMath::Lerp(CarryStartPosition,CarryPlacementTarget,Alpha)
            +FVector(0,0,FMath::Sin(Alpha*PI)*24.f*VisualScale);
        const FQuat Rotation=FQuat::Slerp(CarryStartRotation,CarryPlacementRotation,Alpha).GetNormalized();
        if (!MoveCarrySafely(Target,Rotation,Carrier))
        {
            CarryPhase=EHousePigCarryPhase::Held; CarryBlend=1.f; CarryPhaseStarted=Now; ForceNetUpdate(); return;
        }
        if (Age>=.85f)
        {
            const FVector Feet=CarryPlacementTarget-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3.f);
            if (PlacementClear(Feet,CarryPlacementRotation.Rotator(),Carrier))
            {
                ReleaseCarrier(); ++Placements; LastFeet=FeetLocation(); bWasInside=IsInsidePigsty();
            }
            else { CarryPhase=EHousePigCarryPhase::Held; CarryBlend=1.f; CarryPhaseStarted=Now; ForceNetUpdate(); }
        }
        return;
    }
    FVector View; FRotator ViewRotation; Carrier->GetController()->GetPlayerViewPoint(View,ViewRotation);
    if (const AStoryFirstPersonCharacter* StoryPlayer=Cast<AStoryFirstPersonCharacter>(Carrier))
    {
        // Read this frame's camera attachment and aim, not the previous
        // player-camera-manager cache, after the carrier's movement tick.
        if (StoryPlayer->FirstPersonCamera) View=StoryPlayer->FirstPersonCamera->GetComponentLocation();
        ViewRotation=StoryPlayer->GetBaseAimRotation();
    }
    const FRotator YawFrame(0,ViewRotation.Yaw,0);
    const FVector Velocity=Carrier->GetVelocity();
    const FVector Acceleration=(Velocity-CarryLastPlayerVelocity)/FMath::Max(.008f,DeltaSeconds);
    CarryLastPlayerVelocity=Velocity;
    const FVector LocalAcceleration=YawFrame.UnrotateVector(Acceleration);
    const FVector SwayTarget(FMath::Clamp(-LocalAcceleration.X*.003f,-5.f,5.f),
        FMath::Clamp(LocalAcceleration.Y*.003f,-6.f,6.f),0);
    const float Dt=FMath::Min(DeltaSeconds,.0333f);
    CarrySwayVelocity+=(SwayTarget-CarrySway)*50.f*Dt-CarrySwayVelocity*14.f*Dt;
    CarrySway+=CarrySwayVelocity*Dt;
    CarrySway.X=FMath::Clamp(CarrySway.X,-5.f,5.f); CarrySway.Y=FMath::Clamp(CarrySway.Y,-6.f,6.f);
    const FQuat HangRotation=FRotator(-80.f+CarrySway.X,ViewRotation.Yaw+90.f,-8.f+CarrySway.Y).Quaternion();
    const float StepBob=FMath::Sin(Now*7.f)*FMath::Clamp(Velocity.Size2D()/400.f,0.f,1.f)*.65f;
    const FVector LocalGrip=GetActorTransform().InverseTransformPosition(GetCarryGripPoint());
    const auto FloorClearGrip=[&](FVector Grip,const FQuat& Facing)
    {
        const FVector Offset=Facing.RotateVector(FVector(8.5f,0,-2.f)*VisualScale-LocalGrip);
        const FVector Center=Grip+Offset;
        FVector Start=Center+FVector(0,0,100.f); Start.Z=FMath::Min(Start.Z,View.Z+20.f);
        FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(PigCarryFloor),false,this);
        for (TActorIterator<APawn> It(GetWorld());It;++It) FloorQuery.AddIgnoredActor(*It);
        FHitResult Floor;
        if (GetWorld()->LineTraceSingleByChannel(Floor,Start,Center-FVector(0,0,300.f),ECC_Visibility,FloorQuery)
            &&Floor.ImpactNormal.Z>.65f)
        {
            const FVector Size=FVector(78,29,45)*VisualScale;
            const float HalfZ=FMath::Abs(Facing.GetAxisX().Z)*Size.X+FMath::Abs(Facing.GetAxisY().Z)*Size.Y+FMath::Abs(Facing.GetAxisZ().Z)*Size.Z;
            Grip.Z=FMath::Max(Grip.Z,Floor.ImpactPoint.Z+HalfZ+5.f-Offset.Z);
        }
        return Grip;
    };
    // The grip stays within the camera-mounted arm's natural reach, including
    // when looking down. Raise it only enough for the hanging body to clear
    // the actual floor, rather than pushing the animal through the ground.
    // Lifting aims at the recovered eye height, so a temporary deep reach
    // cannot pull the ankle back down before the camera stands up again.
    const FVector CarryView=CarryPhase==EHousePigCarryPhase::Lifting?Carrier->GetPawnViewLocation():View;
    const FVector DesiredGrip=FloorClearGrip(CarryView+ViewRotation.RotateVector(FVector(42,20,-16))+FVector(0,0,StepBob),HangRotation);
    if (CarryPhase==EHousePigCarryPhase::Lifting)
    {
        const float LiftAlpha=FMath::SmoothStep(0.f,1.05f,Age);
        const float HangAlpha=FMath::SmoothStep(.28f,1.05f,Age);
        CarryBlend=FMath::Lerp(.15f,1.f,FMath::SmoothStep(0.f,1.05f,Age));
        const FQuat Facing=FQuat::Slerp(CarryStartRotation,HangRotation,HangAlpha).GetNormalized();
        // Lift through the gripped ankle. Recompute its actual authored mesh
        // offset as the sleeping/standing legs straighten into the hang.
        const FVector Grip=FloorClearGrip(FMath::Lerp(CarryStartGrip,DesiredGrip,LiftAlpha)
            +FVector(0,0,FMath::Sin(LiftAlpha*PI)*75.f*VisualScale),Facing);
        const FVector Target=Grip-Facing.RotateVector(LocalGrip);
        MoveCarrySafely(Target,Facing,Carrier);
        if (Age>=1.05f)
        { CarryPhase=EHousePigCarryPhase::Held; CarryPhaseStarted=Now; CarryBlend=1.f; bCarryLifting=false; ForceNetUpdate(); }
    }
    else
    {
        CarryBlend=1.f;
        const FQuat Rotation=FQuat::Slerp(GetActorQuat(),HangRotation,1.f-FMath::Exp(-10.f*DeltaSeconds)).GetNormalized();
        // Weight lags through body rotation and sway; the gripped ankle stays
        // at the hand as the player turns, rather than orbiting away from it.
        const FVector Target=DesiredGrip-Rotation.RotateVector(LocalGrip);
        MoveCarrySafely(Target,Rotation,Carrier);
    }
    if (FVector::Dist(GetActorLocation(),Carrier->GetActorLocation())>340.f)
    {
        if (!TryPlace(Carrier)) ReleaseCarrier();
    }
}
bool AHouseRoamingPig::PlacementClear(const FVector& Feet,const FRotator& Rotation,APawn* Player) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PigPlacement),false,this); Query.AddIgnoredActor(Player);
    const float Half=GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    auto Blocked=[&](const FVector& Position,const FQuat& Facing,const FCollisionShape& Shape,const TCHAR* Reason)
    {
        if (!GetWorld()->OverlapBlockingTestByChannel(Position,Facing,ECC_Pawn,Shape,Query)) return false;
        TArray<FOverlapResult> Hits;
        GetWorld()->OverlapMultiByChannel(Hits,Position,Facing,ECC_Pawn,Shape,Query);
        for (const FOverlapResult& Hit:Hits) if (Hit.bBlockingHit)
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_PLACE_REJECT name=%s reason=%s feet=%s actor=%s component=%s"),
                *PigName,Reason,*Feet.ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));
        return true;
    };
    if (Blocked(Feet+FVector(0,0,Half+3.f),FQuat::Identity,
        FCollisionShape::MakeCapsule(GetCapsuleComponent()->GetScaledCapsuleRadius(),Half),TEXT("capsule-overlap"))) return false;
    if (Blocked(Feet+Rotation.RotateVector(FVector(8.5f*VisualScale,0,0))+FVector(0,0,48.f*VisualScale+3.f),
        Rotation.Quaternion(),FCollisionShape::MakeBox(FVector(78,29,43)*VisualScale),TEXT("body-overlap"))) return false;
    FCollisionQueryParams Ground=Query;
    for (TActorIterator<APawn> It(GetWorld());It;++It) Ground.AddIgnoredActor(*It);
    for (float Along:{-52.f,0.f,52.f})
    {
        const FVector Point=Feet+Rotation.RotateVector(FVector(Along*VisualScale,0,0));
        FHitResult Floor;
        if (!GetWorld()->LineTraceSingleByChannel(Floor,Point+FVector(0,0,12),Point-FVector(0,0,24),ECC_Visibility,Ground)
            || Floor.ImpactNormal.Z<.65f || FMath::Abs(Floor.ImpactPoint.Z-Feet.Z)>16.f)
        {
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_PLACE_REJECT name=%s reason=footprint-support feet=%s along=%.1f hit=%d floor=%s normalZ=%.2f actor=%s"),
                *PigName,*Feet.ToString(),Along,Floor.bBlockingHit?1:0,*Floor.ImpactPoint.ToString(),Floor.ImpactNormal.Z,*GetNameSafe(Floor.GetActor()));
            return false;
        }
    }
    return true;
}
bool AHouseRoamingPig::TryPlace(APawn* Player)
{
    if (!HasAuthority() || !IsValid(Player) || !Player->GetController() || Carrier!=Player || !GetWorld()) return false;
    if (IsPlacing()) return true;
    if (CarryPhase!=EHousePigCarryPhase::Held) return false;
    FVector View; FRotator ViewRotation; Player->GetController()->GetPlayerViewPoint(View,ViewRotation);
    const FRotator Rotation(0,ViewRotation.Yaw+90.f,0);
    const FVector Forward=FRotator(0,ViewRotation.Yaw,0).Vector(), Right=FVector::CrossProduct(FVector::UpVector,Forward);
    const ACharacter* Character=Cast<ACharacter>(Player);
    const float PlayerFloor=Player->GetActorLocation().Z-(Character?Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight():90.f);
    TArray<FVector> Candidates={View+Forward*110.f,View+Forward*130.f+Right*65.f,View+Forward*130.f-Right*65.f};
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PigPlaceFloor),false,this); Query.AddIgnoredActor(Player);
    for (TActorIterator<APawn> It(GetWorld());It;++It) Query.AddIgnoredActor(*It);
    for (const FVector& Candidate:Candidates)
    {
        FHitResult Floor;
        if (!GetWorld()->LineTraceSingleByChannel(Floor,FVector(Candidate.X,Candidate.Y,PlayerFloor+100.f),
            FVector(Candidate.X,Candidate.Y,PlayerFloor-55.f),ECC_Visibility,Query) || Floor.ImpactNormal.Z<.65f)
        {
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_PLACE_REJECT name=%s reason=floor-trace candidate=%s playerFloor=%.2f hit=%d normalZ=%.2f actor=%s"),
                *PigName,*Candidate.ToString(),PlayerFloor,Floor.bBlockingHit?1:0,Floor.ImpactNormal.Z,*GetNameSafe(Floor.GetActor()));
            continue;
        }
        const FVector Feet=Floor.ImpactPoint;
        if (!PlacementClear(Feet,Rotation,Player)) continue;
        const FVector Target=Feet+FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3.f);
        FCollisionQueryParams BodyQuery(SCENE_QUERY_STAT(PigPlaceBody),false,this); BodyQuery.AddIgnoredActor(Player);
        // Validate the complete rotating, lowering body rather than a vertical
        // box at its old hanging position. Dynamic obstructions are rechecked
        // every frame before any animation movement is applied.
        bool Blocked=false;
        const FVector LocalCenter=FVector(8.5f,0,-2.f)*VisualScale;
        FVector Previous=GetActorLocation()+GetActorQuat().RotateVector(LocalCenter);
        const FCollisionShape Shape=FCollisionShape::MakeBox(FVector(78,29,45)*VisualScale);
        for (int32 Step=1;Step<=24;++Step)
        {
            const float Alpha=float(Step)/24.f;
            const FQuat Facing=FQuat::Slerp(GetActorQuat(),Rotation.Quaternion(),Alpha).GetNormalized();
            const FVector Center=FMath::Lerp(GetActorLocation(),Target,Alpha)
                +FVector(0,0,FMath::Sin(Alpha*PI)*24.f*VisualScale)+Facing.RotateVector(LocalCenter);
            FHitResult Obstacle;
            if (GetWorld()->OverlapBlockingTestByChannel(Center,Facing,ECC_Pawn,Shape,BodyQuery)
                || GetWorld()->SweepSingleByChannel(Obstacle,Previous,Center,Facing,ECC_Pawn,Shape,BodyQuery))
            { Blocked=true; break; }
            Previous=Center;
        }
        if (Blocked) continue;
        CarryStartPosition=GetActorLocation(); CarryStartRotation=GetActorQuat();
        CarryPlacementTarget=Target; CarryPlacementRotation=Rotation.Quaternion();
        CarryPhase=EHousePigCarryPhase::Lowering; CarryPhaseStarted=GetWorld()->GetTimeSeconds();
        CarryBlend=1.f; ForceNetUpdate();
        return true;
    }
    return false;
}
FString AHouseRoamingPig::MovementDebug() const
{
    const bool Active=bLocalGoal||Path.IsValidIndex(PathCursor);
    const FVector Goal=bLocalGoal?LocalGoal:Path.IsValidIndex(PathCursor)?RoamGraph[Path[PathCursor]].Feet:FeetLocation();
    return FString::Printf(TEXT("mood=%d active=%d target=%s cursor=%d/%d node=%d speed=%.1f noProgress=%.2f probeBlocked=%.2f"),
        static_cast<int32>(Mood),Active?1:0,*Goal.ToString(),PathCursor,Path.Num(),Path.IsValidIndex(PathCursor)?Path[PathCursor]:INDEX_NONE,
        GetVelocity().Size2D(),NoProgressSeconds,StuckSeconds);
}
bool AHouseRoamingPig::SafeDirection(const FVector& Direction,float ProbeDistance) const
{
    const FVector Feet=FeetLocation(), End=Feet+Direction*ProbeDistance;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PigWalkingSafety),false,this);
    // A peer blocks the body probe. Floor probes ignore peers and players so
    // they cannot be mistaken for a floor that allows a pig to step into air.
    FHitResult Obstacle;
    if (GetWorld()->SweepSingleByChannel(Obstacle,Feet+FVector(0,0,64*VisualScale),End+FVector(0,0,64*VisualScale),
        Direction.Rotation().Quaternion(),ECC_Pawn,FCollisionShape::MakeBox(FVector(65,27,20)*VisualScale),Query)) return false;
    FCollisionQueryParams Ground=Query;
    for (TActorIterator<APawn> It(GetWorld());It;++It) Ground.AddIgnoredActor(*It);
    FHitResult Floor;
    return GetWorld()->LineTraceSingleByChannel(Floor,End+FVector(0,0,45),End-FVector(0,0,65),ECC_Visibility,Ground)
        && Floor.ImpactNormal.Z>.55f && Floor.ImpactPoint.Z>=Feet.Z-48.f;
}
bool AHouseRoamingPig::CanWalkStraight(const FVector& Target) const
{
    const FVector To=(Target-FeetLocation()).GetSafeNormal2D(); return SafeDirection(To,FMath::Min(35.f,FVector::Dist2D(Target,FeetLocation())));
}
void AHouseRoamingPig::ChooseActivity()
{
    const float Now=GetWorld()->GetTimeSeconds(); const bool Inside=IsInsidePigsty();
    bLocalGoal=false; bBedIntent=bFeedIntent=false;
    // A finite snack source can tempt either individual, with different timing
    // and feeding sides rather than a synchronized animation for the pair.
    if (Inside && IsValid(SnackPile)&&SnackPile->FoodRemaining>0&&Personality.FRand()<.55f)
    {
        RequestFeeding(); return;
    }
    if (Inside && Personality.FRand()<.25f && Now>=FleeUntil && RequestSleep()) return;
    TArray<int32> Choices;
    const bool TryOutside=Inside?(DoorAllows(PigstyDoorId)&&Personality.FRand()<EscapeProbability):
        !(DoorAllows(PigstyDoorId)&&Personality.FRand()<.15f);
    for (int32 i=0;i<RoamGraph.Num();++i)
        if ((TryOutside?!RoamGraph[i].bInsidePigsty:RoamGraph[i].bInsidePigsty)&&i!=NearestNode()) Choices.Add(i);
    for (int32 Attempt=0;Attempt<Choices.Num();++Attempt)
        if (RequestRoamNode(Choices[Personality.RandRange(0,Choices.Num()-1)])) return;
    Mood=EHousePigMood::Sniffing; ActivityUntil=Now+Personality.FRandRange(1.f,3.f);
}
void AHouseRoamingPig::Think()
{
    const float Now=GetWorld()->GetTimeSeconds();
    if (Health<=0 || IsCarried()) return;
    if (Now>=FleeUntil)
        for (FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
            if (const APlayerController* PC=It->Get(); PC&&PC->GetPawn()
                && FVector::Dist2D(PC->GetPawn()->GetActorLocation(),FeetLocation())<(Mood==EHousePigMood::Sleeping?75.f:110.f)
                && FMath::Abs(PC->GetPawn()->GetActorLocation().Z-GetActorLocation().Z)<150.f)
            {
                FleeUntil=Now+2.8f;
                int32 Best=INDEX_NONE; float Score=-1;
                for (int32 i=0;i<RoamGraph.Num();++i)
                {
                    if (RoamGraph[i].bInsidePigsty!=IsInsidePigsty()) continue;
                    const float Away=FVector::DistSquared2D(RoamGraph[i].Feet,PC->GetPawn()->GetActorLocation());
                    if (Away>Score) { Score=Away; Best=i; }
                }
                if (Best!=INDEX_NONE) RequestRoamNode(Best);
                break;
            }
    if (Mood==EHousePigMood::Feeding)
    {
        if (IsValid(SnackPile)&&SnackPile->FoodRemaining>0&&Now-LastMealAt>=1.f)
        {
            LastMealAt=Now;
            if (SnackPile->Consume(this)) ++SnacksEaten;
        }
        if (!IsValid(SnackPile)||SnackPile->FoodRemaining<=0||Now>=ActivityUntil)
        { Mood=EHousePigMood::Sniffing; ActivityUntil=Now+.4f; }
        return;
    }
    if (Mood==EHousePigMood::Sleeping)
    {
        if (Now<ActivityUntil) return;
        WakeUp(); return;
    }
    if (PathCursor<Path.Num()||bLocalGoal) return;
    if (Now<ActivityUntil) return;
    ChooseActivity();
}
void AHouseRoamingPig::FollowPath(float DeltaSeconds)
{
    if (Health<=0 || IsCarried() || (PathCursor>=Path.Num()&&!bLocalGoal)) return;
    FVector Goal;
    if (bLocalGoal) Goal=LocalGoal;
    else
    {
        const int32 Node=Path[PathCursor];
        if (PathCursor>0&&!EdgeAllows(Path[PathCursor-1],Node))
        { Path.Reset(); PathCursor=0; Mood=EHousePigMood::Sniffing; ActivityUntil=GetWorld()->GetTimeSeconds()+1.f; return; }
        Goal=RoamGraph[Node].Feet;
    }
    const bool FinalBedApproach=bBedIntent&&!bBedApproach;
    if (FVector::Dist2D(FeetLocation(),Goal)<(FinalBedApproach?8.f:22.f)
        && FMath::Abs(FeetLocation().Z-Goal.Z)<(FinalBedApproach?9.f:65.f))
    {
        if (bLocalGoal)
        {
            if (bBedIntent && bBedApproach)
            {
                bBedApproach=false; LocalGoal=BedLocation; BestGoalDistance=BIG_NUMBER; NoProgressSeconds=0; return;
            }
            bLocalGoal=false;
            Mood=bFeedIntent?EHousePigMood::Feeding:bBedIntent?EHousePigMood::Sleeping:EHousePigMood::Sniffing;
            GetCharacterMovement()->StopMovementImmediately();
            ActivityUntil=GetWorld()->GetTimeSeconds()+(bBedIntent?FMath::Max(3.f,SleepDuration):Personality.FRandRange(bFeedIntent?3.f:2.f,bFeedIntent?7.f:5.f));
            if (bBedIntent) { SetActorRotation(FRotator(0,BedYaw,0)); ++SleepStarts; ForceNetUpdate(); }
            if (bFeedIntent && SnackPile) SetActorRotation((SnackPile->GetActorLocation()-FeetLocation()).Rotation());
        }
        else
        {
            if (!RoamGraph[Path[PathCursor]].bInsidePigsty) ++HallNodesVisited;
            ++PathCursor;
            if (PathCursor>=Path.Num()) { Path.Reset(); PathCursor=0; Mood=EHousePigMood::Sniffing; ActivityUntil=GetWorld()->GetTimeSeconds()+Personality.FRandRange(.5f,1.8f); }
        }
        return;
    }
    const float GoalDistance=FVector::Dist2D(FeetLocation(),Goal);
    if (!Goal.Equals(ProgressGoal,.5f)||BestGoalDistance==BIG_NUMBER)
    { ProgressGoal=Goal; BestGoalDistance=GoalDistance; LastProgressFeet=FeetLocation(); NoProgressSeconds=0; }
    else if (GoalDistance<BestGoalDistance-8.f)
    { BestGoalDistance=GoalDistance; LastProgressFeet=FeetLocation(); NoProgressSeconds=0; }
    else NoProgressSeconds+=DeltaSeconds;
    if (NoProgressSeconds>3.5f)
    {
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_PATH_NO_PROGRESS name=%s feet=%s %s"),*PigName,*FeetLocation().ToString(),*MovementDebug());
        Path.Reset(); PathCursor=0; bLocalGoal=false; bFeedIntent=bBedIntent=false;
        GetCharacterMovement()->StopMovementImmediately(); Mood=EHousePigMood::Sniffing;
        ActivityUntil=GetWorld()->GetTimeSeconds()+Personality.FRandRange(.5f,1.4f);
        StuckSeconds=NoProgressSeconds=0; BestGoalDistance=BIG_NUMBER; return;
    }
    FVector Direction=(Goal-FeetLocation()).GetSafeNormal2D();
    // Collision checks are ahead of the whole long body, not just its narrow
    // upright movement capsule. Gravity and CharacterMovement handle the steps.
    const float Probe=FMath::Min(25.f,FVector::Dist2D(Goal,FeetLocation()));
    if (!SafeDirection(Direction,Probe))
    {
        bool bFound=false;
        for (float Offset:{35.f,-35.f,65.f,-65.f})
        {
            const FVector Candidate=Direction.RotateAngleAxis(Offset,FVector::UpVector);
            if (SafeDirection(Candidate,Probe)) { Direction=Candidate; bFound=true; break; }
        }
        if (!bFound) { StuckSeconds+=DeltaSeconds; }
        if (!bFound && StuckSeconds<2.5f) return;
        if (!bFound) { ++UnsafeFloorStops; Path.Reset(); PathCursor=0; bLocalGoal=false; Mood=EHousePigMood::Sniffing; ActivityUntil=GetWorld()->GetTimeSeconds()+Personality.FRandRange(.5f,1.4f); StuckSeconds=0; return; }
    }
    if (FVector::Dist2D(FeetLocation(),LastProgressFeet)>15.f) { LastProgressFeet=FeetLocation(); StuckSeconds=0; }
    UCharacterMovementComponent* Movement=GetCharacterMovement();
    const bool Fast=Mood==EHousePigMood::Running||Mood==EHousePigMood::Fleeing;
    const float TargetSpeed=Fast?RunSpeed:WalkSpeed;
    const FRotator Face=FMath::RInterpTo(GetActorRotation(),Direction.Rotation(),DeltaSeconds,5.f);
    SetActorRotation(FRotator(0,Face.Yaw,0));
    // Weight takes a moment to accelerate, slows before a turn, and eases
    // into an activity instead of sliding sideways at full travel speed.
    const float Alignment=FMath::Clamp(FVector::DotProduct(GetActorForwardVector(),Direction),0.f,1.f);
    const bool LastGoal=bLocalGoal||PathCursor==Path.Num()-1;
    const float Approach=LastGoal?FMath::Clamp(GoalDistance/(FinalBedApproach?55.f:85.f),FinalBedApproach?.22f:.42f,1.f):1.f;
    Movement->MaxWalkSpeed=TargetSpeed*FMath::Lerp(.40f,1.f,Alignment)*Approach;
    Movement->MaxAcceleration=FMath::Clamp(TargetSpeed*(Fast?3.2f:3.0f),300.f,1100.f);
    Movement->BrakingDecelerationWalking=FMath::Clamp(TargetSpeed*4.f,450.f,1000.f);
    AddMovementInput(Direction,1.f,true);
}
void AHouseRoamingPig::Animate(float DeltaSeconds)
{
    if (DeltaSeconds<=0.f) return;
    // The player's hand reaches a fixed ankle before lifting. Preserve the
    // complete standing/sleeping pose until the grasp has closed around it.
    if (GetCarryPhase()==EHousePigCarryPhase::Reaching)
    { GaitLastFeet=FeetLocation(); GaitLastYaw=GetActorRotation().Yaw; return; }
    VisualTime+=DeltaSeconds;
    const UCharacterMovementComponent* Movement=GetCharacterMovement();
    const bool Held=IsCarried();
    const bool Sleep=!Held&&(Mood==EHousePigMood::Sleeping||Mood==EHousePigMood::Down);
    const bool Feed=Mood==EHousePigMood::Feeding;
    const bool Sniff=!Held&&(Mood==EHousePigMood::Sniffing||Feed);
    const FVector Feet=FeetLocation();
    const float ActualSpeed=Held?0.f:GetVelocity().Size2D();
    const float CarryAlpha=FMath::Clamp(GetCarryBlend(),0.f,1.f);
    const bool GroundPose=CarryAlpha<.015f&&!Held;
    const float FloorDistance=Movement->IsMovingOnGround()&&Movement->CurrentFloor.bBlockingHit?Movement->CurrentFloor.FloorDist:0.f;
    const float FloorZ=Feet.Z-FloorDistance;
    if (!GaitInitialized)
    {
        GaitInitialized=true; GaitLastFeet=Feet; GaitLastYaw=GetActorRotation().Yaw;
        GaitHeadLocation=Head->GetRelativeLocation(); GaitHeadRotation=Head->GetRelativeRotation();
        for (int32 i=0;i<4;++i)
        {
            GaitGroundZ[i]=FloorZ; GaitFootPhase[i]=-1.f;
            GaitUpperGround[i]=UpperLegs[i]->GetRelativeRotation();
            GaitLowerGround[i]=LowerLegs[i]->GetRelativeRotation();
        }
    }
    float Distance=FVector::Dist2D(Feet,GaitLastFeet); GaitLastFeet=Feet;
    if (!GroundPose||Sleep||Distance>100.f) Distance=0.f;
    GaitDistance+=Distance;
    GaitSpeed=FMath::FInterpTo(GaitSpeed,ActualSpeed,DeltaSeconds,ActualSpeed>GaitSpeed?8.f:5.f);
    GaitMotion=FMath::FInterpTo(GaitMotion,GroundPose&&!Sleep&&ActualSpeed>8.f?1.f:0.f,DeltaSeconds,6.f);
    const float TrotStart=WalkSpeed*1.10f, TrotFull=FMath::Max(TrotStart+40.f,RunSpeed*.80f);
    GaitTrot=FMath::FInterpTo(GaitTrot,FMath::Clamp((GaitSpeed-TrotStart)/(TrotFull-TrotStart),0.f,1.f),DeltaSeconds,4.f);
    // A pig walks with a lateral, four-beat sequence (RH, RF, LH, LF).
    // The longer stance supports its forward-heavy body. Only fast travel
    // brings diagonal pairs together into a trot; cadence follows distance.
    const float Duty=FMath::Lerp(.69f,.52f,GaitTrot);
    const float Reach=FMath::Lerp(25.f,31.f,GaitTrot);
    const float CycleDistance=(2.f*Reach/Duty)*VisualScale;
    GaitPhase=FMath::Fmod(GaitPhase+Distance*(2.f*PI)/FMath::Max(1.f,CycleDistance),2.f*PI);
    const float Cycle=GaitPhase/(2.f*PI);
    const float DesiredCrouch=Held||Sleep?0.f:Feed?26.f:Sniff&&GaitMotion<.2f?24.f:FMath::Lerp(11.f,16.f,GaitTrot);
    VisualCrouch=FMath::FInterpTo(VisualCrouch,DesiredCrouch,DeltaSeconds,5.f);
    SleepBlend=FMath::FInterpTo(SleepBlend,Sleep?1.f:0.f,DeltaSeconds,4.f);
    const float TurnRate=FMath::FindDeltaAngleDegrees(GaitLastYaw,GetActorRotation().Yaw)/DeltaSeconds;
    GaitLastYaw=GetActorRotation().Yaw;
    GaitTurn=FMath::FInterpTo(GaitTurn,FMath::Clamp(TurnRate,-160.f,160.f),DeltaSeconds,4.f);
    const float FrontGround=(GaitGroundZ[0]+GaitGroundZ[1])*.5f;
    const float RearGround=(GaitGroundZ[2]+GaitGroundZ[3])*.5f;
    const float DesiredTerrainPitch=GroundPose&&!Sleep?FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(FrontGround-RearGround,70.f*VisualScale)),-12.f,12.f):0.f;
    GaitTerrainPitch=FMath::FInterpTo(GaitTerrainPitch,DesiredTerrainPitch,DeltaSeconds,5.f);
    const float Bob=GaitMotion*FMath::Lerp(FMath::Sin(GaitPhase*4.f)*.35f,FMath::Sin(GaitPhase*2.f)*1.1f,GaitTrot);
    const float WeightRoll=GaitMotion*(FMath::Sin(GaitPhase*2.f)*FMath::Lerp(1.1f,.4f,GaitTrot)-GaitTurn*.011f);
    const float RestZ=FMath::Lerp(Bob-VisualCrouch,13.f,SleepBlend)*VisualScale;
    VisualRoot->SetRelativeLocation(FVector(0,-60.f*VisualScale*SleepBlend,-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-FloorDistance+RestZ));
    VisualRoot->SetRelativeRotation(FRotator((GaitTerrainPitch-GaitMotion*1.1f)*(1.f-SleepBlend)*(1.f-CarryAlpha),0,
        75.f*SleepBlend+WeightRoll*(1.f-SleepBlend)*(1.f-CarryAlpha)));
    const float Breath=FMath::Sin(VisualTime*(Sleep?1.25f:2.2f))*(Sleep?.012f:.003f);
    Body->SetRelativeScale3D(FVector(1.f,1.f+Breath*.65f,1.f+Breath));
    Body->SetRelativeRotation(FRotator::ZeroRotator);
    // Head motion has weight: a slow searching snout, small feeding pulses,
    // and counter-motion to steady the head during body sway and turns.
    const float SniffAlpha=Sniff?1.f:0.f;
    const float HeadPitch=FMath::Lerp(-GaitTerrainPitch*.5f+FMath::Sin(VisualTime*1.6f)*1.5f,-52.f+FMath::Sin(VisualTime*(Feed?5.5f:2.8f))*(Feed?2.f:4.f),SniffAlpha);
    const float HeadYaw=Sniff?FMath::Sin(VisualTime*1.15f)*7.f:FMath::Sin(VisualTime*.65f)*2.f+GaitTurn*.025f;
    FVector DesiredHeadLocation(60,0,76-20.f*SniffAlpha);
    FRotator DesiredHeadRotation(FMath::Lerp(HeadPitch,-12.f,SleepBlend),HeadYaw*(1.f-SleepBlend),-WeightRoll*.55f);
    DesiredHeadLocation=FMath::Lerp(DesiredHeadLocation,FVector(60,0,76),CarryAlpha);
    DesiredHeadRotation=FMath::Lerp(DesiredHeadRotation,FRotator(-8.f+FMath::Sin(VisualTime*1.4f)*1.5f,0,0),CarryAlpha);
    GaitHeadLocation=FMath::VInterpTo(GaitHeadLocation,DesiredHeadLocation,DeltaSeconds,5.f);
    GaitHeadRotation=FMath::RInterpTo(GaitHeadRotation,DesiredHeadRotation,DeltaSeconds,4.f);
    Head->SetRelativeLocation(GaitHeadLocation); Head->SetRelativeRotation(GaitHeadRotation);

    const FTransform RootTransform=VisualRoot->GetComponentTransform();
    // A strict budget: at most one floor ray this frame, 30 per second per
    // pig. Round-robin samples look ahead to the next touchdown, not through
    // other characters. Physics still decides the actual body position.
    GaitGroundAccumulator+=DeltaSeconds;
    if (GroundPose&&!Sleep&&GaitGroundAccumulator>=1.f/30.f)
    {
        GaitGroundAccumulator=0.f;
        const int32 Leg=GaitGroundCursor; GaitGroundCursor=(GaitGroundCursor+1)%4;
        const FVector Hip(Leg<2?35.f:-35.f,Leg%2?21.f:-21.f,48.f);
        const FVector Sample=RootTransform.TransformPosition(FVector(Hip.X+Reach*GaitMotion,Hip.Y,0));
        FCollisionQueryParams Query(SCENE_QUERY_STAT(PigHoofGround),false,this);
        for (TActorIterator<APawn> It(GetWorld());It;++It) Query.AddIgnoredActor(*It);
        FHitResult Ground;
        if (GetWorld()->LineTraceSingleByChannel(Ground,FVector(Sample.X,Sample.Y,FloorZ+60.f*VisualScale),
            FVector(Sample.X,Sample.Y,FloorZ-55.f*VisualScale),ECC_Visibility,Query)&&Ground.ImpactNormal.Z>.55f)
            GaitGroundZ[Leg]=Ground.ImpactPoint.Z;
    }
    // Exact two-link IK in the desired leg plane, so turns and uneven floors
    // also retain support. Mesh pivots are the authored 25/23 cm links.
    auto SolveLeg=[](const FVector& Goal,int32 Leg,FQuat& Upper,FQuat& Lower)
    {
        const float Length=FMath::Clamp(static_cast<float>(Goal.Size()),2.1f,47.8f);
        const FVector Direction=Goal.GetSafeNormal(UE_SMALL_NUMBER,FVector(0,0,-1));
        FVector Bend=FVector(Leg<2?1.f:-1.f,0,0);
        Bend=(Bend-Direction*FVector::DotProduct(Bend,Direction)).GetSafeNormal(UE_SMALL_NUMBER,FVector::RightVector);
        const float Along=(25.f*25.f+Length*Length-23.f*23.f)/(2.f*Length);
        const float Out=FMath::Sqrt(FMath::Max(0.f,25.f*25.f-Along*Along));
        const FVector Knee=Direction*Along+Bend*Out;
        const FVector End=Direction*Length;
        Upper=FQuat::FindBetweenNormals(FVector(0,0,-1),Knee.GetSafeNormal());
        const FQuat LowerWorld=FQuat::FindBetweenNormals(FVector(0,0,-1),(End-Knee).GetSafeNormal());
        Lower=Upper.Inverse()*LowerWorld;
    };
    const float WalkOffsets[4]={.25f,.75f,.50f,0.f};
    const float TrotOffsets[4]={0.f,.5f,.5f,0.f};
    for (int32 i=0;i<4;++i)
    {
        const FVector Hip(i<2?35.f:-35.f,i%2?21.f:-21.f,48.f);
        FQuat UpperGoal=GaitUpperGround[i].Quaternion(),LowerGoal=GaitLowerGround[i].Quaternion();
        if (GroundPose&&SleepBlend<.98f)
        {
            const float Offset=FMath::Lerp(WalkOffsets[i],TrotOffsets[i],GaitTrot);
            const float Phase=FMath::Fmod(Cycle+Offset,1.f);
            const bool Settling=ActualSpeed<8.f;
            const bool Stance=Settling||Phase<Duty;
            const bool NewFoot=GaitFootPhase[i]<0.f;
            const FVector Neutral=RootTransform.TransformPosition(FVector(Hip.X,Hip.Y,0));
            if (!GaitRecoveryActive[i])
            {
            if (NewFoot)
            {
                GaitFootWorld[i]=FVector(Neutral.X,Neutral.Y,GaitGroundZ[i]);
                GaitSwingStart[i]=GaitSwingEnd[i]=GaitFootWorld[i];
                GaitPlanted[i]=true;
            }
            if (!Stance&&(GaitPlanted[i]||NewFoot))
            {
                GaitSwingStart[i]=GaitFootWorld[i];
                GaitSwingPhaseAtStart[i]=Phase; GaitSwingProgress[i]=0.f;
            }
            if (Stance&&!Settling&&!GaitPlanted[i]&&!NewFoot)
            {
                const bool Wrapped=Phase<GaitFootPhase[i]-.5f;
                // During walk/trot re-phasing, a support phase can begin
                // before the nominal cycle wraps. Set that hoof down where
                // it currently is, rather than snapping to a future footfall.
                if (Wrapped) GaitFootWorld[i]=GaitSwingEnd[i];
                GaitFootWorld[i].Z=GaitGroundZ[i];
            }
            if (!Stance)
            {
                // Predict only the time remaining in this actual swing.
                // Recompute while velocity changes: acceleration, braking,
                // or blended foot phases must not overreach at touchdown.
                const float RemainingTime=CycleDistance/FMath::Max(35.f,ActualSpeed)*(1.f-Phase);
                const FVector Ahead=(GetVelocity()*RemainingTime).GetClampedToMaxSize(70.f*VisualScale);
                GaitSwingEnd[i]=RootTransform.TransformPosition(FVector(Hip.X+Reach*GaitMotion,Hip.Y,0))+Ahead;
                GaitSwingEnd[i].Z=GaitGroundZ[i];
                const float T=FMath::Clamp((Phase-GaitSwingPhaseAtStart[i])/FMath::Max(.02f,1.f-GaitSwingPhaseAtStart[i]),0.f,1.f);
                GaitSwingProgress[i]=FMath::Max(GaitSwingProgress[i],T);
                const float Ease=GaitSwingProgress[i]*GaitSwingProgress[i]*(3.f-2.f*GaitSwingProgress[i]);
                GaitFootWorld[i]=FMath::Lerp(GaitSwingStart[i],GaitSwingEnd[i],Ease);
                GaitFootWorld[i].Z+=FMath::Sin(GaitSwingProgress[i]*PI)*FMath::Lerp(6.f,10.f,GaitTrot)*VisualScale*GaitMotion;
            }
            else if (Settling)
            {
                // Re-settle individual hooves after stopping instead of
                // cycling stationary legs or snapping all four together.
                GaitFootWorld[i]=FMath::VInterpTo(GaitFootWorld[i],FVector(Neutral.X,Neutral.Y,GaitGroundZ[i]),DeltaSeconds,4.f);
            }
            GaitPlanted[i]=Stance;
            }
            GaitFootPhase[i]=Phase;
            FVector LegGoal=RootTransform.InverseTransformPosition(GaitFootWorld[i])-Hip;
            bool RecoveryBegan=false;
            // A sharp turn, acceleration or frame hitch can leave an old
            // support behind the body's anatomical reach. Actually lift and
            // step that hoof forward; never slide a supposedly planted foot.
            if (!GaitRecoveryActive[i]&&GaitPlanted[i]&&LegGoal.Size()>48.15f&&!Settling)
            {
                GaitRecoveryActive[i]=true; RecoveryBegan=true; GaitRecoveryAge[i]=0.f;
                GaitRecoveryDuration[i]=FMath::Clamp(CycleDistance/FMath::Max(35.f,ActualSpeed)*(1.f-Phase),.075f,.18f);
                GaitRecoveryStart[i]=GetHoofWorldPosition(i); GaitRecoveryStart[i].Z=GaitGroundZ[i];
            }
            if (GaitRecoveryActive[i])
            {
                // Keep a visible intermediate step even across one long
                // rendering frame. The physics body continues normally.
                if (!RecoveryBegan) GaitRecoveryAge[i]+=FMath::Min(DeltaSeconds,1.f/30.f);
                const float T=FMath::Clamp(GaitRecoveryAge[i]/GaitRecoveryDuration[i],0.f,1.f);
                const float Remaining=FMath::Max(0.f,GaitRecoveryDuration[i]-GaitRecoveryAge[i]);
                FVector Landing=RootTransform.TransformPosition(FVector(Hip.X+Reach*GaitMotion,Hip.Y,0))+GetVelocity()*Remaining;
                Landing.Z=GaitGroundZ[i];
                const float Ease=T*T*(3.f-2.f*T);
                GaitFootWorld[i]=FMath::Lerp(GaitRecoveryStart[i],Landing,Ease);
                GaitFootWorld[i].Z+=FMath::Sin(T*PI)*7.f*VisualScale;
                GaitPlanted[i]=false;
                if (T>=1.f)
                { GaitRecoveryActive[i]=false; GaitFootWorld[i]=Landing; GaitPlanted[i]=true; }
                LegGoal=RootTransform.InverseTransformPosition(GaitFootWorld[i])-Hip;
            }
            SolveLeg(LegGoal,i,UpperGoal,LowerGoal);
            if (LowerLegMesh)
            {
                // Contact the authored cloven soles, not an empty corner
                // of the whole calf's bounding box. Each sole is an ellipse
                // at Z=-23: center X2.9/Y+/-2.95, radii3.8/2.5 cm. Its exact
                // support along world down keeps pitched toes touching floor.
                const FVector UncorrectedGoal=LegGoal;
                for (int32 Iteration=0;Iteration<3;++Iteration)
                {
                    const FQuat Combined=UpperGoal*LowerGoal;
                    const float SoleX=RootTransform.TransformVectorNoScale(Combined.RotateVector(FVector::ForwardVector)).Z;
                    const float SoleY=RootTransform.TransformVectorNoScale(Combined.RotateVector(FVector::RightVector)).Z;
                    const float SoleRadius=FMath::Sqrt(FMath::Square(SoleX*3.8f)+FMath::Square(SoleY*2.5f));
                    const float Lowest=SoleX*2.9f-FMath::Abs(SoleY)*2.95f-SoleRadius;
                    LegGoal=UncorrectedGoal+RootTransform.InverseTransformVectorNoScale(FVector(0,0,FMath::Clamp(-Lowest,-7.f,7.f)));
                    SolveLeg(LegGoal,i,UpperGoal,LowerGoal);
                }
            }
            GaitUpperGround[i]=UpperGoal.Rotator(); GaitLowerGround[i]=LowerGoal.Rotator();
        }
        else { GaitFootPhase[i]=-1.f; GaitRecoveryActive[i]=false; }
        const FQuat SleepingUpper=FRotator(95,0,0).Quaternion(),SleepingLower=FRotator(-150,0,0).Quaternion();
        UpperGoal=FQuat::Slerp(UpperGoal,SleepingUpper,SleepBlend);
        LowerGoal=FQuat::Slerp(LowerGoal,SleepingLower,SleepBlend);
        if (CarryAlpha>0.f)
        {
            FQuat HangingUpper,HangingLower;
            if (i==2)
            {
                // The held rear ankle remains extended along the authored
                // leg. Root anchors this actual lower mesh near its hoof.
                HangingUpper=FRotator(-2,0,0).Quaternion(); HangingLower=FRotator(4,0,0).Quaternion();
            }
            else
            {
                const FVector Gravity=RootTransform.InverseTransformVectorNoScale(FVector(0,0,-1)).GetSafeNormal();
                FVector Dangle=Gravity*43.f+FVector(FMath::Sin(VisualTime*1.7f+i*.8f)*1.1f,0,0);
                SolveLeg(Dangle,i,HangingUpper,HangingLower);
            }
            UpperGoal=FQuat::Slerp(UpperGoal,HangingUpper,CarryAlpha);
            LowerGoal=FQuat::Slerp(LowerGoal,HangingLower,CarryAlpha);
            GaitPlanted[i]=false;
        }
        // Smoothing remains active on stand/sleep/lift/placement transitions.
        // Ground IK responds quickly enough to retain the locked stance hoof.
        const float Rate=GroundPose&&SleepBlend<.05f?35.f:i==2&&Held?10.f:6.f;
        // Stance uses exact IK immediately on contact. Filtering joint angles
        // after touchdown makes fast, small pigs skate even with a locked
        // target. Swing, sleep, lift, and release retain smooth interpolation.
        const float Blend=GroundPose&&SleepBlend<.05f&&GaitPlanted[i]?1.f:1.f-FMath::Exp(-Rate*DeltaSeconds);
        UpperLegs[i]->SetRelativeRotation(FQuat::Slerp(UpperLegs[i]->GetRelativeRotation().Quaternion(),UpperGoal,Blend));
        LowerLegs[i]->SetRelativeRotation(FQuat::Slerp(LowerLegs[i]->GetRelativeRotation().Quaternion(),LowerGoal,Blend));
    }
}
FVector AHouseRoamingPig::GetHoofWorldPosition(int32 Leg) const
{
    return LowerLegs.IsValidIndex(Leg)?LowerLegs[Leg]->GetComponentTransform().TransformPosition(FVector(0,0,-23)):FeetLocation();
}
void AHouseRoamingPig::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HasAuthority())
    {
        const FVector Feet=FeetLocation(); const float Moved=FVector::Dist2D(Feet,LastFeet);
        if (Moved<150.f) TravelDistance+=Moved;
        LastFeet=Feet;
        const bool Inside=IsInsidePigsty(); if (bWasInside&&!Inside) ++Escapes; bWasInside=Inside;
        if (Carrier || Mood==EHousePigMood::Carried) UpdateCarry(DeltaSeconds);
        else
        {
            ThinkAccumulator+=DeltaSeconds;
            if (ThinkAccumulator>=.2f) { ThinkAccumulator=0; Think(); }
            FollowPath(DeltaSeconds);
        }
    }
    if (GetNetMode()!=NM_DedicatedServer) Animate(DeltaSeconds);
}
void AHouseRoamingPig::HealFromSnack()
{
    if (HasAuthority()&&Health>0) { Health=FMath::Min(MaxHealth,Health+2.f); ForceNetUpdate(); }
}
float AHouseRoamingPig::TakeDamage(float DamageAmount,const FDamageEvent& DamageEvent,AController* EventInstigator,AActor* DamageCauser)
{
    if (!HasAuthority()||Health<=0||!FMath::IsFinite(DamageAmount)||DamageAmount<=0) return 0;
    const float Actual=FMath::Min(Health,Super::TakeDamage(DamageAmount,DamageEvent,EventInstigator,DamageCauser));
    Health=FMath::Max(0.f,Health-Actual); ++DamageEvents;
    WakeUp();
    Path.Reset(); PathCursor=0; bLocalGoal=false;
    if (Health<=0)
    {
        // A downed animal must leave the animated hold immediately so gravity
        // owns its fall; an asynchronous living-pig placement cannot retain it.
        if (IsCarried()) ReleaseCarrier();
        Mood=EHousePigMood::Down; GetCharacterMovement()->StopMovementImmediately();
        // A pig downed while held still falls onto the floor instead of being
        // left suspended in the air. No AI or movement input runs at zero HP.
        GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    }
    else if (!IsCarried()) { Mood=EHousePigMood::Fleeing; FleeUntil=GetWorld()->GetTimeSeconds()+4.f; ActivityUntil=0; }
    ForceNetUpdate(); return Actual;
}
bool AHouseRoamingPig::ShouldShowHealthTo(APlayerController* Viewer,FVector& LabelWorld) const
{
    if (!Viewer||!GetWorld()) return false;
    FVector View; FRotator Rotation; Viewer->GetPlayerViewPoint(View,Rotation);
    LabelWorld=FeetLocation()+FVector(0,0,(Mood==EHousePigMood::Sleeping?66.f:112.f)*VisualScale);
    const FVector To=LabelWorld-View;
    if (To.SizeSquared()>FMath::Square(1200.f)||FVector::DotProduct(To.GetSafeNormal(),Rotation.Vector())<.25f) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PigHealthVisibility),false,this); Query.AddIgnoredActor(Viewer->GetPawn());
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,View,GetInteractionLocation(),ECC_Visibility,Query)
        && !GetWorld()->LineTraceSingleByChannel(Hit,View,GetInteractionLocation(),ECC_Pawn,Query);
}
void AHouseRoamingPig::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHouseRoamingPig,Health); DOREPLIFETIME(AHouseRoamingPig,Mood);
    DOREPLIFETIME(AHouseRoamingPig,VisualScale); DOREPLIFETIME(AHouseRoamingPig,MaxHealth); DOREPLIFETIME(AHouseRoamingPig,Carrier);
    DOREPLIFETIME(AHouseRoamingPig,CarryPhase); DOREPLIFETIME(AHouseRoamingPig,CarryBlend); DOREPLIFETIME(AHouseRoamingPig,CarryPhaseStarted);
}
