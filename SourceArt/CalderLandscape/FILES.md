# Codex takeover file map

This lists Codex's changes relative to `codex_takeover_20261016_111615`. The larger inherited terrain implementation is described by the master prompt and Claude's original status/history.

| File | Purpose |
|---|---|
| `Source/SeniorSendoff/HouseDetailProps.cpp` | Explicitly owner-approved standalone compatibility: guard the editor-only third material-slot constructor name; editor arguments preserved. |
| `Source/SeniorSendoff/HouseCampusStaticMesh.cpp`, `HouseCalderOuterLand.cpp`, `HouseCalderTerrainTiles.cpp`, `HouseWoodsStructure.cpp` | Same standalone/editor material-slot constructor compatibility guard. |
| `Source/SeniorSendoff/HouseCalderFeatures.cpp` | Concrete approaches at the South Lot, permit lot and walk00; a short Music threshold; actual approach/doorway stair datums, with matching collision, surface queries, paving exclusions and floor samples. |
| `Source/SeniorSendoff/HouseCalderAccessReview.cpp` | Independent full-width curb/tread collision profiles and four actual uphill/downhill walking legs. |
| `Source/SeniorSendoff/HouseCalderRoads.h/.cpp` | Owned asphalt approaches and turnaround, spatial paving queries, limited recessed terrain joins and layout floor samples. |
| `Source/SeniorSendoff/HouseCalderRoadReview.cpp` | Independent8313paving/603deck/651underpass/78parapet checks and1256-point ordinary-input road/bridge round trip, all passing at72564759; full campus integration chain pending. |
| `Source/SeniorSendoff/HouseCalderLandform.h/.cpp` | Optional road geometry and road-bridge crossing data accessors. |
| `Source/SeniorSendoff/HouseCampusLayout.cpp`, `HouseCalderTerrain.cpp` | Connect owned road paving, exclusions, walking samples and variable finished-paving datums. |
| `Tools/calder/report_road_profiles.py` | Summarize actual collision CSV, hit components and measured grades without changing acceptance criteria. |
| `Tools/calder/verify_bridge_candidate.py` | Independent finished-road grades, full road/verge building clearance, unchanged stair endpoints, bed preservation and under-arch standing clearance. |
| `Source/SeniorSendoff/HouseCalderWater.cpp` | Correct downstream water queries; bind gated opaque falls/stream materials; add four local mist volumes outside frozen zones. Waterworks collision remains independent of appearance. |
| `Source/SeniorSendoff/HouseCalderTerrainReview.cpp` | Independent creek/glen queries, passive roof diagnostics, three-width first-riser gate, approach-footprint coverage and floor rays that ignore the test pawn; original limits retained. |
| `Saved/CalderWork/plan/landform.py` | Crow Ledge +41.59 m/three timber flights, two South Lot links, continuous landing grade breaks and actual stair paving/doorway datums. |
| `Tools/calder/bake_landform.py` | Canonical baker: continuous shared-height grade breaks are separate from physical wall records. |
| `Tools/calder/export_stair_surfaces.py` | Export all39 runtime foot/head paving classifications to `phase1/stair_surfaces.json`, with source SHA locks and fixed XY records. |
| `Source/SeniorSendoff/HouseCalderLandformData.inl` | Reproducible generated landform, collision, route and feature data. Never hand-edit. |
| `Source/SeniorSendoff/HouseExterior.cpp` | P8 ground+170 cm, interior comparison eye/target rebased together by BlockDZ, appended lot/stair and supplemental tower captures; house geometry unchanged. |
| `Source/SeniorSendoff/HouseCalderLandscape.h/.cpp` | Landscape gate, scanned floor/bark/leaf binding and two existing non-colliding fern/stump batch replacements. |
| `Source/SeniorSendoff/HouseWoodsFloor.cpp` | Invoke appearance-only floor/prop overrides without changing floor collision or placement. |
| `Source/SeniorSendoff/HouseWoodsStructure.cpp` | Invoke scanned look; improve added hardwood crown profile and foliage retention, preserving original placements and comparison. |
| `Source/SeniorSendoff/HouseRoamingPig.h/.cpp` | Continuous static skin, twelve rigid controls, native anatomy, support/sniff/contact poses, relaxed sleep and gated safe steering recovery. |
| `Source/SeniorSendoff/HouseCalderPigData.inl` | Generated native pivots, link lengths, rotations, soles and ear/snout reference data. |
| `Source/SeniorSendoff/HousePigSmoke.cpp` | Rebase removed upstairs animal-fixture coordinates onto existing current-room graph and observer floor. Physical limits/timeouts/counts unchanged. |
| `Source/SeniorSendoff/HouseCalderPigVisualReview.cpp` | Opt-in assembly/data checks and six unobstructed-camera captures; no animal teleport or locomotion acceptance inferred. |
| `Source/SeniorSendoff/HouseCampusLayoutReview.cpp` | Register the self-exiting pig and South Lot access diagnostics. |
| `Source/SeniorSendoff/HouseCampusInteriorsSmoke.cpp` | Register the self-exiting pig and South Lot access diagnostics. |
| `Source/SeniorSendoff/HouseCampusInteriorsReview.cpp` | Register the self-exiting pig and South Lot access diagnostics. |
| `Tools/calder/bake_landform.py` | Canonical baker including continuous grade breaks; these split triangles with shared heights and never add physical walls. Legacy Saved baker does not support them. |
| `Tools/calder/export_stair_surfaces.py` | Export all fixed stair endpoint paving types from the runtime source cadastre for the planning emulator. |
| `SourceArt/CalderLandscape/phase1/stair_surfaces.json` | 39 surveyed endpoint records, paving offsets and locked cadastre source hashes; not physical acceptance. |
| `Tools/calder/phase2_assets.py` | Read-only acquired animal export; build owned opaque falls/stream masters and compile/sample reports. |
| `Tools/calder/build_pig_skin.py` | Create the twelve-transform static-skin material, posed normals and native texture sampling. |
| `Tools/calder/import_pig_parts.py` | Import derived static meshes, materials, three LODs and numeric UV data; check bounds; disable Nanite/collision. |
| `Tools/calder/pig_skin_batch.py` | Run pig material/import operations with the ordinary editor closed. |
| `Tools/calder/prepare_woods_sprays.py` | Deterministic approved CC0 oak/maple branch atlas preparation and provenance. |
| `Tools/calder/import_woods_sprays.py` | Import four atlases with correct colour/normal settings and alpha coverage mips. |
| `Tools/calder/woods_assets_batch.py` | Woods atlas import and falls preparation without map writes. |
| `Tools/calder/falls_assets_batch.py` | Rebuild only the owned falls/stream masters. |
| `Tools/calder/inspect_woods_assets.py` | Read eight acquired tree mesh budgets without altering sources. |
| `Tools/calder/perf_checkpoint.sh` | Three alternating rendering comparisons with the original sample/warm-up sizes and five Tier A interior eyes. |
| `Tools/calder/report_checkpoint_perf.py` | Require three complete rounds and all 16 eyes before reporting comparison medians. |
| `Tools/calder/run_cooked_multiplayer.py` | Launch-only adapter for the original MP review against a staged native executable; records the two-line diff and hashes. |
| `Tools/calder/create_orphan_groom_cook_rule.py` | Create one editor-only NeverCook label for the proven unused, malformed Boywick binding; original assets read-only. |
| `Content/SeniorSendOff/Calder/Build/Calder_OrphanGroomCookRule.uasset` | Owned, nonrecursive packaging rule naming exactly one orphan asset; characters/maps unchanged. |
| `Tools/calder/export_checkpoint.py` | Export takeover-relative text patch and derived binary payload with SHA-256 manifests. |
| `Tools/calder/check_checkpoint.py` | Actually apply the checkpoint patch to snapshot copies and verify every resulting text and archived binary hash. |
| `SourceArt/CalderLandscape/phase2/build_pig_parts.py` | Offline Blender anatomy/UV conversion and contact-proof generation. |
| `SourceArt/CalderLandscape/phase2/inspect_animals.py` | Offline donor bone/mesh inventory. |
| `SourceArt/CalderLandscape/phase2/*.json` | Source anatomy, tree screening and actual material compile budgets. |
| `SourceArt/CalderLandscape/phase2/pig_parts/*` | Reproducible FBXs, native geometry/contact manifest and installed bounds/LODs. |
| `SourceArt/CalderLandscape/phase2/woods_sprays/*` | Four generated PNGs and CC0 source/import manifests. |
| `Content/SeniorSendOff/Calder/Pig/*` | Owned derived pig meshes and skin material; acquired originals remain read-only. |
| `Content/SeniorSendOff/Calder/Woods/*` | Owned imported oak/maple atlas textures. |
| `Content/SeniorSendOff/Calder/Materials/M_CalderFalls.uasset` | Owned vertical falling-water material. |
| `Content/SeniorSendOff/Calder/Materials/M_CalderStream.uasset` | Owned surface-water derivative; original master unchanged. |
| `SourceArt/CalderLandscape/PLAN.md` | Scope, sequence and acceptance plan. |
| `SourceArt/CalderLandscape/CODEX_HANDOFF.md`, `STATUS.md` | Active checkpoint, verified results and unresolved failures. |
| `SourceArt/CalderLandscape/README.md`, `phase2/README.md` | Architecture, state, source/licence distinctions and reproduction guidance. |

