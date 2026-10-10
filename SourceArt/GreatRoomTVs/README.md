# Great room wall televisions

Two independently mounted televisions replace the original ultra-wide black planes above the great room fireplaces. Existing walls, mantels, furniture, ceiling, and doors are retained. Each television is 3.036 metres wide in the scaled house, with a 16:9 LCD and a slim aluminum/plastic casing. Ports, buttons, vents, speaker slots, screw wells, a small power lead, and separate bolted VESA brackets are modeled.

## Damage and interaction

Each television has intact, cracked, heavily damaged, and broken meshes, with original stage-specific LCD textures. The first damage stage remains mounted. Enough cumulative attachment damage releases the body into gravity while the wall bracket stays fixed. Broken TVs can be moved using the existing furniture interaction. Weight is 23.5 kg, break health is 60, and attachment health is 36. Each television counts once toward house damage.

The `HouseWallTelevision` actor tag enables LCD damage while attached. Other existing wall props retain their previous behavior. Parked TVs do not tick or simulate until detached.

## Rebuild

Run `source/textures.py` with Pillow, then `source/build_tvs.py` and `source/extract_shells.py` in Blender background mode. Run `source/install_tvs.py` inside the full Unreal editor, after stopping Play. A legacy FBX importer warning can open the editor message log, so this installation requires initialized editor UI rather than the Python commandlet.

The extraction manifest records both original wall mesh paths, component bounds, and exact polygon conservation. Only the old LCD and rear casing are removed: two connected components totaling 376 faces per wall. All retained surface geometry, UVs, and material assignments are preserved. A pre-edit map backup is held with the task outputs.

## Verification

The opt-in `HouseTelevisionSmoke` campaign test covers both saved actors, fitted collision on all four stages, an LCD crack that stays attached, attachment release and gravity, actual Braxton sauce throws, settling, and one damage count per TV. Evidence is copied to `validation` after completion. It runs only with the explicit test flag, never in normal play.

## Placement correction

The TVs are scaled 1.65 in width and height, with their thin depth preserved. Both brackets touch the projecting fireplace faces (Upper meshes), and their front bolts reach the TV VESA planes. About 10 cm of fitted mount depth clears the existing raised molding. An initial correction that used the outer wall planes was rejected in visual review. Final front and side reviews and weapon checks are recorded in validation.
