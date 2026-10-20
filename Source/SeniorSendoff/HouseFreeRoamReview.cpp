// -FreeRoamWalkReview: walks the player, with real movement input, across three of the lines where the invisible
// property boundary used to stand (front onto the campus green, west along Fraternity Park Dr, east across the side
// street). With the boundary off (HouseExteriorSiteLayout.h) every leg must cross; with -PropertyBoundary every leg must
// be stopped at its line, so the switch is proved both ways. Writes Saved/FreeRoamWalkReview/result.txt and a screenshot
// from the campus green.
// With the Calder terrain and its features on (Calder job, MASTER_PROMPT "Updated reviews"), two more routes follow the campus
// route, walked the same way: the hill route (up the Senior Steps, over College Hill, down into the glen over the P6
// footbridge and through the rock house, round Gander Pond) and the north route (up Calder Dr and the Crow Ledge trail to
// P8). With -NoCalderTerrain they are not walked and the review is exactly today's.
#include "StoryCampaign.h"
#if !UE_BUILD_SHIPPING
#include "HouseExteriorSiteLayout.h"
#include "HouseCampusLayout.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderLandform.h"
#include "HouseCalderTerrain.h"
#include "HouseExterior.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace HouseFreeRoamReviewImpl
{
/** A walk across one old boundary line: Line is the X (bAlongX) or Y coordinate the boundary stood on. */
struct FLeg { const TCHAR* Name; FVector2D From, To; float Line; bool bAlongX; };
const FLeg Legs[] = {
    { TEXT("front: street onto the campus green"), FVector2D(3100.f, -5000.f), FVector2D(3100.f, -7000.f), HouseExteriorSite::PlayableFrontY, false },
    { TEXT("west: along Fraternity Park Dr"), FVector2D(-7800.f, -4850.f), FVector2D(-9400.f, -4850.f), HouseExteriorSite::PlayableWestX, true },
    // Along the front street through the side-street crossing: the Harvest Fall Jam's fences fill the east lawn.
    { TEXT("east: along Fraternity Park Dr into the side street"), FVector2D(8000.f, -4850.f), FVector2D(9600.f, -4850.f), HouseExteriorSite::PlayableEastX, true },
};
constexpr int32 LegCount = UE_ARRAY_COUNT(Legs);

/** The campus route (with the campus plan on and the boundary off): one continuous walk with real input from the house's
 *  front walk across the mid-block crosswalk, through the Calder Dr gateposts, round the stadium apron to the ticket plaza,
 *  back up the gate path, along the promenade, through the quad to the library and back to the clock tower. Shot names a
 *  screenshot taken on arrival. */
struct FWaypoint { FVector2D At; const TCHAR* Name; const TCHAR* Shot; };
const FWaypoint Route[] = {
    { FVector2D(0.f, -3000.f), TEXT("the house's front walk"), nullptr },
    { FVector2D(0.f, -4000.f), TEXT("Fraternity Park Dr's near sidewalk"), nullptr },
    { FVector2D(2400.f, -4000.f), TEXT("the mid-block crosswalk, near side"), nullptr },
    { FVector2D(2400.f, -5650.f), TEXT("across the crosswalk"), TEXT("Route1_Crosswalk.png") },
    { FVector2D(2900.f, -6000.f), TEXT("past the kiosk"), nullptr },
    { FVector2D(2900.f, -6500.f), TEXT("the promenade"), nullptr },
    { FVector2D(8300.f, -6500.f), TEXT("the promenade's east end"), nullptr },
    { FVector2D(8575.f, -6500.f), TEXT("Calder Dr's west sidewalk"), nullptr },
    { FVector2D(8575.f, -5950.f), TEXT("through the entrance gateposts"), TEXT("Route2_Gateposts.png") },
    { FVector2D(8575.f, -13300.f), TEXT("down Calder Dr to the stadium crossing"), nullptr },
    // Round the stadium on the lawn, about 3 m off its wall opposite the arches of segments 2, 4 and 6 (clear of the arch
    // blocks, the homecoming float and the hay at the north gates), then past the ticket booths' backs to the plaza.
    { FVector2D(7851.f, -11731.f), TEXT("across the lawn to the stadium wall"), nullptr },
    { FVector2D(6886.f, -10637.f), TEXT("round the stadium"), nullptr },
    { FVector2D(5483.f, -9804.f), TEXT("round the stadium's north-east side"), nullptr },
    { FVector2D(4300.f, -8800.f), TEXT("behind the ticket booths"), nullptr },
    { FVector2D(2400.f, -8650.f), TEXT("the ticket plaza"), TEXT("Route3_TicketPlaza.png") },
    { FVector2D(2400.f, -6700.f), TEXT("up the gate path"), nullptr },
    { FVector2D(2000.f, -6500.f), TEXT("back on the promenade"), nullptr },
    { FVector2D(-11000.f, -6500.f), TEXT("west along the promenade"), nullptr },
    { FVector2D(-11000.f, -10800.f), TEXT("into the quad"), TEXT("Route4_Quad.png") },
    { FVector2D(-14400.f, -10800.f), TEXT("the library forecourt"), nullptr },
    { FVector2D(-9500.f, -10800.f), TEXT("the clock tower"), TEXT("Route5_ClockTower.png") },
};
constexpr int32 RouteCount = UE_ARRAY_COUNT(Route);
bool RouteOn() { return HouseCampusLayout::Enabled() && !HouseExteriorSite::PropertyBoundaryEnabled(); }

/** The Calder routes: walked like the campus route once it is done, with the Calder terrain and its features on. Designed on the
 *  landform (Saved/CalderWork/plan/reach_design.md): the plan's walks, roads, stairs and the Senior Steps' railed ramp where it
 *  has them; across lawn and banks no steeper than about 1:2.2; the glen's sides (1:1.5-1:1.8 everywhere: the landform has no
 *  path into the glen) only on the way down to the P6 footbridge and back up past the rock house, crossing the caprock band
 *  where its step is nil (X 20500..20950). The glen's structures, the pond loop and the Crow Ledge trail come from the landform's
 *  data (HouseCalderLandform), so the route follows them. */
struct FCalderPoint { float X, Y; const TCHAR* Name; const TCHAR* Shot; };
const FCalderPoint HillCampus[] = {
    { -10700.f, -10800.f, TEXT("the quad (Q1), on the axis"), nullptr },
    { -14400.f, -10800.f, TEXT("up the quad stairs to Q3"), nullptr },
    { -15000.f, -10800.f, TEXT("up the Senior Steps onto Mercer's forecourt"), TEXT("Hill1_SeniorSteps.png") },
    { -14800.f, -11125.f, TEXT("across the forecourt"), nullptr },
    { -14500.f, -11125.f, TEXT("onto the Senior Steps ramp's landing"), nullptr },
    { -14500.f, -13050.f, TEXT("down the railed ramp to Q3"), nullptr },
    { -14400.f, -13600.f, TEXT("the quad's south-west corner"), nullptr },
    { -14400.f, -13850.f, TEXT("onto walk_11"), nullptr },
    { -17400.f, -13850.f, TEXT("west along walk_11 to Calder Hall's north walk"), nullptr },
    { -17750.f, -13700.f, TEXT("off the walk past Calder Hall's corner"), nullptr },
    { -20375.f, -14550.f, TEXT("down the bank onto Founders Way"), nullptr },
    { -20375.f, -19950.f, TEXT("south up Founders Way"), nullptr },
    { -17300.f, -20000.f, TEXT("east along walk_33 to the foot of Alumni's flight"), nullptr },
    { -17000.f, -20100.f, TEXT("past the flight's foot"), nullptr },
    { -15250.f, -21750.f, TEXT("up the lawn east of Alumni Hall"), nullptr },
    { -15000.f, -24250.f, TEXT("over College Hill's crest"), TEXT("Hill2_CollegeHill.png") },
    { -15250.f, -24600.f, TEXT("off the crest"), nullptr },
    { -15150.f, -26000.f, TEXT("down the bank to the south lane"), nullptr },
    { -15000.f, -26025.f, TEXT("the south lane"), nullptr },
    { 12775.f, -26025.f, TEXT("east along the south lane"), nullptr },
    { 12775.f, -25500.f, TEXT("the foot of the plant lane"), nullptr },
    { 14800.f, -24200.f, TEXT("up the bank east of the plant lane"), nullptr },
    { 16300.f, -22400.f, TEXT("onto the east bench"), nullptr },
    { 17900.f, -21200.f, TEXT("to the glen's west rim, north of the cascade band"), nullptr },
};
const FCalderPoint HillClimb[] = {
    { 24650.f, -19950.f, TEXT("off the rock house shelf"), nullptr },
    { 21700.f, -20200.f, TEXT("up the glen's west side"), nullptr },
    { 21250.f, -19850.f, TEXT("along the rim"), nullptr },
    { 20700.f, -19800.f, TEXT("to the caprock band's end"), nullptr },
    { 20700.f, -19400.f, TEXT("over the caprock band where it has no step"), nullptr },
    { 21000.f, -19100.f, TEXT("north of the falls"), nullptr },
    { 22550.f, -16150.f, TEXT("down to the dam's west end"), nullptr },
};
bool CalderOn() { return RouteOn() && HouseCalderTerrain::Enabled() && HouseCalderFeatures::Active(); }
constexpr double CalderBudget = 900.0;   // seconds for both Calder routes, on top of the legs' and the campus route's 480
const TCHAR* const CalderNames[2] = { TEXT("hill route"), TEXT("north route") };

/** Calder route Which (0 the hill route, 1 the north route) as waypoints; the first is where the walk starts. */
void CalderRoute(int32 Which, TArray<FWaypoint>& Out)
{
    namespace LF = HouseCalderLandform;
    Out.Reset();
    if (Which == 1)
    {
        // Up Calder Dr from the house side's north hedge, west along the Crow Ledge trail and its timber flights, onto the ledge
        // to P8.
        const LF::FLandformPath* Trail = nullptr;
        for (const LF::FLandformPath& P : LF::LandformPaths()) if (!P.bClosed && P.Points.Num() >= 2) Trail = &P;
        const FVector2D Foot = Trail ? Trail->Points[0] : FVector2D(8370.f, 26850.f), Top = Trail ? Trail->Points.Last() : FVector2D(2100.f, 26850.f);
        FVector2D P8(1500.f, 26550.f);
        for (const LF::FSpot& E : LF::EyeSpots()) if (E.Name.StartsWith(TEXT("P8"))) P8 = FVector2D(E.At);
        Out.Add({ FVector2D(9200.f, 19000.f), TEXT("Calder Dr at the north hedge"), nullptr });
        Out.Add({ FVector2D(9200.f, Foot.Y), TEXT("north up Calder Dr"), nullptr });
        Out.Add({ Foot, TEXT("the Crow Ledge trail's foot"), nullptr });
        Out.Add({ Top, TEXT("up the trail's timber flights"), nullptr });
        Out.Add({ FVector2D(P8.X, Top.Y), TEXT("onto Crow Ledge"), nullptr });
        Out.Add({ P8, TEXT("P8: Crow Ledge over the house roofs"), TEXT("North1_CrowLedge.png") });
        return;
    }
    for (const FCalderPoint& P : HillCampus) Out.Add({ FVector2D(P.X, P.Y), P.Name, P.Shot });
    // Into the glen: down its side in line with the timber footbridge, over it (P6) to its east end and back (its east end lands
    // against the caprock band, so the rock house is reached from the west), off its west end along its axis (clear of its
    // trusses), then the rock house path behind the falls to its east end and back.
    const LF::FWaterCrossing* Bridge = nullptr;
    const LF::FWaterCrossing* Arch = nullptr;
    for (const LF::FWaterCrossing& X : LF::WaterCrossings())
    {
        if (X.Kind == LF::EWaterCrossing::TimberFootbridge) Bridge = &X;
        if (X.Kind == LF::EWaterCrossing::ArchFootbridge) Arch = &X;
    }
    if (Bridge)
    {
        const FVector2D West = Bridge->Centre - Bridge->Axis * Bridge->Back, East = Bridge->Centre + Bridge->Axis * Bridge->On;
        Out.Add({ West - Bridge->Axis * 300.f, TEXT("down the glen's side, in line with the footbridge"), nullptr });
        Out.Add({ West, TEXT("onto the timber footbridge"), nullptr });
        Out.Add({ Bridge->Centre, TEXT("P6: the footbridge over the plunge pool"), TEXT("Hill3_P6Footbridge.png") });
        Out.Add({ East, TEXT("the footbridge's east end"), nullptr });
        Out.Add({ Bridge->Centre, TEXT("back over the footbridge"), nullptr });
        Out.Add({ West, TEXT("the footbridge's west end"), nullptr });
        Out.Add({ West - Bridge->Axis * 150.f, TEXT("off the footbridge"), nullptr });
    }
    const LF::FRockHouse& Rock = LF::RockHouse();
    Out.Add({ Rock.PathA, TEXT("into the rock house"), nullptr });
    Out.Add({ (Rock.PathA + Rock.PathB) * .5f, TEXT("behind Calder Falls"), TEXT("Hill4_RockHouse.png") });
    Out.Add({ Rock.PathB, TEXT("the rock house path's east end"), nullptr });
    Out.Add({ Rock.PathA, TEXT("back to the rock house's west end"), nullptr });
    for (const FCalderPoint& P : HillClimb) Out.Add({ FVector2D(P.X, P.Y), P.Name, P.Shot });
    // Round Gander Pond on its walking loop, from the vertex before the dam's crest walk back to it (over P7's arch).
    const LF::FLandformPath* Loop = nullptr;
    for (const LF::FLandformPath& P : LF::LandformPaths()) if (P.bClosed && P.Points.Num() >= 3) Loop = &P;
    if (!Loop) return;
    const int32 Count = Loop->Points.Num();
    const int32 Unique = FVector2D::Distance(Loop->Points[0], Loop->Points[Count - 1]) < 1.f ? Count - 1 : Count;
    int32 First = INDEX_NONE;
    for (int32 K = 0; K < Unique && First == INDEX_NONE; ++K)
        if (Loop->Stretches.IsValidIndex(K) && Loop->Stretches[K] == LF::EPathStretch::DamCrest) First = (K + Unique - 1) % Unique;
    if (First == INDEX_NONE)
    {
        float Near = 1e30f;
        for (int32 K = 0; K < Unique; ++K)
        {
            const float D = float(FVector2D::DistSquared(Loop->Points[K], Out.Last().At));
            if (D < Near) { Near = D; First = K; }
        }
    }
    for (int32 I = 0; I <= Unique; ++I)
    {
        const int32 K = (First + I) % Unique;
        Out.Add({ Loop->Points[K], I == 0 ? TEXT("onto Gander Pond's walking loop") : TEXT("round Gander Pond"), nullptr });
        if (I < Unique && Arch && Loop->Stretches.IsValidIndex(K) && Loop->Stretches[K] == LF::EPathStretch::ArchFootbridge)
            Out.Add({ Arch->Centre, TEXT("P7: the stone-arch footbridge over the dam's notch"), TEXT("Hill5_P7Arch.png") });
    }
}

struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Walker;
    int32 Phase = 0, Leg = 0, Crossed = 0, Stopped = 0, Other = 0;   // Stopped: by a PropertyBoundary_* box at its line
    int32 Waypoint = 0, Reached = 0;                                  // the campus route (Phase 4)
    float Best = 1e9f, StartZ = 0.f;
    double Progress = 0.0;
    double Start = 0.0, Step = 0.0;
    FVector Goal = FVector::ZeroVector;
    float Furthest = -1e9f;
    FString Log, Dir;
    // the Calder routes (Phase 6 walking, Phase 7 between them): which, its waypoints, how far each got
    int32 Calder = -1;
    TArray<FWaypoint> CalderPoints;
    int32 CalderReached[2] = { 0, 0 }, CalderCount[2] = { 0, 0 };
    double CalderBegan = 0.0;
};
FState& State() { static FState S; return S; }

