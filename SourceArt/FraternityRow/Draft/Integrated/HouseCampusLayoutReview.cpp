#include "HouseFraternityRow.h"
// -CampusLayoutReview: checks the Calder State campus plan (HouseCampusLayout) once the exterior has been built.
//  - from the plan's data: no building overlapping a road, lot, walk or other building; every entrance within 2 m of the walk
//    network; every lot reached by a road; one continuous pedestrian network, on a road only at a crossing; no road ending in
//    the open; no lamp in a building or on a lane;
//    every walk that ends on a building ends at one of its entrances;
//  - in the world: a floor at its own height (to 1.5 cm) under every sample of the walks, their run-ons, the curb ramps and
//    the curved sidewalks, with a player's clearance over it; no tree or utility pole standing in a building or on a traffic
//    lane and no trunk collision reaching one or a walk; and the exterior light count;
//  - on the Calder terrain: the samples carry the ground's own heights (HouseCalderTerrain::SurfaceZ), the review waits for
//    the terrain's collision, and every entrance apron's samples sit on their building's pad, where the ground must stand
//    (SurfaceZ, apron class, to 1 mm);
//  - on the Calder stairs and ramps (HouseCalderFeatures): a walk sample inside a stair's or a ramp's footprint is not at walk
//    height (the treads or the slab stand over it), so it is skipped (stairSkipped=) and every tread and ramp is checked
//    instead with the same 1.5 cm floor trace (stairSamples=, counted in floorSamples and floorMisses); the headroom check
//    also passes over samples within a capsule's reach of one (its risers and rails are the stair, not an obstruction).
// Logs CAMPUS_LAYOUT_REVIEW PASS/FAIL with the counts and writes Saved/CampusLayoutReview/result.txt (deleted first, so no
// stale verdict survives). It exits the game on a failure, and on a pass too when no other review, smoke test or capture
// shares the run.
#include "HouseCampusLayout.h"
#if !UE_BUILD_SHIPPING
#include "HouseCalderFeatures.h"
#include "HouseCalderTerrain.h"
#include "HouseExterior.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace HouseCampusLayoutReviewImpl
{
struct FState { TWeakObjectPtr<UWorld> World; double Start = 0.0; bool bDone = false; };
FState& State() { static FState S; return S; }

/** A review or capture that ends the run itself shares it, so a pass here must not end it first. The list is explicit:
 *  the reviews that exit only on a failure (CampusWoods, FraternityBoundary, SurroundingsPolish, WoodsPass) are not on it,
 *  so a run of those with this one still ends when this one passes. */
bool SharedRun()
{
    static const TCHAR* const SelfExiting[] = {
        TEXT("FreeRoamWalkReview"), TEXT("ParkingLotWalkReview"), TEXT("WoodsWalkReview"), TEXT("ClockTowerReview"), TEXT("HouseExteriorViews"),
        TEXT("CampusInteriorsReview"), TEXT("CampusInteriorsSmoke"), TEXT("HouseExteriorWalkReview"), TEXT("HouseWalkPerf"), TEXT("CampusDrumsRecord"), TEXT("WestFieldWalkReview"), TEXT("CalderTerrainReview"), TEXT("CalderPigVisualReview"), TEXT("CalderAccessReview"), TEXT("CalderFarmSolidWalkReview"), TEXT("CalderWildlifeReview"), TEXT("CalderWildlifePathsReview"), TEXT("CalderWildlifeNetworkReview"), TEXT("CalderWildlifeBirdArtReview"), TEXT("CalderWildlifeSkyPathsReview"), TEXT("CalderWildlifeSkyReview"), TEXT("CalderGoosePondReview"), TEXT("CalderGoosePondNetworkReview"), TEXT("CalderReachReview"), TEXT("CalderLandscapeReview"), TEXT("CalderTreePreview"), TEXT("CalderTreeFitReview"), TEXT("CalderRidgeForestReview") };
    TArray<FString> Tokens, Switches;
    FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);
    for (FString S : Switches)
    {
        int32 Eq = INDEX_NONE;
        if (S.FindChar(TEXT('='), Eq)) S.LeftInline(Eq);   // the switch's name only (-UserDir=.../review must not count)
        for (const TCHAR* Name : SelfExiting) if (S.Equals(Name, ESearchCase::CaseSensitive)) return true;
    }
    return false;
}
}

