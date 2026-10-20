#include "HouseCalderWildlifeBirdCalls.h"
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeGooseHonkDsp.h"
#include "HouseCalderCorvidRecordingData.inl"
#include "HouseCalderFarLand.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
namespace HouseCalderWildlifeBirdCallsImpl
{
constexpr int32 Rate=24000;
struct FNoise{uint32 Seed=2026101809u;double Next(){Seed=1664525u*Seed+1013904223u;return double(Seed>>8)*(2./16777216.)-1.;}};
double Envelope(double T,double Length,double Attack=.025,double Release=.12)
{return FMath::SmoothStep(0.,Attack,T)*(1.-FMath::SmoothStep(Length-Release,Length,T));}
TArray<int16> Make(int32 Kind)
{
    const double Length=Kind==0?.8:Kind==1?.5:.65;const int32 N=FMath::RoundToInt(Length*Rate);TArray<double> Values;Values.SetNumZeroed(N);FNoise Noise;Noise.Seed+=Kind*104729u;double Low=0.,Soft=0.,Peak=.000001;HouseCalderWildlifeGooseHonkDsp::FVoice Honk;
    for(int32 I=0;I<N;++I)
    {
        const double T=double(I)/Rate,U=T/Length,White=Noise.Next();Low+=.20*(White-Low);Soft+=.035*(White-Soft);double V=0.;
        if(Kind==0)V=(White-Low)*Envelope(T,Length,.035,.2)*(.75+.25*FMath::Sin(PI*U));
        else if(Kind==1)
        {
            const double Body=Envelope(T,Length,.008,.22)*FMath::Exp(-T*7.);
            const double Droplets=FMath::Square(FMath::Sin(2.*PI*17.*T))*FMath::Exp(-T*10.);
            V=Body*(Low*.8+(White-Low)*.18)+Droplets*Soft*.7;
        }
        else V=Honk.Sample(I);
        Values[I]=V;Peak=FMath::Max(Peak,FMath::Abs(V));
    }
    TArray<int16> Out;Out.SetNumUninitialized(N);const double Amplitude=Kind==0?.55:Kind==1?.55:.72;
    for(int32 I=0;I<N;++I)Out[I]=int16(Values[I]/Peak*Amplitude*32760.);return Out;
}
bool FaceAt(const FVector2D& P,const FVector& Source,FVector& Point)
{
    if(FMath::Abs(P.X)<=32000.&&FMath::Abs(P.Y)<=32000.)return false;
    // The town bluff and riverbank also have steep slopes. Restrict echoes to
    // the enclosing valley/cove/north ridges, at least120m above river datum.
    if(P.Y>-65000.&&P.Y<32000.&&FMath::Abs(P.X)<42000.)return false;
    const double Z=HouseCalderFarLand::SkylineGround(P.X,P.Y),Step=200.;
    if(Z<HouseCalderFarLand::RiverDatum+12000.)return false;
    const double X=(HouseCalderFarLand::SkylineGround(P.X+Step,P.Y)-HouseCalderFarLand::SkylineGround(P.X-Step,P.Y))/(2.*Step),Y=(HouseCalderFarLand::SkylineGround(P.X,P.Y+Step)-HouseCalderFarLand::SkylineGround(P.X,P.Y-Step))/(2.*Step);
    if(X*X+Y*Y<.13)return false;Point=FVector(P,Z);const FVector N=FVector(-X,-Y,1.).GetSafeNormal();return FVector::DotProduct(N,(Source-Point).GetSafeNormal())>.05;
}
}
bool HouseCalderWildlifeBirdCalls::Enabled()
{static const bool B=HouseCalderWildlife::BirdsEnabled()||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseAudioTrial"));return B;}
void HouseCalderWildlifeBirdCalls::Append(TArray<TArray<int16>>& Bank)
{
    check(Bank.Num()==5);using namespace HouseCalderWildlifeBirdCallsImpl;
    Bank.Add(Make(0));Bank.Add(Make(1));Bank.Add(Make(2));
    const auto Dry=Bank[Honk];TArray<double> Filtered;Filtered.SetNum(Dry.Num());double Low=0.,DryEnergy=0.,EchoEnergy=0.;
    for(int32 I=0;I<Dry.Num();++I){Low+=.14*(double(Dry[I])-Low);Filtered[I]=Low;DryEnergy+=double(Dry[I])*Dry[I];EchoEnergy+=Low*Low;}
    const double Gain=FMath::Sqrt(DryEnergy/FMath::Max(1.,EchoEnergy))*FMath::Pow(10.,-18./20.);
    for(int32 Delay=2;Delay<=5;++Delay)
    {TArray<int16> Pcm;Pcm.Init(0,Delay*Rate+Dry.Num());for(int32 I=0;I<Dry.Num();++I){Pcm[I]=Dry[I];Pcm[Delay*Rate+I]=int16(FMath::Clamp(Filtered[I]*Gain,-32760.,32760.));}Bank.Add(MoveTemp(Pcm));}
    static_assert(UE_ARRAY_COUNT(HouseCalderCorvidRecordingData::Caw)==34800);
    static_assert(UE_ARRAY_COUNT(HouseCalderCorvidRecordingData::Raven)==46800);
    TArray<int16> CawPcm,RavenPcm;
    CawPcm.Append(HouseCalderCorvidRecordingData::Caw,UE_ARRAY_COUNT(HouseCalderCorvidRecordingData::Caw));
    RavenPcm.Append(HouseCalderCorvidRecordingData::Raven,UE_ARRAY_COUNT(HouseCalderCorvidRecordingData::Raven));
    Bank.Add(MoveTemp(CawPcm));Bank.Add(MoveTemp(RavenPcm));check(Bank.Num()==Count);
}
bool HouseCalderWildlifeBirdCalls::Cue(double FlightAge,int32 Event,int32& Index,int32& Member,double& Age)
{
    if(FlightAge<2.)return false;Index=FMath::FloorToInt((FlightAge-2.)/8.);Member=(Index*7+Event*3)%22;Age=FlightAge-2.-Index*8.;return Age>=0.&&Age<HonkSeconds;
}
double HouseCalderWildlifeBirdCalls::Mouth(double FlightAge,int32 Event,int32 Member)
{
    int32 Index=0,Caller=0;double Age=0.;if(!Cue(FlightAge,Event,Index,Caller,Age)||Caller!=Member)return 0.;
    using namespace HouseCalderWildlifeGooseHonkDsp;
    return FMath::SmoothStep(Start,Start+.025,Age)*(1.-FMath::SmoothStep(End-.065,End,Age));
}
bool HouseCalderWildlifeBirdCalls::CrowCue(double GroupAge,int32 Event,int32 Members,int32& Index,int32& Member,double& Age)
{
    if(GroupAge<6.||Members<1)return false;Index=FMath::FloorToInt((GroupAge-6.)/12.);
    Member=(Index*3+Event)%Members;Age=GroupAge-6.-Index*12.;return Age>=0.&&Age<CawSeconds;
}
double HouseCalderWildlifeBirdCalls::CrowMouth(double GroupAge,int32 Event,int32 Members,int32 Member)
{
    int32 Index=0,Caller=0;double Age=0.;if(!CrowCue(GroupAge,Event,Members,Index,Caller,Age)||Caller!=Member)return 0.;
    const double Frame=Age*100.;const int32 I=FMath::Clamp(FMath::FloorToInt(Frame),0,int32(UE_ARRAY_COUNT(HouseCalderCorvidRecordingData::CawMouth))-1);
    const int32 J=FMath::Min(I+1,int32(UE_ARRAY_COUNT(HouseCalderCorvidRecordingData::CawMouth))-1);
    return FMath::Lerp(double(HouseCalderCorvidRecordingData::CawMouth[I]),double(HouseCalderCorvidRecordingData::CawMouth[J]),Frame-I);
}
bool HouseCalderWildlifeBirdCalls::RidgeEcho(const FVector& Source,FVector& Face,int32& Delay)
{
    using namespace HouseCalderWildlifeBirdCallsImpl;double Best=TNumericLimits<double>::Max();bool Found=false;
    for(int32 Direction=0;Direction<24;++Direction)for(double Radius=20000.;Radius<=100000.;Radius+=10000.)
    {
        const double Angle=Direction*2.*PI/24.;const FVector2D P=FVector2D(Source)+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Radius;FVector At;
        if(!FaceAt(P,Source,At))continue;const double Distance=FVector::DistSquared(At,Source);if(Distance<Best){Best=Distance;Face=At;Found=true;}
    }
    if(!Found)return false;const FVector Centre=Face;
    for(int32 X=-4;X<=4;++X)for(int32 Y=-4;Y<=4;++Y)
    {FVector At;if(!FaceAt(FVector2D(Centre)+FVector2D(X*1000.,Y*1000.),Source,At))continue;const double Distance=FVector::DistSquared(At,Source);if(Distance<Best){Best=Distance;Face=At;}}
    Delay=FMath::Clamp(FMath::RoundToInt(2.*FMath::Sqrt(Best)/34300.),2,5);return true;
}
