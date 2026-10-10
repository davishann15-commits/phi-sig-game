#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseInteractiveProp.h"
#include "HouseGadgets.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "HouseDetail.generated.h"

class UHouseBlackHallSubsystem;
class AHouseExteriorDetail;
class UProceduralMeshComponent;
class UStaticMesh;
class ULightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
struct FHallMesh;

/** Runtime-built furniture kinds (same mesh generated on every machine; only the kind replicates). */
UENUM()
enum class EHouseKitProp : uint8
{
    Recliner, GreenChair, Bench, WickerBasket, TeaJug, Pillow, BlueBag, WoodBoard, HandVacuum, DoorMat, BucketLid, CrumpledShirt, Count
};

/** A house furnishing whose mesh is generated at runtime from the hallway kit. */
UCLASS(NotPlaceable)
class SENIORSENDOFF_API AHouseKitProp : public AHouseInteractiveProp
{
    GENERATED_BODY()
public:
    AHouseKitProp();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    /** Assigns the kit mesh and the prop tuning for Kind (server before FinishSpawning, clients in BeginPlay). */
    void ApplyKit();
    UPROPERTY(Replicated) uint8 KitKind = 0;
    UPROPERTY(Replicated) uint8 KitVariant = 0;
};

/** Hallway fixtures, signs, conduit, wall damage and clutter for the photographed corridors. Spawned locally on every machine. */
UCLASS(NotPlaceable, Transient)
class SENIORSENDOFF_API AHouseHallwayDetail : public AActor
{
    GENERATED_BODY()
public:
    AHouseHallwayDetail();
    void Build(UHouseBlackHallSubsystem* Look);
    int32 Placed = 0, Skipped = 0, Triangles = 0;
    FString Notes;
    /** Interactive wall gadgets the dressing wants spawned (server spawns replicated actors from these). */
    TArray<FHouseGadgetSpec> GadgetSpecs;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
    UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Extras;
};

/** House-wide detail pass: hallway dressing, kit furniture, focus highlight and rendering tweaks for Chapter01_House. */
UCLASS()
class SENIORSENDOFF_API UHouseDetailSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    /** Kit meshes by kind (built lazily, identical on every machine). */
    UStaticMesh* KitMesh(EHouseKitProp Kind, uint8 Variant = 0);
    UHouseBlackHallSubsystem* Look() const;
    AHouseHallwayDetail* GetHallway() const { return Hallway.Get(); }
    AHouseExteriorDetail* GetExterior() const { return Exterior.Get(); }
    const TArray<TWeakObjectPtr<AHouseKitProp>>& GetKitProps() const { return KitProps; }
    int32 CulledLights = 0;
    /** Authority: every horn/strobe flashes and sounds for Seconds, then the pulled stations reset. */
    void TriggerFireAlarm(float Seconds);
    const TArray<TWeakObjectPtr<AHouseGadget>>& GetGadgets() const { return Gadgets; }

private:
    void ApplyRenderingTweaks(UWorld& World);
    void FixCheckpointLabels(UWorld& World);
    void SpawnKitProps(UWorld& World);
    void SpawnGadgets(UWorld& World);
    bool TickHighlight(float DeltaTime);
    UPROPERTY(Transient) TMap<uint8, TObjectPtr<UStaticMesh>> KitMeshes;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> HighlightMaterial;
    TWeakObjectPtr<UPrimitiveComponent> Highlighted;
    TWeakObjectPtr<AHouseHallwayDetail> Hallway;
    TWeakObjectPtr<AHouseExteriorDetail> Exterior;
    TArray<TWeakObjectPtr<AHouseKitProp>> KitProps;
    TArray<TWeakObjectPtr<AHouseGadget>> Gadgets;
    FTimerHandle AlarmTimer;
    FTSTicker::FDelegateHandle TickerHandle;
    float HighlightClock = 0.f;
    bool bLabelsFixed = false;
    // Shadow culling: house lights that cast shadows, toggled off while far from the camera.
    TArray<TWeakObjectPtr<ULightComponent>> ShadowLights;
    float ShadowCullClock = 0.f;
    void TickShadowCull(UWorld& World, float DeltaTime);
    // Floor culling: lights on another floor that the camera cannot see fade out and leave the scene.
    struct FFloorLight { TWeakObjectPtr<ULightComponent> Light; float Base = 0.f; float Fade = 1.f; int32 Blocked = 0; bool bWantOn = true; };
    TArray<FFloorLight> FloorLights;
    int32 FloorCursor = 0;
    float FloorClock = 0.f;
    void TickFloorCull(UWorld& World, float DeltaTime);
public:
    int32 FloorCulledNow() const;
};

/** Kit look (textures / materials) registered on the black-hall subsystem under "HW_*" keys. */
void HwCreateLook(UHouseBlackHallSubsystem* Sub);
/** Shared kit primitives. */
void HwFrustum(FHallMesh& M, FName Key, const FVector& Base, float R0, float R1, float H, int32 Seg, bool bInside, float V0 = 0.f, float V1 = 1.f);
void HwDisc(FHallMesh& M, FName Key, const FVector& C, float R, int32 Seg, bool bUp);
void HwSuperBox(FHallMesh& M, FName Key, const FTransform& T, const FVector& Half, float E1, float E2, int32 SegU, int32 SegV, FVector2D Tile = FVector2D(2.f, 1.f));
/** Geometry for one kit prop into Mesh (local space, origin on the floor at the footprint centre). Returns collision boxes (centre, half-extent, yaw). */
void HwBuildKitProp(EHouseKitProp Kind, uint8 Variant, FHallMesh& Mesh, TArray<FBox>& OutCollision);
/** Drywall damage decal material matching a wall colour (cached per kind / seed / colour). */
UMaterialInterface* HwDamageMaterial(UHouseBlackHallSubsystem* Sub, const FLinearColor& Wall, float Rough, int32 Kind, int32 Seed);
/** Builds a UStaticMesh from accumulated hall geometry at runtime (fast build, box collision). */
UStaticMesh* HwBuildStaticMesh(const FHallMesh& Mesh, const TCHAR* Name, UHouseBlackHallSubsystem* Look, const TArray<FBox>& Collision);
