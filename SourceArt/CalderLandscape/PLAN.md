# Calder completion plan - Codex takeover, 2026-10-16

Continue the existing runtime-built Calder world from Claude's handoff. The authoritative specification is MASTER_PROMPT.md, including the owner's additions; CODEX_HANDOFF.md and STATUS.md provide the last reported checkpoint. Reported passes must be checked against runtime output. No commits or map saves.

## Protected scope

The fraternity house, its rooms, lot, driveway, court, backyard/rear courtyard, Fall Jam field and house parking lot remain byte-identical to the takeover checkpoint in their own source/assets, except the separately owner-approved one-call compile-only HouseDetailProps constructor guard (editor arguments/material choices/geometry unchanged). Frozen ground stays fixed. Woods appearance and pig animals may change as expressly authorized; woods terrain, tree transforms/checksum, trail, hut and pigsty room stay fixed. Keep 53 exterior lights, the six-light interior pool, existing player movement/checkpoints, and tree checksum 0f3b2bb7. Only one build/game test at a time, except the required paired multiplayer test. All game runs are offscreen and muted.

## Inherited terrain design

The existing design already has named benches, building pads, retaining walls, stairs, paths, water and outer mountains. Do not restart it. Its authoritative source and reports are:

- `../../Saved/CalderWork/plan/landform.py` and generated `Source/SeniorSendoff/HouseCalderLandformData.inl`.
- Site inventory: `../../Saved/CalderWork/plan/site.md`, `site.json`, `map_actors.json`.
- Building/road/grade checks: `../../Saved/CalderWork/plan/landform_report.md`, `landform_verify2.md`, `data_exports.md`.
- Integration evidence: `../../Saved/CalderWork/plan/integration_1de.md`, `integration_1de_verify.md`.

![Original site](../../Saved/CalderWork/plan/site_today.png)
![Contours and protected areas](../../Saved/CalderWork/plan/landform_contours.png)
![Long sections](../../Saved/CalderWork/plan/landform_sections.png)

## Work sequence

1. Preserve a separate takeover checkpoint of Source, Config, Tools, Calder docs, map and landform source. Rerun the unchanged two-process hill walk. Diagnose the original failure without changing movement limits, routes or review thresholds.
2. Finish terrain integration defects: correct water surface queries below the dam, verify water depths, address missing connections/outlets where they can meet the protected-area and grade constraints. Inspect P1-P8 and keep the same comparison eyes; improve Crow Ledge only through a real reachable ledge/route change, never by presenting a raised camera as a ground-level result.
3. Woods and pig appearance: inspect current runtime views and donor assets; use the free assets already acquired, preserving licences and material sources. Stage one editor-only asset batch outside the map. Ask the owner to close the editor only when the batch is ready. Retain rigid runtime animal parts and existing movement/interaction support.
4. Landscape: ground palette/cover, altitude-specific foliage, stone/farm detail, water and mist, then scenery-only mystery reserves. Every new landscape feature must have a working `-NoCalderLandscape` comparison; retain the terrain-off baseline.
5. Wildlife: start with deer and the replicated state director end-to-end, then the remaining core and full roster specified in MASTER_PROMPT. Validate habitat paths, feet, reactions, sound caps and two-process reaction synchronization before adding more species. No species substitutions presented as the requested animal.
6. Final performance and verification: all specified reviews and house baseline verdicts, DisableUnity build, three alternating timing rounds per switch, screenshot inspection, sound recordings/spectrograms, frozen-zone comparison, README/FILES and patch against the original snapshot. Log remaining failures honestly; never loosen a check to turn it green.

## Acceptance and budgets

Use the numeric thresholds and complete review chain in MASTER_PROMPT.md verbatim. Key budgets: collision within 3 cm at 20,000 points; frozen trace grid within 0.5 cm; walkable Z >= -699.2; zero server corrections during the paired hill walk; wildlife tick <= 0.3 ms p95; load <= baseline +3 seconds; 65 terrain tiles, 24 landscape ISMs and 56 wildlife ISMs (20 visible). P1-P8, macro close views and each species need actual runtime captures. No claim of photographic quality until compared with real subject references.

## Current checkpoint - 2026-10-16 18:04 EDT

The built terrain is `dc3c79a5`: Crow Ledge is actual +41.59 m relief with three timber flights and the same ground+170 cm P8 eye. The South Lot tier links, permit-lot approach, level walk00 approaches, short Music sill transition and Plant doorway datum are built. Full authority: 351 PASS/0 FAIL. Latest 198-action DisableUnity build: `codex_nosing_classification_build.log`, PASS in 122.62 s.

