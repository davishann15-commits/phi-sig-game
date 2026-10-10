from pathlib import Path
import subprocess, argparse
root=Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-motion-v3')
project='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SeniorSendoff.uproject'
editor='/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
p=argparse.ArgumentParser(); p.add_argument('check', choices=['motion','grip','stairs','performance']); a=p.parse_args()
flags={'grip':['-HousePigMotionSmoke','-HousePigGripOnly'],'motion':['-HousePigMotionSmoke'],'stairs':['-HousePigStairSmoke'],'performance':['-HouseWalkPerf','-HouseWalkRoute=/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-refine-v2/source/walk-route.json']}
marker={'grip':'HOUSE_PIG_MOTION_SMOKE_RESULT passed=1','motion':'HOUSE_PIG_MOTION_SMOKE_RESULT passed=1','stairs':'HOUSE_PIG_STAIR_SMOKE_RESULT passed=1','performance':'HOUSE_WALK_PERF_RESULT: passed=1'}
name='native-'+a.check
command=[editor,project,'/Game/Story/Maps/Chapter01_House','-game','-ResX=1280','-ResY=720','-windowed','-unattended','-nosplash','-nosound','-UserDir='+str(root/name),'-abslog='+str(root/(name+'.log'))]+flags[a.check]
with open(root/(name+'-console.log'),'w') as out: run=subprocess.run(command,stdout=out,stderr=subprocess.STDOUT,timeout=300)
log=(root/(name+'.log')).read_text(errors='replace')
print('\n'.join([l for l in log.splitlines() if any(w in l for w in ['RESULT','PASSED','MOTION_','FAILED'])][-32:]),flush=True)
if run.returncode!=0 or marker[a.check] not in log: raise RuntimeError('Check failed: '+a.check)
