"""Create the lobby's code-driven sky material; leave the house source image untouched."""
import unreal

assets = unreal.EditorAssetLibrary
edit = unreal.MaterialEditingLibrary
path = '/Game/Lobby/UI/M_LobbyAtmosphere'
texture = assets.load_asset('/Game/Lobby/UI/T_LobbyBackdrop')
assert isinstance(texture, unreal.Texture2D)
material = assets.load_asset(path) if assets.does_asset_exist(path) else None
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_LobbyAtmosphere', '/Game/Lobby/UI', unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
edit.delete_all_material_expressions(material)

def node(kind, **properties):
    result = edit.create_material_expression(material, getattr(unreal, 'MaterialExpression' + kind))
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result

uv = node('TextureCoordinate')
image = node('TextureObjectParameter', parameter_name='HousePlate', texture=texture)
time = node('ScalarParameter', parameter_name='AmbientSeconds', default_value=0.0)
code = r'''
float3 base = Texture2DSample(Plate, PlateSampler, UV).rgb;
// Keep all brickwork, rooflines, chimneys, and warm autumn foliage stationary.
float roof = min(0.124, 0.008 + abs(UV.x - 0.499) * 0.716);
float geometryMask = 1.0 - smoothstep(roof - 0.009, roof, UV.y);
float blueMask = smoothstep(0.018, 0.065, base.b - base.r)
               * smoothstep(0.005, 0.026, base.b - base.g);
float mask = geometryMask * blueMask;
// Animate new translucent cloud layers, never offset the full house photograph.
// This prevents displaced branch/chimney outlines at the sky boundary.
if (mask < 0.001) return base;
struct CloudField
{
    float hash(float2 p)
    {
        p = frac(p * float2(0.1031, 0.11369));
        p += dot(p, p.yx + 33.33);
        return frac((p.x + p.y) * p.x);
    }
    float noise(float2 p)
    {
        float2 cell = floor(p), f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(hash(cell), hash(cell + float2(1,0)), f.x),
                    lerp(hash(cell + float2(0,1)), hash(cell + 1.0), f.x), f.y);
    }
    float clouds(float2 p)
    {
        return noise(p) * 0.57 + noise(p * 2.03 + 7.1) * 0.28
             + noise(p * 4.07 + 13.7) * 0.15;
    }
};
CloudField field;
float2 p = UV * float2(9.0, 33.0) + float2(-Seconds * 0.034, Seconds * 0.003);
float broad = field.clouds(p);
float fine = field.clouds(p * float2(1.8, 1.3) + float2(-Seconds * 0.011, 9.0));
float density = smoothstep(0.30, 0.73, broad * 0.72 + fine * 0.28);
float3 moonlitCloud = lerp(base * 0.76, float3(0.075, 0.115, 0.18), 0.55);
return lerp(base, moonlitCloud, mask * density * 0.47);
'''
custom = node('Custom', code=code, description='Autumn sky drift; masked to sky only',
              output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
inputs = []
for name in ('Plate', 'UV', 'Seconds'):
    item = unreal.CustomInput()
    item.set_editor_property('input_name', name)
    inputs.append(item)
custom.set_editor_property('inputs', inputs)
for source, name in ((image, 'Plate'), (uv, 'UV'), (time, 'Seconds')):
    assert edit.connect_material_expressions(source, '', custom, name)
assert edit.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.layout_material_expressions(material)
edit.recompile_material(material)
assert assets.save_loaded_asset(material, only_if_is_dirty=False)
unreal.log('LOBBY_ATMOSPHERE_MATERIAL_READY: ' + material.get_path_name())
