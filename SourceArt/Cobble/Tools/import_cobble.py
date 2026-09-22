import json
import os
from pathlib import Path
import re
import unreal


SOURCE = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/Cobble')
DEST = '/Game/Characters/Cobble'
UI = '/Game/Characters/UI'
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EDIT = unreal.MaterialEditingLibrary


def option(name, default):
    match = re.search(r'(?:^|\s)-' + re.escape(name) + r'=(?:"([^"]*)"|(\S+))',
                      unreal.SystemLibrary.get_command_line(), re.IGNORECASE)
    return (match.group(1) if match.group(1) is not None else match.group(2)) if match else default


requested = option('CobbleCharacters', os.environ.get('COBBLE_CHARACTERS', 'ALL'))
CHARACTERS = [f'C{i:02d}' for i in range(1, 9)] if requested.upper() == 'ALL' else requested.upper().split(',')
if not CHARACTERS or any(c not in [f'C{i:02d}' for i in range(1, 9)] for c in CHARACTERS):
    raise RuntimeError('CobbleCharacters must be ALL or C01,C02,...,C08')
FORCE_FRONT_X = option('CobbleForceFrontX', '1') != '0'
VERIFY_ONLY = option('CobbleVerifyOnly', '0') == '1'
HAIR_COLORS = ((.027,.020,.015),(.050,.027,.014),(.073,.029,.013),(.021,.010,.006),
               (.029,.017,.011),(.021,.011,.007),(.022,.012,.007),(.037,.019,.010))
IRIS_COLORS = ((.013,.010,.008),(.030,.019,.010),(.047,.045,.034),(.027,.020,.014),
               (.036,.025,.015),(.026,.021,.015),(.030,.024,.017),(.040,.027,.016))


def require_asset(path, asset_class):
    asset = ASSETS.load_asset(path)
    if not isinstance(asset, asset_class):
        raise RuntimeError(f'Expected {asset_class.__name__} at {path}, got {asset}')
    return asset


def save(asset):
    if not ASSETS.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Failed to save ' + asset.get_path_name())


def fbx_options(animation=False, skeleton=None):
    opts = unreal.FbxImportUI()
    opts.set_editor_property('automated_import_should_detect_type', False)
    kind = unreal.FBXImportType.FBXIT_ANIMATION if animation else unreal.FBXImportType.FBXIT_SKELETAL_MESH
    opts.set_editor_property('mesh_type_to_import', kind)
    opts.set_editor_property('original_import_type', kind)
    opts.set_editor_property('import_as_skeletal', True)
    opts.set_editor_property('import_mesh', not animation)
    opts.set_editor_property('import_animations', animation)
    opts.set_editor_property('import_materials', not animation)
    opts.set_editor_property('import_textures', not animation)
    opts.set_editor_property('create_physics_asset', False)
    opts.set_editor_property('override_full_name', True)
    opts.set_editor_property('reset_to_fbx_on_material_conflict', True)
    if skeleton:
        opts.set_editor_property('skeleton', skeleton)
    data = opts.get_editor_property('anim_sequence_import_data' if animation else 'skeletal_mesh_import_data')
    data.set_editor_property('convert_scene', True)
    data.set_editor_property('force_front_x_axis', FORCE_FRONT_X)
    data.set_editor_property('convert_scene_unit', True)
    data.set_editor_property('import_uniform_scale', 1.0)
    if animation:
        data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        data.set_editor_property('use_default_sample_rate', True)
        data.set_editor_property('import_bone_tracks', True)
        data.set_editor_property('import_custom_attribute', False)
    else:
        data.set_editor_property('import_morph_targets', False)
        data.set_editor_property('import_mesh_lods', False)
        data.set_editor_property('use_t0_as_ref_pose', False)
        # Each character owns its skeleton; update its fitted height/head reference
        # pose when regenerating, before importing that same character's animations.
        data.set_editor_property('update_skeleton_reference_pose', True)
        data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return opts


