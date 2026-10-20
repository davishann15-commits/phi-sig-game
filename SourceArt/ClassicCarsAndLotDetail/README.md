# Classic cars and parking lot detail (Claude, Oct 12)

Jacob asked for realistic cars from the 1950s-80s, one awesome sports car in the parking lot, and a more detailed lot.

## Cars
- `Source/SeniorSendoff/HouseClassicCars.h/.cpp` replace the three `HouseCars` models in `HouseExterior.cpp` (`HouseExteriorCars.cpp` is still compiled, unused; `HouseCars::CreateLook` still runs).
- Six original types (land yacht, wood wagon, square-body pickup, 80s hatch, 50s finned hardtop, one fastback muscle car), 18 paint schemes as material overrides, clear-coat paint from `M_CarPaint`.
- Scale 1.1 (house vertical scale), each car on its own paving; west-lot cars keep `HouseParking::ParkCars` bays and stop 4 cm short of the wheel stops.
- The muscle car is added in upper bay 5 (x -5045), clear of the reserved bays and loading aisle.
- HISMs are still named `ExteriorCar*`; car shadows on (`-NoCarShadows` turns them off; no measurable cost).
- `HouseCampusReview.cpp` SurroundingsPolishReview now checks the classic cars (>=5 models, >10 cars, real-car body boxes) instead of the old 3-model / 487 cm box assertion.

## Lot detail
- `Source/SeniorSendoff/HouseParkingDetail.h/.cpp` (`HouseLotDetail`), built after `HouseParking::Build`; `HouseParkingLot.cpp` untouched.
- `ExteriorParkingLotWear` (flat, no collision/shadow): oil stains, cracks, patches, tyre polish, drains and puddles, smudged lines, bay numbers, SLOW/STOP + stop bar, red fire-lane curb with stencils, leaves, cups and cans.
- `ExteriorLotProps` (collision, shadows): lamp pier bases, two bollards at the loading-aisle mouth (2 m clear), tow sign, two RESERVED signs, fire-lane sign, dumpster on a pad with roll carts and bags (three hedge shrubs at x -3660 are skipped there).
- No lights added (53). The bay table is copied from `HouseParkingLot.cpp` Bays(); update both if bays move.

## Verification
- Build passed (Mac, UE 5.8). Native views in `Saved/CarViews_shadows`, `Saved/LotViews`, `Saved/QuickViews`.
- `-ParkingLotWalkReview -CampusWoodsReview -SurroundingsPolishReview -FraternityBoundaryReview -WoodsSceneReview -WoodsUndergrowthReview`: PARKING_LOT_REVIEW PASS (parkedCars=7, reserved bays and aisles clear, routes 4/4, low-cost decoration 1, lights 53, walk PASS), SURROUNDINGS_POLISH_REVIEW PASS, CAMPUS_WOODS_REVIEW PASS.
- Saved house map not modified.
