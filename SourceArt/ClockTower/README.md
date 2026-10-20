# Campus clock tower (2026-10-13)

Unreal project: `/Users/Stewart/Documents/Unreal Projects/SeniorSendoff`. Map: `/Game/Story/Maps/Chapter01_House`.

The stubby campus clock tower (a plain 5 m brick shaft with flat discs and fixed hands) is replaced by an original
collegiate-Gothic landmark that rises well clear of the campus roofline and reads from the frat lawn at dusk. Its four
dials keep the time, and the belfry rings the Westminster Quarters with bells synthesised in code. It is an original
design: it is not modelled on any real campus tower and carries no logos, crests or university marks.

Everything is built at runtime beside the exterior (`AHouseClockTower`, spawned from `AHouseExteriorDetail::Build`).
Nothing is saved into the map, no asset files were created, and no lights were added: the exterior still has 53
lights. The dial glow and the uplight on the stone come from material emission only.

## What was built

**Site.** The tower is centred at world (-9000, -10800), between the campus walk (Y -10400) and the library. Its plinth
stays inside the band where no grid tree can stand (Y -11200 to -9950). It is not added to the campus footprints, so
the tree scatter draws the same random numbers. The tree fingerprint is unchanged: `WOODS_PASS_ORIGINAL_TREES` reports
`crc=0f3b2bb7`, with 134 big, 136 small and 852 distant trees, before and after. No building or tree was moved. The
old tower code is gone from `BuildCampus`.

**Architecture** (heights above campus grade; 70 m to the vane):

| Height | Part | Details |
|---|---|---|
| 0–0.75 m | Plinth | Three limestone steps |
| 0.75–6.5 m | Base | Rusticated ashlar with a pointed oak doorway, hood mould, strap hinges and ring pulls. A date stone above the door reads ERECTED MDCCCXCVII (an invented year). Red Boston ivy climbs both sides of the door and the corners, over the buttresses. |
| 6.5–27.4 m | Shaft | Campus brick in English bond, with set-back buttresses that step in under sloped stone weatherings. Limestone quoins on all four corners and three string courses. Lancet windows with hood moulds: one lit amber on the +X face, one on the lawn side. |
| 27.8–36 m | Belfry | On each face, an open pointed arch with attached shafts and a balustrade, framed by two louvred lancets. Inside are an oak bell frame, the 1.65 m hour bell, four quarter bells and their hammers. The hour bell shows against the sky through the arches. |
| 35.5–36 m | Corbel table | Carries the projecting clock stage |
| 36–44 m | Clock stage | Limestone with copper drip stains washed down from the cornice and from under each dial bezel. Engaged corner shafts, a square stone surround and copper spandrels. |
| 44–46.4 m | Cornice and parapet | Machicolation corbels, a crenellated parapet, and four crocketed corner pinnacles with gilt knobs |
| 44.6–67.5 m | Spire | Verdigris copper with standing-seam ribs, hip rolls, a bellcast eave and four louvred lucarnes |
| Top | Finial and vane | Gilt ball finial and spike. Fixed N/E/S/W arms; the gilt arrow turns slowly in the evening breeze. |

**Dials.** Each dial is 3.7 m across:
- Backlit opal glass in twelve leaded panes. The panes alternate slightly in brightness and are emissive only.
- Black iron Roman numerals in the tower-clock style: IIII for four, with each numeral standing on the centre, so VI reads upside down.
- A minute track with heavier five-minute marks, plus radial tracery and an inner ring.
- A copper bezel.
- Ornate hands: a spade hour hand and a pointed minute hand, both with counterweights. Each hand is its own component; there are 8 in all.

**Life.**
- Six pigeons sit on the belfry sill.
- Twelve crows perch on the merlons. On the hour they burst off, circle the spire once and come back to land.
- A perched crow shuffles round now and then.

**Uplight.** Small bronze flood cans with glowing lenses sit on the plinth. The stone above them glows warm, and the glow steps down band by band: base, then shaft bands 1–3. The steps fall on the string courses, which hide them. This is emission only.

## Time and chimes

**Clock.** The game has no clock of its own, so the hands follow the local system clock and update every second. `-ClockTime=HH:MM[:SS]` sets the time instead. The hands are read back from their components and checked against the clock (`CLOCK_TOWER_HANDS ... worstDiffSeconds=`).

**Westminster Quarters** (1793, public domain): bells 1–4 are G#4, F#4, E4 and B3, and the hour bell is E3.

| When | What rings |
|---|---|
| :15 | Change 1 |
| :30 | Changes 2 and 3 |
| :45 | Changes 4, 5 and 1 |
| :00 | Changes 2, 3, 4 and 5, then the hour bell strikes once for each hour on a 12-hour count |

- A short lead-in lets the first hammer lift.
- A chime asked for while another is ringing is owed, not lost.
- **Random toll.** Now and then a single bell strikes, never within 45 s before a quarter or 30 s after a chime.

