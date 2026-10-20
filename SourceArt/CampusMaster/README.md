# Calder State, made real: Homecoming '86

The campus across Fraternity Park Dr from the house, rebuilt in three phases (the master prompt; the plan is
[PLAN.md](PLAN.md), the step-by-step log with every review's results is [STATUS.md](STATUS.md), the file-by-file summary is
[FILES.md](FILES.md), and `campus_master.patch` is the patch against the pre-job snapshot):

1. **Layout.** A real campus plan: 16 buildings re-placed and re-shaped with their own names and entrances, Calder Dr and
   its curve into Stadium Dr and the bus loop, four lots, a continuous walk network with crossings, berms, signals, signs and
   markings. (`HouseCampusLayout*`, `HouseCampusStreetDetail*`.)
2. **Walk-in buildings.** Seventeen buildings you can walk into (eighteen with the clock tower, added later),
   modelled the way the house's rooms are: Mercer Library,
   Harlow Hall, Hollis Hall of Science, the Goose Union and the stadium (Tier A, whole floors); Old Main, the Commons, Ashby
   Hall, Calder Hall, Kerr Hall and Whitcomb Fine Arts (Tier B, the ground floor); the health center, the fieldhouse, the
   music hall, Pratt Engineering, the physical plant and Alumni Hall (Tier C, lit lobbies with locked doors that say why).
   The house's own furniture (carryable, breakable), its doors, its lighting approach; wall clocks on the clock tower's
   time; synthesised room tone, a dripping tap, TV crowds and the postgame radio show. (`HouseCampusInteriors*`.)
3. **Exterior polish.** The stadium's facade, real window openings, gates, structure, flood lamp banks and apron with the
   buses and the tailgates' aftermath; every building its own roof and character; the ground mown, worn and jointed;
   the goose statue, the drained fountain, the flagpole plaza, the alumni tent, the bonfire's site, the tower's base, street
   furniture; light pools, haze, flags and smoke moving, an outdoor soundscape. (`HouseCampusPolish*`.)

4. **After Phase 4 (approved extras).**
   - The clock tower is walk-in: an oak door, an open-well stair of 143 steps to the belfry and its bells.
   - The town of Calder beyond the campus is a small mountain college town:
     - three ridgelines in fall colour and blue haze;
     - Main Street, the courthouse dome and the church steeples;
     - the river, the mill, houses up the slope, the CALDER water tower;
     - the radio mast and the fire lookout.
   - Four fixes on the house's side of the street:
     - the wall-gap at the backyard court;
     - a WALK / DON'T WALK head at the new crossing;
     - the junction's street blades;
     - the stop bar.

Saturday 18 October 1986, after the final whistle: GEESE 24, DUNMORE 17. Every name is invented (Calder State, the Geese,
Dunmore, the buildings, the streets); nothing is later than 1986; no real brands, schools, teams, people, flags or seals.

## How it is built

Everything is built at runtime in code, the same on every machine; nothing is saved into the map.

- **The campus plan** is data (`HouseCampusLayout.cpp`) that the exterior, the trees, the interiors and the polish all read.
  Tree planting never sees it: the old footprints stay as fixed reservations, so the tree random stream is untouched
  (`WOODS_PASS_ORIGINAL_TREES crc=0f3b2bb7`); a tree that would stand in the new plan's paving or buildings is hidden when
  the batches are built (one tree).
- **The interiors** are a transient actor every machine builds the same way; the server alone spawns the replicated doors
  (`ACampusDoor`, with the locked reason the HUD shows) and props (`ACampusProp`: the house's own furniture meshes and damage
  stages; their breaks are not house damage). A building's inside is built when someone comes within 35 m and freed 10 s
  after everyone has gone beyond 60 m. Each building is one file on a registry (`FInteriorDef`).
- **Light.** The exterior keeps its 53 lights; no light was added outside. Inside, a pool of 6 shadowless point lights on
  lighting channel 1 follows the local view (the interiors and anything in them are lit on channel 1 only, so no street light
  leaks in and none leaks out). Everything else that glows is emission; the light the lamps and windows throw outside is
  emissive decals.
