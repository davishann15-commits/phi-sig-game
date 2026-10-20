"""Read-only validators shared by final recovery and status reporting.

The root reviewer must MANUALLY author WORK/<tag>_visuals.json after inspection:
  schema: FraternityRowManualVisualReviewV1
  tag, source_sha256: the exact acceptance tag and complete Source/Config hash map
  authored_manually: true
  reviewers: a nonempty list of reviewer names
  reviewed_utc: an ISO8601 timestamp with an explicit UTC offset
  zero_open_blocking_findings: true
  open_blocking_findings: []
  review_documents: {project-relative path: SHA256} for both REVIEW_DOCUMENTS
  images: {group: {exact PNG filename: SHA256}} for all eight image_groups(tag)

This module never authors an attestation or infers visual acceptance from counts.
"""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'Saved/FraternityRowWork'
OUT = ROOT / 'SourceArt/FraternityRow'
RUNS = ROOT / 'Saved/CalderWork/runs'
PHASES = ('scenes', 'smoke', 'interiors', 'yards', 'captures', 'lot_controls', 'regressions', 'mp', 'perf')
REVIEW_DOCUMENTS = ('SourceArt/FraternityRow/INTERIOR_VISUAL_REVIEW.md',
                    'SourceArt/FraternityRow/EXTERIOR_VISUAL_REVIEW.md')
PATCH_FILES = ('ROW_IMPLEMENTATION.patch', 'ROW_CAMERAS.patch', 'IMPLEMENTATION_FILES.json')
REPORT_FILES = ('SourceArt/FraternityRow/PERFORMANCE.json', 'SourceArt/FraternityRow/PERFORMANCE.md')
ROW_VIEWS = ('road_wide', 'R1_porch', 'R2_porch', 'R3_porch', 'R1_yard', 'R2_yard',
             'R3_yard', 'infill', 'maple_gap', 'oak_edge', 'rear_fence', 'gate',
             'former_parking', 'house_connection')
OLD_VIEW_FILES = {f'{252 + i}_detail_row_{name}.png' for i, name in enumerate(ROW_VIEWS)}
LOT_VIEW_FILES = {'277_detail_row_R5_lot_front.png', '278_detail_row_R5_lot_rear.png'}
INTERIOR_FILES = {f'R{i}_{shot}.png' for i in range(1, 6) for shot in
                  ('01_Porch', '02_Hall', '03_Lounge', '04_UpperLock', '04b_Stairs',
                   '05_Chapter', '06_Dining', '07_Kitchen', '08_WC', '09_RearSteps', '10_Fence')}
INTERIOR_FILES |= {'R5_03b_Alcove.png', 'R5_06b_ConnectedParlors.png'}
YARD_FILES = {f'R{i}_{n:02d}.png' for i, numbers in
              {1: (8, 11, 17), 2: (7, 10, 15), 3: (8, 11, 17),
               4: (8, 11, 17), 5: (9, 12, 19)}.items() for n in numbers}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def valid_tag(tag):
    require(isinstance(tag, str) and re.fullmatch(r'[A-Za-z0-9_-]+', tag), 'Unsafe acceptance tag')
    return tag


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_json(path):
    value = json.loads(Path(path).read_text())
    require(isinstance(value, dict), 'Expected a JSON object: ' + str(path))
    return value


def runtime_hashes():
    return {str(p.relative_to(ROOT)): sha(p) for folder in ('Source', 'Config')
            for p in sorted((ROOT / folder).rglob('*')) if p.is_file()}


def module_hashes():
    values = {str(p.relative_to(ROOT)): sha(p)
              for p in sorted((ROOT / 'Binaries/Mac').glob('*SeniorSendoff*.dylib'))}
    require(values, 'Built game module missing')
    return values


def project_guards():
    guards = read_json(WORK / 'camera_extra_guards.json')
    require(guards and all(sha(ROOT / rel) == expected for rel, expected in guards.items()),
            'Project/map guard failed')
    return guards


def image_groups(tag):
    valid_tag(tag)
    exterior = 'Saved/HouseExteriorViews'
    return {
        'before_dusk': (RUNS / 'row_current_20261018_dusk' / exterior, OLD_VIEW_FILES),
        'before_dark': (RUNS / 'row_current_20261018_dark' / exterior, OLD_VIEW_FILES),
        'interiors': (RUNS / (tag + '_row_interiors') / 'Saved/CampusInteriorsReview', INTERIOR_FILES),
        'yards': (RUNS / (tag + '_yard_walk') / 'Saved/FraternityRowYardsWalkReview', YARD_FILES),
        'after_dusk': (RUNS / (tag + '_dusk') / exterior, OLD_VIEW_FILES | LOT_VIEW_FILES),
        'after_dark': (RUNS / (tag + '_dark') / exterior, OLD_VIEW_FILES | LOT_VIEW_FILES),
        'lot_legacy_dusk': (RUNS / (tag + '_lot_legacy_dusk') / exterior, LOT_VIEW_FILES),
        'lot_legacy_dark': (RUNS / (tag + '_lot_legacy_dark') / exterior, LOT_VIEW_FILES),
    }


