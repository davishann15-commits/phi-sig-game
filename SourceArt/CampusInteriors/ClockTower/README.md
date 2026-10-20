# The Calder Tower (Calder State, Tier A walk-in)

The clock tower (the clock tower (1897)), free-standing at (-9000, -10800), its shell the tower's own (`HouseClockTower*`, opened for the stair only while the interiors are on). Its oak door on the north face, off the tower plaza. Built at runtime (`HouseCampusInteriorsTower.cpp`); nothing is saved into the map; `-NoCampusInteriors` or `-NoClockTower` closes it again.

## Rooms

- The ground floor: the store under the first flight (worktable, notice board, coffee chest, salt sacks for the winter steps).
- The open-well oak stair: eleven flights of thirteen risers round the shaft to the belfry floor, through its slot.
- The belfry: the hour bell and the four quarter bells in their oak frame, the town and the campus through the arches (guarded).

## Doors (`ACampusDoor`, replicated)

| Door | State (the HUD's reason when locked) |
|---|---|
| `MainA` | open |
| `MainB` | open |

## Smoke test (`-CampusInteriorsSmoke=TWR`)

Builds the inside, waits three seconds, then checks it the way the house's room smoke tests check theirs:

- geometry: 19022 triangles; 13 pieces of the house's kit placed;
- floors: a walkable floor found under 11 of 11 light anchors (the pool lights at most 6 at once);
- props: 21 carryable props from the house's furniture, 0 floating;
- doors: 2, the main ones unlocked, 0 locked and 0 of them with a reason;
- wall clocks: 1, worst hand 0.00 degrees from the clock tower's time;
- sounds: 0;
- verdict: **PASS**

## The walk (`-CampusInteriorsReview=TWR`)

Real input, in through the main doors, through every open room and up and down every stair, trying the locked doors, and back out until the inside is freed: 34 steps in this building (log in `evidence/walk_TWR.txt`). Last run, with every building of its tier: `PASS buildings=LIB,HAR,SCI,UNI,STA,TWR stops=256/256 problems=0 mainDoorsOpened=6/6 lockedDoorsHeld=1 reasonsShown=1 builtWhileInside=1 freedAfterLeaving=6/6 in...`

## References and before

- The site before the job (`-NoCampusLayout`, the in-game top view): [`references/site_before.jpg`](references/site_before.jpg)
- The site after (the same view): [`references/site_after.jpg`](references/site_after.jpg)

| Before | After |
|---|---|
| ![](references/site_before.jpg) | ![](references/site_after.jpg) |

## Shots (from the walk)

![](shots/TWR_1_Door.jpg)
![](shots/TWR_2_Inside.jpg)
![](shots/TWR_3_Stair.jpg)
![](shots/TWR_4_Belfry.jpg)