- **Sound** is synthesised and fed as procedural waves; nothing is a recording.
- **Drawing.** The campus's geometry is made as procedural meshes and drawn as static meshes built at runtime
  (`HouseCampusStaticMesh`), so the renderer caches its draws. A procedural mesh is gathered again every frame on the
  render thread, which is what had made the campus views slow. An inside is converted a piece at a time after it is
  built. Inner doors are drawn only while their building's inside is built.

## Switches

| Switch | Gives back |
|---|---|
| `-NoCampusLayout` | The old campus exactly (and with it no interiors and no polish). |
| `-NoCampusInteriors` | The solid blocks and the closed stadium (Phase 1's campus with Phase 3 still on top). |
| `-NoCampusPolish` | The campus as Phase 2 left it. |
| `-NoCampusFall` | No homecoming dressing (as before the job). |
| `-NoCampusSound` | No campus sounds (interiors and outdoors). |
| `-NoClockTower` | No clock tower (and so no walk-in tower). |

Development switches: `-CampusLayoutReview`, `-CampusInteriorsReview=<Ids>` (walks), `-CampusInteriorsSmoke=TierA` (smoke
tests; `=All` or building Ids too), `-CampusInteriorsAll` (every inside built, for captures), `-HouseExteriorViews -ExteriorViewFilter=...` (the shots),
`-CampusSoundDump`, `-CampusSoundRecord=<s>`, `-CampusPolishRecord=<s>` (sound proofs; add `-muteaudio` to keep the speakers
silent while recording), `-CampusPolishSkip=<parts>`, and for frame times `-ExteriorViewCsv` (a CSV profile a view) and
`-ExteriorViewToggle` (an A/B inside one run). On this Mac, add `-MetalOffscreenOnly` for frame times: a windowed game is
otherwise locked to the display's 120 Hz.

## New assets

Two material assets, made with the project's editor-Python pattern (`Tools/claude/build_campus_materials.py`) and put on
the cook list in `Config/DefaultGame.ini`:
- `Content/SeniorSendOff/House/CampusInterior/M_CampusInteriorWindow`: interior mapping for the closed floors' windows.
- `Content/SeniorSendOff/House/CampusInterior/M_LightPoolDecal`: the emissive light-pool decal.

Every texture is painted in code at runtime; every sound is synthesised. No CC0 or other outside sources were used. The
house's own meshes (the basement library's furniture, the bedrooms' pieces) are reused by spawning them at runtime.

## Per-building folders

`SourceArt/CampusInteriors/<Building>/` for all 18 walk-in buildings (the clock tower too), the way each house room has its folder:
- `README.md`: identity, plan block, entrances, rooms, every door and its locked reason, the smoke test and the walk;
- `references/`: the Tier A floor plan drawn for PLAN.md, and the building from straight above before the job and
  after;
- `shots/`: the walk's photographs;
- `evidence/`: the smoke test's JSON, the walk's log for that building, the verdicts.

`Saved/CampusMasterWork/tools/make_building_folders.py <run>` regenerates them.

## Verification

The final set (STATUS.md, "Phase 4"), all on the finished code:
- **Reviews.** Every one passes: layout, woods, campus woods, boundary, surroundings, parking, free roam (campus route
  20/20), clock tower, and the interiors' walks (Tier A with the clock tower 256/256 stops, Tier B and C 174/174). They pass again with
  `-NoCampusPolish`, `-NoCampusInteriors`, `-NoCampusFall` and `-NoCampusLayout`.
  - 53 exterior lights, none added.
  - Interior light pool of 6.
  - Tree stream `crc=0f3b2bb7`, unchanged.
- **Smoke tests.** All 18 walk-in buildings pass. The house's own 16 room smoke tests give the same result with the
  campus on and off; the ten that fail did so before the job.
- **Frame times.**

  | Where | Cost against the campus before the job |
  |---|---|
  | The street views | -0.1 to +0.1 ms |
  | The campus views at eye level | +0.2 to +0.6 ms |
  | The library forecourt (three open buildings built round it) | +1.5 ms |
  | The aerial view (the whole campus and the town) | +2.5 ms |
  | Inside the Tier A buildings | 79 to 143 fps |

- **Five code reviews** were adversarially verified: one after each phase, one on the performance pass and one on the
  extras. All
  confirmed findings are fixed.
- **Write-up.** `campus_master.patch` is the patch against the pre-job snapshot, and [FILES.md](FILES.md) is the
  per-file summary.
