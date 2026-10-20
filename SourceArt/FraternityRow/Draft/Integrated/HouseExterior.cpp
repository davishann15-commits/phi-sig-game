// Runtime-built exterior of the fraternity house and its night-time surroundings. See HouseExterior.h.
// The upstairs impossible hallway is trimmed in the assets (Saved/exterior_cut2.py) so it ends inside the left wing's front wall.
// Coordinates are world cm of the saved house (1 real m = 198 cm horizontally, 110 cm vertically).
#include "HouseExterior.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderMapTrees.h"
#include "HouseCalderTerrainTiles.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderEdges.h"
#include "HouseCalderLandform.h"
#include "HouseCalderOuterLand.h"
#include "HouseCalderFootPoints.h"
#include "HouseCalderWater.h"
#include "HouseCalderRoadside.h"
#include "HouseExteriorWalks.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseExteriorNature.h"
#include "HouseExteriorBrick.h"
#include "HouseExteriorCars.h"
#include "HouseClassicCars.h"
#include "HouseParkingDetail.h"
#include "HouseStreetScene.h"
#include "HouseDuskAtmosphere.h"
#include "HouseSideCanopy.h"
#include "HouseFallJam.h"
#include "HouseExteriorSpatial.h"
#include "HouseExteriorCampus.h"
#include "HouseClockTower.h"
#include "HouseClockTowerMesh.h"
#include "HouseCampusFall.h"
#include "HouseCampusLayout.h"
#include "HouseCampusStreetDetail.h"
#include "HouseCampusInteriors.h"
#include "HouseCampusPolish.h"
#include "HouseFraternityRow.h"
#include "HouseFraternityRowYards.h"
#include "HouseSurroundingsPolish.h"
#include "HouseWestField.h"
#include "HouseParkingLot.h"
#include "HouseWoodsDetail.h"
#include "HouseWoodsPass.h"
#include "HouseWoodsTraces.h"
#include "HouseWoodsStructure.h"
#include "HouseWoodsFloor.h"
#include "HouseWoodsAtmosphere.h"
#include "HouseWoodsSilhouettes.h"
#include "HouseWoodcutting.h"
#include "HouseFrontPortico.h"
#include "HouseLeafWind.h"
#include "HouseCalderCover.h"
#include "HouseCalderFarm.h"
#include "HouseCalderFarmTree.h"
#include "HouseCalderHedgerow.h"
#include "HouseCalderLandscape.h"
#include "HouseCalderMapTrees.h"
#include "HouseTreeReaction.h"
#include "HouseBasketballCourt.h"
#include "HouseBasketball.h"
#include "HouseBasketballHoop.h"
#include "HouseRearPorch.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCarryTiersReview.h"
#include "HouseDetail.h"
#include "HouseReactiveFixture.h"
#include "ExistingHouseDoor.h"
#include "StoryCampaign.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/HUD.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/LocalFogVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "RenderCore.h"
#include "RenderTimer.h"
#include "Stats/Stats.h"
#include "UnrealClient.h"

bool HouseExteriorSite::PropertyBoundaryEnabled()
{
    return bPropertyBoundaryOn || FParse::Param(FCommandLine::Get(), TEXT("PropertyBoundary"));
}

namespace
{
using HouseExteriorSite::StreetY;
// ------------------------------------------------------------------ site levels
constexpr float GF = -60.f;          // front / street grade
constexpr float GR = -362.f;         // rear courtyard grade (= basement floor)
constexpr float ZTop = 640.f;        // brick stops, cornice starts
constexpr float ZEave = 692.f;       // cornice top = roof eave
constexpr float RoofPitch = .36f;    // 8:12 after the house's vertical squash
constexpr float SlopeY0 = 0.f, SlopeY1 = 1600.f;
float Grade(float X, float Y)
{
    if (Y <= SlopeY0) return GF;
    if (Y >= SlopeY1) return HouseWoods::Height(X,Y);
    return HouseWestField::RaisedHeight(X,Y,GF + (GR - GF) * ((Y - SlopeY0) / (SlopeY1 - SlopeY0)));
}
}

float HouseExteriorSite::GroundHeight(float X, float Y)
{
    // Inside a frozen zone the ground is today's exactly, even where the lattice cell it falls in reaches outside the zone.
    const float G = Grade(X, Y);
    return HouseCalderTerrain::InFrozen(X, Y) ? G : G + HouseCalderTerrain::ReliefLattice(X, Y);
}
float HouseExteriorSite::BaseGrade(float X, float Y) { return Grade(X, Y); }
FVector HouseExteriorSite::GroundNormal(float X, float Y)
{
    constexpr float D = 100.f;
    const float DX = GroundHeight(X + D, Y) - GroundHeight(X - D, Y), DY = GroundHeight(X, Y + D) - GroundHeight(X, Y - D);
    return FVector(-DX, -DY, 2.f * D).GetSafeNormal();
}

namespace
{

// Fixed camera spots used by -HouseExteriorViews; the planting keeps trees away from them.
struct FExView { const TCHAR* Name; FVector Pos, Target; float Fov = 0.f; float OrthoWidth = 0.f; float OrthoNear = 0.f; };   // Fov 0 = the default 85; OrthoWidth > 0 = a straight-down orthographic plan (cm across)
const FExView ExViews[] = {
    // Detail views never participate in the original planting exclusion/RNG.
    { TEXT("detail_woods_edge"), FVector(-2450,4320,-190), FVector(-3200,6000,5), 76.f },
    { TEXT("detail_woods_20m"), FVector(-3310,6550,-195), FVector(-2900,8200,-50), 76.f },
    { TEXT("detail_woods_deep"), FVector(-1230,11000,-192), FVector(-2150,12450,-60), 76.f },
    { TEXT("detail_woods_ravine"), FVector(-4300,7700,-200), FVector(-4100,8150,-438), 74.f },
    { TEXT("detail_woods_silhouette"), FVector(-2720,9330,-164), FVector(-3910,10400,480), 70.f },
    { TEXT("detail_woods_house_back"), FVector(-3350,6600,-182), FVector(-1300,1200,285), 74.f },
    { TEXT("detail_woods_trace_fridge"), FVector(-4320,7760,-265), FVector(-4040,8090,-405), 65.f },
    { TEXT("detail_woods_trace_sign"), FVector(-2700,5400,-190), FVector(-2390,5530,-198), 62.f },
    { TEXT("detail_woods_trace_coat"), FVector(-2200,8650,-170), FVector(-1613,8871,-55), 66.f },
    { TEXT("detail_woods_trace_stand"), FVector(-5510,10850,160), FVector(-5740,11260,85), 70.f },
    { TEXT("detail_woods_trace_swing"), FVector(-920,14300,-175), FVector(-639,14513,-85), 70.f },
    { TEXT("detail_woods_trace_paddle"), FVector(-145,12620,-185), FVector(168,12886,-210), 65.f },
    { TEXT("detail_woods_trace_ashes"), FVector(-2670,11475,-200), FVector(-2470,11680,-348), 70.f },
    { TEXT("detail_woods_trace_hut"), FVector(-1080,13200,-135), FVector(-800,13600,-170), 74.f },
    { TEXT("great_entrance"),  FVector(1184, 624, 185),     FVector(1184, 624, 185) + FRotator(3, -146, 0).Vector() * 500 },
    { TEXT("great_fireplace"), FVector(-267, 614, 185),     FVector(-267, 614, 185) + FRotator(2, -143, 0).Vector() * 500 },
    { TEXT("gallery_hall"),    FVector(-1317, -752, 503),   FVector(-817, -752, 495) },
    { TEXT("rear_hall_upper"), FVector(-2395, 693, 514),    FVector(-2395, 193, 514) },
    { TEXT("deck"),            FVector(1250, 1180, 177),    FVector(-1300, 1250, 120) },
    { TEXT("stair_window"),    FVector(-1789, 1109, 476),   FVector(-1789, 2500, 420) },
    { TEXT("balcony_out"),     FVector(-19, -1198, 517),    FVector(-19, -4000, 250) },
    { TEXT("porch_facade"),    FVector(-10, 2297, 314),     FVector(-10, 2297, 314) + FRotator(-9, -90, 0).Vector() * 500 },
    { TEXT("porch_end"),       FVector(-1832, 2188, 292),   FVector(-1832, 2188, 292) + FRotator(-5, -41, 0).Vector() * 500 },
    { TEXT("deck_edge"),       FVector(-500, 1500, 175),    FVector(-500, 2700, -320) },
    { TEXT("patio_doors"),     FVector(380, 1650, -190),    FVector(0, 930, -260) },
    { TEXT("front_street"),    FVector(-300, StreetY(-4500), 150),   FVector(-300, -1010, 300) },
    { TEXT("front_walk"),      FVector(-20, -2600, 120),    FVector(-20, -1300, 350) },
    { TEXT("front_lawn_overview"), FVector(4600, -6050, 2400), FVector(-250, -2800, 20) },
    { TEXT("lawn_toward_road"), FVector(-36.65, -2180, 130), FVector(-36.65, HouseExteriorSite::FrontRoadFarY, -60) },
    { TEXT("street_along"),    FVector(-7200, StreetY(-3650), 170),  FVector(6000, StreetY(-3650), 150) },
    { TEXT("right_end"),       FVector(6200, -2800, 140),   FVector(3340, -800, 250) },
    { TEXT("back_court"),      FVector(800, 3400, -190),    FVector(-500, 2200, -200) },
    { TEXT("back_stair"),      FVector(-2600, 3200, -190),  FVector(-1190, 1800, -100) },
    { TEXT("left_rear"),       FVector(-5500, 3500, -150),  FVector(-3400, 600, 300) },
    { TEXT("left_front"),      FVector(-5600, -4300, 130),  FVector(-2800, -2200, 380) },
    { TEXT("high"),            FVector(7000, -8000, 3200),  FVector(0, 0, 0) },
    { TEXT("yard_walk"),       FVector(-3900, -2400, 110),  FVector(-1500, -1500, 150) },
    { TEXT("nature_fall_tree"), FVector(700, -3550, 130),   FVector(2200, -2650, 480) },
    { TEXT("nature_shrubs"),    FVector(960, -2780, 80),   FVector(970, -2390, 12) },
    { TEXT("nature_grass"),     FVector(-1800, -3200, 90), FVector(-2400, -2650, -30) },
    { TEXT("detail_brick_window"), FVector(-1800, -2850, 150), FVector(-2100, -2200, 165) },
    { TEXT("detail_brick_arch"), FVector(5, 2150, -200), FVector(5, 1710, -105) },
    { TEXT("detail_grass_lull"), FVector(-1800, -3200, 35), FVector(-2200, -2880, -25) },
    { TEXT("detail_grass_gust"), FVector(-1800, -3200, 35), FVector(-2200, -2880, -25) },
    { TEXT("detail_cars"), FVector(-5960, -2740, 120), FVector(-5650, -2100, 60) },
    { TEXT("detail_car_muscle"), FVector(-4560, -360, 165), FVector(-5045, -560, 70) },
    { TEXT("detail_car_muscle_rear"), FVector(-4560, -1250, 165), FVector(-5045, -820, 60) },
    { TEXT("detail_cars_upper_row"), FVector(-6650, -1500, 175), FVector(-5700, -650, 70) },
    { TEXT("detail_cars_lower_row"), FVector(-4300, -1300, 180), FVector(-5800, -2150, 60) },
    { TEXT("detail_cars_street"), FVector(-2600, -4050, 165), FVector(-1000, -5350, 60) },
    { TEXT("detail_lot_entrance"), FVector(-5500, -3300, 170), FVector(-5500, -2000, 10) },
    { TEXT("detail_lot_aisle"), FVector(-4250, -1350, 170), FVector(-6200, -1520, -20) },
    { TEXT("detail_lot_dumpster"), FVector(-4080, -1950, 175), FVector(-3640, -1380, 10) },
    { TEXT("detail_lot_corner"), FVector(-6080, -1620, 170), FVector(-6350, -2400, -58) },
    { TEXT("detail_lot_reserved"), FVector(-5150, -1150, 165), FVector(-5200, -262, 110) },
    { TEXT("detail_lot_walk"), FVector(-3620, -170, 170), FVector(-4500, -700, 20) },
    { TEXT("nature_oak"), FVector(-1460,-3650,140), FVector(-2350,-3250,460) },
    { TEXT("nature_birch"), FVector(-870,-3800,110), FVector(-1450,-3500,300) },
    { TEXT("nature_tree_variety"), FVector(100,-3980,150), FVector(1800,-3350,380) },
    { TEXT("nature_oak_polished"), FVector(-3400,-4660,180), FVector(-2350,-3250,475) },
    { TEXT("nature_oak_leaf_detail"), FVector(-2050,-3770,445), FVector(-2350,-3250,610) },
    { TEXT("nature_birch_leaf_detail"), FVector(-1675,-3850,345), FVector(-1450,-3500,490) },
    { TEXT("nature_maple_leaf_detail"), FVector(1880,-3070,480), FVector(2200,-2650,560) },
    { TEXT("nature_front_yard_litter"), FVector(690,-3350,95), FVector(710,-3090,-44) },
    { TEXT("nature_sticks_pinecones"), FVector(565,-3155,25), FVector(700,-2910,-52) },
    { TEXT("nature_pinecone_close"), FVector(665,-2960,-25), FVector(700,-2910,-56) },
    { TEXT("nature_stick_close"), FVector(650,-2970,-25), FVector(700,-2910,-57) },
    { TEXT("nature_trash_close"), FVector(670,-3135,-25), FVector(710,-3090,-55) },
    // Fraternity Park Dr at dusk, Halloween 1986 (HouseStreetScene / HouseDuskAtmosphere).
    // Wide establishing shots: raised across the street (the hero), eye level from the far sidewalk and the far lane, and from
    // the payphone corner down the street into the afterglow.
    { TEXT("detail_establishing_high"), FVector(1300, -5800, 800), FVector(-700, -2600, 120), 80.f },
    { TEXT("detail_establishing_front"), FVector(1700, -5700, 280), FVector(-900, -2400, 300), 80.f },
    { TEXT("detail_establishing_lane"), FVector(1800, -5050, 300), FVector(-1300, -2550, 280), 84.f },
    { TEXT("detail_establishing_corner"), FVector(5000, -4450, 165), FVector(1100, -3420, 250), 78.f },
    // The east porte-cochere (HouseSideCanopy): from the drive, from the east lawn, at the door, and high for the black roofs.
    { TEXT("detail_canopy_drive"), FVector(3750, -2650, 165), FVector(3720, -900, 210), 80.f },
    { TEXT("detail_canopy_lawn"), FVector(5400, -2000, 170), FVector(3800, -1060, 230), 70.f },
    { TEXT("detail_canopy_door"), FVector(3950, -1750, 150), FVector(3340, -1060, 140), 72.f },
    { TEXT("detail_canopy_corner"), FVector(5300, -2900, 220), FVector(3900, -1300, 260), 62.f },
    { TEXT("detail_canopy_roofs"), FVector(6200, -3600, 900), FVector(2200, -900, 350), 70.f },
    // The Fall Harvest Jam field (HouseFallJam): the whole east lawn, with the police round it.
    { TEXT("detail_jam_wide"), FVector(4400, -3300, 230), FVector(6400, -1200, 80), 84.f },
    { TEXT("detail_jam_stage"), FVector(6300, -1480, 135), FVector(6300, -520, 125), 72.f },
    { TEXT("detail_jam_drums"), FVector(6440, -800, 165), FVector(6307, -540, 100), 62.f },
    { TEXT("detail_jam_gate"), FVector(3620, -1700, 180), FVector(4700, -1060, 60), 74.f },
    { TEXT("detail_jam_fire"), FVector(7000, -2300, 150), FVector(7300, -1900, 10), 68.f },
    { TEXT("detail_jam_cider"), FVector(5960, -2560, 165), FVector(5520, -2170, 45), 64.f },
    { TEXT("detail_jam_high"), FVector(9300, -5300, 1500), FVector(6200, -1800, 0), 80.f },
    { TEXT("detail_jam_street"), FVector(6300, -4900, 175), FVector(6300, -1300, 160), 80.f },
    { TEXT("detail_jam_overhead"), FVector(6225, -1879, 4300), FVector(6225, -1880, 0), 70.f },
    { TEXT("detail_jam_police"), FVector(4200, -5050, 190), FVector(6500, -3900, 60), 78.f },
    { TEXT("detail_jam_police_side"), FVector(9500, -200, 210), FVector(8500, -2300, 40), 76.f },
    { TEXT("detail_jam_bust"), FVector(6200, -2500, 150), FVector(6900, -3250, 60), 70.f },
    { TEXT("detail_jam_hayride"), FVector(6400, -1850, 200), FVector(7850, -2850, 70), 70.f },
    { TEXT("detail_jam_kegs"), FVector(5300, -2650, 160), FVector(4400, -3150, 70), 72.f },
    { TEXT("detail_party_yard"), FVector(2300, -3950, 175), FVector(1150, -2900, 40) },
    { TEXT("detail_party_grill"), FVector(1880, -3560, 150), FVector(1500, -3150, 50) },
    { TEXT("detail_party_kegs"), FVector(1420, -3180, 140), FVector(1100, -2760, 30) },
    { TEXT("detail_party_banner"), FVector(-20, -2950, 230), FVector(-20, -1500, 640), 70.f },
    { TEXT("detail_street_payphone"), FVector(4000, -4420, 170), FVector(4200, -3930, 120) },
    { TEXT("detail_street_surface"), FVector(-300, -4700, 170), FVector(-1700, -5100, -72) },
    { TEXT("detail_street_kerb_cars"), FVector(-150, -4680, 150), FVector(-2300, -4280, 50) },
    { TEXT("detail_street_pole"), FVector(1050, -4250, 480), FVector(1500, -3840, 760) },
    { TEXT("detail_street_gutter"), FVector(-2050, -4330, 110), FVector(-2250, -4185, -70) },
    { TEXT("detail_street_dusk"), FVector(3200, -4870, 175), FVector(-6000, -4700, 140) },
    // The campus clock tower (HouseClockTower): from the east end of the frat lawn where the sight line clears the halls,
    // from above the science-center lot, close on the lawn-facing dial, and into the belfry at the hour bell.
    { TEXT("detail_clocktower_far"), FVector(2400, -3750, 140), FVector(-9000, -10800, 4366), 40.f },
    { TEXT("detail_clocktower_mid"), FVector(-420, -8100, 1500), FVector(-9000, -10800, 3150), 70.f },
    { TEXT("detail_clocktower_face"), FVector(-7800, -10620, 3990), FVector(-8688, -10800, 3940), 40.f },
    { TEXT("detail_clocktower_belfry"), FVector(-7700, -10480, 3060), FVector(-9000, -10800, 3150), 46.f },
    { TEXT("detail_clocktower_base"), FVector(-8480, -9720, 210), FVector(-9040, -10470, 330), 58.f },
    { TEXT("detail_clocktower_stair"), FVector(-8790, -10590, 200), FVector(-9000, -10800, 2000), 80.f },        // up the well from the foot of the stair
    { TEXT("detail_clocktower_bells_inside"), FVector(-8818, -10800, 2886), FVector(-9000, -10800, 3176), 70.f }, // in the belfry, at the hour bell
    // Homecoming weekend on campus (HouseCampusFall): down the field with the afterglow behind it, the north stands and
    // press box, the bulb scoreboard, the stadium gate from the street, the front green, the dorm facing the frat
    // row (clock tower behind), and the kiosk on the green.
    { TEXT("detail_stadium_field"), FVector(6900, -13300, 1350), FVector(-500, -13300, -56), 70.f },
    { TEXT("detail_stadium_stands"), FVector(3300, -11800, 1450), FVector(1800, -10250, 500), 62.f },
    { TEXT("detail_stadium_scoreboard"), FVector(2650, -14200, 1250), FVector(2400, -16972, 1720), 42.f },
    { TEXT("detail_stadium_gate"), FVector(2900, -5950, 380), FVector(2500, -9500, 950), 76.f },
    { TEXT("detail_campus_quad"), FVector(7000, -6400, 500), FVector(3200, -9050, 200), 80.f },
    { TEXT("detail_campus_dorms"), FVector(-4400, -3700, 1500), FVector(-8700, -6600, 1210), 66.f },
    { TEXT("detail_campus_kiosk"), FVector(3150, -6020, 240), FVector(2400, -6330, 180), 50.f },
    // Top-down orthographic plans (engine top view: +X right, +Y down). Named detail_ so they never touch tree planting.
    { TEXT("detail_plan_campus"), FVector(0, -17000, 30000), FVector(0, -17001, -60), 0.f, 23700.f },   // OrthoWidth spans the image height here (aspect kept on Y)
    { TEXT("detail_plan_site"), FVector(0, -4000, 30000), FVector(0, -4001, -60), 0.f, 66000.f },
    // The campus plan (HouseCampusLayout) from the ground: the Calder Dr gateway, the main quad, the curve into Stadium Dr,
    // the South Lot, an aerial, and the dorm row east of the drive.
    { TEXT("detail_layout_gateway"), FVector(9500, -4500, 230), FVector(9500, -9500, 120) },
    { TEXT("detail_layout_quad"), FVector(-14700, -10800, 260), FVector(-9000, -10800, 900) },
    { TEXT("detail_layout_curve"), FVector(10000, -15000, 700), FVector(5000, -19000, -60) },
    { TEXT("detail_layout_southlot"), FVector(8800, -19800, 900), FVector(3000, -23000, -60) },
    { TEXT("detail_layout_aerial"), FVector(16000, -1500, 7500), FVector(-3000, -15000, 0) },
    { TEXT("detail_layout_dormrow"), FVector(10400, -5900, 200), FVector(16000, -8500, 600) },
    // Step 1c/1d: the signalised mid-block crossing, Calder Dr's STOP at the junction, the tower plaza's seat wall, the
    // overflow lot and a berm, and Founders Way leaving the campus edge.
    { TEXT("detail_layout_crossing"), FVector(2380, -3960, 170), FVector(2520, -6400, 260) },
    { TEXT("detail_layout_stop"), FVector(9050, -7600, 170), FVector(8700, -5800, 190) },
    { TEXT("detail_layout_seatwall"), FVector(-10350, -9750, 190), FVector(-9000, -10800, 90) },
    { TEXT("detail_layout_overflow"), FVector(9200, -25900, 420), FVector(4800, -27900, -60) },
    { TEXT("detail_layout_berm"), FVector(-1700, -24300, 260), FVector(3200, -25500, -30) },
    { TEXT("detail_layout_westroad"), FVector(-20330, -6150, 170), FVector(-20330, -15000, 120) },
    { TEXT("detail_layout_refuge"), FVector(9050, -5980, 230), FVector(9500, -6640, -70) },
    { TEXT("detail_layout_libback"), FVector(-19800, -11650, 190), FVector(-19000, -10850, 100) },
    { TEXT("detail_layout_artsdoors"), FVector(-8200, -20900, 230), FVector(-8200, -19300, 110) },
    // Phase 2a check: a house room and a campus room of the same kind at the same eye height (170 cm above each floor):
    // the house's basement library, and Mercer Library's reference reading room (needs -CampusInteriorsAll).
    { TEXT("detail_compare_house_library"), FVector(-2950, -1350, -192), FVector(-2950, 900, -260), 80.f },
    { TEXT("detail_compare_mercer_reading"), FVector(-18880, -9300, 115), FVector(-17100, -9300, 40), 80.f },
    // Phase 2b: Mercer's house furniture up close (all need -CampusInteriorsAll): a filled reference bookcase, a lobby
    // photograph, a periodicals reading group, the circulation desk, a closed floor's interior-mapped windows from the quad.
    { TEXT("detail_lib_bookcase"), FVector(-17600, -8800, 120), FVector(-17120, -8800, 60), 70.f },
    { TEXT("detail_lib_photo"), FVector(-15850, -10600, 115), FVector(-15740, -10208, 110), 60.f },
    { TEXT("detail_lib_periodicals"), FVector(-16700, -9150, 140), FVector(-16400, -8800, 0), 75.f },
    { TEXT("detail_lib_desk"), FVector(-16600, -11100, 150), FVector(-17050, -10800, 0), 70.f },
    { TEXT("detail_lib_windows"), FVector(-12600, -12400, 160), FVector(-15600, -10200, 900), 70.f },
    { TEXT("detail_win_straight"), FVector(-8700, -6350, 960), FVector(-8700, -7100, 960), 60.f },
    { TEXT("detail_win_oblique"), FVector(-9400, -6400, 900), FVector(-8700, -7100, 980), 60.f },
    // Phase 2e: the wall clocks (they keep the tower's time) and the Harlow lounge's TV, where -CampusSoundRecord= records
    // the mix (all need -CampusInteriorsAll). The tower's lawn-facing dial last, to compare the time.
    { TEXT("detail_clock_lib"), FVector(-17900, -10800, 150), FVector(-18288, -10800, 205), 50.f },
    { TEXT("detail_clock_har_lobby"), FVector(-8900, -7370, 150), FVector(-8960, -7134, 200), 60.f },
    { TEXT("detail_clock_sci_hall"), FVector(-11900, -16050, 150), FVector(-11544, -16120, 205), 60.f },
    { TEXT("detail_clock_ker_classroom"), FVector(11480, -15060, 150), FVector(11418, -15366, 200), 60.f },
    { TEXT("detail_clock_sta_concourse"), FVector(2760, -9640, 150), FVector(2787, -9928, 205), 60.f },
    { TEXT("detail_clock_sta_pressbox"), FVector(2200, -9620, 1220), FVector(2280, -9274, 1290), 60.f },
    { TEXT("detail_clocktower_face_2e"), FVector(-7800, -10620, 3990), FVector(-8688, -10800, 3940), 40.f },
    { TEXT("detail_sound_har_tv"), FVector(-7330, -7600, 140), FVector(-7330, -7370, 70), 70.f },
    // Phase 3a: the stadium's facade, openings, structure and apron at eye level, the flood bank, the buses, the tailgates.
    { TEXT("detail_3a_south"), FVector(2600, -18700, 170), FVector(2300, -17100, 520), 74.f },
    { TEXT("detail_3a_ticketwin"), FVector(2150, -17700, 170), FVector(1961, -17090, 260), 60.f },
    { TEXT("detail_3a_east"), FVector(9400, -12600, 170), FVector(8000, -13300, 480), 74.f },
    { TEXT("detail_3a_grilleup"), FVector(-1600, -9990, 170), FVector(-1237, -10411, 220), 62.f },
    { TEXT("detail_3a_pressbox_side"), FVector(-1700, -9300, 260), FVector(1400, -9600, 1000), 66.f },
    { TEXT("detail_3a_flood"), FVector(3200, -8300, 4200), FVector(2400, -9348, 4000), 50.f },
    { TEXT("detail_3a_apron_se"), FVector(8200, -17200, 200), FVector(6200, -16000, 120), 72.f },
    { TEXT("detail_3a_buses"), FVector(5800, -19300, 220), FVector(2500, -18000, 150), 66.f },
    { TEXT("detail_3a_tailgate"), FVector(2600, -22300, 190), FVector(4200, -23300, 40), 70.f },
    // Phase 3b: each building's character (roofs, entrances, walls, windows) and the ground.
    { TEXT("detail_3b_main"), FVector(-9800, -5900, 300), FVector(-13300, -8400, 2600), 60.f },
    { TEXT("detail_3b_lib"), FVector(-11000, -12400, 900), FVector(-17300, -10800, 2500), 56.f },
    { TEXT("detail_3b_harlow"), FVector(-11000, -6200, 170), FVector(-10300, -8300, 1700), 72.f },
    { TEXT("detail_3b_dormwin"), FVector(-8300, -6450, 700), FVector(-8600, -7100, 1000), 60.f },
    { TEXT("detail_3b_sci"), FVector(-9300, -11600, 900), FVector(-12900, -15800, 2800), 60.f },
    { TEXT("detail_3b_art"), FVector(-7600, -17200, 700), FVector(-11150, -19900, 1600), 66.f },
    { TEXT("detail_3b_plant"), FVector(8800, -16600, 300), FVector(13850, -19750, 2200), 66.f },
    { TEXT("detail_3b_fld"), FVector(13600, -10600, 300), FVector(16950, -14600, 1600), 66.f },
    { TEXT("detail_3b_ash"), FVector(13600, -6300, 200), FVector(16800, -8750, 2300), 70.f },
    { TEXT("detail_3b_union"), FVector(-3200, -9600, 900), FVector(-6250, -12650, 1800), 66.f },
    { TEXT("detail_3b_hum"), FVector(-14600, -12700, 200), FVector(-17400, -16500, 2200), 70.f },
    // Phase 3c: landmarks, homecoming dressing, street furniture, the tower's base.
    { TEXT("detail_3c_statue"), FVector(-12700, -12300, 170), FVector(-11900, -11450, 170), 60.f },
    { TEXT("detail_3c_fountain"), FVector(-14100, -11950, 200), FVector(-14720, -11360, 40), 66.f },
    { TEXT("detail_3c_flag"), FVector(900, -6900, 170), FVector(-600, -7700, 700), 66.f },
    { TEXT("detail_3c_tent"), FVector(5000, -7300, 190), FVector(6700, -8500, 160), 66.f },
    { TEXT("detail_3c_float"), FVector(3700, -7300, 240), FVector(5300, -8150, 260), 62.f },
    { TEXT("detail_3c_bonfire"), FVector(2500, -6950, 320), FVector(3750, -7700, 0), 66.f },
    { TEXT("detail_3c_towerbase"), FVector(-8600, -9650, 170), FVector(-9000, -10420, 160), 66.f },
    { TEXT("detail_3c_map"), FVector(2780, -6600, 165), FVector(2780, -6880, 160), 60.f },
    { TEXT("detail_3c_promenade"), FVector(-2500, -6600, 170), FVector(2000, -6600, 430), 70.f },
    // Phase 3d: light pools, haze, the flag flying; the bus bay, where the outdoor sound is recorded (-CampusPolishRecord=).
    { TEXT("detail_3d_green"), FVector(900, -6750, 320), FVector(3200, -8600, 0), 72.f },
    { TEXT("detail_3d_lamps"), FVector(-6500, -6720, 260), FVector(-2500, -6620, 0), 70.f },
    { TEXT("detail_3d_flag"), FVector(400, -6900, 1500), FVector(-600, -7700, 1580), 50.f },
    { TEXT("detail_3d_floods"), FVector(5200, -8000, 1800), FVector(2400, -12000, 0), 72.f },
    { TEXT("detail_3d_sound"), FVector(3800, -17560, 170), FVector(4520, -18025, 160), 66.f },
    // Phase 4: frame times inside each Tier A building (eye height, its signature room; the building streams in round the
    // camera as it would round a player). Named detail_ so the planting's camera reservations never see them.
    { TEXT("detail_perf_in_lib"), FVector(-16300, -10800, 115), FVector(-17900, -10700, 100), 85.f },
    { TEXT("detail_perf_in_harlow"), FVector(-8050, -7420, 115), FVector(-7300, -7250, 100), 85.f },
    { TEXT("detail_perf_in_science"), FVector(-12100, -16500, 330), FVector(-12900, -15700, 150), 85.f },
    { TEXT("detail_perf_in_union"), FVector(-7300, -12000, 115), FVector(-7350, -12900, 120), 85.f },
    { TEXT("detail_perf_in_stadium"), FVector(2300, -9720, 115), FVector(3600, -9780, 100), 85.f },
    // The house-side items (approved after Phase 4): the court wall's foot from both banks, the junction and the crossing
    // from the house's side of the street.
    { TEXT("detail_court_west_slit"), FVector(-1545, 3600, -207), FVector(-1500, 1600, -330), 90.f },
    { TEXT("detail_court_east_slit"), FVector(1545, 2000, -207), FVector(1500, 3800, -330), 90.f },
    { TEXT("detail_court_gate_hole"), FVector(1250, 1772, -190), FVector(1488, 1764, -359), 45.f },
    { TEXT("detail_house_junction"), FVector(7400, -4000, 170), FVector(9300, -3900, 300), 70.f },
    { TEXT("detail_house_junction_high"), FVector(9500, -2600, 1100), FVector(9500, -4700, -60), 70.f },
    { TEXT("detail_house_crossing"), FVector(2600, -3700, 170), FVector(2720, -5700, 260), 70.f },
    // The house kerb's WALK / DON'T WALK pole: its head read from the campus kerb's landing, then the whole pole (walk to
    // cap), the head and the push button from the crosswalk in the eastbound lane, the near curb ramp at the right.
    { TEXT("detail_house_pedhead"), FVector(2420, -5700, 150), FVector(2720, -4100, 155), 16.f },
    { TEXT("detail_house_pedpole"), FVector(2380, -4800, 110), FVector(2700, -4100, 110), 60.f },
    // The street-name blades on the junction's house corner: CALDER DR face-on from an eastbound driver on Fraternity Park
    // Dr, then the two back faces (CALDER DR, FRATERNITY PARK DR) from the side street north of the junction.
    { TEXT("detail_house_junction_blade"), FVector(6300, -4520, 45), FVector(8080, -3810, 320), 35.f },
    { TEXT("detail_house_junction_blade_north"), FVector(9600, -2950, 100), FVector(8080, -3810, 310), 40.f },
    // The junction's stop bar in Fraternity Park Dr's eastbound lane: from above the centre line (the house corner's STOP
    // sign at the right), and at a driver's eye in that lane.
    { TEXT("detail_house_stopbar"), FVector(6600, -4850, 800), FVector(8350, -4850, -72), 80.f },
    { TEXT("detail_house_stopbar_eye"), FVector(7300, -4650, 60), FVector(8400, -4300, 0), 80.f },
    // The town of Calder over the south hedgerow, at eye level from the campus's south edge and from high above the stadium.
    { TEXT("detail_town_view"), FVector(0, -27600, 170), FVector(0, -60000, 2600), 70.f },
    { TEXT("detail_town_high"), FVector(2400, -7600, 2600), FVector(0, -60000, 1500), 60.f },
    // Phase 4: each walk-in building from straight above (its plan block and 26 m round it), before and after, for its folder.
    { TEXT("detail_plan_bld_lib"), FVector(-17300, -10800, 30000), FVector(-17300, -10801, -60), 0.f, 10200.f },
    { TEXT("detail_plan_bld_har"), FVector(-8700, -8275, 30000), FVector(-8700, -8276, -60), 0.f, 7550.f },
    { TEXT("detail_plan_bld_sci"), FVector(-12900, -15800, 30000), FVector(-12900, -15801, -60), 0.f, 8400.f },
    { TEXT("detail_plan_bld_uni"), FVector(-6250, -12650, 30000), FVector(-6250, -12651, -60), 0.f, 9100.f },
    { TEXT("detail_plan_bld_sta"), FVector(2450, -13150, 30000), FVector(2450, -13151, -60), 0.f, 13900.f },
    { TEXT("detail_plan_bld_main"), FVector(-13300, -8400, 30000), FVector(-13300, -8401, -60), 0.f, 7800.f },
    { TEXT("detail_plan_bld_com"), FVector(12600, -8550, 30000), FVector(12600, -8551, -60), 0.f, 8100.f },
    { TEXT("detail_plan_bld_ash"), FVector(16800, -8750, 30000), FVector(16800, -8751, -60), 0.f, 8500.f },
    { TEXT("detail_plan_bld_hum"), FVector(-17400, -16500, 30000), FVector(-17400, -16501, -60), 0.f, 9400.f },
    { TEXT("detail_plan_bld_ker"), FVector(12700, -13400, 30000), FVector(12700, -13401, -60), 0.f, 9200.f },
    { TEXT("detail_plan_bld_art"), FVector(-11150, -19900, 30000), FVector(-11150, -19901, -60), 0.f, 8200.f },
    { TEXT("detail_plan_bld_hlth"), FVector(-3000, -7900, 30000), FVector(-3000, -7901, -60), 0.f, 6800.f },
    { TEXT("detail_plan_bld_fld"), FVector(16950, -14600, 30000), FVector(16950, -14601, -60), 0.f, 10000.f },
    { TEXT("detail_plan_bld_mus"), FVector(-5950, -18850, 30000), FVector(-5950, -18851, -60), 0.f, 8000.f },
    { TEXT("detail_plan_bld_eng"), FVector(-8200, -23350, 30000), FVector(-8200, -23351, -60), 0.f, 8000.f },
    { TEXT("detail_plan_bld_plt"), FVector(13850, -19750, 30000), FVector(13850, -19751, -60), 0.f, 8200.f },
    { TEXT("detail_plan_bld_alu"), FVector(-17300, -23150, 30000), FVector(-17300, -23151, -60), 0.f, 7900.f },
    // Calder in the mountains: the eye-level proof spots (SourceArt/CalderLandscape; eyes 170 cm over today's grade, lifted
    // onto the relief at capture). P6 and P7 stand on the footbridge below the falls and on the dam (added with the water).
    { TEXT("detail_calder_eye_P1"), FVector(-12000, -6450, 110), FVector(-11000, -6450, 110) },      // shot 1 retaken, facing +X
    { TEXT("detail_calder_eye_P2"), FVector(-4000, -17800, 110), FVector(16000, -17800, 110) },     // shot 2 retaken, facing +X (target on the Physical Plant's bench, 200 m out: a level look)
    { TEXT("detail_calder_eye_P3"), FVector(2380, -3960, 110), FVector(2380, -4960, 110) },          // frat lawn, facing -Y
    // (P4 stands on Q3 13 m east of the Senior Steps' foot, its target far up the axis at Mercer's door, so every flight up to
    // the forecourt is in frame; P5 is absolute (its eye 31.7 m over today's grade, on the overlook: never offset), aimed at
    // the town under Gosling Knob 600 m out at yaw 205, 2 degrees up.)
    { TEXT("detail_calder_eye_P4"), FVector(-13500, -10800, 110), FVector(-17000, -10800, 110) },    // the Senior Steps from Q3, facing -X
    { TEXT("detail_calder_eye_P5"), FVector(-11000, -29800, 3110), FVector(-65378, -55157, 5200) },  // Founders' Overlook, facing yaw 205
    { TEXT("detail_calder_eye_P8"), FVector(1500, 26550, 1708), FVector(1500, -20000, 4963) },       // Crow Ledge, facing -Y (absolute: 170 cm over the ledge, 4 deg up)
    // The two deck spots, absolute (landform.py P6_EYE_ABS / P7_EYE_ABS: 170 cm over the timber footbridge below Calder Falls
    // and over the dam's arch bridge): never offset by the relief under them (the water and the glen bed).
    { TEXT("detail_calder_eye_P6"), FVector(25420, -20652.5f, 31.7f), FVector(25868.75f, -19600, 60), 95.f },  // the falls footbridge 9 m west of P6 (s -900 on its axis), looking upstream: glen walls, falls and pool together
    { TEXT("detail_calder_eye_P7"), FVector(25200, -16420, 890), FVector(25200, -11420, 790) },        // the dam's footbridge, facing +Y over Gander Pond
    { TEXT("detail_calder_access_southlot_A_B"), FVector(1800, -21100, 110), FVector(1800, -22150, 110) },
    { TEXT("detail_calder_access_southlot_B_C"), FVector(1800, -22650, 110), FVector(1800, -23950, 110) },
    // Supplemental ensemble from the same P3 standing spot. Keep the original
    // due-south P3 frame above for every baseline comparison.
    { TEXT("detail_calder_eye_P3_tower"), FVector(2380, -3960, 110), FVector(-3000, -13300, 1500) },
    { TEXT("detail_calder_stair_walk00"), FVector(-19880, -6700, 110), FVector(-19100, -6500, 30) },
    { TEXT("detail_calder_stair_walk29"), FVector(-8200, -18550, 110), FVector(-7815, -18850, 30) },
    { TEXT("detail_calder_stair_walk47"), FVector(9000, -19900, 110), FVector(10000, -19690, 30) },
    { TEXT("detail_calder_road_bridge"), FVector(25000, -8400, 940), FVector(25000, -7000, 1020) },
    { TEXT("detail_calder_road_approach"), FVector(23700, -7000, 110), FVector(25900, -7000, 1050) },
    // Ground macros: actual field placements from the native cover survey,
    // a 1.3m standing eye. Existing comparison views stay in the same slots.
    { TEXT("detail_calder_macro_mown_bank"), FVector(-4125.173f,-17966.273f,70), FVector(-3905.173f,-17816.273f,-40),70.f },
    { TEXT("detail_calder_macro_broomsedge"), FVector(-20317.501f,23756.032f,70), FVector(-20097.501f,23906.032f,-10),70.f },
    { TEXT("detail_calder_macro_aster"), FVector(-19900.410f,23301.968f,70), FVector(-19680.410f,23451.968f,-10),70.f },
    { TEXT("detail_calder_macro_fluff"), FVector(-20681.274f,23958.143f,70), FVector(-20461.274f,24108.143f,-10),70.f },
    // Actual planted instances are selected once at capture; no props move.
    { TEXT("detail_calder_macro_fern"), FVector(-3020,6320,130), FVector(-3200,6500,40),65.f },
    { TEXT("detail_calder_macro_stump"), FVector(-3020,6320,130), FVector(-3200,6500,40),65.f },
    { TEXT("detail_calder_farm"), FVector(-25200,-3250,130), FVector(-27200,-2200,420),75.f },
    { TEXT("detail_calder_bale"), FVector(-24350,-50,130), FVector(-24600,200,75),65.f },
    { TEXT("detail_calder_barn_entry"), FVector(-27200,-3750,130), FVector(-27200,-2200,350),75.f },
    { TEXT("detail_calder_barn_roof"), FVector(-29500,4000,130), FVector(-27200,-2200,800),65.f },
    { TEXT("detail_calder_rock_beds"), FVector(10200,28000,130), FVector(11300,28000,100),60.f },
    { TEXT("detail_calder_wall_stone"), FVector(-19000,-6850,130), FVector(-17900,-6850,130),60.f },
    { TEXT("detail_calder_persimmon"), FVector(-23350,-2100,130), FVector(-22700,-1550,300),95.f },
    { TEXT("detail_calder_persimmon_fruit"), FVector(-22600,-1800,130), FVector(-22600,-1600,240),35.f },
    { TEXT("detail_calder_hedgerow"), FVector(-22700,-3000,130), FVector(-22200,-2850,100),75.f },
    { TEXT("detail_calder_rose_hip"), FVector(-22200,-2900,130), FVector(-22200,-2850,130),35.f },
    { TEXT("detail_calder_cedar_scale"), FVector(-22200,-2900,130), FVector(-22200,-2850,130),35.f },
    { TEXT("detail_row_road_wide"), FVector(-14300,-4800,140), FVector(-14100,-1400,350), 78.f },
    { TEXT("detail_row_R1_porch"), FVector(-17400,-3650,120), FVector(-17400,-2400,260), 72.f },
    { TEXT("detail_row_R2_porch"), FVector(-13900,-3650,120), FVector(-13900,-2400,260), 72.f },
    { TEXT("detail_row_R3_porch"), FVector(-10100,-3650,120), FVector(-10100,-2400,260), 72.f },
    { TEXT("detail_row_R1_yard"), FVector(-17500,3400,20), FVector(-17400,800,140), 74.f },
    { TEXT("detail_row_R2_yard"), FVector(-13900,3400,20), FVector(-13900,800,140), 74.f },
    { TEXT("detail_row_R3_yard"), FVector(-10400,3400,20), FVector(-10100,800,140), 74.f },
    { TEXT("detail_row_infill"), FVector(-7700,-500,80), FVector(-6000,1150,260), 78.f },
    { TEXT("detail_row_maple_gap"), FVector(-6450,2200,20), FVector(-6215,1596,160), 70.f },
    { TEXT("detail_row_oak_edge"), FVector(-4100,2300,20), FVector(-5300,1200,260), 70.f },
    { TEXT("detail_row_rear_fence"), FVector(-12000,4200,20), FVector(-12000,3000,130), 78.f },
    { TEXT("detail_row_gate"), FVector(-7050,4100,20), FVector(-7050,5000,20), 72.f },
    { TEXT("detail_row_former_parking"), FVector(-7300,-2000,140), FVector(-5200,-1000,120), 76.f },
    { TEXT("detail_row_house_connection"), FVector(-4600,-195,130), FVector(-3480,-195,100), 70.f },

    // Stadium detail review cameras only; knee-height exception is explicitly named.
    { TEXT("detail_stadium_aisle_up"), FVector(5958.87,-12299.70,350.00), FVector(7351.47,-11908.27,860.00), 75.f },
    { TEXT("detail_stadium_seat_knee"), FVector(6439.66,-11842.48,550.00), FVector(6381.64,-11740.10,539.00), 68.f },
    { TEXT("detail_stadium_front_rail"), FVector(304.91,-15575.14,350.00), FVector(560.12,-15298.00,0.00), 76.f },
    { TEXT("detail_stadium_press_50yard"), FVector(2400.00,-13300.00,114.00), FVector(2400.00,-9900.00,1650.00), 63.f },
    { TEXT("detail_stadium_score_endzone"), FVector(5200.00,-14300.00,114.00), FVector(2400.00,-16966.00,1750.00), 58.f },
    { TEXT("detail_stadium_bench_chains"), FVector(2750.00,-11275.00,114.00), FVector(2850.00,-11490.00,-10.00), 70.f },
    { TEXT("detail_stadium_portal"), FVector(245.00,-10110.00,115.00), FVector(1232.00,-9951.00,455.00), 73.f },
    { TEXT("detail_stadium_concession"), FVector(4236.28,-9927.58,115.00), FVector(4175.94,-10142.26,175.00), 72.f },
    { TEXT("detail_stadium_turnstiles"), FVector(3838.02,-9235.49,113.00), FVector(3700.76,-9623.47,130.00), 70.f },
    { TEXT("detail_stadium_locker"), FVector(4342.79,-10560.73,115.00), FVector(5096.13,-10872.15,25.00), 80.f },
    { TEXT("detail_stadium_tunnel"), FVector(4594.49,-10869.98,115.00), FVector(4301.89,-11193.98,50.00), 75.f },

    { TEXT("detail_row_R5_lot_front"), FVector(-6380,-3850,160), FVector(-6380,-1000,570), 74.f },
    { TEXT("detail_row_R5_lot_rear"), FVector(-5100,1200,150), FVector(-6380,-800,570), 78.f },

};

// ------------------------------------------------------------------ materials
void ExPaintBrick(FHallImage& Img, TArray<float>& Height, const FLinearColor& Base, const FLinearColor& Mortar, int32 Seed)
{
    const int32 W = 256, H = 256;             // one tile = 120 x 120 cm: 16 courses of 7.5, bricks 40 long
    Img.Init(W, H, Mortar); Height.Init(0.f, W * H);
    const float CourseH = H / 16.f, BrickW = W / 3.f;
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const int32 Course = (int32)(Y / CourseH);
            const float Fy = Y - Course * CourseH;
            const float Shift = (Course & 1) ? BrickW * .5f : 0.f;
            const float Xs = FMath::Fmod(X + Shift, BrickW);
            const int32 BrickIdx = (int32)((X + Shift) / BrickW);
            if (Fy < 2.f || Xs < 2.f)
            {
                Img.At(X, Y) = Mortar * (.85f + .3f * HallNoise(X * .37f, Y * .37f, Seed + 3));
                continue;
            }
            const float V = HallNoise(BrickIdx * 7.31f + 1.13f, Course * 3.71f + .4f, Seed);
            const float G = HallFbm(X * .11f, Y * .11f, 3, Seed + 1);
            const float Edge = FMath::Min(FMath::Min(Fy - 2.f, CourseH - Fy), FMath::Min(Xs - 2.f, BrickW - Xs));
            FLinearColor C = Base * (.72f + .56f * V) * (.82f + .36f * G);
            if (V > .82f) C = C * .75f;                    // a few burnt headers
            Img.At(X, Y) = C;
            Height[Y * W + X] = FMath::Clamp(Edge / 2.f, 0.f, 1.f) * (.85f + .15f * G);
        }
}

