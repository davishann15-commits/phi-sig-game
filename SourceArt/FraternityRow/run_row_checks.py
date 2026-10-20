"""Run one explicitly selected, serialized native acceptance phase after Row integration.

The caller owns the shared engine slot. Unique output names are mandatory because
the project launcher replaces its output directory. This script never edits Source.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse
import hashlib
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'Saved/FraternityRowWork'
RUNS = ROOT / 'Saved/CalderWork/runs'
parser = argparse.ArgumentParser()
parser.add_argument('phase', choices=['scenes', 'smoke', 'interiors', 'yards', 'captures', 'lot_controls', 'regressions'])
parser.add_argument('tag', help='Unique prefix, for example row_after_20261018_r1')
parser.add_argument('--tree-baseline', type=Path)
args = parser.parse_args()
ROW_VIEWS = {'detail_row_' + name for name in [
    'road_wide', 'R1_porch', 'R2_porch', 'R3_porch', 'R1_yard', 'R2_yard', 'R3_yard',
    'infill', 'maple_gap', 'oak_edge', 'rear_fence', 'gate', 'former_parking', 'house_connection',
    'R5_lot_front', 'R5_lot_rear']}
assert re.fullmatch(r'[A-Za-z0-9_-]+', args.tag), 'Unsafe run prefix'
assert (WORK / 'integration_receipt.json').exists(), 'Integrate and build reviewed candidates first'
if args.phase == 'yards':
    assert args.tree_baseline and args.tree_baseline.is_file(), 'Use the prior legacy scene tree JSON'
    control = json.loads(args.tree_baseline.read_text())
    assert control['capture_valid'] and control['upgrade_enabled'] is False

receipt_path = WORK / (args.tag + '_' + args.phase + '.json')
assert not receipt_path.exists(), 'Choose a fresh tag; retain prior evidence'
source_hashes = {
    str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
    for folder in ['Source', 'Config'] for p in (ROOT / folder).rglob('*') if p.is_file()
}
receipt = {'started_utc': datetime.now(timezone.utc).isoformat(),
           'phase': args.phase, 'source_sha256': source_hashes, 'runs': [], 'passed': False}


def game(suffix, seconds, required, *flags, baseline_failure_pattern=None):
    tag = args.tag + '_' + suffix
    out = RUNS / tag
    assert not out.exists(), 'Existing run output: ' + tag
    command = ['bash', 'Saved/CalderWork/tools/run_game.sh', tag, str(seconds),
               '-RenderOffScreen', '-nosound', *flags]
    print('START', tag, flush=True)
    process = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    log = out / 'Saved/Logs/SeniorSendoff.log'
    text = log.read_text(errors='replace') if log.exists() else ''
    missing = [token for token in required if token not in text]
    failures = [line for line in text.splitlines()
                if 'LogTemp' in line and re.search(r'\b(?:FAIL|[A-Z_]+_FAILED)\b', line)]
    stdout_path = out / 'stdout.log'
    native_stdout = stdout_path.read_text(errors='replace') if stdout_path.exists() else ''
    crash_patterns = [r'Fatal error:', r'Assertion failed:', r'appError called:',
                      r'Unhandled Exception', r'uncaught signal', r'Caught signal \d+',
                      r'terminating with uncaught exception', r'Segmentation fault',
                      r'EXC_BAD_ACCESS', r'LowLevelFatalError']
    crash_signatures = [pattern for pattern in crash_patterns
                        if re.search(pattern, text + '\n' + native_stdout, re.IGNORECASE)]
    preserved = bool(baseline_failure_pattern and len(failures) == 1
                     and re.search(baseline_failure_pattern, failures[0]))
    native_pass = not missing and not failures and not crash_signatures
    passed = (process.returncode == 0 and (native_pass or preserved)
              and not crash_signatures and 'TIMEOUT' not in process.stdout)
    result = {'tag': tag, 'passed': passed, 'wrapper_exit': process.returncode,
              'native_pass': native_pass, 'preserved_baseline_failure': preserved,
              'crash_signatures': crash_signatures,
              'missing_verdicts': missing, 'failure_lines': failures,
              'saved': str(out / 'Saved'), 'launcher': process.stdout.strip()}
    receipt['runs'].append(result)
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(result, indent=2), flush=True)
    assert passed, 'Native acceptance failed; inspect and repair before continuing'
    return out / 'Saved'


if args.phase == 'scenes':
    legacy = game('legacy', 300, ['FRATERNITY_ROW_REVIEW PASS', 'WOODS_PASS_REVIEW PASS'],
                  '-NoFraternityRowUpgrade', '-FraternityRowReview', '-WoodsPassReview')
    baseline = legacy / 'FraternityRowReview/actual_tree_instances.json'
    control = json.loads(baseline.read_text())
    assert control['capture_valid'] and control['upgrade_enabled'] is False
    receipt['tree_baseline'] = str(baseline)
    for suffix, flags in [('scene', []), ('no_row_lights', ['-NoFraternityRowLights'])]:
        game(suffix, 300, ['FRATERNITY_ROW_REVIEW PASS', 'TREE_BASELINE PASS', 'CANOPY_ENVELOPE PASS', 'WOODS_PASS_REVIEW PASS'],
             '-FraternityRowReview', '-WoodsPassReview', '-FraternityRowTreeBaseline=' + str(baseline), *flags)
elif args.phase == 'smoke':
    game('row_smoke', 600, ['CAMPUS_SMOKE PASS passed=5/5'], '-CampusInteriorsSmoke=R1,R2,R3,R4,R5')
elif args.phase == 'interiors':
    game('row_interiors', 1800, ['CAMPUS_INTERIORS_REVIEW PASS buildings=R1,R2,R3,R4,R5'],
         '-CampusInteriorsReview=R1,R2,R3,R4,R5')
elif args.phase == 'yards':
    game('yard_walk', 900, ['FRATERNITY_ROW_REVIEW PASS yards=5/5', 'TREE_BASELINE PASS', 'CANOPY_ENVELOPE PASS'],
         '-FraternityRowYardsWalkReview', '-WoodsPassReview',
         '-FraternityRowTreeBaseline=' + str(args.tree_baseline.resolve()))
elif args.phase == 'regressions':
    for suffix, seconds, required, flags in [
        ('terrain', 900, ['CALDER_TERRAIN_REVIEW PASS'], ['-CalderTerrainReview']),
        ('roam', 1800, ['FREE_ROAM_REVIEW PASS'], ['-FreeRoamWalkReview']),
        ('parking_walk', 600, ['PARKING_LOT_REVIEW PASS'], ['-ParkingLotWalkReview']),
        ('woods_walk', 600, ['WOODS_REVIEW PASS'], ['-WoodsWalkReview']),
        ('field_walk', 600, ['WEST_FIELD_REVIEW PASS'], ['-WestFieldWalkReview']),
        ('trees', 600, ['TREES_REVIEW PASS'], ['-HouseTreesReview']),
        ('all_smoke', 1800, ['CAMPUS_SMOKE PASS passed=23/23'], ['-CampusInteriorsSmoke=All']),
        ('intA', 1800, ['CAMPUS_INTERIORS_REVIEW PASS buildings=LIB,HAR,SCI,UNI,STA,TWR'],
         ['-CampusInteriorsReview=LIB,HAR,SCI,UNI,STA,TWR']),
        ('intBC', 1800, ['CAMPUS_INTERIORS_REVIEW PASS buildings=MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU'],
         ['-CampusInteriorsReview=MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU']),
        ('traversal', 600, ['HOUSE_TRAVERSAL_PASSED'], ['-HouseTraversalSmoke']),
    ]:
        game(suffix, seconds, required, *flags)
    before = json.loads((WORK / 'before_checks_receipt.json').read_text())
    by_suffix = {run['tag'].removeprefix('row_current_20261018_'): run for run in before['runs']}
    # The brief permits an unchanged pre-existing verdict. These exact native
    # failures were recorded before integration, never inferred from launcher0.
    comparisons = [
        ('exteriorwalk', 'EXTERIOR_WALK_REVIEW PASS', '-HouseExteriorWalkReview',
         r'EXTERIOR_WALK_REVIEW FAIL Front site surface incorrect at 800,-4025: z=-57\.0 expected=-57\.0 EX_Concrete material=MID_M_House_Textured_\d+ section=\d+ face=\d+$'),
        ('house', 'COMBINED_HOUSE_SMOKE_PASSED', '-CombinedHouseSmoke',
         r'COMBINED_HOUSE_SMOKE_FAILED: Mouse look direction failed \(step 2, pitch 0\.00\)$'),
        ('doors', 'EXISTING_HOUSE_DOOR_PASSED', '-ExistingHouseDoorSmoke',
         r'EXISTING_HOUSE_DOOR_FAILED: An extracted original leaf asset is missing$'),
    ]
    for suffix, pass_token, flag, pattern in comparisons:
        old_failures = by_suffix[suffix]['native_failures']
        assert len(old_failures) == 1 and re.search(pattern, old_failures[0]), 'Unexpected baseline verdict'
        game(suffix, 600, [pass_token], flag, baseline_failure_pattern=pattern)
elif args.phase == 'captures':
    # Capture completion owns the process exit; static reviews may not exit early.
    for suffix, night in [('dusk', '0'), ('dark', '1800')]:
        reviews = ['CampusLayoutReview', 'CampusWoodsReview', 'FraternityBoundaryReview',
                   'SurroundingsPolishReview', 'ParkingLotReview', 'WoodsPassReview',
                   'WestFieldReview', 'WoodsSceneReview'] if suffix == 'dusk' else ['WoodsPassReview']
        required = ['WOODS_PASS_REVIEW PASS']
        if suffix == 'dusk':
            required += ['CAMPUS_LAYOUT_REVIEW PASS', 'CAMPUS_WOODS_REVIEW PASS',
                         'FRATERNITY_BOUNDARY_REVIEW PASS', 'SURROUNDINGS_POLISH_REVIEW PASS',
                         'PARKING_LOT_REVIEW PASS', 'WEST_FIELD_REVIEW PASS', 'WOODS_REVIEW PASS']
        saved = game(suffix, 600, required, '-HouseExteriorViews', '-CleanViews',
                     '-ExteriorViewFilter=detail_row_', '-ExteriorViewWarmup=2', '-ExteriorViewSamples=60',
                     '-CalderNightfallSeconds=' + night, *['-' + flag for flag in reviews])
        folder = saved / 'HouseExteriorViews'
        import csv
        with (folder / 'frametimes.csv').open() as stream:
            rows = list(csv.DictReader(stream))
        assert len(rows) == 16 and {row['view'] for row in rows} == ROW_VIEWS
        assert len(list(folder.glob('*detail_row_*.png'))) == 16
elif args.phase == 'lot_controls':
    # Two appended cameras for the relocated house. The original fourteen paired
    # before/after poses remain unchanged; these extra controls use legacy geometry.
    for suffix, night in [('lot_legacy_dusk', '0'), ('lot_legacy_dark', '1800')]:
        saved = game(suffix, 600, ['WOODS_PASS_REVIEW PASS'], '-NoFraternityRowUpgrade',
                     '-HouseExteriorViews', '-CleanViews', '-WoodsPassReview',
                     '-ExteriorViewFilter=detail_row_R5_lot_', '-ExteriorViewWarmup=2',
                     '-ExteriorViewSamples=60', '-CalderNightfallSeconds=' + night)
        folder = saved / 'HouseExteriorViews'
        import csv
        with (folder / 'frametimes.csv').open() as stream:
            rows = list(csv.DictReader(stream))
        assert len(rows) == 2 and {row['view'] for row in rows} == {'detail_row_R5_lot_front', 'detail_row_R5_lot_rear'}
        assert len(list(folder.glob('*detail_row_R5_lot_*.png'))) == 2

for rel, sha in source_hashes.items():
    assert hashlib.sha256((ROOT / rel).read_bytes()).hexdigest() == sha, 'Source drift during acceptance: ' + rel
receipt['passed'] = True
receipt['finished_utc'] = datetime.now(timezone.utc).isoformat()
receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
print('PHASE PASS', args.phase, flush=True)
