import unreal,json,os,traceback,shutil
B='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/StairwellWall/'
try:
 w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 shutil.copy2(unreal.Paths.project_content_dir()+'Story/Maps/Chapter01_House.umap',B+'before/Chapter01_House.umap')
 rows=[]
 for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
  if not isinstance(a,unreal.StaticMeshActor):continue
  c=a.static_mesh_component;m=c.static_mesh
  if not m:continue
  o,e=a.get_actor_bounds(False)
  if o.x-e.x > -1250 or o.x+e.x < -2350 or o.y-e.y > 1320 or o.y+e.y < 890:continue
  p=a.get_actor_location();s=a.get_actor_scale3d();r=a.get_actor_rotation()
  row=dict(label=a.get_actor_label(),mesh=m.get_path_name(),position=[p.x,p.y,p.z],scale=[s.x,s.y,s.z],rotation=[r.pitch,r.yaw,r.roll],bounds=[[o.x-e.x,o.y-e.y,o.z-e.z],[o.x+e.x,o.y+e.y,o.z+e.z]],materials=[x.get_path_name()if x else None for x in c.get_materials()],visible=c.is_visible(),collision=str(c.get_collision_profile_name()))
  if 'Entrance_p0_m2' in m.get_name() or 'Upper_p0_m2' in m.get_name():
   t=unreal.AssetExportTask();t.object=m;t.filename=B+'exports/'+m.get_name()+'.fbx';t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX();assert unreal.Exporter.run_asset_export_task(t);row['export']=t.filename
  rows.append(row)
 json.dump(rows,open(B+'evidence/actors.json','w'),indent=2)
 unreal.log('STAIR_CONTEXT_PASS actors='+str(len(rows)))
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
