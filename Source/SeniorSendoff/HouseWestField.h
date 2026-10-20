#pragma once
#include "CoreMinimal.h"
struct FHallMesh;
class UHouseBlackHallSubsystem;

namespace HouseWestField
{
constexpr float MoundTop=-161.2f; // Actual west court mound: court floor - 3 + 200 + .8.
constexpr float FenceY=4520.f, FenceWest=-19300.f, FenceEast=-3900.f;
constexpr float GateWest=-7320.f, GateEast=-6780.f;
float RaisedHeight(float X,float Y,float Original);
float JoinBank(float X,float Y,float OriginalSurface);
void CreateLook(UHouseBlackHallSubsystem* Look);
void Fence(FHallMesh& Solid,FHallMesh& Detail);
}