def utc(value):
    require(isinstance(value, str), 'Visual review timestamp missing')
    parsed = datetime.fromisoformat(value.replace('Z', '+00:00'))
    require(parsed.tzinfo is not None and parsed.utcoffset().total_seconds() == 0,
            'Visual review timestamp must explicitly use UTC')
    return parsed


def validate_visuals(tag, current):
    path = WORK / (valid_tag(tag) + '_visuals.json')
    value = read_json(path)
    require(value.get('schema') == 'FraternityRowManualVisualReviewV1', 'Wrong visual receipt schema')
    require(value.get('tag') == tag and value.get('source_sha256') == current,
            'Visual review is not for this tag/current source')
    require(value.get('authored_manually') is True, 'A manually authored visual attestation is required')
    reviewers = value.get('reviewers')
    require(isinstance(reviewers, list) and reviewers and
            all(isinstance(name, str) and name.strip() for name in reviewers), 'Visual reviewers missing')
    require(value.get('zero_open_blocking_findings') is True and value.get('open_blocking_findings') == [],
            'Visual blocking findings remain open/unreported')
    reviewed = utc(value['reviewed_utc'])
    require(reviewed <= datetime.now(timezone.utc), 'Visual review is dated in the future')
    documents = value.get('review_documents', {})
    require(set(documents) == set(REVIEW_DOCUMENTS), 'Exact interior/exterior review documents required')
    for rel, expected in documents.items():
        require(sha(ROOT / rel) == expected, 'Visual review document changed: ' + rel)
    groups = image_groups(tag)
    require(set(value.get('images', {})) == set(groups), 'Wrong visual image groups')
    count = 0
    for group, (folder, expected_names) in groups.items():
        actual_names = {p.name for p in folder.iterdir() if p.is_file() and
                        p.suffix.lower() in {'.png', '.jpg', '.jpeg', '.webp'}}
        require(actual_names == expected_names, 'Image filenames differ: ' + group)
        hashes = value['images'][group]
        require(set(hashes) == expected_names, 'Visual receipt image membership differs: ' + group)
        for name in sorted(expected_names):
            image = folder / name
            require(image.read_bytes()[:8] == b'\x89PNG\r\n\x1a\n', 'Not a native PNG: ' + str(image))
            require(sha(image) == hashes[name], 'Reviewed image changed: ' + str(image))
            require(reviewed.timestamp() >= image.stat().st_mtime,
                    'Visual review predates image: ' + str(image))
            count += 1
    require(count == 136, 'Visual evidence must contain exactly 136 images')
    return {'path': str(path), 'sha256': sha(path)}


def evidence_for(tag, current, include_visuals=True):
    evidence = {}
    suffixes = {
        'scenes': ('legacy', 'scene', 'no_row_lights'), 'smoke': ('row_smoke',),
        'interiors': ('row_interiors',), 'yards': ('yard_walk',), 'captures': ('dusk', 'dark'),
        'lot_controls': ('lot_legacy_dusk', 'lot_legacy_dark'),
        'regressions': ('terrain', 'roam', 'parking_walk', 'woods_walk', 'field_walk', 'trees',
                        'all_smoke', 'intA', 'intBC', 'traversal', 'exteriorwalk', 'house', 'doors'),
        'mp': ('r1', 'r2', 'r3', 'r4', 'r5', 'lib'),
    }
    for phase in PHASES:
        path = WORK / (valid_tag(tag) + '_' + phase + '.json')
        receipt = read_json(path)
        require(receipt.get('source_sha256') == current, 'Evidence source differs: ' + phase)
        require(receipt.get('finished_utc'), 'Acceptance has not finished: ' + phase)
        if phase == 'perf':
            require(receipt.get('native_complete') is True and receipt.get('budget_passed') is True,
                    'Performance acceptance pending')
            controls = receipt.get('controls', [])
            require(len(controls) == 2, 'Both performance controls are required')
            for control, mode, switch in zip(controls, ('full', 'lights'),
                                             ('-NoFraternityRowUpgrade', '-NoFraternityRowLights')):
                require(control.get('prefix') == tag + '_' + mode and control.get('switch') == switch,
                        'Wrong performance control')
                runs = control.get('runs', [])
                expected = {f'{tag}_{mode}_{onoff}{n}' for onoff in ('on', 'off') for n in range(1, 4)}
                require(len(runs) == 6 and {run['tag'] for run in runs} == expected,
                        'Performance control lacks six unique runs')
                for run in runs:
                    csv = RUNS / run['tag'] / 'Saved/HouseExteriorViews/frametimes.csv'
                    require(run.get('views') == 11 and not run.get('native_failures') and
                            sha(csv) == run['csv_sha256'], 'Performance evidence changed/incomplete')
        else:
            require(receipt.get('passed') is True, 'Acceptance pending: ' + phase)
            runs = receipt.get('runs', [])
            expected = {tag + '_' + suffix for suffix in suffixes[phase]}
            require(len(runs) == len(expected) and {run['tag'] for run in runs} == expected and
                    all(run.get('passed') is True for run in runs), 'Incomplete phase runs: ' + phase)
        evidence[phase] = {'path': str(path), 'sha256': sha(path)}
    if include_visuals:
        evidence['visuals'] = validate_visuals(tag, current)
    return evidence


