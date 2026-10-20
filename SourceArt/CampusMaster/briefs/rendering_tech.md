# Rendering / tech brief: campus master job (layout, enterable interiors, eye-level polish)

Project: `/Users/Stewart/Documents/Unreal Projects/SeniorSendoff` (UE 5.8.2, Mac M5, Metal SM6). Paths below are relative
to the project unless absolute. `S/` = `Source/SeniorSendoff/`. Coordinates are world cm, Z up, campus grade Z = -60. This
was a read-only survey: nothing in the project was edited, built or run. Facts marked *(log)* come from the existing run log
`scratchpad/runs/roam_on3/Saved/Logs/SeniorSendoff.log` (2026-10-13, current code).

Helper written next to this brief: `briefs/rendering_tech_pngio.py`, a pure-Python PNG reader/writer/vertical-flip. It was
tested on a real UE screenshot: 1280x720 RGBA decodes in 0.5 s.

---------------------------------------------------------------------------------------------------------------------------

## 0. Summary of decisions and constraints

1. **New material graphs need editor-saved assets.** At runtime the code can only make MIDs of existing parents. The
   documented route is an editor-Python script run headless with `UnrealEditor-Cmd -run=pythonscript`. It has been used
   seven times already (§b). Two assets are worth adding: `M_CampusInteriorWindow` (interior mapping) and
   `M_LightPoolDecal` (emissive-only decal). Put them in a NEW folder, `/Game/SeniorSendOff/House/CampusInterior`, and add
   that folder to `DirectoriesToAlwaysCook`. If assets are not allowed, use the fallback in §b.5: real shallow window boxes
   with the existing masters.
2. **Lumen GI is on and Lumen reflections are off** at the shipped scalability (sg.*=1). Lumen uses software tracing of the
   global SDF only. **No runtime-built geometry is in the Lumen scene.** The engine fast-build path and
   `UProceduralMeshComponent` have no distance field and no mesh cards. As a result:
   - emissive surfaces on runtime meshes light other surfaces only through screen traces;
   - runtime-built rooms are not occluded from the sky light, so it leaks in.

   Light campus interiors with real lights. Stream those lights, put them on their own lighting channel, and keep them
   shadowless by default. Use emissive for fixtures and windows only. Optionally add hidden "Lumen proxy" boxes (§c.5).
3. **Inside a campus building the game currently thinks you are outdoors.** `DistanceOutsideHouse` only knows the house's
   three boxes. Inside a campus building you get:
   - the full dusk film look: grain, teal shadows, vignette 0.55 and fringe;
   - exposure -4.15 instead of the house rooms' -4.5;
   - no indoor duck on the clock-tower bells;
   - leaf-rustle audio.

   Also, **shadowless exterior lights pass through walls**: the 3 stadium lights, the street, side-street and lot sodium
   spots, and one lot pole whose lamp head is inside block 1. Fix with an "inside any interior" test and lighting channels
   (§c.4).
4. **Stream interiors per building on every machine.** Use the union of all known pawn positions plus the local view
   location, with hysteresis. Server and clients must both hold collision wherever any pawn is. Prefer runtime
   `UStaticMesh` (`HwBuildStaticMesh`, cached draw path, box collision) over `UProceduralMeshComponent` (dynamic draw path
   every frame, and synchronous trimesh cooking) for interior shells and furniture (§d).
5. **Ortho site and floor plans work with a small change to `-HouseExteriorViews`.** Add `OrthoWidth` and `OrthoNear` to
   `FExView` and set the camera projection in `TickHouseExteriorViews`. Floor plans come free as a section cut using the
   ortho near clip plane. **New ExViews must be named `detail_*`**, otherwise they join the tree-planting no-go zones and
   change `crc=0f3b2bb7` (§e).

---------------------------------------------------------------------------------------------------------------------------

## (a) Materials used at runtime

### a.1 Parents and their graphs

Parameter values come from `Saved/HallUser/Saved/HouseBlackHallSurvey/census.json`, written by the survey in
`S/HouseBlackHallSurvey.cpp:127-143`. The survey dumps scalar, vector and texture params plus blend mode and two-sidedness.
Node counts come from the export-name numbering in the `.uasset`. The method is validated: the scalar and vector counts
match the census for all four parents.

| Parent (path) | Blend / sides / shading | Parameters (defaults) | Maps | Graph (node inventory) | Built by |
|---|---|---|---|---|---|
| `/Game/SeniorSendOff/House/Materials/M_House_Plain` | Opaque (0), one-sided, DefaultLit | `Tint` V(.5,.5,.5), `Roughness` .7, `Metallic` 0, `Emission` V(0) | none | 2 scalar + 2 vector params wired straight to BaseColor / Roughness / Metallic / Emissive | Hand-made, in git since 0dfa640f (2026-09-27). No script. |
| `.../M_House_Textured` | Opaque, one-sided | `BaseMap` (WhiteSquareTexture), `NormalMap` (DefaultNormal), `UVScale` 1, `Roughness` .7, `Metallic` 0, `NormalStrength` 1, `Tint` .5, `Emission` 0 | BaseColor + tangent normal only | 1 TexCoord, 3 Multiply (UV×UVScale, BaseMap×Tint, normal.xy×Strength), 2 TextureSampleParameter2D, 2 Append, 1 Constant, 1 Normalize, 4 scalars, 2 vectors | Hand-made |
| `.../M_House_Normal` | Opaque, one-sided | as Textured without BaseMap | normal only | same pattern | Hand-made |
| `.../M_House_Glass` | **Translucent (2), two-sided** | `Opacity` .18, `Roughness` .7, `Metallic` 0, `Tint` .5, `Emission` 0 | none | 3 scalars + 2 vectors; ISM usage flag; no Nanite usage flag | Hand-made |
| `/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropPlain` / `M_NaturePropTextured` | as M_House_* but **two-sided**, ISM usage flag | same | same | `duplicate_asset` of M_House_* | `SourceArt/TreeLeafPolish/install_foliage.py:96-108` |
| `.../M_NatureTreeFoliage` | Masked (clip .42), two-sided, TwoSidedFoliage | `BaseMap`, `AtlasRect` V(0,0,1,1), `Tint`, `Transmission` .42, `Roughness` .86, `Specular` .16, `WindStrength` 1.8 | atlas | OpacityMask = BaseMap.a × PerInstanceFadeAmount; flat normal × TwoSidedSign; WPO wind (Custom) | `install_foliage.py:65-95` |
| `.../M_NatureFoliage` | Masked .32, two-sided, TwoSidedFoliage | `BaseMap`, `NormalMap`, `Tint`, `Transmission` .25, `Roughness` .82, `Specular` .2, `WindStrength` 1.3 | base, normal | WPO wind | `SourceArt/AutumnNature/install_materials.py:107-129` |
| `.../M_NatureGround`, `M_WoodsGround`, `M_WoodsTrail` | Opaque | macro/coverage maps in world space (`(P.xy+(8200,10000))/(16400,18000)`) | base, normal, dry, macro | Custom HLSL blends | `AutumnNature/install_materials.py:89-105`, `SourceArt/WoodsSmooth/install_materials.py:38-62` |
| `/Game/SeniorSendOff/House/ExteriorBrick/M_Brick`, `M_BrickFace` | Opaque | `BaseMap`, `NormalMap`, `SurfaceMap` (R roughness, G AO, B height), `HeightMap`, `Tint`, `Specular` .22 | **The only masters with a roughness/AO map** (M_Brick also has BumpOffset parallax) | world-space variation + wet base (Custom) | `SourceArt/ExteriorBrick/install_materials.py:80-100` |
| `.../ExteriorCars/M_CarPaint` (ClearCoat), `M_CarSurface` | Opaque, MaterialAttributes | `Tint`, `Roughness`, `Metallic`, `Specular`, `Emission` (+ `ClearCoat`, `ClearCoatRoughness`) | none | MakeMaterialAttributes | `ExteriorBrick/install_materials.py:122-136` |
| `.../ExteriorNature/M_NatureGrassBlades` | two-sided, TwoSidedFoliage | `Tint`, `Transmission` .65, `Roughness`, `Specular`, `WindStrength` 3 | none | PerInstanceRandom, WPO | `ExteriorBrick/install_materials.py:102-120` |
| `/Game/SeniorSendOff/House/ExteriorStreet/M_StreetDecal` | **DeferredDecal**, Translucent | `Atlas`, `Tint`, `Opacity` 1, `Roughness` .7 | atlas (rgb + alpha) | BaseColor = Atlas.rgb×Tint, Opacity = Atlas.a×Opacity | `Tools/claude/build_street_materials.py:74-91` |
| `.../ExteriorStreet/M_StreetSmoke` | Translucent, **two-sided**, TLM_VolumetricNonDirectional | `Puff`, `SmokeColor` V(.5,.52,.55), `Glow` 4, `Opacity` .55 | puff alpha | Emissive = VertexColor×Glow; Opacity = Puff.a×Vertex.a×Opacity → DepthFade (45 cm) | `build_street_materials.py:93-116` |
| `.../ExteriorNature/M_WoodsSignText` | Unlit (copy of `/Engine/EngineMaterials/DefaultTextMaterialOpaque`) | font | n/a | emissive text | `SourceArt/WoodsFogSign/install_sign_material.py` |

