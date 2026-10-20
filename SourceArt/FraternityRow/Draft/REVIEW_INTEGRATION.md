# Native Fraternity Row review candidate

`HouseFraternityRowReview.cpp` remains a **SourceArt draft**. No Source/Config/Content file, engine process, map or existing test was changed by this agent. It has not been compiled or run in Unreal. Copy it into the normal SeniorSendoff source module only after the unchanged-scene baseline and parent integration permit it. It has no header/UCLASS or StoryCampaign hook: static `FWorldDelegates::OnWorldPostActorTick` and `FCoreDelegates::OnBeginFrame` registrations are inert unless an explicit review flag is present.

Run each review **alone**, with `-WoodsPassReview` as its evidence dependency, using the project's serialized headless wrapper and a fresh unique run directory. The legacy scene capture **must run first**; every upgraded scene and yard-walk run requires its prior legacy artifact through `-FraternityRowTreeBaseline`. Suggested invocations after compile, not commands executed here:

```bash
bash Saved/CalderWork/tools/run_game.sh frat_row_scene_restore 120 -RenderOffScreen -nosound -FraternityRowReview -WoodsPassReview -NoFraternityRowUpgrade
ROW_TREE_BASELINE="/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/CalderWork/runs/frat_row_scene_restore/Saved/FraternityRowReview/actual_tree_instances.json"
bash Saved/CalderWork/tools/run_game.sh frat_row_scene_after 120 -RenderOffScreen -nosound -FraternityRowReview -WoodsPassReview "-FraternityRowTreeBaseline=$ROW_TREE_BASELINE"
bash Saved/CalderWork/tools/run_game.sh frat_row_scene_lights_off 120 -RenderOffScreen -nosound -FraternityRowReview -WoodsPassReview -NoFraternityRowLights "-FraternityRowTreeBaseline=$ROW_TREE_BASELINE"
bash Saved/CalderWork/tools/run_game.sh frat_row_yards_walk 650 -RenderOffScreen -nosound -FraternityRowYardsWalkReview -WoodsPassReview "-FraternityRowTreeBaseline=$ROW_TREE_BASELINE"
```

The wrapper's timeout is outside the review's600second absolute budget. The scene phase waits10seconds and requires terrain collision readiness. All work is gated to the actual `Chapter01_House` game world and a local authority story pawn/controller. No input is generated during normal gameplay. The walk review includes the scene phase; it cannot pass in legacy mode. Scene-only explicitly reports that it makes no physical-walk claim.

`-NoFraternityRowLights` is an optional **light-cost control**. It suppresses only the six appended real Row point lights. All house/yard geometry, pendant fixtures, emissive windows and live/dead string bulbs remain identical, the parking stays removed, and the shared interior pool stays6. Compare default versus this control in three alternating standard-view performance pairs to isolate the six-light cost; keep the full upgrade-versus-legacy pairs separately for the overall<=5% render-thread gate.

| Flags | Upgrade | `LightsEnabled()` | Actual exterior point/spot total | Exact new Row lights | Original parking heads | Authored glow support points |
|---|---:|---:|---:|---:|---:|---:|
| Default | On | True |57|6|0|6|
| `-NoFraternityRowLights` | On | False |51|0|0|6|
| `-NoFraternityRowUpgrade` | Off | False |53|0|2|0|
| Both switches | Off | False |53|0|2|0|

The review reads `ExteriorLightBudget()` for the exact count and `LightsEnabled()` for the real additions, while retaining `Enabled()` for the geometry/emissive/parking expectations. Thus the cost control must still prove six actual glow support positions even though no real Row lights exist. The full opt-out wins when both flags are supplied. No existing threshold or waypoint is changed for this mode.

## Exact integration names/API

- Permanent colliding shell prefix: `FraternityRowStructure*`.
- Yard structural prefix: `FraternityRowYardsSolid*`.
- Noncolliding decoration/glow prefixes: `FraternityRowDetail*`, `FraternityRowGlow*`, `FraternityRowYardsDecor*`, `FraternityRowYardsGlow*`.
- Eight expected plain ISMs: `FraternityRowCups`, `FraternityRowCans`, `FraternityRowLeavesBrown`, `FraternityRowLeavesGold`, `FraternityRowGrass`, `FraternityRowBrambles`, `FraternityRowSaplings`, `FraternityRowPaper`.
- All permanent components and six new real lights are on `AHouseExteriorDetail`; owner-child exterior light components are counted too, matching the WoodsPass counting convention.
- Five interior IDs are `R1`..`R5`, resolved by `HouseCampusInteriors::SeedFor`. `UCampusInteriorsSubsystem::GetInteriors`, `BuildNow`, `Loaded`, `PendingConversions`, `Stats` are the existing APIs.
- Rear leaves use actual current registry IDs `Campus_R1_RearA/B` through `Campus_R5_RearA/B`; normal `ACampusDoor::Interact(Pawn)` opens their existing groups. The review never changes `bOpen`, lock flags, door transforms or collision.

