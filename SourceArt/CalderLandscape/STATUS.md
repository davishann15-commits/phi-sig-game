# Calder in the mountains: STATUS

The job: `MASTER_PROMPT.md` in this folder (with my additions at the end). Work folder: `Saved/CalderWork/` (tools, runs, plan,
baseline). Nothing is committed.

<!-- OWNER_WILDLIFE_REVISION_STATUS -->
## Current owner wildlife and nightfall milestone, 2026-10-18

The requested deer, geese and crow revisions are installed and verified in normal Play. Other species are deferred by the owner. The owner approved the21-control Fab deer gait direction; actual reactions, bounded calm walking/grazing, recovery, sole contact, goose arrival/shore effects/calls and crow roost/calls passed native checks. Longer recorded crow/raven calls are the specific owner-authorized synthesis exception; original twelve sound buffers and approved goose honk remain byte-identical. Normal/disable/indoor/lawn and real lobby MP checks pass. Original21-view/3alternating performance pairs pass all63 wildlife CPU limits (max p95 .124ms), listed absolute frame limits, and five interior noise medians. Wildlife recovery is VERIFIED325text/108binary/7638evidence at `Saved/CalderWork/codex_checkpoint_owner_wildlife_20261018_v1`.

The approved30-minute dusk-to-night progression is now implemented and holds at night. Native0/900/1800/3600s stages, unforced game clock, NoNightfall/NoTerrain/NoLandscape paths and all54 captures pass/reviewed. Normal lobby MP passes23.26ms data/58.12ms completed client GPU frame; after excluding scene-generation clamping, shared clocks differ by at most.518s in two overlapping steady intervals (original failed analysis retained). A real50s play window differs from wall time by.007s. These are pinned-stage plus real-clock checks, not a30-minute soak. Current editor and standalone (255actions/134.05s)/strict signature pass, all420 prior Source/Config files remain byte-identical; only new HouseCalderNightfall.cpp adds this feature. Protected13source/map and90donor hashes pass; no map save or commit. Prior wildlife performance is not relabeled as whole-master acceptance of the new nightfall feature.

Final recovery target is `Saved/CalderWork/codex_checkpoint_nightfall_20261018_v1/recovery_receipt.json`; present `passed=true` is authoritative. Prior immutable checkpoints remain untouched. Source/engine reservation is held through final recovery, then explicitly released to the authorized Fraternity Row chat; see `Saved/CalderWork/nightfall_20261018_v1/reservation_release.json` for the release record. No further species work is part of this milestone. Original full Calder load/hill-route/final-review and photographic quality acceptance remain separate.
<!-- END_OWNER_WILDLIFE_REVISION_STATUS -->

## Decisions from the user (2026-10-15)
- **Hands off:** never touch the main frat house, the basketball court, the party area (Fall Jam field) or the parking lot.
  All four are inside the frozen zones, and their own source files are not edited.
- **New quality bar:** the woods and the pig are NOT the model. Both get redone "hyper realistic", and the bar is real
  photographs.
- **Assets:** free only, from CC0 libraries or free Fab listings. "As long as it's free and can't get copyrighted, sky is
  the limit."
  - The user acquires Fab items; Claude never signs in.
  - Downloads: one list, one "yes" for the batch.
- **Order:** terrain first, then the woods and pig redo, then the wildlife.

## Done
- **Coordination.** Codex has been idle; the project was last changed 2026-10-14 22:07 (by Claude). No editor was open.
- **Snapshot** in `Saved/CalderWork/baseline/`, taken 2026-10-15 00:05: Source, Config and Tools (320 files with SHA-256
  sums), `git_status.txt` (425 lines) and `git_diff.patch`.
- **(1a) Two-player test, on today's build: PASS** (`Saved/CalderWork/tools/campus_mp.py`, run `mp_base2`, 98 s).
  - **Setup:** a listen host and a client, both headless and muted, joined through the real lobby (`SetCharacter`,
    `SetReady`, `StartStory`).
  - **Client checks:**
    - all 114 doors within 120 m, plus the tower's 2, matched the host's lock reasons and states;
    - Mercer was built round the client's player, with all 63 props and their meshes.
  - **Interaction:**
    - the client's E opened both of Mercer's main leaves, on the host and on the client;
    - a prop the host moved arrived on the client.
  - **First run failed, and why:** the client stood 3 m from the doors, but door focus reaches only 2.7 m from the eye.
    It now stands 1.7 m out and aims at the middle of the pair.
- **(1a) Map actors and the fall limit** (`-CalderSiteDump`, `HouseCalderSiteDump.cpp`):
  - one PlayerStart, at (1184, 624, 100.8), inside the house;
  - **FallLimit = -1099.2, so walkable ground must stay at or above -699.2;**
  - 1,957 startup actors (1,309 house props, 254 static meshes, 99 point lights …), **none outside the frozen zones**, so no
    hold-zero discs are needed. The list is in `Saved/CalderWork/plan/map_actors.json`.

