// -CalderReachReview (Calder terrain job, MASTER_PROMPT "Edges and containment" and "Verification": reachability): a flood
// fill over the walkable collision, the way the player's capsule moves through it.
//  - The grid: 25 cm columns over +-32400 (the walkable square and 4 m past its edge). Each column is traced top down on the
//    Pawn channel (simple collision, as the character moves); every upward-facing surface it meets is a floor (several where a
//    deck crosses water or a stair stands over a walk). A floor is walkable by the character's own rule
//    (UCharacterMovementComponent::IsWalkable: its walkable angle and any Unwalkable override) and fits it when a capsule of its
//    radius stands there clear from the step height to 180 cm.
//  - A move goes to one of the 4 neighbouring columns: onto the highest floor there no more than the step height (45 cm) over
//    the floor it leaves; a drop (any depth) needs room to step off at the old height. A column with no floor at all is a fall
//    (off the world's edge: the fall respawn).
//  - Seeds: the house's front porch (the network), and the foot of every drop in the walkable area: the low side of every
//    declared wall, rock face, ledge, parapet and terrace edge (HouseCalderLandform::Walls), of every railed edge
//    (NeedsRail: the bridges, the dock, the rock house path) and beside every stair standing over the ground.
//  - It proves: every entrance (the campus doors in outer walls, every plan entrance apron), gate, overlook, eye spot, stair,
//    ramp and path (the walk network's samples, the landform's paths and decks) is reached from the porch; every node the fill
//    reaches, and every seed, gets back to the porch or can walk off the world's edge (no traps); nothing beyond +-31800 is
//    reached except where it is reached today; nothing walkable lies under FallLimit + 400.
//  - Today's state is the contract: a run with -NoCalderTerrain writes Saved/CalderReachReview/{beyond,traps,unreached,
//    holes}.csv, and -CalderReachBaseline=<that folder> excuses exactly what is already so today (by 25 cm column).
// Logs CALDER_REACH cells= reached= entrances=x/y overlooks=x/y traps= beyondEdge= ... and CALDER_REACH_REVIEW PASS/FAIL, writes
// Saved/CalderReachReview/result.txt (deleted first). It exits the game on a failure, and on a pass when no other self-exiting
// review or capture shares the run. It changes nothing in the world. Deer reach checks are Phase 3 (TODO there).
#if !UE_BUILD_SHIPPING
#include "HouseCalderFeatures.h"
#include "HouseCalderLandform.h"
#include "HouseCalderTerrain.h"
#include "HouseCampusInteriors.h"
#include "HouseCampusLayout.h"
#include "HouseExterior.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseWestField.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace HouseCalderReachReviewImpl
{
constexpr float Cell = 25.f;                      // the grid (column centres at -Extent + 12.5 + 25 k: never on a round wall line). (At
                                                  // 50 cm the capsule found no column in passages under about 1.07 m - a court beside
                                                  // a wall's body, a 1.2 m stair between its handrails - that the player walks.)
constexpr float Extent = 32400.f;                 // the walkable square and 4 m past its edge
constexpr int32 Side = int32(2 * Extent / Cell);  // 2592 columns a side
constexpr int32 Columns = Side * Side;
constexpr float SquareHalf = 32000.f;
constexpr float EdgeLine = 31800.f;               // a reached column beyond this is "beyond the edge"
constexpr float TopAbove = 2500.f;                // a column is traced from this far over the ground ...
constexpr float MinTopZ = 7500.f;                 // ... and from no lower than this (the bluff's 32 m rock band, the tower)
constexpr float BottomZ = -1100.f;                // ... down to the fall limit (StoryCampaign: PlayerStart - 1200 = -1099.2)
constexpr float SkipUnder = 30.f;                 // after a floor, the next trace starts this far under it
constexpr float SolidStep = 40.f;                 // inside a solid body the start moves down this far (doubling)
constexpr int32 MaxFloors = 6, MaxTraces = 24;
constexpr float Headroom = 180.f;                 // the standing capsule: clear from the step height to 180 cm (+2 cm) ...
constexpr float RadiusSlack = 2.f;                // ... at the pawn's radius less this (the grid's quantisation)
constexpr double SliceSeconds = .04;              // fill work per frame
constexpr int32 SlideColumns = 120;               // a slide down a face carries on at most this many columns (30 m)
constexpr int32 SlideTight = 6;                   // and over at most this many columns of ground it has no room to stand on (1.5 m)

enum ENode : uint8 { NWalkable = 1, NChecked = 2, NFits = 4, NReached = 8, NHome = 16, NOut = 32, NSeed = 64, NExit = 128 };
enum class EColumn : uint8 { Open, Void, Solid };
constexpr int32 MoveNone = -1, MoveFall = -2;

struct FTarget { FString Kind, Name; FVector2D At; float ZLo = 0.f, ZHi = 0.f, Radius = 0.f; FBox2D Box = FBox2D(ForceInit); FBox2D Keep = FBox2D(ForceInit); bool bCalder = false; bool bReached = false; bool bToday = false; };
struct FDropSeed { FString Kind; FVector2D At; int32 Node = INDEX_NONE; };

struct FState
{
    TWeakObjectPtr<UWorld> World;
    int32 Phase = 0;
    double Start = 0.0, Began = 0.0;
    FString Dir, Log;
    // the grid
    TArray<int32> First;          // Columns + 1: a column's first node
    TArray<EColumn> Kind;
    TArray<float> Z;              // per node, top down within a column
    TArray<uint8> Flags;
    TArray<int32> Col;            // per node: its column
    int32 Scanned = 0;
    // the walker's shape and rules
    TWeakObjectPtr<UCharacterMovementComponent> Move;
    float Radius = 30.f, StepUp = 45.f;
    FCollisionQueryParams Query;
    TSet<const UPrimitiveComponent*> GroundParts;
    int64 Traces = 0, Overlaps = 0;
    int32 Enclosed = 0;           // floors found inside a closed body (not standing room)
    TMap<int64, int32> SlideTo;     // (node * 4 + direction) -> where its slide lands (a node, or MoveFall)
    TMultiMap<int32, int32> SlideFrom;   // landing node -> the nodes whose slide ends there
    TArray<int32> SlideExits;       // nodes whose slide ends off the world
    int32 Slides = 0;
    // the fill
    TArray<int32> Queue;
    int32 Head = 0;
    int32 Porch = INDEX_NONE;
    int32 Reached = 0, HomeCount = 0, OutCount = 0;
    TArray<FDropSeed> Seeds;
    int32 Unseeded = 0;
    TArray<FTarget> Targets;
    bool bTerrain = false;
    FString Baseline;
};
FState& State() { static FState S; return S; }

bool SharedRun()
{
    static const TCHAR* const SelfExiting[] = {
        TEXT("FreeRoamWalkReview"), TEXT("ParkingLotWalkReview"), TEXT("WoodsWalkReview"), TEXT("ClockTowerReview"), TEXT("HouseExteriorViews"),
        TEXT("CampusInteriorsReview"), TEXT("CampusInteriorsSmoke"), TEXT("HouseExteriorWalkReview"), TEXT("HouseWalkPerf"), TEXT("CampusDrumsRecord"),
        TEXT("WestFieldWalkReview"), TEXT("CampusLayoutReview"), TEXT("CalderTerrainReview") };
    TArray<FString> Tokens, Switches;
    FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);
    for (FString Sw : Switches)
    {
        int32 Eq = INDEX_NONE;
        if (Sw.FindChar(TEXT('='), Eq)) Sw.LeftInline(Eq);   // the switch's name only (-UserDir=.../review must not count)
        for (const TCHAR* Name : SelfExiting) if (Sw.Equals(Name, ESearchCase::CaseSensitive)) return true;
    }
    return false;
}

