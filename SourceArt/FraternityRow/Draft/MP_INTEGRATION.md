# Fraternity Row multiplayer review candidate

This is a SourceArt candidate. This agent has not edited runtime Source/Config/Content, launched an engine job, compiled it or claimed a native pass.

Copy `Draft/HouseCampusMultiplayerReview.cpp` over `Source/SeniorSendoff/HouseCampusMultiplayerReview.cpp` during the parent's serialized integration. No gameplay, StoryCampaign, movement-system, door-system or wildlife implementation changes are part of this file.

## Controls

- Omit `-CampusMPInterior`, or pass `-CampusMPInterior=LIB`, for the existing library sequence. Its original floor placement, door/prop/hash checks, timeouts, thresholds and optional hill walk remain unchanged.
- Pass `-CampusMPInterior=R1`, `R2`, `R3`, `R4` or `R5` to **both** host and client. The existing `Saved/CalderWork/tools/campus_mp.py` passes extra game arguments to both processes, so it needs no change for this selector.
- Use a unique run name. Example, after successful native compilation and while holding the engine slot: `python3 Saved/CalderWork/tools/campus_mp.py row_mp_r1_<unique> -CampusMPInterior=R1 -nosound`.
- Run one serialized pair for each of the five IDs, plus the original default library run. The launcher already uses `-RenderOffScreen -MetalOffscreenOnly`; `-nosound` is an extra muted control.
- `--walk` still runs the original hill route after all selected-interior checks. Existing route coordinates, stall/fall limits and correction-count acceptance are unchanged.
- `-CampusInteriorsAll` is rejected for a Row run because it prevents the required ordinary streaming free. A missing/disabled Row definition fails rather than silently testing LIB.

## Row sequence and proof

1. Use the normal two-player lobby and story travel. Select the interior by its registry ID; use `InteriorFloorZ(Seed,0)` for the actual Row floor. Server-only prop seeds are **not** used for client membership: both peers count props within the selected building's world bounds, following the existing review pattern.
2. Place the remote player 170 cm outside the main pair for R1–R4, or 115 cm outside for R5, retaining the existing review's flying setup. R5 therefore stands on world Y200, its actual porch floor, rather than floating over the lower tread at Y145. If R5 starts propped open, close it in setup. The client explicitly verifies its outside position, floor, a real supporting collision surface and closed pair, focuses the actual main-door group, then presses its own simulated E. The owning-client RPC must open both leaves on the server and then on the client.
3. Keep the original nearby-door reasons/state comparison, always-relevant tower pair, positive exact prop-count/mesh comparison, terrain hash agreement and moved-prop replication test. The snapshot includes the selected ID so mismatched peer selectors fail clearly.
4. Restore the moved prop's original transform, simulation/active state and relevancy before streaming. Place the remote player 170 cm inside the main pair, close it in setup, wait for the client to confirm its inside position/floor and actual collision support, and repeat the real client E/RPC opening. Both leaves must open on both peers.
5. Move the two **players only** to distinct points on the existing front road, `(7000,-4800)` and `(7200,-4800)`. Do not force `Free()`/`BuildNow()`, move wildlife, lower streaming timers or override relevancy. Ordinary streaming must remove the selected interior and its props on both peers within a 45-second observation window; the current free delay itself remains 10 seconds.
6. Return the remote player to the same supported exterior approach (R5:115 cm; other houses:170 cm). Ordinary loading must rebuild the house. Repeat the exact nearby-door/state, tower, prop/mesh and terrain checks, and verify the client is on the same genuinely supported outside porch position. The authored prop count must match its initial count on each peer. The client records explicit outside-opening, inside-opening, free and reentry proof bits before it may log its Row pass.

There is no streaming skip branch. The parent's separately owned integration adds `FInteriorDef::bPlayerProximityOnly=false` and enables it only for the five new Row definitions. This prevents the existing roaming pig from retaining R4/R5 indefinitely, while every earlier definition keeps its all-pawn behavior. `LoadRange=3500`, `FreeRange=6000` and `FreeAfter=10` remain unchanged.

## Expected evidence

Each Row run must have matching `CAMPUS_MP_ROW_CLIENT_CHECK interior=R# pass=1` and `pass=2` records, including positive equal prop/mesh counts. It must also contain:

- `CAMPUS_MP_ROW_CLIENT_FLOOR` for `stage=initial` and `stage=reentry`, both with `supported=1`;
- `CAMPUS_MP_ROW_CLIENT_APPROACH` for `side=outside` and `side=inside`, with `closed=2`, `supported=1`, the actual support Z and the correct Row floor;
- `CAMPUS_MP_ROW_CLIENT_E` for both sides with `owningClientRPC=1`;
- `CAMPUS_MP_ROW_HOST_OPEN` and `CAMPUS_MP_ROW_CLIENT_OPEN` for both sides, each with `leaves=2`;
- `CAMPUS_MP_ROW_HOST_LEAVE` with `bothPlayersAway=1`;
- `CAMPUS_MP_ROW_CLIENT_FREED` and `CAMPUS_MP_ROW_HOST_FREED`, each with `built=0 props=0`;
- `CAMPUS_MP_ROW_HOST_REENTRY` with `rebuilt=1 clientPropsAndDoorsMatch=1`;
- host and client `CAMPUS_MP_ROW_PASS ... outsideAndInsideRPC=1 freeAndReentry=1`, then the existing `CAMPUS_MP_PASS` and zero process exits.

The original launcher still determines its overall result from peer exits, `CAMPUS_MP_PASS`, terrain equality and the unchanged optional walk-correction gate. Preserve the additional Row lines in the evidence; do not infer two-sided or streaming coverage from the generic pass alone. Legacy library wording remains in old shared log lines for compatibility; the additional Row-prefixed records identify the selected house precisely.

## Checks completed and still required

Both Row approach checks trace down using `ECC_Pawn`, ignore the testing pawn, require an upward normal above0.70, support within3 cm of the selected ground-floor datum and a feet-to-support gap within3 cm of the existing3 cm setup clearance. This rejects a floating setup over a lower tread. The inside offset remains170 cm for every house.

The draft was inspected against the live door, prop, streaming and floor APIs. Lexical delimiter/protocol guards passed. Runtime source was not changed by this agent. Native compilation, the default LIB regression and all five Row two-process runs remain pending. Door animation completion, actual client movement replication, kit support, streamed prop destruction/recreation and render/performance cost can only be accepted from those runs.
