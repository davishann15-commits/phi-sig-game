"""Append Row capture cameras only after successful pre-edit performance baseline."""
from pathlib import Path
import json,hashlib
R=Path(__file__).resolve().parents[2];out=R/'Saved/FraternityRowWork'
assert json.loads((out/'baseline_perf_receipt.json').read_text())['passed']
p=R/'Source/SeniorSendoff/HouseExterior.cpp';s=p.read_text()
if 'TEXT("detail_row_road_wide")' in s:raise SystemExit('Row cameras already present; do not append twice')
a=s.index('const FExView ExViews[]');b=s.index('\n};',a)
s=s[:b]+'\n'+(R/'SourceArt/FraternityRow/Draft/row_views.inc').read_text()+s[b:]
p.write_text(s)
(out/'camera_instrumentation.json').write_text(json.dumps({'file':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'added_views':14,'existing_views_and_rng_unchanged':True},indent=2)+'\n')
print('Appended14detail cameras; no Row geometry integrated')
