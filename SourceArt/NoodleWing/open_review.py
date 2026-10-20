import unreal
from pathlib import Path
B=Path(__file__).resolve().parent
view=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world=view.get_editor_world()
assert world and world.get_name()=='Chapter01_House'
p=unreal.Vector(2670,-754,-199);target=unreal.Vector(2840,-1290,-250)
view.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,target))
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
assert sum(a.get_actor_label().startswith('NW_') for a in actors)==121
(B/'evidence/editor-ready.txt').write_text('Chapter01_House open; 121 Noodle Wing actors loaded; camera at the red room.\n')
unreal.log('NOODLE_EDITOR_READY')