The owner explicitly approved a compile-only constructor guard in `HouseDetailProps.cpp`; its editor arguments, geometry and material choices are preserved. No other protected room/court/party/parking source file or map is part of this change set. `Saved/CalderWork` also holds diagnostic scripts, superseded failures, comparison captures and takeover snapshots; these are evidence rather than cooked content.

- Tools/calder/ground_assets_batch.py: owned continuous-ground material only, original-master SHA guards and12sample compile gate.
- Content/SeniorSendOff/Calder/Materials/M_CalderLandscapeGround.uasset: derived ground shader,12actual samples; saved by editor Python.
- HouseCalderTerrainTiles.cpp/.h: gated owned ground material, vertex rock/litter/clay/cavity masks, no collision-height edits; public loaded-material state for lawn handoff.
- HouseCampusPolishGround.cpp: legacy lawn overlays retained in all frozen/intersecting patches; other lawns yield to continuous ground while its material is active.

- Source/SeniorSendoff/HouseCalderCover.cpp/.h:4plainISM ground-cover batches,4mrejection grid/own streams, completed native collision/shoreline/footprint rejection, alpha-preserving seed-spray textures, single deferred build and shared-wind registration.
- Source/SeniorSendoff/HouseCalderLandscapeReview.cpp: opt-in independent actual-instance/settings/render-triangle/world-root/water/protected-bounds check. Explicit ground-cover checkpoint scope, not full landscape acceptance.
- Source/SeniorSendoff/HouseLeafWind.cpp/.h: additive public RegisterWindMaterial API for owned foliage; original gust/audio/RNG logic unchanged.
- Source/SeniorSendoff/HouseExterior.cpp: cover scheduling hook after real campus props are built; four appended macro cameras at actual ground+130cm; existing view XY/framing unchanged.
- Source/SeniorSendoff/HouseCampusLayoutReview.cpp, HouseCampusInteriorsSmoke.cpp, HouseCampusInteriorsReview.cpp: new self-exiting landscape review added to existing sharing lists, thresholds/counts unchanged.


