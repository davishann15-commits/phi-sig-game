import unreal
from pathlib import Path
B=Path(unreal.Paths.project_dir())/'SourceArt/NoodleWing'
e=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
w=e.get_editor_world();assert w and w.get_name()=='Chapter01_House'
p=unreal.Vector(1505,540,-213.966);t=unreal.Vector(1920,513,-205.3762)
e.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,t))
a=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
assert len([x for x in a if x.get_actor_label().startswith('NW_')])==122
(B/'evidence/stair-editor-ready.txt').write_text('Chapter01_House open. Revised stairwell loaded, 122 Noodle Wing actors. Camera at the basement doorway facing the new first flight.\n')
unreal.log('NOODLE_STAIR_EDITOR_READY')
