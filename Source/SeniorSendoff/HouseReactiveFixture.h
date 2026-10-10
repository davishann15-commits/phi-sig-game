#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseReactiveFixture.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class AHouseReactiveFixture;

DECLARE_MULTICAST_DELEGATE_OneParam(FHouseFixtureBroken, AHouseReactiveFixture*);

UENUM(BlueprintType)
enum class EHouseReactiveFixtureKind : uint8
{
    Curtain,
    Chandelier,
    Window
};

USTRUCT()
struct FHouseFixtureBreakState
{
    GENERATED_BODY()
    UPROPERTY() bool bBroken = false;
    UPROPERTY() float AccumulatedDamage = 0.f;
    UPROPERTY() FVector FirstImpactLocal = FVector::ZeroVector;
    UPROPERTY() FVector ImpactLocal = FVector::ZeroVector;
    UPROPERTY() FVector DirectionLocal = FVector::ForwardVector;
};

/** Keep each pendulum snapshot coherent when it arrives over the network. */
USTRUCT()
struct FHouseFixtureSwingState
{
    GENERATED_BODY()
    UPROPERTY() FVector2D Angles = FVector2D::ZeroVector;
    UPROPERTY() FVector2D Velocity = FVector2D::ZeroVector;
    UPROPERTY() float ServerTime = 0.f;
};

