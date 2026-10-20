#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SSOCarrySize.h"
#include "Engine/NetSerialization.h"
#include "HouseSfx.h"
#include "HouseInteractiveProp.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class AController;
class UHouseBreakDebrisComponent;
class UHouseClothFlagComponent;

/** A placed house furnishing, with one persistent rigid body and a broken variant. */
UCLASS(Blueprintable)
class SENIORSENDOFF_API AHouseInteractiveProp : public AActor
{
    GENERATED_BODY()
public:
    AHouseInteractiveProp();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House Prop") TObjectPtr<UStaticMeshComponent> Body;
    /** Cosmetic chips, splinters and shards thrown when the piece is hit, cracks or breaks. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House Prop") TObjectPtr<UHouseBreakDebrisComponent> BreakDebris;
    /** Cloth simulation for flags, banners and pennants (ripples, flaps, tears down). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House Prop") TObjectPtr<UHouseClothFlagComponent> ClothFlag;
    /** Force cloth behavior on this piece (flags are detected automatically). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Interaction") bool bClothFlag = false;
    UFUNCTION(BlueprintPure, Category="House Prop") bool IsClothFlag() const { return bClothMode; }
    /** Force caster/rolling-chair behavior (office chairs are detected automatically). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Interaction") bool bRollingChair = false;
    /** Office chairs on casters: roll away when bumped, swivel, stay upright. */
    UFUNCTION(BlueprintPure, Category="House Prop") bool IsRollingChair() const;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop") FName PropId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop") FString PropName = TEXT("furniture");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Appearance") TObjectPtr<UStaticMesh> IntactMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Appearance") TObjectPtr<UStaticMesh> BrokenMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Appearance") TObjectPtr<UStaticMesh> DamagedMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Appearance") TObjectPtr<UStaticMesh> HeavilyDamagedMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Appearance") TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Interaction") bool bMovable = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Interaction") bool bBreakable = true;
    // The first hit or pull unhooks the original frame; another hit can break it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Interaction") bool bWallMounted = false;
    // Fixed fixtures can retain independent support/collision after breakage.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Interaction") bool bBrokenVersionMovable = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Interaction", meta=(ClampMin="0.1")) float BreakHealth = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Interaction", meta=(ClampMin="0.1")) float DetachHealth = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Interaction", meta=(ClampMin="0.1", ClampMax="300")) float MassKg = 15.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Interaction", meta=(ClampMin="100")) float ImpactBreakSpeed = 540.f;
    // How players handle this piece: Small = one hand, Medium = two hands, Large = lift one side and drag.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Interaction") ESSOCarrySize CarrySize = ESSOCarrySize::Auto;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Prop|Interaction") FName DamageCategory = TEXT("Furniture");

    UPROPERTY(ReplicatedUsing=OnRep_PropState, BlueprintReadOnly, Category="House Prop|State") bool bBroken = false;
    UPROPERTY(ReplicatedUsing=OnRep_PropState, BlueprintReadOnly, Category="House Prop|State") int32 DamageStage = 0;
    UPROPERTY(ReplicatedUsing=OnRep_PropState, BlueprintReadOnly, Category="House Prop|State") bool bDetachedFromWall = false;
    UPROPERTY(ReplicatedUsing=OnRep_PropState, BlueprintReadOnly, Category="House Prop|State") bool bPhysicsActive = false;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="House Prop|State") float Health = 8.f;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="House Prop|State") float AttachmentHealth = 8.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="House Prop|Diagnostics") int32 DamageEvents = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="House Prop|Diagnostics") int32 BreakEvents = 0;

    UFUNCTION(CallInEditor, BlueprintCallable, Category="House Prop") void RefreshProp();
    UFUNCTION(BlueprintCallable, Category="House Prop") bool WakeForInteraction(AActor* Interactor = nullptr);
    UFUNCTION(BlueprintPure, Category="House Prop") FString GetInteractionLabel() const;
    UFUNCTION(BlueprintPure, Category="House Prop") bool CanBeMoved() const;
    UFUNCTION(BlueprintPure, Category="House Prop") bool IsDetached() const { return bDetachedFromWall; }

private:
    /** Plays debris and camera jolt on every machine. Kind: 0 hit, 1 crack (stage change), 2 break. */
    UFUNCTION(NetMulticast, Unreliable) void MulticastDamageFx(FVector_NetQuantize Impact, FVector_NetQuantizeNormal Direction, uint8 Kind);
    void PlayDamageFx(const FVector& Impact, const FVector& Direction, uint8 Kind);
    /** Plays a knock on every machine for a bump too light to damage anything. Strength 1..255. */
    UFUNCTION(NetMulticast, Unreliable) void MulticastBumpFx(FVector_NetQuantize At, uint8 Strength);
    float LastBumpSoundTime = -100.f;
    UMaterialInterface* DebrisSurfaceMaterial(int32 Rank = 0) const;
    UMaterialInterface* DebrisInnerMaterial(bool bBreak) const;
    UMaterialInterface* DebrisGlassMaterial() const;
    float LastHitFxTime = -100.f;
public:
    /** What this piece sounds like (from its name and materials). */
    EHouseSurface SfxSurface = EHouseSurface::Wood;
private:
    bool bClothMode = false;
    UPROPERTY(Transient) TObjectPtr<class UPhysicalMaterial> CasterMaterial;
    bool bCasterUprightLock = false;
    void SetCasterUprightLock(bool bLock);
    bool ShouldUseCloth() const;
    void TearCloth(const FVector& PullFrom, AController* Instigator, bool bCountAsBroken);
    UFUNCTION(NetMulticast, Reliable) void MulticastClothRelease(FVector_NetQuantize PullFrom);
    UFUNCTION() void OnRep_PropState();
    UFUNCTION() void OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
    UFUNCTION() void OnBodyWake(UPrimitiveComponent* WakingComponent, FName BoneName);
    UFUNCTION() void OnBodySleep(UPrimitiveComponent* SleepingComponent, FName BoneName);
    bool HasFreshIncomingVelocity() const;
    void ClearIncomingVelocity();
    void ApplyAppearance();
    UStaticMesh* MeshForDamageStage(int32 Stage) const;
    void UpdateDamageStage(AController* Instigator);
    void ApplyPhysicsState();
    void RestoreReleasedMotion(const FVector& Velocity, const FVector& AngularVelocity);
    void BreakProp(const FVector& HitLocation, const FVector& HitDirection, AController* Instigator);
    void ReleaseBodyHolders();
    void DetachFromWall(const FVector& AwayFromWall, AController* Instigator);
    void WakeSupportedProps(AActor* Interactor);
    bool bSuppressSupportedWakeForHit = false;
    TWeakObjectPtr<AController> LastInteractor;
    bool bApplyingContactDamage = false;
    float LastImpactTime = -100.f;
    float LastImpactDiagnosticTime = -100.f;
    FVector IncomingVelocity = FVector::ZeroVector;
    uint64 IncomingVelocityFrame = 0;
    bool bIncomingVelocityValid = false;
    bool bIncomingVelocityWasHeld = false;
    FVector OriginalScale = FVector::OneVector;
protected:
    /** The intact scale a crushed (broken, no broken mesh) piece is squashed from. Runtime-spawned subclasses whose scale
     *  is part of what they are (ACampusProp) set it before begin play. */
    void SetOriginalScale(const FVector& Scale) { OriginalScale = Scale; }
};
