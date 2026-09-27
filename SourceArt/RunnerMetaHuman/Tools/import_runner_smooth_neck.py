"""Import the flared collarbone insert with Runner's existing skin material."""

import unreal


name = 'SM_Runner_SamVideoCollarBlend'
destination = '/Game/Characters/MetaHumans/Runner/VideoHeadVisual'
assets = unreal.EditorAssetLibrary
if assets.does_asset_exist(destination + '/' + name):
    raise RuntimeError('Refusing to overwrite ' + name)
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.original_import_type = unreal.FBXImportType.FBXIT_STATIC_MESH
options.import_as_skeletal = False
options.import_materials = False
options.import_textures = False
options.static_mesh_import_data.set_editor_property('convert_scene', True)
options.static_mesh_import_data.set_editor_property('convert_scene_unit', False)
task = unreal.AssetImportTask()
task.filename = '/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman/' + name + '.fbx'
task.destination_path = destination
task.destination_name = name
task.automated = True
task.replace_existing = False
task.save = True
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = assets.load_asset(destination + '/' + name)
material = assets.load_asset(destination + '/M_RunnerSamVideoNeckV2')
if not mesh or not material:
    raise RuntimeError('Neck mesh or skin material missing')
slots = list(mesh.get_editor_property('static_materials'))
if not slots:
    slots.append(unreal.StaticMaterial())
for slot in slots:
    slot.set_editor_property('material_interface', material)
mesh.set_editor_property('static_materials', slots)
assets.save_loaded_asset(mesh)
unreal.log('RUNNER_SMOOTH_NECK_IMPORTED ' + str(mesh.get_bounds()))
