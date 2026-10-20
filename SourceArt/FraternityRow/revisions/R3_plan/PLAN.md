# Fraternity Row infill plan

**Latest user constraints: do not enlarge the map; use the existing grass on the Row side of the main house; the main-house parking lot may be removed.**

Five Row houses, plus the separate protected main house. Keep the original three footprints. Place two smaller houses in the existing field, and reclaim the former main-house parking as their front lawns and walking approaches. No road extension, new terrain, woods-fence movement, or map save. `PARKING_REMOVAL.md` records the new authorization and exact removal inventory; the old parking protection is superseded.

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

The machine-readable coordinates are in `PLAN.json` (centimetres). R4 occupies X -8400 to -6800, Y 450 to 1850; its porch occupies Y 150 to 450. R5 occupies X -6000 to -4600, Y 300 to 1800; its front porch occupies Y 150 to 300, with a short west return ending at Y 850. Its turret remains inside the body bounds. The west return stops well south of the maple.

The 180 cm informal access path runs from the unchanged sidewalk at (-8100,-3900), north to Y 0, across to X -6550, then north between the new houses and onward to the existing gate. Its bend at (-6350,3100) stays west of the R4/R5 divider. Additional paths now use the released parking footprint: the old lot-to-house walk remains open at its exact test coordinates, with lawn branches to both new porches. Lot pavement, parked cars, markings, raised curbs, signs and lot-specific mulch/props are removed during implementation. The main-house perimeter connection and east driveway remain. All new dirt paths follow current grade without collision or regrading. Side routes connect each house's rear door to its own yard and fence approach.

Usable plot divisions west to east are X -18740/-15750/-12000/-8500/-6200/-3900. The existing solid hedge at X -18800 +/-60 remains R1's western boundary. Original roots west of it remain outside the usable yard, rather than being enclosed or removed. New dividers mix low picket, chain link, split rail and broken hedge/rail. **These are not changes to the existing woods fence.** Gates at Y 700–1150 and 3100–3550 connect the western parcels; the R3/R4 front gap is Y 100–650. The R4/R5 divider leaves the maple open at Y 1050–2150 and the fixed review route open at Y 3000–3660. Gateposts and furniture must preserve these full clear openings.

All new house bodies end before Y 1900. Their rear stair reservations extend farther north in gaps between the fixed probes: R4 X -7800..-7400, Y 1850..2350; R5 X -5500..-5100, Y 1800..2300. The stairs, porches and outbuildings are included in the source overlap checks. Proposed outbuildings stand beyond the Y 3700 probes, before the existing fence, with walking access around them. The rear third of each yard gradually adds tall grass, leaf drifts, brambles and saplings around a still-clear path. Trees remain in place. Grass and litter are noncolliding; buildings, fences and substantial furniture are solid. The access path crosses probe (-7000,3700), so it is **noncolliding visual dirt/wear following existing ground**, never raised colliding paving.

## Interiors shown before building

![Proposed ground floors](INTERIOR_PLAN.png)

`INTERIOR_REFERENCES.md` documents primary sources, directly inspected photographs, period limits and adaptations. `INTERIOR_PLAN.json` gives local, measured partition/door reservations per house. Ground-floor chapter rooms satisfy the brief without excavation. Floors use plinths and normal steps above current ground. Upper floors end at visible, solid closed doors reached by an ordinary return stair, with no invisible movement blockers.

All houses keep a 220 cm front-to-rear hall, 200 cm front entrance, 140 cm side-room/rear openings and a rear exit to the yard. The return stair sits beside the hall; the adjacent room door sits beyond its footprint. Its two flights have 110 cm clear widths, a 110 cm return landing and 28 cm treads: 24 risers over 390 cm for old houses, 20 over 330 cm for new ones. Reserve 210 cm minimum headroom. The Queen Anne's turret is explicitly **southwest**, serving the front-parlor reading alcove; the northeast remains the kitchen. A wide opening connects its front parlor and dining. The Tudor has a slate vestibule. Furniture, swings and actual stair headroom still require native collision checks.

Existing house floor datum stays Z 30. New R4/R5 floor datum is Z -50, above their existing body/porch ground without excavation. At the bottom face of the front steps, Y 122, existing grade is approximately -83.03: two 16.51 cm risers and one 28 cm tread fit the Y 122..150 front-step reservation, north of the shared path's Y 90 edge. At the rear, grade is approximately -161.2: seven 15.89 cm risers, six 28 cm treads and 120 cm top/bottom landings fit a 500 cm reservation. The original three rear exits reserve 380 cm for six 15 cm risers, five 28 cm treads and 120 cm landings. These are source-derived nominal dimensions; native ground contacts and risers must be measured before acceptance. The southwest turret's 240 cm alcove ends 20 cm before the stair reservation, so the usable envelopes do not overlap.

Reuse the campus interior registry, FHallMesh room kit, `ACampusDoor`, proximity loading and **six shared interior lights**. Add five standalone definitions at the end of the registry to preserve existing seeds. New row geometry uses separate solid/decoration/glow batches with explicit review coverage. Keep the old `ExteriorFraternityRow` decorative bounds/collision invariant intact; add checks for the new colliding shells and northern houses, rather than deleting the old checks or hiding all row geometry behind a rename.

Preserve the original three planting NoGo boxes and the original tree RNG. New dressing uses its own deterministic RNG and ISM batches, not HISM or a new shared tree mesh.

## Light and performance budget

Proposed new exterior real lights: **6**, two each at R3/R4/R5; all shadowless, short radius. Remove the two old parking poles/lights and their bases with the lot. Other windows, lamps, lanterns and most string bulbs are emissive. Quiet-house apparent light can be emissive. The brief's exterior total is 53, so the revised proposed total is **53 - 2 + 6 = 57 (the native baseline confirms53)**. The fresh native baseline verifies53; Stewart explicitly approved57for the combined Row/stadium scene. Update exact light-count expectations and explicitly superseded parking-feature assertions; retain unaffected thresholds and all walking waypoints.

Capture dusk and dark with the same camera poses and recorded lighting settings, using existing lighting controls rather than altering global gameplay. Record frame, game, render and RHI times from the standard views. Three alternating rounds per configuration must be complete; compare the median **render** time per standard view against the <=5% budget. The helper's printed delta is frame time, so it alone is insufficient to establish the render-thread budget.

No Fab assets are required by this proposal. Existing room kits and original runtime geometry cover the plan. Any later asset need must name an exact link for Stewart; do not download/import a Fab product on his behalf.

## Native acceptance, all still pending

Capture baseline verdicts before edits. After integration run campus interior walk and smoke over all five new IDs and existing buildings; FreeRoam, CampusLayoutReview, CalderTerrainReview, Surroundings/Fraternity/ParkingLot/WoodsPass, WestField and Woods reviews, house smokes and multiplayer. Preserve preexisting verdicts without weakening checks. Walk from the road into each house, every room, rear exit, yard, outbuilding and fence approach; open/close doors from both sides; return without trapping. Verify original tree CRC `0f3b2bb7`, no more than 40 hidden TreeA/TreeB, all collision classes, light pool/count, ground contacts and unclipped foliage at dusk and dark. Save before/after evidence, numeric perf tables and an owned-files patch. Do not save a map or commit.


## Combined lighting decision

Stewart explicitly confirmed on18October2026 that the combined Row+stadium map should use57exterior lights, retaining all6Row additions after2west-parking removals. Stadium adds0and keeps its3existingpoints. The untouched native baseline verifies53. Optional NoFraternityRowLights gives51with allnewgeometry/emissivespresent; NoFraternityRowUpgrade restores53and the oldlot/threeclosedhouses.
