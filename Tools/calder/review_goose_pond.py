"""Native background arrival/departure sequence. Reactions/audio remain separate."""
import argparse,csv,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
run=ROOT/'Saved/CalderWork/runs'/args.tag;assert not run.exists()
inv=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines();assert not any(('/UnrealEditor' in x and 'Services' not in x) or x.endswith('/SeniorSendoff') for x in inv)
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ('Source','Config') for q in (ROOT/folder).rglob('*') if q.is_file()}
began=time.monotonic()
subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),args.tag,'340','-RenderOffScreen','-MetalOffscreenOnly','-muteaudio','-NoTraceServer','-CalderGoosePondTrial','-CalderGoosePondReview','-CalderWildlifeClockOffset=1035','-ExecCmds=t.MaxFPS 0,r.VSync 0,r.InstanceCulling.OcclusionCull 1'],cwd=ROOT,check=True)
out=run/'Saved/CalderGoosePondReview';log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');result=(out/'result.txt').read_text() if (out/'result.txt').exists() else 'MISSING RESULT'
rows=list(csv.DictReader((out/'pond.csv').open())) if (out/'pond.csv').exists() else [];images=sorted(out.glob('detail_wild_pond_*.png'))
phases={str(i):sum(r['phase']==str(i) for r in rows) for i in range(1,6)}
unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items())
passed=result.startswith('PASS scope=explicitNativePondSequence') and len(images)==8 and all(q.stat().st_size>1000 for q in images) and all(n>=10 for n in phases.values()) and all(float(r['errorCm'])<=.2 and r['inBounds']=='1' for r in rows) and 'CALDER_POND_ROSTER states=16' in log and 'Fatal error:' not in log and unchanged
report={'machine_passed':passed,'scope':'Native shared-clock background pond sequence with diagnostic starting offset; no reactions/audio acceptance','result':result.strip(),'visible_actual_instance_samples_by_phase':phases,'captures':[str(q.relative_to(ROOT)) for q in images],'captures_inspection_pending':True,'source_config_frozen':unchanged,'source_sha256':freeze,'seconds':round(time.monotonic()-began,2),'normal_cadence_accepted':False,'two_peer_accepted':False,'calls_pending':True,'reactions_pending':True,'photographic_master_accepted':False,'whole_phase3_complete':False}
receipt=run.parent/(args.tag+'_pond.json');assert not receipt.exists();receipt.write_text(json.dumps(report,indent=2)+'\n');print(('GOOSE_POND_MACHINE_PASS ' if passed else 'GOOSE_POND_MACHINE_FAIL ')+result.strip()+' '+str(receipt),flush=True)
raise SystemExit(0 if passed else 1)
