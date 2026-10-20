# Calder State, made real: master plan (2026-10-13)

The job is to make the campus across Fraternity Park Dr hold up now that the player can walk there (the invisible property
boundary is off). It has three phases: (1) layout, (2) enterable buildings modelled on the house's interiors,
(3) eye-level exterior polish. The setting doesn't change: Calder State Fighting Geese, purple and gold, homecoming,
Saturday October 18 1986 at dusk, Geese 24 Dunmore 17.

- Progress lives in [STATUS.md](STATUS.md).
- The six survey briefs behind this plan are in [briefs/](briefs/): campus geometry, review invariants, house interiors,
  rendering tech, stadium and tower interiors, and the wall-gap bug.
- The plan tools are in [plan/](plan/): `layout.py` is the plan as data with a checker and renderer, `treesim.py` replays
  the tree RNG, and `plandraw.py` and `floorplans.py` draw the plans.

## 1. Today

| Top-down capture (new `detail_plan_campus` ortho view, unlit) | The same, drawn from the code |
|---|---|
| ![today](plan/today_topdown_campus.png) | ![today](plan/today_plan_from_code.png) |

All plans use the engine's top view: +X right and +Y down, so the campus sits at the top, as seen from the house. Every
reported layout bug is confirmed from the code, with exact numbers in `briefs/campus_geometry.md` §15-16.

## 2. The new site plan (v1)

![plan](plan/new_site_plan_v1.png)

`plan/layout.py` holds this plan as data. Its checker reports 0 conflicts and 0 hidden trees. It checks against:

- the stadium wall plus 4 m, and the tower plinth plus 3 m;
- every review camera and the free-roam walk corridor;
- the homecoming dressing;
- the clock tower sight line, with the height limit 140 + 0.287·d;
- the ExteriorNear rule (Y < -5550);
- the 97 campus trees.

**Roads.**
- **Fraternity Park Dr** is unchanged. It is a fixed anchor.
- **Calder Dr** is the new main entrance. It follows the old side street's line, X 8700..10300, so the junction and the
  road-floor review samples stay the same. It has brick gateposts with a CALDER STATE UNIVERSITY sign, and a planted median
  as far as Y -9600. It runs south past the east cluster, then curves west into **Stadium Dr** along Y -19000. That ends
  in a bus loop at (-1000,-19300), so no campus road reaches the world edge.
- The old side street south of the junction is replaced by grass. Its stretch north of the junction (the house side) is
  untouched.
- The four side-street utility poles stand on Calder Dr's west verge and are left exactly where they are, so their lights
  are unchanged.

**Plan.**
- **Main quad.** Its axis is Y -10800: Mercer Library's east steps at the west head, and the clock tower at the east
  end, with the tower plaza beyond it.
- **Front row on Fraternity Park Dr:** Old Main and Harlow Hall (dorm).
- **Quad's south side:** Hollis Science Hall.
- **Between the tower plaza and the stadium:** Goose Union.
- **Dorm cluster east of Calder Dr:** Ashby Hall and The Commons (dining hall), with Kerr Hall and the Varsity Fieldhouse
  behind.
- **The wooded south:** Whitcomb Fine Arts, the Band and Music Hall, Pratt Engineering, Alumni Hall and the Physical
  Plant, each in a clearing between the existing trees.

**Parking.**
- **South Lot** (X -1400..8300, Y -25200..-20700): five tree islands built around existing trees, an entry off Stadium Dr,
  and a bus drop-off bay north of Stadium Dr at the stadium's south gate.
- **Health Visitor Lot:** off the existing drive mouth at X -5000..-4400.
- **Ashby Permit Lot.**
- The 21 cars of the old health lot keep their random draws, so the trees are unchanged. Their transforms are moved
  afterwards: 8 into the visitor lot and 13 into the South Lot. The three lot poles move with their lots, so the light
  count stays at 53.

