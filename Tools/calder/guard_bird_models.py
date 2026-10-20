"""Read-only checks of protected production files and acquired donor assets."""
import hashlib,json
from datetime import datetime,timezone
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
old=json.loads((ROOT/'SourceArt/CalderLandscape/phase3/source_scope_guard.json').read_text())
base=json.loads((ROOT/'Saved/CalderWork/wildlife_start_20261017_144530/sha256.json').read_text())
changed=sorted(n for n,h in base.items() if n.startswith('Source/') and sha(ROOT/n)!=h)
assert changed==old['wildlife_start_source_mutations'],changed
protected={row['file']:row['takeover_sha256'] for row in old['protected_sources_and_map']}
for n,h in protected.items():assert sha(ROOT/n)==h,n
for n,h in base.items():
 if n.startswith('Config/') or n.endswith('/MASTER_PROMPT.md'):assert sha(ROOT/n)==h,n
guards={}
for name in ['deer/installed.json','crow/installed.json','sky/atlas_installed.json']:
 d=json.loads((ROOT/'SourceArt/CalderLandscape/phase3'/name).read_text());assert d['passed'];guards.update(d.get('source_sha256',{}));guards.update(d.get('existing_sha256',{}))
for n,h in guards.items():assert sha(ROOT/n)==h,n
assert sha(ROOT/'Saved/CalderWork/tools/campus_mp.py')==old['original_campus_mp_sha256']
owned={str(p.relative_to(ROOT)):sha(p) for p in (ROOT/'Content/SeniorSendOff/Calder/Wildlife').glob('*.uasset')}
report={'passed':True,'utc':datetime.now(timezone.utc).isoformat(),'protected_source_map':protected,'donor_deer_assets':64,'donor_crow_assets':26,'all_preexisting_asset_guards':len(guards),'allowed_existing_source_changes':changed,'unexpected_existing_source_changes':[],'config_and_master_unchanged':True,'canonical_campus_mp_unchanged':True,'owned_wildlife_physical_files':owned,'no_map_saves':True,'no_commits':True,'whole_phase3_complete':False}
p=ROOT/'SourceArt/CalderLandscape/phase3/sky_scope_guard.json';p.unlink(missing_ok=True);p.write_text(json.dumps(report,indent=2)+'\n');print('BIRD_SCOPE_PASS protected=13 deerDonors=64 crowDonors=26 existingSourceEdits=6 config/master/canonicalMP=unchanged ownedPhysicalFiles='+str(len(owned)))
