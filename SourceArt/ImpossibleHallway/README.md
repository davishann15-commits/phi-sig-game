# Impossible hallway

The HallEnd_End door retains its original hinges, leaf, handle, frame, collision,
and entry in the house door registry. The room behind it is visually replaced
with a narrow, dark passage centered at X -2177 and extending in world -Y from
the door plane Y -1386.2425. Floor height remains 302.3 cm.

## Authored art

The corridor contains 18 translated copies of one 180 cm module, for a 32.4 m
visible length. It uses worn desaturated olive plaster, a damp ceiling, dark
floorboards, beveled skirting and cornices, sealed side-door details, and warm,
weak black caged sconces. Textures and geometry repeat at exactly the same
180 cm period as the runtime loop. The 80 cm entrance throat widens from the
existing 163.8 cm doorway to the 220 cm corridor.

Nine deterministic 1024 px textures provide color, roughness, and normal detail.
The surfaces, trim, lamps, throat, and hidden collision are five combined mesh
actors. The new geometry has 45,672 triangles, including the 72 triangle hidden
collider. A plain black cube caps the far endpoint. Six unshadowed point lights
have a bounded 140 cm attenuation radius. These lights and only the new corridor
meshes use private lighting channel 1, preventing light leaking from nearby
rooms and keeping the repeating illumination consistent.
No global rendering settings or existing room lights are modified, except the
retired original end-room practical light which has its intensity set to zero.

The existing end-room floor supports the threshold and loop area. New physical
walls, ceiling, and floor continue through all 18 visible modules, so carried or
thrown props remain inside the passage. New floor collision begins at depth
435 cm, overlapping the existing floor by 18.8 cm without changing its height.

## Surgical changes

The combined shared shell and collision mesh each have only the six surfaces
(12 triangles) of the old end-room far wall removed. The corresponding far
skirting cube alone is removed from the shared trim, and the original end-room
ceiling fixture alone is removed from the shared practicals. Every retained
vertex, face material, UV, and normal is copied. The pigsty, East room,
finished bedrooms, doorway caps, and house hallway are retained.

## Sources and validation

Art contains the portable FBXs, textures, and Blender source. Source contains
the procedural texture builder, geometry builder, independent FBX audit, and
Unreal import/apply script. Runtime gameplay is implemented in the project's
HouseImpossibleHallway class and validated separately by the root task.

The independent FBX audit confirms the bounds of all nine meshes. Every one of
the 18 surface, trim, and fixture modules matches the prototype in material,
UV, and translated position, with less than 0.001 cm serialization error.
The geometry audit records exact source hashes and owned face removals.

The import/apply script checks the imported bounds, all unrelated actors, and
the exact original End door and registry hash before saving the map. It imports
the root-authored pipe drone as a looping SoundWave, then places the runtime
actor at the door with the 180 cm loop period and 360 cm forward loop depth.

## Runtime behavior and playtest

Open the existing end door beside the upstairs pigsty with E or the game's
normal double-click interaction. Walking into the passage loops stock player
movement every 180 cm. When the player looks back, the actual house doorway
returns immediately behind them. Players can walk out, reenter, and close the
original door. The local looping pipe drone adds a quiet mechanical hum.

The final native Chapter01_House playtest passed: 27 walking loops, 7 sprint
loops, 3 crouch loops, 4 jumping loops, grounded movement, side-wall blocking,
backward and forward exit, reentry, and native E open/close. The turned-back
entrance was 263.7 cm from the camera after more than 50 m of virtual walking.
An animated pig lift survived 3 loops and a turn-back, then normal floor
placement. A tagged simulated furniture body survived 4 loops and a turn-back
while aimed diagonally down for the narrow corridor's existing wall clearance
rule, then normal release onto the real floor. Actor and component counts
stayed bounded. Validation/result.json and the two native screenshots preserve
the evidence. The original furniture rule still releases a hold if the player
aims it too close to a wall.

The final map is saved; runtime checks use an isolated temporary save directory
and do not alter the normal campaign save.
