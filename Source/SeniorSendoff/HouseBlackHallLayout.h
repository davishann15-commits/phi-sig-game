#pragma once

#include "CoreMinimal.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"

/** Measured layout of the rebuilt black hall (world cm). Shared by the builder and the door spawner. */
namespace HallLayout
{
    struct FPairSpec { float Center, FrameWidth; uint8 ActiveFlags, PassiveFlags; };
    // Patio pairs along the north wall, east to west (photo order 1..4).
    inline const FPairSpec Pairs[4] = {
        { 603.f, 270.f, AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::Deadbolt, AHouseBlackHallDoor::FlushBolts },
        { 0.f, 270.f, AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::Deadbolt | AHouseBlackHallDoor::KickPlate | AHouseBlackHallDoor::Closer,
            AHouseBlackHallDoor::KickPlate | AHouseBlackHallDoor::Closer | AHouseBlackHallDoor::FlushBolts },
        { -603.f, 270.f, AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::Deadbolt | AHouseBlackHallDoor::KickPlate | AHouseBlackHallDoor::Closer,
            AHouseBlackHallDoor::KickPlate | AHouseBlackHallDoor::Closer | AHouseBlackHallDoor::FlushBolts },
        { -1092.f, 230.f, AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::Deadbolt, AHouseBlackHallDoor::FlushBolts },
    };
    inline constexpr float Casing = 9.f, Liner = 1.6f, WallT = 38.f, DoorTop = HallSpec::ZF + 216.f;
    inline constexpr float ExitA0 = 432.f, ExitA1 = 548.f, EquipA0 = 659.f, EquipA1 = 753.f;
    inline constexpr float RecessA0 = -230.f, RecessA1 = 370.f, RecessDepth = 76.f, RecessTop = -95.f;

    inline TArray<FBox> CarveBoxes()
    {
        using namespace HallSpec;
        TArray<FBox> Boxes;
        for (const FPairSpec& P : Pairs)
            Boxes.Add(FBox(FVector(P.Center - P.FrameWidth * .5f + Casing, 775.f, ZF + 1.f), FVector(P.Center + P.FrameWidth * .5f - Casing, 850.f, DoorTop)));
        Boxes.Add(FBox(FVector(1205.f, ExitA0, ZF + 1.f), FVector(1280.f, ExitA1, DoorTop)));
        Boxes.Add(FBox(FVector(1205.f, RecessA0, ZF - 12.f), FVector(1301.f, RecessA1, RecessTop)));
        return Boxes;
    }
}
