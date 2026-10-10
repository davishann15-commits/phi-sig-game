# Fallen leaves, wind, and rustling

Updated 2026-10-10 in the Unreal `Chapter01_House` project.

## Changes

- Increased fallen-leaf clusters around trees and shrubs, with gold, copper, and brown leaf textures.
- Added a local pool of 96 folded leaves that drift, turn, tumble, and settle with varying gusts. Leaves follow queried ground heights and stop at walls or abrupt height changes. They have no gameplay collision.
- Tree leaves, shrubs, and grass respond to the same gust envelope.
- Added a looping, spatialized leaf-rustle recording. Its level follows the gust strength, fades indoors, and returns outdoors. Wall occlusion also reduces and filters the sound.
- The effect uses three instanced leaf meshes and one audio component per local player world. It adds no real lights, replicated physics bodies, or navigation obstacles.

Implementation: `Source/SeniorSendoff/HouseLeafWind.cpp` and `.h`, with initialization in `HouseExterior.cpp` and leaf geometry/material helpers in `HouseExteriorNature.cpp` and `.h`.

## Audio

“Rustling leaves” by giddster, [Freesound source](https://freesound.org/people/giddster/sounds/437356/), licensed [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).

The public high-quality preview was converted to mono 48 kHz PCM, trimmed, and crossfaded into a roughly seven-second loop. There are no clipped samples. Source URL, license, and SHA-256 are recorded in `assets/source.json`; analysis is in `evidence/audio-analysis.json`.

The saved SoundWave is `/Game/SeniorSendOff/House/ExteriorNature/S_LeavesRustlingLoop`. This directory was already included in the project's cook-directory list. `install_audio.py` imports and saves the looping asset; `prepare_audio.py` reproduces the source processing.

## Verified

- Native Mac editor build succeeded (`evidence/native-build.log`).
- SoundWave import and save succeeded (`evidence/audio-import.txt`).
- Rendered development-game review passed with normal gusts and real ground traces. The 96-leaf pool moved up to 462.3 cm; gust strength ranged from 0.13 to 0.95. Moving-leaf count rose to 92 during the strong gust and fell to zero during the lull.
- Audio was playing and its native playback cursor advanced through 620 callbacks. The exterior volume multiplier reached 0.292, fell to 0.00000 indoors, and returned to 0.130 outside.
- Ground-leaf previews before a gust, while tumbling, after settling, and on the front walk were inspected. The game exited cleanly with status 0. Evidence is in `evidence/playtest.log` and `evidence/rendered-review/`.
- The map, exterior paths, site layout, basketball court, and nightclub source match their starting hashes (`evidence/protected-check.json`). Unity files were not edited.

This was a rendered development-build check with a 30 FPS cap, not an uncapped performance comparison or packaged-build test. The pre-existing VSM non-Nanite marking-queue warning remains. Multiplayer clients share the gust clock, while leaves and audio are local cosmetic effects; a multiple-client session was not run in this pass.

To reproduce the focused review, launch the development game with `-HouseLeafWindReview`. The review captures four images, checks drift and audio fades, writes `Saved/LeafWindReview/result.txt`, and exits after about 27 in-game seconds. Normal launches do not run or alter the camera for this review.
