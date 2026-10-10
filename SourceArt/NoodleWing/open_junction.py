import pathlib,json,unreal
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NoodleWing'
e=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert e.get_editor_world().get_name()=='Chapter01_House'
p=unreal.Vector(2458,-643,-191.966);t=unreal.Vector(2475,-1035,-205)
e.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,t))
s=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=s.get_all_level_actors();names={a.get_actor_label():a for a in actors}
for n in ['NW_RedWalls','NW_BlueWalls','NW_HallWalls','NW_BasementTrim','NW_StairStructure']:assert n in names,n
unreal.log('NOODLE_JUNCTION_EDITOR_READY '+str(len(actors)))
