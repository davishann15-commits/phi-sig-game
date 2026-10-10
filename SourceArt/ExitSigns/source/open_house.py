import unreal,json
w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
a=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
assert not any(isinstance(x,unreal.StoryCheckpoint) for x in a)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(-2167,-480,180),unreal.Rotator(pitch=13,yaw=-90,roll=0))
unreal.log('EXIT_SIGN_HANDOFF_READY map=Chapter01_House checkpoints=0')
