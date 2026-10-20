# Calder asset preparation

Reproducible derived assets for the current in-progress appearance pass. Original acquired Fab content and approved CC0 downloads remain read-only. No script loads or saves a map. Runtime acceptance is recorded separately in `../STATUS.md`.

## Pig pipeline

1. `Tools/calder/phase2_assets.py` exports the owner's acquired pig, doe, fox and crow donors; `inspect_animals.py` records their anatomy in `animal_anatomy.json`.
2. `build_pig_parts.py` fits the pig within the existing X envelope (-69.5..85 cm), retains its source texture UVs and native anatomical link lengths, removes broad detached bristle-shell cards and generates static controls plus the continuous outer skin. UV1-UV4 contain four normalized rigid-transform influences. Its contact proof requires visible snout and hoof endpoints to use exactly the same head/lower-leg transforms measured in game.
3. `Tools/calder/build_pig_skin.py` creates `M_CalderPigSkin` in the new Calder folder, blending posed positions/normals through twelve existing procedural transforms and four native texture samples. No runtime skeletal mesh or per-frame mesh rewrite.
4. `Tools/calder/import_pig_parts.py` imports only `/Game/SeniorSendOff/Calder/Pig`, replaces numeric vertex/UV data, authors three LODs, disables Nanite/collision, enables CPU contact inspection and checks imported bounds against the offline manifest. The visible skin uses LOD0 (6,744 triangles) to preserve joint IDs; hidden controls retain pose/interaction interfaces.
5. `pig_skin_batch.py` runs material creation and imports together in an offscreen ordinary editor (`-ExecutePythonScript`). Commandlets do not expose StaticMeshEditorSubsystem. The owner authorized the editor-closed batch on 2026-10-16; authorization persists unless the ordinary editor reopens. Let ExecutePythonScript exit automatically; do not call quit_editor from the script.

The capsule, hit box, room, door and replication remain unchanged. Native support sampling accounts for all four soles and mattress/floor edges. The snout follows the real surface, and the relaxed native sleeping pose avoids the old toy rig's extreme folds. New-appearance steering adds safe wider turns to escape a wall-side deadlock. `-NoCalderLandscape` and `-NoCalderTerrain` retain the original appearance, pose and steering.

An import or assembly marker does not establish visual or locomotion acceptance. All six `codex_pig_relaxed_sleep` diagnostic captures were saved and inspected. `codex_pig_relaxed_animal` passed at 59.28 observed seconds with independent natural sniff/resume cycles, actual snorts, running ears and bounded support/drift. Existing thresholds/counts/timeouts remain unchanged. Earlier segmented/collar-only visual failures and interim contact/sniff failures are preserved under `Saved/CalderWork/runs/codex_pig_*`.

The optional resize/escape smoke fails at the same open-door graph join with both appearances. The protected door/room was not changed. Historical upstairs carrying/grip fixture coordinates remain unrebased; carrying is not claimed verified.

## Woods pipeline

- `prepare_woods_sprays.py` prepares two deterministic 1024px photographic branch atlases from the approved CC0 LeafSet012/028 downloads, retaining the existing sprite UV/wind layout. Four PNGs, source URLs, raw hashes and import settings live in `woods_sprays/`.
- `import_woods_sprays.py` imports colour as sRGB, normals as linear DirectX normals and coverage-preserving alpha mips. `woods_assets_batch.py` bundles that import with falls preparation.
- Runtime floor/bark overrides use existing imported CC0 scans. Acquired fern/stump meshes replace two existing non-colliding HISM batches, with no new batches, no Nanite, original instance feet and distance culling retained. Added hardwood crown profiles retain more foliage at distance. Original shared tree meshes, transforms/checksum, terrain, trail and hut remain unchanged.
- `inspect_woods_assets.py` reads eight acquired beech/maple trees without modifying them. `tree_donors.json` found Nanite and million-triangle intermediate LODs. Conventional reduced derivatives and visual/performance checks are required before using them; no tree source is bound by the inspection.

