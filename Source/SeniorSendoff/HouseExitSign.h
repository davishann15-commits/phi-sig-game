#pragma once
#include "CoreMinimal.h"
#include "HouseInteractiveProp.h"
#include "HouseExitSign.generated.h"
class UPointLightComponent;

/** Powered exit fixture with breakaway mounting and persistent falling housing. */
UCLASS()
class SENIORSENDOFF_API AHouseExitSign : public AHouseInteractiveProp
{
    GENERATED_BODY()
public:
    AHouseExitSign();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    void Configure(uint8 Direction, bool DoubleSided);
    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Style, Category="Exit Sign") uint8 ArrowStyle = 0;
    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Style, Category="Exit Sign") bool bDoubleSided = false;
    UPROPERTY(VisibleAnywhere, Category="Exit Sign") TObjectPtr<UPointLightComponent> Glow;
private:
    UFUNCTION() void OnRep_Style();
    void ApplyStyle();
    void UpdateGlow();
    void SmokeImpact();
    void SmokeCheckFall();
    void SmokeCheckBreak();
    FVector SmokeStart;
    FTimerHandle GlowTimer, SmokeTimer;
};
