# Rear woodland detail

Unreal project: `/Users/Stewart/Documents/Unreal Projects/SeniorSendoff`.
The saved `Chapter01_House` map remains byte-identical to the prior campus/woods version. No Unity files were changed.

## Added

- A continuous terrain patch behind the court with seven gentle hills and three shallow eroded hollows. The hollows have solid, walkable bottoms.
- A winding trail from clear ground beside the court to a hut clearing at world X=-800, Y=13600.
- Varied tree heights, from smaller saplings to mature trees. Existing native tree hit reactions are retained; focal trees around the hut use that same system.
- 21 fallen trunks with cut ends, growth rings, branch stubs and irregular moss patches; 165 large branch piles; 470 wildflower patches; 101 stones; 2094 additional leaf patches.
- A small open log hut with pitched metal roof, porch steps, windows, a bench, table and hanging lantern. The lantern adds one small real light.

## Implementation

`HouseWoodsDetail` builds the rear scenery with deterministic placement. The fine ground mesh replaces the previous flat patch, so the hollows are actual depressions rather than overlapping meshes. The border blends to the existing site grade. Vegetation and debris fit the new ground; trees and fallen logs stay clear of the trail and hut clearing. Scatter details use shared instanced meshes and distance culling. Decorative flowers, leaf litter and little branches do not block the player. Logs, stones, ground and the hut have collision.

## Checked

- Successful Unreal build: `evidence/build8.log`.
- Actual game scene/collision: `evidence/views/WoodsDetailReview/result.txt`, including 80 ground/hut samples, recessed solid hollow bottoms, doorway capsule clearance and required prop batches.
- Player movement: `evidence/walk/WoodsDetailReview/result.txt`, eight continuous walking legs along the trail, up the steps and into the hut, without stalling or falling through.
- Native rendered views and performance: `evidence/views/HouseVisualReview/review.json`, eight views at 1280×720, quality level 2, with eight seconds of frame-time samples per view. The rear overview measured 35 FPS; close woodland/hut views measured 42–50 FPS on this Mac. The comparable prior rear overview measured 33 FPS; these are short camera measurements, not a guaranteed frame rate or multiplayer benchmark.
- Final moss/wood close-up: `evidence/polish/HouseVisualReview`.
- Protected saved house map: `evidence/map-protection.json`.
- Reopened editor map: `evidence/reopened.json`.

No multiplayer session was part of this change's tests. Geometry is generated through the existing local exterior builder; no new network interaction or player transport was introduced.
