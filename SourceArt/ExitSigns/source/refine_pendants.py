import unreal,glob,os,re,traceback
B='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ExitSigns/'
D='/Game/SeniorSendOff/House/ExitSigns'
try:
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 for p in glob.glob(B+'models/SM_EX_Double_*.fbx'):
  n=os.path.basename(p)[:-4];t=unreal.AssetImportTask();t.filename=p;t.destination_path=D;t.destination_name=n;t.automated=True;t.replace_existing=True;t.save=False;t.factory=unreal.FbxFactory();ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False;ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.auto_generate_collision=False;t.options=ui;unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t]);m=unreal.load_asset(D+'/'+n);slots=list(m.static_materials)
  for s in slots:
   key=re.sub(r'[._]\d+$','',str(s.get_editor_property('imported_material_slot_name')));mat=unreal.load_asset(D+'/'+key);assert mat,key;s.set_editor_property('material_interface',mat)
  m.static_materials=slots;unreal.EditorAssetLibrary.save_loaded_asset(m)
 unreal.log('EXIT_PENDANT_REFINEMENT_PASS')
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
