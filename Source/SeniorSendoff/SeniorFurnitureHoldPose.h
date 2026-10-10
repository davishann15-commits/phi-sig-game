#pragma once
#include "CoreMinimal.h"
#include "SeniorDouliIdle.h"
struct FSeniorFurnitureHoldPose
{
    static FVector GripOffset(const FReferenceSkeleton& Ref,bool Right)
    { return FSeniorDouliIdle::HandFrame(Ref,Right).RotateVector(FVector(6.f,0.f,-1.5f)); }
};
