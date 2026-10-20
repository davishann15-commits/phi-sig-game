# Porte-cochère rebuild + black roofs — Oct 12, 2026

Jacob sent three photos of the real porte-cochère (the covered side entrance at the east end of the house) and asked for that area to be redone to match, plus a black roof on the house. Built at runtime like everything else; the map file is untouched. No real logos: the sign is a generic fire-department-connection sign.

## Porte-cochère (`Source/SeniorSendoff/HouseSideCanopy.h/.cpp`, new)
- Attached to the east wall (x 3340) over the side door (y -1143..-980). Footprint x 3340..4150, y -1500..-620, roof overhang 50. The drive still passes under it to the rear lot.
- **Roof**: hip, ridge running out from the wall, black shingle, dark metal ridge and hip caps, flashing where it meets the wall. Eave at z 436, ridge at 573 (below the house cornice).
- **Entablature**: white ring beam on the three free sides, z 330–390, with raised rails and stiles framing four recessed panels on every face, inside and out; two-step cornice; flat white ceiling; white soffit and fascia at the eaves.
- **Columns**: four white Tuscan columns in two pairs at the east corners (centres x 4122, y -1470/-1380 and -650/-740; 90 cm apart in each pair), shaft radius 24→21, torus base, neck rings, echinus and abacus. Each pair stands on a brick plinth with a stone cap (top z 30); a brick knee wall with a stone cap (top z 0) runs between the plinths along the east side. The existing bushes at x 4260 sit outside it, as in the photo.
- **Stoop**: brick with concrete treads; three 18 cm risers on the drive side and on both ends, so Codex's side path along the wall (x 3398) still walks straight through. White railings (two posts, top and mid rail) flank the drive-side run.
- **At the door**: black wall lanterns either side (bright emissive glass, no lights), black mailbox, FDC sign, entry keypad. Gas meter on the wall north of the door with three yellow bollards in front (x 3466, clear of the side path).
- **Hanging lantern** under the ceiling, carrying the old ceiling point light (2,500 lm). Light count unchanged: 53.
- Concrete pad under the whole canopy, 2 cm above the drive.
- The old flat-roofed brick-parapet version and its square piers are gone.

## Black roofs
- New material `EX_ShingleBlack` (`ExPaintShingle` now takes base/range/grain/blue parameters). `FExBuilder::Shingle` points at it, so the three hip roofs, the portico pediment roof and the canopy are black. The neighbouring houses (`HouseFraternityRow`) keep the grey `EX_Shingle`.

## Views and checks
- New views: `detail_canopy_drive`, `detail_canopy_lawn`, `detail_canopy_door`, `detail_canopy_corner`, `detail_canopy_roofs` (all `detail_`, so tree planting is unchanged).
- `ExProbeAll` gains an `east_side_path` route (x 3398, y -2200→-350): gaps 0, blocks 0, max step 33 (sampled every 50 cm, so it skips a tread; the real risers are 18).
- Codex's checks with an actual walk all pass: SURROUNDINGS_POLISH, FRATERNITY_BOUNDARY, CAMPUS_WOODS, PARKING_LOT (lights 53, walk PASS).
- `-HouseExteriorWalkReview` fails only where it did before (sidewalk material name under Codex's surroundings polish; `ExteriorWalks` tile name). With `-NoSurroundingsPolish` it passes everything up to the tile-name check, including the right path and the side paths.
- `Saved/RunStreet.command` default views now: the canopy views, the establishing shots, `front_street`, `right_end`, and the four rooms. `STREET_REVIEWS=1` and `STREET_AB=1` turn the reviews and the no-dusk A/B back on.
- Canopy mesh: 7.8k triangles into `ExteriorHouse`.
- Backup of `HouseExterior.cpp` before this pass: the previous dusk version is in `Saved/StreetBackup/` lineage (the original is `Saved/StreetBackup/HouseExterior.cpp`).

## Notes
- The real porte-cochère has its drive parallel to the street; ours runs through to the rear lot. The orientation stayed because the drive, the lot access and Codex's routes depend on it; the architecture is what was matched.
- Killing the editor while a Play-In-Editor session is running makes the crash reporter pop up. Harmless, but stop PIE before running the script.
