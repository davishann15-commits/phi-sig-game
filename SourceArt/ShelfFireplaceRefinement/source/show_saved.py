import unreal,json
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ShelfFireplaceRefinement')
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House');assert world
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();fireplaces=[a for a in actors if'GreatRoomSolidFireplace'in[str(t)for t in a.tags]]
assert len(fireplaces)==2
loc=unreal.Vector(-670,-130,170);target=unreal.Vector(-1135,189.9,95)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc,unreal.MathLibrary.find_look_at_rotation(loc,target));unreal.EditorLevelLibrary.editor_set_game_view(True)
(R/'editor-ready.json').write_text(json.dumps(dict(map=world.get_path_name(),fireplaces=[a.get_actor_label()for a in fireplaces],shelves=sum('SSOLibraryShelf'in[str(t)for t in a.tags]for a in actors),saved=True),indent=2))
unreal.log('SHELVES_AND_FIREPLACES_READY')