/** The original movable cloth, hanging light or glass, extracted without its fixed trim. */
UCLASS()
class SENIORSENDOFF_API AHouseReactiveFixture : public AActor
{
    GENERATED_BODY()
public:
    AHouseReactiveFixture();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Configuration, Category="House Fixture")
    EHouseReactiveFixtureKind FixtureKind = EHouseReactiveFixtureKind::Curtain;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Configuration, Category="House Fixture")
    FName FixtureId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Configuration, Category="House Fixture")
    TObjectPtr<UStaticMesh> OriginalMesh;
    /** Preserve the exact materials overridden on the original placed mesh. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Configuration, Category="House Fixture")
    TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;
    /** Window grille extracted from the fixed wall, sharing the pane's local origin. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Configuration, Category="House Fixture|Window")
    TObjectPtr<UStaticMesh> BreakawayFrameMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Configuration, Category="House Fixture|Window")
    TArray<TObjectPtr<UMaterialInterface>> BreakawayFrameMaterials;
    /** Geometry transform before animation, in actor coordinates. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Configuration, Category="House Fixture")
    FTransform MeshTransform = FTransform::Identity;
    /** Curtain's upper anchored edge or chandelier's ceiling attachment, in actor coordinates. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Configuration, Category="House Fixture")
    FVector MotionPivotLocal = FVector::ZeroVector;
    /** 0 = local X, 1 = local Y. The cloth gathers about MotionPivotLocal. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="House Fixture|Curtain", meta=(ClampMin="0",ClampMax="1"))
    int32 CurtainAxis = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="House Fixture|Curtain", meta=(ClampMin="0.08",ClampMax="0.95"))
    float CurtainGatheredScale = .14f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="House Fixture|Curtain", meta=(ClampMin="0.3",ClampMax="3"))
    float CurtainTravelSeconds = 1.1f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="House Fixture|Curtain")
    bool bInitiallyOpen = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="House Fixture|Chandelier", meta=(ClampMin="3",ClampMax="22"))
    float MaximumSwingDegrees = 13.f;
    /** Total damage needed to shatter. Weapon point hits apply at most 8 each. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Configuration, Category="House Fixture|Window", meta=(ClampMin="1"))
    float GlassBreakingDamage = 24.f;

    UFUNCTION(BlueprintCallable, Category="House Fixture")
    bool Interact(APawn* Player);
    UFUNCTION(BlueprintCallable, CallInEditor, Category="House Fixture")
    void RefreshFixture();
    UFUNCTION(BlueprintPure, Category="House Fixture")
    FString FocusLabel() const;
    UFUNCTION(BlueprintPure, Category="House Fixture")
    bool IsOpen() const { return bCurtainOpen; }
    UFUNCTION(BlueprintPure, Category="House Fixture")
    bool IsBroken() const { return BreakState.bBroken; }
    /** Shared by point damage and physical furniture hits. Authority only. */
    void ApplyImpact(const FVector& WorldPoint, const FVector& WorldDirection, float Strength,
        AController* Instigator = nullptr);
    UStaticMeshComponent* GetFixtureMesh() const { return Body; }
    UBoxComponent* GetFixtureCollision() const { return HitVolume; }
    UStaticMeshComponent* GetBreakawayFrameComponent() const { return BreakawayFrame; }
    FVector2D GetSwingAngles() const { return SwingAngles; }
    FVector2D GetRenderedSwingAngles() const { return RenderedSwingAngles; }
    float GetCurtainProgress() const { return CurtainProgress; }
    float GetCurtainVelocity() const { return CurtainVelocity; }
    int32 GetShardCount() const;
    int32 GetRestingShardCount() const;
    int32 GetSupportedShardCount() const;
    int32 GetCrackCount() const;
    int32 GetGlassCrackSoundCount() const { return GlassCrackSoundCount; }
    int32 GetGlassShatterSoundCount() const { return GlassShatterSoundCount; }
    float GetGlassDamage() const { return BreakState.AccumulatedDamage; }
    int32 GetImpactCount() const { return ImpactCount; }
    /** Owner doors update their frame collision once when their glass breaks. */
    FHouseFixtureBroken OnFixtureBroken;

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> MotionPivot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BreakawayFrame;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> HitVolume;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> GlassShards;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> GlassEdgeGlints;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> GlassCracks;
    UPROPERTY(ReplicatedUsing=OnRep_CurtainOpen) bool bCurtainOpen = false;
    UPROPERTY(ReplicatedUsing=OnRep_Swing) FHouseFixtureSwingState ReplicatedSwing;
    UPROPERTY(ReplicatedUsing=OnRep_Broken) FHouseFixtureBreakState BreakState;
    UFUNCTION() void OnRep_CurtainOpen();
    UFUNCTION() void OnRep_Swing();
    UFUNCTION() void OnRep_Broken();
    UFUNCTION() void OnRep_Configuration();
    UFUNCTION() void OnFixtureHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

    void ConfigureGeometry();
    void UpdateCurtainPose();
    void PublishSwingState();
    void BreakGlass(const FVector& Point, const FVector& Direction, AController* Instigator);
    void StartShards();
    void UpdateShards(float DeltaSeconds);
    void UpdateGlassCracks();
    bool CanReach(const APawn* Player) const;

    struct FShard
    {
        FVector Position = FVector::ZeroVector;
        FVector Velocity = FVector::ZeroVector;
        FVector Scale = FVector::OneVector;
        FRotator Rotation = FRotator::ZeroRotator;
        FRotator Spin = FRotator::ZeroRotator;
        float ReleaseTime = 0.f;
        bool bResting = false;
        bool bGlintPlaced = false;
        uint8 FloorBounces = 0;
    };
    TArray<FShard> Shards;
    FVector2D SwingAngles = FVector2D::ZeroVector;
    FVector2D SwingVelocity = FVector2D::ZeroVector;
    FVector2D RenderedSwingAngles = FVector2D::ZeroVector;
    FVector2D RenderedSwingVelocity = FVector2D::ZeroVector;
    float CurtainProgress = 0.f;
    float CurtainVelocity = 0.f;
    FVector2D CurtainSway = FVector2D::ZeroVector;
    FVector2D CurtainSwayVelocity = FVector2D::ZeroVector;
    float ShardAge = 0.f;
    float LastAudibleGlassDamage = 0.f;
    float LastPhysicsImpactTime = -10.f;
    int32 ImpactCount = 0;
    int32 GlassCrackSoundCount = 0;
    int32 GlassShatterSoundCount = 0;
    bool bShardsStarted = false;
    bool bBreakNotified = false;
    bool bGlassShatterSoundPlayed = false;
};
