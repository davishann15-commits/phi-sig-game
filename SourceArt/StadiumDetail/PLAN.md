# Calder State Stadium — Homecoming '86

Owner brief: MASTER_PROMPT.md and the attached 2026-10-18 prompt (same body). Editable project is Documents/Unreal Projects/SeniorSendoff; Chapter01_House remains runtime-built. Commit nothing and never save the map.

## Scope and fixed contracts

Keep bowl centre (2400,-13300), ellipse5600x3800, ground-60, turf-56, concourse-55, press floor1065, press ceiling1480/top1830, exterior roof1920; preserve all existing four stadium Z constants, mast centres, apron, roads, bus bay and loop. House/court/Fall Jam and their files are untouched. Preserve original planting transforms/crc0f3b2bb7, wildlife and30-minute nightfall. The existing STA doors, route and segment11 portal stay exact.

New content uses CalderStadium component names, named Impl namespaces and runtime conventional meshes, with no new dependencies or editor API. Only NoStadiumDetail disables this work; also respect NoCampusFall/NoCampusPolish. No paid/imported assets are needed for the initial authored kit. Existing material parents are reused; authored surfaces are code-painted, with two selected unmodified2K CC0 PolyHaven concrete maps staged for a guarded import.

Lighting: zero new lights, exactly three existing stadium points. Untouched baseline53 total. The owner separately approved57 total in the coordinated Row chat on2026-10-18: Row removes2parking lights and adds6. Stadium never changes that logic. Report local3/0 and the shared total separately; a53-light control uses NoFraternityRowUpgrade after Row integration. No threshold changes under stadium authority.

## Survey findings

- HouseExteriorCampus.cpp builds40segments/12tiers at63cm rise, with EX_CampusSeats quads, shallow continuous colored bench strips and a sparse aisle rail. Keep tier tops and bowl footprint; split the aisle ascent into four15.75cm steps per63cm tier. Selected end-bowl segments0/2/17/19/20/22/37/39 achieve minimum30.946cm perpendicular tread depth; front links rise16.857cm and top links16.714cm. Chord-aligned skew prisms fit the fixed ellipse without changing its four original elevations. Verify physical support and head/capsule clearance before acceptance. Do not treat a63cm tier face as a legal stair.
- Segment11 rows4..7 are the existing walk-in portal. Its floor369, mouth, doors, tour waypoints and guard remain. In stage6 the current26cm-tread concourse flight can use70cm of its existing220cm landing:24steps over720cm give30cm treads/17.667cm rises, with the U383 tour stop on the remaining150cm landing. The foot staysU1133; this changes geometry without changing the route. Native traversal must confirm it. Additional portals can occupy dedicated row/segment openings, but require real stair/landing access and no new traps.
- HouseCampusFallStadium.cpp already supplies field art, posts/pylons, benches/coolers, litter, scoreboard, bunting, gates and float. Retain compatible existing content, replace coarse overlapping pieces only in detail mode.
- HouseCampusPolishStadium.cpp already supplies flood banks, catwalks/caged ladders, press roof antennas, facade/apron/ticket/souvenir detail. Add lattice framing and surface age; avoid stacking duplicate fixtures or ladders.
- HouseCampusInteriorsStadium.cpp supplies exact STA shell, props, door definitions and load/unload. Add/re-skin only this definition, preserving timings/doors/stops and shared six-light pool.
- CampusWoodsReview counts EX_CampusSeats material vertices and requires>1000. Preserve the existing tread material key/topology; do not lower the threshold. Legacy bench removal changes campusFrontVertices and must be documented; NoStadiumDetail must restore exact original counts.

## Implementation order and files

