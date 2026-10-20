"""Run the unchanged rendered paired review through UnrealEditor-Cmd.

Only the executable differs. Keep the project argument, renderer, lobby,
movement, assertions and timeouts exactly as in the original harness.
"""
import difflib
import hashlib
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[2]
source=ROOT/'Saved/CalderWork/tools/campus_mp.py'
if len(sys.argv)<2:
    raise SystemExit('Usage: run_editor_cmd_multiplayer.py <unique-run-name> [--walk] [extra flags]')
original=source.read_text()
old="EXE = '/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'"
new="EXE = '/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd'"
assert original.count(old)==1
adapted=original.replace(old,new)
assert adapted.replace(new,old)==original
receipt={'scope':'Executable-only rendered diagnostic; all original review criteria unchanged',
 'original_sha256':hashlib.sha256(original.encode()).hexdigest(),
 'adapted_sha256':hashlib.sha256(adapted.encode()).hexdigest(),
 'launch_diff':''.join(difflib.unified_diff(original.splitlines(True),adapted.splitlines(True),fromfile='original.py',tofile='editor_cmd.py'))}
name=sys.argv[1]
sys.argv=[str(source),*sys.argv[1:]]
try:
    exec(compile(adapted,str(source),'exec'),{'__name__':'__main__','__file__':str(source)})
finally:
    out=ROOT/'Saved/CalderWork/runs'/name
    out.mkdir(parents=True,exist_ok=True)
    (out/'launch_adapter.json').write_text(json.dumps(receipt,indent=2)+'\n')
