"""Native actual-tree path data. Does not enable or accept crow render/audio."""
import argparse,csv,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');a=p.parse_args();assert re.fullmatch('[a-z0-9_]+',a.tag)
run=ROOT/'Saved/CalderWork/runs'/a.tag;assert not run.exists()
inventory=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines();assert not any(('/UnrealEditor' in s and 'Services' not in s) or s.endswith('/SeniorSendoff') for s in inventory)
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ('Source','Config') for q in (ROOT/folder).rglob('*') if q.is_file()}
began=time.monotonic();job=subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),a.tag,'300','-nullrhi','-nosound','-NoTraceServer','-CalderWildlifeLogicProbe','-CalderCrowRoostDataReview','-CalderSceneTime=300'],cwd=ROOT,check=True,capture_output=True,text=True);print(job.stdout,flush=True)
out=run/'Saved/CalderCrowRoostReview';result=(out/'result.txt').read_text() if (out/'result.txt').exists() else 'MISSING RESULT';log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace')
def rows(name):return list(csv.DictReader((out/name).open())) if (out/name).exists() else []
targets,cadence,routes,trees=rows('targets.csv'),rows('cadence.csv'),rows('routes.csv'),rows('trees.csv')
unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items())
old=json.loads((ROOT/'Saved/CalderWork/runs/codex_goose_honk_native_v3_audio.json').read_text())['source_sha256']
tower_unchanged=all(freeze[n]==h for n,h in old.items() if Path(n).name.startswith('HouseClockTower'))
passed=result.startswith('PASS scope=crowRoostDataOnly') and len(targets)==15 and len({(r['batch'],r['instance']) for r in targets})==15 and all(r['insideTreeBounds']=='1' for r in targets) and len(cadence)==12 and all(3<=int(r['count'])<=15 and float(r['lastArrivalSeconds'])<150 for r in cadence) and len(routes)>10000 and unchanged and tower_unchanged and 'Fatal error:' not in log and 'TIMEOUT after' not in job.stdout
report={'passed':passed,'scope':'Actual native world crow roost path data only','result':result.strip(),'tree_count':len(trees),'targets':len(targets),'route_nodes':len(routes),'cadence':cadence,'source_config_frozen':unchanged,'source_sha256':freeze,'tower_sources_unchanged_from_verified_honk':tower_unchanged,'seconds':round(time.monotonic()-began,2),'native_render_accepted':False,'calls_accepted':False,'pine_taxonomy_verified':False,'whole_phase3_complete':False}
receipt=run.parent/(a.tag+'_crow_paths.json');assert not receipt.exists();receipt.write_text(json.dumps(report,indent=2)+'\n');print(('CROW_ROOST_DATA_PASS ' if passed else 'CROW_ROOST_DATA_FAIL ')+result.strip()+' '+str(receipt),flush=True);raise SystemExit(0 if passed else 1)
