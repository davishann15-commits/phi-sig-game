"""Delete only the two disabled former storage-room fluorescent light actors."""
import json,pathlib,traceback,unreal
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
exec((B/'expansion2_layout/install_expansion2.py').read_text().split('\ndef run():')[0],globals())
W=B/'wrap_refit'
try:
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 assert world.get_name()=='Chapter01_House'
 before={a.get_name():snapshot(a) for a in S.get_all_level_actors()}
 labels=['Room102_CeilingLight_1','Room102_CeilingLight_2'];removed=set()
 for a in S.get_all_level_actors():
  if a.get_actor_label() not in labels:continue
  assert all(c.get_editor_property('intensity')==0 for c in a.get_components_by_class(unreal.LightComponent))
  removed.add(a.get_name());assert S.destroy_actor(a)
 assert len(removed)==2
 after={a.get_name():snapshot(a) for a in S.get_all_level_actors()}
 assert all(after.get(n)==r for n,r in before.items() if n not in removed)
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 report=json.loads((W/'evidence/install-report.json').read_text());report['actor_count']=len(after)
 report['deleted']+=labels
 report['ordinary_lounge_lights_removed']=True
 (W/'evidence/install-report.json').write_text(json.dumps(report,indent=2))
 (W/'evidence/actors_after.json').write_text(json.dumps(list(after.values()),indent=2))
 unreal.log('CLUB_WRAP_OLD_LIGHTS_REMOVED')
except:unreal.log_error('CLUB_WRAP_LIGHT_CLEANUP_FAIL '+traceback.format_exc())
