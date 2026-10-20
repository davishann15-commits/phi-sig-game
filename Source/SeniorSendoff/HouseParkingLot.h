#pragma once
#include "CoreMinimal.h"
struct FHallMesh;
class UHouseBlackHallSubsystem;
namespace HouseParking
{
constexpr float West=-7200,East=-3700,South=-2500,North=-300,Surface=-58;
constexpr float AccessWest=-4480,AccessEast=-4160;
void CreateLook(UHouseBlackHallSubsystem* Look);
void Build(FHallMesh& Solid,FHallMesh& Detail);
void ParkCars(TArray<FTransform>& Red,TArray<FTransform>& White,TArray<FTransform>& Blue);
void Landscaping(TArray<FTransform>& ExistingBushes,TArray<FTransform>& NewPlants);
}
