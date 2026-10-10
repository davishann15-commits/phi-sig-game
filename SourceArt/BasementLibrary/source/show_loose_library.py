"""Open the saved library for the user; no map edits or gameplay changes."""
import unreal,json
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/BasementLibrary/loose-items')
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
if not world:raise RuntimeError('Could not open saved house')
loc=unreal.Vector(-2520,40,-174)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc,unreal.MathLibrary.find_look_at_rotation(loc,unreal.Vector(-2810,223,-249)))
unreal.EditorLevelLibrary.editor_set_game_view(True)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
items=[a for a in actors.get_all_level_actors()if 'SSOLibraryRebuild'in [str(t)for t in a.tags]]
(R/'editor-ready.json').write_text(json.dumps(dict(map=world.get_path_name(),library_actors=len(items),loose_items=sum('SSOLibrarySmallItem'in [str(t)for t in a.tags]for a in items),saved_map=True),indent=2))
unreal.log('LIBRARY_READY_FOR_PLAYER')
