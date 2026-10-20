# Row interiors candidate integration

Status: draft source only. Not copied to `Source`, compiled, loaded, rendered, playtested, or saved into a map. The wildlife reservation has ended, but the baseline/idle gate and integration remain the parent agent's responsibility.

## Owned candidate

`HouseCampusInteriorsRow.cpp` defines `HouseCampusInteriorsImpl::RowDefs(TArray<FInteriorDef>&)` and five free-standing world-space definitions, `R1` through `R5`. The only geometry descriptors come from `HouseFraternityRow::Houses()[Index]`; no second exterior footprint table is embedded here. Every definition supplies Build/Look/Anchors/Openings/Doors/Bounds/Inside/FloorZ callbacks, uses the existing kit and fixture functions, and restricts pooled lights to views inside that house.

All five ground floors have a living/parlor room, chapter room, dining room, practical kitchen, two-cubicle shared WC, and a continuous front-to-rear hall. All five side rooms have unlocked doors from the hall. R5 also has an unlocked pair between its front parlor and dining room. No underground space, terrain cut, gameplay system, new asset package, new light component, or story trigger is included.

## Required shared integration, owned by parent

1. Copy this candidate to `Source/SeniorSendoff/HouseCampusInteriorsRow.cpp` only after the idle/baseline gate.
2. Declare `void RowDefs(TArray<FInteriorDef>& Out);` in `HouseCampusInteriorsRooms.h` and append `RowDefs(Defs);` **after** `GenericDefs(Defs)` in the registry factory. This preserves every earlier registry index/free-standing seed.
3. Append `float LightAnchorFloorOffset = 300.f;` to `FInteriorDef`. Row definitions set it to `200.f`. In the pooled-light floor classification, replace the literal anchor subtraction of `300.f` with the current definition's `LightAnchorFloorOffset`. Existing definitions retain their existing 300 cm behavior. This field is an explicit candidate API dependency; the current shipping header does not yet contain it.
   Also append `bool bPlayerProximityOnly = false;`. Row definitions set it true. The parent's streaming change must then use local-camera and player-controlled/PlayerState pawn interest for these definitions, so the roaming pig cannot hold R4/R5 loaded. Other definitions retain the existing pawn-interest policy, distances and unload timers.
4. Integrate the parent's matching Row exterior header/implementation. This file calls `Walls(Mesh, House, Material, 1.5f, 30.f)`, including the `Inset` argument. The latest helper cuts matching 200/140 cm exterior door holes and real ground-window holes, and provides the SW curved Queen Anne lining. The draft does not add another curved wall.
5. Keep the shared six-light interior pool unchanged. `Anchors` supplies candidates, never components. Keep the exterior six-light arrangement under the exterior owner's control.
6. Extend explicit smoke/review selections to `R1,R2,R3,R4,R5`. Existing `All` lists do not currently include them. Do not change old building expectations to accommodate this addition.
7. `row_walk.inc` is a candidate `RowRoutes` extension to insert inside `HouseCampusInteriorsReviewImpl` before its `Routes()` function, with `HouseFraternityRow.h` included at the translation unit's normal top level. Append `RowRoutes(All)` after the generic routes. Its Name/Shot pointers are static literals. The parent must supply the Row-only 12 cm stop radius, correct floor datum, and idempotent normal Open handling for single doors/pairs. R3's propped room doors and R5's propped front pair must be verified open without toggling closed.

## Precise geometry contracts