def validate_camera_archives():
    rel = 'Source/SeniorSendoff/HouseExterior.cpp'
    baseline = read_json(WORK / 'baseline_source_receipt.json')
    instrument = read_json(WORK / 'camera_instrumentation.json')
    overlay = read_json(WORK / 'camera_overlay_receipt.json')
    integration = read_json(WORK / 'integration_receipt.json')['files'][rel]
    require(sha(WORK / 'baseline_source' / rel) == baseline['sha256'][rel],
            'Archived pre-camera source changed')
    require(sha(WORK / 'camera_only_HouseExterior.cpp') == instrument['sha256'] == overlay['prior_sha256'],
            'Archived Row camera-only source changed')
    require(sha(WORK / 'pre_integration' / rel) == integration['before_sha256'] == overlay['after_sha256'],
            'Archived shared camera overlay base changed')


def artifact_sources(tag):
    files = {name: OUT / name for name in PATCH_FILES}
    files.update({rel: ROOT / rel for rel in (*REVIEW_DOCUMENTS, *REPORT_FILES)})
    files.update({'evidence/' + tag + '_' + phase + '.json': WORK / (tag + '_' + phase + '.json')
                  for phase in (*PHASES, 'visuals')})
    return files


def validate_reports(tag):
    report = read_json(ROOT / REPORT_FILES[0])
    require(report.get('passes_before_budget') is True and report.get('passes_upgrade_ab_budget') is True,
            'Current performance report is not accepted')
    expected = {str(RUNS / f'{prefix}_{mode}{n}' / 'Saved/HouseExteriorViews/frametimes.csv')
                for prefix in ('row_before_20261018_r1', tag + '_full', tag + '_lights')
                for mode in ('on', 'off') for n in range(1, 4)}
    require(len(report.get('csv_files', [])) == 18 and set(report['csv_files']) == expected,
            'Performance report belongs to another acceptance tag')


def validate_copies(folder, manifest, current):
    copied = manifest['copied_sha256']
    owned = read_json(WORK / 'integration_receipt.json')['files']
    expected = set(owned) | set(artifact_sources(manifest['tag']))
    actual = {str(p.relative_to(folder)) for p in folder.rglob('*') if p.is_file() and p != folder / 'manifest.json'}
    require(set(copied) == expected == actual, 'Recovery file membership differs')
    for rel, expected_sha in copied.items():
        require(sha(folder / rel) == expected_sha, 'Recovery copy changed: ' + rel)
        source = ROOT / rel if rel in owned else artifact_sources(manifest['tag'])[rel]
        require(sha(source) == expected_sha, 'Recovery copy/source drift: ' + rel)
        if rel in owned:
            require(expected_sha == current[rel], 'Recovery runtime bytes differ from accepted source: ' + rel)


def validate_manifest(tag, current, manifest=None, require_global=True):
    valid_tag(tag)
    value = read_json(WORK / 'final_manifest.json') if manifest is None else manifest
    require(value.get('schema') == 'FraternityRowCheckpointV1' and value.get('tag') == tag,
            'Recovery manifest belongs to another tag/schema')
    require(value.get('source_sha256') == current, 'Recovery manifest has superseded source')
    require(value.get('owned_files') == sorted(read_json(WORK / 'integration_receipt.json')['files']),
            'Recovery ownership list differs')
    require(value.get('acceptance_evidence') == evidence_for(tag, current), 'Acceptance receipts changed')
    require(value.get('patch_reconstruction_passed') is True and
            value.get('unowned_baseline_bytes_preserved') is True, 'Recovery validation flags missing')
    require(value.get('map_saved') is False and value.get('commit_created') is False,
            'Unexpected map-save/commit assertion')
    require(value.get('project_map_sha256') == project_guards(), 'Project/map guard differs')
    require(value.get('built_modules_sha256') == module_hashes(), 'Built modules changed')
    require(value.get('implementation_patch_sha256') == sha(OUT / 'ROW_IMPLEMENTATION.patch') and
            value.get('camera_patch_sha256') == sha(OUT / 'ROW_CAMERAS.patch'), 'Recovery patch hashes differ')
    validate_camera_archives()
    validate_reports(tag)
    folder = WORK / (tag + '_checkpoint')
    require(value.get('checkpoint') == str(folder), 'Recovery path differs')
    validate_copies(folder, value, current)
    require(read_json(folder / 'manifest.json') == value, 'Checkpoint and final manifest differ')
    if require_global:
        require(read_json(WORK / 'final_manifest.json') == value, 'Published final manifest differs')
    require(runtime_hashes() == current, 'Source/Config changed during final validation')
    require(value['project_map_sha256'] == project_guards(), 'Project/map changed during final validation')
    require(value['built_modules_sha256'] == module_hashes(), 'Modules changed during final validation')
    return value
