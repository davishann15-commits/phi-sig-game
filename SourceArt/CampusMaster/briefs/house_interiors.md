# Brief: how the house interiors were built (model for campus interiors)

Project: `/Users/Stewart/Documents/Unreal Projects/SeniorSendoff` (UE 5.8). Paths below are relative to `Source/SeniorSendoff/`
unless they start with `SourceArt/`, `Content/`, `Config/` or `/Game/`. This survey was read-only: nothing in the project was edited or run.
Units are cm, Z up. "Authored" coordinates are pre-enlargement values. The house map is uniformly scaled ×1.1 (see §1).

---------------------------------------------------------------------------------------------------------------------

## 0. Summary

1. **There are two ways house rooms were built.**
   - **Map-placed photo rooms.** Almost every room (library, kitchen, bedrooms, bath, spa, club, Noodle wing, great room) was
     modelled in Blender background Python. Each one was imported through the GUI editor's Python with the legacy FBX importer,
     placed as `AHouseInteractiveProp`, `AStaticMeshActor` and `AExistingHouseDoor` actors, and saved into
     `Chapter01_House.umap`. The installer checks that "N other actors are unchanged" and keeps a backup .umap.
   - **Runtime C++.** A few areas are built in C++ when the level starts: the basement black hall (`HouseBlackHall*`), the hallway
     dressing, kit props and gadgets (`HouseDetail*`), the vent crawl network (`HouseVentNetwork`), the Boywick string lights, and
     the club's lights, music and post-process (`HouseNightClub`).
   - The campus job may not save the map, so it must copy the **runtime** pattern. It can still reuse the uasset meshes that the
     map-placed rooms produced.
2. **The interaction classes do not replicate their mesh or asset fields.** On the server, set `IntactMesh`/`*Mesh`,
   `OriginalLeaf` and the material overrides before `FinishSpawning`. Clients only receive the replicated state. The house handles
   this with small subclasses that replicate a *kind or style index* and rebuild the visuals in `BeginPlay`:
   `AHouseKitProp` (KitKind/KitVariant), `AHouseExitSign` (ArrowStyle) and `AHouseBlackHallDoor` (Style/Width/Flags).
   Campus props and doors must follow the same pattern, or clients will see invisible furniture and doors without leaves.
3. **HOUSE DAMAGE counts every broken actor.** The HUD shows `GetTotalBrokenCount()` (StoryCampaign.cpp:1946-1949). Every
   `AHouseInteractiveProp` and every window fixture calls `UHouseDamageSubsystem::RegisterBrokenActor`, and there is no filter.
   Fixtures always pass the category `"Window"`, so filtering by category is not enough. Keeping campus damage out needs a small
   tag check in `RegisterBrokenActor` (HouseDamageSubsystem.cpp:48-87). §5.5 gives the exact recipe.
4. **Campus interiors count as "outdoors" for the dusk grade.** `UHouseDuskAtmosphereComponent::DistanceOutsideHouse` only knows
   the house's three blocks (HouseDuskAtmosphere.cpp:233-243). Inside a campus building the camera would still get:
   - the film look at full weight (grain, teal shadows, vignette, fringe);
   - exposure −4.15 instead of the house interior's −4.5;
   - clock-tower bells that do not duck (HouseClockTower.cpp:739-744).
5. **Rooms are lit by real, short-range, mostly unshadowed lights, plus emissive fixture meshes.**
   - Map lights: 99 PointLight actors and 58 RectLight actors (installers use Lumens). The 4 exit-sign point lights, the 6 club
     spots, 1 moon and 1 sky light are separate components. Runtime code adds about 30 more.
   - The exterior budget (≤53) only counts point and spot lights **owned by `AHouseExteriorDetail`**
     (HouseWoodsPass.cpp:51-99, HouseClockTower.cpp:883-896). Campus interior lights would not trip those reviews, but each one
     still costs GPU and should be gated (§6).
6. **No CRT television, radiator, chalkboard, locker or vending machine exists anywhere in Content.**
   - Every TV and monitor is a flat panel, which is wrong for 1986.
   - Many wall props are branded or point to the real University of Tennessee (Vols, Tennessee, Bud Light, Busch, Coors, Corona,
     Red Bull, Galanz, Pulp Fiction, Action Bronson, Flavortown).
   - About 65% of the useful furniture is good 1986 stock (bentwood and spindle chairs, bookcases, loft and twin beds, gang
     bathroom, cafeteria serving line, billiards, arcade cabinet). §7 has the full list.
7. **Many photo-room meshes are stretched 1.8× horizontally** to fit the house's original XY stretch (`baked_xy_scale: 1.8` in the
   kitchen manifest; the library manifest `"scale": [1.8,1.8,1]`). At true campus scale use XY ≈ 0.556 on those meshes.
   Do not use `HouseWorldScale` on the campus: it is ×1.1 everywhere in Chapter01_House.

---------------------------------------------------------------------------------------------------------------------

## 1. Coordinates and scale primer

| Item | Value | Source |
|---|---|---|
| House blocks ("indoors" test) | X[-3400,-1420] Y[-2200,1540]; X[-1420,1360] Y[-1010,1640]; X[1360,3340] Y[-2130,1540]; Z[-420,1150] (world) | HouseDuskAtmosphere.cpp:236-239 |
| House scale | Uniform ×1.1 about the origin, gated by the WorldSettings tag `SSOHouseScale110` | HouseWorldScale.cpp:7-12, SourceArt/HouseScale110/README.md |
| Floor levels (world) | Basement -362 (authored -329). Ground 0 (door hinges at +9). Upper ≈ 332.5 (authored 302.3). Basement ceiling ≈ -48. | HallSpec in HouseBlackHallLook.h:13; door hinges in the actor snapshot |
| Legacy XY stretch | The original house chunks are placed at scale (1.8,1.8,1.0)×1.1. Doors with `bLocalLeafSpace=false` apply 1.8 to the leaf. | ExistingHouseDoor.cpp:72-87 |
| Front doors (walnut pair) | (-176,-989,2) and (102,-989,2). The house front faces -Y, toward Fraternity Park Dr and the campus. | Actor snapshot |
| PlayerStart | (1184,624,101). FallLimit = Z −1099. | StoryCampaign.cpp:1757-1758 |

Approximate world extents of the photo rooms. These come from prop bounds in `SourceArt/HouseScale110/before/actors.json`
multiplied by 1.1. Many actors sit at (0,0,0) with world-baked vertices, so treat the boxes as rough.

| Room | Approx. world box | Floor Z |
|---|---|---|
| BasementLibrary | X -3300…-2600, Y -1500…+1000 | -362 |
| Basement black hall (runtime) | X ±1346, Y ±871 (authored ±1224/±792) | -362, ceiling -48 |
| NoodleWing (billiards / TV lounge / laundry / stair) | X ≈ 1900…3200, Y -1570…+1160 | -362 / 0 / 332 |
| ReferenceKitchen | X ≈ 2000…3200, Y -730…+1380 | 0 |
| GreatRoom V3 | X -1083…+1362, Y -345…+924 | 0 |
| Spa / HotTubRoomV1 | X -3430…-2543, Y -361…+674 | 0 |
| Former Room102 (now club lounge) | X -2742…-2164, Y -2140…-1477 | 0 |
| NightClub main rectangle (both floors) | Authored X[-2212.5,-1490] Y[-1509,-973.5] → world X[-2434,-1639] Y[-1660,-1071] | 0 / 332 |
| Boywick bedroom | X -3314…-2586, Y -1531…-626 | 332 |
| AcrossPoster bedroom (BR_) | X -3358…-2499, Y 476…1492 | 335 |
| Room204 | X -3317…-2549, Y -595…+511 | 332 |
| Upstairs bathroom | X -2246…-1451, Y -504…+862 | 333 |

---------------------------------------------------------------------------------------------------------------------

## 2. Room and area inventory: how each was built and how it is tested

Legend:
- **MAP**: placed in the map by an editor Python installer and saved.
- **RT**: built in C++ at runtime.
- **Flag**: the `-Switch` that runs the opt-in check.
- **Marker**: the log line that means PASS.

