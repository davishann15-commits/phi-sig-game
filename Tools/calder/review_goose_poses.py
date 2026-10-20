"""Eight explicit native goose poses. No landing/server/feet acceptance."""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
run=ROOT/'Saved/CalderWork/runs'/args.tag;assert not run.exists()
inv=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines();assert not any(('/UnrealEditor' in x and 'Services' not in x) or x.endswith('/SeniorSendoff') for x in inv)
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ('Source','Config') for q in (ROOT/folder).rglob('*') if q.is_file()}
began=time.monotonic()
subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),args.tag,'240','-RenderOffScreen','-MetalOffscreenOnly','-muteaudio','-NoTraceServer','-CalderWildlifeBirdArtReview','-CalderGoosePoseReview','-CalderSceneTime=300','-ExecCmds=t.MaxFPS 0,r.VSync 0,r.InstanceCulling.OcclusionCull 1'],cwd=ROOT,check=True)
log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');out=run/'Saved/CalderWildlifeBirdArtReview';result=(out/'result.txt').read_text() if (out/'result.txt').exists() else 'MISSING RESULT';images=sorted(out.glob('detail_goose_pose_*.png'))
unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items())
passed=result.startswith('PASS scope=explicitNativeGooseFixedPoses captures=8 expected=8') and len(images)==8 and all(q.stat().st_size>1000 for q in images) and 'CALDER_BIRD_ART_FAILED' not in log and 'Fatal error:' not in log and unchanged
report={'machine_passed':passed,'scope':'Eight explicit fixed-pose goose controls only; no actual landing/pond/feet/server acceptance','result':result.strip(),'poses':['flight','set_wing_glide','flare','fold_transition','folded_raised_neck','folded_lower_neck','hiss_bill_open','folded_reverse_view'],'captures':[str(q.relative_to(ROOT)) for q in images],'captures_inspection_pending':True,'source_config_frozen':unchanged,'source_sha256':freeze,'seconds':round(time.monotonic()-began,2),'landing_behavior_accepted':False,'server_handoff_accepted':False,'ground_feet_accepted':False,'photographic_master_accepted':False,'whole_phase3_complete':False}
receipt=run.parent/(args.tag+'_poses.json');assert not receipt.exists();receipt.write_text(json.dumps(report,indent=2)+'\n');print(('GOOSE_POSES_MACHINE_PASS ' if passed else 'GOOSE_POSES_MACHINE_FAIL ')+result.strip()+' '+str(receipt),flush=True)
raise SystemExit(0 if passed else 1)
