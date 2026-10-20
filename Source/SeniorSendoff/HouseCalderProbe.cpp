// -CalderProbe="X,Y;X,Y;..." (Calder terrain job, diagnosis only): once the terrain's collision is ready, logs what stands at
// each point the way the reviews see it, then ends the run (unless another self-exiting review shares it):
//  - CALDER_PROBE at=(X,Y) ground= base= surface=: HouseExteriorSite::GroundHeight, BaseGrade, HouseCalderTerrain::SurfaceZ;
//  - CALDER_PROBE_HIT k z= n= comp= walkable= start=: the column traced top down on the Pawn channel (simple collision, as
//    -CalderReachReview scans it: 25 m over the ground down to -1100, the next trace 30 cm under each hit, stepping down out of
//    solids);
//  - CALDER_PROBE_FIT z= clear= over=: at every hit, the player's standing capsule (the pawn's radius less 2 cm, from its step
//    height to 182 cm) and every blocking part it overlaps;
//  - CALDER_PROBE_UP z= hit= comp= n=: a trace up from 5 cm over every hit to 25 m over the ground (what roofs it);
//  - CALDER_PROBE_TILE z=: the terrain tile collision alone (traced complex, as -CalderTerrainReview's TileZ).
// Changes nothing in the world.
#if !UE_BUILD_SHIPPING
#include "HouseCalderTerrain.h"
#include "HouseExterior.h"
#include "HouseExteriorSiteLayout.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace HouseCalderProbeImpl
{
struct FProbeState { TWeakObjectPtr<UWorld> World; double Start = 0.0; bool bDone = false; };
FProbeState& ProbeState() { static FProbeState S; return S; }

bool ProbeSharedRun()
{
    static const TCHAR* const SelfExiting[] = {
        TEXT("FreeRoamWalkReview"), TEXT("ParkingLotWalkReview"), TEXT("WoodsWalkReview"), TEXT("ClockTowerReview"), TEXT("HouseExteriorViews"),
        TEXT("CampusInteriorsReview"), TEXT("CampusInteriorsSmoke"), TEXT("HouseExteriorWalkReview"), TEXT("HouseWalkPerf"), TEXT("CampusDrumsRecord"),
        TEXT("WestFieldWalkReview"), TEXT("CampusLayoutReview"), TEXT("CalderTerrainReview"), TEXT("CalderReachReview") };
    TArray<FString> Tokens, Switches;
    FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);
    for (FString Sw : Switches)
    {
        int32 Eq = INDEX_NONE;
        if (Sw.FindChar(TEXT('='), Eq)) Sw.LeftInline(Eq);
        for (const TCHAR* Name : SelfExiting) if (Sw.Equals(Name, ESearchCase::CaseSensitive)) return true;
    }
    return false;
}

FString PartName(const UPrimitiveComponent* C)
{
    if (!C) return TEXT("none");
    const FWalkableSlopeOverride& O = C->GetWalkableSlopeOverride();
    return FString::Printf(TEXT("%s/%s%s"), C->GetOwner() ? *C->GetOwner()->GetName() : TEXT("-"), *C->GetName(),
        O.GetWalkableSlopeBehavior() == WalkableSlope_Unwalkable ? TEXT("[unwalkable]") : TEXT(""));
}