What this means for interiors:

- **Emission on M_House_Plain and M_House_Textured is a uniform vector.** It is wired straight to Emissive and is not
  multiplied by BaseMap: there are only 3 Multiply nodes, all accounted for above. So a "lit painted window" made with
  `HallMakeTextured(..., Emission)` glows as one flat colour on top of the painting.
  - The house materials have **no emissive map, no roughness map, no AO map and no opacity map** (Glass has a scalar
    opacity only).
  - In the CampusFall code the emission values on textured MIDs are kept tiny, e.g. `CF_Field` .006, `CF_Sheet*` .05
    (`S/HouseCampusFallLook.cpp:413-435`). Real glow comes from separate quads with Plain MIDs: `EX_Window` emission
    (2.2,1.6,0.9) at `S/HouseExterior.cpp:414`, and `CF_Candle`, `CF_Chase` and similar at
    `HouseCampusFallLook.cpp:504-510`.
  - Only the Brick masters have a roughness/AO map. Only M_StreetSmoke has a vertex-colour emission path.
- **Usage flags.** Before Oct 10, M_House_Plain and M_House_Textured lacked `bUsedWithInstancedStaticMeshes`. The native
  game then logged *"missing usage flag InstancedStaticMeshes! Default Material will be used in game"*
  (`SourceArt/ExteriorBrick/evidence/visual-review.log`, `SourceArt/TreeLeafPolish/views-first-material-error.log`).
  - The four `M_House_*.uasset` files are now **modified in the working tree** (`git status`). Their strings show the ISM
    and Nanite usage flags, and the 2026-10-13 log has 0 LogMaterial warnings.
  - Do not edit these four assets. They are shared by the whole house and someone else's uncommitted re-save is sitting
    on them.
- **Substrate is on** (`Config/DefaultEngine.ini:37-39`, `r.Substrate=True`, `ProjectGBufferFormat=0` Blendable GBuffer)
  and has been since the first commit. Legacy pins are auto-converted. All of the scripts above author legacy pins and
  work, including decals.

### a.2 Runtime material helpers (`S/HouseBlackHallLook.h/.cpp`)

- `HallHouseMat(Name)` (`HouseBlackHallLook.cpp:45-48`) loads `/Game/SeniorSendOff/House/Materials/<Name>.<Name>`.
- `HallMakeTextured(Sub, Key, Base, Normal, Rough, Tint=White, NormalStrength=1, Metal=0, Emission=Black)` (`:50-66`)
  creates a MID of M_House_Textured and sets BaseMap, NormalMap, UVScale=1, Roughness, Metallic, NormalStrength, Tint and
  Emission. It is registered as `Sub->Materials[Key]`.
- `HallMakePlain(Sub, Key, Tint, Rough, Metal=0, Emission=Black)` (`:68-80`) does the same for M_House_Plain.
- Geometry picks materials by key. `FHallMesh::Build(PM, [Look](FName K){ return Look->Mat(K); }, bCollision)`
  (`S/HouseBlackHallMesh.h:246-257`) makes one ProcMesh section per key. `HwBuildStaticMesh` makes one material slot per
  key (`S/HouseDetailProps.cpp:323-330`).
- The 298 hand-made house MIs in `Content/SeniorSendOff/House/Materials/MI_SSO_*` are all instances of M_House_* and use
  real texture assets. Examples:
  - `MI_SSO_BHSealedConcrete`, which uses `T_BHSealedConcrete_Base` and `_Normal`;
  - `MI_SSO_BHLightNeutral`, which has emission (39.8, 42.7, 44.6) and is the black hall's light fixture;
  - `MI_SSO_FloorWood`, `MI_SSO_OakTone0..7`, `MI_SSO_PaintCream`, `MI_SSO_S03HallConcrete`, and many more.

  These can be put on runtime meshes directly with `LoadObject<UMaterialInterface>`. That is the cheapest way to get
  "same craft" campus interiors. The shared textures are in `Content/SeniorSendOff/House/Textures/`: T_Plaster_Normal,
  T_Masonry_Base/Normal, T_Concrete_Normal, T_Carpet_Normal, T_Wood_Normal, T_S02FloorOak_*, T_S03HallConcrete_*,
  T_S03WornConcrete_*, T_S03BathFloor_*, T_S03Counter_*, and others.

### a.3 Painted textures (FHallImage → texture)

- `FHallImage` (`S/HouseBlackHallPaint.h:8-29`) is a CPU canvas of linear `FLinearColor` pixels. It provides `At`,
  `Sample`, `Blend`, `FillPolygon`, `Stroke`, `Ellipse` and `Rect` (anti-aliased, pixel space, y down). Related helpers:
  - `HallDrawText` and `HallTextWidth`, a stroke font (`:37-53`);
  - `HallNoise` and `HallFbm`;
  - `HallHeightToNormal(Height, W, H, Strength, bWrap, Out)`, which turns a height field into a tangent normal image
    (`:34-35`).
- `HallMakeTexture(Img, bSRGB, bNormalMap, Name, bWrap=true)` (`S/HouseBlackHallPaint.cpp:166-210`):
  - makes `UTexture2D::CreateTransient`, PF_B8G8R8A8, with the **full mip chain built on the CPU** (2×2 box);
  - `TF_Trilinear`, `TEXTUREGROUP_World`, `TC_Normalmap` for normals, wrap or clamp;
  - **forces alpha = 1** (`:185`).
- `HouseStreetScene::MakeTexture(...)` (`S/HouseDuskAtmosphere.cpp:38-90`) is the alpha-preserving variant, with
  coverage-weighted mips. Use it for any atlas that needs alpha: decals, curtain or furniture cards, the interior-mapping
  atlas.
- Usage pattern (`S/HouseCampusFallLook.cpp:413-418`): paint → `HallMakeTexture` → `Look->Textures.Add(T_Key)` →
  `HallMakeTextured(Look, Key, T, nullptr, Rough, White, 0, 0, Emission)`. The textures are uncompressed and never stream:
  - 1024² is about 5.6 MB with mips;
  - 2048² is about 22 MB;
  - painting a 1024² image in C++ loops takes tens of ms on the game thread. Paint once and cache, or paint on a worker
    (FHallImage is plain data).
- Mip and bias gotcha: textures are clamp or wrap per call. An atlas sampled with computed UVs needs `SampleLevel` or an
  inset (see the HLSL in §b.3).
- **ProcMesh UVs are half precision.** `FStaticMeshVertexBuffers::InitFromDynamicVertex` leaves
  `bUseFullPrecisionUVs=false` (`/Users/UE_5.8/Engine/Source/Runtime/Engine/Private/StaticMesh.cpp:1377-1398`,
  `.../Rendering/StaticMeshVertexBuffer.cpp:68`).
  - World-planar UVs with a 100 cm period at |X| of about 15000 are quantised to 1/8 of a tile.
  - For anything that encodes data in UVs, keep the values in about [0,8].
  - Integers up to 2048 are exact in half precision.

---------------------------------------------------------------------------------------------------------------------------

## (b) Adding new material assets through the editor-Python pattern

### b.1 The pattern (seven precedents)

- **Scripts:**
  - `Tools/claude/build_street_materials.py`, which has a skip-if-exists idiom;
  - `SourceArt/AutumnNature/install_materials.py`, `SourceArt/TreeLeafPolish/install_foliage.py`,
    `SourceArt/ExteriorBrick/install_materials.py` and `SourceArt/WoodsSmooth/install_materials.py`. These rebuild in
    place with the idiom `load or create → delete_all_material_expressions → rebuild → recompile → save_loaded_asset(m,
    False)`.
  - `SourceArt/WoodsFogSign/install_sign_material.py`, which uses `duplicate_asset`.
- **Shared helper idiom.** Copy it from `ExteriorBrick/install_materials.py:13-53`:
  - `node(m, kind, **props)` → `E.create_material_expression(m, getattr(unreal, 'MaterialExpression'+kind))`;
  - `link`, which tolerates pin-name differences between releases (e.g. `Coordinates` vs `UVs`);
  - `out(n, 'BASE_COLOR'|...)`;
  - `custom(m, code, [(name, src)], 'CMOT_FLOAT1..4')` for Custom HLSL nodes with named inputs;
  - `scalar`, `vector`, `op('Multiply', a, b)`, and texture imports through `unreal.AssetImportTask`.
