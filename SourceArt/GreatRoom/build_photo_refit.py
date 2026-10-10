"""Build the reference-photo great room refit from the preserved house exports.

Run with Blender background mode. Existing house geometry, door assets, and the
porch bench correction remain untouched; this emits separate campaign variants.
Coordinates are the room's exported FBX metres (Unreal X = 100*x,
Unreal Y = -100*y, Unreal Z = 100*z).
"""
import bpy
import bmesh
import collections
import glob
import math
import os

PROJECT = '/Users/Stewart/Documents/Unreal Projects/SeniorSendoff'
EXPORTS = '/Users/Stewart/Projects/Senior sendoff/outputs/great-room-refit/exports'
OUT = os.path.join(PROJECT, 'SourceArt/GreatRoom')
os.makedirs(OUT, exist_ok=True)


def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)


def export_selected(objects, name):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    path = os.path.join(OUT, name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True,
                             apply_unit_scale=True, axis_forward='-Z',
                             axis_up='Y', bake_space_transform=False)
    print('GREAT_ROOM_BUILT', name, path)


def connected_parts(obj):
    mesh = obj.data
    parent = list(range(len(mesh.vertices)))

    def find(v):
        while parent[v] != v:
            parent[v] = parent[parent[v]]
            v = parent[v]
        return v

    for face in mesh.polygons:
        for vertex in face.vertices[1:]:
            parent[find(vertex)] = find(face.vertices[0])
    groups = collections.defaultdict(list)
    for face in mesh.polygons:
        groups[find(face.vertices[0])].append(face.index)
    for face_ids in groups.values():
        vertices = {i for index in face_ids for i in mesh.polygons[index].vertices}
        coords = [obj.matrix_world @ mesh.vertices[i].co for i in vertices]
        lo = [min(v[k] for v in coords) for k in range(3)]
        hi = [max(v[k] for v in coords) for k in range(3)]
        center = [(lo[k] + hi[k]) / 2 for k in range(3)]
        material = mesh.materials[mesh.polygons[face_ids[0]].material_index].name.split('.')[0]
        yield face_ids, lo, hi, center, material


def make_clear_variant(source, target, remove_front=False, remove_middle_rug=False):
    clear_scene()
    bpy.ops.import_scene.fbx(filepath=source)
    objects = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
    if len(objects) != 1:
        raise RuntimeError('Expected one imported mesh for ' + source)
    obj = objects[0]
    remove = set()
    removed = collections.Counter()
    for face_ids, lo, hi, center, material in connected_parts(obj):
        x, y, z = center
        front = (remove_front and 4.3 <= x <= 9.4 and -4.2 <= y <= 1.0
                 and hi[2] < 1.3)
        middle_rug = (remove_middle_rug and -2.0 <= x <= 1.8
                      and -3.4 <= y <= 1.2 and hi[2] < 0.075
                      and any(token in material for token in ('Rug', 'FabricCream')))
        if front or middle_rug:
            remove.update(face_ids)
            removed[material] += len(face_ids)
    if not remove:
        raise RuntimeError('Clear variant removed no faces: ' + target)
    before = len(obj.data.polygons)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.faces[i] for i in remove], context='FACES')
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    if len(obj.data.polygons) != before - len(remove):
        raise RuntimeError('Unexpected retained face count for ' + target)
    print('GREAT_ROOM_CLEARED', target, before, len(obj.data.polygons), dict(removed))
    if not obj.data.polygons:
        print('GREAT_ROOM_EMPTY_COLLIDER', target, 'remove its map actor')
        return
    export_selected([obj], target)


# The four visual chunks cross tile seams. The original porch bench adjustment
# is the baseline for its chunk; the other visual and collision chunks are the
# unedited house exports. We remove the old near seating and the unrelated teal
# middle rug, retaining the fireplace grouping and all wall fixtures.
variants = [
    ('SMF_Entrance_m1_m1_Solid', 'SMF_GR_m1_m1_ClearMiddleRug', False, True, None),
    ('SMF_Entrance_m1_p0_Solid', 'SMF_GR_m1_p0_ClearNear', True, True, None),
    ('SMF_Entrance_p0_m1_Solid', 'SMF_GR_p0_m1_BenchClearMiddleRug', False, True,
     os.path.join(PROJECT, 'SourceArt/HouseDoors/SMF_Entrance_p0_m1_BenchClear.fbx')),
    ('SMF_Entrance_p0_p0_Solid', 'SMF_GR_p0_p0_ClearNear', True, True, None),
    ('COLF_Entrance_p0_p0_Solid', 'COLF_GR_p0_p0_ClearNear', True, False, None),
]
for source_name, target_name, front, middle, override in variants:
    source = override or os.path.join(EXPORTS, source_name + '.fbx')
    make_clear_variant(source, target_name, front, middle)


