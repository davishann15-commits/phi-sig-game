#pragma once

#include "CoreMinimal.h"
#include "ExistingHouseDoor.h"
#include "HouseInteractiveProp.h"
#include "HouseRearPorch.generated.h"

struct FHallMesh;
class UHouseBlackHallSubsystem;
class UPhysicalMaterial;
class AStoryFirstPersonCharacter;

namespace HouseRear
{
    // Already-scaled exterior world coordinates, not source scan coordinates.
    constexpr float Floor = -359.f;
    constexpr float GateWidth = 140.f;
    inline FVector GateHinge() { return FVector(1490,1775,Floor); }
    inline FVector GateCenter() { return GateHinge()+FVector(0,GateWidth*.5f,130); }
    inline FVector TableCenter() { return FVector(300,3260,Floor); }
    inline FVector FootballHome() { return FVector(850,3420,Floor+12); }
}

/** Photo-matched metal court door using the house's existing E/double-click interaction. */
UCLASS(NotPlaceable)
class SENIORSENDOFF_API AHouseCourtDoor : public AExistingHouseDoor
{
    GENERATED_BODY()
public:
    AHouseCourtDoor();
    virtual void BeginPlay() override;
};

UENUM()
enum class EHouseCourtItem : uint8 { Football, Die, Volleyball };

/** Small, independent sports props. Art is generated locally; only gameplay state replicates. */
UCLASS(NotPlaceable)
class SENIORSENDOFF_API AHouseCourtProp : public AHouseInteractiveProp
{
    GENERATED_BODY()
public:
    AHouseCourtProp();
    virtual void BeginPlay() override;
    virtual float TakeDamage(float Amount,const FDamageEvent& Event,AController* Instigator,AActor* Causer) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(Replicated) EHouseCourtItem Item = EHouseCourtItem::Football;
private:
    UPROPERTY(Transient) TObjectPtr<UPhysicalMaterial> Surface;
};

void BuildHouseRearPorchDetail(FHallMesh& Solid,FHallMesh& Decor,UHouseBlackHallSubsystem* Look);
void BuildHouseRearCourtDetail(FHallMesh& Solid,FHallMesh& Decor,UHouseBlackHallSubsystem* Look);
void SpawnHouseRearCourtItems(UWorld* World);
void TickHouseRearPorchReview(AStoryFirstPersonCharacter* Pawn);
