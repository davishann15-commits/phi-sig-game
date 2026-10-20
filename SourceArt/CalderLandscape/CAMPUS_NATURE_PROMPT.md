# Prompt: Calder State campus nature, real and detailed

Copy everything below the line into a new session.

---

Make the **nature on the Calder State campus** feel real, lived-in and detailed at eye level. That covers the trees, shrubs, lawns, leaves, moss, dirt and water a student walking between classes would actually notice. The setting is a state college in the Appalachian mountains during Homecoming week, **late October 1986, at dusk**.

The game's tone is **horror, mystery and nostalgia**. The campus should feel like a real place you half-remember, beautiful but slightly too quiet. Nature carries that feeling. Avoid park-perfect landscaping, and avoid creepiness for its own sake.

## Project facts
- **The project:** an Unreal Engine 5.8 game at `~/Documents/Unreal Projects/SeniorSendoff`, developed on an Apple Silicon Mac (Metal SM6).
- **The world is built at runtime in C++.** The campus layout lives in:
  - `HouseCampusLayout*.cpp`, `HouseCampusFallQuad.cpp`, `HouseCampusPolish*.cpp` and `HouseCampusStreetDetail.cpp`;
  - `HouseExteriorCampus.cpp`, `HouseExteriorNature.cpp` and `HouseLeafWind.cpp`;
  - the clock tower files.
- **The ground has real relief now** (the Calder mountains job, `SourceArt/CalderLandscape/MASTER_PROMPT.md` and `STATUS.md`).
  - Place everything on `HouseExteriorSite::GroundHeight` / `HouseCalderTerrain::SurfaceZ`, never on flat Z.
  - Nothing may float, sink, or stand on a wall, stair, ramp or walk.
- **Build and run:** `bash Saved/CalderWork/tools/build.sh` and `bash Saved/CalderWork/tools/run_game.sh <name> <secs> <args>`. Always run headless with `-RenderOffScreen`, and use `-nosound` unless you're testing sound.
- **Assets already in the project:**
  - Megascans Trees European Beech and Norway Maple (they have a **season slider**, so use it for October colour);
  - the `/Game/Environment/Megascans` set: Beech Fern, Lady Fern, Black Locust, Raspberry, logs, stump, cliff, boulder;
  - the CC0 textures under `/Game/SeniorSendOff/Calder`;
  - Norway Spruce (a Megaplant).
- **Megaplants (the spruce and any later oak or birch) aren't proven to render on Mac yet.** Test one before relying on them.
- **More free assets are listed** in `SourceArt/CalderLandscape/FAB_SHOPPING_LIST.md`. If something important is missing, ask me to add it from Fab, with exact links. You can't add Fab items yourself.

## Scope
**In scope:**
- the quad and the campus greens;
- walks and their edges;
- building foundations and entrances;
- the street trees along the campus streets;
- the clock tower knoll;
- the stadium's surroundings (not the field or bowl);
- parking edges on campus;
- the campus edges, where mowing stops and the woods begin.

**Out of scope:**
- **Owned by the mountains job:** the deep woods, the far land, wildlife and the mystery spots. Coordinate with it rather than doing its work.
- **Never touch:** the fraternity house and its rooms, the basketball court, the party / Fall Jam area, the house parking lot and driveway, and any of their source files (HouseFallJam*, HouseParkingLot*, HouseParkingDetail*, HouseBasketballCourt*, HouseRearPorch*, the house rooms).

## What "real" means here (build toward this checklist)
1. **Trees with age and species variety.** A real 1980s campus has mature planted trees 12–25 m tall:
   - sugar and red maple, pin and white oak, tulip poplar, sycamore along the creek or street;
   - a ginkgo or two by the oldest buildings;
   - dark hemlocks and white pines for contrast.

   No two neighbours should be identical clones: vary species, size, lean, crown and October colour. In late October some trees are already bare, some are peak orange or red, and the oaks are still brown-green. Each tree's colour should follow its species and exposure, not random noise.
2. **The ground tells the story.**
   - Lawns are worn into dirt desire paths where students cut corners.
   - Grass is thinner under the trees, and moss grows in the shade.
   - Mown edges meet unmown banks.
   - Mulch beds by the buildings hold Homecoming mums, spent annuals and a few pumpkins by doors.
3. **Leaves gather where wind and feet put them:**
   - against curbs, steps, building corners, fences and walls;
   - in drains and puddles, wet and flattened on walks;
   - raked into piles in one or two spots.

   They thin out on open lawn, and thin out under the trees that are still full.
4. **Old details:**
   - ivy on the oldest brick, and moss and lichen on north faces, stone walls and steps;
   - roots heaving a sidewalk slab, tree grates, a stump where a tree was cut;
   - acorns and seed pods under the oaks, a fallen branch;
   - puddles in low spots, a muddy edge where the walk drains.
