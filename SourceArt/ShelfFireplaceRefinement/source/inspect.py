import unreal,json,traceback
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ShelfFireplaceRefinement')
try:
 w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House');rows=[]
 for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
  c,e=a.get_actor_bounds(False);label=a.get_actor_label()
  if not(label.startswith('SM_Entrance') or label.startswith('SM_Upper') or 'Shelf'in label or 'Bookcase'in label):continue
  p=a.get_actor_location();s=a.get_actor_scale3d();rot=a.get_actor_rotation()
  row=dict(label=label,location=[p.x,p.y,p.z],scale=[s.x,s.y,s.z],rotation=[rot.pitch,rot.yaw,rot.roll],bounds=[[c.x-e.x,c.y-e.y,c.z-e.z],[c.x+e.x,c.y+e.y,c.z+e.z]],components=[])
  for comp in a.get_components_by_class(unreal.StaticMeshComponent):
   mesh=comp.static_mesh
   if not mesh:continue
   d=dict(mesh=mesh.get_path_name(),materials=[comp.get_material(i).get_path_name()if comp.get_material(i) else None for i in range(comp.get_num_materials())]);row['components'].append(d)
   if isinstance(a,unreal.StaticMeshActor):
    t=unreal.AssetExportTask();t.object=mesh;t.filename=str(R/'exports'/(label+'.fbx'));t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX();assert unreal.Exporter.run_asset_export_task(t)
  rows.append(row)
 (R/'inventory.json').write_text(json.dumps(rows,indent=2))
except Exception:(R/'inspect-error.txt').write_text(traceback.format_exc())
ticks=0
def close(dt):
 global ticks
 ticks+=1
 if ticks>5:unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(close)
