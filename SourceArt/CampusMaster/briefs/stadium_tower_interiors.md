# Brief: making the stadium and the clock tower enterable

Project: `/Users/Stewart/Documents/Unreal Projects/SeniorSendoff` (UE 5.8). This brief is read-only analysis; nothing in the
project was changed. Paths below are relative to `Source/SeniorSendoff/` unless they say otherwise.

**Conventions.**
- Units are cm, Z is up, and coordinates are world unless marked otherwise.
- Campus grade is -60. "rel" means height above grade, so world Z = rel - 60.
- **Bowl angle A** is the ellipse parameter the code uses (segment K spans K*9 .. (K+1)*9 degrees). A = 90 points to +Y,
  the house/road side with the press box and gates. A = 270 is the scoreboard side. A is not a compass bearing.
- **Bowl point formula:** `P(A,R,Zrel) = (2400 + 5600*R*cosA, -13300 + 3800*R*sinA, -60 + Zrel)`. Sources:
  HouseExteriorCampus.cpp:143-144, HouseExteriorCampus.h:7 and HouseCampusFallStadium.cpp:21.

---

## 0. Bottom line

**Stadium.** Today the stadium is a **sealed ring**. The interior has no door, and the space under the stands is
**empty, unlit, unfinished void**. It can be made enterable without moving anything:
- **Space.** A stepped-soffit concourse under the stands has 4.9-8.7 m of headroom (§3). On the home side the band
  between the outer wall and row 6 is 5.9-9.3 m deep.
- **Phase 1 (home side, A 36°-198°).** About 860 m² of concourse, about 270 m² of back-of-house for concessions and
  restrooms, a 190 m² home locker room in the west end zone with a team tunnel to the field at A = 180°, three
  vomitories on the existing aisles (45°, 90° and 135°), GATE 4 and GATE 5 cut open, and a press-box stair core.
- **What it needs.** A new interior actor, a shared "cut list" so the ExteriorNear and CampusFall builders leave holes,
  and a lighting answer: every exterior light is shadowless and the three stadium floods reach every interior space
  (§6).

**Clock tower.**
- **Today.** The base and shaft are a **colliding hollow shell crossed by five solid horizontal plates**. The oak door is
  a flat quad. The belfry, clock stage and spire have **no collision**. The clock stage is a closed box.
- **Proposed.** An oak open-well stair, 520 × 520 cm, can go from the door (rel 75) up to the belfry floor (rel 2786):
  11 flights of 13 risers at 18.96 cm.
- **How it stays safe.** The tower's look and `-ClockTowerReview` are preserved by:
  - turning the five plates into frames (outer faces unchanged), and
  - putting every new piece in a **separate actor**. The review requires exactly one colliding part on the tower
    actor (§8).
- **Clock room above the bells.** This is possible later (Phase B) with the house's existing climb system
  (`AHouseClubPole`). It needs a gap in the bell frame's top beams.

---

## 1. Stadium: what is built today

### 1.1 Shell (all in `ExteriorNear`, HouseExteriorCampus.cpp:142-203)

| Element | Code | Geometry |
|---|---|---|
| Outer wall | :146 `Panel(Brick, P(A,1,0), P(B,1,0), 0, 1050, N)` | 40 planar chord quads, **one-sided, facing out**. Height rel 0-1050 (world -60..990). No thickness. Chord length 598 cm (ends) to 877 cm (sides). |
| Stone bands | :146-147 | Cornice rel 1040-1110 and base band rel 30-85. Both quads 3 cm proud. |
| Window band | :147 | Each segment at fraction .27-.60, rel 420-660. `EX_Dark` (or `EX_Window` with `-NoSurroundingsPolish`). Painted on, with no opening. |
| Seat rows | :148 | Row k: R0 = .69+.024k, R1 = R0+.024, tread Zrel = 240+63k. Tread quads face up only. A **63 cm riser**, taller than the 45 cm the player can step. |
| Outward step faces | :148 `Panel(Stone, P(A,R1,0)…, Z-63, Z, N)` | At R1, rel Z-63..Z, facing **outward**. These are already the outward faces of a 63-cm-thick stepped slab (§3). |
| Field risers, podium, back wall | :150, gated on `HouseCampusFall::Enabled()` | Front riser at R0, rel Z-63..Z, facing the field. Row 0 podium at R .69, rel 0-240. Row 11 back wall at R .978, rel 933-1050. **Missing with `-NoCampusFall`.** |
| Rim deck | :151 | Quad facing up at rel 1050 (world 990), from R .98 to 1.045. Its outer .045 R **overhangs the wall** by 171 cm (sides) to 252 cm (ends). It has **no underside and no guard rail**. |
| Rim posts and rail | :152, :155 | K even. Cylinders r 4 at R 1.02, rel 1110-1230. Rail beam at rel 1230. |
| Seat benches | :156 | `Beam` 15 × 12 at Rb = .70+.024k, rel 246+63k. Purple, with gold on every 4th row. |
| Aisles (K%5 == 0: 0°, 45°, 90°, 135°, …) | :157-158 | Stone nosing strips ±.007 rad on each row, plus **one sloped handrail** from P(A,.72,340) to P(A,.98,1050). There are **no intermediate aisle steps**. |
| 20 arches (K even) | :159-166 | Stone prism `Arch(135,295)`, 12 cm thick, set 9-21 cm outside the chord. A **solid slab with no hole**, top at rel 430. Dark infill `Arch(112,295)` 2 cm thick at rel 8-415. Bars (5 × `Beam`) except at K = 8 and 10 when CampusFall is on (:165). |
| Press box | :179-182, :199-200 | Dark box: X 100..4700, Y -10000..-9250, world Z 1050..1890. Stone cap X 50..4750, Y -10050..-9200, world 1850..1920. Street glass panel at Y -9245, X 260..4540, world 1230..1700. Interior posts 40 × 30 at Y -9980 and -9270, every 520 cm, world 1090..1820. |
| Scoreboard | :189-190 | X 1550..3250, Y -17100..-16980, world 1190..2190. |
| Masts | :202 | r 20, P(A,1.04) for A = 35°, 90° and 145°, from grade to world 3840. Positions: (7171,-11033), **(2400,-9348) which goes up through the press box**, and (-2371,-11033). |
| Footprint and NoGo | :203, HouseExterior.cpp:1241 | X -3450..8250, Y -17400..-8700. Already covers the bowl and everything proposed here. **Do not add footprints**, because the tree scatter (crc `0f3b2bb7`) reads them. |

