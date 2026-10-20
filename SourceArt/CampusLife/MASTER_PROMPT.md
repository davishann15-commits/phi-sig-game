# MASTER PROMPT: Calder State, alive: the campus and town after homecoming, dusk into night

## Context
Senior Sendoff is a UE 5.8 game (`~/Documents/Unreal Projects/SeniorSendoff`). The whole world outside the frat house is
built at runtime in C++. It is **Saturday, October 18 1986**, after the homecoming win (Calder State Fighting Geese 24,
Dunmore 17). The tone is **horror, mystery and 1986 nostalgia**.

**The campus today:** detailed buildings, the hills, the stadium and the town are built, but it feels like a model: nobody
lives there, nothing moves, nothing is worn.

**The goal:** make Calder feel lived-in and alive, and use that life for the horror. The planned sky job
(`SourceArt/NightSky/MASTER_PROMPT.md`) runs each session from early dusk to full night. **Life drains away with the
light.**
- **At dusk:** stragglers from the game, cars leaving, music from dorm windows, lights coming on.
- **By full night:** the campus is nearly empty, with a few lit windows, a parked security car with its engine running and
  nobody in it, and a single figure under a far lamp who is gone when you look again.

The bar is real photographs and real memories of a small 1986 college town on a fall Saturday night.

**Read first:**
- `SourceArt/CalderLandscape/CODEX_HANDOFF.md`: the machinery (build, run, test, capture on this Mac) and the owner's
  rules. They all apply here.
- `SourceArt/CalderLandscape/MASTER_PROMPT.md` Phase 3 (the wildlife design and its clock).
- The NightSky and StadiumDetail prompts in `SourceArt/`, if they have run.
- The layout's walk and road network: `HouseCampusLayout` (walks, roads, lots, entrances), which is how people and cars
  find their way.