// ------------------------------------------------------------------ the grid
int32 ColOf(float V) { return FMath::FloorToInt((V + Extent) / Cell); }
float CentreOf(int32 K) { return -Extent + (float(K) + .5f) * Cell; }
FVector2D ColumnXY(int32 C) { return FVector2D(CentreOf(C % Side), CentreOf(C / Side)); }
int32 ColumnAt(const FVector2D& P)
{
    const int32 IX = ColOf(float(P.X)), IY = ColOf(float(P.Y));
    return IX >= 0 && IX < Side && IY >= 0 && IY < Side ? IY * Side + IX : INDEX_NONE;
}
int32 Neighbour(int32 C, int32 Dir)
{
    const int32 IX = C % Side, IY = C / Side;
    const int32 NX = IX + (Dir == 0 ? 1 : Dir == 1 ? -1 : 0), NY = IY + (Dir == 2 ? 1 : Dir == 3 ? -1 : 0);
    return NX >= 0 && NX < Side && NY >= 0 && NY < Side ? NY * Side + NX : INDEX_NONE;
}
bool Inside(const FVector2D& P, float Half) { return FMath::Abs(P.X) <= Half && FMath::Abs(P.Y) <= Half; }
/** Which of Neighbour's four directions leads from column C to its neighbour C2 (0 when they are not neighbours). */
int32 DirOf(int32 C, int32 C2)
{
    for (int32 D = 0; D < 4; ++D) if (Neighbour(C, D) == C2) return D;
    return 0;
}

/** Trace one column top down: its floors (upward-facing surfaces) highest first. */
void ScanColumn(UWorld* W, FState& S, int32 C)
{
    const FVector2D P = ColumnXY(C);
    // (From high enough that no structure's top is above the start: beside the 32 m south bluff, 25 m over the toe's ground
    // started inside the rock band's battered face and saw open sky behind it.)
    float From = FMath::Max(HouseExteriorSite::GroundHeight(float(P.X), float(P.Y)) + TopAbove, MinTopZ);
    float Step = SolidStep;
    bool bSolid = false;
    int32 Floors = 0;
    int32 Shells = 0;   // the Calder works' surfaces (and the exterior ground mesh's) met so far in this column, from above
    for (int32 T = 0; T < MaxTraces && Floors < MaxFloors && From > BottomZ; ++T)
    {
        FHitResult Hit;
        ++S.Traces;
        if (!W->LineTraceSingleByChannel(Hit, FVector(P, From), FVector(P, BottomZ), ECC_Pawn, S.Query)) break;
        if (Hit.bStartPenetrating || Hit.Distance < .5f)
        {
            // Started inside a solid body: step down out of it, further each time.
            bSolid = true;
            From -= Step;
            Step *= 2.f;
            continue;
        }
        Step = SolidStep;
        const float HitZ = float(Hit.ImpactPoint.Z);
        if (Hit.ImpactNormal.Z > 0.f)
        {
            bool bWalk = S.Move.IsValid() ? S.Move->IsWalkable(Hit) : Hit.ImpactNormal.Z >= .71f;
            // A floor under an odd number of the Calder works' surfaces (or the exterior ground mesh's) lies inside one of them:
            // inside a rock band's battered face, the dam's body, a wall's coping, a hedge with no bottom face. Every trace from
            // above meets each surface once, front or back (a deck's top and its underside are two: the bed under it is outside;
            // the rock house's shelf under the caprock's top and underside is outside too).
            if (bWalk && (Shells & 1)) { bWalk = false; ++S.Enclosed; }
            S.Z.Add(HitZ);
            S.Flags.Add(bWalk ? NWalkable : 0);
            S.Col.Add(C);
            ++Floors;
            // Nothing walkable lies under the ground itself.
            if (S.GroundParts.Contains(Hit.GetComponent())) break;
        }
        if (const UPrimitiveComponent* Part = Hit.GetComponent())
            if (Part->GetName().StartsWith(TEXT("Calder")) || Part->GetName() == TEXT("ExteriorGrounds")) ++Shells;
        From = HitZ - SkipUnder;
    }
    S.Kind[C] = Floors > 0 ? EColumn::Open : bSolid ? EColumn::Solid : EColumn::Void;
    S.First[C + 1] = S.Z.Num();
}

/** The standing capsule (clear from the step height to the headroom) fits over a floor at Z in column C. */
bool CapsuleClear(UWorld* W, FState& S, const FVector2D& P, float FloorZ)
{
    const float HalfH = (Headroom + 2.f - S.StepUp) * .5f;
    ++S.Overlaps;
    return !W->OverlapBlockingTestByChannel(FVector(P, FloorZ + S.StepUp + HalfH), FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(S.Radius, FMath::Max(HalfH, S.Radius)), S.Query);
}
bool Fits(UWorld* W, FState& S, int32 N, int32 C)
{
    uint8& F = S.Flags[N];
    if (!(F & NWalkable)) return false;
    if (!(F & NChecked)) { F |= NChecked; if (CapsuleClear(W, S, ColumnXY(C), S.Z[N])) F |= NFits; }
    return (F & NFits) != 0;
}

/** Where a walker on node N (in column C) gets by stepping into the neighbouring column C2. */
int32 MoveTo(UWorld* W, FState& S, int32 N, int32 C2)
{
    if (C2 == INDEX_NONE || S.Kind[C2] == EColumn::Void) return MoveFall;
    if (S.Kind[C2] == EColumn::Solid) return MoveNone;
    const float From = S.Z[N];
    int32 T = INDEX_NONE;
    for (int32 M = S.First[C2]; M < S.First[C2 + 1]; ++M) if (S.Z[M] <= From + S.StepUp) { T = M; break; }
    if (T == INDEX_NONE) return MoveNone;
    if (!Fits(W, S, T, C2))
    {
        // Off an edge onto a face too steep to stand on (a rock face below a ledge), or down beside a wall too close to stand in
        // that column (off a hedge's top): the walker slides or falls on (BuildSlides).
        const int32* Slid = S.SlideTo.Find(int64(N) * 4 + DirOf(S.Col[N], C2));
        return Slid ? *Slid : MoveNone;
    }
    // A drop: there must be room to step off at the old height.
    if (From - S.Z[T] > S.StepUp && !CapsuleClear(W, S, ColumnXY(C2), From)) return MoveNone;
    return T;
}

/** Slides (computed once, before the fill): from every standing node, a step into a neighbouring column whose top surface within
 *  reach is too steep to stand on (and with room to step off at the old height) carries on down that surface the way it stepped,
 *  column by column, each surface no higher than the last, to the first floor there is standing room on: a fall off the bluff's
 *  edge ends on its toe, not on the rock face (UCharacterMovementComponent slides off what it cannot stand on). It ends as a fall
 *  where it leaves the grid or meets a void column, and nowhere where a wall stands in the way. */
