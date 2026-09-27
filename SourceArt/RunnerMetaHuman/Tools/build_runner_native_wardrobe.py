"""Tailor the assembled Runner MetaHuman's *native skinned* wardrobe.

Keeping the original garment vertices, vertex groups and bind matrices avoids
the deformation produced by transferring weights from the older C02 study.
This intentionally starts with a clean, intact outfit; trim and photo details
can be added only after the core silhouette survives an animated preview.
"""
from pathlib import Path
from math import exp, sin, pi

import bpy
import bmesh
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

ROOT = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman')
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(ROOT / 'RunnerNativeOutfit.fbx'), use_anim=False)
rig = next(obj for obj in bpy.data.objects if obj.type == 'ARMATURE')
outfit = next(obj for obj in bpy.data.objects if obj.type == 'MESH' and len(obj.data.vertices) > 1000)
for obj in list(bpy.data.objects):
    if obj not in (rig, outfit):
        bpy.data.objects.remove(obj, do_unlink=True)

if len(outfit.vertex_groups) < 40:
    raise RuntimeError('Native garment did not retain its bone weights')
if {poly.material_index for poly in outfit.data.polygons} != {0, 1}:
    raise RuntimeError('Native garment sections have changed; inspect before tailoring')

# The default garment is sewn from disconnected pattern components. Remove
# only the four sleeve/cuff components on each side; leave both torso panels,
# their hem, and the native skin weights untouched. The jersey-body assembly
# supplies the previously omitted shoulder skin at runtime.
neighbors = {index: set() for face in outfit.data.polygons
             if face.material_index == 1 for index in face.vertices}
for face in outfit.data.polygons:
    if face.material_index == 1:
        for index in face.vertices:
            neighbors[index].update(other for other in face.vertices if other != index)
seen = set()
sleeve_indices = set()
removed_parts = []
sleeve_seams = []
for start in neighbors:
    if start in seen:
        continue
    pending = [start]
    seen.add(start)
    part = []
    while pending:
        current = pending.pop()
        part.append(current)
        for other in neighbors[current]:
            if other not in seen:
                seen.add(other)
                pending.append(other)
    coords = [outfit.data.vertices[index].co for index in part]
    min_x, max_x = min(p.x for p in coords), max(p.x for p in coords)
    min_z, max_z = min(p.z for p in coords), max(p.z for p in coords)
    if (min_z > 131 and max_z < 157 and
        ((min_x > 17 and max_x > 25) or (max_x < -17 and min_x < -25))):
        sleeve_indices.update(part)
        removed_parts.append((len(part), round(min_x), round(max_x)))
        if len(part) == 840:
            members = set(part)
            edge_count = {}
            for face in outfit.data.polygons:
                if face.material_index != 1 or face.vertices[0] not in members:
                    continue
                vertices = list(face.vertices)
                for a, b in zip(vertices, vertices[1:] + vertices[:1]):
                    edge = tuple(sorted((a, b)))
                    edge_count[edge] = edge_count.get(edge, 0) + 1
            boundary = {}
            for (a, b), count in edge_count.items():
                if count == 1:
                    boundary.setdefault(a, []).append(b)
                    boundary.setdefault(b, []).append(a)
            print('RUNNER_SLEEVE_EDGE_DEGREES', len(boundary),
                  sorted({len(link) for link in boundary.values()}))
            start = min(boundary, key=lambda j: abs(outfit.data.vertices[j].co.x))
            ordered = [start]
            previous = None
            current = start
            for _ in range(len(boundary) + 2):
                options = [j for j in boundary[current] if j != previous]
                if not options:
                    break
                following = options[0]
                if following == start:
                    break
                ordered.append(following)
                previous, current = current, following
            usable = [abs(outfit.data.vertices[j].co.x) < 23 for j in ordered]
            print('RUNNER_SLEEVE_EDGE_PATH', len(ordered), sum(usable),
                  'changes', sum(usable[k] != usable[k - 1] for k in range(len(usable))))
            pivot = usable.index(False)
            rotated = ordered[pivot:] + ordered[:pivot]
            inner = [index for index in rotated
                     if abs(outfit.data.vertices[index].co.x) < 23]
            if len(inner) < 60:
                raise RuntimeError('The jersey armhole seam is incomplete')
            seam = []
            for index in inner:
                vertex = outfit.data.vertices[index]
                weights = {outfit.vertex_groups[g.group].name: g.weight
                           for g in vertex.groups}
                seam.append((vertex.co.copy(), weights))
            sleeve_seams.append(seam)
