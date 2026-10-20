# MASTER PROMPT: Calder in the mountains (serious elevation, an Appalachian landscape, wildlife). Homecoming '86

Attached: two in-game screenshots taken on foot at dusk. They are the "before" references, and both face +X (east).
(1) About (-12000,-6450), between Old Main and Harlow: a red-brick dorm on the left, Fraternity Park Dr with its lamp
posts and frat houses on the right, all on a dead-flat lawn. (2) About (-4000,-17800), at the stadium's south-west corner:
a flat lawn, a flat lot with a circular island, the brick drum on the right, and a far town under low ridges on the left.
Everything near the player is flat. Verify both positions in the PLAN.

## Context
Unreal project "Senior Sendoff": UE 5.8 on this Mac (Metal), map Chapter01_House, at ~/Documents/Unreal Projects/SeniorSendoff.
Never touch the old Unity copy at ~/Senior Sendoff Real.

**The setting is fixed:**
- the invented **Calder State University** (the **Fighting Geese**, purple and gold), in the invented small Appalachian
  mountain college town of **Calder**;
- **Saturday October 18 1986, at dusk, just after homecoming**;
- the sun held 4.5° below the horizon (`HouseDuskAtmosphere.cpp:128,158`), so the afterglow lies toward yaw ≈205°
  (west-south-west);
- a clear, still evening at 55 °F, with peak colour on the mid slopes. Tune anything temperature-dependent to 55 °F.

**What's already there.** Everything outside the house is built at runtime in code:
- the street, frat row, west field and woods (`HouseExterior*`, `HouseWoods*`, `HouseWestField*`);
- the campus (`HouseCampusLayout`, `HouseCampusInteriors*`, `HouseCampusPolish*`);
- the town and ridges (`HouseCampusPolishTown.cpp`);
- homecoming (`HouseCampusFall*`) and the clock tower (`HouseClockTower*`).

**What I want.** I asked for Calder to feel like a small mountain college town. It doesn't, because the ground is flat.
In and around Calder (the campus, the town edge, the woods and the fields) I want:
- serious elevation: tens of metres, not a few;
- a real Appalachian landscape that holds up at 1 m;
- really good wildlife.

**Read first:**
- `SourceArt/CampusMaster/` (README, STATUS, FILES, and PLAN §7 and :79-99);
- `SourceArt/CampusMaster/briefs/` (the line numbers there are stale);
- `HouseExterior.cpp`: `Grade()` :111-122, `Terrain()` :1260-1300, `Boundary()` :1418-1440, the tree record :1700 and
  the instancing :1836-1950;
- `HouseWoods*`, `HouseCampusLayout.cpp`, `HouseRoamingPig.cpp`, `HousePigSmoke.cpp`, `HouseLeafWind.cpp`,
  `HouseCampusPolishSound.cpp`.

**Facts this job rests on:**
- **No shared ground.** `Grade()` returns -60 for every Y ≤ 0, so the whole campus and the town valley are one plane.
  Behind the house it is -362. The campus side holds about 470 flat-ground literals (-60, -57, -72, -55, -56, -58, `GF`,
  `Ground`, `RoadTop`, `WalkTop`, `ApronTop`), about 250 of them in world space.
- **The tree crc includes Z.** `HouseWoodsPass.cpp:28-42` hashes the full `FTransform::ToString()` of `TreeA`, `TreeB`
  and `ForestDistant`. So changing `Grade()`, `HouseWoods::Height` or `HouseWestField::RaisedHeight` under any tree
  breaks `crc=0f3b2bb7`. `treesim.py` does not model Z.
- **Fall respawn.** `FallLimit` = the first PlayerStart's Z − 1200 (`StoryCampaign.cpp:1763`), so about -1100..-1200.
  Any walkable ground below that respawns the player.
- **Movement.** The player keeps the engine defaults: step 45, walkable 44.8°.
  - Jump about 138 cm (JumpZVelocity 520); sprint 750 (LeftShift).
  - Crawl capsule 36 cm tall.
- **What doesn't exist:**
  - no navmesh, `AIModule` or `NavigationSystem`;
  - no animal meshes, skeletons or animations;
  - only one Niagara asset;
  - no Landscape, Foliage, PCG or Water module;
  - no `USoundConcurrency`. The Mac mixes 32 channels.
- **Rendering on this Mac:**
  - Runtime meshes can't be Nanite and get no distance fields, so Lumen can't see them.
  - In 5.8 the HISM proxy is dynamic (rebuilt every frame), while ISM is cached.
  - `ALandscape` can only be authored in the editor, so it is out.

**Gotchas from the last job:**
- **Render thread.** The frame is render-thread bound. Draw runtime `UStaticMesh` (`bFastBuild`) and keep procedural
  meshes hidden, for collision only.
- **Timing.** Use `-MetalOffscreenOnly`: windowed Metal locks to 8.33 ms.
- **iCloud.** ~/Documents syncs to iCloud.
  - Delete a generated file before rewriting it, or a "name 2" copy appears.
  - UBT compiles stray "name 2" sources too, so `find Source -name '* 2.*'` must be empty before every build.
- **Stuck runs.** A run stuck at MetalRHI needs `xcrun --kill-cache`.
- **Editor-only code.** Runs are `UnrealEditor -game`, so editor-only APIs compile and work here, then break a packaged
  build. Never use them.

## THE WOODS AND THE PIG ARE THE MODEL
- **The woods (`HouseWoods*`) are the landscape standard.**
  - One analytic height that every placer and review samples.
  - Five Appalachian species with three runtime LODs (`HouseWoodsStructure::BuildMesh` :513-558).
  - Layered floor batches and hollow mist.
  - Reviews that trace the ground and walk the trail.
- **The pig (`HouseRoamingPig`) is the animal standard.**
  - Rigid parts posed procedurally each tick, with legs planted on the ground.
  - A flee from players.
  - Replication only where it matters.
  - `-HousePigAnimalSmoke`: real input, screenshots and hard metrics (`hoofFloorGapP95`, `plantedHoofDriftMax`).

Study both before designing anything. A hillside, a stone wall or a deer must hold up next to the best `detail_woods_*`
views and pass metrics as strict as the pig's. Don't edit the woods' own terrain or batches.

## Phase 1: serious elevation
**Known problems (verify each, then fix):**
- **Two flat planes.** The only relief is the woods' hills (5.2 m at most), the west-field mound and five 70-95 cm
  campus berms (`HouseCampusLayout.cpp:595-640`). The PLAN's stadium collar, library terrace, sunken quad panel and
  retaining walls were never built.
- **Flat primitives.** Every layout primitive is a flat box (`Slab` :358 … `LawnFill` :559). `PaveZ` (:1065) and
  `WalkSamples` (:1160) return constants. `Grade` is copied, each copy different, in `HouseExteriorNature.cpp:41`,
  `HouseExteriorWalks.cpp:15`, `HouseSurroundingsPolish.cpp:35` and `HouseExteriorWalkReview.cpp:103`.
- **Buildings have no Z.** `FBlock` has no Z field, and the building frame is written with Z=0 in five places:
  - `HouseExteriorCampus.cpp:286`;
  - `SideEntrance` :206;
  - `HouseCampusInteriors::Frame` :893;
  - `HouseCampusPolishBuildings.cpp:67`;
  - `HouseCampusPolishLightPools.cpp:31`.
- **The ground mesh.** `ExteriorGrounds` is one procedural mesh (`grounds=69240`), flat-shaded, with 16 m cells on
  campus, drawn on the dynamic path.
- **The mountains.**
  - The near ridge crest is only 41-139 m high and 460-950 m away. It is smooth, one colour, and bare on top.
  - The town valley stays flat out to ±150000.
  - The north ridges rise straight off -362.
  - Nothing exists at |X| > 32000 for Y 0..32000.
