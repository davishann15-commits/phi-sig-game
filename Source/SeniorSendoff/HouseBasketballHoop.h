#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseBasketballHoop.generated.h"

class UProceduralMeshComponent;
class UPrimitiveComponent;
class UPhysicalMaterial;
class AHouseBasketball;
class AStoryFirstPersonCharacter;

/** Physical open rim and board, with a pinned, deformable rope net. */
UCLASS(NotPlaceable, Transient)
class SENIORSENDOFF_API AHouseBasketballHoop : public AActor
{
    GENERATED_BODY()
public:
    AHouseBasketballHoop();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void BallImpact(UPrimitiveComponent* Surface, const FVector& Point, const FVector& Velocity, float Speed);
    void Basket(const FVector& Velocity);
    float NetDisplacement() const;
    float PinError() const;
    int32 RimHits = 0, BoardHits = 0, Swishes = 0, NetContacts = 0, AudioEvents = 0;
    int32 End() const { return GetActorLocation().X < 0 ? 0 : 1; }
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Net;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> RimMesh;
    UPROPERTY(Transient) TObjectPtr<UPhysicalMaterial> Metal;
    struct FLink { int32 A, B; float Length; };
    struct FBallContact { FVector Center, Before; float Radius; };
    TArray<FVector> Rest, P, Prev;
    TArray<FLink> Links;
    TArray<TWeakObjectPtr<AHouseBasketball>> Balls;
    TMap<TWeakObjectPtr<AHouseBasketball>, FVector> LastBallPositions;
    float Accumulator = 0, MeshAge = 0, CacheAge = 10, Clock = 0;
    double LastRimSound = -10, LastBoardSound = -10;
    void Step(float Dt, const TArray<FBallContact>& Contacts, float Fraction);
    void UpdateNetMesh(bool First);
    void PushNet(const FVector& Point, const FVector& Velocity);
    UFUNCTION(NetMulticast, Unreliable)
    void MulticastContact(FVector_NetQuantize Point, FVector_NetQuantize Velocity, uint8 Kind, uint8 Strength);
};

void SpawnHouseBasketballHoops(UWorld* World);
void TickHouseBasketballSkillReview(AStoryFirstPersonCharacter* Pawn);
