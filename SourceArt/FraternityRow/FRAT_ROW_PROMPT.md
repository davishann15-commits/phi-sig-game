# Prompt: Fraternity Row, after the Homecoming game

Copy everything below the line into a new session.

---

Turn **Fraternity Row**, the west side of the map, into a detailed, believable street of Greek houses. Each house should be a different type, and each backyard should run all the way back to the fence at the woods. It is the night of the Homecoming game, October 1986, at dusk going to dark:
- some houses clearly threw parties after the game and the yards are wrecked;
- some didn't, and they look quiet, tidy, or strangely dark.

The game's tone is **horror, mystery and nostalgia**. Aim for the real, specific mess of a 1986 college party street, emptied of people, so it feels like everyone left a minute ago. No gore, no cartoon horror; the unsettling part is the silence.

## What exists today (survey this first and confirm it)
- **The project:** an Unreal Engine 5.8 game at `~/Documents/Unreal Projects/SeniorSendoff`, built from C++ at runtime. Mac, Metal SM6.
- **The row is `Source/SeniorSendoff/HouseFraternityRow.cpp/.h`:** three closed Greek-revival houses, all the same columned type with three facade styles. They stand at X −10100, −13900 and −17400, around Y −1500, facing the road (−Y). Their footprints come back as NoGo boxes reaching Y 100. It's built as the `ExteriorFraternityRow` mesh from `HouseExterior.cpp`.
- **Behind them, to the north, is the west field:**
  - its fence to the woods is at Y 4520, from X −19300 to −3900, with a gate at X −7320 to −6780 (`HouseWestField.h`);
  - its ground is shaped by `HouseWestField::RaisedHeight`.

  **Find out exactly what fills Y 100 to 4520 behind each house today, and show me the backyard plan before you replace or cover anything there.**
- **Ground and nearby jobs:**
  - The Calder mountains job froze ground relief on frat row, the west field and the woods, so the ground is today's grade (`HouseCalderTerrain::InFrozen`).
  - The campus-nature prompt is `SourceArt/CalderLandscape/CAMPUS_NATURE_PROMPT.md`. Match its realism rules, and coordinate with any session running it or the mountains job.
- **Build and run:** `bash Saved/CalderWork/tools/build.sh` and `bash Saved/CalderWork/tools/run_game.sh <name> <secs> <args>`. Always run headless with `-RenderOffScreen`. Use `-nosound` unless you're testing sound.
- **Free assets** are in the project, with more listed in `SourceArt/CalderLandscape/FAB_SHOPPING_LIST.md`. If you need something from Fab, give me exact links; you can't add Fab items yourself.

## The street: 6 houses, every one different
- **Keep the three existing houses**, but give them distinct identities: Greek letters, colours, condition and yards.
- **Add 3 more**, so the row has 6 houses. The main fraternity house is separate: it's already done and stays off-limits. Place the new houses where the road and the space allow. Choose 3 different real American Greek-house types from this list, built at 1:1 scale, each with its own roofline, materials and porch:
  - a brick Colonial Revival with a white portico;
  - a Tudor Revival with half-timbering and steep gables;
  - a big Victorian / Queen Anne converted to a chapter house, with a wrap porch and a turret;
  - a 1960s flat-roofed brick chapter house with a concrete stoop;
  - a stone Gothic-ish house;
  - a tired house on probation: peeling paint, a boarded window, a "closed by order" notice.
- **Letters:** use made-up Greek-letter combinations, never real fraternities. Each house has its letters, a porch light (some on, some dead), house numbers, and a mailbox or sign.
- **Lived-in details:**
  - couches on porches, bikes, a few cars parked on lawns or a gravel pad (period-correct 1970s–80s, no real brand badges);
  - storm doors, window AC units, bedsheet banners hanging from windows;
  - curtains and lamps on in some rooms, dark rooms in others.


## Interiors: walk-in houses, built from real references
Give the houses interiors the player can enter, at least the ground floor of every house plus the party basement or chapter room where one fits. Upper floors can be closed off or partly built.

