# Pig pen → library closet, Boywick's bedroom (2026-10-07)

## What changed
- **Pig pen** (Pepper, Coal, mattress, Cheetos pile, water trough, feed bucket, straw, broken boards,
  bulkhead lamp, KEEP SHUT sign, two pigsty lights) now lives inside the basement library's
  mechanical closet (`Library_ServiceAnnexShell_001`, door `BasementLibrary_Mechanical`).
  The wall plaque that used to hang outside the upstairs pen hangs in the library next to that door.
  Map folder: `Basement/Pigsty closet`. The pigs' instance properties and the C++ defaults in
  `HouseRoamingPig.h` (pigsty bounds, bed spots, door id) and `BuildDefaultGraph()` (closet + library +
  corridor roam nodes) were updated together. The old `HousePigSmoke` harnesses still use the
  second-floor coordinates and will need re-pointing if they are run again.
- **Old pigsty room** is now **Boywick's bedroom** (`Second floor/Boywick bedroom`): the shell keeps its
  mesh (`PIG_ExpandedRoom` / `PIG_ExpandedRoomCollision`) with cream walls, white ceiling and oak floor;
  a partition (`BW_Partition_*`) squares the room off to the photo's proportions and leaves a walk-in
  closet strip (the vent network's `PigstyVent` portal still opens into that strip). Furniture is a mix
  of existing house meshes placed as `HouseInteractiveProp`s and engine-primitive builds (loft bed,
  coffee table, desk, window frame, laptop, flip flops, ceiling vent). Everything is prefixed `BW_`.
- **Boywick** (`BoywickCharacter.h/.cpp`): primitive-built NPC spawned at runtime by
  `UBoywickRoomSubsystem` (`BoywickRoom.cpp`) on `Chapter01_House`. Wanders his room, tells six stories
  (E to listen), grants a 90 s blessing (SPEED / JUMP / MIGHT) for listening to a whole story, has a
  health bar, gets furious when hit (chases and shoves), is knocked out at 0 HP and recovers.
  Player hooks live in `BoywickPlayerHooks.cpp`; HUD overlay in `BoywickRoom.cpp` (`BoywickDrawOverlay`).
  The same subsystem spawns the painted FLAVOR banner, the movie poster and the string lights.

## Tooling
- `Tools/claude/claude_pigs_to_closet_and_bedroom.py` — idempotent editor script that performs the map
  edit (removes `BW_*`, moves the pen, re-skins the shell, rebuilds the bedroom, verifies nothing else changed, saves).
- `Content/HouseReview/boywick_review.json` + `-HouseVisualReview` — screenshot sweep of the rooms.
- `-BoywickCheck` — logs pig/Boywick positions every 4 s (`CLAUDE_CHECK`).
- `-NoBoywick` — skips spawning Boywick and the decor.
- One-shot runners in `Saved/`: `ClaudeBuild.command`, `ClaudeMapEdit.command`, `ClaudeReview.command`,
  `ClaudeBuildAndReview.command`, `ClaudeAll.command`. Logs in `Saved/ClaudeDump/`.
- Pre-edit map backup: `Saved/ClaudeDump/Chapter01_House.before_claude.umap`.
