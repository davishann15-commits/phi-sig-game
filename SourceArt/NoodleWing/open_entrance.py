import pathlib,unreal
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NoodleWing'
e=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert e.get_editor_world().get_name()=='Chapter01_House'
p=unreal.Vector(2110,-520,-190);t=unreal.Vector(1745,-515,-183)
e.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,t))
s=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=s.get_all_level_actors();names={a.get_actor_label():a for a in actors}
assert names['NW_HallWalls'].static_mesh_component.static_mesh.get_name()=='SM_NW_HallWalls'
assert abs(names['NW_Hall_Exit'].get_actor_rotation().yaw-90)<.01
unreal.log('NOODLE_ENTRANCE_EDITOR_READY '+str(len(actors)))
