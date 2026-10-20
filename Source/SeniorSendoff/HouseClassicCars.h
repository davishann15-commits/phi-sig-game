// Procedural classic cars (1950s-80s) for the house exterior: land-yacht sedan, wood-panel wagon, square-body pickup,
// 80s hatchback, 50s two-tone hardtop and one original fastback muscle car. All shapes are original designs (no real
// model's look or badges). Each type is built once at runtime; paint is swapped per parked car through material overrides.
#pragma once

#include "CoreMinimal.h"

struct FHallMesh;
class UHouseBlackHallSubsystem;

enum class EExCar : uint8 { Yacht, Wagon, Pickup, Hatch, Fifties, Muscle, Cruiser, Count };   // Cruiser: 80s black-and-white police sedan

/** Material slot names on every car mesh; CAR_Paint / CAR_Paint2 / CAR_Vinyl / CAR_WheelPaint are overridden per paint scheme. */
FName ExCarSlot(int32 Index);
namespace ExCarSlots { enum : int32 { Paint = 0, Paint2 = 1, Vinyl = 2, WheelPaint = 11 }; }

/** One paint scheme for a car type: "CARP_<Colour>" material keys; NAME_None keeps the mesh's own material. */
struct FExCarScheme
{
    EExCar Type = EExCar::Yacht;
    FName Paint, Paint2, Vinyl, WheelPaint;
};
/** Every paint scheme, grouped by type in EExCar order. */
const TArray<FExCarScheme>& ExCarSchemes();
const TCHAR* ExCarTypeName(EExCar Type);

/** Registers chrome, glass, tyre, lamp, plate, wood... and the paint colours (keys "CARP_<Name>") on the look subsystem. */
void ExCreateCarLook(UHouseBlackHallSubsystem* Sub);
/** One car type in local space (+X forward, origin on the ground, real centimetres). Collision boxes are local too. */
void ExBuildCar(EExCar Type, FHallMesh& Mesh, TArray<FBox>& Collision);
/** Overall length in real centimetres. */
float ExCarLength(EExCar Type);
/** Distance (real cm, along local +X) from the car origin to the front of the front tyre, Height cm above the ground. */
float ExCarFrontTyreReach(EExCar Type, float Height);
