import pathlib, subprocess, sys, uuid, shutil, json
B=pathlib.Path(__file__).resolve().parent
P=B.parents[1]
E=B/'evidence'
stage=sys.argv[1]
assert subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode != 0, 'Close the saved editor first'
user=pathlib.Path('/tmp/court-'+stage+'-'+uuid.uuid4().hex[:8])
log=E/(stage+'.log')
cmd=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(P/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-game','-windowed','-ResX=1280','-ResY=720','-NoSplash','-UserDir='+str(user),'-AbsLog='+str(log)]
if stage=='physics':
    cmd+=['-HouseBasketballReview']
    folder='BasketballReview'
    marker='COURT_REVIEW_PASS'
else:
    cmd+=['-HouseVisualReview','-HouseVisualLayout='+str(B/'views.json'),'-HouseVisualQuality=2']
    folder='HouseVisualReview'
    marker='HOUSE_VISUAL_REVIEW_PASSED'
with (E/(stage+'-stdout.log')).open('w') as output:
    proc=subprocess.Popen(cmd,stdout=output,stderr=subprocess.STDOUT)
    try:code=proc.wait(timeout=550)
    except subprocess.TimeoutExpired:proc.terminate();code=proc.wait(timeout=25)
source=user/'Saved'/folder
if source.exists():shutil.copytree(source,E/stage,dirs_exist_ok=True)
raw=log.read_text(errors='replace') if log.exists() else ''
report=dict(exit_code=code,status='PASS' if code==0 and marker in raw else 'FAIL',user_dir=str(user),marker=marker)
(E/(stage+'-run.json')).write_text(json.dumps(report,indent=2))
print(json.dumps(report))
