import unreal,json,re,glob,os,traceback
B='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ExitSigns/NeonV2/'
D='/Game/SeniorSendOff/House/ExitSigns';ed=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools()
try:
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 me=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);mats={ 'NEX_'+n:unreal.load_asset(D+'/NEX_'+n) for n in json.load(open(B+'materials.json'))}
 cache={}
 for p in sorted(glob.glob(B+'models/*_3.fbx')):
  name=os.path.basename(p)[:-4];t=unreal.AssetImportTask();t.filename=p;t.destination_path=D;t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=False;t.factory=unreal.FbxFactory()
  ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False;ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.auto_generate_collision=False;t.options=ui;at.import_asset_tasks([t]);mesh=unreal.load_asset(D+'/'+name);assert mesh,name
  slots=list(mesh.static_materials)
  for s in slots:
   key=re.sub(r'[._]\d+$','',str(s.get_editor_property('imported_material_slot_name')));assert key in mats,(name,key);s.set_editor_property('material_interface',mats[key])
  mesh.static_materials=slots;mesh.set_editor_property('support_ray_tracing',False);mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
  me.add_simple_collisions(mesh,unreal.ScriptingCollisionShapeType.BOX);ed.save_loaded_asset(mesh);cache[name]=mesh
 unreal.log('NEON_BROKEN_MESH_PASS')
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
