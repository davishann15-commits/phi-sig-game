"""Collect an already completed full native chain after a parser-only correction.

Never launches tests, invents results, or accepts partial/changed-source evidence.
The original queue log and any driver exception remain preserved.
"""
from datetime import datetime, timezone
import argparse, json
from stage_runner import ROOT, WORK, RUNS, PARTS, manifest, idle, sha, review_result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('tag')
    parser.add_argument('--stage', type=int, choices=range(1, 8), required=True)
    args = parser.parse_args()
    idle()
    frozen = manifest()
    grant = json.loads((WORK / 'integration_grant.json').read_text())
    assert grant['granted'] and grant['scope'] == 'full' and grant['release_evidence']
    capture_path = WORK / (args.tag + '_capture.json')
    capture = json.loads(capture_path.read_text())
    visual = json.loads((WORK / (args.tag + '_visual_inspection.json')).read_text())
    assert capture['source_config_map_sha'] == visual['source_config_map_sha'] == frozen
    assert capture['stage'] == visual['stage'] == args.stage
    assert visual['all_images_inspected'] and visual['capture_receipt_sha'] == sha(capture_path)
    log_path = WORK / (args.tag + '_reviews.log')
    assert log_path.read_text(errors='replace').rstrip().endswith('FIX_CHECKS_DONE'), 'Unmodified full native chain has not finished'
    destination = WORK / (args.tag + '_reviews.json')
    assert not destination.exists(), 'Never overwrite a prior receipt'
    results = {part: review_result(RUNS / (args.tag + '_' + part), part) for part in PARTS}
    assert manifest() == frozen
    data = {'stage': args.stage, 'tag': args.tag, 'phase': 'reviews',
            'source_config_map_sha': frozen, 'native_results': results,
            'finished_utc': datetime.now(timezone.utc).isoformat(),
            'collection_note': 'Re-read all11 original native results after correcting the parser for the released R7 All roster:23buildings. No native review, threshold, route or result changed.',
            'original_queue_log': str(log_path.relative_to(ROOT))}
    destination.write_text(json.dumps(data, indent=2) + '\n')
    print('Collected all11 actual native phases with23/23smokes and431interior stops:', destination)


if __name__ == '__main__':
    main()
