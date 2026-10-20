// Landing data only. Local/server population handoff and native pose acceptance
// are separate gates; no live landing behavior is enabled by this file.
#include "HouseCalderWildlifeGoosePaths.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderFarLand.h"
#include "HouseCalderLandform.h"
#include "HouseCalderWater.h"
#include "Misc/Crc.h"
namespace HouseCalderWildlifeGoosePathsImpl
{
double Ground(const FVector& P){return FMath::Abs(P.X)<=32000. && FMath::Abs(P.Y)<=32000.?HouseCalderTerrain::SurfaceZ(P.X,P.Y):HouseCalderFarLand::SkylineGround(P.X,P.Y);}
void Curve(HouseCalderWildlifeSkyPaths::FCurve& Out,const FVector& A,const FVector& B,const FVector& C,const FVector& D)
{
    Out={};const int32 Count=FMath::CeilToInt((FVector::Distance(A,B)+FVector::Distance(B,C)+FVector::Distance(C,D))/40.);
    for(int32 I=0;I<=Count;++I){const double T=double(I)/Count,U=1.-T;const FVector P=A*(U*U*U)+B*(3*U*U*T)+C*(3*U*T*T)+D*(T*T*T);if(I)Out.Length+=FVector::Distance(Out.Points.Last(),P);Out.Points.Add(P);Out.Arc.Add(Out.Length);}
}
}
bool HouseCalderWildlifeGoosePaths::FPlan::Build(UWorld*,const HouseCalderWildlifeSkyPaths::FPlan& Sky)
{
    Samples=Rejected=0;Touches.Reset();Homes.Reset();Brakes.Reset();const double Began=FPlatformTime::Seconds();
    // Split north of Gander Pond, where a shallow descent can clear its shore.
    int32 At=0;double Distance=1.e30;for(int32 I=0;I<Sky.Goose[1].Points.Num();++I){const double D=FMath::Abs(Sky.Goose[1].Points[I].Y-31000.);if(D<Distance){At=I;Distance=D;}}
    SplitScene=1020.+Sky.Goose[1].Arc[At]/1800.;FString Canonical;double Longest=0.;
    for(int32 I=0;I<LandingCount;++I)
    {
        HouseCalderWildlifeSkyPaths::FSample From;if(!Sky.EvaluateGoose(SplitScene,1,30-LandingCount+I,From))return false;
        HouseCalderWildlifeSkyPaths::FSample Next;if(!Sky.EvaluateGoose(SplitScene+.001,1,30-LandingCount+I,Next))return false;
        const FVector Velocity=(Next.Position-From.Position)/.001;StartSpeeds[I]=Velocity.Size();
        const FVector Touch(24800.+(I%2?225.:-225.),-13450.+(I/2)*450.,HouseCalderLandform::PondWaterZ()+14.*From.Scale);Touches.Add(Touch);
        const FVector EndForward=FVector(-.15,-1.,0).GetSafeNormal();
        HouseCalderWildlifeGoosePathsImpl::Curve(Arrivals[I],From.Position,From.Position+Velocity.GetSafeNormal()*13000.,Touch-EndForward*10000.+FVector(0,0,1200.),Touch);
        Longest=FMath::Max(Longest,Arrivals[I].Length/1025.);
        const FVector Brake=Touch+EndForward*427.5;Brakes.Add(Brake);
        const FVector Home=Brake+FVector(I%2?1400.:1200.,0,0);Homes.Add(Home);
        const FVector ExitForward=FVector(.6,-.8,0).GetSafeNormal(),End=Home+ExitForward*75000.;const FVector Exit(End.X,End.Y,HouseCalderWildlifeGoosePathsImpl::Ground(End)+10000.);
        HouseCalderWildlifeGoosePathsImpl::Curve(Departures[I],Home,Home+ExitForward*2500.+FVector(0,0,700.),Home+ExitForward*17000.+FVector(0,0,6500.),Exit);
        DepartureDurations[I]=Departures[I].Length/1250.;
    }
    for(int32 I=0;I<LandingCount;++I)Durations[I]=Longest+(I/2)*4.;
    for(int32 Role=0;Role<2;++Role)for(int32 I=0;I<LandingCount;++I)
    {
        const auto& Path=Role==0?Arrivals[I]:Departures[I];FVector Before=Path.Points[0];
        for(int32 P=0;P<Path.Points.Num();++P)
        {
            const FVector Here=Path.Points[P];bool Clear=Sky.SolidSegmentClear(Before,Here,115.);double Gap=1.e30;
            for(const FVector2D Offset:{FVector2D(0,0),FVector2D(115,0),FVector2D(-115,0),FVector2D(0,115),FVector2D(0,-115)})Gap=FMath::Min(Gap,Here.Z-HouseCalderWildlifeGoosePathsImpl::Ground(Here+FVector(Offset,0)));
            Clear&=Gap>=45.;++Samples;if(!Clear){if(Rejected<12)UE_LOG(LogTemp,Warning,TEXT("CALDER_GOOSE_PATH_REJECT role=%d member=%d point=%d at=%s terrainGap=%.3f"),Role,I,P,*Here.ToString(),Gap);++Rejected;}
            Canonical+=FString::Printf(TEXT("%.3f,%.3f,%.3f;"),Here.X,Here.Y,Here.Z);Before=Here;
        }
        const FVector Touch=Touches[I];float Water=0.f;const bool Pond=HouseCalderWater::WaterAt(FVector2D(Touch),Water)&&FMath::Abs(Water-HouseCalderLandform::PondWaterZ())<.1f;float Deck=0.f;if(!Pond || HouseCalderWater::DeckAt(FVector2D(Touch),Deck))++Rejected;
    }
    // Water braking and the later slow swim home stay inside the pond, clear
    // of decks and with the full115cm horizontal wing/body margin.
    for(int32 I=0;I<LandingCount;++I)for(int32 Segment=0;Segment<2;++Segment)
    {
        const FVector A=Segment==0?Touches[I]:Brakes[I],B=Segment==0?Brakes[I]:Homes[I];const int32 Steps=FMath::CeilToInt(FVector::Distance(A,B)/35.);
        for(int32 Step=0;Step<=Steps;++Step)
        {
            const FVector P=FMath::Lerp(A,B,double(Step)/Steps);bool Good=Sky.SolidSegmentClear(P,P,115.);
            for(const FVector2D Offset:{FVector2D(0,0),FVector2D(115,0),FVector2D(-115,0),FVector2D(0,115),FVector2D(0,-115)})
            {float Water=0.f,Deck=0.f;Good&=HouseCalderWater::WaterAt(FVector2D(P)+Offset,Water) && FMath::Abs(Water-HouseCalderLandform::PondWaterZ())<.1f && !HouseCalderWater::DeckAt(FVector2D(P)+Offset,Deck);}
            ++Samples;if(!Good){if(Rejected<12)UE_LOG(LogTemp,Warning,TEXT("CALDER_GOOSE_PATH_REJECT role=water member=%d point=%d at=%s"),I,Step,*P.ToString());++Rejected;}Canonical+=P.ToString();
        }
    }
    Hash=FCrc::StrCrc32(*Canonical);UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_PATHS clear=%d/%d rejected=%d splitScene=%.6f landingCount=8 restContinue=22 hash=%08x physicsQueries=0 dataOnly=1 buildMs=%.2f"),Samples-Rejected,Samples,Rejected,SplitScene,Hash,(FPlatformTime::Seconds()-Began)*1000.);return Rejected==0;
}
bool HouseCalderWildlifeGoosePaths::FPlan::Arrival(double Scene,int32 Member,HouseCalderWildlifeSkyPaths::FSample& Out)const
{
    Out={};if(Member<0 || Member>=LandingCount || Scene<SplitScene || Scene>SplitScene+Durations[Member])return false;
    const double Age=Scene-SplitScene,U=Age/Durations[Member],L=Arrivals[Member].Length,M0=StartSpeeds[Member]*Durations[Member]/L,M1=250.*Durations[Member]/L;
    // Arc-space Hermite: matching source velocity at peel-off,2.5m/s at splash contact;
    // the later water-braking state must continue to a slow swimming speed.
    const double Arc=(-2*U*U*U+3*U*U)+(U*U*U-2*U*U+U)*M0+(U*U*U-U*U)*M1;
    FVector Direction;const FVector Position=HouseCalderWildlifeSkyPaths::Along(Arrivals[Member],Arc*L,&Direction);
    Out.Active=true;Out.Kind=0;Out.Event=1;Out.Member=30-LandingCount+Member;Out.Seed=2026101803u+137u+Out.Member*71u;Out.Scale=.92f+float(Out.Seed%17u)*.01f;Out.Position=Position;Out.Direction=Direction;Out.Age=Age;return true;
}

