# Front campus and rear woods

Unreal project: `/Users/Stewart/Documents/Unreal Projects/SeniorSendoff`.
Map: `/Game/Story/Maps/Chapter01_House`.

## Layout

- Replaced the previous exterior neighbour builder with a dense front-road college campus: 16 detailed academic/dorm buildings, 12 farther campus buildings, a clock tower, paths, benches and banner lamps.
- Relocated the baseball stadium to world X=2400, Y=-13300. Stepped seating, press box, baseball diamond, scoreboard and three flood-light towers.
- Removed all previous rear neighbour/stadium/background building geometry.
- Rear woodland starts at Y=4650, beyond the basketball court. Two close rows add 34 detailed interactive autumn trees; 859 shared low-cost tree instances fill the deeper woods with photographed leaf clusters.
- Three mountain ridges span the rear horizon, starting at Y=32000, 65000 and 112000. Their heights were increased after the first rendered check to make them visible above the canopy.

## Performance choices

- Three real stadium lights; campus lamps, facade windows and signage use material glow.
- Campus meshes use 40 m spatial groups, without dynamic shadow or indirect-lighting contributions. House mesh grouping, materials and lighting are unchanged.
- Deep woodland uses three shared meshes with no collision, shadows or indirect-lighting contributions. Reachable close trees retain existing trunk collision and hit reactions.
- The first rendering pass exposed excessive cost and opaque canopy blobs. Final geometry uses photographed leaf clusters and approximately five million fewer total triangles; rear view frame rate improved from about 25 FPS to 33–35 FPS in the same 1280×720 quality-2 camera checks.

## Evidence

- `evidence/build6.log`: successful final compile.
- `evidence/views/CampusWoodsReview/result.txt`: actual game placement, region/collision, rear-building removal and preserved court tests.
- `evidence/views/HouseVisualReview/review.json`: seven native rendered camera captures and frame-time measurements.
- `evidence/performance.json`: first implementation versus final implementation, not an original-scene benchmark.
- `evidence/map-protection.json`: byte-identical saved house map before/after. Interiors, saved furniture, intentionally empty rooms, club expansion and door behavior preserved.
- `evidence/court`: actual basketball pickup, bounce, both basket shots and porch/court path results.
- `evidence/reopened.json`: target editor map reopened after the development-build checks.

The camera sweep is an assisted actual-game review, not a manual player traversal or multiplayer test. The separate court review performs actual player movement and existing interaction/physics. No Unity files were edited.