void ExPaintShingle(FHallImage& Img, TArray<float>& Height, int32 Seed, float Base = .17f, float Range = .12f, float Grain = .08f, float Blue = 1.05f)
{
    const int32 W = 256, H = 256;             // tile = 100 x 100 cm: 3 courses, tabs 33 wide
    Img.Init(W, H, FLinearColor(Base, Base, Base * Blue)); Height.Init(1.f, W * H);
    const float CourseH = H / 3.f, TabW = W / 3.f;
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const int32 Course = (int32)(Y / CourseH);
            const float Fy = Y - Course * CourseH;
            const float Shift = (Course & 1) ? TabW * .5f : 0.f;
            const float Xs = FMath::Fmod(X + Shift, TabW);
            const int32 Tab = (int32)((X + Shift) / TabW) + Course * 7;
            const float V = HallNoise(Tab * 5.17f + .3f, Course * 2.9f, Seed);
            const float G = HallFbm(X * .25f, Y * .25f, 2, Seed + 2);
            float Lum = Base + Range * V + Grain * G;
            if (Fy < 4.f) Lum *= .45f;                     // shadow under the course above
            if (Xs < 2.f && Fy > CourseH * .45f) Lum *= .6f;   // tab slot
            Img.At(X, Y) = FLinearColor(Lum, Lum, Lum * Blue);
            Height[Y * W + X] = (Fy < 4.f ? 0.f : 1.f) * ((Xs < 2.f && Fy > CourseH * .45f) ? .3f : 1.f);
        }
}

void ExPaintGrass(FHallImage& Img, int32 Seed)
{
    const int32 W = 256, H = 256;
    Img.Init(W, H, FLinearColor::Black);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float A = HallFbm(X * .05f, Y * .05f, 4, Seed), B = HallNoise(X * .9f, Y * .9f, Seed + 7);
            const float Patch = HallFbm(X * .015f, Y * .015f, 2, Seed + 9);
            FLinearColor C = FLinearColor::LerpUsingHSV(FLinearColor(.07f, .16f, .04f), FLinearColor(.15f, .17f, .05f), Patch);
            C = C * (.7f + .5f * A) * (.8f + .4f * B);
            Img.At(X, Y) = C;
        }
}

void ExPaintAsphalt(FHallImage& Img, int32 Seed)
{
    const int32 W = 256, H = 256;
    Img.Init(W, H, FLinearColor::Black);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float A = HallFbm(X * .07f, Y * .07f, 3, Seed), B = HallNoise(X * 1.3f, Y * 1.3f, Seed + 5);
            const float L = .055f + .035f * A + (B > .93f ? .06f : 0.f);
            Img.At(X, Y) = FLinearColor(L, L, L * 1.03f);
        }
}

void ExPaintConcrete(FHallImage& Img, int32 Seed)
{
    const int32 W = 256, H = 256;             // tile = 240 cm with a joint every 120
    Img.Init(W, H, FLinearColor::Black);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float A = HallFbm(X * .06f, Y * .06f, 3, Seed), B = HallNoise(X * .8f, Y * .8f, Seed + 4);
            float L = .42f + .12f * A + .05f * B;
            const int32 JX = X % 128, JY = Y % 128;
            if (JX < 2 || JY < 2) L *= .55f;
            Img.At(X, Y) = FLinearColor(L, L * .99f, L * .95f);
        }
}

void ExPaintFrieze(FHallImage& Img)
{
    const int32 W = 1024, H = 128;
    Img.Init(W, H, FLinearColor(.84f, .84f, .81f));
    // Keep the architectural fascia, without fraternity lettering.
}

void ExCreateLook(UHouseBlackHallSubsystem* Sub)
{
    if (!Sub || Sub->Materials.Contains(TEXT("EX_Brick"))) return;
    auto Plain = [&](const TCHAR* Key, const FLinearColor& C, float Rough, float Metal = 0.f, const FLinearColor& Em = FLinearColor::Black) { HallMakePlain(Sub, FName(Key), C, Rough, Metal, Em); };
    auto Tex = [&](const TCHAR* Key, const FHallImage& Img, const TArray<float>* Height, float Rough, float NormalStrength, const FLinearColor& Tint = FLinearColor::White)
    {
        UTexture2D* T = HallMakeTexture(Img, true, false, *FString::Printf(TEXT("T_%s"), Key), true);
        if (T) Sub->Textures.Add(FName(*FString::Printf(TEXT("T_%s"), Key)), T);
        UTexture2D* N = nullptr;
        if (Height)
        {
            FHallImage NImg; HallHeightToNormal(*Height, Img.W, Img.H, NormalStrength, true, NImg);
            N = HallMakeTexture(NImg, false, true, *FString::Printf(TEXT("T_%sN"), Key), true);
            if (N) Sub->Textures.Add(FName(*FString::Printf(TEXT("T_%sN"), Key)), N);
        }
        HallMakeTextured(Sub, FName(Key), T, N, Rough, Tint, N ? 1.f : 0.f);
        return T;
    };
    FHallImage Img; TArray<float> Height;
    ExPaintBrick(Img, Height, FLinearColor(.40f, .19f, .13f), FLinearColor(.58f, .55f, .50f), 11);
    UTexture2D* BrickT = Tex(TEXT("EX_Brick"), Img, &Height, .86f, 2.2f);
    // Chimneys: same bond, redder.
    if (BrickT)
    {
        UTexture2D* BrickN = Sub->Textures.FindRef(TEXT("T_EX_BrickN"));
        HallMakeTextured(Sub, TEXT("EX_BrickRed"), BrickT, BrickN, .88f, FLinearColor(1.25f, .78f, .72f), 1.f);
    }
    ExPaintShingle(Img, Height, 23); Tex(TEXT("EX_Shingle"), Img, &Height, .92f, 1.6f);
    // The house's own roofs: black architectural shingle, as in Jacob's photos (the neighbours keep the grey tile).
    ExPaintShingle(Img, Height, 24, .028f, .034f, .024f, 1.f); Tex(TEXT("EX_ShingleBlack"), Img, &Height, .9f, 1.6f);
    ExPaintGrass(Img, 31); Tex(TEXT("EX_Grass"), Img, nullptr, .95f, 0.f);
    ExPaintAsphalt(Img, 41); Tex(TEXT("EX_Asphalt"), Img, nullptr, .9f, 0.f);
    ExPaintConcrete(Img, 51); Tex(TEXT("EX_Concrete"), Img, nullptr, .8f, 0.f);
    {
        FHallImage F; ExPaintFrieze(F);
        UTexture2D* T = HallMakeTexture(F, true, false, TEXT("T_EX_Frieze"), false);
        if (T) { Sub->Textures.Add(TEXT("T_EX_Frieze"), T); HallMakeTextured(Sub, TEXT("EX_Frieze"), T, nullptr, .5f, FLinearColor::White, 0.f); }
    }
    Plain(TEXT("EX_Trim"), FLinearColor(.84f, .84f, .80f), .5f);
    Plain(TEXT("EX_Stone"), FLinearColor(.60f, .58f, .52f), .7f);
    Plain(TEXT("EX_Glass"), FLinearColor(.015f, .02f, .035f), .12f, .35f);
    Plain(TEXT("EX_Wood"), FLinearColor(.10f, .065f, .04f), .75f);
    Plain(TEXT("EX_Dark"), FLinearColor(.05f, .045f, .04f), .9f);
    Plain(TEXT("EX_Leaf"), FLinearColor(.045f, .11f, .035f), .95f);
    Plain(TEXT("EX_LeafDark"), FLinearColor(.03f, .07f, .025f), .95f);
    Plain(TEXT("EX_Bark"), FLinearColor(.11f, .085f, .06f), .9f);
    Plain(TEXT("EX_Hedge"), FLinearColor(.035f, .09f, .03f), .95f);
    Plain(TEXT("EX_Metal"), FLinearColor(.16f, .16f, .17f), .45f, .8f);
    Plain(TEXT("EX_Black"), FLinearColor(.012f, .012f, .012f), .8f);
    Plain(TEXT("EX_Beige"), FLinearColor(.52f, .46f, .36f), .85f);
    Plain(TEXT("EX_Wall"), FLinearColor(.44f, .44f, .42f), .85f);
    Plain(TEXT("EX_Line"), FLinearColor(.85f, .85f, .82f), .7f);
    Plain(TEXT("EX_Yellow"), FLinearColor(.75f, .6f, .08f), .7f);
    Plain(TEXT("EX_Orange"), FLinearColor(.8f, .25f, .04f), .5f, .3f);
    Plain(TEXT("EX_Field"), FLinearColor(.05f, .13f, .04f), .95f);
    Plain(TEXT("EX_Green"), FLinearColor(.06f, .22f, .09f), .55f, .2f);
    Plain(TEXT("EX_Blue"), FLinearColor(.08f, .15f, .4f), .4f, .3f);
    Plain(TEXT("EX_Red"), FLinearColor(.45f, .04f, .03f), .35f, .4f);
    Plain(TEXT("EX_White"), FLinearColor(.78f, .78f, .76f), .35f, .3f);
    Plain(TEXT("EX_Grey"), FLinearColor(.22f, .22f, .23f), .35f, .5f);
    Plain(TEXT("EX_CarGlass"), FLinearColor(.02f, .025f, .035f), .1f, .4f);
    Plain(TEXT("EX_Rubber"), FLinearColor(.02f, .02f, .02f), .9f);
    Plain(TEXT("EX_Lamp"), FLinearColor(1.f, .85f, .6f), .4f, 0.f, FLinearColor(14.f, 10.f, 5.f));
    Plain(TEXT("EX_Flood"), FLinearColor(.9f, .95f, 1.f), .4f, 0.f, FLinearColor(30.f, 32.f, 40.f));
    Plain(TEXT("EX_Window"), FLinearColor(.9f, .8f, .6f), .5f, 0.f, FLinearColor(2.2f, 1.6f, .9f));
    Plain(TEXT("EX_Neon"), FLinearColor(.9f, .2f, .3f), .5f, 0.f, FLinearColor(6.f, .8f, 1.5f));
}

// ------------------------------------------------------------------ geometry
struct FExWin { float U0, U1, Z0, Z1; int32 Kind; };   // 0 scanned window (opening + trim), 1 blind window (dark glass), 2 door, 3 plain hole
struct FExWall
{
    int32 Axis;        // 0: plane x = Face, runs along y;  1: plane y = Face, runs along x
    float Sign;        // outward normal sign
    float Face;        // exterior face coordinate
    float Thick;       // inward thickness
    float U0, U1, Z0, Z1;
    float MuntinD;     // depth of the sash bars for scanned windows (0 = Thick + 22)
    TArray<FExWin> Wins;
};

struct FExBuilder
{
    UWorld* World = nullptr;
    UHouseBlackHallSubsystem* Look = nullptr;
    AHouseExteriorDetail* Owner = nullptr;
    FHallMesh House, Grounds, Near, Far, CourtDecor, Walks, WalkDecor, BrickDecor, WoodsHut, FraternityRow, StreetDecor, FieldFence, FieldFenceDetail, ParkingSolid, ParkingDetail;
    FHallMesh CampusGround;   // the campus plan's roads, lots, walks and plazas (HouseCampusLayout), signs and street furniture
    FHallMesh CampusFlat;     // the campus plan's paint, grates and manhole covers (no collision, no shadows)
    FHallMesh CampusFallFloor; // the hidden collision floor under homecoming's drawn gate path and kiosk plaza
    FHallMesh Attic;   // roof undersides facing into the attic (no shadows, no collision)
    TArray<FBox2D> CalderRegions;   // the ground the Calder terrain tiles build instead of ExPatch (outside the frozen zones)
    FRandomStream Rng = FRandomStream(20261009);
    struct FLightSpec { FVector Pos; FLinearColor Color; float Lumens, Radius, Draw; bool bSpotDown = false; };
    TArray<FLightSpec> LightSpecs;
    struct FNoGo { float X0, X1, Y0, Y1; };
    TArray<FNoGo> NoGo;
    TArray<FBox> BlockBoxes;
    struct FOpening { int32 Axis; FBox Bounds; bool Door; };
    TArray<FOpening> Openings;

