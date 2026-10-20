// Calder State's Tier B and Tier C buildings (master job, step 2d), all from one plan generator.
//
// Tier B (Old Main, the Commons, Ashby Hall, Calder Hall, Kerr Hall, Whitcomb Fine Arts): the ground floor walk-in. A lobby
// at the main doors (the bays round them), a corridor across the building on two piers of the end walls, a row of rooms
// along the front between the corridor and the windows, and a row along the back. Each building's rooms are a spec: tokens
// "<kind><bays>[:NAME]" west to east in its canonical frame ("L" marks the lobby). Rooms that would be too deep for what
// they are stop short of the back wall, and what is left behind them stays closed (interior-mapped windows). Locked rooms
// are unfurnished, their windows interior-mapped, their doors' reasons shown.
// Tier C (the health center, the fieldhouse, the music hall, engineering, the physical plant, Alumni Hall): only a lit
// lobby: directory, notices, payphone, ash urn, a display of its own, and locked doors (CLOSED SATURDAY, AUTHORIZED
// PERSONNEL, a chain and padlock).
// Room plates and the directory are painted from the same plan, so they agree.
#include "HouseCampusInteriorsRooms.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusFallKit.h"
#include "Engine/Texture2D.h"

namespace HouseCampusInteriorsImpl
{
/** Kinds: C classroom, S seminar, O office (locked), F office (open), D dean's office (paneled), R registrar, W waiting room,
 *  U lounge, N dining hall, K kitchen (locked), A laundry, E rec room, V auditorium, T studio, G gallery, Q dorm room (open),
 *  Y dorm room (locked), X locked room, Z storage (locked). Display: Tier C lobbies' own things (and a few Tier B extras). */
struct FGenSpec { const TCHAR* Id; bool bTierC; const TCHAR* Front; const TCHAR* Back; int32 Style; };
const FGenSpec GenSpecs[] = {
    { TEXT("MAIN"), false, TEXT("D2:DEAN_OF_STUDENTS W2:ADMISSIONS L R3:REGISTRAR X1:RECORDS"), TEXT("X3:BURSAR S2:CONFERENCE_ROOM O1:FINANCIAL_AID O1:COUNSELING X3:PRESIDENT"), 2 },
    { TEXT("COM"), false, TEXT("U3:THE_HEARTH L X2:MANAGER"), TEXT("N5:DINING_HALL K2:KITCHEN"), 0 },
    { TEXT("ASH"), false, TEXT("Q1 Y1 Q1 Y1 A1:LAUNDRY L U2:LOUNGE Y1 Q1"), TEXT("E4:RECREATION U3:STUDY_LOUNGE Y2 Z2:STORAGE"), 1 },
    { TEXT("HUM"), false, TEXT("F1:ENGLISH O1 S1:SEMINAR L S1:SEMINAR O1 F1:HISTORY"), TEXT("C3:CLASSROOM V3:AUDITORIUM C2:CLASSROOM"), 0 },
    { TEXT("KER"), false, TEXT("C2:CLASSROOM O1 O1 L C2:CLASSROOM S2:SEMINAR"), TEXT("C3:CLASSROOM C2:CLASSROOM V3:LECTURE_ROOM C2:CLASSROOM"), 0 },
    { TEXT("ART"), false, TEXT("T2:PAINTING T2:DRAWING O1 L G4:WHITCOMB_GALLERY"), TEXT("T3:CERAMICS X2:KILN_ROOM T3:SCULPTURE Z3:STORAGE"), 0 },
    { TEXT("HLTH"), true, TEXT("L"), TEXT(""), 1 },
    { TEXT("FLD"), true, TEXT("L"), TEXT(""), 1 },
    { TEXT("MUS"), true, TEXT("L"), TEXT(""), 0 },
    { TEXT("ENG"), true, TEXT("L"), TEXT(""), 0 },
    { TEXT("PLT"), true, TEXT("L"), TEXT(""), 1 },
    { TEXT("ALU"), true, TEXT("L"), TEXT(""), 2 },
};
const FGenSpec* GenSpecFor(const TCHAR* Id)
{
    for (const FGenSpec& S : GenSpecs) if (FCString::Strcmp(S.Id, Id) == 0) return &S;
    return nullptr;
}
float GenMaxDepth(TCHAR K)
{
    switch (K)
    {
    case 'N': case 'E': return 2000.f;
    case 'V': return 2200.f;
    case 'U': return 1600.f;
    case 'T': return 1500.f;
    case 'G': case 'K': return 1200.f;
    case 'C': return 950.f;
    case 'R': case 'X': return 900.f;
    case 'Q': case 'Y': return 600.f;
    case 'O': case 'F': case 'A': return 700.f;
    default: return 800.f;
    }
}
bool GenOpenKind(TCHAR K) { return !(K == 'O' || K == 'K' || K == 'Y' || K == 'X' || K == 'Z'); }
bool GenWideDoor(TCHAR K) { return K == 'N' || K == 'V' || K == 'E' || K == 'G' || K == 'U'; }
const TCHAR* GenKindName(TCHAR K)
{
    switch (K)
    {
    case 'C': return TEXT("CLASSROOM"); case 'S': return TEXT("SEMINAR ROOM"); case 'O': case 'F': return TEXT("OFFICE");
    case 'D': return TEXT("DEAN"); case 'R': return TEXT("REGISTRAR"); case 'W': return TEXT("WAITING ROOM"); case 'U': return TEXT("LOUNGE");
    case 'N': return TEXT("DINING HALL"); case 'K': return TEXT("KITCHEN"); case 'A': return TEXT("LAUNDRY"); case 'E': return TEXT("RECREATION");
    case 'V': return TEXT("AUDITORIUM"); case 'T': return TEXT("STUDIO"); case 'G': return TEXT("GALLERY"); case 'Z': return TEXT("STORAGE");
    default: return TEXT("");
    }
}

struct FGenRoom
{
    TCHAR Kind = 'X';
    FString Name;
    int32 Number = 0, Bay0 = 0, Bay1 = 0;
    bool bFront = true, bOpen = false;
    FBox2D R;
    float DoorU0 = 0.f, DoorU1 = 0.f;
    FString Reason;
};
struct FGenPlan
{
    const FGenSpec* Spec = nullptr;
    FBox2D In;
    float CorFront = 0.f, CorBack = 0.f, LobbyX0 = 0.f, LobbyX1 = 0.f, F0 = 0.f, Ceil = 0.f;
    TArray<float> Edges;          // bay edges along X: the inner faces at the ends, the piers between
    TArray<FVector2D> Columns;    // the front wall's window columns
    TArray<FGenRoom> Rooms;
    TArray<FVector2D> LobbyDoors; // Tier C: the lobby's locked doors in its back wall (U ranges)
    TArray<FString> LobbyReasons;
};

/** The plan of a generic building by seed (worked out once). */
const FGenPlan* GenPlan(int32 Seed)
{
    // (Each plan on the heap: the map may move its entries as it grows, the plans must not move.)
    static TMap<int32, TUniquePtr<FGenPlan>> Cache;
    if (const TUniquePtr<FGenPlan>* Found = Cache.Find(Seed)) return (*Found)->Spec ? Found->Get() : nullptr;
    FGenPlan& P = *Cache.Add(Seed, MakeUnique<FGenPlan>());
    const HouseCampusLayout::FBlock* B = Block(Seed);
    const FGenSpec* S = B ? GenSpecFor(B->Id) : nullptr;
    if (!S) return nullptr;
    P.Spec = S;
    const FBox2D C = HouseCampusInteriors::CanonRect(Seed);
    P.In = FBox2D(C.Min + FVector2D(WallT), C.Max - FVector2D(WallT));
    P.F0 = HouseCampusInteriors::FloorTop(B->Height, 0);
    P.Ceil = HouseCampusInteriors::FloorTop(B->Height, 1) - 30.f;
    HouseCampusInteriors::WindowColumns(Seed, 0, P.Columns);
    P.Edges.Add(P.In.Min.X);
    for (int32 K = 0; K + 1 < P.Columns.Num(); ++K) P.Edges.Add((P.Columns[K].Y + P.Columns[K + 1].X) * .5f);
    P.Edges.Add(P.In.Max.X);
    const int32 Bays = P.Edges.Num() - 1;
    // The lobby: every bay the main doors and their frames reach into.
    int32 L0 = Bays, L1 = -1;
    for (int32 K = 0; K < Bays; ++K) if (P.Edges[K] < 135.f && P.Edges[K + 1] > -135.f) { L0 = FMath::Min(L0, K); L1 = FMath::Max(L1, K); }
    if (L0 == L1) { if (L1 + 1 < Bays) ++L1; else if (L0 > 0) --L0; }   // at least two bays: room for the doors and the lobby's things
    P.LobbyX0 = P.Edges[L0]; P.LobbyX1 = P.Edges[L1 + 1];
    // The corridor: on two piers of the end walls, the front row about 7 m deep.
    TArray<FVector2D> SideCols;
    HouseCampusInteriors::WindowColumns(Seed, 2, SideCols);
    TArray<float> SidePiers;
    for (int32 K = 0; K + 1 < SideCols.Num(); ++K) SidePiers.Add((SideCols[K].Y + SideCols[K + 1].X) * .5f);
    float Best = 1e9f;
    for (int32 K = 1; K < SidePiers.Num(); ++K)
    {
        const float Depth = P.In.Max.Y - SidePiers[K], Score = FMath::Abs(Depth - 700.f) + (Depth < 520.f ? 1000.f : 0.f);
        if (Score < Best) { Best = Score; P.CorFront = SidePiers[K]; P.CorBack = SidePiers[K - 1]; }
    }
    if (Best > 1e8f) { P.CorFront = P.In.Max.Y - 700.f; P.CorBack = P.CorFront - 340.f; }
    // The rooms.
    auto Parse = [&](const TCHAR* Text, bool bFront)
    {
        TArray<FString> Tokens;
        FString(Text).ParseIntoArrayWS(Tokens);
        // Runs: the front band fills bays 0..L0-1 before the lobby and L1+1.. after; the back band all of them.
        int32 Run1 = bFront ? L0 : Bays, Next = 0;
        int32 Odd = 101, Even = 102;
        TArray<FString> Run;
        auto Flush = [&]()
        {
            for (int32 I = 0; I < Run.Num(); ++I)
            {
                FGenRoom Rm;
                Rm.Kind = Run[I][0];
                FString Name;
                const FString Body = Run[I].Mid(1);
                int32 Colon = INDEX_NONE;
                const int32 Count = Body.FindChar(TEXT(':'), Colon) ? FCString::Atoi(*Body.Left(Colon)) : FCString::Atoi(*Body);
                if (Colon != INDEX_NONE) Name = Body.Mid(Colon + 1).Replace(TEXT("_"), TEXT(" "));
                Rm.Bay0 = Next;
                Rm.Bay1 = I == Run.Num() - 1 ? Run1 - 1 : FMath::Min(Run1 - 1, Next + FMath::Max(1, Count) - 1);
                if (Rm.Bay0 > Rm.Bay1) continue;
                Next = Rm.Bay1 + 1;
                Rm.bFront = bFront;
                Rm.Number = bFront ? Odd : Even;
                if (bFront) Odd += 2; else Even += 2;
                Rm.Name = Name.IsEmpty() ? FString(GenKindName(Rm.Kind)) : Name;
                Rm.bOpen = GenOpenKind(Rm.Kind);
                const float X0 = P.Edges[Rm.Bay0] + (Rm.Bay0 == 0 ? 0.f : PartT * .5f), X1 = P.Edges[Rm.Bay1 + 1] - (Rm.Bay1 + 1 == Bays ? 0.f : PartT * .5f);
                if (bFront) Rm.R = Rect(X0, X1, P.CorFront + PartT * .5f, P.In.Max.Y);
                else Rm.R = Rect(X0, X1, FMath::Max(P.In.Min.Y, P.CorBack - PartT * .5f - GenMaxDepth(Rm.Kind)), P.CorBack - PartT * .5f);
                Rm.DoorU0 = P.Edges[Rm.Bay0] + 30.f;
                Rm.DoorU1 = Rm.DoorU0 + (GenWideDoor(Rm.Kind) ? 200.f : 92.f);
                P.Rooms.Add(Rm);
            }
            Run.Reset();
        };
        for (const FString& T : Tokens)
        {
            if (T == TEXT("L")) { Flush(); Run1 = Bays; Next = L1 + 1; continue; }
            Run.Add(T);
        }
        Flush();
    };
    if (!S->bTierC) { Parse(S->Front, true); Parse(S->Back, false); }
    // Locked rooms' reasons, by kind, varied by number.
    for (FGenRoom& Rm : P.Rooms)
    {
        if (Rm.bOpen) continue;
        switch (Rm.Kind)
        {
        case 'O': Rm.Reason = FString::Printf(TEXT("%d %s - office hours Monday to Friday"), Rm.Number, *Rm.Name); break;
        case 'K': Rm.Reason = TEXT("KITCHEN - STAFF ONLY"); break;
        case 'Z': Rm.Reason = TEXT("STORAGE - AUTHORIZED PERSONNEL ONLY"); break;
        case 'Y': Rm.Reason = FString::Printf(TEXT("Room %d - %s"), Rm.Number, (Rm.Number / 2) % 2 ? TEXT("nobody home, they're at the game") : TEXT("DO NOT DISTURB on the knob")); break;
        default: Rm.Reason = FString::Printf(TEXT("%s - CLOSED SATURDAY"), *Rm.Name); break;
        }
    }
    if (S->bTierC)
    {
        // The lobby's locked doors in its back wall: two, the second chained.
        const float Mid = (P.LobbyX0 + P.LobbyX1) * .5f;
        P.LobbyDoors = { FVector2D(Mid - 250.f, Mid - 158.f), FVector2D(Mid + 158.f, Mid + 250.f) };
        if (FCString::Strcmp(S->Id, TEXT("HLTH")) == 0) P.LobbyReasons = { TEXT("EXAM ROOMS - CLOSED SATURDAY. EMERGENCIES: CAMPUS SECURITY, EXT 2911"), TEXT("PHARMACY - AUTHORIZED PERSONNEL ONLY") };
        else if (FCString::Strcmp(S->Id, TEXT("FLD")) == 0) P.LobbyReasons = { TEXT("LOCKER ROOMS - TEAMS ONLY"), TEXT("THE GYM - chained and padlocked for the weekend") };
        else if (FCString::Strcmp(S->Id, TEXT("MUS")) == 0) P.LobbyReasons = { TEXT("BAND ROOM - AUTHORIZED PERSONNEL"), TEXT("PRACTICE ROOMS - chained shut until Monday") };
        else if (FCString::Strcmp(S->Id, TEXT("ENG")) == 0) P.LobbyReasons = { TEXT("LABS - AUTHORIZED PERSONNEL ONLY"), TEXT("MACHINE SHOP - chain and padlock, CLOSED SATURDAY") };
        else if (FCString::Strcmp(S->Id, TEXT("PLT")) == 0) P.LobbyReasons = { TEXT("BOILER ROOM - AUTHORIZED PERSONNEL ONLY"), TEXT("SHOP - chained, CLOSED SATURDAY") };
        else P.LobbyReasons = { TEXT("ALUMNI OFFICE - CLOSED SATURDAY, SEE YOU AT THE TENT"), TEXT("ARCHIVES - chained, BY APPOINTMENT") };
    }
    return &P;
}

const FGenRoom* GenRoomAtFront(const FGenPlan& P, float U)
{
    for (const FGenRoom& Rm : P.Rooms) if (Rm.bFront && U >= Rm.R.Min.X - 20.f && U <= Rm.R.Max.X + 20.f) return &Rm;
    return nullptr;
}
const FGenRoom* GenRoomAtBack(const FGenPlan& P, float U)
{
    for (const FGenRoom& Rm : P.Rooms) if (!Rm.bFront && U >= Rm.R.Min.X - 20.f && U <= Rm.R.Max.X + 20.f) return &Rm;
    return nullptr;
}

// ------------------------------------------------------------------ fittings the rooms use
void GenEasel(FHallMesh& Solid, FHallMesh& Clutter, const FVector& C, float Yaw, FName Canvas)
{
    const FRotator R(0.f, Yaw, 0.f);
    const FVector F = R.Vector(), S = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
    for (const float Side : { -1.f, 1.f }) HouseCampusFallKit::Bar(Solid, TEXT("CI_Oak"), C + S * (Side * 28.f) + F * 10.f, C + FVector(0, 0, 170.f), 3.f, 2.f, F);
    HouseCampusFallKit::Bar(Solid, TEXT("CI_Oak"), C - F * 40.f, C + FVector(0, 0, 165.f), 3.f, 2.f, S);
    Solid.Box(TEXT("CI_Oak"), FTransform(R, C + F * 8.f + FVector(0, 0, 88.f)), FVector(3.f, 32.f, 1.5f), UVPlain());
    HouseCampusFallKit::Card(Clutter, Canvas, C + F * 10.f + FVector(0, 0, 128.f), -S, FVector::UpVector, 60.f, 76.f, F);
    Clutter.Box(TEXT("CI_Paper"), FTransform(R, C + F * 8.5f + FVector(0, 0, 128.f)), FVector(1.f, 31.f, 39.f), UVPlain());
}
void GenPingPong(FHallMesh& Solid, FHallMesh& Clutter, const FVector& T)
{
    Box(Solid, TEXT("CI_PingPong"), T + FVector(-137.f, -76.f, 73.f), T + FVector(137.f, 76.f, 76.f));
    Box(Clutter, TEXT("CI_Paper"), T + FVector(-137.f, -.5f, 76.f), T + FVector(137.f, .5f, 76.1f));
    for (const float S : { -1.f, 1.f }) for (const float S2 : { -1.f, 1.f }) Box(Solid, TEXT("CI_Steel"), T + FVector(S * 110.f - 2.f, S2 * 60.f - 2.f, 0.f), T + FVector(S * 110.f + 2.f, S2 * 60.f + 2.f, 73.f));
    Box(Clutter, TEXT("CI_Net"), T + FVector(-.3f, -88.f, 76.f), T + FVector(.3f, 88.f, 91.f));
}
/** A chain across a doorway at waist height and a padlock (the door is locked; this says why at a glance). */
void GenChain(FHallMesh& Clutter, const FVector& A, const FVector& B, const FVector& Out)
{
    const int32 N = 14;
    for (int32 I = 0; I < N; ++I)
    {
        const FVector P0 = HouseCampusFallKit::Cord(A, B, float(I) / N, 12.f), P1 = HouseCampusFallKit::Cord(A, B, float(I + 1) / N, 12.f);
        HouseCampusFallKit::Bar(Clutter, TEXT("CI_Steel"), P0 + Out * 2.f, P1 + Out * 2.f, 1.4f, (I % 2) ? .5f : 1.4f, Out);
    }
    const FVector Lock = HouseCampusFallKit::Cord(A, B, .5f, 12.f) + Out * 3.f - FVector(0, 0, 8.f);
    Clutter.Box(TEXT("CI_Brass"), FTransform(Out.Rotation(), Lock), FVector(1.5f, 3.5f, 4.f), UVPlain());
    Clutter.Cylinder(TEXT("CI_Steel"), Lock + FVector(0, -2.f, 4.f), Lock + FVector(0, 2.f, 4.f), 2.5f, 8, UVPlain());
}

// ------------------------------------------------------------------ furnishing a room
void GenFurnish(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, const FGenPlan& P, const FGenRoom& Rm, int32 Seed)
{
    using EKit = HouseCampusInteriors::EKit;
    const float Z = P.F0;
    // Fronts: the side wall away from the door (west for the front row, east for the back), so the open side faces it.
    const int32 Side = Rm.bFront ? 2 : 3;
    const FName Boards[] = { TEXT("CI_ChalkPhysics"), TEXT("CI_ChalkSeminar"), TEXT("CI_GenChalkHistory"), TEXT("CI_GenChalkEnglish"), TEXT("CI_ChalkChem") };
    const FName Board = Boards[(Rm.Number + Seed) % UE_ARRAY_COUNT(Boards)];
    const FVector2D Ctr = Rm.R.GetCenter();
    switch (Rm.Kind)
    {
    case 'C': RoomClassroom(Solid, Clutter, Kit, Rm.R, Z, Side, Board, Rm.Number + Seed); break;
    case 'S': RoomSeminar(Solid, Clutter, Kit, Rm.R, Z, Side, Board, Rm.Number + Seed); break;
    case 'F': RoomOffice(Solid, Clutter, Glow, Kit, Rm.R, Z, Side, Rm.Number + Seed); break;
    case 'W': RoomWaiting(Solid, Clutter, Glow, Kit, Rm.R, Z, Side, Rm.Number + Seed); break;
    case 'U': RoomLounge(Solid, Glow, Kit, Rm.R, Z, Side, Rm.Number + Seed); break;
    case 'N': RoomDining(Solid, Glow, Kit, Rm.R, Z, Rm.bFront ? 0 : 1, Rm.Number + Seed); break;
    case 'D':
    {
        // The dean's office: walnut paneling round the walls (not over the windows), a rug, the desk and chairs, club chairs
        // by the bookcases, the college's founder in oils.
        RoomOffice(Solid, Clutter, Glow, Kit, Rm.R, Z, Side, Rm.Number + Seed);
        const FBox2D& R = Rm.R;
        for (float X = R.Min.X + 2.f; X < R.Max.X - 2.f; X += 61.f)
        {
            if (X + 60.f > Rm.DoorU0 - 12.f && X < Rm.DoorU1 + 12.f) continue;   // the doorway and its frame
            Box(Solid, TEXT("CI_Walnut"), FVector(X, Rm.bFront ? R.Min.Y : R.Max.Y - 1.5f, Z), FVector(FMath::Min(R.Max.X - 2.f, X + 60.f), Rm.bFront ? R.Min.Y + 1.5f : R.Max.Y, Z + 250.f));
        }
        for (const float X : { R.Min.X, R.Max.X - 1.5f }) for (float Y = R.Min.Y + 2.f; Y < R.Max.Y - 2.f; Y += 61.f) Box(Solid, TEXT("CI_Walnut"), FVector(X, Y, Z), FVector(X + 1.5f, FMath::Min(R.Max.Y - 2.f, Y + 60.f), Z + 82.f));
        Kit.Floor(EKit::StudyRug, Ctr, Z, 0.f, false);
        Kit.Floor(EKit::ClubChair, Ctr + FVector2D(R.GetExtent().X * .45f, -R.GetExtent().Y * .35f), Z, 135.f, true);
        Kit.Floor(EKit::ClubChair, Ctr + FVector2D(R.GetExtent().X * .45f, R.GetExtent().Y * .1f), Z, -135.f, true);
        Kit.Floor(EKit::SideTable, Ctr + FVector2D(R.GetExtent().X * .55f, -R.GetExtent().Y * .12f), Z, 0.f, true);
        Kit.Wall(EKit::HistoricPhoto, FVector(R.Max.X - 2.f, Ctr.Y, Z + 170.f), FVector(-1, 0, 0));
        break;
    }
    case 'R':
    {
        // The registrar: the long counter with its glass and three windows facing the door side, staff desks behind it with
        // typewriters and a terminal, filing cabinets, the queue rope, the forms rack; CLOSED SATURDAY on the counter.
        const FBox2D& R = Rm.R;
        const float CY = Rm.bFront ? R.Min.Y + 260.f : R.Max.Y - 260.f, Dir = Rm.bFront ? 1.f : -1.f;
        Box(Solid, TEXT("CI_Oak"), FVector(R.Min.X + 40.f, CY - 25.f, Z), FVector(R.Max.X - 120.f, CY + 25.f, Z + 105.f));
        Box(Solid, TEXT("CI_OakDark"), FVector(R.Min.X + 36.f, CY - 30.f, Z + 105.f), FVector(R.Max.X - 116.f, CY + 30.f, Z + 109.f));
        Box(Solid, TEXT("CI_Glass"), FVector(R.Min.X + 40.f, CY - .5f, Z + 109.f), FVector(R.Max.X - 120.f, CY + .5f, Z + 230.f));
        for (int32 I = 0; I < 3; ++I)
        {
            const float X = FMath::Lerp(R.Min.X + 120.f, R.Max.X - 200.f, I / 2.f);
            Box(Solid, TEXT("CI_OakDark"), FVector(X - 45.f, CY - 2.f, Z + 109.f), FVector(X - 40.f, CY + 2.f, Z + 230.f));
            Box(Solid, TEXT("CI_OakDark"), FVector(X + 40.f, CY - 2.f, Z + 109.f), FVector(X + 45.f, CY + 2.f, Z + 230.f));
            HouseCampusFallKit::Card(Clutter, I == 1 ? TEXT("CI_GenClosedSat") : TEXT("CI_GenWindow"), FVector(X, CY - Dir * 1.f, Z + 190.f), FVector(-Dir, 0, 0), FVector::UpVector, 60.f, 20.f, FVector(0, -Dir, 0));
            Typewriter(Solid, Clutter, FVector(X, CY + Dir * 170.f, Z + 76.f), FVector(0, -Dir, 0));
            Table(Solid, FVector(X, CY + Dir * 170.f, Z), 140.f, 70.f, 76.f, true);
            Kit.Floor(EKit::GrayOfficeChair, FVector2D(X, CY + Dir * 225.f), Z, Dir > 0.f ? -90.f : 90.f, true);
            DeskPhone(Solid, FVector(X + 45.f, CY + Dir * 160.f, Z + 76.f), FVector(0, -Dir, 0), I == 0);
        }
        Terminal(Solid, Glow, FVector(R.Max.X - 260.f, CY + Dir * 160.f, Z + 76.f), FVector(0, -Dir, 0));
        for (float X = R.Min.X + 40.f; X < R.Max.X - 150.f; X += 60.f) Box(Solid, TEXT("CI_Steel"), FVector(X, Rm.bFront ? R.Max.Y - 50.f : R.Min.Y, Z), FVector(X + 50.f, Rm.bFront ? R.Max.Y : R.Min.Y + 50.f, Z + 132.f));
        for (const float X : { R.Min.X + 120.f, R.Min.X + 300.f })
        {
            Solid.Cylinder(TEXT("CI_Brass"), FVector(X, CY - Dir * 120.f, Z), FVector(X, CY - Dir * 120.f, Z + 95.f), 3.f, 8, UVPlain());
            Solid.Cylinder(TEXT("CI_Brass"), FVector(X, CY - Dir * 120.f, Z), FVector(X, CY - Dir * 120.f, Z + 3.f), 16.f, 12, UVPlain());
        }
        Clutter.Cylinder(TEXT("CI_StreamerPurple"), FVector(R.Min.X + 120.f, CY - Dir * 120.f, Z + 90.f), FVector(R.Min.X + 300.f, CY - Dir * 120.f, Z + 86.f), 1.5f, 8, UVPlain());
        BulletinBoard(Solid, Clutter, FVector(R.Min.X, Ctr.Y - Dir * 60.f, Z + 140.f), FVector(1, 0, 0), 100.f, 120.f, TEXT("CI_NoticeBoard"));
        HangingSign(Clutter, TEXT("CI_GenRegistrarSign"), FVector(Ctr.X, CY - Dir * 30.f, P.Ceil - 50.f), FVector(0, -Dir, 0), 240.f, 36.f, P.Ceil);
        break;
    }
    case 'A':
        for (int32 I = 0; I < 4; ++I) Laundry(Solid, Glow, FVector(Rm.R.Min.X + 45.f + 72.f * I, Rm.bFront ? Rm.R.Max.Y - 36.f : Rm.R.Min.Y + 36.f, Z), FVector(0, Rm.bFront ? -1.f : 1.f, 0), I >= 2);
        Table(Solid, FVector(Ctr, Z), 160.f, 70.f, 86.f, true);
        Kit.Floor(EKit::LaundryBasket, Ctr, Z + 86.f, 20.f, true);
        break;
    case 'E':
        GenPingPong(Solid, Clutter, FVector(Ctr.X - Rm.R.GetExtent().X * .4f, Ctr.Y, Z));
        Kit.Floor(EKit::BilliardTable, Ctr + FVector2D(Rm.R.GetExtent().X * .35f, 0.f), Z, 90.f, false);
        Pinball(Solid, Glow, FVector(Rm.R.Max.X - 70.f, Rm.bFront ? Rm.R.Max.Y - 60.f : Rm.R.Min.Y + 70.f, Z), FVector(-1, 0, 0), TEXT("CI_BackglassMoon"));
        Kit.Floor(EKit::LeatherSofa, FVector2D(Rm.R.Min.X + 60.f, Ctr.Y), Z, 0.f, true);
        break;
    case 'V':
    {
        // A flat-floored auditorium: the stage at the far end, curtains, a lectern; rows of seats facing it, a centre aisle.
        const FBox2D& R = Rm.R;
        const float Dir = Rm.bFront ? 1.f : -1.f, StageY = Rm.bFront ? R.Max.Y : R.Min.Y;
        Box(Solid, TEXT("CI_OakDark"), FVector(R.Min.X, FMath::Min(StageY, StageY - Dir * 350.f), Z), FVector(R.Max.X, FMath::Max(StageY, StageY - Dir * 350.f), Z + 75.f));
        for (int32 K = 0; K < 5; ++K) Box(Solid, TEXT("CI_OakDark"), FVector(Ctr.X - 80.f, FMath::Min(StageY - Dir * 350.f, StageY - Dir * (350.f + 30.f * (5 - K))), Z), FVector(Ctr.X + 80.f, FMath::Max(StageY - Dir * 350.f, StageY - Dir * (350.f + 30.f * (5 - K))), Z + 15.f * (K + 1)));
        Box(Clutter, TEXT("CI_SciDrape"), FVector(R.Min.X + 10.f, FMath::Min(StageY - Dir * 5.f, StageY - Dir * 25.f), Z + 75.f), FVector(R.Max.X - 10.f, FMath::Max(StageY - Dir * 5.f, StageY - Dir * 25.f), P.Ceil - 5.f));
        Box(Solid, TEXT("CI_Oak"), FVector(Ctr.X + 150.f, StageY - Dir * 200.f - 20.f, Z + 75.f), FVector(Ctr.X + 200.f, StageY - Dir * 200.f + 20.f, Z + 185.f));
        for (float Y = StageY - Dir * 520.f; Rm.bFront ? Y > R.Min.Y + 200.f : Y < R.Max.Y - 200.f; Y -= Dir * 95.f)
            for (float X = R.Min.X + 60.f; X < R.Max.X - 60.f; X += 55.f)
            {
                if (FMath::Abs(X - Ctr.X) < 70.f) continue;
                Box(Solid, TEXT("CI_StreamerPurple"), FVector(X - 23.f, Y - 20.f, Z + 42.f), FVector(X + 23.f, Y + 20.f, Z + 48.f));
                Box(Solid, TEXT("CI_StreamerPurple"), FVector(X - 23.f, Y - Dir * 22.f - 2.f, Z + 48.f), FVector(X + 23.f, Y - Dir * 22.f + 2.f, Z + 95.f));
                Box(Solid, TEXT("CI_Steel"), FVector(X - 26.f, Y - 18.f, Z), FVector(X - 23.f, Y + 18.f, Z + 62.f));
            }
        break;
    }
    case 'T':
    {
        // A studio: easels round a still-life table (paintings and drawings), wheels and shelves of pots (ceramics), stands
        // with clay heads (sculpture); a paint-spattered sink, a drying rack.
        const FBox2D& R = Rm.R;
        const FName Canvases[] = { TEXT("CI_GenArt0"), TEXT("CI_GenArt1"), TEXT("CI_GenArt2"), TEXT("CI_GenArt3"), TEXT("CI_GenArt4"), TEXT("CI_GenArt5") };
        const bool bCeramics = Rm.Name.Contains(TEXT("CERAMIC")), bSculpture = Rm.Name.Contains(TEXT("SCULPT"));
        if (bCeramics)
        {
            for (int32 I = 0; I < 4; ++I)
            {
                const FVector W(R.Min.X + 120.f + I * ((R.Max.X - R.Min.X - 240.f) / 3.f), Ctr.Y, Z);
                Solid.Cylinder(TEXT("CI_Steel"), W, W + FVector(0, 0, 50.f), 22.f, 14, UVPlain());
                Solid.Cylinder(TEXT("CI_Enamel"), W + FVector(0, 0, 50.f), W + FVector(0, 0, 53.f), 18.f, 16, UVPlain());
                HouseCampusFallKit::Lathe(Clutter, TEXT("CI_GenClay"), W + FVector(0, 0, 53.f), { FVector2D(.1f, 0.f), FVector2D(9.f, 0.f), FVector2D(11.f, 8.f), FVector2D(7.f, 16.f), FVector2D(6.f, 18.f), FVector2D(.1f, 18.f) }, 12);
                Kit.Floor(EKit::BarStool, FVector2D(W.X, W.Y - 50.f), Z, 90.f, true);
            }
            const float SY = Rm.bFront ? R.Max.Y - 30.f : R.Min.Y + 30.f;
            for (int32 Shelf = 0; Shelf < 3; ++Shelf)
            {
                Box(Solid, TEXT("CI_Oak"), FVector(R.Min.X + 40.f, SY - 25.f, Z + 40.f + Shelf * 45.f), FVector(R.Min.X + 340.f, SY + 25.f, Z + 43.f + Shelf * 45.f));
                for (int32 Pot = 0; Pot < 8; ++Pot)
                    HouseCampusFallKit::Lathe(Clutter, Pot % 3 ? TEXT("CI_GenClay") : TEXT("CI_GenGlaze"), FVector(R.Min.X + 60.f + Pot * 36.f, SY, Z + 43.f + Shelf * 45.f), { FVector2D(.1f, 0.f), FVector2D(6.f, 0.f), FVector2D(9.f, 10.f), FVector2D(5.f, 22.f), FVector2D(6.f, 25.f), FVector2D(.1f, 25.f) }, 10);
            }
        }
        else if (bSculpture)
        {
            for (int32 I = 0; I < 5; ++I)
            {
                const FVector St(R.Min.X + 120.f + I * ((R.Max.X - R.Min.X - 240.f) / 4.f), Ctr.Y + (I % 2 ? 120.f : -120.f), Z);
                Solid.Cylinder(TEXT("CI_Oak"), St, St + FVector(0, 0, 105.f), 14.f, 10, UVPlain());
                Box(Solid, TEXT("CI_OakDark"), St + FVector(-25.f, -25.f, 105.f), St + FVector(25.f, 25.f, 109.f));
                HouseCampusFallKit::Ball(Clutter, TEXT("CI_GenClay"), St + FVector(0, 0, 130.f), 16.f, 12);
                Clutter.Cylinder(TEXT("CI_GenClay"), St + FVector(0, 0, 109.f), St + FVector(0, 0, 118.f), 7.f, 10, UVPlain());
            }
        }
        else
        {
            Table(Solid, FVector(Ctr, Z), 120.f, 80.f, 80.f, true);
            Clutter.Cylinder(TEXT("CI_Enamel"), FVector(Ctr, Z + 80.f), FVector(Ctr, Z + 86.f), 22.f, 16, UVPlain());
            for (int32 I = 0; I < 5; ++I) HouseCampusFallKit::Ball(Clutter, I % 2 ? TEXT("CI_TissueGold") : TEXT("CI_CrateRed"), FVector(Ctr + FVector2D(FMath::Cos(I * 1.3f) * 9.f, FMath::Sin(I * 1.3f) * 9.f), Z + 92.f), 5.f, 8);
            Clutter.Cylinder(TEXT("CI_SciBlueGlass"), FVector(Ctr + FVector2D(35.f, 10.f), Z + 80.f), FVector(Ctr + FVector2D(35.f, 10.f), Z + 115.f), 7.f, 12, UVPlain());
            const int32 N = 7;
            for (int32 I = 0; I < N; ++I)
            {
                const float A = PI * .15f + PI * 1.7f * I / (N - 1) + (Rm.bFront ? PI : 0.f);
                const FVector2D Off(FMath::Cos(A) * 260.f, FMath::Sin(A) * 200.f);
                const FVector2D E = Ctr + Off;
                if (!R.ExpandBy(-60.f).IsInside(E)) continue;
                GenEasel(Solid, Clutter, FVector(E, Z), FMath::RadiansToDegrees(FMath::Atan2(-Off.Y, -Off.X)), Canvases[(I + Rm.Number) % 6]);
            }
        }
        Box(Solid, TEXT("CI_Enamel"), FVector(R.Max.X - 90.f, Rm.bFront ? R.Max.Y - 60.f : R.Min.Y, Z), FVector(R.Max.X, Rm.bFront ? R.Max.Y : R.Min.Y + 60.f, Z + 90.f));   // the sink
        for (int32 I = 0; I < 3; ++I) Clutter.Cylinder(I == 1 ? TEXT("CI_StreamerGold") : TEXT("CI_CrateBlue"), FVector(R.Max.X - 60.f + I * 12.f, Ctr.Y, Z), FVector(R.Max.X - 60.f + I * 12.f, Ctr.Y, Z + 12.f), 6.f, 10, UVPlain());
        break;
    }
    case 'G':
    {
        // The gallery: the fall student show on the walls, a bench, pedestals with work on them, the show's title card.
        const FBox2D& R = Rm.R;
        const FName Works[] = { TEXT("CI_GenArt0"), TEXT("CI_GenArt1"), TEXT("CI_GenArt2"), TEXT("CI_GenArt3"), TEXT("CI_GenArt4"), TEXT("CI_GenArt5") };
        const float BackY = Rm.bFront ? R.Min.Y : R.Max.Y, Out = Rm.bFront ? 1.f : -1.f;
        int32 W = 0;
        for (float X = R.Min.X + 260.f; X < R.Max.X - 120.f; X += 220.f, ++W)
            if (X > Rm.DoorU1 + 60.f || X < Rm.DoorU0 - 60.f) HarPoster(Clutter, Works[W % 6], FVector(X, BackY + Out * .1f, P.F0 + 160.f), FVector(0, Out, 0), 90.f, 110.f);
        for (const float X : { R.Min.X + .1f, R.Max.X - .1f })
            for (float Y = R.Min.Y + 150.f; Y < R.Max.Y - 120.f; Y += 200.f)
                HarPoster(Clutter, Works[(W++) % 6], FVector(X, Y, P.F0 + 160.f), FVector(X < Ctr.X ? 1.f : -1.f, 0, 0), 80.f, 100.f);
        Box(Solid, TEXT("CI_OakDark"), FVector(Ctr.X - 120.f, Ctr.Y - 25.f, Z), FVector(Ctr.X + 120.f, Ctr.Y + 25.f, Z + 45.f));
        for (const float S : { -1.f, 1.f })
        {
            const FVector Ped(Ctr.X + S * 300.f, Ctr.Y, Z);
            Box(Solid, TEXT("CI_Enamel"), Ped - FVector(25.f, 25.f, 0.f), Ped + FVector(25.f, 25.f, 100.f));
            HouseCampusFallKit::Ball(Clutter, S > 0.f ? TEXT("CI_Brass") : TEXT("CI_GenClay"), Ped + FVector(0, 0, 118.f), 18.f, 12);
        }
        HarPoster(Clutter, TEXT("CI_GenShowCard"), FVector(R.Min.X + .1f, Ctr.Y, P.F0 + 150.f), FVector(1, 0, 0), 60.f, 40.f);
        for (float X = R.Min.X + 200.f; X < R.Max.X; X += 300.f) Glow.Box(TEXT("CI_HoodLight"), FTransform(FVector(X, BackY + Out * 60.f, P.Ceil - 8.f)), FVector(8.f, 8.f, 3.f), UVPlain());
        break;
    }
    case 'Q':
    {
        FHarFrame F;
        F.XL = Rm.R.Min.X; F.XR = Rm.R.Max.X; F.Dir = Rm.bFront ? 1.f : -1.f;
        F.YDoor = Rm.bFront ? Rm.R.Min.Y : Rm.R.Max.Y; F.Depth = Rm.R.Max.Y - Rm.R.Min.Y;
        float WinA = (Rm.R.Max.X - Rm.R.Min.X) * .5f;
        for (const FVector2D& Col : P.Columns) if (Col.X >= Rm.R.Min.X && Col.Y <= Rm.R.Max.X) WinA = (Col.X + Col.Y) * .5f - Rm.R.Min.X;
        HarFurnish(Solid, Clutter, Glow, Kit, F, Z, WinA, (Rm.Number + Seed) % 6, Rm.Number);
        break;
    }
    default: break;
    }
}

// ------------------------------------------------------------------ the building
void GenWindowTrims(FHallMesh& Solid, int32 Seed, float Height, const FBox2D& In, bool bOak)
{
    for (int32 Side = 0; Side < 4; ++Side)
    {
        TArray<FVector2D> Cols;
        HouseCampusInteriors::WindowColumns(Seed, Side, Cols);
        const float Sill = HouseCampusInteriors::WindowSill(Height, 0), Head = Sill + HouseCampusInteriors::WindowHeight;
        for (const FVector2D& C : Cols)
        {
            if (HouseCampusInteriors::WindowReplaced(Seed, Side, 0, C.X, C.Y) || !HouseCampusInteriors::WindowOpen(Seed, Side, 0, C.X, C.Y)) continue;
            auto Trim = [&](float U0, float U1, float Z0, float Z1, float D0, float D1, FName Key)
            {
                switch (Side)
                {
                case 0: Box(Solid, Key, FVector(U0, In.Max.Y - D1, Z0), FVector(U1, In.Max.Y - D0, Z1)); break;
                case 1: Box(Solid, Key, FVector(U0, In.Min.Y + D0, Z0), FVector(U1, In.Min.Y + D1, Z1)); break;
                case 2: Box(Solid, Key, FVector(In.Min.X + D0, U0, Z0), FVector(In.Min.X + D1, U1, Z1)); break;
                default: Box(Solid, Key, FVector(In.Max.X - D1, U0, Z0), FVector(In.Max.X - D0, U1, Z1)); break;
                }
            };
            Trim(C.X - 6.f, C.Y + 6.f, Sill - 4.f, Sill, -6.f, 10.f, bOak ? FName(TEXT("CI_Oak")) : FName(TEXT("CI_Beige")));
            if (bOak)
            {
                Trim(C.X - 9.f, C.X, Sill, Head + 9.f, 0.f, 2.f, TEXT("CI_OakDark"));
                Trim(C.Y, C.Y + 9.f, Sill, Head + 9.f, 0.f, 2.f, TEXT("CI_OakDark"));
                Trim(C.X - 9.f, C.Y + 9.f, Head, Head + 9.f, 0.f, 2.f, TEXT("CI_OakDark"));
            }
        }
    }
}

/** A plate beside a door: a cell of the building's plates atlas (4 x 8). */
void GenPlate(FHallMesh& M, FName Key, int32 Cell, const FVector& C, const FVector& Out)
{
    const float U0 = (Cell % 4) / 4.f, U1 = (Cell % 4 + 1) / 4.f, V0 = (Cell / 4) / 8.f, V1 = (Cell / 4 + 1) / 8.f;
    const FVector N = Out.GetSafeNormal2D(), R = FVector::CrossProduct(FVector::UpVector, -N).GetSafeNormal() * 22.f, U = FVector(0, 0, 5.5f), P = C + N * .4f;
    M.QuadUV(Key, P - R - U, P + R - U, P + R + U, P - R + U, N, FVector2D(U0, V1), FVector2D(U1, V1), FVector2D(U1, V0), FVector2D(U0, V0), R.GetSafeNormal());
}

/** The Tier C lobbies' own things, and a few extras for Tier B lobbies. */
void GenDisplay(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, const FGenPlan& P, const FString& Id)
{
    using EKit = HouseCampusInteriors::EKit;
    const float Z = P.F0, BackY = P.CorFront + PartT * .5f, X0 = P.LobbyX0 + PartT * .5f, X1 = P.LobbyX1 - PartT * .5f, MidX = (X0 + X1) * .5f;
    if (Id == TEXT("HLTH"))
    {
        // The reception window on the east wall (closed), rows of waiting chairs, the scale, posters.
        Box(Solid, TEXT("CI_Beige"), FVector(X1 - 45.f, BackY + 180.f, Z), FVector(X1, BackY + 400.f, Z + 105.f));
        Box(Solid, TEXT("CI_Glass"), FVector(X1 - 2.f, BackY + 180.f, Z + 105.f), FVector(X1 - 1.f, BackY + 400.f, Z + 220.f));
        HouseCampusFallKit::Card(Clutter, TEXT("CI_GenHealthClosed"), FVector(X1 - 2.6f, BackY + 290.f, Z + 165.f), FVector(0, 1, 0), FVector::UpVector, 90.f, 50.f, FVector(-1, 0, 0));
        for (int32 Row = 0; Row < 2; ++Row) for (int32 I = 0; I < 4; ++I)
            Kit.Floor(EKit::GreenSpindleChair, FVector2D(X0 + 70.f + I * 55.f, BackY + 220.f + Row * 90.f), Z, -90.f, true);
        Box(Solid, TEXT("CI_Steel"), FVector(X0 + 20.f, P.In.Max.Y - 60.f, Z), FVector(X0 + 60.f, P.In.Max.Y - 20.f, Z + 8.f));
        Clutter.Cylinder(TEXT("CI_Steel"), FVector(X0 + 40.f, P.In.Max.Y - 25.f, Z + 8.f), FVector(X0 + 40.f, P.In.Max.Y - 25.f, Z + 140.f), 1.5f, 8, UVPlain());
        HarPoster(Clutter, TEXT("CI_GenFluShots"), FVector(X0, BackY + 120.f, Z + 160.f), FVector(1, 0, 0), 60.f, 80.f);
    }
    else if (Id == TEXT("FLD"))
    {
        TrophyCase(Solid, Glow, FVector(X0, BackY + 350.f, Z), FVector(1, 0, 0), 300.f);   // on the west wall, clear of the doors
        HarPoster(Clutter, TEXT("CI_PosterBeat"), FVector(X1, BackY + 200.f, Z + 180.f), FVector(-1, 0, 0), 160.f, 80.f);
        HarPennant(Clutter, TEXT("CI_Pennant"), FVector(X0, BackY + 320.f, Z + 220.f), FVector(1, 0, 0), 90.f, 34.f);
        Box(Solid, TEXT("CI_Oak"), FVector(X1 - 150.f, P.In.Max.Y - 100.f, Z), FVector(X1 - 30.f, P.In.Max.Y - 60.f, Z + 105.f));   // the ticket counter
    }
    else if (Id == TEXT("MUS"))
    {
        for (int32 I = 0; I < 5; ++I)
        {
            const FVector C(X0 + 80.f + I * 70.f, P.In.Max.Y - 120.f, Z);
            Solid.Box(TEXT("CI_CrtBezel"), FTransform(FRotator(0.f, 10.f * I, 0.f), C + FVector(0, 0, 14.f)), FVector(35.f - I * 3.f, 14.f, 14.f), UVPlain());   // instrument cases
        }
        Clutter.Cylinder(TEXT("CI_Steel"), FVector(X1 - 120.f, BackY + 200.f, Z), FVector(X1 - 120.f, BackY + 200.f, Z + 110.f), 1.f, 6, UVPlain());   // a music stand
        Clutter.Box(TEXT("CI_Steel"), FTransform(FRotator(-20.f, 90.f, 0.f), FVector(X1 - 120.f, BackY + 200.f, Z + 115.f)), FVector(1.f, 25.f, 18.f), UVPlain());
        Kit.Wall(EKit::GroupPhoto, FVector(X1, BackY + 250.f, Z + 165.f), FVector(-1, 0, 0));
        HarPoster(Clutter, TEXT("CI_GenAuditions"), FVector(X0, BackY + 200.f, Z + 160.f), FVector(1, 0, 0), 60.f, 80.f);
    }
    else if (Id == TEXT("ENG"))
    {
        // A truss bridge model in a case, a drafting table.
        const FVector Case(X1 - 130.f, P.In.Max.Y - 200.f, Z);   // by the east wall, clear of the doors and the way in
        Box(Solid, TEXT("CI_OakDark"), Case - FVector(110.f, 40.f, 0.f), Case + FVector(110.f, 40.f, 90.f));
        Box(Solid, TEXT("CI_Glass"), Case + FVector(-108.f, -38.f, 90.f), Case + FVector(108.f, 38.f, 150.f));
        for (int32 I = 0; I <= 8; ++I)
        {
            const FVector A = Case + FVector(-90.f + I * 22.5f, 0.f, 95.f);
            HouseCampusFallKit::Bar(Clutter, TEXT("CI_StaRail"), A, A + FVector(I < 8 ? 22.5f : 0.f, 0.f, 0.f), 1.f, 1.f, FVector::UpVector);
            HouseCampusFallKit::Bar(Clutter, TEXT("CI_StaRail"), A, A + FVector(0, 0, 30.f), 1.f, 1.f, FVector(1, 0, 0));
            if (I < 8) HouseCampusFallKit::Bar(Clutter, TEXT("CI_StaRail"), A + FVector(0, 0, I % 2 ? 30.f : 0.f), A + FVector(22.5f, 0, I % 2 ? 0.f : 30.f), .8f, .8f, FVector(0, 1, 0));
        }
        Solid.Box(TEXT("CI_Oak"), FTransform(FRotator(15.f, 0.f, 0.f), FVector(X0 + 90.f, P.In.Max.Y - 120.f, Z + 95.f)), FVector(60.f, 45.f, 2.f), UVPlain());
        for (const float S : { -1.f, 1.f }) Box(Solid, TEXT("CI_Steel"), FVector(X0 + 90.f + S * 50.f - 2.f, P.In.Max.Y - 122.f, Z), FVector(X0 + 90.f + S * 50.f + 2.f, P.In.Max.Y - 118.f, Z + 88.f));
    }
    else if (Id == TEXT("PLT"))
    {
        // The time clock and the card rack, the work-order board, a mop bucket and a hand truck.
        Box(Solid, TEXT("CI_Beige"), FVector(X0, BackY + 140.f, Z + 120.f), FVector(X0 + 18.f, BackY + 180.f, Z + 170.f));
        Glow.Cylinder(TEXT("CI_GreenScreen"), FVector(X0 + 18.f, BackY + 160.f, Z + 158.f), FVector(X0 + 18.5f, BackY + 160.f, Z + 158.f), 8.f, 12, UVPlain());
        Box(Solid, TEXT("CI_Steel"), FVector(X0, BackY + 190.f, Z + 100.f), FVector(X0 + 8.f, BackY + 290.f, Z + 190.f));
        for (int32 I = 0; I < 10; ++I) Box(Clutter, TEXT("CI_Paper"), FVector(X0 + 8.f, BackY + 195.f + I * 9.5f, Z + 160.f), FVector(X0 + 9.f, BackY + 202.f + I * 9.5f, Z + 185.f));
        BulletinBoard(Solid, Clutter, FVector(X1, BackY + 200.f, Z + 150.f), FVector(-1, 0, 0), 140.f, 90.f, TEXT("CI_NoticeBoard"));
        Solid.Cylinder(TEXT("CI_TissueGold"), FVector(X1 - 60.f, P.In.Max.Y - 80.f, Z), FVector(X1 - 60.f, P.In.Max.Y - 80.f, Z + 40.f), 22.f, 14, UVPlain());
        HouseCampusFallKit::Bar(Solid, TEXT("CI_StaRail"), FVector(X1 - 140.f, P.In.Max.Y - 60.f, Z), FVector(X1 - 150.f, P.In.Max.Y - 40.f, Z + 120.f), 3.f, 3.f, FVector(1, 0, 0));
    }
    else if (Id == TEXT("ALU"))
    {
        // Class photographs, the guest book on its stand, coffee for the alumni, the banner.
        for (int32 I = 0; I < 3; ++I) Kit.Wall(I % 2 ? EKit::HistoricPhoto : EKit::GroupPhoto, FVector(X0, BackY + 120.f + I * 140.f, Z + 165.f), FVector(1, 0, 0));
        Table(Solid, FVector(MidX - 120.f, P.In.Max.Y - 140.f, Z), 70.f, 50.f, 100.f, true);
        Box(Clutter, TEXT("CI_Paper"), FVector(MidX - 145.f, P.In.Max.Y - 155.f, Z + 100.f), FVector(MidX - 95.f, P.In.Max.Y - 125.f, Z + 101.f));
        Table(Solid, FVector(X1 - 120.f, BackY + 140.f, Z), 160.f, 60.f, 76.f, false);
        Solid.Cylinder(TEXT("CI_Steel"), FVector(X1 - 120.f, BackY + 100.f, Z + 76.f), FVector(X1 - 120.f, BackY + 100.f, Z + 118.f), 13.f, 14, UVPlain());
        for (int32 I = 0; I < 4; ++I) Kit.Floor(EKit::PaperCup, FVector2D(X1 - 125.f + (I % 2) * 15.f, BackY + 160.f + (I / 2) * 15.f), Z + 76.f, 0.f, true);
        HouseCampusFallKit::Card(Clutter, TEXT("CI_HarAlumni"), FVector(MidX, P.In.Max.Y - 1.f, Z + 270.f), FVector(-1, 0, 0), FVector::UpVector, 300.f, 30.f, FVector(0, -1, 0));
    }
    else if (Id == TEXT("MAIN"))
    {
        Kit.Wall(EKit::HistoricPhoto, FVector(X0, P.In.Max.Y - 150.f, Z + 170.f), FVector(1, 0, 0));
        HarPoster(Clutter, TEXT("CI_GenFounder"), FVector(X0, P.In.Max.Y - 150.f, Z + 230.f), FVector(1, 0, 0), 60.f, 18.f);
    }
}

void GenBuild(int32 Seed, float Height, FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit)
{
    const FGenPlan* PP = GenPlan(Seed);
    const HouseCampusLayout::FBlock* Blk = Block(Seed);
    if (!PP || !Blk) return;
    const FGenPlan& P = *PP;
    const FString Id = Blk->Id;
    const bool bBlock = P.Spec->Style == 1, bOak = P.Spec->Style != 1;
    const FName WallKey = bBlock ? FName(TEXT("CI_Cinderblock")) : FName(TEXT("CI_PlasterWall"));
    const FHallUV WallUV = bBlock ? FHallUV::World(80.f) : UVPlain();
    const float Z = P.F0, Ceil = P.Ceil, H = PartT * .5f;
    const FBox2D Lobby = Rect(P.LobbyX0, P.LobbyX1, P.CorFront, P.In.Max.Y), Corridor = Rect(P.In.Min.X, P.In.Max.X, P.CorBack, P.CorFront);
    // Shell, floors (terrazzo in the lobby and corridor, tile or carpet in the rooms), the ceiling, window trims.
    HouseCampusInteriors::OpenWalls(Solid, bBlock ? TEXT("CI_Cinderblock") : TEXT("CI_Plaster"), Seed, OuterSkin, WallT, WallUV);
    const FName RoomFloor = P.Spec->Style == 2 ? FName(TEXT("CI_Carpet")) : FName(TEXT("CI_VCT"));
    FloorSlab(Solid, P.In, Z, 70.f, { Lobby, Corridor }, RoomFloor, TEXT("CI_Plaster"), FHallUV::World(P.Spec->Style == 2 ? 180.f : 61.f));
    FloorSlab(Solid, Lobby, Z, 70.f, {}, TEXT("CI_Terrazzo"), TEXT("CI_Plaster"), FHallUV::World(220.f));
    if (!P.Spec->bTierC) FloorSlab(Solid, Corridor, Z, 70.f, {}, TEXT("CI_Terrazzo"), TEXT("CI_Plaster"), FHallUV::World(220.f));
    Box(Solid, TEXT("CI_CeilingTile"), FVector(P.In.Min, Ceil - .5f), FVector(P.In.Max, Ceil), FHallUV::World(122.f));
    GenWindowTrims(Solid, Seed, Height, P.In, bOak);
    if (bOak) Wainscot(Solid, true, P.In.Max.Y, -1.f, P.LobbyX0, P.LobbyX1, Z, { FVector2D(-129.f, 129.f) }, 60.f);
    auto Wall = [&](bool bAlongY, float At, float U0, float U1, const TArray<FVector2D>& Doors, float Head)
    {
        if (bBlock) BlockWall(Solid, WallKey, bAlongY, At, U0, U1, Z, Ceil + 1.f, Doors, Head, {}, TEXT("CI_FrameBrown"), WallUV);
        else PartitionDoors(Solid, WallKey, bAlongY, At, U0, U1, Z, Ceil + 1.f, Doors, Head);
    };
    if (P.Spec->bTierC)
    {
        // Only the lobby: its back wall with the two locked doors, its side walls.
        Wall(false, P.CorFront, P.LobbyX0 - H, P.LobbyX1 + H, P.LobbyDoors, 213.f);
        Wall(true, P.LobbyX0, P.CorFront + H, P.In.Max.Y, {}, 213.f);
        Wall(true, P.LobbyX1, P.CorFront + H, P.In.Max.Y, {}, 213.f);
        const FVector2D Chained = P.LobbyDoors.Last();
        GenChain(Clutter, FVector(Chained.X - 6.f, P.CorFront + H + 1.f, Z + 100.f), FVector(Chained.Y + 6.f, P.CorFront + H + 1.f, Z + 100.f), FVector(0, 1, 0));
    }
    else
    {
        // The corridor's walls with every room's doorway (the lobby open to it), the rooms' side walls on the piers, the
        // back row's back walls where they stop short.
        TArray<FVector2D> FrontDoors = { FVector2D(P.LobbyX0 + 8.f, P.LobbyX1 - 8.f) }, BackDoors;
        for (const FGenRoom& Rm : P.Rooms) (Rm.bFront ? FrontDoors : BackDoors).Add(FVector2D(Rm.DoorU0, Rm.DoorU1));
        Wall(false, P.CorFront, P.In.Min.X, P.In.Max.X, FrontDoors, 213.f);
        Wall(false, P.CorBack, P.In.Min.X, P.In.Max.X, BackDoors, 213.f);
        for (int32 I = 0; I < P.Rooms.Num(); ++I)
        {
            const FGenRoom& Rm = P.Rooms[I];
            const float East = P.Edges[Rm.Bay1 + 1];
            if (Rm.Bay1 + 1 < P.Edges.Num() - 1)
            {
                if (Rm.bFront) Wall(true, East, P.CorFront + H, P.In.Max.Y, {}, 213.f);
                else
                {
                    const FGenRoom* Next = (I + 1 < P.Rooms.Num() && !P.Rooms[I + 1].bFront) ? &P.Rooms[I + 1] : nullptr;
                    const float Low = FMath::Min(Rm.R.Min.Y, Next ? Next->R.Min.Y : Rm.R.Min.Y);
                    Wall(true, East, Low - (Low > P.In.Min.Y + 1.f ? H : 0.f), P.CorBack - H, {}, 213.f);
                }
            }
            if (!Rm.bFront && Rm.R.Min.Y > P.In.Min.Y + 1.f) Wall(false, Rm.R.Min.Y - H, Rm.R.Min.X - H, Rm.R.Max.X + H, {}, 213.f);
        }
        // The lobby's east wall (its west one is the room before it's east wall).
        Wall(true, P.LobbyX1, P.CorFront + H, P.In.Max.Y, {}, 213.f);
        // Rooms: plates by the doors, furnishing, a light or two each.
        const FName Plates(*FString::Printf(TEXT("CI_Gen%sPlates"), *Id));
        for (int32 I = 0; I < P.Rooms.Num(); ++I)
        {
            const FGenRoom& Rm = P.Rooms[I];
            const float WallY = Rm.bFront ? P.CorFront - H : P.CorBack + H;
            GenPlate(Clutter, Plates, I, FVector(Rm.DoorU1 + 32.f, WallY, Z + 150.f), FVector(0.f, Rm.bFront ? -1.f : 1.f, 0.f));
            if (!Rm.bOpen)
            {
                if (Rm.Kind == 'Z' || Rm.Kind == 'X') GenChain(Clutter, FVector(Rm.DoorU0 - 6.f, WallY + (Rm.bFront ? -1.f : 1.f), Z + 100.f), FVector(Rm.DoorU1 + 6.f, WallY + (Rm.bFront ? -1.f : 1.f), Z + 100.f), FVector(0.f, Rm.bFront ? -1.f : 1.f, 0.f));
                continue;
            }
            GenFurnish(Solid, Clutter, Glow, Kit, P, Rm, Seed);
            const FVector2D C = Rm.R.GetCenter();
            const float Half = Rm.R.GetExtent().X;
            for (float X = C.X - Half + 180.f; X < C.X + Half - 100.f; X += 360.f) Troffer(Glow, Clutter, FVector(X, C.Y, Ceil - .5f - .6f), false);
        }
        // The corridor: troffers, a notice board, an extinguisher; lockers in the classroom buildings.
        int32 Tube = 0;
        for (float X = P.In.Min.X + 200.f; X < P.In.Max.X - 100.f; X += 400.f)
            Troffer(Glow, Clutter, FVector(X, (P.CorFront + P.CorBack) * .5f, Ceil - .5f - .6f), true, (++Tube == 3) ? FName(TEXT("CI_TrofferFlicker")) : FName());
        // The notice board and the extinguisher on the corridor's back wall, each where no doorway is.
        auto FreeSpot = [&](float From, float Step, float Half)
        {
            for (float U = From; U > P.In.Min.X + Half && U < P.In.Max.X - Half; U += Step)
            {
                bool bClear = true;
                for (const FVector2D& D : BackDoors) bClear &= U + Half < D.X - 25.f || U - Half > D.Y + 25.f;
                if (bClear) return U;
            }
            return From;
        };
        const float NoticeU = FreeSpot(P.In.Min.X + 150.f, 40.f, 72.f);
        BulletinBoard(Solid, Clutter, FVector(NoticeU, P.CorBack + H, Z + 150.f), FVector(0, 1, 0), 140.f, 90.f, TEXT("CI_NoticeBoard"));
        WallClock(Clutter, FVector(NoticeU, P.CorBack + H, Z + 255.f), FVector(0, 1, 0), 15.f);
        Kit.Floor(HouseCampusInteriors::EKit::FireExtinguisher, FVector2D(FreeSpot(P.In.Max.X - 100.f, -40.f, 20.f), P.CorBack + H + 12.f), Z + 80.f, 90.f, false);
        if (Id == TEXT("KER") || Id == TEXT("HUM"))
        {
            // Lockers along the back wall of the corridor between the doorways.
            TArray<FVector2D> Gaps;
            for (const FGenRoom& Rm : P.Rooms) if (!Rm.bFront) Gaps.Add(FVector2D(Rm.DoorU0 - 12.f, Rm.DoorU1 + 50.f));
            Gaps.Add(FVector2D(P.In.Min.X, P.In.Min.X + 280.f));
            Gaps.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
            float U = P.In.Min.X + 20.f;
            for (const FVector2D& G : Gaps)
            {
                const int32 N = FMath::FloorToInt((G.X - U) / 38.f);
                if (N > 1) Lockers(Solid, Clutter, FVector(U, P.CorBack + H, Z), FVector(1, 0, 0), N, FVector(0, 1, 0));
                U = FMath::Max(U, G.Y);
            }
        }
    }
    // The lobby: mat, sign, directory, notices, payphone, ash urn, fountain, bench, plant, EXIT, streamers; its own display.
    const float LX0 = P.LobbyX0 + H, LX1 = P.LobbyX1 - H, LobbyBack = P.CorFront + H;
    Kit.Floor(HouseCampusInteriors::EKit::EntryRug, FVector2D(0.f, P.In.Max.Y - 70.f), Z, 90.f, false);
    HangingSign(Clutter, *FString::Printf(TEXT("CI_Gen%sSign"), *Id), FVector(0.f, (LobbyBack + P.In.Max.Y) * .5f, Ceil - 55.f), FVector(0, 1, 0), 300.f, 40.f, Ceil);
    ExitSign(Solid, Glow, FVector(0.f, P.In.Max.Y - 6.f, Z + 236.f), FVector(0, -1, 0));
    DirectoryBoard(Solid, Clutter, FVector(LX0, P.In.Max.Y - 130.f, Z + 160.f), FVector(1, 0, 0), *FString::Printf(TEXT("CI_Gen%sDir"), *Id));
    if (P.Spec->bTierC)
    {
        BulletinBoard(Solid, Clutter, FVector(LX1, P.In.Max.Y - 130.f, Z + 150.f), FVector(-1, 0, 0), 120.f, 80.f, TEXT("CI_NoticeBoard"));
        PayPhone(Solid, Clutter, FVector((LX0 + LX1) * .5f - 120.f, LobbyBack, Z + 140.f), FVector(0, 1, 0));
        AshUrn(Solid, FVector((LX0 + LX1) * .5f - 60.f, LobbyBack + 30.f, Z));
        WallPlaque(Clutter, TEXT("CI_GenLobbyNote"), FVector((LX0 + LX1) * .5f, LobbyBack, Z + 240.f), FVector(0, 1, 0), 160.f, 30.f);
    }
    else
    {
        BulletinBoard(Solid, Clutter, FVector(LX1, P.In.Max.Y - 130.f, Z + 150.f), FVector(-1, 0, 0), 120.f, 80.f, TEXT("CI_NoticeBoard"));
        AshUrn(Solid, FVector(LX1 - 40.f, P.In.Max.Y - 260.f, Z));
        WaterFountain(Solid, FVector(LX0, P.In.Max.Y - 280.f, Z), FVector(1, 0, 0));
    }
    WallClock(Clutter, FVector(LX1, P.In.Max.Y - 130.f, Z + 250.f), FVector(-1, 0, 0), 16.f);
    Kit.Floor(HouseCampusInteriors::EKit::PlantStand, FVector2D(LX1 - 40.f, P.In.Max.Y - 50.f), Z, 0.f, true);
    for (const float D : { 0.f, 14.f })
        Streamer(Clutter, FVector(LX0 + 10.f, LobbyBack + 30.f + D, Ceil - 8.f), FVector(LX1 - 10.f, P.In.Max.Y - 30.f + D, Ceil - 8.f), D > 0.f);
    for (float X = LX0 + 150.f; X < LX1 - 60.f; X += 300.f) Troffer(Glow, Clutter, FVector(X, (LobbyBack + P.In.Max.Y) * .5f, Ceil - .5f - .6f), false);
    GenDisplay(Solid, Clutter, Glow, Kit, P, Id);
}

void GenAnchors(int32 Seed, float Height, TArray<FVector>& Out)
{
    const FGenPlan* P = GenPlan(Seed);
    if (!P) return;
    const float Z = P->F0 + 290.f;   // (the pool reads an anchor's floor from 300 below it: lower and it counts as the floor under)
    Out.Add(FVector(0.f, (P->CorFront + P->In.Max.Y) * .5f, Z));
    if (P->Spec->bTierC) return;
    for (float X = P->In.Min.X + 500.f; X < P->In.Max.X; X += 900.f) Out.Add(FVector(X, (P->CorFront + P->CorBack) * .5f, Z));
    for (const FGenRoom& Rm : P->Rooms) if (Rm.bOpen) Out.Add(FVector(Rm.R.GetCenter(), Z));
}
void GenOpenings(int32 Seed, float Height, TArray<HouseCampusInteriors::FOpening>& Out)
{
    const float F0 = HouseCampusInteriors::FloorTop(Height, 0);
    Out.Add({ 0, -120.f, 120.f, F0, F0 + 213.f });
}
void GenDoors(int32 Seed, float Height, TArray<FDoorSpec>& Out)
{
    const FGenPlan* P = GenPlan(Seed);
    const HouseCampusLayout::FBlock* Blk = Block(Seed);
    if (!P || !Blk) return;
    TArray<HouseCampusInteriors::FOpening> Openings;
    GenOpenings(Seed, Height, Openings);
    MainDoorPair(Blk->Id, HouseCampusInteriors::CanonRect(Seed), Openings[0], Out);
    const uint8 Glazed = AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::KickPlate | AHouseBlackHallDoor::Closer;
    const uint8 Steel = AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::KickPlate;
    if (P->Spec->bTierC)
    {
        for (int32 I = 0; I < P->LobbyDoors.Num(); ++I)
            InnerDoor(Out, *FString::Printf(TEXT("Lobby%d"), I), false, P->CorFront, P->LobbyDoors[I].X, P->LobbyDoors[I].Y, P->F0, -1.f, I == 0 ? EHallDoorStyle::French : EHallDoorStyle::Equipment, I == 0 ? Glazed : Steel, P->LobbyReasons.IsValidIndex(I) ? P->LobbyReasons[I] : FString(TEXT("CLOSED SATURDAY")));
        return;
    }
    for (const FGenRoom& Rm : P->Rooms)
    {
        if (GenWideDoor(Rm.Kind)) continue;   // a wide opening, no leaf
        const bool bGlazed = Rm.bOpen && Rm.Kind != 'Q';
        InnerDoor(Out, *FString::Printf(TEXT("Room%d"), Rm.Number), false, Rm.bFront ? P->CorFront : P->CorBack, Rm.DoorU0, Rm.DoorU1, P->F0, Rm.bFront ? 1.f : -1.f,
            bGlazed ? EHallDoorStyle::French : EHallDoorStyle::Equipment, bGlazed ? Glazed : Steel, Rm.Reason, Rm.bOpen);
    }
}
bool GenWindowOpen(int32 Seed, int32 Side, int32 K, float U0, float U1)
{
    const FGenPlan* P = GenPlan(Seed);
    if (!P) return true;
    const float U = (U0 + U1) * .5f;
    if (P->Spec->bTierC) return Side == 0 && U > P->LobbyX0 && U < P->LobbyX1;
    if (Side == 0)
    {
        if (U > P->LobbyX0 && U < P->LobbyX1) return true;
        const FGenRoom* Rm = GenRoomAtFront(*P, U);
        return Rm && Rm->bOpen;
    }
    if (Side == 1)
    {
        const FGenRoom* Rm = GenRoomAtBack(*P, U);
        return Rm && Rm->bOpen && Rm->R.Min.Y <= P->In.Min.Y + 1.f;
    }
    // The end walls: the corridor's ends, the end rooms of each row.
    if (U > P->CorBack && U < P->CorFront) return true;
    const bool bFrontRow = U >= P->CorFront;
    const FGenRoom* End = nullptr;
    for (const FGenRoom& Rm : P->Rooms)
        if (Rm.bFront == bFrontRow && (Side == 2 ? Rm.Bay0 == 0 : Rm.Bay1 == P->Edges.Num() - 2)) End = &Rm;
    return End && End->bOpen && U >= End->R.Min.Y - 1.f && U <= End->R.Max.Y + 1.f;
}

void GenericLook(UHouseBlackHallSubsystem* Look)
{
    if (!Look || Look->Materials.Contains(TEXT("CI_GenArt0"))) return;
    {
        // Six pieces of student work for the easels and the gallery walls: abstracts, a still life, a landscape.
        FRandomStream R(1986);
        for (int32 K = 0; K < 6; ++K)
        {
            FHallImage Img; Img.Init(160, 200, FLinearColor(R.FRandRange(.6f, .9f), R.FRandRange(.55f, .85f), R.FRandRange(.45f, .8f)));
            if (K == 4)
            {
                Img.Rect(FVector2D(0, 120), FVector2D(160, 200), FLinearColor(.25f, .4f, .2f));
                Img.Ellipse(FVector2D(110, 60), FVector2D(22, 22), 0.f, FLinearColor(.9f, .7f, .3f));
                Img.FillPolygon({ FVector2D(0, 130), FVector2D(60, 70), FVector2D(110, 130) }, FLinearColor(.35f, .3f, .4f));
            }
            else if (K == 5)
            {
                Img.Ellipse(FVector2D(80, 130), FVector2D(55, 25), 0.f, FLinearColor(.75f, .75f, .72f));
                for (int32 I = 0; I < 4; ++I) Img.Ellipse(FVector2D(55 + I * 17, 112), FVector2D(12, 12), 0.f, I % 2 ? FLinearColor(.8f, .2f, .1f) : FLinearColor(.85f, .65f, .1f));
            }
            else
                for (int32 I = 0; I < 9; ++I)
                {
                    const FLinearColor C(R.FRandRange(.05f, .95f), R.FRandRange(.05f, .9f), R.FRandRange(.1f, .9f));
                    if (I % 3 == 0) Img.Rect(FVector2D(R.FRandRange(0, 120), R.FRandRange(0, 160)), FVector2D(R.FRandRange(40, 160), R.FRandRange(40, 200)), C);
                    else if (I % 3 == 1) Img.Ellipse(FVector2D(R.FRandRange(20, 140), R.FRandRange(20, 180)), FVector2D(R.FRandRange(10, 50), R.FRandRange(10, 50)), 0.f, C);
                    else Img.Stroke({ FVector2D(R.FRandRange(0, 160), R.FRandRange(0, 200)), FVector2D(R.FRandRange(0, 160), R.FRandRange(0, 200)), FVector2D(R.FRandRange(0, 160), R.FRandRange(0, 200)) }, R.FRandRange(3, 12), C);
                }
            const FName Key(*FString::Printf(TEXT("CI_GenArt%d"), K));
            UTexture2D* Tex = HallMakeTexture(Img, true, false, *Key.ToString(), false);
            if (Tex) Look->Textures.Add(Key, Tex);
            HallMakeTextured(Look, Key, Tex, nullptr, .6f);
        }
    }
    // Each building's plates, directory and sign, painted from its plan.
    const FLinearColor PlateBg(.08f, .06f, .04f), PlateInk(.8f, .65f, .3f), Cream(.86f, .84f, .78f);
    for (int32 Seed = 0; Seed < HouseCampusLayout::Blocks().Num(); ++Seed)
    {
        const FGenPlan* P = GenPlan(Seed);
        if (!P || HouseCampusInteriors::OpenFloors(Seed) <= 0) continue;
        const FString Id = HouseCampusLayout::Blocks()[Seed].Id;
        {
            FHallImage Img; Img.Init(1024, 512, PlateBg);
            for (int32 I = 0; I < FMath::Min(32, P->Rooms.Num()); ++I)
            {
                const FGenRoom& Rm = P->Rooms[I];
                const float X0 = (I % 4) * 256.f, Y0 = (I / 4) * 64.f;
                Img.Rect(FVector2D(X0 + 4, Y0 + 4), FVector2D(X0 + 252, Y0 + 60), PlateInk);
                Img.Rect(FVector2D(X0 + 7, Y0 + 7), FVector2D(X0 + 249, Y0 + 57), PlateBg);
                const FString T = Rm.Kind == 'Q' || Rm.Kind == 'Y' ? FString::FromInt(Rm.Number) : FString::Printf(TEXT("%d  %s"), Rm.Number, *Rm.Name);
                FHallTextStyle S; S.Height = 22.f; S.Stroke = 3.f; S.Spacing = .18f;
                S.WidthScale = FMath::Min(1.f, 230.f / FMath::Max(1.f, HallTextWidth(T, S)));
                HallDrawText(Img, T, FVector2D(X0 + (256.f - HallTextWidth(T, S) * S.WidthScale) * .5f, Y0 + 32.f + 11.f), S, PlateInk);
            }
            const FName Key(*FString::Printf(TEXT("CI_Gen%sPlates"), *Id));
            UTexture2D* Tex = HallMakeTexture(Img, true, false, *Key.ToString(), false);
            if (Tex) Look->Textures.Add(Key, Tex);
            HallMakeTextured(Look, Key, Tex, nullptr, .4f);
        }
        {
            FHallImage Img; Img.Init(256, 360, FLinearColor(.05f, .05f, .06f));
            FHallTextStyle S; S.Height = 15.f; S.Stroke = 2.2f; S.Spacing = .18f;
            float Y = 26.f;
            auto Line = [&](const FString& T)
            {
                FHallTextStyle Fit = S;
                Fit.WidthScale = FMath::Min(1.f, 232.f / FMath::Max(1.f, HallTextWidth(T, S)));
                HallDrawText(Img, T, FVector2D(12.f, Y), Fit, Cream);
                Y += 24.f;
            };
            Line(HouseCampusLayout::Blocks()[Seed].Name);
            Y += 8.f;
            if (P->Spec->bTierC) { Line(TEXT("OFFICES CLOSED SATURDAY")); Line(TEXT("SEE YOU AT THE GAME")); }
            for (const FGenRoom& Rm : P->Rooms) if (Rm.Kind != 'Q' && Rm.Kind != 'Y' && Y < 350.f) Line(FString::Printf(TEXT("%d  %s"), Rm.Number, *Rm.Name));
            const FName Key(*FString::Printf(TEXT("CI_Gen%sDir"), *Id));
            UTexture2D* Tex = HallMakeTexture(Img, true, false, *Key.ToString(), false);
            if (Tex) Look->Textures.Add(Key, Tex);
            HallMakeTextured(Look, Key, Tex, nullptr, .5f);
        }
        PaintText(Look, *FString::Printf(TEXT("CI_Gen%sSign"), *Id), 896, 128, FLinearColor(.03f, .03f, .035f), FLinearColor(.75f, .58f, .2f), { { HouseCampusLayout::Blocks()[Seed].Name, .38f } }, true);
    }
    const FLinearColor Board(.06f, .13f, .09f), Chalk(.8f, .82f, .78f), Dark(.08f, .06f, .05f);
    PaintText(Look, TEXT("CI_GenChalkHistory"), 512, 160, Board, Chalk, { { TEXT("1066 - 1215 - 1492 - 1776"), .17f }, { TEXT("ESSAY DUE TUES - 5 PAGES"), .13f } }, false);
    PaintText(Look, TEXT("CI_GenChalkEnglish"), 512, 160, Board, Chalk, { { TEXT("METAPHOR VS SIMILE"), .18f }, { TEXT("READ ACT III BY MONDAY"), .13f } }, false);
    PaintText(Look, TEXT("CI_GenClosedSat"), 256, 86, Cream, FLinearColor(.6f, .08f, .06f), { { TEXT("CLOSED SATURDAY"), .4f } }, true);
    PaintText(Look, TEXT("CI_GenWindow"), 256, 86, Dark, Cream, { { TEXT("NEXT WINDOW"), .4f } }, true);
    PaintText(Look, TEXT("CI_GenRegistrarSign"), 640, 100, Dark, FLinearColor(.78f, .58f, .18f), { { TEXT("OFFICE OF THE REGISTRAR"), .4f } }, true);
    PaintText(Look, TEXT("CI_GenHealthClosed"), 384, 210, Cream, FLinearColor(.6f, .08f, .06f), { { TEXT("CLOSED"), .22f }, { TEXT("EMERGENCIES"), .12f }, { TEXT("CAMPUS SECURITY EXT 2911"), .1f } }, true);
    PaintText(Look, TEXT("CI_GenFluShots"), 160, 210, FLinearColor(.3f, .55f, .7f), Cream, { { TEXT("FLU"), .2f }, { TEXT("SHOTS"), .18f }, { TEXT("NOV 3 - 7"), .1f } }, true);
    PaintText(Look, TEXT("CI_GenAuditions"), 160, 210, Cream, Dark, { { TEXT("SPRING"), .14f }, { TEXT("AUDITIONS"), .13f }, { TEXT("SIGN UP"), .12f } }, true);
    PaintText(Look, TEXT("CI_GenShowCard"), 256, 170, Cream, Dark, { { TEXT("FALL STUDENT"), .16f }, { TEXT("SHOW 1986"), .16f } }, true);
    PaintText(Look, TEXT("CI_GenFounder"), 512, 96, Dark, FLinearColor(.78f, .58f, .18f), { { TEXT("AMOS CALDER, FOUNDER, 1891"), .34f } }, false);
    PaintText(Look, TEXT("CI_GenLobbyNote"), 512, 96, Cream, Dark, { { TEXT("BUILDING CLOSED SATURDAY"), .27f }, { TEXT("LOBBY OPEN FOR HOMECOMING"), .2f } }, true);
    HallMakePlain(Look, TEXT("CI_GenClay"), FLinearColor(.45f, .3f, .2f), .9f);
    HallMakePlain(Look, TEXT("CI_GenGlaze"), FLinearColor(.12f, .3f, .35f), .2f);
}

void GenericDefs(TArray<FInteriorDef>& Out)
{
    for (const FGenSpec& S : GenSpecs)
    {
        FInteriorDef D{ S.Id, 1, &GenBuild, &GenAnchors, &GenOpenings, &GenDoors, &GenericLook };
        D.WindowOpen = &GenWindowOpen;
        Out.Add(D);
    }
}
}

