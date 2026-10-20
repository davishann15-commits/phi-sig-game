import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
pos=unreal.Vector(-1500,1030,310)
look=unreal.Vector(-1820,869.2,410)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(pos,unreal.MathLibrary.find_look_at_rotation(pos,look))
unreal.log('STAIR_WALL_READY_FOR_REVIEW')
