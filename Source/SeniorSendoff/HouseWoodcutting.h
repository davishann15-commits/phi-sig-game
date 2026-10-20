#pragma once

#include "CoreMinimal.h"
#include "HouseInteractiveProp.h"
#include "HouseWoodcutting.generated.h"

class AStoryFirstPersonCharacter;
class UProceduralMeshComponent;

/** Fixed chopping block beside Cole's hut. One server actor, locally built art. */
UCLASS(NotPlaceable)
class SENIORSENDOFF_API AHouseChoppingStump : public AActor
{
    GENERATED_BODY()
public:
    AHouseChoppingStump();
    virtual void BeginPlay() override;
    virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* Instigator, AActor* Causer) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Block;
    UPROPERTY(ReplicatedUsing=OnRep_Chops) int32 Chops = 0;
    FVector Top() const { return GetActorLocation() + FVector(0, 0, 54); }
    static FVector Home();
private:
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Marks;
    void UpdateMarks();
    UFUNCTION() void OnRep_Chops();
    UFUNCTION(NetMulticast, Unreliable) void MulticastChop(FVector_NetQuantize Point, FVector_NetQuantizeNormal Direction);
};

/** A real pickup body with a handle grip and a reusable, server-authoritative melee swing. */
UCLASS(NotPlaceable)
class SENIORSENDOFF_API AHouseWoodAxe : public AHouseInteractiveProp
{
    GENERATED_BODY()
public:
    AHouseWoodAxe();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* Instigator, AActor* Causer) override { return 0; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    void PreparePickup();
    bool BeginSwing(AStoryFirstPersonCharacter* Holder);
    void CancelSwing();
    void CarryPose(const FVector& View, const FRotator& Aim, FVector& Grip, FQuat& Rotation) const;
    bool CanReturnToStump(const AStoryFirstPersonCharacter* Holder) const;
    void EmbedInStump();
    static FVector GripLocal() { return FVector(1, -1.6f, 6); }
    static FVector EdgeLocal() { return FVector(24, 0, 58); }
    static FVector StumpTipLocal() { return FVector(24, 0, 70); }
    static constexpr float StumpInsertion = 1.5f;
    static FTransform EmbeddedTransform();
    UPROPERTY(Replicated) bool bEmbedded = true;
    UPROPERTY(Replicated) float SwingStarted = -10;
    int32 SwingCount = 0, HitCount = 0;
    bool IsSwinging() const;
private:
    TWeakObjectPtr<AStoryFirstPersonCharacter> SwingHolder;
    bool bStrikeResolved = true;
    float Clock() const;
    void ResolveStrike();
    UFUNCTION(NetMulticast, Unreliable) void MulticastStrikeFx(FVector_NetQuantize Point, uint8 Surface);
};

void SpawnHouseWoodcutting(UWorld* World);
void TickHouseWoodcuttingReview(AStoryFirstPersonCharacter* Pawn);
