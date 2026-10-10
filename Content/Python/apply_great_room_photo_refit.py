"""Apply the photo-guided great-room furniture refit to Chapter01_House.

The generated FBX sources live in SourceArt/GreatRoom; run its Blender build
script before this Unreal Python commandlet. This only changes great-room
furniture meshes, the matching collision, and the campaign map assignments.
"""
import json
import os
import re
import unreal

root = unreal.Paths.project_dir()
source = os.path.join(root, 'SourceArt/GreatRoom')
mesh_dest = '/Game/SeniorSendOff/House/GreatRoom'
house_materials = '/Game/SeniorSendOff/House/Materials/'
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def import_asset(filename, name):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(source, filename)
    task.destination_path = mesh_dest
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if filename.lower().endswith('.fbx'):
        ui = unreal.FbxImportUI()
        ui.import_mesh = True
        ui.import_as_skeletal = False
        ui.import_materials = False
        ui.import_textures = False
        task.options = ui
    tools.import_asset_tasks([task])
    obj = assets.load_asset(mesh_dest + '/' + name)
    if not obj:
        raise RuntimeError('Import failed: ' + filename)
    return obj


names = [
    'SMF_GR_m1_m1_ClearMiddleRug',
    'SMF_GR_m1_p0_ClearNear',
    'SMF_GR_p0_m1_BenchClearMiddleRug',
    'SMF_GR_p0_p0_ClearNear',
    'COLF_GR_p0_p0_ClearNear',
    'SM_GR_PhotoSectionalOttoman',
    'COL_GR_PhotoSectionalOttoman',
    'SM_GR_VintageRug',
]
meshes = {}
for name in names:
    mesh = import_asset(name + '.fbx', name)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError('Imported asset is not a static mesh: ' + name)
    meshes[name] = mesh

texture = import_asset('T_GR_VintageRug_Base.png', 'T_GR_VintageRug_Base')
if not isinstance(texture, unreal.Texture2D):
    raise RuntimeError('Rug texture import failed')
material = assets.load_asset(mesh_dest + '/M_GR_VintageRug')
if not material:
    material = tools.create_asset('M_GR_VintageRug', mesh_dest,
                                  unreal.Material, unreal.MaterialFactoryNew())
if not isinstance(material, unreal.Material):
    raise RuntimeError('Rug material creation failed')
editing = unreal.MaterialEditingLibrary
editing.delete_all_material_expressions(material)
sample = editing.create_material_expression(material,
                                            unreal.MaterialExpressionTextureSample,
                                            -350, -50)
sample.texture = texture
editing.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
rough = editing.create_material_expression(material,
                                           unreal.MaterialExpressionConstant,
                                           -350, 130)
rough.r = 0.94
editing.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
material.set_editor_property('two_sided', True)
editing.recompile_material(material)
assets.save_loaded_asset(material)

