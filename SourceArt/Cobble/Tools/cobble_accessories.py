"""Rig-fitted, mesh-only accessories for the eight COBBLE characters."""

import math
import bpy
from mathutils import Matrix, Vector


class _Mesh:
    def __init__(self, name, materials):
        self.name = name
        self.materials = list(materials)
        self.vertices, self.faces, self.material_ids = [], [], []

    def add(self, vertices, faces, material=0):
        offset = len(self.vertices)
        self.vertices.extend(tuple(v) for v in vertices)
        self.faces.extend(tuple(offset + i for i in face) for face in faces)
        self.material_ids.extend([material] * len(faces))

    def tube(self, points, radius, material=0, sides=8, closed=False):
        points = [Vector(p) for p in points]
        vertices, faces = [], []
        for i, point in enumerate(points):
            before = points[(i - 1) % len(points)] if i or closed else point
            after = points[(i + 1) % len(points)] if i + 1 < len(points) or closed else point
            tangent = (after - before).normalized()
            reference = Vector((0, 0, 1)) if abs(tangent.z) < .85 else Vector((0, 1, 0))
            right = tangent.cross(reference).normalized()
            up = tangent.cross(right).normalized()
            for j in range(sides):
                angle = math.tau * j / sides
                vertices.append(point + radius * (math.cos(angle) * right + math.sin(angle) * up))
        for i in range(len(points) if closed else len(points) - 1):
            nxt = (i + 1) % len(points)
            for j in range(sides):
                faces.append((i * sides + j, i * sides + (j + 1) % sides,
                              nxt * sides + (j + 1) % sides, nxt * sides + j))
        if not closed:
            faces.extend([tuple(reversed(range(sides))),
                          tuple((len(points) - 1) * sides + j for j in range(sides))])
        self.add(vertices, faces, material)

    def ellipsoid(self, center, axes, radii, material=0, segments=20, rings=12):
        center = Vector(center)
        axes = [Vector(axis) for axis in axes]
        vertices = [center - axes[2] * radii[2]]
        for ring in range(1, rings):
            latitude = -math.pi / 2 + math.pi * ring / rings
            for j in range(segments):
                angle = math.tau * j / segments
                vertices.append(center + axes[0] * (radii[0] * math.cos(latitude) * math.cos(angle)) +
                                axes[1] * (radii[1] * math.cos(latitude) * math.sin(angle)) +
                                axes[2] * (radii[2] * math.sin(latitude)))
        top = len(vertices)
        vertices.append(center + axes[2] * radii[2])
        faces = [(0, 1 + (j + 1) % segments, 1 + j) for j in range(segments)]
        for ring in range(rings - 2):
            a, b = 1 + ring * segments, 1 + (ring + 1) * segments
            for j in range(segments):
                faces.append((a + j, a + (j + 1) % segments,
                              b + (j + 1) % segments, b + j))
        last = 1 + (rings - 2) * segments
        faces.extend((last + j, last + (j + 1) % segments, top) for j in range(segments))
        self.add(vertices, faces, material)

    def finish(self, rig, bone):
        mesh = bpy.data.meshes.new(self.name)
        mesh.from_pydata(self.vertices, [], self.faces)
        mesh.update()
        obj = bpy.data.objects.new(self.name, mesh)
        bpy.context.collection.objects.link(obj)
        for material in self.materials:
            mesh.materials.append(material)
        for polygon, material in zip(mesh.polygons, self.material_ids):
            polygon.material_index = material
            polygon.use_smooth = True
        obj.parent = rig
        obj.matrix_parent_inverse = Matrix.Identity(4)
        obj.matrix_basis = Matrix.Identity(4)
        obj.vertex_groups.new(name=bone).add(list(range(len(mesh.vertices))), 1, 'REPLACE')
        modifier = obj.modifiers.new('Skeleton', 'ARMATURE')
        modifier.object = rig
        return obj


def _bone(rig, name):
    return rig.data.bones[name]


def _rounded_rectangle(width, height, radius, steps=5):
    points = []
    for cx, cy, angle in [(width / 2 - radius, height / 2 - radius, 0),
                          (-width / 2 + radius, height / 2 - radius, 90),
                          (-width / 2 + radius, -height / 2 + radius, 180),
                          (width / 2 - radius, -height / 2 + radius, 270)]:
        for i in range(steps + 1):
            a = math.radians(angle + i * 90 / steps)
            points.append((cx + radius * math.cos(a), cy + radius * math.sin(a)))
    return points