WoodsPass/CampusWoods pass and the source checksum remains `0f3b2bb7`, but captures still show overly simple tree forms. This is not final photographic-quality acceptance.

## Water pipeline

`falls_assets_batch.py` runs the two owned material builders in `phase2_assets.py`, with the ordinary editor closed and no map save. `M_CalderFalls` is opaque, world-XZ projected, downward advected and roughness .06. `M_CalderStream` is a derivative of the read-only original water master: continuous procedural foam replaces its leaf-photo foam mask, with glossy whitewater. Actual Metal compile reports are `falls_material.json` (305 pixel instructions/3 samples) and `stream_material.json` (305/7).

Runtime binding is appearance-gated. Four transient `CalderHollowMist*` local volumes reject whole-envelope frozen-zone overlap. Global atmosphere and the three original WoodlandHollowMist actors remain unchanged. Terrain/water checks pass; water/rock edges and falling-sheet appearance still need visual polish.

## Sources and visual references

The skin donor is the owner's acquired Animal Variety Pack under its acquired Fab licence. Fern/stump donors remain under their acquired Fab licences. CC0 scans retain the source manifest in `../ASSETS.md` and adjacent derivation JSON files. Source packs must already be available in a target checkout; the checkpoint package contains only the new derived assets.

References viewed for anatomy/appearance only, never imported or redistributed as game assets:

