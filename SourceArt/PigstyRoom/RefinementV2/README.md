# Upstairs pigsty

Converted only HallEnd_West from the poker room. Expanded its north boundary 120 cm, increasing floor area 24.65%. The initial conversion retained all 677 unrelated actors and all door registry entries. Both finished upstairs bedrooms are preserved.

## Current pig features

Two detailed black pigs have independent wandering and reactions to the player. Sizes are relative to the original full-size pig; speeds are in Unreal centimetres per second.

| Pig | Size | Maximum health | Walk speed | Run speed |
| --- | --- | --- | --- | --- |
| Pepper, the smaller pig | 62% | 85 HP | 155 cm/s | 340 cm/s |
| Coal, the larger pig | 82% | 160 HP | 130 cm/s | 280 cm/s |

Pigs can leave only after the player opens the pigsty door. They wait for other closed gates. The existing hat and sauce damage paths recognize their animated hitbox. Health labels show each visible pig's own name and HP.

Aim at a living pig and press E to pick it up. Carry it back to the room, look at a clear floor and press E to place it down. Deliberately aiming at a door while carrying a pig lets E open or close that door. Pickup validates range, aim and visibility; carrying uses body sweeps, and placement checks collision and floor support. AI pauses while the animal is held. The HUD shows pickup, placement and carried-state prompts.

The stained mattress is directly on the floor. Each pig has a separate sleep place, approaches the mattress physically, then lies on its side with slow breathing for 20 seconds. Damage, pickup, a nearby player or the end of its sleep period wakes it.

The edible snack pile starts with 24 Cheetos, visibly loses pieces as pigs eat and heals a living pig 2 HP per piece. It replenishes 4 pieces every 5 seconds, capped at 24, only while at least one living, unheld pig is physically inside the pigsty. Refills pause while both pigs are out or held, without accumulating missed refills. Food resets on a new level session.

The room contains stained concrete, damp plaster, a worn blanket, straw, rusty water trough, feed bucket, broken boards, vent, warm caged practical light and distressed gate plaques. The Cheetos bag is labeled on both sides. All art uses centimetres and authored normals.

## Prior pigsty validation

These results describe the original pigsty release, before the size, art, pickup, sleep and replenishment upgrade.

- Native feeding, finite visible food, separate health/healing/HUD, closed-gate containment, real player E opening, independent escape and hall motion:passed.
- Both pigs physically descended the staircase and reached the great room after real player E opened the intervening gates:passed. Zero unsafe floor stops.
- Existing door open/close/reopen and pawn passage:passed.
- Native four-angle visual review:passed. Grounded leg animation, orange snacks and readable bag/sign labels inspected.

- Final walking profile with both pigs active:896frames over15.01seconds, p50 16.67ms, p95 16.79ms, p99 16.92ms, zero stalls and46.87m of physical travel. The test follower physically detoured around pigs with all collisions retained. Production player and pig collisions are unchanged.

## Current upgrade validation

- Current saved sizes, capsules, hitboxes, speeds and separate maximum health: passed in the rendered game.
- Both pigs physically approached separate mattress places, climbed onto the repaired collider, lay down and breathed. Damage woke only the hit pig: passed.
- Actual player E pickup and placement, and W carrying, passed for both pigs. AI paused while held and resumed feeding and sleeping after placement.
- Pickup through the closed door or beyond interaction range was rejected. Both pigs escaped physically only after the player opened their door.
- Visible Cheetos matched the remaining food, replenished while occupied and paused while both pigs were outside.
- An escaped Coal was picked up in the upstairs hall, physically carried through the open doorway and placed safely back inside. Full body collision and floor support passed.
- Both resized pigs physically descended the stairs and reached the great room after real E opening of the intervening doors: passed.
- Four native camera views of the imported art: passed and inspected.
- Walking profile with both refined pigs active: 900 frames over 15 seconds, p50 16.67 ms, p95 16.72 ms, p99 16.76 ms, worst 17.06 ms, zero stalls over 49.72 m of physical travel. Tested at the preserved 1280 by 720, 80% resolution and 60 FPS profile.

The refinement retained 708 unrelated map actors and the exact door registry. The existing hidden mattress box asset received working collision. RefinementV2/Validation contains the native logs, screenshots and result report; Source contains the exact gameplay source snapshots and installation tools. Test setup changes are isolated from the saved map and normal player settings.

## Portable source archive

Import contains the exact asset files used for the initial room conversion. Source contains builders, import/apply tools, native camera/route inputs and gameplay source snapshots. Source paths in manifests point to the authoring workspace; use the local Import files when relocating this archive.

RefinementV2 contains the current pig art archive, including the portable Blender source with packed textures, FBX meshes, texture files, build tools, previews and audits. Use its local Art files for the updated pig appearance when relocating the project.
