#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "ExistingHouseDoor.h"
#include "HouseBlackHall.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UBoxComponent;
class AStoryFirstPersonCharacter;
class UPrimitiveComponent;
struct FHallImage;

/** Door leaves added to the basement black hall (French patio pairs, steel exit, equipment closet). */
UENUM()
enum class EHallDoorStyle : uint8
{
    French,
    Exit,
    Equipment,
};

UCLASS()
class SENIORSENDOFF_API AHouseBlackHallDoor : public AExistingHouseDoor
{
    GENERATED_BODY()
public:
    AHouseBlackHallDoor();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Configure before FinishSpawning (server). */
    void Setup(EHallDoorStyle InStyle, float InWidth, float InHeight, float InRoomSign, uint8 InFlags);

    enum EFlags : uint8 { KickPlate = 1, Closer = 2, Lever = 4, Deadbolt = 8, FlushBolts = 16, OutSwing = 32, Sign = 64 };

    UPROPERTY(Replicated) uint8 StyleValue = 0;
    UPROPERTY(Replicated) float LeafWidth = 100.f;
    UPROPERTY(Replicated) float LeafHeight = 213.f;
    /** +1 when the leaf's local +Y faces the hall, -1 otherwise. */
    UPROPERTY(Replicated) float RoomSign = 1.f;
    UPROPERTY(Replicated) uint8 Flags = 0;

private:
    void BuildVisual();
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Visual;
};

/** Kickable trash on the hall floor (cans, cups, paper...). Server spawns, movement replicates. */
UCLASS()
class SENIORSENDOFF_API AHouseBlackHallLitter : public AActor
{
    GENERATED_BODY()
public:
    AHouseBlackHallLitter();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    enum EKind : uint8 { Can, CrushedCan, Cup, Paper, Bottle, Plate, ChipBag, PizzaBox, Keg, KindCount };
    UPROPERTY(Replicated) uint8 Kind = 0;
    UPROPERTY(Replicated) uint8 Variant = 0;
    UPROPERTY(Replicated) int32 Seed = 1;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Mesh;

    /** Approximate half height when lying / standing (for spawning). */
    static float RestHalfHeight(uint8 InKind, bool bStanding);
    /** Server: push the item away from a character that walked into it. */
    void Kick(const AActor* By);

private:
    void BuildItem();
    UFUNCTION() void OnTouched(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    double LastKick = -10.0;
};

/** Cosmetic and architectural rebuild of the basement black hall from the reference photographs. Spawned locally on every machine. */
UCLASS(NotPlaceable, Transient)
class SENIORSENDOFF_API AHouseBlackHallDetail : public AActor
{
    GENERATED_BODY()
public:
    AHouseBlackHallDetail();
    /** Replace the original hall finishes and build the photo-matched room. */
    void Build();

    /** Statistics for the review pass. */
    int32 ReplacedTiles = 0, ClippedCollision = 0, BuiltTriangles = 0, RemovedTriangles = 0;
    bool bRebuilt = false;
    FString Notes;

private:
    void ReplaceOriginalShell();
    void BuildArchitecture();
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
    UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Extras;
};

/** Owns generated textures and material instances; spawns the hall rebuild for Chapter01_House. */
UCLASS()
class SENIORSENDOFF_API UHouseBlackHallSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    UMaterialInterface* Mat(FName Key);
    AHouseBlackHallDetail* GetDetail() const { return Detail.Get(); }
    const TArray<TWeakObjectPtr<AHouseBlackHallDoor>>& GetDoors() const { return Doors; }
    const TArray<TWeakObjectPtr<AHouseBlackHallLitter>>& GetLitter() const { return Litter; }
    class UPhysicalMaterial* LitterPhysics(uint8 Kind);

    /** CPU copies of the block-wall tile, used to paint murals that line up with the wall. */
    TSharedPtr<FHallImage> CmuBase, CmuNormal;

    /** Textures generated once per world. */
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UTexture2D>> Textures;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UMaterialInterface>> Materials;

    /** Generates the shared textures and materials once per world (safe to call early). */
    void CreateLook();

private:
    void SpawnDoors();
    void SpawnLitter();
    TArray<TWeakObjectPtr<AHouseBlackHallLitter>> Litter;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> KeepAlive;
    bool bLookReady = false;
    TWeakObjectPtr<AHouseBlackHallDetail> Detail;
    TArray<TWeakObjectPtr<AHouseBlackHallDoor>> Doors;
};

/** Opt-in (-HouseBlackHallSurvey) mapping / capture pass of the basement black hall. */
void TickHouseBlackHallSurvey(AStoryFirstPersonCharacter* Pawn);
/** Opt-in (-HouseBlackHallReview) capture + door checks for the rebuilt hall. */
void TickHouseBlackHallReview(AStoryFirstPersonCharacter* Pawn);
