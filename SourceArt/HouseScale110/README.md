# House enlargement — 10%

The Chapter01_House map uses a uniform factor of 1.1 about the original world origin. Architecture, furniture, attached decorations, doorway positions, water dimensions and runtime house details follow that factor. Player, Boy Wick and pig body sizes retain their previous settings. The original room arrangement and object facing are preserved.

## Original map backup

`before/Chapter01_House.umap` is the saved map from before enlargement. `before/actors.json` records the original 1,812 placed actors, transforms, attachment parents and relevant world-space properties.

The installer always derives its values from that original snapshot, so rerunning it does not enlarge the house again. It checks positions, rotations, scales and inventory before saving. Coal starts on top of the mattress rather than intersecting its surface.

## Verification

`evidence/resize-audit.json` contains the saved actor audit. `evidence/runtime-results.json` records checks in the actual Chapter 1 game; pass status requires each check's explicit result marker, not process exit code. Earlier attempts are retained under attempt folders.

Final verification passed nine checks: four selected doors with opening/closing and player collision, real walking through all four porch entrances, hot-tub entry/wading/exit, both pigs gated by the closed door and escaping after opening, all six crawl portals and the connecting route, stair ascent/descent, the infinite hallway including held-pig and furniture return/release, twelve hallway/porch views, and four furnished-room views. The latter images are saved in `evidence/final-views` and `evidence/final-rooms`.

To return to the previous size, close Unreal, preserve the current map, and restore the backed-up map to `Content/Story/Maps/Chapter01_House.umap`. The runtime scale helper is gated by this map's SSOHouseScale110 tag, which is absent from the original backup.
