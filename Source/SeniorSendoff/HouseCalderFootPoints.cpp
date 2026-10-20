// The Calder foot-point registry (see HouseCalderFootPoints.h).
#include "HouseCalderFootPoints.h"
#include "HouseCalderTerrain.h"
#include "Misc/ScopeLock.h"

namespace HouseCalderFootPointsImpl
{
struct FRegistry
{
    FCriticalSection Lock;
    TArray<HouseCalderFootPoints::FFootPoint> Points;
};

FRegistry& Registry()
{
    static FRegistry R;
    return R;
}

/** How far a gap lies outside [MinGap, MaxGap] (negative inside: minus the margin to the nearer bound). */
float Outside(float Gap)
{
    return FMath::Max(HouseCalderFootPoints::MinGap - Gap, Gap - HouseCalderFootPoints::MaxGap);
}
}

void HouseCalderFootPoints::Register(const TCHAR* Who, const FVector& Foot, float SurfaceZ)
{
    if (!HouseCalderTerrain::Enabled()) return;
    FFootPoint P;
    P.Who = FName(Who ? Who : TEXT("Unnamed"));
    P.Foot = Foot;
    P.SurfaceZ = SurfaceZ;
    HouseCalderFootPointsImpl::FRegistry& R = HouseCalderFootPointsImpl::Registry();
    FScopeLock Guard(&R.Lock);
    R.Points.Add(P);
}

void HouseCalderFootPoints::Reset()
{
    HouseCalderFootPointsImpl::FRegistry& R = HouseCalderFootPointsImpl::Registry();
    FScopeLock Guard(&R.Lock);
    R.Points.Reset();
}

int32 HouseCalderFootPoints::Num()
{
    HouseCalderFootPointsImpl::FRegistry& R = HouseCalderFootPointsImpl::Registry();
    FScopeLock Guard(&R.Lock);
    return R.Points.Num();
}

TArray<HouseCalderFootPoints::FFootPoint> HouseCalderFootPoints::Snapshot()
{
    HouseCalderFootPointsImpl::FRegistry& R = HouseCalderFootPointsImpl::Registry();
    FScopeLock Guard(&R.Lock);
    return R.Points;
}

HouseCalderFootPoints::FFootReport HouseCalderFootPoints::Check(TFunctionRef<bool(const FVector& Foot, float PlacerSurfaceZ, float& OutSurfaceZ)> Measure, int32 MaxWorst)
{
    FFootReport Report;
    const TArray<FFootPoint> All = Snapshot();
    TArray<FFootResult> Results;
    Results.Reserve(All.Num());
    Report.LowestGap = FLT_MAX;
    Report.HighestGap = -FLT_MAX;
    for (const FFootPoint& P : All)
    {
        FFootResult& R = Results.AddDefaulted_GetRef();
        R.Point = P;
        float Measured = P.SurfaceZ;
        R.bMeasured = Measure(P.Foot, P.SurfaceZ, Measured);
        R.MeasuredZ = R.bMeasured ? Measured : P.SurfaceZ;
        R.Gap = float(P.Foot.Z) - R.MeasuredZ;
        R.Outside = HouseCalderFootPointsImpl::Outside(R.Gap);
        const bool bBad = R.Outside > 0.f;
        Report.Bad += bBad ? 1 : 0;
        Report.Unmeasured += R.bMeasured ? 0 : 1;
        Report.LowestGap = FMath::Min(Report.LowestGap, R.Gap);
        Report.HighestGap = FMath::Max(Report.HighestGap, R.Gap);
        FIntPoint& Per = Report.ByWho.FindOrAdd(P.Who);
        Per.X += 1;
        Per.Y += bBad ? 1 : 0;
    }
    Report.Points = Results.Num();
    Report.Placers = Report.ByWho.Num();
    if (!Results.Num()) Report.LowestGap = Report.HighestGap = 0.f;
    // The furthest outside the rule first; a stable sort keeps registration order among equals (the same list every run).
    Results.StableSort([](const FFootResult& A, const FFootResult& B) { return A.Outside > B.Outside; });
    for (int32 K = 0; K < FMath::Min(MaxWorst, Results.Num()); ++K) Report.Worst.Add(Results[K]);
    return Report;
}
