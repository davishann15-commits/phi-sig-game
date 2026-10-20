#pragma once
#include "CoreMinimal.h"

struct FHallMesh;
struct FHallUV;

/** Owned approach paving; no actors, ticks, lights or changes to existing roads. */
namespace HouseCalderRoads
{
bool Active();
bool Contains(const FVector2D& P, float Pad = 0.f);
/** Finished paving offset above the baked earthwork. Recessed joins meet the existing lane and concrete bridge exactly. */
bool PaveOffsetAt(const FVector2D& P, float& OutOffset);
bool FloorAt(const FVector2D& P, float& OutZ);
void TerrainCuts(TArray<FBox2D>& Out);
void BuildGround(FHallMesh& M, FName Asphalt, const FHallUV& UV);
void FloorSamples(TArray<FVector>& Out);
}
