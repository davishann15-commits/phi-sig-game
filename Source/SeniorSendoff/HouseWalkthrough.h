#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HouseWalkthrough.generated.h"

class UCameraComponent;
class USpotLightComponent;
class ACameraActor;
class FJsonObject;
class FJsonValue;

// These are runtime review data, not a replacement campaign or save format.
struct FHouseReviewLanding
{
    FString Name;
    FVector Feet = FVector::ZeroVector;
    FVector Direction = FVector::ForwardVector;
};

struct FHouseReviewRoute
{
    FString Name;
    TArray<FVector> Feet;
};

struct FHouseReviewView
{
    FString Name;
    FVector Position = FVector::ZeroVector;
    FVector Target = FVector::ForwardVector;
    float VerticalFieldOfView = 70.0f;
};

UCLASS()
class SENIORSENDOFF_API AHouseWalkthroughCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AHouseWalkthroughCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    void JumpToLanding(int32 Number);
    bool IsAutomatedReview() const { return bSmoke || bCapture; }
    FString GetReviewStatus() const { return Status; }
    bool IsFlashlightOn() const;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCameraComponent> ViewCamera;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USpotLightComponent> Flashlight;
    UPROPERTY(Transient)
    TObjectPtr<ACameraActor> CaptureCamera;

    TArray<FHouseReviewLanding> Landings;
    TArray<FHouseReviewRoute> Routes;
    TArray<FHouseReviewView> Views;
    TArray<TSharedPtr<FJsonValue>> FloorResults;
    TArray<TSharedPtr<FJsonValue>> RouteResults;
    TArray<TSharedPtr<FJsonValue>> CaptureResults;
    TArray<TSharedPtr<FJsonValue>> CaptureMetadata;
    TArray<TSharedPtr<FJsonValue>> Errors;
    TArray<TSharedPtr<FJsonValue>> RouteWaypoints;
    FString LayoutPath;
    FString OutputDirectory;
    FString Status;
    FString LastBlocker;
    bool bSmoke = false;
    bool bCapture = false;
    bool bExitAfterReview = false;
    bool bReviewFinished = false;
    int32 RouteIndex = -1;
    int32 RouteStartIndex = 0;
    int32 RouteEndIndex = 0;
    int32 WaypointIndex = 0;
    int32 CaptureIndex = -1;
    double ReviewStartTime = 0;
    double PhaseStartTime = 0;
    float BestWaypointDistance = BIG_NUMBER;
    float RouteDistance = 0;
    FVector PreviousPosition = FVector::ZeroVector;
    FString PendingScreenshot;
    FString ReviewStartedUtc;
    bool bAllViews = false;
    bool bReportWriteFailed = false;
    int32 TotalAvailableViews = 0;
    int32 ExpectedCaptureCount = 0;
    int32 CaptureViewportWidth = 0;
    int32 CaptureViewportHeight = 0;
    float CaptureHorizontalFOV = 0;
    double RouteSimulationSeconds = 0;
    double WaypointSimulationSeconds = 0;
    double NoProgressSimulationSeconds = 0;
    double ScreenshotRequestedTime = 0;

    void LookHorizontal(float Value);
    void LookVertical(float Value);
    void ToggleFlashlight();
    void StartUserJump();
    void StopUserJump();
    bool LoadReviewLayout();
    bool FindSupport(const FVector& ExpectedFeet, FHitResult& Hit) const;
    FVector GetFeet() const;
    void PlaceAtFeet(const FVector& Feet, const FVector& Direction);
    void StartReview();
    void StartRoute();
    void UpdateReview(float DeltaSeconds);
    void FinishRoute(bool bReached, const FString& Reason);
    void StartCaptures();
    void UpdateCaptures();
    void FinishReview();
    bool WriteReport(const FString& Result);
    void AddError(const FString& Message);
};

UCLASS()
class SENIORSENDOFF_API AHouseWalkthroughController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
private:
    void ToggleReviewPause();
    void QuitReview();
    void SelectLanding(int32 Number);
};

UCLASS()
class SENIORSENDOFF_API AHouseWalkthroughHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};

UCLASS()
class SENIORSENDOFF_API AHouseWalkthroughGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AHouseWalkthroughGameMode();
};
