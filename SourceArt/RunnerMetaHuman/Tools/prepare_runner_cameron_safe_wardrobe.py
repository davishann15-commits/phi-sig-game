"""Use the known-good Runner wardrobe set on the fitted Cameron face study."""
import unreal

PATH = '/Game/Characters/MetaHumans/Runner/MH_Runner_CameronStudy'
assets = unreal.EditorAssetLibrary
character = assets.load_asset(PATH)
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not character or not sub.try_add_object_to_edit(character):
    raise RuntimeError('Runner Cameron study unavailable')
try:
    collection = sub.get_preview_collection(character)
    for slot, rel in [
        ('Hair', 'Grooms/Bindings/Hair/WI_Hair_S_Messy'),
        ('Eyebrows', 'Grooms/Bindings/Eyebrows/WI_Eyebrows_M_SlightArch'),
        ('Outfits', 'Clothing/WI_DefaultGarment'),
    ]:
        item = assets.load_asset('/MetaHumanCharacter/Optional/' + rel)
        if not item:
            raise RuntimeError('Known-good wardrobe missing: ' + rel)
        key = collection.try_add_item_from_wardrobe_item(slot, item)
        collection.default_instance.set_single_slot_selection(slot, key)
    sub.on_edit_preview_collection(character)
    if not assets.save_loaded_asset(character):
        raise RuntimeError('Could not save safe Runner wardrobe')
    unreal.log('RUNNER_CAMERON_SAFE_WARDROBE_READY')
finally:
    sub.remove_object_to_edit(character)
