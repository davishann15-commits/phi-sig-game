# Mercer Library (Calder State, Tier A walk-in)

Plan block `LIB`: X -19000..-15600, Y -13300..-8300. Main doors face east onto the library forecourt, and a staff door at
the back opens onto the west walk. Everything here is built at runtime (`HouseCampusInteriors.cpp`, `BuildLibrary`).
Nothing is saved into the map, and `-NoCampusInteriors` gives back the solid block.

## Frame

The building is laid out in its own canonical frame, with the main doors on +Y. For the library that frame is turned
-90 degrees:
- world X = -17300 + canonical y;
- world Y = -10800 - canonical x.

The inside measures X -2470..2470 by Y -1670..1670. Floor 0's top is at -55 and floor 1's at 359. The floors above are
closed, and their windows are interior-mapped.

## Rooms

| Floor | Room | Canonical area | What is in it |
|---|---|---|---|
| 0 | Lobby and circulation | x -600..600, y 200..1670 | Terrazzo floor, entry mat and runner, palms by the doors, photographs of the college, the oak circulation desk (lamp, books waiting to be shelved, a cup) |
| 0 | Periodicals | x -2470..-600, y 200..1670 | Slanted magazine racks, two reading groups (rug, leather club chairs, side table and lamp, floor lamp), the microfiche reader |
| 0 | Card catalog | x 600..2470, y 200..1670 | Six oak catalog cases and a consulting table |
| 0 | Reference reading room | x -2470..-600, y -1670..200 | Three long oak tables with wooden chairs, lamps and pendants, reference bookcases on the inner walls, QUIET PLEASE |
| 0 | Carrels | x 600..2470, y -1670..200 | Three rows of carrels with green banker's lamps; books and backpacks left at them |
| 0 | Staff workroom (locked) | x -600..600, y -1670..-1000 | Trestle worktable, boxes; the STAFF ENTRANCE door at the back |
| 1 | Stacks | whole floor | Steel ranges A-Z, group study tables, carrels along the side windows |
| 1 | Copy room (locked) | x -600..600, y -1670..-1350 | |

## Doors (`ACampusDoor`, replicated)

| Id | Kind | Locked reason (HUD) |
|---|---|---|
| `Campus_LIB_MainA` / `MainB` | Glazed oak French pair, swings in | open |
| `Campus_LIB_Workroom` | Steel equipment door | STAFF ONLY - the library workroom |
| `Campus_LIB_CopyRoom` | Steel equipment door | COPY ROOM - staff only after 5 PM |
| `Campus_LIB_StaffEntrance` | Steel equipment door at the back | STAFF ENTRANCE - deliveries at the front desk |

## Furniture from the house

The pieces are the basement library's own meshes, materials and damage stages, at the house's 1.8 horizontal stretch
undone (scale 0.556 in X and Y):
- **Fixed decor** (instanced, one component per mesh): bookcases with their filled shelf sets, rugs, pendants, wall
  photographs, and the workroom's worktable and boxes.
- **Props** (`ACampusProp`, server-spawned, replicated by kit index): chairs, club chairs, side tables, lamps, plants,
  books, papers, cups, backpacks and the umbrella. They move, carry and break like the house's furniture, but their
  breaks are not house damage (tag `SSOCampusDamage`). A building's props go when its inside is freed, and come back
  intact next time.

## Light

Six pooled, shadowless point lights on lighting channel 1 are switched on near the view only while it is inside. Every
interior mesh, decor piece and prop is lit on channel 1 only; a prop carried outdoors joins channel 0. The troffers,
lamp shades and bulbs glow by emission.

## Checks

`-CampusInteriorsReview` walks a 26-stop route with real input, tries the locked doors and leaves. `-CampusLayoutReview`
checks the outside (doors on the walk network, apron depths, the staff door's walk). Evidence is in
`SourceArt/CampusMaster/shots/2a` and later steps.

## Latest results

## Doors (`ACampusDoor`, replicated)

| Door | State (the HUD's reason when locked) |
|---|---|
| `MainA` | open |
| `MainB` | open |
| `StaffEntrance` | locked - STAFF ENTRANCE - deliveries at the front desk |
| `Workroom` | locked - STAFF ONLY - the library workroom |
| `CopyRoom` | locked - COPY ROOM - staff only after 5 PM |

## Smoke test (`-CampusInteriorsSmoke=LIB`)

Builds the inside, waits three seconds, then checks it the way the house's room smoke tests check theirs:

- geometry: 50046 triangles; 260 pieces of the house's kit placed;
- floors: a walkable floor found under 13 of 13 light anchors (the pool lights at most 6 at once);
- props: 63 carryable props from the house's furniture, 0 floating;
- doors: 5, the main ones unlocked, 3 locked and 3 of them with a reason;
- wall clocks: 2, worst hand 0.00 degrees from the clock tower's time;
- sounds: 0; real windows into built rooms: 84;
- verdict: **PASS**

## The walk (`-CampusInteriorsReview=LIB`)

Real input, in through the main doors, through every open room and up and down every stair, trying the locked doors, and back out until the inside is freed: 31 steps in this building (log in `evidence/walk_LIB.txt`). Last run, with every building of its tier: `PASS buildings=LIB,HAR,SCI,UNI,STA,TWR stops=256/256 problems=0 mainDoorsOpened=6/6 lockedDoorsHeld=1 reasonsShown=1 builtWhileInside=1 freedAfterLeaving=6/6 in...`

## References and before

- Floor plan, drawn for PLAN.md before building: [`references/fp_library_g.png`](references/fp_library_g.png)
- Floor plan, drawn for PLAN.md before building: [`references/fp_library_2.png`](references/fp_library_2.png)
- The site before the job (`-NoCampusLayout`, the in-game top view): [`references/site_before.jpg`](references/site_before.jpg)
- The site after (the same view): [`references/site_after.jpg`](references/site_after.jpg)

| Before | After |
|---|---|
| ![](references/site_before.jpg) | ![](references/site_after.jpg) |

## Shots (from the walk)

![](shots/LIB_1_Doors.jpg)
![](shots/LIB_2_Lobby.jpg)
![](shots/LIB_3_Catalog.jpg)
![](shots/LIB_4_Reading.jpg)
![](shots/LIB_5_Carrels.jpg)
![](shots/LIB_6_Stacks.jpg)

