import sys, json, subprocess, time
from pathlib import Path
base=Path('/Users/Stewart/Projects/Senior sendoff/outputs/impossible-hallway')
evidence=base/(sys.argv[1] if len(sys.argv)>1 else 'playtest-final')
evidence.mkdir(parents=True,exist_ok=True)
args=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor','/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SeniorSendoff.uproject','/Game/Story/Maps/Chapter01_House','-game','-ResX=1280','-ResY=720','-windowed','-unattended','-nosplash','-HouseImpossibleHallwaySmoke',f'-HouseImpossibleHallwayEvidence={evidence}',f'-UserDir={evidence}/user',f'-abslog={evidence}/runtime.log']
with (evidence/'console.log').open('w') as out:
    child=subprocess.Popen(args,stdout=out,stderr=subprocess.STDOUT)
    began=time.monotonic()
    while child.poll() is None:
        if time.monotonic()-began>270:
            child.terminate()
            try: child.wait(timeout=15)
            except subprocess.TimeoutExpired: child.kill(); child.wait()
            print('Hallway runtime timed out',flush=True)
            sys.exit(1)
        time.sleep(.5)
resultfile=evidence/'result.json'
if not resultfile.exists():
    print('Native process exited without traversal result',child.returncode,flush=True)
    sys.exit(1)
result=json.loads(resultfile.read_text())
print(json.dumps({'nativeProcessExit':child.returncode,**result},indent=2),flush=True)
sys.exit(0 if result.get('passed') else 1)
