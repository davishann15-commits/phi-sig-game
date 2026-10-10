"""Preserve the exact source and successful native reviews with the project."""
from pathlib import Path
import json, shutil

ROOT = Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-refine-v2')
PROJECT = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff')
ARCHIVE = PROJECT / 'SourceArt/PigstyRoom/RefinementV2'
markers = {'upgrade': 'HOUSE_PIG_UPGRADE_RESULT passed=1',
           'stairs': 'HOUSE_PIG_SMOKE_RESULT passed=1',
           'visual': 'HOUSE_VISUAL_REVIEW_PASSED',
           'performance': 'HOUSE_WALK_PERF_RESULT: passed=1'}
logs = {name: (ROOT / ('native-' + name + '.log')).read_text(errors='replace') for name in markers}
assert all(markers[name] in logs[name] for name in markers)
upgrade = json.loads((ROOT / 'native-upgrade/Saved/HousePigUpgradeSmoke/result.json').read_text())
assert upgrade['passed'] and upgrade['escapedPigReturned']
install = json.loads((ROOT / 'installed.json').read_text())
collision = json.loads((ROOT / 'mattress-collision.json').read_text())
report = {'project': str(PROJECT), 'map': 'Chapter01_House', 'upgrade': upgrade,
          'installed': install, 'mattressCollision': collision,
          'nativeChecks': {name: {'passed': True, 'result': [line for line in logs[name].splitlines()
                           if markers[name] in line][-1]} for name in markers},
          'protectedHouseActors': install['protectedActors'],
          'doorRegistryUnchanged': install['doorRegistryUnchanged']}
(ROOT / 'final-review.json').write_text(json.dumps(report, indent=2))
for folder in ('Source', 'Validation'):
    (ARCHIVE / folder).mkdir(parents=True, exist_ok=True)
for name in ('HouseRoamingPig.h', 'HouseRoamingPig.cpp', 'StoryCampaign.h', 'StoryCampaign.cpp', 'HousePigSmoke.cpp'):
    shutil.copy2(PROJECT / 'Source/SeniorSendoff' / name, ARCHIVE / 'Source' / name)
for path in (ROOT / 'source').iterdir():
    if path.is_file(): shutil.copy2(path, ARCHIVE / 'Source' / path.name)
for name in ('installed.json', 'mattress-collision.json', 'final-review.json'):
    shutil.copy2(ROOT / name, ARCHIVE / 'Validation' / name)
for name in markers:
    shutil.copy2(ROOT / ('native-' + name + '.log'), ARCHIVE / 'Validation' / ('native-' + name + '.log'))
for name, folder in [('upgrade', 'HousePigUpgradeSmoke'), ('visual', 'HouseVisualReview')]:
    shutil.copytree(ROOT / ('native-' + name) / 'Saved' / folder,
                    ARCHIVE / 'Validation' / folder, dirs_exist_ok=True)
readme = PROJECT / 'SourceArt/PigstyRoom/README.md'
text = readme.read_text()
start = text.index('## Current upgrade validation')
end = text.index('## Portable source archive', start)
text = text[:start] + '''## Current upgrade validation

- Current saved sizes, capsules, hitboxes, speeds and separate maximum health: passed in the rendered game.
- Both pigs physically approached separate mattress places, climbed onto the repaired collider, lay down and breathed. Damage woke only the hit pig: passed.
- Actual player E pickup and placement, and W carrying, passed for both pigs. AI paused while held and resumed feeding and sleeping after placement.
- Pickup through the closed door or beyond interaction range was rejected. Both pigs escaped physically only after the player opened their door.
- Visible Cheetos matched the remaining food, replenished while occupied and paused while both pigs were outside.
- An escaped Coal was picked up in the upstairs hall, physically carried through the open doorway and placed safely back inside. Full body collision and floor support passed.
- Both resized pigs physically descended the stairs and reached the great room after real E opening of the intervening doors: passed.
- Four native camera views of the imported art: passed and inspected.
- Walking profile with both refined pigs active: 900 frames over 15 seconds, p50 16.67 ms, p95 16.72 ms, p99 16.76 ms, worst 17.06 ms, zero stalls over 49.72 m of physical travel. Tested at the preserved 1280 by 720, 80% resolution and 60 FPS profile.

The refinement retained 708 unrelated map actors and the exact door registry. The existing hidden mattress box asset received working collision. RefinementV2/Validation contains the native logs, screenshots and result report; Source contains the exact gameplay source snapshots and installation tools. Test setup changes are isolated from the saved map and normal player settings.

''' + text[end:]
readme.write_text(text)
shutil.copy2(readme, ARCHIVE / 'README.md')
print('PIG_REFINEMENT_ARCHIVE_COMPLETE', ARCHIVE)
