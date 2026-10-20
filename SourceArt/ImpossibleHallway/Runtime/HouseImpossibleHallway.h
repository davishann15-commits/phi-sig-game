#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseImpossibleHallway.generated.h"

class AExistingHouseDoor;
class AStoryFirstPersonCharacter;
class UAudioComponent;

/** A bounded physical passage with periodic scenery and an impossible return. */
UCLASS()
class SENIORSENDOFF_API AHouseImpossibleHallway : public AActor
{
    GENERATED_BODY()
public:
    AHouseImpossibleHallway();
    virtual void BeginPlay() override;
    UPROPERTY(EditAnywhere, Category="Hallway") FName EntranceDoorId = TEXT("HallEnd_End");
    UPROPERTY(EditAnywhere, Category="Hallway", meta=(ClampMin="120")) float ModuleLength = 180.f;
    UPROPERTY(EditAnywhere, Category="Hallway") float LoopDepth = 360.f;
    UPROPERTY(EditAnywhere, Category="Hallway") float ReturnMinDepth = 90.f;
    UPROPERTY(EditAnywhere, Category="Hallway") float CorridorHalfWidth = 110.f;
    UPROPERTY(EditAnywhere, Category="Hallway") float CorridorHeight = 263.8f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Hallway|Diagnostics") int32 TotalLoops = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Hallway|Diagnostics") int32 TotalReturns = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Hallway|Diagnostics") float VirtualTravel = 0.f;

    void UpdateTraveler(AStoryFirstPersonCharacter* Pawn);
    bool IsTravelerActive(const AStoryFirstPersonCharacter* Pawn) const;
private:
    struct FTraveler
    {
        bool bActive = false;
        bool bTurned = false;
        float LastDepth = 0.f;
    };
    TMap<TWeakObjectPtr<AStoryFirstPersonCharacter>, FTraveler> Travelers;
    TWeakObjectPtr<AExistingHouseDoor> EntranceDoor;
    UPROPERTY(VisibleAnywhere, Category="Hallway") TObjectPtr<UAudioComponent> PipeDrone;
    bool TranslateTraveler(AStoryFirstPersonCharacter* Pawn, const FVector& Delta);
};

// Called after CharacterMovement and before the character's hand/carry pose.
void UpdateHouseImpossibleHallwayTraveler(AStoryFirstPersonCharacter* Pawn);
