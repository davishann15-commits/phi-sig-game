#pragma once
#include "CoreMinimal.h"
#include "SeniorDouliIdle.h"

// Use the real wrist-to-palm frame of each selected character, rather than
// assuming that every imported rig has the same hand bone orientation.
struct FSeniorPigHoldPose
{
    static FVector GripOffset(const FReferenceSkeleton& Ref)
    {
        // The leg sits inside the curled fingers, six centimetres from the
        // wrist and slightly below the palm surface. This is also the point
        // measured by the native carry playtest.
        return FSeniorDouliIdle::HandFrame(Ref, true).RotateVector(FVector(6.f, 0.f, -1.5f));
    }
};
