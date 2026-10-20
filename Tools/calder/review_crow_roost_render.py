"""Rendered actual ISM crow routes. Calls/normal cadence/full acceptance separate."""
import argparse,csv,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');a=p.parse_args();assert re.fullmatch('[a-z0-9_]+',a.tag)
run=ROOT/'Saved/CalderWork/runs'/a.tag;assert not run.exists()
inventory=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines();assert not any(('/UnrealEditor' in s and 'Services' not in s) or s.endswith('/SeniorSendoff') for s in inventory)
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ('Source','Config') for q in (ROOT/folder).rglob('*') if q.is_file()}
began=time.monotonic();job=subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),a.tag,'300','-RenderOffScreen','-MetalOffscreenOnly','-nosound','-NoTraceServer','-CalderCrowRoostTrial','-CalderCrowRoostReview','-CalderSceneTime=300','-ExecCmds=t.MaxFPS 60,r.VSync 0,r.InstanceCulling.OcclusionCull 1'],cwd=ROOT,check=True,capture_output=True,text=True);print(job.stdout,flush=True)
out=run/'Saved/CalderCrowRenderReview';result=(out/'result.txt').read_text() if (out/'result.txt').exists() else 'MISSING RESULT';log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');images=sorted(out.glob('detail_crow_*.png'));rows=list(csv.DictReader((out/'submitted.csv').open())) if (out/'submitted.csv').exists() else []
unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items())
passed=result.startswith('PASS scope=explicitNativeCrowRoostRender') and len(images)==8 and all(p.stat().st_size>1000 for p in images) and len(rows)>=120 and all(float(r['errorCm'])<=.2 and r['bounds']==r['anchors']=='1' for r in rows) and 'hash=b7ca5f24' in result and 'baseSky=74bb095c' in result and unchanged and 'Fatal error:' not in log and 'TIMEOUT after' not in job.stdout
report={'machine_passed':passed,'scope':'Explicit native submitted-root/bounds/reentry/roost hiding; photographic review still required','result':result.strip(),'submitted_rows':len(rows),'captures':[str(p.relative_to(ROOT)) for p in images],'captures_inspection_pending':True,'source_config_frozen':unchanged,'source_sha256':freeze,'seconds':round(time.monotonic()-began,2),'calls_accepted':False,'normal_cadence_accepted':False,'pine_taxonomy_verified':False,'whole_phase3_complete':False}
receipt=run.parent/(a.tag+'_crow_render.json');assert not receipt.exists();receipt.write_text(json.dumps(report,indent=2)+'\n');print(('CROW_RENDER_MACHINE_PASS ' if passed else 'CROW_RENDER_MACHINE_FAIL ')+result.strip()+' '+str(receipt),flush=True);raise SystemExit(0 if passed else 1)
