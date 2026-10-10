import unreal,json,re,glob,os,traceback
B='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ExitSigns/'
D='/Game/SeniorSendOff/House/ExitSigns';ed=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
try:
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House');sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);me=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
 actors={a.get_actor_label():a for a in sub.get_all_level_actors()}
 colors=json.load(open(B+'materials.json'));mats={}
 for n,c in colors.items():
  name='EX_'+n;m=unreal.load_asset(D+'/'+name) or at.create_asset(name,D,unreal.Material,unreal.MaterialFactoryNew());ml.delete_all_material_expressions(m)
  node=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);node.constant=unreal.LinearColor(*c,1);ml.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
  for val,prop in [(.25 if n=='Screw' else .38,unreal.MaterialProperty.MP_ROUGHNESS),(.7 if n=='Screw' else 0,unreal.MaterialProperty.MP_METALLIC)]:
   x=ml.create_material_expression(m,unreal.MaterialExpressionConstant);x.r=val;ml.connect_material_property(x,'',prop)
  if n in ['RedLens','GreenLens','Status']:
   em=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);em.constant=unreal.LinearColor(*(v*9 for v in c),1);ml.connect_material_property(em,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  m.set_editor_property('two_sided',True);ml.recompile_material(m);ed.save_loaded_asset(m);mats[name]=m
 clean={'SM_Lib_EntryWallDoorClear':('SM_Basement_p0_m2_Solid','/Game/SeniorSendOff/House/BasementLibrary/SM_Lib_EntryWallDoorClear'),'HI_WindowCleared_SM_Upper_m1_p0_Solid':('SM_Upper_m1_p0_Solid','/Game/SeniorSendOff/House/Interactivity/HI_WindowCleared_SM_Upper_m1_p0_Solid')}
 cache={}
 def norm(s):return re.sub(r'[._]\d+$','',str(s))
 for p in sorted(glob.glob(B+'models/*.fbx')):
  name=os.path.basename(p)[:-4];isclean=name.startswith('SM_EX_Clean_');old=None
  mapping=mats
  if isclean:
   label,path=clean[name[len('SM_EX_Clean_'):]];old=unreal.load_asset(path);mapping={norm(s.get_editor_property('imported_material_slot_name')):s.get_editor_property('material_interface') for s in old.static_materials}
  t=unreal.AssetImportTask();t.filename=p;t.destination_path=D;t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=False;t.factory=unreal.FbxFactory()
  ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False;ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.auto_generate_collision=False;t.options=ui;at.import_asset_tasks([t]);mesh=unreal.load_asset(D+'/'+name);assert mesh,name
  slots=list(mesh.static_materials)
  for s in slots:
   key=norm(s.get_editor_property('imported_material_slot_name'));assert key in mapping,(name,key,list(mapping));s.set_editor_property('material_interface',mapping[key])
  mesh.static_materials=slots;mesh.set_editor_property('support_ray_tracing',False)
  mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if isclean else unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
  if not isclean:me.add_simple_collisions(mesh,unreal.ScriptingCollisionShapeType.BOX)
  ed.save_loaded_asset(mesh);cache[name]=mesh
  if isclean:
   a=actors[label];c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
 for label,variant in [('Interactive_HallEnd_ExitArrowLight__001','L'),('Interactive_Kitchen_ExitSign_001','0')]:
  a=actors[label]
  for prop,i in [('intact_mesh',0),('damaged_mesh',1),('heavily_damaged_mesh',2),('broken_mesh',3)]:a.set_editor_property(prop,cache['SM_EX_Wall_'+variant+'_'+str(i)])
  a.set_editor_property('material_overrides',[]);a.get_component_by_class(unreal.StaticMeshComponent).set_editor_property('override_materials',[]);a.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(cache['SM_EX_Wall_'+variant+'_0']);a.set_actor_location(a.get_actor_location(),False,False)
 for label,mesh,p,yaw in [('Exit sign - basement library','SM_EX_Wall_GreenL',(-2290.5,738,-112.5),-90),('Exit sign - upper porch gallery','SM_EX_Wall_0_0',(1262.4,-684,532.1),90)]:
  a=actors.get(label) or sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p),unreal.Rotator(pitch=0,yaw=yaw,roll=0));a.set_actor_label(label);a.set_folder_path('House/Exit signs');a.set_actor_location(unreal.Vector(*p),False,False);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False);a.static_mesh_component.set_static_mesh(cache[mesh]);a.static_mesh_component.set_collision_profile_name('BlockAll')
 assert unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Story/Maps/Chapter01_House')
 removed=[]
 for a in sub.get_all_level_actors():
  if isinstance(a,unreal.StoryCheckpoint):removed.append(a.get_actor_label());assert sub.destroy_actor(a)
 assert not any(isinstance(a,unreal.StoryCheckpoint) for a in sub.get_all_level_actors())
 assert unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Story/Maps/Chapter01_House')
 json.dump(dict(imported=list(cache),checkpoints_removed=removed or json.load(open(B+'evidence/install.json'))['checkpoints_removed'],checkpoint_count=0),open(B+'evidence/install.json','w'),indent=2)
 unreal.log('EXIT_SIGN_INSTALL_PASS')
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
