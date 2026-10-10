"""Create the private, softly readable sheet-metal finish for the crawl ducts.

This only writes M_VentCrawlSteel. It does not open or save the house map or
change the existing grille, wall, or library-hatch materials.
"""
import unreal

folder = "/Game/SeniorSendOff/House/VentNetwork"
name = "M_VentCrawlSteel"
path = folder + "/" + name
assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

assets.make_directory(folder)
material = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
    name, folder, unreal.Material, unreal.MaterialFactoryNew()
)
if not isinstance(material, unreal.Material):
    raise RuntimeError("Could not create crawl-duct material: " + path)

editing.delete_all_material_expressions(material)


def color(rgb, x, y):
    node = editing.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, x, y
    )
    node.constant = unreal.LinearColor(*rgb, 1.0)
    return node


def scalar(value, x, y):
    node = editing.create_material_expression(
        material, unreal.MaterialExpressionConstant, x, y
    )
    node.r = value
    return node


steel = color((0.19, 0.22, 0.24), -640, -160)
grime = unreal.load_asset(
    "/Game/SeniorSendOff/House/PigstyRoom/PIG_T_RustyMetal.PIG_T_RustyMetal"
)
if not isinstance(grime, unreal.Texture2D):
    raise RuntimeError("Existing pigsty rust texture is unavailable")
sample = editing.create_material_expression(
    material, unreal.MaterialExpressionTextureSample, -640, 10
)
sample.texture = grime
mix = editing.create_material_expression(
    material, unreal.MaterialExpressionLinearInterpolate, -280, -70
)
editing.connect_material_expressions(steel, "", mix, "A")
editing.connect_material_expressions(sample, "RGB", mix, "B")
editing.connect_material_expressions(scalar(0.18, -500, 160), "", mix, "Alpha")
editing.connect_material_property(mix, "", unreal.MaterialProperty.MP_BASE_COLOR)

editing.connect_material_property(
    scalar(0.73, -280, 130), "", unreal.MaterialProperty.MP_ROUGHNESS
)
editing.connect_material_property(
    scalar(0.22, -280, 250), "", unreal.MaterialProperty.MP_METALLIC
)
# A faint blue-gray bounce fill keeps branch silhouettes readable when the
# house's bright exposure otherwise crushes the duct's darker surfaces.
editing.connect_material_property(
    color((0.038, 0.046, 0.053), -280, 390),
    "",
    unreal.MaterialProperty.MP_EMISSIVE_COLOR,
)

editing.recompile_material(material)
if not assets.save_loaded_asset(material):
    raise RuntimeError("Could not save crawl-duct material: " + path)
unreal.log("VENT_MATERIAL_SAVED " + path)
