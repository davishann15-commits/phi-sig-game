"""Create a dark under-hair scalp material for the enlarged Runner head."""

import unreal


destination = '/Game/Characters/MetaHumans/Runner/VideoHeadVisual'
name = 'M_RunnerSamVideoScalpShadow'
assets = unreal.EditorAssetLibrary
path = destination + '/' + name
if assets.does_asset_exist(path):
    raise RuntimeError('Refusing to overwrite ' + path)
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    name, destination, unreal.Material, unreal.MaterialFactoryNew())
if not material:
    raise RuntimeError('Unable to create under-hair material')
editing = unreal.MaterialEditingLibrary
base = editing.create_material_expression(
    material, unreal.MaterialExpressionConstant3Vector, -250, 0)
base.set_editor_property('constant', unreal.LinearColor(.018, .010, .006, 1.0))
editing.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
roughness = editing.create_material_expression(
    material, unreal.MaterialExpressionConstant, -250, 190)
roughness.set_editor_property('r', .91)
editing.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
editing.recompile_material(material)
assets.save_loaded_asset(material)
unreal.log('RUNNER_SCALP_SHADOW_MATERIAL_CREATED ' + path)
