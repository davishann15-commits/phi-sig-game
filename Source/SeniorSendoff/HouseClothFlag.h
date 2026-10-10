#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HouseClothFlag.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;

/**
 * Turns a hung flag, banner or pennant into cloth. A coarse verlet cloth grid
 * is simulated (tacks, gravity, air, wall and floor contact, players brushing
 * past, hits) and drives the flag's own render mesh, so the artwork stays exact.
 * Cosmetic and client-side; the owning prop only tells it about hits and tears.
 */
UCLASS(ClassGroup=(SeniorSendoff))
class SENIORSENDOFF_API UHouseClothFlagComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHouseClothFlagComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Copy the source mesh into a deformable mesh and hide the source. False if the mesh cannot be read. */
    bool Initialize(UStaticMeshComponent* SourceBody, bool bHangFromTop);
    bool IsActive() const { return bActive; }
    bool IsReleased() const { return bReleased; }

    /** A hit or a push: velocity (cm/s) applied around a world point. */
    void AddImpulse(const FVector& WorldPoint, const FVector& Direction, float Speed);
    /** Tear it down: tacks pop one by one (nearest the pull last), then it falls and crumples. */
    void Release(const FVector& PullFrom, bool bQuick);

    int32 GetParticleCount() const { return P.Num(); }
    float GetMaxDisplacement() const;
    FBox GetClothBounds() const;

private:
    struct FSectionData
    {
        TArray<FVector> RestWorld;
        TArray<FVector2D> Grid;      // (u, v) in [0,1], v downward
        TArray<FVector> RestNormals;
        TArray<FVector2D> UV0;
        TArray<int32> Triangles;
        TArray<FVector> Positions, Normals;
    };
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Cloth;
    TWeakObjectPtr<UStaticMeshComponent> Source;
    TArray<FSectionData> Sections;
    struct FLink { int32 A = 0, B = 0; float Rest = 0.f, Stiffness = 1.f; };
    TArray<FVector> P, Prev, Rest;
    TArray<float> InvMass;
    TArray<FLink> Links;
    TArray<int32> Pins, PendingPins;
    int32 NU = 0, NV = 0;
    FVector Origin = FVector::ZeroVector, Across = FVector::ForwardVector, Down = -FVector::UpVector, Out = FVector::RightVector;
    float Width = 100.f, Height = 100.f, FloorZ = -100000.f, Time = 0.f, Accumulator = 0.f, SleepTimer = 0.f, PinTimer = 0.f;
    bool bActive = false, bReleased = false, bAwake = true, bWall = false;

    int32 Index(int32 I, int32 J) const { return J * NU + I; }
    void Step(float Dt);
    void UpdateMesh();
    void Wake() { bAwake = true; SleepTimer = 0.f; SetComponentTickEnabled(true); }
};
