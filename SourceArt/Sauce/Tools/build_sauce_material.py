"""Author the flat tintable material shared by the sauce cup and puddle."""
import unreal

assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
folder = '/Game/Weapons/Sauce'
path = folder + '/M_SauceColor'
material = assets.load_asset(path) if assets.does_asset_exist(path) else None
if material is None:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_SauceColor', folder, unreal.Material, unreal.MaterialFactoryNew())
assert isinstance(material, unreal.Material)
editing.delete_all_material_expressions(material)

color = editing.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
color.set_editor_property('parameter_name', 'BaseColor')
color.set_editor_property('default_value', unreal.LinearColor(.83, .39, .055, 1))
assert editing.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)

roughness = editing.create_material_expression(material, unreal.MaterialExpressionScalarParameter)
roughness.set_editor_property('parameter_name', 'Roughness')
roughness.set_editor_property('default_value', .7)
assert editing.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)

editing.recompile_material(material)
assert assets.save_loaded_asset(material, only_if_is_dirty=False)
unreal.log('SAUCE_MATERIAL_READY ' + path)
