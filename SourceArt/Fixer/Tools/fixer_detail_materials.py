"""Isolated post-assembly surface pass for Fixer. Run inside Unreal Editor.

All writes are restricted to /Game/MetaHumans/Fixer/MH_Fixer. Shared assets and
the original Cobble arm material are read-only inputs. No network requests.
"""
import json
import traceback
from pathlib import Path

import unreal

ROOT = '/Game/MetaHumans/Fixer/MH_Fixer'
DETAILS = ROOT + '/Details'
REPORT_PATH = Path('/private/tmp/fixer_material_report.json')
ASSETS = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
REPORT = {'root': ROOT, 'materials': [], 'clothes': [], 'errors': []}


def scoped(asset):
    if not asset or not asset.get_path_name().split('.')[0].startswith(ROOT + '/'):
        raise RuntimeError('Refusing write outside Fixer: ' + str(asset))
    return asset


def save(asset):
    scoped(asset)
    if not ASSETS.save_loaded_asset(asset):
        raise RuntimeError('Failed to save ' + asset.get_path_name())


def node(material, kind, **properties):
    scoped(material)
    expression = EDIT.create_material_expression(material, getattr(unreal, 'MaterialExpression' + kind))
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression


def connect(source, target, input_name, output_name=''):
    inputs = [str(value) for value in EDIT.get_material_expression_input_names(target)]
    outputs = [str(value) for value in EDIT.get_material_expression_output_names(source)]
    key = lambda value: value.replace(' ', '').lower()
    input_name = next((value for value in inputs if key(value) == key(input_name)), input_name)
    output_name = next((value for value in outputs if key(value) == key(output_name)), output_name)
    if not EDIT.connect_material_expressions(source, output_name, target, input_name):
        raise RuntimeError('Material connection failed: ' + input_name)


def output(source, property_name, output_name=''):
    if not EDIT.connect_material_property(source, output_name, getattr(unreal.MaterialProperty, property_name)):
        raise RuntimeError('Material output failed: ' + property_name)


def custom_input(name):
    value = unreal.CustomInput()
    value.set_editor_property('input_name', name)
    return value


def local_texture(source_path):
    name = source_path.rsplit('/', 1)[-1]
    destination = DETAILS + '/Textures/' + name
    texture = ASSETS.load_asset(destination)
    if not texture:
        source = ASSETS.load_asset(source_path)
        if not source:
            raise RuntimeError('Missing textile source: ' + source_path)
        texture = ASSETS.duplicate_asset(source_path, destination)
        if not texture:
            raise RuntimeError('Texture duplicate failed: ' + destination)
        save(texture)
    return texture


def texture_sample(material, texture, uv, normal=False):
    sampler = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    if texture.get_editor_property('compression_settings') == unreal.TextureCompressionSettings.TC_MASKS:
        sampler = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    elif not normal and not texture.get_editor_property('srgb'):
        sampler = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    sample = node(material, 'TextureSample', texture=texture, sampler_type=sampler)
    connect(uv, sample, 'UVs')
    return sample


