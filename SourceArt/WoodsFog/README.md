# Woodland fog

Added 12 low local fog volumes behind the basketball court. Height falloff keeps the mist low; soft sphere boundaries blend the layers. Existing house-wide distance haze is unchanged. No new lights, collision, or tick work.

## Verification

- Editor build succeeded.
- Native campaign screenshots verified the winding trail, shack approach, woodland layers, and clear house front.
- Woodland scene and collision checks passed.
- Saved Chapter01_House map hash is unchanged.
- At 1280x720, quality 2, three fixed native campaign views were checked with and without woodland fog. This is a camera benchmark, not a traversal or multiplayer test.

| View | Without fog | With fog |
| --- | ---: | ---: |
| SmoothWindingTrail | 37.8 FPS | 38.5 FPS |
| ShackApproach | 44.7 FPS | 43.7 FPS |
| ForestFogLayers | 42.7 FPS | 41.3 FPS |

Screenshots: `final/`. Native logs and detailed results: `evidence/`.
