"""Import the Blender-fitted jersey/accessories onto the assembled Runner rig."""
import unreal

ROOT = '/Game/MetaHumans/RunnerRebuild/MH_Runner_Working'
DEST = ROOT + '/Details/RunnerOutfit'
MESH_NAME = 'SK_Runner_NativeOutfit'
REIMPORT = True
SOURCE = ('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/'
          'SourceArt/RunnerMetaHuman/SK_Runner_MetaOutfit.fbx')
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
body = assets.load_asset(ROOT + '/Body/SKM_MH_Runner_Working_BodyMesh')
if not body:
    raise RuntimeError('Runner native body missing')

unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
options = unreal.FbxImportUI()
for field, value in {
    'automated_import_should_detect_type': False,
    'mesh_type_to_import': unreal.FBXImportType.FBXIT_SKELETAL_MESH,
    'original_import_type': unreal.FBXImportType.FBXIT_SKELETAL_MESH,
    'import_as_skeletal': True,
    'import_mesh': True,
    'import_materials': False,
    'import_textures': False,
    'import_animations': False,
    'create_physics_asset': False,
    'skeleton': body.get_editor_property('skeleton'),
}.items():
    options.set_editor_property(field, value)
for field, value in {
    'convert_scene': True,
    'force_front_x_axis': False,
    # Blender exports this native FBX in centimeter-valued coordinates while
    # declaring meters. Converting scene units would multiply geometry by 100.
    'convert_scene_unit': False,
    'import_uniform_scale': 1.0,
    'import_mesh_lods': False,
    'update_skeleton_reference_pose': False,
    'use_t0_as_ref_pose': False,
    'normal_import_method': unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
}.items():
    options.skeletal_mesh_import_data.set_editor_property(field, value)
mesh = assets.load_asset(DEST + '/' + MESH_NAME)
if mesh is None or REIMPORT:
    task = unreal.AssetImportTask()
    task.filename = SOURCE
    task.destination_path = DEST
    task.destination_name = MESH_NAME
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    tools.import_asset_tasks([task])
    mesh = assets.load_asset(DEST + '/' + MESH_NAME)
if not mesh:
    raise RuntimeError('Runner outfit did not import')

library = unreal.MaterialEditingLibrary
def plain_material(name, color, roughness, metallic=0.0):
    path = DEST + '/' + name
    material = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
        name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    library.delete_all_material_expressions(material)
    material.set_editor_property('used_with_skeletal_mesh', True)
    material.set_editor_property('two_sided', True)
    rgb = library.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
    rgb.set_editor_property('constant', unreal.LinearColor(*color, 1))
    library.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
    for value, prop in [(roughness, unreal.MaterialProperty.MP_ROUGHNESS),
                        (metallic, unreal.MaterialProperty.MP_METALLIC)]:
        node = library.create_material_expression(material, unreal.MaterialExpressionConstant)
        node.set_editor_property('r', value)
        library.connect_material_property(node, '', prop)
    library.recompile_material(material)
    assets.save_loaded_asset(material)
    return material

materials = {
    'Runner_NativeShorts': plain_material('M_Runner_BlackShorts', (.014, .017, .019), .83),
    'Runner_NativeJersey': plain_material('M_Runner_TealJersey', (.105, .43, .34), .84),
    'Runner_PrintBlack': plain_material('M_Runner_JerseyLetter', (.014, .016, .018), .72),
    'Runner_PrintGold': plain_material('M_Runner_JerseyOutline', (.57, .37, .12), .70),
    'Runner_TrimBlack': plain_material('M_Runner_JerseyBindingBlack', (.010, .012, .014), .77),
    'Runner_TrimGold': plain_material('M_Runner_JerseyBindingGold', (.47, .30, .11), .74),
    'Runner_PinstripeDark': plain_material('M_Runner_JerseyPinstripeDark', (.045, .225, .215), .83),
    'Runner_PinstripeGold': plain_material('M_Runner_JerseyPinstripeGold', (.26, .33, .235), .83),
    'C02_Shorts': assets.load_asset('/Game/Characters/Cobble/C02/C02_Shorts'),
    'C02_Shirt': assets.load_asset('/Game/Characters/Cobble/C02/C02_Shirt'),
    'C02_black': assets.load_asset('/Game/Characters/Cobble/C02/C02_black'),
    'RunnerV2_TrimGold': assets.load_asset('/Game/Characters/RunnerV2/M_RunnerV2_TrimGold'),
    'C02_brown': plain_material('M_Runner_SandalBrown', (.08, .053, .036), .84),
    'C02_metal': plain_material('M_Runner_GlassesMetal', (.10, .11, .13), .30, .75),
    'Cobble sunglass lens_001': assets.load_asset('/Game/Characters/Cobble/C02/Cobble_sunglass_lens_001'),
}

