import unreal,json,pathlib,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub/expansion2_envelope'
(B/'fbx').mkdir(parents=True,exist_ok=True)
def xyz(v):return [v.x,v.y,v.z]
def rot(r):return [r.pitch,r.yaw,r.roll]
try:
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_name()=='Chapter01_House',world.get_name()
    S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rows=[];exports=[]
    for a in S.get_all_level_actors():
        o,e=a.get_actor_bounds(False);lo=o-e;hi=o+e
        comps=a.get_components_by_class(unreal.StaticMeshComponent)
        meshes=[c for c in comps if c.static_mesh]
        if not meshes:continue
        if hi.x < -2400 or lo.x > -650 or hi.y < -2500 or lo.y > -600:continue
        row=dict(name=a.get_name(),label=a.get_actor_label(),cls=a.get_class().get_name(),location=xyz(a.get_actor_location()),rotation=rot(a.get_actor_rotation()),scale=xyz(a.get_actor_scale3d()),bounds=[xyz(lo),xyz(hi)],meshes=[])
        for c in meshes:
            cr=dict(component=c.get_name(),asset=c.static_mesh.get_path_name(),collision=str(c.get_collision_enabled()),visible=c.is_visible(),materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())])
            if a.get_class().get_name()=='StaticMeshActor':
                path=B/'fbx'/(a.get_name()+'_'+c.get_name()+'.fbx')
                t=unreal.AssetExportTask();t.object=c.static_mesh;t.filename=str(path);t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX()
                assert unreal.Exporter.run_asset_export_task(t),row['label']
                cr['fbx']=str(path);exports.append(row['label'])
            row['meshes'].append(cr)
        rows.append(row)
    (B/'actors.json').write_text(json.dumps(rows,indent=2))
    unreal.log('EXPANSION2_ENVELOPE_PASS actors='+str(len(rows))+' exported='+str(len(exports)))
except:unreal.log_error('EXPANSION2_ENVELOPE_FAIL '+traceback.format_exc())
