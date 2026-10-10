import unreal
w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
a=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
assert not any(isinstance(x,unreal.StoryCheckpoint) for x in a)
assert len([x for x in a if isinstance(x,unreal.HouseExitSign)])==4
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(-2180,-880,485),unreal.Rotator(pitch=14,yaw=180,roll=0))
unreal.log('NEON_SIGN_HANDOFF_READY map=Chapter01_House checkpoints=0')
