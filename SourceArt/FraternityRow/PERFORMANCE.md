# Fraternity Row render-thread measurements

per-run mean, median of three alternating rounds; baseline median of six unchanged observations. The unchanged baseline range is included to expose measurement variability.

| Standard view | Before median ms | Before range ms | After median ms | Before/after | Upgrade A/B | Six lights A/B |
|---|---:|---:|---:|---:|---:|---:|
| front_street | 16.16 | 14.98–28.51 | 15.64 | -3.2% | -3.3% | -2.7% |
| street_along | 14.35 | 11.75–24.38 | 12.20 | -15.0% | -1.0% | -9.8% |
| detail_establishing_corner | 16.94 | 14.11–27.62 | 15.51 | -8.4% | +7.7% | -5.8% |
| detail_street_dusk | 15.22 | 12.04–24.47 | 12.60 | -17.2% | +1.9% | -9.8% |
| detail_clocktower_far | 9.12 | 6.48–15.83 | 7.70 | -15.6% | +8.8% | -13.9% |
| detail_stadium_gate | 10.41 | 7.23–17.74 | 7.77 | -25.4% | +3.6% | -7.9% |
| detail_campus_quad | 11.95 | 8.16–21.28 | 8.46 | -29.2% | -2.9% | +2.6% |
| detail_layout_gateway | 10.63 | 7.41–17.80 | 7.51 | -29.4% | -2.8% | +4.9% |
| detail_layout_quad | 13.26 | 8.21–20.68 | 8.57 | -35.4% | +2.1% | +3.9% |
| detail_layout_aerial | 19.33 | 12.71–26.67 | 14.98 | -22.5% | +2.1% | +1.9% |
| detail_layout_dormrow | 10.71 | 7.60–16.84 | 7.61 | -28.9% | -9.9% | -0.4% |

Full upgrade A/B uses `-NoFraternityRowUpgrade`; light-only A/B uses `-NoFraternityRowLights` and retains every fixture/emissive/house/yard. Both compare exactly the same11 standard views. Render-thread budget is no view worse than+5%. No unmeasured result is claimed.
