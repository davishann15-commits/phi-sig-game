"""Photo-guided complexion and strawberry-brown hair for the separate face study."""
import unreal

assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
command_line = unreal.SystemLibrary.get_command_line()
variant = 'V4' if '-RunnerFaceStudyV4' in command_line else \
    'V3' if '-RunnerFaceStudyV3' in command_line else \
    'V2' if '-RunnerFaceStudyV2' in command_line else ''
root = '/Game/MetaHumans/RunnerFaceStudy' + variant + '/MH_Runner_FaceStudy' + variant + '/'
for suffix in ('Hair', 'Hair_Cards', 'Hair_Helmet'):
    path = root + 'Grooms/MI_WI_Hair_S_Casual_' + suffix
    material = assets.load_asset(path)
    if not material:
        raise RuntimeError('Face-study hair material missing: ' + path)
    for name, value in (
        ('hairMelanin', 0.70), ('hairRedness', 0.68), ('RedVariation', 0.08),
        ('OmbreMelanin', 0.72), ('OmbreRedness', 0.64),
        ('HighlightsMelanin', 0.62), ('HighlightsRedness', 0.64),
        ('RegionMelanin', 0.70), ('RegionRedness', 0.67),
    ):
        editing.set_material_instance_scalar_parameter_value(material, name, value)
    if not assets.save_loaded_asset(material):
        raise RuntimeError('Could not save face-study hair material: ' + path)

skin_root = root + 'Face/Materials/'
skin_tint = unreal.LinearColor(1.26, 1.29, 1.33, 1.0) if variant in ('V3', 'V4') else \
    unreal.LinearColor(1.18, 1.30, 1.45, 1.0)
for lod in ('LOD1', 'LOD3', 'LOD5to7'):
    path = skin_root + 'MI_Face_Skin_Baked_' + lod + '_VT'
    material = assets.load_asset(path)
    if not material:
        raise RuntimeError('Face-study skin material missing: ' + path)
    editing.set_material_instance_vector_parameter_value(
        material, 'Basecolor Global Multiply Post-Bake', skin_tint)
    if not assets.save_loaded_asset(material):
        raise RuntimeError('Could not save face-study skin material: ' + path)
body_path = '/Game/MetaHumans/RunnerSlimBody/MH_Runner_SlimBody/Body/Materials/MI_Body_Baked_VT'
body_material = assets.load_asset(body_path)
if body_material:
    editing.set_material_instance_vector_parameter_value(
        body_material, 'Basecolor Global Multiply Post-Bake', skin_tint)
    if not assets.save_loaded_asset(body_material):
        raise RuntimeError('Could not save slim-body skin material')
print('RUNNER_FACE_STUDY_STYLE_SAVED')
