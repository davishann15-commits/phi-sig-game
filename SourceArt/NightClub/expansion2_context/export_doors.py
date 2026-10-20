import unreal,json,pathlib,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub/expansion2_doors';(B/'fbx').mkdir(parents=True,exist_ok=True)
def xyz(v):return [v.x,v.y,v.z]
def rot(r):return [r.pitch,r.yaw,r.roll]
try:
 S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);rows=[]
 for a in S.get_all_level_actors():
  if a.get_class().get_name()!='ExistingHouseDoor':continue
  o,e=a.get_actor_bounds(False);lo=o-e;hi=o+e
  if hi.x < -1490 or lo.x > -838 or hi.y < -1521 or lo.y > -1008 or hi.z < -10 or lo.z >640.3:continue
  row=dict(name=a.get_name(),label=a.get_actor_label(),location=xyz(a.get_actor_location()),rotation=rot(a.get_actor_rotation()),scale=xyz(a.get_actor_scale3d()),bounds=[xyz(lo),xyz(hi)],meshes=[],boxes=[])
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   if not c.static_mesh:continue
   transform=c.get_world_transform();r=transform.rotation.rotator()
   path=B/'fbx'/(a.get_name()+'_'+c.get_name()+'.fbx')
   t=unreal.AssetExportTask();t.object=c.static_mesh;t.filename=str(path);t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX()
   assert unreal.Exporter.run_asset_export_task(t)
   row['meshes'].append(dict(component=c.get_name(),asset=c.static_mesh.get_path_name(),location=xyz(transform.translation),rotation=rot(r),scale=xyz(transform.scale3d),fbx=str(path),collision=str(c.get_collision_enabled())))
  for c in a.get_components_by_class(unreal.BoxComponent):
   transform=c.get_world_transform()
   row['boxes'].append(dict(component=c.get_name(),location=xyz(transform.translation),rotation=rot(transform.rotation.rotator()),scale=xyz(transform.scale3d),extent=xyz(c.get_unscaled_box_extent()),collision=str(c.get_collision_enabled())))
  for k in ['closed_yaw','open_yaw','doorway_center','panel_center_world','panel_half_extent','local_leaf_space']:
   v=a.get_editor_property(k);row[k]=xyz(v) if isinstance(v,unreal.Vector) else v
  rows.append(row)
 (B/'actors.json').write_text(json.dumps(rows,indent=2));unreal.log('EXPANSION2_DOORS_PASS '+str(len(rows)))
except:unreal.log_error('EXPANSION2_DOORS_FAIL '+traceback.format_exc())
