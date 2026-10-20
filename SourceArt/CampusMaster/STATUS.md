# Calder State master job: status

The plan is in [PLAN.md](PLAN.md). Steps run 1a → 1d, 2a → 2e, 3a → 3d, then 4. Each step below leaves the game building
and its reviews passing.

## Where things stand (2026-10-14)

| Step | State |
|---|---|
| 1a roads and lots | **done** |
| 1b campus blocks re-placed and re-shaped | **done** |
| 1c pedestrian network and topography | **done** |
| 1d edges, periphery, signs and markings | **done** except three house-side items waiting for an OK (see Open decisions) and the optional town silhouette |
| Code review of Phase 1 | **done**: 7 confirmed findings and 8 smaller ones fixed (see "Review round 1") |
| 2a Mercer Library shell, doors, streaming, light pool | **done** |
| 2b interior kit from the house's own assets; interior-mapped windows | **done**, every check passing |
| Code review of Phase 2 | **done**: 25 confirmed findings, all fixed (see "Review round 2") |
| 2c Tier A: Harlow Hall, Hollis Science, the Goose Union, stadium | **done**, each building's walk passing (see 2c); the clock tower stair (stretch) not started |
| 2d Tier B (6 buildings) and Tier C (6 lobbies) | **done**, every walk passing (see 2d) |
| 2e life and sound: wall clocks on the tower's time, room tone, drip, TV crowds, the radio show, the drumline through the windows | **done** (see 2e) |
| 3a stadium exterior and apron | **done** (see 3a) |
| 3b campus buildings and ground | **done** (see 3b) |
| 3c landmarks, homecoming dressing, street furniture, the tower's base | **done** (see 3c) |
| 3d light, atmosphere and life | **done** (see 3d) |
| Extras (approved after Phase 4): the four house-side fixes, the town of Calder, the clock tower stair | **done** (see "After Phase 4") |
| 4 performance and every review | **done** (see Phase 4): every review passing; all street and campus views within 1 ms of before the job except the library forecourt (+1.5) and the aerial (+2.5); Tier A insides 79-143 fps |

## 1a + 1b: roads, lots, and the blocks re-placed

**What changed (all at runtime, switch `-NoCampusLayout` gives the old campus back exactly):**
- **New file pair** `HouseCampusLayout.h/.cpp` holds the plan as data (16 blocks with names, kinds, tiers, footprints,
  entrance faces) and the builders for the campus ground.
- **Calder Dr** replaces the old side street south of the junction:
  - gateposts and a CALDER STATE UNIVERSITY / FOUNDED 1891 sign wall;
  - a planted median with crosswalks;
  - a curve west into Stadium Dr, which ends in a bus loop;
  - a bus bay at the stadium's south side.
  The street blade now reads CALDER DR. The old name was a real person's.
- **Lots:**
  - the **South Lot** has five tree islands built around existing trees, painted stalls and wheel stops;
  - a small **visitor lot** off the old drive mouth;
  - the **Ashby permit lot** sits on its own lane.
  The old health-centre lot is gone. Its 21 cars keep their random draws and are moved into the new stalls.
- **Blocks:** all 16 buildings stand where the plan puts them, each with its carved name sign. They face their real
  entrances (N, S, E or W) and get extra side entrances.
  - The library heads the main quad on the Y -10800 axis, with the clock tower at its east end.
  - The Goose Union's west doors open onto the tower plaza.
  - Harlow and Ashby are the dorms, and the homecoming dorm dressing follows them.
  - Every walk lamp, with its banners, now stands along the new walk network.
- **Edges:**
  - the campus-side runs of the broken hedge ring are gone;
  - a hedgerow closes the campus at Y -29800 and at X ±31700;
  - far blocks no longer stand on walkable ground.
- **Lights:** the three health-lot poles moved with their lots (same three lights). **Exterior lights: 53.**

**Invariants:**

| Check | Result |
|---|---|
| `WOODS_PASS_ORIGINAL_TREES` | crc=0f3b2bb7, unchanged |
| Trees hidden by the plan | 0 (`CAMPUS_TREES_HIDDEN plan=1 hidden=0`) |
| `-NoCampusLayout` | `campusFrontVertices=1261140`, identical to before the job |

**Reviews (layout on):** all PASS.
- WoodsPass, SurroundingsPolish (road floors 5/5), FraternityBoundary, CampusWoods, ParkingLotWalk;
- ClockTower (`exteriorLights=53`);
- FreeRoamWalk (crossed 3/3).

**Reviews (`-NoCampusLayout`):** the same five exterior reviews PASS.

**Shots** ([shots/1ab](shots/1ab)):

| Before | After |
|---|---|
| ![](shots/1ab/plan_campus_before.png) | ![](shots/1ab/plan_campus_after.png) |
| ![](shots/1ab/detail_layout_gateway.png) | ![](shots/1ab/detail_layout_quad.png) |
| ![](shots/1ab/detail_layout_curve.png) | ![](shots/1ab/detail_layout_southlot.png) |
| ![](shots/1ab/detail_layout_dormrow.png) | ![](shots/1ab/detail_clocktower_far.png) |

## 1c: walk network, the street's real profile, topography

Plan revision 2 (PLAN.md has the reasons). The in-game review found every one of these:

- **Street-level roads.** Roads and lots sit at -72 like Fraternity Park Dr, with the lawn cut away under them. Calder Dr
  now meets the street flush; v1's ramp was half buried and showed as a brown band at the junction. Curbs are 15 cm.
  Walks sit at -57. Curb ramps run down to the road at every crosswalk, and walks ramp across the drives they cross.
- **The loop.** Founders Way behind the library bends into the south lane, which serves the South Lot's back drive, the
  plant yard and the overflow lot. The Commons has a dock lane.
- **Front row set back.** It now stands at -7100, so the 5.5 m entrance aprons, planters and posts are off the
  promenade.
- **Entrances turned or moved.** Calder Hall and the Fieldhouse turned their entrances. Whitcomb Fine Arts moved 4 m.
  Harlow is 2.5 m shallower, so a birch keeps its place.
- **Topography.** Five landscaped berms in open lawn (smooth height fields in the terrain's own grass), and a stone seat
  wall round the tower plaza. The library's raised terrace and steps come with its shell in 2a, because the library
  floor has to rise with it.

**New review `-CampusLayoutReview`:** PASS.
- 16 buildings, 25 entrances all on the walk network, 0 overlaps.
- 4 lots all reached by a road, 16 roads with 0 dead ends.
- One pedestrian network, 0 walk-on-lane cells.
- 2,325 floor samples at walk height: 0 misses and 0 obstructions under a player-sized capsule.
- 0 trees or poles in buildings or lanes; 53 exterior lights.
- Result file: [shots/1c/campus_layout_review_result.txt](shots/1c/campus_layout_review_result.txt).

**`-FreeRoamWalkReview` extended:** PASS. The 3 old legs, plus a continuous 18-waypoint campus route walked with real
input:
- from the house's front walk, across the new mid-block crosswalk;
- through the Calder Dr gateposts;
- round the stadium apron to the ticket plaza;
- along the promenade, through the quad to the library, and to the clock tower.

Shots: Route1..Route5 in [shots/1c](shots/1c).

**Every review, layout on:** PASS.
- CampusLayout, WoodsPass, SurroundingsPolish (road floors 5/5), FraternityBoundary, CampusWoods, ParkingLotWalk;
- ClockTower (`exteriorLights=53`);
- FreeRoamWalk (campusRoute=18/18).

**`-NoCampusLayout`:** the same set passes, and `campusFrontVertices=1261140` is still identical. Tree crc 0f3b2bb7, 0
trees hidden.

| Plan, revision 2 (in game, labelled) | |
|---|---|
| ![](shots/1c/plan_campus_rev2_labelled.png) | ![](shots/1c/Route2_Gateposts.png) |
| ![](shots/1c/detail_layout_curve.png) | ![](shots/1c/Route1_Crosswalk.png) |

## 1d: signs, markings, edges

New file pair `HouseCampusStreetDetail.h/.cpp`. All of it is emission-only and invented.
- **Signal.** A signalised mid-block crossing: mast arm, 3-section heads showing green by emission, a lettered DON'T
  WALK head and a push button.
- **Calder Dr exit.** STOP and a stop bar for traffic leaving campus.
- **Regulatory and guide signs.**
  - SPEED LIMIT 25 on Calder Dr, Stadium Dr and Founders Way;
  - ONE WAY on the bus loop island;
  - FOUNDERS WAY / FRATERNITY PARK DR blades;
  - STADIUM, VISITOR, PERMIT and OVERFLOW parking signs.
- **Kerbside.** Coin meters at the visitor lot, storm inlets and manholes, and the bus bay's yellow curb.
- **Overflow lot.** A gravel overflow lot edged with railroad ties off the south lane.
- **Street fix (withdrawn).** Fraternity Park Dr's stop bar was moved to the lane that approaches the junction. That lane
  is the house's half of the street, so the move was reverted and the question added to Open decisions.
- **Edges.** A hedgerow closes the campus at Y -29800 and X ±31700 (done in 1a).

| | |
|---|---|
| ![](shots/1d/detail_layout_crossing.png) | ![](shots/1d/detail_layout_stop.png) |
| ![](shots/1d/detail_layout_seatwall.png) | ![](shots/1d/detail_layout_westroad.png) |
| ![](shots/1d/detail_layout_overflow.png) | ![](shots/1d/detail_layout_berm.png) |

## 2a: Mercer Library, walk-in

New file pair `HouseCampusInteriors.h/.cpp`, built the way the house's runtime rooms are (the black hall, the hallway
detail): a world subsystem, a transient actor every machine builds the same way, and replicated doors only the server
spawns.
- **Shell.** The library's two open floors are hollow: the outer walls are a skin with every window and door cut
  through, and the closed band above, the plinth, string courses, pilasters and cornice are as before. From the street
  it looks the same as the solid block (`HouseExteriorCampus::ShellBuilding`).
