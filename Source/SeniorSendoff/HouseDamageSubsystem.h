#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "HouseDamageSubsystem.generated.h"

class AController;

USTRUCT(BlueprintType)
struct FHouseDamageCategoryCount
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName Category;
    UPROPERTY(BlueprintReadOnly) int32 Count = 0;
};

/** One small replicated score record per game world, created on the first break. */
UCLASS(NotBlueprintable, NotPlaceable)
class SENIORSENDOFF_API AHouseDamageState : public AActor
{
    GENERATED_BODY()
public:
    AHouseDamageState();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="House Damage") int32 TotalBrokenCount = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="House Damage") TArray<FHouseDamageCategoryCount> CategoryCounts;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="House Damage") FName LastBrokenCategory;
    int32 CountFor(FName Category) const;
};

/** Counts permanent damage once per piece for this visit to the house. */
UCLASS()
class SENIORSENDOFF_API UHouseDamageSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

    UFUNCTION(BlueprintCallable, Category="House Damage")
    bool RegisterBrokenActor(AActor* BrokenActor, FName Category = TEXT("Furniture"), AController* Instigator = nullptr);

    UFUNCTION(BlueprintPure, Category="House Damage") int32 GetTotalBrokenCount() const;
    UFUNCTION(BlueprintPure, Category="House Damage") int32 GetBrokenCount(FName Category) const;

    // These mirrors are useful for authoritative gameplay. HUDs use the getters
    // so remote players read the same replicated record as the server.
    UPROPERTY(Transient, BlueprintReadOnly, Category="House Damage") int32 TotalBrokenCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="House Damage") int32 BrokenFurnitureCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="House Damage") int32 BrokenWindowCount = 0;

    /** Local, cosmetic: a nearby break or crack jolts first-person cameras. */
    void AddCameraImpact(const FVector& Location, float StrengthCm);
    /** Camera-space offset (X forward, Y right, Z up, cm) for a camera at this location. */
    FVector SampleCameraImpact(const FVector& CameraLocation) const;

private:
    struct FCameraImpact { FVector Location = FVector::ZeroVector; float Strength = 0.f; double Time = 0.0; };
    TArray<FCameraImpact> CameraImpacts;
    AHouseDamageState* FindState() const;
    mutable TWeakObjectPtr<AHouseDamageState> State;
    TSet<TWeakObjectPtr<AActor>> CountedActors;
};
