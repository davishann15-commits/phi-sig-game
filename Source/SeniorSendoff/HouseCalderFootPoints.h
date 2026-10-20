// The Calder foot-point registry (SourceArt/CalderLandscape/MASTER_PROMPT.md, "Verification and deliverables": buried or
// floating objects). Every new Calder placer (boulders, logs, posts, benches, stones, culvert headwalls, wildlife props ...)
// registers, at build time, the point its lowest foot or footing rests at and the surface it read there; -CalderTerrainReview
// measures the surface again in the built world and holds every point to the rule: the foot lies within -10..+3 cm of its
// surface (a footing may sink up to 10 cm into the ground, nothing floats more than 3 cm over it), and lists the worst 20.
//
//   HouseCalderFootPoints::Register(TEXT("CalderBoulders"), FVector(X, Y, FootZ), HouseCalderTerrain::SurfaceZ(X, Y));
//
// Who names the placer (a literal; it is kept as an FName). Register is thread-safe (placers may call it from ParallelFor)
// and does nothing while the Calder terrain is off (-NoCalderTerrain), so the switch stays today's. The exterior calls Reset()
// at the start of its build, so a rebuilt world never mixes in an old world's points.
#pragma once

#include "CoreMinimal.h"

namespace HouseCalderFootPoints
{
/** The rule: Foot.Z - the surface under it, in cm. */
constexpr float MinGap = -10.f, MaxGap = 3.f;

struct FFootPoint
{
    FName Who;
    FVector Foot = FVector::ZeroVector;
    float SurfaceZ = 0.f;   // the surface as the placer read it
};

/** Records one foot point (see above). */
void Register(const TCHAR* Who, const FVector& Foot, float SurfaceZ);
/** Forgets every point. */
void Reset();
/** How many points are registered. */
int32 Num();
/** A copy of every point, in registration order. */
TArray<FFootPoint> Snapshot();

/** One point as the review found it: the surface it measured (or the placer's own reading where it could not measure one),
 *  Gap = Foot.Z - that surface, and how far the gap lies outside the rule (<= 0 inside it: the margin left). */
struct FFootResult
{
    FFootPoint Point;
    float MeasuredZ = 0.f;
    float Gap = 0.f;
    float Outside = 0.f;
    bool bMeasured = false;
};

struct FFootReport
{
    int32 Points = 0, Bad = 0, Unmeasured = 0, Placers = 0;
    float LowestGap = 0.f, HighestGap = 0.f;
    /** Up to MaxWorst points, the furthest outside the rule first (then the nearest to it). */
    TArray<FFootResult> Worst;
    /** Points and failures per placer. */
    TMap<FName, FIntPoint> ByWho;
    bool Pass() const { return Bad == 0; }
};

/** Holds every point to the rule. Measure(Foot, PlacerSurfaceZ, OutSurfaceZ) finds the surface under the foot in the built
 *  world and returns false where it cannot tell (the placer's own reading is then used, and the point counts as unmeasured). */
FFootReport Check(TFunctionRef<bool(const FVector& Foot, float PlacerSurfaceZ, float& OutSurfaceZ)> Measure, int32 MaxWorst = 20);
}
