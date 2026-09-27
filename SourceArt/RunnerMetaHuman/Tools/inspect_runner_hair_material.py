"""Read Runner's current groom color controls without changing assets."""

import unreal


path = ('/Game/MetaHumans/RunnerRebuild/MH_Runner_Working/Grooms/'
        'MI_WI_Hair_S_Messy_None_1_Hair')
material = unreal.EditorAssetLibrary.load_asset(path)
if not material:
    raise RuntimeError('Runner hair material not found')
unreal.log('RUNNER_HAIR_MATERIAL ' + material.get_path_name())
unreal.log('RUNNER_HAIR_PARENT ' + str(material.get_editor_property('parent')))
while material:
    unreal.log('RUNNER_HAIR_LAYER ' + material.get_path_name())
    if not isinstance(material, unreal.MaterialInstanceConstant):
        break
    for field in ('scalar_parameter_values', 'vector_parameter_values', 'texture_parameter_values'):
        values = material.get_editor_property(field)
        unreal.log('RUNNER_HAIR_' + field.upper() + ' ' + str(values))
    unreal.log('RUNNER_HAIR_ALL_SCALARS ' + str(
        unreal.MaterialEditingLibrary.get_scalar_parameter_names(material)))
    material = material.get_editor_property('parent')
