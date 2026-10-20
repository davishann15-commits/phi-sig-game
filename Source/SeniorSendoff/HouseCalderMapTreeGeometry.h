#pragma once
#include "CoreMinimal.h"
class UStaticMesh;
class UHouseBlackHallSubsystem;
namespace HouseCalderMapTreeGeometry
{
/** New layered conifer: continuous tapered wood, irregular branch tiers and
 * curved photographed needle sprays; three independent geometry LODs. */
UStaticMesh* Conifer(UHouseBlackHallSubsystem* Look);
UStaticMesh* Deadwood(UHouseBlackHallSubsystem* Look,UStaticMesh* Original,const FString& Name);
}