- Source/SeniorSendoff/HouseCalderTreePreview.cpp: development-only, opt-in unbound tree previews; no default forest binding. Added to the three self-exit-sharing lists.
- Source/SeniorSendoff/HouseCalderTreeMeshes.cpp/.h: preview-only render-data copy preserving full-precision UVs, tangents and materials; intact lower trunk plus clustered upper wood; latest2/12/24cm revision built, autumn native visual check running.
- Tools/calder/tree_assets_batch.py: owned conventional field-form leaf and structural donors; all476acquired pack assets SHA-guarded, no map/source-asset writes.
- Tools/calder/inspect_tree_materials.py: read-only actual Metal shader instruction/sample inventory.
- Tools/calder/inspect_tree_season_graph.py: read-only graph/texture export for seasonal controls; passed with all source assets unchanged.
- Tools/calder/run_editor_cmd_multiplayer.py: executable-only failed diagnostic, original multiplayer harness retained.
- Tools/calder/run_profiled_multiplayer.py: client-only trace instrumentation against the unchanged review; completed capture isolated Python and Unreal GC costs.
- Content/SeniorSendOff/Calder/Trees: four owned unbound mesh candidates plus a local autumn master/two leaf instances, not final appearance/performance acceptance.

- Tools/calder/export_client_timing.py: headless Insights export through a response file; validated against the completed client trace.
- Tools/calder/inspect_tree_leaf_graph.py: read-only leaf-parent/function and shared foliage inspection;49graphs exported with source guards passing.

