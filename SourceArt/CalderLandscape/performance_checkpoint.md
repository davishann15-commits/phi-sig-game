# Performance checkpoint - 2026-10-16

This is a partial implementation checkpoint at terrain hash `ff4f9ab1`. It is not final Phase 4 or wildlife acceptance: the Phase 3 server scene clock is not implemented yet, although `-CalderSceneTime=120` is supplied for forward compatibility. No C++ or active baked terrain changes occurred during these captures.

## Method

`Tools/calder/perf_checkpoint.sh` uses three alternating on/off pairs, the original 300 measured frames per eye and four-second warm-up, uncapped offscreen Metal rendering, `r.InstanceCulling.OcclusionCull=1`, and 16 views including five Tier A interior comparisons. Only one game/build/editor job ran at a time. Strict `report_checkpoint_perf.py` requires every round, every required eye, finite timings and clean capture completion. Results below are medians of each round's measured frame-time average.

## Appearance comparison - complete

Switch: `-NoCalderLandscape`. Source runs: `codex_checkpoint_look_{on,off}{1,2,3}`. All six captures completed cleanly. Every incremental delta is between -0.13 and +0.25 ms, inside the specified +/-0.5 ms noise band; all five interior comparisons are inside that band too. Original tree CRC `0f3b2bb7`, 53 exterior lights and terrain hash `ff4f9ab1` were retained. The off captures disable the new pig, scanned leaf and mist markers.

| View | On ms | Off ms | Delta ms | On game/render | Rounds |
|---|---:|---:|---:|---:|---:|
| detail_campus_quad | 8.06 | 8.10 | -0.04 | 3.35 / 8.06 | 3/3 |
| detail_clocktower_far | 6.29 | 6.14 | +0.15 | 3.31 / 6.28 | 3/3 |
| detail_establishing_corner | 15.16 | 15.08 | +0.08 | 3.35 / 15.16 | 3/3 |
| detail_layout_aerial | 13.45 | 13.47 | -0.02 | 3.29 / 13.44 | 3/3 |
| detail_layout_dormrow | 7.15 | 7.04 | +0.11 | 3.42 / 7.14 | 3/3 |
| detail_layout_gateway | 7.08 | 7.21 | -0.13 | 3.28 / 7.08 | 3/3 |
| detail_layout_quad | 8.25 | 8.19 | +0.06 | 3.43 / 8.19 | 3/3 |
| detail_perf_in_harlow | 9.85 | 9.60 | +0.25 | 3.39 / 9.85 | 3/3 |
| detail_perf_in_lib | 6.62 | 6.53 | +0.09 | 3.13 / 6.62 | 3/3 |
| detail_perf_in_science | 6.23 | 6.10 | +0.13 | 3.17 / 6.22 | 3/3 |
| detail_perf_in_stadium | 6.52 | 6.43 | +0.09 | 3.32 / 6.52 | 3/3 |
| detail_perf_in_union | 6.08 | 6.08 | +0.00 | 3.16 / 6.03 | 3/3 |
| detail_stadium_gate | 6.63 | 6.59 | +0.04 | 3.35 / 6.63 | 3/3 |
| detail_street_dusk | 13.30 | 13.42 | -0.12 | 3.30 / 13.30 | 3/3 |
| front_street | 15.98 | 15.90 | +0.08 | 5.59 / 15.97 | 3/3 |
| street_along | 13.16 | 12.95 | +0.21 | 3.29 / 13.15 | 3/3 |

## Terrain comparison - complete

Switch: `-NoCalderTerrain`. Sources: `codex_checkpoint_terrain_{on,off}{1,2,3}`. All six captures completed cleanly. Street, campus eye-level and aerial absolute medians meet the original p8p budgets (22.96/15.15 ms streets, 9.96/9.58 ms campus and 16.07 ms aerial). Four Tier A interior comparisons remain inside +/-0.5 ms; **Harlow is +0.61 ms and remains outside the interior noise requirement**. Do not call the whole performance gate passed.

