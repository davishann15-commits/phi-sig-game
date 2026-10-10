import os,json,unreal
OUT='/Users/Stewart/Projects/Senior sendoff/outputs/poker-room-v1/context'
labels={'HallEnd_SM_HallEnd_ThreeRoomsPracticalLights','HallEnd_COL_HallEnd_ThreeRoomsShell','HallEnd_SM_HallEnd_ThreeRoomsShell','HallEnd_SM_HallEnd_ThreeRoomsFloors'}
os.makedirs(OUT,exist_ok=True)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
def export(m,name):
 task=unreal.AssetExportTask();task.object=m;task.filename=OUT+'/'+name+'.fbx';task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.StaticMeshExporterFBX()
 if not unreal.Exporter.run_asset_export_task(task):raise RuntimeError('Export failed '+name)
rows=[];exported=[]
for a in unreal.EditorLevelLibrary.get_all_level_actors():
 label=a.get_actor_label();v=a.get_actor_location();s=a.get_actor_scale3d();o,e=a.get_actor_bounds(False)
 r={'label':label,'class':a.get_class().get_name(),'location':[v.x,v.y,v.z],'scale':[s.x,s.y,s.z],'rotation':[a.get_actor_rotation().pitch,a.get_actor_rotation().yaw,a.get_actor_rotation().roll],'bounds':[[o.x-e.x,o.y-e.y,o.z-e.z],[o.x+e.x,o.y+e.y,o.z+e.z]]}
 if isinstance(a,unreal.StaticMeshActor):
  c=a.static_mesh_component;m=c.static_mesh
  if m:
   r.update(mesh=m.get_path_name(),collision=str(c.get_collision_profile_name()),visible=c.is_visible(),materials=[x.get_path_name() if x else None for x in c.get_materials()])
   if label in labels:export(m,label);exported.append(label)
 elif isinstance(a,unreal.ExistingHouseDoor):
  for prop in ['door_id','doorway_center','locked','closed_yaw','open_yaw','original_leaf','panel_center_world','panel_half_extent']:
   value=a.get_editor_property(prop)
   r[prop]=[value.x,value.y,value.z] if isinstance(value,unreal.Vector) else value.get_path_name() if isinstance(value,unreal.Object) else value
  if str(r['door_id'])=='HallEnd_West':export(a.get_editor_property('original_leaf'),'POK_OriginalEntryLeaf');exported.append('POK_OriginalEntryLeaf')
 rows.append(r)
json.dump(rows,open(OUT+'/actors.json','w'),indent=2,default=str)
json.dump({'actors':len(rows),'exported':exported},open(OUT+'/exported.json','w'),indent=2)
unreal.log('POKER_CONTEXT_EXPORTED '+str(len(rows))+' meshes='+str(len(exported)))