def import_asset(source, folder, name, expected_class, opts=None):
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    task.set_editor_property('async_', False)
    if opts:
        task.options = opts
        task.factory = unreal.FbxFactory()
        if expected_class == unreal.AnimSequence:
            opts.set_editor_property('override_animation_name', name)
    TOOLS.import_asset_tasks([task])
    results = task.get_objects()
    path = folder + '/' + name
    asset = require_asset(path, expected_class)
    if not any(item.get_path_name() == asset.get_path_name() for item in results):
        raise RuntimeError(f'Import did not return {path}; results={[item.get_path_name() for item in results]}')
    # Legacy FBX tasks only report the primary mesh. Persist its created dependency packages explicitly.
    if isinstance(asset, unreal.SkeletalMesh):
        skeleton = asset.get_editor_property('skeleton')
        if not skeleton:
            raise RuntimeError('Imported mesh has no skeleton: ' + path)
        save(skeleton)
        for slot in asset.get_editor_property('materials'):
            material = slot.get_editor_property('material_interface')
            if material:
                for texture in EDIT.get_material_used_textures(material):
                    if texture.get_path_name().startswith('/Game/'):
                        save(texture)
                save(material)
    return asset


def fix_masked_materials(folder):
    for path in ASSETS.list_assets(folder, recursive=True, include_folder=False):
        material = ASSETS.load_asset(path)
        if not isinstance(material, unreal.Material):
            continue
        textures = [t for t in EDIT.get_material_used_textures(material) if isinstance(t, unreal.Texture2D)]
        if not textures:
            continue
        names = ' '.join([material.get_name()] + [t.get_name() for t in textures]).lower()
        if not any(part in names for part in ('hair', 'brow', 'lash', 'short02')):
            continue
        color_textures = [t for t in textures if t.get_editor_property('srgb')]
        texture = color_textures[0] if color_textures else textures[0]
        sample = EDIT.create_material_expression(material, unreal.MaterialExpressionTextureSample)
        sample.set_editor_property('texture', texture)
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        material.set_editor_property('two_sided', True)
        material.set_editor_property('opacity_mask_clip_value', 0.3)
        if not EDIT.connect_material_property(sample, 'A', unreal.MaterialProperty.MP_OPACITY_MASK):
            raise RuntimeError('Unable to connect hair alpha: ' + path)
        EDIT.recompile_material(material)
        save(material)


