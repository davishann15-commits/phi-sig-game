# Ashby Hall (Calder State, Tier B walk-in)

Plan block `ASH` (X 14700..18900, Y -10400..-7100): a dorm. Main doors north, west side doors. Built at runtime (`HouseCampusInteriorsGeneric.cpp`); nothing is saved into the map; `-NoCampusInteriors` gives back the solid block.

## Rooms

- Front row, west to east: dorm room, dorm room (locked), dorm room, dorm room (locked), Laundry (laundry), Lounge (lounge), 2 bays, dorm room (locked), dorm room
- Back row, west to east: Recreation (rec room), 4 bays, Study Lounge (lounge), 3 bays, dorm room (locked), 2 bays, Storage (storage (locked)), 2 bays

## Doors (`ACampusDoor`, replicated)

| Door | State (the HUD's reason when locked) |
|---|---|
| `MainA` | open |
| `MainB` | open |
| `Room101` | open |
| `Room103` | locked - Room 103 - nobody home, they're at the game |
| `Room105` | open |
| `Room107` | locked - Room 107 - nobody home, they're at the game |
| `Room109` | open |
| `Room113` | locked - Room 113 - DO NOT DISTURB on the knob |
| `Room115` | open |
| `Room106` | locked - Room 106 - nobody home, they're at the game |
| `Room108` | locked - STORAGE - AUTHORIZED PERSONNEL ONLY |

## Smoke test (`-CampusInteriorsSmoke=ASH`)

Builds the inside, waits three seconds, then checks it the way the house's room smoke tests check theirs:

- geometry: 14022 triangles; 27 pieces of the house's kit placed;
- floors: a walkable floor found under 13 of 13 light anchors (the pool lights at most 6 at once);
- props: 41 carryable props from the house's furniture, 0 floating;
- doors: 11, the main ones unlocked, 5 locked and 5 of them with a reason;
- wall clocks: 4, worst hand 0.00 degrees from the clock tower's time;
- sounds: 0; real windows into built rooms: 18;
- verdict: **PASS**

## The walk (`-CampusInteriorsReview=ASH`)

Real input, in through the main doors, through every open room and up and down every stair, trying the locked doors, and back out until the inside is freed: 32 steps in this building (log in `evidence/walk_ASH.txt`). Last run, with every building of its tier: `PASS buildings=MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU stops=174/174 problems=0 mainDoorsOpened=12/12 lockedDoorsHeld=1 reasonsShown=1 builtWhileInsid...`

## References and before

- The site before the job (`-NoCampusLayout`, the in-game top view): [`references/site_before.jpg`](references/site_before.jpg)
- The site after (the same view): [`references/site_after.jpg`](references/site_after.jpg)

| Before | After |
|---|---|
| ![](references/site_before.jpg) | ![](references/site_after.jpg) |

## Shots (from the walk)

![](shots/ASH_1_Lobby.jpg)
![](shots/ASH_2_Room.jpg)