**Research real fraternity houses online first.**
- Look at photos of real American chapter houses: listings, university housing pages, chapter websites, campus newspapers, historic-preservation surveys and real-estate photos of converted Greek houses. Aim for houses from the 1920s–60s as they looked lived-in in the 1980s.
- Collect what recurs, and write it into `SourceArt/FraternityRow/INTERIOR_REFERENCES.md` with the source links:
  - a grand foyer and staircase;
  - a wall of framed composite photos by year, trophy and paddle cases;
  - a chapter room;
  - a formal living room with a fireplace, a dining hall with long tables, a commercial kitchen;
  - a pay phone in the hall, a TV room;
  - a party basement with a bar, low ceilings, stained concrete and drop ceilings;
  - shared bathrooms;
  - small doubles with lofted beds.
- Note period details (1986): wood paneling, shag or worn commercial carpet, CRT TVs, stereo stacks, posters and pennants, ashtrays, a rotary or pay phone, fluorescent tube lights in the basement.
- **Use the photos as reference only.** Never copy a photo, logo, crest or real chapter's name or letters into the game. Build everything from the project's own runtime geometry and free or CC0 materials, and use made-up letters and crests.

**Match each interior to its house.**
- A party house's inside shows the same night as its yard: cups and cans everywhere, a keg in the basement, furniture shoved aside, a sticky dance floor, a spilled bowl, a phone off the hook.
- The house where the party stopped suddenly should feel abandoned mid-moment.
- Quiet houses are tidy, dark and still, with one room lit in the dark house.
- The Victorian, Tudor, Colonial and 1960s houses each get interiors true to their era and layout.

**Build it with the project's patterns:**
- Follow the existing campus interior system (`HouseCampusInteriors*.cpp`: room kits, fixtures, doors, the interiors review and smoke).
- Doors open like the other campus buildings' doors.
- Every room is reachable with no trapping geometry.
- Run the interiors walk-throughs over the new rooms.
- Keep the interior light pool at 6 shared lights, with all other glow emissive. Ask before raising it.

## Backyards: one per house, all the way to the woods fence
Each yard has its own type and story, with an outbuilding or two (a shed, a garage, a lean-to). Ideas:
- a sand volleyball court with a sagging net;
- a gravel parking pad;
- a stone patio with a grill and picnic tables;
- a fire-pit ring with log benches;
- a horseshoe pit;
- a basketball hoop on a garage;
- an overgrown yard with a rusting swing set left by some earlier owner;
- a neat garden one house still keeps;
- a chain-link dog run with no dog.

**Where the yards meet the fence:** the back third goes wilder. Uncut grass, brambles, saplings, leaf drifts and a worn path to a gap or a gate in the fence. That's where the woods start to feel close.

**Between houses:** hedges, fences of mixed types (picket, chain-link, split-rail), and side gates.

## After the game: who partied and who didn't
Plan it house by house, and show me the plan.
- **Party houses (about half):**
  - red cups and cans on lawns, porches and roofs (fictional brands only);
  - a keg on ice, and folding tables (beer pong is fine);
  - toilet paper in the trees;
  - string lights, half of them dead;
  - a smashed pumpkin, trash bags piled by the curb;
  - a couch dragged into the yard, a mattress, a stolen shopping cart;
  - a burned-down bonfire still smouldering (a smoke wisp is fine, no new Niagara without asking);
  - a "BEAT ___" bedsheet, crushed chrysanthemums, a lawn chair through a hedge;
  - footpath ruts in mud.

  **Vary the scale:** one house wrecked, one merely messy, one with the party clearly cut short. The last should have food still out, music gear on the porch and the door open: a mystery beat, but explain nothing.
- **Quiet houses:**
  - a raked, tidy yard with porch mums and a Homecoming wreath;
  - a dry or honours house with all the lights off;
  - one dark house with every curtain drawn and one upstairs light on.

  Each should read as a deliberate choice.