- **Headless run.** Command used for TreeLeafPolish (`SourceArt/TreeLeafPolish/import4.log`, commandline line):
  ```
  /Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd "/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SeniorSendoff.uproject" \
     -run=pythonscript -script="<abs path>.py" -unattended -nosplash -nopause -NullRHI -NoTraceServer \
     -UserDir=<scratch dir> -AbsLog=<scratch>/mat.log
  ```
  - `Tools/claude/ue_py.sh:4-11` wraps the same call without `-NullRHI` and `-UserDir`, logging to
    `Saved/ClaudeDump/<name>.log`. Prefer the flags above.
  - `Tools/claude/Claude Job Runner.command` runs `*.sh` jobs dropped in `Tools/claude/queue/`.
  - The commandlet exits after the script. `unreal.SystemLibrary.quit_editor()` (WoodsSmooth, WoodsFogSign) is harmless.
- **Must the user's editor be closed?**
  - Right now no `UnrealEditor` process is running (`pgrep` came back empty at survey time).
  - Earlier runners assert that the editor is closed before game runs: `SourceArt/WoodsSmooth/run.py:3` and
    `SourceArt/WoodsFogSign/run.py:3` both run `assert pgrep -x UnrealEditor` and expect it to fail.
  - Creating **new** assets in a **new** folder while an editor is open works mechanically. The open editor's directory
    watcher picks the files up, and building the C++ module with the editor open is fine according to memory.
  - It is unsafe to **overwrite** an asset that the open editor has loaded. The editor keeps the stale object and can
    save over the new one.
  - Rule: run it when the editor is closed and Codex is idle (check mtimes under `Source/`, `Config/` and `Content/`).
    Never touch existing assets. Never load or save `Chapter01_House`.

### b.2 Risks and checklist for new assets

1. **`-NullRHI` does not validate Metal shaders.**
   - TreeLeafPolish's Custom HLSL saved fine, but the first `-game` run logged *"Failed to compile Material
     .../M_NatureTreeFoliage for platform SF_METAL_SM6, Default Material will be used in game … vector swizzle 'zw' is out
     of bounds"* (`SourceArt/TreeLeafPolish/views-first-material-error.log:2875-2879`).
   - Always follow with a short `-game` run (e.g. `scratchpad/run_game.sh`) and
     `grep -a "Failed to compile Material\|LogShaderCompilers: Warning\|missing usage flag"`.
   - The Custom-node pitfalls behind that error: use explicit `float2`/`float3` types and do not swizzle beyond a
     input's width.
2. **Usage flags.** Set `used_with_instanced_static_meshes=True` before saving if the material will ever go on
   HISM/ISM; set the Nanite flag only if needed. ProcMesh and decals need no flag. A missing flag means the Default
   Material in the native game.
3. **Packaging.** Runtime `LoadObject` is not a cook reference.
   - Add `+DirectoriesToAlwaysCook=(Path="/Game/SeniorSendOff/House/CampusInterior")` to `Config/DefaultGame.ini`
     (current list: `:12-34`).
   - Pre-existing gap: `/Game/SeniorSendOff/House/ExteriorStreet` (M_StreetDecal, M_StreetSmoke) is **not** in the list
     and is untracked in git (`?? Content/SeniorSendOff/House/ExteriorStreet/`). Packaged builds would lose the road
     decals, smoke and halos. Fix it in the same pass.
4. **Git LFS.** `.gitattributes` puts `*.uasset` under LFS. New assets are new LFS objects; the user commits.
5. **Module freshness.** The commandlet loads the `SeniorSendoffEditor` module. If Source has changed since the last
   build, build first with `Build.sh SeniorSendoffEditor Mac Development …`, or `-unattended` fails on the out-of-date
   module.
6. **Determinism and idempotence.** The script must:
   - write only its own folder;
   - use the skip-if-exists or delete-all-expressions-and-rebuild idiom;
   - log a marker such as `CAMPUS_INTERIOR_MATERIAL_SAVED`;
   - write an evidence txt.
7. **Graceful degradation.** Every runtime use must null-check the parent, as `HouseStreetScene::MakeDecalMaterial`
   already does (`S/HouseDuskAtmosphere.cpp:92-103`), and fall back to existing keys: `EX_Window`/`EX_Glass` for
   windows, no decal for pools. A missing asset then never breaks a review.

### b.3 `M_CampusInteriorWindow`: interior mapping with engine nodes and one Custom node

**Purpose.** Replace the flat `EX_Window` / `EX_Glass` panes on campus façades with a fake room seen through the glass. The
panes are currently built at `S/HouseSurroundingsPolish.cpp:101` and in the fallback at `S/HouseExteriorCampus.cpp:31-34`.
The pane itself stays one opaque quad: no façade holes, no translucency.

**Material settings.** Domain Surface, Opaque, DefaultLit, one-sided, `used_with_instanced_static_meshes=True`.

**Graph:**

```
TexCoord[0] ─────────────┐
CameraVectorWS ──────────┤                                   (V = unit vector surface → camera)
VertexNormalWS ──────────┤
TextureObjectParameter "RoomAtlas" (default WhiteSquareTexture) ─┤
VectorParameter "RoomSize" (360, 415, 520)  [W, story H, depth cm] ┤
VectorParameter "AtlasGrid" (6, 8, 0)       [columns, style rows]  ┤
ScalarParameter "Style" 0                                          ┤
ScalarParameter "CellPx" 256                                       ┤
ScalarParameter "MidDepth" .42                                     ┤
ScalarParameter "BackDarken" .55 ──────────────────────────────────┴─► Custom "InteriorRoom" (CMOT_Float3) = room colour
Custom × VectorParameter "LitColor" (1,.82,.62) × ScalarParameter "LitScale" 1.6 ─┐
Fresnel(Exponent 5) × VectorParameter "SkyReflection" (.020,.032,.055) ──────────┴─ Add ─► Emissive
VectorParameter "GlassTint" (.012,.015,.02) ─► BaseColor
ScalarParameter "Roughness" .08 ─► Roughness          ScalarParameter "Specular" .5 ─► Specular
```

- **UV0 contract.** The C++ builder writes it, so no world-space phase is needed and UVs stay in [0,1] (half-precision
  safe).
  - `u` is the pane position across its room, 0 = room's left wall to 1 = right wall, as seen from outside.
    "Right" is `R = normalize(cross(N, Z)) = (Ny, -Nx, 0)`.
  - `v` is 0 = room floor to 1 = ceiling.
  - Example: for a 159 cm glass width centred in a 360 cm bay, `u ∈ [0.279, 0.721]`. With the sill 98 cm above floor
    and a 196 cm tall pane in a 415 cm story, `v ∈ [0.236, 0.708]`.
  - Per face: +Y face R=(1,0,0); -Y face R=(-1,0,0); -X face R=(0,1,0); +X face R=(0,-1,0).
  - Window grid (`S/HouseExteriorCampus.cpp:22-38`): 360 cm pitch from `B.Min+160`, 175×212 cm panes,
    `Z = Ground+90+Floor*Story` with `Story = (H-50)/Floors` and `Floors = clamp(floor(H/420), 2, 7)`.
- **Custom node body** (inputs `UV, V, N, Atlas, Size, Grid, Style, CellPx, Mid, Back`). It returns `float3`:

```hlsl
float3 n  = normalize(N);
float3 R  = normalize(cross(n, float3(0, 0, 1)));            // viewer's right on the facade
float3 d  = -V;                                               // camera -> surface
float3 rd = float3(dot(d, R) / Size.x, d.z / Size.y, max(dot(d, -n), 1e-4) / Size.z);
rd = (abs(rd) < 1e-5) ? 1e-5 : rd;
float3 ro = float3(saturate(UV.x), saturate(UV.y), 0.0);
float3 tA = (step(0.0, rd) - ro) / rd;                        // distance to the wall each axis heads for
float  t  = min(min(tA.x, tA.y), tA.z);
float3 h  = ro + rd * t;
float2 f; float col;
if (t >= tA.z)       { f = float2(h.x, 1.0 - h.y);                        col = 0.0; }  // back wall
else if (t >= tA.x)  { f = float2(rd.x > 0 ? 1.0 - h.z : h.z, 1.0 - h.y); col = rd.x > 0 ? 2.0 : 1.0; } // right / left wall
else if (rd.y < 0)   { f = float2(h.x, 1.0 - h.z);                        col = 3.0; }  // floor (far edge at top of cell)
else                 { f = float2(h.x, h.z);                              col = 4.0; }  // ceiling
float2 g   = float2(length(ddx(UV)), length(ddy(UV)));
float  lod = max(0.0, log2(max(max(g.x, g.y) * CellPx * 2.0, 1e-4)));
float  inset = 0.5 / CellPx;
f = clamp(f, inset, 1.0 - inset);
float3 c = Atlas.SampleLevel(AtlasSampler, (float2(col, Style) + f) / Grid.xy, lod).rgb;
float edge = min(min(h.x, 1.0 - h.x), min(h.y, 1.0 - h.y));
c *= lerp(1.0, Back, saturate(h.z)) * (0.72 + 0.28 * saturate(edge * 6.0));   // depth falloff + corner occlusion
float  tm = Mid / rd.z;                                        // midground card plane (desks, bunks, chair backs)
float2 hm = ro.xy + rd.xy * tm;
if (tm < t && all(hm >= 0.0) && all(hm <= 1.0))
{
    float4 m = Atlas.SampleLevel(AtlasSampler, (float2(5.0, Style) + clamp(float2(hm.x, 1.0 - hm.y), inset, 1.0 - inset)) / Grid.xy, lod);
    c = lerp(c, m.rgb, m.a);
}
return c;
```

