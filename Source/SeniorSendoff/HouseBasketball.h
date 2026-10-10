#pragma once
#include "CoreMinimal.h"
#include "HouseInteractiveProp.h"
#include "HouseBasketball.generated.h"

class UPhysicalMaterial;
class AStoryFirstPersonCharacter;

/** One shared server-authoritative ball, carried by the existing replicated furniture handle. */
UCLASS(NotPlaceable)
class SENIORSENDOFF_API AHouseBasketball : public AHouseInteractiveProp
{
    GENERATED_BODY()
public:
    AHouseBasketball();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual float TakeDamage(float Amount,const FDamageEvent& Event,AController* Instigator,AActor* Causer) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    FVector ShotVelocity(const FVector& View,const FRotator& Aim,const AActor* Shooter = nullptr) const;
    void MarkShot();
    UPROPERTY(Replicated) int32 LeftBaskets = 0;
    UPROPERTY(Replicated) int32 RightBaskets = 0;
    UPROPERTY(Replicated) float LastBasketAt = -100.f;
private:
    void UpdateBall();
    UPROPERTY(Transient) TObjectPtr<UPhysicalMaterial> Rubber;
    FTimerHandle BallTimer;
    FVector Previous = FVector::ZeroVector;
    double ShotAt = -100, ScoredAt = -100, OutsideSince = -1;
    float PhysicsStep = 1.f/60.f;
};

/** Explicit opt-in real physics and pickup/shooting check in the campaign map. */
void TickHouseBasketballReview(AStoryFirstPersonCharacter* Pawn);
