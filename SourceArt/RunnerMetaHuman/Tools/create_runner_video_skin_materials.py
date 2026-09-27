"""Make non-destructive color-review materials for the video-fitted head."""

import unreal


destination = '/Game/Characters/MetaHumans/Runner/VideoHeadVisual'
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
edit = unreal.MaterialEditingLibrary


def new(name):
    path = destination + '/' + name
    if assets.does_asset_exist(path):
        raise RuntimeError('Review material already exists: ' + path)
    return tools.create_asset(name, destination, unreal.Material, unreal.MaterialFactoryNew())


def constant(material, position, value):
    node = edit.create_material_expression(material, unreal.MaterialExpressionConstant,
                                           position[0], position[1])
    node.set_editor_property('r', value)
    return node


neck = new('M_RunnerSamVideoNeckMatched')
neck_color = edit.create_material_expression(neck, unreal.MaterialExpressionConstant3Vector, -250, 0)
neck_color.set_editor_property('constant', unreal.LinearColor(.112, .066, .056, 1.0))
edit.connect_material_property(neck_color, '', unreal.MaterialProperty.MP_BASE_COLOR)
neck_rough = constant(neck, (-250, 200), .85)
edit.connect_material_property(neck_rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
edit.recompile_material(neck)
assets.save_loaded_asset(neck)

head = new('M_RunnerSamVideoHeadBalanced')
texture = assets.load_asset(destination + '/T_RunnerSamVideoHead')
sample = edit.create_material_expression(head, unreal.MaterialExpressionTextureSample, -700, 0)
sample.set_editor_property('texture', texture)
power = edit.create_material_expression(head, unreal.MaterialExpressionPower, -450, 0)
gamma = constant(head, (-680, 200), .69)
edit.connect_material_expressions(sample, 'RGB', power, 'Base')
edit.connect_material_expressions(gamma, '', power, 'Exp')
tint = edit.create_material_expression(head, unreal.MaterialExpressionMultiply, -150, 0)
gain = edit.create_material_expression(head, unreal.MaterialExpressionConstant3Vector, -420, 220)
gain.set_editor_property('constant', unreal.LinearColor(1.10, 1.06, 1.04, 1.0))
edit.connect_material_expressions(power, '', tint, 'A')
edit.connect_material_expressions(gain, '', tint, 'B')
edit.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)
rough = constant(head, (-150, 250), .8)
edit.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
edit.recompile_material(head)
assets.save_loaded_asset(head)
unreal.log('RUNNER_VIDEO_SKIN_MATERIALS_READY')
