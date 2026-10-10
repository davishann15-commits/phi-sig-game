# Chapter01_House basketball court

Reference: IMG_1903, IMG_1904, IMG_1905, IMG_1907, IMG_1942.

The exterior builds at map load through HouseExterior.cpp. The court and its
surroundings are in HouseBasketballCourt.cpp; the shared basketball is an
AHouseBasketball, using the existing replicated furniture pickup and throw.
The saved interior map was preserved byte for byte against this task's backup.

## Controls

- Look down at the ball and press E to pick it up.
- Aim at either rim and left-click (right trigger on controller) to shoot.
- E sets the ball down. Aiming elsewhere gives a normal directional throw.
- A lost or abandoned ball returns to the court; no duplicate is spawned.

## Checks

The native -HouseBasketballReview run passed regulation ball size, spherical
collision, CCD, 15 court floor samples, three retaining walls, open rims and
colliding backboards, a real bounce, normal animated pickup, shots through both
rims, lost-ball recovery, and actual walking from the porch and around the court.
Details are in evidence/physics/result.txt and physics.log.

The hands-on native playtest also opened successfully, showed the E pickup
prompt, picked up the ball with E, and scored at the west hoop with a normal
left-click. The manual.log records COURT_BASKET end=0 left=1 right=0. No crash
was observed during this run. The game was left open at the court.

Server-only spawning, movement replication and the existing server grab
reservation are used. A second network client was not part of that test.

The six native game views are in evidence/visuals. Helper launches use isolated
settings and never change the saved map or normal PlayerStart. Run one Unreal
instance at a time. The opt-in -HouseCourtPlaytest only starts the player by the
ball; it leaves pickup, aiming and shooting to manual controls.

Unity files are outside this change.
