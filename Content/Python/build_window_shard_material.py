"""Build the dedicated, lightly reflective material for fallen window fragments.

Only saves M_WindowShard. The intact window material and house map stay untouched.
"""
import unreal

folder = "/Game/SeniorSendOff/House/Interactivity"
name = "M_WindowShard"
path = folder + "/" + name
assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

existing = assets.does_asset_exist(path)
material = assets.load_asset(path) if existing else tools.create_asset(
    name, folder, unreal.Material, unreal.MaterialFactoryNew()
)
if not isinstance(material, unreal.Material):
    raise RuntimeError("Could not create shard material: " + path)
# Unreal 5.8 can crash while deleting already-saved expression objects from a
# commandlet. An existing material only needs its usage flag refreshed.
if existing:
    material.set_editor_property("used_with_instanced_static_meshes", True)
    editing.recompile_material(material)
    if not assets.save_loaded_asset(material):
        raise RuntimeError("Could not save shard material usage flag: " + path)
    unreal.log("WINDOW_SHARD_MATERIAL_SAVED " + path)
    raise SystemExit(0)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("two_sided", True)
material.set_editor_property("used_with_instanced_static_meshes", True)
material.set_editor_property(
    "translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING
)


def scalar(value, x, y):
    node = editing.create_material_expression(material, unreal.MaterialExpressionConstant, x, y)
    node.r = value
    return node


def color(rgb, x, y):
    node = editing.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.constant = unreal.LinearColor(*rgb, 1.0)
    return node


def property_node(node, property_name):
    if not editing.connect_material_property(node, "", property_name):
        raise RuntimeError("Cannot connect " + str(property_name))


property_node(color((0.64, 0.76, 0.82), -440, -230), unreal.MaterialProperty.MP_BASE_COLOR)
property_node(scalar(0.12, -440, -70), unreal.MaterialProperty.MP_ROUGHNESS)
property_node(scalar(0.75, -440, 70), unreal.MaterialProperty.MP_SPECULAR)
property_node(color((0.055, 0.073, 0.083), -440, 220), unreal.MaterialProperty.MP_EMISSIVE_COLOR)

fresnel = editing.create_material_expression(material, unreal.MaterialExpressionFresnel, -600, 390)
edge_weight = editing.create_material_expression(material, unreal.MaterialExpressionMultiply, -340, 390)
editing.connect_material_expressions(fresnel, "", edge_weight, "A")
editing.connect_material_expressions(scalar(0.36, -560, 530), "", edge_weight, "B")
opacity = editing.create_material_expression(material, unreal.MaterialExpressionAdd, -120, 390)
editing.connect_material_expressions(scalar(0.30, -340, 560), "", opacity, "A")
editing.connect_material_expressions(edge_weight, "", opacity, "B")
property_node(opacity, unreal.MaterialProperty.MP_OPACITY)

editing.recompile_material(material)
if not assets.save_loaded_asset(material):
    raise RuntimeError("Could not save shard material: " + path)
unreal.log("WINDOW_SHARD_MATERIAL_SAVED " + path)
