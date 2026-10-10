import unreal,json,re,glob,os,traceback
B='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ExitSigns/NeonV2/'
D='/Game/SeniorSendOff/House/ExitSigns';ed=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
try:
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House');sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);me=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
 colors=json.load(open(B+'materials.json'));mats={}
 for n,c in colors.items():
  name='NEX_'+n;m=unreal.load_asset(D+'/'+name) or at.create_asset(name,D,unreal.Material,unreal.MaterialFactoryNew());ml.delete_all_material_expressions(m)
  node=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);node.constant=unreal.LinearColor(*c,1)
  if n in ['RedLens','Status']:
   m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
   node.constant=unreal.LinearColor(1.8,0,0,1) if n=='RedLens' else unreal.LinearColor(0,.35,0,1)
   exposure=ml.create_material_expression(m,unreal.MaterialExpressionEyeAdaptationInverse)
   light_input=next(x for x in ml.get_material_expression_input_names(exposure) if 'light' in x.lower())
   assert ml.connect_material_expressions(node,'',exposure,light_input)
   assert ml.connect_material_property(exposure,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  else:
   assert ml.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
   for val,prop in [(.3 if n in ['Screw','Housing','Face'] else .58,unreal.MaterialProperty.MP_ROUGHNESS),(.55 if n in ['Screw','Housing'] else .15,unreal.MaterialProperty.MP_METALLIC)]:
    x=ml.create_material_expression(m,unreal.MaterialExpressionConstant);x.r=val;ml.connect_material_property(x,'',prop)
  m.set_editor_property('two_sided',True);ml.recompile_material(m);ed.save_loaded_asset(m);mats[name]=m
 cache={}
 for p in sorted(glob.glob(B+'models/*.fbx')):
  name=os.path.basename(p)[:-4];t=unreal.AssetImportTask();t.filename=p;t.destination_path=D;t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=False;t.factory=unreal.FbxFactory()
  ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False;ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.auto_generate_collision=False;t.options=ui;at.import_asset_tasks([t]);mesh=unreal.load_asset(D+'/'+name);assert mesh,name
  slots=list(mesh.static_materials)
  for s in slots:
   key=re.sub(r'[._]\d+$','',str(s.get_editor_property('imported_material_slot_name')));assert key in mats,(name,key);s.set_editor_property('material_interface',mats[key])
  mesh.static_materials=slots;mesh.set_editor_property('support_ray_tracing',False);mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
  me.add_simple_collisions(mesh,unreal.ScriptingCollisionShapeType.BOX);ed.save_loaded_asset(mesh);cache[name]=mesh
 actors={a.get_actor_label():a for a in sub.get_all_level_actors()};installed=[]
 for label,arrow in [('Interactive_HallEnd_ExitArrowLight__001',1),('Interactive_Kitchen_ExitSign_001',0),('Exit sign - basement library',1),('Exit sign - upper porch gallery',0)]:
  old=actors[label];p=old.get_actor_location();rot=old.get_actor_rotation();ident=old.get_editor_property('prop_id') if isinstance(old,unreal.HouseInteractiveProp) else unreal.Name('Exit_'+label.replace(' ','_'))
  if isinstance(old,unreal.HouseExitSign):a=old
  else:
   a=sub.spawn_actor_from_class(unreal.HouseExitSign,p,rot);a.set_actor_label(label+' neon');a.set_folder_path(old.get_folder_path());a.set_editor_property('prop_id',ident)
  a.set_editor_property('arrow_style',arrow);a.set_editor_property('double_sided',False)
  for prop,i in [('intact_mesh',0),('damaged_mesh',1),('heavily_damaged_mesh',2),('broken_mesh',3)]:a.set_editor_property(prop,cache['SM_NEX_Wall_'+('L'if arrow==1 else'0')+'_'+str(i)])
  a.set_editor_property('wall_mounted',True);a.set_editor_property('movable',True);a.set_editor_property('breakable',True);a.refresh_prop()
  if a!=old:assert sub.destroy_actor(old);a.set_actor_label(label)
  installed.append(dict(label=label,id=str(ident),position=str(p),rotation=str(rot)))
 # Keep a small bloom response even at the house's optimized graphics settings.
 vol=actors.get('Exit lens bloom') or sub.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector(0,0,0));vol.set_actor_label('Exit lens bloom');vol.set_editor_property('unbound',True);vol.set_editor_property('priority',2)
 settings=vol.get_editor_property('settings');settings.set_editor_property('override_bloom_intensity',True);settings.set_editor_property('bloom_intensity',1.2);settings.set_editor_property('override_bloom_threshold',True);settings.set_editor_property('bloom_threshold',.05);vol.set_editor_property('settings',settings)
 assert not any(isinstance(a,unreal.StoryCheckpoint) for a in sub.get_all_level_actors())
 assert unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Story/Maps/Chapter01_House')
 json.dump(dict(models=list(cache),placed=installed,checkpoints=0),open(B+'evidence/install.json','w'),indent=2)
 unreal.log('NEON_EXIT_INSTALL_PASS')
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
