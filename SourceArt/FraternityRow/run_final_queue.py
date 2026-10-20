"""Serialize remaining final native phases; never edits runtime Source."""
from pathlib import Path
import json,subprocess,sys
R=Path(__file__).resolve().parents[2]
tag=sys.argv[1]
W=R/'Saved/FraternityRowWork'
scene=json.loads((W/(tag+'_scenes.json')).read_text())
assert scene['passed'] is True
legacy=scene['tree_baseline']
for phase in ['smoke','yards','captures','lot_controls']:
 cmd=['python3','SourceArt/FraternityRow/run_row_checks.py',phase,tag]
 if phase=='yards':cmd+=['--tree-baseline',legacy]
 subprocess.run(cmd,cwd=R,check=True)
subprocess.run(['python3','SourceArt/FraternityRow/run_row_perf.py',tag],cwd=R,check=True)
for phase in ['interiors','regressions']:
 subprocess.run(['python3','SourceArt/FraternityRow/run_row_checks.py',phase,tag],cwd=R,check=True)
subprocess.run(['python3','SourceArt/FraternityRow/run_row_mp.py',tag],cwd=R,check=True)
print('ALL FINAL NATIVE PHASES COMPLETE',flush=True)
