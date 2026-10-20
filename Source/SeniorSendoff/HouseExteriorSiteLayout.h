#pragma once

// World-centimeter coordinates shared by the street, concrete approach and runtime review.
namespace HouseExteriorSite
{
constexpr float FrontStreetSetback = 1200.f;
constexpr float StreetY(float OriginalY) { return OriginalY - FrontStreetSetback; }
constexpr float FrontRoadNearY = StreetY(-2950.f);
constexpr float FrontRoadFarY = StreetY(-4350.f);
constexpr float FrontSidewalkInnerY = StreetY(-2700.f);
// Property-sized play area: both street sidewalks, the court and the full
// detailed woodland remain inside; neighbouring houses and campus are scenery.
constexpr float PlayableWestX = -8500.f;
constexpr float PlayableEastX = 8800.f;
constexpr float PlayableFrontY = -5900.f;
constexpr float PlayableRearY = 19800.f;
constexpr float BoundaryHalfThickness = 60.f;
// The invisible player-only walls along those four lines (PropertyBoundary_* in AHouseExteriorDetail) are OFF for now so
// the whole site can be walked (2026-10-13). Set this true to bring them back for everyone. -PropertyBoundary turns them
// on for one run (the -FraternityBoundaryReview check uses it); in multiplayer pass it to every instance, because each
// machine builds its own exterior. Walking off the edge of the world (+/-32000) is caught by the story game mode's fall
// respawn, except on a save whose chapter is already completed (AStoryGameMode::Tick skips it then).
constexpr bool bPropertyBoundaryOn = false;
bool PropertyBoundaryEnabled();
/** The ground every placer outside the house stands on (Calder terrain job): today's grade plus the Calder relief
 *  (HouseCalderTerrain). Equals today's grade exactly in the frozen zones and with -NoCalderTerrain. */
float GroundHeight(float X, float Y);
FVector GroundNormal(float X, float Y);
/** Today's grade alone (the site's -60 / -362 planes, the bank, the woods' hills, the west field's mound), without the
 *  Calder relief: what the relief is added to. */
float BaseGrade(float X, float Y);
}
