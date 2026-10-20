# Irregular club expansion: read-only spatial analysis

Input: fresh `expansion2_context/actors.json` and its 67 binary FBX exports.
Every exported mesh's transformed vertex bounds agrees with its saved actor
bounds within 0.1 cm. The parser applies actor world transforms to FBX mesh
centimeter coordinates with the Unreal export's Y-axis sign conversion.

## Clear stepped east wing candidate

Keep the existing footprint X[-2212.51019,-1490], Y[-1509.02675,-973.5].
Add a connected east wing X[-1490,-860], Y[-1509.02675,-1030] on both floors.

Exact triangle-versus-box separating-axis checks found zero intersections with
exported non-club static geometry in both room interior heights, and zero in the
full structural envelope X[-1490,-838], Y[-1521.02675,-1008], Z[-10,640.3].
This represents about 30.18 m2 of added floor footprint per room, or 78% of the
current club footprint. It is a clearance candidate, not a certified internal
house expansion or gameplay acceptance result.

The north setback is necessary: the rectangular extension without a setback
intersects the real ground-level hallway corner at X[-1461.24,-1413.72],
Y[-977.13,-953.37], Z[-2.2,303.6], and the neighboring m1_m2 hallway wall.
The candidate east exterior wall at X=-838 leaves about 47 cm before the nearest
upper gallery wall face at X=-790.87. The proposed north outer wall at Y=-1008
leaves about 27 cm before the original hallway face at Y=-981.09.

Southward expansion across X[-2140,-860] is blocked by the protected Room102
shell and the upstairs HallEnd end-room shell. Moving only the east club walls
and changing club-generated floor/ceiling pieces avoids those source surfaces.

## Limits requiring additional evidence

Vertical rays throughout the proposed wing hit no original exported floor or
ceiling. The initial export only includes mesh actors intersecting Z[-25,660],
so meshes wholly above/below that band are absent. Empty space here is not proof
that the wing lies inside the original exterior envelope. A complete vertical
exterior/roof export is required to distinguish an internal void from an annex.

No small protected non-static prop intersects the candidate actor-bound box.
The broad AABBs of camera proxies and several ExistingHouseDoor actors overlap;
camera proxies do not represent physical occupancy. The later native door
component export allows exact porch leaf clearance using component world
transforms because those actor AABBs also encompass nonblocking helper origins.

All six exported native doors clear the candidate envelope over their complete
closed-to-open motion. `door_sweeps.py` calculates the continuous extrema of
each vertex's hinge arc and includes active moving panel collision-box corners.
It does not rely on discrete animation poses. The closest ground porch leaf
remains 108.9 cm north of the proposed shell; the upstairs leaf remains 141.5 cm
north. Evidence: `native-door-sweep-clearance.json`.

The later complete vertical export, `expansion2_envelope/actors.json`, currently
contains 156 actors and 77 FBX files. All 77 parsed mesh bounds agree with their
saved actors within 0.1 cm. The candidate full-shell prism remains free of
non-club polygons, and its vertical rays still hit no original floor, ceiling,
or roof. This supports classifying the proposal as a new club annex in empty
level space, rather than space enclosed by the original house. Existing
non-club exterior surfaces can remain untouched; the new club shell extends
the club's own exterior footprint. The exact previous house silhouette is not
preserved by claiming that this is an internal room.

Full-envelope evidence: `envelope-volume-clearance.json`,
`envelope-vertical-rays.json`, and `envelope-survey.json`.

No existing house file, source, map, editor state, or asset was modified by this
analysis. Created files are confined to `expansion2_analysis/`.