void BuildSlides(UWorld* W, FState& S)
{
    for (int32 C = 0; C < Columns; ++C)
    {
        if (S.Kind[C] != EColumn::Open) continue;
        for (int32 N = S.First[C]; N < S.First[C + 1]; ++N)
        {
            if (!(S.Flags[N] & NWalkable)) continue;
            for (int32 D = 0; D < 4; ++D)
            {
                const int32 C2 = Neighbour(C, D);
                if (C2 == INDEX_NONE || S.Kind[C2] != EColumn::Open) continue;
                int32 T = INDEX_NONE;
                for (int32 M = S.First[C2]; M < S.First[C2 + 1]; ++M) if (S.Z[M] <= S.Z[N] + S.StepUp) { T = M; break; }
                if (T == INDEX_NONE) continue;
                // A steep face, or a fall to ground too close to a wall to stand on in that column (the grid's quantum).
                const bool bSteep = !(S.Flags[T] & NWalkable);
                if (!bSteep && (S.Z[N] - S.Z[T] <= S.StepUp || Fits(W, S, T, C2))) continue;
                if (!Fits(W, S, N, C) || !CapsuleClear(W, S, ColumnXY(C2), S.Z[N])) continue;
                float RefZ = S.Z[T];
                int32 Cur = C2, Lands = MoveNone, Tight = 0;
                for (int32 K = 0; K < SlideColumns; ++K)
                {
                    const int32 Next = Neighbour(Cur, D);
                    if (Next == INDEX_NONE || S.Kind[Next] == EColumn::Void) { Lands = MoveFall; break; }
                    if (S.Kind[Next] == EColumn::Solid) break;
                    int32 M = INDEX_NONE;
                    for (int32 I = S.First[Next]; I < S.First[Next + 1]; ++I) if (S.Z[I] <= RefZ + 5.f) { M = I; break; }
                    if (M == INDEX_NONE) break;                                       // a wall stands in the way
                    if (!CapsuleClear(W, S, ColumnXY(Next), S.Z[N])) break;          // the walker passes over it, from the old height
                    if (Fits(W, S, M, Next)) { Lands = M; break; }
                    // Standing ground with no room to stand (beside the face's foot, a wall): carried a little further, no more.
                    if ((S.Flags[M] & NWalkable) && ++Tight > SlideTight) break;
                    RefZ = S.Z[M];
                    Cur = Next;
                }
                if (Lands == MoveNone) continue;
                S.SlideTo.Add(int64(N) * 4 + D, Lands);
                ++S.Slides;
                if (Lands >= 0) S.SlideFrom.Add(Lands, N);
                else S.SlideExits.Add(N);
            }
        }
    }
}

/** The standing node (walkable, the capsule fits) nearest P within Radius (0: P's own column) whose floor lies in [ZLo, ZHi]. */
int32 NodeNear(UWorld* W, FState& S, const FVector2D& P, float Radius, float ZLo, float ZHi)
{
    const int32 R = FMath::CeilToInt(Radius / Cell);
    const int32 IX = ColOf(float(P.X)), IY = ColOf(float(P.Y));
    int32 Best = INDEX_NONE;
    double BestD = 1e30;
    for (int32 DY = -R; DY <= R; ++DY)
        for (int32 DX = -R; DX <= R; ++DX)
        {
            const int32 X = IX + DX, Y = IY + DY;
            if (X < 0 || X >= Side || Y < 0 || Y >= Side) continue;
            const int32 C = Y * Side + X;
            const double D = FVector2D::DistSquared(ColumnXY(C), P);
            if ((DX || DY) && D > FMath::Square(Radius + Cell * .5f)) continue;
            if (D >= BestD) continue;
            for (int32 N = S.First[C]; N < S.First[C + 1]; ++N)
            {
                if (S.Z[N] < ZLo || S.Z[N] > ZHi || !Fits(W, S, N, C)) continue;
                Best = N; BestD = D;
                break;
            }
        }
    return Best;
}

// ------------------------------------------------------------------ the targets
float Ground(const FVector2D& P) { return HouseExteriorSite::GroundHeight(float(P.X), float(P.Y)); }
float Base(const FVector2D& P) { return HouseExteriorSite::BaseGrade(float(P.X), float(P.Y)); }

void AddPoint(FState& S, const TCHAR* Kind, const FString& Name, const FVector2D& At, float Z, float Below, float Above, float Radius, bool bCalder)
{
    FTarget T;
    T.Kind = Kind; T.Name = Name; T.At = At; T.ZLo = Z - Below; T.ZHi = Z + Above; T.Radius = Radius; T.bCalder = bCalder;
    S.Targets.Add(T);
}