def fit_constant_materials(cid, folder, meshes):
    # Atomic FBX reimport leaves newly added slots empty rather than creating
    # their materials. Reconstruct the groom/lens shaders from the source recipe.
    index = int(cid[1:])-1
    recipes = {
        cid+'_EyeWhite': ((.38,.35,.32),.32,.5),
        cid+'_Iris': (IRIS_COLORS[index],.31,.5),
        cid+'_Pupil': ((.001,.001,.001),.24,.5),
    }
    if cid=='C01':
        recipes[cid+'_Shorts']=((.020,.024,.020),.94,.08)
        recipes[cid+'_sage']=((.15,.19,.15),.90,.10)
    for number, factor in enumerate((.70,.84,1.,1.15,1.28,1.10)):
        recipes[cid+f'_HairDetail_{number}'] = (tuple(c*factor*.34 for c in HAIR_COLORS[index]),.79,.20)
    cache = {}
    for mesh in meshes:
        slots = mesh.get_editor_property('materials')
        for slot_index, slot in enumerate(slots):
            name = str(slot.get_editor_property('imported_material_slot_name'))
            recipe = recipes.get(name)
            if name.startswith('Cobble sunglass lens'):
                recipe = ((.009,.014,.018),.18,.5)
            if recipe:
                if name not in cache:
                    asset_name = re.sub(r'[^A-Za-z0-9_]', '_', name)
                    path = folder+'/'+asset_name
                    material = ASSETS.load_asset(path) if ASSETS.does_asset_exist(path) else TOOLS.create_asset(
                        asset_name,folder,unreal.Material,unreal.MaterialFactoryNew())
                    EDIT.delete_all_material_expressions(material)
                    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
                    color, roughness, specular = recipe
                    rgb = EDIT.create_material_expression(material,unreal.MaterialExpressionConstant3Vector)
                    rgb.set_editor_property('constant',unreal.LinearColor(*color,1))
                    EDIT.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
                    for value,prop in [(roughness,unreal.MaterialProperty.MP_ROUGHNESS),
                                       (specular,unreal.MaterialProperty.MP_SPECULAR)]:
                        node=EDIT.create_material_expression(material,unreal.MaterialExpressionConstant)
                        node.set_editor_property('r',value)
                        EDIT.connect_material_property(node,'',prop)
                    EDIT.set_material_usage(material,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
                    EDIT.recompile_material(material)
                    save(material)
                    cache[name]=material
                slot.set_editor_property('material_interface',cache[name])
            if not slot.get_editor_property('material_interface'):
                raise RuntimeError('Unresolved source material '+cid+' '+name)
            # Python iterates reflected USTRUCT arrays by value; replace each
            # element explicitly before writing the array back to the mesh.
            slots[slot_index] = slot
        mesh.set_editor_property('materials',slots)
        save(mesh)
        if any(not slot.get_editor_property('material_interface') for slot in mesh.get_editor_property('materials')):
            raise RuntimeError('Material assignment did not persist: '+mesh.get_path_name())


def rebuild_baked_materials(cid, folder):
    for suffix, filename, masked, roughness in [
        ('Skin', f'{cid}_SkinReference.png', False, 0.64),
        ('Shirt', f'{cid}_Shirt_Cotton.png', False, 0.88),
        ('Hair', f'{cid}_Hair_Texture.png', True, 0.90),
        ('Brows', f'{cid}_Brows_Texture.png', True, 0.85),
    ]:
        material_path = folder + '/' + cid + '_' + suffix
        source = SOURCE / cid / filename
        if not ASSETS.does_asset_exist(material_path):
            continue
        if not source.is_file():
            raise RuntimeError('Missing baked material texture: ' + str(source))
        texture = import_asset(source, folder, 'T_' + source.stem, unreal.Texture2D)
        if suffix == 'Skin':
            texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
            texture.set_editor_property('max_texture_size', 4096)
            save(texture)
        material = require_asset(material_path, unreal.Material)
        EDIT.delete_all_material_expressions(material)
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED if masked else unreal.BlendMode.BLEND_OPAQUE)
        material.set_editor_property('two_sided', masked)
        if masked:
            material.set_editor_property('opacity_mask_clip_value', 0.3)
        sample = EDIT.create_material_expression(material, unreal.MaterialExpressionTextureSample)
        sample.set_editor_property('texture', texture)
        EDIT.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
        if suffix == 'Hair':
            # The groom keeps the original strand opacity but uses a matte dark
            # scalp base. Match its Blender shader, not the previous pale atlas.
            color = EDIT.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
            color.set_editor_property('constant', unreal.LinearColor(
                *[channel * .32 for channel in HAIR_COLORS[int(cid[1:])-1]], 1))
            EDIT.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
            hair_spec = EDIT.create_material_expression(material, unreal.MaterialExpressionConstant)
            hair_spec.set_editor_property('r', .08)
            EDIT.connect_material_property(hair_spec, '', unreal.MaterialProperty.MP_SPECULAR)
        if masked:
            EDIT.connect_material_property(sample, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
        rough = EDIT.create_material_expression(material, unreal.MaterialExpressionConstant)
        rough.set_editor_property('r', roughness)
        EDIT.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
        if suffix == 'Skin' or (cid == 'C01' and suffix == 'Shirt'):
            for detail, prop in [('Normal', unreal.MaterialProperty.MP_NORMAL),
                                 ('Roughness', unreal.MaterialProperty.MP_ROUGHNESS)]:
                detail_file = SOURCE / cid / f'{cid}_{suffix}{detail}.png'
                if not detail_file.is_file():
                    continue
                detail_tex = import_asset(detail_file, folder, f'T_{cid}_{suffix}{detail}', unreal.Texture2D)
                detail_tex.set_editor_property('srgb', False)
                detail_tex.set_editor_property('max_texture_size', 2048)
                detail_tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP
                                              if detail == 'Normal' else unreal.TextureCompressionSettings.TC_MASKS)
                if detail == 'Normal':
                    detail_tex.set_editor_property('flip_green_channel', True)
                save(detail_tex)
                detail_sample = EDIT.create_material_expression(material, unreal.MaterialExpressionTextureSample)
                detail_sample.set_editor_property('texture', detail_tex)
                detail_sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
                                                 if detail == 'Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
                EDIT.connect_material_property(detail_sample, 'RGB' if detail == 'Normal' else 'R', prop)
            specular = EDIT.create_material_expression(material, unreal.MaterialExpressionConstant)
            specular.set_editor_property('r', 0.25)
            EDIT.connect_material_property(specular, '', unreal.MaterialProperty.MP_SPECULAR)
        EDIT.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
        EDIT.recompile_material(material)
        save(material)


