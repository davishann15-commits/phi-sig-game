#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "BoywickRoom.generated.h"

class ABoywickCharacter;
class AStoryFirstPersonCharacter;
class AHUD;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/** Where Boywick's bedroom is (the old second-floor pigsty), in world cm. */
namespace BoywickRoomSpec
{
    // Interior of the bedroom, sized to match Room 204 next door (the north wall moved up to just behind 204's closets).
    constexpr float XW = -2972.f, XE = -2355.f, YS = -1371.8f, YN = -587.f, ZF = 302.3f, ZC = 566.1f;
    // Built-in closets along the door wall, north of the hallway door.
    constexpr float ClosetFaceX = -2445.f, ClosetY0 = -1140.f, ClosetY1 = -657.f;
    // Window on the far (west) wall.
    constexpr float WindowY0 = -1330.8f, WindowY1 = -1202.8f, WindowTop = ZF + 226.f;
    inline FVector SpawnFeet() { return FVector(-2650.f, -938.8f, ZF); }
    // Z band is tight so a wander goal can never land on a table top or the loft deck.
    inline FVector WanderMin() { return FVector(XW + 40.f, YS + 40.f, ZF - 10.f); }
    inline FVector WanderMax() { return FVector(ClosetFaceX - 40.f, YN - 160.f, ZF + 12.f); }
}

/** Cosmetic, locally spawned dressing for the bedroom that needs painted textures: banner, poster, string lights. */
UCLASS(NotPlaceable, Transient)
class SENIORSENDOFF_API ABoywickRoomDecor : public AActor
{
    GENERATED_BODY()
public:
    ABoywickRoomDecor();
    void Build();
    int32 BulbCount = 0;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Panels;
    UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Bulbs;
    UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Wire;
    UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Glow;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
    UStaticMeshComponent* Panel(const FName& Name, const FVector& Center, const FRotator& Rotation, float Width, float Height, UMaterialInterface* Material, float Thickness = 1.2f);
    void StringLights(const TArray<FVector>& Hooks);
};

/** Spawns Boywick in his bedroom on Chapter01_House and the room's painted dressing on every machine. */
UCLASS()
class SENIORSENDOFF_API UBoywickRoomSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    ABoywickCharacter* GetBoywick() const { return Boywick.Get(); }
private:
    TWeakObjectPtr<ABoywickCharacter> Boywick;
    TWeakObjectPtr<ABoywickRoomDecor> Decor;
    FTimerHandle CheckTimer, SmokeTimer;
    void TickSmoke();
    int32 SmokeStep = 0, SmokeLine = -1;
    float SmokeStarted = 0.f, SmokeStepAt = 0.f;
    float SmokeBaseWalk = 0.f, SmokeBaseSprint = 0.f, SmokeBaseJump = 0.f;
    float SmokeJawMaximum = 0.f;
    bool bSmokeStoryImage = false;
};

/** HUD overlay: prompt, speech box, health bar and blessing timer. Called from AStoryHUD::DrawHUD. */
void BoywickDrawOverlay(AHUD* Hud, AStoryFirstPersonCharacter* Pawn, APlayerController* PC, float S, float W, float H,
    const FLinearColor& Ink, const FLinearColor& Gold);