# A single small repeating normal texture gives the polyester a breathable
# knit finish without adding costly geometric perforations to the mobile mesh.
detail_file = ('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/'
               'SourceArt/RunnerMetaHuman/T_Runner_JerseyMeshNormal.png')
texture_task = unreal.AssetImportTask()
texture_task.filename = detail_file
texture_task.destination_path = DEST
texture_task.destination_name = 'T_Runner_JerseyMeshNormal'
texture_task.automated = True
texture_task.replace_existing = True
texture_task.save = True
tools.import_asset_tasks([texture_task])
detail = assets.load_asset(DEST + '/T_Runner_JerseyMeshNormal')
if not isinstance(detail, unreal.Texture2D):
    raise RuntimeError('Runner jersey fabric normal failed to import')
detail.set_editor_property('srgb', False)
detail.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
detail.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
detail.set_editor_property('max_texture_size', 512)
assets.save_loaded_asset(detail)
jersey_material = materials['Runner_NativeJersey']
# Keep the jersey graphics in the fabric's UVs. Separate lettering meshes
# floated away from the cloth during animation and exposed seams at the sides.
art_file = ('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/'
            'SourceArt/RunnerMetaHuman/T_Runner_JerseyArt.png')
art_task = unreal.AssetImportTask()
art_task.filename = art_file
art_task.destination_path = DEST
art_task.destination_name = 'T_Runner_JerseyArt'
art_task.automated = True
art_task.replace_existing = True
art_task.save = True
tools.import_asset_tasks([art_task])
art = assets.load_asset(DEST + '/T_Runner_JerseyArt')
if not isinstance(art, unreal.Texture2D):
    raise RuntimeError('Runner jersey print failed to import')
art.set_editor_property('srgb', True)
art.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
art.set_editor_property('max_texture_size', 2048)
assets.save_loaded_asset(art)
teal = library.create_material_expression(jersey_material, unreal.MaterialExpressionConstant3Vector)
teal.set_editor_property('constant', unreal.LinearColor(.105, .43, .34, 1))
ink = library.create_material_expression(jersey_material, unreal.MaterialExpressionTextureSample)
ink.set_editor_property('texture', art)
blend = library.create_material_expression(jersey_material, unreal.MaterialExpressionLinearInterpolate)
library.connect_material_expressions(teal, '', blend, 'A')
library.connect_material_expressions(ink, 'RGB', blend, 'B')
library.connect_material_expressions(ink, 'A', blend, 'Alpha')
library.connect_material_property(blend, '', unreal.MaterialProperty.MP_BASE_COLOR)
uv = library.create_material_expression(jersey_material, unreal.MaterialExpressionTextureCoordinate)
uv.set_editor_property('u_tiling', 7.0)
uv.set_editor_property('v_tiling', 7.0)
sample = library.create_material_expression(jersey_material, unreal.MaterialExpressionTextureSample)
sample.set_editor_property('texture', detail)
sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
library.connect_material_expressions(uv, '', sample, 'Coordinates')
library.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_NORMAL)
spec = library.create_material_expression(jersey_material, unreal.MaterialExpressionConstant)
spec.set_editor_property('r', .24)
library.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
library.recompile_material(jersey_material)
assets.save_loaded_asset(jersey_material)

gold = materials['RunnerV2_TrimGold']
if isinstance(gold, unreal.Material):
    gold.set_editor_property('used_with_skeletal_mesh', True)
    assets.save_loaded_asset(gold)
slots = list(mesh.get_editor_property('materials'))
labels = []
for index, slot in enumerate(slots):
    name = str(slot.get_editor_property('material_slot_name'))
    labels.append(name)
    # FBX reimport preserves the old slot *names*. The new native-garment FBX
    # has shorts in section 0 and jersey in section 1; the earlier C02 photo
    # materials have alpha cutouts on incompatible UVs, making these vanish.
    target = (materials['Runner_NativeShorts'] if index == 0 else
              materials['Runner_NativeJersey'] if index == 1 else
              materials.get(name))
    if not target:
        raise RuntimeError('Unmapped material slot: ' + name)
    slot.set_editor_property('material_interface', target)
mesh.set_editor_property('materials', slots)
mesh.set_editor_property('physics_asset', body.get_editor_property('physics_asset'))
if not unreal.SeniorCharacterAssetTools.match_braxton_garment_bind_pose(mesh, body):
    raise RuntimeError('Runner wardrobe skeleton does not match native body hierarchy')
if not assets.save_loaded_asset(mesh):
    raise RuntimeError('Could not save Runner outfit mesh')
unreal.log('RUNNER_OUTFIT_IMPORTED skeleton=' + str(mesh.get_editor_property('skeleton').get_path_name())
           + ' source_slots=' + str(labels) + ' first_two=opaque_black_and_teal')
