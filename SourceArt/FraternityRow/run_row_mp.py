"""Serialize the five Row peer tests and default hill regression, retaining native proof."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
tag = sys.argv[1]
assert re.fullmatch(r'[A-Za-z0-9_-]+', tag)
receipt_path = ROOT / 'Saved/FraternityRowWork' / (tag + '_mp.json')
assert not receipt_path.exists(), 'Use a fresh tag'
hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
          for folder in ['Source', 'Config'] for p in (ROOT / folder).rglob('*') if p.is_file()}
receipt = {'started_utc': datetime.now(timezone.utc).isoformat(), 'source_sha256': hashes,
           'runs': [], 'passed': False}
crashes = r'Fatal error:|Assertion failed:|appError called:|Unhandled Exception|uncaught signal|Caught signal \d+|Segmentation fault|EXC_BAD_ACCESS|LowLevelFatalError'
for interior in ['R1', 'R2', 'R3', 'R4', 'R5', 'LIB']:
    name = tag + '_' + interior.lower()
    out = ROOT / 'Saved/CalderWork/runs' / name
    assert not out.exists(), 'Never replace prior evidence'
    cmd = ['python3', 'Saved/CalderWork/tools/campus_mp.py', name, '-nosound']
    cmd += ['--walk'] if interior == 'LIB' else ['-CampusMPInterior=' + interior]
    print('START', name, flush=True)
    process = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    (out / 'launcher_receipt.log').write_text(process.stdout + process.stderr)
    result = json.loads((out / 'result.json').read_text())
    logs = {role: (out / (role + '.log')).read_text(errors='replace') for role in ['host', 'client']}
    missing = []
    failures = []
    for role, log in logs.items():
        combined = log + (out / (role + '-stdout.log')).read_text(errors='replace')
        failures += [role + ': ' + line for line in log.splitlines()
                     if re.search(r'\b(?:CAMPUS_MP_FAIL|[A-Z_]+_FAILED)\b', line)]
        if re.search(crashes, combined, re.I):
            failures.append(role + ': crash signature')
    if interior != 'LIB':
        checks = {
            'host': [
                f'HOST_OPEN interior={interior} side={side} leaves=2' for side in ['outside', 'inside']
            ] + [
                f'HOST_LEAVE interior={interior} bothPlayersAway=1 naturalStreaming=1',
                f'HOST_FREED interior={interior} built=0 props=0 clientConfirmed=1',
                f'HOST_REENTRY interior={interior} rebuilt=1 clientPropsAndDoorsMatch=1',
                f'PASS host interior={interior} outsideAndInsideRPC=1 freeAndReentry=1',
            ],
            'client': [
                f'CLIENT_FLOOR interior={interior} stage={stage} supported=1' for stage in ['initial', 'reentry']
            ] + [
                f'CLIENT_CHECK interior={interior} pass={n} built=1' for n in [1, 2]
            ] + [
                f'CLIENT_E interior={interior} side={side} owningClientRPC=1' for side in ['outside', 'inside']
            ] + [
                f'CLIENT_OPEN interior={interior} side={side} leaves=2' for side in ['outside', 'inside']
            ] + [
                f'CLIENT_FREED interior={interior} built=0 props=0 naturalStreaming=1',
                f'PASS client interior={interior} outsideAndInsideRPC=1 freeAndReentry=1 checks=2',
            ],
        }
        for role, tokens in checks.items():
            missing += [role + ': ' + token for token in tokens if 'CAMPUS_MP_ROW_' + token not in logs[role]]
        for side in ['outside', 'inside']:
            pattern = rf'CAMPUS_MP_ROW_CLIENT_APPROACH interior={interior} side={side} .* supported=1 closed=2'
            if not re.search(pattern, logs['client']):
                missing.append('client approach ' + side)
        prop_checks = re.findall(rf'CAMPUS_MP_ROW_CLIENT_CHECK interior={interior} pass=([12]) built=1 props=(\d+) meshed=(\d+) doors=(\d+)', logs['client'])
        if len(prop_checks) != 2 or any(int(x[1]) <= 0 or int(x[1]) != int(x[2]) for x in prop_checks) or prop_checks[0][1:] != prop_checks[-1][1:]:
            failures.append('Row prop/mesh/door counts missing or changed after reentry')
    passed = process.returncode == 0 and result['status'] == 'PASS' and not failures and not missing
    item = {'tag': name, 'interior': interior, 'passed': passed,
            'launcher_exit': process.returncode, 'native_result': result,
            'missing_proof': missing, 'failures': failures}
    receipt['runs'].append(item)
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({k: item[k] for k in ['tag', 'passed', 'missing_proof', 'failures']}), flush=True)
    assert passed, 'Inspect native MP evidence before continuing'
for rel, sha in hashes.items():
    assert hashlib.sha256((ROOT / rel).read_bytes()).hexdigest() == sha, 'Source drift: ' + rel
receipt['passed'] = True
receipt['finished_utc'] = datetime.now(timezone.utc).isoformat()
receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
print('MP PHASE PASS', flush=True)