def preview_material():
    name = 'RT_CharacterPreview_Default'
    target = ASSETS.load_asset(UI + '/' + name) if ASSETS.does_asset_exist(UI + '/' + name) else None
    if not target:
        target = TOOLS.create_asset(name, UI, unreal.TextureRenderTarget2D, unreal.TextureRenderTargetFactoryNew())
    target.set_editor_property('size_x', 32)
    target.set_editor_property('size_y', 64)
    target.set_editor_property('render_target_format', unreal.TextureRenderTargetFormat.RTF_RGBA16F)
    target.set_editor_property('clear_color', unreal.LinearColor(0, 0, 0, 1))
    save(target)
    material = ASSETS.load_asset(UI + '/M_CharacterPreview') if ASSETS.does_asset_exist(UI + '/M_CharacterPreview') else None
    if not material:
        material = TOOLS.create_asset('M_CharacterPreview', UI, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    EDIT.delete_all_material_expressions(material)

    def node(kind, **props):
        obj = EDIT.create_material_expression(material, getattr(unreal, 'MaterialExpression' + kind))
        for key, value in props.items():
            obj.set_editor_property(key, value)
        return obj

    def link(source, target_node, pin, output=''):
        if not EDIT.connect_material_expressions(source, output, target_node, pin):
            raise RuntimeError('Failed preview material connection: ' + pin)

    sample = node('TextureSampleParameter2D', parameter_name='PreviewTexture', texture=target,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    opacity = node('OneMinus')
    link(sample, opacity, '', 'A')
    safe_alpha = node('Max', const_b=0.001)
    link(opacity, safe_alpha, 'A')
    color = node('Divide')
    link(sample, color, 'A', 'RGB')
    link(safe_alpha, color, 'B')
    denominator = node('Add', const_b=1.0)
    link(color, denominator, 'A')
    tone_map = node('Divide')
    link(color, tone_map, 'A')
    link(denominator, tone_map, 'B')
    if not EDIT.connect_material_property(tone_map, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Failed preview color output')
    if not EDIT.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError('Failed preview opacity output')
    EDIT.layout_material_expressions(material)
    EDIT.recompile_material(material)
    save(material)


def vector(value):
    return [round(float(value.x), 4), round(float(value.y), 4), round(float(value.z), 4)]


def mesh_metadata(mesh, is_body):
    skeleton = mesh.get_editor_property('skeleton')
    if not skeleton:
        raise RuntimeError('Missing skeleton: ' + mesh.get_path_name())
    component = unreal.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    bones = [str(component.get_bone_name(i)) for i in range(component.get_num_bones())]
    if len(bones) < 5:
        raise RuntimeError('Insufficient rig bones: ' + mesh.get_path_name())
    bounds = mesh.get_imported_bounds()
    height = float(bounds.box_extent.z * 2)
    if is_body and not 100.0 <= height <= 240.0:
        raise RuntimeError(f'Unexpected character height {height} cm: {mesh.get_path_name()}')
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    lod_count = subsystem.get_lod_count(mesh)
    lods = [{'index': i, 'vertices': subsystem.get_num_verts(mesh, i),
             'sections': subsystem.get_num_sections(mesh, i)} for i in range(lod_count)]
    if not lods or lods[0]['vertices'] < 100:
        raise RuntimeError('Missing mesh geometry: ' + mesh.get_path_name())
    materials = []
    for slot in mesh.get_editor_property('materials'):
        material = slot.get_editor_property('material_interface')
        if not material:
            raise RuntimeError('Missing material: ' + mesh.get_path_name())
        materials.append({'slot': str(slot.get_editor_property('material_slot_name')),
                          'asset': material.get_path_name()})
    return {'asset': mesh.get_path_name(), 'skeleton': skeleton.get_path_name(), 'bones': bones,
            'height_cm': height, 'bounds_origin': vector(bounds.origin), 'bounds_extent': vector(bounds.box_extent),
            'lods': lods, 'materials': materials}


def animation_metadata(animation, expected_skeleton):
    skeleton = animation.get_editor_property('skeleton')
    frames = unreal.AnimationLibrary.get_num_frames(animation)
    tracks = [str(name) for name in unreal.AnimationLibrary.get_animation_track_names(animation)]
    if skeleton != expected_skeleton or frames < 2 or len(tracks) < 3 or animation.get_play_length() <= 0:
        raise RuntimeError('Invalid animation/skeleton mapping: ' + animation.get_path_name())
    return {'asset': animation.get_path_name(), 'skeleton': skeleton.get_path_name(), 'frames': frames,
            'duration': animation.get_play_length(), 'tracks': tracks}


def main():
    if not VERIFY_ONLY:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        # Reimports otherwise switch to Interchange despite the explicit legacy factory.
        unreal.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.Enable 0')
        for cid in CHARACTERS:
            for name in [f'SK_{cid}.fbx', f'SK_{cid}_Arms.fbx', f'AN_{cid}_Idle.fbx',
                         f'AN_{cid}_Walk.fbx', f'AN_{cid}_ArmsIdle.fbx', f'T_{cid}.png']:
                source = SOURCE / cid / name
                if not source.is_file() or source.stat().st_size == 0:
                    raise RuntimeError('Missing/empty source: ' + str(source))
        preview_material()
    manifest = {'characters': [], 'force_front_x_axis': FORCE_FRONT_X, 'source': str(SOURCE)}
    for cid in CHARACTERS:
        folder = DEST + '/' + cid
        if VERIFY_ONLY:
            body = require_asset(folder + '/SK_' + cid, unreal.SkeletalMesh)
            arms = require_asset(folder + '/SK_' + cid + '_Arms', unreal.SkeletalMesh)
        else:
            # The legacy skeletal reimport does not restore missing external
            # textures referenced by an already-existing material package.
            if cid == 'C03' and not ASSETS.does_asset_exist(folder + '/shoes05_diffuse'):
                import_asset(SOURCE / cid / f'SK_{cid}.fbm' / 'shoes05_diffuse.png',
                             folder, 'shoes05_diffuse', unreal.Texture2D)
            body = import_asset(SOURCE / cid / f'SK_{cid}.fbx', folder, 'SK_' + cid,
                                unreal.SkeletalMesh, fbx_options())
            arms = import_asset(SOURCE / cid / f'SK_{cid}_Arms.fbx', folder, 'SK_' + cid + '_Arms',
                                unreal.SkeletalMesh, fbx_options())
            fit_constant_materials(cid,folder,(body,arms))
        animations = []
        for suffix, mesh in [('Idle', body), ('Walk', body), ('ArmsIdle', arms)]:
            name = f'AN_{cid}_{suffix}'
            skeleton = mesh.get_editor_property('skeleton')
            animation = require_asset(folder + '/' + name, unreal.AnimSequence) if VERIFY_ONLY else import_asset(
                SOURCE / cid / (name + '.fbx'), folder, name, unreal.AnimSequence, fbx_options(True, skeleton))
            animations.append(animation_metadata(animation, skeleton))
        if VERIFY_ONLY:
            portrait = require_asset(DEST + '/Portraits/T_' + cid, unreal.Texture2D)
        else:
            portrait = import_asset(SOURCE / cid / f'T_{cid}.png', DEST + '/Portraits', 'T_' + cid, unreal.Texture2D)
            portrait.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
            portrait.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            portrait.set_editor_property('never_stream', True)
            save(portrait)
            rebuild_baked_materials(cid, folder)
            fix_masked_materials(folder)
        entry = {'id': cid, 'body': mesh_metadata(body, True), 'arms': mesh_metadata(arms, False),
                 'animations': animations, 'portrait': portrait.get_path_name()}
        manifest['characters'].append(entry)
        unreal.log('COBBLE_CHARACTER_READY: ' + cid + ' height_cm=' + str(entry['body']['height_cm']))
    manifest['preview_material'] = require_asset(UI + '/M_CharacterPreview', unreal.Material).get_path_name()
    output = Path(__file__).with_name('cobble-import-manifest-' + '-'.join(CHARACTERS) + '.json')
    output.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    unreal.log('COBBLE_IMPORT_PASSED: ' + ','.join(CHARACTERS) + ' manifest=' + str(output))


main()