void ProbePoint(UWorld* W, const FVector2D& P, UCharacterMovementComponent* Move, float Radius, float StepUp, const FCollisionQueryParams& Query, const TArray<UPrimitiveComponent*>& Tiles)
{
    const float G = HouseExteriorSite::GroundHeight(float(P.X), float(P.Y));
    UE_LOG(LogTemp, Display, TEXT("CALDER_PROBE at=(%.1f,%.1f) ground=%.2f base=%.2f surface=%.2f frozen=%d"), P.X, P.Y, G,
        HouseExteriorSite::BaseGrade(float(P.X), float(P.Y)), HouseCalderTerrain::SurfaceZ(float(P.X), float(P.Y)), int32(HouseCalderTerrain::InFrozen(float(P.X), float(P.Y))));
    float From = FMath::Max(G + 2500.f, 7500.f), Step = 40.f;
    TArray<float> Floors;
    for (int32 T = 0; T < 24 && From > -1100.f; ++T)
    {
        FHitResult Hit;
        if (!W->LineTraceSingleByChannel(Hit, FVector(P, From), FVector(P, -1100.0), ECC_Pawn, Query)) break;
        const bool bStart = Hit.bStartPenetrating || Hit.Distance < .5f;
        const bool bWalk = Move ? Move->IsWalkable(Hit) : Hit.ImpactNormal.Z >= .71f;
        UE_LOG(LogTemp, Display, TEXT("CALDER_PROBE_HIT %d z=%.2f n=(%.2f,%.2f,%.2f) comp=%s walkable=%d start=%d"), T, Hit.ImpactPoint.Z,
            Hit.ImpactNormal.X, Hit.ImpactNormal.Y, Hit.ImpactNormal.Z, *PartName(Hit.GetComponent()), int32(bWalk), int32(bStart));
        if (bStart) { From -= Step; Step *= 2.f; continue; }
        Step = 40.f;
        if (Hit.ImpactNormal.Z > 0.f) Floors.Add(float(Hit.ImpactPoint.Z));
        From = float(Hit.ImpactPoint.Z) - 30.f;
    }
    const float HalfH = (180.f + 2.f - StepUp) * .5f;
    for (const float Z : Floors)
    {
        TArray<FOverlapResult> Over;
        W->OverlapMultiByChannel(Over, FVector(P, Z + StepUp + HalfH), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, FMath::Max(HalfH, Radius)), Query);
        FString List;
        for (const FOverlapResult& O : Over) if (O.bBlockingHit) List += (List.IsEmpty() ? TEXT("") : TEXT(",")) + PartName(O.GetComponent());
        UE_LOG(LogTemp, Display, TEXT("CALDER_PROBE_FIT z=%.2f clear=%d over=%s"), Z, int32(List.IsEmpty()), List.IsEmpty() ? TEXT("none") : *List);
        if (!List.IsEmpty())
        {
            // Where it is blocked: spheres of the capsule's radius stacked every 10 cm from the step height up, and, at each, a
            // horizontal ray out to 40 cm along the four axes to the nearest blocking surface.
            FString Stack;
            for (float H = StepUp; H <= 182.f; H += 10.f)
            {
                TArray<FOverlapResult> Ov;
                if (!W->OverlapMultiByChannel(Ov, FVector(P, Z + H + Radius * 0.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Query)) continue;
                FString Dirs;
                for (const FVector2D D : { FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(0, -1) })
                {
                    FHitResult R;
                    if (W->LineTraceSingleByChannel(R, FVector(P, Z + H), FVector(P + D * 40.0, Z + H), ECC_Pawn, Query))
                        Dirs += FString::Printf(TEXT("%s%+.0f,%+.0f:%.0f"), Dirs.IsEmpty() ? TEXT("") : TEXT("|"), D.X, D.Y, R.Distance);
                }
                Stack += FString::Printf(TEXT(" h%.0f[%s]"), H, Dirs.IsEmpty() ? TEXT("-") : *Dirs);
            }
            UE_LOG(LogTemp, Display, TEXT("CALDER_PROBE_STACK z=%.2f%s"), Z, *Stack);
        }
        FHitResult Up;
        FCollisionQueryParams UpQ = Query;
        UpQ.bTraceComplex = true;
        const bool bUp = W->LineTraceSingleByChannel(Up, FVector(P, Z + 5.0), FVector(P, G + 2500.0), ECC_Pawn, UpQ);
        UE_LOG(LogTemp, Display, TEXT("CALDER_PROBE_UP z=%.2f hit=%d at=%.2f comp=%s n=(%.2f,%.2f,%.2f)"), Z, int32(bUp), bUp ? Up.ImpactPoint.Z : 0.0,
            bUp ? *PartName(Up.GetComponent()) : TEXT("none"), Up.ImpactNormal.X, Up.ImpactNormal.Y, Up.ImpactNormal.Z);
    }
    for (UPrimitiveComponent* Tile : Tiles)
    {
        FHitResult Hit;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderProbeTile), true);
        if (Tile->LineTraceComponent(Hit, FVector(P, G + 300.0), FVector(P, G - 300.0), Q))
            UE_LOG(LogTemp, Display, TEXT("CALDER_PROBE_TILE z=%.3f comp=%s n=(%.3f,%.3f,%.3f) dGround=%.3f"), Hit.ImpactPoint.Z, *Tile->GetName(), Hit.ImpactNormal.X, Hit.ImpactNormal.Y, Hit.ImpactNormal.Z, Hit.ImpactPoint.Z - G);
    }
}

