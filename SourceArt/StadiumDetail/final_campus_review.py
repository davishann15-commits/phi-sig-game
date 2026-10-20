"""Fresh normal-lobby multiplayer after the accepted final stadium revision.

The native result is separate from actual image inspection and final acceptance.
No forced scene clock, review route edits or runtime changes are made here.
"""
from datetime import datetime, timezone
import argparse, json, re, subprocess
from stage_runner import ROOT, WORK, RUNS, idle, manifest, sha


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('tag')
    parser.add_argument('--capture-tag', required=True)
    parser.add_argument('--performance-tag', required=True)
    args = parser.parse_args()
    assert re.fullmatch(r'[a-z0-9_]+', args.tag)
    idle()
    grant = json.loads((WORK / 'integration_grant.json').read_text())
    assert grant['granted'] and grant['scope'] == 'full' and grant['release_evidence']
    frozen = manifest()
    for phase in ('capture', 'visual_inspection', 'reviews'):
        path = WORK / (args.capture_tag + '_' + phase + '.json')
        data = json.loads(path.read_text())
        assert data['stage'] == 7 and data['source_config_map_sha'] == frozen
        if phase == 'visual_inspection':
            assert data['all_images_inspected'] and data['accepted_for_stage']
            assert data['capture_receipt_sha'] == sha(WORK / (args.capture_tag + '_capture.json'))
    performance = json.loads((WORK / (args.performance_tag + '_performance.json')).read_text())
    assert performance['stage'] == 7 and performance['passed']
    assert performance['source_config_map_sha'] == frozen
    folder = RUNS / args.tag
    destination = WORK / (args.tag + '_multiplayer_native.json')
    assert not folder.exists() and not destination.exists()
    wrapper = WORK / (args.tag + '_multiplayer_wrapper.log')
    with wrapper.open('x') as output:
        command = ['python3', 'Saved/CalderWork/tools/campus_mp.py', args.tag, '--walk']
        output.write(repr(command) + '\n')
        output.flush()
        subprocess.run(command, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT, check=True)
    idle()
    assert manifest() == frozen
    result = json.loads((folder / 'result.json').read_text())
    assert result['status'] == 'PASS' and result['terrain_hashes_match']
    assert result['terrain_hashes'] == {'host': '72564759', 'client': '72564759'}
    assert result['walk']['walked'] and result['walk']['corrections_during_walk'] == 0
    for role in ('host', 'client'):
        text = (folder / (role + '.log')).read_text(errors='replace')
        assert 'CAMPUS_MP_PASS' in text and 'CAMPUS_MP_FAIL' not in text
        assert not re.search(r'Fatal error|Assertion failed', text)
        assert 'CALDER_STADIUM built step=7 ' in text
        assert 'crc=0f3b2bb7' in text
    files = [p for p in folder.rglob('*') if p.is_file()]
    images = [p for p in files if p.suffix.lower() == '.png']
    assert images, 'Actual peer captures must exist for subsequent inspection.'
    destination.write_text(json.dumps({
        'stage': 7, 'tag': args.tag, 'native_passed': True, 'final_acceptance': False,
        'finished_utc': datetime.now(timezone.utc).isoformat(),
        'source_config_map_sha': frozen, 'normal_lobby': True,
        'forced_scene_clock': False, 'multiplayer': result,
        'images': [{'file': str(p.relative_to(ROOT)), 'sha256': sha(p)} for p in sorted(images)],
        'run_tags': [args.tag],
        'evidence_files': [str(wrapper.relative_to(ROOT))] + [str(p.relative_to(ROOT)) for p in files],
        'pending': 'Actual peer image inspection, final combined acceptance and recovery export.'
    }, indent=2) + '\n')
    print('PASS native normal-lobby multiplayer; peer image inspection remains:', destination)


if __name__ == '__main__':
    main()
