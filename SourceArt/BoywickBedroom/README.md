# Boy Wick bedroom photo rebuild

Installed in `/Game/Story/Maps/Chapter01_House` using the six original room photos in `references/`.

## Layout

The one steel loft-bed unit occupies the far-left corner when entering from the hallway. The lower dark pad is seating within the same frame. Both open closets occupy the entrance wall. The gray futon, cluttered coffee table, white desk, green chair, mini fridge, wire rack, dresser/TV, laundry, shoes and wall decorations follow the photographed arrangement. Object dimensions are estimated from the photos and fitted to the existing house horizontal scale of 1.8; this is not a measured survey. The room envelope, adjacent rooms, hallway door and existing vent are retained.

The replacement has 76 authored mesh assets, 119 placed room objects, two local lights, and lightweight runtime string lights. 110 placed objects use the existing furniture interaction system. Large furnishings have three mesh damage stages. Small items remain independent and ignore player capsules while retaining floor/furniture collision and interaction targeting.

## Assets and editing

Assets live under `/Game/SeniorSendOff/House/BoywickBedroom`. Room actors carry `BoywickPhotoRoom` and use the `BWR_` label prefix. The previous `BW_` pieces are replaced. Original photos are copied unchanged; wall art uses mesh UV coordinates to sample the photographs. Unrelated actors are checked before saving, including their transforms, meshes, materials and actual collision responses.

`manifest.json` is the final saved placement/asset manifest. `source/install_room.py` installs it through the full Unreal editor Python environment. Use the full editor for FBX imports: the commandlet crashes when the FBX importer attempts to display a smoothing warning without Slate.

Authoring order for a clean rebuild is `build_room.py`, `build_damage.py`, `build_details.py`, `finalize_layout.py`, `refine_details.py`, then `build_damage.py` again, using Blender background Python for geometry. `build_details.py` is intended only after a fresh base build. `finalize_layout.py` and refinement placement changes are guarded against repeated application. Preserve `manifest.json` when manually adjusting placements in the editor.

## Validation

- `evidence/install8.log` / `installed.json`: saved room, all 1,675 unrelated actor records preserved.
- `evidence/build_native3.log`: successful Mac Development editor build.
- `evidence/preview5.log`: all six player-capsule route segments clear, 110 interactive props, 94 small props with correct pawn response, native Boy Wick ready.
- `evidence/preview5/Saved/HouseVisualReview/`: five inspected views captured from the actual campaign map.
- `evidence/smoke1.log`: passed NPC conversation, native face animation, temporary speed/jump/might abilities, expiry and recovery.

The review cameras and collision sweeps are assisted diagnostics, not a recorded manual traversal. Backups of the original map and touched C++ files are in `before/`.