- SourceArt's hall rectangle is interpreted as **220 cm finished clear width**, including the shared partition helper's 3 cm wainscot cap projection. Wall centres therefore sit at `leftHall - 10.5` and `rightHall + 10.5`, rather than at the plan rectangle edges. Ground room door centres move with those walls. This is a small construction correction to the schematic plan, not a house-footprint change.
- Root owns permanent exterior floors, 30 cm exterior wall thickness, glazing, porches, access steps, roof and grade. Interior lining is noncolliding and follows the root wall helper. Interior finishes rise 0.35 cm above `House.Floor`; that actual walkable top is returned by `FloorZ(0)`.
- There is an owned continuous ground finish under the interior, so floor-support smoke traces can hit the Interiors actor rather than relying on the permanent exterior floor actor. Kitchen and WC surfaces overlay that floor; nothing is excavated.
- Inner doors are 140 cm wide and 213 cm high. Outer pairs occupy real 200 cm front/140 cm rear holes; each leaf height is **228 cm**, matching the root's lintel. Stalls have 84 cm openings, leaves raised 12 cm and 193 cm tall. Each house has exactly one locked upper door with a readable reason.
- R5 uses two compact 125 cm WC cells (110 cm clear between partitions), shifted 20 cm west compared with the other houses' 155 cm cells. This leaves approximately 101 cm between the stall side and the bathroom leaf's open west tip; the route physically goes around that leaf into the front aisle. Larger houses retain their 155 cm cells.
- The hall's coin phone and notice board are mounted on room faces. No solid ground furniture is placed in the hall. Curtains and framed artwork are decoration; native review still needs to assess image-frame projection and door casing contact.
- R5's reading alcove remains within the southwest local 30..270 cm area. Its chair is at local `(145,150)`. Interior floor/ceiling finishes and the indoor predicate follow the SW quarter-circle, rather than treating the cut-off corner as indoor space. The parent's square structural corner remains an exterior porch finish.

## Stairs and the limited upper landing

Two floors are declared because the player can walk up to the upper landing. The upper storey beyond its locked door is closed. It is not a second furnished floor.

| Houses | Floor-to-floor | Risers | Rise | Clear flight | Intermediate landing | Flight run |
|---|---:|---:|---:|---:|---:|---:|
| R1–R3 | 390 cm | 24 | 16.25 cm | 110 cm | 110 cm | 11 × 28 cm |
| R4–R5 | 330 cm | 20 | 16.5 cm | 110 cm | 110 cm | 9 × 28 cm |

The lower flight starts on the north and rises south to the intermediate landing at local Y80..190. The second flight returns north. The final riser reaches a separate **110 cm top landing**: local Y498..608 in old houses, Y442..552 in new houses. This top landing extends past the schematic stair rectangle at upper-floor height only, fully inside the house body. It does not add a ground obstacle near a room door. The north edge contains the visible locked upper door, surrounded by solid drawn walls and a 240 cm ceiling. All guard geometry is visible and colliding.

Ground ceiling holes cover the full flights and return landing. Timber beams are clipped around the stairwell and upper-landing reservations. Ceiling/floor patches do not overlap the separate top landing. Stair wedges are solid to the floor, avoiding a floating intermediate landing or an inviting low under-stair void. `FloorZ(K)` remains monotonic for all K, so the shared floor-classification scan cannot label the upper floor as floor eight.

The source-derived ground ceiling clearance is 312 cm in new houses and 372 cm in old houses. Native capsule sweeps remain necessary for the stair turn, rail contact, upper door approach, and ceiling-hole edges.

## Household identities and light behavior

| ID | Interior identity | Light candidates | Doors |
|---|---|---:|---:|
| R1 | Quiet closed curtains, dark carpet, switched-off CRT and ground fixtures, orderly chapter/dining | **1 upper, 0 ground** | 12 |
| R2 | Formal green lounge, tidy chapter table, orderly dining and notices | 7 ground + 1 upper | 12 |
| R3 | Wrecked party lounge, pool table when its real bounds fit, cassette bar, long dining and isolated spills | 7 ground + 1 upper | 12 |
| R4 | Slate vestibule, oak stair and beams, interior-wall hearth, oak chapter/dining, small scullery | 7 ground + 1 upper | 12 |
| R5 | Carved/dark wood parlor, curved reading nook, connected dining, interrupted food, mismatched furniture | 7 ground + 1 upper | 14 |

R1 ground procedural glow sections are assigned nonemissive dead lens/screen materials. Its single upper landing pendant is added after that remap. No ground lamp kit is placed in R1. Closed ground curtains follow the current root's original-house window spacing. The exterior owner controls the single emissive upstairs window; the interior does not add exterior windows.

