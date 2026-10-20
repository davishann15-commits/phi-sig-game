# MASTER PROMPT: the real sky over Calder: the evening of Saturday, October 18 1986

## Context
Senior Sendoff is a UE 5.8 game (`~/Documents/Unreal Projects/SeniorSendoff`). The whole world outside the frat house is
built at runtime in C++.
- **The scene:** Calder, an invented college town in a mountain cove in the southern Appalachians, just after homecoming
  on **Saturday, October 18 1986**.
- **Tone:** horror, mystery and 1986 nostalgia.
- **The sky today:** a frozen dusk. A purple sky with a warm afterglow behind Gosling Knob (yaw ≈205°), built by
  `HouseDuskAtmosphere.cpp` and the sky/fog/sun block in `HouseExterior.cpp`. It has no stars, no moon and no clouds, and it
  doesn't feel like a real evening sky. See `Saved/CalderWork/runs/w1_shots/Saved/HouseExteriorViews/` (P5, P7, P8,
  `front_street`, `detail_clocktower_far`).

**The goal:** a sky a player would believe if they stepped outside on that exact night: correct stars, moon and planets
for the date and place, real light pollution from the town, believable clouds and moonlight, the small moving things a
real sky has, at the 1986 standard of a dark mountain town. The bar is real photographs and a planetarium check, not
"looks nice".

**Read first:**
- `SourceArt/CalderLandscape/CODEX_HANDOFF.md`: the machinery (build, run, test, capture on this Mac) and the owner's
  rules. They all apply here.
- `HouseDuskAtmosphere.cpp`, and the sky/fog/sun code in `HouseExterior.cpp` (grep `SkyAtmosphere`,
  `ExponentialHeightFog`, `DirectionalLight`, `SkyLight`).
- How exposure is set (`ExteriorBrightness` work in SourceArt).

## The owner's decision: early dusk to night
**Decided by the owner (2026-10-18): "start early dusk and finish at night".** The sky is no longer frozen. Each session
starts at early dusk and darkens through the real twilight into full night, then holds at night.
- **Start:** early dusk, a few minutes after sunset (compute sunset for the date and place, about 7 pm EDT). The sky is
  bright in the west, with a warm afterglow behind Gosling Knob, and no stars yet except perhaps the brightest planet.
- **The path:** the real sequence for that evening.
  - Civil twilight fades.
  - Jupiter and the brightest stars appear one by one, in their real order of brightness.
  - The town's sodium glow takes over the south.
  - Nautical twilight: the ridges go black against a deep blue.
  - The Hunter's Moon clears the east ridge at its computed time, orange and huge, and climbs and whitens.
  - Astronomical twilight ends, and the full star field shows as far as the moon allows.
- **End:** full night, about 9:30 pm EDT, held from then on.
- **Pace:** compress the real ~2.5 hours. The default is **45 minutes of play from start to full night**, set by one
  constant and a `-SkyMinutes=<n>` switch. The rate is uniform in real sky time, so the long nautical stage doesn't drag.
- **Clock:** server time since the session's map started, replicated through the GameState. Every player, including late
  joiners, sees the same sky. It is never tied to story checkpoints (no story hooks).
- **Lighting follows the sky.**
  - The directional light hands over from the sun (below the horizon, its colour fading) to the moon at the moment it
    rises.
  - The sky light and the exposure follow, smoothly, with no visible steps.
  - The 53 exterior lights stay the same lights. If any already switch on at dusk, let them keep their behaviour; don't
    add switching logic that the owner didn't ask for.
- **Reproducible captures and reviews:** a `-SkyTime=<HH:MM>` switch pins the clock (e.g. `-SkyTime=19:05` for the
  start, `-SkyTime=21:30` for night). Run the full review set at both ends. Shots go in pairs: dusk and night of the same
  view. The wildlife job's 30-minute cycle is separate: leave it a `CalderSky::NowMinutes()` hook to read.
- **Plan for the impact:** every review that measures brightness and every existing screenshot will change. Re-base
  openly and never loosen a threshold.

## The owner's decision: the real moon
**Decided by the owner (2026-10-18): "a real moon would be awesome".** Use the real moon of that night: about one day after
the full **Hunter's Moon** (verify the phase, rise time and path with the ephemeris). It rises orange and huge over the east
ridge during the dusk-to-night progression, then climbs and whitens. As in reality, its light washes out the Milky Way and
the faint stars near it. Don't cheat the star count up to compensate. Instead, make the stars that do survive, the moonlit
clouds, the moon shadows and the silver light on the ridges carry the night.

## The real sky to build (compute everything; never guess)
- **Place and time.**
  - Compute the sky for latitude 35.96° N, longitude 83.92° W (a southern-Appalachian stand-in; never named in the game).
  - The date is 1986-10-18. **EDT is UTC−4** (daylight time ended Oct 26, 1986).
  - Use an ephemeris (e.g. Python `skyfield` with JPL DE421; the data is public domain) to get, for the chosen time: the
    sun's depression, the moon's position and phase and the libration angle for its texture, the positions and
    magnitudes of the planets, and the sidereal rotation of the stars.
  - Log them in `PLAN.md`.
- **Stars.**
  - The Yale Bright Star Catalogue (≈9,100 stars to mag 6.5; public domain) at their real RA/Dec, rotated to the time
    and place.
  - Brightness from magnitude and colour from B−V: Antares orange, Rigel blue-white.
  - Scintillation: twinkling grows toward the horizon, almost none overhead.
  - Atmospheric extinction near the horizon.
  - Limiting magnitude falls where the moon and the town's glow brighten the sky (compute it per direction; under the
    real moon only the brighter stars survive near it).
