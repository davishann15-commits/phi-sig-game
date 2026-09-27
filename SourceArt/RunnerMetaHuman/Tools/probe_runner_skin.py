"""Read-only inspection of Runner's assembled skin and source appearance controls."""
import unreal

source = unreal.load_asset('/Game/Characters/MetaHumans/Runner/MH_Runner_MaleFace')
editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
print('RUNNER_SKIN_SOURCE_METHODS', [name for name in dir(editor)
      if any(word in name.lower() for word in ('skin', 'appearance', 'complexion', 'color', 'tone'))])
print('RUNNER_SKIN_SOURCE_PROPERTIES', [name for name in dir(source)
      if any(word in name.lower() for word in ('skin', 'appearance', 'complexion', 'color', 'tone'))])
print('RUNNER_FACE_STYLE_PROPERTIES', [name for name in dir(source)
      if any(word in name.lower() for word in ('makeup', 'lip', 'brow', 'eye', 'face'))])
for name in ('makeup_settings', 'face_settings', 'face_feature_settings'):
    try:
        value = source.get_editor_property(name)
        print('RUNNER_FACE_STYLE', name, value)
    except Exception:
        pass
settings = source.get_editor_property('skin_settings')
print('RUNNER_SKIN_SETTINGS', settings)
print('RUNNER_SKIN_SETTINGS_FIELDS', [name for name in dir(settings) if not name.startswith('_')])
print('RUNNER_MATERIAL_LIBRARY_METHODS', [name for name in dir(unreal.MaterialEditingLibrary)
      if 'parameter' in name.lower()])

for path in (
    '/Game/MetaHumans/RunnerMaleFace/MH_Runner_MaleFace/Face/Materials/MI_Face_Skin_Baked_LOD1_VT',
    '/Game/MetaHumans/RunnerJerseyBody/MH_Runner_JerseyBody/Body/Materials/MI_Body_Baked_VT',
):
    material = unreal.load_asset(path)
    print('RUNNER_SKIN_MATERIAL', path, material)
    if material:
        for name in ('Basecolor Global Multiply Post-Bake', 'Basecolor Global Value Post-Bake',
                     'Basecolor Global Hue Post-Bake', 'Basecolor Global Saturation Post-Bake'):
            try:
                print('RUNNER_SKIN_VALUE', path, name,
                      unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(material, name)
                      if 'Multiply' in name else
                      unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(material, name))
            except Exception as exc:
                print('RUNNER_SKIN_VALUE_ERROR', name, exc)
        for kind, function in (('VECTORS', unreal.MaterialEditingLibrary.get_vector_parameter_names),
                               ('SCALARS', unreal.MaterialEditingLibrary.get_scalar_parameter_names)):
            try:
                names = function(material)
                print('RUNNER_SKIN_' + kind, path, names)
            except Exception as exc:
                print('RUNNER_SKIN_NAMES_ERROR', kind, exc)
        parent = material.get_editor_property('parent')
        print('RUNNER_SKIN_PARENT', parent)
        if parent:
            print('RUNNER_SKIN_PARENT_METHODS', [name for name in dir(parent) if 'parameter' in name.lower()])
            try:
                print('RUNNER_SKIN_PARENT_EXPRESSIONS', [(str(type(item)), str(item.get_name()))
                      for item in parent.get_editor_property('expressions') if 'Parameter' in str(type(item))])
            except Exception as exc:
                print('RUNNER_SKIN_PARENT_ERROR', exc)
        print('RUNNER_SKIN_MATERIAL_METHODS', [name for name in dir(material) if 'parameter' in name.lower()])
        try:
            print('RUNNER_SKIN_VECTORS', material.get_editor_property('vector_parameter_values'))
        except Exception as exc:
            print('RUNNER_SKIN_VECTOR_ERROR', exc)