def _sunglasses(human, rig, materials, scale, hanging=False):
    lens = bpy.data.materials.new('Cobble sunglass lens')
    lens.use_nodes = True
    lens.diffuse_color = (.009,.014,.018,1)
    lens.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value = (.009,.014,.018,1)
    lens.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value = .18
    mesh = _Mesh('Neckline sunglasses' if hanging else 'Worn rectangular sunglasses',
                 [materials['black'], materials['metal'], lens])
    head = _bone(rig, 'head')
    top = max(v.co.z for v in human.data.vertices)
    eye_z = top - .115 * scale
    eye_vertices = [v.co for v in human.data.vertices
                    if abs(v.co.z - eye_z) < .014 * scale and .025 * scale < abs(v.co.x) < .060 * scale]
    front = min((v.y for v in eye_vertices), default=head.head_local.y - .085 * scale) - .009 * scale
    center = Vector((0, front, eye_z))
    right, up, back = Vector((1, 0, 0)), Vector((0, 0, 1)), Vector((0, 1, 0))
    if hanging:
        neck = _bone(rig, 'neck_01').head_local
        chest_z = neck.z - .050 * scale
        chest = [v.co for v in human.data.vertices
                 if abs(v.co.z - chest_z) < .025 * scale and abs(v.co.x) < .08 * scale]
        center = Vector((.015 * scale, min(min((v.y for v in chest), default=-.09 * scale) - .026 * scale, -.178 * scale),
                         chest_z - .075 * scale))
        angle = math.radians(78)
        right, up = Vector((math.cos(angle), 0, math.sin(angle))), Vector((-math.sin(angle), 0, math.cos(angle)))
    for sign in (-1, 1):
        lens_center = center + right * (.034 * scale * sign)
        lens_shape = (.054, .036, .006) if hanging else (.052, .040, .012)
        outline = [lens_center + right * (x * scale) + up * (z * scale)
                   for x, z in _rounded_rectangle(*lens_shape)]
        mesh.tube(outline, (.0033 if hanging else .0016) * scale, closed=True)
        vertices = [point + back * .0015 * scale for point in outline]
        mesh.add(vertices, [tuple(range(len(vertices))), tuple(reversed(range(len(vertices))))], material=2)
        hinge = center + right * (.066 * scale * sign) + up * (.011 * scale)
        if hanging:
            stem = [hinge, hinge + back * (.016 * scale) + up * (.018 * scale),
                    center + right * (.018 * scale * sign) + up * (.077 * scale) + back * (.012 * scale)]
        else:
            stem = [hinge, hinge + back * (.060 * scale) + right * (.005 * scale * sign),
                    hinge + back * (.118 * scale) + up * (-.009 * scale)]
        mesh.tube(stem, (.003 if hanging else .0017) * scale)
    mesh.tube([center + right * (-.009 * scale) + up * (.008 * scale),
               center + up * (.012 * scale),
               center + right * (.009 * scale) + up * (.008 * scale)], (.003 if hanging else .0017) * scale)
    return mesh.finish(rig, 'spine_03' if hanging else 'head')


