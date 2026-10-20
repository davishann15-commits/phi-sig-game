# Wall-gap bug: blue strip at the foot of a brick wall

Brief for the master campus job. This was a read-only investigation: nothing in the project was edited, built or run.
Units are world cm with Z up. Front grade is -60, rear and court grade is -362, and the court floor is F = -359.
Paths are relative to `Source/SeniorSendoff/`.

## 1. Verdict

**Where:** at the outside foot of the basketball court's brick side wall, seen from the grass bank beside it. The
view is from the west bank looking south, toward the house. The east bank has the same geometry as a mirror image.

- The camera (fitted from the screenshot) is at about X = -1545, Y = 2200 to 3650, eye Z = -207 (154 cm above the
  bank). It faces -Y and is turned about 6 degrees toward +X. Pitch is about 20 degrees down. FOV is about 90.
- The near wall on the right is the court's west wall. Its outer face is at X = -1508.
- The "light/dark trim strip near its corner" is the narrow face of a brick pier, which stands 8 cm proud of the wall.
  Then come the library-side wall at X = -1516 (Y 1720 to 1976) and the porch-end brick block at X = -1530
  (Y 1480 to 1760).
- The "brick building with dark windows" behind is the left wing's rear facade, the Y = 1480 and Y = 1540 faces. The
  window pairs line up with the facade's listed windows at X about -1789 and -2431.
- The blue strip is the gap between the bank lawn's court-side edge (X = -1530) and the wall face (X = -1508, or
  -1516 at the piers and library wall). It runs along Y about 1770 to 3820.

**Why:** the bank lawn mesh stops 22 cm short of the court wall and has no skirt. Nothing of the court's own fills
that 22 cm strip. It is covered only when the base terrain (ExPatch) is not cut away there.

The terrain cut around the court used to be X = +/-1530. That left exactly this hole down to the wall bottoms at
F - 40 = -399. Through it you see the void, which renders as the dusk sky/fog colour. The source still records the
bug at `HouseExterior.cpp:1127-1128`: "The previous +/-1530 cut left a 22 cm open slit between the piers."

**Status (important):** I could not explain why today's build still shows the hole.

- Today's source uses a +/-1506 cut (`HouseExterior.cpp:1133`).
- The editor module that ran the PIE session (`Binaries/Mac/libUnrealEditor-SeniorSendoff.dylib`, built 16:04)
  contains `FExRect{-1506,1506,1720,3820}`. It does not contain the ±1530 rect. I checked this by searching the
  binary for the 16-byte float pattern.
- An offline replica of `Terrain()` shows soil at Z -362 for every point of X -1530 to -1506, Y 1600 to 4300. The
  only bare ground in that area is inside the court, at X > -1506.
- So with today's code the strip should be soil, 0.8 cm below the lawn, and the screenshot shows void there.

Either the screenshot came from a build before 2026-10-11 18:40, or something hides that soil quad that I could not
find in the source. Section 5 gives a 2-minute probe that settles it.

The fix in section 4 makes the lawn close the gap itself, so it works whichever is true.

## 2. Evidence from the screenshot

I measured the user's image (`images/6.webp`, 2000x1360) pixel by pixel.

| Observation | Measurement | Matches |
|---|---|---|
| Three-blade black triangle clumps in the grass (the "tall dark grass spikes") | 3 triangles per clump, about 2 cm wide, 3 to 13 cm tall | Exists only in the court-bank decor, `HouseBasketballCourt.cpp:155-162`: X = ±(1545..2960), Y 1850..4100, `EX_Leaf` / `EX_CourtDryGrass` |
| Blue strip colour | Uniform sRGB (12,28,48) along about 10 m; the same under the dark wall and the lamp-lit wall | Unlit void (sky/fog), not a lit surface |
| Strip edges converge to one vanishing point (919,230) | Left edge (grass) slope 0.090; right edge (wall foot) slope 0.190 | Lateral/height ratios 0.098 (lawn edge) and 0.207 (wall foot) |
| Solve with eye 154 cm above the lawn | Lawn edge 15 cm right of the camera; wall 37 cm right; visible wall foot 20 to 38 cm below the lawn | Lawn edge X -1530, wall X -1508; wall bottom is F - 40, which is 37.8 cm below the lawn |
| Step at the dark column | Right edge slope drops to 0.149 for about 0.5 m, then returns to the wall line | Pier 8 cm proud (X -1516, 52 cm long), every 467 cm (`HouseBasketballCourt.cpp:120`) |
| Dark column | 38 px wide; brick courses 28 px | 8 cm face at about 2.6 m: the north face of the pier at Y 3352.5 (or 1950) |
| Strip narrows to 0 at the far end | about 3 px at y 360 to 400 | Porch-end block face X = -1530 (`HouseExterior.cpp:923`) sits exactly on the lawn-edge line |
| Small grey box and bright white element at the top near the wall line | (922-995, 22-62) | Court service lamp housing at X -1530..-1503, Y 2500..2535, Z -47..-30, and its emissive `EX_Flood` lens (`HouseExterior.cpp:1216-1221`); also the wall's stone cap edge |
| Frontal brick facade with dark window pairs, lit path at its foot | Windows about 6 degrees and 1 degree west of the wall direction; lit band at y about 255 | Left-wing rear facade (`HouseExterior.cpp:893`, Y = 1480 face) and the rear path (`HouseExteriorWalks.cpp:159`) |

