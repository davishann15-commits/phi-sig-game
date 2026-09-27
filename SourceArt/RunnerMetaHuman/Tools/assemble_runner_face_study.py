"""Assemble the saved local Runner face study with GPU material baking enabled."""
import unreal

assets = unreal.EditorAssetLibrary
character = assets.load_asset('/Game/Characters/MetaHumans/Runner/MH_Runner_FaceStudy')
editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not character or not editor.try_add_object_to_edit(character):
    raise RuntimeError('Saved Runner face study could not be opened')
try:
    if not editor.can_build_meta_human(character):
        raise RuntimeError('Face study requires new rigging; no cloud request was made')
    settings = unreal.MetaHumanCharacterEditorBuildParameters()
    settings.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    settings.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
    settings.absolute_build_path = '/Game/MetaHumans/RunnerFaceStudy'
    settings.common_folder_path = '/Game/MetaHumans/RunnerFaceStudy/Common'
    settings.enable_wardrobe_item_validation = False
    print('RUNNER_FACE_ASSEMBLY_BEGIN')
    editor.build_meta_human(character, settings)
    if not assets.save_directory('/Game/MetaHumans/RunnerFaceStudy',
                                 only_if_is_dirty=False, recursive=True):
        raise RuntimeError('Runner face assembly was not saved')
    print('RUNNER_FACE_ASSEMBLY_COMPLETE')
finally:
    editor.remove_object_to_edit(character)
