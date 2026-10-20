# Fraternity Row status

**Paused at Stewart’s request so the stadium can take priority. Row is unfinished.**

Updated 2026-10-19 03:51 UTC. Runtime evidence prefix: `row_after_20261018_r7`.

The three existing houses are walk-in houses with distinct dark, tidy and wrecked-party states. A Tudor occupies the existing grass, and a Queen Anne with a turret and connected parlors uses the authorized former parking area. Each has a separate backyard, an accessible outbuilding and a path to the original woods fence. The setting is Homecoming night in October 1986.

The Queen Anne was moved forward after image review found real branches inside its original rooms. Its revised complete envelope clears the original canopies; trees were preserved. Window-mounted displays and shelving were corrected to use opaque backing, with supported brackets. Current coordinates and routes are in [PLAN.md](PLAN.md) and [PLAN.json](PLAN.json). The amended plan was shown before rebuilding.

## Scope and budgets

- Existing map bounds, road, terrain grade, woods fence/gate, main house/rooms, court, Fall Jam, east driveway, story and movement stay in place.
- West main-house parking pavement, bay furniture/cars and two parking lights are removed under Stewart's explicit instruction. The old parking walk and main-house connection retain their coordinates.
- Exterior light budgets: baseline 53, revised 57 = 53−2+6; lights-off control 51; shared interior pool 6. The six new points are small and shadowless.
- Original tree CRC `0f3b2bb7`; current native controls compare all 1600 rendered transforms exactly. Hidden original A/B count 26 is below the unchanged 40 limit. The canopy-envelope check reports zero overlap.
- No map save or commit is authorized. Final Source/Config/project/map and recovery-copy verification remains pending.

## Current native acceptance

| Phase | Current source | Evidence |
|---|---|---|
| Legacy / upgrade / six-lights-off scene controls | PASS | [row_after_20261018_r7_scenes.json](</Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/FraternityRowWork/row_after_20261018_r7_scenes.json>) |
| All five Row room smokes | Pending | [row_after_20261018_r7_smoke.json](</Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/FraternityRowWork/row_after_20261018_r7_smoke.json>) |
| All five physical interior routes and natural unloading | Pending | [row_after_20261018_r7_interiors.json](</Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/FraternityRowWork/row_after_20261018_r7_interiors.json>) |
| Five yards, sheds, fence approaches and return indoors | Pending | [row_after_20261018_r7_yards.json](</Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/FraternityRowWork/row_after_20261018_r7_yards.json>) |
| 16 upgraded dusk +16 dark views | PASS | [row_after_20261018_r7_captures.json](</Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/FraternityRowWork/row_after_20261018_r7_captures.json>) |
| Two added poses at legacy dusk +dark | Pending | [row_after_20261018_r7_lot_controls.json](</Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/FraternityRowWork/row_after_20261018_r7_lot_controls.json>) |
| Full existing campus, terrain, outdoor and house regression chain | Pending | [row_after_20261018_r7_regressions.json](</Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/FraternityRowWork/row_after_20261018_r7_regressions.json>) |
| Five Row host/client pairs +original library hill walk | Incomplete: R1/R2 pass, R3 host crash; remaining tests not run | [row_after_20261018_r7_mp.json](</Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/FraternityRowWork/row_after_20261018_r7_mp.json>) |
| Full-upgrade and six-light A/B:12 native runs | Pending | [row_after_20261018_r7_perf.json](</Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/FraternityRowWork/row_after_20261018_r7_perf.json>) |
| Manual review: all 136 required images and current review documents | Pending | [row_after_20261018_r7_visuals.json](</Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/FraternityRowWork/row_after_20261018_r7_visuals.json>) |

Source-derived arithmetic, an exited launcher, and historical images are not used as native passes. The unfinished checks remain pending. Stewart’s explicit pause permits transfer to the stadium after a verified pause snapshot and clean engine shutdown; this transfer does not declare Row acceptance.

The unchanged before run had three existing failures. Final regression acceptance requires a pass or the same sole native verdict; no threshold or original waypoint is loosened:

- Exterior walk: front site surface material at(800,-4025), Z−57.
- Combined-house smoke: mouse-look direction, step2, pitch0.
- Original-house door smoke: missing extracted original leaf asset.

The before campus groups passed431/431 stops across18 buildings; terrain, FreeRoam, old parking/woods/field walks, tree interactions,18 interior smokes and house traversal also passed. Exact before lines are in [BEFORE_CRITIQUE.md](BEFORE_CRITIQUE.md) and the before-check receipt. These historical results do not replace the current-source phases.

## Images

The original14 Row poses are identical before/after. Two new Queen Anne lot poses were appended after all existing Row and stadium cameras; matching legacy controls supply their comparison. Nightfall is pinned to0 for dusk and1800 for dark. Older October12 images are historical and excluded.

Before, dusk: current-source capture pending.

![After, dusk](</Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/CalderWork/runs/row_after_20261018_r7_dusk/Saved/HouseExteriorViews/277_detail_row_R5_lot_front.png>)

Before, dark: current-source capture pending.

![After, dark](</Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/CalderWork/runs/row_after_20261018_r7_dark/Saved/HouseExteriorViews/277_detail_row_R5_lot_front.png>)

## Performance and recovery

[PERFORMANCE.md](PERFORMANCE.md) reports actual `render_ms` per standard view, with baseline ranges, before/after, full-upgrade A/B and six-light A/B. Every standard view must remain within+5%; incomplete or over-budget evidence does not pass.

[ROW_IMPLEMENTATION.patch](ROW_IMPLEMENTATION.patch) contains only Row-owned runtime changes against captured bytes after both owners' camera instrumentation. [ROW_CAMERAS.patch](ROW_CAMERAS.patch) separately contains the original 14 Row cameras. Recovery creation verifies both patches by applying them to isolated copies and comparing exact resulting bytes.

The current R7 owned patches and a verified pause snapshot are saved for continuation under `Saved/FraternityRowWork/row_pause_for_stadium_manifest.json`. They do not constitute the strict final-acceptance checkpoint; all unfinished native and manual phases remain pending.

No Fab downloads are required. Existing project kits/materials and runtime geometry are used.
