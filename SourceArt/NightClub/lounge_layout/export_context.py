import json, pathlib, shutil, traceback, unreal

B = pathlib.Path(unreal.Paths.project_dir()) / 'SourceArt/NightClub/lounge_layout'
B.mkdir(parents=True, exist_ok=True)
def xyz(v): return [v.x, v.y, v.z]
try:
    src = pathlib.Path(unreal.Paths.project_content_dir()) / 'Story/Maps/Chapter01_House.umap'
    shutil.copy2(src, B / 'Chapter01_House_before_lounge.umap')
    unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
    S = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rows = []
    for a in S.get_all_level_actors():
        r = a.get_actor_rotation(); origin, extent = a.get_actor_bounds(False)
        row = dict(name=a.get_name(), label=a.get_actor_label(), cls=a.get_class().get_name(),
                   location=xyz(a.get_actor_location()), rotation=[r.pitch,r.yaw,r.roll],
                   scale=xyz(a.get_actor_scale3d()), bounds=[xyz(origin-extent),xyz(origin+extent)], meshes=[], lights=[])
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            if c.static_mesh:
                row['meshes'].append(dict(component=c.get_name(), mesh=c.static_mesh.get_path_name(),
                   materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],
                   collision=str(c.get_collision_enabled()), visible=c.is_visible()))
        for c in a.get_components_by_class(unreal.LightComponent):
            row['lights'].append(dict(component=c.get_name(), intensity=c.intensity))
        rows.append(row)
    (B/'actors_before.json').write_text(json.dumps(rows,indent=2))
    unreal.log('NIGHTCLUB_LOUNGE_CONTEXT_PASS actors='+str(len(rows)))
except:
    unreal.log_error('NIGHTCLUB_LOUNGE_CONTEXT_FAIL '+traceback.format_exc())