// ------------------------------------------------------------------ the review's walk
bool HouseCampusInteriors::IsGeneric(int32 Seed) { return HouseCampusInteriorsImpl::GenPlan(Seed) != nullptr; }
void HouseCampusInteriors::GenericTour(int32 Seed, TArray<FTourStop>& Out)
{
    namespace CI = HouseCampusInteriorsImpl;
    const CI::FGenPlan* P = CI::GenPlan(Seed);
    const HouseCampusLayout::FBlock* Blk = CI::Block(Seed);
    if (!P || !Blk) return;
    const FString Id = Blk->Id;
    const float Outside = P->In.Max.Y + CI::WallT + 600.f, AtDoor = P->In.Max.Y + CI::WallT + 90.f, Lobby = P->In.Max.Y - 230.f;
    const float CorMid = (P->CorFront + P->CorBack) * .5f;
    auto Walk = [&](const FVector2D& At, const FString& Name, const FString& Shot = FString(), float Yaw = 0.f) { FTourStop S; S.At = At; S.Name = Name; S.Shot = Shot; S.LookYaw = Yaw; Out.Add(S); };
    auto Try = [&](const FVector2D& At, const FString& Name, const FString& Door) { FTourStop S; S.At = At; S.Kind = 1; S.Name = Name; S.Door = Door; Out.Add(S); };
    Walk(FVector2D(0.f, Outside), TEXT("the front walk"));
    Walk(FVector2D(0.f, AtDoor), TEXT("at the main doors"));
    FTourStop Open; Open.At = FVector2D(0.f, AtDoor); Open.Kind = 2; Open.Name = TEXT("open the main doors"); Open.Door = TEXT("MainA"); Out.Add(Open);
    Walk(FVector2D(0.f, Lobby), TEXT("into the lobby"), FString::Printf(TEXT("%s_1_Lobby.png"), *Id), -90.f);
    if (P->Spec->bTierC)
    {
        const float Front = P->CorFront + CI::PartT * .5f + 75.f;
        for (int32 I = 0; I < P->LobbyDoors.Num(); ++I)
        {
            const FVector2D At((P->LobbyDoors[I].X + P->LobbyDoors[I].Y) * .5f, Front);
            Walk(At, FString::Printf(TEXT("at the lobby's door %d"), I));
            Try(At, FString::Printf(TEXT("try the lobby's door %d"), I), FString::Printf(TEXT("Lobby%d"), I));
        }
        Walk(FVector2D(0.f, Lobby), TEXT("back across the lobby"));
    }
    else
    {
        Walk(FVector2D(0.f, CorMid), TEXT("the corridor"));
        bool bTriedLocked = false, bShot = false;
        for (const CI::FGenRoom& Rm : P->Rooms)
        {
            const bool bWide = CI::GenWideDoor(Rm.Kind);
            const float DoorX = bWide ? (Rm.DoorU0 + Rm.DoorU1) * .5f : Rm.DoorU0 + 50.f, Dir = Rm.bFront ? 1.f : -1.f;
            const float WallY = Rm.bFront ? P->CorFront : P->CorBack;
            if (!Rm.bOpen)
            {
                if (bTriedLocked || bWide) continue;
                bTriedLocked = true;
                Walk(FVector2D(DoorX, CorMid), FString::Printf(TEXT("at %d"), Rm.Number));
                Try(FVector2D(DoorX, CorMid), FString::Printf(TEXT("try %d's door"), Rm.Number), FString::Printf(TEXT("Room%d"), Rm.Number));
                continue;
            }
            Walk(FVector2D(DoorX, CorMid), FString::Printf(TEXT("at %d %s"), Rm.Number, *Rm.Name));
            Walk(FVector2D(DoorX, WallY + Dir * 110.f), FString::Printf(TEXT("into %d"), Rm.Number), bShot ? FString() : FString::Printf(TEXT("%s_2_Room.png"), *Id), Rm.bFront ? 60.f : -60.f);
            bShot = true;
            Walk(FVector2D(DoorX, CorMid), TEXT("back to the corridor"));
        }
        Walk(FVector2D(0.f, CorMid), TEXT("the corridor at the lobby"));
        Walk(FVector2D(0.f, Lobby), TEXT("back through the lobby"));
    }
    Walk(FVector2D(0.f, Outside), TEXT("out through the doors"));
}
