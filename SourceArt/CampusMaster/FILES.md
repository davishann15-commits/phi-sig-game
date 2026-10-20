# Calder State master job: what changed, file by file

Everything is built at runtime in code; nothing is saved into the map. New files are the job's own; the files under
"Changed" existed before it, and the patch (`campus_master.patch`, beside this file) shows exactly what the job changed in
them against the pre-job baseline. That baseline (`Saved/CampusMasterWork/baseline/`, with its `REPORT.md`) was rebuilt
and verified: replaying only the job's own edits onto it reproduces every current file byte for byte, and with it in place
`git diff` is exactly the size recorded when the job started. Line counts are as of the end of the job (2026-10-14).

## New: Phase 1, the campus plan

| File | Lines | What it is |
|---|---:|---|
| `Source/SeniorSendoff/HouseCampusLayout.h/.cpp` | 105 / 1416 | The plan as data: 16 blocks (names, kinds, tiers, footprints, entrance faces), Calder Dr, its curve, Stadium Dr and the bus loop, the west road and south lane, four lots with stalls and tree islands, the walk network, plazas, berms, the gateposts; builders for the ground (roads at street level with the terrain cut away, curbs, ramps, paint), lamp and bench points; queries the exterior, the trees, the interiors and the polish use (Occupied, OnTrafficLane, TreeHidden, ApronDepth, WalkStrips...). `-NoCampusLayout` gives the old campus back exactly. |
| `Source/SeniorSendoff/HouseCampusLayoutReview.cpp` | 184 | `-CampusLayoutReview`: the plan's data checks and the in-world ones (floor samples at walk height, capsule obstruction tests on the walks, trunks, poles, lights). |
| `Source/SeniorSendoff/HouseCampusStreetDetail.h/.cpp` | 18 / 255 | The signalised mid-block crossing, STOP and stop bar, regulatory and guide signs, meters, inlets and manholes, the bus bay curb, the gravel overflow lot. |

## New: Phase 2, the walk-in buildings

| File | Lines | What it is |
|---|---:|---|
| `HouseCampusInteriors.h/.cpp` | 293 / 2074 | The framework: the world subsystem, the transient interiors actor (builds a building's inside when someone comes near, frees it when everyone has gone, the pool of 6 interior lights on channel 1, the indoor test), `ACampusDoor` (replicated, with its locked reason), `ACampusProp` (the house's furniture by kit index), the library (Mercer), the shared kit and look, interior-mapped windows, wall clocks on the tower's time, the interior sounds' playback; the parts converted to static meshes a piece at a time, inner doors drawn only while their inside is built (Phase 4). `-NoCampusInteriors` gives back the solid blocks. |
| `HouseCampusInteriorsRooms.h` | 282 | The building registry (`FInteriorDef`) and the shared helpers each building file uses. |
| `HouseCampusInteriorsFixtures.cpp` | 457 | 1986 fixtures in code: troffers, chalkboards, bulletin boards, lockers, water fountains, payphones, vending machines, CRT televisions, radiators, exit signs, pinball machines and more. |
| `HouseCampusInteriorsRoomKits.cpp` | 185 | Room templates: classroom, office, lounge, dining room, seminar, reception, waiting room, trophy case. |
| `HouseCampusInteriorsHarlow.cpp` | 988 | Harlow Hall (dorm): lobby and hall desk, TV lounge, bathrooms, laundry, rec room, study, kitchen, eleven furnished dorm rooms in six characters. |
| `HouseCampusInteriorsScience.cpp` | 763 | Hollis Hall of Science: lobby with twin stairs, chemistry and physics labs, the tiered lecture hall, optics lab, seminar, faculty lounge. |
| `HouseCampusInteriorsUnion.cpp` | 638 | The Goose Union: concourse round the atrium, TV lounge, bookstore, the Goose Pit, game room, mailroom, committee room, ballroom. |
| `HouseCampusInteriorsStadium.cpp` | 786 | The stadium walk-in: concourse, gates 4 and 5, vomitory, locker room and tunnel, press box and its stair tower; the exterior's cut-outs. |
| `HouseCampusInteriorsGeneric.cpp` | 931 | Tier B (Old Main, the Commons, Ashby, Calder Hall, Kerr, Whitcomb) and Tier C (six lobbies) from one plan generator. |
| `HouseCampusInteriorsSound.cpp` | 155 | The interiors' synthesised sounds: drip, TV crowd, the postgame radio show, room tone. |
| `HouseCampusInteriorsReview.cpp` | 699 | `-CampusInteriorsReview=...`: real-input walks through every building, locked doors, streaming, light pool, wall clocks against the tower. |
| `HouseCampusInteriorsSmoke.cpp` | 276 | `-CampusInteriorsSmoke=TierA` (or `=All`, or building Ids): a smoke test per building in the house rooms' style (geometry, floors under the lights, props resting, doors and reasons, clocks, signs, sounds, windows). |