Two things do not fit:

- The bare-ish tree in front of the facade, about 11 degrees west of the wall line, is not in the original-tree list
  (`SourceArt/WoodsPass/evidence/pass2_final/.../original_trees.csv`). The nearest listed tree is at (-4200, 1200),
  which is outside this view.
- The brick-course sizes at the far right edge are noisy.

Neither changes where the bank, wall, piers and lamp are.

Ruled out:

- **House wall next to the West Field mound:** house skins go down to Z -400 (`HouseExterior.cpp:878`, `ZB`). The
  soil cut stops 2 cm under every skin face (`:1131`). The West Field raise (`HouseWestField.cpp:21-31`) is zero for
  X > -3700 and for Y < 1770 by the house.
- **Frat row:** the stone plinth runs from Ground - 45 (`HouseFraternityRow.cpp:59`). It has no grass tufts or
  spikes. It is outside the tuft area X -7900..8100 (`HouseExteriorNature.cpp:1058`).
- **Campus Building():** the body runs from Ground - 70 and the plinth from Ground (`HouseExteriorCampus.cpp:46-47`)
  on flat -60 ground.
- **Stadium wall:** a single-sided panel at exactly Ground (`HouseExteriorCampus.cpp:146`). It has a stone band and
  no tufts.
- **Blue materials** (`EX_ParkingBlue`, tarp, water): none run along a wall base.

## 3. Geometry (current source)

**Court side walls** (`HouseBasketballCourt.cpp:116`, `:120`, `:195-197`):

- A brick bar centred on X = ±1490, 36 cm thick, so it spans X ±1472..±1508. It runs Y 1950..3820, Z F - 40
  (-399) to F + 210, with a stone cap.
- Piers are 52 cm square (X ±1464..±1516), Z -399..-31, at Y 1950, 2417.5, 2885, 3352.5 and 3820.
- An iron fence sits above the wall between piers.

**West-side extras:**

- Library-side wall, X -1516..-1464, Y 1720..1976, Z -399..-31 (`HouseRearPorch.cpp:280-281`).
- Porch-end block, X -1530..-1470, Y 1480..1760, Z -400..380, stone cap to 392 (`HouseExterior.cpp:923`, `:925`).

**Bank lawn** (`HouseBasketballCourt.cpp:145-152`):

- A 24x30 grid with `WX = Sign*(1530 + X*80)`, so it covers X ±1530..±3450 and Y 1770..4290.
- Its height is `Bank()` + 0.8 (`:130-144`), which is -361.2 at the court edge.
- It has no skirt and no edge strip.

**Base terrain** (`HouseExterior.cpp:1119-1153`):

- Cuts at `:1131-1134` include the court, `{-1506,1506,1720,3820}`.
- Grade (`:112-117`) for Y >= 1600 calls `HouseWoods::Height` (`HouseWoodsDetail.cpp:97-99`). That calls
  `HouseWestField::RaisedHeight(X,Y,-362)`, which returns -362 for X >= -2500.
- So the soil strip X -1530..-1506 is one 161.5 x 192 cm ExPatch quad column in piece (-2152,-1506, 2120..3850),
  plus the pieces for Y 1600..2120.

**Grass tufts:** `HouseExteriorNature.cpp:79` excludes |X| < 1538 by the court, so tufts stop 8 cm before the lawn
edge. That matches the image.

**Mirror image (east):**

- Lawn edge X +1530, wall face +1508, piers +1516, along Y 1950..3820.
- The strip ends at the kitchen-gate concrete pad: X 1430..1640, Y 1768..1924 (`HouseRearPorch.cpp:275`).

## 4. Fix (runtime-only, no lights, no map save, does not change tree placement)

**A. Close the gap from the lawn itself** (`HouseBasketballCourt.cpp:150`). This is the main fix and does not depend
on any terrain cut. Start the first lawn column at the wall instead of 22 cm short:

