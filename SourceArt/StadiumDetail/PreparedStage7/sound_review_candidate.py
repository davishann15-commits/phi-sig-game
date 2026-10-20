"""Prepared driver for the opt-in real-audio/temporal review; stage7 only.

The normal audio mixer must run: muteaudio silences the output device without
disabling source consumption or registered-submix recordings. No forced drum
trigger, shortened timer, fixed timestep or accelerated clock is used.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse, json, re, subprocess, sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'SourceArt/StadiumDetail'))
from stage_runner import idle, manifest, WORK, RUNS, sha


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('tag')
    args = parser.parse_args()
    assert re.fullmatch(r'[a-z0-9_]+', args.tag)
    idle()
    grant = json.loads((WORK / 'integration_grant.json').read_text())
    assert grant['granted'] and grant['scope'] == 'full' and grant['release_evidence']
    frozen = manifest()
    assert 'int32 Step(){int32 Value=7;' in (ROOT / 'Source/SeniorSendoff/HouseStadiumDetail.cpp').read_text()
    assert not (RUNS / args.tag).exists()
    receipt = WORK / (args.tag + '_sound_native.json')
    assert not receipt.exists()
    out = WORK / (args.tag + '_sound_native.log')
    with out.open('x') as output:
        command = ['bash', 'Saved/CalderWork/tools/run_game.sh', args.tag, '900',
                   '-RenderOffScreen', '-MetalOffscreenOnly', '-muteaudio',
                   '-StadiumSoundReview', '-CampusDrumsDump', '-CalderNightfallReview', '-CalderNightfallSeconds=0']
        output.write(repr(command) + '\n')
        output.flush()
        subprocess.run(command, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT, check=True)
    idle()
    assert manifest() == frozen
    saved = RUNS / args.tag / 'Saved'
    log = (saved / 'Logs/SeniorSendoff.log').read_text(errors='replace')
    summary = (saved / 'StadiumSoundReview/result.txt').read_text()
    assert not re.search(r'REVIEW FAIL|Fatal error|Assertion failed', log)
    assert 'CALDER_STADIUM built step=7 ' in log
    sky_rows = re.findall(r'CALDER_NIGHTFALL seconds=([0-9.]+) alpha=([0-9.]+) duration=1800 pinned=(\d)', log)
    assert len(sky_rows) >= 2 and all(float(t) == 0 and float(a) == 0 and pin == '1' for t, a, pin in sky_rows)
    assert 'CALDER_STADIUM_SOUND_REVIEW PASS ' in log and summary.startswith('PASS ')
    assert 'actualRegisteredSubmixes=4 defaultTimerUnchanged=1' in log
    assert len(re.findall('CALDER_STADIUM_SOUND_RECORD_START phase=', log)) == 8
    assert len(re.findall('CALDER_STADIUM_SOUND_RECORD_STOP phase=', log)) == 8
    dry = saved / 'CampusFall/CampusDrumsDry.wav'
    assert dry.exists() and dry.stat().st_size > 1000
    files = [p for p in (saved / 'StadiumSoundReview').iterdir() if p.is_file()] + [dry]
    receipt.write_text(json.dumps({
        'stage': 7, 'tag': args.tag, 'native_passed': True, 'final_acceptance': False,
        'source_config_map_sha': frozen, 'finished_utc': datetime.now(timezone.utc).isoformat(),
        'summary': summary, 'normal_audio_clock': True, 'forced_timer_triggers': False,
        'sky_only_held_at_initial_dusk': True, 'world_audio_timers_unmodified': True,
        'files': [{'file': str(p.relative_to(ROOT)), 'sha256': sha(p)} for p in sorted(files)],
        'evidence_files': [str(out.relative_to(ROOT)), str((saved / 'Logs/SeniorSendoff.log').relative_to(ROOT))],
        'pending': 'Offline actual-PCM analysis, all25 actual images and spatial listening.'
    }, indent=2) + '\n')
    print('PASS native real-audio/temporal review; PCM and visual/listening acceptance remain:', receipt)


if __name__ == '__main__':
    main()
