# Brighter exterior

Implemented 2026-10-10 in the Unreal project for Chapter01_House.

The existing moon light increases from the runtime setting of 2 lux to 24 lux, with a softer neutral blue tint. The existing sky fill increases from the saved 0.15 intensity to 0.8. These changes make the open lawn, concrete approaches and side yards readable at night. The same 52 existing exterior point lights remain, and no additional lights or shadow casters were added. Shadow distance and cascades retain the previous values.

The saved map, front-road setback and walk geometry are unchanged. Room lights and exposure settings were not edited. The source builds the exterior lighting at level startup; a new Play session loads the updated values. Unity files were not edited.

## Verification

- Native Development build succeeded.
- Rendered in-game screenshots inspected: front lawn overview, front approach, lawn facing the road, both side yards and great room.
- Runtime logs confirm 24 lux moon, 0.8 sky fill and the unchanged exterior triangle/instance/light counts.
- Both rendered sweeps exited cleanly. The courtyard camera was moved inside the court after the initial view was blocked by the perimeter fence; the follow-up capture shows the court floor, lines, fence, basket and rear facade clearly visible.
- Evidence screenshots and logs are in evidence. Backup source and protected-file hashes are retained beside them.

The screenshot runs use a 30 FPS cap to keep the visual review responsive. Their frame timings are not an uncapped before/after benchmark. Multiplayer was not run for this local static lighting change.