- [Domestic pig side photograph](https://a-z-animals.com/animals/pig/).
- [Autumn mixed woodland photographs](https://www.whitetailproperties.com/hunting-land/west-virginia/ritchie/peaceful-retreat-with-acreage-and-cabin-just-outside-pennsboro-in-ritchie-county-wv).

## Current ground and cover checkpoint

`ground_assets_batch.py` created the owned `M_CalderLandscapeGround`; actual Metal compilation is377pixel instructions/12samples. Original ground/rock/water masters remain SHA-identical. Runtime vertex masks and field-zone tones bind it only when Calder landscape is enabled. Frozen/intersecting legacy lawn patches remain unchanged.

Four collision-free plain ISMs contain57,815 grass/broomsedge/aster/fluff clumps, with54/56/58/40triangles and25-45m culling. Seed sprays use alpha-preserving mipmaps. Native review and exact paired placement agree; the separate rendered movement gate still fails after stalls. Four130cm-eye macro captures were inspected before the stadium-only exclusion. This is partial appearance acceptance; final photographic comparison and performance remain open.


## Conventional tree candidates and trials

`tree_assets_batch.py` creates owned Field_01 leaf meshes and single-LOD structural donors from the acquired beech/maple packs. All 476 source assets are SHA-guarded. Derived assets have Nanite and collision disabled. They began as unbound previews; the explicit fitted maple trial below is now available, while default trees remain unchanged.

Initial forest forms were rejected after eight views. Broad field forms improved the crowns, but automatic reduction lost structural wood. Retaining all wood repaired distant attachment at excessive triangle cost. Full-precision UV/tangent copying then repaired near bark spots; main-trunk-only retention still lost the upper bole at distance. All 32 frames from those four trials were individually inspected.

The current helper keeps the detailed lower trunk and clusters only upper wood on 2/12/24cm source grids, retaining full-precision packed UVs/tangents. It rejects nonmonotonic LOD counts. DisableUnity build: 204 actions, 126.78s. Native counts are Beech 40,193/16,256/8,653 and Maple 34,283/17,728/9,561. The autumn-only trial still showed trunk patches; switching candidate wood to local opaque materials removed them, isolating inherited material behavior rather than a remaining UV-copy defect. All eight latest images were individually inspected (56 across all trials). They clear these diagnostic defects; forest fit, wind registration, altitude palette and performance remain unaccepted.

Read-only `inspect_tree_season_graph.py` passed and exported the seasonal function, shared collection and four 8192px source leaf textures. The shared collection has Season Strength zero, so a local Season Offset alone does not enable the full autumn blend. Shared parameters/assets remain unchanged. Local autumn materials, altitude palettes, common wind registration and physical/performance checks are still required before forest integration. `inspect_tree_leaf_graph.py` subsequently completed read-only inspection; its acquired branch-selector result is used by the latest owned shader below.


## Owned local autumn leaf materials

Run `tree_foliage_assets_batch.py` after `tree_assets_batch.py`. It creates only an owned master and two local instances under Calder/Trees, assigning them only to the four unbound preview meshes. The shared NatureFoliage asset, original acquired materials and shared parameter collection remain unchanged. Re-running the geometry batch restores original material assignments; re-run the foliage batch afterward.

Read-only inspection traced the acquired leaf function: Mask.R is coverage, Mask.G roughness and Mask.B translucency. The local master retains NatureFoliage wind/two-sided normals, applies autumn colour only to the translucent leaf portion, preserves branch colour, and uses Mask.R with per-instance fade. Source albedo alpha is opaque and is deliberately not used for coverage. No shared seasonal setting is changed. Local AutumnAmount/AutumnTint parameters allow a later altitude palette.

`codex_tree_local_foliage_batch_v2` passed actual material compilation: both instances243pixel instructions/3texture samples. Source476pack assets and the shared foliage master passed SHA guards. An initial script assertion incorrectly trusted UE5.8 setter return values, which are false placeholders in the engine implementation; the corrected batch verifies actual parameter readback instead. Native visual acceptance and live forest wind registration/fit remain pending.


## Owned local bark material isolation

Run `tree_bark_assets_batch.py` after the geometry and leaf batches. It creates eight owned instances of read-only M_NaturePropTextured using the acquired bark albedo/normal maps and assigns them only to the four preview meshes. Actual compilation is229pixel instructions/2samples per material. All476pack source hashes and the shared parent hash pass. Prior leaf-only assets are backed up in Saved/CalderWork/tree_candidate_trials/local_foliage_baseline.

`codex_tree_local_bark_preview` passed native output/WoodsPass/CampusWoods checks, retaining treeCRC0f3b2bb7,53lights and cover57,815/CRC29aceab6. All eight raw images were inspected: root bark patches are removed and upperbole/crown attachment remains intact. This is a candidate geometry/material checkpoint only. It is not a live forest or final photographic/performance pass.


## Independent wood-only LOD trial

`export_tree_wood.py` reads CPU geometry/UV0/normals from the owned structural donors without saving any asset. `build_tree_wood.py` runs in background Blender, reflecting Y/reversing winding and V to retain the established UE FBX convention. `import_tree_wood.py` imports only owned SM_CalderTreeWood_Beech/Maple with independent threeLOD reduction and high silhouette importance. Source FBXs and their hashes/bounds are in tree_wood/wood_build.json. Import counts: Beech28,019/4,202/421;Maple32,822/4,923/493. Maximum bounds error0.000061cm; full-precision UVs/high-precision tangent basis, no Nanite/collision, original476source hashes unchanged. Only UV0 is required by the local opaque wood materials; original packed wind data is not interpolated into the derivative.

New explicit -CalderTreeReducedWood preview combines those independent woodLODs with the existing reduced leaf cards. Existing clustered fallback remains. DisableUnity204actions passed126.12s; native eight-view texture/attachment verification is running. No live forest binding/physics/placement change. New FBXs are included in recovery export.


Latest6percent far-wood trial: Beechwood28,019/4,202/1,682;Maple32,822/4,923/1,969. Combined trees40,481/9,538/3,611 and43,132/10,284/3,535. The earlier1.5percent far variant failed visual branch geometry despite passing import counts. Current native ten-view run preserves the eight prior shots and adds150m views. All ten inspected: readable distant silhouettes and clean bark, but forced-close farLOD is still coarse. Candidate for physical forest-fit work only; no final photographic/performance acceptance.


## Explicit fitted maple trial

`-CalderWoodsTreeTrial` targets the two existing mapleHISMs only. FitToOriginal keeps the standing stem caliper, uses a monotone radial transition into the original maximum-absolute crown envelope and originalheight, transforms normals/tangents with the full Jacobian, and duplicates the original60x60x400simple trunk body. No instance update. Existing culling/shadows/DF/indirect settings remain. New local leafMIDs append to the existing sharedwind keys. Owned runtime material slots are assigned directly; UStaticMesh::SetMaterial is an editor rebuild path and must not be called here. Default remains original trees until acceptance.

206-action DisableUnity build121.05s. Original/trial native reviews each pass65trees/780actual face contacts with maxerror0.000004cm. Exactactual poseSHA both b011f36278dea26d5de634d30734b64dcfa934a4d8c8ef43b60f71053865d33c. Trialwindscalar range2.238571;14registeredMIDs versus old12. WoodsPass/CampusWoods/CRC0f3b2bb7 pass. `compare_tree_fit.py` requires all native passes before byte comparison.

Twelve in-game woods frames were inspected across original, firsttrial and palette refinement. Current leaf shader uses the acquired MF_GenerateBranchMask selector saturate(B^5*25000), instead of treating translucency strength as autumn blend weight. Local gain6 yields245instructions/3samples; sourcepack/sharedparent hashes pass. Geometry/collision are unchanged by this colour-only revision. Staging preview images predate it; new in-game receipt is tree_woods_visual_checkpoint.json. Broader photographic species/altitude/ground finish remains.

`perf_tree_trial.py` completed three alternating tree-only on/off pairs, all11canonical exterior views plus4woods views,300requested samples/4secondwarmup/uncapped MetalOffscreenOnly. It does not alter original scripts. This is diagnostic: final pinned-scene-clock/allnamedviews/fiveTierAinterior chain is still required. No heavy work alongside timing. Median trial cost is+1.22/+1.89/+1.24/+1.84ms in woods edge/20m/deep/silhouette. All eleven street/campus deltas fall within0.5ms noise. Full report and summary are codex_maple_trial_perf_perf.json and tree_perf_checkpoint.json. The acquired fern/stump usage issue is repaired through owned derivatives below.


## Owned instanced fern and stump derivatives

Read-only inspect_woods_prop_materials.py found both acquired masters had instanced-mesh usage disabled. woods_prop_assets_batch.py duplicates only their materials and selected meshes under Calder/Woods, enables that usage and adds independent distance LODs. Near triangle counts and exact mesh bounds match the originals. Fern5,779/1,445/289, stump22,469/4,494/1,123, screens1/.15/.045, no Nanite/collision/distance fields. Shader240instructions/7samples and220/5, both below12samples. All36acquired source hashes remain unchanged; manifest woods_props.json.

Runtime uses the same two existing HISMs, original foot positions/culls and no collision/shadows/DF/indirect. Native codex_woods_props_review passed:655ferns/22stumps, zero actual original-foot error, no instancing fallback warning, WoodsPass/CampusWoods and65-maple/780contact fit checks allPASS. Four established woods frames were individually inspected, total90tree/woods diagnostic frames. Wide views preserve trail/hut and show no missing-geometry regression. Appended actual-instance fern/stump macro cameras now trace physical ground+130cm and reject865actual shrub/bramble bounds. Initial obscured stump frame was rejected; both revised frames inspected (94total tree/woods diagnostic images). Fern fronds read naturally; low fallenwood is visible amid ground cover. No plants move/hide, no lighting change, no final whole-woods photographic acceptance. Latest207-action DisableUnity build passed123.20s.


## Opt-in photographed evergreen atlas

inspect_evergreen_kit.py/inspect_evergreen_fbx.py/inspect_evergreen_material_graph.py screened the acquired138asset kit and31functions read-only. Its fulltree is skeletal139,849triangles and remains unused; no PVE/skeletal runtime module/material is bound. prepare_evergreen_spray.py isolates five healthy needle components and decodes NT.RG normalXY/reconstructedZ (Btranslucency), assembling999needles with ownseed and13.8869percent alpha coverage. Source/material/export/output hashes are in woods_sprays/evergreen_manifest.json. import_evergreen_spray.py saves only two owned1024px/nonvirtual textures with proper colour/normal/alpha coverage mips; all138source SHA guards pass.

-CalderEvergreenAtlasTrial changes only existing species4 BaseMap/NormalMap. Two existing batches remain19/12instances,3196/1032/226triangles, original collision/culling/wind. Default remains original atlas. DisableUnity207actions PASS120.53s. Native WoodsPass/CampusWoods/65-maple780contacts/exactactualposes pass. Additional scene review passes81floor samples and hut entry. Four existing forest images individually inspected (98total diagnostic frames); change is subtle in wide frames, so closer/botanical/fullperformance acceptance remains open. This is an acquired-Fab photographic derivative, not CC0 or a new true-hemlock mesh.


## Merged ridge forest trial

-CalderRidgeForestTrial adds12merged static scenery ISMs with three2000/4000/8000cm gridLODs outside the playable square; existing ridge/terrain geometry is unchanged. MainStdatum-1900 gives23100absolute for250m colourline. Highrusset/brown/grey, midpeakmix, valleygreen andnorth-cove evergreen use independent hash fields. Town/river/roadspur/watertowerclearings remain open. Owned M_CalderRidgeCanopy compiles224instructions/3samples using approved CC0 leaf albedo/normal and a temporal edge; shared parent unchanged.

Latest209-action DisableUnity build PASS122.41s. Native12tiles/19totalCalderISMs/583134actualvertices/zero failures, offsets2.9995..1452.3451cm and147628LOD0triangles; build166.72ms. Currentvalleygreen19.6033percent passes10-25; initial9.15986percent was corrected bygenerationcutoff, never bylooseningreview. Actualorderedpositiondigest47513baa460c8c7e5cd5150d4b8053d54ed163c5b06827438a333e03838f0940 matches throughcolour correction. WoodsPass/CampusWoods/53lights retainPASS. Two nine-frame P1-P8/tower sweeps inspected (116totaldiagnostic tree/woods/ridge frames). Canopy/colourline remains subtlein existinghaze; no final photographic/performance acceptance, trialOFFdefault. Globalfogproposal is prepared/unapplied awaitingexplicitownerapproval.


## Opt-in animated waterfall jets

`falls_jets_assets_batch.py` duplicates only the owned falls master to `M_CalderFallsJetsTrial`, retaining five existing material SHA hashes. It uses a masked opaque GBuffer surface, roughness .06, temporal coverage, reduced emission and a downward time-of-flight pattern. Unreal's Has Pixel Animation property is enabled and read back; actual Metal budget is 305 instructions/4 samples. Existing surface-water material stays unchanged.

`-CalderFallsJetsTrial` selects the candidate inside the landscape gate. Its noncolliding curtain columns reach their existing pool; animated masking supplies the broken jets instead of static pointed ends. Collision, beds, water queries, bridges and mist remain unchanged. This is an OFF-by-default appearance trial, pending native and visual acceptance. `-ExteriorMotionFrames=4` captures four delayed frames after the ordinary view's timing sample, using the same camera. Current evidence and limitations are in STATUS and the active handoff.


## Opt-in west farm trial

`-CalderFarmTrial` adds the west-edge timber curing barn, six twine-bound round bales and chestnut worm fence. Five spatially coherent static plain ISMs and three LODs; barn wood/roof use180-225m culls, field props70-90m under the unchanged graphics quality scale0.6. No collision/shadows/DF/indirect/navigation/new lights. Piers and fixed placements follow existing ground without grading. Five farm components plus other Calder scenery/cover reach the unchanged24 budget.

The normal-culling frame defect was resolved by separating distant fence from barn wood and populating/bounding instances before registration. No diagnostic global CVar was promoted. Pure barn/bale reserves leave57,705cover/17dc66ec with farm ON; OFF retains57,815/29aceab6. Native footprint/root/frozen/water/settings, terrain20k samples/zeroholes/.03cm, water and original woods checks pass; current-source fallback checks are recorded in farm_roof_checkpoint.json.

Hay side/end and four-variant curing-leaf albedo/normals are deterministic512px transient textures. Broad curved leaves hang on whole-plant stems with smooth normals; curing113280/10368/1152triangles, performance still unaccepted. The real UK leaf photo is morphology reference only (farm_reference_plan.json), never an imported asset. Parent shader229instructions/2samples, original input assets read-only.

Owned `M_CalderFarmRoof` compiles229instructions/2samples, opaque/ISM enabled, six original material hashes unchanged. Periodic code-painted zinc/rust albedoA stores metal fraction; a new owned writer retains/linearly averages that alpha over10mips. Native raw readback0/255, tail171 occurs beforeRHI upload; shared Hall painter untouched. Roughness.93rust/.49metal, actual corrugation normals and320cmUV. Builder farm_roof_assets_batch.py; no maps saved. First script-path and C++shadow failures remain in STATUS/evidence.

All four farm frames (including actualground+130cm hill roof eye) were inspected; total158diagnostic images. Barn is visible from the66m hill eye after architecture-range correction, with corrugated metal/rust detail instead of flatbrown. Original three farm eyes are unchanged. Local improvement is not whole-environment photographic/performance acceptance; trial remains OFFdefault. Pasture/terracettes/hedgerow/persimmon, broader rock/wall/water-edge finish, wildlife and final validation remain open.


## Rock material candidate

`-CalderRockStrataTrial` selects the owned 8-sample rock material only for natural rock and masonry scan sets. Natural rock gets tilted bedding and shallow recessed shale seams; masonry gets restrained weathering without bedding. Original concrete, gravel, timber and rust appearances stay on their existing material. Geometry, water levels, collision and original view positions are unchanged.

The current build and native terrain/water/woods/cover reviews pass, as do both disable switches. Six latest captures were inspected, including close-ups with physically checked 1.3 m eyes and real rock/wall faces. The change remains subtle under dusk lighting; photographic quality and final performance are not accepted, so the trial remains off by default. Details and actual hashes: `rock_strata_checkpoint.json`. Wet shoreline bands and the remaining Phase2 details are unfinished.


## Optional farm persimmon

Add `-CalderFarmTreeTrial` with `-CalderFarmTrial` to inspect the young fruiting tree beside the existing worm fence. It uses private code-painted maps and three static LODs, merged into the same fence component. Small orange fruit and sparse autumn leaves stay on the branches; only the exact visible main stem collides. Clients and the dedicated server pass96contact probes. Existing five farm components/24landscape ISMs remain, with one cover tuft excluded at the trunk and no other cover placements moved.

TreeOFF, farmOFF, terrainOFF and landscapeOFF comparisons pass; the existing barn/roof/bale/curing geometry is exact. New root-to-tip and fruit views use physical130cm eyes. Earlier detached twigs, open bend seams and clipped framing were corrected and preserved as diagnostic evidence. The final tree still has a constructed appearance under dusk; full photographic quality, final timing and whole-master acceptance remain pending. See `farm_persimmon_checkpoint.json`.


## Optional coherent barn merge

`-CalderBarnMergeTrial` combines only the nearby barn timber and roof into one plain ISM. It preserves their materials, cull distances and all actual triangle-corner position/normal/tangent/UV/handedness data. The distant fence stays separate. Paired native checks and all12images pass; other farm geometry and57,704cover poses match exactly. This reduces farm components5→4 and the current landscape total24→23 without changing the24component limit. The disable comparisons restore their original states. The switch is off by default and hedge planting is not implemented yet. See `barn_merge_checkpoint.json` and `Tools/calder/compare_barn_draw.py`.
