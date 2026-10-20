"""Opt-in native flight integration evidence; normal cadence/audio remain open."""
import argparse,csv,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
run=ROOT/'Saved/CalderWork/runs'/args.tag;assert not run.exists()
inv=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines();assert not any(('/UnrealEditor' in x and 'Services' not in x) or x.endswith('/SeniorSendoff') for x in inv)
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ('Source','Config') for q in (ROOT/folder).rglob('*') if q.is_file()}
began=time.monotonic()
subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),args.tag,'270','-RenderOffScreen','-MetalOffscreenOnly','-muteaudio','-NoTraceServer','-CalderWildlifeSkyTrial','-CalderWildlifeSkyReview','-CalderSceneTime=300','-ExecCmds=t.MaxFPS 0,r.VSync 0,r.InstanceCulling.OcclusionCull 1'],cwd=ROOT,check=True)
out=run/'Saved/CalderWildlifeSkyReview';log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');result=(out/'result.txt').read_text() if (out/'result.txt').exists() else 'MISSING RESULT'
rows=list(csv.DictReader((out/'flight.csv').open())) if (out/'flight.csv').exists() else [];images=sorted(out.glob('detail_wild_*.png'))
expected={'goose_v0','goose_v1','crow_stream','bats_pond','bats_glen','bats_lamp'}
stages={name:sum(r['stage']==name and r['visible']=='1' for r in rows) for name in expected}
unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items())
passed=result.startswith('PASS scope=explicitNativeSkyMotion') and len(images)==18 and all(q.stat().st_size>1000 for q in images) and all(n>=20 for n in stages.values()) and all(float(r['errorCm'])<=.2 and (r['insideBounds']=='1' or r['visible']=='0') for r in rows) and 'CALDER_SKY_READY components=4' in log and 'Fatal error:' not in log and unchanged
report={'machine_passed':passed,'scope':'Explicit native flight/render setup with review-only moving clock; no normal cadence/audio acceptance','result':result.strip(),'visible_samples_by_stage':stages,'captures':[str(q.relative_to(ROOT)) for q in images],'captures_inspection_pending':True,'source_config_frozen':unchanged,'source_sha256':freeze,'seconds':round(time.monotonic()-began,2),'normal_cadence_accepted':False,'two_peer_accepted':False,'calls_pending':True,'landing_pending':True,'photographic_master_accepted':False,'whole_phase3_complete':False}
receipt=run.parent/(args.tag+'_flight.json');assert not receipt.exists();receipt.write_text(json.dumps(report,indent=2)+'\n');print(('SKY_FLIGHT_MACHINE_PASS ' if passed else 'SKY_FLIGHT_MACHINE_FAIL ')+result.strip()+' '+str(receipt),flush=True)
raise SystemExit(0 if passed else 1)
