import pathlib,json,re,traceback,unreal
R=pathlib.Path(unreal.Paths.project_dir());B=R/'SourceArt/NightClub'
exec((B/'expansion2_layout/install_expansion2.py').read_text().split('\ndef run():')[0],globals())
J=R/'SourceArt/HouseCleanup2';D='/Game/SeniorSendOff/House/HouseCleanup2'
def run():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='Chapter01_House'
 actors={a.get_actor_label():a for a in S.get_all_level_actors()};before={a.get_name():snapshot(a)for a in S.get_all_level_actors()};changed=set();added=[]
 if not (J/'before/actors.json').exists():(J/'before/actors.json').write_text(json.dumps(list(before.values()),indent=2))
 art=json.loads((J/'geometry/manifest.json').read_text());mats={k:A.load_asset(v['reuse_asset'])for k,v in art['materials'].items()};assert all(mats.values())
 for row in art['assets']:
  mesh=imp(row['fbx'],row['name']);slots=list(mesh.static_materials)
  for slot in slots:
   raw=str(slot.get_editor_property('imported_material_slot_name'));key=next((k for k in mats if k==raw or k.replace('.','_')==raw),None)
   if key is None:key=re.sub(r'[.]\d+$','',raw)
   assert key in mats,(row['name'],raw);slot.material_interface=mats[key]
  mesh.static_materials=slots;collision(mesh,complex=row['collision']);settings=mesh.get_editor_property('nanite_settings');settings.enabled=False;mesh.set_editor_property('nanite_settings',settings);assert A.save_loaded_asset(mesh,False)
  if row['target_actor']:
   a=actors[row['target_actor']];changed.add(a.get_name());a.static_mesh_component.set_editor_property('override_materials',[]);a.static_mesh_component.set_static_mesh(mesh)
   a.set_actor_location(unreal.Vector(),False,True);a.set_actor_rotation(unreal.Rotator(),False);a.set_actor_scale3d(unreal.Vector(1,1,1));a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
  else:
   label='HouseCleanup_BasementStairConnection'
   if label in actors:a=actors[label];changed.add(a.get_name());a.static_mesh_component.set_static_mesh(mesh)
   else:a=static(label,mesh,block=True);added.append(label)
   a.set_folder_path('HouseRepairs/Joins');a.static_mesh_component.set_cast_shadow(False)
 after={a.get_name():snapshot(a)for a in S.get_all_level_actors()}
 assert all(after[n]==r for n,r in before.items()if n not in changed)
 assert all(n in after for n in before),'No existing room or actor may be deleted'
 assert len(after)==len(before)+len(added)
 assert all(after[n]==r for n,r in before.items()if r['label'].startswith(('NightClub_','BWR_'))or r['cls']in ['ExistingHouseDoor','HouseClubPole'])
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 report=dict(status='PASS',changed=[before[n]['label']for n in sorted(changed)],added=added,deleted=[],tiny_door='Only the baked 90 cm utility leaf and casing were removed from the wall mesh',protected_actor_count=len(before)-len(changed),nightclub_doors_pole_rooms_preserved=True)
 (J/'evidence/install-report.json').write_text(json.dumps(report,indent=2));(J/'evidence/actors_after.json').write_text(json.dumps(list(after.values()),indent=2));unreal.log('HOUSE_CLEANUP_INSTALL_PASS')
try:run()
except:unreal.log_error('HOUSE_CLEANUP_INSTALL_FAIL '+traceback.format_exc())
unreal.SystemLibrary.quit_editor()
