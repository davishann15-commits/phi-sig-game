import unreal,json,pathlib,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def state(a):
 p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d()
 return ((p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z),[(c.static_mesh.get_path_name() if c.static_mesh else None,[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())])for c in a.get_components_by_class(unreal.StaticMeshComponent)],[(c.intensity,c.cast_shadows)for c in a.get_components_by_class(unreal.LightComponent)])
try:
 unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 before={a.get_name():state(a)for a in S.get_all_level_actors() if not a.get_actor_label().startswith('NightClub_')}
 for a in S.get_all_level_actors():
  if a.get_actor_label()=='NightClub_UpperPracticalLensCover':S.destroy_actor(a)
 a=S.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-1924.67,-1224.3,614.65))
 a.set_actor_label('NightClub_UpperPracticalLensCover');a.set_folder_path('Nightclub/Decor');a.set_actor_scale3d(unreal.Vector(.65,.65,.012))
 c=a.static_mesh_component;c.set_static_mesh(unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/Cylinder'))
 c.set_material(0,unreal.EditorAssetLibrary.load_asset('/Game/SeniorSendOff/House/NightClub/M_NC_PowderCoat'))
 c.set_collision_profile_name('NoCollision');c.set_cast_shadow(False);c.set_editor_property('affect_dynamic_indirect_lighting',False);c.set_editor_property('affect_distance_field_lighting',False)
 after={a.get_name():state(a)for a in S.get_all_level_actors()if not a.get_actor_label().startswith('NightClub_')}
 assert before==after,'Unrelated actor changed'
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 json.dump(dict(status='PASS',original_actors_unchanged=True,cover_location=[-1924.67,-1224.3,614.65],cover_collision='NoCollision'),open(B/'evidence/dim-lens.json','w'),indent=2)
 unreal.log('NIGHTCLUB_DIM_LENS_PASS')
except:unreal.log_error('NIGHTCLUB_DIM_LENS_FAIL '+traceback.format_exc())
