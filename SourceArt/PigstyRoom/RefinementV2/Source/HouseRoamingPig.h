#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HouseRoamingPig.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class AExistingHouseDoor;
class AHouseRoamingPig;

UENUM(BlueprintType)
enum class EHousePigMood : uint8 { Walking, Running, Sniffing, Feeding, Resting, Sleeping, Fleeing, Down, Carried };

USTRUCT(BlueprintType)
struct FHousePigRoamNode
{
    GENERATED_BODY()
    // Feet coordinates in Unreal world cm, never a teleport target.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Feet = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> Links;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bInsidePigsty = true;
    // Edges touching this node wait for the named door to be fully open.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName GateDoorId;
};

UCLASS(Blueprintable)
class SENIORSENDOFF_API AHousePigSnackPile : public AActor
{
    GENERATED_BODY()
public:
    AHousePigSnackPile();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UInstancedStaticMeshComponent> Cheetos;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig Food") TObjectPtr<UStaticMesh> CheetoMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig Food", meta=(ClampMin="1",ClampMax="128")) int32 FoodCount = 24;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig Food") float ScatterRadius = 50.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig Food") int32 ScatterSeed = 2481;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig Food") FVector FeedingOffsetA = FVector(-90,0,-.5);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig Food") FVector FeedingOffsetB = FVector(90,0,-.5);
    UPROPERTY(ReplicatedUsing=OnRep_Food, BlueprintReadOnly, Category="Pig Food") int32 FoodRemaining = 24;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig Food", meta=(ClampMin="0.1")) float RefillInterval = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig Food", meta=(ClampMin="1",ClampMax="24")) int32 RefillAmount = 4;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig Food") int32 FoodRespawns = 0;
    bool Consume(AHouseRoamingPig* Pig);
    FVector FeedingFeet(int32 Seed) const;
private:
    UFUNCTION() void OnRep_Food();
    void RefreshFood();
    float RefillElapsed = 0.f;
};

UCLASS(Blueprintable)
class SENIORSENDOFF_API AHouseRoamingPig : public ACharacter
{
    GENERATED_BODY()
public:
    AHouseRoamingPig();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|Meshes") TObjectPtr<UStaticMesh> BodyMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|Meshes") TObjectPtr<UStaticMesh> HeadMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|Meshes") TObjectPtr<UStaticMesh> UpperLegMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|Meshes") TObjectPtr<UStaticMesh> LowerLegMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|Appearance") FString PigName = TEXT("Pepper");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Appearance, Category="Pig|Appearance", meta=(ClampMin="0.4",ClampMax="1.0")) float VisualScale = .82f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Pig|Health") float MaxHealth = 160.f;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Pig|Health") float Health = 100.f;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Pig|AI") EHousePigMood Mood = EHousePigMood::Sniffing;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") int32 PersonalitySeed = 173;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") float WalkSpeed = 110.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") float RunSpeed = 245.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI", meta=(ClampMin="0",ClampMax="1")) float EscapeProbability = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") FName PigstyDoorId = TEXT("HallEnd_West");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") TArray<FHousePigRoamNode> RoamGraph;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") TObjectPtr<AHousePigSnackPile> SnackPile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") FVector BedLocation = FVector(-2825,-920,323.3);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") float BedYaw = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI", meta=(ClampMin="3")) float SleepDuration = 20.f;
    UPROPERTY(ReplicatedUsing=OnRep_Carrier, BlueprintReadOnly, Category="Pig|Interaction") TObjectPtr<APawn> Carrier;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") FVector PigstyMinimum = FVector(-2972,-1371.8425,285);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") FVector PigstyMaximum = FVector(-2349.2328,-765,380);
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") float TravelDistance = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") float GaitDistance = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") int32 Escapes = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") int32 HallNodesVisited = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") int32 SnacksEaten = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") int32 DamageEvents = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") int32 UnsafeFloorStops = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") int32 SleepStarts = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") int32 WakeEvents = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") int32 Pickups = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") int32 Placements = 0;
    UFUNCTION(BlueprintPure, Category="Pig") FVector FeetLocation() const;
    UFUNCTION(BlueprintPure, Category="Pig") bool IsInsidePigsty() const;
    UFUNCTION(BlueprintPure, Category="Pig") bool ShouldShowHealthTo(APlayerController* Viewer, FVector& LabelWorld) const;
    // Also used by the opt-in native harness; follows the same gated, physical path.
    bool RequestRoamNode(int32 Node);
    bool RequestFeeding();
    UFUNCTION(BlueprintCallable, Category="Pig") bool RequestSleep();
    UFUNCTION(BlueprintPure, Category="Pig") bool IsSleepingOnBed() const;
    UFUNCTION(BlueprintPure, Category="Pig") bool IsCarried() const { return IsValid(Carrier); }
    UFUNCTION(BlueprintPure, Category="Pig") FVector GetInteractionLocation() const;
    bool CanPickup(APawn* Player) const;
    bool TryPickup(APawn* Player);
    bool TryPlace(APawn* Player);
    void HealFromSnack();
    FString MovementDebug() const;

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> VisualRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Head;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> UpperLegs;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> LowerLegs;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> WeaponHitBox;
    void RefreshMeshes();
    UFUNCTION() void OnRep_Appearance();
    UFUNCTION() void OnRep_Carrier();
    void UpdateCarry(float DeltaSeconds);
    bool CarryLiftClear(APawn* Player, FVector& LiftTarget) const;
    void ReleaseCarrier();
    void WakeUp();
    bool PlacementClear(const FVector& Feet, const FRotator& Rotation, APawn* Player) const;
    void BuildDefaultGraph();
    void Think();
    void ChooseActivity();
    void FollowPath(float DeltaSeconds);
    void Animate(float DeltaSeconds);
    bool DoorAllows(FName Id) const;
    bool EdgeAllows(int32 From, int32 To) const;
    bool SafeDirection(const FVector& Direction, float ProbeDistance) const;
    bool CanWalkStraight(const FVector& Target) const;
    int32 NearestNode() const;
    FRandomStream Personality;
    TArray<int32> Path;
    int32 PathCursor = 0;
    FVector LocalGoal = FVector::ZeroVector;
    bool bLocalGoal = false, bWasInside = true, bBedIntent = false, bFeedIntent = false;
    bool bBedApproach = false;
    float ThinkAccumulator = 0, ActivityUntil = 0, LastMealAt = -10, StuckSeconds = 0;
    float NoProgressSeconds = 0, BestGoalDistance = BIG_NUMBER;
    FVector ProgressGoal = FVector::ZeroVector;
    float GaitPhase = 0, VisualTime = 0, FleeUntil = 0, VisualCrouch = 0;
    float SleepBlend = 0.f;
    bool bCarryLifting = false;
    FVector CarryLiftTarget = FVector::ZeroVector;
    FVector LastFeet = FVector::ZeroVector, LastProgressFeet = FVector::ZeroVector;
};
