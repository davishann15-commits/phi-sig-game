"""Assemble the masculine fitted face through Runner's known-working pipeline."""
import unreal

PATH = '/Game/Characters/MetaHumans/Runner/MH_Runner_MaleFace'
TARGET = '/Game/MetaHumans/RunnerMaleFace'
assets = unreal.EditorAssetLibrary
character = assets.load_asset(PATH)
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not character or not sub.try_add_object_to_edit(character):
    raise RuntimeError('Runner male-face source unavailable')
try:
    if not character.has_high_resolution_textures:
        tex = unreal.MetaHumanCharacterTextureRequestParams()
        tex.blocking = True
        tex.report_progress = False
        sub.request_texture_sources(character, tex)
        assets.save_loaded_asset(character)
    rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
    rig.blocking = True
    rig.report_progress = False
    rig.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
    unreal.log('RUNNER_MALE_FACE_RIG_BEGIN')
    sub.request_auto_rigging(character, rig)
    assets.save_loaded_asset(character)
    unreal.log('RUNNER_MALE_FACE_RIG_DONE')
    if not sub.can_build_meta_human(character):
        raise RuntimeError('Runner male-face rig cannot assemble')
    build = unreal.MetaHumanCharacterEditorBuildParameters()
    build.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    build.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
    build.absolute_build_path = TARGET
    build.common_folder_path = TARGET + '/Common'
    build.enable_wardrobe_item_validation = False
    unreal.log('RUNNER_MALE_FACE_ASSEMBLY_BEGIN')
    sub.build_meta_human(character, build)
    if not assets.save_directory(TARGET, only_if_is_dirty=False, recursive=True):
        raise RuntimeError('Could not save Runner male-face build')
    assets.save_loaded_asset(character)
    unreal.log('RUNNER_MALE_FACE_ASSEMBLY_COMPLETE ' +
               str(len(assets.list_assets(TARGET, recursive=True))))
finally:
    sub.remove_object_to_edit(character)
