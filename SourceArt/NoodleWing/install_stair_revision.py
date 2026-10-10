import pathlib,unreal,json,traceback,re
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NoodleWing'
exec((B/'install.py').read_text().split('def run():')[0],globals())
def revise():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='Chapter01_House'
 art=json.loads((B/'art/stair_revision.json').read_text());before={a.get_name():snap(a)for a in S.get_all_level_actors()};changed=set()
 for r in art['assets']:
  me=imp(r['fbx'],r['name']);slots=list(me.static_materials)
  for sl in slots:
   key=re.sub(r'[._]\d+$','',str(sl.get_editor_property('imported_material_slot_name')));m=A.load_asset(r.get('material_overrides',{}).get(key,D+'/'+key));assert m,key;sl.material_interface=m
  me.static_materials=slots;setupcol(me,r['kind']);assert A.save_loaded_asset(me,False)
  actor=next(a for a in S.get_all_level_actors()if a.get_actor_label()==r['name'].replace('SM_',''))
  actor.static_mesh_component.set_static_mesh(me);actor.static_mesh_component.set_editor_property('override_materials',[])
  assert all(abs(v-1)<.001 for v in xyz(actor.get_actor_scale3d()))
  if r['name']=='SM_NW_StairStructure':assert 'NoodleWingStairwell' in [str(t)for t in actor.tags]
  changed.add(actor.get_name())
 # Replace only the four named stair practical lights, keeping the fixture budget.
 for a in list(S.get_all_level_actors()):
  if a.get_actor_label().startswith('NW_StairLamp_'):changed.add(a.get_name());S.destroy_actor(a)
 for r in art['lights']:
  a=S.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*r['pos']));a.set_actor_label(r['label']);a.set_folder_path('Noodle Wing/Lighting');c=a.point_light_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_intensity(r['power']);c.set_attenuation_radius(r['radius']);c.set_temperature(r['temperature']);c.set_use_temperature(True);c.set_source_radius(12);c.set_soft_source_radius(15);c.set_cast_shadows(True);c.set_editor_property('max_draw_distance',1800);c.set_editor_property('max_distance_fade_range',250)
 after={a.get_name():snap(a)for a in S.get_all_level_actors()};drift=[r['label']for n,r in before.items()if n not in changed and after.get(n)!=r];assert not drift,drift
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 p=unreal.Vector(1940,-563,145);q=unreal.Vector(1720,260,90);rot=unreal.MathLibrary.find_look_at_rotation(p,q);unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(p,rot)
 report=dict(status='PASS',revised_meshes=[r['name']for r in art['assets']],protected_actors=len(before)-len(changed),changed_labels=[before[n]['label']for n in changed],actor_count=len(after));(B/'evidence/stair-install-report.json').write_text(json.dumps(report,indent=2));unreal.log('NOODLE_STAIR_INSTALL_PASS '+json.dumps(report))
try:revise()
except:unreal.log_error('NOODLE_STAIR_INSTALL_FAIL '+traceback.format_exc())
