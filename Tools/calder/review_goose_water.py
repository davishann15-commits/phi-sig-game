"""Native pooled water effects/contact geometry and images. No audio acceptance."""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('--entry',action='store_true');p.add_argument('--close',action='store_true');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
run=ROOT/'Saved/CalderWork/runs'/args.tag;assert not run.exists()
inv=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines();assert not any(('/UnrealEditor' in x and 'Services' not in x) or x.endswith('/SeniorSendoff') for x in inv)
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ('Source','Config') for q in (ROOT/folder).rglob('*') if q.is_file()};began=time.monotonic();flags=['-CalderGooseEffectsEntryReview'] if args.entry else []
if args.close:flags.append('-CalderGooseEffectsCloseReview')
# This review makes no audio claim. Disabling the audio device also avoids the
# CoreAudio shutdown stall observed after the v1 geometry results were written.
process=subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),args.tag,'280','-RenderOffScreen','-MetalOffscreenOnly','-nosound','-NoTraceServer','-CalderGooseEffectsTrial','-CalderGooseEffectsReview','-CalderWildlifeClockOffset='+('1030' if args.entry else '1100'),*flags,'-ExecCmds=t.MaxFPS 0,r.VSync 0,r.InstanceCulling.OcclusionCull 1'],cwd=ROOT,check=True,capture_output=True,text=True);print(process.stdout,flush=True);timed_out='TIMEOUT after' in process.stdout
out=run/'Saved/CalderGooseEffectsReview';log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');result=(out/'result.txt').read_text() if (out/'result.txt').exists() else 'MISSING RESULT';images=sorted(out.glob('detail_goose_water_*.png'));unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items())
passed=result.startswith('PASS scope=nativeGooseWaterEffects') and len(images)==8 and all(q.stat().st_size>1000 for q in images) and 'CALDER_POND_EFFECT_FAILED' not in log and 'Fatal error:' not in log and unchanged and not timed_out
report={'machine_passed':passed,'process_timeout':timed_out,'scope':'Native actual effect transforms/customdata/vertices atwater andshore;imageinspectionrequired;noaudio/fullperformance acceptance','mode':'shoreEntry' if args.entry else 'arrival','close_camera':args.close,'result':result.strip(),'captures':[str(q.relative_to(ROOT)) for q in images],'captures_inspection_pending':True,'source_config_frozen':unchanged,'source_sha256':freeze,'seconds':round(time.monotonic()-began,2),'audio_accepted':False,'full_performance_accepted':False,'photographic_master_accepted':False,'whole_phase3_complete':False}
receipt=run.parent/(args.tag+'_water.json');assert not receipt.exists();receipt.write_text(json.dumps(report,indent=2)+'\n');print(('GOOSE_WATER_MACHINE_PASS ' if passed else 'GOOSE_WATER_MACHINE_FAIL ')+result.strip()+' '+str(receipt),flush=True);raise SystemExit(0 if passed else 1)
