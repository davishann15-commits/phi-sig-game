# MASTER PROMPT: Calder State Stadium, made real. Homecoming '86

## Context
Senior Sendoff is a UE 5.8 game (`~/Documents/Unreal Projects/SeniorSendoff`). The whole world outside the frat house is
built at runtime in C++. It is **Saturday, October 18 1986, at dusk, just after homecoming**: the Calder State Fighting Geese
(purple and gold, goose mascot; invented) beat Dunmore 24–17. The tone of the game is **horror, mystery and 1986 nostalgia**.

**The stadium today reads as a blockout.** See `Saved/CalderWork/runs/w1_shots/Saved/HouseExteriorViews/108_*`–`111_*`,
`220_*` and `221_*`.
- **The stands:** flat grey concrete tiers with smooth purple and gold strips for benches. There are no seat planks, aisle
  steps, handrails, section signs, row numbers, drainage, wear or stains.
- **The vomitory:** one plain box.
- **The press box:** a flat panel with roundels.
- **The field:** clean lines and plain goalposts, with almost nothing on the sidelines.
- **The rest:** the scoreboard, light masts, gate and concourse are simple shapes.

**The job:** make it look and feel like a real small college stadium of 1986, empty an hour after the homecoming game, with
the lights still on. The quality bar is **real photographs** of such places (judge your screenshots next to photos you
find; describe the comparison in STATUS). Up close at 1–2 m it must hold up, not only from far away.

**Read first:**
1. `SourceArt/CalderLandscape/CODEX_HANDOFF.md`: the machinery (how to build, run, test and capture on this Mac) and the
   owner's rules. They all apply here.
2. `SourceArt/CampusFall/README.md`: how the homecoming stadium was made.
3. The stadium code: `HouseCampusFallStadium.cpp`, `HouseCampusInteriorsStadium.cpp` (concourse, locker room, tunnel,
   vomitory), `HouseCampusPolishStadium.cpp`. `grep -n Stadium` across `Source/SeniorSendoff` for where the bowl, stands,
   masts, gate and scoreboard are built and which reviews measure them.

## What "real" means here (build all of it; invented names only, nothing after 1986, no real brands)
- **Seating:**
  - **Benches:** aluminium bench planks on steel stanchions bolted to the concrete treads, or weathered painted wood on the
    older end sections. Seat numbers stencilled on the planks; row letters painted on the risers.
  - **Aisles:** aisle steps with painted nosings, centre pipe handrails, and section numbers on painted boards ("SEC 12") at
    every aisle and portal.
  - **Rails and walls:** front-row walls with pipe guard rails, and rails along the top.
  - **Concrete:** expansion joints, drain gutters with grates, rust weeping from bolts, water stains, chewing-gum spots.
  - **The student section:** a slightly different, more beaten-up section.
- **Vomitories:** several portals round the bowl, each with a section board, a pipe rail round the opening and concrete
  cheeks. The existing walkable one (segment 11, the STA interior route) stays exactly walkable.
- **Press box:** a real two- or three-level box: angled glass, radio and coaching booths with silhouettes of equipment, an
  open camera platform on the roof with a rail and ladder, a PA horn cluster, flagpoles with flags and clanking halyards,
  antenna masts, painted steel siding with rust, "CALDER STATE" lettering.
- **Scoreboard:** a 1986 incandescent-bulb board.
  - **Layout:** HOME / GUESTS, QTR, DOWN, TO GO, BALL ON, TIME OUTS, and the game clock. The final score, 24–17, is lit.
  - **Light:** bulbs by emission, with a few dead or flickering bulbs.
  - **Panels:** invented local sponsor panels (a hardware store, a diner, a bank with made-up names), and a goose logo.
  - **Structure:** steel framing behind, with catwalk and ladder.
- **Light towers:** lattice steel masts with banks of fixtures. Fixture glow is emission. One fixture is dead and one
  flickers subtly (emission only). Add caged ladders and catwalks.
- **The field:**
  - **Turf:** worn natural grass, with dirt between the hashes, divots, chalk lines slightly scuffed, the painted midfield
    goose and end-zone lettering.
  - **Hashes and markers:** college hash marks, yard numbers with arrows, orange pylons.
  - **Goalposts:** 1986 single-post "slingshot" goalposts with padded bases.
  - **Surroundings:** a cinder track or a grass apron round the field.
- **Sidelines, an hour after the game:**
  - aluminium team benches, the down-and-distance chains and box lying on the ground;
  - water coolers (generic orange), paper cups, tape balls and a coiled headset cable;
  - a rolled tarp, field-maintenance cart, a chalk liner, band risers and the band's empty folding chairs;
  - a dropped trombone case, the homecoming float frame left by the end zone, crepe-paper and confetti.
- **Concourse, gates and outside:**
  - concession stands with shutters half down, hand-painted 1986 price boards (HOT DOG 75¢), restrooms (signs only, no new
    rooms), souvenir stand with pennants;
  - turnstiles and ticket windows at the gate, queue rails, trash barrels overflowing with cups and programs;
  - a dedication plaque ("ERECTED 1931", invented), the Fighting Geese hall-of-fame display case, bulletin boards with
    flyers.
