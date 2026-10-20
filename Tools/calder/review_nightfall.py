"""Native phase, clock and restoration checks for the owner's 30-minute night.

Phase pins are limited to the existing self-exiting exterior camera review.
The normal clock run has no night pin or wildlife scene-time pin. Its10fps
cap observes real elapsed time only, not performance or reaction latency.
"""
import argparse,datetime,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('--case',choices=['dusk','middle','night','hold','clock','off','terrain_off','landscape_off'],required=True);p.add_argument('--fill',type=float);p.add_argument('--quick',action='store_true');a=p.parse_args()
assert re.fullmatch('[a-z0-9_]+',a.tag)
run=ROOT/'Saved/CalderWork/runs'/a.tag;assert not run.exists()
inventory=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines()
assert not any(('/UnrealEditor' in s and 'Services' not in s) or s.endswith('/SeniorSendoff') for s in inventory)
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ['Source','Config'] for q in (ROOT/folder).rglob('*') if q.is_file()}
phase={'dusk':0,'middle':900,'night':1800,'hold':3600}.get(a.case)
views='detail_calder_eye_P,front_street,street_along,detail_woods_20m,detail_woods_deep,detail_perf_in_lib,gallery_hall'
if a.case in ['hold','off','terrain_off','landscape_off']:views='front_street,detail_woods_20m'
if a.case=='clock':views='detail_woods_edge'
if a.quick:views='detail_woods_20m,front_street,gallery_hall,detail_perf_in_lib'
flags=['-RenderOffScreen','-MetalOffscreenOnly','-muteaudio','-NoTraceServer','-HouseExteriorViews','-CleanViews','-CampusInteriorsAll','-ExteriorViewWarmup=4','-ExteriorViewFilter='+views,'-ExteriorViewSamples='+('600' if a.case=='clock' else '45'),'-ExecCmds=t.MaxFPS '+('10' if a.case=='clock' else '60')+',r.VSync 0,r.InstanceCulling.OcclusionCull 1']
if phase is not None:flags+=['-CalderNightfallSeconds='+str(phase),'-CalderSceneTime=300']
if a.fill is not None:flags+=['-CalderNightFill='+str(a.fill)]
if a.case=='off':flags+=['-NoCalderNightfall']
if a.case=='terrain_off':flags+=['-NoCalderTerrain']
if a.case=='landscape_off':flags+=['-NoCalderLandscape']
began=time.monotonic();job=subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),a.tag,'360',*flags],cwd=ROOT,check=True,capture_output=True,text=True)
log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace')
pattern=r'CALDER_NIGHTFALL seconds=([0-9.]+) alpha=([0-9.]+) duration=1800 pinned=(\d) nativeWall=([0-9.]+) sunPitch=([0-9.]+) skyIntensity=([0-9.]+) fogDensityOpacityUnchanged=(\d) netMode=(\d)'
rows=[dict(zip(['seconds','alpha','pinned','wall','sun_pitch','sky_intensity','fog_preserved','net_mode'],map(float,row))) for row in re.findall(pattern,log)]
passed='HOUSE_EXTERIOR_VIEWS done' in log and 'TIMEOUT' not in job.stdout and 'Fatal error:' not in log
clock_window=None
if a.case in ['off','terrain_off','landscape_off']:passed &= not rows and 'CALDER_NIGHTFALL_READY' not in log
else:
 passed &= bool(rows) and 'CALDER_NIGHTFALL_READY' in log
 for row in rows:
  u=min(1.,row['seconds']/1800.);passed &= abs(row['alpha']-u*u*(3.-2.*u))<=1e-7 and row['fog_preserved']==1
  if phase is not None:passed &= row['pinned']==1 and row['seconds']==phase
 if a.case=='clock':
  passed &= all(row['pinned']==0 for row in rows) and len(rows)>=5
  # Measure the actual600-frame renderer sample, excluding map generation
  # and screenshot/shutdown pauses. Those stalls clamp World DeltaSeconds
  # and do not represent elapsed interactive play. Keep the1s tolerance.
  lines=log.splitlines();fps=next((l for l in lines if 'HOUSE_EXTERIOR_FPS view=' in l),None)
  if fps:
   stamp=lambda l:datetime.datetime.strptime(l[1:l.index(']')],'%Y.%m.%d-%H.%M.%S:%f')
   end=stamp(fps);average=float(re.search(r'avg_ms=([0-9.]+)',fps)[1]);start=end-datetime.timedelta(milliseconds=average*600)
   times=[stamp(l) for l in lines if re.search(pattern,l)]
   active=[r for r,t in zip(rows,times) if start<=t<=end]
   clock_window={'renderer_sample_start_utc':start.isoformat(),'renderer_sample_end_utc':end.isoformat(),'rows':active,'excluded_loading_and_screenshot_rows':len(rows)-len(active),'tolerance_seconds':1.}
   passed &= len(active)>=5
   if len(active)>=2:
    elapsed=active[-1]['seconds']-active[0]['seconds'];wall=active[-1]['wall']-active[0]['wall'];clock_window.update({'game_elapsed_seconds':elapsed,'wall_elapsed_seconds':wall,'error_seconds':abs(elapsed-wall)});passed &= elapsed>=45 and abs(elapsed-wall)<=1.
  else:passed=False
unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items());passed &= unchanged
images=sorted((run/'Saved/HouseExteriorViews').glob('*.png'));passed &= bool(images)
result={'machine_passed':bool(passed),'case':a.case,'seconds':round(time.monotonic()-began,2),'nightfall_duration_seconds':1800,'pinned_phase_seconds':phase,'diagnostic_fill_override':a.fill,'quick_visual_probe':a.quick,'clock_rows':rows,'actual_renderer_clock_window':clock_window,'source_config_frozen':unchanged,'source_sha256':freeze,'captures':[str(q.relative_to(ROOT)) for q in images],'captures_inspection_pending':True,'performance_acceptance':False,'whole_calder_master_complete':False}
out=run.parent/(a.tag+'_nightfall.json');assert not out.exists();out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:result[k] for k in ['machine_passed','case','seconds','pinned_phase_seconds']},indent=2),flush=True)
raise SystemExit(0 if passed else 1)
