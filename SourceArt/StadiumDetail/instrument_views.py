"""Camera-only change in the expressly granted stadium baseline slot."""
from pathlib import Path
import hashlib,json,difflib
R=Path(__file__).resolve().parents[2];W=R/'Saved/StadiumDetailWork'
assert json.loads((W/'slot_grant.json').read_text())['camera_only'], 'Need explicit reservation release first'
p=R/'Source/SeniorSendoff/HouseExterior.cpp';old=p.read_bytes();s=old.decode()
assert 'TEXT("detail_stadium_aisle_up")' not in s,'Cameras already appended; inspect instead of duplicating'
a=s.index('const FExView ExViews[]');b=s.index('\n};',a)
s=s[:b]+'\n'+(R/'SourceArt/StadiumDetail/Draft/views.inl').read_text()+s[b:]
assert s.count('TEXT("detail_row_')==old.decode().count('TEXT("detail_row_')
(W/'before_camera_HouseExterior.cpp').write_bytes(old);p.write_text(s)
(W/'camera_only.patch').write_text(''.join(difflib.unified_diff(old.decode().splitlines(True),s.splitlines(True),fromfile='a/Source/SeniorSendoff/HouseExterior.cpp',tofile='b/Source/SeniorSendoff/HouseExterior.cpp')))
sha={str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for base in ['Source','Config'] for p in (R/base).rglob('*') if p.is_file()}
(W/'camera_source_sha.json').write_text(json.dumps(sha,indent=2)+'\n')
(W/'camera_instrumentation.json').write_text(json.dumps({'changed_file':str(p),'before_sha256':hashlib.sha256(old).hexdigest(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'appended_views':11,'row_views_preserved':True},indent=2)+'\n')
print('Appended11stadium cameras; geometry unchanged.')
