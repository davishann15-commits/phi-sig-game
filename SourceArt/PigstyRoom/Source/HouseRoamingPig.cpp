#include "HouseRoamingPig.h"
#include "ExistingHouseDoor.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
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
    PrimaryActorTick.bCanEverTick=false;
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
    const int32 Count=FMath::Clamp(FoodCount,1,128);
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
    if (HasAuthority()) FoodRemaining=FMath::Clamp(FoodCount,1,128);
    RefreshFood();
}
void AHousePigSnackPile::OnRep_Food() { RefreshFood(); }
bool AHousePigSnackPile::Consume(AHouseRoamingPig* Pig)
{
    if (!HasAuthority() || !IsValid(Pig) || FoodRemaining<=0 || FVector::Dist2D(Pig->FeetLocation(),GetActorLocation())>145.f) return false;
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
    Body->SetStaticMesh(BodyMesh); Head->SetStaticMesh(HeadMesh);
    for (int32 i=0;i<4;++i) { UpperLegs[i]->SetStaticMesh(UpperLegMesh); LowerLegs[i]->SetStaticMesh(LowerLegMesh); }
}
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
    if (!HasAuthority() || Health<=0 || !RoamGraph.IsValidIndex(Node)) return false;
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
    if (!HasAuthority()||Health<=0||!IsInsidePigsty()||!IsValid(SnackPile)||SnackPile->FoodRemaining<=0) return false;
    Path.Reset(); PathCursor=0; LocalGoal=SnackPile->FeedingFeet(PersonalitySeed);
    bLocalGoal=true; bFeedIntent=true; bBedIntent=false; Mood=EHousePigMood::Walking;
    StuckSeconds=NoProgressSeconds=0; BestGoalDistance=BIG_NUMBER;
    return true;
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
    if (GetWorld()->SweepSingleByChannel(Obstacle,Feet+FVector(0,0,64),End+FVector(0,0,64),
        Direction.Rotation().Quaternion(),ECC_Pawn,FCollisionShape::MakeBox(FVector(65,27,20)),Query)) return false;
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
    if (Inside && Personality.FRand()<.15f && Now>=FleeUntil)
    { LocalGoal=BedLocation; bLocalGoal=true; bBedIntent=true; Mood=EHousePigMood::Walking; return; }
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
    if (Health<=0) return;
    if (Now>=FleeUntil)
        for (FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
            if (const APlayerController* PC=It->Get(); PC&&PC->GetPawn()
                && FVector::Dist2D(PC->GetPawn()->GetActorLocation(),FeetLocation())<120.f
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
    if (PathCursor<Path.Num()||bLocalGoal) return;
    if (Now<ActivityUntil) return;
    ChooseActivity();
}
void AHouseRoamingPig::FollowPath(float DeltaSeconds)
{
    if (Health<=0 || (PathCursor>=Path.Num()&&!bLocalGoal)) return;
    FVector Goal;
    if (bLocalGoal) Goal=LocalGoal;
    else
    {
        const int32 Node=Path[PathCursor];
        if (PathCursor>0&&!EdgeAllows(Path[PathCursor-1],Node))
        { Path.Reset(); PathCursor=0; Mood=EHousePigMood::Sniffing; ActivityUntil=GetWorld()->GetTimeSeconds()+1.f; return; }
        Goal=RoamGraph[Node].Feet;
    }
    if (FVector::Dist2D(FeetLocation(),Goal)<22.f && FMath::Abs(FeetLocation().Z-Goal.Z)<65.f)
    {
        if (bLocalGoal)
        {
            bLocalGoal=false;
            Mood=bFeedIntent?EHousePigMood::Feeding:bBedIntent?
                (Personality.FRand()<.5f?EHousePigMood::Sleeping:EHousePigMood::Resting):EHousePigMood::Sniffing;
            ActivityUntil=GetWorld()->GetTimeSeconds()+Personality.FRandRange(bFeedIntent?3.f:2.f,bFeedIntent?7.f:5.f);
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
    GetCharacterMovement()->MaxWalkSpeed=(Mood==EHousePigMood::Running||Mood==EHousePigMood::Fleeing)?RunSpeed:WalkSpeed;
    const FRotator Face=FMath::RInterpTo(GetActorRotation(),Direction.Rotation(),DeltaSeconds,5.f);
    SetActorRotation(FRotator(0,Face.Yaw,0));
    AddMovementInput(Direction,1.f,true);
}
void AHouseRoamingPig::Animate(float DeltaSeconds)
{
    VisualTime+=DeltaSeconds; const float Speed=GetVelocity().Size2D();
    const bool Sleep=Mood==EHousePigMood::Sleeping||Mood==EHousePigMood::Down;
    const bool Sniff=Mood==EHousePigMood::Sniffing||Mood==EHousePigMood::Feeding;
    const bool Moving=Speed>15.f;
    const float Stride=Speed>160.f?18.f:12.f;
    GaitPhase=FMath::Fmod(GaitPhase+Speed*DeltaSeconds*(2.f*PI)/(4.f*Stride),2.f*PI);
    if (Speed>15.f) GaitDistance+=Speed*DeltaSeconds;
    const float Bob=Moving?FMath::Sin(GaitPhase*2)*.8f:0;
    const bool Feed=Mood==EHousePigMood::Feeding;
    const float DesiredCrouch=Feed?26.f:Sniff?24.f:Sleep?17.f:3.f;
    VisualCrouch=FMath::FInterpTo(VisualCrouch,DesiredCrouch,DeltaSeconds,5.f);
    const UCharacterMovementComponent* Movement=GetCharacterMovement();
    const float FloorDistance=Movement->IsMovingOnGround()&&Movement->CurrentFloor.bBlockingHit?Movement->CurrentFloor.FloorDist:0.f;
    VisualRoot->SetRelativeLocation(FVector(0,0,-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-FloorDistance+Bob-VisualCrouch));
    Body->SetRelativeRotation(FRotator(0,0,Sleep?12.f:0.f));
    Head->SetRelativeLocation(FVector(60,0,76-(Sniff?20.f:0.f)));
    Head->SetRelativeRotation(FRotator(Sniff?-55.f+FMath::Sin(VisualTime*7)*3.f:Sleep?-12.f:FMath::Sin(VisualTime*1.6f)*3.f,
        Sniff?FMath::Sin(VisualTime*2.4f)*8.f:FMath::Sin(VisualTime*.8f)*3.f,0));
    for (int32 i=0;i<4;++i)
    {
        if (Sleep)
        {
            UpperLegs[i]->SetRelativeRotation(FRotator(48.f,0,0));
            LowerLegs[i]->SetRelativeRotation(FRotator(-65.f,0,0));
            continue;
        }
        const float Phase=FMath::Fmod(GaitPhase+((i==0||i==3)?0.f:PI),2.f*PI);
        const bool Swing=Moving&&Phase<PI;
        // The stance hoof travels backward as the body moves forward. Only
        // the returning swing hoof lifts; solving both bones preserves contact.
        const float GoalX=!Moving?0.f:Swing?FMath::Lerp(-Stride,Stride,Phase/PI):
            FMath::Lerp(Stride,-Stride,(Phase-PI)/PI);
        const float Lift=Swing?FMath::Sin(Phase)*8.f:.05f;
        const float GoalZ=VisualCrouch-Bob+Lift;
        const float DownZ=48.f-GoalZ;
        const float Distance=FMath::Clamp(FMath::Sqrt(GoalX*GoalX+DownZ*DownZ),2.05f,47.8f);
        const float BendSign=i<2?1.f:-1.f;
        const float Upper=FMath::Atan2(-GoalX,DownZ)+BendSign*FMath::Acos(FMath::Clamp(
            (25.f*25.f+Distance*Distance-23.f*23.f)/(50.f*Distance),-1.f,1.f));
        const float Lower=-BendSign*FMath::Acos(FMath::Clamp(
            (Distance*Distance-25.f*25.f-23.f*23.f)/(2.f*25.f*23.f),-1.f,1.f));
        UpperLegs[i]->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(Upper),0,0));
        LowerLegs[i]->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(Lower),0,0));
    }
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
        ThinkAccumulator+=DeltaSeconds;
        if (ThinkAccumulator>=.2f) { ThinkAccumulator=0; Think(); }
        FollowPath(DeltaSeconds);
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
    Path.Reset(); PathCursor=0; bLocalGoal=false;
    if (Health<=0) { Mood=EHousePigMood::Down; GetCharacterMovement()->StopMovementImmediately(); GetCharacterMovement()->DisableMovement(); }
    else { Mood=EHousePigMood::Fleeing; FleeUntil=GetWorld()->GetTimeSeconds()+4.f; ActivityUntil=0; }
    ForceNetUpdate(); return Actual;
}
bool AHouseRoamingPig::ShouldShowHealthTo(APlayerController* Viewer,FVector& LabelWorld) const
{
    if (!Viewer||!GetWorld()) return false;
    FVector View; FRotator Rotation; Viewer->GetPlayerViewPoint(View,Rotation);
    LabelWorld=FeetLocation()+FVector(0,0,112);
    const FVector To=LabelWorld-View;
    if (To.SizeSquared()>FMath::Square(1200.f)||FVector::DotProduct(To.GetSafeNormal(),Rotation.Vector())<.25f) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PigHealthVisibility),false,this); Query.AddIgnoredActor(Viewer->GetPawn());
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,View,FeetLocation()+FVector(0,0,70),ECC_Visibility,Query);
}
void AHouseRoamingPig::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AHouseRoamingPig,Health); DOREPLIFETIME(AHouseRoamingPig,Mood);
}
