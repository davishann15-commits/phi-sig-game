"""Accept a reviewed, explicitly released camera-only shared slot; never edits runtime Source."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,re,sys
R=Path(__file__).resolve().parents[2];W=R/'Saved/FraternityRowWork'
assert '--release-reviewed' in sys.argv,'Read the other owner release/patch before accepting its camera overlay'
assert json.loads((W/'before_checks_receipt.json').read_text())['cameras_complete']
assert not (W/'integration_receipt.json').exists(),'Row already integrated; this is only a before-stage camera guard'
base=json.loads((W/'baseline_source_receipt.json').read_text())['sha256']
for rel,sha in base.items():
    if rel!='Source/SeniorSendoff/HouseExterior.cpp':assert hashlib.sha256((R/rel).read_bytes()).hexdigest()==sha,'Unexpected shared-slot drift: '+rel
known={k for k in base if k.startswith(('Source/','Config/'))}
actual={str(p.relative_to(R)) for name in ['Source','Config'] for p in (R/name).rglob('*') if p.is_file()}
assert actual==known,'Shared slot added/removed Source or Config files'
old=(W/'camera_only_HouseExterior.cpp').read_text();new=(R/'Source/SeniorSendoff/HouseExterior.cpp').read_text()
def split(s):
    a=s.index('const FExView ExViews[]');b=s.index('{',a);c=s.index('\n};',b);return s[:b+1],s[b+1:c],s[c:]
op,ob,ot=split(old);np,nb,nt=split(new)
assert op==np and ot==nt and nb.startswith(ob),'Shared slot changed noncamera code or existing camera entries'
extra=nb[len(ob):];plain=re.sub(r'/\*.*?\*/|//[^\n]*','',extra,flags=re.S)
num=r'[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?[fF]?'
vec=r'FVector\(\s*'+num+r'\s*,\s*'+num+r'\s*,\s*'+num+r'\s*\)'
entry=r'\{\s*TEXT\("(detail_stadium_[A-Za-z0-9_]+)"\)\s*,\s*'+vec+r'\s*,\s*'+vec+r'(?:\s*,\s*'+num+r'){0,2}\s*\}\s*,'
names=re.findall(entry,plain);rest=re.sub(entry,'',plain)
assert names and not rest.strip(),'Only literal appended detail_stadium_* capture entries are accepted'
assert len(set(names))==len(names),'Duplicate new camera names'
old_names=re.findall(r'TEXT\("([^"]+)"\)',ob)
assert not set(names)&set(old_names),'New camera name duplicates an existing one'
extra_guard=json.loads((W/'camera_extra_guards.json').read_text())
for rel,sha in extra_guard.items():assert hashlib.sha256((R/rel).read_bytes()).hexdigest()==sha,'Unexpected project/map drift: '+rel
(W/'camera_overlay_receipt.json').write_text(json.dumps({'reviewed_utc':datetime.now(timezone.utc).isoformat(),'source_only_append_validated':True,'prior_sha256':hashlib.sha256(old.encode()).hexdigest(),'after_sha256':hashlib.sha256(new.encode()).hexdigest(),'added_views':names},indent=2)+'\n')
print('Validated camera-only overlay:',','.join(names))
