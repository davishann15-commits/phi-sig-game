# Interactive autumn trees

Unreal project: `SeniorSendoff.uproject` / `Chapter01_House`.

## Changes

- Mature oaks with thick trunks, buttress roots, crooked spreading limbs and lobed gold/copper leaves.
- Slim pale-barked birches with drooping branches and small golden leaves, mixed with the existing maples.
- Four focal trees on the front lawn, outside the concrete approach and side paths. Existing site planting and road/house dimensions are retained.
- Weapon hits and fast thrown physics objects knock leaves from the struck tree. Walking into a trunk does not trigger shedding or count as breaking the house.
- Leaves originate in that tree's crown, tumble in the existing wind, settle on traced surfaces and fade away. Leaf rustling becomes briefly stronger near a hit.
- One server-owned relay shares accepted hits with clients. Leaf movement is cosmetic and simulated locally from the event seed.

## Runtime limits

- Eight populated species/shape batches: 67 maples, 81 oaks and 80 birches.
- Fixed 216 falling-leaf slots across species; repeat impacts reuse the slots. The existing 96 ground-wind leaf slots remain.
- No leaf actors, rigid bodies or extra real lights. Surface traces are limited to 12 per frame for falling leaves.
- Exterior real-light count remains 52. Runtime geometry audit reports 6,880,491 counted triangles and 29,867 instances.
- Tree trunks keep blocking collision; foliage does not block players.

## Verification

- Development editor build succeeded (`build5.log`).
- Native game review passed actual weapon throws against all three species and an actual basketball physics impact against an oak.
- Eleven accepted impacts produced eleven local shed events; 96 leaves settled; peak active falling leaves was 76 of the 216-slot limit.
- House-damage count stayed unchanged during tree hits. Screenshots and result: `evidence/single/`.
- Two real game processes passed normal lobby travel, an owning-client weapon throw and a host physics-object impact. Each peer received both events, shed locally, and settled 11 leaves before its screenshot. Both exited successfully (`multiplayer-verification.json`, `evidence/host/`, `evidence/client/`).
- All seven exterior routes passed with normal character movement after the new trees were added: front approach/steps/landing, both side paths, lawn, road/driveway connections, and E interaction plus passage through both front doors (`evidence/walk/`).
- Protected map, exterior paths/layout, house bricks, cars, court and Billy Bass hashes remain unchanged (`protected-after.json`).

The native reviews are opt-in development flags only (`HouseTreesReview` and `HouseTreesNetworkReview`). The multiplayer runner uses the normal lobby with distinct characters and no production join-policy changes.

These tree changes are local source changes made after the October 10 GitHub/ZIP handoff snapshot.
