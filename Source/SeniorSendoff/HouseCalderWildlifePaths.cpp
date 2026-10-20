#include "HouseCalderWildlifePaths.h"
#include "HouseCalderMapTrees.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderFarm.h"
#include "HouseCalderLandform.h"
#include "HouseCalderRoadside.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "HouseCampusLayout.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseWoodsDetail.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Misc/Crc.h"
#include "PhysicsEngine/BodySetup.h"

namespace HouseCalderWildlifePathsImpl
{
double Distance(const FVector2D& P,const FVector2D& A,const FVector2D& B)
{
    const FVector2D D=B-A;
    return FVector2D::Distance(P,A+D*(D.SizeSquared()>.01?FMath::Clamp(FVector2D::DotProduct(P-A,D)/D.SizeSquared(),0.,1.):0.));
}
bool Protected(const FVector2D& P)
{
    // Includes the main house, its entire front/side/court/rear gardens, party
    // field, parking and west field. The separately permitted woods stay open.
    return FBox2D(FVector2D(-21100,-6100),FVector2D(9100,5500)).IsInsideOrOn(P)
        || FBox2D(FVector2D(HouseWoods::HutX-1100,HouseWoods::HutY-1100),FVector2D(HouseWoods::HutX+1100,HouseWoods::HutY+1100)).IsInsideOrOn(P);
}
FVector Ground(const FVector2D& P) { return FVector(P,HouseCalderTerrain::SurfaceZ(P.X,P.Y)); }
}