- **Atlas.** Columns are `[0 back | 1 left | 2 right | 3 floor | 4 ceiling | 5 midground RGBA card]` × 8 style rows, 256
  px cells, for a 1536×2048 texture (about 17 MB with mips).
  - Paint it at runtime with FHallImage in the house style, for example dorm room, lecture hall, lab, library stacks,
    office, stair, lounge, and closed blinds.
  - Make it with `HouseStreetScene::MakeTexture(Img, true, false, TEXT("T_CI_RoomAtlas"), false)`, which keeps the alpha
    the midground card needs.
  - Paint the ceiling fluorescent fixtures bright. They are what reads as "lit room".
- **Variation and lit pattern are decided in C++, not in the shader.**
  - Register MIDs per `(style 0..7) × (lit, dark)`, 16 MIDs, as keys like `CI_Win_<s>_<L>`.
  - Lit: `LitScale` 1.6 matches the luminance of `EX_Window` (2.2, 1.6, .9). Dark: `LitScale` about .04.
  - Pick lit or dark with `HouseCampusFall::WindowLit(Index, Floor, Seed)` (`S/HouseCampusFall.h:35-36`) so the façade
    keeps its present 1-in-5 pattern (about 2-in-5 for dorms).
  - Mirror `u` (u → 1-u) on alternate bays for free variety.
  - A per-pixel hash of world position is **not** recommended for room identity. It flickers at quantisation boundaries,
    and UV0 is half precision.
- **Python builder sketch** (same helper idiom as `ExteriorBrick/install_materials.py`). Writes only
  `/Game/SeniorSendOff/House/CampusInterior`:

```python
m = material('M_CampusInteriorWindow')                    # load-or-create, delete_all_material_expressions
m.set_editor_property('used_with_instanced_static_meshes', True)
white = A.load_asset('/Engine/EngineResources/WhiteSquareTexture')
atlas = node(m, 'TextureObjectParameter', parameter_name='RoomAtlas', texture=white)
room = custom(m, HLSL_ROOM, [('UV', node(m, 'TextureCoordinate')), ('V', node(m, 'CameraVectorWS')),
              ('N', node(m, 'VertexNormalWS')), ('Atlas', atlas), ('Size', vector(m, 'RoomSize', (360, 415, 520))),
              ('Grid', vector(m, 'AtlasGrid', (6, 8, 0))), ('Style', scalar(m, 'Style', 0)), ('CellPx', scalar(m, 'CellPx', 256)),
              ('Mid', scalar(m, 'MidDepth', .42)), ('Back', scalar(m, 'BackDarken', .55))], 'CMOT_FLOAT3')
lit  = op(m, 'Multiply', op(m, 'Multiply', room, vector(m, 'LitColor', (1, .82, .62))), scalar(m, 'LitScale', 1.6))
refl = op(m, 'Multiply', node(m, 'Fresnel'), vector(m, 'SkyReflection', (.020, .032, .055)))
out(op(m, 'Add', lit, refl), 'EMISSIVE_COLOR')
out(vector(m, 'GlassTint', (.012, .015, .02)), 'BASE_COLOR')
out(scalar(m, 'Roughness', .08), 'ROUGHNESS'); out(scalar(m, 'Specular', .5), 'SPECULAR')
save(m)                                                    # recompile + save_loaded_asset(m, False) + log marker
```

- **Engine facts checked for this graph:**
  - Custom nodes support Texture-object inputs. The sampler is `<InputName>Sampler`, here `AtlasSampler`.
  - `AdditionalOutputs` exist if a second output is wanted
    (`/Users/UE_5.8/Engine/Source/Runtime/Engine/Public/Materials/MaterialExpressionCustom.h:88-97`; python
    `unreal.CustomOutput`).
  - The ddx/ddy in the Custom code is fine because the code only feeds Emissive, which is pixel-shader only.
- **Pane swap when a building's interior streams in.** Put each building's panes in their own component, e.g.
  `CampusWindows_<bld>`, with one section per key. On load, swap the panes of enterable rooms to a `M_House_Glass` MID
  (two-sided translucent, Opacity about .12). Panes on floors that are not modelled stay interior-mapped.

### b.4 `M_LightPoolDecal`: additive ground "light pool"

- **Engine fact.** An emissive-only deferred decal goes to the Emissive render stage and is blended
  `SceneColor += Emissive × Opacity`. The blend state is `TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_One>`
  (`/Users/UE_5.8/Engine/Source/Runtime/Renderer/Private/DecalRenderingCommon.cpp:891-919`). `EDecalBlendMode` is
  deprecated and inferred from the connected pins (`.../Engine/Public/Materials/Material.h:74-100,489`).
- **Rule:** connect only Emissive (colour × intensity) and Opacity (the falloff shape). Do **not** multiply the shape into
  Emissive as well, or it is applied twice.
- **Graph:** Domain DeferredDecal, BlendMode Translucent:

```
TexCoord[0] (decal UV 0..1) → Custom "PoolMask" (CMOT_Float1):
    float2 d = (UV - 0.5) * 2.0; d.x *= Stretch;            // Stretch > 1: narrower across (window throw)
    float a = saturate(1.0 - length(d)); a = a * a * (3.0 - 2.0 * a);
    return pow(a, Falloff);                                   // ScalarParameters Stretch 1, Falloff 1.4
  → Opacity
VectorParameter "Color" (1, .62, .32) × ScalarParameter "Intensity" .5 → Emissive
```

  Python: `m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)` and `blend_mode
  BLEND_TRANSLUCENT`, exactly as `build_street_materials.py:76-81`. Then `out(mask,'OPACITY')` and
  `out(color*intensity,'EMISSIVE_COLOR')`.
- **Spawning.** Copy `HouseDusk::SpawnDecals` (`S/HouseDuskAtmosphere.cpp:202-222`):
  - `UDecalComponent`, rotation `FRotator(-90, Yaw, 0)` (projects down);
  - `DecalSize = (half depth ≈ 60, half width, half length)`;
  - use a small `FadeScreenSize` such as .004 instead of 0, so distant pools cull.
  - Place under each lit ground-floor window, at the window base + N·(120..180) cm, about 160×260 cm, warm colour.
  - Place under open doors and lit vestibules (cool white for mercury, warm for incandescent).
  - Budget: no more than about 120 pools. There are already 177 road decals *(log: `roadDecals=177/177`)*.
- **Calibration.** Exposure is manual -4.5 (+0.35 outdoors). Start at `Intensity` .3–.5 against `EX_Window` (2.2, 1.6, .9)
  and tune with an A/B `detail_*` view.
- Pools are not real light. Characters standing in them get the glow on their feet, so keep the decal depth shallow (60).
  They also add no bounce.
- **Why not reuse M_StreetSmoke on a ground quad?** Its DepthFade (45 cm) erases a quad lying 2 cm above the ground. And
  M_StreetDecal only writes BaseColor to the GBuffer, which recolours the ground but adds no light.

### b.5 Fallback if no new assets are allowed

Real **window boxes** with existing masters:
- Cut each pane opening in the façade and put a shallow room behind it: a back wall and two side walls 60–150 cm deep,
  plus floor and ceiling.
- Use `HallMakeTextured` with painted cards. Lit rooms use uniform `Emission`, so the whole box glows evenly, which is
  acceptable at distance.
- Glaze with an `M_House_Glass` MID.
- Cost: 5 quads per window; about 1,500 windows comes to roughly 15k triangles.
- This needs façade holes, which enterable buildings need anyway for doors. It is heavier on translucency than interior
  mapping.

---------------------------------------------------------------------------------------------------------------------------

## (c) Lighting, GI and exposure

### c.1 Settings in force (config and log)

