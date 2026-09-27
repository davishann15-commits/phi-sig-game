"""Rig and assemble the isolated Runner MetaHuman, following Braxton's pipeline."""
import unreal

PATH = '/Game/Characters/MetaHumans/Runner/MH_Runner_Working'
TARGET = '/Game/MetaHumans/RunnerRebuild'
assets = unreal.EditorAssetLibrary
character = assets.load_asset(PATH)
if not character:
    raise RuntimeError('Runner MetaHuman source missing')
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not sub.try_add_object_to_edit(character):
    raise RuntimeError('Runner could not be opened for assembly')

try:
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
        ['/MetaHumanCharacter/Optional'], force_rescan=True)
    collection = sub.get_preview_collection(character)
    for slot, rel in [
        ('Hair', 'Grooms/Bindings/Hair/WI_Hair_S_Messy'),
        ('Eyebrows', 'Grooms/Bindings/Eyebrows/WI_Eyebrows_M_SlightArch'),
        ('Outfits', 'Clothing/WI_DefaultGarment'),
    ]:
        path = '/MetaHumanCharacter/Optional/' + rel
        item = assets.load_asset(path)
        if not item:
            raise RuntimeError('Missing MetaHuman wardrobe: ' + path)
        key = collection.try_add_item_from_wardrobe_item(slot, item)
        collection.default_instance.set_single_slot_selection(slot, key)
    sub.on_edit_preview_collection(character)
    assets.save_loaded_asset(character)
    unreal.log('RUNNER_WARDROBE_READY')

    # Same authorized Epic MetaHuman build service used for Braxton. The raw
    # reference photographs are not uploaded by this script.
    if not character.has_high_resolution_textures:
        tex = unreal.MetaHumanCharacterTextureRequestParams()
        tex.blocking = True
        tex.report_progress = False
        unreal.log('RUNNER_TEXTURE_REQUEST_BEGIN')
        sub.request_texture_sources(character, tex)
        assets.save_loaded_asset(character)
        unreal.log('RUNNER_TEXTURE_REQUEST_DONE ' + str(character.has_high_resolution_textures))

    rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
    rig.blocking = True
    rig.report_progress = False
    rig.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
    unreal.log('RUNNER_RIG_REQUEST_BEGIN')
    sub.request_auto_rigging(character, rig)
    assets.save_loaded_asset(character)
    unreal.log('RUNNER_RIG_REQUEST_DONE')

    if not sub.can_build_meta_human(character):
        raise RuntimeError('MetaHuman build prerequisites still missing')
    build = unreal.MetaHumanCharacterEditorBuildParameters()
    build.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    build.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
    build.absolute_build_path = TARGET
    build.common_folder_path = TARGET + '/Common'
    build.enable_wardrobe_item_validation = False
    unreal.log('RUNNER_ASSEMBLY_BEGIN')
    sub.build_meta_human(character, build)
    if not assets.save_directory(TARGET, only_if_is_dirty=False, recursive=True):
        raise RuntimeError('Could not persist Runner assembled packages')
    assets.save_loaded_asset(character)
    found = assets.list_assets(TARGET, recursive=True)
    unreal.log('RUNNER_ASSEMBLY_COUNT ' + str(len(found)))
    unreal.log('RUNNER_ASSEMBLY_BLUEPRINTS ' + str([p for p in found if 'BP_' in p]))
    if not found:
        raise RuntimeError('MetaHuman assembly generated no assets')
    unreal.log('RUNNER_META_ASSEMBLY_COMPLETE')
finally:
    sub.remove_object_to_edit(character)
