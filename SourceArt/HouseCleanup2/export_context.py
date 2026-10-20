import unreal,json,pathlib,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/HouseCleanup2'
S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
exec((B.parent/'NightClub/expansion2_layout/install_expansion2.py').read_text().split('\ndef run():')[0],globals())
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/HouseCleanup2'
def bounds(c):
 m=c.static_mesh; bb=m.get_bounds(); tr=c.get_world_transform(); pp=[unreal.MathLibrary.transform_location(tr,bb.origin+bb.box_extent*unreal.Vector(x,y,z)) for x in [-1,1] for y in [-1,1] for z in [-1,1]]
 return [[min(getattr(p,k) for p in pp) for k in ['x','y','z']],[max(getattr(p,k) for p in pp) for k in ['x','y','z']]]
try:
 w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert w.get_name()=='Chapter01_House'
 allrows=[];exports={}
 for a in S.get_all_level_actors():
  r=snapshot(a);o,e=a.get_actor_bounds(False);r['bounds']=[xyz(o-e),xyz(o+e)]
  for c,m in zip([c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh],r['meshes']):
   tr=c.get_world_transform();m.update(loc=xyz(tr.translation),quat=[tr.rotation.x,tr.rotation.y,tr.rotation.z,tr.rotation.w],scale=xyz(tr.scale3d),bounds=bounds(c),shadow=c.cast_shadow,nanite=c.static_mesh.get_editor_property('nanite_settings').enabled)
   target=(a.get_actor_label().startswith(('SM_','COL_')) and any(k in a.get_actor_label() for k in ['Basement','Entrance','Upper_p0_m2'])) or 'SM_HFP_' in c.static_mesh.get_name() or a.get_actor_label().startswith('OriginalPorchDoor') or a.get_actor_label() in ['SM_Upper_p0_m1_Solid','SM_Upper_p0_p0_Solid','Library_ServiceAnnexShell_001','Library_MechanicalServiceDoor','HouseJoin_GalleryBannerBrackets','OriginalDoor_Basement_p0_m2_Solid_01']
   if target:
    path=B/'context/fbx'/(c.static_mesh.get_name()+'.fbx');m['fbx']=str(path)
    if m['mesh'] not in exports:
     t=unreal.AssetExportTask();t.object=c.static_mesh;t.filename=str(path);t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX();assert unreal.Exporter.run_asset_export_task(t);exports[m['mesh']]=str(path)
  for c,l in zip(a.get_components_by_class(unreal.LightComponent),r['lights']):
   l.update(shadow=c.cast_shadows,loc=xyz(c.get_world_location()),type=c.get_class().get_name())
  allrows.append(r)
 (B/'context/actors.json').write_text(json.dumps(allrows,indent=2));unreal.log('HOUSE_JOIN_CONTEXT_PASS '+str(len(allrows)))
except:unreal.log_error('HOUSE_JOIN_CONTEXT_FAIL '+traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