| Area (SourceArt/…) | Build | Content folder | Runtime C++ involved | Check: flag → marker / output | Status, notes |
|---|---|---|---|---|---|
| **BasementLibrary** | MAP. 157 placed objects and 151 interactive. Tags `SSOLibraryRebuild`, `SSOLibrarySmallItem`, `LibraryPhotoShell`. Ceiling panels tagged `LibraryCeilingPanel`. | /Game/SeniorSendOff/House/BasementLibrary (676 assets) | Vent portals use the ceiling panels (§5.6) | `-HouseLibrarySmoke` → `LIBRARY_CHECK_PASSED` / `Saved/LibraryCheck/result.json` (HouseLibrarySmoke.cpp:60). Optional `-LibraryEvidence=` | Current. Pipeline: build_textures → build_library → build_geometry → polish → build_damage → install_library (README) |
| **ReferenceKitchen** | MAP. 137 props, 69 families, damage V2. Tags `SSOKitchenRefit`, `KitchenVisualOnly`, `KitchenPhotoShell`. Locked closet door `ReferenceKitchen_Closet`. | /Game/SeniorSendOff/House/ReferenceKitchen | — | `-HouseKitchenSmoke` → `KITCHEN_CHECK_PASSED` / `Saved/KitchenCheck/result.json` (HouseKitchenSmoke.cpp:58-177) | Current. Template for an inventory, pickup and damage-stage smoke (§4) |
| **BoywickBedroom** | MAP: 119 objects, 110 interactive, `BWR_` labels, tag `BoywickPhotoRoom`. RT: string lights. | /Game/SeniorSendOff/House/BoywickBedroom | `ABoywickRoomDecor` (BoywickRoom.cpp:262-297); NPC `BoywickCharacter` | `-BoywickRoomCheck`, `-BoywickSmoke`, `-BoywickCheck` (BoywickRoom.cpp:341,391,397); `HouseVisualReview` | Current. Furniture fitted to the 1.8 XY stretch |
| **AcrossPosterBedroom** (BR_) | MAP via `apply_bedroom.py` (Lumens lights) | /Game/SeniorSendOff/House/AcrossPosterBedroom | — | Views only | Current. Branded wall art (Vols, Texas, Corona) |
| **Room204** (+Revision2) | MAP via `Content/Python/apply_room204.py`, `refine_room204.py` | /Game/SeniorSendOff/House/Room204 | — | Views | Current. Branded posters |
| **UpstairsBathroom** | MAP via `apply_upstairs_bathroom.py` | /Game/SeniorSendOff/House/UpstairsBathroom (interactive copies HI_UpBath_* in Interactivity) | — | Views | Current. Gang-bathroom kit |
| **HotTubRoomV1** (spa) | MAP (`build_spa_room.py`, `apply_spa_room.py`) and a placed `AHouseHotTubWater` | /Game/SeniorSendOff/House/HotTubRoomV1 | HouseHotTubWater.cpp | `-HouseHotTubWaterSmoke` | Current |
| **PigstyRoom** (HallEnd_West, upstairs) | MAP, plus placed `AHouseRoamingPig` ×2 and `AHousePigSnackPile` | /Game/SeniorSendOff/House/PigstyRoom | HouseRoamingPig.cpp (pigs respect door IDs) | `-HousePigSmoke` (and Stair/Upgrade/Motion/Animal variants) | Current. **It replaced the PokerRoom.** |
| **PokerRoom** | Was MAP (`apply_room.py`) plus `AHousePokerTable` | /Game/SeniorSendOff/House/PokerRoom | HousePokerTable.cpp | `-HousePokerSmoke` (no table is in the map any more, so it is stale) | **Retired.** The SM_POK_* assets are not referenced, so they are not cooked |
| **NightClub** (+ wrap_refit, connection_polish, hall_conversion, exterior_fill, lounge, lighting) | MAP geometry and props (`NC*`, `NCR*`, `NCL*`, `CP*`, `NCH*`) plus a placed `AHouseNightClub` and `AHouseClubPole` | /Game/SeniorSendOff/House/NightClub | HouseNightClub.cpp (6 spots, room-local post-process, music zones); HouseClubPole.cpp | `-NightClubSmoke` → `NIGHT_CLUB_SMOKE_PASSED:`; `-NightClubEntranceSmoke`; `-HouseClubPoleSmoke`, `-ClubPoleMultiplayerSmoke`, `-ClubHallWalkReview`, `-ClubHallNetworkReview` | Current. Absorbed Room102 and the former impossible hallway |
| **Room101Entry** | MAP: two door leaves on `AExistingHouseDoor` (`install.py`, `door_manifest.json`) | /Game/SeniorSendOff/House/Room101Entry | — | `-Room101EntrySmoke` → `ROOM101_ENTRY_PASSED:` plus views (SourceArt/Room101Entry/verify.py) | Current |
| **Room102Chairs** | Was MAP (`apply_room102.py`; 52 storage chairs) | /Game/SeniorSendOff/House/Room102Chairs | — | — | **Absorbed into the club.** Chairs and fixtures removed, so not cooked |
| **NoodleWing** (billiards, TV lounge, laundry, 3-floor stair) | MAP: 122 actors with `NW_` prefix, 18 lights, 2 doors. Tags `NoodleWing`, `NoodleWingStairwell`. | /Game/SeniorSendOff/House/NoodleWing | HouseBlackHallBuild.cpp drops dead-end bounds when the tag exists | `-NoodleWingSmoke` (dispatched in HouseCarryTiersReview.cpp:822; markers `NOODLE_WALK_PASS`) | Current. Its props have **no damage stages** |
| **GreatRoom / V2 / V3 / Corner / TVs / Collision** | MAP via Content/Python/apply_full_great_room.py, apply_great_room_polish.py and others. TVs tagged `HouseWallTelevision`. | GreatRoom*, GreatRoomTVs | — | `-HouseTelevisionSmoke` → `HOUSE_TV_CHECK_PASSED`; `-GreatRoomCollisionReview`; pillow/cushion reviews | Current. V3 seating pieces are `HI_SM_GR3_*` |
| **HallFloorsPorch** | MAP (`install.py`, tag `HallFloorsPorch`) | /Game/SeniorSendOff/House/HallFloorsPorch | — | Views | Current |
| **StairwellWall** | MAP (repaired meshes in StairwellRepair) | /Game/SeniorSendOff/House/StairwellRepair | — | `-HouseStairSurvey -StairWallWalk` | Current |
| **ExitSigns** (+NeonV2) | MAP: `AHouseExitSign` actors (4) plus one RT exit sign in the black hall | /Game/SeniorSendOff/House/ExitSigns (**DirectoriesToAlwaysCook**) | HouseExitSign.cpp (point light per sign) | `-HouseExitSignSmoke` | Current. Good runtime-spawn example (§5.2) |
| **HallwayEnd** | MAP (three-room shell, doors HallEnd_West/End/East) | /Game/SeniorSendOff/House/HallwayEnd | — | — | Current |
| **ImpossibleHallway** | Was MAP plus a placed `AHouseImpossibleHallway` | /Game/SeniorSendOff/House/ImpossibleHallway | HouseImpossibleHallway.cpp | `-HouseImpossibleHallwaySmoke` (stale) | **Removed** by NightClub/hall_conversion |
| **Basement black hall** | **RT.** `UHouseBlackHallSubsystem` clips the original shell, builds procedural architecture, murals and litter, and spawns 10 doors | Materials are dynamic instances of M_House_* | HouseBlackHall.cpp, HouseBlackHallBuild.cpp, HouseBlackHallLook.cpp, HouseBlackHallLitter.cpp, HouseBlackHallPaint.cpp | `-HouseBlackHallReview`, `-HouseBlackHallSurvey` (pawn tick). `-NoBlackHallRebuild` turns it off | **Best model for a runtime room** |
| **Hallway detail and kit props** | **RT.** `UHouseDetailSubsystem` dresses hallways (traced walls), spawns 29 `AHouseKitProp` and gadgets, and applies light culling | — | HouseDetailBuild.cpp, HouseDetailKit.cpp, HouseDetailProps.cpp, HouseGadgets.cpp | `-HouseGadgetSmoke`, `-HouseDetailSurvey`. `-NoHouseDetail` turns it off | **Best model for runtime interactive props** |
| **Vent crawl network** | **RT.** Spawned by `AStoryFirstPersonCharacter` on the server | /Game/SeniorSendOff/House/VentNetwork (1 material) | HouseVentNetwork.cpp | `-HouseVentSmoke` → `HOUSE_VENT_SMOKE_PASSED`; `-VentMultiplayerSmoke`; `-HouseVentVisualReview` | §5.6 |
| **HouseDoors / DoorRepair / DoorSwing / FrontDoubleDoors** | MAP via Content/Python/build_existing_house_doors.py, using Content/HouseReview/original_house_doors.json | /Game/ExistingHouseDoors, DoorRepair, FrontDoubleDoors | ExistingHouseDoor.cpp | `-ExistingHouseDoorSmoke` → `EXISTING_HOUSE_DOOR_PASSED:`; `-DoorSwingReview`, `-DoorSwingAudit`, `-DoorSwingNetworkReview`; `-FrontDoubleDoorReview` | §5.1 |
| **HouseInteractivity** (damage V2) | Offline: converted ~600 original house pieces into interactive props with 4 damage stages and UCX hulls | /Game/SeniorSendOff/House/Interactivity (3305 assets, HI_*) | — | `-HouseInteractivitySmoke`, `-HouseGlassSmoke`, `-HouseChairSmoke`, `-HouseFurnitureCarrySmoke`, `-HouseCarryTiersReview` | SourceArt/HouseInteractivity/Damage-V2.md |
| **HouseCleanup2 / HouseJoinRepair / ShelfFireplaceRefinement / HouseLetterRemoval** | MAP repair passes | HouseCleanup2, HouseJoinRepairs, FireplaceRefinement | — | `-HouseCleanupWalkReview`, `-HouseJoinWalkReview` | Current |
| **HouseScale110** | MAP-wide ×1.1 enlargement (`resize.py` from the `before/actors.json` snapshot) | — | HouseWorldScale.cpp | `verify_runtime.py` | Explains the ×1.1 |

### 2.1 The SourceArt folder workflow used by every photo room

```
references/        user photos (IMG_####) — or context/ for exported shell neighbours
before/            Chapter01_House.umap backup + touched C++ snapshots (+ actors.json snapshot)
source/ or *.py    Blender background builders (build_*.py, build_damage.py) + Unreal editor installers (install*.py, apply_*.py)
models/ textures/  FBX (with UCX_ hulls, 4 damage stages) and PNGs
manifest.json      assets[] {asset_name, mass_kg, break_health, movable, wall, baked_xy_scale, *_asset_name for each stage, material slots}
                   placements[] {asset, name, location, yaw, scale}
installed*.json    what the installer changed. "N unrelated actors preserved" audit
views*.json        HouseVisualReview camera layouts. walk-route*.json for walking checks
evidence/ validation/ review/  logs, result.json, screenshots (pass = explicit log marker, never the exit code)
README.md          what changed, how to regenerate, verified behaviour, FPS table
```
Constraints the READMEs repeat:
- FBX import needs the **GUI editor**; the commandlet crashes on the smoothing warning (BoywickBedroom README).
- Installers are idempotent and use tag or prefix protection (`SSOLibraryRebuild`, `NW_`, `BWR_`).
- They never edit shared house meshes; they import cut copies instead.
- Do not rerun old installers over later passes (NightClub README).

**For the campus job:** saving the map is forbidden, so this pipeline becomes:
1. C++ builders (`FHallMesh` → `UProceduralMeshComponent`, or `HwBuildStaticMesh`).
2. `LoadObject` reuse of the existing uassets.
3. Server-spawned replicated interactive actors.
4. A `-<Area>Smoke` plus `-HouseVisualReview -HouseVisualLayout=<views.json>` evidence folder modelled on the above.