- **Inside** (canonical frame, main doors on +Y):
  - ground floor: terrazzo lobby, circulation desk, card catalog hall, periodicals, reference reading room, carrels,
    and the staff workroom (locked);
  - two stairs up to the stacks floor: steel ranges, carrels along the windows, group study, and the copy room (locked);
  - troffers, signs, balustrades round both stair wells.
- **Doors** are `ACampusDoor`, the black hall's procedural door with a campus ID and a replicated `LockedReason` the HUD
  shows ("Locked: STAFF ONLY - the library workroom."):
  - a glazed oak French pair at the front;
  - the locked workroom and copy room doors;
  - a locked STAFF ENTRANCE at the back, added in review round 1.
  Doors are not spawned under the house's own door reviews, so their counts are unchanged.
- **Streaming.** The inside is built when a view or pawn comes within 35 m and freed 10 s after everyone is beyond 60 m.
- **Light pool.** 6 shadowless point lights on lighting channel 1 only, switched on only while the local view is inside.
  The interior meshes are on channel 1 only, so no street light leaks in and no interior light leaks out. **Exterior
  lights stay 53;** interior lights are reported separately (`maxInteriorLights=6`).
- **Indoors.** Exposure, the film look and the bell ducking treat the inside of the library as indoors
  (`HouseDuskAtmosphere`, via `HouseCampusInteriors::DistanceOutside`).
- **Materials.** `Tools/claude/build_campus_materials.py` (editor Python, the project's pattern) made
  `/Game/SeniorSendOff/House/CampusInterior/M_CampusInteriorWindow` (interior mapping for closed floors, used in 2b) and
  `M_LightPoolDecal` (3d). The folder is on the cook list in `Config/DefaultGame.ini`.

**New review `-CampusInteriorsReview`:** PASS. Real input walks a 26-stop route:
- in through the front doors;
- round every ground-floor room, up the stairs, round the stacks, and back down;
- tries both locked doors (they hold, and the reason shows);
- back out, then away, so the inside is freed.

Its checks: indoor test 24/24 and outdoor 2/2, built while inside, freed after leaving, at most 6 interior lights.
Result and photos: [shots/2a](shots/2a).

**Every review after 2a:** PASS.
- CampusLayout, WoodsPass, SurroundingsPolish, FraternityBoundary, CampusWoods, ParkingLotWalk, ClockTower,
  FreeRoamWalk (campusRoute 18/18);
- CampusInteriors;
- with `-NoCampusLayout`, and with `-NoCampusInteriors`;
- tree crc 0f3b2bb7, 53 exterior lights.

**The honest gap.** Side by side with the house's basement library, Mercer reads as a blockout: the right rooms and
furniture in the right places, but plain boxes, flat colours and no layering. Closing that gap is 2b.

![](shots/2a/compare_house_vs_mercer_2a.png)

| | |
|---|---|
| ![](shots/2a/LIB_1_Doors.png) | ![](shots/2a/LIB_2_Lobby.png) |
| ![](shots/2a/LIB_4_Reading.png) | ![](shots/2a/LIB_6_Stacks.png) |

## 2b: the house's own furniture in Mercer, and interior-mapped windows

**The kit.** Mercer is now furnished from the house's basement library:
- **The pieces.** 34 kinds: bookcases with the house's shelf rows, study tables, bentwood chairs, leather club chairs,
  lamps, pendants, rugs, framed photographs, plants, books, papers, cups, backpacks and an umbrella. Each comes with its
  own materials and all four damage stages.
- **The house's proportions undone.** The house builds horizontally 1.8× (SourceArt/BasementLibrary/README.md), so the
  campus scales its meshes 0.556 in X and Y.
- **Decor (267 pieces):** bookcases and shelf rows, rugs, pendants, wall art, and the workroom's worktable and boxes. They
  are fixed plain mesh components, one per piece, as the house places them. Instancing was tried first, but the house's
  materials are not flagged for instanced meshes, so every instance drew the engine's checker fallback.
- **Props (63):** chairs, club chairs, side tables, lamps, plants, books, papers, cups, backpacks and the umbrella. They
  are `ACampusProp` (: `AHouseInteractiveProp`), which the server spawns; only a kit index replicates. They carry, move
  and break like the house's furniture. Their breaks do not count as house damage (`SSOCampusDamage`, filtered in
  `HouseDamageSubsystem`).
- **Shelf rows.** The house's own bookcases were surveyed read-only (`Tools/claude/survey_campus_shelves.py`). Its shelf
  rows sit in the case's frame on boards 2 cm up and then every 30 cm, and the campus fills its cases the same way.
- **Wall craft.** Oak wainscot with a cap rail and stiles on every partition face and outer wall, plus oak window stools
  and casings inside. The pooled lights are warmer and fuller (3200 lm, r 1150), still 6.

**Interior-mapped windows on every campus building's closed floors.**
- **The material.** `M_CampusInteriorWindow` builds its view ray in world space from the camera vector, the wall normal
  and world up. The first version used tangent space and was wrong on two wall sides. It casts the ray into a box room
  and shades it from a painted 2×2 atlas: a dorm room, an office, a classroom and a lounge.
- **Variants.** Eight MIDs (`CI_Room0..3Lit/Dark`). The windows' u runs along cross(normal, up) on every side.
- **Which room shows.** Dorms show mostly dorm rooms with a lounge per floor; the other buildings show offices,
  classrooms and lounges. Lit or dark follows `HouseCampusFall::WindowLit`, as before.
- **Off switch.** `-NoCampusInteriors` gives back the painted windows.

**Evidence** ([shots/2b](shots/2b)):
- `-CampusInteriorsReview` still PASSes 26/26 with all the furniture in place (the route stays clear).

![](shots/2b/compare_house_vs_mercer_2b.png)

| | |
|---|---|
| ![](shots/2b/132_detail_lib_bookcase.png) | ![](shots/2b/134_detail_lib_periodicals.png) |
| ![](shots/2b/133_detail_lib_photo.png) | ![](shots/2b/137_detail_win_straight.png) |

**Open for Phase 4.** Building Mercer's inside takes one frame of about 265 ms when someone first comes within 35 m. The
plan's budget is 3 ms a frame, so the build must be spread over frames.

## 2c: Tier A, the four big buildings and the stadium

Each building is one file on the registry (`FInteriorDef` in `HouseCampusInteriorsRooms.h`), built from the shared room kit,
the 1986 fixtures and the house's own furniture. `-CampusInteriorsReview=HAR,SCI,UNI,STA` walks them (the list form now
works: `FParse::Param` does not match `=...`, and `FParse::Value` stopped at the comma; both fixed).