`Make()` must disable dynamic indirect/distance-field lighting on every decorative/glow and yard-solid prefix, as root has now added in the integration draft. Shadowless/no-collision call arguments alone do not clear those other flags. The structural building prefix can retain ordinary building shadows.

## Scene checks against actual installed geometry

The result fails unless the enabled scene has all five shell floors and20 opaque wall contacts, five shed floors and five shed wall contacts on their own cooked Row components. Opaque-wall rays use `Floor+20`, below the lowest45cm old-house sill (new sills95cm), avoiding real window holes/panes. Every component in the structural batches needs a registered valid physics body and collision. Every small dressing/ISM component must have no collision/shadows/indirect lighting; HISM is rejected, all eight batches must exist with nonzero instances/triangles and distance culling. Yard-solid components must also be shadowless and have indirect/distance-field lighting off.

Four real capsule sweeps must hit closed divider sections. Eight further sweeps check divider openings and eight independent sweeps check those crossings against the **whole world**, including buildings/furniture/trees. The crossing lines use the planned clear part of each opening: the Tudor's front opening atY100 (before its porch), its rear opening atY3500 (past the chair), and the preserved maple's opening atY1900 (around the trunk). This adds Row checks; no preexisting waypoint or test threshold is moved.

Actual point/spot components must total **57 default upgrade /51 upgrade without real Row lights /53 legacy**, with zero casting shadows. Interior pooled components must total exactly6. When `LightsEnabled()` is true, each R3/R4/R5 porch/yard light must match one actual component at `HouseFraternityRow::PorchLight/YardLight` (within0.5cm), shadowless,650lm/700cm or500lm/800cm respectively; when false there must be zero such additions. The R3 porch uses `H.Index==2 ? H.F-450 : 210`; the two new porches useY210. Yard lights coincide with the existing strings' live bulbs: R3 bulb8/18, R4 bulb8/16, R5 bulb6/12, using their exact sag and8cm pendant drop. Every upgraded scene, including the light-cost control, requires a world vertex from an actual installed Row glow batch within12cm of each of the six authored light points, so helpers alone cannot substitute for missing glow geometry. Both old lot lamp-head positions, including rotated lens offsets, must be absent enabled/present legacy. This checks real components rather than only the builder's light counter.

Parking absence requires zero old `ExteriorParkingLot*`/`ExteriorLotProps*`/`ExteriorBushParking*` components, zero old car bounding boxes overlapping the released lot, zero poles at the two removed roots, and zero asphalt triangles whose actual world bounds overlap the released lot/entrance apron. The apron check startsY-4149 so the unchanged road endingY-4150 is preserved. The retained concrete connector needs actual floor support at (-4320,-195,-57),(-3700,-195,-57),(-3480,-195,-57), using the existing3cm tolerance. Keep that connector under an independent non-parking name. Legacy mode explicitly requires old parking parts/cars/poles/asphalt rather than silently skipping the old feature.

The unchanged12 fixed WestField plateau probes use the existing6cm tolerance. The existing six woods-fence sweep positions and original gate crossing are copied without change. In addition, the installed fence's actual world vertices/indices are compared with a pure in-memory call to the unchanged deterministic `HouseWestField::Fence`: section/index counts must match and maximum vertex error must be<0.001cm. That call attaches nothing and consumes only the fence's private random stream. Existing fence source/hash protection is still required by the parent guard.

The original-tree proof requires **the current process's** `Saved/WoodsPassReview/original_trees.csv` written by `RecordOriginalTrees`, created after review-module load (2second filesystem timestamp allowance). It recomputes the exact wide-TCHAR snapshot CRC and requires `0f3b2bb7`. Big/Small roots count as rendered only when the matching `ExteriorTree*`/`CampusFallTree*` component is registered, visible, not hidden in game, its owning actor is not hidden, and its non-null mesh has nonempty render data/LOD0 triangles. Its full world transform must read successfully, contain finite values and a normalized rotation, and have nontrivial scale. Eligible root XY positions match within0.5cm; at most40 may be missing/hidden. Invalid/null/hidden render components cannot silently count as retained. This source-CRC/eligibility/XY-count check alone makes **no claim of unchanged rendered Z/rotation/scale or mesh identity**; upgraded acceptance additionally requires the rendered legacy comparison below.

## Rendered tree control and comparison

Every execution of the scene phase writes `actual_tree_instances.json` before other exterior checks can fail. Its `FraternityRowTreeInstancesV2` schema includes explicit `upgrade_enabled`, canonical ISO8601 `review_loaded_utc` and `captured_utc` provenance. It records all map-tree ISMs recognized by the existing `HouseCalderMapTrees::TreeBatch` catalog: original site and fall trees, distant trees, woods trees, crow trees and accepted deadwood. Newly authored `FraternityRowSaplings` are intentionally outside this original-map preservation comparison. The component array is sorted by component name, full mesh path, owner class and owner name; instances remain in ascending instance index. Each entry includes mesh name/path, owner identity, registration/visibility/hidden flags, render-data/LOD/triangle status, and every instance's full **world location, quaternion and scale**, written as17-significant-digit JSON doubles. Hidden/invalid components are exported too; invalid reads/numerics are flagged instead of being discarded. The file's source CRC/freshness ties it to that process's source-tree snapshot.

