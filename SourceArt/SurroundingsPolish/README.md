# Surroundings detail pass

Unreal project: `SeniorSendoff.uproject`
Map: `/Game/Story/Maps/Chapter01_House`

## Added

- Fraternity houses: layered porch bases/capitals, balcony returns, soffit brackets, deeper window trim, bay windows, flower boxes, transoms, shutters, gutters, downpipes, chimney caps, roof/siding surface detail and natural shrubs.
- Campus buildings: deeper windows, entrance hardware/signs, mechanical roof screens/fans/vents, parapet coping and downpipes. Stadium: arched gates, benches, aisle rails and scoreboard lettering.
- Streets: fine asphalt/concrete texture and normal maps, repairs, hairline cracks, joints, iron drainage grates, service covers, crossing, stop/street signs, shuttle shelter, benches, bins and bike stands.
- Shared car models: transparent tinted glazing, seats/headrests, dashboard/console, steering wheels, lens ribs, fuel flap, calipers, sill dust and fictional Tennessee plates.

## Scope

Everything is built at runtime in the Unreal project. The saved house map is preserved; Unity was not edited. Parking locations and the two collision boxes on each of the three shared car models are unchanged. New street decoration has no collision, shadows or indirect-light updates, and adds no real lights. Existing property boundaries, court, woods and shack remain intact.

## Verification

- Mac Development editor build passed.
- Native campaign render review plus actual road-floor, court-floor, woods and property-boundary collision probes passed.
- Benchmark: 1280 x 720, quality 2, uncapped frame rate, same three cameras, eight-second samples after warmup.

| View | Before FPS | After FPS | Change |
|---|---:|---:|---:|
| Fraternity road | 49.1 | 47.4 | -3.6% |
| Campus buildings | 49.2 | 46.5 | -5.5% |
| Parking/cars | 50.2 | 48.3 | -3.8% |

These are local camera measurements, not a multiplayer or manual traversal test. Reports are in `evidence/verification.json`; final native captures are in `final/`.

For a diagnostic comparison, `-NoSurroundingsPolish` bypasses this pass without reverting the existing house/campus/woods work.
