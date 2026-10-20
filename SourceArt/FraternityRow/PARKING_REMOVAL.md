# Authorized parking removal — scope revision, 18 October 2026

Stewart's latest instruction: **“honestly you can get rid the the main house parking lot”**.

This supersedes the original brief's parking-lot protection. It authorizes removing the west main-house parking feature and using its footprint for Fraternity Row. It does not authorize enlarging the map or changing the main house/rooms, basketball court, Fall Jam, east driveway, road, woods fence, terrain, story or movement. The wildlife source/engine freeze and required baseline still apply.

## Revised design

Keep five Row houses and the already-shown ground-floor layouts. The Tudor uses the existing grass; the Queen Anne now occupies the western part of the former lot, with lawns and walking approaches around it. This revised position avoids branches that entered the original Queen Anne rooms while retaining every original tree. Its body is X -7080..-5680, Y -1700..-200; the roof's east edge is -5630, preserving 120 cm to the fixed old parking walk at X -5510. Autumn ground cover, the Tudor picnic table and abandoned Queen Anne food table remain outside the houses and protected routes. Maintain the connection to the main-house perimeter walk.

The lot's body is X -7200..-3700, Y -2500..-300. Its dedicated entrance strip is X -5800..-5200, Y -4190..-2500; replace that lot-specific apron with pedestrian approach ground, preserving the road and sidewalk it meets. The existing connector at X about -4490..-3400, Y -285..-105 remains a pedestrian connection. It must be emitted independently if the parking builder no longer runs.

## Removal inventory for implementation

| Current source | Required scoped change after baseline |
|---|---|
| `HouseExterior.cpp`, `Streets()` | Omit only the west-lot slab and dedicated entrance slab. Keep base grade, original planting NoGo exclusions/RNG, east driveway and other lots/roads. Check that supporting ground exists beneath both removed overlays. |
| `HouseParkingLot.cpp` / its integration | Omit west-lot curbs, wheel stops, bay paint/symbols, signs, mulch/islands and dedicated lot plants. Preserve the house pedestrian connector separately. Retain any clearance needed around that connector. |
| `HouseParkingDetail.cpp` / its integration | Omit lot-specific stains/paint/props, bollards/signs/dumpster where they belong to this removed feature. Inspect extents: some lot props extend east of the nominal rectangle. Protect the main-house perimeter walk. |
| `HouseExterior.cpp`, vehicle generation | **Keep both old lot-row generation calls:** `Vehicles()` consumes the shared RNG before `Plants()`. Generate/record the original site exactly, then filter west-lot cars from the rendered lists after tree generation. Deleting the source row calls would change the required tree CRC. Disable the independent special open-bay muscle-car insertion; place the requested period sports car later in a planned Row garage/gravel area if needed. Leave street, east/rear-lot and Fall Jam vehicles unchanged. No gameplay/story files need changing. |
| `HouseExterior.cpp`, pole/light generation | Remove the two west-lot steel poles at (-7160,-2600) and (-3740,-2530), their matching light specs and both pier bases in lot props. Identify paired light heads using the source pole transform/lens offset; do not broadly filter unrelated lights. Keep original site generation order, then suppress the corresponding rendered poles/lights. Road utility poles/wires and other lot lights remain. |
| `HouseExteriorNature.cpp` | Existing lawn logic excludes the lot, entrance and parking mulch. Add scoped Row ground cover using independent RNG; avoid silently reshuffling global plant/tree generation. Keep original tree transforms. |
| Parking/exterior reviews | Make feature-mode expectations explicit: legacy mode retains old lot assertions; upgraded Row mode asserts the lot/cars/curbs/paint/signs are absent and new ground/access is supported. Preserve actual walking waypoints and tolerances. Never report old parking assertions passing after simply skipping them. |

The old parking route stays exactly: (-5510,-2800) -> (-5510,-1400) -> (-4320,-1400) -> (-4320,-600) -> (-4320,-195) -> (-3480,-195). `PLAN.json` retains it, and the new approach paths follow it. Solid replacement props stay outside a 120 cm path buffer in source-plan checks. Native capsule sweeps and player walks remain mandatory. Preserve `Landscaping()`'s existing-bush clearance filtering for the connector even when its separate parking-shrub additions are disabled.

Remove both old lot lights with their poles/bases. With six proposed new Row lights, the nominal expected exterior count becomes **53 - 2 + 6 = 57**, subject to fresh baseline confirmation. The job still adds only six new shadowless small-radius lights; the shared interior pool remains six. Update exact feature-mode count assertions accordingly.

Old feature counts (cars, blue bays, symbols, curbs, parking plants) no longer describe the authorized scene. Replace those feature assertions with explicit absence/support/access assertions, keeping the legacy branch for the A/B restore. Keep unaffected route coordinates, geometric tolerances and performance thresholds. Update `HouseExteriorWalkReview`'s west-lot entrance surface expectation to the replacement ground/path material at the same probe; leave its east-driveway asphalt expectation intact.

## Current state

**Authorization and plans are updated. The actual parking lot is still present in the game.** No runtime Source/Config/Content files or maps have been changed by this Row task. Capture before images and performance with the lot still present after the shared slot is released, then integrate removal together with the Row upgrade. `-NoFraternityRowUpgrade` must restore the original lot as well as the original three-house scene for a complete cost comparison.
