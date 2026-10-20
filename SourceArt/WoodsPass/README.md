# Appalachian woods pass — Senior Sendoff

Target: Unreal Engine 5.8 on Mac, runtime map `/Game/Story/Maps/Chapter01_House`.

This extends the existing rear woods rather than replacing their terrain, paths,
tree grid, dusk lighting, or Coles Quarters. New scenery is created in code when
the exterior loads. Nothing is placed or saved into the map.

## Status

- Pass 1, forest structure and floor: implemented and rendered. The native
  `pass1_polished` run exited successfully and its static scene checks passed.
- Pass 2, fog, darkness and silhouettes: implemented; six-view capture complete.
  Final low-pool ravine capture passed and was reviewed/shown before pass 3.
- Pass 3, human traces, motion and sound: **pending implementation/verification**.
- Pass 4, final performance comparison and complete route review set: **pending**.

The sections marked pending are not completion claims. This document will be
updated after those passes have their own native screenshots and review evidence.

## What pass 1 adds

### Forest structure

`Source/SeniorSendoff/HouseWoodsStructure.h/.cpp` creates additive oak, hickory,
maple, tulip-poplar and occasional hemlock tree batches. Trees have crooked and
leaning trunks, independent branch shapes, thickness variation, sparse October
foliage and a few large old oaks. Dogwood saplings and laurel thickets fill the
middle layer. Density increases deeper into the woods.

The current seed produces 336 additional trees, including four large old oaks,
and 532 understory plants. These are additional instances, not replacements for
the original trees. Trees use three actual runtime mesh LODs; understory uses two.
Only the new tree trunks have collision. Leaves, branches and understory do not
add obstructing collision or shadow/indirect-light updates.

### Forest floor

`Source/SeniorSendoff/HouseWoodsFloor.h/.cpp` creates a 1024-pixel wrapping leaf
litter color texture and relief-derived normal map entirely on the CPU. The
painted leaves have original oak, hickory, maple and tulip-poplar masks, veins,
curled edges, overlapping layers and a dark soil bed. Only the woodland ground
material, `EX_WoodsGround`, is replaced; the house lawn and existing trail master
remain unchanged.

Additional instanced detail in the current pass-one run:

- 1,956 local patches of small curled leaves.
- 655 brown fern clusters and 149 thorny bramble clusters.
- 22 broken, hollow, rotting stumps.
- 325 rocks, including larger mossy outcrops and 273 small creek stones.
- Terrain-following root extensions on up to 130 existing trees.
- North-facing moss on 28 existing near-tree trunks. It samples the exact
  original trunk ring positions, leaned frames, radial ridges and triangle
  diagonals, then offsets its irregular patches 0.20 world centimetres outward.

Rock moss remains north-facing; nonuniform scale supplies shape variation
without randomizing its north direction. Roots, trunk moss and creek/deer-track
surface geometry are spatially divided into 40-metre cells with local origins,
so distance culling does not incorrectly measure from world zero.

The 15,997 existing woodland grass instances keep their original counts,
positions and rotations. Their material is replaced with a brown code-painted
blade, and their local height is reduced to 15–30% using deterministic position
noise. There are 321 occasional 55%-height tufts in the current seed. Existing
woodland flowers retain their geometry/counts but receive dry brown colors.
This exposes the leaf floor rather than covering it with uniformly tall straw.

All new floor pieces are cosmetic, nonblocking, unshadowed and excluded from
distance-field/dynamic indirect-light updates.

## Locations and protected space

Coordinates below are native world centimetres, not metres or JSON-review units.

- New tree grid: roughly x −7500..7600, y 5110..18300, further reduced by crown,
  route, hollow, clearing and boundary margins.
- Understory: x −7580..7580, y 5120..18150, clustered by position noise.
- Floor detail: x −7800..7800, y 4901..18600, excluding the original trail and hut
  approach. A 200cm allowance protects the west-gate corridor at x < −6200,
  y < 6800 from the extents of roots, ferns and rocks.
- Dry creek: winds around the existing hollow at (−4200,8000), from approximately
  y6960 to y9126. It uses gravel, exposed soil and the **existing** recessed
  terrain. No ground height, tree height or collision floor is changed.
- Faint deer trace: leaves the main trail near (−3530,6640), winds toward roughly
  (−5530,8840), then narrows and breaks up. It is a cosmetic surface trace, not a
  replacement or obstruction for the original walking route.
- Original woods trail and Coles Quarters at (−800,13600) remain protected.
- The party field, drive, court, rear lot, parking/accessibility routes, house
  perimeter and property collision boundaries are outside the new placement
  areas or explicitly excluded.

`HouseWoods::Height`, the original hollow definitions and the original trail
curve are not modified. There is no map save and no new point/spot light.

## Determinism and original-tree safety

