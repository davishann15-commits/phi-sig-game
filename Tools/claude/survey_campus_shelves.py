"""Read-only survey for the Calder State interiors (master job, step 2b): how the house's basement library fills its
six-shelf bookcases. For every bookcase in Chapter01_House, the shelf-row meshes near it (BookShelfContents*), in the
bookcase's own local frame (location, yaw), plus each actor's rotation and scale. Writes
Saved/ClaudeDump/campus_shelves.json. Loads the map but never saves it.
"""
import json, math, os, unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), 'ClaudeDump', 'campus_shelves.json')
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors = unreal.EditorLevelLibrary.get_all_level_actors()


def mesh_of(a):
    comps = a.get_components_by_class(unreal.StaticMeshComponent)
    for c in comps:
        if c.static_mesh:
            return c.static_mesh.get_name()
    return None


cases, rows = [], []
for a in actors:
    m = mesh_of(a)
    if not m:
        continue
    if m.startswith('SM_Lib_SixShelfBookcase'):
        cases.append((a, m))
    elif 'BookShelfContents' in m:
        rows.append((a, m))

res = {'cases': []}
for a, m in cases:
    t = a.get_actor_transform()
    entry = {'label': a.get_actor_label(), 'mesh': m, 'loc': [round(t.translation.x, 2), round(t.translation.y, 2), round(t.translation.z, 2)],
             'yaw': round(t.rotation.rotator().yaw, 2), 'scale': [round(t.scale3d.x, 3), round(t.scale3d.y, 3), round(t.scale3d.z, 3)], 'rows': []}
    inv = t.inverse()
    for b, n in rows:
        p = b.get_actor_location()
        if (p - t.translation).length() > 160:
            continue
        local = inv.transform_location(p)
        yaw = b.get_actor_rotation().yaw - t.rotation.rotator().yaw
        entry['rows'].append({'mesh': n, 'local': [round(local.x, 2), round(local.y, 2), round(local.z, 2)], 'yaw': round((yaw + 540) % 360 - 180, 1),
                              'scale': [round(b.get_actor_scale3d().x, 3), round(b.get_actor_scale3d().y, 3), round(b.get_actor_scale3d().z, 3)]})
    res['cases'].append(entry)

with open(OUT, 'w') as f:
    json.dump(res, f, indent=1)
unreal.log('CAMPUS_SHELVES cases=%d rows=%d -> %s' % (len(cases), len(rows), OUT))