if len(removed_parts) != 6:
    raise RuntimeError('Unexpected sleeve-pattern layout: ' + str(removed_parts))
mesh_edit = bmesh.new()
mesh_edit.from_mesh(outfit.data)
bmesh.ops.delete(mesh_edit, geom=[vert for vert in mesh_edit.verts
                                 if vert.index in sleeve_indices], context='VERTS')
mesh_edit.to_mesh(outfit.data)
mesh_edit.free()
outfit.data.update()
print('RUNNER_JERSEY_SLEEVES_REMOVED', removed_parts)

# Let the shirt fall over the waistband like an untucked basketball jersey.
# Its skinned torso panels and sewn hem move together; the shorts stay intact.
shirt_vertices = {index for face in outfit.data.polygons
                  if face.material_index == 1 for index in face.vertices}
for index in shirt_vertices:
    p = outfit.data.vertices[index].co
    fall = min(1.0, max(0.0, (118.0 - p.z) / 17.0))
    if fall:
        p.z -= 4.0 * fall
        p.x *= 1.0 + .075 * fall
        p.y += (-1.2 if p.y < 0 else 1.0) * fall

# Pull the centre of the forward collar into a shallow athletic V. The mesh
# sections at the neckline move together so their trim still follows the rib.
for vertex in outfit.data.vertices:
    p = vertex.co
    if p.y < -2.5 and p.z > 153 and abs(p.x) < 11:
        across = 1 - abs(p.x) / 11
        forward = min(1.0, max(0.0, (-p.y - 2.5) / 4.0))
        up = min(1.0, max(0.0, (p.z - 153) / 8.0))
        p.z -= 7.0 * across * forward * up
    if -11 < p.y < -2.5 and p.z > 150 and abs(p.x) < 11:
        # The assembled Face includes the lower neck. Its front surface is
        # about 2-3 cm ahead of the factory collar at the centre, hiding the
        # binding completely once the jersey opens up. Ease only the narrow
        # rib band forward; the broad torso remains in its native position.
        across = 1 - abs(p.x) / 11
        p.y -= 3.1 * across

# Ease out the small inward pinch below the V. This is confined to the front
# chest panel, fading to zero before the collar, side seams and printed number.
for index in shirt_vertices:
    p = outfit.data.vertices[index].co
    if p.y < -4.0 and 143.0 < p.z < 154.0 and abs(p.x) < 10.0:
        across = (1.0 - (p.x / 10.0) ** 2) ** 2
        height = sin(pi * (p.z - 143.0) / 11.0)
        p.y -= 1.15 * across * height
        if 149.0 < p.z < 152.6:
            # The native shirt has a recessed row just below the neckline.
            # Bring that row level with the fabric on either side, without
            # moving the V-shaped binding itself.
            p.y -= 1.60 * across * exp(-((p.z - 151.8) / 1.5) ** 2)

# The stock shirt inherited a concave mid-chest profile from the underlying
# body. From the side it drew inward below the breast, then jutted outward at
# the upper chest. A loose basketball jersey should fall over that contour.
# Keep the center front nearly vertical, fade the correction toward the side
# seams and hem, and never move cloth inward toward the visible skin.
for index in shirt_vertices:
    p = outfit.data.vertices[index].co
    if p.y >= 0.0 or not 113.0 < p.z < 153.0 or abs(p.x) >= 16.0:
        continue
    center_front = (-15.0 + (p.z - 118.0) / 20.0 if p.z <= 138.0
                    else -14.0 - .25 * (p.z - 138.0) / 14.0)
    desired_y = center_front + .040 * p.x * p.x
    lower_fade = min(1.0, max(0.0, (p.z - 113.0) / 9.0))
    upper_fade = min(1.0, max(0.0, (153.0 - p.z) / 3.0))
    p.y += min(0.0, desired_y - p.y) * lower_fade * upper_fade