Current runtime terrain/layout pass: 20,000 collision samples, zero holes, max 0.03 cm; 4,967 layout floor samples and zero misses/obstructions. All 117 real first-riser checks pass at the original 14..18 cm/0.05 cm limits; all real tread goings are at least 30.16 cm. Eighteen independent profiles across six flights pass (21,942 samples), as do the four original access walking legs. The short paving-strip boundary uses the existing 5 cm significant-riser rule and is not counted as a stair nosing. No original route, movement setting, sample count or dimensional limit was relaxed.

All 14 comparison views and four access captures were inspected. Music's outdoor view is partly occluded by the near wall; its complete collision profiles pass. Original P3 framing remains, with an appended tower/stadium companion. P8 exposes some roofs but trees still cover others. Frozen-ground comparison: the same 685 rows (616 unique XY), 0.000 cm maximum difference from terrain-off. All 13 checked protected source/map files remain identical; the 319-file scope census retains 318 unchanged files and the authorized CalderFeatures changes.

The full original walking/surroundings/terrain-off/reach chain passed as `codex_entry_verify`: all99walking stops, five surroundings reviews, exact terrain-off counts,2,557current path stations,1,845drop seeds and zero traps. The older full walking/reach chain passed at `2749a6c7`. Rendered multiplayer still has an unresolved older failure (four corrections); the NullRHI diagnostic passed without waiving it. The latest hash still needs paired rendered play.

Remaining terrain work: reroute four proposed drainage outlets that currently face rising ground or building footprints; author the road-bridge/lane connection; finish required view coverage. Full Phase 2 appearance, all wildlife/sound, final whole-implementation timings and complete verification/recovery documentation remain. No commits or map saves.

## Current bridge checkpoint - 2026-10-16 21:36 EDT

Ground72564759, source authority3f43df9d38490ed3dd42b9133d7ce4bdd348818e9fb5e1eb61fdcd6ba17660de. The Ashby bridge, two approaches and turnaround pass8313paving/603deck/651underpass/78parapet checks and a1256-goal ordinary-input round trip in both directions. The fresh canonical bake matches compiled data byte-for-byte; all39original stair endpoint records are unchanged and source-locked. Frozen685rows match baseline exactly. Latest DisableUnity build123.78s.

Broader current-build checks already PASS: layout8493floor samples, terrain20k/0holes/.03cm/117first risers/301feet/zero gaps/CRC0f3b2bb7, all99original walking stops, five surroundings reviews, clock/chimes/crows and18/18campus smoke tests. Full interior walkthroughs, exact terrain-off, reach/no-traps, original access, refreshed captures and recovery refresh are still running/pending. Last rendered MP at the previous dc3c79a5 failed a GC-correlated walk correction; current72564759 has not yet had a paired run. No movement, clamps or review criteria changed.

Verified partial recovery: codex_checkpoint_bridge_walk_20261016_2118 (70test-applied text files,38verified binaries,15hash-verified evidence files; its two images are explicitly earlier WIP captures). Original1813stairs and compatibility/numeric gold remain. Current319-file census is309unchanged plus10documented authorized differences; all13explicit protected source/map files remain unchanged, with the separate owner-approved HouseDetailProps compile-only exception. No commits/map saves.

Seven automatically proposed roadside outfalls are rejected by current clearance/gravity bounds; their design remains open. Actual roadside construction is3lined ditches/8basins/1inlet headwall/2drop inlets/3gutter inlets and0open outlets, plus the waterworks' causeway pipe/headwalls. Do not force uphill outlets or build inside new lanes, buildings or frozen zones. Phase2/3 and final timings remain unfinished. The owned ground-material builder is prepared but unrun/unbound; inherited material report15samples needs fresh verification against the12sample requirement.

## Continuous-ground checkpoint and next cover stage

Owned12sample ground material/binding/masks pass on/off terrain/layout/frozen proofs and current16view inspection; no terrain or collision changes. Terrain-off original counts restored. Next add short lawn tufts and tawny field cover with own seeded placement,4mrejection buckets,<=120kinstances,<=60triangles/tuft,plainISM(noHISM),no shadows/DF/indirect/collision and25..45mculls. Reject frozen zones, paving/lots/stairs/water/buildings and protected sightline/camera areas. Existing tree transforms/CRC and rooms remain untouched. Preserve the current ground checkpoint before new source. Full Phase2/3/final performance and rendered MP failure remain open.


## Phase2c merged ridge canopy trial (2026-10-17)

Use an independent opt-in CalderRidgeForestTrial layer over the existing pure FarLand/SkylineGround authority. Do not modify terrain/ridge vertices, EX_Ridge sheets, original tree meshes/transforms, road/water data or collision. Preserve NoCalderLandscape/NoTerrain restoration; no draw build on dedicated servers.

