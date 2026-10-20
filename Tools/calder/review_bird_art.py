"""Native fixed-pose model review; does not accept ambient flight or full Phase3."""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];RUNS=ROOT/'Saved/CalderWork/runs'
p=argparse.ArgumentParser();p.add_argument('tag');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
run=RUNS/args.tag;assert not run.exists()
inventory=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines()
assert not any(('/UnrealEditor' in item and 'Services' not in item) or item.endswith('/SeniorSendoff') for item in inventory)
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ('Source','Config') for q in (ROOT/folder).rglob('*') if q.is_file()}
began=time.monotonic()
subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),args.tag,'240','-RenderOffScreen','-MetalOffscreenOnly','-muteaudio','-NoTraceServer','-CalderWildlifeBirdArtReview','-CalderSceneTime=300','-ExecCmds=t.MaxFPS 0,r.VSync 0,r.InstanceCulling.OcclusionCull 1'],cwd=ROOT,check=True)
log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');out=run/'Saved/CalderWildlifeBirdArtReview'
assert 'PASS scope=explicitNativeBirdModelsAndFixedPoses captures=16' in (out/'result.txt').read_text()
assert 'CALDER_BIRD_ART_FAILED' not in log and 'Fatal error:' not in log
assert 'CALDER_BIRD_ART_READY components=4' in log and 'crc=0f3b2bb7' in log
rows=re.findall(r'CALDER_BIRD_ART_MESH species=(\w+) lods=3 triangles=([0-9/]+) UV=(\d+) usedUV=5 controls=16 customFloats=225',log)
models={name:[int(n) for n in counts.split('/')] for name,counts,uv in rows}
assert all(5<=int(uv)<=8 for name,counts,uv in rows)
assert set(models)=={'goose','crow','brown_bat','red_bat'} and all(len(x)==3 and x[0]>x[1]>x[2]>0 for x in models.values())
files=sorted(out.glob('detail_bird_*.png'));assert len(files)==16 and all(q.stat().st_size>1000 for q in files)
assert all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items())
report={'machine_passed':True,'scope':'Explicit fixed-pose native bird models only; flight/cadence/ambient calls not accepted','models_lod_triangles':models,'actual_uv_channels':{name:int(uv) for name,counts,uv in rows},'used_uv_channels':5,'captures':[str(q.relative_to(ROOT)) for q in files],'captures_inspection_pending':True,'eye_cm':170,'source_config_frozen':True,'source_sha256':freeze,'seconds':round(time.monotonic()-began,2),'whole_phase3_complete':False}
receipt=RUNS/(args.tag+'_art.json');assert not receipt.exists();receipt.write_text(json.dumps(report,indent=2)+'\n');print('BIRD_ART_MACHINE_PASS captures=16 '+str(receipt),flush=True)
