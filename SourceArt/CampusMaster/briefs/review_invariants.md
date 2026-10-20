# Review invariants for the campus master job (brief: review-invariants)

Project: `/Users/Stewart/Documents/Unreal Projects/SeniorSendoff` (UE 5.8). All paths below are relative to `Source/SeniorSendoff/`
unless stated. Coordinates are world cm, Z up. The campus/front grade is `GF = -60` (`HouseExterior.cpp:106`). This was a
read-only study: nothing in the project was edited, built or run.

**Offline proof tool:** `briefs/treesim.py` is a pure-Python replica of every `FExBuilder::Rng(20261009)` consumer. It
reproduces all 232 trees that `Plants()` writes into `TreeA`/`TreeB` (108 big and 124 small), in order. Positions, yaw and
scale match the evidence CSV
`SourceArt/WoodsPass/evidence/*/Saved/WoodsPassReview/original_trees.csv` (md5 `fa8c6305c4adb31a9a0bad91b9eab17c`) with
0 mismatches. CRC-32 of that CSV re-encoded as UTF-16LE gives `0f3b2bb7`, so that file is the crc.
- Run it with `python3 -I treesim.py --report`.
- To test a planned change, write a spec like `_whatif_example.json` and run `python3 -I treesim.py --whatif spec.json`. It prints `WHATIF crc-safe: True/False` and the first grid cell that diverges.

---

## 0. TL;DR

1. **What the crc covers.** It hashes `Site.TreeA`, `Site.TreeB` and `Site.ForestDistant`. For each tree it includes the position (Z = `Grade(X,Y)-5`), the rotator and the scale, in array order. The hash is taken at `HouseExterior.cpp:1510`, right after `FExSite::BuildAll()` and before any batching.
2. **Only two functions draw from `FExBuilder::Rng`:** `Vehicles()` (`:1323-1336`) and then `Plants()` (`:1270-1312`). Nothing in `BuildHouse`, `Terrain`, `Streets`, `Courtyard`, `Neighbours`, `Boundary` or `Lamps` draws from it. `BuildCampus` uses its own `FRandomStream(82026)`, and `RearTrees` uses `FRandomStream(731026)`.
3. **Campus geometry never feeds the stream directly.** It feeds it only through the **NoGo list**, which `Free()` reads at `:1304`. `Free()` is consulted **only for grid points with max(|X|,|Y|) ≤ 19500**. The other two gates are `NearView` (non-`detail_`/`nature_` ExViews) and `HouseFallJam::Inside`.
4. **On the campus side (Y < -5550) only 27 grid points matter.**
   - **22 blocked points must stay inside some NoGo box** (§1.5).
   - **5 planted points must stay outside every NoGo box:** (-19218.3,-19484.6), (-17084.0,-13519.8), (-8320.0,-9539.9), (-2563.9,-19063.5) and (5558.8,-8315.5).
   - The other 84 campus grid trees lie beyond the 19500 square. NoGo boxes never apply to them.
5. **The vehicle rows have a fixed draw count.** The number of iterations in each `Row(...)` call, the skip thresholds and the order of the rows are load-bearing. A row's Y position is not (verified).
6. **The safest technique has two parts.**
   - Freeze today's NoGo inputs as **legacy reservations**: the 16 padded footprints and the stadium, the walk bands, the health-lot and side-street boxes, the vehicle rows, and the non-detail ExViews. The new layout's footprints must **not** be fed to `NoGo`.
   - **Hide** trees that collide with the new geometry **at batch time** (the per-species loop at `:1686-1702`, before the Autumn split). Never edit the `Site` arrays.
   - The simulator confirms that keeping only 22 small pins in place of all campus NoGo boxes still gives the identical tree list (§1.7).
7. **CampusWoodsReview checks every `ExteriorNear*` vertex for Y < -5550 and every `EX_CampusSeats` vertex for Y < -8000.**
   - `ExteriorFar` must keep its exact name: it is built with `Make()` and must not be tiled. It needs ridges at Y > 30000 and no brick at Y > 0.
   - Near and Far must not cast shadows.
   - It counts vertices globally, with a region predicate. It does not count per region or per tile.
8. **Lights are exactly at the cap of 53** (`LightSpecs`), so there is no headroom.
   - ParkingLot and ClockTower check `E->Lights <= 53`.
   - WoodsPass counts real point and spot components on the exterior actor **and on any actor whose Owner is the exterior** (`HouseWoodsPass.cpp:54-99`).
   - The 4 side-street poles and 3 health-lot poles on the campus side are real spot lights. They can move, but none can be added.
9. **FreeRoamWalkReview walks the player straight from (3100,-5000) to (3100,-7000).** Anything solid, any drop, or any step taller than about 45 cm on that line fails the review.
10. **Today's layout has real bugs that the re-plan can fix:**
    - The side street (X 8700..10300) runs *through* campus blocks 3, 11 and 13.
    - The old boundary hedge at Y = -18800 cuts through blocks 7, 8 and 13.
    - 7 trees stand inside buildings 9, 14 and 15, and 3 stand on the side street (§1.9).

---

## 1. The tree random stream, traced exactly

### 1.1 Order of `FExSite::BuildAll()` (`HouseExterior.cpp:1338-1341`)
`Terrain(); Streets(); Courtyard(); Neighbours(); Boundary(); Lamps(); Vehicles(); Plants();`

| Step | file:line | Draws on `B.Rng`? | Adds NoGo? | Campus relevance |
|---|---|---|---|---|
| Terrain | 1119-1153 | no | no | Grass patches. Cuts out Fraternity Park Dr (`:1134` `{-E,8700,-5550,-4150}`) and the side street (`{8700,10300,-E,E}`). |
| Streets | 1155-1202 | no | **yes** (1197-1201) | Side-street asphalt (`:1162`) and sidewalks (`:1174-1177`); health-centre lot slab (`:1187`) and bay lines (`:1191`) |
| Courtyard | 1204-1236 | no | yes (1223, 1235) | House side only |
| Neighbours | 1238-1248 | no (BuildCampus has its own stream) | **yes** (1241, 1242, 1246, 1247) | All campus footprints and walk bands. Also adds 3 stadium LightSpecs (`:1243-1244`). |
| Boundary | 1250-1263 | no | no | 2.1 m hedge boxes at Y = -18800, X -18800..8650 and 10350..18800 (`:1259`), and at X = ±18800. They have collision (they are in `ExteriorGrounds`). Free to remove or move. |
| Lamps | 1314-1321 | no | no | `Pole()` → `LightSpecs` (spot lights). See §5.4. |
| **Vehicles** | 1323-1336 | **yes** | **yes** (`Car()` → `:1116`, ±500 × ±300 per car) | Health-centre rows `:1334-1335` |
| **Plants** | 1270-1312 | **yes** | yes (`:1273` view boxes) | Grid scatter |

### 1.2 Exact draw sequence
`FExBuilder::R` is `A+(B-A)*S.FRand()`, one draw (`:677`). `FRandomStream` is `seed = seed*196314165+907633515` (`/Users/UE_5.8/Engine/Source/Runtime/Core/Public/Math/RandomStream.h:115-121,362-364`). The total is **3731 draws**.
1. **Vehicles, in this order:**
   - `Row(-9000,6000,1300,Y=-5350,…,.45)` (12 iterations)
   - west lot `(-6750,-3950,500,-2150,.3)` (6) and `(-6750,-3950,500,-650,.3)` (6)
   - rear lot `(3800,6300,500,4100,.4)` (6)
   - **health lot** `(-8550,-250,500,Y=StreetY(-7050)=-8250,.35)` (17) and `(…,Y=StreetY(-4950)=-6150,.35)` (17)

   Each iteration draws one `FRand` for the skip. A car that is kept draws one more `R(-2,2)` and adds a NoGo box. Today 8 + 14 = 22 health-lot cars are kept; the X values are in §5.5.