void ProbeTick(UWorld* W, ELevelTick, float)
{
    FString Spec;
    if (!W || !W->IsGameWorld() || !FParse::Value(FCommandLine::Get(), TEXT("CalderProbe="), Spec, false)) return;
    if (UGameplayStatics::GetCurrentLevelName(W, true) != TEXT("Chapter01_House")) return;
    FProbeState& S = ProbeState();
    if (S.World.Get() != W) { S = FProbeState(); S.World = W; S.Start = W->GetTimeSeconds(); }
    if (S.bDone) return;
    AHouseExteriorDetail* Exterior = nullptr;
    for (TActorIterator<AHouseExteriorDetail> It(W); It; ++It) Exterior = *It;
    APlayerController* PC = W->GetFirstPlayerController();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Exterior || !Pawn || W->GetTimeSeconds() - S.Start < 8.0 || !HouseCalderTerrain::CollisionReady(W)) return;
    S.bDone = true;
    UCharacterMovementComponent* Move = Pawn->FindComponentByClass<UCharacterMovementComponent>();
    const UCapsuleComponent* Cap = Pawn->FindComponentByClass<UCapsuleComponent>();
    const float Radius = (Cap ? Cap->GetScaledCapsuleRadius() : 30.f) - 2.f, StepUp = Move ? Move->MaxStepHeight : 45.f;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CalderProbe), false);
    for (TActorIterator<APawn> It(W); It; ++It) Query.AddIgnoredActor(*It);
    TArray<UPrimitiveComponent*> Parts, Tiles;
    Exterior->GetComponents(Parts);
    for (UPrimitiveComponent* C : Parts) if (C->GetName().StartsWith(TEXT("CalderTerrainCollision_"))) Tiles.Add(C);
    Spec.ReplaceInline(TEXT("\""), TEXT(""));
    TArray<FString> Points;
    Spec.ParseIntoArray(Points, TEXT(";"));
    for (const FString& Pt : Points)
    {
        FString XS, YS;
        if (!Pt.Split(TEXT(","), &XS, &YS)) continue;
        ProbePoint(W, FVector2D(FCString::Atod(*XS), FCString::Atod(*YS)), Move, Radius, StepUp, Query, Tiles);
    }
    // -CalderProbeLine="X0,Y0,X1,Y1,Step;...": one compact line a point along each segment: the ground, then every hit down the
    // column (z, component, walkable) as the reach review scans it.
    FString Lines;
    if (FParse::Value(FCommandLine::Get(), TEXT("CalderProbeLine="), Lines, false))
    {
        Lines.ReplaceInline(TEXT("\""), TEXT(""));
        TArray<FString> Segs;
        Lines.ParseIntoArray(Segs, TEXT(";"));
        for (const FString& Seg : Segs)
        {
            TArray<FString> V;
            Seg.ParseIntoArray(V, TEXT(","));
            if (V.Num() < 5) continue;
            const FVector2D A(FCString::Atod(*V[0]), FCString::Atod(*V[1])), B(FCString::Atod(*V[2]), FCString::Atod(*V[3]));
            const double StepCm = FMath::Max(1.0, FCString::Atod(*V[4]));
            const int32 Count = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, B) / StepCm));
            for (int32 K = 0; K <= Count; ++K)
            {
                const FVector2D P = FMath::Lerp(A, B, double(K) / Count);
                const float G = HouseExteriorSite::GroundHeight(float(P.X), float(P.Y));
                FString Row;
                float From = FMath::Max(G + 2500.f, 7500.f), Down = 40.f;
                for (int32 T = 0; T < 12 && From > -1100.f; ++T)
                {
                    FHitResult Hit;
                    if (!W->LineTraceSingleByChannel(Hit, FVector(P, From), FVector(P, -1100.0), ECC_Pawn, Query)) break;
                    if (Hit.bStartPenetrating || Hit.Distance < .5f) { From -= Down; Down *= 2.f; continue; }
                    Down = 40.f;
                    Row += FString::Printf(TEXT(" %.1f:%s:%d:%.2f"), Hit.ImpactPoint.Z, Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("-"),
                        int32(Move ? Move->IsWalkable(Hit) : Hit.ImpactNormal.Z >= .71f), Hit.ImpactNormal.Z);
                    From = float(Hit.ImpactPoint.Z) - 30.f;
                }
                UE_LOG(LogTemp, Display, TEXT("CALDER_PROBE_LINE (%.1f,%.1f) ground=%.1f hits=%s"), P.X, P.Y, G, Row.IsEmpty() ? TEXT(" none") : *Row);
            }
        }
    }
    UE_LOG(LogTemp, Display, TEXT("CALDER_PROBE done points=%d radius=%.1f step=%.0f"), Points.Num(), Radius, StepUp);
    if (!ProbeSharedRun()) FPlatformMisc::RequestExitWithStatus(false, 0);
}

struct FProbeRegistration { FProbeRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&ProbeTick); } };
FProbeRegistration ProbeRegistrationInstance;
}
#endif