    void MeasureConnections()
    {
        // The two front entrances have local-space leaves; measure their fitted
        // architectural apertures, rather than interpreting the local leaf center
        // as a world coordinate or cutting an upstairs door down to ground level.
        Openings.Add({1,FBox(FVector(-186,-993,-2),FVector(113,-985,274)),true});
        Openings.Add({1,FBox(FVector(-116,-929,332),FVector(79,-920,571)),true});
        TMap<FName,FBox> PorchPairs;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* A=*It;
            if (auto* G=Cast<AHouseReactiveFixture>(A))
            {
                if (G->FixtureKind!=EHouseReactiveFixtureKind::Window || G->GetOwner()) continue;
                const FBox Box=G->GetFixtureMesh()->Bounds.GetBox();
                Openings.Add({Box.GetSize().X<Box.GetSize().Y?0:1,Box,false});
                continue;
            }
            if (auto* D=Cast<AExistingHouseDoor>(A))
            {
                if (!D->DoorId.ToString().StartsWith(TEXT("Porch_"))) continue;
                const FVector H=D->PanelHalfExtent;
                if (H.IsNearlyZero()) continue;
                FBox Box(D->PanelCenterWorld-H,D->PanelCenterWorld+H);
                Box.Min.Z=-2.f; Box.Max.Z=FMath::Max(Box.Max.Z,274.f);
                // Include the jamb, meeting stile, and transom in one opening.
                Box.Min.X-=25.f; Box.Max.X+=25.f;
                // One complete architectural opening per double-door pair.
                // Per-leaf reveals left stationary strips at the meeting seam.
                const FName PairId=D->DoorGroup.IsNone()?D->DoorId:D->DoorGroup;
                if(FBox* Pair=PorchPairs.Find(PairId))*Pair+=Box;
                else PorchPairs.Add(PairId,Box);
                continue;
            }

        }
        for(const auto& Pair:PorchPairs)Openings.Add({1,Pair.Value,true});
    }

    void ClearOpeningSurfaces()
    {
        // Cut every generated layer at the real pane, including corner returns
        // and trim belonging to a neighboring facade. The pane fixture alone
        // provides the temporary blocking surface until it shatters.
        struct FV {FVector P,N;FVector2D UV;FProcMeshTangent T;};
        auto Clip=[](const TArray<FV>& In,int Axis,float Value,bool Greater)
        {
            TArray<FV> Out;
            for (int I=0;I<In.Num();++I)
            {
                const FV& A=In[I];const FV& B=In[(I+1)%In.Num()];
                const float DA=(A.P[Axis]-Value)*(Greater?1:-1),DB=(B.P[Axis]-Value)*(Greater?1:-1);
                if(DA>=-KINDA_SMALL_NUMBER)Out.Add(A);
                if((DA>KINDA_SMALL_NUMBER && DB< -KINDA_SMALL_NUMBER)||(DA< -KINDA_SMALL_NUMBER && DB>KINDA_SMALL_NUMBER))
                {
                    const float Alpha=DA/(DA-DB);FV V=A;V.P=FMath::Lerp(A.P,B.P,Alpha);
                    V.N=FMath::Lerp(A.N,B.N,Alpha).GetSafeNormal();V.UV=FMath::Lerp(A.UV,B.UV,Alpha);Out.Add(V);
                }
            }
            return Out;
        };
        for (auto& Pair:House.Sections)
        {
            FHallSection& S=Pair.Value;FHallSection Result;
            for(int I=0;I<S.I.Num();I+=3)
            {
                TArray<TArray<FV>> Parts;TArray<FV> Triangle;
                for(int K=0;K<3;++K){const int V=S.I[I+K];Triangle.Add({S.V[V],S.N[V],S.UV[V],S.T[V]});}
                Parts.Add(Triangle);
                for (const FOpening& O:Openings)
                {
                    if(O.Door)continue;
                    FBox Cut=O.Bounds;Cut.Min[O.Axis]-=180.f;Cut.Max[O.Axis]+=180.f;
                    Cut.Min[1-O.Axis]-=.1f;Cut.Max[1-O.Axis]+=.1f;Cut.Min.Z-=.1f;Cut.Max.Z+=.1f;
                    // Floor-height windows retain the floor, but no strip of
                    // brick beneath the pane that catches a walking capsule.
                    if(O.Bounds.Min.Z>=0.f && O.Bounds.Min.Z<30.f)Cut.Min.Z=.05f;
                    TArray<TArray<FV>> Next;
                    for (const TArray<FV>& Part:Parts)
                    {
                        FBox Bounds(ForceInit);for(const FV& V:Part)Bounds+=V.P;
                        if(!Bounds.Intersect(Cut)){Next.Add(Part);continue;}
                        TArray<FV> Inside=Part;
                        for (int Axis=0;Axis<3 && Inside.Num()>=3;++Axis)
                            for(int Side=0;Side<2 && Inside.Num()>=3;++Side)
                            {
                                const float Value=Side?Cut.Max[Axis]:Cut.Min[Axis];const bool Greater=Side==0;
                                auto Outside=Clip(Inside,Axis,Value,!Greater);
                                if(Outside.Num()>=3)Next.Add(MoveTemp(Outside));
                                Inside=Clip(Inside,Axis,Value,Greater);
                            }
                    }
                    Parts=MoveTemp(Next);
                }
                for(const auto& Part:Parts)for(int K=1;K+1<Part.Num();++K)
                {
                    const int Base=Result.V.Num();
                    for(const FV& V:{Part[0],Part[K],Part[K+1]}){Result.V.Add(V.P);Result.N.Add(V.N);Result.UV.Add(V.UV);Result.T.Add(V.T);}
                    Result.I.Append({Base,Base+1,Base+2});
                }
            }
            S=MoveTemp(Result);
        }
    }

    void BuildConnectedWall(const FExWall& Original)
    {
        // Keep the authored exterior outline. Interior walls and empty rooms
        // must never determine the house footprint by a first-hit raycast.
        FExWall W=Original;W.Thick=8.f;
        W.Wins.RemoveAll([](const FExWin& X){return X.Kind==0 || X.Kind==1;});
        W.Wins.RemoveAll([&](const FExWin& X)
        {
            return X.Kind==3 && Openings.ContainsByPredicate([&](const FOpening& O)
            {return O.Door && O.Axis==W.Axis && FMath::Abs(O.Bounds.GetCenter()[W.Axis]-W.Face)<160
                && FMath::Abs(O.Bounds.GetCenter()[1-W.Axis]-(X.U0+X.U1)*.5f)<3
                && O.Bounds.Max.Z>W.Z0 && O.Bounds.Min.Z<W.Z1;});
        });
        struct FPortal { FExWin Inner, Outer; float Depth; };
        TArray<FPortal> Portals;
        for (const FOpening& O:Openings)
        {
            if(O.Axis!=W.Axis)continue;
            const int32 Along=1-W.Axis;
            if(O.Bounds.Max[Along]<=W.U0 || O.Bounds.Min[Along]>=W.U1
                || O.Bounds.Max.Z<=W.Z0 || O.Bounds.Min.Z>=W.Z1
                || FMath::Abs(O.Bounds.GetCenter()[W.Axis]-W.Face)>160.f)continue;
            FExWin Inner{float(O.Bounds.Min[Along]-.5f),float(O.Bounds.Max[Along]+.5f),
                float(O.Bounds.Min.Z-.5f),float(O.Bounds.Max.Z+.5f),O.Door?3:0};
            const float Depth=FMath::Max(8.f,float((W.Face-O.Bounds.GetCenter()[W.Axis])*W.Sign));
            FExWin Outer=Inner;
            // A modest splay exposes brick at the outer recess; only the last
            // 12 cm at the sash is painted white, instead of a thick white tunnel.
            if(Depth>24.f)
            {
                const float Flare=FMath::Min(12.f,(Inner.U1-Inner.U0)*.08f);
                Outer.U0-=Flare;Outer.U1+=Flare;Outer.Z1+=8.f;
                if(Inner.Z0>30.f)Outer.Z0-=6.f;
            }
            W.Wins.Add(Outer);Portals.Add({Inner,Outer,Depth});
        }
        BuildWall(W,false);
        for(const FExWin& X:W.Wins)
        {
            const FPortal* P=Portals.FindByPredicate([&](const FPortal& V){return FMath::IsNearlyEqual(V.Outer.U0,X.U0,.01f)&&FMath::IsNearlyEqual(V.Outer.Z0,X.Z0,.01f);});
            if(P)
            {
                const auto& I=P->Inner;const float Throat=FMath::Max(0.f,P->Depth-12.f);
                auto Lining=[&](float A,float B,float Z,float ZZ,bool Top)
                {
                    const FVector P0=WP(W,A,-.2f,Z),P1=WP(W,B,Throat,ZZ);
                    FVector Q0=P0,Q1=P1;
                    if(Top){Q0=WP(W,X.U1,-.2f,Z);Q1=WP(W,I.U1,Throat,ZZ);}
                    else{Q0.Z=X.Z1;Q1.Z=I.Z1;}
                    const FVector Center=(P0+P1+Q1+Q0)*.25f;
                    const FVector TowardOpening=WP(W,(I.U0+I.U1)*.5f,Throat*.5f,(I.Z0+I.Z1)*.5f)-Center;
                    if(FVector::DotProduct(FVector::CrossProduct(P1-P0,Q1-P0),TowardOpening)>=0)
                        Face(House,Brick,P0,P1,Q1,Q0,BrickUV);
                    else Face(House,Brick,Q0,Q1,P1,P0,BrickUV);
                };
                if(Throat>0)
                {
                    Lining(X.U0,I.U0,X.Z0,I.Z0,false);
                    Lining(X.U1,I.U1,X.Z0,I.Z0,false);
                    Lining(X.U0,I.U0,X.Z1,I.Z1,true);
                    Lining(X.U0,I.U0,X.Z0,I.Z0,true);
                }
                WallBox(Trim,W,I.U0-2,I.U0,Throat,P->Depth+2,I.Z0,I.Z1,PlainUV);
                WallBox(Trim,W,I.U1,I.U1+2,Throat,P->Depth+2,I.Z0,I.Z1,PlainUV);
                WallBox(Trim,W,I.U0-2,I.U1+2,Throat,P->Depth+2,I.Z1,I.Z1+2,PlainUV);
                // Continuous floor/sill support spans the cavity at every real
                // opening. It is below the clear aperture, never a glass blocker.
                const bool AtFloor=FMath::Abs(I.Z0)<30.f || I.Kind==3;
                const float FloorZ=AtFloor && FMath::Abs(I.Z0)<3.f?.02f:I.Z0;
                WallBox(AtFloor?Concrete:Stone,W,I.U0,I.U1,-1,P->Depth+8,
                    FloorZ-9.f,FloorZ,PlainUV);
                continue;
            }
            if(X.Kind==3)continue;
            FExWall Frame=W;
            for(const FOpening& O:Openings)if(O.Axis==W.Axis && !O.Door
                && FMath::IsNearlyEqual(O.Bounds.Min[1-W.Axis]-.5f,X.U0,1.f)
                && FMath::IsNearlyEqual(O.Bounds.Min.Z-.5f,X.Z0,1.f))
                Frame.MuntinD=FMath::Max(8.f,(W.Face-O.Bounds.GetCenter()[W.Axis])*W.Sign);
            WindowDetail(Frame,X);
            const float Top=FMath::Min(W.Z1-.3f,X.Z1+22.f);
            if(Top-X.Z1>8.f)
                HouseBrick::Header(BrickDecor,WP(W,FMath::Max(W.U0,X.U0-7.f),-.12f,X.Z1+.15f),
                    WP(W,FMath::Min(W.U1,X.U1+7.f),-.12f,X.Z1+.15f),WN(W),Top-X.Z1);
        }
    }

    const FName Brick = TEXT("EX_Brick"), BrickRed = TEXT("EX_BrickRed"), Trim = TEXT("EX_Trim"), Stone = TEXT("EX_Stone"), Shingle = TEXT("EX_ShingleBlack"),
        Glass = TEXT("EX_Glass"), Grass = TEXT("EX_Grass"), Asphalt = TEXT("EX_Asphalt"), Concrete = TEXT("EX_Concrete"), Wood = TEXT("EX_Wood"), Dark = TEXT("EX_Dark"),
        Leaf = TEXT("EX_Leaf"), LeafDark = TEXT("EX_LeafDark"), Bark = TEXT("EX_Bark"), Hedge = TEXT("EX_Hedge"), Metal = TEXT("EX_Metal"), Black = TEXT("EX_Black"),
        Beige = TEXT("EX_Beige"), Wall = TEXT("EX_Wall"), Line = TEXT("EX_Line"), Yellow = TEXT("EX_Yellow"), Orange = TEXT("EX_Orange"), Field = TEXT("EX_Field"),
        Green = TEXT("EX_Green"), Blue = TEXT("EX_Blue"), Red = TEXT("EX_Red"), White = TEXT("EX_White"), Grey = TEXT("EX_Grey"), CarGlass = TEXT("EX_CarGlass"),
        Rubber = TEXT("EX_Rubber"), Lamp = TEXT("EX_Lamp"), Flood = TEXT("EX_Flood"), Window = TEXT("EX_Window"), Neon = TEXT("EX_Neon"), Frieze = TEXT("EX_Frieze");
    const FHallUV BrickUV = FHallUV::World(192.f), ShingleUV = FHallUV::World(100.f), GrassUV = FHallUV::World(180.f), AsphaltUV = FHallUV::World(500.f),
        ConcreteUV = FHallUV::World(240.f), PlainUV = FHallUV::World(100.f);

    // ---------------------------------------------------------------- primitives
    static void Box(FHallMesh& M, FName K, float X0, float X1, float Y0, float Y1, float Z0, float Z1, const FHallUV& UV, int32 Skip = 0)
    {
        M.BoxMinMax(K, FVector(FMath::Min(X0, X1), FMath::Min(Y0, Y1), FMath::Min(Z0, Z1)), FVector(FMath::Max(X0, X1), FMath::Max(Y0, Y1), FMath::Max(Z0, Z1)), UV, Skip);
    }
    /** One triangle, front facing N (any winding). */
    static void Tri(FHallMesh& M, FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& N, const FHallUV& UV)
    {
        FHallSection& S = M.Sec(K);
        const int32 Base = S.V.Num();
        FVector UAx, VAx; FHallMesh::PlanarAxes(N, UAx, VAx);
        for (const FVector& P : { A, B, C }) { S.V.Add(P); S.N.Add(N); S.UV.Add(M.ComputeUV(P, N, UV)); S.T.Add(FProcMeshTangent(UAx, false)); }
        if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), N) < 0) S.I.Append({ Base, Base + 1, Base + 2 });
        else S.I.Append({ Base, Base + 2, Base + 1 });
    }
    /** Flat quad with an explicit normal (triangles when D == C). */
    static void Face(FHallMesh& M, FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FHallUV& UV)
    {
        FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        if (N.IsNearlyZero()) N = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
        M.Quad(K, A, B, C, D, N, UV);
    }
    /** Box rotated so its local Y runs from A to B (rails, raking cornices). */
    static void Bar(FHallMesh& M, FName K, const FVector& A, const FVector& B, float HalfX, float HalfZ, const FHallUV& UV)
    {
        const FVector Dir = (B - A).GetSafeNormal();
        const FQuat Q = FRotationMatrix::MakeFromYZ(Dir, FVector::UpVector).ToQuat();
        M.Box(K, FTransform(Q, (A + B) * .5f), FVector(HalfX, (B - A).Size() * .5f, HalfZ), UV);
    }
    static float R(FRandomStream& S, float A, float B) { return A + (B - A) * S.FRand(); }

    // ---------------------------------------------------------------- walls
    FVector WP(const FExWall& W, float U, float D, float Z) const
    {
        const float C = W.Face - W.Sign * D;
        return W.Axis == 0 ? FVector(C, U, Z) : FVector(U, C, Z);
    }
    FVector WN(const FExWall& W) const { return W.Axis == 0 ? FVector(W.Sign, 0, 0) : FVector(0, W.Sign, 0); }
    void WallBox(FName K, const FExWall& W, float U0, float U1, float D0, float D1, float Z0, float Z1, const FHallUV& UV)
    {
        const FVector A = WP(W, U0, D0, Z0), B = WP(W, U1, D1, Z1);
        House.BoxMinMax(K, A.ComponentMin(B), A.ComponentMax(B), UV);
    }
    void WindowDetail(const FExWall& W, const FExWin& X)
    {
        if (X.Kind == 3) return;
        const float U0 = X.U0, U1 = X.U1, Z0 = X.Z0, Z1 = X.Z1;
        if (X.Kind == 2)
        {
            WallBox(Trim, W, U0, U0 + 6, 2, 14, Z0, Z1, PlainUV);
            WallBox(Trim, W, U1 - 6, U1, 2, 14, Z0, Z1, PlainUV);
            WallBox(Trim, W, U0, U1, 2, 14, Z1 - 6, Z1, PlainUV);
            if (Z1 - Z0 > 245.f) WallBox(Trim, W, U0 + 6, U1 - 6, 2, 14, Z0 + 228, Z0 + 236, PlainUV);
            WallBox(Stone, W, U0 - 4, U1 + 4, -2, W.Thick, Z0 - 4, Z0 + 1, PlainUV);
            return;
        }
        const float F = 4.f;
        // Perimeter returns join the brick to the actual interior sash. Their
        // solid geometry stays outside the clear opening after glass breaks.
        const float Reveal=W.MuntinD>0?W.MuntinD:30.f;
        WallBox(Trim, W, U0-F, U0, -1, Reveal, Z0, Z1, PlainUV);
        WallBox(Trim, W, U1, U1+F, -1, Reveal, Z0, Z1, PlainUV);
        WallBox(Trim, W, U0-F, U1+F, -1, Reveal, Z1, Z1+F, PlainUV);
        WallBox(Trim, W, U0-F, U1+F, -1, Reveal, Z0-F, Z0, PlainUV);
        WallBox(Stone, W, U0 - 8, U1 + 8, -6, 14, Z0 - 7, Z0 + 1, PlainUV);
        // The fixture owns the glass and breakaway grille, including replication.
    }
    void BuildWall(const FExWall& W,bool Details=true)
    {
        TArray<float> Us = { W.U0, W.U1 };
        for (const FExWin& X : W.Wins) { Us.Add(X.U0); Us.Add(X.U1); }
        Us.Sort();
        for (int32 I = 0; I + 1 < Us.Num(); ++I)
        {
            const float A = FMath::Max(W.U0,Us[I]), B = FMath::Min(W.U1,Us[I + 1]);
            if (B - A < .5f) continue;
            const float Mid = (A + B) * .5f;
            TArray<TPair<float, float>> Cuts;
            for (const FExWin& X : W.Wins) if (Mid > X.U0 && Mid < X.U1) Cuts.Add(TPair<float, float>(X.Z0, X.Z1));
            Cuts.Sort([](const TPair<float, float>& P, const TPair<float, float>& Q) { return P.Key < Q.Key; });
            float Z = W.Z0;
            for (const TPair<float, float>& C : Cuts)
            {
                if (C.Key > Z + .5f) WallBox(Brick, W, A, B, 0, W.Thick, Z, FMath::Min(C.Key,W.Z1), BrickUV);
                Z = FMath::Max(Z, C.Value);
            }
            if (W.Z1 > Z + .5f) WallBox(Brick, W, A, B, 0, W.Thick, Z, W.Z1, BrickUV);
        }
        if (Details) for (const FExWin& X : W.Wins)
        {
            WindowDetail(W, X);
            const float Top=FMath::Min(W.Z1-.3f,X.Z1+22.f);
            if(Top-X.Z1>8.f)
                HouseBrick::Header(BrickDecor,WP(W,FMath::Max(W.U0,X.U0-7.f),-.12f,X.Z1+.15f),
                    WP(W,FMath::Min(W.U1,X.U1+7.f),-.12f,X.Z1+.15f),WN(W),Top-X.Z1);
        }
        // cast-stone water table just below the main floor
        if (W.Z0 < -22.f && W.Z1 > 0.f) WallBox(Stone, W, W.U0 - 6, W.U1 + 6, -6, 2, -22, -8, PlainUV);
    }
    /** Cornice band at the top of a wall; x-running bands are a hair taller than y-running ones so their overlaps never share a plane. */
    void Cornice(const FExWall& W, float Ext0, float Ext1)
    {
        const float Z0 = W.Axis == 1 ? 640.f : 641.f, Z1 = W.Axis == 1 ? 692.f : 691.f;
        WallBox(Trim, W, W.U0 - Ext0, W.U1 + Ext1, -60, 0, Z0, Z1, PlainUV);
        WallBox(Trim, W, W.U0 - Ext0, W.U1 + Ext1, -62, -60, Z0 + 20, Z0 + 30, PlainUV);   // thin bed mould line
    }

    // ---------------------------------------------------------------- roofs
    static void SlopeQuadM(FHallMesh& M, FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FHallUV& UV)
    {
        FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        if (N.IsNearlyZero()) N = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
        if (N.Z < 0) N = -N;
        if (C.Equals(D)) Tri(M, K, A, B, C, N, UV);
        else M.Quad(K, A, B, C, D, N, UV);
    }
    void SlopeQuad(FName K, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FHallUV& UV) { SlopeQuadM(House, K, A, B, C, D, UV); }
    void HipRoof(float X0, float X1, float Y0, float Y1, float Over, float ZE, float P) { HipRoofM(House, X0, X1, Y0, Y1, Over, ZE, P); }
    void HipRoofM(FHallMesh& M, float X0, float X1, float Y0, float Y1, float Over, float ZE, float P)
    {
        const float x0 = X0 - Over, x1 = X1 + Over, y0 = Y0 - Over, y1 = Y1 + Over;
        const float W = x1 - x0, D = y1 - y0;
        if (W >= D)
        {
            const float Half = D * .5f, ZR = ZE + Half * P, CY = (y0 + y1) * .5f;
            const FVector R0(x0 + Half, CY, ZR), R1(x1 - Half, CY, ZR);
            SlopeQuadM(M, Shingle, FVector(x0, y0, ZE), FVector(x1, y0, ZE), R1, R0, ShingleUV);
            SlopeQuadM(M, Shingle, FVector(x1, y1, ZE), FVector(x0, y1, ZE), R0, R1, ShingleUV);
            SlopeQuadM(M, Shingle, FVector(x0, y1, ZE), FVector(x0, y0, ZE), R0, R0, ShingleUV);
            SlopeQuadM(M, Shingle, FVector(x1, y0, ZE), FVector(x1, y1, ZE), R1, R1, ShingleUV);
            Bar(M, Metal, R0 + FVector(0, 0, 2), R1 + FVector(0, 0, 2), 6, 3, PlainUV);
        }
        else
        {
            const float Half = W * .5f, ZR = ZE + Half * P, CX = (x0 + x1) * .5f;
            const FVector R0(CX, y0 + Half, ZR), R1(CX, y1 - Half, ZR);
            SlopeQuadM(M, Shingle, FVector(x0, y1, ZE), FVector(x0, y0, ZE), R0, R1, ShingleUV);
            SlopeQuadM(M, Shingle, FVector(x1, y0, ZE), FVector(x1, y1, ZE), R1, R0, ShingleUV);
            SlopeQuadM(M, Shingle, FVector(x0, y0, ZE), FVector(x1, y0, ZE), R0, R0, ShingleUV);
            SlopeQuadM(M, Shingle, FVector(x1, y1, ZE), FVector(x0, y1, ZE), R1, R1, ShingleUV);
            Bar(M, Metal, R0 + FVector(0, 0, 2), R1 + FVector(0, 0, 2), 6, 3, PlainUV);
        }
    }
    /** The underside of a hipped roof, 2 cm under the shingles and facing down into the attic. The shingles are one-sided, so a
     *  hairline gap where a room's wall meets its ceiling used to look straight through the roof at the sky: invisible against
     *  the night sky, a glowing line against the dusk one. Now such a gap shows a dark roof void. */
    void AtticLiner(float X0, float X1, float Y0, float Y1, float Over, float ZE, float P)
    {
        const float x0 = X0 - Over, x1 = X1 + Over, y0 = Y0 - Over, y1 = Y1 + Over;
        const float W = x1 - x0, D = y1 - y0;
        const FVector Drop(0, 0, -2);
        auto Under = [&](const FVector& A, const FVector& Bv, const FVector& C, const FVector& Dv)
        {
            FVector N = FVector::CrossProduct(Bv - A, C - A).GetSafeNormal();
            if (N.IsNearlyZero()) N = FVector::CrossProduct(Bv - A, Dv - A).GetSafeNormal();
            if (N.Z > 0) N = -N;
            if (C.Equals(Dv)) Tri(Attic, Black, A + Drop, Bv + Drop, C + Drop, N, PlainUV);
            else Attic.Quad(Black, A + Drop, Bv + Drop, C + Drop, Dv + Drop, N, PlainUV);
        };
        if (W >= D)
        {
            const float Half = D * .5f, ZR = ZE + Half * P, CY = (y0 + y1) * .5f;
            const FVector R0(x0 + Half, CY, ZR), R1(x1 - Half, CY, ZR);
            Under(FVector(x0, y0, ZE), FVector(x1, y0, ZE), R1, R0);
            Under(FVector(x1, y1, ZE), FVector(x0, y1, ZE), R0, R1);
            Under(FVector(x0, y1, ZE), FVector(x0, y0, ZE), R0, R0);
            Under(FVector(x1, y0, ZE), FVector(x1, y1, ZE), R1, R1);
        }
        else
        {
            const float Half = W * .5f, ZR = ZE + Half * P, CX = (x0 + x1) * .5f;
            const FVector R0(CX, y0 + Half, ZR), R1(CX, y1 - Half, ZR);
            Under(FVector(x0, y1, ZE), FVector(x0, y0, ZE), R0, R1);
            Under(FVector(x1, y0, ZE), FVector(x1, y1, ZE), R1, R0);
            Under(FVector(x0, y0, ZE), FVector(x1, y0, ZE), R0, R0);
            Under(FVector(x1, y1, ZE), FVector(x0, y1, ZE), R1, R1);
        }
    }
    /** Roof surface height of a hipped roof at (X,Y) (for chimneys). */
    static float HipZ(float X, float Y, float X0, float X1, float Y0, float Y1, float Over, float ZE, float P)
    {
        const float x0 = X0 - Over, x1 = X1 + Over, y0 = Y0 - Over, y1 = Y1 + Over;
        const float Dx = FMath::Min(X - x0, x1 - X), Dy = FMath::Min(Y - y0, y1 - Y);
        return ZE + FMath::Max(0.f, FMath::Min(Dx, Dy)) * P;
    }
    void Chimney(float CX, float CY, float ZBase, float ZTopC, float W, float D)
    {
        Box(House, BrickRed, CX - W / 2, CX + W / 2, CY - D / 2, CY + D / 2, ZBase, ZTopC, BrickUV);
        Box(House, Stone, CX - W / 2 - 8, CX + W / 2 + 8, CY - D / 2 - 8, CY + D / 2 + 8, ZTopC, ZTopC + 10, PlainUV);
        Box(House, Black, CX - W / 4, CX - W / 4 + 22, CY - 11, CY + 11, ZTopC + 10, ZTopC + 32, PlainUV);
        Box(House, Black, CX + W / 4 - 22, CX + W / 4, CY - 11, CY + 11, ZTopC + 10, ZTopC + 32, PlainUV);
    }

    // ---------------------------------------------------------------- arcade (round arches between brick piers, wall runs along x)
    void Arcade(const TArray<float>& Piers, float Y0, float Y1, float ZB, float ZSpring, float ZT)
    {
        for (int32 I = 0; I + 1 < Piers.Num(); I += 2) Box(House, Brick, Piers[I], Piers[I + 1], Y0, Y1, ZB, ZT, BrickUV);
        for (int32 I = 1; I + 2 < Piers.Num(); I += 2)
        {
            const float A = Piers[I], B = Piers[I + 1];
            const float Rd = (B - A) * .5f, Cx = (A + B) * .5f;
            HouseBrick::Arch(BrickDecor,Cx,ZSpring,Rd,Y1+.12f,1.f,ZT);
            HouseBrick::Arch(BrickDecor,Cx,ZSpring,Rd,Y0-.12f,-1.f,ZT);
            const int32 N = 14;
            for (int32 K = 0; K < N; ++K)
            {
                const float T0 = PI * K / N, T1 = PI * (K + 1) / N;
                const float Xa = Cx + Rd * FMath::Cos(T0), Xb = Cx + Rd * FMath::Cos(T1);
                const float Za = ZSpring + Rd * FMath::Sin(T0), Zb = ZSpring + Rd * FMath::Sin(T1);
                // outer face (+y) and inner face (-y)
                House.Quad(Brick, FVector(Xa, Y1, ZT), FVector(Xb, Y1, ZT), FVector(Xb, Y1, Zb), FVector(Xa, Y1, Za), FVector(0, 1, 0), BrickUV);
                House.Quad(Brick, FVector(Xa, Y0, ZT), FVector(Xb, Y0, ZT), FVector(Xb, Y0, Zb), FVector(Xa, Y0, Za), FVector(0, -1, 0), BrickUV);
                // intrados
                const FVector Nm = -FVector(FMath::Cos((T0 + T1) * .5f), 0, FMath::Sin((T0 + T1) * .5f));
                House.Quad(Stone, FVector(Xa, Y0, Za), FVector(Xb, Y0, Zb), FVector(Xb, Y1, Zb), FVector(Xa, Y1, Za), Nm, PlainUV);
            }
        }
    }

    // ---------------------------------------------------------------- the house shell
    void BuildHouse()
    {
        MeasureConnections();
        TArray<FExWall> Walls;
        auto W = [&](int32 Axis, float Sign, float FaceC, float Thick, float U0, float U1, float Z0, float Z1, std::initializer_list<FExWin> Wins, float MuntinD = 0.f)
        {
            FExWall X; X.Axis = Axis; X.Sign = Sign; X.Face = FaceC; X.Thick = Thick; X.U0 = U0; X.U1 = U1; X.Z0 = Z0; X.Z1 = Z1; X.MuntinD = MuntinD;
            for (const FExWin& Wn : Wins) X.Wins.Add(Wn);
            Walls.Add(X);
        };
        const float ZB = -400.f;
        // Centre block front (faces -y). Outside the portico it rises to the cornice; inside, the scanned balcony sits above z 310.
        W(1, -1, -1010, 45, -1420, -830, ZB, ZTop, { {-1172, -1016, 37, 264, 0}, {-1167, -1003, 398, 562, 0} });
        // The central entrance has its own fitted walnut double-door frame.
        // Keep the opening free of the old fixed glass transom bars.
        W(1, -1, -1010, 45, -830, 790, ZB, 296, { {-548, -412, 39, 240, 0}, {-186, 113, -2, 270, 3}, {408, 596, 39, 240, 0} });
        W(1, -1, -1010, 45, 790, 1360, ZB, ZTop, { {1006, 1135, 37, 252, 0}, {1003, 1167, 398, 562, 0} });
        // Left wing: front (with the hole the impossible hallway leaves through), inner face, end, rear (jogged at the stair hall).
        W(1, -1, -2200, 55, -3400, -1420, ZB, ZTop, { {-3150, -3000, 40, 240, 1}, {-2850, -2700, 40, 240, 1}, {-1900, -1750, 40, 240, 1},
            {-3150, -3000, 398, 562, 1}, {-2850, -2700, 398, 562, 1}, {-1900, -1750, 398, 562, 1} });
        W(0, 1, -1420, 50, -2200, -1010, ZB, ZTop, { {-1900, -1750, 40, 240, 1}, {-1400, -1250, 40, 240, 1}, {-1900, -1750, 398, 562, 1}, {-1400, -1250, 398, 562, 1} });
        W(0, -1, -3400, 70, -2200, 1540, ZB, ZTop, { {-39, 157, 415, 559, 0}, {1042, 1238, 417, 562, 0}, {-1470, -1316, 395, 580, 1},
            {-39, 157, 40, 240, 1}, {1042, 1238, 40, 240, 1}, {-1470, -1316, 40, 240, 1}, {-900, -720, 40, 240, 1}, {-900, -720, 415, 559, 1}, {-1950, -1800, 40, 240, 1}, {-1950, -1800, 415, 559, 1} });
        W(1, 1, 1540, 55, -3400, -2150, ZB, ZTop, { {-3114, -2922, 418, 562, 0}, {-2650, -2460, 418, 562, 1}, {-3114, -2922, 40, 240, 1}, {-2650, -2460, 40, 240, 1},
            {-3114, -2922, -250, -110, 1}, {-2650, -2460, -250, -110, 1} });
        W(1, 1, 1480, 60, -2150, -1530, ZB, ZTop, { {-2521, -2341, 374, 602, 0}, {-1868, -1710, 366, 538, 0}, {-1868, -1710, 40, 240, 1}, {-1868, -1710, -250, -110, 1} });
        W(0, 1, -2150, 45, 1480, 1540, ZB, ZTop, {});
        // Right wing: inner face, front, end (with the porte-cochere door), rear.
        W(0, -1, 1360, 55, -2130, -1010, ZB, ZTop, { {-1877, -1677, 425, 570, 0}, {-1877, -1677, 40, 240, 1} }, 65.f);
        W(1, -1, -2130, 60, 1360, 3340, ZB, ZTop, { {2305, 2448, 386, 547, 0}, {1700, 1850, 386, 547, 1}, {2900, 3050, 386, 547, 1},
            {1700, 1850, 40, 240, 1}, {2305, 2448, 40, 240, 1}, {2900, 3050, 40, 240, 1} });
        W(0, 1, 3340, 60, -2130, 1540, ZB, ZTop, { {-1143, -980, -2, 222, 2}, {-446, -264, 41, 214, 0}, {289, 472, 41, 214, 0}, {1014, 1197, 41, 214, 0},
            {-1806, -1612, 430, 574, 0}, {1111, 1305, 430, 575, 0}, {-446, -264, 410, 570, 1}, {289, 472, 410, 570, 1} });
        W(1, 1, 1540, 55, 1530, 3340, ZB, ZTop, { {1753, 1902, 50, 192, 0}, {2853, 3002, 47, 210, 0}, {1567, 1742, 427, 572, 0}, {2089, 2289, 428, 572, 0},
            {2853, 3002, 427, 572, 1}, {2089, 2289, 47, 210, 1}, {1753, 1902, -250, -110, 1}, {2853, 3002, -250, -110, 1} });
        for (const FExWall& X : Walls) BuildConnectedWall(X);
        // Restore the continuous, authored cornices around the original outline.
        auto Cor = [&](int32 I, float E0, float E1) { Cornice(Walls[I], E0, E1); };
        Cor(0, 60, 0); Cor(2, 0, 0); Cor(3, 60, 60); Cor(4, 60, 0); Cor(5, 60, 60); Cor(6, 60, 60); Cor(7, 0, 190); Cor(8, 0, 60);
        Cor(9, 60, 0); Cor(10, 60, 60); Cor(11, 60, 60); Cor(12, 190, 60);
        // The balcony facade and the porch face used to be absent from the
        // outside skin. Both now share the actual room openings and jambs.
        FExWall FrontUpper=Walls[1];FrontUpper.Z0=318;FrontUpper.Z1=640;
        FrontUpper.Wins={{-116,79,332,571,3}};BuildConnectedWall(FrontUpper);
        FExWall Rear;Rear.Axis=1;Rear.Sign=1;Rear.Face=963;Rear.Thick=8;
        Rear.U0=-1400;Rear.U1=1400;Rear.Z0=-22;Rear.Z1=640;Rear.MuntinD=0;
        BuildConnectedWall(Rear);
        // Walls above the porch roof (wings facing the pergola) and the deck-end skins.
        FExWall PorchSide;PorchSide.Axis=0;PorchSide.Sign=1;PorchSide.Face=-1400;PorchSide.Thick=8;
        PorchSide.U0=950;PorchSide.U1=1480;PorchSide.Z0=372;PorchSide.Z1=640;PorchSide.MuntinD=0;
        BuildConnectedWall(PorchSide);
        PorchSide.Sign=-1;PorchSide.Face=1400;PorchSide.U1=1540;BuildConnectedWall(PorchSide);
        Box(House, Trim, -1400, -1340, 950, 1540, 641, 691, PlainUV);
        Box(House, Trim, 1340, 1400, 950, 1600, 641, 691, PlainUV);
        Box(House, Trim, -1400, 1400, 950, 1010, 640, 692, PlainUV);
        Box(House, Brick, -1530, -1470, 1480, 1760, ZB, 380, BrickUV);
        Box(House, Brick, 1470, 1530, 1540, 1760, ZB, 380, BrickUV);
        Box(House, Stone, -1538, -1462, 1480, 1768, 380, 392, PlainUV);
        Box(House, Stone, 1462, 1538, 1540, 1768, 380, 392, PlainUV);
        // Roofs.
        HipRoof(-3400, -1420, -2200, 1540, 60, ZEave, RoofPitch);
        HipRoof(-1420, 1360, -1010, 950, 60, ZEave, RoofPitch);
        HipRoof(1360, 3340, -2130, 1540, 60, ZEave, RoofPitch);
        AtticLiner(-3400, -1420, -2200, 1540, 60, ZEave, RoofPitch);
        AtticLiner(-1420, 1360, -1010, 950, 60, ZEave, RoofPitch);
        AtticLiner(1360, 3340, -2130, 1540, 60, ZEave, RoofPitch);
        // Behind the portico the front wall stops at the cornice line with no cornice of its own: close that band from inside.
        Attic.Quad(Black, FVector(-830, -1009, 636), FVector(790, -1009, 636), FVector(790, -1009, 694), FVector(-830, -1009, 694), FVector(0, 1, 0), PlainUV);
        Chimney(-2600, 300, HipZ(-2600, 300, -3400, -1420, -2200, 1540, 60, ZEave, RoofPitch) - 60, 1180, 130, 70);
        Chimney(2350, -500, HipZ(2350, -500, 1360, 3340, -2130, 1540, 60, ZEave, RoofPitch) - 60, 1230, 130, 70);
        Chimney(900, 600, HipZ(900, 600, -1420, 1360, -1010, 950, 60, ZEave, RoofPitch) - 60, 1110, 110, 70);
        BuildPortico();
        BuildPorch();
        BuildPorteCochere();
    }

    void BuildPortico()
    {
        // Open porch ends: the columns support the balcony without tall brick side walls.
        Box(House, Stone, -836, 796, -1491, -1010, -22, -8, PlainUV);
        // The photo-matched concrete forecourt supplies the floor and centered steps.
        // Retain the fitted threshold through the wall at the walnut front doors.
        Box(House, Stone, -200, 130, -1012, -918, -30, -1, PlainUV);
        // Keep the balcony slab, with six full-height photo-matched columns.
        Box(House, Trim, -805, 765, -1485, -1010, 296, 306, PlainUV);
        BuildHouseFrontPortico(House,Owner,Look);
        // Entablature over the scanned balcony (frieze with the chapter name), cornice and pediment.
        Box(House, Trim, -830, 790, -1485, -1010, 628, 691, PlainUV);
        House.QuadUV(Frieze, FVector(-700, -1486, 636), FVector(660, -1486, 636), FVector(660, -1486, 686), FVector(-700, -1486, 686), FVector(0, -1, 0),
            FVector2D(1, 1), FVector2D(0, 1), FVector2D(0, 0), FVector2D(1, 0), FVector(-1, 0, 0));
        Box(House, Trim, -850, 810, -1510, -1010, 692, 712, PlainUV);
        const float Apex = 712.f + 810.f * .42f;
        const FVector TL(-830, -1485, 712), TR(790, -1485, 712), TA(-20, -1485, Apex);
        Tri(House, Trim, TL, TR, TA, FVector(0, -1, 0), PlainUV);                     // tympanum
        const FVector BL(-850, -1510, 712), BR(810, -1510, 712), BA(-20, -1510, Apex + 10);
        const FVector KL(-850, 40, 712), KR(810, 40, 712), KA(-20, 40, Apex + 10);
        SlopeQuad(Shingle, BL, BA, KA, KL, ShingleUV);
        SlopeQuad(Shingle, BA, BR, KR, KA, ShingleUV);
        Bar(House, Trim, BL + FVector(0, 0, -2), BA + FVector(0, 0, -2), 14, 12, PlainUV);  // raking cornices
        Bar(House, Trim, BA + FVector(0, 0, -2), BR + FVector(0, 0, -2), 14, 12, PlainUV);
        // Porch lanterns either side of the door and a hanging lantern.
        for (float LX : { -260.f, 190.f })
        {
            // Detailed lantern cages are built with the front portico.
            LightSpecs.Add({ FVector(LX, -1030, 190), FLinearColor(1.f, .78f, .5f), 1300.f, 600.f, 8000.f });
        }
        LightSpecs.Add({ FVector(-20, -1245, 230), FLinearColor(1.f, .8f, .55f), 1800.f, 1600.f, 8000.f });
    }

    void BuildPorch()
    {
        // Brick wall under the deck, opened in front of the basement's four French patio pairs (HallLayout::Pairs, authored x 603 / 0 / -603 / -1092,
        // frames 270 / 270 / 270 / 230, scaled by 1.1), then the end walls, patio slab and the deck's underside.
        {
            FExWall X; X.Axis = 1; X.Sign = 1.f; X.Face = 921.f; X.Thick = 8.f; X.U0 = -1400.f; X.U1 = 1400.f; X.Z0 = -400.f; X.Z1 = -22.f; X.MuntinD = 0.f;
            for (const FVector2D& D : { FVector2D(663.f, 140.f), FVector2D(0.f, 140.f), FVector2D(-663.f, 140.f), FVector2D(-1201.f, 118.f) })
                X.Wins.Add({ D.X - D.Y, D.X + D.Y, -362.f, -116.f, 2 });
            BuildConnectedWall(X);
        }
        Box(House, Brick, -1470, -1400, 913, 1720, -400, -22, BrickUV);
        Box(House, Brick, 1400, 1470, 913, 1720, -400, -22, BrickUV);
        Box(House, TEXT("RP_Patio"), -1470, 1470, 905, 1720, -400, GR + 3, FHallUV::World(256));
        Box(House, Dark, -1400, 1400, 913, 1720, -30, -24, PlainUV);
        // Arcade under the deck edge (piers under the porch columns, round arches between).
        Arcade({ -1470, -1330, -880, -620, -150, 110, 580, 840, 1330, 1470 }, 1560, 1720, -400, -290, -22);
        BuildHouseRearPorchDetail(House,CourtDecor,Look);
        // Patio and pergola lights.
        for (float LX : { -700.f, 700.f })
        {
            Box(House, Lamp, LX - 15, LX + 15, 1330, 1360, -40, -30, PlainUV);
            LightSpecs.Add({ FVector(LX, 1345, -60), FLinearColor(1.f, .8f, .55f), 1000.f, 900.f, 4500.f });
            LightSpecs.Add({ FVector(LX, 1330, 300), FLinearColor(1.f, .8f, .55f), 700.f, 800.f, 4500.f });
        }
        // The photo's warm ceiling pools keep the two furnished end bays readable.
        for(float LX:{-1220.f,1220.f})
        {
            Box(House,Lamp,LX-12,LX+12,1290,1320,-40,-30,PlainUV);
            LightSpecs.Add({FVector(LX,1305,-58),FLinearColor(1.f,.84f,.66f),850.f,600.f,4300.f});
        }
        LightSpecs.Add({ FVector(-1620, 1560, 520), FLinearColor(.85f, .9f, 1.f), 2500.f, 1800.f, 6000.f });
        LightSpecs.Add({ FVector(1620, 1560, 520), FLinearColor(.85f, .9f, 1.f), 2500.f, 1800.f, 6000.f });
    }

    void BuildPorteCochere()
    {
        // Rebuilt from the photos of the real one (HouseSideCanopy): hip roof, paneled entablature, paired columns on brick
        // plinths, the railed stoop and the door's lanterns. The old ceiling light is now the hanging lantern's.
        HouseSideCanopy::Build(House);
        Box(House, Stone, 3228, 3345, -1150, -975, -30, -1, PlainUV);
        LightSpecs.Add({ HouseSideCanopy::LanternLight(), FLinearColor(1.f, .8f, .55f), 2500.f, 900.f, 8000.f });
    }

};

