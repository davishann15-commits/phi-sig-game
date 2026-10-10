import unreal, json
from pathlib import Path
ROOT = Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library')
(ROOT/'context/fbx').mkdir(parents=True, exist_ok=True)
world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
def v(p): return [p.x,p.y,p.z]
def r(p): return [p.pitch,p.yaw,p.roll]
records=[]
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    label=a.get_actor_label(); loc=a.get_actor_location()
    origin, extent=a.get_actor_bounds(False)
    if not ('Basement' in label or 'Library' in label or (-3150<loc.x<-2150 and -1550<loc.y<1300 and loc.z<0)):
        continue
    row=dict(label=label,class_name=a.get_class().get_name(),location=v(loc),rotation=r(a.get_actor_rotation()),scale=v(a.get_actor_scale3d()),bounds=[v(origin-extent),v(origin+extent)],components=[])
    if isinstance(a,unreal.ExistingHouseDoor):
        for key in ['door_id','doorway_center','closed_yaw','open_yaw','locked','original_leaf','panel_half_extent']:
            try:
                value=a.get_editor_property(key)
                row[key]=v(value) if isinstance(value,unreal.Vector) else str(value) if isinstance(value,(unreal.Name,unreal.Object)) else value
            except Exception: pass
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        m=c.static_mesh
        if not m: continue
        row['components'].append(dict(mesh=m.get_path_name(),name=c.get_name(),materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],location=v(c.get_world_location()),rotation=r(c.get_world_rotation()),scale=v(c.get_world_scale()),visible=c.visible,collision=str(c.get_collision_enabled())))
        if label in ['SM_Basement_m1_m2_Solid','SM_Basement_p0_m2_Solid','SM_Basement_m1_m3_Solid','COL_Basement_m1_m2_Solid','COL_Basement_p0_m2_Solid','COL_Basement_m1_m3_Solid']:
            task=unreal.AssetExportTask(); task.object=m; task.filename=str(ROOT/'context/fbx'/(label+'.fbx'));task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.StaticMeshExporterFBX()
            if not unreal.Exporter.run_asset_export_task(task): raise RuntimeError('Export failed '+label)
            row['export']=task.filename
    records.append(row)
(ROOT/'context/actors.json').write_text(json.dumps(records,indent=2))
unreal.log('BASEMENT_LIBRARY_INSPECTED '+str(len(records)))
