#pragma once
#include "CoreMinimal.h"
class AActor;
class UStaticMesh;
class UHouseBlackHallSubsystem;
struct FHallMesh;
struct FHallSection;
namespace HouseCalderMapTrees
{
/** Owner-authorized map-wide visual replacement; original plants and bodies
 * remain the physical authority. Disabled by the normal landscape A/B gates. */
bool Enabled();
bool TreeBatch(const FString& Name);
UStaticMesh* Mesh(AActor* Owner,UHouseBlackHallSubsystem* Look,const FString& Name,
    UStaticMesh* Original,const TArray<FTransform>& Instances);
void AppendWindKeys(TArray<FName>& Keys);
void Canopy(UStaticMesh* Mesh,TArray<FVector>& Leaves);
bool Bark(UStaticMesh* Mesh,FHallSection& Out);
/** Replace baked town crowns while retaining the old builder's RNG draws and
 * exact tree feet. Uses the town's existing static tiles, adding no ISMs. */
bool TownTree(FHallMesh& Into,const FHallMesh& Original,const FVector& Foot,bool Evergreen,int32 Tone);
void TownLook(UHouseBlackHallSubsystem* Look);
}
