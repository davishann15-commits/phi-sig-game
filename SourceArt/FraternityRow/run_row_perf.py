"""Run the required helper twice; accept only complete, crash-free native CSV evidence."""
from pathlib import Path
from datetime import datetime, timezone
import argparse
import csv
import hashlib
import json
import math
import re
import statistics
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('tag')
parser.add_argument('--stop-after-full-failure', action='store_true',
                    help='Stop after six validated full runs if their render budget fails; final acceptance still requires both controls.')
args = parser.parse_args()
tag = args.tag
assert re.fullmatch(r'[A-Za-z0-9_-]+', tag)
WORK = ROOT / 'Saved/FraternityRowWork'
receipt_path = WORK / (tag + '_perf.json')
assert not receipt_path.exists(), 'Use a fresh tag'
hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
          for folder in ['Source', 'Config'] for p in (ROOT / folder).rglob('*') if p.is_file()}

def check_source_freeze():
    current = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
               for folder in ['Source', 'Config'] for p in (ROOT / folder).rglob('*') if p.is_file()}
    assert current == hashes, 'Source/Config drift (including added or removed files)'

receipt = {'started_utc': datetime.now(timezone.utc).isoformat(), 'source_sha256': hashes,
           'controls': [], 'native_complete': False, 'budget_passed': False}
view_order = ['front_street', 'street_along', 'detail_establishing_corner', 'detail_street_dusk',
              'detail_clocktower_far', 'detail_stadium_gate', 'detail_campus_quad', 'detail_layout_gateway',
              'detail_layout_quad', 'detail_layout_aerial', 'detail_layout_dormrow']
views = set(view_order)

