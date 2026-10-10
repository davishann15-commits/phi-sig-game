# Pig movement and single back leg pickup

MotionV3 preserves the approved pig art, sizes, health, room, mattress, food and door layout.

The pigs use a four beat walk blending into a diagonal trot. Hooves plant in world space, with two link leg IK, limited floor sampling, turning weight shifts, restrained head motion, and smoother activity transitions.

Stand close to a rear leg and press E to start a visible reach and bend, then a gradual lift. Pickup requires anatomically reachable contact; the local hand must touch the ankle before the body moves. The player grips one back leg with the right hand and curled fingers. The other legs hang with gravity; the suspended body has bounded, damped weight response. The actual animated ankle drives first person hand IK and Braxton's visible body. Equipped weapons hide while holding and return on release.

E on clear floor begins an animated lowering before the pig is released. Full body translation and rotation collision and supported placement remain required. E can still open deliberately aimed doors while carrying.

Validation contains observed walk and trot hoof contacts, actual E reach/lift/grip/carry/lower for both sizes, rendered right hand contact, closed door rejection, and saved screenshots. Both pigs also physically descended the stairs and reached the great room. The separate walking performance profile passed. Source contains exact final code and local check tools. Setup changes use isolated test saves and leave the saved map byte for byte unchanged.
