# Fraternity Row yard integration notes

These are **SourceArt integration notes**, reconciled against the current saved `Source/SeniorSendoff/HouseFraternityRow.cpp`, `HouseFraternityRowYards.cpp` and `HouseExterior.cpp`. The parent has integrated the source copies. This documentation pass records source geometry and wiring; it does not establish a native compile, collision walk, visual or performance verdict. No Source/Draft code, engine job, map or commit was changed by this pass. Current native acceptance remains tracked in the parent validation reports.

## Integration API and ownership

The shared exterior calls the integrated module in this order:

```cpp
HouseFraternityRowYards::CreateLook(Look);
HouseFraternityRowYards::Build(RowSolid, RowDecor, RowGlow);
// After the identity/world-origin row actor has a root component:
HouseFraternityRowYards::AttachInstances(RowActor, Look);
```

`Build` and `AttachInstances` independently check `HouseFraternityRow::Enabled()`. Preserve legacy mode as a complete A/B restore. `HouseFraternityRow::Houses()` supplies the five permanent shell envelopes for scatter exclusion. Geometry is in **world centimetres**; root integration must attach the rendered/collision components at identity, as the existing exterior does. ISM additions explicitly use world transforms. A transformed parent must still preserve these world transforms.

Render/collide `RowSolid` using the project's static-render plus hidden procedural-collision pattern; houses, sheds, substantial furniture and fences must block players. `RowDecor` and `RowGlow` must never collide. Root must disable cast shadows, dynamic indirect lighting, distance-field lighting and navigation on dressing components, as on the retained exterior. The yard module adds **zero real lights**. Root owns the six new row lights and exterior-count review changes.

`AttachInstances` creates exactly **eight possible plain `UInstancedStaticMeshComponent` batches**, using `HouseCampusStaticMesh::Build` from FHallMesh prototypes. No HISM. Components are retained by `Owner->AddInstanceComponent`; their static-mesh properties retain the transient meshes. Batches have no collision, overlaps, nav effect, shadows, indirect lighting or decals; cull5500cm (saplings9000cm). Dedicated servers omit them. A component-name guard prevents duplicate calls after a successful cup batch. Components and geometry have `FraternityRow*` names; root must add explicit checks for this category rather than weakening the legacy `ExteriorFraternityRow` invariant.

## Authored content

| Yard | Particulars |
|---|---|
| R1 dark | Three neglected raised beds, rusted empty swing frame with chains/seats, two folded/fallen chairs, potting bench/pots, overgrown rear route and garden shelter. No party litter. |
| R2 tidy | Segmented stone patio, plank picnic table with splayed trestles, grill/lid/grate, orderly three-bed garden, hose reel/tool chest, three raked leaf piles, painted tool shed. No party litter. |
| R3 wrecked | 9x16m sand volleyball area, sagging open net, gravel pad and unbranded late1970s coupe, garage/workbench, dragged upholstered couch, fallen chair, mattress, wire shopping cart, folding table/pong cups, iced keg, trash bags, smashed pumpkin shells/stem, muddy shortcuts, half-dead string bulbs and original hand-painted `BEAT DUNMORE` bedsheet. The opponent name follows the existing fictional campus. |
| R4 moderate | Stone picnic patio, two chairs, grill, iced keg, askew cooler lid, half-dead string bulbs, lean-to with stacked wood/tools. Separate picnic table in released-lot lawn. |
| R5 interrupted | Two abandoned food tables with plates, food and napkins; unbranded cassette stereo stack on a supported wood equipment table, speakers and unplugged cable on the actual front porch; lone fallen chair, cold ash/charred logs in a stone ring, two log benches, half-dead bulbs and small shed with stored crates. |

Every rear path reaches Y4400 beside the unchanged Y4520 woods fence, with the shared access continuing through the existing gate atX-7050. The retained western hedge is not rebuilt or moved. Ground wear follows the unchanged access/commons waypoints; samples are<=75cm apart, noncolliding and only0.25cm above current ground. TudorApproach runs (-5510,-2800)->(-7600,-2800)->(-7600,122). The Queen Anne branch runs (-6380,-2800)->(-6380,-2020), sharing the Tudor's existing Y-2800 leg without drawing it twice. R5's rear path now starts (-6300,300)->(-6300,700)->(-5500,900)->(-5500,2850)->(-5700,3750)->(-5550,4400), clear of the original maple. The main-house connector's concrete is preserved: new wear stops atY-300 before its Y-285..-105 slab. Replacement lawn scatter covers only the released west-lot footprint and avoids its retained walking coordinates/table reservations. The separate commons picnic table is at(-8500,-850); the forecourt food table, food and cups moved together to(-5950,-2450).

