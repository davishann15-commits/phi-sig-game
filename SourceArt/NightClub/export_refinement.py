import unreal,json,pathlib,traceback,shutil
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
O=B/'refinement_context';(O/'fbx').mkdir(parents=True,exist_ok=True)
def xyz(v):return[v.x,v.y,v.z]
try:
 src=pathlib.Path(unreal.Paths.project_content_dir())/'Story/Maps/Chapter01_House.umap'
 shutil.copy2(src,O/'Chapter01_House_before_refinement.umap')
 unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);rows=[];exported=[]
 for a in S.get_all_level_actors():
  r=a.get_actor_rotation();o,e=a.get_actor_bounds(False)
  row=dict(name=a.get_name(),label=a.get_actor_label(),cls=a.get_class().get_name(),location=xyz(a.get_actor_location()),rotation=[r.pitch,r.yaw,r.roll],scale=xyz(a.get_actor_scale3d()),bounds=[xyz(o-e),xyz(o+e)],meshes=[],lights=[])
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   if c.static_mesh:
    row['meshes'].append(dict(component=c.get_name(),mesh=c.static_mesh.get_path_name(),materials=[c.get_material(i).get_path_name()if c.get_material(i)else None for i in range(c.get_num_materials())],collision=str(c.get_collision_enabled()),visible=c.is_visible()))
    if a.get_actor_label() in ['SM_Upper_m1_m1_Solid','COL_Upper_m1_m1_Solid','SM_Entrance_m1_m1_Solid','COL_Entrance_m1_m1_Solid','SM_Upper_m1_m2_Solid','COL_Upper_m1_m2_Solid']:
     t=unreal.AssetExportTask();t.object=c.static_mesh;t.filename=str(O/'fbx'/(a.get_actor_label()+'.fbx'));t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX();assert unreal.Exporter.run_asset_export_task(t);exported.append(a.get_actor_label())
  for c in a.get_components_by_class(unreal.LightComponent):row['lights'].append(dict(component=c.get_name(),intensity=c.intensity))
  rows.append(row)
 json.dump(rows,open(O/'actors.json','w'),indent=2)
 unreal.log('CLUB_REFINEMENT_CONTEXT_PASS actors='+str(len(rows))+' exported='+str(exported))
except:unreal.log_error('CLUB_REFINEMENT_CONTEXT_FAIL '+traceback.format_exc())
