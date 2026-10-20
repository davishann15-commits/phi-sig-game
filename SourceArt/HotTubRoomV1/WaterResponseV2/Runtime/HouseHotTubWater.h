#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "HouseHotTubWater.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UPrimitiveComponent;
class ACharacter;

// A bounded damped wave surface, not a full volume fluid solver. The solid tub,
// seats and stairs use independent static collision; this surface never blocks
// the player. Disturbances propagate through the grid instead of looping a
// canned ripple animation. The actor origin is the still-water surface centre.
UCLASS(Blueprintable)
class SENIORSENDOFF_API AHouseHotTubWater : public AActor
{
    GENERATED_BODY()
public:
    AHouseHotTubWater();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spa") TObjectPtr<UProceduralMeshComponent> WaterSurface;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spa") TObjectPtr<UInstancedStaticMeshComponent> SteamCards;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spa") TObjectPtr<UInstancedStaticMeshComponent> SplashDroplets;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spa") TObjectPtr<UInstancedStaticMeshComponent> SplashFoam;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Geometry", meta=(ClampMin="80")) FVector2D WaterSize = FVector2D(295, 250);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Geometry", meta=(ClampMin="0")) float CornerRadius = 22.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Geometry", meta=(ClampMin="15", ClampMax="150")) float WaterDepth = 49.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Geometry", meta=(ClampMin="17", ClampMax="65")) int32 GridResolution = 41;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Appearance") TObjectPtr<UMaterialInterface> WaterMaterial;
    // Unlit translucent, two sided, soft UV cloud mask. PerInstanceCustomData(0)
    // through a VertexInterpolator supplies the per-card lifetime opacity.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Appearance") TObjectPtr<UMaterialInterface> SteamMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Appearance") TObjectPtr<UMaterialInterface> SplashMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Appearance") TObjectPtr<UMaterialInterface> FoamMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Appearance") TObjectPtr<UStaticMesh> SteamCardMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Steam", meta=(ClampMin="0", ClampMax="32")) int32 SteamCount = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Steam", meta=(ClampMin="0", ClampMax="0.25")) float SteamOpacity = .07f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Steam", meta=(ClampMin="20", ClampMax="140")) float SteamRiseHeight = 95.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Waves", meta=(ClampMin="15", ClampMax="90")) float WaveSpeed = 55.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Waves", meta=(ClampMin="0.2", ClampMax="4")) float WaveDamping = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Waves", meta=(ClampMin="0.5", ClampMax="6")) float MaximumWaveHeight = 3.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Spa|Waves") bool bJetsRunning = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Interaction") bool bAffectCharacters = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spa|Interaction") bool bAffectPhysicsBodies = true;

    UFUNCTION(CallInEditor, BlueprintCallable, Category="Spa") void RefreshWater();

    // Server-side API; clients receive the same localized impulses. Strength is
    // a signed vertical velocity change in cm/s, not a height offset.
    UFUNCTION(BlueprintCallable, Category="Spa|Waves") void AddWaterImpulse(FVector WorldPosition, float Strength = 45.f, float Radius = 20.f);
    // Visible impact spray and a displaced-volume impulse. Geometry is reused
    // from two fixed pools, so repeated entries never create new actors.
    UFUNCTION(BlueprintCallable, Category="Spa|Waves") void AddWaterSplash(FVector WorldPosition, float Strength = 65.f, float Radius = 24.f, FVector WorldVelocity = FVector::ZeroVector);
    UFUNCTION(BlueprintPure, Category="Spa|Waves") float GetWaveEnergy() const;
    UFUNCTION(BlueprintPure, Category="Spa|Waves") float GetMaximumDisplacement() const;
    UFUNCTION(BlueprintPure, Category="Spa|Waves") float GetDisplacementAtWorld(FVector WorldPosition) const;
    UFUNCTION(BlueprintPure, Category="Spa|Interaction") bool IsPointInsideWater(FVector WorldPosition) const;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Spa|Diagnostics") int32 ReceivedImpulseCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Spa|Diagnostics") int32 PawnEntryImpulseCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Spa|Diagnostics") int32 PawnMotionImpulseCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Spa|Diagnostics") int32 PawnExitImpulseCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Spa|Diagnostics") int32 SplashBurstCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Spa|Diagnostics") int32 ActiveSplashParticleCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Spa|Diagnostics") int32 ActiveFoamParticleCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Spa|Diagnostics") int32 PhysicsImpactImpulseCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Spa|Diagnostics") int32 ThrowableImpulseCount = 0;

