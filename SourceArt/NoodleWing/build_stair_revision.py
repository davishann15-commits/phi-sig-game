from pathlib import Path
B=Path(__file__).resolve().parent
s=(B/'build_art.py').read_text()
exec(s.split('# Tiling micro-surface masks.')[0],globals())
F=-361.966;C=-54.;T=18
exec(s[s.index('def wall('):s.index("g='RedWalls'")],globals())
exec((B/'stair_layout.py').read_text(),globals())
(B/'art/stair_revision.json').write_text(json.dumps(MAN,indent=2))
old=json.loads((B/'art/manifest.json').read_text())
names={r['name'] for r in MAN['assets']}
old['assets']=[r for r in old['assets'] if r['name'] not in names]+MAN['assets']
old['lights']=[r for r in old['lights'] if not r['label'].startswith('NW_StairLamp_')]+MAN['lights']
(B/'art/manifest.json').write_text(json.dumps(old,indent=2))
print('NOODLE_STAIR_ART_PASS',flush=True)
