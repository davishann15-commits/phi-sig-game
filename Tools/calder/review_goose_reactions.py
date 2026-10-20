"""Real local-player normal movement, authority hiss/waddle and walking recovery."""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('--mode',choices=['walk','sprint','crouch'],default='walk');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
run=ROOT/'Saved/CalderWork/runs'/args.tag;assert not run.exists()
inv=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines();assert not any(('/UnrealEditor' in x and 'Services' not in x) or x.endswith('/SeniorSendoff') for x in inv)
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ('Source','Config') for q in (ROOT/folder).rglob('*') if q.is_file()};began=time.monotonic()
subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),args.tag,'460','-RenderOffScreen','-MetalOffscreenOnly','-muteaudio','-NoTraceServer','-CalderGooseReactionsTrial','-CalderGooseReactionReview','-CalderGooseReactionMode='+args.mode,'-CalderWildlifeClockOffset=300','-ExecCmds=t.MaxFPS 0,r.VSync 0,r.InstanceCulling.OcclusionCull 1'],cwd=ROOT,check=True)
out=run/'Saved/CalderGooseReactionReview';log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');result=(out/'result.txt').read_text() if (out/'result.txt').exists() else 'MISSING RESULT';images=sorted(out.glob('detail_goose_reaction_*.png'));unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items())
passed=result.startswith('PASS scope=nativeGoosePlayerReaction') and len(images)==4 and all(q.stat().st_size>1000 for q in images) and 'CALDER_POND_FAILED' not in log and 'Fatal error:' not in log and unchanged
report={'machine_passed':passed,'scope':'Real local player approach,authority hiss/retreat;walk case waits real recovery;no two-peer/audio acceptance','mode':args.mode,'result':result.strip(),'captures':[str(q.relative_to(ROOT)) for q in images],'captures_inspection_pending':True,'source_config_frozen':unchanged,'source_sha256':freeze,'seconds':round(time.monotonic()-began,2),'two_peer_accepted':False,'calls_accepted':False,'photographic_master_accepted':False,'whole_phase3_complete':False}
receipt=run.parent/(args.tag+'_reactions.json');assert not receipt.exists();receipt.write_text(json.dumps(report,indent=2)+'\n');print(('GOOSE_REACTION_MACHINE_PASS ' if passed else 'GOOSE_REACTION_MACHINE_FAIL ')+result.strip()+' '+str(receipt),flush=True);raise SystemExit(0 if passed else 1)
