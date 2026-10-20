"""Validate final native/manual evidence and atomically save Row-owned recovery files.

Never commits, saves a map, or authors the required manual visual receipt.
"""
from pathlib import Path
from datetime import datetime, timezone
import json
import os
import shutil
import subprocess
import sys
import tempfile

from checkpoint_validation import (
    ROOT, WORK, OUT, artifact_sources, evidence_for, module_hashes, project_guards,
    read_json, require, runtime_hashes, sha, valid_tag, validate_camera_archives,
    validate_copies, validate_manifest, validate_reports,
)


def publish_manifest(payload):
    """Publish a complete manifest without exposing a partially written JSON file."""
    handle, name = tempfile.mkstemp(prefix='.row_final_manifest_', dir=WORK)
    temporary = Path(name)
    try:
        with os.fdopen(handle, 'w') as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, WORK / 'final_manifest.json')
    finally:
        if temporary.exists():
            temporary.unlink()


def main():
    require(len(sys.argv) == 2, 'Usage: python3 save_checkpoint.py <acceptance-tag>')
    tag = valid_tag(sys.argv[1])
    current = runtime_hashes()
    evidence = evidence_for(tag, current)  # Includes the manually authored visual attestation.
    guards, binaries = project_guards(), module_hashes()
    validate_reports(tag)
    validate_camera_archives()
    baseline = read_json(WORK / 'baseline_source_receipt.json')
    owned = read_json(WORK / 'integration_receipt.json')['files']
    for rel, expected in baseline['sha256'].items():
        if rel not in owned:
            require(sha(ROOT / rel) == expected, 'Protected baseline bytes changed: ' + rel)
    for rel in current.keys() - baseline['sha256'].keys():
        require(rel in owned, 'Unexpected new runtime file: ' + rel)
    for rel, entry in owned.items():
        require(sha(Path(entry['candidate'])) == current[rel], 'Draft/runtime drift: ' + rel)
    after_baseline = datetime.fromisoformat(baseline['utc']).timestamp()
    # This is a write-time heuristic, not an initial whole-Content checksum inventory.
    changed_content = [str(p.relative_to(ROOT)) for p in (ROOT / 'Content').rglob('*')
                       if p.is_file() and p.stat().st_mtime > after_baseline]
    require(not changed_content, 'Content writes after baseline require investigation: ' + repr(changed_content))
    checkpoint = WORK / (tag + '_checkpoint')
    if checkpoint.exists():
        # A complete directory may survive failed final-manifest publication. Revalidate
        # and republish on retry; never replace an existing recovery directory.
        manifest = read_json(checkpoint / 'manifest.json')
        validate_manifest(tag, current, manifest, require_global=False)
        publish_manifest((checkpoint / 'manifest.json').read_text())
        print('Revalidated existing recovery snapshot:', checkpoint)
        return

    subprocess.run(['python3', str(OUT / 'save_owned_patch.py')], cwd=ROOT, check=True)
    patch = OUT / 'ROW_IMPLEMENTATION.patch'
    with tempfile.TemporaryDirectory(prefix='row_patch_check_') as folder:
        scratch = Path(folder)
        for rel, entry in owned.items():
            if entry['before_sha256']:
                path = scratch / rel
                path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(WORK / 'pre_integration' / rel, path)
        subprocess.run(['git', 'apply', '--check', str(patch)], cwd=scratch, check=True)
        subprocess.run(['git', 'apply', str(patch)], cwd=scratch, check=True)
        for rel in owned:
            require(sha(scratch / rel) == current[rel], 'Patch reconstruction mismatch: ' + rel)
    with tempfile.TemporaryDirectory(prefix='row_camera_patch_check_') as folder:
        scratch = Path(folder)
        rel = 'Source/SeniorSendoff/HouseExterior.cpp'
        path = scratch / rel
        path.parent.mkdir(parents=True)
        shutil.copy2(WORK / 'baseline_source' / rel, path)
        subprocess.run(['git', 'apply', '--check', str(OUT / 'ROW_CAMERAS.patch')], cwd=scratch, check=True)
        subprocess.run(['git', 'apply', str(OUT / 'ROW_CAMERAS.patch')], cwd=scratch, check=True)
        require(sha(path) == sha(WORK / 'camera_only_HouseExterior.cpp'), 'Camera reconstruction mismatch')

    staging = Path(tempfile.mkdtemp(prefix='.' + tag + '_checkpoint_staging_', dir=WORK))
    try:
        sources = {rel: ROOT / rel for rel in owned}
        sources.update(artifact_sources(tag))
        copied = {}
        for rel, source in sources.items():
            dest = staging / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, dest)
            copied[rel] = sha(dest)
        manifest = {
            'schema': 'FraternityRowCheckpointV1', 'tag': tag,
            'utc': datetime.now(timezone.utc).isoformat(), 'source_sha256': current,
            'project_map_sha256': guards, 'built_modules_sha256': binaries,
            'owned_files': sorted(owned), 'checkpoint': str(checkpoint), 'copied_sha256': copied,
            'acceptance_evidence': evidence, 'implementation_patch_sha256': sha(patch),
            'camera_patch_sha256': sha(OUT / 'ROW_CAMERAS.patch'),
            'patch_reconstruction_passed': True, 'unowned_baseline_bytes_preserved': True,
            'content_files_modified_since_baseline': changed_content,
            'content_guard_method': 'project-map SHA256 plus post-baseline file-mtime scan',
            'map_saved': False, 'commit_created': False,
        }
        payload = json.dumps(manifest, indent=2) + '\n'
        (staging / 'manifest.json').write_text(payload)
        validate_copies(staging, manifest, current)
        # Recheck every input after copying/reconstruction, before publication.
        require(runtime_hashes() == current, 'Source/Config changed while saving recovery')
        require(project_guards() == guards, 'Project/map changed while saving recovery')
        require(module_hashes() == binaries, 'Built modules changed while saving recovery')
        require(evidence_for(tag, current) == evidence, 'Acceptance/manual review evidence changed')
        validate_camera_archives()
        validate_reports(tag)
        validate_copies(staging, manifest, current)
        require(runtime_hashes() == current, 'Source/Config changed before publication')
        require(project_guards() == guards, 'Project/map changed before publication')
        require(module_hashes() == binaries, 'Built modules changed before publication')
        require(not checkpoint.exists(), 'Never replace a recovery snapshot')
        staging.rename(checkpoint)
        publish_manifest(payload)
        validate_manifest(tag, current)
        print('Recovery snapshot and exact patch reconstruction passed:', checkpoint)
    finally:
        # Remove only this invocation's private staging directory. Published snapshots
        # are retained and can be strictly revalidated on retry.
        if staging.exists():
            shutil.rmtree(staging)


if __name__ == '__main__':
    main()
