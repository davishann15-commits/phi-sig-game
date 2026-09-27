"""Tune only Runner's assembled materials toward the supplied complexion.

No photos or face data leave this machine. Material overrides are reversible.
"""
import unreal

assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
hair_root = '/Game/MetaHumans/RunnerCasualHairStudy/MH_Runner_CasualHairStudy/Grooms/'
for suffix in ('Hair', 'Hair_Cards', 'Hair_Helmet'):
    path = hair_root + 'MI_WI_Hair_S_Casual_' + suffix
    material = assets.load_asset(path)
    if not material:
        raise RuntimeError('Runner hair material missing: ' + path)
    for name, value in (
        ('hairMelanin', 0.38), ('hairRedness', 0.68), ('RedVariation', 0.10),
        ('OmbreMelanin', 0.39), ('OmbreRedness', 0.64),
        ('HighlightsMelanin', 0.29), ('HighlightsRedness', 0.60),
        ('RegionMelanin', 0.38), ('RegionRedness', 0.66),
    ):
        editing.set_material_instance_scalar_parameter_value(material, name, value)
    if not assets.save_loaded_asset(material):
        raise RuntimeError('Could not save Runner hair material: ' + path)
    print('RUNNER_STRAWBERRY_HAIR', path)

face = '/Game/MetaHumans/RunnerCasualHairStudy/MH_Runner_CasualHairStudy/Face/Materials/'
skin_paths = [face + 'MI_Face_Skin_Baked_' + lod + '_VT'
              for lod in ('LOD1', 'LOD3', 'LOD5to7')]
skin_paths.append('/Game/MetaHumans/RunnerJerseyBody/MH_Runner_JerseyBody/Body/Materials/MI_Body_Baked_VT')
light_fair = unreal.LinearColor(1.22, 1.31, 1.46, 1.0)
for path in skin_paths:
    material = assets.load_asset(path)
    if not material:
        raise RuntimeError('Runner skin material missing: ' + path)
    editing.set_material_instance_vector_parameter_value(
        material, 'Basecolor Global Multiply Post-Bake', light_fair)
    if not assets.save_loaded_asset(material):
        raise RuntimeError('Could not save Runner skin material: ' + path)
    print('RUNNER_FAIR_SKIN', path)
