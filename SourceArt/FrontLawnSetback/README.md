# Front lawn and road setback

Implemented 2026-10-10 in the Unreal project for `Chapter01_House`.

The front road is 1200 world centimeters farther from the house. Both road edges, paint, sidewalks, curbside vehicles, street lights, street tree rows, the opposite parking lot and its frontage move together. The north/south road retains its position and meets the relocated intersection.

The terrain restores grass in the former front road/sidewalk strip. A clear front lawn remains on both sides of the centered concrete approach. The approach, side-door sidewalk, and both driveway entrances extend to the new sidewalk/road. The house and the front landing/steps keep their existing dimensions and location. Protected map and great-room collision mesh hashes match the before-task `protected.json`.

## Verification

- Native Development build succeeded.
- Actual collision-face material checks confirmed grass in the old road strip, concrete on the extended approach/sidewalk, asphalt on the relocated road, and supported driveway extensions.
- The rendered `-HouseExteriorWalkReview` run passed with normal CharacterMovement across the expanded lawn, front approach, street curb and both driveway entrances.
- The original stairs and both perimeter paths passed again.
- Normal E interaction opened both walnut front-door leaves; the player walked through into the great room.
- The game exited cleanly after PASS. Screenshots and logs are in `evidence/`.

The source geometry is generated at level startup. `Source/SeniorSendoff/HouseExteriorSiteLayout.h` owns the setback used by the road, walk and review. Other implementation files are `HouseExterior.cpp`, `HouseExteriorWalks.cpp`, and the opt-in `HouseExteriorWalkReview.cpp`. Before-task source copies are in `backup/`.

No Unity files were edited. No multiplayer session or FPS benchmark was run for this static site layout change.
