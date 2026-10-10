import json, pathlib, re, traceback, unreal

B = pathlib.Path(unreal.Paths.project_dir()) / 'SourceArt/NightClub'
exec((B / 'install.py').read_text().split('def run():')[0], globals())
D = '/Game/SeniorSendOff/House/NightClub/Lounge'
L = B / 'lounge_layout'
CHANGED = {
    'NightClub_Lower_DJ_console', 'NightClub_Lower_Speaker_0',
    'NightClub_Lower_Speaker_1', 'NightClub_Lower_Leather_sofa',
    'NightClub_Upper_Leather_sofa', 'NightClub_Upper_Lounge_table',
}
NEW = {
    'NightClub_Lounge_UpperChaise', 'NightClub_Lounge_UpperOttoman',
    'NightClub_Lounge_UpperSideTable', 'NightClub_Lounge_UpperPillow_0',
    'NightClub_Lounge_UpperPillow_1',
}

def snapshot(a):
    r = a.get_actor_rotation(); center, extent = a.get_actor_bounds(False)
    row = dict(name=a.get_name(), label=a.get_actor_label(), cls=a.get_class().get_name(),
               location=xyz(a.get_actor_location()), rotation=[r.pitch,r.yaw,r.roll],
               scale=xyz(a.get_actor_scale3d()), bounds=[xyz(center-extent),xyz(center+extent)],
               meshes=[], lights=[])
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        if c.static_mesh:
            row['meshes'].append(dict(component=c.get_name(), mesh=c.static_mesh.get_path_name(),
                materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],
                collision=str(c.get_collision_enabled()), visible=c.is_visible()))
    for c in a.get_components_by_class(unreal.LightComponent):
        row['lights'].append(dict(component=c.get_name(), intensity=c.intensity))
    return row

def authored_state(row):
    # Bounds of instanced Cheetos/window fragments settle after asynchronous mesh
    # loading. Preserve every authored field while retaining bounds for fit checks.
    return {k:v for k,v in row.items() if k != 'bounds'}

def position(a, p, yaw=0, scale=None):
    a.set_actor_location(unreal.Vector(*p), False, True)
    a.set_actor_rotation(unreal.Rotator(yaw=yaw), True)
    if scale: a.set_actor_scale3d(unreal.Vector(*scale))

