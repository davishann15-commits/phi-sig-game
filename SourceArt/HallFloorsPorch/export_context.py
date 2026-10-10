import unreal,json,os,shutil,traceback
B=unreal.Paths.project_dir()+'SourceArt/HallFloorsPorch/'
for p in ['before','context/fbx','evidence']:os.makedirs(B+p,exist_ok=True)
try:
 w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 shutil.copy2(unreal.Paths.project_content_dir()+'Story/Maps/Chapter01_House.umap',B+'before/Chapter01_House.umap')
 def vec(v):return [v.x,v.y,v.z]
 rows=[];exports={}
 for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
  p=a.get_actor_location();s=a.get_actor_scale3d();r=a.get_actor_rotation();o,e=a.get_actor_bounds(False)
  row=dict(label=a.get_actor_label(),class_name=a.get_class().get_name(),location=vec(p),scale=vec(s),rotation=[r.pitch,r.yaw,r.roll],bounds=[vec(o-e),vec(o+e)],components=[])
  if row['class_name']=='ExistingHouseDoor':
   row['door']={}
   for k in ['door_id','original_leaf','original_glass','closed_yaw','open_yaw','local_leaf_space','doorway_center','panel_center_world','panel_half_extent']:
    v=a.get_editor_property(k);row['door'][k]=v.get_path_name() if isinstance(v,unreal.Object) else vec(v) if isinstance(v,unreal.Vector) else str(v)
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   m=c.static_mesh
   if not m:continue
   cr=dict(mesh=m.get_path_name(),name=c.get_name(),materials=[x.get_path_name()if x else None for x in c.get_materials()],location=vec(c.get_world_location()),scale=vec(c.get_world_scale()),visible=c.is_visible(),collision=str(c.get_collision_enabled()))
   row['components'].append(cr)
   architectural=row['class_name']=='StaticMeshActor' and (row['label'].startswith(('SM_Upper','SM_Entrance','COL_Upper','COL_Entrance')) or 'Hall' in m.get_name() or 'Bath' in m.get_name() or 'STAIR_' in m.get_name())
   selected_prop='Porch' in row['label'] or 'Porch' in m.get_name() or (row['class_name']=='ExistingHouseDoor' and abs(p.y+700)<200 and p.z>280)
   if (architectural or selected_prop) and m.get_path_name() not in exports:
    path=B+'context/fbx/'+m.get_name()+'.fbx';t=unreal.AssetExportTask();t.object=m;t.filename=path;t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX()
    if unreal.Exporter.run_asset_export_task(t):exports[m.get_path_name()]=path
  rows.append(row)
 json.dump(rows,open(B+'context/actors.json','w'),indent=2);json.dump(exports,open(B+'context/exports.json','w'),indent=2)
 unreal.log('HALL_PORCH_CONTEXT_PASS actors='+str(len(rows))+' exports='+str(len(exports)))
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