---------------------------------------------------------------------------------------------------------------------

## 3. The runtime-room pattern (black hall and hallway detail)

- **World subsystem, gated to this map.**
  - `UHouseBlackHallSubsystem::OnWorldBeginPlay` (HouseBlackHall.cpp:48-95) returns unless the level is
    `Chapter01_House`, and checks a `-NoX` opt-out (line 51-52).
  - It spawns a `NotPlaceable, Transient` cosmetic actor **on every machine** with `RF_Transient`
    (`AHouseBlackHallDetail`, HouseBlackHall.h:80-100).
  - Replicated gameplay actors (doors, litter) are spawned **only when `NetMode != NM_Client`** (HouseBlackHall.cpp:64-66).
  - `UHouseDetailSubsystem::OnWorldBeginPlay` (HouseDetailProps.cpp:461-512) does the same for hallway detail, kit props and
    gadgets (line 501).
- **Geometry.**
  - `FHallMesh` (HouseBlackHallMesh.h:33-258) accumulates per-material sections using Box, BoxMinMax, Cylinder, Dome, Prism and
    Quad.
  - `Build()` writes them into a `UProceduralMeshComponent` (complex collision when requested).
  - Materials come from `Sub->Mat(Key)`, which are dynamic instances of `/Game/SeniorSendOff/House/Materials/M_House_Textured`
    or `M_House_Plain` (and `M_House_Glass`).
    - Parameters: `BaseMap`, `NormalMap`, `UVScale`, `Roughness`, `Metallic`, `NormalStrength`, `Tint`, `Emission`.
    - Helpers: HouseBlackHallLook.cpp:45-79.
  - Painted signage and murals are CPU-painted textures: `FHallPanelSpec` and `HallMakePanelMaterial` (HouseBlackHallLook.h:33-56).
    This is the right tool for Calder State purple/gold posters, pennants, room plaques and bulletin boards.
- **Runtime interactive meshes.**
  - `HwBuildStaticMesh` (HouseDetailProps.cpp:308-372) turns an `FHallMesh` into a transient `UStaticMesh` with **box simple
    collision** (`CTF_UseSimpleAsComplex`). That is the only way to make procedural furniture carryable (§5.2).
  - `AHouseKitProp` (HouseDetail.h:27-40; HouseDetailProps.cpp:377-436) replicates `KitKind`/`KitVariant` `COND_InitialOnly` and
    rebuilds the mesh locally in `BeginPlay`.
  - Placement uses a floor line trace (HouseDetailProps.cpp:706-750).
- **Traced dressing.** `AHouseHallwayDetail` (HouseDetailBuild.cpp:1-120) traces the real walls, skipping doors, props and
  fixtures (`HwIgnored`, line 33-36). It reads the wall paint tint so outlets, switches, signs and conduit sit on surfaces.

---------------------------------------------------------------------------------------------------------------------

## 4. How the smoke tests are launched

- **Dispatch.**
  - World smokes are called from `AStoryGameMode::Tick` (StoryCampaign.cpp:1811-1836; declarations 70-93). The game mode ticks
    at **4 Hz** (`TickInterval 0.25`, StoryCampaign.cpp:344-345). Guarded by `#if !UE_BUILD_SHIPPING`.
  - `TickHouseVisualReview` also chains the world reviews: JoinRepair, Cleanup, DoorSwing, ClubHall, Campus, Woods, WestField,
    ParkingLot, FreeRoam (HouseVisualReview.cpp:155-166).
  - Pawn smokes run from the character tick inside **`#if WITH_EDITOR`** (StoryCampaign.cpp:605-614), via the dispatcher
    `TickHouseCarryTiersReview` (HouseCarryTiersReview.cpp:818-841). They run in `UnrealEditor -game` but **not** in a packaged
    Development build.
  - Each smoke gives itself an isolated save slot in `UStoryCampaign::Init` (StoryCampaign.cpp:95-135), so tests never touch the
    real campaign save.
