# Fraternity Row handoff

## User-requested pause — 2026-10-19T03:57:25.836634+00:00

Stewart explicitly requested a break from Row and sole focus on the stadium. This supersedes the historical requirement below to complete every Row acceptance gate before transferring the shared project. Row is **paused and unfinished**, not accepted. No further Row jobs or source edits are authorized during the stadium reservation.

R7 Source remains installed and its build succeeded. Current R7 scene controls and 16 dusk +16 dark captures passed. R1 and R2 multiplayer checks passed; R3 failed: its host hit SIGBUS in RenderCore while loading Chapter01_House before the pause. The remaining client was shut down for this pause. Raw result and logs are preserved; do not count R3 as passed. All queue parents are terminated and session32774 closed143. R4/R5/library multiplayer, current performance, smokes/interior routes/yards/legacy lot controls/regressions, the complete 136-image attestation and strict final-acceptance checkpoint remain pending. Prior R5 performance had two views above the 5% limit; R7 has no performance result yet. The independent review of the four roof-repair images passed with the documented coverage limits.

A verified **pause snapshot**, exact owned patches and a full source/config/project/map manifest are recorded in `Saved/FraternityRowWork/row_pause_for_stadium_manifest.json`. This snapshot records current work for continuation and does not certify acceptance. Full ownership will transfer to “The handoff” only after native processes exit; see `Saved/FraternityRowWork/reservation_release_full_to_stadium.json` for the actual release. Do not infer a release from engine idleness or the older instructions below.

On any later Row resume, compare the pause manifest with the stadium's final source and merge deliberately. Never restore the whole old snapshot over later stadium work. Preserve all existing native evidence and the agreed 57 total exterior lights including six Row lights.

---

## Authoritative live state — 19 October 2026,03:44UTC

Actual checkout `/Users/Stewart/Documents/Unreal Projects/SeniorSendoff`. Runtime **R7**; build Succeeded andfullcompiler/hashreceipt `Saved/FraternityRowWork/build_pass_r7.log/json`. **Active unifiedexec session32774** is sequentialR7scenes→run_candidate_queue.py. Current phaseMPafterall3R7scenesand16+16capturesPASS. Combinedlog `Saved/FraternityRowWork/row_after_20261018_r7_queue.log`. Source frozen.

Queue order: scenes3controls; captures16dusk+16dark FIRST; MP R1..R5plusLIB--walk; performancefull6thenlights6(withoptionalearlyfull-budgetfail); smoke5; physicalinteriors57images; yards15images; lot_controls2+2legacy; fullregressions. It stops on anyfailure. Source/Config hashesmustmatchcurrentR7foreachfinalreceipt. No duplicateengines/builds. Allnativeworkoffscreen/nosound. Nativeverdicts/crash/cleanexit matter,notlauncher0.

### Latest fixes and unaccepted history

R5 physicallypassed all5interiors307/307(earlierR4source), all5smokes, allyards85/85, scenestree/fence/lights, exteriorcontrols. All57R4roomimagesreviewedclear. But R5 fullperformance6 had2viewsabove5%: establishing_corner+7.71%,clocktower_far+8.76%; allbefore/aftermediansimprove3.22..35.37%. Light6completeandmax+4.91%. R5all12nativeexitsclean; budgetFAILcorrectlystoppedqueue. ArchivedR5PERFORMANCE.md/jsoninrevisions. Neverdismissfailuresasnoiseorclaimaccepted.

R6onlychangedHouseExterior.cpp: revieweddecor regroup ofalreadyclipped1600cmtilesinto3200cmgroups forFraternityRowDetail/YardsDecorONLY. NativeR6proofpassed everyattribute scalarbit/triangleoffset: components55→23,materialsections191→114; triangles5374+21759unchanged. Allstructure/collision/shadow/light/ISM/tree/camera groupsunchanged. R6build+all3scenesand16+16capturespassed. Full16duskcontact+full254/259/277/278reviewed; darknotyetreviewed. R6 rawMP **R1andR2PASS** bothroles/RPC/floors/unload/reentry; fullMPwrapperincomplete. R6performanceNOTRUN.