shorts = bpy.data.materials.new('Runner_NativeShorts')
jersey = bpy.data.materials.new('Runner_NativeJersey')
section_ids = [face.material_index for face in outfit.data.polygons]
outfit.data.materials.clear()
outfit.data.materials.append(shorts)
outfit.data.materials.append(jersey)
for face, section_id in zip(outfit.data.polygons, section_ids):
    face.material_index = section_id
    face.use_smooth = True

# Transfer the jersey's photographed lettering onto the *native* shirt
# surface. The old study's clothing mesh is not reused; only its seven small
# print shapes are conformed to this fitted fabric and reweighted to its bones.
source_study = ROOT.parent / 'RunnerV2' / 'RunnerV2.blend'
print_names = [
    'Garment print.001', 'Garment print.003',
    'RunnerV2_GoldOutline_Garment_print.001',
    'RunnerV2_GoldOutline_Garment_print.003',
]
with bpy.data.libraries.load(str(source_study), link=False) as (src, dst):
    dst.objects = [name for name in print_names if name in src.objects]
loaded = {obj.name: obj for obj in dst.objects if obj is not None}
if len(loaded) != len(print_names):
    raise RuntimeError('Reference jersey lettering is incomplete')

surface = BVHTree.FromPolygons(
    [v.co.copy() for v in outfit.data.vertices],
    [list(poly.vertices) for poly in outfit.data.polygons])
shirt_indices = sorted({index for poly in outfit.data.polygons if poly.material_index == 1
                        for index in poly.vertices})
shirt_tree = KDTree(len(shirt_indices))
for index in shirt_indices:
    shirt_tree.insert(outfit.data.vertices[index].co, index)
shirt_tree.balance()
group_names = {group.index: group.name for group in outfit.vertex_groups}

def garment_surface_weights(hit, polygon_index):
    """Interpolate the actual face's bone weights onto a sewn graphic point."""
    face = outfit.data.polygons[polygon_index]
    indices = list(face.vertices)
    triangles = ([indices] if len(indices) == 3 else
                 ([indices[:3], [indices[0], indices[2], indices[3]]]
                  if len(indices) == 4 else []))
    if not triangles:
        raise RuntimeError('Runner shirt contains an unsupported polygon')
    chosen = None
    for triangle in triangles:
        a, b, c = [outfit.data.vertices[index].co for index in triangle]
        ab, ac, ap = b - a, c - a, hit - a
        d00, d01, d11 = ab.dot(ab), ab.dot(ac), ac.dot(ac)
        d20, d21 = ap.dot(ab), ap.dot(ac)
        denominator = d00 * d11 - d01 * d01
        if abs(denominator) < 1e-7:
            continue
        v = (d11 * d20 - d01 * d21) / denominator
        w = (d00 * d21 - d01 * d20) / denominator
        bary = (1 - v - w, v, w)
        if min(bary) >= -.002:
            chosen = (triangle, bary)
            break
    if chosen is None:
        raise RuntimeError('Runner print point missed its target triangle')
    triangle, bary = chosen
    weights = {}
    for index, portion in zip(triangle, bary):
        for group in outfit.data.vertices[index].groups:
            label = group_names[group.group]
            weights[label] = weights.get(label, 0.0) + group.weight * portion
    total = sum(max(0.0, weight) for weight in weights.values())
    return {label: max(0.0, weight) / total for label, weight in weights.items()}

black = bpy.data.materials.new('Runner_PrintBlack')
gold = bpy.data.materials.new('Runner_PrintGold')
trim_black = bpy.data.materials.new('Runner_TrimBlack')
trim_gold = bpy.data.materials.new('Runner_TrimGold')
stripe_dark = bpy.data.materials.new('Runner_PinstripeDark')
stripe_gold = bpy.data.materials.new('Runner_PinstripeGold')
outfit.data.materials.append(trim_black)
outfit.data.materials.append(trim_gold)

# The native neckline is a set of separate knitted rib pieces. Give the outer
# rib a dark edge and the inner band a narrow warm-gold accent.
collar_neighbors = {}
for face in outfit.data.polygons:
    if face.material_index != 1:
        continue
    for index in face.vertices:
        collar_neighbors.setdefault(index, set()).update(face.vertices)
