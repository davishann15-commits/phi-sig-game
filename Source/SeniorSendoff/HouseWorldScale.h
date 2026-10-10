#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Maps measured house authoring coordinates into the saved house's world.
 *  Character dimensions and interaction reach deliberately remain unchanged.
 *  The map tag makes an unscaled backup and every other level retain 1:1 behavior.
 */
namespace HouseWorldScale
{
    float Factor(const UWorld* World);
    FVector Point(const UWorld* World, const FVector& AuthoredPoint);
    FVector Unpoint(const UWorld* World, const FVector& WorldPoint);
    FBox Bounds(const UWorld* World, const FBox& AuthoredBounds);
    FTransform Transform(const UWorld* World, const FTransform& AuthoredTransform);
}
