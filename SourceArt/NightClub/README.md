# Connected nightclub — Chapter01_House

Current revision 2026-10-09 local time in the Unreal project only.

## Connected interior polish — 2026-10-09 (current)

The club keeps the wrapped downstairs footprint and the restored upstairs size.
The DJ console and both speakers now occupy the south side of former room 102,
facing its open floor. Seating is relocated to the main-room east wall and the
DJ-room west wall, with the existing corner route clear. A continuous 80 cm world
grid and paired cyan/violet ceiling ribbons carry the finishes through the join.

All six neon word boards are replaced by bent glass picture signs: headphones,
a disco ball and a cocktail. The boards have retaining clips, mounting screws,
backing plates and small cords. Private club materials add worn charcoal walls,
scratched flooring, rubbed metal and timber. Lower acoustic wall panels and a DJ
backdrop add depth. The wear is world projected on both floors, including the
original upper finish that has no UV stream. Three wall drink ledges and 17 loose
cups, bottles, coasters and napkins add independently interactive details.

Only 19 existing club actors are changed and 21 actors are added (1828 saved
actors). All 1788 other original actor snapshots remain unchanged. Door actors,
pole actor, structural collision, guarded opening, room bounds, music transport,
audio regions and six-light budget are preserved. No shared house materials or
meshes are edited. No native gameplay code is changed. The native replicated
furniture classes are retained; multiplayer is not rerun for this appearance and
placement pass. The previous level remains in
connection_polish/backup/Chapter01_House_before_polish.umap.

