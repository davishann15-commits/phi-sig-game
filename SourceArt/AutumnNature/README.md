# Chapter01_House autumn vegetation

Updated on 2026-10-10 in the Unreal project.

## Changes

- Replaced the exterior's solid tree and shrub canopies with curved branches, folded masked leaf cards, varied silhouettes, bark detail, and three deterministic plant variants.
- Trees use gold, copper, rust, some remaining green leaves, and exposed twigs for a fall stage. Shrubs have uneven crowns and lighter new growth.
- Grass mixes short green blades, longer dry blades, worn soil patches, and textured ground. Coverage changes gradually across the lawn and follows the existing slopes.
- Added curled fallen leaves near trees and scattered across the lawn, plus subtle material wind.
- Grass and litter use instancing and distance fading. Decorative foliage has no collision and does not cast dynamic shadows. No real lights were added.

## Scope preserved

The house, interior rooms, brighter exterior lighting, relocated road, front steps, perimeter paths, front double doors, basketball court, and existing tree/shrub collision bounds were preserved. Unity projects were not edited.

`protected.json` records the starting hashes for the map, path implementation, site layout, and court implementation. The final comparison in `evidence/protected-check.json` reports all four unchanged.

## Assets and rebuild

The native implementation is in `Source/SeniorSendoff/HouseExteriorNature.cpp` and `.h`, called by `HouseExterior.cpp`. Plant geometry and placement are generated at runtime; the map itself was not resaved.

`install_materials.py` imports six source textures and builds the ground and foliage material masters under `/Game/SeniorSendOff/House/ExteriorNature`. The saved eight assets are included in the cook directory list. A packaged build was not tested in this pass.

Source textures are unmodified 1K diffuse and DirectX normal maps from Poly Haven, licensed CC0. Download URLs and verified MD5 hashes are in `assets/manifest.json`.

- [Grass Ground](https://polyhaven.com/a/grass_ground) — Charlotte Baglioni
- [Withered Grass](https://polyhaven.com/a/withered_grass) — Charlotte Baglioni
- [Tree Bark 03](https://polyhaven.com/a/tree_bark_03) — Rob Tuytel
- [Poly Haven license](https://polyhaven.com/license)

## Verification

- Native editor build succeeded; see `evidence/native-build.log`.
- All six textures and both material masters imported and saved successfully; see `evidence/material-install.log` and `evidence/materials.txt`.
- Rendered the front lawn, front approach, court, and close views of trees, shrubs, and grass. Final previews are under `evidence/screenshots/`. The final running game was also visually inspected.
- The actual player completed all seven existing exterior movement routes, including the enlarged lawn, steps, both side paths, displaced sidewalk, and driveway connections. E opened both walnut front doors, and the player walked into the original great room. See `evidence/walk-review/result.txt` and `evidence/walk-playtest.log` (PASS; clean exit).
- Final close-view samples held approximately 30 FPS with a 30 FPS cap (33.34–33.36 ms average frame time). This is a capped smoke check, not an uncapped performance comparison; see `evidence/screenshots/frametimes.csv`.
- The existing VSM non-Nanite marking queue warning was visible in the running game. This pass did not suppress it or change global shadow settings.

The older crash-report windows already running on this Mac date from October 8–9. The rendered verification runs for this vegetation revision completed and shut down cleanly.
