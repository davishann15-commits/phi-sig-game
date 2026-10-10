import unreal,json
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs/placement-fix')
R.mkdir(parents=True,exist_ok=True)
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert w.get_name()=='Chapter01_House'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();bylabel={a.get_actor_label():a for a in actors}
rows=[]
for label in ['SM_Entrance_m1_p0_Solid','SM_Upper_p0_m1_Solid','SM_Upper_p0_p0_Solid']:
 a=bylabel[label];mesh=a.static_mesh_component.static_mesh
 t=unreal.AssetExportTask();t.object=mesh;t.exporter=unreal.StaticMeshExporterFBX();t.filename=str(R/(label+'.fbx'));t.automated=True;t.prompt=False;t.replace_identical=True
 assert unreal.Exporter.run_asset_export_task(t)
 rows.append(dict(label=label,mesh=mesh.get_path_name(),scale=[a.get_actor_scale3d().x,a.get_actor_scale3d().y,a.get_actor_scale3d().z]))
(R/'front-walls.json').write_text(json.dumps(rows,indent=2));unreal.log('TV_FRONT_WALL_GEOMETRY_READY')
