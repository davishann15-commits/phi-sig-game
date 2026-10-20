"""Import the two leaf atlases and a dedicated foliage master. No map saves."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parent
DEST = '/Game/SeniorSendOff/House/ExteriorNature'
A = unreal.EditorAssetLibrary
E = unreal.MaterialEditingLibrary
T = unreal.AssetToolsHelpers.get_asset_tools()

def node(m, kind, **properties):
    n = E.create_material_expression(m, getattr(unreal, 'MaterialExpression' + kind))
    for k, v in properties.items():
        n.set_editor_property(k, v)
    return n

def link(a, b, slot, output=''):
    assert E.connect_material_expressions(a, output, b, slot)

def out(n, prop, output=''):
    assert E.connect_material_property(n, output, getattr(unreal.MaterialProperty, 'MP_' + prop))

def custom(m, code, inputs, typ='CMOT_FLOAT3'):
    n = node(m, 'Custom', code=code, output_type=getattr(unreal.CustomMaterialOutputType, typ))
    data = []
    for name, _ in inputs:
        i = unreal.CustomInput()
        i.set_editor_property('input_name', name)
        data.append(i)
    n.set_editor_property('inputs', data)
    for name, source in inputs:
        link(source, n, name)
    return n

def scalar(m, name, value):
    return node(m, 'ScalarParameter', parameter_name=name, default_value=value)

def run():
    tasks = []
    for src, name in [('AutumnClusters.png', 'T_Nature_LeafClusters'), ('AutumnSingleLeaves.png', 'T_Nature_LeafSingles')]:
        task = unreal.AssetImportTask()
        for k, v in dict(filename=str(ROOT/'assets'/src), destination_path=DEST,
                         destination_name=name, automated=True, replace_existing=True, save=False).items():
            task.set_editor_property(k, v)
        tasks.append(task)
    T.import_asset_tasks(tasks)
    textures = []
    for task in tasks:
        paths = task.get_editor_property('imported_object_paths')
        assert len(paths) == 1, paths
        tex = A.load_asset(paths[0])
        tex.set_editor_property('srgb', True)
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property('power_of_two_mode', unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
        tex.set_editor_property('max_texture_size', 2048)
        tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_SHARPEN0)
        tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
        tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
        tex.set_editor_property('do_scale_mips_for_alpha_coverage', True)
        tex.set_editor_property('alpha_coverage_thresholds', unreal.Vector4(0, 0, 0, .42))
        assert A.save_loaded_asset(tex, False)
        textures.append(tex)

    name = 'M_NatureTreeFoliage'
    m = A.load_asset(DEST+'/'+name) if A.does_asset_exist(DEST+'/'+name) else T.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    E.delete_all_material_expressions(m)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property('opacity_mask_clip_value', .42)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    uv = node(m, 'TextureCoordinate')
    rect = node(m, 'VectorParameter', parameter_name='AtlasRect', default_value=unreal.LinearColor(0, 0, 1, 1))
    tile = custom(m, 'return Rect.xy+UV*Rect.zz;', [('Rect', rect), ('UV', uv)], 'CMOT_FLOAT2')
    base = node(m, 'TextureSampleParameter2D', parameter_name='BaseMap', texture=textures[1], sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    # UE names the UV input differently between releases.
    uv_input = next(v for v in E.get_material_expression_input_names(base) if 'uv' in v.lower() or 'coordinates' in v.lower())
    link(tile, base, uv_input)
    tint = node(m, 'VectorParameter', parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    color = node(m, 'Multiply'); link(base, color, 'A'); link(tint, color, 'B'); out(color, 'BASE_COLOR')
    sss = node(m, 'Multiply'); link(color, sss, 'A'); link(scalar(m, 'Transmission', .42), sss, 'B'); out(sss, 'SUBSURFACE_COLOR')
    opacity = node(m, 'Multiply'); link(base, opacity, 'A', 'A'); link(node(m, 'PerInstanceFadeAmount'), opacity, 'B'); out(opacity, 'OPACITY_MASK')
    out(scalar(m, 'Roughness', .86), 'ROUGHNESS')
    out(scalar(m, 'Specular', .16), 'SPECULAR')
    flat = node(m, 'Constant3Vector', constant=unreal.LinearColor(0, 0, 1, 1))
    normal = node(m, 'Multiply'); link(flat, normal, 'A'); link(node(m, 'TwoSidedSign'), normal, 'B'); out(normal, 'NORMAL')
    wind = custom(m, '''float tip=saturate(1-UV.y);tip*=tip;
float phase=Clock*.85+P.x*.006+P.y*.004;
float gust=sin(phase)+sin(Clock*1.55+P.y*.011)*.23;
return float3(gust,sin(phase*.8+1.3)*.55,sin(phase*1.2)*.12)*Strength*tip;''',
        [('P', node(m, 'WorldPosition')), ('Clock', node(m, 'Time')), ('UV', uv), ('Strength', scalar(m, 'WindStrength', 1.8))])
    out(wind, 'WORLD_POSITION_OFFSET')
    E.recompile_material(m)
    assert A.save_loaded_asset(m, False)
    # Keep the indoor masters unchanged. Tree bark and little lawn props need
    # saved instancing permutations in a native game, not editor-only recompiles.
    prop_materials = []
    for suffix in ['Plain', 'Textured']:
        path = DEST+'/M_NatureProp'+suffix
        p = A.load_asset(path) if A.does_asset_exist(path) else A.duplicate_asset(
            '/Game/SeniorSendOff/House/Materials/M_House_'+suffix, path)
        assert p is not None
        p.set_editor_property('used_with_instanced_static_meshes', True)
        p.set_editor_property('two_sided', True)
        E.recompile_material(p)
        assert A.save_loaded_asset(p, False)
        prop_materials.append(p.get_path_name())
    report = {'status': 'PASS', 'textures': [v.get_path_name() for v in textures], 'material': m.get_path_name(),
              'prop_materials': prop_materials, 'map_edited': False}
    (ROOT/'evidence/import.json').write_text(json.dumps(report, indent=2))
    unreal.log('TREE_LEAF_POLISH_IMPORTED '+json.dumps(report))

run()
