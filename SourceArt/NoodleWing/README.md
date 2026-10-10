# East wing: Noodle reference build

Editable Unreal project: `SeniorSendoff.uproject` in this checkout.
Saved level: `/Game/Story/Maps/Chapter01_House`.

## Scope and entrances

- Red billiards room, navy TV lounge, laundry, and service halls behind the existing basement flat white door (`OriginalDoor_Basement_m1_p1_Solid_01`).
- Three-floor stairwell accessed from the basement black-block hallway pushbar exit, a new main-floor opening in the gray/taupe wall, and the existing upstairs end door (`OriginalDoor_Upper_m1_p1_Solid_05`).
- Room shells are fitted to the existing house envelope and floor heights. Reference furniture placement and appearance follow the photos/scans; this is not a survey-accurate reconstruction.
- Existing rooms, club, original stairwell, and exterior generation are outside the edit scope.

## References

All fourteen user photos IMG_2387 through IMG_2400 were inspected. Noodle scan photographs and the three unique GLBs were inspected for the floor plan and stair construction. Local reference copies and plan previews are in `references/`, `scans/`, and `context/`.

## Implementation

`build_art.py` builds the modular meshes, materials, fixtures, and furniture manifest. `build_cuts.py` clips only the openings/shaft from original architecture into separate retained-mesh assets. Original mesh assets are preserved.

`import_art.py` imports art. `install.py` installs the wing idempotently using the `NW_` actor prefix, and verifies that actors outside its explicit replacement list stay unchanged before saving. `finalize.py` runs both steps. `fix_doors.py` installs moving panel collision for the two original entrance leaves.

Assets live in `/Game/SeniorSendOff/House/NoodleWing`. The map contains 122 new actors: 102 placements, 18 localized lights, and 2 new doors. The new architecture uses complex collision; small decorative surfaces do not block walking. Furniture uses the existing interactive prop system. Main and laundry doors use the existing replicated door system.

`HouseBlackHallBuild.cpp` omits three obsolete dead-end bounds and their duplicate slab only when the saved Noodle stairwell tag exists. Maps without the extension retain the original behavior.

## Validation

`HouseNoodleWingReview.cpp` is an opt-in non-shipping runtime check (`-NoodleWingSmoke`). It drives the actual player movement through the new rooms and both directions along all four stair flights, including the main and upstairs thresholds. It does not use recovery teleports on walking routes. Separate door checks position the player within normal interaction range, close each of five entrance doors, and trace its blocking collision.

`run_walk.py` runs that check; `run_views.py` captures ten views from the live game. Test quality settings and saves are isolated in temporary user directories. Logs, result files and screenshots are retained under `evidence/`. A zero process exit code alone is not accepted as a test pass; the explicit success marker is required.

The pre-edit map backup is `backup/Chapter01_House_before_noodle.umap`. Preserve it and the reference source geometry if revising the extension.

## Stairwell revision — 2026-10-10

The user requested a complete replacement of the stair layout to match the opposite stairwell and IMG_2387–2392. The perimeter side galleries are removed. Four paired steel flights now meet full-width north floor landings and south turning landings. The existing three entrance locations and adjacent room shells are retained.

The revised flight widths are 248–263 cm in the enlarged house, with shorter tread runs. The lower run has 13 + 9 equal-height risers to preserve 201.9 cm beneath its turning landing. The main-to-upper run has 10 + 10 equal-height risers. The upper exposed edge has a full guard. The wall lining covers the old protruding trim, and the retained doors have flush jamb returns. The new gray treads and basement concrete reuse the other staircase's existing materials. Yellow masonry, closed steel risers, rounded handrails, wall brackets, fluorescent fittings, conduit, and a ceiling access hatch follow the references.

`stair_layout.py` contains the geometry, also embedded in `build_art.py` for full reproducibility. `build_stair_revision.py` regenerates only the three stair meshes. `install_stair_revision.py` imports those meshes, adjusts the stair practical lights, adds one basement-foot light, and preserves all other actors. The installation audit confirms 1,940 existing actors stayed unchanged. The stairwell has six real lights across three floors, with no new tick-driven behavior.

