#pragma once
#include "HouseCalderWildlifeGoosePaths.h"
#include "HouseCalderWildlifeSkyRuntime.h"
#include "HouseCalderWildlifeGooseGait.h"
struct FHouseCalderPondSlot : FHouseCalderSkySlot
{
    HouseCalderWildlifeGooseGait::FCache Feet;
};
struct FHouseCalderWildlifePondRuntime
{
    HouseCalderWildlifeGoosePaths::FPlan Plan;
    HouseCalderWildlifeGooseShore::FPlan Shore;
    bool ShoreEnabled=false,ReactionsEnabled=false;
    TArray<HouseCalderWildlifeGooseShore::FReaction> Reactions;
#if !UE_BUILD_SHIPPING
    double ChangeWall[16]={};
#endif
    bool Sample(double Raw,int32 Cohort,int32 Member,HouseCalderWildlifeGoosePaths::FCycleSample& Out)const
    {
        const int32 Id=Cohort*8+Member;
        if(ReactionsEnabled&&Reactions.IsValidIndex(Id)&&Reactions[Id].Mode>=64&&Raw<Reactions[Id].Expiry)return Shore.Reaction(Plan,Raw,Member,Reactions[Id],Out);
        return ShoreEnabled?Shore.Cycle(Plan,Raw,Cohort,Member,Out):Plan.Cycle(Raw,Cohort,Member,Out);
    }
    TArray<FHouseCalderPondSlot> Slots;
    TArray<FTransform> Current,Previous;
    TArray<FTransform> EffectCurrent[4],EffectPrevious[4];TArray<float> EffectData[4];
    bool EffectsAttempted=false,EffectsReady=false,EffectsFailed=false,EffectVisible[4]={};
    double EffectHideAfter[4]={},EffectNextBounds=0.,EffectKey=-1.,EffectStatsAt=0.,WaterExitArc[2]={-1.,-1.};
    int32 EffectShown[4]={};
    double ShoreSplashBirth(double Raw,int32 Id)const
    {
        if(Id<0||Id>=16||Id%8<6||!Reactions.IsValidIndex(Id)||WaterExitArc[Id%8-6]<0.)return -1.e30;
        const double Arc=WaterExitArc[Id%8-6];const auto& R=Reactions[Id];double Birth=-1.e30;
        if(R.Mode==64&&R.Arc>Arc&&Raw<R.Expiry)Birth=R.Start+.8+(R.Arc-Arc)/35.;
        else if(R.Mode<64&&Id/8==int32((FMath::FloorToInt64(Raw/1800.)%2+2)%2))Birth=Shore.HomeDeadline(Plan,Raw,Id/8)-Arc/35.;
        return Birth+.65;
    }
    FTransform EffectContactKeys[16],EffectContacts[16];bool EffectContactValid[16]={};
    FVector EffectLastWaterPosition[16],EffectWetCentre[32];
    double EffectLastWaterMotion[16]={},EffectClock=-1.e30;
    bool EffectWasWater[16]={},EffectWetValid[32]={},EffectWet[32]={};
    FVector HiddenAnchors[2]={};
    bool Attempted=false,Ready=false,Failed=false,Visuals=false,Visible=false;
    double NextBounds=0.,HideAfter=0.,StatsAt=0.,NextAuthority=0.,LastClockStamp=-1.;int32 Received=0,Shown=0,OnWater=0;
};
