"""Read-only inventory and FBX export of possible house-name lettering."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()) / 'SourceArt/HouseLetterRemoval'
(root / 'source').mkdir(parents=True, exist_ok=True)
(root / 'evidence').mkdir(parents=True, exist_ok=True)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors = unreal.EditorLevelLibrary.get_all_level_actors()
meshes = {}
rows = []
texts = []
inventory=[]

def relevant(name):
    return any(token in name.lower() for token in ('identity', 'phisig', 'phi_sig', 'sigma', 'kappa', 'crest'))

for actor in actors:
    for component in actor.get_components_by_class(unreal.TextRenderComponent):
        value = str(component.get_editor_property('text'))
        texts.append(dict(actor=actor.get_actor_label(), text=value))
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.static_mesh
        if not mesh:
            continue
        names = [component.get_material(i).get_path_name() if component.get_material(i) else ''
                 for i in range(component.get_num_materials())]
        inventory.append(dict(actor=actor.get_actor_label(),mesh=mesh.get_path_name(),materials=names))
        if not relevant(actor.get_actor_label()) and not relevant(mesh.get_name()) and not any(relevant(n) for n in names):
            continue
        p = component.get_world_location()
        scale = component.get_world_scale()
        row = dict(actor=actor.get_actor_label(), component=component.get_name(),
                   mesh=mesh.get_path_name(), materials=names,
                   location=[p.x,p.y,p.z], scale=[scale.x,scale.y,scale.z],
                   hidden=actor.get_editor_property('hidden'))
        rows.append(row)
        meshes[mesh.get_path_name()] = mesh
        for field in ('intact_mesh', 'damaged_mesh', 'heavily_damaged_mesh', 'broken_mesh'):
            try:
                stage = actor.get_editor_property(field)
                if stage:
                    meshes[stage.get_path_name()] = stage
                    row[field] = stage.get_path_name()
            except Exception:
                pass

exports=[]
for path, mesh in sorted(meshes.items()):
    slots=mesh.get_editor_property('static_materials')
    mats=[dict(slot=str(s.material_slot_name),
               material=s.material_interface.get_path_name() if s.material_interface else '')
          for s in slots]
    output=root/'source'/(mesh.get_name()+'.fbx')
    task=unreal.AssetExportTask()
    task.object=mesh;task.filename=str(output);task.automated=True
    task.prompt=False;task.replace_identical=True;task.exporter=unreal.StaticMeshExporterFBX()
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError('Failed to export '+path)
    exports.append(dict(asset=path, file=str(output), slots=mats))
    unreal.log('LETTER_AUDIT_MESH '+mesh.get_name())

report=dict(actor_count=len(actors), candidates=rows, meshes=exports, texts=texts)
(root/'evidence/all-meshes.json').write_text(json.dumps(inventory,indent=2))
(root/'evidence/audit.json').write_text(json.dumps(report,indent=2))
unreal.log('LETTER_AUDIT_COMPLETE actors={} candidates={} meshes={} text_components={}'.format(
    len(actors),len(rows),len(exports),len(texts)))
