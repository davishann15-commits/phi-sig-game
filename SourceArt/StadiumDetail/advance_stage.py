"""Advance only after the preceding stage's actual native and inspected gates pass."""
from datetime import datetime, timezone
import argparse, json
from stage_runner import ROOT, WORK, idle, manifest, sha, NAMES


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('accepted_tag')
    parser.add_argument('performance_tag')
    parser.add_argument('--to', type=int, choices=range(2, 8), required=True)
    args = parser.parse_args()
    idle()
    grant = json.loads((WORK / 'integration_grant.json').read_text())
    assert grant['granted'] and grant['scope'] == 'full' and grant['release_evidence']
    frozen = manifest()
    previous = args.to - 1
    evidence = {}
    for phase in ('capture', 'visual_inspection', 'reviews'):
        path = WORK / (args.accepted_tag + '_' + phase + '.json')
        data = json.loads(path.read_text())
        assert data['stage'] == previous and data['source_config_map_sha'] == frozen
        evidence[phase] = str(path.relative_to(ROOT))
        if phase == 'capture':
            assert len(data['runs']) == 2 and all([row['view'] for row in run['frames']] == NAMES for run in data['runs'])
        elif phase == 'visual_inspection':
            assert data['all_images_inspected'] and data['accepted_for_stage']
            assert data['capture_receipt_sha'] == sha(WORK / (args.accepted_tag + '_capture.json'))
        else:
            assert set(data['native_results']) == set('layout terrain roam reviews clock smoke intA intBC noterrain reach_off reach'.split())
    performance = WORK / (args.performance_tag + '_performance.json')
    data = json.loads(performance.read_text())
    assert data['passed'] and data['stage'] == previous and data['source_config_map_sha'] == frozen
    evidence['performance'] = str(performance.relative_to(ROOT))
    draft = ROOT / 'SourceArt/StadiumDetail/Draft'
    sources = ROOT / 'Source/SeniorSendoff'
    for path in draft.glob('HouseStadiumDetail*'):
        if path.suffix in ('.cpp', '.h'):
            assert path.read_bytes() == (sources / path.name).read_bytes(), 'Unapplied draft differs: ' + path.name
    source = sources / 'HouseStadiumDetail.cpp'
    old = source.read_text()
    marker = f'int32 Step(){{int32 Value={previous};'
    assert old.count(marker) == 1
    new = old.replace(marker, f'int32 Step(){{int32 Value={args.to};', 1)
    destination = WORK / f'stage_{previous}_accepted_advance_{args.to}.json'
    assert not destination.exists()
    source.unlink()
    source.write_text(new)
    draft_source = draft / source.name
    draft_source.unlink()
    draft_source.write_text(new)
    receipt = {'accepted_stage': previous, 'new_default_stage': args.to, 'evidence': evidence,
               'previous_source_config_map_sha': frozen, 'new_source_config_map_sha': manifest(),
               'updated_utc': datetime.now(timezone.utc).isoformat(), 'build_pending': True}
    destination.write_text(json.dumps(receipt, indent=2) + '\n')
    print('Advanced tested stadium default', previous, 'to', args.to, '; new native build required')


if __name__ == '__main__':
    main()
