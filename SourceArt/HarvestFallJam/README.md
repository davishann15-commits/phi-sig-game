# Fall Harvest Jam (feat. Project Jaystorm) — Oct 12–13, 2026

Jacob asked for the aftermath of the party on the east lawn: temporary fences, a stage with instruments, the theme switched from Halloween to a harvest jam. Then, after seeing it from the sky view: make the party the size of the whole lawn with the same fence, cop cars pulled in around it, and both banners reading FALL HARVEST JAM · FEAT: PROJECT JAYSTORM. Built at runtime (the map is untouched), everything original and painted in code, no brands or badges. No lights added anywhere: bulbs, embers and the police light bars are emissive only, so the count stays at 53.

## Where
The field is now the whole east lawn: x 4050..8400, y -3660..-100 (flat ground at -60). The snow fence runs at x 4150..8300, y -3560..-200: a metre off the side drive, a metre and a half inside the side-street sidewalk, well clear of the front sidewalk's lamp poles, bench and stop sign, and just short of where the ground starts sloping to the rear court. The gate in the west fence (y -1210..-910) faces the porte-cochere's side door; a 4 m hayride gate in the south fence (x 6700..7100) faces the street.

## What's there (`Source/SeniorSendoff/HouseFallJam.h/.cpp`)
- Stage (unchanged position, north end): plywood deck on scaffold, black skirt, steps, PA stacks, wedges, drum riser; the backdrop sign now reads FALL HARVEST JAM / FEAT: PROJECT JAYSTORM / 1986 LIVE BANDS HOT CIDER HAYRIDES. The generator that ran the PA sits behind it with its cable up to the deck.
- Backline: drum kit, combo amp, bass stack, guitar on a stand, bass, banjo, keyboard, mic stands, open case, cooler, cables, the snake to the mixer table.
- Fences: lath snow fence on green T-posts round the whole lawn (59 sections), four sections down (two flat, two leaning), the west gate panel leaning in the gap, the hayride pair leaning and flat. Crowd barricades at the stage front, a chute of them at the west gate.
- Gate: ticket table with chair and cash box, trash drum, corn either side, and a half-size copy of the painted sign on stakes outside, facing the drive.
- Hayride: a red farm tractor hitched to a flatbed hay wagon (plank deck, slatted head/tail boards, side rails, bales and loose straw on the deck, one bale tipped), parked along the east fence.
- Facilities: three chemical toilets along the west fence (one door open), a galvanised keg tank with two kegs and the cup table, a 3 x 3 m blue pop-up canopy with one leg buckled and the merch tables under it.
- Field: two bale arcs facing the stage plus a back arc, pairs and stacks across the lawn, the ring round the dead fire pit, the pumpkin-toss pile on straw, pumpkins and gourds on bales and in the grass, corn shocks at every corner and both gates, scarecrow, cider barrel with chalkboard, apple crates, fiddle, cornhole boards facing each other, pizza table.
- String lights: the four inner poles round the stage field (the north pair strung to the backdrop uprights so no cord sags across the sign) and an outer ring of 14 poles a metre inside the fence with cords between neighbours and four spokes in; one inner diagonal down on the grass; streamers on both rings.
- Litter: ~560 cups, 230 cans, 60 plates, 56 napkins, 10 pizza boxes, 18 folding chairs (most knocked about), 5 tables (two collapsed), 4 trash drums, bags along the west fence, balloons, a sweater on the fence, a sneaker, a frisbee. Random litter keeps out of the big props.
- Meshes: `ExteriorFallJam` (solid, collision + shadows, ~38k tris), `ExteriorFallJamLitter` (~76k, no collision/shadows), `ExteriorFallJamGlow` (~21k, emissive).

## Police (`HouseClassicCars` + `HouseExterior.cpp`)
- New car type `EExCar::Cruiser`: the land-yacht sedan body in black with a painted white roof and white doors (a `Doors` x-zone on the body sides), roof light bar (chrome feet, black centre, red and blue domes, emissive), spotlight on the driver's pillar, push bar on the bumper, POLICE in stroke lettering standing just off both front doors (reads left to right from either side), steel wheels with dog-dish caps, no whitewalls. One paint scheme (`Black` / `White`), so one more HISM group.
- Seven cruisers pulled in: three nosed over the front kerb (front tyres on the sidewalk, 3 degrees of pitch), three off the side street, one that drove in through the hayride gate onto the grass. None on the drive, because the walk review walks it.
- Material keys added: `CAR_LightRed`, `CAR_LightBlue`.

## Banners (`HouseStreetScene.cpp`)
- House banner: FALL / HARVEST / JAM (green-over-orange, rust-over-gold, green-over-orange), FEAT: PROJECT JAYSTORM in purple, the date small under it.
- Pole flyers: FALL / HARVEST / JAM / FEAT PROJECT / JAYSTORM / SAT OCT 18 / 1800 FRAT PARK DR.
- A `:` glyph was added to the shared stroke font (`HouseBlackHallPaint.cpp`).

## Trees
The random tree grid keeps out of the field. A tree that lands there still draws its three random numbers, so every other tree stays where it was. The street trees between the sidewalk and the fence remain.

## Views
`detail_jam_overhead` (straight down over the field), `detail_jam_high` (from the street corner), `detail_jam_wide`, `_stage`, `_drums`, `_gate`, `_fire`, `_cider`, `_street`, `_police`, `_police_side`, `_bust`, `_hayride`, `_kegs`.

## Results
- Build passes (Mac, UE 5.8). Codex's checks with the walk all pass: SURROUNDINGS_POLISH (7 car models, 49 cars), FRATERNITY_BOUNDARY, CAMPUS_WOODS, PARKING_LOT (lights 53, walk PASS). The front-walk review fails only where it did before this work.
- Frame time at 1280x720: jam views 11-17 ms; the high establishing shot 20 ms.
- Shots: `Saved/StreetViews/*_detail_jam_*.png`.

## Build agent (no more screen takeovers)
- `Saved/BuildAgent.command`: double-click once and leave the Terminal window open (minimised is fine). It idles and runs `Saved/RunStreet.command` whenever a request file appears, then reopens the editor in the background (`open -g`) without stealing focus.
- Request: a file `Saved/agent/request` with `STREET_*=value` lines (e.g. `STREET_VIEWS=detail_jam`, `STREET_REVIEWS=1`, `STREET_NOVIEWS=1`, `STREET_SKYSMOKE=1`); the agent writes `Saved/agent/done` when finished and logs to `Saved/agent/agent.log`. Only `STREET_` variables are accepted; anything with shell characters is rejected.
- Stop it by closing the window or creating `Saved/agent/stop`. It cannot stop a Play-In-Editor session; stop Play before a build or the crash reporter pops up (harmless).
