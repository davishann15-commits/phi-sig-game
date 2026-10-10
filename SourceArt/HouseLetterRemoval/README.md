# House lettering removal — 2026-10-10

Scope: remove the Phi Sigma Kappa name and Greek letters from Chapter01_House, preserving architecture, furniture, backing boards, flag stripes, and unrelated decorations. No saved-map actors or transforms were changed.

Runtime painters updated in HouseExterior.cpp (entrance fascia), HouseBlackHallBuild.cpp (lettering above dancing dogs), and HouseDetailKit.cpp (composite photo headers).

Eleven existing mesh assets were reimported in place. These include three architectural meshes, a wooden plaque with three damaged variants, and a striped flag with three damaged variants. The flag's fracture stages were rebuilt from the clean flag to prevent lettering from reappearing on broken fragments. Existing material-slot order was validated during import.

The current nightclub neon actors contain pictures, not the lettering present in an older asset generator. They were preserved. Unrelated T emblems, dancing dogs, exit signs, room numbers, and artwork were preserved. Lobby background images and character designs were not changed.

`backup/` contains the exact original binary mesh assets, organized by their Content-relative path. `source/` contains the pre-edit FBX exports. Restore only a specifically requested asset with Unreal closed; do not restore the entire checkout or a previous map.

Reproducible tools:
- `Content/Python/audit_house_lettering.py`: read-only current-map inventory and exports.
- `clean_meshes.py`: targeted geometry cleanup and clean-flag fracture generation.
- `Content/Python/install_house_letter_removal.py`: imports and validates materials/bounds; does not save the map.
- `review.json`: in-game review cameras.

Evidence is in `evidence/`, including the original actor inventory, material/geometry inspection, exact removed-face counts, and successful asset imports.
