import unreal,json
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs');assets=unreal.EditorAssetLibrary
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
rows=[]
def vec(v):return [v.x,v.y,v.z]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 c,e=a.get_actor_bounds(False)
 if c.z>310 or c.z<-50 or c.y<-350 or c.x<-1650 or c.x>1300:continue
 row=dict(label=a.get_actor_label(),class_name=a.get_class().get_name(),location=vec(a.get_actor_location()),scale=vec(a.get_actor_scale3d()),rotation=[a.get_actor_rotation().pitch,a.get_actor_rotation().yaw,a.get_actor_rotation().roll],bounds=[vec(c-e),vec(c+e)],components=[])
 for comp in a.get_components_by_class(unreal.StaticMeshComponent):
  mesh=comp.static_mesh
  if not mesh:continue
  b=dict(mesh=mesh.get_path_name(),materials=[comp.get_material(i).get_path_name()if comp.get_material(i) else None for i in range(comp.get_num_materials())]);row['components'].append(b)
  if isinstance(a,unreal.StaticMeshActor) and a.get_actor_label().startswith('SM_Entrance_p0'):
   t=unreal.AssetExportTask();t.object=mesh;t.filename=str(R/'context/fbx'/(a.get_actor_label()+'.fbx'));t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX();assert unreal.Exporter.run_asset_export_task(t);b['export']=t.filename
 rows.append(row)
(R/'context/actors.json').write_text(json.dumps(rows,indent=2));unreal.log('TV_INSPECTION_DONE '+str(len(rows)))
