#pragma once
#include "CoreMinimal.h"
struct FHallMesh;
class UHouseBlackHallSubsystem;

// World coordinates of the already-scaled exterior. The saved house is untouched.
namespace HouseCourt
{
    constexpr float Floor = -359.f;
    constexpr float MidY = 2820.f;
    inline FVector Rim(int32 End) { return FVector(End == 0 ? -1150.f : 1150.f, MidY, Floor + 305.f); }
    inline FVector BallHome() { return FVector(-260.f, 2720.f, Floor + 15.f); }
}
void BuildHouseBasketballCourt(FHallMesh& Solid, FHallMesh& Decor, UHouseBlackHallSubsystem* Look);
