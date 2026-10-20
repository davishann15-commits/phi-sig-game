# Calder State Stadium — Homecoming ’86

**Current state: stages 1–6 accepted; final Stage7 gates pending.** Owner-approved utility-room/office character and creepy acoustic snare installed. Actual sound7 native/32stem PCM/all25images and owner listening passed. Current full41view dusk/night capture passed1427floors/1293capsules/42wallseams per exposure; all82images inspected. Current office walk8 passed8stops/4doors/3indoor/4shell/pool6. Original nightSTA3 is active. Final all41view timing, full11reviews, night7image inspection, normalMP, builds, controls and verified recovery remain. Preserve paused R7 Row and57exteriorlights.

The attached MASTER_PROMPT.md defines the requested finish. PLAN.md describes the implementation and fixed contracts; STATUS.md records results without treating source preparation as runtime acceptance.

## Implemented seating and later staged scope

Individual aluminium and worn painted-wood bench planks, supports/bolts and stencils; eight end-bowl aisles with intermediate steps, nosings, rails, top landings and section boards; concrete joints, drainage, rust, water marks and gum. Two additional passages in segments4/16 have25x17.04cm steps, a369cm landing and row-D cross aisles connected by3x15.75cm side steps to aisles2/17. Their feet are inside the facade, keeping the apron walk clear. Both dusk/night physical audits pass1177floor checks and1120capsule sweeps, with0misses/0blocked and minimum30.946cm tread. The existing segment11 portal keeps its mouth, floor, doors and canonical route. Stage6 uses part of the existing concourse/tower landings to reach30cm treads without moving a tour waypoint.

Layered press booths with angled glazing/equipment, roof camera platform, horns, flags/halyards and retained antennas. An incandescent24–17/00:00 scoreboard with secondary statistics, invented sponsors and rear steel/catwalk/ladder. Lattice around the three fixed mast centres; retained fixture banks, ladders and catwalks, with one dead and one subtly flickering fixture.

Worn natural turf, divots and retained field art; down-and-distance equipment, tape/cable, strapped tarp, maintenance cart/chalk liner, band risers with a clear aisle, folding chairs, trombone case and stripped float frame. Retained substantial sideline props receive matching collision. Gate hardware, plaque, glazed trophy case and flyers; wooden lockers/nameplates, play diagram, trainer table, oval whirlpool, wet floor and padded tunnel/torn banner. Bounded mechanical hum, rare halyard clank, drifting paper and quiet distant snare.

Every new component uses the CalderStadium prefix. Repeated details use HISM kits, with conventional LODs and culling; unique static parts merge by material. New light count0, stadium point lights3. No new module, saved map, story hook or commit.

## Switches and scene controls

- `-NoStadiumDetail`: restore the legacy stadium paths/materials/geometry and suppress the new actor. Native exactness remains pending.
- `-StadiumDetailStep=1..7`: development-only staged construction. Current source default is7; it advances only after each stage is built, visually inspected and fully reviewed.
- `-StadiumDetailReview`: additive physical/component audit; original review routes, tolerances and timeouts are unchanged by this job.
- `-NoFraternityRowUpgrade`: separate Row control. The owner approved57 total exterior lights for Row (53−2+6); stadium always retains its original3 and adds0. The original53-light comparison uses both upgrades off.
- The historic terrain-off scene uses `-NoCalderTerrain -NoFraternityRowUpgrade -NoStadiumDetail`; new independent upgrade counts must be distinguished from that original scene.

## Assets

