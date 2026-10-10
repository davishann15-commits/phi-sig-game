# Stairwell wall repair — 2026-10-08

The staircase wall in the supplied two photos now shares a flat front plane at Y=869.2 cm. Both original combined architectural meshes are retained; the saved map points to two repaired copies. The new yellow masonry faces use the existing cream masonry material and a shared UV projection matching its original brick scale. Door openings, open-side guardrails and stair geometry remain intact. Buried wall-side rail geometry and the two runtime wall handrails were removed.

## Recovery and reproduction

- `before/Chapter01_House.umap`: saved map before this repair.
- `exports/`: original Unreal mesh exports.
- `build_repair.py`: reconstructs the two meshes in Blender.
- `install.py`: imports the repaired meshes, assigns original materials, updates the existing actors, adds thin hidden wall collision, and saves Chapter01_House.
- `open_review.py`: opens the saved house at the repaired wall.
- `evidence/`: original geometry census, repair plan, and final game screenshots.

The new faces use the original first UV-layer name so the brick texture is retained when joining the meshes. The replacement assets live under `/Game/SeniorSendOff/House/StairwellRepair`.

## Verification

Both repaired meshes imported and the house map saved. The module build succeeded. Five fixed in-game views confirmed yellow brick, a flush wall, no buried rails, and intact door frames and open-side guards. A focused `-HouseStairSurvey -StairWallWalk` run verifies normal character movement on both stair flights in both directions; its result is recorded separately with the runtime clearance evidence.
