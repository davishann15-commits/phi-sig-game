# Old Main (Calder State, Tier B walk-in)

Plan block `MAIN` (X -15200..-11400, Y -9700..-7100): administration. Main doors north onto the promenade, south doors onto the quad. Built at runtime (`HouseCampusInteriorsGeneric.cpp`); nothing is saved into the map; `-NoCampusInteriors` gives back the solid block.

## Rooms

- Front row, west to east: Dean Of Students (dean's office), 2 bays, Admissions (waiting room), 2 bays, Registrar (registrar), 3 bays, Records (locked room)
- Back row, west to east: Bursar (locked room), 3 bays, Conference Room (seminar), 2 bays, Financial Aid (office (locked)), Counseling (office (locked)), President (locked room), 3 bays

## Doors (`ACampusDoor`, replicated)

| Door | State (the HUD's reason when locked) |
|---|---|
| `MainA` | open |
| `MainB` | open |
| `Room101` | open |
| `Room103` | open |
| `Room105` | open |
| `Room107` | locked - RECORDS - CLOSED SATURDAY |
| `Room102` | locked - BURSAR - CLOSED SATURDAY |
| `Room104` | open |
| `Room106` | locked - 106 FINANCIAL AID - office hours Monday to Friday |
| `Room108` | locked - 108 COUNSELING - office hours Monday to Friday |
| `Room110` | locked - PRESIDENT - CLOSED SATURDAY |

## Smoke test (`-CampusInteriorsSmoke=MAIN`)

Builds the inside, waits three seconds, then checks it the way the house's room smoke tests check theirs:

- geometry: 14144 triangles; 20 pieces of the house's kit placed;
- floors: a walkable floor found under 9 of 9 light anchors (the pool lights at most 6 at once);
- props: 76 carryable props from the house's furniture, 0 floating;
- doors: 11, the main ones unlocked, 5 locked and 5 of them with a reason;
- wall clocks: 5, worst hand 0.00 degrees from the clock tower's time;
- sounds: 0; real windows into built rooms: 12;
- verdict: **PASS**

## The walk (`-CampusInteriorsReview=MAIN`)

Real input, in through the main doors, through every open room and up and down every stair, trying the locked doors, and back out until the inside is freed: 23 steps in this building (log in `evidence/walk_MAIN.txt`). Last run, with every building of its tier: `PASS buildings=MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU stops=174/174 problems=0 mainDoorsOpened=12/12 lockedDoorsHeld=1 reasonsShown=1 builtWhileInsid...`

## References and before

- The site before the job (`-NoCampusLayout`, the in-game top view): [`references/site_before.jpg`](references/site_before.jpg)
- The site after (the same view): [`references/site_after.jpg`](references/site_after.jpg)

| Before | After |
|---|---|
| ![](references/site_before.jpg) | ![](references/site_after.jpg) |

## Shots (from the walk)

![](shots/MAIN_1_Lobby.jpg)
![](shots/MAIN_2_Room.jpg)

