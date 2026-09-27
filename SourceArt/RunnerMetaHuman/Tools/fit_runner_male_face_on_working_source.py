"""Copy the fitted masculine face shape onto a duplicate of the working rig source.

The original working MetaHuman and Braxton remain unchanged. The target keeps
the already-working Runner's body proportions, skin settings and wardrobe.
"""
import unreal

assets = unreal.EditorAssetLibrary
working_path = '/Game/Characters/MetaHumans/Runner/MH_Runner_Working'
male_path = '/Game/Characters/MetaHumans/Runner/MH_Runner_CameronStudy'
target_path = '/Game/Characters/MetaHumans/Runner/MH_Runner_MaleFace'
working = assets.load_asset(working_path)
male = assets.load_asset(male_path)
target = assets.load_asset(target_path)
if not target:
    target = assets.duplicate_asset(working_path, target_path)
if not working or not male or not target:
    raise RuntimeError('Runner source assets missing')

sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not sub.try_add_object_to_edit(male):
    raise RuntimeError('Could not read fitted male face')
try:
    coefficients = sub.get_face_model_coefficients(male)
    unreal.log('RUNNER_MALE_FACE_COEFFICIENTS ' + str(len(coefficients)))
finally:
    sub.remove_object_to_edit(male)

if len(coefficients) < 50:
    raise RuntimeError('Fitted male face coefficients incomplete')
if not sub.try_add_object_to_edit(target):
    raise RuntimeError('Could not open working Runner duplicate')
try:
    before = sub.get_face_model_coefficients(target)
    if len(before) != len(coefficients):
        raise RuntimeError('Face model topology differs between sources')
    sub.set_face_model_coefficients(target, coefficients)
    sub.commit_face_state(target)
    assets.set_metadata_tag(target, 'RunnerMaleFaceStudy', '1')
    assets.set_metadata_tag(target, 'ReferenceFront', 'IMG_1982.HEIC')
    assets.set_metadata_tag(target, 'ReferenceSide', 'IMG_1983.HEIC')
    assets.set_metadata_tag(target, 'ReferenceBack', 'IMG_1984.HEIC')
    if not assets.save_loaded_asset(target):
        raise RuntimeError('Could not save Runner male-face source')
    unreal.log('RUNNER_MALE_FACE_SOURCE_READY high_res=' +
               str(target.has_high_resolution_textures))
finally:
    sub.remove_object_to_edit(target)
