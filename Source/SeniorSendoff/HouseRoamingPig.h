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
class AHouseInteractiveProp;
class UAudioComponent;
class USoundBase;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EHousePigMood : uint8 { Walking, Running, Sniffing, Feeding, Resting, Sleeping, Fleeing, Down, Carried };

UENUM(BlueprintType)
enum class EHousePigCarryPhase : uint8 { None, Reaching, Lifting, Held, Lowering };

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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|Meshes") TObjectPtr<UStaticMesh> LeftEarMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|Meshes") TObjectPtr<UStaticMesh> RightEarMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|Audio") TObjectPtr<USoundBase> SnortSound;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") int32 SnortEvents = 0;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") FName PigstyDoorId = TEXT("BasementLibrary_Mechanical");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") TArray<FHousePigRoamNode> RoamGraph;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") TObjectPtr<AHousePigSnackPile> SnackPile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") FVector BedLocation = FVector(-2815,1000,-308.06);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") float BedYaw = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI", meta=(ClampMin="3")) float SleepDuration = 20.f;
    UPROPERTY(ReplicatedUsing=OnRep_Carrier, BlueprintReadOnly, Category="Pig|Interaction") TObjectPtr<APawn> Carrier;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") FVector PigstyMinimum = FVector(-2900,795,-345);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pig|AI") FVector PigstyMaximum = FVector(-2540,1132,-250);
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
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") int32 SniffStarts = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") int32 SniffCompletes = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") float LastSniffStartedAt = -1.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") float LastSniffCompletedAt = -1.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Pig|Diagnostics") float LastSniffDuration = 0.f;
    UFUNCTION(BlueprintPure, Category="Pig") bool IsGroundSniffing() const
        { return bGroundSniffActive && Mood==EHousePigMood::Sniffing && Health>0.f && !IsCarried(); }
    float GetNextGroundSniffAt() const { return NextSniffAt; }
    UFUNCTION(BlueprintPure, Category="Pig") FVector FeetLocation() const;
    UFUNCTION(BlueprintPure, Category="Pig") bool IsInsidePigsty() const;
    UFUNCTION(BlueprintPure, Category="Pig") bool ShouldShowHealthTo(APlayerController* Viewer, FVector& LabelWorld) const;
    // Also used by the opt-in native harness; follows the same gated, physical path.
    bool RequestRoamNode(int32 Node);
    bool RequestFeeding();
    UFUNCTION(BlueprintCallable, Category="Pig") bool RequestSleep();
    UFUNCTION(BlueprintPure, Category="Pig") bool IsSleepingOnBed() const;
    UFUNCTION(BlueprintPure, Category="Pig") bool IsBedAvailable() const { return bBedAvailable; }
    UFUNCTION(BlueprintPure, Category="Pig") bool IsCarried() const { return IsValid(Carrier); }
    UFUNCTION(BlueprintPure, Category="Pig") float GetCarryBlend() const { return CarryBlend; }
    UFUNCTION(BlueprintPure, Category="Pig") bool IsPlacing() const { return CarryPhase==EHousePigCarryPhase::Lowering; }
    UFUNCTION(BlueprintPure, Category="Pig") EHousePigCarryPhase GetCarryPhase() const { return CarryPhase; }
    UFUNCTION(BlueprintPure, Category="Pig") float GetPickupBend() const;
    UFUNCTION(BlueprintPure, Category="Pig") FVector GetCarryGripPoint() const;
    UFUNCTION(BlueprintPure, Category="Pig") FVector GetCarryGripDirection() const;
    UFUNCTION(BlueprintPure, Category="Pig") FVector GetInteractionLocation() const;
    bool CanPickup(APawn* Player) const;
    bool TryPickup(APawn* Player);
    bool CanTranslateWithCarrier(const FVector& Delta) const;
    void TranslateWithCarrier(const FVector& Delta);
    bool TryPlace(APawn* Player);
    void HealFromSnack();
    FString MovementDebug() const;
    // Runtime motion diagnostics use the real animated component transform.
    FVector GetHoofWorldPosition(int32 Leg) const;
    bool IsHoofPlanted(int32 Leg) const
        { return Leg>=0&&Leg<4&&GaitPlanted[Leg]&&(!bCalderAppearance||SleepBlend<.005f); }
    float GetTrotBlend() const { return GaitTrot; }
    float GetGroundSniffBlend() const { return GaitSniffBlend; }
    float GetSnortPulse() const;
    bool IsSnortAudioPlaying() const;