// ------------------------------------------------------------------ grounds and surroundings
struct FExRect { float X0, X1, Y0, Y1; };

void ExSubtract(TArray<FExRect>& Pieces, const FExRect& Cut)
{
    TArray<FExRect> Out;
    for (const FExRect& P : Pieces)
    {
        if (Cut.X1 <= P.X0 || Cut.X0 >= P.X1 || Cut.Y1 <= P.Y0 || Cut.Y0 >= P.Y1) { Out.Add(P); continue; }
        if (Cut.X0 > P.X0) Out.Add({ P.X0, Cut.X0, P.Y0, P.Y1 });
        if (Cut.X1 < P.X1) Out.Add({ Cut.X1, P.X1, P.Y0, P.Y1 });
        const float MX0 = FMath::Max(P.X0, Cut.X0), MX1 = FMath::Min(P.X1, Cut.X1);
        if (Cut.Y0 > P.Y0) Out.Add({ MX0, MX1, P.Y0, Cut.Y0 });
        if (Cut.Y1 < P.Y1) Out.Add({ MX0, MX1, Cut.Y1, P.Y1 });
    }
    Pieces = Out;
}
void ExSplitY(TArray<FExRect>& Pieces, float Y)
{
    TArray<FExRect> Out;
    for (const FExRect& P : Pieces)
    {
        if (Y > P.Y0 + 1.f && Y < P.Y1 - 1.f) { Out.Add({ P.X0, P.X1, P.Y0, Y }); Out.Add({ P.X0, P.X1, Y, P.Y1 }); }
        else Out.Add(P);
    }
    Pieces = Out;
}
void ExSplitX(TArray<FExRect>& Pieces, float X)
{
    TArray<FExRect> Out;
    for (const FExRect& P : Pieces)
    {
        if (X > P.X0 + 1.f && X < P.X1 - 1.f) { Out.Add({ P.X0, X, P.Y0, P.Y1 }); Out.Add({ X, P.X1, P.Y0, P.Y1 }); }
        else Out.Add(P);
    }
    Pieces = Out;
}
/** Splits only the pieces that straddle R's edges, so every piece then lies wholly inside or wholly outside R; a piece
 *  already inside or clear of R keeps its exact shape (and so its patch grid). */
void ExSplitAround(TArray<FExRect>& Pieces, const FExRect& R)
{
    TArray<FExRect> Out;
    for (const FExRect& P : Pieces)
    {
        const bool bOverlap = P.X0 < R.X1 && P.X1 > R.X0 && P.Y0 < R.Y1 && P.Y1 > R.Y0;
        const bool bInside = P.X0 >= R.X0 && P.X1 <= R.X1 && P.Y0 >= R.Y0 && P.Y1 <= R.Y1;
        if (!bOverlap || bInside) { Out.Add(P); continue; }
        TArray<FExRect> Part = { P };
        ExSplitX(Part, R.X0); ExSplitX(Part, R.X1); ExSplitY(Part, R.Y0); ExSplitY(Part, R.Y1);
        Out.Append(Part);
    }
    Pieces = Out;
}

/** Grid of quads following the ground (HouseExteriorSite::GroundHeight: the site grade plus the Calder relief, which is 0 in
 *  the frozen zones) +Raise. Skirts close the sides down to the ground when the patch sits off the ground plane. */
/** ExPatch with its grid given: NX by NY quads. */
void ExPatchGrid(FHallMesh& M, FName K, const FExRect& R, int32 NX, int32 NY, float Raise, const FHallUV& UV, bool bSkirt, FName SkirtKey = NAME_None)
{
    auto P = [&](int32 I, int32 J) { const float X = R.X0 + (R.X1 - R.X0) * I / NX, Y = R.Y0 + (R.Y1 - R.Y0) * J / NY; return FVector(X, Y, HouseExteriorSite::GroundHeight(X, Y) + Raise); };
    for (int32 J = 0; J < NY; ++J)
        for (int32 I = 0; I < NX; ++I)
        {
            const FVector A = P(I, J), B = P(I + 1, J), C = P(I + 1, J + 1), D = P(I, J + 1);
            FVector N = FVector::CrossProduct(B - A, D - A).GetSafeNormal(); if (N.Z < 0) N = -N;
            M.Quad(K, A, B, C, D, N, UV);
        }
    if (!bSkirt) return;
    const FName SK = SkirtKey.IsNone() ? K : SkirtKey;
    auto Skirt = [&](const FVector& A, const FVector& B, const FVector& Nrm)
    {
        const FVector A2(A.X, A.Y, HouseExteriorSite::GroundHeight(A.X, A.Y) - 40.f), B2(B.X, B.Y, HouseExteriorSite::GroundHeight(B.X, B.Y) - 40.f);
        M.Quad(SK, A, B, B2, A2, Nrm, UV);
    };
    for (int32 I = 0; I < NX; ++I) { Skirt(P(I, 0), P(I + 1, 0), FVector(0, -1, 0)); Skirt(P(I + 1, NY), P(I, NY), FVector(0, 1, 0)); }
    for (int32 J = 0; J < NY; ++J) { Skirt(P(NX, J), P(NX, J + 1), FVector(1, 0, 0)); Skirt(P(0, J + 1), P(0, J), FVector(-1, 0, 0)); }
}

void ExPatch(FHallMesh& M, FName K, const FExRect& R, float Cell, float Raise, const FHallUV& UV, bool bSkirt, FName SkirtKey = NAME_None)
{
    ExPatchGrid(M, K, R, FMath::Max(1, FMath::CeilToInt((R.X1 - R.X0) / Cell)), FMath::Max(1, FMath::CeilToInt((R.Y1 - R.Y0) / Cell)), Raise, UV, bSkirt, SkirtKey);
}

/** ExPatch for a patch running along Y out of a frozen zone (the side street and its sidewalks: Calder Dr leaves the frozen
 *  zones at Y 19800 and climbs). On the Calder terrain it is cut at every frozen rectangle's Y edge across its whole width, with
 *  today's rows kept where they are, so the frozen part keeps today's surface to the millimetre (one quad across the edge
 *  took the relief beyond it and tilted inside it by up to 2 cm; Calder 1d/1e verification, 2026-10-16). With the terrain
 *  off it is exactly ExPatch. */
void ExPatchFrozenCut(FHallMesh& M, FName K, const FExRect& R, float Cell, float Raise, const FHallUV& UV, bool bSkirt, FName SkirtKey = NAME_None)
{
    const int32 NX = FMath::Max(1, FMath::CeilToInt((R.X1 - R.X0) / Cell)), NY = FMath::Max(1, FMath::CeilToInt((R.Y1 - R.Y0) / Cell));
    TArray<float> Edges;
    if (HouseCalderTerrain::Enabled())
        for (const FBox2D& F : HouseCalderTerrain::FrozenRects())
            if (F.Min.X <= R.X0 && F.Max.X >= R.X1)
                for (const double Edge : { F.Min.Y, F.Max.Y })
                    if (Edge > R.Y0 && Edge < R.Y1) Edges.AddUnique(float(Edge));
    Edges.Sort();
    auto RowY = [&R, NY](int32 J) { return R.Y0 + (R.Y1 - R.Y0) * J / NY; };
    int32 J0 = 0;      // today's row the next piece starts on ...
    float From = R.Y0;  // ... at this Y (an edge, or that row)
    for (const float Edge : Edges)
    {
        const int32 Below = FMath::Clamp(FMath::FloorToInt(double(Edge - R.Y0) / double(R.Y1 - R.Y0) * NY), 0, NY - 1);
        if (Below < J0 || Edge <= From || FMath::Abs(RowY(Below) - Edge) < .01f || FMath::Abs(RowY(Below + 1) - Edge) < .01f) continue;
        if (Below > J0) ExPatchGrid(M, K, { R.X0, R.X1, From, RowY(Below) }, NX, Below - J0, Raise, UV, bSkirt, SkirtKey);
        ExPatchGrid(M, K, { R.X0, R.X1, Below > J0 ? RowY(Below) : From, Edge }, NX, 1, Raise, UV, bSkirt, SkirtKey);
        ExPatchGrid(M, K, { R.X0, R.X1, Edge, RowY(Below + 1) }, NX, 1, Raise, UV, bSkirt, SkirtKey);
        J0 = Below + 1;
        From = RowY(J0);
    }
    if (J0 < NY) ExPatchGrid(M, K, { R.X0, R.X1, From, R.Y1 }, NX, NY - J0, Raise, UV, bSkirt, SkirtKey);
}

struct FExSite
{
    FExBuilder& B;
    FHallMesh& G;
    TArray<FTransform> TreeA, TreeB, ForestDistant, Bush, Poles, LotPoles, CarsA, CarsB, CarsC, Chairs;
    explicit FExSite(FExBuilder& InB) : B(InB), G(InB.Grounds) {}

    bool Free(float X, float Y) const
    {
        for (const FExBuilder::FNoGo& N : B.NoGo) if (X > N.X0 && X < N.X1 && Y > N.Y0 && Y < N.Y1) return false;
        return true;
    }
    void NoGo(float X0, float X1, float Y0, float Y1) { B.NoGo.Add({ X0, X1, Y0, Y1 }); }
    void Slab(FName K, float X0, float X1, float Y0, float Y1, float Z0, float Z1, const FHallUV& UV) { FExBuilder::Box(G, K, X0, X1, Y0, Y1, Z0, Z1, UV); }
    /** Thin painted line lying on a flat surface. */
    void Mark(FName K, float X0, float X1, float Y0, float Y1, float Z) { G.Quad(K, FVector(X0, Y0, Z), FVector(X1, Y0, Z), FVector(X1, Y1, Z), FVector(X0, Y1, Z), FVector::UpVector, B.PlainUV); }
    /** Street poles are wooden utility poles carrying the wires; the rest are steel lot poles. Every lamp is a sodium
     *  cobra head whose existing light becomes a spot aimed down (HouseStreetScene). */
    void Pole(float X, float Y, float Yaw, bool bParkingLight = false, bool bStreet = true)
    {
        const float Z = HouseExteriorSite::GroundHeight(X, Y);
        (bStreet ? Poles : LotPoles).Add(FTransform(FRotator(0, Yaw, 0), FVector(X, Y, Z)));
        const FVector Head = FVector(X, Y, Z) + FRotator(0, Yaw, 0).RotateVector(HouseStreetScene::LensOffset());
        // Street cobra heads are 250-400 W sodium class, bright enough to pool orange on the road against the blue hour.
        FExBuilder::FLightSpec L{ Head, HouseStreetScene::SodiumColor(), bParkingLight ? 42000.f : bStreet ? 48000.f : 12500.f,
            bParkingLight ? 2600.f : bStreet ? 2400.f : 2000.f, bParkingLight ? 6000.f : bStreet ? 6500.f : 5500.f };
        L.bSpotDown = true;
        B.LightSpecs.Add(L);
    }
    void Car(int32 Kind, float X, float Y, float Yaw)
    {
        // On the Calder relief a car pitches and rolls with the ground under it; where the relief under it is flat (every
        // frozen zone, all of today and -NoCalderTerrain) it stands level exactly as before.
        FRotator CarRot(0, Yaw, 0);
        if (!HouseCalderTerrain::ReliefFlat(FBox2D(FVector2D(X - 500, Y - 500), FVector2D(X + 500, Y + 500))))
        {
            const FVector CarUp = HouseExteriorSite::GroundNormal(X, Y);
            if (CarUp.Z < 1.f) CarRot = FRotationMatrix::MakeFromZX(CarUp, CarRot.Vector()).Rotator();
        }
        const FTransform T(CarRot, FVector(X, Y, HouseExteriorSite::GroundHeight(X, Y)));
        (Kind == 0 ? CarsA : Kind == 1 ? CarsB : CarsC).Add(T);
        NoGo(X - 500, X + 500, Y - 300, Y + 300);
    }

    void Terrain()
    {
        const float E = 32000.f;
        // Under the campus plan the side street stops at the junction: its old campus stretch becomes lawn and Calder Dr.
        const float SideY0 = HouseCampusLayout::Enabled() ? HouseCampusLayout::JunctionSouthY : -E;
        TArray<FExRect> Pieces = { { -E, E, -E, E } };
        // Soil extends two centimeters under the brick skin, avoiding a sky
        // slit at grazing angles where the terrain and facade meet. The left
        // rear wing steps inward at X=-2150; its soil edge follows Y=1480
        // there instead of leaving the old sixty-centimeter rectangular void.
        // Court side walls end at +/-1508, so soil overlaps them by 2 cm.
        // The previous +/-1530 cut left a 22 cm open slit between the piers.
        // The basement stair connection crosses a former 4 cm seam between
        // wing footprints; soil there intersected the player inside the house.
        for (const FExRect& Cut : { FExRect{ -3398, -2152, -2198, 1538 }, FExRect{ -2152, -1422, -2198, 1478 }, FExRect{ -1418, 1358, -1008, 1718 }, FExRect{ 1362, 3338, -2128, 1538 },
            FExRect{ -1540, -1320, 570, 885 },
            FExRect{ -1506, 1506, 1720, 3820 },
            FExRect{ -E, 8700, StreetY(-4350), StreetY(-2950) }, FExRect{ 8700, 10300, SideY0, E } }) ExSubtract(Pieces, Cut);
        // The campus plan's roads and lots sit at street level, so the lawn is cut away under them (HouseCampusLayout).
        TArray<FBox2D> PlanCuts;
        HouseCampusLayout::TerrainCuts(PlanCuts);
        for (const FBox2D& Cut : PlanCuts) ExSubtract(Pieces, FExRect{ float(Cut.Min.X), float(Cut.Max.X), float(Cut.Min.Y), float(Cut.Max.Y) });
        // Replace this rear patch with a finer, continuous woodland height field.
        // The patch border blends exactly to the old grade, away from the court.
        ExSubtract(Pieces,{HouseWoods::XMin,HouseWoods::XMax,HouseWoods::YMin,HouseWoods::YMax});
        ExSplitY(Pieces, SlopeY0); ExSplitY(Pieces, SlopeY1);
        for (float S : { -7000.f, 7000.f }) ExSplitX(Pieces, S);
        for (float S : { -5000.f, 5000.f }) ExSplitY(Pieces, S);
        // Resolve the new field and smooth bank junction with a fine, continuous
        // collision surface; surrounding roads and building grade remain intact.
        for(float X:{-21000.f,-19500.f,-4300.f,-3700.f,-2830.f,-2500.f})ExSplitX(Pieces,X);
        for(float Y:{300.f,1650.f,1770.f,2120.f,3850.f,4300.f,4600.f,6500.f})ExSplitY(Pieces,Y);
        // Calder terrain (step 1b): outside the frozen zones the ground is the Calder tiles (HouseCalderTerrainTiles), not
        // these patches. The pieces are split at the frozen rectangles' edges first, so each lies wholly in or out of one
        // (a frozen piece keeps today's exact grid), and the campus buildings' footprints become holes, as the house's is.
        if (HouseCalderTerrain::Enabled())
        {
            for (const HouseCampusLayout::FBlock& Blk : HouseCampusLayout::Blocks()) ExSubtract(Pieces, FExRect{ Blk.X0, Blk.X1, Blk.Y0, Blk.Y1 });
            for (const FBox2D& R : HouseCalderTerrain::FrozenRects()) ExSplitAround(Pieces, FExRect{ float(R.Min.X), float(R.Max.X), float(R.Min.Y), float(R.Max.Y) });
            TArray<FExRect> Frozen;
            for (const FExRect& P : Pieces)
            {
                const FBox2D Box(FVector2D(P.X0, P.Y0), FVector2D(P.X1, P.Y1));
                if (HouseCalderTerrain::BoxFrozen(Box)) Frozen.Add(P);
                else B.CalderRegions.Add(Box);
            }
            Pieces = Frozen;
        }
        for (const FExRect& P : Pieces)
        {
            const bool bNear = P.X1 <= 7000.f && P.X0 >= -7000.f && P.Y1 <= 5000.f && P.Y0 >= -5000.f;
            const bool bSlope = P.Y0 >= SlopeY0 - 1.f && P.Y1 <= SlopeY1 + 1.f;
            const bool Field=P.X0>=-21000&&P.X1<=-2500&&P.Y0>=0&&P.Y1<=6500;
            ExPatch(G, B.Grass, P, Field?125.f:bNear ? 200.f : (bSlope ? 400.f : 1600.f), 0.f, B.GrassUV, false);
        }
        HouseWoods::Terrain(G);
    }

    void Streets()
    {
        const float E = 32000.f;
        const FName Asphalt=HouseSurroundings::Enabled()?FName(TEXT("EX_StreetAsphalt")):B.Asphalt;
        const FName Concrete=HouseSurroundings::Enabled()?FName(TEXT("EX_StreetConcrete")):B.Concrete;
        // Fraternity Park Dr (flat, in front) and Pat Head Summitt St (runs down the slope at the +x end).
        ExPatch(G, Asphalt, { -E, 8700, StreetY(-4350), StreetY(-2950) }, 4000.f, -12.f, B.AsphaltUV, false);
        const bool bPlan = HouseCampusLayout::Enabled();
        const float SideY0 = bPlan ? HouseCampusLayout::JunctionSouthY : -E;
        ExPatchFrozenCut(G, Asphalt, { 8700, 10300, SideY0, E }, 400.f, -12.f, B.AsphaltUV, true, Concrete);
        // With the street scene's decals loaded, its worn double yellow and parking lines replace these east of MarkingsStartX.
        const bool bDecalLines = HouseStreetScene::DecalsReady(B.Look);
        const float LinesEnd = bDecalLines ? HouseStreetScene::MarkingsStartX : 8400.f;
        for (float X = -E + 400; X + (bDecalLines ? 300.f : 0.f) < LinesEnd; X += 800) Mark(B.Yellow, X, X + 300, StreetY(-3658), StreetY(-3642), GF - 11.5f);
        for (float Y = -E + 400; Y < E; Y += 800) { const float Z = HouseExteriorSite::GroundHeight(9500, Y + 150) - 11.5f; if ((Y + 300 > StreetY(-4350) && Y < StreetY(-2950)) || (Y > -400 && Y < 2000) || Y < SideY0) continue; Mark(B.Yellow, 9492, 9508, Y, Y + 300, Z); }
        const float EdgeEnd = bDecalLines ? LinesEnd : 8600.f;
        Mark(B.Line, -E, EdgeEnd, StreetY(-2985), StreetY(-2975), GF - 11.5f); Mark(B.Line, -E, EdgeEnd, StreetY(-4325), StreetY(-4315), GF - 11.5f);
        // Sidewalks with curbs (gaps at the drives).
        auto Walk = [&](float X0, float X1, float Y0, float Y1) { Slab(Concrete, X0, X1, Y0, Y1, GF - 40, GF + 3, B.ConcreteUV); };
        Walk(-E, -5800, StreetY(-2950), StreetY(-2700)); Walk(-5200, 3450, StreetY(-2950), StreetY(-2700)); Walk(4050, 8450, StreetY(-2950), StreetY(-2700));
        // Under the campus plan the far sidewalk and its kerb stop either side of the two drives that now meet the street
        // (the west road and the visitor lot); HouseCampusLayout ramps the sidewalk down to them.
        TArray<FVector2D> FarRuns = { FVector2D(-E, -5000), FVector2D(-4400, 8450) };
        TArray<FVector2D> KerbRuns = { FVector2D(-E, 8700) };
        TArray<FVector2D> FarGaps;
        HouseCampusLayout::FarSidewalkGaps(FarGaps);
        if (FarGaps.Num() > 0)
        {
            auto Split = [&FarGaps](float X0, float X1, TArray<FVector2D>& Into)
            {
                for (const FVector2D& Gap : FarGaps) { if (Gap.X > X0 && Gap.X < X1) { Into.Add(FVector2D(X0, Gap.X)); X0 = Gap.Y; } }
                if (X1 > X0) Into.Add(FVector2D(X0, X1));
            };
            FarRuns.Reset(); KerbRuns.Reset();
            Split(-E, 8450, FarRuns); Split(-E, 8700, KerbRuns);
        }
        for (const FVector2D& R : FarRuns) Walk(R.X, R.Y, StreetY(-4600), StreetY(-4350));
        if (!bPlan) ExPatch(G, Concrete, { 8450, 8700, -E, StreetY(-4600) }, 400.f, 3.f, B.ConcreteUV, true);
        ExPatch(G, Concrete, { 8450, 8700, StreetY(-2700), 2500 }, 400.f, 3.f, B.ConcreteUV, true);
        ExPatchFrozenCut(G, Concrete, { 8450, 8700, 3100, E }, 400.f, 3.f, B.ConcreteUV, true);
        ExPatchFrozenCut(G, Concrete, { 10300, 10550, bPlan ? StreetY(-4600) : -E, E }, 400.f, 3.f, B.ConcreteUV, true);
        for (const FVector2D& R : KerbRuns) Slab(Concrete, R.X, R.Y, StreetY(-4350), StreetY(-4330), GF - 40, GF + 3, B.ConcreteUV);
        Slab(Concrete, -E, 8700, StreetY(-2970), StreetY(-2950), GF - 40, GF + 3, B.ConcreteUV);
        // Drives and lots.
        Slab(Asphalt, 3450, 4050, StreetY(-2990), -620, GF - 40, GF + 2, B.AsphaltUV);
        ExPatch(G, Asphalt, { 3450, 4050, -620, 1900 }, 200.f, 2.f, B.AsphaltUV, true);
        Slab(Asphalt, 3450, 6600, 1900, 4500, GR - 40, GR + 2, B.AsphaltUV);
        Slab(Asphalt, 6600, 8720, 2500, 3100, GR - 40, GR + 2, B.AsphaltUV);
        if(!HouseFraternityRow::Enabled())
        {
            Slab(Asphalt, -7200, -3700, -2500, -300, GF - 40, GF + 2, B.AsphaltUV);
            Slab(Asphalt, -5800, -5200, StreetY(-2990), -2500, GF - 40, GF + 2, B.AsphaltUV);
        }
        if (!bPlan) Slab(Asphalt, -9000, 0, StreetY(-7400), StreetY(-4600), GF - 40, GF + 2, B.AsphaltUV);   // the old health-centre lot
        if (!bPlan) Slab(Asphalt, -5000, -4400, StreetY(-4600), StreetY(-4330), GF - 40, GF + 2, B.AsphaltUV);   // the old lot's mouth (the plan's visitor drive replaces it)
        // The west lot receives its fitted bays, accessible aisle and driveway
        // opening from HouseParking, rather than the old lines across the entrance.
        if (!bPlan) for (float X = -8800; X < 0; X += 500) { Mark(B.Line, X - 5, X + 5, StreetY(-7350), StreetY(-6800), GF + 2.5f); Mark(B.Line, X - 5, X + 5, StreetY(-5250), StreetY(-4700), GF + 2.5f); }
        for (float X = 3600; X < 6600; X += 500) Mark(B.Line, X - 5, X + 5, 3950, 4450, GR + 2.5f);
        // The new concrete approach is aligned to the centered front stair.
        BuildHouseExteriorWalks(B.Walks, B.WalkDecor);
        // Keep the side-door walk and the courtyard routes.
        Slab(B.Concrete, 3340, 3450, HouseExteriorSite::FrontSidewalkInnerY, -1220, GF - 40, GF + 3, B.ConcreteUV);
        NoGo(-E, 8700, StreetY(-4700), StreetY(-2600)); NoGo(8300, 10700, -E, E); NoGo(3300, 4200, StreetY(-3000), 2000); NoGo(3300, 6700, 1800, 4600); NoGo(6500, 8800, 2400, 3200);
        NoGo(-7300, -3600, StreetY(-3000), -200); NoGo(-9100, 100, StreetY(-7500), StreetY(-4500)); NoGo(-1500, 1500, 1800, 3900); NoGo(-500, 420, HouseExteriorSite::FrontSidewalkInnerY-80, -1400); NoGo(3200, 3600, HouseExteriorSite::FrontSidewalkInnerY-80, -1100);
        // Keep the expanded front lawn open rather than filling the old road strip with random trees.
        NoGo(-3550, 3300, HouseExteriorSite::FrontSidewalkInnerY, -2550);
        NoGo(-3800, 4500, -2550, 2200);
    }