Four dividers stand at X-15750,-12000,-8500,-6700: low picket, actual colliding chain-link wire lattice, split rail, and broken hedge with rail sections. **No opaque/invisible chain-link barrier is used.** Gateposts sit5cm inside the closed sections so their4.5cm half-width does not eat into a gap. The last divider's openings are Y1050..2150 and Y3000..3920; the wider rear opening clears the unchanged native diagonal walk. Its foliage grows only west of the rails (centresX-6742,radius36cm), leaving the shared Access path clear. There are still four dividers and eight openings.

All five outbuildings use the exact PLAN.json rectangles. Three front openings are150cm wide/215cm high; the garage is290cm wide/215cm high; the lean-to is open across its front. Door leaves are visibly parked fully open against the facade, solid and **static**, with no extra interactable door actor. Each has a back-wall workbench/storage and a clear approach/central floor. Root's house-door system remains responsible for the five house entrances/room doors. Native shed entry/exit sweeps are still required.

The shed floor/shell/doors/furniture all use one local datum, `Z=max(GroundHeight at all four shed corners)`. Door bottoms are Z+0.5cm on the same floorZ+0.5cm; leaves are210cm high below215cm lintels. The shed helper no longer samples a separate local grade for its open doors or bench/box contacts. This was corrected after adversarial review identified a potential mismatch.

## Instance/content bounds

One deterministic `FRandomStream(19861025)` is independent of all base site/vehicle/tree RNG. Original trees are read only as exclusion coordinates, never moved, hidden or regenerated. Sampling excludes house footprints, all five outbuildings, stair reservations, furnishings, divider gaps, archived roots, exact fixed field probes and native/retained walking routes. Rear grass grows from sparse/mown to taller tuft/bramble/sapling clusters toward the fence. The two quiet yards get leaves/grass without party cups/cans/paper.

Authoring counts:5 outbuildings,4 mixed dividers,6 raised beds,1 swing set,3 picnic tables,3 folding tables,1 narrow wood stereo equipment table,2 grills,2 iced kegs,7 chairs,1 couch,1 mattress,1 coupe,1 cart,1 fire ring,1 stereo stack,3 strings/46 bulbs (23 emissive/23 dead),1 original1024x256 sheet texture. Shelf/workbench contents and detailed planks/trestles, tires/hubcaps, cooler, grill grate, net, pumpkin and food use original runtime geometry.

R5 now occupies **X-7080..-5680,Y-1700..-200,floorZ30**. Its porch front isF-150=-1850 and its west return spans X-7230..-7080,Y-1850..-1150. The six-riser front stair has five28cm treads, startingY-1990; its footprint is X-6580..-6180,Y-1990..-1850. The rear door remains atX-6380, while `RearStairX` centres the wide rear stair atX-6300: X-6500..-6100,Y-200..208, plus the3cm top seam inside the body. The rear dressing reservation extends toY300. R4 retains its original new-house position X-8400..-6800,Y450..1850 and floorZ-50; its rear path reservation remainsY1850..2350, with built landing endingY2258.

The supported R5 stereo is now mirrored to the **east porch at centre(R-230,F-75)=(-5910,-1775), fixed floorZ30**, freeing the west return. `Stereo` accepts an explicit floorZ and uses that same datum for every speaker, rack board/leg/stretcher, deck, faceplate and cable vertex; it never samples terrain. The narrow wood table is98x58cm, four legs rise72cm, the3cm board ends75cm above the porch, and the deck bottom is exactly75cm. In world Z the legs end102, the board spans102..105, and the deck starts105 and ends151. Both speaker cabinets rest on worldZ30. The1.2cm cable coil has its centre0.6cm above the porch so its underside meets the floor.

