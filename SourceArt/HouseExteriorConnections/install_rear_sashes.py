import unreal,json
from pathlib import Path
B=Path(__file__).resolve().parent
rows=json.load(open(B/'sash-manifest.json'))
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);A={a.get_actor_label():a for a in S.get_all_level_actors()}
AT=unreal.AssetToolsHelpers.get_asset_tools();EA=unreal.EditorAssetLibrary
for r in rows:
 ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False;ui.automated_import_should_detect_type=False;ui.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
 ui.static_mesh_import_data.auto_generate_collision=False;ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.reorder_material_to_fbx_order=True
 task=unreal.AssetImportTask();task.factory=unreal.FbxFactory();task.filename=r['fbx'];task.destination_path='/Game/SeniorSendOff/House/ExteriorConnections';task.destination_name=r['asset'];task.automated=True;task.replace_existing=True;task.options=ui
 AT.import_asset_tasks([task]);m=unreal.load_asset(task.destination_path+'/'+r['asset']);assert isinstance(m,unreal.StaticMesh)
 for i,p in enumerate(r['materials']):m.set_material(i,unreal.load_asset(p))
 m.set_editor_property('support_ray_tracing',False)
 a=A[r['actor']]
 if r['kind']=='frame':
  a.set_editor_property('breakaway_frame_mesh',m)
  a.set_editor_property('breakaway_frame_materials',[unreal.load_asset(p) for p in r['materials']])
 else:
  c=a.get_component_by_class(unreal.StaticMeshComponent);tr=c.get_world_transform()
  c.set_static_mesh(m);c.set_editor_property('override_materials',[])
  for i,p in enumerate(r['materials']):c.set_material(i,unreal.load_asset(p))
  c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
  assert c.get_world_transform()==tr
 assert EA.save_loaded_asset(m)
assert unreal.EditorLoadingAndSavingUtils.save_map(unreal.EditorLevelLibrary.get_editor_world(),'/Game/Story/Maps/Chapter01_House')
unreal.log('CONNECTED_SASH_INSTALLED '+str(len(rows)));unreal.SystemLibrary.quit_editor()