# Photo-guided upholstered pieces. These use existing house materials after
# import; the front composition is a 4-seat dark leather sectional with a
# left chaise, tufted ottoman, worn rug, and a few restrained throw pillows.
clear_scene()
materials = {}


def mat(name, color):
    if name not in materials:
        material = bpy.data.materials.new(name)
        material.diffuse_color = (*color, 1.0)
        materials[name] = material
    return materials[name]


LEATHER = mat('MI_SSO_LeatherBrown', (0.10, 0.045, 0.035))
ACCENT = mat('MI_SSO_LeatherTobacco', (0.25, 0.11, 0.065))
OLIVE = mat('MI_SSO_LeatherOlive', (0.12, 0.13, 0.10))
CREAM = mat('MI_SSO_FabricCream', (0.70, 0.63, 0.51))
WOOD = mat('MI_SSO_WoodWalnut', (0.12, 0.055, 0.025))
METAL = mat('MI_SSO_MetalDark', (0.03, 0.025, 0.025))
parts = []


def box(name, center, size, radius, material, bevel_segments=3):
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(material)
    if radius:
        mod = obj.modifiers.new('Upholstery rounded corners', 'BEVEL')
        mod.width = radius
        mod.segments = bevel_segments
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
        for face in obj.data.polygons:
            face.use_smooth = True
    parts.append(obj)
    return obj


def pillow(name, center, size, material, angle=0):
    obj = box(name, center, size, min(size) * 0.24, material, 4)
    obj.rotation_euler[0] = angle
    return obj


# Sectional foundation and short walnut feet. The sofa faces the fireplace at
# decreasing x, while its chaise sits on the porch-door side of the rug.
box('Sectional dark base', (8.20, -1.34, 0.305), (1.38, 4.18, 0.30), 0.09, LEATHER)
box('Sectional low skirt', (8.20, -1.34, 0.16), (1.31, 4.05, 0.15), 0.035, LEATHER)
box('Sectional full back', (8.76, -1.34, 0.75), (0.31, 4.18, 0.68), 0.09, LEATHER)
box('Sectional porch arm', (8.19, -3.36, 0.66), (1.43, 0.27, 0.54), 0.10, LEATHER)
box('Sectional outer arm', (8.19, 0.69, 0.66), (1.43, 0.27, 0.54), 0.10, LEATHER)
for x in (7.62, 8.75):
    for y in (-3.20, 0.50):
        box('Sectional walnut foot', (x, y, 0.075), (0.105, 0.115, 0.15), 0.02, WOOD)
for i in range(5):
    y = -2.96 + i * 0.81
    box('Sectional seat %02d' % i, (8.08, y, 0.548),
        (1.04, 0.78, 0.235), 0.088, LEATHER, 4)
    back = box('Sectional back cushion %02d' % i, (8.58, y, 0.86),
               (0.355, 0.755, 0.47), 0.12, LEATHER, 4)
    back.rotation_euler[1] = -0.09

# The chaise projects toward the fireplace without entering the door lane.
box('Chaise foundation', (6.62, -2.99, 0.305), (2.25, 0.96, 0.30), 0.075, LEATHER)
box('Chaise outer arm', (6.94, -3.38, 0.61), (2.65, 0.23, 0.44), 0.085, LEATHER)
for i, x in enumerate((6.05, 7.07)):
    box('Chaise seat %02d' % i, (x, -2.99, 0.548),
        (1.04, 0.84, 0.235), 0.095, LEATHER, 4)
for x in (5.67, 7.56):
    box('Chaise walnut foot', (x, -3.22, 0.075),
        (0.105, 0.115, 0.15), 0.02, WOOD)

# Two pillows catch the real photo's off-white and olive accents.
pillow('Ivory throw pillow', (8.03, -0.12, 0.765), (0.47, 0.51, 0.18), CREAM, 0.18)
pillow('Olive throw pillow', (8.05, -2.84, 0.785), (0.49, 0.48, 0.19), OLIVE, -0.11)

# Dark upholstered tufted ottoman with rolled edge and short turned feet.
box('Ottoman upholstered base', (5.86, -0.93, 0.265),
    (1.47, 1.95, 0.37), 0.12, OLIVE, 4)
box('Ottoman rolled edge', (5.86, -0.93, 0.456),
    (1.53, 2.01, 0.16), 0.075, OLIVE, 4)
for x in (5.28, 6.44):
    for y in (-1.72, -0.14):
        box('Ottoman walnut foot', (x, y, 0.05),
            (0.13, 0.13, 0.10), 0.035, WOOD)