def _flipflop(human, rig, materials, side, scale):
    bone_name = 'foot_' + side
    foot, ball = _bone(rig, bone_name), _bone(rig, 'ball_' + side)
    axis = ball.tail_local - foot.head_local
    axis.z = 0
    if axis.length < .02:
        axis = Vector((0, -1, 0))
    axis.normalize()
    across = Vector((-axis.y, axis.x, 0))
    up = Vector((0, 0, 1))
    # Rig toe direction establishes the orientation; foot surface determines sole fit.
    foot_vertices = [v.co.copy() for v in human.data.vertices
                     if v.co.z < foot.head_local.z + .016 * scale and
                     abs(v.co.x - foot.head_local.x) < .085 * scale and
                     (v.co.x >= 0) == (side == 'l')]
    if foot_vertices:
        forward = [v.dot(axis) for v in foot_vertices]
        cross = [v.dot(across) for v in foot_vertices]
        heel, toe = min(forward) - .007 * scale, max(forward) + .014 * scale
        midpoint = (min(cross) + max(cross)) / 2
        halfwidth = (max(cross) - min(cross)) / 2 + .006 * scale
        ground = min(v.z for v in foot_vertices) - .011 * scale
    else:
        heel = foot.head_local.dot(axis) - .045 * scale
        toe = ball.tail_local.dot(axis) + .036 * scale
        midpoint, halfwidth, ground = foot.head_local.dot(across), .052 * scale, -.011 * scale
    length = toe - heel
    origin = axis * heel + across * midpoint + up * ground
    mesh = _Mesh('Flip flop ' + side, [materials['black'], materials['brown']])
    # Foot-shaped perimeter with a narrow heel, supported arch, and roomy rounded toe.
    outline = [(-.25, 0), (-.70, .035), (-.86, .13), (-.77, .34), (-.76, .52),
               (-.98, .72), (-1, .87), (-.83, .96), (-.40, 1), (.15, 1.015),
               (.72, .98), (1, .91), (.98, .77), (.85, .58), (.76, .36),
               (.78, .14), (.64, .04), (.20, 0)]
    levels = [(0, .91), (.003 * scale, 1), (.010 * scale, 1), (.013 * scale, .95)]
    vertices = [origin + across * (x * halfwidth * radius) + axis * (t * length) + up * z
                for z, radius in levels for x, t in outline]
    count = len(outline)
    faces = [tuple(reversed(range(count)))]
    for ring in range(len(levels) - 1):
        for i in range(count):
            faces.append((ring * count + i, ring * count + (i + 1) % count,
                          (ring + 1) * count + (i + 1) % count, (ring + 1) * count + i))
    faces.append(tuple((len(levels) - 1) * count + i for i in range(count)))
    mesh.add(vertices, faces)
    # Broad Y straps, a toe post, and a subtly raised center arch.
    fork = origin + axis * (.77 * length) + across * (-.12 * halfwidth) + up * (.056 * scale)
    mesh.tube([fork - up * (.029 * scale), fork], .0045 * scale)
    for sign in (-1, 1):
        end = origin + axis * (.43 * length) + across * (sign * halfwidth * .92) + up * (.022 * scale)
        middle = fork.lerp(end, .54) + up * (.013 * scale)
        curve = [(1 - t) ** 2 * fork + 2 * (1 - t) * t * middle + t * t * end
                 for t in [i / 10 for i in range(11)]]
        # Elliptical flat strap section rather than a round cord.
        vs, fs = [], []
        for i, point in enumerate(curve):
            direction = curve[min(i + 1, 10)] - curve[max(i - 1, 0)]
            width = direction.cross(up).normalized() * .0085 * scale
            for lateral, vertical in [(-1, -1), (1, -1), (1, 1), (-1, 1)]:
                vs.append(point + width * lateral + up * (.0026 * scale * vertical))
        for i in range(10):
            for j in range(4):
                fs.append((i * 4 + j, i * 4 + (j + 1) % 4, (i + 1) * 4 + (j + 1) % 4, (i + 1) * 4 + j))
        fs.extend([(3, 2, 1, 0), (40, 41, 42, 43)])
        mesh.add(vs, fs)
    return mesh.finish(rig, bone_name)


def _hand_axes(rig, side):
    forearm = _bone(rig, 'lowerarm_' + side)
    wrist = _bone(rig, 'hand_' + side).head_local.copy()
    forward = (forearm.tail_local - forearm.head_local).normalized()
    index, pinky = rig.data.bones.get('index_01_' + side), rig.data.bones.get('pinky_01_' + side)
    across = (index.head_local - pinky.head_local).normalized() if index and pinky else Vector((1, 0, 0))
    across = (across - forward * across.dot(forward)).normalized()
    normal = forward.cross(across).normalized()
    return wrist, across, normal, forward


def _boxing_glove(rig, materials, side, scale):
    wrist, across, normal, forward = _hand_axes(rig, side)
    mesh = _Mesh('Boxing glove ' + side, [materials['black'], materials['white']])
    axes = (across, normal, forward)
    # Continuous rounded wrist-to-knuckle shell with a curled finger pocket.
    profiles = [(-.040, .033, .027, .000), (-.025, .037, .031, .000),
                (.000, .037, .033, .000), (.032, .049, .039, .002),
                (.072, .059, .044, .007), (.115, .063, .050, .011),
                (.147, .053, .047, .006), (.166, .032, .034, -.003),
                (.174, .006, .007, -.007)]
    vertices, faces, sides = [], [], 24
    for distance, width, thickness, lift in profiles:
        for i in range(sides):
            angle = math.tau * i / sides
            vertices.append(wrist + forward * (distance * scale) +
                            across * (width * scale * math.cos(angle)) +
                            normal * ((lift + thickness * math.sin(angle)) * scale))
    for ring in range(len(profiles) - 1):
        for i in range(sides):
            faces.append((ring * sides + i, ring * sides + (i + 1) % sides,
                          (ring + 1) * sides + (i + 1) % sides, (ring + 1) * sides + i))
    faces.extend([tuple(reversed(range(sides))), tuple((len(profiles) - 1) * sides + i for i in range(sides))])
    mesh.add(vertices, faces)
    thumb_center = wrist + forward * (.075 * scale) + across * (.053 * scale) - normal * (.015 * scale)
    thumb_forward = (forward * .90 - across * .32).normalized()
    thumb_across = (across * .90 + forward * .32).normalized()
    mesh.ellipsoid(thumb_center, (thumb_across, normal, thumb_forward),
                   (.025 * scale, .029 * scale, .052 * scale))
    for distance in (-.030, -.005):
        ring = [wrist + forward * (distance * scale) + across * (.038 * scale * math.cos(a)) +
                normal * (.032 * scale * math.sin(a)) for a in [math.tau * i / 32 for i in range(32)]]
        mesh.tube(ring, .0028 * scale, material=1, closed=True)
    # White stitched line follows the curved knuckle edge.
    seam = [wrist + forward * (.132 * scale) + across * (.060 * scale * math.cos(a)) +
            normal * ((.009 + .048 * math.sin(a)) * scale)
            for a in [math.pi * i / 20 for i in range(21)]]
    mesh.tube(seam, .0016 * scale, material=1)
    return mesh.finish(rig, 'hand_' + side)