private:
    UFUNCTION(NetMulticast, Unreliable) void MulticastWaterImpulse(FVector_NetQuantize Position, float Strength, float Radius);
    UFUNCTION(NetMulticast, Unreliable) void MulticastWaterSplash(FVector_NetQuantize Position, float Strength, float Radius, FVector_NetQuantize Velocity);
    void RebuildWater();
    void RebuildSteam();
    void RebuildSplash();
    void ApplyLocalImpulse(FVector WorldPosition, float Strength, float Radius);
    void StepWaves(float Step);
    void UpdateWaterMesh();
    void UpdateSteam(float DeltaSeconds);
    void EmitSplash(FVector WorldPosition, float Strength, float Radius, FVector WorldVelocity);
    void UpdateSplash(float DeltaSeconds);
    float SampleHeight(const FVector& Local) const;
    void TickCharacters(float DeltaSeconds);
    void TickPhysicsBodies(float DeltaSeconds);
    void TickThrowables(float DeltaSeconds);
    void TickSmoke(float DeltaSeconds);
    bool InsideXY(const FVector& Local, float Inset = 0.f) const;
    bool SweepWaterCrossing(const FVector& PreviousWorld, const FVector& CurrentWorld, FVector& SurfaceWorld) const;
    void RestoreSmokePawn();

    TArray<float> Heights, Speeds, Accelerations, FoamLevels;
    TArray<uint8> ActiveCells;
    TArray<FVector> MeshVertices, MeshNormals;
    TArray<FVector2D> MeshUVs;
    TArray<FColor> MeshColors;
    TArray<int32> MeshTriangles;
    float CellWidth = 1.f, CellDepth = 1.f;
    float SimulationAccumulator = 0.f, RenderAccumulator = 0.f, JetAccumulator = 0.f;
    int32 JetStep = 0;
    struct FPawnWaterState
    {
        FVector PreviousFeet = FVector::ZeroVector;
        FVector LastRippleFeet = FVector::ZeroVector;
        float LastRippleAt = -100.f, LastSplashAt = -100.f;
        int32 Step = 0;
        bool bInside = false, bSeen = false;
    };
    struct FBodyWaterState { FVector PreviousLocal = FVector::ZeroVector; bool bSeen = false; float LastSeenAt = 0.f; };
    struct FSteamState { FVector2D Start; float Phase = 0, Duration = 7, Sway = 0, Size = 1; };
    TMap<TWeakObjectPtr<ACharacter>, FPawnWaterState> PawnStates;
    TMap<TWeakObjectPtr<UPrimitiveComponent>, FBodyWaterState> BodyStates;
    TMap<TWeakObjectPtr<AActor>, FVector> ThrowablePrevious;
    TArray<FSteamState> SteamStates;
    struct FSplashState
    {
        FVector Position = FVector::ZeroVector, Velocity = FVector::ZeroVector;
        float Age = 0.f, Lifetime = 0.f, Size = 1.f, Yaw = 0.f;
        bool bActive = false;
    };
    // One instanced draw for spray and one for foam. These capacities are fixed
    // independently of the number of splashes or time spent in the tub.
    static constexpr int32 DropletPoolSize = 96;
    static constexpr int32 FoamPoolSize = 64;
    TArray<FSplashState> DropletStates, FoamStates;
    int32 NextDroplet = 0, NextFoam = 0;

#if !UE_BUILD_SHIPPING
    int32 SmokePhase = 0;
    double SmokeStarted = 0;
    float SmokeInitialEnergy = 0;
    int32 SmokeInitialPawnEntries = 0, SmokeInitialMotion = 0, SmokeInitialPhysics = 0, SmokeInitialThrowables = 0;
    bool bSmokeSavedJets = true;
    TWeakObjectPtr<ACharacter> SmokePawn;
    TWeakObjectPtr<AActor> SmokeBody;
    FTransform SmokePawnTransform;
    FRotator SmokeControlRotation;
    FVector SmokePawnVelocity;
    float SmokePawnGravity = 0, SmokePawnMaxSpeed = 0;
    uint8 SmokePawnMovementMode = 0, SmokePawnCustomMode = 0;
#endif
};