// Background movement is a pure function of shared scene time and fixed cohort.
// Authority reaction overrides will be replicated separately; no actor per bird.
bool HouseCalderWildlifeGoosePaths::FPlan::Cycle(double Scene,int32 Cohort,int32 Member,FCycleSample& Out)const
{
    Out={};if(Cohort<0 || Cohort>1 || Member<0 || Member>=LandingCount || Homes.Num()!=LandingCount)return false;
    const int64 Epoch=FMath::FloorToInt64(Scene/1800.);const double T=Scene-Epoch*1800.;const int32 Resident=int32((Epoch%2+2)%2);const bool Incoming=Cohort!=Resident;
    const double TouchAt=SplitScene+Durations[Member],LeaveAt=TouchAt-2.;
    auto& Bird=Out.Bird;Bird.Active=true;Bird.Kind=0;Bird.Event=1;Bird.Member=Member;Bird.Seed=2026101803u+137u+(22+Member)*71u;Bird.Scale=.92f+float(Bird.Seed%17u)*.01f;Bird.Direction=FVector(-.15,-1.,0).GetSafeNormal();
    if(!Incoming)
    {
        if(T<LeaveAt){Bird.Position=Homes[Member];const double Turn=FMath::SmoothStep(LeaveAt-3.,LeaveAt,T);const FVector Exit=(Departures[Member].Points[1]-Homes[Member]).GetSafeNormal();Bird.Direction=FMath::Lerp(Bird.Direction,Exit,Turn).GetSafeNormal();Out.Phase=1;Out.OnWater=true;return true;}
        const double Age=T-LeaveAt,D=DepartureDurations[Member],L=Departures[Member].Length;if(Age>D){Bird.Active=false;return false;}
        const double U=Age/D,M0=35.*D/L,M1=1650.*D/L,Arc=(-2*U*U*U+3*U*U)+(U*U*U-2*U*U+U)*M0+(U*U*U-U*U)*M1;
        Bird.Position=HouseCalderWildlifeSkyPaths::Along(Departures[Member],Arc*L,&Bird.Direction);Bird.Age=Age;Out.Phase=5;Out.Flap=1.;Out.Fold=FMath::Clamp(1.-Age/1.5,0.,1.);Out.Neck=FMath::Clamp(1.-Age/2.,0.,1.);return true;
    }
    if(T<SplitScene){Bird.Active=false;return false;}
    if(T<=TouchAt)
    {
        if(!Arrival(T,Member,Bird))return false;Out.Phase=2;const double Left=TouchAt-T;
        const double Set=FMath::SmoothStep(0.,2.,Bird.Age);Out.Flap=FMath::Lerp(1.,FMath::Clamp(Left/5.,0.,1.)*.15,Set);Out.Fold=0.;Out.Neck=FMath::Clamp((4.-Left)/4.,0.,1.);return true;
    }
    const double Age=T-TouchAt;Out.OnWater=true;
    if(Age<3.){Bird.Position=Touches[Member]+Bird.Direction*(250.*Age-215.*Age*Age/6.);Out.Phase=3;Out.Fold=FMath::SmoothStep(0.,.8,Age);return true;}
    const double SwimAt=SplitScene+Durations[LandingCount-1]+23.,Distance=FVector::Distance(Brakes[Member],Homes[Member]),SwimAge=FMath::Clamp(T-SwimAt,0.,Distance/35.);
    Bird.Position=FMath::Lerp(Brakes[Member],Homes[Member],SwimAge*35./Distance);
    const FVector Across=(Homes[Member]-Brakes[Member]).GetSafeNormal();const double TurnIn=FMath::SmoothStep(SwimAt-2.,SwimAt,T),TurnOut=FMath::SmoothStep(SwimAt+Distance/35.,SwimAt+Distance/35.+2.,T);
    Bird.Direction=FMath::Lerp(FMath::Lerp(Bird.Direction,Across,TurnIn).GetSafeNormal(),Bird.Direction,TurnOut).GetSafeNormal();
    if(SwimAge>0. && SwimAge<Distance/35.)Out.Phase=4;else Out.Phase=1;return true;
}
