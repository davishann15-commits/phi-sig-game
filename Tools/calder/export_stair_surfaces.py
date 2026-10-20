"""Export the runtime source-cadastre survey for the planning emulator.

This records paving classification only. It is not terrain/physical acceptance;
independent collision and walking reviews remain required after changes.
"""
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
source = Path(sys.argv[1])
if not source.is_absolute():
    source = ROOT / source
pattern = re.compile(r'CALDER_STAIR_SURFACES name="([^"]+)" foot=\(([-\d.]+),([-\d.]+)\) footKind=(\d+) head=\(([-\d.]+),([-\d.]+)\) headKind=(\d+)')
rows = {}
for match in pattern.finditer(source.read_text(errors='replace')):
    name, fx, fy, fk, hx, hy, hk = match.groups()
    row = {'name': name, 'foot': [float(fx), float(fy)], 'foot_kind': int(fk),
           'head': [float(hx), float(hy)], 'head_kind': int(hk)}
    assert row['foot_kind'] in range(4) and row['head_kind'] in range(4), row
    key = (name, fx, fy, hx, hy)
    assert key not in rows or rows[key] == row, 'Conflicting survey row'
    rows[key] = row
assert len(rows) == 39, 'Expected all 39 current stair records; got ' + str(len(rows))
paths = ['Source/SeniorSendoff/HouseCampusLayout.cpp', 'Source/SeniorSendoff/HouseCalderTerrain.cpp',
         'Source/SeniorSendoff/HouseCalderRoads.cpp', 'Source/SeniorSendoff/HouseCalderRoads.h']
report = {'source_log': str(source.relative_to(ROOT)), 'scope': 'Paving kinds at fixed XY endpoints; not a physical acceptance test',
          'pave_offsets_cm': [0.0, -12.0, 3.0, 5.0],
          'cadastre_sources_sha256': {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in paths},
          'stairs': list(rows.values())}
output = ROOT/'SourceArt/CalderLandscape/phase1/stair_surfaces.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.unlink(missing_ok=True)
output.write_text(json.dumps(report, indent=2))
print('CALDER_STAIR_SURFACES_EXPORTED records=%d path=%s' % (len(rows), output))
