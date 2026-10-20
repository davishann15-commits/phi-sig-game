"""One bounded task: respect the shared idle window, snapshot sources, run the required baseline helper."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,time,sys
R=Path(__file__).resolve().parents[2]
W=R/'Saved/FraternityRowWork';W.mkdir(exist_ok=True)
release=json.loads((R/'Saved/CalderWork/nightfall_20261018_v1/reservation_release.json').read_text())
assert release['source_engine_reservation_released'] and release['all_project_write_freeze_ended']
deadline=datetime.fromisoformat(release['released_utc']).timestamp()+1200
print('Idle deadline UTC:',datetime.fromtimestamp(deadline,timezone.utc).isoformat(),flush=True)
while time.time()<deadline:time.sleep(min(5,deadline-time.time()))
commands=subprocess.run(['ps','-axo','pid=,comm='],capture_output=True,text=True,check=True).stdout
busy=[line for line in commands.splitlines() if line.strip().split(maxsplit=1)[-1].endswith(('/UnrealEditor','/UnrealEditor-Cmd','/SeniorSendoff','/UnrealBuildTool'))]
if busy:
    print('Baseline not started: an editor/game/build is active (names only):',busy,flush=True);sys.exit(2)
snapshot=W/'baseline_source'
if snapshot.exists():raise SystemExit('Refusing to overwrite existing baseline snapshot')
files={}
for name in ['Source','Config']:
    for p in sorted((R/name).rglob('*')):
        if p.is_file():
            rel=p.relative_to(R);dest=snapshot/rel;dest.parent.mkdir(parents=True,exist_ok=True);payload=p.read_bytes();dest.write_bytes(payload);files[str(rel)]=hashlib.sha256(payload).hexdigest()
for rel in ['Content/Story/Maps/Chapter01_House.umap']:
    files[rel]=hashlib.sha256((R/rel).read_bytes()).hexdigest()
(W/'baseline_source_receipt.json').write_text(json.dumps({'utc':datetime.now(timezone.utc).isoformat(),'snapshot':str(snapshot),'sha256':files},indent=2)+'\n')
print('Native baseline starting; Source/Config frozen until helper completes.',flush=True)
tag='row_before_20261018_r1'
if any((R/'Saved/CalderWork/runs').glob(tag+'*')):raise SystemExit('Run tag already exists')
with (W/'baseline_perf.log').open('w') as log:
    proc=subprocess.run(['bash','Saved/CalderWork/tools/perf_ab.sh',tag],cwd=R,stdout=log,stderr=subprocess.STDOUT)
print('Baseline helper exit:',proc.returncode,flush=True)
import csv
runs=[]
for mode in ['on','off']:
    for i in range(1,4):
        p=R/'Saved/CalderWork/runs'/f'{tag}_{mode}{i}'/'Saved/HouseExteriorViews/frametimes.csv'
        rows=list(csv.DictReader(p.open())) if p.exists() else []
        runs.append({'run':f'{mode}{i}','views':len(rows),'csv':str(p),'complete':len(rows)==11})
passed=proc.returncode==0 and all(x['complete'] for x in runs)
(W/'baseline_perf_receipt.json').write_text(json.dumps({'passed':passed,'tag':tag,'helper_exit':proc.returncode,'runs':runs,'finished_utc':datetime.now(timezone.utc).isoformat()},indent=2)+'\n')
print(json.dumps({'baseline_complete':passed,'runs':runs}),flush=True)
sys.exit(0 if passed else 1)