collar_seen = set()
collar_parts = []
for start in collar_neighbors:
    if start in collar_seen:
        continue
    pending = [start]
    collar_seen.add(start)
    part = set()
    while pending:
        index = pending.pop()
        part.add(index)
        for other in collar_neighbors[index]:
            if other not in collar_seen:
                collar_seen.add(other)
                pending.append(other)
    coords = [outfit.data.vertices[index].co for index in part]
    if (len(part) >= 70 and min(p.z for p in coords) > 148 and
        max(p.z for p in coords) > 161 and
        max(abs(p.x) for p in coords) < 11):
        collar_parts.append((len(part), part))
for size, part in collar_parts:
    material_index = 3 if size == 486 else 2
    for face in outfit.data.polygons:
        if face.material_index == 1 and face.vertices[0] in part:
            face.material_index = material_index
print('RUNNER_JERSEY_COLLAR_RIBS', [size for size, _ in collar_parts])
for size, part in collar_parts:
    if size == 648:
        samples = []
        for x in range(-8, 9, 2):
            candidates = [index for index in part
                          if abs(outfit.data.vertices[index].co.x - x) < .65 and
                          outfit.data.vertices[index].co.y < 0]
            if candidates:
                index = min(candidates, key=lambda j: outfit.data.vertices[j].co.y)
                p = outfit.data.vertices[index].co
                samples.append((x, round(p.x, 1), round(p.y, 1), round(p.z, 1)))
        print('RUNNER_COLLAR_FRONT_SAMPLES', samples)

pieces = []

def make_trim_tube(name, seam, radius, material, outward=0.0):
    sign = 1 if sum(point.x for point, _ in seam) > 0 else -1
    points = [point + Vector((sign * outward, 0, 0)) for point, _ in seam]
    count = len(points)
    sides = 6
    vertices = []
    faces = []
    for index, point in enumerate(points):
        tangent = (points[min(index + 1, count - 1)] -
                   points[max(index - 1, 0)]).normalized()
        reference = Vector((0, 0, 1))
        if abs(tangent.dot(reference)) > .94:
            reference = Vector((0, 1, 0))
        cross = tangent.cross(reference).normalized()
        second = tangent.cross(cross).normalized()
        for side in range(sides):
            angle = 2 * 3.141592653589793 * side / sides
            vertices.append(point + radius *
                            (cross * __import__('math').cos(angle) +
                             second * __import__('math').sin(angle)))
    for index in range(count - 1):
        for side in range(sides):
            following = (side + 1) % sides
            faces.append((index * sides + side, index * sides + following,
                          (index + 1) * sides + following,
                          (index + 1) * sides + side))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    mesh.materials.append(material)
    piece = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(piece)
    groups = {label: piece.vertex_groups.new(name=label) for label in group_names.values()}
    for index, (_, weights) in enumerate(seam):
        for label, weight in weights.items():
            if weight > .0001:
                groups[label].add(list(range(index * sides, (index + 1) * sides)),
                                  weight, 'REPLACE')
    piece.parent = rig
    piece.matrix_world = outfit.matrix_world.copy()
    modifier = piece.modifiers.new('Runner native skin', 'ARMATURE')
    modifier.object = rig
    for face in mesh.polygons:
        face.use_smooth = True
    return piece

for index, seam in enumerate(sleeve_seams):
    pieces.append(make_trim_tube('Runner_ArmholeBlack_' + str(index), seam,
                                 .53, trim_black))
    pieces.append(make_trim_tube('Runner_ArmholeGold_' + str(index), seam,
                                 .24, trim_gold, outward=.60))
