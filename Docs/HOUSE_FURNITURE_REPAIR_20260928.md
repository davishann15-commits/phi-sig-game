# House furniture repair — September 28

The imported striped-bedroom futon now has the source-visible low, tilted tufted back instead of three tall cushion blocks. Its back is approximately 0.676 m high and its seat is 0.356 m high. The resized 0.76 × 0.40 m ottoman sits immediately in front at the corrected raw scan station. Human-size furniture and the approved 1.8× game footprint remain. The editable house generators are in the sibling Senior Sendoff repository; its user working Blender file and all original scans/photos were preserved.

Source SHA-256: `22b4bdcf5e04db96f4e6a1931d54611950ec0cc0fb79b5f8737365cdbebb7047`. Source/campaign imports, native captures and the refreshed package correspond to that generated house. The campaign refresh preserved three gameplay actors and four movable physics pieces.

All 14 campaign checks actually passed: Editor compilation, normal movement/furniture, targeted ottoman grab/drag/release, controlled full story, solo lobby, Linux packaging, packaged map loads, default startup/return, deliberately incomplete-party recovery, packaged movement, packaged ottoman interaction and windowed Vulkan startup/travel/return. The player dragged the real tagged ottoman 42.1 cm in Editor and package. The Development-only `-CombinedHouseFurniture=S05_Furn_SouthB_Ottoman` flag selects that actual body for the existing player interaction smoke. These tests use an isolated automation campaign save.

The later native house review passed two affected bedroom entry/return routes, ten floor checks and all 83 current views. The preceding 62-route pass belongs to the study-chair revision; exact source comparison proves 8,816 other meshes and 69 other S05 furniture stations unchanged. These are assisted checks, not an unassisted campaign or new multiplayer playthrough. Complete exact house fidelity remains in progress.

Source and review evidence: [bedroom repair](</home/davis/Documents/ChatGPT/Senior Sendoff/Docs/HouseRebuild/BEDROOM_FUTON_20260928.md>) and [source-bound delivery checks](</home/davis/Documents/ChatGPT/Senior Sendoff/Docs/HouseRebuild/ReviewCaptures/Unreal20260928BedroomFuton/Validation/delivery_verification.json>).

Launch the updated Linux Development game:

```bash
"/home/davis/Documents/ChatGPT/phi-sig-game/Play Senior Sendoff.sh"
```
