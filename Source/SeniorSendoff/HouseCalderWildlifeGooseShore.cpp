// Shore access through the original creek-inlet opening. No world geometry
// edits or collision/physics queries. Pure reactions require director authority.
#include "HouseCalderWildlifeGooseShore.h"
#include "Algo/Reverse.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderLandform.h"
#include "HouseCalderWater.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/Crc.h"
#include "ProceduralMeshComponent.h"
namespace HouseCalderWildlifeGooseShoreImpl
{
constexpr int32 NX=70,NY=45;constexpr double X0=23800.,Y0=-12800.,Step=50.;
double Segment(const FVector2D& P,const FVector2D& A,const FVector2D& B)
{const FVector2D D=B-A;return FVector2D::Distance(P,A+D*FMath::Clamp(FVector2D::DotProduct(P-A,D)/FMath::Max(.001,D.SizeSquared()),0.,1.));}
FVector2D XY(int32 Id){return FVector2D(X0+(Id%NX)*Step,Y0+(Id/NX)*Step);}
int32 Id(const FVector2D& P){const int32 X=FMath::RoundToInt((P.X-X0)/Step),Y=FMath::RoundToInt((P.Y-Y0)/Step);return X>=0&&X<NX&&Y>=0&&Y<NY?Y*NX+X:INDEX_NONE;}
double Height(const FVector2D& P){return HouseCalderTerrain::SurfaceZ(P.X,P.Y);}
double CreekDistance(const FVector2D& P)
{double Best=1.e30;const auto& Points=HouseCalderLandform::CreekPoints();for(int32 I=1;I<Points.Num();++I)Best=FMath::Min(Best,Segment(P,Points[I-1].At,Points[I].At));return Best;}
double EdgeDistance(const FVector2D& P)
{double Best=1.e30;const auto& Points=HouseCalderLandform::PondOutline();for(int32 I=0;I<Points.Num();++I)Best=FMath::Min(Best,Segment(P,Points[I],Points[(I+1)%Points.Num()]));return Best;}
FVector Root(const FVector2D& P)
{
    float Water=0.f;const double Ground=Height(P);const bool Wet=HouseCalderWater::WaterAt(P,Water);
    const double Stand=Wet?FMath::SmoothStep(0.,20.,20.-(Water-Ground)):1.;
    return FVector(P,Wet?Water+14.+13.2*Stand:Ground+27.2);
}
bool Clear(const FVector2D& P,const HouseCalderWildlifeSkyPaths::FPlan& Sky,const TArray<FBox>& Works)
{
    float Water=0.f,Deck=0.f;const bool Wet=HouseCalderWater::WaterAt(P,Water);const double Ground=Height(P),Level=Wet?Water:Ground;
    if(HouseCalderWater::DeckAt(P,Deck) || (!Wet&&Ground<HouseCalderLandform::PondWaterZ()-.5))return false;
    // Original rim is a5cm-high stone tread with a45cm shore strip. Keep the
    // whole animal off that tread; choose the existing creek-mouth opening.
    if(EdgeDistance(P)<150. && CreekDistance(P)>=500.)return false;
    if(!Wet)
    {
        const double DX=(Height(P+FVector2D(10,0))-Height(P-FVector2D(10,0)))/20.,DY=(Height(P+FVector2D(0,10))-Height(P-FVector2D(0,10)))/20.;
        if(DX*DX+DY*DY>FMath::Square(FMath::Tan(FMath::DegreesToRadians(28.))))return false;
    }
    const FVector R=Root(P);const FBox Body(FVector(P.X-75.,P.Y-75.,Level+.5),FVector(P.X+75.,P.Y+75.,Level+100.));
    const FIntPoint Cell(FMath::FloorToInt(P.X/4000.),FMath::FloorToInt(P.Y/4000.));
    if(const auto* Indices=Sky.SolidCells.Find(Cell))for(int32 Index:*Indices)if(Sky.Solids[Index].Intersect(Body))return false;
    for(const auto& Box:Works)if(Box.Intersect(Body))return false;
    for(const FVector2D Offset:{FVector2D(0,0),FVector2D(65,0),FVector2D(-65,0),FVector2D(0,65),FVector2D(0,-65)})
    {if(HouseCalderWater::DeckAt(P+Offset,Deck))return false;if(Height(P+Offset)>R.Z-13.+2.)return false;}
    return true;
}
bool Line(const FVector2D& A,const FVector2D& B,const HouseCalderWildlifeSkyPaths::FPlan& Sky,const TArray<FBox>& Works,int32& Samples)
{
    const int32 Count=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(A,B)/20.));
    FVector Before=Root(A);for(int32 I=0;I<=Count;++I)
    {const FVector2D P=FMath::Lerp(A,B,double(I)/Count);++Samples;if(!Clear(P,Sky,Works))return false;const FVector Here=Root(P);if(I&&FMath::Abs(Here.Z-Before.Z)>15.)return false;Before=Here;}return true;
}
}
bool HouseCalderWildlifeGooseShore::FPlan::Build(UWorld* World,const HouseCalderWildlifeSkyPaths::FPlan& Sky,const HouseCalderWildlifeGoosePaths::FPlan& Pond)
{
    using namespace HouseCalderWildlifeGooseShoreImpl;const double Began=FPlatformTime::Seconds();WaterWorks.Reset();Samples=Rejected=Nodes=0;Hash=0;Routes[0]={};Routes[1]={};if(!World || Pond.Homes.Num()!=8)return false;
    // Read the actual authored water-work triangles, not physics results. Their
    // bounds conservatively exclude the dock,stone rim,dam and inlet causeway.
    for(TActorIterator<AActor> Actor(World);Actor;++Actor)
    {
        TInlineComponentArray<UProceduralMeshComponent*> Parts(*Actor);
        for(auto* Part:Parts)if(Part->GetName().StartsWith(TEXT("CalderWaterWorks_Pond_Collision")))
        for(int32 Section=0;Section<Part->GetNumSections();++Section)if(auto* Mesh=Part->GetProcMeshSection(Section))
        for(int32 T=0;T+2<Mesh->ProcIndexBuffer.Num();T+=3)
        {
            FBox Box(ForceInit);for(int32 K=0;K<3;++K)Box+=Part->GetComponentTransform().TransformPosition(Mesh->ProcVertexBuffer[Mesh->ProcIndexBuffer[T+K]].Position);
            if(Box.Max.X>=X0-100. && Box.Min.X<=X0+NX*Step+100. && Box.Max.Y>=Y0-100. && Box.Min.Y<=Y0+NY*Step+100.)WaterWorks.Add(Box);
        }
    }
    if(WaterWorks.IsEmpty()){UE_LOG(LogTemp,Error,TEXT("CALDER_GOOSE_SHORE_REJECT missing native water-work data"));return false;}
    TArray<uint8> Good;Good.SetNumZeroed(NX*NY);for(int32 I=0;I<Good.Num();++I)if(Clear(XY(I),Sky,WaterWorks)){Good[I]=1;++Nodes;}
    TArray<FVector2D> Rests;FString Canonical;
    for(int32 Member=0;Member<2;++Member)
    {
        const FVector2D Start(Pond.Homes[Member+6]);int32 Seed=INDEX_NONE;double Nearest=1.e30;
        for(int32 I=0;I<Good.Num();++I)if(Good[I]){const double D=FVector2D::DistSquared(Start,XY(I));if(D<Nearest&&Line(Start,XY(I),Sky,WaterWorks,Samples)){Nearest=D;Seed=I;}}
        if(Seed<0){++Rejected;return false;}TArray<int32> Prev,Queue;Prev.Init(INDEX_NONE,Good.Num());Prev[Seed]=Seed;Queue.Add(Seed);
        for(int32 At=0;At<Queue.Num();++At)
        {
            const int32 A=Queue[At],AX=A%NX,AY=A/NX;
            for(int32 DY=-1;DY<=1;++DY)for(int32 DX=-1;DX<=1;++DX)
            {if((DX==0&&DY==0)||AX+DX<0||AX+DX>=NX||AY+DY<0||AY+DY>=NY)continue;const int32 B=A+DX+DY*NX;if(!Good[B]||Prev[B]!=INDEX_NONE||!Line(XY(A),XY(B),Sky,WaterWorks,Samples))continue;Prev[B]=A;Queue.Add(B);}
        }
        int32 End=INDEX_NONE;double Score=1.e30;
        for(int32 I:Queue)
        {
            const FVector2D P=XY(I);float Water=0.f;if(HouseCalderWater::WaterAt(P,Water) || Height(P)<HouseCalderLandform::PondWaterZ()+3. || CreekDistance(P)>500.)continue;
            bool Separated=true;for(const auto& Rest:Rests)Separated&=FVector2D::Distance(P,Rest)>=240.;if(!Separated)continue;
            const double D=FVector2D::DistSquared(P,FVector2D(25200,-10800))+FVector2D::DistSquared(P,Start)*.01;if(D<Score){Score=D;End=I;}
        }
        if(End<0){++Rejected;UE_LOG(LogTemp,Error,TEXT("CALDER_GOOSE_SHORE_REJECT no accessible dry inlet bank member=%d nodes=%d reachable=%d works=%d"),Member,Nodes,Queue.Num(),WaterWorks.Num());return false;}
        Rests.Add(XY(End));TArray<FVector2D> Path;for(int32 I=End;;I=Prev[I]){Path.Add(XY(I));if(I==Seed)break;}Algo::Reverse(Path);Path.Insert(Start,0);
        TArray<FVector2D> Simple;Simple.Add(Start);for(int32 At=0;At<Path.Num()-1;){int32 Next=Path.Num()-1;while(Next>At+1&&!Line(Path[At],Path[Next],Sky,WaterWorks,Samples))--Next;Simple.Add(Path[Next]);At=Next;}
        auto& Route=Routes[Member];for(int32 S=1;S<Simple.Num();++S)
        {
            const int32 Count=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(Simple[S-1],Simple[S])/20.));
            for(int32 I=S==1?0:1;I<=Count;++I){const FVector P=Root(FMath::Lerp(Simple[S-1],Simple[S],double(I)/Count));if(!Route.Points.IsEmpty())Route.Length+=FVector::Distance(Route.Points.Last(),P);Route.Points.Add(P);Route.Arc.Add(Route.Length);Canonical+=FString::Printf(TEXT("%d,%.5f,%.5f,%.5f\n"),Member,P.X,P.Y,P.Z);}
        }
    }
    Hash=FCrc::StrCrc32(*Canonical);UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_SHORE_DATA clearNodes=%d samples=%d rejected=%d nativeWorksTriangles=%d routes=2 hash=%08x physicsQueries=0 dataOnly=1 buildMs=%.2f"),Nodes,Samples,Rejected,WaterWorks.Num(),Hash,(FPlatformTime::Seconds()-Began)*1000.);return Rejected==0;
}

