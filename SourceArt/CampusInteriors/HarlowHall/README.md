# Harlow Hall (Calder State, Tier A walk-in)

Plan block `HAR` (X -10600..-6800, Y -9450..-7100): a dorm. Main doors north onto the promenade. Built at runtime (`HouseCampusInteriorsHarlow.cpp`); nothing is saved into the map; `-NoCampusInteriors` gives back the solid block.

## Rooms

- Floor 0: lobby with the hall desk's pass-through and mailboxes, the TV lounge, the men's bathroom, laundry, rec room, window commons, furnished dorm rooms.
- Floor 1: study room, floor kitchen, the women's bathroom, window commons, furnished dorm rooms; the rest of the rooms locked.

## Doors (`ACampusDoor`, replicated)

| Door | State (the HUD's reason when locked) |
|---|---|
| `MainA` | open |
| `MainB` | open |
| `Room101` | open |
| `Room103` | locked - Room 103 - nobody's home |
| `Room105` | open |
| `DeskOffice` | locked - HALL STAFF ONLY - ring at the desk |
| `Room107` | locked - Room 107 - GONE HOME FOR THE WEEKEND |
| `Room102` | locked - Room 102 - a note taped to it: AT THE GAME, BACK LATE |
| `Room104` | open |
| `Room106` | locked - Room 106 - locked tight |
| `Room108` | locked - Room 108 - a note taped to it: AT THE GAME, BACK LATE |
| `Room110` | open |
| `Room112` | locked - Room 112 - locked tight |
| `Room114` | locked - Room 114 - a note taped to it: AT THE GAME, BACK LATE |
| `Room116` | open |
| `Room118` | locked - Room 118 - locked tight |
| `Room120` | locked - Room 120 - a note taped to it: AT THE GAME, BACK LATE |
| `Room201` | locked - Room 201 - locked; a radio playing inside |
| `Room203` | locked - Room 203 - GONE HOME FOR THE WEEKEND |
| `Room205` | open |
| `Room207` | locked - Room 207 - locked; a radio playing inside |
| `Room209` | open |
| `Room211` | locked - Room 211 - nobody's home |
| `Room213` | open |
| `Room215` | locked - Room 215 - GONE HOME FOR THE WEEKEND |
| `Room217` | locked - Room 217 - nobody's home |
| `Room219` | locked - Room 219 - locked; a radio playing inside |
| `Room202` | open |
| `Room204` | locked - Room 204 - a note taped to it: AT THE GAME, BACK LATE |
| `Room206` | locked - Room 206 - DO NOT DISTURB on the knob |
| `Room208` | locked - Room 208 - locked tight |
| `Room210` | open |
| `Room212` | locked - Room 212 - DO NOT DISTURB on the knob |
| `Room214` | locked - Room 214 - locked tight |
| `Room216` | open |
| `Room218` | locked - Room 218 - DO NOT DISTURB on the knob |
| `Room220` | locked - Room 220 - locked tight |

## Smoke test (`-CampusInteriorsSmoke=HAR`)

Builds the inside, waits three seconds, then checks it the way the house's room smoke tests check theirs:

- geometry: 54860 triangles; 112 pieces of the house's kit placed;
- floors: a walkable floor found under 44 of 44 light anchors (the pool lights at most 6 at once);
- props: 147 carryable props from the house's furniture, 0 floating;
- doors: 37, the main ones unlocked, 24 locked and 24 of them with a reason;
- wall clocks: 10, worst hand 0.00 degrees from the clock tower's time;
- sounds: 3; real windows into built rooms: 35;
- verdict: **PASS**

## The walk (`-CampusInteriorsReview=HAR`)

Real input, in through the main doors, through every open room and up and down every stair, trying the locked doors, and back out until the inside is freed: 65 steps in this building (log in `evidence/walk_HAR.txt`). Last run, with every building of its tier: `PASS buildings=LIB,HAR,SCI,UNI,STA,TWR stops=256/256 problems=0 mainDoorsOpened=6/6 lockedDoorsHeld=1 reasonsShown=1 builtWhileInside=1 freedAfterLeaving=6/6 in...`

## References and before

- Floor plan, drawn for PLAN.md before building: [`references/fp_harlow_g.png`](references/fp_harlow_g.png)
- The site before the job (`-NoCampusLayout`, the in-game top view): [`references/site_before.jpg`](references/site_before.jpg)
- The site after (the same view): [`references/site_after.jpg`](references/site_after.jpg)

| Before | After |
|---|---|
| ![](references/site_before.jpg) | ![](references/site_after.jpg) |

## Shots (from the walk)

![](shots/HAR_1_Doors.jpg)
![](shots/HAR_2_Lobby.jpg)
![](shots/HAR_3_Lounge.jpg)
![](shots/HAR_4_Room105.jpg)
![](shots/HAR_5_Commons.jpg)
![](shots/HAR_6_Room205.jpg)
![](shots/HAR_7_Bath.jpg)
![](shots/HAR_8_Room202.jpg)
![](shots/HAR_9_Laundry.jpg)

