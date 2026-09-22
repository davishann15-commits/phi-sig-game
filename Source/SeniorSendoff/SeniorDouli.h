#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SeniorDouli.generated.h"
class UStaticMeshComponent;
class AStoryFirstPersonCharacter;
UENUM(BlueprintType)
enum class EDouliPhase : uint8 { Ready, Windup, Outbound, Returning, Catching };

// The server owns flight, hit detection, damage and the one-hat gate.
// Held poses are cosmetic on each machine; flight is replicated movement.
UCLASS()
class SENIORSENDOFF_API ASeniorDouli : public AActor
{
    GENERATED_BODY()
public:
    ASeniorDouli();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Hat;
    UPROPERTY(ReplicatedUsing=OnRep_Phase, BlueprintReadOnly) EDouliPhase Phase = EDouliPhase::Ready;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float Damage = 35.f;
    UPROPERTY(EditDefaultsOnly) float ThrowSpeed = 2100.f;
    UPROPERTY(EditDefaultsOnly) float ReturnSpeed = 2700.f;
    UPROPERTY(EditDefaultsOnly) float MaxRange = 1800.f;
    bool BeginThrow();
    bool IsHeld() const { return Phase == EDouliPhase::Ready || Phase == EDouliPhase::Windup || Phase == EDouliPhase::Catching; }
    float GetPhaseAge() const;
    FString StatusText() const;
    UFUNCTION() void OnRep_Phase();
private:
    void SetPhase(EDouliPhase Value);
    void DamageHit(const FHitResult& Hit);
    UPROPERTY(Replicated) float PhaseStart = 0;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FirstPersonHat;
    FVector FlightDirection = FVector::ForwardVector;
    float DistanceTravelled = 0;
    float Spin = 0;
};

// Transient automation fixture, never placed in campaign maps.
UCLASS(NotPlaceable)
class ASeniorDouliTestTarget : public AActor
{
    GENERATED_BODY()
public:
    float ReceivedDamage = 0;
    virtual float TakeDamage(float Amount, const FDamageEvent&, AController*, AActor*) override
    { ReceivedDamage += Amount; return Amount; }
};
