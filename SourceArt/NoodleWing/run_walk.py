import pathlib,subprocess,json,time,sys,shutil,uuid
B=pathlib.Path(__file__).resolve().parent;R=B.parents[1];E=B/'evidence';u=pathlib.Path('/tmp/noodle-walk-'+uuid.uuid4().hex[:8]);stage=sys.argv[1]if len(sys.argv)>1 else'walk';log=E/(stage+'.log')
assert subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode!=0
cmd=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(R/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-game','-windowed','-ResX=1280','-ResY=720','-NoSplash','-UserDir='+str(u),'-AbsLog='+str(log),'-NoodleWingSmoke','-NoodleRoute='+str(B/'walk.json')]
with(E/(stage+'-stdout.log')).open('w')as out:
 p=subprocess.Popen(cmd,stdout=out,stderr=subprocess.STDOUT)
 try:code=p.wait(timeout=480)
 except subprocess.TimeoutExpired:p.terminate();code=p.wait(timeout=20)
folder=u/'Saved/NoodleWingReview'
if folder.exists():shutil.copytree(folder,E/stage,dirs_exist_ok=True)
txt=log.read_text(errors='replace');report=dict(code=code,status='PASS'if code==0 and 'NOODLE_WALK_PASS'in txt else'FAIL',user_dir=str(u),markers=[l for l in txt.splitlines()if'NOODLE_'in l]);(E/(stage+'-run.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
