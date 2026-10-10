# Furniture damage V2

448 unique furniture assets, 513 placed furniture instances, and 1,344 new stage FBXs.

The original intact FBXs, actor grouping, material slots, UVs, pivots, and placement are untouched. Their SHA256 hashes were checked before and after generation. Windows, room geometry, doorways and floor plan are unchanged by this pass.

## Damage appearance

- First stage: capped chips and local missing surface patches reveal wood, foam, raw metal or neutral interior.
- Heavy stage: larger local fractures, loosened upholstery and selected snapped legs. Attached structural supports remain connected.
- Final stage: recognizable rigid panels, cushions, boards and legs lie on their broad faces near the original base level. No global squash, per-face stretching, spike displacement or unsupported wire-like seam debris.
- Thin wall artwork is clipped/torn into sections while preserving printed exterior UVs. Rugs and cloth sections receive finite thin collision hulls.

New damage materials are defined in manifest.damage_materials. Each stage's *_material_slots retains the complete original material-slot prefix, including unused slots, and appends the four interior materials. Native per-instance material overrides should only affect the original prefix.

## Native import contract

Use each furniture row's damaged_asset_name, heavily_damaged_asset_name and broken_asset_name, plus corresponding *_fbx and *_material_slots. Import each FBX's UCX collisions with one_convex_hull_per_ucx=True. Each stage has 1–12 hulls fitted to that stage's fragments; do not reuse the intact object's hull. These are combined stage meshes, not independently simulated fragment actors.

FBX paths in the manifest point to the active output folder. Archived copies are in models/damage-v2; names match exactly.

## Verification

The complete binary-FBX audit is inventory/damage-v2-validation.json: all 448 sets pass finite-geometry, distinct-stage vertex geometry, exact slot-order and valid 1–12 UCX name/hull checks. inventory/damage-v2-assets.json records all original intact SHA256 hashes and per-stage bounds/counts. Representative same-scale geometry reviews are inventory/damage-v2-row-0..5.png: club chair, sofa, plastic chair, coffee table, framed picture, fridge/microwave. Previews use simple audit colors, not the native textured materials.

Runtime physics, native material appearance and map saving are verified separately by the parent integration task.
