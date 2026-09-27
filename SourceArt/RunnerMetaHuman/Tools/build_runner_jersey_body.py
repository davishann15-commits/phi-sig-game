"""Assemble a shoulder-complete copy of Runner for a sleeveless jersey.

The regular optimized MetaHuman body omits vertices hidden by its shirt. This
separate character source removes only the default outfit, preserving the
approved face and rig, and gives the jersey an actual skinned upper body.
"""
import unreal

SOURCE = '/Game/Characters/MetaHumans/Runner/MH_Runner_MaleFace'
CHARACTER = '/Game/Characters/MetaHumans/Runner/MH_Runner_JerseyBody'
TARGET = '/Game/MetaHumans/RunnerJerseyBody'
assets = unreal.EditorAssetLibrary
character = assets.load_asset(CHARACTER)
if not character:
    character = assets.duplicate_asset(SOURCE, CHARACTER)
if not character:
    raise RuntimeError('Runner male-face source could not be duplicated')

sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not sub.try_add_object_to_edit(character):
    raise RuntimeError('Runner jersey-body source is locked')
try:
    collection = sub.get_preview_collection(character)
    empty = unreal.MetaHumanPaletteItemKey()
    collection.default_instance.set_single_slot_selection('Outfits', empty)
    sub.on_edit_preview_collection(character)
    if not assets.save_loaded_asset(character):
        raise RuntimeError('Could not save the shoulder-complete source')
    unreal.log('RUNNER_JERSEY_BARE_SOURCE_READY')
    if not sub.can_build_meta_human(character):
        raise RuntimeError('Runner jersey-body source needs rigging')

    build = unreal.MetaHumanCharacterEditorBuildParameters()
    build.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    build.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
    build.absolute_build_path = TARGET
    build.common_folder_path = TARGET + '/Common'
    build.enable_wardrobe_item_validation = False
    unreal.log('RUNNER_JERSEY_BARE_ASSEMBLY_BEGIN')
    sub.build_meta_human(character, build)
    if not assets.save_directory(TARGET, only_if_is_dirty=False, recursive=True):
        raise RuntimeError('Could not save the shoulder-complete body')
    unreal.log('RUNNER_JERSEY_BARE_ASSEMBLY_COMPLETE ' +
               str(len(assets.list_assets(TARGET, recursive=True))))
finally:
    sub.remove_object_to_edit(character)
