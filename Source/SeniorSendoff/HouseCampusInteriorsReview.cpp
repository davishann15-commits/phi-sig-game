// -CampusInteriorsReview[=LIB,HAR,...]: walks the player, with real movement input, through every walk-in building (or
// the ones named) and back out, one after another. In through the main doors (opened with the same Interact the E key uses), round the
// circulation desk, into the reading room, up the stairs to the stacks, to the locked staff doors (they must stay shut
// and give their reason), and out again. It checks:
// - no stall and no fall, and each stop is reached on the floor it belongs to;
// - the building's inside is built while the walker is near and freed after everyone has gone;
// - the indoor test (exposure, film grade, bell ducking) reads "inside" in the rooms and "outside" on the forecourt;
// - the pooled interior lights (at most 6) are on inside and off outside.
// Logs CAMPUS_INTERIORS_REVIEW PASS/FAIL, writes Saved/CampusInteriorsReview/result.txt and screenshots of the rooms.
#include "HouseCampusInteriors.h"
#include "HouseFraternityRow.h"
#if !UE_BUILD_SHIPPING
#include "HouseCampusLayout.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseExterior.h"
#include "StoryCampaign.h"
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

namespace HouseCampusInteriorsReviewImpl
{
enum class EStep : uint8 { Start, Walk, Open, Locked, Leave };
/** One stop of a building's route, in its canonical frame (main doors on +Y). Floor is the floor the stop is on (-1: a stair). */
struct FStop { EStep Kind; FVector2D At; int32 Floor; const TCHAR* Name; const TCHAR* Door; const TCHAR* Shot; float LookYaw = 0.f; };
/** Each walk-in building's route (its stops in the building's canonical frame). */
struct FBuildingRoute { const TCHAR* Id; TArray<FStop> Stops; };
const TArray<FStop>& LibraryRoute()
{
    static const TArray<FStop> R = {
        { EStep::Start, { 0.f, 2300.f }, 0, TEXT("the library forecourt"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 1880.f }, 0, TEXT("at the main doors"), nullptr, TEXT("LIB_1_Doors.png"), -90.f },
        { EStep::Open, { 0.f, 1880.f }, 0, TEXT("open the main doors"), TEXT("MainA"), nullptr },
        { EStep::Walk, { 0.f, 1450.f }, 0, TEXT("through the doors into the lobby"), nullptr, TEXT("LIB_2_Lobby.png"), -90.f },
        { EStep::Walk, { 900.f, 850.f }, 0, TEXT("into the card catalog hall"), nullptr, TEXT("LIB_3_Catalog.png"), 0.f },
        { EStep::Walk, { -530.f, 750.f }, 0, TEXT("past the circulation desk"), nullptr, nullptr },
        { EStep::Walk, { -530.f, -150.f }, 0, TEXT("behind the desk"), nullptr, nullptr },
        { EStep::Walk, { -1100.f, -100.f }, 0, TEXT("into the reference reading room"), nullptr, TEXT("LIB_4_Reading.png"), 180.f },
        { EStep::Walk, { -530.f, -150.f }, 0, TEXT("back to the circulation area"), nullptr, nullptr },
        { EStep::Walk, { 0.f, -150.f }, 0, TEXT("the corridor between the stairs"), nullptr, nullptr },
        { EStep::Walk, { 0.f, -880.f }, 0, TEXT("at the staff workroom"), nullptr, nullptr },
        { EStep::Locked, { 0.f, -880.f }, 0, TEXT("try the workroom door"), TEXT("Workroom"), nullptr },
        { EStep::Walk, { 0.f, -150.f }, 0, TEXT("back out of the corridor"), nullptr, nullptr },
        { EStep::Walk, { 880.f, -100.f }, 0, TEXT("into the carrels"), nullptr, TEXT("LIB_5_Carrels.png"), -20.f },
        { EStep::Walk, { 0.f, -150.f }, 0, TEXT("back to the circulation area"), nullptr, nullptr },
        { EStep::Walk, { -350.f, -180.f }, 0, TEXT("the foot of the stairs"), nullptr, nullptr },
        { EStep::Walk, { -350.f, -1090.f }, 1, TEXT("up the stairs to the stacks"), nullptr, nullptr },
        { EStep::Walk, { -350.f, -1220.f }, 1, TEXT("the stacks landing"), nullptr, nullptr },
        { EStep::Walk, { 0.f, -1250.f }, 1, TEXT("at the copy room"), nullptr, nullptr },
        { EStep::Locked, { 0.f, -1250.f }, 1, TEXT("try the copy room door"), TEXT("CopyRoom"), nullptr },
        { EStep::Walk, { -500.f, -1080.f }, 1, TEXT("toward the west stacks"), nullptr, nullptr },
        { EStep::Walk, { -1100.f, -1080.f }, 1, TEXT("along the back of the stacks"), nullptr, nullptr },
        { EStep::Walk, { -1100.f, 160.f }, 1, TEXT("the stacks' main aisle"), nullptr, TEXT("LIB_6_Stacks.png"), 180.f },
        { EStep::Walk, { -700.f, -1150.f }, 1, TEXT("back past the end of the ranges"), nullptr, nullptr },
        { EStep::Walk, { -350.f, -1150.f }, 1, TEXT("the head of the stairs"), nullptr, nullptr },
        { EStep::Walk, { -350.f, -180.f }, 0, TEXT("down the stairs"), nullptr, nullptr },
        { EStep::Walk, { -530.f, -150.f }, 0, TEXT("round the desk"), nullptr, nullptr },
        { EStep::Walk, { -530.f, 750.f }, 0, TEXT("the lobby side"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 1450.f }, 0, TEXT("the lobby"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 2300.f }, 0, TEXT("out through the doors"), nullptr, nullptr },
        { EStep::Leave, { 0.f, 0.f }, 0, TEXT("away to the house; the inside must be freed"), nullptr, nullptr },
    };
    return R;
}
/** Into a furnished dorm room and out again: from the corridor (B -100) through the doorway's clear width (A 70), round the
 *  desk chairs and on to the rug (A 150). A and B are the room's own frame (HouseCampusInteriorsHarlow.cpp): A from its west
 *  wall at XL, B in from its door wall at YDoor along Dir. */
void HarlowRoomStops(TArray<FStop>& R, float XL, float YDoor, float Dir, int32 Floor, const TCHAR* Name, const TCHAR* Shot)
{
    auto P = [&](float A, float B) { return FVector2D(XL + A, YDoor + Dir * B); };
    R.Add({ EStep::Walk, P(70.f, -100.f), Floor, TEXT("at the door"), nullptr, nullptr });
    R.Add({ EStep::Walk, P(70.f, 45.f), Floor, TEXT("in the doorway"), nullptr, nullptr });
    R.Add({ EStep::Walk, P(150.f, 100.f), Floor, TEXT("past the desk chairs"), nullptr, nullptr });
    R.Add({ EStep::Walk, P(150.f, 260.f), Floor, Name, nullptr, Shot, Dir > 0.f ? 60.f : -60.f });
    R.Add({ EStep::Walk, P(150.f, 100.f), Floor, TEXT("back toward the door"), nullptr, nullptr });
    R.Add({ EStep::Walk, P(70.f, 45.f), Floor, TEXT("the doorway"), nullptr, nullptr });
    R.Add({ EStep::Walk, P(70.f, -100.f), Floor, TEXT("out to the corridor"), nullptr, nullptr });
}
const TArray<FStop>& HarlowRoute()
{
    static const TArray<FStop> R = []
    {
        TArray<FStop> Out = {
            { EStep::Start, { 0.f, 1700.f }, 0, TEXT("the Harlow Hall front walk"), nullptr, nullptr },
            { EStep::Walk, { 0.f, 1260.f }, 0, TEXT("at the main doors"), nullptr, TEXT("HAR_1_Doors.png"), -90.f },
            { EStep::Open, { 0.f, 1260.f }, 0, TEXT("open the main doors"), TEXT("MainA"), nullptr },
            { EStep::Walk, { 0.f, 900.f }, 0, TEXT("into the lobby"), nullptr, TEXT("HAR_2_Lobby.png"), 160.f },
            { EStep::Walk, { 0.f, 525.f }, 0, TEXT("the front corridor"), nullptr, nullptr },
            { EStep::Walk, { 880.f, 525.f }, 0, TEXT("at the TV lounge"), nullptr, nullptr },
            { EStep::Walk, { 880.f, 800.f }, 0, TEXT("into the TV lounge"), nullptr, TEXT("HAR_3_Lounge.png"), 10.f },
            { EStep::Walk, { 880.f, 560.f }, 0, TEXT("back to the corridor"), nullptr, nullptr },
            { EStep::Walk, { 1483.f, 560.f }, 0, TEXT("at room 107"), nullptr, nullptr },
            { EStep::Locked, { 1483.f, 560.f }, 0, TEXT("try room 107's door"), TEXT("Room107"), nullptr },
        };
        HarlowRoomStops(Out, -1105.f, 657.5f, 1.f, 0, TEXT("in room 105"), TEXT("HAR_4_Room105.png"));
        Out.Append({
            { EStep::Walk, { -1600.f, 525.f }, 0, TEXT("the west end of the front corridor"), nullptr, nullptr },
            { EStep::Walk, { -1600.f, 0.f }, 0, TEXT("the west commons"), nullptr, TEXT("HAR_5_Commons.png"), 0.f },
            { EStep::Walk, { -1600.f, -555.f }, 0, TEXT("the back corridor"), nullptr, nullptr },
            { EStep::Walk, { -1380.f, -555.f }, 0, TEXT("the foot of the west stair"), nullptr, nullptr },
            { EStep::Walk, { -1380.f, -405.f }, 0, TEXT("at the bottom step"), nullptr, nullptr },
            { EStep::Walk, { -1380.f, 340.f }, 1, TEXT("up the west stair"), nullptr, nullptr },
            { EStep::Walk, { -1380.f, 525.f }, 1, TEXT("the front corridor upstairs"), nullptr, nullptr },
        });
        HarlowRoomStops(Out, -1105.f, 657.5f, 1.f, 1, TEXT("in room 205"), TEXT("HAR_6_Room205.png"));
        Out.Append({
            { EStep::Walk, { -1194.f, 525.f }, 1, TEXT("at the bathroom door"), nullptr, nullptr },
            { EStep::Walk, { -1194.f, 330.f }, 1, TEXT("inside the bathroom door"), nullptr, nullptr },
            { EStep::Walk, { -1040.f, 300.f }, 1, TEXT("round the privacy wall"), nullptr, nullptr },
            { EStep::Walk, { -1000.f, 0.f }, 1, TEXT("in the bathroom"), nullptr, TEXT("HAR_7_Bath.png"), -60.f },
            { EStep::Walk, { -1040.f, 300.f }, 1, TEXT("back past the sinks"), nullptr, nullptr },
            { EStep::Walk, { -1194.f, 330.f }, 1, TEXT("the bathroom door"), nullptr, nullptr },
            { EStep::Walk, { -1194.f, 525.f }, 1, TEXT("out to the corridor"), nullptr, nullptr },
            { EStep::Walk, { -1600.f, 525.f }, 1, TEXT("the west end upstairs"), nullptr, nullptr },
            { EStep::Walk, { -1600.f, -555.f }, 1, TEXT("the back corridor upstairs"), nullptr, nullptr },
        });
        HarlowRoomStops(Out, -1870.f, -687.5f, -1.f, 1, TEXT("in room 202"), TEXT("HAR_8_Room202.png"));
        HarlowRoomStops(Out, -385.f, -687.5f, -1.f, 1, TEXT("in room 210"), nullptr);
        Out.Append({
            { EStep::Walk, { 1380.f, -555.f }, 1, TEXT("the head of the east stair"), nullptr, nullptr },
            { EStep::Walk, { 1380.f, -385.f }, 1, TEXT("the east stair's landing"), nullptr, nullptr },
            { EStep::Walk, { 1380.f, 372.f }, 0, TEXT("down the east stair"), nullptr, nullptr },
            { EStep::Walk, { 1380.f, 525.f }, 0, TEXT("the front corridor"), nullptr, nullptr },
            { EStep::Walk, { 166.f, 525.f }, 0, TEXT("at the laundry"), nullptr, nullptr },
            { EStep::Walk, { 166.f, 250.f }, 0, TEXT("in the laundry"), nullptr, TEXT("HAR_9_Laundry.png"), -100.f },
            { EStep::Walk, { 166.f, 525.f }, 0, TEXT("back out of the laundry"), nullptr, nullptr },
            { EStep::Walk, { 0.f, 525.f }, 0, TEXT("the corridor at the lobby"), nullptr, nullptr },
            { EStep::Walk, { 0.f, 900.f }, 0, TEXT("back through the lobby"), nullptr, nullptr },
            { EStep::Walk, { 0.f, 1700.f }, 0, TEXT("out through the doors"), nullptr, nullptr },
            { EStep::Leave, { 0.f, 0.f }, 0, TEXT("away to the house; the inside must be freed"), nullptr, nullptr },
        });
        return Out;
    }();
    return R;
}

const TArray<FStop>& ScienceRoute()
{
    static const TArray<FStop> R = {
        { EStep::Start, { 0.f, 2200.f }, 0, TEXT("the Hollis Hall front walk"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 1690.f }, 0, TEXT("at the main doors"), nullptr, TEXT("SCI_1_Doors.png"), -90.f },
        { EStep::Open, { 0.f, 1690.f }, 0, TEXT("open the main doors"), TEXT("MainA"), nullptr },
        { EStep::Walk, { 0.f, 1300.f }, 0, TEXT("into the lobby"), nullptr, TEXT("SCI_2_Lobby.png"), -110.f },
        { EStep::Walk, { 0.f, 700.f }, 0, TEXT("between the stairs"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 90.f }, 0, TEXT("the corridor"), nullptr, nullptr },
        { EStep::Walk, { -1966.f, 90.f }, 0, TEXT("at the chemistry lab"), nullptr, nullptr },
        { EStep::Walk, { -1966.f, 450.f }, 0, TEXT("into the chemistry lab"), nullptr, TEXT("SCI_3_Chem.png"), 130.f },
        { EStep::Walk, { -1966.f, 90.f }, 0, TEXT("back to the corridor"), nullptr, nullptr },
        { EStep::Walk, { -1904.f, 60.f }, 0, TEXT("at the prep room"), nullptr, nullptr },
        { EStep::Locked, { -1904.f, 60.f }, 0, TEXT("try the prep room door"), TEXT("PrepRoom"), nullptr },
        { EStep::Walk, { -1351.f, 90.f }, 0, TEXT("at the lecture hall"), nullptr, nullptr },
        { EStep::Walk, { -1351.f, -300.f }, 0, TEXT("into the lecturer's well"), nullptr, nullptr },
        { EStep::Walk, { -420.f, -480.f }, 0, TEXT("past the demo bench"), nullptr, nullptr },
        { EStep::Walk, { -73.f, -500.f }, 0, TEXT("the foot of the centre aisle"), nullptr, nullptr },
        { EStep::Walk, { -73.f, -1500.f }, -1, TEXT("up the aisle to the cross-aisle"), nullptr, TEXT("SCI_4_Hall.png"), 90.f },
        { EStep::Walk, { -73.f, -500.f }, 0, TEXT("back down the aisle"), nullptr, nullptr },
        { EStep::Walk, { -420.f, -480.f }, 0, TEXT("across the well"), nullptr, nullptr },
        { EStep::Walk, { -1351.f, -300.f }, 0, TEXT("to the hall door"), nullptr, nullptr },
        { EStep::Walk, { -1351.f, 90.f }, 0, TEXT("out to the corridor"), nullptr, nullptr },
        { EStep::Walk, { 946.f, 90.f }, 0, TEXT("at the physics lab"), nullptr, nullptr },
        { EStep::Walk, { 946.f, 450.f }, 0, TEXT("into the physics lab"), nullptr, TEXT("SCI_5_Physics.png"), 40.f },
        { EStep::Walk, { 946.f, 90.f }, 0, TEXT("back to the corridor"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 90.f }, 0, TEXT("the corridor at the lobby"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 1060.f }, 0, TEXT("back into the lobby"), nullptr, nullptr },
        { EStep::Walk, { -660.f, 1060.f }, 0, TEXT("the foot of the west stair"), nullptr, nullptr },
        { EStep::Walk, { -660.f, 150.f }, 1, TEXT("up the west stair to the corridor"), nullptr, nullptr },
        { EStep::Walk, { -1054.f, 150.f }, 1, TEXT("at the optics lab"), nullptr, nullptr },
        { EStep::Walk, { -1054.f, 450.f }, 1, TEXT("into the optics lab"), nullptr, TEXT("SCI_6_Optics.png"), 140.f },
        { EStep::Walk, { -1054.f, 150.f }, 1, TEXT("back to the corridor"), nullptr, nullptr },
        { EStep::Walk, { 946.f, 150.f }, 1, TEXT("at the seminar room"), nullptr, nullptr },
        { EStep::Walk, { 946.f, 500.f }, 1, TEXT("into the seminar room"), nullptr, nullptr },
        { EStep::Walk, { 946.f, 150.f }, 1, TEXT("back to the corridor"), nullptr, nullptr },
        { EStep::Walk, { 1746.f, 150.f }, 1, TEXT("at the faculty lounge"), nullptr, nullptr },
        { EStep::Walk, { 1746.f, 500.f }, 1, TEXT("into the faculty lounge"), nullptr, TEXT("SCI_7_Lounge.png"), 60.f },
        { EStep::Walk, { 1746.f, 150.f }, 1, TEXT("back to the corridor"), nullptr, nullptr },
        { EStep::Walk, { 1646.f, 60.f }, 1, TEXT("at the faculty offices"), nullptr, nullptr },
        { EStep::Locked, { 1646.f, 60.f }, 1, TEXT("try the offices' door"), TEXT("Offices214"), nullptr },
        { EStep::Walk, { 515.f, 150.f }, 1, TEXT("the head of the east stair"), nullptr, nullptr },
        { EStep::Walk, { 515.f, 1060.f }, 0, TEXT("down the east stair"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 1060.f }, 0, TEXT("across the lobby"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 1690.f }, 0, TEXT("to the doors"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 2200.f }, 0, TEXT("out through the doors"), nullptr, nullptr },
        { EStep::Leave, { 0.f, 0.f }, 0, TEXT("away to the house; the inside must be freed"), nullptr, nullptr },
    };
    return R;
}

const TArray<FStop>& UnionRouteToday();
/** On the Calder terrain the Union's main (west) door stands 5.2 m below the tower plaza, in its west court: the walk comes
 *  down the Union steps from the plaza (its forecourt) to the doors, and goes back out into the court. */
const TArray<FStop>& UnionRoute()
{
    static const TArray<FStop> R = []
    {
        TArray<FStop> Out = UnionRouteToday();
        if (!HouseCalderFeatures::Active()) return Out;
        Out[0] = { EStep::Start, { 1550.f, 2175.f }, 0, TEXT("the head of the Union steps, on the tower plaza"), nullptr, nullptr };
        Out.Insert({ EStep::Walk, { 150.f, 2175.f }, 0, TEXT("down the Union steps to the west court"), nullptr, nullptr }, 1);
        for (FStop& S : Out) if (S.Kind == EStep::Walk && S.At == FVector2D(0.f, 2550.f)) S.At = FVector2D(0.f, 2200.f);   // out into the court
        return Out;
    }();
    return R;
}

const TArray<FStop>& UnionRouteToday()
{
    static const TArray<FStop> R = {
        { EStep::Start, { 0.f, 2550.f }, 0, TEXT("the Goose Union's front walk"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 2040.f }, 0, TEXT("at the main doors"), nullptr, TEXT("UNI_1_Doors.png"), -90.f },
        { EStep::Open, { 0.f, 2040.f }, 0, TEXT("open the main doors"), TEXT("MainA"), nullptr },
        { EStep::Walk, { -82.f, 1600.f }, 0, TEXT("into the north arm"), nullptr, TEXT("UNI_2_Concourse.png"), -90.f },
        { EStep::Walk, { -82.f, 400.f }, 0, TEXT("past the north stair"), nullptr, nullptr },
        { EStep::Walk, { -82.f, -82.f }, 0, TEXT("under the atrium"), nullptr, TEXT("UNI_3_Atrium.png"), 0.f },
        { EStep::Walk, { 400.f, 0.f }, 0, TEXT("the east arm"), nullptr, nullptr },
        { EStep::Walk, { 400.f, 450.f }, 0, TEXT("into the Goose Pit"), nullptr, TEXT("UNI_4_GoosePit.png"), 30.f },
        { EStep::Walk, { 400.f, 0.f }, 0, TEXT("back to the east arm"), nullptr, nullptr },
        { EStep::Walk, { 1250.f, 40.f }, 0, TEXT("at the trophy case"), nullptr, TEXT("UNI_5_Trophy.png"), 90.f },
        { EStep::Walk, { 0.f, 0.f }, 0, TEXT("back across the atrium"), nullptr, nullptr },
        { EStep::Walk, { -1150.f, -82.f }, 0, TEXT("the west arm"), nullptr, nullptr },
        { EStep::Walk, { -1150.f, -700.f }, 0, TEXT("into the game room"), nullptr, TEXT("UNI_6_GameRoom.png"), -120.f },
        { EStep::Walk, { -1150.f, -82.f }, 0, TEXT("back to the west arm"), nullptr, nullptr },
        { EStep::Walk, { -1700.f, -82.f }, 0, TEXT("at the TV lounge"), nullptr, nullptr },
        { EStep::Walk, { -1700.f, 450.f }, 0, TEXT("into the TV lounge"), nullptr, TEXT("UNI_7_TVLounge.png"), 50.f },
        { EStep::Walk, { -1700.f, -82.f }, 0, TEXT("back to the west arm"), nullptr, nullptr },
        { EStep::Walk, { -82.f, -82.f }, 0, TEXT("the atrium again"), nullptr, nullptr },
        { EStep::Walk, { -100.f, -1400.f }, 0, TEXT("down the south arm"), nullptr, nullptr },
        { EStep::Walk, { -335.f, -1400.f }, 0, TEXT("the foot of the south stair"), nullptr, nullptr },
        { EStep::Walk, { -335.f, -500.f }, 1, TEXT("up the south stair"), nullptr, nullptr },
        { EStep::Walk, { -380.f, -500.f }, 1, TEXT("the stair's landing"), nullptr, nullptr },
        { EStep::Walk, { -380.f, 646.f }, 1, TEXT("round the atrium to the committee room"), nullptr, nullptr },
        { EStep::Walk, { -600.f, 646.f }, 1, TEXT("into the homecoming committee room"), nullptr, TEXT("UNI_8_Committee.png"), 160.f },
        { EStep::Walk, { -380.f, 646.f }, 1, TEXT("back to the north arm"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 1450.f }, 1, TEXT("at the ballroom doors"), nullptr, nullptr },
        { EStep::Walk, { 600.f, 1450.f }, 1, TEXT("into the Calder Ballroom"), nullptr, TEXT("UNI_9_Ballroom.png"), -20.f },
        { EStep::Walk, { 0.f, 1450.f }, 1, TEXT("back to the north arm"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 450.f }, 1, TEXT("past the north stair's well"), nullptr, nullptr },
        { EStep::Walk, { 170.f, 450.f }, 1, TEXT("the head of the north stair"), nullptr, nullptr },
        { EStep::Walk, { 170.f, 1400.f }, 0, TEXT("down the north stair"), nullptr, nullptr },
        { EStep::Walk, { -82.f, 1400.f }, 0, TEXT("the north arm"), nullptr, nullptr },
        { EStep::Walk, { -350.f, 1304.f }, 0, TEXT("at the bookstore"), nullptr, nullptr },
        { EStep::Locked, { -350.f, 1304.f }, 0, TEXT("try the bookstore door"), TEXT("Bookstore"), nullptr },
        { EStep::Walk, { -82.f, 1600.f }, 0, TEXT("back to the doors"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 2040.f }, 0, TEXT("at the doors"), nullptr, nullptr },
        { EStep::Walk, { 0.f, 2550.f }, 0, TEXT("out through the doors"), nullptr, nullptr },
        { EStep::Leave, { 0.f, 0.f }, 0, TEXT("away to the house; the inside must be freed"), nullptr, nullptr },
    };
    return R;
}

/** The stadium (free-standing, world space): up the stair tower to the press box and down again, round to GATE 5, along
 *  the concourse to the locker room and its tunnel, then up to the vomitory's landing. */
const TArray<FStop>& StadiumRoute()
{
    static const TArray<FStop> R = []
    {
        auto Bowl = [](float Deg, float Rad) { return HouseCampusInteriors::StadiumPoint(Deg, Rad); };
        auto Toward = [](const FVector2D& From, const FVector2D& To) { return FMath::RadiansToDegrees(FMath::Atan2(To.Y - From.Y, To.X - From.X)); };
        TArray<FStop> Out = {
            { EStep::Start, { 4600.f, -9150.f }, 0, TEXT("beside the press box's stair tower"), nullptr, nullptr },   // (not under the press box: the start drops onto what is below)
            { EStep::Walk, { 4600.f, -9400.f }, 0, TEXT("under the press box"), nullptr, nullptr },
            { EStep::Walk, { 4600.f, -9553.f }, 0, TEXT("at the tower's doors"), nullptr, TEXT("STA_1_Tower.png"), 0.f },
            { EStep::Open, { 4600.f, -9553.f }, 0, TEXT("open the tower's doors"), TEXT("MainA"), nullptr },
            { EStep::Walk, { 4770.f, -9553.f }, 0, TEXT("inside the tower"), nullptr, nullptr },
            { EStep::Walk, { 4888.f, -9560.f }, 0, TEXT("to the first flight"), nullptr, nullptr },
        };
        // Up: odd flights climb the east lane north, even ones the west lane south.
        for (int32 L = 1; L <= 8; ++L)
        {
            const bool bNorth = L % 2 == 1;
            const float X = bNorth ? 4888.f : 4772.f, Y = bNorth ? -9190.f : -9550.f;
            Out.Add({ EStep::Walk, { X, Y }, L == 8 ? 1 : -1, L == 8 ? TEXT("up to the top landing") : TEXT("up a flight"), nullptr, nullptr });
            if (L < 8) Out.Add({ EStep::Walk, { bNorth ? 4772.f : 4888.f, Y }, -1, TEXT("across the landing"), nullptr, nullptr });
        }
        Out.Append({
            { EStep::Walk, { 4600.f, -9564.f }, 1, TEXT("into the press box"), nullptr, nullptr },
            { EStep::Walk, { 2700.f, -9700.f }, 1, TEXT("along the press row"), nullptr, TEXT("STA_2_PressBox.png"), -100.f },
            { EStep::Walk, { 1100.f, -9504.f }, 1, TEXT("at the radio booth"), nullptr, nullptr },
            { EStep::Locked, { 1100.f, -9504.f }, 1, TEXT("try the radio booth's door"), TEXT("RadioBooth"), nullptr },
            { EStep::Walk, { 4600.f, -9564.f }, 1, TEXT("back to the tower"), nullptr, nullptr },
            { EStep::Walk, { 4772.f, -9550.f }, 1, TEXT("the top landing"), nullptr, nullptr },
        });
        for (int32 L = 8; L >= 1; --L)
        {
            const bool bNorth = L % 2 == 1;   // flight L runs between the landing at this end and the other
            const float XFrom = bNorth ? 4888.f : 4772.f, YDown = bNorth ? -9560.f : -9190.f;
            Out.Add({ EStep::Walk, { XFrom, YDown }, L == 1 ? 0 : -1, L == 1 ? TEXT("down to the foot of the tower") : TEXT("down a flight"), nullptr, nullptr });
            if (L > 1) Out.Add({ EStep::Walk, { bNorth ? 4772.f : 4888.f, YDown }, -1, TEXT("across the landing"), nullptr, nullptr });
        }
        const FVector2D Gate5 = Bowl(94.5f, 1.f), InGate = Bowl(94.5f, .94f);
        Out.Append({
            { EStep::Walk, { 4770.f, -9553.f }, 0, TEXT("to the tower's doors"), nullptr, nullptr },
            { EStep::Walk, { 4560.f, -9553.f }, 0, TEXT("out of the tower"), nullptr, nullptr },
            { EStep::Walk, { 4560.f, -9200.f }, 0, TEXT("under the press box"), nullptr, nullptr },
            { EStep::Walk, { Gate5.X + 10.f, -9200.f }, 0, TEXT("along the north stands"), nullptr, nullptr },
            { EStep::Walk, Bowl(94.5f, 1.025f), 0, TEXT("at GATE 5"), nullptr, TEXT("STA_3_Gate5.png"), Toward(Gate5, InGate) },
            { EStep::Walk, InGate, 0, TEXT("through the gate into the concourse"), nullptr, nullptr },
            { EStep::Walk, Bowl(85.f, .94f), 0, TEXT("along the concourse"), nullptr, TEXT("STA_4_Concourse.png"), Toward(Bowl(85.f, .94f), Bowl(70.f, .94f)) },
            { EStep::Walk, Bowl(75.f, .94f), 0, TEXT("past GATE 4"), nullptr, nullptr },
            { EStep::Walk, Bowl(64.3f, .93f), 0, TEXT("at the locker room"), nullptr, nullptr },
            { EStep::Walk, Bowl(64.3f, .80f), 0, TEXT("into the Geese locker room"), nullptr, TEXT("STA_5_Locker.png"), Toward(Bowl(64.3f, .80f), Bowl(50.f, .79f)) },
            { EStep::Walk, Bowl(58.5f, .75f), 0, TEXT("by the tunnel"), nullptr, TEXT("STA_6_Tunnel.png"), Toward(Bowl(58.5f, .75f), Bowl(58.5f, 0.f)) },
            { EStep::Walk, Bowl(64.3f, .80f), 0, TEXT("back across the locker room"), nullptr, nullptr },
            { EStep::Walk, Bowl(64.3f, .93f), 0, TEXT("out to the concourse"), nullptr, nullptr },
            { EStep::Walk, Bowl(75.f, .955f), 0, TEXT("back along the concourse"), nullptr, nullptr },
            { EStep::Walk, Bowl(95.f, .955f), 0, TEXT("past GATE 5"), nullptr, nullptr },
            { EStep::Walk, Bowl(104.f, .955f), 0, TEXT("behind the vomitory stair"), nullptr, nullptr },
            { EStep::Walk, Bowl(112.f, .955f), 0, TEXT("the west end"), nullptr, nullptr },
            { EStep::Walk, { 425.9f, -10082.5f }, 0, TEXT("the foot of the vomitory stair"), nullptr, nullptr },
            { EStep::Walk, { 1232.2f, -9951.2f }, -1, TEXT("up to the vomitory's landing"), nullptr, nullptr },
            { EStep::Walk, { 1278.9f, -10237.4f }, -1, TEXT("out over the stands"), nullptr, TEXT("STA_7_Vomitory.png"), Toward(FVector2D(1278.9f, -10237.4f), Bowl(0.f, 0.f)) },
            { EStep::Walk, { 1232.2f, -9951.2f }, -1, TEXT("back in"), nullptr, nullptr },
            { EStep::Walk, { 425.9f, -10082.5f }, 0, TEXT("down the stair"), nullptr, nullptr },
            { EStep::Walk, Bowl(112.f, .955f), 0, TEXT("round the stair's foot"), nullptr, nullptr },
            { EStep::Walk, Bowl(104.f, .955f), 0, TEXT("back along the concourse"), nullptr, nullptr },
            { EStep::Walk, InGate, 0, TEXT("to GATE 5"), nullptr, nullptr },
            { EStep::Walk, Bowl(94.5f, 1.06f), 0, TEXT("out through the gate"), nullptr, nullptr },
            { EStep::Leave, { 0.f, 0.f }, 0, TEXT("away to the house; the inside must be freed"), nullptr, nullptr },
        });
        return Out;
    }();
    return R;
}

/** The clock tower (free-standing, world space; HouseCampusInteriorsTower): in at its oak door, across the foot of the well
 *  to the first flight, up the eleven flights round it and through the slot in the belfry's floor, to the hour bell, and
 *  down and out again. A flight's stop is 60 cm past the middle of the landing at its head (or foot, going down) along the
 *  flight, so the walker, which stops 60 cm short, stands in the landing's middle and turns there clear of the newel. */
const TArray<FStop>& TowerRoute()
{
    static const TArray<FStop> R = []
    {
        const float TX = -9000.f, TY = -10800.f, Mid = 211.5f;   // HouseClockTowerSite; the flights' walking line
        auto W = [TX, TY](float X, float Y) { return FVector2D(TX + X, TY + Y); };
        auto Corner = [Mid](int32 K) { const int32 C = K % 4; return FVector2D(C == 0 || C == 3 ? Mid : -Mid, C < 2 ? -Mid : Mid); };
        auto Past = [TX, TY, &Corner](int32 K, int32 From) { const FVector2D P = Corner(K) + (Corner(K) - Corner(From)).GetSafeNormal() * 60.f; return FVector2D(TX + P.X, TY + P.Y); };
        TArray<FStop> Out = {
            { EStep::Start, W(0.f, 560.f), 0, TEXT("on the plaza before the clock tower"), nullptr, nullptr },   // (north of its steps: nothing overhead)
            { EStep::Walk, W(0.f, 380.f), -1, TEXT("at the tower's door"), nullptr, TEXT("TWR_1_Door.png"), -90.f },   // (the granite landing, or the plinth's step)
            { EStep::Open, W(0.f, 380.f), 0, TEXT("open the tower's door"), TEXT("MainA"), nullptr },
            { EStep::Walk, W(0.f, 150.f), 0, TEXT("through the door"), nullptr, TEXT("TWR_2_Inside.png"), -120.f },
            // (East of the walking line, so the next leg runs nearly straight down the east side, clear of the first flight's
            // newel and the end of its well guard at X 165.)
            { EStep::Walk, W(Mid + 25.f, 20.f), 0, TEXT("across the foot of the well"), nullptr, nullptr },
            { EStep::Walk, W(Mid, -Mid - 60.f), 0, TEXT("to the foot of the first flight"), nullptr, nullptr },
        };
        for (int32 K = 1; K <= 10; ++K)
            Out.Add({ EStep::Walk, Past(K, K - 1), -1, TEXT("up a flight"), nullptr, K == 5 ? TEXT("TWR_3_Stair.png") : nullptr, K == 5 ? 45.f : 0.f });
        Out.Append({
            // Up the top flight to its last step (it stops 60 short of the corner post's side), then onto the belfry floor
            // past the stair head (east of the slot's guard), to the hour bell; back to the top step from the east.
            { EStep::Walk, W(215.f, Mid), -1, TEXT("up the top flight through the belfry floor"), nullptr, nullptr },
            { EStep::Walk, W(182.f, 0.f), 1, TEXT("in the belfry, by the hour bell"), nullptr, TEXT("TWR_4_Belfry.png"), 180.f },
            { EStep::Walk, W(140.f, 270.f), -1, TEXT("back to the head of the stair"), nullptr, nullptr },
        });
        for (int32 K = 10; K >= 0; --K)
            Out.Add({ EStep::Walk, Past(K, K + 1), K == 0 ? 0 : -1, K == 0 ? TEXT("down to the ground floor") : TEXT("down a flight"), nullptr, nullptr });
        Out.Append({
            { EStep::Walk, W(Mid, 0.f), 0, TEXT("across the foot of the well"), nullptr, nullptr },
            { EStep::Walk, W(0.f, 150.f), 0, TEXT("to the door"), nullptr, nullptr },
            { EStep::Walk, W(0.f, 640.f), 0, TEXT("out onto the plaza"), nullptr, nullptr },   // (it stops at 580, past the steps)
            { EStep::Leave, { 0.f, 0.f }, 0, TEXT("away to the house; the inside must be freed"), nullptr, nullptr },
        });
        return Out;
    }();
    return R;
}

/** The Tier B and C buildings' walks, from their plans (HouseCampusInteriors::GenericTour); the strings kept alive here. */
void GenericRoutes(TArray<FBuildingRoute>& Out)
{
    static TArray<TUniquePtr<FString>> Strings;
    auto Keep = [](const FString& S) -> const TCHAR* { if (S.IsEmpty()) return nullptr; Strings.Add(MakeUnique<FString>(S)); return **Strings.Last(); };
    for (int32 Seed = 0; Seed < HouseCampusLayout::Blocks().Num(); ++Seed)
    {
        if (!HouseCampusInteriors::IsGeneric(Seed)) continue;
        TArray<HouseCampusInteriors::FTourStop> Tour;
        HouseCampusInteriors::GenericTour(Seed, Tour);
        FBuildingRoute Route;
        Route.Id = Keep(HouseCampusLayout::Blocks()[Seed].Id);
        for (int32 I = 0; I < Tour.Num(); ++I)
        {
            const HouseCampusInteriors::FTourStop& T = Tour[I];
            const EStep Kind = I == 0 ? EStep::Start : T.Kind == 1 ? EStep::Locked : T.Kind == 2 ? EStep::Open : EStep::Walk;
            Route.Stops.Add({ Kind, T.At, 0, Keep(T.Name), Keep(T.Door), Keep(T.Shot), T.LookYaw });
        }
        Route.Stops.Add({ EStep::Leave, FVector2D::ZeroVector, 0, TEXT("away to the house; the inside must be freed"), nullptr, nullptr });
        // On the Calder terrain a front-walk stop that would land on a stair or ramp (entrance steps, a flight up to the
        // door's landing: FLD, ENG, PLT, ALU) moves OUT past the steps, and past any further flight within 3 m beyond them,
        // onto the walk at their foot: it stands on one floor, and the walk in climbs (the walk out comes down) every step
        // between the walk and the doors, so the entrance is proven reachable on foot from its walk.
        if (HouseCalderFeatures::Active())
        {
            const FTransform Frame = HouseCampusInteriors::Frame(Seed);
            const float Face = HouseCampusInteriors::CanonRect(Seed).Max.Y;
            auto OnSteps = [&Frame](const FVector2D& C)
            {
                const FVector W = Frame.TransformPosition(FVector(C, 0.f));
                return HouseCalderFeatures::InStair(FVector2D(W.X, W.Y), 40.f) || HouseCalderFeatures::InRamp(FVector2D(W.X, W.Y), 40.f);
            };
            auto StepsAhead = [&OnSteps](const FVector2D& C)
            {
                for (float D = 0.f; D <= 300.f; D += 25.f) if (OnSteps(C + FVector2D(0.f, D))) return true;
                return false;
            };
            // Then 75 cm further: a stop counts as reached within 60 cm, so one that stood just past the foot could be
            // "reached" with the walker still on the bottom step (a riser above the walk: off the floor band).
            for (FStop& St : Route.Stops)
                if ((St.Kind == EStep::Start || St.Kind == EStep::Walk) && St.At.X == 0.f && St.At.Y > Face + 200.f && OnSteps(St.At))
                {
                    for (int32 Guard = 0; Guard < 240 && StepsAhead(St.At); ++Guard) St.At.Y += 25.f;   // (at most 60 m out)
                    St.At.Y += 75.f;
                }
        }
        Out.Add(Route);
    }
}

// Insert INSIDE HouseCampusInteriorsReviewImpl, before Routes().
// Review translation unit must include HouseFraternityRow.h at its normal top-level include block.
// Root owns Row-only12cm stop tolerance, floor datum, and idempotent normal door Open/wait handling.
// These are new routes. No pre-existing review waypoint is altered.
void RowRoutes(TArray<FBuildingRoute>& Out)
{
    if(!HouseFraternityRow::Enabled())return;
    // Static literals: FStop stores pointers, so no temporary FString is used for names or screenshot paths.
    static const TCHAR* Shots[5][11]={
        {TEXT("R1_01_Porch.png"),TEXT("R1_02_Hall.png"),TEXT("R1_03_Lounge.png"),TEXT("R1_04_UpperLock.png"),TEXT("R1_05_Chapter.png"),TEXT("R1_06_Dining.png"),TEXT("R1_07_Kitchen.png"),TEXT("R1_08_WC.png"),TEXT("R1_09_RearSteps.png"),TEXT("R1_10_Fence.png"),TEXT("R1_04b_Stairs.png")},
        {TEXT("R2_01_Porch.png"),TEXT("R2_02_Hall.png"),TEXT("R2_03_Lounge.png"),TEXT("R2_04_UpperLock.png"),TEXT("R2_05_Chapter.png"),TEXT("R2_06_Dining.png"),TEXT("R2_07_Kitchen.png"),TEXT("R2_08_WC.png"),TEXT("R2_09_RearSteps.png"),TEXT("R2_10_Fence.png"),TEXT("R2_04b_Stairs.png")},
        {TEXT("R3_01_Porch.png"),TEXT("R3_02_Hall.png"),TEXT("R3_03_Lounge.png"),TEXT("R3_04_UpperLock.png"),TEXT("R3_05_Chapter.png"),TEXT("R3_06_Dining.png"),TEXT("R3_07_Kitchen.png"),TEXT("R3_08_WC.png"),TEXT("R3_09_RearSteps.png"),TEXT("R3_10_Fence.png"),TEXT("R3_04b_Stairs.png")},
        {TEXT("R4_01_Porch.png"),TEXT("R4_02_Hall.png"),TEXT("R4_03_Lounge.png"),TEXT("R4_04_UpperLock.png"),TEXT("R4_05_Chapter.png"),TEXT("R4_06_Dining.png"),TEXT("R4_07_Kitchen.png"),TEXT("R4_08_WC.png"),TEXT("R4_09_RearSteps.png"),TEXT("R4_10_Fence.png"),TEXT("R4_04b_Stairs.png")},
        {TEXT("R5_01_Porch.png"),TEXT("R5_02_Hall.png"),TEXT("R5_03_Lounge.png"),TEXT("R5_04_UpperLock.png"),TEXT("R5_05_Chapter.png"),TEXT("R5_06_Dining.png"),TEXT("R5_07_Kitchen.png"),TEXT("R5_08_WC.png"),TEXT("R5_09_RearSteps.png"),TEXT("R5_10_Fence.png"),TEXT("R5_04b_Stairs.png")}
    };
    // Same clear backyard paths used by HouseFraternityRowYards, ending120cm before the original fence.
    static const TArray<TArray<FVector2D>> Yard={
        {{-17400,85},{-17400,900},{-17100,1450},{-17100,3300},{-16900,4400}},
        {{-13900,-50},{-13900,900},{-13650,2300},{-13650,4400}},
        {{-10100,-20},{-10100,550},{-9700,1900},{-9350,3300},{-9350,4400}},
        {{-7600,2350},{-7600,2700},{-7400,3100},{-7400,3620},{-7500,4400}},
        {{-6300,300},{-6300,700},{-5500,900},{-5500,2850},{-5700,3750},{-5550,4400}}
    };
    const auto& Houses=HouseFraternityRow::Houses();
    if(Houses.Num()!=5)return;
    for(int32 I=0;I<5;++I)
    {
        const auto& H=Houses[I];
        const float W=H.Width(),D=H.Depth(),C=W*.5f,LW=C-120.5f,RW=C+120.5f;
        const float Y1=D*.47f,Y2=D*.76f;
        const float DoorY[5]={Y1-110,Y1*.42f,(Y1+Y2)*.5f,D*.68f,(Y2+D)*.5f};
        const float XL=FMath::Max(50.f,C-410.f),XR=C-135.f;
        const float LowX=XL+55,UpX=XR-55,RunEnd=190+(I<3?11:9)*28;
        FBuildingRoute R;R.Id=H.Id;
        auto At=[&](float X,float Y){return FVector2D(H.L+X,H.F+Y);};
        auto Add=[&](EStep Kind,float X,float Y,int32 Floor,const TCHAR* Name,const TCHAR* Door=nullptr,const TCHAR* Shot=nullptr,float Yaw=0.f)
        {R.Stops.Add({Kind,At(X,Y),Floor,Name,Door,Shot,Yaw});};
        auto Walk=[&](float X,float Y,int32 Floor,const TCHAR* Name,const TCHAR* Shot=nullptr,float Yaw=0.f)
        {Add(EStep::Walk,X,Y,Floor,Name,nullptr,Shot,Yaw);};
        auto Door=[&](float X,float Y,const TCHAR* Id,const TCHAR* Name)
        {Add(EStep::Open,X,Y,0,Name,Id);};
        auto Enter=[&](bool Left,int32 Room,const TCHAR* Id,const TCHAR* Name,const TCHAR* Shot)
        {
            Walk(C,DoorY[Room],0,TEXT("hall outside the room"));
            Door(C,DoorY[Room],Id,TEXT("open the room door normally"));
            Walk((Left?LW-110:RW+110),DoorY[Room],0,Name,Shot,Left?180.f:0.f);
        };
        auto Exit=[&](int32 Room){Walk(C,DoorY[Room],0,TEXT("back through the room door into the hall"));};

        // Start truly on existing grade, walk each exterior riser, and interact from the actual porch.
        if(I<3)
        {
            Add(EStep::Start,C,-1060,0,TEXT("original Row front grade"));
            Walk(C,-960,0,TEXT("approach the original front steps"));
            Walk(C,-750,-1,TEXT("walk the original front risers"));
            Walk(C,-500,0,TEXT("onto the original front porch"));
        }
        else
        {
            const float Front=I==4?H.F-150.f:150.f;
            Add(EStep::Start,C,Front-(I==4?280.f:150.f)-H.F,0,TEXT("existing grade before the new front porch"));
            Walk(C,Front-(I==4?210.f:70.f)-H.F,0,TEXT("at existing grade before the ordinary front risers"));
            Walk(C,Front-(I==4?126.f:14.f)-H.F,-1,TEXT("over the first new front tread"));
        }
        Walk(C,-100,0,TEXT("on the front porch at the main pair"),Shots[I][0],90);
        Door(C,-100,TEXT("MainA"),TEXT("open or verify the front pair"));
        Walk(C,200,0,TEXT("through the main pair into the clear hall"),Shots[I][1],90);

        Enter(true,0,TEXT("Lounge"),TEXT("inside the living room"),Shots[I][2]);
        // Clear the open lounge leaf BEFORE moving south; compact R5's bag stays north of this line.
        Walk(LW-110,DoorY[0]-25,0,TEXT("clear the open living-room leaf"));
        Walk(LowX,DoorY[0]-25,0,TEXT("around the living-room leaf's west tip"));
        Walk(LowX,RunEnd+62,0,TEXT("at the actual lower stair approach"));
        Walk(LowX,RunEnd-56,-1,TEXT("start up the southbound lower flight"));
        Walk(LowX,230,-1,TEXT("continue up the lower flight"));
        Walk(LowX,135,-1,TEXT("on the110cm intermediate return landing"));
        Walk(UpX,135,-1,TEXT("turn across the return landing between visible guards"));
        Walk(UpX,274,-1,TEXT("up the northbound upper flight"));
        Walk(UpX,RunEnd-56,-1,TEXT("near the top of the returning flight"));
        Walk(UpX,RunEnd+55,1,TEXT("on the110cm upper landing"),Shots[I][3],90);
        Add(EStep::Locked,UpX,RunEnd+55,1,TEXT("try the visible private upper door"),TEXT("UpperPrivate"));
        Walk(UpX,RunEnd-56,-1,TEXT("descend the upper flight"));
        Walk(UpX,274,-1,TEXT("continue down toward the intermediate landing"));
        Walk(UpX,135,-1,TEXT("back on the intermediate return landing"));
        Walk(LowX,135,-1,TEXT("turn back onto the lower flight"));
        Walk(LowX,230,-1,TEXT("descend the lower flight"));
        Walk(LowX,RunEnd-56,-1,TEXT("near the foot of the lower flight"));
        Walk(LowX,RunEnd+62,0,TEXT("fully off the stair onto the ground floor"),Shots[I][10],-90);
        if(I==4)
        {
            // East of the sofa, west of the stair, then into the actual curved reading alcove.
            Walk(238.5f,RunEnd+62,0,TEXT("past the sofa's east side"));
            Walk(238.5f,230,0,TEXT("past the south end of the sofa"));
            Walk(200,250,0,TEXT("inside the curved southwest reading alcove"),TEXT("R5_03b_Alcove.png"),-120);
            Walk(238.5f,230,0,TEXT("out of the reading alcove"));
            Walk(238.5f,RunEnd+62,0,TEXT("back along the sofa's clear east side"));
            Walk(LowX,RunEnd+62,0,TEXT("back to the stair approach"));
        }
        Walk(LowX,DoorY[0]-25,0,TEXT("return north of the open lounge leaf"));
        Walk(LW-110,DoorY[0]-25,0,TEXT("round the lounge leaf's tip"));
        Walk(LW-110,DoorY[0],0,TEXT("aligned with the living-room opening"));
        Exit(0);

        Enter(false,1,TEXT("Chapter"),TEXT("inside the chapter room"),Shots[I][4]);Exit(1);
        Enter(true,2,TEXT("Dining"),TEXT("inside the dining room"),Shots[I][5]);
        if(I==4)
        {
            // Compact dining table ends at localX312; the open hall leaf ends nearX432.
            Walk(370,DoorY[2],0,TEXT("around the dining hall leaf's west tip"));
            Walk(370,Y1+54,0,TEXT("the clear aisle south of the dining chairs"));
            Walk(200,Y1+54,0,TEXT("at the real parlor connecting pair"));
            Door(200,Y1+54,TEXT("ParlorA"),TEXT("open the connecting parlor pair normally"));
            Walk(200,Y1-55,0,TEXT("through the connecting pair into the front parlor"),TEXT("R5_06b_ConnectedParlors.png"),-90);
            Walk(200,Y1+54,0,TEXT("back through the parlor pair into dining"));
            Walk(370,Y1+54,0,TEXT("back along the front dining aisle"));
            Walk(370,DoorY[2],0,TEXT("round the dining hall leaf"));
            Walk(LW-110,DoorY[2],0,TEXT("aligned with the dining-room exit"));
        }
        Exit(2);
        Enter(false,3,TEXT("Kitchen"),TEXT("inside the practical kitchen"),Shots[I][6]);Exit(3);
        Enter(true,4,TEXT("SharedWC"),TEXT("inside the shared bathroom"),Shots[I][7]);
        // Bypass the open140cm bathroom leaf before moving south into the stall aisle.
        const float Cell=I==4?125.f:155.f,StallL=I==4?73.f:93.f;
        const float StallR=StallL+2*Cell+7.5f,Front=D-223;
        const float LeafTip=LW-8.5f-139.2f;
        const float Bypass=(StallR+LeafTip)*.5f;
        const float Aisle=(Y2+7.5f+Front-7.5f)*.5f;
        Walk(Bypass,DoorY[4],0,TEXT("around the bathroom leaf's west tip"));
        Walk(Bypass,Aisle,0,TEXT("into the bathroom's front aisle"));
        for(int32 N=0;N<2;++N)
        {
            const float X=StallL+(N+.5f)*Cell;
            Walk(X,Aisle,0,TEXT("at the cubicle door from the clear front aisle"));
            Door(X,Aisle,N==0?TEXT("WCStall1"):TEXT("WCStall2"),TEXT("open the cubicle door normally"));
            Walk(X,Front+55,0,N==0?TEXT("inside the first WC cubicle"):TEXT("inside the second WC cubicle"));
            Walk(X,Aisle,0,TEXT("out of the cubicle to the front aisle"));
        }
        Walk(Bypass,Aisle,0,TEXT("back around the bathroom door leaf"));
        Walk(Bypass,DoorY[4],0,TEXT("north of the open bathroom leaf"));
        Walk(LW-110,DoorY[4],0,TEXT("aligned with the bathroom exit"));Exit(4);

        Walk(C,D-170,0,TEXT("at the real rear pair"));
        Door(C,D-170,TEXT("RearA"),TEXT("open or verify the rear pair normally"));
        Walk(C,D+65,0,TEXT("through the rear pair onto its top landing"),Shots[I][8],-90);
        Walk(C,D+180,-1,TEXT("walk down the ordinary rear risers"));
        // World coordinates: these continue the same physical walk through the protected clear yard path.
        for(int32 N=0;N<Yard[I].Num();++N)
        {
            const bool Last=N==Yard[I].Num()-1;
            R.Stops.Add({EStep::Walk,Yard[I][N],0,N==0?TEXT("fully off the rear steps onto existing grade"):Last?TEXT("at the original rear fence approach"):TEXT("follow the clear existing yard trail"),nullptr,Last?Shots[I][9]:nullptr,90});
        }
        R.Stops.Add({EStep::Leave,FVector2D::ZeroVector,0,TEXT("leave the Row; verify player-only streaming unloads it"),nullptr,nullptr});
        Out.Add(MoveTemp(R));
    }
}

const TArray<FBuildingRoute>& Routes()
{
    static const TArray<FBuildingRoute> R = []
    {
        TArray<FBuildingRoute> All = {
        { TEXT("LIB"), LibraryRoute() },
        { TEXT("HAR"), HarlowRoute() },
        { TEXT("SCI"), ScienceRoute() },
        { TEXT("UNI"), UnionRoute() },
        { TEXT("STA"), StadiumRoute() },
        { TEXT("TWR"), TowerRoute() },
        };
        GenericRoutes(All);
        RowRoutes(All);
        return All;
    }();
    return R;
}

struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Walker;
    int32 Phase = 0, Stop = 0, Reached = 0, Problems = 0, InsideChecks = 0, InsideOk = 0, OutsideChecks = 0, OutsideOk = 0;
    int32 MaxLights = 0, Seed = -1, Building = 0, Buildings = 0, DoorsChecked = 0, DoorsOk = 0, FreedChecks = 0, FreedOk = 0;
    bool bLockedHeld = true, bReasons = true, bLoadedWhileIn = true;
    /** Wall clocks: buildings checked against the clock tower, clocks seen, the worst hand's error (degrees). */
    TSet<int32> ClockSeeds;
    int32 Clocks = 0;
    float ClockErr = 0.f;
    TArray<int32> Order;   // indices into Routes() this run walks
    double Start = 0.0, Step = 0.0, Progress = 0.0;
    float Best = 1e9f;
    FVector Goal = FVector::ZeroVector;
    FString Log, Dir;
};
FState& State() { static FState S; return S; }
/** Another review or capture on the command line ends the run itself (so this one must not end it on a pass). */
bool OthersEndTheRun()
{
    static const TCHAR* const SelfExiting[] = {
        TEXT("FreeRoamWalkReview"), TEXT("ParkingLotWalkReview"), TEXT("WoodsWalkReview"), TEXT("ClockTowerReview"), TEXT("HouseExteriorViews"),
        TEXT("HouseExteriorWalkReview"), TEXT("HouseWalkPerf"), TEXT("CampusDrumsRecord"), TEXT("WestFieldWalkReview"), TEXT("CalderTerrainReview"), TEXT("CalderPigVisualReview"), TEXT("CalderAccessReview"), TEXT("CalderFarmSolidWalkReview"), TEXT("CalderWildlifeReview"), TEXT("CalderWildlifePathsReview"), TEXT("CalderWildlifeNetworkReview"), TEXT("CalderWildlifeBirdArtReview"), TEXT("CalderWildlifeSkyPathsReview"), TEXT("CalderWildlifeSkyReview"), TEXT("CalderGoosePondReview"), TEXT("CalderGoosePondNetworkReview"), TEXT("CalderReachReview"), TEXT("CalderLandscapeReview"), TEXT("CalderTreePreview"), TEXT("CalderTreeFitReview"), TEXT("CalderRidgeForestReview") };
    for (const TCHAR* Name : SelfExiting) if (FParse::Param(FCommandLine::Get(), Name)) return true;
    FString Smoke;   // the interiors' smoke test ends the run too (it takes a value, so Param does not see it)
    return FParse::Value(FCommandLine::Get(), TEXT("CampusInteriorsSmoke="), Smoke, false);
}
}

