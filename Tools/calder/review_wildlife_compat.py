"""Current deer integration against original reviews and optional farm geometry.
Sequential native games, unique evidence, no original review/threshold changes.
"""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];RUNS=ROOT/'Saved/CalderWork/runs'
p=argparse.ArgumentParser();p.add_argument('tag');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
assert not any('/UnrealEditor.app/Contents/MacOS/UnrealEditor' in item for item in subprocess.check_output(['ps','-axo','comm='],text=True).splitlines()),'A user editor/game is open'
cases=[('original_reviews',['-CampusLayoutReview','-CampusWoodsReview','-FraternityBoundaryReview','-SurroundingsPolishReview','-ParkingLotWalkReview','-WoodsPassReview'],['CampusLayoutReview','CampusWoodsReview','FraternityBoundaryReview','SurroundingsPolishReview','ParkingLotReview','WoodsPassReview']),('farm_paths',['-CalderWildlifePathsReview','-CalderFarmTrial','-CalderFarmSolidTrial','-CalderFarmTreeTrial'],['CalderWildlifePathsReview'])]
freeze={str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in (ROOT/'Source/SeniorSendoff').rglob('*') if f.is_file()}
report=RUNS/(args.tag+'_compat.json');assert not report.exists();rows=[];began=time.monotonic()
for name,flags,expected in cases:
 run=RUNS/(args.tag+'_'+name);assert not run.exists()
 print('WILDLIFE_COMPAT start '+name,flush=True)
 subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),run.name,'240','-RenderOffScreen','-MetalOffscreenOnly','-muteaudio','-NoTraceServer','-CalderWildlifeTrial','-CalderSceneTime=300','-ExecCmds=t.MaxFPS 60,r.InstanceCulling.OcclusionCull 1',*flags],cwd=ROOT,check=True)
 log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');assert 'Fatal error:' not in log and 'CALDER_WILDLIFE_ROSTER' in log and 'customFloats=225' in log and 'CALDER_WILDLIFE_SOUND_SUBMIX_REGISTERED' in log
 checks={}
 for directory in expected:
  f=run/'Saved'/directory/'result.txt';s=f.read_text();ok=bool(re.search(r'^PASS\b',s,re.M)) and not re.search(r'^FAIL\b',s,re.M);checks[directory]={'passed':ok,'result':str(f.relative_to(ROOT))};assert ok,(directory,s)
 assert 'crc=0f3b2bb7' in log,'Original tree CRC changed'
 assert all(hashlib.sha256((ROOT/f).read_bytes()).hexdigest()==sha for f,sha in freeze.items()),'Source changed during compatibility checks'
 rows.append({'case':name,'passed':True,'checks':checks});print('WILDLIFE_COMPAT PASS '+name,flush=True)
report.write_text(json.dumps({'passed':True,'scope':'Deer integration; full original master review/hill-route/load chain still separate','native_render':True,'source_frozen':True,'source_sha256':freeze,'cases':rows,'seconds':round(time.monotonic()-began,2),'whole_phase3_complete':False},indent=2)+'\n');print('WILDLIFE_COMPAT complete '+str(report),flush=True)