1. HouseStadiumDetail.h/.cpp/Kit.cpp/Look.cpp/Seating.cpp: actor, pooled HISM kits with collision and LODs, aged materials, individual bench planks, brackets/bolts, intermediate aisle treads/nosings, rails, section/row/seat stencils, joints/gutters/stains/gum/student wear. Narrow gated legacy bench/aisle hook in HouseExteriorCampus.cpp and old-litter support remap in HouseCampusFallStadium.cpp.
2. HouseStadiumDetailStructures.cpp: press-box cladding/angled panes, upper coaching/radio volume and open camera roof platform, horn cluster/flags/halyards; replacement scoreboard face, bulb layout, invented sponsors, rear truss/catwalk/ladder. Retain lower walkable press box and entrance exactly. Gate only obsolete face geometry.
3. Same structures file: lattice around fixed three mast cores, retain bank/ladder/catwalk positions, one dead/one emission-only flicker fixture.
4. HouseStadiumDetailField.cpp: worn turf detail using existing field art, scuffs/divots/chains/down-box/cable/tarp/cart/liner/band risers/chairs/trombone case/float-frame/confetti, solid larger props clear of all paths.
5. HouseStadiumDetailConcourse.cpp: gate/ticket/queue hardware, overflowing barrels, plaque/hall-of-fame/flyers/souvenir/pennants/price boards. Existing concession/restroom openings remain.
6. HouseStadiumDetailInteriors.cpp called from BuildStadium with the same Solid/Clutter/Glow batches: wood lockers/nameplates, play chalkboard, trainer table/whirlpool/wetfloor, padded tunnel and torn banner. Keep route clear and load/unload ownership in STA.
7. HouseStadiumDetailMood.cpp: bounded source-positioned hum/halyard sounds, low rare unlocated snare, emission-only flicker and a few drifting programs; distance-throttled, no triggers/story/checkpoints. Reuse existing drumline where suitable rather than doubling it.

## Rendering and collision

Small repeated elements share HISM mesh/material batches; kit meshes have inexpensive near/far LODs and cull distances, no small-prop shadows. Static unique surfaces merge per material and locality. Collision uses matching simple kit bodies or merged collision-only geometry for reachable structures; no opaque box across an aisle or opening. No per-bolt actors/components. Target <25 added visible material draws per close stadium view and no distant small-detail draws. Construction cost measured as actual native load, not sum of helper timings.

## Evidence and acceptance

Before integration obtain current native stadium/P2/P3/aerial views and close-ups, source/config/map SHA manifest and exact review counts. Preserve all previous evidence. Append cameras only at end of ExViews, with distinct detail_stadium_* names; existing cameras unchanged. Add aisle-up, knee-height seat row, front rail, press50yard, scoreboard-endzone, bench/chains, portal, concession, turnstiles, locker and tunnel. Eye cameras170cm above actual support; requested knee view is explicitly lower.

After EACH of seven steps: serialized build, native original+close views and visual comparison to archival photographs, complete fix_checks.sh review chain and short STATUS entry. Capture failures honestly and repair geometry; never change thresholds/tolerances/samples/timeouts/waypoints. Final: all handoff reviews, three alternating300-sample offscreen/Metal-only rounds for every detail_stadium_* plusP2/P3/layout aerial; require median delta<=1.5ms per view and native load delta<=1s. Run exact NoStadiumDetail counter/review/crc and protected-file hash audit, final DisableUnity compile and recovery export/test-apply. Record results in README/STATUS and append Current state/CampusFall notes without replacing other chat's records.

## Shared project reservation

19October04:00UTC: Stewart explicitly paused Row and requested sole stadium focus. Its full verified transfer in Saved/FraternityRowWork/reservation_release_full_to_stadium.json supersedes the earlier queue order below. Stadium now owns serialized runtime integration/build/tests against the protected installed R7 Row baseline; Row remains unfinished.

Fraternity Row has the current Source/engine reservation for its unchanged before-test queue. Until explicit release, this job only reads source and writes SourceArt/StadiumDetail and Saved/StadiumDetailWork. The next agreed slot is camera-only baseline capture/performance, then release back to Row before its geometry integration. Runtime implementation remains in Draft during that slot. Use a serialized integration/build window after that; no parallel engine or builds.

## References

