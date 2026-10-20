"""After scene controls, surface peer/performance failures before long final walks."""
from pathlib import Path
import json,subprocess,sys
R=Path(__file__).resolve().parents[2]
tag=sys.argv[1]
scene=json.loads((R/'Saved/FraternityRowWork'/(tag+'_scenes.json')).read_text())
assert scene['passed'] is True
# Inspect current panes before the long performance cycle; the grouping change affects translucent bounds.
subprocess.run(['python3','SourceArt/FraternityRow/run_row_checks.py','captures',tag],cwd=R,check=True)
subprocess.run(['python3','SourceArt/FraternityRow/run_row_mp.py',tag],cwd=R,check=True)
subprocess.run(['python3','SourceArt/FraternityRow/run_row_perf.py',tag,'--stop-after-full-failure'],cwd=R,check=True)
for phase in ['smoke','interiors','yards','lot_controls','regressions']:
 cmd=['python3','SourceArt/FraternityRow/run_row_checks.py',phase,tag]
 if phase=='yards':cmd+=['--tree-baseline',scene['tree_baseline']]
 subprocess.run(cmd,cwd=R,check=True)
print('ALL CANDIDATE NATIVE PHASES COMPLETE',flush=True)
