import unreal, json, pathlib, traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/Room101Entry'
B.mkdir(parents=True,exist_ok=True)
def arr(v):return [v.x,v.y,v.z]
def value(v):
 if isinstance(v,unreal.Vector):return arr(v)
 if isinstance(v,unreal.Object):return v.get_path_name()
 if isinstance(v,(bool,int,float,str)) or v is None:return v
 return str(v)
try:
 unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 rows=[]
 for a in sub.get_all_level_actors():
  r=a.get_actor_rotation();o,e=a.get_actor_bounds(False)
  row=dict(name=a.get_name(),label=a.get_actor_label(),cls=a.get_class().get_name(),location=arr(a.get_actor_location()),rotation=[r.pitch,r.yaw,r.roll],scale=arr(a.get_actor_scale3d()),bounds=[arr(o-e),arr(o+e)],meshes=[])
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   if c.static_mesh:row['meshes'].append(dict(component=c.get_name(),mesh=c.static_mesh.get_path_name(),materials=[value(c.get_material(i)) for i in range(c.get_num_materials())]))
  if isinstance(a,unreal.ExistingHouseDoor):
   row['properties']={k:value(a.get_editor_property(k)) for k in ['door_id','original_leaf','original_glass','added_handle_mesh','doorway_center','panel_center_world','panel_half_extent','local_leaf_space','closed_yaw','open_yaw','locked','source_geometry_origin_world']}
  rows.append(row)
 json.dump(rows,open(B/'baseline_actors.json','w'),indent=2)
 unreal.log('ROOM101_INSPECT_PASS actors='+str(len(rows)))
except:unreal.log_error('ROOM101_INSPECT_FAIL '+traceback.format_exc())