- Tools/calder/tree_foliage_assets_batch.py: owned local autumn leaves and readback-verified instance assignment to preview candidates; source assets unchanged.

- Tools/calder/tree_bark_assets_batch.py: eight owned opaque bark instances, preview assignments and source SHA guards.
- SourceArt/CalderLandscape/phase2/tree_preview_checkpoint.json: eight individually inspected native image hashes, actual tree triangle counts, preserved gates and outstanding integration checks.

- Tools/calder/export_tree_wood.py, build_tree_wood.py, import_tree_wood.py: read-only owned-donor export, Blender UV/normal-preserving FBX derivation and owned static threeLOD import/reduction.
- SourceArt/CalderLandscape/phase2/tree_wood.json and tree_wood/: actual imported budgets/axes check, source FBXs and derivation hashes; visual acceptance separate.

- HouseCalderTrees.cpp/.h: explicit65-maple render-fit trial, preserved original body/instances, local shared-wind MIDs.
- HouseCalderTreeFitReview.cpp: native780contact/body/settings/wind checks and actualpose CSV.
- Tools/calder/compare_tree_fit.py: exact original/trial65pose comparison after native passes.
- Tools/calder/perf_tree_trial.py: prepared canonical-method three alternating exterior/woods A/B diagnostic.
- phase2/tree_fit_checkpoint.json, tree_woods_visual_checkpoint.json: independent physical/pose and inspected in-game visual receipts.

- Tools/calder/inspect_woods_prop_materials.py: read-only original36-asset SHA inventory, material graphs/instancing flags and source mesh budgets.
- Tools/calder/woods_prop_assets_batch.py: two owned instancing-ready materials and three-LOD fern/stump derivatives, exact original bounds and LOD0 counts, no source/map writes.
- Source/SeniorSendoff/HouseCalderLandscape.cpp: load those owned props into the same two existing HISM batches; log actual original-foot error, triangles, materials/culls/collision/lighting flags.
- SourceArt/CalderLandscape/phase2/tree_perf_checkpoint.json: completed three-pair/15-view tree timing diagnostic, distinct from full final acceptance.

- Source/SeniorSendoff/HouseCalderPropViews.cpp: cached screenshot-only selection of existing fern/stump instances, physical ground+130cm eye and unobstructed sight line. New detail macro entries append to HouseExterior views; no prop movement, default gameplay or prior camera changes.

- Tools/calder/inspect_evergreen_kit.py, inspect_evergreen_fbx.py, inspect_evergreen_material_graph.py: read-only acquired-kit metadata/FBX geometry/material-function/channel screening; no source/map saves or runtime skeletal/PVE bindings.
- Tools/calder/prepare_evergreen_spray.py: deterministic photographed-needle atlas, actual component-isolated alpha, decoded/rotated packed normalXY, own seed/no runtime RNG change.
- Tools/calder/import_evergreen_spray.py: two owned nonvirtual atlas textures with proper colour/normal and alpha-preserving mip settings;138source SHA guards.
- HouseCalderLandscape.cpp: opt-in CalderEvergreenAtlasTrial replaces only existing species4 MID textures; geometry/placement/collision/wind keys unchanged.

- HouseCalderRidgeForest.cpp/h: opt-in12merged scenery tiles,3LODs, altitude/species patchwork, pure existing heights, no collision/shadows/DF/indirect/tick. OuterLand invokes the gated builder.
- HouseCalderRidgeForestReview.cpp: actualrender positions/LODs/flags/ISMbudget/surfacebands and10-25percent valleygreen validation; added tothreeexisting self-exit lists.
- Tools/calder/ridge_canopy_assets_batch.py: owned224instruction/3sample canopy shader with palette/temporaledge and original-parent SHA guard.
- phase2/ridge_forest_checkpoint.json: native/geometry/visual checkpoint; sourcev1 retained, palettev2 geometrydigest identical; final photo/performance open.

