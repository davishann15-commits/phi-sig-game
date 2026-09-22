#pragma once
#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"

// Shared only by the character-selection presentation, never by gameplay.
struct FSeniorSelectionRoom
{
    static float Time() { return FMath::Fmod(float(FPlatformTime::Seconds()-Epoch()),48.f); }
    static double Epoch() { static const double Start=FPlatformTime::Seconds(); return Start; }
    static float Pulse(float T,float Start,float End)
    {
        if(T<=Start || T>=End)return 0;
        return FMath::SmoothStep(0.f,1.f,FMath::Min((T-Start)/.7f,(End-T)/.7f));
    }
    static float Light(float T)
    {
        return 1.f-.18f*Pulse(T,3.f,5.f)*FMath::Square(FMath::Sin((T-3.f)*7.f));
    }
};
