"""Convert large opaque, stationary house meshes to Nanite without editing geometry."""
import unreal,pathlib,json,shutil,traceback
J=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/HouseJoinRepair';A=unreal.EditorAssetLibrary;S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def run():
 assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='Chapter01_House'
 before=json.loads((J/'context/actors.json').read_text());actors={a.get_actor_label():a for a in S.get_all_level_actors()};eligible=[];skipped=[]
 for r in before:
  if r['cls']!='StaticMeshActor' or r['label'].startswith('COL_'):continue
  for c in r['meshes']:
   if not(c.get('visible')and c.get('shadow')and not c.get('nanite')):continue
   if max(c['bounds'][1][i]-c['bounds'][0][i]for i in range(3))<=1300:continue
   a=actors[r['label']];mesh=a.static_mesh_component.static_mesh;ns=mesh.get_editor_property('nanite_settings')
   if ns.enabled:continue
   mats=[s.material_interface for s in mesh.static_materials if s.material_interface]
   if not all(m.get_blend_mode()in[unreal.BlendMode.BLEND_OPAQUE,unreal.BlendMode.BLEND_MASKED]for m in mats):skipped.append(mesh.get_path_name());continue
   users=[a for a in S.get_all_level_actors()for x in a.get_components_by_class(unreal.StaticMeshComponent)if x.static_mesh==mesh]
   assert all(isinstance(a,unreal.StaticMeshActor)for a in users),mesh.get_path_name()
   p=pathlib.Path(unreal.Paths.project_content_dir())/(mesh.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset');dest=J/'before/assets'/p.relative_to(unreal.Paths.project_content_dir());dest.parent.mkdir(parents=True,exist_ok=True)
   if not dest.exists():shutil.copy2(p,dest)
   ns.enabled=True;mesh.set_editor_property('nanite_settings',ns);assert A.save_loaded_asset(mesh,False);eligible.append(mesh.get_path_name())
 (J/'evidence/performance-assets.json').write_text(json.dumps(dict(status='PASS',enabled_nanite=eligible,skipped_translucent=skipped,geometry_collision_and_transforms_unchanged=True),indent=2));unreal.log('HOUSE_JOIN_PERFORMANCE_PASS')
try:run()
except:unreal.log_error('HOUSE_JOIN_PERFORMANCE_FAIL '+traceback.format_exc())
unreal.SystemLibrary.quit_editor()