private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> VisualRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Head;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> LeftEar;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> RightEar;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> SnortAudio;
    UFUNCTION(NetMulticast, Unreliable) void MulticastSnort(float Pitch, float Volume);
    void UpdateSnorts();
    FRandomStream VocalRandom;
    float NextSnortAt = 0.f, LastSnortAt = -100.f;
    bool bWasGroundSniffing = false;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> UpperLegs;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> LowerLegs;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> WeaponHitBox;
    void RefreshMeshes();
    void UpdateCalderSkin();
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CalderSkinMaterial;
    UFUNCTION() void OnRep_Appearance();
    UFUNCTION() void OnRep_Carrier();
    void UpdateCarry(float DeltaSeconds);
    bool MoveCarrySafely(const FVector& Target, const FQuat& Rotation, APawn* Player);
    bool CarryLiftClear(APawn* Player, FVector& LiftTarget) const;
    void ReleaseCarrier();
    void WakeUp();
    void ResolveMovableBed();
    void UpdateMovableBed();
    TWeakObjectPtr<AHouseInteractiveProp> MovableBed;
    FTransform InitialBedTransform = FTransform::Identity;
    FTransform ObservedBedTransform = FTransform::Identity;
    FVector BedLocalSleepPoint = FVector::ZeroVector;
    FVector InitialBedLocation = FVector::ZeroVector;
    FVector BedApproachDirection = FVector::ForwardVector;
    float InitialBedYaw = 180.f, BedTopOffset = 0.f;
    int32 ObservedBedDamageStage = 0;
    bool bHadMovableBed = false, bBedAvailable = true;
    bool PlacementClear(const FVector& Feet, const FRotator& Rotation, APawn* Player) const;
    void BuildDefaultGraph();
    void Think();
    void ChooseActivity();
    void FollowPath(float DeltaSeconds);
    bool CanGroundSniff() const;
    void StartGroundSniff(float Now);
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
    // Curiosity holds the current route; the animal resumes it after inspecting
    // the ground rather than choosing another target every time it stops.
    UPROPERTY(Replicated) bool bGroundSniffActive = false;
    float SniffUntil = 0.f, NextSniffAt = 0.f;
    EHousePigMood ResumeSniffMood = EHousePigMood::Walking;
    FVector TravelSteering = FVector::ZeroVector;
    float TravelTempo = 1.f, TravelTempoTarget = 1.f, NextTempoAt = 0.f;
    float GaitPhase = 0, VisualTime = 0, FleeUntil = 0, VisualCrouch = 0;
    float SleepBlend = 0.f;
    bool GaitInitialized = false;
    bool bCalderAppearance = false;
    float CalderSupportDrop = 0.f;
    TArray<FVector> CalderSoleVertices[4];
    bool GaitPlanted[4] = {true,true,true,true};
    float GaitSpeed = 0.f, GaitMotion = 0.f, GaitTrot = 0.f, GaitTurn = 0.f, GaitTerrainPitch = 0.f;
    float GaitLastYaw = 0.f, GaitGroundAccumulator = 0.f;
    float GaitSniffBlend = 0.f;
    float GaitEarPitch[2] = {0,0}, GaitEarPitchVelocity[2] = {0,0};
    float GaitEarRoll[2] = {0,0}, GaitEarRollVelocity[2] = {0,0};
    int32 GaitGroundCursor = 0;
    float GaitGroundZ[4] = {0,0,0,0};
    float GaitFootPhase[4] = {-1,-1,-1,-1};
    float GaitSwingPhaseAtStart[4] = {0,0,0,0}, GaitSwingProgress[4] = {0,0,0,0};
    bool GaitRecoveryActive[4] = {false,false,false,false};
    float GaitRecoveryAge[4] = {0,0,0,0}, GaitRecoveryDuration[4] = {.1f,.1f,.1f,.1f};
    FVector GaitRecoveryStart[4];
    FVector GaitLastFeet = FVector::ZeroVector;
    FVector GaitFootWorld[4], GaitSwingStart[4], GaitSwingEnd[4];
    FRotator GaitUpperGround[4], GaitLowerGround[4];
    FVector GaitHeadLocation = FVector(60,0,76);
    FRotator GaitHeadRotation = FRotator::ZeroRotator;

    bool bCarryLifting = false;
    FVector CarryLiftTarget = FVector::ZeroVector;
    UPROPERTY(Replicated) EHousePigCarryPhase CarryPhase = EHousePigCarryPhase::None;
    UPROPERTY(Replicated) float CarryBlend = 0.f;
    UPROPERTY(Replicated) float CarryPhaseStarted = 0.f;
    FVector CarryStartPosition = FVector::ZeroVector;
    FVector CarryStartGrip = FVector::ZeroVector;
    FQuat CarryStartRotation = FQuat::Identity;
    FVector CarryPlacementTarget = FVector::ZeroVector;
    FQuat CarryPlacementRotation = FQuat::Identity;
    FVector CarryLastPlayerVelocity = FVector::ZeroVector;
    FVector CarrySway = FVector::ZeroVector;
    FVector CarrySwayVelocity = FVector::ZeroVector;
    float CarryBlockedLogAt = -100.f;
    TWeakObjectPtr<APawn> CarryTickOwner;
    FVector LastFeet = FVector::ZeroVector, LastProgressFeet = FVector::ZeroVector;
};