Rootfounda genuineopenroof/eaveatticseaminfull278_R5rearDusk,alsoexistinginR5beforebatching. Earlierzero-exterior-blockerclaimWITHDRAWN; EXTERIOR_VISUAL_REVIEW.mdandarchivedR5reportmarked superseded. R6queuewaspaused, childrenallowedtofinish, allnativeenginesverifiedgone, onlyownedpausedparents85034/85045/85932terminated. Session15196closed143. No pausedjobsremain. Stopreceiptrow_after_20261018_r6_queue_stopped.json. Do notresumethoseoldPIDs.

R7roofrepaironlyHouseFraternityRow.cpp/NewHouse(R4/R5):8cmSolid/Pale mainsoffitsatTop−8..Topoverexistingroofbase; Tudorwest5cm/front15cmcross-footprintstrips(nooverlap), mainendgablesnowactualoutereaveedges/fullPeak, crosswestgablealignedtoL−55, neweastendtriangleatX+70closesexposedfront15cmsliver(rises16.14cm). Existingroofplanes/silhouetteunchanged; R1–R3/routestreeslightsgradestayexact. Clearanceunderside/upperlanding321.65cmR4,401.65cmR5. Sourceadversarialauditrow_verifyPASS(withrootcatchingfinaleastsliver), expected+49preclipSolidtris. `revision_r7_integration.json`,backup`revision_r6_source/HouseFraternityRow.cpp`. Runtime/Draftmatch. Freshsameposepixelsmustcloseobservedroofblockerbeforeacceptance.

### Next actions

1. R7scenes/capturesnowPASS. Rootinspectedall16+16contacts,full278rear/259Tudoratbothdusk/darkand277frontdusk: oldroofgapvisiblyclosed, noglasssortingblocker. row_verifyindependent4frameconfirmationpending. Monitor session32774MP→perf. Ifactualdefectpersists,stopparentqueue safelybeforelongtests, letcurrentnativechildrenexit, fixreviewedDraftthenSourcewithguards/freshtag. NevereditSourcewhileenginesrun.
2. MPall6targetsandfullperformancefollow. Optional `run_row_perf.py TAG --stop-after-full-failure` verifiesall6fullnativeCSV/screenshots/cleanexits, thencomparessameunroundedmediansasreport. Ifexceeds5%, savesfull_budget_preflight(all11rows+12CSVhashes),finishedUTC,native_complete=false/budget_passed=false,exit2BEFORElight6; noacceptedreport. Ifpasses,full12andnormalreportrequired. Rootreadnewhelper; row_verify4mockedbranchtestsandR5mathequalitypassed. Defaultbehaviorunchanged.
3. Ifperfpasses, remainingqueuecollectsallroom/yards/legacy/regressionevidence. Followup `/root/row_references` whenR7roomframesready toinspectall57andupdateINTERIOR_VISUAL_REVIEW.md(currentstillR4history). Rootmustinspectallcurrentexterior/yard/legacyframes; before28actualpreintegrationframesalreadyreviewedandunchanged.
4. Exactly136imagesmanuallyattest onlyafteractualinspection; schema/helperandstrictcheckpointinstructionsinthebackgroundbelow. BothreviewdocsandcurrentwholeSource/Confighashesrequired. Noautomaticpassfromfilecounts.
5. save_checkpoint.pyTAG performsownedpatches/isolatedexactreconstruction/atomiccopiedsnapshot/final_manifest.json; refresh_status.pyTAGonlycompleteafterstrictmanifestvalidation. FinalownedSource/Draft/project/map/unownedbaselineguards, Contentmtimeguard(noinitialwholeContentinventory), builtdylibhashes. No map save/commit.
6. ExplicitFULLreleasetoThehandoffONLYafterALLrequirednative/manual/recoverypasses. CurrentreservationstillROW; stadiumnoSource/enginepermission. CurrentSTATUSR6needsrefreshR7next. No finalreleaseorcompletionclaimed.

### Coordination and agents