| View | On ms | Off ms | Delta ms | On game/render | Rounds |
|---|---:|---:|---:|---:|---:|
| detail_campus_quad | 8.11 | 7.89 | +0.22 | 3.31 / 8.11 | 3/3 |
| detail_clocktower_far | 6.14 | 5.88 | +0.26 | 3.17 / 6.13 | 3/3 |
| detail_establishing_corner | 15.11 | 15.23 | -0.12 | 3.34 / 15.10 | 3/3 |
| detail_layout_aerial | 13.60 | 12.55 | +1.05 | 3.26 / 13.60 | 3/3 |
| detail_layout_dormrow | 7.09 | 6.96 | +0.13 | 3.27 / 7.08 | 3/3 |
| detail_layout_gateway | 7.23 | 6.99 | +0.24 | 3.20 / 7.22 | 3/3 |
| detail_layout_quad | 8.16 | 8.17 | -0.01 | 3.36 / 8.09 | 3/3 |
| detail_perf_in_harlow | 9.91 | 9.30 | +0.61 | 3.39 / 9.91 | 3/3 |
| detail_perf_in_lib | 6.62 | 6.69 | -0.07 | 3.05 / 6.62 | 3/3 |
| detail_perf_in_science | 6.06 | 5.99 | +0.07 | 3.23 / 6.05 | 3/3 |
| detail_perf_in_stadium | 6.36 | 6.25 | +0.11 | 3.26 / 6.35 | 3/3 |
| detail_perf_in_union | 5.94 | 6.10 | -0.16 | 3.13 / 5.94 | 3/3 |
| detail_stadium_gate | 6.62 | 6.25 | +0.37 | 3.21 / 6.62 | 3/3 |
| detail_street_dusk | 13.29 | 13.41 | -0.12 | 3.38 / 13.29 | 3/3 |
| front_street | 15.93 | 16.02 | -0.09 | 5.69 / 15.93 | 3/3 |
| street_along | 13.05 | 12.76 | +0.29 | 3.28 / 13.05 | 3/3 |

## Corrected interior fixture - complete at 2749a6c7

The inherited Harlow target used terrain relief 205.57 cm while the eye/building used pad DZ 242 cm. This changed camera pitch between terrain on/off. The five interior views now translate both eye and target by the same `BlockDZ`; XY, FOV, room geometry, warm-up, sample counts and limits remain unchanged. Both actual Harlow images were inspected after the correction and retain the same relative room framing.

`codex_interiors_rebased_{on,off}{1,2,3}`: explicit five-interior diagnosis, three complete alternating pairs, 300 measured frames/eye and four-second warm-up. All five deltas are inside +/-0.5 ms. Harlow is -0.05 ms. The earlier +0.61 ms result is retained above with its fixture mismatch; it is not silently waived. This focused rerun resolves the interior comparison at this checkpoint, and does not replace the final full 16-eye comparison after all implementation.

| View | On ms | Off ms | Delta ms | On game/render | Rounds |
|---|---:|---:|---:|---:|---:|
| detail_perf_in_harlow | 9.25 | 9.30 | -0.05 | 3.36 / 9.25 | 3/3 |
| detail_perf_in_lib | 6.89 | 6.99 | -0.10 | 6.60 / 6.79 | 3/3 |
| detail_perf_in_science | 5.99 | 6.01 | -0.02 | 3.26 / 5.99 | 3/3 |
| detail_perf_in_stadium | 6.25 | 6.29 | -0.04 | 3.40 / 6.24 | 3/3 |
| detail_perf_in_union | 5.81 | 6.16 | -0.35 | 3.16 / 5.81 | 3/3 |

## Other outstanding acceptance

The rendered paired hill walk still fails with four corrections; the identical simulation-only NullRHI diagnostic passed with zero walk corrections. The diagnostic does not waive the rendered result. Full wildlife timing, voice/source budgets and the completed-implementation load/draw/frozen checks remain pending.

## Counts and load

All twelve captures retain tree CRC `0f3b2bb7` and 53 exterior lights. Every terrain-off capture restores `instances=53740` and `grounds=69240`; terrain-on retains `ff4f9ab1` and 24 terrain tiles. Appearance-on load median 10.085 s; terrain-on median 10.240 s (individual on loads 10.017..10.246 s), below baseline 9.993 s plus the allowed three seconds. Terrain-off median 8.718 s. Source logs remain under each run's `Saved/Logs` directory.
