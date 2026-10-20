#pragma once
#include "CoreMinimal.h"
class AActor;
class AHouseLeafWind;
class UHouseBlackHallSubsystem;

/** Cosmetic ground cover only; separate seeded placement, ordinary ISMs,
 *  no collision, and no instances in any frozen/protected rectangle. */
namespace HouseCalderCover
{
int32 Build(AActor* Owner, UHouseBlackHallSubsystem* Look);
/** Wait for existing campus-polish asynchronous collision, without blocking
 *  the game thread, then scatter once and attach to the shared wind clock. */
void ScheduleBuild(AActor* Owner, UHouseBlackHallSubsystem* Look);
void RegisterWind(AHouseLeafWind* Wind, UHouseBlackHallSubsystem* Look);
}