Thehandoff thread01a141dc-4e91-7aa2-a008-3028aac3a31d,hostlocal,userexplicitlyauthorizeddirectmessages. Latestcoord03:19reportedoverbudgetR5andDraftbatchingcandidate/continuedreservation. UpdateaftermeaningfulR7results. Userchose57exteriorlightsretains6Row; baseline53, upgraded57, NoRowLights51, NoRowUpgrade53; interiorpool6.

row_verifyidleafterroof/blockerandhelperaudits; noenginejobs. row_yardsidleafterdecorcandidate/docs. row_referencesidleawaitingfresh57roomimages. Existingagentsauthorizedbybrief; nonewunrelatedagentsneeded.

## Historical R5 execution and stable background

Actual checkout: `/Users/Stewart/Documents/Unreal Projects/SeniorSendoff`. The similarly named Projects asset workspace is separate. Runtime is **R5**, built successfully. Source is frozen during the current native queue. No map save, commit, visible editor takeover, Fab import or unrelated runtime change is authorized.

### Active process and remaining queue

Unified exec session **53487** runs `python3 SourceArt/FraternityRow/run_final_queue.py row_after_20261018_r5`. Log: `Saved/FraternityRowWork/row_after_20261018_r5_queue.log`. It serializes smoke, yards, captures, lot_controls, performance, interiors, regressions and multiplayer, stopping on any failure. All native jobs use the existing launchers, offscreen and no sound. Do not launch another engine or edit Source until it exits.

- R5 build succeeded: `build_pass_r5.log/json` in `Saved/FraternityRowWork`. The full compiler/link log is saved.
- R5 scenes passed all three controls: legacy53, upgrade57, six-lights-off51. All1600 original rendered tree transforms match with zero deltas; canopy-envelope tested1603 with zero overlaps/bad reads. Native house/shed floors, divider/gate gaps, fixed routes/probes and original fence pass. Receipt: `row_after_20261018_r5_scenes.json`.
- R5 smoke passed all five houses with no kit rejects or floating props. Receipt: `row_after_20261018_r5_smoke.json`.
- R5 yards passed5/5 continuous routes,85/85 goals,575 support checks,15 images. Captures and matching lot controls pass:16+16 upgraded and2+2 legacy extras. Root visually inspected all79 exterior/yard/before/control images; EXTERIOR_VISUAL_REVIEW.md is complete with no observed blockers and explicit limits.
- Current phase: **six-light performance pairs** inside session53487. Full6 finished with only detail_establishing_corner +7.71% and detail_clocktower_far +8.76% above the5% on/off budget. Other9 pass; before/after medians all improve3.22..35.37%. Preserve original data; do not call it noise or accepted. Current wrapper completes lights6 then reports budget failure, so the queue is expected to stop before final interiors/regressions/MP.
- A reviewed performance candidate exists ONLY in Draft/Integrated/HouseExterior.cpp:73newlines inside Local regroup alreadyclipped1600cm FraternityRowDetail/YardsDecor into3200cm cells, append matching sections, audit every scalar bit/offset index/count. No other groups, Source, collision/shadow/light/ISM/tree/camera change. Candidate SHA26cb9ba60b149f3acfbc1f63a4180ee0b93251d3628c99a86ad664a2529a8adc. row_yards authored; row_verify independent source/API/lifetime/count audit PASS; root read exactdiff. Native compile/perf/window sorting untested. Translucent EX_RowPane sorting may change with larger bounds, so require fresh captures/interior visuals after integration.
- Source staysR5 frozen until ALL current native processes exit. Then guard against R5 receipt, back up current HouseExterior.cpp, record revisionR6 integration, copy reviewedDraft, build and use fresh tag row_after_20261018_r6. Never apply during lights controls. Performance should be tested before expensive later checks; do not assume batching fixes clocktower, whose image does not directly show Row.
- Investigation also found R5 interior stays loaded after street_along: parked benchmark pawnXY0 is5684cm from its footprint, inside globalFreeRange6000. This is actual protocol cost, not a proven defect or variability explanation; no streaming-range edit is prepared/authorized by root yet.

Use actual native verdicts, not launcher exit0. The launcher can exit0 after a native failure. Wrappers reject missing verdicts, FAIL/_FAILED, crash signatures and timeouts.