## Hard constraints (stop and ask me before breaking any of these)
- Commit nothing; leave commits to me. Never touch the old Unity copy at `~/Senior Sendoff Real`. Don't take over the screen.
- **Codex shares the project:** wait until it has been idle for 20 minutes. Take turns with any other Claude session on builds and game runs, one at a time.
- **Never touch:** the main fraternity house and its rooms, its basketball court, the party / Fall Jam area, the house parking lot and driveway, or their source files (HouseFallJam*, HouseParkingLot*, HouseParkingDetail*, HouseBasketballCourt*, HouseRearPorch*, the house rooms).
- **Ask first before changing:** the woods fence position, `HouseWestField::RaisedHeight`, `Grade()`, `HouseWoods::Height`, ground in a frozen zone, or the road.
- **Ask first before:**
  - re-enabling PropertyBoundary or adding invisible walls;
  - touching StoryCampaign, checkpoints or player movement;
  - navmesh, Landscape, PCG, Water, Niagara or Mass;
  - saving into a map;
- **Lights:** the game has 53 exterior lights today. This job may add **up to 8 new real lights on the row**:
  - all **shadowless** and small-radius, for porch lights and a string-light glow on a party yard;
  - everything else (windows, lamps, signs, most string bulbs) uses emissive materials.

  Ask before going over 8 new lights or adding any shadow-casting one. Measure the cost of the new lights in the perf A/B. Update the expected exterior light count in the reviews that check it (e.g. WoodsPass) to the new total, and report the old and new count. That's an agreed budget change, not a loosened threshold.
- **Trees:** the tree transform crc `0f3b2bb7` must hold. Don't swap the shared ExteriorTree meshes, and don't hide more than 40 TreeA/TreeB.
- **Reviews:** don't loosen review thresholds or move waypoints.
- **Collision:**
  - houses, sheds and fences are solid;
  - porch steps are walkable;
  - litter and grass have no collision.

  The player must be able to walk into every yard and reach the fence, and nothing may trap them.

## Performance
- **Measure first:** capture a baseline before any change with `Saved/CalderWork/tools/perf_ab.sh`.
- **Mesh and instancing:** build with the project's runtime-mesh patterns (FHallMesh, ISM not HISM). Use instancing for repeated litter, cups and leaves.
- **Budget:** no worse than +5% render-thread time on the standard views. Show me the numbers if the look needs more.

## How to work
**Usage pacing:**
- Work lean. Use one agent for each disjoint set of files, cheaper models only for mechanical stages, and an adversarial verifier on every substantive change.
- Never skip verification to save usage.
- Near the usage limit, write `CODEX_HANDOFF.md` with exactly where you stopped, so the next agent (Codex or Claude) can continue.

**Phases:**
1. **Survey.** Read the row, the west field and the fence code. Capture baseline views from the road, the backyards and the fence line (new views are named `detail_*` and appended at the end), and write a short "what reads fake or empty" critique.
2. **Plan.** A plan view of the street showing each house's type, letters and condition, its backyard type and its party state, plus how each yard meets the fence. Show it to me before building.
3. **Houses.** New house types and the three upgraded houses. Then the interiors, after the reference research and an interior plan per house that you show me.
4. **Yards and fences.** Outbuildings, the wild back edge, the paths to the fence.
5. **The aftermath dressing,** house by house.
6. **Verify.** Run both campus interiors walk-throughs (extended to the new rooms), FreeRoam, CampusLayoutReview, CalderTerrainReview, the Surroundings/Fraternity/ParkingLot/WoodsPass reviews, the WestField and Woods reviews, the house smokes and the MP test. Each must pass, or keep the same verdict it had before. Then:
   - walk to every yard's fence;
   - capture before/after views at dusk and at dark;
   - run perf A/B;
   - check that nothing floats or clips.
7. **Write-up.** Write `SourceArt/FraternityRow/STATUS.md` with the before/after images, save a patch, and list any Fab downloads you need from me.
