import unreal,json
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen');(R/'context/fbx').mkdir(parents=True,exist_ok=True)
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
def v(p):return [p.x,p.y,p.z]
rows=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 c,e=a.get_actor_bounds(False);label=a.get_actor_label();row=dict(label=label,class_name=a.get_class().get_name(),location=v(a.get_actor_location()),scale=v(a.get_actor_scale3d()),rotation=[a.get_actor_rotation().pitch,a.get_actor_rotation().yaw,a.get_actor_rotation().roll],bounds=[v(c-e),v(c+e)],tags=[str(t) for t in a.tags],components=[])
 if isinstance(a,unreal.ExistingHouseDoor):
  for k in ['door_id','doorway_center','closed_yaw','open_yaw','locked','original_leaf','panel_half_extent']:
   value=a.get_editor_property(k);row[k]=v(value) if isinstance(value,unreal.Vector) else str(value) if isinstance(value,(unreal.Name,unreal.Object)) else value
 if isinstance(a,unreal.HouseInteractiveProp):
  for k in ['prop_id','prop_name','mass_kg','break_health','wall_mounted']:
   row[k]=str(a.get_editor_property(k))
 for component in a.get_components_by_class(unreal.StaticMeshComponent):
  mesh=component.static_mesh
  if not mesh:continue
  row['components'].append(dict(mesh=mesh.get_path_name(),materials=[component.get_material(i).get_path_name() if component.get_material(i) else None for i in range(component.get_num_materials())],location=v(component.get_world_location()),scale=v(component.get_world_scale()),visible=component.visible,collision=str(component.get_collision_enabled())))
  if label.startswith(('SM_Entrance','COL_Entrance','SMF_Entrance_ALL')) and isinstance(a,unreal.StaticMeshActor):
   task=unreal.AssetExportTask();task.object=mesh;task.filename=str(R/'context/fbx'/(label+'.fbx'));task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.StaticMeshExporterFBX()
   if not unreal.Exporter.run_asset_export_task(task):raise RuntimeError('Export failed '+label)
   row['export']=task.filename
 rows.append(row)
(R/'context/actors.json').write_text(json.dumps(rows,indent=2));unreal.log('KITCHEN_INSPECTED '+str(len(rows)))