for name in print_names:
    original = loaded[name]
    piece = bpy.data.objects.new('Native_' + name.replace(' ', '_'), original.data.copy())
    bpy.context.collection.objects.link(piece)
    piece.data.materials.clear()
    piece.data.materials.append(gold if 'GoldOutline' in name else black)
    back = name.endswith('.002') or name.endswith('.003')
    is_gold = 'GoldOutline' in name
    design = name.replace('RunnerV2_GoldOutline_', '').replace('Garment_print', 'Garment print')
    scale_x, scale_z, raise_z = {
        'Garment print.001': (1.39, 1.90, -2.0), # front 2
        'Garment print.003': (1.72, 2.30, -0.5), # raised, slightly smaller back 2
    }[design]
    mapped = []
    for vertex in piece.data.vertices:
        point = original.matrix_world @ vertex.co
        point *= 100.0 * (1.75 / 1.92)
        point.z += 8.0
        mapped.append(point)
    center_x = (min(p.x for p in mapped) + max(p.x for p in mapped)) * .5
    center_z = (min(p.z for p in mapped) + max(p.z for p in mapped)) * .5
    group_map = {label: piece.vertex_groups.new(name=label) for label in group_names.values()}
    for vertex, point in zip(piece.data.vertices, mapped):
        point.x = center_x + (point.x - center_x) * scale_x
        point.z = center_z + (point.z - center_z) * scale_z + raise_z
        direction = Vector((0, -1 if back else 1, 0))
        start = Vector((point.x, 100 if back else -100, point.z))
        hit, normal, polygon_index, _ = surface.ray_cast(start, direction, 200)
        if hit is None or outfit.data.polygons[polygon_index].material_index != 1:
            raise RuntimeError('Runner lettering projected outside the jersey')
        outward = 1 if back else -1
        if normal.y * outward < 0:
            normal = -normal
        vertex.co = hit + normal * (.36 if is_gold else .48)
        weights = garment_surface_weights(hit, polygon_index)
        for label, weight in weights.items():
            if weight > .0001:
                group_map[label].add([vertex.index], weight, 'REPLACE')
    coords = [vertex.co for vertex in piece.data.vertices]
    print('RUNNER_PRINT_BOUNDS', name,
          tuple(round(max(p[i] for p in coords) - min(p[i] for p in coords), 2)
                for i in (0, 2)),
          round(sum(p.z for p in coords) / len(coords), 2))
    piece.parent = rig
    piece.matrix_world = outfit.matrix_world.copy()
    modifier = piece.modifiers.new('Runner native skin', 'ARMATURE')
    modifier.object = rig
    pieces.append(piece)

# Rebuild the wordmarks with clean athletic lettering. The old proof-of-
# concept font was fragmented at its outer letters and looked unlike the
# photographed jersey. These temporary meshes are baked into the fabric and
# deliberately never exported as raised geometry.
word_font = bpy.data.fonts.load('/System/Library/Fonts/Supplemental/DIN Condensed Bold.ttf')
def add_wordmark(name, label, back, material, width, height, center_z, arch):
    curve = bpy.data.curves.new(name, type='FONT')
    curve.body = label
    curve.font = word_font
    curve.align_x = 'CENTER'
    curve.align_y = 'CENTER'
    curve.fill_mode = 'BOTH'
    obj = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(obj)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.convert(target='MESH')
    mesh = obj.data
    bounds_x = [vertex.co.x for vertex in mesh.vertices]
    bounds_y = [vertex.co.y for vertex in mesh.vertices]
    midpoint_x = (min(bounds_x) + max(bounds_x)) * .5
    midpoint_y = (min(bounds_y) + max(bounds_y)) * .5
    divisor_x = max(bounds_x) - min(bounds_x)
    divisor_y = max(bounds_y) - min(bounds_y)
    for vertex in mesh.vertices:
        x = (vertex.co.x - midpoint_x) * width / divisor_x
        if back:
            x = -x
        z = (vertex.co.y - midpoint_y) * height / divisor_y + center_z
        z += arch * (1.0 - (2.0 * x / width) ** 2)
        vertex.co = (x, 20.0 if back else -20.0, z)
    mesh.materials.append(material)
    pieces.append(obj)

for back, label, width, center_z, arch in (
        (False, 'BUZZ CITY', 28.0, 141.0, .8),
        (True, 'BALL', 21.0, 145.0, 0.0)):
    side = 'Back' if back else 'Front'
    add_wordmark('Runner_' + side + 'WordGold', label, back, gold,
                 width + .85, 5.55, center_z, arch)
    add_wordmark('Runner_' + side + 'WordBlack', label, back, black,
                 width, 5.05, center_z, arch)

