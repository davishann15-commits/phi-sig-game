#!/usr/bin/env python3
"""Summarize native evidence without changing the project or its map."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import statistics

ROOT = Path(__file__).resolve().parent
PROJECT = ROOT.parents[1]
VIEWS = ['detail_woods_' + name for name in
         ('edge', '20m', 'deep', 'ravine', 'silhouette', 'house_back')]


def read_run(name):
    directory = ROOT / 'evidence' / name
    metadata = json.loads((directory / 'invocation.json').read_text())
    csv_path = directory / 'Saved/HouseExteriorViews/frametimes.csv'
    timings = {}
    if csv_path.exists():
        with csv_path.open(encoding='utf-8-sig', newline='') as source:
            timings = {row['view']: {key: float(value) for key, value in row.items()
                                    if key != 'view'} for row in csv.DictReader(source)}
    reports = {}
    for path in sorted((directory / 'Saved').glob('*/result.txt')):
        if path.parent.name not in ('WoodsPassReview', 'CampusWoodsReview',
                                   'FraternityBoundaryReview', 'WoodsDetailReview',
                                   'ParkingLotReview', 'LeafWindReview'):
            continue
        contents = path.read_text(encoding='utf-8-sig')
        reports[path.parent.name] = {
            'passed': bool(re.search(r'^PASS\b', contents, re.M)) and
                      not bool(re.search(r'^FAIL\b', contents, re.M)),
            'evidence': str(path.relative_to(ROOT)),
            'summary': [line for line in contents.splitlines() if line.startswith(('PASS', 'FAIL'))]}
    tree_bytes = (directory / 'Saved/WoodsPassReview/original_trees.csv').read_bytes()
    text = (directory / 'runtime.log').read_text(encoding='utf-8-sig', errors='replace')
    lights = re.findall(r'actualExteriorPointAndSpotLights=(\d+)', text)
    return dict(name=name, stage=metadata['stage'], exit_code=metadata['exit_code'],
                resolution=metadata['resolution'], quality=metadata['quality'],
                verified=metadata.get('verified', False), timings=timings, reports=reports,
                original_tree_sha256=hashlib.sha256(tree_bytes).hexdigest(),
                exterior_lights=int(lights[-1]) if lights else None,
                fog_lease_released='WOODS_FOG_NEAR released=1 effective=1000' in text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', nargs='+', required=True)
    parser.add_argument('--final', nargs='+', required=True)
    parser.add_argument('--routes', nargs='+', required=True)
    args = parser.parse_args()
    baselines = [read_run(name) for name in args.baseline]
    finals = [read_run(name) for name in args.final]
    routes = [read_run(name) for name in args.routes]
    runs = baselines + finals + routes
    if not all(run['stage'] == 0 for run in baselines) or not all(
            run['stage'] == 3 for run in finals + routes):
        parser.error('Comparison requires stage 0 baselines and stage 3 finals/routes')
    if len({(tuple(run['resolution']), run['quality']) for run in runs}) != 1:
        parser.error('Resolution and quality must match for every run')
    comparison = []
    for view in VIEWS:
        before = [run['timings'][view]['avg_ms'] for run in baselines]
        after = [run['timings'][view]['avg_ms'] for run in finals]
        delta = statistics.median(after) - statistics.median(before)
        comparison.append(dict(view=view, baseline_avg_samples_ms=before,
                               final_avg_samples_ms=after,
                               baseline_median_ms=round(statistics.median(before), 2),
                               final_median_ms=round(statistics.median(after), 2),
                               delta_ms=round(delta, 2), within_3_ms=delta <= 3.0))
    protected = json.loads((ROOT / 'protected.json').read_text())
    map_hash = hashlib.sha256((PROJECT / 'Content/Story/Maps/Chapter01_House.umap').read_bytes()).hexdigest()
    result = dict(comparison=comparison, runs=runs, map_sha256=map_hash,
                  map_unchanged=map_hash == protected['beforeMapSHA256'],
                  original_trees_identical=len({run['original_tree_sha256'] for run in runs}) == 1,
                  light_limit_passed=all(run['exterior_lights'] is not None and
                                        run['exterior_lights'] <= 53 for run in runs),
                  native_reviews_passed=all(run['verified'] and run['exit_code'] == 0 and
                                            all(r['passed'] for r in run['reports'].values()) for run in runs),
                  performance_passed=all(row['within_3_ms'] for row in comparison))
    result['passed'] = all(result[key] for key in ('map_unchanged', 'original_trees_identical',
                         'light_limit_passed', 'native_reviews_passed', 'performance_passed'))
    (ROOT / 'evidence/final_summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: value for key, value in result.items() if key != 'runs'}, indent=2))
    raise SystemExit(0 if result['passed'] else 1)


if __name__ == '__main__':
    main()