Imported two new texture assets: [Poly Haven concrete_floor_worn_001](https://polyhaven.com/a/concrete_floor_worn_001), by Dimitrios Savva/Rico Cilliers, [CC0](https://polyhaven.com/license). Unmodified2048px colour and DirectX normal maps,3m physical tile. SOURCE.json records original URLs, bytes and SHA256. The unused concrete_floor_worn_02 candidate remains SourceArt-only. No map or existing material asset was saved. One new owned M_Stadium_FieldDetail was created by field_material_batch.py (221pixel instructions/3reads), reusing the already acquired ambientCGGrass004 CC0 colour/DirectXnormal assets read-only. It combines the original painted field with220cm detail and.55normal strength; no field markings or coordinates move.

Other draft surfaces and invented marks/signs are authored in C++ using existing material parents. No purchased asset, Fab sign-in, real brand or reference photograph is used in the game. REFERENCES.md distinguishes actually inspected period sources from undated/supplementary references.

## Native evidence so far

Camera-only builds passed. Three300-sample dusk rounds and a night baseline were captured, followed by three corrected new poses in three300-sample rounds and night. All88 native images were inspected. Existing original/Row cameras are preserved; the corrected portal/concession/turnstile views have no camera overlaps. These are **before** images, not redesigned-stadium acceptance.

Before geometry:53 exterior lights, treeCRC0f3b2bb7, campusFrontVertices1112052, stadiumSeatVertices4110, layoutFloorSamples8493 with0misses/0obstructions, grounds72842 and HOUSE_EXTERIOR instances53615. The underlying map SHA stayed5309c8f80470373721148eabdc62bc6d70ed40fe12ad789587881a2ca7335e5c. Row integration now legitimately changes unrelated scene counts; final stadium on/off performance must use the same released Row state.

| Before view | Median frame ms | Median render ms |
|---|---:|---:|
| detail_stadium_field | 8.94 | 8.94 |
| detail_stadium_stands | 6.73 | 6.73 |
| detail_stadium_scoreboard | 6.63 | 6.63 |
| detail_stadium_gate | 7.45 | 7.44 |
| detail_layout_aerial | 13.14 | 13.13 |
| detail_calder_eye_P2 | 7.14 | 7.14 |
| detail_calder_eye_P3 | 8.75 | 8.75 |
| detail_calder_eye_P3_tower | 8.47 | 8.47 |
| detail_stadium_aisle_up | 7.78 | 7.77 |
| detail_stadium_seat_knee | 8.19 | 8.19 |
| detail_stadium_front_rail | 6.10 | 6.09 |
| detail_stadium_press_50yard | 6.46 | 6.46 |
| detail_stadium_score_endzone | 6.82 | 6.81 |
| detail_stadium_bench_chains | 6.16 | 6.16 |
| detail_stadium_locker | 6.88 | 6.87 |
| detail_stadium_tunnel | 6.50 | 6.50 |
| detail_stadium_portal | 7.59 | 7.51 |
| detail_stadium_concession | 6.22 | 6.22 |
| detail_stadium_turnstiles | 6.06 | 6.05 |

Baseline measurements are recorded in Saved/StadiumDetailWork/before_performance.json and the camera release receipt. They are not a claim about the current Row scene or the stadium candidate.

## Acceptance still required

| Stage | Native build | Dusk/night images and photo comparison | Full fix_checks chain |
|---|---|---|---|
| 1. Seating and aisles | PASS |42current images inspected; both physical audits PASS | PASS all11phases;23smokes/431stops/6094paths/0traps |
| 2. Press box and scoreboard | PASS |42dusk/night frames inspected; physical and21-view performance PASS | PASS all11phases;431stops/6094paths/0traps |
| 3. Masts | PASS |42dusk/night frames inspected;1243floors/1184sweeps/0problems;21-view performance PASS | PASS all11phases;431stops/6094paths/0traps |
| 4. Field and sidelines | PASS |56frames inspected;1301floors/1234sweeps/0problems;28view timing PASS | PASS all11phases/431stops/6094paths/0traps |
| 5. Concourse and gates | PASS |62frames inspected;1304floors/1237sweeps/0problems;31-view timing PASS, worst+0.94ms/load−0.654156s | PASS all11phases/431stops/6094paths/0traps |
| 6. Interiors | PASS a7 | All68 images actually inspected;1386floors/1254sweeps/42wall rays pass;34view timing PASS | All11PASS; nightSTA69actions/65walks/7images PASS |
| 7. Mood, office and final finish | Current source compiled | Eight-view proof inspected; current sound7 under test; final 82-image capture and full reviews pending | Pending |

Final gates: all handoff reviews, exact NoStadiumDetail controls, three alternating300-sample Metal-offscreen timing rounds for all37 current named views (19baseline,2passages,7fieldprops,3concoursewall details,3interior details,1movingprogram,2owner-requested office) (≤+1.5ms), native load≤+1s, DisableUnity build, protected-file/hash audit, recovery export and actual test-apply. No completion or photographic-quality claim is made until these pass.

stage_runner.py, performance.py, snapshot_integration.py, import_textures.py and recovery.py require a recorded explicit full reservation release. They cannot grant their own slot. Evidence tags are fresh and original captures/checkpoints are never overwritten.

Timing runs require `performance.py <fresh_tag> --stage N --capture-tag <inspected_stage_tag>`. Physical audits stay in both current-stage dusk/night captures and must pass at the same frozen Source/Config/map/texture hashes. Frame sampling excludes their diagnostic work, which otherwise overlaps the first field view;21poses,300samples,4warm seconds,3alternating rounds and both budgets remain exact. Stage3 first failed set and the one-pair isolation diagnostic are retained without being relabelled as acceptance.

From stage4, seven appended field-prop close views expand capture/timing to28poses/56dusk-night pictures. They add actual170cm-eye turf, tarp, cart, liner, band, case and float inspection; every prior camera and planting NoGo remains unchanged. All28must pass the same timing budgets. Earlier stages retain their actual21-view records.

Latest stage5 acceptance: a9 includes the corrected west bin and parallel image-row preparation. All11 original review phases passed, all31 frame budgets passed across3alternating300-sample pairs (worst+0.94ms), and median load delta was−0.654156s. Failed earlier load/geometry candidates remain archived. Guarded5→6 advance verified all matching-source receipts.

Current stage6 detail pass adds chord-aligned end walls with overlap, timber wainscot, framed oak lockers with brass labels/hardware, shaped numbered jerseys, slatted timber benches, fuller helmet shells/cages, table stretchers/upholstery seams and whirlpool hardware. Thirty-four authored sign faces plus both complete exterior price boards have higher-resolution lettering, borders and wear; existing gate/ticket/section plates gain physical frames and screw details. All use runtime art and existing parents; no new saved Content assets. The optional physical review now also waits for the actual streamed STA room and checks42 rays through its two end-wall seams. Fresh visual, collision, night-tour, full regression and performance gates remain required. Previous a1/night_a3 receipts describe the earlier art, not final acceptance.
