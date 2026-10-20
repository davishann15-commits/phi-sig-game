# Calder State Stadium (Calder State, Tier A walk-in)

The stadium (football), free-standing on its bowl at (2400, -13300). GATE 4 and GATE 5 from the ticket plaza; the press box by its stair tower. Built at runtime (`HouseCampusInteriorsStadium.cpp`; the exterior's cut-outs in `HouseExteriorCampus.cpp`, its facade in `HouseCampusPolishStadium.cpp`); nothing is saved into the map; `-NoCampusInteriors` closes it again.

## Rooms

- The concourse behind the north stands (GEESE GRILL, popcorn, cold drinks; the restrooms locked), the vomitory to rows 4-7, the Geese locker room and the players' tunnel, the press box (press row, PA desk; the radio booth locked) and its stair tower.

## Doors (`ACampusDoor`, replicated)

| Door | State (the HUD's reason when locked) |
|---|---|
| `MainA` | open |
| `MainB` | open |
| `PressBox` | open |
| `RadioBooth` | locked - ON AIR - THE POSTGAME SHOW, DO NOT ENTER |
| `LockerRoom` | open |
| `MensRoom` | locked - MEN - CLOSED FOR CLEANING AFTER THE GAME |
| `WomensRoom` | locked - WOMEN - CLOSED FOR CLEANING AFTER THE GAME |

## Smoke test (`-CampusInteriorsSmoke=STA`)

Builds the inside, waits three seconds, then checks it the way the house's room smoke tests check theirs:

- geometry: 17548 triangles; 7 pieces of the house's kit placed;
- floors: a walkable floor found under 15 of 15 light anchors (the pool lights at most 6 at once);
- props: 48 carryable props from the house's furniture, 0 floating;
- doors: 7, the main ones unlocked, 3 locked and 3 of them with a reason;
- wall clocks: 3, worst hand 0.00 degrees from the clock tower's time;
- sounds: 1;
- verdict: **PASS**

## The walk (`-CampusInteriorsReview=STA`)

Real input, in through the main doors, through every open room and up and down every stair, trying the locked doors, and back out until the inside is freed: 69 steps in this building (log in `evidence/walk_STA.txt`). Last run, with every building of its tier: `PASS buildings=LIB,HAR,SCI,UNI,STA,TWR stops=256/256 problems=0 mainDoorsOpened=6/6 lockedDoorsHeld=1 reasonsShown=1 builtWhileInside=1 freedAfterLeaving=6/6 in...`

## References and before

- The site before the job (`-NoCampusLayout`, the in-game top view): [`references/site_before.jpg`](references/site_before.jpg)
- The site after (the same view): [`references/site_after.jpg`](references/site_after.jpg)

| Before | After |
|---|---|
| ![](references/site_before.jpg) | ![](references/site_after.jpg) |

## Shots (from the walk)

![](shots/STA_1_Tower.jpg)
![](shots/STA_2_PressBox.jpg)
![](shots/STA_3_Gate5.jpg)
![](shots/STA_4_Concourse.jpg)
![](shots/STA_5_Locker.jpg)
![](shots/STA_6_Tunnel.jpg)
![](shots/STA_7_Vomitory.jpg)