- `Tools/calder/falls_jets_assets_batch.py`: rebuild only the owned, opt-in waterfall jet material; five existing-material SHA guards, actual shader budget and pixel-animation readback.
- `Content/SeniorSendOff/Calder/Materials/M_CalderFallsJetsTrial.uasset`: masked opaque GBuffer jets with ballistic flow, roughness .06; OFF by default.
- `Source/SeniorSendoff/HouseCalderWater.cpp`: optional material and noncolliding strand-tail trial; gameplay water/collision authority unchanged.
- `Source/SeniorSendoff/HouseExterior.cpp`: optional generic delayed motion screenshots after the original timing sample.


- `Source/SeniorSendoff/HouseCalderFarm.cpp/.h`: OFF-by-default farm trial; five plain static ISMs/three LODs, geometry/material assembly, deterministic code-painted hay fibre textures, pure barn/bale reserves, cached actual-ground cameras and opt-in actual render-buffer/settings dump.
- `Tools/calder/inspect_farm_material.py`: read-only actual shader/ISM/source-SHA audit; no Unreal assets saved.
- `SourceArt/CalderLandscape/phase2/farm_trial_plan.json`, `farm_material_audit.json`: preparation scope and original nine-source budget receipts. Runtime/visual acceptance stays separate.
- `Source/SeniorSendoff/HouseCalderCover.cpp`, `HouseCalderLandscapeReview.cpp`: trial-only farm-footprint rejection, independent actual footprint checks and stricter total Calder plain-ISM budget; original thresholds/samples unchanged.


- `Tools/calder/farm_roof_assets_batch.py`, owned `M_CalderFarmRoof.uasset`: opaque2sample metal/rust derivative; six existing material SHA guards, no map/shared parent writes.
- `Source/SeniorSendoff/HouseCalderFarm.cpp`: private six-map hay/leaf and two-map roof painters, alpha-preserving roof writer/readback, curved plant surfaces/stems, scoped architecture draw range and actual native data checks.
- `SourceArt/CalderLandscape/phase2/farm_reference_plan.json`, `farm_leaf_checkpoint.json`, `farm_arch_cull_checkpoint.json`, `farm_roof_material.json`, `farm_roof_checkpoint.json`: reference provenance, intermediate failures/results and current partial art/native proofs.


Rock trial additions: `Tools/calder/rock_strata_assets_batch.py` builds only the owned rock material and checks six original hashes. `HouseCalderLandscape::RockMaster/RockLook` select and configure the trial for five natural-rock/masonry scan sets; only the two existing RockMaterial helpers in `HouseCalderFeatures.cpp` and `HouseCalderWater.cpp` call them. Geometry and collision are unchanged. `HouseCalderPropViews.cpp` and the appended exterior view entries provide physical-floor/face-checked close-ups of the existing road cut and campus retaining wall. The original screenshot entries and planting exclusions stay fixed. The exporter includes the new owned material.


Persimmon additions: `HouseCalderFarmTree.h/.cpp` owns the deterministic site check, private code-painted maps, shared branch attachment positions, closed branch-bend surfaces, three tree LODs, exact visible trunk collision,96contact probes, and cached physical/detail cameras. `HouseCalderFarm.cpp` gates those calls, merges geometry into the existing fence cluster and rejects ground-cover footprints at the trunk. The original four barn/roof/bale/curing geometries remain unchanged. New exterior screenshots are appended; original views and planting exclusions remain fixed. `phase2/farm_persimmon_plan.json` records scope/references/required acceptance.


Barn merge: `HouseCalderFarm.cpp` conditionally appends the co-located roof draw sections to the timber mesh, retaining original material data and culls. Its opt-in FarmDump now writes actual architecture triangle-corner shading/UV/material records. `Tools/calder/compare_barn_draw.py` compares native renderer data, all other farm positions and cover poses across the switch. `phase2/barn_merge_checkpoint.json` records checks and the freed component slot. The original distant fence remains separate.


Hedgerow additions: `HouseCalderHedgerow.h/.cpp` owns the optional farm-fence clumps, private textures, shared wind registration, physical macro cameras and passive native review; `HouseCalderHedgerowGeometry.h/.cpp` owns cedar scale sprays, rose compound leaves/hips and blackberry canes with three LODs. `Tools/calder/hedgerow_assets_batch.py` and `hedgerow_foliage_assets_batch.py` create only the owned opaque/wind and masked foliage derivatives with original-material SHA guards. `Tools/calder/review_hedgerow.py` runs the feature/terrain/landscape/farm-off and dedicated-server comparisons. Existing boundary boxes and collision are untouched. All acceptance results belong in the current phase2 receipt; geometry or successful compilation alone is not visual acceptance.