void TickHouseCampusInteriorsReview(UWorld* W)
{
    namespace CR = HouseCampusInteriorsReviewImpl;
    // (FParse::Param does not match the =LIB,HAR form, so look for that too.)
    static const bool bOn = [] { FString Pick; return FParse::Param(FCommandLine::Get(), TEXT("CampusInteriorsReview")) || FParse::Value(FCommandLine::Get(), TEXT("CampusInteriorsReview="), Pick); }();
    if (!bOn || !W || UGameplayStatics::GetCurrentLevelName(W, true) != TEXT("Chapter01_House")) return;
    APlayerController* PC = W->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = PC ? Cast<AStoryFirstPersonCharacter>(PC->GetPawn()) : nullptr;
    if (!Pawn) return;
    CR::FState& S = CR::State();
    static const FDelegateHandle Input = FCoreDelegates::OnBeginFrame.AddLambda([]()
    {
        CR::FState& Live = CR::State();
        if (Live.Phase == 2 && Live.Walker.IsValid()) Live.Walker->AddMovementInput((Live.Goal - Live.Walker->GetActorLocation()).GetSafeNormal2D(), 1.f);
    });
    if (S.World.Get() != W)
    {
        S = CR::FState();
        S.World = W;
        S.Start = S.Step = W->GetTimeSeconds();
        S.Dir = FPaths::ProjectSavedDir() / TEXT("CampusInteriorsReview");
        IFileManager::Get().MakeDirectory(*S.Dir, true);
        IFileManager::Get().Delete(*(S.Dir / TEXT("result.txt")), false, true, true);   // no stale verdict
        // Which buildings: -CampusInteriorsReview=LIB,HAR picks; plain -CampusInteriorsReview walks every walk-in one.
        FString Pick;
        FParse::Value(FCommandLine::Get(), TEXT("CampusInteriorsReview="), Pick, false);   // (the whole list, commas and all)
        for (int32 I = 0; I < CR::Routes().Num(); ++I)
        {
            const int32 Seed = HouseCampusInteriors::SeedFor(CR::Routes()[I].Id);
            if (Seed < 0 || HouseCampusInteriors::OpenFloors(Seed) <= 0) continue;
            if (!Pick.IsEmpty() && !Pick.Contains(CR::Routes()[I].Id)) continue;
            S.Order.Add(I);
        }
        S.Buildings = S.Order.Num();
        S.Seed = S.Order.Num() ? HouseCampusInteriors::SeedFor(CR::Routes()[S.Order[0]].Id) : -1;
    }
    S.Walker = Pawn;
    auto Note = [&S](const FString& T) { S.Log += T + TEXT("\n"); UE_LOG(LogTemp, Display, TEXT("CAMPUS_INTERIORS_REVIEW %s"), *T); };
    auto Finish = [&](bool bOK, const FString& T)
    {
        Note((bOK ? TEXT("PASS ") : TEXT("FAIL ")) + T);
        FFileHelper::SaveStringToFile(S.Log, *(S.Dir / TEXT("result.txt")));
        S.Phase = -1;
        // A failure ends the run; a pass leaves it to another review that ends the run itself, if one shares it.
        if (!bOK || !CR::OthersEndTheRun()) FPlatformMisc::RequestExitWithStatus(false, bOK ? 0 : 1);
    };
    if (S.Phase < 0) return;
    const double Now = W->GetTimeSeconds();
    if (Now - S.Start > 120.0 + 360.0 * FMath::Max(1, S.Buildings)) { Finish(false, TEXT("timed out")); return; }
    UCampusInteriorsSubsystem* Sub = W->GetSubsystem<UCampusInteriorsSubsystem>();
    AHouseCampusInteriors* Interiors = Sub ? Sub->GetInteriors() : nullptr;
    if (!HouseCampusInteriors::Enabled())
    {
        // Nothing to walk. Report at once, but leave the run open for 30 s so the reviews sharing it can report too.
        if (S.Log.IsEmpty())
        {
            Note(TEXT("PASS interiors=off (-NoCampusInteriors or -NoCampusLayout): nothing to walk"));
            FFileHelper::SaveStringToFile(S.Log, *(S.Dir / TEXT("result.txt")));
        }
        if (Now - S.Start > 30.0 && !CR::OthersEndTheRun()) { S.Phase = -1; FPlatformMisc::RequestExitWithStatus(false, 0); }
        return;
    }
    const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Feet = Pawn->GetActorLocation() - FVector(0, 0, Half);
    if (!S.Order.IsValidIndex(S.Building) || S.Seed < 0) { Finish(false, TEXT("no walk-in building with a route")); return; }
    const TArray<CR::FStop>& Route = CR::Routes()[S.Order[S.Building]].Stops;
    const TCHAR* BuildingId = HouseCampusInteriors::InteriorId(S.Seed);
    const FTransform Frame = HouseCampusInteriors::Frame(S.Seed);
    const bool bRow = HouseFraternityRow::Enabled() && HouseFraternityRow::Houses().ContainsByPredicate([BuildingId](const HouseFraternityRow::FHouse& H){return FCString::Strcmp(H.Id,BuildingId)==0;});
    auto World2 = [&Frame](const FVector2D& C) { const FVector P = Frame.TransformPosition(FVector(C, 0.f)); return FVector2D(P.X, P.Y); };
    // The ground a stop stands on (all -60 at zero relief): inside, the building's -60 on its pad (BaseDZ); outside (the
    // same test as the outside-stop check below), the entrance apron's grade (its top less 5) or else the ground there.
    auto Datum = [&](const CR::FStop& Stop) -> float
    {
        const float Indoor = bRow ? HouseCampusInteriors::InteriorFloorZ(S.Seed,FMath::Max(0,Stop.Floor)) : -60.f + HouseCampusInteriors::BaseDZ(S.Seed);
        const FVector2D Spot = World2(Stop.At);
        const bool bOut = S.Seed >= HouseCampusInteriors::FreeSeedBase ? !HouseCampusInteriors::InsideInterior(S.Seed, FVector(Spot, Indoor + 100.f))
            : Stop.At.Y > HouseCampusInteriors::CanonRect(S.Seed).Max.Y;
        if (!bOut) return Indoor;
        static const TArray<HouseCampusLayout::FApron> Aprons = [] { TArray<HouseCampusLayout::FApron> Out; HouseCampusLayout::EntranceAprons(Out); return Out; }();
        for (const HouseCampusLayout::FApron& Apron : Aprons) if (Apron.R.IsInside(Spot)) return Apron.Top - 5.f;
        // On a Calder stair's tread or a ramp (entrance steps up to a door on its pad), that is the floor there.
        float Tread = 0.f;
        if (HouseCalderFeatures::FloorAt(Spot, Tread)) return Tread - 3.6f;
        return HouseExteriorSite::GroundHeight(Spot.X, Spot.Y);
    };
    auto FindDoor = [&](const TCHAR* Name) -> ACampusDoor*
    {
        const FName Id(*FString::Printf(TEXT("Campus_%s_%s"), BuildingId, Name));
        for (TActorIterator<ACampusDoor> It(W); It; ++It) if (It->DoorId == Id) return *It;
        return nullptr;
    };
    // Every tick inside the building: the inside is built, the indoor test reads inside, and lights are on (capped at 6).
    const FVector View = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : Pawn->GetActorLocation();
    if (Interiors) S.MaxLights = FMath::Max(S.MaxLights, Interiors->ActiveLights());
    // Once a building is built (and its hands set): its wall clocks must show the clock tower's time.
    if (Interiors && S.Seed >= 0 && !S.ClockSeeds.Contains(S.Seed))
    {
        int32 N = 0;
        const float Err = Interiors->ClockError(S.Seed, N);
        if (Err >= 0.f)
        {
            S.ClockSeeds.Add(S.Seed);
            S.Clocks += N;
            S.ClockErr = FMath::Max(S.ClockErr, Err);
            Note(FString::Printf(TEXT("wall clocks: %d, worst hand %.2f degrees from the tower's time"), N, Err));
            if (N == 0 || Err > 1.f) ++S.Problems;
        }
    }
    auto Advance = [&]()
    {
        ++S.Stop;
        S.Step = Now;
        S.Phase = 1;
    };
    if (S.Phase == 0)
    {
        bool bExterior = false;
        for (TActorIterator<AHouseExteriorDetail> It(W); It; ++It) bExterior = true;
        if (Now - S.Start < 8.0 || !bExterior || !Interiors || !HouseCalderTerrain::CollisionReady(W)) return;
        Note(FString::Printf(TEXT("doors=%d building=%s seed=%d openFloors=%d"), Sub->GetDoors().Num(), BuildingId, S.Seed, HouseCampusInteriors::OpenFloors(S.Seed)));
        S.Phase = 1;
        return;
    }
    if (S.Phase == 1)
    {
        if (Now - S.Step < .6) return;
        if (!Route.IsValidIndex(S.Stop))
        {
            // This building is done: on to the next, or the verdict.
            if (S.Order.IsValidIndex(S.Building + 1))
            {
                ++S.Building;
                S.Seed = HouseCampusInteriors::SeedFor(CR::Routes()[S.Order[S.Building]].Id);
                S.Stop = 0;
                Note(FString::Printf(TEXT("next building: %s"), CR::Routes()[S.Order[S.Building]].Id));
                return;
            }
            int32 Walks = 0;
            FString Ids;
            for (const int32 I : S.Order)
            {
                for (const CR::FStop& R : CR::Routes()[I].Stops) Walks += R.Kind == CR::EStep::Walk ? 1 : 0;
                Ids += (Ids.IsEmpty() ? TEXT("") : TEXT(",")) + FString(CR::Routes()[I].Id);
            }
            const bool bOK = S.Problems == 0 && S.Reached == Walks && S.DoorsOk == S.DoorsChecked && S.DoorsChecked == S.Buildings && S.bLockedHeld && S.bReasons
                && S.bLoadedWhileIn && S.FreedOk == S.FreedChecks && S.FreedChecks == S.Buildings
                && S.InsideOk == S.InsideChecks && S.OutsideOk == S.OutsideChecks && S.MaxLights > 0 && S.MaxLights <= AHouseCampusInteriors::LightPool
                && S.ClockSeeds.Num() == S.Buildings && S.ClockErr <= 1.f;
            Finish(bOK, FString::Printf(TEXT("buildings=%s stops=%d/%d problems=%d mainDoorsOpened=%d/%d lockedDoorsHeld=%d reasonsShown=%d builtWhileInside=%d freedAfterLeaving=%d/%d indoorTest=%d/%d outdoorTest=%d/%d maxInteriorLights=%d pool=%d wallClocks=%d buildingsWithClocks=%d/%d clockErrorDeg=%.2f"),
                *Ids, S.Reached, Walks, S.Problems, S.DoorsOk, S.DoorsChecked, int32(S.bLockedHeld), int32(S.bReasons), int32(S.bLoadedWhileIn), S.FreedOk, S.FreedChecks,
                S.InsideOk, S.InsideChecks, S.OutsideOk, S.OutsideChecks, S.MaxLights, AHouseCampusInteriors::LightPool, S.Clocks, S.ClockSeeds.Num(), S.Buildings, S.ClockErr));
            return;
        }
        const CR::FStop& St = Route[S.Stop];
        if (St.Kind == CR::EStep::Start || St.Kind == CR::EStep::Leave)
        {
            // Leaving: back toward the house, or (for a building near it, which would stay in range) away to the south campus.
            const FBox Here = HouseCampusInteriors::InteriorBox(S.Seed);
            const FVector2D Away = FMath::Sqrt(Here.ComputeSquaredDistanceToPoint(FVector(0.f, -3000.f, Here.GetCenter().Z))) < 8000.f ? FVector2D(0.f, -26000.f) : FVector2D(0.f, -3000.f);
            const FVector2D To = St.Kind == CR::EStep::Start ? World2(St.At) : Away;
            FCollisionQueryParams Q(SCENE_QUERY_STAT(CampusInteriorsGround), true, Pawn);
            FHitResult Ground;
            const float GroundAt = HouseExteriorSite::GroundHeight(To.X, To.Y);
            const bool bGround = W->LineTraceSingleByChannel(Ground, FVector(To, GroundAt + 2060.f), FVector(To, GroundAt - 2940.f), ECC_Pawn, Q);
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Pawn->SetActorLocation(FVector(To, (bGround ? float(Ground.ImpactPoint.Z) : GroundAt) + Half + 3.f), false, nullptr, ETeleportType::TeleportPhysics);
            Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            Note(FString::Printf(TEXT("stop %d: %s (placed at %.0f,%.0f)"), S.Stop, St.Name, To.X, To.Y));
            if (St.Kind == CR::EStep::Leave) { S.Phase = 3; S.Step = Now; return; }
            Advance();
            return;
        }
        if (St.Kind == CR::EStep::Open || St.Kind == CR::EStep::Locked)
        {
            ACampusDoor* Door = FindDoor(St.Door);
            if (!Door) { ++S.Problems; Note(FString::Printf(TEXT("problem: no door %s"), St.Door)); Advance(); return; }
            if (St.Kind == CR::EStep::Open)
            {
                PC->SetControlRotation((Door->DoorwayCenter - Pawn->GetActorLocation()).GetSafeNormal2D().Rotation());
                const bool bDid = bRow && Door->IsOpen() ? true : Door->Interact(Pawn);
                Note(FString::Printf(TEXT("stop %d: %s -> %s"), S.Stop, St.Name, bDid ? TEXT("opening") : TEXT("refused")));
                if (!bDid) ++S.Problems;
                S.Phase = 4; S.Step = Now;   // wait for the swing
                return;
            }
            const bool bDid = Door->Interact(Pawn);
            const bool bHeld = Door->bLocked && !bDid && !Door->IsOpen();
            S.bLockedHeld &= bHeld;
            S.bReasons &= !Door->LockedReason.IsEmpty();
            Note(FString::Printf(TEXT("stop %d: %s -> %s, reason \"%s\""), S.Stop, St.Name, bHeld ? TEXT("stayed locked") : TEXT("OPENED"), *Door->LockedReason));
            Advance();
            return;
        }
        // A walk.
        S.Goal = FVector(World2(St.At), Feet.Z + Half);
        S.Best = 1e9f;
        S.Progress = Now;
        S.Phase = 2;
        return;
    }
    if (S.Phase == 4)
    {
        // The pair should both be open after the swing.
        if (Now - S.Step < 1.6) return;
        const CR::FStop& OpenStop=Route[S.Stop];
        const bool Main=FCString::Strcmp(OpenStop.Door,TEXT("MainA"))==0;
        ACampusDoor* A = FindDoor(OpenStop.Door);
        ACampusDoor* B = Main ? FindDoor(TEXT("MainB")) : nullptr;
        if(bRow&&FString(OpenStop.Door).EndsWith(TEXT("A")))
        { FString Pair(OpenStop.Door);Pair[Pair.Len()-1]=TEXT('B');B=FindDoor(*Pair); }
        const bool Pair=Main||(bRow&&FString(OpenStop.Door).EndsWith(TEXT("A")));
        const bool Open=A&&A->IsOpen()&&(!Pair||(B&&B->IsOpen()));
        if(Main){++S.DoorsChecked;S.DoorsOk += A&&B&&Open ? 1:0;}
        else if(!Open)++S.Problems;
        Note(FString::Printf(TEXT("doors %s open: A=%d B=%d"),OpenStop.Door,A?int32(A->IsOpen()):-1,B?int32(B->IsOpen()):-1));
        Advance();
        return;
    }
    if (S.Phase == 2)
    {
        const CR::FStop& St = Route[S.Stop];
        PC->SetControlRotation((S.Goal - Feet).GetSafeNormal2D().Rotation());
        const float Dist = float(FVector::Dist2D(Feet, S.Goal));
        if (Dist < S.Best - (bRow ? 4.f : 25.f)) { S.Best = Dist; S.Progress = Now; }
        const CR::FStop& Prev = Route.IsValidIndex(S.Stop - 1) ? Route[S.Stop - 1] : St;
        if (Feet.Z < FMath::Min(Datum(Prev), Datum(St)) - 340.f) { ++S.Problems; Note(FString::Printf(TEXT("problem: FELL on the way to %s, feet at %s"), St.Name, *Feet.ToString())); S.Phase = 1; S.Stop = Route.Num() - 1; return; }
        if (Dist < (bRow ? 12.f : 60.f))
        {
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            ++S.Reached;
            const float Want = HouseCampusInteriors::InteriorFloorZ(S.Seed, St.Floor);
            // (Floor -1: a stop on a stair or a lecture hall's tiers, between the floors; only reaching it counts.)
            const bool bOnFloor = St.Floor < 0 || FMath::Abs(Feet.Z - Want) < 25.f || (St.Floor == 0 && FMath::Abs(Feet.Z - Datum(St)) < 20.f);
            const bool bIn = HouseCampusInteriors::DistanceOutside(View) < 1.f;
            // An outside stop: past the main wall of a plan block; for a free-standing interior, wherever its own test says.
            const bool bOutStop = S.Seed >= HouseCampusInteriors::FreeSeedBase ? !HouseCampusInteriors::InsideInterior(S.Seed, Feet + FVector(0, 0, 100.f))
                : St.At.Y > HouseCampusInteriors::CanonRect(S.Seed).Max.Y;
            if (!bOnFloor) { ++S.Problems; Note(FString::Printf(TEXT("problem: %s reached at Z=%.1f, not on floor %d (%.0f)"), St.Name, Feet.Z, St.Floor, Want)); }
            if (bOutStop) { ++S.OutsideChecks; S.OutsideOk += !bIn ? 1 : 0; }
            else
            {
                ++S.InsideChecks; S.InsideOk += bIn ? 1 : 0;
                if (!Interiors || !Interiors->Loaded().Contains(S.Seed)) S.bLoadedWhileIn = false;
            }
            Note(FString::Printf(TEXT("stop %d: reached %s, feet at (%.0f,%.0f,%.1f), indoor=%d, interiorLights=%d"), S.Stop, St.Name, Feet.X, Feet.Y, Feet.Z, int32(bIn), Interiors ? Interiors->ActiveLights() : -1));
            if (St.Shot)
            {
                // Face the room the shot is for (the stop's look direction, in the building's frame), then take it.
                PC->SetControlRotation(FRotator(-4.f, St.LookYaw + Frame.Rotator().Yaw, 0.f));
                S.Phase = 5;
                S.Step = Now;
                return;
            }
            Advance();
            return;
        }
        if (Now - S.Progress > 7.0 || (bRow && Now - S.Step > 30.0))
        {
            FCollisionQueryParams Q(SCENE_QUERY_STAT(CampusInteriorsBlocker), false, Pawn);
            FHitResult Hit;
            const FVector Dir = (S.Goal - Feet).GetSafeNormal2D();
            const FVector Centre = Pawn->GetActorLocation();
            const bool bHit = W->SweepSingleByChannel(Hit, Centre, Centre + Dir * 120.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(), Half - 5.f), Q);
            ++S.Problems;
            Note(FString::Printf(TEXT("problem: STALLED on the way to %s at (%.0f,%.0f,%.1f), %.0f cm short, blocked by %s/%s"), St.Name, Feet.X, Feet.Y, Feet.Z, Dist,
                bHit ? *GetNameSafe(Hit.GetActor()) : TEXT("nothing"), bHit ? *GetNameSafe(Hit.GetComponent()) : TEXT("nothing")));
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Advance();
        }
        return;
    }
    if (S.Phase == 5)
    {
        if (Now - S.Step < .5) return;
        if (S.Step >= 0.0) { FScreenshotRequest::RequestScreenshot(S.Dir / Route[S.Stop].Shot, false, false); S.Step = -Now; return; }
        if (Now + S.Step < .4) return;
        Advance();
        return;
    }
    if (S.Phase == 3)
    {
        // Away at the house: after the free delay the library's inside must be gone and the lights off.
        if (Now - S.Step < 14.0) return;
        ++S.FreedChecks;
        S.FreedOk += Interiors && !Interiors->Loaded().Contains(S.Seed) && Interiors->ActiveLights() == 0 ? 1 : 0;
        Note(FString::Printf(TEXT("after leaving: built=%s lights=%d"), Interiors && Interiors->Loaded().Contains(S.Seed) ? TEXT("still") : TEXT("freed"), Interiors ? Interiors->ActiveLights() : -1));
        S.Stop = Route.Num();
        S.Phase = 1;
        S.Step = Now - 1.0;
    }
}
#else
void TickHouseCampusInteriorsReview(UWorld*) {}
#endif
