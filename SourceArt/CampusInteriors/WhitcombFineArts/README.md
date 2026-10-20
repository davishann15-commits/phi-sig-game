# Whitcomb Fine Arts (Calder State, Tier B walk-in)

Plan block `ART` (X -13250..-9050, Y -21400..-18400): the arts. Main doors north, east side doors. Built at runtime (`HouseCampusInteriorsGeneric.cpp`); nothing is saved into the map; `-NoCampusInteriors` gives back the solid block.

## Rooms

- Front row, west to east: Painting (studio), 2 bays, Drawing (studio), 2 bays, office (locked), Whitcomb Gallery (gallery), 4 bays
- Back row, west to east: Ceramics (studio), 3 bays, Kiln Room (locked room), 2 bays, Sculpture (studio), 3 bays, Storage (storage (locked)), 3 bays

## Doors (`ACampusDoor`, replicated)

| Door | State (the HUD's reason when locked) |
|---|---|
| `MainA` | open |
| `MainB` | open |
| `Room101` | open |
| `Room103` | open |
| `Room105` | locked - 105 OFFICE - office hours Monday to Friday |
| `Room102` | open |
| `Room104` | locked - KILN ROOM - CLOSED SATURDAY |
| `Room106` | open |
| `Room108` | locked - STORAGE - AUTHORIZED PERSONNEL ONLY |

## Smoke test (`-CampusInteriorsSmoke=ART`)

Builds the inside, waits three seconds, then checks it the way the house's room smoke tests check theirs:

- geometry: 22042 triangles; 2 pieces of the house's kit placed;
- floors: a walkable floor found under 11 of 11 light anchors (the pool lights at most 6 at once);
- props: 5 carryable props from the house's furniture, 0 floating;
- doors: 9, the main ones unlocked, 3 locked and 3 of them with a reason;
- wall clocks: 2, worst hand 0.00 degrees from the clock tower's time;
- sounds: 0; real windows into built rooms: 18;
- verdict: **PASS**

## The walk (`-CampusInteriorsReview=ART`)

Real input, in through the main doors, through every open room and up and down every stair, trying the locked doors, and back out until the inside is freed: 26 steps in this building (log in `evidence/walk_ART.txt`). Last run, with every building of its tier: `PASS buildings=MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU stops=174/174 problems=0 mainDoorsOpened=12/12 lockedDoorsHeld=1 reasonsShown=1 builtWhileInsid...`

## References and before

- The site before the job (`-NoCampusLayout`, the in-game top view): [`references/site_before.jpg`](references/site_before.jpg)
- The site after (the same view): [`references/site_after.jpg`](references/site_after.jpg)

| Before | After |
|---|---|
| ![](references/site_before.jpg) | ![](references/site_after.jpg) |

## Shots (from the walk)

![](shots/ART_1_Lobby.jpg)
![](shots/ART_2_Room.jpg)