Farm solids: `HouseCalderFarmSolid.h/.cpp` builds3static collision components from exactLOD0visible solids and supplies a passive9s renderer/nativephysics review; `HouseCalderFarm.cpp` shares the original deterministic bale poses/fence runs and adds optional solid geometry construction plus trial-only cover reserves. `HouseCalderFarmSolidWalkReview.cpp` is the separate11leg input walker/profile/capture test. Its flag is appended to the3requiredSelfExiting lists and terrainSharedRun list, without changing existing routes/limits. `Tools/calder/review_farm_solids.py` runs5compatibility/server cases and compares colliderCRC to client.


## Wildlife framework and current bird-model preparation

- `HouseCalderWildlife.h/.cpp`: one authority director,FastArray,deer reactions/stillness/alarm/recovery/dormancy,scene clock,console and Cycles64/CSV timing; normal terrain-enabled default with explicit disable switch. Bird-model review remains behind its own flag.
- `HouseCalderWildlifePrivate.h`: deer route/pose/stance/statistics/audio caches and true4096sample ring windows.
- `HouseCalderWildlifePaths.h/.cpp`: analytic deer graphs,3habitats,building/pave/stair/apron/road/water/trunk/solid-prop rejection and deterministic routes; no physics path queries.
- `HouseCalderWildlifePose.cpp`, `HouseCalderDeerData.inl`: procedural deer skin controls,gaits,feet/ground anchors and offline donor anatomy.
- `HouseCalderWildlifeDraw.cpp`:4movableISMs,3LODs,225current/previous custom floats,full/15Hz tiers,per-cycle medium sole anchors,batch updates/culling/bounds/contact cards.
- `HouseCalderWildlifeSound.cpp`, `HouseCalderWildlifeSoundPcm.cpp`: once-worker PCM,fourpooled one-shots/twoloops,registered recording submix,indoor/lawn/source caps and original cricket restoration. Remaining species/echoes unfinished.
- `HouseCalderWildlifeReview.cpp`: first-slice graph/component review. `HouseCalderWildlifeApproachReview.h/.cpp`: actual-input reaction/recovery/render/audio/bounds/unpinned diagnostics. `HouseCalderWildlifeSoleReview.cpp`: submitted reduced-pose sole matrices/per-variant samples/physical170cm cameras. `HouseCalderWildlifeNetworkReview.cpp`: real-lobby deer state/GPU-completed-frame evidence. These do not accept the whole roster/master.
- `HouseCalderWildlifeBirds.h/.cpp`, `HouseCalderCrowData.inl`: new static goose/brown-red bat geometry,acquired crow controls,3LODs,sharedcode atlas,five used UVs/225pose floats,smooth normals,per-triangle winding,blended bat membrane and bill poses. No ambient bird behavior yet.
- `HouseCalderWildlifeBirdArtReview.cpp`: explicit16fixed-pose native model review/physical170cm eyes;diagnostic components stay absent from normal on/off/NoTerrain launches.
- `SeniorSendoff.Build.cs`: NetCore dependency. `HouseCampusPolish.cpp`: authorized conditional night-bed replacement of original campus cricket. Four existing layout/interior/terrain review files append only the new self-exiting flags.
- `Tools/calder/build_deer_static.py`, `wildlife_assets_batch.py`: owned acquired-deer static derivatives/materials and donor guards. `build_crow_static.py`, `crow_assets_batch.py`: crow conversion/jaw/LODs/owned shader/import. `bird_atlas.py`, `bird_assets_batch.py`: original code-painted shared bird palette/rig material.
- `Tools/calder/review_bird_art.py`, `guard_bird_models.py`, `review_wildlife_default.py`, `verify_game_signature.py`: native model evidence,scope hashes,normal/disable/no-art-leak launches and strict generated-game verification.
- Other `Tools/calder/*wildlife*.py`: actual reaction/MP/timing/compatibility/audio evidence; paired timing audit preserves the original independent-median report and thresholds. `phase3/deer_checkpoint.json` records the verified deer snapshot; `sky_checkpoint.json` records later explicit model work. `phase3/sky/PLAN.md` retains remaining flight/count/audio/native gates.
- `Saved/CalderWork/sky_flight_preparation/*.candidate`: uncompiled data-only flight drafts,not runtime integration or accepted paths. Landing-count clarification remains pending.

