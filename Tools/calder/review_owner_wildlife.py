"""Fresh native runs for owner's deer correction and normal geese/crows."""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('--case',choices=['deer','preview','calm','medium','crow','default','audio','ambient'],required=True);p.add_argument('--cache-check',action='store_true');a=p.parse_args()
assert re.fullmatch('[a-z0-9_]+',a.tag)
run=ROOT/'Saved/CalderWork/runs'/a.tag;assert not run.exists()
inv=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines();assert not any(('/UnrealEditor' in s and 'Services' not in s) or s.endswith('/SeniorSendoff') for s in inv)
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ['Source','Config'] for q in (ROOT/folder).rglob('*') if q.is_file()}
flags=['-RenderOffScreen','-MetalOffscreenOnly','-muteaudio','-NoTraceServer','-ExecCmds=t.MaxFPS 60,r.VSync 0,r.InstanceCulling.OcclusionCull 1']
if a.case=='deer':flags+=['-NoCalderBirds','-CalderWildlifeReview','-CalderSceneTime=300','-CalderWildlifeSoundDump','-CalderWildlifeRecord=120'];folder='CalderWildlifeReview';timeout=600
elif a.case=='preview':flags+=['-NoCalderBirds','-nosound','-CalderWildlifeReview','-CalderWildlifeReviewMode=campus','-CalderSceneTime=300'];folder='CalderWildlifeReview';timeout=240
elif a.case=='calm':flags+=['-NoCalderBirds','-nosound','-CalderDeerCalmReview'];folder='CalderDeerCalmReview';timeout=240
elif a.case=='medium':flags+=['-NoCalderBirds','-nosound','-CalderWildlifeSoleReview','-CalderSceneTime=300'];folder='CalderWildlifeSoleReview';timeout=240
elif a.case=='crow':flags+=['-CalderCrowRoostReview','-CalderSceneTime=300'];folder='CalderCrowRenderReview';timeout=240
elif a.case=='audio':flags+=['-CalderGooseAudioReview','-CalderWildlifeClockOffset=1100','-CalderWildlifeSoundDump','-CalderWildlifeRecord=60'];folder='CalderGooseAudioReview';timeout=360
elif a.case=='ambient':
 # Exterior samples are capped at600 natively. Three600-frame views at10fps
 # deliberately cover >120 real seconds, including the110s raven cue.
 # This is clock/audio integration only; performance and reaction timing use
 # their separate original-rate tests. No artificial scene-time pin/offset.
 flags=[f if not f.startswith('-ExecCmds=') else '-ExecCmds=t.MaxFPS 10,r.VSync 0,r.InstanceCulling.OcclusionCull 1' for f in flags]
 flags+=['-HouseExteriorViews','-CleanViews','-ExteriorViewSamples=600','-ExteriorViewWarmup=4','-ExteriorViewFilter=detail_woods_edge,detail_woods_20m','-CalderWildlifeSoundDump','-CalderWildlifeRecord=112'];folder='HouseExteriorViews';timeout=300
else:flags+=['-HouseExteriorViews','-CleanViews','-CalderSceneTime=300','-ExteriorViewSamples=120','-ExteriorViewWarmup=4','-ExteriorViewFilter=detail_woods_edge,detail_woods_20m,detail_woods_deep'];folder='HouseExteriorViews';timeout=240
if a.cache_check:flags.append('-CalderDeerPoseCacheReview')
began=time.monotonic();job=subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),a.tag,str(timeout),*flags],cwd=ROOT,check=True,capture_output=True,text=True)
print(job.stdout,flush=True)
out=run/'Saved'/folder;result=(out/'result.txt').read_text() if (out/'result.txt').exists() else ''
log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');images=sorted(out.glob('*.png'))
unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items())
passed=unchanged and 'Fatal error:' not in log and 'CALDER_WILDLIFE_FAILED' not in log and 'CALDER_POND_FAILED' not in log and 'CALDER_SKY_FAILED' not in log and 'TIMEOUT' not in job.stdout
cache_rows=re.findall(r'CALDER_DEER_POSE_CACHE id=(\d+) hits=(\d+) misses=(\d+) reusedMediumKeys=(\d+) freshReferenceChecks=(\d+) maxPositionErrorCm=([0-9.]+) maxAngleErrorDegrees=([0-9.]+) maxScaleError=([0-9.]+)',log)
if a.cache_check:passed &= 'CALDER_DEER_POSE_CACHE_FAILED' not in log and sum(int(r[4]) for r in cache_rows)>=1000
passed &= bool(re.search(r'^PASS\b',result,re.M)) and not re.search(r'^FAIL\b',result,re.M) if a.case not in ['default','ambient'] else 'HOUSE_EXTERIOR_VIEWS done' in log
if a.case in ['default','ambient','audio','crow']:passed &= 'CALDER_POND_DRAW' in log and 'CALDER_CROW_ROOST_READY' in log
if a.case=='ambient':passed &= 'CALDER_CROW_CAW' in log and 'CALDER_RAVEN_CROAK' in log and (run/'Saved/CalderWildlifeSound/WildlifeMix.wav').exists()
receipt={'machine_passed':bool(passed),'case':a.case,'result':result,'captures':[str(x.relative_to(ROOT)) for x in images],'captures_inspection_pending':True,'source_sha256':freeze,'source_frozen':unchanged,'seconds':round(time.monotonic()-began,2),'bird_enabling_trial_flags':False,'ambient_rate_limit_for_observation_only':10 if a.case=='ambient' else None,'pose_cache_reference_checked':a.cache_check,'pose_cache_statistics':cache_rows,'whole_phase3_complete':False}
target=run.parent/(a.tag+'_owner.json');assert not target.exists();target.write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({k:receipt[k] for k in ['machine_passed','case','seconds','result']},indent=2),flush=True)
raise SystemExit(0 if passed else 1)
