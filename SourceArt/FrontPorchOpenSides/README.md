# Open front porch sides

Requested on 2026-10-10: remove the tall brick side walls shown flanking the front porch.

Implementation:
- Removed four generated brick wall/skin boxes from `FExBuilder::BuildPortico` in `HouseExterior.cpp`.
- Removed the separate long brick panel island from each of `SM_HFP_Upper_m1_m1_Solid` and `SM_HFP_Upper_m1_p0_Solid`: 188 faces per asset.
- Kept the original face instances and material slots for retained geometry, including the balcony side railings, floor, columns, roof, and rear wall.
- Original asset packages are retained in `backup/`; exported originals and cleaned FBX files are retained here.
- The campaign map was not resaved or replaced.

Verification: editor target compiled successfully; three fixed gameplay views are defined in `review.json`. The final render sweep and screenshots are under `final/Saved/HouseVisualReview/`. This camera sweep does not claim a manual traversal or multiplayer test.
