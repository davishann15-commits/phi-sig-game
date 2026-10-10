import unreal,json
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen')
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
count=0
for a in actors.get_all_level_actors():
 if isinstance(a,unreal.HouseInteractiveProp) and str(a.prop_id).startswith('Kitchen_ServingCounter_'):
  a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=180,roll=0),False);count+=1
assert count==3
assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Story/Maps/Chapter01_House')
(R/'final-placement.json').write_text(json.dumps(dict(passed=True,servingCountersFacedTowardDining=count)))
unreal.log('KITCHEN_FINAL_PLACEMENT_SAVED')
unreal.SystemLibrary.quit_editor()
