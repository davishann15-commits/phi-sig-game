#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExistingHouseDoor.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class AHouseReactiveFixture;

/** Moves a leaf extracted from the imported house mesh; no replacement door art. */
UCLASS()
class SENIORSENDOFF_API AExistingHouseDoor : public AActor
{
    GENERATED_BODY()
public:
    AExistingHouseDoor();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door")
    TObjectPtr<UStaticMesh> OriginalLeaf;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door")
    TObjectPtr<UStaticMesh> OriginalGlass;
    /** Perimeter-only porch frame after its glass and attached grille break. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door")
    TObjectPtr<UStaticMesh> BrokenFrameMesh;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door")
    FVector DoorwayCenter;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door")
    FName DoorId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door")
    float ClosedYaw = -90.f;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door")
    float OpenYaw = 0.f;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door")
    bool bLocked = false;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door")
    FName DoorGroup;
    /** A simple moving blocker for each framed porch glass leaf. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door|Collision")
    FVector PanelCenterWorld = FVector::ZeroVector;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door|Collision")
    FVector PanelHalfExtent = FVector::ZeroVector;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door|Hardware")
    TObjectPtr<UStaticMesh> AddedHandleMesh;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door|Hardware")
    FVector AddedHandleWorldPosition;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door|Hardware")
    FRotator AddedHandleRotation;

    /** Leaf/panel/handle values are authored relative to the hinge pivot (doors built in code). */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door")
    bool bLocalLeafSpace = false;

    /** World anchor of the extracted mesh's original coordinate origin. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="House Door")
    FVector SourceGeometryOriginWorld = FVector::ZeroVector;

    bool IsOpen() const { return bOpen; }
    USceneComponent* GetLeafPivot() const { return LeafPivot; }
    bool Interact(APawn* Player);
    AHouseReactiveFixture* GetPorchGlassFixture() const { return PorchGlassFixture; }
    UStaticMeshComponent* GetOriginalLeafComponent() const { return Leaf; }
    UBoxComponent* GetPanelCollision() const { return PanelCollision; }

private:
    void SetGroupOpen(bool bShouldOpen);
    void StartMotion();
    void UpdateBlockingCollision(float CurrentYaw);
    bool IsDoorwayOccupied() const;
    UFUNCTION()
    void OnRep_Open();
    void CreatePorchGlassFixture();
    void OnPorchGlassBroken(AHouseReactiveFixture* Fixture);
    UFUNCTION() void OnRep_PorchGlassFixture();
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> LeafPivot;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Leaf;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Glass;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> AddedHandle;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBoxComponent> PanelCollision;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBoxComponent> Approach;
    UPROPERTY(ReplicatedUsing=OnRep_Open)
    bool bOpen = false;
    UPROPERTY(ReplicatedUsing=OnRep_PorchGlassFixture, Transient)
    TObjectPtr<AHouseReactiveFixture> PorchGlassFixture;
    bool bComplexFrameReady = false;
    float MotionFromYaw = 0.f;
    float MotionToYaw = 0.f;
    float MotionElapsed = 0.f;
    float MotionDuration = 0.7f;
};
