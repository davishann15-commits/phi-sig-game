# Homecoming on the front campus (2026-10-13)

Unreal project: `/Users/Stewart/Documents/Unreal Projects/SeniorSendoff`. Map: `/Game/Story/Maps/Chapter01_House`.

It is homecoming weekend, Saturday 18 October 1986, at dusk, just after the final whistle. The campus across the street
belongs to an invented school, **Calder State**. Its team is the **Fighting Geese**, in purple and gold, and today they
beat **Dunmore** 24–17. The school, the team, the rival, the mascot and every sign, band, flyer and slogan are made up
for this game. None of them is a real school, team, brand or mark, and the old "VOLS" lettering and orange T are gone.

Everything is built at runtime beside the exterior: the `AHouseCampusFall` actor, plus a few switches in the campus
builder. Nothing is saved into the map and no asset files were created. **No lights were added**; the exterior still has
53. Every glow comes from material emission or from camera-facing haze quads.

**A/B switch.** `-NoCampusFall` turns all of it off and gives back the campus and stadium exactly as they were: the
baseball diamond, the VOLS scoreboard, the orange T, the orange seats and banners, the 1-in-5 lit windows and the
original tree batches.

## The stadium: a college football ground

The old bowl keeps its footprint, walls, seat rows, arches, press box and masts. What changed:

**Field.** The baseball diamond (turf box, dirt infield, bases, mound and foul lines) is replaced by a painted football
field: `CF_Field`, 2048 × 1024, painted in code. The bowl's clear oval is only 77 × 52 m, so the field is at 0.55 scale
(a "yard" is 50.3 cm; the field with its end zones is 60.4 × 26.8 m). That is the largest standard-proportioned field
the bowl holds without moving a seat. A regulation field would need the bowl 1.8 times bigger, which would run into
four campus buildings. The painted field has:
- mowing stripes every 5 yards, and late-season wear and dirt between the hashes and in front of the goals;
- purple end zones with gold bands, lettered CALDER (west) and GEESE (east);
- white sidelines and end lines, goal lines, 5-yard lines, hash marks and sideline ticks;
- yard numbers with direction arrows, reading from each sideline;
- the Fighting Goose logo at midfield;
- scuffs.

Around it:
- a cinder track with a concrete kerb;
- gold slingshot goalposts on both end lines, with purple pads and purple and gold streamers that flutter;
- orange pylons;
- aluminium team benches: the Geese on the press-box side, Dunmore across the field;
- water coolers on folding tables, towels, and the cups and programmes left on the field.

**Stands.** The 12 seat rows stay. The benches are now purple with a gold row every fourth, and the treads are weathered
concrete. The steps are closed with risers facing the field, and there is a podium wall under the front row and a back
wall up to the rim, so the field no longer sees through the stands. About 1,100 pieces of after-game litter lie on the
treads and benches: cups, programmes, popcorn boxes, napkins, hand pennants and seat cushions. Ten bedsheet banners are
tied along the podium wall. Purple, gold and white pennants hang on cords from post to post all the way round the rim.

**Scoreboard.** The south board becomes a 1980s incandescent bulb board:
- HOMECOMING '86 across the top;
- GEESE 24 and DUNMORE 17;
- the clock run out at 00:00, and QTR 4;
- the CALDER STATE STADIUM name board along the bottom.

About 1,550 bulbs sit in 5 × 7 cells, with dark bulbs filling every cell. Marquee bulbs chase round the edge, two lit and
one dark, marching.

**Press box.** On the field side:
- lit windows with mullions;
- a CALDER STATE STADIUM sign;
- purple, white and gold bunting fans.

On the street side, the name board is across the glass where the orange T was.

**Floods.** The three masts read as ON through their emissive heads and a soft blue-white haze (two camera-facing quads
per mast). Their three existing point lights moved 35% of the way from the mast toward the field centre, as if aimed
down at it. Their reach went from 5000 to 6500, so the field is lit to the centre line while the spill onto the street
ends about where it did before.