- **Water.** The woods' "creek" is a dry soil ribbon, and there is no water anywhere.

**Target: a small mountain college town in a cove.**
- The house stays in the valley bottom.
- Across the street the campus climbs College Hill in benches and terraces.
- The town lies in its river valley, 45-55 m below the south bluff, and is revealed from the overlook.
- Forested ridges close the cove on every side.

**Spot heights** (above today's -60). The plan may raise any of them by up to 40%. Lowering one by more than 10% needs my
OK; log every shrink in STATUS under "Shrunk:" with the reason.
- **Front row.** Where no drive meets the street (Old Main and Harlow, X -15200..-6800; The Commons and Ashby, X
  11000..18900):
  - the lawn stands at +1.5..+3.5 m behind a mortared sandstone **street wall** 0.9-2.4 m high along the far sidewalk;
  - the wall has a verge of at least 45 cm, coping stepped in level runs, 1930s date stones, and stairs up from the
    sidewalk;
  - drives meet the street flush and climb at no more than 10%.
- **Lower campus:** +3..+5 m by Y -10000.
- **Tower Knoll:** +6..+8 m.
- **The quad:** terraces climbing west from +6 to +9 m around a sunken panel.
- **Mercer Library:** on a terrace at about +11 m, up the **Senior Steps** (an invented CLASS OF 1936 plaque) and a
  railed ramp.
- **College Hill:** Hollis, Calder Hall, Whitcomb, Band and Music, and Pratt and Alumni, cresting at +24..+30 m around
  Y -21000..-25000.
- **The Physical Plant:** a cut bench at about +12 m, up a 16-18% service lane.
- **Founders' Overlook:** +28..+34 m on the bluff edge, west of the gravel overflow lot, with Main St 47-53 m below.

**Frozen zones.** Ground, geometry, collision and look are held at today's state to the millimetre:
- the old property rectangle X -8500..8800, Y -5900..19800: the house, its lot, the driveway, the parking lot, the
  backyard, the court and the woods;
- Fraternity Park Dr (X -32000..8700) with both sidewalks;
- Calder Dr (X 8700..10300), from the street to Y 19800;
- the frat row lots, the Fall Jam field and the west field;
- a 12 m disc round every map-placed actor outside these zones (PlayerStarts, `AStoryCheckpoint`s, pigs, triggers and
  volumes). List them in (1a).

Non-colliding wildlife and its sound may use frozen zones (the deer track, the west-field fence line, the frat yards).
New landscape dressing may not.

**Blend bands.** Each band lies wholly outside its zone. It starts at zero relief and zero slope, and its steepest slope
is 1:12, so it is at least 18 times as wide as the height it gains, and at least 12 m wide. Beyond the band the normal
grade rules apply.

The one exception: a declared retaining wall may stand on a frozen edge in place of the band. Its foot stays at today's
grade, and nothing inside the zone moves.

**Buildings on slopes.** Each building sits on a level pad. Its treatment depends on the fall across its footprint:
- up to 0.6 m: a stone plinth;
- 0.6-4 m: a raised or walk-out basement storey on the downhill side, with a rusticated base and areaway windows, and the
  shell carried down to 30 cm below the lowest ground;
- over 4 m: cut into the hill, with a retaining wall and a service court.

Entrances sit 3-6 risers above their walk, or level with it by a ramp.

The basement storey is exterior shell only:
- basement windows are dark, blind glass;
- service doors are fixed, non-interactive geometry, never `ACampusDoor`;
- no new rooms, interior lights or floor-count changes;
- footprints don't change, and service courts and areaways are never NoGo zones or footprints.

**Clock tower.** It keeps its XY and rides up onto Tower Knoll.
- A stair and a ramped walk lead up from the quad, on the Y -10800 axis.
- The sight line from the frat lawn (2400,-3750,140) to the raised dial centre stays clear of buildings and of every tree
  canopy's bounds.

**Stadium.** It keeps its XY and its Z: turf -56, its four Z constants and the masts are untouched.
- The apron ring stays at today's grade and carries Stadium Dr, the bus bay and the loop.
- The bowl reads as dug in because the land to the south climbs away from it:
  - the South Lot (X -1400..8300, Y -25050..-20550) in three tiers at about +1.5, +4 and +7 m, with stone walls and
    stairs;
  - then **the Hill**: grass terraces at +10..+16 m below Pratt and Alumni, looking down on the rim and the glow of the
    lit field.
- No earth collar wherever a building stands within 25 m of the bowl. Goose Union is 11 m away, and the bus bay and
  Calder Dr are 7 m away.

**Grades: 1986, not the ADA (which dates from 1990).**
- **Walks:** up to 12%. Pitches up to 15% are allowed if shorter than 30 m and fitted with a pipe handrail. Anything
  steeper is stairs.
- **Accessible route:** one route at 8.3% or less (railed 1:12 ramps) to every Tier A entrance.
- **Roads:** up to 12% with vertical curves and a 2% crown; the Physical Plant lane only, 16-18%.
- **Lots:** up to 5%, terraced with walls and stairs.
- **Cut and fill:**
  - uphill: a grass bank (1:2), a rock cut, or a mortared stone wall;
  - downhill: fill at 1:2;
  - always a level verge of at least 45 cm before a bank.
- **Guardrail:** 1986 W-beam (timber posts, blockouts, turned-down ends) wherever a drop of more than 1.5 m lies within
  3 m of a road.
- **Drainage:** ditches with rip-rap, catch basins and culvert headwalls.

**Calder Branch, Gander Pond and Calder Falls.**
- **Course.** A creek comes off the north-east foothills and runs down the east side (X about 21000..30000), falling all
  the way to the south edge.
- **Gander Pond.** It is dammed into a pond (about X 23000..27000, Y -11000..-16000), with a stone edge, a dock and a
  walking loop.
- **The glen below the dam.** Its depth comes from **raising the rims** (the east campus and the north-east hills,
  +10..+18 m), never from digging. The glen floor stays at or above FallLimit + 400 (expect about -700). The PLAN sets the
  pond surface and the depths and logs them.
- **Calder Falls.**
  - A single 6-8 m plunge over the sandstone caprock.
  - Behind it, a walk-behind **rock house** (an undercut in the shale) with a railed path and Unwalkable rock.
  - A plunge pool, then a 2 m cascade.
- **Crossings.** A stone-arch footbridge on the dam, a timber footbridge at the falls, stepping stones, a road bridge with
  stone parapets, and a pipe culvert in a stone headwall.
- **No swimming.**
  - Water surfaces have no collision; the beds are walkable terrain.
  - Depth is at most 60 cm within 3 m of any reachable bank, and at most 120 cm anywhere.
  - Every point in the water connects back to a bank at 1:3 or flatter.
  - Where the creek leaves the walkable area it passes through a stone headwall with a grate.

**Overlooks.**
- **Founders' Overlook:** a 1.07 m stone parapet, a bronze plaque naming invented peaks (GOSLING KNOB), and a coin viewer
  with no brand.
- Tower Knoll and the pond.
- **Crow Ledge:** a railed sandstone ledge in the north foothills, looking back over the house roofs.

**The house's side outside the frozen zones** (see "I allow"):
- north: foothills climbing to +40..+55 m by Y 32000;
- east: rolling hills;
- west: a farm edge rising to a wooded spur;
- Calder Dr north of Y 19800: it climbs with a rock cut and guardrail and curves out of sight round a spur.

**Edges and containment.** With the property boundary off, the 190 cm hedges are the only edge collision. The world edge
at ±32000 is open, and the fall respawn catches anyone who walks off.
- Keep that wherever the edge ground stays within 3 m of today's grade.
- Where relief raises walkable ground at the edge, end it with visible scenery the player can't climb: an Unwalkable rock
  band, or a hedge or fence of 2 m or more. Never an invisible wall.
- Split every hedge run into segments of 2 m or less. Each segment's bottom sits 20 cm below the lowest ground under it,
  and its top stands at least 190 cm above the highest.
- The flood fill proves nothing beyond ±31800 is reachable, except where it is reachable today.

**Beyond the edge** (scenery, no collision):
- **The town** sinks into its valley:
  - the river at about Z -2400, Main St at about -1900;
  - every building on a pad with a foundation (`G` in `HouseCampusPolishTown.cpp:22` becomes a valley function);
  - from campus, only the water tower (raised onto a knob) and the steeples show above the bluff; the whole town is the
    reveal at Founders' Overlook.
- **The near ridge** climbs from the river (Y -46000) to a crest 240-320 m higher at Y -85000..-100000: an 18-22° wall
  seen from campus, with knobs, saddles, coves and a water gap.
- **Behind it:** a mid ridge of 400-500 m and a far ridge of 600-750 m.
- **Cove walls**, east and west, along the whole length from Y +32000 to -60000:
  - forested spurs whose crests stand 150-280 m above campus, 400-900 m out;
  - notches only where water leaves (the Calder Branch valley and the river's water gap);
  - the north ridges re-based onto the foothills.
- **Gosling Knob:** a 350-450 m knob with a notched shoulder, on the west-south-west wall at the afterglow (yaw ≈205°).
  From the frat lawn, the overlook and the west road it stands black against the glow.
- **Layers.** Five ridge layers, each 15-25% paler and bluer than the one in front, the last barely darker than the sky.
  Crests within 1.5 km are serrated by individual tree crowns.
- **Haze.** Valley haze is `LocalFogVolume`s beyond the edge only. The global height fog, the sky and the dusk sun are
  untouched.

**How to build it:**
1. **One ground.**
   - `HouseCalderTerrain::Relief(X,Y)` is analytic, built from named landforms with its own seeds. It is then constrained
     in priority order, each step C1-blended: frozen zones; pads, the stadium apron and the tower plaza; road, lot and
     walk corridors (profile, cross-section, cut and fill); water levels.
   - Retaining walls are declared polylines, the only places the height may step.
   - At load, `Relief` is sampled once onto the tile lattice (`ParallelFor`, deterministic). From then on the lattice is
     the authority.
   - `HouseExteriorSite::GroundHeight(X,Y)` returns exactly the triangle interpolation (same diagonal rule) that LOD0 and
     its collision use. Add `GroundNormal` beside it. It is the grade datum: today's -60/-362 surface plus relief.
   - `HouseCalderTerrain::SurfaceZ(X,Y)` = `GroundHeight` + today's paving offset there (road -12, walk +3, apron +5).
     It is what feet, props and animals stand on.
   - No height feature shorter than 16 m in the walkable area; finer detail goes in the material and the normals.
   - Log a hash of the lattice (`CALDER_TERRAIN hash=`).
2. **Route everything through it.**
   - **Never substitute a copy of `Grade`.** Add `Relief` to each copy's own result. Review expectations inside the frozen
     zones (`HouseExteriorWalkReview.cpp:80-104`, `HouseCampusReview.cpp:72`) stay literal.
   - **Callers:** `ExPatch`, `Pole()` (the 53 lights move in Z only), `Car()` (with pitch and roll from `GroundNormal`;
     cars are not in the crc), the shrubs, `HouseNature::Surface`, `HouseLeafWind::GroundHeight`, every layout primitive
     (draped, split only where relief isn't 0), `PaveZ`, `WalkSamples`, the stalls, benches, gateposts, lamps
     (`CampusLamp`, `LampPoints`, `LotPoles`) and the polish.
   - **Placement.** Props stand on their lowest foot, with a footing. Fence posts are plumb, and copings step in level
     runs.
   - **Inside `BuildAll()`** (before :1700), relief may change Z values only. Never a branch, a count, an RNG draw, a
     NoGo box, `Campus.BuildingFootprints` or the frat-row footprints. Never skip a tree or car for slope there: anything
     that must go is hidden after :1700 and counted.
3. **Trees.**
   - Between `RecordOriginalTrees` (:1700) and the copies at :1701, add `Relief` (its minimum over the trunk footprint) to
     the Z of each entry in `Site.TreeA`, `TreeB` and `ForestDistant`.
   - Relief is exactly 0.0f in the frozen zones, so those trees stay bit-identical.
   - Check that every downstream copy sees the new Z: `OriginalWoodlandTrees`, the per-batch copies at :1880-1903,
     ForestDistant near :1946, `BigWithOaks` and `SmallWithBirches`.
   - `ExteriorForestDistant` stays above 600.
   - Along walkable house-side routes (Crow Ledge), hide `ForestDistant` instances within 40 m of the path (they have no
     collision and low detail; count them), and plant new woods-species trees with trunk collision.
4. **Tiles.** 128 m tiles with LODs at 2/4/8/16 m, built by a multi-LOD `bFastBuild` builder like
   `HouseWoodsStructure::BuildMesh`, with explicit screen sizes.
   - **Grid.** Each tile is a tensor grid: the 2 m lattice plus every `TerrainCuts` edge, frozen-rectangle edge and
     building-footprint edge inside it.
   - **Holes.** Cells inside cuts or footprints are dropped, so footprints are holes, as the house's is today.
   - **Walls.** Wall polylines split the cells they cross (convex clip, duplicated vertices, each side sampled on its own
     side), so steps are exact. Every LOD keeps the same cut lines.
   - **Skirts.** 40 cm skirts on every tile edge, and on every edge that meets the frozen `ExPatch` or the woods grid.
   - **The frozen ground.** Rebuild `ExPatch` over the frozen rectangles with today's split lines and cell sizes, and
     prove it with the frozen-zone trace grid.
   - **Slope limit.** `GroundHeight` never exceeds 1:1.5 except at declared walls. Anything steeper is a separate rock or
     wall mesh.
   - **Normals and colour.** Normals come from the gradient. Extend `HouseCampusStaticMesh::Describe` with vertex colour
     for zone masks and baked cavity occlusion: Lumen can't see the hills, so valleys must be darkened in the mesh.
   - **Draw settings.** No shadows, no distance-field lighting.
   - **Outer land.** A 16-32 m grid with no collision, matching `GroundHeight` exactly along the ±32000 seam. It replaces
     the far-lawn box (`HouseExteriorCampus.cpp:382`).
5. **Collision.**
   - A hidden procedural mesh per walkable tile: complex-as-simple, cooked async (`HouseCampusPolish.cpp:158-172`).
   - Log `CALDER_TERRAIN_COLLISION ready=N/N ms=`. Reviews and the multiplayer client wait for it before moving a pawn.
   - Walls, cuts and rock faces get `SetWalkableSlopeOverride(WalkableSlope_Unwalkable)`, and so does anything over 40°.
   - On `NM_DedicatedServer`, build only collision tiles, walls, stairs, trunks and the wildlife director.
6. **Buildings.**
   - Add `DZ` (pad Z − (−60)) to `FBlock`, and use one `HouseCampusLayout::Frame(Seed)` in all five places. Rooms, doors,
     props and light anchors keep their frame-relative positions.
   - Split `OpenTop` into the canonical `OpenTop` and `OpenTopWorld`, and classify every caller (7 `OpenTop`, 8
     `InteriorBox`, 3 `InteriorFloorZ`, 25 `FloorTop`).
   - `InteriorBox`, `InteriorFloorZ`, `FloorOf`, the `FOpening` Z, the aprons and the light pools add `DZ`, and so do
     `-CampusInteriorsReview`'s absolute -400 and -80..-40 windows. Fill `DistanceOutside`'s static cache afterwards.
7. **Clock tower.**
   - One runtime `Lift` translates the actor and its components, the plaza and the seat wall
     (`HouseCampusLayout.cpp:643-656`), `TwrG`, the review cameras and the `detail_clocktower_*` targets.
     `HouseClockTowerSite::GroundZ` is `constexpr`, so add the lift beside it and leave the constant alone.
   - The crows follow `Center()`.
   - A blocking tree is hidden; a blocking building is stop-and-ask.
8. **Views.** At capture time only, for every perspective view whose `Pos.Z − Grade < 600`, add `Relief(Pos.XY)` to Pos
   and `Relief(Target.XY)` to Target.
   - Never edit `ExViews[]`: the XY of views not named `detail_`/`nature_` feeds the tree NoGo.
   - Never move orthographic plans.
   - Log `inside=` for every view.
   - New views are `detail_*`, appended at the end, because screenshots are named by index.
9. **Ground sheets.**
   - Every sheet (`LawnFill`, the lawn tone and stripe overlays, desire paths, mulch, leaf drifts, paint) is re-meshed on
     the tile lattice with the same vertices and diagonals, 1.5 cm up along the normal. Or drop it where `M_CalderGround`
     replaces it.
   - A light-pool decal's half-depth is 60 + its radius × the steepest slope under it.
10. **Far scenery names.**
    - Re-basing the north ridges changes only the Z of the existing `EX_Ridge*` vertices, which stay in `ExteriorFar`.
      Never convert `ExteriorFar` to a static mesh.
    - Foothills, spurs, farm stone and barns use new material names, and none of those appears on the house side as
      brick, limestone or warm brick.

## Phase 2: an Appalachian landscape
**What's missing:**
- **Grass.** The campus has no 3D grass. Its flat 3 m lawn quads read as a checkerboard in both shots.
- **Materials.** There is no rock, cut, clay or wall material, and no slope-blended ground.
- **Hedges** are boxes. There's no water and no mist outside the woods.
- **Trees.** Shot 2's trees read as bare twigs at the peak of the colour. Check their LODs and leaf cards.

**Target** (period-correct, invented names only):
- **Ground.**
  - One terrain material blends lawn, rough grass, broomsedge, leaf litter, orange-brown clay in fresh cuts, gravel and
    rock, by slope, height and zone masks.
  - Macro variation everywhere, baked cavity darkening, and triplanar rock only on slopes over 35°.
- **Campus grass.**
  - Green cool-season lawns, mow stripes following the contours, trimmed edges.
  - Rutted desire paths on the slopes, raked leaf piles at the curbs.
  - Banks steeper than 1:3 left long and tawny.
  - 3D tufts at bases, curbs and fences.
- **Fields.**
  - Orange-tan broomsedge, goldenrod gone to fluff, asters in bloom and rust ironweed.
  - Blackberry canes, scarlet sumac and split milkweed pods.
  - Red cedar and multiflora rose with red hips along the fence rows.
- **Trees in October.** Reuse the woods' species and LODs. Colour follows altitude, and the colour line must read from P2,
  P3 and P5:
  - **crests 250 m or more above the town:** past peak, russet and brown and thinning, with grey bare patches;
  - **mid slopes:** at peak, with sugar maple orange, red maple scarlet, poplar yellow, hickory gold, white oak russet,
    sourwood and black gum crimson, and white ash purple-bronze;
  - **the valley floor and the town's street trees:** 10-25% still green.

  Also add:
  - dogwood with red berries;
  - bare yellow buckeye with buckeyes on the walks, and bare black walnut with its nuts down;
  - sycamores along the creek;
  - healthy hemlock and white pine;
  - witch hazel in bloom;
  - a persimmon at the farm, hung with orange fruit.
- **Hillsides and the glen.**
  - Rhododendron "laurel hells" on the north slopes, and mountain laurel on the spurs.
  - Exposed roots, mossy boulders, bronze galax, and straw-gold hay-scented fern.
  - Rock polypody and moss on wall tops and boulders.
  - A fallen grey American chestnut log with yellow root sprouts beside it.
- **Rock.** Grey-brown sandstone over shale in tilted beds: outcrops, ledges and scree, and road cuts with drill marks and
  iron-stained seeps.
- **Walls and fences.**
  - Mortared stone on campus, with stained weep holes; dry-laid fieldstone on the farm.
  - 1930s parapets with date stones.
  - A chestnut split-rail worm fence, woven wire, and real hedgerows instead of boxes (keeping the hedge collision).
- **The farm edge.**
  - A hay meadow with weathered, twine-tied round bales (no net wrap or plastic).
  - A steep pasture with terracettes (the stepped cow paths across a hillside).
  - A slatted tobacco barn with burley curing, with no barn ads.
- **Water.**
  - The creek: clear and tea-tinted, with white riffles, dark pools and spray at the falls. Every rock carries a wet band
    5-15 cm high, leaf packs sit on the upstream side of stones, and leaves drift at the current's speed.
  - The pond: dark and still, reflecting the dusk and the lamps, with floating leaves, cattails and a PLEASE DON'T FEED
    THE GEESE sign.
- **Mist.** Thin mist in the glen and over the pond (`LocalFogVolume`s named `CalderHollowMist*`).
- **Ridges.** Forested to the crest in the altitude patchwork, with dark hemlock streaks in the coves and fuzzy crest
  lines.
- **Signs of life.** Buck rubs and scrapes, tracks in the creek mud, gnawed stumps with fresh chips, a bald-faced hornet
  nest, spotted orb-weaver webs catching the lamp light, woolly bear caterpillars on the walks, and squirrel leaf nests in
  the oaks.

**How to build it:**
- **Materials.**
  - `M_CalderGround`, `M_CalderRock` and `M_CalderWater` come only through the editor-Python pattern
    (`Tools/claude/build_campus_materials.py`, `Tools/claude/ue_py.sh`).
  - Prepare them as ONE batch by the end of (1b), so I close the editor once. Until then, use MIDs of existing masters
    (`EX_Grass` and friends).
  - Requirements: Substrate-safe, ISM-enabled, their folder in `DirectoriesToAlwaysCook`, and at most 12 texture samples
    per pixel.
  - Water is opaque (roughness 0.02-0.08), so Lumen reflections work, with dithered edges.
- **Textures.** Painted in code (`HallFbm`, `HallHeightToNormal`). If a CC0 texture would be clearly better, list it in
  STATUS for me to download; never wait for it.
- **Instancing.**
  - ISM, not HISM, for everything new.
  - No shadows, distance fields or indirect lighting.
  - Cull distances: 25-45 m for tufts, 60-90 m for shrubs, about 225 m for trees.
  - Trunk-only collision.
  - Leave the woods' HISMs alone, since reviews iterate that type.
- **Ground-cover budget.**
  - At most 120,000 instances in total.
  - At most 60 triangles per tuft at LOD0.
  - Placement rejection on a 4 m bucket grid.
  - Your own seeded streams.
  - Nothing on walks, roads, lots, stairs or water, or inside buildings.
- **Names.**
  - New ISMs never start with a reserved prefix (see Naming).
  - They never go through the exterior's `Instance()` lambda, which gives anything named `Tree*` hit handlers.
- **Wind.** Add a public `HouseLeafWind::RegisterWindMaterial`, so new foliage that reuses `M_NatureFoliage` sways with the
  rest.
- **Ridges.** The ridge forest is merged canopy-clump meshes per tile.

## Phase 3: wildlife
**What exists:**
- The tower's 12 crows (one procedural mesh rebuilt only in flight; `CLOCK_TOWER_CROWS burst=12`) and the pig.
- An owl call in the woods (`HouseLeafWind.cpp:84`), and campus crickets and wind (`HouseCampusPolishSound.cpp`), as
  sound only.
- A deer track (`HouseWoodsFloor.cpp:520-526`) with no deer on it.

**The clock.** The scene is a frozen dusk, so wildlife runs on a 30-minute cycle of server time. "Server" and "local" are
explained under Multiplayer.

**Core (build these first, end to end):**
- **Canada geese, the mascot.**
  - Overhead (local): a V of 14-40 birds every 10-20 min, 60-150 m up, heading SSW down the valley and honking. Every
    other V sets its wings and splashes down on Gander Pond.
  - On the pond (server): 6-12 birds. They hiss at 6 m and waddle into the water.
- **White-tailed deer (server).**
  - Grey winter coats: does with spotless fawns, and one buck with hard antlers.
  - Three groups of 2-5: on the deer track, in the farm field, and on the lawn under the bluff.
  - At 60 m they stare; at 45 m they stamp and blow; at 35 m they bound off with their tails flagged.
  - They come back after 2-4 min.
- **Crows (local).**
  - Keep the tower's 12 exactly as they are.
  - Add a roost stream: groups of 3-15 crossing the valley every 1-3 min, cawing, into the north woods' pines.
  - Ravens croak over the ridge.
- **Gray squirrels (local).** 2-4 latecomers caching acorns under the oaks. At 8 m they spiral up a trunk, chattering,
  and then go into their leaf nests.
- **Wild turkeys (server).**
  - A flock of 8-14 on the farm edge.
  - One fly-up per cycle: one by one, into a big oak to roost. The flock re-forms out of sight behind the barn, and they
    never fly down again during the dusk.
  - They run at 25 m and flush at 12 m.
- **The night bed (local).**
  - Snowy tree crickets at 60 chirps/min (Dolbear's law at 55 °F), slow field crickets and a few cold katydids.
  - 1-3 lone spring peeper calls per 10 min at the woods edge, never a chorus.
  - Nothing indoors.

**Full roster:**
- **Bats (local):** 4-10 big brown and red bats, erratic and silent, over the pond, the glen and the mercury lamps.
- **Moths (local):** 0-3 per mercury lamp, flying slowly, only on lamps within 60 m of a viewer.
- **Red fox (server):** it trots the fence line and pounces on mice. It looks back at 40 m and trots off at 30 m.
- **Raccoons (server):** 1-2 at The Commons' dumpsters (add the dumpsters; none exist). They freeze at 15 m and climb a
  tree at 10 m.
- **Striped skunk (server):** it digs cone-shaped holes in the lawns. At 5 m it stamps and raises its tail. It never
  sprays.
- **Opossum (server):** it freezes at 8 m and hisses at 3 m. If a player stays within 2 m, it plays dead for 20-40 s.
- **Black bear (server, rare):** it visits the Physical Plant's dumpster every 25-40 min, only when no player is within
  80 m. At 35 m it woofs and gallops into the woods.
- **Great blue heron (server):** it stalks the shallows. At 25 m it lifts off croaking and flies 100-200 m down the creek.
- **Ducks (server):** 6-10 mallards and 2-4 wood ducks. They flush at 12 m and circle back after 1-2 min. Wood-duck
  roost flights of 4-12 drop fast into the pond, squealing.
- **Beaver (server):** a V-shaped wake on the pond, a lodge in the bank and gnawed stumps. At 15 m it slaps its tail and
  dives.
- **Trout (local):** rise rings on the pools and the pond.
- **Owls.**
  - **Barred owl (local):** calls from the glen every 2-5 min, the 8-9 note "who-cooks-for-you", with occasional
    caterwauling duets.
  - **Great horned owl (local):** a deep 4-5 hoot duet in the north woods.
  - **Screech-owl (local):** a whinny in the frat yards.
  - **Seen great horned owl (server):** one sits on a woods snag. Crows mob it for a minute, then it glides off silently
    at 20 m.
- **Southern flying squirrel (local, rare):** one glide across a lamp-lit gap between two oaks every 10-15 min.
- **Dusk chip chorus (local):** robins' "tut", cardinals' chips, white-throated sparrows' "seet" and a Carolina wren's
  rattle, in the first minutes of each cycle, then fading out.
- **The farm.**
  - 6-10 beef cattle on the steep pasture (server). They stare at a player standing still by the fence, then slowly walk
    over.
  - A farm dog barks at the deer (sound).
- **Distant (local sound):** coonhounds baying on the north ridge, 1-2 bouts per 10 min.
- **Stretch:** a starling murmuration over the town, vultures on the water tower, killdeer over the gravel lot.

**Never** (all wrong for this place on October 18 1986):
- **Out of season:** fireflies, whip-poor-wills, nighthawks, chimney swifts, hummingbirds, cicadas, monarchs, frog
  choruses.
- **Wrong for the hour:** chipmunks (in their burrows by sunset), songbird song (calls only), snakes on the walks at 55 °F.
- **Wrong for the season's stage:** spotted fawns, velvet antlers.
- **Not here yet in 1986:**
  - coyotes (barely here), elk (from 1997), red wolves (from 1987 and 1991);
  - collared doves, armadillos;
  - Asian lady beetle swarms (from 1988), stink bugs (from 1996);
  - ash borer damage (from 2002), hemlock adelgid;
  - anthracnose-killed dogwoods.

**Behaviour:**
- **Reactions.** Animals react only to player-controlled pawns, measured in 3D. The views' pawn parked at Z 12000, the pig
  and NPCs don't count.
  - Sprinting (`IsSprinting()`) doubles the distances; crouching scales them by 0.6.
  - Campus animals react at half the distances of woods and farm animals.
  - A player who stands still for 15 s lets wary animals settle back one step and start feeding again.
  - Alarm spreads: a snort or a flush alerts every animal within 40 m, so a deer's blow makes the turkeys putt and run.
- **Idle life.** Vigilance loops: deer and geese lift their heads every 4-12 s, and ears and tails flick. Turkeys leave
  scratched patches in the leaf litter.
- **No contact.** No collision, no damage, no carrying. Animals never trap or block anyone.
- **No-go places.** Never a building, an interior box, a stair or a door apron. Roads only at authored crossings.
- **Paths.** Movement between replicated states is a pure function of (state, from, to, start time, seed) over an
  authored habitat graph, steered like the pig but analytic. Graph edges and every flight path are validated at build time
  against `FBlock` boxes + `DZ`, trunk positions, wall polylines, water polygons and the `SurfaceZ` slope, never with
  physics queries. Log `CALDER_WILDLIFE_PATHS clear=N/N`.
- **Feet.** On `SurfaceZ`, with swimmers on the water.
- **Recovery.** Scared animals come back within minutes.

**Multiplayer:**
- **Server-owned:** the deer, turkeys, fox, raccoons, skunk, opossum, bear, heron, pond geese, ducks, beaver, cattle and
  the seen owl.
  - **Why:** their reactions depend on where each player walks, which no clock can know. Local reactions would diverge:
    one player sees the deer bolt while the other sees it graze.
  - **How:** one replicated `AHouseCalderWildlifeDirector`, `bAlwaysRelevant`, spawned only when not a client.
  - It holds a FastArray of small states (id, state, start time, from, to, seed), sent only on change and dormant when
    idle. Add `NetCore` to Build.cs.
  - The server checks pawns at 10 Hz, and clients animate from the states.
- **Local and deterministic:** everything else, with `bReplicates=false`.
  - **Why:** nothing players do changes the sky or the ambience.
  - **How:** `GetServerWorldTimeSeconds()` plus fixed seeds gives every machine the same scene.
  - Squirrels react locally to the replicated pawns. A small divergence is harmless.
- **Every instance gets the same switches.** Pass `-NoCalder*` and `-CalderSceneTime` to every instance.

**How to build it:**
- **Meshes.** Rigid parts built from `FHallMesh` into runtime static meshes with 2-3 LODs, sharing one code-painted atlas.
  Leg segments and wings are shared within a colour family and scaled per instance. If a CC0 model would be clearly
  better, list it for me.
- **Components.** At most 56 Movable `UInstancedStaticMeshComponent`s, one per unique part:
  - NoCollision, no shadow, `SetCanEverAffectNavigation(false)`, `SetHasPerInstancePrevTransforms(true)`;
  - a species' components are hidden (hysteresis of at least 5 s) while none of its instances is within cull distance of
    a local view, so no more than 20 are visible at once;
  - a soft contact card under each ground animal.
- **Updates.**
  - One `BatchUpdateInstancesTransforms(..., bMarkRenderStateDirty=false)` per changed component per frame. Never
    `MarkRenderStateDirty()`, which `HouseLeafWind.cpp:560` does.
  - When an instance stops, send one last update with Prev = Cur so TSR doesn't smear it.
  - Prove an instance 300 m from its spawn still draws. If not, call `UpdateBounds()` + `MarkRenderTransformDirty()` at
    2 Hz at most.
- **Never:** an actor per animal, procedural-mesh rewrites, HISM, skeletal meshes, Niagara or Mass.
- **Motion.**
  - Species-true gaits: the deer's bound with the tail flagged, the turkey's head-bob and fly-up, the fox's pounce, the
    heron's S-neck strike, the goose's set-wing glide and splash-down.
  - **Ground animals:**
    - within 40 m, the full pose every frame;
    - from 40 to 150 m, the root every frame and the limbs at 15 Hz;
    - beyond that, or outside every local view's frustum (+10°), no update, except a moving animal's root at 10 Hz out to
      300 m.
  - **Flyers:** root and wings every frame while within 400 m and in front of a view; otherwise hidden.
  - Self-time the tick (`FPlatformTime::Cycles64`) and log p50/p95 per view.
- **Sound.**
  - **Synthesis.** Every call synthesised once on a worker thread, as `HouseCampusPolishSound` does.
  - **Voices.** Four pooled spatial one-shot voices, with shared attenuation, `PlayWhenSilent`, deadlines and occlusion.
  - **Loops.** The night bed, and the creek: one looping voice moved each tick to the nearest point of the creek
    polyline.
  - **Indoors.** Duck via both `HouseCampusInteriors::DistanceOutside` and
    `UHouseDuskAtmosphereComponent::DistanceOutsideHouse`.
  - **Caps.**
    - At most 7 wildlife voices, and one call per species at a time.
    - Peak `GetNumActiveSources()` of 24 or fewer.
    - Nothing above -36 dBFS RMS on the frat lawn, except a flock overhead.
  - **Ridge echo.** Far one-shots (honks, owls, hounds, the bear's woof) get one filtered echo, delayed by the round trip
    to the nearest ridge face (2-5 s) and 15-20 dB lower.
  - **Leaf crunch.** Unseen ground animals crunch through dry leaves, audible out to 30 m.
  - **What it replaces.** The night bed replaces the campus crickets. Re-voice the woods' 3-note owl only if
    `WOODS_AUDIO` stays green.
- **Switches.**
  - `-NoCalderWildlife` and `-NoCalderWildlifeSound`;
  - `-CalderSceneTime=<s>` pins the clock;
  - `-CalderWildlifeSoundDump` and `-CalderWildlifeRecord=<s>`;
  - console `Calder.Wildlife <species>` forces an appearance.

## Process
1. **Coordinate.** If another agent (Codex or Claude) is editing the project, wait until it has been idle 20 minutes.
   Snapshot the uncommitted state first. Commit nothing.
2. **One master plan, before building.** Write `SourceArt/CalderLandscape/PLAN.md` containing:
   - a top-down image of today's site;
   - the landform drawn in code: contours, hillshade, frozen zones, walls and water, with long sections through the quad
     axis, College Hill and the glen;
   - a table of the buildings (pad, `DZ`, fall, treatment, risers), and every road, lot, walk and stair with its grade;
   - the tree re-seat and the crc proof;
   - the FreeRoam legs and how each stays walkable;
   - the landscape palette;
   - the wildlife roster: count, habitat, cadence, reactions, call, and owner with the reason;
   - the perf budgets.

   **Decided already** (each line is mine to edit):
   - *I allow* re-seating only the Z of `TreeA`, `TreeB` and `ForestDistant` after `RecordOriginalTrees`. The crc still
     hashes today's transforms, and frozen-zone trees stay bit-identical.
   - *I allow* grading the house's side outside the frozen zones: the north foothills, the east hills, the west farm edge,
     and Calder Dr north of Y 19800. This overrides my earlier rule that anything on the house's side is stop-and-ask,
     for these areas only.
   - *I allow* the plan to decide which house-side features are reachable. Crow Ledge and the farm field must be, so their
     animals can be reviewed.
   - *I allow* lifting the clock tower onto Tower Knoll by one runtime `Lift`. Its XY never changes.
   - *I allow* sinking the town and raising the ridges.
   - The stadium keeps its XY and Z.
   - The fall respawn is not touched. Walkable ground stays at least 400 cm above FallLimit, computed in the review the way
     `StoryCampaign.cpp:1761-1763` does.
   - Grades are 1986's, as above.

   Proceed with the plan. Only stop to ask me if it needs:
   - any change inside a frozen zone, or to `Grade()`, `HouseWoods::Height` or `HouseWestField::RaisedHeight`;
   - moving the stadium or the tower in XY, a new stadium Z, a building that blocks the tower's sight line, or a new crc;
   - walkable ground below FallLimit + 400;
   - changes to `StoryCampaign`, the checkpoints or the player's movement (including a slope speed factor);
   - saving into the map, a navmesh, or a Landscape, PCG, Water, Niagara or Mass module;
   - more than 53 lights;
   - a house room;
   - re-enabling `PropertyBoundary`, or any invisible or player-only collision;
   - changing the global height fog, the sky or the dusk sun (`HouseExterior.cpp:2351-2366`,
     `HouseDuskAtmosphere.cpp:186-196`);
   - changing the shared `ExteriorTree*` meshes or LODs as the house sees them;
   - changing a review threshold or waypoint beyond re-basing it on `GroundHeight`/`DZ`, or a FreeRoam leg that can't be
     kept walkable;
   - hiding more than 40 `TreeA`/`TreeB` trees;
   - lowering a spot height by more than 10%.

   **Don't wait on:**
   - **Editor-Python runs.** Batch them, ask me once to close the editor, and carry on meanwhile.
   - **Downloads.** List them in STATUS.
3. **Build in this order, with screenshots and a short note after each step:**
   - **(1a) Plumbing at zero relief.** `GroundHeight` = `Grade`, `SurfaceZ`, `DZ` = 0 with one `Frame()`, the `OpenTop`
     split, the draped primitives (dormant where relief is 0), the view offset, and reviews re-expressed relative to
     `GroundHeight`/`DZ`.
     - Record the baselines and log FallLimit.
     - List the map-placed actors.
     - Get `-CampusMultiplayerReview` running unchanged on today's build with a new launcher.
     - Classify every campus-side Z literal as routed, canonical-frame, frozen or stadium-fixed, with none left
       unclassified.
     - **Gate:** the crc, `grounds=69240`, `campusFrontVertices`, `HOUSE_EXTERIOR built instances=53740`, and every review
       identical.
   - **(1b) Tiles at zero relief.** Tiles, collision and skirts replace `ExPatch` outside the frozen rectangles.
     - The first `-CalderTerrainReview` checks accuracy, holes, the frozen zones and collision-ready.
     - A perf A/B of that swap alone.
     - The material batch is ready.
   - **(1c) The relief, in one step with every consumer.** Landform, pads, the knoll, the South Lot and the Hill, the
     road, lot and walk corridors with the walls, stairs, ramps and rails the grades need, the tree re-seat and the hedge
     segments.
     - `-CalderReliefScale=<0..1>` (debug only, default 1) is for bisecting failures. All reviews pass at 1.
     - The multiplayer test checks the terrain hashes.
   - **(1d)** Guardrail, drainage and footings.
   - **(1e)** The creek, pond, falls, bridges, overlooks, the house side and edge closure.
   - **(1f)** The outer valley, the town, the cove walls, Gosling Knob and the ridges.
   - **(2a)** Material. **(2b)** Ground cover. **(2c)** Trees and colour by altitude. **(2d)** Rock, walls and the farm.
     **(2e)** Water and mist.
   - **(3a)** The wildlife framework and director, with the deer end to end, including the deer's two-process test.
     **(3b)** Sky: geese, crows, bats. **(3c)** Ground animals. **(3d)** Water animals. **(3e)** Sound.
     **(3f)** `-CalderWildlifeReview`.
   - **(4)** Performance and every review.
4. **Keep a log.** This is several sessions of work. Keep `SourceArt/CalderLandscape/STATUS.md` current: what's done,
   what's next, open decisions, "Shrunk:", pending downloads and editor runs, and the latest results. Leave every step
   building and passing.

## Hard constraints
- **Lights.** Exactly 53 exterior lights today, so the new-light budget is zero; all glow is emission. The interior pool
  stays at 6.
  - Spawn every new actor with the exterior actor as its Owner, since `HouseWoodsPass::Audit` counts only owned lights.
  - The new reviews count point and spot lights world-wide outside the house.
- **Runtime build.** Everything is built at runtime in code; never save into the map.
  - No editor-only APIs, no `ALandscape`, no Nanite, no runtime virtual textures.
  - New materials come only through editor Python, and are listed in the README.
- **Fixed anchors.** The frozen zones keep their ground, geometry, collision and look. The stadium keeps its XY and Z, and
  the clock tower its XY.
- **Trees.**
  - `WOODS_PASS_ORIGINAL_TREES ... crc=0f3b2bb7` stays identical. Prove it from the game log.
  - Never touch `FExBuilder::Rng` (20261009) or the NoGo calls before `Plants()`.
  - Trees that land in new hard features are hidden, never moved, and counted.
- **Clock tower.** The sight line stays clear, and `-ClockTowerReview` passes with `CLOCK_TOWER_CROWS burst=12`.
- **Names and marks.**
  - Code or CC0 only.
  - No real brands, schools, people, places, peaks, flags or seals. Keep Calder, the Geese and Dunmore.
  - Nothing from after 1986: no tactile warning domes, no net-wrapped bales.
- **Walkability.**
  - **Collision.** Everything reachable collides.
  - **Steps.** Stair risers 14-18 cm, treads at least 30 cm.
  - **Grades.** The 1986 limits above. Anything over 40° is a wall, cut or rock with Unwalkable collision.
  - **Climbing.** Assume the player can get over anything under 150 cm. Below every wall, rail, parapet, terrace edge or
    ledge inside the walkable area there is walkable ground that leads back to the walk network. At the ±32000 edge, the
    fall respawn catches anyone, as it does today.
  - **Rails.** A rail or wall of at least 90 cm at any drop of 75 cm or more beside a walk, stair, road or overlook.
  - **Gaps.** No gap of 36 cm or more under any wall, hedge or fence.
  - **No traps.** No pit without a way out, and no new invisible walls.
  - **Today's FreeRoam waypoints are a contract.** Every straight leg stays walkable: stairs, ramps and slopes of 1:3 or
    flatter only, with no wall, rail, cut, water or sunken edge across it. Put the Senior Steps, the knoll stair and the
    panel's opening on the Y -10800 axis. Never move a waypoint.
- **Story and multiplayer.** Don't touch the checkpoints or `StoryCampaign`. Builds are deterministic on every machine.
- **Performance.** The frame is render-thread bound. Use medians of 3+ alternating rounds (±0.5 ms is noise).
  - **Budgets against `p8p`:**
    - street views +0.8 ms (front_street 22.16, street_along 14.35);
    - campus eye-level views +1.5 ms (detail_layout_quad 8.46, detail_campus_quad 8.08);
    - the aerial (13.57) +2.5 ms;
    - Tier A insides within noise.
  - **Wildlife and load:**
    - wildlife at most 0.3 ms p95 on the game thread;
    - load within +3 s of today's `HOUSE_EXTERIOR ms=9993`, counting every `CALDER_* build ms` and the time to
      collision-ready.
  - **Draws.** At most 65 terrain tiles, 24 landscape ISMs and 56 wildlife ISMs (20 visible at once). Test
    `r.InstanceCulling.OcclusionCull=1`.
- **A/B switches.**
  - `-NoCalderTerrain`, `-NoCalderLandscape` and `-NoCalderWildlife` each give back today's state exactly.
  - `-NoCalderTerrain` restores `DZ`, `Lift`, tree Z, the view offsets, the hedge heights, the town `G`, the ridge bases
    and the far lawn.
  - `-NoCalderWildlifeSound` restores the campus crickets and the woods' owl exactly.
  - `-NoCampusLayout` forces `-NoCalderTerrain`.
  - The other `-NoCampus*` switches keep working. With `-NoCampusPolish` and the terrain on, the terrain material shows
    with no lawn overlays.
- **Naming.**
  - **Never start a name with:** ExteriorCar, ExteriorTree, ExteriorPole, ExteriorNear, ExteriorFar, ExteriorStreetDecor,
    ExteriorParkingLot, ExteriorBushParking, CampusFallTree, ExteriorWoods, ExteriorAppalachian, ExteriorForestDistant or
    ExteriorGrass.
  - Leave `EX_Ridge*` (except re-basing), `EX_CampusSeats` and `WoodlandHollowMist` alone (exactly 3; no new mist carries
    that tag).
  - New views are `detail_*`, appended at the end.
- **Code rules.**
  - Unity-build safety: named Impl namespaces, no using-directives, clean under -Wshadow.
  - Build once with `-DisableUnity` at the end of each phase.
  - New self-exiting reviews join the three `SelfExiting` lists (`HouseCampusLayoutReview.cpp:35-37`,
    `HouseCampusInteriorsSmoke.cpp:49-51`, `HouseCampusInteriorsReview.cpp` ~:421).
- **Don't take over the screen.** Build with Build.sh while the editor stays open. Capture with
  `-game -RenderOffScreen -UserDir=<scratch>`, and use `-muteaudio` for sound.
- **Hands off.** Never touch the Unity copy. Commit nothing.

## Verification and deliverables
- **`-CalderTerrainReview`.**
  - **Eye-level proof (first, and the point of this job).**
    - **Method.** From each spot, start at ground + 170 cm. March the whole ground (`GroundHeight`, the outer valley and
      the ridges, with no buildings) every 5° of azimuth out to 30 km. Log
      `CALDER_SKYLINE spot=<n> minDeg medDeg maxDeg relief150m`.
    - **Pass at P1-P4, P6 and P7:**
      - median elevation angle at least 8°;
      - every azimuth at least 3°, except the declared notches (the Calder Branch valley and the water gap), which must
        show at least 3 ridge layers;
      - the south sector at least 14°;
      - ground range within 150 m of at least 8 m outside the frozen zones.
    - **The spots,** as views `detail_calder_eye_P1..P8` with the same framing at every step:
      - **P1, shot 1 retaken** (≈(-12000,-6450), facing +X): standing on the raised Old Main and Harlow lawn, with the
        street wall dropping to the sidewalk on the right and the campus climbing on the left; past the X 8700 junction
        the east cove wall rises at least 6° above the vanishing point.
      - **P2, shot 2 retaken** (≈(-4000,-17800), facing +X): the South Lot tiers stepping up on the left, the Physical
        Plant on its bench ahead, and at least 3 ridge layers.
      - **P3** (2380,-3960), facing -Y: the stadium rim, the South Lot tiers, College Hill's crest and the near ridge
        stacked, with the tower on its knoll in frame.
      - **P4:** the foot of the Senior Steps, facing -X, with every flight up to Mercer's terrace.
      - **P5:** Founders' Overlook, facing yaw 205. The town 45 m or more below, the river, 4 or more ridge layers, and
        Gosling Knob black against the afterglow.
      - **P6:** the footbridge at the falls, looking upstream. The glen walls 15 m or more above eye, the falls and the
        mist.
      - **P7:** the dam footbridge, facing +Y over Gander Pond, with geese on the water and the foothills behind.
      - **P8:** Crow Ledge, facing -Y. The house roofs below, the street, the campus climbing, the tower and the near
        ridge: the whole cove in one frame.
  - **Accuracy:** collision matches `GroundHeight` within 3 cm at 20,000 points.
  - **Holes:** none, and no tile cracks.
  - **Limits:** every walkability limit.
  - **Buried or floating objects:** none. Each placer registers its foot points at build time, and every one lies within
    -10..+3 cm of its surface; list the worst 20.
  - **Reachability:** a flood fill reaches every entrance, gate, overlook, stair and path. It seeds from the foot of every
    drop and finds no traps.
  - **Sight line:** the tower's is clear against buildings and canopy bounds.
  - **Frozen zones:** a trace grid matches today within ±0.5 cm.
  - **Water:** none above its banks, and depths within limits.
  - **Logged:** lights ≤ 53 world-wide, the crc, FallLimit, the lowest walkable Z, the terrain hash and collision-ready.
- **`-CalderLandscapeReview`.**
  - **Placement and settings:** counts; nothing on walks, roads, lots, stairs or water, or in buildings; no shadows;
    cull distances set; names legal.
  - **Macro views** `detail_calder_macro_*`, taken at 1-1.5 m and judged next to the best `detail_woods_*` views:
    - a stone wall with moss, polypody and weep holes;
    - a fresh cut with clay and roots;
    - the creek edge;
    - broomsedge with asters;
    - oak litter with acorns;
    - a rutted desire path on a slope;
    - a mown bank;
    - a boulder with lichen;
    - a buck rub;
    - an orb-weaver web under a lamp.
- **`-CalderWildlifeReview`.**
  - **Method.** It teleports the pawn to 1.5 times each species' outermost reaction distance, forces rare species with
    `Calder.Wildlife`, then walks in with real input, sprinting and crouching too.
  - **Reactions:** each within ±20% or ±1.2 m, whichever is larger.
  - **Movement:** no stalls; no animal in a building, on a stair or on a door apron; `CALDER_WILDLIFE_PATHS` all clear.
  - **Feet:** gap and planted drift at most 3 cm at p95.
  - **Coverage:** every species screenshotted (`detail_wild_*`) and every call recorded (with `-muteaudio`).
  - **Budgets:** voices and active sources within their caps, and the tick within its budget.
- **Updated reviews** (re-based, never loosened):
  - `-CampusLayoutReview` samples `SurfaceZ`.
  - `-FreeRoamWalkReview` measures drops from `GroundHeight`. Add a hill route (up the Senior Steps, over College Hill,
    into the glen and round the pond) and a north route to Crow Ledge.
  - `-CampusInteriorsReview` measures relative to `DZ`.
- **Multiplayer, two processes.**
  - **Harness.** `HouseCampusMultiplayerReview.cpp` exists but has never been run and has no launcher. Write the launcher
    in (1a): a listen host and a client, headless, through the real lobby.
  - **Checks:**
    - the terrain hashes match;
    - no corrections on the hill route: zero `*** Server: Error for` lines with `-ExecCmds="p.NetShowCorrections 1"` on
      the host;
    - the client sees the host's deer flee within 0.5 s.
- **Screenshots, after every step,** with the eye 170 cm above the local ground:
  - top-down before and after, and the contour map;
  - P1-P8;
  - the macro views;
  - one per species;
  - the existing `detail_stadium_*`, `detail_campus_*`, `front_street`, `street_along` and `detail_clocktower_far`.
- **Frame times.** The `phase4_perf.sh` method with `-CalderSceneTime` pinned: three alternating rounds per switch.
- **Sound proof.** `-CalderWildlifeSoundDump` WAVs, `-CalderWildlifeRecord=<s>` recordings and spectrograms; `WOODS_AUDIO`
  stays green.
- **Reviews.** All must pass, reporting the lights, the crc, FallLimit and the lowest walkable Z:
  - the three new ones;
  - `-CampusWoodsReview`, `-SurroundingsPolishReview`, `-FraternityBoundaryReview`, `-ParkingLotWalkReview`;
  - `-WoodsPassReview`, `-WoodsSceneReview`, `-WoodsWalkReview`, `-WestFieldReview`, `-WestFieldWalkReview`;
  - `-HouseExteriorWalkReview`, `-HouseTreesReview`, `-ClockTowerReview`, `-FreeRoamWalkReview`;
  - `-CampusLayoutReview`, `-CampusInteriorsReview`, `-CampusInteriorsSmoke=All`, `-HousePigAnimalSmoke`;
  - the house room smokes.
- **Write-up.**
  - `SourceArt/CalderLandscape/README.md`, covering all three phases and any new assets;
  - a per-file `FILES.md`;
  - a patch against the snapshot.

## Added by me when starting the job (2026-10-15)
"just make sure to not touch the main fraternity house / the court, party area and parking lot please"

Read as: zero change to the house (rooms, lot, driveway), the basketball court, the backyard and rear courtyard, the Fall Jam field (the party area) and the house parking lot: not their ground, geometry, collision or look, and not their own source files (HouseFallJam*, HouseParkingLot*, HouseParkingDetail*, HouseBasketballCourt*, HouseRearPorch*, the house rooms). Wildlife never enters them.

"also the pig and the woods should not be the modle the pigs look terrible and should be redone to by hyper realistc as well as the woods"

Read as: replace "THE WOODS AND THE PIG ARE THE MODEL". The quality bar is hyper-realism (judged against real photographs of the same subject), not the existing woods or pig. Two jobs are added: redo the pigs to be hyper-realistic (the animals only; the pigsty room itself is still a house room) and redo the woods to be hyper-realistic (their look; the tree positions and the crc, the trail and the hut stay). "Do not edit the woods' own terrain or batches" no longer applies to their look.

Decided with me on 2026-10-15 (questions):
- Assets: free CC0 only (Poly Haven, ambientCG and similar). Claude lists each file (name, site, size) and downloads only what I approve. Code-built where no good CC0 asset exists.
- Order: terrain first (Phase 1), then the woods and pig redo, then the wildlife.
- Later the same day: "you can also get anything you need from fab as long as its free". Free Fab assets are allowed too (Fab Standard License or CC0). I add them to my library myself; Claude never signs in or accepts terms for me.

"remeber the game is supposed to have a horror, mystery, nostiaglic feel"

Read as: every landscape, sound and wildlife choice serves horror, mystery and nostalgia, not postcard prettiness.
- **Nostalgia:** warm sodium and window light, the 1986 town lights below the bluff, homecoming leftovers.
- **Mystery:** mist pooling in the glen and hollows, paths that vanish into laurel hells, old things half-hidden (a sealed mine portal with a rusted grate, a hillside family cemetery with invented names, the dead chestnut snag, a fire tower on the ridge), sounds with no visible source.
- **Restrained horror:** deer that stand and stare, crows that follow a player along the treeline, eyeshine at the edge of the lamp light, the insects falling silent near the glen, a screech-owl whinny, one distant fox scream.
- **Limits:** set dressing only, with no story hooks, triggers or checkpoints, and no new lights.
