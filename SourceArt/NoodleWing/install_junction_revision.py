import pathlib,unreal,json,traceback,re
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NoodleWing'
exec((B/'install.py').read_text().split('def run():')[0],globals())
def revise():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='Chapter01_House'
 art=json.loads((B/'art/junction_revision.json').read_text())
 before={a.get_name():snap(a)for a in S.get_all_level_actors()};changed=set()
 for r in art['assets']:
  me=imp(r['fbx'],r['name']);slots=list(me.static_materials)
  for sl in slots:
   key=re.sub(r'[._]\d+$','',str(sl.get_editor_property('imported_material_slot_name')));m=A.load_asset(D+'/'+key);assert m,key;sl.material_interface=m
  me.static_materials=slots;setupcol(me,r['kind']);assert A.save_loaded_asset(me,False)
  actor=next(a for a in S.get_all_level_actors()if a.get_actor_label()==r['name'].replace('SM_',''))
  actor.static_mesh_component.set_static_mesh(me);actor.static_mesh_component.set_editor_property('override_materials',[])
  assert all(abs(v-1)<.001 for v in xyz(actor.get_actor_scale3d()))
  changed.add(actor.get_name())
 after={a.get_name():snap(a)for a in S.get_all_level_actors()}
 drift=[r['label']for n,r in before.items()if n not in changed and after.get(n)!=r];assert not drift,drift
 assert len(before)==len(after)
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 report=dict(status='PASS',revised_meshes=[r['name']for r in art['assets']],protected_actors=len(before)-len(changed),changed_labels=[before[n]['label']for n in changed],actor_count=len(after),new_lights=0,red_entry=[2458,-917,-361.966],blue_entry=[2458,-370,-361.966])
 (B/'evidence/junction-install-report.json').write_text(json.dumps(report,indent=2));unreal.log('NOODLE_JUNCTION_INSTALL_PASS '+json.dumps(report))
try:revise()
except:unreal.log_error('NOODLE_JUNCTION_INSTALL_FAIL '+traceback.format_exc())
