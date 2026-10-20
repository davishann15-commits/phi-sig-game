"""Inspect fern/stump parents, graphs and instancing support without saving assets.

Run in a separate ordinary offscreen editor only after timing jobs finish.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
E = unreal.MaterialEditingLibrary
sources = [
    '/Game/Environment/Megascans/BeechFern_vmkpdbeia',
    '/Game/Environment/Megascans/RottenTreeStump_whnpaed',
]
files = [p for folder in sources for p in (ROOT/'Content'/folder.removeprefix('/Game/')).rglob('*.uasset')]
before = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
assert before
rows = []
meshes = []
seen = set()
for folder in sources:
    for path in unreal.EditorAssetLibrary.list_assets(folder, recursive=True):
        value = unreal.load_asset(path)
        if isinstance(value, unreal.StaticMesh):
            meshes.append({'asset': value.get_path_name(),
                           'triangles': [value.get_num_triangles(i) for i in range(value.get_num_lods())],
                           'nanite': value.get_editor_property('nanite_settings').enabled,
                           'materials': [slot.material_interface.get_path_name() if slot.material_interface else None
                                         for slot in value.get_editor_property('static_materials')]})
        if not isinstance(value, unreal.MaterialInterface):
            continue
        stats = E.get_statistics(value)
        row = {'asset': value.get_path_name(), 'class': value.get_class().get_name(),
               'pixel_instructions': stats.num_pixel_shader_instructions,
               'pixel_samples': stats.num_pixel_texture_samples}
        parent = value
        row['parent_chain'] = []
        while isinstance(parent, unreal.MaterialInstanceConstant):
            parent = parent.get_editor_property('parent')
            assert parent, value.get_path_name()
            row['parent_chain'].append(parent.get_path_name())
        assert isinstance(parent, unreal.Material), parent
        row['base'] = parent.get_path_name()
        row['instanced_mesh_usage'] = parent.get_editor_property('used_with_instanced_static_meshes')
        row['blend_mode'] = str(parent.get_editor_property('blend_mode'))
        if parent.get_path_name() not in seen:
            seen.add(parent.get_path_name())
            row['expressions'] = []
            for expression in E.get_material_expressions(parent):
                item = {'class': expression.get_class().get_name(), 'name': expression.get_name()}
                for key in ['parameter_name', 'texture', 'material_function']:
                    try:
                        prop = expression.get_editor_property(key)
                        item[key] = prop.get_path_name() if isinstance(prop, unreal.Object) else str(prop)
                    except Exception:
                        pass
                row['expressions'].append(item)
        rows.append(row)
        unreal.log('CALDER_WOODS_PROP_MATERIAL '+json.dumps(row))
for name, expected in before.items():
    assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest() == expected, name
output = ROOT/'Saved/CalderWork/woods_prop_material_inspection.json'
output.write_text(json.dumps({'materials': rows, 'meshes': meshes, 'source_sha256': before,
                             'source_assets_unchanged': True}, indent=2)+'\n')
unreal.log('CALDER_WOODS_PROP_INSPECTION PASS materials='+str(len(rows)))
