// The porte-cochere at the east end of the house, rebuilt from Jacob's photos of the real one: a black-shingled hip roof on a
// white paneled entablature, paired white columns on brick plinths with a brick knee wall between them, a railed brick stoop
// at the side door with wall lanterns, a hanging lantern, the fire-department sign and mailbox, and yellow bollards round the
// gas meter. The drive still passes under it to the rear lot. Adds no lights (the old ceiling light becomes the lantern's).
#pragma once

#include "CoreMinimal.h"

struct FHallMesh;
class UHouseBlackHallSubsystem;

namespace HouseSideCanopy
{
/** Materials (SC_*). Runs after ExCreateLook (uses EX_Brick, EX_Stone, EX_Concrete, EX_ShingleBlack). */
void CreateLook(UHouseBlackHallSubsystem* Look);
/** Builds into the house mesh (collision and shadows). */
void Build(FHallMesh& House);
/** Where the hanging lantern's light goes. */
FVector LanternLight();
}
