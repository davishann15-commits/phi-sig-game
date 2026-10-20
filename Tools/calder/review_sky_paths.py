"""Nonrendered native terrain/obstacle flight-data audit; no visual acceptance."""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('--landing',action='store_true');p.add_argument('--shore',action='store_true');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
run=ROOT/'Saved/CalderWork/runs'/args.tag;assert not run.exists()
inventory=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines()
assert not any(('/UnrealEditor' in q and 'Services' not in q) or q.endswith('/SeniorSendoff') for q in inventory)
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ('Source','Config') for q in (ROOT/folder).rglob('*') if q.is_file()}
began=time.monotonic()
flags=['-CalderGooseShoreDataReview'] if args.shore else ['-CalderGoosePathsReview'] if args.landing else []
subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),args.tag,'240','-nullrhi','-muteaudio','-NoTraceServer','-CalderWildlifeLogicProbe','-CalderWildlifeSkyPathsReview','-CalderSceneTime=300',*flags],cwd=ROOT,check=True)
logpath=run/'Saved/Logs/SeniorSendoff.log';log=logpath.read_text(errors='replace');resultpath=run/'Saved/CalderWildlifeSkyPathsReview/result.txt'
result=resultpath.read_text() if resultpath.exists() else 'MISSING RESULT'
unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items())
scope='gooseShoreDataOnly' if args.shore else 'gooseLandingDataOnly' if args.landing else 'skyGraphDataOnly'
passed=result.startswith('PASS scope='+scope) and 'Fatal error:' not in log and 'physicsQueries=0' in result and unchanged
report={'passed':passed,'scope':('Nonrendered native shore-access graph data only' if args.shore else 'Nonrendered native goose landing/departure graph data only' if args.landing else 'Nonrendered native sky graph data only'),'result':result.strip(),'rejects':re.findall(r'CALDER_(?:SKY_PATH|GOOSE_PATH)_REJECT[^\n]*',log),'source_config_frozen':unchanged,'source_sha256':freeze,'seconds':round(time.monotonic()-began,2),'model_render_acceptance':False,'ambient_flight_acceptance':False,'server_handoff_pending':True,'landing_pending':True,'whole_phase3_complete':False}
receipt=run.parent/(args.tag+'_paths.json');assert not receipt.exists();receipt.write_text(json.dumps(report,indent=2)+'\n');print(('SKY_PATHS_DATA_PASS ' if passed else 'SKY_PATHS_DATA_FAIL ')+result.strip()+' '+str(receipt),flush=True)
raise SystemExit(0 if passed else 1)
