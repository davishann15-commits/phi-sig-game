#pragma once
#include "CoreMinimal.h"
class AActor;
class USceneComponent;
class UWidgetComponent;
class UPointLightComponent;
class USceneCaptureComponent2D;

// Physical selection-room geometry. All fixtures, writing, props and the
// character are rendered by ONE perspective camera, with real depth/shadows.
class FSeniorCharacterRoom
{
public:
    void Build(AActor* InOwner);
    void Update(int32 Character,int32 Weapon,float Seconds);
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
    TWeakObjectPtr<UPointLightComponent> CeilingLight;
    int32 ShownCharacter=-1,SelectedWeapon=0;
};
