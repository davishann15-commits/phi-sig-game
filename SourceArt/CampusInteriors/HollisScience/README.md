# Hollis Hall of Science (Calder State, Tier A walk-in)

Plan block `SCI` (X -15200..-10600, Y -17400..-14200): the sciences. Main doors north onto the quad's south walk. Built at runtime (`HouseCampusInteriorsScience.cpp`); nothing is saved into the map; `-NoCampusInteriors` gives back the solid block.

## Rooms

- Floor 0: lobby with twin oak stairs and the directory, chemistry lab, physics lab, the tiered lecture hall (two storeys).
- Floor 1: optics lab, seminar room, faculty lounge; prep room, office, darkroom, faculty offices locked.

## Doors (`ACampusDoor`, replicated)

| Door | State (the HUD's reason when locked) |
|---|---|
| `MainA` | open |
| `MainB` | open |
| `Lab101West` | open |
| `Lab101` | open |
| `Lab102` | open |
| `Lab102East` | open |
| `HallWest` | open |
| `HallEast` | open |
| `PrepRoom` | locked - PREP ROOM 112 - AUTHORIZED PERSONNEL ONLY |
| `Office` | locked - DEPARTMENT OFFICE 114 - CLOSED SATURDAY |
| `Lab204` | open |
| `Seminar210` | open |
| `Lounge212` | open |
| `Darkroom206` | locked - DARKROOM 206 - IN USE, DO NOT OPEN |
| `Offices214` | locked - FACULTY OFFICES 214 - OFFICE HOURS MON-FRI |

## Smoke test (`-CampusInteriorsSmoke=SCI`)

Builds the inside, waits three seconds, then checks it the way the house's room smoke tests check theirs:

- geometry: 101720 triangles; 12 pieces of the house's kit placed;
- floors: a walkable floor found under 23 of 23 light anchors (the pool lights at most 6 at once);
- props: 79 carryable props from the house's furniture, 0 floating;
- doors: 15, the main ones unlocked, 4 locked and 4 of them with a reason;
- wall clocks: 8, worst hand 0.00 degrees from the clock tower's time;
- sounds: 0; real windows into built rooms: 48;
- verdict: **PASS**

## The walk (`-CampusInteriorsReview=SCI`)

Real input, in through the main doors, through every open room and up and down every stair, trying the locked doors, and back out until the inside is freed: 44 steps in this building (log in `evidence/walk_SCI.txt`). Last run, with every building of its tier: `PASS buildings=LIB,HAR,SCI,UNI,STA,TWR stops=256/256 problems=0 mainDoorsOpened=6/6 lockedDoorsHeld=1 reasonsShown=1 builtWhileInside=1 freedAfterLeaving=6/6 in...`

## References and before

- Floor plan, drawn for PLAN.md before building: [`references/fp_science_g.png`](references/fp_science_g.png)
- The site before the job (`-NoCampusLayout`, the in-game top view): [`references/site_before.jpg`](references/site_before.jpg)
- The site after (the same view): [`references/site_after.jpg`](references/site_after.jpg)

| Before | After |
|---|---|
| ![](references/site_before.jpg) | ![](references/site_after.jpg) |

## Shots (from the walk)

![](shots/SCI_1_Doors.jpg)
![](shots/SCI_2_Lobby.jpg)
![](shots/SCI_3_Chem.jpg)
![](shots/SCI_4_Hall.jpg)
![](shots/SCI_5_Physics.jpg)
![](shots/SCI_6_Optics.jpg)
![](shots/SCI_7_Lounge.jpg)

