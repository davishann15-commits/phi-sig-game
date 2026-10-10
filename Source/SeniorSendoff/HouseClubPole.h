#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseClubPole.generated.h"

class AHouseClubPole;
class AStoryFirstPersonCharacter;
class APlayerController;
class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;

/** One atomic replicated journey, authored by the server on the owned pawn. */
USTRUCT(BlueprintType)
struct FHouseClubPoleJourney
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<AHouseClubPole> Pole = nullptr;
    UPROPERTY() uint32 Serial = 0;
    UPROPERTY() bool bClimbing = false;
    UPROPERTY() float ServerStartTime = 0.f;
    UPROPERTY() float Duration = 0.f;
    UPROPERTY() FVector StartCenter = FVector::ZeroVector;
    UPROPERTY() float CapsuleHalfHeight = 0.f;
    // Authority chooses collision-checked alignment around the solid shaft.
    UPROPERTY() TArray<FVector> EntryPoints;
    // Retained when Pole is cleared, so clients distinguish arrival from abort.
    UPROPERTY() bool bCompleted = false;
};

/** A real swept-capsule route through the club aperture, with one-user reservation. */
UCLASS(Blueprintable)
class SENIORSENDOFF_API AHouseClubPole : public AActor
{
    GENERATED_BODY()
public:
    AHouseClubPole();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // All points are actor-local FLOOR/FEET points, not capsule centres.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Club Pole|Route") FVector LowerLanding = FVector(0,150,0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Club Pole|Route") FVector UpperLanding = FVector(0,150,331.65f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Club Pole|Route") FVector LowerShaft = FVector(0,45,0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Club Pole|Route") FVector UpperShaft = FVector(0,45,331.65f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Club Pole|Route") FVector PoleBase = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Club Pole|Route") FVector PoleTop = FVector(0,0,545);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club Pole|Route", meta=(ClampMin="1.2",ClampMax="5")) float SlideSeconds = 2.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club Pole|Route", meta=(ClampMin="2",ClampMax="9")) float ClimbSeconds = 4.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club Pole|Route", meta=(ClampMin="180",ClampMax="300")) float InteractionRadius = 210.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club Pole|Gate") float GateOpenYaw = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club Pole|Appearance") bool bAutoFitPoleMesh = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Club Pole|Appearance", meta=(ClampMin="2",ClampMax="12")) float PoleRadius = 4.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> PoleMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> GatePivot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> GateVisual;
    // This box stays closed to everybody except the server-reserved traveller.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> GateCollision;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Club Pole") TObjectPtr<AStoryFirstPersonCharacter> Occupant;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Club Pole") int32 CompletedJourneys = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Club Pole") int32 AbortedJourneys = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Club Pole") bool bLastCompletedUp = false;

    /** Finds the physically reachable endpoint. Returns false through walls/floors. */
    bool FindFocusedEnd(const AStoryFirstPersonCharacter* Pawn, bool& bUpper) const;
    FString FocusDiagnostics(const AStoryFirstPersonCharacter* Pawn) const;
    FString FocusLabel(const AStoryFirstPersonCharacter* Pawn) const;
    bool TryUse(AStoryFirstPersonCharacter* Pawn, bool bUpper);
    void AbortTraveller(AStoryFirstPersonCharacter* Pawn, const TCHAR* Why);
    void ApplyJourney(AStoryFirstPersonCharacter* Pawn, const FHouseClubPoleJourney& Journey);
    void FinishReplicatedTraveller(AStoryFirstPersonCharacter* Pawn, const FHouseClubPoleJourney& Previous, const FHouseClubPoleJourney& Finished);
    void RestoreTraveller(AStoryFirstPersonCharacter* Pawn);
    void UpdateTraveller(AStoryFirstPersonCharacter* Pawn, float DeltaSeconds);
    FVector LandingWorld(bool bUpper) const;
    FVector FocusWorld(bool bUpper) const;
    FVector SampleJourney(const FHouseClubPoleJourney& Journey, float Progress) const;
    bool CheckRoute(const AStoryFirstPersonCharacter* Pawn, const FHouseClubPoleJourney& Journey, FString& Why) const;
    bool IsEndpointClear(const AStoryFirstPersonCharacter* Pawn, bool bUpper, FString& Why) const;
    float JourneyProgress(const FHouseClubPoleJourney& Journey) const;
    // Test-only readiness comes through the pawn's owning connection. In a
    // normal session this flag can never be set by the opt-in RPC.
    bool bMultiplayerSmokeClientReady = false;
    void MarkMultiplayerSmokeClientReady(AStoryFirstPersonCharacter* Pawn);

private:
    struct FTravellerRestore
    {
        TWeakObjectPtr<APlayerController> Controller;
        uint8 PreviousMode = 1;
        uint8 PreviousCustomMode = 0;
        bool bIgnoredMove = false;
        bool bIgnoreCorrections = false;
        bool bIgnoreClientCorrections = false;
        bool bAcceptClientPosition = false;
        bool bAlreadyIgnoredGate = false;
        uint32 Serial = 0;
        FVector LastPosition = FVector::ZeroVector;
        float MaximumStep = 0.f;
        int32 Samples = 0;
        FHouseClubPoleJourney Journey;
        float LocalElapsed = 0.f;
        float Progress = 0.f;
        bool bFinishing = false;
        float FinishElapsed = 0.f;
        float FinishFromProgress = 0.f;
    };
    TMap<TWeakObjectPtr<AStoryFirstPersonCharacter>, FTravellerRestore> TravellerState;
    float ClosedGateYaw = 0.f;
    float ServerTime() const;
    bool CapsuleClear(const AStoryFirstPersonCharacter* Pawn, const FVector& Center, bool bIgnoreGate, FHitResult* Hit = nullptr) const;
    bool SweepTravellerRoute(AStoryFirstPersonCharacter* Pawn, FTravellerRestore& Restore, float Progress);
    void UpdateFinishedTraveller(AStoryFirstPersonCharacter* Pawn, float DeltaSeconds);
    void CompleteTraveller(AStoryFirstPersonCharacter* Pawn);
};
