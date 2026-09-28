# Linux build — September 28, 2026

Native Unreal 5.8.2 Linux Development compilation succeeded in 11.38 seconds. The current source includes the house furniture/material refresh, protected lobby game-mode setup, native party-ready diagnostics and opt-in 64-frame render-review warm-up.

The latest staged Linux package completed native compile/cook/package and eight assisted checks: lobby load, house load, solo start/house entry/lobby return, missing-party recovery, incorrect-mode startup, movement and movable ottoman interaction. All passed without modifying production save/settings. The separate house walkthrough passed all 62 collision routes, ten floor contacts and 83 captures. These are automated checks, not a manual campaign or real-peer multiplayer session.

The final corrected campaign render sweep was stopped at the user's request before completion; visual acceptance and promotion into the normal CombinedLinux launcher remain pending. The installed launcher retains the previously verified ceiling build. Whole-house dimensional accuracy and unregistered photo-only rooms remain unresolved.

Latest local package: `Deliverables/S02MediaBinFinalReview20260928/Linux/Play Senior Sendoff.sh`.
Normal installed launcher: `Play Senior Sendoff.sh`.
Rebuild: `./Package Linux Game.sh`.

Build binaries, caches, personal save files and raw house photos/scans are excluded from Git. The generated Unreal assets are tracked with Git LFS.