**Bells.** Synthesised in code; no recordings are used.
- **Partials:** additive church-bell partials: hum, prime, tierce (minor third), quint, nominal, deciem, undeciem, duodeciem and upper partials. Each has a long T60 decay, longer for bigger bells (the hour bell's hum lasts about 35 s).
- **Inharmonicity:** a slight per-bell inharmonicity, and mistuned doublets on the main partials, so the bells warble as real bells do.
- **Hammer clank:** a short band-passed noise burst.
- **Campus echo:** a baked touch of echo: early reflections off the halls and a slap back from the stadium.
- **Renders:** the five strikes render once at 32 kHz on a worker thread, in about 80 ms. Each chime is mixed on a worker thread.

**Playback.**
- Two spatialised voices in the belfry take turns, so a chime never cuts the last one's ring-out. The PCM is streamed to the wave about 6 s ahead.
- **Attenuation:** a custom curve slightly gentler than 1/r (r^-0.75), with an inner radius of 25 m. It falls to zero at 25 m + 900 m, plus distance low-pass from 20 kHz down to 3.2 kHz, so the bells are clear on the frat lawn and louder on campus.
- **Indoor duck:** inside the house the bells duck to 55 % and are low-passed to 1.7 kHz, using the house blocks from `UHouseDuskAtmosphereComponent::DistanceOutsideHouse`, the same footprint the dusk film look uses.

**Animation in sync.** The strike timeline runs on the world's audio clock: real time, undilated, and paused with the game's sound. Each hammer lifts about 0.3 s before its strike, lands on the sound bow on the beat and settles back. The struck bell shivers on its headstock and its clapper swings a little.

**Event.** `AHouseClockTower::OnClockTowerChime(int32 Hour, bool bQuarter)` (`BlueprintAssignable`) fires as the first bell of each chime sounds:
- `Hour` is 1–12.
- `bQuarter` is false only for the full hour chime.

Find the tower with `AHouseClockTower::Find(World)`.

## Tunables

| Switch / console | Default | What it does |
|---|---|---|
| `-ClockChimeEvery=<seconds>` | off (real quarters) | Test cadence: hour, :15, :30, :45 in turn, every N seconds (never overlapping) |
| `-ClockChimeNow` | off | Ring the full hour chime 3 s after the tower is built |
| `ClockTower.Chime [quarter 1-4] [hour 1-12]` | – | Console: ring now. 4 = the hour; the hour defaults to the clock's. |
| `ClockTower.Toll` / `ClockTower.Crows` | – | Console: one bell / send the crows off |
| `-ClockTollEvery=<seconds>` | 420 | Mean gap between random single-bell tolls (actual gap 0.5–1.5× this); 0 turns them off |
| `-ClockChimeVolume=<0..2>` | 1.0 | Bell volume. `ClockTower.Volume` (console variable) multiplies it. |
| `-ClockChimeFalloff=<cm>` | 90000 | Audible range beyond the 25 m inner radius |
| `-NoClockChimes` | – | Silent tower (hands, hammers and events still run) |
| `-ClockTime=HH:MM[:SS]` | system clock | Set the tower's clock (it keeps running from there) |
| `-ClockDialGlow=<x>` / `-ClockUplight=<x>` | 1.0 | Scale the dial glow or the stone uplight (tuning) |
| `-NoClockTower` | – | Build no tower (A/B performance checks) |
| `-ClockTowerReview` | – | Self-check; see below |

**Levels.** On the frat lawn, about 138 m from the belfry, the recorded hour chime peaks at about -32 dBFS RMS (50 ms windows), with nothing else playing. Within 25 m of the belfry the attenuation curve gives about 13 dB more.

## Performance

Frame times are in ms, from 1280×720 offscreen dusk captures with uncapped fps (`-ExecCmds=t.MaxFPS 0`). Each "mean of 3" is the mean of three runs without the tower (`-NoClockTower`) and three with it, alternating on the same build. "Old tower" is a single run of the previous tower, taken before any changes.

| View | Old tower | No tower, mean of 3 | New tower, mean of 3 | New − none |
|---|---|---|---|---|
| `balcony_out` | 15.11 | 15.08 | 16.42 | +1.34 |
| `front_street` | 24.27 | 24.68 | 24.71 | +0.03 |
| `front_lawn_overview` | 20.55 | 23.13 | 20.49 | -2.64 |
| `street_along` | 15.36 | 15.64 | 15.53 | -0.11 |
| `detail_establishing_high` | 21.22 | 21.22 | 21.03 | -0.19 |
| `detail_establishing_corner` | 19.02 | 19.31 | 19.84 | +0.53 |
| `detail_street_dusk` | 14.83 | 15.04 | 15.14 | +0.10 |
| `detail_clocktower_far` | 8.42 | 8.56 | 8.53 | -0.03 |
| `detail_clocktower_mid` | 8.51 | 8.58 | 8.62 | +0.04 |
| `detail_clocktower_face` | 9.00 | 8.69 | 9.28 | +0.59 |
| `detail_clocktower_belfry` | 8.42 | 8.50 | 8.49 | -0.02 |
| `detail_clocktower_base` | – | 8.65 | 8.66 | +0.01 |
| **Mean, 7 campus/street views** | **18.62** | **19.16** | **19.02** | **-0.13** |

- **Noise.** Single views swing by up to about ±3 ms from run to run, and hitch frames of 90–370 ms appear in some runs. `balcony_out` is the first view of each sweep and the noisiest.
- **Longer runs.** Two further pairs at 400 frames per view gave these differences (new − none):

  | View | Difference (ms) |
  |---|---|
  | `balcony_out` | -1.3 |
  | `front_street` | -0.7 |
  | `detail_clocktower_face` | -0.1 |
  | `detail_establishing_corner` | +0.15 (excluding one 367 ms hitch frame) |

- **Result.** No consistent cost is measurable; the tower is within the ~1 ms budget.
- **Data.** The raw CSVs are in `Deliverables/ClockTower_20261013/pass5_perf/`.

- **Budget.** The tower is 57.8k triangles in 29 components, plus two audio voices. It is built in about 150 ms during the exterior build, and the five bell strikes render in about 70 ms on a worker thread.
- **Per-frame cost.** Hands update once a second. Bells, hammers and crows touch components only while they move. The vane updates one transform per frame.
- **Shadows, collision and culling.** No shadows. Only the plinth, base and shaft have collision. Ornament is culled at 90 m, and the quarter bells, clappers and hammers at 140 m.

## Checks

All checks ran on the final build:

| Check | Result |
|---|---|
| `-CampusWoodsReview` | PASS: campusFrontVertices=1261938 (the old stub tower removed), rearBuildingVertices=0, closeRearTrees=40, distantRearTrees=852, geometryRegionsValid=1 |
| `-SurroundingsPolishReview` | PASS: streetVertices=23748, parkedCars=49, roadFloors=5/5 |
| `-ParkingLotWalkReview` | PASS: walk from the driveway to the house without a collision stall, exteriorLights=53 |
| `-WoodsPassReview` | PASS: actualExteriorPointAndSpotLights=53 |
| Tree fingerprint | `WOODS_PASS_ORIGINAL_TREES crc=0f3b2bb7`, the same as before the change |
| `-ClockTowerReview -ClockChimeEvery=20` (sound on) | PASS: see below |

The `-ClockTowerReview` run reported:
- parts=29, collidingParts=1 (the shaft), culledParts=15, policyViolations=0, towerLights=0, exteriorLights=53;
- handsWorstDiffSeconds=0.1;
- the 12 o'clock chime rang with hourStrikesRung=12;
- audioConsumed=1, and the crows burst.

`-ClockTowerReview -ClockChimeEvery=20` runs a self-check. It:
- builds the tower;
- checks the collision, shadow and cull policy, that the tower has no lights and that the exterior has 53 or fewer;
- reads the hands back against the clock;
- moves the audio listener to the frat lawn;
- rings the hour and the next quarter, recording the mixer output to `Saved/ClockTowerReview/ClockTowerHourChime.wav`;
- takes timed pictures: crows leaving, a hammer lifted and striking, the dial, and the view from the lawn;
- writes `Saved/ClockTowerReview/result.txt`.

**Recorded proof.** `Deliverables/ClockTower_20261013/pass3_bells_and_chime/analyze_chime.py` checks the recording against the logged strikes, in pure Python. The recorded 11 o'clock chime contains:
- all 16 melody notes of the Westminster hour chime, identified by pitch;
- all 12 hour strokes (the final run landed on 12 o'clock; an earlier run's 11 o'clock chime also checked 16/16 and 11/11);
- 0 ms lag between the hammer timeline and the audio.

## Files

- `Source/SeniorSendoff/HouseClockTowerMesh.h/.cpp`: site constants, painted textures and materials (`CT_*`), the architecture, dials, ivy, pigeons and floods. Moving parts are built in local space: hands, bells, clappers, hammers, vane and crow poses.
- `Source/SeniorSendoff/HouseClockTowerBells.h/.cpp`: bell synthesis and the Westminster schedule.
- `Source/SeniorSendoff/HouseClockTower.h/.cpp`: the actor, which handles the clock, hands, scheduler, mixing and streaming, attenuation, indoor duck, hammers, crows, vane, console commands, `OnClockTowerChime` and the self-review.
- `Source/SeniorSendoff/HouseExterior.cpp`: spawns the tower, and adds views `detail_clocktower_far`, `_mid`, `_face`, `_belfry` and `_base`. Their `detail_` prefix keeps them out of the planting.
- `Source/SeniorSendoff/HouseExteriorCampus.cpp`: the old tower is removed.

## Sources and licences

All geometry, textures and audio are generated in code in this project:
- The brick, ashlar, stained clock stone, belfry brick and copper textures are painted procedurally.
- The bells are additive synthesis.
- The Westminster Quarters melody (1793) is public domain.
- No third-party recordings, textures or models are used, so there is nothing CC0 to list.
