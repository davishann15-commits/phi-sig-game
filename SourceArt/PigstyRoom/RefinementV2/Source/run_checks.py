from pathlib import Path
import subprocess, argparse

root = Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-refine-v2')
project = '/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SeniorSendoff.uproject'
editor = '/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
options = argparse.ArgumentParser()
options.add_argument('check', choices=['upgrade', 'stairs', 'visual', 'performance'])
choice = options.parse_args().check
checks = {
    'upgrade': (['-HousePigUpgradeSmoke'], 'HOUSE_PIG_UPGRADE_RESULT passed=1'),
    'stairs': (['-HousePigStairSmoke'], 'HOUSE_PIG_SMOKE_RESULT passed=1'),
    'visual': (['-HouseVisualReview', '-HouseVisualLayout=' + str(root / 'source/views.json'), '-HouseVisualQuality=1'], 'HOUSE_VISUAL_REVIEW_PASSED'),
    'performance': (['-HouseWalkPerf', '-HouseWalkRoute=' + str(root / 'source/walk-route.json')], 'HOUSE_WALK_PERF_RESULT: passed=1'),
}
args, marker = checks[choice]
name = 'native-' + choice
command = [editor, project, '/Game/Story/Maps/Chapter01_House', '-game', '-ResX=1280', '-ResY=720',
           '-windowed', '-unattended', '-nosplash', '-nosound', '-UserDir=' + str(root / name),
           '-abslog=' + str(root / (name + '.log'))] + args
with open(root / (name + '-console.log'), 'w') as stream:
    run = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, timeout=240)
log = (root / (name + '.log')).read_text(errors='replace')
lines = [line for line in log.splitlines() if any(word in line for word in ['RESULT', 'PASSED', 'UPGRADE_', 'FAILED'])]
print('\n'.join(lines[-24:]), flush=True)
if run.returncode != 0 or marker not in log:
    raise RuntimeError('Check failed: ' + choice)
