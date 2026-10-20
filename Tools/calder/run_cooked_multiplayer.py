"""Run the existing paired review against a staged native Development executable.

Usage: python3 -I Tools/calder/run_cooked_multiplayer.py <native-executable> <run-name> --walk

Only the executable and omission of the .uproject launch argument differ from
the original harness. Flags, assertions, counts, timeouts and movement stay
byte-for-byte identical. A launch-adapter receipt is saved after the review.
"""
import difflib
import hashlib
import json
from pathlib import Path
import sys

P=Path(__file__).resolve().parents[2]
if len(sys.argv)<3:
    raise SystemExit(__doc__)
exe=Path(sys.argv[1]).resolve(strict=True)
if not exe.is_file():
    raise SystemExit('Native executable must be a file.')
source=P/'Saved/CalderWork/tools/campus_mp.py'
original=source.read_text()
old_exe="EXE = '/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'"
new_exe='EXE = '+repr(str(exe))
old_launch="[EXE, str(P / 'SeniorSendoff.uproject'), map_arg]"
new_launch='[EXE, map_arg]'
assert original.count(old_exe)==1 and original.count(old_launch)==1
adapted=original.replace(old_exe,new_exe).replace(old_launch,new_launch)
assert adapted.replace(new_exe,old_exe).replace(new_launch,old_launch)==original
receipt={'scope':'Only executable and project launch argument differ; original review criteria unchanged',
         'original_harness':str(source),
         'original_sha256':hashlib.sha256(original.encode()).hexdigest(),
         'adapted_sha256':hashlib.sha256(adapted.encode()).hexdigest(),
         'native_executable':str(exe),
         'launch_diff':''.join(difflib.unified_diff(original.splitlines(True),adapted.splitlines(True),
                         fromfile='original_campus_mp.py',tofile='cooked_launch_only.py'))}
run_name=sys.argv[2]
sys.argv=[str(source),*sys.argv[2:]]
try:
    exec(compile(adapted,str(source),'exec'),{'__name__':'__main__','__file__':str(source)})
finally:
    out=P/'Saved/CalderWork/runs'/run_name
    out.mkdir(parents=True,exist_ok=True)
    (out/'launch_adapter.json').write_text(json.dumps(receipt,indent=2))
