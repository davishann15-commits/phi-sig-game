# Fraternity Row infill plan

**Latest user constraints: do not enlarge the map; use the existing grass on the Row side of the main house; the main-house parking lot may be removed.**

Five Row houses, plus the separate protected main house. Keep the original three footprints. The Tudor uses the existing field; the Queen Anne occupies the authorized former parking area, with lawns and walking approaches around it. This revision avoids actual branches seen inside its earlier rooms while preserving the original trees. No road extension, terrain change, woods-fence movement, or map save. `PARKING_REMOVAL.md` records the authorization and removal inventory.

![Proposed site](PLAN.png)

## House identities

The four-letter combinations below are original provisional fictional identities; no real chapter names, crests or reference photographs are reproduced.

| ID | Exterior and letters | Condition / interior story | Own backyard, to existing Y 4520 fence |
|---|---|---|---|
| R1 | Retained red brick; ΩΞΛΗ; tired paint and dark shutters | Quiet dark house, drawn curtains, one upstairs emissive lamp; still TV lounge, hearth chapter room, dining/kitchen, WC | Overgrown old garden, rusted swing frame, small garden shelter, leaf-choked trail |
| R2 | Retained clapboard; ΨΗΞΡ; restrained cream and green | Tidy house; raked porch mums and Homecoming wreath; orderly lounge, chapter room, dining/kitchen, WC | Stone patio, maintained garden, tools put away, garden shed |
| R3 | Retained brick Colonial and white portico; ΗΞΩΡ | Wrecked postgame party; furniture moved to edges, keg, cans, original fictional composites, CRT and pay phone; chapter room replaces any need to excavate a basement | Sand volleyball area, garage, gravel pad with period car, dragged couch, muddy shortcuts, bagged trash |
| R4 | New Tudor Revival; ΛΩΗΞ; steep gables, half timber, recessed casements, substantial entry porch | Moderate party; slate vestibule, oak/plaster hall, hearth lounge, ground-floor chapter room, dining/kitchen, WC | Picnic patio, grill, partial string lights, lean-to, brambles toward woods |
| R5 | New compact Queen Anne; ΡΞΗΨ; turret inside body envelope, front porch with short west return | Party stopped mid-moment; connected parlors, retained carved woodwork, food left out, stereo gear, off-hook phone, open entrance | Fire ring with cold ash, food table, small shed, one abandoned chair, wild back edge |

All houses have original house numbers, mailboxes/signs, plausible storm doors, selected window AC units, curtains, lamp silhouettes and worn surfaces. Three distinct architectural families are Colonial/Greek Revival, Tudor, and Queen Anne. Preserve the three original houses; differentiate their details rather than replacing them with unrelated buildings.

## Placement and circulation

The machine-readable coordinates are in `PLAN.json` (centimetres); the dimensions here were reconciled against the current saved `Source/SeniorSendoff/HouseFraternityRow.cpp` and `HouseFraternityRowYards.cpp`. R4 occupies X -8400 to -6800, Y 450 to 1850, at floor Z-50; its porch occupies Y 150 to 450. R5 occupies X -7080 to -5680, Y -1700 to -200, at floor Z30. Its front porch occupies Y -1850 to -1700, with a short west return at X -7230..-7080 ending at Y -1150. The turret remains inside the body. The source overlap audit places its complete roof/porch/steps envelope clear of the measured original canopy bounds, with the nearest rear canopy about 655 cm beyond the new rear landing envelope; this is source-bound evidence, not native collision acceptance. The roof keeps 120 cm to the original parking walk. Six nominal 15 cm front risers and a wide offset rear landing preserve access without regrading. The stereo is on the east porch at (R-230,F-75)=(-5910,-1775), floor Z30, leaving the west return free.

The 180 cm informal access path keeps every planned point, starting at (-8100,-3900), running through (-6550,0) and (-6550,2200), and reaching the existing woods gate. The old lot-to-house walk also stays open at its exact test coordinates. New lawn branches approach the Tudor from the south and reach the Queen Anne at (-6380,-2020). The last new yard divider is X -6700, with openings Y1050..2150 and Y3000..3920 to preserve the fixed diagonal field walk. The main-house perimeter connection and east driveway remain. All dirt paths follow current grade without collision or regrading. Each rear door connects to its own shed and fence approach.

Usable plot divisions west to east are X -18740/-15750/-12000/-8500/-6700/-3900. The existing solid hedge at X -18800 +/-60 remains R1's western boundary. Original roots west of it remain outside the usable yard, rather than being enclosed or removed. New dividers mix low picket, chain link, split rail and broken hedge/rail. **These are not changes to the existing woods fence.** Gates at Y 700–1150 and 3100–3550 connect the western parcels; the R3/R4 gaps are Y 100–650 and 3100–3600. The R4/R5 divider at X -6700 leaves openings at Y 1050–2150 and 3000–3920. Its hedge growth stays west of the rails. The wider rear opening clears the unchanged fixed diagonal field route; all four dividers retain eight openings in total. Gateposts and furniture must preserve these full clear openings.

All new house bodies end before Y 1900. R4 keeps its rear path/scatter reservation at X -7800..-7400, Y 1850..2350; its built rear landing ends at Y2258. R5's rear door stays centred at X-6380, while `RearStairX` offsets its 400 cm wide landing to X-6300. Its stair/landing footprint is X -6500..-6100, Y -200..208, with a 3 cm seam extending inside the house to Y-203. The dressing reservation continues to Y300 before the rear path starts at (-6300,300). The stairs, porches and outbuildings are included in the source overlap checks. Outbuildings stand beyond the Y 3700 probes, before the existing fence, with walking access around them. The rear third of each yard gradually adds tall grass, leaf drifts, brambles and saplings around a still-clear path. Trees remain in place. Grass and litter are noncolliding; buildings, fences and substantial furniture are solid. The access path crosses probe (-7000,3700), so it is **noncolliding visual dirt/wear following existing ground**, never raised colliding paving.

