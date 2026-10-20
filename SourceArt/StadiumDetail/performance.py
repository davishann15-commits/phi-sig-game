"""Three alternating300-sample stadium on/off pairs at one frozen Row/source state.

No build, import, source edits or acceptance from an older53-light scene. All named
poses must exist in every run. The result fails if any measured view exceeds1.5ms
or the native median LoadMap delta exceeds1second.
"""
from pathlib import Path
from datetime import datetime, timezone
from statistics import median
import argparse, csv, json, re
from stage_runner import ROOT, WORK, RUNS, NAMES, manifest, idle, native, sha


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('tag')
    parser.add_argument('--capture-tag', required=True, help='Actual passed, inspected current-stage physical capture, kept separate from frame timing')
    parser.add_argument('--stage', type=int, default=7, choices=range(1, 8))
    parser.add_argument('--scene-time', type=float, help='Existing shared wildlife-cycle control on BOTH variants; keeps native FApp wallclock and all species enabled')
    args = parser.parse_args()
    assert re.fullmatch(r'[a-z0-9_]+', args.tag)
    grant = json.loads((WORK / 'integration_grant.json').read_text())
    assert grant['granted'] and grant['scope'] == 'full' and grant.get('release_evidence')
    idle()
    frozen = manifest()
    capture_path = WORK / (args.capture_tag + '_capture.json')
    visual_path = WORK / (args.capture_tag + '_visual_inspection.json')
    capture = json.loads(capture_path.read_text())
    visual = json.loads(visual_path.read_text())
    assert capture['stage'] == visual['stage'] == args.stage
    assert capture['source_config_map_sha'] == visual['source_config_map_sha'] == frozen
    assert visual['capture_receipt_sha'] == sha(capture_path)
    assert visual['all_images_inspected'] and visual['accepted_for_stage']
    assert len(capture['runs']) == 2
    assert all(any('CALDER_STADIUM_REVIEW PASS enabled=1' in row for row in run['physical_review']) for run in capture['runs'])
    destination = WORK / (args.tag + '_performance.json')
    assert not destination.exists()
    data = {'on': [], 'off': []}
    with (WORK / (args.tag + '_performance.log')).open('x') as output:
        for round_number in (1, 2, 3):
            for mode in ('on', 'off'):
                assert manifest() == frozen
                flags = ['-HouseExteriorViews', '-CleanViews',
                    '-ExteriorViewFilter=' + ','.join(NAMES), '-ExteriorViewSamples=300',
                    '-ExteriorViewWarmup=4', '-CalderNightfallSeconds=0',
                    '-ExecCmds=t.MaxFPS 0']
                if args.scene_time is not None:
                    flags += ['-CalderSceneTime=' + str(args.scene_time)]
                if mode == 'off':
                    flags += ['-NoStadiumDetail']
                # Physical scans affect sampled frames; their identical-source captures passed above.
                # Movement reviewers run separately in the full unchanged review chain.
                tag = args.tag + '_' + mode + str(round_number)
                log = native(tag, flags, output)
                assert 'HOUSE_EXTERIOR_VIEWS done' in log and 'CALDER_STADIUM_REVIEW' not in log
                if args.scene_time is not None:
                    assert 'sceneTime=' + format(args.scene_time % 1800, '.3f') in log
                    assert 'NoCalderWildlife' not in ' '.join(flags) and 'NoCalderBirds' not in ' '.join(flags)
                if mode == 'on':
                    assert 'CALDER_STADIUM built step=' + str(args.stage) + ' ' in log
                else:
                    assert 'CALDER_STADIUM built step=' not in log
                rows = list(csv.DictReader((RUNS / tag / 'Saved/HouseExteriorViews/frametimes.csv').open()))
                assert [r['view'] for r in rows] == NAMES
                times = re.findall(r'Took ([0-9.]+) seconds to LoadMap\(/Game/Story/Maps/Chapter01_House\)', log)
                assert len(times) == 1
                data[mode].append({'tag': tag, 'round': round_number, 'load_seconds': float(times[0]), 'rows': rows})
                (WORK / (args.tag + '_performance_progress.json')).write_text(json.dumps(data, indent=2) + '\n')
    idle()
    assert manifest() == frozen
    comparisons = []
    for index, name in enumerate(NAMES):
        result = {'view': name}
        for metric in ('avg_ms', 'render_ms', 'game_ms'):
            on = [float(run['rows'][index][metric]) for run in data['on']]
            off = [float(run['rows'][index][metric]) for run in data['off']]
            result[metric] = {'on_median': median(on), 'off_median': median(off),
                'delta': median(on) - median(off), 'paired_delta_median': median(a-b for a, b in zip(on, off))}
        result['passed'] = all(result[k]['delta'] <= 1.5 and result[k]['paired_delta_median'] <= 1.5 for k in ('avg_ms', 'render_ms'))
        comparisons.append(result)
    load_on = median(run['load_seconds'] for run in data['on'])
    load_off = median(run['load_seconds'] for run in data['off'])
    result = {'finished_utc': datetime.now(timezone.utc).isoformat(), 'stage': args.stage,
        'source_config_map_sha': frozen, 'pairs': data, 'views': comparisons,
        'evidence_files': [str((WORK / (args.tag + '_performance.log')).relative_to(ROOT)),
                           str((WORK / (args.tag + '_performance_progress.json')).relative_to(ROOT))],
        'samples': 300, 'warmup_seconds': 4, 'alternating_rounds': 3, 'metal_offscreen_only': True,
        'physical_audits_separate_from_frame_samples': True,
        'common_wildlife_scene_time': args.scene_time,
        'native_fapp_render_wallclock_unchanged': True,
        'all_species_retained': True,
        'physical_capture_receipt': str(capture_path.relative_to(ROOT)),
        'physical_visual_receipt': str(visual_path.relative_to(ROOT)),
        'shared_row_state_unchanged': True, 'load_seconds': {'on': load_on, 'off': load_off, 'delta': load_on-load_off},
        'passed': all(r['passed'] for r in comparisons) and load_on-load_off <= 1.0,
        'scope': 'Final stadium performance acceptance' if args.stage == 7 else 'Intermediate-stage performance; later geometry still needs final acceptance'}
    destination.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS' if result['passed'] else 'FAIL', destination, 'load delta', round(load_on-load_off, 3))
    assert result['passed'], 'Retain the evidence and repair the measured stadium overhead before acceptance.'


if __name__ == '__main__':
    main()
