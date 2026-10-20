"""Render current acceptance receipts without promoting pending or superseded evidence."""
from datetime import datetime, timezone
import sys
from checkpoint_validation import (
    ROOT, OUT, WORK, PHASES, read_json, runtime_hashes, valid_tag,
    validate_manifest, validate_visuals,
)

tag = valid_tag(sys.argv[1])
current = runtime_hashes()
phases = (*PHASES, 'visuals')
states = {}
for phase in PHASES:
    path = WORK / (tag + '_' + phase + '.json')
    try:
        value = read_json(path) if path.exists() else {}
    except (OSError, ValueError, TypeError):
        states[phase] = 'Pending; receipt incomplete or unreadable'
        continue
    passed = value.get('native_complete') and value.get('budget_passed') if phase == 'perf' else value.get('passed')
    states[phase] = 'PASS' if passed and value.get('source_sha256') == current else 'Pending'
    if value and value.get('source_sha256') != current:
        states[phase] = 'Superseded; rerun pending'
try:
    validate_visuals(tag, current)
    states['visuals'] = 'PASS'
except (OSError, ValueError, KeyError, TypeError) as error:
    states['visuals'] = 'Pending'
try:
    manifest = validate_manifest(tag, current)
    complete, recovery_reason = True, ''
except (OSError, ValueError, KeyError, TypeError) as error:
    complete, recovery_reason = False, str(error)
scene_current = states['scenes'] == 'PASS'
tree_facts = ('- Original tree CRC `0f3b2bb7`; current native controls compare all 1600 rendered transforms exactly. Hidden original A/B count 26 is below the unchanged 40 limit. The canopy-envelope check reports zero overlap.'
              if scene_current else '- Tree requirements: original CRC `0f3b2bb7`, exact rendered transforms, at most 40 hidden original A/B trees, and zero canopy-envelope overlap. Current-source scene verification is pending.')
protection_facts = ('- No map save or commit. The validated recovery manifest confirms the declared Source/Config/project/map guards and exact recovery copies.'
                    if complete else '- No map save or commit is authorized. Final Source/Config/project/map and recovery-copy verification remains pending.')
lines = ['# Fraternity Row status', '',
         '**Implementation and acceptance complete.**' if complete else '**Revised five-house Row. Final acceptance is in progress.**', '',
         'Updated ' + datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M UTC') + '. Runtime evidence prefix: `' + tag + '`.', '',
         'The three existing houses are walk-in houses with distinct dark, tidy and wrecked-party states. A Tudor occupies the existing grass, and a Queen Anne with a turret and connected parlors uses the authorized former parking area. Each has a separate backyard, an accessible outbuilding and a path to the original woods fence. The setting is Homecoming night in October 1986.', '',
         'The Queen Anne was moved forward after image review found real branches inside its original rooms. Its revised complete envelope clears the original canopies; trees were preserved. Window-mounted displays and shelving were corrected to use opaque backing, with supported brackets. Current coordinates and routes are in [PLAN.md](PLAN.md) and [PLAN.json](PLAN.json). The amended plan was shown before rebuilding.', '',
         '## Scope and budgets', '',
         '- Existing map bounds, road, terrain grade, woods fence/gate, main house/rooms, court, Fall Jam, east driveway, story and movement stay in place.',
         '- West main-house parking pavement, bay furniture/cars and two parking lights are removed under Stewart\'s explicit instruction. The old parking walk and main-house connection retain their coordinates.',
         '- Exterior light budgets: baseline 53, revised 57 = 53−2+6; lights-off control 51; shared interior pool 6. The six new points are small and shadowless.',
         tree_facts, protection_facts, '',
         '## Current native acceptance', '', '| Phase | Current source | Evidence |', '|---|---|---|']
labels = {'scenes': 'Legacy / upgrade / six-lights-off scene controls', 'smoke': 'All five Row room smokes',
          'interiors': 'All five physical interior routes and natural unloading', 'yards': 'Five yards, sheds, fence approaches and return indoors',
          'captures': '16 upgraded dusk +16 dark views', 'lot_controls': 'Two added poses at legacy dusk +dark',
          'regressions': 'Full existing campus, terrain, outdoor and house regression chain',
          'mp': 'Five Row host/client pairs +original library hill walk', 'perf': 'Full-upgrade and six-light A/B:12 native runs',
          'visuals': 'Manual review: all 136 required images and current review documents'}
for phase in phases:
    path = WORK / (tag + '_' + phase + '.json')
    lines.append(f'| {labels[phase]} | {states[phase]} | [{path.name}](<{path}>) |')
lines += ['', 'Source-derived arithmetic, an exited launcher, and historical images are not used as native passes. Final-source receipts above must pass before release.', '',
          'The unchanged before run had three existing failures. Final regression acceptance requires a pass or the same sole native verdict; no threshold or original waypoint is loosened:', '',
          '- Exterior walk: front site surface material at(800,-4025), Z−57.',
          '- Combined-house smoke: mouse-look direction, step2, pitch0.',
          '- Original-house door smoke: missing extracted original leaf asset.', '',
          'The before campus groups passed431/431 stops across18 buildings; terrain, FreeRoam, old parking/woods/field walks, tree interactions,18 interior smokes and house traversal also passed. Exact before lines are in [BEFORE_CRITIQUE.md](BEFORE_CRITIQUE.md) and the before-check receipt. These historical results do not replace the current-source phases.', '',
          '## Images', '',
          'The original14 Row poses are identical before/after. Two new Queen Anne lot poses were appended after all existing Row and stadium cameras; matching legacy controls supply their comparison. Nightfall is pinned to0 for dusk and1800 for dark. Older October12 images are historical and excluded.', '']
for label, suffix, phase in [('Before, dusk', tag + '_lot_legacy_dusk', 'lot_controls'),
                             ('After, dusk', tag + '_dusk', 'captures'),
                             ('Before, dark', tag + '_lot_legacy_dark', 'lot_controls'),
                             ('After, dark', tag + '_dark', 'captures')]:
    pattern = '277_detail_row_R5_lot_front.png'
    files = list((ROOT / 'Saved/CalderWork/runs' / suffix / 'Saved/HouseExteriorViews').glob(pattern))
    lines += [f'![{label}](<{files[0]}>)' if files and states[phase] == 'PASS' else label + ': current-source capture pending.', '']
lines += ['## Performance and recovery', '',
          '[PERFORMANCE.md](PERFORMANCE.md) reports actual `render_ms` per standard view, with baseline ranges, before/after, full-upgrade A/B and six-light A/B. Every standard view must remain within+5%; incomplete or over-budget evidence does not pass.', '',
          '[ROW_IMPLEMENTATION.patch](ROW_IMPLEMENTATION.patch) contains only Row-owned runtime changes against captured bytes after both owners\' camera instrumentation. [ROW_CAMERAS.patch](ROW_CAMERAS.patch) separately contains the original 14 Row cameras. Recovery creation verifies both patches by applying them to isolated copies and comparing exact resulting bytes.', '',
          ('Recovery is complete: [validated checkpoint](<' + manifest['checkpoint'] + '/manifest.json>).'
           if complete else 'Patch/recovery publication remains pending until every current-source native and manual visual phase passes, followed by strict snapshot validation.'), '',
          'No Fab downloads are required. Existing project kits/materials and runtime geometry are used.']
(OUT / 'STATUS.md').write_text('\n'.join(lines) + '\n')
print('STATUS refreshed; complete=' + str(complete))
