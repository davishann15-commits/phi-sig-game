#pragma once
#include "HouseCalderWildlifeGoosePaths.h"
class UWorld;
namespace HouseCalderWildlifeGooseShore
{
// Pure override data. The director will authorize and replicate real events;
// this struct never selects threats or changes any network state.
struct FReaction {uint8 Mode=0;double Start=0.,Arc=0.,Yaw=0.,Expiry=1.e30;};
struct FPlan
{
    HouseCalderWildlifeSkyPaths::FCurve Routes[2];
    TArray<FBox> WaterWorks;uint32 Hash=0;int32 Nodes=0,Samples=0,Rejected=0;
    bool Cycle(const HouseCalderWildlifeGoosePaths::FPlan& Pond,double Raw,int32 Cohort,int32 Member,HouseCalderWildlifeGoosePaths::FCycleSample& Out)const;
    bool At(int32 Route,double Arc,bool Reverse,HouseCalderWildlifeGoosePaths::FCycleSample& Out)const;
    bool Reaction(const HouseCalderWildlifeGoosePaths::FPlan& Pond,double Raw,int32 Member,const FReaction& State,HouseCalderWildlifeGoosePaths::FCycleSample& Out)const;
    bool Project(int32 Route,const FVector& Point,double& Arc,double& Error)const;
    double HomeDeadline(const HouseCalderWildlifeGoosePaths::FPlan& Pond,double Raw,int32 Cohort)const;
    bool Build(UWorld* World,const HouseCalderWildlifeSkyPaths::FPlan& Sky,const HouseCalderWildlifeGoosePaths::FPlan& Pond);
};
}
