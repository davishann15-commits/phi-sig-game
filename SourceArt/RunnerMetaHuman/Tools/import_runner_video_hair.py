"""Import Sam's video-inspired hair/neck into isolated Runner review assets."""

from pathlib import Path
import unreal


root = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman')
destination = '/Game/Characters/MetaHumans/Runner/VideoHeadVisual'
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
editing = unreal.MaterialEditingLibrary


def material(name, rgb, roughness, two_sided=False):
    path = destination + '/' + name
    if assets.does_asset_exist(path):
        return assets.load_asset(path)
    else:
        result = tools.create_asset(name, destination, unreal.Material, unreal.MaterialFactoryNew())
    if not result:
        raise RuntimeError('Could not make ' + name)
    if two_sided:
        result.set_editor_property('two_sided', True)
    color = editing.create_material_expression(result, unreal.MaterialExpressionConstant3Vector, -250, 0)
    color.set_editor_property('constant', unreal.LinearColor(*rgb, 1.0))
    editing.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = editing.create_material_expression(result, unreal.MaterialExpressionConstant, -250, 200)
    rough.set_editor_property('r', roughness)
    editing.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    editing.recompile_material(result)
    assets.save_loaded_asset(result)
    return result


hair_materials = [
    material('M_RunnerSamVideoHairRoot', (.075, .050, .036), .92, True),
    material('M_RunnerSamVideoHairBrown', (.105, .071, .050), .87, True),
    material('M_RunnerSamVideoHairSoft', (.132, .092, .063), .84, True),
    material('M_RunnerSamVideoHairHighlight', (.175, .130, .090), .81, True),
]
neck_material = material('M_RunnerSamVideoNeck', (.47, .312, .265), .82)

unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
for asset_name, materials in (
    ('SM_Runner_SamVideoHair', hair_materials),
    ('SM_Runner_SamVideoNeck', [neck_material]),
):
    path = destination + '/' + asset_name
    if assets.does_asset_exist(path):
        raise RuntimeError('Asset already exists; refusing to overwrite ' + path)
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
    task.filename = str(root / (asset_name + '.fbx'))
    task.destination_path = destination
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = False
    task.save = True
    task.options = options
    tools.import_asset_tasks([task])
    mesh = assets.load_asset(path)
    if not mesh:
        raise RuntimeError('Import failed ' + asset_name)
    slots = list(mesh.get_editor_property('static_materials'))
    unreal.log('RUNNER_VIDEO_MATERIAL_SLOTS ' + asset_name + ': ' + str(len(slots)))
    while len(slots) < len(materials):
        slots.append(unreal.StaticMaterial())
    for slot, mat in zip(slots, materials):
        slot.set_editor_property('material_interface', mat)
    mesh.set_editor_property('static_materials', slots)
    assets.save_loaded_asset(mesh)
    unreal.log('RUNNER_VIDEO_ASSET_IMPORTED ' + path + ' ' + str(mesh.get_bounds()))

texture = assets.load_asset(destination + '/T_RunnerSamVideoHead')
if not texture:
    raise RuntimeError('Video-fitted head texture was not imported')
name = 'M_RunnerSamVideoHeadBright'
path = destination + '/' + name
if assets.does_asset_exist(path):
    raise RuntimeError('Refusing to overwrite ' + path)
result = tools.create_asset(name, destination, unreal.Material, unreal.MaterialFactoryNew())
sample = editing.create_material_expression(result, unreal.MaterialExpressionTextureSample, -500, 0)
sample.set_editor_property('texture', texture)
tint = editing.create_material_expression(result, unreal.MaterialExpressionMultiply, -100, 0)
color = editing.create_material_expression(result, unreal.MaterialExpressionConstant3Vector, -350, 210)
color.set_editor_property('constant', unreal.LinearColor(1.52, 1.48, 1.44, 1.0))
editing.connect_material_expressions(sample, 'RGB', tint, 'A')
editing.connect_material_expressions(color, '', tint, 'B')
editing.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)
rough = editing.create_material_expression(result, unreal.MaterialExpressionConstant, -100, 300)
rough.set_editor_property('r', .79)
editing.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
editing.recompile_material(result)
assets.save_loaded_asset(result)
unreal.log('RUNNER_VIDEO_HAIR_IMPORT_DONE')
