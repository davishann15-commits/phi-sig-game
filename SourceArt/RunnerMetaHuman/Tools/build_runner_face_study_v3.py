"""Target the Runner's mouth, nose and rounded lower face in a separate study.

Everything runs on local assets; there is no cloud face-rig or texture request.
"""
import math
import unreal

assets = unreal.EditorAssetLibrary
source_path = '/Game/Characters/MetaHumans/Runner/MH_Runner_FaceStudyV2'
study_path = '/Game/Characters/MetaHumans/Runner/MH_Runner_FaceStudyV3'
build_path = '/Game/MetaHumans/RunnerFaceStudyV3'
if assets.does_asset_exist(study_path):
    raise RuntimeError('V3 study already exists')
character = assets.duplicate_asset(source_path, study_path)
if not character:
    raise RuntimeError('Could not duplicate the V2 reference study')

editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not editor.try_add_object_to_edit(character):
    raise RuntimeError('Could not edit independent V3 study')
try:
    before = editor.get_face_landmarks(character)
    if len(before) != 79:
        raise RuntimeError('Unexpected landmark topology')

    def bell(value, center, radius):
        return max(0.0, 1.0 - abs(value - center) / radius)

    offsets = []
    for p in before:
        x, y, z = p.x, p.y, p.z
        dx = dy = dz = 0.0

        # Compact the mouth horizontally, flatten its front projection, and
        # close the overly full upper/lower lip spacing toward the lip seam.
        mouth = bell(z, 169.6, 3.1) * bell(abs(x), 1.7, 3.2)
        if y > 9.5:
            dx -= 0.56 * x * mouth
            dy -= 1.75 * mouth
            dz += (169.5 - z) * 0.48 * mouth

        # Fuller but softly rounded lower-face silhouette in the photo.
        jaw = bell(z, 166.5, 5.8) * bell(abs(x), 4.6, 4.0)
        if 0.0 < y < 12.0:
            dx += math.copysign(2.0 * jaw, x)
            dy += 0.30 * jaw

        # Short straight nose, narrower nostril edge, and less pronounced tip.
        nose = bell(z, 174.8, 3.0) * bell(abs(x), 0.8, 2.6)
        if y > 11.0:
            dy -= 1.2 * nose
            dx -= 0.38 * x * nose

        offsets.append(unreal.Vector(dx, dy, dz))

    editor.translate_face_landmarks(character, list(range(len(before))), offsets)
    editor.commit_face_state(character)
    after = editor.get_face_landmarks(character)
    print('RUNNER_V3_FACE_DELTA_MAX_CM', round(max(math.dist(
        (a.x,a.y,a.z), (b.x,b.y,b.z)) for a,b in zip(before,after)), 3))

    settings = character.get_editor_property('skin_settings')
    accents = settings.get_editor_property('accents')
    lips = accents.get_editor_property('lips')
    lips.set_editor_property('redness', 0.34)
    lips.set_editor_property('saturation', 0.08)
    lips.set_editor_property('lightness', 0.52)
    accents.set_editor_property('lips', lips)
    settings.set_editor_property('accents', accents)
    editor.commit_skin_settings(character, settings)
    if not assets.save_loaded_asset(character):
        raise RuntimeError('Could not save V3 face source')
    if not editor.can_build_meta_human(character):
        print('RUNNER_V3_NOT_BUILDABLE_NO_CLOUD_REQUEST')
    else:
        build = unreal.MetaHumanCharacterEditorBuildParameters()
        build.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        build.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
        build.absolute_build_path = build_path
        build.common_folder_path = build_path + '/Common'
        build.enable_wardrobe_item_validation = False
        editor.build_meta_human(character, build)
        if not assets.save_directory(build_path, only_if_is_dirty=False, recursive=True):
            raise RuntimeError('Could not save V3 assembled assets')
        print('RUNNER_V3_FACE_ASSEMBLED')
finally:
    editor.remove_object_to_edit(character)
