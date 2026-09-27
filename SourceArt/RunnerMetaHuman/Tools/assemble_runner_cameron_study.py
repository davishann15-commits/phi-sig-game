"""Rig and assemble the independent Runner preset study for visual QA."""
import unreal

PATH = '/Game/Characters/MetaHumans/Runner/MH_Runner_CameronStudy'
TARGET = '/Game/MetaHumans/RunnerCameronStudy'
assets = unreal.EditorAssetLibrary
character = assets.load_asset(PATH)
if not character:
    raise RuntimeError('Fitted Runner preset study missing')
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not sub.try_add_object_to_edit(character):
    raise RuntimeError('Could not edit Runner study for assembly')
try:
    if not character.has_high_resolution_textures:
        params = unreal.MetaHumanCharacterTextureRequestParams()
        params.blocking = True
        params.report_progress = False
        unreal.log('RUNNER_CAMERON_TEXTURE_BEGIN')
        sub.request_texture_sources(character, params)
        assets.save_loaded_asset(character)
        unreal.log('RUNNER_CAMERON_TEXTURE_DONE ' + str(character.has_high_resolution_textures))
    params = unreal.MetaHumanCharacterAutoRiggingRequestParams()
    params.blocking = True
    params.report_progress = False
    params.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
    unreal.log('RUNNER_CAMERON_RIG_BEGIN')
    sub.request_auto_rigging(character, params)
    assets.save_loaded_asset(character)
    unreal.log('RUNNER_CAMERON_RIG_DONE')
    if not sub.can_build_meta_human(character):
        raise RuntimeError('Runner preset study cannot be assembled')
    build = unreal.MetaHumanCharacterEditorBuildParameters()
    build.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    build.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
    build.absolute_build_path = TARGET
    build.common_folder_path = TARGET + '/Common'
    build.enable_wardrobe_item_validation = False
    unreal.log('RUNNER_CAMERON_ASSEMBLY_BEGIN')
    sub.build_meta_human(character, build)
    if not assets.save_directory(TARGET, only_if_is_dirty=False, recursive=True):
        raise RuntimeError('Could not save Runner preset study')
    assets.save_loaded_asset(character)
    unreal.log('RUNNER_CAMERON_ASSEMBLY_COMPLETE ' + str(len(assets.list_assets(TARGET, recursive=True))))
finally:
    sub.remove_object_to_edit(character)
