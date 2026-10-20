#pragma once

#include "CoreMinimal.h"

struct FHallMesh;
class UHouseBlackHallSubsystem;

/** Where the campus clock tower stands (world cm) and the heights other code needs. Heights are above the tower's foot:
    campus grade (GroundZ, today's), raised by Lift() onto Tower Knoll. */
namespace HouseClockTowerSite
{
constexpr float CenterX = -9000.f, CenterY = -10800.f, GroundZ = -60.f;   // inside the band no grid tree can reach (Y -11200..-9950)
constexpr float DialHeight = 4000.f, DialRadius = 185.f, ClockHalf = 312.f;
constexpr float BelfryHeight = 3150.f, TopHeight = 7040.f;
/** How far the tower stands above GroundZ on Tower Knoll (the Calder terrain; 0 with it off). Its XY never changes. */
float Lift();
/** A point on the axis Height above the tower's foot, in the world: the knoll's Lift() is in it. The actor stays at the
    origin; every world position of the tower (its static parts, DialCenter, BellPivot, HammerPlacement, VanePivot,
    CrowPerches, the bell voices, the crows' flight and the walk-in inside's TwrBase) takes the lift from here or adds
    Lift() itself, exactly once. */
inline FVector Center(float Height) { return FVector(CenterX, CenterY, GroundZ + Lift() + Height); }
}

/** Geometry and look of the campus clock tower, all made in code. Static parts are written in world space (on the knoll:
    Center() and the foot datum carry HouseClockTowerSite::Lift()); moving parts (hands, bells, clappers, weathervane,
    crows) in their own local space around their pivots, which are world positions on the knoll too. */
namespace HouseClockTowerMesh
{
constexpr int32 NumFaces = 4, NumBells = 5;
/** Face 0 looks +Y (along the campus walk toward the fraternity house), 1 +X, 2 -Y, 3 -X. Right is the viewer's right. */
FVector FaceNormal(int32 Face);
FVector FaceRight(int32 Face);
/** Centre of a dial on the face of the clock stage (world, on the knoll: Lift() is in it). */
FVector DialCenter(int32 Face);

/** Registers the CT_ materials and their painted textures (idempotent). */
void CreateLook(UHouseBlackHallSubsystem* Look);
/** Solid: plinth, base and shaft (gets collision). Upper: belfry to finial (no collision, never culled).
    Detail: small ornament read only up close (no collision, culled). Dials: the four faces without their hands. */
void Build(FHallMesh& Solid, FHallMesh& Upper, FHallMesh& Detail, FHallMesh& Dials);
/** The walk-in tower's inside (HouseCampusInteriorsTower, while HouseCampusInteriors::ClockTowerOpen): a lining for the
    shaft's walls (Dist 258..260 from the axis, from the floor at 75 to the course under the belfry at 2780; Dist 258..270 on
    the door wall below the base course) with the doorway and every lancet opened through it exactly where the shell has
    them, their reveals in RevealKey; the door's tympanum seen from inside in DoorBack (the shell's fill over the leaves faces
    out only); and Panes, the lancets' glass seen from inside (the shell's fills face out only), facing in 1 cm inside each
    fill, GlassLit where the outside shows a lit window. World space like the shell (Lift() is in it). */
void ShaftLining(FHallMesh& M, FHallMesh& Panes, FName Key, FName RevealKey, FName Glass, FName GlassLit, FName DoorBack);

/** Hands point along local +Z (twelve o'clock) with local +X toward the viewer; the pivot is the origin. */
void HourHand(FHallMesh& M);
void MinuteHand(FHallMesh& M);
/** Bell 0..3 are the quarter bells (0 highest), 4 the hour bell. Pivot (world) is the headstock axis, which runs along X. */
FVector BellPivot(int32 Bell);
float BellDiameter(int32 Bell);
/** Bell and clapper hang below the origin. */
void Bell(FHallMesh& M, float Diameter);
void Clapper(FHallMesh& M, float Diameter);
/** Clock bells hang still and are struck by hammers outside the sound bow. The hammer's local X points away from its
    bell; its head rests against the bell at zero pitch and lifts away with positive pitch. */
void HammerPlacement(int32 Bell, FVector& OutPivot, FVector& OutAway);
void Hammer(FHallMesh& M, float Diameter);
/** The weathervane's arrow (it turns about local Z at the pivot). */
FVector VanePivot();
void Vane(FHallMesh& M);

/** Ledges the crows sit on (world; X faces away from the tower). */
void CrowPerches(TArray<FTransform>& Out);
/** One crow in a pose: Flap -1..1 beats the wings, Fold 0..1 folds them along the body. Always the same vertex count. */
void AppendCrow(const FTransform& Pose, float Flap, float Fold, TArray<FVector>& V, TArray<FVector>& N, TArray<int32>& I);
}
