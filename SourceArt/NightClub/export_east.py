import unreal,json,pathlib,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
def xyz(v):return [v.x,v.y,v.z]
try:
 unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);rows=[]
 for a in S.get_all_level_actors():
  r=a.get_actor_rotation();o,e=a.get_actor_bounds(False)
  row=dict(name=a.get_name(),label=a.get_actor_label(),cls=a.get_class().get_name(),location=xyz(a.get_actor_location()),rotation=[r.pitch,r.yaw,r.roll],scale=xyz(a.get_actor_scale3d()),bounds=[xyz(o-e),xyz(o+e)],meshes=[],lights=[])
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   if c.static_mesh:
    row['meshes'].append(dict(component=c.get_name(),mesh=c.static_mesh.get_path_name(),materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],collision=str(c.get_collision_enabled()),visible=c.is_visible()))
    if a.get_actor_label() in ['HallEnd_SM_HallEnd_ThreeRoomsFloors','HallEnd_SM_HallEnd_ThreeRoomsShell','HallEnd_COL_HallEnd_ThreeRoomsShell']:
     t=unreal.AssetExportTask();t.object=c.static_mesh;t.filename=str(B/'context/fbx'/(a.get_actor_label()+'.fbx'));t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX();assert unreal.Exporter.run_asset_export_task(t)
  for c in a.get_components_by_class(unreal.LightComponent):
   row['lights'].append(dict(component=c.get_name(),intensity=c.intensity,color=str(c.light_color)))
  rows.append(row)
 json.dump(rows,open(B/'context/actors-export2.json','w'),indent=2)
 unreal.log('NIGHTCLUB_INSPECT_PASS actors='+str(len(rows)))
except:unreal.log_error('NIGHTCLUB_INSPECT_FAIL '+traceback.format_exc())