Each house has its own original chapter sign, two original silhouette composite paintings labelled 1979/1986, hand-made paddle forms, notice board, trophies, coin phone, and one wall clock registered through the shared clock sink. Original fictional names are spelled out on signs; no real chapter name, photograph, crest or logo is imported. Period fixtures use CRTs, rotary desk phones, typewriters, enamel kitchen appliances, paper notices and cassette equipment. Optional source sound spots are not added; ordinary shared campus sound behavior remains the existing system's responsibility.

## Kit and prop expectations

`FitKit` checks the actual campus-scale bounding box after its real front-axis rotation against a reserved rectangle. It rejects pieces that do not fit; chairs and sofas have solid procedural fallbacks. This protects door/stair reservations without silently scaling furniture. All major tables, kitchens, bathrooms, fireplaces, partitions and stairs remain present even if a kit piece is missing.

The following are **source call expectations when every requested kit asset fits**, not observed runtime counts. Filled bookcases add extra fixed kit rows and are excluded from the fixed base counts below.

| House | Fixed base kit requests | Loose prop requests |
|---|---:|---:|
| R1 | 5 + book rows | 27 |
| R2 | 6 + book rows | 28 |
| R3, pool fits | 8 + book rows | 21 |
| R3, meeting-table fallback | 6 + book rows | 27 |
| R4 | 6 + book rows | 21 |
| R5 | 7 + book rows | 21 |

Loose props have meaningful support: dining/chapter chairs, cups and papers on tables, supplies beside kitchen counters, a backpack and laundry basket. The R5 reading-table cup stands on the closed book only when that book actually fits. Do not add geometry or duplicate cups to satisfy a threshold. Log actual kit skips, final props, support traces, rendered triangles and material availability during native validation; the above counts can be lower when a real bounding box rejects a request. The existing generic free-standing Tier A contract is 15k triangles, 5 decor pieces, 20 spawned props and 6 anchors. The parent has specified an explicit **new R1 dark-state contract** of exactly one upper anchor and zero ground anchors. Other existing building contracts stay intact.

## Native checks still required

- Compile the integrated project with the appended API field; check all dynamic Row material keys resolve.
- Run `CampusInteriorsSmoke=R1,R2,R3,R4,R5` with the R1 dark-state anchor exception. Inspect real kit/prop counts and floor-support results rather than accepting source estimates.
- Walk front porch -> main pair -> all five rooms -> both WC stalls -> stair turn -> locked upper door -> return to ground -> rear pair -> rear steps in each house. Walk R5's additional parlor pair both ways. Verify propped R3 room doors and R5 front leaves cannot trap the capsule.
- Run the drafted `RowRoutes`: it continues every rear exit along the same clear yard path defined by the yard builder, ending at the original fence approach. The existing fixed field/gate waypoints are not changed. R5's sofa footprint is capped at localX194, preserving an 89 cm aisle to the stair guard; its reading-alcove route centres at X238.5. The current character capsule radius is 32 cm, so this tight aisle requires the specified Row-only 12 cm acceptance radius and native verification.
- Sweep complete door arcs, capsule shoulders at wainscot/casings, and the upper landing's 96 cm locked-door opening. Verify the north room access around the compact new stairs and kitchen supply boxes.
- Capture R1's darkness with ground CRT/troffers/case/pendants off and its single upstairs light; capture R2 tidy, R3 wrecked, R4 Tudor hearth/stair, R5 parlor/curved alcove/connected dining.
- Verify furniture and kit fallbacks from actual asset bounds, no floating props, realistic hearth/chimney relationships, no ceiling beams across an open stair, and no exposed interior finish outside the Queen Anne curve.
- Compare baseline/candidate performance at the same route, rendering and wildlife/nightfall state, preserving the project's complete smoke/walk/render/save validation chain. Check all protected fixed exterior routes/probes and map bounds remain unchanged.

Completed candidate checks are source/API inspection, lexical bracket balance, and arithmetic assertions for hall width/stair rise/run/upper-landing fit. They establish construction intent only. No native acceptance is claimed.
