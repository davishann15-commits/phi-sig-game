"""Locate pawn blockers in clear great-room space, without editing assets."""
import unreal, json
from pathlib import Path
BASE = Path(__file__).resolve().parent
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
mesh_edit = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
types = [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1, unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2]
rows = []
for x in range(-1100, 1201, 100):
    for y in range(-500, 701, 100):
        center = unreal.Vector(x, y, 90)
        hits = unreal.SystemLibrary.capsule_trace_multi_for_objects(
            world, center, center + unreal.Vector(0.1, 0, 0), 28, 82, types,
            False, [], unreal.DrawDebugTrace.NONE, True)
        labels = []
        for hit in hits or []:
            vals = hit.to_tuple()
            actor = vals[9]
            if actor:
                labels.append(actor.get_actor_label())
        if labels:
            rows.append({'x': x, 'y': y, 'blockers': sorted(set(labels))})
(BASE/'probe.json').write_text(json.dumps(rows, indent=2))
exports = BASE/'meshes'
exports.mkdir(exist_ok=True)
inventory = []
for actor in actors:
    label = actor.get_actor_label()
    if label not in ('COL_Entrance_m1_p0_Solid', 'COL_Entrance_p0_m1_Solid',
                     'COL_Entrance_p0_p0_Solid', 'SM_Upper_p0_m1_Solid',
                     'SM_Upper_p0_p0_Solid', 'COL_Upper_p0_m1_Solid',
                     'COL_Upper_p0_p0_Solid'):
        continue
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    mesh = component.static_mesh
    item = {'label': label, 'mesh': mesh.get_path_name()}
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = str(exports/(label+'.fbx'))
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.StaticMeshExporterFBX()
    item['exported'] = unreal.Exporter.run_asset_export_task(task)
    inventory.append(item)
(BASE/'mesh-collision.json').write_text(json.dumps(inventory, indent=2))
unreal.log('GREAT_ROOM_PROBE_SAVED samples=' + str(len(rows)))
