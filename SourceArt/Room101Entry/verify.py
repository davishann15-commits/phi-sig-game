import pathlib,subprocess,time,json
B=pathlib.Path(__file__).resolve().parent;R=B.parent.parent
UE='/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
results=[]
cases=[('walking',['-Room101EntrySmoke'],'ROOM101_ENTRY_PASSED',220),('views',['-HouseVisualReview','-HouseVisualLayout='+str(B/'views.json'),'-HouseVisualQuality=1'],'HOUSE_VISUAL_REVIEW_PASSED',180)]
for name,flags,marker,limit in cases:
 user='/tmp/room101-'+name;log=B/'evidence'/(name+'.log')
 command=[UE,str(R/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-game','-windowed','-ResX=1280','-ResY=720','-nosound','-UserDir='+user,'-AbsLog='+str(log)]+flags
 print('START '+name,flush=True);start=time.time()
 with open(B/'evidence'/(name+'-stdout.log'),'w') as stream:
  p=subprocess.Popen(command,stdout=stream,stderr=subprocess.STDOUT)
  try:rc=p.wait(timeout=limit)
  except subprocess.TimeoutExpired:
   p.terminate()
   try:p.wait(timeout=12)
   except subprocess.TimeoutExpired:p.kill();p.wait()
   rc='TIMEOUT'
 text=log.read_text(errors='replace') if log.exists() else ''
 result=dict(case=name,status='PASS' if marker in text else 'FAIL',log=str(log),exit_code=rc,seconds=round(time.time()-start,2),user_dir=user)
 results.append(result);json.dump(results,open(B/'evidence/verification.json','w'),indent=2);print(result['status']+' '+name,flush=True)
