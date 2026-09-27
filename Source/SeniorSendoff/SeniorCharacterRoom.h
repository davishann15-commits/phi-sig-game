#pragma once
#include "CoreMinimal.h"
class AActor;
class USceneComponent;
class UWidgetComponent;
class UPointLightComponent;
class USceneCaptureComponent2D;
class UStaticMeshComponent;

// Physical selection-room geometry. All fixtures, writing, props and the
// character are rendered by ONE perspective camera, with real depth/shadows.
class FSeniorCharacterRoom
{
public:
    static FVector ArrowPosition(int32 Index) { return FVector(-53.f,Index==0?90.f:-90.f,130.f); }
    static FVector RosterLampPosition(int32 Index);
    void Build(AActor* InOwner);
    void Update(int32 Character,int32 Weapon,float Seconds);
    void SetReturnHovered(bool bHovered);
    void SetArrowHovered(int32 ArrowIndex);
    void ReleaseWidgets();
    int32 Pick(const FVector& Origin,const FVector& Direction) const;
    bool Validate()const;
private:
    TWeakObjectPtr<AActor> Owner;
    TWeakObjectPtr<USceneComponent> Root;
    TWeakObjectPtr<USceneComponent> Weapons[2];
    TWeakObjectPtr<UWidgetComponent> Writing;
    TWeakObjectPtr<UWidgetComponent> Labels[2];
    TWeakObjectPtr<UWidgetComponent> Neon;
    TWeakObjectPtr<UWidgetComponent> AbilityDisplay;
    TWeakObjectPtr<UWidgetComponent> RosterCard;
    TWeakObjectPtr<UStaticMeshComponent> ArrowBars[2][2];
    TWeakObjectPtr<UStaticMeshComponent> ShelfGlowBars[2][2];
    TWeakObjectPtr<UStaticMeshComponent> WeaponStandPosts[2][2];
    TWeakObjectPtr<UStaticMeshComponent> WeaponStandDecks[2];
    TWeakObjectPtr<UPointLightComponent> ShelfLights[2];
    TArray<TWeakObjectPtr<UStaticMeshComponent>> RosterLights;
    TWeakObjectPtr<UPointLightComponent> CeilingLight;
    TWeakObjectPtr<UPointLightComponent> ReturnGlow;
    int32 ShownCharacter=-1,SelectedWeapon=0;
    int32 HoveredArrow=-1;
    bool bReturnHovered=false,bReturnHoverInitialized=false;
};