- **Command** (Mac, from SourceArt/Room101Entry/verify.py and the user's notes):
  ```
  /Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor \
    "<proj>/SeniorSendoff.uproject" /Game/Story/Maps/Chapter01_House -game -windowed -ResX=1280 -ResY=720 \
    -RenderOffScreen -nosound -UserDir=/tmp/<isolated> -AbsLog=<log> -HouseKitchenSmoke
  ```
  - Judge PASS by the marker in the log (`KITCHEN_CHECK_PASSED`, `LIBRARY_CHECK_PASSED`, `ROOM101_ENTRY_PASSED:`,
    `NIGHT_CLUB_SMOKE_PASSED:`, `HOUSE_TV_CHECK_PASSED`, `EXISTING_HOUSE_DOOR_PASSED:`, `HOUSE_VENT_SMOKE_PASSED`) or by
    `<UserDir>/Saved/<Check>/result.json|txt`. Exit codes are ignored on Mac.
  - Build first with `/Users/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh SeniorSendoffEditor Mac Development
    -Project=... -WaitMutex -NoHotReloadFromIDE`.
- **Captures.** Use `-HouseVisualReview -HouseVisualLayout=<views.json> -HouseVisualQuality=1`.
  - The layout's `views[] {name, position{x,y,z}, target{x,y,z}, fieldOfView}` are in **metres, Y-up**, and map to UE as
    `(z*100, x*100, y*100)` (HouseVisualReview.cpp:107-116).
  - Marker: `HOUSE_VISUAL_REVIEW_PASSED`. Output: `Saved/HouseVisualReview/`.
- **Smoke template** (HouseKitchenSmoke.cpp:58-177). Phase machine with a 60 s timeout:
  1. Freeze input.
  2. Inventory by tag.
  3. Fail if any untouched prop is simulating or ticking.
  4. Fail if any stage mesh lacks hulls or materials (line 88-93). This checks `ConvexElems`; a runtime `HwBuildStaticMesh` prop has
     **BoxElems**, so a campus smoke must accept boxes.
  5. Locked door blocks the capsule and refuses `Interact`.
  6. Real `FocusedFurnitureForInteraction` plus `ToggleGrab` with two hand contacts.
  7. Three `TakeDamage` steps through each damage stage.
  8. Broken body settles below 20 cm/s.
  9. Write JSON and `RequestExitWithStatus`.
- **Shared-iteration hazard.** Several existing checks iterate **all** `AExistingHouseDoor` and `AHouseInteractiveProp` in the world.
  - `ExistingHouseDoorSmoke` fails on any door with an empty or duplicate `DoorId` or **`OriginalLeaf == nullptr`**
    (ExistingHouseDoorSmoke.cpp:328-352). Runtime black-hall doors already have no leaf.
  - It also requires `Total >= 39`, `Porch == 8`, and runs a house-wide close pass over every unlocked door.
  - `-DoorSwingAudit` swings every unlocked door (HouseDoorSwingReview.cpp:67-68).
  - Campus doors and props therefore need a unique `DoorId` prefix (e.g. `Campus_`) and a tag. Either the campus spawner should
    stay off under those house-only flags, or those checks should skip the tag. Otherwise existing reviews change.

---------------------------------------------------------------------------------------------------------------------

## 5. Interaction systems and how to use them on runtime-built content

### 5.1 `AExistingHouseDoor` (ExistingHouseDoor.h / .cpp)

**Configuration fields (h:39-76).**
- `OriginalLeaf`, `OriginalGlass`, `AddedHandleMesh`, `AddedHandleWorldPosition`/`Rotation` are *not replicated*.
- Replicated `COND_InitialOnly` (cpp:339-354): `DoorwayCenter`, `DoorId`, `ClosedYaw`, `OpenYaw`, `DoorGroup` (paired leaves open
  together, cpp:428-446), `PanelCenterWorld`/`PanelHalfExtent` (moving box blocker), `bLocalLeafSpace`, `SourceGeometryOriginWorld`.
- `bLocked` and `Swing` (FHouseDoorSwing: from/to/start/duration/revision) replicate continuously, so unlocking at runtime
  replicates.

**Leaf space.**
- With **`bLocalLeafSpace = true`** (use this on the campus), leaf, glass, handle and panel values are hinge-local (cpp:63-71).
- With `false`, the leaf gets the legacy `(1.8,1.8,1)` stretch and world-baked vertices (cpp:72-87). Avoid `SM_OriginalLeaf_*` for
  that reason.

**Behaviour.**
- The leaf blocks; if `PanelHalfExtent.X > 1` a box blocker replaces leaf collision (cpp:159-191).
- The swing stops at real furniture: it does half-degree arc sweeps (`ClearSwingYaw`, cpp:318-337) and ignores static architecture
  contacts recorded in `PrepareSwing`.
- It refuses to close on a pawn standing in the opening (cpp:395-417).
- It ticks only while moving.

**Interaction path.**
1. The HUD prompt reads `LOCKED`, `E OPEN DOOR` or `E CLOSE DOOR` (StoryCampaign.cpp:2096-2101).
2. Focus: within 270 cm of `DoorwayCenter`, facing ≥0.45 (≥0.78 for LMB double-click), with a clear line of sight
   (StoryCampaign.cpp:1179-1221).
3. Press: `InteractWithExistingDoorOrFurniture` (StoryCampaign.cpp:1416-1466) → `InteractWithExistingDoor` (1267-1281).
   - **If `bLocked`, the client shows `Story->Notify("Locked: this area is not playable yet.")` locally and returns true.**
   - Otherwise the server calls `Interact`. The server RPC `ServerInteractExistingDoor` re-validates focus (1486-1496).
4. `Interact` itself refuses when locked, when the player is over 270 cm away, or when the opening is occupied (cpp:419-426).
5. LMB double-click within 0.36 s also toggles unlocked doors (StoryCampaign.cpp:1314-1331).

**Runtime spawn recipe** (copy `UHouseBlackHallSubsystem::SpawnDoors`, HouseBlackHall.cpp:97-139):
```cpp
auto* Door = World->SpawnActorDeferred<AMyCampusDoor>(AMyCampusDoor::StaticClass(), HingeXf, nullptr, nullptr,
                                                      ESpawnActorCollisionHandlingMethod::AlwaysSpawn);   // server only
Door->Setup(Style, Width, Height, RoomSign, Flags);   // sets bLocalLeafSpace, PanelCenterWorld/HalfExtent, ClosedYaw=0, OpenYaw=±92
Door->DoorwayCenter = Center;  Door->DoorGroup = PairName_or_None;  Door->DoorId = "Campus_<Bldg>_<n>";  Door->bLocked = !bPlayable;
Door->FinishSpawning(HingeXf);
```
- `AHouseBlackHallDoor` (HouseBlackHall.h:27-51; .cpp:430-563) is the model.
  - It sets `bLocalLeafSpace=true` in its constructor.
  - It replicates `StyleValue`/`LeafWidth`/`LeafHeight`/`RoomSign`/`Flags` `COND_InitialOnly`.
  - In `BeginPlay` it builds a procedural leaf: French glazed, steel exit with push bar, or equipment with sign. It adds kick plate,
    closer, lever, deadbolt, flush bolts and hinge knuckles.
  - The visual leaf is QueryOnly/Visibility for focus traces, while `PanelCollision` blocks the pawn and physics bodies.
- **Do not apply `HouseWorldScale` to campus doors.** The black hall does because it lives in the scaled house.
- If you want a mesh leaf (`SM_NW_FireDoor`, `SM_FD_WalnutLeaf`, `SM_R101Entry_Door101Leaf`, all hinge-pivot):
  - Replicate a style index.
  - Set `OriginalLeaf`/`AddedHandleMesh` in an override of `BeginPlay` **before** `Super::BeginPlay()`, because
    `AExistingHouseDoor::BeginPlay` re-runs `OnConstruction` (cpp:95-108).
  - Use the `AHouseExitSign` ApplyStyle-then-Super pattern (HouseExitSign.cpp:47-49).
- Glass leaves.
  - When `OriginalGlass` is set and a panel exists, the door spawns a breakable `AHouseReactiveFixture` window owned by the door
    (cpp:356-378).
  - It counts as **HOUSE DAMAGE "Window"** unless filtered (§5.5).

### 5.2 `AHouseInteractiveProp` (HouseInteractiveProp.h / .cpp)

**Root and tags.**
- One replicated actor; the root is `Body` (UStaticMeshComponent), `BlockAllDynamic`, initially **not simulating, not ticking**
  (cpp:101-128).
- It adds the tags `SSOMovableFurniture` and `HouseBreakable` (cpp:24-28,124-125). The weapon's LMB "throw at scenery" uses
  `HouseBreakable` (StoryCampaign.cpp:1302-1311).

**Configuration** (h:38-63; set on the server before `FinishSpawning`):
- `IntactMesh`, `DamagedMesh`, `HeavilyDamagedMesh`, `BrokenMesh` (stage by health fraction: >2/3 intact, ≤2/3 damaged,
  ≤1/3 heavy, broken; cpp:371-407).
- `MaterialOverrides`, matched by slot name across stages (cpp:52-96).
- `bMovable`, `bBreakable`, `bWallMounted` (the first hit detaches it), `bBrokenVersionMovable`, `BreakHealth`, `DetachHealth`,
  `MassKg`, `ImpactBreakSpeed`, `CarrySize`, `DamageCategory` (default "Furniture").
- Auto behaviours:
  - Cloth for names containing flag, banner, pennant or tapestry, when thin (cpp:227-246).
  - Casters for officechair, swivel or gamingchair (cpp:218-225).
- If there is no `BrokenMesh`, a broken prop is squashed to (1,.94,.38) (cpp:442-445). That is what the stage-less NoodleWing props
  do.

**Replicated state only** (cpp:347-356): `bBroken`, `DamageStage`, `bPhysicsActive`, `bDetachedFromWall`, `Health`,
`AttachmentHealth`.
- **Meshes and tuning are not replicated.** A server-spawned plain `AHouseInteractiveProp` therefore arrives on clients with no
  mesh.
- Use a subclass that replicates a catalog index (`AHouseKitProp`, HouseDetailProps.cpp:404-436) or a style byte
  (`AHouseExitSign`, HouseExitSign.cpp:30-49,73). It applies meshes in `BeginPlay` before `Super::BeginPlay()`.

**Physics.**
- Collision must be **simple** (convex UCX or box elements). Complex-as-simple logs `HOUSE_PROP_INVALID_DYNAMIC_COLLISION` and will
  not simulate properly (cpp:466-477).
- Imported damage-V2 assets carry 1–12 UCX hulls per stage. `HwBuildStaticMesh` gives box elements.
- Tags `SSOLibrarySmallItem`/`SSOBoywickSmallItem` make a prop ignore pawns, so clutter cannot snag capsules (cpp:455-461).

**Spawn examples.**
- Exit sign: HouseBlackHallBuild.cpp:431-441 (`SpawnActorDeferred` → `Configure` → `FinishSpawning`).
- Kit props: HouseDetailProps.cpp:706-750.
- Bare prop: HouseInteractivitySmoke.cpp:126-143 and HouseFurnitureCarrySmoke.cpp:162-190, which copy the tuning and materials
  from a reference placed prop.

**Performance rule.** Untouched props stay parked: no simulation, no tick. They wake on E, a hit, or a pawn bump
(StoryMovementComponent.cpp:438-452). Kitchen and library smokes fail if any untouched prop simulates or ticks.

### 5.3 `UHouseFurnitureInteractionComponent`: what makes something carryable

A replicated default subobject on the story pawn (h:13-20). Focus and grab (cpp:370-395, 549-600) require:
- A non-complex `ECC_Visibility` line trace within **GrabRange 260 cm** hits a component.
- The owner actor has tag **`SSOMovableFurniture`** (auto-managed by `AHouseInteractiveProp::ApplyAppearance` when `CanBeMoved()`).
- The hit component **is the actor's root**.
- `CanBeMoved()` = `bWallMounted || bDetachedFromWall || (bBroken ? bBrokenVersionMovable : bMovable)` (HouseInteractiveProp.cpp:358-361).
- The body is not tagged `SSOFurnitureHeld` (a server reservation).
- A real surface point is found on the body (`ResolveFurnitureSurface`, cpp:28-58).
- `WakeForInteraction` succeeds, so the body starts simulating.

Handling size comes from `CarrySize`, or automatically (cpp:257-270):
- **Large** (lift one side and drag): mass ≥65 kg, footprint half-extent >110, or >75 and ≥35 kg.
- **Small** (one hand, sprint, long throw): ≤12 kg, ≤40 footprint, ≤45 height.
- Otherwise **Medium** (two hands, short toss).

HUD hints are at StoryCampaign.cpp:1955-1963. A held prop takes E priority over doors; furniture under the reticle beats a door's
wider cone (E chain, StoryCampaign.cpp:1416-1466).

### 5.4 `AHouseReactiveFixture`: curtains, chandeliers and windows (HouseReactiveFixture.h / .cpp)

**Configuration.** Kinds are Curtain, Chandelier and Window (h:17-23).
- `OriginalMesh`, `MaterialOverrides`, `MeshTransform`, `MotionPivotLocal`, `GlassBreakingDamage` (default 24) and the optional
  `BreakawayFrameMesh` **all replicate** (cpp:182-197).
- Replicated mesh references only work for **asset** meshes (cooked uassets or /Engine/BasicShapes). A transient
  `HwBuildStaticMesh` mesh cannot be referenced over the network.
- The hit volume is a box fitted to the mesh bounds. Windows also block pawns (cpp:108-143).

**Behaviour.**
- E on a curtain toggles it; E on a chandelier nudges it. Window prompt: "THROW TO BREAK GLASS" (cpp:212-238).
- Weapon point damage is capped at 8 per hit (cpp:340-355). Thrown physics bodies apply `Speed*.045 + Impulse*.001` (cpp:397-411).
- When accumulated damage reaches `GlassBreakingDamage`, `BreakGlass` calls
  **`RegisterBrokenActor(this, "Window", Instigator)`** (cpp:413-423). The category is hard-coded.
- Glass shards are local instanced cubes with the material `/Game/SeniorSendOff/House/Interactivity/M_WindowShard`.
- Tags: `HouseReactiveFixture`, `HouseBreakable`.

**Gotcha.**
- `AHouseExteriorDetail::MeasureConnections` (HouseExterior.cpp:447-483) treats **every owner-less window fixture in the world** as
  a house wall opening when the exterior is built (lines 458-463).
- Campus window fixtures must have an `Owner` (the building actor or door), or be spawned after the exterior build. Otherwise they
  risk carving the house shell.

### 5.5 `UHouseDamageSubsystem`: counting, and keeping campus damage out

How it counts:
- `RegisterBrokenActor(Actor, Category, Instigator)` (HouseDamageSubsystem.cpp:48-87) works on the server only, once per actor (a
  `CountedActors` set).
- It creates one replicated `AHouseDamageState` (bAlwaysRelevant) holding `TotalBrokenCount` plus per-category counts.
- Log line: `HOUSE_DAMAGE_COUNT actor=… category=… total=…`.

Callers:
- `AHouseInteractiveProp` when cloth is torn (cpp:257-259), when unhooked from the wall (cpp:581-582) and when broken (cpp:799-800),
  each with `DamageCategory`.
- `AHouseReactiveFixture::BreakGlass` with `"Window"`.

The HUD (chapter 1 only) shows `HOUSE DAMAGE | %d BROKEN` from **`GetTotalBrokenCount()`**, which is the sum of all categories
(StoryCampaign.cpp:1946-1949).

**Recipe** (minimal, keeps every house review identical):
1. Tag every campus actor that can break with `SSOCampusDamage`: props, wall-mounted props, campus window fixtures and the doors
   that own glass. Set the tag before `FinishSpawning`.
   Also set `DamageCategory = "Campus"` (or `"CampusWindow"`) on campus props for readable logs.
2. In `RegisterBrokenActor`, right after the existing guard at line 51-53, add:
   ```cpp
   const AActor* Owner = BrokenActor->GetOwner();
   if (BrokenActor->ActorHasTag(TEXT("SSOCampusDamage")) || (Owner && Owner->ActorHasTag(TEXT("SSOCampusDamage"))))
   {
       CountedActors.Add(BrokenActor);
       ++CampusBrokenCount;   // new transient (or replicated on AHouseDamageState if a campus HUD line is wanted)
       UE_LOG(LogTemp, Display, TEXT("CAMPUS_DAMAGE_COUNT actor=%s category=%s campus=%d"), *BrokenActor->GetName(), *Category.ToString(), CampusBrokenCount);
       return true;
   }
   ```
   Callers ignore the return value. The owner check covers door-owned glass, because `CreatePorchGlassFixture` spawns with
   `Owner = door`.
3. Leave `AddCameraImpact` and debris as they are; they are cosmetic.
4. Review assertion: breaking a campus prop must leave `GetTotalBrokenCount()` unchanged.
   HouseTreesReview.cpp:110,232 already asserts this pattern for leaves.

### 5.6 `AHouseVentNetwork`: crawlable vents (HouseVentNetwork.h / .cpp)

How it works:
- One replicated actor (`bAlwaysRelevant`). The server spawns it when the pawn first enters chapter 1, with the house transform
  (StoryCampaign.cpp:1763-1772).
- Each machine builds the same procedural duct in `BeginPlay`:
  - 108 cm cells, 72 cm clear height, at local `ServiceOrigin (0,0,-900)`, which is world Z ≈ −990 after ×1.1 (h:55-64).
  - The route grid is X −6…6 by Y −2…5 (`IsRouteCell`, cpp:74-82).
  - It adds 10 unshadowed point lights (cpp:430-445).
- **Portals are a hard-coded static table** of six facade positions (cpp:28-56).
  - Each is enabled only if a *saved* facade exists: the pig vent mesh, the rebuilt black hall, or tagged library panels
    (`HasSavedFacade`, cpp:84-108).
  - `ActivePortalMask` is a `uint8`, so there are at most 8 portals.
- Interaction:
  - Focus is within 270 cm with line of sight (cpp:320-352).
  - The server checks it is not carrying a pig or furniture, fades the camera for 0.12 s, teleports and forces crawl
    (`SetVentCrawlForced`), then fades back (cpp:552-600).
  - E-chain priority is just after the club pole (StoryCampaign.cpp:1424-1429). The HUD is at 2131-2135.

To reuse it on the campus, refactor first:
- Portals and route need to be per-instance data, e.g. a `TArray<FPortal>` plus a cell list, or a subclass overriding virtual
  accessors.
- `FocusedVentNetwork` already loops over all networks (StoryCampaign.cpp:1468-1479).
- **But the fall-limit exemption checks only the first network found** (StoryCampaign.cpp:1840-1846). FallLimit is Z −1099.
  Either keep campus ducts above −1099 world Z, which is easy since campus grade is about −60, or make the respawn check loop over
  all networks.

### 5.7 Other runtime pieces worth reusing

- **`AHouseGadget`** (HouseGadgets.h): pull station, horn/strobe, light switch and Billy Bass.
  - Server-spawned; only Kind, Variant and State replicate.
  - **Light switch:** `FindLights` grabs the 4 nearest non-directional lights within 480 cm horizontal and ±260 cm vertical, from
    **any** owner (HouseGadgets.cpp:165-184). A campus switch near a window could grab exterior street lights.
  - **Pull station:** calls `UHouseDetailSubsystem::TriggerFireAlarm(24)` (HouseGadgets.cpp:139). That only sounds the gadgets in the
    subsystem's own list, which are the **house** horns (HouseDetailProps.cpp:530-541). Campus alarms need per-building groups.
- **`AHouseNightClub`.** Room-local effects driven by the local camera's position inside allowed axis-aligned regions:
  - lights on only in the listener's room;
  - music gain by region plus doorway fades;
  - a bounded `UPostProcessComponent`, Priority 100, `BlendRadius` 35. It is attached to a **QueryOnly/IgnoreAll** box: the UE 5.8
    workaround noted at HouseNightClub.cpp:141-166 (post-process bounds need a QueryOnly shape).
  - This is the right pattern for "enter a building, its lights, ambience and post-process wake up".
- **E-interaction priority** (StoryCampaign.cpp:1416-1466): club pole, vent, crawl door, carried pig, held furniture, mask, gadget,
  non-window fixture, Boywick, focused furniture, pig, poker table, door, then grab.

---------------------------------------------------------------------------------------------------------------------

## 6. Lighting of the house rooms

**Census.** From SourceArt/NightClub/hall_conversion/evidence/actors_after.json, the newest saved-map snapshot (Oct 11 20:43, 1957
actors; the map was saved at 20:51).

Map-placed:
- **99 PointLight** actors.
- **58 RectLight** actors.
- 4 `AHouseExitSign` point components: 18 intensity, 95 cm radius, red, unshadowed (HouseExitSign.cpp:20-26).
- `AHouseNightClub`: 6 spotlights, 3 active per floor.
- 1 DirectionalLight ("Night moon").
- 1 SkyLight.
- 2 unbound PostProcessVolumes: "House night exposure" and "Exit lens bloom".

Runtime additions:
- Black hall: 2 shadowed spots at 650 lm and 8 fill points at 420 lm, unshadowed (HouseBlackHallBuild.cpp:446-478).
- One more exit sign.
- Vent duct: 10 points.
- Boywick string lights: about 4 glows (BoywickRoom.cpp:262-270).
- Horn/strobe flashes, 110 cd (HouseGadgets.cpp:311-319).

Typical per-room recipe (intensities in the snapshot; reference-room installers use `LightUnits.LUMENS`; values are ×1.21 since the
×1.1 enlargement):
- **Bedroom.** One ceiling practical, about 2000–2800, attenuation 680–770. Plus one *window* fill as an interior point light
  (`BR_WindowDaylight` 1512/520, `R204_WindowSoftDaylight` 508/500, `BWR_WindowLight` 218/360). Plus an optional lamp glow
  (≈400/470).
- **Halls.** Ceiling rect or flush lights, about 1650–1900 at 612. Pendants: 16× 799/450.
- **Bath.** Ceiling 1331/750 plus vanity 157/165.
- **Library.** 9 recessed rect lights, 1220/612.
- **Spa.** 2 rect at about 2300–2500/550 plus 3 small points at 225 radius.
- **Noodle wing.** About 3 ceiling points per room, 800–1200.
- **"ExteriorMoonlight" ×4.** Rect lights *inside* the house at y=1198, z=352, 4574, facing -Y. They fake moonlight coming through
  windows. This is the established trick: indoor window light comes from interior-side lights, never outdoor ones.
- **Emissive.** Fixture meshes carry emission, e.g. `CanGlow` (30,26,20), `ExitGlow` (48,1.2,.4) (HouseBlackHallLook.cpp:739-742),
  club neon `M_NC_Neon*`, `M_Kitchen_RedExitLight`, and the fluorescent fitting meshes. Real lights do the illuminating; emissive
  sells the source.

**Exposure and post-process stack.**
- Unbound "House night exposure": **manual, bias −4.5** (SourceArt/NightClub/evidence/glow.json).
- Unbound "Exit lens bloom": priority 2.
- `ExteriorDuskFilmLook`: unbound, priority 3 (HouseDuskAtmosphere.cpp:262-287). Its weight is
  `clamp(DistanceOutsideHouse(Cam)/40)`: 0 inside the house blocks, 1 at 40 cm outside (cpp:336-340). At weight 1:
  - exposure −4.5 + 0.35 = **−4.15** (`-DuskEV=` overrides);
  - grain .3, saturation .9, contrast .94, teal shadow gain, warm highlights, vignette .55, fringe .6;
  - a further −.24 EV in the deep rear woods only (cpp:341-347).
- The club adds bounded post-process volumes at priority 100 (HouseNightClub.cpp:509-529).

**What this means for campus interiors.** They are "outside the house", so with no change:
1. The full dusk film look applies indoors. Inside the house it is deliberately absent.
2. Exposure is 0.35 EV brighter than house rooms.
3. Clock-tower bells do not duck (HouseClockTower.cpp:739-744 uses the same function).

Recommended fix: a shared indoor test, e.g. `HouseIndoors::DistanceOutside(P) = min(DistanceOutsideHouse(P), DistanceOutsideCampusInteriors(P))`.
- Campus interior boxes are registered by the campus builder.
- Use it at HouseDuskAtmosphere.cpp:339 and HouseClockTower.cpp:743.
- Alternative: a bounded campus post-process (priority above 3, like the club) that sets neutral grade, grain 0 and bias −4.5.
- Treating campus rooms exactly like house rooms is the "same craft and systems" reading.

**Lumen, GI and scalability.**
- `r.DynamicGlobalIlluminationMethod=1` (Lumen), software only, `r.Lumen.TraceMeshSDFs=0`, VSM shadows, no static lighting
  (Config/DefaultEngine.ini:16-46).
- Shipped scalability is all **1 (Medium)** (Config/DefaultGameUserSettings.ini). In UE 5.8 that means:
  - Lumen diffuse GI through the **Irradiance Volume** final gather;
  - `r.Lumen.Reflections.Allow=0`, so SSR only (Engine/Config/BaseScalability.ini `[GlobalIlluminationQuality@1]`,
    `[ReflectionQuality@1]`).

Runtime light handling:
- At world begin play, `ApplyRenderingTweaks` gives every then-existing non-directional light `MaxDrawDistance` 3200
  (`house.LightCullDistance`) and builds the shadow-cull list (HouseDetailProps.cpp:550-597; cvars 37-42).
- **Lights spawned later (campus) are not culled.** Set `MaxDrawDistance` and `CastShadows=false` yourself.

**Risk to verify.** Procedural-mesh and fast-built runtime meshes have **no mesh distance fields**, so Lumen's global SDF does not
see them. Skylight and moon bounce may leak into fully procedural rooms. The exterior already disables DF lighting on its decoration.
- Mitigation: build large interior planes (walls, floors, ceilings) from cooked cubes such as `/Engine/BasicShapes/Cube` or
  `/Game/LevelPrototyping/Meshes/SM_Cube` (scaled ISM/HISM, which have DFs since `r.GenerateMeshDistanceFields=True`). Keep
  procedural meshes for trim and detail.
- Check with `-HouseVisualReview` captures.

**Light budget for campus interiors.**
- The ≤53 rule audits point and spot components owned by `AHouseExteriorDetail` (HouseWoodsPass.cpp:57-64,99; the parking review
  uses `E->Lights`).
- Interior lights on a separate campus actor are therefore not counted, but treat them as budgeted anyway:
  - No always-on lights.
  - At most 2–3 real lights per building, enabled only while the local camera is inside that building's regions (club pattern:
    "three active for the listener's floor").
  - Unshadowed, 250–700 cm attenuation, `MaxDrawDistance` set.
  - Everything else emissive (troffers, exit signs, CRT glow).
  - Rect lights at windows for moon or sodium spill, interior side only.
  - Never place any light outdoors.
  - Log `CAMPUS_INTERIOR lights=N active=M` so the orchestrator can confirm the exterior total is still 53.

