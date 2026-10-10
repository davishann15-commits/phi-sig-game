# Basement library reference rebuild

Saved map: `/Game/Story/Maps/Chapter01_House`.

Ten unique supplied photographs were inspected: IMG_2266, 2267, 2268, 2269,
2270, 2272, 2273, 2274, 2275, and 2284. Duplicate filenames were used once.
Furniture measurements are inferred from those photographs. The existing
library footprint is retained, with the house's horizontal 1.8 scale.

## Contents

71 model families, 157 placed objects, and 151 interactive furnishings.
Interactive families have intact, damaged, heavily damaged, and broken meshes
with fitted convex collision. Untouched furnishings do not simulate or tick.

The layout includes the octagonal card table with recessed cup wells, mixed
office chairs, gray armchairs, trestle worktable and its chip case/umbrella/bags,
black leather lounge seating and pillows, walnut coffee chest, rugs, six filled
bookcases, framed photographs and lighthouse print, television and loose
cables, distressed console and game boxes, lamps/plants, study desk/monitors,
drawer tower, stools, glass jar, roster frame, papers and small desk objects.

The extra brown mechanical door beside the entry opens into a modeled service
alcove. The three original ceiling crawl-space panels have seams, hinges and
pulls. Their existing positions and the far library door are retained.

## Controls

Use E to interact or pick up/release furniture; double-click doors to toggle.
Large furniture drags rather than lifting. Light objects can be thrown, and
furniture retains staged damage and the house damage counter.

## Verified behavior

`runtime-3/result.json` verifies all 151 furnishings start parked, every damage
stage has fitted collision and assigned materials, all three ceiling panels
exist, the closed service door blocks the full player capsule, the open door
clears it, the actual office chair can be picked up with two hand contacts,
and the coffee chest reaches all three damage stages and settles at zero speed.

`walk-result.json` records the complete 35-waypoint walking loop: 52.1 metres,
one lap, zero stalls, median frame time 16.76 ms and 95th percentile 24.48 ms.
`review` contains ten inspected native screenshots and the camera review report.

## Sources and regeneration

`models`, `textures`, `references`, `context`, and `source` preserve the edit
inputs. The working evidence mirror is
`/Users/Stewart/Projects/Senior sendoff/outputs/basement-library`.

Full generation order: `build_textures.py`, `build_library.py`,
`build_geometry.py`, `polish_textures.py`, `polish_library.py`,
`refine_table_bag.py`, `fit_card_area.py`, `build_damage.py`,
`finalize_reference_details.py`, then `install_library.py` in the native editor with the legacy FBX importer.
Run inspection exports only before rebuilding the source geometry; the context
exports here preserve the original shell. Targeted refinements can run the
polish scripts without overwriting the current manifest's placements.

The general house interactivity manifest marks the 45 replaced originals as
superseded. Its installer preserves actors tagged `SSOLibraryRebuild` and
`LibraryPhotoShell`, so a rerun keeps this photo layout and the new wall opening.
