// Shared by worker-thread PCM generation and the small offline preview tool.
// Authored synthesis; no animal recording or third-party asset is embedded.
#pragma once
#include <cmath>
#include <cstdint>
namespace HouseCalderWildlifeGooseHonkDsp
{
constexpr double Rate=24000.,Length=.65,Start=.012,Change=.195,End=.365;
constexpr double Pi=3.14159265358979323846;
inline double Smooth(double A,double B,double T)
{const double U=T<A?0.:T>B?1.:(T-A)/(B-A);return U*U*(3.-2.*U);}
inline double Bell(double Hz,double Centre,double Width)
{const double D=(Hz-Centre)/Width;return std::exp(-.5*D*D);}
struct FVoice
{
    uint32_t Seed=2026101821u;double RattlePhase=0.,CallPhase=0.,Breath=0.,Drift=0.,CycleJitter=0.;
    double Noise(){Seed=1664525u*Seed+1013904223u;return double(Seed>>8)*(2./16777216.)-1.;}
    double Sample(int Index)
    {
        const double T=Index/Rate,White=Noise();
        Breath+=.22*(White-Breath);Drift+=.002*(White-Drift);
        if(T<Start||T>=End)return 0.;
        // A rough low-register onset changes register into a shorter nasal
        // call. Independent phases avoid a continuous electronic pitch sweep.
        const double A=Smooth(Start,Start+.017,T)*(1.-Smooth(End-.065,End,T));
        const double Register=Smooth(Change-.009,Change+.009,T);
        const double U=Smooth(Change,End,T);
        const double RattleHz=145.+10.*std::sin(Pi*Smooth(Start,Change,T))+5.*Drift;
        const double CallHz=665.+105.*std::sin(Pi*U)-68.*U+9.*Drift;
        const double Old=RattlePhase;RattlePhase+=2.*Pi*RattleHz*(1.+CycleJitter*.012)/Rate;
        if(int(RattlePhase/(2.*Pi))!=int(Old/(2.*Pi)))CycleJitter=Noise();
        CallPhase+=2.*Pi*CallHz*(1.+CycleJitter*.003)/Rate;
        double Rattle=0.,Call=0.;
        for(int H=1;H<=24;++H)
        {
            const double F=H*RattleHz;
            const double W=(.07+.85*Bell(F,1030.,330.)+.32*Bell(F,1800.,420.))*std::exp(-F/5400.);
            Rattle+=W*std::sin(H*RattlePhase+.075*std::sin(H*1.71));
        }
        for(int H=1;H<=13;++H)
        {
            const double F=H*CallHz;
            if(F>10500.)break;
            const double W=(.10+.85*Bell(F,1480.-110.*U,440.)+.24*Bell(F,2950.,630.)+.07*Bell(F,4400.,800.))*std::exp(-F/6500.);
            Call+=W*std::sin(H*CallPhase+.05*std::sin(H*2.13));
        }
        const double Roughness=1.+.10*std::sin(RattlePhase*.5)+.04*CycleJitter;
        const double Pressure=.62+.38*Smooth(Start,.105,T);
        const double V=(1.-Register)*.36*Rattle*Roughness+Register*.82*Call;
        // Breath follows vocal pressure, and disappears with the call.
        // Soft pressure limiting fills out the call instead of allowing a few
        // phase-aligned partials to set the normalization of the whole buffer.
        return A*Pressure*(std::tanh(2.2*V)+.045*Breath);
    }
};
}
