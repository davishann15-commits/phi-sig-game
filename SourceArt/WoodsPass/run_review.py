#!/usr/bin/env python3
"""Run one isolated native UE review. Never modifies or saves the map."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import time

PROJECT = Path(__file__).resolve().parents[2]
ENGINE = Path('/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor')
QUALITY = ','.join(f'sg.{name}Quality 2' for name in (
    'ViewDistance', 'AntiAliasing', 'Shadow', 'GlobalIllumination', 'Reflection',
    'PostProcess', 'Texture', 'Effects', 'Foliage', 'Shading'))
QUALITY += ',r.ScreenPercentage 100,t.MaxFPS 0,r.VSync 0'
CORE_VIEWS = ','.join('detail_woods_' + name for name in (
    'edge', '20m', 'deep', 'ravine', 'silhouette', 'house_back'))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('name', help='Fresh evidence directory name')
    parser.add_argument('--pass-number', type=int, choices=range(4), default=3)
    parser.add_argument('--kind', choices=('views', 'parking', 'woods', 'leaves'), default='views')
    parser.add_argument('--filter', default=CORE_VIEWS)
    parser.add_argument('--motion-frames', type=int, choices=range(17), default=0)
    parser.add_argument('--timeout', type=int, default=360)
    args = parser.parse_args()
    if Path(args.name).name != args.name:
        parser.error('name must be a single directory name')
    evidence = PROJECT / 'SourceArt/WoodsPass/evidence'
    run = evidence / args.name
    if run.exists():
        parser.error('Use a fresh name; previous evidence is preserved')
    run.mkdir(parents=True)
    command = [str(ENGINE), str(PROJECT / 'SeniorSendoff.uproject'),
               '/Game/Story/Maps/Chapter01_House', '-game', '-windowed',
               '-ResX=1280', '-ResY=720', '-NoSplash', '-NoTraceServer',
               f'-UserDir={run}', f'-AbsLog={run / "runtime.log"}',
               f'-WoodsPass={args.pass_number}', '-WoodsPassReview',
               f'-ExecCmds={QUALITY}']
    if args.kind == 'views':
        command += ['-HouseExteriorViews', '-CleanViews',
                    '-ExteriorViewWarmup=5', '-ExteriorViewSamples=120',
                    f'-WoodsMotionFrames={args.motion_frames}',
                    f'-ExteriorViewFilter={args.filter}', '-CampusWoodsReview',
                    '-FraternityBoundaryReview', '-WoodsSceneReview',
                    '-WoodsUndergrowthReview']
    elif args.kind == 'parking':
        command += ['-ParkingLotWalkReview', '-CampusWoodsReview',
                    '-FraternityBoundaryReview', '-WoodsSceneReview',
                    '-WoodsUndergrowthReview']
    elif args.kind == 'woods':
        command += ['-WoodsWalkReview', '-WoodsUndergrowthReview']
    else:
        command += ['-HouseLeafWindReview', '-CampusWoodsReview',
                    '-FraternityBoundaryReview', '-WoodsSceneReview',
                    '-WoodsUndergrowthReview']
    started = time.monotonic()
    with (run / 'launcher.log').open('w') as output:
        process = subprocess.Popen(command, cwd=PROJECT, stdout=output,
                                   stderr=subprocess.STDOUT)
        print(f'Native {args.kind} review started, pid={process.pid}, evidence={run}', flush=True)
        timed_out = False
        try:
            code = process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.terminate()  # Only this script's own game process.
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            code = 124
    result = dict(kind=args.kind, stage=args.pass_number, exit_code=code,
                  timed_out=timed_out, elapsed_seconds=round(time.monotonic()-started, 2),
                  resolution=[1280,720], quality=QUALITY, command=command)
    expected = ['WoodsPassReview', 'WoodsDetailReview']
    if args.kind != 'woods':
        expected += ['CampusWoodsReview', 'FraternityBoundaryReview']
    if args.kind == 'parking':
        expected += ['ParkingLotReview']
    if args.kind == 'leaves':
        expected += ['LeafWindReview']
    reports = {}
    for review in expected:
        report = run / 'Saved' / review / 'result.txt'
        contents = report.read_text(encoding='utf-8-sig') if report.exists() else ''
        reports[review] = bool(re.search(r'^PASS\b', contents, re.M)) and not bool(
            re.search(r'^FAIL\b', contents, re.M))
    result['reports_passed'] = reports
    result['verified'] = code == 0 and all(reports.values())
    (run / 'invocation.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('command','quality')}), flush=True)
    raise SystemExit(code if code else (0 if result['verified'] else 1))

if __name__ == '__main__':
    main()