    void Courtyard()
    {
        // Photo-matched enclosed court. The old porch-to-court stair stays removed.
        BuildHouseBasketballCourt(G, B.CourtDecor, B.Look);
        BuildHouseRearCourtDetail(G,B.CourtDecor,B.Look);
        for (float X : { -994.f, 994.f })
        {
            G.Cylinder(B.Black, FVector(X, 3820, GR+340), FVector(X, 3820, GR+470), 3.5f, 8, B.PlainUV);
            FExBuilder::Box(B.CourtDecor, B.Grey, X-19, X+19, 3780, 3825, GR+461, GR+476, B.PlainUV);
            FExBuilder::Box(B.CourtDecor, B.Flood, X-16, X+16, 3783, 3818, GR+459, GR+461, B.PlainUV);
            B.LightSpecs.Add({ FVector(X, 3780, GR+449), FLinearColor(.92f, .90f, .82f), 20000.f, 3200.f, 6500.f });
        }
        // Small service lamps wash the condenser and the two grassy banks.
        for (float Side : {-1.f, 1.f})
        {
            FExBuilder::Box(B.CourtDecor, B.Grey, Side<0?-1530:1503, Side<0?-1503:1530, 2500, 2535, GR+315, GR+332, B.PlainUV);
            FExBuilder::Box(B.CourtDecor, B.Flood, Side<0?-1532:1530, Side<0?-1530:1532, 2504, 2531, GR+317, GR+330, B.PlainUV);
            B.LightSpecs.Add({ FVector(Side*1550, 2518, GR+323), FLinearColor(.85f, .90f, 1.f), 5000.f, 1500.f, 5500.f });
        }
        NoGo(-3500, 3500, 1750, 4320);
        // Dumpster at the back corner of the rear lot. Keep the bike rack off the new walking path.
        FExBuilder::Box(G, B.Green, 5900, 6300, 4150, 4450, GR + 12, GR + 150, B.PlainUV);
        FExBuilder::Box(G, B.Green, 5890, 6310, 4140, 4460, GR + 150, GR + 165, B.PlainUV);
        for (float WX : { 5940.f, 6260.f }) for (float WY : { 4180.f, 4420.f }) G.Cylinder(B.Rubber, FVector(WX, WY - 6, GR + 8), FVector(WX, WY + 6, GR + 8), 8, 8, B.PlainUV);
        FExBuilder::Box(B.Walks, TEXT("EX_WalkConcrete"), 4210, 4390, -2070, -1630, GF-25, GF+3, B.ConcreteUV);
        for (float RY = -2020; RY < -1720; RY += 70)
        {
            G.Cylinder(B.Metal, FVector(4235, RY, GF+3), FVector(4235, RY, GF+78), 2.5f, 6, B.PlainUV);
            G.Cylinder(B.Metal, FVector(4300, RY, GF+3), FVector(4300, RY, GF+78), 2.5f, 6, B.PlainUV);
            G.Cylinder(B.Metal, FVector(4235, RY, GF+78), FVector(4300, RY, GF+78), 2.5f, 6, B.PlainUV);
        }
        NoGo(1700, 2600, 2000, 3200); NoGo(5800, 6400, 4100, 4500);
    }

    void Neighbours()
    {
        const HouseCampus::FLayout Campus=HouseCampus::BuildCampus(B.Near,B.Far);
        if(HouseCampusLayout::Enabled())HouseCampusLayout::BuildGround(B.CampusGround,B.Grass,B.GrassUV);
        HouseCampusStreetDetail::Build(B.CampusGround,B.CampusFlat);
        HouseCampusLayout::BuildFallFloors(B.CampusFallFloor);
        for(const FBox2D& Foot:Campus.BuildingFootprints)NoGo(Foot.Min.X,Foot.Max.X,Foot.Min.Y,Foot.Max.Y);
        for(const FBox2D& Foot:HouseFraternityRow::Build(B.FraternityRow))NoGo(Foot.Min.X,Foot.Max.X,Foot.Min.Y,Foot.Max.Y);
        for(const FVector& Lamp:Campus.StadiumLamps)
            B.LightSpecs.Add({HouseCampusFall::StadiumLightAt(Lamp),FLinearColor(.85f,.9f,1.f),60000.f,HouseCampusFall::StadiumLightRadius(),22000.f});
        HouseCampus::Mountains(B.Far);
        for(float X:{-11200.f,-6100.f,7200.f,12800.f})NoGo(X-350,X+350,-27300,-5800);
        for(float Y:{-6000.f,-10300.f,-16100.f,-21100.f})NoGo(-18100,18300,Y-350,Y+350);
    }

    /** Lowest and highest ground (HouseExteriorSite::GroundHeight) under an axis-aligned rectangle. The relief lattice is a
     *  plane over each of its triangles, so its extremes lie on the rectangle's corners, on the lattice nodes inside it, or
     *  where its outline crosses a lattice line or a cell's (I,J)-(I+1,J+1) diagonal (X - Y = a whole number of cells);
     *  today's grade under it (the woods' hills, the west field's mound, the bank behind the house) is smooth, and a 50 cm
     *  grid over the rectangle holds that. */
    static void HedgeGroundRange(float X0, float X1, float Y0, float Y1, float& OutLowest, float& OutHighest)
    {
        OutLowest = FLT_MAX; OutHighest = -FLT_MAX;
        auto ProbeGround = [&OutLowest, &OutHighest](float PX, float PY)
        {
            const float Ground = HouseExteriorSite::GroundHeight(PX, PY);
            OutLowest = FMath::Min(OutLowest, Ground); OutHighest = FMath::Max(OutHighest, Ground);
        };
        const int32 NX = FMath::Max(1, FMath::CeilToInt((X1 - X0) / 50.f)), NY = FMath::Max(1, FMath::CeilToInt((Y1 - Y0) / 50.f));
        for (int32 J = 0; J <= NY; ++J) for (int32 I = 0; I <= NX; ++I) ProbeGround(X0 + (X1 - X0) * I / NX, Y0 + (Y1 - Y0) * J / NY);
        const float LatHalf = HouseCalderTerrain::Half, LatCell = HouseCalderTerrain::Cell;
        for (int32 I = FMath::CeilToInt((X0 + LatHalf) / LatCell); -LatHalf + I * LatCell <= X1; ++I)
        {
            const float LX = -LatHalf + I * LatCell;
            ProbeGround(LX, Y0); ProbeGround(LX, Y1);
            for (int32 J = FMath::CeilToInt((Y0 + LatHalf) / LatCell); -LatHalf + J * LatCell <= Y1; ++J) ProbeGround(LX, -LatHalf + J * LatCell);
        }
        for (int32 J = FMath::CeilToInt((Y0 + LatHalf) / LatCell); -LatHalf + J * LatCell <= Y1; ++J) { const float LY = -LatHalf + J * LatCell; ProbeGround(X0, LY); ProbeGround(X1, LY); }
        for (int32 K = FMath::CeilToInt((X0 - Y1) / LatCell); K * LatCell <= X1 - Y0; ++K)
        {
            const float Diag = K * LatCell;
            if (Diag + Y0 >= X0 && Diag + Y0 <= X1) ProbeGround(Diag + Y0, Y0);
            if (Diag + Y1 >= X0 && Diag + Y1 <= X1) ProbeGround(Diag + Y1, Y1);
            if (X0 - Diag >= Y0 && X0 - Diag <= Y1) ProbeGround(X0, X0 - Diag);
            if (X1 - Diag >= Y0 && X1 - Diag <= Y1) ProbeGround(X1, X1 - Diag);
        }
    }

    /** A hedge run on the Calder relief (SourceArt/CalderLandscape, "Edges and containment"). The run is cut where its
     *  centre line enters or leaves a frozen zone. A stretch inside one stays exactly as today: today's box there, standing
     *  on the grade under the run's middle. Every other stretch is split into even segments of 2 m or less, each from 20 cm
     *  below the lowest ground under it to 190 cm above the highest. Neighbours share their end face and each reaches the
     *  ground on it, so no gap opens under or between them; a segment's bottom face is always buried, so it is left out. */
    void HedgeOnRelief(float X0, float X1, float Y0, float Y1)
    {
        const bool bAlongX = X1 - X0 >= Y1 - Y0;
        const float RunStart = bAlongX ? X0 : Y0, RunEnd = bAlongX ? X1 : Y1, RunMid = bAlongX ? (Y0 + Y1) * .5f : (X0 + X1) * .5f;
        const float TodayZ = Grade((X0 + X1) * .5f, (Y0 + Y1) * .5f);   // what GroundHeight gives at the middle with no relief
        auto HedgeBox = [&](float P0, float P1, float Bottom, float Top, int32 SkipMask)
        {
            if (bAlongX) FExBuilder::Box(G, B.Hedge, P0, P1, Y0, Y1, Bottom, Top, B.PlainUV, SkipMask);
            else FExBuilder::Box(G, B.Hedge, X0, X1, P0, P1, Bottom, Top, B.PlainUV, SkipMask);
        };
        auto CentreFrozen = [&](float P) { return bAlongX ? HouseCalderTerrain::InFrozen(P, RunMid) : HouseCalderTerrain::InFrozen(RunMid, P); };
        // Stops: the run's ends and every frozen rectangle's edge across its centre line.
        TArray<float> Cuts;
        for (const FBox2D& Rect : HouseCalderTerrain::FrozenRects())
        {
            const double Across0 = bAlongX ? Rect.Min.Y : Rect.Min.X, Across1 = bAlongX ? Rect.Max.Y : Rect.Max.X;
            if (RunMid < Across0 || RunMid > Across1) continue;
            for (const double RectEdge : { bAlongX ? Rect.Min.X : Rect.Min.Y, bAlongX ? Rect.Max.X : Rect.Max.Y })
                if (RectEdge > RunStart && RectEdge < RunEnd) Cuts.Add(float(RectEdge));
        }
        Cuts.Sort();
        TArray<float> Stops = { RunStart };
        for (const float Cut : Cuts) if (Cut - Stops.Last() > 1.f && RunEnd - Cut > 1.f) Stops.Add(Cut);
        Stops.Add(RunEnd);
        for (int32 S = 0; S + 1 < Stops.Num();)
        {
            // One stretch: the stops between which the centre line stays inside (or outside) the frozen zones.
            const bool bFrozen = CentreFrozen((Stops[S] + Stops[S + 1]) * .5f);
            int32 E = S + 1;
            while (E + 1 < Stops.Num() && CentreFrozen((Stops[E] + Stops[E + 1]) * .5f) == bFrozen) ++E;
            const float From = Stops[S], To = Stops[E];
            S = E;
            if (bFrozen) { HedgeBox(From, To, TodayZ - 20, TodayZ + 190, 0); continue; }
            const int32 Count = FMath::Max(1, FMath::CeilToInt((To - From) / 200.f - .001f));
            for (int32 I = 0; I < Count; ++I)
            {
                const float P0 = From + (To - From) * I / Count, P1 = I + 1 == Count ? To : From + (To - From) * (I + 1) / Count;
                float Lowest = 0.f, Highest = 0.f;
                if (bAlongX) HedgeGroundRange(P0, P1, Y0, Y1, Lowest, Highest); else HedgeGroundRange(X0, X1, P0, P1, Lowest, Highest);
                HedgeBox(P0, P1, Lowest - 20, Highest + 190, 1 << 5);   // no bottom (-Z) face
            }
        }
    }

    void Boundary()
    {
        // Hedges just inside the playable edge, gaps where the streets run through.
        const float Eg = 18800.f;
        auto HedgeRun = [&](float X0, float X1, float Y0, float Y1)
        {
            // On the Calder relief the run follows the ground (HedgeOnRelief). Where the relief under it and 2 m round it is
            // flat (all of today, and -NoCalderTerrain) it is one flat box on the ground at the run's middle, as before.
            if (!HouseCalderTerrain::ReliefFlat(FBox2D(FVector2D(X0, Y0), FVector2D(X1, Y1)).ExpandBy(200.f))) { HedgeOnRelief(X0, X1, Y0, Y1); return; }
            const float Z = HouseExteriorSite::GroundHeight((X0 + X1) * .5f, (Y0 + Y1) * .5f);
            FExBuilder::Box(G, B.Hedge, X0, X1, Y0, Y1, Z - 20, Z + 190, B.PlainUV);
        };
        const bool bPlan = HouseCampusLayout::Enabled();
        if (!bPlan) { HedgeRun(-Eg, 8650, -Eg - 60, -Eg + 60); HedgeRun(10350, Eg, -Eg - 60, -Eg + 60); }
        HedgeRun(-Eg, 8650, Eg - 60, Eg + 60); HedgeRun(10350, Eg, Eg - 60, Eg + 60);
        if (!bPlan) HedgeRun(-Eg - 60, -Eg + 60, -Eg, StreetY(-4400));
        HedgeRun(-Eg - 60, -Eg + 60, StreetY(-2900), Eg);
        HedgeRun(Eg - 60, Eg + 60, bPlan ? StreetY(-4350) : -Eg, Eg);
        if (bPlan)
        {
            // The campus plan's edge: a hedgerow across the south and down both sides, short of the end of the ground.
            const float Edge = 31700.f, South = -29800.f;
            // On the Calder terrain the 20 m in front of Founders' Overlook (X -12000..-10000) are left open: its 1.07 m
            // parapet and the bluff's rock band below take over there (landform.py, edge closure).
            // On the Calder terrain the hedge also stops at the glen's headwall (X 26000..28580: its wing walls and grate close the
            // glen there): every gap the landform declares on this hedge (HouseCalderLandform::HedgeGaps(), along X).
            if (HouseCalderTerrain::Enabled() && !HouseCalderTerrain::ReliefFlat(FBox2D(FVector2D(-12000.f, South - 60.f), FVector2D(-10000.f, South + 60.f))))
            {
                const int32 CampusSouth = HouseCalderLandform::HedgeNames().IndexOfByKey(FString(TEXT("campus_south")));
                TArray<FVector2D> Gaps;
                for (const HouseCalderLandform::FHedgeGap& Gap : HouseCalderLandform::HedgeGaps()) if (Gap.Hedge == CampusSouth) Gaps.Add(FVector2D(Gap.From, Gap.To));
                Gaps.Sort([](const FVector2D& P, const FVector2D& Q) { return P.X < Q.X; });
                float RunFrom = -Edge;
                for (const FVector2D& Gap : Gaps)
                {
                    if (float(Gap.X) > RunFrom) HedgeRun(RunFrom, float(Gap.X), South - 60, South + 60);
                    RunFrom = FMath::Max(RunFrom, float(Gap.Y));
                }
                if (RunFrom < Edge) HedgeRun(RunFrom, Edge, South - 60, South + 60);
            }
            else HedgeRun(-Edge, Edge, South - 60, South + 60);
            HedgeRun(-Edge - 60, -Edge + 60, South, StreetY(-4600)); HedgeRun(Edge - 60, Edge + 60, South, StreetY(-4600));
        }
    }

    bool NearView(float X, float Y) const
    {
        for (const FExView& V : ExViews) if (!FString(V.Name).StartsWith(TEXT("nature_")) && !FString(V.Name).StartsWith(TEXT("detail_")) && FMath::Abs(V.Pos.X - X) < 900.f && FMath::Abs(V.Pos.Y - Y) < 900.f) return true;
        return false;
    }
    void Plants()
    {
        // Foundation shrubs along the front faces, trees on a jittered grid wherever nothing else is (never on a camera spot).
        for (const FExView& V : ExViews) if (!FString(V.Name).StartsWith(TEXT("nature_")) && !FString(V.Name).StartsWith(TEXT("detail_"))) NoGo(V.Pos.X - 800, V.Pos.X + 800, V.Pos.Y - 800, V.Pos.Y + 800);
        auto Shrubs = [&](float X0, float X1, float Y, float Step)
        {
            for (float X = X0; X <= X1; X += Step) Bush.Add(FTransform(FRotator(0, B.Rng.FRand() * 360, 0), FVector(X + FExBuilder::R(B.Rng, -15, 15), Y, Grade(X, Y)), FVector(FExBuilder::R(B.Rng, .8f, 1.2f), FExBuilder::R(B.Rng, .8f, 1.2f), FExBuilder::R(B.Rng, .8f, 1.15f))));
        };
        Shrubs(-1300, -570, -2390, 160); Shrubs(500, 1240, -2390, 160);
        Shrubs(-3300, -2650, -2550, 160); Shrubs(-2150, -1550, -2550, 160);
        Shrubs(1480, 3250, -2480, 160);
        for (float Y = -1900; Y <= 1300; Y += 180) Bush.Add(FTransform(FRotator(0, B.Rng.FRand() * 360, 0), FVector(-3660, Y, Grade(-3660, Y))));
        for (float Y = -1360; Y <= -100; Y += 180) Bush.Add(FTransform(FRotator(0, B.Rng.FRand() * 360, 0), FVector(4260, Y, Grade(4260, Y))));
        auto Tree = [&](float X, float Y, bool bBig)
        {
            if (NearView(X, Y)) return;
            const float S = FExBuilder::R(B.Rng, .85f, 1.25f);
            (bBig ? TreeA : TreeB).Add(FTransform(FRotator(0, B.Rng.FRand() * 360, 0), FVector(X, Y, Grade(X, Y) - 5), FVector(S, S, S * FExBuilder::R(B.Rng, .9f, 1.1f))));
        };
        Tree(-3000, -2600, true); Tree(2200, -2650, true);
        Tree(-4200, 1200, true); Tree(4300, 800, true); Tree(-3170, 4450, false); Tree(3200, 4450, true); Tree(-4800, -2600, false);
        for (float X = -18000; X < 8000; X += 2400)
        {
            if (FMath::Abs(X + 5500) < 800 || FMath::Abs(X - 3750) < 800 || FMath::Abs(X) < 1500) continue;
            Tree(X + FExBuilder::R(B.Rng, -200, 200), StreetY(-2600), false);
            Tree(X + 1200 + FExBuilder::R(B.Rng, -200, 200), StreetY(-4700), false);
        }
        for (float X = -27000; X < 27000; X += 1900)
            for (float Y = -27000; Y < 27000; Y += 1900)
            {
                const float PX = X + FExBuilder::R(B.Rng, -600, 600), PY = Y + FExBuilder::R(B.Rng, -600, 600);
                const float Dist = FMath::Max(FMath::Abs(PX), FMath::Abs(PY));
                const float Chance = Dist > 19500.f ? .42f : .16f;
                if (B.Rng.FRand() > Chance) continue;
                if (Dist <= 19500.f && !Free(PX, PY)) continue;
                const bool bBig = B.Rng.FRand() < .5f;
                // The jam field is kept clear of trees; a tree there still draws its three numbers so nothing else moves.
                if (!NearView(PX, PY) && HouseFallJam::Inside(PX, PY)) { B.Rng.FRand(); B.Rng.FRand(); B.Rng.FRand(); continue; }
                Tree(PX, PY, bBig);
            }
        HouseCampus::RearTrees(TreeA,TreeB,ForestDistant);
        HouseWoods::FitTrees(TreeA,TreeB,ForestDistant);
    }

    void Lamps()
    {
        for (float X = -15000; X <= 6000; X += 3000) Pole(X + 1500, StreetY(-2640), -90);
        for (float X = -13500; X <= 7500; X += 3000) Pole(X, StreetY(-4660), 90);
        for (float Y = -15000; Y <= 15000; Y += 3000) { if (Y > StreetY(-4600) && Y < StreetY(-2500)) continue; Pole(8420, Y, 0); }
        Pole(-7160, -2600, 0, true, false); Pole(-3740, -2530, 180, true, false); Pole(5000, 3200, 180, false, false);
        if (HouseCampusLayout::Enabled())
        {
            // The three health-lot poles (and so their three lights) stand in the campus plan's lots instead; the count stays 53.
            TArray<FVector> Moved; HouseCampusLayout::LotPoles(Moved);
            for (const FVector& P : Moved) Pole(P.X, P.Y, P.Z, false, false);
        }
        else
        {
            Pole(-7000, StreetY(-5500), 180, false, false);
            Pole(-2500, StreetY(-6500), 180, false, false); Pole(-6500, StreetY(-7400), 0, false, false);
        }
    }

    void Vehicles()
    {
        int32 K = 0;
        auto Row = [&](float X0, float X1, float Step, float Y, float Yaw, float Skip)
        {
            for (float X = X0; X <= X1; X += Step) { if (B.Rng.FRand() < Skip) continue; Car(K++ % 3, X, Y, Yaw + FExBuilder::R(B.Rng, -2, 2)); }
        };
        Row(-9000, 6000, 1300, StreetY(-4150), 180, .45f);     // kerb parking across the street (facing -x: right-hand traffic)
        Row(-6750, -3950, 500, -2150, 90, .3f);                 // front lot, two rows
        Row(-6750, -3950, 500, -650, -90, .3f);
        Row(3800, 6300, 500, 4100, 90, .4f);                     // rear lot
        Row(-8550, -250, 500, StreetY(-7050), 90, .35f);        // health-centre lot
        Row(-8550, -250, 500, StreetY(-4950), -90, .35f);
    }

    void BuildAll()
    {
        Terrain(); Streets(); Courtyard(); Neighbours(); Boundary(); Lamps(); Vehicles(); Plants();
    }
};

// ------------------------------------------------------------------ instanced meshes (local space, origin on the ground)
void ExChairMesh(FExBuilder& B, FHallMesh& M, TArray<FBox>& Col)
{
    FExBuilder::Box(M, B.White, -42, 42, -40, 40, 44, 48, B.PlainUV);
    FExBuilder::Box(M, B.White, -42, 42, 36, 44, 48, 92, B.PlainUV);
    for (float LX : { -38.f, 38.f }) for (float LY : { -36.f, 36.f }) FExBuilder::Box(M, B.Metal, LX - 2, LX + 2, LY - 2, LY + 2, 0, 44, B.PlainUV);
    Col.Add(FBox(FVector(-42, -40, 0), FVector(42, 44, 92)));
}
} // namespace

// ------------------------------------------------------------------ actor
AHouseExteriorDetail::AHouseExteriorDetail()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ExteriorRoot"));
    RootComponent->SetMobility(EComponentMobility::Static);
    bReplicates = false;
    SetCanBeDamaged(true);
    Tags.Add(TEXT("HouseTree"));
}

bool AHouseExteriorDetail::ReactToTreeHit(UPrimitiveComponent* Component,const FHitResult& Hit,const FVector& Direction)
{
    if(!GetWorld()||GetNetMode()==NM_Client||!TreeReaction)return false;
    for(const FHouseTreeBatch& Batch:TreeBatches)
    {
        auto* H=Batch.Component.Get();if(!H||H!=Component)continue;
        FTransform Tree;
        if(Hit.Item>=0&&H->GetInstanceTransform(Hit.Item,Tree,true))
            return TreeReaction->HitTree(Tree,Batch.Kind,Batch.Variant,Direction);
        // Chaos contacts sometimes omit the instance index. Resolve only a trunk
        // beside the actual impact, never a neighbouring tree across the yard.
        float Nearest=FMath::Square(135.f);int32 Index=INDEX_NONE;
        for(int32 K=0;K<H->GetInstanceCount();++K)
        {
            FTransform T;H->GetInstanceTransform(K,T,true);
            const float D=FVector::DistSquared2D(T.GetLocation(),Hit.ImpactPoint);
            if(D<Nearest){Nearest=D;Index=K;Tree=T;}
        }
        return Index!=INDEX_NONE&&TreeReaction->HitTree(Tree,Batch.Kind,Batch.Variant,Direction);
    }
    return false;
}

float AHouseExteriorDetail::TakeDamage(float Damage,const FDamageEvent& Event,AController* Instigator,AActor* Causer)
{
    if(Damage<=0||!Event.IsOfType(FPointDamageEvent::ClassID))return 0.f;
    const FPointDamageEvent& Point=static_cast<const FPointDamageEvent&>(Event);
    return ReactToTreeHit(Point.HitInfo.GetComponent(),Point.HitInfo,Point.ShotDirection)?Damage:0.f;
}

void AHouseExteriorDetail::OnTreeHit(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit)
{
    if(!Other||Other==this||Cast<APawn>(Other)||!OtherComponent||!OtherComponent->IsSimulatingPhysics())return;
    if(Impulse.Size()<6000.f&&OtherComponent->GetComponentVelocity().Size()<180.f)return;
    ReactToTreeHit(Component,Hit,OtherComponent->GetComponentVelocity().GetSafeNormal());
}

void AHouseExteriorDetail::OnConnectedWindowBroken(AHouseReactiveFixture* Window)
{
    if (!Window) return;
    if(Window->FixtureId==TEXT("UpperLandingSashWindow") && LandingGuard.IsValid() && GuardFallAge<0)
    {
        auto* G=LandingGuard.Get();G->SetMobility(EComponentMobility::Movable);
        GuardStart=G->GetComponentTransform();G->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(WindowGuardDrop),false,G->GetOwner());
        const FVector C=G->Bounds.Origin;
        GuardDrop=GetWorld()->LineTraceSingleByChannel(Floor,C-FVector(0,0,10),C-FVector(0,0,220),ECC_Pawn,Q)
            ?FMath::Max(0.f,float(G->Bounds.GetBox().Min.Z-Floor.ImpactPoint.Z-1.f)):110.f;
        GuardFallAge=0;SetActorTickEnabled(true);
    }
    if (!HasAuthority()) return;
    const FBox A=Window->GetFixtureMesh()->Bounds.GetBox();
    const int Axis=A.GetSize().X<A.GetSize().Y?0:1,Along=1-Axis;
    // Some imported sashes have two glass layers. A single shatter opens the
    // complete sash; both fixtures use their existing replicated break state.
    for (auto Weak:ConnectedWindows) if (auto* Other=Weak.Get())
    {
        if (Other==Window || Other->IsBroken()) continue;
        const FBox B=Other->GetFixtureMesh()->Bounds.GetBox();
        if ((B.GetSize().X<B.GetSize().Y?0:1)!=Axis || FMath::Abs(A.GetCenter()[Axis]-B.GetCenter()[Axis])>85.f) continue;
        const float OverlapU=FMath::Min(A.Max[Along],B.Max[Along])-FMath::Max(A.Min[Along],B.Min[Along]);
        const float OverlapZ=FMath::Min(A.Max.Z,B.Max.Z)-FMath::Max(A.Min.Z,B.Min.Z);
        if (OverlapU<FMath::Min(A.GetSize()[Along],B.GetSize()[Along])*.45f
            || OverlapZ<FMath::Min(A.GetSize().Z,B.GetSize().Z)*.6f) continue;
        Other->ApplyImpact(B.GetCenter(),FVector::ForwardVector,Other->GlassBreakingDamage+1.f);
    }
}

void AHouseExteriorDetail::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!LandingGuard.IsValid() || GuardFallAge<0){SetActorTickEnabled(false);return;}
    GuardFallAge+=DeltaSeconds;
    FTransform T=GuardStart;const float Drop=FMath::Min(GuardDrop,490.f*GuardFallAge*GuardFallAge);
    T.AddToTranslation(FVector(0,0,-Drop));
    const float Twist=12.f*FMath::Clamp(Drop/FMath::Max(GuardDrop,1.f),0.f,1.f);
    T.SetRotation(FQuat(FVector::UpVector,FMath::DegreesToRadians(Twist))*GuardStart.GetRotation());
    LandingGuard->SetWorldTransform(T,false,nullptr,ETeleportType::TeleportPhysics);
    if(Drop>=GuardDrop){SetActorTickEnabled(false);}
}

