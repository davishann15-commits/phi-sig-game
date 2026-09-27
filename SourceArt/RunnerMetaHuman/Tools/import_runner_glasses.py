"""Import Runner's reference sunglasses as an independently head-attached mesh."""
import unreal

DEST = '/Game/MetaHumans/RunnerRebuild/MH_Runner_Working/Details/RunnerOutfit'
NAME = 'SM_RunnerSunglasses'
SOURCE = ('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/'
          'SourceArt/RunnerMetaHuman/SM_RunnerSunglasses.fbx')
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
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
options.static_mesh_import_data.set_editor_property('import_uniform_scale', 1.0)
task = unreal.AssetImportTask()
task.filename = SOURCE
task.destination_path = DEST
task.destination_name = NAME
task.automated = True
task.replace_existing = True
task.save = True
task.options = options
tools.import_asset_tasks([task])
mesh = assets.load_asset(DEST + '/' + NAME)
if not mesh:
    raise RuntimeError('Runner sunglasses did not import')
materials = list(mesh.get_editor_property('static_materials'))
frame = assets.load_asset('/Game/MetaHumans/RunnerRebuild/MH_Runner_Working/Details/RunnerOutfit/M_Runner_GlassesMetal')
lens = assets.load_asset('/Game/Characters/Cobble/C02/Cobble_sunglass_lens_001')
if not frame or not lens:
    raise RuntimeError('Runner glasses materials unavailable')
for index, slot in enumerate(materials):
    slot.set_editor_property('material_interface', frame if index == 0 else lens)
mesh.set_editor_property('static_materials', materials)
if not assets.save_loaded_asset(mesh):
    raise RuntimeError('Could not save Runner sunglasses')
unreal.log('RUNNER_GLASSES_IMPORTED bounds=' + str(mesh.get_bounds()) +
           ' materials=' + str(len(materials)))