Its full world XY envelope is **X-6019..-5801,Y-1805..-1745** (centre±109cm/±30cm), including speakers, rack and cable. It leaves41cm from the left adjacent post's right faceX-6060 and49cm from the right adjacent post's left faceX-5752. The front opening's right edgeX-6280 is261cm away (243cm from the outer door-frame trim atX-6262), and the front stair's right edgeX-6180 is161cm away. The envelope remains45cm inside the porch's south edgeY-1850 and45cm before the house wallY-1700. The whole porch is excluded from the independent ground grass/litter scatter; the food table's reservation follows its moved forecourt position. These are source dimensions/contact checks; native contact/appearance and porch walking remain mandatory.

Scatter is **bounded**, not a runtime measured count: at most203 cups,98 cans,105 paper pieces,896 individual leaves,1824 tuft instances,135 bramble clusters and40 saplings (3301 total upper bound before exclusions). Many proposals are rejected by clearance/density. The native logs `FRAT_ROW_YARDS_ISM` report the actual accepted count/prototype triangle count per batch. `FRAT_ROW_YARDS` reports this module's triangle deltas,5 yards/5 outbuildings/4 dividers,0 new lights and0 terrain/tree edits. Do not present these logs/counts as observed until a native run exists.

## Source-only validation receipt

On18 October2026, the initial Python geometry check compared49 nominal conservative solid envelopes (planned sheds, divider closed segments and furniture bounds) with the12 fixed WestField probes and the unchanged seven-point native walk. Route segments were sampled every<=2cm. No envelope entered the120cm audit buffers. Closest nominal probe clearance:243.66cm at the R5 eastern log; closest nominal fixed-route clearance:126.85cm at the fallen R5 chair. The later stereo correction has its separate exact porch-support/envelope check above; do not treat the initial49-envelope receipt as a native check of the revised stereo. This is source planning, not a physics sweep or an exhaustive cooked-mesh proof. Audit the fallen chair and corrected porch gear in the native walkthrough. Sampled noncolliding scatter uses the same120cm minimum probe/native-route buffer plus each prototype radius.

The subsequent relocation/divider audit used the final X-6700 divider and Y3000..3920 rear opening. Its closed solid bounds stayed at least250.384cm from the fixed native route and295.686cm from the fixed probes. West-only hedge bounds stayed at least217.024cm from that route and222.109cm from the probes, after conservatively subtracting their36cm radius. Across the whole shared Access path, minimum separation was100cm to divider solids and76.5cm to foliage; the145.5cm post clearance applies only to the vertical X-6550 segment. The whole-route figure leaves32.5cm beyond a32cm capsule radius plus12cm stop allowance. The moved picnic envelope clears Access by276cm; the food table clears Commons by320cm. The relocated R5 roof keeps120cm to Commons (body170cm), and the rear path keeps715.25cm to the archived maple root. These are source-derived bounds, not native sweeps. The final east-porch stereo bounds/contact dimensions are recorded separately above.

These source receipts are deliberately limited: they establish no UE compilation, ground support/collision, current tree CRC, camera appearance, light count, walk-through pass or render-thread cost. The parent acceptance process still requires the applicable native reviews, all house/yard/shed walks, dusk/night views and standard three alternating A/B performance pairs, with results recorded separately. Existing woods-fence/terrain/road/story/main-house/court/FallJam geometry is untouched. No Fab asset is required.

House facade/roof litter, original-tree toilet-paper ribbons, porch details/house numbers/mailboxes, interior doors and pooled lights belong to the other Row modules. Do not duplicate them here. No animated fire/smoke/Niagara, external sound, terrain paint, nav changes or additional light components are introduced.


## Existing vegetation clearance

`ExistingVegetationBlocked(FBox2D)` clears only rendered grass/shrub mesh bounds that overlap R4/R5 bodies, front/return porches, the front stairs, the actual rear stair extent (B+408), or any of the five sheds including the15cm open-door/hinge projection. R5 uses its descriptor-relative six-riser front footprint and `RearStairX` offset; R4 retains its existing footprint. Original-three-house gardens, paths, commons and broad yard vegetation are excluded. Legacy mode always returns false.

The parent shared Instance integration uses each built mesh bounding box transformed by the original world transform. It skips AddInstance only for Grass/Bush names after original scatter and variant selection; the input arrays, RNG draws, order, trees and leaves stay unchanged. Counters use the actual retained instance count. This wiring is present in saved source; its native verification status is recorded in the parent reports.
