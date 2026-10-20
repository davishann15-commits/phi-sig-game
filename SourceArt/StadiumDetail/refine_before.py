"""Camera-only native follow-up, after the first frozen baseline queue has fully exited."""
from pathlib import Path
from datetime import datetime,timezone
import json,hashlib,subprocess,re,math,csv,difflib,time
R=Path(__file__).resolve().parents[2];W=R/'Saved/StadiumDetailWork';assert (W/'before_receipt.json').exists()
assert not (W/'refined_before_receipt.json').exists()
active=[x for x in subprocess.check_output(['ps','-axo','pid=,comm='],text=True).splitlines() if 'UnrealEditor.app/Contents/MacOS/UnrealEditor' in x or 'UnrealBuildTool' in x or 'Binaries/Mac/SeniorSendoff' in x];assert not active,active
frozen=json.loads((W/'camera_source_sha.json').read_text())
for rel,sha in frozen.items():assert hashlib.sha256((R/rel).read_bytes()).hexdigest()==sha,rel
p=R/'Source/SeniorSendoff/HouseExterior.cpp';old=p.read_text();s=old
def bowl(a,r,z):
 a=math.radians(a);return (2400+math.cos(a)*5600*r,-13300+math.sin(a)*3800*r,z)
def chord(k,r,u,d,z):
 a=bowl(k*9,r,0);b=bowl((k+1)*9,r,0);length=math.dist(a,b);dx=(b[0]-a[0])/length;dy=(b[1]-a[1])/length;return (a[0]+dx*u+dy*d,a[1]+dy*u-dx*d,z)
def vec(a):return 'FVector('+','.join(f'{x:.2f}' for x in a)+')'
poses=[('portal',(245,-10110,115),(1232,-9951,455),73),('concession',chord(7,.885,500,250,115),chord(7,.885,500,27,175),72),('turnstiles',bowl(76.5,1.10,113),bowl(76.5,.995,130),70)]
for name,pos,target,fov in poses:
 pattern=r'    \{ TEXT\("detail_stadium_'+name+r'"\),[^\n]+\n';new='    { TEXT("detail_stadium_'+name+'"), '+vec(pos)+', '+vec(target)+', '+str(fov)+'.f },\n';s,n=re.subn(pattern,lambda m:new,s);assert n==1,(name,n)
assert s.count('TEXT("detail_row_')==14
(W/'before_refinement_HouseExterior.cpp').write_text(old);p.write_text(s)
origin=(W/'before_camera_HouseExterior.cpp').read_text();(W/'refined_camera_only.patch').write_text(''.join(difflib.unified_diff(origin.splitlines(True),s.splitlines(True),fromfile='a/Source/SeniorSendoff/HouseExterior.cpp',tofile='b/Source/SeniorSendoff/HouseExterior.cpp')))
frozen['Source/SeniorSendoff/HouseExterior.cpp']=hashlib.sha256(p.read_bytes()).hexdigest();(W/'refined_camera_source_sha.json').write_text(json.dumps(frozen,indent=2)+'\n')
names=['detail_stadium_portal','detail_stadium_concession','detail_stadium_turnstiles'];runs=[]
def guard():
 for rel,sha in frozen.items():assert hashlib.sha256((R/rel).read_bytes()).hexdigest()==sha,rel
with (W/'refined_before.log').open('x') as out:
 code=subprocess.run(['bash','Saved/CalderWork/tools/build.sh'],cwd=R,stdout=out,stderr=subprocess.STDOUT).returncode;out.flush();assert code==0
 (W/'refined_camera_build.log').write_bytes((R/'Saved/CalderWork/build.log').read_bytes())
 for suffix,phase,samples in [('r1',0,300),('r2',0,300),('r3',0,300),('night',1800,60)]:
  guard();tag='stadium_before_refined_20261018_'+suffix;dst=R/'Saved/CalderWork/runs'/tag;assert not dst.exists(),tag
  args=['bash','Saved/CalderWork/tools/run_game.sh',tag,'900','-RenderOffScreen','-MetalOffscreenOnly','-nosound','-HouseExteriorViews','-CleanViews','-ExteriorViewFilter='+','.join(names),'-ExteriorViewSamples='+str(samples),'-ExteriorViewWarmup=4','-CalderNightfallSeconds='+str(phase),'-NoStadiumDetail','-ExecCmds=t.MaxFPS 0']
  out.write('\nSTART '+repr(args)+'\n');out.flush();t=time.monotonic();subprocess.run(args,cwd=R,stdout=out,stderr=subprocess.STDOUT);out.flush();log=(dst/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');folder=dst/'Saved/HouseExteriorViews';frames=list(csv.DictReader((folder/'frametimes.csv').open()));assert [x['view'] for x in frames]==names;assert len(list(folder.glob('*.png')))==3;assert 'HOUSE_EXTERIOR_VIEWS done' in log and 'Fatal error' not in log
  diagnostics=[x for x in log.splitlines() if 'HOUSE_EXTERIOR_VIEW ' in x];inside=[x for x in diagnostics if x.split('inside=',1)[-1].strip()];runs.append({'tag':tag,'phase':phase,'samples':samples,'warmup_seconds':4,'frames':frames,'diagnostics':diagnostics,'camera_overlap_lines':inside,'wall_seconds':time.monotonic()-t});(W/'refined_before_progress.json').write_text(json.dumps(runs,indent=2)+'\n')
 guard();(W/'refined_before_receipt.json').write_text(json.dumps({'finished_utc':datetime.now(timezone.utc).isoformat(),'camera_only':True,'source_config_frozen':True,'HouseExterior_sha256':frozen['Source/SeniorSendoff/HouseExterior.cpp'],'runs':runs,'geometry_changes':0,'all_original_and_Row_views_preserved':True,'visual_inspection_pending':True},indent=2)+'\n')
 print('Refined before captures complete; verify3dusk/night poses before source/engine release.')
