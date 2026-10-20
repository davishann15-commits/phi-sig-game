#pragma once

#include "CoreMinimal.h"

class UHouseBlackHallSubsystem;
class AActor;
class UWorld;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Calder appearance only. Terrain/placements and the legacy comparison stay intact. */
namespace HouseCalderLandscape
{
bool Enabled();
/** Optional owned rock appearance; all non-rock scan sets retain the original master. */
UMaterialInterface* RockMaster(const TCHAR* Set);
void RockLook(UMaterialInstanceDynamic* Material, const TCHAR* Set);
void WoodsFloorLook(UHouseBlackHallSubsystem* Look);
void WoodsBarkLook(UHouseBlackHallSubsystem* Look);
void WoodsScannedProps(AActor* Owner);
/** Screenshot-only close-ups of actual planted props, with a traced 130cm eye. */
bool RockDetailView(UWorld* World, FName View, FVector& Eye, FVector& Target);
bool WoodsPropMacro(UWorld* World, FName View, FVector& Eye, FVector& Target);
}
