#pragma once

#include "CoreMinimal.h"
struct FHallMesh;
class UHouseBlackHallSubsystem;

/** Saved PBR masonry and batched, shallow decorative bricks. Does not change wall collision. */
namespace HouseBrick
{
    void CreateLook(UHouseBlackHallSubsystem* Look);
    void Header(FHallMesh& Mesh, const FVector& Left, const FVector& Right, const FVector& Normal, float Height);
    void Arch(FHallMesh& Mesh, float CentreX, float SpringZ, float Radius, float Y, float Sign, float TopZ);
}
