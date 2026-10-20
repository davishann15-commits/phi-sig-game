#pragma once
#include "CoreMinimal.h"
struct FHallMesh;
class UHouseBlackHallSubsystem;

/** Deterministic rear woodland scenery, built locally with the house exterior. */
namespace HouseWoods
{
constexpr float XMin=-8200.f,XMax=8200.f,YMin=4600.f,YMax=19200.f;
constexpr float HutX=-800.f,HutY=13600.f,HutFloor=-344.f;
struct FHollow { FVector2D Center; float Radius,Depth; };
const TArray<FHollow>& Hollows();
const TArray<FVector2D>& Trail();
const TArray<FVector2D>& SmoothTrail();
float Height(float X,float Y);
float TrailDistance(float X,float Y);
bool TreeAllowed(float X,float Y);
void FitTrees(TArray<FTransform>& Big,TArray<FTransform>& Small,TArray<FTransform>& Distant);
void CreateLook(UHouseBlackHallSubsystem* Look);
void Terrain(FHallMesh& Mesh);
void Hut(FHallMesh& Mesh);
struct FScatter
{
 TArray<FTransform> Logs,Branches,Flowers,Rocks,Leaves,Shrubs,GrassGreen,GrassDry;
};
FScatter Scatter();
void FallenLog(FHallMesh& Mesh,int Variant,TArray<FBox>& Collision);
void BranchPile(FHallMesh& Mesh,int Variant);
void Flowers(FHallMesh& Mesh,int Variant);
void Rock(FHallMesh& Mesh,int Variant,TArray<FBox>& Collision);
void ThickGrass(FHallMesh& Mesh,bool Dry,int Variant);
}