void BuildTargets(UWorld* W, FState& S)
{
    // Entrances: every campus door in an outer wall (a pair counts once), from 1.2 m outside it, and every plan entrance apron.
    TSet<FName> Groups;
    for (TActorIterator<ACampusDoor> It(W); It; ++It)
    {
        if (!It->bOuterDoor) continue;
        const FName Group = It->DoorGroup.IsNone() ? It->DoorId : It->DoorGroup;
        if (Groups.Contains(Group)) continue;
        Groups.Add(Group);
        FString Id = It->DoorId.ToString().RightChop(7);   // Campus_<Id>_<Name>
        Id = Id.Left(FMath::Max(0, Id.Find(TEXT("_"))));
        const int32 Seed = HouseCampusInteriors::SeedFor(*Id);
        const float Floor = float(It->DoorwayCenter.Z) - 105.f;
        // Outward: the axis direction whose point 1.5 m away is furthest outside the building.
        FVector2D Out(0.f, 0.f);
        float BestOut = -1.f;
        for (const FVector2D D : { FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(0, -1) })
        {
            const FVector Q(FVector2D(It->DoorwayCenter) + D * 150.f, Floor + 50.f);
            if (Seed != INDEX_NONE && HouseCampusInteriors::InsideInterior(Seed, Q)) continue;
            const float Dist = HouseCampusInteriors::DistanceOutside(Q);
            if (Dist > BestOut) { BestOut = Dist; Out = D; }
        }
        AddPoint(S, TEXT("entrance"), Group.ToString(), FVector2D(It->DoorwayCenter) + Out * 120.f, Floor, 150.f, 60.f, 150.f, false);
    }
    TArray<HouseCampusLayout::FApron> Aprons;
    HouseCampusLayout::EntranceAprons(Aprons);
    for (int32 I = 0; I < Aprons.Num(); ++I)
    {
        // Any standing node on the apron (clear of its building's face) or, for a shallow apron, within 1 m of its outer edge's
        // middle (the walk that meets it) at the apron's level.
        FTarget T;
        T.Kind = TEXT("entrance"); T.Name = FString::Printf(TEXT("apron %d"), I);
        T.Box = Aprons[I].R.ExpandBy(-10.f); T.ZLo = Aprons[I].Top - 40.f; T.ZHi = Aprons[I].Top + 40.f; T.Radius = 100.f;
        T.At = Aprons[I].R.GetCenter();
        for (const HouseCampusLayout::FBlock& B : HouseCampusLayout::Blocks())
        {
            if (!B.Rect().ExpandBy(1.f).Intersect(Aprons[I].R)) continue;
            T.Keep = B.Rect().ExpandBy(35.f);
            // the outer edge: the apron's side furthest from the building's centre
            const FVector2D C = Aprons[I].R.GetCenter(), E = Aprons[I].R.GetExtent(), Away = C - B.Centre();
            T.At = FMath::Abs(Away.X) * (B.Y1 - B.Y0) > FMath::Abs(Away.Y) * (B.X1 - B.X0) ? FVector2D(C.X + FMath::Sign(Away.X) * E.X, C.Y) : FVector2D(C.X, C.Y + FMath::Sign(Away.Y) * E.Y);
        }
        S.Targets.Add(T);
    }
    // Gates: the campus gateposts' opening, the stadium's north gates (its ticket plaza), the west field's gate.
    if (HouseCampusLayout::Enabled())
    {
        AddPoint(S, TEXT("gate"), TEXT("Calder Dr gateposts"), FVector2D(9500.f, -6150.f), Ground(FVector2D(9500.f, -6150.f)) - 12.f, 40.f, 40.f, 150.f, false);
        AddPoint(S, TEXT("gate"), TEXT("stadium north gates (ticket plaza)"), FVector2D(2400.f, -9000.f), Ground(FVector2D(2400.f, -9000.f)) + 3.f, 40.f, 40.f, 150.f, false);
    }
    {
        const FVector2D G((HouseWestField::GateWest + HouseWestField::GateEast) * .5f, HouseWestField::FenceY);
        AddPoint(S, TEXT("gate"), TEXT("west field gate"), G, Ground(G), 80.f, 80.f, 200.f, false);
    }
    // Paths: the walk network's own samples (not those a Calder stair or ramp stands over).
    TArray<FVector> Walks;
    HouseCampusLayout::WalkSamples(Walks);
    for (int32 I = 0; I < Walks.Num(); ++I)
    {
        const FVector2D P(Walks[I]);
        if (HouseCalderFeatures::InStair(P, 5.f) || HouseCalderFeatures::InRamp(P, 5.f)) continue;
        AddPoint(S, TEXT("path"), FString::Printf(TEXT("walk sample %d"), I), P, float(Walks[I].Z), 30.f, 30.f, 75.f, false);
    }
    if (!S.bTerrain) return;

    // ---- the Calder landform's own (valid with the terrain on)
    for (const HouseCalderLandform::FSpot& O : HouseCalderLandform::Overlooks())
        AddPoint(S, TEXT("overlook"), O.Name, FVector2D(O.At), O.GroundZ, 60.f, 60.f, 250.f, true);
    for (const HouseCalderLandform::FSpot& O : HouseCalderLandform::EyeSpots())
        AddPoint(S, TEXT("eye"), O.Name, FVector2D(O.At), O.GroundZ, 60.f, 60.f, 150.f, true);
    for (const HouseCalderLandform::FStair& T : HouseCalderLandform::Stairs())
    {
        AddPoint(S, TEXT("stair"), T.Name + TEXT(" (head)"), T.Head, Base(T.Head) + T.HeadRelief, 60.f, 60.f, 150.f, true);
        AddPoint(S, TEXT("stair"), T.Name + TEXT(" (foot)"), T.Foot, Base(T.Foot) + T.FootRelief, 60.f, 60.f, 150.f, true);
    }
    for (const HouseCalderLandform::FRamp& R : HouseCalderLandform::Ramps())
    {
        AddPoint(S, TEXT("ramp"), R.Name + TEXT(" (foot)"), R.A, Base(R.A) + R.Z0, 60.f, 60.f, 150.f, true);
        AddPoint(S, TEXT("ramp"), R.Name + TEXT(" (top)"), R.B, Base(R.B) + R.Z1, 60.f, 60.f, 150.f, true);
    }
    // The landform's paths every 2 m (gravel on the ground; the dam's crest walk, the arch and the causeway at their decks).
    float ArchDeck = HouseCalderLandform::PondDam().CrestZ, CausewayDeck = ArchDeck;
    for (const HouseCalderLandform::FWaterCrossing& X : HouseCalderLandform::WaterCrossings())
    {
        if (X.Kind == HouseCalderLandform::EWaterCrossing::ArchFootbridge) ArchDeck = X.DeckZ;
        if (X.Kind == HouseCalderLandform::EWaterCrossing::CausewayCulvert) CausewayDeck = X.DeckZ;
        // and every crossing's own deck, end to end
        const int32 N = FMath::Max(1, FMath::CeilToInt((X.Back + X.On) / 200.f));
        for (int32 K = 0; K <= N; ++K)
        {
            const FVector2D P = X.Centre + X.Axis * FMath::Lerp(-X.Back, X.On, float(K) / N);
            AddPoint(S, TEXT("path"), FString::Printf(TEXT("%s %d"), *X.Name, K), P, X.DeckZ, 60.f, 60.f, 100.f, true);
        }
    }
    for (const HouseCalderLandform::FLandformPath& Path : HouseCalderLandform::LandformPaths())
    {
        const int32 Count = Path.Points.Num(), Legs = Path.bClosed ? Count : Count - 1;
        for (int32 I = 0; I < Legs; ++I)
        {
            const FVector2D A = Path.Points[I], B = Path.Points[(I + 1) % Count];
            const HouseCalderLandform::EPathStretch K = Path.Stretches.IsValidIndex(I) ? Path.Stretches[I] : HouseCalderLandform::EPathStretch::Ground;
            const int32 N = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, B) / 200.f));
            for (int32 J = 0; J < N; ++J)
            {
                const FVector2D P = FMath::Lerp(A, B, float(J) / N);
                float Zp = K == HouseCalderLandform::EPathStretch::Ground ? Ground(P) + 3.f
                    : K == HouseCalderLandform::EPathStretch::ArchFootbridge ? ArchDeck
                    : K == HouseCalderLandform::EPathStretch::Causeway ? CausewayDeck : HouseCalderLandform::PondDam().CrestZ;
                if (K == HouseCalderLandform::EPathStretch::Ground) HouseCalderFeatures::FloorAt(P, Zp);   // a timber flight's tread
                AddPoint(S, TEXT("path"), FString::Printf(TEXT("%s %d.%d"), *Path.Name, I, J), P, Zp, 40.f, 40.f, 100.f, true);
            }
        }
    }
    {
        const HouseCalderLandform::FRockHouse& R = HouseCalderLandform::RockHouse();
        const int32 N = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(R.PathA, R.PathB) / 200.f));
        for (int32 K = 0; K <= N; ++K)
            AddPoint(S, TEXT("path"), FString::Printf(TEXT("the rock house path %d"), K), FMath::Lerp(R.PathA, R.PathB, float(K) / N), R.FloorZ, 40.f, 40.f, 75.f, true);
        const HouseCalderLandform::FPondDock& D = HouseCalderLandform::PondDock();
        const int32 ND = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(D.Land, D.Tip) / 200.f));
        for (int32 K = 0; K <= ND; ++K)
            AddPoint(S, TEXT("path"), FString::Printf(TEXT("the dock %d"), K), FMath::Lerp(D.Land, D.Tip, float(K) / ND) - D.Axis * (K == ND ? 40.f : 0.f), D.DeckZ, 40.f, 40.f, 75.f, true);
        const HouseCalderLandform::FCrowLedge& L = HouseCalderLandform::CrowLedge();
        FVector2D C(0.f, 0.f);
        for (const FVector2D& V : L.Outline) C += V / float(FMath::Max(1, L.Outline.Num()));
        AddPoint(S, TEXT("overlook"), TEXT("Crow Ledge (its middle)"), C, L.LevelZ, 40.f, 40.f, 150.f, true);
    }
}