---------------------------------------------------------------------------------------------------------------------

## 7. Reusable assets for 1986 campus interiors

**General notes.**
- Paths are `/Game/...` object paths without the `.Name` suffix.
- **+V2** means damage stages `<name>_DamageV2`, `<name>_HeavyV2`, `<name>_BrokenV2` exist in the same folder; V2 is the current
  set (SourceArt/HouseInteractivity/Damage-V2.md). Older `_Damaged/_HeavilyDamaged/_Broken` also exist for `HI_*`.
- Mass and HP come from the room manifests (`mass_kg`, `break_health`). Size is the authored bounding box in cm.

**Scale.**
- `xy1.8` marks meshes authored stretched 1.8× in X and Y. Use XY ≈ 0.556 and Z 1.0 at true scale.
- Kitchen-manifest `baked_xy_scale 1.0` meshes, and the `HI_*` beds and bath pieces, are true size. Use 1.0.
  The house placed them at 1.1, or 1.87/1.98 for beds to fit the stretched rooms.

**Cooking.** "REF" means the saved map references the intact mesh, so it is cooked. Others are **not** cooked unless added to
`DirectoriesToAlwaysCook` (Config/DefaultGame.ini:12-34). Damage-stage meshes of placed props are referenced through
properties.

**Period flags.** ✗ = post-1986. ® = brand or real school (conflicts with the invented Calder State). ~ = borderline.

