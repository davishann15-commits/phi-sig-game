"""Import Fixer's Blender-authored sneakers and weighted shirt graphics.

Run in Unreal after native assembly. Inputs default to /private/tmp/fixer_details;
override with -FixerDetailsSource=/absolute/folder. No shared asset is altered.
"""
import json
import re
import traceback
from pathlib import Path

import unreal

ROOT = '/Game/MetaHumans/Fixer/MH_Fixer'
LEAN = '-FixerLeanDetails' in unreal.SystemLibrary.get_command_line()
LEAN_ROOT = '/Game/MetaHumans/FixerLean/MH_Fixer_Lean'
DEST = ROOT + '/Details'
match = re.search(r'-FixerDetailsSource=(?:"([^"]+)"|(\S+))', unreal.SystemLibrary.get_command_line())
SOURCE = Path((match.group(1) or match.group(2)) if match else '/private/tmp/fixer_details')
A = unreal.EditorAssetLibrary
E = unreal.MaterialEditingLibrary
T = unreal.AssetToolsHelpers.get_asset_tools()
REPORT = {'source': str(SOURCE), 'root': ROOT, 'static_meshes': [], 'print_mesh': None}


def scoped(asset):
    if not asset or not asset.get_path_name().split('.')[0].startswith(ROOT + '/'):
        raise RuntimeError('Refusing to write outside Fixer: ' + str(asset))
    return asset


def save(asset):
    if not A.save_loaded_asset(scoped(asset)):
        raise RuntimeError('Failed to save ' + asset.get_path_name())


def node(material, kind, **properties):
    result = E.create_material_expression(scoped(material), getattr(unreal, 'MaterialExpression' + kind))
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result


def connect_output(expression, prop, pin=''):
    if not E.connect_material_property(expression, pin, getattr(unreal.MaterialProperty, prop)):
        raise RuntimeError('Material output failed: ' + prop)


def plain_material(name, color, roughness, skeletal=False):
    material = A.load_asset(DEST + '/' + name)
    if not material:
        material = T.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    scoped(material)
    E.delete_all_material_expressions(material)
    material.set_editor_property('two_sided', True)
    material.set_editor_property('used_with_skeletal_mesh', skeletal)
    connect_output(node(material, 'Constant3Vector', constant=unreal.LinearColor(*color, 1.0)), 'MP_BASE_COLOR')
    connect_output(node(material, 'Constant', r=roughness), 'MP_ROUGHNESS')
    connect_output(node(material, 'Constant', r=0.20), 'MP_SPECULAR')
    E.recompile_material(material)
    save(material)
    return material


def neutral_shoe_upper():
    path = DEST + '/M_FixerSneakerUpper'
    material = A.load_asset(path)
    if not material:
        material = A.duplicate_asset('/Game/Characters/Cobble/C03/C03_Body_shoes05', path)
    scoped(material)
    if not isinstance(material, unreal.Material):
        raise RuntimeError('Expected direct source shoe Material; got ' + str(material))
    if A.get_metadata_tag(material, 'FixerNeutralSneaker') != '1':
        source = E.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
        source_pin = E.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_BASE_COLOR)
        if not source:
            raise RuntimeError('Source sneaker has no texture base-color connection')
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', 'C')
        gray = node(material, 'Custom',
                    code='float v=dot(C,float3(0.2126,0.7152,0.0722)); return saturate(v.xxx*float3(0.66,0.665,0.66));',
                    output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, inputs=[ci])
        if not E.connect_material_expressions(source, str(source_pin), gray, 'C'):
            raise RuntimeError('Failed neutral sneaker texture connection')
        connect_output(gray, 'MP_BASE_COLOR')
        connect_output(node(material, 'Constant', r=0.88), 'MP_ROUGHNESS')
        connect_output(node(material, 'Constant', r=0.18), 'MP_SPECULAR')
        A.set_metadata_tag(material, 'FixerNeutralSneaker', '1')
        E.recompile_material(material)
    save(material)
    return material


def import_asset(name, options, source_name=None):
    file = SOURCE / ((source_name or name) + '.fbx')
    if not file.is_file():
        raise RuntimeError('Missing Blender output ' + str(file))
    task = unreal.AssetImportTask()
    task.filename = str(file)
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    T.import_asset_tasks([task])
    asset = A.load_asset(DEST + '/' + name)
    if not asset:
        raise RuntimeError('Import did not create ' + name)
    return scoped(asset)


def assign_slots(mesh, materials, property_name):
    slots = list(mesh.get_editor_property(property_name))
    labels = []
    for index, slot in enumerate(slots):
        label = str(slot.get_editor_property('material_slot_name'))
        clean = re.sub(r'[._]\d+$', '', label)
        material = materials.get(label) or materials.get(clean)
        if not material:
            raise RuntimeError('Unmapped Fixer material slot: ' + label)
        slot.set_editor_property('material_interface', material)
        slots[index] = slot
        labels.append({'slot': label, 'material': material.get_path_name()})
    mesh.set_editor_property(property_name, slots)
    return labels


