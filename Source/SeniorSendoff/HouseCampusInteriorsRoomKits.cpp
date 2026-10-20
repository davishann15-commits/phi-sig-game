// Calder State's room templates (master job, step 2d): whole 1986 rooms furnished in one call, so a walk-in building's
// file is mostly its plan. Each fills an axis-aligned room rect R (canonical frame, the inside faces of its walls) on floor
// Z; Front is the side the teacher, counter or TV faces from (0 +Y, 1 -Y, 2 -X, 3 +X). They keep a 120 cm clear way from
// the room's doorway side (DoorSide) to its middle. Declared in HouseCampusInteriorsRooms.h.
#include "HouseCampusInteriorsRooms.h"
#include "HouseCampusFallKit.h"

namespace HouseCampusInteriorsImpl
{
/** A side's outward normal, its yaw, and the room's extent across and along it. */
struct FRoomSide
{
    FVector2D N; float Yaw; float Across, Along; FVector2D Centre;
    FRoomSide(const FBox2D& R, int32 Side)
    {
        Centre = R.GetCenter();
        const FVector2D E = R.GetExtent();
        switch (Side)
        {
        case 0: N = FVector2D(0, 1); Yaw = 90.f; Across = E.X; Along = E.Y; break;
        case 1: N = FVector2D(0, -1); Yaw = -90.f; Across = E.X; Along = E.Y; break;
        case 2: N = FVector2D(-1, 0); Yaw = 180.f; Across = E.Y; Along = E.X; break;
        default: N = FVector2D(1, 0); Yaw = 0.f; Across = E.Y; Along = E.X; break;
        }
    }
    /** A point D in from the wall on this side, T along it (from the centre). */
    FVector2D At(float D, float T) const { const FVector2D Tan(-N.Y, N.X); return Centre + N * (Along - D) + Tan * T; }
    FVector2D Tangent() const { return FVector2D(-N.Y, N.X); }
};

void RoomClassroom(FHallMesh& Solid, FHallMesh& Clutter, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, FName Board, int32 Seed)
{
    // The board and the teacher's desk at the front; rows of tablet-arm desks facing them, an aisle down one side.
    const FRoomSide S(R, Front);
    const FVector2D T = S.Tangent();
    const float Wall = S.Along;
    Chalkboard(Solid, Clutter, FVector(S.At(2.f, 0.f), Z + 150.f), FVector(-S.N, 0.f), FMath::Min(480.f, S.Across * 1.4f), 120.f, Board);
    WallClock(Clutter, FVector(S.At(0.f, 0.f), Z + 258.f), FVector(-S.N, 0.f), 15.f);
    Table(Solid, FVector(S.At(150.f, -S.Across * .45f), Z), 150.f, 75.f, 76.f, FMath::Abs(T.X) > .5f);
    Kit.Floor(HouseCampusInteriors::EKit::GrayOfficeChair, S.At(105.f, -S.Across * .45f), Z, S.Yaw + 180.f, true);
    Kit.Floor(HouseCampusInteriors::EKit::StackedBooks, S.At(150.f, -S.Across * .45f - 30.f), Z + 76.f, 20.f + Seed * 37.f, true);
    FRandomStream Rnd(Seed);
    for (float D = 300.f; D < 2.f * Wall - 120.f; D += 110.f)
        for (float U = -S.Across + 70.f; U < S.Across - 150.f; U += 80.f)
        {
            const FVector2D P = S.At(D, U);
            TabletDesk(Solid, FVector(P, Z), FVector(S.N, 0.f));
            if (Rnd.FRand() < .18f) Kit.Floor(HouseCampusInteriors::EKit::RedBook, P + S.N * 14.f + T * 18.f, Z + 73.f, Rnd.FRandRange(0.f, 360.f), true);
        }
    Kit.Floor(HouseCampusInteriors::EKit::TrashCan, S.At(2.f * Wall - 40.f, S.Across - 40.f), Z, 0.f, true);
}

void RoomOffice(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, int32 Seed)
{
    // A faculty office: a desk facing the door with a typewriter and a phone, a bookcase on the back wall, a visitor chair,
    // a filing cabinet, a framed diploma.
    const FRoomSide S(R, Front);
    const FVector2D Desk = S.At(S.Along * .9f, 0.f);
    Table(Solid, FVector(Desk, Z), 150.f, 75.f, 76.f, FMath::Abs(S.Tangent().X) > .5f);
    Kit.Floor(HouseCampusInteriors::EKit::GrayOfficeChair, Desk + S.N * 55.f, Z, S.Yaw + 180.f, true);
    Kit.Floor(HouseCampusInteriors::EKit::DeskChair, Desk - S.N * 70.f, Z, S.Yaw, true);
    Kit.Floor(HouseCampusInteriors::EKit::TableLamp, Desk + S.Tangent() * 55.f, Z + 76.f, S.Yaw + 180.f, true);
    Kit.Floor(HouseCampusInteriors::EKit::PaperStack, Desk - S.Tangent() * 30.f, Z + 76.f, Seed * 23.f, true);
    Kit.Floor(HouseCampusInteriors::EKit::Bookcase, S.At(18.f, S.Across * .4f), Z, S.Yaw + 180.f, false);
    Kit.FillBookcase(S.At(18.f, S.Across * .4f), Z, S.Yaw + 180.f, Seed);
    Box(Solid, TEXT("CI_Steel"), FVector(S.At(25.f, -S.Across + 30.f) - FVector2D(22.f, 22.f), Z), FVector(S.At(25.f, -S.Across + 30.f) + FVector2D(22.f, 22.f), Z + 132.f));   // filing cabinet
    Kit.Wall(HouseCampusInteriors::EKit::HistoricPhoto, FVector(S.At(0.5f, -S.Across * .3f), Z + 160.f), FVector(-S.N, 0.f));
    WallClock(Clutter, FVector(S.At(2.f * S.Along, S.Across * .2f), Z + 250.f), FVector(S.N, 0.f), 14.f);
}

void RoomLounge(FHallMesh& Solid, FHallMesh& Glow, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, int32 Seed)
{
    // A TV lounge: the console TV against the front wall, a sofa and recliners facing it on a rug, a coffee chest, a lamp.
    const FRoomSide S(R, Front);
    CrtTelevision(Solid, Glow, FVector(S.At(30.f, 0.f), Z), FVector(-S.N, 0.f), true);
    Kit.Floor(HouseCampusInteriors::EKit::StudyRug, S.At(S.Along, 0.f), Z, S.Yaw, false);
    Kit.Floor(HouseCampusInteriors::EKit::LeatherSofa, S.At(S.Along + 150.f, 0.f), Z, S.Yaw, true);
    Kit.Floor(HouseCampusInteriors::EKit::TanRecliner, S.At(S.Along - 20.f, S.Across * .6f), Z, S.Yaw + 30.f, true);
    Kit.Floor(HouseCampusInteriors::EKit::ClubChair, S.At(S.Along - 20.f, -S.Across * .6f), Z, S.Yaw - 30.f, true);
    Kit.Floor(HouseCampusInteriors::EKit::CoffeeChest, S.At(S.Along + 40.f, 0.f), Z, S.Yaw, true);
    Kit.Floor(HouseCampusInteriors::EKit::FloorLamp, S.At(S.Along + 150.f, S.Across * .75f), Z, S.Yaw, true);
    Kit.Floor(HouseCampusInteriors::EKit::PaperCup, S.At(S.Along + 40.f, 30.f), Z + Kit.Height(HouseCampusInteriors::EKit::CoffeeChest), Seed * 41.f, true);
    WallClock(Solid, FVector(S.At(2.f * S.Along, S.Across * .5f), Z + 250.f), FVector(S.N, 0.f), 15.f);
}

void RoomDining(FHallMesh& Solid, FHallMesh& Glow, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, int32 Seed)
{
    // A dining room: the serving line along the front wall, rows of tables with green spindle chairs, trays left behind.
    const FRoomSide S(R, Front);
    const FVector2D T = S.Tangent();
    for (float U = -S.Across + 150.f; U < S.Across - 100.f; U += 250.f) Kit.Floor(HouseCampusInteriors::EKit::ServingCounter, S.At(60.f, U), Z, S.Yaw + 180.f, false);
    FRandomStream Rnd(Seed);
    for (float D = 300.f; D < 2.f * S.Along - 120.f; D += 230.f)
        for (float U = -S.Across + 160.f; U < S.Across - 120.f; U += 260.f)
        {
            const FVector2D C = S.At(D, U);
            Kit.Floor(HouseCampusInteriors::EKit::DiningTable, C, Z, S.Yaw, false);
            for (const float Side : { -1.f, 1.f })
                for (const float Off : { -45.f, 45.f })
                    Kit.Floor(HouseCampusInteriors::EKit::GreenSpindleChair, C + S.N * (Side * 60.f) + T * Off, Z, S.Yaw + (Side > 0.f ? 180.f : 0.f) + Rnd.FRandRange(-8.f, 8.f), true);
            if (Rnd.FRand() < .5f) Kit.Floor(HouseCampusInteriors::EKit::PaperCup, C + T * Rnd.FRandRange(-60.f, 60.f), Z + Kit.Height(HouseCampusInteriors::EKit::DiningTable), 0.f, true);
        }
    Kit.Floor(HouseCampusInteriors::EKit::TrashCan, S.At(2.f * S.Along - 40.f, -S.Across + 40.f), Z, 0.f, true);
    WallClock(Solid, FVector(S.At(2.f * S.Along, 0.f), Z + 262.f), FVector(S.N, 0.f), 22.f);
}


void RoomSeminar(FHallMesh& Solid, FHallMesh& Clutter, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, FName Board, int32 Seed)
{
    // A seminar room: one long table, chairs round it, a board at the front, bookcases on the back wall.
    const FRoomSide S(R, Front);
    const FVector2D T = S.Tangent();
    Chalkboard(Solid, Clutter, FVector(S.At(2.f, 0.f), Z + 150.f), FVector(-S.N, 0.f), FMath::Min(360.f, S.Across * 1.2f), 110.f, Board);
    WallClock(Clutter, FVector(S.At(0.f, 0.f), Z + 252.f), FVector(-S.N, 0.f), 15.f);
    const FVector2D C = S.At(S.Along, 0.f);
    Table(Solid, FVector(C, Z), 2.f * S.Along - 330.f, 110.f, 76.f, FMath::Abs(S.N.X) > .5f);
    for (float D = -S.Along + 230.f; D <= S.Along - 230.f; D += 80.f)
        for (const float Side : { -1.f, 1.f })
            Kit.Floor(HouseCampusInteriors::EKit::DeskChair, C + S.N * -D + T * (Side * 85.f), Z, FMath::RadiansToDegrees(FMath::Atan2(-T.Y * Side, -T.X * Side)), true);
    Kit.Floor(HouseCampusInteriors::EKit::StackedBooks, C, Z + 76.f, Seed * 17.f, true);
    Kit.Floor(HouseCampusInteriors::EKit::Bookcase, S.At(2.f * S.Along - 18.f, S.Across * .5f), Z, S.Yaw, false);
    Kit.FillBookcase(S.At(2.f * S.Along - 18.f, S.Across * .5f), Z, S.Yaw, Seed + 2);
}

void RoomReception(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, FName Sign, int32 Seed)
{
    // A front lobby: a reception counter facing the doors (Front = the doors' side), benches by the walls, a plant, a notice
    // board, a sign over the counter, a rug inside the doors.
    const FRoomSide S(R, Front);
    const FVector2D Back = S.At(2.f * S.Along - 120.f, 0.f);
    Box(Solid, TEXT("CI_Oak"), FVector(Back - FVector2D(FMath::Abs(S.N.Y) * 150.f + FMath::Abs(S.N.X) * 30.f, FMath::Abs(S.N.X) * 150.f + FMath::Abs(S.N.Y) * 30.f), Z),
        FVector(Back + FVector2D(FMath::Abs(S.N.Y) * 150.f + FMath::Abs(S.N.X) * 30.f, FMath::Abs(S.N.X) * 150.f + FMath::Abs(S.N.Y) * 30.f), Z + 104.f));
    Kit.Floor(HouseCampusInteriors::EKit::TableLamp, Back + S.Tangent() * 110.f, Z + 104.f, S.Yaw, true);
    Kit.Floor(HouseCampusInteriors::EKit::PaperStack, Back - S.Tangent() * 80.f, Z + 104.f, Seed * 13.f, true);
    Kit.Floor(HouseCampusInteriors::EKit::EntryRug, S.At(110.f, 0.f), Z, S.Yaw, false);
    for (const float Side : { -1.f, 1.f })
    {
        Kit.Floor(HouseCampusInteriors::EKit::SpindleChair, S.At(S.Along, Side * (S.Across - 35.f)), Z, S.Yaw + (Side > 0.f ? -90.f : 90.f), true);
        Kit.Floor(HouseCampusInteriors::EKit::SpindleChair, S.At(S.Along + 60.f, Side * (S.Across - 35.f)), Z, S.Yaw + (Side > 0.f ? -90.f : 90.f), true);
    }
    Kit.Floor(HouseCampusInteriors::EKit::PlantStand, S.At(60.f, S.Across - 50.f), Z, S.Yaw, true);
    BulletinBoard(Solid, Clutter, FVector(S.At(2.f * S.Along - 1.f, -S.Across * .6f), Z + 150.f), FVector(S.N, 0.f), 120.f, 80.f, NAME_None);
    if (!Sign.IsNone()) HangingSign(Clutter, Sign, FVector(S.At(2.f * S.Along - 160.f, 0.f), Z + 290.f), FVector(-S.N, 0.f), 220.f, 34.f, Z + 340.f);
    Kit.Floor(HouseCampusInteriors::EKit::FireExtinguisher, S.At(2.f * S.Along - 10.f, S.Across - 30.f), Z + 80.f, S.Yaw + 180.f, true);
    WallClock(Clutter, FVector(S.At(2.f * S.Along, -S.Across * .25f), Z + 262.f), FVector(S.N, 0.f), 16.f);
}

void RoomWaiting(FHallMesh& Solid, FHallMesh& Clutter, FHallMesh& Glow, const FKitter& Kit, const FBox2D& R, float Z, int32 Front, int32 Seed)
{
    // A clinic waiting room: rows of chairs facing the window desk (a magazine left on it), a portable TV on a stand.
    const FRoomSide S(R, Front);
    Box(Solid, TEXT("CI_Beige"), FVector(S.At(30.f, -120.f) - FVector2D(30.f), Z), FVector(S.At(30.f, 120.f) + FVector2D(30.f), Z + 110.f));
    for (float D = 200.f; D < 2.f * S.Along - 60.f; D += 90.f)
        for (float U = -S.Across + 60.f; U < S.Across - 40.f; U += 55.f)
            Kit.Floor(HouseCampusInteriors::EKit::GreenSpindleChair, S.At(D, U), Z, S.Yaw + 180.f, true);
    CrtTelevision(Solid, Glow, FVector(S.At(2.f * S.Along - 40.f, S.Across - 50.f), Z + 90.f), FVector(-S.N, 0.f), false);
    Box(Solid, TEXT("CI_Steel"), FVector(S.At(2.f * S.Along - 40.f, S.Across - 50.f) - FVector2D(25.f), Z), FVector(S.At(2.f * S.Along - 40.f, S.Across - 50.f) + FVector2D(25.f), Z + 90.f));
    Kit.Floor(HouseCampusInteriors::EKit::RedBook, S.At(30.f, 70.f), Z + 110.f, Seed * 29.f, true);
    WallClock(Clutter, FVector(S.At(0.f, S.Across * .6f), Z + 240.f), FVector(-S.N, 0.f), 14.f);
}

void TrophyCase(FHallMesh& Solid, FHallMesh& Glow, const FVector& C, const FVector& Out, float W)
{
    // A lit glass trophy case against a wall: oak base and top, glass front, cups and plaques on three shelves.
    const FVector N = Out.GetSafeNormal2D(), U = FVector(-N.Y, N.X, 0.f);
    const FRotator Rot = N.Rotation();
    Solid.Box(TEXT("CI_OakDark"), FTransform(Rot, C + N * 22.f + FVector(0, 0, 40.f)), FVector(22.f, W * .5f, 40.f), UVPlain());
    Solid.Box(TEXT("CI_OakDark"), FTransform(Rot, C + N * 22.f + FVector(0, 0, 214.f)), FVector(22.f, W * .5f, 6.f), UVPlain());
    Solid.Box(TEXT("CI_Glass"), FTransform(Rot, C + N * 43.f + FVector(0, 0, 147.f)), FVector(.5f, W * .5f, 61.f), UVPlain());
    for (const float S : { -1.f, 1.f }) Solid.Box(TEXT("CI_OakDark"), FTransform(Rot, C + N * 22.f + U * (S * (W * .5f - 2.f)) + FVector(0, 0, 147.f)), FVector(22.f, 2.f, 61.f), UVPlain());
    Glow.Box(TEXT("CI_HoodLight"), FTransform(Rot, C + N * 22.f + FVector(0, 0, 206.f)), FVector(18.f, W * .5f - 6.f, .5f), UVPlain());
    for (int32 Shelf = 0; Shelf < 3; ++Shelf)
    {
        const float Z = 86.f + Shelf * 42.f;
        Solid.Box(TEXT("CI_Glass"), FTransform(Rot, C + N * 22.f + FVector(0, 0, Z)), FVector(20.f, W * .5f - 4.f, .6f), UVPlain());
        for (float T = -W * .5f + 25.f; T < W * .5f - 15.f; T += 32.f)
        {
            const FVector P = C + N * 20.f + U * T + FVector(0, 0, Z + .6f);
            Solid.Cylinder(TEXT("CI_Brass"), P, P + FVector(0, 0, 4.f), 5.f, 10, UVPlain());
            Solid.Cylinder(TEXT("CI_Brass"), P + FVector(0, 0, 4.f), P + FVector(0, 0, 14.f + (int32(T) % 3) * 4.f), 1.2f, 6, UVPlain());
            HouseCampusFallKit::Ball(Solid, TEXT("CI_Brass"), P + FVector(0, 0, 20.f + (int32(T) % 3) * 4.f), 6.f, 8);
        }
    }
}
}
