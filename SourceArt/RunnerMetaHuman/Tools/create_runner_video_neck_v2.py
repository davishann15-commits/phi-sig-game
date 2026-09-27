"""Warm the review neck to the directly video-fitted jaw/ear tone."""

import unreal


destination = '/Game/Characters/MetaHumans/Runner/VideoHeadVisual'
name = 'M_RunnerSamVideoNeckV2'
path = destination + '/' + name
assets = unreal.EditorAssetLibrary
if assets.does_asset_exist(path):
    raise RuntimeError('Refusing to replace ' + path)
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    name, destination, unreal.Material, unreal.MaterialFactoryNew())
edit = unreal.MaterialEditingLibrary
color = edit.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -250, 0)
color.set_editor_property('constant', unreal.LinearColor(.19, .105, .073, 1.0))
edit.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
rough = edit.create_material_expression(material, unreal.MaterialExpressionConstant, -250, 190)
rough.set_editor_property('r', .85)
edit.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
edit.recompile_material(material)
assets.save_loaded_asset(material)
unreal.log('RUNNER_VIDEO_NECK_V2_READY')