# Lay out fine, low-contrast pinstripes on both sides of the sewn fabric. They
# and the photographed lettering are baked into the native garment UVs below.
for back, x, material in (
        (False, -13.2, stripe_dark), (False, -8.6, stripe_gold),
        (False, -3.6, stripe_dark), (False, 3.6, stripe_dark),
        (False, 8.6, stripe_gold), (False, 13.2, stripe_dark),
        (True, -13.2, stripe_dark), (True, -8.6, stripe_gold),
        (True, -3.6, stripe_dark), (True, 3.6, stripe_dark),
        (True, 8.6, stripe_gold), (True, 13.2, stripe_dark)):
    stripe_verts = []
    stripe_faces = []
    for row in range(32):
        z = 100.0 + row * 1.72
        for edge in (-1, 1):
            edge_x = x + edge * (.12 if material == stripe_dark else .16)
            hit, _, _, _ = surface.ray_cast(
                Vector((edge_x, 100 if back else -100, z)),
                Vector((0, -1 if back else 1, 0)), 200)
            if hit is None:
                raise RuntimeError('Runner jersey pinstripe left the fabric')
            stripe_verts.append((edge_x, hit.y + (.10 if back else -.10), z))
        if row:
            a = (row - 1) * 2
            stripe_faces.append((a, a + 1, a + 3, a + 2))
    name = ('Runner_JerseyPinstripe_' + ('Back' if back else 'Front') +
            '_' + str(round(x * 10)))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(stripe_verts, [], stripe_faces)
    mesh.update()
    mesh.materials.append(material)
    piece = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(piece)
    groups = {label: piece.vertex_groups.new(name=label) for label in group_names.values()}
    for vertex in mesh.vertices:
        neighbors_found = shirt_tree.find_n(vertex.co, 4)
        weights = {}
        total = 0.0
        for _, index, distance in neighbors_found:
            influence = 1.0 / max(distance, .03) ** 2
            total += influence
            for group in outfit.data.vertices[index].groups:
                label = group_names[group.group]
                weights[label] = weights.get(label, 0.0) + group.weight * influence
        for label, weight in weights.items():
            if weight / total > .0001:
                groups[label].add([vertex.index], weight / total, 'REPLACE')
    piece.parent = rig
    piece.matrix_world = outfit.matrix_world.copy()
    modifier = piece.modifiers.new('Runner native skin', 'ARMATURE')
    modifier.object = rig
    pieces.append(piece)

# The photographs show flat, dye-printed graphics. Rasterize each graphic in
# its X/Z plane first, then ask every *shirt UV texel* where it sits on the
# fabric. This inverse bake cannot bridge a UV seam or lose parts of a letter
# when the projection ray lands on an adjacent pattern panel.
shirt_faces = [face for face in outfit.data.polygons if face.material_index == 1]
uv_layer = outfit.data.uv_layers.active.data
atlas_size = 2048
planar_size = 2048
atlas = np.zeros((atlas_size, atlas_size, 4), dtype=np.uint8)
front_art = np.zeros((planar_size, planar_size, 4), dtype=np.uint8)
back_art = np.zeros_like(front_art)

def triangle_region(points, size):
    x0 = max(0, int(min(p[0] for p in points)) - 1)
    x1 = min(size - 1, int(max(p[0] for p in points)) + 1)
    y0 = max(0, int(min(p[1] for p in points)) - 1)
    y1 = min(size - 1, int(max(p[1] for p in points)) + 1)
    if x1 < x0 or y1 < y0:
        return None
    (ax, ay), (bx, by), (cx, cy) = points
    denominator = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
    if abs(denominator) < 1e-7:
        return None
    yy, xx = np.mgrid[y0:y1 + 1, x0:x1 + 1]
    px, py = xx + .5, yy + .5
    a = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / denominator
    b = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / denominator
    c = 1 - a - b
    covered = (a >= -.001) & (b >= -.001) & (c >= -.001)
    return (slice(y0, y1 + 1), slice(x0, x1 + 1)), (a, b, c), covered

def planar_xy(point):
    return ((point.x + 40.0) / 80.0 * (planar_size - 1),
            (170.0 - point.z) / 80.0 * (planar_size - 1))

