import unreal,json,math
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs')
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
tvs=[a for a in actors if isinstance(a,unreal.HouseInteractiveProp) and 'HouseWallTelevision' in [str(t) for t in a.tags]]
assert len(tvs)==2
assert sum(isinstance(a,unreal.HouseInteractiveProp) and 'SSOKitchenRefit' in [str(t) for t in a.tags] for a in actors)==137
camera=unreal.Vector(-800,310,169);target=next(a.get_actor_location() for a in tvs if a.get_actor_label()=='GreatRoom_WestTV');delta=target-camera
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(camera,unreal.Rotator(pitch=math.degrees(math.atan2(delta.z, math.hypot(delta.x,delta.y))), yaw=math.degrees(math.atan2(delta.y,delta.x)), roll=0))
(R/'editor-ready.json').write_text(json.dumps(dict(passed=True,tvs=2,level=world.get_name())))
unreal.log('GREAT_ROOM_TVS_READY_FOR_REVIEW')
