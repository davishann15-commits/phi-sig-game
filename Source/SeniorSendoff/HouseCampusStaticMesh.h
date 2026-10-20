// FHallMesh -> a transient UStaticMesh built at runtime (master job, Phase 4): its draws are cached by the renderer, where
// a procedural mesh is gathered again every frame. Used by the campus polish and the campus interiors.
#pragma once

#include "CoreMinimal.h"
#include "HouseBlackHallMesh.h"

class UStaticMesh;
class UHouseBlackHallSubsystem;

namespace HouseCampusStaticMesh
{
/** One polygon group a material key (its material from Look); no collision; null if the build fails or it is empty. */
UStaticMesh* Build(const FHallMesh& Source, FName Name, UHouseBlackHallSubsystem* Look);
/** The mesh in pieces of at most MaxTriangles (whole triangles, materials kept), so each can be built in a short frame. */
TArray<FHallMesh> Split(const FHallMesh& Source, int32 MaxTriangles);
}