// Pure background schedule. Both birds are back home before the earliest
// departing pair; this keeps6..8on water while2can rest on the bank.
bool HouseCalderWildlifeGooseShore::FPlan::At(int32 Route,double Arc,bool Reverse,HouseCalderWildlifeGoosePaths::FCycleSample& Out)const
{
    if(Route<0 || Route>1 || Routes[Route].Points.Num()<2)return false;const auto& Path=Routes[Route];Arc=FMath::Clamp(Arc,0.,Path.Length);FVector Direction;
    FVector P=HouseCalderWildlifeSkyPaths::Along(Path,Arc,&Direction);float Water=0.f;const double Ground=HouseCalderTerrain::SurfaceZ(P.X,P.Y);const bool Wet=HouseCalderWater::WaterAt(FVector2D(P),Water);
    const double Stand=Wet?FMath::SmoothStep(0.,20.,20.-(Water-Ground)):1.,Scale=Out.Bird.Scale;P.Z=Wet?Water+Scale*(14.+13.2*Stand):Ground+27.2*Scale;
    Out.Bird.Position=P;Out.Bird.Direction=FVector(Direction.X,Direction.Y,0).GetSafeNormal()*(Reverse?-1.:1.);Out.Bird.Active=true;Out.Stand=Stand;Out.ShoreArc=Arc;Out.ShoreRoute=Route;Out.OnWater=Wet;Out.WaterSurface=Wet?Water:0.;Out.Flap=0.;Out.Fold=Out.Neck=1.;return true;
}
bool HouseCalderWildlifeGooseShore::FPlan::Cycle(const HouseCalderWildlifeGoosePaths::FPlan& Pond,double Raw,int32 Cohort,int32 Member,HouseCalderWildlifeGoosePaths::FCycleSample& Out)const
{
    if(!Pond.Cycle(Raw,Cohort,Member,Out))return false;if(Out.OnWater)Out.WaterSurface=HouseCalderLandform::PondWaterZ();if(Member<6)return true;
    const int32 Route=Member-6;if(Routes[Route].Points.Num()<2)return false;const int64 Epoch=FMath::FloorToInt64(Raw/1800.);const double T=Raw-Epoch*1800.;const bool Resident=Cohort==int32((Epoch%2+2)%2);const double Length=Routes[Route].Length,HomeBy=Pond.SplitScene+Pond.Durations[0]-6.,ReturnAt=HomeBy-Length/35.;
    if(Resident)
    {
        if(T>=HomeBy)return true;
        const double TurnAt=ReturnAt-2.5;
        if(T<ReturnAt)
        {
            if(!At(Route,Length,false,Out))return false;Out.Phase=6;
            if(T>=TurnAt)
            {
                const double Blend=FMath::SmoothStep(0.,2.5,T-TurnAt);const FQuat Turn(FVector::UpVector,PI*Blend);
                Out.Bird.Direction=Turn.RotateVector(Out.Bird.Direction);Out.WalkAge=T-TurnAt;
            }
            return true;
        }
        if(!At(Route,FMath::Max(0.,(HomeBy-T)*35.),true,Out))return false;Out.Phase=8;Out.WalkAge=T-TurnAt;Out.ShoreSpeed=-35.;return true;
    }
    const double SwimAt=Pond.SplitScene+Pond.Durations[7]+23.,LongestHome=FMath::Max(FVector::Distance(Pond.Brakes[6],Pond.Homes[6]),FVector::Distance(Pond.Brakes[7],Pond.Homes[7]))/35.,Start=SwimAt+LongestHome+20.;
    if(T<Start)return true;const double Age=T-Start;if(Age<Length/35.){if(!At(Route,Age*35.,false,Out))return false;Out.Phase=7;Out.WalkAge=Age;Out.ShoreSpeed=35.;return true;}
    if(!At(Route,Length,false,Out))return false;Out.Phase=6;
    // Finish the last alternating steps before settling both feet at rest.
    if(Age<Length/35.+2.)Out.WalkAge=Age;
    return true;
}

