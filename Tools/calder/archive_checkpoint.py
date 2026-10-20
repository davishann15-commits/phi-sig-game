"""Copy a verified recovery export and append immutable, versioned evidence.

The plan names a fresh destination, a verified prior checkpoint and project-
relative evidence globs. Existing verified checkpoints are never modified.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('plan', type=Path)
    args = parser.parse_args()
    plan = json.loads(args.plan.read_text())
    destination = ROOT / plan['destination']
    prior = ROOT / plan['prior']
    current = ROOT / 'Saved/CalderWork/codex_checkpoint'
    assert not destination.exists(), 'Never overwrite an existing checkpoint'
    manifest = json.loads((current / 'manifest.json').read_text())
    verified = json.loads((current / 'verification.json').read_text())
    assert verified['passed']
    for key, file in [('patch_sha256', 'codex_takeover.patch'),
                      ('archive_sha256', 'codex_derived_assets.tar.gz')]:
        assert sha(current / file) == manifest[key] == verified[key]
    for item in manifest['text_files'] + manifest['derived_binary_files']:
        if item['sha256'] is not None:
            assert sha(ROOT / item['file']) == item['sha256'], item['file']
    old = json.loads((prior / 'recovery_receipt.json').read_text())
    assert old['passed']
    shutil.copytree(current, destination)
    shutil.copytree(prior / 'evidence', destination / 'evidence')
    # A fresh prefix preserves old receipts even when a working plan or CSV
    # has the same filename as an earlier version.
    prefix = destination / 'evidence/increments' / destination.name
    sources = set()
    for pattern in plan['evidence_globs']:
        sources.update(path for path in ROOT.glob(pattern) if path.is_file())
    copied = []
    for source in sorted(sources):
        target = prefix / source.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        source_sha = sha(source)
        if source.suffix == '.csv':
            target = Path(str(target) + '.gz')
            with source.open('rb') as input_stream, gzip.open(target, 'wb') as output:
                shutil.copyfileobj(input_stream, output)
            digest = hashlib.sha256()
            with gzip.open(target, 'rb') as input_stream:
                for block in iter(lambda: input_stream.read(1024 * 1024), b''):
                    digest.update(block)
            assert digest.hexdigest() == source_sha, source
        else:
            shutil.copy2(source, target)
            assert sha(target) == source_sha, source
        copied.append({'source': str(source.relative_to(ROOT)),
                       'source_sha256': source_sha,
                       'file': str(target.relative_to(destination))})
    for item in old['evidence']:
        assert sha(destination / item['file']) == item['sha256'], item['file']
    evidence = [{'file': str(path.relative_to(destination)), 'sha256': sha(path)}
                for path in sorted((destination / 'evidence').rglob('*')) if path.is_file()]
    for key, file in [('patch_sha256', 'codex_takeover.patch'),
                      ('archive_sha256', 'codex_derived_assets.tar.gz')]:
        assert sha(destination / file) == manifest[key]
    receipt = {'passed': True,
               'text_files_test_applied': verified['text_files_test_applied'],
               'binary_files_verified': verified['binary_files_verified'],
               'evidence_count': len(evidence), 'evidence': evidence,
               'new_evidence_sources': copied,
               'patch_sha256': manifest['patch_sha256'],
               'archive_sha256': manifest['archive_sha256'],
               'prior_checkpoint': plan['prior'],
               'master_complete': False, 'photo_quality_accepted': False,
               'default_promoted': bool(plan.get('default_promoted', False)),
               'scope': plan.get('scope', 'Immutable partial recovery only; full master acceptance remains incomplete.')}
    (destination / 'recovery_receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    for item in evidence:
        assert sha(destination / item['file']) == item['sha256']
    print('CALDER_IMMUTABLE_RECOVERY PASS %s text=%d binary=%d evidence=%d' %
          (plan['destination'], verified['text_files_test_applied'],
           verified['binary_files_verified'], len(evidence)))


if __name__ == '__main__':
    main()