Twelve merged tiles cover outer scenery X±320000/Y-320000..153600. New visible geometry starts beyond the walkable square: north43000..50000, south48000..60000 or sides52000..62000. Town/river remain open, far road spur ends before the north forest, and the water-tower clearing has an explicit1800cm taper. No new vertex is created for unused grid points in the central opening. Crown surface sits3..1453cm above the existing scenery surface, with independent2000/4000/8000cm grid LODs. Identity instance per tile, plain static ISM, no collision/overlaps/navigation/shadows/DF/indirect and no per-frame updates. Twelve scenery ISMs plusfourcover ISMs fit the24landscape budget. Long culling is for merged distant scenery, while existing individual-tree225m culls remain unchanged.

TownMainSt datum-1900 makes the250m line23100absolute. A±18m transition turns mid-slope scarlet/orange/gold/crimson/purplebronze into high russet/brown, with grey thin patches. Valley patches retain20percent green; north-facing concave coves carry dark evergreen streaks. New hash seeds are independent. Existing terrain/CRC/door/walk/light heights remain authoritative.

Owned M_CalderRidgeCanopy derives from read-only NaturePropTextured and approved CC0 forest-leaf albedo/normal maps. VertexRGB holds palette, vertexA edge coverage with temporal dither. Actual compile/sample counts must pass before use; no new lights.

Acceptance remains pending: DisableUnity build; actual component/LOD/vertex/settings/budget review, outside-world geometry and surface-offset checks; P1-P8 plusP2/P3/P5 colour-line and crest captures; ordinary original/native woods/frozen checks; three alternating frame-time rounds including TierAinsides and final all-switch chain. Trial stays offdefault until visual/performance acceptance.


## Phase2d owned rock bedding/weathering trial

Opt-in CalderRockStrataTrial under the existing landscape gate. Own opaque M_CalderStrataRock wraps the existing rock scan graph; original six material SHA guards retained. Tilted restrained82cm bedding/derivative normal only on rock_face_04 and lichen_rock; masonry keeps zero bedding and subtle weathering. Concrete, gravel, wood and rust retain original master. No geometry/authority/water/collision/ISM/light/fog edits. Actual shader370instructions/8samples. Native binding readback/original review chains and paired P1/P2/P6/P7 captures required; NoTerrain/NoLandscape must disable trial. Keep OFFdefault pending visual/performance acceptance. Wet bands, rock drill marks/seeps and other master details remain separate unfinished work.


## Farm persimmon trial

Add one source-reference-informed common persimmon at(-22700,-1550), under the new optional CalderFarmTreeTrial and existing farm/landscape gates. Keep the original five farm ISMs/24 total by merging the tree with the nearby fence cluster; only that mixed tree/fence cluster gains225m tree range. Use private code-painted bark/leaf/fruit maps, 4.1cm fruit, sparse autumn leaves and three geometry LODs. No downloaded photo pixels. Build the same exact visible main-stem collision on server/client, with no crown/branch/fruit collision, terrain grading, room, light, original-tree or route edits. Pure trunk footprint rejection preserves ground-cover access. Native96trunk rays, floor/cover/original reviews, new physical whole-tree/fruit views, dedicated-server and disable comparisons must pass before acceptance.


## Coherent barn merge before hedge detail

Optional CalderBarnMergeTrial combines only the co-located timber and roof draw sections. Geometry, normals, UVs, material identities and180/225m culls are unchanged; the distant fence stays separate. Existing five-component behavior remains available with the switch off. Free one component (farm5→4/landscape24→23) for later hedgerow plants while retaining the original24landscape limit. Actual attribute/position comparison, normal-culling farm/tree images and native regression gates must pass. Hedgerow material/planting and collision-preserving core treatment remain unimplemented at this step.


## First farm-fence hedge planting

Under CalderHedgerowTrial and existing farm/landscape gates, add one plain static ISM of mixed young cedar/rose/blackberry clumps beside the worm fence. Tested barn merge keeps landscape at24. Private seed2026101781, three LODs,70/90mculls, no collision/shadows/DF/indirect/navigation, rootZ/height custom data and shared existing gust material updates. Keep broad gate and full frozen/paving/water/stair/path/building exclusions. Original boundary boxes and collision stay unchanged; their replacement remains later work. New field/hip/scale cameras use actual floor+130cm and actual prototype points with triangle visibility rejection, cached once per world/view. Build plus native placement/settings/custom data/shared wind/budget/original terrain-water-woods tests and all images are required; source is currently untested and OFFdefault.