**Hidden Near geometry already inside the sealed volume.** It must be clipped where an interior is built:
- **Campus walk Y = -10300** (:135, 3 cm concrete strip). It runs under the home stands between A 52° and 128° and
  crosses the concourse band (R .834-1) at A 52°-71° and 109°-128°.
- **Walk Y = -16100** (A 227°-313°) and **walk X = 7200** (A ±31°): both in Phase 2 areas.
- **CampusLamp posts** (:137) at (7300,-11800) (R .96, A 24°) and (7300,-14600) (R .94, A 339°). They are 520 cm tall
  and sit inside the stands (Phase 2).

### 1.2 HouseCampusFall dressing on and around the shell (HouseCampusFallStadium.cpp)

- **Gates, :463-510.** Purple iron leaves open 72° outward (:445-461), with tips at ±77 cm and 123 cm out. GATE boards
  bottom out at rel 306. Hay stacks are at Out*150 ±245 and corn shocks at Out*52 ±168, all clear of a ±112 opening.
  - Gate centres (chord midpoint + 9 out): **GATE 4 (K=8) at (3705,-9608)**, chord 866; **GATE 5 (K=10) at
    (1961,-9514)**, chord 877.
  - Gate numbering is K/2, so the other arches are GATE 1-19 and K=0 is GATE 20.
  - Arch centres: K2 (7566,-11847), K4 (6652,-10834), K6 (5322,-10062), K12 (260,-9792), K14 (-1232,-10413),
    K16 (-2368,-11316), K18 (-3037,-12414), K20 (-3174,-13598).
- **Ticket booths, :488-489.** At (3330,-9030) and (1560,-8975). Hidden blocker boxes 150 × 170 × 230 in
  `CampusFallSolid` (:405).
- **String lights on the wall, :494-498.** K = 8, 9 and 10 (A 72°-99°, X 4130..1524), at rel 700, 14 cm out.
- **Wall sheets, :503-509.** At rel 755-1015 on K=7 (centre (4536,-9800)), K=9 (2838,-9523) and K=11 (1097,-9616).
  Each is 520 wide.
- **Pennant strings round the rim, :359-363.** At P(A,1.012, rel 1215).
- **Bedsheets on the podium wall, :349-357.**
- **Litter, :315-348.** 1,100 pieces on the treads, kept out of a **±2.0° band round each aisle** (:324-326). That is
  134-156 cm each side of an aisle on the long sides.
- **Field facts:**
  - The field is X -617..5417, Y -14641..-11959, turf at world -56, with goalposts at X -787 and 5587.
  - The **home (Geese) bench is on the press-box side** at Y -11529 (:220-224).
  - The track is at R .665-.693.
- **Collision.** Only `CampusFallSolid` collides: booths, hay, float deck, trailer and kiosk (HouseCampusFall.cpp:205-233,
  260-268). Goalposts, field benches, coolers, gate leaves and litter have none.

---

## 2. Collision and reachability today

- **What collides.**
  - Everything in `ExteriorNear` is `BlockAll` with complex-as-simple collision (camera ignored), built in 4000 cm tiles
    (HouseExterior.cpp:1549-1551, 1558-1567). That covers the wall, bands, arch slabs and dark infill, bars, every tread,
    riser and bench, the rim deck, posts and rail, handrails, press box, masts, scoreboard and the hidden walks and lamps.
  - `ExteriorNear` has `CastShadow = false` (HouseExterior.cpp:1567). `-CampusWoodsReview` checks this
    (HouseCampusReview.cpp:31-38).
- **The bowl is fully enclosed.** The wall is a closed 40-gon, 10.5 m tall (rel 0-1050), with no openings. The 20
  "arches" are solid 12 cm slabs with a 2 cm dark slab in front, so walking into GATE 4 or 5 stops you at the dark
  slab, about 22 cm outside the wall line.
- **Nothing outside can be climbed.** The player has radius 32 and half-height 90 (StoryCampaign.cpp:364), engine
  default MaxStepHeight 45, and JumpZ 520 (StoryMovementComponent.cpp:101), which gives a jump of about 138 cm. The
  tallest solid prop near the wall is a booth (230) or the trailer (293).
- **Nothing inside is reachable.** The field, seats, rim, press box top and the void under the stands cannot be reached.
  No review walks there. The nearest leg is `FreeRoamWalkReview`, which stops at Y -7000.
- **Inside the bowl, the stands are not walkable as built.** Rows are 63 cm, so a player needs to jump. The
  outer rim edge has no guard (a 10.5 m drop outside). Under the press box, row 11 has 177 cm of headroom and the rim
  deck 60 cm (§5).

---

## 3. Usable volume under the stands

The existing outward step faces (:148) are exactly the vertical faces of a **63-cm-thick stepped slab**. Adding one
downward-facing quad under each tread, at rel Z-63 from R0 to R1, closes the soffit with no other change. The clear
heights under that soffit are:

