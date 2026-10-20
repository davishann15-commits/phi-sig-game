"""Read-only Fab animation export for offline rigid-pose baking. No saves."""
import hashlib,json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT=ROOT/'Saved/CalderWork/wildlife_revision_20261018_v1/animation_exports_v2'
OUT.mkdir(parents=True,exist_ok=False)
donors=list((ROOT/'Content/AnimalVarietyPack/DeerStagAndDoe').rglob('*.uasset'))
before={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in donors}
rows=[]
for name in ['Walk','Run','IdleGraze','IdleLookAround','IdleBreathe']:
    asset=unreal.load_asset('/Game/AnimalVarietyPack/DeerStagAndDoe/Animations/ANIM_DeerDoe_'+name)
    assert asset,name
    task=unreal.AssetExportTask()
    task.object=asset
    task.filename=str(OUT/(name+'.fbx'))
    task.automated=True
    task.prompt=False
    task.replace_identical=False
    options=unreal.FbxExportOption()
    # The mesh was already exported separately. Animation-only export also
    # avoids UE's CPU-skinned preview/material-baking path under NullRHI.
    options.set_editor_property('export_preview_mesh',False)
    task.options=options
    assert unreal.Exporter.run_asset_export_task(task),name
    target=Path(task.filename);assert target.stat().st_size>1000
    rows.append({'name':name,'file':target.name,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==h for p,h in before.items())
(OUT/'receipt.json').write_text(json.dumps({'passed':True,'exports':rows,'donor_sha256':before,'donors_unchanged':True,'assets_saved':False},indent=2)+'\n')
unreal.log('CALDER_DEER_ANIMATION_EXPORT PASS clips=5 donorAssetsUnchanged=1 assetsSaved=0')
