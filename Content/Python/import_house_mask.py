"""Import the photo-guided oxblood beak mask and its four simple game materials."""
import unreal

root = unreal.Paths.project_dir()
source = root + 'SourceArt/HouseMask/SM_HouseOxbloodMask.fbx'
destination = '/Game/Items/HouseMask'
name = 'SM_HouseOxbloodMask'
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# The legacy FBX factory opens a Slate warning tab in commandlets on Mac.
# Use Unreal's Interchange importer, which can run unattended.
task = unreal.AssetImportTask()
task.filename = source
task.destination_path = destination
task.destination_name = name
task.automated = True
task.replace_existing = True
task.save = True
tools.import_asset_tasks([task])
mesh = assets.load_asset(destination + '/' + name)
if not mesh:
    raise RuntimeError('House mask mesh failed to import')

palette = {
    'Mask_Oxblood_Leather': ((.052, .012, .016), .46),
    'Mask_Charcoal_Leather': ((.006, .004, .005), .59),
    'Mask_Worn_Red_Highlights': ((.095, .020, .026), .50),
    'Mask_Dark_Seams': ((.003, .002, .003), .65),
    'Mask_Tarnished_Brass': ((.115, .088, .054), .38),
    'Mask_Scarred_Leather': ((.15, .037, .039), .70),
}
editing = unreal.MaterialEditingLibrary
for slot_name, (rgb, roughness) in palette.items():
    path = destination + '/M_' + slot_name
    material = assets.load_asset(path)
    if not material:
        material = tools.create_asset('M_' + slot_name, destination,
                                      unreal.Material, unreal.MaterialFactoryNew())
        color = editing.create_material_expression(material,
                    unreal.MaterialExpressionConstant3Vector, -350, -50)
        color.set_editor_property('constant', unreal.LinearColor(*rgb, 1))
        editing.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
        matte = editing.create_material_expression(material,
                    unreal.MaterialExpressionConstant, -350, 130)
        matte.set_editor_property('r', roughness)
        editing.connect_material_property(matte, '', unreal.MaterialProperty.MP_ROUGHNESS)
        if slot_name == 'Mask_Tarnished_Brass':
            metallic = editing.create_material_expression(material,
                        unreal.MaterialExpressionConstant, -350, 300)
            metallic.set_editor_property('r', .72)
            editing.connect_material_property(metallic, '', unreal.MaterialProperty.MP_METALLIC)
        editing.recompile_material(material)
        assets.save_loaded_asset(material)

slots = list(mesh.get_editor_property('static_materials'))
for slot in slots:
    source_name = str(slot.get_editor_property('material_slot_name'))
    path = destination + '/M_' + source_name
    material = assets.load_asset(path)
    if not material:
        raise RuntimeError('Unknown house mask material slot: ' + source_name)
    slot.set_editor_property('material_interface', material)
mesh.set_editor_property('static_materials', slots)
if not assets.save_loaded_asset(mesh):
    raise RuntimeError('Could not save imported house mask mesh')
unreal.log('HOUSE_MASK_IMPORTED: bounds=' + str(mesh.get_bounds()) +
           ' materials=' + str(len(slots)))