// ------------------------------------------------------------------ the seeds at the foot of every drop
void AddSeed(UWorld* W, FState& S, const TCHAR* Kind, const FVector2D& P, const FVector2D& Out, float HighZ, float StartAt)
{
    // (The foot of a drop is in the open: a spot with one of the Calder works over it - inside a rock band's battered face, under
    // the dam's crest - is not where a fall ends, so the search goes on out past it, as far as a battered face's foot.)
    for (const float D : { StartAt, StartAt + 35.f, StartAt + 75.f, StartAt + 125.f, StartAt + 185.f, StartAt + 260.f, StartAt + 350.f, StartAt + 460.f, StartAt + 600.f })
    {
        const FVector2D Q = P + Out * D;
        if (!Inside(Q, SquareHalf)) break;
        const float G = Ground(Q);
        if (G > HighZ - S.StepUp) break;                     // no drop here (the ground climbs back)
        {
            FHitResult Over;
            if (W->LineTraceSingleByChannel(Over, FVector(Q, FMath::Max(G + TopAbove, MinTopZ)), FVector(Q, G + 30.f), ECC_Pawn, S.Query) && Over.GetComponent()
                && Over.GetComponent()->GetName().StartsWith(TEXT("Calder")))
            {
                ++S.Traces;
                continue;
            }
        }
        const int32 N = NodeNear(W, S, Q, 0.f, G - 60.f, G + 60.f);
        if (N == INDEX_NONE) continue;
        S.Flags[N] |= NSeed;
        S.Seeds.Add({ FString(Kind), Q, N });
        return;
    }
    ++S.Unseeded;
    if (S.Unseeded <= 12) S.Log += FString::Printf(TEXT("unseeded: %s at (%.0f,%.0f) (no standing room in the open on the low side within %.0f cm)\n"), Kind, P.X, P.Y, StartAt + 600.f);
}

void BuildSeeds(UWorld* W, FState& S)
{
    if (!S.bTerrain) return;
    auto InBuilding = [](const FVector2D& P)
    {
        for (const HouseCampusLayout::FBlock& B : HouseCampusLayout::Blocks()) if (B.Rect().ExpandBy(40.f).IsInside(P)) return true;
        return false;
    };
    for (const HouseCalderLandform::FWall& Wl : HouseCalderLandform::Walls())
    {
        const FVector2D AB = Wl.B - Wl.A;
        const float Len = float(AB.Size());
        if (Len < 1.f) continue;
        const FVector2D U = AB / Len, Nrm(-U.Y, U.X);
        const int32 N = FMath::Max(1, FMath::CeilToInt(Len / 200.f));
        for (int32 K = 0; K < N; ++K)
        {
            const FVector2D P = Wl.A + U * (Len * (float(K) + .5f) / float(N));
            if (InBuilding(P) || HouseCalderFeatures::InStair(P, 30.f) || HouseCalderFeatures::InRamp(P, 30.f)) continue;
            const float ZL = Ground(P + Nrm * 5.f), ZR = Ground(P - Nrm * 5.f);
            if (FMath::Abs(ZL - ZR) <= S.StepUp) continue;
            const bool bLeftLow = ZL < ZR;
            AddSeed(W, S, TEXT("wall foot"), P, bLeftLow ? Nrm : -Nrm, FMath::Max(ZL, ZR), 70.f);
        }
    }
    for (const HouseCalderLandform::FRoadsideRun& R : HouseCalderLandform::NeedsRail())
        for (int32 I = 0; I < R.Points.Num(); ++I)
        {
            const FVector2D A = R.Points[FMath::Max(0, I - 1)], B = R.Points[FMath::Min(R.Points.Num() - 1, I + 1)];
            const FVector2D U = (B - A).GetSafeNormal();
            if (U.IsNearlyZero()) continue;
            const FVector2D Out = R.Side == 0 ? (I == 0 ? -U : U) : FVector2D(-U.Y, U.X) * float(R.Side);
            AddSeed(W, S, TEXT("railed edge"), R.Points[I], Out, Ground(R.Points[I]) + R.Value, 80.f);
        }
    for (const HouseCalderLandform::FStair& T : HouseCalderLandform::Stairs())
    {
        const FVector2D M = (T.Head + T.Foot) * .5f, U = (T.Head - T.Foot).GetSafeNormal(), Across(-U.Y, U.X);
        float Tread = (Base(T.Head) + T.HeadRelief + Base(T.Foot) + T.FootRelief) * .5f;
        HouseCalderFeatures::FloorAt(M, Tread);
        for (const float Sgn : { -1.f, 1.f })
        {
            const FVector2D Edge = M + Across * Sgn * (T.Width * .5f + 30.f);
            if (Ground(Edge + Across * Sgn * 60.f) < Tread - S.StepUp) AddSeed(W, S, TEXT("beside a stair"), Edge, Across * Sgn, Tread, 50.f);
        }
    }
}

// ------------------------------------------------------------------ the baseline (today's, from a -NoCalderTerrain run)
int64 Key(int32 C) { return int64(C); }
void ReadCsvColumns(const FString& Path, TSet<int64>& Out)
{
    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *Path)) return;
    for (int32 I = 1; I < Lines.Num(); ++I)
    {
        TArray<FString> F;
        Lines[I].ParseIntoArray(F, TEXT(","), false);
        if (F.Num() < 2) continue;
        const int32 C = ColumnAt(FVector2D(FCString::Atof(*F[F.Num() >= 4 ? 1 : 0]), FCString::Atof(*F[F.Num() >= 4 ? 2 : 1])));
        if (C != INDEX_NONE) Out.Add(Key(C));
    }
}
bool InSetNear(const TSet<int64>& Set, int32 C)
{
    if (Set.Contains(Key(C))) return true;
    for (int32 D = 0; D < 4; ++D) { const int32 N = Neighbour(C, D); if (N != INDEX_NONE && Set.Contains(Key(N))) return true; }
    return false;
}

// ------------------------------------------------------------------ the fill, a slice at a time
/** Forward: from the porch over every move. Returns true when the queue is empty. */
bool PumpForward(UWorld* W, FState& S, double Until)
{
    while (S.Head < S.Queue.Num())
    {
        if ((S.Head & 255) == 0 && FPlatformTime::Seconds() > Until) return false;
        const int32 N = S.Queue[S.Head++];
        const int32 C = S.Col[N];
        for (int32 D = 0; D < 4; ++D)
        {
            const int32 T = MoveTo(W, S, N, Neighbour(C, D));
            if (T == MoveFall) { S.Flags[N] |= NExit; continue; }
            if (T < 0 || (S.Flags[T] & NReached)) continue;
            S.Flags[T] |= NReached;
            ++S.Reached;
            S.Queue.Add(T);
        }
    }
    return true;
}
/** Backward: every node from which a move reaches a marked node gets the mark too. */
bool PumpBackward(UWorld* W, FState& S, double Until, uint8 Mark, int32& Count)
{
    while (S.Head < S.Queue.Num())
    {
        if ((S.Head & 255) == 0 && FPlatformTime::Seconds() > Until) return false;
        const int32 H = S.Queue[S.Head++];
        const int32 C = S.Col[H];
        for (int32 D = 0; D < 4; ++D)
        {
            const int32 C2 = Neighbour(C, D);
            if (C2 == INDEX_NONE || S.Kind[C2] != EColumn::Open) continue;
            for (int32 M = S.First[C2]; M < S.First[C2 + 1]; ++M)
            {
                if ((S.Flags[M] & Mark) || !Fits(W, S, M, C2)) continue;
                if (MoveTo(W, S, M, C) != H) continue;
                S.Flags[M] |= Mark;
                ++Count;
                S.Queue.Add(M);
            }
        }
        TArray<int32> Sliders;
        S.SlideFrom.MultiFind(H, Sliders);
        for (const int32 M : Sliders)
        {
            if (S.Flags[M] & Mark) continue;
            S.Flags[M] |= Mark;
            ++Count;
            S.Queue.Add(M);
        }
    }
    return true;
}
/** Every node that can walk off the world (a move into a void column or off the grid). */
void FindExits(UWorld* W, FState& S)
{
    S.Queue.Reset(); S.Head = 0;
    for (int32 C = 0; C < Columns; ++C)
    {
        if (S.Kind[C] != EColumn::Open) continue;
        bool bEdge = false;
        for (int32 D = 0; D < 4 && !bEdge; ++D) { const int32 N = Neighbour(C, D); bEdge = N == INDEX_NONE || S.Kind[N] == EColumn::Void; }
        if (!bEdge) continue;
        for (int32 N = S.First[C]; N < S.First[C + 1]; ++N)
        {
            if (!Fits(W, S, N, C)) continue;
            for (int32 D = 0; D < 4; ++D)
                if (MoveTo(W, S, N, Neighbour(C, D)) == MoveFall) { S.Flags[N] |= NExit | NOut; ++S.OutCount; S.Queue.Add(N); break; }
        }
    }
    for (const int32 N : S.SlideExits)
        if (!(S.Flags[N] & NOut)) { S.Flags[N] |= NExit | NOut; ++S.OutCount; S.Queue.Add(N); }
}

