# Exterior masonry, grass, and cars

Updated 2026-10-10 in the editable Unreal project, for `Chapter01_House`.

## Scope

- Replace the exterior's simple brick pattern with varied red clay brick and recessed warm mortar, following the supplied front/rear/arcade photographs.
- Add shallow, beveled vertical brick rows above existing windows and radial brick bands around the rear arcade openings.
- Replace flat grass tufts with individual curved, tapered blades that bend in the existing gusts.
- Improve the parked cars with three shapes, plausible proportions, paint, glazing, wheel arches, tyres, rims, mirrors, door seams, handles, lights, grilles, and trim.

This work uses the existing house shell, openings, parking positions, lawn distribution, and shared weather. The new brick trim and grass have no blocking collision. Car collision follows the revised body dimensions. It adds no real lights.

## Implementation

- `HouseExteriorBrick.cpp`: masonry materials and batched window/arcade brick trim.
- `HouseExteriorCars.cpp`: sedan, crossover, and pickup geometry and materials.
- `HouseExteriorNature.cpp`: 32 tapered blades per instanced grass clump, with green/dry variants and upward volume normals.
- `HouseLeafWind.cpp`: stronger grass bending in the shared gust envelope. Tree, shrub, leaf-pool, and audio behavior retained.
- `HouseExterior.cpp`: integration and opt-in detail cameras. Those cameras are excluded from the planting exclusion list so review views do not change the existing landscaping.
- `DefaultGame.ini`: includes the two new masonry/car asset directories in packaged builds.

`prepare_textures.py` creates original 1024 px clay/mortar textures with a 192 cm tile, 24 x 8 cm brick modules, normal maps, and packed roughness/AO/height. `install_materials.py` imports them and saves masonry, upright grass, and car material masters through Unreal. It never loads or saves the map. The car paint uses a clear-coat layer; the grass uses solid blade geometry with two-sided foliage shading and motion at the tips.

Original files are retained in `backup/`; photo references are in `references/`. Saved assets are under `/Game/SeniorSendOff/House/ExteriorBrick`, `/ExteriorCars`, and `/ExteriorNature/M_NatureGrassBlades`.

## Verification

- Final native build succeeded: `evidence/native-build-final.log`.
- Final material import succeeded: `evidence/material-import-final.log` and `evidence/materials.txt`.
- Rendered the front entry, window masonry, rear arches, two identical grass views at different times, and parked cars. Inspected the images in `evidence/final-pass/`.
- The grass frame comparison excludes the HUD and uses the same camera. Visible blades change between frames: `evidence/grass-frame-comparison.json`. This comparison is visual evidence of motion, not a physical wind simulation benchmark.
- Existing wind/audio review passed with 96 leaves, 503.5 cm maximum drift, gusts from 0.13 to 0.95, outdoor gain 0.292, indoor gain 0, and restored gain 0.130 on returning outside. See `evidence/wind/result.txt` and its rendered frames.
- Existing rendered walking review passed all seven routes using normal character movement across the lawn, three front risers, both side paths, the approach, and driveway connections. Both walnut front doors still offered E, opened together, and permitted entry into the great room. See `evidence/walking/result.txt` and its rendered frames. This was a development automation playtest, not a human-controlled full-house playthrough.
- The saved map, site layout, walk geometry, court, club, and existing door source hashes matched the pre-edit files: `evidence/protected-check.json`.

The screenshot reviews used a 30 FPS cap. They are not an uncapped before/after performance benchmark. Most exterior steady views stayed near 33.3 ms; startup shader work and occasional frame spikes occurred. The pre-existing VSM warning and instancing-usage warnings on older shared house materials remain. These checks do not establish a packaged-build or multiplayer performance result.
