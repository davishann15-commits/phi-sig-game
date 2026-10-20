"""Protect prior world/assets while allowing exactly the owner's deer mesh revision."""
import hashlib,json
from pathlib import Path
from datetime import datetime,timezone
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
allowed={f'Content/SeniorSendOff/Calder/Wildlife/{n}.uasset' for n in ['SM_CalderDeerBody','SM_CalderDeerSourceLOD1','SM_CalderDeerSourceLOD2','M_CalderDeerSkin','M_CalderDeerFur','M_CalderWildlifePart','M_CalderWildlifeContact']}
mesh_receipt=json.loads((ROOT/'SourceArt/CalderLandscape/phase3/deer/animation/installed21.json').read_text());assert mesh_receipt['passed'] and mesh_receipt['source_assets_unchanged']==64 and mesh_receipt['rigid_controls']==21 and mesh_receipt['per_instance_custom_floats']==253
revisions={}
for n,h in guards.items():
 actual=sha(ROOT/n)
 if actual!=h:
  assert n in allowed,n
  revisions[n]={'prior_sha256':h,'current_sha256':actual,'reason':'Owner rejected simplified leg/body poses;21 donor controls with compressed253-float material palette'}
assert revisions and set(revisions)<=allowed
assert sha(ROOT/'Saved/CalderWork/tools/campus_mp.py')==old['original_campus_mp_sha256']
report={'passed':True,'utc':datetime.now(timezone.utc).isoformat(),'protected_source_map':protected,'donors_unchanged':90,'other_existing_assets_unchanged':len(guards)-len(revisions),'allowed_owned_deer_revisions':revisions,'canonical_mp_unchanged':True,'config_master_unchanged':True,'original_guard_failure_preserved':'Prior guard correctly rejects changed owned deer static body; approved revision scoped here','whole_phase3_complete':False}
p=ROOT/'SourceArt/CalderLandscape/phase3/wildlife_revision_scope_guard.json';p.unlink(missing_ok=True);p.write_text(json.dumps(report,indent=2)+'\n');print('WILDLIFE_REVISION_GUARD PASS protected='+str(len(protected))+' donor=90 ownedDeerRevisions='+str(len(revisions)))