def full_budget_preflight(samples, runs):
    # Match performance_report.py exactly: six unchanged baseline means and three means per full mode.
    baseline = {view: [] for view in view_order}
    csv_files, csv_hashes = [], {}
    for mode in ['on', 'off']:
        for round_id in range(1, 4):
            path = ROOT / 'Saved/CalderWork/runs' / f'row_before_20261018_r1_{mode}{round_id}' / 'Saved/HouseExteriorViews/frametimes.csv'
            rows = list(csv.DictReader(path.open()))
            assert len(rows) == 11 and {row['view'] for row in rows} == views, 'Incomplete baseline CSV: ' + str(path)
            assert all(math.isfinite(float(row['render_ms'])) and float(row['render_ms']) > 0 for row in rows), 'Invalid baseline render time'
            for row in rows:
                baseline[row['view']].append(float(row['render_ms']))
            csv_files.append(str(path))
            csv_hashes[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    for run in runs:
        path = ROOT / 'Saved/CalderWork/runs' / run['tag'] / 'Saved/HouseExteriorViews/frametimes.csv'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == run['csv_sha256'], 'Validated full CSV changed: ' + run['tag']
        csv_files.append(str(path))
        csv_hashes[str(path)] = run['csv_sha256']
    rows = []
    for view in view_order:
        base = statistics.median(baseline[view])
        on = statistics.median(samples['on'][view])
        off = statistics.median(samples['off'][view])
        rows.append({'view': view, 'baseline_ms': base, 'baseline_min': min(baseline[view]),
                     'baseline_max': max(baseline[view]), 'on_ms': on, 'off_ms': off,
                     'before_to_after_pct': 100 * (on / base - 1), 'upgrade_ab_pct': 100 * (on / off - 1)})
    return {'metric': 'render_ms',
            'sample_unit': 'per-run mean, median of three alternating rounds; baseline median of six unchanged observations',
            'rows': rows, 'csv_files': csv_files, 'csv_sha256': csv_hashes,
            'passes_before_budget': all(row['before_to_after_pct'] <= 5 for row in rows),
            'passes_upgrade_ab_budget': all(row['upgrade_ab_pct'] <= 5 for row in rows)}

crashes = r'Fatal error:|Assertion failed:|appError called:|Unhandled Exception|uncaught signal|Caught signal \d+|Segmentation fault|EXC_BAD_ACCESS|LowLevelFatalError'
for suffix, switch in [('full', '-NoFraternityRowUpgrade'), ('lights', '-NoFraternityRowLights')]:
    prefix = tag + '_' + suffix
    for mode in ['on', 'off']:
        for round_id in range(1, 4):
            assert not (ROOT / 'Saved/CalderWork/runs' / f'{prefix}_{mode}{round_id}').exists(), 'Never replace prior evidence'
    print('START', prefix, switch, flush=True)
    process = subprocess.run(['bash', 'Saved/CalderWork/tools/perf_ab.sh', prefix, switch],
                             cwd=ROOT, capture_output=True, text=True)
    (WORK / (prefix + '_launcher.log')).write_text(process.stdout + process.stderr)
    runs = []
    samples = {mode: {view: [] for view in view_order} for mode in ['on', 'off']}
    for mode in ['on', 'off']:
        for round_id in range(1, 4):
            name = f'{prefix}_{mode}{round_id}'
            out = ROOT / 'Saved/CalderWork/runs' / name
            log = (out / 'Saved/Logs/SeniorSendoff.log').read_text(errors='replace')
            stdout = (out / 'stdout.log').read_text(errors='replace')
            csv_path = out / 'Saved/HouseExteriorViews/frametimes.csv'
            rows = list(csv.DictReader(csv_path.open()))
            failures = [line for line in log.splitlines()
                        if 'LogTemp' in line and re.search(r'\b(?:FAIL|[A-Z_]+_FAILED)\b', line)]
            assert not failures and not re.search(crashes, log + stdout, re.I), name + ': native failure/crash'
            assert len(rows) == 11 and {row['view'] for row in rows} == views, name + ': incomplete CSV'
            assert len(list((out / 'Saved/HouseExteriorViews').glob('*.png'))) == 11, name + ': incomplete screenshots'
            assert all(math.isfinite(float(row['render_ms'])) and float(row['render_ms']) > 0 for row in rows)
            for row in rows:
                samples[mode][row['view']].append(float(row['render_ms']))
            assert 'HOUSE_EXTERIOR_VIEWS done' in log, name + ': no native completion'
            done = log.rfind('HOUSE_EXTERIOR_VIEWS done')
            assert 'LogExit: Exiting.' in log[done:] and 'Log file closed' in log[done:], name + ': no clean native shutdown'
            runs.append({'tag': name, 'csv_sha256': hashlib.sha256(csv_path.read_bytes()).hexdigest(),
                         'views': len(rows), 'native_failures': failures})
    assert process.returncode == 0, 'Performance helper failed'
    receipt['controls'].append({'prefix': prefix, 'switch': switch, 'runs': runs})
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    print('NATIVE COMPLETE', prefix, flush=True)
    if suffix == 'full' and args.stop_after_full_failure:
        preflight = full_budget_preflight(samples, runs)
        check_source_freeze()
        receipt['full_budget_preflight'] = preflight
        if not (preflight['passes_before_budget'] and preflight['passes_upgrade_ab_budget']):
            receipt['finished_utc'] = datetime.now(timezone.utc).isoformat()
            receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
            print('FULL BUDGET FAILED; retained six full runs; light runs and final acceptance remain incomplete', flush=True)
            raise SystemExit(2)
        receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
check_source_freeze()
receipt['native_complete'] = True
report = subprocess.run(['python3', 'SourceArt/FraternityRow/performance_report.py', tag + '_full', tag + '_lights'], cwd=ROOT)
metrics = json.loads((ROOT / 'SourceArt/FraternityRow/PERFORMANCE.json').read_text())
receipt['budget_passed'] = metrics['passes_before_budget'] and metrics['passes_upgrade_ab_budget']
receipt['finished_utc'] = datetime.now(timezone.utc).isoformat()
receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
assert report.returncode == 0 and receipt['budget_passed'], 'Render budget exceeded; inspect per-view measurements'
print('PERFORMANCE PHASE PASS', flush=True)
