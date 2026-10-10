"""Make the compact two-sided lever used where an imported leaf lacks a handle."""
import bpy
import math
import os

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
brass = bpy.data.materials.new('MI_SSO_MetalBrass')
dark = bpy.data.materials.new('MI_SSO_MetalDark')
pieces = []


def finish(obj, material=brass, bevel=0):
    obj.data.materials.append(material)
    if bevel:
        mod = obj.modifiers.new('Soft machined edges', 'BEVEL')
        mod.width = bevel
        mod.segments = 3
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
    for face in obj.data.polygons:
        face.use_smooth = True
    pieces.append(obj)
    return obj


def box(name, at, size, bevel, material=brass):
    bpy.ops.mesh.primitive_cube_add(size=1, location=at)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish(obj, material, bevel)


def cylinder(name, at, radius, depth, material=brass, vertices=24):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=at)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler[0] = math.pi / 2
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    return finish(obj, material, .001)


for side in (-1, 1):
    suffix = 'A' if side > 0 else 'B'
    # The two plates sit just outside an 8 cm thick door.
    box('Backplate_' + suffix, (0, side * .047, 0), (.055, .008, .165), .004)
    cylinder('Rose_' + suffix, (0, side * .055, 0), .028, .009)
    cylinder('Spindle_' + suffix, (0, side * .071, 0), .010, .031)
    bar = box('Lever_' + suffix, (-.055, side * .091, -.006), (.125, .018, .018), .007)
    bar.rotation_euler[1] = -.08
    for z in (-.063, .063):
        cylinder('Screw_' + suffix, (0, side * .053, z), .0042, .002)
        box('ScrewSlot_' + suffix, (0, side * .0548, z), (.0055, .001, .0009), .0002, dark)

bpy.ops.object.select_all(action='DESELECT')
for obj in pieces:
    obj.select_set(True)
bpy.context.view_layer.objects.active = pieces[0]
bpy.ops.object.convert(target='MESH')
bpy.ops.object.join()
bpy.context.object.name = 'SM_HouseLeverHandle'
destination = os.path.join(os.path.dirname(__file__), 'SM_HouseLeverHandle.fbx')
bpy.ops.export_scene.fbx(filepath=destination, use_selection=True, apply_unit_scale=True,
                         axis_forward='-Z', axis_up='Y', bake_space_transform=False)
print('DOOR_HANDLE_EXPORTED', destination)