**Gate.** Arches K=8 and K=10 on the side facing the house become GATE 4 and GATE 5:
- their bars are gone, and purple iron gates stand open with gold spear tips;
- GATE 4 / GATE 5 boards hang in the arches;
- two ticket booths stand in front, with lit windows and TICKETS / SOLD OUT signs;
- hay bales, corn shocks, pumpkins and jack-o'-lanterns;
- pennant strings with warm string lights, from booth to mast to booth and along the wall;
- sheets on the wall: BEAT DUNMORE!, HONK IF YOU ♥ THE GEESE and DUNMORE GO HOME.

**On the green, in front of the gates:**
- **Homecoming float:** a chicken-wire and tissue Fighting Goose on a flatbed, with a brown body, black neck and head,
  white chinstrap and raised purple and gold wings. The skirt reads HOMECOMING '86 / THE GEESE TAKE FLIGHT, a BEAT
  DUNMORE! sheet hangs on the back, and string lights run round the deck.
- **Band equipment trailer:** CALDER STATE / MARCHING GEESE on its sides, the ramp down, a bass drum and two cases.
- **Gate path:** a concrete path from the walk to the gates, with chalk from Friday's pep rally: BEAT DUNMORE!, GO GEESE,
  HONK!, HOMECOMING 86, CLASS OF 87, an arrow to the gates, and a goose on the plaza.

## The campus

- **Dorms.**
  - Blocks 1 and 4 (the HALL buildings) are dorms: about two windows in five are lit, against one in five on the other
    buildings.
  - In the lit windows: posters, half-drawn warm curtains, felt pennants, and string lights inside the top of the
    frame.
  - Some rooms have string lights round the outside of the frame; on some, the lights chase.
  - Jack-o'-lanterns flicker on the sills, and up to seven sheets per dorm hang out of the windows.
- **Lamps.** Every walkway lamp has a mercury-vapour lens with a blue-green haze, and the orange banners are replaced
  by two-sided Calder State banners: the goose on one side, HOME COMING '86 / OCT 18 on the other.
