"""Native stadium-disabled restoration controls. No changes to original reviewers."""
from pathlib import Path
from datetime import datetime, timezone
import argparse, json, re
from stage_runner import ROOT, WORK, RUNS, idle, manifest, native, review_result


def exterior_counts(log):
    rows = [line for line in log.splitlines() if 'HOUSE_EXTERIOR built tris=' in line]
    assert len(rows) == 1
    fields = dict(re.findall(r'\b(tris|instances|lights|house|grounds|near|far|clockTower|campusFall|campusPolish|pools)=(\d+)\b', rows[0]))
    assert len(fields) == 11, fields
    return fields


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('tag')
    parser.add_argument('--released-row-log', type=Path, required=True,
                        help='Actual pre-stadium R7 upgraded-scene native log from the released Row evidence.')
    args = parser.parse_args()
    assert re.fullmatch(r'[a-z0-9_]+', args.tag)
    grant = json.loads((WORK / 'integration_grant.json').read_text())
    assert grant['granted'] and grant['scope'] == 'full' and grant['release_evidence']
    release = json.loads((ROOT / 'Saved/FraternityRowWork/reservation_release_full_to_stadium.json').read_text())
    assert release['row_state'] == 'paused_unfinished' and release['no_further_row_jobs_or_edits']
    assert 'row_after_20261018_r7_' in str(args.released_row_log)
    before_log = args.released_row_log.read_text(errors='replace')
    assert 'CALDER_STADIUM built step=' not in before_log
    before_counts = exterior_counts(before_log)
    assert before_counts['lights'] == '57'
    frozen = manifest()
    result = {'source_config_map_sha': frozen, 'passed': False, 'run_tags': [],
              'released_row_counts': before_counts, 'controls': {},
              'evidence_files': [str(args.released_row_log.resolve().relative_to(ROOT))]}
    destination = WORK / (args.tag + '_exact_controls.json')
    assert not destination.exists()
    idle()
    with (WORK / (args.tag + '_exact_controls.log')).open('x') as output:
        for mode, extra, lights, floors in (
            ('row57', [], 57, 8493),
            ('original53', ['-NoFraternityRowUpgrade'], 53, 8493),
            ('original_no_terrain53', ['-NoFraternityRowUpgrade', '-NoCalderTerrain'], 53, 2654),
        ):
            assert manifest() == frozen
            tag = args.tag + '_' + mode
            flags = ['-NoStadiumDetail', '-StadiumDetailReview', '-CampusLayoutReview',
                     '-CampusWoodsReview', '-FraternityBoundaryReview', '-SurroundingsPolishReview',
                     '-ParkingLotWalkReview', '-WoodsPassReview'] + extra
            log = native(tag, flags, output)
            verdicts = review_result(RUNS / tag, 'noterrain')
            assert 'CALDER_STADIUM built step=' not in log
            match = re.search(r'CALDER_STADIUM_REVIEW PASS enabled=0 step=\d+ actors=0 components=0 instances=0 newLights=0', log)
            assert match, 'Stadium-disabled actor/component audit did not pass'
            assert 'crc=0f3b2bb7' in log
            assert f'floorSamples={floors} floorMisses=0 walkObstructions=0' in log
            assert f'exteriorLights={lights}' in log
            assert 'stadiumSeatVertices=4110' in log
            counts = exterior_counts(log)
            if mode == 'row57':
                assert counts == before_counts, ('Released Row scene changed with stadium off', counts, before_counts)
            else:
                expected = {'instances': '53615', 'grounds': '72842'} if mode == 'original53' else {
                    'instances': '53740', 'grounds': '69240'}
                assert all(counts[key] == value for key, value in expected.items()), (mode, counts, expected)
                front = 1112052 if mode == 'original53' else 1091586
                assert f'campusFrontVertices={front} ' in log
            result['run_tags'].append(tag)
            result['controls'][mode] = {'passed': True, 'counts': counts, 'original_verdicts': verdicts,
                                       'stadium_disabled_actor_components_zero': True, 'tree_crc': '0f3b2bb7'}
    idle()
    assert manifest() == frozen
    result['passed'] = len(result['controls']) == 3
    result['finished_utc'] = datetime.now(timezone.utc).isoformat()
    result['evidence_files'].append(str((WORK / (args.tag + '_exact_controls.log')).relative_to(ROOT)))
    destination.write_text(json.dumps(result, indent=2) + '\n')
    print('STADIUM_EXACT_CONTROLS PASS', destination)


if __name__ == '__main__':
    main()