def import_shoes(materials):
    for side in ('L', 'R'):
        for prefix in ('SM_FixerSneaker_', 'SM_FixerSneakerBody_'):
            name = prefix + side
            options = unreal.FbxImportUI()
            for key, value in {
                'automated_import_should_detect_type': False,
                'mesh_type_to_import': unreal.FBXImportType.FBXIT_STATIC_MESH,
                'original_import_type': unreal.FBXImportType.FBXIT_STATIC_MESH,
                'import_as_skeletal': False, 'import_mesh': True,
                'import_materials': False, 'import_textures': False,
                'import_animations': False,
            }.items():
                options.set_editor_property(key, value)
            data = options.static_mesh_import_data
            for key, value in {
                'convert_scene': True, 'convert_scene_unit': True,
                'force_front_x_axis': False, 'import_uniform_scale': 1.0,
                'combine_meshes': True, 'auto_generate_collision': False,
                'normal_import_method': unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
            }.items():
                data.set_editor_property(key, value)
            mesh = import_asset(name, options)
            slots = assign_slots(mesh, materials, 'static_materials')
            bounds = mesh.get_bounds()
            ext = bounds.box_extent
            size = [float(ext.x * 2), float(ext.y * 2), float(ext.z * 2)]
            if not 20.0 <= max(size) <= 40.0:
                raise RuntimeError(name + ' imported at unexpected cm scale: ' + str(size))
            save(mesh)
            REPORT['static_meshes'].append({'path': mesh.get_path_name(), 'size_cm': size,
                'bounds': str(bounds), 'slots': slots,
                'space': 'native_body' if 'SneakerBody' in name else 'foot_bone_local'})


def import_shirt_prints(materials, body):
    options = unreal.FbxImportUI()
    for key, value in {
        'automated_import_should_detect_type': False,
        'mesh_type_to_import': unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        'original_import_type': unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        'import_as_skeletal': True, 'import_mesh': True,
        'import_materials': False, 'import_textures': False,
        'import_animations': False, 'create_physics_asset': False,
        'skeleton': body.get_editor_property('skeleton'),
    }.items():
        options.set_editor_property(key, value)
    # Native exported FBX has centimeter-valued skeletal coordinates but a
    # meter declaration. Match the established Runner native outfit import.
    for key, value in {
        'convert_scene': True, 'convert_scene_unit': False,
        'force_front_x_axis': False, 'import_uniform_scale': 1.0,
        'import_mesh_lods': False, 'update_skeleton_reference_pose': False,
        'use_t0_as_ref_pose': False,
        'normal_import_method': unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
    }.items():
        options.skeletal_mesh_import_data.set_editor_property(key, value)
    mesh = import_asset('SK_FixerLeanShirtPrints' if LEAN else 'SK_FixerShirtPrints', options, 'SK_FixerShirtPrints')
    labels = assign_slots(mesh, materials, 'materials')
    if mesh.get_editor_property('skeleton') != body.get_editor_property('skeleton'):
        raise RuntimeError('Shirt prints did not retain native Fixer body skeleton')
    garment = A.load_asset(LEAN_ROOT + '/Clothing/MH_Fixer_Lean_Outfits' if LEAN else ROOT + '/Clothing/MH_Fixer_Outfits')
    if not garment:
        raise RuntimeError('Native fitted garment is missing')
    if not unreal.SeniorCharacterAssetTools.match_braxton_garment_bind_pose(mesh, garment):
        raise RuntimeError('Shirt print bind pose normalization failed; helper must allow Fixer Details path')
    bounds = mesh.get_bounds()
    if not 100.0 < float(bounds.origin.z) < 155.0:
        raise RuntimeError('Shirt print vertices are not in native centimeter space: ' + str(bounds))
    save(mesh)
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    REPORT['print_mesh'] = {'path': mesh.get_path_name(), 'slots': labels,
        'skeleton': mesh.get_editor_property('skeleton').get_path_name(),
        'vertices': sub.get_num_verts(mesh, 0), 'native_bind_pose': True,
        'bind_source': garment.get_path_name(), 'bounds': str(mesh.get_bounds())}


def main():
    body = A.load_asset(LEAN_ROOT + '/Body/SKM_MH_Fixer_Lean_BodyMesh' if LEAN else ROOT + '/Body/SKM_MH_Fixer_BodyMesh')
    if not body:
        raise RuntimeError('Native Fixer body is missing')
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    shoe_materials = {
        'M_FixerSneakerUpper': neutral_shoe_upper(),
        'M_FixerShoeFoam': plain_material('M_FixerShoeFoam', (0.67, 0.67, 0.63), 0.88),
        'M_FixerShoeLace': plain_material('M_FixerShoeLace', (0.62, 0.62, 0.59), 0.92),
        'M_FixerAnkleSock': plain_material('M_FixerAnkleSock', (0.69, 0.68, 0.64), 0.94),
        'M_FixerShoeRubber': plain_material('M_FixerShoeRubber', (0.14, 0.145, 0.14), 0.90),
    }
    print_materials = {
        name: plain_material(name, color, 0.93, skeletal=True)
        for name, color in {
            'M_FixerPrintCoral': (0.61, 0.23, 0.35),
            'M_FixerPrintNavy': (0.017, 0.025, 0.049),
            'M_FixerPrintCream': (0.65, 0.65, 0.59),
            'M_FixerPrintTeal': (0.18, 0.46, 0.42),
        }.items()
    }
    if not LEAN:
        import_shoes(shoe_materials)
    import_shirt_prints(print_materials, body)
    REPORT['complete'] = True
    unreal.log('FIXER_DETAILS_IMPORTED ' + json.dumps(REPORT))


try:
    main()
except Exception:
    REPORT['complete'] = False
    REPORT['error'] = traceback.format_exc()
    unreal.log_error(REPORT['error'])
    raise
finally:
    Path('/private/tmp/fixer_details_import_report.json').write_text(json.dumps(REPORT, indent=2))