## New: Phase 3, the exterior polish

| File | Lines | What it is |
|---|---:|---|
| `HouseCampusPolish.h/.cpp` | 137 / 604 | The polish actor: builds the parts (Solid, Detail, Ground, Far, Glow, Lawn, light-pool decals), the haze and smoke puffs, the flags and streamers in the wind, the outdoor soundscape. The parts are drawn as runtime static meshes, with the solid part's collision as procedural meshes that are not drawn (Phase 4). `-NoCampusPolish` gives back Phase 2's campus. |
| `HouseCampusPolishStadium.cpp` | 1188 | 3a: the stadium's facade, real window openings, gates and ticket windows, press box steel and roof masts, flood lamp banks, the apron and its props, the buses and coach, the tailgates in the South Lot. |
| `HouseCampusPolishBuildings.cpp` | 766 | 3b: each building's roof and character, entrances, fire escapes, ivy, window treatments, window wells, meters. |
| `HouseCampusPolishGround.cpp` | 263 | 3b: mown lawns, desire paths, mulched tree rings, wall leaves, walk joints and cracks. |
| `HouseCampusPolishLandmarks.cpp` | 355 | 3c: the goose statue, the dry fountain, the flagpole plaza, the alumni tent, lamp streamers, the bonfire site, the clock tower's base. |
| `HouseCampusPolishStreetFurniture.cpp` | 263 | 3c: campus maps, fingerposts, trash cans, bike racks, notice boards, hydrants. |
| `HouseCampusPolishLightPools.cpp` | 69 | 3d: the light pools and the haze. |
| `HouseCampusPolishSound.cpp` | 194 | 3d: the outdoor sounds, synthesised. |

## New: Phase 4, performance

