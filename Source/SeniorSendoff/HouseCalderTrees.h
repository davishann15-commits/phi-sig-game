#pragma once
#include "CoreMinimal.h"
class AActor;
class UStaticMesh;
class UHouseBlackHallSubsystem;
namespace HouseCalderTrees
{
bool Enabled();
UStaticMesh* WoodsMesh(AActor* Owner,UHouseBlackHallSubsystem* Look,const FString& Name,
    UStaticMesh* Original,const TArray<FTransform>& Instances);
void AppendWindKeys(TArray<FName>& Keys);
}