def build_cloth_material(garment, color):
    """Real garment normal/AO plus low-contrast heather and fine cotton knit."""
    name = 'M_Fixer_' + garment
    material = ASSETS.load_asset(DETAILS + '/' + name)
    if not material:
        material = TOOLS.create_asset(name, DETAILS, unreal.Material, unreal.MaterialFactoryNew())
    scoped(material)
    if not isinstance(material, unreal.Material):
        raise RuntimeError('Expected Material: ' + name)
    EDIT.delete_all_material_expressions(material)
    material.set_editor_property('two_sided', True)
    material.set_editor_property('used_with_skeletal_mesh', True)
    material.set_editor_property('used_with_clothing', True)
    uv = node(material, 'TextureCoordinate')
    fine_uv = node(material, 'TextureCoordinate', u_tiling=85.0, v_tiling=85.0)
    macro_uv = node(material, 'TextureCoordinate', u_tiling=2.8, v_tiling=2.8)
    texture_root = '/MetaHumanCharacter/Optional/Clothing/DefaultGarment/ClothAssets/bodyShapeA/Textures/DG_bodyShapeA_' + garment
    normal = texture_sample(material, local_texture(texture_root + '_Normal'), uv, normal=True)
    ao = texture_sample(material, local_texture(texture_root + '_AO'), uv)
    micro = texture_sample(material, local_texture('/MetaHumanCharacter/Optional/Clothing/Common/Materials/Textures/Clothing/Micros/micro_knit_front_N'), fine_uv, normal=True)
    heather = texture_sample(material, local_texture('/MetaHumanCharacter/Optional/Clothing/Common/Materials/Textures/Clothing/Macros/macro_heather_04'), macro_uv)
    detail_normal = node(material, 'Custom', code='return normalize(float3(B.xy + N.xy * 0.22, B.z * N.z));',
                         output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                         inputs=[custom_input('B'), custom_input('N')])
    connect(normal, detail_normal, 'B', 'RGB')
    connect(micro, detail_normal, 'N', 'RGB')
    fabric = node(material, 'VectorParameter', parameter_name='FabricColor', default_value=unreal.LinearColor(*color, 1.0))
    base = node(material, 'Custom', code='return C * lerp(0.90, 1.06, H.r);',
                output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                inputs=[custom_input('C'), custom_input('H')])
    connect(fabric, base, 'C')
    connect(heather, base, 'H', 'RGB')
    output(base, 'MP_BASE_COLOR')
    output(detail_normal, 'MP_NORMAL')
    output(ao, 'MP_AMBIENT_OCCLUSION', 'R')
    output(node(material, 'Constant', r=0.86 if garment == 'Shirt' else 0.79), 'MP_ROUGHNESS')
    output(node(material, 'Constant', r=0.24), 'MP_SPECULAR')
    EDIT.recompile_material(material)
    save(material)
    return material


def apply_clothing(shirt, shorts):
    count = 0
    for path in ASSETS.list_assets(ROOT + '/Clothing', recursive=True):
        mesh = ASSETS.load_asset(path)
        if not isinstance(mesh, unreal.SkeletalMesh):
            continue
        scoped(mesh)
        slots = list(mesh.get_editor_property('materials'))
        replaced = []
        for index, slot in enumerate(slots):
            label = str(slot.get_editor_property('material_slot_name'))
            old = slot.get_editor_property('material_interface')
            lookup = (label + ' ' + (old.get_name() if old else '')).lower()
            material = shorts if 'short' in lookup else shirt if 'shirt' in lookup else None
            if material:
                slot.set_editor_property('material_interface', material)
                slots[index] = slot
                replaced.append({'slot': label, 'material': material.get_path_name()})
        if replaced:
            mesh.set_editor_property('materials', slots)
            save(mesh)
            REPORT['clothes'].append({'mesh': mesh.get_path_name(), 'slots': replaced})
            count += len(replaced)
    if count < 2:
        raise RuntimeError('Could not assign both Fixer garment surfaces; inspect native Clothing slots')


def style_native_materials():
    """Only mutate instances created inside the Fixer assembly."""
    hair_count = brow_count = skin_count = 0
    for path in ASSETS.list_assets(ROOT, recursive=True):
        if '/Details/' in path:
            continue
        material = ASSETS.load_asset(path)
        if not isinstance(material, unreal.MaterialInstanceConstant):
            continue
        scalar_names = {str(value) for value in EDIT.get_scalar_parameter_names(material)}
        vector_names = {str(value) for value in EDIT.get_vector_parameter_names(material)}
        name = material.get_name().lower()
        changes = {}
        if '/Grooms/' in path and ('hair' in name or 'eyebrow' in name):
            is_brow = 'eyebrow' in name
            # Warm copper/auburn with dark strand variation rather than dyed orange.
            melanin = 0.55 if is_brow else 0.52
            for key, value in {
                'hairMelanin': melanin, 'hairRedness': 0.90,
                'HairRoughness': 0.53, 'RedVariation': 0.08,
                'OmbreMelanin': melanin + 0.06, 'OmbreRedness': 0.86,
                'HighlightsMelanin': melanin - 0.07, 'HighlightsRedness': 0.86,
                'RegionMelanin': melanin, 'RegionRedness': 0.90,
                'WhiteAmount': 0.0, 'LightAmount': 0.0,
            }.items():
                if key in scalar_names:
                    EDIT.set_material_instance_scalar_parameter_value(scoped(material), key, value)
                    changes[key] = value
            if changes:
                if is_brow:
                    brow_count += 1
                else:
                    hair_count += 1
        if ('/Face/Materials/' in path or '/Body/Materials/' in path) and \
                ('skin' in name or 'body_baked' in name or 'head_baked' in name):
            # Keep the source skin U/V unchanged to retain downloaded animated
            # texture maps, then establish the lighter neutral-pink complexion
            # here while preserving the native pores, SSS and roughness inputs.
            key = 'Basecolor Global Multiply Post-Bake'
            if key in vector_names:
                tint = (1.12, 1.20, 1.28, 1.0)
                EDIT.set_material_instance_vector_parameter_value(scoped(material), key, unreal.LinearColor(*tint))
                changes[key] = tint
            key = 'Roughness Global Multiply Post-Bake'
            if key in scalar_names:
                EDIT.set_material_instance_scalar_parameter_value(scoped(material), key, 1.04)
                changes[key] = 1.04
            if changes:
                skin_count += 1
        # Eyes are brown in MH_Fixer's source settings. Do not recolor the whole
        # assembled eye texture: that would tint the sclera as well as the iris.
        if changes:
            EDIT.update_material_instance(material)
            save(material)
            REPORT['materials'].append({'path': material.get_path_name(), 'changes': changes,
                                        'parent': str(material.get_editor_property('parent'))})
    REPORT.update(hair_material_count=hair_count, eyebrow_material_count=brow_count, skin_material_count=skin_count)
    if not hair_count or not brow_count:
        raise RuntimeError('Missing independent Fixer hair or eyebrow material parameter overrides')
    if not skin_count:
        raise RuntimeError('Missing independent Fixer baked skin color-correction parameters')


