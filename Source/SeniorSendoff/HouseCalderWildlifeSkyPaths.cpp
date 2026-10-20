// Authored sky data prototype; landing handoff/calls/native validation remain open.
#include "HouseCalderWildlifeSkyPaths.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderFarLand.h"
#include "HouseCalderLandform.h"
#include "HouseCalderMapTrees.h"
#include "HouseCampusLayout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Misc/Crc.h"
#include "PhysicsEngine/BodySetup.h"

namespace HouseCalderWildlifeSkyPathsImpl
{
double Wrap(double T){return FMath::Fmod(FMath::Fmod(T,1800.)+1800.,1800.);}
double Ground(const FVector2D& P)
{return FMath::Abs(P.X)<=32000. && FMath::Abs(P.Y)<=32000.?HouseCalderTerrain::SurfaceZ(P.X,P.Y):HouseCalderFarLand::SkylineGround(P.X,P.Y);}
void Curve(HouseCalderWildlifeSkyPaths::FCurve& Out,const FVector2D& A,const FVector2D& B,const FVector2D& C,const FVector2D& D,double Height)
{
    Out=HouseCalderWildlifeSkyPaths::FCurve();const int32 Steps=FMath::CeilToInt((FVector2D::Distance(A,B)+FVector2D::Distance(B,C)+FVector2D::Distance(C,D))/150.);
    for(int32 I=0;I<=Steps;++I)
    {
        const double T=double(I)/Steps,U=1.-T;const FVector2D P=A*(U*U*U)+B*(3*U*U*T)+C*(3*U*T*T)+D*(T*T*T);
        const FVector At(P,Ground(P)+Height);if(!Out.Points.IsEmpty())Out.Length+=FVector::Distance(At,Out.Points.Last());Out.Points.Add(At);Out.Arc.Add(Out.Length);
    }
}
// Choose one planar V height from every member's terrain interval. No runtime
// ground query or vertical per-member distortion is needed for the formation.
bool FitV(HouseCalderWildlifeSkyPaths::FCurve& Out,int32 Event)
{
    const int32 Count=14+int32((2026101803u+Event*137u)%27u);
    for(int32 I=0;I<Out.Points.Num();++I)
    {
        const FVector D=(Out.Points[FMath::Min(I+1,Out.Points.Num()-1)]-Out.Points[FMath::Max(0,I-1)]).GetSafeNormal();
        const FVector F=FVector(D.X,D.Y,0).GetSafeNormal(),R(-F.Y,F.X,0);double Low=-1.e20,High=1.e20;
        for(int32 Member=0;Member<Count;++Member)
        {
            const double Row=Member>0?(Member+1)/2:0,Side=Member%2?1.:-1.;const FVector At=Out.Points[I]+R*(Side*Row*240.)-F*(Row*265.);
            for(const FVector2D Offset:{FVector2D(0,0),FVector2D(110,0),FVector2D(-110,0),FVector2D(0,110),FVector2D(0,-110)})
            {const double Z=Ground(FVector2D(At)+Offset);Low=FMath::Max(Low,Z+6200.);High=FMath::Min(High,Z+14800.);}
        }
        if(Low>High){UE_LOG(LogTemp,Error,TEXT("CALDER_SKY_PATH_V_UNFIT event=%d point=%d low=%.2f high=%.2f"),Event,I,Low,High);return false;}
        Out.Points[I].Z=FMath::Clamp(Out.Points[I].Z,Low,High);
    }
    Out.Length=0.;for(int32 I=0;I<Out.Points.Num();++I){if(I)Out.Length+=FVector::Distance(Out.Points[I-1],Out.Points[I]);Out.Arc[I]=Out.Length;}return true;
}
bool SweptBox(const FBox& Raw,const FVector& A,const FVector& B,double Radius)
{
    const FBox Box=Raw.ExpandBy(Radius);double Lo=0.,Hi=1.;const FVector D=B-A;
    for(int32 Axis=0;Axis<3;++Axis)
    {
        if(FMath::Abs(D[Axis])<1.e-9){if(A[Axis]<Box.Min[Axis] || A[Axis]>Box.Max[Axis])return false;continue;}
        double L=(Box.Min[Axis]-A[Axis])/D[Axis],H=(Box.Max[Axis]-A[Axis])/D[Axis];if(L>H)Swap(L,H);Lo=FMath::Max(Lo,L);Hi=FMath::Min(Hi,H);if(Lo>Hi)return false;
    }
    return true;
}
FVector BatPoint(const FVector& Centre,double Radius,double Scene,int32 Member)
{
    const double P=Wrap(Scene)*2*PI/1800.,Seed=Member*.873,Loops=Radius>=300.?270.:900.;
    return Centre+FVector(Radius*FMath::Cos(Loops*P+Seed)+Radius*.14*FMath::Sin(517*P+Seed),
        Radius*.76*FMath::Sin(Loops*P+Seed)+Radius*.14*FMath::Sin(613*P+Seed),55*FMath::Sin(373*P+Seed)+22*FMath::Cos(521*P+Seed));
}
}
FVector HouseCalderWildlifeSkyPaths::Along(const FCurve& Curve,double Distance,FVector* Direction)
{
    if(Curve.Points.Num()<2){if(Direction)*Direction=FVector::ForwardVector;return Curve.Points.IsEmpty()?FVector::ZeroVector:Curve.Points[0];}
    Distance=FMath::Clamp(Distance,0.,Curve.Length);int32 Lo=0,Hi=Curve.Arc.Num()-1;
    while(Hi-Lo>1){const int32 Mid=(Lo+Hi)/2;if(Curve.Arc[Mid]<=Distance)Lo=Mid;else Hi=Mid;}
    if(Direction)*Direction=(Curve.Points[Hi]-Curve.Points[Lo]).GetSafeNormal();
    return FMath::Lerp(Curve.Points[Lo],Curve.Points[Hi],(Distance-Curve.Arc[Lo])/FMath::Max(.001,Curve.Arc[Hi]-Curve.Arc[Lo]));
}
bool HouseCalderWildlifeSkyPaths::FPlan::Clear(const FVector& P,double Radius)const
{
    if(P.Z-HouseCalderWildlifeSkyPathsImpl::Ground(FVector2D(P))<Radius+50.)return false;
    const FIntPoint Cell(FMath::FloorToInt(P.X/4000.),FMath::FloorToInt(P.Y/4000.));
    if(const auto* Indices=SolidCells.Find(Cell))for(int32 Index:*Indices)if(Solids[Index].ExpandBy(Radius).IsInsideOrOn(P))return false;return true;
}
bool HouseCalderWildlifeSkyPaths::FPlan::SolidSegmentClear(const FVector& A,const FVector& B,double Radius)const
{
    for(int32 X=FMath::FloorToInt(FMath::Min(A.X,B.X)/4000.);X<=FMath::FloorToInt(FMath::Max(A.X,B.X)/4000.);++X)
        for(int32 Y=FMath::FloorToInt(FMath::Min(A.Y,B.Y)/4000.);Y<=FMath::FloorToInt(FMath::Max(A.Y,B.Y)/4000.);++Y)
            if(const auto* Indices=SolidCells.Find(FIntPoint(X,Y)))for(int32 Index:*Indices)if(HouseCalderWildlifeSkyPathsImpl::SweptBox(Solids[Index],A,B,Radius))return false;
    return true;
}
bool HouseCalderWildlifeSkyPaths::FPlan::EvaluateGoose(double Scene,int32 Event,int32 Member,FSample& Out)const
{
    Out=FSample();if(Event<0 || Event>1)return false;const uint32 Seed=2026101803u+Event*137u;
    const int32 Count=14+int32(Seed%27u);if(Member<0 || Member>=Count)return false;
    const double Age=HouseCalderWildlifeSkyPathsImpl::Wrap(Scene-(120.+900.*Event)),Speed=1800.;
    if(Age>Goose[Event].Length/Speed)return false;FVector Direction;FVector Position=Along(Goose[Event],Age*Speed,&Direction);
    const FVector Forward=FVector(Direction.X,Direction.Y,0).GetSafeNormal(),Right(-Forward.Y,Forward.X,0);
    if(Member>0){const double Row=(Member+1)/2,Side=Member%2?1.:-1.;Position+=Right*(Side*Row*240.)-Forward*(Row*265.);}
    // FitV validates a common planar height against all actual member offsets.
    Out.Active=true;Out.Kind=0;Out.Event=Event;Out.Member=Member;Out.Seed=Seed+Member*71u;Out.Position=Position;Out.Direction=Direction;Out.Age=Age;Out.Scale=.92f+float(Out.Seed%17u)*.01f;return true;
}
bool HouseCalderWildlifeSkyPaths::FPlan::EvaluateCrow(double Scene,int32 Event,int32 Member,FSample& Out)const
{
    Out=FSample();if(Event<0 || Event>=12)return false;const uint32 Seed=2026101817u+Event*191u;
    const int32 Count=3+int32(Seed%13u);if(Member<0 || Member>=Count)return false;
    const double Age=HouseCalderWildlifeSkyPathsImpl::Wrap(Scene-(35.+150.*Event)),Speed=1200.;
    if(Age>Crow.Length/Speed)return false;FVector Direction;FVector Position=Along(Crow,Age*Speed,&Direction);
    const FVector Forward=FVector(Direction.X,Direction.Y,0).GetSafeNormal(),Right(-Forward.Y,Forward.X,0);
    Position+=Right*((int32((Seed+Member*71u)%7u)-3)*170.)-Forward*(Member*210.);
    Position.Z+=double((Seed+Member*53u)%301u);
    Out.Active=true;Out.Kind=1;Out.Event=Event;Out.Member=Member;Out.Seed=Seed+Member*71u;Out.Position=Position;Out.Direction=Direction;Out.Age=Age;Out.Scale=.9f+float(Out.Seed%19u)*.01f;return true;
}
bool HouseCalderWildlifeSkyPaths::FPlan::EvaluateBat(double Scene,int32 Member,FSample& Out)const
{
    Out=FSample();if(!BatCentres.IsValidIndex(Member))return false;
    const FVector P=HouseCalderWildlifeSkyPathsImpl::BatPoint(BatCentres[Member],BatRadii[Member],Scene,Member),After=HouseCalderWildlifeSkyPathsImpl::BatPoint(BatCentres[Member],BatRadii[Member],Scene+.02,Member);
    Out.Active=true;Out.Kind=Member%2?3:2;Out.Event=0;Out.Member=Member;Out.Seed=2026101831u+Member*73u;Out.Position=P;Out.Direction=(After-P).GetSafeNormal();Out.Age=HouseCalderWildlifeSkyPathsImpl::Wrap(Scene);Out.Scale=.9f+float(Out.Seed%17u)*.01f;return true;
}
bool HouseCalderWildlifeSkyPaths::FPlan::Build(UWorld* World)
{
    const double Began=FPlatformTime::Seconds();Solids.Reset();SolidCells.Reset();BatCentres.Reset();BatRadii.Reset();Samples=0;
    for(int32 Index=0;Index<HouseCampusLayout::Blocks().Num();++Index)
    {
        const auto& B=HouseCampusLayout::Blocks()[Index];const double Base=HouseCampusLayout::Frame(Index).GetTranslation().Z;
        Solids.Add(FBox(FVector(B.X0,B.Y0,Base-100.),FVector(B.X1,B.Y1,Base+B.Height+600.)));
    }
    for(TActorIterator<AActor> Actor(World);Actor;++Actor)
    {
        TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(*Actor);
        for(auto* Part:Parts)
        {
            const FString Name=Part->GetName();const bool Tree=HouseCalderMapTrees::TreeBatch(Name),Solid=Name.StartsWith(TEXT("ExteriorWoods")) || Name.StartsWith(TEXT("ExteriorAppalachian"));
            if((!Tree&&!Solid) || Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision)continue;
            const UStaticMesh* Mesh=Part->GetStaticMesh();const auto* Body=Mesh?Mesh->GetBodySetup():nullptr;if(!Body)continue;
            for(int32 I=0;I<Part->GetInstanceCount();++I){FTransform T;Part->GetInstanceTransform(I,T,true);const FBox Box=Body->AggGeom.CalcAABB(T);if(Box.IsValid)Solids.Add(Box);}
        }
    }
    for(const auto& W:HouseCalderLandform::Walls())
    {
        FBox Box(ForceInit);for(const FVector2D& P:{W.A,W.B}){const double Z=HouseCalderWildlifeSkyPathsImpl::Ground(P);Box+=FVector(P,Z-250.);Box+=FVector(P,Z+150.);}Solids.Add(Box.ExpandBy(FVector(40,40,0)));
    }
    Solids.Sort([](const FBox& A,const FBox& B){for(int32 Axis=0;Axis<3;++Axis){if(A.Min[Axis]!=B.Min[Axis])return A.Min[Axis]<B.Min[Axis];if(A.Max[Axis]!=B.Max[Axis])return A.Max[Axis]<B.Max[Axis];}return false;});
    for(int32 Index=0;Index<Solids.Num();++Index)
    {
        const FBox Box=Solids[Index].ExpandBy(115.);
        for(int32 X=FMath::FloorToInt(Box.Min.X/4000.);X<=FMath::FloorToInt(Box.Max.X/4000.);++X)
            for(int32 Y=FMath::FloorToInt(Box.Min.Y/4000.);Y<=FMath::FloorToInt(Box.Max.Y/4000.);++Y)SolidCells.FindOrAdd(FIntPoint(X,Y)).Add(Index);
    }
    const double BoxesAt=FPlatformTime::Seconds();
    for(int32 Event=0;Event<2;++Event)
    {
        const FVector2D Shift(Event*2100.,0);HouseCalderWildlifeSkyPathsImpl::Curve(Goose[Event],FVector2D(52000,105000)+Shift,FVector2D(25000,35000)+Shift,FVector2D(5000,-35000)+Shift,FVector2D(-23000,-105000)+Shift,10500.+Event*800.);if(!HouseCalderWildlifeSkyPathsImpl::FitV(Goose[Event],Event))return false;
    }
    HouseCalderWildlifeSkyPathsImpl::Curve(Crow,{100000,-30000},{32000,-18000},{-7000,18000},{-15000,100000},5000.);
    for(const FVector2D& P:{FVector2D(24700,-12800),FVector2D(25800,-13600),FVector2D(24800,-14800)})
    {BatCentres.Add(FVector(P,HouseCalderLandform::PondWaterZ()+550.));BatRadii.Add(400.);}
    for(double Target:{-18000.,-22000.,-25500.})
    {
        const auto& Points=HouseCalderLandform::GlenPoints();if(Points.IsEmpty())return false;int32 Best=0;for(int32 I=1;I<Points.Num();++I)if(FMath::Abs(Points[I].At.Y-Target)<FMath::Abs(Points[Best].At.Y-Target))Best=I;
        BatCentres.Add(FVector(Points[Best].At,Points[Best].WaterZ+500.));BatRadii.Add(FMath::Min(90.,double(Points[Best].HalfWidth)*.5));
    }
    TArray<FVector2D> Lamps;HouseCampusLayout::LampPoints(Lamps);Lamps.Sort([](const FVector2D& A,const FVector2D& B){const double DA=FVector2D::DistSquared(A,FVector2D(20000,-14000)),DB=FVector2D::DistSquared(B,FVector2D(20000,-14000));return DA!=DB?DA<DB:A.Y!=B.Y?A.Y<B.Y:A.X<B.X;});
    for(int32 I=0;I<FMath::Min(2,Lamps.Num());++I){const auto& P=Lamps[I];BatCentres.Add(FVector(P,HouseCalderWildlifeSkyPathsImpl::Ground(P)+650.));BatRadii.Add(100.);}
    if(BatCentres.Num()!=8)return false;FString Canonical;int32 Failures=0,KindFailures[4]={};
    auto Reject=[&](const FSample& S,double Alt){if(KindFailures[S.Kind]<4)UE_LOG(LogTemp,Warning,TEXT("CALDER_SKY_PATH_REJECT kind=%d event=%d member=%d age=%.3f xyz=%.2f,%.2f,%.2f altitude=%.2f"),S.Kind,S.Event,S.Member,S.Age,S.Position.X,S.Position.Y,S.Position.Z,Alt);++Failures;++KindFailures[S.Kind];};
    const double RoutesAt=FPlatformTime::Seconds();
    for(int32 Event=0;Event<2;++Event)for(int32 Member=0;Member<40;++Member)
    {
        FSample Before;bool Had=false;const double Duration=Goose[Event].Length/1800.;
        for(double Age=0.;Age<Duration+.025;Age+=.025)
        {FSample S;if(!EvaluateGoose(120.+900.*Event+FMath::Min(Age,Duration),Event,Member,S))continue;++Samples;const double Alt=S.Position.Z-HouseCalderWildlifeSkyPathsImpl::Ground(FVector2D(S.Position));if(Alt<6000. || Alt>15000. || !Clear(S.Position,115.) || (Had&&!SolidSegmentClear(Before.Position,S.Position,115.)))Reject(S,Alt);Before=S;Had=true;}
    }
    for(int32 Event=0;Event<12;++Event)for(int32 Member=0;Member<15;++Member)
    {
        FSample Before;bool Had=false;const double Duration=Crow.Length/1200.;
        for(double Age=0.;Age<Duration+.04;Age+=.04)
        {FSample S;if(!EvaluateCrow(HouseCalderWildlifeSkyPathsImpl::Wrap(35.+150.*Event+FMath::Min(Age,Duration)),Event,Member,S))continue;++Samples;if(!Clear(S.Position,80.) || (Had&&!SolidSegmentClear(Before.Position,S.Position,80.)))Reject(S,S.Position.Z-HouseCalderWildlifeSkyPathsImpl::Ground(FVector2D(S.Position)));Before=S;Had=true;}
    }
    // The full analytic trigonometric envelope covers every instant of each bat
    // circuit, including its 30cm wing radius; no temporal sampling can miss it.
    for(int32 Member=0;Member<8;++Member)
    {
        const double R=BatRadii[Member];const FVector Extent(R*1.14+30.,R*.90+30.,107.);const FBox Envelope(BatCentres[Member]-Extent,BatCentres[Member]+Extent);
        bool Good=true;for(const FBox& Box:Solids)if(Box.Intersect(Envelope)){Good=false;break;}
        for(double X=Envelope.Min.X;X<Envelope.Max.X+40.;X+=40.)for(double Y=Envelope.Min.Y;Y<Envelope.Max.Y+40.;Y+=40.)
        {++Samples;if(Envelope.Min.Z-HouseCalderWildlifeSkyPathsImpl::Ground(FVector2D(FMath::Min(X,Envelope.Max.X),FMath::Min(Y,Envelope.Max.Y)))<150.)Good=false;}
        if(!Good){FSample S;EvaluateBat(0.,Member,S);Reject(S,S.Position.Z-HouseCalderWildlifeSkyPathsImpl::Ground(FVector2D(S.Position)));}
    }
    for(const auto* CurveData:{&Goose[0],&Goose[1],&Crow})for(const auto& P:CurveData->Points)Canonical+=FString::Printf(TEXT("%.2f,%.2f,%.2f;"),P.X,P.Y,P.Z);
    for(const auto& P:BatCentres)Canonical+=FString::Printf(TEXT("bat:%.2f,%.2f,%.2f;"),P.X,P.Y,P.Z);RouteHash=FCrc::StrCrc32(*Canonical);for(const FBox& Box:Solids)Canonical+=Box.ToString();Canonical+=TEXT("radii115/80/30;V240/265;seed2026101803/1817/1831;cadence900/150;bat270or900/517/613/373/521");Hash=FCrc::StrCrc32(*Canonical);
    UE_LOG(LogTemp,Display,TEXT("CALDER_SKY_BUILD_STAGES boxesMs=%.2f routesMs=%.2f validateMs=%.2f cells=%d"),(BoxesAt-Began)*1000.,(RoutesAt-BoxesAt)*1000.,(FPlatformTime::Seconds()-RoutesAt)*1000.,SolidCells.Num());
    UE_LOG(LogTemp,Display,TEXT("CALDER_SKY_PATH_COUNTS rejects=%d/%d/%d/%d solids=%d continuousSolidSweeps=1 fullBatEnvelope=1"),KindFailures[0],KindFailures[1],KindFailures[2],KindFailures[3],Solids.Num());
    UE_LOG(LogTemp,Display,TEXT("CALDER_SKY_PATHS clear=%d/%d hash=%08x physicsQueries=0 overheadOnlyLandingPending=1 buildMs=%.2f"),Samples-Failures,Samples,Hash,(FPlatformTime::Seconds()-Began)*1000.);return Failures==0;
}