5. **Foundation plantings:** clipped yews and boxwood, rhododendron and azalea, overgrown in places the grounds crew forgot.
6. **Transitions:** a density gradient from the manicured core, to shaggier edges (brambles, raspberry, ferns, saplings), to the woods. No hard line where campus ends.
7. **Motion and air:**
   - leaves and branches moving in the existing wind (`HouseLeafWind`);
   - low ground mist settling in hollows at dusk.

   Ask before adding any new Niagara system or changing global fog.
8. **Dusk-correct look.** Colours and values must read right under the game's current dusk sky and sun, with no glowing saturated foliage. Check every change in captures at that light.

## Realism rules
- **Variation everywhere:** mix species, rotation, scale (±15%), tilt on slopes for small plants, and per-instance tint. Use **clustered distributions** (Poisson or cluster noise driven by shade, slope, wetness and nearness to walls or walks), never uniform scatter.
- **Correct scale:** check sizes against a 180 cm person. Fab Megascans sizes come from their scan metadata.
- **No trunk** on a walk, step, door path, sight line to the clock tower, or any of the story or checkpoint routes. Keep walks and doors clear for the reviews.
- **Collision:** ground cover and small plants have none. Trees get a simple trunk capsule only where the player can reach them.

## Hard constraints (stop and ask me before breaking any of these)
- Commit nothing; leave commits to me. Never touch the old Unity copy at `~/Senior Sendoff Real`. Don't take over the screen.
- **Codex shares the project:** wait until it has been idle for 20 minutes before editing. If another Claude session is building or running reviews, take turns: only one build or game run at a time.
- **Existing site trees:**
  - Don't move them; the tree transform crc `0f3b2bb7` must hold.
  - Don't swap the shared `ExteriorTree` meshes, and don't hide more than 40 TreeA/TreeB.
  - New trees are additions. If you want to replace existing tree meshes, show me before/after captures first.
- **No changes to:** `Grade()`, `HouseWoods::Height`, `HouseWestField::RaisedHeight` or the landform data; StoryCampaign, checkpoints or player movement; navmesh, Landscape, PCG, Water or Mass; anything saved into a map.
- **Limits:** no more than 53 lights, and no invisible collision.
- **Reviews:** don't loosen review thresholds or move waypoints.

## Performance
- **Measure first:** capture a baseline before any change with `Saved/CalderWork/tools/perf_ab.sh`.
- **Use ISM, not HISM.** Runtime-built meshes can't be Nanite.
- **Imported assets:** decide Nanite per asset, with an A/B measurement.
- **Material usage flags:** materials used on instanced meshes need `used_with_instanced_static_meshes` set, or a packaged build shows the default material.
- **Textures:** cap at 2K for packaging.
- **Budget:** no worse than +5% render-thread time on the standard views. If the look needs more, show me the trade-off with numbers.

## How to work
**Usage pacing:**
- Work lean. Use one agent for each disjoint set of files, cheaper models only for mechanical stages, and an adversarial verifier on every substantive change.
- Never skip verification to save usage.
- Check usage before big fan-outs. Near the limit, write `CODEX_HANDOFF.md` and hand off.

**Phases:**
1. **Survey.** List every current campus plant, tree and ground-cover system: what it is, where it comes from in code, and its count. Capture baseline eye-level views (new views are named `detail_*` and appended at the end of the list), and write a short "what reads fake" critique with images.
2. **Plan.** Draw a zone map: core lawn, beds, street trees, knoll, stadium apron, edges, wet spots. Give each zone its species, density, ground mix and leaf behaviour. Show it to me before building.
3. **Ground.** Lawn wear, desire paths, moss, mulch and leaf-litter layers, with drift against obstacles.
4. **Trees and shrubs.** Placement, species mix, October colour, foundation plantings.
5. **Small detail.** Leaf piles, acorns, branches, ivy, puddles, mums, root-heaved slabs, the edge transition.
6. **Verify.** Run every review the mountains job uses. Each must pass, or keep the same verdict it had before:
   - CampusLayoutReview, CalderTerrainReview, FreeRoam;
   - both interiors walk-throughs;
   - CampusWoods, Surroundings, Fraternity, ParkingLot, WoodsPass;
   - ClockTower, WestField and Woods;
   - the house smokes and the MP test.

   Then capture before/after eye-level views at dusk from at least 12 spots, run perf A/B, and check that nothing floats or clips (sample-based).
7. **Write-up.** Update `STATUS.md` with a short write-up and the before/after images, and save a patch. Note anything that needs a Fab download from me.
