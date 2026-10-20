"""Create the two materials Calder State's buildings need (master job; HouseCampusInteriors and the exterior polish):

M_CampusInteriorWindow  interior mapping for the windows of closed floors. Each window quad carries 0..1 UVs (u along
                        cross(normal, up), v up from the sill); a Custom node builds the view ray in world space from the
                        camera vector, the vertex normal and world up, casts it into a box room behind the glass (side
                        walls, floor, ceiling and back wall) and shades it from a painted 2x2 room atlas (RoomAtlas, one room
                        per tile, chosen by the Room scalar). Aspect is the glass height over its width. Lit (0..1) scales an
                        emissive term so a lit room glows and a dark one only reflects a little. Opaque, default lit.
M_LightPoolDecal        deferred decal, emissive only: a soft radial pool of Tint * Intensity (the ground under a mercury
                        lamp or below a lit window), no base colour change.

Writes only these two assets under /Game/SeniorSendOff/House/CampusInterior. An existing one is skipped, except the names
in REBUILD (this job's own assets, rebuilt when their graph changes). It does not open or save any map. Run headless with
Tools/claude/ue_py.sh while the editor is closed.
"""
import unreal

folder = "/Game/SeniorSendOff/House/CampusInterior"
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


REBUILD = {"M_CampusInteriorWindow"}   # 2b: world-space ray (v1's tangent-space ray was wrong on two sides); one gradient read


def new_material(name):
    path = folder + "/" + name
    if assets.does_asset_exist(path):
        if name not in REBUILD:
            unreal.log("CAMPUS_MATERIAL exists " + path)
            return None
        assets.delete_asset(path)
        unreal.log("CAMPUS_MATERIAL rebuilding " + path)
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


def custom_node(mat, code, inputs, out_type, x, y):
    node = editing.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", out_type)
    node.set_editor_property("description", "CampusInterior")
    pins = []
    for n in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", n)
        pins.append(pin)
    node.set_editor_property("inputs", pins)
    return node


# --------------------------------------------------------------------------------------------- M_CampusInteriorWindow
INTERIOR_HLSL = r"""
// The view ray in the window's own space, built in world space: +X along the window (cross(normal, up), the way the
// quad's u runs), +Y up, +Z out of the wall. Units: the window's width, so the height is scaled by Aspect.
float3 Nw = normalize(N);
float3 Rw = normalize(cross(Nw, float3(0, 0, 1)));
float3 Dw = -normalize(V);
float3 Ray = float3(dot(Dw, Rw), dot(Dw, float3(0, 0, 1)) / max(Aspect, 0.1), dot(Dw, Nw));
Ray.z = min(Ray.z, -0.02);
float2 P = frac(UV);
float D = max(Depth, 0.05);
// Distances along the ray to the side walls (x), floor/ceiling (y) and the back wall (z = -D).
float Tx = (Ray.x > 0 ? (1.0 - P.x) : -P.x) / (abs(Ray.x) < 1e-4 ? 1e-4 : Ray.x);
float Ty = (Ray.y > 0 ? (1.0 - P.y) : -P.y) / (abs(Ray.y) < 1e-4 ? 1e-4 : Ray.y);
float Tz = -D / Ray.z;
Tx = Tx < 0 ? 1e6 : Tx; Ty = Ty < 0 ? 1e6 : Ty;
float T = min(Tz, min(Tx, Ty));
float3 H = float3(P, 0) + Ray * T;
float Room = floor(fmod(max(RoomIndex, 0.0), 4.0));
float2 Tile = float2(fmod(Room, 2.0), floor(Room / 2.0)) * 0.5;
// The tile: the back wall fills the inner 80% (its top row is the wall's top); the border holds the side wall (left
// column), the ceiling (top rows) and the floor (bottom rows). The branches only pick where to read; the one read after
// them uses the quad's own UV gradients, so pixels either side of a room corner agree on the mip.
float2 At;
float Shade = 1.0;
if (T == Tz)
{
    At = Tile + (0.1 + 0.8 * saturate(float2(H.x, 1.0 - H.y))) * 0.5;
}
else if (T == Tx)
{
    At = Tile + float2(0.03, 0.1 + 0.8 * saturate(1.0 - H.y)) * 0.5;
    Shade = 0.55 + 0.45 * saturate(1.0 + H.z / D);
}
else if (Ray.y < 0)
{
    At = Tile + float2(0.1 + 0.8 * saturate(H.x), 0.97) * 0.5;
    Shade = 0.5 + 0.5 * saturate(1.0 + H.z / D);
}
else
{
    At = Tile + float2(0.1 + 0.8 * saturate(H.x), 0.03) * 0.5;
    Shade = 0.7 + 0.3 * saturate(1.0 + H.z / D);
}
float3 C = Tex.SampleGrad(TexSampler, At, ddx(UV) * 0.4, ddy(UV) * 0.4).rgb;
return C * Shade;
"""


