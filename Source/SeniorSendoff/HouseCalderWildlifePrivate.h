#pragma once
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifePaths.h"
#include "HouseCalderWildlifeSkyRuntime.h"
#include "HouseCalderWildlifePondRuntime.h"
#include "Async/Future.h"

struct FHouseCalderWildlifeAnimal
{
    struct FStanceMemo {double Time=0.;FTransform Pose;bool Valid=false;};
    TArray<FVector> Route;
    FVector Home=FVector::ZeroVector;
    float Scale=1.f;
    double ComeBackAt=0.,AlarmUntil=0.,LastPose=-100.,LastRoot=-100.,NextIdleSoleSample=0.,NextEscapeCheck=0.;
    bool Escaped=false,WasMoving=false;
    uint8 LastPoseTier=0;
    bool Planted[4]={};
    int64 CurrentFootCycles[4]={};
    double KeyStart=-100.;
    TArray<FTransform> KeyControls[2];
    TArray<FTransform> LocalKeyControls[2];
    FTransform KeyRoot[2],KeyTail[2],KeyAntler[2],KeyContact[2];
    FTransform LocalKeyTail[2],LocalKeyAntler[2],LocalKeyContact[2];
    double KeyScene[2]={-1.,-1.};float KeyScale=-1.f;
    FHouseCalderWildlifeState KeyState;bool KeyStateValid=false;
    mutable FStanceMemo StanceMemo[4][3];mutable int32 StanceCursor[4]={};
    mutable FHouseCalderWildlifeState StanceState;mutable bool StanceStateValid=false;mutable float StanceScale=-1.f;
    mutable int64 MemoHits=0,MemoMisses=0,CacheChecks=0;int64 ReusedKeys=0;
    mutable double CachePositionErrorCm=0.,CacheAngleErrorDegrees=0.,CacheScaleError=0.;
    FTransform MediumStance[4],MediumNextStance[4];
    int64 MediumCycles[4]={MIN_int64,MIN_int64,MIN_int64,MIN_int64};
    double MediumStateStart=-1.;uint8 MediumStateMood=255;
    TArray<FTransform> Controls;
    FVector Hooves[4]={};
    FVector Anchors[4]={};
    int64 FootCycles[4]={MIN_int64,MIN_int64,MIN_int64,MIN_int64};
};
struct FHouseCalderWildlifeVoice
{
    int32 Kind=-1,Species=-1;double Until=0.;float Gain=0.,Applied=-1.;FVector At=FVector::ZeroVector;
    bool Overhead=false;double Started=0.;int32 FollowEvent=-1,FollowMember=-1;
};
struct FHouseCalderWildlifeSound
{
    TFuture<TArray<TArray<int16>>> Task;TArray<TArray<int16>> Pcm;TArray<double> Rms;
    TMap<uint16,float> SeenStarts,PondSeenStarts;FHouseCalderWildlifeVoice Voices[6];int32 Cursors[2]={};
    int64 LastHonk=MIN_int64,LastCrow=MIN_int64,LastRaven=MIN_int64;double LastShoreSplash[16]={};
    int32 GooseHiss=0,GooseSplash=0,GooseHonk=0,GooseEcho=0,MaxSpecies=0;
    double Started=-1.,NextCrunch=0.,NextStats=0.,RecordUntil=-1.,NextPeeperScene=-1.,LastScene=-1.;
    int32 PeakVoices=0,PeakSources=0,Calls=0,MissedDeadlines=0;bool Ready=false,Dumped=false,Recording=false,Recorded=false,SubmixVerified=false;
};
namespace HouseCalderWildlifeSoundPcm
{
constexpr int32 Rate=24000;
TArray<TArray<int16>> Build(bool BirdCalls=false);
}
struct FHouseCalderWildlifeSoleWindow
{
    TArray<double> Gaps,Drifts;int32 GapCursor=0,DriftCursor=0;
};
namespace HouseCalderWildlifeStats
{
inline void Sample(TArray<double>& Values,int32& Cursor,double Value)
{
    if(Values.Num()<4096)Values.Add(Value);
    else {Values[Cursor]=Value;Cursor=(Cursor+1)%4096;}
}
}
struct FHouseCalderWildlifeRuntime
{
    HouseCalderWildlifePaths::FPlan Plan;
    FHouseCalderWildlifeSkyRuntime Sky;
    FHouseCalderWildlifePondRuntime Pond;
    FHouseCalderWildlifeSound Sound;
    TArray<FHouseCalderWildlifeAnimal> Animals;
    TMap<TWeakObjectPtr<APawn>,TPair<FVector,double>> StillPawns;
    TArray<FTransform> Current[4],Previous[4];
    TArray<float> Custom;
    TArray<double> TickMs,HoofGaps,HoofDrifts,ChangeWallTimes;
    int32 TickCursor=0,GapCursor=0,DriftCursor=0;TMap<uint8,FHouseCalderWildlifeSoleWindow> SoleWindows;
    FName TimingView=TEXT("gameplay");TMap<FName,TArray<double>> ViewTickMs;TMap<FName,int32> ViewTickCursors;
    TArray<FVector> Soles[4];
    bool Graph=false,Failed=false,Visuals=false,Dirty=true,Visible=false;
    double CheckAt=0.,LastChange=0.,NextBounds=0.,HideAfter=0.,StatsAt=0.;
    int32 Changes=0,Received=0,Alarms=0;
};
namespace HouseCalderWildlifePose
{
// GPU Scene carries at most255 custom floats. Positive-W quaternion XYZ
// plus translation uses6 floats/control;21 current/previous controls + scale=253.
constexpr int32 Controls=21,CurrentFloats=Controls*6,CustomFloats=CurrentFloats*2+1;
static_assert(CustomFloats<=255);
double Speed(ECalderWildlifeMood Mood);
bool SameInputs(const FHouseCalderWildlifeState& A,const FHouseCalderWildlifeState& B);
bool CacheReview();
void CheckCache(const FHouseCalderWildlifeAnimal& Animal,const FTransform& Cached,const FTransform& Fresh);
FTransform Frame(const FHouseCalderWildlifeState& State,const FHouseCalderWildlifeAnimal& Animal,double Now);
double AnimationPhase(const FHouseCalderWildlifeState& State,const FHouseCalderWildlifeAnimal& Animal,double Now);
struct FFootPhase {bool Moving=false,Planted=true;int64 Cycle=0;double Touch=0.,Lift=0.,NextTouch=0.,Local=0.;};
FFootPhase FootPhase(const FHouseCalderWildlifeState& State,const FHouseCalderWildlifeAnimal& Animal,int32 Leg,double Now);
FTransform StanceSole(const FHouseCalderWildlifeState& State,const FHouseCalderWildlifeAnimal& Animal,int32 Leg,double TouchTime);
FVector Root(const FHouseCalderWildlifeState& State,const FHouseCalderWildlifeAnimal& Animal,double Now,FVector* Direction=nullptr);
void Evaluate(const FHouseCalderWildlifeState& State,FHouseCalderWildlifeAnimal& Animal,double Now,double SceneNow,
    FTransform& RootPose,FTransform& Tail,FTransform& Antlers,FTransform& Contact,TArray<FTransform>& Controls);
}
