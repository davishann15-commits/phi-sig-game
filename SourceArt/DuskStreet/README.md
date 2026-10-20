# Dusk street + Halloween party (Claude, Oct 12)

Jacob asked for a more realistic road and a 1986 frat house at dusk, seen from the street. The scene has boxy cars out front (a woody wagon and a square-bodied sedan), kegs, red cups, aluminum lawn chairs, a smoking grill, a payphone and coin newspaper box on the corner, sodium streetlights, a hand-painted banner (made a Halloween banner on request), and a warm, faded film look with teal shadows and orange highlights.

## Files
- `Source/SeniorSendoff/HouseStreetScene.h/.cpp` (new): the road, poles and wires, party props, banner, payphone corner, painted textures and decal layout. Everything is painted in code; no real brands or logos.
- `Source/SeniorSendoff/HouseDuskAtmosphere.h/.cpp` (new): dusk lighting (the sun below the horizon drives the sky only), outside-only film look, grill smoke, decal spawning.
- `Source/SeniorSendoff/HouseExterior.cpp`:
  - hooks for the above
  - wooden street poles with sodium spot lights (no new lights; still 53)
  - near-kerb cars at `HouseStreetScene::KerbCarX`
  - attic liner under the roofs, which hides ceiling hairline cracks that showed the dusk sky
  - new `detail_` views
  - Backup of the old file: `Saved/StreetBackup/`.
- `Tools/claude/build_street_materials.py` makes `Content/SeniorSendOff/House/ExteriorStreet/M_StreetDecal` and `M_StreetSmoke`. It runs once, from `Saved/RunStreet.command`.

## Switches
`-NoDusk`, `-DuskSun=`, `-DuskBelow=`, `-DuskYaw=`, `-DuskSkyTint=r,g,b`, `-DuskSky=`, `-DuskSkyMin=`, `-DuskGlow=`, `-DuskEV=`, `-DuskSodium=`, `-CleanViews`.

## Verification
- Build passed (Mac, UE 5.8). Shots are in `Saved/StreetViews` (hero: `54_detail_establishing_high.png`).
- Codex's checks with an actual walk all pass: SURROUNDINGS_POLISH, FRATERNITY_BOUNDARY, CAMPUS_WOODS, PARKING_LOT (lights 53, walk PASS).
- `-HouseExteriorWalkReview` fails on two checks that were already out of date before this pass:
  - the sidewalk material name after Codex's surroundings polish
  - the `ExteriorWalks` component name, now that the walks mesh is tiled
  - With `-NoSurroundingsPolish` it passes everything up to the tile-name check.
- Rooms are unchanged: dusk vs `-NoDusk` mean luminance is within 0.4 in four room views.
- Saved house map not modified.
