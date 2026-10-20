"""Compare actual legacy/new tree placements, bodies and component settings.

This checks runtime output; source changes and a successful build alone are not
accepted as proof that the old models disappeared from Chapter01_House.
"""
import csv
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
assert len(sys.argv) == 3, 'compare_map_trees.py <original-run> <replacement-run>'
run_names = sys.argv[1:]
for name in run_names:
    assert re.fullmatch(r'[A-Za-z0-9_-]+', name), name
runs = [ROOT/'Saved/CalderWork/runs'/name for name in run_names]
dirs = [run/'Saved/CalderMapTreesReview' for run in runs]
reports = [(directory/'result.txt').read_text() for directory in dirs]
assert all(report.startswith('PASS ') for report in reports), reports
assert 'enabled=0 ' in reports[0] and 'enabled=1 ' in reports[1]
assert 'legacyBatches=0 ' in reports[1] and 'legacyTownSections=0 ' in reports[1]
for name in ['poses.csv', 'bodies.csv']:
    assert (dirs[0]/name).read_bytes() == (dirs[1]/name).read_bytes(), name
settings = []
for directory in dirs:
    with (directory/'settings.csv').open() as stream:
        settings.append({row['batch']: row for row in csv.DictReader(stream)})
assert set(settings[0]) == set(settings[1]), 'Tree batch roster changed'
preserved = ['count', 'collision', 'cullStart', 'cullEnd', 'shadow', 'distanceField', 'indirect', 'boxes']
for name, before in settings[0].items():
    after = settings[1][name]
    assert all(before[key] == after[key] for key in preserved), (name, before, after)
    assert after['lods'] == '3' and int(after['tri0']) > int(after['tri1']) > int(after['tri2']) > 0, (name, after)
logs = [(run/'Saved/Logs/SeniorSendoff.log').read_text() for run in runs]
assert all('crc=0f3b2bb7' in log for log in logs), 'Original planting CRC'
assert 'CALDER_MAP_TREE_FAILED' not in logs[1] and 'CALDER_TREE_FIT_REJECT' not in logs[1]
assert logs[1].count('CALDER_TOWN_TREE foot=') == 280, 'Incomplete baked town trees'
town_positions = re.findall(r'CALDER_TOWN_TREE foot=([^\r\n]+)', logs[1])
receipt = {
    'passed': True, 'runs': run_names, 'tree_batches': len(settings[1]),
    'tree_instances': sum(int(row['count']) for row in settings[1].values()),
    'poses_identical': True, 'bodies_identical': True, 'component_settings_identical': preserved,
    'poses_sha256': hashlib.sha256((dirs[1]/'poses.csv').read_bytes()).hexdigest(),
    'bodies_sha256': hashlib.sha256((dirs[1]/'bodies.csv').read_bytes()).hexdigest(),
    'baked_town_trees': len(town_positions), 'all_replacement_lods_nonlegacy': True,
    'native_reports': reports, 'full_master_complete': False,
    'visual_review_and_performance': 'Separate required checks; not implied by this receipt',
}
destination = ROOT/'Saved/CalderWork/map_tree_replacement/physical_comparison.json'
destination.unlink(missing_ok=True)
destination.write_text(json.dumps(receipt, indent=2)+'\n')
print(json.dumps(receipt, indent=2))
