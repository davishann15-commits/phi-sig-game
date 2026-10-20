# Calder Hall (Calder State, Tier B walk-in)

Plan block `HUM` (X -19000..-15800, Y -18600..-14400): the humanities. Main doors north, west side doors. Built at runtime (`HouseCampusInteriorsGeneric.cpp`); nothing is saved into the map; `-NoCampusInteriors` gives back the solid block.

## Rooms

- Front row, west to east: English (office), office (locked), Seminar (seminar), Seminar (seminar), office (locked), History (office)
- Back row, west to east: Classroom (classroom), 3 bays, Auditorium (auditorium), 3 bays, Classroom (classroom), 2 bays

## Doors (`ACampusDoor`, replicated)

| Door | State (the HUD's reason when locked) |
|---|---|
| `MainA` | open |
| `MainB` | open |
| `Room101` | open |
| `Room103` | locked - 103 OFFICE - office hours Monday to Friday |
| `Room105` | open |
| `Room107` | open |
| `Room109` | locked - 109 OFFICE - office hours Monday to Friday |
| `Room111` | open |
| `Room102` | open |
| `Room106` | open |

## Smoke test (`-CampusInteriorsSmoke=HUM`)

Builds the inside, waits three seconds, then checks it the way the house's room smoke tests check theirs:

- geometry: 40152 triangles; 32 pieces of the house's kit placed;
- floors: a walkable floor found under 11 of 11 light anchors (the pool lights at most 6 at once);
- props: 36 carryable props from the house's furniture, 0 floating;
- doors: 10, the main ones unlocked, 2 locked and 2 of them with a reason;
- wall clocks: 8, worst hand 0.00 degrees from the clock tower's time;
- sounds: 0; real windows into built rooms: 18;
- verdict: **PASS**

## The walk (`-CampusInteriorsReview=HUM`)

Real input, in through the main doors, through every open room and up and down every stair, trying the locked doors, and back out until the inside is freed: 32 steps in this building (log in `evidence/walk_HUM.txt`). Last run, with every building of its tier: `PASS buildings=MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU stops=174/174 problems=0 mainDoorsOpened=12/12 lockedDoorsHeld=1 reasonsShown=1 builtWhileInsid...`

## References and before

- The site before the job (`-NoCampusLayout`, the in-game top view): [`references/site_before.jpg`](references/site_before.jpg)
- The site after (the same view): [`references/site_after.jpg`](references/site_after.jpg)

| Before | After |
|---|---|
| ![](references/site_before.jpg) | ![](references/site_after.jpg) |

## Shots (from the walk)

![](shots/HUM_1_Lobby.jpg)
![](shots/HUM_2_Room.jpg)