/** How far past its line the walker is (negative while still inside the old boundary). */
float PastLine(const FLeg& L, const FVector& Feet)
{
    const float Sign = L.bAlongX ? FMath::Sign(L.To.X - L.From.X) : FMath::Sign(L.To.Y - L.From.Y);
    return float(((L.bAlongX ? Feet.X : Feet.Y) - L.Line) * Sign);
}
}

void TickHouseFreeRoamReview(UWorld* W)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("FreeRoamWalkReview")) || !W || UGameplayStatics::GetCurrentLevelName(W, true) != TEXT("Chapter01_House")) return;
    APlayerController* PC = W->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = PC ? Cast<AStoryFirstPersonCharacter>(PC->GetPawn()) : nullptr;
    if (!Pawn) return;
    HouseFreeRoamReviewImpl::FState& S = HouseFreeRoamReviewImpl::State();
    static const FDelegateHandle Input = FCoreDelegates::OnBeginFrame.AddLambda([]()
    {
        HouseFreeRoamReviewImpl::FState& Live = HouseFreeRoamReviewImpl::State();
        if ((Live.Phase == 2 || Live.Phase == 4 || Live.Phase == 6) && Live.Walker.IsValid()) Live.Walker->AddMovementInput((Live.Goal - Live.Walker->GetActorLocation()).GetSafeNormal2D(), 1.f);
    });
    const bool bBoundary = HouseExteriorSite::PropertyBoundaryEnabled();
    if (S.World.Get() != W)
    {
        S = HouseFreeRoamReviewImpl::FState();
        S.World = W;
        S.Start = S.Step = W->GetTimeSeconds();
        S.Dir = FPaths::ProjectSavedDir() / TEXT("FreeRoamWalkReview");
        IFileManager::Get().MakeDirectory(*S.Dir, true);
        IFileManager::Get().Delete(*(S.Dir / TEXT("result.txt")), false, true, true);   // no stale verdict
    }
    S.Walker = Pawn;
    auto Note = [&S](const FString& T) { S.Log += T + TEXT("\n"); UE_LOG(LogTemp, Display, TEXT("FREE_ROAM_REVIEW %s"), *T); };
    auto Finish = [&](bool bOK, const FString& T)
    {
        Note((bOK ? TEXT("PASS ") : TEXT("FAIL ")) + T);
        FFileHelper::SaveStringToFile(S.Log, *(S.Dir / TEXT("result.txt")));
        S.Phase = -1;
        FPlatformMisc::RequestExitWithStatus(false, bOK ? 0 : 1);
    };
    if (S.Phase < 0) return;
    const double Now = W->GetTimeSeconds();
    const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Feet = Pawn->GetActorLocation() - FVector(0, 0, Half);
    if (Now - S.Start > (HouseFreeRoamReviewImpl::RouteOn() ? 480.0 : 150.0) + (S.CalderBegan > 0.0 ? HouseFreeRoamReviewImpl::CalderBudget : 0.0)) { Finish(false, TEXT("Free-roam walk timed out")); return; }
    auto StartLeg = [&](int32 Index)
    {
        const HouseFreeRoamReviewImpl::FLeg& L = HouseFreeRoamReviewImpl::Legs[Index];
        FCollisionQueryParams Q(SCENE_QUERY_STAT(FreeRoamGround), true, Pawn);
        FHitResult Ground;
        const float GroundAt = HouseExteriorSite::GroundHeight(L.From.X, L.From.Y);
        const bool bGround = W->LineTraceSingleByChannel(Ground, FVector(L.From, GroundAt + 2060.f), FVector(L.From, GroundAt - 2940.f), ECC_Pawn, Q);
        const float Z = (bGround ? float(Ground.ImpactPoint.Z) : GroundAt) + Half + 3.f;
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->SetActorLocation(FVector(L.From, Z), false, nullptr, ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        S.Leg = Index;
        S.Goal = FVector(L.To, Z);
        S.Furthest = -1e9f;
        S.Step = Now;
        S.Phase = 2;
        Note(FString::Printf(TEXT("leg %d %s: from (%.0f,%.0f) over the old line at %s=%.0f to (%.0f,%.0f), ground %s"), Index + 1, L.Name, L.From.X, L.From.Y,
            L.bAlongX ? TEXT("X") : TEXT("Y"), L.Line, L.To.X, L.To.Y, bGround ? *GetNameSafe(Ground.GetComponent()) : TEXT("none")));
    };
    auto EndLeg = [&](bool bCrossed, const FString& Why, bool bByBoundary)
    {
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        // A stop only counts as the boundary's when a PropertyBoundary_* box held the walker right at its line (the box is
        // 60 thick, the capsule's radius about 32); a fall or any other blocker is a failure in either mode.
        const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const bool bAtLine = S.Furthest <= 5.f && S.Furthest >= -(Radius + HouseExteriorSite::BoundaryHalfThickness + 40.f);
        const TCHAR* Outcome = bCrossed ? TEXT("CROSSED") : bByBoundary && bAtLine ? TEXT("STOPPED") : TEXT("FAILED");
        (bCrossed ? S.Crossed : bByBoundary && bAtLine ? S.Stopped : S.Other)++;
        Note(FString::Printf(TEXT("leg %d %s %s; furthest %.0f cm past the old line, feet at %s"), S.Leg + 1, Outcome, *Why, S.Furthest, *Feet.ToString()));
        S.Step = Now;
        S.Phase = 3;
        if (S.Leg == 0 && bCrossed) FScreenshotRequest::RequestScreenshot(S.Dir / TEXT("OnTheCampusGreen.png"), false, false);
    };
    if (S.Phase == 0)
    {
        // Let the exterior build and the pawn settle, then begin.
        bool bExterior = false;
        for (TActorIterator<AHouseExteriorDetail> It(W); It; ++It) bExterior = true;
        if (Now - S.Start < 8.0 || !bExterior || !HouseCalderTerrain::CollisionReady(W)) return;
        Note(FString::Printf(TEXT("boundary=%s (PropertyBoundary_* boxes %s)"), bBoundary ? TEXT("on") : TEXT("off"), bBoundary ? TEXT("built") : TEXT("not built")));
        StartLeg(0);
        return;
    }
    if (S.Phase == 2)
    {
        const HouseFreeRoamReviewImpl::FLeg& L = HouseFreeRoamReviewImpl::Legs[S.Leg];
        PC->SetControlRotation((S.Goal - Feet).GetSafeNormal2D().Rotation());
        S.Furthest = FMath::Max(S.Furthest, HouseFreeRoamReviewImpl::PastLine(L, Feet));
        // A fall: the feet more than 4 m under the start's feet, that height carried along over the ground (a climb is no fall).
        if (Feet.Z < S.Goal.Z - Half - 400.f + (HouseExteriorSite::GroundHeight(Feet.X, Feet.Y) - HouseExteriorSite::GroundHeight(L.From.X, L.From.Y))) { EndLeg(false, TEXT("fell"), false); return; }
        if (FVector::Dist2D(Feet, S.Goal) < 60.f) { EndLeg(true, TEXT("reached the far side"), false); return; }
        if (Now - S.Step > 14.0)
        {
            // Stalled: name whatever is in front of the walker.
            FCollisionQueryParams Q(SCENE_QUERY_STAT(FreeRoamBlocker), false, Pawn);
            FHitResult Hit;
            const FVector Dir = (S.Goal - Feet).GetSafeNormal2D();
            const FVector Centre = Pawn->GetActorLocation();
            const bool bHit = W->SweepSingleByChannel(Hit, Centre, Centre + Dir * 120.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(), Half - 5.f), Q);
            EndLeg(false, FString::Printf(TEXT("stalled by %s/%s"), bHit ? *GetNameSafe(Hit.GetActor()) : TEXT("nothing"), bHit ? *GetNameSafe(Hit.GetComponent()) : TEXT("nothing")),
                bHit && Hit.GetComponent() && Hit.GetComponent()->GetName().StartsWith(TEXT("PropertyBoundary_")));
        }
        return;
    }
    auto Summary = [&]()
    {
        const bool bLegs = bBoundary ? S.Stopped == HouseFreeRoamReviewImpl::LegCount : S.Crossed == HouseFreeRoamReviewImpl::LegCount;
        const bool bRoute = !HouseFreeRoamReviewImpl::RouteOn() || S.Reached == HouseFreeRoamReviewImpl::RouteCount - 1;
        // The Calder routes count only when they are walked (Calder terrain on); with it off the line is today's.
        const bool bCalder = !HouseFreeRoamReviewImpl::CalderOn()
            || (S.CalderCount[0] > 1 && S.CalderReached[0] == S.CalderCount[0] - 1 && S.CalderCount[1] > 1 && S.CalderReached[1] == S.CalderCount[1] - 1);
        const FString CalderPart = HouseFreeRoamReviewImpl::CalderOn() ? FString::Printf(TEXT(" hillRoute=%d/%d northRoute=%d/%d"), S.CalderReached[0], FMath::Max(0, S.CalderCount[0] - 1),
            S.CalderReached[1], FMath::Max(0, S.CalderCount[1] - 1)) : FString();
        Finish(bLegs && bRoute && bCalder, FString::Printf(TEXT("boundary=%s crossed=%d/%d stoppedAtLine=%d/%d failed=%d campusRoute=%s%s (%s)"), bBoundary ? TEXT("on") : TEXT("off"), S.Crossed, HouseFreeRoamReviewImpl::LegCount,
            S.Stopped, HouseFreeRoamReviewImpl::LegCount, S.Other,
            HouseFreeRoamReviewImpl::RouteOn() ? *FString::Printf(TEXT("%d/%d"), S.Reached, HouseFreeRoamReviewImpl::RouteCount - 1) : TEXT("off"), *CalderPart,
            bBoundary ? TEXT("a PropertyBoundary box must stop the walker at every old line") : TEXT("nothing invisible may stop the walker")));
    };
    // A Calder route starts where its first waypoint is (teleported there, as the campus route starts on the front walk).
    auto StartCalder = [&](int32 Which)
    {
        TArray<HouseFreeRoamReviewImpl::FWaypoint> Points;
        HouseFreeRoamReviewImpl::CalderRoute(Which, Points);
        S.Calder = Which;
        S.CalderPoints = Points;
        S.CalderCount[Which] = Points.Num();
        S.CalderReached[Which] = 0;
        if (S.CalderBegan <= 0.0) S.CalderBegan = Now;
        if (Points.Num() < 2) { Note(FString::Printf(TEXT("%s: no waypoints"), HouseFreeRoamReviewImpl::CalderNames[Which])); S.Phase = 7; S.Step = Now; return; }
        const FVector2D From = Points[0].At;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(FreeRoamCalderGround), true, Pawn);
        FHitResult Ground;
        const float GroundAt = HouseExteriorSite::GroundHeight(From.X, From.Y);
        const bool bGround = W->LineTraceSingleByChannel(Ground, FVector(From, GroundAt + 2060.f), FVector(From, GroundAt - 2940.f), ECC_Pawn, Q);
        const float Z = (bGround ? float(Ground.ImpactPoint.Z) : GroundAt) + Half + 3.f;
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->SetActorLocation(FVector(From, Z), false, nullptr, ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        S.StartZ = Z - Half;
        S.Waypoint = 1;
        S.Goal = FVector(Points[1].At, Z);
        S.Best = 1e9f;
        S.Progress = Now;
        S.Phase = 6;
        Note(FString::Printf(TEXT("%s: %d waypoints from %s (ground %s)"), HouseFreeRoamReviewImpl::CalderNames[Which], Points.Num(), Points[0].Name, bGround ? *GetNameSafe(Ground.GetComponent()) : TEXT("none")));
    };
    // After the campus route (done or failed): the Calder routes when they are on, then the verdict.
    auto AfterCampus = [&]() { if (HouseFreeRoamReviewImpl::CalderOn()) StartCalder(0); else Summary(); };
    if (S.Phase == 3 && Now - S.Step > 1.5)
    {
        if (S.Leg + 1 < HouseFreeRoamReviewImpl::LegCount) { StartLeg(S.Leg + 1); return; }
        if (!HouseFreeRoamReviewImpl::RouteOn()) { Summary(); return; }
        // The campus route starts on the house's front walk.
        const FVector2D From = HouseFreeRoamReviewImpl::Route[0].At;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(FreeRoamRouteGround), true, Pawn);
        FHitResult Ground;
        const float GroundAt = HouseExteriorSite::GroundHeight(From.X, From.Y);
        const bool bGround = W->LineTraceSingleByChannel(Ground, FVector(From, GroundAt + 2060.f), FVector(From, GroundAt - 2940.f), ECC_Pawn, Q);
        const float Z = (bGround ? float(Ground.ImpactPoint.Z) : GroundAt) + Half + 3.f;
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->SetActorLocation(FVector(From, Z), false, nullptr, ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        S.StartZ = Z - Half;
        S.Waypoint = 1;
        S.Goal = FVector(HouseFreeRoamReviewImpl::Route[1].At, Z);
        S.Best = 1e9f;
        S.Progress = Now;
        S.Phase = 4;
        Note(FString::Printf(TEXT("campus route: %d waypoints from %s (ground %s)"), HouseFreeRoamReviewImpl::RouteCount, HouseFreeRoamReviewImpl::Route[0].Name, bGround ? *GetNameSafe(Ground.GetComponent()) : TEXT("none")));
        return;
    }
    if (S.Phase == 4)
    {
        const HouseFreeRoamReviewImpl::FWaypoint& P = HouseFreeRoamReviewImpl::Route[S.Waypoint];
        PC->SetControlRotation((S.Goal - Feet).GetSafeNormal2D().Rotation());
        // A fall: the feet more than 3 m under the start walk's height carried over the ground (a climb is not a fall).
        if (Feet.Z < S.StartZ - 300.f + (HouseExteriorSite::GroundHeight(Feet.X, Feet.Y) - HouseExteriorSite::GroundHeight(HouseFreeRoamReviewImpl::Route[0].At.X, HouseFreeRoamReviewImpl::Route[0].At.Y))) { Note(FString::Printf(TEXT("campus route FELL on the way to %s, feet at %s"), P.Name, *Feet.ToString())); AfterCampus(); return; }
        const float Dist = float(FVector::Dist2D(Feet, S.Goal));
        if (Dist < S.Best - 30.f) { S.Best = Dist; S.Progress = Now; }
        if (Dist < 70.f)
        {
            ++S.Reached;
            Note(FString::Printf(TEXT("campus route reached %d/%d %s, feet at (%.0f,%.0f,%.1f)"), S.Reached, HouseFreeRoamReviewImpl::RouteCount - 1, P.Name, Feet.X, Feet.Y, Feet.Z));
            if (P.Shot) FScreenshotRequest::RequestScreenshot(S.Dir / P.Shot, false, false);
            if (S.Waypoint + 1 >= HouseFreeRoamReviewImpl::RouteCount) { S.Phase = 5; S.Step = Now; return; }
            ++S.Waypoint;
            S.Goal = FVector(HouseFreeRoamReviewImpl::Route[S.Waypoint].At, S.Goal.Z);
            S.Best = 1e9f;
            S.Progress = Now;
            return;
        }
        if (Now - S.Progress > 6.0)
        {
            FCollisionQueryParams Q(SCENE_QUERY_STAT(FreeRoamRouteBlocker), false, Pawn);
            FHitResult Hit;
            const FVector Dir = (S.Goal - Feet).GetSafeNormal2D();
            const FVector Centre = Pawn->GetActorLocation();
            const bool bHit = W->SweepSingleByChannel(Hit, Centre, Centre + Dir * 120.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(), Half - 5.f), Q);
            Note(FString::Printf(TEXT("campus route STALLED on the way to %s at (%.0f,%.0f,%.1f), %.0f cm short, blocked by %s/%s"), P.Name, Feet.X, Feet.Y, Feet.Z, Dist,
                bHit ? *GetNameSafe(Hit.GetActor()) : TEXT("nothing"), bHit ? *GetNameSafe(Hit.GetComponent()) : TEXT("nothing")));
            AfterCampus();
        }
        return;
    }
    if (S.Phase == 5 && Now - S.Step > 1.5) { AfterCampus(); return; }
    if (S.Phase == 6)
    {
        // A Calder route, walked as the campus route is: a fall is the feet 3 m under the route's start carried over the ground,
        // a stall 6 s without 30 cm of progress; arrival within 70 cm.
        const int32 Which = S.Calder;
        const TCHAR* RouteName = HouseFreeRoamReviewImpl::CalderNames[Which];
        const HouseFreeRoamReviewImpl::FWaypoint& P = S.CalderPoints[S.Waypoint];
        PC->SetControlRotation((S.Goal - Feet).GetSafeNormal2D().Rotation());
        auto Next = [&]() { if (Which == 0) { S.Phase = 7; S.Step = Now; } else { S.Phase = 8; S.Step = Now; } };
        const FVector2D From = S.CalderPoints[0].At;
        if (Feet.Z < S.StartZ - 300.f + (HouseExteriorSite::GroundHeight(Feet.X, Feet.Y) - HouseExteriorSite::GroundHeight(From.X, From.Y)))
        {
            Note(FString::Printf(TEXT("%s FELL on the way to %d %s, feet at %s"), RouteName, S.Waypoint, P.Name, *Feet.ToString()));
            Next();
            return;
        }
        const float Dist = float(FVector::Dist2D(Feet, S.Goal));
        if (Dist < S.Best - 30.f) { S.Best = Dist; S.Progress = Now; }
        if (Dist < 70.f)
        {
            ++S.CalderReached[Which];
            Note(FString::Printf(TEXT("%s reached %d/%d %s, feet at (%.0f,%.0f,%.1f)"), RouteName, S.CalderReached[Which], S.CalderCount[Which] - 1, P.Name, Feet.X, Feet.Y, Feet.Z));
            if (P.Shot) FScreenshotRequest::RequestScreenshot(S.Dir / P.Shot, false, false);
            if (S.Waypoint + 1 >= S.CalderPoints.Num()) { Next(); return; }
            ++S.Waypoint;
            S.Goal = FVector(S.CalderPoints[S.Waypoint].At, S.Goal.Z);
            S.Best = 1e9f;
            S.Progress = Now;
            return;
        }
        if (Now - S.Progress > 6.0)
        {
            FCollisionQueryParams Q(SCENE_QUERY_STAT(FreeRoamCalderBlocker), false, Pawn);
            FHitResult Hit;
            const FVector Dir = (S.Goal - Feet).GetSafeNormal2D();
            const FVector Centre = Pawn->GetActorLocation();
            const bool bHit = W->SweepSingleByChannel(Hit, Centre, Centre + Dir * 120.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(), Half - 5.f), Q);
            Note(FString::Printf(TEXT("%s STALLED on the way to %d %s (%.0f,%.0f) at (%.0f,%.0f,%.1f), %.0f cm short, blocked by %s/%s"), RouteName, S.Waypoint, P.Name, P.At.X, P.At.Y, Feet.X, Feet.Y, Feet.Z, Dist,
                bHit ? *GetNameSafe(Hit.GetActor()) : TEXT("nothing"), bHit ? *GetNameSafe(Hit.GetComponent()) : TEXT("nothing")));
            Next();
        }
        return;
    }
    if (S.Phase == 7 && Now - S.Step > 1.5) { StartCalder(1); return; }
    if (S.Phase == 8 && Now - S.Step > 1.5) Summary();
}

/** A Calder route's waypoints (0 the hill route, 1 the north route) for the two-process review, which walks the client along the
 *  hill route; empty with the Calder terrain or its features off. */
void HouseFreeRoamCalderRoute(int32 Which, TArray<FVector2D>& Out)
{
    Out.Reset();
    if (!HouseCalderTerrain::Enabled() || !HouseCalderFeatures::Active()) return;
    TArray<HouseFreeRoamReviewImpl::FWaypoint> Points;
    HouseFreeRoamReviewImpl::CalderRoute(Which, Points);
    for (const HouseFreeRoamReviewImpl::FWaypoint& P : Points) Out.Add(P.At);
}
#else
void TickHouseFreeRoamReview(UWorld*) {}
void HouseFreeRoamCalderRoute(int32, TArray<FVector2D>& Out) { Out.Reset(); }
#endif
