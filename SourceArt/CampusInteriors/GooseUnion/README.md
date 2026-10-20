# The Goose Union (Calder State, Tier A walk-in)

Plan block `UNI` (X -8200..-4300, Y -14600..-10700): the student union. Four entrance pairs, one on every side; the west one opens onto the tower plaza. Built at runtime (`HouseCampusInteriorsUnion.cpp`); nothing is saved into the map; `-NoCampusInteriors` gives back the solid block.

## Rooms

- Floor 0: the concourse round the atrium, TV lounge, bookstore (closed), the Goose Pit, game room, mailroom, trophy case with The Old Lantern.
- Floor 1: homecoming committee room, the Calder Ballroom; student government, The Honker, meeting rooms locked.

## Doors (`ACampusDoor`, replicated)

| Door | State (the HUD's reason when locked) |
|---|---|
| `MainA` | open |
| `MainB` | open |
| `EastA` | open |
| `EastB` | open |
| `SouthA` | open |
| `SouthB` | open |
| `NorthA` | open |
| `NorthB` | open |
| `Bookstore` | locked - CAMPUS BOOKSTORE - CLOSED SATURDAY, OPEN MONDAY 8 AM |
| `Sorting` | locked - MAIL SORTING - STAFF ONLY |
| `Committee` | open |
| `CommitteeWest` | open |
| `Ballroom` | open |
| `BallroomEast` | open |
| `StudentGov` | locked - STUDENT GOVERNMENT - BACK MONDAY |
| `Honker` | locked - THE HONKER, THE STUDENT PAPER - DEADLINE SUNDAY 6 PM |
| `MeetingEast` | locked - MEETING ROOMS - RESERVED FOR THE ALUMNI BOARD |
| `MeetingSouth` | locked - MEETING ROOMS - RESERVED FOR THE ALUMNI BOARD |

## Smoke test (`-CampusInteriorsSmoke=UNI`)

Builds the inside, waits three seconds, then checks it the way the house's room smoke tests check theirs:

- geometry: 91030 triangles; 51 pieces of the house's kit placed;
- floors: a walkable floor found under 25 of 25 light anchors (the pool lights at most 6 at once);
- props: 169 carryable props from the house's furniture, 0 floating;
- doors: 18, the main ones unlocked, 6 locked and 6 of them with a reason;
- wall clocks: 4, worst hand 0.00 degrees from the clock tower's time;
- sounds: 1; real windows into built rooms: 59;
- verdict: **PASS**

## The walk (`-CampusInteriorsReview=UNI`)

Real input, in through the main doors, through every open room and up and down every stair, trying the locked doors, and back out until the inside is freed: 38 steps in this building (log in `evidence/walk_UNI.txt`). Last run, with every building of its tier: `PASS buildings=LIB,HAR,SCI,UNI,STA,TWR stops=256/256 problems=0 mainDoorsOpened=6/6 lockedDoorsHeld=1 reasonsShown=1 builtWhileInside=1 freedAfterLeaving=6/6 in...`

## References and before

- Floor plan, drawn for PLAN.md before building: [`references/fp_union_g.png`](references/fp_union_g.png)
- The site before the job (`-NoCampusLayout`, the in-game top view): [`references/site_before.jpg`](references/site_before.jpg)
- The site after (the same view): [`references/site_after.jpg`](references/site_after.jpg)

| Before | After |
|---|---|
| ![](references/site_before.jpg) | ![](references/site_after.jpg) |

## Shots (from the walk)

![](shots/UNI_1_Doors.jpg)
![](shots/UNI_2_Concourse.jpg)
![](shots/UNI_3_Atrium.jpg)
![](shots/UNI_4_GoosePit.jpg)
![](shots/UNI_5_Trophy.jpg)
![](shots/UNI_6_GameRoom.jpg)
![](shots/UNI_7_TVLounge.jpg)
![](shots/UNI_8_Committee.jpg)
![](shots/UNI_9_Ballroom.jpg)

