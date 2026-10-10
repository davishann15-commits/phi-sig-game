# Pig animal motion refinement

The approved Pepper and Coal models, sizes, health, room, mattress, food and house map are preserved.

Walking now has gentle side-to-side weight transfer, individual continuous stride and pace variation, softer acceleration and turns, and restrained head counter-motion. Hooves remain planted in world space with their existing anatomical IK. Route starts choose an initially clear floor join around sleeping companions, and speed eases near intermediate turns so tight stair waypoints remain reachable.

Both pigs autonomously stop on safe flat floor for short nose-down sniff bouts, then resume their saved route. Doorways, stairs, feeding, sleep and frightened escape journeys take precedence. Newly requested journeys get individually staggered curiosity timing.

The original approved ear geometry is partitioned from the head with all 19,500 triangles preserved. Separate ear hinges use independently damped bounce and lag during running, with slightly livelier motion on Pepper.

Short spatial snorts use JarredGibb's Pig - Snort 6 field recording, available under CC0: https://freesound.org/people/JarredGibb/sounds/233165/ . The source high-quality preview is converted to mono PCM, with gentle individual pitch and volume variation and room occlusion. Source license and conversion details are in Audio/source-license.json.

Controls remain E near a rear ankle to lift by one back leg and E to lower onto supported clear floor.

Validation contains runtime captures and reports for autonomous sniffing, active audio, ear travel and walking sway, plus the full planted-hoof/carry regression and physical stairs traversal. The saved house map hash is unchanged.
