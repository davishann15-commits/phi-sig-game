#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseTreeReaction.generated.h"

/** One server-owned relay for cosmetic tree impacts; individual leaves never replicate. */
UCLASS(NotPlaceable, Transient)
class SENIORSENDOFF_API AHouseTreeReaction : public AActor
{
    GENERATED_BODY()
public:
    AHouseTreeReaction();
    bool HitTree(const FTransform& Tree,uint8 Kind,uint8 Variant,const FVector& Direction);
    int32 AcceptedHits=0, ReceivedEvents=0;
private:
    UFUNCTION(NetMulticast, Reliable)
    void MulticastShedLeaves(const FTransform& Tree,uint8 Kind,uint8 Variant,FVector_NetQuantizeNormal Direction,int32 Seed);
    TMap<FIntVector,double> LastHits;
};

class AStoryFirstPersonCharacter;
/** Opt-in actual-weapon, falling-leaf and placement checks. */
void TickHouseTreesReview(AStoryFirstPersonCharacter* Pawn);
