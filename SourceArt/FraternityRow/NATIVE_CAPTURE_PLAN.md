# Native baseline and validation queue — not executed

**Do not run while wildlife owns Source/Config or the engine.** Obtain the owner's explicit release, then satisfy the brief's 20-minute idle condition. Confirm no engine job is active using process names only; do not print full argv, which can contain Epic credentials. Do not infer permission from a temporarily absent process during a multi-run suite.

All runs use the existing checkout and wrappers, `-RenderOffScreen` and normally `-nosound`. Unique run tags must not collide with wildlife work or historical results. Never run the old `SourceArt/FraternityRow/run.py` or `finish.py`: the former is visible/windowed and the latter uses editor map operations. Latest scope update: **west main-house parking removal is authorized**, superseding its earlier protection. Capture it before removal; read `PARKING_REMOVAL.md`.

## Before Row geometry changes

1. Capture the source/map hashes and current native review verdicts. No Row implementation is integrated yet.
2. Run the existing performance helper with both configurations unchanged, for six baseline observations per standard view:

   ```bash
   bash Saved/CalderWork/tools/perf_ab.sh row_before_20261018_v1
   ```

   The helper always supplies `-RenderOffScreen -MetalOffscreenOnly -nosound`. Verify all six `frametimes.csv` files exist and contain all eleven standard views. Its printed delta is frame time; retain the `render_ms` column for the render-thread budget. The repeated baseline also reveals measurement variability.
3. Append only the new `detail_row_*` camera definitions below at the **end** of `ExViews` in `HouseExterior.cpp`, under the existing detail-view rules. This is capture instrumentation, before any geometry upgrade. Do not move or rename old views or change planting exclusions. Build once with `Saved/CalderWork/tools/build.sh`, then capture these before views at recorded dusk and dark settings.

   ```cpp
   { TEXT("detail_row_road_wide"), FVector(-14300,-4800,140), FVector(-14100,-1400,350), 78.f },
   { TEXT("detail_row_R1_porch"), FVector(-17400,-3650,120), FVector(-17400,-2400,260), 72.f },
   { TEXT("detail_row_R2_porch"), FVector(-13900,-3650,120), FVector(-13900,-2400,260), 72.f },
   { TEXT("detail_row_R3_porch"), FVector(-10100,-3650,120), FVector(-10100,-2400,260), 72.f },
   { TEXT("detail_row_R1_yard"), FVector(-17500,3400,20), FVector(-17400,800,140), 74.f },
   { TEXT("detail_row_R2_yard"), FVector(-13900,3400,20), FVector(-13900,800,140), 74.f },
   { TEXT("detail_row_R3_yard"), FVector(-10400,3400,20), FVector(-10100,800,140), 74.f },
   { TEXT("detail_row_infill"), FVector(-7700,-500,80), FVector(-6000,1150,260), 78.f },
   { TEXT("detail_row_maple_gap"), FVector(-6450,2200,20), FVector(-6215,1596,160), 70.f },
   { TEXT("detail_row_oak_edge"), FVector(-4100,2300,20), FVector(-5300,1200,260), 70.f },
   { TEXT("detail_row_rear_fence"), FVector(-12000,4200,20), FVector(-12000,3000,130), 78.f },
   { TEXT("detail_row_gate"), FVector(-7050,4100,20), FVector(-7050,5000,20), 72.f },
   { TEXT("detail_row_former_parking"), FVector(-7300,-2000,140), FVector(-5200,-1000,120), 76.f },
   { TEXT("detail_row_house_connection"), FVector(-4600,-195,130), FVector(-3480,-195,100), 70.f },
   ```

   These are proposed world-space camera poses, not new acceptance waypoints. Check camera height/occlusion against the current scene before committing the comparison set; if a camera must move, update both before and after poses and retain that fact. Never change the fixed review walk route.
4. Dusk uses current lighting defaults. A darker comparison can use the existing `DuskSun`, `DuskBelow`, `DuskSky`, `DuskSkyMin`, `DuskGlow` and `DuskEV` controls; inspect their semantics first and record all chosen values. Do not treat `-NoDusk` as proof of a natural elapsed-time transition. The Row job does not add a global day/night system.

