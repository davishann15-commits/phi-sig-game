import unreal,json,re,traceback
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs');DEST='/Game/SeniorSendOff/House/GreatRoomTVs'
try:
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 if world.get_name()!='Chapter01_House':world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
 rows=json.loads((R/'shell-manifest.json').read_text());at=unreal.AssetToolsHelpers.get_asset_tools()
 for a in rows:
  ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False
  ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.auto_generate_collision=False;ui.static_mesh_import_data.reorder_material_to_fbx_order=True
  t=unreal.AssetImportTask();t.factory=unreal.FbxFactory();t.filename=a['fbx'];t.destination_path=DEST;t.destination_name=a['asset_name'];t.options=ui;t.automated=True;t.replace_existing=True;t.save=False
  at.import_asset_tasks([t]);mesh=unreal.load_asset(DEST+'/'+a['asset_name']);assert isinstance(mesh,unreal.StaticMesh)
  byname={p.rsplit('/',1)[-1].split('.')[0]:p for p in a['materials']};slots=list(mesh.static_materials)
  for slot in slots:
   n=re.sub(r'[._][0-9]{3}$','',str(slot.get_editor_property('imported_material_slot_name')));assert n in byname,n
   slot.set_editor_property('material_interface',unreal.load_asset(byname[n]))
  mesh.set_editor_property('static_materials',slots);mesh.set_editor_property('support_ray_tracing',False)
  mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
  assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
  c=existing[a['label']].static_mesh_component;c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
  assert a['removed_tv_faces']==376 and a['removed_tv_components']==2
 assert sum(isinstance(a,unreal.HouseInteractiveProp) and 'HouseWallTelevision' in [str(t) for t in a.tags] for a in actors.get_all_level_actors())==2
 assert sum(isinstance(a,unreal.HouseInteractiveProp) and 'SSOKitchenRefit' in [str(t) for t in a.tags] for a in actors.get_all_level_actors())==137
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Story/Maps/Chapter01_House')
 (R/'shells-patched.json').write_text(json.dumps(dict(passed=True,shells=rows,kitchen_props_preserved=137),indent=2))
 unreal.log('GREAT_ROOM_TV_CASINGS_REMOVED')
except Exception:
 (R/'shells-patch-error.txt').write_text(traceback.format_exc());unreal.log_error(traceback.format_exc())
unreal.SystemLibrary.execute_console_command(None,'QUIT_EDITOR')