bool HouseCalderWildlifePaths::FPlan::PointClear(const FVector2D& P) const
{
    if (HouseCalderFarm::Occupied(P,135)) return false;
    if (P.X<-31000 || P.X>31000 || P.Y<-31000 || P.Y>31000 || HouseCalderWildlifePathsImpl::Protected(P)) return false;
    if (HouseCampusLayout::PaveKindAt(P)!=HouseCalderTerrain::EPave::Lawn
        || HouseCampusLayout::PavingNear(P,160) || HouseCalderFeatures::InStair(P,170)
        || HouseCalderFeatures::InRamp(P,170) || HouseCalderRoadside::Occupied(P,150)) return false;
    for (const auto& B:HouseCampusLayout::Blocks()) if (B.Rect().ExpandBy(220).IsInsideOrOn(P)) return false;
    for (const auto& A:Aprons) if (A.IsInsideOrOn(P)) return false;
    for (const auto& B:Trunks) if (B.IsInsideOrOn(P)) return false;
    for (const auto& B:Solids) if (B.IsInsideOrOn(P)) return false;
    for (const auto& W:HouseCalderLandform::Walls()) if (HouseCalderWildlifePathsImpl::Distance(P,W.A,W.B)<170.) return false;
    for (const auto& H:HouseCalderLandform::HedgeSegments()) if (HouseCalderWildlifePathsImpl::Distance(P,H.A,H.B)<175.) return false;
    for (const auto& Road:HouseCalderLandform::Roads())
        for (int32 I=1;I<Road.Points.Num();++I)
            if (HouseCalderWildlifePathsImpl::Distance(P,Road.Points[I-1],Road.Points[I])<Road.Width*.5+160.) return false;
    for (const auto& Path:HouseCalderLandform::LandformPaths())
        for (int32 I=0;I<Path.Points.Num()-(Path.bClosed?0:1);++I)
            if (HouseCalderWildlifePathsImpl::Distance(P,Path.Points[I],Path.Points[(I+1)%Path.Points.Num()])<Path.Width*.5+160.) return false;
    float Level=0.f;
    if (HouseCalderWater::WaterAt(P,Level) || HouseCalderWater::DeckAt(P,Level)) return false;
    const double Z=HouseCalderTerrain::SurfaceZ(P.X,P.Y);
    for (const FVector2D Offset:{FVector2D(130,0),FVector2D(-130,0),FVector2D(0,130),FVector2D(0,-130)})
    {
        const FVector2D Q=P+Offset;
        if (HouseCalderWildlifePathsImpl::Protected(Q) || HouseCalderWater::WaterAt(Q,Level)
            || FMath::Abs(HouseCalderTerrain::SurfaceZ(Q.X,Q.Y)-Z)>62.) return false;
    }
    return Z>HouseCalderLandform::MinWalkableZ();
}
bool HouseCalderWildlifePaths::FPlan::SegmentClear(const FVector& A,const FVector& B) const
{
    const int32 N=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(FVector2D(A),FVector2D(B))/45.));
    double Before=A.Z;
    for (int32 I=0;I<=N;++I)
    {
        const FVector2D P=FMath::Lerp(FVector2D(A),FVector2D(B),double(I)/N);
        if (!PointClear(P)) return false;
        const double Z=HouseCalderTerrain::SurfaceZ(P.X,P.Y);
        if (I>0 && FMath::Abs(Z-Before)>23.) return false;
        Before=Z;
    }
    return true;
}
bool HouseCalderWildlifePaths::FPlan::Build(UWorld* World)
{
    const double Start=FPlatformTime::Seconds();
    Trunks.Reset();Solids.Reset();Aprons.Reset();Groups.Reset();PathSamples=0;
    TArray<HouseCampusLayout::FApron> AuthoredAprons;HouseCampusLayout::EntranceAprons(AuthoredAprons);
    for(const auto& A:AuthoredAprons)Aprons.Add(A.R.ExpandBy(180));
    for (TActorIterator<AActor> Actor(World);Actor;++Actor)
    {
        TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(*Actor);
        for (auto* Part:Parts)
        {
            const FString Name=Part->GetName();const bool Tree=HouseCalderMapTrees::TreeBatch(Name);
            // Original collidable woods rocks and deadwood are independent of
            // standing-tree batches. Their physics bounds are analytic inputs,
            // never runtime physics queries or changes to the protected props.
            const bool Solid=Name.StartsWith(TEXT("ExteriorWoods")) || Name.StartsWith(TEXT("ExteriorAppalachian"));
            if ((!Tree && !Solid) || Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision) continue;
            UStaticMesh* Mesh=Part->GetStaticMesh();auto* Body=Mesh?Mesh->GetBodySetup():nullptr;if (!Body) continue;
            for (int32 I=0;I<Part->GetInstanceCount();++I)
            {
                FTransform Pose;Part->GetInstanceTransform(I,Pose,true);
                FBox Box=Body->AggGeom.CalcAABB(Pose);
                if(!Box.IsValid && !Tree)Box=Mesh->GetBoundingBox().TransformBy(Pose.ToMatrixWithScale());
                if (Box.IsValid)
                {const FBox2D Bounds=FBox2D(FVector2D(Box.Min),FVector2D(Box.Max)).ExpandBy(135);if(Tree)Trunks.Add(Bounds);else Solids.Add(Bounds);}
            }
        }
    }
    const FVector2D Centres[]={FVector2D(-4700,8250),FVector2D(-25200,6800),FVector2D(-18800,-11000)};
    for (int32 Group=0;Group<3;++Group)
    {
        FGraph Candidate;Candidate.Campus=Group==2;
        TMap<FIntPoint,int32> Grid;
        for (int32 Y=-10;Y<=10;++Y) for (int32 X=-8;X<=8;++X)
        {
            const FVector2D P=Centres[Group]+FVector2D(X*350.,Y*350.);
            if (PointClear(P)) { Grid.Add(FIntPoint(X,Y),Candidate.Points.Num());Candidate.Points.Add(HouseCalderWildlifePathsImpl::Ground(P));Candidate.Links.AddDefaulted(); }
        }
        const FIntPoint Steps[]={FIntPoint(1,0),FIntPoint(0,1),FIntPoint(1,1),FIntPoint(-1,1)};
        for (int32 Y=-10;Y<=10;++Y) for (int32 X=-8;X<=8;++X)
        {
            const int32* A=Grid.Find(FIntPoint(X,Y));if (!A) continue;
            for (const auto Step:Steps) if (const int32* B=Grid.Find(FIntPoint(X,Y)+Step))
                if (SegmentClear(Candidate.Points[*A],Candidate.Points[*B]))
                {Candidate.Links[*A].Add(*B);Candidate.Links[*B].Add(*A);}
        }
        TSet<int32> Seen;TArray<int32> Largest;
        for (int32 I=0;I<Candidate.Points.Num();++I) if (!Seen.Contains(I))
        {
            TArray<int32> Queue={I};Seen.Add(I);
            for (int32 Q=0;Q<Queue.Num();++Q) for (const int32 Next:Candidate.Links[Queue[Q]])
                if (!Seen.Contains(Next)) {Seen.Add(Next);Queue.Add(Next);}
            if (Queue.Num()>Largest.Num()) Largest=MoveTemp(Queue);
        }
        Largest.Sort();FGraph Final;Final.Campus=Candidate.Campus;TMap<int32,int32> Remap;
        for (const int32 I:Largest) {Remap.Add(I,Final.Points.Num());Final.Points.Add(Candidate.Points[I]);Final.Links.AddDefaulted();}
        for (const int32 I:Largest) for (const int32 J:Candidate.Links[I])
            if (const int32* Mapped=Remap.Find(J)) Final.Links[Remap[I]].Add(*Mapped);
        for (auto& Links:Final.Links) {Links.Sort();Final.Edges+=Links.Num();}Final.Edges/=2;
        UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_HABITAT group=%d nodes=%d edges=%d campus=%d"),Group,Final.Points.Num(),Final.Edges,int32(Final.Campus));
        if (Final.Points.Num()<8 || Final.Edges<7) return false;
        Groups.Add(MoveTemp(Final));
    }
    FString Canonical;int32 Clear=0,Total=0;
    for (int32 G=0;G<Groups.Num();++G)
    {
        const auto& Graph=Groups[G];
        for (int32 I=0;I<Graph.Points.Num();++I)
        {
            Canonical+=FString::Printf(TEXT("%d/%d:%.1f,%.1f,%.1f;"),G,I,Graph.Points[I].X,Graph.Points[I].Y,Graph.Points[I].Z);
            for (const int32 J:Graph.Links[I]) if (I<J)
            {++Total;Clear+=SegmentClear(Graph.Points[I],Graph.Points[J])?1:0;Canonical+=FString::Printf(TEXT("%d>%d;"),I,J);}
        }
    }
    PathSamples=Clear;Hash=FCrc::StrCrc32(*Canonical);
    UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_PATHS clear=%d/%d groups=3 trunks=%d solidProps=%d hash=%08x physicsQueries=0 buildMs=%.2f"),Clear,Total,Trunks.Num(),Solids.Num(),Hash,(FPlatformTime::Seconds()-Start)*1000.);
    return Total>0 && Clear==Total;
}
bool HouseCalderWildlifePaths::FPlan::Route(int32 Group,const FVector& From,const FVector& To,TArray<FVector>& Out) const
{
    Out.Reset();if (!Groups.IsValidIndex(Group)) return false;
    if (From.Equals(To,.11)) {Out.Add(From);return true;}
    const FGraph& G=Groups[Group];TArray<double> Cost;Cost.Init(TNumericLimits<double>::Max(),G.Points.Num());
    TArray<int32> Previous;Previous.Init(INDEX_NONE,G.Points.Num());TSet<int32> Closed;
    TArray<TPair<double,int32>> Starts,Ends;
    for (int32 I=0;I<G.Points.Num();++I)
    {Starts.Emplace(FVector::DistSquared(From,G.Points[I]),I);Ends.Emplace(FVector::DistSquared(To,G.Points[I]),I);}
    const auto Sort=[](const auto& A,const auto& B){return A.Key==B.Key?A.Value<B.Value:A.Key<B.Key;};Starts.Sort(Sort);Ends.Sort(Sort);
    int32 First=INDEX_NONE,Last=INDEX_NONE;
    for (int32 I=0;I<FMath::Min(8,Starts.Num());++I) if (SegmentClear(From,G.Points[Starts[I].Value])) {First=Starts[I].Value;break;}
    for (int32 I=0;I<FMath::Min(8,Ends.Num());++I) if (SegmentClear(To,G.Points[Ends[I].Value])) {Last=Ends[I].Value;break;}
    if (First<0 || Last<0) return false;Cost[First]=0.;
    for (int32 Step=0;Step<G.Points.Num();++Step)
    {
        int32 Best=INDEX_NONE;double Distance=TNumericLimits<double>::Max();
        for (int32 I=0;I<Cost.Num();++I) if (!Closed.Contains(I) && Cost[I]<Distance) {Best=I;Distance=Cost[I];}
        if (Best<0) break;if (Best==Last) break;Closed.Add(Best);
        for (const int32 Next:G.Links[Best])
        {const double C=Distance+FVector::Distance(G.Points[Best],G.Points[Next]);if (C<Cost[Next]) {Cost[Next]=C;Previous[Next]=Best;}}
    }
    if (Cost[Last]==TNumericLimits<double>::Max()) return false;
    TArray<int32> Reverse;for (int32 I=Last;I>=0;I=Previous[I]) {Reverse.Add(I);if (I==First) break;}
    Out.Add(From);for (int32 I=Reverse.Num()-1;I>=0;--I) if (!Out.Last().Equals(G.Points[Reverse[I]],.1)) Out.Add(G.Points[Reverse[I]]);
    if (!Out.Last().Equals(To,.1)) Out.Add(To);
    // Pull a path through the already-validated free space instead of walking
    // the square grid. Every shortcut and rounded corner is checked against
    // the same protected geometry, trees, water and slope constraints.
    TArray<FVector> Pulled;Pulled.Add(Out[0]);
    for(int32 I=0;I<Out.Num()-1;)
    {
        int32 LastVisible=I+1;
        for(int32 J=Out.Num()-1;J>I+1;--J)if(SegmentClear(Out[I],Out[J])){LastVisible=J;break;}
        Pulled.Add(Out[LastVisible]);I=LastVisible;
    }
    TArray<FVector> Rounded;Rounded.Add(Pulled[0]);
    for(int32 I=1;I<Pulled.Num()-1;++I)
    {
        const FVector Corner=Pulled[I];
        const double Cut=FMath::Min(110.,FMath::Min(FVector::Distance(Pulled[I-1],Corner),FVector::Distance(Pulled[I+1],Corner))*.23);
        const FVector A=Corner+(Pulled[I-1]-Corner).GetSafeNormal()*Cut,B=Corner+(Pulled[I+1]-Corner).GetSafeNormal()*Cut;
        TArray<FVector> Curve;bool Clear=true;FVector Before=Rounded.Last();
        for(int32 K=0;K<=6;++K)
        {
            const double T=double(K)/6.,U=1.-T;FVector P=A*(U*U)+Corner*(2.*U*T)+B*(T*T);P.Z=HouseCalderTerrain::SurfaceZ(P.X,P.Y);
            if(!SegmentClear(Before,P)){Clear=false;break;}Curve.Add(P);Before=P;
        }
        if(Clear&&SegmentClear(Before,Pulled[I+1]))Rounded.Append(Curve);else Rounded.Add(Corner);
    }
    if(!Rounded.Last().Equals(Pulled.Last(),.1))Rounded.Add(Pulled.Last());Out=MoveTemp(Rounded);return true;
}
double HouseCalderWildlifePaths::Length(const TArray<FVector>& Route)
{double D=0.;for(int32 I=1;I<Route.Num();++I)D+=FVector::Dist(Route[I-1],Route[I]);return D;}
double HouseCalderWildlifePaths::TravelSeconds(double Span,double Speed)
{if(Span<=.001||Speed<=0.)return 0.;const double Ramp=FMath::Min(Speed>500.?.24:.6,Span/Speed*.35);return Span/Speed+Ramp;}
double HouseCalderWildlifePaths::TravelDistance(double Age,double Span,double Speed)
{
    if(Span<=.001||Speed<=0.)return 0.;const double Ramp=FMath::Min(Speed>500.?.24:.6,Span/Speed*.35),Duration=Span/Speed+Ramp,T=FMath::Clamp(Age,0.,Duration);
    if(T<Ramp)return Speed*T*T/(2.*Ramp);
    if(T>Duration-Ramp)return Span-Speed*FMath::Square(Duration-T)/(2.*Ramp);
    return Speed*(T-Ramp*.5);
}
FVector HouseCalderWildlifePaths::Evaluate(const TArray<FVector>& Route,double Distance,FVector* Tangent)
{
    if (Route.IsEmpty()) return FVector::ZeroVector;
    for (int32 I=1;I<Route.Num();++I)
    {
        const FVector Delta=Route[I]-Route[I-1];const double Span=Delta.Size();
        if (Distance<=Span || I==Route.Num()-1)
        {if (Tangent)*Tangent=Delta.GetSafeNormal2D();FVector P=FMath::Lerp(Route[I-1],Route[I],FMath::Clamp(Distance/FMath::Max(Span,.001),0.,1.));P.Z=HouseCalderTerrain::SurfaceZ(P.X,P.Y);return P;}
        Distance-=Span;
    }
    if (Tangent)*Tangent=FVector::ForwardVector;return Route.Last();
}