## Staged integration after baseline

Keep an opt-out `-NoFraternityRowUpgrade` restoring the original three-house scenic path, **original west parking lot and its vehicles/props**, original tree exclusions, and old light count for cost comparison. The switch must also exclude the five new interior definitions and their doors/lights; an exterior-only visibility toggle is not an adequate A/B.

Suggested owned source areas, subject to a fresh source hash/diff check after the freeze:

- `HouseFraternityRow.cpp/.h`: preserve original default geometry behind the A/B path, add five-house descriptors, distinct shells/porches/signs and new solid/decor/glow geometry.
- New `HouseFraternityRowYards.cpp/.h`: parcels, outbuildings, ground-following wear, independent seeded dressing and instancing.
- New `HouseCampusInteriorsRow.cpp`: five free-standing definitions, room kit, light anchors, door specs, interior material/sign setup and indoor bounds.
- Small integration in `HouseExterior.cpp` and campus interior registry/room declarations. Append new definitions after existing registry entries to preserve seeds.
- Interiors walk/smoke and a Row-specific review: five interiors, solid shells, solid outbuildings/fences, traversable steps, no-collision litter/grass, exterior light count and original trees. Preserve legacy review coverage and add explicit new coverage.
- Scoped west-parking integration in `HouseParkingLot*`, `HouseParkingDetail*`, `HouseExterior.cpp` and `HouseExteriorNature.cpp` is now authorized as needed. Updated feature-mode review asserts west parking/cars/bay paint/curbs/signs absent, current-grade ground supported and all exact old pedestrian route coordinates still walkable. Legacy mode retains original parking checks. No broad deletion or edit of protected house/east-driveway code.
- Only the agreed exterior light expectation changes in WoodsPass or other relevant count reviews; no threshold or waypoint changes.

Do not edit wildlife, protected main-house/court/FallJam/room files or east driveway, StoryCampaign, player movement, terrain/fence/road functions, Config or Content. The previous west-parking prohibition is superseded by the user's removal instruction. No map saves or commits.

## Required verification after each substantive integration

Adversarial review first; then serialize a build and focused native checks. The final suite must include:

- Existing campus interiors walkthrough and smoke, extended to Row IDs and all new rooms; existing buildings retain verdicts.
- FreeRoam, CampusLayoutReview and CalderTerrainReview.
- Surroundings, FraternityBoundary, ParkingLot and WoodsPass reviews.
- For the explicitly removed parking feature, replace obsolete presence assertions with absence/support/access assertions in upgraded mode; retain the same walking waypoints and tolerances, keep the old assertions in legacy mode, and identify the change in the report. Do not skip the parking walk or call an inapplicable old lot-presence check a pass.
- WestField geometry and its exact existing walk; Woods reviews and woods walk.
- House smokes; host/client multiplayer, including replicated Row door interaction from both sides and streaming/reentry.
- A new physical player route per house: sidewalk -> porch -> entry -> every room -> rear exit -> own yard -> outbuilding -> fence -> return. Sweep and walk; do not substitute camera teleporting for traversal.
- Dusk/dark before/after comparisons from identical retained cameras. Inspect stair headroom, door swings, roof/wall seams, terrain contacts, tree canopy clipping, period details and night legibility.
- Original tree transform CRC `0f3b2bb7`, shared tree meshes unchanged, <=40 hidden original TreeA/TreeB, six shared interior lights, <=8 additional shadowless exterior lights (proposal:6).

After integration:

```bash
bash Saved/CalderWork/tools/perf_ab.sh row_after_20261018_v1 -NoFraternityRowUpgrade
```

Require complete 3-on/3-off CSVs. Show per-view median `render_ms`, before-to-after delta/percent, and after-on versus after-off. No standard view may exceed the agreed +5% render-thread cost without showing Stewart the numbers. Investigate thermal/noise variation rather than declaring a pass from a single run or from frame-time delta alone.

Finish `STATUS.md`, add actual evidence paths, generate an owned-files implementation patch against captured pre-edit bytes (not just Git HEAD, which contains unrelated local changes), and leave commits to Stewart.
