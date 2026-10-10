# Linux development handoff — October 10, 2026

This is the editable Unreal source project, not a prebuilt Linux executable.

## What is included

- All current `Config`, `Content`, and `Source` files and `SeniorSendoff.uproject`.
- The updated `Chapter01_House` map, completed rooms, nightclub, pigs, Boywick, furniture interactions, basketball court, exterior, front doors, brickwork, grass, wind, and cars.
- Imported meshes, textures, materials, animations, audio, MetaHuman assets, and selected authored source assets and tools.
- The existing Linux game launcher and packaging script, plus a helper to rebuild the editor module.

Local editor caches, Mac binaries, save games, raw photo references, revision backups, and local review captures are not required to edit or play this project and are not part of the new upload. The repository retains its previously tracked source-art files. Some historical source-art scripts refer to local reference files; the imported game assets are already in `Content` and do not need those scripts to open the project.

## Requirements

Use **Unreal Engine 5.8.2**. The Mac version used to save and validate this snapshot is build `56702186`, compatible changelist `55116800`, release branch `++UE5+Release-5.8`. The project descriptor associates with `5.8`; the helper checks the full version in the installed engine.

Install Git, Git LFS, and Python 3 on the Linux PC. Use a Linux Unreal installation with its C++ build tools and the plugins enabled in `SeniorSendoff.uproject`, including the MetaHuman editor tools, RigLogic, HairStrands, Python/editor scripting, and ProceduralMeshComponent. The engine and its licensed plugin content are supplied by the engine installation, not this repository.

## Get this version from GitHub

For a new checkout:

```bash
git lfs install
git clone https://github.com/davishann15-commits/phi-sig-game.git
cd phi-sig-game
git lfs pull
```

For an existing checkout, commit or stash local work before pulling the update:

```bash
git pull --ff-only
git lfs pull
```

Use Git LFS rather than GitHub's ordinary "Download ZIP" option. Asset files must be the actual binaries, not small files beginning with `version https://git-lfs.github.com/spec/v1`. The helper checks this before building. The final handoff tag identifies the exact saved snapshot even after `main` moves forward.

## Build and open on Linux

Set `SSO_UNREAL_ENGINE` to the engine root, the folder containing `Engine`. If it is already at `$HOME/UnrealEngine/5.8.2`, no override is necessary.

```bash
export SSO_UNREAL_ENGINE="$HOME/UnrealEngine/5.8.2"
./Setup\ Linux\ Development.sh
```

The helper fetches LFS assets when run from a Git checkout, checks the engine version and asset files, and builds `SeniorSendoffEditor` for Linux Development. Then open the editable project:

```bash
"$SSO_UNREAL_ENGINE/Engine/Binaries/Linux/UnrealEditor" "$PWD/SeniorSendoff.uproject"
```

Open `/Game/Story/Maps/Chapter01_House` and press Play to inspect the house. The default startup map remains the lobby. First startup will rebuild Linux shaders and caches; let that finish before assessing frame rate.

The house exterior, club, pigs, wind, and many other details are generated or controlled by native code when the map runs. Their source is under `Source/SeniorSendoff`. Do not replace the current map with `Chapter01_House 2` or regenerate the old base import over it.

## Build a standalone Linux game later

After the editor build works:

```bash
./Package\ Linux\ Game.sh
./Play\ Senior\ Sendoff.sh
```

This produces the Linux Development package in `Deliverables/CombinedLinux`. Neither the old Linux packages described elsewhere nor Mac binaries are this snapshot's Linux build.

## Validation for this handoff

The current source built successfully on the Mac with Unreal 5.8.2. Rendered in-game review covered the new brickwork, grass, and cars. The gust/leaf/audio check and all seven exterior walking routes passed, including front steps, both side paths, lawn/driveway connections, and front-door entry. These were assisted development checks, not a human walkthrough of every room.

The transferred source/assets and Git LFS objects are checked before publication. A native Linux build and playtest of this October 10 snapshot still need to run on the receiving Linux PC; this Mac cannot validate its Linux GPU, toolchain, or engine installation.
