"""Require all three alternating rounds/16 eyes before reporting a median."""
import csv
import json
import math
from pathlib import Path
import statistics
import sys

ROOT = Path(__file__).resolve().parents[2]
RUNS = ROOT / 'Saved/CalderWork/runs'
tag = sys.argv[1]
required = {
    'front_street', 'street_along', 'detail_establishing_corner', 'detail_street_dusk',
    'detail_clocktower_far', 'detail_campus_quad', 'detail_layout_quad', 'detail_layout_gateway',
    'detail_stadium_gate', 'detail_layout_aerial', 'detail_layout_dormrow',
    'detail_perf_in_lib', 'detail_perf_in_harlow', 'detail_perf_in_science',
    'detail_perf_in_union', 'detail_perf_in_stadium',
}
interiors_only = '--interiors' in sys.argv[2:]
if interiors_only:
    required = {name for name in required if name.startswith('detail_perf_in_')}
values = {}
for mode in ['on', 'off']:
    values[mode] = {name: [] for name in required}
    for round_number in [1, 2, 3]:
        folder = RUNS / (tag + '_' + mode + str(round_number))
        path = folder / 'Saved/HouseExteriorViews/frametimes.csv'
        with path.open() as stream:
            rows = list(csv.DictReader(stream))
        found = [r['view'] for r in rows]
        if len(found) != len(set(found)) or not required.issubset(found):
            raise RuntimeError('Incomplete/duplicate eyes in ' + str(path) + ': missing ' + str(sorted(required-set(found))))
        log = (folder / 'Saved/Logs/SeniorSendoff.log').read_text(errors='replace')
        if 'HOUSE_EXTERIOR_VIEWS done' not in log or 'Fatal error:' in log:
            raise RuntimeError('Capture did not finish cleanly: ' + str(folder))
        for row in rows:
            if row['view'] not in required:
                continue
            data = {k: float(row[k]) for k in ['avg_ms', 'game_ms', 'render_ms']}
            if any(not math.isfinite(x) or x < 0 for x in data.values()):
                raise RuntimeError('Invalid timing: ' + str(path))
            values[mode][row['view']].append(data)

result = {}
lines = ['| View | On ms | Off ms | Delta ms | On game/render | Rounds |',
         '|---|---:|---:|---:|---:|---:|']
for name in sorted(required):
    result[name] = {}
    for mode in ['on', 'off']:
        samples = values[mode][name]
        if len(samples) != 3:
            raise RuntimeError('Missing round for ' + name)
        result[name][mode] = {key: statistics.median(r[key] for r in samples) for key in samples[0]}
    on, off = result[name]['on'], result[name]['off']
    delta = on['avg_ms'] - off['avg_ms']
    result[name]['delta_ms'] = delta
    lines.append('| %s | %.2f | %.2f | %+.2f | %.2f / %.2f | 3/3 |' %
                 (name, on['avg_ms'], off['avg_ms'], delta, on['game_ms'], on['render_ms']))
table = '\n'.join(lines) + '\n'
for suffix, contents in [('md', table), ('json', json.dumps(result, indent=2))]:
    output = RUNS / (tag + '_perf.' + suffix)
    output.unlink(missing_ok=True)
    output.write_text(contents)
print(table)
print('CALDER_CHECKPOINT_PERF complete=1 rounds=3 eyes=%d sources=6 focus=%s sceneClockImplementation=pendingWildlife' % (len(required), 'interiors' if interiors_only else 'all'))
