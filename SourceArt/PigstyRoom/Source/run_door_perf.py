from pathlib import Path
import subprocess,time
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-room-v1');P='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SeniorSendoff.uproject';E='/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
common=[E,P,'/Game/Story/Maps/Chapter01_House','-game','-ResX=1280','-ResY=720','-windowed','-unattended','-nosplash','-nosound']
for name,args,marker in [('native-door',['-ExistingHouseDoorSmoke','-ExistingHouseDoorTargets=HallEnd_West'],'EXISTING_HOUSE_DOOR_PASSED'),('native-perf',['-HouseWalkPerf','-HouseWalkRoute='+str(R/'source/walk-route.json')],'HOUSE_WALK_PERF_RESULT')]:
 with open(R/(name+'-console.log'),'w') as out:subprocess.call(common+args+['-UserDir='+str(R/name),'-abslog='+str(R/(name+'.log'))],stdout=out,stderr=subprocess.STDOUT)
 txt=(R/(name+'.log')).read_text(errors='replace');lines=[l for l in txt.splitlines() if marker in l];print('\n'.join(lines),flush=True)
 if marker not in txt:raise RuntimeError('Native check failed '+name)