### User scope and coordination

Original brief: `/Users/Stewart/.codex/attachments/dc73f04a-ffbb-4d9b-a84d-3b632ec1e20b/Pasted text.txt`. User later explicitly permits removing the west main-house parking lot, forbids map enlargement, and chooses **57 combined exterior lights, retaining all six Row lights**. Main house/rooms, court, Fall Jam, east driveway, road, terrain grade, woods fence/gate, original trees, story/checkpoints/movement, Config/Content/maps stay protected. Interior pool remains six. No navmesh, Landscape, PCG, Water, Niagara, Mass or invisible blockers.

User authorized direct coordination with chat **The handoff**, thread `01a141dc-4e91-7aa2-a008-3028aac3a31d`, host local. It owns future Stadium Homecoming86 work and is waiting for explicit full release after ALL Row acceptance/recovery. Row retains the full shared Source/Config/Content and engine reservation. The latest coordination message at03:19 explains the two over-budget views, Draft-only batching candidate and remaining final-source checks; release takes longer than the earlier minimum estimate. No release time is promised.

The stadium camera-only slot was explicitly returned at01:17:19.755889UTC in `Saved/StadiumDetailWork/reservation_release_camera.json`. Accepted HouseExterior camera overlay SHA is `e077b0646e9efc0164f0d82069c8552e6bfa98887f5014405eb205cd04cabd5e`. Provenance is `camera_overlay_receipt.json`. All11 stadium poses and14 original Row poses stay fixed. Two new lot views were appended after them. Stadium must not integrate geometry or build until explicit full release.

### Current geometry and last corrections

Five hollow, solid walk-in houses: three retained footprints plus a Tudor in existing grass and Queen Anne in the authorized former parking area. All five have distinct room/yard states, rear exits, outbuildings and routes to the unchanged woods fence. Plans were shown before building.

R5 descriptor: L-7080,R-5680,F-1700,B-200,Floor30. Rear stair centre-6300; actual footprint X[-6500,-6100],Y[-200,208]. Front porch Y[-1850,-1700], six15cm nominal risers/five28cm treads fromY-1990. R4 remains X[-8400,-6800],Y[450,1850],Floor-50. All original tree meshes/transforms and CRC0f3b2bb7 stay unchanged; hidden A/B26 <=40. Original fence has10152 exact vertices atY4520, gate[-7320,-6780]. Last divider X-6700 leaves gapsY1050..2150 and3000..3920. See current PLAN.md/json and PARKING_REMOVAL.md for routes and dimensions.

R4 revision relocated R5 after native images and a no-shadows diagnostic confirmed real branches inside its old rooms. It also corrected shelves/displays/paddles/composites/mirrors onto opaque walls, retaining doors/routes/props/light anchors. R4 scenes, smoke and physical interiors all passed; interiors307/307, zero problems, doors/unloads5/5, max interior pool6. All57 R4 room images were visually reviewed; no visible blocker remains. Those historical receipts do not substitute for the final R5 source identity.

R5 changes ONLY `HouseFraternityRowYards.cpp`: porch stereo mirrored to `(Anne.R-230,Anne.F-75)` =(-5910,-1775),floor30. Old west position left22..51cm passages for a64cm capsule. Independent row_verify audit confirms a feasible west-return path with53/62.5cm centre clearances and unchanged support/counts/materials/lights/RNG. **West-wrap access is source-audited; this specific turn is not a native-walked route.** Runtime/Draft match; exact before/after hashes and R4 backup are in `revision_r5_integration.json` and `revision_r4_source/`.

### Baseline and required regression comparison

The complete native baseline finished00:55:19UTC. Six unchanged performance runs,14 dusk+14 dark actual before images and full before-check receipts are saved. Baseline snapshot and camera archives are under `Saved/FraternityRowWork`; see `BEFORE_CRITIQUE.md`, `baseline_source_receipt.json`, `baseline_perf_receipt.json`, `before_checks_receipt.json`, `camera_extra_guards.json`. Map SHA remains `5309c8f80470373721148eabdc62bc6d70ed40fe12ad789587881a2ca7335e5c`.

