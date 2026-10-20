"""Read the acquired spruce kit and export one rest-pose donor for screening.

No source asset, shared material, map or gameplay tree is modified. Skeletal and
PVE source assets are never bound at runtime; any future derivative must be a
conventional owned static mesh with independent validation.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
FOLDER = '/Game/Megaplant_Library/Tree_Norway_Spruce'
WORK = ROOT/'Saved/CalderWork/evergreen_screening'
WORK.mkdir(exist_ok=True)
files = list((ROOT/'Content/Megaplant_Library/Tree_Norway_Spruce').rglob('*.uasset'))
before = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
registry = unreal.AssetRegistryHelpers.get_asset_registry()
inventory = [{'asset': str(a.package_name), 'class': str(a.asset_class_path.asset_name),
              'class_package': str(a.asset_class_path.package_name)}
             for a in registry.get_assets_by_path(FOLDER, recursive=True)]
E = unreal.MaterialEditingLibrary
materials = []
for name in ['Bark', 'Decorations', 'Foliage']:
    asset = unreal.load_asset(FOLDER+'/Materials/MI_Norway_Spruce_'+name+'_01')
    assert isinstance(asset, unreal.MaterialInstanceConstant), name
    textures = {}
    for parameter in E.get_texture_parameter_names(asset):
        texture = E.get_material_instance_texture_parameter_value(asset, parameter)
        textures[str(parameter)] = texture.get_path_name() if texture else None
    parent = asset
    while isinstance(parent, unreal.MaterialInstanceConstant):
        parent = parent.get_editor_property('parent')
    row = {'asset': asset.get_path_name(), 'textures': textures, 'parent': parent.get_path_name()}
    if isinstance(parent, unreal.Material):
        row['instanced_mesh_usage'] = parent.get_editor_property('used_with_instanced_static_meshes')
        row['expressions'] = [{'class': x.get_class().get_name(), 'name': x.get_name()}
                              for x in E.get_material_expressions(parent)]
    materials.append(row)
donors = []
for suffix in ['Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_A', 'Instances/Branch_Norway_Spruce_02']:
    asset = unreal.load_asset(FOLDER+'/'+suffix)
    assert asset, suffix
    row = {'asset': asset.get_path_name(), 'class': asset.get_class().get_name()}
    if isinstance(asset, unreal.StaticMesh):
        row['triangles'] = [asset.get_num_triangles(i) for i in range(asset.get_num_lods())]
        row['materials'] = [s.material_interface.get_path_name() if s.material_interface else None for s in asset.static_materials]
    if isinstance(asset, (unreal.SkeletalMesh, unreal.StaticMesh)):
        task = unreal.AssetExportTask()
        task.object = asset
        task.filename = str(WORK/(asset.get_name()+'.fbx'))
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        task.exporter = unreal.SkeletalMeshExporterFBX() if isinstance(asset, unreal.SkeletalMesh) else unreal.StaticMeshExporterFBX()
        assert unreal.Exporter.run_asset_export_task(task), suffix
        row['export'] = task.filename
        row['fbx_bytes'] = Path(task.filename).stat().st_size
    donors.append(row)
for name, expected in before.items():
    assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest() == expected, name
(WORK/'inspection.json').write_text(json.dumps({'inventory': inventory, 'materials': materials, 'donors': donors,
    'source_sha256': before, 'source_assets_unchanged': True, 'runtime_binding': False}, indent=2)+'\n')
unreal.log('CALDER_EVERGREEN_INSPECTION PASS sources='+str(len(before))+' inventory='+str(len(inventory))+' donors='+json.dumps(donors))
