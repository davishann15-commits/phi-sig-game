#pragma once
#include "CoreMinimal.h"
struct FHallMesh;
class UHouseBlackHallSubsystem;

namespace HouseSurroundings
{
bool Enabled();
void CreateLook(UHouseBlackHallSubsystem* Look);
// Architectural trim is scenery, built into shared spatial mesh batches.
void Beam(FHallMesh& M,FName Key,FVector A,FVector B,float Width,float Depth);
/** Room >= 0 glazes it with the interior-mapped room Room (0..3) of Calder State's closed floors (CI_Room<n>Lit/Dark,
 *  HouseCampusInteriors) instead of the flat glass. */
void Window(FHallMesh& M,FVector Base,FVector Right,FVector Out,float Width,float Height,bool Lit,int32 Room=-1);
void RoofServices(FHallMesh& M,const FBox2D& Bounds,float RoofZ,int Seed,FName SignKey=NAME_None);
void Street(FHallMesh& M);
void Garden(TArray<FTransform>& Shrubs);
}
