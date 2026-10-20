// Optional -CampusMPInterior=R1..R5 targets one Row house. Omit it (or pass LIB) for the unchanged library path.
// Row runs add real client-E openings from both sides, normal streamed free/reentry and a second replication check.
// -CampusMultiplayerReview -CampusMPSessionDir=<dir> (master job, after Phase 4): the campus with two real game processes,
// a listen host and a client, joined through the game's own lobby (SourceArt/CampusMaster/run_campus_multiplayer.py
// launches both). The host puts the client's player at Mercer Library's doors; then, through command/ack files in the
// session folder:
// - check: every campus door within 120 m of the client's player, and the clock tower's (always relevant), is on the client
//   with the host's locked reason and state; the client has built the library's inside round its own player; the
//   library's furniture (ACampusProp, server-spawned) is on the client, as many as the host has, each with its mesh;
// - open: the client presses E at the library's main doors (its own interaction, the owning client's RPC); the host sees
//   both leaves open, then the client does;
// - prop: the host moves one of the library's props; the client sees it there;
// - the terrain (Calder job): both log CAMPUS_MP_TERRAIN hash=; the host's hash travels with the check and the client's must match;
// - with -CampusMPWalk (Calder job, Saved/CalderWork/tools/campus_mp.py --walk): the host stands the client's player at the
//   start of -FreeRoamWalkReview's hill route (HouseFreeRoamCalderRoute) and, once it has settled there, logs
//   CAMPUS_MP_WALK_START; the client walks the whole route with its own real movement input (up the Senior Steps, over College
//   Hill, into the glen, round Gander Pond), the host checks its player arrived there too and logs CAMPUS_MP_WALK_END. Run with
//   -ExecCmds="p.NetShowCorrections 1": the launcher counts the host's "*** Server: Error for" lines between the two marks
//   (there must be none);
// - done: both log CAMPUS_MP_PASS and exit 0. Anything else logs CAMPUS_MP_FAIL with the reason and exits 1.
// TODO(Calder Phase 3, wildlife): the client sees the host's deer flee within 0.5 s (the deer's two-process test).
#include "HouseCampusInteriors.h"
#if !UE_BUILD_SHIPPING
#include "ExistingHouseDoor.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorSiteLayout.h"
#include "SeniorLobby.h"
#include "StoryCampaign.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"

// -FreeRoamWalkReview's Calder routes (HouseFreeRoamReview.cpp): 0 the hill route; empty with the Calder terrain off.
void HouseFreeRoamCalderRoute(int32 Which, TArray<FVector2D>& Out);

namespace HouseCampusMultiplayerImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;
    int32 Phase = 0;
    double Start = 0.0, Step = 0.0, Logged = 0.0;
    FString Last;
    TWeakObjectPtr<ACampusProp> Moved;
    bool bHashLogged = false;
    bool bRowComplete = false, bMovedPhysics = false, bMovedActive = false, bMovedRelevant = false;
    FTransform MovedFrom;
    int32 RowChecks = 0, RowProps = -1;
    uint8 RowProof = 0;
    // the client's walk along the hill route
    TArray<FVector2D> Route;
    int32 Waypoint = 0;
    float Best = 1e9f, StartZ = 0.f;
    double Progress = 0.0;
    double LastTick = 0.0;   // the walk's hitch log (diagnosis only)
};
FState& State() { static FState S; return S; }

const FString& SelectedId()
{
    static const FString Id=[]
    {
        FString Pick;
        FParse::Value(FCommandLine::Get(),TEXT("CampusMPInterior="),Pick);
        Pick=Pick.TrimStartAndEnd().ToUpper();
        return Pick.IsEmpty()?FString(TEXT("LIB")):Pick;
    }();
    return Id;
}
bool RowOn() { return SelectedId().Len()==2&&SelectedId()[0]==TEXT('R')&&SelectedId()[1]>=TEXT('1')&&SelectedId()[1]<=TEXT('5'); }
int32 SelectedSeed() { return HouseCampusInteriors::SeedFor(*SelectedId()); }
FString MainDoorId(const TCHAR* Leaf) { return FString::Printf(TEXT("Campus_%s_Main%s"),*SelectedId(),Leaf); }
// R5's porch is150cm deep: 170cm from its doorway centre lands on the lower tread, not the porch floor.
float OutsideDistance() { return SelectedId()==TEXT("R5")?115.f:170.f; }
bool RowFloorSupported(UWorld* W,const AStoryFirstPersonCharacter* P,float& FloorZ)
{
    const FVector Feet=P->GetActorLocation()-FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    FHitResult Hit;
    const bool Found=W->LineTraceSingleByChannel(Hit,Feet+FVector(0,0,10),Feet-FVector(0,0,40),ECC_Pawn,
        FCollisionQueryParams(SCENE_QUERY_STAT(CampusMPRowApproachFloor),true,P));
    FloorZ=Found?float(Hit.ImpactPoint.Z):MAX_flt;
    return Found&&Hit.ImpactNormal.Z>.70f&&FMath::Abs(FloorZ-HouseCampusInteriors::InteriorFloorZ(SelectedSeed(),0))<3.f&&FMath::Abs(Feet.Z-FloorZ-3.f)<3.f;
}
const FVector2D RowLeaveClient(7000.f,-4800.f),RowLeaveHost(7200.f,-4800.f);

