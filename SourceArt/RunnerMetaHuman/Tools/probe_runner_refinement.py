"""Read-only options for photo-guided Runner hair, body and face refinement."""
import unreal

assets = unreal.EditorAssetLibrary
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
source = assets.load_asset('/Game/Characters/MetaHumans/Runner/MH_Runner_CasualHairStudy')
if not source or not sub.try_add_object_to_edit(source):
    raise RuntimeError('Runner casual study unavailable')
try:
    print('RUNNER_REFINEMENT_BODY', [(c.name, c.target_measurement, c.is_active)
          for c in sub.get_body_constraints(source)])
    print('RUNNER_REFINEMENT_LANDMARKS',
          [(i, round(p.x, 2), round(p.y, 2), round(p.z, 2))
           for i, p in enumerate(sub.get_face_landmarks(source))])
    print('RUNNER_REFINEMENT_FRECKLES', source.get_editor_property('skin_settings').freckles)
finally:
    sub.remove_object_to_edit(source)

root = '/Game/MetaHumans/RunnerCasualHairStudy/MH_Runner_CasualHairStudy/Grooms/'
for suffix in ('Hair', 'Hair_Cards'):
    material = assets.load_asset(root + 'MI_WI_Hair_S_Casual_' + suffix)
    if not material:
        print('RUNNER_REFINEMENT_HAIR_MISSING', suffix)
        continue
    vectors = unreal.MaterialEditingLibrary.get_vector_parameter_names(material)
    scalars = unreal.MaterialEditingLibrary.get_scalar_parameter_names(material)
    print('RUNNER_REFINEMENT_HAIR_VECTORS', suffix,
          [(str(name), str(unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(material, name)))
           for name in vectors])
    print('RUNNER_REFINEMENT_HAIR_SCALARS', suffix,
          [(str(name), unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(material, name))
           for name in scalars if any(part in str(name).lower()
                                  for part in ('melanin', 'red', 'color', 'dye', 'gloss', 'rough'))])
