"""Import the transparent mask image used by the story HUD."""
import unreal

destination = '/Game/Items/HouseMask'
task = unreal.AssetImportTask()
task.filename = unreal.Paths.project_dir() + 'SourceArt/HouseMask/T_HouseMaskIcon.png'
task.destination_path = destination
task.destination_name = 'T_HouseMaskIcon'
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
icon = unreal.EditorAssetLibrary.load_asset(destination + '/T_HouseMaskIcon')
if not icon:
    raise RuntimeError('Mask HUD icon failed to import')
icon.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
icon.set_editor_property('never_stream', True)
if not unreal.EditorAssetLibrary.save_loaded_asset(icon):
    raise RuntimeError('Mask HUD icon could not be saved')
unreal.log('HOUSE_MASK_ICON_IMPORTED')
