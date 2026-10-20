# Current development checkpoint — October 19, 2026

This checkpoint publishes the saved project as it currently exists so both collaborators can continue from the same version. It is not a final release or a freshly validated packaged build.

## Included work

- Current house and interaction changes, project configuration and Chapter 1 map.
- Campus and Calder terrain, trees, fall environment, wildlife and supporting assets.
- Installed five-house Fraternity Row, including walk-in interiors and backyards. Row remains paused and unfinished.
- Installed stadium changes through stage 7, including the approved snare sound. Earlier stages have recorded validation, but final stage 7 acceptance is incomplete.
- Runtime source, imported game assets, production source art and development tools. Local backups, reference collections, debug captures and generated Unreal caches remain local.

## Opening this version

Use Unreal Engine 5.8 with the project-enabled plugins. Existing Linux setup instructions are in [LINUX_DEVELOPMENT_HANDOFF_20261010.md](LINUX_DEVELOPMENT_HANDOFF_20261010.md); this checkpoint has not been newly built or playtested on Linux.

After pulling this commit, download the actual binary assets:

```sh
git lfs install
git lfs pull
```

Rebuild the C++ editor module before playing `Content/Story/Maps/Chapter01_House.umap`. The runtime-generated environment requires the matching source build. `Binaries`, `Intermediate`, `Saved`, local save files and packaged deliverables are intentionally excluded from Git.

## Outstanding validation

The stadium still needs its final performance, original regression chain, multiplayer and recovery checks. Benchmarking was interrupted by a Mac restart; partial timing results are not acceptance.

Row has current build, scene and dusk/night capture evidence, but its full current-source acceptance remains incomplete. Its R3 multiplayer host crashed in RenderCore; the previous Row revision also exceeded its performance budget in two views. Preserve these findings for later work.

The agreed map-wide exterior-light total is 57, including six Row lights; the shared interior-light pool remains six. Preserve the existing map bounds and the paused Row while completing the stadium.

Current feature handoffs are in `SourceArt/StadiumDetail/CODEX_HANDOFF.md` and `SourceArt/FraternityRow/CODEX_HANDOFF.md`. Some historical diagnostic links there point into the original machine's local `Saved` directory and are not included in this Git checkpoint. A completed write-up or patch does not establish a new native test pass.