def _watch(rig, materials, scale):
    wrist, across, normal, forward = _hand_axes(rig, 'l')
    center = wrist - forward * (.035 * scale)
    mesh = _Mesh('Left wrist watch', [materials['black'], materials['metal'], materials['white']])
    for offset in (-.009, 0, .009):
        ring = [center + forward * (offset * scale) + across * (.031 * scale * math.cos(a)) +
                normal * (.026 * scale * math.sin(a)) for a in [math.tau * i / 32 for i in range(32)]]
        mesh.tube(ring, .0045 * scale, closed=True)
    face = center + normal * (.030 * scale)
    mesh.ellipsoid(face, (across, forward, normal), (.021 * scale, .023 * scale, .006 * scale), 1)
    mesh.ellipsoid(face + normal * (.005 * scale), (across, forward, normal),
                   (.018 * scale, .020 * scale, .0015 * scale))
    mesh.tube([face + normal * (.007 * scale) - across * (.006 * scale),
               face + normal * (.007 * scale),
               face + normal * (.007 * scale) + forward * (.012 * scale)], .0009 * scale, 2)
    return mesh.finish(rig, 'lowerarm_l')


def _hood(human, rig, materials, scale):
    mesh = _Mesh('Folded hood and drawstrings', [materials['sage'], materials['black']])
    neck = _bone(rig, 'neck_01').head_local.copy()
    # A thick fabric shell cups the back of the neck and collapses onto the upper back.
    # Its open rim is higher at the shoulders; the center forms a soft hanging fold.
    rings, around, vertices, faces = 9, 32, [], []
    for ring in range(rings):
        t = ring / (rings - 1)
        for i in range(around):
            a = math.tau * i / around
            x = (.076 + .036 * math.sin(math.pi * t)) * math.cos(a)
            y = .061 + (.043 + .025 * math.sin(math.pi * t)) * math.sin(a)
            z = -.035 - .090 * t + .015 * math.cos(2 * a) * math.sin(math.pi * t)
            # Narrowing at the bottom gives the hood a folded teardrop profile.
            x *= 1 - .58 * t ** 3
            y = .055 + (y - .055) * (1 - .50 * t ** 3)
            vertices.append(neck + Vector((x, y, z)) * scale)
    for ring in range(rings - 1):
        for i in range(around):
            faces.append((ring * around + i, ring * around + (i + 1) % around,
                          (ring + 1) * around + (i + 1) % around, (ring + 1) * around + i))
    faces.append(tuple((rings - 1) * around + i for i in range(around)))
    mesh.add(vertices, faces)
    mesh.tube(vertices[:around], .006 * scale, closed=True)
    # Two sloping fabric collar ends join the hood to the front neckline.
    for sign in (-1, 1):
        collar = [neck + Vector((sign * .077, .044, -.023)) * scale,
                  neck + Vector((sign * .065, -.018, -.015)) * scale,
                  neck + Vector((sign * .042, -.070, -.042)) * scale]
        mesh.tube(collar, .009 * scale)
        path = [neck + Vector((sign * .040, -.088, -.048)) * scale,
                neck + Vector((sign * .043, -.097, -.103)) * scale,
                neck + Vector((sign * .033, -.100, -.152 - (sign + 1) * .010)) * scale]
        mesh.tube(path, .0021 * scale, 1)
        mesh.tube([path[-1], path[-1] - Vector((0, 0, .009 * scale))], .0026 * scale, 1)
    return mesh.finish(rig, 'spine_03')


def add_accessories(index, human, rig, materials):
    """Create accessories in rig rest coordinates and return bound mesh objects."""
    height = max(v.co.z for v in human.data.vertices) - min(v.co.z for v in human.data.vertices)
    scale = max(.65, min(1.4, height / 1.75))
    objects = []
    if index != 3:
        objects.extend(_flipflop(human, rig, materials, side, scale) for side in ('l', 'r'))
    if index in (1, 2):
        objects.append(_sunglasses(human, rig, materials, scale, hanging=index == 1))
    if index == 1:
        objects.append(_hood(human, rig, materials, scale))
    if index == 4:
        objects.append(_watch(rig, materials, scale))
    if index == 8:
        objects.extend(_boxing_glove(rig, materials, side, scale) for side in ('l', 'r'))
    return objects