**Walks.**
- A front promenade at Y -6500, so the street-row trees and lamps sit on a verge.
- The quad axis and its diagonals, plus a walk from every entrance to the network.
- Sidewalks along Calder Dr, the gate path, and walks to the stadium's east, west and south gates.
- Walks never pass under the stadium or through a building.

**Edges.**
- The campus-side runs of the hedge ring (the Y -18800 run and the campus parts of the X ±18800 runs) are removed. The
  house-side runs stay.
- A visible fence and hedgerow at Y -29800 stops people walking onto the non-colliding Far lawn.
- Far blocks F3 and F7 move off walkable ground.

**Topography (Phase 1c).** `Grade()` is not touched, because tree Z is part of the fingerprint. Relief comes from separate
meshes instead: a stadium berm collar with steps at the gates, the library's raised terrace and steps, a sunken centre
panel in the quad, and low retaining walls. All of it stays walkable.

### How each known bug is fixed

| Bug | Fix |
|---|---|
| Side street through blocks 3/11/13 | Those blocks move; the street becomes Calder Dr and ends in a loop |
| Utility poles inside campus | The poles stay; the buildings move off their line |
| Health lot under block 1 | The lot is removed; the visitor lot and the South Lot replace it, and the cars are remapped |
| Walks through buildings and under the stands | A new walk network that the checker keeps clear |
| Lamps under the stands | The lamp list is regenerated along the new walks |
| Trees inside buildings | Hidden at instancing time, never moved. Plan v1 hides none: the buildings that held trees have moved |
| No stadium access | Phase 1 adds walks to the gates; Phase 2 adds the concourse and cuts the gates open |
| All entrances face +Y | Each building gets its own entrance faces (N/S/E/W), built in a canonical frame and rotated |
| No quad or axis | The main quad on Y -10800 ends at the clock tower |
| Roads to the void | Calder Dr ends in the bus loop; the side street south of the junction is removed |
| Non-colliding Far lawn | A south fence and hedgerow at Y -29800; F3 and F7 move |
| Broken hedge ring | The campus-side runs are removed |
| Flat campus | Relief meshes (berm, terraces, steps), see above |
| Real street name "Pat Head Summitt St" | Renamed CALDER DR on the blade and the new signs |
| The clock tower stands on block 6's entrance pad | Block 6 is now the Goose Union, whose west doors face the tower plaza |
| The mast through the press box, and the floating scoreboard | Phase 3: press box structure and scoreboard legs |
| Wall-gap void at the basketball court (the user's screenshot) | The bank lawn starts at the wall (`HouseBasketballCourt.cpp:150`); also close the 2 cm in-court slits and the gate hole (briefs/wall_gap_bug.md) |
| Kerb snag on Fraternity Park Dr (logged three times) | Curb ramps where campus walks meet the far kerb |

### Revision 2 (built in step 1c, checked in game by `-CampusLayoutReview`)

The plan above is v1. While building it at eye level, the in-game review found real problems, and the plan changed:

- **The street's profile.** Campus roads and lots now sit at street level (-72), the same as Fraternity Park Dr. The lawn
  is cut away under them, so:
  - Calder Dr meets the street flush (v1 had a ramp, half buried under the grass);
  - every road edge that meets lawn gets a 15 cm curb;
  - sidewalks and walks sit at -57, just above the lawn;
  - curb ramps dip to the road at every crosswalk, and walks ramp down across the drives they cross.
- **A campus loop.** A service road runs behind the library and Calder Hall from Fraternity Park Dr. It bends into a
  south lane that serves the South Lot's back drive and the Physical Plant's yard. The Commons gets a loading dock lane.
  No road ends in the open.
- **The front row steps back.** Every main entrance carries a 5.5 m concrete apron with planters and handrail posts. Old
  Main, Harlow, the Health Center, the Commons and Ashby now have their north faces at Y -7100, so all of that stays off
  the promenade at Y -6500.
- **Entrances that faced a wall:**
  - Calder Hall now faces north (its east door looked at Hollis Science 6 m away), with a back door on the west road.
  - Whitcomb Fine Arts moved 4 m south.
  - The Fieldhouse's main door turned north to the Ashby lot.
- **One continuous walk network.**
  - A mid-block crosswalk on the stadium axis joins the frat side to the kiosk.
  - A south walk links Hollis, the arts and music halls and the bus loop.
  - Every one of the 25 entrances is on the network.
- **Other moves.**
  - The visitor drive moved 1.5 m west, clear of a Fraternity Park Dr utility pole.
  - The homecoming kiosk and bike rack moved off the promenade.
  - The quad benches now stand clear of the diagonals.

Still 0 trees hidden, tree crc `0f3b2bb7`, 53 exterior lights. `-NoCampusLayout` gives back the old campus exactly.

## 3. Buildings

Every building is invented; no real school's names are used. Heights are world cm above grade. Tiers follow the master
prompt: A = full interiors over several floors, B = ground floor plus at most one more, C = a lit lobby or vestibule.

| Id | Name | Function | Era and style | Footprint (X / Y) | H | Faces | Tier | Open floors | Signature rooms | Locked and why | Homecoming state |
|---|---|---|---|---|---|---|---|---|---|---|---|
| LIB | Mercer Library | library | 1920s limestone | -19000..-15600 / -13300..-8300 | 2120 | E | **A** | G, 2 | card catalog hall, reference reading room, microfiche, stacks, carrels | 3-5: special collections, by appointment | open till 10; a few coats on chairs |
| MAIN | Old Main | administration | 1890s brick | -15200..-11400 / -9700..-7100 | 2540 | N, S | B | G | registrar counter, a wood-paneled dean's office | upper floors: offices, CLOSED SATURDAY | homecoming court photos on the wall |
| HAR | Harlow Hall | dorm | 1950s limestone | -10600..-6800 / -9450..-7100 | 2540 | N | **A** | G, 2 | lobby and RA desk, TV lounge, laundry, communal bath, 6 distinct rooms | 3-6: quiet hours | lived-in, sheets out, lanterns |
| HLTH | Student Health Center | clinic | 1960s warm brick | -4200..-1800 / -8700..-7100 | 1260 | N, W | C | lobby | waiting room | clinic doors CLOSED | — |
| COM | The Commons | dining hall | 1960s warm brick | 11000..14200 / -10000..-7100 | 1300 | W, N | B | G | serving line, dining room, homecoming buffet aftermath | kitchen | trays left after the tailgate |
| ASH | Ashby Hall | dorm | 1950s brick | 14700..18900 / -10400..-7100 | 2540 | N, W | B | G | lobby, lounge, a few rooms | upper floors | sheets in the windows |
| SCI | Hollis Science Hall | science | 1930s warm brick | -15200..-10600 / -17400..-14200 | 2540 | N | **A** | G, 2 | tiered lecture hall, two teaching labs with fume hoods, specimen case | prep room, offices | equations still on the boards |
| HUM | Calder Hall | humanities | 1910s brick | -19000..-15800 / -18600..-14400 | 2120 | N, W | B | G | seminar rooms, English department lounge | upper floors | — |
| UNI | Goose Union | student union | 1960s limestone | -8200..-4300 / -14600..-10700 | 1700 | W, N, E | **A** | G, 2 | the Goose Pit snack bar, game room (pool, pinball), TV lounge, mailroom, ride board, homecoming committee room, ballroom | offices | half-eaten food, TV on, streamers |
| KER | Kerr Hall | classrooms | 1960s warm brick | 11000..14400 / -15400..-11400 | 2540 | W | B | G | tablet-arm classrooms, faculty corridor | upper floors | — |
| FLD | Varsity Fieldhouse | athletics | 1950s brick | 15000..18900 / -17000..-12200 | 1700 | N, W | C | lobby | trophy lobby | gym doors | — |
| ART | Whitcomb Fine Arts | arts and auditorium | 1970s limestone | -13250..-9050 / -21400..-18400 | 1700 | N | C | lobby | box office lobby | auditorium | a fall play poster |
| MUS | Band and Music Hall | music | 1960s warm brick | -7750..-4150 / -20250..-17450 | 1500 | N, E | C | lobby | instrument lockers glimpse | practice rooms | band uniforms on racks |
| ENG | Pratt Engineering | engineering | 1950s brick | -10000..-6400 / -24750..-21950 | 2120 | N | C | lobby | — | labs | — |
| PLT | Physical Plant | power plant and smokestack | 1920s warm brick | 11750..15950 / -21250..-18250 | 1300 + stack | W | C | — | — | AUTHORIZED PERSONNEL | — |
| ALU | Alumni Hall | alumni house | 1900s limestone | -19000..-15600 / -24500..-21800 | 1500 | N | C | lobby | alumni registration | — | alumni coffee urns |
| — | Calder State Stadium | football | 1930s brick bowl | fixed | — | gates 4/5, team tunnel | **A** | concourse, Geese locker room, press box | concessions, team tunnel, radio booth | Dunmore locker room | after the game |
| — | Clock tower | campanile | fixed | fixed | — | door | stretch | base room, stair to the belfry | bells and dial backs | clock room (padlocked trapdoor) | — |
| — | Frat row A/B/C | neighbours | fixed | fixed | — | — | optional B | G | party aftermath | — | — |

### Tier A floor plans (schematic)

![lib](plan/fp_library_g.png) ![lib2](plan/fp_library_2.png)
![har](plan/fp_harlow_g.png) ![sci](plan/fp_science_g.png) ![uni](plan/fp_union_g.png)

The stadium interior follows `briefs/stadium_tower_interiors.md`:
- the home-side concourse from R .834 to the wall, at A 36..198°, with 555-987 cm of clearance under the stepped soffit;
- restrooms at K=4 and K=14, concessions at K=6-7 and K=12-13;
- vomitories at the 45°, 90° and 135° aisles;
- the Geese locker room at A 162..189 with a team tunnel to the CALDER end zone;
- a press-box stair core under the street-side overhang.

## 4. The house interiors are the model: what is reused and how

This is summarised from `briefs/house_interiors.md`.

**Construction.**
- The house rooms were built in Blender and placed into the saved map. The campus can't save the map, so it copies the
  C++ runtime-room pattern instead: the black hall and hallway detail (`HouseBlackHall*`, `HouseDetail*`).
- A `UCampusInteriorsSubsystem` (Chapter01_House only, `-NoCampusInteriors`) spawns:
  - transient cosmetic actors on every machine;
  - replicated gameplay actors on the server only.
- Large planes use cooked cube instances, so Lumen gets distance fields. `FHallMesh` is used for trim.

**Systems, reused rather than forked.**
- **Doors.** `ACampusDoor` : `AHouseBlackHallDoor`, with a replicated style index. DoorIds are `Campus_<bldg>_<n>`.
  Locked doors use the existing "not playable yet" message and LOCKED HUD.
- **Props.** `ACampusProp` : `AHouseInteractiveProp`, with a replicated `CatalogIndex` that rebuilds the meshes on
  clients. Carryable props use the house carry rules: the `SSOMovableFurniture` tag, simple collision, and mass for the
  size class.
- **Windows.** `AHouseReactiveFixture` windows, owned by their building and tagged `SSOCampusDamage`.
- **Damage.** `HouseDamageSubsystem::RegisterBrokenActor` sends anything tagged `SSOCampusDamage` to its own campus
  counter, so the HUD's HOUSE DAMAGE total is unchanged.

**Assets reused from `Content/SeniorSendOff/House/`** (where they fit 1986): the green spindle chair, bentwood desk chair,
six-shelf bookcase, loft and twin beds, gang-bathroom pieces, cafeteria serving counter, billiards table, arcade cabinet,
and fire and steel doors. The photo-room meshes are stretched 1.8×, so they are scaled 0.556 in XY.

**Built new at runtime** (the project has none): CRT TVs, chalkboards, tablet-arm desks, radiators, lockers, vending
machines, rotary phones, typewriters and card catalogs.

**Workflow.** Each building gets `SourceArt/CampusInteriors/<Building>/` (references, before shots, evidence, a views
json) and a smoke test in the style of `HouseKitchenSmoke`/`HouseLibrarySmoke`, judged by its log marker.

## 5. Lighting

- **Exterior: 53 lights, never more.** Three things move with their objects, and the count is unchanged: the three lot
  poles move with their lots, the four Calder Dr poles stay, and the three stadium lights stay where they are.
- **Glow is emission.** Fake light pools come from a new `M_LightPoolDecal`, an emissive-only deferred decal, under the
  mercury lamps and lit windows.
- **Interiors.**
  - Emission first: troffers, exit signs, CRTs, desk lamps.
  - Interior meshes go on lighting channel 1, so the 53 shadowless exterior lights don't leak in.
  - A pooled interior budget of at most 6 shadowless point lights, on a separate actor not owned by the exterior. Lights
    are assigned to the rooms nearest the local camera and switched on only while it is inside. They are reported
    separately, never as exterior lights.
- **Exposure.** One shared "inside a building" test, min(DistanceOutsideHouse, distance outside the campus interior
  boxes), drives the dusk film-look weight and the clock-bell ducking. Campus rooms then get the house's -4.5 indoor
  exposure.
- **Windows.**
  - Closed floors use a new `M_CampusInteriorWindow`: interior mapping with a painted room atlas, lit or dark per
    `HouseCampusFall::WindowLit`.
  - Both new materials are made headless by `Tools/claude/build_campus_materials.py`, in
    `/Game/SeniorSendOff/House/CampusInterior`, while the editor is closed and Codex is idle. That folder (and
    ExteriorStreet) is added to DirectoriesToAlwaysCook.
  - Fallback: plain glass with shallow window boxes.

## 6. Streaming and performance

- `AHouseCampusInteriors` is transient and not replicated, and exists on every net mode.
- **What it loads.** Its interest set is every known pawn, plus the local view location, plus `-CampusInteriorsAll` for
  captures.
- **When it loads.** A building loads within 3500 cm and unloads beyond 6000 cm after 10 s.
- **How it builds.**
  - Geometry is built on a worker thread.
  - Components are created on the game thread, at most 3 ms per frame.
  - Shells and furniture use `HwBuildStaticMesh` (cached static draws, box collision) and per-building HISMs.
- **Budgets.**
  - Outside: within about 1 ms of today in every street and campus view.
  - Inside a Tier A building: about 60 fps at 1280×720.

A/B switches: `-NoCampusLayout`, `-NoCampusInteriors`, `-NoCampusPolish` (and `-NoCampusFall`).

## 7. Invariants (how they are kept)

- **Tree crc `0f3b2bb7`.** The tree-RNG inputs are frozen as legacy reservations:
  - the old 16 footprints and the stadium rectangle still go to NoGo;
  - the walk bands, the side-street and health-lot NoGo, and all 64 vehicle slots stay byte-identical.
  - The new layout never feeds NoGo.
  - Trees that collide are hidden at batch time (Y < -5550 only), and the hidden count is logged.
  - Every build is first checked offline with `plan/treesim.py --whatif`.
- **CampusWoodsReview.** Every ExteriorNear vertex stays at Y < -5550, and seats stay at Y < -8000. New layout ground is
  its own mesh (`CampusLayoutGround`), not ExteriorNear.
- **Other reviews.** The SurroundingsPolish road-floor samples, the FreeRoamWalk corridors (X 3100, Y -5000..-7000; and
  the junction), the clock-tower sight line and the camera spots are all kept.
- **New views.** New ExViews are appended at the end and named `detail_*`.
- **Multiplayer.** Everything is deterministic and built identically on every machine.

## 8. Order of work

The order is 1a → 1b → 1c → 1d → 2a … 2e → 3a … 3d → 4, as in the master prompt, with screenshots and a short note
after each step in `STATUS.md`. Every step leaves the game building and passing its reviews, so the work can stop and
resume anywhere.
