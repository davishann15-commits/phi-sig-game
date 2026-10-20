#include "HouseCalderWildlifeCrowRoost.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderFarLand.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Misc/Crc.h"
namespace HouseCalderWildlifeCrowRoostImpl
{
double Wrap(double T){return FMath::Fmod(FMath::Fmod(T,1800.)+1800.,1800.);}
double Ground(const FVector2D& P)
{return FMath::Abs(P.X)<=32000.&&FMath::Abs(P.Y)<=32000.?HouseCalderTerrain::SurfaceZ(P.X,P.Y):HouseCalderFarLand::SkylineGround(P.X,P.Y);}
bool Route(HouseCalderWildlifeSkyPaths::FCurve& Out,const FVector& End,double Lift,const HouseCalderWildlifeSkyPaths::FPlan& Sky,int32& Samples)
{
    Out=HouseCalderWildlifeSkyPaths::FCurve();
    const FVector2D A(80000,-27000),B(42000,-19000),C(End.X+10500.,End.Y-14000.),D(End);
    const int32 Steps=FMath::CeilToInt((FVector2D::Distance(A,B)+FVector2D::Distance(B,C)+FVector2D::Distance(C,D))/90.);
    const double StartZ=Ground(A)+Lift;
    for(int32 I=0;I<=Steps;++I)
    {
        const double T=double(I)/Steps,U=1.-T;
        const FVector2D P=A*(U*U*U)+B*(3.*U*U*T)+C*(3.*U*T*T)+D*(T*T*T);
        const double Floor=Ground(P)+250.+(Lift-250.)*(1.-FMath::SmoothStep(.65,1.,T));
        const double Desired=FMath::Lerp(StartZ,End.Z,T)+1800.*FMath::Sin(PI*T);
        const FVector At(P,I==Steps?End.Z:FMath::Max(Desired,Floor));
        ++Samples;if(!Sky.Clear(At,HouseCalderWildlifeCrowRoost::Radius))return false;
        if(!Out.Points.IsEmpty())
        {
            const FVector Before=Out.Points.Last();
            if(!Sky.SolidSegmentClear(Before,At,HouseCalderWildlifeCrowRoost::Radius))return false;
            const int32 Segments=FMath::CeilToInt(FVector::Distance(Before,At)/25.);
            for(int32 K=1;K<Segments;++K)
            {
                const FVector Probe=FMath::Lerp(Before,At,double(K)/Segments);
                for(const FVector2D O:{FVector2D(0,0),FVector2D(80,0),FVector2D(-80,0),FVector2D(0,80),FVector2D(0,-80)})
                {++Samples;if(Probe.Z-Ground(FVector2D(Probe)+O)<130.)return false;}
            }
            Out.Length+=FVector::Distance(Before,At);
        }
        Out.Points.Add(At);Out.Arc.Add(Out.Length);
    }
    return Out.Points.Num()>2;
}
}
int32 HouseCalderWildlifeCrowRoost::Count(int32 Event)
{return Event>=0&&Event<Events?3+int32((2026101817u+uint32(Event)*191u)%13u):0;}
bool HouseCalderWildlifeCrowRoost::FPlan::Evaluate(double Scene,int32 Event,int32 Member,HouseCalderWildlifeSkyPaths::FSample& Out)const
{
    Out=HouseCalderWildlifeSkyPaths::FSample();if(Member<0||Member>=Count(Event)||Routes[Member].Points.IsEmpty())return false;
    const double GroupAge=HouseCalderWildlifeCrowRoostImpl::Wrap(Scene-(35.+Interval*Event));
    const double Age=GroupAge-LaunchSpacing*Member,Duration=Routes[Member].Length/Speed;
    if(Age<0.||Age>=Duration)return false;
    Out.Active=true;Out.Kind=1;Out.Event=Event;Out.Member=Member;Out.Seed=2026101817u+Event*191u+Member*71u;
    Out.Position=HouseCalderWildlifeSkyPaths::Along(Routes[Member],Age*Speed,&Out.Direction);Out.Age=Age;Out.Scale=.9f+float(Out.Seed%19u)*.01f;return true;
}
bool HouseCalderWildlifeCrowRoost::FPlan::Build(UWorld* World,const HouseCalderWildlifeSkyPaths::FPlan& Sky)
{
    Trees.Reset();Targets.Reset();Samples=RejectedCandidates=0;Hash=0;MaximumGroupDuration=0.;MinimumPairDistance=1.e20;
    for(TActorIterator<AActor> Actor(World);Actor;++Actor)
    {
        TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(*Actor);
        for(auto* Part:Parts)
        {
            if(!Part->GetName().StartsWith(TEXT("ExteriorWoodsPassHemlock"))||!Part->GetStaticMesh())continue;
            const FBox Bounds=Part->GetStaticMesh()->GetBoundingBox();
            for(int32 I=0;I<Part->GetInstanceCount();++I)
            {
                FTransform T;if(!Part->GetInstanceTransform(I,T,true)||T.GetTranslation().Y<5000.)continue;
                FTree Tree;Tree.Batch=Part->GetName();Tree.Instance=I;Tree.Transform=T;Tree.Canopy=Bounds.TransformBy(T);Trees.Add(Tree);
            }
        }
    }
    Trees.Sort([](const FTree& A,const FTree& B){const FVector P=A.Transform.GetTranslation(),Q=B.Transform.GetTranslation();if(P.Y!=Q.Y)return P.Y>Q.Y;if(P.X!=Q.X)return P.X<Q.X;const int32 C=A.Batch.Compare(B.Batch);return C?C<0:A.Instance<B.Instance;});
    for(const FTree& Tree:Trees)
    {
        if(Targets.Num()==Slots)break;bool Found=false;HouseCalderWildlifeSkyPaths::FCurve Candidate;FTree Pick=Tree;
        const double LocalHeight=(Tree.Canopy.Max.Z-Tree.Transform.GetTranslation().Z)/Tree.Transform.GetScale3D().Z;
        for(double Lift:{4000.,6000.,8000.})
        {
            for(double Height:{.56,.48,.65})
            {
                for(int32 Angle=0;Angle<8;++Angle)
                {
                    const double A=Angle*PI/4.;const FVector Local(180.*FMath::Cos(A),180.*FMath::Sin(A),LocalHeight*Height);
                    const FVector End=Tree.Transform.TransformPosition(Local);
                    if(!Tree.Canopy.IsInsideOrOn(End)){++RejectedCandidates;continue;}
                    if(HouseCalderWildlifeCrowRoostImpl::Route(Candidate,End,Lift,Sky,Samples)){Pick.End=End;Found=true;break;}
                    ++RejectedCandidates;
                }
                if(Found)break;
            }
            if(Found)break;
        }
        if(Found){Routes[Targets.Num()]=MoveTemp(Candidate);Targets.Add(Pick);}
    }
    if(Targets.Num()!=Slots){UE_LOG(LogTemp,Error,TEXT("CALDER_CROW_ROOST_FAILED trees=%d targets=%d rejects=%d"),Trees.Num(),Targets.Num(),RejectedCandidates);return false;}
    bool Separated=false;
    for(double Spacing:{1.,1.4,1.8,2.2})
    {
        LaunchSpacing=Spacing;MinimumPairDistance=1.e20;MaximumGroupDuration=0.;
        for(int32 M=0;M<Slots;++M)MaximumGroupDuration=FMath::Max(MaximumGroupDuration,M*Spacing+Routes[M].Length/Speed);
        // Select a full flock explicitly so every member pair is covered
        // regardless of which group counts occur in the cadence.
        for(double Age=0.;Age<MaximumGroupDuration;Age+=.05)
        {
            TArray<FVector,TInlineAllocator<Slots>> Positions;
            for(int32 M=0;M<Slots;++M)
            {
                const double Flight=Age-M*Spacing;
                if(Flight>=0.&&Flight<Routes[M].Length/Speed)Positions.Add(HouseCalderWildlifeSkyPaths::Along(Routes[M],Flight*Speed));
            }
            for(int32 A=0;A<Positions.Num();++A)for(int32 B=A+1;B<Positions.Num();++B)MinimumPairDistance=FMath::Min(MinimumPairDistance,FVector::Distance(Positions[A],Positions[B]));
        }
        if(MinimumPairDistance>=180.&&MaximumGroupDuration<Interval){Separated=true;break;}
    }
    if(!Separated){UE_LOG(LogTemp,Error,TEXT("CALDER_CROW_ROOST_FAILED pairCm=%.3f duration=%.3f"),MinimumPairDistance,MaximumGroupDuration);return false;}
    FString Canonical;
    for(int32 M=0;M<Slots;++M){Canonical+=FString::Printf(TEXT("%s:%d;"),*Targets[M].Batch,Targets[M].Instance);for(const FVector& P:Routes[M].Points)Canonical+=FString::Printf(TEXT("%.2f,%.2f,%.2f;"),P.X,P.Y,P.Z);}
    Canonical+=FString::Printf(TEXT("spacing=%.3f"),LaunchSpacing);Hash=FCrc::StrCrc32(*Canonical);
    UE_LOG(LogTemp,Display,TEXT("CALDER_CROW_ROOST_PATHS trees=%d targets=%d samples=%d candidateRejects=%d minimumPairCm=%.3f duration=%.3f spacing=%.3f interval=150 hash=%08x baseSky=%08x physicsQueries=0 runtimeRenderAccepted=0 callsAccepted=0"),Trees.Num(),Targets.Num(),Samples,RejectedCandidates,MinimumPairDistance,MaximumGroupDuration,LaunchSpacing,Hash,Sky.Hash);return true;
}