def run_lounge():
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
    original = {a.get_name(): snapshot(a) for a in S.get_all_level_actors()}
    actors = {a.get_actor_label(): a for a in S.get_all_level_actors()}
    assert CHANGED <= actors.keys(), 'Expected club furnishings missing'
    art = json.loads((B / 'lounge_refit/lounge_manifest.json').read_text())
    mats = {}
    for name, row in art['materials'].items():
        if row.get('reuse_asset'):
            mats[name] = A.load_asset(row['reuse_asset'])
            assert mats[name], ('Missing protected common material', name)
        else:
            mats[name] = A.load_asset(D+'/'+name) if A.does_asset_exist(D+'/'+name) else material(name, row)
    meshes = {}; imports = []
    for row in art['assets']:
        mesh = A.load_asset(D+'/'+row['name']) if A.does_asset_exist(D+'/'+row['name']) else imp(row['fbx'], row['name'])
        slots = list(mesh.static_materials)
        for slot in slots:
            key = re.sub(r'[._]\d+$', '', str(slot.get_editor_property('imported_material_slot_name')))
            assert key in mats, (row['name'], key)
            slot.material_interface = mats[key]
        mesh.static_materials = slots
        mesh.set_editor_property('support_ray_tracing', False)
        setup = mesh.get_editor_property('body_setup')
        setup.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
        count = M.get_convex_collision_count(mesh)
        assert count == row['collisionHullCount'], (row['name'], 'UCX hulls', count, row['collisionHullCount'])
        assert A.save_loaded_asset(mesh, False), row['name']
        meshes[row['name']] = mesh
        imports.append(dict(name=row['name'], convex_hulls=count, materials=[s.material_interface.get_path_name() for s in slots]))

    def furnish(label, base, p, display, mass=None, yaw=0):
        spec = art['props'][base]
        actor = actors.get(label)
        if not actor:
            actor = S.spawn_actor_from_class(unreal.HouseInteractiveProp, unreal.Vector(*p), unreal.Rotator(yaw=yaw))
            actor.set_actor_label(label)
            actor.set_editor_property('prop_id', label)
            actors[label] = actor
        assert isinstance(actor, unreal.HouseInteractiveProp), label
        actor.set_folder_path('Nightclub/Furniture')
        position(actor, p, yaw, (1,1,1))
        for stage, key in [('intact','intact_mesh'),('damaged','damaged_mesh'),('heavily_damaged','heavily_damaged_mesh'),('broken','broken_mesh')]:
            actor.set_editor_property(key, meshes.get(spec['meshes'].get(stage)))
        actor.set_editor_property('material_overrides', [])
        actor.set_editor_property('prop_name', display)
        weight = float(mass if mass is not None else spec['massKg'])
        actor.set_editor_property('mass_kg', weight)
        actor.set_editor_property('break_health', max(float(spec['breakHealth']), weight*.65+18))
        actor.set_editor_property('movable', True)
        actor.set_editor_property('breakable', 'broken' in spec['meshes'])
        actor.set_editor_property('wall_mounted', False)
        actor.set_editor_property('broken_version_movable', True)
        actor.refresh_prop()
        assert actor.get_editor_property('replicates'), ('Furniture must replicate', label)
        return actor

    position(actors['NightClub_Lower_DJ_console'], (-1545,-1210,.88), 90)
    position(actors['NightClub_Lower_Speaker_0'], (-1530,-1400,.88), 90)
    position(actors['NightClub_Lower_Speaker_1'], (-1530,-1025,.88), 90)
    position(actors['NightClub_Lower_Leather_sofa'], (-1851.255095,-1440,.88), 0)
    furnish('NightClub_Upper_Leather_sofa', 'SM_NCL_SectionalMain', (-1850,-1440,333.1), 'Leather sectional sofa',115)
    furnish('NightClub_Lounge_UpperChaise', 'SM_NCL_SectionalChaise', (-1630,-1382.5,333.1), 'Leather sectional chaise',70)
    furnish('NightClub_Upper_Lounge_table', 'SM_NCL_CoffeeTable', (-1765,-1340,333.1), 'Walnut coffee table',22)
    furnish('NightClub_Lounge_UpperOttoman', 'SM_NCL_Ottoman', (-1620,-1155,333.1), 'Leather ottoman',24)
    furnish('NightClub_Lounge_UpperSideTable', 'SM_NCL_SideTable', (-1538,-1370,333.1), 'Walnut side table',14)
    furnish('NightClub_Lounge_UpperPillow_0', 'SM_NCL_PillowPlum', (-1960,-1425,377.1), 'Plum throw pillow',.7,yaw=-10)
    furnish('NightClub_Lounge_UpperPillow_1', 'SM_NCL_PillowTeal', (-1633,-1400,377.1), 'Teal throw pillow',.7,yaw=12)

    # Actual imported mesh bounds: avoid walls and the north pole access gate.
    furniture = [snapshot(actors[label]) for label in sorted(CHANGED | NEW)]
    for row in furniture:
        lo, hi = row['bounds']
        assert lo[0] >= -2212.52 and hi[0] <= -1490+.1 and lo[1] >= -1509.04 and hi[1] <= -973.4, ('Room bounds', row['label'], row['bounds'])
        # The landing/gate corridor lies north of Y=-1065 and around X=-1935.
        assert not (lo[0] < -1865 and hi[0] > -2005 and hi[1] > -1065), ('Pole landing blocked', row['label'])
    main, chaise = actors['NightClub_Upper_Leather_sofa'], actors['NightClub_Lounge_UpperChaise']
    mc, me = main.get_actor_bounds(False); cc, ce = chaise.get_actor_bounds(False)
    assert abs((mc.x+me.x)-(cc.x-ce.x)) < .1, 'Sectional join has a gap'
    assert abs((mc.y-me.y)-(cc.y-ce.y)) < .1, 'Sectional rear edges misaligned'

    after = {a.get_name(): snapshot(a) for a in S.get_all_level_actors()}
    drift = [r['label'] for n,r in original.items() if r['label'] not in CHANGED | NEW and (n not in after or authored_state(r) != authored_state(after[n]))]
    (L/'protected-drift.json').write_text(json.dumps([dict(before=r,after=after.get(n)) for n,r in original.items() if r['label'] in drift],indent=2))
    assert not drift, ('Protected actors changed', drift)
    assert not [r['label'] for n,r in after.items() if n not in original and r['label'] not in NEW], 'Unexpected new actor'
    assert len(after) == len(original) + len(NEW - {r['label'] for r in original.values()})
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level(), 'Map save failed'
    (L/'actors_after.json').write_text(json.dumps(list(after.values()), indent=2))
    report = dict(status='PASS', protected_unchanged=True,
        protected_actor_count=sum(r['label'] not in CHANGED | NEW for r in original.values()),
        changed_existing=sorted(CHANGED), added=sorted(NEW), actor_count=len(after),
        derived_bounds_settled=[r['label'] for n,r in original.items() if r['label'] not in CHANGED | NEW and r['bounds'] != after[n]['bounds']],
        club_lights_unchanged=True, pole_and_doors_unchanged=True, imports=imports, furniture=furniture,
        notes=['Both speakers and DJ face into room from east wall. Lower couch uses former DJ wall.',
               'Upper sectional consists of independently movable and breakable modules.',
               'Loose cushions use native pickup and multiplayer replication.'])
    (L/'lounge-install.json').write_text(json.dumps(report, indent=2))
    unreal.log('NIGHTCLUB_LOUNGE_INSTALL_PASS protected='+str(report['protected_actor_count']))

try:
    run_lounge()
except:
    unreal.log_error('NIGHTCLUB_LOUNGE_INSTALL_FAIL '+traceback.format_exc())