def solid_material(name, color, roughness, specular):
    path = mesh_dest + '/' + name
    result = assets.load_asset(path) if assets.does_asset_exist(path) else None
    if not result:
        result = tools.create_asset(name, mesh_dest,
                                    unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(result, unreal.Material):
        raise RuntimeError('Cannot create upholstery material ' + name)
    editing.delete_all_material_expressions(result)
    base = editing.create_material_expression(result,
                                              unreal.MaterialExpressionConstant3Vector,
                                              -350, -50)
    base.set_editor_property('constant',
                             unreal.LinearColor(color[0], color[1], color[2], 1))
    editing.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    matte = editing.create_material_expression(result,
                                               unreal.MaterialExpressionConstant,
                                               -350, 130)
    matte.r = roughness
    editing.connect_material_property(matte, '', unreal.MaterialProperty.MP_ROUGHNESS)
    shine = editing.create_material_expression(result,
                                               unreal.MaterialExpressionConstant,
                                               -350, 260)
    shine.r = specular
    editing.connect_material_property(shine, '', unreal.MaterialProperty.MP_SPECULAR)
    editing.recompile_material(result)
    assets.save_loaded_asset(result)
    return result

upholstery = {
    0: solid_material('M_GR_ChocolateLeather', (0.055, 0.023, 0.016), 0.46, 0.52),
    1: solid_material('M_GR_WalnutWood', (0.045, 0.020, 0.010), 0.59, 0.42),
    2: solid_material('M_GR_IvoryPillow', (0.59, 0.51, 0.40), 0.90, 0.30),
    3: solid_material('M_GR_OliveLeather', (0.078, 0.068, 0.049), 0.55, 0.48),
}

for name, mesh in meshes.items():
    for index, slot in enumerate(mesh.static_materials):
        if name == 'SM_GR_VintageRug':
            target = material
        elif name.startswith(('COLF_', 'COL_')):
            continue
        elif name == 'SM_GR_PhotoSectionalOttoman':
            target = upholstery[index]
        else:
            slot_name = re.sub(r'_\d{3}$', '', str(slot.material_slot_name).split('.')[0])
            target = assets.load_asset(house_materials + slot_name)
            if not target:
                raise RuntimeError('No house material for ' + name + ' slot ' + slot_name)
        mesh.set_material(index, target)
    if name.startswith(('COLF_', 'COL_')):
        body = mesh.get_editor_property('body_setup')
        if not body:
            raise RuntimeError('Missing collision body on ' + name)
        body.set_editor_property('collision_trace_flag',
                                 unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    if not assets.save_loaded_asset(mesh):
        raise RuntimeError('Cannot save ' + name)
    unreal.log('GREAT_ROOM_IMPORT ' + name + ' ' + str(mesh.get_bounds()))

unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors = {actor.get_actor_label(): actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()}
assignments = {
    'SMF_Entrance_m1_m1_Solid': 'SMF_GR_m1_m1_ClearMiddleRug',
    'SMF_Entrance_m1_p0_Solid': 'SMF_GR_m1_p0_ClearNear',
    'SMF_Entrance_p0_m1_Solid': 'SMF_GR_p0_m1_BenchClearMiddleRug',
    'SMF_Entrance_p0_p0_Solid': 'SMF_GR_p0_p0_ClearNear',
    'COLF_Entrance_p0_p0_Solid': 'COLF_GR_p0_p0_ClearNear',
}
for label, mesh_name in assignments.items():
    actor = actors.get(label)
    if not isinstance(actor, unreal.StaticMeshActor):
        raise RuntimeError('Missing great-room chunk actor ' + label)
    actor.static_mesh_component.set_static_mesh(meshes[mesh_name])
    if label.startswith('SMF_'):
        # The dedicated COLF chunks provide furniture blocking. FBX reimport
        # can otherwise create a broad auto-collision hull across the room.
        actor.static_mesh_component.set_collision_profile_name('NoCollision')
    unreal.log('GREAT_ROOM_ASSIGNED ' + label + ' -> ' + mesh_name)

old_collision = actors.get('COLF_Entrance_m1_p0_Solid')
if old_collision:
    if not isinstance(old_collision, unreal.StaticMeshActor):
        raise RuntimeError('Old straight-couch collision actor changed class')
    unreal.EditorLevelLibrary.destroy_actor(old_collision)

# Idempotently replace only our own new actors if this script is rerun.
for label in ('SM_GR_PhotoSectionalOttoman', 'COL_GR_PhotoSectionalOttoman',
              'SM_GR_VintageRug'):
    existing = actors.get(label)
    if existing:
        unreal.EditorLevelLibrary.destroy_actor(existing)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if not actor:
        raise RuntimeError('Cannot place ' + label)
    actor.set_actor_label(label)
    actor.static_mesh_component.set_static_mesh(meshes[label])
    actor.static_mesh_component.set_collision_profile_name(
        'BlockAll' if label.startswith('COL_') else 'NoCollision')
    if label.startswith('COL_'):
        actor.static_mesh_component.set_visibility(False)
    actor.set_folder_path('Great room photo refit')

# Bounds check confirms imported FBX world coordinates survived the round-trip.
sectional = next(a for a in unreal.EditorLevelLibrary.get_all_level_actors()
                 if a.get_actor_label() == 'SM_GR_PhotoSectionalOttoman')
origin, extent = sectional.get_actor_bounds(False)
lo, hi = origin - extent, origin + extent
if not (500 < lo.x < 530 and 885 < hi.x < 910 and
        -95 < lo.y < -55 and 345 < hi.y < 365 and
        -0.1 <= lo.z < 10 and 105 < hi.z < 115):
    raise RuntimeError('Sectional bounds unexpected: ' + str((lo, hi)))

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError('Could not save Chapter01_House refit')
manifest = {
    'map': '/Game/Story/Maps/Chapter01_House',
    'assigned': assignments,
    'newActors': ['SM_GR_PhotoSectionalOttoman', 'COL_GR_PhotoSectionalOttoman',
                  'SM_GR_VintageRug'],
    'removedCollisionActor': 'COLF_Entrance_m1_p0_Solid',
    'sectionalBoundsCm': [[lo.x, lo.y, lo.z], [hi.x, hi.y, hi.z]],
}
with open('/Users/Stewart/Projects/Senior sendoff/outputs/great-room-refit/work/applied.json', 'w') as f:
    json.dump(manifest, f, indent=2)
unreal.log('GREAT_ROOM_REFIT_SAVED')
