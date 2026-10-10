"""Rebuild only the mini hall wall, including the original entry upper infill."""
from pathlib import Path
B=Path(__file__).resolve().parent
s=(B/'build_art.py').read_text()
exec(s.split('# Tiling micro-surface masks.')[0],globals())
F=-361.966;C=-54.;T=18
exec(s[s.index('def wall('):s.index("g='LaundryWalls'")],globals())
layout=(B/'junction_layout.py').read_text()
exec(layout[:layout.index("g='RedWalls'")],globals())
exec(layout[layout.index("g='HallWalls'"):layout.index("g='BasementTrim'")],globals())
assert len(MAN['assets'])==1 and MAN['assets'][0]['name']=='SM_NW_HallWalls'
MAN['sign']=dict(label='NW_Hall_Exit',pos=[1776.6,-526,F+267],yaw=90)
(B/'art/entrance_revision.json').write_text(json.dumps(MAN,indent=2))
old=json.loads((B/'art/manifest.json').read_text())
old['assets']=[MAN['assets'][0]if r['name']=='SM_NW_HallWalls'else r for r in old['assets']]
for r in old['placements']:
 if r['label']=='NW_Hall_Exit':r.update(pos=MAN['sign']['pos'],yaw=MAN['sign']['yaw'])
(B/'art/manifest.json').write_text(json.dumps(old,indent=2))
print('NOODLE_ENTRANCE_ART_PASS',flush=True)
