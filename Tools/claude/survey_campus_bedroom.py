"""Read-only (master job, step 2c): how the house's Boywick bedroom stacks its loft bed, mattress and pillows, so the
campus dorm rooms can stack them the same way. For each SM_BWR_LoftFrame actor in Chapter01_House, the bedroom pieces
within 400 cm, in the loft's local frame (location, yaw, scale). Writes Saved/ClaudeDump/campus_bedroom.json; saves nothing."""
import json, os, unreal
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'ClaudeDump', 'campus_bedroom.json')
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
def mesh_of(a):
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        if c.static_mesh: return c.static_mesh.get_name()
    return None
acts = [(a, mesh_of(a)) for a in unreal.EditorLevelLibrary.get_all_level_actors()]
res = []
for a, m in acts:
    if not m or not m.startswith('SM_BWR_LoftFrame'): continue
    t = a.get_actor_transform(); inv = t.inverse()
    e = {'loft': a.get_actor_label(), 'yaw': round(t.rotation.rotator().yaw, 1), 'scale': [round(t.scale3d.x, 3), round(t.scale3d.y, 3), round(t.scale3d.z, 3)], 'near': []}
    for b, n in acts:
        if not n or b == a or not n.startswith('SM_BWR_'): continue
        p = b.get_actor_location()
        if (p - t.translation).length() > 400: continue
        l = inv.transform_location(p)
        e['near'].append({'mesh': n, 'local': [round(l.x, 1), round(l.y, 1), round(l.z, 1)], 'yaw': round((b.get_actor_rotation().yaw - t.rotation.rotator().yaw + 540) % 360 - 180, 1),
                          'scale': [round(b.get_actor_scale3d().x, 3), round(b.get_actor_scale3d().y, 3), round(b.get_actor_scale3d().z, 3)]})
    res.append(e)
json.dump(res, open(OUT, 'w'), indent=1)
unreal.log('CAMPUS_BEDROOM lofts=%d' % len(res))
