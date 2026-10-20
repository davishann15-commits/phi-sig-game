"""Test-apply the exported checkpoint and verify every text/binary hash."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/CalderWork/codex_checkpoint'
manifest = json.loads((OUT / 'manifest.json').read_text())
snapshot = ROOT / manifest['snapshot']
patch = OUT / 'codex_takeover.patch'
archive = OUT / 'codex_derived_assets.tar.gz'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(patch) == manifest['patch_sha256'], 'Patch hash differs'
assert sha(archive) == manifest['archive_sha256'], 'Asset archive hash differs'

with tempfile.TemporaryDirectory(prefix='calder-checkpoint-') as temporary:
    destination = Path(temporary)
    for item in manifest['text_files']:
        old = snapshot / item['file']
        if old.exists():
            target = destination / item['file']
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(old, target)
    for arguments in [['--check'], []]:
        subprocess.run(['git', 'apply', *arguments, str(patch)], cwd=destination, check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    for item in manifest['text_files']:
        actual = destination / item['file']
        if item['sha256'] is None:
            assert not actual.exists(), item['file']
        else:
            assert sha(actual) == item['sha256'], 'Applied file differs: ' + item['file']
            assert sha(ROOT / item['file']) == item['sha256'], 'Current file changed after export: ' + item['file']

expected = {item['file']: item for item in manifest['derived_binary_files']}
with tarfile.open(archive, 'r:gz') as bundle:
    members = bundle.getmembers()
    assert {member.name for member in members} == set(expected), 'Archive membership differs'
    assert len(members) == len(expected), 'Duplicate archive member'
    for member in members:
        item = expected[member.name]
        stream = bundle.extractfile(member)
        assert stream is not None, member.name
        payload = stream.read()
        assert len(payload) == item['bytes'] and hashlib.sha256(payload).hexdigest() == item['sha256'], member.name
        assert sha(ROOT / member.name) == item['sha256'], 'Current binary changed after export: ' + member.name

report = {'passed': True, 'text_files_test_applied': len(manifest['text_files']),
          'binary_files_verified': len(expected), 'patch_sha256': manifest['patch_sha256'],
          'archive_sha256': manifest['archive_sha256'], 'snapshot': manifest['snapshot'],
          'scope': 'Recovery verification only; does not imply full master-prompt acceptance'}
p = OUT / 'verification.json'
p.unlink(missing_ok=True)
p.write_text(json.dumps(report, indent=2))
print('CALDER_CHECKPOINT_RECOVERY PASS text=%d binaries=%d actualGitApply=1 exactHashes=1' %
      (report['text_files_test_applied'], report['binary_files_verified']))