Saved-map acceptance passed 110 floor/rail/real-walk checks, 139 audio checks and
the six-light budget (three active for the listener's floor). All three entrances
passed closed blocking, native E open/close from both sides and actual walking.
The pole passes natural standing focus, real approach, guarded collision,
E slide down, E climb up and restored movement. Its first check caught one drink
ledge overlapping the landing: both north-wall ledges and their five loose items
were moved east, then the complete pole check passed. This initial failure is
kept in connection_polish/evidence/initial-pole-ledge-obstruction/.

Authoring and current evidence are in connection_polish/. The installers are
historical scoped scripts; do not rerun old installers over the completed level.

Seven final native game screenshots were inspected after the floor mapping and
landing corrections. The material compiled cleanly in both fresh game runs.

Final uncapped comparison used one game process at 1280 × 720, Medium, 71% screen
percentage, 5 seconds of camera warmup and 8 seconds per sample. Main views use
two interleaved samples; the moved DJ room has one warmed sample.

| View | Mean FPS |
| --- | ---: |
| Normal comparison room | 94.36 |
| Main downstairs club | 107.97 |
| Upstairs club | 97.34 |
| Moved DJ room | 108.79 |

These figures describe the measured camera views on this Mac at these settings.
Final screenshots are in connection_polish/evidence/views-results/.

## Club wraps into the former chair room — 2026-10-09 (prior layout pass)

This replaces the east-wing enlargement described below. Upstairs is restored
exactly to its pre-east-annex footprint (38.69 m²), with its sectional and other
furniture retained. Downstairs connects into former chair room 102 through two
wide openings and a short corner return. Connected lower floor area is 82.43 m².
The long east annexes are removed on both floors. Both 101 and 102 lower hall
entrances remain usable; the upper entrance and guarded pole stay in place.

The 52 storage chairs, fluorescent fixture mesh and two obsolete ordinary lights
are removed from 102. It now has dark wall/ceiling linings, a cyan-grid dance-floor
finish, two wall sofas, a coffee table, ottoman and side table. Shared original
house mesh assets were not edited. New geometry is confined to the approved
club/102 layout. The saved level has 1807 actors; 1775 actors outside the changed
set retain their complete audited snapshot. Existing door and pole actor states
are unchanged. The prior saved level is backed up in
wrap_refit/backup/Chapter01_House_before_wrap_refit.umap.

The existing six real club lights are retained: three active for the listener's
floor. One lower moving fixture now covers the new lounge; neon strips reuse
emissive materials. The same original filler track plays continuously through
the two lower joins. An additional doorway fade isolates 102's entrance; the
main and upper entrances retain their fades. The union of the three downstairs
regions is exact, so the removed annexes and neighboring rooms stay silent.

Current saved-map checks passed: 11 generated-geometry gates, 110 floor/rail and
real-walking checks, 129 music checks, and 23 lighting checks. Four normal player
walking legs crossed the main/return/lounge joins out and back with grounded
collision. All three doors passed closed collision, native E open/close on both
sides, and real walking in/out. The pole passed standing focus, guarded collision,
E slide down, E climb up and restored movement. Six fresh native game screenshots
were visually inspected, including both pole prompts. A fresh local listen host
and separate owning client passed lobby travel, shared pole reservation, host
slide, client climb, replicated movement, shared music and restored controls.
Settled native playback comparison matched 10 samples over 9.27 seconds with a
maximum 183 ms difference after automatic startup clock correction. Evidence is
in wrap_refit/evidence/; earlier evidence below belongs to earlier layouts.

Fresh uncapped 1280 × 720 Medium measurements used 71% screen percentage, one
game process, 5 seconds of warmup and 8 seconds per sample. Main room views have
two interleaved samples; the lounge has one warmed sample.

| View | Mean FPS |
| --- | ---: |
| Normal comparison room | 91.17 |
| Main downstairs club | 105.90 |
| Upstairs club | 96.95 |
| Former 102 lounge | 112.07 |

These are current measured views on this Mac at these settings, not a guarantee
for every camera, device or multiplayer load. The authoring sources and scoped
installer are in wrap_refit/. Previous installers should not be rerun over this
layout. The first runtime attempt caught two disabled but obsolete fluorescent
light actors; they were removed, then the complete runtime check passed. That
first failure is retained in evidence/initial-disabled-light-count/.

## Historical east-wing enlargement — superseded

Both selected club rooms now include a connected east wing. The original main
rectangle stays X[-2212.51019,-1490], Y[-1509.02675,-973.5]; the addition is
X[-1490,-860], Y[-1509.02675,-1030] on each floor. Gross floor footprint grows
from 38.69 to 68.87 m² per room, a 78.0% increase. The north setback bypasses the
protected hallway corner. This is a club-only annex in previously empty level
space, so it enlarges the club exterior footprint; existing non-club exterior
surfaces remain in place. This supersedes the earlier rectangle-only limit in
“Final refinement” below.

Fresh saved-map exports included 77 full static meshes and six native door
component sets. Exact polygon checks found no protected structural intersection
with the complete wing envelope. Analytic continuous door-swing bounds also
clear it, with the nearest porch leaf remaining about 109 cm away. Independent
checks on seven generated meshes passed 42 gates, including floor coverage,
open full-height joins, solid notch walls, and the preserved 100 cm pole aperture.
The seven geometry assets have 3,012 triangles total and reuse current materials.
No shared original house meshes were modified.

The scoped installer is expansion2_layout/install_expansion2.py. It replaces
seven club structure/finish meshes, moves the DJ console and speakers to the new
far wall, moves east-wall neon to the new wall, and adds two emissive north-wing
strips. The current upstairs sectional, entrances, pole, and music transport stay
in place. It changes 17 club actors and adds two; all 1,837 other actor states
match the pre-install snapshot. The saved map has 1,856 actors. Backup:
expansion2_context/Chapter01_House_before_expansion2.umap. The Unity workspace was
not edited.

HouseNightClub supports the exact union of main and wing regions. Music and
room effects remain disabled in the excluded northeast notch and other rooms;
there is no fade or song restart across the internal join. Each floor still uses
three shadowless real lights, with one moving fixture covering the main room,
one moving fixture in the wing, and one short, soft strobe. No real lights were
added. The circular floor opening, guard, gate and pole coordinates are preserved.

Actual saved Chapter01_House runtime checks passed 110 floor/rail/structural
checks, 74 music checks, and 21 lighting checks. Four normal player walking legs
crossed the joins out and back on both floors, with collision enabled and grounded
movement throughout. Both doors passed E open/close from each side and real
entry/exit walking. The pole passed natural standing focus, guard collision, E
slide down, E climb up, and restored movement. Six fresh native camera views were
visually inspected, including both pole prompts. A fresh listen host and owning
client also passed normal lobby travel, host slide, client climb, shared reservation,
replicated travel, restored controls, shared music transport and the three-light limit. Evidence is in evidence/expansion2/.

Fresh single-process frame pacing: uncapped 1280 × 720, Medium, 71% screen
percentage, 5-second warmup and 8-second samples per view. The normal/main club
views have two interleaved samples; new wing views have one warmed sample each.

| View | Mean FPS |
| --- | ---: |
| Normal comparison room | 101.59 |
| Main downstairs club | 111.29 |
| Main upstairs club | 97.64 |
| Downstairs wing facing DJ | 98.06 |
| Upstairs wing facing east wall | 104.62 |

Individual main-club samples ranged from 93.48 to 115.73 FPS. These are measured
views on this Mac at the stated settings, not a guarantee for all cameras or devices.

## Nightclub lighting refinement

Each room has three shadowless real spotlights: two moving cyan/violet/magenta
beams and one soft strobe. Moving lights peak at 430 cd, use 8° inner/16° outer
cones, and pulse with the shared music beat. The strobe produces one 65 ms flash
every two beats, capped at 65 cd; at the filler track's 120 BPM this is one flash
per second. This supersedes the earlier 45 ms lighting configuration described
below.

Emissive optical lenses and lightweight additive cone meshes make the sources
and shafts visible. Cosmetic fixtures, mounts, and cones have no collision,
navigation effect, or shadows. The existing six-light budget is retained, with
three real lights and two cone meshes active in the room containing the local
camera. Global fog is unchanged.

Room-local post-processing uses exposure bias +0.25, bloom intensity 0.9, and
bloom threshold 0.65. Lighting, cosmetics, and post-processing turn off outside
the exact club bounds. Enlarged room bounds, doors, pole settings, music asset,
transport, volume, and doorway fades are preserved. The saved-map installer
changed only NightClub_Ambience and preserved the other 1,853 actor states.
Fixed-yoke hinge positioning keeps optical heads attached as they sweep.
The additive shaft material uses longitudinal, cubic view-edge, and depth fades
with opacity 0.05 so beams remain faint instead of resembling solid cones.

Final rendered Chapter01_House verification passed: 13 lighting/isolation checks,
46 floor/rail checks, and 17 music checks plus the sampled walking fade.
Moving lights actually rotated and changed color; short strobes were observed
within the 65 cd cap. Cosmetic fixtures, mounts, and beams have no collision,
navigation, shadows, or GI. Evidence: evidence/lighting-after/,
evidence/lighting-validation.json, evidence/lighting-screenshots/, and
lighting_layout/lighting-install.json. Older results below belong to their
earlier scene versions.

Final single-process comparison on this Mac used uncapped 1280 × 720, Medium,
71% screen percentage, with two interleaved warmed 8-second samples per view.

| Room | Pass 1 FPS | Pass 2 FPS | Mean FPS |
| --- | ---: | ---: | ---: |
| Normal comparison room | 99.45 | 99.23 | 99.34 |
| Downstairs club | 112.53 | 113.54 | 113.03 |
| Upstairs club | 104.02 | 105.90 | 104.96 |

The club views had no frame-rate drop versus this normal-room view at these
settings. This does not guarantee every camera, device, or multiplayer load.
The earlier before-lighting run overlapped Blender rendering, so it is retained
for diagnostics rather than used as a fair performance baseline.

## Lounge furniture update

The downstairs DJ console and both speakers now occupy the east wall previously
used by the couch, facing west into the dance floor. The couch moved onto the
former south DJ wall. Upstairs has a 350 cm main sectional with a 90 × 210 cm
chaise, a walnut coffee table and side table, an ottoman, and two loose cushions.
The sofa modules and tables use native interactive furniture with four visible
damage stages; the cushions are independently movable/throwable. Heavy modules
retain appropriately greater mass and durability. Embedded convex collision
reaches the floor, and collapsed versions stay within the furniture footprint.

The scoped installer is lounge_layout/install_lounge.py. It changes six existing
furniture actors and adds five; all 1,843 other actor authoring states, lights,
doors, pole, music and surrounding rooms are unchanged. The original saved map
for this update is lounge_layout/Chapter01_House_before_lounge.umap. Imported
assets live in /Game/SeniorSendOff/House/NightClub/Lounge. Authoring sources and
collision/dimension manifest are in lounge_refit/.

Four native in-game camera views were inspected after saving; see
evidence/lounge-screenshots/. These show the relocated DJ wall, upstairs lounge,
and the E SLIDE DOWN prompt. A fresh native walking/input test passed after the
layout change: closed guard collision, normal focus at both ends, E slide down,
E climb up and restored movement (evidence/pole-lounge.log). Both club doors also
passed native E open/close from both sides, closed collision and actual in/out
walking (evidence/entrance-lounge-verification.json). The older installers describe previous scene stages
and should not be rerun over this furnished layout.

## Final refinement

Both rooms now have **25.5% more floor area**: their east boundary moved 146.8 cm into verified unused space, from X=-1636.8 to X=-1490. The upper extension clears the nearest protected wall by 6.76 cm; going farther would overlap that hallway. Existing neighboring rooms, Boy Wick, stairwell and house exterior remain in place. The pre-refinement map is backed up in refinement_context/Chapter01_House_before_refinement.umap.

The downstairs entrance now matches the white worn six-panel reference, with black 101 lettering, a black keypad and silver lever. Its original hinge, panel collision and NightClubLower interaction ID remain intact. Club props, neon, music volumes and lighting bounds follow the enlarged rooms. install_refinement.py uses absolute baseline placements, so rerunning it does not move props twice.

The pole now accepts normal standing approaches beside its visible shaft, including angled aim and a lower approach from behind it. Focus uses the current character eye and controller rotation; room walls still block interaction. The closed upper gate remains solid. Server-validated entry waypoints align the player outside the gate or route around the solid lower pole before travel.

## Rooms and controls

The upstairs club is HallEnd_East, across from Boy Wick. The lower club is directly beneath it, with a new ground-floor hallway entrance. The corrected request determines this location; the existing Room 101 and Boy Wick's room are preserved.

- Three glowing signs and neon tube accents per room, dark floors/walls, disco balls, wall seating and a downstairs DJ console.
- Three shadowless real spotlights per room; rotating colors and a short, low-power 45 ms strobe follow a 120 BPM beat. Effects deactivate outside the two club volumes.
- The shiny pole passes through a real 2 m diameter circular opening. The upstairs guard and gate block normal walking; floors have collision everywhere outside the opening.
- At the top: **E SLIDE DOWN**. At the bottom: **E CLIMB UP**. The existing interaction prompt/input path is used. Reservation, completion, abort handling and movement are replicated.
- Original 16-second filler music loops in both rooms. It fades through the entrances, is silent in adjacent rooms/hallways, and remains faint through the pole opening. Replace the SoundWave on NightClub_Ambience and update MusicLoopSeconds and MusicBPM when choosing another song.

## Verified results

All runs used the actual saved Chapter01_House and rendered native game, with fresh isolated user directories.

| Check | Result | Evidence |
| --- | --- | --- |
| Build | Succeeded | evidence/build-pole-current-aim.log |
| Natural standing focus at both ends, four angled poses each, facing-away rejection; real walking; E down/up; swept travel; restored controls | PASS | evidence/pole-direct-aim.log |
| Two actual processes joining via normal lobby, host down/remote-owner up, reservation and both landings | PASS both peers | evidence/multiplayer-verification.json; evidence/pole-host.log; evidence/pole-client.log |
| Both club doors, including the new 101 leaf: E open/close on both sides, real walking in/out, closed collision | PASS | evidence/entrance-refinement-verification.json |
| Enlarged floors, circular hole, guard, doorway audio fades, adjacent-room silence, actual mixer phase, light/post-process bounds | PASS: 46 floor/rail checks, 66 audio checks | evidence/runtime-verification.json |
| Earlier native music cursor sync comparison across peers after loading | PASS: 12 matched samples, maximum 0.134 s difference | evidence/music-sync-verification.json |
| In-game visual inspection | PASS: six final views, both larger rooms, white 101 entrance and both natural standing pole prompts; additional full-door framing | evidence/refinement-screenshots/ |

Cold loading with two game processes on this Mac stalled the client for about 8 seconds. That temporarily put playback 4.67 seconds apart. Two automatic reseeks restored native playback sync about 12 seconds after initial playback. The stable comparison spans 11 seconds; this is a local two-player test, not a wide-area network measurement. Startup logs and the correction are retained in the sync evidence.

## Performance before the lounge furniture update

Final comparison on this Mac: 1280 × 720 output, Medium scalability, its normal 71% screen percentage, no VSync/FPS cap or dynamic resolution. Each view warmed for at least 5 seconds and 120 frames, then sampled for 8 seconds, in two interleaved passes. Only one game process was running.

| Room | Pass 1 FPS | Pass 2 FPS | Mean FPS |
| --- | ---: | ---: | ---: |
| Normal Room 101 | 100.38 | 100.36 | 100.37 |
| Downstairs club | 110.76 | 115.63 | 113.20 |
| Upstairs club | 104.62 | 107.54 | 106.08 |

The club showed no frame-rate drop relative to this comparison room under these settings. These are measured room views, not a guarantee for every camera, device or multiplayer load.

## Preservation and source

The original map backup is before/Chapter01_House.umap. The scene audit preserves every original actor except six explicitly scoped actors: the upper east practical light and five shared geometry actors receiving local door/hole variants. Shared original mesh assets, protected rooms, stairwell, Billy Bass and existing checkpoint/door state were not replaced. The later ceiling-lens cover is confined to the club and has no collision.

The refinement audit (evidence/refinement-install.json) preserves all non-club actor transforms, lights and mesh assignments except three named HallEnd geometry actors receiving scoped extension/hole variants. Original shared mesh assets remain untouched. Fresh saved-map runtime checks cover the new east floor area, hole and safety rail.

New assets are in /Game/SeniorSendOff/House/NightClub. Scene installation/build sources and original filler composition are kept here. Native behavior is in HouseNightClub and HouseClubPole; opt-in smoke checks do not run during normal gameplay. Unity versions were not edited.
