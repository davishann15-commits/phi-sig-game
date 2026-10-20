"""Guarded source-only integration. Baseline/camera evidence and unchanged source hashes are required."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess
R=Path(__file__).resolve().parents[2];D=R/'SourceArt/FraternityRow/Draft';W=R/'Saved/FraternityRowWork'
assert json.loads((W/'baseline_perf_receipt.json').read_text())['passed']
assert json.loads((W/'before_checks_receipt.json').read_text())['cameras_complete']
assert not (W/'integration_receipt.json').exists(),'Inspect existing integration before reapplying'
base=json.loads((W/'baseline_source_receipt.json').read_text())['sha256']
base['Source/SeniorSendoff/HouseExterior.cpp']=json.loads((W/'camera_instrumentation.json').read_text())['sha256']
overlay=W/'camera_overlay_receipt.json'
if overlay.exists():
    cam=json.loads(overlay.read_text());assert cam['source_only_append_validated']
    base['Source/SeniorSendoff/HouseExterior.cpp']=cam['after_sha256']
for rel,sha in json.loads((W/'camera_extra_guards.json').read_text()).items():assert hashlib.sha256((R/rel).read_bytes()).hexdigest()==sha,'Protected project/map drift: '+rel
for rel,sha in base.items():
    assert hashlib.sha256((R/rel).read_bytes()).hexdigest()==sha,'Unrelated source change: '+rel
assert (D/'row_walk.inc').exists() and 'void RowRoutes(' in (D/'row_walk.inc').read_text()
subprocess.run(['python3','SourceArt/FraternityRow/prepare_integration.py'],cwd=R,check=True)
files={p.name:p for p in (D/'Integrated').glob('*') if p.is_file() and p.suffix in ['.cpp','.h']}
for n in ['HouseFraternityRow.h','HouseFraternityRow.cpp','HouseFraternityRowYards.h','HouseFraternityRowYards.cpp','HouseCampusInteriorsRow.cpp','HouseFraternityRowReview.cpp','HouseCampusMultiplayerReview.cpp']:
    assert (D/n).exists(),'Missing reviewed candidate '+n
    files[n]=D/n
sources=R/'Source/SeniorSendoff';receipt={}
for name,p in sorted(files.items()):
    out=sources/name;rel=str(out.relative_to(R))
    if rel not in base:assert not out.exists(),'Unexpected existing new-source file: '+rel
    original=out.read_bytes() if out.exists() else None
    backup=W/'pre_integration'/rel
    if original is not None:backup.parent.mkdir(parents=True,exist_ok=True);backup.write_bytes(original)
    receipt[rel]={'before_sha256':hashlib.sha256(original).hexdigest() if original is not None else None,'after_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'candidate':str(p)}
for name,p in sorted(files.items()):(sources/name).write_bytes(p.read_bytes())
(W/'integration_receipt.json').write_text(json.dumps({'utc':datetime.now(timezone.utc).isoformat(),'source_only':True,'files':receipt},indent=2)+'\n')
print('Integrated',len(receipt),'source files; no Config/Content/map writes.')
