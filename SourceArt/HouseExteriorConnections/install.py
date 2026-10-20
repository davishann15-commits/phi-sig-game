"""Install only revised collision assets; preserve the original meshes and room layouts."""
import unreal,json,shutil
from pathlib import Path
B=Path(__file__).resolve().parent
P=B.parents[1]
rows=json.load(open(B/'collision-manifest.json'))
AT=unreal.AssetToolsHelpers.get_asset_tools();EA=unreal.EditorAssetLibrary
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors={a.get_actor_label():a for a in S.get_all_level_actors()}
installed=[]
for r in rows:
 # Original packages remain untouched and recoverable.
 rel='Content/'+r['source_mesh'].split('.')[0].removeprefix('/Game/')+'.uasset'
 src=P/rel;dst=B/'backup'/rel;dst.parent.mkdir(parents=True,exist_ok=True)
 if src.exists() and not dst.exists():shutil.copy2(src,dst)
 ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False;ui.automated_import_should_detect_type=False;ui.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
 ui.static_mesh_import_data.auto_generate_collision=False;ui.static_mesh_import_data.combine_meshes=True
 task=unreal.AssetImportTask();task.factory=unreal.FbxFactory();task.filename=r['fbx'];task.destination_path='/Game/SeniorSendOff/House/ExteriorConnections';task.destination_name=r['asset'];task.automated=True;task.replace_existing=True;task.options=ui;task.save=False
 AT.import_asset_tasks([task]);m=unreal.load_asset(task.destination_path+'/'+r['asset'])
 if not isinstance(m,unreal.StaticMesh):raise RuntimeError('Collision import failed '+r['asset'])
 m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 m.set_editor_property('complex_collision_mesh',None)
 m.set_editor_property('support_ray_tracing',False)
 if not EA.save_loaded_asset(m):raise RuntimeError('Collision save failed '+r['asset'])
 a=actors[r['actor']];c=a.get_component_by_class(unreal.StaticMeshComponent)
 old=c.static_mesh.get_path_name();tr=c.get_world_transform()
 if r.get('custom_collision'):
  # Retain the original rendered mesh and its material slots. Its imported
  # convex hull spans the little rear windows; replace that physics with an
  # exact, hidden triangle collider at the identical component transform.
  label='COL_Connected_Rendered_'+r['actor'][3:]
  collider=actors.get(label)
  if not collider:
   collider=S.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
   collider.set_actor_label(label)
  cc=collider.get_component_by_class(unreal.StaticMeshComponent)
  cc.set_static_mesh(m);collider.set_actor_transform(tr,False,False)
  cc.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
  cc.set_visibility(False);collider.set_actor_hidden_in_game(True)
  c.set_static_mesh(unreal.load_asset(r['source_mesh']))
  c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 else:c.set_static_mesh(m)
 assert c.get_world_transform()==tr
 installed.append(dict(actor=r['actor'],before=old,after=c.static_mesh.get_path_name()))
if not unreal.EditorLoadingAndSavingUtils.save_map(unreal.EditorLevelLibrary.get_editor_world(),'/Game/Story/Maps/Chapter01_House'):raise RuntimeError('Map save failed')
(B/'installed.json').write_text(json.dumps(installed,indent=2));unreal.log('CONNECTED_COLLISION_INSTALLED '+str(len(installed)))

unreal.SystemLibrary.quit_editor()
