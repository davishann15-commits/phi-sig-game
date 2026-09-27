"""Import the new local head geometry for a non-destructive fit study."""

import unreal


SOURCE = "/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman/SM_Runner_SamVideoHeadTargetCM.fbx"
DEST = "/Game/Characters/MetaHumans/Runner"
NAME = "SM_Runner_SamVideoHeadTargetCM"
assets = unreal.EditorAssetLibrary
if assets.does_asset_exist(DEST + "/" + NAME):
    raise RuntimeError("Head conform target already exists; refusing to replace it")

unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.original_import_type = unreal.FBXImportType.FBXIT_STATIC_MESH
options.import_as_skeletal = False
options.import_materials = False
options.import_textures = False
options.static_mesh_import_data.set_editor_property("convert_scene", True)
options.static_mesh_import_data.set_editor_property("convert_scene_unit", False)
options.static_mesh_import_data.set_editor_property("import_uniform_scale", 1.0)
task = unreal.AssetImportTask()
task.filename = SOURCE
task.destination_path = DEST
task.destination_name = NAME
task.automated = True
task.replace_existing = False
task.save = True
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = assets.load_asset(DEST + "/" + NAME)
if not mesh:
    raise RuntimeError("Head target import failed")
unreal.log("RUNNER_VIDEO_HEAD_TARGET_IMPORTED " + str(mesh.get_bounds()))
