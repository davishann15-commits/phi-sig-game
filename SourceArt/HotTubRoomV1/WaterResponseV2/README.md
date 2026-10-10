# Hot tub reactive water, version 2

The existing first-floor spa tub now responds to actual character movement.
Entry and exit produce localized splashes, wading displaces the live wave grid,
and small foam patches follow the water surface. Ballistic droplets fall back
into the basin and produce small secondary ripples. Waves settle when movement
stops; the existing gentle jets and steam remain.

The physical surface is a bounded damped height field, not a volume fluid
solver. Tuning: 41x41 grid, 62 cm/s propagation, 1.1 damping, 5 cm maximum height.
Rounded tub bounds keep effects inside. Fixed pools contain 96 droplets and 64
foam flecks; repeated splashes create no actors. The water surface is nonblocking
and the existing immersion movement, tub collision, stairs and room remain.

Water material v3 copies the approved Substrate single-layer water v2. Only the
vertex-color foam multiplier increases from .42 to .65. The old material remains
available. New glossy aqua droplets and ivory foam have dedicated materials.

Native Chapter01_House test passed with actual stair walking and jump into the
basin, 301 cm of wading, 1 entry, 43 movement impulses and 17 splash bursts. Peak
wave height was 4.797 cm with 36 active droplets. Stationary grid energy fell
from 1.12371 to .00007355 in five seconds with jets temporarily off. The player
walked out normally; jets were restored, controls and movement settings stayed
unchanged, and runtime actors remained 748 before and after. The map audit
confirmed all 723 other actors unchanged and the saved map still has 724 actors.

Validation contains the native result and screenshots. Source retains the
material authoring/apply script and opt-in isolated test runner. The map backup
is in the task outputs/hot-tub-water/backup directory.