- **Plaza and walk furniture.**
  - A green flyer kiosk on a round plaza off the main walk, wrapped in stapled flyers for invented bands and events:
    THE PAPER LANTERNS at the Annex, MIDNIGHT CHORUS, a haunted hayride, the fall formal, a lost cat, a ski club, and
    more.
  - A 1980s outdoor TELEPHONE booth with its light on and the door folded open.
  - Three CAMPUS COURIER newspaper boxes (the street scene's invented paper).
  - A comb rack of ten-speeds in red, blue, yellow and silver.
- **Fall colour.**
  - Five of the campus trees the street sees go full colour: the maple at (-7277, -5900) and the oak on the green
    are red, and three birches are orange.
  - The trees are not moved. They are drawn by their own batches (`CampusFallTree…`) instead of the species batches,
    with the same mesh and transforms. Red is the copper leaf cell through the foliage master's `Tint`.
  - The tree fingerprint is unchanged: `WOODS_PASS_ORIGINAL_TREES big=134 small=136 distant=852 crc=0f3b2bb7`.
- **Leaves on the ground.** Leaves under the trees, raked piles, a rake leaning on the big pile, and three full leaf
  bags.
- **Bonfire.** The pep-rally bonfire, burned down inside a ring of fieldstones: charred logs fallen inward with glowing
  ends, embers in the ash, hay bales round it for sitting, a warm flickering glow, and a column of smoke drifting with
  the evening breeze.

Props stay clear of trees. Before building, the actor reads where the exterior's trees stand, then places the float,
the trailer, the bonfire and the leaf piles in tree-free spots.

## Life

| What | How |
|---|---|
| Flutter | Pennants, goalpost streamers and bedsheets are cloth grids in one movable mesh. They are rewritten 30 times a second while the camera is within 170 m: waves plus gusts, hanging cloth swinging from its top edge, tied sheets breathing. |
| Flicker | Three candle materials, so the jack-o'-lanterns don't flicker in step. Embers breathe slowly. Their haze flickers with them. |
| Marquee | Three chase materials on the scoreboard edge and some dorm windows: two lit, one dark, stepping five times a second |
| Smoke | 44 camera-facing puffs from the bonfire, glowing warm near the coals, using the street's smoke material |
| Leaves | 96 leaves come down over the green and the gate, tumbling and drifting with the breeze. They lie on the grass a few seconds, then fall again. |
| Drumline | Synthesised in code: snare, five split basses, four tenors and crashes. It plays a four-bar cadence at 120 bpm three times (27 s), with slap-back from the stands. It plays from the field every couple of minutes, spatialised and fading with distance, so from the house it is faint. |

All motion pauses while the camera is far from it. A stats line, `CAMPUS_FALL_MOTION`, is logged every 20 s.

## Switches and commands

| Switch | Effect |
|---|---|
| `-NoCampusFall` | Turns all of the above off (A/B) |
| `-NoCampusDrums` | Silences the drumline |
| `-CampusDrumsEvery=<s>` | Sets the drumline interval (default 110 s; the first play is after half that, at most 30 s) |
| `-CampusDrumsRecord` | Records the first play from the mixer to `Saved/CampusFall/CampusDrums.wav` (dev builds) |
| `-CampusDrumsDump` | Writes the dry cadence to `Saved/CampusFall/CampusDrumsDry.wav` (dev builds) |
| Console `CampusFall.Drums` | Plays the cadence now |

The seven new cameras, for `-HouseExteriorViews -ExteriorViewFilter=…`, are `detail_stadium_field`,
`detail_stadium_stands`, `detail_stadium_scoreboard`, `detail_stadium_gate`, `detail_campus_quad`,
`detail_campus_dorms` and `detail_campus_kiosk`.

## Files

New, all under `Source/SeniorSendoff/`. Each has its own named `…Impl` namespace, no using-directives, and builds clean
under -Wshadow.

| File | What it does |
|---|---|
| `HouseCampusFall.h` / `.cpp` | The public switches (`Enabled`, `WindowLit`, `SeatKey`, `MercuryLensKey`, `StadiumLightRadius`/`StadiumLightAt`, `AutumnTone`/`AutumnLeafKey`) and the `AHouseCampusFall` actor. It reads where the trees stand, builds the parts, and runs flutter, glow, halos, smoke, leaves and the drumline (synthesis, spatial playback, record/dump), plus the `CampusFall.Drums` command. |
| `HouseCampusFallLook.cpp` | Every `CF_` material, and the textures painted in code: field, six sheets, two lamp banners, press-box, trailer, float, ticket and flyer art. Also the red leaf MIDs, and the seat treads recoloured to concrete. |
| `HouseCampusFallKit.h` | Small geometry helpers: triangles, bars, fans, cards, lathe, pumpkin, jack-o'-lantern face, hay bale, ten-speed, chalk glyph strips, 5×7 bulb font, cords. |
| `HouseCampusFallParts.h` | What the builders hand the actor: static meshes by area, the cloth list, halos, bonfire, leaf area, tree positions with tree-clear checks. |
| `HouseCampusFallStadium.cpp` | Field, track, goalposts, benches and coolers; bulb scoreboard; stand litter, podium sheets and rim pennants; press-box windows, sign and bunting; flood haze; gates, booths, hay, corn shocks and lanterns; float; band trailer. |
| `HouseCampusFallQuad.cpp` | Dorm windows and sheets; lamp banners and mercury haze; kiosk plaza, gate path and chalk; phone booth; news boxes; bike rack; leaf piles, rake and bags; bonfire. |

Changed (every change is gated on `HouseCampusFall::Enabled()`):

| File | Change |
|---|---|
| `HouseExteriorCampus.cpp` | Lit windows go through `WindowLit`. Lamp lenses are mercury and the orange lamp banners are dropped. Stands get field-facing risers, a podium wall and a back wall, and the seat benches go purple and gold. Bars are taken out of arches 8 and 10. The turf and baseball diamond, the orange T and the VOLS/HOME GUEST/03 01 lettering are skipped. The layout log line gains `homecoming=`. |
| `HouseExterior.cpp` | Includes `HouseCampusFall.h` and calls `HouseCampusFall::CreateLook` after the fall-jam look. The stadium lights use `StadiumLightAt` and `StadiumLightRadius`. Chosen trees move into `CampusFallTree…` batches with the same mesh, transforms and hit reaction. `AHouseCampusFall` spawns after the clock tower. The seven `detail_stadium_*` and `detail_campus_*` cameras are added. |


## Results

Evidence is in `Deliverables/CampusFall_20261013/`: `pass0_before`, `pass1_stadium`, `pass2_campus`,
`pass3_motion_sound`, `pass4_perf_reviews` and `pass4_final_dusk_views`, with `on` and `off_NoCampusFall` shots of all
12 views. `campusfall_changes.patch` is against the snapshot taken before the work.

**Build log line.**
```
CAMPUS_FALL built school="CALDER STATE" team=GEESE rival=DUNMORE tris=102422 parts=12 cloths=512 halos=87 lanterns=31
puffs=44 leaves=96 trees=274 bonfire=(3750,-7700) glows=9 lights=0 ms=10
```

**Lights.** The exterior still has 53 point and spot lights: `actualExteriorPointAndSpotLights=53` from the WoodsPass
review, `exteriorLights=53` from ParkingLot and ClockTower. No light was added.

**Trees.** The fingerprint is unchanged: `WOODS_PASS_ORIGINAL_TREES stage=3 big=134 small=136 distant=852 crc=0f3b2bb7`.
The recoloured trees are logged as `CAMPUS_FALL_TREES …`: Maple 2 red ×1, Oak 1 red ×1, Birch 0 orange ×2, Birch 1
orange ×1.

**Reviews, with homecoming on (all pass):**
- CampusWoods: `PASS campusFrontVertices=1261140 stadiumSeatVertices=4134 … geometryRegionsValid=1`
- SurroundingsPolish: `PASS streetVertices=23748 … newDecorationCollisionAndShadowsOff=1`
- ParkingLotWalk: `PASS Player walked from the driveway … without a collision stall` (exteriorLights=53)
- WoodsPass: `PASS stage=3 actualExteriorPointAndSpotLights=53 …`
- ClockTower: `PASS … towerLights=0 exteriorLights=53 … chimesStarted=2 chimesFinished=2 … audioConsumed=1 sound=1`. This ran
  with the drumline playing too.

**Motion.** The `CAMPUS_FALL_MOTION` log, taken with the player in the house, shows:
- cloth rewritten about 26 times a second, with peak swing 12–16 cm;
- leaves, smoke and glow updating every frame.

**Drumline.** The dry render is 27.0 s at -1.4 dBFS peak / -17.6 dBFS RMS, with 45 onsets and no clipping. The mixer
recording in the game, from the house about 130 m away, is -37 dBFS peak / -53 dBFS RMS with 42 onsets: faint and
distant, as intended. The level was then raised about 3 dB. Both WAVs are in `pass3_motion_sound`.

**Frame time.** Five alternating runs each, homecoming on (A) and `-NoCampusFall` (B), `-ExecCmds="t.MaxFPS 0"`, 200
samples per view, median ms:

| View | On | Off | Δ |
|---|---|---|---|
| front_street | 23.32 | 23.23 | +0.09 |
| street_along | 14.71 | 14.46 | +0.25 |
| detail_establishing_corner | 18.23 | 18.00 | +0.23 |
| detail_street_dusk | 14.19 | 14.04 | +0.15 |
| detail_clocktower_far | 8.34 | 8.33 | +0.01 |
| detail_stadium_field | 8.35 | 8.33 | +0.02 |
| detail_stadium_stands | 8.60 | 8.82 | -0.22 |
| detail_stadium_scoreboard | 8.33 | 8.34 | -0.01 |
| detail_stadium_gate | 8.57 | 8.57 | +0.00 |
| detail_campus_quad | 9.15 | 8.34 | +0.81 |
| detail_campus_dorms | 8.38 | 8.33 | +0.05 |
| detail_campus_kiosk | 8.84 | 8.43 | +0.41 |

Every view is within about 1 ms. The quad, which has the smoke, halos and falling leaves, costs the most. Many campus
views sit at 8.33 ms, the 120 Hz pacing floor of these off-screen runs, so small costs there are hidden. Run B5 was slow
across every view (machine noise), which is why the table uses medians. All ten CSVs are kept.


## Sources

Everything here is made in code: geometry, painted textures (field, sheets, banners, signs, flyers, halo), materials
and the drumline. No outside assets, CC0 or otherwise, were used. Assets already in the project are reused: the street
scene's smoke material and puff texture, its leaf colours, the CAMPUS COURIER newspaper textures and the TELEPHONE sign
colours; the fall-jam straw and twine; and the existing foliage master and leaf atlas for the red trees.