| File | Lines | What it is |
|---|---:|---|
| `HouseCampusStaticMesh.h/.cpp` | 17 / 108 | An `FHallMesh` as a transient static mesh built at runtime (the woods' way). The polish's tiles and the interiors' parts are drawn this way: a static mesh's draws are cached by the renderer, where a procedural mesh's sections are gathered again every frame on the render thread. |

## New: after Phase 4 (the approved extras)

| File | Lines | What it is |
|---|---:|---|
| `HouseCampusInteriorsTower.cpp` | 484 | The walk-in clock tower ("TWR"), free-standing like the stadium: the plank oak door pair, the brick linings and lancet glass seen from inside, the open-well oak stair (11 flights of 13 risers) to the belfry, its store and dressing, the invisible belfry guards, the light anchors, the walk-in predicate `ClockTowerOpen`. |
| `HouseCampusPolishTown.cpp` | 428 | The town of Calder: three ridgelines (the near one wooded in fall colour), the valley floor and river with its truss bridge, Main Street, the inn, the courthouse, two churches, the mill and its stack, houses up the slope, fall trees, the CALDER water tower, the radio mast and the fire lookout. |

## New: tools and assets

| File | What it is |
|---|---|
| `Tools/claude/build_campus_materials.py` | Editor Python (the project's pattern) that made `M_CampusInteriorWindow` and `M_LightPoolDecal`. |
| `Tools/claude/survey_campus_*.py` | Read-only surveys of the house's assets used for the kit (shelves, kit, bedroom, flags, house assets). |
| `Content/SeniorSendOff/House/CampusInterior/M_CampusInteriorWindow.uasset` | Interior-mapped windows for the closed floors (Phase 2b). |
| `Content/SeniorSendOff/House/CampusInterior/M_LightPoolDecal.uasset` | The emissive light-pool decal (Phase 3d). |

## Changed (existing files; see the patch)

| File | Why |
|---|---|
| `Config/DefaultGame.ini` | The campus interior materials and the house folders the campus reuses at runtime (street, basement library, Boywick bedroom, kitchen, Noodle Wing) on the cook list. |
| `HouseExterior.cpp` | Spawns the clock tower, the homecoming dressing and the polish in order; the plan's ground, terrain cuts, lot cars and trees hidden under the plan; the detail views for every step's shots; the view sampler's Phase 4 tools (render and RHI thread times, `-ExteriorViewCsv` profiles, the `-ExteriorViewToggle` A/B). |
| `HouseExteriorCampus.cpp` | The campus blocks from the plan (shells for the walk-in ones); the stadium's cut-outs for the walk-in and its real window openings, its grilles and flood heads left to the polish. With the polish on, the twelve random far blocks give way to the town of Calder. |
| `HouseSurroundingsPolish.h/.cpp` | The window builder takes an interior-mapped room; small changes for the plan; the junction's street-name blades turned to read the right way (with the plan) and Fraternity Park Dr's stop bar moved to the lane that approaches the junction (approved). |
| `HouseCampusFall.h/.cpp`, `HouseCampusFallLook.cpp`, `HouseCampusFallQuad.cpp`, `HouseCampusFallStadium.cpp` | The homecoming dressing follows the plan (kiosk, gate path, dorm sheets); the drums muffle indoors; the stadium's gating (litter, sheets, pennants) for the walk-in; where the float, trailer and bonfire stand; with the polish on, the float in tissue pomps with a real neck and a torn patch, the flood halos on the new lamp banks. |
| `ExistingHouseDoor.h` | `SetStartsOpen` (a campus door that stands open). |
| `HouseDamageSubsystem.cpp` | Campus props' breaks are not house damage (`SSOCampusDamage`). |
| `HouseBreakDebris.cpp`, `HouseInteractiveProp.h/.cpp` | Debris and a broken prop's ghost are lit like the piece they came from (the interior channel indoors); a prop already broken when someone joins keeps its intact scale. |
| `HouseDuskAtmosphere.cpp` | A campus interior counts as indoors (exposure, no film grade, the tower's bells muffled). |
| `HouseStreetScene.cpp` | No leaf lies over the far sidewalk where the plan's drives cross it (its random draws are still made, so every other leaf stays put). |
| `HouseVisualReview.cpp` | The new reviews and the smoke test ticked like the house's. |
| `HouseClockTowerMesh.h/.cpp` | The walk-in tower (only while `ClockTowerOpen()`): the plates across the shaft become frames, the door below its springing becomes the interiors' leaves, the belfry floor gets the stair's slot and its trap lid, the open base its own inner faces; `ShaftLining` for the inside. Closed, the tower is built exactly as before. |
| `HouseBlackHall.h/.cpp` | A `Plank` door style (the clock tower's ledged oak leaves, in the tower's materials) and an `Astragal` flag; the house's existing door styles are unchanged. |
| `HouseBasketballCourt.cpp`, `HouseRearPorch.cpp` | The wall-gap fixes at the backyard court (approved): the bank lawn reaches under the court wall, the apron strips under its inner face, the kitchen-gate pad meets the porch block. |
| `StoryCampaign.cpp` | A locked campus door's message says why (every house door keeps its own). |
| `HouseFreeRoamReview.cpp` | The campus route (20 waypoints from the house's front walk round the stadium to the clock tower). |

The house's own rooms are untouched.
