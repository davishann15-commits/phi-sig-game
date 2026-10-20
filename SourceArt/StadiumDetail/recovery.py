"""Export and ACTUALLY test-apply the stadium delta against the released Row baseline.

Run only after final acceptance, with the engine closed and the full reservation.
This does not edit the project, save the map, commit or modify older checkpoints.
Copyrighted study PDFs are deliberately outside both payload and evidence.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse, difflib, hashlib, json, shutil, subprocess, tarfile, tempfile
from stage_runner import ROOT, WORK, RUNS, idle, manifest, sha

OWNED = 'SourceArt/StadiumDetail/'
OWNED_CONTENT = 'Content/SeniorSendOff/Calder/Stadium/'
TEXTURES = OWNED_CONTENT + 'Textures/'


def record(path):
    return {'file': str(path.relative_to(ROOT)), 'bytes': path.stat().st_size, 'sha256': sha(path)}


def pack(output, paths):
    assert all(not p.is_symlink() for p in paths), 'Recovery accepts ordinary owned files only.'
    entries = [record(p) for p in sorted(set(paths))]
    with tarfile.open(output, 'x:gz') as bundle:
        for item in entries:
            bundle.add(ROOT / item['file'], arcname=item['file'], recursive=False)
    # Verify actual extracted bytes, not merely the directory listing or manifest.
    with tempfile.TemporaryDirectory(prefix='stadium-payload-') as temporary:
        target = Path(temporary)
        with tarfile.open(output, 'r:gz') as bundle:
            assert [m.name for m in bundle.getmembers()] == [e['file'] for e in entries]
            assert all(m.isfile() for m in bundle.getmembers())
            bundle.extractall(target, filter='data')
        for item in entries:
            restored = target / item['file']
            assert sha(restored) == item['sha256'] and restored.stat().st_size == item['bytes']
            assert sha(ROOT / item['file']) == item['sha256'], 'File changed during recovery export'
    return entries


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('tag')
    parser.add_argument('--acceptance', required=True, type=Path,
                        help='Final inspected acceptance receipt, including all seven stage receipts.')
    args = parser.parse_args()
    assert args.tag and all(c in 'abcdefghijklmnopqrstuvwxyz0123456789_' for c in args.tag)
    idle()
    grant = json.loads((WORK / 'integration_grant.json').read_text())
    assert grant['granted'] and grant['scope'] == 'full' and grant['release_evidence']
    snapshot = json.loads((WORK / 'pre_integration_snapshot.json').read_text())
    acceptance = json.loads(args.acceptance.read_text())
    frozen = manifest()
    assert acceptance['passed'] and acceptance['source_config_map_sha'] == frozen
    assert sorted(acceptance['stages'], key=int) == [str(n) for n in range(1, 8)]
    for stage, paths in acceptance['stages'].items():
        assert set(paths) == {'capture', 'visual_inspection', 'reviews'}
        results = {phase: json.loads((ROOT / path).read_text()) for phase, path in paths.items()}
        assert all(r['stage'] == int(stage) for r in results.values())
        assert set(results['reviews']['native_results']) == {
            'layout', 'terrain', 'roam', 'reviews', 'clock', 'smoke', 'intA', 'intBC', 'noterrain', 'reach_off', 'reach'}
        assert results['visual_inspection']['all_images_inspected']
        assert results['visual_inspection']['capture_receipt_sha'] == sha(ROOT / paths['capture'])
    # Preserve all intermediate frame-time gates as well as final timing evidence.
    # Each must belong to that stage's actual captured/inspected frozen revision.
    assert set(acceptance['stage_performance']) == {str(n) for n in range(1, 8)}
    stage_performance = {}
    for stage, name in acceptance['stage_performance'].items():
        receipt = json.loads((ROOT / name).read_text())
        captured = json.loads((ROOT / acceptance['stages'][stage]['capture']).read_text())
        assert receipt['passed'] and receipt['stage'] == int(stage)
        assert receipt['source_config_map_sha'] == captured['source_config_map_sha']
        assert receipt['samples'] == 300 and receipt['warmup_seconds'] == 4
        assert receipt['alternating_rounds'] == 3 and receipt['metal_offscreen_only']
        expected_views = [row['view'] for row in captured['runs'][0]['frames']]
        assert len(expected_views) == (21 if int(stage) < 4 else 28 if int(stage) == 4 else 31 if int(stage) == 5 else 34 if int(stage) == 6 else 41)
        assert [view['view'] for view in receipt['views']] == expected_views and all(view['passed'] for view in receipt['views'])
        assert all(len(receipt['pairs'][mode]) == 3 for mode in ('on', 'off'))
        assert receipt['load_seconds']['delta'] <= 1.0
        stage_performance[stage] = receipt
    assert acceptance['stage_performance']['7'] == acceptance['performance']
    for gate in ('performance', 'exact_controls', 'disable_unity', 'handoff_reviews', 'office_walk'):
        receipt = json.loads((ROOT / acceptance[gate]).read_text())
        assert receipt['passed'], gate
        assert receipt['source_config_map_sha'] == frozen, gate + ': stale source evidence'
    baseline = snapshot['source_config_project_map_sha']
    hooks = ['Source/SeniorSendoff/' + name for name in snapshot['legacy_hooks']]
    new = [str(p.relative_to(ROOT)) for p in (ROOT / 'Source/SeniorSendoff').glob('HouseStadiumDetail*')
           if p.suffix in ('.h', '.cpp')]
    assert len(new) == 11, new
    assert set(frozen) - set(baseline) == set(new) | {p for p in frozen if p.startswith(OWNED_CONTENT)}
    assert not set(baseline) - set(frozen)
    for name, expected in baseline.items():
        if name not in hooks:
            assert frozen[name] == expected, 'Unrelated runtime file changed: ' + name
    camera_source = 'Source/SeniorSendoff/HouseExterior.cpp'
    old_camera_lines = (WORK / 'pre_integration' / camera_source).read_text().splitlines()
    new_camera_lines = (ROOT / camera_source).read_text().splitlines()
    field_views = json.loads((ROOT / 'SourceArt/StadiumDetail/field_close_views.json').read_text())
    concourse_views = json.loads((ROOT / 'SourceArt/StadiumDetail/concourse_close_views.json').read_text())
    interior_views = json.loads((ROOT / 'SourceArt/StadiumDetail/interior_close_views.json').read_text())
    program_views = json.loads((ROOT / 'SourceArt/StadiumDetail/program_close_view.json').read_text())
    office_views = json.loads((ROOT / 'SourceArt/StadiumDetail/CoachOffice/office_views.json').read_text())
    added_names = ('detail_stadium_portal_east', 'detail_stadium_portal_west', *[view['name'] for view in field_views['views']], *[view['name'] for view in concourse_views['views']], *[view['name'] for view in interior_views['views']], *[view['name'] for view in program_views['views']], *[view['name'] for view in office_views['views']])
    assert len(added_names) == len(set(added_names)) == 22
    for name in added_names:
        assert sum('TEXT("' + name + '")' in line for line in new_camera_lines) == 1
    filtered = [line for line in new_camera_lines if line.strip()
                and not any('TEXT("' + name + '")' in line for name in added_names)
                and 'New real stadium passages: eye170cm above the ninth17.04cm tread.' not in line
                and field_views['added_comment'] not in line
                and concourse_views['added_comment'] not in line
                and interior_views['added_comment'] not in line
                and program_views['added_comment'] not in line
                and office_views['added_comment'] not in line]
    assert filtered == [line for line in old_camera_lines if line.strip()], \
        'HouseExterior.cpp must retain every prior camera and all house geometry exactly.'
    content = {str(p.relative_to(ROOT)): sha(p) for p in sorted((ROOT / 'Content').rglob('*')) if p.is_file()}
    for name, expected in snapshot['content_sha'].items():
        assert content.get(name) == expected, 'Pre-existing Content changed: ' + name
    added_content = set(content) - set(snapshot['content_sha'])
    assert added_content == {TEXTURES + 'T_Stadium_Concrete_Color.uasset',
                             TEXTURES + 'T_Stadium_Concrete_Normal.uasset',
                             OWNED_CONTENT + 'Materials/M_Stadium_FieldDetail.uasset',
                             TEXTURES + 'T_Stadium_RoomPlaster_Color.uasset',
                             TEXTURES + 'T_Stadium_RoomPlaster_Normal.uasset',
                             TEXTURES + 'T_Stadium_FurnitureWood_Color.uasset',
                             TEXTURES + 'T_Stadium_FurnitureWood_Normal.uasset',
                             TEXTURES + 'T_Stadium_FurnitureLeather_Color.uasset',
                             TEXTURES + 'T_Stadium_FurnitureLeather_Normal.uasset',
                             TEXTURES + 'T_Stadium_CoachSnapshot.uasset'}, added_content
    out = WORK / ('recovery_' + args.tag)
    out.mkdir(exist_ok=False)
    text_paths = hooks + new + sorted(snapshot['shared_docs_sha'])
    patch = []
    text_entries = []
    for name in sorted(text_paths):
        old = WORK / 'pre_integration' / name
        current = ROOT / name
        before = old.read_text() if old.exists() else ''
        after = current.read_text()
        if before == after:
            continue
        patch.append('diff --git a/' + name + ' b/' + name + '\n')
        if not old.exists():
            patch.append('new file mode 100644\n')
        for line in difflib.unified_diff(before.splitlines(keepends=True), after.splitlines(keepends=True),
                                        fromfile='a/' + name if old.exists() else '/dev/null', tofile='b/' + name):
            patch.append(line)
            if not line.endswith('\n'):
                patch.append('\n\\ No newline at end of file\n')
        text_entries.append(record(current))
    patch_path = out / 'stadium.patch'
    patch_path.write_text(''.join(patch))
    with tempfile.TemporaryDirectory(prefix='stadium-apply-') as temporary:
        target = Path(temporary)
        for item in text_entries:
            old = WORK / 'pre_integration' / item['file']
            if old.exists():
                restored = target / item['file']
                restored.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(old, restored)
        for flags in (['--check'], []):
            subprocess.run(['git', 'apply', *flags, str(patch_path)], cwd=target, check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        for item in text_entries:
            assert sha(target / item['file']) == item['sha256'], item['file']
    payload_paths = [p for p in (ROOT / OWNED).rglob('*') if p.is_file()
                     and '__pycache__' not in p.parts and 'Patched' not in p.parts]
    payload_paths += [ROOT / name for name in added_content]
    payload = pack(out / 'stadium_payload.tar.gz', payload_paths)
    # Preserve originals of the narrow patch baseline, making recovery independent of Saved elsewhere.
    base_paths = [WORK / 'pre_integration' / name for name in text_paths
                  if (WORK / 'pre_integration' / name).exists()]
    baseline_entries = pack(out / 'released_baseline.tar.gz', base_paths)
    evidence_paths = [args.acceptance.resolve(), WORK / 'pre_integration_snapshot.json',
                      WORK / 'integration_grant.json']
    for name in ('before_receipt.json', 'refined_before_receipt.json', 'before_performance.json',
                 'reservation_release_camera.json', 'refined_camera_only.patch', 'camera_visual_inspection.json',
                 'reference_manifest.json', 'additional_reference_manifest.json', 'eku_reference_inspection.json'):
        evidence_paths.append(WORK / name)
    for name in ('before_receipt.json', 'refined_before_receipt.json'):
        for run in json.loads((WORK / name).read_text())['runs']:
            evidence_paths += [p for p in (RUNS / run['tag']).rglob('*') if p.is_file()]
    for paths in acceptance['stages'].values():
        for phase, name in paths.items():
            receipt_path = ROOT / name
            evidence_paths.append(receipt_path)
            receipt = json.loads(receipt_path.read_text())
            if phase == 'capture':
                for run in receipt['runs']:
                    evidence_paths += [p for p in (RUNS / run['tag']).rglob('*') if p.is_file()]
            if phase == 'reviews':
                for part in receipt['native_results']:
                    evidence_paths += [p for p in (RUNS / (receipt['tag'] + '_' + part)).rglob('*') if p.is_file()]
    for stage, receipt in stage_performance.items():
        evidence_paths.append(ROOT / acceptance['stage_performance'][stage])
        evidence_paths += [ROOT / name for name in receipt['evidence_files']]
        for mode in ('on', 'off'):
            for run in receipt['pairs'][mode]:
                folder = RUNS / run['tag']
                assert folder.is_dir()
                evidence_paths += [p for p in folder.rglob('*') if p.is_file()]
    for gate in ('performance', 'exact_controls', 'disable_unity', 'handoff_reviews', 'office_walk'):
        gate_path = ROOT / acceptance[gate]
        evidence_paths.append(gate_path)
        receipt = json.loads(gate_path.read_text())
        if gate == 'performance':
            tags = [run['tag'] for mode in ('on', 'off') for run in receipt['pairs'][mode]]
        else:
            tags = receipt['run_tags']
        for tag in tags:
            assert (RUNS / tag).is_dir(), (gate, tag)
            evidence_paths += [p for p in (RUNS / tag).rglob('*') if p.is_file()]
        assert receipt['evidence_files'], 'Each final gate needs its actual original logs/receipts: ' + gate
        evidence_paths += [ROOT / name for name in receipt['evidence_files']]
    # Gate receipts enumerate further original logs/captures/build logs explicitly.
    evidence_paths += [ROOT / name for name in acceptance['additional_evidence_files']]
    assert all(p.is_file() and p.suffix.lower() != '.pdf' for p in evidence_paths)
    evidence = pack(out / 'evidence.tar.gz', evidence_paths)
    assert manifest() == frozen
    idle()
    report = {'passed': True, 'finished_utc': datetime.now(timezone.utc).isoformat(),
              'scope': 'Verified recovery of the accepted stadium delta against the released Row baseline',
              'actual_git_apply': True, 'actual_payload_extraction': True, 'map_saved': False,
              'source_config_map_sha': frozen, 'text_files': text_entries, 'payload_files': payload,
              'baseline_files': baseline_entries, 'evidence_files': evidence,
              'archives': {p.name: sha(p) for p in out.iterdir() if p.is_file()}}
    (out / 'recovery_receipt.json').write_text(json.dumps(report, indent=2) + '\n')
    print('STADIUM_RECOVERY PASS actualGitApply=1 exactHashes=1 text=%d payload=%d evidence=%d' %
          (len(text_entries), len(payload), len(evidence)))


if __name__ == '__main__':
    main()
