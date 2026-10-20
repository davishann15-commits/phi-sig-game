"""Install front-bed foliage only. Never edit the map or other vegetation masters."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parent
DEST = '/Game/SeniorSendOff/House/FrontBushDetail'
A = unreal.EditorAssetLibrary
E = unreal.MaterialEditingLibrary
T = unreal.AssetToolsHelpers.get_asset_tools()

def node(material, kind, **properties):
    expression = E.create_material_expression(material, getattr(unreal, 'MaterialExpression' + kind))
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression

def link(source, target, slot, output=''):
    assert E.connect_material_expressions(source, output, target, slot)

def output(expression, property_name, slot=''):
    assert E.connect_material_property(expression, slot, getattr(unreal.MaterialProperty, 'MP_' + property_name))

def scalar(material, name, value):
    return node(material, 'ScalarParameter', parameter_name=name, default_value=value)

def custom(material, code, inputs, output_type='CMOT_FLOAT3'):
    expression = node(material, 'Custom', code=code, output_type=getattr(unreal.CustomMaterialOutputType, output_type))
    entries = []
    for name, _ in inputs:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        entries.append(entry)
    expression.set_editor_property('inputs', entries)
    for name, source in inputs:
        link(source, expression, name)
    return expression

def run():
    task = unreal.AssetImportTask()
    for key, value in dict(filename=str(ROOT/'assets/FrontShrubAtlas.png'), destination_path=DEST,
                           destination_name='T_FrontShrubAtlas', automated=True,
                           replace_existing=True, save=False).items():
        task.set_editor_property(key, value)
    T.import_asset_tasks([task])
    paths = task.get_editor_property('imported_object_paths')
    assert len(paths) == 1, paths
    texture = A.load_asset(paths[0])
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property('power_of_two_mode', unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
    texture.set_editor_property('max_texture_size', 2048)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_SHARPEN0)
    texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('do_scale_mips_for_alpha_coverage', True)
    texture.set_editor_property('alpha_coverage_thresholds', unreal.Vector4(0, 0, 0, .42))
    assert A.save_loaded_asset(texture, False)

    name = 'M_FrontShrubFoliage'
    material = A.load_asset(DEST+'/'+name) if A.does_asset_exist(DEST+'/'+name) else T.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    E.delete_all_material_expressions(material)
    material.set_editor_property('used_with_instanced_static_meshes', True)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property('opacity_mask_clip_value', .42)
    material.set_editor_property('two_sided', True)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    uv = node(material, 'TextureCoordinate')
    rect = node(material, 'VectorParameter', parameter_name='AtlasRect', default_value=unreal.LinearColor(0, 0, 1, 1))
    tile = custom(material, 'return Rect.xy + UV * Rect.zz;', [('Rect', rect), ('UV', uv)], 'CMOT_FLOAT2')
    base = node(material, 'TextureSampleParameter2D', parameter_name='BaseMap', texture=texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    uv_input = next(value for value in E.get_material_expression_input_names(base) if 'uv' in value.lower() or 'coordinates' in value.lower())
    link(tile, base, uv_input)
    tint = node(material, 'VectorParameter', parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    color = node(material, 'Multiply'); link(base, color, 'A'); link(tint, color, 'B')
    # One plant shares a coherent slight tint instead of every leaf being a different hue.
    variation = custom(material, 'return lerp(.88, 1.08, Seed);', [('Seed', node(material, 'PerInstanceRandom'))], 'CMOT_FLOAT1')
    final_color = node(material, 'Multiply'); link(color, final_color, 'A'); link(variation, final_color, 'B'); output(final_color, 'BASE_COLOR')
    transmission = node(material, 'Multiply'); link(final_color, transmission, 'A'); link(scalar(material, 'Transmission', .38), transmission, 'B'); output(transmission, 'SUBSURFACE_COLOR')
    opacity = node(material, 'Multiply'); link(base, opacity, 'A', 'A'); link(node(material, 'PerInstanceFadeAmount'), opacity, 'B'); output(opacity, 'OPACITY_MASK')
    output(scalar(material, 'Roughness', .68), 'ROUGHNESS')
    output(scalar(material, 'Specular', .20), 'SPECULAR')
    flat = node(material, 'Constant3Vector', constant=unreal.LinearColor(0, 0, 1, 1))
    normal = node(material, 'Multiply'); link(flat, normal, 'A'); link(node(material, 'TwoSidedSign'), normal, 'B'); output(normal, 'NORMAL')
    wind = custom(material, '''float pin = saturate(1 - UV.y); pin *= pin;
float phase = Clock * 1.05 + P.x * .007 + P.y * .005;
float breeze = sin(phase) + .23 * sin(Clock * 1.8 + P.y * .014);
return float3(breeze, .5 * sin(phase + 1.1), .16 * sin(phase * 1.2)) * Strength * pin;''',
        [('P', node(material, 'WorldPosition')), ('Clock', node(material, 'Time')), ('UV', uv), ('Strength', scalar(material, 'WindStrength', .65))])
    output(wind, 'WORLD_POSITION_OFFSET')
    E.recompile_material(material)
    assert A.save_loaded_asset(material, False)
    report = {'status': 'PASS', 'texture': texture.get_path_name(), 'material': material.get_path_name(), 'map_edited': False,
              'scope': 'New front-planting-bed assets only; all existing foliage masters untouched'}
    (ROOT/'evidence/import.json').write_text(json.dumps(report, indent=2))
    unreal.log('FRONT_BUSH_IMPORTED ' + json.dumps(report))

run()
