"""Read-only survey for the Calder State interiors (master job, steps 2c/2d): every house room's static meshes that the
campus could reuse, so each building's furniture can come from the house as the master prompt asks.

For every intact SM_* mesh under /Game/SeniorSendOff/House (damage stages _DamageV2/_HeavyV2/_BrokenV2 are listed with
their intact mesh, not on their own): folder, local bounds (cm), triangle count, material slots, simple-collision element
counts, Nanite, and which damage stages exist. Writes Saved/ClaudeDump/campus_house_assets.json. Opens no map and saves
nothing.
"""
import json, os, unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), 'ClaudeDump', 'campus_house_assets.json')
A = unreal.EditorAssetLibrary
ROOT = '/Game/SeniorSendOff/House'
SKIP = ('DamageV2', 'HeavyV2', 'BrokenV2', 'BrokenV3', '_COL', 'COL_', '_Loose_', 'Collision')


def v3(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


res = {}
paths = A.list_assets(ROOT, recursive=True, include_folder=False)
for path in paths:
    name = path.split('/')[-1].split('.')[0]
    if not name.startswith('SM_') or any(s in name for s in SKIP):
        continue
    folder = '/'.join(path.split('/')[:-1])
    if '/CampusInterior' in folder or '/Exterior' in folder:
        continue
    data = unreal.EditorAssetLibrary.find_asset_data(path)
    if not data or str(data.asset_class_path.asset_name) != 'StaticMesh':
        continue
    mesh = unreal.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        continue
    box = mesh.get_bounding_box()
    d = {'folder': folder, 'min': v3(box.min), 'max': v3(box.max)}
    try:
        d['tris'] = mesh.get_num_triangles(0)
    except Exception:
        pass
    try:
        d['mats'] = [m.material_interface.get_name() if m.material_interface else None for m in mesh.static_materials]
    except Exception:
        pass
    try:
        agg = mesh.get_editor_property('body_setup').get_editor_property('agg_geom')
        d['simple'] = sum(len(agg.get_editor_property(k)) for k in ('box_elems', 'convex_elems', 'sphere_elems', 'sphyl_elems'))
    except Exception:
        d['simple'] = None
    try:
        d['nanite'] = bool(mesh.get_editor_property('nanite_settings').get_editor_property('enabled'))
    except Exception:
        pass
    d['stages'] = [s for s in ('DamageV2', 'HeavyV2', 'BrokenV2') if A.does_asset_exist(folder + '/' + name + '_' + s)]
    res[name] = d

with open(OUT, 'w') as f:
    json.dump(res, f, indent=0, sort_keys=True)
unreal.log('CAMPUS_HOUSE_ASSETS meshes=%d -> %s' % (len(res), OUT))
