import unreal
sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
pos = unreal.Vector(-2240, 947, 466)
look = unreal.Vector(-2210, 1234, 416)
sub.set_level_viewport_camera_info(pos, unreal.MathLibrary.find_look_at_rotation(pos, look), sub.get_active_viewport_config_key())
sub.editor_set_game_view(True, sub.get_active_viewport_config_key())
