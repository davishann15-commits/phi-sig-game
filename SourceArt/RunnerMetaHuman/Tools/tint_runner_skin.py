"""Make Runner's assembled face/body less orange to match the indoor photos.

This adjusts baked-material color correction only; it does not regenerate or
upload a face, change the source MetaHuman, or touch other characters.
"""
import unreal

assets = unreal.EditorAssetLibrary
base = '/Game/MetaHumans/RunnerCasualHairStudy/MH_Runner_CasualHairStudy/Face/Materials/'
paths = [base + 'MI_Face_Skin_Baked_' + lod + '_VT'
         for lod in ('LOD1', 'LOD3', 'LOD5to7')]
paths.append('/Game/MetaHumans/RunnerJerseyBody/MH_Runner_JerseyBody/Body/Materials/MI_Body_Baked_VT')
correction = unreal.LinearColor(1.08, 1.17, 1.26, 1.0)
parameter = 'Basecolor Global Multiply Post-Bake'
for path in paths:
    material = assets.load_asset(path)
    if not material:
        raise RuntimeError('Runner skin material missing: ' + path)
    before = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(material, parameter)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(material, parameter, correction)
    if not assets.save_loaded_asset(material):
        raise RuntimeError('Runner skin material did not save: ' + path)
    print('RUNNER_SKIN_CORRECTED', path, 'before', before, 'after', correction)