2. **Plants:**
   1. View NoGo boxes (no draws).
   2. Shrubs at `:1278-1280`: 31 shrubs × 5 draws.
   3. Bushes at X = -3660 (18 × 1 draw) and X = 4260 (8 × 1 draw).
   4. 7 fixed `Tree()` calls (`:1289-1290`).
   5. Street rows (`:1291-1296`). For X = -18000..<8000 step 2400, skipping |X+5500| < 800, |X-3750| < 800 and |X| < 1500:
      - `R(-200,200)` then `Tree(…, Y=-3800)`;
      - `R(-200,200)` then `Tree(X+1200…, Y=StreetY(-4700)=-5900)`.

      These are the **7 far-side street trees at Y = -5900**. They are not subject to NoGo, only to NearView.
   6. **Grid** (`:1297-1309`). For X, Y = -27000..<27000 step 1900 (29 × 29 = 841 cells):
      - draw `PX=X+R(-600,600)` and `PY=Y+R(-600,600)`, then `FRand` for the chance (.16, or .42 if Chebyshev > 19500);
      - **if Chebyshev ≤ 19500 and `!Free` → continue** (the cell's remaining draws are skipped);
      - draw `bBig`;
      - if `!NearView && FallJam::Inside`, make 3 dummy draws;
      - otherwise call `Tree()`, which returns with 0 draws if `NearView`, else draws S, yaw and Z-scale (3 draws).
   7. `HouseCampus::RearTrees` (own stream, adds only at Y ≥ 4540) and `HouseWoods::FitTrees` (removes or edits only Y ≥ 1600, `HouseWoodsDetail.cpp:118-130`).
3. Grid cell counts: 594 lose the chance roll, 211 plant (103 big, 108 small), 35 are blocked by NoGo, and 1 is a FallJam dummy.

### 1.3 The NoGo list as it is today (every entry `Free()` sees)
| Tag | Box X0..X1, Y0..Y1 | file:line | Load-bearing on the campus side? |
|---|---|---|---|
| Fraternity Park Dr + verges | -32000..8700, -5900..-3800 | Exterior.cpp:1197 | Fixed anchor (sole blocker for 3 road-side points). Keep. |
| Pat Head Summitt St (side street) | 8300..10700, full Y | :1197 | Sole blocker at (9590.7,1365.4), (8823.9,5594.5) and (9113.7,14266.0), all **north** of the campus. Its campus-side point (9599.2,-7573.8) is also covered by block03. |
| East drive / rear lot / spur / west lot / court / walk / lawn / house block | various, house side | :1197-1201, 1223, 1235 | Not campus |
| Health-centre lot | -9100..100, -8700..-5700 | :1198 | **Redundant.** Each of its 3 points is also covered by a walk band, block01 or the `front_street` view. |
| Campus footprints (block + 150) | see the table below | Exterior.cpp:1241 ← ExteriorCampus.cpp:44,121-130 | Blocks 00, 04, 07, 11, 12 are sole blockers (§1.5) |
| Stadium footprint | -3450..8250, -17400..-8700 | ExteriorCampus.cpp:203 | Sole blocker for 4 points (fixed anchor anyway) |
| Frat row ×3 | (-11600..-8600), (-15550..-12250), (-18850..-15950) × about -3750..100 | Exterior.cpp:1242 ← FraternityRow.cpp:162 | Not campus |
| N–S walk bands | X ∈ {-11200,-6100,7200,12800} ±350, Y -27300..-5800 | Exterior.cpp:1246 | -6100 and 12800 are sole blockers |
| E–W walk bands | Y ∈ {-6000,-10300,-16100,-21100} ±350, X -18100..18300 | :1247 | -10300 is sole blocker for 2 points |
| Car boxes | (X±500, Y±300) per kept car | :1116 | Health-lot boxes are redundant |
| View boxes | non-`detail_`/`nature_` ExView pos ±800 | :1273 | On the campus side: `front_street` (-1100..500, -6500..-4900), `front_lawn_overview` (3800..5400, -6850..-5250), `high` (6200..7800, -8800..-7200) |

Campus blocks (`HouseExteriorCampus.cpp:121-130`, seed = array index):

| # | centre | W×D | H | style | footprint | NoGo box (+150) |
|---|---|---|---|---|---|---|
| 0 | (-14800,-8100) | 5200×3000 | 2120 | 0 | X -17400..-12200, Y -9600..-6600 | X -17550..-12050, Y -9750..-6450 |
| 1 (dorm) | (-8700,-7900) | 3800×2600 | 2540 | 1 | X -10600..-6800, Y -9200..-6600 | X -10750..-6650, Y -9350..-6450 |
| 2 | (-4850,-7000) | 2100×1700 | 1260 | 2 | X -5900..-3800, Y -7850..-6150 | X -6050..-3650, Y -8000..-6000 |
| 3 | (9600,-7700) | 3500×2500 | 1780 | 1 | X 7850..11350, Y -8950..-6450 | X 7700..11500, Y -9100..-6300 |
| 4 (dorm) | (14500,-8450) | 4300×3600 | 2540 | 0 | X 12350..16650, Y -10250..-6650 | X 12200..16800, Y -10400..-6500 |
| 5 | (-14500,-12500) | 4800×3000 | 2120 | 2 | X -16900..-12100, Y -14000..-11000 | X -17050..-11950, Y -14150..-10850 |
| 6 | (-8700,-13100) | 3600×3000 | 2960 | 0 | X -10500..-6900, Y -14600..-11600 | X -10650..-6750, Y -14750..-11450 |
| 7 | (-14800,-17300) | 4400×3500 | 1700 | 1 | X -17000..-12600, Y -19050..-15550 | X -17150..-12450, Y -19200..-15400 |
| 8 | (-9300,-18500) | 3500×4300 | 2540 | 2 | X -11050..-7550, Y -20650..-16350 | X -11200..-7400, Y -20800..-16200 |
| 9 | (-14300,-22700) | 4800×3700 | 2960 | 0 | X -16700..-11900, Y -24550..-20850 | X -16850..-11750, Y -24700..-20700 |
| 10 | (-7900,-24100) | 4200×3100 | 2120 | 1 | X -10000..-5800, Y -25650..-22550 | X -10150..-5650, Y -25800..-22400 |
| 11 | (10000,-12800) | 3000×3400 | 2540 | 2 | X 8500..11500, Y -14500..-11100 | X 8350..11650, Y -14650..-10950 |
| 12 | (15000,-14200) | 4200×3900 | 2960 | 0 | X 12900..17100, Y -16150..-12250 | X 12750..17250, Y -16300..-12100 |
| 13 | (11000,-19100) | 3300×3200 | 2120 | 1 | X 9350..12650, Y -20700..-17500 | X 9200..12800, Y -20850..-17350 |
| 14 | (15500,-22300) | 4200×3600 | 2540 | 2 | X 13400..17600, Y -24100..-20500 | X 13250..17750, Y -24250..-20350 |
| 15 | (2100,-23000) | 7300×2900 | 2120 | 0 | X -1550..5750, Y -24450..-21550 | X -1700..5900, Y -24600..-21400 |

### 1.4 NearView and FallJam
- **NearView** (`:1265-1269`) checks |dx| < 900 and |dy| < 900 against every ExView whose name does **not** start with `nature_` or `detail_`. That is 23 views (`:137-159`).
  - On the campus side these are `front_street` (-300,-5700), `front_lawn_overview` (4600,-6050) and `high` (7000,-8000).
  - The same 23 views also produce the ±800 NoGo boxes at `:1273`.
  - The comment at `:122` says detail views never participate.
- **FallJam** (`HouseFallJam.cpp:14,1372`): X 4050..8400, Y -3660..-100, house side only. A tree inside it still consumes its 3 draws, but it is dropped from the arrays. Changing the zone would change the crc if a tree crosses the zone edge.

### 1.5 Exact campus-side sensitivity (simulated and verified)
**Blocked candidates with Y < -5550.** Each must remain inside at least one NoGo box. Removing its last cover plants a tree and shifts the stream.
| Point | Covered today by |
|---|---|
| (-16148.9,-19073.7) | block07 **(sole)** |
| (-13508.7,-9442.0) | block00 **(sole)** |
| (-14268.9,-8526.3) | block00 **(sole)** |
| (-12391.8,-10496.7) | E–W walk -10300 **(sole)** |
| (-10239.1,-5667.6) | Fraternity Park Dr box + E–W walk -6000 |
| (-6227.8,-12251.6) | N–S walk -6100 **(sole)** |
| (-6368.5,-7833.7) | health lot + N–S walk -6100 |
| (-6693.3,-6666.2) | health lot + block01 |
| (-959.7,-15519.4) | stadium **(sole)** |
| (-436.3,-11539.3) | stadium **(sole)** |
| (-967.0,-6141.3) | health lot + E–W -6000 + cars healthLotNorth@-1050 and @-550 + view `front_street` |
| (1443.8,-14105.0) | stadium **(sole)** |
| (2020.0,-11202.0) | stadium **(sole)** |
| (3880.2,-16003.2) | stadium + E–W -16100 |
| (5038.1,-15807.8) | stadium + E–W -16100 |
| (7216.2,-8326.9) | N–S walk 7200 + view `high` |
| (9599.2,-7573.8) | side street + block03 |
| (11143.9,-13104.5) | block11 **(sole)** |
| (10980.5,-10042.8) | E–W walk -10300 **(sole)** |
| (13118.7,-18960.9) | N–S walk 12800 **(sole)** |
| (14391.4,-15202.9) | block12 **(sole)** |
| (15067.0,-9872.4) | block04 **(sole)** |

**Planted candidates with Y < -5550 and Chebyshev ≤ 19500.** Each must stay outside every NoGo box. Covering one removes the tree and shifts the stream.
- (-19218.3,-19484.6) Small[45]
- (-17084.0,-13519.8) Small[49]
- (-8320.0,-9539.9) Small[60]: AutumnTone orange, `CampusFallTreeBirch0Orange`
- (-2563.9,-19063.5) Small[67]
- (5558.8,-8315.5) Big[61]: AutumnTone red, `CampusFallTreeOak1Red`

Elsewhere on the map (relevant if the side street changes north of Y = -5550), these planted candidates must also stay uncovered: (11473.5,-4525.8), (15386.5,973.2), (5847.6,1221.0), plus the woods points that `--report` lists. Blocked points that rely only on the side-street box are listed in §1.3.

**The other 84 campus-side grid trees lie beyond Chebyshev 19500** (Y < -19500 or |X| > 19500). NoGo boxes never apply to them, so they stand wherever the RNG puts them, including inside buildings (§1.9). Only the 3 campus-side non-detail views could affect them, and none of those 3 is near one.

### 1.6 Which changes alter the crc, and which do not
**These change the crc:**
- Adding, removing or reshaping any NoGo box so that `Free()` flips for one of the candidates in §1.5 (or any of the 247 candidates map-wide). That includes feeding the **new** campus footprints, walks or lots into `NoGo` at `:1241/1246/1247/1197/1198`.
- Any change to `Vehicles()` iteration counts or skip thresholds: X0, X1 or Step of any row, adding or removing a row, or reordering rows. Removing the health-centre rows alone changes the crc (verified).
- Anything else that draws from `B.Rng` before or inside the grid: the shrub, bush or street-tree loops; the 7 fixed trees; the grid constants or chances; the 19500 radius; the FallJam dummy-draw logic.
- Adding, moving or renaming a **non-**`detail_`/`nature_` ExView if its ±800 box or ±900 NearView flips a candidate, a fixed tree or a street tree. (Adding one at (0,-12000) or (2400,-6330) happens to be safe today, but don't do it.)
- Changing `Grade()` for any tree position (Z = `Grade-5` is hashed; for Y ≤ 0 Grade is GF = -60, `:112-117`). Raising campus terrain by editing `Grade()` changes the crc.
- Adding, removing or reordering entries of `Site.TreeA`, `Site.TreeB` or `Site.ForestDistant` before `:1510`. That includes appending new campus trees to those arrays, or removing hidden ones from them. The species assignment `I<2||I%4==0` (`:1688`) and `I%3==2` (`:1693`) also depends on the array index.

**These do not change the crc:**
- Any geometry change in `B.Near`, `B.Far`, `B.Grounds`, `Walks` and so on (BuildCampus's own `FRandomStream(82026)` is independent), **as long as the NoGo inputs stay frozen.**
- Moving the health-lot slab, its bay lines, its poles or its cars' **transforms after `BuildAll`**. Moving the rows' Y only is also safe today (verified: +3000 cm south gives the identical result).
- Removing the health-lot NoGo alone, or splitting the side-street NoGo to Y > -5550 (both verified), provided the other legacy reservations stay.
- Adding new NoGo boxes that cover no free candidate. Verified: (-15000..0, -15000..-10000) is safe. The whole-campus box (-19000..19000, -19400..-6200) is **not** safe, because it covers (-17084,-13520).
- The tower actor, the CampusFall actor, lamps (emissive only), `Boundary()` hedges, `Lamps()` positions, Terrain cuts and `Lawn()`. None of these draw from `B.Rng` or add NoGo boxes.
- Hiding trees at batch time (after `:1510`), as the Autumn split at `:1699-1702` already does.

### 1.7 What-if results (`treesim.py`)
| Experiment | Identical to the baseline? |
|---|---|
| Drop every campus NoGo (16 blocks, stadium, walks, health lot) | **No.** It diverges at cell (-15600,-19400). |
| Drop all of those, add 22 pins of ±100 cm around the blocked points | **Yes** |
| Keep the legacy list, add a big box -19000..19000 × -19400..-6200 | **No.** It diverges at cell (-17500,-13700). |
| Keep the legacy list, add -15000..0 × -15000..-10000 | Yes |
| Move both health-lot car rows 3000 cm south | Yes |
| Drop the `healthLotSouth` row | **No** |
| Add a non-detail view at (0,-12000) or (2400,-6330) | Yes (but don't) |
| Side-street NoGo only for Y > -5550 | Yes |
| Drop the side-street NoGo entirely | **No.** It diverges at cell (9100,1500). |
| Add (-15000..-11000, -9000..-6600) and drop the health lot | Yes |

### 1.8 The safest technique (recommended)
1. **Freeze the reservations.** Add a header such as `HouseCampusLegacyReservations.h` that holds today's 17 boxes (16 padded blocks and the stadium, exactly as in the §1.3 table). Change `Neighbours()` `:1241` to loop over that frozen table instead of `Campus.BuildingFootprints`. Keep `:1246-1247` (the walk bands), `:1197` (the side street) and `:1198` (the health lot) verbatim. Each moved line gets a comment such as "frozen 2026-10-13: WOODS_PASS_ORIGINAL_TREES crc=0f3b2bb7".
   - The minimal alternative is to replace all campus boxes with the 22 pins from §1.5 (proven equivalent). The full legacy table is easier to review.
   - The new layout's footprints, walks, lots and side street go into a **separate** list, for example `FLayout::KeepClear`. That list is never passed to `NoGo()`.
2. **Keep `Vehicles()` byte-identical** (`:1330-1335`). To relocate or remove the health-centre cars, remap or drop their transforms after `BuildAll`, in the classic-car block (`:1896-1920`). Update `PaveZ` at `:1876` to the new lot. Keep total cars above 10 (49 today, 22 of them in the health lot).
3. **Hide colliding trees at batch time.** This is the same pattern as the CampusFallTree split. Inside `for(Kind)for(Variant)` at `:1686-1696`, add `if(CampusHidden(Site.TreeA[I].GetLocation()))continue;` (and the same for `TreeB`) **after** the Type/Variant test, which uses the original index `I`. Put it **before** the Autumn `RemoveAll` at `:1702`.
   - Leave `Site.*`, `OriginalWoodlandTrees` (`:1511-1513`) and the `RecordOriginalTrees` call untouched.
   - `CampusHidden()` should test only Y < -5550, so the lawn trees that `HouseTreesReview` needs can never be hit.
   - Use a margin of the trunk radius plus about 250 cm against buildings and walk edges, or a canopy radius of about 400 × scale near façades.
   - Log a line such as `CAMPUS_TREES_HIDDEN big=… small=…` as evidence.
   - `HouseNature::Scatter` (`:1798-1801`) only reads trees with Y ≥ -6150, so the 3 street-row trees at Y = -5900 should simply be kept.
4. **New campus trees** (quad and street trees) are appended per batch, as `GardenOaks` is at `:1525-1530`, `:1697-1698`. Never append them to `Site.*`. They then render in the `ExteriorTree*` batches, get hit reactions, and `AHouseCampusFall` sees them for prop clearance (`HouseCampusFall.cpp:189-201`).
5. **Verify.**
   - Offline: `treesim.py --whatif` before building.
   - In engine: run with `-WoodsPassReview`, then diff `<UserDir>/Saved/WoodsPassReview/original_trees.csv` against the evidence file (md5 `fa8c6305…`). Also check the log for `WOODS_PASS_ORIGINAL_TREES stage=3 big=134 small=136 distant=852 crc=0f3b2bb7`.

### 1.9 Today's campus trees that already collide (the hide filter can fix these)
Inside buildings, all beyond 19500, so NoGo never applied:
- Big[36] (-15273,-21294): block 9
- Small[51] (-14172,-23555): block 9
- Big[75] (14685,-21779): block 14
- Big[79] (16582,-23166): block 14
- Small[90] (14251,-23582): block 14
- Small[95] (16946,-21295): block 14
- Small[77] (3034,-21572): block 15

On the side street:
- Big[70] (9166,-21158): on the asphalt and on the E–W walk at -21100
- Small[81] (9686,-26492): on the asphalt
- Small[82] (10515,-26874): on the east sidewalk

On walks:
- Big[51] (-6255,-20865): N–S walk -6100
- Big[66] (7347,-21896): N–S walk 7200
- The 7 street-row trees at Y = -5900 sit on the edge of the E–W walk at -6000 (Y -6100..-5900). Two of them, Small[11] (-7277) and Small[13] (-2424), are 100 cm inside the health-lot asphalt (-9000..0, -8600..-5800).

The five AutumnTone trees (`HouseCampusFallLook.cpp:352-363`, matched by ±40 cm) are (-7277,-5900) red, (5559,-8315) red, (-2424,-5900) orange, (7365,-5900) orange and (-8320,-9540) orange. Keep them out of the keep-clear areas, or the homecoming colour is lost. No review checks them.

The full list of all 96 campus-side trees is in Appendix A.

---

## 2. ExteriorNear: how it is tiled and how reviews read it
- `Local(B.Near, TEXT("ExteriorNear"), true, false)` (`HouseExterior.cpp:1567`, lambda `:1558-1563`) calls `HouseSpatialMeshes(M, 4000.f)` (`HouseExteriorSpatial.h:7-53`).
  - Every triangle is clipped to a **4000 cm grid** (lines at X and Y multiples of 4000, e.g. Y = -8000, -12000, …) with interpolated UVs.
  - Tiles are named `ExteriorNear_%02d` in TMap first-touch order: material sections in insertion order, then triangle order. **Tile names renumber whenever geometry changes**, so never refer to a tile by name.
  - Tiles have collision (BlockAll, camera ignored, complex-as-simple) and no shadows. `Make()` turns off indirect and distance-field lighting for the `ExteriorNear` prefix (`:1544-1546`).
  - The log line is `HOUSE_LOCAL_EXTERIOR: ExteriorNear tiles=N shadows=0`. Other `Local()` meshes use 1600 cm tiles.
- `ExteriorFar` is built with `Make()`: one component with the exact name `ExteriorFar`, no collision, no shadow (`:1568`).
- `ExteriorGrounds` is built with `Make()`: one component, collision on (`:1566`). The campus grass, the side street, the sidewalks, the health lot and the boundary hedges all live in it.
- **CampusWoodsReview counts Near vertices globally with a region predicate. It does not count per tile or per region** (`HouseCampusReview.cpp:33-45`):
  - every vertex of every procedural component whose name starts with `ExteriorNear` must be at world **Y < -5550**, and NearVertices must be > 15000;
  - every vertex using the `EX_CampusSeats` material in Near **or** Far must be at **Y < -8000**, and SeatVertices must be > 1000. Do not reuse `EX_CampusSeats` for interior seating north of Y = -8000.
  - The clipping only adds vertices on tile lines. Since all source geometry is below -5550, the clipped vertices are too.
- These checks run only on components of the `AHouseExteriorDetail` actor. Interiors built on a separate actor (as `AHouseClockTower` and `AHouseCampusFall` are) are not checked at all, and can use shadows.

---

## 3. Review by review
| Review (flag) | What it asserts (file:line) | Could the campus job break it? |
|---|---|---|
| **CampusWoodsReview** (`-CampusWoodsReview`, t ≥ 8 s; `HouseCampusReview.cpp:27`) | §2 Near/seat rules (40-41). Far: brick, limestone or warm-brick vertices at Y > 0 must be 0 (42). Ridge materials more than 1000 vertices each, all at Y > 30000 (43). Near and Far `CastShadow=false` (36). `ExteriorForestDistant*` with no shadow or collision, more than 600 instances (48). `ExteriorTree*` instances with Y 4400..10100 and \|X\| < 7900: at least 30 (49). Court floor traces at X {-1000,0,1000} × Y {2050,2750,3400} (53). Hoops = 2, balls = 3 (51). PASS line at 129. | **Yes**, if Near geometry reaches Y ≥ -5550 (for example entrance paving onto the far sidewalk at Y -5800..-5550), if seats are used north of -8000, if Far is renamed or tiled, if shadows are turned on for Near, or if Far gets building material at Y > 0. Tree hiding on campus does not affect it. |
| **SurroundingsPolishReview** (`-CampusWoodsReview -SurroundingsPolishReview`; 54-79) | Every `ExteriorStreetDecor*` vertex is in Y < -2700, X > 8400, (X > 3450 and Y > 1900) or (X < -3700 and Y < -300) (61). Road floors are hit by a pawn trace from Z+70 to Z-120 with normal Z > .8 at (-7500,-4800,-60), (-3000,-4800,-60), (3000,-4800,-60), **(8200,-4900,-60)** and **(9550,500,-140)** (72-73). StreetVertices > 5000. At least 5 distinct car models. More than 10 car instances. Every `ExteriorCar*` HISM has at least 2 collision boxes, box[0] X in 400..600 and Y in 150..205 (66-68). Materials `EX_StreetAsphalt`, `EX_FratRoof`, `EX_DetailStone`, `EX_CarSeat`, `EX_CarPlate0` exist (70-71). | Only through the side-street probe at (9550,500), on the house side, which needs a floor; through removing too many cars; or **naming a new vehicle HISM `ExteriorCar*`** (a bus or golf cart would fail the box-size test). The campus side is always "front street" (Y < -2700), so decor there is fine. |
| **FraternityBoundaryReview** (`-CampusWoodsReview -FraternityBoundaryReview`; 80-128) | `ExteriorFraternityRow*` vertices at X < -8560 and -4150 < Y < 0, more than 10000 of them (89). When the boundary is off (the default, `HouseExteriorSiteLayout.h:23`) there must be 0 edge boxes. With `-PropertyBoundary`, 4 boxes at West -8500, East 8800, Front **-5900**, Rear 19800, and 24 of 24 blocked sweeps (123). The sweeps ignore every other actor and component (93-97). | Only if campus geometry goes into `B.FraternityRow`. With `-PropertyBoundary` the whole campus is unreachable (front wall at Y = -5900). |
| **ParkingLotReview / ParkingLotWalkReview** (`HouseParkingLotReview.cpp`) | West lot only (house side). **`E->Lights <= 53`** (62). | Only by adding exterior LightSpecs |
| **WoodsPassReview** (`-WoodsPassReview`; `HouseWoodsPass.cpp:51-107`; also writes `original_trees.csv` at 43-48) | Point and spot components on the exterior **plus every actor whose Owner is the exterior**: at most 53 (99). Batches named `ExteriorWoodsPass*`/`ExteriorAppalachian*` must follow the shadow, collision and cull rules. 3 `WoodlandHollowMist` pools. | Yes, if campus interior lights are put on the exterior or on actors it owns, or if the new code uses those name prefixes. |
| **WoodsSceneReview / WoodsWalkReview / WoodsUndergrowthReview** (`HouseWoodsReview.cpp`) | `ExteriorWoods*` props and woods ground only | No. Avoid the `ExteriorWoods` prefix. |
| **HouseTreesReview / HouseTreesNetworkReview** (`HouseCarryTiersReview.cpp:840` → `HouseTreesReview.cpp:66-82`) | Within 10 cm, an `ExteriorTree{Maple}` must stand at (2200,-2650), an `ExteriorTreeOak` at (-2350,-3250) and an `ExteriorTreeBirch` at (-1450,-3500). The trunk must block the trace. Leaf pool at most 216. | Only if the `Site` arrays are reordered (TreeA[1] is the maple) or the hide filter reaches the lawn. |
| **WestFieldReview / WestFieldWalkReview** (`HouseWestFieldReview.cpp:55-58`) | `ExteriorTree*`/`ExteriorGrass*` in X -7900..-4100, Y 1700..5900 at grade, at least 100 of them | No. (The grass loop runs in ascending X, so changing `Lawn()` at X ≥ 0 does not reach it.) |
| **HouseExteriorWalkReview** (`HouseCarryTiersReview.cpp:836` → `HouseExteriorWalkReview.cpp`) | House front surfaces. Asphalt at (-2000/0/2500, -4350, -72) and at (-5500/3750, -3700). Seven routes on the house side. Camera shots `FrontLawnOverview.png` from (4600,-6050,2400) and `PerimeterOverview.png` from (5000,-4100,2400). | No. Note a **likely failure that predates this job**: line 108 expects a component named exactly `ExteriorWalks`, but `:1570` creates `ExteriorWalks_NN` tiles. Run a baseline before blaming campus changes. |
| **HouseRearPorchReview** (`HouseRearPorchReview.cpp`) | Rear court props: 3 basketballs, a door, floor | No |
| **FreeRoamWalkReview** (`HouseFreeRoamReview.cpp:28-33`, 138-156) | Real movement input. Leg 1 goes **(3100,-5000) → (3100,-7000)**. Leg 2 goes (-7800,-4850) → (-9400,-4850). Leg 3 goes (8000,-4850) → (9600,-4850) through the side-street junction. Each leg must reach within 60 cm in 14 s with no fall (drop > half-height + 400). With `-PropertyBoundary` each leg must stop at its line. Screenshot `OnTheCampusGreen.png`. | **Yes.** Keep the line X = 3100, Y -5000..-7000 (with capsule clearance of about 40 cm either side) free of collision: buildings, kiosks, booths, fences, trunks, benches, bollards, and steps or curbs taller than about 45 cm (UE default MaxStepHeight). Keep the leg-3 path along Y = -4850 at X 8000..9600 open. The current campus lamp at (3000,-5900) clears by about 60 cm. |
| **ClockTowerReview** (`-ClockTowerReview`; `HouseClockTower.cpp:842-958`) | More than 20 parts. Exactly 1 colliding part (`ClockTowerSolid`). At least 1 culled part. 0 shadow or collision violations. 0 tower lights. **Exterior `Lights` ≤ 53** (893-897). Hands within 5 s. Chimes. Shots from Far (2400,-3750,140)→(-9000,-10800,4366) FOV 40; Mid (-420,-8100,1500); Face (-7800,-10620,3990); Belfry (-7700,-10480,3060). | Only through lights. Visually (not asserted), new buildings must not block the shots. Sight-line rule in §4. Tower plinth X -9395..-8605, Y -11195..-10405 (`HouseClockTowerMesh.cpp:517-521`): keep clear. |
| **HouseExteriorViews** (`-HouseExteriorViews [-ExteriorViewFilter=…] [-CleanViews]`; `HouseExterior.cpp:2279-2418`) | No pass or fail. Logs `HOUSE_EXTERIOR_VIEW … inside=` (30 cm sphere overlap at the camera) and FPS. Screenshots are named `%02d_<name>.png` by **array index**. Also runs the log-only `ExProbeAll` / `ExProbeSweeps` (2267-2276; all on the house side). | Inserting views mid-array renumbers every later screenshot, so **append** new views. A camera that ends up inside new geometry ruins the evidence (see §4). |
| **HouseConnectionsReview** | Screenshot `house-footprint-restored.png` from (7000,-8000,3200) (`HouseConnectionsReview.cpp:46`) | Keep that spot open air |
| **HouseLeafWindReview** | Fixed pool of 96 leaves (`HouseLeafWind.cpp:34,775`) | No |

---

## 4. ExViews: cameras on the campus or looking at it (`HouseExterior.cpp:121-246`)
| View | Pos → Target (FOV) | In the planting? | Notes |
|---|---|---|---|
| `front_street` | (-300,-5700,150) → (-300,-1010,300) | **yes** | On the far sidewalk; looks at the house |
| `front_lawn_overview` | (4600,-6050,2400) → (-250,-2800,20) | **yes** | Also the ExteriorWalkReview `FrontLawnOverview.png` |
| `high` | (7000,-8000,3200) → (0,0,0) | **yes** | Also HouseConnectionsReview. Keep anything at this XY below 31 m. |
| `lawn_toward_road`, `street_along`, `balcony_out`, `left_front` | house side; look at the road and campus edge | yes | Do not move them |
| `detail_establishing_high` / `_front` / `_lane` / `_corner` | (1300,-5800,800), (1700,-5700,280), (1800,-5050,300), (5000,-4450,165) → house | no | The first two stand on the campus edge |
| `detail_clocktower_far` | (2400,-3750,140) → (-9000,-10800,4366) FOV 40 | no | Sight line, below |
| `detail_clocktower_mid` | (-420,-8100,**1500**) → tower | no | Keep any building at this XY under 14 m |
| `detail_clocktower_face` / `_belfry` / `_base` | (-7800,-10620,3990), (-7700,-10480,3060), **(-8480,-9720,210)** | no | `_base` is at ground level beside the tower: keep it outside footprints |
| `detail_stadium_field` / `_stands` / `_scoreboard` | (6900,-13300,1350), (3300,-11800,1450), (2650,-14200,1250) | no | Stadium (fixed) |
| `detail_stadium_gate` | (2900,-5950,380) → (2500,-9500,950) | no | Looks down the green to GATE 4/5 |
| `detail_campus_quad` | (7000,-6400,500) → (3200,-9050,200) | no | |
| `detail_campus_dorms` | (-4400,-3700,1500) → (-8700,-6600,1210) | no | Frames block 1 (the dorm) with the tower behind |
| `detail_campus_kiosk` | (3150,-6020,240) → (2400,-6330,180) FOV 50 | no | On the FreeRoam line (cameras don't collide) |

- New cameras must be **appended** with a `detail_` prefix. Any other name enters the planting.
- **Clock-tower sight line (Far shot, not asserted):** from (2400,-3750,140) to the dial centres (-8688,-10800,3940) and (-9000,-10488,3940).
  - The 2D length is about 13.1–13.2 k cm, and the line rises **0.287 cm per cm** of horizontal distance d. Anything within about 400 cm of the corridor line must top out below `Z = 140 + 0.287·d`. The belfry slope is 0.220.
  - Today, block 1's corner clears the dial line by about 6.3–7.1 m and grazes the belfry line (-0.4 m). Block 2 clears by about 9 m.

---

## 5. Other couplings the new layout must carry along (not reviews, but they break silently)
1. **`HouseCampusFall` duplicates campus positions.**
   - Dorm boxes are hardcoded for blocks 1 and 4 (`HouseCampusFallQuad.cpp:357-358`), and `IsDorm(Seed==1||Seed==4)` uses the **Blocks array index** (`HouseCampusFallLook.cpp:374`). `WindowLit(I,Floor,Seed)` also uses the index. Reordering Blocks changes which hall is a dorm.
   - Lamp banners and halos repeat the lamp grid from `HouseExteriorCampus.cpp:136-137` (`HouseCampusFallQuad.cpp:157-158`).
   - Fixed props, some with hidden `CampusFallSolid` collision:
     - kiosk (2400,-6330) and the gate path from the kiosk to Y = -8930 (`:361-362`, 193-229)
     - phone booth (1950,-6045), news boxes at X 2240–2378, Y -6060, bike rack (1180,-6270) (`:364-368`)
     - ticket booths (3330,-9030) and (1560,-8975) (`HouseCampusFallStadium.cpp:488-489`)
   - Tree-dependent placement through `FirstClear`:
     - float at (5300,-8150), (5650,-7700), (4700,-8250) or (3900,-7900); trailer at (1000,-8150), (1500,-7800) or (700,-7550) (`Stadium.cpp:609-612`)
     - leaf piles at (4300,-7050) and so on; bonfire at (3750,-7700), (3900,-7350) or (6500,-7900) (`Quad.cpp:390,408`)
     - leaf area (-300..7200, -8900..-5600) (`:410`)
   - `AHouseCampusFall` reads tree positions from the `ExteriorTree*` and `CampusFallTree*` batches (`HouseCampusFall.cpp:189-201`). Hiding trees therefore moves the float, the bonfire and the piles, which is consistent behaviour. **Do not name non-tree ISMs `CampusFallTree*`.**
2. **The side street** (Pat Head Summitt St, X 8700..10300, full Y):
   - asphalt `:1162`; sidewalks `:1174-1177`; centre line `:1167`; Terrain cut `:1134`; NoGo `:1197`
   - poles at X = 8420 for Y = -15000..15000 step 3000 (`:1318`): 11 spot lights, **4 on the campus side** (Y -15000, -12000, -9000, -6000). They must move with the street, keeping the count.
   - `Lawn()` excludes X > 8300 (`HouseExteriorNature.cpp:70`)
   - `RearTrees` skips X 8300..10700 (`HouseExteriorCampus.cpp:228`)
   - curb joints at X 8575 and 10425 for Y ≥ -5900; the crossing and stop bar at X 8150–8510 (`HouseSurroundingsPolish.cpp:141` and `:155`)
   - junction road floor probe (8200,-4900); north probe (9550,500)
   - Today it passes through blocks 3, 11 and 13.
3. **Health-centre lot:**
   - slab (-9000..0, -8600..-5800) at `:1187`; bay lines `:1191`
   - 3 lot-pole spot lights at (-7000,-6700), (-2500,-7700) and (-6500,-8600) (`:1319-1320`)
   - 22 cars:
     - Y = -8250: X -8550, -8050, -7050, -6050, -2550, -2050, -1550, -550
     - Y = -6150: X -8550, -8050, -7550, -7050, -6050, -5050, -4550, -4050, -3550, -3050, -2050, -1550, -1050, -550
   - `PaveZ` rectangle `:1876`; `Lawn()` exclusion `HouseExteriorNature.cpp:71`
4. **Lights budget is exactly 53 `LightSpecs`:**
   - 33 poles (8 near the street at Y -3840; 8 on the far side at **Y -5860**, X -13500..7500 step 3000; 11 on the side street; 6 lot/parking)
   - court 4, stadium 3 (`:1243-1244`, fixed), hut 1, house 12 (`:972-1017`)
   - The stadium mast lights must remain exactly 3 from `Campus.StadiumLamps`. Interiors need a decision from the user: the review counters do **not** see lights on a separate, unowned actor, but the hard rule may still count them.
5. **Grass and leaf scatter** (`HouseNature::Scatter`, `HouseExteriorNature.cpp:1054-1093`, its own stream 202610101) reaches the campus strip Y -6150..-5825 at X 0..8100, and so grows through the existing E–W walk at -6000. Add `Lawn()` exclusions for new paving there. That is safe for reviews.
6. **`Grade()` for Y ≤ 0 is flat at -60.** Campus relief or terraces must be separate geometry. Leave `Grade()` alone (§1.6), and do not raise ground under the trees that are kept.
7. **Collision footprint of the boundary hedges** (`:1259`, inside `ExteriorGrounds`): Y = -18800 across X -18800..8650 and 10350..18800. They currently cut through buildings 7, 8 and 13. Remove or move them freely (no RNG).

## 6. Component-name prefixes the new code must avoid (reviews match on them)
| Avoid this prefix | Unless the component is |
|---|---|
| `ExteriorNear*` | Near geometry at Y < -5550 with no shadows |
| `ExteriorFar` | The single far mesh (the name must be exact) |
| `ExteriorTree*` | Real trees: they count toward CloseTrees in the woods region, and HouseTreesReview hits them |
| `CampusFallTree*` | Trees: CampusFall reads them as trees |
| `ExteriorForestDistant*`, `ExteriorWoods*`, `ExteriorWoodsPass*`, `ExteriorAppalachian*` | Woods content |
| `ExteriorCar*` | Car-sized collision (box[0] X 400..600, Y 150..205) |
| `ExteriorStreetDecor*` | Street decor (region rule) |
| `ExteriorFraternityRow*` | Frat row (bounds rule) |
| `ExteriorParkingLot*`, `ExteriorBushParking*`, `ExteriorPole` (exact, counted inside the west lot), `ExteriorWestFieldFence` (exact) | Those exact uses |
| `PropertyBoundary_*` | The boundary boxes |
| `ExteriorYardPinecone/Stick/Cup` | Picked by the close-up views |

The safest choice is a separate actor (like `AHouseClockTower`) with a `Campus…` prefix, for example `CampusHall03_Floor`.

## 7. Verification recipe (headless, Mac)
- Build, then run the map with `-RenderOffScreen` (see memory `mac-headless-ue-runs`).
- Run `-CampusWoodsReview -SurroundingsPolishReview -FraternityBoundaryReview`, then `-ParkingLotWalkReview`, `-WoodsPassReview`, `-FreeRoamWalkReview`, `-ClockTowerReview -ClockChimeEvery=20`, `-HouseTreesReview`, and `-HouseExteriorViews -ExteriorViewFilter=campus,stadium,clocktower,establishing,street -CleanViews`.
- Judge PASS or FAIL from `<UserDir>/Saved/<Review>/result.txt`.
- Grep the log for:
  - `WOODS_PASS_ORIGINAL_TREES … crc=0f3b2bb7`
  - `actualExteriorPointAndSpotLights=53`
  - `exteriorLights=53`
  - `HOUSE_CLASSIC_CARS cars=`, which must stay above 10
  - `HOUSE_EXTERIOR_VIEW <name> … inside=`, which must be empty for the campus cameras
- Diff `original_trees.csv` against the evidence file.

---

## Appendix A: all 96 trees on the campus side (Y < -5550), from the evidence CSV
- "scaleXY" is the site-array scale. TreeB batches multiply it by 0.71 (maple) or 0.82 (birch) at render time (`:1695`).
- Batch: TreeA Type = (I<2 || I%4==0) ? Maple : Oak. TreeB Type = (I%3==2) ? Maple : Birch. Variant = I%3.
- "grid≤19500" rows are the only ones NoGo can affect. "street-row" rows depend only on NearView.

| array[idx] | X | Y | scaleXY | batch | source |
|---|---|---|---|---|---|
| Big[5] | -26857 | -19838 | 1.01 | ExteriorTreeOak2 | grid>19500 |
| Big[6] | -26927 | -7493 | 1.11 | ExteriorTreeOak0 | grid>19500 |
| Big[11] | -25089 | -27353 | 1.23 | ExteriorTreeOak2 | grid>19500 |
| Big[12] | -25613 | -19828 | 0.98 | ExteriorTreeMaple0 | grid>19500 |
| Big[17] | -23525 | -21774 | 0.88 | ExteriorTreeOak2 | grid>19500 |
| Big[23] | -21089 | -23287 | 1.09 | ExteriorTreeOak2 | grid>19500 |
| Big[24] | -20946 | -20951 | 0.87 | ExteriorTreeMaple0 | grid>19500 |
| Big[25] | -21315 | -18865 | 0.91 | ExteriorTreeOak1 | grid>19500 |
| Big[26] | -21473 | -17048 | 0.93 | ExteriorTreeOak2 | grid>19500 |
| Big[28] | -19970 | -23018 | 0.92 | ExteriorTreeMaple1 | grid>19500 |
| Big[29] | -18809 | -21495 | 1.24 | ExteriorTreeOak2 | grid>19500 |
| Big[30] | -19531 | -11492 | 1.01 | ExteriorTreeOak0 | grid>19500 |
| Big[32] | -17839 | -24714 | 1.24 | ExteriorTreeMaple2 | grid>19500 |
| Big[35] | -15089 | -24716 | 1.24 | ExteriorTreeOak2 | grid>19500 |
| Big[36] | -15273 | -21294 | 1.01 | ExteriorTreeMaple0 | grid>19500 |
| Big[41] | -13632 | -25470 | 1.11 | ExteriorTreeOak2 | grid>19500 |
| Big[45] | -12298 | -26528 | 0.85 | ExteriorTreeOak0 | grid>19500 |
| Big[46] | -10435 | -23428 | 0.95 | ExteriorTreeOak1 | grid>19500 |
| Big[50] | -6670 | -27204 | 0.98 | ExteriorTreeOak2 | grid>19500 |
| Big[51] | -6255 | -20865 | 1.03 | ExteriorTreeOak0 | grid>19500 |
| Big[61] | 5559 | -8315 | 1.12 | CampusFallTreeOak1Red | grid<=19500 |
| Big[65] | 6958 | -24532 | 0.91 | ExteriorTreeOak2 | grid>19500 |
| Big[66] | 7347 | -21896 | 1.16 | ExteriorTreeOak0 | grid>19500 |
| Big[70] | 9166 | -21158 | 1.16 | ExteriorTreeOak1 | grid>19500 |
| Big[71] | 10922 | -22807 | 0.91 | ExteriorTreeOak2 | grid>19500 |
| Big[74] | 14766 | -26886 | 1.10 | ExteriorTreeOak2 | grid>19500 |
| Big[75] | 14685 | -21779 | 1.08 | ExteriorTreeOak0 | grid>19500 |
| Big[79] | 16582 | -23166 | 1.04 | ExteriorTreeOak1 | grid>19500 |
| Big[83] | 18805 | -23253 | 0.97 | ExteriorTreeOak2 | grid>19500 |
| Big[84] | 20837 | -19048 | 1.05 | ExteriorTreeMaple0 | grid>19500 |
| Big[85] | 20593 | -9656 | 1.09 | ExteriorTreeOak1 | grid>19500 |
| Big[86] | 20573 | -5920 | 1.08 | ExteriorTreeOak2 | grid>19500 |
| Big[95] | 23793 | -9817 | 1.05 | ExteriorTreeOak2 | grid>19500 |
| Big[96] | 23793 | -8199 | 1.15 | ExteriorTreeMaple0 | grid>19500 |
| Big[97] | 23789 | -5796 | 1.17 | ExteriorTreeOak1 | grid>19500 |
| Big[102] | 26088 | -23469 | 0.96 | ExteriorTreeOak0 | grid>19500 |
| Big[103] | 25845 | -14154 | 1.17 | ExteriorTreeOak1 | grid>19500 |
| Big[104] | 26713 | -5983 | 1.06 | ExteriorTreeMaple2 | grid>19500 |
| Small[3] | -16971 | -5900 | 1.09 | ExteriorTreeBirch0 | street-row |
| Small[5] | -14463 | -5900 | 0.89 | ExteriorTreeMaple2 | street-row |
| Small[7] | -12036 | -5900 | 0.93 | ExteriorTreeBirch1 | street-row |
| Small[9] | -9698 | -5900 | 1.21 | ExteriorTreeBirch0 | street-row |
| Small[11] | -7277 | -5900 | 1.06 | CampusFallTreeMaple2Red | street-row |
| Small[13] | -2424 | -5900 | 1.07 | CampusFallTreeBirch1Orange | street-row |
| Small[15] | 7365 | -5900 | 1.24 | CampusFallTreeBirch0Orange | street-row |
| Small[16] | -26484 | -27369 | 0.94 | ExteriorTreeBirch1 | grid>19500 |
| Small[18] | -24982 | -23059 | 0.92 | ExteriorTreeBirch0 | grid>19500 |
| Small[19] | -24557 | -13986 | 0.96 | ExteriorTreeBirch1 | grid>19500 |
| Small[20] | -24580 | -8478 | 0.96 | ExteriorTreeMaple2 | grid>19500 |
| Small[27] | -23044 | -25015 | 1.07 | ExteriorTreeBirch0 | grid>19500 |
| Small[28] | -22810 | -17682 | 1.15 | ExteriorTreeBirch1 | grid>19500 |
| Small[29] | -23436 | -15273 | 1.12 | ExteriorTreeMaple2 | grid>19500 |
| Small[34] | -20915 | -26832 | 1.13 | ExteriorTreeBirch1 | grid>19500 |
| Small[35] | -21430 | -15119 | 1.12 | ExteriorTreeMaple2 | grid>19500 |
| Small[36] | -21206 | -14125 | 1.18 | ExteriorTreeBirch0 | grid>19500 |
| Small[37] | -21275 | -11424 | 0.93 | ExteriorTreeBirch1 | grid>19500 |
| Small[38] | -20766 | -9816 | 0.91 | ExteriorTreeMaple2 | grid>19500 |
| Small[39] | -20792 | -8565 | 0.93 | ExteriorTreeBirch0 | grid>19500 |
| Small[40] | -21457 | -5826 | 0.85 | ExteriorTreeBirch1 | grid>19500 |
| Small[44] | -19339 | -27054 | 0.89 | ExteriorTreeMaple2 | grid>19500 |
| Small[45] | -19218 | -19485 | 0.87 | ExteriorTreeBirch0 | grid<=19500 |
| Small[48] | -17788 | -21326 | 1.13 | ExteriorTreeBirch0 | grid>19500 |
| Small[49] | -17084 | -13520 | 1.07 | ExteriorTreeBirch1 | grid<=19500 |
| Small[50] | -13146 | -26582 | 0.98 | ExteriorTreeMaple2 | grid>19500 |
| Small[51] | -14172 | -23555 | 1.00 | ExteriorTreeBirch0 | grid>19500 |
| Small[55] | -10170 | -25298 | 0.99 | ExteriorTreeBirch1 | grid>19500 |
| Small[56] | -9945 | -21510 | 0.88 | ExteriorTreeMaple2 | grid>19500 |
| Small[59] | -7793 | -26538 | 0.91 | ExteriorTreeMaple2 | grid>19500 |
| Small[60] | -8320 | -9540 | 1.14 | CampusFallTreeBirch0Orange | grid<=19500 |
| Small[65] | -3660 | -25110 | 1.10 | ExteriorTreeMaple2 | grid>19500 |
| Small[66] | -1743 | -20724 | 1.09 | ExteriorTreeBirch0 | grid>19500 |
| Small[67] | -2564 | -19064 | 1.00 | ExteriorTreeBirch1 | grid<=19500 |
| Small[71] | -941 | -26690 | 0.95 | ExteriorTreeMaple2 | grid>19500 |
| Small[72] | -884 | -24705 | 1.11 | ExteriorTreeBirch0 | grid>19500 |
| Small[77] | 3034 | -21572 | 0.88 | ExteriorTreeMaple2 | grid>19500 |
| Small[79] | 5559 | -24805 | 0.85 | ExteriorTreeBirch1 | grid>19500 |
| Small[81] | 9686 | -26492 | 1.19 | ExteriorTreeBirch0 | grid>19500 |
| Small[82] | 10515 | -26874 | 0.97 | ExteriorTreeBirch1 | grid>19500 |
| Small[89] | 15098 | -25461 | 0.87 | ExteriorTreeMaple2 | grid>19500 |
| Small[90] | 14251 | -23582 | 1.19 | ExteriorTreeBirch0 | grid>19500 |
| Small[93] | 16626 | -27304 | 1.18 | ExteriorTreeBirch0 | grid>19500 |
| Small[94] | 16604 | -25359 | 0.88 | ExteriorTreeBirch1 | grid>19500 |
| Small[95] | 16946 | -21295 | 1.09 | ExteriorTreeMaple2 | grid>19500 |
| Small[96] | 16699 | -19669 | 1.07 | ExteriorTreeBirch0 | grid>19500 |
| Small[100] | 18708 | -21362 | 0.94 | ExteriorTreeBirch1 | grid>19500 |
| Small[101] | 18016 | -19857 | 1.02 | ExteriorTreeMaple2 | grid>19500 |
| Small[102] | 20078 | -14299 | 1.10 | ExteriorTreeBirch0 | grid>19500 |
| Small[106] | 22228 | -22788 | 1.22 | ExteriorTreeBirch1 | grid>19500 |
| Small[107] | 21890 | -21802 | 0.96 | ExteriorTreeMaple2 | grid>19500 |
| Small[108] | 22891 | -19911 | 1.25 | ExteriorTreeBirch0 | grid>19500 |
| Small[109] | 22185 | -17229 | 1.03 | ExteriorTreeBirch1 | grid>19500 |
| Small[110] | 22528 | -15404 | 1.09 | ExteriorTreeMaple2 | grid>19500 |
| Small[111] | 22848 | -12054 | 1.00 | ExteriorTreeBirch0 | grid>19500 |
| Small[112] | 22167 | -8541 | 1.09 | ExteriorTreeBirch1 | grid>19500 |
| Small[118] | 26288 | -15444 | 1.18 | ExteriorTreeBirch1 | grid>19500 |
| Small[119] | 26792 | -8135 | 1.22 | ExteriorTreeMaple2 | grid>19500 |

## Appendix B: using `treesim.py`
```python
import sys; sys.path.insert(0, "<briefs dir>"); import treesim as t
base  = t.run()
trial = t.run(extra_nogo=[("newHall", -15000, -11000, -9000, -6600)],   # boxes you plan to ADD to NoGo
              removed_tags=["street:healthCentreLot"],                    # tags from t.baseline_nogo_before_vehicles()
              views=t.VIEWS + [("my_view", 0, -12000)],                   # only for NON-detail views
              vehicle_rows=None)                                          # or a modified copy of t.VEHICLE_ROWS
print(t.same(base, trial), t.first_divergence(base, trial))
```
- `same()` returning True together with an equal `draws` count (3731) means the `Plants()` output is identical, and so the crc is identical, provided nothing outside these inputs changed (Grade, the loops, RearTrees and FitTrees).
- Assumptions: clang float semantics (FMA and non-FMA both reproduce the CSV) and UE 5.8 `FRandomStream`.
- The script mirrors only the code at the file:line refs in its docstring. If those lines change, update the script first.
