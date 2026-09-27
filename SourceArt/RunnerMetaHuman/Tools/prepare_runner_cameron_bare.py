"""Omit optional wardrobe from the male Runner build.

Runtime hair, jersey, glasses and sandals are attached separately; this also
avoids the MetaHuman 5.8 Mac texture-graph crash in wardrobe material baking.
"""
import unreal

PATH = '/Game/Characters/MetaHumans/Runner/MH_Runner_CameronStudy'
assets = unreal.EditorAssetLibrary
character = assets.load_asset(PATH)
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not character or not sub.try_add_object_to_edit(character):
    raise RuntimeError('Runner Cameron study unavailable')
try:
    collection = sub.get_preview_collection(character)
    empty = unreal.MetaHumanPaletteItemKey()
    for slot in ('Hair', 'Eyebrows', 'Outfits'):
        collection.default_instance.set_single_slot_selection(slot, empty)
    sub.on_edit_preview_collection(character)
    if not assets.save_loaded_asset(character):
        raise RuntimeError('Could not save bare Runner source')
    unreal.log('RUNNER_CAMERON_BARE_READY')
finally:
    sub.remove_object_to_edit(character)
