"""Set provisional tall, slim Runner proportions on MetaHuman source."""
import unreal

PATH = '/Game/Characters/MetaHumans/Runner/MH_Runner_Working'
assets = unreal.EditorAssetLibrary
character = assets.load_asset(PATH)
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not sub.try_add_object_to_edit(character):
    raise RuntimeError('Runner not editable')
try:
    constraints = sub.get_body_constraints(character)
    targets = {
        'Height': 190.0, 'Inseam': 83.0, 'Shoulder Height': 163.0,
        'Chest': 96.0, 'Waist': 77.0, 'Hip': 94.0,
        'Thigh': 53.0, 'Calf': 36.0, 'Across Shoulder': 39.0,
        'Neck to Waist': 41.0,
    }
    for index, item in enumerate(constraints):
        key = str(item.name)
        if key in targets:
            item.is_active = True
            item.target_measurement = targets[key]
            constraints[index] = item
            unreal.log('RUNNER_BODY_TARGET ' + key + ' ' + str(item))
    if not any(c.is_active for c in constraints):
        raise RuntimeError('No body constraint matched')
    sub.set_body_constraints(character, constraints)
    sub.commit_body_state(character)
    after = sub.get_body_constraints(character)
    unreal.log('RUNNER_BODY_AFTER ' + str([(str(c.name), c.target_measurement, c.is_active) for c in after if c.is_active]))
    if not any(str(c.name) == 'Height' and c.is_active and abs(c.target_measurement-190.0)<.1 for c in after):
        raise RuntimeError('Height constraint did not persist')
    assets.set_metadata_tag(character, 'RunnerBodyFitVersion', '1')
    assets.save_loaded_asset(character)
    unreal.log('RUNNER_BODY_SAVED')
finally:
    sub.remove_object_to_edit(character)
