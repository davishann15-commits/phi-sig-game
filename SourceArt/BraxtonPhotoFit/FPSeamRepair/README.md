# First-person proximal sleeve repair

This derivative repairs only the active C01 first-person arm geometry. Do not overwrite `../SK_C01_Arms.fbx`: that file is the preserved baseline used by the generator's hash guard. No Unreal assets, runtime code, animation source, third-person source, skeleton or grip socket were edited by this repair.

## Files and method

- `generate_fp_seams.py`: idempotent native Blender generator plus topology, original-data, raw FBX record and reopen checks.
- `SK_C01_Arms_SeamRepair.fbx`: repaired derivative; final hash and measured counts are in `repair_export.json`.
- `review_fp_seams.py`: isolated CPU Cycles before/after pose review; evidence is saved under the sibling Senior Sendoff repository's `ArtSource/HouseRebuild/Analysis/FirstPersonSleeveAudit20260928`.

The old arm-only copies ended at a jagged `summed arm weight > 0.5` crop. Each of the two proximal garment loops and two skin loops is first smoothed locally, then continues through eleven curved rings to a hidden terminal cap behind the FP camera. The old jagged boundaries become interior edges. The hidden skin roots taper to a quarter of their shoulder-section radius inside the outer sleeve, preventing near-plane exposure of a second skin surface. Their ordered oval sections sit just proximal to the retained sleeve surface so adjacent old faces do not fold inside out. The two intentionally open garment cuffs remain open. This is a continuation, not a flat cap at the visible cut.

The intact `../Braxton_Playable.blend` supplies shoulder cross-section dimensions. Its later-refined full body/garment no longer matches the cropped FP copies exactly, so it is not substituted for the hands or forearms. The repair moves only the four original boundary loops: 97 garment vertices (maximum 5.64 cm) and 168 skin vertices (maximum 7.93 cm). The other 7,020 original vertices and their weights, every existing UV, and all original polygon connectivity are preserved exactly. Only the four repaired boundary loops receive a normalized mean of their own source shoulder weights; this removes the high-frequency arm/torso weighting discontinuity that contributed to the torn silhouette. Local quad triangulation and proximal normals can change with the smoothed shape; distal raw FBX normal values are copied exactly, with native Blender custom-normal re-encoding measured separately. Appended vertices smoothly interpolate from that source-derived shoulder blend toward the existing clavicle bones. The per-mesh maximum influence counts and precise proximal weight deltas are recorded in `repair_export.json`; distal influences are untouched. A closure-only first draft was rejected because its old cut remained angular in the source-height camera view. That draft is not the delivered derivative.

Ordinary Blender FBX round-tripping slightly rounded bone matrices. The generator therefore replaces only geometry child arrays and cluster vertex index/weight lists in the original parsed FBX document. Original model records, bones, bind transforms, materials, object IDs, connections, unit/axis settings and animation metadata are retained and compared after reopening. The final file is then imported anew in native Blender and checked independently.

## Commands

Run from any working directory on this machine:

```bash
flatpak run --filesystem=host org.blender.Blender --background --threads 4 --python-exit-code 1 --python '/home/davis/Documents/ChatGPT/phi-sig-game/SourceArt/BraxtonPhotoFit/FPSeamRepair/generate_fp_seams.py'
flatpak run --filesystem=host org.blender.Blender --background --threads 4 --python-exit-code 1 --python '/home/davis/Documents/ChatGPT/phi-sig-game/SourceArt/BraxtonPhotoFit/FPSeamRepair/review_fp_seams.py'
```

Run the generator twice to verify `reusedByteIdenticalOutput: true`. It still reconstructs and validates in memory and reopens the existing derivative on the second invocation; it does not rewrite a matching deliverable. An unexpected baseline hash deliberately fails instead of silently repairing a different source.

## Exact Unreal integration target

Reimport only the existing skeletal mesh:

`/Game/Characters/Cobble/C01/SK_C01_Arms.SK_C01_Arms`

Use **Reimport With New File** pointing to this directory's `SK_C01_Arms_SeamRepair.fbx`, or the equivalent existing-asset native API. Keep the runtime object path unchanged. Preserve this existing skeleton reference:

`/Game/Characters/Cobble/C01/SK_C01_Arms_Skeleton.SK_C01_Arms_Skeleton`

Current serialized material slots, in order, from the existing UAsset metadata:

| Slot | Existing material interface |
|---|---|
| `Braxton_DetailedCloth` | `/Game/Characters/Cobble/C01/Braxton_DetailedCloth.Braxton_DetailedCloth` |
| `Braxton_BodySkin` | `/Game/Characters/Cobble/C01/Braxton_BodySkin.Braxton_BodySkin` |

C01's runtime cloth override remains `/Game/MetaHumans/BraxtonRebuild/MH_Braxton_Rebuild/Details/Hoodie/M_GrayHoodie.M_GrayHoodie` (`StoryCampaign.cpp`, C01 branch). It must still bind by the same cloth slot name. Do not reimport or replace either material.

Before import, snapshot the mesh's actual native import settings, skeleton reference, both material interfaces and `hand_r` socket/bone transform. Use the existing mesh conversion settings. The historical Cobble importer sets `convert_scene=True`, `force_front_x_axis=True` (default command option), `convert_scene_unit=True`, `import_uniform_scale=1.0`, and imports normals. The UAsset metadata records `FBXNIM_ImportNormals`; the other historical values have not been read through the native Unreal API by this source-only task, so retain the current asset's settings rather than blindly recreating them.

For this **mesh-only** reimport, turn off animation/material/texture/physics-asset generation and skeleton reference-pose updates (`update_skeleton_reference_pose=False`, `use_t0_as_ref_pose=False`). Preserve material assignments and the existing skeleton. The older full-character importer intentionally enables skeleton updates and imports materials; do not run that importer as this repair's integration step.

Recorded Blender candidate export settings: global scale 1, apply unit scale, `FBX_SCALE_NONE`, `axis_forward='-Y'`, `axis_up='Z'`, no leaf bones, no baked animation, no mesh modifiers, face smoothing, selected armature plus these two meshes, relative paths, no embedded textures. The final derivative takes its original FBX 7400 global settings from the baseline instead of adopting the candidate's regenerated skeleton/conversion data: UpAxis=2/+1, FrontAxis=1/+1, CoordAxis=0/-1, UnitScaleFactor=OriginalUnitScaleFactor=99.91816878318787. These seemingly unusual factors are intentional exact preservation, not a new scale correction.

## Native validation still required after import

The source-level checks do not prove native Unreal rendering, material overrides, gameplay or network behavior. Run the existing `-DouliTest -DouliVisualTest` and third-person `-DouliBodyTest`, and inspect actual first-person idle, windup, release, flight, return and catch at normal/wide FOV. Revisit house cameras 09, 53, 64 and 83, including both sleeves. Preserve rim/finger contact and visible hands. Compare the existing third-person art and owner/remote visibility in the usual multiplayer check. Only the integrating task should claim those native results after they run.

CPU previews use the source ArmsIdle animation at frame 17 and a two-bone solve transcribed from the local engine mathematics with the game's target/pole vectors. They omit the hat, native materials and C++ finger curl. Camera height is estimated from full-body source bounds using the game's height/184 attachment factor; the native mesh may have a different bounds extension. The views are useful source comparisons, not substitutes for native captures.
