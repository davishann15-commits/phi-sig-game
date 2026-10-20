"""Run one rendered actual-map acceptance stage with an isolated player save."""
import argparse,json,pathlib,shutil,subprocess,time,uuid
p=argparse.ArgumentParser();p.add_argument('stage',choices=['runtime','doors','pole','views']);a=p.parse_args()
B=pathlib.Path(__file__).resolve().parent.parent;R=B.parent.parent;E=B/'evidence/expansion2';E.mkdir(parents=True,exist_ok=True)
if subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode==0:raise SystemExit('Another Unreal process is open; sequence this stage after it exits.')
if subprocess.run(['pgrep','-x','Blender'],capture_output=True).returncode==0:raise SystemExit('Blender is running; finish geometry work before measuring runtime.')
u=pathlib.Path('/tmp/club-expansion2-'+a.stage+'-'+uuid.uuid4().hex[:8]);log=E/(a.stage+'.log')
flags={
 'runtime':['-NightClubSmoke'],
 'doors':['-NightClubEntranceSmoke'],
 'pole':['-HouseClubPoleSmoke'],
 'views':['-HouseVisualReview','-HouseVisualLayout='+str(B/'expansion2_layout/views.json'),'-HouseVisualQuality=2']}
cmd=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(R/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-game','-windowed','-ResX=1280','-ResY=720','-NoSplash','-UserDir='+str(u),'-AbsLog='+str(log)]+flags[a.stage]
start=time.monotonic()
with (E/(a.stage+'-stdout.log')).open('w') as out:
 job=subprocess.Popen(cmd,stdout=out,stderr=subprocess.STDOUT)
 try:code=job.wait(timeout=550)
 except subprocess.TimeoutExpired:
  job.terminate()
  try:job.wait(timeout=12)
  except subprocess.TimeoutExpired:job.kill();job.wait()
  code='TIMEOUT'
text=log.read_text(errors='replace') if log.exists() else ''
markers={'runtime':'NIGHT_CLUB_SMOKE_PASSED','doors':'NIGHT_CLUB_ENTRANCE_PASSED','pole':'CLUB_POLE_SMOKE_PASS','views':'HOUSE_VISUAL_REVIEW_PASSED'}
passed=code==0 and markers[a.stage] in text
folder=u/'Saved'/('HouseVisualReview' if a.stage=='views' else 'NightClub')
if folder.exists():shutil.copytree(folder,E/(a.stage+'-results'),dirs_exist_ok=True)
report=dict(status='PASS' if passed else 'FAIL',stage=a.stage,exit_code=code,user_dir=str(u),seconds=round(time.monotonic()-start,2),log=str(log),results=str(E/(a.stage+'-results')))
(E/(a.stage+'-run.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2),flush=True)
raise SystemExit(0 if passed else 1)
