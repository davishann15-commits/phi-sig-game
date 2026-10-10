#pragma once

struct FHallMesh;
class UHouseBlackHallSubsystem;

/** Front concrete landing and grade-following paths, generated with the existing exterior. */
void CreateHouseExteriorWalkLook(UHouseBlackHallSubsystem* Look);
void BuildHouseExteriorWalks(FHallMesh& Solid, FHallMesh& Decor);

