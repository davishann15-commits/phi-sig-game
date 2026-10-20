# Kerr Hall (Calder State, Tier B walk-in)

Plan block `KER` (X 11000..14400, Y -15400..-11400): classrooms. Main doors west onto Calder Dr. Built at runtime (`HouseCampusInteriorsGeneric.cpp`); nothing is saved into the map; `-NoCampusInteriors` gives back the solid block.

## Rooms

- Front row, west to east: Classroom (classroom), 2 bays, office (locked), office (locked), Classroom (classroom), 2 bays, Seminar (seminar), 2 bays
- Back row, west to east: Classroom (classroom), 3 bays, Classroom (classroom), 2 bays, Lecture Room (auditorium), 3 bays, Classroom (classroom), 2 bays

## Doors (`ACampusDoor`, replicated)

| Door | State (the HUD's reason when locked) |
|---|---|
| `MainA` | open |
| `MainB` | open |
| `Room101` | open |
| `Room103` | locked - 103 OFFICE - office hours Monday to Friday |
| `Room105` | locked - 105 OFFICE - office hours Monday to Friday |
| `Room107` | open |
| `Room109` | open |
| `Room102` | open |
| `Room104` | open |
| `Room108` | open |

## Smoke test (`-CampusInteriorsSmoke=KER`)

Builds the inside, waits three seconds, then checks it the way the house's room smoke tests check theirs:

- geometry: 51248 triangles; 9 pieces of the house's kit placed;
- floors: a walkable floor found under 12 of 12 light anchors (the pool lights at most 6 at once);
- props: 60 carryable props from the house's furniture, 0 floating;
- doors: 10, the main ones unlocked, 2 locked and 2 of them with a reason;
- wall clocks: 8, worst hand 0.00 degrees from the clock tower's time;
- sounds: 0; real windows into built rooms: 21;
- verdict: **PASS**

## The walk (`-CampusInteriorsReview=KER`)

Real input, in through the main doors, through every open room and up and down every stair, trying the locked doors, and back out until the inside is freed: 32 steps in this building (log in `evidence/walk_KER.txt`). Last run, with every building of its tier: `PASS buildings=MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU stops=174/174 problems=0 mainDoorsOpened=12/12 lockedDoorsHeld=1 reasonsShown=1 builtWhileInsid...`

## References and before

- The site before the job (`-NoCampusLayout`, the in-game top view): [`references/site_before.jpg`](references/site_before.jpg)
- The site after (the same view): [`references/site_after.jpg`](references/site_after.jpg)

| Before | After |
|---|---|
| ![](references/site_before.jpg) | ![](references/site_after.jpg) |

## Shots (from the walk)

![](shots/KER_1_Lobby.jpg)
![](shots/KER_2_Room.jpg)