| Row k | R0-R1 | Tread rel / world | **Soffit (clear height) rel / world** |
|---|---|---|---|
| 0 | .690-.714 | 240 / 180 | 177 / 117 |
| 1 | .714-.738 | 303 / 243 | 240 / 180 |
| 2 | .738-.762 | 366 / 306 | 303 / 243 |
| 3 | .762-.786 | 429 / 369 | 366 / 306 |
| 4 | .786-.810 | 492 / 432 | 429 / 369 |
| 5 | .810-.834 | 555 / 495 | 492 / 432 |
| 6 | .834-.858 | 618 / 558 | 555 / 495 |
| 7 | .858-.882 | 681 / 621 | 618 / 558 |
| 8 | .882-.906 | 744 / 684 | 681 / 621 |
| 9 | .906-.930 | 807 / 747 | 744 / 684 |
| 10 | .930-.954 | 870 / 810 | 807 / 747 |
| 11 | .954-.978 | 933 / 873 | 870 / 810 |
| rim | .978-1.0 | deck 1050 / 990 | about 987 / 927 (new underside; add an outward face at R .978, rel 870-987) |

**Radial depth.** On every ray through the centre, one R unit is `hypot(5600cosA, 3800sinA)` cm long.

| A | cm per R | One row (.024) | Whole stand (.69-1) | **Concourse band (.834 → wall - 45 cm)** | Back-of-house (.762-.834) |
|---|---|---|---|---|---|
| 0° / 180° (end zones) | 5600 | 134 | 1736 | **885** | 403 |
| 45° / 135° | 4785 | 115 | 1483 | **749** | 345 |
| 76.5° (GATE 4) | 3919 | 94 | 1215 | **606** | 282 |
| 90° (press box, centre) | 3800 | 91 | 1178 | **586** | 274 |

**Areas** (floor at grade, under the soffit):
- Full concourse ring: 1,907 m². Perimeter is 248 m at R .834 and 298 m at the wall.
- Phase 1 arc (36°-198°): **857 m²** of concourse, **269 m²** of back-of-house (36°-162°) and **190 m²** of locker room
  (162°-198°, R .762-.93).

**What fits where.**
- **R .834 → wall (rows 6-11 plus rim):** clear 555 → 987 cm. This is the concourse: a lofty, stepped-ceiling
  promenade, 5.9-8.9 m wide.
- **R .762-.834 (rows 3-5):** clear 366-492, 274-403 deep. Kitchens behind concession counters and restrooms.
- **R .714-.762 (rows 1-2):** clear 240-303. Service corridors, storage and plumbing chases.
- **R .69-.714 (row 0):** clear 177. Sealed, except where a tunnel cuts through it with a locally thinner slab.

---

## 4. Recommended stadium layout: Phase 1 (home side, A 36°-198°)

### 4.1 How to build it

1. **New actor `AHouseStadiumInterior`.**
   - Transient and local on every machine, like `AHouseCampusFall`.
   - Spawn it beside the CampusFall actor at HouseExterior.cpp:2177-2178, **with no Owner**. `HouseWoodsPass::Audit`
     counts point lights on actors owned by the exterior (HouseWoodsPass.cpp:54-63).
   - Parts:
     - **Shell:** soffit undersides, outer-wall inner skin, inner walls, floors and bulkheads. Complex-as-simple
       BlockAll, camera ignored, no shadows.
     - **Rooms and props:** visual only, culled at about 6000-8000 cm.
     - **Doors:** server-spawned replicated `AHouseBlackHallDoor` (HouseBlackHall.h:28-39). Use the Exit style with
       `KickPlate | Closer | Sign` flags for locker room, restroom and press doors.
     - **Kickable cups and programmes:** `AHouseBlackHallLitter` (HouseBlackHall.h:55).
   - Keep interior geometry **out of `ExteriorNear`**, so `-CampusWoodsReview`'s Near rules (Y < -5550, no shadows)
     and vertex counts stay as they are.
2. **A shared cut list**, for example `HouseStadiumCuts.h`, holding the gate openings, vom trenches, aisle stairs, press
   core and press-box volume. It is read by:
   - `HouseCampus::BuildCampus` (HouseExteriorCampus.cpp:145-168), which splits the wall, arch and infill at K = 8 and
     10, splits treads, risers and benches at the voms, re-routes the aisle handrail around the trench, and skips the
     walk strip inside the concourse;
   - `HouseCampusFallStadium.cpp`, for the litter skip (widen ±2.0° to about ±2.5° at 45° and 135° for a 300 cm vom),
     the K=11 sheet and the rim pennants inside the press box;
   - the interior actor.
3. **Gate it.** Respect `HouseCampusFall::Enabled()`. With `-NoCampusFall` the field risers, podium and back wall do not
   exist (:150), so either build them in the interior actor or skip the interior.

### 4.2 Zone plan (angles are segment boundaries; world corners from P(A,R))

