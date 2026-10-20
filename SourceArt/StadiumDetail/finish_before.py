"""Finish camera-only baseline; exact filter-derived19-view expectation includes P3_tower.
The initial driver's18-view count error is retained; no native route/threshold/sample/timeout changes.
"""
from pathlib import Path
from datetime import datetime,timezone
import csv,hashlib,json,re,subprocess,time
R=Path(__file__).resolve().parents[2];W=R/'Saved/StadiumDetailWork';assert not (W/'before_receipt.json').exists()
frozen=json.loads((W/'camera_source_sha.json').read_text());original=json.loads((W/'before_slot_sha.json').read_text())
keys=['detail_stadium_','detail_calder_eye_P2','detail_calder_eye_P3','detail_layout_aerial'];source=(R/'Source/SeniorSendoff/HouseExterior.cpp').read_text();start=source.index('const FExView ExViews[]');stop=source.index('\n};',start)
names=[n for n in re.findall(r'TEXT\("([^"]+)"\)',source[start:stop]) if any(k in n for k in keys)]
assert len(names)==19 and len(set(names))==19 and 'detail_calder_eye_P3_tower' in names,names
def guard():
 for rel,sha in frozen.items():assert hashlib.sha256((R/rel).read_bytes()).hexdigest()==sha,'Source/Config drift '+rel
 assert set(frozen)=={str(p.relative_to(R)) for base in ['Source','Config'] for p in (R/base).rglob('*') if p.is_file()}
guard();rows=[]
with (W/'before_finish.log').open('x') as out:
 def collect(tag,phase,samples,seconds=None):
  dst=R/'Saved/CalderWork/runs'/tag;log=(dst/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');folder=dst/'Saved/HouseExteriorViews';frames=list(csv.DictReader((folder/'frametimes.csv').open()));actual=[x['view'] for x in frames]
  assert actual==names,(tag,actual,names)
  assert len(list(folder.glob('*.png')))==19
  assert 'HOUSE_EXTERIOR_VIEWS done' in log and 'Fatal error' not in log and 'REVIEW FAIL' not in log
  if tag.endswith('perf1'):
   for marker in ['CAMPUS_LAYOUT_REVIEW PASS','CAMPUS_WOODS_REVIEW PASS','FRATERNITY_BOUNDARY_REVIEW PASS','SURROUNDINGS_POLISH_REVIEW PASS','WOODS_PASS_REVIEW PASS']:assert marker in log,marker
   assert 'crc=0f3b2bb7' in log and 'actualExteriorPointAndSpotLights=53' in log
  guard();counts=[x for x in log.splitlines() if any(k in x for k in ['HOUSE_EXTERIOR built','CAMPUS_WOODS_REVIEW','CAMPUS_LAYOUT_REVIEW PASS','WOODS_PASS_ORIGINAL_TREES','WOODS_PASS_REVIEW'])]
  rows.append({'tag':tag,'phase':phase,'samples':samples,'warmup_seconds':4,'views':19,'wall_seconds':seconds,'frames':frames,'counts':counts,'load_seconds':re.findall(r'Took ([0-9.]+) seconds to LoadMap',log)})
  (W/'before_progress.json').write_text(json.dumps(rows,indent=2)+'\n')
 collect('stadium_before_20261018_perf1',0,300)
 for tag,phase,samples in [('stadium_before_20261018_perf2',0,300),('stadium_before_20261018_perf3',0,300),('stadium_before_20261018_night',1800,60)]:
  guard();assert not (R/'Saved/CalderWork/runs'/tag).exists(),tag
  args=['bash','Saved/CalderWork/tools/run_game.sh',tag,'900','-RenderOffScreen','-MetalOffscreenOnly','-nosound','-HouseExteriorViews','-CleanViews','-ExteriorViewFilter='+','.join(keys),'-ExteriorViewSamples='+str(samples),'-ExteriorViewWarmup=4','-CalderNightfallSeconds='+str(phase),'-NoStadiumDetail','-ExecCmds=t.MaxFPS 0']
  out.write('\nSTART '+repr(args)+'\n');out.flush();t=time.monotonic();rc=subprocess.run(args,cwd=R,stdout=out,stderr=subprocess.STDOUT).returncode;out.flush();collect(tag,phase,samples,time.monotonic()-t)
 guard();changed=[r for r in original if original[r]!=frozen[r]];assert changed==['Source/SeniorSendoff/HouseExterior.cpp'],changed
 result={'finished_utc':datetime.now(timezone.utc).isoformat(),'camera_only':True,'geometry_unchanged':True,'source_config_frozen':True,'only_source_changed':changed,'expected_views':names,'oracle_repair':'Initial driver expected18but existing filter also selects P3_tower. Raised exact required count to19with same flags/samples/timeouts; original failure retained in before.log/session86211.','runs':rows}
 (W/'before_receipt.json').write_text(json.dumps(result,indent=2)+'\n')
 print('Camera-only baseline complete: four native19-view runs; images need inspection before release.')
