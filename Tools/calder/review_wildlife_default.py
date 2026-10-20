"""Native default activation/restoration proof, run only after enabling defaults.
Uses original exterior views without any wildlife-enabling review/trial flag.
Three unique sequential games. This is activation evidence, not final timings.
"""
import argparse,csv,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];RUNS=ROOT/'Saved/CalderWork/runs'
p=argparse.ArgumentParser();p.add_argument('tag');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
assert not any('/UnrealEditor.app/Contents/MacOS/UnrealEditor' in item for item in subprocess.check_output(['ps','-axo','comm='],text=True).splitlines()),'Editor/game must be closed'
freeze={str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for folder in ('Source','Config') for f in (ROOT/folder).rglob('*') if f.is_file()}
report=RUNS/(args.tag+'_default.json');assert not report.exists();rows=[];began=time.monotonic();views={'detail_woods_edge','detail_woods_20m','detail_woods_deep'}
for name,extra,enabled in [('on',[],True),('off',['-NoCalderWildlife'],False),('terrain_off',['-NoCalderTerrain'],False)]:
 run=RUNS/(args.tag+'_'+name);assert not run.exists();print('WILDLIFE_DEFAULT start '+name,flush=True)
 flags=['-RenderOffScreen','-MetalOffscreenOnly','-muteaudio','-NoTraceServer','-HouseExteriorViews','-CleanViews','-CalderSceneTime=300','-ExteriorViewSamples=120','-ExteriorViewWarmup=4','-ExteriorViewFilter='+','.join(sorted(views)),'-ExecCmds=t.MaxFPS 0,r.VSync 0,r.InstanceCulling.OcclusionCull 1']
 # No CalderWildlifeTrial/Review/PathsReview flag may make this positive.
 assert not any(re.search(r'^-CalderWildlife(?:Trial|.*Review)',flag,re.I) for flag in flags+extra)
 assert not any(re.search(r'^-CalderGoose(?:PondTrial|.*Review)',flag,re.I) for flag in flags+extra)
 subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),run.name,'240',*flags,*extra],cwd=ROOT,check=True)
 log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');data=list(csv.DictReader((run/'Saved/HouseExteriorViews/frametimes.csv').open()));assert {r['view'] for r in data}==views
 assert 'CALDER_BIRD_ART_' not in log,'Explicit bird-art setup leaked into an ordinary launch'
 if enabled:
  assert 'CALDER_SKY_READY' in log and 'CALDER_CROW_ROOST_READY' in log and 'CALDER_POND_DRAW' in log,'Normal wildlife did not load'
 else:
  assert 'CALDER_SKY_' not in log and 'CALDER_POND_' not in log,'Disabled wildlife loaded bird components'
 assert 'HOUSE_EXTERIOR_VIEWS done' in log and 'Fatal error:' not in log and 'CALDER_WILDLIFE_FAILED' not in log
 if enabled:
  assert 'CALDER_WILDLIFE_ROSTER species=deer groups=3 counts=3/4/3 animals=10' in log and 'customFloats=253' in log and 'CALDER_WILDLIFE_SOUND_SUBMIX_REGISTERED' in log
  match=re.search(r'CALDER_WILDLIFE_PATHS clear=(\d+)/(\d+).*hash=([0-9a-f]{8})',log);assert match and match[1]==match[2]
 else:assert 'CALDER_WILDLIFE_ROSTER' not in log and 'CALDER_WILDLIFE_SOUND_READY' not in log and 'CALDER_WILDLIFE_DRAW' not in log
 assert 'crc=0f3b2bb7' in log
 assert all(hashlib.sha256((ROOT/f).read_bytes()).hexdigest()==sha for f,sha in freeze.items()),'Source/config changed during default proof'
 rows.append({'case':name,'passed':True,'wildlife_expected':enabled,'run':str(run.relative_to(ROOT)),'wildlife_enabling_flag':False,'captures':3,'capture_inspection_pending':True});print('WILDLIFE_DEFAULT PASS '+name,flush=True)
report.write_text(json.dumps({'passed':True,'scope':'Normal default on/off/NoTerrain activation proof; does not replace full timings or roster/master acceptance','source_frozen':True,'source_sha256':freeze,'cases':rows,'seconds':round(time.monotonic()-began,2),'whole_phase3_complete':False},indent=2)+'\n');print('WILDLIFE_DEFAULT complete '+str(report),flush=True)