def create_first_person_skin():
    source_path = '/Game/Characters/Cobble/C03/C03_Skin'
    path = DETAILS + '/MI_FixerFirstPersonSkin'
    material = ASSETS.load_asset(path)
    if not material:
        material = ASSETS.duplicate_asset(source_path, path)
    scoped(material)
    if not isinstance(material, unreal.Material):
        raise RuntimeError('First-person skin source is not the expected direct legacy Material')
    if ASSETS.get_metadata_tag(material, 'FixerFairTintVersion') != '1':
        base = EDIT.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
        base_output = EDIT.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_BASE_COLOR)
        if not base:
            raise RuntimeError('Legacy arm skin has no base-color input')
        tint = node(material, 'VectorParameter', parameter_name='FixerFairSkinTint',
                    default_value=unreal.LinearColor(1.10, 1.16, 1.22, 1.0))
        corrected = node(material, 'Custom', code='float L=dot(C,float3(0.2126,0.7152,0.0722)); return saturate(lerp(L.xxx,C,0.92)*T);',
                         output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                         inputs=[custom_input('C'), custom_input('T')])
        connect(base, corrected, 'C', str(base_output))
        connect(tint, corrected, 'T')
        output(corrected, 'MP_BASE_COLOR')
        # Every other connection remains from the duplicated C03 material,
        # including its own UV-compatible skin normal and roughness maps.
        material.set_editor_property('used_with_skeletal_mesh', True)
        ASSETS.set_metadata_tag(material, 'FixerFairTintVersion', '1')
        EDIT.recompile_material(material)
    save(material)
    REPORT['first_person_skin'] = {'path': material.get_path_name(), 'source': source_path,
        'normal': str(EDIT.get_material_property_input_node(material, unreal.MaterialProperty.MP_NORMAL)),
        'roughness': str(EDIT.get_material_property_input_node(material, unreal.MaterialProperty.MP_ROUGHNESS))}
    return material


def main():
    if not ASSETS.does_directory_exist(ROOT):
        raise RuntimeError('Fixer assembly does not exist: ' + ROOT)
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/MetaHumanCharacter/Optional'], force_rescan=True)
    shirt = build_cloth_material('Shirt', (0.010, 0.021, 0.046))
    shorts = build_cloth_material('Short', (0.46, 0.49, 0.49))
    apply_clothing(shirt, shorts)
    style_native_materials()
    create_first_person_skin()
    REPORT['complete'] = True
    unreal.log('FIXER_DETAIL_MATERIALS_COMPLETE ' + json.dumps(REPORT))


try:
    main()
except Exception:
    REPORT['complete'] = False
    REPORT['errors'].append(traceback.format_exc())
    unreal.log_error(REPORT['errors'][-1])
    raise
finally:
    REPORT_PATH.write_text(json.dumps(REPORT, indent=2))