### 7.1 Seating
| Asset | Notes |
|---|---|
| `/Game/SeniorSendOff/House/ReferenceKitchen/SM_Kitchen_GreenSpindleChair` +V2 | 6.5 kg, hp 26, 47×49×92, true size. Steel and green vinyl: cafeteria or dining hall. REF |
| `…/ReferenceKitchen/SM_Kitchen_BlackBarStool` +V2 | 6 kg, 37×37×73, true size. REF |
| `/Game/SeniorSendOff/House/BasementLibrary/SM_Lib_CurvedWoodDeskChair` +V2 | 14 kg, hp 34. Bentwood desk chair, very 1986. xy1.8. REF |
| `…/BasementLibrary/SM_Lib_GrayOfficeChair` +V2 | 14 kg, casters (auto rolling chair). The library placed it at 0.88 XY. REF |
| `…/BasementLibrary/SM_Lib_MeshOfficeChair` +V2 | ✗ mesh-back office chair |
| `…/BasementLibrary/SM_Lib_GrayArmchair`, `SM_Lib_LeatherClubChair`, `SM_Lib_LeatherLoveseat`, `SM_Lib_BrownLeatherBarStool` +V2 | 45 / 48 / 85 / 12 kg. Lounges and faculty rooms. REF |
| `/Game/SeniorSendOff/House/Interactivity/HI_SM_GR3_Conversation_001…008` +V2 | Great-room cognac and olive chairs. REF |
| `/Game/SeniorSendOff/House/HallFloorsPorch/SM_HFP_Chair`, `SM_HFP_Bench`, `SM_HFP_SeatCushion` +V2 | REF |
| `/Game/SeniorSendOff/House/NoodleWing/SM_NW_SpindleChair`, `SM_NW_LeatherStool`, `SM_NW_LeatherSofa`, `SM_NW_TanRecliner` | No damage stages (squash fallback). REF |
| `/Game/SeniorSendOff/House/Room102Chairs/SM_R102_FoldingChair`, `SM_R102_FoldingChairFolded`; staged versions `/Game/SeniorSendOff/House/Interactivity/HI_Room102_Chair_01_001`, `HI_Room102_Chair_31_001` +V2 | Ivory plastic folding chair, 4 kg, 6 UCX. Lecture or event seating. **Not cooked** (removed from the map) |
| `/Game/SeniorSendOff/House/PokerRoom/SM_POK_Chair` | Leather and walnut. **Not cooked** (poker room retired) |
| `/Game/SeniorSendOff/House/BoywickBedroom/SM_BWR_GreyFuton` +V2, `SM_BWR_LowDaybed` +V2 | 44 kg futon. REF |
| `/Game/SeniorSendOff/House/AcrossPosterBedroom/SM_Bedroom_BrownReclinerSofa` (staged `HI_BR_SM_Bedroom_BrownReclinerSofa_001`) | REF (HI) |

### 7.2 Desks and tables
| Asset | Notes |
|---|---|
| `…/BasementLibrary/SM_Lib_RoundedOakStudyTable` +V2 | 105 kg, hp 100, 335×185×78 (xy1.8 → ≈186×103). Reading-room table. REF |
| `…/BasementLibrary/SM_Lib_TrestlePlankWorktable` +V2 | 100 kg, 388×148×80 (xy1.8). Studio or lab bench. REF |
| `…/BasementLibrary/SM_Lib_OctagonalCardTable` +V2 | 85 kg, 295 across (xy1.8). Lounge game table. REF |
| `…/BasementLibrary/SM_Lib_BlackSideTable`, `SM_Lib_RoundWoodSideTable`, `SM_Lib_CoffeeChest`, `SM_Lib_DistressedCreamConsole` +V2 | REF |
| `…/ReferenceKitchen/SM_Kitchen_DiningTable` +V2 | 48 kg, hp 65, 306×140×77 (xy1.8 → ≈170×78). Dining hall. REF |
| `/Game/SeniorSendOff/House/BoywickBedroom/SM_BWR_WritingDesk` +V2 | 23 kg, hp 60. Dorm desk. REF |
| `…/BoywickBedroom/SM_BWR_CoffeeTable` +V2; `…/AcrossPosterBedroom/SM_Bedroom_OvalCoffeeTable`; `SM_BedProps_CoffeeTable` | REF |
| `/Game/SeniorSendOff/House/NoodleWing/SM_NW_BilliardTable` + `SM_NW_CueRack`, `SM_NW_PoolCue`, `SM_NW_CueBall`, `SM_NW_YellowBall` | Rec room or student union. REF |
| `…/NoodleWing/SM_NW_SideTable`, `SM_NW_Counter` | REF |
| `/Game/SeniorSendOff/House/PokerRoom/SM_POK_Table` (+`_Collision`), `SM_POK_Sideboard` | Not cooked |

### 7.3 Shelving and storage
| Asset | Notes |
|---|---|
| `…/BasementLibrary/SM_Lib_SixShelfBookcase` +V2 (+`_BrokenV3`) | 85 kg, hp 85, 134×54×189 (xy1.8 → ≈74×30×189). REF |
| `…/BasementLibrary/SM_Lib_BookShelfContents0…4` +V2; loose `SM_Lib_Loose_BookShelfContents{0-4}_{00-10}` +V2; `SM_Lib_StackedBooks`, `SM_Lib_Loose_StackedBooks_00…05` | Filled shelves plus individually throwable books (0.35–0.65 kg). REF |
| `…/BoywickBedroom/SM_BWR_Dresser` +V2 (22 kg), `SM_BWR_WireRack` +V2, `SM_BWR_ClosetDrawerUnit` +V2, `SM_BWR_RopeShelf` +V2, `SM_BWR_StorageBox` +V2 | Dorm storage. REF |
| `…/AcrossPosterBedroom/SM_Bedroom_OakFourDrawerChest`, `SM_BedProps_Dresser` | REF via HI_BR_* |
| `/Game/SeniorSendOff/House/Room204/SM_R204_OpenDarkWoodDresser` (staged `HI_R204_OpenDarkWoodDresser_001`) | REF (HI) |
| `…/NoodleWing/SM_NW_WallCabinet`; `/Game/SeniorSendOff/House/UpstairsBathroom/SM_Bath_UtilityShelf` | REF / HI |
| `…/BasementLibrary/SM_Lib_WhitePlasticDrawerTower` | ~ plastic drawer tower (1990s look) |
| `…/ReferenceKitchen/SM_Kitchen_ClosedStorageBox`, `SM_Kitchen_SuppliesBox`; `…/BasementLibrary/SM_Lib_OpenCardboardBox` | Boxes. REF |

### 7.4 Beds (dorms)
| Asset | Notes |
|---|---|
| `…/BoywickBedroom/SM_BWR_LoftFrame` +V2, `SM_BWR_QuiltedMattress` +V2 | Steel loft bed, 65 kg, hp 160. The archetypal dorm bed. REF |
| `/Game/SeniorSendOff/House/Interactivity/HI_BR_SM_Bedroom_TwinSpindleBed_001` +V2 / `HI_BR_SM_Bedroom_FullSpindleBed_001` +V2 | True size (placed 1.87 XY in the house). REF |
| `/Game/SeniorSendOff/House/Interactivity/HI_R204_BlackDoubleBed_001`, `HI_R204_WhiteStripedDoubleBed_001` (+ `_HeavilyDamaged` etc.) | REF |
| Pillows: `…/BasementLibrary/SM_Lib_{Tan,Dark,Orange}ThrowPillow` +V2; `…/BoywickBedroom/SM_BWR_GreyPillow`, `SM_BWR_PatternedPillow` | REF |