The original `FExSite::Plants` RNG stream is not used by any new scatter. The
original tree arrays are recorded before additions by `HouseWoodsPass`:

- Big trees: 134.
- Small trees: 136.
- Distant trees: 852.
- Current original-tree CRC: `0f3b2bb7`.

`baseline_stable/Saved/WoodsPassReview/original_trees.csv` and the corresponding
`pass1_polished` file compare byte-for-byte equal. New camera names begin with
`detail_woods_`, which the existing planting camera-exclusion code ignores, so
adding views does not perturb the old tree layout.

The new files use named `Impl` namespaces and no using-directives. Compilation
and final warning review remain part of the final evidence set, not an assertion
that all pre-existing source files follow these conventions.

## Tuning

| Area | Location / parameter | Current setting |
| --- | --- | --- |
| Stage switch | `HouseWoodsPass::Stage`, `-WoodsPass=N` | 0 baseline; 1 structure/floor; 2 atmosphere; 3 traces/motion |
| Tree scatter seed | `HouseWoodsStructureImpl::ScatterSeed` | 2026101307 |
| Understory seed | `HouseWoodsStructure::Build` | 2026101329 |
| Tree grid spacing | `HouseWoodsStructure::Build` | 580cm columns, 530cm rows |
| Depth density | `HouseWoodsStructure::Build` | acceptance 0.57 → 0.89 |
| New species mix | tree `Kind` thresholds | oak31%, hickory21%, maple21%, poplar18%, hemlock9% before placement rejection |
| Understory attempts | `HouseWoodsStructure::Build` | 1100 before exclusions |
| Tree cull | tree `AddBatch` calls | 22500cm; three LODs |
| Understory cull | laurel / dogwood batches | 6400 / 7800cm; two LODs |
| Floor seed | `HouseWoodsFloorImpl::FloorSeed` | 202610131 |
| Floor scatter stream | `FloorSeed + 101` | independent of all old RNG streams |
| Leaf ground texture | `TextureSize`, `PaintFloor` | 1024²; 1900 painted overlapping leaves |
| Floor detail attempts | `HouseWoodsFloor::Build` | 2100 before exclusions |
| Floor culls | litter / fern / bramble / stones / stumps | 2400 / 4500 / 5200 / 6000 / 6500cm |
| Root/moss cells | `SpatialGeometry` | 4000cm cells, 6500cm cull |
| Dried old grass | `DryExistingFoliage` | ordinary Z×0.15–0.30; occasional Z×0.55 |

Changing new seeds changes only the additions. Keep placement exclusions,
original-tree recording and the existing review thresholds intact when tuning.
Pass-two/three-specific controls will be documented after implementation.

## Six native dusk views

The views are in `HouseExterior.cpp`'s `ExViews` table:

| View | Camera position | Purpose |
| --- | --- | --- |
| `detail_woods_edge` | (−2450,4320,−190) | Threshold from the lawn/court edge |
| `detail_woods_20m` | (−3310,6550,−195) | About 20 m into the woods |
| `detail_woods_deep` | (−1230,11000,−192) | Deeper canopy and ground layers |
| `detail_woods_ravine` | (−4300,7700,−200) | Existing hollow and new creek detail; target (−4100,8150,−438) |
| `detail_woods_silhouette` | (−2720,9330,−164) | Branches against the remaining sky |
| `detail_woods_house_back` | (−3350,6600,−182) | House seen through intervening trees |

They use the existing dusk look; no separate showcase lighting is used.
The ravine camera was revised during pass 2. Final paired stage-0/final
performance runs must both use this revised camera; the preliminary older
ravine measurements are not a like-for-like comparison with it.

## Reproducing native reviews

Build the native `SeniorSendoffEditor` target first. The runner does not build or
save the map. From the Unreal project root:

```bash
python3 SourceArt/WoodsPass/run_review.py baseline_new --pass-number 0
python3 SourceArt/WoodsPass/run_review.py pass1_new --pass-number 1
python3 SourceArt/WoodsPass/run_review.py parking_new --pass-number 1 --kind parking
python3 SourceArt/WoodsPass/run_review.py woods_walk_new --pass-number 1 --kind woods
```

Use a fresh run name every time: old evidence is deliberately not overwritten.
Use `--filter detail_woods_ravine` for a single camera, or `--timeout 600` for a
longer process deadline. Later completed stages can be selected with
`--pass-number 2` or `3`; selecting a number by itself is not proof that a pending
stage has been implemented or tested.

The script launches the actual campaign map at 1280×720, all quality groups 2,
100% screen percentage, uncapped FPS and VSync off. It uses a dedicated
`-UserDir` inside each evidence directory, with `-AbsLog` and invocation metadata.
Camera captures settle for 5 s per view and sample 120 frames. Keep the same
settings and comparable machine load for before/after timing.

