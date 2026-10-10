import pathlib,subprocess,json,time,sys,shutil,uuid
B=pathlib.Path(__file__).resolve().parent;R=B.parents[1];stage=sys.argv[1];E=B/'evidence';u=pathlib.Path('/tmp/noodle-'+stage+'-'+uuid.uuid4().hex[:8]);log=E/(stage+'.log')
assert subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode!=0
cmd=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(R/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-game','-windowed','-ResX=1280','-ResY=720','-NoSplash','-UserDir='+str(u),'-AbsLog='+str(log),'-HouseVisualReview','-HouseVisualLayout='+str(B/(stage+'.json')),'-HouseVisualQuality=2']
with (E/(stage+'-stdout.log')).open('w')as out:
 p=subprocess.Popen(cmd,stdout=out,stderr=subprocess.STDOUT)
 try:code=p.wait(timeout=600)
 except subprocess.TimeoutExpired:p.terminate();code=p.wait(timeout=20)
folder=u/'Saved/HouseVisualReview'
if folder.exists():shutil.copytree(folder,E/stage,dirs_exist_ok=True)
txt=log.read_text(errors='replace');report=dict(code=code,status='PASS'if code==0 and 'HOUSE_VISUAL_REVIEW_PASSED'in txt else'FAIL',user_dir=str(u));(E/(stage+'-run.json')).write_text(json.dumps(report,indent=2));print(report)
