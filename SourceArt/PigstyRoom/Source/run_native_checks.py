from pathlib import Path
import subprocess,re,time
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-room-v1');P='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SeniorSendoff.uproject'
CMD='/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd';GAME='/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
def run(args,log,marker):
 start=time.monotonic()
 with open(R/(log+'-console.log'),'w') as out:code=subprocess.call(args,stdout=out,stderr=subprocess.STDOUT)
 path=R/(log+'.log');txt=path.read_text(errors='replace')
 lines=[l for l in txt.splitlines() if any(s in l for s in [marker,'RESULT','PASSED','FEEDING','TRAVERSED'])]
 print('\n'.join(lines[-15:]),flush=True)
 if marker not in txt:raise RuntimeError('Native check failed '+log+' exit='+str(code))
 print('CHECK_COMPLETE',log,'seconds',round(time.monotonic()-start,1),flush=True)
run([CMD,P,'-run=pythonscript','-script='+str(R/'source/apply_room.py'),'-unattended','-nosplash','-nosound','-abslog='+str(R/'import/final-apply.log')],'import/final-apply','PIGSTY_ROOM_APPLY_PASSED')
common=[GAME,P,'/Game/Story/Maps/Chapter01_House','-game','-ResX=1280','-ResY=720','-windowed','-unattended','-nosplash','-nosound']
run(common+['-HousePigStairSmoke','-UserDir='+str(R/'native-pigs-final'),'-abslog='+str(R/'native-pigs-final.log')],'native-pigs-final','HOUSE_PIG_STAIR_SMOKE_RESULT passed=1')
run(common+['-HouseVisualReview','-HouseVisualLayout='+str(R/'source/views.json'),'-HouseVisualQuality=1','-UserDir='+str(R/'native-visual'),'-abslog='+str(R/'native-visual.log')],'native-visual','HOUSE_VISUAL_REVIEW_PASSED')
