"""Locally sculpt a separate Runner face study against his close-up reference.

This script deliberately does not request cloud rigging or texture synthesis.
"""
import math
import unreal

assets = unreal.EditorAssetLibrary
source_path = '/Game/Characters/MetaHumans/Runner/MH_Runner_CasualHairStudy'
variant = 'V2' if '-RunnerFaceStudyV2' in unreal.SystemLibrary.get_command_line() else ''
study_path = '/Game/Characters/MetaHumans/Runner/MH_Runner_FaceStudy' + variant
build_path = '/Game/MetaHumans/RunnerFaceStudy' + variant

if assets.does_asset_exist(study_path):
    raise RuntimeError('Runner face study already exists; review it before another sculpt')
character = assets.duplicate_asset(source_path, study_path)
if not character:
    raise RuntimeError('Could not make an independent Runner face study')

editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not editor.try_add_object_to_edit(character):
    raise RuntimeError('Could not edit independent Runner face study')
try:
    before = editor.get_face_landmarks(character)
    if len(before) != 79:
        raise RuntimeError('Unexpected Runner facial topology')

    def bell(value, center, radius):
        return max(0.0, 1.0 - abs(value - center) / radius)

    offsets = []
    for point in before:
        x, y, z = point.x, point.y, point.z
        dx = dy = dz = 0.0

        # In the front photo the lower face is soft and rounded rather than
        # tapered to the avatar's small chin.  Widen the jaw gradually while
        # keeping the centerline and ear attachment stable.
        lower_face = bell(z, 168.0, 8.0) * bell(abs(x), 4.0, 4.5)
        dx += math.copysign(0.38 * lower_face, x)
        if abs(x) < 2.5 and z < 166.5 and y > 3.0:
            dz += 0.18 * bell(z, 163.5, 4.0)

        # Soften the hollow-looking cheek transition and round the nose-side
        # area.  Do not move the hidden eyes behind the reference sunglasses.
        cheek = bell(z, 175.0, 5.5) * bell(abs(x), 5.1, 3.0)
        if y > 5.0:
            dy += 0.13 * cheek

        # Photo: short, fairly straight nose.  Keep the bridge but reduce the
        # protruding tip and the width of the lower nostril landmarks.
        nose = bell(z, 174.6, 3.2) * bell(abs(x), 0.5, 2.8)
        if y > 11.0:
            dy -= 0.30 * nose
            dx -= math.copysign(0.07 * nose, x)

        # The original lips are rounded and prominent; the reference has a
        # smaller, flatter, relaxed mouth.  Keep its expression neutral.
        mouth = bell(z, 169.5, 3.2) * bell(abs(x), 1.5, 3.7)
        if y > 10.0:
            dy -= 0.28 * mouth
            dx -= 0.05 * x * mouth

        sculpt_strength = 4.0 if variant else 1.0
        offsets.append(unreal.Vector(dx * sculpt_strength,
                                     dy * sculpt_strength,
                                     dz * sculpt_strength))

    editor.translate_face_landmarks(character, list(range(len(before))), offsets)
    editor.commit_face_state(character)
    after = editor.get_face_landmarks(character)
    print('RUNNER_FACE_DELTA_MAX_CM', round(max(math.dist(
        (a.x, a.y, a.z), (b.x, b.y, b.z))
        for a, b in zip(before, after)), 3))
    print('RUNNER_FACE_BUILDABLE_AFTER_SCULPT', editor.can_build_meta_human(character))
    if variant:
        # The source has conspicuously saturated, full-looking pink lips;
        # lower their colour and give the fair skin subdued freckles/flush.
        skin_settings = character.get_editor_property('skin_settings')
        freckles = skin_settings.get_editor_property('freckles')
        freckles.set_editor_property('density', 0.62)
        freckles.set_editor_property('strength', 0.32)
        freckles.set_editor_property('saturation', 0.48)
        freckles.set_editor_property('mask', unreal.MetaHumanCharacterFrecklesMask.TYPE2)
        skin_settings.set_editor_property('freckles', freckles)
        accents = skin_settings.get_editor_property('accents')
        lips = accents.get_editor_property('lips')
        lips.set_editor_property('redness', 0.40)
        lips.set_editor_property('saturation', 0.27)
        lips.set_editor_property('lightness', 0.48)
        accents.set_editor_property('lips', lips)
        for name in ('nose', 'cheeks'):
            region = accents.get_editor_property(name)
            region.set_editor_property('redness', 0.55)
            region.set_editor_property('saturation', 0.47)
            accents.set_editor_property(name, region)
        skin_settings.set_editor_property('accents', accents)
        editor.commit_skin_settings(character, skin_settings)
        print('RUNNER_FACE_BUILDABLE_AFTER_SKIN', editor.can_build_meta_human(character))
    assets.set_metadata_tag(character, 'RunnerFaceReference',
                            'COBBLE IMG_1982/1983/1984; close-up sunglasses photo 2026-09-23')
    if not assets.save_loaded_asset(character):
        raise RuntimeError('Could not save the face study source')

    if editor.can_build_meta_human(character):
        settings = unreal.MetaHumanCharacterEditorBuildParameters()
        settings.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        settings.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
        settings.absolute_build_path = build_path
        settings.common_folder_path = build_path + '/Common'
        settings.enable_wardrobe_item_validation = False
        editor.build_meta_human(character, settings)
        if not assets.save_directory(build_path, only_if_is_dirty=False, recursive=True):
            raise RuntimeError('Could not save assembled face study')
        print('RUNNER_FACE_STUDY_ASSEMBLED')
    else:
        print('RUNNER_FACE_STUDY_SAVED_NOT_ASSEMBLED_NO_CLOUD_REQUEST')
finally:
    editor.remove_object_to_edit(character)
