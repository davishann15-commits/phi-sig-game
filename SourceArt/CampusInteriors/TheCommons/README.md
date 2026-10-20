# The Commons (Calder State, Tier B walk-in)

Plan block `COM` (X 11000..14200, Y -10000..-7100): dining. Main doors west, north doors onto the promenade. Built at runtime (`HouseCampusInteriorsGeneric.cpp`); nothing is saved into the map; `-NoCampusInteriors` gives back the solid block.

## Rooms

- Front row, west to east: The Hearth (lounge), 3 bays, Manager (locked room), 2 bays
- Back row, west to east: Dining Hall (dining hall), 5 bays, Kitchen (kitchen (locked)), 2 bays

## Doors (`ACampusDoor`, replicated)

| Door | State (the HUD's reason when locked) |
|---|---|
| `MainA` | open |
| `MainB` | open |
| `Room103` | locked - MANAGER - CLOSED SATURDAY |
| `Room104` | locked - KITCHEN - STAFF ONLY |

## Smoke test (`-CampusInteriorsSmoke=COM`)

Builds the inside, waits three seconds, then checks it the way the house's room smoke tests check theirs:

- geometry: 8502 triangles; 52 pieces of the house's kit placed;
- floors: a walkable floor found under 6 of 6 light anchors (the pool lights at most 6 at once);
- props: 196 carryable props from the house's furniture, 0 floating;
- doors: 4, the main ones unlocked, 2 locked and 2 of them with a reason;
- wall clocks: 4, worst hand 0.00 degrees from the clock tower's time;
- sounds: 0; real windows into built rooms: 14;
- verdict: **PASS**

## The walk (`-CampusInteriorsReview=COM`)

Real input, in through the main doors, through every open room and up and down every stair, trying the locked doors, and back out until the inside is freed: 17 steps in this building (log in `evidence/walk_COM.txt`). Last run, with every building of its tier: `PASS buildings=MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU stops=174/174 problems=0 mainDoorsOpened=12/12 lockedDoorsHeld=1 reasonsShown=1 builtWhileInsid...`

## References and before

- The site before the job (`-NoCampusLayout`, the in-game top view): [`references/site_before.jpg`](references/site_before.jpg)
- The site after (the same view): [`references/site_after.jpg`](references/site_after.jpg)

| Before | After |
|---|---|
| ![](references/site_before.jpg) | ![](references/site_after.jpg) |

## Shots (from the walk)

![](shots/COM_1_Lobby.jpg)
![](shots/COM_2_Room.jpg)