```cpp
auto P=[&](int X,int Y){float WX=Sign*(X==0?1506.f:1530.f+X*80.f),WY=1770+Y*84.f;return FVector(WX,WY,Bank(WX,WY,East)+.8f);};
```

- `Bank()` clamps T to 0 for |X| < 1530, so the new 104 cm first column stays flat at -361.2 and meets the soil.
- It runs 2 cm under the wall face (±1508) and 10 cm into the piers and library wall (±1516), which are opaque bricks
  from F - 40.
- The lawn is in `Grounds`, which has BlockAll collision, so the player can also stand on the strip.
- Decor RNG (`:156-161`) and tuft placement are untouched.

**B. Defence in depth.**

- Keep the base-terrain cut at ±1506 (`HouseExterior.cpp:1133`) and add a 30 cm "skirt" rule. Any ground patch whose
  edge faces a wall must either pass at least 2 cm under the wall face, or drop a skirt below the wall's bottom.
- `ExPatch(..., bSkirt=true)` already exists (`HouseExterior.cpp:1072-1080`) for raised patches.
- **Walls must start below the lowest ground beside them.** The stadium panel starts exactly at Ground
  (`HouseExteriorCampus.cpp:146`); start it at Ground - 40.

**C. Same family of holes inside the court.** Not in the screenshot, but cheap to close in the same pass:

- **2 cm void slits** run along both side walls inside the court. The concrete apron ends at |X| = 1470
  (`HouseRearPorch.cpp:272-273`) but the wall's inner face is at 1472 (Y 1901..3820 between piers). Fix by widening
  to `-1472` and `1472`.
- **36 x 8 cm hole** at the kitchen gate: X 1470..1506, Y 1760..1768, between the porch block's north face (Y 1760)
  and the concrete pad (Y 1768, `HouseRearPorch.cpp:275`). Fix by starting the pad at Y 1760.

## 5. Verification

1. **Settle the "today's build" question first** (headless, `-RenderOffScreen`).
   - Add a temporary ExView, e.g. `detail_court_west_slit`, at `(-1545,3600,-207)` looking at `(-1500,1600,-330)`,
     FOV 90.
   - Add `detail_court_east_slit` at `(1545,2000,-207)` looking at `(1500,3800,-330)`.
   - Then probe with downward line traces at X = ±1518, Y = 2000..3800 step 200 (from Z -200 to -500,
     `ECC_Visibility`). Expect a hit on `ExteriorGrounds` at about -362.
   - If the traces hit at -362 and the capture shows soil, the screenshot predates the fix. If they miss, something
     is removing the soil.
2. **After fix A:** the same traces should hit at -361.2 (the lawn), and the captures should show grass right up to
   the wall at the piers.
3. **Regression:**
   - Re-run `-ClockTowerReview`, `-CampusWoodsReview` (it checks `courtFloorSolid`), `-WestFieldReview`, the basketball
     reviews and `-WoodsPassReview`.
   - Confirm `WOODS_PASS_ORIGINAL_TREES ... crc=0f3b2bb7` is unchanged. No tree, NoGo, view or RNG input changes.
   - Confirm the light count stays at 53.

## 6. Notes for the master job

- **Rule for every new or re-planned ground surface beside a building.** Ground meshes that are separate from the
  base terrain (bank lawns, plazas, lawns around enterable buildings) must reach under the wall face. Do not stop at a
  "nice" offset. Base-terrain cuts must stay at least 2 cm inside wall faces (the house already follows this:
  `HouseExterior.cpp:1123-1133`). This applies to any new building footprints cut out of the terrain for interiors.
- **Frat row has no collision** (`HouseExterior.cpp:1574`, `Local(B.FraternityRow,...,false,false)`). With the
  boundary off, players can walk into the boxes and see through back-faces. This needs to be fixed if those houses
  become enterable.
- **Campus building inside a parking lot.** Campus building {-4850,-7000} (X -5900..-3800, Y -7850..-6150) sits inside
  the asphalt slab X -9000..0, Y -8600..-5800 (`HouseExterior.cpp:1187`). This is a layout clash to resolve in the
  re-plan.
- **Free-roam snag at the campus-side kerb.**
  - The editor logs show the player stuck against the south kerb of Fraternity Park Dr three times:
    (-222,-5503), (8637,-5510) and (-3932,-5508).
  - The logged contact normal is about (0, 0.6-0.8, 0.55-0.8), on `ExteriorGrounds`.
  - The kerb is the slab at Y -5550..-5530, top -57 (`HouseExterior.cpp:1178`), above the road at -72.
  - Every campus walk will cross this kerb, so it needs a ramp or curb cuts.
