#include "HouseCalderRoads.h"
#include "HouseBlackHallMesh.h"
#include "HouseCalderDrape.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderLandform.h"
#include "HouseCalderWater.h"
#include "HouseExteriorSiteLayout.h"

namespace HouseCalderRoadsImpl
{
constexpr double BucketSize = 1000.0;
struct FPolygon
{
    const TArray<FVector2D>* Points = nullptr;
    FBox2D Box = FBox2D(ForceInit);
    int32 RoadIndex = INDEX_NONE;
};
uint64 Key(int32 X, int32 Y) { return (uint64(uint32(X)) << 32) | uint32(Y); }
int32 Bucket(double Value) { return FMath::FloorToInt(Value / BucketSize); }
struct FGeometry
{
    TArray<FPolygon> Polygons;
    TMap<uint64, TArray<int32>> Buckets;
    TArray<TArray<double>> Distances;
    FGeometry()
    {
        const TArray<HouseCalderLandform::FRoad>& Roads = HouseCalderLandform::Roads();
        for (int32 RoadIndex = 0; RoadIndex < Roads.Num(); ++RoadIndex)
        {
            const HouseCalderLandform::FRoad& Road = Roads[RoadIndex];
            TArray<double>& Run = Distances.AddDefaulted_GetRef();
            Run.Add(0.0);
            for (int32 I = 1; I < Road.Points.Num(); ++I) Run.Add(Run.Last() + FVector2D::Distance(Road.Points[I - 1], Road.Points[I]));
            for (const TArray<FVector2D>& Points : Road.Polygons)
            {
                if (Points.Num() < 3) continue;
                FPolygon Poly;
                Poly.Points = &Points;
                Poly.RoadIndex = RoadIndex;
                for (const FVector2D& Point : Points) Poly.Box += Point;
                const int32 PolygonIndex = Polygons.Add(Poly);
                for (int32 X = Bucket(Poly.Box.Min.X); X <= Bucket(Poly.Box.Max.X); ++X)
                    for (int32 Y = Bucket(Poly.Box.Min.Y); Y <= Bucket(Poly.Box.Max.Y); ++Y)
                        Buckets.FindOrAdd(Key(X, Y)).Add(PolygonIndex);
            }
        }
    }
};
const FGeometry& Geometry() { static const FGeometry Value; return Value; }
double SegmentDistance(const FVector2D& P, const FVector2D& A, const FVector2D& B, double& Fraction)
{
    const FVector2D AB = B - A;
    Fraction = AB.SizeSquared() > 1e-9 ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / AB.SizeSquared(), 0.0, 1.0) : 0.0;
    return (P - A - AB * Fraction).Size();
}
bool Inside(const FVector2D& P, const TArray<FVector2D>& Points, float Pad)
{
    bool bPositive = false, bNegative = false;
    double Closest = TNumericLimits<double>::Max();
    for (int32 I = 0; I < Points.Num(); ++I)
    {
        const FVector2D& A = Points[I];
        const FVector2D& B = Points[(I + 1) % Points.Num()];
        const double Cross = (B.X - A.X) * (P.Y - A.Y) - (B.Y - A.Y) * (P.X - A.X);
        bPositive |= Cross > .001;
        bNegative |= Cross < -.001;
        if (Pad != 0.f) { double Fraction = 0.0; Closest = FMath::Min(Closest, SegmentDistance(P, A, B, Fraction)); }
    }
    const bool bInside = !(bPositive && bNegative);
    return Pad < 0.f ? bInside && Closest >= -Pad : bInside || (Pad > 0.f && Closest <= Pad);
}
int32 FindRoad(const FVector2D& P, float Pad)
{
    const FGeometry& G = Geometry();
    const double Reach = FMath::Max(0.f, Pad);
    for (int32 X = Bucket(P.X - Reach); X <= Bucket(P.X + Reach); ++X)
        for (int32 Y = Bucket(P.Y - Reach); Y <= Bucket(P.Y + Reach); ++Y)
            if (const TArray<int32>* Entries = G.Buckets.Find(Key(X, Y)))
                for (const int32 PolygonIndex : *Entries)
                {
                    const FPolygon& Poly = G.Polygons[PolygonIndex];
                    if (Poly.Box.ExpandBy(Pad + .001f).IsInside(P) && Inside(P, *Poly.Points, Pad)) return Poly.RoadIndex;
                }
    return INDEX_NONE;
}
double Along(const FVector2D& P, int32 RoadIndex)
{
    const HouseCalderLandform::FRoad& Road = HouseCalderLandform::Roads()[RoadIndex];
    const TArray<double>& Run = Geometry().Distances[RoadIndex];
    double Best = TNumericLimits<double>::Max(), Result = 0.0;
    for (int32 I = 1; I < Road.Points.Num(); ++I)
    {
        double Fraction = 0.0;
        const double Distance = SegmentDistance(P, Road.Points[I - 1], Road.Points[I], Fraction);
        if (Distance < Best) { Best = Distance; Result = FMath::Lerp(Run[I - 1], Run[I], Fraction); }
    }
    return Result;
}
double Smooth(double Value) { const double T = FMath::Clamp(Value, 0.0, 1.0); return T * T * (3.0 - 2.0 * T); }
float Offset(const FVector2D& P, int32 RoadIndex)
{
    const HouseCalderLandform::FRoad& Road = HouseCalderLandform::Roads()[RoadIndex];
    if (Road.Mode == 2) return 3.f;
    const double S = Along(P, RoadIndex), Length = Geometry().Distances[RoadIndex].Last();
    double Result = 3.0 - 15.0 * (1.0 - Smooth(S / 300.0));
    if (Road.Mode == 0) Result -= 15.0 * Smooth((S - (Length - 300.0)) / 300.0);
    return float(Result);
}
float RoadGround(const FVector2D& P,int32 RoadIndex)
{
    const HouseCalderLandform::FRoad& Road=HouseCalderLandform::Roads()[RoadIndex];
    FVector2D Read=P;
    if(Road.Mode!=2 && Road.Points.Num()>1)
    {
        const double S=Along(P,RoadIndex),Length=Geometry().Distances[RoadIndex].Last();
        // The slab reads 1 mm into its own side at a retaining line. Its
        // public datum must select that same side at an exact join endpoint.
        if(S<.001) Read+=(Road.Points[1]-Road.Points[0]).GetSafeNormal()*.1;
        else if(S>Length-.001) Read-=(Road.Points.Last()-Road.Points[Road.Points.Num()-2]).GetSafeNormal()*.1;
    }
    return HouseExteriorSite::GroundHeight(Read.X,Read.Y);
}
TArray<FVector2D> HalfPolygon(const TArray<FVector2D>& In, const FVector2D& Axis, double Datum, double Sign)
{
    TArray<FVector2D> Out;
    for(int32 I=0;I<In.Num();++I)
    {
        const FVector2D A=In[I],B=In[(I+1)%In.Num()];
        const double FA=Sign*(FVector2D::DotProduct(A,Axis)-Datum),FB=Sign*(FVector2D::DotProduct(B,Axis)-Datum);
        if(FA>=0.) Out.Add(A);
        if((FA>=0.)!=(FB>=0.)) Out.Add(FMath::Lerp(A,B,FA/(FA-FB)));
    }
    return Out;
}
void SplitAt(TArray<TArray<FVector2D>>& Parts,const FVector2D& Axis,double Datum)
{
    TArray<TArray<FVector2D>> Result;
    for(const TArray<FVector2D>& Part:Parts)
    {
        double Low=MAX_dbl,High=-MAX_dbl;
        for(const FVector2D& P:Part) { const double D=FVector2D::DotProduct(P,Axis)-Datum; Low=FMath::Min(Low,D); High=FMath::Max(High,D); }
        if(Low<-.001 && High>.001)
        {
            for(const double Sign:{-1.,1.})
            {
                TArray<FVector2D> Half=HalfPolygon(Part,Axis,Datum,Sign);
                if(Half.Num()>=3) Result.Add(MoveTemp(Half));
            }
        }
        else Result.Add(Part);
    }
    Parts=MoveTemp(Result);
}
}

