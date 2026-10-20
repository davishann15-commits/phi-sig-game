// Independent roost routes: original goose/sky plan and tower crows stay intact.
#pragma once
#include "HouseCalderWildlifeSkyPaths.h"
namespace HouseCalderWildlifeCrowRoost
{
constexpr int32 Slots=15,Events=12;
constexpr double Period=1800.,Interval=150.,Speed=1200.,Radius=80.;
struct FTree
{
    FString Batch;int32 Instance=0;FTransform Transform;FBox Canopy;
    FVector End=FVector::ZeroVector;
};
struct FPlan
{
    TArray<FTree> Trees,Targets;
    HouseCalderWildlifeSkyPaths::FCurve Routes[Slots];
    uint32 Hash=0;int32 Samples=0,RejectedCandidates=0;
    double LaunchSpacing=1.,MaximumGroupDuration=0.,MinimumPairDistance=1.e20;
    bool Build(UWorld* World,const HouseCalderWildlifeSkyPaths::FPlan& Sky);
    bool Evaluate(double Scene,int32 Event,int32 Member,HouseCalderWildlifeSkyPaths::FSample& Out)const;
};
int32 Count(int32 Event);
}