REFERENCES.md records archival image links, dates/uncertainty, observed construction and what is transferable. Study only; never copy real school marks, brands or licensed photographs into the game. Photographic quality is a visual gate, not a claim earned by passing a build.

2026-10-18 21:32 EDT isolated refinement: drafted oval hollow whirlpool/rolled rim, flared PA horns, removed duplicated scoreboard cross-braces, and changed stage7 audio candidate to a single four-bar low-passed snare with no bass/tenor/cymbals. First occurrence130s, subsequent interval260s with original randomization; NoStadiumDetail preserves the old cadence. Halyard transient queues once per wind cycle. All are still uncompiled Draft work under Row's exclusive runtime reservation.

Additional stage6 tower correction: each existing140cm landing gives10cm to the flight, leaving130cm landings and240cm runs (8x30cm treads, unchanged17.5cm risers). Shell/doors/landings at all eight heights and all canonical waypoints stay fixed. Stage1 replaces the obsolete180cm-high inner rim guard with the authored105cm outer guard; NoStadiumDetail restores the old components. Band risers use18/16/16cm rises and leave an east130cm aisle.

Prepared portal completion: new real passages atsegments4/16, within the existing apron ring, preserve segment11 and every original route. Each has25risers17.04cm and minimum36.171cm perpendicular tread fromapron-57 tolanding369, clear300cm width; a420cm-local outer-wall opening gives>220cm headroom at the wall and the original row8 underside leaves252cm above the landing. Rows4..7 get a350cm central opening, stepped95cm cheeks and a guard above the rear lip. Row3 is a clear cross aisle through2..4 and16..17; three35cm treads/15.75cm rises connect to each existing ascending aisle, avoiding a47.25cm side rise. Obsolete blind-arch fills/grilles and litter in those two owned passages are gated out only in detail mode; existing outer props reserve their entrances. Two new detail_stadium_portal_east/west views append after all current original/Row/stadium poses at later integration. Stage capture/performance therefore covers21views; the original19-view before evidence remains unchanged and the two additional off views must be captured natively. Six geometry hooks plus a camera-only HouseExterior.cpp delta are backed up/restored. All new passage collision, walking, visual quality and performance remain unverified.

Native portal correction,19October: the draft r1.08foot intersected the immutable apron walk. Revised feet start at r1.0 and rise inside the bowl to r.802, with a supported r.770→.802landing at369. The25rises, perpendicular tread dimensions and300cm width stay unchanged. The actual facade plinth collider/cap are opened. New-camera eyes follow the ninth tread at170cm height; all pre-integration poses remain exact. Original layout8493floors/0misses/0obstructions and additive1177floors/1120sweeps/0misses/0blocked now pass; both current21-view dusk/night sweeps were inspected. Side rails return before the main aisle's skewed lanes; guards are110cm. Full stage1regression/performance remains pending.

## Stage4 refinements and evidence coverage,19October

Seven additional170cm-eye field views are appended after all prior21poses, for turf/cart/tarp/liner/band/case/float. All28current poses must pass both visual exposures and all3alternating300sample/4warm Metal timing pairs at the unchangedbudgets. Own physical audits now include55equipment-bypassfloors/50sweeps in addition to seating/portal/rim/bandchecks; no originalnativecheckchanged. Large field cylinders use a separate32/16sidednear/far kit, preserving16sidedincandescentbulbs and10/5railprofiles. Soft small props reuse the bevelledplankkit at bounded physicaldimensions; conservative matching boxes collide. PaintedfieldkeepsTurf-56/layout and gains one ownshader using existingCC0Grass004A/N at220cm,221instructions/3reads. OnlynewM_Stadium_FieldDetail saved; oldassetsandmapremainexact.

Frame sampling is pure production rendering. The actual identical-source physical+manualvisualcapture receipts are mandatory inputs kept separate from samples, because executingtrace/sweepaudits within a timed firstview contaminated earlierstage3numbers. Thatfailedset, diagnosticpair, olddriverand completepassedfresh3pairset are all retained. This is not permission to omit physicaltests or alter originalbudget/pose/samplecount/timeouts.
