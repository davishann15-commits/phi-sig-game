#include "HouseTreeReaction.h"
#include "HouseLeafWind.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AHouseTreeReaction::AHouseTreeReaction()
{
    bReplicates=true;
    bAlwaysRelevant=true;
    SetReplicateMovement(false);
    PrimaryActorTick.bCanEverTick=false;
    SetNetUpdateFrequency(2.f);
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("TreeReactionOrigin"));
}

bool AHouseTreeReaction::HitTree(const FTransform& Tree,uint8 Kind,uint8 Variant,const FVector& Direction)
{
    if(!HasAuthority()||!GetWorld()||Kind>=3||Variant>=3)return false;
    const FVector P=Tree.GetLocation();
    const FIntVector Key(FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y),FMath::RoundToInt(P.Z));
    const double Now=GetWorld()->GetTimeSeconds();
    if(const double* Last=LastHits.Find(Key))if(Now-*Last<.65)return false;
    LastHits.Add(Key,Now);
    const int32 Seed=202610110+ ++AcceptedHits*137;
    ForceNetUpdate();
    MulticastShedLeaves(Tree,Kind,Variant,Direction.GetSafeNormal(),Seed);
    UE_LOG(LogTemp,Display,TEXT("TREE_HIT species=%s base=%s seed=%d count=%d"),
        HouseNature::TreeName(HouseNature::ETreeKind(Kind)),*P.ToCompactString(),Seed,AcceptedHits);
    return true;
}

void AHouseTreeReaction::MulticastShedLeaves_Implementation(const FTransform& Tree,uint8 Kind,uint8 Variant,FVector_NetQuantizeNormal Direction,int32 Seed)
{
    ++ReceivedEvents;
    if(GetNetMode()==NM_DedicatedServer)return;
    TActorIterator<AHouseLeafWind> It(GetWorld());
    if(It)
    {
        It->ShedLeaves(Tree,Kind,Variant,Direction,Seed);
    }
    UE_LOG(LogTemp,Display,TEXT("TREE_EVENT mode=%d species=%d seed=%d received=%d"),int32(GetNetMode()),Kind,Seed,ReceivedEvents);
}
