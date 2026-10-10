"""Reimport only the eleven inspected meshes; never resave/replace the open map."""
import json,re,shutil
from pathlib import Path
import unreal
project=Path(unreal.Paths.project_dir())
root=project/'SourceArt/HouseLetterRemoval'
rows=json.loads((root/'evidence/cleaned.json').read_text())
audit=json.loads((root/'evidence/audit.json').read_text())
old={r['asset']:r for r in audit['meshes']}
backup=root/'backup'
def norm(s):return re.sub(r'[._]\d+$','',str(s))
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
results=[]
for row in rows:
    path=row['asset'].split('.')[0];name=path.rsplit('/',1)[1]
    original=unreal.load_asset(path)
    assert isinstance(original,unreal.StaticMesh),path
    local=project/'Content'/Path(path.removeprefix('/Game/')+'.uasset')
    target=backup/Path(path.removeprefix('/Game/')+'.uasset')
    if not target.exists():target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(local,target)
    materials={norm(s['material'].rsplit('.',1)[-1]):unreal.load_asset(s['material'])
               for s in old[row['asset']]['slots'] if s['material']}
    # The flag's rebuilt fracture surfaces use the same core material.
    for m in ('M_HI_FractureCore','M_HI_SplinterWood','M_HI_Foam','M_HI_RawMetal'):
        materials[m]=unreal.load_asset('/Game/SeniorSendOff/House/Interactivity/'+m)
    old_slots=[s.material_interface.get_path_name() if s.material_interface else ''
               for s in original.static_materials]
    before=original.get_bounds();trace=original.get_editor_property('body_setup').get_editor_property('collision_trace_flag')
    ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False
    ui.import_materials=False;ui.import_textures=False
    d=ui.static_mesh_import_data;d.combine_meshes=True;d.auto_generate_collision=False
    d.reorder_material_to_fbx_order=False
    d.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    d.normal_generation_method=unreal.FBXNormalGenerationMethod.BUILT_IN
    task=unreal.AssetImportTask();task.factory=unreal.FbxFactory();task.filename=row['file']
    task.destination_path=path.rsplit('/',1)[0];task.destination_name=name
    task.automated=True;task.replace_existing=True;task.save=False;task.options=ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=unreal.load_asset(path);assert mesh,path
    for i,s in enumerate(mesh.static_materials):
        existing=s.material_interface
        key=norm(str(s.get_editor_property('imported_material_slot_name')))
        if existing and existing.get_path_name() in old_slots:continue
        assert key in materials and materials[key],(name,key,list(materials))
        mesh.set_material(i,materials[key])
    new_slots=[s.material_interface.get_path_name() if s.material_interface else '' for s in mesh.static_materials]
    # Existing component material overrides rely on the stable slot prefix.
    assert new_slots[:len(old_slots)]==old_slots,(name,'slot order changed',old_slots,new_slots)
    after=mesh.get_bounds()
    if not row.get('regenerated_from_clean_flag'):
        assert max(abs(getattr(before.box_extent,a)-getattr(after.box_extent,a)) for a in ('x','y','z'))<1.0,(name,'unexpected bounds change')
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',trace)
    mesh.set_editor_property('allow_cpu_access',True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh),path
    results.append(dict(asset=path,materials=new_slots,backup=str(target)))
    unreal.log('LETTER_INSTALL_ASSET '+name)
(root/'evidence/installed.json').write_text(json.dumps(results,indent=2))
unreal.log('LETTER_INSTALL_PASS meshes={} map_untouched=true'.format(len(results)))
