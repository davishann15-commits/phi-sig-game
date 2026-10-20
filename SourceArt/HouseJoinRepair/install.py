"""Install only the measured stair joins and two supported gallery banners."""
import pathlib,json,re,traceback,unreal
R=pathlib.Path(unreal.Paths.project_dir());B=R/'SourceArt/NightClub'
exec((B/'expansion2_layout/install_expansion2.py').read_text().split('\ndef run():')[0],globals())
J=R/'SourceArt/HouseJoinRepair';D='/Game/SeniorSendOff/House/HouseJoinRepairs'
def run():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='Chapter01_House'
 actors={a.get_actor_label():a for a in S.get_all_level_actors()};before={a.get_name():snapshot(a)for a in S.get_all_level_actors()};changed=set();added=[]
 art=json.loads((J/'geometry/manifest.json').read_text());mats={k:A.load_asset(v['reuse_asset'])for k,v in art['materials'].items()};assert all(mats.values())
 def touch(a):changed.add(a.get_name());return a
 for row in art['assets']:
  mesh=imp(row['fbx'],row['name']);slots=list(mesh.static_materials)
  for slot in slots:
   raw=str(slot.get_editor_property('imported_material_slot_name'));key=next((k for k in mats if k==raw or k.replace('.','_')==raw),None)
   if key is None:key=re.sub(r'[.]\d+$','',raw)
   assert key in mats,(row['name'],raw);slot.material_interface=mats[key]
  mesh.static_materials=slots;collision(mesh,complex=row['collision'])
  settings=mesh.get_editor_property('nanite_settings');settings.enabled=True;mesh.set_editor_property('nanite_settings',settings)
  assert A.save_loaded_asset(mesh,False)
  if row['target_actor']:
   a=touch(actors[row['target_actor']]);a.static_mesh_component.set_editor_property('override_materials',[]);a.static_mesh_component.set_static_mesh(mesh)
   a.set_actor_location(unreal.Vector(),False,True);a.set_actor_rotation(unreal.Rotator(),False);a.set_actor_scale3d(unreal.Vector(1,1,1))
  else:
   label='HouseJoin_'+row['name'].removeprefix('SM_HJ_')
   if label in actors:a=touch(actors[label]);a.static_mesh_component.set_static_mesh(mesh)
   else:a=static(label,mesh,block=row['collision']);added.append(label)
   a.set_folder_path('HouseRepairs/Joins');a.static_mesh_component.set_cast_shadow(False)
 for n in range(8):
  a=touch(actors['Stairwall flush collision '+str(n)]);p=a.get_actor_location();assert abs(p.y-954.47)<.1 or abs(p.y-917.85)<.1,(n,p);p.y=917.85;a.set_actor_location(p,False,True)
 for label,delta in [('Interactive_SMF_Upper_ALL__038',(0,-33,10)),('Interactive_SMF_Upper_ALL__039',(0,-33,0))]:
  a=touch(actors[label]);base=next(r['location']for r in json.loads((J/'context/actors.json').read_text())if r['label']==label);a.set_actor_location(unreal.Vector(*base)+unreal.Vector(*delta),False,True)
 after={a.get_name():snapshot(a)for a in S.get_all_level_actors()};assert all(after[n]==r for n,r in before.items()if n not in changed)
 assert all(n in after for n in before),'An existing actor was deleted'
 assert len(after)==len(before)+len(added)
 assert all(after[n]==r for n,r in before.items()if r['label'].startswith(('NightClub_','BWR_'))or r['cls']in ['ExistingHouseDoor','HouseClubPole'])
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 report=dict(status='PASS',changed=[before[n]['label']for n in sorted(changed)],added=added,deleted=[],protected_actor_count=len(before)-len(changed),nightclub_doors_pole_other_rooms_unchanged=True)
 (J/'evidence/install-report.json').write_text(json.dumps(report,indent=2));(J/'evidence/actors_after.json').write_text(json.dumps(list(after.values()),indent=2));unreal.log('HOUSE_JOIN_INSTALL_PASS')
try:run()
except:unreal.log_error('HOUSE_JOIN_INSTALL_FAIL '+traceback.format_exc())
unreal.SystemLibrary.quit_editor()