The backup immediately before this revision is `backup/stair-revision-20261010/`. The earlier pre-wing backup is also retained.

## Paired-flight revision validation

`evidence/stair_walk_final/result.txt`: PASS. The actual player walked all 49 route targets, up and down all four flights, through the three floor entrances, and through the preserved basement rooms. No recovery teleports were used on walking routes. Five separate closed-door collision checks and the upper-edge guard sweep passed.

The opt-in test now stops movement for 0.8 seconds after interacting with a door, allowing its normal swing to finish. The first attempt walked into the upstairs leaf during its swing; that failed run is retained as `stair_walk_v1`. Production door behavior was not changed.

Final native-game views are in `evidence/stair_final_views/`. These use isolated quality-2 test settings; no whole-house performance comparison or multiplayer session was performed for this geometry-only revision.

## Basement entrance orientation — 2026-10-10

The first basement flight now faces the existing west entrance door. Six risers lead straight ahead to a quarter-turn landing, followed by fourteen risers north to the existing main-floor landing. All twenty basement risers are 18.0983 cm high. The player no longer needs to cross beneath the stairs to reach the first tread. The upper pair, all three door positions, and neighboring room layouts are retained.

The lower handrails follow the new entrance and turn. Existing stair lights were repositioned without adding lights. The scoped saved-map audit again preserves 1,940 other actors. A fresh backup is retained at `backup/basement-facing-20261010-004703/`.

Latest walking result: `evidence/basement_facing_walk/result.txt` PASS. The native player walked the direct entrance flight and the full three-floor route in both directions; 42 route targets, five closed-door collision checks, and the upper guard collision passed. This was an automated game run, not a multiplayer test or whole-house performance comparison. New screenshot evidence is in `evidence/basement_facing_views/`.

## Basement T junction — 2026-10-10

The red and blue room entrances now face each other on opposite ends of the mini-hall junction. Entering from the house, the red room is left and the blue lounge is right. Both openings share x=2458, have 184 cm rough widths and 235 cm heights, and use lined ivory jambs and casings. The earlier east-facing opening in the red room is filled in; the hall-facing surfaces stay cream. Open doorways follow IMG_2395, IMG_2400, and the basement Noodle scan.

`junction_layout.py` is the common source used by the full build and `build_junction_revision.py`. Only RedWalls, BlueWalls, HallWalls and BasementTrim were reimported by `install_junction_revision.py`. Room envelopes, floors, furniture, existing door actors, all lights, and the rebuilt stairs are preserved. The saved-map audit protects 1,943 other actors, with no added actors or real lights. The immediate backup is `backup/t-junction-20261010-011216/`.

`evidence/junction_walk/result.txt`: PASS. The actual player walked the new red and blue entrances in both directions, continued to the laundry, and walked the preserved three-floor staircase both ways. All 46 targets, five existing closed-door collision checks, and the upstairs guard check passed. This was an automated native-game check, not a multiplayer or whole-house performance test. Junction views are captured separately with `junction_views.json`.


## Basement entrance header repair (2026-10-10)

The original house doorway wall ended at z=-97.966 while the mini hall ceiling is z=-54. The HallWalls mesh now fills that overhead void, restores the head jamb removed by the earlier access cut, and includes an ivory surround. The original leaf has 1.266 cm of clearance below the new z=-134.1 head. A continuous cream face closes the header seam. Hall_Exit now faces the mini hall and sits against the wall.

Only NW_HallWalls and NW_Hall_Exit were changed; 1,945 other saved actors and the map's 1,947 actor total were preserved. No lights were added and no shadow settings were changed. build_entrance_revision.py rebuilds this one wall asset; the canonical full art build also consumes the corrected junction_layout.py and sign transform.

The final native entrance_walk passed movement through the entry both ways, all five entrance door closure/collision checks, and the upper stair guard. Both entrance_views screenshots were visually inspected. Reports and screenshots are in evidence/entrance-verification.json, evidence/entrance_walk/, and evidence/entrance_views/. The existing VSM queue warning remains separate from this geometry repair.
