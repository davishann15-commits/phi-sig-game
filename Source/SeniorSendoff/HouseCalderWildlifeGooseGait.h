#pragma once
#include "HouseCalderWildlifeGooseShore.h"
namespace HouseCalderWildlifeGooseGait
{
struct FFoot
{
    FVector Sole=FVector::ZeroVector,Normal=FVector::UpVector,Forward=FVector::ForwardVector;
};
struct FCache
{
    double Start=-1.e30;int32 Route=-1;int64 Cycles[2]={MIN_int64,MIN_int64};
    FFoot Stance[2],Next[2];bool Planted[2]={};int64 CurrentCycles[2]={};
};
using FSampler=TFunctionRef<void(double,HouseCalderWildlifeGoosePaths::FCycleSample&)>;
void Apply(FSampler Evaluate,double Raw,const HouseCalderWildlifeGoosePaths::FCycleSample& Sample,
    const FTransform& Root,bool Medium,FCache& Cache,TArray<FTransform>& Controls);
}