def build_interior_window():
    mat = new_material("M_CampusInteriorWindow")
    if not mat:
        return
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", False)
    uv = editing.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    cam = editing.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -900, 160)
    nrm = editing.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 240)
    atlas = editing.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -900, 320)
    atlas.set_editor_property("parameter_name", "RoomAtlas")
    tex = default_texture()
    if tex:
        atlas.set_editor_property("texture", tex)
    depth = scalar_param(mat, "Depth", 1.4, -900, 480)
    room = scalar_param(mat, "Room", 0.0, -900, 560)
    lit = scalar_param(mat, "Lit", 1.0, -500, 400)
    glow = scalar_param(mat, "GlowIntensity", 3.0, -500, 480)
    tint = vector_param(mat, "LightTint", (1.0, 0.86, 0.66), -500, 560)
    aspect = scalar_param(mat, "Aspect", 1.23, -900, 640)
    node = custom_node(mat, INTERIOR_HLSL, ["UV", "V", "N", "Tex", "Depth", "RoomIndex", "Aspect"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -600, 100)
    editing.connect_material_expressions(uv, "", node, "UV")
    editing.connect_material_expressions(cam, "", node, "V")
    editing.connect_material_expressions(nrm, "", node, "N")
    editing.connect_material_expressions(aspect, "", node, "Aspect")
    editing.connect_material_expressions(atlas, "", node, "Tex")
    editing.connect_material_expressions(depth, "", node, "Depth")
    editing.connect_material_expressions(room, "", node, "RoomIndex")
    # Emissive = room * tint * lit * intensity; base colour = a dim reflection of the room (so dark rooms read faintly).
    m1 = editing.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 100)
    editing.connect_material_expressions(node, "", m1, "A")
    editing.connect_material_expressions(tint, "", m1, "B")
    m2 = editing.create_material_expression(mat, unreal.MaterialExpressionMultiply, -100, 160)
    editing.connect_material_expressions(lit, "", m2, "A")
    editing.connect_material_expressions(glow, "", m2, "B")
    em = editing.create_material_expression(mat, unreal.MaterialExpressionMultiply, 50, 100)
    editing.connect_material_expressions(m1, "", em, "A")
    editing.connect_material_expressions(m2, "", em, "B")
    editing.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    dim = editing.create_material_expression(mat, unreal.MaterialExpressionMultiply, 50, -40)
    editing.connect_material_expressions(node, "", dim, "A")
    dim.set_editor_property("const_b", 0.12)
    editing.connect_material_property(dim, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = editing.create_material_expression(mat, unreal.MaterialExpressionConstant, 50, 260)
    rough.set_editor_property("r", 0.06)
    editing.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = editing.create_material_expression(mat, unreal.MaterialExpressionConstant, 50, 320)
    spec.set_editor_property("r", 0.9)
    editing.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    editing.recompile_material(mat)
    assets.save_asset(folder + "/M_CampusInteriorWindow")
    unreal.log("CAMPUS_MATERIAL created M_CampusInteriorWindow")


# --------------------------------------------------------------------------------------------- M_LightPoolDecal
POOL_HLSL = r"""
// A soft radial pool over the decal's 0..1 UVs: 1 at the centre, 0 at the rim, with a little falloff texture.
float2 Q = (frac(UV) - 0.5) * 2.0;
float R = length(Q);
float F = saturate(1.0 - R);
return F * F * (3.0 - 2.0 * F);
"""


def build_light_pool():
    mat = new_material("M_LightPoolDecal")
    if not mat:
        return
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    try:
        mat.set_editor_property("decal_blend_mode", unreal.DecalBlendMode.DBM_EMISSIVE)
    except Exception as e:  # newer engines name the emissive-only decal differently; fall back to translucent emissive
        unreal.log_warning("CAMPUS_MATERIAL decal blend mode: " + str(e))
    uv = editing.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -700, 0)
    node = custom_node(mat, POOL_HLSL, ["UV"], unreal.CustomMaterialOutputType.CMOT_FLOAT1, -500, 0)
    editing.connect_material_expressions(uv, "", node, "UV")
    tint = vector_param(mat, "Tint", (0.55, 0.95, 0.85), -500, 160)
    inten = scalar_param(mat, "Intensity", 2.0, -500, 260)
    m1 = editing.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 100)
    editing.connect_material_expressions(tint, "", m1, "A")
    editing.connect_material_expressions(inten, "", m1, "B")
    em = editing.create_material_expression(mat, unreal.MaterialExpressionMultiply, -50, 60)
    editing.connect_material_expressions(m1, "", em, "A")
    editing.connect_material_expressions(node, "", em, "B")
    editing.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    editing.connect_material_property(node, "", unreal.MaterialProperty.MP_OPACITY)
    editing.recompile_material(mat)
    assets.save_asset(folder + "/M_LightPoolDecal")
    unreal.log("CAMPUS_MATERIAL created M_LightPoolDecal")


build_interior_window()
build_light_pool()
unreal.log("CAMPUS_MATERIAL done")
