# Fraternity row and property boundaries

Added three exterior fraternity-style houses to the west strip identified by the supplied screenshot and the recorded native camera at X=-6638, Y=-758. The row faces the existing front road. It uses brick, pale siding, tall columns, balconies, pitched roofs, chimneys, shutters, banners, Greek-letter plaques and entrance paths. No real lights were added; the row has no shadow or indirect-light updates.

The four old distant world-edge blockers were replaced by player-only property boundaries at world-centimeter coordinates X=-8500/8800 and Y=-5900/19800. Both road sidewalks, the main property, the court and the detailed woodland are inside. Cameras, interaction rays and thrown props pass through these boundaries.

## Verification

- Editor build succeeded.
- All 24 native player-capsule sweeps across edges and corners were blocked.
- All 20 inward movement sweeps stayed clear of the perimeter.
- All 20 camera rays and 20 visibility rays passed through the perimeter.
- Campus, court and woodland scene/collision checks passed.
- Native screenshots reviewed the requested strip, the house row, main house, woodland trail and Coles Quarters.
- Saved Chapter01_House map hash is unchanged; saved rooms and actors were preserved.

The test used 1280x720, quality 2. It was a camera sweep and collision integration check, not a manual traversal, multiplayer test or FPS benchmark. Native results are under evidence; final screenshots are under final.