| Segment K (A) | Zone | Radial band | Notes / world anchors |
|---|---|---|---|
| bulkhead at 36° | Closed end of Phase 1 | R .834 → wall | Masonry wall with a chained gate ("SECTIONS 1-6 CLOSED"). (6178,-11437) → (6930,-11066). |
| 4 (36-45) | **Restrooms 1 (MEN / WOMEN)** | .738-.834 behind the .834 wall | Doors 120 wide on the R .834 line. Rooms 345-460 deep; ceilings 303-492. |
| 5 (45-54) | **Vom V1 at the 45° aisle** | stair .834-.876; trench .762-.834 | §4.4. Portal at (5702,-11059). Trench to (5417,-11253). |
| 6-7 (54-72) | **Concession C1** | counter on .834; kitchen .762-.834 | Arc about 1500 cm at the counter. Roll-up shutters and menu boards. |
| 8 (72-81) | **GATE 4 vestibule** | R .97 → wall | Turnstiles and ticket-taker. Opening at (3705,-9608). |
| 9 to 10 (83.6-96.4) | **Vom V2, the grand stair at 90°** | stairs .834-.876 | Two flights rise toward 90° from both gates (§4.4). Portal at (2400,-10131). Trench to (2400,-10404). |
| 10 (90-99) | **GATE 5 vestibule** | R .97 → wall | Opening at (1961,-9514). |
| 11 (99-108) | First aid / souvenir stand (programmes, pennants, purple cushions) | .762-.834 | **Press core is outside the wall here** (§5). |
| 12-13 (108-126) | **Concession C2** | as C1 | |
| 14 (126-135) | **Restrooms 2** | .738-.834 | |
| 15 (135-144) | **Vom V3 at the 135° aisle** | as V1 | Portal at (-902,-11059). |
| 16-17 (144-162) | Band and equipment room; trainers' room | .762-.93 | Near the 145° mast (outside the wall). |
| 18-21 (162-198) | **Home locker room, coaches' office, showers** | .762-.93; service corridor .714-.762 | §4.6. Corners (-1658,-12405), (-2553,-12208), (-1815,-13753), (-2744,-13853). |
| bulkhead at 198° | Closed end of Phase 1 | .93 → wall | (-2553,-14392) → (-2926,-14474). |

- **Concourse.** Runs the full arc at R .834 → wall - 45 cm on the home sides. Behind the locker room it narrows to
  R .93 → wall, which is **347 cm** wide.
- **Other arches in the arc** (K = 4, 6, 12, 14, 16, 18, 20). Recommended: cut them open behind their existing bars.
  The bars collide, so the result is an open arcade showing the lit concourse. Outside Phase 1, keep the dark slabs.
- **Outer-wall window band.** Rel 420-660 already lies inside the concourse height, so it can become real glazing using
  the `Glass` key (`M_House_Glass`, HouseBlackHallLook.cpp:820-827).

### 4.3 Gates (K = 8 and 10)

- **Wall cut.** Replace the segment's brick panel with two piers and an arch head. The opening is the infill outline:
  **±112 wide, rel 8-415** (world -52..355). The floor is at grade, with no step.
- **Arch frame.** The stone prism becomes a band 23 cm wide (radius 112 → 135) on two jambs. Add 45 cm reveals through
  to the inner skin.
- **Remove the dark infill** (:164) for K = 8 and 10.
- **Clearances.** The open leaves leave about 143 cm clear between their tips. GATE boards hang at rel 306+. Both fit
  a 64 cm capsule.
- **Vestibule.** From R .97 to the wall: 2 three-arm turnstiles (bodies collide, arms do not) plus a push gate.
- **Wall dressing.** The string lights (rel 700) and sheets (rel 755-1015) are above the opening, so nothing on the wall
  has to move.

### 4.4 Vomitories and aisle stairs

On the long sides the stands are too shallow (11.8 m) for a classic radial vom stair. Use **flights that run along the
concourse inner wall** (tangential), then a trench.

1. **Tangential stair** in the band R .834-.876 (160 cm on the sides, 201 cm at 45°). It rises from the concourse floor
   (rel 0) to **rel 303**: 17 risers × 17.8 cm, 16 goings × 28 cm = 448 cm, about 5.4° of arc at 90°.
   - Clearance under rows 6-7 (soffits 555 and 618) is **252-315 cm**.
   - At 90°, V2 uses two mirrored flights, from 83.6° and 96.4°, meeting on a landing at 89°-91° (about 167 cm). Each
     gate feeds one flight.
2. **Portal** in the R .834 wall: 300 wide, rel 303-523, under row 6 (soffit 555).
3. **Trench**, open to the sky, from R .834 to R .762 (274 cm at 90°, 345 at 45°). The floor stays at rel 303, which is
   **row 2's tread level**.
   - It cuts rows 3, 4 and 5 for ±1.8° at 90° (±2.25° at 45° and 135°).
   - The cut risers become side walls 126, 189 and 252 cm tall. Add pipe rails.
   - Clip the treads, risers, benches and the aisle handrail; re-route the handrail onto two side rails. Clip the
     litter too.
4. **Exiting the trench.** You come out on row 2 (world Z 243). Aisle steps on the same aisle (K%5 == 0) split each
   63 cm row into **two risers of 31.5 cm** (goings: 45.6 at 90°, 57 at 45°, 67 at the ends).
   - At row 11 → rim (117 cm), use **3 risers of 39 cm**.
   - Add a 107 cm guard rail on the rim deck's outer edge at R 1.045. There is none today.
5. **Why it cannot exit higher.** Exiting at row 3 or above would leave less than 210 cm under row 6's soffit.

### 4.5 Concessions and restrooms

- **Counters.** On the R .834 line, with kitchens behind in R .762-.834 (ceilings 366-492).
- **Restrooms.** Use R .738-.834 (ceilings 303-492), with trough urinals and painted MEN and WOMEN signs. Doors are
  `AHouseBlackHallDoor` with closers.
- **Concourse finish.** Use what the house already has:
  - painted CMU (`HallBuildCmu`, HouseBlackHallLook.cpp:85);
  - worn concrete `MI_SSO_S03WornConcrete` (:828);
  - panel materials for signs (`HallMakePanelMaterial`).
- **Period detail (1986):** "NO CHEERING IN THE PRESS BOX", a hand-lettered menu (HOT DOGS 1.25 · POPCORN .75 · COKE .75),
  and purple seat-cushion stacks.

### 4.6 Home locker room and team tunnel (west end zone)

