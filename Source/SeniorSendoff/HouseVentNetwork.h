#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseVentNetwork.generated.h"

class APawn;
class APlayerController;
class AStoryFirstPersonCharacter;
class UPointLightComponent;
class UProceduralMeshComponent;
class USceneComponent;

/**
 * A hidden, connected crawl space reached through the existing house vent and
 * ceiling-hatch facades. The facades and the solid house shell are untouched:
 * interaction makes a brief concealed handoff to a real, collidable duct.
 * Spawn once on the authority in Chapter01_House; the actor replicates, while
 * each machine constructs the same static duct collision in BeginPlay.
 */
UCLASS()
class SENIORSENDOFF_API AHouseVentNetwork : public AActor
{
    GENERATED_BODY()

public:
    AHouseVentNetwork();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** The closest visible facade, or the nearby exit marker while in the duct. */
    FName FindFocusedPortal(const APawn* Pawn, float MinimumFacing = 0.35f) const;
    /** Called only from the story pawn's server-validated interaction path. */
    bool Interact(AStoryFirstPersonCharacter* Pawn, FName PortalId);
    bool IsInDuct(const APawn* Pawn) const;
    bool IsPortalActive(FName PortalId) const;
    FString PortalDisplayName(FName PortalId) const;
    TArray<FName> GetPortalIds() const;
    int32 GetPortalCount() const;
    int32 NumActivePortals() const;
    int32 GetDuctSectionCount() const;
    FBox GetDuctBounds() const;
    TArray<FIntPoint> GetRouteCells() const;
    /** World-space duct cell centres at mid-height, in GetRouteCells order. */
    TArray<FVector> GetDuctCellCenters() const;
    FVector GetPortalDuctLocation(FName PortalId) const;
    FVector GetPortalFacadeLocation(FName PortalId) const;
    /** Measured floor point in front of the facade; add pawn half-height to place a pawn. */
    FVector GetPortalRoomApproach(FName PortalId) const;

    /** Recheck the saved map actors after the black-hall rebuild has run. */
    void RefreshPortalAnchors();

    /** Actor-local origin beneath the existing basement, above the map's fall limit. */
    UPROPERTY(EditDefaultsOnly, Category="House Vent Network")
    FVector ServiceOrigin = FVector(0.f, 0.f, -900.f);

    /** 108 cm clear cell width and 72 cm clear height suit the prone capsule. */
    UPROPERTY(EditDefaultsOnly, Category="House Vent Network", meta=(ClampMin="90", ClampMax="130"))
    float CellSize = 108.f;

    UPROPERTY(EditDefaultsOnly, Category="House Vent Network", meta=(ClampMin="65", ClampMax="90"))
    float ClearHeight = 72.f;

private:
    struct FReturnPoint
    {
        FName PortalId;
        FTransform Transform;
    };

    void BuildDuct();
    FVector CellCenterLocal(const FIntPoint& Cell) const;
    FVector CellCenterWorld(const FIntPoint& Cell) const;
    bool FindDuctLanding(const AStoryFirstPersonCharacter* Pawn, int32 PortalIndex, FVector& OutLocation) const;
    bool FindRoomLanding(const AStoryFirstPersonCharacter* Pawn, int32 PortalIndex,
        FVector& OutLocation, FRotator& OutRotation) const;
    bool IsCapsuleClear(const APawn* Pawn, const FVector& Location, float Radius, float HalfHeight) const;
    void FinishTransition(TWeakObjectPtr<AStoryFirstPersonCharacter> WeakPawn,
        TWeakObjectPtr<APlayerController> WeakController, int32 PortalIndex, bool bEntering);

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;
    UPROPERTY(Transient)
    TObjectPtr<UProceduralMeshComponent> DuctShell;
    UPROPERTY(Transient)
    TObjectPtr<UProceduralMeshComponent> DuctTrim;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UPointLightComponent>> DuctLights;
    UPROPERTY(Replicated)
    uint8 ActivePortalMask = 0;

    TSet<TWeakObjectPtr<AStoryFirstPersonCharacter>> PendingTransitions;
    TMap<TWeakObjectPtr<AStoryFirstPersonCharacter>, TWeakObjectPtr<APlayerController>> PendingControllers;
    TMap<TWeakObjectPtr<AStoryFirstPersonCharacter>, FReturnPoint> ReturnPoints;
};
