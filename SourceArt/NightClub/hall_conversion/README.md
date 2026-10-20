# Finite upstairs nightclub wing

Chapter01_House in the Unreal project only.

- Removed the ten InfiniteHall actors, including the teleport/drone controller and its three lamps.
- Preserved HallEnd_End as a normal room entrance. Opened the room's east partition and a 5.9 m join into the upper pole room.
- Added continuous floor support, a finite enclosed front/east boundary and ceiling within the existing exterior outline.
- Reused the upstairs sectional with its tables and pillows in the former hall room. Added the adjoining dance floor, picture neon, scuffed finishes and acoustic panels. Detailed the downstairs DJ backdrop.
- Extended upper room music/postprocess volumes and the HallEnd_End doorway fade. Reused exactly three real lights per floor; no new real club lights.
- Doors, pole actor, Boywick, Pigsty, ground-floor club footprint and 1,926 other original actor states were protected.

## Evidence

`evidence/install.json`: scoped actor changes and protections.
`geometry/manifest.json`: geometry aperture, floor and enclosure probes.
`evidence/walk/ClubHallWalkReview/result.txt`: normal E and eight actual movement legs, floor samples and music boundaries.
`evidence/views/HouseVisualReview/review.json`: six rendered views and frame pacing at 1280 x 720, quality 2, uncapped.
`evidence/multiplayer-verification.json`: separate host/client normal-lobby session, pole travel, shared song and new wing regions.
`evidence/finalviews/HouseVisualReview`: final captures following internal trim removal.
`evidence/reopened.json`: saved actor states verified after reopening.

The frame measurements are a local comparison, not a guarantee on other hardware. Existing whole-house VSM warnings are outside this change.

`build.py` authors the scoped geometry. `install.py` applies it to the editor map; the saved pre-change map is in `before/`. `trim_fix.py` applies the final trim cleanup. Development-only native review flags do not change ordinary gameplay.
