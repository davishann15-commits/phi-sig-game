"""Native real-rate background shore gait. No server reaction acceptance."""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('--medium',action='store_true');p.add_argument('--outward',action='store_true');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
run=ROOT/'Saved/CalderWork/runs'/args.tag;assert not run.exists()
inv=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines();assert not any(('/UnrealEditor' in x and 'Services' not in x) or x.endswith('/SeniorSendoff') for x in inv)
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ('Source','Config') for q in (ROOT/folder).rglob('*') if q.is_file()}
began=time.monotonic();flags=['-CalderGooseShoreMediumReview'] if args.medium else [];flags+=['-CalderGooseShoreOutwardReview'] if args.outward else []
subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),args.tag,'240','-RenderOffScreen','-MetalOffscreenOnly','-muteaudio','-NoTraceServer','-CalderGooseShoreTrial','-CalderGooseShoreReview','-CalderWildlifeClockOffset='+('1190' if args.outward else '1000'),*flags,'-ExecCmds=t.MaxFPS 0,r.VSync 0,r.InstanceCulling.OcclusionCull 1'],cwd=ROOT,check=True)
out=run/'Saved/CalderGooseShoreReview';log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');result=(out/'result.txt').read_text() if (out/'result.txt').exists() else 'MISSING RESULT';images=sorted(out.glob('detail_goose_shore_*.png'))
unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items())
passed=result.startswith('PASS scope=nativeGooseShoreBackground') and len(images)==8 and all(q.stat().st_size>1000 for q in images) and 'CALDER_POND_FAILED' not in log and 'Fatal error:' not in log and unchanged
report={'machine_passed':passed,'scope':'Opt-in native background shore gait with diagnostic start offset; no player reaction acceptance','result':result.strip(),'tier':'medium' if args.medium else 'full','direction':'outward' if args.outward else 'return','captures':[str(q.relative_to(ROOT)) for q in images],'captures_inspection_pending':True,'source_config_frozen':unchanged,'source_sha256':freeze,'seconds':round(time.monotonic()-began,2),'normal_cadence_accepted':False,'player_reactions_accepted':False,'two_peer_accepted':False,'calls_pending':True,'photographic_master_accepted':False,'whole_phase3_complete':False}
receipt=run.parent/(args.tag+'_shore.json');assert not receipt.exists();receipt.write_text(json.dumps(report,indent=2)+'\n');print(('GOOSE_SHORE_MACHINE_PASS ' if passed else 'GOOSE_SHORE_MACHINE_FAIL ')+result.strip()+' '+str(receipt),flush=True)
raise SystemExit(0 if passed else 1)
