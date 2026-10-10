import unreal,json,pathlib,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NoodleWing/context';(B/'fbx').mkdir(parents=True,exist_ok=True)
exec((B.parent/'capture_live.py').read_text().split('try:')[0],globals())
S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
try:
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='Chapter01_House'
 rows=[];done={}
 for a in S.get_all_level_actors():
  row=snap(a)
  if isinstance(a,unreal.ExistingHouseDoor):
   row['door']={k:str(a.get_editor_property(k)) for k in ['door_id','original_leaf','original_glass','doorway_center','closed_yaw','open_yaw','locked','panel_center_world','panel_half_extent','local_leaf_space']}
  lo,hi=row['bounds'];struct=any(x in row['label'] for x in ['COL_','OriginalRoom','ArchitecturalRemainder','SM_Entrance','SM_Basement'])
  if hi[0]>1200 and lo[0]<4700 and hi[2]>-450 and lo[2]<850 and (struct or max(hi[i]-lo[i] for i in range(3))>250):
   for c,m in zip([c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh],row['meshes']):
    path=m['asset']
    if path not in done:
     dst=B/'fbx'/(c.static_mesh.get_name()+'.fbx');t=unreal.AssetExportTask();t.object=c.static_mesh;t.filename=str(dst);t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX()
     assert unreal.Exporter.run_asset_export_task(t),path;done[path]=str(dst)
    m['fbx']=done[path]
  rows.append(row)
 (B/'saved_actors.json').write_text(json.dumps(rows,indent=2));unreal.log('NOODLE_CONTEXT_PASS actors=%d exports=%d'%(len(rows),len(done)))
except:unreal.log_error('NOODLE_CONTEXT_FAIL '+traceback.format_exc())
