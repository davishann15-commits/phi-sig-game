"""Read-only inventory of the front portico's large glass entrance."""
import unreal, json
from pathlib import Path
B=Path(__file__).resolve().parent
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def xyz(v): return [v.x,v.y,v.z]
rows=[]
for a in S.get_all_level_actors():
    o,e=a.get_actor_bounds(False)
    lo,hi=o-e,o+e
    if hi.x < -230 or lo.x > 180 or hi.y < -1060 or lo.y > -870 or hi.z < -10 or lo.z > 300: continue
    row=dict(label=a.get_actor_label(),name=a.get_name(),cls=a.get_class().get_name(),bounds=[xyz(lo),xyz(hi)],components=[])
    if isinstance(a,unreal.ExistingHouseDoor):
        row['door']={k:str(a.get_editor_property(k)) for k in ['door_id','door_group','doorway_center','local_leaf_space','closed_yaw','open_yaw','panel_center_world','panel_half_extent']}
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        m=c.static_mesh
        if not m: continue
        tr=c.get_world_transform();q=tr.rotation
        cr=dict(name=c.get_name(),mesh=m.get_path_name(),loc=xyz(tr.translation),quat=[q.x,q.y,q.z,q.w],scale=xyz(tr.scale3d),visible=c.is_visible(),collision=str(c.get_collision_enabled()),materials=[x.get_path_name() if x else None for x in c.get_materials()],near_vertices=0,sections=[])
        for i in range(m.get_num_sections(0)):
            vs,inds,*_=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(m,0,i)
            near=[]
            for v in vs:
                p=unreal.MathLibrary.transform_location(tr,v)
                if -230<=p.x<=180 and -1060<=p.y<=-870 and -10<=p.z<=300:near.append(xyz(p))
            cr['near_vertices']+=len(near)
            if near: cr['sections'].append(dict(section=i,n=len(near),lo=[min(v[j] for v in near) for j in range(3)],hi=[max(v[j] for v in near) for j in range(3)]))
        if cr['near_vertices'] and c.is_visible():
            task=unreal.AssetExportTask();task.object=m;task.filename=str(B/'meshes'/(m.get_name()+'.fbx'));task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.StaticMeshExporterFBX();cr['exported']=unreal.Exporter.run_asset_export_task(task)
        row['components'].append(cr)
    rows.append(row)
(B/'audit.json').write_text(json.dumps(rows,indent=2))
unreal.log('FRONT_DOOR_AUDIT_SAVED actors='+str(len(rows)))
