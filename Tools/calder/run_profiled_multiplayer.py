"""Profile only the client of the unchanged rendered paired walking review.

Adds CPU/frame/bookmark tracing and named events to the client launch only.
All movement, render flags, assertions, waypoints and timeouts remain identical.
The trace is diagnostic overhead, not a replacement for final default acceptance.
"""
import difflib
import hashlib
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[2]
source=ROOT/'Saved/CalderWork/tools/campus_mp.py'
if len(sys.argv)<2:
    raise SystemExit('Usage: run_profiled_multiplayer.py <unique-run-name> [--walk] [extra flags]')
original=source.read_text()
assert hashlib.sha256(original.encode()).hexdigest()=='404b4e7727205d7f01a7b8dd5f1f9205ef12a4ee7db6618eb32626e5642f3e79'
old="f'-UserDir={ud}', f'-AbsLog={log}'],"
new="f'-UserDir={ud}', f'-AbsLog={log}'] + (['-trace=cpu,frame,bookmark', '-statnamedevents', f'-tracefile={out}/client.utrace'] if role == 'client' else []),"
assert original.count(old)==1
adapted=original.replace(old,new)
assert adapted.replace(new,old)==original
receipt={'scope':'Diagnostic client tracing only; original rendered review unchanged',
 'original_sha256':hashlib.sha256(original.encode()).hexdigest(),
 'adapted_sha256':hashlib.sha256(adapted.encode()).hexdigest(),
 'launch_diff':''.join(difflib.unified_diff(original.splitlines(True),adapted.splitlines(True),fromfile='original.py',tofile='client_trace.py'))}
name=sys.argv[1]
sys.argv=[str(source),*sys.argv[1:]]
try:
    exec(compile(adapted,str(source),'exec'),{'__name__':'__main__','__file__':str(source)})
finally:
    out=ROOT/'Saved/CalderWork/runs'/name
    out.mkdir(parents=True,exist_ok=True)
    (out/'trace_adapter.json').write_text(json.dumps(receipt,indent=2)+'\n')
