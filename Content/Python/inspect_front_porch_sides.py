import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()) / 'SourceArt/FrontPorchOpenSides'
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
boxes = [(unreal.Vector(-850,-1500,307),unreal.Vector(-745,-1000,635)),
         (unreal.Vector(710,-1500,307),unreal.Vector(810,-1000,635))]
rows=[]
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh=comp.static_mesh
        if not mesh: continue
        loc=comp.get_world_location(); scale=comp.get_world_scale(); rot=comp.get_world_rotation()
        transform=comp.get_world_transform()
        counts=[]
        for lo,hi in boxes:
            corners=[transform.inverse_transform_location(unreal.Vector(x,y,z))
                     for x in (lo.x,hi.x) for y in (lo.y,hi.y) for z in (lo.z,hi.z)]
            a=unreal.Vector(*(min(getattr(p,k) for p in corners) for k in ('x','y','z')))
            b=unreal.Vector(*(max(getattr(p,k) for p in corners) for k in ('x','y','z')))
            count,report=unreal.HouseCoplanarFixLibrary.cut_triangles_in_box(mesh,a,b,unreal.Vector(1,1,1),False,False)
            counts.append(count)
        if sum(counts):
            task=unreal.AssetExportTask();task.object=mesh;task.filename=str(root/(mesh.get_name()+'.fbx'))
            task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.StaticMeshExporterFBX()
            assert unreal.Exporter.run_asset_export_task(task)
            rows.append(dict(actor=actor.get_actor_label(),asset=mesh.get_path_name(),location=[loc.x,loc.y,loc.z],scale=[scale.x,scale.y,scale.z],rotation=[rot.pitch,rot.yaw,rot.roll],cuts=counts))
(root/'candidates.json').write_text(json.dumps(rows,indent=2))
unreal.log('PORCH_SIDES_CANDIDATES '+json.dumps(rows))
