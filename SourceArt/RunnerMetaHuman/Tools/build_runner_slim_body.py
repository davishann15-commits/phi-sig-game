"""Locally assemble slimmer exposed arms for Runner's sleeveless outfit."""
import unreal

assets = unreal.EditorAssetLibrary
source_path = '/Game/Characters/MetaHumans/Runner/MH_Runner_JerseyBody'
study_path = '/Game/Characters/MetaHumans/Runner/MH_Runner_SlimBody'
build_path = '/Game/MetaHumans/RunnerSlimBody'
character = assets.load_asset(study_path)
if not character:
    character = assets.duplicate_asset(source_path, study_path)
if not character:
    raise RuntimeError('Could not duplicate the shoulder-complete Runner source')

editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not editor.try_add_object_to_edit(character):
    raise RuntimeError('Could not edit independent slim-body study')
try:
    constraints = editor.get_body_constraints(character)
    targets = {'Bicep': 31.5, 'Elbow': 26.5, 'Forearm': 26.5, 'Wrist': 16.5}
    for index, constraint in enumerate(constraints):
        if str(constraint.name) in targets:
            constraint.is_active = True
            constraint.target_measurement = targets[str(constraint.name)]
            constraints[index] = constraint
    editor.set_body_constraints(character, constraints)
    editor.commit_body_state(character)
    print('RUNNER_SLIM_ARM_CONSTRAINTS',
          [(str(c.name), c.target_measurement) for c in editor.get_body_constraints(character)
           if str(c.name) in targets])
    if not assets.save_loaded_asset(character):
        raise RuntimeError('Could not save the slim-body source')
    if not editor.can_build_meta_human(character):
        print('RUNNER_SLIM_BODY_NOT_BUILDABLE_NO_CLOUD_REQUEST')
    else:
        settings = unreal.MetaHumanCharacterEditorBuildParameters()
        settings.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        settings.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
        settings.absolute_build_path = build_path
        settings.common_folder_path = build_path + '/Common'
        settings.enable_wardrobe_item_validation = False
        editor.build_meta_human(character, settings)
        if not assets.save_directory(build_path, only_if_is_dirty=False, recursive=True):
            raise RuntimeError('Could not save the slim-body assembly')
        print('RUNNER_SLIM_BODY_ASSEMBLED')
finally:
    editor.remove_object_to_edit(character)