| Setting | Value | Source |
|---|---|---|
| Dynamic GI | Lumen (`r.DynamicGlobalIlluminationMethod=1`) | `Config/DefaultEngine.ini:19`; *(log)* `r.Lumen.DiffuseIndirect.Allow:1` is never turned off by GI@1 |
| Lumen tracing | software only. `r.Lumen.TraceMeshSDFs=0` (global SDF), `r.Lumen.HardwareRayTracing 1→0` (Mac profile), `r.RayTracing=False` | `DefaultEngine.ini:20,31-35`; `Config/DefaultDeviceProfiles.ini:4`; *(log)* GI@1 sets `r.Lumen.TraceMeshSDFs.Allow:0`, `SurfaceCache.CardMaxResolution 256` |
| Reflections | `r.ReflectionMethod=1` (Lumen) in the project, **but ReflectionQuality@1 sets `r.Lumen.Reflections.Allow:0` and `r.SSR.Quality:2` (half res)** | `DefaultEngine.ini:17`; *(log)* lines ~896-899 |
| Shipped scalability | all `sg.*=1`, ResolutionQuality 80, 90 fps cap | `Config/DefaultGameUserSettings.ini` |
| Confirmed live state | `gi=1 lumenHw=0 lumenMeshSdf=0 vsm=1 vsmPages=1024 vsmCoarseNonNanite=0 skylight=1.30` | `HOUSE_WALK_PERF_READY` lines in older run logs (S/HouseTraversalSmoke.cpp:202) |
| Virtual shadow maps | on; `MaxPhysicalPages=1024` at ShadowQuality@1; Mac: `NonNanite.IncludeInCoarsePages=0`, `ResolutionLodBiasLocal=1.0` | `DefaultEngine.ini:21`; `Config/DefaultScalability.ini`; `DefaultDeviceProfiles.ini:2-11` |
| Sky light multiplier | 1.3 (Mac device profile) | `DefaultDeviceProfiles.ini:6` |
| Static lighting | disallowed (`r.AllowStaticLighting=False`) | `DefaultEngine.ini:27` |
| Reviews | many force `SetOverallScalabilityLevel(2)` (e.g. `S/HouseExteriorWalkReview.cpp:61`); `-HouseVisualReview` uses `-HouseVisualQuality=N` (`S/HouseVisualReview.cpp:225-232`). `-HouseExteriorViews` stays at the default (1). | |

### c.2 Exposure and grade stack

- **Map PPVs** *(log `HOUSE_EXTERIOR ppv=`, printed by `S/HouseExterior.cpp:2102-2107`)*:
  - `PostProcessVolume_0`: unbound, priority 0, **AutoExposureMethod=Manual (2), bias -4.5**;
  - `PostProcessVolume_1`: unbound, priority 2, no exposure overrides.
- **`UHouseDuskAtmosphereComponent`** (`S/HouseDuskAtmosphere.h:17-45`, built from `S/HouseExterior.cpp:2161-2170`) adds
  an **unbound `UPostProcessComponent` "ExteriorDuskFilmLook" at priority 3** (`HouseDuskAtmosphere.cpp:263-287`):
  - film grain .3, saturation (.9,.92,.95), contrast .94;
  - shadows gain (.84,1,1.08) with offset (0,.007,.010) for teal;
  - highlights gain (1.08,1,.88) for warm;
  - vignette .55, fringe .6;
  - `AutoExposureBias = -4.5 + 0.35` (`-DuskEV=` changes the 0.35).
- **Blend weight:** `W = clamp(DistanceOutsideHouse(CameraLocation) / 40, 0, 1)` (`:339`).
  - `DistanceOutsideHouse` (`:233-244`) measures distance to the house's three boxes only:
    - `(-3400,-2200,-420)–(-1420,1540,1150)`;
    - `(-1420,-1010,-420)–(1360,1640,1150)`;
    - `(1360,-2130,-420)–(3340,1540,1150)`.
  - Indoors in the house, W = 0, so the house keeps exactly -4.5 with no grade.
  - In the woods (Stage ≥ 2) the bias drops a further .24 × WoodlandDepth (`:341-347`).
- **Lighting changes** (`HouseDusk::ApplyLighting`, `:119-199`):
  - The directional light becomes a 14000 lux sun 4.5° **below** the horizon with shadows off. The atmosphere's
    transmittance toward it is zero, so it lights nothing directly.
  - Sky-atmosphere tint (.80,.93,1.16).
  - Sky light `max(.6, old×2)` with realtime capture *(log: 0.15→0.60)*.
  - The height fog gets inscatter (.020,.033,.072) and a warm directional inscatter.
- **Exterior fog actor** spawned by `S/HouseExterior.cpp:2141-2156`: density .016, height falloff .08,
  **StartDistance 2600**, max opacity .97. Rooms deeper than 26 m (gym, auditorium, library hall) will haze at the far
  end.

### c.3 Does emissive light rooms here? Mostly no. The evidence:

1. **Engine.**
   - `UStaticMesh::BuildFromMeshDescriptions` with `bFastBuild` builds only the LOD render data and the ray-tracing
     representation, then calls `InitResources()`. There is no distance field and no Lumen cards
     (`/Users/UE_5.8/Engine/Source/Runtime/Engine/Private/StaticMesh.cpp:8988-9016`).
   - `ProceduralMeshComponent.cpp` has no distance-field or card code. Its proxy is dynamic-relevance only
     (`.../ProceduralMeshComponent.cpp:309,395`).
   - With global-SDF-only tracing, **every runtime-built wall and fixture is invisible to Lumen's off-screen rays**.
     Emissive contributes only through Lumen screen traces: view-dependent, and gone when the emitter is off-screen.
2. **Project flags.** Almost all exterior and campus runtime parts set `bAffectDynamicIndirectLighting=false` and
   `bAffectDistanceFieldLighting=false`. Examples:
   - `S/HouseCampusFall.cpp:215-216, 243-244`;
   - `S/HouseExterior.cpp:1544-1546`, covering ExteriorNear, Far, FraternityRow, StreetDecor and others;
   - `S/HouseClockTower.cpp:163`.

   Reviews assert these flags: `S/HouseCampusReview.cpp:59,87`, `HouseWoodsPass.cpp:81`, `HouseParkingLotReview.cpp:43,50`.
3. **Precedents chose real lights or painted light:**
   - The black hall's emissive strip fixtures (`MI_SSO_BHLightNeutral`, emission about 40) still needed real fills:
     *"Open strip fixtures also light the white ceiling in the photos; add soft fill just under each run"*
     (`S/HouseBlackHallBuild.cpp:462-478`). Eight 420 lm × 1.21 shadowless point lights with `SourceLength` 120, plus two
     650 lm × 1.21 shadowed spots (`:443-460`).
   - Clock tower: *"The dial glow and the uplight on the stone come from material emission only"*. The glow on the stone
     is painted in steps (`SourceArt/ClockTower/README.md:12,48`).
   - Nightclub: emissive lenses and additive cones *"make the sources and shafts visible"*. There are 3 real shadowless
     lights per room, active only in the room containing the local camera, plus room-local PP (exposure +.25, bloom .9,
     threshold .65) (`SourceArt/NightClub/README.md:176-192`).
   - CampusFall: *"Every glow comes from material emission or from camera-facing haze quads"*
     (`SourceArt/CampusFall/README.md:12`).
4. **Conclusion for interiors:**
   - Emissive gives you visible fixtures and windows.
   - Illumination must come from real lights: a few per active room, shadowless fills plus at most one shadowed key.
   - Or bake it into painted textures and emission (the clock-tower technique).
   - Ambient occlusion and bounce in runtime rooms are screen-space only.

### c.4 What happens today inside a campus building, and what to change

| Effect today | Why | Fix |
|---|---|---|
| Full outdoor film look: grain, teal shadows, vignette .55, fringe .6; exposure **-4.15** instead of the house's -4.5 | `DistanceOutsideHouse` knows only the house (`HouseDuskAtmosphere.cpp:233-244,339`) | Add `HouseCampusInteriors::DistanceOutside(P)` (0 inside any loaded or declared interior volume). Set the weight to `min(DistanceOutsideHouse/40, DistanceOutsideCampus/40)`. Keep the old function: the ClockTower review relies on it. |
| Clock bells not ducked indoors | `S/HouseClockTower.cpp:743` uses `DistanceOutsideHouse` | Same combined test |
| Leaf-rustle audio plays indoors | `S/HouseLeafWind.cpp:626`: "outside" = ground area plus height band | Same combined test, or exclude the interior volumes |
| **Exterior shadowless lights shine through walls into rooms** | All 53 exterior lights have `SetCastShadows(false)` (`S/HouseExterior.cpp:2005-2031`). Positions and reach are listed below. | **Lighting channels.** Interior primitives `LightingChannels = {bChannel0=false, bChannel1=true}`; interior lights channel 1 only. Façade shells, glass and the ground stay on channel 0, so interior lights don't spill outside either; the light-pool decals stand in for the spill. Sky light and Lumen ignore channels. Verify in a `detail_*` A/B. |
| Sky light leaks indoors (cool ambient fill in corners) | Runtime walls are not in the global SDF | Real fill lights; optional Lumen proxies (§c.5); room-local PP as in the nightclub |
| Haze at the far end of rooms over 26 m | Height fog StartDistance 2600 | Keep rooms under about 25 m, or accept |
| Dusk sun | Below the horizon, shadows off | Nothing lit; no action |

