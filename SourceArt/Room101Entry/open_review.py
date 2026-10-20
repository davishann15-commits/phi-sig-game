import unreal
sub=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
k=sub.get_active_viewport_config_key()
p=unreal.Vector(-2110*1.1,-1030*1.1,155*1.1)
t=unreal.Vector(-2318*1.1,-1330*1.1,114*1.1)
sub.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,t),k)
sub.editor_set_game_view(True,k)
unreal.log('ROOM101_EDITOR_VIEW_READY')
