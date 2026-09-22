#pragma once
#include "Animation/AnimInstance.h"
#include "SeniorGarmentAnimInstance.generated.h"

// A full pose copy retains the garment's own buffers for cloth simulation,
// while following the body's corrected shoulders, elbows and torso exactly.
UCLASS(Transient)
class USeniorGarmentAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
