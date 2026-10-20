// Synthesised once on a worker. No animal recordings or runtime synthesis.
#include "HouseCalderWildlifePrivate.h"
#include "HouseCalderWildlifeBirdCalls.h"

namespace HouseCalderWildlifeSoundPcmImpl
{
struct FNoise
{
    uint32 Seed=20261017;float Next(){Seed=1664525u*Seed+1013904223u;return float(Seed>>8)*(2.f/16777216.f)-1.f;}
};
float Pulse(double Time,double Length)
{return Time<0. || Time>=Length?0.f:float(FMath::Square(FMath::Sin(PI*Time/Length)));}
}
TArray<TArray<int16>> HouseCalderWildlifeSoundPcm::Build(bool BirdCalls)
{
    TArray<TArray<int16>> Bank;Bank.SetNum(5);
    const double Durations[]={.62,.43,60.,12.,.24};
    for(int32 Kind=0;Kind<Bank.Num();++Kind)
    {
        auto& Out=Bank[Kind];const int32 Count=FMath::RoundToInt(Durations[Kind]*Rate);TArray<float> F;F.SetNumZeroed(Count);
        HouseCalderWildlifeSoundPcmImpl::FNoise Noise;Noise.Seed+=uint32(Kind)*104729u;float Low=0.,High=0.,Brown=0.;double Phase=0.;float Peak=.0001f;
        for(int32 I=0;I<Count;++I)
        {
            const double T=double(I)/Rate;const float White=Noise.Next();Low+=.13f*(White-Low);High+=.025f*(White-High);float Value=0.;
            if(Kind==0)
            {
                // A short, forceful exhalation through the nose, no musical grunt.
                const double Env=FMath::Min(1.,T*80.)*FMath::Exp(-T*6.5)*FMath::Min(1.,(Durations[Kind]-T)*25.);
                Value=float(Env)*(Low-High)*3.2f;
            }
            else if(Kind==1)
            {
                const float Env=HouseCalderWildlifeSoundPcmImpl::Pulse(T-.025,.10)+.6f*HouseCalderWildlifeSoundPcmImpl::Pulse(T-.15,.085)+.35f*HouseCalderWildlifeSoundPcmImpl::Pulse(T-.28,.12);
                Value=(White-Low)*Env*.7f+Low*Env*.3f;
            }
            else if(Kind==2)
            {
                // Snowy tree cricket: exactly60 chirps in this60-second loop.
                const double S=FMath::Fmod(T,1.);float Snow=0.;for(int32 P=0;P<4;++P)Snow+=HouseCalderWildlifeSoundPcmImpl::Pulse(S-.20-P*.029,.018);
                Value=Snow*float(FMath::Sin(2.*PI*3100.*T))*.62f;
                // Slower field crickets and sparse cold katydid rasps at55F.
                const double Field=FMath::Fmod(T+.37,2.);for(int32 P=0;P<3;++P)Value+=.16f*HouseCalderWildlifeSoundPcmImpl::Pulse(Field-.18-P*.055,.025)*float(FMath::Sin(2.*PI*4200.*T));
                Value+=.055f*HouseCalderWildlifeSoundPcmImpl::Pulse(FMath::Fmod(T,10.)-4.,.16)*(White-Low);
            }
            else if(Kind==3)
            {
                Brown=FMath::Clamp(Brown+White*.014f,-.7f,.7f);
                Value=Low*.6f+(White-Low)*.08f+Brown*.06f;
            }
            else
            {
                Phase+=2.*PI*(2550.+220.*FMath::Sin(PI*T/Durations[Kind]))/Rate;
                Value=float(FMath::Sin(Phase))*HouseCalderWildlifeSoundPcmImpl::Pulse(T,.24);
            }
            if(Kind==3)Value*=FMath::Min(1.f,float(T/.08))*FMath::Min(1.f,float((Durations[Kind]-T)/.08));
            F[I]=Value;Peak=FMath::Max(Peak,FMath::Abs(Value));
        }
        const float Amplitudes[]={.85f,.5f,.23f,.42f,.3f};Out.SetNumUninitialized(Count);
        for(int32 I=0;I<Count;++I)Out[I]=int16(FMath::Clamp(F[I]/Peak*Amplitudes[Kind],-1.f,1.f)*32760.f);
    }
    if(BirdCalls)HouseCalderWildlifeBirdCalls::Append(Bank);return Bank;
}
