#pragma once
#include "CoreMinimal.h"
struct FHallMesh;
class AActor;
class UHouseBlackHallSubsystem;

/** Five Row yards. Coordinates are world centimetres; no terrain/woods-fence edits, lights or shared RNG. */
namespace HouseFraternityRowYards
{
/** Render-only existing grass and shrub filter. Pass the built mesh's transformed world XY bounds.
 *  Tests only the new R4/R5 solid footprints and five sheds; false in legacy mode. Never mutates scatter/RNG. */
bool ExistingVegetationBlocked(const FBox2D& WorldBounds);
void CreateLook(UHouseBlackHallSubsystem* Look);
/** Solid = fences, walk-in sheds and substantial furnishings; Decor/Glow never collide. */
void Build(FHallMesh& Solid,FHallMesh& Decor,FHallMesh& Glow);
/** Retained by Owner->InstanceComponents. Plain ISM batches, no collision/shadows/indirect lighting. */
void AttachInstances(AActor* Owner,UHouseBlackHallSubsystem* Look);
}
