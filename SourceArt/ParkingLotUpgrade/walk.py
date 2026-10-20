from pathlib import Path
import subprocess,shutil,json,time
b=Path(__file__).resolve().parent;r=b.parents[1];e=b/'evidence';u=Path('/tmp/parking-lot-walk');u.mkdir(exist_ok=True)
assert subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode!=0
cmd=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(r/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-game','-windowed','-ResX=1280','-ResY=720','-NoSplash','-NoTraceServer','-UserDir='+str(u),'-AbsLog='+str(e/'walk.log'),'-ParkingLotWalkReview','-CampusWoodsReview','-FraternityBoundaryReview','-WoodsSceneReview','-WoodsUndergrowthReview','-ExecCmds=sg.ViewDistanceQuality 2,sg.AntiAliasingQuality 2,sg.ShadowQuality 2,sg.GlobalIlluminationQuality 2,sg.ReflectionQuality 2,sg.PostProcessQuality 2,sg.TextureQuality 2,sg.EffectsQuality 2,sg.FoliageQuality 2,sg.ShadingQuality 2,t.MaxFPS 0,r.VSync 0']
t=time.monotonic()
with (e/'walk-stdout.log').open('w')as f:
 p=subprocess.Popen(cmd,stdout=f,stderr=subprocess.STDOUT)
 try:code=p.wait(timeout=200)
 except subprocess.TimeoutExpired:p.terminate();code=p.wait(timeout=20)
if (u/'Saved').exists():shutil.copytree(u/'Saved',e/'walk',dirs_exist_ok=True)
log=(e/'walk.log').read_text(errors='replace');passed=code==0 and 'PARKING_LOT_REVIEW PASS Player walked' in log and 'CAMPUS_WOODS_REVIEW PASS' in log
(e/'walk-run.json').write_text(json.dumps(dict(status='PASS' if passed else 'FAIL',exit_code=code,seconds=time.monotonic()-t,scope='Actual character movement from the parking driveway, along the central drive aisle and shared accessible aisle, onto the house walkway; single player'),indent=2));print('PASS' if passed else 'FAIL',code,flush=True)
raise SystemExit(0 if passed else 1)