- `HouseCalderWildlifeSkyPaths.h/.cpp`: analytic goose/crow carrier paths plus deterministic V/stream members and8bat circuits; common planar V fits terrain altitude intervals,193-cell solid broadphase with continuous segment sweeps/full bat trigonometric envelopes. Native data acceptance is separate from rendering.
- `HouseCalderWildlifeSkyRuntime.h`, `HouseCalderWildlifeSky.cpp`:opt-in4localISMs and63fixed slots,225rig floats,shared scene clock,root/wing updates within400m/frustum,zero-scale individual hiding,5s component hysteresis,Prev=Cur stop and2Hz bounds refresh on the existing director.
- `HouseCalderWildlifeSkyPathsReview.cpp`, `Tools/calder/review_sky_paths.py`:nonrendered native geometry audit,with source/config freezes and retained failures.
- `HouseCalderWildlifeSkyReview.cpp`, `Tools/calder/review_sky_flight.py`:explicit moving review clock,physical170cm standing eyes,actual submitted ISM roots/bounds and18native captures; normal clock/two-peer/audio/photo acceptance remain separate.

## Owner wildlife revision (2026-10-18)

- `HouseCalderDeerAnimation.inl`, `HouseCalderDeerData.inl`: five sampled Fab clips and 21 actual donor controls.
- `HouseCalderWildlifePrivate.h`, `HouseCalderWildlifePose.cpp`, `HouseCalderWildlifeDraw.cpp`: 253-float deer palette; donor legs, terrain stance, bounded state-dependent pose memoization, exact medium-key reuse and cached local transforms. Nonshipping fresh-reference review verifies the optimization.
- `HouseCalderWildlife.cpp`: quieter home-bounded wandering, nonzero safer escape destinations, normal three-species defaults and optional phase timing breakdown.
- `HouseCalderWildlifeBirdCalls.*`, `HouseCalderCorvidRecordingData.inl`: longer owner-approved recorded crow/raven calls, source-derived crow beak envelope, separate ridge raven attenuation; approved goose honk and original twelve buffers retained.
- `phase3/audio/nps_corvids/`, `Tools/calder/prepare_corvid_recordings.py`, `analyze_corvid_audio.py`: original public-domain masters, processed excerpts, exact processing/attribution and actual native mixer proof.
- `Tools/calder/review_owner_wildlife.py`, `review_goose_audio.py`, `review_wildlife_multiplayer.py`, `review_goose_reaction_multiplayer.py`, `perf_wildlife.py`, `guard_wildlife_revision.py`: normal-default native, source freeze, reaction, audio, sole, MP, unchanged-budget timing and protected-scope checks.
- `phase3/owner_wildlife_revision_20261018.json`: current owner scope, approvals, accepted native results, retained failures and remaining build/performance/recovery gates.
- `Tools/calder/review_nightfall.py`: prepared phase/real-clock/restoration review only; no nightfall source is active at this wildlife checkpoint.

## Thirty-minute nightfall follow-up (2026-10-18)

- `HouseCalderNightfall.cpp`: shared GameState time, clamped30-minute smoothstep and night hold;1Hz existing sun/sky/fog-color updates. One48KiB generated uniform cube on existing ExteriorDuskFilmLook supplies a small late-night fill; original settings restored indoors every frame. No new actors/lights/collision/saved assets; terrain/landscape/dusk/night switches gate it.
- `Tools/calder/review_nightfall.py`: pinned phase, actual600-frame renderer-clock window, hold and three restoration paths; unique evidence directories and Source/Config freezes. Diagnostic fill overrides never enter normal launches.
- `Tools/calder/perf_wildlife.py`: future original dusk comparisons explicitly pin nightfall to0; original wildlife benchmark remains a recorded pre-nightfall source-addition result.
- `phase3/nightfall_20261018.json`: current source, retained failed candidates/oracles, final54capture native checks, real-lobby/shared-clock evidence, build/sign and recovery target.
- `Saved/CalderWork/nightfall_20261018_v1/`: raw/checkpoint plans, phase comparison images, full source carryforward proof and both initial/settled clock analyses. No actual30-minute wall-clock soak is claimed.