### 7.5 Lighting fixtures (pair each with emission or one gated real light)
- `/Game/SeniorSendOff/House/Room102Chairs/SM_R102_CeilingFixtures`: fluorescent troffer set. Classrooms and halls. Not cooked.
- `/Game/SeniorSendOff/House/UpstairsBathroom/SM_Bath_FluorescentCeiling`.
  Staged: `/Game/SeniorSendOff/House/Interactivity/HI_UpBath_Bath_white_fluorescent_fitting_1_001` +V2. REF (HI).
- `…/NoodleWing/SM_NW_CeilingFixtures` (fluorescent), `SM_NW_StairNorthernPractical`, `SM_NW_TableLamp`. REF.
- `…/BasementLibrary/SM_Lib_StudyPendant` (fixed, `movable False`), `SM_Lib_FloorLamp`, `SM_Lib_TableLamp` +V2. REF.
- `/Game/SeniorSendOff/House/Interactivity/HI_R204_SM_FlushCeilingLight_001`; `…/UpstairsBathroom/SM_Bath_MirrorLightRail`;
  `/Game/SeniorSendOff/House/HallwayEnd/SM_HallEnd_ThreeRoomsPracticalLights` (room-fitted).
- `…/AcrossPosterBedroom/SM_Bedroom_FiveBladeCeilingFan`. REF via HI.
- `/Game/SeniorSendOff/House/PokerRoom/SM_POK_Pendant`. Not cooked.

### 7.6 Doors, trim and hardware
- **Procedural `AHouseBlackHallDoor`.** French glazed pair, steel exit with push bar, equipment closet with sign. No asset,
  replication-safe, preferred.
- `/Game/SeniorSendOff/House/NoodleWing/SM_NW_FireDoor`: steel fire door, hinge-pivot, used at scale 1.0 by NW_MainStairDoor.
  Dorm corridor and stair doors. REF.
- `/Game/SeniorSendOff/House/FrontDoubleDoors/SM_FD_WalnutLeaf` and `SM_FD_WalnutFrame`: walnut double entrance. Academic halls.
  REF.
- `/Game/SeniorSendOff/House/Room101Entry/SM_R101Entry_Door101Leaf`, `…Door102Leaf`: white six-panel, hinge-pivot, 147×200.
  ✗ the electronic **keypad**; prefer `SM_R101Entry_BlankNamePlaque` and a plain leaf.
- `/Game/SeniorSendOff/House/ReferenceKitchen/SM_Kitchen_ClosedClosetDoor` (six-panel).
  `/Game/SeniorSendOff/House/BasementLibrary/SM_Lib_MechanicalDoorSource` (brown service door).
  `/Game/SeniorSendOff/House/DoorRepair/SM_PorchAccessFittedLeaf_v2`. All REF.
- `/Game/ExistingHouseDoors/SM_HouseLeverHandle`: the house's added lever. REF.
- Avoid `/Game/ExistingHouseDoors/SM_OriginalLeaf_*` and `/Game/SeniorSendOff/House/HallwayEnd/*_LeafWorld`: world-baked vertices
  that need `SourceGeometryOriginWorld`.
- Trim and floor details: `/Game/SeniorSendOff/House/HallFloorsPorch/SM_HFP_WoodThreshold`, `SM_HFP_RubberMat`, `SM_HFP_FloorGrate`,
  `SM_HFP_LandingGuard`. NoodleWing `SM_NW_StairRails`, `SM_NW_StairHardware`, `SM_NW_UtilityCable`, `SM_NW_UtilityDetail` are
  room-fitted, so take them as reference rather than reuse.
- Shared materials: `/Game/SeniorSendOff/House/Materials/` contains `MI_SSO_PaintCream`, `MI_SSO_PaintWhite`, `MI_SSO_PaintGrey`,
  `MI_SSO_WoodOak`, `MI_SSO_WoodWalnut`, `MI_SSO_OakTone0…7`, `MI_SSO_TileCream`, `MI_SSO_S02BathroomTile`, `MI_SSO_S03HallConcrete`,
  `MI_SSO_S03WornConcrete`, `MI_SSO_S03MasonryCream`, `MI_SSO_Brick`, `MI_SSO_S03StairTreadSteel`, `MI_SSO_S03HallSkirting`,
  `MI_SSO_MetalBrass`, `MI_SSO_MetalDark`, `MI_SSO_Glass`, `MI_SSO_LightWarm`, `MI_SSO_RugRed/Gold/Teal`, `MI_SSO_FeltGreen`, and the
  masters `M_House_Textured`, `M_House_Plain`, `M_House_Glass`.
- Textures: `/Game/SeniorSendOff/House/Textures/` (`T_S03HallConcrete_*`, `T_S02FloorOak_*`, `T_Masonry_*`, `T_S03BathFloor_*`,
  carpet and plaster normals).

### 7.7 Signage, safety and wall items
- `/Game/SeniorSendOff/House/ExitSigns/SM_NEX_{Wall,Double}_{0,L,R}_{0..3}`, spawned through **`AHouseExitSign::Configure(dir,double)`**:
  - the 4 stage meshes are auto-loaded;
  - the folder is always cooked;
  - each sign has one 18-intensity / 95 cm red point light. Count them, or drop the light for campus via a subclass.
  - ~ the modern molded LED housing; a 1986 incandescent box would be more period-correct.
- `…/ReferenceKitchen/SM_Kitchen_ExitSign`, `SM_Kitchen_FireCabinet` (7 kg, wall), `SM_Kitchen_SmokeDetector`, `SM_Kitchen_Outlet`.
  `…/NoodleWing/SM_NW_FireExtinguisher`, `SM_NW_ExitSign`. `…/BoywickBedroom/SM_BWR_SmokeAlarm`, `SM_BWR_PowerOutlet`. All REF.
- Room-number plaques:
  - `/Game/SeniorSendOff/House/Interactivity/HI_WallDecor_SM_Room102_Sign_1` +V2 and `/Game/SeniorSendOff/House/Room102Chairs/SM_R102_NumberPlate`
    (not cooked);
  - `…/Room101Entry/SM_R101Entry_BlankNamePlaque` (REF).
- Frames and prints: `…/BasementLibrary/SM_Lib_FramedHistoricPhoto`, `SM_Lib_FramedGroupPhoto`, `SM_Lib_RosterFrame`,
  `SM_Lib_LighthouseCanvas` (wall, +V2). `…/ReferenceKitchen/SM_Kitchen_RulesWallPrint`, `SM_Kitchen_CityWallPrint`,
  `SM_Kitchen_CafeWallPrint` (wall +V2). REF.
- `…/BasementLibrary/SM_Lib_GlassWhiteboard` is ✗ (glass whiteboard). A 1986 room needs a **chalkboard**, which does not exist.
- Runtime gadgets (`AHouseGadget`): pull station, horn/strobe and light switch. The 1986 hardware is plausible, but mind §5.7.

### 7.8 TVs and electronics
**There is no CRT in the project.** Every TV is a flat panel:
- `/Game/SeniorSendOff/House/GreatRoomTVs/SM_GRTV_Television` (16:9 LCD) ✗
- `…/AcrossPosterBedroom/SM_Bedroom_WallTelevision` ✗
- `…/BasementLibrary/SM_Lib_WallTelevision` ✗
- `…/ReferenceKitchen/SM_Kitchen_TV` ✗ ("flat television", build_kitchen.py:151)
- `…/NoodleWing/SM_NW_TV` ✗ (+ `SM_NW_TVConsole` cabinet ~ OK)
- `…/BoywickBedroom/SM_BWR_SmallTelevision` ✗ (flat bezel)

Also ✗: monitors `SM_Lib_DeskMonitor` and `SM_Lib_SmallMonitor`; `SM_BWR_Laptop`, `SM_BWR_Phone`, `SM_BWR_GameConsole`,
`SM_BWR_Controller`; `SM_Bedroom_BlackGamingTable`; `SM_BR_PurplePerimeterLED`.

OK for 1986: **`/Game/SeniorSendOff/House/NoodleWing/SM_NW_Arcade`** (arcade cabinet, REF).