Baseline passes include431/431 old campus stops, terrain hash72564759, FreeRoam20campus/71hill/5north, old parking/woods/field walks, tree interactions,18 interior smokes and house traversal. Three pre-existing failures MUST retain their semantic verdict without weakening thresholds or moving protected waypoints:

1. Exterior walk: front site material at(800,-4025),Z-57, expectedEX_Concrete, actualMID_M_House_Textured_<digits>. Mesh section/face numbers may vary.
2. CombinedHouseSmoke: Mouse look direction failed(step2,pitch0.00).
3. ExistingHouseDoorSmoke: An extracted original leaf asset is missing.

The final regressions wrapper accepts native PASS or that exact sole baseline failure, clearly labeled separately. Any extra failure is a blocker.

### Evidence and final recovery

`run_row_checks.py` has scenes/smoke/interiors/yards/captures/lot_controls/regressions. Yards requires the earlier legacy tree JSON; current one is `Saved/CalderWork/runs/row_after_20261018_r5_legacy/Saved/FraternityRowReview/actual_tree_instances.json`. Captures require16 upgraded dusk+16 dark; lot controls2 matching legacy poses at each. Pins0/1800, all original poses unchanged.

`run_row_perf.py` calls required perf_ab.sh for full upgrade vs NoFraternityRowUpgrade and lights vs NoFraternityRowLights:12 native runs,300samples/11standard views, uncapped Metal offscreen. It requires complete CSVs/screenshots, actualrender_ms, nativecompletion and clean shutdown. PERFORMANCE.md/json compare before/after and paired full upgrade to+5% each view; report six-light cost. Investigate any excess; do not declare noise or a pass without evidence.

`run_row_mp.py` serializes R1..R5 host/client tests and default LIB--walk. Require owning-client RPC from both door sides, both leaves, floor support/replicated prop meshes, natural unload and identical reentry; LIBhillwalk zero corrections.

After current captures arrive, root must inspect all15yards+16afterdusk+16afterdark+2legacydusk+2legacydark. Before28 were inspected again in both complete contact sheets. Root started EXTERIOR_VISUAL_REVIEW.md with after review explicitly pending. Agent `/root/row_references` already reviewed all57 R4 interiors and owns INTERIOR_VISUAL_REVIEW.md; follow up when final R5 room images are ready to inspect all57 current frames. Other agents are idle; do not start another engine.

Final manual receipt `Saved/FraternityRowWork/<tag>_visuals.json` must be authored only after actual inspection of exactly136images. Schema is documented in `checkpoint_validation.py`: authored_manually=true, exact current whole Source/Config hashes, UTC after image mtimes, reviewers, zero open blocking findings, both review-doc SHAs, exact8image groups with filenames/SHAs. `image_groups(tag)` provides expected136 files. R5 yard shots09/12/19; other yard shots are defined in helper. Coverage limits may be honestly noted; unseen details do not become pixel proof.

Then run `save_checkpoint.py <tag>`: all9 native phases+manual evidence pass for current source; declared18owned files only; Draft matches; no unowned baseline/source/config/project/map drift; Contentmtime heuristic clear. It creates ROW_IMPLEMENTATION.patch against captured shared-camera preintegration bytes and separate ROW_CAMERAS.patch, verifies isolated exact reconstruction, atomically copies owned runtime/reports/evidence and publishes final_manifest.json. Never substitute GitHEAD or overwrite an existing recovery snapshot. `refresh_status.py <tag>` declares completion only after strict manifest validation.

Finally refresh this handoff, confirm native processes exited, and send The handoff explicit full release with final_manifest/checkpoint/patch paths and protected hashes. Final user response should show actual new image(s), link STATUS, report material performance results and the three unchanged baseline failures, and state no map save/commit.

### Implementation cautions

Never rerun apply_candidates.py, prepare_integration.py or oldrun.py/finish.py against integrated Source. Independent Drafts and11 shared Draft/Integrated files are synchronized; owned18 listed in integration_receipt.json. Originalpreintegration bytes and allrevisionbackups are retained. Oldhandoff is archived at `revisions/R4_HANDOFF.md`; its active-session text is historical.