bool ReachedTarget(FState& S, const FTarget& T)
{
    if (T.Box.bIsValid)
    {
        bool bOnBox = false;
        const int32 X0 = FMath::Max(0, ColOf(float(T.Box.Min.X))), X1 = FMath::Min(Side - 1, ColOf(float(T.Box.Max.X)));
        const int32 Y0 = FMath::Max(0, ColOf(float(T.Box.Min.Y))), Y1 = FMath::Min(Side - 1, ColOf(float(T.Box.Max.Y)));
        for (int32 Y = Y0; Y <= Y1; ++Y)
            for (int32 X = X0; X <= X1; ++X)
            {
                const int32 C = Y * Side + X;
                const FVector2D P = ColumnXY(C);
                if (!T.Box.IsInside(P) || (T.Keep.bIsValid && T.Keep.IsInside(P))) continue;
                for (int32 N = S.First[C]; N < S.First[C + 1]; ++N) if ((S.Flags[N] & NReached) && S.Z[N] >= T.ZLo && S.Z[N] <= T.ZHi) return true;
                bOnBox = true;
            }
        if (bOnBox || T.Radius <= 0.f) return false;   // a box with room on it must be reached on it; a shallow one at its edge
    }
    const int32 R = FMath::CeilToInt(T.Radius / Cell);
    const int32 IX = ColOf(float(T.At.X)), IY = ColOf(float(T.At.Y));
    for (int32 DY = -R; DY <= R; ++DY)
        for (int32 DX = -R; DX <= R; ++DX)
        {
            const int32 X = IX + DX, Y = IY + DY;
            if (X < 0 || X >= Side || Y < 0 || Y >= Side) continue;
            const int32 C = Y * Side + X;
            if (FVector2D::DistSquared(ColumnXY(C), T.At) > FMath::Square(T.Radius + Cell * .5f)) continue;
            for (int32 N = S.First[C]; N < S.First[C + 1]; ++N) if ((S.Flags[N] & NReached) && S.Z[N] >= T.ZLo && S.Z[N] <= T.ZHi) return true;
        }
    return false;
}

struct FCluster { int32 Nodes = 0; FVector2D Sum = FVector2D::ZeroVector; float MinZ = 1e9f, MaxZ = -1e9f; int32 Seeds = 0; int32 Today = 0; FVector At = FVector::ZeroVector; };

