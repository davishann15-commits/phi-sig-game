#pragma once
#include "CoreMinimal.h"
class UWorld;

namespace HouseCalderWildlifePaths
{
struct FGraph
{
    TArray<FVector> Points;
    TArray<TArray<int32>> Links;
    bool Campus=false;
    int32 Edges=0;
};
struct FPlan
{
    TArray<FGraph> Groups;
    TArray<FBox2D> Trunks,Solids,Aprons;
    int32 PathSamples=0;
    uint32 Hash=0;
    bool Build(UWorld* World);
    bool PointClear(const FVector2D& Point) const;
    bool SegmentClear(const FVector& A,const FVector& B) const;
    bool Route(int32 Group,const FVector& From,const FVector& To,TArray<FVector>& Out) const;
};
FVector Evaluate(const TArray<FVector>& Route,double Distance,FVector* Tangent=nullptr);
double Length(const TArray<FVector>& Route);
// Symmetric acceleration and braking, shared by authority and clients.
double TravelSeconds(double Length,double Speed);
double TravelDistance(double Age,double Length,double Speed);
}