Shadowless exterior lights that reach the existing campus blocks, computed from code (block list at
`S/HouseExteriorCampus.cpp:121-130`; footprints are X/Y ranges):

- **Stadium floods.** 60000 lm, radius 6500, draw 22000, at about (5501,-11957,3690), (2400,-10861,3690) and
  (-701,-11957,3690). Set from `S/HouseExterior.cpp:1243-1244` and `S/HouseCampusFallLook.cpp:382-389` (lamp = mast top
  + (0,-200,-150), pulled 35% toward the field).
  - Reaches block 3 (X 7850..11350, Y -8950..-6450): 4294 cm.
  - Reaches block 11 (X 8500..11500, Y -14500..-11100): 3234 cm.
  - Reaches block 2 (X -5900..-3800, Y -7850..-6150): 5716 cm.
  - Reaches block 6 (X -10500..-6900, Y -14600..-11600): 6249 cm.
- **Street sodium spots on the campus side.** 48000 lm, radius 2400, spot cone outer 74°, poles at `(X, -5860)` for
  X = -13500..7500 step 3000, head +251 toward the road at about Z 650 (`S/HouseExterior.cpp:1101-1111,1317`). These
  touch every front-row block: 0, 1, 2 and 3.
- **Side-street spots.** Poles at `(8420, -6000/-9000/-12000/-15000)`, heads at X 8671 (`:1318`). These reach blocks 3
  and 11. The pole at (8420,-12000) is 80 cm from block 11.
- **Lot poles.** 12500 lm, radius 2000 (`:1319-1320`):
  - (-7000,-6700) has its **lamp head at (-7251,-6700) inside block 1's footprint** (X -10600..-6800, Y -9200..-6600);
  - (-2500,-7700) has its head at (-2751,-7700);
  - (-6500,-8600) has its head at (-6249,-8600).

  The layout pass should move or remove these. The exterior light total must stay at 53 or fewer, and no new outdoor
  lights may be added.

### c.5 Optional: "Lumen proxies" for runtime rooms (untested here, needs an A/B)

`UPrimitiveComponent::bAffectIndirectLightingWhileHidden` exists:
*"Controls whether the primitive should affect indirect lighting when hidden"*
(`/Users/UE_5.8/Engine/Source/Runtime/Engine/Classes/Components/PrimitiveComponent.h:556-560`).

Setup per room:
- Hidden `UStaticMeshComponent`s using a **cooked mesh that has a distance field**, for example
  `/Engine/BasicShapes/Cube`.
- Scale them to the room's walls, floor and roof slabs, at least 20 cm thick.
- Settings: `SetHiddenInGame(true)`, `bAffectDynamicIndirectLighting=true`, `bAffectIndirectLightingWhileHidden=true`,
  `bAffectDistanceFieldLighting=true`, no collision, no shadow.
- Material: an MID in the wall colour, which is used for the bounce colour.

They would occlude the sky light and give Lumen bounce. Things to check:
- non-uniform-scale distance-field quality;
- the cost of global-SDF updates when rooms stream in and out;
- `/Engine/BasicShapes` must be cooked, which needs a reference or a cook directory.

Only adopt this if a `detail_*` interior A/B shows a visible gain over plain fill lights.

### c.6 Interior light recipe (house craft)

- Use real lights only in loaded buildings, and only in the camera's room plus the adjacent ones (the nightclub pattern,
  `S/HouseNightClub.cpp:487-521`).
- Set `MaxDrawDistance` about 2500 with `MaxDistanceFadeRange` .25× yourself. `UHouseDetailSubsystem::ApplyRenderingTweaks`
  only culls lights that exist at BeginPlay (`S/HouseDetailProps.cpp:550-597`; `house.LightCullDistance`=3200,
  `:37-38`).
- Fluorescent classroom or hall:
  - 2–4 `UPointLightComponent` per room, 400–650 lm × `HouseWorldScale::Factor²` only if the room is inside the scaled
    house (campus rooms are not), with `SourceLength` 120, shadowless, colour (.88,.93,1);
  - plus 1 shadowed spot or rect key if needed.
- Incandescent dorm room: 1 point light at 600–900 lm, colour (1,.82,.62).
- Ceiling fixture meshes get emissive similar to `MI_SSO_BHLightNeutral`.
- Interior lights must **not** be owned by `AHouseExteriorDetail`. Its `Lights` (`S/HouseExterior.cpp:2032`) is what
  ClockTower, ParkingLot and WoodsPass check against 53 (`S/HouseClockTower.cpp:883-896`, `S/HouseWoodsPass.cpp:57-63,99`).
  Owning interiors with a separate actor keeps those reviews meaningful.
- **Policy question for the orchestrator:** confirm that streamed, channel-1-only lights inside a building are not
  "outdoor lights".

---------------------------------------------------------------------------------------------------------------------------

## (d) Performance tools and an interior streaming design

### d.1 What exists

- **`HwBuildStaticMesh(FHallMesh, Name, Look, CollisionBoxes)`** (`S/HouseDetail.h:122`, implementation
  `S/HouseDetailProps.cpp:308-376`):
  - a transient `UStaticMesh` with `bAllowCPUAccess` and `NeverStream`, one UV channel, binormal sign +1;
  - one material slot per FHallMesh key, looked up with `Look->Mat(Key)`;
  - `BuildFromMeshDescriptions` with `bFastBuild=true`;
  - collision = the given boxes as `AggGeom.BoxElems`, `CTF_UseSimpleAsComplex`. No trimesh cooking.
- **Caching.** `UHouseDetailSubsystem::KitMesh(Kind, Variant)` caches kit meshes by kind (`S/HouseDetailProps.cpp:~445-458`).
  The exterior keeps its meshes alive with `KeepAlive` (`S/HouseExterior.h:51`, `S/HouseExterior.cpp:1646`).
- **`Instance(...)` lambda** (`S/HouseExterior.cpp:1639-1668`): HwBuildStaticMesh + HISM, `SetCullDistances(foliage ?
  .72·End : 0, End)`, no shadows, static mobility, optional BlockAll collision.
  - The M_House_* masters have no `PerInstanceFadeAmount`, so non-foliage HISMs pop at End.
  - Only the foliage masters fade.
- **`Make` and `Local` lambdas** (`S/HouseExterior.cpp:1538-1562`) build static-mobility ProcMesh components with complex
  collision (`bUseComplexAsSimpleCollision`, `bUseAsyncCooking=false`).
  - `Local` splits a mesh into XY tiles with `HouseSpatialMeshes(M, Cell)` (`S/HouseExteriorSpatial.h:7-53`): cell 4000
    for `ExteriorNear` and 1600 otherwise. Triangles are clipped at cell borders with interpolated UVs.
  - The reason is in the comment at `:4-6`: one site-wide non-Nanite primitive invalidates VSM pages for distant walls
    whenever a nearby light updates.
  - *(log)* ExteriorNear has 59 tiles with shadows=0. The campus buildings are part of ExteriorNear (`B.Near`, filled by
    `HouseCampus::BuildCampus` at `S/HouseExterior.cpp:1240`).
- **ProcMesh cost.** The proxy is **dynamic relevance**: no cached mesh draw commands, so it is re-collected every frame
  and for every shadow view (`ProceduralMeshComponent.cpp:309,395`). Complex collision is cooked synchronously.
- **Cull distances in use:**
  - CampusFall parts via `PM->SetCullDistance`: Stands 32000, Dorms 30000, Gate and Green 24000, Lamps 22000, Walks
    14000 (`S/HouseCampusFall.cpp:205-233,260-268`);
  - exterior lights `MaxDrawDistance = L.Draw`: stadium 22000, street 6500, lot 5500. Fade .2× (`S/HouseExterior.cpp:2020-2021`);
  - house lights 3200 (`house.LightCullDistance`).
- **Motion gating by camera distance.** CampusFall: cloth rewrite only within 170 m (`HouseCampusFall.cpp:352`), smoke
  within 200 m (`:543`).
- **Nothing streams.** `AHouseExteriorDetail` is built once in `UHouseDetailSubsystem::OnWorldBeginPlay`
  (`S/HouseDetailProps.cpp:461-512`) on **every** machine, including clients and dedicated servers. It is transient with
  `bReplicates=false` (`S/HouseExterior.cpp:1361`). CampusFall and the clock tower are spawned from it
  (`S/HouseExterior.cpp:2177-2178`), also with `bReplicates=false` (`HouseCampusFall.cpp:170`, `HouseClockTower.cpp:121`).
  *(log)* `HOUSE_EXTERIOR built tris=25868157 instances=53741 lights=53 … ms=7339`;
  `CAMPUS_FALL built … tris=102422 … ms=11`.
- **Only the server spawns gameplay props:** kit props and gadgets, `if (GetNetMode() != NM_Client)`
  (`S/HouseDetailProps.cpp:501`).