bool HouseCalderRoads::Active() { return HouseCalderWater::Active() && !HouseCalderLandform::Roads().IsEmpty(); }
bool HouseCalderRoads::Contains(const FVector2D& P, float Pad)
{
    return Active() && HouseCalderRoadsImpl::FindRoad(P, Pad) != INDEX_NONE;
}
bool HouseCalderRoads::PaveOffsetAt(const FVector2D& P, float& OutOffset)
{
    if (!Active()) return false;
    const int32 Index = HouseCalderRoadsImpl::FindRoad(P, 0.f);
    if (Index == INDEX_NONE) return false;
    OutOffset = HouseCalderRoadsImpl::Offset(P, Index)+HouseCalderRoadsImpl::RoadGround(P,Index)-HouseExteriorSite::GroundHeight(P.X,P.Y);
    return true;
}
bool HouseCalderRoads::FloorAt(const FVector2D& P, float& OutZ)
{
    float Offset = 0.f;
    if (!PaveOffsetAt(P, Offset)) return false;
    OutZ = HouseExteriorSite::GroundHeight(float(P.X), float(P.Y)) + Offset;
    return true;
}
void HouseCalderRoads::TerrainCuts(TArray<FBox2D>& Out)
{
    if (!Active()) return;
    // Only recessed joins remove terrain. The rest of the asphalt is drawn
    // 3 cm above its continuous earthwork; the central creek bed stays intact.
    for (const FBox2D& Cut : {
        FBox2D(FVector2D(15000, -11000), FVector2D(15300, -10450)),
        FBox2D(FVector2D(23700, -7275), FVector2D(24000, -6725)),
        FBox2D(FVector2D(26000, -7275), FVector2D(26300, -6725)),
        FBox2D(FVector2D(24000, -7320), FVector2D(24300, -6680)),
        FBox2D(FVector2D(25700, -7320), FVector2D(26000, -6680)) }) Out.Add(Cut);
}
void HouseCalderRoads::BuildGround(FHallMesh& M, FName Asphalt, const FHallUV& UV)
{
    if (!Active()) return;
    for (int32 Index = 0; Index < HouseCalderLandform::Roads().Num(); ++Index)
        for (const TArray<FVector2D>& Polygon : HouseCalderLandform::Roads()[Index].Polygons)
        {
            const HouseCalderLandform::FRoad& Road=HouseCalderLandform::Roads()[Index];
            TArray<FVector2D> Joined=Polygon;
            if(Joined.Num()==4 && Road.Points.Num()>1)
            {
                const FVector2D A=(Joined[0]+Joined[1])*.5,B=(Joined[2]+Joined[3])*.5,Direction=(B-A).GetSafeNormal();
                if(FVector2D::Distance(A,Road.Points[0])>.01) { Joined[0]-=Direction*.2; Joined[1]-=Direction*.2; }
                if(FVector2D::Distance(B,Road.Points.Last())>.01) { Joined[2]+=Direction*.2; Joined[3]+=Direction*.2; }
            }
            TArray<TArray<FVector2D>> Parts={Joined};
            if(Road.Mode!=2 && Road.Points.Num()>1)
            {
                const FVector2D First=(Road.Points[1]-Road.Points[0]).GetSafeNormal();
                const FVector2D Last=(Road.Points.Last()-Road.Points[Road.Points.Num()-2]).GetSafeNormal();
                // A 3 m smooth recessed join needs finer geometry than the
                // ground's 2 m lattice to reproduce its finished surface.
                for(double S=50.;S<=300.;S+=50.)
                {
                    HouseCalderRoadsImpl::SplitAt(Parts,First,FVector2D::DotProduct(Road.Points[0],First)+S);
                    if(Road.Mode==0) HouseCalderRoadsImpl::SplitAt(Parts,Last,FVector2D::DotProduct(Road.Points.Last(),Last)-S);
                }
            }
            for(const TArray<FVector2D>& Part:Parts) HouseCalderDrape::Slab(M, Asphalt, Part,
                [Index](float X, float Y) { return HouseExteriorSite::GroundHeight(X, Y) + HouseCalderRoadsImpl::Offset(FVector2D(X, Y), Index); },
                [](float X, float Y) { return HouseExteriorSite::GroundHeight(X, Y) - 20.f; }, UV);
        }
}
void HouseCalderRoads::FloorSamples(TArray<FVector>& Out)
{
    if (!Active()) return;
    for (const HouseCalderLandform::FRoad& Road : HouseCalderLandform::Roads())
    {
        for (int32 I = 1; I < Road.Points.Num(); ++I)
        {
            const FVector2D A = Road.Points[I - 1], B = Road.Points[I], Delta = B - A;
            const double Length = Delta.Size();
            const FVector2D Side(-Delta.Y / Length, Delta.X / Length);
            const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length / 200.0));
            for (int32 K = 0; K <= Steps; ++K)
                for (const double Across : { -.4 * Road.Width, 0.0, .4 * Road.Width })
                {
                    const FVector2D P = FMath::Lerp(A, B, double(K) / Steps) + Side * Across;
                    float Z = 0.f;
                    if (FloorAt(P, Z)) Out.Add(FVector(P, Z));
                }
        }
        if (Road.Mode == 2 && Road.Points.Num())
        {
            const FVector2D P = Road.Points[0];
            float Z = 0.f;
            if (FloorAt(P, Z)) Out.Add(FVector(P, Z));
        }
    }
}