void AHouseExteriorDetail::Build(UHouseBlackHallSubsystem* Look)
{
    UWorld* World = GetWorld();
    if (!World || !Look) return;
    const double Start = FPlatformTime::Seconds();
    HouseCalderFootPoints::Reset();   // the Calder placers register their foot points from here (-CalderTerrainReview checks them)
    // These two rendered room tiles have exact replacement colliders saved in
    // the map. Disable the old actor hull as well as its component: the static
    // mesh's default collision profile can restore a component-only override
    // when it registers during game loading.
    TSet<FString> ConnectedCollisionMeshes;
    for(TActorIterator<AActor> It(World);It;++It)
    {
        TInlineComponentArray<UStaticMeshComponent*> Cs(*It);
        for(auto* C:Cs)if(C->GetStaticMesh())ConnectedCollisionMeshes.Add(C->GetStaticMesh()->GetName());
    }
    for(TActorIterator<AActor> It(World);It;++It)
    {
        TInlineComponentArray<UStaticMeshComponent*> Cs(*It);
        for(auto* C:Cs)if(C->GetStaticMesh())
        {
            const FString Name=C->GetStaticMesh()->GetName();
            const bool Replaced=((Name==TEXT("SM_Upper_p0_m1_Solid") || Name==TEXT("SM_Connected_Upper_p0_m1_Solid_SashCleared")) && ConnectedCollisionMeshes.Contains(TEXT("COL_Connected_pper_p0_m1_Solid")))
                || ((Name==TEXT("SM_Upper_p0_p0_CornerCleared_NoodleCut") || Name==TEXT("SM_Connected_Upper_p0_p0_Solid_SashCleared")) && ConnectedCollisionMeshes.Contains(TEXT("COL_Connected_pper_p0_p0_Solid")));
            if(Replaced){It->SetActorEnableCollision(false);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);}
        }
    }
    ExCreateLook(Look);
    HouseBrick::CreateLook(Look);
    HouseCars::CreateLook(Look);
    ExCreateCarLook(Look);
    HouseNature::CreateLook(Look);
    HouseCampus::CreateLook(Look);
    HouseFraternityRow::CreateLook(Look);
    HouseFraternityRowYards::CreateLook(Look);
    HouseSurroundings::CreateLook(Look);
    HouseCampusLayout::CreateLook(Look);
    HouseCampusStreetDetail::CreateLook(Look);
    HouseCampusInteriors::CreateLook(Look);
    HouseWestField::CreateLook(Look);
    HouseParking::CreateLook(Look);
    HouseLotDetail::CreateLook(Look);
    HouseStreetScene::CreateLook(Look);   // after HouseSurroundings: replaces the street asphalt tile
    HouseSideCanopy::CreateLook(Look);
    HouseFallJam::CreateLook(Look);
    HouseCampusFall::CreateLook(Look);
    HouseWoods::CreateLook(Look);
    if (HouseWoodsPass::Stage() >= 1) HouseWoodsFloor::CreateLook(Look);
    CreateHouseExteriorWalkLook(Look);
    FExBuilder B; B.World = World; B.Look = Look; B.Owner = this;
    B.BuildHouse();
    B.ClearOpeningSurfaces();
    for(TActorIterator<AActor> It(World);It;++It)
    {
        TInlineComponentArray<UStaticMeshComponent*> Cs(*It);
        for(auto* C:Cs)if(C->GetStaticMesh() && C->GetStaticMesh()->GetName()==TEXT("SM_HFP_LandingGuard"))LandingGuard=C;
    }
    for (TActorIterator<AHouseReactiveFixture> It(World);It;++It)
        if (It->FixtureKind==EHouseReactiveFixtureKind::Window && !It->GetOwner())
        {
            ConnectedWindows.Add(*It);
            It->OnFixtureBroken.AddUObject(this,&AHouseExteriorDetail::OnConnectedWindowBroken);
        }
    // A joining player can receive a broken pane before this local exterior
    // finishes building. Apply the same guard state to that initial snapshot.
    for(auto Weak:ConnectedWindows)if(auto* G=Weak.Get())if(G->IsBroken())OnConnectedWindowBroken(G);
    FExSite Site(B);
    Site.BuildAll();
    HouseWoodsPass::RecordOriginalTrees(Site.TreeA, Site.TreeB, Site.ForestDistant);
    HouseCalderTerrain::LogSummary();
    // Calder terrain: the trees were planted (and hashed just above, WOODS_PASS_ORIGINAL_TREES) on today's grade. Each now
    // stands on the lowest relief under its trunk (+-60 cm), so no root hangs over a slope. Z only, before anything copies
    // them; the relief is 0 in the frozen zones, everywhere today and with -NoCalderTerrain. A tree standing in a frozen zone
    // is left exactly as planted (its trunk box may reach over the zone's edge onto the relief beyond).
    for (TArray<FTransform>* PlantedSet : { &Site.TreeA, &Site.TreeB, &Site.ForestDistant })
        for (FTransform& PlantedXf : *PlantedSet)
        {
            const FVector PlantedAt = PlantedXf.GetLocation();
            if (HouseCalderTerrain::InFrozen(PlantedAt.X, PlantedAt.Y)) continue;
            const FBox2D PlantedTrunk(FVector2D(PlantedAt.X - 60.f, PlantedAt.Y - 60.f), FVector2D(PlantedAt.X + 60.f, PlantedAt.Y + 60.f));
            PlantedXf.SetLocation(FVector(PlantedAt.X, PlantedAt.Y, PlantedAt.Z + HouseCalderTerrain::ReliefMin(PlantedTrunk)));
        }
    TArray<FTransform> OriginalWoodlandTrees = Site.TreeA;
    OriginalWoodlandTrees.Append(Site.TreeB);
    OriginalWoodlandTrees.Append(Site.ForestDistant);
    HouseSurroundings::Street(B.StreetDecor);
    HouseWestField::Fence(B.FieldFence,B.FieldFenceDetail);
    if(!HouseFraternityRow::Enabled())HouseParking::Build(B.ParkingSolid,B.ParkingDetail);
    FHallMesh LotWear, LotProps;   // stains, cracks, puddles, leaves, painted numbers; pier bases, bollards, signs, dumpster
    if(!HouseFraternityRow::Enabled())HouseLotDetail::Build(LotWear, LotProps);
    HouseParking::ParkCars(Site.CarsA,Site.CarsB,Site.CarsC);
    HouseCampusLayout::RemapLotCars(Site.CarsA,Site.CarsB,Site.CarsC);   // the old health-lot cars park in the plan's lots
    TArray<FTransform> ParkingPlants;HouseParking::Landscaping(Site.Bush,ParkingPlants);
    FHallMesh RowSolid,RowDetail,RowGlow,YardSolid,YardDecor,YardGlow;
    if(HouseFraternityRow::Enabled())
    {
        // Keep all base generation/vehicle RNG draws and original NoGo exclusions. Trees were recorded above.
        auto InOldLot=[](const FTransform& T){const FVector P=T.GetLocation();return P.X>HouseParking::West&&P.X<HouseParking::East&&P.Y>HouseParking::South&&P.Y<HouseParking::North;};
        Site.CarsA.RemoveAll(InOldLot);Site.CarsB.RemoveAll(InOldLot);Site.CarsC.RemoveAll(InOldLot);
        ParkingPlants.Reset(); // Existing-bush clearance filtering in Landscaping() is intentionally retained.
        for(const FVector P:{FVector(-7160,-2600,0),FVector(-3740,-2530,180)})
        {
            Site.LotPoles.RemoveAll([&](const FTransform& T){return FVector::DistSquared2D(T.GetLocation(),FVector(P.X,P.Y,0))<1;});
            const FVector Head=FVector(P.X,P.Y,HouseExteriorSite::GroundHeight(P.X,P.Y))+FRotator(0,P.Z,0).RotateVector(HouseStreetScene::LensOffset());
            B.LightSpecs.RemoveAll([&](const FExBuilder::FLightSpec& L){return FVector::DistSquared(L.Pos,Head)<1;});
        }
        HouseFraternityRow::BuildStructures(RowSolid,RowDetail,RowGlow);
        HouseFraternityRowYards::Build(YardSolid,YardDecor,YardGlow);
        // Retain the exact pedestrian connector independently of the removed parking builder.
        RowSolid.BoxMinMax(TEXT("EX_StreetConcrete"),{-4490,-285,-85},{-3550,-105,-57},FHallUV::World(180));
        if(HouseFraternityRow::LightsEnabled())for(int32 I=2;I<5;++I)
        {
            const auto& H=HouseFraternityRow::Houses()[I];
            const FVector Porch=HouseFraternityRow::PorchLight(H);
            const FVector Yard=HouseFraternityRow::YardLight(H);
            B.LightSpecs.Add({Porch,FLinearColor(1.f,.65f,.32f),650.f,700.f,5500.f});
            B.LightSpecs.Add({Yard,FLinearColor(1.f,.72f,.43f),500.f,800.f,5500.f});
        }
    }

    HouseWoods::Hut(B.WoodsHut);
    B.LightSpecs.Add({FVector(HouseWoods::HutX,HouseWoods::HutY-200,HouseWoods::HutFloor+231),FLinearColor(1.f,.69f,.34f),1200.f,1000.f,6500.f});
    // New focal trees stand on open lawn, clear of the concrete approach and
    // side paths. Separate transforms leave the existing site RNG untouched.
    const TArray<FTransform> GardenOaks={
        FTransform(FRotator(0,29,0),FVector(-2350,-3250,GF-5),FVector(.98f)),
        FTransform(FRotator(0,157,0),FVector(2600,-3350,GF-5),FVector(1.12f))};
    const TArray<FTransform> GardenBirches={
        FTransform(FRotator(0,71,0),FVector(-1450,-3500,GF-5),FVector(.85f)),
        FTransform(FRotator(0,218,0),FVector(1700,-3520,GF-5),FVector(.80f))};
    if(World->GetNetMode()!=NM_Client)
    {
        FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;
        P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        TreeReaction=World->SpawnActor<AHouseTreeReaction>(AHouseTreeReaction::StaticClass(),FTransform::Identity,P);
    }
    // ---------------------------------------------------------------- components
    auto Make = [&](const FHallMesh& M, const TCHAR* Name, bool bCollision, bool bShadow)
    {
        if (M.NumTriangles() == 0) return;
        UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, Name);
        PM->SetMobility(EComponentMobility::Static);
        PM->SetCastShadow(bShadow);
        if(FString(Name).StartsWith(TEXT("FraternityRowDetail"))||FString(Name).StartsWith(TEXT("FraternityRowGlow"))||FString(Name).StartsWith(TEXT("FraternityRowYards"))||FString(Name).StartsWith(TEXT("ExteriorFar"))||FString(Name).StartsWith(TEXT("ExteriorNear"))||FString(Name).StartsWith(TEXT("ExteriorFraternityRow"))||FString(Name).StartsWith(TEXT("ExteriorStreetDecor"))||FString(Name).StartsWith(TEXT("ExteriorWestFieldFence"))||FString(Name).StartsWith(TEXT("ExteriorParkingLot"))
            ||FString(Name).StartsWith(TEXT("ExteriorStreetWear"))||FString(Name).StartsWith(TEXT("ExteriorStreetClean"))||FString(Name).StartsWith(TEXT("ExteriorStreetWires"))||FString(Name).StartsWith(TEXT("ExteriorPartyLitter"))
            ||FString(Name).StartsWith(TEXT("ExteriorAtticLiner"))||FString(Name).StartsWith(TEXT("ExteriorFallJamLitter"))||FString(Name).StartsWith(TEXT("ExteriorFallJamGlow"))){PM->bAffectDynamicIndirectLighting=false;PM->bAffectDistanceFieldLighting=false;}
        // Leaves, iron covers, cups and wires sit on top of the road decals rather than taking them.
        if(FString(Name).StartsWith(TEXT("ExteriorStreetClean"))||FString(Name).StartsWith(TEXT("ExteriorStreetWires"))||FString(Name).StartsWith(TEXT("ExteriorPartyLitter")))PM->SetReceivesDecals(false);
        PM->bUseComplexAsSimpleCollision = true;
        PM->bUseAsyncCooking = false;
        if (bCollision) { PM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName); PM->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); }
        else PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PM->RegisterComponent();
        M.Build(PM, [Look](FName Key) { return Look->Mat(Key); }, bCollision);
        Meshes.Add(PM);
        Triangles += M.NumTriangles();
    };
    auto Local=[&](const FHallMesh& M,const TCHAR* Name,bool Collision,bool Shadow)
    {
        auto Tiles=HouseSpatialMeshes(M,FString(Name).StartsWith(TEXT("ExteriorNear"))||FString(Name).StartsWith(TEXT("CampusLayout"))?4000.f:1600.f);
        if(FString(Name)==TEXT("FraternityRowDetail")||FString(Name)==TEXT("FraternityRowYardsDecor"))
        {
            // Regroup the already clipped1600cm tiles; never reclip/recompute their vertex attributes at3200cm.
            struct FTotals {int32 Components=0,Sections=0,Vertices=0,Triangles=0;};
            auto Totals=[](const TArray<FHallMesh>& Parts,FTotals& Out)
            {
                Out.Components=Parts.Num();bool Valid=true;
                for(const FHallMesh& Part:Parts)
                {
                    Valid&=Part.Order.Num()==Part.Sections.Num();
                    for(FName Key:Part.Order)
                    {
                        const FHallSection* S=Part.Sections.Find(Key);if(!S){Valid=false;continue;}
                        Valid&=S->V.Num()>0&&S->I.Num()>0&&S->I.Num()%3==0&&S->N.Num()==S->V.Num()&&S->UV.Num()==S->V.Num()&&S->T.Num()==S->V.Num();
                        ++Out.Sections;Out.Vertices+=S->V.Num();Out.Triangles+=S->I.Num()/3;
                        for(int32 Index:S->I)Valid&=Index>=0&&Index<S->V.Num();
                    }
                }
                return Valid;
            };
            FTotals Before,After;bool Valid=Totals(Tiles,Before)&&!Collision&&!Shadow;
            struct FSpan {const FHallSection* Source;int32 Group,Vertex,Index;FName Key;};
            TMap<FIntPoint,int32> Groups;TArray<FHallMesh> Coarse;TArray<FSpan> Spans;
            for(const FHallMesh& Tile:Tiles)
            {
                FBox Bounds(ForceInit);
                for(FName Key:Tile.Order)if(const FHallSection* S=Tile.Sections.Find(Key))for(const FVector& P:S->V)Bounds+=P;
                if(!Bounds.IsValid||Bounds.ContainsNaN()){Valid=false;continue;}
                // Floor handles negative coordinates; a boundary-only vertical plane belongs to its actual bounds centre.
                const FVector Centre=Bounds.GetCenter();const FIntPoint Cell(FMath::FloorToInt(Centre.X/3200.),FMath::FloorToInt(Centre.Y/3200.));
                const int32* Existing=Groups.Find(Cell);const int32 Group=Existing?*Existing:Coarse.AddDefaulted();
                if(!Existing)Groups.Add(Cell,Group);
                for(FName Key:Tile.Order)
                {
                    const FHallSection* S=Tile.Sections.Find(Key);if(!S)continue;
                    FHallSection& D=Coarse[Group].Sec(Key);const int32 Vertex=D.V.Num(),Index=D.I.Num();
                    Spans.Add({S,Group,Vertex,Index,Key});
                    D.V.Append(S->V);D.N.Append(S->N);D.UV.Append(S->UV);D.T.Append(S->T);
                    for(int32 I:S->I)D.I.Add(I+Vertex);
                }
            }
            Valid&=Totals(Coarse,After)&&After.Components<=Before.Components&&After.Sections<=Before.Sections
                &&After.Vertices==Before.Vertices&&After.Triangles==Before.Triangles;
            // Compare every copied scalar's bits, never struct padding. Offset-index equality preserves every triangle/winding.
            auto Same=[](const auto& A,const auto& B){return FMemory::Memcmp(&A,&B,sizeof(A))==0;};
            auto VectorSame=[&](const FVector& A,const FVector& B){return Same(A.X,B.X)&&Same(A.Y,B.Y)&&Same(A.Z,B.Z);};
            int32 VerifiedTriangles=0;
            for(const FSpan& Span:Spans)
            {
                const FHallSection& S=*Span.Source;const FHallSection& D=Coarse[Span.Group].Sections[Span.Key];
                if(D.V.Num()<Span.Vertex+S.V.Num()||D.N.Num()<Span.Vertex+S.N.Num()||D.UV.Num()<Span.Vertex+S.UV.Num()
                    ||D.T.Num()<Span.Vertex+S.T.Num()||D.I.Num()<Span.Index+S.I.Num()){Valid=false;continue;}
                if(S.N.Num()!=S.V.Num()||S.UV.Num()!=S.V.Num()||S.T.Num()!=S.V.Num()){Valid=false;continue;}
                for(int32 I=0;I<S.V.Num();++I)
                {
                    const int32 J=Span.Vertex+I;
                    Valid&=VectorSame(S.V[I],D.V[J])&&VectorSame(S.N[I],D.N[J])&&Same(S.UV[I].X,D.UV[J].X)&&Same(S.UV[I].Y,D.UV[J].Y)
                        &&VectorSame(S.T[I].TangentX,D.T[J].TangentX)&&S.T[I].bFlipTangentY==D.T[J].bFlipTangentY;
                }
                for(int32 I=0;I<S.I.Num();++I)Valid&=D.I[Span.Index+I]==S.I[I]+Span.Vertex;
                VerifiedTriangles+=S.I.Num()/3;
            }
            Valid&=VerifiedTriangles==Before.Triangles;
            UE_LOG(LogTemp,Display,TEXT("FRATERNITY_ROW_DECOR_BATCH %s name=%s fineCell=1600 coarseCell=3200 components=%d->%d materialSections=%d->%d triangles=%d->%d vertices=%d->%d verifiedTriangles=%d scalarBitsExact=%d indexOffsetsExact=%d"),
                Valid?TEXT("PASS"):TEXT("FAIL"),Name,Before.Components,After.Components,Before.Sections,After.Sections,
                Before.Triangles,After.Triangles,Before.Vertices,After.Vertices,VerifiedTriangles,int(Valid),int(Valid));
            if(!Valid)
            {
                UE_LOG(LogTemp,Error,TEXT("FRATERNITY_ROW_DECOR_BATCH NATIVE FAIL geometry/count consistency; refusing altered batch %s"),Name);
                FPlatformMisc::RequestExitWithStatus(false,1);return;
            }
            Tiles=MoveTemp(Coarse);
        }
        for(int i=0;i<Tiles.Num();++i)Make(Tiles[i],*FString::Printf(TEXT("%s_%02d"),Name,i),Collision,Shadow);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_LOCAL_EXTERIOR: %s tiles=%d shadows=%d"),Name,Tiles.Num(),int(Shadow));
    };
    Local(B.House, TEXT("ExteriorHouse"), true, true);
    Make(B.Attic, TEXT("ExteriorAtticLiner"), false, false);
    Make(B.Grounds, TEXT("ExteriorGrounds"), true, false);
    if (HouseCalderTerrain::Enabled()) HouseCalderTerrainTiles::Build(this, B.CalderRegions);
    HouseCalderFeatures::Build(this);   // retaining walls, rock faces, stairs and ramps (nothing unless the relief is real)
    HouseCalderWater::Build(this);   // Calder Branch, Gander Pond, Calder Falls and their crossings (nothing unless the relief is real)
    HouseCalderRoadside::Build(this);   // guardrails, rails, ditches, catch basins and culverts (nothing unless the relief is real)
    HouseCalderEdges::Build(this, Look, Site.ForestDistant);   // edge closures, the overlooks' furniture, Crow Ledge (nothing unless the relief is real)
    HouseCalderOuterLand::Build(this);   // the outer land beyond +-32000 (scenery, no collision; nothing with -NoCalderTerrain)
    Local(B.Near, TEXT("ExteriorNear"), true, false);
    Local(B.CampusGround, TEXT("CampusLayoutGround"), true, false);
    Local(B.CampusFlat, TEXT("CampusLayoutFlat"), false, false);
    {
        const int32 Before = Meshes.Num();
        Local(B.CampusFallFloor, TEXT("CampusLayoutFallFloor"), true, false);
        for (int32 I = Before; I < Meshes.Num(); ++I) if (Meshes[I]) Meshes[I]->SetVisibility(false);   // collision only
    }
    Make(B.Far, TEXT("ExteriorFar"), false, false);
    Make(B.CourtDecor, TEXT("ExteriorCourtDecor"), false, false);
    Local(B.Walks, TEXT("ExteriorWalks"), true, true);
    Make(B.WalkDecor, TEXT("ExteriorWalkDecor"), false, false);
    Make(B.BrickDecor, TEXT("ExteriorBrickDecor"), false, false);
    Make(B.WoodsHut,TEXT("ExteriorWoodsHut"),true,true);
    Local(B.FraternityRow,TEXT("ExteriorFraternityRow"),false,false);
    Local(RowSolid,TEXT("FraternityRowStructure"),true,true);
    Local(RowDetail,TEXT("FraternityRowDetail"),false,false);
    Make(RowGlow,TEXT("FraternityRowGlow"),false,false);
    Local(YardSolid,TEXT("FraternityRowYardsSolid"),true,false);
    Local(YardDecor,TEXT("FraternityRowYardsDecor"),false,false);
    Make(YardGlow,TEXT("FraternityRowYardsGlow"),false,false);
    if(HouseFraternityRow::Enabled())HouseFraternityRowYards::AttachInstances(this,Look);
    Local(B.StreetDecor,TEXT("ExteriorStreetDecor"),false,false);
    Make(B.FieldFence,TEXT("ExteriorWestFieldFence"),true,false);
    Make(B.FieldFenceDetail,TEXT("ExteriorWestFieldFenceDetail"),false,false);
    Make(B.ParkingSolid,TEXT("ExteriorParkingLotSolid"),true,false);
    Make(B.ParkingDetail,TEXT("ExteriorParkingLotDetail"),false,false);
    Make(LotWear,TEXT("ExteriorParkingLotWear"),false,false);
    Make(LotProps,TEXT("ExteriorLotProps"),true,true);
    // Fraternity Park Dr at dusk (HouseStreetScene): gutters, iron covers, leaves and litter on the paving, street furniture
    // and the payphone corner, the wires between the wooden poles, and the Halloween party with its banner.
    FHallMesh StreetWear, StreetClean, StreetProps, StreetWires, PartyProps, PartyLitter, PartyBanner;
    HouseStreetScene::BuildRoad(StreetWear, StreetClean, StreetProps);
    HouseStreetScene::BuildCorner(StreetProps);
    HouseStreetScene::BuildWires(StreetWires, Site.Poles);
    HouseStreetScene::BuildParty(PartyProps, PartyLitter);
    HouseStreetScene::BuildBanner(PartyBanner);
    Local(StreetWear, TEXT("ExteriorStreetWear"), false, false);
    Local(StreetClean, TEXT("ExteriorStreetClean"), false, false);
    Local(StreetProps, TEXT("ExteriorStreetProps"), true, true);
    Local(StreetWires, TEXT("ExteriorStreetWires"), false, false);
    Make(PartyProps, TEXT("ExteriorPartyProps"), true, true);
    Make(PartyLitter, TEXT("ExteriorPartyLitter"), false, false);
    Make(PartyBanner, TEXT("ExteriorPartyBanner"), false, true);
    // The morning after the Harvest Fall Jam on the east lawn (HouseFallJam): stage and backline, fences, bales, lights, litter.
    FHallMesh JamSolid, JamLitter, JamGlow;
    HouseFallJam::Build(JamSolid, JamLitter, JamGlow);
    Local(JamSolid, TEXT("ExteriorFallJam"), true, true);
    Make(JamLitter, TEXT("ExteriorFallJamLitter"), false, false);
    Make(JamGlow, TEXT("ExteriorFallJamGlow"), false, false);
    UE_LOG(LogTemp, Display, TEXT("HOUSE_FALL_JAM solid=%d litter=%d glow=%d"), JamSolid.NumTriangles(), JamLitter.NumTriangles(), JamGlow.NumTriangles());
    // Locally built on every machine with the shack; a single text component
    // keeps its name sharp without adding lights or blocking the doorway.
    auto* HutSign = NewObject<UTextRenderComponent>(this, TEXT("ExteriorWoodsHutSign"));
    HutSign->SetMobility(EComponentMobility::Static);
    HutSign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HutSign->SetCastShadow(false);
    HutSign->bAffectDynamicIndirectLighting = false;
    HutSign->bAffectDistanceFieldLighting = false;
    HutSign->SetText(FText::FromString(TEXT("Coles Quarters")));
    HutSign->SetHorizontalAlignment(EHTA_Center);
    HutSign->SetVerticalAlignment(EVRTA_TextCenter);
    HutSign->SetTextRenderColor(FColor(226, 213, 178));
    if (auto* SignMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_WoodsSignText.M_WoodsSignText")))
        HutSign->SetTextMaterial(SignMaterial);
    HutSign->SetWorldSize(26.f);
    const float SignTextWidth = HutSign->GetTextLocalSize().Y;
    if (SignTextWidth > 204.f) HutSign->SetWorldSize(26.f * 204.f / SignTextWidth);
    HutSign->SetWorldLocation(FVector(HouseWoods::HutX, HouseWoods::HutY - 269.f, HouseWoods::HutFloor + 269.f));
    HutSign->SetWorldRotation(FRotator(0.f, -90.f, 0.f));
    HutSign->RegisterComponent();
    KeepAlive.Add(HutSign);
    UE_LOG(LogTemp, Display, TEXT("WOODS_HUT_SIGN text=Coles Quarters width=%.1f collision=0"), HutSign->GetTextLocalSize().Y);
    // A replicated gameplay actor is spawned only by the authority. The static
    // court and its collision are generated identically on every machine.
    if (World->GetNetMode() != NM_Client)
    {
        FActorSpawnParameters BallParams;
        BallParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        BallParams.ObjectFlags |= RF_Transient;
        World->SpawnActor<AHouseBasketball>(AHouseBasketball::StaticClass(),
            FTransform(FRotator::ZeroRotator, HouseCourt::BallHome(), FVector(.248f)), BallParams);
    }
    SpawnHouseRearCourtItems(World);
    SpawnHouseBasketballHoops(World);
    SpawnHouseWoodcutting(World);
    auto Instance = [&](const TCHAR* Name, TFunctionRef<void(FHallMesh&, TArray<FBox>&)> Builder, const TArray<FTransform>& Xforms, bool bCollision, float CullEnd)->UHierarchicalInstancedStaticMeshComponent*
    {
        if (Xforms.Num() == 0) return nullptr;
        FHallMesh M; TArray<FBox> Col;
        Builder(M, Col);
        UStaticMesh* SM = HwBuildStaticMesh(M, *FString::Printf(TEXT("SM_Exterior_%s"), Name), Look, Col);
        if (!SM) return nullptr;
        SM=HouseCalderMapTrees::Mesh(this,Look,FString(TEXT("Exterior"))+Name,SM,Xforms);
        KeepAlive.Add(SM);
        UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("Exterior%s"), Name));
        H->SetStaticMesh(SM);
        H->SetMobility(EComponentMobility::Static);
        if (bCollision) { H->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName); H->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); }
        else H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        const bool bFoliage = FString(Name).StartsWith(TEXT("Tree")) || FString(Name).StartsWith(TEXT("Bush")) || FString(Name).StartsWith(TEXT("Grass")) || FString(Name).StartsWith(TEXT("Leaves"));
        H->SetCullDistances(bFoliage ? (int32)(CullEnd * .72f) : 0, (int32)CullEnd);
        H->SetCastShadow(false);
        if(FString(Name).StartsWith(TEXT("Tree")))
        {
            // Each instance copies this flag when its Chaos body is created.
            H->SetNotifyRigidBodyCollision(true);
            H->OnComponentHit.AddDynamic(this,&AHouseExteriorDetail::OnTreeHit);
        }
        H->RegisterComponent();
        // Keep every scatter draw and variant assignment; clear only rendered grass/shrubs overlapping new solids.
        const bool RowVegetation=HouseFraternityRow::Enabled()&&(FString(Name).StartsWith(TEXT("Grass"))||FString(Name).StartsWith(TEXT("Bush")));
        int32 Added=0,RowCleared=0;
        for (const FTransform& T : Xforms)
        {
            if(RowVegetation)
            {
                const FBox WorldBounds=SM->GetBoundingBox().TransformBy(T);
                if(HouseFraternityRowYards::ExistingVegetationBlocked(FBox2D(FVector2D(WorldBounds.Min),FVector2D(WorldBounds.Max))))
                {++RowCleared;continue;}
            }
            H->AddInstance(T,true);++Added;
        }
        if(RowCleared>0)UE_LOG(LogTemp,Display,TEXT("FRATERNITY_ROW_VEGETATION batch=%s removedOverlappingActualMeshBounds=%d retained=%d"),Name,RowCleared,Added);
        Instanced.Add(H);
        Instances += Added;
        Triangles += M.NumTriangles() * Added;
        return H;
    };
    TArray<HouseNature::FTreeCanopy> Canopies;
    for(int V=0;V<3;++V)
    {
        TArray<FTransform> Plants;for(int I=0;I<ParkingPlants.Num();++I)if(I%3==V)Plants.Add(ParkingPlants[I]);
        if(auto* H=Instance(*FString::Printf(TEXT("BushParking%d"),V),[&](FHallMesh& M,TArray<FBox>& C){HouseNature::Shrub(M,V,C);},Plants,false,10000.f))
        {H->bAffectDynamicIndirectLighting=false;H->bAffectDistanceFieldLighting=false;H->MarkRenderStateDirty();}
    }
    TArray<FTransform> FraternityShrubs;HouseSurroundings::Garden(FraternityShrubs);
    for(int V=0;V<3;++V)
    {
        TArray<FTransform> Plants;for(int I=0;I<FraternityShrubs.Num();++I)if(I%3==V)Plants.Add(FraternityShrubs[I]);
        Instance(*FString::Printf(TEXT("BushFraternity%d"),V),[&](FHallMesh& M,TArray<FBox>& C){HouseNature::Shrub(M,V,C);},Plants,false,17000.f);
    }
    // Species share nine instanced meshes; collision remains restricted to the trunks.
    int32 CampusTreesHidden=0;
    for(int32 Kind=0;Kind<3;++Kind)for(int32 Variant=0;Variant<3;++Variant)
    {
        const auto Species=HouseNature::ETreeKind(Kind);
        TArray<FTransform> Trees;
        // How far this species' trunk collision box reaches from the tree's centre at any yaw (maple, oak, birch), scaled.
        const float TrunkReach=Kind==0?43.f:Kind==1?75.f:51.f;
        auto Reach=[TrunkReach](const FTransform& T){return TrunkReach*FMath::Max(FMath::Abs(T.GetScale3D().X),FMath::Abs(T.GetScale3D().Y))+5.f;};
        for(int32 I=0;I<Site.TreeA.Num();++I)
        {
            const int32 Type=I<2||I%4==0?0:1;
            // A tree that the campus plan builds over, or whose trunk would reach a building, lane or walk, is hidden here,
            // never moved (the planting itself is unchanged).
            if(Type==Kind&&I%3==Variant&&HouseCampusLayout::TreeHidden(Site.TreeA[I].GetLocation(),Reach(Site.TreeA[I]))){++CampusTreesHidden;continue;}
            if(Type==Kind&&I%3==Variant)Trees.Add(Site.TreeA[I]);
        }
        for(int32 I=0;I<Site.TreeB.Num();++I)
        {
            const int32 Type=I%3==2?0:2;
            if(Type!=Kind||I%3!=Variant)continue;
            FTransform T=Site.TreeB[I];T.SetScale3D(T.GetScale3D()*(Type==0?.71f:.82f));
            if(HouseCampusLayout::TreeHidden(T.GetLocation(),Reach(T))){++CampusTreesHidden;continue;}
            Trees.Add(T);
        }
        const auto& Garden=Kind==1?GardenOaks:GardenBirches;
        if(Kind>0)for(int32 I=0;I<Garden.Num();++I)if(I%3==Variant)Trees.Add(Garden[I]);
        // Homecoming (HouseCampusFall): a few campus trees go full orange or red. They keep their transforms and the
        // site arrays and random stream are untouched; only which batch draws them changes.
        TArray<FTransform> Autumn[2];
        Trees.RemoveAll([&Autumn](const FTransform& T){const int32 Tone=HouseCampusFall::AutumnTone(T.GetLocation());if(Tone<0)return false;Autumn[Tone].Add(T);return true;});
        HouseNature::FTreeCanopy Canopy;Canopy.Kind=Species;Canopy.Variant=Variant;
        if(auto* H=Instance(*FString::Printf(TEXT("Tree%s%d"),HouseNature::TreeName(Species),Variant),
            [&](FHallMesh& M,TArray<FBox>& C){HouseNature::Tree(M,Species,Variant,C,&Canopy.Leaves);},Trees,true,Kind==1?29000.f:26000.f))
        {
            for(int32 Tone=0;Tone<2;++Tone)
            {
                if(Autumn[Tone].Num()==0)continue;
                // Same mesh and settings as the species batch, leaf slots recoloured.
                auto* F=NewObject<UHierarchicalInstancedStaticMeshComponent>(this,*FString::Printf(TEXT("CampusFallTree%s%d%s"),HouseNature::TreeName(Species),Variant,Tone?TEXT("Red"):TEXT("Orange")));
                F->SetStaticMesh(HouseCalderMapTrees::Mesh(this,Look,F->GetName(),H->GetStaticMesh(),Autumn[Tone]));
                F->SetMobility(EComponentMobility::Static);
                F->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);F->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
                F->SetCullDistances(H->InstanceStartCullDistance,H->InstanceEndCullDistance);
                F->SetCastShadow(false);
                F->SetNotifyRigidBodyCollision(true);
                F->OnComponentHit.AddDynamic(this,&AHouseExteriorDetail::OnTreeHit);
                for(const FName Slot:F->GetMaterialSlotNames())
                {
                    const FName Leaf=HouseCampusFall::AutumnLeafKey(Slot,Tone);
                    if(!Leaf.IsNone())F->SetMaterialByName(Slot,Look->Mat(Leaf));
                }
                F->RegisterComponent();
                for(const FTransform& T:Autumn[Tone])F->AddInstance(T,true);
                Instanced.Add(F);
                Instances+=Autumn[Tone].Num();
                TreeBatches.Add({F,uint8(Kind),uint8(Variant)});
                UE_LOG(LogTemp,Display,TEXT("CAMPUS_FALL_TREES %s variant=%d tone=%s instances=%d"),HouseNature::TreeName(Species),Variant,Tone?TEXT("red"):TEXT("orange"),Autumn[Tone].Num());
            }
            TreeBatches.Add({H,uint8(Kind),uint8(Variant)});
            HouseCalderMapTrees::Canopy(H->GetStaticMesh(),Canopy.Leaves);
            Canopies.Add(MoveTemp(Canopy));
            UE_LOG(LogTemp,Display,TEXT("TREE_CATALOG %s variant=%d instances=%d"),HouseNature::TreeName(Species),Variant,Trees.Num());
        }
    }
    UE_LOG(LogTemp,Display,TEXT("CAMPUS_TREES_HIDDEN plan=%d hidden=%d"),HouseCampusLayout::Enabled()?1:0,CampusTreesHidden);
    // Calder (master prompt step 1.3, Crow Ledge): a distant-forest tree within 40 m of the Crow Ledge trail or the ledge is
    // hidden here, never moved, after RecordOriginalTrees, so the crc is unchanged (HouseCalderEdges plants woods trees with
    // trunk collision there instead). None with -NoCalderTerrain.
    int32 CalderHiddenDistant=0;
    for(int32 Variant=0;Variant<3;++Variant)
    {
        TArray<FTransform> Trees;
        for(int32 I=0;I<Site.ForestDistant.Num();++I)if(I%3==Variant)
        {
            if(HouseCalderEdges::HidesForestDistant(Site.ForestDistant[I].GetLocation())){++CalderHiddenDistant;continue;}
            Trees.Add(Site.ForestDistant[I]);
        }
        if(auto* H=Instance(*FString::Printf(TEXT("ForestDistant%d"),Variant),[&](FHallMesh& M,TArray<FBox>& C){HouseCampus::DistantTree(M,Variant);},Trees,false,56000.f))
        {
            // Registration can copy the generated static mesh's default body
            // profile. Distant canopy scenery must stay out of gameplay physics.
            H->SetCollisionProfileName(TEXT("NoCollision"));
            H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            H->bAffectDynamicIndirectLighting=false;
            H->bAffectDistanceFieldLighting=false;
            H->MarkRenderStateDirty();
        }
    }
    UE_LOG(LogTemp,Display,TEXT("CALDER_EDGES_FOREST hiddenForestDistant=%d of %d"),CalderHiddenDistant,Site.ForestDistant.Num());
    // The parking lot's dumpster pad and roll carts (HouseLotDetail) stand where three hedge shrubs grew beside the house walk.
    auto UnderLotProps = [](const FVector& Pos) { return Pos.X > -3720.f && Pos.X < -3540.f && Pos.Y > -1660.f && Pos.Y < -1040.f; };
    auto InFrontPlantingBed = [](const FVector& Pos) { return Pos.Y < -2100.f && Pos.X > -3400.f && Pos.X < 3400.f; };
    for (int32 Variant = 0; Variant < 3; ++Variant)
    {
        TArray<FTransform> Shrubs, FrontShrubs;
        for (int32 I = 0; I < Site.Bush.Num(); ++I)
        {
            if (I % 3 != Variant || UnderLotProps(Site.Bush[I].GetLocation())) continue;
            (InFrontPlantingBed(Site.Bush[I].GetLocation()) ? FrontShrubs : Shrubs).Add(Site.Bush[I]);
        }
        Instance(*FString::Printf(TEXT("BushDetailed%d"), Variant), [&](FHallMesh& M, TArray<FBox>& C) { HouseNature::Shrub(M, Variant, C); }, Shrubs, true, 12000.f);
        Instance(*FString::Printf(TEXT("BushFrontDetailed%d"), Variant), [&](FHallMesh& M, TArray<FBox>& C) { HouseNature::FrontShrub(M, Variant, C); }, FrontShrubs, true, 12000.f);
        UE_LOG(LogTemp,Display,TEXT("FRONT_BUSH_PLACEMENT variant=%d front=%d unchangedSide=%d"),Variant,FrontShrubs.Num(),Shrubs.Num());
    }
    const auto Woodland=HouseWoods::Scatter();
    auto WoodlandBatch=[&](const TCHAR* Name,const TArray<FTransform>& All,int Variant,TFunctionRef<void(FHallMesh&,TArray<FBox>&)> Builder,bool Collision,float Cull)
    {
        TArray<FTransform> Transforms;for(int I=0;I<All.Num();++I)if(I%3==Variant)Transforms.Add(All[I]);
        if(auto* H=Instance(*FString::Printf(TEXT("Woods%s%d"),Name,Variant),Builder,Transforms,Collision,Cull))
        {
            H->SetCollisionProfileName(Collision?UCollisionProfile::BlockAll_ProfileName:FName(TEXT("NoCollision")));
            H->SetCollisionEnabled(Collision?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
            if(FString(Name).Contains(TEXT("Grass"))||FString(Name).Contains(TEXT("Shrub")))H->SetCullDistances(int32(Cull*.72f),int32(Cull));
            H->bAffectDynamicIndirectLighting=false;H->bAffectDistanceFieldLighting=false;H->MarkRenderStateDirty();
        }
    };
    for(int V=0;V<3;++V)
    {
        WoodlandBatch(TEXT("FallenLog"),Woodland.Logs,V,[&](FHallMesh& M,TArray<FBox>& C){HouseWoods::FallenLog(M,V,C);},true,13000);
        WoodlandBatch(TEXT("Branches"),Woodland.Branches,V,[&](FHallMesh& M,TArray<FBox>& C){HouseWoods::BranchPile(M,V);},false,5500);
        WoodlandBatch(TEXT("Flowers"),Woodland.Flowers,V,[&](FHallMesh& M,TArray<FBox>& C){HouseWoods::Flowers(M,V);},false,5000);
        WoodlandBatch(TEXT("Rock"),Woodland.Rocks,V,[&](FHallMesh& M,TArray<FBox>& C){HouseWoods::Rock(M,V,C);},true,8500);
        WoodlandBatch(TEXT("Leaves"),Woodland.Leaves,V,[&](FHallMesh& M,TArray<FBox>& C){HouseNature::LeafLitter(M,V);},false,3200);
        WoodlandBatch(TEXT("Shrub"),Woodland.Shrubs,V,[&](FHallMesh& M,TArray<FBox>& C){HouseNature::Shrub(M,V,C);},false,7500);
        WoodlandBatch(TEXT("GrassGreen"),Woodland.GrassGreen,V,[&](FHallMesh& M,TArray<FBox>& C){HouseWoods::ThickGrass(M,false,V);},false,4000);
        WoodlandBatch(TEXT("GrassDry"),Woodland.GrassDry,V,[&](FHallMesh& M,TArray<FBox>& C){HouseWoods::ThickGrass(M,true,V);},false,4000);
    }
    UE_LOG(LogTemp,Display,TEXT("WOODS_DETAIL logs=%d branches=%d flowers=%d rocks=%d litter=%d hut=(%.0f,%.0f) hollows=3"),Woodland.Logs.Num(),Woodland.Branches.Num(),Woodland.Flowers.Num(),Woodland.Rocks.Num(),Woodland.Leaves.Num(),HouseWoods::HutX,HouseWoods::HutY);
    UE_LOG(LogTemp,Display,TEXT("WOODS_UNDERGROWTH shrubs=%d thickGreen=%d thickDry=%d sharedBatches=9"),Woodland.Shrubs.Num(),Woodland.GrassGreen.Num(),Woodland.GrassDry.Num());
    if (HouseWoodsPass::Stage() >= 1)
    {
        HouseWoodsStructure::Build(this, Look, OriginalWoodlandTrees);
        HouseWoodsFloor::Build(this, Look, OriginalWoodlandTrees);
    }
    if (HouseWoodsPass::Stage() >= 2) HouseWoodsSilhouettes::Build(this, Look);
    if (HouseWoodsPass::Stage() >= 3) HouseWoodsTraces::Build(this, Look);
    TArray<FTransform> GrassGreen, GrassDry, Leaves;
    TArray<FTransform> BigWithOaks=Site.TreeA,SmallWithBirches=Site.TreeB;
    BigWithOaks.Append(GardenOaks);SmallWithBirches.Append(GardenBirches);
    HouseNature::Scatter(GrassGreen, GrassDry, Leaves, BigWithOaks, SmallWithBirches, Site.Bush);
    for (int32 Variant = 0; Variant < 3; ++Variant)
    {
        TArray<FTransform> Green, Dry, Litter;
        for (int32 I = 0; I < GrassGreen.Num(); ++I) if (I % 3 == Variant) Green.Add(GrassGreen[I]);
        for (int32 I = 0; I < GrassDry.Num(); ++I) if (I % 3 == Variant) Dry.Add(GrassDry[I]);
        for (int32 I = 0; I < Leaves.Num(); ++I) if (I % 3 == Variant) Litter.Add(Leaves[I]);
        Instance(*FString::Printf(TEXT("GrassGreen%d"), Variant), [&](FHallMesh& M, TArray<FBox>& C) { HouseNature::GrassClump(M, false, Variant); }, Green, false, 4400.f);
        Instance(*FString::Printf(TEXT("GrassDry%d"), Variant), [&](FHallMesh& M, TArray<FBox>& C) { HouseNature::GrassClump(M, true, Variant); }, Dry, false, 4400.f);
        Instance(*FString::Printf(TEXT("LeavesFallen%d"), Variant), [&](FHallMesh& M, TArray<FBox>& C) { HouseNature::LeafLitter(M, Variant); }, Litter, false, 2600.f);
    }
    TArray<FTransform> YardItems[5];
    HouseNature::ScatterYardDebris(YardItems[0],YardItems[1],YardItems[2],YardItems[3],YardItems[4]);
    const TCHAR* YardNames[]={TEXT("YardStick"),TEXT("YardPinecone"),TEXT("YardPaper"),TEXT("YardCan"),TEXT("YardCup")};
    for(int32 Kind=0;Kind<5;++Kind)
    {
        const int32 Variants=Kind<2?3:1;
        for(int32 Variant=0;Variant<Variants;++Variant)
        {
            TArray<FTransform> Parts;
            for(int32 I=0;I<YardItems[Kind].Num();++I)if(I%Variants==Variant)Parts.Add(YardItems[Kind][I]);
            Instance(*FString::Printf(TEXT("%s%d"),YardNames[Kind],Variant),
                [&](FHallMesh& M,TArray<FBox>& C){HouseNature::YardDebris(M,HouseNature::EYardDebris(Kind),Variant);},Parts,false,2600.f);
        }
    }
    if (World->GetNetMode() != NM_DedicatedServer)
    {
        FActorSpawnParameters WindParams;
        WindParams.Owner = this;
        WindParams.ObjectFlags |= RF_Transient;
        WindParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (AHouseLeafWind* Wind = World->SpawnActor<AHouseLeafWind>(AHouseLeafWind::StaticClass(), FTransform::Identity, WindParams))
        {
            Wind->Initialize(Look, Leaves, Canopies);
            if (HouseWoodsPass::Stage() >= 3)
            {
                TArray<FTransform> WindTrees = OriginalWoodlandTrees;
                TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> TreeParts(this);
                for (auto* Part : TreeParts)
                    if (Part->GetName().StartsWith(TEXT("ExteriorWoodsPassOak")) ||
                        Part->GetName().StartsWith(TEXT("ExteriorWoodsPassHickory")) ||
                        Part->GetName().StartsWith(TEXT("ExteriorWoodsPassMaple")) ||
                        Part->GetName().StartsWith(TEXT("ExteriorWoodsPassTulipPoplar")) ||
                        Part->GetName().StartsWith(TEXT("ExteriorWoodsPassHemlock")))
                        for (int32 TreeIndex = 0; TreeIndex < Part->GetInstanceCount(); ++TreeIndex)
                        {
                            FTransform TreeTransform;
                            Part->GetInstanceTransform(TreeIndex, TreeTransform, true);
                            WindTrees.Add(TreeTransform);
                        }
                Wind->EnableWoodlandEffects(Look, HouseWoodsStructure::SwayMaterialKeys(), WindTrees);
            }
        }
    }
    Instance(TEXT("Pole"), [&](FHallMesh& M, TArray<FBox>& C) { HouseStreetScene::StreetPoleMesh(M, C); }, Site.Poles, true, 25000.f);
    Instance(TEXT("PoleLot"), [&](FHallMesh& M, TArray<FBox>& C) { HouseStreetScene::LotPoleMesh(M, C); }, Site.LotPoles, true, 25000.f);
    // Classic cars (1950s-80s, HouseClassicCars). Every parked spot keeps its place and heading; each gets a period car
    // type and paint scheme, sits on its own paving and is scaled to the house (cars are modelled in real centimetres and
    // the house is built 1.1x tall). The west lot gets a curated row plus the one muscle car, nose-in like the others.
    {
        constexpr float CarScale = 1.1f;
        const bool bCarShadows = !FParse::Param(FCommandLine::Get(), TEXT("NoCarShadows"));
        const TArray<FExCarScheme>& Schemes = ExCarSchemes();
        TArray<TArray<FTransform>> PerScheme; PerScheme.SetNum(Schemes.Num());
        auto SchemeFor = [&](EExCar Type, int32 Pick) -> int32
        {
            TArray<int32> Of;
            for (int32 I = 0; I < Schemes.Num(); ++I) if (Schemes[I].Type == Type) Of.Add(I);
            return Of.Num() ? Of[Pick % Of.Num()] : 0;
        };
        auto InLot = [](const FVector& P) { return P.X > HouseParking::West && P.X < HouseParking::East && P.Y > HouseParking::South && P.Y < HouseParking::North; };
        auto PaveZ = [&](const FVector& P) -> float
        {
            float PlanZ;
            if (HouseCampusLayout::PaveZ(P, PlanZ)) return PlanZ;
            if (InLot(P)) return HouseParking::Surface;
            if (P.X < 8700.f && P.Y > HouseExteriorSite::FrontRoadFarY && P.Y < HouseExteriorSite::FrontRoadNearY) return GF - 12.f;   // street patch
            if (P.X > -9000.f && P.X < 0.f && P.Y > StreetY(-7400.f) && P.Y < StreetY(-4600.f)) return GF + 2.f;                   // health-centre lot
            if (P.X > 3450.f && P.X < 6600.f && P.Y > 1900.f && P.Y < 4500.f) return GR + 2.f;                                     // rear lot
            return HouseExteriorSite::GroundHeight(P.X, P.Y);   // (Car() already pitched it to the ground there)
        };
        auto Hash = [](const FVector& P, uint32 Salt)
        {
            uint32 H = ((uint32)FMath::RoundToInt(P.X) * 73856093u) ^ ((uint32)FMath::RoundToInt(P.Y) * 19349663u) ^ (Salt * 83492791u);
            H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
            return H;
        };
        // HouseParking's wheel stops sit 190 cm ahead of each bay's parking point: stop the front tyres 4 cm short of them.
        auto Place = [&](FTransform T, EExCar Type, int32 Scheme)
        {
            FVector P = T.GetLocation();
            if (InLot(P)) P += T.GetRotation().GetForwardVector() * (186.f - CarScale * ExCarFrontTyreReach(Type, 11.f / CarScale));
            P.Z = PaveZ(P) - 1.f;   // tyres settle 1 cm into the paving
            T.SetLocation(P);
            T.SetScale3D(FVector(CarScale));
            PerScheme[Scheme].Add(T);
        };
        TArray<FTransform> All; All.Append(Site.CarsA); All.Append(Site.CarsB); All.Append(Site.CarsC);
        TArray<int32> Lot;
        for (int32 I = 0; I < All.Num(); ++I) if (InLot(All[I].GetLocation())) Lot.Add(I);
        Lot.Sort([&](int32 A, int32 Bi)
        {
            const FVector PA = All[A].GetLocation(), PB = All[Bi].GetLocation();
            const bool UA = PA.Y > -1400.f, UB = PB.Y > -1400.f;
            return UA != UB ? UA : PA.X < PB.X;
        });
        static const EExCar LotMix[] = { EExCar::Wagon, EExCar::Yacht, EExCar::Fifties, EExCar::Pickup, EExCar::Hatch, EExCar::Yacht,
            EExCar::Hatch, EExCar::Pickup, EExCar::Wagon, EExCar::Fifties };
        int32 Used[(int32)EExCar::Count] = {};
        for (int32 K = 0; K < Lot.Num(); ++K)
        {
            const EExCar Type = LotMix[K % UE_ARRAY_COUNT(LotMix)];
            Place(All[Lot[K]], Type, SchemeFor(Type, Used[(int32)Type]++));
        }
        // Kerbside and the other lots: a weighted mix of the five everyday types (land yachts and hatchbacks most common).
        for (int32 I = 0; I < All.Num(); ++I)
        {
            if (InLot(All[I].GetLocation())) continue;
            const float R = (Hash(All[I].GetLocation(), 1) & 0xffff) / 65536.f;
            const EExCar Type = R < .26f ? EExCar::Yacht : R < .44f ? EExCar::Wagon : R < .64f ? EExCar::Pickup : R < .88f ? EExCar::Hatch : EExCar::Fifties;
            Place(All[I], Type, SchemeFor(Type, (int32)(Hash(All[I].GetLocation(), 2) % 7)));
        }
        // Out front: a woody wagon, a square-bodied sedan and a hatchback at the near kerb, west of the front walk (the walk
        // review uses the near lane east of it). Right-hand traffic: the near kerb faces +x, the far kerb -x.
        struct FKerbCar { float X; EExCar Type; int32 Pick; float Yaw; };
        const FKerbCar FrontKerb[] = { { HouseStreetScene::KerbCarX[0], EExCar::Wagon, 1, -.6f }, { HouseStreetScene::KerbCarX[1], EExCar::Yacht, 0, .8f },
            { HouseStreetScene::KerbCarX[2], EExCar::Hatch, 1, -1.2f } };
        for (const FKerbCar& K : FrontKerb) Place(FTransform(FRotator(0.f, K.Yaw, 0.f), FVector(K.X, -4300.f, 0.f)), K.Type, SchemeFor(K.Type, K.Pick));
        // The muscle car takes the open bay beside the accessible spaces: the first car the player reaches from the house walk.
        FVector MusclePos = FVector::ZeroVector;
        if(!HouseFraternityRow::Enabled())for (float X : { -5045.f, -5705.f, -6365.f })   // centres of HouseParking's upper-row bays 5, 3 and 1
        {
            bool bFree = true;
            for (int32 I : Lot) bFree &= FVector::Dist2D(All[I].GetLocation(), FVector(X, -640.f, 0.f)) > 150.f;
            if (!bFree) continue;
            MusclePos = FVector(X, -640.f, HouseParking::Surface);
            Place(FTransform(FRotator(0.f, 90.f, 0.f), MusclePos), EExCar::Muscle, SchemeFor(EExCar::Muscle, 0));
            break;
        }
        // Police cruisers (HouseClassicCars' Cruiser) pulled in round the Fall Harvest Jam field: three nosed over the front
        // kerb (front tyres on the sidewalk, so a touch of pitch), three off the side street, and one that drove in through
        // the hayride gate onto the grass. None on the drive: the walk review walks it.
        {
            const int32 CopScheme = SchemeFor(EExCar::Cruiser, 0);
            auto Cruiser = [&](float X, float Y, float Yaw, float Pitch, float Z)
            {
                FTransform T(FRotator(Pitch, Yaw, 0.f), FVector(X, Y, Z));
                T.SetScale3D(FVector(CarScale));
                PerScheme[CopScheme].Add(T);
            };
            const float Road = GF - 12.f - 1.f;
            Cruiser(5000.f, -4190.f, 72.f, 3.f, Road);
            Cruiser(5900.f, -4200.f, 106.f, 3.f, Road);
            Cruiser(6900.f, -4190.f, 88.f, 3.f, Road);
            Cruiser(8770.f, -3300.f, 158.f, 3.f, Road);
            Cruiser(8790.f, -2000.f, 184.f, 3.f, Road);
            Cruiser(8770.f, -800.f, 212.f, 3.f, Road);
            Cruiser(6880.f, -3150.f, 96.f, 0.f, GF - 1.f);
        }
        UStaticMesh* TypeMesh[(int32)EExCar::Count] = {};
        int32 TypeTris[(int32)EExCar::Count] = {};
        int32 CarCount = 0, Groups = 0;
        for (int32 S = 0; S < Schemes.Num(); ++S)
        {
            if (PerScheme[S].Num() == 0) continue;
            const int32 Ty = (int32)Schemes[S].Type;
            if (!TypeMesh[Ty])
            {
                FHallMesh M; TArray<FBox> Col;
                ExBuildCar(Schemes[S].Type, M, Col);
                TypeMesh[Ty] = HwBuildStaticMesh(M, *FString::Printf(TEXT("SM_Exterior_Car%s"), ExCarTypeName(Schemes[S].Type)), Look, Col);
                TypeTris[Ty] = M.NumTriangles();
                if (!TypeMesh[Ty]) continue;
                KeepAlive.Add(TypeMesh[Ty]);
            }
            UStaticMesh* SM = TypeMesh[Ty];
            UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("ExteriorCar%s%d"), ExCarTypeName(Schemes[S].Type), S));
            H->SetStaticMesh(SM);
            H->SetMobility(EComponentMobility::Static);
            H->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
            H->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
            H->SetCullDistances(0, 16000);
            H->SetCastShadow(bCarShadows);   // the moon's single near cascade gives nearby cars a ground shadow
            H->SetReceivesDecals(false);       // tyres stand in the road decals' projection
            auto Override = [&](int32 Slot, FName Key)
            {
                if (Key.IsNone()) return;
                const int32 Index = SM->GetMaterialIndex(ExCarSlot(Slot));
                if (Index != INDEX_NONE) H->SetMaterial(Index, Look->Mat(Key));
            };
            Override(ExCarSlots::Paint, Schemes[S].Paint);
            Override(ExCarSlots::Paint2, Schemes[S].Paint2);
            Override(ExCarSlots::Vinyl, Schemes[S].Vinyl);
            Override(ExCarSlots::WheelPaint, Schemes[S].WheelPaint);
            H->RegisterComponent();
            for (const FTransform& T : PerScheme[S]) H->AddInstance(T, true);
            Instanced.Add(H);
            Instances += PerScheme[S].Num();
            Triangles += TypeTris[Ty] * PerScheme[S].Num();
            CarCount += PerScheme[S].Num();
            ++Groups;
        }
        UE_LOG(LogTemp, Display, TEXT("HOUSE_CLASSIC_CARS cars=%d paintGroups=%d westLot=%d muscle=(%.0f,%.0f) scale=%.2f shadows=%d"),
            CarCount, Groups, Lot.Num() + (MusclePos.IsZero() ? 0 : 1), MusclePos.X, MusclePos.Y, CarScale, bCarShadows ? 1 : 0);
    }
    Instance(TEXT("Chair"), [&](FHallMesh& M, TArray<FBox>& C) { ExChairMesh(B, M, C); }, Site.Chairs, true, 8000.f);
    // ---------------------------------------------------------------- lights
    float SodiumScale = 1.f;   // -DuskSodium=<scale>: look tests of the pole lamps' strength
    FParse::Value(FCommandLine::Get(), TEXT("DuskSodium="), SodiumScale);
    for (const FExBuilder::FLightSpec& L : B.LightSpecs)
    {
        // Pole lamps are spots aimed straight down from the cobra-head lens; everything else stays a point light.
        UPointLightComponent* P = L.bSpotDown ? NewObject<USpotLightComponent>(this) : NewObject<UPointLightComponent>(this);
        P->SetupAttachment(RootComponent);
        P->SetMobility(EComponentMobility::Movable);
        P->SetIntensityUnits(ELightUnits::Lumens);
        P->SetIntensity(L.Lumens * (L.bSpotDown ? SodiumScale : 1.f));
        P->SetLightColor(L.Color);
        P->SetAttenuationRadius(L.Radius);
        P->SetSourceRadius(6.f);
        P->SetCastShadows(false);
        P->MaxDrawDistance = L.Draw;
        P->MaxDistanceFadeRange = L.Draw * .2f;
        P->RegisterComponent();
        P->SetWorldLocation(L.Pos);
        if (USpotLightComponent* Spot = Cast<USpotLightComponent>(P))
        {
            Spot->SetInnerConeAngle(36.f);
            Spot->SetOuterConeAngle(74.f);
            Spot->SetWorldRotation(FRotator(-90.f, 0.f, 0.f));
        }
        PointLights.Add(P);
    }
    Lights = PointLights.Num();
    // ---------------------------------------------------------------- invisible blockers: railing remnants and the playable edge
    for (const FBox& Bx : B.BlockBoxes)
    {
        UBoxComponent* C = NewObject<UBoxComponent>(this);
        C->SetupAttachment(RootComponent);
        C->SetBoxExtent(Bx.GetExtent());
        C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        C->RegisterComponent();
        C->SetWorldLocation(Bx.GetCenter());
        Blockers.Add(C);
    }
    // Player-only property boundaries sit beyond usable routes. Cameras,
    // visibility traces and thrown props pass through this invisible perimeter.
    // Off for now (HouseExteriorSite::PropertyBoundaryEnabled): the whole site is open to walk.
    const bool bPropertyBoundary=HouseExteriorSite::PropertyBoundaryEnabled();
    const float West=HouseExteriorSite::PlayableWestX,East=HouseExteriorSite::PlayableEastX;
    const float Front=HouseExteriorSite::PlayableFrontY,Rear=HouseExteriorSite::PlayableRearY;
    const float Thick=HouseExteriorSite::BoundaryHalfThickness;
    const FBox PropertyEdges[]={
        FBox(FVector(West-Thick,Front-Thick,-1200),FVector(West+Thick,Rear+Thick,5600)),
        FBox(FVector(East-Thick,Front-Thick,-1200),FVector(East+Thick,Rear+Thick,5600)),
        FBox(FVector(West-Thick,Front-Thick,-1200),FVector(East+Thick,Front+Thick,5600)),
        FBox(FVector(West-Thick,Rear-Thick,-1200),FVector(East+Thick,Rear+Thick,5600))};
    const TCHAR* EdgeNames[]={TEXT("PropertyBoundary_West"),TEXT("PropertyBoundary_East"),TEXT("PropertyBoundary_Front"),TEXT("PropertyBoundary_Rear")};
    for (int32 K = 0; bPropertyBoundary && K < 4; ++K)
    {
        UBoxComponent* C = NewObject<UBoxComponent>(this,EdgeNames[K]);
        C->SetupAttachment(RootComponent);
        C->SetMobility(EComponentMobility::Static);
        C->SetBoxExtent(PropertyEdges[K].GetExtent());
        C->SetCollisionObjectType(ECC_WorldStatic);
        C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        C->SetCollisionResponseToAllChannels(ECR_Ignore);
        C->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
        C->SetGenerateOverlapEvents(false);
        C->SetHiddenInGame(true);
        C->SetWorldLocation(PropertyEdges[K].GetCenter());
        C->RegisterComponent();
        Blockers.Add(C);
    }
    UE_LOG(LogTemp,Display,TEXT("FRATERNITY_PROPERTY houses=3 west=%.0f east=%.0f front=%.0f rear=%.0f playerOnly=1 boundary=%s extraLights=0"),West,East,Front,Rear,bPropertyBoundary?TEXT("on"):TEXT("off"));
    // ---------------------------------------------------------------- readable night exterior: reuse the moon and ambient sky, with no extra local lights
    // Blue-hour dusk (HouseDuskAtmosphere) replaces the moonlit night unless -NoDusk; it runs after the fog is spawned.
    const bool bDusk = !FParse::Param(FCommandLine::Get(), TEXT("NoDusk"));
    for (TActorIterator<ADirectionalLight> It(World); !bDusk && It; ++It)
    {
        if (UDirectionalLightComponent* D = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
        {
            Notes += FString::Printf(TEXT(" moon(%.2f->24.0)"), D->Intensity);
            // The map uses a fixed indoor exposure. Two lux left the open yard nearly black.
            D->SetIntensity(24.f);
            D->SetLightColor(FLinearColor(.78f, .86f, 1.f));
            D->DynamicShadowDistanceMovableLight = 5000.f;
            D->DynamicShadowCascades = 1;
            D->MarkRenderStateDirty();
        }
        It->SetActorRotation(FRotator(-38.f, 115.f, 0.f));
    }
    for (TActorIterator<ASkyLight> It(World); !bDusk && It; ++It)
    {
        if (USkyLightComponent* Sky = It->GetLightComponent())
        {
            // Lift the shaded grass and side paths using the existing captured sky.
            // The interior exposure and all room/club fixtures remain unchanged.
            const float OldIntensity = Sky->Intensity;
            Sky->SetIntensity(FMath::Max(.8f, OldIntensity * 2.f));
            Notes += FString::Printf(TEXT(" sky(%.2f->%.2f)"), OldIntensity, Sky->Intensity);
        }
    }
    for (TActorIterator<APostProcessVolume> It(World); It; ++It)
    {
        const FPostProcessSettings& PS = It->Settings;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR ppv=%s unbound=%d prio=%.1f expMethod=%d(%d) min=%.2f(%d) max=%.2f(%d) bias=%.2f(%d)"), *It->GetName(), It->bUnbound ? 1 : 0, It->Priority,
            (int32)PS.AutoExposureMethod, PS.bOverride_AutoExposureMethod ? 1 : 0, PS.AutoExposureMinBrightness, PS.bOverride_AutoExposureMinBrightness ? 1 : 0,
            PS.AutoExposureMaxBrightness, PS.bOverride_AutoExposureMaxBrightness ? 1 : 0, PS.AutoExposureBias, PS.bOverride_AutoExposureBias ? 1 : 0);
    }
    // Low, softly bounded mist belongs to the rear woodland. Analytic local
    // volumes avoid a new full-scene volumetric simulation or extra lights.
    if (!FParse::Param(FCommandLine::Get(),TEXT("NoWoodsFog")))
    {
        int Count=0;
        for(int Row=0;Row<3;++Row)for(int Column=0;Column<4;++Column)
        {
            const float Xs[]={-6100.f,-2200.f,2200.f,6100.f};
            const float Y=7300.f+Row*4900.f,Radius=Row==0?2700.f:2850.f;
            FActorSpawnParameters P;P.Owner=this;P.ObjectFlags|=RF_Transient;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            if(auto* Fog=World->SpawnActor<ALocalFogVolume>(ALocalFogVolume::StaticClass(),FVector(Xs[Column],Y,GR+155.f+Row*14.f),FRotator::ZeroRotator,P))
            {
                Fog->Tags.Add(TEXT("WoodlandMist"));Fog->SetActorEnableCollision(false);Fog->SetActorTickEnabled(false);Fog->SetActorScale3D(FVector(Radius/ULocalFogVolumeComponent::GetBaseVolumeSize()));
                if(auto* C=Fog->GetComponent())
                {
                    C->RadialFogExtinction=.30f;
                    C->HeightFogExtinction=.24f;
                    C->HeightFogFalloff=900.f;
                    C->HeightFogOffset=0.f;
                    C->FogAlbedo=FLinearColor(.56f,.61f,.64f);
                    C->FogEmissive=FLinearColor(.045f,.055f,.065f);
                    C->FogPhaseG=.15f;C->FogStartDistance=1000.f;
                    C->MarkRenderStateDirty();++Count;
                }
            }
        }
        Notes+=FString::Printf(TEXT(" woodlandMist=%d"),Count);
        UE_LOG(LogTemp,Display,TEXT("WOODS_FOG localVolumes=%d rearBoundaryY=4600 tick=0 collision=0 extraLights=0"),Count);
    }
    if (HouseWoodsPass::Stage() >= 2) HouseWoodsAtmosphere::Apply(this);
    // ---------------------------------------------------------------- distance fade so the world ends in haze rather than a void
    if (!FParse::Param(FCommandLine::Get(), TEXT("NoHouseFog")))
    {
        FActorSpawnParameters SP; SP.ObjectFlags |= RF_Transient; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector(0, 0, GR), FRotator::ZeroRotator, SP))
        {
            if (UExponentialHeightFogComponent* FC = Fog->GetComponent())
            {
                FC->FogDensity = HouseCalderLandscape::Enabled() ? .012f : .016f;
                FC->FogHeightFalloff = .08f;
                FC->FogInscatteringLuminance = FLinearColor(.02f, .026f, .05f);
                FC->StartDistance = 2600.f;
                FC->FogMaxOpacity = HouseCalderLandscape::Enabled() ? .90f : .97f;
                FC->FogCutoffDistance = 0.f;
                FC->MarkRenderStateDirty();
            }
            Notes += TEXT(" fog");
        }
    }
    if (bDusk)
    {
        Notes += HouseDusk::ApplyLighting(World);
        if (World->GetNetMode() != NM_DedicatedServer)
        {
            auto* Dusk = NewObject<UHouseDuskAtmosphereComponent>(this, TEXT("ExteriorDusk"));
            AddInstanceComponent(Dusk); Dusk->RegisterComponent();
            Dusk->Build(Look, HouseStreetScene::GrillSmokeOrigin());
        }
    }
    if (World->GetNetMode() != NM_DedicatedServer)
    {
        TArray<HouseStreetScene::FDecal> RoadDecals; HouseStreetScene::RoadDecals(RoadDecals);
        TArray<TObjectPtr<UDecalComponent>> DecalParts;
        const int32 DecalCount = HouseDusk::SpawnDecals(this, Look, RoadDecals, DecalParts);
        Notes += FString::Printf(TEXT(" roadDecals=%d/%d"), DecalCount, RoadDecals.Num());
    }
    // The campus clock tower is its own actor so its hands, bells and crows can move (HouseClockTower); no lights.
    if (const AHouseClockTower* Tower = AHouseClockTower::SpawnFor(this, Look)) Notes += FString::Printf(TEXT(" clockTower=%d"), Tower->Triangles);
    if (const AHouseCampusFall* Fall = AHouseCampusFall::SpawnFor(this, Look)) Notes += FString::Printf(TEXT(" campusFall=%d"), Fall->Triangles);
    // Calder State's exterior polish (Phase 3): its own actor, no lights, off with -NoCampusPolish.
    if (const AHouseCampusPolish* Polish = AHouseCampusPolish::SpawnFor(this, Look)) Notes += FString::Printf(TEXT(" campusPolish=%d pools=%d"), Polish->Triangles, Polish->PoolCount);
    // Scatter only after the real paving, tree trunks and polish solids exist;
    // the cover's native trace must reject those completed collision surfaces.
    HouseCalderCover::ScheduleBuild(this, Look);
    HouseCalderFarm::Build(this);
    HouseCalderHedgerow::Build(this, Look);
    HouseWoodsPass::Audit(this);
    UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR built tris=%d instances=%d lights=%d house=%d grounds=%d near=%d far=%d ms=%.0f%s"), Triangles, Instances, Lights,
        B.House.NumTriangles(), B.Grounds.NumTriangles(), B.Near.NumTriangles(), B.Far.NumTriangles(), (FPlatformTime::Seconds() - Start) * 1000.0, *Notes);
}

