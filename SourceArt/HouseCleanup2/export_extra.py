import unreal,json,pathlib,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/HouseCleanup2'
try:
 rows=json.loads((B/'context/actors.json').read_text());S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors={a.get_actor_label():a for a in S.get_all_level_actors()}
 for row in rows:
  if row['label']!='SM_Upper_p0_p1_Solid':continue
  c=actors[row['label']].static_mesh_component;p=(B/'context/fbx'/(c.static_mesh.get_name()+'.fbx')).resolve();t=unreal.AssetExportTask();t.object=c.static_mesh;t.filename=str(p);t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX();assert unreal.Exporter.run_asset_export_task(t);row['meshes'][0]['fbx']=str(p)
 (B/'context/actors.json').write_text(json.dumps(rows,indent=2));unreal.log('CLEANUP_EXTRA_PASS')
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