void TickHouseCampusLayoutReview(UWorld* W)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("CampusLayoutReview")) || !W || UGameplayStatics::GetCurrentLevelName(W, true) != TEXT("Chapter01_House")) return;
    HouseCampusLayoutReviewImpl::FState& S = HouseCampusLayoutReviewImpl::State();
    if (S.World.Get() != W)
    {
        S = HouseCampusLayoutReviewImpl::FState(); S.World = W; S.Start = W->GetTimeSeconds();
        IFileManager::Get().Delete(*(FPaths::ProjectSavedDir() / TEXT("CampusLayoutReview") / TEXT("result.txt")), false, true, true);   // no stale verdict
    }
    if (S.bDone) return;
    AHouseExteriorDetail* Exterior = nullptr;
    for (TActorIterator<AHouseExteriorDetail> It(W); It; ++It) Exterior = *It;
    if (!Exterior || W->GetTimeSeconds() - S.Start < 6.0 || !HouseCalderTerrain::CollisionReady(W)) return;
    S.bDone = true;

    const FString Dir = FPaths::ProjectSavedDir() / TEXT("CampusLayoutReview");
    IFileManager::Get().MakeDirectory(*Dir, true);
    FString Log;
    auto Note = [&Log](const FString& T) { Log += T + TEXT("\n"); UE_LOG(LogTemp, Display, TEXT("CAMPUS_LAYOUT_REVIEW %s"), *T); };
    auto Finish = [&](bool bOK, const FString& T)
    {
        Note((bOK ? TEXT("PASS ") : TEXT("FAIL ")) + T);
        FFileHelper::SaveStringToFile(Log, *(Dir / TEXT("result.txt")));
        if (!bOK || !HouseCampusLayoutReviewImpl::SharedRun()) FPlatformMisc::RequestExitWithStatus(false, bOK ? 0 : 1);
    };
    if (!HouseCampusLayout::Enabled()) { Finish(true, TEXT("plan=off (-NoCampusLayout): the old campus has no plan to check")); return; }

    // The plan's own data.
    HouseCampusLayout::FPlanReport R;
    HouseCampusLayout::CheckPlan(R);
    for (const FString& P : R.Problems) Note(TEXT("problem: ") + P);

    // A floor at walk height under every walk sample (a hole, a missing collision or a buried walk all show up here; the lawn
    // under a missing walk is 3 cm lower, so the tolerance is tighter than that).
    TArray<FVector> Samples;
    HouseCampusLayout::WalkSamples(Samples);
    // The Calder stairs and ramps: walk samples inside a stair's or a ramp's footprint (5 cm round it) stand under its treads or
    // slab and are skipped; every tread's own samples (its centre and 25 cm in from each side) and every ramp's (each metre,
    // centre and 25 cm in from each side) are traced instead.
    auto OnStairOrRamp = [](const FVector& P, float Pad) { const FVector2D P2(P.X, P.Y); return HouseCalderFeatures::InStair(P2, Pad) || HouseCalderFeatures::InRamp(P2, Pad); };
    TArray<bool> OnStair;
    OnStair.SetNumZeroed(Samples.Num());
    int32 StairSkipped = 0;
    for (int32 I = 0; I < Samples.Num(); ++I)
        if (OnStairOrRamp(Samples[I], 5.f)) { OnStair[I] = true; ++StairSkipped; }
    TArray<FVector> StairPoints;
    HouseCalderFeatures::StairSamples(StairPoints);
    const int32 TreadPoints = StairPoints.Num();
    HouseCalderFeatures::RampSamples(StairPoints);
    int32 FloorMisses = 0, FloorChecks = 0;
    // Every problem sample, not only the first few logged, for the terrain work's clustering (problems.csv: x,y,z,kind,detail).
    FString ProblemCsv = TEXT("x,y,z,kind,detail\n");
    auto Csv = [&ProblemCsv](const FVector& P, const TCHAR* Kind, const FString& Detail) { ProblemCsv += FString::Printf(TEXT("%.0f,%.0f,%.1f,%s,%s\n"), P.X, P.Y, P.Z, Kind, *Detail.Replace(TEXT(","), TEXT(";"))); };
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CampusLayoutFloor), true);
    // The property-boundary walls (on only with -PropertyBoundary) stand across the campus's edge on purpose.
    {
        TInlineComponentArray<UPrimitiveComponent*> Parts(Exterior);
        for (UPrimitiveComponent* C : Parts) if (C && C->GetName().StartsWith(TEXT("PropertyBoundary"))) Q.AddIgnoredComponent(C);
    }
    auto FloorCheck = [&](const FVector& P, const TCHAR* What)
    {
        ++FloorChecks;
        FHitResult Hit;
        const bool bHit = W->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 60.f), P - FVector(0, 0, 200.f), ECC_Pawn, Q);
        if (!bHit || FMath::Abs(Hit.ImpactPoint.Z - P.Z) > 1.5f)
        {
            Csv(P, What, bHit ? FString::Printf(TEXT("hit %s at Z=%.1f"), *GetNameSafe(Hit.GetComponent()), Hit.ImpactPoint.Z) : FString(TEXT("nothing")));
            if (++FloorMisses <= 20)
                Note(FString::Printf(TEXT("problem: no floor at %s height at (%.0f,%.0f): %s"), What, P.X, P.Y,
                    bHit ? *FString::Printf(TEXT("hit %s at Z=%.1f"), *GetNameSafe(Hit.GetComponent()), Hit.ImpactPoint.Z) : TEXT("nothing")));
            // With -CampusLayoutReviewProbe (diagnosis only): what was hit, its footprint on a 10 cm grid round the sample and,
            // for a procedural mesh, the triangles under the sample at that height.
            if (bHit && FloorMisses <= 6 && FParse::Param(FCommandLine::Get(), TEXT("CampusLayoutReviewProbe")))
            {
                FBox2D Foot(ForceInit);
                for (int32 GX = -15; GX <= 15; ++GX)
                    for (int32 GY = -15; GY <= 15; ++GY)
                    {
                        FHitResult H2;
                        const FVector At = P + FVector(GX * 10.f, GY * 10.f, 0.f);
                        if (W->LineTraceSingleByChannel(H2, At + FVector(0, 0, 60.f), At - FVector(0, 0, 200.f), ECC_Pawn, Q) && H2.GetComponent() == Hit.GetComponent() && FMath::Abs(H2.ImpactPoint.Z - Hit.ImpactPoint.Z) < 30.f)
                            Foot += FVector2D(At.X, At.Y);
                    }
                Note(FString::Printf(TEXT("probe: %s near (%.0f,%.0f): footprint %s normal %s"), *GetNameSafe(Hit.GetComponent()), P.X, P.Y, Foot.bIsValid ? *FString::Printf(TEXT("X %.0f..%.0f Y %.0f..%.0f"), Foot.Min.X, Foot.Max.X, Foot.Min.Y, Foot.Max.Y) : TEXT("-"), *Hit.ImpactNormal.ToCompactString()));
                if (const UProceduralMeshComponent* PM = Cast<UProceduralMeshComponent>(Hit.GetComponent()))
                {
                    int32 Shown = 0;
                    for (int32 Sec = 0; Sec < PM->GetNumSections() && Shown < 3; ++Sec)
                    {
                        const FProcMeshSection* S2 = const_cast<UProceduralMeshComponent*>(PM)->GetProcMeshSection(Sec);
                        if (!S2) continue;
                        for (int32 I3 = 0; I3 + 2 < S2->ProcIndexBuffer.Num() && Shown < 3; I3 += 3)
                        {
                            const FVector V0 = S2->ProcVertexBuffer[S2->ProcIndexBuffer[I3]].Position, V1 = S2->ProcVertexBuffer[S2->ProcIndexBuffer[I3 + 1]].Position, V2 = S2->ProcVertexBuffer[S2->ProcIndexBuffer[I3 + 2]].Position;
                            const FBox Tb(TArray<FVector>{ V0, V1, V2 });
                            if (Tb.Min.X > P.X + 5.f || Tb.Max.X < P.X - 5.f || Tb.Min.Y > P.Y + 5.f || Tb.Max.Y < P.Y - 5.f || Tb.Min.Z > Hit.ImpactPoint.Z + 10.f || Tb.Max.Z < Hit.ImpactPoint.Z - 10.f) continue;
                            ++Shown;
                            Note(FString::Printf(TEXT("probe tri: section %d material %s: %s %s %s"), Sec, *GetNameSafe(PM->GetMaterial(Sec)), *V0.ToCompactString(), *V1.ToCompactString(), *V2.ToCompactString()));
                        }
                    }
                }
            }
        }
    };
    for (int32 I = 0; I < Samples.Num(); ++I) if (!OnStair[I]) FloorCheck(Samples[I], TEXT("walk"));
    for (int32 I = 0; I < StairPoints.Num(); ++I) FloorCheck(StairPoints[I], I < TreadPoints ? TEXT("stair tread") : TEXT("ramp"));

    // The entrance aprons' samples stand at their building's pad (ApronTop + its DZ); the ground under an apron must be that
    // pad, or the apron floats over the relief or sinks into it.
    int32 ApronMisses = 0;
    {
        TArray<HouseCampusLayout::FApron> Aprons;
        HouseCampusLayout::EntranceAprons(Aprons);
        for (const FVector& P : Samples)
            for (const HouseCampusLayout::FApron& A : Aprons)
            {
                if (!A.R.ExpandBy(-5.f).IsInside(FVector2D(P.X, P.Y))) continue;
                const float PadZ = HouseCalderTerrain::SurfaceZ(P.X, P.Y, HouseCalderTerrain::EPave::Apron);
                if (FMath::Abs(PadZ - P.Z) >= .1f) Csv(P, TEXT("apron"), FString::Printf(TEXT("ground %.2f pad %.2f"), PadZ, P.Z));
                if (FMath::Abs(PadZ - P.Z) >= .1f && ++ApronMisses <= 10)
                    Note(FString::Printf(TEXT("problem: the ground under the apron at (%.0f,%.0f) is at Z=%.2f, its pad at %.2f"), P.X, P.Y, PadZ, P.Z));
                break;
            }
    }

    // Clear headroom over every walk: a player-sized capsule standing on each sample must touch nothing (a planter, a post, a
    // bench or a trunk on a walk shows up here).
    int32 Blocked = 0;
    for (const FVector& P : Samples)
    {
        // On or beside a Calder stair or ramp (within the capsule's 30 cm and 5 cm more) the capsule meets its own risers and
        // rails; the treads' and slabs' floors are checked above.
        if (OnStairOrRamp(P, 35.f)) continue;
        TArray<FOverlapResult> Hits;
        if (!W->OverlapMultiByChannel(Hits, P + FVector(0, 0, 4.f + 85.f + 30.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(30.f, 85.f + 30.f), Q)) continue;
        bool bReal = false;
        for (const FOverlapResult& O : Hits) bReal |= O.bBlockingHit || (O.GetComponent() && O.GetComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block);
        if (!bReal) continue;
        Csv(P, TEXT("obstruction"), GetNameSafe(Hits[0].GetComponent()));
        if (++Blocked > 25) continue;
        // Where it is: drop a small sphere onto the sample to find the top of whatever is there.
        FHitResult Top;
        const bool bTop = W->SweepSingleByChannel(Top, P + FVector(0, 0, 400.f), P + FVector(0, 0, 2.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(30.f), Q);
        Note(FString::Printf(TEXT("problem: something stands on the walk at (%.0f,%.0f): %s%s"), P.X, P.Y, *GetNameSafe(Hits[0].GetComponent()),
            bTop ? *FString::Printf(TEXT(", top at (%.0f,%.0f,%.0f)"), Top.ImpactPoint.X, Top.ImpactPoint.Y, Top.ImpactPoint.Z) : TEXT("")));
    }
    FFileHelper::SaveStringToFile(ProblemCsv, *(Dir / TEXT("problems.csv")));

    // Trees and utility poles: none in a building or on a traffic lane, and no trunk's collision (its real box, at the
    // instance's own scale and yaw) reaching a building, a lane or a walk.
    int32 Trees = 0, Poles = 0, BadTrees = 0, BadPoles = 0;
    TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Batches(Exterior);
    for (UHierarchicalInstancedStaticMeshComponent* H : Batches)
    {
        const FString Name = H->GetName();
        const bool bTree = Name.StartsWith(TEXT("ExteriorTree")) || Name.StartsWith(TEXT("CampusFallTree"));
        const bool bPole = Name.StartsWith(TEXT("ExteriorPole"));
        if (!bTree && !bPole) continue;
        for (int32 I = 0; I < H->GetInstanceCount(); ++I)
        {
            FTransform T;
            if (!H->GetInstanceTransform(I, T, true) || T.GetLocation().Y > HouseCampusLayout::JunctionSouthY) continue;
            const FVector2D P(T.GetLocation().X, T.GetLocation().Y);
            (bTree ? Trees : Poles)++;
            bool bBad = HouseCampusLayout::OnTrafficLane(P);
            for (const HouseCampusLayout::FBlock& B : HouseCampusLayout::Blocks()) bBad |= B.Rect().IsInside(P);
            FString What = TEXT("a building or a lane");
            if (!bBad && bTree && H->GetStaticMesh() && H->GetStaticMesh()->GetBodySetup())
            {
                // The trunk boxes' real corners under the instance's full transform (CalcAABB would scale them by the
                // smallest axis and undersize a tree squashed in Z).
                const FKAggregateGeom& Agg = H->GetStaticMesh()->GetBodySetup()->AggGeom;
                FBox Col(ForceInit);
                for (const FKBoxElem& E : Agg.BoxElems)
                    for (int32 Corner = 0; Corner < 8; ++Corner)
                    {
                        const FVector Local(((Corner & 1) ? .5f : -.5f) * E.X, ((Corner & 2) ? .5f : -.5f) * E.Y, ((Corner & 4) ? .5f : -.5f) * E.Z);
                        Col += T.TransformPosition(E.Center + E.Rotation.RotateVector(Local));
                    }
                if (!Col.IsValid) Col = Agg.CalcAABB(T);
                if (Col.IsValid) bBad = HouseCampusLayout::FootprintClash(FBox2D(FVector2D(Col.Min), FVector2D(Col.Max)), What);
            }
            if (!bBad) continue;
            (bTree ? BadTrees : BadPoles)++;
            Note(FString::Printf(TEXT("problem: %s instance at (%.0f,%.0f) reaches %s"), *Name, P.X, P.Y, *What));
        }
    }

    // The exterior's actual point and spot lights (spots derive from point lights), as the woods review counts them.
    TInlineComponentArray<UPointLightComponent*> Lights(Exterior);
    int32 ExteriorLights = Lights.Num();
    for (TActorIterator<AActor> It(W); It; ++It)
        if (It->GetOwner() == Exterior) { TInlineComponentArray<UPointLightComponent*> Owned(*It); ExteriorLights += Owned.Num(); }

    const bool bOK = R.Overlaps == 0 && R.EntrancesOff == 0 && R.BlankWalls == 0 && R.LotsUnreached == 0 && R.WalkOnLane == 0 && R.DeadEnds == 0 && R.PropsOnLane == 0
        && R.Components == 1 && FloorMisses == 0 && ApronMisses == 0 && Blocked == 0 && BadTrees == 0 && BadPoles == 0 && ExteriorLights <= HouseFraternityRow::ExteriorLightBudget();
    Finish(bOK, FString::Printf(TEXT("buildings=%d entrances=%d entrancesOffNetwork=%d walksAtBlankWalls=%d overlaps=%d lots=%d lotsWithoutRoad=%d roads=%d deadEnds=%d ")
        TEXT("pedestrianCells=%d networks=%d walkOnLaneCells=%d floorSamples=%d floorMisses=%d walkObstructions=%d campusTrees=%d treesInBuildingsOrLanes=%d ")
        TEXT("campusPoles=%d polesInBuildingsOrLanes=%d lampsInBuildingsOrLanes=%d exteriorLights=%d stairSkipped=%d stairSamples=%d"),
        R.Buildings, R.Entrances, R.EntrancesOff, R.BlankWalls, R.Overlaps, R.Lots, R.LotsUnreached, R.Roads, R.DeadEnds,
        R.PedestrianCells, R.Components, R.WalkOnLane, FloorChecks, FloorMisses, Blocked, Trees, BadTrees, Poles, BadPoles, R.PropsOnLane, ExteriorLights,
        StairSkipped, StairPoints.Num()));
}
#else
void TickHouseCampusLayoutReview(UWorld*) {}
#endif
