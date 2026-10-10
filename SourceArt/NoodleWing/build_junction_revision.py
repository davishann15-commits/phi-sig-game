from pathlib import Path
B=Path(__file__).resolve().parent
s=(B/'build_art.py').read_text()
exec(s.split('# Tiling micro-surface masks.')[0],globals())
F=-361.966;C=-54.;T=18
exec(s[s.index('def wall('):s.index("g='LaundryWalls'")],globals())
exec((B/'junction_layout.py').read_text(),globals())
assert {r['name'] for r in MAN['assets']}=={'SM_NW_'+n for n in ['RedWalls','BlueWalls','HallWalls','BasementTrim']}
(B/'art/junction_revision.json').write_text(json.dumps(MAN,indent=2))
old=json.loads((B/'art/manifest.json').read_text());replacement={r['name']:r for r in MAN['assets']}
old['assets']=[replacement.get(r['name'],r) for r in old['assets']]
for r in old['route']:
 if r['name']=='Red room':r['p']=[2570,-1030,F]
(B/'art/manifest.json').write_text(json.dumps(old,indent=2))
print('NOODLE_JUNCTION_ART_PASS',flush=True)