- **Perf measurement:**
  - `-HouseExteriorViews -ExteriorViewFilter=<substr,…> -ExteriorViewSamples=200..600 -ExecCmds="t.MaxFPS 0"
    -RenderOffScreen -nosound -CleanViews` writes `Saved/HouseExteriorViews/frametimes.csv`, with avg, max, game and
    render ms per view (`S/HouseExterior.cpp:2362-2400`).
  - A/B harness: `scratchpad/perf_ab.sh` with `scratchpad/run_game.sh`. Use medians of five or more alternating runs:
    many campus views sit on the 8.33 ms pacing floor.
  - `S/HouseDetailSurvey.cpp:60-95` holds a CVar matrix of Lumen and VSM trims.
- **Baseline frame times** (`SourceArt/CampusFall/README.md`, median ms, homecoming on): detail_campus_quad 9.15,
  detail_campus_kiosk 8.84, detail_campus_dorms 8.38, street views 14–23. Budget interiors so a loaded building adds no
  more than about 1.5 ms in its own views and about 0 elsewhere.

### d.2 Recommended streaming design (`AHouseCampusInteriors`, a new transient actor, `bReplicates=false`)

```
Spawn: from AHouseExteriorDetail::Build next to CampusFall/ClockTower, on every net mode (dedicated server: collision only,
       no visuals/lights/decals - mirror the existing `GetNetMode() != NM_DedicatedServer` checks).
Data:  per building b: footprint FBox2D (from the new layout), door points, floors, seed, State{Unloaded, Generating,
       Creating, Loaded}, owned components, owned lights, last-wanted time.
Tick (every 0.25 s, staggered):
  interest = all pawns known to this machine (server: every PlayerController's pawn; client: every APawn with a
             PlayerState, which includes replicated remote players) + the local view location (PlayerCameraManager:
             sky-view camera and -HouseExteriorViews spots) + -CampusInteriorsAll / -CampusInteriors=<ids> overrides
  d(b) = min over interest points of the 2D distance to b's footprint
  want  if d < 3500 cm;  keep while d < 6000;  unload after d >= 6000 for 10 s       (hysteresis in space and time)
Load pipeline (deterministic, identical on every machine):
  1. Async(EAsyncExecution::ThreadPool) - generate FHallMesh data (pure CPU; same pattern as HouseCampusFall.cpp:687 and
     HouseClockTower.cpp:824).
  2. Game thread, time-sliced (≤ 3 ms/frame, one component per frame): shells/partitions/floors as HwBuildStaticMesh +
     UStaticMeshComponent (static mobility, box collision = the walls/floors/stairs you already know), furniture as
     per-building HISMs of meshes cached in a session-wide TMap<kind, UStaticMesh*> (UPROPERTY KeepAlive). Use ProcMesh only
     for one-off surfaces; if you do, set bUseAsyncCooking=true so collision cooking leaves the game thread (the load radius
     gives it time to finish before the pawn reaches the door).
  3. Lights (channel 1, shadowless, MaxDrawDistance), light-pool decals, pane swap to glass, room-local PP volumes.
Unload: DestroyComponent on everything owned; keep cached meshes, MIDs, textures.
Logging: CAMPUS_INTERIORS loaded=<ids> ms=<build> tris=<n> lights=<n>, so reviews and perf runs can read it.
```

- **Why the union of pawns.**
  - The server simulates and validates every client's movement against **its own** collision.
  - If the listen-server host is at the house while a client walks into the library, the server must still have the
    library floor and walls, or the client gets corrected or falls.
  - Clients predict their own pawn against their own collision, so each machine also needs it locally.
  - The geometry is deterministic: seeds only, no RNG shared with the exterior. Never draw from the exterior's
    `FExBuilder::Rng` (`S/HouseExterior.cpp:438`), or the tree crc changes. All machines therefore agree.
- **Interactive replicated props** (doors, carryables): the server spawns them.
  - Either keep them alive on the server permanently, which is cheap (small actors, net-dormant until touched),
  - or stream them by the same union rule and keep a server-side `TMap<PropId, State>`, so a door does not reset when a
    building unloads.
  - Clients receive them through normal relevancy.
- **Naming.** Do not put interior components on `AHouseExteriorDetail` with names starting `ExteriorNear`,
  `ExteriorTree`, `ExteriorFraternityRow` or `ExteriorStreetDecor`. Reviews iterate those prefixes and assert
  `Y < -5550`, no shadows and no Lumen (`S/HouseCampusReview.cpp:30-45,52-60`). A separate actor sidesteps all of that.
- **Lights and VSM.** Shadowless interior lights cost no VSM pages. Each shadowed movable light invalidates the pages of
  the static meshes it touches, so allow at most one per active room. Keep interior meshes at static mobility so VSM can
  cache them. The non-Nanite coarse pages are already off on Mac.
- **Lumen flags on interior meshes.** It makes no difference to the result, since runtime meshes have no distance field.
  Keep `bAffectDistanceFieldLighting=false` for consistency with the rest of the runtime parts.

---------------------------------------------------------------------------------------------------------------------------

## (e) Top-down orthographic site and floor plans, and PNG output

### e.1 How `-HouseExteriorViews` works today (`S/HouseExterior.cpp`)

- **Views.** `struct FExView { const TCHAR* Name; FVector Pos, Target; float Fov = 0.f; }` (`:120`) and
  `const FExView ExViews[]` (`:121-245`). The campus entries are `detail_campus_quad`, `detail_campus_dorms` and
  `detail_campus_kiosk` (`:243-245`).
- **Camera setup.** `TickHouseExteriorViews` (`:2279-2420`):
  - waits 6 s (`:2297`);
  - spawns an `ACameraActor` that copies the pawn's FirstPersonCamera PostProcessSettings, FOV 85 and
    `bConstrainAspectRatio=false` (`:2299-2305`);
  - hides the pawn, sets `MOVE_None` and **parks it at (0,0,12000)** (`:2306-2309`);
  - runs route probes.
- **Per view:**
  - `C->SetActorLocationAndRotation(V.Pos, (V.Target-V.Pos).Rotation())` (`:2360`), FOV;
  - warm-up `-ExteriorViewWarmup` (2–20 s) and `-ExteriorViewSamples` (45–600) frames (`:2366-2378`);
  - log `HOUSE_EXTERIOR_VIEW`, `HOUSE_EXTERIOR_FPS`;
  - `FScreenshotRequest::RequestScreenshot(<ProjectSavedDir>/HouseExteriorViews/NN_<name>.png)` (`:2395`). With
    `-UserDir=<dir>` the file lands at `<dir>/Saved/HouseExteriorViews/`;
  - `frametimes.csv` at the end, then exit (`:2323-2330`).
- `-ExteriorViewFilter=a,b` keeps views whose name contains any key (`:2314-2321`). `-CleanViews` hides the HUD.
- **Gotcha: tree RNG.** `NearView` and `Plants` add a planting no-go zone of ±800 around every view **not** named
  `nature_*` or `detail_*` (`:1265-1273`). A new view named e.g. `plan_site` would move trees and break
  `WOODS_PASS_ORIGINAL_TREES … crc=0f3b2bb7`. **Always name new views `detail_*`.**
- **Gotcha: streaming.** The pawn is parked at (0,0,12000). An interior streamer keyed only on pawns would load nothing
  for these shots. Include the view location in the interest set, or pass `-CampusInteriorsAll`.

### e.2 Code change for orthographic views

Minimal, and existing entries are unchanged:

```cpp
// S/HouseExterior.cpp:120
struct FExView { const TCHAR* Name; FVector Pos, Target; float Fov = 0.f; float OrthoWidth = 0.f; float OrthoNear = 0.f; };
// new entries (C++14 aggregate init with default member initialisers works with the existing 4-value rows)
{ TEXT("detail_plan_campus"), FVector(0, -16500, 30000), FVector(0, -16501, -60), 0.f, 41000.f },
// after SetFieldOfView at :2361
UCameraComponent* CC = C->GetCameraComponent();
if (V.OrthoWidth > 0.f)
{
    CC->SetProjectionMode(ECameraProjectionMode::Orthographic);
    CC->SetOrthoWidth(V.OrthoWidth);                 // world cm across the image (X)
    CC->SetAutoCalculateOrthoPlanes(false);          // explicit planes, as below
    CC->SetUpdateOrthoPlanes(false);
    CC->SetOrthoNearClipPlane(V.OrthoNear);          // >0 = section cut (floor plans)
    CC->SetOrthoFarClipPlane(60000.f);
    FPostProcessSettings& PP = CC->PostProcessSettings;   // camera PP blends after the volumes: kill the film look
    PP.bOverride_VignetteIntensity = true; PP.VignetteIntensity = 0.f;
    PP.bOverride_FilmGrainIntensity = true; PP.FilmGrainIntensity = 0.f;
    PP.bOverride_SceneFringeIntensity = true; PP.SceneFringeIntensity = 0.f;
    PP.bOverride_AutoExposureBias = true; PP.AutoExposureBias = PlanEV;   // e.g. -1..+1 with lighting, 0 with viewmode unlit
}
else CC->SetProjectionMode(ECameraProjectionMode::Perspective);  // and restore the copied PP if you changed it
```