- **Room.** A 162°-189°, R .762-.93: about **941 cm deep and 1,400 cm wide** (190 m²). The ceiling steps up
  rel 366 → 744 (world 306 → 684), which suits a 1930s concrete locker room.
- **Service corridor.** Along the front, R .714-.762 (240-303 clear).
- **Team tunnel at A 180°.** From R .762 (X -1867) to the podium face at R .69 (X -1464). 403 cm long, **Y -13480..-13120
  (360 wide), rel 0-225**. Under row 0 (tread 240) the slab is only 15 cm.
  - It comes out on the track behind the CALDER end zone: end line X -617, goalpost X -787.
  - The home bench is at Y -11529, a short run up the sideline.
- **Team entrance from outside.** GATE 10 (K=20, (-3174,-13598)) opens into the concourse straight behind the locker
  room.
- **Field collision.** Once the field can be reached, add simple blockers for the goalposts and team benches. Today
  they are in `CampusFallField` with no collision.

### 4.7 Player routes

- **Seats:** gate path (ends at (2400,-8930), HouseCampusFallQuad.cpp:202) → open leaves → GATE 4 or 5 → vestibule
  turnstiles → concourse → V2 grand stair (rel 0 → 303) → portal → trench → row 2 → aisle steps up to row 11 → rim
  promenade.
- **Field:** concourse west → locker room → tunnel → track and field. The way back is the tunnel. You can also drop
  240 cm off the podium (there is no fall damage).
- **Press box:** §5.

### 4.8 Phase 2

Mirror Phase 1 on the scoreboard side:
- the visitors' locker room in the east end zone (A 342°-18°), with the tunnel at 0° (X 6264);
- voms at 225°, 270° and 315°;
- C3 and C4, plus restrooms.

This needs the lamp posts and the X 7200 and Y -16100 walk strips clipped (§1.1). The scoreboard (rel 1250+) does not
conflict.

---

## 5. Press box

- **Volume.** Outer X 100..4700, Y -10000..-9250, world Z 1050..1890.
  - Interior with 20 cm walls and slab: **X 120..4680, Y -9980..-9270 (4,560 × 710), floor world 1070 (rel 1130)**.
  - It covers A 61.8°-118.2° at its front (R .868, row 7) and A 65.8°-114.2° at the wall. It overhangs the wall by
    250 cm at X 2400 and 584 cm at its ends.
  - Its underside is 60 cm above the rim deck, so the **rim promenade is cut at the press box**. Row 11 underneath has
    177 cm.
- **Existing openings and dressing.**
  - Field windows are an emissive quad at Y -10001.5, X 300..4500, **world 1180..1430**, which is 110-360 above the new
    floor. Mullions every 300 cm (HouseCampusFallStadium.cpp:365-369).
  - The CALDER STATE STADIUM sign is at world 1474..1749. Bunting hangs below the windows.
  - On the street side: glass at world 1230..1700 and a sign at world 1310..1610 (:388).
  - For a view out, open the box's field face at the window band and replace `EX_Window` with `Glass`. From outside the
    interior then has to read as lit, using emissive fluorescent panels.
- **Existing geometry that would show inside.** Clip it or keep it as structure:
  - rim posts at A 72°, 90° and 108°: (4165,-9614), (2400,-9424), (635,-9614), world 1050..1170;
  - the rim rail beam at world 1170;
  - **rim pennant strings at R 1.012** (world about 1123-1155);
  - **the 90° mast (r 20, at (2400,-9348))**: keep it as a column in the rear corridor;
  - the 40 × 30 metal posts at Y -9980 and -9270: keep them as wall columns.
- **Plan (1986 college press box).**
  - A rear corridor on the street side (Y -9520..-9270).
  - Along the glass, from west to east:
    - radio booths X 120-1300 (two booths: "Geese Radio Network" and visitors);
    - visiting coaches X 1300-1900;
    - PA announcer and scoreboard operator X 1900-2900, centred on the 50 at X 2400;
    - a two-tier working-press row X 2900-4100, with counter at about rel 1205 and the rear tier 45 cm up (typewriters
      and rotary phones);
    - home coaches X 4100-4680.
  - The roof deck (camera platform) could later be reached through the club-pole climb (§9.4).