## Interiors shown before building

![Proposed ground floors](INTERIOR_PLAN.png)

`INTERIOR_REFERENCES.md` documents primary sources, directly inspected photographs, period limits and adaptations. `INTERIOR_PLAN.json` gives local, measured partition/door reservations per house. Ground-floor chapter rooms satisfy the brief without excavation. Floors use plinths and normal steps above current ground. Upper floors end at visible, solid closed doors reached by an ordinary return stair, with no invisible movement blockers.

All houses keep a 220 cm front-to-rear hall, 200 cm front entrance, 140 cm side-room/rear openings and a rear exit to the yard. The return stair sits beside the hall; the adjacent room door sits beyond its footprint. Its two flights have 110 cm clear widths, a 110 cm return landing and 28 cm treads: 24 risers over 390 cm for old houses, 20 over 330 cm for new ones. Reserve 210 cm minimum headroom. The Queen Anne's turret is explicitly **southwest**, serving the front-parlor reading alcove; the northeast remains the kitchen. A wide opening connects its front parlor and dining. The Tudor has a slate vestibule. Furniture, swings and actual stair headroom still require native collision checks.

R1/R2/R3 keep floor Z30; R4 keeps floor Z-50; R5 now uses floor Z30. R4's front entry remains Y122..150: one 28 cm tread and the porch edge give two terrain-derived risers (about16.51 cm at the source grade). Its rear stair keeps seven terrain-derived risers, six 28 cm treads and 120 cm top/bottom landings inside the 500 cm path reservation. R5's porch front is F-150=-1850: five 28 cm treads start at Y-1990, then the porch edge completes six nominal15 cm risers. Its front stair footprint is X -6580..-6180, Y -1990..-1850; the approach ends just before it at (-6380,-2020). R5's rear uses seven terrain-derived risers, six 28 cm treads and 120 cm top/bottom landings, ending at Y208; the bottom datum samples existing grade at (-6300,88), with foundations extending below sampled grade throughout the footprint. The original three rear exits reserve 380 cm for six terrain-derived risers, five 28 cm treads and 120 cm landings. These are source dimensions; native ground contacts, the R5 bottom-landing transition and all risers must be measured before acceptance. The southwest turret's 240 cm alcove ends 20 cm before the interior stair reservation, so the usable envelopes do not overlap.

Reuse the campus interior registry, FHallMesh room kit, `ACampusDoor`, proximity loading and **six shared interior lights**. Add five standalone definitions at the end of the registry to preserve existing seeds. New row geometry uses separate solid/decoration/glow batches with explicit review coverage. Keep the old `ExteriorFraternityRow` decorative bounds/collision invariant intact; add checks for the new colliding shells and northern houses, rather than deleting the old checks or hiding all row geometry behind a rename.

Preserve the original three planting NoGo boxes and the original tree RNG. New dressing uses its own deterministic RNG and ISM batches, not HISM or a new shared tree mesh.

## Light and performance budget

Proposed new exterior real lights: **6**, two each at R3/R4/R5; all shadowless, short radius. Remove the two old parking poles/lights and their bases with the lot. Other windows, lamps, lanterns and most string bulbs are emissive. Quiet-house apparent light can be emissive. The brief's exterior total is 53, so the revised proposed total is **53 - 2 + 6 = 57 (the native baseline confirms53)**. The fresh native baseline verifies53; Stewart explicitly approved57for the combined Row/stadium scene. Update exact light-count expectations and explicitly superseded parking-feature assertions; retain unaffected thresholds and all walking waypoints.

Capture dusk and dark with the same camera poses and recorded lighting settings, using existing lighting controls rather than altering global gameplay. Record frame, game, render and RHI times from the standard views. Three alternating rounds per configuration must be complete; compare the median **render** time per standard view against the <=5% budget. The helper's printed delta is frame time, so it alone is insufficient to establish the render-thread budget.

No Fab assets are required by this proposal. Existing room kits and original runtime geometry cover the plan. Any later asset need must name an exact link for Stewart; do not download/import a Fab product on his behalf.

## Native acceptance requirements

Retain the unchanged-scene baseline verdicts. After integration run campus interior walk and smoke over all five new IDs and existing buildings; FreeRoam, CampusLayoutReview, CalderTerrainReview, Surroundings/Fraternity/ParkingLot/WoodsPass, WestField and Woods reviews, house smokes and multiplayer. Preserve preexisting verdicts without weakening checks. Walk from the road into each house, every room, rear exit, yard, outbuilding and fence approach; open/close doors from both sides; return without trapping. Verify original tree CRC `0f3b2bb7`, no more than 40 hidden TreeA/TreeB, all collision classes, light pool/count, ground contacts and unclipped foliage at dusk and dark. Save before/after evidence, numeric perf tables and an owned-files patch. This documentation reconciliation establishes source consistency only; current native verdicts belong in the parent validation reports. Do not save a map or commit.


## Combined lighting decision

Stewart explicitly confirmed on18October2026 that the combined Row+stadium map should use57exterior lights, retaining all6Row additions after2west-parking removals. Stadium adds0and keeps its3existingpoints. The untouched native baseline verifies53. Optional NoFraternityRowLights gives51with allnewgeometry/emissivespresent; NoFraternityRowUpgrade restores53and the oldlot/threeclosedhouses.
