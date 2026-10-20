# Varsity Fieldhouse (Calder State, Tier C walk-in)

Plan block `FLD` (X 15000..18900, Y -17000..-12200): athletics. Main doors north, west side door. Built at runtime (`HouseCampusInteriorsGeneric.cpp`); nothing is saved into the map; `-NoCampusInteriors` gives back the solid block.

## Rooms

- The lobby only: directory, notice board, payphone, ash urn, the building's own display; every other door locked, with its reason on the HUD.

## Doors (`ACampusDoor`, replicated)

| Door | State (the HUD's reason when locked) |
|---|---|
| `MainA` | open |
| `MainB` | open |
| `Lobby0` | locked - LOCKER ROOMS - TEAMS ONLY |
| `Lobby1` | locked - THE GYM - chained and padlocked for the weekend |

## Smoke test (`-CampusInteriorsSmoke=FLD`)

Builds the inside, waits three seconds, then checks it the way the house's room smoke tests check theirs:

- geometry: 7814 triangles; 1 pieces of the house's kit placed;
- floors: a walkable floor found under 1 of 1 light anchors (the pool lights at most 6 at once);
- props: 1 carryable props from the house's furniture, 0 floating;
- doors: 4, the main ones unlocked, 2 locked and 2 of them with a reason;
- wall clocks: 1, worst hand 0.00 degrees from the clock tower's time;
- sounds: 0; real windows into built rooms: 2;
- verdict: **PASS**

## The walk (`-CampusInteriorsReview=FLD`)

Real input, in through the main doors, through every open room and up and down every stair, trying the locked doors, and back out until the inside is freed: 11 steps in this building (log in `evidence/walk_FLD.txt`). Last run, with every building of its tier: `PASS buildings=MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU stops=174/174 problems=0 mainDoorsOpened=12/12 lockedDoorsHeld=1 reasonsShown=1 builtWhileInsid...`

## References and before

- The site before the job (`-NoCampusLayout`, the in-game top view): [`references/site_before.jpg`](references/site_before.jpg)
- The site after (the same view): [`references/site_after.jpg`](references/site_after.jpg)

| Before | After |
|---|---|
| ![](references/site_before.jpg) | ![](references/site_after.jpg) |

## Shots (from the walk)

![](shots/FLD_1_Lobby.jpg)