- **Planets.** As computed. In October 1986 Jupiter should be the brightest point in the south-east evening sky (verify).
  Steady points, no twinkle, the right colours. Don't add Halley's Comet: by October 1986 it was far below naked-eye
  brightness (verify).
- **The Milky Way** (under the real moon at most a faint trace, overhead and to the west, far from the moon; let the sky-brightness model decide, not taste): from a public-domain source (NASA SVS "Deep Star Maps", NASA
  imagery is public domain), aligned to the galactic coordinates, faint and grey-green, never the saturated purple of
  posters.
- **The moon.**
  - A real lunar albedo map (NASA LRO/Clementine; public domain) at the computed phase and position, with an earthshine
    level appropriate to the phase.
  - Near the horizon: orange, flattened slightly by refraction, dimmed by extinction.
  - A soft halo when thin cloud crosses it.
  - Moonlight takes over the directional light at the moon's direction once the moon is up. Its intensity is physically about 1/400,000 of the
    sun's; tune the exposure so the result reads like a photo of a moonlit night, not a blue-tinted day. Keep the colour
    slightly warm in the light but graded cool and desaturated in the shadows, the way eyes see at night.
- **Sky brightness.** Real twilight colour for the sun's depression. Airglow (a faint green-brown band ~10–15° above the
  horizon on dark nights). Sky colour shifts with the moon's brightness.
- **Light pollution.** A 1986 sodium-orange dome over the town in the valley to the south (Main St and the mill). Its
  strength falls off with direction and altitude; it lights the underside of low clouds and the haze in the valley. Fainter
  glows over campus. None over the north ridges.
- **Clouds.**
  - A few slow, thin layers: high cirrus that catches the moonlight, and occasional low stratocumulus drifting over the
    ridges, lit orange from below over the town.
  - They hide and reveal stars as they pass.
  - Deterministic from server time, so every player sees the same clouds.
- **Moving things (rare, all emission-only):**
  - Orionid meteors: a few per hour, since the peak is Oct 21, from the radiant near Orion's club.
  - A satellite's slow, steady dot crossing in a couple of minutes; a rare aircraft with red and green nav lights and a
    white strobe, very high, silent.
  - Bats crossing the moon's disc now and then belong to the wildlife job: leave a hook, don't build them.
- **Mountains against the sky.** The ridges stay black silhouettes. Stars set behind the real skyline (never through the
  terrain). Gosling Knob, the fire tower ridge and the water tower read against the town glow.

## Hard constraints
- Everything in `CODEX_HANDOFF.md` "The owner's rules" and "Hard constraints":
  - hands off the frat house, court, Fall Jam field and parking lot;
  - tree crc `0f3b2bb7`;
  - runtime-built only; no Nanite, Landscape, Niagara, PCG or Water modules; no editor-only APIs;
  - Unity-build safe and `-Wshadow` clean; commit nothing.
- **Exactly 53 exterior point/spot lights.** The sky adds none. Every star, meteor, nav light and glow is emission. The only
  directional light stays the one that exists: it follows the sun, then hands over to the moon when the moon rises.
- **This job is the owner's explicit OK to change the sky, the sun and the sky light.** Keep the global height fog unless
  the owner agrees. Log every changed value, its old value and why.
- **A/B:** everything sits behind `-NoRealSky`, which gives back today's exact state (prove it: review numbers, crc and
  identical screenshots).
- **Determinism:** the sky (stars, moon, clouds, meteors, aircraft) comes from server time and seeds, so two players on two
  machines see the same sky. Check it in the two-player test.
- **Performance:** the frame is render-thread bound.
  - **Cost budget:** the sky may cost **+0.5 ms** in the street and campus views (median of 3 alternating rounds,
    `-MetalOffscreenOnly`).
  - **How:** stars as one instanced mesh or a baked star texture, not 9,000 components. No volumetric clouds unless they
    fit the budget.
- **Licences:** only public-domain or CC0 data and textures (NASA, JPL, the Yale BSC, Poly Haven). Note the source of each
  in `README.md`. No copyrighted sky photos.
- **No in-game text, logos or names** of real places, observatories or brands.

## Verification
- **Planetarium check.**
  - Render the same time and place in Stellarium (free software, used only as a reference). Compare it with in-game
    captures facing N, E, S, W and straight up.
  - Pegasus's Great Square, the Summer Triangle, Cassiopeia, the Big Dipper, Jupiter and the moon must sit within 1° of
    the reference.
  - Put the comparisons side by side in `SourceArt/NightSky/`.
- **Photo check.** Next to real photos of a near-full-moon (or dark, per the decision) autumn night in a small mountain
  town, judge exposure, colour, star density near the moon and the town's glow. Write the verdict honestly in STATUS.
- **Views** (`detail_sky_*`, appended to `ExViews`):
  - the moon over the east ridge;
  - the zenith from the frat lawn;
  - the town glow from Founders' Overlook;
  - the stars through bare branches in the woods;
  - the clock tower against the stars;
  - a meteor (force one with a debug switch).
- **Reviews:** every review in `CODEX_HANDOFF.md`'s list, plus a `-RealSkyReview`:
  - logs the computed sky (sun depression, moon alt/az/phase, Jupiter alt/az, star count above the limiting magnitude);
  - checks no star renders below the terrain skyline;
  - checks the 53 lights;
  - checks determinism (same seed, same sky).
- **Finish:** a frame-time A/B, one `-DisableUnity` build, `SourceArt/NightSky/README.md` (what was built, sources and
  licences, switches, numbers), and STATUS and `CODEX_HANDOFF.md` "Current state" updated.

**Stop and ask only for:** any change to the global height fog, more than 53 lights, or touching
the frat house area.