// ------------------------------------------------------------------ -HouseExteriorViews: fixed views, screenshots and frame-time probes
namespace
{
struct FExViewState
{
    TWeakObjectPtr<UWorld> World; TWeakObjectPtr<ACameraActor> Cam;
    int32 View = -1, Frames = 0, MotionFrames = 0;
    double Started = 0, StepStarted = 0, NextMotionShot = 0; bool bShot = false;
    double SumMs = 0, MaxMs = 0, SumGame = 0, SumRender = 0, SumRhi = 0, SumOn = 0, SumOff = 0; int32 NOn = 0, NOff = 0;
    TArray<TWeakObjectPtr<USceneComponent>> Hidden;   // -ExteriorViewToggle: what is hidden in the current off block
    FString Dir, Report;
};
}

namespace
{
/** Walks a route with downward traces (floor continuity) and knee/chest traces (blockers) and logs one line per route. */
void ExProbeRoute(UWorld* World, AActor* Ignore, const TCHAR* Name, const TArray<FVector>& Waypoints)
{
    FCollisionQueryParams QP(SCENE_QUERY_STAT(ExteriorProbe), true); QP.AddIgnoredActor(Ignore);
    float MaxStep = 0.f, PrevFloor = Waypoints[0].Z; int32 Gaps = 0, Blocks = 0, Samples = 0; FString Worst, StepAt;
    FVector Prev = Waypoints[0];
    for (int32 W = 0; W + 1 < Waypoints.Num(); ++W)
    {
        const FVector A = Waypoints[W], Bw = Waypoints[W + 1];
        const int32 N = FMath::Max(1, FMath::CeilToInt((Bw - A).Size2D() / 50.f));
        for (int32 K = (W == 0 ? 0 : 1); K <= N; ++K)
        {
            const FVector P = FMath::Lerp(A, Bw, (float)K / N);
            FHitResult Hit;
            const FVector Top(P.X, P.Y, PrevFloor + 140.f);
            const bool bFloor = World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 700), ECC_Visibility, QP);
            ++Samples;
            if (!bFloor) { ++Gaps; if (Worst.IsEmpty()) Worst = FString::Printf(TEXT("gap@%.0f,%.0f"), P.X, P.Y); continue; }
            const float Step = FMath::Abs(Hit.ImpactPoint.Z - PrevFloor);
            if (Samples > 1 && Step > MaxStep) { MaxStep = Step; if (Step > 45.f) StepAt = FString::Printf(TEXT("step%.0f@%.0f,%.0f(%s)"), Step, P.X, P.Y, *GetNameSafe(Hit.GetComponent())); }
            const FVector Here(P.X, P.Y, Hit.ImpactPoint.Z);
            if (Samples > 1)
                for (float H : { 70.f, 150.f })
                {
                    FHitResult Blk;
                    if (World->LineTraceSingleByChannel(Blk, Prev + FVector(0, 0, H), Here + FVector(0, 0, H), ECC_Visibility, QP))
                    { ++Blocks; if (Worst.IsEmpty()) Worst = FString::Printf(TEXT("block@%.0f,%.0f(%s)"), P.X, P.Y, *GetNameSafe(Blk.GetComponent())); }
                }
            PrevFloor = Hit.ImpactPoint.Z; Prev = Here;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR_PATH %s samples=%d gaps=%d blocks=%d maxstep=%.0f %s %s"), Name, Samples, Gaps, Blocks, MaxStep, *Worst, *StepAt);
}
/** Sweeps the pawn's own capsule across the deck and the patio and logs where it stops and what stopped it. */
void ExProbeSweeps(UWorld* World, ACharacter* Pawn)
{
    UCapsuleComponent* Cap = Pawn->GetCapsuleComponent();
    if (!Cap) return;
    const float Rad = Cap->GetScaledCapsuleRadius(), Half = Cap->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams QP(SCENE_QUERY_STAT(ExteriorSweep), false); QP.AddIgnoredActor(Pawn);
    FCollisionResponseParams RP; Cap->InitSweepCollisionParams(QP, RP);
    const ECollisionChannel Ch = Cap->GetCollisionObjectType();
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Rad - 1.f, Half - 1.f);
    auto Run = [&](const TCHAR* Name, float Floor, const FVector& Dir, float From, float To, float Across0, float Across1, float Step)
    {
        TMap<FString, int32> Who; FString Line; int32 Clear = 0, N = 0;
        for (float C = Across0; C <= Across1 + .1f; C += Step)
        {
            const bool bY = FMath::Abs(Dir.Y) > .5f;
            const FVector A = bY ? FVector(C, From, Floor + Half + 4.f) : FVector(From, C, Floor + Half + 4.f);
            const FVector Bv = bY ? FVector(C, To, Floor + Half + 4.f) : FVector(To, C, Floor + Half + 4.f);
            FHitResult Hit; ++N;
            if (World->SweepSingleByChannel(Hit, A, Bv, FQuat::Identity, Ch, Shape, QP, RP))
            {
                const FString Key = FString::Printf(TEXT("%s/%s%s"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.bStartPenetrating ? TEXT("(start)") : TEXT(""));
                Who.FindOrAdd(Key)++;
                Line += FString::Printf(TEXT(" %.0f:%.0f"), C, bY ? Hit.Location.Y : Hit.Location.X);
            }
            else { ++Clear; Line += FString::Printf(TEXT(" %.0f:-"), C); }
        }
        FString W; for (const auto& P : Who) W += FString::Printf(TEXT(" [%s x%d]"), *P.Key, P.Value);
        UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR_SWEEP %s radius=%.0f half=%.0f rays=%d clear=%d stops:%s blockers:%s"), Name, Rad, Half, N, Clear, *Line, *W);
    };
    Run(TEXT("deck_to_railing"), 0.f, FVector(0, 1, 0), 1000.f, 1800.f, -1360.f, 1360.f, 40.f);
    Run(TEXT("deck_along_mid"), 0.f, FVector(1, 0, 0), -1380.f, 1380.f, 1400.f, 1560.f, 40.f);
    Run(TEXT("patio_to_court"), -362.f, FVector(0, 1, 0), 1000.f, 2400.f, -1360.f, 1360.f, 40.f);
    Run(TEXT("patio_along"), -362.f, FVector(1, 0, 0), -1360.f, 1360.f, 1000.f, 1500.f, 50.f);
}
void ExProbeAll(UWorld* World, AActor* Ignore)
{
    ExProbeRoute(World, Ignore, TEXT("front_door_to_street"), { FVector(-36, -700, 0), FVector(-36, -1100, 0), FVector(-36, -1700, 0), FVector(-36, -2800, 0), FVector(-36, StreetY(-3650), 0), FVector(-36, StreetY(-4500), 0) });
    ExProbeRoute(World, Ignore, TEXT("patio_door_to_court"), { FVector(0, 950, -362), FVector(0, 1200, -362), FVector(-380, 1500, -362), FVector(-380, 1800, -362), FVector(-380, 3000, -362) });
    ExProbeRoute(World, Ignore, TEXT("east_side_path"), { FVector(3398, -2200, 0), FVector(3398, -1370, 0), FVector(3398, -970, 0), FVector(3398, -770, 0), FVector(3398, -350, 0) });
    ExProbeRoute(World, Ignore, TEXT("side_door_and_drive"), { FVector(3200, -1060, 0), FVector(3600, -1060, 0), FVector(3750, -2000, 0), FVector(3750, -2800, 0), FVector(3750, -1000, 0), FVector(3750, 1000, 0), FVector(3750, 2500, 0), FVector(5000, 4000, 0) });
    ExProbeRoute(World, Ignore, TEXT("perimeter"), { FVector(0, -1700, 0), FVector(-1300, -1700, 0), FVector(-1300, -2400, 0), FVector(-3600, -2400, 0), FVector(-3600, 1700, 0), FVector(-1700, 1700, 0),
        FVector(-1700, 1900, 0), FVector(1700, 1900, 0), FVector(1700, 1700, 0), FVector(3600, 1700, 0), FVector(3600, -2300, 0), FVector(1250, -2300, 0), FVector(1250, -1700, 0), FVector(0, -1700, 0) });
    ExProbeRoute(World, Ignore, TEXT("to_the_edge"), { FVector(-200, -2500, 0), FVector(-8000, -2500, 0), FVector(-17000, -2500, 0) });
    // The near sidewalk past the house kerb's crossing pole (campus plan only): the through path stays clear, and a line
    // along the kerb zone meets the pole (blocks=2 on CampusLayoutGround; 0 under -NoCampusLayout).
    ExProbeRoute(World, Ignore, TEXT("near_walk_through"), { FVector(2200, -4000, 0), FVector(3400, -4000, 0) });
    ExProbeRoute(World, Ignore, TEXT("near_walk_ped_pole"), { FVector(2230, -4100, 0), FVector(3030, -4100, 0) });
    // The court's wall-gap fixes: the bank strip outside each side wall, the 2 cm slit inside it (the bay between the piers
    // at Y 1950 and 2324, so no trace starts inside a pier), and the hole at the kitchen gate.
    ExProbeRoute(World, Ignore, TEXT("court_west_wall_foot"), { FVector(-1518, 1990, -362), FVector(-1518, 3790, -362) });
    ExProbeRoute(World, Ignore, TEXT("court_east_wall_foot"), { FVector(1518, 1990, -362), FVector(1518, 3790, -362) });
    ExProbeRoute(World, Ignore, TEXT("court_west_inner_slit"), { FVector(-1471, 2000, -359), FVector(-1471, 2270, -359) });
    ExProbeRoute(World, Ignore, TEXT("court_east_inner_slit"), { FVector(1471, 2000, -359), FVector(1471, 2270, -359) });
    ExProbeRoute(World, Ignore, TEXT("court_kitchen_gate_hole"), { FVector(1475, 1764, -359), FVector(1503, 1764, -359) });
}
}

