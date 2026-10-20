"""Create the two materials the dusk street scene needs (HouseStreetScene / HouseDuskAtmosphere):

M_StreetDecal  deferred decal: base colour = Atlas.rgb * Tint, opacity = Atlas.a * Opacity, roughness parameter.
M_StreetSmoke  lit translucent, two-sided: opacity = Puff.a * vertex alpha * Opacity, depth-faded;
               emissive = vertex colour * Glow (the coals lighting the smoke from below).

Only writes these two assets under /Game/SeniorSendOff/House/ExteriorStreet and skips any that already exist.
It does not open or save any map.
"""
import unreal

folder = "/Game/SeniorSendOff/House/ExteriorStreet"
assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets.make_directory(folder)


def default_texture():
    for path in ("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture",
                 "/Engine/EngineResources/DefaultTexture.DefaultTexture"):
        tex = unreal.load_asset(path)
        if isinstance(tex, unreal.Texture2D):
            return tex
    return None


def new_material(name):
    path = folder + "/" + name
    if assets.does_asset_exist(path):
        unreal.log("STREET_MATERIAL exists " + path)
        return None
    mat = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(mat, unreal.Material):
        raise RuntimeError("Could not create " + path)
    return mat


def scalar_param(mat, name, value, x, y):
    node = editing.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def vector_param(mat, name, rgb, x, y):
    node = editing.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return node


def texture_param(mat, name, x, y):
    node = editing.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property("parameter_name", name)
    tex = default_texture()
    if tex:
        node.set_editor_property("texture", tex)
    return node


def multiply(mat, a, a_out, b, b_out, x, y):
    node = editing.create_material_expression(mat, unreal.MaterialExpressionMultiply, x, y)
    editing.connect_material_expressions(a, a_out, node, "A")
    editing.connect_material_expressions(b, b_out, node, "B")
    return node


def connect(node, output, prop):
    if not editing.connect_material_property(node, output, prop):
        raise RuntimeError("Cannot connect " + str(prop))


decal = new_material("M_StreetDecal")
if decal:
    decal.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    decal.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    try:
        decal.set_editor_property("decal_blend_mode", unreal.DecalBlendMode.DBM_TRANSLUCENT)
    except Exception as error:  # newer engines infer it from the connected outputs
        unreal.log("STREET_MATERIAL decal_blend_mode left default: " + str(error))
    atlas = texture_param(decal, "Atlas", -760, 0)
    tint = vector_param(decal, "Tint", (1.0, 1.0, 1.0), -760, -260)
    connect(multiply(decal, atlas, "RGB", tint, "", -420, -120), "", unreal.MaterialProperty.MP_BASE_COLOR)
    opacity = scalar_param(decal, "Opacity", 1.0, -760, 260)
    connect(multiply(decal, atlas, "A", opacity, "", -420, 160), "", unreal.MaterialProperty.MP_OPACITY)
    connect(scalar_param(decal, "Roughness", 0.7, -420, 380), "", unreal.MaterialProperty.MP_ROUGHNESS)
    editing.recompile_material(decal)
    if not assets.save_loaded_asset(decal):
        raise RuntimeError("Could not save M_StreetDecal")
    unreal.log("STREET_MATERIAL saved M_StreetDecal")

smoke = new_material("M_StreetSmoke")
if smoke:
    smoke.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    smoke.set_editor_property("two_sided", True)
    smoke.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    puff = texture_param(smoke, "Puff", -860, 0)
    vertex = editing.create_material_expression(smoke, unreal.MaterialExpressionVertexColor, -860, 320)
    connect(vector_param(smoke, "SmokeColor", (0.5, 0.52, 0.55), -420, -260), "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = scalar_param(smoke, "Glow", 4.0, -860, 520)
    connect(multiply(smoke, vertex, "", glow, "", -420, 420), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    density = multiply(smoke, puff, "A", vertex, "A", -560, 120)
    opacity = scalar_param(smoke, "Opacity", 0.55, -860, 220)
    scaled = multiply(smoke, density, "", opacity, "", -360, 160)
    fade = editing.create_material_expression(smoke, unreal.MaterialExpressionDepthFade, -170, 160)
    try:
        fade.set_editor_property("fade_distance_default", 45.0)
    except Exception as error:
        unreal.log("STREET_MATERIAL depth fade distance left default: " + str(error))
    editing.connect_material_expressions(scaled, "", fade, "Opacity")
    connect(fade, "", unreal.MaterialProperty.MP_OPACITY)
    editing.recompile_material(smoke)
    if not assets.save_loaded_asset(smoke):
        raise RuntimeError("Could not save M_StreetSmoke")
    unreal.log("STREET_MATERIAL saved M_StreetSmoke")

unreal.log("STREET_MATERIAL done")
