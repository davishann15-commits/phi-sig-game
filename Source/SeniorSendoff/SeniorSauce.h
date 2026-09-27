#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "SeniorSauce.generated.h"

class ACharacter;
class USphereComponent;
class UStaticMeshComponent;
class USceneComponent;

// Shared by overlapping sauce patches, so leaving one patch does not cancel
// the slow while a senior is still standing in another one.
UCLASS(ClassGroup=(SeniorSendoff))
class SENIORSENDOFF_API USeniorSauceSlowComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    void AddSource();
    void RemoveSource();
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void ApplySpeed(float Speed);
    int32 Sources=0;
    float OriginalSpeed=0.f;
};

UCLASS()
class SENIORSENDOFF_API ASeniorSaucePuddle : public AActor
{
    GENERATED_BODY()
public:
    ASeniorSaucePuddle();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Sauce") float Radius=190.f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Sauce") float Duration=6.f;
private:
    UFUNCTION() void Enter(UPrimitiveComponent* Overlapped,AActor* Other,UPrimitiveComponent* OtherComp,int32 BodyIndex,bool FromSweep,const FHitResult& Sweep);
    UFUNCTION() void Leave(UPrimitiveComponent* Overlapped,AActor* Other,UPrimitiveComponent* OtherComp,int32 BodyIndex);
    void AddEnemy(AActor* Actor);
    void RemoveEnemy(AActor* Actor);
    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Area;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Splashes;
    TMap<TWeakObjectPtr<ACharacter>,TWeakObjectPtr<USeniorSauceSlowComponent>> Affected;
};

UENUM(BlueprintType)
enum class ESeniorSaucePhase : uint8 { Ready, Flying, Recovering };

// One reusable packet per Braxton player. The server flies it, places the
// puddle on impact, and makes the next packet available after a short delay.
UCLASS()
class SENIORSENDOFF_API ASeniorSaucePacket : public AActor
{
    GENERATED_BODY()
public:
    ASeniorSaucePacket();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="Sauce") ESeniorSaucePhase Phase=ESeniorSaucePhase::Ready;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Sauce") float ThrowSpeed=1450.f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Sauce") float Cooldown=1.7f;
    bool BeginThrow();
    FString StatusText() const;
private:
    void Burst(const FHitResult* Impact);
    void SetPacketVisible(USceneComponent* Visual,bool Visible,bool HideOwner);
    UPROPERTY(Transient) TObjectPtr<USceneComponent> WorldPacket;
    UPROPERTY(Transient) TObjectPtr<USceneComponent> FirstPersonPacket;
    FVector Velocity=FVector::ZeroVector;
    float Distance=0.f;
    float ReadyAt=0.f;
    float Spin=0.f;
};