# Continuous soft tufting patch: low depressions at the buttons, raised leather
# between them. Grid density is modest so this does not burden the house map.
verts = []
faces = []
nx, ny = 30, 42
for ix in range(nx + 1):
    x = 5.14 + 1.44 * ix / nx
    for iy in range(ny + 1):
        y = -1.88 + 1.90 * iy / ny
        z = 0.542
        for bx in (5.46, 5.86, 6.26):
            for by in (-1.61, -1.16, -0.71, -0.26):
                distance = ((x-bx)/0.115)**2 + ((y-by)/0.13)**2
                z -= 0.046 * math.exp(-distance)
        z += 0.008 * math.sin((x-5.14)*23) * math.sin((y+1.88)*18)
        verts.append((x, y, z))
        if ix and iy:
            a = (ix-1)*(ny+1) + iy-1
            b = ix*(ny+1) + iy-1
            faces.append((a, b, b+1, a+1))
mesh = bpy.data.meshes.new('Ottoman tufted top')
mesh.from_pydata(verts, [], faces)
mesh.materials.append(OLIVE)
mesh.update()
top = bpy.data.objects.new('Ottoman tufted top', mesh)
bpy.context.collection.objects.link(top)
for polygon in mesh.polygons:
    polygon.use_smooth = True
parts.append(top)
for bx in (5.46, 5.86, 6.26):
    for by in (-1.61, -1.16, -0.71, -0.26):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=10, ring_count=6,
                                             radius=1, location=(bx, by, 0.505))
        button = bpy.context.object
        button.name = 'Ottoman inset button'
        button.scale = (0.028, 0.028, 0.010)
        button.data.materials.append(LEATHER)
        parts.append(button)

# Narrow recessed stitch lines connect the inset buttons and make the tufting
# legible from a standing player's viewpoint without using an expensive shader.
seam_data = bpy.data.curves.new('Ottoman tuft seams', type='CURVE')
seam_data.dimensions = '3D'
seam_data.bevel_depth = 0.0045
seam_data.bevel_resolution = 2
for bx in (5.46, 5.86, 6.26):
    for y1, y2 in zip((-1.61, -1.16, -0.71), (-1.16, -0.71, -0.26)):
        spline = seam_data.splines.new('POLY')
        spline.points.add(2)
        for point, xyz in zip(spline.points, ((bx, y1, 0.505),
                                              (bx, (y1+y2)/2, 0.543),
                                              (bx, y2, 0.505))):
            point.co = (*xyz, 1)
for by in (-1.61, -1.16, -0.71, -0.26):
    for x1, x2 in zip((5.46, 5.86), (5.86, 6.26)):
        spline = seam_data.splines.new('POLY')
        spline.points.add(2)
        for point, xyz in zip(spline.points, ((x1, by, 0.505),
                                              ((x1+x2)/2, by, 0.543),
                                              (x2, by, 0.505))):
            point.co = (*xyz, 1)
seam_data.materials.append(LEATHER)
seams = bpy.data.objects.new('Ottoman tuft seams', seam_data)
bpy.context.collection.objects.link(seams)
bpy.ops.object.select_all(action='DESELECT')
seams.select_set(True)
bpy.context.view_layer.objects.active = seams
bpy.ops.object.convert(target='MESH')
parts.append(bpy.context.object)

# One mesh and six retained house materials keeps rendering cost predictable.
bpy.ops.object.select_all(action='DESELECT')
for obj in parts:
    obj.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
furniture = bpy.context.object
furniture.name = 'SM_GR_PhotoSectionalOttoman'
export_selected([furniture], furniture.name)


# The visual never blocks movement; a three-box proxy follows the new silhouette.
clear_scene()
collision_material = mat('WorldGridMaterial', (0.3, 0.3, 0.3))
parts = []
box('Sectional block', (8.20, -1.34, 0.48), (1.41, 4.22, 0.96), 0, collision_material)
box('Chaise block', (6.62, -2.99, 0.43), (2.27, 0.98, 0.86), 0, collision_material)
box('Ottoman block', (5.86, -0.93, 0.27), (1.49, 1.97, 0.54), 0, collision_material)
bpy.ops.object.select_all(action='DESELECT')
for obj in parts:
    obj.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
proxy = bpy.context.object
proxy.name = 'COL_GR_PhotoSectionalOttoman'
export_selected([proxy], proxy.name)


# Rug plane: the texture carries its faded border and ornamental field.
clear_scene()
rug = mat('M_GR_VintageRug', (0.65, 0.52, 0.44))
bpy.ops.mesh.primitive_plane_add(size=1, location=(6.84, -1.62, 0.034))
plane = bpy.context.object
plane.name = 'SM_GR_VintageRug'
plane.scale = (4.5, 4.7, 1)
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
plane.data.materials.append(rug)
export_selected([plane], plane.name)
