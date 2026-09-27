"""Import V2's cleaned-up rear neck as a separate review mesh."""

import unreal
import sys


name = ('SM_Runner_SamVideoRearFill' if '--rear' in sys.argv else
        'SM_Runner_SamVideoHeadVisualV6' if '--v6' in sys.argv else
        'SM_Runner_SamVideoHeadVisualV5' if '--v5' in sys.argv else
        'SM_Runner_SamVideoHeadVisualV4' if '--v4' in sys.argv else
        'SM_Runner_SamVideoHeadVisualV3' if '--v3' in sys.argv else
        'SM_Runner_SamVideoHeadVisualV2')
destination = '/Game/Characters/MetaHumans/Runner/VideoHeadVisual'
assets = unreal.EditorAssetLibrary
if assets.does_asset_exist(destination + '/' + name):
    raise RuntimeError('Refusing to replace ' + name)
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
material = assets.load_asset(destination + (
    '/M_RunnerSamVideoHairBrown' if '--rear' in sys.argv else
    '/M_RunnerSamVideoHeadBalanced'))
if not mesh or not material:
    raise RuntimeError('Fitted face or material import failed')
slots = list(mesh.get_editor_property('static_materials'))
if not slots:
    slots.append(unreal.StaticMaterial())
if '--v6' in sys.argv:
    if len(slots) < 3:
        raise RuntimeError('Expected three face material zones; got ' + str(len(slots)))
    zone_materials = [
        material,
        assets.load_asset(destination + '/M_RunnerSamVideoHairRoot'),
        assets.load_asset(destination + '/M_RunnerSamVideoNeckV2'),
    ]
    if not all(zone_materials):
        raise RuntimeError('Missing a face zone material')
    for slot, zone_material in zip(slots, zone_materials):
        slot.set_editor_property('material_interface', zone_material)
else:
    for slot in slots:
        slot.set_editor_property('material_interface', material)
mesh.set_editor_property('static_materials', slots)
assets.save_loaded_asset(mesh)
unreal.log('RUNNER_VIDEO_HEAD_V2_IMPORTED ' + str(mesh.get_bounds()))
