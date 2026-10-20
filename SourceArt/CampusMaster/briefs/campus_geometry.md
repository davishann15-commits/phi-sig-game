# Campus geometry catalogue: the campus side and roads (read-only survey, 2026-10-13)

- **Project:** `/Users/Stewart/Documents/Unreal Projects/SeniorSendoff`. Paths below are relative to `Source/SeniorSendoff/` unless they say otherwise.
- **How this was made:**
  - Nothing in the project was edited, built or run.
  - The numbers come from the source plus scripts in `campus_geometry_src/` beside this file.
  - Those scripts re-implement UE `FRandomStream` (`RandomStream.h:115-124,362-365`; uint32 seed) and use the saved tree snapshot `SourceArt/WoodsPass/evidence/*/Saved/WoodsPassReview/original_trees.csv`. All 13 copies are identical (md5 fa8c6305…), with crc `0f3b2bb7`.
- **Validation of the replays:**
  - The builder RNG `FRandomStream(20261009)` (`HouseExterior.cpp:438`) is only drawn by `Vehicles()` and `Plants()`. Starting at draw 0, `Vehicles()` uses draws 0–101 and makes 38 cars. The 181 shrub draws follow. The first tree's scale and yaw land exactly on draws 283–284 of the stream, which matches the CSV.
  - The homecoming prop solver reproduces the logged `bonfire=(3750,-7700)`.
- **Units:** world cm, Z up.
- **Directions:** in this brief, "north" means +Y (toward the house and woods) and "south" means −Y (toward the campus). +X is east; the code calls −X west (sun side).
- **Plotting:** plot X to the right and Y up as given, which puts the house at the top and the campus at the bottom. Because UE is left-handed, the editor's Top view is the mirror image of this plot.
- **Grade:** front and campus grade is Z = −60. The road top is −72, sidewalks and kerbs −57, campus walks −56.

## Index

