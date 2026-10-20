#pragma once
#include "HouseCalderWildlifeSkyPaths.h"
#include "HouseCalderWildlifeCrowRoost.h"
struct FHouseCalderSkySlot
{
    TArray<FTransform> Controls;
    uint32 Seed=0;double Scene=-1.,PoseKey=-1.,NextRoot=0.;bool Visible=false,Settled=true;
};
struct FHouseCalderWildlifeSkyRuntime
{
    HouseCalderWildlifeSkyPaths::FPlan Plan;
    HouseCalderWildlifeCrowRoost::FPlan CrowRoost;
    bool Attempted=false,PlanReady=false,Ready=false,Failed=false,Visible[4]={};
    #if !UE_BUILD_SHIPPING
    double ReviewScene=-1.;
#endif
    double HideAfter[4]={},NextBounds=0.,StatsAt=0.;
    TArray<FHouseCalderSkySlot> Slots[4];
    TArray<FTransform> Current[4],Previous[4];
    int32 Shown[4]={},BatchCalls=0,BoundsCalls=0,Frames=0;
};
