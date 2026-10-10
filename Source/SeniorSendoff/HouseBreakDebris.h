#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HouseBreakDebris.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UPrimitiveComponent;

/** What kind of damage moment a debris burst represents. */
enum class EHouseDebrisKind : uint8
{
    Hit,    // a knock that did not change the damage stage: a few chips
    Crack,  // the piece moved to its next damage stage: chips and splinters
    Break   // the piece broke: a full burst of chunks, splinters and shards
};

/**
 * Cosmetic, client-side break debris. Pieces are instanced meshes simulated
 * with simple ballistic motion, bounces, sliding and settling, then fade out.
 * No rigid bodies, no network traffic beyond the trigger, no gameplay effect.
 */
UCLASS(ClassGroup=(SeniorSendoff))
class SENIORSENDOFF_API UHouseBreakDebrisComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHouseBreakDebrisComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /**
     * Throw debris from a piece.
     * @param Bounds      world bounds of the piece at the moment of impact
     * @param Impact      world impact point
     * @param Direction   direction the blow travelled (into the piece)
     * @param Surface     outer material (paint, wood, fabric)
     * @param Inner       exposed material (splinters, foam); may be null
     * @param Glass       glass/screen material for TVs and frames; null = none
     */
    void Burst(EHouseDebrisKind Kind, const FBox& Bounds, const FVector& Impact, const FVector& Direction,
        UMaterialInterface* Surface, UMaterialInterface* Inner, UMaterialInterface* Glass, float MassKg,
        UMaterialInterface* SurfaceAlt = nullptr);

    /**
     * Animate a stand-in copy of the piece while the real body is hidden:
     * a collapse (buckle down into the broken shape) or a crack shudder.
     * The real body is shown again when the animation ends.
     */
    void PlayGhost(UStaticMeshComponent* Ghost, UPrimitiveComponent* HiddenBody, bool bCollapse, const FVector& Direction, float Duration);
    bool IsGhostPlaying() const { return Ghost != nullptr; }

    int32 GetLivePieceCount() const;
    int32 GetSpawnedPieceCount() const { return SpawnedTotal; }

private:
    enum EGroup : uint8 { SurfaceChunk, SurfaceWedge, InnerChunk, InnerSplinter, GlassShard, GroupCount };
    struct FPiece
    {
        FVector Position = FVector::ZeroVector, Velocity = FVector::ZeroVector, Scale = FVector::OneVector;
        FQuat Rotation = FQuat::Identity, RestRotation = FQuat::Identity;
        FVector SpinAxis = FVector::UpVector;
        float SpinSpeed = 0.f, Age = 0.f, Life = 8.f, RestBlend = 0.f, Drag = .15f, Gravity = 980.f;
        int32 Instance = INDEX_NONE;
        TWeakObjectPtr<UInstancedStaticMeshComponent> Component;
        uint8 Group = SurfaceChunk, Bounces = 0;
        bool bSliding = false, bResting = false, bDead = false;
    };
    UInstancedStaticMeshComponent* GroupComponent(EGroup Group, UMaterialInterface* Material);
    void AddPiece(EGroup Group, UMaterialInterface* Material, const FVector& Position, const FVector& Velocity,
        const FVector& SizeCm, FRandomStream& Random, float Life, bool bSoft = false);
    void ClearAll();

    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Groups;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Ghost;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> AltMaterial;
    TWeakObjectPtr<UPrimitiveComponent> GhostHidden;
    FTransform GhostStart;
    FVector GhostPivot = FVector::ZeroVector, GhostDirection = FVector::ForwardVector;
    float GhostAge = 0.f, GhostDuration = .15f;
    bool bGhostCollapse = false;
    void FinishGhost();
    void UpdateGhost(float DeltaTime);
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> GroupMaterials;
    TArray<FPiece> Pieces;
    FVector MeshSize[GroupCount];
    int32 SpawnedTotal = 0;
    int32 BurstSeed = 0;
};