void Finish(UWorld* W, FState& S)
{
    const bool bBaseline = !S.Baseline.IsEmpty();
    TSet<int64> TodayBeyond, TodayTraps, TodayUnreached, TodayHoles;
    if (bBaseline)
    {
        ReadCsvColumns(S.Baseline / TEXT("beyond.csv"), TodayBeyond);
        ReadCsvColumns(S.Baseline / TEXT("traps.csv"), TodayTraps);
        ReadCsvColumns(S.Baseline / TEXT("unreached.csv"), TodayUnreached);
        ReadCsvColumns(S.Baseline / TEXT("holes.csv"), TodayHoles);
    }
    const auto Allowed = [&](const TSet<int64>& Today, int32 C) { return bBaseline ? InSetNear(Today, C) : !S.bTerrain; };

    // Beyond the edge, the lowest reached ground, falls into holes inside the square.
    FString BeyondCsv = TEXT("x,y,z\n"), HolesCsv = TEXT("x,y,z\n");
    int32 Beyond = 0, BeyondToday = 0, HoleExits = 0, HoleToday = 0;
    float Lowest = 1e9f;
    FVector2D LowestAt(0.f, 0.f);
    for (int32 C = 0; C < Columns; ++C)
    {
        const FVector2D P = ColumnXY(C);
        for (int32 N = S.First[C]; N < S.First[C + 1]; ++N)
        {
            if (!(S.Flags[N] & NReached)) continue;
            if (Inside(P, SquareHalf) && S.Z[N] < Lowest) { Lowest = S.Z[N]; LowestAt = P; }
            if (!Inside(P, EdgeLine))
            {
                BeyondCsv += FString::Printf(TEXT("%.0f,%.0f,%.1f\n"), P.X, P.Y, S.Z[N]);
                // Without a baseline: the frozen zones are today's, and the edge stays open as today wherever its ground keeps
                // within 3 m of today's grade (MASTER_PROMPT "Edges and containment").
                const bool bToday = bBaseline ? InSetNear(TodayBeyond, C) : (!S.bTerrain || HouseCalderTerrain::InFrozen(float(P.X), float(P.Y))
                    || FMath::Abs(Ground(P) - Base(P)) <= 300.f);
                (bToday ? BeyondToday : Beyond)++;
                if (!bToday && Beyond <= 10) S.Log += FString::Printf(TEXT("beyond the edge: reached (%.0f,%.0f,%.0f)\n"), P.X, P.Y, S.Z[N]);
            }
            if ((S.Flags[N] & NExit) && Inside(P, SquareHalf - Cell))
                for (int32 D = 0; D < 4; ++D)
                {
                    const int32 C2 = Neighbour(C, D);
                    if (C2 == INDEX_NONE || S.Kind[C2] != EColumn::Void || !Inside(ColumnXY(C2), SquareHalf)) continue;
                    HolesCsv += FString::Printf(TEXT("%.0f,%.0f,%.1f\n"), ColumnXY(C2).X, ColumnXY(C2).Y, S.Z[N]);
                    const bool bToday = Allowed(TodayHoles, C2);
                    (bToday ? HoleToday : HoleExits)++;
                    if (!bToday && HoleExits <= 10) S.Log += FString::Printf(TEXT("a hole in the ground at (%.0f,%.0f) beside a reached floor at %.0f\n"), ColumnXY(C2).X, ColumnXY(C2).Y, S.Z[N]);
                }
        }
    }

    // Traps: reached nodes and seeds that neither get back to the porch nor can walk off the world, in clusters.
    TArray<int32> Trapped;
    for (int32 N = 0; N < S.Z.Num(); ++N)
        if ((S.Flags[N] & (NReached | NSeed)) && !(S.Flags[N] & (NHome | NOut)) && (S.Flags[N] & NFits)) Trapped.Add(N);
    TMap<int32, TArray<int32>> ByCol;
    for (const int32 N : Trapped) ByCol.FindOrAdd(S.Col[N]).Add(N);
    TArray<FCluster> Clusters;
    TSet<int32> Done;
    FString TrapsCsv = TEXT("cluster,x,y,z,seed\n");
    for (const int32 N0 : Trapped)
    {
        const int32 C0 = S.Col[N0];
        if (Done.Contains(C0)) continue;
        FCluster K;
        TArray<int32> Stack = { C0 };
        Done.Add(C0);
        while (Stack.Num())
        {
            const int32 C = Stack.Pop(EAllowShrinking::No);
            const FVector2D P = ColumnXY(C);
            for (const int32 N : ByCol[C])
            {
                ++K.Nodes; K.Sum += P; K.MinZ = FMath::Min(K.MinZ, S.Z[N]); K.MaxZ = FMath::Max(K.MaxZ, S.Z[N]);
                K.Seeds += (S.Flags[N] & NSeed) ? 1 : 0;
                K.Today += InSetNear(TodayTraps, C) ? 1 : 0;
                if (K.Nodes == 1) K.At = FVector(P, S.Z[N]);
                TrapsCsv += FString::Printf(TEXT("%d,%.0f,%.0f,%.1f,%d\n"), Clusters.Num(), P.X, P.Y, S.Z[N], (S.Flags[N] & NSeed) ? 1 : 0);
            }
            for (int32 DY = -1; DY <= 1; ++DY)
                for (int32 DX = -1; DX <= 1; ++DX)
                {
                    const int32 X = C % Side + DX, Y = C / Side + DY;
                    if (X < 0 || X >= Side || Y < 0 || Y >= Side) continue;
                    const int32 C2 = Y * Side + X;
                    if (ByCol.Contains(C2) && !Done.Contains(C2)) { Done.Add(C2); Stack.Add(C2); }
                }
        }
        Clusters.Add(K);
    }
    int32 Traps = 0, TrapsToday = 0, SeedTraps = 0;
    for (int32 I = 0; I < Clusters.Num(); ++I)
    {
        const FCluster& K = Clusters[I];
        const bool bToday = !S.bTerrain || (bBaseline && K.Today * 2 >= K.Nodes);
        (bToday ? TrapsToday : Traps)++;
        SeedTraps += K.Seeds;
        if (!bToday && Traps <= 20)
            S.Log += FString::Printf(TEXT("trap %d: %d nodes (%d seeds) round (%.0f,%.0f), Z %.0f..%.0f, first at %s\n"), I, K.Nodes, K.Seeds,
                K.Sum.X / K.Nodes, K.Sum.Y / K.Nodes, K.MinZ, K.MaxZ, *K.At.ToCompactString());
    }

    // Diagnosis only (-CalderReachExplain="X,Y;..."): every floor in the column at each point, its flags, and where each of its
    // four moves goes (a node with its floor and flags, none, or a fall). Changes no verdict.
    {
        FString Spec;
        if (FParse::Value(FCommandLine::Get(), TEXT("CalderReachExplain="), Spec, false))
        {
            Spec.ReplaceInline(TEXT("\""), TEXT(""));
            TArray<FString> Pts;
            Spec.ParseIntoArray(Pts, TEXT(";"));
            for (const FString& Pt : Pts)
            {
                FString XS, YS;
                if (!Pt.Split(TEXT(","), &XS, &YS)) continue;
                const int32 C = ColumnAt(FVector2D(FCString::Atod(*XS), FCString::Atod(*YS)));
                if (C == INDEX_NONE) continue;
                for (int32 N = S.First[C]; N < S.First[C + 1]; ++N)
                {
                    FString Moves;
                    for (int32 D = 0; D < 4; ++D)
                    {
                        const int32 C2 = Neighbour(C, D);
                        const int32 T = MoveTo(W, S, N, C2);
                        FString Floors;
                        if (C2 != INDEX_NONE) for (int32 M = S.First[C2]; M < S.First[C2 + 1]; ++M) Floors += FString::Printf(TEXT("%s%.0f/%d"), Floors.IsEmpty() ? TEXT("") : TEXT(","), S.Z[M], int32(S.Flags[M]));
                        Moves += FString::Printf(TEXT(" d%d->%s[%s]"), D, T == MoveFall ? TEXT("fall") : T < 0 ? TEXT("none") : *FString::Printf(TEXT("%.0f/%d"), S.Z[T], int32(S.Flags[T])), *Floors);
                    }
                    UE_LOG(LogTemp, Display, TEXT("CALDER_REACH_EXPLAIN (%.0f,%.0f) z=%.1f flags=%d%s"), ColumnXY(C).X, ColumnXY(C).Y, S.Z[N], int32(S.Flags[N]), *Moves);
                }
            }
        }
    }

    // The targets.
    FString UnreachedCsv = TEXT("kind,x,y,z,name\n");
    TMap<FString, FIntPoint> Tally;
    int32 Missing = 0, MissingToday = 0;
    for (FTarget& T : S.Targets)
    {
        T.bReached = ReachedTarget(S, T);
        FIntPoint& Row = Tally.FindOrAdd(T.Kind);
        ++Row.Y;
        if (T.bReached) { ++Row.X; continue; }
        const int32 C = ColumnAt(T.At);
        UnreachedCsv += FString::Printf(TEXT("%s,%.0f,%.0f,%.1f,%s\n"), *T.Kind, T.At.X, T.At.Y, (T.ZLo + T.ZHi) * .5f, *T.Name.Replace(TEXT(","), TEXT(";")));
        T.bToday = !S.bTerrain || (!T.bCalder && bBaseline && C != INDEX_NONE && InSetNear(TodayUnreached, C));
        (T.bToday ? MissingToday : Missing)++;
        if (!T.bToday && Missing <= 30)
            S.Log += FString::Printf(TEXT("unreached %s: %s at (%.0f,%.0f) Z %.0f..%.0f\n"), *T.Kind, *T.Name, T.At.X, T.At.Y, T.ZLo, T.ZHi);
    }
    auto Count = [&Tally](const TCHAR* K) { const FIntPoint* R = Tally.Find(K); return R ? FString::Printf(TEXT("%d/%d"), R->X, R->Y) : FString(TEXT("0/0")); };

    IFileManager::Get().MakeDirectory(*S.Dir, true);
    FFileHelper::SaveStringToFile(BeyondCsv, *(S.Dir / TEXT("beyond.csv")));
    FFileHelper::SaveStringToFile(TrapsCsv, *(S.Dir / TEXT("traps.csv")));
    FFileHelper::SaveStringToFile(UnreachedCsv, *(S.Dir / TEXT("unreached.csv")));
    FFileHelper::SaveStringToFile(HolesCsv, *(S.Dir / TEXT("holes.csv")));
    {
        FString SeedsCsv = TEXT("kind,x,y,z,home,out\n");
        for (const FDropSeed& D : S.Seeds)
            SeedsCsv += FString::Printf(TEXT("%s,%.0f,%.0f,%.1f,%d,%d\n"), *D.Kind, D.At.X, D.At.Y, S.Z[D.Node], (S.Flags[D.Node] & NHome) ? 1 : 0, (S.Flags[D.Node] & NOut) ? 1 : 0);
        FFileHelper::SaveStringToFile(SeedsCsv, *(S.Dir / TEXT("seeds.csv")));
    }

    const float MinWalk = HouseCalderLandform::MinWalkableZ();
    const bool bLowOK = !S.bTerrain || Lowest >= MinWalk - .5f;
    const bool bOK = S.Porch != INDEX_NONE && Missing == 0 && Traps == 0 && Beyond == 0 && HoleExits == 0 && bLowOK;
    const int32 Ms = int32((FPlatformTime::Seconds() - S.Began) * 1000.0);
    const FString Line = FString::Printf(TEXT("cells=%d reached=%d entrances=%s overlooks=%s traps=%d beyondEdge=%d gates=%s eyes=%s stairs=%s ramps=%s paths=%s ")
        TEXT("seeds=%d seedTraps=%d unseeded=%d home=%d out=%d todayTraps=%d todayBeyond=%d todayUnreached=%d holeExits=%d lowestReachedZ=%.1f at (%.0f,%.0f) ")
        TEXT("minWalkableZ=%.1f nodes=%d enclosed=%d slides=%d traces=%lld overlaps=%lld ms=%d terrain=%d baseline=%s"),
        Columns, S.Reached, *Count(TEXT("entrance")), *Count(TEXT("overlook")), Traps, Beyond, *Count(TEXT("gate")), *Count(TEXT("eye")), *Count(TEXT("stair")),
        *Count(TEXT("ramp")), *Count(TEXT("path")), S.Seeds.Num(), SeedTraps, S.Unseeded, S.HomeCount, S.OutCount, TrapsToday, BeyondToday, MissingToday,
        HoleExits, Lowest, LowestAt.X, LowestAt.Y, MinWalk, S.Z.Num(), S.Enclosed, S.Slides, S.Traces, S.Overlaps, Ms, int32(S.bTerrain), bBaseline ? *S.Baseline : TEXT("none"));
    UE_LOG(LogTemp, Display, TEXT("CALDER_REACH %s"), *Line);
    TArray<FString> Detail;
    S.Log.ParseIntoArrayLines(Detail);
    for (const FString& D : Detail) UE_LOG(LogTemp, Display, TEXT("CALDER_REACH_DETAIL %s"), *D);
    const FString Verdict = FString::Printf(TEXT("%s %s%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), *Line,
        S.Porch == INDEX_NONE ? TEXT(" (no standing room on the house's front porch to start from)") : !bLowOK ? TEXT(" (walkable ground under FallLimit + 400)") : TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("CALDER_REACH_REVIEW %s"), *Verdict);
    FFileHelper::SaveStringToFile(Verdict + TEXT("\n") + S.Log, *(S.Dir / TEXT("result.txt")));
    if (!bOK || !SharedRun()) FPlatformMisc::RequestExitWithStatus(false, bOK ? 0 : 1);
}

void Tick(UWorld* W, ELevelTick, float)
{
    if (!W || !W->IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("CalderReachReview"))) return;
    if (UGameplayStatics::GetCurrentLevelName(W, true) != TEXT("Chapter01_House")) return;
    FState& S = State();
    if (S.World.Get() != W)
    {
        S = FState();
        S.World = W;
        S.Start = W->GetTimeSeconds();
        S.Dir = FPaths::ProjectSavedDir() / TEXT("CalderReachReview");
        IFileManager::Get().Delete(*(S.Dir / TEXT("result.txt")), false, true, true);   // no stale verdict
    }
    if (S.Phase < 0) return;
    const double Until = FPlatformTime::Seconds() + SliceSeconds;

    if (S.Phase == 0)
    {
        // Wait for the exterior, the terrain's collision and the player's pawn (its capsule and movement rules are the walker's).
        AHouseExteriorDetail* Exterior = nullptr;
        for (TActorIterator<AHouseExteriorDetail> It(W); It; ++It) Exterior = *It;
        APlayerController* PC = W->GetFirstPlayerController();
        APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        if (!Exterior || !Pawn || W->GetTimeSeconds() - S.Start < 8.0 || !HouseCalderTerrain::CollisionReady(W)) return;
        S.Began = FPlatformTime::Seconds();
        S.Query = FCollisionQueryParams(SCENE_QUERY_STAT(CalderReach), false);
        S.bTerrain = HouseCalderTerrain::Enabled() && HouseCalderFeatures::Active();
        FParse::Value(FCommandLine::Get(), TEXT("CalderReachBaseline="), S.Baseline);
        if (UCharacterMovementComponent* M = Pawn->FindComponentByClass<UCharacterMovementComponent>())
        {
            S.Move = M;
            S.StepUp = M->MaxStepHeight;
        }
        if (const UCapsuleComponent* Cap = Pawn->FindComponentByClass<UCapsuleComponent>()) S.Radius = Cap->GetScaledCapsuleRadius() - RadiusSlack;
        // The walkers are not obstacles: every pawn (the players, the pigs) is left out of the queries.
        for (TActorIterator<APawn> It(W); It; ++It) S.Query.AddIgnoredActor(*It);
        TArray<UPrimitiveComponent*> Parts;
        Exterior->GetComponents(Parts);
        for (const UPrimitiveComponent* C : Parts)
            if (C->GetName().StartsWith(TEXT("CalderTerrainCollision_")) || C->GetName() == TEXT("ExteriorGrounds")) S.GroundParts.Add(C);
        S.First.SetNumZeroed(Columns + 1);
        S.Kind.SetNumZeroed(Columns);
        S.Z.Reserve(Columns + Columns / 4);
        S.Flags.Reserve(Columns + Columns / 4);
        S.Col.Reserve(Columns + Columns / 4);
        UE_LOG(LogTemp, Display, TEXT("CALDER_REACH start: %dx%d columns of %.0f cm, step %.0f, radius %.1f, headroom %.0f, walkable %.1f deg, terrain=%d"),
            Side, Side, Cell, S.StepUp, S.Radius, Headroom, S.Move.IsValid() ? S.Move->GetWalkableFloorAngle() : -1.f, int32(S.bTerrain));
        S.Phase = 1;
        return;
    }
    if (S.Phase == 1)
    {
        while (S.Scanned < Columns && ((S.Scanned & 63) != 0 || FPlatformTime::Seconds() < Until)) ScanColumn(W, S, S.Scanned++);
        if (S.Scanned < Columns) return;
        // The network starts on the house's front porch (or its front walk).
        for (const FVector P : { FVector(-36.65f, -1370.f, 0.f), FVector(0.f, -3000.f, -57.f) })
        {
            S.Porch = NodeNear(W, S, FVector2D(P), 100.f, float(P.Z) - 60.f, float(P.Z) + 60.f);
            if (S.Porch != INDEX_NONE) break;
        }
        BuildTargets(W, S);
        BuildSeeds(W, S);
        BuildSlides(W, S);
        UE_LOG(LogTemp, Display, TEXT("CALDER_REACH scanned %d columns, %d floors, %lld traces, %.1f s; %d targets, %d seeds"), Columns, S.Z.Num(), S.Traces,
            FPlatformTime::Seconds() - S.Began, S.Targets.Num(), S.Seeds.Num());
        S.Queue.Reset(); S.Head = 0;
        if (S.Porch != INDEX_NONE) { S.Flags[S.Porch] |= NReached; S.Reached = 1; S.Queue.Add(S.Porch); }
        S.Phase = 2;
        return;
    }
    if (S.Phase == 2)
    {
        if (!PumpForward(W, S, Until)) return;
        S.Queue.Reset(); S.Head = 0;
        if (S.Porch != INDEX_NONE) { S.Flags[S.Porch] |= NHome; S.HomeCount = 1; S.Queue.Add(S.Porch); }
        S.Phase = 3;
        return;
    }
    if (S.Phase == 3)
    {
        if (!PumpBackward(W, S, Until, NHome, S.HomeCount)) return;
        FindExits(W, S);
        S.Phase = 4;
        return;
    }
    if (S.Phase == 4)
    {
        if (!PumpBackward(W, S, Until, NOut, S.OutCount)) return;
        S.Phase = -1;
        Finish(W, S);
        // TODO(Phase 3, wildlife): the deer's reach checks (their tracks and beds reached from the network, none in a frozen
        // zone they may not enter) join this review with the wildlife director.
    }
}

struct FRegistration { FRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick); } };
FRegistration Registration;
}
#endif