/** Selected building's props by world bounds. Seed is server-only and cannot identify client props. */
TArray<ACampusProp*> SelectedProps(UWorld* W)
{
    TArray<ACampusProp*> Out;
    const FBox Box = HouseCampusInteriors::InteriorBox(SelectedSeed()).ExpandBy(50.f);
    for (TActorIterator<ACampusProp> It(W); It; ++It) if (Box.IsInsideOrOn(It->GetActorLocation())) Out.Add(*It);
    return Out;
}

ACampusDoor* FindDoor(UWorld* W, const FString& Id)
{
    for (TActorIterator<ACampusDoor> It(W); It; ++It) if (It->DoorId.ToString() == Id) return *It;
    return nullptr;
}

bool WalkOn() { return FParse::Param(FCommandLine::Get(), TEXT("CampusMPWalk")); }
constexpr double WalkBudget = 900.0;   // seconds more for the hill route

void Tick(UWorld* W, ELevelTick, float)
{
    if (!W || !W->IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("CampusMultiplayerReview"))) return;
    FString Dir;
    if (!FParse::Value(FCommandLine::Get(), TEXT("CampusMPSessionDir="), Dir)) return;
    FState& S = State();
    const double Now = FPlatformTime::Seconds();
    if (S.World.Get() != W) { S = FState(); S.World = W; S.Start = S.Step = Now; }
    if (S.Phase < 0) return;
    // Diagnosis only (no verdict reads it): a frame of 80 ms or more on either machine during a walk run, so a correction can be
    // told from a hitch (a long client frame draws a server correction wherever the walker is).
    if (WalkOn() && S.LastTick > 0.0 && Now - S.LastTick > .08)
        UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_HITCH mode=%d frameMs=%.0f"), int32(W->GetNetMode()), (Now - S.LastTick) * 1000.0);
    S.LastTick = Now;
    auto Fail = [&](const FString& Why)
    {
        UE_LOG(LogTemp, Error, TEXT("CAMPUS_MP_FAIL mode=%d %s"), int32(W->GetNetMode()), *Why);
        FFileHelper::SaveStringToFile(Why, *(Dir / TEXT("failed")));
        S.Phase = -1;
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };
    if (Now - S.Start > 300.0 + (WalkOn() ? WalkBudget : 0.0)) { Fail(FString::Printf(TEXT("timed out in phase %d"), S.Phase)); return; }

    if(SelectedId()!=TEXT("LIB")&&!RowOn()){Fail(TEXT("-CampusMPInterior accepts LIB or R1..R5"));return;}
    if(RowOn()&&FParse::Param(FCommandLine::Get(),TEXT("CampusInteriorsAll")))
    {Fail(TEXT("Row MP streaming requires ordinary proximity loading; remove -CampusInteriorsAll"));return;}

    // The lobby: the host readies both players and starts the story, the game's normal way in.
    const FString Map = UGameplayStatics::GetCurrentLevelName(W, true);
    if (Map == TEXT("Lobby"))
    {
        if (W->GetNetMode() != NM_ListenServer || S.Phase != 0) return;
        ASeniorLobbyGameState* Lobby = W->GetGameState<ASeniorLobbyGameState>();
        if (!Lobby || Lobby->GetMembers().Num() != 2) return;
        ASeniorLobbyController *Host = nullptr, *Remote = nullptr;
        for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
            if (ASeniorLobbyController* C = Cast<ASeniorLobbyController>(It->Get())) (C->IsLocalController() ? Host : Remote) = C;
        if (!Host || !Remote || !Remote->HasClientLoadedCurrentWorld()) return;
        Host->SetCharacter(0); Remote->SetCharacter(1); Host->SetReady(true); Remote->SetReady(true);
        if (!Lobby->CanStart() || !Host->CanStartStory()) return;
        S.Phase = 1;
        Host->StartStory(false);
        UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP two players readied in the lobby; story travel"));
        return;
    }
    if (Map != TEXT("Chapter01_House")) return;

    UCampusInteriorsSubsystem* Sub = W->GetSubsystem<UCampusInteriorsSubsystem>();
    AHouseCampusInteriors* Interiors = Sub ? Sub->GetInteriors() : nullptr;
    APlayerController *Local = nullptr, *Remote = nullptr;
    for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
        if (APlayerController* C = It->Get()) (C->IsLocalController() ? Local : Remote) = C;
    AStoryFirstPersonCharacter* Me = Local ? Cast<AStoryFirstPersonCharacter>(Local->GetPawn()) : nullptr;
    if (!Interiors || !Me) return;
    if(SelectedSeed()<0){Fail(TEXT("selected interior is absent: ")+SelectedId());return;}
    if (!S.bHashLogged && HouseCalderTerrain::CollisionReady(W))
    {
        // Every machine builds the same terrain: its lattice hash (CALDER_TERRAIN hash=) must match the host's.
        UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_TERRAIN mode=%d hash=%08x terrain=%d"), int32(W->GetNetMode()), HouseCalderTerrain::Hash(), int32(HouseCalderTerrain::Enabled()));
        S.bHashLogged = true;
    }
    auto Write = [&](const FString& Text) { FFileHelper::SaveStringToFile(Text, *(Dir / TEXT("command"))); S.Step = Now; };
    auto Acked = [&](const FString& Text) { FString Got; return FFileHelper::LoadFileToString(Got, *(Dir / TEXT("ack"))) && Got == Text; };

    // ---------------------------------------------------------------- the client
    if (W->GetNetMode() == NM_Client)
    {
        FString Command;
        if (!FFileHelper::LoadFileToString(Command, *(Dir / TEXT("command"))) || Command == S.Last) return;
        auto Ack = [&]() { FFileHelper::SaveStringToFile(Command, *(Dir / TEXT("ack"))); S.Last = Command; S.Step = Now; };
        if (Command == TEXT("check"))
        {
            FString Data;
            if (!FFileHelper::LoadFileToString(Data, *(Dir / TEXT("server.txt")))) return;
            TArray<FString> Rows;
            Data.ParseIntoArrayLines(Rows);
            int32 WantProps = -1, Doors = 0, Tower = 0;
            FString Missing;
            for (const FString& Row : Rows)
            {
                TArray<FString> F;
                Row.ParseIntoArray(F, TEXT("|"), false);
                if(F.Num()==2&&F[0]==TEXT("interior"))
                {
                    if(F[1]!=SelectedId()){Fail(TEXT("host/client -CampusMPInterior differs"));return;}
                    continue;
                }
                if (F.Num() == 2 && F[0] == TEXT("props")) { WantProps = FCString::Atoi(*F[1]); continue; }
                if (F.Num() == 2 && F[0] == TEXT("hash"))
                {
                    const FString Mine = FString::Printf(TEXT("%08x"), HouseCalderTerrain::Hash());
                    if (F[1] != Mine) { Fail(FString::Printf(TEXT("the terrain differs: the host's hash is %s, this machine's %s"), *F[1], *Mine)); return; }
                    continue;
                }
                if (F.Num() != 4) continue;
                FVector At;
                At.InitFromString(F[1]);
                const bool bTower = F[0].StartsWith(TEXT("Campus_TWR_"));
                if (!bTower && FVector::Dist2D(At, Me->GetActorLocation()) > 12000.f) continue;   // beyond the client's relevancy
                ACampusDoor* D = FindDoor(W, F[0]);
                if (!D || D->LockedReason != F[2] || D->IsOpen() != (F[3] == TEXT("1"))) { Missing = F[0] + (D ? TEXT(" (state)") : TEXT(" (absent)")); break; }
                ++Doors; Tower += bTower ? 1 : 0;
            }
            const TArray<ACampusProp*> Props = SelectedProps(W);
            int32 Meshed = 0;
            for (const ACampusProp* P : Props) Meshed += P->Body && P->Body->GetStaticMesh() ? 1 : 0;
            const bool bBuilt = Interiors->Loaded().Contains(SelectedSeed());
            if (Now - S.Logged > 3.0)
            {
                UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_CLIENT waiting: doors=%d tower=%d missing=%s built=%d props=%d/%d meshed=%d"), Doors, Tower, *Missing, int32(bBuilt), Props.Num(), WantProps, Meshed);
                S.Logged = Now;
            }
            // (And not before this machine's terrain collision is cooked round the player.)
            if (!Missing.IsEmpty() || !bBuilt || WantProps <= 0 || Props.Num() != WantProps || Meshed != Props.Num() || Tower != 2 || !HouseCalderTerrain::CollisionReady(W)) return;
            UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_CLIENT_CHECK %d nearby doors and the tower's 2 as on the host (reasons, states); the library built round this player; %d library props, every one with its mesh; terrain hash %08x as the host's"), Doors, Props.Num(), HouseCalderTerrain::Hash());
            if(RowOn())
            {
                ACampusDoor *A=FindDoor(W,MainDoorId(TEXT("A"))),*B=FindDoor(W,MainDoorId(TEXT("B")));
                if(!A||!B)return;
                const FVector Mid=(A->DoorwayCenter+B->DoorwayCenter)*.5f;
                const FVector Away=(Mid-HouseCampusInteriors::InteriorBox(SelectedSeed()).GetCenter()).GetSafeNormal2D();
                if(FVector::Dist2D(Me->GetActorLocation(),Mid+Away*OutsideDistance())>40.f)return;
                float Support=0;
                if(!RowFloorSupported(W,Me,Support))
                {if(Now-S.Step>8.f)Fail(TEXT("Row initial/reentry approach has no actual supporting porch floor"));return;}
                UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_CLIENT_FLOOR interior=%s stage=%s supported=1 supportZ=%.2f"),*SelectedId(),S.RowChecks==0?TEXT("initial"):TEXT("reentry"),Support);
                if(S.RowChecks==0)S.RowProps=WantProps;
                else if(WantProps!=S.RowProps){Fail(TEXT("Row reentry changed the authored prop count"));return;}
                ++S.RowChecks;
                if(S.RowChecks==2)S.RowProof|=8;
                UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_CLIENT_CHECK interior=%s pass=%d built=1 props=%d meshed=%d doors=%d"),*SelectedId(),S.RowChecks,Props.Num(),Meshed,Doors);
            }
            Ack();
            return;
        }
        if(Command==TEXT("row_inside_ready"))
        {
            ACampusDoor *A=FindDoor(W,MainDoorId(TEXT("A"))),*B=FindDoor(W,MainDoorId(TEXT("B")));
            if(!A||!B)return;
            const FVector Mid=(A->DoorwayCenter+B->DoorwayCenter)*.5f;
            const FVector Away=(Mid-HouseCampusInteriors::InteriorBox(SelectedSeed()).GetCenter()).GetSafeNormal2D();
            const float Feet=Me->GetActorLocation().Z-Me->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            if(FVector::Dist2D(Me->GetActorLocation(),Mid-Away*170.f)>40.f||
                FMath::Abs(Feet-HouseCampusInteriors::InteriorFloorZ(SelectedSeed(),0)-3.f)>10.f||
                !Interiors->Loaded().Contains(SelectedSeed())||A->IsOpen()||B->IsOpen())return;
            float Support=0;
            if(!RowFloorSupported(W,Me,Support))
            {if(Now-S.Step>8.f)Fail(TEXT("Row inside approach has no supporting floor at the intended height"));return;}
            UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_CLIENT_APPROACH interior=%s side=inside floor=%.2f supportZ=%.2f supported=1 closed=2"),*SelectedId(),Feet,Support);
            Ack();return;
        }
        if(Command==TEXT("row_stream_freed"))
        {
            if(FVector2D::Distance(FVector2D(Me->GetActorLocation()),RowLeaveClient)>40.f||
                Interiors->Loaded().Contains(SelectedSeed())||SelectedProps(W).Num()!=0)return;
            UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_CLIENT_FREED interior=%s built=0 props=0 naturalStreaming=1"),*SelectedId());
            S.RowProof|=4;Ack();return;
        }
        if (Command == TEXT("open")||Command==TEXT("row_open_inside"))
        {
            if (Now - S.Step < 1.0) return;
            ACampusDoor* A = FindDoor(W, MainDoorId(TEXT("A")));
            if (!A) { Fail(TEXT("the library's main doors are not on the client")); return; }
            ACampusDoor* B = FindDoor(W, MainDoorId(TEXT("B")));
            const FVector Mid = B ? (A->DoorwayCenter + B->DoorwayCenter) * .5f : A->DoorwayCenter;
            if(RowOn()&&Command==TEXT("open"))
            {
                if(!B)return;
                const FVector Away=(Mid-HouseCampusInteriors::InteriorBox(SelectedSeed()).GetCenter()).GetSafeNormal2D();
                const float Feet=Me->GetActorLocation().Z-Me->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                if(FVector::Dist2D(Me->GetActorLocation(),Mid+Away*OutsideDistance())>40.f||
                    FMath::Abs(Feet-HouseCampusInteriors::InteriorFloorZ(SelectedSeed(),0)-3.f)>10.f||A->IsOpen()||B->IsOpen())return;
                float Support=0;
                if(!RowFloorSupported(W,Me,Support))
                {if(Now-S.Step>8.f)Fail(TEXT("Row outside approach has no supporting porch floor at the intended height"));return;}
                UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_CLIENT_APPROACH interior=%s side=outside floor=%.2f supportZ=%.2f supported=1 closed=2"),*SelectedId(),Feet,Support);
            }
            Local->SetControlRotation((Mid - Me->FirstPersonCamera->GetComponentLocation()).Rotation());
            AExistingHouseDoor* Focus = Me->FocusedDoorForInteraction();
            if (!Focus || Focus->DoorGroup != A->DoorGroup) { if (Now - S.Step > 8.0) Fail(TEXT("the client cannot focus the library's main doors")); return; }
            Local->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, IE_Pressed, 1.f));
            Local->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, IE_Released, 0.f));
            UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_CLIENT_E at the library's main doors"));
            if(RowOn())UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_CLIENT_E interior=%s side=%s owningClientRPC=1"),*SelectedId(),Command==TEXT("open")?TEXT("outside"):TEXT("inside"));
            Ack();
            return;
        }
        if (Command == TEXT("opened")||Command==TEXT("row_inside_opened"))
        {
            ACampusDoor *A = FindDoor(W, MainDoorId(TEXT("A"))), *B = FindDoor(W, MainDoorId(TEXT("B")));
            if (!A || !B || !A->IsOpen() || !B->IsOpen()) return;
            UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_CLIENT_OPEN both leaves open here too"));
            if(RowOn())
            {
                S.RowProof|=Command==TEXT("opened")?1:2;
                UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_CLIENT_OPEN interior=%s side=%s leaves=2"),*SelectedId(),Command==TEXT("opened")?TEXT("outside"):TEXT("inside"));
            }
            Ack();
            return;
        }
        if (Command.StartsWith(TEXT("prop=")))
        {
            FVector At;
            At.InitFromString(Command.RightChop(5));
            for (TActorIterator<ACampusProp> It(W); It; ++It)
                if (FVector::Dist(It->GetActorLocation(), At) < 5.f)
                {
                    UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_CLIENT_PROP the moved prop is here at %s"), *It->GetActorLocation().ToCompactString());
                    Ack();
                    return;
                }
            return;
        }
        if (Command == TEXT("walkready"))
        {
            // Stood at the hill route's start by the host: wait until this machine has its player there, walking and still.
            if (S.Route.Num() == 0) HouseFreeRoamCalderRoute(0, S.Route);
            if (S.Route.Num() < 2) { Fail(TEXT("no hill route on the client (is the Calder terrain on?)")); return; }
            const UCharacterMovementComponent* Move = Me->GetCharacterMovement();
            const float Half = Me->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            if (FVector2D::Distance(FVector2D(Me->GetActorLocation()), S.Route[0]) > 150.f || !Move->IsMovingOnGround() || Move->Velocity.Size2D() > 5.f) return;
            S.StartZ = float(Me->GetActorLocation().Z) - Half;
            UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_CLIENT_WALK at the hill route's start %s, %d waypoints"), *Me->GetActorLocation().ToCompactString(), S.Route.Num());
            Ack();
            return;
        }
        if (Command == TEXT("walk"))
        {
            // The client's own walk with real input, as -FreeRoamWalkReview walks it: within 70 cm of each waypoint in turn; a fall is
            // the feet 3 m under the start carried over the ground, a stall 8 s without 30 cm of progress.
            if (S.Waypoint == 0) { S.Waypoint = 1; S.Best = 1e9f; S.Progress = Now; }
            const float Half = Me->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            const FVector Feet = Me->GetActorLocation() - FVector(0.f, 0.f, Half);
            const FVector2D Goal = S.Route[S.Waypoint];
            const float Dist = float(FVector2D::Distance(FVector2D(Feet), Goal));
            if (Feet.Z < S.StartZ - 300.f + (HouseExteriorSite::GroundHeight(float(Feet.X), float(Feet.Y)) - HouseExteriorSite::GroundHeight(float(S.Route[0].X), float(S.Route[0].Y))))
            {
                Fail(FString::Printf(TEXT("the client's walk fell on the way to waypoint %d (%.0f,%.0f), feet at %s"), S.Waypoint, Goal.X, Goal.Y, *Feet.ToCompactString()));
                return;
            }
            if (Dist < S.Best - 30.f) { S.Best = Dist; S.Progress = Now; }
            if (Dist < 70.f)
            {
                if (S.Waypoint % 10 == 0) UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_CLIENT_WALK reached %d/%d at %s"), S.Waypoint, S.Route.Num() - 1, *Feet.ToCompactString());
                if (S.Waypoint + 1 >= S.Route.Num())
                {
                    UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_CLIENT_WALKED the hill route, %d waypoints, feet at %s"), S.Route.Num() - 1, *Feet.ToCompactString());
                    Ack();
                    return;
                }
                ++S.Waypoint; S.Best = 1e9f; S.Progress = Now;
                return;
            }
            if (Now - S.Progress > 8.0)
            {
                FHitResult Hit;
                const FVector Ahead = FVector(Goal - FVector2D(Feet), 0.f).GetSafeNormal();
                const bool bHit = W->SweepSingleByChannel(Hit, Me->GetActorLocation(), Me->GetActorLocation() + Ahead * 120.f, FQuat::Identity, ECC_Pawn,
                    FCollisionShape::MakeCapsule(Me->GetCapsuleComponent()->GetScaledCapsuleRadius(), Half - 5.f), FCollisionQueryParams(SCENE_QUERY_STAT(CampusMPWalkBlocker), false, Me));
                Fail(FString::Printf(TEXT("the client's walk stalled on the way to waypoint %d (%.0f,%.0f) at %s, %.0f cm short, blocked by %s/%s"), S.Waypoint, Goal.X, Goal.Y,
                    *Feet.ToCompactString(), Dist, bHit ? *GetNameSafe(Hit.GetActor()) : TEXT("nothing"), bHit ? *GetNameSafe(Hit.GetComponent()) : TEXT("nothing")));
                return;
            }
            Local->SetControlRotation(FVector(Goal - FVector2D(Feet), 0.f).Rotation());
            Me->AddMovementInput(FVector(Goal - FVector2D(Feet), 0.f).GetSafeNormal(), 1.f);
            return;
        }
        if (Command == TEXT("done"))
        {
            if(RowOn())
            {
                if(S.RowChecks!=2||S.RowProof!=15){Fail(TEXT("Row MP requires both client openings, free and reentry checks"));return;}
                UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_PASS client interior=%s outsideAndInsideRPC=1 freeAndReentry=1 checks=2"),*SelectedId());
            }
            UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_PASS client: doors (with reasons and states) and the tower's, the library built round this player, its props with their meshes, E at its doors, a moved prop%s"),
                WalkOn() ? TEXT(", the hill route walked") : TEXT(""));
            S.Phase = -1;
            FPlatformMisc::RequestExitWithStatus(false, 0);
        }
        return;
    }

    // ---------------------------------------------------------------- the host
    if (W->GetNetMode() != NM_ListenServer) return;
    if (S.Phase == 9)
    {
        if (Now - S.Step < 3.0) return;
        if(RowOn())
        {
            if(!S.bRowComplete){Fail(TEXT("Row MP sequence did not finish"));return;}
            UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_PASS host interior=%s outsideAndInsideRPC=1 freeAndReentry=1"),*SelectedId());
        }
        UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_PASS host: the client's doors, inside and props matched, its E opened the library's doors, a moved prop replicated, terrain hash %08x%s"),
            HouseCalderTerrain::Hash(), WalkOn() ? TEXT(", the client walked the hill route") : TEXT(""));
        S.Phase = -1;
        FPlatformMisc::RequestExitWithStatus(false, 0);
        return;
    }
    AStoryFirstPersonCharacter* Them = Remote ? Cast<AStoryFirstPersonCharacter>(Remote->GetPawn()) : nullptr;
    if (!Them) return;
    ACampusDoor* MainA = FindDoor(W, MainDoorId(TEXT("A")));
    ACampusDoor* MainB = FindDoor(W, MainDoorId(TEXT("B")));
    if (!MainA || !MainB) return;
    if (S.Phase <= 1)
    {
        if (Now - S.Start < 12.0 || !HouseCalderTerrain::CollisionReady(W)) return;
        // The client's player 1.7 m outside Mercer's main double door, facing it (door focus reaches 2.7 m from the eye), at the
        // library's floor on its pad.
        const FVector Mid = (MainA->DoorwayCenter + MainB->DoorwayCenter) * .5f;
        const FVector Away = (Mid - HouseCampusInteriors::InteriorBox(SelectedSeed()).GetCenter()).GetSafeNormal2D();
        const FVector Out = Mid + Away * OutsideDistance();
        Them->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        Them->SetActorLocation(FVector(Out.X, Out.Y, (RowOn()?HouseCampusInteriors::InteriorFloorZ(SelectedSeed(),0):-60.f+HouseCampusInteriors::BaseDZ(SelectedSeed())) + Them->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3.f), false, nullptr, ETeleportType::TeleportPhysics);
        Remote->SetControlRotation((Mid - Them->GetActorLocation()).Rotation());
        Them->ForceNetUpdate();
        if(RowOn())
        {
            // R5 is authored propped open. Setup closes it once, then the actual opening must come from the client RPC.
            if(MainA->IsOpen()&&!MainA->Interact(Them)){Fail(TEXT("cannot reset Row main pair from outside"));return;}
            UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_HOST_APPROACH interior=%s side=outside floor=%.2f"),*SelectedId(),HouseCampusInteriors::InteriorFloorZ(SelectedSeed(),0));
        }
        UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_HOST client's player at the library's doors (%s)"), *Them->GetActorLocation().ToCompactString());
        S.Phase = 2; S.Step = Now;
        return;
    }
    if (S.Phase == 2||S.Phase==24)
    {
        const TArray<ACampusProp*> Props = SelectedProps(W);
        if (!Interiors->Loaded().Contains(SelectedSeed()) || Props.Num() == 0 || Now - S.Step < 4.0) return;
        FString Data = FString::Printf(TEXT("props|%d\nhash|%08x\n"), Props.Num(), HouseCalderTerrain::Hash());
        if(RowOn())
        {
            if(S.Phase==2)S.RowProps=Props.Num();
            else if(Props.Num()!=S.RowProps){Fail(TEXT("Row reentry changed the host authored prop count"));return;}
            Data+=TEXT("interior|")+SelectedId()+TEXT("\n");
        }
        int32 Doors = 0;
        for (TActorIterator<ACampusDoor> It(W); It; ++It, ++Doors)
            Data += FString::Printf(TEXT("%s|%s|%s|%d\n"), *It->DoorId.ToString(), *It->GetActorLocation().ToString(), *It->LockedReason, int32(It->IsOpen()));
        FFileHelper::SaveStringToFile(Data, *(Dir / TEXT("server.txt")));
        UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_HOST doors=%d libraryProps=%d"), Doors, Props.Num());
        Write(TEXT("check"));
        S.Phase = S.Phase==24?25:3;
        return;
    }
    if (S.Phase == 3 && Acked(TEXT("check"))) { Write(TEXT("open")); S.Phase = 4; return; }
    if (S.Phase == 4 && Acked(TEXT("open")))
    {
        if (!MainA->IsOpen() || !MainB->IsOpen()) { if (Now - S.Step > 10.0) Fail(TEXT("the client's E did not open the library's doors on the host")); return; }
        UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_HOST the client's E opened both leaves"));
        if(RowOn())UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_HOST_OPEN interior=%s side=outside leaves=2"),*SelectedId());
        Write(TEXT("opened"));
        S.Phase = 5;
        return;
    }
    if (S.Phase == 5 && Acked(TEXT("opened")))
    {
        // Move one of the library's props (held still, as the door test holds its chair) and tell the client where.
        TArray<ACampusProp*> Props = SelectedProps(W);
        if (Props.Num() == 0) { Fail(TEXT("no library prop to move")); return; }
        ACampusProp* P = Props[0];
        if(RowOn())
        {
            S.MovedFrom=P->GetActorTransform();S.bMovedPhysics=P->Body->IsSimulatingPhysics();
            S.bMovedActive=P->bPhysicsActive;S.bMovedRelevant=P->bAlwaysRelevant;
        }
        P->Body->SetSimulatePhysics(false);
        P->bPhysicsActive = false;
        P->bAlwaysRelevant = true;
        P->SetActorLocation(P->GetActorLocation() + FVector(0.f, 0.f, 150.f), false, nullptr, ETeleportType::TeleportPhysics);
        P->FlushNetDormancy();
        P->ForceNetUpdate();
        S.Moved = P;
        Write(TEXT("prop=") + P->GetActorLocation().ToString());
        S.Last = TEXT("prop=") + P->GetActorLocation().ToString();
        S.Phase = 6;
        return;
    }
    if(S.Phase==20&&Acked(TEXT("row_inside_ready"))){Write(TEXT("row_open_inside"));S.Phase=21;return;}
    if(S.Phase==21&&Acked(TEXT("row_open_inside")))
    {
        if(!MainA->IsOpen()||!MainB->IsOpen())
        {if(Now-S.Step>10.f)Fail(TEXT("client inside E did not open the Row pair on host"));return;}
        UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_HOST_OPEN interior=%s side=inside leaves=2"),*SelectedId());
        Write(TEXT("row_inside_opened"));S.Phase=22;return;
    }
    if(S.Phase==22&&Acked(TEXT("row_inside_opened")))
    {
        // Move BOTH players beyond ordinary FreeRange; no forced Free()/BuildNow(), culling overrides or lowered timers.
        for(AStoryFirstPersonCharacter* P:{Me,Them})
        {
            const FVector2D Away=P==Me?RowLeaveHost:RowLeaveClient;
            P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
            P->SetActorLocation(FVector(Away,HouseExteriorSite::GroundHeight(Away.X,Away.Y)+P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3.f),false,nullptr,ETeleportType::TeleportPhysics);
            P->ForceNetUpdate();
        }
        UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_HOST_LEAVE interior=%s bothPlayersAway=1 naturalStreaming=1"),*SelectedId());
        Write(TEXT("row_stream_freed"));S.Phase=23;return;
    }
    if(S.Phase==23)
    {
        if(Now-S.Step>45.f){Fail(TEXT("Row interior/props did not naturally free on both peers"));return;}
        if(!Acked(TEXT("row_stream_freed"))||Interiors->Loaded().Contains(SelectedSeed())||SelectedProps(W).Num()!=0)return;
        UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_HOST_FREED interior=%s built=0 props=0 clientConfirmed=1"),*SelectedId());
        const FVector Mid=(MainA->DoorwayCenter+MainB->DoorwayCenter)*.5f;
        const FVector Away=(Mid-HouseCampusInteriors::InteriorBox(SelectedSeed()).GetCenter()).GetSafeNormal2D();
        const FVector Outside=Mid+Away*OutsideDistance();
        Them->SetActorLocation(FVector(Outside.X,Outside.Y,HouseCampusInteriors::InteriorFloorZ(SelectedSeed(),0)+Them->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3.f),false,nullptr,ETeleportType::TeleportPhysics);
        Them->ForceNetUpdate();S.Step=Now;S.Phase=24;return;
    }
    if(S.Phase==25&&Acked(TEXT("check")))
    {
        UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_HOST_REENTRY interior=%s rebuilt=1 clientPropsAndDoorsMatch=1"),*SelectedId());
        S.bRowComplete=true;S.Last=TEXT("check");S.Phase=6;return;
    }
    if (S.Phase == 6 && Acked(S.Last))
    {
        if(RowOn()&&!S.bRowComplete)
        {
            ACampusProp* P=S.Moved.Get();
            if(!P||!P->Body){Fail(TEXT("moved Row prop disappeared before restoration"));return;}
            P->Body->SetSimulatePhysics(false);P->SetActorTransform(S.MovedFrom,false,nullptr,ETeleportType::TeleportPhysics);
            P->bPhysicsActive=S.bMovedActive;P->bAlwaysRelevant=S.bMovedRelevant;P->Body->SetSimulatePhysics(S.bMovedPhysics);
            P->FlushNetDormancy();P->ForceNetUpdate();
            const FVector Mid=(MainA->DoorwayCenter+MainB->DoorwayCenter)*.5f;
            const FVector Away=(Mid-HouseCampusInteriors::InteriorBox(SelectedSeed()).GetCenter()).GetSafeNormal2D();
            const FVector Inside=Mid-Away*170.f;
            Them->GetCharacterMovement()->StopMovementImmediately();Them->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
            Them->SetActorLocation(FVector(Inside.X,Inside.Y,HouseCampusInteriors::InteriorFloorZ(SelectedSeed(),0)+Them->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3.f),false,nullptr,ETeleportType::TeleportPhysics);
            Them->ForceNetUpdate();
            if(!MainA->IsOpen()||!MainB->IsOpen()||!MainA->Interact(Them))
            {Fail(TEXT("cannot reset Row main pair from interior approach"));return;}
            UE_LOG(LogTemp,Display,TEXT("CAMPUS_MP_ROW_HOST_APPROACH interior=%s side=inside restoredMovedProp=1"),*SelectedId());
            Write(TEXT("row_inside_ready"));S.Phase=20;return;
        }
        if (!WalkOn()) { Write(TEXT("done")); S.Phase = 9; return; }
        // The hill route: stand the client's player at its start, walking (the correction this teleport sends is before the
        // walk's start mark).
        HouseFreeRoamCalderRoute(0, S.Route);
        if (S.Route.Num() < 2) { Fail(TEXT("-CampusMPWalk: no hill route (is the Calder terrain on?)")); return; }
        const float Half = Them->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const float GroundAt = HouseExteriorSite::GroundHeight(float(S.Route[0].X), float(S.Route[0].Y));
        FHitResult Ground;
        const bool bGround = W->LineTraceSingleByChannel(Ground, FVector(S.Route[0], GroundAt + 2060.f), FVector(S.Route[0], GroundAt - 2940.f), ECC_Pawn,
            FCollisionQueryParams(SCENE_QUERY_STAT(CampusMPWalkGround), true, Them));
        Them->GetCharacterMovement()->StopMovementImmediately();
        Them->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Them->SetActorLocation(FVector(S.Route[0], (bGround ? float(Ground.ImpactPoint.Z) : GroundAt) + Half + 3.f), false, nullptr, ETeleportType::TeleportPhysics);
        Them->ForceNetUpdate();
        UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_HOST client's player at the hill route's start (%s), %d waypoints"), *Them->GetActorLocation().ToCompactString(), S.Route.Num());
        Write(TEXT("walkready"));
        S.Phase = 10;
        return;
    }
    if (S.Phase == 10 && Acked(TEXT("walkready"))) { S.Phase = 11; S.Step = Now; return; }
    if (S.Phase == 11 && Now - S.Step > 2.0)
    {
        UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_WALK_START the client walks the hill route (p.NetShowCorrections counts from here)"));
        Write(TEXT("walk"));
        S.Phase = 12;
        return;
    }
    if (S.Phase == 12 && Acked(TEXT("walk")))
    {
        // The server's own idea of where the client's player is must agree that it got there.
        const float Off = float(FVector2D::Distance(FVector2D(Them->GetActorLocation()), S.Route.Last()));
        UE_LOG(LogTemp, Display, TEXT("CAMPUS_MP_WALK_END the client walked the hill route; on the host its player is %.0f cm from the end"), Off);
        if (Off > 200.f) { Fail(FString::Printf(TEXT("the client says it walked the hill route, but on the host its player is %.0f cm from the end"), Off)); return; }
        Write(TEXT("done"));
        S.Phase = 9;
        return;
    }
}

struct FRegistration { FRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick); } };
FRegistration Registration;
}
#endif
