"""Read-only survey for the Calder State interiors (master job, step 2b): the house's library meshes and its MI_SSO_*
material instances, so the campus can reuse them at runtime.

For every intact SM_Lib_* mesh: its local bounds (min/max, cm), material slots, whether it has simple collision, and
which damage stages exist (_DamageV2, _HeavyV2, _BrokenV2). For every MI_SSO_* instance: its parent and its texture,
scalar and vector parameters. Writes Saved/ClaudeDump/campus_kit_survey.json. Opens no map and saves nothing.
"""
import json, os, unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), 'ClaudeDump', 'campus_kit_survey.json')
A = unreal.EditorAssetLibrary
LIB = '/Game/SeniorSendOff/House/BasementLibrary'


def v3(v):
    return [round(v.x, 2), round(v.y, 2), round(v.z, 2)]


res = {'meshes': {}, 'materials': {}}
for path in A.list_assets(LIB, recursive=False, include_folder=False):
    name = path.split('/')[-1].split('.')[0]
    if not name.startswith('SM_Lib_') or '_Loose_' in name or any(s in name for s in ('DamageV2', 'HeavyV2', 'BrokenV2', 'BrokenV3')):
        continue
    mesh = unreal.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        continue
    box = mesh.get_bounding_box()
    d = {'min': v3(box.min), 'max': v3(box.max), 'tris': mesh.get_num_triangles(0) if hasattr(mesh, 'get_num_triangles') else None}
    try:
        d['materials'] = [(str(m.material_slot_name), m.material_interface.get_name() if m.material_interface else None) for m in mesh.static_materials]
    except Exception as e:
        d['materials'] = str(e)
    try:
        body = mesh.get_editor_property('body_setup')
        agg = body.get_editor_property('agg_geom') if body else None
        d['simple'] = {k: len(agg.get_editor_property(k)) for k in ('box_elems', 'convex_elems', 'sphere_elems', 'sphyl_elems')} if agg else None
    except Exception as e:
        d['simple'] = str(e)
    d['stages'] = [s for s in ('DamageV2', 'HeavyV2', 'BrokenV2') if A.does_asset_exist(LIB + '/' + name + '_' + s)]
    res['meshes'][name] = d

for path in A.list_assets('/Game/SeniorSendOff', recursive=True, include_folder=False):
    name = path.split('/')[-1].split('.')[0]
    if not (name.startswith('MI_SSO_') or (name.startswith('M_Lib_') and LIB in path)):
        continue
    mi = unreal.load_asset(path)
    if isinstance(mi, unreal.Material):
        tex = []
        try:
            for t in unreal.MaterialEditingLibrary.get_used_textures(mi):
                tex.append(t.get_path_name())
        except Exception as e:
            tex = [str(e)]
        res['materials'][name] = {'path': path.split('.')[0], 'class': 'Material', 'textures': tex}
        continue
    if not isinstance(mi, unreal.MaterialInstanceConstant):
        continue
    d = {'path': path.split('.')[0], 'parent': mi.get_editor_property('parent').get_path_name() if mi.get_editor_property('parent') else None}
    try:
        d['textures'] = {str(p.parameter_info.name): (p.parameter_value.get_path_name() if p.parameter_value else None) for p in mi.get_editor_property('texture_parameter_values')}
        d['scalars'] = {str(p.parameter_info.name): round(p.parameter_value, 3) for p in mi.get_editor_property('scalar_parameter_values')}
        d['vectors'] = {str(p.parameter_info.name): [round(p.parameter_value.r, 3), round(p.parameter_value.g, 3), round(p.parameter_value.b, 3)] for p in mi.get_editor_property('vector_parameter_values')}
    except Exception as e:
        d['error'] = str(e)
    res['materials'][name] = d

with open(OUT, 'w') as f:
    json.dump(res, f, indent=1, sort_keys=True)
unreal.log('CAMPUS_KIT_SURVEY meshes=%d materials=%d -> %s' % (len(res['meshes']), len(res['materials']), OUT))
