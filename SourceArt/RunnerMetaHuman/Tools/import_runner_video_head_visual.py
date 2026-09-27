"""Import video-fitted head/texture as an isolated visual-review asset."""

import unreal


assets = unreal.EditorAssetLibrary
tool = unreal.AssetToolsHelpers.get_asset_tools()
source_root = "/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman/"
destination = "/Game/Characters/MetaHumans/Runner/VideoHeadVisual"
mesh_name = "SM_Runner_SamVideoHeadVisual"
texture_name = "T_RunnerSamVideoHead"
material_name = "M_RunnerSamVideoHead"
for name in (mesh_name, texture_name, material_name):
    if assets.does_asset_exist(destination + "/" + name):
        raise RuntimeError("Asset already exists; refusing to replace " + name)

texture_task = unreal.AssetImportTask()
texture_task.filename = source_root + texture_name + ".png"
texture_task.destination_path = destination
texture_task.destination_name = texture_name
texture_task.automated = True
texture_task.replace_existing = False
texture_task.save = True
tool.import_asset_tasks([texture_task])
texture = assets.load_asset(destination + "/" + texture_name)
if not texture:
    raise RuntimeError("Video head texture import failed")

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
mesh_task = unreal.AssetImportTask()
mesh_task.filename = source_root + mesh_name + ".fbx"
mesh_task.destination_path = destination
mesh_task.destination_name = mesh_name
mesh_task.automated = True
mesh_task.replace_existing = False
mesh_task.save = True
mesh_task.options = options
tool.import_asset_tasks([mesh_task])
mesh = assets.load_asset(destination + "/" + mesh_name)
if not mesh:
    raise RuntimeError("Video head mesh import failed")

material = tool.create_asset(material_name, destination, unreal.Material, unreal.MaterialFactoryNew())
if not material:
    raise RuntimeError("Could not create video head material")
editing = unreal.MaterialEditingLibrary
sample = editing.create_material_expression(material, unreal.MaterialExpressionTextureSample, -300, 0)
sample.set_editor_property("texture", texture)
tint = editing.create_material_expression(material, unreal.MaterialExpressionMultiply, -80, 0)
gain = editing.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 210)
gain.set_editor_property("r", 1.25)
editing.connect_material_expressions(sample, "RGB", tint, "A")
editing.connect_material_expressions(gain, "", tint, "B")
editing.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
roughness = editing.create_material_expression(material, unreal.MaterialExpressionConstant, -100, 300)
roughness.set_editor_property("r", 0.78)
editing.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
editing.recompile_material(material)
assets.save_loaded_asset(material)

slots = list(mesh.get_editor_property("static_materials"))
if not slots:
    slots = [unreal.StaticMaterial()]
for slot in slots:
    slot.set_editor_property("material_interface", material)
mesh.set_editor_property("static_materials", slots)
assets.save_loaded_asset(mesh)
unreal.log("RUNNER_VIDEO_HEAD_VISUAL_IMPORTED " + str(mesh.get_bounds()))
