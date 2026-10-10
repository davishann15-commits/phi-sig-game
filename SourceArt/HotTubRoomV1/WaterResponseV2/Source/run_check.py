import sys, json, subprocess, time
from pathlib import Path

base = Path('/Users/Stewart/Projects/Senior sendoff/outputs/hot-tub-water')
evidence = base / (sys.argv[1] if len(sys.argv) > 1 else 'playtest-final')
evidence.mkdir(parents=True, exist_ok=True)
args = [
    '/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',
    '/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SeniorSendoff.uproject',
    '/Game/Story/Maps/Chapter01_House', '-game', '-ResX=1280', '-ResY=720',
    '-windowed', '-unattended', '-nosplash', '-HouseHotTubWaterSmoke',
    f'-HouseHotTubWaterEvidence={evidence}', f'-UserDir={evidence}/user',
    f'-abslog={evidence}/runtime.log',
]
with (evidence / 'console.log').open('w') as out:
    child = subprocess.Popen(args, stdout=out, stderr=subprocess.STDOUT)
    began = time.monotonic()
    while child.poll() is None:
        if time.monotonic() - began > 160:
            child.terminate()
            try:
                child.wait(timeout=15)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
            print('Native hot tub interaction timed out', flush=True)
            sys.exit(1)
        time.sleep(.5)
result_file = evidence / 'result.json'
if not result_file.exists():
    print('Native process exited without a water interaction result', child.returncode, flush=True)
    sys.exit(1)
result = json.loads(result_file.read_text())
print(json.dumps({'nativeProcessExit': child.returncode, **result}, indent=2), flush=True)
sys.exit(0 if result.get('passed') else 1)