| Need | Section |
|---|---|
| Verdicts on the 12 reported layout bugs, with numbers | §15 (all 12 confirmed; #3 and #4 are worse than reported) |
| Problems not on the list (tower on the library pad, mast through the press box, floating scoreboard, cars through props, kerb cut, signage) | §16 |
| What the master job must keep: tree CRC inputs, ≤ 53 lights, review rules, views, sight line | §17 |
| Constants copied between files | §18 |
| Coordinates for a top-down plan | §19, plus the point tables in §5, §10, §11 and §14 |

## 0. Constants

| Name | Value | Where |
|---|---|---|
| `StreetY(y)` | y − 1200 | `HouseExteriorSiteLayout.h:7` |
| Front road (Fraternity Park Dr) carriageway | Y −5550..−4150, X −32000..8700 | `HouseExterior.cpp:1161`, SiteLayout.h:8-9 |
| Near (house) kerb face / far (campus) kerb face | −4170 / −5530 (kerb slabs −4170..−4150, −5550..−5530) | `HouseStreetScene.cpp:17-18`; `HouseExterior.cpp:1178-1179` |
| Road centre line | Y −4850 | `HouseStreetScene.cpp:19` |
| `GF` front grade, `GR` rear grade | −60, −362 | `HouseExterior.cpp:106-107` |
| Campus `Ground` | −60 (`Rear` = −362) | `HouseExteriorCampus.cpp:14` |
| Stadium centre | (2400, −13300) | `HouseExteriorCampus.h:7` |
| Clock tower centre | (−9000, −10800), ground Z −60 | `HouseClockTowerMesh.h:11` |
| World / terrain edge | ±32000 (X and Y) | `HouseExterior.cpp:1121` |
| Old playable box (barriers OFF) | X −8500..8800, Y −5900..19800; `bPropertyBoundaryOn=false` | `HouseExteriorSiteLayout.h:13-23` |
| Woods patch | X −8200..8200, Y 4600..19200 | `HouseWoodsDetail.h:9` |
| West field raise | X −21000..−2500, Y 300..6500 only | `HouseWestField.cpp:21-31` |
| Fall Jam field (east lawn) | X 4050..8400, Y −3660..−100 | `HouseFallJam.cpp:14` |

## 1. Terrain and grade

| Item | Value | Where |
|---|---|---|
| `Grade(X,Y)` | Y ≤ 0 → **−60 flat**. 0 < Y < 1600 → linear −60 → −362, then `HouseWestField::RaisedHeight`. Y ≥ 1600 → `HouseWoods::Height`. | `HouseExterior.cpp:112-117` |
| `HouseWestField::RaisedHeight` | Identity unless −21000 < X < −2500 and 300 < Y < 6500. **No effect on the campus.** | `HouseWestField.cpp:21-31` |
| `HouseSurroundings` local `Grade(Y)` (used for the side-street kerb joints) | −60 / linear / −362 | `HouseSurroundingsPolish.cpp:34` |
| Terrain mesh | One `ExteriorGrounds` mesh, **collision on**. Grass patch over ±32000 with the road, side street, house and court cut out. Campus cells are 1600 cm with no relief. | `HouseExterior.cpp:1119-1153`, made at `1566` |
| Terrain cuts | Front road (−32000..8700 × −5550..−4150); side street (8700..10300 × ±32000); house wings (−3398..3338 × −2198..1718); court (−1506..1506 × 1720..3820) | `HouseExterior.cpp:1129-1134` |
| **Campus relief** | **None.** Every point with Y ≤ 0 is Z −60, all the way to Y −32000. | — |
| Beyond Y −32000 | Only `ExteriorFar` grass (X −31000..31000, Y −49000..−31800, Z −135..−63). **No collision**, so a walker falls (story-mode fall respawn). | `HouseExteriorCampus.cpp:205`; `HouseExterior.cpp:1568` (`Make(B.Far,…,false,…)`) |
| Mountains | Rear only: ridges start at Y +32000, +65000 and +112000 and span X ±145000. **Nothing on the campus horizon.** | `HouseExteriorCampus.cpp:210-220` |

## 2. Roads, kerbs, sidewalks, markings and lots

| Item | X | Y | Z (top) | Collision | Where |
|---|---|---|---|---|---|
| Fraternity Park Dr asphalt | **−32000**..8700 | −5550..−4150 | −72 | yes (Grounds) | `HouseExterior.cpp:1161` |
| **Pat Head Summitt St** asphalt (the side street) | 8700..10300 | **−32000..+32000** | Grade − 12 (−72 on the campus side) | yes; skirt in concrete | `:1162` |
| Far kerb (campus side) | −32000..8700 | −5550..−5530 | −57 (bottom −100) | yes | `:1178` |
| Near kerb | −32000..8700 | −4170..−4150 | −57 | yes | `:1179` |
| Far sidewalk W | −32000..−5000 | −5800..−5550 | −57 | yes | `:1173` |
| Far sidewalk E | −4400..8450 | −5800..−5550 | −57 | yes | `:1173` |
| Gap in the far sidewalk = health-lot drive | −5000..−4400 | −5800..−5530 | −58 (asphalt) | yes | `:1188` |
| Near sidewalks | −32000..−5800, −5200..3450, 4050..8450 | −4150..−3900 | −57 | yes | `:1172` |
| Side-street W sidewalk (campus) | 8450..8700 | −32000..−5800 | −57 | yes | `:1174` |
| Side-street W sidewalk (house side) | 8450..8700 | −3900..2500 and 3100..32000 | Grade + 3 | yes | `:1175-1176` |
| Side-street E sidewalk | 10300..10550 | −32000..32000 | Grade + 3 | yes | `:1177` |
| Painted centre dashes, front road | X −31600..−20000, 300 long every 800 | −4858..−4842 | −71.5 | — | `:1166` (decals replace them east of −20000: `HouseStreetScene.h:21`) |
| Decal centre line and parking lines | −20000..8400 | centre −4850; parking lines Y −4460 and −5240 | — | decal | `HouseStreetScene.cpp:2014-2019` |
| Half-road decals | −9000..8450 | — | — | decal | `HouseStreetScene.cpp:2007-2011` |
| Edge lines | −32000..−20000 | −4185..−4175, −5525..−5515 | −71.5 | — | `HouseExterior.cpp:1169` |
| Side-street centre dashes | 9492..9508 | all Y except −5550..−4150 and −400..2000 (they run on under blocks 3, 11, 13) | Grade − 11.5 | — | `:1167` |
| Crosswalk (11 bars, 60 wide at 118 pitch) | 8320..8510 | −5490..−4220 | −71 | no (StreetDecor) | `HouseSurroundingsPolish.cpp:155` |
| Stop bar | 8150..8175 | −5480..−4900 (**south half, which is the westbound exit lane**) | −71 | no | `:156` |
| Stop sign and name blades on one post | post (8080, −3810), Z −60..265 | blades at Y −3826/−3827, **both facing −Y and parallel**: FRATERNITY PARK DR at Z 285, PAT HEAD SUMMITT ST at Z 330 | — | no | `:157-168`; text `:82-83` |
| Side-street kerb joints | X 8575, 10425 | **only Y −5900..4500** (none on the campus stretch) | — | no | `:141` |
| Far-kerb inlets (kerb openings) | X −8100, −4200, 1200, 7200 | face −5530 | — | no | `HouseStreetScene.cpp:1661`; grates `HouseSurroundingsPolish.cpp:143-149` |
| Manholes | (−1050,−5160), (4900,−5100), (−6100,−5060) | — | −70.8 | no | `HouseStreetScene.cpp:1678` |
| **Health-centre lot** | **−9000..0** | **−8600..−5800** | −58 (bottom −100) | yes | `HouseExterior.cpp:1187` |
| Health-lot bay lines (10 cm wide, every 500) | X −8800..−300 | −8550..−8000 and −6450..−5900 | −57.5 (under block 1, block 2 and the walk) | — | `:1191` |
| Health-lot car Z | −58 (`PaveZ`) | — | — | — | `:1876` |
| Lawn and leaf exclusion copy of the lot | −9035..35 × −8635..−5765, and the drive | — | — | — | `HouseExteriorNature.cpp:72` |
| No kerb cut at the lot drive | the far kerb (−5550..−5530, top −57) runs straight across the drive mouth; road −72 | — | — | — | `HouseExterior.cpp:1178` |

**Fixed anchors on the house side (do not move)**

| Anchor | Rect | Where |
|---|---|---|
| House shell (terrain cut) | X −3398..3338, Y −2198..1718 | `HouseExterior.cpp:1131` |
| Front lawn (kept open) | X −3550..3300, Y −3900..−2550 | `:1200` |
| West parking lot | X −7200..−3700, Y −2500..−300 | `:1185`; `HouseParkingLot.h:7` |
| West lot drive | X −5800..−5200, Y −4190..−2500 | `:1186` |
| Side drive | X 3450..4050, Y −4190..1900 | `:1181-1182` |
| Rear lot | X 3450..6600 × 1900..4500 and X 6600..8720 × 2500..3100 | `:1183-1184` |
| Court / backyard | X −1506..1506, Y 1720..3820 | `:1132` |
| Fraternity row footprints (no collision) | A: X −11600..−8600 × −3720..100 (house at −10100,−1500, 2500×2200, H 900). B: X −15550..−12250 × −3700..100 (−13900,−1480, 2800×2100, H 820). C: X −18850..−15950 × −3750..100 (−17400,−1420, 2450×2250, H 950). | `HouseFraternityRow.cpp:157-163`; `Local(…,false,false)` at `HouseExterior.cpp:1574` |
| West field and fence | field X −21000..−2500, Y 300..6500; fence Y 4520, X −19300..−3900 | `HouseWestField.h:9` |

## 3. The 16 campus blocks (`BuildCampus`, `HouseExteriorCampus.cpp:116-130`)

- Each `{X,Y,W,D,H,Style}` is at lines 121-129. The seed is the array index (`:130`).
- Every block is one solid `ExteriorNear` box with collision (`HouseExterior.cpp:1567`, split into 4000 cm tiles), with **no interior**.
- Z values are world Z (grade −60).
- **Windows:** windows per floor on each ±Y face / on each ±X face; the total covers all four faces.
- **Signs:** sign text is chosen by `Seed%3` (`HouseSurroundingsPolish.cpp:120`): 0 = UNIVERSITY LIBRARY, 1 = UNIVERSITY HALL, 2 = SCIENCE CENTER. **Six blocks say LIBRARY, five HALL and five SCIENCE CENTER.**
- **Dorms:** seeds 1 and 4 only (`HouseCampusFallLook.cpp:374`), and the dorm dressing hard-codes their rectangles (`HouseCampusFallQuad.cpp:357-358`).

| # (=seed) | centre X,Y | W x D | X min..max | Y min..max | H | roof Z / parapet Z | floors x story | win per floor: +-Y face / +-X face | windows total (lit, homecoming) | style / body material | sign (Seed%3) | footprint NoGo (+150) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | -14800,-8100 | 5200x3000 | -17400..-12200 | -9600..-6600 | 2120 | 2060 / 2174 | 5 x 414 | 13 / 7 | 200 (40) | 0 EX_Brick | UNIVERSITY LIBRARY | -17550..-12050 x -9750..-6450 |
| 1 | -8700,-7900 | 3800x2600 | -10600..-6800 | -9200..-6600 | 2540 | 2480 / 2594 | 6 x 415 | 10 / 6 | 192 (78) | 1 EX_CampusLimestone | UNIVERSITY HALL (dorm) | -10750..-6650 x -9350..-6450 |
| 2 | -4850,-7000 | 2100x1700 | -5900..-3800 | -7850..-6150 | 1260 | 1200 / 1314 | 3 x 403 | 5 / 4 | 54 (12) | 2 EX_CampusWarmBrick | SCIENCE CENTER | -6050..-3650 x -8000..-6000 |
| 3 | 9600,-7700 | 3500x2500 | 7850..11350 | -8950..-6450 | 1780 | 1720 / 1834 | 4 x 432 | 9 / 6 | 120 (24) | 1 EX_CampusLimestone | UNIVERSITY LIBRARY | 7700..11500 x -9100..-6300 |
| 4 | 14500,-8450 | 4300x3600 | 12350..16650 | -10250..-6650 | 2540 | 2480 / 2594 | 6 x 415 | 11 / 9 | 240 (94) | 0 EX_Brick | UNIVERSITY HALL (dorm) | 12200..16800 x -10400..-6500 |
| 5 | -14500,-12500 | 4800x3000 | -16900..-12100 | -14000..-11000 | 2120 | 2060 / 2174 | 5 x 414 | 12 / 7 | 190 (38) | 2 EX_CampusWarmBrick | SCIENCE CENTER | -17050..-11950 x -14150..-10850 |
| 6 | -8700,-13100 | 3600x3000 | -10500..-6900 | -14600..-11600 | 2960 | 2900 / 3014 | 7 x 416 | 9 / 7 | 224 (42) | 0 EX_Brick | UNIVERSITY LIBRARY | -10650..-6750 x -14750..-11450 |
| 7 | -14800,-17300 | 4400x3500 | -17000..-12600 | -19050..-15550 | 1700 | 1640 / 1754 | 4 x 412 | 11 / 9 | 160 (34) | 1 EX_CampusLimestone | UNIVERSITY HALL | -17150..-12450 x -19200..-15400 |
| 8 | -9300,-18500 | 3500x4300 | -11050..-7550 | -20650..-16350 | 2540 | 2480 / 2594 | 6 x 415 | 9 / 11 | 240 (48) | 2 EX_CampusWarmBrick | SCIENCE CENTER | -11200..-7400 x -20800..-16200 |
| 9 | -14300,-22700 | 4800x3700 | -16700..-11900 | -24550..-20850 | 2960 | 2900 / 3014 | 7 x 416 | 12 / 9 | 294 (62) | 0 EX_Brick | UNIVERSITY LIBRARY | -16850..-11750 x -24700..-20700 |
| 10 | -7900,-24100 | 4200x3100 | -10000..-5800 | -25650..-22550 | 2120 | 2060 / 2174 | 5 x 414 | 11 / 8 | 190 (38) | 1 EX_CampusLimestone | UNIVERSITY HALL | -10150..-5650 x -25800..-22400 |
| 11 | 10000,-12800 | 3000x3400 | 8500..11500 | -14500..-11100 | 2540 | 2480 / 2594 | 6 x 415 | 7 / 8 | 180 (34) | 2 EX_CampusWarmBrick | SCIENCE CENTER | 8350..11650 x -14650..-10950 |
| 12 | 15000,-14200 | 4200x3900 | 12900..17100 | -16150..-12250 | 2960 | 2900 / 3014 | 7 x 416 | 11 / 10 | 294 (58) | 0 EX_Brick | UNIVERSITY LIBRARY | 12750..17250 x -16300..-12100 |
| 13 | 11000,-19100 | 3300x3200 | 9350..12650 | -20700..-17500 | 2120 | 2060 / 2174 | 5 x 414 | 8 / 8 | 160 (32) | 1 EX_CampusLimestone | UNIVERSITY HALL | 9200..12800 x -20850..-17350 |
| 14 | 15500,-22300 | 4200x3600 | 13400..17600 | -24100..-20500 | 2540 | 2480 / 2594 | 6 x 415 | 11 / 9 | 240 (48) | 2 EX_CampusWarmBrick | SCIENCE CENTER | 13250..17750 x -24250..-20350 |
| 15 | 2100,-23000 | 7300x2900 | -1550..5750 | -24450..-21550 | 2120 | 2060 / 2174 | 5 x 414 | 19 / 7 | 260 (52) | 0 EX_Brick | UNIVERSITY LIBRARY | -1700..5900 x -24600..-21400 |

**Window rows and string courses** (world Z)

| # | window bottom Z per floor (height 212) | string-course Z (13 tall) | first +Y window centre X, pitch |
|---|---|---|---|
| 0 | 30, 444, 858, 1272, 1686 | 380, 800, 1220, 1640 | -17152.5, 360 |
| 1 | 30, 445, 860, 1275, 1690, 2105 | 380, 800, 1220, 1640, 2060 | -10352.5, 360 |
| 2 | 30, 433, 837 | 380, 800 | -5652.5, 360 |
| 3 | 30, 462, 895, 1328 | 380, 800, 1220, 1640 | 8097.5, 360 |
| 4 | 30, 445, 860, 1275, 1690, 2105 | 380, 800, 1220, 1640, 2060 | 12597.5, 360 |
| 5 | 30, 444, 858, 1272, 1686 | 380, 800, 1220, 1640 | -16652.5, 360 |
| 6 | 30, 446, 861, 1277, 1693, 2109, 2524 | 380, 800, 1220, 1640, 2060, 2480 | -10252.5, 360 |
| 7 | 30, 442, 855, 1268 | 380, 800, 1220 | -16752.5, 360 |
| 8 | 30, 445, 860, 1275, 1690, 2105 | 380, 800, 1220, 1640, 2060 | -10802.5, 360 |
| 9 | 30, 446, 861, 1277, 1693, 2109, 2524 | 380, 800, 1220, 1640, 2060, 2480 | -16452.5, 360 |
| 10 | 30, 444, 858, 1272, 1686 | 380, 800, 1220, 1640 | -9752.5, 360 |
| 11 | 30, 445, 860, 1275, 1690, 2105 | 380, 800, 1220, 1640, 2060 | 8747.5, 360 |
| 12 | 30, 446, 861, 1277, 1693, 2109, 2524 | 380, 800, 1220, 1640, 2060, 2480 | 13147.5, 360 |
| 13 | 30, 444, 858, 1272, 1686 | 380, 800, 1220, 1640 | 9597.5, 360 |
| 14 | 30, 445, 860, 1275, 1690, 2105 | 380, 800, 1220, 1640, 2060 | 13647.5, 360 |
| 15 | 30, 444, 858, 1272, 1686 | 380, 800, 1220, 1640 | -1302.5, 360 |

**Building kit** (`Building()`, `HouseExteriorCampus.cpp:42-71`). Coordinates are relative to `CX` = centre X and `Y` = max Y (the +Y face).

| Part | Geometry | Line |
|---|---|---|
| NoGo footprint | box +150 on every side, added to `Layout.BuildingFootprints` | 44 |
| Body | `Box(Min.X..Max.X, Min.Y..Max.Y, Z −130..−60+H)` in Brick / Limestone / WarmBrick by Style | 45-46 |
| Plinth | ±12 out, Z −60..15 | 47 |
| String courses | ±8 out, 13 tall, every 420 from Z 380 while below −60+H−50 | 48 |
| Cornice | ±28 out, Z H−100..H−48 (world: −60+H−40..−60+H+12) | 49 |
| Roof slab | inset 45, Z −60+H+12..+38 | 50 |
| Pilasters (±Y faces only) | 44 wide, at X = Min.X+65+1200k, from Y∓16 ±13 | 51 |
| Windows | 175 wide × 212 tall, every 360 along each face starting 160 in, sill Z = 30 + floor × story. Floors = clamp(floor(H/420), 2, 7), story = (H−50)/floors. Lit: `(I+F+Seed)%5==0`, or dorm `(3I+2F+Seed)%5<2` (`HouseCampusFallLook.cpp:375-380`). | 22-41, 52 |
| Entrance portico (**always the +Y face**) | stone CX±450, Y+1..Y+85, Z −54..330; glass CX±320 at Y+87 (a painted panel, **no opening**); 3 mullions | 53-56 |
| Canopy | Dark CX±470, Y+2..Y+240, Z 277..295; 2 columns at CX±365, Y+195 | 57, 59 |
| Entrance pad | concrete CX±520, Y..Y+550, Z −59..−55 | 58 |
| Planters | stone CX±520 ±75, Y+220..Y+380, Z −55..8; shrubs (`RoofServices`) | 62, `HouseSurroundingsPolish.cpp:122` |
| Rooftop mechanical | CX−220..CX+80, Min.Y+250..+550, Z H−22..H+115 | 61 |
| `RoofServices` (polish on) | parapet ±17..20, Z H−22..H+54; drain pipes at Min.X+85 and Max.X−85 at Y∓24 from Z −40; domes and fans | `HouseSurroundingsPolish.cpp:108-118` |
| Sign | 580×66 at (CX, Y+99, **Z 340 absolute**), facing +Y | `HouseSurroundingsPolish.cpp:120` |

**Entrances** (one per block, always on the +Y face)

| # | door face Y (+Y) | portico X / Y | pad (concrete) X x Y | canopy X x Y (Z) | planters X centres (Y) | first obstacle in front (+Y) | conflicts |
|---|---|---|---|---|---|---|---|
| 0 | -6600 | -15250..-14350 / -6599..-6515 | -15320..-14280 x -6600..-6050 | -15270..-14330 x -6598..-6360 (277..295) | -15320, -14280 (-6380..-6220) | 1050 cm: front-road far kerb (Y -5550) | pad meets walkY-6000 |
| 1 | -6600 | -9150..-8250 / -6599..-6515 | -9220..-8180 x -6600..-6050 | -9170..-8230 x -6598..-6360 (277..295) | -9220, -8180 (-6380..-6220) | 1050 cm: front-road far kerb (Y -5550) | pad meets walkY-6000; pad on health lot |
| 2 | -6150 | -5300..-4400 / -6149..-6065 | -5370..-4330 x -6150..-5600 | -5320..-4380 x -6148..-5910 (277..295) | -5370, -4330 (-5930..-5770) | 600 cm: front-road far kerb (Y -5550) | pad meets walkY-6000; pad ON FarSidewalkW (-5370, -5000, -5800, -5600); pad ON FarSidewalkE (-4400, -4330, -5800, -5600); pad on health lot |
| 3 | -6450 | 9150..10050 / -6449..-6365 | 9080..10120 x -6450..-5900 | 9130..10070 x -6448..-6210 (277..295) | 9080, 10120 (-6230..-6070) | 900 cm: front-road far kerb (Y -5550) | pad meets walkY-6000; pad ON SideStreet (9080, 10120, -6450, -5900); PORTICO STANDS IN SIDE STREET |
| 4 | -6650 | 14050..14950 / -6649..-6565 | 13980..15020 x -6650..-6100 | 14030..14970 x -6648..-6410 (277..295) | 13980, 15020 (-6430..-6270) | 1100 cm: front-road far kerb (Y -5550) | - |
| 5 | -11000 | -14950..-14050 / -10999..-10915 | -15020..-13980 x -11000..-10450 | -14970..-14030 x -10998..-10760 (277..295) | -15020, -13980 (-10780..-10620) | 1400 cm: back (south face) of B0 | - |
| 6 | -11600 | -9150..-8250 / -11599..-11515 | -9220..-8180 x -11600..-11050 | -9170..-8230 x -11598..-11360 (277..295) | -9220, -8180 (-11380..-11220) | 405 cm: clock-tower plinth | CLOCK-TOWER PLINTH ON PAD (-9220, -8605, -11195, -11050) |
| 7 | -15550 | -15250..-14350 / -15549..-15465 | -15320..-14280 x -15550..-15000 | -15270..-14330 x -15548..-15310 (277..295) | -15320, -14280 (-15330..-15170) | 1550 cm: back (south face) of B5 | - |
| 8 | -16350 | -9750..-8850 / -16349..-16265 | -9820..-8780 x -16350..-15800 | -9770..-8830 x -16348..-16110 (277..295) | -9820, -8780 (-16130..-15970) | 1750 cm: back (south face) of B6 | pad meets walkY-16100 |
| 9 | -20850 | -14750..-13850 / -20849..-20765 | -14820..-13780 x -20850..-20300 | -14770..-13830 x -20848..-20610 (277..295) | -14820, -13780 (-20630..-20470) | 1800 cm: back (south face) of B7 | - |
| 10 | -22550 | -8350..-7450 / -22549..-22465 | -8420..-7380 x -22550..-22000 | -8370..-7430 x -22548..-22310 (277..295) | -8420, -7380 (-22330..-22170) | 1900 cm: back (south face) of B8 | - |
| 11 | -11100 | 9550..10450 / -11099..-11015 | 9480..10520 x -11100..-10550 | 9530..10470 x -11098..-10860 (277..295) | 9480, 10520 (-10880..-10720) | 2150 cm: back (south face) of B3 | pad ON SideStreet (9480, 10300, -11100, -10550); pad ON SideWalkE (10300, 10520, -11100, -10550); PORTICO STANDS IN SIDE STREET |
| 12 | -12250 | 14550..15450 / -12249..-12165 | 14480..15520 x -12250..-11700 | 14530..15470 x -12248..-12010 (277..295) | 14480, 15520 (-12030..-11870) | 2000 cm: back (south face) of B4 | - |
| 13 | -17500 | 10550..11450 / -17499..-17415 | 10480..11520 x -17500..-16950 | 10530..11470 x -17498..-17260 (277..295) | 10480, 11520 (-17280..-17120) | 3000 cm: back (south face) of B11 | pad ON SideWalkE (10480, 10550, -17500, -16950) |
| 14 | -20500 | 15050..15950 / -20499..-20415 | 14980..16020 x -20500..-19950 | 15030..15970 x -20498..-20260 (277..295) | 14980, 16020 (-20280..-20120) | 4350 cm: back (south face) of B12 | - |
| 15 | -21550 | 1650..2550 / -21549..-21465 | 1580..2620 x -21550..-21000 | 1630..2570 x -21548..-21310 (277..295) | 1580, 2620 (-21330..-21170) | 4460 cm: stadium outer wall | pad meets walkY-21100 |

**What each block footprint overlaps**

| # | overlaps (X0, X1, Y0, Y1 of the overlap) |
|---|---|
| 0 | - |
| 1 | health lot (-9000, -6800, -8600, -6600) |
| 2 | health lot (-5900, -3800, -7850, -6150) |
| 3 | SideStreet (8700, 10300, -8950, -6450); SideWalkW_S (8450, 8700, -8950, -6450); SideWalkE (10300, 10550, -8950, -6450) |
| 4 | walkX12800 (12700, 12900, -10250, -6650); walkY-10300 (12350, 16650, -10250, -10200) |
| 5 | - |
| 6 | - |
| 7 | walkY-16100 (-17000, -12600, -16200, -16000); HedgeS_W (-17000, -12600, -18860, -18740) |
| 8 | HedgeS_W (-11050, -7550, -18860, -18740); lies past \|19500\|: grid trees ignore its NoGo |
| 9 | walkY-21100 (-16700, -11900, -21200, -21000); lies past \|19500\|: grid trees ignore its NoGo |
| 10 | walkX-6100 (-6200, -6000, -25650, -22550); lies past \|19500\|: grid trees ignore its NoGo |
| 11 | SideStreet (8700, 10300, -14500, -11100); SideWalkW_S (8500, 8700, -14500, -11100); SideWalkE (10300, 10550, -14500, -11100); stadium wall only r=1.09 away |
| 12 | walkY-16100 (12900, 17100, -16150, -16000) |
| 13 | SideStreet (9350, 10300, -20700, -17500); SideWalkE (10300, 10550, -20700, -17500); HedgeS_E (10350, 12650, -18860, -18740); lies past \|19500\|: grid trees ignore its NoGo |
| 14 | walkY-21100 (13400, 17600, -21200, -21000); lies past \|19500\|: grid trees ignore its NoGo |
| 15 | lies past \|19500\|: grid trees ignore its NoGo |

## 4. Campus walks (`HouseExteriorCampus.cpp:134-135`; tree NoGo bands `HouseExterior.cpp:1246-1247` at ±350)

- All walks are concrete with collision.
- The X walks run Y −27000..−6100. The Y walks run X −17800..18000.
- **None of them is connected to an entrance by a path.** The only links are a few entrance pads that touch a walk (section 3, entrance table).

| walk | X | Y | Z | crosses (overlap X0,X1,Y0,Y1) |
|---|---|---|---|---|
| X=-11200 | -11300..-11100 | -27000..-6100 | -59..-56 | skims B8 (<150) (-11200, -11100, -20800, -16200); hedge HedgeS_W (-11300, -11100, -18860, -18740) |
| X=-6100 | -6200..-6000 | -27000..-6100 | -59..-56 | skims B2 (<150) (-6050, -6000, -8000, -6100); **THROUGH B10** (-6200, -6000, -25650, -22550); health lot (-6200, -6000, -8600, -6100); hedge HedgeS_W (-6200, -6000, -18860, -18740) |
| X=7200 | 7100..7300 | -27000..-6100 | -59..-56 | **UNDER STADIUM STANDS** Y -15250..-11350; hedge HedgeS_W (7100, 7300, -18860, -18740) |
| X=12800 | 12700..12900 | -27000..-6100 | -59..-56 | **THROUGH B4** (12700, 12900, -10250, -6650); skims B12 (<150) (12750, 12900, -16300, -12100); skims B13 (<150) (12700, 12800, -20850, -17350); hedge HedgeS_E (12700, 12900, -18860, -18740) |
| Y=-6000 | -17800..18000 | -6100..-5900 | -59..-56 | skims B2 (<150) (-6050, -3650, -6100, -6000); SideStreet (8700, 10300, -6100, -5900); SideWalkW_S (8450, 8700, -6100, -5900); SideWalkE (10300, 10550, -6100, -5900); health lot (-9000, 0, -6100, -5900) |
| Y=-10300 | -17800..18000 | -10400..-10200 | -59..-56 | **THROUGH B4** (12350, 16650, -10250, -10200); **UNDER STADIUM STANDS** X -1030..5830; SideStreet (8700, 10300, -10400, -10200); SideWalkW_S (8450, 8700, -10400, -10200); SideWalkE (10300, 10550, -10400, -10200) |
| Y=-16100 | -17800..18000 | -16200..-16000 | -59..-56 | **THROUGH B7** (-17000, -12600, -16200, -16000); **THROUGH B12** (12900, 17100, -16150, -16000); **UNDER STADIUM STANDS** X -1380..6180; SideStreet (8700, 10300, -16200, -16000); SideWalkW_S (8450, 8700, -16200, -16000); SideWalkE (10300, 10550, -16200, -16000) |
| Y=-21100 | -17800..18000 | -21200..-21000 | -59..-56 | **THROUGH B9** (-16700, -11900, -21200, -21000); **THROUGH B14** (13400, 17600, -21200, -21000); SideStreet (8700, 10300, -21200, -21000); SideWalkW_S (8450, 8700, -21200, -21000); SideWalkE (10300, 10550, -21200, -21000) |

## 5. Lamps and benches (`ExteriorNear`, with collision)

- **Lamp (`CampusLamp`, `HouseExteriorCampus.cpp:72-79`):**
  - Pole: cylinder r6, Z −60..460.
  - Head: ±23, Z 452..468.
  - Lens: ±17, Z 447..452. It is `CF_MercuryLens` under homecoming, otherwise `EX_Flood`.
  - **No real light.**
- **Homecoming dressing (`HouseCampusFallQuad.cpp:141-158`):** adds banners and a halo to every lamp **except the two inside the ellipse (X−2400)/5900, (Y+13300)/4100**. Line 148 says "The two lamps the east stands were built over stay as they are."
- **Lamps on walk edges are intentional.** The Y −5900 row sits on the north edge of walk Y=−6000, and X −11300 / 7300 sit on the edges of walks X=−11200 / 7200.

| # | X | Y | source | flags |
|---|---|---|---|---|
| L0 | -16000 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000 |
| L1 | -14100 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000 |
| L2 | -12200 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000 |
| L3 | -10300 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000 |
| L4 | -8400 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000; in health lot |
| L5 | -6500 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000; in health lot |
| L6 | -4600 | -5900 | HouseExteriorCampus.cpp:136 | on B2 entrance pad; on walk Y=-6000; in health lot |
| L7 | -2700 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000; in health lot; **through shuttle-shelter roof** |
| L8 | -800 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000; in health lot |
| L9 | 1100 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000 |
| L10 | 3000 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000 |
| L11 | 4900 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000 |
| L12 | 6800 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000 |
| L13 | 8700 | -5900 | HouseExteriorCampus.cpp:136 | on SideStreet; on SideWalkW_S; on walk Y=-6000 |
| L14 | 10600 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000 |
| L15 | 12500 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000 |
| L16 | 14400 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000 |
| L17 | 16300 | -5900 | HouseExteriorCampus.cpp:136 | on walk Y=-6000 |
| L18 | -11300 | -9000 | HouseExteriorCampus.cpp:137 | on walk X=-11200 |
| L19 | -11300 | -11800 | HouseExteriorCampus.cpp:137 | on walk X=-11200 |
| L20 | -11300 | -14600 | HouseExteriorCampus.cpp:137 | on walk X=-11200 |
| L21 | -11300 | -17400 | HouseExteriorCampus.cpp:137 | on walk X=-11200 |
| L22 | -11300 | -20200 | HouseExteriorCampus.cpp:137 | on walk X=-11200 |
| L23 | -11300 | -23000 | HouseExteriorCampus.cpp:137 | on walk X=-11200 |
| L24 | 7300 | -9000 | HouseExteriorCampus.cpp:137 | on walk X=7200 |
| L25 | 7300 | -11800 | HouseExteriorCampus.cpp:137 | on walk X=7200; **INSIDE STADIUM r=0.960 (under the stands)** |
| L26 | 7300 | -14600 | HouseExteriorCampus.cpp:137 | on walk X=7200; **INSIDE STADIUM r=0.940 (under the stands)** |
| L27 | 7300 | -17400 | HouseExteriorCampus.cpp:137 | on walk X=7200 |
| L28 | 7300 | -20200 | HouseExteriorCampus.cpp:137 | on walk X=7200 |
| L29 | 7300 | -23000 | HouseExteriorCampus.cpp:137 | on walk X=7200 |

**Benches** (`HouseExteriorCampus.cpp:138-141`):
- Seat X±110, Y−28..+28, Z −20..−13; back at Y+23..+33.
- Legs at X±85.

| X | Y | seat box X / Y | flags |
|---|---|---|---|
| -11000 | -9700 | -11110..-10890 / -9728..-9667 | overlaps walk X=-11200 by (-11110, -11100, -9728, -9667) |
| -11000 | -15900 | -11110..-10890 / -15928..-15867 | overlaps walk X=-11200 by (-11110, -11100, -15928, -15867) |
| -6900 | -9700 | -7010..-6790 / -9728..-9667 | - |
| -6900 | -15900 | -7010..-6790 / -15928..-15867 | - |
| 7800 | -9700 | 7690..7910 / -9728..-9667 | - |
| 7800 | -15900 | 7690..7910 / -15928..-15867 | stadium r=1.18 (outside wall, inside stadium NoGo rect) |
| 12000 | -9700 | 11890..12110 / -9728..-9667 | - |
| 12000 | -15900 | 11890..12110 / -15928..-15867 | - |

## 6. Stadium bowl (`HouseExteriorCampus.cpp:142-203`; homecoming dressing in `HouseCampusFallStadium.cpp`)

**Bowl formula:** `P(A,R,Z) = (2400 + cos A·5600·R, −13300 + sin A·3800·R, −60 + Z)`. There are 40 segments of 9° each; A = 0 points +X (east) and A = 90° points +Y (north, toward the street).

| Part | Geometry (world) | Line |
|---|---|---|
| Outer wall | Ellipse RX 5600, RY 3800 → X −3200..8000, Y −17100..−9500. Solid brick panels on **all 40 segments**, Z −60..990. Stone bands at Z −30..25 and 980..1050; rim cap R .98..1.045 at Z 990. | 145-146, 151 |
| Concourse "arches" (K even, 20) | A stone prism 270 wide × 430 tall plus a dark inset prism, 9..22 cm **outside** the wall. **Decoration only: the wall behind is solid.** | 159-166 |
| Arch centres (K: angle) | K0 (7974,−13002); K2 (7566,−11847); K4 (6652,−10834); K6 (5322,−10062); **K8 (3705,−9608) = GATE 4**; **K10 (1961,−9514) = GATE 5**; K12 (260,−9792); K14 (−1232,−10413); K16 (−2368,−11316); K18 (−3037,−12414); K20 (−3174,−13598); K22 (−2766,−14753); K24 (−1852,−15766); K26 (−522,−16538); K28 (1095,−16992); K30 (2839,−17086); K32 (4540,−16808); K34 (6032,−16187); K36 (7168,−15284); K38 (7837,−14186) | computed |
| Gates 4/5 (homecoming) | Bars removed from K8/K10 (`HouseExteriorCampus.cpp:165`). Open iron gates and boards are drawn **in front of the solid wall** (`HouseCampusFallStadium.cpp:463-505`). | — |
| Wall glow strip | per segment at Z 360..600 (Dark when polish is on) | 147 |
| Seat rows | 12 rows, R .69 + .024·row (inner ellipse 3864×2622, outer 5477×3716), tread Z 180, 243 … 873 (63 step). Risers and back wall are added under homecoming. | 148-150 |
| Aisle stairs | at K%5==0, i.e. 0°, 45° … 315° | 157-158 |
| Rim posts and rail | K even, R 1.02, Z 1050..1170 | 152, 155 |
| Field (homecoming) | Painted field quad X −777..5577, Y −14801..−11799, Z −56. Grass to R .668. Track R .665..693. Goalposts at X −787 and 5587. | `HouseCampusFallStadium.cpp:171-200` |
| Press box | Dark box X 100..4700, Y −10000..−9250, Z 1050..1890; stone cap to 1920; glass on Y −9245 (street side); field-side windows at Y −10001.5, Z 1180..1430 (`HouseCampusFallStadium.cpp:366-368`). It overhangs the wall by 250 (centre) to 585 (ends). | 179-182 |
| Scoreboard | Dark box X 1550..3250, Y −17100..−16980, **Z 1190..2190**, face at Y −16978 toward the field. Homecoming bulb face origin (1630,−16972,1320), 1540×770 (`HouseCampusFallStadium.cpp:275`). | 189-190 |
| Light masts | Cylinder r20, Z −60..3840 at **35°: (7171,−11033); 90°: (2400,−9348); 145°: (−2371,−11033)**. Five EX_Flood heads each. | 202 |
| Stadium lights (3 of the 53) | Mast top + (0,−200,−150), then moved 35% toward the field: (5501,−11957,3690), (2400,−10861,3690), (−701,−11957,3690). Radius 6500, 60000 lm. | 202; `HouseExterior.cpp:1243-1244`; `HouseCampusFallLook.cpp:383-390` |
| Tree NoGo footprint | X −3450..8250, Y −17400..−8700 | 203 |
| Ticket booths (collide) | (3330,−9030) and (1560,−8975), 150×170, Z −60..170 | `HouseCampusFallStadium.cpp:488-489` |
| Gate pennant "mast" anchor | (2400,−9348) — the mast at 90° | `:490` |

**Stadium problems**

| Problem | Numbers |
|---|---|
| **No access** | The wall is continuous and collides on every segment from Z −60 to 990. The arches and gates are drawn in front of it. There is no ramp, tunnel or gap. The field (Z −56) and the stands can't be reached. |
| Mast through the press box | The 90° mast (2400,−9348, r20, Z −60..3840) passes through the press box (X 100..4700, Y −10000..−9250, Z 1050..1890). |
| Scoreboard has no supports | Bottom at Z 1190; the wall band top is at 1050 and the rim rail at 1170. Nothing reaches the ground. |
| Walks and lamps inside the bowl | See section 4 (X=7200, Y=−10300, Y=−16100) and lamps L25/L26 at r = 0.960 and 0.940, inside the closed risers. |

## 7. Clock tower (`HouseClockTowerMesh.h:9-15`, `HouseClockTowerMesh.cpp:20-25,517-522`; its own actor, spawned at `HouseExterior.cpp:2177`)

| Item | Value |
|---|---|
| Centre and grade | (−9000, −10800), Z −60 |
| Plinth (3 steps) | ±395 / ±372 / ±350 → **X −9395..−8605, Y −11195..−10405**, Z −100..15 |
| Base / shaft half-widths | 330 / 300 (buttresses reach about 376) |
| Heights above grade | base top 650, belfry 2780..3600, dial centre 4000 (r185), cornice 4400, parapet 4560, apex 6750, top 7040 (Z 6980) |
| Door | +Y face (toward walk Y=−10300, whose south edge is −10400): 5 cm from the plinth |
| Collision | Only `ClockTowerSolid` (plinth, base, shaft) |
| NoGo | **Not** in `BuildingFootprints`, on purpose (keeps the tree RNG) |
| **Sits on block 6's entrance pad** | The block 6 (LIBRARY) door is at Y −11600 and its pad runs Y −11600..−11050. The plinth overlaps the pad at X −9220..−8605, Y −11195..−11050. The library doors are 318 cm from the plinth's back step, and the W planter (Y −11380..−11220) is 25 cm off it. |
| Review sight line | `-ClockTowerReview` shot 7 and the `detail_clocktower_far` view look from (2400,−3750,140) to (−9000,−10800,4366) (`HouseClockTower.cpp:850`; `HouseExterior.cpp:231`). In plan the line crosses Y −5550 at X −511 and block 2's east-face plane (X −3800) at Y −7584, at height Z ≈ 2207, which is above block 2's parapet at 1314. It passes 240 south of block 1's SE corner (−6800,−9200). Height of the sight line ≈ 140 + 3800·t, where t = (2400 − X)/11400. **Any new mass under this line hides the tower.** |

## 8. Far blocks and Far lawn (`HouseExteriorCampus.cpp:204-206`)

- **Mesh:** `ExteriorFar` has **no collision** and no shadows (`HouseExterior.cpp:1568`).
- **Layout:** blocks come from `FRandomStream(82026)`, drawn in the order X, Y, H. Each is 3200 × 2800 and built by the same `Building()`, so each also has a +Y entrance and a sign.
- **Tree NoGo:** they write to `FLayout Unused`, so they have none.

| # | seed | centre X,Y | X min..max | Y min..max | H | style | sign | notes |
|---|---|---|---|---|---|---|---|---|
| F0 | 41 | -2856,-40804 | -4456..-1256 | -42204..-39404 | 1805 | 0 EX_Brick | SCIENCE CENTER | - |
| F1 | 42 | 643,-42299 | -957..2243 | -43699..-40899 | 2757 | 1 EX_CampusLimestone | UNIVERSITY LIBRARY | - |
| F2 | 43 | 21484,-36719 | 19884..23084 | -38119..-35319 | 2095 | 2 EX_CampusWarmBrick | UNIVERSITY HALL | - |
| F3 | 44 | 186,-32210 | -1414..1786 | -33610..-30810 | 1796 | 0 EX_Brick | SCIENCE CENTER | **reaches 1190 cm onto collidable ExteriorGrounds (Y > -32000)** |
| F4 | 45 | -7194,-34586 | -8794..-5594 | -35986..-33186 | 2810 | 1 EX_CampusLimestone | UNIVERSITY LIBRARY | - |
| F5 | 46 | -14381,-39668 | -15981..-12781 | -41068..-38268 | 2309 | 2 EX_CampusWarmBrick | UNIVERSITY HALL | - |
| F6 | 47 | 20796,-34634 | 19196..22396 | -36034..-33234 | 1770 | 0 EX_Brick | SCIENCE CENTER | - |
| F7 | 48 | 17976,-33119 | 16376..19576 | -34519..-31719 | 2659 | 1 EX_CampusLimestone | UNIVERSITY LIBRARY | **reaches 281 cm onto collidable ExteriorGrounds (Y > -32000)** |
| F8 | 49 | 22354,-38508 | 20754..23954 | -39908..-37108 | 2739 | 2 EX_CampusWarmBrick | UNIVERSITY HALL | - |
| F9 | 50 | -20344,-34086 | -21944..-18744 | -35486..-32686 | 2038 | 0 EX_Brick | SCIENCE CENTER | - |
| F10 | 51 | -8436,-38896 | -10036..-6836 | -40296..-37496 | 2313 | 1 EX_CampusLimestone | UNIVERSITY LIBRARY | - |
| F11 | 52 | 18125,-41267 | 16525..19725 | -42667..-39867 | 2340 | 2 EX_CampusWarmBrick | UNIVERSITY HALL | - |

## 9. Hedge "ring" (`Boundary()`, `HouseExterior.cpp:1250-1263`)

- **Shape:** each hedge is a box 120 thick, from Grade(midpoint) − 20 to + 190. It is part of `ExteriorGrounds`, so it **collides**.
- **Placement:** the ring sits at ±18800, which matches nothing else:
  - the old play box is X −8500..8800;
  - the world edge is ±32000;
  - the campus runs to Y −25650 and the Far blocks to −43699.
- **Z values** come from `Grade` at each segment's midpoint, checked with `grade.py` in the session scratchpad.

| segment (HouseExterior.cpp line) | X | Y | Z (from Grade at its midpoint) | crosses |
|---|---|---|---|---|
| HedgeS_W (1259) | -18800..8650 | -18860..-18740 | -80..130 | **B7 (-17000, -12600, -18860, -18740)**; **B8 (-11050, -7550, -18860, -18740)**; walk X=-11200; walk X=-6100; walk X=7200; blocks SideWalkW_S (8450, 8650, -18860, -18740) |
| HedgeS_E (1259) | 10350..18800 | -18860..-18740 | -80..130 | **B13 (10350, 12650, -18860, -18740)**; walk X=12800; blocks SideWalkE (10350, 10550, -18860, -18740) |
| HedgeN_W (1260) | -18800..8650 | 18740..18860 | -347..-137 (79 cm above the ground at its lowest, 23 cm gap at worst) | blocks SideWalkW_N (8450, 8650, 18740, 18860) |
| HedgeN_E (1260) | 10350..18800 | 18740..18860 | -382..-172 | blocks SideWalkE (10350, 10550, 18740, 18860) |
| HedgeW_S (1261) | -18860..-18740 | -18800..-5600 | -80..130 | blocks FarSidewalkW (-18860, -18740, -5800, -5600) |
| HedgeW_N (1261) | -18860..-18740 | -4100..18800 | -382..-172 | **buried** for Y −4100..~4700 (top Z −172, ground −60..−179); wall from Y ≈ 5100. So it does **not** actually block NearSidewalk1 (−4100..−3900) or show inside frat footprint C (X −18850..−15950) |
| HedgeE (1262) | 18740..18860 | -18800..18800 | -80..130 | floats over the rear: 27 cm gap from Y ≈ 250, walk-under (188+) from Y ≈ 1100 |

**What this means**

- **South run (Y −18800):**
  - It is a 190 cm wall across the middle of the campus.
  - It cuts through blocks 7, 8 and 13 and through all four N–S walks.
  - It leaves blocks 9, 10, 14 and 15 outside it.
  - Its "street gap" is X 8650..10350, but the street and sidewalks span 8450..10550, so the hedge covers 200 cm of each sidewalk.
- **West run (X −18800):**
  - The road gap is Y −5600..−4100, but the road and sidewalks span −5800..−3900, so the hedge covers 200 cm of the far sidewalk.
  - The north part (Y −4100..~4700) is **buried**: its top is at Z −172, below the −60..−179 ground. It reappears as a step-over at Y ≈ 4700 and as a wall from Y ≈ 5100.
- **East run (X 18800):** built at Z −80..130 along its whole length. North of Y ≈ 250 the ground falls away, leaving a 27 cm gap underneath, and from Y ≈ 1100 it floats 188+ cm (walk-under).
- **No hedge reaches the world edge**, so there is no closed ring.


## 10. Utility poles, lot poles, wires and lights on the campus side

- **Street poles** (`Pole()`, `HouseExterior.cpp:1101-1110`; `Lamps()`, `:1314-1321`):
  - Wooden pole, collision box ±16 to Z 1070 (`HouseStreetScene.cpp:1534-1576`).
  - Crossarm ±125 in pole-local X at local Y 22, Z 926.
  - Cobra-head lens at local (251, 0, 708).
  - **Each pole also adds one spot light** (part of the 53; street 48000 lm, lot 12500 lm).
  - `Pole()` **adds no NoGo**, so moving poles does not change the trees.
- **Lot poles** (`LotPoleMesh`, `:1578-1588`): concrete pier r23.5 and steel pole to Z 700.
- **Wire runs** (`BuildWires`, `HouseStreetScene.cpp:1590-1628`):
  - Poles are grouped by Y −3840, Y −5860 and X 8420, with spans of 3300 or less.
  - The 10 side-street spans run from (8420,−15000) to (8420,15000).
  - `IsStreetPole` hard-codes the same three lines (`:1528-1532`).

| X | Y | yaw | kind (HouseExterior.cpp line) | lens / spot light at (X,Y,Z) | flags |
|---|---|---|---|---|---|
| -13500 | -5860 | 90 | street pole, far (campus) kerb (1317) | (-13500,-5609,648) | - |
| -10500 | -5860 | 90 | street pole, far (campus) kerb (1317) | (-10500,-5609,648) | - |
| -7500 | -5860 | 90 | street pole, far (campus) kerb (1317) | (-7500,-5609,648) | in health lot |
| -4500 | -5860 | 90 | street pole, far (campus) kerb (1317) | (-4500,-5609,648) | on B2 entrance pad; in health lot |
| -1500 | -5860 | 90 | street pole, far (campus) kerb (1317) | (-1500,-5609,648) | in health lot |
| 1500 | -5860 | 90 | street pole, far (campus) kerb (1317) | (1500,-5609,648) | - |
| 4500 | -5860 | 90 | street pole, far (campus) kerb (1317) | (4500,-5609,648) | - |
| 7500 | -5860 | 90 | street pole, far (campus) kerb (1317) | (7500,-5609,648) | - |
| 8420 | -15000 | 0 | street pole, side street W (1318) | (8671,-15000,648) | - |
| 8420 | -12000 | 0 | street pole, side street W (1318) | (8671,-12000,648) | 64 cm from B11; **crossarm inside B11**; **lamp head + spot light inside B11** |
| 8420 | -9000 | 0 | street pole, side street W (1318) | (8671,-9000,648) | 34 cm from B3 |
| 8420 | -6000 | 0 | street pole, side street W (1318) | (8671,-6000,648) | on walk Y=-6000 |
| 8420 | -3000 | 0 | street pole, side street W (1318) | (8671,-3000,648) | - |
| 8420 | 0 | 0 | street pole, side street W (1318) | (8671,0,648) | - |
| -7000 | -6700 | 180 | lot pole (12500 lm) (1319) | (-7251,-6700,648) | **INSIDE B1**; in health lot; **lamp head + spot light inside B1** |
| -2500 | -7700 | 180 | lot pole (1320) | (-2751,-7700,648) | in health lot |
| -6500 | -8600 | 0 | lot pole (1320) | (-6249,-8600,648) | in health lot |

**Wires that pass through campus buildings** (side-street run; lines at pole-local offsets; Z = −60 + attach height minus 4·sag·t·(1−t))

| span | line (world X at local offset) | Z (sag) | passes through |
|---|---|---|---|
| (8420,-15000)-(8420,-12000) | phase E X=8525 | 900..944 | **B11 Y -14500..-11978** |
| (8420,-12000)-(8420,-9000) | phase E X=8525 | 900..944 | **B11 Y -11978..-11100** |
| (8420,-9000)-(8420,-6000) | phase W X=8315 | 900..944 | **B3 Y -8950..-6450** |
| (8420,-9000)-(8420,-6000) | phase E X=8525 | 900..944 | **B3 Y -8950..-6450** |
| (8420,-9000)-(8420,-6000) | top X=8420 | 985..1025 | **B3 Y -8950..-6450** |
| (8420,-9000)-(8420,-6000) | neutral X=8399 | 793..845 | **B3 Y -8950..-6450** |
| (8420,-9000)-(8420,-6000) | telephone cable X=8401 | 627..697 | **B3 Y -8950..-6450** |

## 11. Vehicles on the campus side (`Vehicles()`, `HouseExterior.cpp:1323-1336`)

- **Replay:** `B.Rng` draws 0–101 (one draw per slot, plus one for the yaw jitter when the slot is kept).
- **Car NoGo:** `Car()` adds NoGo X±500, Y±300 (`:1112-1117`), so **car slots feed the tree placement**.
- **Totals:** 38 site cars, plus 3 at the front kerb, 7 cruisers and 1 muscle car = 49 (`SurroundingsPolishReview parkedCars=49`).
- **House-side vehicles:** cruisers at (5000,−4190), (5900,−4200), (6900,−4190), (8770,−3300), (8790,−2000), (8770,−800) and (6880,−3150) (`:1950-1956`). None of them is on the campus.
- **Car box:** the table uses ±300 along the car and ±110 across it. Real sizes are 446–622 × 179–221 cm at ×1.1 (`HouseClassicCars.cpp:137-255`).
- **Health-lot slots:** of the 34, the 21 shown below are occupied.

| row (HouseExterior.cpp line) | site array | X | Y | yaw | approx box X / Y | flags |
|---|---|---|---|---|---|---|
| kerb far side (1330) | CarsA | -9000 | -5350 | 180.2 | -9300..-8700 / -5461..-5239 | - |
| kerb far side (1330) | CarsB | -7700 | -5350 | 179.1 | -8002..-7398 / -5465..-5235 | - |
| kerb far side (1330) | CarsC | -3800 | -5350 | 181.6 | -4103..-3497 / -5468..-5232 | - |
| kerb far side (1330) | CarsA | -1200 | -5350 | 178.8 | -1502..-898 / -5466..-5234 | - |
| kerb far side (1330) | CarsB | 4000 | -5350 | 181.3 | 3698..4302 / -5467..-5233 | - |
| health lot S row (1334) | CarsB | -8550 | -8250 | 89.3 | -8663..-8437 / -8551..-7949 | **INSIDE B1** |
| health lot S row (1334) | CarsC | -8050 | -8250 | 89.7 | -8162..-7938 / -8551..-7949 | **INSIDE B1** |
| health lot S row (1334) | CarsA | -7050 | -8250 | 90.0 | -7160..-6940 / -8550..-7950 | **INSIDE B1** |
| health lot S row (1334) | CarsB | -6050 | -8250 | 91.6 | -6169..-5931 / -8553..-7947 | walk X=-6100 |
| health lot S row (1334) | CarsC | -2550 | -8250 | 90.4 | -2662..-2438 / -8551..-7949 | - |
| health lot S row (1334) | CarsA | -2050 | -8250 | 91.4 | -2167..-1933 / -8553..-7947 | - |
| health lot S row (1334) | CarsB | -1550 | -8250 | 89.2 | -1664..-1436 / -8552..-7948 | - |
| health lot S row (1334) | CarsC | -550 | -8250 | 90.8 | -664..-436 / -8551..-7949 | - |
| health lot N row (1335) | CarsA | -8550 | -6150 | -89.3 | -8664..-8436 / -6451..-5849 | walk Y=-6000 |
| health lot N row (1335) | CarsB | -8050 | -6150 | -91.9 | -8170..-7930 / -6453..-5847 | B1 E planter; walk Y=-6000 |
| health lot N row (1335) | CarsC | -7550 | -6150 | -90.0 | -7660..-7440 / -6450..-5850 | pole (-7500,-5860); walk Y=-6000 |
| health lot N row (1335) | CarsA | -7050 | -6150 | -88.5 | -7168..-6932 / -6453..-5847 | walk Y=-6000 |
| health lot N row (1335) | CarsB | -6050 | -6150 | -88.2 | -6169..-5931 / -6453..-5847 | walk X=-6100; walk Y=-6000 |
| health lot N row (1335) | CarsC | -5050 | -6150 | -90.8 | -5164..-4936 / -6452..-5848 | **INSIDE B2**; walk Y=-6000 |
| health lot N row (1335) | CarsA | -4550 | -6150 | -91.2 | -4666..-4434 / -6452..-5848 | **INSIDE B2**; lamp (-4600,-5900); pole (-4500,-5860); walk Y=-6000 |
| health lot N row (1335) | CarsB | -4050 | -6150 | -89.5 | -4163..-3937 / -6451..-5849 | **INSIDE B2**; walk Y=-6000 |
| health lot N row (1335) | CarsC | -3550 | -6150 | -90.5 | -3663..-3437 / -6451..-5849 | walk Y=-6000 |
| health lot N row (1335) | CarsA | -3050 | -6150 | -91.0 | -3165..-2935 / -6452..-5848 | shuttle shelter; walk Y=-6000 |
| health lot N row (1335) | CarsB | -2050 | -6150 | -91.4 | -2167..-1933 / -6453..-5847 | walk Y=-6000 |
| health lot N row (1335) | CarsC | -1550 | -6150 | -90.8 | -1664..-1436 / -6452..-5848 | pole (-1500,-5860); walk Y=-6000 |
| health lot N row (1335) | CarsA | -1050 | -6150 | -89.3 | -1164..-936 / -6451..-5849 | bench (-1200,-6070); bin (-1015,-6070); walk Y=-6000 |
| health lot N row (1335) | CarsB | -550 | -6150 | -88.6 | -667..-433 / -6453..-5847 | walk Y=-6000 |

## 12. Street furniture on the campus side

- `ExteriorStreetDecor` has **no collision** (`HouseExterior.cpp:1575`).
- `HouseSurroundings::Street` is at `HouseSurroundingsPolish.cpp:124-179`.

| Item | Geometry | Line | Conflict |
|---|---|---|---|
| CAMPUS SHUTTLE shelter pad | X −3210..−2490, Y −6350..−5810, Z −90..−55 | 170 | Sits in the health lot and **straddles walk Y=−6000** |
| Shelter posts / roof / glass | posts at X −3160 and −2540 × Y −6260 and −5870, Z −55..230. Roof X −3210..−2490, Y −6340..−5810, Z 230..244. Glass at Y −6266. | 171-173 | **Lamp L7 (−2700,−5900) passes through the roof.** Car (−3050,−6150) is parked through it. Birch (−2424,−5900) is 66 cm off its east edge. |
| Shelter bench / bin | bench (−2850,−6170), bin (−3370,−6130) | 174 | — |
| Shuttle sign | 400×75 at (−2850,−5812, Z 210), facing +Y | 175 | — |
| Bench and bin | bench (−1200,−6070), bin (−1015,−6070) | 176 | **Car (−1050,−6150) is parked through both** |
| Bench and bin (house side) | (7650,−3720) and (7835,−3720) | 176 | — |
| Road repairs and cracks | 44 random patches, X −19400..7600, Y −5450..−4250 (seed 12102026) | 128-135 | — |


## 13. Homecoming dressing (HouseCampusFall*). Positions are fixed except where a FirstClear pick is noted.

| Item | Position / extent | Collision | Line |
|---|---|---|---|
| Dorm dressing | Block 1 rect (−10600..−6800 × −9200..−6600) and block 4 rect (12350..16650 × −10250..−6650), **hard-coded** | no | `HouseCampusFallQuad.cpp:357-358` |
| Lamp banners and halos | the same lamp grid as section 5, minus L25/L26 | no | `:141-158` |
| Kiosk | (2400,−6330), solid drum r58, Z −60..198 | yes | `:361-362`, `:161-182` |
| Kiosk plaza | disc r300 about the kiosk, Z −56.4 (touches walk Y=−6000 at Y −6100..−6030) | no | `:196-201` |
| Gate path | X 2250..2550, Y −6600..−8930, Z −56.4; chalk at Y −6800..−8450 | no | `:202-216` |
| Phone booth | (1950,−6045), 88×88, Z −57..169 (on walk Y=−6000) | yes | `:364` |
| News boxes | (2240,−6060), (2310,−6064), (2378,−6060) (on the walk) | yes | `:365-367` |
| Bike rack | from (1180,−6270): solid X 1150..1660, Y −6370..−6170 | yes | `:368`, `:309` |
| Leaf piles under green trees | up to 7, around trees with X −3000..7600 and Y −8700..−5800 | no | `:371-388` |
| Leaf pile, rake, bags | FirstClear → **(4300,−7050)**; second pile (4560,−6930) | no | `:390-405` |
| Bonfire | FirstClear → **(3750,−7700)** (matches the log); stone ring r158 | no (hay seats are `Solid`) | `:408-409` |
| Falling-leaf box | X −300..7200, Y −8900..−5600, Z −60..900 | — | `:410` |
| Float | FirstClear → **(4700,−8250)**, yaw −38; deck 800×260 | yes | `HouseCampusFallStadium.cpp:609-610`, `:513-521` |
| Band trailer | FirstClear → **(1000,−8150)**, yaw 15; body 650×240, Z 3..233 | yes | `:611-612`, `:568-575` |
| Ticket booths, gate boards, hay, corn, sheets | see section 6 | booths and hay yes | `:463-505` |
| Autumn trees (by position ±40) | red: (−7277,−5900) maple, (5559,−8315) oak. Orange: (−2424,−5900), (7365,−5900), (−8320,−9540) birches. | — | `HouseCampusFallLook.cpp:352-360` |


## 14. Trees on the campus side or within 300 cm of a road (97 trees)

- **Selection:** every `Big`/`Small` tree in the CSV with Y < −5500 or ≤ 300 cm from either carriageway. `Distant` trees all have Y > 6000, so none qualify.
- **Columns:**
  - "HISM batch #inst" is the instance index inside `ExteriorTree<Species><Variant>`, worked out with the assignment rule at `HouseExterior.cpp:1682-1698`. Autumn trees are drawn by `CampusFallTree…` instead (`:1702`).
  - Z is −65 for all of them (Grade −60, minus 5).
  - The scale column shows the effective instance scale (Small trees are ×.71 for maple and ×.82 for birch).
- **Placement rules:**
  - Grid trees with max(|X|,|Y|) > 19500 skip the NoGo test (`:1304`), which is why trees stand in blocks 9, 14 and 15 and on the side street.
  - The street-row trees at Y −5900 are placed with no NoGo test at all (`:1291-1296`).
- **Totals:** 7 trees inside buildings; 7 on walk Y=−6000 (2 of them also in the health lot); 2 on the side-street carriageway and 1 on its E sidewalk; 1 on a house-side sidewalk.

| HISM batch #inst | site array | X | Y | yaw | scale XY / Z | homecoming tone | origin (HouseExterior.cpp) | flags |
|---|---|---|---|---|---|---|---|---|
| Oak0 #4 | Big[18] | -23336 | -4111 | 125.5 | 1.06 / 1.02 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | 39 cm from FrontRoad; on NearSidewalk1 |
| Oak1 #23 | Big[97] | 23789 | -5796 | -51.1 | 1.17 / 1.20 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #13 | Small[40] | -21457 | -5826 | 77.4 | 0.70 / 0.70 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | 276 cm from FrontRoad |
| Birch0 #1 | Small[3] | -16971 | -5900 | 45.4 | 0.89 / 0.93 | - | street row Y=-5900 (1295) | **on walk Y=-6000** |
| Maple2 #12 | Small[5] | -14463 | -5900 | -40.2 | 0.63 / 0.61 | - | street row Y=-5900 (1295) | **on walk Y=-6000** |
| Birch1 #2 | Small[7] | -12036 | -5900 | 78.8 | 0.76 / 0.83 | - | street row Y=-5900 (1295) | **on walk Y=-6000** |
| Birch0 #3 | Small[9] | -9698 | -5900 | 59.7 | 0.99 / 0.99 | - | street row Y=-5900 (1295) | **on walk Y=-6000** |
| Maple2 #14 | Small[11] | -7277 | -5900 | 157.3 | 0.75 / 0.68 | red | street row Y=-5900 (1295) | **on walk Y=-6000**; in health-lot asphalt |
| Birch1 #4 | Small[13] | -2424 | -5900 | -21.2 | 0.88 / 0.83 | orange | street row Y=-5900 (1295) | **on walk Y=-6000**; in health-lot asphalt; 66 cm from shuttle shelter |
| Birch0 #5 | Small[15] | 7365 | -5900 | -84.0 | 1.01 / 0.94 | orange | street row Y=-5900 (1295) | **on walk Y=-6000** |
| Oak2 #21 | Big[86] | 20573 | -5920 | 155.0 | 1.08 / 1.05 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #8 | Big[104] | 26713 | -5983 | 147.5 | 1.06 / 0.97 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak0 #1 | Big[6] | -26927 | -7493 | 29.5 | 1.11 / 1.16 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #50 | Small[119] | 26792 | -8135 | 96.6 | 0.86 / 0.79 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple0 #8 | Big[96] | 23793 | -8199 | -70.4 | 1.15 / 1.16 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak1 #14 | Big[61] | 5559 | -8315 | 119.9 | 1.12 / 1.03 | red | grid (1297-1309) | - |
| Maple2 #17 | Small[20] | -24580 | -8478 | -104.6 | 0.68 / 0.62 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #37 | Small[112] | 22167 | -8541 | -6.6 | 0.89 / 0.81 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch0 #13 | Small[39] | -20792 | -8565 | -15.9 | 0.76 / 0.69 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch0 #20 | Small[60] | -8320 | -9540 | -35.4 | 0.93 / 0.98 | orange | grid (1297-1309) | - |
| Oak1 #20 | Big[85] | 20593 | -9656 | -10.6 | 1.09 / 1.18 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #23 | Small[38] | -20766 | -9816 | 23.0 | 0.64 / 0.60 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak2 #23 | Big[95] | 23793 | -9817 | 144.1 | 1.05 / 1.11 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #12 | Small[37] | -21275 | -11424 | 105.8 | 0.76 / 0.80 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak0 #7 | Big[30] | -19531 | -11492 | -46.6 | 1.01 / 1.10 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch0 #37 | Small[111] | 22848 | -12054 | 132.4 | 0.82 / 0.76 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #16 | Small[49] | -17084 | -13520 | 74.9 | 0.88 / 0.82 | - | grid (1297-1309) | - |
| Birch1 #6 | Small[19] | -24557 | -13986 | -137.9 | 0.79 / 0.84 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch0 #12 | Small[36] | -21206 | -14125 | 96.4 | 0.97 / 1.00 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak1 #24 | Big[103] | 25845 | -14154 | -28.9 | 1.17 / 1.18 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch0 #34 | Small[102] | 20078 | -14299 | -38.3 | 0.90 / 0.85 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #22 | Small[35] | -21430 | -15119 | -115.3 | 0.79 / 0.72 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #20 | Small[29] | -23436 | -15273 | -97.8 | 0.79 / 0.83 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #47 | Small[110] | 22528 | -15404 | 32.7 | 0.77 / 0.73 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #39 | Small[118] | 26288 | -15444 | 35.0 | 0.97 / 0.98 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak2 #6 | Big[26] | -21473 | -17048 | 62.7 | 0.93 / 0.84 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #36 | Small[109] | 22185 | -17229 | -121.6 | 0.84 / 0.88 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #9 | Small[28] | -22810 | -17682 | -67.4 | 0.94 / 0.89 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak1 #5 | Big[25] | -21315 | -18865 | 60.2 | 0.91 / 0.93 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple0 #7 | Big[84] | 20837 | -19048 | -106.0 | 1.05 / 0.97 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #22 | Small[67] | -2564 | -19064 | 22.7 | 0.82 / 0.74 | - | grid (1297-1309) | - |
| Birch0 #15 | Small[45] | -19218 | -19485 | -126.7 | 0.71 / 0.67 | - | grid (1297-1309) | - |
| Birch0 #32 | Small[96] | 16699 | -19669 | -52.6 | 0.88 / 0.87 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple0 #1 | Big[12] | -25613 | -19828 | -163.6 | 0.98 / 1.06 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak2 #1 | Big[5] | -26857 | -19838 | -97.4 | 1.01 / 1.02 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #44 | Small[101] | 18016 | -19857 | 106.0 | 0.72 / 0.72 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch0 #36 | Small[108] | 22891 | -19911 | 120.9 | 1.02 / 1.09 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch0 #22 | Small[66] | -1743 | -20724 | -82.8 | 0.90 / 0.87 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak0 #12 | Big[51] | -6255 | -20865 | -124.2 | 1.03 / 0.99 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | 55 cm from walk X=-6100; 135 cm from walk Y=-21100 |
| Maple0 #2 | Big[24] | -20946 | -20951 | -165.9 | 0.87 / 0.89 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak1 #16 | Big[70] | 9166 | -21158 | -3.1 | 1.16 / 1.11 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | **ON SideStreet**; **on walk Y=-21100** |
| Maple0 #3 | Big[36] | -15273 | -21294 | -48.5 | 1.01 / 0.94 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | **INSIDE B9**; 94 cm from walk Y=-21100 |
| Maple2 #42 | Small[95] | 16946 | -21295 | -31.3 | 0.77 / 0.76 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | **INSIDE B14**; 95 cm from walk Y=-21100 |
| Birch0 #16 | Small[48] | -17788 | -21326 | -89.0 | 0.93 / 0.94 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | 126 cm from walk Y=-21100 |
| Birch1 #33 | Small[100] | 18708 | -21362 | 15.1 | 0.77 / 0.72 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak2 #7 | Big[29] | -18809 | -21495 | 113.8 | 1.24 / 1.18 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #29 | Small[56] | -9945 | -21510 | 126.8 | 0.63 / 0.63 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #36 | Small[77] | 3034 | -21572 | 61.2 | 0.63 / 0.69 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | **INSIDE B15** |
| Oak2 #4 | Big[17] | -23525 | -21774 | 87.1 | 0.88 / 0.94 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak0 #18 | Big[75] | 14685 | -21779 | 49.3 | 1.08 / 1.00 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | **INSIDE B14** |
| Maple2 #46 | Small[107] | 21890 | -21802 | 138.9 | 0.68 / 0.67 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak0 #16 | Big[66] | 7347 | -21896 | 133.9 | 1.16 / 1.09 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | 47 cm from walk X=7200 |
| Birch1 #35 | Small[106] | 22228 | -22788 | -81.3 | 1.00 / 0.96 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak2 #17 | Big[71] | 10922 | -22807 | 41.3 | 0.91 / 0.83 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple1 #3 | Big[28] | -19970 | -23018 | -107.0 | 0.92 / 0.91 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch0 #6 | Small[18] | -24982 | -23059 | 40.6 | 0.76 / 0.83 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak1 #18 | Big[79] | 16582 | -23166 | -33.7 | 1.04 / 1.01 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | **INSIDE B14** |
| Oak2 #20 | Big[83] | 18805 | -23253 | -87.7 | 0.97 / 1.00 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak2 #5 | Big[23] | -21089 | -23287 | -51.0 | 1.09 / 1.05 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak1 #10 | Big[46] | -10435 | -23428 | -44.0 | 0.95 / 0.96 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak0 #25 | Big[102] | 26088 | -23469 | 2.3 | 0.96 / 0.94 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch0 #17 | Small[51] | -14172 | -23555 | 109.2 | 0.82 / 0.81 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | **INSIDE B9** |
| Birch0 #30 | Small[90] | 14251 | -23582 | 148.1 | 0.98 / 1.07 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | **INSIDE B14** |
| Oak2 #16 | Big[65] | 6958 | -24532 | -166.5 | 0.91 / 0.95 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | 142 cm from walk X=7200 |
| Birch0 #24 | Small[72] | -884 | -24705 | 98.8 | 0.91 / 0.87 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #2 | Big[32] | -17839 | -24714 | -114.1 | 1.24 / 1.33 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak2 #8 | Big[35] | -15089 | -24716 | 45.7 | 1.24 / 1.27 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #26 | Small[79] | 5559 | -24805 | 152.3 | 0.70 / 0.76 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch0 #9 | Small[27] | -23044 | -25015 | -113.5 | 0.88 / 0.91 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #32 | Small[65] | -3660 | -25110 | -31.2 | 0.78 / 0.75 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #18 | Small[55] | -10170 | -25298 | 128.9 | 0.81 / 0.78 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #31 | Small[94] | 16604 | -25359 | -135.4 | 0.72 / 0.78 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #40 | Small[89] | 15098 | -25461 | 11.8 | 0.62 / 0.60 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak2 #10 | Big[41] | -13632 | -25470 | 84.4 | 1.11 / 1.09 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch0 #27 | Small[81] | 9686 | -26492 | 16.5 | 0.98 / 0.98 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | **ON SideStreet** |
| Oak0 #11 | Big[45] | -12298 | -26528 | 82.1 | 0.85 / 0.91 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #30 | Small[59] | -7793 | -26538 | -146.1 | 0.64 / 0.70 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #27 | Small[50] | -13146 | -26582 | 137.8 | 0.70 / 0.66 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #34 | Small[71] | -941 | -26690 | 16.5 | 0.67 / 0.62 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #11 | Small[34] | -20915 | -26832 | 133.0 | 0.93 / 0.98 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #27 | Small[82] | 10515 | -26874 | 151.2 | 0.80 / 0.78 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | 215 cm from SideStreet; on SideWalkE |
| Oak2 #18 | Big[74] | 14766 | -26886 | 1.4 | 1.10 / 1.08 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Maple2 #25 | Small[44] | -19339 | -27054 | -57.9 | 0.63 / 0.63 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak2 #12 | Big[50] | -6670 | -27204 | -39.0 | 0.98 / 1.03 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch0 #31 | Small[93] | 16626 | -27304 | -95.8 | 0.97 / 0.92 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Oak2 #2 | Big[11] | -25089 | -27353 | -81.4 | 1.23 / 1.22 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |
| Birch1 #5 | Small[16] | -26484 | -27369 | 104.1 | 0.77 / 0.79 | - | grid (1297-1309), no NoGo check (\|X\| or \|Y\| > 19500) | - |

## 15. Known layout bugs: verdicts and exact numbers

| # | Reported bug | Verdict | Exact numbers | Source |
|---|---|---|---|---|
| 1 | Side street through blocks 3, 11 and 13 | **CONFIRMED** | The street (X 8700..10300, Y ±32000) and its sidewalks (8450..8700, 10300..10550) are cut through:<br>• **B3** (7850..11350 × −8950..−6450): the whole 1600-wide carriageway plus both sidewalks, for 2500 cm. B3's portico (9150..10050) and pad (9080..10120 × −6450..−5900) **stand in the street**.<br>• **B11** (8500..11500 × −14500..−11100): the whole carriageway for 3400 cm. Portico 9550..10450 in the street; pad on the street and the E sidewalk.<br>• **B13** (9350..12650 × −20700..−17500): X 9350..10300 of the street (950 of 1600) plus the E sidewalk, for 3200 cm. Pad on the E sidewalk.<br>Going south from the junction, the street **dead-ends into B3's front door** 900 cm past the far kerb. | `HouseExterior.cpp:1162,1174,1177`; `HouseExteriorCampus.cpp:122-128` |
| 2 | Utility poles inside the campus | **CONFIRMED** | • Four side-street poles with spot lights stand on the campus: (8420,−6000), (8420,−9000), (8420,−12000), (8420,−15000) (`:1318`).<br>• (8420,−6000) stands **on walk Y=−6000**.<br>• (8420,−9000) is 34 cm off B3's south face.<br>• (8420,−12000): **crossarm and lamp head inside B11, with its spot light at (8671,−12000,648) inside B11**.<br>• **All 5 wires of span −9000→−6000 pass straight through B3** (Z 627..1025). The E phase wire (X 8525) runs through B11 for Y −14500..−11100.<br>• The far-kerb row (Y −5860, `:1317`) has 3 poles in the health lot, and (−4500,−5860) stands **on B2's entrance pad in the lot-drive mouth**.<br>• Health-lot poles (`:1319-1320`): (−7000,−6700) is **inside B1, with its light (−7251,−6700,648) inside B1**; (−2500,−7700) and (−6500,−8600) are in the open lot. | section 10 |
| 3 | Health lot under block 1 | **CONFIRMED, and worse** | Lot X −9000..0 × Y −8600..−5800 (`:1187`):<br>• **overlaps B1 by 2200 × 2000** (−9000..−6800 × −8600..−6600);<br>• **contains B2 entirely** (−5900..−3800 × −7850..−6150).<br>B2's pad (−6150..−5600) runs over the lot edge and the far sidewalk; the lot drive (−5000..−4400) points into B2's portico.<br>Parked cars: 3 inside B1 (S row X −8550, −8050, −7050) and 3 inside B2 (N row X −5050, −4550, −4050). All 13 N-row cars are half on walk Y=−6000. Bay lines also run under B1, B2 and the walk. | sections 2, 11 |
| 4 | Walks through buildings (X 12800, X −6100, X 7200 under the stands, Y −16100, Y −21100) | **CONFIRMED, plus 2 more** | • X=12800 **through B4** (Y −10250..−6650), and 50/100 cm off B12 and B13.<br>• X=−6100 **through B10** (Y −25650..−22550) and across the health lot (Y −8600..−6100).<br>• X=7200 **under the E stands, Y −15250..−11350**.<br>• Y=−16100 **through B7** (X −17000..−12600), **clips B12** (X 12900..17100, 150 deep) and runs **under the S stands, X −1380..6180**.<br>• Y=−21100 **through B9** (X −16700..−11900) and **B14** (X 13400..17600).<br>• Extra: Y=−10300 clips **B4** (X 12350..16650, 50 deep) and runs **under the N stands, X −1030..5830**.<br>• Extra: all four E–W walks cross the side street, and Y=−6000 crosses the health lot and runs under the shuttle shelter.<br>• All four N–S walks are cut by the S hedge at Y −18800. | section 4 |
| 5 | Lamps under the stands | **CONFIRMED** | L25 (7300,−11800) at r 0.960 and L26 (7300,−14600) at r 0.940, i.e. under seat rows 11 and 10. Their heads (Z 447..468) are inside the closed risers. Homecoming skips their banners (`HouseCampusFallQuad.cpp:148`). | `HouseExteriorCampus.cpp:137` |
| 6 | Trees inside buildings | **CONFIRMED: 7** | • B9: Maple0 #3 (−15273,−21294), Birch0 #17 (−14172,−23555).<br>• B14: Maple2 #42 (16946,−21295), Oak0 #18 (14685,−21779), Oak1 #18 (16582,−23166), Birch0 #30 (14251,−23582).<br>• B15: Maple2 #36 (3034,−21572).<br>All are past \|19500\|, where the NoGo test is skipped. | section 14; `HouseExterior.cpp:1304` |
| 7 | No stadium access | **CONFIRMED** | The 40-segment wall collides from Z −60 to 990. Gates 4/5 (K8 (3705,−9608), K10 (1961,−9514)) are dressing in front of a solid panel. | section 6 |
| 8 | All entrances face +Y | **CONFIRMED** | All 16 entrances are on the max-Y face. • 9 face the back of another block 1400–4350 cm away (B5→B0, B7→B5, B8→B6, B9→B7, B10→B8, B11→B3, B12→B4, B13→B11, B14→B12). • B15 faces the stadium's outer wall (4460). • B6 faces the clock tower (405 to the plinth, which sits on its pad). • B0–B4 face the road (600–1100 cm to the far kerb), and B3 and B11 open into the side street. • B2's pad crosses the lot edge. • The 12 Far blocks also face +Y. | `HouseExteriorCampus.cpp:53-62` |
| 9 | Flat campus | **CONFIRMED** | `Grade()` returns −60 for every Y ≤ 0 (`HouseExterior.cpp:114`). Terrain cells are 1600 on the campus; buildings, walks and the stadium all sit at −60. | section 1 |
| 10 | Roads run to the world edge | **CONFIRMED** | The front road runs X −32000..8700 and its sidewalks −32000.., so its west end stops at the terrain edge. The side street, both sidewalks and the centre dashes run Y −32000..+32000. Past ±32000 there is no ground (south: the non-colliding Far lawn). | `HouseExterior.cpp:1161-1177` |
| 11 | Far lawn past Y −31800 doesn't collide | **CONFIRMED** | `ExteriorFar` is built with `bCollision=false` (`HouseExterior.cpp:1568`): lawn X ±31000 × Y −49000..−31800, Z −135..−63. Collidable ground ends at Y −32000. **F3 (−1414..1786 × −33610..−30810) and F7 (16376..19576 × −34519..−31719) poke 1190 and 281 cm onto the collidable ground and can be walked through.** | sections 1, 8 |
| 12 | Broken hedge ring | **CONFIRMED** | Ring at ±18800: through B7, B8 and B13 and all four N–S walks; covers 200 of the 250 cm of the side-street and west far sidewalks; W run buried for Y −4100..~4700; E run floats over the rear; nothing closes at the world edge. | section 9 |


## 16. Other problems found (not on the list)

| # | Problem | Numbers | Source |
|---|---|---|---|
| A | Clock tower stands on block 6's (LIBRARY) entrance pad | overlap X −9220..−8605, Y −11195..−11050; library doors 318 cm from the plinth | §7 |
| B | Mast through the press box | mast (2400,−9348) r20 against the box X 100..4700, Y −10000..−9250, Z 1050..1890 | §6 |
| C | Scoreboard floats | bottom Z 1190, nothing below it to the wall top at 1050 | `HouseExteriorCampus.cpp:189` |
| D | Lamp L7 through the shuttle-shelter roof; L13 on the side-street kerb line (8700,−5900); L6 on B2's pad | — | §5, §12 |
| E | Parked cars through the shelter, the bench and bin (−1200,−6070), a B1 planter, the lot poles and lamp L6 | health-lot N row | §11 |
| F | Two autumn street trees grow out of the health-lot asphalt; every Y −5900 street-row tree straddles the walk's north edge | (−7277,−5900), (−2424,−5900); 7 trees on Y −5900 | §14 |
| G | Tree in the side-street carriageway and on walk Y=−21100; trees on the side street and its E sidewalk | (9166,−21158); (9686,−26492); (10515,−26874) | §14 |
| H | Bench (−11000, −9700/−15900) overlaps walk X=−11200 by 10 cm | — | §5 |
| I | Duplicate building names | 6 LIBRARY, 5 HALL, 5 SCIENCE CENTER, and the Far blocks repeat them | `HouseSurroundingsPolish.cpp:120` |
| J | No path connects any campus entrance to a walk except where a pad happens to touch one (B0, B1, B2, B3, B8, B15); B4–B7 and B9–B14 have no path at all | — | §3 |
| K | No kerb cut at the health-lot drive; the far kerb runs across it; drive top −58, kerb top −57, road −72 | X −5000..−4400 | `HouseExterior.cpp:1178,1188` |
| L | Stop bar in the westbound (exit) lane; stop sign and both street-name blades face −Y and are parallel (the side-street blade should be turned 90°) | stop bar X 8150..8175, Y −5480..−4900; post (8080,−3810) | `HouseSurroundingsPolish.cpp:155-168` |
| M | Side-street kerb joints and decal markings stop at Y −5900 / X −9000..8450; the campus stretch of the side street has plain asphalt and centre dashes running under the blocks | — | `HouseSurroundingsPolish.cpp:141`; `HouseExterior.cpp:1167` |
| N | Street-name text "PAT HEAD SUMMITT ST" (and "FRATERNITY PARK DR") names a real person and real University of Tennessee streets, while the school is the invented Calder State (the campus README says no real school, team or mark is used) | `EX_StreetNameSide` | `HouseSurroundingsPolish.cpp:82-83` |
| O | Entrance "doors" are a glass panel on a solid stone block; the bodies are solid boxes (Z −130..H) with collision, so there is nothing to enter | — | `HouseExteriorCampus.cpp:46,54-55` |
| P | Floor-0 windows behind the porticos (CX±450, Z up to 330) are buried in the stone | — | `HouseExteriorCampus.cpp:31-38,54` |

## 17. Constraints the master job must keep (from the code)

| Constraint | Detail | Source |
|---|---|---|
| Tree CRC `0f3b2bb7` | It is `MemCrc32` over every TreeA/TreeB/ForestDistant transform (`HouseWoodsPass.cpp:40-42`). The trees depend on: (a) the **number of `B.Rng` draws before `Plants()`**, i.e. the 64 car slots in `Vehicles()` (`HouseExterior.cpp:1330-1335`) and their skip results; (b) **every NoGo box** tested at a grid candidate with max(\|X\|,\|Y\|) ≤ 19500 — campus footprints +150 (`HouseExteriorCampus.cpp:44,203`), walk bands (`HouseExterior.cpp:1246-1247`), car boxes (`:1116`), frat row, street and house NoGos, and non-detail ExViews ±800 (`:1273`); (c) `NearView` (non-detail ExViews ±900); (d) `HouseFallJam::Inside`. **Any change to these changes every later tree.** Safe pattern: keep the legacy NoGo list and the 64 car slots exactly as they are, build the new layout without adding NoGo before `Plants()`, and remap or hide cars afterwards, as `HouseParking::ParkCars` already does (`HouseParkingLot.cpp:122-131`). | — |
| Trees are fixed | New footprints must avoid the 97 trees in §14, or the plan has to accept them. The five autumn trees are matched by position ±40 (`HouseCampusFallLook.cpp:357`). | — |
| Lights ≤ 53, no new ones | 18 are on the campus side: 8 far-kerb poles, 4 side-street poles at Y ≤ −6000, 3 health-lot poles and 3 stadium lights. Moving a `Pole()` moves its light; the count stays the same. `-ClockTowerReview` fails if `ExteriorLights > 53` (`HouseClockTower.cpp:896`). | §6, §10 |
| `-CampusWoodsReview` | Every `ExteriorNear` vertex must have **Y < −5550**. `EX_CampusSeats` vertices must have Y < −8000. No campus-brick materials in `ExteriorFar` with Y > 0. Ridge vertices Y > 30000. Near and Far cast no shadows. | `HouseCampusReview.cpp:31-46` |
| `-SurroundingsPolishReview` | `ExteriorStreetDecor` vertices must lie in Y < −2700, X > 8400, or the rear or west lots. Road-floor traces must hit at (−7500,−4800), (−3000,−4800), (3000,−4800), (8200,−4900) and (9550,500). At least 5 car models and more than 10 cars. | `HouseCampusReview.cpp:48-70` |
| `-FreeRoamWalkReview` | The walker must pass (3100,−5000)→(3100,−7000), (−7800,−4850)→(−9400,−4850) and (8000,−4850)→(9600,−4850) with nothing in the way. | `HouseFreeRoamReview.cpp:29-32` |
| ExViews | Campus cameras at `HouseExterior.cpp:231-245` (clocktower ×5, stadium ×4, campus ×3) plus `front_lawn_overview` (150), `high` (158) and `detail_establishing_*` (194-197). Add new views only with a `detail_` prefix; anything else adds NoGo and changes the trees. | — |
| Clock-tower sight line | (2400,−3750,140) → (−9000,−10800,4366); see §7 | — |
| Runtime only | Everything is generated in `AHouseExteriorDetail::Build` and the `AHouseCampusFall` / `AHouseClockTower` actors; nothing is saved to the map | — |

## 18. Constants copied across files (change them together)

| What | Copies |
|---|---|
| Block rects | `HouseExteriorCampus.cpp:121-129`; dorm rects `HouseCampusFallQuad.cpp:357-358`; `IsDorm` seeds `HouseCampusFallLook.cpp:374`; sign `Seed%3` `HouseSurroundingsPolish.cpp:120` |
| Lamp grid | `HouseExteriorCampus.cpp:136-137` and `HouseCampusFallQuad.cpp:156-157` (plus the ellipse skip at `:148`) |
| Stadium | `HouseExteriorCampus.h:7`, `HouseExteriorCampus.cpp:143`; `HouseCampusFallStadium.cpp:15-19,275,366,488-490`; `HouseCampusFallLook.cpp:19` (FieldCX/CY) |
| Health lot | `HouseExterior.cpp:1187-1188,1191,1319-1320,1334-1335,1876`; `HouseExteriorNature.cpp:72` |
| Street pole lines | `HouseExterior.cpp:1316-1318`; `HouseStreetScene.cpp:1528-1532,1597-1600` |
| Front-road Y values | `HouseExteriorSiteLayout.h:8-10`; `HouseStreetScene.cpp:17-19`; `HouseSurroundingsPolish.cpp:131,138-139,145`; `HouseExteriorNature.cpp:69` |
| Clock tower site | `HouseClockTowerMesh.h:11`; review views `HouseClockTower.cpp:850-853`; ExViews `HouseExterior.cpp:231-235` |
| Homecoming props | `HouseCampusFallQuad.cpp:361-368,390,408-410`; `HouseCampusFallStadium.cpp:488-490,609-612` |


## 19. Plan primitives (to draw a top-down plan)

- Rects are X0..X1, Y0..Y1. For an ellipse, use x = 2400 + rx·cos θ, y = −13300 + ry·sin θ.
- For point objects (lamps, poles, trees, cars) use the tables in §5, §10, §11 and §14.

| id | layer | shape | coordinates (cm) | Z | collides | source |
|---|---|---|---|---|---|---|
| WORLD | ground | rect | X -32000..32000, Y -32000..32000 | -60 (Y<=0) | yes | HouseExterior.cpp:1121 |
| FARLAWN | ground | rect | X -31000..31000, Y -49000..-31800 | -135..-63 | NO | HouseExteriorCampus.cpp:205 |
| ROAD_FRONT | road | rect | X -32000..8700, Y -5550..-4150 | -72 | yes | HouseExterior.cpp:1161 |
| ROAD_SIDE | road | rect | X 8700..10300, Y -32000..32000 | -72 (campus) | yes | :1162 |
| KERB_FAR | kerb | rect | X -32000..8700, Y -5550..-5530 | -57 | yes | :1178 |
| KERB_NEAR | kerb | rect | X -32000..8700, Y -4170..-4150 | -57 | yes | :1179 |
| SW_FAR_W | sidewalk | rect | X -32000..-5000, Y -5800..-5550 | -57 | yes | :1173 |
| SW_FAR_E | sidewalk | rect | X -4400..8450, Y -5800..-5550 | -57 | yes | :1173 |
| SW_NEAR | sidewalk | rect | X -32000..-5800 / -5200..3450 / 4050..8450, Y -4150..-3900 | -57 | yes | :1172 |
| SW_SIDE_W_S | sidewalk | rect | X 8450..8700, Y -32000..-5800 | -57 | yes | :1174 |
| SW_SIDE_E | sidewalk | rect | X 10300..10550, Y -32000..32000 | -57 (campus) | yes | :1177 |
| HEALTH_LOT | lot | rect | X -9000..0, Y -8600..-5800 | -58 | yes | :1187 |
| HEALTH_DRIVE | lot | rect | X -5000..-4400, Y -5800..-5530 | -58 | yes | :1188 |
| CROSSWALK | marking | rect | X 8320..8510, Y -5490..-4220 | -71 | no | HouseSurroundingsPolish.cpp:155 |
| SIGNPOST | sign | point | (8080,-3810) | -60..265 | no | :157 |
| HOUSE | anchor | rect | X -3398..3338, Y -2198..1718 | - | yes | HouseExterior.cpp:1131 |
| FRONT_LAWN | anchor | rect | X -3550..3300, Y -3900..-2550 | -60 | - | :1200 |
| WEST_LOT | anchor | rect | X -7200..-3700, Y -2500..-300 | -58 | yes | :1185 |
| WEST_LOT_DRIVE | anchor | rect | X -5800..-5200, Y -4190..-2500 | -58 | yes | :1186 |
| SIDE_DRIVE | anchor | rect | X 3450..4050, Y -4190..1900 | -58 | yes | :1181-1182 |
| REAR_LOT | anchor | rect | X 3450..6600, Y 1900..4500 + X 6600..8720, Y 2500..3100 | -360 | yes | :1183-1184 |
| COURT | anchor | rect | X -1506..1506, Y 1720..3820 | -362 | yes | :1132 |
| JAM_FIELD | anchor | rect | X 4050..8400, Y -3660..-100 | -60 | - | HouseFallJam.cpp:14 |
| WEST_FIELD | anchor | rect | X -21000..-2500, Y 300..6500 | ~-161 | yes | HouseWestField.cpp:23 |
| WOODS | anchor | rect | X -8200..8200, Y 4600..19200 | varies | yes | HouseWoodsDetail.h:9 |
| FRAT_A | anchor (no collision) | rect | X -11600..-8600, Y -3720..100 | H 820-950 | NO | HouseFraternityRow.cpp:162 |
| FRAT_B | anchor (no collision) | rect | X -15550..-12250, Y -3700..100 | H 820-950 | NO | HouseFraternityRow.cpp:162 |
| FRAT_C | anchor (no collision) | rect | X -18850..-15950, Y -3750..100 | H 820-950 | NO | HouseFraternityRow.cpp:162 |
| B0 | campus block | rect | X -17400..-12200, Y -9600..-6600 | -130..2060 (parapet 2174) | yes | HouseExteriorCampus.cpp:121-130 |
| B0_PAD | entrance | rect | X -15320..-14280, Y -6600..-6050 (door face +Y at -6600) | -59..-55 | yes | :58 |
| B1 | campus block | rect | X -10600..-6800, Y -9200..-6600 | -130..2480 (parapet 2594) | yes | HouseExteriorCampus.cpp:121-130 |
| B1_PAD | entrance | rect | X -9220..-8180, Y -6600..-6050 (door face +Y at -6600) | -59..-55 | yes | :58 |
| B2 | campus block | rect | X -5900..-3800, Y -7850..-6150 | -130..1200 (parapet 1314) | yes | HouseExteriorCampus.cpp:121-130 |
| B2_PAD | entrance | rect | X -5370..-4330, Y -6150..-5600 (door face +Y at -6150) | -59..-55 | yes | :58 |
| B3 | campus block | rect | X 7850..11350, Y -8950..-6450 | -130..1720 (parapet 1834) | yes | HouseExteriorCampus.cpp:121-130 |
| B3_PAD | entrance | rect | X 9080..10120, Y -6450..-5900 (door face +Y at -6450) | -59..-55 | yes | :58 |
| B4 | campus block | rect | X 12350..16650, Y -10250..-6650 | -130..2480 (parapet 2594) | yes | HouseExteriorCampus.cpp:121-130 |
| B4_PAD | entrance | rect | X 13980..15020, Y -6650..-6100 (door face +Y at -6650) | -59..-55 | yes | :58 |
| B5 | campus block | rect | X -16900..-12100, Y -14000..-11000 | -130..2060 (parapet 2174) | yes | HouseExteriorCampus.cpp:121-130 |
| B5_PAD | entrance | rect | X -15020..-13980, Y -11000..-10450 (door face +Y at -11000) | -59..-55 | yes | :58 |
| B6 | campus block | rect | X -10500..-6900, Y -14600..-11600 | -130..2900 (parapet 3014) | yes | HouseExteriorCampus.cpp:121-130 |
| B6_PAD | entrance | rect | X -9220..-8180, Y -11600..-11050 (door face +Y at -11600) | -59..-55 | yes | :58 |
| B7 | campus block | rect | X -17000..-12600, Y -19050..-15550 | -130..1640 (parapet 1754) | yes | HouseExteriorCampus.cpp:121-130 |
| B7_PAD | entrance | rect | X -15320..-14280, Y -15550..-15000 (door face +Y at -15550) | -59..-55 | yes | :58 |
| B8 | campus block | rect | X -11050..-7550, Y -20650..-16350 | -130..2480 (parapet 2594) | yes | HouseExteriorCampus.cpp:121-130 |
| B8_PAD | entrance | rect | X -9820..-8780, Y -16350..-15800 (door face +Y at -16350) | -59..-55 | yes | :58 |
| B9 | campus block | rect | X -16700..-11900, Y -24550..-20850 | -130..2900 (parapet 3014) | yes | HouseExteriorCampus.cpp:121-130 |
| B9_PAD | entrance | rect | X -14820..-13780, Y -20850..-20300 (door face +Y at -20850) | -59..-55 | yes | :58 |
| B10 | campus block | rect | X -10000..-5800, Y -25650..-22550 | -130..2060 (parapet 2174) | yes | HouseExteriorCampus.cpp:121-130 |
| B10_PAD | entrance | rect | X -8420..-7380, Y -22550..-22000 (door face +Y at -22550) | -59..-55 | yes | :58 |
| B11 | campus block | rect | X 8500..11500, Y -14500..-11100 | -130..2480 (parapet 2594) | yes | HouseExteriorCampus.cpp:121-130 |
| B11_PAD | entrance | rect | X 9480..10520, Y -11100..-10550 (door face +Y at -11100) | -59..-55 | yes | :58 |
| B12 | campus block | rect | X 12900..17100, Y -16150..-12250 | -130..2900 (parapet 3014) | yes | HouseExteriorCampus.cpp:121-130 |
| B12_PAD | entrance | rect | X 14480..15520, Y -12250..-11700 (door face +Y at -12250) | -59..-55 | yes | :58 |
| B13 | campus block | rect | X 9350..12650, Y -20700..-17500 | -130..2060 (parapet 2174) | yes | HouseExteriorCampus.cpp:121-130 |
| B13_PAD | entrance | rect | X 10480..11520, Y -17500..-16950 (door face +Y at -17500) | -59..-55 | yes | :58 |
| B14 | campus block | rect | X 13400..17600, Y -24100..-20500 | -130..2480 (parapet 2594) | yes | HouseExteriorCampus.cpp:121-130 |
| B14_PAD | entrance | rect | X 14980..16020, Y -20500..-19950 (door face +Y at -20500) | -59..-55 | yes | :58 |
| B15 | campus block | rect | X -1550..5750, Y -24450..-21550 | -130..2060 (parapet 2174) | yes | HouseExteriorCampus.cpp:121-130 |
| B15_PAD | entrance | rect | X 1580..2620, Y -21550..-21000 (door face +Y at -21550) | -59..-55 | yes | :58 |
| WALK_X-11200 | walk | rect | X -11300..-11100, Y -27000..-6100 | -59..-56 | yes | :134 |
| WALK_X-6100 | walk | rect | X -6200..-6000, Y -27000..-6100 | -59..-56 | yes | :134 |
| WALK_X7200 | walk | rect | X 7100..7300, Y -27000..-6100 | -59..-56 | yes | :134 |
| WALK_X12800 | walk | rect | X 12700..12900, Y -27000..-6100 | -59..-56 | yes | :134 |
| WALK_Y-6000 | walk | rect | X -17800..18000, Y -6100..-5900 | -59..-56 | yes | :135 |
| WALK_Y-10300 | walk | rect | X -17800..18000, Y -10400..-10200 | -59..-56 | yes | :135 |
| WALK_Y-16100 | walk | rect | X -17800..18000, Y -16200..-16000 | -59..-56 | yes | :135 |
| WALK_Y-21100 | walk | rect | X -17800..18000, Y -21200..-21000 | -59..-56 | yes | :135 |
| STADIUM_WALL | stadium | ellipse | c (2400,-13300), rx 5600, ry 3800 | -60..990 (cap) | yes | :143-151 |
| STADIUM_SEATS | stadium | ellipse ring | rx 3864..5477, ry 2622..3716 | 180..873 (+60/row) | yes | :148 |
| STADIUM_TRACK | stadium | ellipse ring | rx 3724..3881, ry 2527..2633 | -55.5 | no | HouseCampusFallStadium.cpp:182-186 |
| STADIUM_FIELD | stadium | rect | X -777..5577, Y -14801..-11799 | -56 | no | HouseCampusFallStadium.cpp:179-181 |
| STADIUM_NOGO | stadium | rect | X -3450..8250, Y -17400..-8700 | - | - | HouseExteriorCampus.cpp:203 |
| PRESS_BOX | stadium | rect | X 100..4700, Y -10000..-9250 | 1050..1920 | yes | :179-180 |
| SCOREBOARD | stadium | rect | X 1550..3250, Y -17100..-16980 | 1190..2190 | yes | :189 |
| MAST_35 | stadium | circle | c (7171,-11033) r20 | -60..3840 (+floods) | yes | :202 |
| MAST_90 | stadium | circle | c (2400,-9348) r20 | -60..3840 (+floods) | yes | :202 |
| MAST_145 | stadium | circle | c (-2371,-11033) r20 | -60..3840 (+floods) | yes | :202 |
| GATE4_K8 | stadium | point | (3705,-9608) facing (0.233,0.972) | -60..370 | wall behind | HouseCampusFallStadium.cpp:467 |
| GATE5_K10 | stadium | point | (1961,-9514) facing (-0.078,0.997) | -60..370 | wall behind | :467 |
| BOOTH_E | stadium | rect~ | c (3330,-9030) 150x170 | -60..170 | yes | :488 |
| BOOTH_W | stadium | rect~ | c (1560,-8975) 150x170 | -60..170 | yes | :489 |
| CLOCK_TOWER | landmark | square | X -9395..-8605, Y -11195..-10405 (plinth); shaft +-300 about (-9000,-10800) | -100..6980 | yes (solid part) | HouseClockTowerMesh.h:11; .cpp:517-521 |
| SHELTER | street decor | rect | X -3210..-2490, Y -6350..-5810 | -90..244 | NO | HouseSurroundingsPolish.cpp:170-173 |
| KIOSK | homecoming | circle | c (2400,-6330) r58; plaza r300 | -60..198 | yes | HouseCampusFallQuad.cpp:361 |
| GATE_PATH | homecoming | rect | X 2250..2550, Y -8930..-6600 | -56.4 | no | HouseCampusFallQuad.cpp:202 |
| FLOAT | homecoming | obb | c (4700,-8250) yaw -38, 800x260 | -60..~400 | yes (deck) | HouseCampusFallStadium.cpp:609 |
| TRAILER | homecoming | obb | c (1000,-8150) yaw 15, 650x240 | 3..233 | yes | HouseCampusFallStadium.cpp:611 |
| BONFIRE | homecoming | circle | c (3750,-7700) r158 | -60..~40 | hay only | HouseCampusFallQuad.cpp:408 |
| LEAF_PILES | homecoming | circle | (4300,-7050) r115; (4560,-6930) r80 | -60..-10 | no | HouseCampusFallQuad.cpp:390 |
| PHONE_BOOTH | homecoming | rect~ | c (1950,-6045) 88x88 | -57..169 | yes | HouseCampusFallQuad.cpp:364 |
| NEWS_BOXES | homecoming | points | (2240,-6060) (2310,-6064) (2378,-6060) | -57..43 | yes | :365-367 |
| BIKE_RACK | homecoming | rect | X 1150..1660, Y -6370..-6170 | -59..31 | yes | :368 |
| HEDGES_W | hedge | rect | X -18800..8650, Y -18860..-18740 | see §9 | yes | HouseExterior.cpp:1259-1262 |
| HEDGES_E | hedge | rect | X 10350..18800, Y -18860..-18740 | see §9 | yes | HouseExterior.cpp:1259-1262 |
| HEDGEN_W | hedge | rect | X -18800..8650, Y 18740..18860 | see §9 | yes | HouseExterior.cpp:1259-1262 |
| HEDGEN_E | hedge | rect | X 10350..18800, Y 18740..18860 | see §9 | yes | HouseExterior.cpp:1259-1262 |
| HEDGEW_S | hedge | rect | X -18860..-18740, Y -18800..-5600 | see §9 | yes | HouseExterior.cpp:1259-1262 |
| HEDGEW_N | hedge | rect | X -18860..-18740, Y -4100..18800 | see §9 | yes | HouseExterior.cpp:1259-1262 |
| HEDGEE | hedge | rect | X 18740..18860, Y -18800..18800 | see §9 | yes | HouseExterior.cpp:1259-1262 |
| F0 | far block | rect | X -4456..-1256, Y -42204..-39404 | -130..1745 | NO | HouseExteriorCampus.cpp:206 |
| F1 | far block | rect | X -957..2243, Y -43699..-40899 | -130..2697 | NO | HouseExteriorCampus.cpp:206 |
| F2 | far block | rect | X 19884..23084, Y -38119..-35319 | -130..2035 | NO | HouseExteriorCampus.cpp:206 |
| F3 | far block | rect | X -1414..1786, Y -33610..-30810 | -130..1736 | NO | HouseExteriorCampus.cpp:206 |
| F4 | far block | rect | X -8794..-5594, Y -35986..-33186 | -130..2750 | NO | HouseExteriorCampus.cpp:206 |
| F5 | far block | rect | X -15981..-12781, Y -41068..-38268 | -130..2249 | NO | HouseExteriorCampus.cpp:206 |
| F6 | far block | rect | X 19196..22396, Y -36034..-33234 | -130..1710 | NO | HouseExteriorCampus.cpp:206 |
| F7 | far block | rect | X 16376..19576, Y -34519..-31719 | -130..2599 | NO | HouseExteriorCampus.cpp:206 |
| F8 | far block | rect | X 20754..23954, Y -39908..-37108 | -130..2679 | NO | HouseExteriorCampus.cpp:206 |
| F9 | far block | rect | X -21944..-18744, Y -35486..-32686 | -130..1978 | NO | HouseExteriorCampus.cpp:206 |
| F10 | far block | rect | X -10036..-6836, Y -40296..-37496 | -130..2253 | NO | HouseExteriorCampus.cpp:206 |
| F11 | far block | rect | X 16525..19725, Y -42667..-39867 | -130..2280 | NO | HouseExteriorCampus.cpp:206 |

**Fixed cameras looking at the campus** (`HouseExterior.cpp`, ExViews; all have the `detail_` prefix except `front_lawn_overview` and `high`, which add NoGo ±800)

| View | Pos | Target | FOV | Line |
|---|---|---|---|---|
| front_lawn_overview | (4600,−6050,2400) | (−250,−2800,20) | 85 | 150 |
| high | (7000,−8000,3200) | (0,0,0) | 85 | 158 |
| detail_clocktower_far | (2400,−3750,140) | (−9000,−10800,4366) | 40 | 231 |
| detail_clocktower_mid | (−420,−8100,1500) | (−9000,−10800,3150) | 70 | 232 |
| detail_clocktower_face | (−7800,−10620,3990) | (−8688,−10800,3940) | 40 | 233 |
| detail_clocktower_belfry | (−7700,−10480,3060) | (−9000,−10800,3150) | 46 | 234 |
| detail_clocktower_base | (−8480,−9720,210) | (−9040,−10470,330) | 58 | 235 |
| detail_stadium_field | (6900,−13300,1350) | (−500,−13300,−56) | 70 | 239 |
| detail_stadium_stands | (3300,−11800,1450) | (1800,−10250,500) | 62 | 240 |
| detail_stadium_scoreboard | (2650,−14200,1250) | (2400,−16972,1720) | 42 | 241 |
| detail_stadium_gate | (2900,−5950,380) | (2500,−9500,950) | 76 | 242 |
| detail_campus_quad | (7000,−6400,500) | (3200,−9050,200) | 80 | 243 |
| detail_campus_dorms | (−4400,−3700,1500) | (−8700,−6600,1210) | 66 | 244 |
| detail_campus_kiosk | (3150,−6020,240) | (2400,−6330,180) | 50 | 245 |

## 20. Sources read

`HouseExteriorCampus.cpp/.h`, `HouseExterior.cpp/.h`, `HouseExteriorSiteLayout.h`, `HouseStreetScene.cpp/.h`, `HouseSurroundingsPolish.cpp`, `HouseCampusFall.cpp/.h`, `HouseCampusFallLook.cpp`, `HouseCampusFallQuad.cpp`, `HouseCampusFallStadium.cpp`, `HouseCampusFallParts.h`, `HouseClockTowerMesh.h/.cpp`, `HouseClockTower.cpp`, `HouseFraternityRow.cpp`, `HouseWestField.h/.cpp`, `HouseParkingLot.cpp/.h`, `HouseFallJam.cpp`, `HouseWoodsDetail.h`, `HouseWoodsPass.cpp`, `HouseExteriorNature.cpp`, `HouseClassicCars.cpp`, `HouseCampusReview.cpp`, `HouseFreeRoamReview.cpp`. Also `SourceArt/CampusFall/README.md`, `SourceArt/ClockTower/README.md`, `Deliverables/InvisibleBarriersOff_20261013/*`, `SourceArt/WoodsPass/evidence/*/Saved/WoodsPassReview/original_trees.csv`, and `/Users/UE_5.8/Engine/Source/Runtime/Core/Public/Math/RandomStream.h`.

The replay scripts are in `campus_geometry_src/` next to this file: `gen.py`, `plan.py`, `cars.py`, `rng.py` and `cs.py`.
