import unreal,json,os,traceback
B=unreal.Paths.project_dir()+'SourceArt/HouseScale110/'
def v(x):return [x.x,x.y,x.z]
def prop(a,k):
 try:return a.get_editor_property(k)
 except:return None
def serial(x):
 if isinstance(x,unreal.Vector):return v(x)
 if isinstance(x,unreal.Vector2D):return [x.x,x.y]
 if isinstance(x,unreal.Object):return x.get_path_name()
 if isinstance(x,(str,int,float,bool)) or x is None:return x
 return str(x)
try:
 sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 rows=[]
 for a in sub.get_all_level_actors():
  p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d();o,e=a.get_actor_bounds(False)
  par=a.get_attach_parent_actor()
  row=dict(name=a.get_name(),label=a.get_actor_label(),class_name=a.get_class().get_name(),location=v(p),rotation=[r.pitch,r.yaw,r.roll],scale=v(s),bounds=[v(o-e),v(o+e)],parent=par.get_name() if par else None,properties={},lights=[])
  cls=row['class_name']
  fields=[]
  if cls=='ExistingHouseDoor':fields=['door_id','doorway_center','panel_center_world','panel_half_extent','added_handle_world_position','local_leaf_space','closed_yaw','open_yaw','source_geometry_origin_world']
  if cls=='HouseHotTubWater':fields=['water_size','corner_radius','water_depth','steam_rise_height','maximum_wave_height']
  if cls=='HouseRoamingPig':
   fields=['bed_location','pigsty_minimum','pigsty_maximum','visual_scale','max_health','walk_speed','run_speed']
   row['roam_graph']=[dict(feet=v(n.get_editor_property('feet')),links=list(n.get_editor_property('links')),inside=n.get_editor_property('inside_pigsty'),gate=str(n.get_editor_property('gate_door_id'))) for n in a.get_editor_property('roam_graph')]
  if cls=='HouseImpossibleHallway':fields=['module_length','loop_depth','return_min_depth','corridor_half_width','corridor_height']
  if isinstance(a,unreal.Character):row['capsule_half_height']=a.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
  if cls=='PlayerStart':row['capsule_half_height']=a.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
  row['properties']={k:serial(prop(a,k)) for k in fields}
  for c in a.get_components_by_class(unreal.LocalLightComponent):
   cr={'name':c.get_name(),'class':c.get_class().get_name()}
   for k in ['attenuation_radius','source_radius','soft_source_radius','source_length','intensity','source_width','source_height']:
    q=prop(c,k)
    if q is not None:cr[k]=serial(q)
   row['lights'].append(cr)
  rows.append(row)
 json.dump(rows,open(B+'before/actors.json','w'),indent=2)
 unreal.log('HOUSE_SCALE_SNAPSHOT_PASS actors='+str(len(rows)))
except:unreal.log_error(traceback.format_exc())