void TickHouseExteriorViews(AStoryFirstPersonCharacter* Pawn)
{
    if (!Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled()) return;
    UWorld* World = Pawn->GetWorld();
    if (!World || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
    if (!PC) return;
    static FExViewState S;
    const double Now = World->GetTimeSeconds();
    if (S.World.Get() != World)
    {
        S = FExViewState(); S.World = World; S.Started = S.StepStarted = Now;
        S.Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HouseExteriorViews")));
        IFileManager::Get().MakeDirectory(*S.Dir, true);
        S.Report = TEXT("view,avg_ms,max_ms,game_ms,render_ms,rhi_ms,on_ms,off_ms\n");
        return;
    }
    if (Now - S.Started < 6.0) return;                       // let shaders and the exterior settle
    if (!S.Cam.IsValid())
    {
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ACameraActor* C = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, P);
        if (!C) return;
        if (Pawn->FirstPersonCamera) C->GetCameraComponent()->PostProcessSettings = Pawn->FirstPersonCamera->PostProcessSettings;
        C->GetCameraComponent()->SetFieldOfView(85.f);   // per view below
        C->GetCameraComponent()->bConstrainAspectRatio = false;
        S.Cam = C;
        Pawn->SetActorHiddenInGame(true);
        if (UCharacterMovementComponent* Move = Pawn->GetCharacterMovement()) Move->SetMovementMode(MOVE_None);
        // The pawn (and its gown) must not sit in front of the camera: park it high above the map like the stair survey does.
        Pawn->SetActorLocation(FVector(0, 0, 12000), false, nullptr, ETeleportType::TeleportPhysics);
        S.View = 0; S.StepStarted = Now; S.Frames = 0; S.SumMs = S.MaxMs = S.SumGame = S.SumRender = S.SumRhi = S.SumOn = S.SumOff = 0; S.NOn = S.NOff = 0; S.bShot = false;
        ExProbeAll(World, Pawn);
        ExProbeSweeps(World, Pawn);
    }
    {
        FString Filter;
        if (FParse::Value(FCommandLine::Get(), TEXT("ExteriorViewFilter="), Filter, false) && !Filter.IsEmpty())
        {
            TArray<FString> Keys; Filter.ParseIntoArray(Keys, TEXT(","));
            auto Wanted = [&](const TCHAR* Name) { for (const FString& K : Keys) if (FString(Name).Contains(K)) return true; return false; };
            while (S.View < (int32)UE_ARRAY_COUNT(ExViews) && !Wanted(ExViews[S.View].Name)) ++S.View;
        }
    }
    if (S.View >= (int32)UE_ARRAY_COUNT(ExViews))
    {
        if (FScreenshotRequest::IsScreenshotRequested()) return;
        FFileHelper::SaveStringToFile(S.Report, *FPaths::Combine(S.Dir, TEXT("frametimes.csv")));
        UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR_VIEWS done\n%s"), *S.Report);
        S.View = 1000;
        if (Now - S.StepStarted > 2.0) FPlatformMisc::RequestExit(false);
        return;
    }
    FExView V = ExViews[S.View];
    {
        // Calder terrain (master prompt step 8): a view stands on the ground as it is now. The offset is made here, at
        // capture, never in ExViews[] (whose XY also keep the planting's NoGo). Orthographic plans never move. The clock
        // tower's views ride its knoll (Lift) with it, except the far one, which keeps its spot on the frozen frat lawn and
        // only lifts its target. Every other perspective view under 6 m over today's grade (eye-level and interior views)
        // rises with the relief under its position, and its target with the relief under the target; higher (aerial) views
        // stay as written. Every offset is exactly 0 today and with -NoCalderTerrain.
        const FString ViewNamed(V.Name);
        const bool bPerspective = V.OrthoWidth <= 0.f;
        if (bPerspective && (ViewNamed.StartsWith(TEXT("detail_clocktower_")) || ViewNamed == TEXT("detail_3c_towerbase")))
        {
            const float ViewLift = HouseClockTowerSite::Lift();
            if (ViewNamed != TEXT("detail_clocktower_far")) V.Pos.Z += ViewLift;
            V.Target.Z += ViewLift;
        }
        else if (bPerspective && ViewNamed.StartsWith(TEXT("detail_perf_in_")))
        {
            // Interior comparisons move with the building's one level frame.
            // Ground under a target can be a sloping court outside the shell;
            // using it would change the camera pitch between terrain on/off.
            const TCHAR* ViewBuilding = ViewNamed == TEXT("detail_perf_in_lib") ? TEXT("LIB")
                : ViewNamed == TEXT("detail_perf_in_harlow") ? TEXT("HAR")
                : ViewNamed == TEXT("detail_perf_in_science") ? TEXT("SCI")
                : ViewNamed == TEXT("detail_perf_in_union") ? TEXT("UNI")
                : ViewNamed == TEXT("detail_perf_in_stadium") ? TEXT("STA") : TEXT("");
            const float InteriorLift = HouseCampusLayout::BlockDZ(HouseCampusLayout::Find(ViewBuilding));
            V.Pos.Z += InteriorLift;
            V.Target.Z += InteriorLift;
        }
        else if (bPerspective && ViewNamed.StartsWith(TEXT("detail_calder_macro_")))
        {
            // Front campus and the north field use different legacy grades.
            // Set the actual macro eye over local ground, rather than adding
            // relief to an assumed -60cm datum in both places.
            V.Pos.Z=HouseExteriorSite::GroundHeight(V.Pos.X,V.Pos.Y)+130.f;
            V.Target.Z=HouseExteriorSite::GroundHeight(V.Target.X,V.Target.Y)
                +(ViewNamed==TEXT("detail_calder_macro_mown_bank")?20.f:50.f);
            if (S.Frames==1) UE_LOG(LogTemp,Display,TEXT("CALDER_MACRO_CAMERA view=%s eyeGroundCm=%.2f"),*ViewNamed,
                V.Pos.Z-HouseExteriorSite::GroundHeight(V.Pos.X,V.Pos.Y));
        }
        else if (bPerspective && ViewNamed == TEXT("detail_calder_eye_P8") && HouseCalderTerrain::Enabled())
        {
            // Stand at the same 170 cm eye height on the actual ledge. The
            // landform and its stepped approach rise; the camera never floats.
            const float Rise = HouseExteriorSite::GroundHeight(V.Pos.X, V.Pos.Y) + 170.f - V.Pos.Z;
            V.Pos.Z += Rise;
            V.Target.Z += Rise;
        }
        else if(bPerspective && ViewNamed==TEXT("detail_calder_road_approach") && HouseCalderWater::Active())
        {
            V.Pos.Z=HouseExteriorSite::GroundHeight(V.Pos.X,V.Pos.Y)+170.f;
            for(const HouseCalderLandform::FWaterCrossing& Crossing:HouseCalderLandform::WaterCrossings())
                if(Crossing.Kind==HouseCalderLandform::EWaterCrossing::RoadBridge) V.Target.Z=Crossing.DeckZ+50.f;
        }
        else if (bPerspective && V.Pos.Z - Grade(V.Pos.X, V.Pos.Y) < 600.f && ViewNamed != TEXT("detail_calder_eye_P6") && ViewNamed != TEXT("detail_calder_eye_P7"))
        {
            V.Pos.Z += HouseCalderTerrain::ReliefLattice(V.Pos.X, V.Pos.Y);
            V.Target.Z += HouseCalderTerrain::ReliefLattice(V.Target.X, V.Target.Y);
        }
    }
    // These close-ups use an actual scattered instance, not a separate display
    // prop. They exist only in the opt-in screenshot sweep.
    const FString ViewName(V.Name);
    HouseCalderLandscape::WoodsPropMacro(World, FName(V.Name), V.Pos, V.Target);
    HouseCalderFarm::DetailView(World, FName(V.Name), V.Pos, V.Target);
    HouseCalderLandscape::RockDetailView(World, FName(V.Name), V.Pos, V.Target);
    HouseCalderFarmTree::DetailView(World, FName(V.Name), V.Pos, V.Target);
    HouseCalderHedgerow::DetailView(World, FName(V.Name), V.Pos, V.Target);
    const TCHAR* ItemPrefix=ViewName==TEXT("nature_pinecone_close")?TEXT("ExteriorYardPinecone"):
        ViewName==TEXT("nature_stick_close")?TEXT("ExteriorYardStick"):
        ViewName==TEXT("nature_trash_close")?TEXT("ExteriorYardCup"):nullptr;
    if(ItemPrefix)
    {
        float Nearest=FLT_MAX;FTransform Chosen;
        for(TActorIterator<AHouseExteriorDetail> It(World);It;++It)
        {
            TArray<UHierarchicalInstancedStaticMeshComponent*> Parts;It->GetComponents(Parts);
            for(auto* H:Parts)if(H->GetName().StartsWith(ItemPrefix))
                for(int32 I=0;I<H->GetInstanceCount();++I)
                {
                    FTransform X;H->GetInstanceTransform(I,X,true);
                    const float Dist=FVector::DistSquared2D(X.GetLocation(),V.Target);
                    if(Dist<Nearest){Nearest=Dist;Chosen=X;}
                }
        }
        if(Nearest<FLT_MAX)
        {
            V.Target=Chosen.TransformPosition(FVector(5,0,3));
            V.Pos=V.Target+FVector(-28,-25,48);
        }
    }
    ACameraActor* C = S.Cam.Get();
    C->SetActorLocationAndRotation(V.Pos, (V.Target - V.Pos).Rotation());
    C->GetCameraComponent()->SetFieldOfView(V.Fov > 0.f ? V.Fov : 85.f);
    {
        // Plans: orthographic, straight down, without the film look's vignette, grain and fringe.
        UCameraComponent* CC = C->GetCameraComponent();
        static FPostProcessSettings PerspectiveLook;
        static bool bSaved = false;
        if (!bSaved) { PerspectiveLook = CC->PostProcessSettings; bSaved = true; }
        if (V.OrthoWidth > 0.f)
        {
            CC->SetProjectionMode(ECameraProjectionMode::Orthographic);
            CC->SetOrthoWidth(V.OrthoWidth);
            CC->SetAutoCalculateOrthoPlanes(false);
            CC->SetUpdateOrthoPlanes(false);
            CC->SetOrthoNearClipPlane(V.OrthoNear);
            CC->SetOrthoFarClipPlane(80000.f);
            FPostProcessSettings& PP = CC->PostProcessSettings;
            PP.bOverride_VignetteIntensity = true; PP.VignetteIntensity = 0.f;
            PP.bOverride_FilmGrainIntensity = true; PP.FilmGrainIntensity = 0.f;
            PP.bOverride_SceneFringeIntensity = true; PP.SceneFringeIntensity = 0.f;
        }
        else
        {
            CC->SetProjectionMode(ECameraProjectionMode::Perspective);
            CC->PostProcessSettings = PerspectiveLook;
        }
    }
    if (PC->GetViewTarget() != C) PC->SetViewTarget(C);
    // -CleanViews: hide the story HUD so the screenshots show only the scene.
    if (FParse::Param(FCommandLine::Get(), TEXT("CleanViews"))) if (AHUD* Hud = PC->GetHUD()) Hud->bShowHUD = false;
    const double Age = Now - S.StepStarted;
    float ViewWarmup = 2.f;
    int32 ViewSamples = 45;
    FParse::Value(FCommandLine::Get(), TEXT("ExteriorViewWarmup="), ViewWarmup);
    FParse::Value(FCommandLine::Get(), TEXT("ExteriorViewSamples="), ViewSamples);
    ViewWarmup = FMath::Clamp(ViewWarmup, 2.f, 20.f);
    ViewSamples = FMath::Clamp(ViewSamples, 45, 600);
    if (Age < ViewWarmup) return;
    // -ExteriorViewCsv (master job, Phase 4): a CSV profile of each view's sampled frames (with r.GPUCsvStatsEnabled 1,
    // the GPU's passes too), one file per view in Saved/Profiling/CSV, in view order.
    static const bool bCsv = FParse::Param(FCommandLine::Get(), TEXT("ExteriorViewCsv"));
    if (bCsv && S.Frames == 0 && GEngine) GEngine->Exec(World, TEXT("CsvProfile Start"));
    if (bCsv && S.Frames == ViewSamples - 1 && GEngine) GEngine->Exec(World, TEXT("CsvProfile Stop"));
    // -ExteriorViewToggle=<actor class part>/<component name prefix>+... (master job, Phase 4): an A/B inside one run. The
    // samples alternate in six blocks, shown and hidden with bHiddenInGame (the first frames after each switch are not
    // counted); on_ms and off_ms are the two blocks' averages. Hiding many parts makes the renderer rebuild for a while
    // (Lumen's cards, the GPU scene), so this measures that churn as much as the parts: use it to compare, not to cost. Either side may be empty
    // ("HouseCampusPolish/" is all of the polish's drawn components and decals; "/CampusPolishFar" those parts anywhere).
    static const FString Toggle = [] { FString T; FParse::Value(FCommandLine::Get(), TEXT("ExteriorViewToggle="), T, false); return T; }();
    auto SetHidden = [&](bool bHide)
    {
        if (!bHide) { for (const TWeakObjectPtr<USceneComponent>& C : S.Hidden) if (C.IsValid()) C->SetHiddenInGame(false); S.Hidden.Reset(); return; }
        TArray<FString> Tokens;
        Toggle.ParseIntoArray(Tokens, TEXT("+"));
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            TInlineComponentArray<USceneComponent*> Comps(*It);
            for (USceneComponent* C : Comps)
            {
                if (!C || !C->IsVisible() || !(C->IsA<UPrimitiveComponent>() || C->IsA<UDecalComponent>())) continue;
                for (const FString& K : Tokens)
                {
                    FString A, P;
                    if (!K.Split(TEXT("/"), &A, &P)) continue;
                    if ((A.IsEmpty() || It->GetClass()->GetName().Contains(A)) && (P.IsEmpty() || C->GetName().StartsWith(P))) { C->SetHiddenInGame(true); S.Hidden.Add(C); break; }
                }
            }
        }
    };
    if (S.Frames < ViewSamples)
    {
        const double Ms = FApp::GetDeltaTime() * 1000.0;
        if (!Toggle.IsEmpty())
        {
            const int32 Block = FMath::Max(1, ViewSamples / 6), In = S.Frames % Block;
            const bool bOff = (S.Frames / Block) % 2 == 1;
            if (In == 0) SetHidden(bOff);
            if (In >= FMath::Min(8, Block / 2)) { (bOff ? S.SumOff : S.SumOn) += Ms; ++(bOff ? S.NOff : S.NOn); }
            if (S.Frames == ViewSamples - 1) SetHidden(false);
        }
        ++S.Frames; S.SumMs += Ms; S.MaxMs = FMath::Max(S.MaxMs, Ms);
        S.SumGame += FPlatformTime::ToMilliseconds(GGameThreadTime); S.SumRender += FPlatformTime::ToMilliseconds(GRenderThreadTime);
        S.SumRhi += FPlatformTime::ToMilliseconds(GRHIThreadTime);   // the RHI thread (master job, Phase 4)
        return;
    }
    if (!S.bShot)
    {
        {
            FHitResult Hit; FCollisionQueryParams QP(SCENE_QUERY_STAT(ExteriorViews), true); QP.AddIgnoredActor(Pawn);
            const bool bHit = World->LineTraceSingleByChannel(Hit, V.Pos, V.Pos + (V.Target - V.Pos).GetSafeNormal() * 20000.f, ECC_Visibility, QP);
            TArray<FOverlapResult> Ov; FCollisionShape Sph = FCollisionShape::MakeSphere(30.f);
            World->OverlapMultiByChannel(Ov, V.Pos, FQuat::Identity, ECC_Visibility, Sph, QP);
            FString Inside;
            for (const FOverlapResult& O : Ov) Inside += FString::Printf(TEXT(" %s/%s"), *GetNameSafe(O.GetActor()), *GetNameSafe(O.GetComponent()));
            UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR_VIEW %s hit=%s/%s dist=%.0f inside=%s"), V.Name, bHit ? *GetNameSafe(Hit.GetActor()) : TEXT("-"),
                bHit ? *GetNameSafe(Hit.GetComponent()) : TEXT("-"), bHit ? Hit.Distance : -1.f, *Inside);
        }
        const double N = S.Frames;
        S.Report += FString::Printf(TEXT("%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n"), V.Name, S.SumMs / N, S.MaxMs, S.SumGame / N, S.SumRender / N, S.SumRhi / N,
            S.NOn ? S.SumOn / S.NOn : 0.0, S.NOff ? S.SumOff / S.NOff : 0.0);
        UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR_FPS view=%s avg_ms=%.2f max_ms=%.2f game_ms=%.2f render_ms=%.2f fps=%.1f"), V.Name, S.SumMs / N, S.MaxMs, S.SumGame / N, S.SumRender / N, 1000.0 / FMath::Max(.01, S.SumMs / N));
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir, FString::Printf(TEXT("%02d_%s.png"), S.View, V.Name)), false, false);
        S.bShot = true;
        S.MotionFrames = 0;
        S.NextMotionShot = Now + .8;
        return;
    }
    if (FScreenshotRequest::IsScreenshotRequested()) return;
    // Optional evidence sequence runs AFTER the performance sample. Screenshot
    // readback must never contaminate the HOUSE_EXTERIOR_FPS measurement.
    int32 MotionSamples = 0;
    FParse::Value(FCommandLine::Get(), TEXT("WoodsMotionFrames="), MotionSamples);
    int32 ExteriorMotionSamples = 0;
    FParse::Value(FCommandLine::Get(), TEXT("ExteriorMotionFrames="), ExteriorMotionSamples);
    const bool bExteriorMotion = ExteriorMotionSamples > 0;
    if (bExteriorMotion) MotionSamples = ExteriorMotionSamples;
    MotionSamples = FMath::Clamp(MotionSamples, 0, 16);
    if ((bExteriorMotion || ViewName.StartsWith(TEXT("detail_woods_"))) && S.MotionFrames < MotionSamples)
    {
        if (Now < S.NextMotionShot) return;
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir,
            FString::Printf(TEXT("%02d_%s_motion_%02d.png"), S.View, V.Name, S.MotionFrames)), false, false);
        if (bExteriorMotion)
        {
            UE_LOG(LogTemp, Display, TEXT("HOUSE_EXTERIOR_MOTION_FRAME view=%s frame=%d time=%.3f"), V.Name, S.MotionFrames, Now);
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("WOODS_MOTION_FRAME view=%s frame=%d time=%.3f"), V.Name, S.MotionFrames, Now);
        }
        ++S.MotionFrames;
        S.NextMotionShot = Now + .8;
        return;
    }
    ++S.View; S.StepStarted = Now; S.Frames = 0; S.SumMs = S.MaxMs = S.SumGame = S.SumRender = S.SumRhi = S.SumOn = S.SumOff = 0; S.NOn = S.NOff = 0; S.bShot = false;
}
