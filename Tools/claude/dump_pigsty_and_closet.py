"""Read-only survey: pigsty actors, library actors, doors, and the mechanical closet interior.
Run headless:  UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>
Writes Saved/ClaudeDump/pigsty_closet.json
"""
import json, os, unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), 'ClaudeDump', 'pigsty_closet.json')
MAP = '/Game/Story/Maps/Chapter01_House'
unreal.EditorLoadingAndSavingUtils.load_map(MAP)
world = unreal.EditorLevelLibrary.get_editor_world()
actors = unreal.EditorLevelLibrary.get_all_level_actors()
log = unreal.log

def v3(v):
    return [round(v.x, 3), round(v.y, 3), round(v.z, 3)]

def rot(r):
    return [round(r.pitch, 3), round(r.yaw, 3), round(r.roll, 3)]

def prop(a, name):
    try:
        val = a.get_editor_property(name)
    except Exception as e:
        return None
    if isinstance(val, unreal.Vector): return v3(val)
    if isinstance(val, unreal.Rotator): return rot(val)
    if isinstance(val, unreal.Name): return str(val)
    if isinstance(val, (int, float, bool, str)): return val
    if isinstance(val, unreal.Object):
        try: return val.get_actor_label()
        except Exception: return val.get_name()
    if isinstance(val, unreal.Array):
        out = []
        for item in val:
            if isinstance(item, unreal.HousePigRoamNode):
                out.append({'feet': v3(item.feet), 'links': list(item.links), 'inside': bool(item.inside_pigsty), 'gate': str(item.gate_door_id)})
            else:
                out.append(str(item))
        return out
    return str(val)

def describe(a, full=False):
    d = {
        'label': a.get_actor_label(),
        'class': a.get_class().get_name(),
        'folder': str(a.get_folder_path()),
        'location': v3(a.get_actor_location()),
        'rotation': rot(a.get_actor_rotation()),
        'scale': v3(a.get_actor_scale3d()),
    }
    try:
        o, e = a.get_actor_bounds(False)
        d['bounds_origin'] = v3(o); d['bounds_extent'] = v3(e)
    except Exception:
        pass
    try:
        d['tags'] = [str(t) for t in a.tags]
    except Exception:
        pass
    if full:
        # static mesh + materials
        try:
            smc = a.static_mesh_component if hasattr(a, 'static_mesh_component') else None
            if smc is None:
                comps = a.get_components_by_class(unreal.StaticMeshComponent)
                smc = comps[0] if comps else None
            if smc is not None and smc.static_mesh is not None:
                d['static_mesh'] = smc.static_mesh.get_path_name()
                d['materials'] = [m.get_path_name() if m else None for m in smc.get_materials()]
                d['collision_profile'] = str(smc.get_collision_profile_name())
                d['mobility'] = str(smc.mobility)
        except Exception as e:
            d['mesh_error'] = str(e)
        for name in ('prop_id', 'door_id', 'doorway_center', 'closed_yaw', 'open_yaw', 'locked', 'original_leaf',
                     'pig_name', 'visual_scale', 'max_health', 'pigsty_door_id', 'pigsty_minimum', 'pigsty_maximum',
                     'bed_location', 'bed_yaw', 'snack_pile', 'personality_seed', 'walk_speed', 'run_speed',
                     'escape_probability', 'roam_graph', 'food_count', 'scatter_radius', 'scatter_seed',
                     'feeding_offset_a', 'feeding_offset_b', 'cheeto_mesh', 'body_mesh', 'head_mesh', 'upper_leg_mesh', 'lower_leg_mesh'):
            val = prop(a, name)
            if val is not None:
                d[name] = val
        # component list summary
        try:
            d['components'] = [c.get_name() + ':' + c.get_class().get_name() for c in a.get_components_by_class(unreal.SceneComponent)][:40]
        except Exception:
            pass
    return d