- **Baselines of the reviews the last job never ran** (`Saved/CalderWork/tools/base_extra.sh`, run `b1a_extra`, today's build):
  - **PASS:** WoodsScene, WoodsWalk, WestField, WestFieldWalk, HouseTrees.
  - **FAIL, already, so the gate for them is "same verdict":**
    - `-HouseExteriorWalkReview`: "Front site surface incorrect at 800,-4025 … EX_Concrete". It expects another sidewalk material than the street polish gives.
    - `-HousePigAnimalSmoke`: "Far physical observer has no clear floor". Its observer stands on an old upstairs test course. It is fixed in the pig redo.
  - The 16 house room smokes: 10 already failed before the campus job (p8z, identical with the campus on and off). Their gate is the same verdict too.
- **CC0 assets downloaded** (the user approved them: "yes fetch, download whatever you need"):
  - 266 files, 1.21 GB, from Poly Haven, ambientCG and OpenGameArt, into `~/SeniorSendoffCC0Raw/` (outside iCloud);
  - the manifest `CC0_SOURCES.csv` holds the URL, licence label, bytes and sha256 of every file;
  - fetcher: `Saved/CalderWork/tools/fetch_cc0.py`. It uses curl, because this Mac's python.org Python has no CA bundle.
  - Fab items the user adds to the project: the Animal Variety Pack, and Megascans Trees (European Beech, Norway Maple, European Hornbeam, Common Hazel).
- **(1a) Plumbing at zero relief: code done, gate running.**
  - **New:** `HouseCalderTerrain.h/.cpp` (gate, 2 m relief lattice, SurfaceZ, pads, lift, valley and ridge, collision-ready) and `HouseCalderLandform.h/.cpp` (the analytic landform; zero until the 1c port; it holds the frozen rectangles).
  - `HouseExteriorSite::GroundHeight/GroundNormal`: today's Grade plus the lattice relief. Exactly Grade inside every frozen zone.
  - `HouseCampusLayout::BlockDZ/Frame/PaveKindAt`. Interiors: `InteriorBox`, `InteriorFloorZ`, `Frame` and `FloorOf` are pad-aware; `OpenTopWorld` and `BaseDZ` are new. `HouseClockTowerSite::Lift()`.
  - **Routing:** five agents on disjoint files routed every ROUTED, TOWER-LIFT, FAR and REVIEW row of `plan/zlits.csv` (845 classified height uses), plus the tree re-seat after the crc record (Z only; frozen trees skipped) and the capture-time view offset.
  - **Adversarial verification** (`plan/routing_verify.md`): no value, count, branch, RNG draw or NoGo changes at zero relief. 3 small fixes, all identical today.
  - **Gate so far (run `g1a`, same numbers as before):** `crc=0f3b2bb7`, `grounds=69240`, `instances=53740`, `campusFrontVertices=1091586`, `floorSamples=2654 floorMisses=0`; reviews, FreeRoam and the clock tower PASS. The Tier A interiors walk is identical to p8z (256/256 stops).

  - **1a closed** (run `g1a_close`, rebuilt with the verifier's three fixes and a pin that keeps the ground exactly today's inside every frozen zone): `CALDER_TERRAIN hash=1594102d lattice=321x321 nonzero=0`; reviews PASS; `grounds=69240`.
- **Materials and textures** (`Tools/claude/calder_materials.py`, run headless with the editor closed; independently re-checked; see `ASSETS.md`):
  - 164 CC0 textures under `/Game/SeniorSendOff/Calder/Textures`, each checked against its download sha256.
  - `M_CalderGround`: lawn, meadow, litter, clay and rock, blended by slope and vertex colour (R rock, G litter, B clay, A cavity), 12 texture samples.
  - `M_CalderRock` and `M_CalderWater`.
  - All three compile on METAL_SM6 with 0 errors. The cook directory was added to `DefaultGame.ini`.
- **(1b) Terrain tiles at zero relief: PASS** (`HouseCalderTerrainTiles.cpp`, `-CalderTerrainReview` in `HouseCalderTerrainReview.cpp`).
  - **Tiles:** outside the frozen zones, today's ExPatch grass gives way to 24 tiles (128 m) built from 318 regions. Pieces are split at the frozen edges, and building footprints are holes. Every triangle lies on one lattice triangle: 180,585 tris, a runtime static mesh each in `M_CalderGround`, built in 0.22 s.
  - **Collision:** a hidden procedural mesh per tile, cooked at load in 67 ms. The asynchronous cook only finished after the rest of the load, about 22 s.
  - **Terrain review:** 20,000 points, 0 holes, max error 0.00 cm. The frozen-zone 10 m grid (613 points) is identical with and without the terrain (0.000 cm). 42 point and spot lights outside the house both ways.
  - **Every review PASS:** layout (2654 samples, 0 misses), CampusWoods, SurroundingsPolish, FraternityBoundary, ParkingLot walk, WoodsPass (53 lights), FreeRoam 20/20, `crc=0f3b2bb7`. `grounds` 69240 → 64594 (the patches the tiles replace).

  - **Frame-time A/B of the swap alone** (`tools/perf_ab.sh p1b -NoCalderTerrain`, three alternating rounds, medians, `-MetalOffscreenOnly`, uncapped): **the tiles are faster in every view**, because today's ExPatch ground is a procedural mesh redrawn every frame.

    | View | On | Off | Δ ms |
    |---|---:|---:|---:|
    | front_street | 23.47 | 23.61 | -0.14 |
    | street_along | 15.28 | 15.34 | -0.06 |
    | detail_street_dusk | 14.92 | 15.01 | -0.09 |
    | detail_establishing_corner | 18.94 | 19.05 | -0.11 |
    | detail_campus_quad | 9.36 | 10.05 | -0.69 |
    | detail_layout_quad | 10.92 | 12.32 | -1.40 |
    | detail_layout_gateway | 8.09 | 8.91 | -0.82 |
    | detail_layout_dormrow | 8.33 | 8.51 | -0.18 |
    | detail_stadium_gate | 7.83 | 8.39 | -0.56 |
    | detail_clocktower_far | 7.63 | 8.38 | -0.75 |
    | detail_layout_aerial | 16.33 | 16.96 | -0.63 |

    Absolute times run about 1 ms above p8p because the machine was busier (agents working). Only the A/B column is comparable.
- **(1c prep):** `HouseCalderDrape.h/.cpp` cuts any convex footprint along the lattice lines and diagonals, so walks, ramps, curbs, lawn fills, paint and overlays follow the relief exactly, with side walls to the ground. Two agents are converting the layout, polish ground, street detail and homecoming ground primitives; each stays dormant (today's exact shape) wherever the relief under it is flat.

- **(1c) in code, not yet built or run with the relief on (2026-10-15 ~05:45):**
  - **Baked landform:** `Saved/CalderWork/tools/bake_landform.py` writes `HouseCalderLandformData.inl` from `plan/landform.py` (183 PASS / 0 FAIL). It holds the lattice, the pieces of 3,142 wall-split cells, 16 pads, the tower lift, 257 walls, 40 stairs, 6 ramps, 6 frozen rects, 20 hidden trees and 4 reserves.
  - **Loader and walls:** `HouseCalderLandform.cpp` loads it. `HouseCalderTerrain` evaluates the wall pieces per side, and the tiles draw wall cells exactly. `BaseGrade` is exported. Landform-hidden trees go through `HouseCampusLayout::TreeHidden`.
  - **Dormant pieces:** drape (`HouseCalderDrape`), building foundations and hedge segments are all dormant on flat ground. The features (`HouseCalderFeatures`: walls, rock faces, stairs, ramps, rails) are hooked up after the tiles.
  - **Landform fixes still needed:** see the features report (stairs with no rise or head=foot, walls across walks with no stair, ramp overlaps, Crow Ledge unreachable) and the foundations report (ground above the floor at building corners: MUS +2.4 m and others; the court strips must wrap the corners).
  - **Next:** rebake from the revised landform, build, run every review with the relief on, take the P1-P8 views, fix.

- **First run with the relief on (2026-10-15 07:53, runs `r1c_reviews`, `r1c_shots`; pre-revision bake):**
  - **Terrain:** `CALDER_TERRAIN hash=50141f52 nonzero=82980 relief=-510..4621 tower=700`, tiles 182,562 tris. Features 202 walls (4.2 km), 32 stairs (498 steps), 6 ramps, 101,876 tris, built in 0.16 s. Foundations on 15 buildings. Tree crc unchanged.
  - **The layout review FAILs on landform data**, as expected (the revision is fixing it): the front-row walk at Y -6625 runs about 33 cm under the stoops on raised pads, axis stairs leave gaps, apron ground 7 cm off the pad at COM.
  - **Views:** P1 shows the raised promenade and the stone street wall (good). P2 stands 3.5 m in front of a terrace wall (re-pick it). P4 stands on the Senior Steps (move it to X -13500). P5 stands inside the campus hedge at Y -29800 (the overlook needs the hedge cut back, step 1e). The aerial shows College Hill rising behind the tower and stadium.
  - **Open issue:** in the orthographic plan views the terrain tiles don't show (perspective views are fine). To investigate.

- **(1f) The far land, ported and in the game** (`HouseCalderFarLand.*`, `HouseCalderOuterLand.*`; verified in `plan/farland_verify.md`):
  - **Port:** an exact port of `landform.py`'s outer land. The in-game `-CalderFarLandSelfTest` reads `maxErr=0.000000` at 2000 points.
  - **Outer land mesh:** 3 rings, 12 meshes, 80,551 tris, built in 57 ms, no collision. It meets the walkable tiles with `seamMaxGap=0.0002` cm.
  - **Town and ridges:** the old town ridges and valley floor are replaced. The town stands on the sunken valley floor. The north ridges are re-based, with only their Z changed.
  - **Views (run `r1f_shots`):** from the street, mountains now rise behind the frat house, and Fraternity Park Dr ends against a mountainside.
  - **To fix in the landform:**
    - Gosling Knob came out as a 400 m needle on a 140 m radius (a units slip).
    - The outer land has steps of 13-35 m along Y +32000 east and west of the square, 4.85 m along X +32000 north, 0.9-3.8 m at the river's far bank, and up to 3.75 m at the side walls' foot.
    - After fixing them, rerun `plan/farland_port_check.py` and re-sync the port.

- **Integration fix (2026-10-15 afternoon): every review PASS with the relief on** (log: `Saved/CalderWork/plan/integration_fix.md`;
  runner `Saved/CalderWork/tools/fix_checks.sh <tag>`; final runs `f1_*`, shots `fix_shots`).
  - **Was failing:** layout floorMisses=178 / walkObstructions=108, Tier A 223/256 (Union west door), Tier B/C 151/174.
  - **Causes and fixes:**
    - the drape ignored walls (walks interpolated across steps) and laid paving under stairs and ramps: it now cuts along
      wall-split cells' own triangles and cuts the stair/ramp footprints out (`HouseCalderDrape`); plazas drape where the
      relief or a stair needs it;
    - wall bodies (40 cm on the high side) and coping overhangs stood in walks along the top of walls; rock faces jutted
      into walks in cuts: bodies go to the low side by paving, copings never overhang it, rock keeps 22 cm off it
      (`HouseCalderFeatures`); stair cheeks stop at walls alongside;
    - aprons/pads/stoops/planters stood over entrance stairs: the bake exports the landform's apron depths, the game builds
      to them; stoops, planters and canopy posts stay off Calder stairs;
    - the bake split wall cells along a wall's whole line (a false tapered crack past a wall's end): free ends now taper the
      step to nothing, off-wall vertices are single-valued, edge points shared with neighbouring cells;
    - landform data: Old Main's 6 entrance risers moved off the promenade (2.2 m landing + a strip at the promenade level),
      the Union steps (31 risers from the tower plaza down to the Union's west court), Kerr's north court clear of Ashby
      Lane's sidewalk, Calder Hall's landing and walk_10, Band and Music's west landing, walk_39 and Kerr's east areaway,
      FLIGHT_SHOULDER 25 cm. `landform.py quick`: 329 PASS, 0 FAIL.
  - **Results:** layout PASS (4223 samples, 0 misses, 0 obstructions, 53 lights), terrain PASS (`hash=a4a044fd`, 0 holes),
    FreeRoam 20/20, Tier A 257/257, Tier B/C 174/174, smoke 18/18, clock tower PASS (crows 12), the woods/polish set PASS,
    crc `0f3b2bb7`. `-NoCalderTerrain` gives today's gate numbers (grounds 69240, instances 53740, campusFrontVertices
    1091586, floorSamples 2654).
  - **Views:** P2 levelled, P4 on Q3 up the axis to Mercer, P5 absolute and aimed at the town under Gosling Knob (the south
    hedge leaves the overlook's 20 m open), P6/P7 added (absolute deck eyes).
  - **Independent verification (evening):** a second agent rebuilt and reran everything: all PASS, and a read-only audit of
    every review change found no loosened check (`LOOSENING=none`); protected files byte-identical. One Tier B/C timing
    flake (a stop placed too close to the bottom step) was fixed. One pre-existing Tier A flake at the stadium vomitory
    (1 run in 6; the stadium is untouched) is logged in `integration_fix.md` for the next step.
  - **Still open:** a short terrain slit at the Union's NW corner (a landform step within 30 cm of walls); P8 looks into tree
    crowns; no water yet for P6/P7.

- **Cracks fixed, builders' data exported (2026-10-15 evening).** Details: `Saved/CalderWork/plan/data_exports.md`.
  - **Bake.** `bake_landform.py` now writes one conforming mesh:
    - no T-junctions;
    - heights per sector, so pieces differ only across declared walls and building faces;
    - wall cells split on the lattice diagonal and on building faces;
    - every piece fan-safe.
  - **Bake crack check.** It refuses to write the .inl if any sample is over 3 cm.
  - **Cracks: 67 over 3 cm before, 0 now; 0 over 0.5 cm.** The Union north-west slit is gone.
  - **Landform causes fixed:**
    - the walk_23/30 and walk_14/15 cut corners;
    - the knoll's 0..15 deg edge;
    - the wall-corner shadows;
    - the dam run-on;
    - the wall-end snap;
    - the visitor lot's order;
    - 5 kerbs.
  - **New quick checks:** `check_wall_ends` and the near-wall part of `check_c0`. Quick is 339 PASS, 0 FAIL.
  - **Tile skirts** are clipped to their region (`HouseCalderTerrainTiles.cpp`).
  - **New water design:** the spillway race and the rock house shelf.
  - **Exported tables and accessors in `HouseCalderLandform.h`:**
    - water: the pond, the creek, the glen, the falls, the cascade, the pool, the headwall, the crossings, the dam and
      race, the stones, the dock, the rock house, the pond loop;
    - roadside: guardrails, ditches, catch basins, culverts, rails needed;
    - edges: closures, hedge segments and gaps;
    - overlooks, the eye spots and the levels.
  - **Runs.** dz1 layout, terrain and roam all PASS (terrain hash c686da03). `-NoCalderTerrain` gives today's gate.
  - **Open:** the road bridge has no road to carry.

- **(1d/1e) Water, crossings, roadside, edges and the reach checks, integrated (2026-10-15 night to 10-16 morning).** Log:
  `Saved/CalderWork/plan/integration_1de.md`.
  - **Wired:** water, roadside and edges builders (dormant with `-NoCalderTerrain`), foot points, the edges' hedge patch, the
    distant forest hidden near Crow Ledge and in the P8 view down to the old property (100 of 852; crc unchanged), the reach
    review and probe in every self-exiting list.
  - **Fixed on the way:** collision cracks and >40 deg walkable triangles, rock-cut rails, Founders Way and the lots split or
    draped at frozen edges, lamps/benches/hydrants off stepped walls, sunken service courts given exit stairs (landform.py, rebaked:
    hash `78d6c86a`), the falls (dark streaked water, white lip and foot, frayed edges, a gap; P6 reframed), landings over the
    dam's run-ons and ramps off the causeway's ends (they tripped the two-player walk).
  - **Results (runs w1q/w1s):** layout PASS (4394 samples, 0 misses, 0 obstructions, 53 lights), reach PASS (traps 0), FreeRoam
    PASS (hill 71/71), woods/polish/boundary/parking PASS, clock tower PASS (crows 12), smoke 18/18, Tier A 257/257, Tier B/C
    174/174, two-player PASS (hashes match), `-NoCalderTerrain` gate exact, extras = baseline verdicts, `-DisableUnity` build
    clean. Load 10.1 s (budget 13.0). Frame time A/B within noise; street views over p8p in absolute terms on this loaded
    machine, but so is today's state (off) by the same amount.
  - **Open (the user's call):** terrain review sightline fails only on the frozen garden oak's canopy box; the two-player walk
    gets one server correction per run, always on a long client frame (interiors streaming / garbage purge), not on geometry;
    P8's house roofs sit behind the frozen woods. Mist at the falls not built.
  - **Verified 2026-10-16 (adversarial pass, `Saved/CalderWork/plan/integration_1de_verify.md`, runs v1de*/v1de4*).** Found and
    fixed: Calder Dr's frozen north end 0.1-2 cm high (street patches now cut at frozen edges), two wall-coping corners 10 cm
    inside frozen rectangles, and a terrain-review gap check that counted battered rock faces closed without checking their
    foot (now checked: 0 open). Sight line re-based to today's line for frozen trees (user sign-off wanted). After the fixes
    every review passes except the two-player walk (engine move-clamp asymmetry on client hitches; diagnosed, not geometry)
    and the two baseline failures; `-NoCalderTerrain` gate exact; `-DisableUnity` clean.

## Codex takeover - 2026-10-16

- Owner explicitly handed over this checkout; Claude's last recorded source edit was 06:33. A separate source/config/tools/docs/map snapshot and SHA-256 manifest are in `Saved/CalderWork/codex_takeover_20261016_111615/` (369 files), including the pre-Codex Git status and binary patch. No commits or map saves.
- Created the previously missing `PLAN.md`, linking the existing site/contour/section evidence and preserving the full master specification and verification chain.
- Unchanged two-process hill walk, `codex_takeover_mp`: host/client interaction checks PASS, terrain hashes both `78d6c86a`, route finished (71 waypoints); overall **FAIL, 10 server corrections during the walk**. Client hitches reached 2.738 seconds near the start, and there were later 253/293 ms frames. This is recorded as an unresolved network/performance failure, not waived. No movement or review limits changed.
- Corrected `HouseCalderWater::WaterAt`: clip pond/race queries at the downstream weir face, sample the weir drawdown, use actual built floors/decks, and bound creek/glen domains. Added independent checks against exported creek/glen water levels to `CalderTerrainReview`. Build PASS; `codex_water_query` terrain PASS, including 165 independent water-query samples (0 failures), 20,000 terrain samples, hash `78d6c86a`, crc `0f3b2bb7`.
- Owner confirmed Unreal was closed and authorized the asset batch. `Tools/calder/phase2_assets.py` completed: existing acquired pig/deer/fox/crow donors exported for offline rigid-part preparation, and a separate experimental falling-water material compiled. The latter is **not hooked up**; it must conform to the opaque-water requirement and pass visual/performance review before use. No map saves or changes to original Fab assets.
- Woods appearance first pass: four floor material keys and five species bark keys use already-imported CC0 scans. `codex_woods_scans`: WoodsPass and CampusWoods PASS, 14 comparison captures, 53 exterior lights, original tree crc unchanged. Visual review still shows overly simple tree shapes, so the woods redo is **not complete**. A subsequent non-colliding fern/stump replacement uses acquired scans in the existing two HISM batches (677 instances, no new batches, no collision or Nanite); its full woods review remains pending.
- Pig donor converted offline to 12 static parts, each with three static LODs, preserving source UVs/materials. New assets live only in `/Game/SeniorSendOff/Calder/Pig`; runtime skeletons remain zero. Import checks now verify axis/scale bounds. `codex_pig_visual` assembly/capture probe passed, but inspected images exposed inward winding and poor joint rendering: **visual failure, not acceptance**. Winding corrected and reimported cleanly; fresh images and physical tests pending.
- Fixed the pre-existing `HousePigAnimalSmoke` fixture's removed upstairs observer/course references: it now observes from the actual player spawn and exercises existing authored nodes inside the current pigsty. No room, graph, movement, thresholds, tolerances or timeouts changed. Build PASS; runtime verdict pending.
- Subsequent pig iteration: stopped forcing the donor into 25/23 cm links. The new appearance uses native anatomical static-part pivots, lengths and bend directions; the original comparison retains its old rig. Fixed FBX UV seam welding/winding, centred the visible soles on their real endpoints, sampled all four visible supports and made the sniffing neck follow the actual snout surface, including the mattress. No capsule/AI/room/replication changes.
- **Animal smoke PASS**, `codex_pig_authored_animal` (78.17 observed seconds): Pepper hoof gap p95 2.10 cm, drift 0.024 cm; Coal 3.78 cm, drift <0.001 cm; both snout gaps 2.00 cm, with independent natural sniffs/resumes, running ears and recorded audio. Existing limits unchanged. Original appearance with the repaired fixture (`codex_pig_animal_original`) failed around the mattress; that baseline failure is retained.
- Uncut static donor control (`codex_pig_donor_control`) inspected in game: intact source anatomy/materials. Animated cuts still showed visible seams, so **pig visual acceptance remains open**. Staged a narrow torso joint blend material (`Tools/calder/build_pig_skin.py`) and vertex-colour collars driven by the five existing rigid transforms. No runtime skeleton, procedural mesh rewrite or extra draw batch. The material/import batch and final appearance/contact/interaction reruns remain pending; do not treat the preceding smoke as final approval of this newer skin change.

### Latest checkpoint - 2026-10-16 14:10 EDT

- Full DisableUnity build PASS (197 actions). No commits or map saves; all 13 explicitly protected source/map hashes still match takeover.
- Crow Ledge actually raised 6.5 m, relief +25.5 m, five buildable timber flights/landings. Full authoritative plan: 346 PASS/0 FAIL; contours/sections/report regenerated. Baked terrain hash `ff4f9ab1`; P8 eye reads actual ground +170 cm.
- `codex_crow_runtime`: terrain and layout PASS. Collision 20,000 samples, zero holes, max error .03 cm; frozen grid 685; 165 independent water queries PASS; all terrain subchecks PASS. Layout 4,565 floor samples, zero misses/obstructions. 53 lights, crc `0f3b2bb7`, exterior build 10.384 s. Eight P1-P8 captures saved; P8/P6 inspected. P8 house roofs still not clearly proved.
- Active: sequential `codex_crow_verify` roam/reviews/terrain-off/reach-baseline/reach. See its root log; do not overlap another engine. Isolated `codex_crow_roof_trial.py quick` tests a further ledge raise but is not applied to the authoritative terrain.
- Pig native relaxed sleep removes earlier severe torso folding. Six `codex_pig_relaxed_sleep` captures PASS and all inspected. `codex_pig_relaxed_animal` PASS at 59.28 s: both pigs naturally sniffed 3 times, resumed travel, ran with independent ears and actual snort playback. Pepper hoof p95/drift 3.62/1.61 cm; Coal 3.87/<.001 cm; snout p95 2.01/2.00 cm, min ~1.98 cm. Test limits unchanged. Continuous static skin remains 6,744 triangles/four texture samples, 12 rigid transforms, no runtime skeleton or mesh rewrite.
- Extra pig resize/escape: same original/new failure at open door graph join; no protected door/room fix. Carrying remains unverified.
- Initial woods scans/atlases/fern-stump replacements and broader added crowns: WoodsPass/CampusWoods PASS (`codex_crown_water`), 14 woods comparisons, 53 lights and original CRC. Woods still need visual finish. Native acquired tree sources require reduced conventional derivatives before use (Nanite and million-triangle intermediate LODs found).
- Falls/stream assets compile: opaque, 305 instructions and 3/7 texture samples. Four bounded local mist volumes, frozen envelopes/global atmosphere/original hollows preserved. New stream foam fixes the leaf-photo mask; finer falls detail visible, but remaining water/rock-edge visual polish is open.
- MP baseline remains FAIL, 10 corrections during the walk correlated with initial interior construction/conversion hitches and a later 353 ms frame. No movement/harness limits changed.
- Local checkpoint patch/archive created; test-apply and exact hashes PASS for 45 text files. `README.md` and `FILES.md` describe current partial delivery and remaining work. Regenerate package after further edits.

### Verification follow-up - 2026-10-16 14:32 EDT

- `codex_crow_verify` complete: all routes PASS (3 boundary legs, 20 campus, 71 hill, 5 north), five surroundings/woods/boundary/parking reviews PASS, terrain-off layout/woods PASS. Reach baseline/on PASS: 47 entrances, 6 overlooks, 8 eyes, 78 stair endpoints, 12 ramp endpoints, 2,563 path targets, 1,848 drop-foot seeds, zero traps.
- All eight P1-P8 images inspected. Passive P8 roof diagnostic reports 8/8/9 canopy crossings at the three protected ridge centres; whole-canopy upper-bound clearance is much higher than the current eye. No roof-visibility gate was loosened. Higher +30m isolated hard-core trial has 346 PASS/0 FAIL but is unbaked; current terrain remains `ff4f9ab1`.
- Clock regression PASS and campus smoke PASS 18/18; `codex_scope_guard` physical interiors still running. Six-light interior pool observed unchanged. A mist actor-name outer fix is written but needs rebuild. MP and three-round timing reruns remain outstanding.

### Full campus regression checkpoint - 2026-10-16 14:55 EDT

- `codex_scope_guard` complete: clock PASS (two completed chimes, 12-crow burst, 53 lights); campus smoke PASS 18/18; physical interior Tier A/tower PASS 257/257 stops and other buildings PASS 174/174 stops, zero problems. All 18 door groups opened, locks/reasons held, 65 wall clocks correct, six-light pool preserved, all 18 interiors freed after departure. The previously flaky stadium return passed this run.
- Expanded scope hash audit PASS: all 319 snapshotted source/config/map files outside the approved edits remain unchanged. HouseExterior.cpp's only takeover delta is P8's ground-relative capture calculation; its house geometry is unchanged.
- Final DisableUnity rebuild is in progress (`codex_checkpoint_disable_unity.log`) after the passive roof diagnostic, unique mist-name outer fix and comment-only bake refresh. Terrain numeric data remains the verified `ff4f9ab1` design. Idle paired MP and three alternating appearance/terrain rendering rounds remain next; full Phase 2/3/4 completion is still open.

### Multiplayer follow-up - 2026-10-16 15:02 EDT

- Latest final DisableUnity build PASS, 197 actions (`codex_checkpoint_disable_unity.log`).
- Idle rendered `codex_checkpoint_mp`: both functional peers PASS, same `ff4f9ab1` hash, all 71 waypoints physically walked; **overall FAIL**, 4 corrections during walk (7 whole run), 305.4 s. Two were ~10 cm after 149-150 ms frames; two were ~1.95 cm later. No thresholds, movement settings or harness altered. Near the first hitches the measured native interior construction/conversion was ~29/7 ms, so those timings alone do not explain every long frame.
- A separate unchanged-harness `-NullRHI` paired run is now in progress to separate rendering stalls from simulation. It is diagnostic; do not relabel the rendered failure as a pass. Three-round rendering comparisons follow. The checkpoint package was refreshed and test-applied with exact hashes for 46 text files; regenerate after documentation updates.

### Headless multiplayer diagnostic - 2026-10-16 15:07 EDT

`codex_checkpoint_mp_nullrhi` PASS: both peers and matching `ff4f9ab1`, all 71 waypoints, **zero corrections during the walk** (3 initial setup corrections outside the measured interval), 276.9 s. Exactly the existing harness with `-NullRHI` supplied to both peers; no movement/clamp/review changes. This separates a passing simulation route from the rendered Metal run's four walk corrections; the rendered failure is not waived. Three alternating rendering rounds are now running for appearance and terrain, including five Tier A interior eyes.

## Codex performance checkpoint - 2026-10-16 15:42 EDT

- Completed all twelve rendering captures: three alternating pairs for appearance and terrain, 16 eyes per run, original warm-up/sample method, occlusion culling on. Strict report confirms all rounds complete. Tables: `performance_checkpoint.md`.
- Appearance deltas -0.13..+0.25 ms; all five interiors within +/-0.5 ms noise. Terrain streets/campus/aerial meet p8p budgets; four interiors within noise, **Harlow +0.61 ms outside the interior noise requirement**. Whole performance gate is not passed.
- Current-on exterior load medians 10.085/10.240 s, below the 12.993 s limit; terrain-off median 8.718 s. Every run retains crc 0f3b2bb7 and 53 exterior lights. All terrain-off runs restore instances=53740, grounds=69240. Terrain on: 24 tiles, same ff4f9ab1 hash.
- Rendering captures are partial-checkpoint evidence: wildlife/SceneTime server clock are not implemented. Final comparisons must follow the completed implementation.
- Isolated +41.6 m Crow Ledge planning trial runs next; current active terrain remains +25.5 m/five flights until a tested visual/physical improvement is proven. No thresholds or waypoints changed.

## Higher Crow Ledge and interior comparison - 2026-10-16 16:15 EDT

- Active built terrain `2749a6c7`: actual Crow Ledge +41.6 m relief, three timber flights; full authority 344 PASS/0 FAIL, runtime terrain/layout PASS, all eight eye captures inspected. P8 now shows more campus/roofline above the trees; centre/east roofs remain partly covered. First-riser warnings and P3 framing remain open.
- `codex_crow_high_verify` complete: all 3 boundary/20 campus/71 hill/5 north stops PASS; all surroundings/boundary/parking/woods reviews PASS; terrain-off counts exact. Reach PASS: 47 entrances, 6 overlooks, 8 eyes, 74 stair and 12 ramp endpoints, 2,563 paths, 1,845 drop seeds, zero traps; 267 unseeded stations retained. Original crc, exterior53/interior6 unchanged.
- Corrected interior comparison fixture: eye and target both use the same building BlockDZ; old Harlow target differed by 36.43 cm. Actual on/off images inspected. Three alternating five-eye pairs PASS within +/-0.5 ms: Harlow -0.05, library -0.10, science -0.02, stadium -0.04, union -0.35 ms. Earlier full-run result retained with mismatch explanation. No room geometry, camera XY/FOV or timing limits changed.
- Next step in progress: South Lot pedestrian link trial 348 PASS/0 FAIL, two new stair flights clear of selected cars/islands. Full authoritative plan regenerating before bake; candidate approach landings and independent physical/bidirectional walking review remain unbuilt under `Saved/CalderWork/codex_southlot_candidates`.

## South Lot access completed - 2026-10-16 16:33 EDT

- Authoritative plan348 PASS/0 FAIL, same ground hash2749a6c7. Two connecting flights and four concrete approaches built at X1800; no car/tree movement. New physical access review PASS: 6,876 floor samples, no holes, all entry/stair/exit rises15.60..16.78cm, treads>=33cm, four actual uphill/downhill legs. Four raw walkthrough captures inspected.
- Final 198-action DisableUnity build PASS (`codex_south_lot_review_build.log`). Terrain/layout PASS: 20k collision points/.03cm, all subchecks, 4,949 floor samples and zero obstructions. New-approach diagnostic coverage includes its real outer curbs; old14 first-riser warnings retained and still need correction.
- Ten current comparison captures saved/inspected in `codex_southlot_runtime_views` (eight eyes plus two lot views). QA contact sheet separate from raw captures.
- `codex_southlot_verify` complete PASS: reach all47 entrances/6overlooks/8eyes/78stair/12ramp endpoints/2,563paths, 1,845drop seeds, no traps; 267unseeded retained. All five surrounding reviews pass; terrain-off restores all original counts exactly.
- Original319-file scope census: 318 unchanged plus the new authorized CalderFeatures approach-landings edit. All13 protected source/map files unchanged. No commits/map saves.

## Stricter first-riser check - correction in progress

- `codex_stair_datum_runtime`, hash `dc3c79a5`: layout PASS4,961 samples, all terrain checks except the new first-riser gate. All3 selected static approach profiles passed; walking did not complete because the terrain failure exited the game.
- New gate sampled117 first edges, five failures: walk00 at both sides12.585/19.998cm, walk29 centre11.511cm, walk47 centre/right18.089/18.523cm. The original internal-riser check passed728 risers15.12..18.00cm. This runtime is a FAIL, not an accepted checkpoint.
- Correction being planned: level concrete approaches on walk00/walk29; walk47 foot datum derived from the lowest real approach surface. Floor queries ignore the concurrent test pawn. An approach's tiny paving lip is distinguished from its first actual stair nosing using the existing5cm riser detection rule; all significant curbs and14..18cm/.05cm acceptance limits remain enforced.
- Canonical baker is now `Tools/calder/bake_landform.py`; the legacy Saved helper cannot retain the three new continuous grade breaks. Those breaks make two Crow landings and the Mercer foot level, without adding physical walls. Last complete plan351/0 and bake185,318 edge samples/none over.5cm. Current corrective edits remain unbuilt/unverified.

## Entrance transition iteration - 2026-10-16 17:38 EDT

- `codex_approach_fix_runtime`: layout PASS4,963 samples. Four of six wider static stair profiles pass; two new entrance profiles hit overhead geometry with their original5m-high ray. Entrance-only profiles now start40cm above the expected walking surface and still read actual world collision/normals. The three original lot/permit-lot profiles retain their original ray and limits. All rise, tread, expected-count and missing-floor criteria are unchanged.
- `codex_approach_fix_terrain`: other first-step fixes now comply (maximum17.997cm), but the long Music landing was invalid: it extended into the building and affected the neighboring walk30 scan. Runtime FAIL retained. Replaced in source with a12cm foot-only strip outside the Music facade, at its real floor minus.6cm, with no head extension. The Plant flight now meets its actual doorway floor, avoiding a6.4cm threshold jump.
- The intermediate Music-only full plan passed351/0; `codex_entry_thresholds_full.log` reruns the complete plan including the later Plant-head correction. Neither entrance correction is built/runtime-verified yet. Separate screenshot sweep needed, because a shared terrain failure can exit before images save.

## Stair/entrance checkpoint PASS - 2026-10-16 17:59 EDT

- Authoritative `codex_threshold_sill_full.log`:351PASS/0FAIL. Canonical bake185,318edge samples, none over.5cm,39stairs/3continuous grade breaks. Latest198-action DisableUnity `codex_nosing_classification_build.log` PASS122.62s.
- `codex_nosing_classification_runtime`, hashdc3c79a5: layout PASS4,967floor samples/zero misses/obstructions. Terrain PASS20,000collision samples, zero holes, max.03cm; all subchecks pass.
- ALL117physical first-riser checks PASS13.982..17.997cm, same14..18cm/.05cm tolerance. All39stairs built;730internal significant rises15.12..18.00cm, going>=30.16cm, top tread>=33.16cm;6ramps<=8.33%. Two legacy10cm-before-foot warnings remain for approach paving/inside-building probes, not true stair risers; the actual first risers are independently checked above.
- All18independent profiles PASS across6flights at3walking widths:21,942floor samples, no holes; Music9/9rises15.42..16.02cm/goings34.3cm, Plant66/66rises16.10..17.91cm/goings>=30cm. All original lot profile points/criteria retained. Music retains750samples per width at denser spacing over its exterior strip/stair/joins; no unloaded-room samples. Four original South Lot uphill/downhill walking legs PASS; four raw captures saved (QA contact separate).
- Floor datums now agree between Python and C++: native FloorTop=-55; Music sill is5cm above that floor,12cm strip.6cm below sill; Plant head meets its actual floor+.6cm. The approach boundary is a nosing only when its physical rise meets the same existing>5cm significant-riser rule. Tiny paving lips are not counted as short stair treads. Riser/tread/going limits unchanged.
- Frozen comparison against `v1de_terrain_off`: same685sample rows/616unique XY, max0.000cm difference. Exterior53, treeCRC0f3b2bb7. The newly appended Music view has moved from inside the building to the outdoor adjoining walk; current14-view sweep in progress. Long routes/reach and updated recovery export still pending for this checkpoint.

## Full current route/reach chain PASS - 2026-10-16 18:13 EDT

- `codex_entry_verify`: all3boundary/20campus/71hill/5north stops PASS (444s), all5surroundings/parking reviews PASS, exact terrain-off counts PASS.
- Reach PASS:47/47entrances,6/6overlooks,8/8eyes,78/78stair endpoints,12/12ramps,2,557/2,557current path stations;1,845drop seeds,zero traps,267unseeded stations retained. ReachReview source and sampling rules unchanged; the path total is derived from current geometry (earlier checkpoint2,563).
- Terrain-off: grounds69,240,instances53,740,campusFrontVertices1,091,586,floorSamples2,654,treeCRC0f3b2bb7,exterior53 exactly restored. All14current comparison images and4access images inspected; raw captures unchanged. Music's outdoor view is partly occluded by the near wall. No photographic-quality acceptance claimed.
- Scope census318unchanged+authorizedFeatures; all13protected source/map files unchanged. Recovery export/check/copy runs next, then current-hash rendered MP. The whole master remains unfinished.

## Current rendered MP remains FAIL - 2026-10-16 18:21 EDT

- `codex_entry_mp`:300.9s, peers bothdc3c79a5, all functional checks and71hill stops pass; overallFAIL one walk correction11.677cm, immediately following a486ms client frame. Four corrections in the whole run include three setup teleports outside the measured walk. No movement/clamp/route/harness changes.
- `codex_entry_mp_gc` running with only verbose garbage-collection logging on both peers. Same rendered test and acceptance; diagnostic, not a waiver. Initial reachability only7..10ms; renderer/lifetime stalls still need attribution. No GC or PSO settings changed.
- Verified stair recovery copy saved at `codex_checkpoint_20261016_1813_stairs`:54test-applied text files,37verified derived binaries,34hash-checked evidence files with raw captures/logs. Earlier gold retained.

## In progress
- Recovery package refresh; higher Crow Ledge and South Lot access now tested. Corrected five-interior timing pairs pass; full final timing remains pending. Existing first-riser warnings, four outlets and road crossing remain next terrain work.
- Final report/package refresh after timings. Rendered multiplayer failure remains open; NullRHI diagnostic passed the unchanged hill route with zero walk corrections.
- P8 roof-view coverage and woods/pig/landscape/water visual finish. The higher ledge and South Lot links are already built/tested; the latest entrance corrections await their runtime verdict.

## Next
- Outstanding roadside drainage rerouting and road-bridge/lane design remain open. The South Lot tier connection is built and walked in both directions.
- Planning survey `Saved/CalderWork/plan/codex_outfall_ground_survey.txt` explains why the four inherited outlets fail: west road and north Calder Drive discharge into rising hills; Ashby Lane and the Plant yard point into building footprints. Reroute drainage deliberately; enlarging the builder's allowed rise/drop or clearance bounds would not fix those routes. Survey is planning evidence only, no new outlet geometry authored.
- Full Phase 2 finish, all Phase 3, final write-up/evidence and original-snapshot patch.

## Runtime compatibility and independent bridge feasibility - 2026-10-16

- The owner explicitly approved the prepared one-call `HouseDetailProps.cpp` compatibility patch. It now preserves the editor's three constructor arguments and omits the editor-only third name in the standalone runtime. The same guard is applied in the four permitted static-mesh builders; no selected materials, geometry or collision change. All five runtime source files compiled and linked in `codex_standalone_compat_development.log`; the later Xcode signing step failed because the generated app bundle carried `com.apple.FinderInfo`.
- Removed only that signing-blocking metadata from `Binaries/Mac/SeniorSendoff.app`, then retried BuildCookRun in `codex_standalone_sign_retry.log`. Cooked runtime and multiplayer acceptance remain pending.
- Original319-file scope census retained:314unchanged+5authorized differences, including the approved house compatibility exception. All13explicit protected source/map files still match takeover exactly (`codex_runtime_compat_protected_audit.json`).
- Actor-cluster diagnostic failed1walk correction103.673cm and was removed from Source; archived only in `Saved/CalderWork/diagnostics`. The ordinary rendered MP failure remains open. Incremental-GC diagnostic pass is not a project/config change or final acceptance.
- Isolated bridge planning is reproducible with `Saved/CalderWork/plan/codex_road_bridge_trial.py`: rounded west lane175.23m, maximum sampled three-width grade9.13%; east lane32.71m, maximum3.81%. A2percent crown feathers into existing paving/flat turnaround. Sampled creek-bed/frozen/building ground changes0.0cm. These are feasibility checks, not baked geometry, a built bridge or route acceptance. Full terrain checks remain pending.
- The authority has a behaviour-preserving `Corridor.target` hook for the trial's crowned cross-sections; all existing corridors return the same profile height. Current C++ ground/baked hash remains dc3c79a5. Keep the verified18:13stairs gold package intact.

## Bridge integration checkpoint - 2026-10-16 20:42 EDT

- Owner-approved HouseDetailProps compatibility guard is applied exactly as reviewed. Native build/link/sign and a fresh cook of6,624packages passed with0errors/3warnings; staging/archive passed. Only the owned, one-asset NeverCook rule excludes the proven orphan groom binding; original character/map hashes remain unchanged.
- Optional cooked-native MP diagnostic crashed in inherited protected AHouseBlackHallDetail::ReplaceOriginalShell before terrain/walking began. It is deferred; zero measured corrections is not a pass. Normal rendered Editor-game MP remains unresolved, with no experimental GC configuration promoted.
- Bridge-builder DisableUnity Editor build PASS122.66s. Against unchanged ground dc3c79a5, `codex_bridge_builder_survey` terrain PASS:20,000samples/0holes/max.03cm, all117first-riser checks PASS13.982..17.997cm, water queries165/165, woodsCRC0f3b2bb7. Fresh39record runtime paving survey exported; no endpoint XY/classification changes.
- New road schema/builders are present, but active baked data still contains0roads. Isolated candidate now has a1075cm deck,35cm concrete arch crown thickness,107cm stone parapets,2approaches and a turnaround. An earlier1050cm candidate failed the newly added200cm standing-clearance check at the creek-bank edge (181.27cm); the raised candidate passes at206.27cm without changing that requirement. Finished paving grades pass<=12%, both640cm road/verge sweeps clear buildings, all78existing stair endpoints remain outside new paving,651creek-bed samples unchanged.
- Full candidate authority review and canonical isolated bake are running. New independent runtime road profiles/real walking and appended bridge captures are authored but not yet compiled/tested. Do not promote or describe the bridge as runtime accepted until those checks pass.
- Previous1813stairs and runtime-compat recovery gold are preserved. Working export requires refresh after integration. Full Phase2/Phase3 and final verification remain unfinished.

## Bridge collision and ordinary walking PASS - 2026-10-16 21:13 EDT

- Active terrain72564759; DisableUnity lot-join build PASS123.78s. Road review passes all8313paving/603deck/651underpass/78parapet samples with0bad, worst paving error.253cm and max measured grade9.424percent. Real player movement completes1256/1256goals across both approaches and bridge both directions, with only the initial setup teleport; clean run153s.
- Geometry repairs address actual failures: roadside clearance recognizes the new paving; tiny asphalt seams overlap2mm; exact endpoint queries choose the road's own terrain sector; physical bridge verges close the abutment seam; bridge foot registration uses its actual paved support; duplicate legacy permit-lot pavement is cut only in the owned recessed join box. No limits/counts/timeouts/waypoints/movement settings changed.
- Full terrain review passed on the immediately preceding clearance binary:20,000samples/0holes/max.03cm,117first risers,165water queries, all rails/gaps/feet/levels/skyline checks,CRC0f3b2bb7. Frozen685rows/616uniqueXY match baseline exactly, max0.000cm.39current stair paving records remain unchanged and their source hashes are freshly locked to the new lot-join binary.
- Broader campus/full routes/reach, original access review, latest captures, final rendered MP/performance and updated recovery package still required. Seven roadside outlet proposals are rejected by clearance/gravity constraints; no outfall acceptance claimed. Full Phase2/Phase3 remains unfinished.

## Full bridge campus regression chain PASS - 2026-10-16 21:55 EDT

`codex_bridge_verify_summary.json` confirms all10parts PASS on72564759: layout8493floor samples/0obstructions; terrain20k/0holes/.03cm/117first risers/301feet/0gaps;99original walking stops; five surroundings reviews; clock/two chimes/12crows;18/18smokes;431/431physical interior stops across18buildings/0problems/six-light pool; exact terrain-off counts;6094/6094reachable path stations/1891drop seeds/0traps,267unseeded retained. No C++ or active asset changed during this chain.

The original access review and16comparison captures are refreshing next, followed by rendered MP on the current hash. The unbound ground-material builder is syntax-checked only and has created no asset. Drainage resolution, full appearance, wildlife/sound, final performance and whole-master acceptance remain incomplete.

## Earlier planning status (superseded by the completed steps above)

### Earlier in progress
- **PLAN research (workflow):**
  - the site inventory (`site.json`) from the C++;
  - a landform prototype in pure Python with every check (grades, frozen zones, FreeRoam legs, the tower sight line, the
    eye-level skyline proof), then an adversarial critique and a revision;
  - the Z-literal classification (`zlits.csv`);
  - the review re-basing (`reviews.md`).
- **Asset search (workflows; nothing downloaded):**
  - CC0 libraries: animals, trees and plants, ground, rock and bark;
  - free Fab listings.

  Output: `Saved/CalderWork/plan/cc0_approval_list.md` and `fab_approval_list.md`.

### Earlier next
- Write `PLAN.md` from the research.
- Then (1a): plumbing at zero relief, with every review identical.

## Bridge access and current visual receipt - 2026-10-16 22:02 EDT

Original access review PASS:18profiles/21,942samples with zero holes and4/4ordinary-input South Lot walking legs. Current16raw captures saved and visually inspected (`codex_bridge_visual_review.json`): bridge/approaches/parapets and stair joins appear continuous; Music view remains partly blocked by the near wall. Bare hills, repeating ground, sheet-like falls and dusk lighting limit appearance acceptance. No photographic-quality claim. Recovery refresh precedes original rendered paired MP at72564759; no new active material or C++ changes.

## Rendered multiplayer bridge checkpoint - 2026-10-16 22:07 EDT

Original unchanged campus_mp.py --walk, no extra flags: 300.4s, both peers72564759, all functional checks and71hill route stops PASS. OverallFAIL:1measured walk correction60.711cm, immediately after a251msclient frame. Four setup corrections outside the measured walk are not included in that count. No movement, thresholds, clamps, waypoints or harness changes. Native diagnostic remains deferred; experimental GC flags remain disabled. Next prepare the owned ground material while preserving this unresolved result.

## Ground material compiled; runtime binding in progress - 2026-10-16 22:10 EDT

Owned M_CalderLandscapeGround compiled/saved with367pixel instructions and12actual texture samples (9explicit graph reads); original M_CalderGround freshly measures379instructions/15samples. All3original master SHAs unchanged. Nine approved CC0 texture reads support grass/meadow/litter/clay and triplanar rock. Script/report: Tools/calder/ground_assets_batch.py, phase2/ground_material.json. Runtime binding/masks and protected lawn-overlay handoff authored; DisableUnity build in progress, no runtime/visual acceptance yet. TerrainTiles collision arrays/heights unchanged. NoCalderLandscape selects original material/neutral colours/legacy lawn patches. All13protected source/map hashes unchanged. Latest recovery gold codex_checkpoint_bridge_full_20261016_2207 contains71test-applied text files,38derived binaries,47hashed evidence files; it predates this material/binding work and retains MPFAIL.

## Continuous ground runtime checkpoint PASS - 2026-10-16 22:16 EDT

DisableUnity build123.62s. Landscape on/off eachPASS original terrain20k/0holes/.03cm and layout8493floor samples/0misses/0obstructions, same72564759/CRC0f3b2bb7. All685frozen CSV rows exactly match original terrain-off in both modes. On166,437painted vertices; off0neutral vertices; material choice verified in logs. Original terrain-off baseline counts exactly restored.16on images individually inspected;16off composition sheet plus P2/bridge approach full-res inspected. Checkerboard replaced on nonprotected lawns; hills/trees/falls/groundcover remain unfinished. Report codex_ground_checkpoint_evidence.json.367instructions/12actualsamples in owned material; original masters unchanged. Census307unchanged+12authorizeddifferences; all13explicit protected/map files unchanged. Whole-master MPFAIL remains1correction on the previous bridge binary, not retested after this appearance change.

## Ground cover authored, first build in progress

HouseCalderCover.cpp/.h adds4plain ISM batches using own seed2026101701 and a4mreject grid; <=76,800candidates before rejection, alltufts<=60triangles,25..45mculling, no collision/shadows/DF/indirect. Reject frozen rectangles, building pads, reserves, paving, stairs/ramps, named paths, ditches, water/decks, roadside solids, steep ground and discontinuous roots. Existing grass-blade master reused; only owned MIDs register through the new public wind hook, existing RNG/audio unchanged. New opt-in CalderLandscapeReview independently enumerates actual instances, render geometry/settings, protected bounding footprints and world-traced root contact. This is explicitly a ground-cover checkpoint review, not complete landscape acceptance. Added new review to all3existing self-exit sharing lists. Build/runtime verdict pending. Need strengthen shoreline footprint rejection and add close views after the first actual placement survey.

## First cover survey and correction in progress

First DisableUnity build124.06sPASS. codex_cover_first_survey:45,143instances/4plainISM, max46tufttriangles,0badsettings/0frozenfootprints; independent world root traces FAIL337placements (191campuspolish spread solids,119campuspaving,8Fallsolids,15tree trunks,4othernear/polishsolids). Terrain20k and layout8493PASS, CRC0f3b2bb7. Failed placements are retained in the native CSV. Build cost106.8ms. Cause: placement ran before the real later paving/props/tree collision existed. Source now places after all exterior/campus polish construction and then registers the owned wind MIDs. Full110cm-expanded shoreline/deck probes and an independent actual-render-bounds water check added. Four1.3meye macros appended from actual valid survey plant positions; existing cameras unchanged. Second DisableUnity build running; not accepted yet. No thresholds, sample counts or old review paths changed.

## Cover asynchronous-collision diagnosis

Second DisableUnity123.60sPASS. codex_cover_fixed_review_views:44,740instances/4ISM,46maxtriangles,0badsettings/0frozenfootprints/0waterfootprints; terrain/layoutPASS andsameCRC. StillFAIL193worldroottraces, allCampusPolish async collision (191spread+2solid), worst99.85cm. Moving placement afterconstruction cleared allotherpaving/tree/prop failures; remaining issue is asyncphysicsbodyreadiness. New ScheduleBuild waits nonblocking for existing campus-polish collision bodies and terrain CollisionReady, then scatters/registers wind once. No old collision is modified. Landscape review now waits for actual cover build (90sreadiness cap) and validates zero instances on a dedicated server. Macro eyes now explicitly GroundHeight+130cm instead of assuming front-campusGrade=-60 in the northfieldGrade=-362. Current build session62810; next run coverage/terrain/layout and20views. No acceptance yet; failed193CSVretained.

## Cover placement PASS; appearance revision in progress - 2026-10-16 22:39 EDT

Async-ready build123.39sPASS. codex_cover_ready_review_views:44,547actual instances/4plainISM,46maxtriangles,0badsettings/0badrootplacements/0frozenfootprints/0waterfootprints, worstrootgap.182cm. Terrain20k andlayout8493PASS, same72564759/CRC.20captures saved; all4macros individually inspected atnative-logged130.00cmeye height. VisualfinishFAIL: sparse straight stems and green field ground do not yet meet the target. No photographic-quality acceptance. NoCalderLandscape native reviewPASS0batches/0instances, original material/neutralpaint, terrain/layoutPASS. Callback waited13.96swall time tofirstgametick(frame1), then actualscatter180.2ms; this includes existing remainingmapstartup and is nonblocking. Whole-load comparison remains required.

Revisedownedground material now377instructions/12actualsamples (field masks on north/east/west zones), originals3SHAunchanged, v2asset6dc14c7aa1e6160bc7b70704d3340ddd719df64bc373bcc05b1fae26566bce85. Tuft revision: fanned bent blades, stem-attached panicles, smaller cupped asters with leaves, and4candidates/4mbucket (<=102,400before rejection); extraindependentstream preserves old3candidatepositions. Alltufts<=60triangles byconstruction, untested newbuildsession58044. Next native review/20captures and NoLandscape/terrain-off comparisons; then recovery/checkpoint. Old visual/placement receipts and earliergroundgold preserved.

## Cover visual revision2: placementPASS, seed silhouette rejected

Build123.43sPASS. Nativecodex_cover_visual_v2_reviewPASS59,347instances(15,522lawn/41,276broomsedge/1,244aster/1,305fluff),4ISM,max58triangles,0settings/placement/frozen/water failures,worstrootgap.182cm. Terrain/layoutPASSsameground/CRC.20captures saved; P7 andmown/broomsedge macros individually inspected. Fieldtone andcurvedclumps improved, but opaque seedheads read as flatdiamonds, so visualacceptance rejected. Newrevision uses existingM_NatureFoliage, code-painted irregularseedawns plusnormalmap oncrossedcards, matchingtheexistingalpha-preserving/coverage-scaledmip chain. A firstcompileusingthegeneralHallMakeTexturehelperwasstopped(SIGINT,exit6) beforetesting because thathelper discardsalpha; nativefoliageMaskTexturepatterncopiedintoownedCoverhelper, originalnaturecodeunchanged. Currentbuildsession15979 codex_cover_seed_alpha_build.log; nextsame20captures/nativechecks. Original3materialmastersunchanged, ownedgroundv2stays377instructions/12samples.

## Initial ground-cover checkpoint PASS - 2026-10-16 22:53 EDT

Buildcodex_cover_seed_alpha_build122.77sPASS. Nativeon59,347instances/4plainISM,54/56/58/40triangles,0badsettings/placements/frozen/waterfootprints,worstrootgap.182cm; actualbuild256.5ms. Terrain20k/0holes/.03cm andlayout8493/0misses/0obstructionsPASS, ground72564759/CRC0f3b2bb7. NoLandscapePASS0batches/0instances/originalmaterial/neutralpaint. Bothmode685frozenCSVrows exactlymatch original terrainoff. Freshterrainoffbaseline exactgrounds69240/instances53740/campusFrontVertices1091586/floorSamples2654/lights53/CRC. All13protectedsource/maphashesunchanged.20views/contactsheet and4actual130cmeye macros inspected; maskedseedheads removeopaque diamonds. Stillinitialappearance only: fields/grassclose detail, forested/altitude ridges, rock/farm/mystery/water/woods/pig andallwildlife/sound/finalperformance unfinished. Receiptcodex_cover_checkpoint_evidence.json. Recoveryrefresh/goldnext, thenoriginalrenderedMPoncurrentappearance(noheavyCPUalongside). PreviousbridgeMPFAIL1correction preserved.

## Current-cover multiplayerFAIL and deterministic placement correction - 2026-10-16 23:01 EDT

Originalunchangedcampus_mp.py codex_cover_mp --walk completed297.9s. Bothterrain72564759, allfunctionalchecks/71waypointsPASS, overallFAIL3walkcorrections23.538/17.161/17.366cm after177/129msclientframes. SeparatecoverdeterminismFAIL: host59,347plants/client59,346. Currentverifiedgold codex_checkpoint_cover_20261016_2253=80test-appliedtext/39derivedbinary/47hashed evidence; scopedinitialcoverproof, notfinalappearance/MPacceptance.

Static-scene correction nowbuilding: nativeplacement traces onlyECC_WorldStatic so transientpawns/movable replicatedprops cannotinfluencefieldgeneration; canonicalhardscape/frozen/water/grade checksremain. AseparatequantizedtransformCRC isloggedforactualpeercomparison. Originalfoot/reviewlimits andmovement/harness unchanged. Diagnostic-only -CalderLoadCleanupProbe (OFFbydefault) requests the normalGEngine->ForceGarbageCollection(true) afterinitialproceduralsceneryfinishes, atthenextsafeengineframe; regularGCremainsenabled, noexperimentalGCsettings. Hypothesis fromearlierGCverbose trace: firstcollection55.58mswith23,234constructionobjectsgarbage; later25/14.5/7.9ms. Probeisnotpromotedoracceptance. Next nativeNoProbeplacement/terrain/layout, thenpairedProbe+verboseGCruntodiagnose; ifuseful, furtherdefault/houseruntimevalidationrequired. Activebuildsession16138 codex_cover_static_scene_build.log.

## Static query/cleanup diagnostic both FAIL; exact placement dump building

Static-source build124.68sPASS; nativeNoProbe reviewPASS59,347instances/placementCRCdf8c5a87,0badplacements/frozen/water,terrain20k/layout8493PASS andno cleanup probe log. Pairedcodex_cover_cleanup_probe_mp completed298.9s: originalfunctional/71stops/terrainhashPASS; overallFAIL2walkcorrections26.776/26.771cm after63.59msGC reachability/183msclientframe. InitialstandardfullGC request(OFFbydefault) ran10.76ms, didnotfixperiodicstall; doNOTpromoteasfix. CoverstillFAILhost59,347/df8c5a87vsclient59,346/e9cc032f evenwithECC_WorldStatictrace. Supplementcodex_cover_cleanup_probe_supplement.json.

Nextdiagnostic -CalderCoverDump writesacceptedtransforms andactualnative-rejectedpoint/hitcomponent/class/mobility/objecttype, plusworld/actorBegunPlaymetadata. Itdoesnotchangeplacementlogic. Sourcecandidateapplied, currentDisableUnitybuildsession14371 codex_cover_dump_build.log. NextshortpairedrunWITHOUT--walk andWITH-CalderCoverDump(no cleanup probe), compareexactXY pointdifference/hitactors, thenfixobservedcause. Existing counts mismatch is stable across tworuns. Clientcallbackran257msafterexteriorbuild,host17.46slater; fullworldreadinessmaymatter, butnotyetproved. Workingrecoveryexportstale. Fullmasterunfinished.

## Exact one-plant mismatch identified; stadium exclusion building

Dumpbuild124.15sPASS. Capture-only codex_cover_placement_dump (no--walk, no cleanup) saved both CSV sets; stopped both peers after capture, notfunctionalacceptance. Host59,347/client59,346; all59,346sharedtransforms EXACT toCSVprecision, no client-onlyplants. Onlyhostplant=(4715.227746,-9426.150860),ground-60. ClientrejectednativehitExteriorNear_40,HouseExteriorDetail_0,WorldStatic/Static,hitZ=-60: coplanarstadiumapron/groundcollisiontie gave differentfirsthitnames. ThisisnotanRNGproblem. Additionalnative metadata: clientworld/ownerBegunPlay0/0;host1/1.

Currentbuildsession74386 codex_cover_stadium_ready_build.log: explicitstadium+5mapronellipse(6100x4300semiaxes, expanded110cmtuft radius), matchingexistingHouseCampusPolishGround lawnexclusion; no stadiumgeometry/collision changes. AlsoawaitactualWorld/OwnerBegunPlay aswellasexistingcollisionreadiness. Allold thresholds/paving/footchecksremain. Nextdefaultnativeplacement/terrain/layout thenpairedhash/dumpandfullwalk; cleanupProbe remainsdiagnosticOFFbydefault, previouslyFAILED beforefullreadiness. Workingexportstale, fullmasterunfinished.


## Stable cover checkpoint and multiplayer diagnostics - 2026-10-16 23:46 EDT

The default stadium/readiness build passed `codex_cover_stadium_ready_review`: 57,815 actual plants, four ISMs, placement CRC `29aceab6`, zero bad settings/root placements/frozen/water overlaps, worst root gap 0.182 cm. Terrain 20,000 samples and layout 8,493 samples passed, terrain `72564759` and original tree CRC `0f3b2bb7`. World and owner BeginPlay now precede scatter. The explicit stadium apron exclusion fixes the old coplanar collision tie without changing stadium geometry or any existing transform.

Paired `codex_cover_ready_cleanup_mp` proved all 57,815 actual transforms identical, but failed the unchanged movement gate with three walk corrections (35.761, 51.189, 51.194 cm). The optional normal-GC cleanup probe remains OFF by default and is rejected as a fix. CLI-only single-thread GC diagnostic `codex_cover_serial_gc_mp` also failed: one 26.321 cm walk correction in 302.8 s, both peers 57,815 plants/CRC29aceab6. No project GC configuration changed. Full functional/71-waypoint checks passed in both completed walks, but do not waive the movement gate.

The executable-only `UnrealEditor-Cmd` diagnostic `codex_cover_editor_cmd_mp` failed in 154.7 s: host SIGBUS in DerivedDataCache / HairStrands streaming cleanup during lobby travel, before terrain/walking. The client was terminated after the host crash and connection timeout; its exit0 is not acceptance. Adapter records the one-line executable substitution; the original harness SHA remains404b4e7727205d7f01a7b8dd5f1f9205ef12a4ee7db6618eb32626e5642f3e79. Do not repeat this diagnostic as a fix. Optional older cooked-native check remains blocked by protected BlackHall CPU mesh access; no room source changed.

No engine/build running. Refresh and verify a recovery checkpoint before more appearance changes. Next independent work: conventional reduced derivatives of acquired tree donors, with source SHA guards and an unbound preview before any woods binding. Original tree positions/collision/CRC, terrain/trail/hut and protected house/court/party/parking remain fixed. Ground/cover are partial visual checkpoints; the full master, rendered multiplayer acceptance, remaining Phase2/3 and final performance are unfinished. No approval pending.


## Unbound tree candidate preparation - 2026-10-16 23:49 EDT

Verified deterministic-cover recovery gold preserved at `Saved/CalderWork/codex_checkpoint_cover_deterministic_20261016_2346`:81test-appliedtext/39derivedbinaries/63hashed evidence. Working source now removes the rejected cleanup-GC probe; next build must verify it. Added opt-in development-only `CalderTreePreview`, registered in the three self-exiting lists. It renders owned unbound candidates at a common16m height and three forced LODs plus close detail, with three-second texture/mesh settling. No default forest bindings/transforms/collision change.

`tree_assets_batch.py` creates only owned conventional derivatives from the owner's acquired beech/maple packs, guarding every source-pack asset SHA. First attempt failed before creating assets because StaticMeshFactoryNew is not exposed; replaced creation with owned asset duplication and source-LOD copies. V2 currently running. Actual beech render triangles15,132/6,053/1,968; Nanite disabled, collision removed, final report/source-hash checks pending until both trees complete. These are candidates, not native-species/final appearance acceptance. Next build/preview, then decide from observed shapes and material/LOD quality. No source pack/map/shared ExteriorTree writes.


## Unbound forest-form preview rejected; broad field forms preparing

DisableUnity build203actions PASS127.19s, including removal of the rejected cleanup-GC probe. Default scatter still57,815/29aceab6, originalCRC0f3b2bb7 and WoodsPass/CampusWoods PASS. Read-only material inspection compiled10materials, maximum9actualsamples/477pixel instructions, all476acquired source-pack hashes unchanged. Native `codex_tree_unbound_preview` saved8raw frames (three forcedLODs and close detail for each tree); all8individually inspected. Harness/capture validityPASS, appearance NOT accepted: crowns too thin/narrow/green for the mature October canopy. No default woods binding. Exact rejected derivatives/reports/builder preserved in `Saved/CalderWork/tree_candidate_trials/forest`.

Next owned candidates use already-acquired Field_01 forms, sourceLOD5reduced55/20/6percent for beech and LOD4/5/5 with100/50/12percent for maple, aiming at broader crown geometry with bounded render cost. Batch `codex_tree_field_candidates_batch.log` currently running; same original-pack SHA guards and no map writes. The20k-triangle candidate budget is an internal initial screening target, not a user/review limit. Final materials/fit/wind/native appearance/performance still unaccepted.


## Field-form structural LOD failure - 2026-10-17 00:01 EDT

Field candidate batchPASS:Beech17,683/6,429/1,929;Maple18,459/6,521/1,566, Nanite/collisionoff, original476packSHAsunchanged. Native `codex_tree_field_preview` saved8raw frames, all8individually inspected. Crowns improved, but reduction removes structural wood: lowest LOD canopies detach, some near geometry has broken thin branches/stem joins. This is a visual failure, despite capturevalidityPASS. Do not bind the candidates.

Current editorbatch builds owned CPU-readable sourceLOD3 structural donors, stillunbound. New UNBUILT runtime helper merges only retained leaf sections with full structural sections at all threeLODs, preserving material slots, normals/tangents, source colours and allUVchannels. Preview-only opt-in; no gameplay forest/physics changes. Next DisableUnitybuild afterbatchexits, same8frames with -CalderTreeRetainStructure, inspect actualwoodgeometry. Recoveryworkingstale; stable2346gold remains intact. FullPhase2/3 and renderedMP/finalacceptance unfinished.


## Retained-structure preview: distant attachment improved, not promotable

DisableUnity204actionsPASS126.95s. Nativecodex_tree_retained_structure_preview captured8frames; all8individuallyinspected. Structural preservation restores distant trunks, but far cost too high and front bark still has spotting/gaps. Beechactual40,481/33,355/29,948tris (28,019retainedwood);Maple43,132/38,183/34,388(32,822retainedwood), build61.5/70.4ms. ActualsourceUVchannels8, allcopied; earlier four-channel wording was an unverified assumption. Crowns alone are not acceptance. Current helper remains opt-in preview-only; originalgame forest/physics unchanged. Need main-trunk-specific retention/reduced secondary branches, independent bark/material diagnosis, proper autumn colour/fit/wind, then native/performance checks.

Nowprofilingoriginal renderedMPvia client-only trace, codex_cover_client_timing_mp, session77518. No heavyparallelwork. The launch-only adapter adds CPU/frame/bookmark tracing and named events only toclient; original harness/criteria unchanged. Afterjobfinish export timing with UnrealInsights -NoUI -AutoQuit and diagnose actual stallscopes. Existing source/tree assets remain unpromoted; default terrain72564759/cover57,815/29aceab6/CRC0f3b2bb7 unchanged in latestpreview. Recoveryworkingstale;2346stablegold preserved.


## Profiling port obstruction; user editor reopened - 2026-10-17 00:13 EDT

Profileattemptcodex_cover_client_timing_mp FAIL140.4s beforeclientlaunch: hostbound7787ratherthancontract7786. lsofidentifiedoldCrashReportClient18973holding7786, withinheritedstdoutfromourcodex_cover_editor_cmd_mpcrash. Closedonlythatownedreporterandownedhost24200; originalharness/portunchanged. No timingtracewascreated. SeparateneweditorPID24582 isactivelyplayingUEDPIE_0_Chapter01_House at>300percentCPU; leaveitanduserworkalone. Asyncreadinessquestionpending: usertosave/closeUnrealforaccurateMP/assetbatch,orcontinuesourceonly. No task-ownedengine/build/batch currentlyrunning.

Source-onlypreparation: ownTreeMesheshelpernowretainsmain-trunksectionsfromdetaileddonorwhileeachLODkeepsitsreducedleaves/secondarybranches, addressingunacceptablefar28..32kstructuralcost. FullprecisionUV/highprecisiontangentoverridesaddedtosafelycopydonorwind/pivotcoordinates; possiblebarkquantizationcauseUNPROVED. ThisrevisionisUNBUILT/untested, opt-inpreviewonly, nodefaultforestbinding. Oldhelperpreservedunder tree_candidate_trials/retained_all_wood. NeedDisableUnitybuild/8samepreviewframes/nativeactualcounts/barkcheckafterreadiness, thenautumnpalette/physicalfit/wind/performance.


## Recovery saved while waiting for editor readiness

`codex_checkpoint_tree_wip_20261017_0018`:90textfiles actuallytest-applied/43derivedbinaries exactSHA/40hash-verifiedevidencefiles, including24rawcandidateframes andthree native logs. Thispreserves anUNBUILT main-trunk/fullprecision/source-donor-preview revision, notpassedappearance. Stable2346covergold preserved. Current13protected/mapfilehashes, original3materialmasterSHAs andoriginalMP-harnessSHAallunchanged. UsereditorPlayremainsuntouched; waitforactualeditorclosedreadinessbeforenative/MP/assetjobs.


## Testing resumed after user closed Unreal - 2026-10-17 03:52 EDT

User confirmed save/close readiness. Process/port check found no editor/game/build/crash reporter; UDP7786/7787free. Read-onlyseason/textureinspectionPASS, originalMF/MPCandall476tree-packSHAunchanged. Exportedsourcegraphs andfour8192pxleaftexturesforinspection; resizedinspectioncopiesonly, sourceassetsunchanged. Initialfull-sizeimageviewsexceededtoolIPCbudget; nativepreviewremainsacceptanceauthority.

Preparedtrunk/fullprecisionrevision DisableUnity204actionsPASS144.95s. Nativecodex_tree_trunk_precision_previewnowrunningoffscreen/muted. Itcombinesreducedleaves/secondarybrancheswithretainedmain-trunksections, no defaultforest/collision/placementchanges. Exactnativecounts/bark/LODappearancepending. Nextinspectresults, usepreparedoriginal-donorpreviewifnecessarytoisolateasset-vs-copyfault, thenclient-onlytimingcapturewithoriginalMPcriteria. NoheavyworkduringMP.


## Trunk precision preview and live MP trace - 2026-10-17 03:58 EDT

All8codex_tree_trunk_precision_previewframesindividuallyinspected (32totalcandidateframes). Fullprecisioncopycleansnearrootbark. ActualBeech19,992/8,827/4,404tris,Maple20,332/9,090/4,186;20.6/20.1msbuild. LowestLODstilllosesupperbolebecauseTileablesectionscontainit; retained_Trunk_sectiononlycoverslowerstem. Thisisnotacceptedforlivewoods. Nophysics/defaultforest/transformedits. Needretainupperstem/majorbrancheswithboundedwoodcost.

OriginalrenderedMPnowprofilingclientonly:codex_cover_client_timing_mp_v2,session95365. Bothpeersconnected/terrain72564759matches;client.utracerecording. Noheavyparallelwork. Exactoriginalharness,port,flags,waypoints,movement,thresholdsretained. AfterrunexportGameThreadCPUeventswithUnrealInsights-NoUI/-AutoQuitanddiagnoseactualstallscopes. NoexperimentalGC/projectconfigurationchanges.


### Codex 2026-10-17 04:04 EDT - client trace captured, correction remains

Rendered `codex_cover_client_timing_mp_v2` completed299.9s with both gameplay PASS markers and 71-waypoint walk. Terrain72564759 matches. One correction during walk89.629cm; four whole-run including startup resets. Client hitch308ms at07:57:49UTC coincides with GC98.664ms reachability; trace export pending. No criteria or defaultGC changes. Both game processes exited.

Tree helper has an UNBUILT preview-only experiment retaining all nonleaf wood at farLODs with4/8cm spatial clustering and degenerate removal, preserving original packed UV/pivot data. Prior compiled precisionhelper archived. Need actual build/native geometry counts and eight inspected frames; no defaultwoods/collision change.


### Codex 2026-10-17 04:07 EDT - measured hitch diagnosis and new tree build

Insights export passed: 14,702,727 GameThread events from the client trace. At trace88.2448s, one333.025ms frame (next-frame game marker308ms) spent299.642ms in ConditionalCollectGarbage:95.446ms PyUtil::CollectGarbage,98.648ms actual Unreal reachability,96.902ms GatherUnreachableObjects. Receipt timing_diagnosis.json and long_game_thread_events.json live beside the trace. The original criteria are unchanged. A standard rendered harness comparison with -DisablePython is running; both peers confirm the supported flag disabled Python. No project GC or plugin configuration change.

Spatially clustered all-wood preview revision: DisableUnity204 actions PASS127.19s, codex_tree_clustered_wood_build.log. Native eight-view geometry/appearance check pending until multiplayer exits. No default forest binding.


### Codex 2026-10-17 04:18 EDT - local autumn shader built; revised tree preview running

`codex_cover_python_disabled_mp` finished302.9s: both gameplay/71-waypoint route checks pass, terrain72564759 agrees; one5.666cm walking correction after140ms hitch. Four whole-run corrections include startup resets. Supported Python-disable flag reduced hitch/correction but did not pass the unchanged zero-correction gate. No project GC/plugin settings changed.

All eight `codex_tree_clustered_wood_preview` images individually inspected (40total candidate frames so far). Upperbole attachment is repaired, but LODs were nonmonotonic (Beech19,992/28,130/16,075;Maple20,332/24,972/15,630) and snapping the lowertrunk reintroduced bark artifacts. Trial rejected; helper archived under tree_candidate_trials/clustered_all_wood.

Revised helper preserves the detailed lowertrunk at everyLOD and clusters only upperbole/secondarywood on2/12/24cm sourcegrids. It now rejects nonmonotonic actualLOD triangle counts. Explicit preview-only grid arguments permit calibration without changing any gameplay/review threshold. DisableUnity204actionsPASS126.78s.

Read-only `inspect_tree_leaf_graph.py` passed49graphs, all source hashes unchanged. Verified packedMask Rcoverage/Groughness/Btranslucency. New ownedlocal autumnshader batch passed: two instances243pixelinstructions/3samples, original476packassets and sharedNatureFoliageSHA unchanged. Only four owned, unbound previewmesh material assignments changed. No live forest binding. `codex_tree_fixed_base_autumn_preview` is running for native counts/eight raw views.


### Codex 2026-10-17 04:25 EDT - connected autumn tree candidate and isolated bark fix

All eight codex_tree_fixed_base_autumn_preview frames inspected (48total at that point). Geometry is monotonic: Beech40,193/16,256/8,653;Maple34,283/17,728/9,561. The upperbole is connected, but bark patches persisted despite preserving the lowertrunk, so the earlier precision-only diagnosis was incomplete. Engine source confirms vertexcolour conversion already matches runtime BuildFromMeshDescriptions (ToFColor(true)); no speculative colour-data edit was made.

Eight owned local opaque bark instances created from the read-only NaturePropTextured parent and acquired maps. Actual Metal229pixel instructions/2samples each. Original476pack assets and shared parent hash remain identical. All eight codex_tree_local_bark_preview raw frames inspected (56total); bark patches are gone and upperbole remains attached, isolating inherited material behavior. Same geometry, so the comparison changes only bark materials. WoodsPass/CampusWoods pass, treeCRC0f3b2bb7, lights53, cover57,815/29aceab6.

Candidate remains unbound to the live woods. No originaltree transform, collision, map or shared exterior mesh changes. Forest fit/collision retention, live common-wind registration, altitude/species palette, lower distant cost and full native photographic/performance checks remain. Current compiled build204DisableUnity actionsPASS126.78s. Recovery export/verification being refreshed now; old gold retained.


### Codex 2026-10-17 04:35 EDT - wood-only static reduction built as owned assets

Immutable candidate recovery codex_checkpoint_tree_materials_20261017_0428 verified97text/54binaries/21evidence, actualGitApply and exact hashes. This is the last compiled/visually inspected gold before the following work.

Read-only export_tree_wood.py exports non-leaf CPU-buffer sections from owned structural donors. Background Blender5.2.2 builds normal/UV0-preserving FBXs in phase2/tree_wood. import_tree_wood.py imports only Calder/Trees and applies independent wood-only reduction: Beech28,019/4,202/421;Maple32,822/4,923/493. Maximum bounds error0.000061cm confirms axes/scale; Nanite/collision off; all476original source hashes pass. Texture UVs and attachment need native visual checks. Initial Python class alias/wrapper-setting mismatches were corrected against local engine headers; completed importer uses native reduction settings for silhouette importance and the wrapper only for percentages/screens.

Preview helper now supports pre-reduced threeLOD wood, preserving the existing clustered fallback. New mode requires explicit -CalderTreeReducedWood. Current editor build running; no native acceptance yet. No live forest/map/collision/source-pack change. Export payload now includes the two owned FBXs.


### Codex 2026-10-17 04:45 EDT - far reduction trial rejected; additive distance views

`codex_tree_reduced_wood_preview` produced eight inspected frames (64total). Combinedcounts Beech40,481/9,538/2,350, Maple43,132/10,284/2,059; runtimebuild30.1/37.0ms. Near/middle attachment and texture are sound; far1.5percentwood collapsed major maple branches into jagged wedges. Numerical geometry savings are not visual acceptance; that far result is rejected.

New6percent trial imports Beechwood28,019/4,202/1,682;Maple32,822/4,923/1,969. Own import count cap changed with the different quality configuration; original native gameplay/performance/review limits remain unchanged. This is not a passed final budget. Expected combinedfar3,611/3,535 still savesabout58/63percent versus the clusteredcandidate. Maximum imported bounds error0.000061cm; sourcepack hashes pass.

Added opt-in CalderTreeDistanceViews preserves all eight original diagnostic shots and appends two150m captures. No old ExViews/waypoint/count/threshold changed. Build204DisableUnity actionsPASS126.23s. Native ten-view codex_tree_reduced_wood_distance_preview running. No live forest/collision/instance-transform change.


### Codex 2026-10-17 04:49 EDT - ten native distance views inspected

Build204DisableUnity actionsPASS126.23s. All ten codex_tree_reduced_wood_distance_preview frames individually inspected (74total). CombinedBeech40,481/9,538/3,611;Maple43,132/10,284/3,535. Actual camera distance15018.6cm/eye170cm above stagedroot/forcedLOD3 logged for the two newdistance views. Original eight diagnostic shots unchanged. Both distant silhouettes retain attached readable trunks/crowns; closeforcedfar geometry remains coarse and is not final photographic acceptance. NativeWoodsPass/CampusWoods pass; originaltreeCRC0f3b2bb7, exteriorlights53, cover57,815/29aceab6 unchanged.

Current assets/source remain preview-only. Candidate recovery is refreshed before forest fit/collision/wind/performance integration. No protected/map/original-tree/shared-exterior-mesh change.


### Codex 2026-10-17 04:56 EDT - explicit maple forest-fit trial prepared, unbuilt

Gold codex_checkpoint_tree_wood_20261017_0450 passed102text/58binaries/18evidence and protected13/13 hash check. Subsequent source now prepares opt-in -CalderWoodsTreeTrial on the two existing mapleHISMs only. Default world unchanged. FitToOriginal morphs only rendergeometry into the original envelope, retaining normal/tangent frames via the Jacobian and duplicating the existing exact trunk body. No instance transform update. Local leafMIDs join existing shared wind keys. WoodsStructure diagnostics now report actual render-data triangles rather than generatorinput counts.

New -CalderTreeFitReview independently checks two batches/65instances/780actual trunk-face rays, body/cull/settings and sharedwind scalar variation, then emits actualposes.csv for exact baseline/trial comparison. It is additive in shared self-exit lists; no existing review threshold/count changed. Build currently running; physical, pose, visual and performance acceptance pending.


### Codex 2026-10-17 05:07 EDT - baseline physical check passes; first fitted trial rejected

206-action DisableUnity build passed125.41s. Native codex_tree_fit_original PASS:2original batches/65instances/780actual trunk-face contacts/0failures, WoodsPass/CampusWoods PASS. Actualposes.csv (file is named poses.csv under CalderTreeFitReview) retained for exact trial comparison.

First codex_tree_fit_trial falls back to original meshes because FitToOriginal rejects both candidates. Native review correctly FAILS replacements0/expected2 and missing candidatewind, while all780original contacts still pass. No transform or originalbody edits. Failed trial is preserved; do not claim forest-fit success.

Added rejection-stage/envelope diagnostics only, with every existing guard unchanged. Diagnostics build running. Suspected whole-section stem scale expanding lowercrown is a hypothesis until actual bounds log confirms it. Proposed monotone radial repair noted in Saved only, not applied. Default game remains unmodified by the explicit trial flag.


### Codex 2026-10-17 05:16 EDT - radial envelope passes; runtime material setter corrected

Diagnostic run confirmed envelope rejection: maple0 original maxabs634/500 versus candidate699/653, maple1 original591/460 versus candidate699/653. The revised radial map preserves the stem band and integrates a positive radial derivative, with the full Jacobian for normals/tangents and constrained outercrown. All original .1cm bounds/physical criteria remain.206-action buildPASS126.37s.

Native codex_tree_fit_radial reached a successful first fit: originalmaple0 targetMin(-425.817,-500.069,-9.217)/targetMax(634.437,421.885,1650.986); actualMin(-487.898,-457.010,-9.217)/actualMax(494.270,476.282,1650.986), preserving maxabsolute envelope/height. It then asserted when attaching uninitialized renderdata. Local engine source identified UStaticMesh::SetMaterial as the editor transaction/rebuild path releasing resources. Replaced it with direct assignment to the owned transient material slot, avoiding editor APIs and preserving initialized renderdata. No source asset/shared material modification. Native fit/contact/wind comparison is still unverified. Task-owned CrashReportClient46447 was identified by its exact stdout path; cleanup only targets that process.


### Codex 2026-10-17 05:23 EDT - maple physical/pose/wind trial PASS

Runtime material fix206-action DisableUnity buildPASS121.05s. Native codex_tree_fit_runtime_material PASS:2replacement batches/65instances/780actual trunk-face contacts/zero failures; original60x60x400box centeredZ200, simple-as-complex, oldculls/settings retained. Sharedwind registered14MIDs (old12+new2); candidate scalar range2.238571 proves livegust updates. WoodsPass/CampusWoods PASS.

compare_tree_fit.py independently requires both nativePASS results, both woods audits and CRC0f3b2bb7, then compares actual65pose CSVs byte-for-byte. PASS: bothSHA b011f36278dea26d5de634d30734b64dcfa934a4d8c8ef43b60f71053865d33c. Maximum contacterror0.000004cm in both original/trial. Report at trial/fit_comparison.json and phase2/tree_fit_checkpoint.json. No instance transforms or originalcollision changes. Default remains unchanged; trial is explicit only.

Capturing four existing in-game woods views, original first then trial, before visual or performance promotion. No oldview/NoGo/waypoint/threshold changes.


### Codex 2026-10-17 05:36 EDT - forest colour refinement and prepared A/B timing

All four original and four initialtrial in-game views inspected. Trial adds dense crowns/branch detail without changed trail/hut/poses, but palette too muted. Read-only acquired MF_GenerateBranchMask uses saturate(B^5*25000); replaced the initial direct-translucency colour weight with that verified selector and added owned gain6. Actualshader245instructions/3samples; original476pack and sharedparent hashes pass. No geometry/physics change. All four codex_tree_woods_autumn_views inspected; warmer/readable detail.86total tree/woods diagnostic frames inspected. Default remains original maples; broader photo quality is not accepted.

Prepared perf_tree_trial.py: all11canonical exterior views plus4woods, three alternating pairs,300requested samples/4secondwarmup/uncapped MetalOffscreenOnly; original scripts unchanged. Tree-only diagnostic, not final pinned-clock/interior acceptance. No asset/editor/build/heavy job alongside it. Runtime fern/stump material usage warnings were observed and remain open for a later owned-material correction.


## Codex 2026-10-17 05:59 EDT - three-pair maple diagnostic complete

All six sequential offscreen/MetalOffscreenOnly processes completed all15views. Original11views retained, plus4woods,300samples/4secondwarmup/uncapped, unchanged launchers. Median on/off deltas: woods edge+1.22ms,20m+1.89,deep+1.24,silhouette+1.84; all11street/campus views within0.5ms noise (range-0.23..+0.08ms). Source count/failure/complete assertions passed. Trial stays explicit/offdefault; no full project or pinned-scene-clock/interior acceptance implied. Report Saved/CalderWork/codex_maple_trial_perf_perf.json; summary phase2/tree_perf_checkpoint.json. Read-only fern/stump inspection is the next asset job. Immutable fit checkpoint codex_checkpoint_tree_fit_20261017_054056 retains109text/58binary/27evidence and13protected hashes unchanged.


## Codex 2026-10-17 06:12 EDT - owned instanced woods props

Read-only inspection found fern/stump masters not marked for InstancedStaticMeshes. Created owned copies only, preserving all36acquired SHA hashes. Fern5779/1445/289triangles, stump22469/4494/1123; exactsource near counts/bounds, 3conventionalLODs at1/.15/.045, no Nanite/collision/distance fields. Materials240instructions/7samples and220/5. DisableUnity206-action codex_woods_prop_material_build.log PASS120.97s.

Native codex_woods_props_review passes:655ferns+22stumps, original-foot maxerror0.000000cm, original3375/4500 and4875/6500culling, no collision/shadows/DF/indirect. Source instancing-fallback warnings removed. WoodsPass/CampusWoods PASS;65maple/780contact fit PASS, posesbyteidentical tooriginal, windrange2.238581. Four established forest frames inspected, total90candidate/woods diagnostic frames. Wide views retain trail/hut and reveal no missing-geometry regression; do not establish close texture quality.

Added two actual-instance macro cameras, physical eye130cm and clear sightline, cached perWorld/view so selection work stays outside sampled frames. New detail views append at end and do not enter original planting NoGo filters. First207-action macro build failed due unbraced UE_LOG else and stale truncated compiler inputs after an include edit during build. All final source files were unlink-rewritten before retry, with hashes checked unchanged through compilation. codex_woods_prop_macro_fixed_build.log PASS120.13s/207actions. No further source edits during builds. Two macro captures codex_woods_props_macros currently running; visual acceptance remains pending.

Macro codex_woods_props_macros completed both ground+130cm cameras and two frames were inspected (92total). Fern reads as real fronds in original dusk. Stump frame rejected: decorative noncolliding laurel obscures the subject despite clear physical line. No plant was moved/hidden. Camera helper now rejects actual per-instance shrub/bramble visual bounds over four target rays and caches results outside measured frames. codex_woods_prop_clear_macro_build.log is running; next recapture both macros, verify evidence and refresh recovery.

207-action codex_woods_prop_clear_macro_build.log PASS123.20s. Revised codex_woods_props_clear_macros completes both existing-instance cameras at actualground+130cm, checks865shrub/bramble bounds, has no material fallback warning, originaltreeCRC0f3b2bb7/coverPlacementHash29aceab6 retained. Both revised images inspected (94total). Fern detail readable; low fallenwood visible amid groundcover, broader photo finish stillopen. Initial blocked frame remains explicitly rejected. All13protected/map hashes refreshed and unchanged. Receipt collector initially looked for crc instead of the actual placementHash field; fixed collector, no native threshold/sample/geometry changed. New checkpoint export follows; no engine job active.


## Codex 2026-10-17 06:38 EDT - opt-in photographed evergreen atlas

Read-only kit inventory138assets/897.9MiB, source hashes unchanged; whole26.84m skeletal donor exports139,849triangles/allslot0 and is not used at runtime. Staticbranch11,962triangles and originalpacked textures are available. Material graph31functions confirms CA.a coverage, NT.RG normalXY reconstructedZ, NT.B translucency; barkNAH.B AO/Aheight. Source plugin uses virtual samplers, so owned standalone PNG derivatives are imported nonvirtual under Calder/Woods with normalDX and alpha-preserving mips. No PVE/skeletal dependency at runtime.

prepare_evergreen_spray.py isolates five healthy green needle components, builds999photographed needles with rotated decoded normals, ownseed2026101707 and13.8869percent coverage at0.32. Initial479needle/5.50percent texture-only candidate was revised before import; never native-bound. Existing species4 geometry/poses/collision/culling/wind unchanged; only BaseMap/NormalMap swap under -CalderEvergreenAtlasTrial, offdefault. Import passed138source SHA guards. Sourceart PNG/normal/provenance saved under woods_sprays. DisableUnity207actions codex_evergreen_atlas_build.log PASS120.53s. Native woods/fit review codex_evergreen_atlas_review running; in-game four-view inspection stillpending. No botanical/wholeforest photographic acceptance implied.

Evergreen native codex_evergreen_atlas_review PASS: sourceatlas bound1, no fallback/missing warnings; two originalhemlock batches19/12instances retain3196/1032/226triangles; WoodsPass/CampusWoods/65maples780contacts PASS, exactactualmapleposesbyteidentical. Four codex_evergreen_atlas_views frames individually inspected (98total): subtle change in establishedwideframing, no apparent world/foliage regression. -WoodsSceneReview additionally passes81floor samples/3hollowbottoms/hutentry and716shrubs15997grassclumps. Keep atlasopt-in; closer/botanical/finalperformance acceptance pending. Native/tree and cover checksums0f3b2bb7/29aceab6 retained. No enginejob active.


## Codex 2026-10-17 06:58 EDT - merged ridge forest trial prepared

New HouseCalderRidgeForest.cpp/h adds12merged scenery tiles outside the walkable square,3gridLODs2000/4000/8000cm, oneidentity plainstaticISM/tile, no collision/overlaps/navigation/shadows/DF/indirect/tick. PureFarLand/SkylineGround reference only; existingterrain/ridge geometry and all authority hashes unchanged. New hash streams; north/south/side forest margins leave town/river/farroad/watertowerclear; only referenced vertices emitted. Own palette crosses250maboveTown(23100absolute), highrusset/brown/thinninggrey, midorange/scarlet/gold/crimson/bronze,20percentgreen valley/north-cove evergreen. NoCalderLandscape/NoTerrain and dedicated-server skip retained; -CalderRidgeForestTrial OFFdefault.

Owned M_CalderRidgeCanopy compiles224instructions/3samples includingengine temporal-dither lookup; original NaturePropTextured SHA2aef46578e18be21ea43c979cd8db2b4fea3fc251deb846c100cfb3ef35f238e retained. Firstshadercompilefailed RGBdefault/invalid.a; explicit VertexColor A connection fixedit, v2batchPASS. New nativeRidgeForestReview inspects actual CPU-retained renderpositions/settings/LODs, checks12tiles/≤24CalderISMs/surface-offsetrange/outsideworld; threeexisting self-exit lists appendflag. Original review thresholds/counts/paths untouched. Build codex_ridge_forest_build.log running; native/visual/performance unverified. Exporter now includes newowned material; earlier evergreen immutable123text/66binary/61evidence remains latestverifiedgold.

First209-action ridge build PASS121.68s. Native review12tiles/19totalCalderISMs/583134actualvertices/0failures, offsets2.9995..1452.3451cm,147628LOD0triangles, build163.52ms; WoodsPass/CampusWoods/53lights pass. NineP1-P8+P3towerframes allinspected,107totaldiagnostic tree/woods/ridge frames. Canopy subtlein haze; no finalphotographic/performance acceptance. OriginalP2verticalsilhouette verifiedagainstbridgebaseline; didnotalterit. ActualLOD0valleygreen9.15986percent missesowner10-25; cutoff.32 calibration predicts19.6033percent, newreview explicitlychecks10-25. V1 sources/evidence retained, palettebuildrunning.

Prepared3-setting fogproposal (density.016→.012, opacitycap.97→.90, glow1.3→.8), dryrunpasses, sourcesunchanged. OwnerapprovalrequestedasrequiredbyexplicitMASTER_PROMPT globalfog clause; requestpending, doNOTapplyuntilreply. Continue independentpalette/environmentwork. Proposal andsourceSHAs Saved/CalderWork/p5_fog_proposal.

Ridge palette209-action build PASS122.41s. Native palette reviewPASS12tiles/19totalCalderISMs/583134vertices/0failures, valleygreen19.6033percent, offsets2.9995..1452.3451cm,147628LOD0triangles, build166.72ms. Actual geometry CSVpositiondigest matches v1 exactly at0.001cmprecision:47513baa460c8c7e5cd5150d4b8053d54ed163c5b06827438a333e03838f0940. Both48MBverticesCSVs retained and gzip-compressedforevidence. Nine revisedP1-P8/towerframes individually inspected;116totaldiagnostic frames. Photo/colourline/finalperformance open, trialOFFdefault. All13protected/map hashes unchanged, fogproposalsourceSHAs stillunchanged/unapplied. No activeenginejob; save recovery then independently improve falling-sheet appearance while fogapproval remains pending.

Ridgepalettegold saved:129text/67binary/100evidence, actualpatchapplication/exactSHAsverified. No globalfogchange; pendingownerapproval remains. Independentfallsjettrial M_CalderFallsJetsTrial created305instructions/4samples, originalfivewater/ground/rockmaterialSHA guards retained. OpaqueGBuffer maskeddithered jets, loweruniformemission, roughness.06. NewCalderFallsJetsTrial runtimeflag offdefault selectsownmaterial only; geometry/waterqueries/collision/bridges/mist/flowparams unchanged. Buildcodex_falls_jets_build.log running; P6same-camera comparison/nativeproof pending. Exporter includesnewtrialmaterial.


## Codex 2026-10-17 07:46 EDT - waterfall motion trial

Owned `M_CalderFallsJetsTrial` compiles at 305 instructions/4 samples, roughness .06; masked opaque GBuffer with temporal coverage, reduced uniform emission, no translucent/refraction pass. Five existing material hashes stay unchanged. `-CalderFallsJetsTrial` is opt-in inside the landscape gate. Builds `codex_falls_jets_build.log` and `codex_falls_motion_build.log` pass 209 DisableUnity actions in 121.50/121.70 s. The optional `-ExteriorMotionFrames=N` helper appends frames after timing, preserves original woods-only behavior and samples, and uses the original camera.

Native P6 plus ten motion frames inspected individually (127 total diagnostic images). WoodsPass/CampusWoods/53 lights pass; four delayed frames per run spaced >=0.8 s. Open gaps and reduced glow improve the first sheet, but fixed triangular ends still resemble cloth. Enabling/readback of Unreal's Has Pixel Animation flag alone gives no decisive visible improvement. No photographic acceptance/default promotion.

The next owned shader uses continuous ballistic time-of-flight advection rather than constant-Z sliding (v0=90 cm/s, g=980 cm/s2); analytic position/velocity round-trip receipt saved. Shader still 305/4. In the same opt-in runtime path, noncolliding columns extend to their existing pool; moving shader coverage replaces fixed tapered tails. Water collision/queries/beds, bridge, mist and authority remain unchanged. `codex_falls_ballistic_build.log` running; native/visual checks pending. Previous material/script/source preserved in `falls_trials/pixel_animation_v1`. Global fog proposal remains unapplied awaiting direct owner approval.


### 07:53 EDT - ballistic falls build, motion and physical review PASS

`codex_falls_ballistic_build.log`: 209 DisableUnity actions, PASS122.22 s. All five `codex_falls_ballistic_motion` images individually inspected, total132 diagnostic frames. Three actual material bindings and noncolliding curtain markers confirm640/952/912 triangles for weir/main/cascade, fixed tails0. Animated open gaps retain the rock-house path. Static pointed tails are removed; dither grain and planar silhouette remain, so photographic finish/default promotion are still open.

`codex_falls_ballistic_terrain` passes every original terrain stage:20,000 collision samples/0holes/max.03cm, water/footpoints/world and terrain72564759/treeCRC0f3b2bb7. `codex_falls_trials_noterrain` supplies all four appearance trial flags while terrain is off; exact grounds69240/instances53740/campusFrontVertices1091586/floorSamples2654/53lights/CRC0f3b2bb7 and all layout/surroundings gates pass, no active trial markers. All13protected/map hashes remain unchanged. Receipt falls_jets_checkpoint.json. Fog proposal remains unapplied pending direct owner approval. Recovery export/test-apply/hash verification follows; broader Phase2/Phase3 and final multiplayer/performance remain unfinished.


## Farm appearance trial prepared (not validated)

Falls immutable recovery `codex_checkpoint_falls_20261017_0753` passes actual git-apply and SHA verification:132text/68binary/138evidence. Current new farm work is not in that archive. Four proposed static plain ISMs,3LODs,70-90m culls, no collision/shadows/DF/indirect/navigation/new lights. Barn(-27200,-2200) has level frame on individual stone piers over unchanged existing ground, open southern gable/soil floor, hanging dry leaves and dull corrugated roof. Proposed six native-ground bales use twine only; worm fence retains broad field entrance. Placement rejects protected/paved/stair/ramp/water/path envelopes. Existing CC0 textures/shared master read-only. Two appended cameras trace actual ground+130cm and cache before timing. `-CalderFarmTrial` OFFdefault; `-CalderFarmDump` inspects actual render buffers/foot positions/settings. `codex_farm_build.log` now running; no native/visual acceptance yet. Scope/limitations in farm_trial_plan.json. Pending fog proposal values remain untouched; its whole-file preimage SHA refreshed only for independent farm/camera hooks, patch dry-run passes.


Farm first build `codex_farm_build.log` PASS210DisableUnityactions/123.49s. `codex_farm_first_views` running with both new cameras, actual render-buffer dump and passive WoodsPass/CampusWoods checks. No visual/native acceptance until output inspected.


Farm v1 native `codex_farm_first_views` passes4components/6bales,91.1ms build. Actual geometry101,304 render vertices; bale support gaps-5.2582..1.081cm. LODs wood6616/4208/3004, roof1472/736/192, hay3280/1368/160, curing4800/864/192. Both physical-ground+130cm captures inspected (134total). Appearance REJECTED: dark drawn spiral on bale, side-lit/backlit barn hides entry. Native geometry is not photographic acceptance. V1 source/evidence preserved under farm_trial/v1 and the original run.

Read-only ordinary-editor audit `codex_farm_material_audit.log` PASS: existing instanced parent229instructions/2samples,9source hashes unchanged, no assets saved. New straw revision uses deterministic code-painted side/end albedo/normal maps on the same shader, no additional samples. Short real fibre geometry replaces dark coil; only two actual twine bands remain. Warmer weathered wood, additional appended barn-entry camera; original two cameras retained. `codex_farm_straw_build.log` running; no native/visual acceptance yet. Next also reserve barn/bale footprints from optional trial ground-cover placements, retaining exact default scatter when farm trial is off; do not edit C++ during build.


Straw build PASS210actions/122.63s; codex_farm_straw_views native4components/6bales/144.7ms, WoodsPass/CampusWoods pass. All three captures inspected,137total. Straw no longer has drawn coil, but new entry frame shows floating roof/leaves without timber frame: appearance remains REJECTED. Non-bale sources currently recentered around Barn with an equal instance translation to preserve actual world geometry; changed only appended entry eye/aim, with actual target logs. Visibility diagnosis is not yet confirmed by a new render.

Trial-only barn/bale cover exclusion prepared through pure Farm::Occupied. Default path unchanged; five-trial cover count/hash will differ. LandscapeReview retains original limits and adds stricter actual farm-footprint and <=24 total Calder ISM checks. All C++ edits finished before `codex_farm_clearance_build.log` started. Next combined actual views/cover/terrain/woods validation, then current all-trial terrain-off compatibility and recovery. New fog preimage SHA refreshed only for independent camera change; original3fogvalues untouched.


Clearance build PASS210actions/122.62s. Combined codex_farm_clearance_views passes native farm4components/6bales/145.1ms; wood6616/4208/3004, roof1472/736/192, hay3600/864/192, curing4800/864/192. Strict ground-cover review PASS57,705instances/23totalCalderISMs/zero farm footprint overlaps; old default57,815/29aceab6 remains reserved for farm OFF, current farm ON cover17dc66ec. Terrain/water/all original stages/woods pass,20kcollisions/0holes/.03cm andterrain72564759/tree0f3b2bb7 unchanged.

All three latest images inspected (140total). Recentered local mesh/translated instance preserves placement but does NOT fix entry visibility: roof/curing leaves draw without wood frame. Farm appearance remains REJECTED/OFFdefault. A launch-only no-occlusion comparison is now running, with verified engine CVars r.AllowOcclusionQueries=0/r.InstanceCulling.OcclusionCull=0; no project defaults changed or performance acceptance claimed. Current receipts farm_checkpoint.json. Do not call the visual defect fixed from numerical passes.


### Farm frame defect isolated to GPU instance culling

Three launch-only comparisons, no project CVar changes: no-occlusion still missing; forcedLOD0 still missing; r.CullInstances=0 restores the full timber boards/frame/piers at the same physical entry camera. All three images inspected,143total. Actual v2→clearance world-vertex comparison100056vertices gives max .001cm difference and exact bales: prior recentering preserved geometry but did not fix visibility. These debug settings are not fixes or acceptance.

Current `codex_farm_bounds_build.log` separates worm fence from barn wood into a fifth spatial cluster, centered at its own position. Instances populated and UpdateBounds before RegisterComponent, no global culling adjustment. Expected all-landscape total24 retains original limit24; no threshold loosened. Adds actual component/mesh bound logs and section-index/material checks. No C++ edits during build. Normal-culling native/review/capture validation still required. Farm work remains offdefault and unarchived since falls gold.


### Normal GPU culling farm fix PASS

`codex_farm_bounds_build.log` PASS210actions/123.25s. `codex_farm_bounds_views` with normal culling (no ExecCmds) passes5farm components/6bales/148.2ms and actual section ranges/settings/footprints. Split barn wood3816/2208/1404 +fence2800/2000/1600 preserves old summed triangles. World-vertex comparison100056points max.0005cm difference. All three images inspected,146total: complete timber frame/boards/piers now render at entry. All debug CVars remain unpromoted.

Cover PASS57,705/17dc66ec, zero farm footprints,24totalCalderISMs within unchanged24. Terrain/water/original stages and WoodsPass/CampusWoods pass,20kcollisions/0holes/.03cm/terrain72564759/tree0f3b2bb7 unchanged. Current all-five-trial NoTerrain compatibility running; then rerun default farm-OFF cover57,815/29aceab6. Farm remains OFFdefault pending photo/performance. Angular curing leaves, provisional mineral roof texture, pasture/terracettes/hedgerow/persimmon and broader visual work still open. No new gameplay room, collision, light or protected edit. Recovery refresh next; whole master incomplete.


### 08:32 EDT - final farm compatibility PASS; recovery refresh

`codex_farm_all_trials_noterrain` passes all original gates with all5appearance flags: exact69240grounds/53740instances/1091586campusFrontVertices/2654floorSamples/53lights/0f3b2bb7 tree CRC; no active trial markers and zero Calder ISMs. Fresh default farm-OFF `codex_farm_off_cover` PASS57,815cover/29aceab6, native settings/root/frozen/water/woods pass. All13protected/map SHA hashes unchanged. Exact global-fog preimages/source SHA/dry-run patch still original and unapplied.

Current partial farm recovery export target codex_checkpoint_farm_bounds_20261017_0832 follows, with actual git-apply/source/binary SHA checks and native/capture/failure evidence. Earlier gold untouched. Farm appearance remains provisional/offdefault despite normal-culling visibility fix and numerical acceptance; no whole-master completion. Broader Phase2/art/performance, allPhase3 and final MP/review chain remain.


Farm immutable recovery `codex_checkpoint_farm_bounds_20261017_0832` VERIFIED138text/68binary/206evidence, actual git-apply/source/binary/captured-evidence SHA checks. New curing art is beyond that archive. Real University of Kentucky cured-leaf photo inspected (reference only, no imported pixels): broad wrinkled papery lamina, pale branching rib; morphology metadata in farm_reference_plan.json. Period LOC barn photo blocked by automated security, NOT inspected/used as visual proof; browser closed.

Current new code paints512px four-variant leaf albedo/normal atlas on same229instruction/2sample shader, replaces diamond rosettes with broad curved leaves on visible hanging stems, analytic smooth normals, three LODs. Five coherent components and prior normal-culling fix retained; geometry/terrain/bales/cover footprint authority unchanged. New appended actual-ground+130cm roof eye(-29500,4000) inspects existing material BEFORE roof changes. Original three farm eyes unchanged. `codex_farm_leaf_build.log` session59091 running; no native/visual acceptance yet. Next four-view native/cover/terrain/woods run, inspect all outputs and refresh recovery. Fog remains unapplied/pending human approval.


### Curing leaf review PASS; hill roof view culled

Leaf build PASS210actions/121.95s. codex_farm_leaf_views all4frames individually inspected,150total. Entry leaves now broad/curved/veined on whole-plant stems; normal-culling timber intact. Native curing113280/10368/1152triangles, diagnostic FarmDump build408.0ms; not final timing. Five farm/24totalCalderISMs,6bales, cover57,705/17dc66ec/zero farm overlaps; terrain/water/20kfloor samples/woods all pass. Material same parent229/2; new textures code-painted, real photo reference only. Receipt farm_leaf_checkpoint.json.

Hill camera actualground+130 at(-29500,4000), eye1129.682/targetroof800, but barn absent: first roof frame rejected for material assessment. Existing project ViewDistanceQuality1 applies r.ViewDistanceScale0.6 (native log835), so90m cull scales to54m, shorter than66m camera range. Current scoped architecture cull fix: ONLY wood/roof180/225m; hay/curing/fence remain70/90m, globalquality/fog untouched. Native checks/logs now read actual ranges and scale. codex_farm_arch_cull_build.log session98033 running; no source edits during build, four normal-culling views/all native reviews next. Whole photo/performance/default promotion remains open.


### 2026-10-17 09:02 EDT - roof range verified; owned metal/rust shader prepared

Arch-cull build PASS210actions/121.90s. All4codex_farm_arch_cull_views frames inspected,154total. Native exact architecture18000/22500, field7000/9000, existing scale.600/globalunchanged; physical hill roof view now shows barn. Entry/cure shape intact. All farm/24ISM/57,705cover17dc66ec/terrain/water/woods reviews PASS. Roof remains flat/uniformbrown: next scoped material/normal refinement. Receipt farm_arch_cull_checkpoint.json.

Owned farm_roof_assets_batch.py PASS229instructions/2samples/six materialSHA guards unchanged in codex_farm_roof_assets_v2.log. First launch had duplicated script folder and did not execute; logpreserved. New opaque ownM_CalderFarmRoof uses albedoA asmetal fraction and roughness.93rust/.49metal; no other master/map saved. Shared Hall painter forcesalpha1, so new ownedRGBA/mip writer preservesmetal data and reads actual top/tail bytes beforeRHI update. Code-painted periodicrust/galvanized maps, true corrugation normals/320cmUV; export includes newasset. codex_farm_roof_build.log session23075 running; nativealpha/mips/binding and4images/allpriorreviews stillrequired. No globalquality/fog/terrain/collision/cover/tree/light changes. Whole photo/performance/allwildlife/finalMP remainopen.


Roof first C++ build codex_farm_roof_build.log FAIL121.11s on new local float Metal shadowing namespace material key Metal (-Werror,-Wshadow). No native roof run launched. Renamed only the scalar to MetalFraction after build exited; retry codex_farm_roof_name_build.log. Asset compile229/2 remains valid, previous tested farm gold intact.


### 2026-10-17 09:25 EDT - roof native/art and current fallback gates PASS

Retry build210DisableUnityactions/121.70sPASS. codex_farm_roof_views native bound1/rawalpha0/255/tail171/mips10, five farm/24totalISMs/6bales, diagnosticbuild415.5ms (notfinalperf), cover57,705/17dc66ec/zero farmoverlaps. Terrain/water/20kfloor samples/0holes/.03cm/woods/allstagesPASS. Four images individually inspected,158total: truecorrugation normals plus differentiatedmetal/rust improveflatbrown; entrycurvedplants/frame intact, originalwide/baleeyes retained. Wholephoto/performance stillopen/OFFdefault.

Fresh codex_farm_roof_noterrain with all5trialflags restoresexact69240/53740/1091586/2654/53/0f3b2bb7, zeroCalderISMs/no activebindings. Fresh farmOFF cover57,815/29aceab6/woodsPASS. Fresh codex_farm_roof_nolandscape with all5trialflags keeps terrain/water/woodsPASS, zerocover/3legacyCalderISMs/no activebindings. Current13protected/map hashes unchanged; fogoriginals/sourceSHA/exactpatchdryrun unchanged/unapplied. New partial recovery target codex_checkpoint_farm_roof_20261017_092532, actualgitapply/source/binary/evidenceverification next. Prior immutablefarm_bounds138/68/206 retained. BroaderPhase2/Phase3/finalperformanceMPvalidation incomplete.


### Owned rock bedding shader - failed first compile retained

First unbound rock shader batch failed because HLSL reserves `line`; actual compile errors retained under rock_strata_trial/failed_shader_v1 with the owned asset/script/log. No runtime binding or original-master save. Renamed local to bedLine; ownership hash receipt allows replacing only this proved owned failed asset. Successful statistics now required BEFORE saving. Retrying same opaque/world-normal scan wrapper; all six original-material guards retained.


Rock binding build PASS210/122.94s. Paired originalP1/P2/P6/P7 captures all8inspected (166total); native eight-sample shader bound only five intendedsets, zero bedding on masonry. Terrain20k/0holes/.03cm, water/woods/cover57705/all24ISMs pass. Wide-view change subtle; no photographic acceptance/default promotion. Appended physical-eye rock/wall macros for closer assessment. First macro build deliberately canceled after source-name inspection found helper expected CalderFeatures rather than actual CalderWall collision prefix; correct prefix/low-wall aim before restarting, no concurrent C++ edits.


Macro-prefix build PASS210/122.95s, but first two camera probes both rejected their physical surfaces. Two fallback frames inspected (168total), NOT material acceptance. Crow rear aim hit no wall; P1-side aim hit terrain rather than wall. Read-only source-authority survey now identifies actual430cm road-cut rise atX10630/Y28000 and360cm campus terrace atX-18700; new seedeyes chosen on lower existingroad/path ground away from walk00 stair axis. No geometry/authority change or rebake. Pendingfogpreimage refreshed for independent screenshotseed edit, originalvalues unchanged.


### Rock seam checkpoint - current native and image review PASS

Final source build codex_rock_macro_survey_build.log PASS210actions/122.77s. Owned v3 material compiles370instructions/8samples, six originalmaterial hashes unchanged. Latest six frames individually inspected; 178total diagnostic frames. Existing scan texture dominates under dusk; shale seams subtle, no photographic/performance acceptance or default promotion. Native actual binding covers five intended sets with bedding zero on masonry.

Current terrain/water/woods checks PASS20,000collision samples/0holes/.03cm, terrain72564759/tree0f3b2bb7; four cover batches57,705/17dc66ec and24totalCalderISMs/zero farm overlaps. Both real close-up probes confirm130cm eye height and actual CalderWall collision faces at387.57cm/300cm. First two failed macro cameras and first shader failure/canceledbuild/earlier subtle shader preserved.

Both all-six-trial fallback runs pass: NoTerrain restores original69240grounds/53740instances/1091586campus vertices/2654floor probes/53lights and zeroCalderISMs; NoLandscape retains terrain/water/woods PASS with zero cover/three legacyCalderISMs/no active trial bindings. All13protected/map SHA guards retained. Fog remains unchanged/unapplied; proposal preimage refreshed only for appended screenshot code and exact patch dry-run passes. New immutable recovery target codex_checkpoint_rock_strata_20261017_100252 next; verify actual applied source/binary/evidence hashes before relying on it. Broader rock drill marks/seeps/stone vegetation, farm pasture/terracettes/persimmon/hedgerows, water edges, wildlife and final performance/MP chain remain open.


### Persimmon source prepared, not yet accepted

New optional CalderFarmTreeTrial adds a source-reference-informed common persimmon merged into the nearby existing fence cluster, retaining five farm/24landscape ISMs. Source photo references (fruit, winter habit, bark) individually inspected on Oregon State's page; no photo pixels downloaded/imported. Private code-painted maps and three runtime geometry LODs. Exact visible main-stem rings provide trunk-only collision on clients and dedicated server; 96native contact rays and physical image cameras are required. FarmTreeOFF retains existing farm footprint/range; new switch enabled uses225m tree/fence cluster range with unchanged globalquality. No terrain grading, originaltree edit, newlight, room, map save or fog change. Fresh build and all native/image/dedicated/disable checks pending; keep OFFdefault.


Persimmon v1 build PASS211actions/122.73s. Client and dedicated-server96/96trunk contacts pass, native terrain/water/woods and24ISM/57,704cover pass. Four images inspected (182total). Rejected tree/fruit presentation: secondary twigs started on the straight chord rather than the rendered bent branch, causing visible gaps; whole-tree camera clipped its top and a branch obscured the fruit. Fix uses shared piecewise branch points for geometry/twig attachment, continuous bark coordinates, less regular bark plates, a gently bent exact shared stem, a wider physical whole-tree eye and branch-clearance-tested fruit eyes. Native/render/dedicated/disable checks must rerun on the changed source.


Attachment build PASS211/122.77s; client and dedicated server96/96trunk rays pass. Four new images inspected (186total). Shared branch points remove floating twigs, but open angled tube joins leave visible wedges in the close-up and first clear fruit eye is backlit. Add small bark joint surfaces at branch/twig bends, prioritize the southern physical fruit eye (diagnostic angle can be selected without changing original views), and lower whole-tree aim30cm for more root margin. Source remains optional/unaccepted until fresh native/art/server/fallback checks. Actual324,840barn/roof/bale/curing vertices match prior roof checkpoint exactly, maxdelta0cm. Cover changes only by removing one tuft whose bounds meet trunkfootprint; no added or moved roots. Spatial proof farm_persimmon_trial/spatial_comparison.json.


Joint build PASS211/123.27s. Current trunk96/96/cover57,704/24ISM/terrain/water/woods pass; four landscape images inspected (190total). Branch-bend wedges closed; fruit eye clearer but whole tree still crops highest shoot. All six final gate runs PASS: treeOFF57,705 and original2800/2000/1600fence70/90m, NoTerrain exact69240/53740/1091586/2654, NoLandscape originalterrain/water/woods and0cover/3legacycomponents, NoFarm restores57,815, dedicated96/96 with no addedfarmdraw batches, portraitnativechecks. Both720x982portrait frames inspected (192total): macOS clamps requested720x1280window, treetop still cropped; NOT accepted as complete-tree framing. Only appended whole-tree camera FOV70→95 now changes; all tree/farm/collision/gate source unchanged, original cameras unchanged. One final targeted native/capture run required after camera build.


### Persimmon source/native/visual checkpoint

Latest camera build PASS211actions/124.27s. codex_persimmon_framed_views current native96/96exact visible trunk contacts, terrain72564759/treeCRC0f3b2bb7/20kcollisions/0holes/.03cm, water/woods/cover57,704/24ISMs pass. Both latest frames individually inspected (194total);95degree appended whole-tree view now contains root/highestshoot and35degree fruit view retains physical floor/branch clearance. Full photographic/performance acceptance remains open; default OFF.

Tree/fence shared mesh12898/7236/4626LODtriangles,39fruits/26sparse leaves. No additional draw component, one exact trunk collision component; original five farm ISMs retained. Native/current-source geometry comparison324,840barn/roof/bale/curing vertices exact0cm delta; one cover tuft removed by trunk footprint and no added/moved roots. All six gate runs pass at the unchanged tree/farm/physics source: treeOFF restores57,705/originalfence/culls; NoTerrain restores69240/53740/1091586/2654/0CalderISMs; NoLandscape keeps terrain/water/woods with0cover/3legacycomponents; NoFarm restores57,815; dedicated96/96with no addedfarmdraws; portraitnativepass but rejected framing. Only later source change was new camera FOV.

All13protected/map hashes and seven materialinput hashes unchanged. Fog remains original/unapplied and exact patch dry-run passes; proposalSHA tracks independent screenshot edits. Target immutable recovery codex_checkpoint_persimmon_20261017_104039 next, require actual source/binary/evidence verification. Earlier rejected twigs/joints/framing and all native logs/CSVs remain. Pasture/terracettes/hedgerows, other rock/wall/field detail, water edges/mist, all wildlife and final review/performance/MP chain remain unfinished.


Optional barn merge initial build PASS211/122.86s. Before native comparison, added opt-in FarmDump export of actual architecture triangle-corner world positions/normals/tangents/handedness/UVs/material slots/parent names. This diagnostic-only output supports a renderer-data comparison between mergeOFF and mergeON rather than relying on unchanged source section arrays. No screenshot, collision, original review threshold or runtime geometry change from the exporter. Merge remains OFFdefault/unaccepted until both exports and all images/native checks pass.


### Coherent barn merge native/attribute/image checks PASS

Build211/121.58sPASS. MergeOFF and ON actual render-buffer comparison29484triangle corners/19472unique records retain identical positions/normals/tangents/handedness/UVs/materialslots/parentmaterials, canonicalSHA31839b264ab7d9d048c0d8efb9e5b461bf47e089916e5c34cf88ddd653848bbf; maxdelta0. Other362064farm vertices and57704cover poses exact. Twelve paired images individually inspected (206total), normal GPU culling and original view eyes retained.

MergeON uses fourfarm/23totalCalderISMs; barn5288/2944/1596LODtris and180/225mculls. OFF retains fivefarm/24total, originalwood/roofgeometry/culls. Cover57704/a9b515e8, terrain72564759/tree0f3b2bb7/20kcollisions/0holes/.03cm, water/woods pass. New all-eight-trial NoTerrain restores exact69240/53740/1091586/2654/0CalderISMs; NoLandscape keeps terrain/water/woods with0cover/3legacycomponents. All13protected/map and seven materialinput SHA guards unchanged; fog unchanged/unapplied/exactdryrunPASS.

Recovery target codex_checkpoint_barn_merge_20261017_110139; verify recovery_receipt before relying on it. Previous persimmon gold verified152text/70binary/641evidence retained. Merge remains optional; full photographic/final performance unaccepted. One landscape component slot is free for hedge detail. Existing hedge boxes remain untouched; 3D hedgerow plants/core treatment, farm pasture/terracettes, other rock/wall/field detail, water edges, all wildlife and final full-chain acceptance remain open.


### Farm-fence hedgerow material prepared; planting unimplemented

Owned M_CalderHedgerow compiles229instructions/2samples, opaque/ISM-enabled; all eight existing material SHA guards unchanged. Builder hedgerow_assets_batch.py. Two per-instance custom floats(rootZ/height) give coherent root-pinned world wind across canes/leaves/hips; existing public gust actor drives WindStrength. Asset is unbound; no new shrub geometry, placement, camera, wind registration or native acceptance exists yet. Exporter includes the owned material.

Inspected actual NC State rose leaf/hip and redcedar scale photographs in a background browser, then closed the tab. No photos downloaded/imported; plan and clean source URLs in hedgerow_fence_plan.json. First planting pass is alongside existing farm worm fence, with broad gate/frozen/paving/path/water/building exclusions and one new plain ISM at70/90m. Barn merge frees the slot so total24limit stays unchanged. Existing boundary boxes/collision remain untouched; replacing their visible box cores is separate unfinished work. Preserve references to actual cedar scales and small~0.6cm multiflora hips; do not substitute spruce sprays or large persimmon-like fruit. C++ planting implementation and all native/visual/performance checks remain next.


Hedgerow C++ first implementation prepared: HouseCalderHedgerow(.h/.cpp) plus geometry helper, one mixed plain ISM, three LODs, coherent custom-data wind and existing public gust registration, full farm-fence exclusions and no added collision. Native passive review checks settings, physical root gaps, custom data, frozen bounds and actual WindStrength variation; original terrain/water/woods/24limit unchanged. New field/hip/cedar-scale cameras appended, physical130cm eyes and bounded triangle-visibility search. All source edits finished before first build. No native or visual acceptance yet; keep OFFdefault. Globalfog originals preserved; proposal preimage refreshed only for independent integration/view hooks.


### Hedgerow native pass; appearance diagnosis and foliage revision

First213action/123.57sbuild passes. Native25clumps/24landscapeISMs/57,704cover/a9b515e8/terrain72564759/tree0f3b2bb7 and originalterrain/water/woods pass. Rootsmax.004cm and sharedwindrange.907795. Three captures inspected,209total; rejected bare/coarse appearance. Confirmed far-leaf geometry used only zero-width endpoints and had no leaf triangles.

Foliage geometry rebuild213/125.02sPASS; all actual foliage sections now present at every LOD. Three new images inspected,212total, still coarse/bare. Both globalForceLOD0 diagnostic and component-onlyForceLOD0 probe produced similar coarse appearance; six further images individually inspected,218total. Neither force is promoted. Probebuild213/124.70sPASS, actual screens1/.65/.12 andradius160.521cm. Earlier source/captures preserved under hedgerow_trial/fence_plants.

New owned M_CalderHedgerowFoliage compiles240instructions/2samples, masked/two-sided foliage, nine materialSHAguards unchanged. Curved code-painted alpha-preserving cedar sprays retain medium/far foliage mass; selected real1.9mmscales remain for close views. Leaf camber reduced. SixMIDs retain common root-pinned wind. Current sprays build pending; native/art/fallback/server checks and immutable recovery refresh required. Existing boundary boxes/collision untouched, globalfog original/unapplied pending direct ownerreply. No whole-master/photo/performance/default promotion.


### Hedgerow first integration and five fallback gates PASS; visual acceptance open

Sprays build213DisableUnityactions/124.02sPASS. Current native25clumps, one newplainISM,24totalCalderISMs, six windMIDs/range.907797, rootsmax.004cm and actualLODs28485/4286/1450/firstresident0. All original terrain/water/woods pass:20kcollisions/0holes/.03cm, hash72564759/tree0f3b2bb7. Cover57,704/a9b515e8; allactual57,704poses match prior barn-merge nativeCSV exactly, SHA db3066baec3f56eaa2388f92d05da9efb7aa6089c8959fa621b58f1930a42aa9.

All five current-source comparisons PASS: hedgeOFF originalfivefarm/24ISMs; NoTerrain exact69240/53740/1091586/2654/0CalderISMs; NoLandscape originalterrain/water/woods and0cover/3legacy; NoFarm57,815/19ISMs; dedicated96/96persimmoncontacts/noaddedhedgedraws. All13protected/map and nine originalmaterial hashes remain unchanged; globalfog original/unapplied/exactdryrunPASS.

Latest3images individually inspected,221total. Leaf camber softer; field silhouette sparse and cedar/macro presentation still below photo target. No default promotion or finalperformance/photo/master acceptance. Existing boundaryboxes/collision untouched. Current phase2/hedgerow_checkpoint.json; immutable recovery target codex_checkpoint_hedgerow_20261017_115338 next. Continue missing2d/2e, allPhase3 and fullfinalchain; prior gold untouched.


Cedar-density geometry-only build213/125.34sPASS; all three new images inspected,224total. Field cedar clearer, macro still artificial/occluded; not photo accepted. Current native25clumps/24ISMs/57,704cover/a9b515e8/terrain72564759/tree0f3b2bb7/roots.004cm/wind.907803 allpass. LOD29925/6210/3598. Prior five fallback/server cases invoke no hedge geometry; their enable/placement/server code is unchanged. Geometry change and new farm-solid plan remain beyond immutable hedge115338(167text/72binary/912evidence).

Farm-solid implementation prepared behind CalderFarmSolidTrial: same LOD0barn frame/piers/roof and fence, exact shared original bale poses/core surfaces, three static collision components on client and dedicated, no new draws/ISMs/lights or terrain edits. Curing leaves, loose straw and twine excluded. Original draw geometry/poses retained. New passive native review compares every collision vertex against actual LOD0render data on client, frozen hits, native physical contacts and client/server canonical collider CRC. Routes remain a separate required gate. No build/native/route acceptance yet. All C++ edits completed before build.


Owner now explicitly approved the prepared global-fog comparison and keeping the better result. The three-setting patch remains unapplied until the current farm-solid native run finishes. Preserve a same-source original P1-P8/street/house/woods baseline, then compare approved .012density/.90opacity/.8glow against originals. No extra approval needed. Farm-solid first build214/124.50sPASS; first physical/native run codex_farm_solid_first_native now active.


Farm-solid first native run caught original slope review failure: four steep bale faces lacked explicit Unwalkable override. No review was weakened; new props now use Unwalkable, matching the hard constraint. First build214/124.50sPASS, collision9,240triangles/clientCRC7764fe3e/5.7ms; own native9s review was not reached before terrain exited. Failed source/log retained. New pure cached identical fence runs reserve only overlapping cover roots under the solid trial; all candidate RNG draws remain before rejection, preserving remaining poses. New build/native checks follow; no route/physical acceptance yet.


Farm-solid slope-fix build214/132.57sPASS. Current codex_farm_solid_slope_native PASS original terrain/water/woods/slopes/cover and newSolidReview:18,632collisionvertices all match actualrenderedLOD0vertices,1,685/1,685nativecontacts/max.0008cm,3collisioncomponents/9,240triangles/CRC7764fe3e/clientbuild6.0ms. Cover57,652; only52tufts removed, zeroadded/moved. Actual29,484architecturecorner attributes and362,064otherfarmvertices exactly match prior barn-merge native export; canonicalSHA31839b264ab7d9d048c0d8efb9e5b461bf47e089916e5c34cf88ddd653848bbf. All24ISM/terrain72564759/tree0f3b2bb7 preserved. Five fallback/server tests in progress.

New unbuilt farm walker prepared:HouseCalderFarmSolidWalkReview.cpp,11ordinary-input legs through gate/barn and back,25cm actualcapsule/floorprofiles and3captures. Uses same8s stall/30s leg/35cmhorizontal/8cmvertical movement checks as existingAccessReview; originalAccessReview routes/limits unchanged. New self-exiting flag registered only by appending its name in three master-required lists and terrain shared-run list. Build waits until active five-gate run finishes. Then walk/no-traps/screenshots and approved fogbaseline/comparison remain.


Farm walker build215/134.11sPASS; codex_farm_solid_walk passes773actualpawn-capsule/floorprofiles and11ordinary-input legs through fieldgate/barn and back,96swalltime. New physicalreview stillPASS1685contacts/18632render-matchedvertices/CRC7764fe3e; originalterrain/water/woods/slopesPASS. All3player-camera captures individually inspected,227total. Character/heldhat remain visible (testdoesnotaltercharacter). FullCalderReachReview/no-traps active as codex_farm_solid_reach; fog remainsoriginal/unapplied/explicitlyapprovedforcomparison.


First currentReach invocation omitted the documented required NoTerrain baseline and is NOT accepted:6094/6094paths,47/47entrances,6/6overlooks,0traps/0seedtraps but7040unclassifiededgecolumns withbaseline=none. No review/source change. Fresh all-nine-flag NoTerrain reach baseline now active; then use -CalderReachBaseline=<thatNoTerrainfolder> exactly as existing fix_checks.sh. Retain first failed log/result.


Correct full farmSolid Reach comparison PASS after freshNoTerrainbaseline:6094/6094paths,47/47entrances,6/6overlooks,3/3gates,8/8eyes,78/78stairends,12/12ramps,1891dropseeds,0traps/0seedtraps/0newedgeescapes/0holeexits; lowestreached-564.9 above-699.2minimum. Originalreviewunchanged. Native/walker/5fallback-server/render-attribute/cover checks nowpass; fullphoto/finalperformance/masterremainopen/defaultOFF. Target newimmutable codex_checkpoint_farm_solid_20261017_123003. Fogcomparison explicitlyapproved; sourceoriginal/unapplied, prepared gatedpatch preserves oldfogwithNoTerrain/NoLandscape. Next sixteenoriginalbaselineviews beforepatch/build/matchedcomparison.


### Owner map-wide tree replacement validated, 2026-10-17 14:19 EDT

**Latest update, 2026-10-17 14:19 EDT:** The owner's map-wide tree request is functionally complete. Normal default Calder play (no optional trial flags) passes 33/33 tree-batch replacements, 1,600 instances, 280 baked town trees, zero legacy tree batches/sections, 7,800 contacts/max0.000006cm. Actual planting poses and body boxes are byte-identical to the original baseline; original culls, collision, shadow/DF/indirect settings and treeCRC0f3b2bb7 remain exact. The default three-camera woods/street/tree-variety capture was individually inspected. Total diagnostic images inspected:328.

The owner explicitly permits tree appearance changes across the entire map, including house yards. Other house/room/lot/ground/light protections remain. New models use existing tree components and skyline tiles; no additional tree actors, components, lights or ISMs. Dead trees and saplings are included. Legacy source assets remain only for the intentional -NoCalderTrees/-NoCalderLandscape/-NoCalderTerrain comparison paths. No map saves or commits.

All current gates PASS: full27-camera sweep inspected; two sign-clearance frames inspected; five woods attachments retained; original65-maple fit780contacts/wind2.238593/exactposes; original44-leg woods walk; actual weapon/physics tree review11events/115landed/peak78of216/zero house damage; NoTerrain and NoLandscape original fallback counts; native dedicated tree physics with exact client/server poses/bodies. All650 acquired-source and13 protected/map hashes unchanged. Latest build codex_map_trees_sign_build.log218actions/133.52sPASS. Earlier failed/superseded geometry and harness invocations are retained; no review threshold changed.

All six map-tree timing runs complete, C++ SHA freeze verified unchanged. Three alternating pairs,17views,300samples/4swarmup,uncappedMetalOffscreenOnly. New-minus-original medians: front_street-7.31ms,street_along-3.05ms; woods edge-1.91,20m-1.55,deep-3.14,silhouette-.73ms. Largest increase+.46ms(clocktower), within specified±.5ms noise. Other campus views within noise or faster. This is a tree A/B diagnostic, not the final pinned-clock/all-interior/p8p/wildlife/MP acceptance. Receipt SourceArt/CalderLandscape/phase2/map_trees_checkpoint.json, full report Saved/CalderWork/codex_map_trees_timing_perf.json.

Latest immutable recovery **Saved/CalderWork/codex_checkpoint_map_trees_20261017_141249** is VERIFIED:187text test-applied/72derived binary SHAs/1782evidence SHAs, recovery_receipt passed=true. Prior farmSolid123003 remains unchanged. Approved fog work is beyond this tree checkpoint until a fresh recovery export.

**Next actual work: approved fog comparison.** Approval is already recorded; do not ask again. Current fog remains original. Capture fresh current-tree17-camera original baseline as codex_fog_map_trees_original, then apply ONLY Saved/CalderWork/p5_fog_proposal/tree_scope_refresh/approved_gated_comparison.patch after verifying its plan preimages. It includes explicit HouseCalderLandscape.h in both files, .016→.012 density, .97→.90 opacity cap,1.3→.8 sunset glow; disabled terrain/landscape retain original values. Build, capture identical17views, inspect pairs, keep only the better dusk/visibility result. Prior pre-tree17baseline remains as history. Do not apply stale older source preimages.

Broader master remains incomplete: Phase2 field/boundary hedge/rock/wall/farm/water/mist finish, all Phase3 director/serverclock/roster/reactions/sound, final performance/rendered multiplayer and full smoke/walkthrough/render/documentation/capture chain. Whole-master photographic acceptance is not implied by the completed tree request. Continue in master order with actual native and visual evidence.



### Approved fog comparison retained; normal and disabled modes verified, 2026-10-17 14:32 EDT

**Latest update, 2026-10-17 14:32 EDT. No active engine/build/editor job.** The owner's map-wide tree replacement is complete and active in normal Calder play. Runtime default census:33/33 standing/sapling/dead-tree batches replaced,1,600 instances plus280 baked town trees, zero legacy render batches/sections. The user explicitly authorizes tree appearance changes across the map, including house yards; all other house/room/lot/ground/light protections remain. Existing tree components and skyline tiles are reused; no added tree actors/components/ISMs/lights, no map saves/commits.

Original planting poses and body boxes are byte-identical, SHA25e75205527fa35eef12f58a553e09b6d4f1109d4212b8f2f03f82bce707fd7d /3d7b2a78a33ffd88932b1ee43b2af10250d5472bcf7a98aca83412ed595e3b3c. Existing collision/culls/shadow/DF/indirect settings retained. Actual7,800 contacts/max0.000006cm and original treeCRC0f3b2bb7. Full27tree images plus local sign2 inspected; five woods attachments retained. Original65-maple fit780contacts/wind2.238593/exactposes,44-leg woods walk, weapon/physics interaction11events/115landed/peak78of216/zero house damage allPASS. Native dedicated tree bodies/poses match client. Earlier failed/superseded trials preserved; no original review threshold changed. All650 acquired-source SHAs were verified during tree pass;13 protected/map SHAs reverified unchanged after fog. Source details in phase2/map_trees_checkpoint.json.

Tree A/B completed all six17-camera runs, three alternating pairs/300samples/4swarmup/uncappedMetalOffscreenOnly, C++ freeze exact. New-minus-original median front_street-7.31ms,street_along-3.05; woods edge-1.91,20m-1.55,deep-3.14,silhouette-.73ms. Largest increase+.46ms(clocktower) is within specified±.5msnoise; other campus views within noise or faster. Those measurements isolate tree geometry with original fog; **not final pinned-clock/all-interior/p8p/wildlife/rendered-MP acceptance**. Full report Saved/CalderWork/codex_map_trees_timing_perf.json. Do not rerun unchanged geometry gates without a new change or concern.

**Approved fog comparison is finished and candidate retained.** Exact three values only: density.016→.012,maxopacity.97→.90,glow1.3→.8, conditional on HouseCalderLandscape::Enabled. The explicit HouseCalderLandscape.h include is present in both consumers. Build codex_fog_map_trees_build.log218actions/135.27sPASS. Fresh current-tree17original+17candidate frames individually inspected. P5/P8 warm wash modestly reduced and silhouettes clearer; woods/porch/street keep dark dusk/sodium mood. **P5 river/full ridge-layer readability still unresolved**, as are P8partial roof occlusion,P2existing stadium slab and waterfall planar/temporal-grain finish. No additional global tuning is approved by this test.

Live GetAll world-component dumps and dusk logs verify normal default actualdensity.012000/opacity.900000/glow.80. Both NoTerrain and NoLandscape restore actual.016000/.970000/glow1.30. All three native default/fallback cases pass with tree poses/bodies/settings/results byte-identical to their prior verified counterparts; NoTerrain original69240grounds/53740instances/1091586frontvertices/2654floorsamples/0CalderISMs; NoLandscape original terrain/water/woods/0cover/3legacyCalderISMs. Candidate all-nine-trial native terrain/farm/hedge/cover/tree/water checks pass:20ksamples/0holes/.03cm/685frozenrows/57,652cover90e3d189/24ISMs/terrain72564759. Final normal default three woods/street/tree-variety images inspected; **365diagnostic images inspected total**. Detailed receipt phase2/fog_comparison_checkpoint.json, runtime proof p5_fog_proposal/tree_scope_refresh/runtime_fog_receipt.json. No repeat approval needed; do not reapply the now-applied comparison patch.

Recovery target **Saved/CalderWork/codex_checkpoint_trees_fog_20261017_143043**; verify its recovery_receipt passed=true before relying on it. Prior immutable tree141249 is VERIFIED187text/72binary/1782evidence and remains unchanged. The generic archive's whole-master default_promoted=false does not disable the owner-requested default tree/fog changes; wider unfinished art trials remain opt-in.

**Resume broader master:** Finish remaining Phase2d field/boundary-hedge/rock/wall/farm presentation and2e water/edges/mist, then all Phase3 director/FastArray/server scene clock/roster/species/reactions/sound, then final full smoke/walkthrough/render/documentation/capture/performance/multiplayer chain. No scene-clock flag implementation or wildlife framework is complete yet. Original rendered multiplayer zero-correction gate remains unresolved; do not claim server-only tree physics is multiplayer acceptance. Preserve the exact original harness, protected rooms/sources, terrain frozen/cadastre locks and all review thresholds. Whole-master/photo-quality acceptance remains incomplete.



### Final editor inventory correction, 2026-10-17 14:36 EDT

Recovery target **Saved/CalderWork/codex_checkpoint_trees_fog_editor_note_20261017_143608**; verify recovery_receipt passed=true before relying on it. Previous trees/fog143043 recovery VERIFIED189text/72binary/1962evidence and tree141249187/72/1782 remain unchanged. This follow-up explicitly preserves the timing caveat and editor restart note. The generic whole-master default_promoted=false does not disable the requested default tree/fog changes; wider unfinished art trials remain opt-in.

**Editor/timing correction:** Final process inventory found a separate interactive SeniorSendoff editorPID38368, running for about65minutes and therefore present during tree timing. It has no task review UserDir and was left untouched; never kill/save/close that session automatically. The native/capture game processes used the new build, but the interactive editor needs a save/restart to load latest C++ changes. The six A/B results above are valid reported diagnostics, **not isolated final-performance acceptance**. A clean final benchmark is still required with the editor closed, along with the full master chain. No claim that all engine/editor processes were closed should be inferred from completed task-owned jobs.


### Wildlife Phase3a implementation started; not yet built

Owner explicitly requested wildlife now and original MASTER_PROMPT plus handoff as requirements; this advances Phase3a ahead of remaining scenery polish. Start snapshot Saved/CalderWork/wildlife_start_20261017_144530 (549files). New phase3/PLAN.md, offline static deer derivatives and prepared Tools/calder/wildlife_assets_batch.py. Asset batch is UNRUN: interactive editor38368 is open, one close-editor question is pending. Do not run asset mutation until owner closes it. Source Fab/map remain unchanged.

New UNBUILT/UNACCEPTED HouseCalderWildlife director/FastArray,16-control static pose/render, analytic habitat graph and PathsReview; NetCore dependency added. Roster first-slice3/4/3deer,onebuck/3spotlessyoung. Four shared movableISM batches use Wildlife_ naming (not reserved or landscape-Calder component prefixes); original landscape24budget unchanged and new native wildlife<=56/20 checks are separate. Feature currently development-opt-in CalderWildlifeTrial (or review flag), NoCalderWildlife/NoTerrain disable it. New self-exiting review names appended to original three lists plus terrain shared-run list without numeric changes. First source build and dedicated path gate next. Visuals/actual feet/ordinary-input reactions/audio/two-process synchronization/bounds/performance and all other species remain unaccepted. Do not treat source implementation or offline geometry preview as native map acceptance.



### Wildlife native graph checkpoint,2026-10-17
Third223-actionbuild PASS135.13s (codex_wildlife_pimpl_build.log). Standalone dedicated codex_wildlife_paths_server PASS:2138/2138clear graph edges,649trunks,groups258/347/59nodes,10deer3/4/3,onehard-antlerbuck/3spotlessyoung,habitat32f8531b/terrain72564759,no runtime skeletons/lights. First combined dedicated invocation failed the original render-dependent terrain check because dedicated has0render tiles; preserved, no threshold changed. Asset batch remains UNRUN pending owner close-editor reply. Further source is unbuilt: explicit direct/alarm/setup state causes, return-interruption fix,farm footprint checks,ordinary-input walking/sprint/crawl/campus reaction and detail review. New review acceptance is deer-only; all native visuals/feet/reactions/audio/two-process/bounds/performance/other roster remain pending.


### Deer ordinary-input logic/fallback proof,2026-10-17 15:42EDT
Build codex_wildlife_probe_build.log224actions/136.48sPASS. Real walking/sprint/crawl/campus approaches allPASS12thresholds using actual3Ddistance and key posture. Walking59.88/44.85/34.61m, sprint119.94/89.62/69.34m, crouch35.87/26.97/20.90m,campus29.81/22.23/17.17m. Same wary deer15ssettlePASS. Explicit nonrendered logic probes:0hoof samples,0captures, no native feet/visual acceptance. Default dedicated2138/2138paths, farm trial2107/2107; both10deer3/4/3onebuck/3spotlessyoung. NoWildlife/NoTerrain0actors/components/instances. Receipt Saved/CalderWork/codex_wildlife_logic_remaining/receipt.json, walk codex_wildlife_walk_probe. Protected13source/map hashesPASS. Current network build underway; old multiplayerharnessunchanged. Assetclosure question/editor38368 stillpending, batchUNRUN. Sound/fullroster/feet/render/perf/return/two-process allunaccepted.


### Deer network harness diagnostic,15:49EDT
Clean225-actionbuild codex_wildlife_network_clean_build.log PASS134.71s. First two-process real-lobby logic run received matching direct flee on client21.46ms after authority change;terrain/habitat/state/referencepose matched. Client PASS, host shutdown hung because client exit removed the remote pawn before the host's delayed finish. Preserved codex_wildlife_network_logic_v1 as FAIL; shutdown guard is corrected, rebuild/rerun next. A mistakenly started return probe was stopped during startup immediately when the still-running host was found; it has no acceptance result. Only those task-owned games were stopped; userEditor38368 remains untouched. No visual/render/feet/audio acceptance.


### Deer recovery and network logic checkpoint,16:00EDT
Latest editorbuild codex_wildlife_scene_clock_build.log225actions/136.27sPASS, no include-order diagnostics. Background scene pin now holds feedposes/wandering while reactive serverclock advances. Two-process normal-lobby codex_wildlife_network_logic_v2 PASS bothpeers:matchingterrain72564759/habitat32f8531b/state/referencepath;client31.83msafterauthorityflee. First21.46msclientrun'shost-exitbugfixed andfailedrunretained. Thisisdata-only, notclient-visiblepixelacceptance;originalcampus/hillharnessSHAunchanged. Real-time campus returnprobe_v2 PASS:142.02sdelay, ordinary-inputretreat, safegraphreturn tooriginalhome andfeedingresumed. No timeracceleration. Feature remainsoptin; assetbatchUNRUN/editorclosurepending. Native importedshader/visuals/feet/audio/300mbounds/finalperformance/fullroster remainopen. StandaloneGamebuildrunning, then immutablepartialrecoveryexport.


### Wildlife partial recovery,16:09EDT
StandaloneGame227-actionbuild/signPASS119.88s(codex_wildlife_game_sign_retry.log) afterclearingonlyregeneratedappRootFinderInfo duringnormalbuild. Source/Content/securitysettingsunchanged. StrictcodesignverifyPASSwithsameguard; directverifyfailedafteriCloudreattachedmetadata. See phase3/game_build_receipt.json; no standalonegameplay/cook acceptance. Latesteditorbuild225/136.27sPASS. All13protectedsource/map and64read-onlydeerassets SHA-verified, otherSourcebaselineoutside5allowedfilesunchanged;6Pythonscripts parse; no game/build remains. OwnerEditor38368stillopen, assetclosurepromptpending, assetbatchUNRUN. Partialrecoverytarget Saved/CalderWork/codex_checkpoint_deer_logic_20261017_160949 prepared; requirepassedrecoveryreceipt. No deer defaultpromotion or nativeart/feet/audio/visibleMP/bounds/performance/fullroster acceptance.

### Wildlife asset batch resumed, evening EDT
Owner approval received. Interactive Unreal Editor verified closed before mutation; only UnrealEditorServices helper remains. First approved deer batch running in codex_deer_assets_batch_v1.log; no installation receipt or native rendering acceptance yet.

### Deer asset import passed, 20:17 EDT
Asset batch v4 PASS:3LODs/5UVchannels,16rigidcontrols/current+previous384floats,4ownedmaterials andbody/sourceLODs;64Fabsourceassets SHA-verified unchanged. Failed v1-v3 preserved (UE property/input API fixes). Final shader stats skin/fur249pixelinstructions/3samples,part186/3,contact168/1. Working native graph/component review underway. Native rendered poses/feet/visibleMP/sound/bounds/performance remain pending; no default promotion.

### First native deer art review exposed GPU data overflow
Native component review PASS in codex_deer_native_paths_v1. Detailv1 retained FAIL:4capturedimages all inspected, skin stretched; later nominalcamera has no physicalground. Root cause:UE GPU Scene customfloatcount is8bits(max255), original384overflow. Current225packedpose and ground+170clear-view search awaitbuild/assetregen/rerender. No art acceptance. Original importedgeometrybounds/UV/source64SHA checks didpass.

### Packed deer native detail passed; rendered multiplayer timing unresolved
225-action/140.40s packed-pose build PASS; batchv6 PASS225customfloats/3LODs/5UVs/64FabSHAs. Detailv2 PASS9captures all inspected, no stretched skin, actual imported sole4096samples gap2.553cm/drift0.000cm p95, diagnostictick.2997ms. Some foliage occlusion remains in woods views; no lighting or vegetation changed. NativeMPv1 matchingterrain/habitat/state and actual visiblefleeimage inspected, butFAIL screenshotcompletion1.5801s vs0.5s. OriginalFAILretained. New instrumentation keeps0.5s and records a GPU fence after a subsequent full view frame; PNG readback/encoding is recorded separately. No assertion that originalcapturetime was visibility time. Build codex_deer_native_review_build.log also adds8actual motionframes per ordinary-inputcase, renderer-only300m setup onexistingvalidatedhabitatnodes, Cycles64p50/p95perview, and idle-onlysole diagnostic10Hz(movingplantsstill everyfullposeframe). Reruns pending.

### Native multiplayer and300m renderer proof passed
225-action/139.49s reviewbuild PASS. Boundsv1 PASS samebodyinstance32842.909cm betweenexistingvalidatedhabitatnodes, r.InstanceCulling.OcclusionCull=1, actualbounds/visibility/renderstamp and2inspectedcaptures. Renderer-only explicitsetup teleports, no ecologyroute claim; diagnosticdrift4743cmfromsetupanchorreset is notfootacceptance (resetadded innewsource). Cycles64perviewsourcep50.0393/p95.1221ms,destination.0243/.0581ms. Native real-lobby MPv2 PASS bothpeers, matchinghashes/states, data30.26ms andend-of-next-viewGPUfencecompletion66.14ms afterauthoritychange(<500ms). Capturecompleted1592.54ms andvisuallyinspected; PNGsavingtimingseparate, originalfailedv1retained. OriginalcampusMP SHAunchanged andhillroutepending. Newaudioimplementationbuildv1 underway:4pooledspatialoneshots+night/nearestcreekloops,worker-onlydeerblow/leafcrunch/nightbed/creek/lonepeeperPCM,sharedattenuation/occlusion,deadlines/onecallperspecies,dualindoorzero ducking,protectedlawnRMSbound,ownsubmixrecording. Onlyoriginal-sourceaddition isconditionalcampuscricketsreplacement, authorizedbyMaster andrestoredwithNoWildlifeSound. No soundacceptanceyet; no broadroster/defaultpromotion.

### Native deer full run machinePASS; capture and indoor-pool corrections
Native fullv1 finished335s:4cases/12thresholds/15sstillness/142.01srealreturn/homefeeding/53captures. Imported sole4096samples gap2.504cm/drift0.000cm p95; selftick.0827msincludingaudio. All53images reviewed in6nativegrids; first motion/reactioncamera transition is invalid (playerhead/initialcamera frame), latergaits valid. Camera now waits for reactionPNG completion, sets legitimateEye thenwaits.6s before8posecaptures; buildv2 pending. Audiofullv1 produced fivePCM/WildlifeMix120s and6FFmpegspectrograms; NumPyactualPCM no clipping/night60chirps, mixRMS-57.51dBFS. LocationSoundv1 retainedFAIL afterlawnrecording: strictindoorchecks all6volume fields, inactivevoicesdefault1.0. Addedinitial/inactivevolume0 instead ofrelaxing test; no actualindoorwaveformacceptance yet. NoWildlifeSound nativev1 PASS0wildlifeaudio/originalcampuscricketcomponentplaying; woodsowlunchanged. Buildsoundreviewv1 PASS227/145.24s; sourcecamera+inactivepool fixesnowbuildingv2. No defaultpromotion/fullroster/globalacceptance.

### Audio scope and statistics corrections; prior mix-proof claims superseded
Locationsoundv2 machinechecks PASS4captures/all6volume0indoors, butactualWAVanalysis FAIL (insidehouse-48.24dBFS,campus-42.94; nearcreek-82.84). Primaryenginecode shows FMixerDevice recording silently falls back to master iftargetunregistered. OurtransientNewObjectSubmix had noPostLoad registration. Newsource explicitlyregisters/unregisters onworlddevice andguards liveinstancebeforeanyrecording; logs verifiedsubmixid. **Prior mixed WAVs, including fullv1WildlifeMix, have invalidwildlife-onlyscope and cannot acceptwildlifeaudio/indoor/lawnproof.** FivegeneratedsourcePCMs/noclip/exact60chirps remainvalid. analyze_wildlife_audio nowrequires registrationlog forlocationacceptance. Also corrected boundedstatistics:RemoveAtSwap(0) retainedinitial4094samples; replacewithrealringcursor andretainseparateplantedsolewindowspermood, maxp95acrossmoodssoidlecannotwashoutflee. Returnwait/walk timinglabelsseparate. Oldnativebehavior/range/realreturn/model/GPUfenceevidence retained; fullnewnativefoot/stat/capture/audio rerunneeded. Currentbuild codex_deer_audio_scope_stats_build.log; no game. No newthresholds/oldharnesschanges/defaultpromotion.


### Current deer native/audio checks passed; isolated timing active, 2026-10-17 evening EDT

This update supersedes the older running/pending notes above. Native fullv2 passes all four ordinary-input cases,12 thresholds,15s stillness and real142.08s recovery then walking home and feeding. All53 native images reviewed. Worst-mood imported sole gap2.553cm/drift0.000cm, with544/369 flee samples after the true-ring/per-mood statistics correction. Registered wildlife-only fullv2 mix/PCM/spectrograms and location soundv3 pass objective checks: exact silence in house/library, lawn-71.75dBFS, creek-44.70dBFS, no clipping, exact60 snowy chirps/minute. Earlier unregistered-submix mixed recordings are invalid wildlife-only evidence; keep the failures.

Native normal-lobby multiplayer v3 passes with audio: matching hashes/states, data44.95ms and GPU-completed following view109.81ms below unchanged500ms gate; PNG completion1760.47ms is reported separately. Actual same-instance328m bounds proof and two images passed with instance occlusion enabled. NoWildlifeSound nativev1 restores original campus cricket and creates no wildlife audio; woods owl unchanged.

Fullv2 close-up08 p95=.3021ms exceeded the.3ms limit. Exact coincident sole-point dedup retains every physical point and unchanged moving sample frequency. Fresh `codex_deer_current_walk_v1` PASS:20 inspected images, correct first detail camera,3 real walk thresholds, gap2.553cm/drift0,801/709 samples; maximum diagnostic view p95=.2288ms. The review checks actual cached camera POV before requesting close-ups. Its first compile pointer-deduction error was fixed; editor rebuild v2 PASS227actions/137.30s. Current standalone build PASS229actions/128.46s; strict deep signature verified with generated-app FinderInfo guard. These are compilation/signing checks, not a standalone gameplay claim.

CSV smoke validates299 actual wildlife self-timing samples per view. `Tools/calder/perf_wildlife.py codex_deer_perf_v1` is running three alternating pairs/21 original and woods/interior views,300 frames/4s warmup,Scene300/all interiors/sound,uncapped MetalOffscreenOnly/instance occlusion. Source/Config are frozen and the editor is closed. No final timing acceptance yet. Thirteen protected files/map and64 donor assets remain SHA-identical; only six authorized existing-source edits since wildlife start. Latest immutable fallback remains the16:09 deer-logic checkpoint; imported evening assets/audio/evidence still need a fresh verified recovery archive. No map saves/commits/default promotion; whole roster/master/global hill-route and final load/review chain remain unfinished.


### Isolated deer timings and compatibility passed; medium-foot diagnostic next

All six21-view runs completed: three alternating pairs, original300samples/4s warmup, pinnedScene300/all interiors/sound/normal occlusion, editor closed, Source/Config SHA frozen. Maximum actual wildlife tick p95=.128ms; all views/rounds below.3ms; listed p8p frame budgets pass. TierA deltas+.12/+.26/+.22/+.29/+.11ms are within±.5noise. Peak total sources14, registered submix and originalCRC/lights confirmed. `codex_deer_current_compat.json` passes all six original layout/surroundings/woods/parking reviews and native optional farm graph/components. Current source/13protected files/map/64donor uassets/original multiplayer harness hashes reverified unchanged within approved scope.

Native medium-distance hoof coverage remains open: full-pose native cases pass, but analytic15Hz interpolation across stance boundaries suggests4.045cmgap/7.424cmdrift. This is not native acceptance or a confirmed runtime measurement. Native sole-reader candidate and analytic receipt are staged under Saved/CalderWork/deer_medium_probe; runtime source is unchanged. Verify this concern in Unreal before default activation and fix only on evidence. Save fresh immutable recovery to `Saved/CalderWork/codex_checkpoint_deer_native_20261017_evening_v1`, requiring passed receipt. Current fallback remains16:09logic until archive verifies. Full roster/master/global hill-route/load/subjective audio remain pending; no commits/map saves/default promotion.


### Medium-distance native issue confirmed; repair and camera coverage in progress

Native SoleReview v1 measured actual submitted rigid sole transforms against current stance and independent SurfaceZ. All three variants exceeded3cm gap/drift (3.409/4.213/4.852cm gaps;7.689/6.651/13.878cm drifts). Coverage remained incomplete when its physical camera could not find a view. The boundary-aware sole fix retains15Hz body/limb keys and caches world stance anchors; v2 partial measurements give gaps.766/.543/.326cm and zero drift, but still fails camera coverage, so this is not acceptance. Root now reports actual SurfaceZ (as drawing already did), avoiding graph-height chords pointing below curved terrain; physical eye remains ground+170cm and all thresholds remain unchanged. Editor surface-root build running; no asset changes. Next nativev3 coverage/images, current full native/MP/standalone/unpinned/performance and fresh recovery. Earlier six-run timing is valid pre-medium evidence, not final current-source acceptance. Verified pre-medium recovery:220text/85binary/2967evidence, passed receipt. No default promotion/master completion.


Camera/root correction to the previous note: v3 still lacks coverage, though sampled feet show gap<.8cm/drift0. Its measured route/SurfaceZ difference is0; Paths::Evaluate already applies the surface, so the extra Root correction was redundant and is reverted. Do not treat it as a confirmed cause. New debug build logs exact floor misses/mismatches/slopes and closest blocking component for camera failure. Original Root and full-pose evaluation remain intact. Current contact fix still needs complete variant/capture coverage and final current-source tests.


### Native medium contact and corrected solid-prop routes passed

Exact blocker diagnostic found `ExteriorWoodsRock1` at the animal target. The authored graph had excluded standing trunks but missed collidable rocks/deadwood. It now reads analytic collision bounds for142solid woods props, without moving/editing any protected geometry or querying physics for paths. Current native graph1980/1980,trunks649,habitat5c0dee53; terrain72564759 and original treeCRC unchanged. Boundary-aware sole anchors preserve15Hz keys. Native mediumv5 PASS925frames,8physical170cm-eye captures inspected (some foliage occlusion), all per-variant coverage gates met: adult1545/1333samples gap1.8748/drift0; buck259/210 gap1.9308/drift0; young565/440 gap.6093/drift0. Explicit valid-graph gait setup is separate from ordinary-input reaction proof. Editor solid-props build PASS228actions/139.75s. Full current nativev3 now runs all4ordinary approaches/real recovery/53captures/SoundDump/120srecord. NativeMP/standalone/unpinned/current timings and fresh recovery still pending; no default activation/full-roster claim.


### Current deer verification complete except final timing/default package

Current standalone compile/sign PASS230actions/127.43s. Native fullv3 PASS313s:4real-input cases/12thresholds/15sstillness/142.09sreturn/homefeeding/53inspected frames, full-pose gap2.181/drift0 and current registered120s audio analysis passed. Native reduced-posev5 PASS925frames/all3variants/worstgap1.9308/drift0,8frames inspected with some foliage occlusion. Normal-lobby MPv4 PASS22.95msdata/58.31msGPUframe/1596.70msPNG,matching current5c0dee53habitat and inspected client image. Boundsv2 samebody328.42909m/two images pass. Unpinnedv1 all10walked and fed naturally without setup,three views inspected; this is not a full30min seam test. Corrected compatibility passes six original reviews and optional-farm1949/1949/habitat118c027b. Source/Config are now frozen for six21-view final timing runs `codex_deer_perf_v2`; first two on passes maxp95.1191/.1213ms, all listed frame views inside limits. Final medians/budgets pending. No C++/assets/heavy second job during the batch. Protected hashes/donor assets unchanged. Fresh current recovery/default activation still pending; whole roster/master remain unfinished.


### Corrected timing complete; paired analysis and default gate build

All6corrected21-view runs completed with Source/Config frozen. Maximum tickp95.1871ms,all individual views/rounds under.3;all listed absolute p8p frame budgets pass. The original difference-of-separate-medians interior check flagged faster wildlife-on times(-.93..-1.40ms); report retained. Raw sequence shows a timing shift duringpair2. All10matched interior images were inspected and geometry/viewpoints match. Using medians of all three signed alternating-pair differences, with no excluded runs or changed±.5ms limit, gives LIB-.13/HAR-.28/SCI-.26/UNI-.25/STA-.37ms. Paired audit saved separately and passes; do not label the original unpaired gate passed or discard it.

Exact one-file default-enable gate/message patch applied only after that analysis. Simulation/rendering/audio/state logic unchanged, backup/patch/SHA under Saved/CalderWork/deer_default_activation. Default editor build running; current standalone/sign and native default-on/off/NoTerrain (without any wildlife enabling flag) remain before fresh immutable recovery. Default source enabled is not yet runtime verified; full roster/master still unfinished.


### Deer normal default verified; current recovery package

Default editor build PASS228actions/139.12s; standalone PASS230actions/122.31s. Strict deep signaturev2 passes with generated-app root FinderInfo removed before and during verification; first metadata-race failure retained, no source/security change. Native default run codex_deer_default_native_v1 passes normal on/off/NoTerrain WITHOUT enabling Trial/Review flags, all nine original woods captures inspected. Ten deer/current graph/registered sound appear normally; both disabled cases have no wildlife draws/roster/sound. Original treeCRC0f3b2bb7 remains. Native game proof is not packaged standalone gameplay.

Source-scope refresh passes13protected/map/64donor hashes,six allowed existing-source edits and original campus_mp.py. Timing-scope/audio audit proves only exact default gate/message delta after final six runs; all85owned binary assets unchanged. Peak total audio sources14. Current full/native medium/ordinary life/recovery/MP/bounds/compatibility evidence is linked in schema2 deer_checkpoint.json. Old timing/contact/audio failures remain preserved. Current immutable recovery target codex_checkpoint_deer_corrected_20261017_v2 requires passed recovery_receipt; prior snapshots untouched. Next Phase3b geese/crows/bats, then remaining original roster/sound/final gates. WholePhase3/master remain incomplete.


Current immutable deer recovery VERIFIED: codex_checkpoint_deer_corrected_20261017_v2,224text/85ownedbinary/4552evidence,actualgitapply/exactSHA. Prior checkpoints untouched. Phase3b begins with the acquired crow export converted offline by Tools/calder/build_crow_static.py. This is asset preparation; no new bird runtime/import/visual acceptance yet.


### Phase3b static birds prepared; native art build

Crow offlinev1 conversion preserved; v2 reserves jaw control14. Crow editor batchv1 stopped before mutation because its expected seven logical deer assets omitted three retained physical iCloud conflict copies. Guard now matches the verified recovery manifest exactly. Batchv2 PASS26donor/10existing physical files unchanged;3LODs/5UV/225floats; owned shader244instructions/3samples. Shared original code-painted bird atlas materialv1 compiled, then v2 adds225-float current/previous rigid poses, normal rotation and PreviousFrameSwitch without touching deer/crow materials. No reference pixels.

New FHallMesh goose and brown/red bat static3LOD/control geometry plus explicit native16fixed-pose/170cm-eye art review are being compiled with -DisableUnity. Default play does not create these diagnostic components; tower crows unchanged. These are model candidates, not accepted ambient flight/populations. See phase3/sky/PLAN.md. No commits/mapsave/protected-production changes.


Bird art editorv1 PASS230actions/142.73s. Nativev1 found generated goose UV channel guard failure before screenshots; stopped only task-owned PID, preserved log/manual_failure and source under Saved/CalderWork/bird_art_uv_v1. Engine StaticMeshAttributes::Register derives vertex-instance UV channels from mesh UV-element count; setting only the attribute to5 left elements at1 during editor commit. Candidatev2 synchronizes SetNumUVChannels(5), skips unnecessary runtime editor commit,sets per-LOD fullprecision UV/highprecision tangent flags. Explicit review failure is now recorded/self-exits once instead of retrying the build every frame. Editor -DisableUnity v2 is running. No art/flight acceptance from compile or offline preview; no default deer changes.


Bird art editorv2 PASS230actions/146.48s. Nativev2 self-exits clearly: actual generated mesh UV count8 versus the diagnostic's exact5 check. Correction: UV bookkeeping inconsistency was not established as the failure cause. Engine StaticMeshVertexBuffer.cpp:113 overload ignores requested UV count when it constructs a full FStaticMeshBuildVertex view (8channels). Diagnosticv3 now requires the five used channels to exist,records actual count and retains per-vertex4weight normalization/joint-range checks for everyLOD. No master review threshold changed. FullprecisionUV/tangent flags and consistentUV-element bookkeeping retained. Buildv3 is running; first two failed native runs/sources retained. Cornell goose flight/resting and FWC brown-bat flight/resting plus NPS red-bat photographs inspected in hidden browser,closed afterward;no pixels downloaded/imported.


Bird editorv3 PASS230actions/141.30s. Nativeartv3 machine PASS88s: all4models/all3LODs/packed-weight checks,16captures,actual generatedUV8/importedcrowUV5 with usedUV5. Goose9792/5120/1968tris,crow2208/1434/706,brown/redbats5312/2632/1088. Models build37.17ms. All16images individually inspected and VISUAL ACCEPTANCE REJECTED: rounded goose surfaces have pole-cap holes,bat inner membranes are narrow/disconnected and ears oversized,crow bill/leg tuck plus upper-wing crop need work; backlit camera obscures colour. Nativev3 report records machine pass separately from visual failure. No ambient flight/default acceptance.

Candidatev4 rewinds each actual triangle from its area/vertex normals (QuadUV first triangles degenerate at ellipsoid poles),uses analytic smooth ellipsoid/tube normals,adds bat inner membrane panels/blended torso-shoulder-elbow-wrist weights,reduces ears/adds red shoulder markings,and adjusts crow bill closure/leg tuck. Photo diagnostic poses turn180degrees toward existing light,remain physical170cm eyes,and allow full crow wings in frame; global lights/fog/sky unchanged. Pose secondary frequencies now divide the1800second cycle exactly. Editor -DisableUnity v4 running; all previous sources/runs retained.


Bird editorv4 failed -Wshadow: membrane local From shadowed Describe's input parameter. Waited for the build to exit,then renamed only FromJoint/ToJoint. Editorv5 PASS230actions/138.82s. Nativeartv4 now captures the revised models and lit-side views,Source/Config frozen. No native art acceptance yet; no default birds/flight behavior.


Nativeartv4 PASS93s/16captures;all individually inspected. Pole holes closed,analytic smooth normals retained,all crow wings in frame and legs tucked. Crow jaw still over-opens: read-only Blender probe confirms original lower jaw tip(17.8026,0,19.8253) versus fixed bill tip(19.4524,0,23.377),hinge(12.8733,0,23.7841). Candidate closes lower jaw at-.61rad; separate0..1mouth parameter supports later caws/honks,last review pose explicitly opens the bills. Bat rear membrane anchor moved inside torso; goose neck/coverts thinned. Editorv6 PASS230actions/139.18s; nativeartv5 running with Source/Config frozen. Procedural goose/bat macro detail remains simple and no photographic master/ambient-flight acceptance is claimed.

Owner clarification requested: original14-40overhead geese versus6-12pond residents. Offered6-12peeloff/otherscontinue (recommended) or allow wholeflock/largerpondgroup. Pending; no dependent landing implementation/default behavior has been assumed. Continue independent model,flight and other-species work. No source asset purchases/downloads/map saves/commits.


Nativeartv5 PASS99s/16captures;all individually inspected. Closed/open crow/goose bills now distinguish correctly,rounded caps remain closed,smooth surfaces retained,bat rear membrane reaches torso and full crow wings stay in frame. Model rendering is ready for opt-in flight integration; procedural goose/bat macro detail remains simple and no photographic master acceptance is claimed. Actual models build40.38ms. Standalone game build running; signature/default on-off/no-terrain/no-art-leak checks and current immutable model recovery remain. Scope guard passes13protected/90donor hashes/sixallowed existingSource edits plus Config/master/canonicalMP unchanged. Shrunk:none; full master species/calls/reviews remain binding. No ambient/default birds yet.


Bird-model standalone build PASS232actions/126.58s; strict deep signaturePASSwith generated-app root FinderInfo guard,no packaged gameplay claim. Native default sanity codex_bird_models_default_v1 PASSon/off/NoTerrain,all9originalwoods captures inspected,Source/Config frozen,no CALDER_BIRD_ART setup in any ordinary launch. Original treeCRC0f3b2bb7 retained. Current model snapshot ready for export/check/immutable archive at codex_checkpoint_bird_models_20261018_v1; require passed receipt. Data-only flight candidates under Saved are uncompiled/unvalidated,not integrated. Important discovery for future bats/moths: mercury heads are emissive,no actual light components; use HouseCampusLayout::LampPoints and ground+520cm,not UPointLight enumeration. No default birds or landing behavior. Full master roster/photographic/audio/final validation remains.


### 2026-10-18 EDT — bird model recovery verified; sky graph prototype

Immutable `codex_checkpoint_bird_models_20261018_v1` recovery PASS:244text/97ownedbinary/4741evidence. New sky flight Source is data/review only and postdates that checkpoint. First editor compile failed one incompatible auto-pointer deduction from TObjectPtr; corrected to explicit UStaticMesh pointer and added bounded reject locations before v2 rebuild. Native path clearance/load cost, runtime flight, sounds, landing and photographic acceptance remain pending. No ambient birds enabled.


### Sky graph data v1 native failure retained; v3 compile

Editor v2 PASS232actions/140.88s. Nonrendered native `codex_sky_paths_data_v1` FAIL445792/446552clear,317.15ms setup,terrain72564759. First rejects are goose members over a distant ridge above150m; no clearance requirement was relaxed. New source fits one V plane within every member's terrain-derived60–150m interval (with2m internal margins), includes wing-radius offsets and reparameterizes its arc. Continuous solid swept segments and the full analytic bat trigonometric envelope supplement center samples. Per-species reject counts added. v3 editor compile in progress; no ambient/default birds or path acceptance.


### Sky graph native data v2 PASS; opt-in runtime begins

Editor v3 PASS232/144.18s. Native nonrendered `codex_sky_paths_data_v2` PASS779920/779920checks with1408solidboxes,continuous solid sweeps/full bat envelopes,1704.90ms setup,hash350e18eb,terrain72564759. New opt-in runtime Source now compiles in `codex_sky_runtime_editor_build_v1`:four local ordinary ISMs on the existing director,225cur/prev rigid-control floats,400m/view culling,5s component visibility hysteresis,frozen-clock Prev=Cur stop and2Hz bounds. A spatial solid index avoids scanning distant obstacles without relaxing the checks. Bat loop speed increased with identical envelope limits. Native data and motion captures must validate this later source. Review clock is explicitly separate from normal shared-clock/two-peer proof. Landing/calls/roost finish/photo/performance remain pending; no default sky birds.


### Native sky flight v1 rejected; bounds and review-camera fixes

Native `codex_sky_flight_native_v1` FAIL after3captures/73matching visible roots:zero root error,130958cm travel from entry,but a root escaped cached bounds. All3frames inspected and visually rejected:Ashby Hall blocks most of the V. Owned mesh extension setters do not calculate extended bounds (confirmed UE5.8 StaticMesh.h); now explicitly CalculateExtendedBounds after12m margins. Review-only clock used wall time through capture stalls; it now advances server game time like production. Eye selection now analytically clears FBlock/DZ boxes and actual physics for all V/stream members at0/3.5/7s,also covering appearance-only facades. No protected buildings moved. Current editor v2 rebuild; no flight acceptance or default promotion.


### Native sky flight v2 integration PASS,all18frames inspected

Editor v2 PASS234/147.10s. `codex_sky_flight_native_v2` native PASS86s,4880visible actual-ISM/root/bounds checks,zero root error,133705.77cm travel from entry,18captures individually inspected. Both complete goose Vs read well against dusk; crow first2frames show the stream,third is naturally foliage-occluded. Pond bats small/dark; glen and mercury-lamp bat silhouettes show changing wing phases. Model macro/photo detail still provisional. All6diagnostic Cycles64 view p95s<=.0534ms,not the required full paired benchmark. Data v3 native remains779920/779920,241.56ms,hash74bb095c. Flight is opt-in only;normal shared-clock/two-peer/landing/calls/full performance/photographic/master acceptance remain open.

Owner chose6–12peeling off to land while the rest continue;currentfixedseedchooses8. Landing plan draft under Saved/CalderWork/sky_runtime_preparation/landing_handoff_plan.md proposes visible pair departures/arrivals retaining6–12pond residents. No landing runtime yet. Next preserve this validated flight slice with normal on/off/noTerrain,standalone compile/sign and immutable recovery before new pond state/pose integration.


### Current-source default restoration PASS,standalone build next

`codex_sky_flight_default_v1` normal/on-off/NoTerrain PASS3sequential cases,215s,all9original woods images individually inspected. No enabling flag was passed and no CALDER_SKY_/BIRD_ART markers appeared. Original treeCRC0f3b2bb7 remains. `codex_sky_flight_game_build_v1` standalone compile/sign now running;do not editSource until it exits. Fresh immutable target is codex_checkpoint_sky_flight_20261018_v1,not usable until passed recovery receipt. Incoming8/departure/shore/state/pose/audio work remains draft/pending.


### Sky-flight standalone compile/sign PASS; immutable recovery in progress

Standalone `codex_sky_flight_game_build_v1` PASS236actions/130.26s;strict signaturev1 PASS. This confirms compilation/signing,not standalone gameplay. Protected13sources/map,64deer/26crowdonors,Config/master/canonicalMP and16ownedphysicalWildlife assets all pass the scope audit. No editor/game/build now. Freeze Source/Tools/SourceArt during export/check/archive at codex_checkpoint_sky_flight_20261018_v1 via sky_flight_recovery_plan.json;only its passed recovery receipt confirms the new recovery. Prior model/deer recoveries remain immutable. Draft goose landing/departure code is Saved-only/uncompiled/unvalidated and has not affected normal or trial flight.


### Sky-flight immutable recovery verified; goose landing data compiles

`codex_checkpoint_sky_flight_20261018_v1` PASS252text/97ownedbinary/4875evidence with actual git-apply/exact hashes;prior model/deer recoveries retained. Post-checkpoint new GoosePaths data computes8departing-from-V approaches and8resident departure curves,touchpoints on actual Gander Pond,matching split roots/source speed and a2.5m/s touch speed via monotone arc-space Hermite. Four landing pairs stagger4s;later water-braking/state/pose/visible population turnover is still required. Optional native sky paths review writes both curve and motion CSVs and checks split/speeds/analytic solid+terrain/water/deck clearance. Editor buildv1 running;data uncompiled/unvalidated and no new live pond behavior. Owner count choice is resolved.


### Landing/departure native data PASS; goose pose controls compile

EditorGoosePathsv1 PASS235/145.32s. Nonrendered `codex_goose_paths_data_v1` PASS25741checks/zero rejects;8approaches+8departures,same source root and starting velocity,2.5m/s touch speed,splitScene1065.571083/zero split error. Hash78bf2036,terrain72564759,29.16ms data build/296.58ms full sky+landing review. Native CSVs retain all path/motion positions. This accepts data only,not live population/server/landing/feet/render. Current new pose controls add optional flap/fold/neck parameters to existing16rig/225floats;normal flight defaults unchanged. Explicit8pose review prepares flight,glide,flare,folding,raised/lower neck,hiss and reverse view. Editorposev1 compiling.


### Goose data v2 PASS; pose v2 still rejected; full cohort data compile

Poseeditorv3 PASS235/143.36s. Native landing-datav2 PASS25741checks/hashfd371568/zero split error,297.53ms fullreview. Reversing touchdown pairs south-first fixes a design-predicted water-braking convergence:minimum66.56cm(v1FAIL)→356.17cm(v2PASS),threshold120cm unchanged;proposed3s brake uses250→35cm/s and is not live behavior proof. Native posev2 machinePASS68s/8individuallyinspected,but folded feather planes still project too broadly. Current source shortens secondary tips and tightens stow targets;new pose review required.

Pure2cohort cycle data now adds eastern pond holding positions,3s braking,35cm/s swimming home,accelerated resident departures and a3600s native population/position-separation/1800s-seam audit. Newhome/departure/water clearance must validate. No server/PondStates/actual landing behavior yet. Do not mistake data or fixed-pose acceptance for full species completion.


## Owner free-Fab steering and opt-in pond work,2026-10-18
Owner authorizes any free Fab asset and requests use of existing downloads. Confirmed acquired AnimalVarietyPack deer/crow donors already used; textured fox export available. No suitable free goose/bat replacement found in targeted checks so far; see phase3/fab_wildlife_inventory_20261018.json for listing URLs and rejected candidates. No new acquisition,account sign-in,terms acceptance or charge.

Pure two-cohort goose cycle native audit PASS26364/26364,72000samples/3600s,population6–8,pairmin356.17cm,seam0/hashbf170b85. First opt-in pond integration editor PASS236/145.43s; original P7 pinned native machinePASS67s,oneframe inspected.16server roster entries share one director with10unchanged deer states; onepondISM. Eight of the second V peel off,22continue. This is background/pinned evidence only. Posev3 eightframes individually inspected; tighter stow still simple code-painted art. Current editor build codex_pond_sequence_editor_build_v1 compiles smooth water turns,lower idle head/tail speed,visible/update accounting,actualRoot readback and explicit native live-sequence review. No new default promotion. Moving behavior,two-peer,shore reactions,wakes,calls,full timing/roster remain. Latest flight immutable checkpoint predates these changes.

Native pond sequencev1 rejected after98s:35945actual-instance samples had zero positional error; newly visible incoming member9 fell outside cached bounds at raw1072.797414. Both produced frames inspected; no landing occurred before stop. Bounds fix seeds and retains hidden zero-scale approach anchors before400m activation,still2Hz updates and normal per-instance culling. Slow folded poses now use integer1800s periodic frequencies. A separate normal-lobby/two-native-peer background test is staged,checking10deer/16goose states,shared-clock roots within unchanged1.2m and capture timing separately. Editor buildv2 pending; no acceptance inferred.

Pond sequencev2 also rejected(first left-arm incoming visibility at raw1072.979204,X23869.54):start-point anchors did not cover the curve's westward bow.37138actual-root comparisons still exact,both frames inspected. Native data CSV confirms fullapproach X23423.67..32185.99. v3 uses opposite corners of the complete curved approach expanded20m,as zero-scale hidden anchors; no extra component or culling-distance change. Added detailed bounds diagnostics and host-native root checks to MP test. Editor buildv3 running. Earlier failed receipts/images retained.

Native pond sequencev3 PASS205s,192069actual ISM root samples/worst positional error0/18440frames,6–8pond population,phases1–5seen,50,125.39cm incoming travel from entry. All8originalP7 standing-eye images individually inspected: later landing/departure silhouettes,settled folded-wing heads and swimming visible. Full-approach hidden anchors resolve activation bounds failures while2Hz updates remain. These are background sequence/render checks with explicit diagnostic starting offset,not shore reactions/splashes/calls/normal30min/two-peer/performance/photographic/master acceptance. Normal-lobby native MP codex_pond_mp_native_v1 nowrunning; current receipt phase3/pond_checkpoint.json.

Pond MPv1 FAIL93.09s:both terrain/habitat/pondplan hashes match,16replicated geese coexistwith10deer,but client first incoming actual root error246.0634cm exceedsunchanged120cm gate. CSV host1072.96554 pluswall.00805 versusclient1072.810768 indicates.1628s GameState startup-average clock lag. No images produced beforefailure. Isolated candidate sends natural phase transitions only,with freshauthority StartTime; client anchorsambientcyclewithout changingoriginal deer reaction clock oraddingheartbeat. Stale idle timestamps rejected for latejoin. Clock editorv1PASS; small stale-guard/first-failure-preservation buildv2 running. NeedMPv2.

New owner request queued afteranimals:gradual dusk-to-night progression over time spent on map,same shared transition for multiplayer. This supersedes original frozen-dusk requirement for the later lighting stage; no sky changes now. Optional duration preference30/15/45min pending. Ledger phase3/after_wildlife_night_request.json.

Pond MPv2 PASS189.33s via real lobby:10deer/16replicated geese each peer,matchingterrain/habitat/pondplan.37927host actual-root samples/error0;10505client actual-root samples/worst same-wall error32.6595cm<unchanged120cm. Arrival/water/brake/swim phases covered;both native P7captures individually inspected. Source/Config frozen. Clock phase-event anchors fix priorstartup averaging;noidleheartbeat. Native sequencev3 image/path carryforward audit confirms DrawPond and full Birds/GoosePaths/Sky bytes unchanged; currenttwopeer directly checksnewclock. No claim of exactv3source revision/fullmaster/art/shore/audio/perf acceptance. Currentdefaulton/off/NoTerrain restoration run codex_pond_background_default_v1; standalone/recovery next.

Pond background current-source restorationPASS216.96s,on/off/NoTerrain3cases/all9originalwoods captures individuallyinspected,noSky/Pond/artmarkersinordinarylaunches. StandaloneDevelopmentcompilePASS240actions/134.44s;strictsignaturev1PASS,generatedapp-rootFinderInfo-onlyguard. Scopeguard13protected/map,64deer/26crow donor hashes,6existingSourceexceptions,Config/master/canonicalMPunchanged,16physicalWildlifeuassets. Nextfreeze/export/check/archive freshpond_background_v1target;no wholemaster/defaultbirds/gameplayclaim.

Shore dataeditorv1PASS239actions/146.45s. Nativedatav1PASS40s/2539nodes/31577samples/1593nativewaterworkstrianglebounds/0rejects/hashd966aba1. Routes18.367/26.219m to dryinletbanks25200,-10800 and24500,-10700. Nophysicsqueries,noreaction/render/footacceptance. Newgroundleg artcandidate retains16controls/225floats andisoptinonly. Groundeditorv1PASS;v2compiles actualuploaded-ISMsole decoder,drybankreviewphysicaleyes;next8nativefixedposes. Movingfeet/hiss/wakes/callsremain. Verifiedpondbackground268/97/5141predatesnewshore/modelwork.
