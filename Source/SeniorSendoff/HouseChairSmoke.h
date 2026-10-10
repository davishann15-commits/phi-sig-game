#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseChairSmoke.generated.h"

class UPrimitiveComponent;

// Spawned only by -HouseChairSmoke. Records a real Chaos body collision after LMB throw.
UCLASS(Transient)
class SENIORSENDOFF_API AHouseChairSmokeHitObserver : public AActor
{
    GENERATED_BODY()
public:
    AHouseChairSmokeHitObserver();
    UPROPERTY(Transient) int32 HitCount = 0;
    UFUNCTION() void OnChairHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
