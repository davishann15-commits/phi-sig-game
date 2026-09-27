"""Finish a local Runner mouth-profile study without touching the playable asset."""
import math
import unreal

assets = unreal.EditorAssetLibrary
source_path = '/Game/Characters/MetaHumans/Runner/MH_Runner_FaceStudyV3'
study_path = '/Game/Characters/MetaHumans/Runner/MH_Runner_FaceStudyV4'
build_path = '/Game/MetaHumans/RunnerFaceStudyV4'
if assets.does_asset_exist(study_path):
    raise RuntimeError('V4 study already exists')
character = assets.duplicate_asset(source_path, study_path)
if not character:
    raise RuntimeError('Could not duplicate V3 face source')
editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not editor.try_add_object_to_edit(character):
    raise RuntimeError('Could not edit V4 face source')
try:
    before = editor.get_face_landmarks(character)
    # MetaHuman constrains requested landmark translations to plausible face
    # space.  These concentrated moves visibly reduce the oversized lips in
    # the previous profile while preserving the revised nose and jaw.
    requests = {
        4: (0.0, -2.0, -2.0),   # upper-lip center
        9: (0.8, -1.3, -1.2),   # upper lip, character left
        16: (-0.8, -1.3, -1.2), # upper lip, character right
        62: (0.0, -2.1, 2.1),  # lower-lip center
        3: (0.7, -1.4, 1.0),   # lower lip left
        30: (-0.7, -1.4, 1.0), # lower lip right
    }
    editor.translate_face_landmarks(
        character, list(requests),
        [unreal.Vector(*requests[index]) for index in requests])
    editor.commit_face_state(character)
    after = editor.get_face_landmarks(character)
    print('RUNNER_V4_MOUTH_FEATURES', [(index,
          tuple(round(value, 3) for value in (after[index].x, after[index].y, after[index].z)))
          for index in requests])
    print('RUNNER_V4_FACE_DELTA_MAX_CM', round(max(math.dist(
        (a.x,a.y,a.z), (b.x,b.y,b.z)) for a,b in zip(before,after)), 3))
    if not assets.save_loaded_asset(character):
        raise RuntimeError('Could not save V4 face source')
    if not editor.can_build_meta_human(character):
        print('RUNNER_V4_NOT_BUILDABLE_NO_CLOUD_REQUEST')
    else:
        build = unreal.MetaHumanCharacterEditorBuildParameters()
        build.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        build.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
        build.absolute_build_path = build_path
        build.common_folder_path = build_path + '/Common'
        build.enable_wardrobe_item_validation = False
        editor.build_meta_human(character, build)
        if not assets.save_directory(build_path, only_if_is_dirty=False, recursive=True):
            raise RuntimeError('Could not save V4 assembly')
        print('RUNNER_V4_FACE_ASSEMBLED')
finally:
    editor.remove_object_to_edit(character)
