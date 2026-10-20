import unreal,json
from pathlib import Path
B=Path(__file__).resolve().parent
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def xyz(v):return [round(v.x,4),round(v.y,4),round(v.z,4)]
rows=[];exports=set()
for a in S.get_all_level_actors():
 label=a.get_actor_label();cls=a.get_class().get_name()
 if not (label.startswith(('SM_','COL_')) or cls in ('HouseReactiveFixture','ExistingHouseDoor','HouseInteractiveProp')):continue
 if cls=='HouseInteractiveProp':continue
 o,e=a.get_actor_bounds(False)
 row=dict(label=label,name=a.get_name(),cls=cls,bounds=[xyz(o-e),xyz(o+e)],components=[])
 if cls=='HouseReactiveFixture':
  row['fixture']={k:str(a.get_editor_property(k)) for k in ['fixture_kind','fixture_id','original_mesh','breakaway_frame_mesh','glass_breaking_damage']}
 if cls=='ExistingHouseDoor':
  row['door']={k:str(a.get_editor_property(k)) for k in ['door_id','door_group','doorway_center','closed_yaw','open_yaw','panel_center_world','panel_half_extent']}
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  m=c.static_mesh
  if not m:continue
  tr=c.get_world_transform();q=tr.rotation
  cr=dict(name=c.get_name(),mesh=m.get_path_name(),loc=xyz(tr.translation),quat=[q.x,q.y,q.z,q.w],scale=xyz(tr.scale3d),visible=c.is_visible(),collision=str(c.get_collision_enabled()),materials=[x.get_path_name() if x else None for x in c.get_materials()])
  if (label.startswith(('SM_','COL_')) and 'Solid' in label and m.get_path_name() not in exports):
   task=unreal.AssetExportTask();task.object=m;task.filename=str(B/'context/fbx'/(m.get_name()+'.fbx'));task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.StaticMeshExporterFBX()
   cr['exported']=Path(task.filename).exists() or unreal.Exporter.run_asset_export_task(task);exports.add(m.get_path_name())
  if cls=='HouseReactiveFixture' and 'WINDOW' in row['fixture']['fixture_kind'] and c.get_name()=='OriginalFixture':
   bb=m.get_bounds();pts=[]
   for x in [-1,1]:
    for y in [-1,1]:
     for z in [-1,1]:pts.append(xyz(unreal.MathLibrary.transform_location(tr,bb.origin+bb.box_extent*unreal.Vector(x,y,z))))
   cr['pane_bounds']=[[min(p[j] for p in pts) for j in range(3)],[max(p[j] for p in pts) for j in range(3)]]
  row['components'].append(cr)
 rows.append(row)
(B/'context/actors.json').write_text(json.dumps(rows,indent=2));unreal.log('CONNECTION_AUDIT_SAVED actors='+str(len(rows))+' exported='+str(len(exports)))
