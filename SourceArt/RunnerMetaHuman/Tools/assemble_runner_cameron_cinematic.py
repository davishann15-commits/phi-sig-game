"""Assemble the fitted Runner head/body without optimized texture baking."""
import unreal

PATH = '/Game/Characters/MetaHumans/Runner/MH_Runner_CameronStudy'
TARGET = '/Game/MetaHumans/RunnerCameronCinematic'
assets = unreal.EditorAssetLibrary
character = assets.load_asset(PATH)
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not character or not sub.try_add_object_to_edit(character):
    raise RuntimeError('Runner Cameron source unavailable')
try:
    if not sub.can_build_meta_human(character):
        raise RuntimeError('Runner Cameron source is not rigged/textured')
    build = unreal.MetaHumanCharacterEditorBuildParameters()
    build.pipeline_type = unreal.MetaHumanDefaultPipelineType.CINEMATIC
    build.pipeline_quality = unreal.MetaHumanQualityLevel.CINEMATIC
    build.absolute_build_path = TARGET
    build.common_folder_path = TARGET + '/Common'
    build.enable_wardrobe_item_validation = False
    unreal.log('RUNNER_CAMERON_CINEMATIC_ASSEMBLY_BEGIN')
    sub.build_meta_human(character, build)
    if not assets.save_directory(TARGET, only_if_is_dirty=False, recursive=True):
        raise RuntimeError('Could not save Runner cinematic assembly')
    unreal.log('RUNNER_CAMERON_CINEMATIC_ASSEMBLY_COMPLETE ' +
               str(len(assets.list_assets(TARGET, recursive=True))))
finally:
    sub.remove_object_to_edit(character)
