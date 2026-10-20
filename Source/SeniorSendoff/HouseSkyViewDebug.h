#pragma once
// TEMPORARY helper (Oct 2026): tap Space twice to lift the camera high above the player and look down over the grounds;
// tap twice again to drop back into first person. The mouse wheel zooms while it is up, WASD and the mouse still move
// and turn the player underneath. Nothing else refers to this file pair, so deleting HouseSkyViewDebug.h/.cpp removes
// the feature. It stays inert in the automated runs (-...Review, -...Smoke, -HouseExteriorViews) and with -NoSkyView;
// -HouseSkyViewSmoke runs its own self-test and writes Saved/HouseSkyView/sky_up.png and sky_down.png.
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HouseSkyViewDebug.generated.h"

class ACameraActor;
class APawn;
class APlayerController;

UCLASS()
class SENIORSENDOFF_API UHouseSkyViewDebugSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UHouseSkyViewDebugSubsystem, STATGROUP_Tickables); }
    bool IsUp() const { return bUp; }
private:
    void Enter(APlayerController* PC, APawn* Pawn);
    void Leave(APlayerController* PC, APawn* Pawn);
    void Place(APlayerController* PC, APawn* Pawn, float DeltaSeconds);
    void Smoke(APlayerController* PC, APawn* Pawn);
    UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
    bool bUp = false;
    double LastSpace = -100.0;
    double LeftAt = -100.0;
    float Zoom = 1.f;
    int32 SmokeStep = 0;
    double SmokeAt = 0.0;
    int32 SmokeFails = 0;
};