result = {'map': MAP, 'pigsty': [], 'pig_like': [], 'library': [], 'doors': [], 'hall_end': [], 'traces': {}, 'folders': {}}
for a in actors:
    label = a.get_actor_label(); folder = str(a.get_folder_path()); cls = a.get_class().get_name()
    result['folders'][folder] = result['folders'].get(folder, 0) + 1
    if folder.startswith('Second floor/Pigsty') or label.startswith('PIG_') or cls in ('HouseRoamingPig', 'HousePigSnackPile'):
        result['pigsty'].append(describe(a, True))
    elif 'pig' in label.lower():
        result['pig_like'].append(describe(a, False))
    if folder.startswith('Basement/ReferenceLibrary') or 'Library' in label or 'Lib_' in label:
        result['library'].append(describe(a, 'Mechanical' in label or 'Door' in label))
    if cls in ('ExistingHouseDoor', 'HouseBlackHallDoor') or isinstance(a, unreal.ExistingHouseDoor):
        result['doors'].append(describe(a, True))
    if 'HallEnd' in label or folder.startswith('Second floor'):
        result['hall_end'].append(describe(a, False))

# ---------------- traces around the mechanical closet door and the library ----------------
def trace(start, end):
    hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    try:
        t = hit.to_tuple()
    except Exception:
        return {'raw': str(hit)}
    # (blocking_hit, initial_overlap, time, distance, location, impact_point, normal, impact_normal, phys_mat, hit_actor, hit_component, ...)
    out = {'distance': None, 'point': None, 'actor': None}
    try:
        out['distance'] = round(float(t[3]), 2)
        out['point'] = v3(t[4])
        act = t[9]
        out['actor'] = act.get_actor_label() if act else None
    except Exception as e:
        out['tuple'] = [str(x) for x in t]
    return out

def probe(name, p):
    r = {'probe': v3(p)}
    L = 2000.0
    dirs = {'+X': unreal.Vector(L, 0, 0), '-X': unreal.Vector(-L, 0, 0), '+Y': unreal.Vector(0, L, 0), '-Y': unreal.Vector(0, -L, 0),
            '+Z': unreal.Vector(0, 0, 600), '-Z': unreal.Vector(0, 0, -600)}
    for k, dv in dirs.items():
        r[k] = trace(p, p + dv)
    result['traces'][name] = r

mech = None
for d in result['doors']:
    if d.get('door_id') == 'BasementLibrary_Mechanical':
        mech = d
if mech:
    hinge = unreal.Vector(*mech['location'])
    center = unreal.Vector(*mech['doorway_center']) if mech.get('doorway_center') else hinge
    along = unreal.Vector(center.x - hinge.x, center.y - hinge.y, 0)
    if along.length() < 1: along = unreal.Vector(1, 0, 0)
    along = along / along.length()
    normal = unreal.Vector(-along.y, along.x, 0)
    floor_z = -329.06 + 110
    for sign, tag in ((1, 'A'), (-1, 'B')):
        for dist in (80, 160):
            p = unreal.Vector(center.x + normal.x * sign * dist, center.y + normal.y * sign * dist, floor_z)
            probe('closet_side_%s_%d' % (tag, dist), p)
    result['traces']['door_axes'] = {'along': v3(along), 'normal': v3(normal), 'center': v3(center), 'hinge': v3(hinge)}
# library room probes
for i, (x, y) in enumerate([(-2700, 0), (-2700, -700), (-2700, 700), (-2550, -800), (-2850, -800)]):
    probe('library_%d' % i, unreal.Vector(x, y, -329.06 + 120))
# pigsty room probes (second floor)
for i, (x, y) in enumerate([(-2660, -1070), (-2460, -900), (-2860, -1200)]):
    probe('pigsty_%d' % i, unreal.Vector(x, y, 302.3 + 120))

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w') as f:
    json.dump(result, f, indent=1)
log('CLAUDE_DUMP_OK ' + OUT + ' pigsty=%d library=%d doors=%d' % (len(result['pigsty']), len(result['library']), len(result['doors'])))
