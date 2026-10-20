// Analytic sky data prototype; ambient renderer/landing/native acceptance separate.
#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace HouseCalderWildlifeSkyPaths
{
struct FCurve { TArray<FVector> Points;TArray<double> Arc;double Length=0.; };
struct FSample
{
    bool Active=false;int32 Kind=0,Event=0,Member=0;uint32 Seed=0;
    FVector Position=FVector::ZeroVector,Direction=FVector::ForwardVector;
    double Age=0.;float Scale=1.f;
};
struct FPlan
{
    FCurve Goose[2],Crow;TArray<FVector> BatCentres;TArray<double> BatRadii;
    TArray<FBox> Solids;TMap<FIntPoint,TArray<int32>> SolidCells;uint32 Hash=0,RouteHash=0;int32 Samples=0;
    bool SolidSegmentClear(const FVector& A,const FVector& B,double Radius)const;
    bool Build(UWorld* World);
    bool Clear(const FVector& P,double Radius)const;
    bool EvaluateGoose(double Scene,int32 Event,int32 Member,FSample& Out)const;
    bool EvaluateCrow(double Scene,int32 Event,int32 Member,FSample& Out)const;
    bool EvaluateBat(double Scene,int32 Member,FSample& Out)const;
};
FVector Along(const FCurve& Curve,double Distance,FVector* Direction=nullptr);
}