colors = {
    'Runner_PinstripeDark': (44, 137, 130, 95),
    'Runner_PinstripeGold': (130, 158, 111, 110),
    'Runner_PrintGold': (182, 139, 67, 255),
    'Runner_PrintBlack': (22, 27, 29, 255),
}
order = {name: priority for priority, name in enumerate(colors)}
art_pieces = sorted((piece for piece in pieces
                     if piece.data.materials[0].name in colors),
                    key=lambda piece: order[piece.data.materials[0].name])
painted_triangles = 0
for piece in art_pieces:
    material_name = piece.data.materials[0].name
    is_back = 'Back' in piece.name or piece.name.endswith('.002') or piece.name.endswith('.003')
    canvas = back_art if is_back else front_art
    for face in piece.data.polygons:
        corners = list(face.vertices)
        for index in range(1, len(corners) - 1):
            points = [planar_xy(piece.data.vertices[corners[k]].co)
                      for k in (0, index, index + 1)]
            result = triangle_region(points, planar_size)
            if result is None:
                continue
            region, _, covered = result
            canvas[region][covered] = colors[material_name]
            painted_triangles += 1

shirt_texels = 0
for face in shirt_faces:
    corners = list(face.vertices)
    face_uvs = [uv_layer[loop].uv for loop in face.loop_indices]
    canvas = (front_art if sum(outfit.data.vertices[index].co.y
                              for index in corners) < 0 else back_art)
    for index in range(1, len(corners) - 1):
        ids = (0, index, index + 1)
        points = [(float(face_uvs[k].x) * (atlas_size - 1),
                   (1.0 - float(face_uvs[k].y)) * (atlas_size - 1)) for k in ids]
        result = triangle_region(points, atlas_size)
        if result is None:
            continue
        region, (a, b, c), covered = result
        verts = [outfit.data.vertices[corners[k]].co for k in ids]
        world_x = a * verts[0].x + b * verts[1].x + c * verts[2].x
        world_z = a * verts[0].z + b * verts[1].z + c * verts[2].z
        planar_x = np.clip(((world_x + 40.0) / 80.0 * (planar_size - 1)).astype(np.int32),
                           0, planar_size - 1)
        planar_y = np.clip(((170.0 - world_z) / 80.0 * (planar_size - 1)).astype(np.int32),
                           0, planar_size - 1)
        sampled = canvas[planar_y, planar_x]
        atlas[region][covered] = sampled[covered]
        shirt_texels += int(covered.sum())

art_image = bpy.data.images.new('Runner_JerseyArt', width=atlas_size,
                                height=atlas_size, alpha=True, float_buffer=False)
art_image.pixels.foreach_set((atlas[::-1].astype(np.float32) / 255.0).ravel())
art_image.filepath_raw = str(ROOT / 'T_Runner_JerseyArt.png')
art_image.file_format = 'PNG'
art_image.save()
print('RUNNER_JERSEY_ART_BAKED', painted_triangles,
      'shirt_texels', shirt_texels, art_image.filepath_raw)
for piece in art_pieces:
    pieces.remove(piece)
    bpy.data.objects.remove(piece, do_unlink=True)

native_matrix = outfit.matrix_world.copy()
bpy.ops.object.select_all(action='DESELECT')
outfit.select_set(True)
for piece in pieces:
    piece.select_set(True)
bpy.context.view_layer.objects.active = outfit
bpy.ops.object.join()
outfit.matrix_world = Matrix.Identity(4)
print('RUNNER_NATIVE_TRIM_FITTED', len(pieces))
outfit.name = 'SK_Runner_MetaOutfit'
outfit.data.name = outfit.name

bpy.ops.object.select_all(action='DESELECT')
outfit.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active = outfit
bpy.ops.export_scene.fbx(
    filepath=str(ROOT / 'SK_Runner_MetaOutfit.fbx'),
    use_selection=True, object_types={'ARMATURE', 'MESH'},
    add_leaf_bones=False, armature_nodetype='NULL',
    use_armature_deform_only=False, bake_anim=False,
    apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
    axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / 'Runner_MetaOutfit.blend'))
print('RUNNER_NATIVE_WARDROBE_EXPORTED', len(outfit.data.vertices),
      len(outfit.data.polygons), [m.name for m in outfit.data.materials])
