"""Build a separate Runner study with a shorter, forward-falling hairstyle.

This keeps the current playable character and its source intact for review.
No new auto-rigging or texture-source requests are made.
"""
import unreal

assets = unreal.EditorAssetLibrary
source_path = '/Game/Characters/MetaHumans/Runner/MH_Runner_MaleFace'
target_path = '/Game/Characters/MetaHumans/Runner/MH_Runner_CasualHairStudy'
build_path = '/Game/MetaHumans/RunnerCasualHairStudy'
character = assets.load_asset(target_path)
if not character:
    character = assets.duplicate_asset(source_path, target_path)
if not character:
    raise RuntimeError('Could not duplicate Runner without touching the playable source')

editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not editor.try_add_object_to_edit(character):
    raise RuntimeError('Runner hair study could not be opened')
try:
    collection = editor.get_preview_collection(character)
    item = assets.load_asset('/MetaHumanCharacter/Optional/Grooms/Bindings/Hair/WI_Hair_S_Casual')
    if not item:
        raise RuntimeError('Short casual hairstyle is unavailable locally')
    key = collection.try_add_item_from_wardrobe_item('Hair', item)
    collection.default_instance.set_single_slot_selection('Hair', key)
    editor.on_edit_preview_collection(character)
    assets.set_metadata_tag(character, 'RunnerHairReference', 'IMG_1982.HEIC;IMG_1983.HEIC;IMG_1984.HEIC')
    assets.set_metadata_tag(character, 'RunnerHairStudy', 'ShortCasual')
    if not assets.save_loaded_asset(character):
        raise RuntimeError('Could not save separate hairstyle study')
    print('RUNNER_CASUAL_BUILDABLE', editor.can_build_meta_human(character))
    if not editor.can_build_meta_human(character):
        raise RuntimeError('Hair study is not rig-ready; original playable model is unchanged')
    settings = unreal.MetaHumanCharacterEditorBuildParameters()
    settings.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    settings.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
    settings.absolute_build_path = build_path
    settings.common_folder_path = build_path + '/Common'
    settings.enable_wardrobe_item_validation = False
    print('RUNNER_CASUAL_ASSEMBLY_BEGIN')
    editor.build_meta_human(character, settings)
    if not assets.save_directory(build_path, only_if_is_dirty=False, recursive=True):
        raise RuntimeError('Could not save assembled hair study')
    print('RUNNER_CASUAL_ASSEMBLY_COMPLETE',
          len(assets.list_assets(build_path, recursive=True)))
finally:
    editor.remove_object_to_edit(character)
