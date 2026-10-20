// Blue-hour dusk for the exterior: the existing sun below the horizon behind the house, an outside-only film look
// (soft grain, faded teal shadows, warm highlights) that fades out as the camera goes indoors, the grill's smoke,
// and the road decals. Adds no lights.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "HouseStreetScene.h"
#include "HouseDuskAtmosphere.generated.h"

class UHouseBlackHallSubsystem;
class UPostProcessComponent;
class UProceduralMeshComponent;
class UDecalComponent;

UCLASS()
class SENIORSENDOFF_API UHouseDuskAtmosphereComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHouseDuskAtmosphereComponent();
    void Build(UHouseBlackHallSubsystem* Look, const FVector& SmokeOrigin);
    virtual void TickComponent(float Dt, ELevelTick Type, FActorComponentTickFunction* Function) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    /** How far (cm) a point is outside the house's three blocks; 0 inside. */
    static float DistanceOutsideHouse(const FVector& P);
private:
    UPROPERTY(Transient) TObjectPtr<UPostProcessComponent> FilmLook;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Smoke;
    struct FPuff { FVector P = FVector::ZeroVector, V = FVector::ZeroVector; float Age = 0, Life = 1, Size = 20, Spin = 0, SpinRate = 0; bool bLive = false; };
    TArray<FPuff> Puffs;
    FVector Origin = FVector::ZeroVector;
    float Clock = 0, Spawn = 0, Weight = -1;
    float BaseExposure = -4.15f, WoodlandDepth = 0.f;
    bool bWoodlandFogLease = false;
    uint32 Seed = 1;
    TArray<FVector> Pos, Nrm;
    TArray<FVector2D> UV;
    TArray<FLinearColor> Col;
    TArray<FProcMeshTangent> Tan;
    float Rand();
    void SimSmoke(float Dt);
    void WriteSmoke(const FVector& Cam, const FRotator& CamRot);
};

namespace HouseDusk
{
/** Sun below the horizon behind the house (blue hour), recaptured sky, dusk fog. Returns a note for the build log. */
FString ApplyLighting(UWorld* World);
/** Projects the road decals; returns how many were made. */
int32 SpawnDecals(AActor* Owner, UHouseBlackHallSubsystem* Look, const TArray<HouseStreetScene::FDecal>& Decals, TArray<TObjectPtr<UDecalComponent>>& Out);
}