First run the post-build `-NoFraternityRowUpgrade` scene capture with this V2 exporter, verify its **result.txt says PASS** and its source CRC equals the pre-edit accepted `0f3b2bb7`. Then use that run's absolute `actual_tree_instances.json` path in `-FraternityRowTreeBaseline=<path>` for upgraded/default, lights-off and yard-walk acceptance. This ordering is mandatory: an enabled upgrade without the flag fails, even if its current tree snapshot exports successfully. This observes the already accepted runtime tree meshes/LODs rather than assuming old replacements' names or shapes. Keep the control in its own unique run directory; rerunning a wrapper name deletes that run's previous evidence.

The reference must explicitly record **`upgrade_enabled=false`**. Its file modification timestamp must be valid and **strictly before this process's `ReviewLoadedUTC`**, without a timestamp allowance. Its canonical captured UTC must be at or after its own module-load UTC and strictly before the current module-load UTC. Thus a same-process capture, upgraded capture, missing provenance or old V1 artifact fails; capture a new legacy V2 control rather than editing an artifact. The report includes the reference file timestamp and current module-load UTC.

When the flag is supplied, comparison is mandatory in either mode: missing/empty/unreadable/malformed baselines, wrong schema/source CRC/provenance, invalid capture, missing rows, changed component/owner/mesh names or paths, changed rendering flags, changed LOD/triangle counts, or changed transforms fail the scene. Exact component count and per-component/total instance counts are required. Position tolerance is0.001cm Euclidean distance, rotation tolerance0.0001degrees, scale tolerance0.000001 maximum component delta. Rotation comparison normalizes the nearly normalized float-backed quaternions and accepts equivalent quaternion signs; it does not relax the angular tolerance. The report records maximum observed deltas and the first mismatch. Current actual state is exported even if comparison fails. Passing the current output as its own baseline fails and writes a separately named current artifact rather than overwriting that reference. A supplied invalid flag cannot fall back to no-comparison mode.

With no baseline flag, only `-NoFraternityRowUpgrade` can proceed, logging `TREE_BASELINE LEGACY_CAPTURE_ONLY; upgrade disabled, rendered TRS/mesh equivalence is not claimed`. Upgraded runs log `TREE_BASELINE FAIL` and still attempt the actual-state export before returning failure. `capture_valid` means the snapshot/transform export itself is valid, not that every other scene acceptance check passed; the parent must still inspect the control's final result. No rendered control has been captured or compared by this drafting agent.

## Real movement walkthrough

There is exactly **one `SetActorLocation` call site**, executed once per yard setup, for five placements total. Each setup is170cm inside its own rear wall at the actual hall floor, after that interior's normal build, complete static conversion and cooked solid-part checks. An overlap test rejects an occupied starting capsule. The normal rear-door interaction must succeed, then waits1.5seconds before movement.

All remaining legs use `AddMovementInput`, ordinary walking mode, the actual pawn capsule and no jump/fly/noclip. For each yard the player:

1. Crosses the rear doorway and top landing, walks down the actual rear stairs.
2. Follows that yard's own authored path.
3. Approaches its shed/garage/lean-to, crosses the actual opening, stands140cm inside, then exits by that opening.
4. Walks around the outbuilding's side rather than diagonally through its wall, reaches its own Y4400 fence approach.
5. Returns along its yard path, climbs the rear steps and crosses back inside its house.

Doors encountered again within250cm use the same normal interaction if they have closed; no forced state change. Missing/locked/inconsistent rear pairs fail explicitly. Each leg fails after7seconds without at least4cm of new progress or30seconds total. The target needs32cm horizontal /8cm height accuracy. Every0.2seconds an actual floor trace is recorded; a missing/unwalkable floor, feet below it by>8cm, or gap>45cm fails. Floor traces use known house/stair/shed floor bounds so a shed roof cannot substitute for a missing floor. Unrelated existing WestField/Parking/house/interior reviews still run separately with their unchanged paths/tolerances.

Success requires exactly5 completed routes,5 setup placements and15 saved screenshots (shed interior, fence approach, return interior per house). Files are `Saved/FraternityRowReview/result.txt` or `Saved/FraternityRowYardsWalkReview/result.txt`, `feet.csv`, and the walk captures. The review exits with0 only on pass and1 on failure. Missing floor/component/light/CRC, blocked path, timeout or missing screenshot cannot produce a pass.

## Current verification state

Only source inspection/basic structural checks have been performed. The current candidate has one starting-placement call site and balanced source braces. No native compile, screenshot, walk, tree proof, physical sweep or performance result has been produced here. Parent owns compile, adversarial review, native execution and any fixes justified by observed failures. Do not mark runtime acceptance complete from this draft.
