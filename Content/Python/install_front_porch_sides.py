import json,shutil
from pathlib import Path
import unreal
project=Path(unreal.Paths.project_dir());root=project/'SourceArt/FrontPorchOpenSides'
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
rows=json.loads((root/'cleaned.json').read_text());results=[]
assert len(rows)==2
for row in rows:
    path=row['asset'].split('.')[0];name=path.rsplit('/',1)[-1]
    original=unreal.load_asset(path);assert original
    slots=[s.material_interface for s in original.static_materials]
    source=project/'Content'/Path(path.removeprefix('/Game/')+'.uasset')
    backup=root/'backup'/source.name;backup.parent.mkdir(exist_ok=True)
    if not backup.exists():shutil.copy2(source,backup)
    trace=original.get_editor_property('body_setup').get_editor_property('collision_trace_flag')
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
    mesh=unreal.load_asset(path);assert len(mesh.static_materials)==len(slots)
    for i,mat in enumerate(slots):mesh.set_material(i,mat)
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',trace)
    mesh.set_editor_property('allow_cpu_access',True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    results.append(dict(asset=path,removed_faces=row['removed'],backup=str(backup)))
(root/'installed.json').write_text(json.dumps(results,indent=2))
unreal.log('PORCH_SIDES_INSTALL_PASS two_side_panels_removed map_untouched=true')
