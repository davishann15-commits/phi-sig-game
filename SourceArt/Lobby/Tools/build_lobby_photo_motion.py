"""Import hidden reveal plates and build motion from the original lobby pixels."""
from pathlib import Path
import unreal

assets = unreal.EditorAssetLibrary
edit = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
source = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/Lobby/PhotoMotion')
folder = '/Game/Lobby/UI'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.Enable 0')

def import_texture(filename, name, srgb=True):
    task = unreal.AssetImportTask()
    task.filename = str(source / filename)
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.set_editor_property('async_', False)
    asset_tools.import_asset_tasks([task])
    texture = assets.load_asset(folder + '/' + name)
    assert isinstance(texture, unreal.Texture2D)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('never_stream', True)
    texture.set_editor_property('srgb', srgb)
    assert assets.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture

original = assets.load_asset(folder + '/T_LobbyBackdrop')
assert isinstance(original, unreal.Texture2D)
clean = import_texture('HouseRevealPlate.png', 'T_LobbyHouseReveal')
sky = import_texture('SkyExtension.png', 'T_LobbySkyExtension')
foliage = import_texture('FoliageMatte.png', 'T_LobbyFoliageMatte', srgb=False)
ground = import_texture('GroundWithoutLeaves.png', 'T_LobbyGroundWithoutLeaves')
path = folder + '/M_LobbyAtmosphere'
material = assets.load_asset(path) if assets.does_asset_exist(path) else asset_tools.create_asset(
    'M_LobbyAtmosphere', folder, unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
edit.delete_all_material_expressions(material)

def node(kind, **properties):
    result = edit.create_material_expression(material, getattr(unreal, 'MaterialExpression' + kind))
    for name, value in properties.items():
        result.set_editor_property(name, value)
    return result

bindings = [
    ('Plate', node('TextureObjectParameter', parameter_name='HousePlate', texture=original)),
    ('CleanPlate', node('TextureObjectParameter', parameter_name='HouseRevealPlate', texture=clean)),
    ('SkyPlate', node('TextureObjectParameter', parameter_name='SkyExtension', texture=sky)),
    ('FoliageMatte', node('TextureObjectParameter', parameter_name='FoliageMatte', texture=foliage)),
    ('GroundPlate', node('TextureObjectParameter', parameter_name='GroundWithoutLeaves', texture=ground)),
    ('UV', node('TextureCoordinate')),
    ('Seconds', node('ScalarParameter', parameter_name='AmbientSeconds', default_value=0.0)),
    ('LeafWind', node('ScalarParameter', parameter_name='LeafWindStrength', default_value=0.0)),
    ('LightDim', node('VectorParameter', parameter_name='HouseLightDim', default_value=unreal.LinearColor(0,0,0,0))),
]
bindings.append(('LightDimA', bindings[-1][1]))
shader = Path(__file__).with_name('lobby_photo_motion.hlsl').read_text()
custom = node('Custom', code=shader, description='Existing sky, flags, foliage and windows; stationary ground for photo-leaf meshes',
              output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
inputs = []
for name, expression in bindings:
    item = unreal.CustomInput()
    item.set_editor_property('input_name', name)
    inputs.append(item)
custom.set_editor_property('inputs', inputs)
for name, expression in bindings:
    assert edit.connect_material_expressions(expression, 'A' if name == 'LightDimA' else '', custom, name)
assert edit.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.layout_material_expressions(material)
edit.recompile_material(material)
assert assets.save_loaded_asset(material, only_if_is_dirty=False)
unreal.log('LOBBY_PHOTO_MOTION_MATERIAL_READY: existing atmosphere plus stationary leaf-free ground; original leaf meshes are rendered by Slate')