| Building | File | Walk | What is in it |
|---|---|---|---|
| Harlow Hall (dorm) | `HouseCampusInteriorsHarlow.cpp` | **PASS 61/61** | Two floors, racetrack plan, painted block, vinyl tile, dropped ceilings. Lobby with the hall desk's pass-through, mailboxes, alumni sign-in; TV lounge (console TV showing the game, vending, card table, coats on chairs); communal bathrooms (stalls, showers, sinks, urinals or vanity); laundry; rec room (ping-pong, pinball GOOSE CHASE, arcade); study room; floor kitchen; window commons. 11 furnished rooms in 6 characters (football player, record collector, studious, mess, float builders, musician), each with the house's loft bunk, desks, dresser, rug and clothes, milk crates, boomboxes, invented-band posters, mini fridge and hot plate, a Geese pennant. 29 locked rooms with reasons; their windows interior-mapped. |
| Hollis Hall of Science | `HouseCampusInteriorsScience.cpp` | **PASS 39/39** | Lobby with twin oak stairs, directory (agrees with the plates), specimen case, Physics Club bake sale. Chemistry lab (three fume hoods, four epoxy islands with glassware, reagent cabinets, safety shower, periodic table to 103). Physics lab (air track, pendulum, inclined plane, oscilloscope, Van de Graaff). Tiered lecture hall two storeys high (ten rows of fixed seats, three boards of equations, giant slide rule, demo bench, lectern, overhead projector and screen, drapes). Upstairs: optics lab (laser benches), seminar room, faculty lounge with the alumni coffee. Prep room, office, darkroom, faculty offices locked. |
| The Goose Union | `HouseCampusInteriorsUnion.cpp` | **PASS 34/34** | Four real entrance pairs (`SidePair`), a concourse cross round a two-storey atrium, two stairs. TV lounge (three CRTs replaying the game), bookstore behind glass (closed Saturday), the Goose Pit (menu board, serving line, kitchen, half-eaten burgers and fries, coats on chairs), game room (four pool tables, three pinballs: GOOSE CHASE, HALFTIME, MOON RANCH; arcades, foosball, jukebox, pool ladder), mailroom (brass PO boxes, service window closed), ride board, payphones, cigarette machine, trophy case with The Old Lantern (Calder vs Dunmore since 1923), floor seal. Upstairs: homecoming committee room (tissue scraps, chicken wire, paint, the float's goose head half done, BEAT DUN banner), Calder Ballroom set for the Class of 1961's 25th reunion dinner. Student government, The Honker, meeting rooms locked. |
| The stadium | `HouseCampusInteriorsStadium.cpp` | **PASS 65/65** | Free-standing (world space). Concourse behind the north stands entered through GATE 4 (turnstiles) and GATE 5; window band lit; GEESE GRILL, popcorn stand, cold drinks (shuttered), restrooms (locked for cleaning); a stair to the vomitory in segment 11, where rows 4..7 give way to a landing over the field. Geese locker room under the east stands: lockers with jerseys, benches, helmets, tape on the floor, BEAT DUNMORE struck out with 24-17 under it, the game ball on its stand, the tunnel through the podium wall. Press box hollow (the exterior's shell): press row of typewriters and phones, the papers' cards, radio booth ON AIR (locked), PA and scoreboard desk, spotters' charts. A brick stair tower (eight flights) climbs to it. |

**The exterior's side of the stadium** (only while it is walk-in; `-NoCampusInteriors` gives the old bowl back):
`HouseExteriorCampus` asks `HouseCampusInteriors::StadiumGate/Concourse/Vomitory/Tunnel` and builds the gate segments
(8, 10) with a real opening, the stone arch, and the grille half up in the lunette (the base course stops at the opening);
the concourse segments' window band lit; rows 4..7 of segment 11 cut; the podium wall of segment 6 with the tunnel's
mouth; the press box as a hollow shell; the stair tower. The rim's rail and posts and the rim pennants are left out under
the press box (they ran through it), and the bedsheets on the outer wall hang 80 cm lower; litter is not dropped where the
vomitory cut the rows, and no bedsheet hangs over the tunnel.

**Shared framework added:** `BlockWall` (cinderblock partition with hollow-metal frames and cove base, openings),
`InnerDoor` (a door in any axis-aligned partition, hinged on its U0 jamb), `SidePair` (a glazed pair on any outer wall),
doors that stand open (`FDoorSpec::bOpen`, `AExistingHouseDoor::SetStartsOpen`), `Troffer` with a flicker key, a pinball's
own backglass, 11 more house pieces (jacket, shirt, towels, sneaker, striped rug, wastebasket, laundry basket, desk lamp,
daybed, laundry bag; surveyed by `Tools/claude/survey_campus_bedroom.py` for the loft bunk's stacking). Fixture lettering
was mirrored (the fixture frame's Right is its own, the viewer's is the other way): fixed in `FFixFrame::Face`.

**Lights:** still at most 6 pooled interior lights at once (every walk: `maxInteriorLights=6 pool=6`); everything else is
emission. Exterior lights unchanged (53).

## 2d: Tier B and Tier C, from one plan generator

`HouseCampusInteriorsGeneric.cpp` builds twelve buildings from a one-line spec each (`FGenSpec`): rooms as tokens
`<kind><bays>[:NAME]` west to east along the front and the back, `L` for the lobby. The plan generator puts the lobby at
the main doors, a corridor across the building on the end walls' piers, a row of rooms either side; rooms too deep for
what they are stop short and leave the rest closed (interior-mapped windows). Room plates, the directory and the hanging
sign are painted from the same plan, so they agree. Rooms are furnished by the shared templates
(`HouseCampusInteriorsRoomKits.cpp`: classroom, seminar, office, lounge, dining, reception, waiting room) and a few
specials (registrar's counter, the dean's paneled office, studios, the gallery, the auditorium, the rec room, the laundry,
dorm rooms through Harlow's `HarFurnish`).

| Building | Tier | Open rooms |
|---|---|---|
| Old Main | B | Dean of Students (paneled), Admissions waiting room, the Registrar's counter, the conference room; Records, Bursar, Financial Aid, Counseling, the President locked |
| The Commons | B | The Hearth lounge, the dining hall set with trays left behind; the manager's office and the kitchen locked |
| Ashby Hall (dorm) | B | Three open dorm rooms (Harlow's characters), the laundry, the lounge, the rec room, the study lounge; the rest locked |
| Calder Hall (humanities) | B | English and History offices, two seminars, two classrooms, the auditorium |
| Kerr Hall | B | Four classrooms, the seminar, the lecture room |
| Whitcomb Fine Arts | B | Painting and drawing studios, the Whitcomb Gallery (the fall student show), ceramics and sculpture; the kiln room locked |
| Health center, Fieldhouse, Band and Music, Pratt Engineering, Physical Plant, Alumni Hall | C | A lit lobby each: directory, notices, payphone, ash urn, its own display (the health center's closed reception window, waiting chairs and flu-shot posters; the fieldhouse's trophy case and ticket counter; the music hall's instrument cases and stand; engineering's truss bridge model and drafting table; the plant's time clock, card rack and work-order board; Alumni Hall's class photographs, guest book and coffee), locked doors with their reasons and a chain and padlock; BUILDING CLOSED SATURDAY / LOBBY OPEN FOR HOMECOMING |

**Walk:** `-CampusInteriorsReview=MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU` **PASS 174/174**, problems 0, main
doors 12/12, built while inside, freed after leaving 12/12, at most 6 interior lights. Result and shots: [shots/2d](shots/2d).

| | |
|---|---|
| ![](shots/2d/MAIN_2_Room.jpg) | ![](shots/2d/HLTH_1_Lobby.jpg) |
| ![](shots/2d/ART_2_Room.jpg) | ![](shots/2d/COM_2_Room.jpg) |

## 2e: life and sound

- **Wall clocks (64 across the campus).** A schoolroom clock (steel rim, a round painted dial, two hands that turn) in
  every room template (over the board in classrooms and seminars, high on the back wall elsewhere), in each Tier B/C lobby
  and corridor, and placed by hand in the custom buildings: the library (over the workroom door, and upstairs), Harlow
  (lobby, lounge, both corridors on both floors, laundry/study, rec room/kitchen), Hollis (the lecture hall, over each
  lab's board, both corridors), the Union (TV lounge, over the information desk, upstairs), the stadium (concourse,
  locker room, press box). They read the clock tower's own time (`AHouseClockTower::ClockSeconds`, so `-ClockTime=`
  moves them too), updated once a second. The shots show the tower and the wall clocks agreeing.
- **Sounds (`HouseCampusInteriorsSound.cpp`), all synthesised, no recordings and no voices**, fed as looping procedural
  waves:
  - **room tone** inside any building: HVAC rumble, the 60-cycle hum, the tubes' 120/240/360 Hz buzz; it fades in when
    you step inside and out when you leave;
  - **a dripping tap** in Harlow's men's room;
  - **the TV crowd** (the replay: swells, two cheers, a whistle, the band's tag) from Harlow's and the Union's lounge sets;
  - **the postgame radio show** (a jingle, then two "voices" made of buzzing formants in syllables and phrases through a
    phone-line band: talk heard through a wall, no words) from room 101's boombox and the press box's broadcast desk;
  - **the drumline through the windows:** while you are inside, `HouseCampusFall`'s drums are low-passed to 650 Hz and
    down to 40%.
  Close sounds are full within 2.5 m and gone by 13 m, muffled with distance. `-NoCampusSound` turns them off.
- **Signs and directories agree:** every directory is painted from the same data as the plates and signs it lists (2c, 2d).
- **The proof.** `-CampusSoundDump` writes the four loops as WAVs ([shots/2e](shots/2e)). The loop seams are measured clean (the
  jump at the seam is no larger than a typical sample step). `-CampusSoundRecord=6` recorded the game's mix with the view
  2.7 m from Harlow's lounge TV ([recorded_har_lounge_tv_6s.wav](shots/2e/recorded_har_lounge_tv_6s.wav)): the crowd band
  (250 Hz-2 kHz) sits over the room tone with the crowd's swells, after a rebalance (room tone 0.55 → 0.3, TV and radio
  louder). `-muteaudio` keeps the speakers silent while the submix is still recorded, so these runs make no noise.
- **Also fixed here:** the Tier C lobby note was clipped ("UILDING CLOSED SATURDA"); the library's corridor between the
  stairs was dark (its pooled-light anchor moved from inside the locked workroom to the corridor).

**Walks after 2e:** Tier A `LIB,HAR,SCI,UNI,STA` **PASS 225/225** (the Union rerun after its ground-floor walls were cut at
the ceiling), Tier B/C **PASS 174/174**; every walk `maxInteriorLights=6 pool=6`.

| | |
|---|---|
| ![](shots/2e/140_detail_clock_har_lobby.jpg) | ![](shots/2e/145_detail_clocktower_face_2e.jpg) |
| ![](shots/2e/141_detail_clock_sci_hall.jpg) | ![](shots/2e/143_detail_clock_sta_concourse.jpg) |

## Phase 3: exterior polish (HouseCampusPolish*.cpp)

One transient actor, `AHouseCampusPolish`, beside the exterior (spawned by `HouseExterior` after the clock tower and the
homecoming dressing); everything built at runtime, the same on every machine, with no lights. `-NoCampusPolish` (or
`-NoCampusLayout`) gives back the campus exactly as Phase 2 left it: every change the exterior or the homecoming files make
for Phase 3 asks `HouseCampusPolish::Enabled()`. Parts: `Solid` (collides), `Detail` (drawn, culled at 90 m), `Far`
(drawn, never culled: roofs and the stadium's facade, the skyline), `Glow` (emission), `Pools` (emissive decals), puffs and
cloth (moved every frame), sound.

### 3a: the stadium and its apron

- **Facade.** Brick pilasters at all 40 bays with stone bases and caps; a stone plinth course; sill and lintel courses; a
  frieze under a real cornice (fascia and soffit on the rim's overhang, modillions every 1.2 m). CALDER STATE STADIUM is cut
  into the frieze at the three approaches (east, west, south), with ERECTED BY THE ALUMNI and DEDICATED OCTOBER 22 1927
  either side of the south one. A GATE number in every arch, a SEC plate on every bay.
- **Openings.** Every bay's window is now a real opening (`HouseCampusPolish::StadiumWall` cuts the exterior's brick;
  the gate segments' wall too): stone reveals back to the glass, a projecting sill, a lintel and keystone. Where the walk-in
  concourse is behind the bay (segments 7-12) you see it lit through the glass, its steel sash lining up with the concourse's
  own mullions (`HouseCampusInteriors::StadiumWin*`, shared by both); elsewhere dark glass and a sash. Iron gates in the blind
  arches, five of them with the grille rolled half up over a lit vestibule and its turnstiles; ticket windows with their
  counters and bars set into six bays.
- **Structure.** Steel outriggers and knee braces under the press box's overhang, a column under its deepest (west) end,
  antennas, guy wires and a dish on its roof. The flood heads are real lamp banks now: a tilted frame of twelve lamps with
  visors (the lenses emissive), a catwalk with its rail, a caged ladder down the pole, the junction box and conduit at a
  concrete footing (`HouseCampusPolish::FloodBanks`; HouseCampusFall's halos sit on the same lenses).
- **Apron.** A concrete ring round the wall with joints and drains, laid only where nothing of the plan is (walks, roads,
  lots and plazas keep theirs); leaf drifts against the wall, worn grass and mud, overflowing barrels, chain-link panels
  across the service arch with a CALDER STATE PHYSICAL PLANT sawhorse, two shuttered concession stands with hand-painted price
  boards (HOT DOGS 75¢ and the rest; CIDER MILL), a souvenir table with pennants, four portable toilets, the band's two
  yellow buses and Dunmore's charter coach (plain white, CHARTER in its sign) in the bus bay.
- **The South Lot after the tailgates.** Behind the parked cars, in the aisle: webbed chairs (one tipped over), coolers,
  crushed cans, a purple and gold canopy, a kettle grill still smoking; shoe-polish on the cars' windows (GO GEESE, HONK!,
  BEAT DUNMORE, GEESE 24-17...); streamers on their antennas.
- **Kept clear.** Solid props keep off the plan's paving, the free-roam route round the stadium, the walk-in stadium's route
  outside, the tower's doors and the gates' fronts (`FParts::KeepClear`); props that stand on a road on purpose (the
  sawhorses, the tailgates) keep off every walk (the layout review caught a sawhorse on the bus bay's crossing; moved).

| | |
|---|---|
| ![](shots/3a/147_detail_3a_south.jpg) | ![](shots/3a/149_detail_3a_east.jpg) |
| ![](shots/3a/109_detail_stadium_gate.jpg) | ![](shots/3a/151_detail_3a_pressbox_side.jpg) |
| ![](shots/3a/150_detail_3a_grilleup.jpg) | ![](shots/3a/152_detail_3a_flood.jpg) |
| ![](shots/3a/153_detail_3a_apron_se.jpg) | ![](shots/3a/155_detail_3a_tailgate.jpg) |

### 3b: each building's character, and the ground

- **Roofs and massing** (each from its identity): Mercer's copper dome on a pilastered drum; Old Main's slate mansard with
  dormers, a cupola with a lit lantern and a goose weathervane; Harlow's slate gable and chimneys (kept low enough that the
  tower still clears it from the frat lawn); Calder Hall's steep collegiate gable with crenellated ends and pinnacles;
  Hollis's observatory dome and fume stacks; the Union's lit glass lantern over its atrium; the Commons' standing-seam hip and
  kitchen stacks; the fieldhouse's vault with its thermal windows; Whitcomb's sawtooth north lights; the music hall's gable and
  round window; Pratt's wooden water tank; the plant's 36 m smokestack (smoking); Alumni Hall's slate hip; penthouses and TV
  antennas on the rest.
- **Entrances:** a pediment and engaged columns over the classical doors (Mercer, Old Main, Alumni Hall), wall lanterns by
  every main door (emission, with their light pools).
- **Walls:** fire escapes on Harlow, Ashby, Old Main and Calder Hall; autumn Boston ivy on the old stone and brick (kept off
  the doors); window wells along the old buildings' backs; gas and electric meters.
- **Windows** (closed floors only; the open floors are real rooms): blinds at all heights, curtains, a few sashes up,
  window air conditioners and box fans in the dorms.
- **Ground:** the lawns mown in 4.5 m stripes, their colour wandering from lush to dry; desire paths worn where people cut
  corners (the free-roam route round the stadium among them); mulched rings round the campus trees; leaves blown against the
  buildings; every walk jointed every 1.5 m, the sidewalks and plazas slabbed, the tower plaza ringed, a few slabs cracked.
  (`HouseCampusLayout::WalkStrips/PavedRects/PavedDiscs` were added for this, read-only.)

| | |
|---|---|
| ![](shots/3b/119_detail_layout_aerial.jpg) | ![](shots/3b/101_detail_clocktower_far.jpg) |
| ![](shots/3b/157_detail_3b_lib.jpg) | ![](shots/3b/161_detail_3b_art.jpg) |
| ![](shots/3b/162_detail_3b_plant.jpg) | ![](shots/3b/164_detail_3b_ash.jpg) |

### 3c: landmarks, homecoming, street furniture, the tower's base

- **Landmarks:** the Calder Goose in bronze on a granite plinth in the quad (THE CALDER GOOSE, GIVEN BY THE CLASS OF 1961); the
  library forecourt's fountain drained for the season, leaves in its basin; a flagpole plaza on the green flying the
  school's own flag (purple, a gold goose on a white disc, CALDER STATE). No real flags or seals.
- **Homecoming:** the alumni tent (WELCOME HOME ALUMNI - HOMECOMING '86; coffee urns, cider jugs, cups); crepe streamers
  twisted between the walk lamps; the float now reads as a goose (HouseCampusFall, polish on: tissue-pomp textures, one
  smooth neck, a proper bill and chinstrap, a torn patch on its flank showing the chicken wire); the bonfire's site scaled to
  the green: the grass burned in a wide ring, rope on brass stanchions round it, the BONFIRE SITE sign.
- **Street furniture:** YOU ARE HERE maps at four junctions, each painted from the plan itself with its own spot starred;
  fingerposts at the promenade's two big junctions; a trash can by every bench; bike racks with ten-speeds at busy doors;
  roofed notice boards thick with flyers; hydrants on the road verges. No blue-light phones.
- **The tower's base:** four granite steps and a landing up to its door on the lawn face, beds of mums at the plinth's
  west corners, benches on the plaza's west side (review round 3 cleared the Union's forecourt), the bronze plaque (THE
  CALDER TOWER, RAISED 1897). Its door, date stone, lit lancets and ivy were already the tower's own.

| | |
|---|---|
| ![](shots/3c/167_detail_3c_statue.jpg) | ![](shots/3c/168_detail_3c_fountain.jpg) |
| ![](shots/3c/172_detail_3c_bonfire.jpg) | ![](shots/3c/173_detail_3c_towerbase.jpg) |
| ![](shots/3c/174_detail_3c_map.jpg) | ![](shots/3c/170_detail_3c_tent.jpg) |

### 3d: light, atmosphere and life

- **Light pools** (emissive decals, no lights): blue-green under every mercury walk lamp, warm under the lit ground-floor
  windows and round the ticket booths and the main doors' lanterns, the bonfire's embers, a bluish wash under the flood towers
  and over the field (171 pools; 156 after review round 3).
- **Haze** lying low over the green, the quad and the Union's lawn (camera-facing puffs, fading as you walk into them).
- **Motion:** the school's flag flies, the streamers on the antennas whip; the grill and the plant's stack smoke. (Leaves
  falling and the tower's crows were already moving.)
- **Sound** (synthesised, spatialised, quiet; `HouseCampusPolishSound.cpp`): the crowd going home from the gates and through
  the lot, thinning out over ten minutes; the coach idling in the bus bay; a horn honking for the Geese now and then (two
  long, three short); car doors in the lot; wind and crickets everywhere on campus; all of it muffled to a quarter when you go
  inside. `-NoCampusSound` turns it off; `-CampusSoundDump` writes the loops, `-CampusPolishRecord=<s>` records the mix
  (`-muteaudio` keeps the speakers silent). Proof in [shots/3d](shots/3d).

| | |
|---|---|
| ![](shots/3d/177_detail_3d_lamps.jpg) | ![](shots/3d/176_detail_3d_green.jpg) |
| ![](shots/3d/178_detail_3d_flag.jpg) | ![](shots/3d/116_detail_layout_quad.jpg) |

## Phase 4: performance and every review

### How the frame times are taken

Each set is the views below, sampled uncapped (`-ExecCmds="t.MaxFPS 0"`, 300 frames a view after a 4 s warm-up) with
everything on, then with `-NoCampusLayout` (the campus as it was before the job), `-NoCampusPolish` and
`-NoCampusInteriors`, plus the five Tier A insides. That is one round; three rounds alternate, and each number is the
median of the three. `tools/phase4_perf.sh` runs them and `tools/perf_table.py` makes the table.

The Mac needs one more switch, `-MetalOffscreenOnly`. A windowed Mac game always locks its present to the display
(`MetalViewport.cpp` turns on display sync unless fullscreen, whatever `r.VSync` says). Every view therefore read at
least 8.33 ms (120 Hz), which hid any cost below that. The first Phase 4 tables had that floor and are superseded.

### What the campus cost, and what was done

With the floor gone, the campus views were well over the 1 ms target. The aerial view was +6.3 ms and the library
forecourt +3.8 ms. Unreal's CSV profiler (`-ExteriorViewCsv`, added to the view sampler) showed why: the frames were
bound by the **render thread** (about 10.9 ms) while the GPU used about 6.7 ms. The campus had added:
- 1,418 procedural mesh sections (the polish). A procedural mesh's sections are gathered again every frame on the
  dynamic path, for every pass;
- the interiors' procedural parts;
- 161 doors that stood all the time, with about 1,000 moving parts, even with no inside built.

The changes:
- **Polish tiles are runtime static meshes** (`HouseCampusStaticMesh`, the woods' method): the renderer caches their
  draws. The solid part keeps a procedural copy that is not drawn, for its collision.
- **Fewer, larger tiles where tiling did nothing.** The skyline, glow and lawns are never culled, so their small tiles
  only multiplied their materials. The ground's joints, leaves and mulch were in every Detail tile; they are now their
  own `Ground` part. Any Detail or Solid material that lands in six or more small tiles goes to large ones. Sections fell
  from 1,418 to 668.
- **Interior parts are drawn as static meshes too.** They are converted a piece at a time after the building is built;
  the longest step is about 14 ms.
- **Inner doors are drawn only while their building's inside is built.**
- Furniture inside a building is culled at 60 m, as its fixed decor already was.

### The final frame times

Everything on, against the campus as it was before the job (`-NoCampusLayout`), on the finished code with the extras (the
town, the walk-in tower): `tools/phase4_perf.sh p8p`, three alternating rounds, medians (`runs/p8p_perf_table.md`).
Before the extras, the same comparison pooled over six rounds (`p6z` and `tile_*`) gave the second column. Three-round
sets differ by up to about 0.5 ms from one another.

| View | All on (ms) | Before the job (ms) | Campus cost | Before the extras | Before this pass |
|---|---:|---:|---:|---:|---:|
| `front_street` | 22.16 | 22.25 | **-0.09** | +0.08 | +0.43 |
| `street_along` | 14.35 | 14.36 | **-0.01** | +0.24 | +0.26 |
| `detail_street_dusk` | 14.03 | 13.97 | **+0.06** | +0.22 | +0.25 |
| `detail_establishing_corner` | 17.81 | 17.71 | **+0.10** | +0.27 | +0.54 |
| `detail_layout_dormrow` | 7.90 | 7.74 | **+0.16** | +0.66 | +0.37 |
| `detail_stadium_gate` | 7.20 | 6.99 | **+0.21** | +0.62 | +0.61 |
| `detail_campus_quad` | 8.08 | 7.81 | **+0.27** | +0.73 | +1.62 |
| `detail_clocktower_far` | 6.11 | 5.79 | **+0.32** | +0.33 | +1.14 |
| `detail_layout_gateway` | 7.16 | 6.61 | **+0.55** | +0.76 | +1.97 |
| `detail_layout_quad` (the library forecourt) | 8.46 | 7.00 | **+1.46** | +1.84 | +3.80 |
| `detail_layout_aerial` | 13.57 | 11.08 | **+2.49** | +2.02 | +6.32 |

"Before this pass" is the same comparison on the code as Phase 3 left it, uncapped (`p5z`, two rounds).

Two views are still over the 1 ms target:
- **The library forecourt** stands 9 m from Mercer's doors, with Old Main and Hollis inside the 35 m build range too.
  Three walk-in insides are built around the camera and seen through their real glass. The interiors cost about 1.0 ms
  of the 1.8 and the polish about 1.0 (the two overlap). Any spot next to three open buildings will be about the same.
- **The aerial view** is 75 m up and sees the whole campus at once, and now the town of Calder and its ridges behind it.
  The three insides built for the forecourt view are still standing (an inside is freed 10 s after everyone leaves).
  No player stands there.

Every street view, and every campus view at eye level away from that cluster, is within 1 ms.

Inside each Tier A building (its signature room, uncapped, three runs, `p8p`):

| Inside | ms | fps |
|---|---:|---:|
| Harlow Hall (the TV lounge) | 12.64 | 79 |
| Hollis Hall of Science (the lecture hall) | 8.73 | 115 |
| The Goose Union (the concourse) | 8.61 | 116 |
| Mercer Library (circulation) | 7.88 | 127 |
| The stadium (the concourse) | 7.01 | 143 |

All are well above the 60 fps target.

### Every review, on the finished code (run `p8z`, after the extras; `p6z` before them gave the same)

| Review | Result |
|---|---|
| `-CampusLayoutReview` | PASS: 16 buildings, 31 entrances, 0 off the network, 0 overlaps, 4 lots all reached, 16 roads, 0 dead ends, one walk network, 2,654 floor samples with 0 misses, 0 obstructions, 95 campus trees and 15 poles all clear, 53 exterior lights |
| `-WoodsPassReview` | PASS: 53 exterior point and spot lights; tree stream `crc=0f3b2bb7` (unchanged); 1 tree hidden under the plan |
| `-CampusWoodsReview`, `-FraternityBoundaryReview`, `-SurroundingsPolishReview`, `-ParkingLotWalkReview` | PASS |
| `-FreeRoamWalkReview` | PASS: the three street crossings and the campus route, 20/20 waypoints |
| `-ClockTowerReview` | PASS (with the walk-in tower): 1 colliding part, hands within 0.4 s of the time, chimes and strikes, 0 tower lights, 53 exterior lights |
| `-CampusInteriorsReview` (Tier A and the clock tower) | PASS: 256/256 stops, main doors 6/6, locked doors hold and show their reason, built while inside, freed after leaving 6/6, indoor test 233/233, light pool 6 (at most 6 lit), 28 wall clocks on the tower's time |
| `-CampusInteriorsReview` (Tier B and C) | PASS: 174/174 stops, main doors 12/12, freed 12/12, indoor test 150/150, light pool 6, 37 wall clocks |
| `-CampusInteriorsSmoke=All` | PASS: 18/18 buildings, the clock tower included (each folder in `SourceArt/CampusInteriors/` has its JSON) |
| The same reviews with `-NoCampusPolish`, `-NoCampusInteriors`, `-NoCampusFall`, `-NoCampusLayout` | PASS (with `-NoCampusLayout` the layout review reports the plan off) |

The interior light pool stays at 6, and no light was added outside. The polish builds in about 1.4 s at load; an inside
builds in 15-75 ms when someone comes within 35 m, then turns into static meshes in steps of at most about 14 ms.

### Screenshots

The required views, before the job (`-NoCampusLayout`) and after, are in [shots/4](shots/4), as `before/` and `after/`
with the same names:
- the top-down site and campus (`detail_plan_site`, `detail_plan_campus`);
- eye level:
  - the main intersection (`detail_layout_crossing`);
  - the entrance drive (`detail_layout_gateway`);
  - the quad axis (`detail_layout_quad`, `detail_3c_promenade`);
  - the stadium approach and gate (`detail_3c_map`, `detail_stadium_gate`);
  - the lot (`detail_layout_southlot`);
  - the dorm cluster (`detail_layout_dormrow`);
  - a road leaving town (`detail_layout_westroad`);
  - the clock tower's base (`detail_3c_towerbase`);
- the existing `detail_stadium_*`, `detail_campus_*`, `front_street`, `street_along`, `detail_establishing_corner`,
  `detail_street_dusk` and `detail_clocktower_far`.

Every Tier A signature room and one shot of each Tier B or C building are in that building's folder under
`SourceArt/CampusInteriors/`, beside a top view of it before and after. The campus room beside a house room at the same
camera height is [shots/2a/compare_house_vs_mercer_2a.png](shots/2a/compare_house_vs_mercer_2a.png).

| Before | After |
|---|---|
| ![](shots/4/before/detail_plan_campus.jpg) | ![](shots/4/after/detail_plan_campus.jpg) |
| ![](shots/4/before/detail_layout_gateway.jpg) | ![](shots/4/after/detail_layout_gateway.jpg) |
| ![](shots/4/before/detail_layout_quad.jpg) | ![](shots/4/after/detail_layout_quad.jpg) |
| ![](shots/4/before/detail_3c_towerbase.jpg) | ![](shots/4/after/detail_3c_towerbase.jpg) |

### Snapshot and patch

`campus_master.patch` (next to this file) holds every change the job made: 63 files, 18,106 lines. It contains the
pre-existing files' diffs against the pre-job snapshot (`Saved/CampusMasterWork/baseline/`, see its `REPORT.md`), then
every new source file in full.
- **Verified:** applying it to a copy of the snapshot reproduces every one of the 63 files in the project byte for byte.
- **Complete:** every source, config and tool file changed since the job began is in it.
- **Not in it:** the two new binary material assets, `M_CampusInteriorWindow` and `M_LightPoolDecal`. They are made by
  `Tools/claude/build_campus_materials.py`, which is in the patch.
- **Nothing is committed.**

## Review round 1 (Phase 1 code, adversarially verified)

A review workflow (3 finders, each finding checked by a separate skeptic) went over the layout, its review and the
free-roam route. Seven findings were confirmed. All are fixed, along with eight smaller ones the finders also reported:
- **The Calder Dr median ran through two crosswalks.** Its "refuge cut" added a box inside the median instead of
  removing one, so a 15 cm grass curb stood mid-crossing. The median is now built in pieces with a road-level refuge at
  each crosswalk.
- **Walk ends ran onto ramps, lots and roads.** Every free walk end ran on by half its width. On a curb ramp that left a
  7-12 cm ledge, and over a lot it left a slab floating 7 cm above the asphalt. A walk now runs on only over lawn
  (`RunOn`).
- **Entrance aprons** lapped 1 m onto the promenade and onto the Commons' and Kerr Hall's Calder Dr curb ramps. Each
  apron now stops where paving that does not lead to its door begins (`ApronDepth`); posts and planters stand on the
  ground.
- **Six walks ended at blank walls.** The Union, Whitcomb Fine Arts, the Band and Music Hall, Pratt Engineering and
  Alumni Hall got the side entrances those walks lead to. The arts/music connector became two walks to the right doors.
  Mercer Library got a real, locked staff door at the back, into the workroom. The east walk that dead-ended at the
  stadium wall (no gate there) is gone.
- **Hairline slits to the void:**
  - round the bus loop and the bend, the road polygons are now circumscribed;
  - round all six planting islands, the infill now uses the curb's own polygon;
  - curbs run on one cell under a flat sidewalk, which closes the South Lot corner;
  - road curbs stop at a lot's curb line, which ends the tie/concrete z-fight at the overflow apron.
- **Fraternity Park Dr's far sidewalk.** Leaves no longer float over the visitor-drive gap. The house-side sidewalk's
  joints, which a skip had removed, are back.
- **The review was too lenient.** It is now much stricter:
  - the floor tolerance is 1.5 cm (the lawn under a missing walk is only 3 cm lower, so 6 cm missed it);
  - it samples every curb ramp at its own slope, the curved sidewalks and the walk run-ons;
  - a tree's real trunk collision box must not reach a building, lane or walk (1 tree is now hidden for it);
  - every walk that ends on a building must end at a door;
  - a stale result.txt is deleted at start;
  - a pass no longer ends a run that other reviews share.
- **The free-roam route** no longer grazes the stadium wall's arch blocks or a ticket booth. It now goes round the
  stadium about 3 m off the wall, on clear ground.

Stricter `-CampusLayoutReview` after the fixes: PASS.
- 31 entrances, 0 walks at blank walls;
- 2,620 floor samples, 0 misses at 1.5 cm and 0 obstructions;
- 0 trunks reaching anything;
- 53 lights.

## Review round 2 (Phase 2 code, adversarially verified)

A second review workflow had five finders, each finding checked by its own skeptic. It looked at the interiors code, the
kit, the window material and the round-1 fixes. It confirmed 25 findings and refuted none. All are fixed:
- **Props and freeing.** Freeing a building used to destroy every prop it had spawned, including one a player was
  holding or had carried home. Now:
  - those props stay where they are (orphans), and a rebuild never spawns their twin;
  - a building is not freed while anything else (a house prop, another building's prop) rests on its floors, so
    nothing falls through a removed slab and counts as house damage.
- **The build hitch.** Interior collision cooked synchronously, once per material section. It now cooks once, off the
  game thread. Mercer's build went from 265 ms to 17-30 ms.
- **Unique names.** The interior parts get unique names, so a rebuild right after a free cannot overwrite a dying
  component.
- **Lighting:**
  - The pool lights the view's own floor (a slab no longer has lights shining through it from the other floor).
  - Three lights stay on in the nearest building while you stand just outside, so the rooms seen through the doors and
    windows are lit.
  - Players, the house's props, campus props and break debris take the pool (channel 1) indoors and the street outside.
  - Campus doors are lit with their room.
- **The window material:**
  - It reloads after a second visit to the map (a one-shot cache had lost it).
  - The room atlas is read once with explicit gradients (no mip seams at room corners).
  - It is rebuilt with `build_campus_materials.py` (`REBUILD`, this job's own asset).
- **The library:**
  - The east walls moved to X 740 (at 600 they met the outer walls in the middle of three windows).
  - Door openings match the 213 cm leaves, with no slot above the doors.
  - The baseboard no longer runs across the doorways.
  - The troffers no longer float over the stair wells, and the floor-1 troffers sit under the tiles.
  - The plaques are on real walls, and the runner runs down the lobby.
  - The bookcases clear the wainscot, the shelving ranges have end uprights, and the desk clutter and the microfiche
    screen sit where they should.
- **Late joiners.** A campus prop broken before someone joins keeps its kit scale on that client.
- **Homecoming paving.** The gate path and kiosk plaza now have a hidden collision floor at walk height, and the layout
  review samples it. Homecoming leaf piles stay off the plan's walks.
- **The reviews:**
  - The interiors and free-roam reviews delete stale result files.
  - Every review uses an explicit list of the reviews that end a run themselves, so none cuts another off or hangs.
  - The layout review ignores the property-boundary walls, which are on only with `-PropertyBoundary`.
  - The tree check uses each trunk box's real corners.
- **The A/B switch.** `-NoCampusLayout` again rebuilds the old campus exactly (the post and planter bottoms are gated on
  the plan).
- **House side.** The junction's stop bar is back where it was (see Open decisions).

## Review round 3 (Phase 3 code and the smoke tests, verified)

Three read-only reviewers went over the polish actor, the polish content and the smoke test with the wall clocks and the
interior sounds. Each finding was checked against the code, the logs or a capture before it was fixed; 33 of 35 are fixed,
two are noted below.

- **The polish actor.**
  - Its root was movable while every tile is static, so every launch logged 152 "cannot attach" warnings and left the
    tiles unattached. The root is static now (the puffs, flags and sounds are movable children); 0 warnings.
  - The light-pool decals reached 1.2 m up, painting a band on lamp posts, booths, benches and legs. They now reach 60 cm,
    and the polish's furniture does not take decals (the lawns and the ground detail still do).
  - The six outdoor sound loops were synthesised on the game thread in the first frame. They are now made on a worker
    thread, and the soundscape starts when they are ready. The interiors' four loops take 20 ms (measured), so they stay
    where they are.
  - Negative float to unsigned conversions (undefined behaviour) gave every looped sound the same start on this Mac, so
    both crowds and every TV started in step. Fixed here and in the interiors.
  - A one-shot sound that never played (culled, or no device) was never destroyed; each now has a deadline.
  - A dedicated server built the drawn-only parts (decals, haze, flags, the skyline). It now keeps only what collides and
    does not tick.
  - The warm window pools fell under doors and under the real glass of dark rooms, and the first buildings used up the
    cap. They now fall only under painted lit windows, at most five per building (171 pools became 156).
  - The animation clock wrapped every hour, so the haze and the flags jumped. It now wraps on periods that every rate
    divides.
  - Smaller fixes:
    - no per-frame map building or audio commands;
    - the float's tissue textures are not made when the polish is off;
    - the haze faces this frame's camera, not the last one's.
- **The polish content.**
  - The middle flood tower's ladder and cage ran up through the walk-in press box. They now start on its roof, and the
    exterior's pole is cased as a walnut column inside the box.
  - The tower's steps stopped 50 cm proud of the plinth's 25 cm tiers, with a slot between. A landing level with the
    sill now covers the tiers up to the door.
  - The polish's mortar joints doubled the tower ashlar's own and are gone.
  - The YOU ARE HERE maps were mirror images. With north up, +X runs to the left in Unreal's axes; the maps now read
    true (checked against the in-game top view).
  - The Calder Hall fire escape hung over the exterior's stand-in west entrance, and window wells sat on its pad. The
    walls' dressing now keeps clear of every stand-in entrance as well as the real doors.
  - The lanterns beside the main doors floated 40 cm off the wall. They now have a backplate and an arm.
  - The chimney pots were both built in the same place.
  - Several props were walk-through, overlapped other things or blocked a path:
    - the tailgate lawn chairs and kettle grill are solid now, like the coolers beside them;
    - the flood footings stand only where they leave the routes clear;
    - the bonfire's rope ring is closed except for the crew's gap, and its keep-clear covers the whole ring;
    - the tower plaza's east mum beds and benches no longer stand in the Union's forecourt;
    - no lamp streamer crosses a building.
- **The smoke test, the clocks and the interior sounds.** Several checks could not fail. Now:
  - collision means a cooked body in the physics scene;
  - the floor under each light must be the building's own (its furniture is looked past; the stadium's shell is the
    exterior's);
  - props rest within 3 cm, with none floating, and there are minimums of kit and furniture;
  - every door is checked by name, place and lock state against its spec;
  - the sign must be on one of the building's own parts;
  - its sounds must be playing, and the JSON says when that check was skipped;
  - the clock check needs the tower and every hand on its dial;
  - it fails, rather than hangs, when the interiors are off, on another map, or when it runs out of time;
  - it shares a run correctly with the other reviews.

  The stricter test found two things straight away:
  - five buildings' light traces landed on furniture (now looked past);
  - one clock read 5.5 degrees off. That was a 56-second stall of the whole process, logged when macOS reported a network
    change; the game clock stopped while the wall clock ran on. The check now compares the hands with the tower's time
    at their last update, which must be under two game seconds old.
- **Noted, not changed.**
  - The windows check asks the plan which windows are open. The cut glass itself is checked by eye in the walk's shots.
  - Old results are deleted at the run's first game tick, so a run that dies before that leaves the last result behind.
    Every run here gets a fresh, emptied `-UserDir` (`run_game.sh`), so none is read twice.
  - The clock check cannot see a dial painted mirrored. The walk's clock shots show that the dials read right.

## Review round 4 (the Phase 4 performance changes, verified)

A review workflow (three reviewers with different lenses, every finding checked by its own skeptic) went over the
performance changes. It confirmed 12 findings (7 distinct) and refuted 5. All are fixed:
- **Doors.** The first pass culled inner doors at 45 m and outer ones at 150 m. The game ships with
  `sg.ViewDistanceQuality=1`, which scales every cull distance by 0.6, so those were really 27 m and 90 m.
  - The stadium's main pair counted as an inner door (it is on no plan block), so its doorway in the stair tower showed
    a hole from the green.
  - Outer doors left holes in the facades in the campus-wide views.
  - An inner door could go before the room around it, which you can see through the open floors' glass.

  Now no door is distance-culled. An inner door is drawn only while its building's inside is built on that machine,
  and it keeps its collision. A main pair always counts as outer.
- **Furniture carried outdoors** vanished at about 36 m. A campus prop is now culled only while it is inside a
  building.
- **The conversion to static meshes** made frames of up to 47 ms, sometimes on the frame after the build.
  - Parts are now converted in pieces of at most 12,000 triangles, one piece every 0.2 s and never on the build's
    frame. The longest step is now about 14 ms.
  - The procedural part is hidden only when its last piece is drawn.
  - The queued geometry is moved, not copied.
- **The mesh builder** made one vertex per triangle corner (three times the vertices). Vertices are now shared, as the
  procedural meshes shared them.
- **The A/B timing tool** (`-ExteriorViewToggle`):
  - restoring used the same visibility flag the game uses, so it could show a part the game had hidden; it now uses
    the hidden-in-game flag;
  - at the default sample count it recorded nothing.

  It turned out to be the wrong tool anyway: hiding hundreds of parts makes the renderer rebuild Lumen's cards and the
  GPU scene for many frames. Costs are measured between alternating runs instead.

## Review round 5 (the extras, verified)

Three reviewers went over the house-side fixes, the town and the walk-in tower, each finding checked by its own skeptic. It
confirmed 8 findings and refuted 6. All are fixed:
- **The near ridge was wound face-down**, so its fall-forest face was culled from the campus. The "lower ridge" in the
  first town shots was really the ridges behind it. It now faces up.
- **The open tower's base had no inner faces.** With a door left open after its inside was freed, or for a distant client,
  the doorway showed the campus straight through the back-face-culled walls. The open shell now has its own dark inner
  faces behind the lining, and the tower's doors are always relevant to clients.
- **Town details:**
  - the river lay under the valley's grass; it now sits just above it, with stone abutments for the bridge;
  - one slope house stood on a water tower leg; that house is dropped and the rest of the town is unchanged;
  - the canopy's "seamless" blend was mirrored, which left hard seams every 40 m;
  - beyond the lawn's edge, the ridge's foot, the river and the trees stood over the void; the valley floor now runs out
    to the ridge's ends;
  - the pines were trunkless cones hovering over the ground.
- **The house-side junction view** stood a metre behind a utility pole; it is moved onto the near sidewalk.

## The house's own room smoke tests

All 16 were run with the campus on and with it off (`-NoCampusLayout -NoCampusFall`); `tools/house_smokes.sh`, runs
`p6z` and `p8z` (after the extras, which touched the house's door class and its backyard; the same results). Every one gives the same result both ways: the campus changes none of them.

| Smoke test | Campus on | Campus off | Note |
|---|---|---|---|
| `Room101EntrySmoke` | PASS | PASS | |
| `SpaRoomSmoke` | PASS | PASS | Failed once with the campus on ("wave solver failed damping or bounds"); then passed six reruns, three each way. Its own solver is flaky. |
| `BoywickSmoke` | PASS | PASS | |
| `HouseTelevisionSmoke` | PASS | PASS | |
| `HouseHotTubWaterSmoke` | PASS | PASS | |
| `HouseVentSmoke` | PASS | PASS | |
| `HouseKitchenSmoke` | FAIL | FAIL | "Two additional serving carts or corrected appliance positions are missing" |
| `HouseLibrarySmoke` | FAIL | FAIL | "Door input did not start the opening" |
| `ExistingHouseDoorSmoke` | FAIL | FAIL | "An extracted original leaf asset is missing" |
| `NightClubSmoke` | FAIL | FAIL | "The wrap layout must retain the original upstairs room..." (run with `-muteaudio`; with `-nosound` it stops earlier, at the audio check) |
| `NightClubEntranceSmoke` | FAIL | FAIL | "Opened entrance remains blocked" |
| `NoodleWingSmoke` | FAIL | FAIL | "Closed doorway blocking failed" |
| `HouseGlassSmoke` | FAIL | FAIL | "Repeated equipped weapon strikes did not shatter native porch glass" |
| `HousePokerSmoke` | FAIL | FAIL | No poker table in the installed map |
| `HouseImpossibleHallwaySmoke` | FAIL | FAIL | "Expected exactly one config" |
| `HouseChairSmoke` | FAIL | FAIL | No `SSORoom102Chair` actors in the map |

The failures date from before this job. These tests were written 4-8 October; the house's map, materials and
`ExistingHouseDoor` changed on 11 October, before the job began, and this job touches none of them. The step checks also
run the newer house reviews (great room collision, rear porch, club hall).

## After Phase 4: the extras you approved (2026-10-14)

You said to go ahead with the four house-side items, the clock tower stair and the town skyline.

### The house's side of Fraternity Park Dr

Each fix was planned by a scout and checked by a second agent before it was applied. Shots are in [shots/5](shots/5).
- **The wall-gap void at the backyard court.** The captures and traces show soil there in today's build. Your
  screenshot came from a build before the 11 October terrain cut. The fixes make sure it cannot come back:
  - the bank lawn now starts 2 cm under the court wall instead of 22 cm short of it;
  - the concrete strips inside the court run under the walls' inner face, closing a 2 cm slit;
  - the kitchen-gate pad meets the porch block, closing a 36 x 8 cm hole.

  Five new probe routes along those seams find 0 gaps, and `-HouseCleanupWalkReview` passes ("both court soil seams").
- **The WALK / DON'T WALK head on the house side.** The new crossing now has a pole on the house kerb, with its
  lettered head facing the campus side and a push button.
  - It stands 1.9 m clear of the walk and off the curb ramp; free roam still crosses 3/3 and walks the campus route
    20/20.
  - The font has no apostrophe, so both heads now draw it themselves and read DON'T WALK, not DON T.
- **The street-name blades at the junction.** Both blades now read the right way: CALDER DR from Fraternity Park Dr,
  FRATERNITY PARK DR from Calder Dr, each double-sided like real blades. With `-NoCampusLayout` the original pair is
  back unchanged.
- **The stop bar.** Fraternity Park Dr ends at this junction, and the through road runs north-south. So the eastbound
  approach (the house's half) needs the bar, beside the house-corner STOP sign. It is moved there, in every mode.
  - It was a bug in the house's street.
  - The westbound lane leaves the junction and has no bar.

| Before | After |
|---|---|
| ![](shots/5/before_house_junction_high.jpg) | ![](shots/5/after_house_stopbar.jpg) |
| ![](shots/5/before_court_west_slit.jpg) | ![](shots/5/after_court_west_slit.jpg) |
| ![](shots/5/after_house_pedpole.jpg) | ![](shots/5/after_house_junction_blade.jpg) |

### The town of Calder (the skyline)

`HouseCampusPolishTown.cpp`, a small mountain college town beyond the south hedgerow, in place of the twelve random
far blocks. With `-NoCampusPolish` those blocks are back.
- **The mountains.** Three ridgelines behind the town. The near one is wooded in October colour (rust, gold, red,
  evergreen; its own painted canopy, smooth-shaded). The two behind go blue in the haze, as the valley's north side
  already did.
- **The valley floor:**
  - Main Street's brick fronts with their shop windows lit;
  - the Calder Inn with its name on the roof frame;
  - the courthouse on its square with a copper dome;
  - a white steeple and a brick church tower;
  - the river with a steel truss bridge;
  - the old mill on the bank, with its stack smoking.
- **Up the slope:**
  - frame houses step up the lower slope, with smoke from a few chimneys;
  - fall trees among the houses and thick on the slope;
  - the CALDER water tower on a knoll;
  - the radio station's mast on the ridge, with red beacons (glow only);
  - the fire lookout on the highest peak.
- **What it costs.** It is all one Skyline part: 12 static meshes, 60 sections, about 105,000 triangles, no lights.
  It adds 31 smoke puffs (117 in all).

| | |
|---|---|
| ![](shots/5/town_town_high.jpg) | ![](shots/5/town_layout_gateway.jpg) |
| ![](shots/5/town_town_view.jpg) | ![](shots/5/town_layout_aerial.jpg) |

### The clock tower stair

The clock tower is now a walk-in building ("TWR", `HouseCampusInteriorsTower.cpp`), free-standing like the stadium, so it
uses the interiors framework: replicated doors, the light pool of 6, built near you and freed when you leave, the indoor
test, the walk and the smoke test. The design is the brief's (`briefs/stadium_tower_interiors.md` §9). A workflow mapped
the code, drafted it, and a second agent checked the draft before it was applied. That check fixed six things:
- a rail that would have shown in the belfry review shot;
- a self-crossing outline on the top flight;
- a hole above the door, seen from inside;
- glowing lancet recesses;
- furniture overlaps;
- a lighting change that would have reached the stadium.

What it has:
- **The door.** A pair of plank oak leaves in the tower's existing doorway (a new `Plank` door style). Closed, they carry
  the old door's boards, straps and rings, so the outside reads as before.
- **The stair.** An open-well oak stair from the door to the belfry floor: 11 flights of 13 risers of 19 cm, with 95 x 95
  landings, balusters and a handrail along the well.
  - The course under the belfry is thinned over the last landing, giving 228 cm of headroom.
  - The top flight climbs through a slot in the belfry floor, its trap lid standing open against the wall.
- **Inside.** Brick linings with the lancets' glass seen from inside, a flagstone floor, caged bulbs (emission) and a wall
  clock on the tower's time. A store under the first flight: worktable, notice board, coffee chest, salt sacks for the
  winter steps, 21 of the house's props.
- **Safety.** Invisible, colliding belfry guards on every arch, the walls, the frame posts, the tie beams (crouch under
  them), the bells and the slot, so nobody can fall out or down the well.
- **The bells.** In the belfry you are outside the indoor test, so the chimes ring at full strength.

How it keeps the tower as it was:
- Every change to the tower's own mesh is gated: with `-NoCampusInteriors`, `-NoCampusLayout` or `-NoClockTower` the
  closed tower is back exactly.
- The tower actor gains no parts, no colliders and no lights; every new collider is on the interiors actor.
- `-ClockTowerReview` passes: 1 colliding part, 0 tower lights, 53 exterior lights.
- Comparing the six tower review views open and closed, the only differences are the clock hands (time passed) and the
  lit windows of other buildings.

Checks:
- `-CampusInteriorsSmoke=TWR`: PASS (19,022 triangles, 21 props, 0 floating, 11 of 11 lights over floor, 2 doors).
- `-CampusInteriorsReview=TWR`: PASS, 31/31 stops. The walk goes in at the door, up all eleven flights, through the slot
  to the hour bell, and back down and out; indoor test 26/26, freed after leaving.

| | |
|---|---|
| ![](shots/5/tower_stair.jpg) | ![](shots/5/tower_bells_inside.jpg) |
| ![](shots/5/tower_TWR_2_Inside.jpg) | ![](shots/5/tower_TWR_4_Belfry.jpg) |

## Process notes

- **Lost scratch.** The session's /tmp scratch (with the pre-job backups) was wiped by a restart. The pre-job snapshot
  of every touched file was rebuilt from the session transcript and checked by replaying the job's edits forward to an
  exact match. It is kept, with the tools and all runs, in `Saved/CampusMasterWork/` (git-ignored).
- **Metal toolchain.** Headless Metal runs "hung" because a stale xcrun cache made the engine's Metal-compiler check fail
  and open a hidden dialog. `xcrun --kill-cache` fixed it.

## Open items carried forward

- **The library's raised terrace:** not built. The forecourt meets the door sill with a 2 cm step, which works.
- **Performance** (Phase 4 has the numbers):
  - the library forecourt (+1.5 ms) and the aerial view (+2.5 ms) are still over the 1 ms target;
  - the polish (with the town) takes about 1.5 s to build at load;
  - building an inside costs one frame of up to about 75 ms when someone comes within 35 m.
- **The clock room above the bells** (the brief's phase B) is not built. It would need a ladder through the bell frame.
- Done since the first report: the clock tower stair, the town skyline (with its water tower), and the four house-side
  items.