bool HouseCalderWildlifeGooseShore::FPlan::Project(int32 Route,const FVector& Point,double& Arc,double& Error)const
{
    Arc=0.;Error=1.e30;if(Route<0||Route>1)return false;const auto& Path=Routes[Route];
    for(int32 I=1;I<Path.Points.Num();++I)
    {
        const FVector D=Path.Points[I]-Path.Points[I-1];
        // Use XY: route data stores nominal unscaled height, while actual
        // swimmer/standing roots follow the fixed bird scale.
        const double U=FMath::Clamp(FVector2D::DotProduct(FVector2D(Point-Path.Points[I-1]),FVector2D(D))/FMath::Max(1.e-9,FVector2D(D).SizeSquared()),0.,1.);
        const FVector Closest=Path.Points[I-1]+D*U;const double Distance=FVector2D::Distance(FVector2D(Point),FVector2D(Closest));
        if(Distance<Error){Error=Distance;Arc=FMath::Lerp(Path.Arc[I-1],Path.Arc[I],U);}
    }
    return Error<1.e29;
}
double HouseCalderWildlifeGooseShore::FPlan::HomeDeadline(const HouseCalderWildlifeGoosePaths::FPlan& Pond,double Raw,int32 Cohort)const
{
    const int64 Epoch=FMath::FloorToInt64(Raw/1800.);const int32 Resident=int32((Epoch%2+2)%2);
    return double(Epoch+(Cohort==Resident?0:1))*1800.+Pond.SplitScene+Pond.Durations[0]-6.;
}
bool HouseCalderWildlifeGooseShore::FPlan::Reaction(const HouseCalderWildlifeGoosePaths::FPlan& Pond,double Raw,int32 Member,const FReaction& State,HouseCalderWildlifeGoosePaths::FCycleSample& Out)const
{
    const int32 Route=Member-6;if(Route<0||Route>1||(State.Mode!=64&&State.Mode!=65)||Routes[Route].Points.Num()<2)return false;
    const double Length=Routes[Route].Length;if(State.Arc<-.1||State.Arc>Length+.1||!FMath::IsFinite(State.Arc)||!FMath::IsFinite(Raw)||!FMath::IsFinite(State.Start)||!FMath::IsFinite(State.Yaw)||(State.Mode==65&&FMath::Abs(State.Arc)>.1))return false;
    if(!Pond.Cycle(0.,0,Member,Out))return false;const double Age=FMath::Max(0.,Raw-State.Start);const double Hold=State.Mode==64?.8:2.5;
    const double Arc=State.Mode==64?FMath::Max(0.,State.Arc-FMath::Max(0.,Age-Hold)*35.):FMath::Min(Length,FMath::Max(0.,Age-Hold)*35.);
    if(!At(Route,Arc,State.Mode==64,Out))return false;
    Out.WalkAge=Age;Out.ShoreSpeed=Age<Hold?0.:State.Mode==64?-35.:35.;
    if(Age<Hold)
    {
        const FQuat From=FRotator(0.,State.Yaw,0.).Quaternion(),To=Out.Bird.Direction.Rotation().Quaternion();
        Out.Bird.Direction=FQuat::Slerp(From,To,FMath::SmoothStep(0.,Hold,Age)).GetForwardVector();
        Out.Phase=State.Mode==64?9:12;Out.Mouth=State.Mode==64?FMath::SmoothStep(0.,.1,Age)*(1.-FMath::SmoothStep(Hold-.2,Hold,Age)):0.;Out.Neck=State.Mode==64?1.-.25*FMath::Sin(PI*Age/Hold):1.;
    }
    else if(State.Mode==64)
    {Out.Phase=Arc>0.?10:11;if(Arc==0.)Out.WalkAge=-1.;}
    else
    {Out.Phase=Arc<Length?12:6;if(Age>Hold+Length/35.+2.)Out.WalkAge=-1.;}
    return true;
}
