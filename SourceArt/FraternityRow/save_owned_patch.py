"""Save only Row-owned runtime differences against captured pre-integration bytes."""
from pathlib import Path
from datetime import datetime, timezone
import difflib
import hashlib
import json

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'Saved/FraternityRowWork'
OUT = ROOT / 'SourceArt/FraternityRow'
receipt = json.loads((WORK / 'integration_receipt.json').read_text())
patch = []
files = {}
for rel, entry in sorted(receipt['files'].items()):
    current = (ROOT / rel).read_bytes()
    before_path = WORK / 'pre_integration' / rel
    before = before_path.read_bytes() if entry['before_sha256'] else b''
    assert (hashlib.sha256(before).hexdigest() if entry['before_sha256'] else None) == entry['before_sha256'], rel
    if current == before:
        continue
    patch.append('diff --git a/' + rel + ' b/' + rel + '\n')
    if entry['before_sha256'] is None:
        patch.append('new file mode 100644\n')
    patch.extend(difflib.unified_diff(before.decode().splitlines(keepends=True),
                                    current.decode().splitlines(keepends=True),
                                    fromfile='a/' + rel if entry['before_sha256'] else '/dev/null',
                                    tofile='b/' + rel))
    files[rel] = {'before_sha256': entry['before_sha256'],
                  'final_sha256': hashlib.sha256(current).hexdigest()}
(OUT / 'ROW_IMPLEMENTATION.patch').write_text(''.join(patch))

# Capture cameras were installed before the shared stadium slot; deliver that
# separate owned change without absorbing the other owner's appended cameras.
rel = 'Source/SeniorSendoff/HouseExterior.cpp'
original = (WORK / 'baseline_source' / rel).read_text()
camera_only = (WORK / 'camera_only_HouseExterior.cpp').read_text()
camera_patch = 'diff --git a/' + rel + ' b/' + rel + '\n' + ''.join(
    difflib.unified_diff(original.splitlines(keepends=True), camera_only.splitlines(keepends=True),
                         fromfile='a/' + rel, tofile='b/' + rel))
(OUT / 'ROW_CAMERAS.patch').write_text(camera_patch)
(OUT / 'IMPLEMENTATION_FILES.json').write_text(json.dumps({
    'utc': datetime.now(timezone.utc).isoformat(),
    'base': 'captured pre-integration bytes, after both owners camera-only instrumentation',
    'implementation_patch': 'ROW_IMPLEMENTATION.patch',
    'separate_prior_camera_patch': 'ROW_CAMERAS.patch',
    'files': files,
    'excluded': ['unrelated existing checkout changes', 'stadium cameras', 'Config', 'Content', 'maps'],
}, indent=2) + '\n')
print('Saved Row-owned implementation patch for', len(files), 'runtime files; no commit or map save.')
