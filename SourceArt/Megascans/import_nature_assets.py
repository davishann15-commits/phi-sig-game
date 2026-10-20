"""Import supplied Megascans packs into dedicated project asset folders."""
import json
import os
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()) / 'SourceArt' / 'Megascans'
LIB = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EDIT = unreal.MaterialEditingLibrary
REPORT = {'packs': [], 'status': 'in_progress'}
BATCH_CONFIG = os.environ.get('SENIOR_SENDOFF_NATURE_IMPORT_CONFIG')
MANIFEST = ROOT / ('import-manifest-' + Path(BATCH_CONFIG).stem + '.json' if BATCH_CONFIG else 'import-manifest.json')


def log(message):
    unreal.log('NATURE_PACKS: ' + message)


def save(asset):
    if not LIB.save_loaded_asset(asset, False):
        raise RuntimeError('Could not save ' + asset.get_path_name())


def import_file(source, folder, name, mesh=False):
    desired = folder + '/' + name
    if LIB.does_asset_exist(desired):
        log('Reuse ' + desired)
        return LIB.load_asset(desired)
    LIB.make_directory(folder)
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = False
    task.save = True
    if mesh:
        # Do not force FbxFactory: UE 5.8 converts these settings to Interchange.
        options = unreal.FbxImportUI()
        options.set_editor_property('import_as_skeletal', False)
        options.set_editor_property('import_mesh', True)
        options.set_editor_property('import_materials', False)
        options.set_editor_property('import_textures', False)
        options.set_editor_property('import_animations', False)
        options.set_editor_property('automated_import_should_detect_type', False)
        options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
        data = options.get_editor_property('static_mesh_import_data')
        data.set_editor_property('combine_meshes', True)
        data.set_editor_property('auto_generate_collision', True)
        data.set_editor_property('convert_scene', True)
        data.set_editor_property('convert_scene_unit', True)
        task.options = options
    TOOLS.import_asset_tasks([task])
    imported = []
    for path in task.get_editor_property('imported_object_paths'):
        asset = LIB.load_asset(path)
        if isinstance(asset, unreal.StaticMesh if mesh else unreal.Texture2D):
            imported.append(asset)
    if len(imported) != 1:
        raise RuntimeError('Expected one asset for ' + str(source) + ': ' + str(task.get_editor_property('imported_object_paths')))
    asset = imported[0]
    if asset.get_path_name().split('.')[0] != desired:
        if not LIB.rename_asset(asset.get_path_name(), desired):
            raise RuntimeError('Could not name imported asset ' + desired)
        asset = LIB.load_asset(desired)
    save(asset)
    return asset


def texture_node(material, texture, x, y):
    node = EDIT.create_material_expression(material, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property('texture', texture)
    if texture.get_editor_property('compression_settings') == unreal.TextureCompressionSettings.TC_NORMALMAP:
        sampler = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    elif texture.get_editor_property('srgb'):
        sampler = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    else:
        sampler = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    node.set_editor_property('sampler_type', sampler)
    return node


def material_for(pack, textures):
    folder = pack['destination'] + '/Materials'
    name = 'M_' + pack['label']
    path = folder + '/' + name
    if LIB.does_asset_exist(path):
        return LIB.load_asset(path)
    LIB.make_directory(folder)
    material = TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if pack['foliage']:
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        material.set_editor_property('opacity_mask_clip_value', 0.333)
        material.set_editor_property('two_sided', True)
        material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    connections = [('BaseColor', 'RGB', unreal.MaterialProperty.MP_BASE_COLOR),
                   ('Normal', 'RGB', unreal.MaterialProperty.MP_NORMAL),
                   ('Roughness', 'R', unreal.MaterialProperty.MP_ROUGHNESS),
                   ('Specular', 'R', unreal.MaterialProperty.MP_SPECULAR),
                   ('AO', 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)]
    if pack['foliage']:
        connections += [('Opacity', 'R', unreal.MaterialProperty.MP_OPACITY_MASK),
                        ('Translucency', 'RGB', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)]
    for index, (suffix, channel, prop) in enumerate(connections):
        if suffix not in textures:
            continue
        node = texture_node(material, textures[suffix], -450, index * 180)
        if not EDIT.connect_material_property(node, channel, prop):
            raise RuntimeError('Could not connect material input ' + suffix)
    EDIT.recompile_material(material)
    save(material)
    return material


def run():
    packs = [
        {'folder': 'raspberry_wf0oefeja_high', 'label': 'Raspberry', 'prefix': 'Raspberry_wf0oefeja_High_4K_',
         'destination': '/Game/Environment/Megascans/Raspberry_wf0oefeja', 'foliage': True},
        {'folder': 'large_fallen_tree_wd1mbcvbw_high', 'label': 'LargeFallenTree', 'prefix': 'Large_Fallen_Tree_wd1mbcvbw_High_4K_',
         'destination': '/Game/Environment/Megascans/LargeFallenTree_wd1mbcvbw', 'foliage': False},
    ]
    if BATCH_CONFIG:
        packs = json.loads(Path(BATCH_CONFIG).read_text())
    for pack in packs:
        source = ROOT / pack['folder']
        log('Importing ' + pack['label'])
        textures = {}
        for file in sorted(source.glob('*.jpg')):
            suffix = file.stem.removeprefix(pack['prefix'])
            texture = import_file(file, pack['destination'] + '/Textures', 'T_' + pack['label'] + '_' + suffix)
            color = suffix.endswith('BaseColor') or suffix.endswith('Translucency')
            texture.set_editor_property('srgb', color)
            texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if suffix.endswith('Normal') else unreal.TextureCompressionSettings.TC_DEFAULT)
            if suffix.endswith('Normal'):
                texture.set_editor_property('flip_green_channel', False)
            save(texture)
            textures[suffix] = texture
        material = material_for(pack, textures)
        files = sorted(source.glob('*_LOD0.fbx')) if pack['foliage'] else sorted(source.glob('*.fbx'))
        meshes = []
        for file in files:
            if pack['foliage']:
                variant = file.stem.split('_Var')[1].split('_')[0]
                name = 'SM_' + pack['label'] + '_Var' + variant
            else:
                name = 'SM_' + pack['label']
            mesh = import_file(file, pack['destination'] + '/Meshes', name, mesh=True)
            for index in range(len(mesh.get_editor_property('static_materials'))):
                mesh.set_material(index, material)
            save(mesh)
            bounds = mesh.get_bounds()
            meshes.append({'asset': mesh.get_path_name(), 'source': str(file),
                           'extent_cm': [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z],
                           'material': material.get_path_name()})
            log('Ready ' + mesh.get_path_name())
        REPORT['packs'].append({'name': pack['label'], 'content_folder': pack['destination'],
                                'textures': [tex.get_path_name() for tex in textures.values()], 'meshes': meshes})
        MANIFEST.write_text(json.dumps(REPORT, indent=2) + '\n')
    REPORT['status'] = 'complete'
    MANIFEST.write_text(json.dumps(REPORT, indent=2) + '\n')
    log('COMPLETE: ' + str(len(packs)) + ' packs saved; all source files preserved. No levels edited.')


try:
    run()
except Exception as exc:
    REPORT['status'] = 'failed'
    REPORT['error'] = str(exc)
    MANIFEST.write_text(json.dumps(REPORT, indent=2) + '\n')
    raise