`--kind views` combines only static checks with the camera runner:

- `-CampusWoodsReview -FraternityBoundaryReview` — the latter requires the former.
- `-WoodsSceneReview -WoodsUndergrowthReview` — undergrowth is an extra scene check,
  not an independent runner.

`--kind parking` instead runs the actual `-ParkingLotWalkReview`, plus static
checks. `--kind woods` runs the actual `-WoodsWalkReview -WoodsUndergrowthReview`.
Do not combine camera captures with walking reviews or combine two walking
runners: they both control the pawn.

The runner records process completion in `invocation.json`; an exit code alone
is not sufficient evidence. Inspect the expected PASS markers and screenshots.

## Evidence recorded so far

All paths in this section are relative to this directory.

- `protected.json`: protected project/map, pre-pass map hash, light limit and
  routes. `backup/` contains the pre-pass copies of `HouseExterior.cpp`,
  `HouseDuskAtmosphere.cpp`, `HouseLeafWind.h/.cpp`, `HouseExteriorNature.h/.cpp`,
  and `HouseWoodsDetail.h/.cpp`. These preserve the already-dirty starting state;
  they are not a clean Git baseline and must not overwrite unrelated user work.
- `evidence/baseline/`: initial pre-addition six-camera run.
- `evidence/baseline_stable/`: stage0 comparison run using5s settling and120 samples.
- `evidence/pass1_structure_floor/`: first-pass development images and metrics.
- `evidence/pass1_polished/`: pass-one polished native images, metrics and checks.

Inside each completed camera run:

- `Saved/HouseExteriorViews/00_detail_woods_edge.png` through
  `05_detail_woods_house_back.png`: actual native screenshots.
- `Saved/HouseExteriorViews/frametimes.csv`: `avg_ms,max_ms,game_ms,render_ms` by view.
- `runtime.log`: `HOUSE_EXTERIOR_FPS`, generated detail counts, light count,
  original-tree CRC and review markers.
- `Saved/WoodsPassReview/original_trees.csv`: original transform snapshot.
- `Saved/CampusWoodsReview/result.txt`, `Saved/FraternityBoundaryReview/result.txt`
  and `Saved/WoodsDetailReview/result.txt`: static review details.

The `pass1_polished` run records 53 exterior lights, CampusWoods PASS,
FraternityBoundary PASS and WoodsScene/Undergrowth PASS. It retains 81 supported
ground/hut samples, all three hollow bottoms and a clear hut entry capsule.
These are static/swept scene assertions, **not** a manual playtest or completed
parking/full-trail walking verification for the final pass.

The map's SHA256 after pass one matches the protected starting value:
`5309c8f80470373721148eabdc62bc6d70ed40fe12ad789587881a2ca7335e5c`.

### Preliminary pass-one frame times — not final acceptance

| Native view | Stage0 stable avg ms | Pass1 polished avg ms |
| --- | ---: | ---: |
| Edge | 22.93 | 23.82 |
| 20m in | 19.58 | 20.33 |
| Deep | 20.82 | 20.43 |
| Ravine | 19.73 | 17.40 |
| Silhouette | 17.27 | 18.54 |
| House back | 29.21 | 21.17 |

The stage 0 house-back sample contains a 248.80 ms maximum frame, so do not interpret
its lower pass-one average as an established optimization. Final repeat captures
and the approximately +3 ms budget comparison remain pending.

## Pass 2 — fog, darkness and silhouettes

### Ground mist and deeper darkness

`HouseWoodsAtmosphere.h/.cpp` retunes the **12 existing** owner-tagged
`WoodlandMist` volumes and adds three transient, noncolliding local fog pools at
the existing hollows. It does not replace the dusk sky, sun or global fog.
The broad volumes use the lowest ground sampled at their centre and eight
neighbouring points, 900 cm apart, then sit 42 cm above that ground reference.
This keeps a nearby bank from lifting a whole volume into the canopy.

The added pools sit 38 cm above each hollow's unchanged bottom:

| Hollow centre | Pool centre Z | Radius |
| --- | --- | --- |
| (−4200,8000) | −457.64 cm | 607 cm |
| (2700,10400) | −499.69 cm | 800 cm |
| (−3300,16000) | −464.15 cm | 718 cm |

Current low-pool tuning in `Configure` is listed here for reproducibility. Its
final ravine capture is recorded in `evidence/pass2_low_pools`.

| Control | Broad mist | Hollow pools |
| --- | --- | --- |
| Radial extinction | .040 → .085 with depth | .30 |
| Height extinction | .20 → .34 with depth | .80 → 1.10 with depth |
| Height falloff | 2500 | 1200 |
| Height offset | 0 | .08 |
| Start distance | 1000 cm | 200 cm |
| Albedo | (.38,.46,.52) | (.38,.46,.52) |
| Emissive scattering | (.018,.025,.036) | (.32,.40,.50) |

