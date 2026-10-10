import unreal,json,traceback
from pathlib import Path
B=Path(__file__).resolve().parent
exec((B/'install.py').read_text().split('def run():')[0])
try:
 assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='Chapter01_House'
 before={a.get_name():snap(a)for a in S.get_all_level_actors()};changed=set()
 configs={'OriginalDoor_Basement_m1_p1_Solid_01':([1747.55,-513.81,-248.666],[5,83.16,113.3]),'OriginalDoor_Upper_m1_p1_Solid_05':([2070.82,-328.95,457.03],[7,84.15,116.49])}
 for a in S.get_all_level_actors():
  if a.get_actor_label() not in configs:continue
  center,half=configs[a.get_actor_label()];sc=a.get_actor_scale3d();changed.add(a.get_name())
  a.set_editor_property('panel_center_world',unreal.Vector(*center));a.set_editor_property('panel_half_extent',unreal.Vector(half[0]/sc.x,half[1]/sc.y,half[2]/sc.z))
  p=a.get_actor_location();a.set_actor_location(p+unreal.Vector(.001,0,0),False,True);a.set_actor_location(p,False,True)
 after={a.get_name():snap(a)for a in S.get_all_level_actors()};assert len(changed)==2
 assert not [n for n,r in before.items()if n not in changed and after.get(n)!=r]
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 (B/'evidence/door-collision-repair.json').write_text(json.dumps({'status':'PASS','doors':configs,'protected_actors':len(before)-2},indent=2))
 unreal.log('NOODLE_DOOR_REPAIR_PASS')
except:unreal.log_error('NOODLE_DOOR_REPAIR_FAIL '+traceback.format_exc())
