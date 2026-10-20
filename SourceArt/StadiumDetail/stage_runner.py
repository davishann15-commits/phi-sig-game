"""Serialized native stadium evidence. Requires the later explicit FULL Row release.

capture: fresh dusk/night28-view sweeps plus the additive physical review.
reviews: full, unmodified fix_checks chain after the actual captures were inspected.
This script does not edit Source, change stage defaults or grant its own reservation.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse, csv, hashlib, json, os, re, signal, subprocess, time

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'Saved/StadiumDetailWork'
RUNS = ROOT / 'Saved/CalderWork/runs'
PARTS = 'layout terrain roam reviews clock smoke intA intBC noterrain reach_off reach'.split()
NAMES = json.loads((WORK / 'before_receipt.json').read_text())['expected_views'] + [
    'detail_stadium_portal_east', 'detail_stadium_portal_west'] + [
    view['name'] for view in json.loads((ROOT / 'SourceArt/StadiumDetail/field_close_views.json').read_text())['views']] + [
    view['name'] for view in json.loads((ROOT / 'SourceArt/StadiumDetail/concourse_close_views.json').read_text())['views']] + [
    view['name'] for view in json.loads((ROOT / 'SourceArt/StadiumDetail/interior_close_views.json').read_text())['views']] + [
    view['name'] for view in json.loads((ROOT / 'SourceArt/StadiumDetail/program_close_view.json').read_text())['views']] + [
    view['name'] for view in json.loads((ROOT / 'SourceArt/StadiumDetail/CoachOffice/office_views.json').read_text())['views']]
COMMON = ['CAMPUS_WOODS_REVIEW', 'FRATERNITY_BOUNDARY_REVIEW',
          'SURROUNDINGS_POLISH_REVIEW', 'PARKING_LOT_REVIEW', 'WOODS_PASS_REVIEW']
REQUIRED = {'layout': ['CAMPUS_LAYOUT_REVIEW'], 'terrain': ['CALDER_TERRAIN_REVIEW', 'CALDER_WATER_REVIEW'],
            'roam': ['FREE_ROAM_REVIEW'], 'reviews': COMMON, 'clock': ['CLOCK_TOWER_REVIEW'],
            'smoke': ['CAMPUS_SMOKE'], 'intA': ['CAMPUS_INTERIORS_REVIEW'],
            'intBC': ['CAMPUS_INTERIORS_REVIEW'], 'noterrain': ['CAMPUS_LAYOUT_REVIEW'] + COMMON,
            'reach_off': ['CALDER_REACH_REVIEW'], 'reach': ['CALDER_REACH_REVIEW']}
RESULTS = {'layout': ['CampusLayoutReview'], 'terrain': ['CalderTerrainReview'],
           'roam': ['FreeRoamWalkReview'], 'reviews': ['CampusWoodsReview', 'FraternityBoundaryReview',
           'SurroundingsPolishReview', 'ParkingLotReview', 'WoodsPassReview'], 'clock': ['ClockTowerReview'],
           'smoke': ['CampusInteriorsSmoke'], 'intA': ['CampusInteriorsReview'],
           'intBC': ['CampusInteriorsReview'], 'noterrain': ['CampusLayoutReview', 'CampusWoodsReview',
           'FraternityBoundaryReview', 'SurroundingsPolishReview', 'ParkingLotReview', 'WoodsPassReview'],
           'reach_off': ['CalderReachReview'], 'reach': ['CalderReachReview']}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def manifest():
    paths = []
    for folder in ('Source', 'Config'):
        paths += [p for p in (ROOT / folder).rglob('*') if p.is_file()]
    paths += [ROOT / 'SeniorSendoff.uproject', ROOT / 'Content/Story/Maps/Chapter01_House.umap']
    textures = ROOT / 'Content/SeniorSendOff/Calder/Stadium'
    if textures.exists():
        paths += [p for p in textures.rglob('*') if p.is_file()]
    return {str(p.relative_to(ROOT)): sha(p) for p in sorted(paths)}


def idle():
    rows = subprocess.check_output(['ps', '-axo', 'pid=,comm='], text=True).splitlines()
    active = [s for s in rows if any(x in s for x in (
        'UnrealEditor.app/Contents/MacOS/UnrealEditor', 'UnrealBuildTool', 'Binaries/Mac/SeniorSendoff'))]
    assert not active, 'An engine/build is active; keep the shared reservation serialized: ' + repr(active)


def native(tag, flags, output):
    assert not (RUNS / tag).exists(), 'Refusing to overwrite native evidence: ' + tag
    idle()
    command = ['bash', 'Saved/CalderWork/tools/run_game.sh', tag, '900',
               '-RenderOffScreen', '-MetalOffscreenOnly', '-nosound'] + flags
    output.write('\nSTART ' + repr(command) + '\n')
    output.flush()
    subprocess.run(command, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT, check=True)
    log = (RUNS / tag / 'Saved/Logs/SeniorSendoff.log').read_text(errors='replace')
    assert 'Fatal error' not in log and 'Assertion failed' not in log
    assert not re.search(r'REVIEW FAIL|CALDER_STADIUM_REVIEW FAIL', log), tag
    return log


def review_result(folder, part):
    """Read all native verdicts. A wrapper exit or one unrelated PASS is insufficient."""
    log = (folder / 'Saved/Logs/SeniorSendoff.log').read_text(errors='replace')
    assert not re.search(r'REVIEW FAIL|CAMPUS_SMOKE FAIL|CAMPUS_SMOKE_[A-Z]*_FAILED|Fatal error|Assertion failed', log), part
    lines = []
    for marker in REQUIRED[part]:
        matching = [x for x in log.splitlines() if marker + ' PASS' in x]
        assert len(matching) == 1, (part, marker, len(matching))
        lines += matching
    for name in RESULTS[part]:
        summary = (folder / 'Saved' / name / 'result.txt').read_text()
        assert not re.search(r'^FAIL\b', summary, re.M), (part, name)
        verdicts = re.findall(r'^PASS\b[^\n]*', summary, re.M)
        assert len(verdicts) == 1, (part, name, len(verdicts))
        lines += [name + ': ' + verdicts[0]]
    joined = '\n'.join(lines)
    if part == 'smoke':
        # Released R7 Row source adds R1..R5 to All:18 original campus buildings plus5 approved Row houses.
        assert 'CAMPUS_SMOKE PASS passed=23/23' in joined
        assert 'PASS buildings=23/23' in joined
        ids = re.findall(r'^([A-Z0-9]+): PASS ', (folder / 'Saved/CampusInteriorsSmoke/result.txt').read_text(), re.M)
        assert len(ids) == 23 and set(ids) == set('LIB HAR SCI UNI STA TWR MAIN HLTH COM ASH HUM KER FLD ART MUS ENG PLT ALU R1 R2 R3 R4 R5'.split())
    if part in ('intA', 'intBC'):
        expected = '257/257' if part == 'intA' else '174/174'
        assert 'stops=' + expected in joined and 'problems=0' in joined
        assert 'maxInteriorLights=6 pool=6' in joined
    if part == 'roam':
        assert all(x in joined for x in ('crossed=3/3', 'campusRoute=20/20', 'hillRoute=71/71', 'northRoute=5/5'))
    if part == 'reach':
        assert all(x in joined for x in ('traps=0', 'paths=6094/6094', 'seedTraps=0'))
    if part == 'terrain':
        assert 'treeCrc=0f3b2bb7' in joined and 'samples=20000 holes=0' in joined
    return lines


def original_reviews(tag, output):
    """Keep the original chain exact, but stop our queue on an actual native FAIL.

    fix_checks.sh prints its aggregate verdicts only at the end. Inspecting the
    actual result files while it runs prevents a failed early phase from wasting
    the rest of the queue. A dedicated child process group limits cancellation
    to this invocation and its own game; nothing is killed by application name.
    """
    command = ['bash', 'Saved/CalderWork/tools/fix_checks.sh', tag]
    job = subprocess.Popen(command, cwd=ROOT, stdout=output,
                           stderr=subprocess.STDOUT, start_new_session=True)

    def stop_owned_queue():
        if job.poll() is not None:
            return
        os.killpg(job.pid, signal.SIGTERM)
        try:
            job.wait(timeout=12)
        except subprocess.TimeoutExpired:
            os.killpg(job.pid, signal.SIGKILL)
            job.wait()

    try:
        while job.poll() is None:
            for part in PARTS:
                saved = RUNS / (tag + '_' + part) / 'Saved'
                for review in RESULTS[part]:
                    result = saved / review / 'result.txt'
                    if result.exists():
                        text = result.read_text(errors='replace')
                        if re.search(r'^FAIL\b', text, re.M):
                            raise AssertionError('Actual native ' + part + '/' + review + ' FAIL; original evidence retained, queue stopped.')
                log_path = saved / 'Logs/SeniorSendoff.log'
                if log_path.exists():
                    text = log_path.read_text(errors='replace')
                    if re.search(r'REVIEW FAIL|CAMPUS_SMOKE FAIL|CAMPUS_SMOKE_[A-Z]*_FAILED|Fatal error|Assertion failed', text):
                        raise AssertionError('Actual native ' + part + ' failure; original evidence retained, queue stopped.')
            time.sleep(2)
        assert job.returncode == 0, ('Original review wrapper exited', job.returncode)
    finally:
        stop_owned_queue()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('phase', choices=('capture', 'reviews'))
    parser.add_argument('tag')
    parser.add_argument('--stage', type=int, required=True, choices=range(1, 8))
    args = parser.parse_args()
    assert re.fullmatch(r'[a-z0-9_]+', args.tag)
    grant = json.loads((WORK / 'integration_grant.json').read_text())
    assert grant['granted'] and grant['scope'] == 'full'
    assert grant.get('release_evidence'), 'Record the other owner\'s actual full-release evidence first.'
    idle()
    frozen = manifest()
    receipt = WORK / (args.tag + '_' + args.phase + '.json')
    assert not receipt.exists()
    with (WORK / (args.tag + '_' + args.phase + '.log')).open('x') as output:
        result = {'stage': args.stage, 'tag': args.tag, 'phase': args.phase, 'source_config_map_sha': frozen}
        if args.phase == 'capture':
            result['runs'] = []
            for label, seconds in [('dusk', 0), ('night', 1800)]:
                assert manifest() == frozen, 'Source/Config changed during the frozen queue'
                tag = args.tag + '_' + label
                log = native(tag, ['-HouseExteriorViews', '-CleanViews',
                    '-ExteriorViewFilter=' + ','.join(NAMES), '-ExteriorViewSamples=60',
                    '-ExteriorViewWarmup=4', '-CalderNightfallSeconds=' + str(seconds),
                    '-StadiumDetailReview', '-ExecCmds=t.MaxFPS 0'], output)
                assert 'HOUSE_EXTERIOR_VIEWS done' in log
                assert 'CALDER_STADIUM built step=' + str(args.stage) + ' ' in log
                assert 'CALDER_STADIUM_REVIEW PASS' in log
                if args.stage >= 6:
                    assert 'CALDER_STADIUM_WALL_SEAMS PASS rays=42 leaks=0 actualStreamedRoom=1' in log
                    assert 'CALDER_STADIUM_WALL_SEAMS FAIL' not in log
                folder = RUNS / tag / 'Saved/HouseExteriorViews'
                rows = list(csv.DictReader((folder / 'frametimes.csv').open()))
                assert [x['view'] for x in rows] == NAMES
                assert len(list(folder.glob('*.png'))) == len(NAMES)
                result['runs'].append({'tag': tag, 'phase_seconds': seconds, 'frames': rows,
                    'physical_review': [x for x in log.splitlines() if 'CALDER_STADIUM_REVIEW ' in x],
                    'camera_diagnostics': [x for x in log.splitlines() if 'HOUSE_EXTERIOR_VIEW ' in x]})
            result['visual_inspection_pending'] = True
            result['performance_acceptance'] = False
        else:
            seen = json.loads((WORK / (args.tag + '_visual_inspection.json')).read_text())
            assert seen['stage'] == args.stage and seen['all_images_inspected']
            assert seen['capture_receipt_sha'] == sha(WORK / (args.tag + '_capture.json'))
            assert seen['source_config_map_sha'] == frozen
            for part in PARTS:
                assert not (RUNS / (args.tag + '_' + part)).exists(), part
            original_reviews(args.tag, output)
            output.flush()
            result['native_results'] = {}
            for part in PARTS:
                result['native_results'][part] = review_result(RUNS / (args.tag + '_' + part), part)
        idle()
        assert manifest() == frozen, 'Source/Config changed during evidence collection'
        result['finished_utc'] = datetime.now(timezone.utc).isoformat()
        receipt.write_text(json.dumps(result, indent=2) + '\n')
        print('Recorded', receipt)


if __name__ == '__main__':
    main()