- **Interiors that exist (STA tour):** the concourse, locker room, tunnel and vomitory landing get the same pass.
  - **Locker room:** wooden lockers with nameplates, a chalkboard with a play drawn on it, tape, helmets, a trainer's
    table, a whirlpool tub, a wet floor.
  - **Tunnel:** padded walls painted purple, a "BEAT DUNMORE" banner torn on the floor.
  - Same doors, same routes, same stops.
- **Mood (restrained; set dressing only, no story hooks, triggers or checkpoints):**
  - the lights hum (a sound with a source);
  - one bank of bulbs dying, flags and halyards ticking in the wind, programs blowing across the empty rows;
  - a single seat cushion left in the top row;
  - the PA horn's faint carrier hum;
  - far off, very low, the ghost of a marching-band snare that might be the town (a sound with no visible source, rare).
  - It should feel like everyone left at once.

## Hard constraints
- **The rest of the world:**
  - Everything in `CODEX_HANDOFF.md` "The owner's rules" and "Hard constraints" applies: hands off the frat house, court,
    Fall Jam field and parking lot; tree crc `0f3b2bb7`; runtime-built only; no Nanite, Landscape, Niagara, PCG or Water
    modules; no editor-only APIs; Unity-build safe; `-Wshadow`; commit nothing.
- **Lights:** **still exactly 53 exterior lights.** The stadium's 3 point lights stay 3. Every new glow (fixtures, bulbs,
  scoreboard, booths) is emission.
- **The stadium stays where it is:** its XY, the turf Z (-56) and its four Z constants, the mast positions, the apron ring,
  Stadium Dr, the bus bay and the loop. The Calder terrain was built around them. The tower's sight line stays clear.
- **Walkability:**
  - everything reachable collides;
  - stair risers 14–18 cm, treads ≥ 30 cm;
  - a rail ≥ 90 cm at any drop ≥ 75 cm;
  - no traps;
  - the existing routes (the STA interior tour, FreeRoam's ticket-plaza leg, the layout review's walks) stay walkable;
  - props on walkways must not block them (or must be on the review's allowed list for a reason you document).
- **Performance:** the frame is render-thread bound.
  - **Draw calls:** instance everything repeated (seat planks, stanchions, rails, litter) with HISMs, and merge static
    parts per material.
  - **Cost control:** set cull distances and LODs, and no shadows on small props.
  - **Budget:** the stadium views (`detail_stadium_*`, P2, P3, `detail_layout_aerial`) may cost at most +1.5 ms (median of 3
    alternating rounds, `-MetalOffscreenOnly`), and load time at most +1 s.
- **A/B:** everything new sits behind `-NoStadiumDetail`, which must give back today's state exactly. Prove it with the
  review numbers and the crc.
- **Names:** new components start with `CalderStadium`. Never start a name with `Exterior*` or `CampusFallTree*`. New views
  are `detail_stadium_*`, appended at the end of `ExViews`.
- **Assets:** free only (CC0 from Poly Haven / ambientCG, or free Fab listings the owner adds). List anything you want the
  owner to add from Fab with links; never sign in. Textures or code-painted materials for aged concrete, galvanised and
  painted steel, aluminium, chipped paint, rust, worn turf, chalk.
- **Don't loosen any review** (threshold, tolerance, sample count, timeout, waypoint). If a review counts something that
  legitimately grows (e.g. `CAMPUS_WOODS_REVIEW stadiumSeatVertices=`), re-base its expectation openly and say why in
  STATUS.

## Process
1. **Survey.** Read the stadium code and its reviews. Capture today's views. Gather 10+ reference photo descriptions of
   real 1980s small college stadiums (stands, press box, scoreboard, sidelines, concourse), and write
   `SourceArt/StadiumDetail/PLAN.md`: what to build, where in code, how it is gated, the perf plan and the review impact.
2. **Build in steps.** Do the stands and aisles first, then the press box and scoreboard, then the masts, then the field and
   sidelines, then the concourse and gates, then the interiors, then the mood and sound. After each step:
   - build;
   - run the stadium views and new close-up views at 1.7 m eye height and **look at them** next to the reference photos;
   - run the review set (`Saved/CalderWork/tools/fix_checks.sh <tag>`, which includes the STA interior tour, FreeRoam, the
     layout review and the woods set);
   - write a short STATUS entry.
3. **Close-up views to add** (`detail_stadium_*`): an aisle looking up the stands, a seat row at knee height, the front
   rail looking onto the field, the press box from the 50-yard line, the scoreboard from the end zone, the sideline bench
   with the chains, a vomitory from the concourse, a concession stand, the turnstiles, the locker room, the tunnel.
4. **Finish:**
   - Run every review in `CODEX_HANDOFF.md`'s list.
   - Do the `-NoStadiumDetail` A/B for exactness, a frame-time A/B and one `-DisableUnity` build.
   - Write `SourceArt/StadiumDetail/README.md` covering what was built, the switches, the asset list and the numbers.
   - Update `STATUS.md`, `CODEX_HANDOFF.md` "Current state" and `SourceArt/CampusFall/README.md`.

**Stop and ask only if it needs:** more than 53 lights; moving the stadium, the masts or the turf; touching the frat house
area; changing a review threshold or route; or a paid asset.
