"""Read-only inspection of Runner MetaHuman body and wardrobe controls."""
import unreal

character = unreal.load_asset('/Game/Characters/MetaHumans/Runner/MH_Runner_Working')
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not sub.try_add_object_to_edit(character):
    raise RuntimeError('Runner MetaHuman not editable')
try:
    constraints = sub.get_body_constraints(character)
    unreal.log('RUNNER_BODY_CONSTRAINTS ' + str(constraints))
    unreal.log('RUNNER_BODY_CONSTRAINT_PROPS ' + str([x for x in dir(constraints) if not x.startswith('_')]))
    unreal.log('RUNNER_BODY_TYPE ' + str(character.fixed_body_type))
    unreal.log('RUNNER_BODY_KEYPOINTS ' + str(sub.get_preset_body_key_points(character)))
    unreal.log('RUNNER_BODY_DOC ' + str(sub.set_body_constraints.__doc__))
    unreal.log('RUNNER_FACE_DOC ' + str(sub.translate_face_landmarks.__doc__))
    unreal.log('RUNNER_TEXTURE_DOC ' + str(sub.request_texture_sources.__doc__))
    unreal.log('RUNNER_ASSET_BODYSETTINGS ' + str([x for x in dir(character) if 'setting' in x.lower()]))
finally:
    sub.remove_object_to_edit(character)
