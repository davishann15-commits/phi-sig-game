"""Final standalone/signature and actual non-Unity editor compilation.

Requires the accepted stage7 capture, full original reviews and performance at
the current frozen source. Builds stay serial and do not save the map.
"""
from datetime import datetime, timezone
import argparse, json, re, subprocess
from stage_runner import ROOT, WORK, idle, manifest, sha


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('tag')
    parser.add_argument('--capture-tag', required=True)
    parser.add_argument('--performance-tag', required=True)
    args = parser.parse_args()
    assert re.fullmatch(r'[a-z0-9_]+', args.tag)
    idle()
    frozen = manifest()
    grant = json.loads((WORK / 'integration_grant.json').read_text())
    assert grant['granted'] and grant['scope'] == 'full' and grant['release_evidence']
    for phase in ('capture', 'visual_inspection', 'reviews'):
        receipt = json.loads((WORK / (args.capture_tag + '_' + phase + '.json')).read_text())
        assert receipt['stage'] == 7 and receipt['source_config_map_sha'] == frozen
        if phase == 'visual_inspection':
            assert receipt['all_images_inspected'] and receipt['accepted_for_stage']
            assert receipt['capture_receipt_sha'] == sha(WORK / (args.capture_tag + '_capture.json'))
    perf = json.loads((WORK / (args.performance_tag + '_performance.json')).read_text())
    assert perf['stage'] == 7 and perf['passed'] and perf['source_config_map_sha'] == frozen
    output = WORK / (args.tag + '_disable_unity.json')
    assert not output.exists()

    standalone_name = args.tag + '_game'
    game_wrapper = WORK / (args.tag + '_game_wrapper.log')
    with game_wrapper.open('x') as stream:
        subprocess.run(['python3', 'Tools/calder/build_wildlife_game.py', standalone_name],
                       cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, check=True)
    game_receipt = ROOT / 'Saved/CalderWork' / (standalone_name + '_build.json')
    game = json.loads(game_receipt.read_text())
    assert game['passed'] and game['strict_signature_passed'] and game['source_config_frozen']
    idle()
    assert manifest() == frozen

    # Fresh mtimes ensure that all ten owned cpp files really compile separately.
    sources = sorted((ROOT / 'Source/SeniorSendoff').glob('HouseStadiumDetail*.cpp'))
    assert len(sources) == 10
    for source in sources:
        source.touch()
    command = ['/Users/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh',
               'SeniorSendoffEditor', 'Mac', 'Development',
               '-Project=' + str(ROOT / 'SeniorSendoff.uproject'),
               '-WaitMutex', '-NoHotReloadFromIDE', '-DisableUnity']
    editor_log = WORK / (args.tag + '_editor_disable_unity.log')
    with editor_log.open('x') as stream:
        stream.write(repr(command) + '\n')
        stream.flush()
        subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, check=True)
    text = editor_log.read_text(errors='replace')
    assert 'Result: Succeeded' in text
    for source in sources:
        assert any('Compile' in line and source.name in line for line in text.splitlines()), source.name
    idle()
    assert manifest() == frozen
    evidence = [game_wrapper, game_receipt, editor_log,
                ROOT / 'Saved/CalderWork' / (standalone_name + '.log'),
                ROOT / 'Saved/CalderWork' / (standalone_name + '_signature.log')]
    assert all(p.exists() for p in evidence)
    output.write_text(json.dumps({
        'passed': True, 'stage': 7, 'source_config_map_sha': frozen,
        'finished_utc': datetime.now(timezone.utc).isoformat(), 'run_tags': [],
        'standalone_compile_and_strict_signature': True, 'standalone_gameplay_tested': False,
        'editor_disable_unity': True, 'actual_owned_cpp_compiled': [p.name for p in sources],
        'command': command, 'evidence_files': [str(p.relative_to(ROOT)) for p in evidence],
        'scope': 'Final compilation/signature compatibility; rendered native gameplay evidence is separate.'
    }, indent=2) + '\n')
    print('PASS final game/signature and actual ten-file DisableUnity editor compilation:', output)


if __name__ == '__main__':
    main()
