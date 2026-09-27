#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

// A small sealed sauce cup with a printed peel-top. The same packet is used on
// the armory shelf, in Braxton's hand, and as the thrown projectile.
namespace SeniorSauceVisual
{
inline USceneComponent* Build(AActor* Owner,USceneComponent* Parent,bool bOnlyOwnerSee=false)
{
    auto* Root=NewObject<USceneComponent>(Owner);
    Owner->AddInstanceComponent(Root);Root->SetupAttachment(Parent);Root->RegisterComponent();
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Weapons/Sauce/M_SauceColor.M_SauceColor"));
    auto Color=[&](FLinearColor Tint)->UMaterialInterface*
    {
        if(!Base)return nullptr;
        auto* Material=UMaterialInstanceDynamic::Create(Base,Owner);
        Material->SetVectorParameterValue(TEXT("BaseColor"),Tint);
        Material->SetScalarParameterValue(TEXT("Roughness"),.78f);
        return Material;
    };
    auto* Sauce=Color(FLinearColor(.83f,.39f,.055f));
    auto* Foil=Color(FLinearColor(.19f,.025f,.025f));
    auto* Gold=Color(FLinearColor(.91f,.57f,.12f));
    auto* Cream=Color(FLinearColor(.94f,.78f,.46f));
    auto* Silver=Color(FLinearColor(.72f,.70f,.61f));
    auto Part=[&](FVector Position,FVector Size,UMaterialInterface* Finish)
    {
        auto* C=NewObject<UStaticMeshComponent>(Owner);Owner->AddInstanceComponent(C);C->SetupAttachment(Root);
        UStaticMesh* Rounded=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Lobby/CharacterRoom/SM_RoomBeveledCube.SM_RoomBeveledCube"));
        if(!Rounded)Rounded=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
        C->SetStaticMesh(Rounded);
        for(int32 Slot=0;Slot<C->GetNumMaterials();++Slot)C->SetMaterial(Slot,Finish);
        C->SetRelativeLocation(Position);C->SetRelativeScale3D(Size/100.f);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetOnlyOwnerSee(bOnlyOwnerSee);C->RegisterComponent();
        return C;
    };
    Part(FVector(-.15f,0,2.3f),FVector(5.5f,8.3f,4.6f),Sauce);
    Part(FVector(.2f,0,4.6f),FVector(6.1f,9.5f,.45f),Silver);
    Part(FVector(0,0,9.4f),FVector(.75f,11.2f,9.8f),Foil);
    Part(FVector(.44f,0,9.6f),FVector(.17f,10.35f,6.15f),Gold);
    Part(FVector(.55f,0,9.7f),FVector(.08f,9.65f,5.25f),Cream);
    Part(FVector(.57f,0,6.58f),FVector(.12f,10.5f,.34f),Silver);
    Part(FVector(.58f,0,12.5f),FVector(.11f,10.1f,.27f),Gold);
    auto Print=[&](const TCHAR* Words,FVector Position,float Size,FColor Ink)
    {
        auto* T=NewObject<UTextRenderComponent>(Owner);Owner->AddInstanceComponent(T);T->SetupAttachment(Root);
        T->SetText(FText::FromString(Words));T->SetHorizontalAlignment(EHTA_Center);
        T->SetWorldSize(Size);T->SetTextRenderColor(Ink);T->SetRelativeLocation(Position);
        T->SetOnlyOwnerSee(bOnlyOwnerSee);T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        T->SetCastShadow(false);T->RegisterComponent();
    };
    Print(TEXT("SMACKDOWN"),FVector(.68f,0,10.8f),1.04f,FColor(65,13,9));
    Print(TEXT("WILLIAMS"),FVector(.68f,0,9.35f),1.37f,FColor(134,22,14));
    Print(TEXT("FAST FOOD"),FVector(.68f,0,8.15f),.50f,FColor(53,25,19));
    Print(TEXT("SIGNATURE SAUCE"),FVector(.68f,0,7.28f),.60f,FColor(39,22,14));
    Print(TEXT("1 OZ"),FVector(.69f,0,5.85f),.36f,FColor(236,216,172));
    return Root;
}
}
