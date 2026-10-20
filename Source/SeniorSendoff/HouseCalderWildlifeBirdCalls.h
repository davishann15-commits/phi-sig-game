#pragma once
#include "CoreMinimal.h"
namespace HouseCalderWildlifeBirdCalls
{
constexpr int32 Hiss=5,Splash=6,Honk=7,FirstEcho=8,Caw=12,Raven=13,Count=14;
constexpr double HonkSeconds=.65;
constexpr double CawSeconds=1.45,RavenSeconds=1.95;
bool Enabled();
// Original twelve synthesized buffers are untouched. The owner's NPS-recorded
// crow/raven exception is copied into this same bank once on the worker.
void Append(TArray<TArray<int16>>& Bank);
bool Cue(double FlightAge,int32 Event,int32& Index,int32& Member,double& Age);
double Mouth(double FlightAge,int32 Event,int32 Member);
bool CrowCue(double GroupAge,int32 Event,int32 Count,int32& Index,int32& Member,double& Age);
double CrowMouth(double GroupAge,int32 Event,int32 Count,int32 Member);
// Actual analytic outer-land faces; no physics queries. Returns false if none.
bool RidgeEcho(const FVector& Source,FVector& Face,int32& Delay);
}
