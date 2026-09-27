# Senior Sendoff

Unreal Engine 5.8 project for a three-chapter story campaign with saved checkpoints.

Open `SeniorSendoff.uproject` with Unreal Engine 5.8 for editing. On this Linux machine, `./Play\ Senior\ Sendoff.sh` starts the packaged game at the lobby when it exists; pass `--editor` to run the Editor game mode instead. `./Package\ Linux\ Game.sh` rebuilds the standalone Linux Development game under `Deliverables/CombinedLinux` when the local Unreal engine is installed. Generated build, editor cache, packages, and local save data are excluded from version control. Unreal binary assets are stored with Git LFS.

The packaged launcher is `Deliverables/CombinedLinux/Linux/Play Senior Sendoff.sh`. The September 26 Development package was built and its lobby and integrated Chapter 1 map each opened in headless runtime checks. The house also opened with Vulkan graphics enabled. An interactive packaged playthrough and networked furniture test remain future validation.

## Maps

The lobby and campaign maps are in `Content/Story/Maps`. Chapter 1 now travels to `Chapter01_House`, built from the registered house in the neighboring `Senior Sendoff` repository. The original `Chapter01` map remains as a prototype. `Content/SeniorSendOff/Maps/House_Walkthrough` is an independent art and traversal review map.

The story player uses WASD and mouse look, Space to jump, Shift to sprint, Ctrl to slide, and E to grab or release movable furniture. Esc opens the in-game pause/settings menu; Resume restores mouse capture. The first furniture slice includes a great-room chair, a chair by the entrance wing, a basement armchair, and an upstairs ottoman. Dragging uses physics and movement replication; the rest of the furnishings remain fixed for now.

The lobby and pause menu expose graphics preset, display mode, resolution, 3D render scale, frame cap, V-Sync, mouse sensitivity, vertical-look inversion, field of view, and lobby-motion controls. Settings persist locally. New installations start with Balanced graphics, 80% 3D render scale, a 90 FPS cap, and V-Sync off; raise quality or render scale from Settings if the machine has spare headroom. The house importer keeps every practical fixture but budgets dynamic shadows to selected lights, so room updates remain editable without reintroducing the old lighting cost.

Edit the house in `../Senior Sendoff/ArtSource/HouseRebuild/Reconstruction/House_GameReady.generated.blend`. From that repository run `python3 Tools/Unreal/sync_house_to_campaign.py` to re-export and refresh this project. The refresh only replaces generated house actors in `Chapter01_House`; the three campaign checkpoint/exit actors are retained. Do not edit generated house mesh assets by hand, because the next refresh replaces them.
