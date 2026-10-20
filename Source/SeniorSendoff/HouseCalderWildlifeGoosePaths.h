#pragma once
#include "HouseCalderWildlifeSkyPaths.h"
class UWorld;
namespace HouseCalderWildlifeGoosePaths
{
constexpr int32 LandingCount=8;
struct FCycleSample
{
    HouseCalderWildlifeSkyPaths::FSample Bird;
    // 0inactive,1resident water,2arrival,3water brake,4swim home,5depart.
    int32 Phase=0;bool OnWater=false;double Flap=0.,Fold=1.,Neck=1.;
    // Optional shore layer only:6bank rest,7outward,8back to water.
    double Stand=0.,ShoreArc=-1.,WalkAge=-1.,ShoreSpeed=0.,WaterSurface=0.,Mouth=0.;int32 ShoreRoute=-1;
};
struct FPlan
{
    HouseCalderWildlifeSkyPaths::FCurve Arrivals[LandingCount],Departures[LandingCount];
    double SplitScene=0.,Durations[LandingCount]={},StartSpeeds[LandingCount]={},DepartureDurations[LandingCount]={};
    TArray<FVector> Touches,Homes,Brakes;uint32 Hash=0;int32 Samples=0,Rejected=0;
    bool Build(UWorld* World,const HouseCalderWildlifeSkyPaths::FPlan& Sky);
    bool Cycle(double Scene,int32 Cohort,int32 Member,FCycleSample& Out)const;
    bool Arrival(double Scene,int32 Member,HouseCalderWildlifeSkyPaths::FSample& Out)const;
};
}