Both use phase G .10. Pool radius is `Hollow.Radius * 1.38`. Fog emissive is a
volume-scattering term, not a point/spot light or luminous ground sheet.
`AcquireNearFog` leases a tagged temporary 200 cm override of
`r.LocalFogVolume.GlobalStartDistance`; the last `ReleaseNearFog` removes that
override at EndPlay. The completed six-view run records restoration to 1000 cm.
`-NoWoodsFog` disables the local fog changes and near-distance lease.

`HouseDuskAtmosphere.cpp` retains its existing colour grading and base exposure
(`−4.5 + DuskEV`, with default `DuskEV=.35`). The woods smoothly lose at most
.24 EV and gain .045 vignette intensity beyond the base .55. `DepthAt` ramps
from y=4800 to 11800 and tapers laterally between |x|=7400 and 8500; the film
response interpolates at speed 2. House/party exposure and the existing sky/sun
are unchanged by this depth response. No new lights were added.

### Deadwood silhouettes

`HouseWoodsSilhouettes.h/.cpp` adds original procedural crooked snags, a visibly
supported leaning dead tree and a hollow, shoulder-shaped broken stump without
human features. Verified placements from `pass2_final/runtime.log` are:

| Feature | Ground position (cm) |
| --- | --- |
| Snag 0 | (−4130,10730,−141.69) |
| Snag 1 | (450,10200,−356.78) |
| Snag 2 | (−5550,15150,−360.84) |
| Snag 3 | (3813.11,17250.57,−354.86) |
| Leaning trunk foot | (−3169.81,11864.51,−356.69) |
| Its new standing support | (−2799.81,12464.51,−360.14) |
| Hollow stump | (−2450,12170,−368.56) |

Placement checks preserve the main trail by 450 cm plus each prop's radius,
the hut clearing, hollow bottoms, existing trunks and review ground probes.
Only small boxes around the lowest trunks collide; high branches do not.
All batches disable shadows, distance-field and indirect-light updates.
Cull ends are 19500 cm for snags, 12500 cm for the leaning pair and 6800 cm for
the stump. Geometry is original; existing trees are not moved.

### Pass-two evidence

`evidence/pass2_final` completed all six dusk views at 1280×720, exited with
code 0 and did not time out. Its `Saved/HouseExteriorViews/` contains all six
PNGs plus `frametimes.csv`: average frame times, in view order, are
23.42, 21.52, 21.10, 18.09, 19.29 and 22.30 ms. These are stage evidence,
**not** the final paired performance acceptance.

That run reports CampusWoods, FraternityBoundary and WoodsScene/Undergrowth
PASS, 81 supported ground samples, a clear hut-entry capsule and **53 exterior
point/spot lights**. The original-tree fingerprint remains `0f3b2bb7`.
These are static scene checks, not a replacement for the final moving route
reviews. The last low-pool fog adjustment has a separate ravine-only capture
in `evidence/pass2_low_pools`. That run exited 0, did not time out, and its four
static review reports passed. Its final revised-camera PNG was reviewed and
shown before pass 3 began. The ravine average was 10.48 ms (maximum 19.05 ms);
this is not comparable with the older ravine camera's timings. Final paired
baseline/final runs remain required, with the same revised camera in both.

## Pass 3 — human traces, motion and sound (pending)

Pending: document the implemented original, unbranded human traces and exact
locations; any shared wind/leaf-motion extension; sound implementation or clearly
named empty hooks; and screenshots from the completed stage. No monster, real
brand, UT mark or fraternity crest is intended for this pass.

## Pass 4 — final performance and reviews (pending)

Pending final results, including:

- Six stage0/final dusk captures with `HOUSE_EXTERIOR_FPS` numbers and comparison.
- `CampusWoodsReview`, `WoodsSceneReview`, `WoodsUndergrowthReview`,
  `FraternityBoundaryReview` and actual `ParkingLotWalkReview` results.
- Full woods walking route as an additional regression check.
- Original-tree snapshot comparison, current map hash and final exterior light count.
- Build/warning outcome and per-file change summary after all stages are integrated.

The user identified two pre-existing `EXTERIOR_WALK_REVIEW` failures. They must be
reported distinctly from any new regression; their known status is not a blanket
waiver for changes to other routes.

## Artwork provenance

Every **new** mesh, leaf/grass mask, ground texture and normal map in this pass is
original code-generated geometry or pixels. Existing saved Unreal material
masters are reused as shaders; new textures are transient runtime objects.

No CC0 download, externally sourced model, new AI-generated bitmap, real brand,
UT logo or crest has been added. Therefore there are no new third-party/CC0
source URLs to list. The project already contains earlier assets; this pass does
not replace or relicense those unrelated pre-existing assets. No Unity project
or Unity-version asset is used.
