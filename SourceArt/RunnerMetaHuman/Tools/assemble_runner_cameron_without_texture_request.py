"""Test assembling the fitted male Runner preset using its bundled textures.

This is an isolated study: it does not touch the current playable Runner.
"""
import unreal

PATH = '/Game/Characters/MetaHumans/Runner/MH_Runner_CameronStudy'
TARGET = '/Game/MetaHumans/RunnerCameronStudy'
assets = unreal.EditorAssetLibrary
character = assets.load_asset(PATH)
if not character:
    raise RuntimeError('Runner Cameron study missing')
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not sub.try_add_object_to_edit(character):
    raise RuntimeError('Could not open Runner Cameron study')
try:
    unreal.log('RUNNER_CAMERON_OFFLINE_BEGIN high_res=' +
               str(character.has_high_resolution_textures) +
               ' can_build=' + str(sub.can_build_meta_human(character)))
    rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
    rig.blocking = True
    rig.report_progress = False
    rig.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
    unreal.log('RUNNER_CAMERON_OFFLINE_RIG_BEGIN')
    sub.request_auto_rigging(character, rig)
    assets.save_loaded_asset(character)
    unreal.log('RUNNER_CAMERON_OFFLINE_RIG_DONE can_build=' +
               str(sub.can_build_meta_human(character)))
    if not sub.can_build_meta_human(character):
        raise RuntimeError('Cameron preset cannot assemble without high-res textures')
    build = unreal.MetaHumanCharacterEditorBuildParameters()
    build.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    build.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
    build.absolute_build_path = TARGET
    build.common_folder_path = TARGET + '/Common'
    build.enable_wardrobe_item_validation = False
    unreal.log('RUNNER_CAMERON_OFFLINE_ASSEMBLE_BEGIN')
    sub.build_meta_human(character, build)
    assets.save_directory(TARGET, only_if_is_dirty=False, recursive=True)
    unreal.log('RUNNER_CAMERON_OFFLINE_COMPLETE')
finally:
    sub.remove_object_to_edit(character)
