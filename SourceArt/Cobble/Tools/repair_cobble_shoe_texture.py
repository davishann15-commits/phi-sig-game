from pathlib import Path
import unreal

folder = '/Game/Characters/Cobble/C03'
source = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/Cobble/C03/SK_C03.fbm/shoes05_diffuse.png')
assert source.is_file()
assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.Enable 0')
task = unreal.AssetImportTask()
task.filename = str(source)
task.destination_path = folder
task.destination_name = 'shoes05_diffuse'
task.automated = True
task.replace_existing = False
task.save = True
task.set_editor_property('async_', False)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = assets.load_asset(folder + '/shoes05_diffuse')
assert isinstance(texture, unreal.Texture2D)
assert assets.save_loaded_asset(texture, only_if_is_dirty=False)
material = assets.load_asset(folder + '/C03_Body_shoes05')
assert isinstance(material, unreal.Material)
editing.delete_all_material_expressions(material)
sample = editing.create_material_expression(material, unreal.MaterialExpressionTextureSample)
sample.set_editor_property('texture', texture)
assert editing.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
roughness = editing.create_material_expression(material, unreal.MaterialExpressionConstant)
roughness.set_editor_property('r', .8)
assert editing.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
editing.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
editing.recompile_material(material)
# A null-RHI maintenance run has no compiled shader texture list. Verify the
# graph reference itself; the rendered lobby check validates the shader.
assert sample.get_editor_property('texture') == texture
assert assets.save_loaded_asset(material, only_if_is_dirty=False)
unreal.log('COBBLE_SHOE_TEXTURE_REPAIRED')
