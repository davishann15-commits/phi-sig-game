"""Serialized current-scene evidence, with frozen source and camera-only instrumentation."""
from pathlib import Path
from datetime import datetime,timezone
import csv,hashlib,json,re,subprocess,time
R=Path(__file__).resolve().parents[2];W=R/'Saved/StadiumDetailWork'
assert json.loads((W/'slot_grant.json').read_text())['camera_only']
assert not (W/'before_receipt.json').exists(),'Never overwrite evidence'
before={str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for base in ['Source','Config'] for p in (R/base).rglob('*') if p.is_file()}
(W/'before_slot_sha.json').write_text(json.dumps(before,indent=2)+'\n')
rows=[];filter='detail_stadium_,detail_calder_eye_P2,detail_calder_eye_P3,detail_layout_aerial'
with (W/'before.log').open('w') as out:
 def command(args):
  out.write('\nSTART '+repr(args)+'\n');out.flush();code=subprocess.run(args,cwd=R,stdout=out,stderr=subprocess.STDOUT).returncode;out.flush();return code
 assert command(['python3','SourceArt/StadiumDetail/instrument_views.py'])==0
 assert command(['bash','Saved/CalderWork/tools/build.sh'])==0
 (W/'camera_build.log').write_bytes((R/'Saved/CalderWork/build.log').read_bytes())
 frozen=json.loads((W/'camera_source_sha.json').read_text())
 def game(tag,phase,samples,static=False):
  for rel,sha in frozen.items():assert hashlib.sha256((R/rel).read_bytes()).hexdigest()==sha,'Source drift '+rel
  dst=R/'Saved/CalderWork/runs'/tag;assert not dst.exists(),tag
  args=['bash','Saved/CalderWork/tools/run_game.sh',tag,'900','-RenderOffScreen','-MetalOffscreenOnly','-nosound','-HouseExteriorViews','-CleanViews','-ExteriorViewFilter='+filter,'-ExteriorViewSamples='+str(samples),'-ExteriorViewWarmup=4','-CalderNightfallSeconds='+str(phase),'-NoStadiumDetail','-ExecCmds=t.MaxFPS 0']
  if static:args+=['-CampusLayoutReview','-CampusWoodsReview','-FraternityBoundaryReview','-SurroundingsPolishReview','-WoodsPassReview']
  t=time.monotonic();rc=command(args);log=(dst/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');folder=dst/'Saved/HouseExteriorViews';frames=list(csv.DictReader((folder/'frametimes.csv').open()));assert len(frames)==18,(tag,len(frames));assert len(list(folder.glob('*.png')))==18
  assert 'Fatal error' not in log and 'REVIEW FAIL' not in log
  expected=[x for x in log.splitlines() if any(k in x for k in ['HOUSE_EXTERIOR built','CAMPUS_WOODS_REVIEW','CAMPUS_LAYOUT_REVIEW PASS','WOODS_PASS_ORIGINAL_TREES','WOODS_PASS_REVIEW'])]
  rows.append({'tag':tag,'phase':phase,'samples':samples,'wrapper_exit':rc,'wall_seconds':time.monotonic()-t,'frames':frames,'counts':expected,'load_seconds':re.findall(r'Took ([0-9.]+) seconds to LoadMap',log)})
  (W/'before_progress.json').write_text(json.dumps(rows,indent=2)+'\n')
 for i in range(1,4):game('stadium_before_20261018_perf'+str(i),0,300,i==1)
 game('stadium_before_20261018_night',1800,60)
 for rel,sha in frozen.items():assert hashlib.sha256((R/rel).read_bytes()).hexdigest()==sha,rel
 result={'finished_utc':datetime.now(timezone.utc).isoformat(),'camera_only':True,'geometry_unchanged':True,'source_frozen':True,'runs':rows}
 (W/'before_receipt.json').write_text(json.dumps(result,indent=2)+'\n')
 print('Stadium before evidence complete, four native18-view runs; inspect images before acceptance.')
