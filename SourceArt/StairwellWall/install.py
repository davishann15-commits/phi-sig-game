import unreal,json,re,traceback,os
B='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/StairwellWall/'
D='/Game/SeniorSendOff/House/StairwellRepair'
try:
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 plan=json.load(open(B+'evidence/build.json'));ed=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools()
 w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House');sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors={a.get_actor_label():a for a in sub.get_all_level_actors()}
 for row in plan['meshes']:
  a=actors[row['label']];c=a.static_mesh_component;old=c.static_mesh
  t=unreal.AssetImportTask();t.filename=row['file'];t.destination_path=D;t.destination_name=row['name'];t.automated=True;t.replace_existing=True;t.save=False;t.factory=unreal.FbxFactory()
  opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_as_skeletal=False;opt.import_materials=False;opt.import_textures=False;opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=False;t.options=opt
  at.import_asset_tasks([t]);m=unreal.load_asset(D+'/'+row['name']);assert m
  mats={p.split('.')[-1]:unreal.load_asset(p) for p in row['materials']}
  slots=list(m.static_materials)
  for s in slots:
   key=re.sub(r'[._]\d+$','',str(s.get_editor_property('imported_material_slot_name')));assert key in mats,key;s.set_editor_property('material_interface',mats[key])
  m.static_materials=slots;m.set_editor_property('nanite_settings',old.get_editor_property('nanite_settings'));m.set_editor_property('support_ray_tracing',False)
  assert ed.save_loaded_asset(m);c.set_static_mesh(m)
 cube=unreal.load_asset('/Engine/BasicShapes/Cube')
 for i,(x0,x1,z0,z1) in enumerate(plan['rectangles']):
  label='Stairwall flush collision '+str(i)
  a=actors.get(label) or sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector((x0+x1)/2,plan['front_plane_y']-1.5,(z0+z1)/2))
  a.set_actor_label(label);a.set_folder_path('House repairs/Stairwell');a.set_actor_hidden_in_game(True)
  c=a.static_mesh_component;c.set_static_mesh(cube);c.set_collision_profile_name('BlockAll');c.set_editor_property('cast_shadow',False);a.set_actor_scale3d(unreal.Vector((x1-x0)/100,.03,(z1-z0)/100))
 assert unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Story/Maps/Chapter01_House')
 assert not any(isinstance(a,unreal.StoryCheckpoint) for a in sub.get_all_level_actors())
 unreal.log('STAIR_WALL_INSTALL_PASS wall_y='+str(plan['front_plane_y']))
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
