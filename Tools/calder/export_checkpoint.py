"""Export a local takeover-relative patch/payload without changing Git history.

The payload contains only owned derived assets; it depends on the same acquired
source packs and original Calder material/texture imports as the project.
"""
import difflib
import hashlib
import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[2]
SNAPSHOT = ROOT / 'Saved/CalderWork/codex_takeover_20261016_111615'
OUT = ROOT / 'Saved/CalderWork/codex_checkpoint'
OUT.mkdir(exist_ok=True)
baseline = json.loads((SNAPSHOT / 'sha256.json').read_text())
text_paths = {p for p in baseline if Path(p).suffix in {'.cpp', '.h', '.inl', '.cs', '.py', '.sh', '.md', '.ini', '.uproject'}}
text_paths.update(str(p.relative_to(ROOT)) for p in (ROOT / 'Tools/calder').glob('*') if p.suffix in {'.py', '.sh'})
for p in (ROOT / 'Source/SeniorSendoff').glob('HouseCalder*'):
    if p.suffix in {'.cpp', '.h', '.inl'}:
        text_paths.add(str(p.relative_to(ROOT)))
for p in (ROOT / 'SourceArt/CalderLandscape').rglob('*'):
    if p.suffix in {'.md', '.py', '.json'}:
        text_paths.add(str(p.relative_to(ROOT)))

patch = []
changed = []
for name in sorted(text_paths):
    current = ROOT / name
    old = SNAPSHOT / name
    before = old.read_text() if old.exists() else ''
    after = current.read_text() if current.exists() else ''
    if before == after:
        continue
    patch.append('diff --git a/' + name + ' b/' + name + '\n')
    if not old.exists():
        patch.append('new file mode 100644\n')
    elif not current.exists():
        patch.append('deleted file mode 100644\n')
    for line in difflib.unified_diff(before.splitlines(keepends=True), after.splitlines(keepends=True),
                                    fromfile='a/' + name if old.exists() else '/dev/null',
                                    tofile='b/' + name if current.exists() else '/dev/null'):
        patch.append(line)
        if not line.endswith('\n'):
            patch.append('\n\\ No newline at end of file\n')
    changed.append({'file': name, 'sha256': hashlib.sha256(current.read_bytes()).hexdigest() if current.exists() else None})
patch_path = OUT / 'codex_takeover.patch'
patch_path.unlink(missing_ok=True)
patch_path.write_text(''.join(patch))

binary = list((ROOT / 'Content/SeniorSendOff/Calder/Pig').glob('*.uasset'))
binary += list((ROOT / 'Content/SeniorSendOff/Calder/Woods').glob('*.uasset'))
binary += list((ROOT / 'Content/SeniorSendOff/Calder/Trees').glob('*.uasset'))
binary += list((ROOT / 'Content/SeniorSendOff/Calder/Wildlife').glob('*.uasset'))
binary += list((ROOT / 'Content/SeniorSendOff/Calder/Build').glob('Calder_*.uasset'))
binary += [ROOT / ('Content/SeniorSendOff/Calder/Materials/' + name + '.uasset')
           for name in ['M_CalderFalls', 'M_CalderStream', 'M_CalderLandscapeGround', 'M_CalderRidgeCanopy', 'M_CalderFallsJetsTrial', 'M_CalderFarmRoof', 'M_CalderStrataRock', 'M_CalderHedgerow', 'M_CalderHedgerowFoliage']]
binary += list((ROOT / 'SourceArt/CalderLandscape/phase2/pig_parts').glob('*.fbx'))
binary += list((ROOT / 'SourceArt/CalderLandscape/phase2/tree_wood').glob('*.fbx'))
binary += list((ROOT / 'SourceArt/CalderLandscape/phase3').rglob('*.fbx'))
# Preserve public-domain masters, production WAVs and attribution for the
# owner's specific recorded-corvid exception; no dependency on Saved previews.
binary += [p for p in (ROOT / 'SourceArt/CalderLandscape/phase3/audio/nps_corvids').glob('*') if p.suffix in {'.wav', '.mp3', '.html', '.txt'}]
binary += list((ROOT / 'SourceArt/CalderLandscape/phase2/woods_sprays').glob('*.png'))
manifest = []
archive_path = OUT / 'codex_derived_assets.tar.gz'
archive_path.unlink(missing_ok=True)
with tarfile.open(archive_path, 'w:gz') as archive:
    for p in sorted(set(binary)):
        if not p.exists():
            raise RuntimeError('Missing derived asset ' + str(p))
        name = str(p.relative_to(ROOT))
        manifest.append({'file': name, 'bytes': p.stat().st_size, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()})
        archive.add(p, arcname=name, recursive=False)
report = {'snapshot': str(SNAPSHOT.relative_to(ROOT)), 'text_files': changed, 'derived_binary_files': manifest,
          'patch_sha256': hashlib.sha256(patch_path.read_bytes()).hexdigest(),
          'archive_sha256': hashlib.sha256(archive_path.read_bytes()).hexdigest(),
          'scope': 'Partial Codex checkpoint; see STATUS.md for remaining work and failures'}
manifest_path = OUT / 'manifest.json'
manifest_path.unlink(missing_ok=True)
manifest_path.write_text(json.dumps(report, indent=2))
print('CALDER_CHECKPOINT textFiles=%d derivedBinaryFiles=%d patchBytes=%d archiveBytes=%d' %
      (len(changed), len(manifest), patch_path.stat().st_size, archive_path.stat().st_size))
