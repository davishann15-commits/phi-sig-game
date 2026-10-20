"""Capture the actual released Row baseline before any stadium integration.

The grant is recorded only after the other chat explicitly releases its FULL
reservation. Its exact final source/config/project/map hashes are required here.
This script cannot grant a reservation, overwrite a snapshot or save a map.
"""
from pathlib import Path
from datetime import datetime, timezone
import hashlib, json, shutil, subprocess

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'Saved/StadiumDetailWork'
HOOKS = ['HouseExteriorCampus.cpp', 'HouseCampusFallLook.cpp', 'HouseCampusFall.cpp',
         'HouseCampusFallStadium.cpp', 'HouseCampusInteriorsStadium.cpp', 'HouseCampusPolishStadium.cpp',
         'HouseExterior.cpp']
DOCS = ['SourceArt/CalderLandscape/CODEX_HANDOFF.md', 'SourceArt/CampusFall/README.md']


def main():
    grant = json.loads((WORK / 'integration_grant.json').read_text())
    assert grant['granted'] and grant['scope'] == 'full' and grant.get('release_evidence')
    assert not (WORK / 'pre_integration_snapshot.json').exists()
    assert not (WORK / 'pre_integration').exists()
    processes = subprocess.check_output(['ps', '-axo', 'pid=,comm='], text=True).splitlines()
    assert not [p for p in processes if any(x in p for x in (
        'UnrealEditor.app/Contents/MacOS/UnrealEditor', 'UnrealBuildTool', 'Binaries/Mac/SeniorSendoff'))]
    paths = [p for directory in ('Source', 'Config') for p in (ROOT / directory).rglob('*') if p.is_file()]
    paths += [ROOT / 'SeniorSendoff.uproject', ROOT / 'Content/Story/Maps/Chapter01_House.umap']
    actual = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}
    expected = grant['source_config_project_map_sha']
    assert actual == expected, 'The released baseline differs; inspect the difference and do not integrate over it.'
    duplicates = [str(p) for p in (ROOT / 'Source/SeniorSendoff').rglob('* 2.*')]
    assert not duplicates, duplicates
    for p in (ROOT / 'SourceArt/StadiumDetail/Draft').glob('HouseStadiumDetail*'):
        if p.suffix in ('.cpp', '.h'):
            assert not (ROOT / 'Source/SeniorSendoff' / p.name).exists()
    for name in HOOKS:
        source = ROOT / 'Source/SeniorSendoff' / name
        target = WORK / 'pre_integration/Source/SeniorSendoff' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        assert hashlib.sha256(target.read_bytes()).hexdigest() == actual[str(source.relative_to(ROOT))]
    shared_docs = {}
    for name in DOCS:
        source = ROOT / name
        target = WORK / 'pre_integration' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        shared_docs[name] = hashlib.sha256(source.read_bytes()).hexdigest()
        assert hashlib.sha256(target.read_bytes()).hexdigest() == shared_docs[name]
    content = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in sorted((ROOT / 'Content').rglob('*')) if p.is_file()}
    assert not any(p.startswith('Content/SeniorSendOff/Calder/Stadium/') for p in content), \
        'A pre-existing stadium asset needs a separate recovery baseline; do not overwrite it.'
    result = {'recorded_utc': datetime.now(timezone.utc).isoformat(), 'source_config_project_map_sha': actual,
        'release_evidence': grant['release_evidence'], 'legacy_hooks': HOOKS,
        'shared_docs_sha': shared_docs,
        'content_sha': content,
        'map_saved': False, 'source_changed': False, 'scope': 'Recoverable baseline for the stadium delta after Row acceptance'}
    (WORK / 'pre_integration_snapshot.json').write_text(json.dumps(result, indent=2) + '\n')
    print('Verified released baseline and backed up six geometry hooks plus one camera-only file; no runtime source changed.')


if __name__ == '__main__':
    main()