## Build all of this (invented names only; nothing after 1986; no real brands, songs or people)
**1. People** (the heart of "alive").
- **Pedestrians:** 10–25 at dusk, thinning to 0–3 by full night.
  - **Groups:** students in twos and threes heading home from the game, an alum couple, a band member carrying a case, a
    custodian with a cart, a jogger on the hill road.
  - **1986 clothes:** letterman jackets, purple and gold, jean jackets, flannel, big hair, a homecoming mum pinned to a
    jacket.
  - **Source:** free assets only (Fab free listings the owner adds, CC0 characters, or the project's own). Realism
    matters, but they are seen at 15 m and more in dusk light, so silhouette, gait and clothing read first.
- **Movement:** they walk the real walk network (no navmesh), stop at crossings, cluster at a building entrance, sit on a
  bench, stand talking under a lamp, and go into buildings (fade at the door; no new rooms).
- **Behaviour near players:** never closer than ~12 m. They turn a corner or go inside as a player approaches. They never
  block a route and are non-colliding.
- **Never** in the frat house lot, the driveway, the court, the backyard, the Fall Jam field or the house parking lot. They
  may use the far sidewalk of Fraternity Park Dr.
- **Ambient voices:** low laughter and murmured talk from groups at dusk, words never clear. The quiet as they go is part
  of the effect.

**2. Cars.**
- **Traffic:** a few 1986 cars (the project's classic car models) on the campus and town roads. They follow the road
  splines, stop at the stop signs and the light, slow for players, and leave town.
- **Lights and sound:** headlights and taillights are emission only. A car stereo's muffled bass Dopplers past.
- **The security car:** cruises a slow loop at dusk, then sits parked at night, engine idling, lights on, empty.
- **Thinning:** traffic thins with the sky. By night, one car every few minutes, then none.

**3. Windows and buildings.**
- **The dorm windows' evening:** lights come on through dusk in a believable order, then thin out late. All emission.
  - **What shows:** varied curtains and blinds, posters, a box fan, a TV's blue flicker, a lava lamp.
  - **The spirit banner:** a "WELCOME BACK ALUMNI" bedsheet hanging from a sill.
  - **The figure:** one silhouette behind a shade that is there, and then isn't.
- **Office and lab windows:** mostly dark, one janitor's light moving floor to floor.

**4. Homecoming aftermath and lived-in detail.**
- **Spirit:** toilet-papered trees moving in the wind, chalk on the walks ("BEAT DUNMORE", "GO GEESE"), and a painted
  spirit rock thick with layers of paint.
- **Leftovers:** float scraps (chicken wire, tissue-paper pomps) in a lot, a deflated purple balloon in a bush, cups and
  programs blowing.
- **Things people left:** 10-speed bikes at racks (one missing a wheel), a backpack forgotten on a bench, a frisbee on a
  roof, a parking ticket on a windshield, rakes on a half-raked leaf pile.
- **Halloween is two weeks off:** pumpkins on porches and dorm sills, corn stalks tied to lamp posts.

**5. Infrastructure and wear.**
- **Steam:** rising from manholes and grates and from the Physical Plant chimney. Use cheap animated mesh or cards with
  depth fade; no Niagara.
- **Works and repairs:** a dug-up steam line behind sawhorses with blinking amber barricade lamps (emission), and
  scaffolding with a flapping tarp on one building.
- **Signs:** a faded "YOU ARE HERE" campus map, "FACULTY PARKING / TOW-AWAY ZONE" signs, and a bulletin-board kiosk layered
  with flyers (rides home, a lost cat, a rush party, a typing service, a band night).
- **Wear:** leaves in gutters and against curbs, puddles that reflect the lamps, cracked pavement with weeds, ivy and moss
  on north brick, stains under sills, gum on the busy walks.

**6. The town edge (nostalgia):** a diner with a neon OPEN sign, a glowing laundromat, a gas station with 1986 prices
(about 89¢), a church with lit stained glass, and the freight train in the valley (see sound).

**7. Sound** (a layered bed that follows the clock).
- **The freight train:** a long horn from the valley, the rumble growing and fading, its headlight sweeping the bluff
  (emission).
- **Music:** muffled from a dorm window. It must be **original or public-domain**, never a real 1986 song.
- **Town and campus sounds:** a distant car radio, a slammed door, a chapel bell on the hour, sodium lamps buzzing,
  building fans, a flag halyard clanking.
- **Over the evening,** voices and traffic fade out and the night sounds take over. Leave a hook for the wildlife job's
  insects and owls.

## Hard constraints
- Everything in `CODEX_HANDOFF.md` "The owner's rules" and "Hard constraints":
  - hands off the frat house, court, Fall Jam field and parking lot;
  - tree crc `0f3b2bb7`;
  - runtime-built only; no Nanite, Landscape, Niagara, PCG, Mass or Water modules; no navmesh; no editor-only APIs;
  - Unity-build safe and `-Wshadow` clean; commit nothing.
- **Exactly 53 exterior point/spot lights.** Every window, headlight, neon sign, barricade lamp and train light is emission.
- **No story hooks:**
  - People and cars are set dressing: no dialogue, no interaction, no triggers, no checkpoints.
  - Don't touch `StoryCampaign` or the player's movement.
  - No new rooms or interiors.
- **Walkability:**
  - nothing new blocks a walk, stair, ramp, door or route;
  - the layout, reach, FreeRoam and interiors reviews stay exactly as strict;
  - people and moving cars are non-colliding with players, or stop for them (cars).
- **Multiplayer:**
  - Server-time driven and seeded, so every player sees the same people, cars and windows at the same moments.
  - Decide per system what replicates and what is deterministic locally (like the wildlife design), and keep the
    bandwidth small.
  - Prove it in the two-player test.
- **Performance:** the frame is render-thread bound.
  - **Cost budget:** +1.0 ms in the street and campus views (median of 3 alternating rounds, `-MetalOffscreenOnly`), and
    ≤ 0.3 ms p95 game thread for people and cars together.
  - **How:** instance everything repeated, cull at distance, animation budget (update-rate optimisation), and at most
    ~25 animated characters at once.
- **A/B:** `-NoCampusLife` gives back today's exact state (prove it with the review numbers and the crc). Use separate
  switches for people, cars, windows and sound so each can be timed.
- **Names:** new components start with `CalderLife`. Never start a name with `Exterior*` or `CampusFallTree*`. New views
  are `detail_life_*`, appended at the end of `ExViews`.
- **Assets:** free only (CC0, or free Fab listings the owner adds; give links, never sign in). Music and voices must be
  original or public-domain; record their sources in `README.md`.

## Process
1. **Plan.** Survey the walk and road graph, the existing cars, characters and window materials, and the clock (use the
   sky job's `CalderSky::NowMinutes()` if it exists, else add one shared clock that it can adopt). Collect reference photos
   of small college towns on a fall evening in the mid-80s. Write `SourceArt/CampusLife/PLAN.md`: the per-system design,
   the population curve over the evening, the replication plan, the perf plan and the asset list.
2. **Build in steps.**
   - **Order:** the shared clock and the windows first, then sound, then the aftermath and wear, then steam and the
     infrastructure, then cars, then people, then the town edge.
   - **After each step:**
     - build;
     - capture dusk and night pairs (`-SkyTime=` if the sky job exists) of new `detail_life_*` views at 1.7 m eye height,
       and **look at them** next to the reference photos;
     - run the review set (`Saved/CalderWork/tools/fix_checks.sh <tag>`);
     - write a short STATUS entry.
3. **Add a `-CampusLifeReview`.** It runs the evening at high speed and checks:
   - the population curve;
   - no person or car in a forbidden zone, a building or a wall;
   - no one within 12 m of the test pawn;
   - cars stopping for the pawn;
   - 53 lights, and the frame and game-thread budgets;
   - the same sequence on two runs with the same seed.
4. **Finish:** every review in `CODEX_HANDOFF.md`'s list, the two-player test, a frame-time A/B, one `-DisableUnity` build,
   `SourceArt/CampusLife/README.md`, and STATUS and `CODEX_HANDOFF.md` "Current state" updated.

**Stop and ask only for:** more than 53 lights; anything in the frat house area; changing a review threshold or route;
NPCs that talk, interact or tie into the story; or a paid asset.
