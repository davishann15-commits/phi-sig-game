#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseGadgets.generated.h"

class UAudioComponent;
class UBoxComponent;
class ULightComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UProceduralMeshComponent;

/** Small wall gadgets the player can use: alarm pull stations, the horn/strobes they set off, light switches and the singing bass. */
UENUM()
enum class EHouseGadgetKind : uint8
{
    PullStation, HornStrobe, LightSwitch, BillyBass, Count
};

/** Where the hallway dressing wants a gadget: frame X points out of the wall, Z up, origin on the wall surface. */
USTRUCT()
struct FHouseGadgetSpec
{
    GENERATED_BODY()
    UPROPERTY() EHouseGadgetKind Kind = EHouseGadgetKind::PullStation;
    UPROPERTY() FTransform Frame;
    UPROPERTY() uint8 Variant = 0;
};

/** Server-spawned, replicated. The geometry is rebuilt locally from Kind; only Kind/Variant/State replicate. */
UCLASS(NotPlaceable)
class SENIORSENDOFF_API AHouseGadget : public AActor
{
    GENERATED_BODY()
public:
    AHouseGadget();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Authority. Returns true when something happened. */
    bool Interact(APawn* Player);
    /** Authority: horns start / stop flashing and sounding, pulled stations reset. */
    void SetAlarm(bool bOn);
    bool IsInteractive() const { return Kind != EHouseGadgetKind::HornStrobe; }
    FString FocusLabel() const;

    UPROPERTY(ReplicatedUsing=OnRep_Kind) EHouseGadgetKind Kind = EHouseGadgetKind::PullStation;
    UPROPERTY(Replicated) uint8 Variant = 0;
    UPROPERTY(ReplicatedUsing=OnRep_State) uint8 State = 0;
    UPROPERTY(Replicated) float StateTime = 0.f;
    /** Horn/strobe only: true on the frames the lens is flashing. */
    bool bStrobeLit = false;

private:
    UFUNCTION() void OnRep_Kind();
    UFUNCTION() void OnRep_State();
    void SetState(uint8 NewState);
    void BuildParts();
    void ApplyState(bool bWithSound = true);
    void FindLights();
    void StartHorn();
    void StopHorn();
    float ServerNow() const;

    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TObjectPtr<UBoxComponent> Focus;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Fixed;
    UPROPERTY() TObjectPtr<USceneComponent> Hinge;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Moving;
    UPROPERTY() TObjectPtr<USceneComponent> Hinge2;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Moving2;
    UPROPERTY() TObjectPtr<USceneComponent> Hinge3;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Moving3;
    float BassHeadSign = 1.f;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Glow;
    UPROPERTY() TObjectPtr<UAudioComponent> Horn;
    UPROPERTY() TObjectPtr<UAudioComponent> Song;
    UPROPERTY() TObjectPtr<UPointLightComponent> Flash;
    TArray<TWeakObjectPtr<ULightComponent>> Lights;
    FTimerHandle ResetTimer;
    bool bBuilt = false;
    bool bLightsFound = false;
    float Pitch = 0.f;
};
