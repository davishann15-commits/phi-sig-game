"""Native goose PCM/mixer/beak evidence. Objective analysis/listening is separate."""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('--mode',choices=['arrival','hiss','locations','off'],default='arrival');p.add_argument('--recheck',action='store_true');p.add_argument('--normal',action='store_true',help='Use ordinary bird defaults without the enabling trial');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
run=ROOT/'Saved/CalderWork/runs'/args.tag
prior_path=run.parent/(args.tag+'_audio.json');prior=json.loads(prior_path.read_text()) if args.recheck else None
assert run.exists() if args.recheck else not run.exists()
inv=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines();assert not any(('/UnrealEditor' in x and 'Services' not in x) or x.endswith('/SeniorSendoff') for x in inv)
freeze=prior['source_sha256'] if args.recheck else {str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ('Source','Config') for q in (ROOT/folder).rglob('*') if q.is_file()};began=time.monotonic()
locations=args.mode in ('locations','off')
flags=['-CalderGooseReactionReview','-CalderGooseReactionMode=walk'] if args.mode=='hiss' else ['-CalderWildlifeReview','-CalderWildlifeReviewMode=sound'] if locations else []
if args.mode=='off':flags.append('-NoCalderWildlifeSound')
if not locations:flags.append('-CalderWildlifeRecord='+('120' if args.mode=='hiss' else '60'))
if args.recheck:
 from types import SimpleNamespace
 process=SimpleNamespace(stdout='TIMEOUT after' if prior['process_timeout'] else 'RECHECK existing native evidence; no Unreal launch');print(process.stdout,flush=True)
else:
 process=subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),args.tag,'600' if args.mode=='hiss' else '360','-RenderOffScreen','-MetalOffscreenOnly','-muteaudio','-NoTraceServer',*([] if args.normal else ['-CalderGooseAudioTrial']),'-CalderGooseAudioReview','-CalderWildlifeSoundDump','-CalderWildlifeClockOffset='+('160' if locations else '300' if args.mode=='hiss' else '1100'),*flags,'-ExecCmds=t.MaxFPS 60,r.VSync 0,r.InstanceCulling.OcclusionCull 1'],cwd=ROOT,check=True,capture_output=True,text=True);print(process.stdout,flush=True)
output=run/'Saved'/('CalderWildlifeReview' if locations else 'CalderGooseReactionReview' if args.mode=='hiss' else 'CalderGooseAudioReview');result=(output/'result.txt').read_text() if (output/'result.txt').exists() else 'MISSING RESULT';log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');images=sorted(output.glob('detail_*.png'));sounds=run/'Saved/CalderWildlifeSound'
expected=['DeerBlow','LeafCrunch','NightBed','Creek','LonePeeper','GooseHiss','GooseSplash','GooseHonk',*[f'GooseHonkEcho{i}' for i in range(2,6)]]
waves={n:str((sounds/(n+'.wav')).relative_to(ROOT)) for n in expected if (sounds/(n+'.wav')).exists()};unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items());timeout='TIMEOUT after' in process.stdout
calls={k:len(re.findall(r'CALDER_WILDLIFE_CALL kind='+str(k)+r'\b',log)) for k in range(5,12)}
passed=bool(re.search(r'^PASS\b',result,re.M)) and not re.search(r'^FAIL\b',result,re.M) and len(images)==(0 if args.mode=='off' else 4) and all(q.stat().st_size>1000 for q in images) and 'Fatal error:' not in log and unchanged and not timeout
if args.mode=='off':passed &= len(waves)==0 and '0wildlifeaudio' in result
else:
 passed &= len(waves)==12 and 'CALDER_WILDLIFE_SOUND_SUBMIX_REGISTERED' in log
 if locations:passed &= all((sounds/('Wildlife_'+n+'.wav')).exists() for n in ('frat_lawn','inside_house','inside_campus','near_creek'))
 else:passed &= (sounds/'WildlifeMix.wav').exists()
 if args.mode=='hiss':passed &= calls[5]>0 and calls[6]>0
 elif args.mode=='arrival':passed &= sum(calls[k] for k in range(7,12))>0 and calls[6]>0
report={'machine_passed':bool(passed),'mode':args.mode,'scope':'Native integration/files only; PCM/mix analysis, listening, indoor/lawn and full performance separate','result':result.strip(),'process_timeout':timeout,'calls_by_kind':calls,'captures':[str(q.relative_to(ROOT)) for q in images],'captures_inspection_pending':True,'source_wavs':waves,'mix':str((sounds/'WildlifeMix.wav').relative_to(ROOT)) if not locations else None,'location_wavs':[str(q.relative_to(ROOT)) for q in sounds.glob('Wildlife_*.wav')] if locations else [],'source_config_frozen':unchanged,'source_sha256':freeze,'seconds':round(time.monotonic()-began,2),'subjective_listening_accepted':False,'full_performance_accepted':False,'whole_phase3_complete':False}
report['existing_evidence_recheck']=args.recheck;report['original_receipt']=str(prior_path.relative_to(ROOT)) if args.recheck else None
receipt=run.parent/(args.tag+('_audio_recheck_v1.json' if args.recheck else '_audio.json'));assert not receipt.exists();receipt.write_text(json.dumps(report,indent=2)+'\n');print(('GOOSE_AUDIO_MACHINE_PASS ' if passed else 'GOOSE_AUDIO_MACHINE_FAIL ')+result.strip()+' '+str(receipt),flush=True);raise SystemExit(0 if passed else 1)
