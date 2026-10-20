// Fraternity Park Dr at dusk, Halloween 1986: a worn, patched road with a concrete gutter, inlets, iron covers and fallen
// leaves; wooden utility poles carrying sodium cobra-head streetlights and sagging wires; the front-yard party (kegs, red
// cups, webbed lawn chairs, a smoking kettle grill) under a hand-painted HALLOWEEN BASH banner; and a payphone with a coin
// newspaper box on the corner by the side drive. Adds no lights: the existing pole lights become sodium spots.
#pragma once

#include "CoreMinimal.h"

struct FHallMesh;
struct FHallImage;
class UHouseBlackHallSubsystem;
class UTexture2D;
class UMaterialInterface;

namespace HouseStreetScene
{
/** Materials and painted textures. Runs after HouseSurroundings::CreateLook (it replaces EX_StreetAsphalt). */
void CreateLook(UHouseBlackHallSubsystem* Look);
/** True once the decal material loaded: the decal markings then replace the old painted lines east of MarkingsStartX. */
bool DecalsReady(UHouseBlackHallSubsystem* Look);
constexpr float MarkingsStartX = -20000.f;

/** Street poles (both kerbs of the front street and the side street) are wooden utility poles that carry the wires. */
bool IsStreetPole(float X, float Y);
/** Light position in a pole's frame (x toward the road, origin on the ground). The same for street and lot poles. */
FVector LensOffset();
FLinearColor SodiumColor();
void StreetPoleMesh(FHallMesh& M, TArray<FBox>& Collision);
void LotPoleMesh(FHallMesh& M, TArray<FBox>& Collision);

/** Flat = gutter pans (take the road decals); Clean = iron covers, grates, leaves and litter on the paving (no decals);
 *  neither has collision or shadows. Solid = street furniture (collision, shadows). */
void BuildRoad(FHallMesh& Flat, FHallMesh& Clean, FHallMesh& Solid);
void BuildWires(FHallMesh& Wires, const TArray<FTransform>& StreetPoles);
/** Props = kegs, tub, chairs, grill, cooler (collision, shadows); Litter = red cups and other small pieces (no collision). */
void BuildParty(FHallMesh& Props, FHallMesh& Litter);
void BuildBanner(FHallMesh& Banner);
void BuildCorner(FHallMesh& Solid);
/** Where the grill smoke starts (just above the coals). */
FVector GrillSmokeOrigin();
/** Out front at the near kerb (y -4300, facing +x): the woody wagon, the square-bodied sedan and the hatchback, all west of the
 *  front walk because the exterior walk review crosses the near lane east of it, and clear of its asphalt samples at x 0 and
 *  -2000 (y -4350). Shared with the oil stains under them. */
constexpr float KerbCarX[3] = { -560.f, -1300.f, -2900.f };

/** Road decals: projected straight down. HalfSize = (depth, half width across y before yaw, half length along x before yaw). */
struct FDecal { FName Material; FVector Center; float Yaw; FVector HalfSize; int32 Sort; };
void RoadDecals(TArray<FDecal>& Out);

// Engine-side helpers (HouseDuskAtmosphere.cpp).
/** Transient texture that keeps the image's alpha (HallMakeTexture always writes opaque). */
UTexture2D* MakeTexture(const FHallImage& Img, bool bSRGB, bool bNormalMap, const TCHAR* Name, bool bWrap);
/** Instance of M_StreetDecal (deferred decal) registered on the subsystem; null if the parent material is missing. */
UMaterialInterface* MakeDecalMaterial(UHouseBlackHallSubsystem* Look, FName Key, UTexture2D* Atlas, float Roughness, float Opacity, const FLinearColor& Tint);
/** Instance of M_StreetSmoke (lit translucent, vertex colour alpha) registered on the subsystem; null if missing. */
UMaterialInterface* MakeSmokeMaterial(UHouseBlackHallSubsystem* Look, FName Key, UTexture2D* Puff);
}