Recommendation: build a CRT (wood-grain console TV and a 13" portable), a rotary or desk phone, a typewriter and a boom box as
runtime kit props (`HwBuildKitProp` style), with emissive screens.

### 7.9 Kitchen, dining, bath and laundry
**Dining and kitchen (REF, xy1.8 unless noted):**
- `…/ReferenceKitchen/SM_Kitchen_ServingCounter` +V2: stainless serving cart with sneeze guard and food wells, 145 kg. A **cafeteria
  line**.
- `SM_Kitchen_TopFreezerFridge` (92 kg), `SM_Kitchen_UtilitySink`, `SM_Kitchen_IceMachine`, `SM_Kitchen_IceMachinePallet`,
  `SM_Kitchen_MicrowaveCart`, `SM_Kitchen_StainlessApplianceRack`, `SM_Kitchen_SmallBlackCabinet`, `SM_Kitchen_TrashCan`,
  `SM_Kitchen_WhiteMicrowave`/`SilverMicrowave` (1986 OK).
- `SM_Kitchen_Toaster`, `SM_Kitchen_CoffeeMaker` (true size), `SM_Kitchen_CoffeePump`, `SM_Kitchen_WaterPitcher`,
  `SM_Kitchen_ServingBowl`, `SM_Kitchen_Saucepan`, `SM_Kitchen_PanLid`, `SM_Kitchen_FruitTray`, `SM_Kitchen_UtensilCup`,
  `SM_Kitchen_NapkinBasket`, `SM_Kitchen_PaperTowelRoll`, `SM_Kitchen_ClearCup`. All +V2.
- ~ `SM_Kitchen_WaterDispenser` (true size). ~ `SM_Kitchen_TowerFan`.
- ✗ `SM_Kitchen_AirFryer`, ✗ `SM_Kitchen_CoffeePods`, ✗® `SM_Kitchen_RedBullCase`, ® `SM_Kitchen_TennesseeCoolerBag`.

**Bath (dorm gang bathroom), staged HI versions, REF:**
- `/Game/SeniorSendOff/House/Interactivity/HI_UpBath_Bath_ceramic_toilet_1_001`, `HI_UpBath_Bath_open_toilet_stall_1_001`,
  `HI_UpBath_SM_Bath_VanityTriple_001` (triple-sink vanity), `HI_UpBath_SM_Bath_MirrorLightRail_001`,
  `HI_UpBath_Bath_orange_shower_frame_1_001`, `HI_UpBath_Bath_black_dispenser_1_001`, `HI_UpBath_SM_Bath_ShowerShelf_001`,
  `HI_UpBath_SM_Bath_UtilityShelf_001`. All +V2.
- Plain: `/Game/SeniorSendOff/House/UpstairsBathroom/SM_Bath_FloorDrain`, `SM_Bath_ShowerCurtainClosed`/`Gathered`,
  `SM_Bath_WallDispenser`.

**Laundry and rec (NoodleWing, REF):**
- `SM_NW_TopWasher`, `SM_NW_UtilitySink`, `SM_NW_LaundryBasket`, `SM_NW_LaundryPile`, `SM_NW_StackedLaundry`.
- ~ `SM_NW_FrontWasher`. ® check `SM_NW_Detergent` and `SM_NW_DryerSheet` labels.

**Dorm appliances:** `…/BoywickBedroom/SM_BWR_MiniFridge` +V2 (32 kg) OK. ~ `…/AcrossPosterBedroom/SM_Bedroom_StainlessMiniFridge`.
✗® `…/Room204/SM_R204_GalanzFridgeAndMicrowave`.

### 7.10 Rugs, window dressing and clutter
**Rugs (REF):** `…/BasementLibrary/SM_Lib_LongRug`, `SM_Lib_EntryRug`, `SM_Lib_StudyRug` +V2; `/Game/SeniorSendOff/House/GreatRoom/SM_GR_VintageRug`;
`/Game/SeniorSendOff/House/Interactivity/HI_SM_GR3_GreenRug/RoseRug/FarRug`; `…/BoywickBedroom/SM_BWR_StripedRug`;
`HI_R204_LargeMottledGrayRug_001`.

**Window dressing:**
- Blinds: `…/ReferenceKitchen/SM_Kitchen_WindowBlinds` +V2 and `…/BoywickBedroom/SM_BWR_WindowBlinds` (REF).
- Curtains: `…/AcrossPosterBedroom/SM_Bedroom_CharcoalCurtainsClosed`, `SM_Bedroom_CreamCurtainsPartOpen`. These were placed as
  `AHouseReactiveFixture` curtains.

**Clutter (REF):**
- Library papers and books: `SM_Lib_Loose_PaperScatter_00…06`, `SM_Lib_Loose_LoosePaperStack_00…07`, `SM_Lib_RedCoffeeBook`,
  `SM_Lib_CardDeck`, `SM_Lib_PaperCup`, `SM_Lib_GlassJar`, `SM_Lib_ClosedSilverCase`, `SM_Lib_FoldedBlueWhiteUmbrella`,
  `SM_Lib_GrayBackpack` (and `_Loose_GrayBackpack_bag/jacket`).
- Boywick clothes: `SM_BWR_HangingShirt0…7`, `SM_BWR_LooseShirt0…5`, `SM_BWR_Sneaker`, `SM_BWR_LaundryBasket` +V2,
  `SM_BWR_TrashBin` +V2.
- Club drinks: `/Game/SeniorSendOff/House/NightClub/ConnectionPolish/SM_CP_LooseCup`, `SM_CP_LooseBottle`, `SM_CP_LooseCoaster`,
  `SM_CP_CrumpledNapkin`.
- ~ `SM_Lib_PartyGameStack`: modern party-game boxes. Check the art.

### 7.11 Do not reuse as-is (branded, real-school or anachronistic)
- Real University of Tennessee and state items:
  - AcrossPosterBedroom: `SM_BedArt_VolsHelmet`, `SM_BedArt_VolsPennant`, `SM_BedArt_TennesseeBanner`, `SM_BedArt_TennesseeMiniFlag`,
    `SM_BedArt_TennesseeDoorPlaque`, `SM_BedArt_CollegiateT`, `SM_BedArt_ChampionsPennant`, `SM_BedArt_XIIICrest`,
    `SM_BedArt_TexasFlag`, `SM_BedArt_AmericanWomanFlag`.
  - Room204: `SM_R204_Art_OrangeTFlag`, `SM_R204_Art_BlackTFlag`, `SM_R204_Art_CheckeredT`.
  - Materials `MI_SSO_S03Colorado*`, `MI_SSO_S03Rangers*`, `MI_SSO_S05TennesseeOrange`, `M_Kitchen_TennesseeOrange`.
- Brands:
  - Corona (`SM_BedArt_CoronaOval`).
  - Bud Light (`/Game/SeniorSendOff/House/HallwayEnd/SM_HallEnd_BudLightFlag`).
  - Busch Apple (`SM_R204_Art_BuschApple`, ✗ 2019).
  - Coors (`SM_R204_CoorsCarton`).
  - Red Bull, Galanz, Ford (`SM_NW_FordPlaque`), JAWS and Augusta posters (`SM_R204_Art_JAWS`, `SM_R204_Art_Augusta`).
  - Pulp Fiction (✗ 1994, `SM_BWR_PulpFictionPoster`), Action Bronson (✗, `SM_BWR_ActionBronsonPoster`), Flavortown
    (✗, `SM_BWR_FlavortownBanner`), Santa Bass.
- Club neon `SM_NC_Sign_PhiSig`: a real fraternity name pattern.
- NoodleWing wall art `SM_NW_RedSport`, `SM_NW_RedWildlife`, `SM_NW_RedDogs`, `SM_NW_SportsPennant`, `SM_NW_HallFlag`: check each.
- Replace them with runtime-painted Calder State purple/gold panels (`FHallPanelSpec` painters, as the black-hall murals do).

### 7.12 Missing for a 1986 campus (build at runtime)
CRT televisions, chalkboards, lecture tablet-arm desks, radiators and steam pipes, steel lockers, vending and cigarette machines,
a pay phone on the wall (the quad has a booth), rotary phones, typewriters, card-catalog cabinets, metal bunk beds,
cinder-block dorm walls (the CMU kit in HouseBlackHallLook already exists: `Cmu = 81.28`), terrazzo and VCT floor tile,
acoustic ceiling tile grids, and drinking fountains.

---------------------------------------------------------------------------------------------------------------------

## 8. Recommended blueprint for campus interiors (same craft and systems)

1. **One `UCampusInteriorsSubsystem`** (UWorldSubsystem, Chapter01_House only, `-NoCampusInteriors` opt-out). It mirrors
   `UHouseBlackHallSubsystem`:
   - spawns a transient, NotPlaceable cosmetic actor per building on **every machine** (procedural trim and painted signage);
   - spawns replicated gameplay actors only when `NetMode != NM_Client`;
   - never touches map actors or trees, and never applies `HouseWorldScale`.
2. **Shell.** Large planes come from cooked cube ISM/HISM, for Lumen DF occlusion and cheap collision. Detail comes from `FHallMesh`.
   Floors at campus grade + 60 cm (the ground is about −60). Openings line up with the exterior façades (coordinate with the
   layout brief).
3. **Props.**
   - Use `ACampusProp : AHouseInteractiveProp` with a replicated `uint16 CatalogIndex` (`COND_InitialOnly`).
   - The catalog row holds the intact and V2 stage paths, MassKg, BreakHealth, CarrySize, bWallMounted and a real-world scale
     (0.556 XY for xy1.8 assets).
   - `ApplyCatalog()` runs on the server before `FinishSpawning` and on clients in `BeginPlay` before `Super`.
   - Set the tag `SSOCampusDamage` and `DamageCategory="Campus"`. Spawn parked.
   - Add the folders used to `DirectoriesToAlwaysCook`, or reuse only REF assets.
4. **Doors.**
   - Use `ACampusDoor : AHouseBlackHallDoor` (procedural styles), or a mesh-leaf subclass with a replicated style.
   - `DoorId` must be unique with a `Campus_` prefix.
   - Non-enterable buildings get `bLocked=true`, which gives the "not playable yet" toast.
   - Keep 2 m swing arcs clear of props.
5. **Windows.** Use `AHouseReactiveFixture` Window kind with an **asset** mesh (cube or plane plus `M_House_Glass` instance) and
   an Owner set. Tag it so its breaks count as campus damage.
6. **Light and post-process.**
   - A per-building camera-inside test, reusing the club's region and fade logic.
   - It enables 2–3 unshadowed lights with `MaxDrawDistance` set, plus emissive fixtures.
   - Extend the indoor test so the dusk film look and the bell ducking treat campus interiors like house interiors.
   - No outdoor lights.
7. **Verification.**
   - `-CampusInteriorsSmoke`, modelled on HouseKitchenSmoke: inventory, parked props, stage meshes with box or convex hulls, real
     pickup, three damage stages, `GetTotalBrokenCount()` unchanged after a campus break, locked-door toast and blocking, an open
     door that clears the capsule.
   - `-HouseVisualReview` views per building, through a `SourceArt/CampusInteriors/` folder with README, views.json and evidence/.
   - Re-run all existing house and exterior reviews plus `-ClockTowerReview`.
   - Confirm the log line `WOODS_PASS_ORIGINAL_TREES … crc=0f3b2bb7`.