- The API was checked in `/Users/UE_5.8/Engine/Source/Runtime/Engine/Classes/Camera/CameraComponent.h:66-101,226-228`:
  `OrthoWidth`, `bAutoCalculateOrthoPlanes`, `OrthoNearClipPlane`, `OrthoFarClipPlane`, `bUpdateOrthoPlanes`,
  `ProjectionMode`, and their setters.
- **Orientation.** For a camera with pitch -90, the yaw decides the axes. Yaw 0 gives image-up = +X and image-right = +Y.
  `Target = Pos + (0,-1,-big)` gives **yaw -90**:
  - image-right = **+X (east)**, image-down = **+Y**;
  - this is the editor's Top view, and it matches `master/plandraw.py`'s default `engine_view=True` (+X right, +Y down,
    campus at the top as seen from the house);
  - no rotation gives +X right with +Y up, because that is a mirror. For north-up plans, flip vertically with
    `rendering_tech_pngio.flip_v`.
- **Coverage.** At 1280×720 the image height = `OrthoWidth × 720/1280`.
  - Campus (X -20000..20000, Y -28000..-5000): `OrthoWidth 41000` centred (0,-16500) → 32 cm/px.
  - For about 8 cm/px, use 4×4 tiles of `OrthoWidth 10240` (5760 tall): X centres -15000, -5000, 5000, 15000; Y centres
    -7880, -13640, -19400, -25160. Stitch them in Python.
  - Or try `-ResX=3840 -ResY=2160` with `-RenderOffScreen`: 10.7 cm/px for the single view. Larger than 1280×720 is
    untested here.
- **Floor plans by section cut.** Camera at Z = 30000; `OrthoNear = 30000 - (FloorZ + 150)`. Everything above the cut is
  clipped. Ground floor FloorZ = -60 gives OrthoNear = 29910; upper floors use `FloorZ = -60 + k·Story`. Frame the
  building with `OrthoWidth = 1.15 × max(W, D·1280/720)`.
- **Run flags for plan captures** (separate runs from the beauty and perf shots):
  ```
  -HouseExteriorViews -ExteriorViewFilter=detail_plan -CleanViews -RenderOffScreen -nosound -UserDir=<dir>
  -ExecCmds="ShowFlag.Fog 0,ShowFlag.VolumetricFog 0,r.Lumen.DiffuseIndirect.Allow 0,r.Lumen.Reflections.Allow 0"
  ```
  - Fog: with StartDistance 2600 and a camera 300 m up, the whole frame would be fogged.
  - Lumen with ortho views is the least-tested path in UE5. Plan shots don't need GI.
  - For a clean albedo plan, also add `viewmode unlit`, or `ShowFlag.Lighting 0` if `viewmode` is rejected in `-game`.
    Verify on the first run.
  - Exterior lights are 53 shadowless sources, and from above they make bright discs; unlit avoids them.
- **No-code alternative for eye-level interior shots:** `-HouseVisualReview -HouseVisualLayout=<json>`
  (`S/HouseVisualReview.cpp:203-232,455-476`).
  - It moves the **real pawn**, so pawn-based streaming triggers.
  - JSON is in Unity-style metres: `{x: UE_Y/(100·f), y: UE_Z/(100·f), z: UE_X/(100·f)}` with `f` = 1.1. The map's
    WorldSettings carries the tag `SSOHouseScale110` (`S/HouseWorldScale.cpp:7-12`).
  - It needs `-UserDir`. It sets scalability from `-HouseVisualQuality`.
  - Not suited to aerial shots: the pawn is teleported each view.

### e.3 Writing PNGs

- **From the game (pixels on screen).** Use `FScreenshotRequest::RequestScreenshot(path, false, false)`, as everywhere in
  the reviews.
- **From C++ data**, for example dumping an FHallImage atlas or a rasterised plan: `FImageUtils::SaveImageByExtension`
  or `SaveImageAutoFormat(Filename, FImageView(const FColor*, W, H, EGammaSpace::sRGB))`
  (`/Users/UE_5.8/Engine/Source/Runtime/Engine/Public/ImageUtils.h:109,120`; FImageView constructor in
  `.../ImageCore/Public/ImageCore.h:274`).
  - `ImageCore` and `ImageWrapper` are already dependencies of Engine (`Engine.Build.cs:45,86`), so normally no
    `SeniorSendoff.Build.cs` change is needed. If the include fails, add `"ImageCore"` to `PrivateDependencyModuleNames`
    (`Source/SeniorSendoff/SeniorSendoff.Build.cs:12`).
  - Convert FLinearColor to FColor with `ToFColor(true)`.
  - Or write JSON of footprints and rooms, as `S/HouseBlackHallSurvey.cpp:144-148` writes `census.json`, and draw it in
    Python.
- **Pure Python** (python3.13; PIL and numpy are not installed):
  - Writing: `scratchpad/master/plandraw.py` (`Canvas(xmin,xmax,ymin,ymax,px_per_m, engine_view=True)`, with `rect`,
    `poly`, `line`, `circle`, `ellipse`, a 5×7 `text`, `grid` and `save`). Its PNG writer at `:136-142` is RGB 8-bit
    zlib.
  - Reading: `briefs/rendering_tech_pngio.py`. It has `read_png(path) -> (W,H,bpp,bytearray)` (8-bit gray, RGB or RGBA,
    filters 0–4, which covers UE screenshots), `write_png` and `flip_v`. Use it to stitch ortho tiles, overlay plan
    vectors on captures, crop, or flip to north-up.
  - World↔pixel for a yaw -90 ortho shot centred (cx,cy), width Wcm, image W×H:
    `px = (X-cx)/Wcm·W + W/2`, `py = (Y-cy)/Wcm·W + H/2`.

---------------------------------------------------------------------------------------------------------------------------

## Risks, in order of impact

1. **Cooking and packaging.** New material folders must be added to `DirectoriesToAlwaysCook`. ExteriorStreet already
   isn't, so packaged builds lose the road decals and smoke today.
2. **Metal shader compile failures in Custom HLSL** show up only in a real `-game` run. The result is the Default
   Material, not an error dialog. Gate on a log grep.
3. **Lighting channels plus Substrate's Blendable GBuffer plus Lumen** is untested in this project. If channels misbehave,
   the fallback is to keep interior rooms out of the 2000–6500 cm reach of exterior lights listed in §c.4, or accept the
   spill.
4. **Interior lights vs the "53 exterior lights" rule** is a policy decision, not a technical one. Keep the lights
   streamed, channel 1, owned by a separate actor, and logged.
5. **Startup time.** The exterior already takes about 7.3 s at BeginPlay. Building all interiors up front would add
   more. Streaming with hysteresis plus time-slicing avoids this. ProcMesh trimesh cooking for big shells is synchronous
   unless `bUseAsyncCooking=true`.
6. **Tree crc.** Any new ExView not named `detail_*`/`nature_*`, or any new use of `FExBuilder::Rng` or the site RNG
   before the trees are planted, changes `crc=0f3b2bb7`.
7. **Overwriting shared assets.** M_House_* already have uncommitted modifications. Create new masters; never edit these.
8. **Half-precision ProcMesh UVs** limit UV-encoded data. Keep interior-mapping UVs in [0,1] and decide style and lit
   state in C++.

## Verification checklist for the master job

- `-game` run log has no `Failed to compile Material`, `missing usage flag` or `Default Material will be used` lines.
- `HOUSE_EXTERIOR built … lights=53`. `WOODS_PASS … actualExteriorPointAndSpotLights=53` and `exteriorLights=53` in the
  ParkingLot and ClockTower reviews.
- `WOODS_PASS_ORIGINAL_TREES stage=3 big=134 small=136 distant=852 crc=0f3b2bb7`.
- These reviews pass: `-ClockTowerReview`, `-CampusWoodsReview`, `-SurroundingsPolishReview`, `-ParkingLotWalkReview`,
  `-FraternityBoundaryReview`, `-FreeRoamWalkReview` (see `review_invariants.md` in this folder).
- New `detail_campus_*` interior and exterior views plus `detail_plan_*` ortho captures. Compare inside a campus room
  with and without the indoor test, and with and without lighting channels.
- Frame-time A/B with `perf_ab.sh` (five or more alternating runs, medians): interior on vs `-NoCampusInteriors`.
- Multiplayer smoke: a listen server at the house and a client inside a loaded building. The client stays on the floor
  and is not corrected, which shows the union-of-pawns rule works.
