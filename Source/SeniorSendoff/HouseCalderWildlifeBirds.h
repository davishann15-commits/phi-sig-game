#pragma once
#include "CoreMinimal.h"
class UStaticMesh;
class UMaterialInterface;
namespace HouseCalderWildlifeBirds
{
// Kind0 goose,1 acquired crow,2 brown bat,3 red bat. No runtime skeleton.
UStaticMesh* Build(UObject* Outer,int32 Kind,UMaterialInterface* Material);
void Pose(int32 Kind,double Seconds,const FTransform& Root,TArray<FTransform>& Controls,double Mouth=0.,double Flap=1.,double Fold=0.,double Neck=0.);
void GroundLegs(const FTransform& Root,const FVector* Soles,const FVector* Normals,TArray<FTransform>& Controls,const FVector* Forwards=nullptr);
void Pack(const TArray<FTransform>& Controls,float* Data);
double Frequency(int32 Kind);
}
