"""Import both reference flip-flops as foot-attached static meshes."""
import unreal

DEST = '/Game/MetaHumans/RunnerRebuild/MH_Runner_Working/Details/RunnerOutfit'
SOURCE_ROOT = ('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/'
               'SourceArt/RunnerMetaHuman/')
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
sole = assets.load_asset(DEST + '/M_Runner_BlackShorts')
strap = assets.load_asset(DEST + '/M_Runner_SandalBrown')
if not sole or not strap:
    raise RuntimeError('Runner sandal materials unavailable')
for side in ('L', 'R'):
    name = 'SM_RunnerSandal' + side
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
    task.filename = SOURCE_ROOT + name + '.fbx'
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    tools.import_asset_tasks([task])
    mesh = assets.load_asset(DEST + '/' + name)
    if not mesh:
        raise RuntimeError('Runner sandal did not import: ' + side)
    slots = list(mesh.get_editor_property('static_materials'))
    for index, slot in enumerate(slots):
        slot.set_editor_property('material_interface', sole if index == 0 else strap)
    mesh.set_editor_property('static_materials', slots)
    assets.save_loaded_asset(mesh)
    unreal.log('RUNNER_SANDAL_IMPORTED ' + side + ' bounds=' + str(mesh.get_bounds()))