- **Access (recommended): an external stair core under the street-side overhang.**
  - **X 600..1500, Y -9540..-9250, grade → world 1070.**
  - Switchback: 7 flights × 9 risers × 17.94 cm, 28 cm goings, 2 × 120 lanes plus a 20 cm wall. Optionally an
    "OUT OF ORDER" lift.
  - Door on its east face (the W booth's back is 190 cm in front of its +Y face). It rises into the press box's rear
    corridor through a floor opening.
  - The wall line here is at Y -9549 (X 1500) to -9702 (X 600). The overhang covers it, and it visually supports the
    floating box.
  - **Conflicts:**
    - the **K=11 wall sheet** (centre (1097,-9616), rel 755-1015): move it to the core's +Y face or to K=13;
    - the rim-deck overhang, which needs a local cut;
    - the string lights stop at X 1524, so they are clear.
  - Free wall stretches if the sheet stays: X 395..785 (390 cm) only. The other gaps are under 220 cm, or crossed by
    the K = 8-10 string lights.
- **Alternative: an internal core** under the press box at K=11, R .875-.99, about 600 × 420. It rises through rows 8-11
  and the rim deck into the press box floor. It is reached from the concourse, but it cuts seats and can be seen from
  the field as a block under the box.

---

## 6. Lighting and rendering constraints (stadium and tower)

- **No exterior light casts shadows** (HouseExterior.cpp:2019). Near tiles do not cast shadows either, and the dusk sun
  has shadows off (HouseDuskAtmosphere.cpp:155).
- **The three stadium floods reach every interior space.** They are 60,000 lm, radius 6500, at (5501,-11957),
  (2400,-10861) and (-701,-11957), world 3690 (HouseCampusFallLook.cpp:383-390).
  - Distances: concourse at 90° is 3,738 cm away; press box centre 2,558; locker room 4,038.
  - Floors and inner walls under the stands would be lit through the seats.
  - **Fix:** put interior meshes on **lighting channel 1 only**, so the exterior lights (channel 0) skip them.
  - The skylight and Lumen GI ignore lighting channels. Procedural meshes have no mesh SDFs (`r.Lumen.TraceMeshSDFs=0`,
    Config/DefaultEngine.ini:20), so expect some sky ambient inside. Test with captures and tune using darker interior
    albedo or a local post-process volume.
- **Lights budget (needs the user's decision).** The rule is "≤ 53 exterior lights, no new outdoor point or spot
  lights". Interiors can be:
  - **(a) emission-only:** fluorescent panels, caged bulbs and lit menu boards; or
  - **(b) emission plus a few shadowless interior lights** on channel 1. Radius about 600 or less, as in the Black Hall
    (HouseBlackHallBuild.cpp:449-474). Proximity-gated, and on an actor **not owned by the exterior**, so
    `-WoodsPassReview` and `-ClockTowerReview` counts stay unchanged.
  - `AHouseExitSign` carries its own 95 cm point light (HouseExitSign.cpp:21-26), so it counts as a light.
  - (b) looks like the house interiors; (a) is the strict reading of the rule. **Ask the user.**
- **Materials are effectively one-sided.** The code draws both faces wherever both must read: the louvres "both sides"
  at HouseClockTowerMesh.cpp:171-182, and `Both()` at :896-900. Every interior needs **inward-facing skins**. The outer
  wall, press box and clock-stage faces are invisible from inside today.
- **Interior views.** New ExViews must use the `detail_` prefix, or they affect planting (README; HouseExterior.cpp:231-245).

---

## 7. Clock tower: what is built today

Site: (-9000,-10800), grade -60 (HouseClockTowerMesh.h:11-13). Faces: 0 = +Y (door, toward the campus walk at
Y -10300), 1 = +X, 2 = -Y, 3 = -X.

| Stage (rel) | World Z | Code | As built | Inner clear |
|---|---|---|---|---|
| Plinth 3 steps, -40..75 | -100..15 | Mesh.cpp:517-522 | Solid boxes, steps 25 cm. Top face at rel 75 is the would-be floor (`CT_StoneLit`, **emissive**). | n/a |
| Base 75..650 | 15..590 | :524-551 | Ashlar box ±330, **zero thickness**, +Y face skipped (:527). +Y face is a 60-deep wall with a pointed door. **Door opening X -9095..-8905, sill rel 75, spring 330, apex 483.** Oak fill is a single quad 36 cm in, at Y -10506 (`Door.Fill = KOak`, :528). Boards, straps and rings are in `Detail` (:538-550). | ±330 (±270 on the door wall), **rel 75 → 618: 543 cm** |
| Base course 618..664 | 558..604 | :532 | **Solid Square ±346: a plate across the whole interior** | n/a |
| Shaft band 1, 660..1440 | 600..1380 | :556-561 | Four 40 cm walls (`Wall`, inner faces at ±260, brick `CT_Brick0`, **emissive uplight ×1.2**). Lancet per face: ±32, sill 880, apex 1312, fill at Dist 282 (+X face lit amber). | ±260 (520 × 520), 776 cm |
| String course 1440..1462 | 1380..1402 | :574 | **Solid plate ±318** | n/a |
| Shaft band 2, 1462..2240 | 1402..2180 | :562-570 | 40 cm walls (`CT_Brick1`, emission ×.55). Twin lancets at ±62 (±26), sill 1680, apex 2118, fill at 284 (face 0, left lancet lit). | 520 × 520, 778 |
| String course 2240..2262 | 2180..2202 | :575 | **Solid plate ±316** | n/a |
| Shaft band 3, 2262..2740 | 2202..2680 | :572 | **Solid box ±300** (`CT_Brick2`, zero thickness) | 600 × 600 hollow, 478 |
| String course 2740..2780 | 2680..2720 | :576 | **Solid plate ±324.** Its top at rel 2780 is the belfry floor collision today. | n/a |
| Belfry 2780..3600 | 2720..3540 | :616-663 (Upper) | 45 cm walls (inner faces ±255). Arch per face: ±105, sill 2860, apex 3408, balustrade at Dist 270-286, rail 2948-2958. Louvred lancets at U ±200 (±34), sill 2880, apex 3293. Floor `CT_Interior` Square ±260 at 2770-2786. Oak frame: posts (±100,±150) to 3250, corner posts (±228,±228) to 3330, **tie beams Y ±150 at 2900-2916 (114-130 above the floor)**, top beams at ±190/±228, rel 3285-3344. | **510 × 510 × 814** (floor 2786) |
| Bells | Bells start at world 3044 | Mesh.cpp:1016-1059 | Hour bell: pivot (0,0,3236), Ø165, body 3104-3226, rim r 83, hammer on its -Y side. Quarter bells at (±190,±190, 3292), Ø58-92, bottoms 3218-3246, hammers facing the centre. | n/a |
| Clock stage 3600..4400 | 3540..4340 | :665-711 (Upper) | **Closed box ±312** (`CT_ClockStone`, box-mapped UV). Dials: centres at rel 4000 (world 3940) on each face, r 185, bezel to 199, copper spandrels to a ±214 square, stone surround ±214..234. Hands outside at Dist 317.6 / 321. **There is no dial back and no movement.** | 624 × 624 × 800 (hollow shell) |
| Cornice, parapet, spire 4400..6750 | 4340..6690 | :685, :713-760 | No parapet walk: the spire's bellcast eave (292) meets the parapet (306). | Not usable |

- **Solid or hollow?**
  - It is a hollow shell, but **not walkable**. The door is a fixed collision quad.
  - Inside, the five full plates (:532, :572-576) and the base box's top at rel 650 stop any vertical route.
  - The inner faces of shaft bands 1-2 glow with the exterior uplight emission.
- **Collision.** Only `ClockTowerSolid` collides: plinth, base, shaft and string courses (HouseClockTower.cpp:202).
  Upper (belfry, clock stage, spire), Detail, Dials, hands, bells and so on have none (:203-240). From the belfry floor
  a player could jump over the 74-cm-high arch sill and fall about 28 m.
- **What the review cameras see** (HouseClockTower.cpp:846-851, the same as the ExViews at HouseExterior.cpp:231-235):
  - **Belfry:** from (-7700,-10480,3060), looking in through the **+X arch toward -X and -Y**. It sees the -X wall, the
    hour bell and the -Y half.
  - **Mid and Far:** looking through the +X and +Y arches toward -X and -Y.
  - **So the +Y wall's inner face is hidden from every review view.**

---

## 8. What `-ClockTowerReview` checks (HouseClockTower.cpp:873-897, 946-947)

The review counts only `AHouseClockTower::Meshes` (parts made by `MakePart`, :152-172):
- `Parts > 20` (currently 29);
- **`Collides == 1`**: exactly one colliding part;
- **`Bad == 0`**: `ClockTowerSolid` must collide, nothing else may, and no part casts a shadow;
- `Culled >= 1`;
- **`TowerLights == 0`**: no `ULocalLightComponent` on the tower actor;
- `AHouseExteriorDetail::Lights` in 0..53;
- the hands read back within 5 s;
- the hour chime: `ChimesFinished >= 1`, hour count correct, strikes > 0, audio heard when sound is on;
- 7 screenshots (no pixel test).

**So:**
- Put every new collider, light and prop on a **new actor** (for example `AHouseClockTowerInterior`, spawned at the end
  of `AHouseClockTower::Build` or at HouseExterior.cpp:2177).
- Changes to the existing meshes must stay inside `ClockTowerSolid` (still the only collider) and Upper/Detail (still
  non-colliding).
- Never add colliding or shadowed parts through `AHouseClockTower::MakePart`.

---

## 9. Recommended clock tower interior

### 9.1 Look-preserving edits inside the existing tower meshes

Outer faces stay identical. UVs are world-projected (`BrickUV` and `StoneUV` are `FHallUV::World`) or box-mapped with the
same frame.

- :527 base Square: also skip +Z and -Z (`1<<2 | 1<<4 | 1<<5`). The top face is hidden under the base course and the
  bottom face is coincident with the plinth top.
- :532 base course, :574 and :575 string courses: **frames with a ±260 hole** (four boxes). The visible ledge rings
  (300..346, 300..318, 300..316) are unchanged.
- :572 shaft band 3: replace the zero-thickness box with **four 40 cm slabs** at Dist 260..300. Use the `Wall()` helper
  with no openings, the same way bands 1-2 are built.
- :576 string course under the belfry: keep it **solid except a slot** at rel X -165..165, Y 165..260 (world
  X -9165..-8835, Y -10635..-10540). Its top (rel 2780) stays the belfry floor collision. Its underside (rel 2740)
  becomes the stair shaft's ceiling.
- :649 belfry floor (Upper): the same slot.
- :528 door: drop the rectangular part of the oak fill (sill to spring). Keep only the pointed tympanum (rel 330-483).
  `Wall()` needs a "fill above spring only" option. Move the boards, straps at Z 135/255 and ring pulls (:538-550) onto
  the door leaves.

### 9.2 New actor `AHouseClockTowerInterior` (local, no Owner, not in the tower's `Meshes`)

- **Door.**
  - A replicated pair of oak leaves, each 95 × 255 (rel 75-330), swinging inward. Use the `AHouseBlackHallDoor` /
    `AExistingHouseDoor` pattern with a new plank style.
  - The closed door must match `detail_clocktower_base` (HouseExterior.cpp:235).
  - Ivy is outside U ±150, so it is clear of the leaves.
- **Linings.** Interior brick (no emission, sooty) at Dist 259 from rel 75 to 2780, with pointed reveals at the lancets.
  Reuse `Wall()` with Depth 1. Add **inward-facing lancet glass** at Dist about 281/283: dusk-blue, with the two lit
  ones amber. Add a floor overlay at rel 75.5 to cover the emissive plinth top.
- **Open-well oak stair**, rel 75 → 2786 (world 15 → 2726):
  - **11 flights × 13 risers = 143 risers of 18.96 cm.** Goings 27.5 cm (12 per flight, 330 cm run). Width 95,
    95 × 95 corner landings, open well 330 × 330.
  - One turn is 986 cm, so headroom is never a problem.
  - Flights run along the walls in the order -Y, -X, +Y, +X and repeat:
    - **flight 1 starts at the -Y wall**, opposite the door, at Y -11060..-10965;
    - flights 3 and 7 pass over the door (rel 666+ underside, against an apex of 483);
    - **flight 11 runs along the +Y wall** (Y -10635..-10540), rising from rel 2540 through the slot onto the belfry
      floor at the +X/+Y corner.
  - After the top step, turn toward -Y into the +X strip of the belfry (X 110..255, 146 wide). The corner post at
    (+228,+228) blocks the corner itself.
  - **Collision:** box treads, landings, stringers, and a 100 cm handrail along the well (otherwise there is a 27 m fall).
  - Optional dressing:
    - caged emissive bulbs at the landings (the lit +X lancet suggests a lamp inside);
    - the clock-weight chute: a 60 × 60 timber box down one corner of the well;
    - a notice board (bell schedule, "BUILDINGS & GROUNDS — NO ADMITTANCE");
    - "CLASS OF '61" scratched in the plaster.
- **Belfry blockers.** Invisible and colliding:
  - the four wall planes at ±255 from 2786 to 3600;
  - each arch opening, sill to about rel 3100, so nobody can jump out;
  - posts, **tie beams (crouch under them: crouched height 108 is less than 114)**, and the hour bell.
  - Around the slot, a 90 cm rail at Y 165. At 90 cm it stays below the outside sight lines (about rel 2879-2887,
    135 cm inside the +Y arch). An open trapdoor lid against the wall is also fine.
  - Optional: a 45 cm timber step at each arch, so the eye (about rel 2985) clears the balustrade rail (2958).
  - Placed this way, nothing new appears in the Belfry, Mid, Far, Face or Base review views.
- **Sound.** Standing in the belfry the player is inside the bells' 25 m inner radius and the house-footprint duck does
  not apply, so the chimes play at full level. That is the point of the climb.

### 9.3 Clock room (Phase B, optional)

- **Hollow the clock stage** (:667) into four 40 cm slabs (Dist 272..312), using the same `BoxUV` frame.
  - Cut a **±214 square** behind each dial. Outside, the dial glass, bezel and spandrels already cover that area, so the
    exterior is unchanged.
  - Add inward-facing dial backs: opal discs with lower emission and mirrored iron tracery and numerals (reuse `Dials()`
    with N flipped).
  - Add a flatbed turret movement in the centre, leading-off rods, bevel boxes at each dial, and hammer wires down
    through the floor.
  - Interior volume: **544 × 544 × 800** (floor about rel 3620, ceiling under the cornice at 4400).
- **Hands.** Interior hand silhouettes would need new arrays driven from `UpdateHands`. Do not touch `HourHands` and
  `MinuteHands`, which `ReadHands` checks.
- **Access.** The project has no ladder movement, but **`AHouseClubPole`** (HouseClubPole.h:34-63) is a replicated
  climb with "E CLIMB UP / SLIDE DOWN" prompts and swept-capsule route checks (`ClimbSeconds` 2-9).
  - A ladder variant could carry the player from the belfry floor up through a trapdoor (834 cm).
  - **However,** the bell frame's top perimeter beams (±190/±228, rel 3285-3344) block every vertical path.
  - The route needs a gap in the +Y top beams at X 140..240, behind the solid pier beside the +Y arch. A vertical
    capsule path near Y about 210 must also clear quarter bell 0 (X 161..219, bottom rel 3246).
  - Until then, show a fixed iron ladder to a padlocked trapdoor.

---

## 10. Verification plan

Run on every pass:
- the reviews `-ClockTowerReview -ClockChimeEvery=20` (sound on), `-CampusWoodsReview`, `-SurroundingsPolishReview`,
  `-ParkingLotWalkReview`, `-WoodsPassReview`, `-FreeRoamWalkReview` and `-FraternityBoundaryReview`;
- the log check `WOODS_PASS_ORIGINAL_TREES … crc=0f3b2bb7` (it changes only if footprints or NoGo change);
- the light check: `actualExteriorPointAndSpotLights` and `exteriorLights` both ≤ 53.

New opt-in reviews, modelled on the walking legs in `ParkingLotWalkReview` and `FreeRoamWalkReview` and on
`HouseStairSurvey.cpp`:
- **`-StadiumInteriorReview`.** Walk these legs:
  - (2400,-8930) → GATE 5 → concourse → V2 → row 2 → aisle to row 11 → rim;
  - concourse → locker room → tunnel → field (-617,-13300);
  - grade → press core → press box (1070).

  Assert no collision stall, feet Z on each floor, capsule sweeps through the gate openings and portals, and no hits
  against hidden walk strips.
- **`-ClockTowerInteriorReview`.** Walk the door → 143 steps → belfry floor (rel 2786). Ring `ClockTower.Chime 4` while
  the player is in the belfry. Confirm the tower review's static checks are unchanged.
- **Captures** with `-ExteriorViewFilter=detail_stadium,detail_clocktower`, plus new `detail_stadium_concourse`,
  `detail_stadium_vom`, `detail_pressbox_inside`, `detail_clocktower_stair` and `detail_clocktower_bells_inside`.
  Compare the existing views pixel for pixel to confirm the look is unchanged.

---

## 11. Risks and open questions

1. **Interior lighting.** Shadowless floods and skylight leak into procedural interiors; the lighting-channel fix is in §6.
   The lights budget question (§6) needs the user's call.
2. **The stands are shallow on the long sides.** Vom exits are limited to row 2 (rel 303). Tangential stairs narrow the
   concourse locally to about 430 cm.
3. **Cuts in shared dressing.** Treads, risers, benches, handrails and litter at the voms; the K=11 sheet and pennant
   strings at the press box. These cross `HouseExteriorCampus.cpp` and `HouseCampusFallStadium.cpp`, so a single cut
   list is essential.
4. **`-NoCampusFall`** removes the field risers, podium and back wall. The interior then needs its own, or must be
   disabled.
5. **Stands not designed for people.** Row height (63), no rim guard rail, 177 cm under the press box at row 11, and
   goalposts and benches with no collision once the field is reachable.
6. **Clock room access** requires changing the bell frame (visible through the arches) and adapting `AHouseClubPole`.
   Treat it as optional.
7. **Codex shares the project.** Check recent modification times under Source/, Config/ and Content/ before editing,
   and never save into `Chapter01_House`.
