from pathlib import Path
import subprocess
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-room-v1');P='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SeniorSendoff.uproject';E='/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
common=[E,P,'/Game/Story/Maps/Chapter01_House','-game','-ResX=1280','-ResY=720','-windowed','-unattended','-nosplash','-nosound']
for name,args,marker in [('native-food-final',['-HousePigSmoke'],'HOUSE_PIG_SMOKE_RESULT passed=1'),('native-visual-final',['-HouseVisualReview','-HouseVisualLayout='+str(R/'source/views.json'),'-HouseVisualQuality=1'],'HOUSE_VISUAL_REVIEW_PASSED')]:
 with open(R/(name+'-console.log'),'w') as out:subprocess.call(common+args+['-UserDir='+str(R/name),'-abslog='+str(R/(name+'.log'))],stdout=out,stderr=subprocess.STDOUT)
 txt=(R/(name+'.log')).read_text(errors='replace');lines=[l for l in txt.splitlines() if any(s in l for s in ['RESULT','PASSED','PROJECTED','FEEDING'])];print('\n'.join(lines[-10:]),flush=True)
 if marker not in txt:raise RuntimeError('Native check failed '+name)
 if 'missing usage flag InstancedStaticMeshes' in txt:raise RuntimeError('Runtime food material usage failed')
