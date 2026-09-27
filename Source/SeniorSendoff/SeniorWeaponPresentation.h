#pragma once
#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "SeniorSauceVisual.h"

// Cosmetic prototypes only. Shared geometry between the wall rack and held preview;
// deliberately independent from the gameplay weapon / projectile implementation.
namespace SeniorWeaponPresentation
{
inline USceneComponent* Build(AActor* Owner, USceneComponent* Parent, int32 Character, int32 Weapon)
{
    auto* Root=NewObject<USceneComponent>(Owner);
    Owner->AddInstanceComponent(Root); Root->SetupAttachment(Parent); Root->RegisterComponent();
    auto Part=[&](const TCHAR* Shape,FVector Position,FVector Size,FRotator Rotation,bool Wood=false)
    {
        auto* C=NewObject<UStaticMeshComponent>(Owner); Owner->AddInstanceComponent(C);
        C->SetupAttachment(Root);
        C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Shape,Shape)));
        if(auto* Material=LoadObject<UMaterialInterface>(nullptr,Wood?TEXT("/Game/Weapons/Presentation/M_ArmoryWalnut.M_ArmoryWalnut"):TEXT("/Game/Weapons/Presentation/M_ArmorySteel.M_ArmorySteel"))) C->SetMaterial(0,Material);
        C->SetRelativeLocation(Position); C->SetRelativeScale3D(Size/100.f); C->SetRelativeRotation(Rotation);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(true); C->RegisterComponent();
    };
    if(Character==0 && Weapon==0)
    {
        auto* Hat=NewObject<UStaticMeshComponent>(Owner); Owner->AddInstanceComponent(Hat); Hat->SetupAttachment(Root);
        Hat->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Weapons/Douli/SM_Douli.SM_Douli")));
        Hat->SetCollisionEnabled(ECollisionEnabled::NoCollision); Hat->RegisterComponent();
    }
    else if(Character==0 && Weapon==1)
    {
        SeniorSauceVisual::Build(Owner,Root);
    }
    else if(Weapon==1)
    {
        const bool Pistol=Character==1 || Character==5 || Character==6;
        const float Length=Pistol?19.f:55.f;
        // Origin is the grip, barrel points +X, top is +Z.
        Part(TEXT("Cube"),FVector(3,0,4),FVector(Pistol?16:20,5,6),FRotator::ZeroRotator);
        Part(TEXT("Cube"),FVector(-2,0,-3),FVector(5,4,12),FRotator(18,0,0),true);
        const bool Double=Character==0;
        for(int32 I=0;I<(Double?2:1);++I)
            Part(TEXT("Cylinder"),FVector(Length*.5f+9,Double?(I==0?-1.6f:1.6f):0,5),FVector(2.7,2.7,Length),FRotator(90,0,0));
        if(!Pistol)
        {
            Part(TEXT("Cube"),FVector(-19,0,0),FVector(27,5,9),FRotator(-10,0,0),true);
            Part(TEXT("Cube"),FVector(-32,0,-2),FVector(2,6,11),FRotator(-10,0,0));
            Part(TEXT("Cube"),FVector(24,0,1),FVector(23,5.5,5),FRotator::ZeroRotator,true);
            for(int32 I=0;I<7;++I) Part(TEXT("Cube"),FVector(15+I*3,0,-1.5),FVector(.5,5.6,.5),FRotator::ZeroRotator);
        }
        Part(TEXT("Cube"),FVector(Length+7,0,7),FVector(2,1,2),FRotator::ZeroRotator);
        // Visible trigger guard: three pieces instead of an opaque block.
        Part(TEXT("Cube"),FVector(5,0,-7),FVector(10,1,1),FRotator::ZeroRotator);
        Part(TEXT("Cube"),FVector(10,0,-3),FVector(1,1,7),FRotator::ZeroRotator);
        Part(TEXT("Cube"),FVector(3,0,-2),FVector(1,1,4),FRotator(-18,0,0));
    }
    else
    {
        const float Length=Character==6?115.f:Character==4?80.f:Character==5?40.f:70.f;
        Part(TEXT("Cylinder"),FVector(Length*.45f,0,0),FVector(3,3,Length),FRotator(90,0,0),Character!=2 && Character!=7);
        if(Character==1) Part(TEXT("Sphere"),FVector(48,0,0),FVector(40,6,6),FRotator::ZeroRotator);
        if(Character==4) Part(TEXT("Cube"),FVector(Length-8,0,0),FVector(13,25,12),FRotator::ZeroRotator);
        if(Character==5) Part(TEXT("Cube"),FVector(Length-5,4,0),FVector(12,15,3),FRotator(0,0,12));
        if(Character==2 || Character==7)
        {
            Part(TEXT("Cube"),FVector(Length-5,3,0),FVector(12,6,4),FRotator(0,30,0));
            Part(TEXT("Cube"),FVector(Length,8,0),FVector(4,10,4),FRotator::ZeroRotator);
        }
        if(Character==3) Part(TEXT("Cylinder"),FVector(Length-5,0,0),FVector(8,8,13),FRotator(90,0,0));
        for(int32 I=0;I<8;++I) Part(TEXT("Cylinder"),FVector(I*1.3f,0,0),FVector(3.4,3.4,.6),FRotator(90,0,0));
    }
    return Root;
}
inline void Destroy(USceneComponent* Root)
{
    if(!Root)return;
    TArray<USceneComponent*> Children; Root->GetChildrenComponents(true,Children);
    for(auto* C:Children) C->DestroyComponent();
    Root->DestroyComponent();
}
}

class SWidget;
class UWorld;
TSharedRef<SWidget> MakeSeniorWeaponWallPreview(UWorld* World,TAttribute<int32> Character,int32 Weapon);
